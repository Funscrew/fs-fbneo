using System.Diagnostics;
using System.Net;
using System.Net.Sockets;

// TODO: Others may find this useful!
namespace funscrew
{
  // ========================================================================================================================
  public interface IUdpBlaster : IDisposable
  {
    /// <returns>The number of bytes that were received.</returns>
    int Receive(byte[] receiveBuffer, ref EndPoint remoteEP);

    /// <returns>The number of bytes that were sent.</returns>
    int Send(byte[] sendBuffer, int packetSize, SocketAddress useRemote);

    /// <summary>
    /// Set of addresses that we will not send or receive data from.  In the case of receiving,
    /// the client will filter those packets and return 0.
    /// </summary>
    HashSet<SocketAddress> Blacklist { get; }
    void AddToBlacklist(SocketAddress address);
    void RemoveFromBlacklist(SocketAddress address);
  }

  // ========================================================================================================================
  /// <summary>
  /// Because UdpClient NEEDS to make bullshit assumptions about how udp works, and how you want to use it!
  /// </summary>
  public sealed class UdpBlaster : IUdpBlaster
  {
    private readonly Socket Socket;
    private bool IsDisposed;

    // OPTIONS:
    const int RECEIVE_BUFFER_SIZE = 8192;

    public HashSet<SocketAddress> Blacklist { get; private set; } = new HashSet<SocketAddress>();

    /// <summary>
    /// Is set, receive operations are blocking.
    /// </summary>
    /// <remarks>Not the same as the socket blocking.</remarks>
    public bool IsBlocking { get; private set; } = false;

    // ------------------------------------------------------------------------------------------------------------
    public UdpBlaster(int localPort, bool isBlocking = false)
        : this(localPort, IPAddress.Any, isBlocking)
    { }

    // ------------------------------------------------------------------------------------------------------------
    public void AddToBlacklist(SocketAddress at)
    {
      Blacklist.Add(at);
    }

    // ------------------------------------------------------------------------------------------------------------
    public void RemoveFromBlacklist(SocketAddress at)
    {
      Blacklist.Remove(at);
    }

    // ------------------------------------------------------------------------------------------------------------
    // Use port zero (0) to use an ephemeral port.
    public UdpBlaster(int localPort, IPAddress localAddress, bool isBlocking = false)
    {
      Socket = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp);
      Socket.Blocking = true;
      IsBlocking = true;

      IPEndPoint bindEndPoint = new IPEndPoint(localAddress, localPort);
      Socket.Bind(bindEndPoint);

      // Suppress WSAECONNRESET (“connection forcibly closed”) on Windows for UDP.
      TryDisableConnReset();

      Socket.ReceiveBufferSize = RECEIVE_BUFFER_SIZE;
    }

    public int LocalPort
    {
      get
      {
        IPEndPoint ep = (IPEndPoint)Socket.LocalEndPoint;
        return ep.Port;
      }
    }

    public IPAddress LocalAddress
    {
      get
      {
        IPEndPoint ep = (IPEndPoint)Socket.LocalEndPoint;
        return ep.Address;
      }
    }


    public void Dispose()
    {
      if (IsDisposed)
      {
        return;
      }

      IsDisposed = true;

      try
      {
        Socket.Shutdown(SocketShutdown.Both);
      }
      catch (SocketException)
      {
      }
      catch (ObjectDisposedException)
      {
      }

      try
      {
        Socket.Close();
      }
      catch
      {
      }
    }

    // ------------------------------------------------------------------------------------------
    public int Send(byte[] buffer, int size, SocketAddress remoteEndPoint)
    {
      if (this.Blacklist.Contains(remoteEndPoint)) { return 0; }

      if (buffer == null)
      {
        throw new ArgumentNullException(nameof(buffer));
      }

      if (remoteEndPoint == null)
      {
        throw new ArgumentNullException(nameof(remoteEndPoint));
      }

      // TODO: If we want ipv6 support, then we should reintroduce this..
      // In reality, we will use the network family that we initialize this with!
      // TODO: This is probably making garbage....
      //  EndPoint ep = remoteEndPoint; // ForceIPv6(remoteEndPoint);
      var span = new ReadOnlySpan<byte>(buffer, 0, size);
      int sent = Socket.SendTo(span, SocketFlags.None, remoteEndPoint);
      //int sent = Socket.SendTo(buffer, 0, size, SocketFlags.None, remoteEndPoint);
      return sent;
    }

    // ------------------------------------------------------------------------------------------
    public int Receive(byte[] buffer, ref EndPoint remote)
    {
      if (buffer == null)
      {
        throw new ArgumentNullException(nameof(buffer));
      }

      // EndPoint any = Remote; //new IPEndPoint(IPAddress.IPv6Any, 0);
      if (IsBlocking || Socket.Available > 0)
      {
        int read = Socket.ReceiveFrom(buffer, 0, buffer.Length, SocketFlags.None, ref remote);

        // TODO: This will create garbage.  Probably not the end of the world tho?
        if (Blacklist.Contains(remote.Serialize()))
        {
          return 0;
        }

        return read;
      }

      // Nothing!
      return 0;
    }

    // ------------------------------------------------------------------------------------------
    public void SetReceiveTimeout(int milliseconds)
    {
      if (milliseconds < 0)
      {
        throw new ArgumentOutOfRangeException(nameof(milliseconds));
      }

      Socket.ReceiveTimeout = milliseconds;
    }

    // ------------------------------------------------------------------------------------------
    public void SetSendTimeout(int milliseconds)
    {
      if (milliseconds < 0)
      {
        throw new ArgumentOutOfRangeException(nameof(milliseconds));
      }

      Socket.SendTimeout = milliseconds;
    }



    // ------------------------------------------------------------------------------------------
    /// <summary>
    /// Windows systems do some stupid horseshit where they bomb the socket if you send data
    /// to an endpoint that isn't currently listening.  This attempts to fix that problem!
    /// </summary>
    private void TryDisableConnReset()
    {
      if (Environment.OSVersion.Platform == PlatformID.Win32NT)
      {
        // Windows-only: SIO_UDP_CONNRESET = _WSAIOW(IOC_VENDOR, 12) => -1744830452
        const int SIO_UDP_CONNRESET = -1744830452;
        byte[] inValue = new byte[] { 0, 0, 0, 0 }; // FALSE to disable errors
        byte[] outValue = new byte[4];
        Socket.IOControl((IOControlCode)SIO_UDP_CONNRESET, inValue, outValue);
      }
    }

    //// ------------------------------------------------------------------------------------------
    //private static EndPoint ForceIPv6(IPEndPoint remote)
    //{
    //  if (remote.AddressFamily == AddressFamily.InterNetwork)
    //  {
    //    IPAddress v6 = remote.Address.MapToIPv6();
    //    return new IPEndPoint(v6, remote.Port);
    //  }

    //  return remote;
    //}
  }

  public static class UdpHelpers
  {
    public static IPEndPoint Endpoint(string host, int port)
    {
      if (host == null)
      {
        throw new ArgumentNullException(nameof(host));
      }

      IPAddress[] addresses = Dns.GetHostAddresses(host);
      if (addresses == null || addresses.Length == 0)
      {
        throw new SocketException((int)SocketError.HostNotFound);
      }

      // Prefer IPv4, but return whatever exists.
      IPAddress chosen = null;
      for (int i = 0; i < addresses.Length; i++)
      {
        if (addresses[i].AddressFamily == AddressFamily.InterNetwork)
        {
          chosen = addresses[i];
          break;
        }
      }

      if (chosen == null)
      {
        chosen = addresses[0];
      }

      return new IPEndPoint(chosen, port);
    }
  }
}
