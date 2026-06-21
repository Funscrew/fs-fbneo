
using System.Diagnostics;
using System.Net;

namespace funscrewTesters
{
  // ==================================================================================================================
  /// <summary>
  /// This is how we are simulating packets across a network.
  /// Using this approach we can setup all kinds of different test scenarios for our UDP
  /// based GGPO client, and others.
  /// </summary>
  /// REFACTOR: This is basically our "test network" so its name should reflect that.
  public class TestMessageQueue
  {
    // NOTE: We shouldn't expect to see a huge number of entries in this as we will
    // tend to receive them in order, and will remove all messages that have been received, etc.
    private List<SimUdpMessage> MsgQueue = new List<SimUdpMessage>();

    // TODO: Let's make unique IPEndpoint instances for each of the addresses that are listed.

    // ----------------------------------------------------------------------------------------------------------------
    /// <summary>
    /// host + port are used to make sure that we only get the data that is intended for this endpoint.
    /// </summary>
    public SimUdpMessage? GetNextMessage(SimUdp udp)
    {
      int minTime = int.MaxValue;
      SimUdpMessage? res = null;

      int curTime = udp.Clock.CurTime;

      // Grab all messages in the queue up to the current time.
      // only include those messages that have the matching port...
      // SimUdpMessage? toRemove = null;
      int index = -1;
      int len = MsgQueue.Count;
      for (int i = 0; i < len; i++)
      {
        SimUdpMessage next = MsgQueue[i];
        if (next.ReceiveTime <= curTime &&
          next.DestPort == udp.Port &&
          next.DestHost == udp.Host && next.ReceiveTime < minTime)
        {
          index = i;
          res = next;
          minTime = res.ReceiveTime;
        }
      }

      if (index != -1)
      {
        MsgQueue.RemoveAt(index);
      }

      return res;
    }

    // ---------------------------------------------------------------------------------------------------------------------------
    internal void AddMessage(SimUdpMessage msg)
    {
      MsgQueue.Add(msg);
    }
  }

}

// ==============================================================================================================================
[DebuggerDisplay("{SrcHost}:{SrcPort} -> {DestHost}:{DestPort}")]
public class SimUdpMessage
{
  public const int MAX_MSG_SIZE = 1024;

  public IPEndPoint From { get; set; }
  public IPEndPoint To { get; set; }

  // This is where the data originally came from.
  [Obsolete("We will replace this with 'From'")]
  public string SrcHost { get; set; }
  [Obsolete("We will replace this with 'From'")]
  public int SrcPort { get; set; }

  // NOTE: Host + port are used for sending the message to the correct place...
  [Obsolete("We will replace this with 'To'")]
  public string DestHost { get; set; }
  [Obsolete("We will replace this with 'To'")]
  public int DestPort { get; set; }

  public byte[] Data { get; set; } = null;
  public int ReceiveTime { get; set; }
}