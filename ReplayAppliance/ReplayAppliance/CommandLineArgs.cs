using CommandLine;

namespace funscrew;

using milliseconds = System.Int32;


// ==============================================================================================================================
/// <summary>
/// Not really a first class feauture of the application, rather a place to put some example code for its operation.
/// </summary>
[Verb("session-request")]
public class SessionRequestOptions 
{
  [Option("host", Required = true, HelpText = "Host address to make the request on.")]
  public string Host { get; set; } = "localhost";

  [Option("port", Required = true, HelpText = "(TCP) Port to listen on for session start requests.")]
  public int Port { get; set; } = SessionPrimerOptions.DEFAULT_PORT;

  [Option("timeout", Required = false, HelpText = "Request timeout.")]
  public milliseconds Timeout { get; set; } = 5000;


  // NOTE: These could all be CLI args if we care / want to do extra testing or whatever...
  // Game Options.
  public uint16_t MaxPlayerCount { get; set; } = 2;
  public uint16_t TotalInputSize { get; set; } = 10;
  public string GameName { get; set; } = "sfiii3nr1";
  public string GameVersion { get; set; } = "0.5a";

  /// <summary>
  /// Comma seperated list of player names in the same player order.  i.e. index 0 == player index 0
  /// </summary>
  public string PlayerNames { get; set; } = "Echoman,Screwie";

}

// ==============================================================================================================================
[Verb("replay-appliance-ex")]
public class ReplayOptions
{
  // REFACTOR: This should live somewhere else....
  public const int DEFAULT_PORT = 7002;

  [Option("request-port", Required = true, HelpText = "(TCP) Port to listen on for session start requests.")]
  public int RequestPort { get; set; } = SessionPrimerOptions.DEFAULT_PORT;

  [Option("replay-port", Required = true, HelpText = "(UDP) Port where replay data will be sent.")]
  public int ReplayPort { get; set; } = DEFAULT_PORT;

}

// ==============================================================================================================================
public abstract class ClientOptions
{
  [Option("local-port", Required = false, Default = Defaults.LOCAL_PORT, HelpText = "The port that we are listening on.")]
  public int LocalPort { get; set; } = Defaults.LOCAL_PORT;

  [Option("auto-reinit", HelpText = "If set, the client will automatically reinitialize when the client(s) have disconnected.", Required = false)]
  public bool AutoReinitialize { get; set; }

  public uint ProtocolVersion { get; set; } = Defaults.PROTOCOL_VERSION;

  [Option("replay-options", Required = false, HelpText = "Address of the replay applicance in the form of: <host>:<port>")]
  public string? ReplayAddress { get; set; } = null;

  [Option("replay-timeout", Required = false, HelpText = "Time in ms. that attempts to sync will time out.  Gameplay will continue as normal, but no replay data will be sent.")]
  public int ReplayTimeout { get; set; } = Defaults.REPLAY_TIMEOUT;

  /// <summary>
  /// This is how the replay sessions are uniquely identified.
  /// Use 'zero' to auto-assign a session ID!
  /// </summary>
  [Option("session-id", Required = true, HelpText = "Session ID.  This should match between clients and should be unique.")]
  public UInt64 SessionId { get; set; } = 0;
}

// ==============================================================================================================================
/// <summary>
/// This client will listen to two or more connected endpoints and record / merge the input packets
/// that are sent along.
/// </summary>
[Verb("replay-appliance")]
public class ReplayApplianceOptions : ClientOptions
{
  /// <summary>
  /// Time in ms. for how long we will wait for the expected players to connect / sync.
  /// </summary>
  [Option("startup-timeout", Required = false, HelpText = "Time in ms. that we will wait for connections.  Use -1 for unlimited time.")]
  public int StartupTimeout { get; set; } = GGPOConsts.UNLIMITED_TIME;

  [Option("data-dir", Required = false, HelpText = "Directory where replay data files will be stored.")]
  public string DataDir { get; set; } = "replays";

  [Option("game-name", Required = true, HelpText = "The name of the game")]
  public string GameName { get; set; } = null!;

  [Option("game-version", Required = true, HelpText = "Version of the game.  Max 16 chars.")]
  public string GameVersion { get; set; } = null!;

}

// ==============================================================================================================================
[Verb("input-echo")]
public class InputEchoOptions : ClientOptions
{
  [Option("game-name", Required = true, HelpText = "The name of the game that we are playing.")]
  public string GameName { get; set; }

  [Option("player", HelpText = "The player number: 1, 2, etc.")]
  public byte PlayerNumber { get; set; }

  [Option("name", HelpText = "Name of the player", Required = true)]
  public string PlayerName { get; set; }


  [Option("remote", Required = false, HelpText = "comma delimited list of all <host>:<port>-<playerNumber> of the remote players that we expect to connect to.  NOTE: Currently only one remote player is supported!")]
  public string RemotePlayers { get; set; } = $"{Defaults.REMOTE_HOST}:{Defaults.REMOTE_PORT}-{Defaults.PLAYER_TWO}";


  /// <summary>
  /// Should the left / right buttons be reversed?
  /// </summary>
  [Option("invert-controls", HelpText = "If set, the left/right controls will be inverted when echoing the input.")]
  public bool InvertLeftRightControls { get; set; } = true;

  /// <summary>
  /// How many frams should the echo be delayed?
  /// </summary>
  [Option("delay-frames", HelpText = "How many frames should the echo be delayed?")]
  public int DelayFrameCount { get; set; } = 30;

  //[Option("replay-appliance", Required = false, HelpText = "Optional, use this to send gameplay data to a replay applicance, in the form of: <host>:<port>-<sessionId>")]
  //public string? ReplayAppliance { get; set; } = null;

}