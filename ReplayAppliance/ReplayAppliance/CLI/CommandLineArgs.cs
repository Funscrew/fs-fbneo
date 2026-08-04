// GENERATED CODE!  DO NOT EDIT BY HAND!
using Tommy;
using drewCo.CLI;

public class SessionRequestOptions : ICommand
{
  public String Host = "localhost";
  public Int32 Port = 5000;
  public Int32 Timeout = 5000;
  public Int32 MaxPlayerCount = 2;
  public Int32 TotalInputSize = 10;
  public String GameName = "sfiii3nr1";
  public String GameVersion = "0.5a";
  public String PlayerNames = "Echoman, Screwie";

  public SessionRequestOptions() { }

  public static SessionRequestOptions FromToml(TomlTable table)
  {
    var res = new SessionRequestOptions();
    res.Host = table.GetString("Host", "localhost");
    res.Port = table.GetInt("Port", 5000);
    res.Timeout = table.GetInt("Timeout", 5000);
    res.MaxPlayerCount = table.GetInt("MaxPlayerCount", 2);
    res.TotalInputSize = table.GetInt("TotalInputSize", 10);
    res.GameName = table.GetString("GameName", "sfiii3nr1");
    res.GameVersion = table.GetString("GameVersion", "0.5a");
    res.PlayerNames = table.GetString("PlayerNames", "Echoman, Screwie");
    return res;
  }

  public CommandValidationResult Validate()
  {
    var res = new CommandValidationResult();
    return res;
  }

  public CommandDef Configure()
  {
    var res = new CommandDef();
    res.Name = "SessionRequestOptions";
    res.Alias = "session-request";
    res.HelpText = "";

    var hostOption = new CommandOption();
    hostOption.Name = "Host";
    hostOption.HelpText = "Host address to make the request on.";
    hostOption.DataType = typeof(String);
    hostOption.IsRequired = false;
    hostOption.Aliases = new[] { "--host" };
    hostOption.Options = null;
    res.Options.Add(hostOption);

    var portOption = new CommandOption();
    portOption.Name = "Port";
    portOption.HelpText = "(TCP) Port to listen on for session start requests.";
    portOption.DataType = typeof(Int32);
    portOption.IsRequired = false;
    portOption.Aliases = new[] { "--port" };
    portOption.Options = null;
    res.Options.Add(portOption);

    var timeoutOption = new CommandOption();
    timeoutOption.Name = "Timeout";
    timeoutOption.HelpText = "Request timeout in milliseconds";
    timeoutOption.DataType = typeof(Int32);
    timeoutOption.IsRequired = false;
    timeoutOption.Options = null;
    res.Options.Add(timeoutOption);

    var maxPlayerCountOption = new CommandOption();
    maxPlayerCountOption.Name = "MaxPlayerCount";
    maxPlayerCountOption.HelpText = "";
    maxPlayerCountOption.DataType = typeof(Int32);
    maxPlayerCountOption.IsRequired = false;
    maxPlayerCountOption.Options = null;
    res.Options.Add(maxPlayerCountOption);

    var totalInputSizeOption = new CommandOption();
    totalInputSizeOption.Name = "TotalInputSize";
    totalInputSizeOption.HelpText = "";
    totalInputSizeOption.DataType = typeof(Int32);
    totalInputSizeOption.IsRequired = false;
    totalInputSizeOption.Options = null;
    res.Options.Add(totalInputSizeOption);

    var gameNameOption = new CommandOption();
    gameNameOption.Name = "GameName";
    gameNameOption.HelpText = "";
    gameNameOption.DataType = typeof(String);
    gameNameOption.IsRequired = false;
    gameNameOption.Options = null;
    res.Options.Add(gameNameOption);

    var gameVersionOption = new CommandOption();
    gameVersionOption.Name = "GameVersion";
    gameVersionOption.HelpText = "";
    gameVersionOption.DataType = typeof(String);
    gameVersionOption.IsRequired = false;
    gameVersionOption.Options = null;
    res.Options.Add(gameVersionOption);

    var playerNamesOption = new CommandOption();
    playerNamesOption.Name = "PlayerNames";
    playerNamesOption.HelpText = "";
    playerNamesOption.DataType = typeof(String);
    playerNamesOption.IsRequired = false;
    playerNamesOption.Options = null;
    res.Options.Add(playerNamesOption);

    return res;
  }
}

public class ReplayApplianceOptions : ICommand
{
  public Int32 ServicePort = 5000;
  public Int32 ReplayPort = 7002;
  public Boolean UseTestSession = false;
  public String ReplayDataDir = "replay-data";

  public ReplayApplianceOptions() { }

  public static ReplayApplianceOptions FromToml(TomlTable table)
  {
    var res = new ReplayApplianceOptions();
    res.ServicePort = table.GetInt("ServicePort", 5000);
    res.ReplayPort = table.GetInt("ReplayPort", 7002);
    res.UseTestSession = table.GetBool("UseTestSession", false);
    res.ReplayDataDir = table.GetString("ReplayDataDir", "replay-data");
    return res;
  }

  public CommandValidationResult Validate()
  {
    var res = new CommandValidationResult();
    return res;
  }

  public CommandDef Configure()
  {
    var res = new CommandDef();
    res.Name = "ReplayApplianceOptions";
    res.Alias = "replay-appliance";
    res.HelpText = "";

    var servicePortOption = new CommandOption();
    servicePortOption.Name = "ServicePort";
    servicePortOption.HelpText = "(TCP) Port to listen on for session start requests.";
    servicePortOption.DataType = typeof(Int32);
    servicePortOption.IsRequired = false;
    servicePortOption.Aliases = new[] { "--service-port" };
    servicePortOption.Options = null;
    res.Options.Add(servicePortOption);

    var replayPortOption = new CommandOption();
    replayPortOption.Name = "ReplayPort";
    replayPortOption.HelpText = "(UDP) Port where replay data will be sent.";
    replayPortOption.DataType = typeof(Int32);
    replayPortOption.IsRequired = false;
    replayPortOption.Aliases = new[] { "--replay-port" };
    replayPortOption.Options = null;
    res.Options.Add(replayPortOption);

    var useTestSessionOption = new CommandOption();
    useTestSessionOption.Name = "UseTestSession";
    useTestSessionOption.HelpText = "If set, leaves a test session with ID 12345 always running.  Don't do this in production tho!";
    useTestSessionOption.DataType = typeof(Boolean);
    useTestSessionOption.IsRequired = false;
    useTestSessionOption.Aliases = new[] { "--use-test-session" };
    useTestSessionOption.Options = null;
    res.Options.Add(useTestSessionOption);

    var replayDataDirOption = new CommandOption();
    replayDataDirOption.Name = "ReplayDataDir";
    replayDataDirOption.HelpText = "";
    replayDataDirOption.DataType = typeof(String);
    replayDataDirOption.IsRequired = false;
    replayDataDirOption.Options = null;
    res.Options.Add(replayDataDirOption);

    return res;
  }
}

public class InputEchoOptions : ICommand
{
  public String GameName = string.Empty;
  public Int32 PlayerNumber = -1;
  public String PlayerName = string.Empty;
  public String RemotePlayers = "127.0.0.1:7001-2";
  public Boolean InvertLeftRightControls = true;
  public Int32 DelayFrameCount = 30;
  public Int32 LocalPort = 7000;
  public String ReplayAddress = string.Empty;
  public Int32 ReplayTimeout = 5000;
  public Int32 SessionId = 12345;
  public Int32 ProtocolVersion = 4;

  public InputEchoOptions() { }

  public static InputEchoOptions FromToml(TomlTable table)
  {
    var res = new InputEchoOptions();
    res.GameName = table.GetString("GameName");
    res.PlayerNumber = table.GetInt("PlayerNumber");
    res.PlayerName = table.GetString("PlayerName");
    res.RemotePlayers = table.GetString("RemotePlayers");
    res.InvertLeftRightControls = table.GetBool("InvertLeftRightControls", true);
    res.DelayFrameCount = table.GetInt("DelayFrameCount", 30);
    res.LocalPort = table.GetInt("LocalPort", 7000);
    res.ReplayAddress = table.GetString("ReplayAddress", string.Empty);
    res.ReplayTimeout = table.GetInt("ReplayTimeout", 5000);
    res.SessionId = table.GetInt("SessionId", 12345);
    res.ProtocolVersion = table.GetInt("ProtocolVersion", 4);
    return res;
  }

  public CommandValidationResult Validate()
  {
    var res = new CommandValidationResult();
    if (string.IsNullOrWhiteSpace(GameName))
    {
      res.AddError("Option: 'GameName' (--game-name) is required!");
    }
    if (string.IsNullOrWhiteSpace(PlayerName))
    {
      res.AddError("Option: 'PlayerName' (--name) is required!");
    }
    if (string.IsNullOrWhiteSpace(RemotePlayers))
    {
      res.AddError("Option: 'RemotePlayers' (--remote) is required!");
    }
    return res;
  }

  public CommandDef Configure()
  {
    var res = new CommandDef();
    res.Name = "InputEchoOptions";
    res.Alias = "input-echo";
    res.HelpText = "";

    var gameNameOption = new CommandOption();
    gameNameOption.Name = "GameName";
    gameNameOption.HelpText = "The name of the game that we are playing.";
    gameNameOption.DataType = typeof(String);
    gameNameOption.IsRequired = true;
    gameNameOption.Aliases = new[] { "--game-name" };
    gameNameOption.Options = null;
    res.Options.Add(gameNameOption);

    var playerNumberOption = new CommandOption();
    playerNumberOption.Name = "PlayerNumber";
    playerNumberOption.HelpText = "The player number: 1, 2, etc.";
    playerNumberOption.DataType = typeof(Int32);
    playerNumberOption.IsRequired = true;
    playerNumberOption.Aliases = new[] { "--player" };
    playerNumberOption.Options = null;
    res.Options.Add(playerNumberOption);

    var playerNameOption = new CommandOption();
    playerNameOption.Name = "PlayerName";
    playerNameOption.HelpText = "Name of the player";
    playerNameOption.DataType = typeof(String);
    playerNameOption.IsRequired = true;
    playerNameOption.Aliases = new[] { "--name" };
    playerNameOption.Options = null;
    res.Options.Add(playerNameOption);

    var remotePlayersOption = new CommandOption();
    remotePlayersOption.Name = "RemotePlayers";
    remotePlayersOption.HelpText = "Comma delimited list of all <host>:<port>-<playerNumber> of the remote players that we expect to connect to.  NOTE: Currently only one remote player is supported!";
    remotePlayersOption.DataType = typeof(String);
    remotePlayersOption.IsRequired = true;
    remotePlayersOption.Aliases = new[] { "--remote" };
    remotePlayersOption.Options = null;
    res.Options.Add(remotePlayersOption);

    var invertLeftRightControlsOption = new CommandOption();
    invertLeftRightControlsOption.Name = "InvertLeftRightControls";
    invertLeftRightControlsOption.HelpText = "If set, the left/right controls will be inverted when echoing the input.";
    invertLeftRightControlsOption.DataType = typeof(Boolean);
    invertLeftRightControlsOption.IsRequired = false;
    invertLeftRightControlsOption.Aliases = new[] { "--invert-controls" };
    invertLeftRightControlsOption.Options = null;
    res.Options.Add(invertLeftRightControlsOption);

    var delayFrameCountOption = new CommandOption();
    delayFrameCountOption.Name = "DelayFrameCount";
    delayFrameCountOption.HelpText = "How many frames should the echo be delayed?";
    delayFrameCountOption.DataType = typeof(Int32);
    delayFrameCountOption.IsRequired = false;
    delayFrameCountOption.Aliases = new[] { "--delay-frames" };
    delayFrameCountOption.Options = null;
    res.Options.Add(delayFrameCountOption);

    var localPortOption = new CommandOption();
    localPortOption.Name = "LocalPort";
    localPortOption.HelpText = "";
    localPortOption.DataType = typeof(Int32);
    localPortOption.IsRequired = false;
    localPortOption.Aliases = new[] { "--local-port" };
    localPortOption.Options = null;
    res.Options.Add(localPortOption);

    var replayAddressOption = new CommandOption();
    replayAddressOption.Name = "ReplayAddress";
    replayAddressOption.HelpText = "Address of the replay applicance in the form of: <host>:<port>";
    replayAddressOption.DataType = typeof(String);
    replayAddressOption.IsRequired = false;
    replayAddressOption.Aliases = new[] { "--replay-options" };
    replayAddressOption.Options = null;
    res.Options.Add(replayAddressOption);

    var replayTimeoutOption = new CommandOption();
    replayTimeoutOption.Name = "ReplayTimeout";
    replayTimeoutOption.HelpText = "Time in ms. that attempts to sync will time out.  Gameplay will continue as normal, but no replay data will be sent.";
    replayTimeoutOption.DataType = typeof(Int32);
    replayTimeoutOption.IsRequired = false;
    replayTimeoutOption.Aliases = new[] { "--replay-timeout" };
    replayTimeoutOption.Options = null;
    res.Options.Add(replayTimeoutOption);

    var sessionIdOption = new CommandOption();
    sessionIdOption.Name = "SessionId";
    sessionIdOption.HelpText = "Session ID.  This should match between clients and should be unique.";
    sessionIdOption.DataType = typeof(Int32);
    sessionIdOption.IsRequired = false;
    sessionIdOption.Aliases = new[] { "--session-id" };
    sessionIdOption.Options = null;
    res.Options.Add(sessionIdOption);

    var protocolVersionOption = new CommandOption();
    protocolVersionOption.Name = "ProtocolVersion";
    protocolVersionOption.HelpText = "";
    protocolVersionOption.DataType = typeof(Int32);
    protocolVersionOption.IsRequired = false;
    protocolVersionOption.Options = null;
    res.Options.Add(protocolVersionOption);

    return res;
  }
}

