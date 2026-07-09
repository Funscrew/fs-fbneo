using drewCo.Tools;
using drewCo.Tools.Logging;
using System.Diagnostics;
using System.Net;

namespace funscrew.Clients;



// ==============================================================================================================================
/// <summary>
/// This is the class that will be responsible for receiving and logging input data from
/// two or more players.
/// </summary>
public class ReplayAppliance
{
  // The two clients that we expect to receive data from.  These will be the remote endpoints that we
  // then set up.
  public List<string> Errors { get; private set; } = new List<string>();

  // We will have a list of active sessions, each of which will have their own game recorder.
  private object SessionLock = new object();
  protected Dictionary<uint64_t, ReplaySession> IdToSession = new Dictionary<uint64_t, ReplaySession>();
  protected Dictionary<AddrHash, ReplaySession> AddressToSession = new Dictionary<AddrHash, ReplaySession>();
  protected List<ReplaySession> ActiveSessions = new List<ReplaySession>(0xff);
  protected List<ReplaySession> CompleteSessions = new List<ReplaySession>(0xff);

  private ReplayOptions Options = null!;

  private CancellationTokenSource CTSource = new CancellationTokenSource();
  private CancellationToken CancelToken = default!;

  private IClockSource Clock = null!;
  public IUdpBlaster UDP { get; private set; } = null!;

  private byte[] _ReceiveBuffer = new byte[4096];

  // OPTIONS: This is a hard value for how often we will update the sessions.
  private const milliseconds UPDATE_INTERVAL = 20;
  private long LastUpdateTime = 0;

  // --------------------------------------------------------------------------------------------------------------------------
  public ReplayAppliance(ReplayOptions ops_, IUdpBlaster udp_, IClockSource clock_)
  {
    Options = ops_;
    UDP = udp_;
    Clock = clock_;

    CancelToken = CTSource.Token;

    if (string.IsNullOrWhiteSpace(Options.ReplayDataDir))
    {
      throw new InvalidOperationException("Invalid data directory!");
    }
    FileTools.CreateDirectory(Options.ReplayDataDir);

  }

  // --------------------------------------------------------------------------------------------------------------------------
  public virtual unsafe ReplaySession BeginSession(UInt64 sessionId, SessionOptions sessOps)
  {
    Log.Info($"Starting new session with id: {sessionId}...");
    lock (SessionLock)
    {
      if (IdToSession.ContainsKey(sessionId))
      {
        throw new InvalidOperationException($"Session id: {sessionId} is already active!");
      }


      bool overwrite = sessionId == GGPOConsts.TEST_SESSION_ID;
      if (overwrite)
      {
        Log.Info($"Test Session ID: {GGPOConsts.TEST_SESSION_ID} detected!  Existing data will be overwritten!");
      }

      var gameData = new CGameData()
      {
        GameName = sessOps.GameName,
        GameVersion = sessOps.GameVersion,
        MaxPlayerCount = sessOps.MaxPlayerCount,
        TotalInputSize = sessOps.TotalInputSize
      };

      uint8_t index = 0;
      foreach (var item in sessOps.PlayerNames)
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

      var session = new ReplaySession(this.Clock, this.UDP, sessionId, recorder, sessOps, callbacks);
      IdToSession.Add(sessionId, session);
      ActiveSessions.Add(session);

      return session;
    }
  }

  // --------------------------------------------------------------------------------------------------------------------------
  public void Update()
  {
    // TODO: Some kind of completion flag here......
    // if (IsComplete) { return; }

    lock (SessionLock)
    {
      // FIX: This is going to create garbage....
      EndPoint ep = new IPEndPoint(IPAddress.Any, 0);//  default!;

      // This is a blocking call!
      while (true)
      {
        int received = UDP.Receive(_ReceiveBuffer, ref ep);
        if (received == 0)
        {
          break;
        }

        UdpMsg msg = new UdpMsg();
        UdpMsg.FromBytes(_ReceiveBuffer, ref msg, received);

        ReplaySession? sess = DeliverMessage(ref msg, received, (IPEndPoint)ep);
        if (sess == null)
        {
          Log.Error("Could not deliver message to session!  It may not be in service anymore....");
        }

        // Here we want to run updates after a certain amount of time.....
        UpdateSessions();
      }

      UpdateSessions();
    }
  }

  // --------------------------------------------------------------------------------------------------------------------------
  private void UpdateSessions()
  {
    // Log.Info("running updates on active sessions....");
    if (ActiveSessions.Count == 0) { return; }

    var now = this.Clock.CurTime;
    var elapsed = now - LastUpdateTime;
    if (elapsed >= UPDATE_INTERVAL)
    {
      // Update all sessions:
      // NOTE:  If we have a lot of sessions, we might get better perf by have some kind of
      // 'last updated' timestamp + 'force update every x. ms' type of deal.
      int len = ActiveSessions.Count;
      for (int i = 0; i < len; i++)
      {
        var sess = ActiveSessions[i];
        sess.DoPoll();

        if (sess.IsComplete) { CompleteSessions.Add(sess); }
      }
      CleanupCompleteSessions();

      LastUpdateTime = now;
    }
  }


  // --------------------------------------------------------------------------------------------------------------------------
  private void CleanupCompleteSessions()
  {
    lock (SessionLock)
    {
      int len = CompleteSessions.Count;
      for (int i = 0; i < len; i++)
      {
        var sess = CompleteSessions[i];
        EndSession(sess);
      }

      CompleteSessions.Clear();
    }
  }

  // --------------------------------------------------------------------------------------------------------------------------
  protected virtual void EndSession(ReplaySession sess)
  {
    Log.Info($"Session: {sess.SessionId} is complete and will be removed!");

    Debug.Assert(ActiveSessions.Contains(sess));
    Debug.Assert(IdToSession.ContainsKey(sess.SessionId));

    ActiveSessions.Remove(sess);
    IdToSession.Remove(sess.SessionId);

    int epCount = sess.EndpointCount;
    for (int j = 0; j < epCount; j++)
    {
      var hash = sess.Endpoints[j].AddressHash;
      Debug.Assert(AddressToSession.ContainsKey(hash));

      AddressToSession.Remove(hash);
    }
  }

  // --------------------------------------------------------------------------------------------------------------------------
  public void EndWork()
  {
    if (!CTSource.IsCancellationRequested)
    {
      CTSource.Cancel();
    }
  }

  // --------------------------------------------------------------------------------------------------------------------------
  // NOTE: This should only be used in production.  Not suitable for test code....
  public Task BeginWork()
  {
    var res = Task.Factory.StartNew(() =>
    {
      Log.Info("Starting ReplayAppliance update loop.");

      while (!CancelToken.IsCancellationRequested)
      {
        this.Update();
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
      // TODO : We should return null and add the blacklist code in the calling function!
      var sid = msg.u.sync_request.session_id;
      if (!this.IdToSession.TryGetValue(sid, out useSession))
      {
        Log.Warning($"Invalid session ID!  Connection from: {receivedFrom.ToString()} should be blacklisted!");
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
        // Sometimes we can get the last few packets or whatever after a disconnect from a legitimate session that no longer exists.
        // I don't think that blowing up the appliance is a good idea either, so we might have to just
        // put these on some kind of "suspicious" list and trigger a blacklist if there are too many of them?
        Log.Debug($"There is no sessions associated with the address: {receivedFrom.ToString()}");
        return null;
      }
    }

    useSession.DeliverMessage(ref msg, received, receivedFrom);

    return useSession;

  }


  // --------------------------------------------------------------------------------------------------------------------------
  private GGPOEndpoint AddReplayEndpoint(ReplaySession replaySesh, IPEndPoint from, UdpMsg msg)
  {
    var playerIndex = msg.u.sync_request.player_index;

    Log.Info($"Adding replay endpoint for player index: {playerIndex}!");

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
    // Log.Debug("ggpo callback: event");
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

}



