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

  public ReplayOptions Options { get; private set; }

  private CancellationTokenSource CTSource = default!;
  private CancellationToken CancelToken = default!;
  private TcpListener Listener = null!;
  protected ReplayAppliance ReplayAppliance = null!;
  private SessionIDGenerator IDGenerator = new SessionIDGenerator();

  // --------------------------------------------------------------------------------------------------------------------------
  public SessionPrimer(ReplayOptions options_, ReplayAppliance replayAppliance_)
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
  public Task[] BeginListen()
  {
    Log.Info($"The front door is open on port {Options.ServicePort}");

    Task? testMonitor = CreateTestSessionMonitor();

    var mainTask = Task.Factory.StartNew(() =>
    {
      Listener = new TcpListener(IPAddress.Any, Options.ServicePort);
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

    int len = testMonitor != null ? 2 : 1;
    var res = new Task[len];
    if (len > 1)
    {
      res[0] = testMonitor!;
      res[1] = mainTask;
    }
    else
    {
      res[0] = mainTask;
    }

    return res;
  }

  // --------------------------------------------------------------------------------------------------------------------------
  private Task? CreateTestSessionMonitor()
  {
    Task? testTask = null;
    if (Options.UseTestSession)
    {
      ReplaySession? activeSession = null;
      Log.Info("Setting up test session monitor!");
      testTask = Task.Factory.StartNew(() =>
      {
        while (!this.CancelToken.IsCancellationRequested)
        {
          const int CHECK_DELAY = 1000;

          // This is a pretty crunchy way to do it, but every second we will check to see if the test session is running.
          // If not, then we will start it up.
          Thread.Sleep(CHECK_DELAY);

          if (activeSession != null)
          {
            if (activeSession.IsComplete)
            {
              Log.Info("The test session is marked as complete!  We will restart it!");

              // TODO: Cleanup, replay file copies, etc.
              activeSession = null;
            }
          }

          if (activeSession == null)
          {
            activeSession = BeginSession(GGPOConsts.TEST_SESSION_ID, new SessionOptions()
            {
              GameName = "sfiii3nr1",
              GameVersion = "123-a",
              PlayerNames = new string[] { "Joe", "Archie" },
              MaxPlayerCount = 2,
              TotalInputSize = 10,
              ConnectTimeout = GGPOConsts.DEFAULT_CONNECT_TIMEOUT      // A very long timeout period is OK!
            });
          }
        }
      }, this.CancelToken);


      // throw new NotSupportedException("test session is not yet supported!");
    }

    return testTask;
  }

  // --------------------------------------------------------------------------------------------------------------------------
  protected ReplaySession BeginSession(ulong sessionId, SessionOptions sessOps)
  {
    ReplaySession res = this.ReplayAppliance.BeginSession(sessionId, sessOps);
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
        client.Connect("127.0.0.1", Options.ServicePort);
      }
    }
  }
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
