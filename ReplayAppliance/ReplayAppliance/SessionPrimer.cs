using drewCo.Tools;
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

  private TcpListener Listener = null!;
  protected ReplayAppliance ReplayAppliance = null!;
  private SessionIDGenerator IDGenerator = new SessionIDGenerator();

  public bool IsWorking { get; set; } = true;
  public void CancelWork() { this.IsWorking = false; }

  // --------------------------------------------------------------------------------------------------------------------------
  public SessionPrimer(ReplayOptions options_, ReplayAppliance replayAppliance_)
  {
    Options = options_;
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

      while (this.IsWorking)
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
          if (!IsWorking)
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
          EndListen();
        }
      }

    });

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
        try
        {
          while (this.IsWorking)
          {
            const int CHECK_DELAY = 1000;

            // This is a pretty crunchy way to do it, but every second we will check to see if the test session is running.
            // If not, then we will start it up.
            // We could also do an event based approach, but this will be OK!
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
              // Check for + copy any output replay file first.
              string replayPath = Path.Combine(Options.ReplayDataDir, $"{GGPOConsts.TEST_SESSION_ID}.replay");

              if (File.Exists(replayPath))
              {
                // NOTE: For convenience, I am using the FC replay extension here.
                Log.Info("Copying existing replay data...");
                string uniquePath = FileTools.GetSequentialFileName(Options.ReplayDataDir, "replay", ".replay");
                File.Copy(replayPath, uniquePath, true);
              }

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

          Log.Info("Test session monitor is complete....");

        }
        catch (Exception ex)
        {
          //Log.Error("Unhandled exception while running the test session monitor!");
          //Log.Error(ex.Message);
          Log.Exception(ex);

         // throw;
        }
      });

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

    if (IsWorking)
    {
      try
      {
        IsWorking = false;
        Listener.Stop();

        // Dirty trick to force network event.
        using (var client = new TcpClient())
        {
          client.Connect("127.0.0.1", Options.ServicePort);
        }
      }
      catch (SocketException sex)
      {
        // This is OK, we kind of expect this one to happen....
        // throw;
        int x = 10;
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
