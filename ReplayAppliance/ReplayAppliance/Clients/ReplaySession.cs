using drewCo.Tools.Logging;
using System.Net;
using System.Reflection;

namespace funscrew.Clients;

// ==============================================================================================================================
/// <remarks>
/// This is very much like an implementation of GGPOClient, but I am redoing all of those kinds of
/// functions, etc. by hand in an attempt to further distill, and hopefully simplify the internals
/// of GPPOClient, which is a direct port of some C++ code that I plan on rewriting anyway.
/// </remarks>
public class ReplaySession : IGGPOClient
{
  // Setting a large upper bound in the hopes of using mem pools in production.... (recycle ReplaySession instances)
  private const int MAX_PLAYERS_EVER = 8;

  public int ConnectTimeout { get { return SessionArgs.ConnectTimeout; } }

  public UInt64 SessionId { get; private set; }
  public SessionOptions SessionArgs { get; private set; }
  public GameRecorder Recorder { get; private set; }

  public EReplaySessionState State { get; private set; } = EReplaySessionState.Invalid;

  public int EndpointCount { get; private set; } = 0;
  public GGPOEndpoint[] Endpoints = new GGPOEndpoint[MAX_PLAYERS_EVER];

  public bool IsComplete { get; private set; } = false;

  private AddrHash[] UsedAddreses = new AddrHash[MAX_PLAYERS_EVER];
  private int[] UsedPlayerIndexes = new int[MAX_PLAYERS_EVER];

  private object ConnectionLock = new object();

  // Long term we aren't going to keep the 'local connect status' stuff around, so we can just hack this number into it....
  const int LEGACY_MAX_PLAYERS = 4;
  private ConnectStatus[] LocalConnectStatus = new ConnectStatus[LEGACY_MAX_PLAYERS];

  private Sync Sync = null!;
  private GGPOSessionCallbacks Callbacks = null!;

  public int ID { get; set; }

  // IGGPOClient interface stuff:
  // NOTE: With this version of ReplayAppliance, these properties don't make sense in terms
  // of how we are dealing with endpoints.... kind of telling ;)
  public string LocalPlayerName { get; private set; } = "<replay-sess>";
  public UInt32 ClientVersion { get; } = 0;
  public int CurrentFrame { get; } = -1;
  public string GameName { get { return SessionArgs.GameName; } }

  public int CurTime { get { return SessionArgs.Clock.CurTime; } }

  public IUdpBlaster UDP { get; private set; } = null!;

  public bool IsSyncing { get; private set; } = true;

  // --------------------------------------------------------------------------------------------------------------------------
  public ReplaySession(IUdpBlaster udp_, UInt64 sessionId_, GameRecorder recorder_, SessionOptions sessOps_, GGPOSessionCallbacks callbacks_)
  {
    UDP = udp_;
    SessionId = sessionId_;
    Recorder = recorder_;
    SessionArgs = sessOps_;
    Callbacks = callbacks_;
    State = EReplaySessionState.Initializing;
    IsSyncing = true;

    var syncOps = new SyncOptions()
    {
      callbacks = callbacks_,
      input_size = SessionArgs.TotalInputSize / SessionArgs.MaxPlayerCount,
      num_players = 2,
      num_prediction_frames = GGPOConsts.MAX_PREDICTION_FRAMES
    };

    // RESET DATA:
    ClearData();

    // NOTE: Not really sure what we are syncing in this context?
    Sync = new Sync(LocalConnectStatus, syncOps);

  }

  // --------------------------------------------------------------------------------------------------------------------------
  public bool IsReadyToSync(ref UdpMsg msg)
  {
    // We are only ready once we have all connections made!
    bool res = this.EndpointCount == this.SessionArgs.MaxPlayerCount;
    return res;
  }

  // --------------------------------------------------------------------------------------------------------------------------
  // Reset the internal data of this session.
  // This will become more useful when we are on to mempools.
  internal void ClearData()
  {
    int len = LocalConnectStatus.Length;
    for (int i = 0; i < len; i++)
    {
      LocalConnectStatus[i].last_frame = 0;
      LocalConnectStatus[i].disconnected = false;
    }

    for (int i = 0; i < MAX_PLAYERS_EVER; i++)
    {
      UsedPlayerIndexes[i] = -1;
    }
  }

  // --------------------------------------------------------------------------------------------------------------------------
  public GGPOEndpoint GetEndpoint(int index)
  {
    if (index >= EndpointCount)
    {
      throw new ArgumentOutOfRangeException($"Invalid {nameof(index)}!");
    }
    return Endpoints[index];
  }

  // --------------------------------------------------------------------------------------------------------------------------
  public void AddConnection(GGPOEndpoint endpoint)
  {
    lock (ConnectionLock)
    {
      if (this.SessionId != endpoint.SessionId)
      {
        throw new InvalidOperationException($"Session ids do not match! {this.SessionId}:{endpoint.SessionId}");
      }

      if (EndpointCount >= SessionArgs.MaxPlayerCount)
      {
        throw new InvalidOperationException("Max number of players have already been added!");
      }

      // Uniqueness check....
      for (int i = 0; i < EndpointCount; i++)
      {
        if (UsedAddreses[i] == endpoint.AddressHash) { throw new InvalidOperationException($"This address: {endpoint.AddressHash} is already connected to the replay session!"); }
        if (UsedPlayerIndexes[i] == endpoint.PlayerIndex) { throw new InvalidOperationException($"This player index: {endpoint.PlayerIndex} is already connected to the replay session!"); }
      }

      // OK we are all good!
      this.Endpoints[EndpointCount] = endpoint;
      ++EndpointCount;

      if (EndpointCount == SessionArgs.MaxPlayerCount)
      {
        this.State = EReplaySessionState.Active;
      }
    }
  }

  // --------------------------------------------------------------------------------------------------------------------------
  private void DisconnectAll(int curFrame = 0)
  {
    lock (ConnectionLock)
    {
      int len = EndpointCount;
      for (int i = 0; i < len; i++)
      {
        Endpoints[i].Disconnect(curFrame);
      }

      // NOTE: Do not modify the endpoint count at this time.  We need that data to properly cleanup the
      // session.
    }
  }

  // --------------------------------------------------------------------------------------------------------------------------
  private void CompleteSession(int curFrame, ECompletionReason reason, EErrorReason errReason, string? message)
  {
    DisconnectAll();

    Recorder.CompleteReplay(curFrame, reason, errReason, message);

    // NOTE: We can add more logging here if we wanted to.

    State = EReplaySessionState.Complete;
    this.IsComplete = true;
  }

  // --------------------------------------------------------------------------------------------------------------------------
  /// <summary>
  /// This is where the inputs for the different frames will get merged, recorded, and later sent out.
  /// </summary>
  internal void MergeInput(ref GameInput input, int playerIndex)
  {
    if (Recorder.HasError)
    {
      // We have detected an error in the recorder.  We will log this, and send disconnect
      // notices to all active clients.
      Log.Error($"Session:{this.SessionId} - There was a recording error: {Recorder.ErrorReason} : {Recorder.ErrorMessage}");
      DisconnectAll();
      return;
    }
    Recorder.AddInput(playerIndex, ref input);
  }

  // --------------------------------------------------------------------------------------------------------------------------
  internal void DeliverMessage(ref UdpMsg msg, int received, IPEndPoint receivedFrom)
  {
    int epCount = EndpointCount;
    for (int i = 0; i < epCount; i++)
    {
      var ep = Endpoints[i];
      if (!ep.IsLocalPlayer && ep.HasAddress(receivedFrom))
      {
        ep.HandleMessage(ref msg, received);
        break;
      }
    }
  }


  // --------------------------------------------------------------------------------------------------------------------------
  /// <summary>
  /// This is like the 'DoPoll' of GGPOClient, but for this particular session....
  /// </summary>
  internal void DoPoll()
  {
    for (int i = 0; i < EndpointCount; i++)
    {
      if (Endpoints[i].IsDisconnected)
      {
        var reason = Endpoints[i].ConnectionTimedOut ? ECompletionReason.ConnectionTimeout : ECompletionReason.NormalDisconnect;

        // One or more endpoints have disconnected, so we will disconnect them all / wrap this up!
        CompleteSession(CurrentFrame, reason, EErrorReason.None, null);
        break;
      }

      Endpoints[i].OnLoopPoll();
    }

    if (this.IsComplete) { return; }

    HandleEvents();


    if (!IsSyncing)
    {
      Sync.CheckSimulation();

      // notify all of our endpoints of their local frame number for their
      // next connection quality report
      int current_frame = Sync.GetFrameCount();
      for (int i = 0; i < EndpointCount; i++)
      {
        Endpoints[i].SetLocalFrameNumber(current_frame);
      }

      int total_min_confirmed = current_frame; // Nothing to poll in replay context!   PollPlayers(current_frame);

      Utils.LogIt(LogCategories.ENDPOINT, "last confirmed: %d.", total_min_confirmed);
      if (total_min_confirmed >= 0)
      {
        Utils.ASSERT(total_min_confirmed != int.MaxValue);

        Utils.LogIt(LogCategories.ENDPOINT, "set confirmed: %d.", total_min_confirmed);
        Sync.SetLastConfirmedFrame(total_min_confirmed);
      }

      // send timesync notifications if now is the proper time
      // NOTE: We don't need to care about timesync in replay clients.
      //if (current_frame > _next_recommended_sleep)
      //{
      //  int interval = 0;
      //  for (int i = 0; i < _endpoints.Count; i++)
      //  {
      //    interval = Math.Max(interval, _endpoints[i].RecommendFrameDelay());
      //  }

      //  if (interval > 0)
      //  {
      //    GGPOEvent info = new GGPOEvent();
      //    info.event_code = EEventCode.GGPO_EVENTCODE_TIMESYNC;
      //    info.u.timesync.frames_ahead = interval;
      //    _callbacks.on_event(ref info);
      //    _next_recommended_sleep = current_frame + GGPOConsts.RECOMMENDATION_INTERVAL;
      //  }
      //}

      // NOTE: Not sure what that means.... we should use the timeout for the sleep value,
      // or we should not sleep it here?
      // XXX: this is obviously a farce...
      // --> It means that we should not sleep it here b/c the game loop should provide all of the timing.
      // It is being preserved like this for legacy purposes, but I will nuke it later.
      //if (timeout > 0)
      //{
      //  Thread.Sleep(1);
      //}
    }

  }

  // --------------------------------------------------------------------------------------------------------------------------
  // NOTE: This is like 'PollUdpProtocolEvents' from GPPOClient.
  private void HandleEvents()
  {

    var evt = new UdpEvent();
    for (UInt16 i = 0; i < EndpointCount; i++)
    {
      var ep = Endpoints[i];

      // NOTE: Local players aren't really going to have events because they don't poll or receive messages.
      while (ep.GetEvent(ref evt))
      {
        OnUdpProtocolPeerEvent(ref evt, ep);
      }
    }
  }

  // --------------------------------------------------------------------------------------------------------------------------
  private void OnUdpProtocolPeerEvent(ref UdpEvent evt, GGPOEndpoint endpoint)
  {

    var playerIndex = endpoint.PlayerIndex;

    // int playerIndex = -1;
    OnUdpProtocolEvent(ref evt, endpoint);

    switch (evt.type)
    {
      case EEventType.Input:
        // An input was received:
        if (!endpoint.IsReplayClient)
        {
          if (!LocalConnectStatus[playerIndex].disconnected)
          {

            int current_remote_frame = LocalConnectStatus[playerIndex].last_frame;
            int new_remote_frame = evt.u.input.frame;
            Utils.ASSERT(current_remote_frame == -1 || new_remote_frame == (current_remote_frame + 1));

            Sync.AddRemoteInput(playerIndex, ref evt.u.input);

            // Notify the other endpoints which frame we received from a peer
            Utils.LogIt(LogCategories.INPUT, "remote frame for: %d - %d", playerIndex, evt.u.input.frame);
            LocalConnectStatus[playerIndex].last_frame = evt.u.input.frame;
          }
        }
        else
        {
          int x = 10;
        }
        break;

      case EEventType.Datagram:

        // One endpoint wants to disconnect, so we will disconect everyone.
        if (evt.u.datagram.code == UdpEvent.DATAGRAM_CODE_DISCONNECT)
        {
          Log.Info($"Disconnect signal received on session: {this.SessionId} from: {endpoint.PlayerIndex}");
          DisconnectAll();

          // HandleDisconnect(endpoint);
        }
        // TODO: Detect + log chat messages here!

        break;

      case EEventType.Disconnected:
        DisconnectEndpoint(endpoint);
        break;

    }
  }

  // --------------------------------------------------------------------------------------------------------------------------
  public void OnDisconnect(GGPOEndpoint endpoint)
  {
    // We don't need to send out any more messges / events in a replay session.
    Log.Info("An endpoint was disconnected...");
  }

  // --------------------------------------------------------------------------------------------------------------------------
  private void DisconnectEndpoint(GGPOEndpoint endpoint)
  {
    // NOTE: We may not need to do anything here!
    Log.Info("Disconnect endpoint event.  Not sure what to do?");
  }

  // --------------------------------------------------------------------------------------------------------------------------
  private unsafe void OnUdpProtocolEvent(ref UdpEvent evt, GGPOEndpoint endpoint)
  {

    byte playerIndex = endpoint.PlayerIndex;
    bool isReplay = endpoint.IsReplayClient;

    GGPOEvent info = new GGPOEvent();
    info.player_index = playerIndex;
    info.isReplayEndpoint = (byte)(isReplay ? 1 : 0);

    switch (evt.type)
    {
      case EEventType.Connected:
        info.event_code = EEventCode.GGPO_EVENTCODE_CONNECTED_TO_PEER;
        info.player_index = playerIndex;

        //if (!isReplay)
        //{
        //  string name = evt.u.connected.GetPlayerName();
        //  _PlayerNames[playerIndex] = name;
        //}
        // strcpy_s(_PlayerNames[playerIndex], evt.u.connected.playerName);
        // strcpy_s(info.u.connected.playerName, evt.u.connected.playerName);

        Callbacks.on_event(ref info);
        break;
      case EEventType.Synchronizing:
        info.event_code = EEventCode.GGPO_EVENTCODE_SYNCHRONIZING_WITH_PEER;
        info.player_index = playerIndex;
        info.u.synchronizing.count = evt.u.synchronizing.count;
        info.u.synchronizing.total = evt.u.synchronizing.total;
        Callbacks.on_event(ref info);
        break;

      case EEventType.Synchronized:
        info.event_code = EEventCode.GGPO_EVENTCODE_SYNCHRONIZED_WITH_PEER;
        info.player_index = playerIndex;
        Callbacks.on_event(ref info);

        CheckInitialSync();
        break;

      case EEventType.NetworkInterrupted:
        info.event_code = EEventCode.GGPO_EVENTCODE_CONNECTION_INTERRUPTED;
        info.player_index = playerIndex;
        info.u.connection_interrupted.disconnect_timeout = evt.u.network_interrupted.disconnect_timeout;
        Callbacks.on_event(ref info);
        break;

      case EEventType.NetworkResumed:
        info.event_code = EEventCode.GGPO_EVENTCODE_CONNECTION_RESUMED;
        info.player_index = playerIndex;
        Callbacks.on_event(ref info);
        break;

      case EEventType.Datagram:

        info.event_code = EEventCode.GGPO_EVENTCODE_DATAGRAM;
        info.u.datagram.player_index = (byte)playerIndex;           // NOTE: For replay appliance, etc. we should include for information like 'endpoint type'
        info.u.datagram.code = evt.u.datagram.code;
        info.u.datagram.frame = evt.u.datagram.frame;
        info.u.datagram.dataSize = evt.u.datagram.dataSize;

        fixed (byte* pSrc = evt.u.datagram.data)
        {
          Utils.CopyMem(info.u.datagram.data, pSrc, evt.u.datagram.dataSize);
        }

        // NOTE: I am going to change this up so that we can surface the events in a different way?
        // I am not convinced that a union is the best way?

        if (info.u.datagram.code == (byte)EDatagramCode.DATAGRAM_CODE_CHAT)
        {
          // string text = AnsiHelpers.PtrToFixedLengthString(info.u.datagram.data, evt.u.chat.dataSize, GGPOConsts.MAX_GGPO_DATA_SIZE);
          // Log.Info($"Text is: {text}");
        }

        if (info.u.datagram.code == (byte)EDatagramCode.DATAGRAM_CODE_DISCONNECT)
        {
          var pi = info.u.datagram.player_index;

          // Disconnect datagrams come in bursts, so if we have already handled it for this index,
          // then we can skip raising the event multiple times.
          // NOTE:  We may want to keep more information about the conditions of a disconnect....
          if (Endpoints[pi].IsDisconnected) { return; }

          // Log.Info("disconnect notice was received...");
          // The endpoint has disconnected.... what do we do?
          int frameCount = Sync.GetFrameCount();
          Endpoints[pi].Disconnect(frameCount);
        }

        Callbacks.on_event(ref info);

        break;
    }
  }

  // ----------------------------------------------------------------------------------------------------------
  protected virtual void CheckInitialSync()
  {
    int i;

    if (IsSyncing)
    {
      // Check to see if everyone is now synchronized.  If so,
      // go ahead and tell the client that we're ok to accept input.
      int epLen = EndpointCount;
      for (i = 0; i < epLen; i++)
      {
        var ep = Endpoints[i];
        int epi = ep.PlayerIndex;

        // xxx: IsInitialized() must go... we're actually using it as a proxy for "represents the local player"
        // NOTE: The above comment is a bit misleading.  'Is initialized' means that the endpoint is remote.
        if (!ep.IsLocalPlayer &&
            !ep.IsSynchronized() &&
            !LocalConnectStatus[epi].disconnected)
        {
          return;
        }
      }

      GGPOEvent info = new GGPOEvent();
      info.event_code = EEventCode.GGPO_EVENTCODE_RUNNING;
      Callbacks.on_event(ref info);
      IsSyncing = false;
    }
  }


}



// ==============================================================================================================================
public enum EReplaySessionState
{
  Invalid = 0,

  /// <summary>
  /// We are waiting for all connections to be added + synced.
  /// </summary>
  Initializing,

  /// <summary>
  /// Everyone is connected + synced and we are exchanging packets and whatnot...
  /// </summary>
  Active,

  /// <summary>
  /// All clients have been disconnected + session is complete.
  /// </summary>
  Complete
}