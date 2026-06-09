// using MsgHandler = System.Action<GGPOSharp.UdpMsg, int>;
namespace funscrew
{
  // ================================================================================================================
  public class TestOptions
  {
    /// <summary>
    /// Use this to simulate latency / jitter when sending packets. (in ms)
    /// </summary>
    public int SendLatency { get; set; } = 0;

    /// <summary>
    /// A certain % of packets will be sent based on this option.
    /// </summary>
    public float OOPercent { get; set; } = 0.0f;
  }


}
