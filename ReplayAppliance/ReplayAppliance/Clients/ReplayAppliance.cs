using drewCo.Tools;
using drewCo.Tools.Logging;
using System.Net;
using System.Security.Cryptography;
using System.Transactions;

namespace funscrew.Clients;



// ==============================================================================================================================
/// <summary>
/// This is the class that will be responsible for receiving and logging input data from
/// two or more players.
/// </summary>
public class ReplayAppliance
{
  // private ReplayApplianceOptions ReplayOptions = default!;

  // The two clients that we expect to receive data from.  These will be the remote endpoints that we
  // then set up.
  // private HashSet<SocketAddress> ConnectedClients = new HashSet<SocketAddress>();
  // private bool AllConnected = false;
  // private List<int> ConnectedPlayerIndexes = new List<int>();

  public List<string> Errors { get; private set; } = new List<string>();

  // We will have a list of active sessions, each of which will have their own game recorder.
  private object SessionLock = new object();
  private Dictionary<uint64_t, ReplaySession> ActiveSessions = new Dictionary<uint64_t, ReplaySession>();
  private Dictionary<AddrHash, ReplaySession> AddressToSession = new Dictionary<AddrHash, ReplaySession>();

  /// <summary>
  /// This is the set of all known connected clients regardless of their session.
  /// Each entry is a hash of the IP + port.
  /// When sessions expire or are closed, this set should be updated.
  /// </summary>
  // private HashSet<UInt64> AllConnections = new HashSet<UInt64>();

  // public GameRecorder Recorder { get; private set; }
  private ReplayOptions Options = null!;

  private CancellationTokenSource CTSource = new CancellationTokenSource();
  private CancellationToken CancelToken = default!;

  private IClockSource Clock = null!;
  public IUdpBlaster UDP { get; private set; } = null!;

  private byte[] _ReceiveBuffer = new byte[4096];

  // --------------------------------------------------------------------------------------------------------------------------
  public ReplayAppliance(ReplayOptions ops_, IUdpBlaster udp_, IClockSource clock_)
  {
    Options = ops_;
    UDP = udp_;

    // NOTE: I don't think that this should apply in test cases....
    // Maybe we will make it an option or something....
    if (!UDP.IsBlocking && false)
    {
      throw new InvalidOperationException("ReplayAppliance requires a blocking IUdpBlaster instance!");
    }

    Clock = clock_;
    // LocalConnectStatus = localConnectStatus_;

    CancelToken = CTSource.Token;

    if (string.IsNullOrWhiteSpace(Options.ReplayDataDir))
    {
      throw new InvalidOperationException("Invalid data directory!");
    }
    FileTools.CreateDirectory(Options.ReplayDataDir);

  }

  // --------------------------------------------------------------------------------------------------------------------------
  public unsafe ReplaySession BeginSession(UInt64 sessionId, SessionOptions sessionOps)
  {
    Log.Info($"Starting new session with id: {sessionId}...");
    lock (SessionLock)
    {
      if (ActiveSessions.ContainsKey(sessionId))
      {
        throw new InvalidOperationException($"Session id: {sessionId} is already active!");
      }


      bool overwrite = sessionId == SessionPrimer.TEST_SESSION_ID;
      if (overwrite)
      {
        Log.Info($"Test Session ID: {SessionPrimer.TEST_SESSION_ID} detected!  Existing data will be overwritten!");
      }

      var gameData = new CGameData()
      {
        GameName = sessionOps.GameName,
        GameVersion = sessionOps.GameVersion,
        MaxPlayerCount = sessionOps.MaxPlayerCount,
        TotalInputSize = sessionOps.TotalInputSize
      };

      uint8_t index = 0;
      foreach (var item in sessionOps.PlayerNames)
      {
        gameData.SetPlayerName(index, item);
        ++index;
      }

      var recorder = new GameRecorder(gameData, Options.ReplayDataDir, sessionId, overwrite);


      // TODO: We can care about handling events later (maybe begingame), but the rest of these can be safely ignored!
      var callbacks = new GGPOSessionCallbacks()
      {
        on_event = NoOp_Event,
        free_buffer = NoOp_FreeBuffer,
        begin_game = NoOp_BeginGame,
        save_game_state = NoOp_SaveGame,
        load_game_state = NoOp_LoadGame,
      };

      var session = new ReplaySession(sessionId, recorder, sessionOps, callbacks);
      ActiveSessions.Add(sessionId, session);

      return session;
    }
  }

  // --------------------------------------------------------------------------------------------------------------------------
  public bool Update()
  {
    EndPoint ep = default!;

    // This is a blocking call!
    int received = UDP.Receive(_ReceiveBuffer, ref ep);
    if (received == 0)
    {
        return false;
    }

    UdpMsg msg = new UdpMsg();
    UdpMsg.FromBytes(_ReceiveBuffer, ref msg, received);

    if (msg.header.type == EMsgType.Heartbeat)
    {
      Log.Verbose("Heartbeat!");
      return true;
    }

    ReplaySession? sess = DeliverMessage(ref msg, received, (IPEndPoint)ep);
    if (sess == null)
    {
      // NOTE: Possibly got data from a blacklist client?
      // Ideally this branch would not be possible...
      // TODO: LOG?
      return false;
      // goto lblReceiveData;
    }

    // Now update the session so it can do its thing.....
    sess.DoPoll();
    return true;
    // The rest of 'DoPoll'


    //// TEMP: TEST:
    //if (Clock.CurTime > 2000)
    //{
    //  Log.Info("Test timeout has expired!");
    //  break;
    //}
  }

  // --------------------------------------------------------------------------------------------------------------------------

  // NOTE: This should only be used in production.  Not suitable for test code....
  public Task BeginUpdateLoop()
  {
    var res = Task.Factory.StartNew(() =>
    {

      EndPoint ep = default!;
      while (true)
      {

      lblReceiveData:

        bool updated = Update();
        if (!updated) {
          // AAIIIIIEEEEE EVIL! EVIL! EVIL! CALL THE COAST GUARD!!!!!
          goto lblReceiveData;
        }

        // This is a blocking call!
        int received = UDP.Receive(_ReceiveBuffer, ref ep);
        if (received == 0)
        {
          // AAIIIIIEEEEE EVIL! EVIL! EVIL! CALL THE COAST GUARD!!!!!
          goto lblReceiveData;
        }

        UdpMsg msg = new UdpMsg();
        UdpMsg.FromBytes(_ReceiveBuffer, ref msg, received);

        if (msg.header.type == EMsgType.Heartbeat)
        {
          goto lblReceiveData;
        }

        ReplaySession? sess = DeliverMessage(ref msg, received, (IPEndPoint)ep);
        if (sess == null)
        {
          // NOTE: Possibly got data from a blacklist client?
          goto lblReceiveData;
        }

        sess.DoPoll();


        // The rest of 'DoPoll'


        // TEMP: TEST:
        if (Clock.CurTime > 2000)
        {
          Log.Info("Test timeout has expired!");
          break;
        }
      }


    }, CancelToken);

    return res;
  }

  // --------------------------------------------------------------------------------------------------------------------------
  private ReplaySession? DeliverMessage(ref UdpMsg msg, int received, IPEndPoint receivedFrom)
  {
    // NOTE: This is going to make garbage.... lame!
    // SocketAddress ipa = receivedFrom.Serialize();
    AddrHash hashedAddr = IUdpBlaster.GetAddrHash(receivedFrom);

    ReplaySession? useSession = null!;

    if (msg.header.type == EMsgType.SyncRequest && !this.AddressToSession.ContainsKey(hashedAddr)) // .Keys.Contains(hashedAddr))
    {
      // Validate session ID!
      var sid = msg.u.sync_request.session_id;
      if (!this.ActiveSessions.TryGetValue(sid, out useSession))
      {
        Log.Warning($"Invalid session ID!  Connection from: {receivedFrom.ToString()} should be blacklisted!");
        // throw new InvalidOperationException("Invalid session ID!  Connection should be blacklisted!");
        UDP.Blacklist.Add(hashedAddr);
        return null;
      }

      // We are going to add this connection....
      lock (SessionLock)
      {
        // var ep = ConnectNewClient(ref msg, receivedFrom);
        // NOTE: We should have a sync request with the correct request ID set!
        // Don't know what to do if we don't... probably just ignore it...
        var newEndpoint = AddReplayEndpoint(useSession, receivedFrom, msg);
        AddressToSession.Add(hashedAddr, useSession);
      }
    }
    else
    {
      // We should not get to this branch for clients were we have not received a sync request first!
      AddressToSession.TryGetValue(hashedAddr, out useSession);
      if (useSession == null)
      {
        throw new InvalidOperationException("There is no session associated with this address!");
      }
    }

    useSession.DeliverMessage(ref msg, received, receivedFrom);

    return useSession;

  }


  // --------------------------------------------------------------------------------------------------------------------------
  private GGPOEndpoint AddReplayEndpoint(ReplaySession replaySesh, IPEndPoint from, UdpMsg msg)
  {
    var playerIndex = msg.u.sync_request.player_index;
    var ops = new GGPOEndpointOptions()
    {
      Delay = 0,
      IsLocal = false,
      PlayerIndex = playerIndex, // GGPOConsts.REPLAY_APPLIANCE_PLAYER_INDEX,
      PlayerName = "REPLAY_APP",
      RemoteHost = from.Address.ToString(),
      RemotePort = from.Port,
      Runahead = 0,
      IsReplayClient = true,
      TestOptions = new TestOptions(),
      SessionId = replaySesh.SessionId
    };

    // NOTE: We may not want to send out the sync request immediately on these endpoints?
    // Nah -> it should be OK that they bounce around.....
    var connectStatus = new ConnectStatus[GGPOConsts.MAX_PLAYERS];
    var res = new ReplayEndpoint(replaySesh, ops, connectStatus);
    res.AddressHash = IUdpBlaster.GetAddrHash(from);

    replaySesh.AddConnection(res);

    Log.Info($"A remote endpoint for session: {replaySesh.SessionId} was added...");

    return res;
  }


  // --------------------------------------------------------------------------------------------------------------------------
  public void Shutdown(bool forceDisconnect)
  {
    throw new NotImplementedException();
  }


  #region Callback Event Handlers

  // --------------------------------------------------------------------------------------------------------------------------
  private unsafe bool NoOp_LoadGame(byte** buffer, int len)
  {
    Log.Debug("ggpo callback: loadgame");
    return true;
  }

  // --------------------------------------------------------------------------------------------------------------------------
  private unsafe bool NoOp_SaveGame(byte** buffer, int* len, int* checksum, int frame)
  {
    Log.Debug("ggpo callback: savegame");
    return true;
  }

  // --------------------------------------------------------------------------------------------------------------------------
  private bool NoOp_Event(ref GGPOEvent arg)
  {
    Log.Debug("ggpo callback: event");
    return true;
  }

  // --------------------------------------------------------------------------------------------------------------------------
  private void NoOp_BeginGame(string gameName)
  {
    Log.Debug("ggpo callback: begingame");
  }

  // --------------------------------------------------------------------------------------------------------------------------
  private unsafe bool NoOp_FreeBuffer(byte* arg)
  {
    Log.Debug("ggpo callback: freebuffer");
    return true;
  }

  #endregion


  //// --------------------------------------------------------------------------------------------------------------------------
  //private void ValidateOptions()
  //{
  //  if (string.IsNullOrWhiteSpace(ReplayOptions.GameName))
  //  {
  //    throw new InvalidOperationException("Invalid game name!");
  //  }
  //}

  //// --------------------------------------------------------------------------------------------------------------------------
  //private void InitGameRecorder()
  //{
  //  if (string.IsNullOrWhiteSpace(ReplayOptions.GameVersion))
  //  {
  //    throw new InvalidOperationException("Invalid game version!");
  //  }

  //  var gameData = new CGameData()
  //  {
  //    GameName = ReplayOptions.GameName,
  //    GameVersion = ReplayOptions.GameVersion,
  //    MaxPlayerCount = (UInt16)ClientOptions.MaxPlayerCount,
  //    TotalInputSize = (UInt16)(ClientOptions.InputSize * ClientOptions.MaxPlayerCount)
  //  };

  //  Recorder = new GameRecorder(gameData,
  //  ReplayOptions.DataDir,
  //  ClientOptions.SessionId
  //  );
  //}

  //// --------------------------------------------------------------------------------------------------------------------------
  //protected override void HandleDisconnect(GGPOEndpoint endpoint)
  //{
  //  base.HandleDisconnect(endpoint);
  //  if (endpoint.IsDisconnected)
  //  {
  //    Log.Info("A player disconnected.... wrapping up....");
  //  }
  //}

  //// --------------------------------------------------------------------------------------------------------------------------
  //public override void DisconnectAll()
  //{
  //  base.DisconnectAll();

  //  this.AllConnected = false;
  //  this.ConnectedPlayerIndexes.Clear();
  //}


  //// --------------------------------------------------------------------------------------------------------------------------
  //public GGPOEndpoint GetEndpoint(int index)
  //{
  //  return _endpoints[index];
  //}

  //// --------------------------------------------------------------------------------------------------------------------------
  //protected override void DeliverMessage(ref UdpMsg msg, int received, EndPoint receivedFrom)
  //{
  //  // NOTE: This is going to make garbage.... lame!
  //  SocketAddress ipa = receivedFrom.Serialize();
  //  if (msg.header.type == EMsgType.SyncRequest && !this.ConnectedClients.Contains(ipa))
  //  {
  //    int index = ConnectedClients.Count;
  //    var ep = ConnectNewClient(ref msg, ipa);
  //    if (ep != null)
  //    {
  //      _endpoints.Add(ep);
  //    }
  //  }

  //  // Now that the end
  //  base.DeliverMessage(ref msg, received, receivedFrom);

  //}

  //// --------------------------------------------------------------------------------------------------------------------------
  //private GGPOEndpoint ConnectNewClient(ref UdpMsg msg, SocketAddress ipa)
  //{
  //  // JFC can we make this any more of a pain in the ass?
  //  // TODO: This will probably go away when we fix how we represent this stuff....
  //  // Also, this won't work with IPV6, booooo
  //  var bufferData = ipa.Buffer.ToArray();
  //  byte[] port = new byte[2];
  //  port[0] = bufferData[3];
  //  port[1] = bufferData[2];
  //  var remotePort = BitConverter.ToUInt16(port);
  //  string remoteHost = $"{bufferData[4]}.{bufferData[5]}.{bufferData[6]}.{bufferData[7]}";

  //  // Make sure that session id + player index are correct....
  //  var sid = msg.u.sync_request.session_id;
  //  if (sid != ReplayOptions.SessionId)
  //  {
  //    // We don't want to receive from this endpoint anymore.....
  //    // How can we block receiving?
  //    AddError($"Connection attempt with invalid session id! ({ReplayOptions.SessionId}-{msg.u.sync_request.session_id}) [adding to blacklist]");
  //    UDP.AddToBlacklist(ipa);
  //    return null;
  //  }

  //  // We also want to check to see if we are getting the correct player index.
  //  // NOTE: If a certain player index is already connected, then we want to
  //  // reject those other connections that are reporting the wrong one!
  //  var pi = msg.u.sync_request.player_index;
  //  if (ConnectedPlayerIndexes.Contains(pi))
  //  {
  //    AddError($"The player with index: {pi} has already been connected! [adding to blacklist]");
  //    UDP.AddToBlacklist(ipa);
  //    return null;
  //  }

  //  // NOTE: We should have a sync request with the correct request ID set!
  //  // Don't know what to do if we don't... probably just ignore it...
  //  var newEndpoint = AddReplayEndpoint(remoteHost, remotePort, msg);

  //  Log.Info("A remote endpoint was added...");

  //  this.ConnectedClients.Add(ipa);
  //  if (this.ConnectedClients.Count == 2)
  //  {
  //    AllConnected = true;
  //    Log.Info("All clients are setup...");
  //  }

  //  return newEndpoint;

  //  // Send the sync reply, immediately.
  //  // newEndpoint.OnSyncRequest(ref msg, received);

  //}

  //// --------------------------------------------------------------------------------------------------------------------------
  //protected override int PollPlayers(int current_frame)
  //{
  //  // Replay appliance doesn't really do anything at this point, tho maybe this is where
  //  // we do stuff like confim inputs or whatever.....?
  //  // return base.PollPlayers(current_frame);
  //  return current_frame;
  //}

  // --------------------------------------------------------------------------------------------------------------------------
  private void AddError(string msg)
  {
    Log.Error(msg);
    this.Errors.Add(msg);
  }

  //// --------------------------------------------------------------------------------------------------------------------------
  //protected override void CheckInitialSync()
  //{
  //  if (_synchronizing)
  //  {
  //    int epLen = _endpoints.Count;
  //    if (epLen < 2) { return; }

  //    for (int i = 0; i < epLen; i++)
  //    {
  //      var ep = _endpoints[i];
  //      if (!ep.IsSynchronized() && !_local_connect_status[ep.PlayerIndex].disconnected)
  //      {
  //        return;
  //      }
  //    }

  //    GGPOEvent info = new GGPOEvent();
  //    info.event_code = EEventCode.GGPO_EVENTCODE_RUNNING;
  //    _callbacks.on_event(ref info);
  //    _synchronizing = false;
  //  }
  //}

  //// --------------------------------------------------------------------------------------------------------------------------
  //private GGPOEndpoint AddReplayEndpoint(string remoteHost, int remotePort, UdpMsg msg)
  //{
  //  if (remoteHost == "0.0.0.0") { throw new InvalidOperationException("Invalid host!"); }
  //  if (remotePort == 0) { throw new InvalidOperationException("Invalid port!"); }

  //  var playerIndex = msg.u.sync_request.player_index;
  //  var ops = new GGPOEndpointOptions()
  //  {
  //    Delay = 0,
  //    IsLocal = false,
  //    PlayerIndex = playerIndex, // GGPOConsts.REPLAY_APPLIANCE_PLAYER_INDEX,
  //    PlayerName = "REPLAY_APP",
  //    RemoteHost = remoteHost,
  //    RemotePort = remotePort,
  //    Runahead = 0,
  //    IsReplayClient = true,
  //    TestOptions = new TestOptions()
  //  };

  //  // NOTE: We may not want to send out the sync request immediately on these endpoints?
  //  // Nah -> it should be OK that they bounce around.....
  //  var remote = new ReplayEndpoint(this, ops, _local_connect_status);

  //  ConnectedPlayerIndexes.Add(playerIndex);

  //  return remote;
  //}

  //// --------------------------------------------------------------------------------------------------------------------------
  //public override bool SyncInput(in byte[] values, int isize, int maxPlayers)
  //{
  //  // TODO: Maybe this is where we merge + ACK inputs?
  //  return true;
  //}

  //// --------------------------------------------------------------------------------------------------------------------------
  //protected override bool AddLocalInput(byte[] values, int isize)
  //{
  //  // Do nothing, we don't have local inputs!
  //  return true;
  //}


  //// --------------------------------------------------------------------------------------------------------------------------
  ///// <summary>
  ///// This is where the inputs for the different frames will get merged, recorded, and later sent out.
  ///// </summary>
  //// private bool _WarningSent = false;
  //internal void MergeInput(ref GameInput input, int playerIndex)
  //{
  //  if (Recorder.HasError)
  //  {
  //    int x = 10;
  //    // We have detected an error in the recorder.  We will log this, and send disconnect
  //    // notices to all active clients.
  //    Log.Error($"There was a recording error: {Recorder.ErrorReason} : {Recorder.ErrorMessage}");
  //    DisconnectAll();
  //    return;
  //  }
  //  Recorder.AddInput(playerIndex, ref input);
  //}

}



