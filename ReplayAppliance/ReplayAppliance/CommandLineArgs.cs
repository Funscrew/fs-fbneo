using CommandLine;
using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace funscrew;


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