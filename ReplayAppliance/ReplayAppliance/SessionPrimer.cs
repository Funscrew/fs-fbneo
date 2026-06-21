using drewCo.Tools.Logging;
using funscrew.Clients;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Text.Json;

namespace funscrew;

// ==============================================================================================================================
/// <summary>
/// SessionPrimer is responsible for generating and reporting session ids, and getting the system setup to receive the connections.
/// </summary>
/// REFACTOR: 'FrontDoor' or similar....
public class SessionPrimer : IDisposable
{
  public const UInt64 TEST_SESSION_ID = 12345;

  public SessionPrimerOptions Options { get; private set; }

  private CancellationTokenSource CTSource = default!;
  private CancellationToken CancelToken = default!;
  private TcpListener Listener = null!;
  protected ReplayAppliance ReplayAppliance = null!;
  private SessionIDGenerator IDGenerator = new SessionIDGenerator();

  // --------------------------------------------------------------------------------------------------------------------------
  public SessionPrimer(SessionPrimerOptions options_, ReplayAppliance replayAppliance_)
  {
    Options = options_;
    CTSource = new CancellationTokenSource();
    CancelToken = CTSource.Token;
    ReplayAppliance = replayAppliance_;
  }

  // --------------------------------------------------------------------------------------------------------------------------
  public void Dispose()
  {
    this.EndListen();
  }

  // --------------------------------------------------------------------------------------------------------------------------
  public Task BeginListen()
  {
    Log.Info($"The front door is open on port {Options.Port}");


    var res = Task.Factory.StartNew(() =>
    {
      Listener = new TcpListener(IPAddress.Any, Options.Port);
      Listener.Start();

      while (!this.CancelToken.IsCancellationRequested)
      {

        try
        {
          var client = Listener.AcceptTcpClient();
          Log.Info("Client connected.");

          using (client)
          using (NetworkStream stream = client.GetStream())
          {
            var id = IDGenerator.GetNextSessionID();
            var sessOps = SessionRequester.ReadMessageFromStream<SessionOptions>(stream);

            // Make the session active!
            this.BeginSession(id, sessOps);

            var response = new SessionRequestResponse()
            {
              Code = SessionRequestResponse.CODE_OK,
              Message = string.Empty,
              SessionId = id
            };
            string responseContent = JsonSerializer.Serialize(response);

            string responseJson = $"--START--{responseContent}--END--";
            byte[] responseBytes = Encoding.UTF8.GetBytes(responseJson);

            stream.Write(responseBytes, 0, responseBytes.Length);
            stream.Flush();

            Log.Info($"Sent: {responseJson}");
          }
        }
        catch (SocketException sex)
        {
          // This is the expected behaviour.
          if (this.CancelToken.IsCancellationRequested)
          {
            Log.Info("Shutdown complete!");
          }
          else
          {
            // OOPS!  Rethrow it!
            throw;
          }
        }
        catch (Exception ex)
        {
          Log.Info($"Error: {ex.Message}");
        }
      }

    }, this.CancelToken);


    return res;
  }

  // --------------------------------------------------------------------------------------------------------------------------
  protected ReplaySession BeginSession(ulong id, SessionOptions sessOps)
  {
    ReplaySession res = this.ReplayAppliance.BeginSession(id, sessOps);
    return res;
  }

  // --------------------------------------------------------------------------------------------------------------------------
  public void EndListen()
  {
    Log.Info("The front door is closing....");

    if (!CancelToken.IsCancellationRequested)
    {
      CTSource.Cancel();
      Listener.Stop();

      // Dirty trick to force network event.
      using (var client = new TcpClient())
      {
        client.Connect("127.0.0.1", Options.Port);
      }
    }
  }
}


// ==============================================================================================================================
public class SessionPrimerOptions
{
  public const int DEFAULT_PORT = 5000;

  /// <summary>
  /// The port to listen on.
  /// </summary>
  public int Port { get; set; } = DEFAULT_PORT;
}


// ==============================================================================================================================
public interface ISessionIDGenerator
{
  UInt64 GetNextSessionID();
}

// ==============================================================================================================================
public class SessionIDGenerator : ISessionIDGenerator
{
  private object IDLock = new object();
  private UInt64 LastSessionID = 0;
  public SessionIDGenerator()
  {
    GetNextSessionID();
  }

  // --------------------------------------------------------------------------------------------------------------------------
  public UInt64 GetNextSessionID()
  {
    lock (IDLock)
    {
      while (true)
      {
        UInt64 res = (UInt64)DateTimeOffset.Now.ToUnixTimeMilliseconds();
        if (res == LastSessionID)
        {
          Thread.Sleep(1);
        }
        else
        {
          return res;
        }
      }
    }
  }

}
