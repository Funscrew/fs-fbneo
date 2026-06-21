using funscrew;
using NUnit.Framework.Internal.Commands;
using System;
using System.Net;
using System.Runtime.Intrinsics.Arm;

namespace funscrewTesters
{

  // ==================================================================================================================
  public class SimUdp : IUdpBlaster
  {
    public funscrew.IClockSource TimeSource { get; private set; }
    public TestMessageQueue MsgQueue { get; private set; }

    public uint AvgPing { get; set; }
    public uint PingJitter { get; set; }
    public bool IsBlocking { get; private set; }
    public IPEndPoint Endpoint { get; private set; }

    // ----------------------------------------------------------------------------------------------------------------
    public SimUdp(string host_, int port_, funscrew.IClockSource timeSource_, TestMessageQueue msgQueue_, uint avgPing_, uint pingJitter_ = 0)
    {
      Host = host_;
      Port = port_;

      var addr = IPAddress.Parse(Host);
      Endpoint = new IPEndPoint(addr, Port);

      TimeSource = timeSource_;
      MsgQueue = msgQueue_;

      AvgPing = avgPing_;
      PingJitter = pingJitter_;
    }

    // NOTE: This doesn't really matter, just a name / IP will do.
    public string Host { get; private set; }

    // NOTE: This does matter as it is how we are going to track the replays...
    public int Port { get; private set; }

    // TODO: We need a real blacklisting tool where it is easier to add / remove stuff.....
    public HashSet<UInt64> Blacklist { get; private set; } = new HashSet<UInt64>();

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

    // ----------------------------------------------------------------------------------------------------------------
    public int Receive(byte[] receiveBuffer, ref EndPoint remoteEP)
    {
      // TODO: Implement blocking logic...
      if (this.IsBlocking) { throw new NotImplementedException(); }

      SimUdpMessage? msg = MsgQueue.GetNextMessage(this);
      if (msg == null)
      {
        return 0;
      }

      // TODO: This is going to create a lot of garbage.....
      // I'm thinking the message queue tracks the host/port for the clients in a better way.....
      //remoteEP = new IPEndPoint(IPAddress.Parse(msg.SrcHost), msg.SrcPort);
      // TODO: This will make extra garbage too.....
      if (Blacklist.Contains(IUdpBlaster.GetAddrHash(remoteEP)))
      {
        return 0;
      }

      // NOTE: There is probably a better way to do this....
      // Utils.CopyMem
      int res = msg.Data.Length;
      Buffer.BlockCopy(msg.Data, 0, receiveBuffer, 0, res);

      return res;
    }

    // ----------------------------------------------------------------------------------------------------------------
    public int Send(byte[] sendBuffer, int packetSize, EndPoint useRemote)
    {
      // NOTE: This is a very roundabout way to get the host + address from 'useRemote'
      // There is very likely a better way to do this...
      //var ep = new IPEndPoint(IPAddress.Any, 0);
      //var x = ep.Create(useRemote);
      //IPEndPoint ipEndPoint = (IPEndPoint)x;
      var ipEndPoint = useRemote as IPEndPoint;
      if (ipEndPoint == null) {  throw new ArgumentException($"{nameof(useRemote)} must be an {nameof(IPEndPoint)} instance!"); }

      string useHost = ipEndPoint.Address.ToString();
      int usePort = ipEndPoint.Port;

      uint usePing = ComputePing();

      var msg = new SimUdpMessage()
      {
        Data = CopyBytes(sendBuffer, packetSize),
        ReceiveTime = (int)(TimeSource.CurTime + usePing),

        From = this.Endpoint,
        SrcHost = this.Host,
        SrcPort = this.Port,

        To = ipEndPoint,
        DestHost = useHost,
        DestPort = usePort
      };
      MsgQueue.AddMessage(msg);

      // I need to have the ping times so I can make this work.....
      // throw new NotImplementedException();
      return packetSize;
    }

    // ----------------------------------------------------------------------------------------------------------------
    // SHARE: This has utility function written all over it...
    public static byte[] CopyBytes(byte[] sendBuffer, int packetSize)
    {
      var res = new byte[packetSize];
      for (int i = 0; i < packetSize; i++)
      {
        res[i] = sendBuffer[i];
      }
      return res;
    }

    // ----------------------------------------------------------------------------------------------------------------
    private uint ComputePing()
    {
      uint res = this.AvgPing;
      if (this.PingJitter > 0)
      {
        throw new NotSupportedException("Ping jitter is not supported at this time!");
        // TODO: LATER:
        // Do a normal distribution with the jitter (variance) so that
        // the ping times aren't always the same.
      }

      return res;
    }

    // ----------------------------------------------------------------------------------------------------------------
    public void Dispose()
    {
      // NOOP
    }


    // ----------------------------------------------------------------------------------------------------------------


  }
}
