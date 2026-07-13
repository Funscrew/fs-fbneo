using CommandLine;
using drewCo.Tools.Logging;
using funscrew.Clients;
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace funscrew;

// ========================================================================================================
public partial class Program
{
  [DllImport("winmm.dll", EntryPoint = "timeBeginPeriod")]
  public static extern void TimeBeginPeriod(int t);

  [DllImport("winmm.dll", EntryPoint = "timeEndPeriod")]
  public static extern void TimeEndPeriod(int t);

  static ClientOptions CLIOptions = default!;
  static GGPOClientOptions ClientOptions = default!;
  static GGPOClient Client = null!;

  // Some test input.  This mimics no buttons being pushed, and one DIP set
  // for 3rd strike.
  // The rest of the array data is reserved for the rest of the player inputs.
  // REFACTOR: Move this elsehere....  Might not even want to use a const so mem can be dynamic for different games.....
  public const int INPUT_SIZE = 5;
  static byte[] TestInput = new byte[INPUT_SIZE * GGPOConsts.MAX_PLAYERS];

  private static Stopwatch Clock = default!;


  // ------------------------------------------------------------------------------------------------------
  static unsafe int Main(string[] args)
  {
    InitLogging();

    Log.Info("Welcome to ReplayAppliance");

    int res = Parser.Default.ParseArguments<SessionRequestOptions, ReplayOptions, InputEchoOptions>(args).MapResult(
                                            (SessionRequestOptions ops) => TestSessionRequest(ops),
                                            (ReplayOptions ops) => RunReplayAppliance(ops),
                                            (InputEchoOptions ops) => RunEchoClient(ops),
                                            errs => 1);


    return res;
  }



  // --------------------------------------------------------------------------------------------------------------------------
  private static int TestSessionRequest(SessionRequestOptions ops)
  {
    var sr = new SessionRequester(ops);

    try
    {
      Log.Info("Requesting a session!");
      var response = sr.RequestSession();

      Log.Info("Got the response!");

      Log.Info($"Code is: {response.Code}");
      Log.Info($"Message is: {(response.Message == null ? "<null>" : response.Message == string.Empty ? "<empty>" : response.Message)}");
      Log.Info($"Session ID is: {response.SessionId}");
    }
    catch (Exception ex)
    {
      Log.Exception(ex);
      return -1;
    }



    return 0;
  }

  // --------------------------------------------------------------------------------------------------------------------------
  private static int RunReplayAppliance(ReplayOptions ops)
  {
    try
    {
      Log.Info("Setting up replay appliance...");

      var udp = new UdpBlaster(ops.ReplayPort, UdpBlaster.ONE_SECOND);
      ReplayAppliance replayAppliance = new ReplayAppliance(ops, udp, new ClockTimer());
      Task raWorkTask = replayAppliance.BeginWork();

      var sp = new SessionPrimer(ops, replayAppliance);

      Console.CancelKeyPress += (s, e) =>
      {
        replayAppliance.EndWork();
        sp.EndListen();
      };

      // Session Primer looks for TCP traffic to begin new sessions.
      Task[] fdTasks = sp.BeginListen();

      // UUUUUUUUUUgly
      var allTasks = fdTasks.Concat(new[] { raWorkTask }).ToArray();

      Task.WaitAll(allTasks);

      Log.Info("Everything is now donw!");

      return 0;
    }
    catch (Exception ex)
    {
      int x = 10;
      throw;
    }

  }


  // ------------------------------------------------------------------------------------------------------
  public static bool RunFrame(GGPOClient c, byte[] testInput)
  {
    if (!c._synchronizing)
    {
      bool syncOK = c.SyncInput(testInput, INPUT_SIZE, GGPOConsts.MAX_PLAYERS);

      // Tell the client that we have moved ahead one frame.
      if (syncOK)
      {
        c.IncrementFrame();
      }
      return syncOK;
    }
    return false;
  }

  // ------------------------------------------------------------------------------------------------------
  private static unsafe int RunEchoClient(InputEchoOptions ops)
  {
    Log.Info("Setting up echo client....");




    CLIOptions = ops;

    ClientOptions = new GGPOClientOptions(ops.GameName, (byte)(ops.PlayerNumber - 1), ops.LocalPort, ops.ProtocolVersion, ops.SessionId)
    {
      Callbacks = new GGPOSessionCallbacks()
      {
        begin_game = OnBeginGame,
        free_buffer = OnFreeBuffer,
        on_event = OnEchoClientEvent,
        rollback_frame = OnRollback,
        save_game_state = SaveGameState,
        load_game_state = LoadGameState
      }
    };

    ClientOptions.SetReplayOption(ops.ReplayAddress, ops.ReplayTimeout);

    // NOTE: This is pretty much how echo client / replay appliance would work.
    InitializeClient();

    // Game loop:
    // No, this isn't meant to be a sophisticated timing scenario, just get us in the ballpark...
    TimeBeginPeriod(1);

    Clock = Stopwatch.StartNew();
    const double FPS = 60.0d;
    double frameTime = 1.0d / FPS;
    double nextFrameTime = 0.0d;


    int frameCount = 0;
    while (true)
    {
      if (Client.IsComplete)
      {
        // Exit the program.
        // Probably finalize logging, etc. and then deal with it.
        throw new InvalidOperationException("Not sure what to do here....");
      }

      double elapsed = Clock.Elapsed.TotalSeconds;
      if (elapsed < nextFrameTime)
      {
        // This is where the endpoints are polled for data, events are sent out, etc.
        // Because this runs at higher frequency than 'SyncInputs (RunFrame)' we
        // can expect that many events, text, and other data messages to come through
        // outside of the frame boundaries.
        Client.Idle();
      }
      else
      {
        // This is so we send the correct data each time.
        // Following the FC example, we write our inputs at the p1 address
        // and it will pass it on to the correct input queue.
        for (int i = 0; i < INPUT_SIZE; i++)
        {
          TestInput[0] = 0;
        }

        // Send + receive inputs across the network.
        // NOTE: The bytes in TestInput will be overwritten during this process!  This is
        // by design!  For emulators, etc. it is convenient to always use the p1 control scheme,
        // even if you are repping p2!
        // NOTE: RunFrame() syncs the inputs, it doesn't do any network stuff until the
        // input sync is complete.  After that it will call DoPoll(0), but is that necessary?
        // --> Seems to me that we should poll immediately before syncing inputs, if anything, but that
        // may take too long... what about putting the netcode on a different thread.
        bool synced = RunFrame(Client, TestInput);

        // This is where we will increment the frame!
        ++frameCount;
        nextFrameTime += frameTime;
      }

    }

  }

  // ------------------------------------------------------------------------------------------------------
  private static unsafe void InitializeClient()
  {
    Log.Info("Initializing the client...");
    if (Client != null)
    {
      Log.Info("Disposing old client....");
      Client.Dispose();
    }

    var cliOps = CLIOptions as InputEchoOptions;
    var udp = new UdpBlaster(ClientOptions.LocalPort, UdpBlaster.NO_DELAY);
    Client = new InputEchoClient(ClientOptions, cliOps, udp, new ClockTimer());

    var local = Client.AddLocalPlayer(cliOps.PlayerName, (byte)(ClientOptions.PlayerIndex), null);

    if (string.IsNullOrWhiteSpace(cliOps.RemotePlayers))
    {
      throw new InvalidOperationException("Missing or invalid argument for 'remote'!");
    }
    var remotes = cliOps.RemotePlayers.Split(",");
    if (remotes.Length > 1)
    {
      throw new InvalidOperationException("Only one remote player is supported at this time!");
    }

    foreach (var item in remotes)
    {
      var rOps = new RemoteEndpointData(item);

      // HACK: We are going to auto-change the remote player index here if it is incorrect!
      // Keep in mind that this really only supports two players total, so it is OK!
      if (rOps.PlayerNumber == cliOps.PlayerNumber)
      {
        rOps.PlayerNumber = (byte)(cliOps.PlayerNumber == 1 ? 2 : 1);
      }


      Client.AddRemotePlayer(rOps);
    }

    if (ClientOptions.ReplayHost != null)
    {
      Client.AddReplayAppliance(ClientOptions.ReplayHost, ClientOptions.ReplayPort);
    }

    // No more endpoints can be added!
    Client.Lock();

  }

  // ------------------------------------------------------------------------------------------------------
  private static void OnBeginGame(string gameName)
  {
    Log.Info("The game has started!  Waiting for sync....");
  }

  // ------------------------------------------------------------------------------------------------------
  private static unsafe bool OnFreeBuffer(byte* arg)
  {
    // NOTE: We don't have to do anything here!
    //Log.Info("An indication to free a buffer happened!");
    return true;
  }

  // ------------------------------------------------------------------------------------------------------
  private static unsafe bool LoadGameState(byte** buffer, int len)
  {
    // NOTE: We don't attempt to load game state....
    // Log.Info("no state to load...");
    return true;
  }

  // This is just here so that we can have a non-zero game state....
  // private static byte FAKE_STATE = 1;
  // ------------------------------------------------------------------------------------------------------
  private static unsafe bool SaveGameState(byte** buffer, int* len, int* checksum, int frame)
  {
    // NOTE: This is a FAKE buffer!
    // We don't have a game state, but the system expects something non-zero!
    // In the future I want there to be options for how the save / load, etc. handlers work....
    *buffer = (byte*)0x1;
    *len = 1;
    *checksum = 0;

    //Log.Info("nothing to save....");
    return true;
    // throw new NotImplementedException();
  }

  // ------------------------------------------------------------------------------------------------------
  private static unsafe bool OnEchoClientEvent(ref GGPOEvent evt)
  {
    return true;
  }


  // ------------------------------------------------------------------------------------------------------
  static void OnRollback(int flags)
  {
    // We run the next frame on rollback, or it all gets fucked!
    RunFrame(Client, TestInput);
  }


  // ------------------------------------------------------------------------------------------------------
  private static void InitLogging()
  {
    Log.AddLogger(new ConsoleLogger());
  }
}

// ==============================================================================================================================
public class RemoteEndpointData
{
  // --------------------------------------------------------------------------------------------------------------------------
  public RemoteEndpointData(string host_, int port_, byte playerNumber_)
  {
    Host = host_;
    Port = port_;
    PlayerNumber = playerNumber_;
  }

  // --------------------------------------------------------------------------------------------------------------------------
  public RemoteEndpointData(string fromCliOption)
  {
    var parts = fromCliOption.Trim().Split("-");

    var hostAndPort = parts[0].Split(":");
    Host = hostAndPort[0];
    Port = int.Parse(hostAndPort[1]);

    if (parts.Length > 1)
    {
      PlayerNumber = byte.Parse(parts[1]);
    }
  }

  public string Host { get; set; }
  public int Port { get; set; }

  public byte PlayerNumber { get; set; } = GGPOConsts.PLAYER_NOT_SET;
}
