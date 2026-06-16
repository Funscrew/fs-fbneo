using drewCo.Tools.Logging;
using System.Net;
using System.Net.Sockets;
using System.Text;

namespace funscrew;


// ==============================================================================================================================
/// <summary>
/// SessionPrimer is responsible for generating and reporting session ids, and getting the system setup to receive the connections.
/// </summary>
public class SessionPrimer : IDisposable
{
  private object IDLock = new object();
  private UInt64 LastSessionID = 0;

  public SessionPrimerOptions Options { get; private set; }

  private CancellationTokenSource CTSource = default!;
  private CancellationToken CancelToken = default!;
  private TcpListener Listener = null!;

  // --------------------------------------------------------------------------------------------------------------------------
  public SessionPrimer(SessionPrimerOptions options_)
  {
    Options = options_;
    CTSource = new CancellationTokenSource();
    CancelToken = CTSource.Token;

    GetNextSessionID();
  }

  // --------------------------------------------------------------------------------------------------------------------------
  public void Dispose()
  {
    this.EndListen();
  }

  // --------------------------------------------------------------------------------------------------------------------------
  public Task BeginListen()
  {
    Log.Info($"Now listening for session requests on port {Options.Port}");


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
            // Read incoming data (optional)
            byte[] buffer = new byte[1024];
            int bytesRead = stream.Read(buffer, 0, buffer.Length);

            string request = Encoding.UTF8.GetString(buffer, 0, bytesRead);
            Log.Info($"Received: {request}");

            // Send JSON response
            // TODO: We will get the session ID + indicate to the replay handler that connections will be incoming.
            string responseJson = @"--START--{""data"": ""x"" }--END--";
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
  public void EndListen()
  {
    Log.Info("Session primer is shutting down....");

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


// ==============================================================================================================================
public class SessionPrimerOptions
{
  public const int DEFAULT_PORT = 5000;

  /// <summary>
  /// The port to listen on.
  /// </summary>
  public int Port { get; set; } = DEFAULT_PORT;
}
