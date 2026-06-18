using funscrew;
using funscrew.Clients;

namespace funscrewTesters
{
  // ==================================================================================================================
  public class SimReplayAppliance : ReplayAppliance
  {
    // ----------------------------------------------------------------------------------------------------------------
    public SimReplayAppliance(GGPOClientOptions ggpoOps_, ReplayApplianceOptions ops_, IUdpBlaster udp_, funscrew.IClockSource clock_)
      : base(ggpoOps_, ops_, udp_, clock_)
    { }
  }
}
