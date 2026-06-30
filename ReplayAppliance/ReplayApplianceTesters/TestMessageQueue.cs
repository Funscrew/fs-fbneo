
using drewCo.Curations;
using funscrew;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.Dynamic;
using System.Net;
using System.Security.Cryptography;

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


    class IPEndpointComparer : IEqualityComparer<IPEndPoint>
    {
      // ----------------------------------------------------------------------------------------------------------------
      public bool Equals(IPEndPoint? x, IPEndPoint? y)
      {
        if (x == null) { return y == null; }
        if (y == null) { return x == null; }

        var xVal = IUdpBlaster.GetAddrHash(x);
        var yVal = IUdpBlaster.GetAddrHash(x);
        return xVal == yVal;
      }

      // ----------------------------------------------------------------------------------------------------------------
      public int GetHashCode([DisallowNull] IPEndPoint obj)
      {
        var res = IUdpBlaster.GetAddrHash(obj);
        return (int)res;
      }
    }

    private static IPEndpointComparer IPEComparer = new IPEndpointComparer();
    private MultiDictionary<IPEndPoint, IPEndPoint, CLinkSettings> LinkSettings = new MultiDictionary<IPEndPoint, IPEndPoint, CLinkSettings>(IPEComparer, IPEComparer);

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
      // Check packet loss stats + drop as needed.
      // if (LinkSettings.TryGetValue(msg.From



      MsgQueue.Add(msg);
    }

    // ---------------------------------------------------------------------------------------------------------------------------
    /// <summary>
    /// Set parameters to simulate packet loss between clients.
    /// </summary>
    /// <param name="isBiDirectional">
    /// If true, loss percentage value for (from -> to) and (to -> from) will be configured.
    /// </param>
    internal void SetPacketLossPct(IPEndPoint from, IPEndPoint to, float lossPct, bool isBiDirectional)
    {
      if (isBiDirectional)
      {
        SetPacketLossPct(from, to, lossPct, false);
        SetPacketLossPct(to, from, lossPct, false);
      }
      else
      {
        var match = LinkSettings.TryGetValue(from, to, out CLinkSettings settings);
        if (!match)
        {
          settings = new CLinkSettings();

          // BUG: This form doesn't work if there isn't already data for the keys.
          LinkSettings.Add(from, to, settings);
        }
        settings.PacketLossPct = lossPct;
      }
    }

    // ---------------------------------------------------------------------------------------------------------------------------
    internal CLinkSettings? GetLinkSettings(IPEndPoint from, IPEndPoint to)
    {
      if (LinkSettings.TryGetValue(from, to, out var res))
      {
        return res;
      }
      return null;
    }
  }

}

// ==================================================================================================================
internal class CLinkSettings
{
  public const uint USE_GLOBAL = uint.MaxValue;

  public IPEndPoint From { get; set; } = default!;
  public IPEndPoint To { get; set; } = default!;

  public double PacketLossPct { get; set; } = 0.0f;

  public uint Ping { get; set; } = USE_GLOBAL;
  public uint Jitter { get; set; } = USE_GLOBAL;
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