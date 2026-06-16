using drewCo.Tools.Logging;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;

namespace funscrew;


// ==============================================================================================================================
public class SessionPrimerOptions
{
  /// <summary>
  /// The port to listen on.
  /// </summary>
  public int Port { get; set; }
}

// ==============================================================================================================================
/// <summary>
/// SessionPrimer is responsible for generating and reporting session ids, and getting the system setup to receive the connections.
/// </summary>
public class SessionPrimer
{
  private object IDLock = new object();
  private UInt64 LastSessionID = 0;


  public SessionPrimerOptions Options { get; private set; }

  private CancellationTokenSource CTSource=  default!;// new CancellationTokenSource();
  private CancellationToken CancelToken = default!; //CTS

  // --------------------------------------------------------------------------------------------------------------------------
  public SessionPrimer(SessionPrimerOptions options_)
  {
    Options = options_;
  }

  // --------------------------------------------------------------------------------------------------------------------------
  public void BeginListen()
  {
    Log.Info("Now listening for session requests...");


    Task.Factory.StartNew(() => {

    TcpListener listener = new TcpListener(IPAddress.Any, Options.Port);
    listener.Start();

    while (true)
    {
      TcpClient client = listener.AcceptTcpClient();
      Console.WriteLine("Client connected.");

      try
      {
        using (client)
        using (NetworkStream stream = client.GetStream())
        {
          // Read incoming data (optional)
          byte[] buffer = new byte[1024];
          int bytesRead = stream.Read(buffer, 0, buffer.Length);

          string request = Encoding.UTF8.GetString(buffer, 0, bytesRead);
          Console.WriteLine($"Received: {request}");

          // Send JSON response
          string responseJson = @"{ ""data"": ""x"" }";
          byte[] responseBytes = Encoding.UTF8.GetBytes(responseJson);

          stream.Write(responseBytes, 0, responseBytes.Length);
          stream.Flush();

          Console.WriteLine($"Sent: {responseJson}");
        }
      }
      catch (Exception ex)
      {
        Console.WriteLine($"Error: {ex.Message}");
      }
    }

    }, this.CancelToken);

  }


  // --------------------------------------------------------------------------------------------------------------------------
    public void EndListen() { 
      CTSource.Cancel();
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
