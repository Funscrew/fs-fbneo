using funscrew;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Net;
using System.Text;
using System.Threading.Tasks;

namespace funscrewTesters
{

  // ==============================================================================================================================
  public class SimClock : IClockSource
  {
    private int _CurTime = 0;
    public int CurTime { get { return this._CurTime; } }

    // ----------------------------------------------------------------------------------------------------------------
    public void AddTime(int ms)
    {
      _CurTime += ms;
    }
  }

  //// ==============================================================================================================================
  //public class TestUDP : IUdpBlaster
  //{
  //  public TestGGPOClient Client { get; private set; }
  //  public ITimeSource TimeSource { get; private set; }

  //  // ----------------------------------------------------------------------------------------------------------------
  //  public TestUDP(TestGGPOClient client_, ITimeSource timeSource_)
  //  {
  //    Client = client_;
  //    TimeSource = timeSource_;
  //  }

  //  // ----------------------------------------------------------------------------------------------------------------
  //  public void Dispose()
  //  {
  //    // NOOP
  //  }

  //  // ----------------------------------------------------------------------------------------------------------------
  //  public int Receive(byte[] receiveBuffer, ref EndPoint remoteEP)
  //  {
  //    throw new NotImplementedException();
  //  }

  //  // ----------------------------------------------------------------------------------------------------------------
  //  public int Send(byte[] sendBuffer, int packetSize, SocketAddress useRemote)
  //  {
  //    throw new NotImplementedException();
  //  }
  //}

  //// ==============================================================================================================================
  //public class TestGGPOClient : IGGPOClient
  //{
  //  public int CurTime { get { return _TimeSource.CurTime; } }
  //  public uint ClientVersion { get; private set; } = Defaults.PROTOCOL_VERSION;
  //  public IUdpBlaster UDP { get; set; }
  //  private ITimeSource _TimeSource;

  //  // ---------------------------------------------------------------------------------------------------------------------------
  //  public TestGGPOClient(IUdpBlaster udp_, ITimeSource timeSrc_)
  //  {
  //    UDP = udp_; // new UdpBlaster(localPort);
  //    _TimeSource = timeSrc_;
  //  }
  //}


}
