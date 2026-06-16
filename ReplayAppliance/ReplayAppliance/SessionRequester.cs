using System.Net.Sockets;
using System.Text;

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
  public SessionRequestResult RequestSession()
  {

    using (var client = new TcpClient(Options.Host, Options.Port))
    {
      client.ReceiveTimeout = Options.Timeout;
      using (var stream = client.GetStream())
      {
        var toSend = Encoding.UTF8.GetBytes("--BEGIN--TEST!--END--");
        stream.Write(toSend.AsSpan());

        // Let's get the response.....
        var buffer = new byte[0x400];
        string allData = string.Empty;

        const int MAX_LENGTH = 0x400;

        // Read bytes....
        while (true)
        {
            int size = stream.Read(buffer, 0, buffer.Length);
            string nextChunk = Encoding.UTF8.GetString(buffer, 0, size);
            allData += nextChunk;

          // TOOD: We have to interpret the data...
          if (allData.Length > MAX_LENGTH)
          {
            throw new InvalidOperationException("Max data size exceeded!");
          }
          if (allData.EndsWith("--END--"))
          {
            // TODO: We will deserialize the response.
            // This is the end of the response!
            break;
            int x = 10;
          }
            // if (allData.Length > 1000) { return; }
        }

      }
    }

    return new SessionRequestResult() { Code = 1, Message = "Not Complete!" };
  }
}

// ============================================================================================================================
public class SessionRequestResult
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
