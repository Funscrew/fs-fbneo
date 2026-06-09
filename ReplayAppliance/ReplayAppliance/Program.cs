using CommandLine;
using drewCo.Tools;
using drewCo.Tools.Logging;
using funscrew.Clients;
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace funscrew;

// ========================================================================================================
public class Program
{
  enum EMode
  {
    Invalid = 0,
    Echo,
    Replay
  }

  private static EMode ClientMode = EMode.Invalid;

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

  // NOTE: We are assuming one connected client still...
  private const int RECONNECT_WAIT_TIME = 250;
  private static double ReconnectTime = -1;

  private static Stopwatch Clock = default!;


  // ------------------------------------------------------------------------------------------------------
  static unsafe int Main(string[] args)
  {
    InitLogging();

    Log.Info("Welcome to GGPOSharp");

    int res = Parser.Default.ParseArguments<InputEchoOptions,
                                            ReplayApplianceOptions>(args).MapResult(
                                            (InputEchoOptions ops) => RunEchoClient(ops),
                                            (ReplayApplianceOptions ops) => SetupClientOptions(ops),
                                            errs => 1);

    if (res != 0)
    {
      return res;
    }

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
        if (ReconnectTime != -1 && elapsed >= ReconnectTime)
        {
          Log.Info("complete!");
          return 0;

          // TEMP: I am disabling this for now....
          //Log.Info("Putting the client back into sync state, waiting for remote connection...");
          //ReconnectTime = -1.0d;
          //InitializeClient();

        }
        else
        {
          // This is where the endpoints are polled for data, events are sent out, etc.
          // Because this runs at higher frequency than 'SyncInputs (RunFrame)' we
          // can expect that many events, text, and other data messages to come through
          // outside of the frame boundaries.
          Client.Idle();
        }
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
  private static unsafe int SetupClientOptions(ReplayApplianceOptions ops)
  {
    Log.Info("Setting up replay appliance...");

    CLIOptions = ops;

    ClientOptions = new GGPOClientOptions(0, ops.LocalPort, ops.ProtocolVersion, ops.SessionId)
    {
      Callbacks = new GGPOSessionCallbacks()
      {
        begin_game = OnBeginGame,
        free_buffer = OnFreeBuffer,
        on_event = OnEvent,
        rollback_frame = OnRollback,
        save_game_state = SaveGameState,
        load_game_state = LoadGameState
      }
    };

    ClientMode = EMode.Replay;

    return 0;
  }

  // ------------------------------------------------------------------------------------------------------
  private static unsafe int RunEchoClient(InputEchoOptions ops)
  {
    Log.Info("Setting up echo client....");

    CLIOptions = ops;

    ClientOptions = new GGPOClientOptions((byte)(ops.PlayerNumber - 1), Defaults.LOCAL_PORT, ops.ProtocolVersion, ops.SessionId)
    {
      Callbacks = new GGPOSessionCallbacks()
      {
        begin_game = OnBeginGame,
        free_buffer = OnFreeBuffer,
        on_event = OnEvent,
        rollback_frame = OnRollback,
        save_game_state = SaveGameState,
        load_game_state = LoadGameState
      }
    };

    ClientOptions.SetReplayOption(ops.ReplayAddress, ops.ReplayTimeout);

    ClientMode = EMode.Echo;

    return 0;
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

    switch (ClientMode)
    {

      case EMode.Replay:
        {


          ReplayApplianceOptions cliOps = (CLIOptions as ReplayApplianceOptions)!;

          // TODO: We will have to come up with a robust way to create session ids....
          if (cliOps.SessionId == 0)
          {
            Log.Warning("Session ID is zero.  A new one will be generated, but this probably isn't what you want.");

            long ts = DateTimeOffset.Now.ToUnixTimeMilliseconds();
            cliOps.SessionId = (ulong)ts;
          }

          FileTools.CreateDirectory(cliOps.DataDir);

          var udp = new UdpBlaster(ClientOptions.LocalPort);
          Client = new ReplayAppliance(ClientOptions, cliOps, udp, new ClockTimer());
          Client.Lock();
          // NOTE: Remotes are setup inside of the client.
        }
        break;

      case EMode.Echo:
        {
          var cliOps = CLIOptions as InputEchoOptions;
          var udp = new UdpBlaster(ClientOptions.LocalPort);
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
            Client.AddRemotePlayer(rOps);
          }

          if (ClientOptions.ReplayHost != null)
          {
            Client.AddReplayAppliance(ClientOptions.ReplayHost, ClientOptions.ReplayPort, ClientOptions.ReplayTimeout);
          }

          // No more endpoints can be added!
          Client.Lock();
        }
        break;

      default:
        throw new ArgumentOutOfRangeException($"Unsupported client mode: {ClientMode}");
    }
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
  private static unsafe bool OnEvent(ref GGPOEvent evt)
  {
    if (evt.event_code == EEventCode.GGPO_EVENTCODE_DATAGRAM)
    {
      if (evt.u.datagram.code == (byte)EDatagramCode.DATAGRAM_CODE_CHAT)
      {
        fixed (byte* pData = evt.u.datagram.data)
        {
          string text = AnsiHelpers.PtrToFixedLengthString(pData, evt.u.datagram.dataSize, GGPOConsts.MAX_GGPO_DATA_SIZE);
          Log.Info($"Text is: {text}");
        }
      }

      else if (evt.u.datagram.code == (byte)EDatagramCode.DATAGRAM_CODE_DISCONNECT)
      {
        Log.Info("disconnect notice was received...");
        ReconnectTime = Clock.Elapsed.TotalSeconds + ((float)RECONNECT_WAIT_TIME / 1000.0f);
      }

      else
      {
        int angaweghag = 10;
      }
    }

    return true;
  }


  // ------------------------------------------------------------------------------------------------------
  static void OnRollback(int flags)
  {
    // We run the next frame on rollback, or it all gets fucked!
    RunFrame(Client, TestInput);

    //Log.Info($"A rollback was detected!");
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
