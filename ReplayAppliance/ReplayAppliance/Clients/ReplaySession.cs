using drewCo.Tools.Logging;
using System.Net;
using System.Security.Cryptography;
using System.Threading;

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

  public UInt64 SessionId { get; private set; }
  public SessionOptions SessionArgs { get; private set; }
  public GameRecorder Recorder { get; private set; }

  public EReplaySessionState State { get; private set; } = EReplaySessionState.Invalid;

  private int ConnectedCount = 0;
  public GGPOEndpoint[] Endpoints = new GGPOEndpoint[MAX_PLAYERS_EVER];

  public bool IsComplete { get; private set; } = false;

  private AddrHash[] UsedAddreses = new AddrHash[MAX_PLAYERS_EVER];
  private int[] UsedPlayerIndexes = new int[MAX_PLAYERS_EVER];

  private object ConnectionLock = new object();

  private ConnectStatus[] LocalConnectStatus = null!;

  private Sync Sync = null!; //new Sync()
  private GGPOSessionCallbacks Callbacks = null!;


  // IGGPOClient interface stuff:
  // NOTE: With this version of ReplayAppliance, these properties don't make sense in terms
  // of how we are dealing with endpoints.... kind of telling ;)
  public string LocalPlayerName { get; private set; } = "not matters";
  public UInt32 ClientVersion { get; } = 0;
  public int CurrentFrame { get; } = 0;
  public string GameName { get; } = string.Empty;

  public int CurTime { get { return SessionArgs.Clock.CurTime; } }

  public IUdpBlaster UDP { get { throw new NotImplementedException("does anyone really need this?"); } }

  public bool IsSyncing { get; private set; } = true;

  // --------------------------------------------------------------------------------------------------------------------------
  public ReplaySession(UInt64 sessionId_, ConnectStatus[] localConnectStatus_, GameRecorder recorder_, SessionOptions sessOps_, GGPOSessionCallbacks callbacks_)
  {
    SessionId = sessionId_;
    LocalConnectStatus = localConnectStatus_;
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

    // NOTE: Not really sure what we are syncing in this context?
    Sync = new Sync(LocalConnectStatus, syncOps);
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

      if (ConnectedCount >= SessionArgs.MaxPlayerCount)
      {
        throw new InvalidOperationException("Max number of players have already been added!");
      }

      // Uniqueness check....
      for (int i = 0; i < MAX_PLAYERS_EVER; i++)
      {
        if (UsedAddreses[i] == endpoint.AddressHash) { throw new InvalidOperationException($"This address: {endpoint.AddressHash} is already connected to the replay session!"); }
        if (UsedPlayerIndexes[i] == endpoint.PlayerIndex) { throw new InvalidOperationException($"This player index: {endpoint.PlayerIndex} is already connected to the replay session!"); }
      }

      // OK we are all good!
      this.Endpoints[ConnectedCount] = endpoint;
      ++ConnectedCount;

      if (ConnectedCount == SessionArgs.MaxPlayerCount)
      {
        this.State = EReplaySessionState.Active;
      }
    }
  }

  // --------------------------------------------------------------------------------------------------------------------------
  public void DisconnectAll(int curFrame = 0)
  {
    lock (ConnectionLock)
    {
      int len = ConnectedCount;
      for (int i = 0; i < len; i++)
      {
        Endpoints[i].Disconnect(curFrame);
      }

      State = EReplaySessionState.Complete;
    }
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
      Log.Error($"There was a recording error: {Recorder.ErrorReason} : {Recorder.ErrorMessage}");
      DisconnectAll();
      return;
    }
    Recorder.AddInput(playerIndex, ref input);
  }

  // --------------------------------------------------------------------------------------------------------------------------
  internal void DeliverMessage(ref UdpMsg msg, int received, IPEndPoint receivedFrom)
  {
    int epCount = ConnectedCount;
    for (int i = 0; i < epCount; i++)
    {
      var ep = Endpoints[i]; // _endpoints[i];
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
    for (int i = 0; i < ConnectedCount; i++)
    {
      Endpoints[i].OnLoopPoll();
    }
    HandleEvents();


    if (!IsSyncing)
    {
      Sync.CheckSimulation();

      // notify all of our endpoints of their local frame number for their
      // next connection quality report
      int current_frame = Sync.GetFrameCount();


      for (int i = 0; i < ConnectedCount; i++)
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
    for (UInt16 i = 0; i < ConnectedCount; i++)
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
        info.u.datagram.player_index = (byte)playerIndex;
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
      int epLen = ConnectedCount;
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