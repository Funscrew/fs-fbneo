using drewCo.Tools.Logging;
using System.ComponentModel;
using System.Diagnostics;
using System.Net;
using System.Transactions;

namespace funscrew;

// ==========================================================================================
/// <summary>
/// Main client that is used to connect to one or more other players over the network.
/// Each GGPOClient instance will have one local player, and one or more remote players.
/// </summary>
public class GGPOClient : IGGPOClient, IDisposable
{
  protected GGPOClientOptions Options = null!;


  protected List<GGPOEndpoint> _endpoints = new List<GGPOEndpoint>(0x04);
  protected GGPOEndpoint[] _Players = new GGPOEndpoint[GGPOConsts.MAX_PLAYERS];
  protected int _ConnectedPlayerCount = 0;

  public IUdpBlaster UDP { get; private set; } = null!;

  private IClockSource Clock = null!;
  public int CurTime { get { return Clock.CurTime; } }

  public int ConnectTimeout { get { return this.Options.PeerConnectTimeout; } }
  public UInt32 ClientVersion { get { return this.Options.ClientVersion; } }

  public int ID { get; set; }

  /// <summary>
  /// Indicates that the client is officially started, and no new connections can be added.
  /// </summary>
  private bool IsLocked = false;

  /// <summary>
  /// Are we currently processing a rollback?
  /// </summary>
  private bool InRollback = false;
  public bool _synchronizing { get; protected set; } = false;

  protected Sync _sync = null!;

  protected ConnectStatus[] _local_connect_status = null!;

  private string[] _PlayerNames = new string[GGPOConsts.MAX_PLAYERS];
  protected GGPOSessionCallbacks _callbacks;

  public GGPOSessionCallbacks Callbacks { get { return _callbacks; } }

  private GGPOEndpoint LocalPlayer = null!;

  /// <summary>
  /// Session Id, which corresponds to unix time in milliseconds.
  /// Only used in replay contexts.
  /// </summary>
  public UInt64 SessionId { get { return Options.SessionId; } }

  private int _next_recommended_sleep = 0;

  public string LocalPlayerName { get; private set; }

  private byte[] _ReceiveBuffer = new byte[8192];
  private EndPoint ReceivedFrom = new IPEndPoint(IPAddress.Any, 0);   // This doesn't matter, we just need a place to stuff the data...

  public bool IsComplete { get; protected set; } = false;

  public int ClientCount { get { return this._endpoints.Count; } }

  public int CurrentFrame { get { return this._sync.GetFrameCount(); } }

  // ----------------------------------------------------------------------------------------
  public GGPOClient(GGPOClientOptions options_, IUdpBlaster udp_, IClockSource clock_)
  {
    Options = options_;

    ValidateOptions();

    UDP = udp_;
    Clock = clock_;

    _local_connect_status = new ConnectStatus[GGPOConsts.MAX_PLAYERS];
    for (int i = 0; i < GGPOConsts.MAX_PLAYERS; i++)
    {
      _local_connect_status[i].last_frame = -1;
    }

    var SyncOps = new SyncOptions()
    {
      callbacks = Options.Callbacks,
      input_size = Options.InputSize,
      num_players = 2,
      num_prediction_frames = GGPOConsts.MAX_PREDICTION_FRAMES
    };
    _sync = new Sync(_local_connect_status, SyncOps);

    _callbacks = Options.Callbacks;

    // I'm implementing this out of a sense of tradition....
    // Not really sure if this matters here, or if this is even the best place to
    // fire this off.
    _callbacks.begin_game(string.Empty);

    BeginSync();
  }

  // ----------------------------------------------------------------------------------------
  public void Dispose()
  {
    UDP?.Dispose();
  }

  public string GameName { get { return this.Options.GameName; } }

  // ----------------------------------------------------------------------------------------
  public virtual void DisconnectAll()
  {
    int frameCount = _sync.GetFrameCount();
    for (int i = 0; i < this.ClientCount; i++)
    {
      this._endpoints[i].Disconnect(frameCount);
    }
    this._endpoints.Clear();

    this.IsComplete = true;
  }

  // ----------------------------------------------------------------------------------------
  public void BeginSync()
  {
    // TODO: Some kind of check to make sure that one or more clients are actually disconnected....
    _synchronizing = true;

    int len = this._endpoints.Count;
    for (int i = 0; i < len; i++)
    {
      var ep = _endpoints[i];
      ep.Synchronize();
    }
  }

  // ----------------------------------------------------------------------------------------
  private void ValidateOptions()
  {
    // if (Options.LocalPort == Options.
    if (Options.Callbacks == null)
    {
      throw new InvalidOperationException("Callbacks are null!");
    }
  }


  // ----------------------------------------------------------------------------------------
  /// <summary>
  /// Add a connection to the replay appliance.  Send out inputs, etc. to this endpoint
  /// so that recordings can be made, and other can spectate.
  /// </summary>
  public GGPOEndpoint AddReplayAppliance(string host, int port)
  {
    if (LocalPlayer == null)
    {
      throw new InvalidOperationException("The local player must be added before adding a replay client!");
    }

    // This is one of the clients that will be sending the input, etc. data to the replay appliance.
    var epOps = new GGPOEndpointOptions()
    {
      PlayerIndex = LocalPlayer.PlayerIndex,
      PlayerName = LocalPlayer.GetPlayerName(),
      RemoteHost = host,
      RemotePort = port,
      IsReplayClient = true,
      SessionId = this.SessionId,
      ConnectTimeout = GGPOConsts.DEFAULT_CONNECT_TIMEOUT,
    };

    var replayClient = new GGPOEndpoint(this, epOps, this._local_connect_status);
    replayClient.EndpointType = EEndpointType.ReplayAppliance;
    this._endpoints.Add(replayClient);
    return replayClient;
  }

  // ----------------------------------------------------------------------------------------
  /// <summary>
  /// Add a local player!
  /// </summary>
  public GGPOEndpoint AddLocalPlayer(string playerName, byte playerIndex, TestOptions? testOptions = null)
  {
    if (LocalPlayer != null)
    {
      throw new InvalidOperationException("The local player has already been set!");
    }

    CheckLocked();
    var ops = new GGPOEndpointOptions()
    {
      PlayerIndex = playerIndex,
      PlayerName = playerName,
      IsLocal = true,
      TestOptions = testOptions ?? new TestOptions()
    };
    var res = new GGPOEndpoint(this, ops, _local_connect_status);
    LocalPlayer = res;

    this._endpoints.Add(res);

    this.LocalPlayerName = playerName;
    this._Players[this._ConnectedPlayerCount] = res;
    this._ConnectedPlayerCount++;

    return res;
  }

  // ----------------------------------------------------------------------------------------
  public GGPOEndpoint AddRemotePlayer(RemoteEndpointData remoteData, TestOptions? testOps = null)
  {
    CheckLocked();

    var ops = new GGPOEndpointOptions()
    {
      IsLocal = false,
      PlayerIndex = (byte)(remoteData.PlayerNumber - 1),
      RemoteHost = remoteData.Host,
      RemotePort = remoteData.Port,
      TestOptions = testOps ?? new TestOptions()
    };

    GGPOEndpoint res = CreateEndpoint(this, ops, _local_connect_status);
    this._endpoints.Add(res);

    this._Players[this._ConnectedPlayerCount] = res;
    this._ConnectedPlayerCount++;

    return res;
  }

  // --------------------------------------------------------------------------------------------------------------------------
  protected virtual GGPOEndpoint CreateEndpoint(GGPOClient gGPOClient, GGPOEndpointOptions ops, ConnectStatus[] local_connect_status)
  {
    var res = new GGPOEndpoint(this, ops, _local_connect_status);
    return res;
  }


  // ----------------------------------------------------------------------------------------------------------
  public bool IsReadyToSync(ref UdpMsg msg)
  {
    switch (msg.header.type)
    {
      case EMsgType.SyncRequest:

        // We are always ready to connect to the replay appliance.
        if (msg.u.sync_request.endpointType == (uint8_t)EEndpointType.ReplayAppliance)
        {
          return true;
        }

        break;

      case EMsgType.SyncReply:
        // Special case for replay appliance...?
        if (msg.u.sync_reply.isReady == 1 && msg.u.sync_reply.endpointType == (uint8_t)EEndpointType.ReplayAppliance)
        {
          return true;
        }

        break;

      default:
        throw new InvalidOperationException($"Unsupported message type: {msg.header.type}!");
    }

    // Check all endpoints for replay appliance + see if we are synced with it yet.
    for (int i = 0; i < _endpoints.Count; i++)
    {
      var ep = _endpoints[i];
      if (ep.IsReplayClient && !ep.IsDisconnected)
      {
        bool res = ep.IsRunning();
        return res;
      }
    }

    return true;
  }

  // ----------------------------------------------------------------------------------------
  private void CheckLocked()
  {
    if (IsLocked)
    {
      throw new InvalidOperationException("New connections are not allowed at this time!");
    }
  }

  // ----------------------------------------------------------------------------------------
  public void Idle()
  {
    DoPoll(Options.IdleTimeout);
  }

  // ----------------------------------------------------------------------------------------
  public virtual void DoPoll(int timeout)
  {
    if (IsComplete) { return; }

    // Receive all messages + send them off to the correct endpoints.
    while (true)
    {
      int received = this.UDP.Receive(_ReceiveBuffer, ref ReceivedFrom);
      if (received == 0) { break; }

      UdpMsg msg = new UdpMsg();
      UdpMsg.FromBytes(_ReceiveBuffer, ref msg, received);

      // Endpoints get updated first so that we can get events, inputs, etc.
      DeliverMessage(ref msg, received, ReceivedFrom);
    }

    // Now we can do the normal polling for the endpoints.
    int epCount = _endpoints.Count;
    for (int i = 0; i < epCount; i++)
    {
      _endpoints[i].OnLoopPoll();
    }

    // Now we can handle the results of the endpoint updates (events, etc.)
    // Handle events!
    PollUdpProtocolEvents();

    if (!_synchronizing)
    {
      _sync.CheckSimulation();

      // notify all of our endpoints of their local frame number for their
      // next connection quality report
      int current_frame = _sync.GetFrameCount();


      for (int i = 0; i < epCount; i++)
      {
        _endpoints[i].SetLocalFrameNumber(current_frame);
      }

      int total_min_confirmed = PollPlayers(current_frame);


      Utils.LogIt(LogCategories.ENDPOINT, "last confirmed: %d.", total_min_confirmed);
      if (total_min_confirmed >= 0)
      {
        Utils.ASSERT(total_min_confirmed != int.MaxValue);

        Utils.LogIt(LogCategories.ENDPOINT, "set confirmed: %d.", total_min_confirmed);
        _sync.SetLastConfirmedFrame(total_min_confirmed);
      }

      // send timesync notifications if now is the proper time
      if (current_frame > _next_recommended_sleep)
      {
        int interval = 0;
        for (int i = 0; i < _endpoints.Count; i++)
        {
          interval = Math.Max(interval, _endpoints[i].RecommendFrameDelay());
        }

        if (interval > 0)
        {
          GGPOEvent info = new GGPOEvent();
          info.event_code = EEventCode.GGPO_EVENTCODE_TIMESYNC;
          info.u.timesync.frames_ahead = interval;
          _callbacks.on_event(ref info);
          _next_recommended_sleep = current_frame + GGPOConsts.RECOMMENDATION_INTERVAL;
        }
      }

      // NOTE: Not sure what that means.... we should use the timeout for the sleep value,
      // or we should not sleep it here?
      // XXX: this is obviously a farce...
      // --> It means that we should not sleep it here b/c the game loop should provide all of the timing.
      // It is being preserved like this for legacy purposes.
      if (timeout > 0)
      {
        Thread.Sleep(1);
      }
    }

    // NOTE: Replay client is going to be converted into another type of endpoint!
    // UpdateReplayClient();
  }

  // ----------------------------------------------------------------------------------------------------------
  protected virtual int PollPlayers(int current_frame)
  {
    int total_min_confirmed;
    if (_ConnectedPlayerCount == 2)
    {
      // We are connected to one other player....
      total_min_confirmed = Poll2Players(current_frame);
    }
    else
    {
      total_min_confirmed = PollNPlayers(current_frame);
    }

    return total_min_confirmed;
  }

  // ----------------------------------------------------------------------------------------------------------
  protected virtual void DeliverMessage(ref UdpMsg msg, int received, EndPoint receivedFrom)
  {
    int epCount = _endpoints.Count;
    for (int i = 0; i < epCount; i++)
    {
      var ep = _endpoints[i];
      if (!ep.IsLocalPlayer && ep.HasAddress(receivedFrom))
      {
        ep.HandleMessage(ref msg, received);
        break;
      }
      else
      {
        int x = 10;
      }
    }
  }

  //// ----------------------------------------------------------------------------------------------------------
  //private void UpdateReplayClient()
  //{
  //  if (ReplayClient != null)
  //  {
  //    // Events are handled internally.
  //    ReplayClient.OnLoopPoll();
  //  }
  //}

  // ----------------------------------------------------------------------------------------------------------
  private unsafe int Poll2Players(int current_frame)
  {
    UInt16 i;

    // discard confirmed frames as appropriate
    int total_min_confirmed = int.MaxValue;

    // NOTE: because the replay appliance is another endpoint, things get a bit weird...
    // In a future iteration, I think that we would keep track of player endpoints, and 'other' endpoints
    // to make the distinction a bit more clear, and so we don't have to keep indexes in sync across components.
    for (i = 0; i < _ConnectedPlayerCount; i++)
    {
      GGPOEndpoint ep = _Players[i];

      byte epi = ep.PlayerIndex;

      // We only care if the queue is connected so that we can maybe disconnect it.
      bool queue_connected = true;
      if (ep.IsRunning())
      {
        int ignore;
        queue_connected = ep.GetPeerConnectStatus(i, &ignore);
      }
      if (!_local_connect_status[epi].disconnected)
      {
        total_min_confirmed = Math.Min(_local_connect_status[epi].last_frame, total_min_confirmed);
      }
      Utils.LogIt(LogCategories.ENDPOINT, "local frame: %d, last: %d, confirmed: %d", !_local_connect_status[i].disconnected, _local_connect_status[i].last_frame, total_min_confirmed);
      if (!queue_connected && !_local_connect_status[epi].disconnected)
      {
        Utils.LogIt(LogCategories.ENDPOINT, "disconnect by request: %d", i);
        DisconnectEndpoint(ep, total_min_confirmed);
      }
    }
    return total_min_confirmed;
  }


  // ----------------------------------------------------------------------------------------------------------
  private int PollNPlayers(int current_frame)
  {
    // I'm not really sure how we want to handle this in the future...
    throw new NotSupportedException("Only two players are currently supported!");
    //uint16 i, queue;
    //int last_received;

    //// discard confirmed frames as appropriate
    //int total_min_confirmed = MAX_INT;
    //for (queue = 0; queue < _num_players; queue++)
    //{
    //  bool queue_connected = true;
    //  int queue_min_confirmed = MAX_INT;
    //  Utils.Log("considering playerIndex %d.", queue);
    //  for (i = 0; i < _endpoints.Count; i++)
    //  {
    //    // we're going to do a lot of logic here in consideration of endpoint i.
    //    // keep accumulating the minimum confirmed point for all n*n packets and
    //    // throw away the rest.
    //    if (_endpoints[i].IsRunning())
    //    {
    //      bool connected = _endpoints[i].GetPeerConnectStatus((int)queue, &last_received);

    //      queue_connected = queue_connected && connected;
    //      queue_min_confirmed = Math.Min(last_received, queue_min_confirmed);
    //      Utils.Log("  endpoint %d: connected = %d, last_received = %d, queue_min_confirmed = %d.", i, connected, last_received, queue_min_confirmed);
    //    }
    //    else
    //    {
    //      Utils.Log("  endpoint %d: ignoring... not running.", i);
    //    }
    //  }
    //  // merge in our local status only if we're still connected!
    //  if (!_local_connect_status[queue].disconnected)
    //  {
    //    queue_min_confirmed = Math.Min(_local_connect_status[queue].last_frame, queue_min_confirmed);
    //  }
    //  Utils.Log("  local endp: connected = %d, last_received = %d, queue_min_confirmed = %d.", !_local_connect_status[queue].disconnected, _local_connect_status[queue].last_frame, queue_min_confirmed);

    //  if (queue_connected)
    //  {
    //    total_min_confirmed = Math.Min(queue_min_confirmed, total_min_confirmed);
    //  }
    //  else
    //  {
    //    // check to see if this disconnect notification is further back than we've been before.  If
    //    // so, we need to re-adjust.  This can happen when we detect our own disconnect at frame n
    //    // and later receive a disconnect notification for frame n-1.
    //    if (!_local_connect_status[queue].disconnected || _local_connect_status[queue].last_frame > queue_min_confirmed)
    //    {
    //      Utils.Log("disconnecting playerIndex %d by remote request.", queue);
    //      DisconnectPlayer(queue, queue_min_confirmed);
    //    }
    //  }
    //  Utils.Log("  total_min_confirmed = %d.", total_min_confirmed);
    //}
    //return total_min_confirmed;
  }

  // ----------------------------------------------------------------------------------------------------------
  public bool IncrementFrame()
  {
    // Utils.Log("End of frame (%d)...", _sync.GetFrameCount());
    _sync.IncrementFrame();

    // Are we polling after the sync so that we can get sync events?
    // If we are just trying to grab network packets, then doesn't it make more
    // sense to have poll BEFORE we sync so we can get whatever, if any, packets
    // that may have arrived?
    DoPoll(0);
    PollSyncEvents();

    return true;
  }

  // ----------------------------------------------------------------------------------------------------------
  // NOTE:  There is no such thing as a sync event!
  private void PollSyncEvents()
  {
    SyncEvent e = new SyncEvent();
    while (_sync.GetEvent(ref e))
    {
      OnSyncEvent(e);
    }
    return;
  }

  // ----------------------------------------------------------------------------------------------------------
  // NOTE: Referefence implementation (p2p.cpp) does not implement this function
  // either.  I think that this is more stuff that was left out or not needed....
  private void OnSyncEvent(SyncEvent e)
  {
    throw new NotImplementedException();
  }

  // ----------------------------------------------------------------------------------------
  /// <summary>
  /// Sync the inputs for all players for the current frame.
  /// This sends the local inputs, receives the remote ones, and intiates any rollbacks if needed.
  /// </summary>
  public virtual bool SyncInput(in byte[] values, int isize, int maxPlayers)
  {
    if (_synchronizing) { return false; }


    // If we are rolling back, there is no need to attempt to add a local input.
    // The call will result in an error code anyway....
    if (!_sync.InRollback())
    {
      if (!AddLocalInput(values, isize)) { return false; }
    }

    // NOTE: We aren't doing anything with the flags... I think the system is probably using the event codes
    // to playerIndex this kind of thing......
    _sync.SynchronizeInputs(values, isize * maxPlayers);


    return true;

  }

  // ----------------------------------------------------------------------------------------
  protected virtual bool AddLocalInput(byte[] values, int isize)
  {

    // NOTE: When this function is called, we already know that we aren't in rollback!
    // REDUNDANT CHECK:
    if (_sync.InRollback())
    {
      return false;
    }
    // REDUNDANT CHECK:
    if (_synchronizing)
    {
      return false;
    }

    GameInput input = new GameInput();
    input.init(-1, values, isize);

    // Feed the input for the current frame into the synchronzation layer.
    if (!_sync.AddLocalInput(Options.PlayerIndex, ref input))
    {
      // return GGPO_ERRORCODE_PREDICTION_THRESHOLD;
      return false;
    }

    if (input.frame != GameInput.NULL_FRAME)
    { // xxx: <- comment why this is the case
      // Update the local connect status state to indicate that we've got a
      // confirmed local frame for this player.  this must come first so it
      // gets incorporated into the next packet we send.

      // NOTE: All endpoints send out the _local_connect_status data with each message.
      // An ideal implemetation would have a single 'client' that we set this data on,
      // and then all endpoints would also be contained internally.
      Utils.LogIt(LogCategories.INPUT, "local frame for: %d - %d", Options.PlayerIndex, input.frame);
      _local_connect_status[Options.PlayerIndex].last_frame = input.frame;

      // Send the input to all the remote players.
      // NOTE: This queues input, and it gets pumped out later....
      // NOTE: In a two player game, only one of these endpoints has the 'udp' member set, and so
      // only one of them will actully do anything.....
      int epLen = _endpoints.Count;
      for (int i = 0; i < epLen; i++)
      {
        var ep = _endpoints[i];
        ep.SendInput(ref input);
      }

      //if (ReplayClient != null)
      //{
      //  ReplayClient.SendInput(ref input);
      //}
    }

    return true;
  }


  // ----------------------------------------------------------------------------------------
  // REFACTOR: 'HandleEvents'
  protected void PollUdpProtocolEvents()
  {
    // throw new NotImplementedException();
    var evt = new UdpEvent();
    for (UInt16 i = 0; i < _endpoints.Count; i++)
    {
      var ep = _endpoints[i];

      // NOTE: Local players aren't really going to have events because they don't poll or receive messages.
      while (ep.GetEvent(ref evt))
      {
        OnUdpProtocolPeerEvent(ref evt, ep);
      }
    }
  }

  // ----------------------------------------------------------------------------------------------------------
  protected virtual void OnUdpProtocolPeerEvent(ref UdpEvent evt, GGPOEndpoint endpoint)
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
          if (!_local_connect_status[playerIndex].disconnected)
          {

            int current_remote_frame = _local_connect_status[playerIndex].last_frame;
            int new_remote_frame = evt.u.input.frame;
            Utils.ASSERT(current_remote_frame == -1 || new_remote_frame == (current_remote_frame + 1));

            _sync.AddRemoteInput(playerIndex, ref evt.u.input);

            // Notify the other endpoints which frame we received from a peer
            Utils.LogIt(LogCategories.INPUT, "remote frame for: %d - %d", playerIndex, evt.u.input.frame);
            _local_connect_status[playerIndex].last_frame = evt.u.input.frame;
          }
        }
        else
        {
          int x = 10;
        }
        break;

      case EEventType.Datagram:

        // NOTE: At some point we will end up with a first class disconnect signal / event!
        if (evt.u.datagram.code == UdpEvent.DATAGRAM_CODE_DISCONNECT)
        {
          OnDisconnect(endpoint);
        }

        break;

      case EEventType.Disconnected:
        DisconnectEndpoint(endpoint);
        break;

      case EEventType.SyncFailed:
        GGPOEvent e = new GGPOEvent();
        e.event_code = EEventCode.GGPO_EVENTCODE_CONNECT_TIMEOUT;
        e.player_index = endpoint.PlayerIndex;
        e.isReplayEndpoint = (uint8_t)(endpoint.IsReplayClient ? 1 : 0);
        _callbacks.on_event(ref e);

        break;

      default:
        Utils.LogIt(LogCategories.DEBUG, "There is no handler for event type:%s", (int)evt.type);
        break;
    }
  }

  // ----------------------------------------------------------------------------------------------------------
  public virtual void OnDisconnect(GGPOEndpoint endpoint)
  {
    GGPOEvent e = new GGPOEvent();
    e.event_code = EEventCode.GGPO_EVENTCODE_DISCONNECTED_FROM_PEER;
    e.player_index = endpoint.PlayerIndex;
    e.isReplayEndpoint = (uint8_t)(endpoint.IsReplayClient ? 1 : 0);
    _callbacks.on_event(ref e);

    // The given endpoint is indicating that it wants to disconnect.
    // For now, I think that we will only care about the case where it is a replay appliance...
    if (endpoint.IsReplayClient)
    {
      // Effectively disconnect the endpoint so it no longer sends / receives data...
      endpoint.Disconnect(0, false);

      // Log.Info($"Received disconnect signal from ReplayAppliance on session: {this.SessionId}");
      // this._endpoints.Remove(endpoint);
    }
  }

  // ----------------------------------------------------------------------------------------------------------
  bool DisconnectEndpoint(GGPOEndpoint endpoint)
  {
    if (!endpoint.IsReplayClient)
    {
      byte playerIndex = endpoint.PlayerIndex;

      if (_local_connect_status[playerIndex].disconnected)
      {
        // TODO: Log this !
        return false; //GGPO_ERRORCODE_PLAYER_DISCONNECTED;
      }

      if (!_endpoints[playerIndex].IsInitialized())
      {
        int current_frame = _sync.GetFrameCount();
        // xxx: we should be tracking who the local player is, but for now assume
        // that if the endpoint is not initalized, this must be the local player.
        Utils.LogIt(LogCategories.ENDPOINT, "Disconnecting local player %d at frame %d by user request.", playerIndex, _local_connect_status[playerIndex].last_frame);
        int epCount = _endpoints.Count;
        for (UInt16 i = 0; i < epCount; i++)
        {
          var ep = _endpoints[i];
          if (ep.IsInitialized())
          {
            DisconnectEndpoint(ep, current_frame);
          }
        }
      }
      else
      {
        Utils.LogIt(LogCategories.ENDPOINT, "Disconnecting player: %d at frame: %d by user request.", playerIndex, _local_connect_status[playerIndex].last_frame);
        DisconnectEndpoint(endpoint, _local_connect_status[playerIndex].last_frame); // playerIndex, _local_connect_status[playerIndex].last_frame);
      }
    }
    else
    {
      // We need to remove the replay client.
      // NOTE: At this point, the disconnect logic isn't very good.
    }


    return true;
  }


  // --------------------------------------------------------------------------------------------------------------
  //void DisconnectPlayer(byte playerIndex, int syncto)
  [Obsolete("This never actually gets called in the emulator, or any other implementation at at this time so it is basicall garbage.  We will get rid of it, and a lot of the concepts around it!")]
  void DisconnectEndpoint(GGPOEndpoint endpoint, int syncto)
  {
    Log.Warning($"Function: {nameof(DisconnectEndpoint)} is called, but doesn't do anything!");
    return;
  }

  // ----------------------------------------------------------------------------------------------------------
  protected unsafe virtual void OnUdpProtocolEvent(ref UdpEvent evt, GGPOEndpoint endpoint)
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

        if (!isReplay)
        {
          string name = evt.u.connected.GetPlayerName();
          _PlayerNames[playerIndex] = name;
        }
        // strcpy_s(_PlayerNames[playerIndex], evt.u.connected.playerName);
        // strcpy_s(info.u.connected.playerName, evt.u.connected.playerName);

        _callbacks.on_event(ref info);
        break;
      case EEventType.Synchronizing:
        info.event_code = EEventCode.GGPO_EVENTCODE_SYNCHRONIZING_WITH_PEER;
        info.player_index = playerIndex;
        info.u.synchronizing.count = evt.u.synchronizing.count;
        info.u.synchronizing.total = evt.u.synchronizing.total;
        _callbacks.on_event(ref info);
        break;

      case EEventType.Synchronized:
        info.event_code = EEventCode.GGPO_EVENTCODE_SYNCHRONIZED_WITH_PEER;
        info.player_index = playerIndex;
        _callbacks.on_event(ref info);

        CheckInitialSync();
        break;

      case EEventType.NetworkInterrupted:
        info.event_code = EEventCode.GGPO_EVENTCODE_CONNECTION_INTERRUPTED;
        info.player_index = playerIndex;
        info.u.connection_interrupted.disconnect_timeout = evt.u.network_interrupted.disconnect_timeout;
        _callbacks.on_event(ref info);
        break;

      case EEventType.NetworkResumed:
        info.event_code = EEventCode.GGPO_EVENTCODE_CONNECTION_RESUMED;
        info.player_index = playerIndex;
        _callbacks.on_event(ref info);
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
          if (endpoint.IsDisconnected) { return; }
          // I think we can just disconnect the endpoint directly, and not care about what the
          // playerIndex might be...
          //// TODO: Maybe a differnt logging category for all this?
          //if (endpoint.EndpointType == EEndpointType.ReplayAppliance){
          //  Log.Info("Got disconnect signal from replay appliance!");
          //}
          endpoint.Disconnect(this.CurrentFrame, false);
        }

        _callbacks.on_event(ref info);

        break;
    }
  }

  // ----------------------------------------------------------------------------------------------------------
  protected virtual void CheckInitialSync()
  {
    int i;

    if (_synchronizing)
    {
      // Check to see if everyone is now synchronized.  If so,
      // go ahead and tell the client that we're ok to accept input.
      int epLen = _endpoints.Count;
      for (i = 0; i < epLen; i++)
      {
        var ep = _endpoints[i];
        int epi = ep.PlayerIndex;

        if (ep.IsLocalPlayer) { continue; }
        if (ep.IsReplayClient && ep.IsDisconnected) { continue; }
        if (!ep.IsSynchronized()) { return; }
      }

      GGPOEvent info = new GGPOEvent();
      info.event_code = EEventCode.GGPO_EVENTCODE_RUNNING;
      _callbacks.on_event(ref info);
      _synchronizing = false;
    }
  }

  // ----------------------------------------------------------------------------------------
  /// <summary>
  /// Disallow new remotes / connections from being added.
  /// It is recommended that you call this after all endpoints are setup and you don't have a
  /// good reason to reconnect them.
  /// </summary>
  public void Lock()
  {
    IsLocked = true;
  }

  // ----------------------------------------------------------------------------------------
  public GGPOEndpoint? GetLocalPlayer()
  {
    var res = (from x in _endpoints where x.IsLocalPlayer select x).SingleOrDefault();
    return res;
  }

  // ----------------------------------------------------------------------------------------
  public GGPOEndpoint? GetRemotePlayer()
  {
    var res = (from x in _endpoints where !x.IsLocalPlayer && !x.IsReplayClient select x).SingleOrDefault();
    return res;
  }

  // ----------------------------------------------------------------------------------------
  public GGPOEndpoint? GetReplayApplianceEndpoint()
  {
    var res = (from x in _endpoints where x.IsReplayClient select x).SingleOrDefault();
    return res;
  }
}

// ==========================================================================================
public class GGPOClientOptions
{

  private const int DEFAULT_INPUT_SIZE = 5;   // This is for 3rd strike.
  private const int MAX_PLAYER_COUNT = 4;     // NOTE: This should be 2, but that will make trouble!
  private const int DEFAULT_PLAYER_COUNT = 2;

  // ----------------------------------------------------------------------------------------
  public GGPOClientOptions(string gameName_, byte playerIndex_, int localPort_, UInt32 clientVersion_, UInt64 sessionId_)
  {
    GameName = gameName_;
    PlayerIndex = playerIndex_;
    LocalPort = localPort_;
    ClientVersion = clientVersion_;
    SessionId = sessionId_;
  }

  // --------------------------------------------------------------------------------------------------------------------------
  public void SetReplayOption(string hostAndPort, int replayTimeout)
  {
    if (!string.IsNullOrWhiteSpace(hostAndPort))
    {
      var x = new RemoteEndpointData(hostAndPort);

      ReplayHost = x.Host;
      ReplayPort = x.Port;
      PeerConnectTimeout = replayTimeout;
    }
  }

  public string GameName { get; set; }

  /// <summary>
  /// Index of the player, coresponding to 0 == player 1, 1 == player 2, etc.
  /// </summary>
  public byte PlayerIndex { get; set; }
  public int LocalPort { get; set; } = Defaults.LOCAL_PORT;
  public int InputSize { get; set; } = DEFAULT_INPUT_SIZE;
  public int MaxPlayerCount { get; set; } = DEFAULT_PLAYER_COUNT;
  public GGPOSessionCallbacks Callbacks { get; set; } = null!;

  /// <summary>
  /// Session Id, which corresponds to unix time in milliseconds.
  /// Only used in replay contexts.
  /// </summary>
  public UInt64 SessionId { get; private set; }

  public string? ReplayHost { get; private set; } = null;
  public int ReplayPort { get; private set; } = -1;

  /// <summary>
  /// How long will we wait for a replay client connection to timeout.
  /// </summary>
  public int PeerConnectTimeout { get; private set; } = GGPOConsts.UNLIMITED_TIME; // GGPOConsts.DEFAULT_CONNECT_TIMEOUT;

  /// <summary>
  /// Verseion of this client.  It is a 32 bitmasked number as follows:
  /// MAJOR (8bits) - MINOR (8bits) - REVISION (8bits) - GGPO VERSION (8bits)
  /// TODO: Put this information in the readme somewhere.....
  /// </summary>
  public UInt32 ClientVersion { get; set; }

  // NOTE: This should only be set in testing scenarios.  We may drop it in the future altogether.
  public int IdleTimeout { get; set; } = 1;
}

// ==========================================================================================
public static class Defaults
{
  public const int LOCAL_PORT = 7001;
  public const int REMOTE_PORT = 7000;
  public const int PROTOCOL_VERSION = 4;
  public const byte PLAYER_TWO = 2;

  public const string REMOTE_HOST = "127.0.0.1";

  public const int REPLAY_TIMEOUT = 5000;
}


// ==========================================================================================
public interface IClockSource
{
  /// <summary>
  /// The current time in Milliseconds
  /// </summary>
  public int CurTime { get; }
}


// ==============================================================================================================================
public class ClockTimer : IClockSource
{
  private Stopwatch Clock = Stopwatch.StartNew();
  public int CurTime { get { return (int)Clock.ElapsedMilliseconds + 1; } }
}


// ==========================================================================================
public interface IGGPOClient : IClockSource
{
  /// <summary>
  /// Optional value to identify the client.  Useful for testing and logging.  0 == Not Set.
  /// </summary>
  int ID { get; }

  IUdpBlaster UDP { get; }
  UInt32 ClientVersion { get; }
  string LocalPlayerName { get; }
  int CurrentFrame { get; }
  string GameName { get; }

  [Obsolete("not yet, but in the future, all connection timeouts will be set on a per-endpoint basis.")]
  int ConnectTimeout { get; }

  bool IsReadyToSync(ref UdpMsg syncRequest);

  void OnDisconnect(GGPOEndpoint endpoint);
}

