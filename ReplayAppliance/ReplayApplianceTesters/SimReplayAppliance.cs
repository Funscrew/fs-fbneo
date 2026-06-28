using funscrew;
using funscrew.Clients;

namespace funscrewTesters
{
  // ==================================================================================================================
  public class SimSessionPrimer : SessionPrimer
  {
    private byte[] _SendBuffer = new byte[0x400];

    // --------------------------------------------------------------------------------------------------------------------------
    public SimSessionPrimer(SessionPrimerOptions ops_, ReplayAppliance replayAppliance_)
      : base(ops_, replayAppliance_)
    { }

    // --------------------------------------------------------------------------------------------------------------------------
    internal void SendHeartbeat()
    {
      var msg = new UdpMsg(EMsgType.Heartbeat);

      int packetSize = msg.PacketSize();
      UdpMsg.ToBytes(msg, _SendBuffer, packetSize);
      this.ReplayAppliance.UDP.Send(_SendBuffer, packetSize, ReplayAppliance.UDP.Endpoint);
    }

    // --------------------------------------------------------------------------------------------------------------------------
    public new ReplaySession BeginSession(UInt64 sessionId, SessionOptions ops)
    {
      ReplaySession res = base.BeginSession(sessionId, ops);
      return res;
    }

  }

  // ==================================================================================================================
  public class SimReplayAppliance : ReplayAppliance
  {
    /// <summary>
    /// The number of sessions that we have started.
    /// </summary>
    public int SessionsStarted { get; set; } = 0;

    /// <summary>
    /// The number of sessions that have been completed.
    /// </summary>
    public int SessionsEnded { get; set; }

    // ----------------------------------------------------------------------------------------------------------------
    public SimReplayAppliance(ReplayOptions ops_, IUdpBlaster udp_, IClockSource clock_)
      : base(ops_, udp_, clock_)
    { }

    public int ActiveSessionCount { get { return this.ActiveSessions.Count; } }


    // --------------------------------------------------------------------------------------------------------------------------
    public override ReplaySession BeginSession(ulong sessionId, SessionOptions sessionOps)
    {
      ++SessionsStarted;
      return base.BeginSession(sessionId, sessionOps);
    }

    // --------------------------------------------------------------------------------------------------------------------------
    protected override void EndSession(ReplaySession sess)
    {
      ++SessionsEnded;
      base.EndSession(sess);
    }

  }
}
