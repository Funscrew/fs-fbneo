using drewCo.Tools.Logging;
using System.Net;

namespace funscrew.Clients;

// ==============================================================================================================================
/// <summary>
/// This is the class that will be responsible for receiving and logging input data from
/// two or more players.
/// NOTE: This is the test version that we used while developing the basics....
/// </summary>
public class ReplayAppliance_LEGACY : GGPOClient
{
  private ReplayApplianceOptions ReplayOptions = default!;

  // The two clients that we expect to receive data from.  These will be the remote endpoints that we
  // then set up.
  private HashSet<SocketAddress> ConnectedClients = new HashSet<SocketAddress>();
  private bool AllConnected = false;
  private List<int> ConnectedPlayerIndexes = new List<int>();

  public List<string> Errors { get; private set; } = new List<string>();

  public GameRecorder Recorder { get; private set; }

  // --------------------------------------------------------------------------------------------------------------------------
  public ReplayAppliance_LEGACY(GGPOClientOptions ggpoOps_, ReplayApplianceOptions ops_, IUdpBlaster udp_, IClockSource clock_)
    : base(ggpoOps_, udp_, clock_)
  {
    ReplayOptions = ops_;
    ValidateOptions();

    // Validate options:
    if (ReplayOptions.SessionId == 0) { throw new InvalidOperationException("Invalid session id!"); }

    InitGameRecorder();
  }

  // --------------------------------------------------------------------------------------------------------------------------
  private void ValidateOptions()
  {
    if (string.IsNullOrWhiteSpace(ReplayOptions.GameName))
    {
      throw new InvalidOperationException("Invalid game name!");
    }
  }

  // --------------------------------------------------------------------------------------------------------------------------
  private void InitGameRecorder()
  {
    if (string.IsNullOrWhiteSpace(ReplayOptions.GameVersion))
    {
      throw new InvalidOperationException("Invalid game version!");
    }

    var gameData = new CGameData()
    {
      GameName = ReplayOptions.GameName,
      GameVersion = ReplayOptions.GameVersion,
      MaxPlayerCount = (UInt16)ClientOptions.MaxPlayerCount,
      TotalInputSize = (UInt16)(ClientOptions.InputSize * ClientOptions.MaxPlayerCount)
    };

    Recorder = new GameRecorder(gameData,
    ReplayOptions.DataDir,
    ClientOptions.SessionId
    );
  }

  // --------------------------------------------------------------------------------------------------------------------------
  protected override void HandleDisconnect(GGPOEndpoint endpoint)
  {
    base.HandleDisconnect(endpoint);
    if (endpoint.IsDisconnected)
    {
      Log.Info("A player disconnected.... wrapping up....");
    }
  }

  // --------------------------------------------------------------------------------------------------------------------------
  public override void DisconnectAll()
  {
    base.DisconnectAll();

    this.AllConnected = false;
    this.ConnectedPlayerIndexes.Clear();
  }


  // --------------------------------------------------------------------------------------------------------------------------
  public GGPOEndpoint GetEndpoint(int index)
  {
    return _endpoints[index];
  }

  // --------------------------------------------------------------------------------------------------------------------------
  protected override void DeliverMessage(ref UdpMsg msg, int received, EndPoint receivedFrom)
  {
    // NOTE: This is going to make garbage.... lame!
    SocketAddress ipa = receivedFrom.Serialize();
    if (msg.header.type == EMsgType.SyncRequest && !this.ConnectedClients.Contains(ipa))
    {
      int index = ConnectedClients.Count;
      var ep = ConnectNewClient(ref msg, ipa);
      if (ep != null)
      {
        _endpoints.Add(ep);
      }
    }

    // Now that the end
    base.DeliverMessage(ref msg, received, receivedFrom);

  }

  // --------------------------------------------------------------------------------------------------------------------------
  private GGPOEndpoint ConnectNewClient(ref UdpMsg msg, SocketAddress ipa)
  {
    // JFC can we make this any more of a pain in the ass?
    // TODO: This will probably go away when we fix how we represent this stuff....
    // Also, this won't work with IPV6, booooo
    var bufferData = ipa.Buffer.ToArray();
    byte[] port = new byte[2];
    port[0] = bufferData[3];
    port[1] = bufferData[2];
    var remotePort = BitConverter.ToUInt16(port);
    string remoteHost = $"{bufferData[4]}.{bufferData[5]}.{bufferData[6]}.{bufferData[7]}";

    // Make sure that session id + player index are correct....
    var sid = msg.u.sync_request.session_id;
    if (sid != ReplayOptions.SessionId)
    {
      // We don't want to receive from this endpoint anymore.....
      // How can we block receiving?
      AddError($"Connection attempt with invalid session id! ({ReplayOptions.SessionId}-{msg.u.sync_request.session_id}) [adding to blacklist]");
      UDP.AddToBlacklist(ipa);
      return null;
    }

    // We also want to check to see if we are getting the correct player index.
    // NOTE: If a certain player index is already connected, then we want to
    // reject those other connections that are reporting the wrong one!
    var pi = msg.u.sync_request.player_index;
    if (ConnectedPlayerIndexes.Contains(pi))
    {
      AddError($"The player with index: {pi} has already been connected! [adding to blacklist]");
      UDP.AddToBlacklist(ipa);
      return null;
    }

    // NOTE: We should have a sync request with the correct request ID set!
    // Don't know what to do if we don't... probably just ignore it...
    var newEndpoint = AddReplayEndpoint(remoteHost, remotePort, msg);

    Log.Info("A remote endpoint was added...");

    this.ConnectedClients.Add(ipa);
    if (this.ConnectedClients.Count == 2)
    {
      AllConnected = true;
      Log.Info("All clients are setup...");
    }

    return newEndpoint;

    // Send the sync reply, immediately.
    // newEndpoint.OnSyncRequest(ref msg, received);

  }

  // --------------------------------------------------------------------------------------------------------------------------
  protected override int PollPlayers(int current_frame)
  {
    // Replay appliance doesn't really do anything at this point, tho maybe this is where
    // we do stuff like confim inputs or whatever.....?
    // return base.PollPlayers(current_frame);
    return current_frame;
  }

  // --------------------------------------------------------------------------------------------------------------------------
  private void AddError(string msg)
  {
    Log.Error(msg);
    this.Errors.Add(msg);
  }

  // --------------------------------------------------------------------------------------------------------------------------
  protected override void CheckInitialSync()
  {
    if (_synchronizing)
    {
      int epLen = _endpoints.Count;
      if (epLen < 2) { return; }

      for (int i = 0; i < epLen; i++)
      {
        var ep = _endpoints[i];
        if (!ep.IsSynchronized() && !_local_connect_status[ep.PlayerIndex].disconnected)
        {
          return;
        }
      }

      GGPOEvent info = new GGPOEvent();
      info.event_code = EEventCode.GGPO_EVENTCODE_RUNNING;
      _callbacks.on_event(ref info);
      _synchronizing = false;
    }
  }

  // --------------------------------------------------------------------------------------------------------------------------
  private GGPOEndpoint AddReplayEndpoint(string remoteHost, int remotePort, UdpMsg msg)
  {
    if (remoteHost == "0.0.0.0") { throw new InvalidOperationException("Invalid host!"); }
    if (remotePort == 0) { throw new InvalidOperationException("Invalid port!"); }

    var playerIndex = msg.u.sync_request.player_index;
    var ops = new GGPOEndpointOptions()
    {
      Delay = 0,
      IsLocal = false,
      PlayerIndex = playerIndex, // GGPOConsts.REPLAY_APPLIANCE_PLAYER_INDEX,
      PlayerName = "REPLAY_APP",
      RemoteHost = remoteHost,
      RemotePort = remotePort,
      Runahead = 0,
      IsReplayClient = true,
      TestOptions = new TestOptions()
    };

    // NOTE: We may not want to send out the sync request immediately on these endpoints?
    // Nah -> it should be OK that they bounce around.....
    var remote = new ReplayEndpoint_LEGACY(this, ops, _local_connect_status);

    ConnectedPlayerIndexes.Add(playerIndex);

    return remote;
  }

  // --------------------------------------------------------------------------------------------------------------------------
  public override bool SyncInput(in byte[] values, int isize, int maxPlayers)
  {
    // TODO: Maybe this is where we merge + ACK inputs?
    return true;
  }

  // --------------------------------------------------------------------------------------------------------------------------
  protected override bool AddLocalInput(byte[] values, int isize)
  {
    // Do nothing, we don't have local inputs!
    return true;
  }


  // --------------------------------------------------------------------------------------------------------------------------
  /// <summary>
  /// This is where the inputs for the different frames will get merged, recorded, and later sent out.
  /// </summary>
  // private bool _WarningSent = false;
  internal void MergeInput(ref GameInput input, int playerIndex)
  {
    if (Recorder.HasError)
    {
      int x = 10;
      // We have detected an error in the recorder.  We will log this, and send disconnect
      // notices to all active clients.
      Log.Error($"There was a recording error: {Recorder.ErrorReason} : {Recorder.ErrorMessage}");
      DisconnectAll();
      return;
    }
    Recorder.AddInput(playerIndex, ref input);
  }

}




// ==============================================================================================================================
/// <summary>
/// This is very much like GGPOClient, but it is only for sending replay data to an appliance.
/// </summary>
public class ReplayEndpoint_LEGACY : GGPOEndpoint
{
  public static int MAX_ACKS = 0x100;

  private ReplayAppliance Appliance = null;

  /// <summary>
  /// The acks that we still need to send out.
  /// </summary>
  private RingBuffer<GameInput> _PendingAcks = null!;

  // --------------------------------------------------------------------------------------------------------------------------  
  public ReplayEndpoint_LEGACY(IGGPOClient client_, GGPOEndpointOptions ops_, ConnectStatus[] localConnectStatus_)
    : base(client_, ops_, localConnectStatus_)
  {
    this.Appliance = this.Client as ReplayAppliance;
    _PendingAcks = new RingBuffer<GameInput>(MAX_ACKS);
  }

  // --------------------------------------------------------------------------------------------------------------------------  
  public override void OnLoopPoll()
  {
    base.OnLoopPoll();
    SendPendingAcks();
  }

  // --------------------------------------------------------------------------------------------------------------------------  
  protected override bool OnInput(ref UdpMsg msg, int msgLen)
  {
    // The replay client needs to do that same thing as the normal endpoint by keeping
    // a set of ACKS to send back to the client. 
    // In this case our parent is going to be a replay appliance so we may need to keep a separate list
    // of stuff that needs to be acked / resolved on our end.....
    bool res = base.OnInput(ref msg, msgLen);

    // Housekeeping.  We can get rid of all confirmed acks.
    // TODO: I'd like to log the size of these ring buffers to see what is typical.  Is there really a certain amount of 'overdraw' in the system always?
    while (_PendingAcks.Size > 0 && _PendingAcks.Front().frame < msg.u.input.ack_frame)
    {
      Utils.LogIt(LogCategories.INPUT, "ACK: Throwing away pending ACK frame %d", _PendingAcks.Front().frame);
      _last_acked_input = _PendingAcks.Front();
      _PendingAcks.Pop();

      if (this.Appliance != null)
      {
        this.Appliance.MergeInput(ref _last_acked_input, this.PlayerIndex);
      }
    }

    return res;
  }

  // --------------------------------------------------------------------------------------------------------------------------  
  protected override void SendInputEvent(ref GameInput input)
  {
    base.SendInputEvent(ref input);

    // Send the ACK for this input!
    SendInputAck(ref input);
  }

  // --------------------------------------------------------------------------------------------------------------------------  
  private void SendInputAck(ref GameInput input)
  {
    if (_PendingAcks.IsFull)
    {
      Log.Error($"ACK BUFFER full for: player: {this.PlayerIndex}.  Disconnecting!");
      Disconnect(0);

      // We aren't going to fail, we are simply going to disconnect the client!

      throw new InvalidOperationException($"{nameof(_PendingAcks)} buffer is full!  System will fail!");
    }
    _PendingAcks.Push(input);

    SendPendingAcks();
  }

  // --------------------------------------------------------------------------------------------------------------------------  
  /// <summary>
  /// This is like 'SendPendingOutput' but it is for input ACK messages.
  /// </summary>
  private void SendPendingAcks()
  {
    // GameInput last;
    // NEW:
    // We will collect all of the pending acks and send a message for each:
    // In the future we can combine them all into a single message to ACK mulitple inputs.
    if (_PendingAcks.Size > 0)
    {
      var last = _last_acked_input;
      var front = _PendingAcks.Front();
      Utils.ASSERT(last.frame == -1 || last.frame + 1 == front.frame);

      var msg = new UdpMsg(EMsgType.InputAck);
      msg.u.input_ack.start_frame = _PendingAcks[0].frame;

      UInt16 useCount = 1;
      int expectedFrame = _PendingAcks[0].frame;
      for (int i = 1; i < _PendingAcks.Size; i++)
      {
        // NOTE: These checks are really more exploratory than anything....
        // I am pretty sure that neither case will be encountered as part of the OOP handling...
        if (expectedFrame == _PendingAcks[i].frame)
        {
          // A duplicate frame...
          Log.Debug("duplicate frame in ACK encountered!");
        }

        ++expectedFrame;
        if (expectedFrame != _PendingAcks[i].frame)
        {
          Log.Debug("incorrect next frame....");
          break;
        }

        ++useCount;
      }

      // Log.Info($"sending input ack: {msg.u.input_ack.start_frame} - {useCount}");

      msg.u.input_ack.frame_count = useCount;
      SendMsg(ref msg);
    }

  }

}
