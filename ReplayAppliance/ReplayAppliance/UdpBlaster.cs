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
    int Send(byte[] sendBuffer, int packetSize, EndPoint useRemote);

    /// <summary>
    /// Set of addresses that we will not send or receive data from.  In the case of receiving,
    /// the client will filter those packets and return 0.
    /// </summary>
    HashSet<UInt64> Blacklist { get; }
    void AddToBlacklist(UInt64 address);
    void RemoveFromBlacklist(UInt64 address);

    IPEndPoint Endpoint { get; }

    // --------------------------------------------------------------------------------------------------------------------------
    /// <summary>
    /// Get a hash for the address + the port, NOT just the address.
    /// </summary>
    public static AddrHash GetAddrHash(IPEndPoint receivedFrom)
    {
      AddrHash res = (uint64_t)(receivedFrom.GetHashCode()) << 16;
      res |= (uint16_t)receivedFrom.Port;   // lol, of course it is an int.  Thanks M$!

      return res;
    }

    // --------------------------------------------------------------------------------------------------------------------------
    public static AddrHash GetAddrHash(EndPoint receivedFrom)
    {
      var ip = receivedFrom as IPEndPoint;
      if (ip == null) { throw new ArgumentException($"{nameof(receivedFrom)} is not an {nameof(IPEndPoint)} instance!"); }

      return GetAddrHash(ip);
    }
  }

  // ========================================================================================================================
  /// <summary>
  /// Because UdpClient NEEDS to make bullshit assumptions about how udp works, and how you want to use it!
  /// </summary>
  public sealed class UdpBlaster : IUdpBlaster
  {
    public const microseconds ONE_SECOND = 1000 * 1000;
    public const int NO_DELAY = 0;

    private readonly Socket _Socket;
    private readonly Socket[] ReadSockets = null!;

    private bool IsDisposed;

    // OPTIONS:
    const int RECEIVE_BUFFER_SIZE = 8192;

    public HashSet<UInt64> Blacklist { get; private set; } = new HashSet<UInt64>();

    public IPEndPoint Endpoint { get; private set; }

    /// <summary>
    /// How long to wait when polling to receive data.
    /// </summary>
    public int PollDelayTime { get; private set; } = 0;

    // ------------------------------------------------------------------------------------------------------------
    public UdpBlaster(int localPort, int pollDelayTime_)
        : this(localPort, IPAddress.Any)
    {
      PollDelayTime = pollDelayTime_;
    }

    // ------------------------------------------------------------------------------------------------------------
    public void AddToBlacklist(UInt64 at)
    {
      Blacklist.Add(at);
    }

    // ------------------------------------------------------------------------------------------------------------
    public void RemoveFromBlacklist(UInt64 at)
    {
      Blacklist.Remove(at);
    }

    // ------------------------------------------------------------------------------------------------------------
    // Use port zero (0) to use an ephemeral port.
    public UdpBlaster(int localPort, IPAddress localAddress)
    {
      _Socket = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp);
      _Socket.Blocking = true;
      ReadSockets = new[] { _Socket };

      IPEndPoint bindEndPoint = new IPEndPoint(localAddress, localPort);
      _Socket.Bind(bindEndPoint);
      Endpoint = bindEndPoint;

      // Suppress WSAECONNRESET (“connection forcibly closed”) on Windows for UDP.
      TryDisableConnReset();

      _Socket.ReceiveBufferSize = RECEIVE_BUFFER_SIZE;
    }

    public int LocalPort
    {
      get
      {
        IPEndPoint ep = (IPEndPoint)_Socket.LocalEndPoint;
        return ep.Port;
      }
    }

    public IPAddress LocalAddress
    {
      get
      {
        IPEndPoint ep = (IPEndPoint)_Socket.LocalEndPoint;
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
        _Socket.Shutdown(SocketShutdown.Both);
      }
      catch (SocketException)
      {
      }
      catch (ObjectDisposedException)
      {
      }

      try
      {
        _Socket.Close();
      }
      catch
      {
      }
    }

    // ------------------------------------------------------------------------------------------
    public int Send(byte[] buffer, int size, EndPoint remoteEndPoint)
    {
      var hash = IUdpBlaster.GetAddrHash((IPEndPoint)remoteEndPoint);
      if (this.Blacklist.Contains(hash)) { return 0; }

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
      var span = new ReadOnlySpan<byte>(buffer, 0, size);
      int sent = _Socket.SendTo(span, SocketFlags.None, remoteEndPoint);
      return sent;
    }

    // ------------------------------------------------------------------------------------------
    public int Receive(byte[] buffer, ref EndPoint remote)
    {
      if (buffer == null)
      {
        throw new ArgumentNullException(nameof(buffer));
      }

      // Man... Socket.select sux
      // NOTE: This is actually where we would use the 'is blocking' type code from before, but we would call it something better like 'MinPollTime'
      // The reason is because in some cases we have an external loop (ont a timer) that is calling these functions to receive data, and sometimes,
      // like for a server application, we have a blind while loop with no exact timer.
      ReadSockets[0] = _Socket;

      Socket.Select(ReadSockets, null, null, PollDelayTime);
      var target = ReadSockets[0];
      if (target != null)
      {
        if (target.Available > 0)
        {
          int read = target.ReceiveFrom(buffer, 0, buffer.Length, SocketFlags.None, ref remote);
          // target.RemoteEndPoint = remote;

          var hash = IUdpBlaster.GetAddrHash((IPEndPoint)remote);
          if (Blacklist.Contains(hash))
          {
            return 0;
          }

          return read;
        }
      }

      return 0;

      // LEGACY:
      //// EndPoint any = Remote; //new IPEndPoint(IPAddress.IPv6Any, 0);
      //if (Socket.Available > 0)
      //{
      //  int read = Socket.ReceiveFrom(buffer, 0, buffer.Length, SocketFlags.None, ref remote);

      //  // TODO: This will create garbage.  Probably not the end of the world tho?
      //  var hash = IUdpBlaster.GetAddrHash((IPEndPoint)remote);
      //  if (Blacklist.Contains(hash))
      //  {
      //    return 0;
      //  }

      //  return read;
      //}

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

      _Socket.ReceiveTimeout = milliseconds;
    }

    // ------------------------------------------------------------------------------------------
    public void SetSendTimeout(int milliseconds)
    {
      if (milliseconds < 0)
      {
        throw new ArgumentOutOfRangeException(nameof(milliseconds));
      }

      _Socket.SendTimeout = milliseconds;
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
        _Socket.IOControl((IOControlCode)SIO_UDP_CONNRESET, inValue, outValue);
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

  // ==============================================================================================================================
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
