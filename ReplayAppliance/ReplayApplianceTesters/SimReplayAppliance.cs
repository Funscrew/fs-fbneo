using funscrew;
using funscrew.Clients;

namespace funscrewTesters
{
  // ==================================================================================================================
  public class SimReplayAppliance : ReplayAppliance
  {
    // ----------------------------------------------------------------------------------------------------------------
    public SimReplayAppliance(ReplayOptions ops_, IUdpBlaster udp_, IClockSource clock_)
      : base(ops_, udp_, clock_)
    { }
  }
}
