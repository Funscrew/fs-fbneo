using System;
using System.ComponentModel.DataAnnotations;
using System.Net.Sockets;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace funscrew;

// ============================================================================================================================
public class SessionRequester
{
  public SessionRequestOptions Options { get; private set; }

  // --------------------------------------------------------------------------------------------------------------------------
  public SessionRequester(SessionRequestOptions options_)
  {
    Options = options_;
  }

  // --------------------------------------------------------------------------------------------------------------------------
  public SessionRequestResponse RequestSession()
  {

    using (var client = new TcpClient(AddressFamily.InterNetwork))
    {
      client.Connect(Options.Host, Options.Port);

      client.ReceiveTimeout = Options.Timeout;
      using (var stream = client.GetStream())
      {

        var args = new SessionOptions()
        {
          GameName = Options.GameName,
          GameVersion = Options.GameVersion,
          MaxPlayerCount = Options.MaxPlayerCount,
          TotalInputSize = Options.TotalInputSize,
          PlayerNames = (from x in Options.PlayerNames.Split(",") select x.Trim()).ToArray()
        };
        string argsString = JsonSerializer.Serialize(args);

        var toSend = Encoding.UTF8.GetBytes($"--BEGIN--{argsString}--END--");
        stream.Write(toSend.AsSpan());


        SessionRequestResponse response = ReadMessageFromStream<SessionRequestResponse>(stream);
        return response;
      }
    }

  }

  // --------------------------------------------------------------------------------------------------------------------------
  // TODO: SHARE:
  // This should go into some kind of shared library....
  public static T ReadMessageFromStream<T>(NetworkStream stream, int maxSize = 0x400)
  {
    // Let's get the response.....
    var buffer = new byte[0x400];
    string allData = string.Empty;


    // Read bytes....
    while (true)
    {
      int size = stream.Read(buffer, 0, buffer.Length);
      string nextChunk = Encoding.UTF8.GetString(buffer, 0, size);
      allData += nextChunk;

      // TOOD: We have to interpret the data...
      if (allData.Length > maxSize)
      {
        throw new InvalidOperationException("Max data size exceeded!");
      }
      if (allData.EndsWith("--END--"))
      {
        // TODO: We will deserialize the response.
        // This is the end of the response!
        if (!allData.StartsWith("--START--"))
        {
          throw new InvalidOperationException("Invalid request data (header)!");
        }

        var checkSize = ("--START--".Length) + ("--END--".Length);
        string responseData = allData.Substring("--START--".Length, allData.Length - checkSize);

        var res = JsonSerializer.Deserialize<T>(responseData);
        if (res == null)
        {
          throw new InvalidOperationException($"Could not deserialize response data into the correct type: {typeof(T)}!");
        }

        return res;
      }
    }
  }
}

// ==============================================================================================================================
// TODO: Should this have a 'SessionId' property as well?
public class SessionOptions
{
  public string GameName { get; set; }
  public string GameVersion { get; set; }

  /// <summary>
  /// NOTE: This says 'max player count', but is really the total count of expected players for the session.  When protocol gets updated, we can think about stuff like absolute max + players jumping in and out of a session.
  /// </summary>
  public uint16_t MaxPlayerCount = 0;
  public uint16_t TotalInputSize = 0;
  public string[] PlayerNames { get; set; }

  public int ConnectTimeout { get; set; } = GGPOConsts.DEFAULT_CONNECT_TIMEOUT;
}

// ============================================================================================================================
public class SessionRequestResponse
{
  public const int CODE_OK = 0;

  /// <summary>
  /// Can be an error, or any other message we might want to send....
  /// </summary>
  public string? Message { get; set; } = null;
  public UInt64 SessionId { get; set; } = 0;

  /// <summary>
  /// Code that indicates success (0) or errors (not 0)
  /// </summary>
  public int Code { get; set; } = CODE_OK;
}
