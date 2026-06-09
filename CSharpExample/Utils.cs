using System.Diagnostics;
using System.Runtime.CompilerServices;
using System.Text;

namespace GGPOSharp;

// =======================================================================================
public class GGPOLogOptions
{
  public bool LogToFile { get; set; } = false;
  public string FilePath { get; set; } = string.Empty;

  /// <summary>
  /// Comma delimited list of categories that will be logged.  All others will be ignored.
  /// If empty, all categories will be logged.
  /// </summary>
  public string ActiveCategories { get; set; } = string.Empty;
}

// =======================================================================================
public static class Utils
{
  public const int LOG_VERSION = 1;

  public static bool IsLoggingEnabled = false;

  private static GGPOLogOptions LogOptions = null!;
  private static bool IsLogActive = false;
  private static FileStream? LogStream = null;

  private static Stopwatch Clock = Stopwatch.StartNew();



  // ------------------------------------------------------------------------
  internal unsafe static bool ReadBit(byte* from, int offset)
  {
    int byteIndex = offset >> 3;     // offset / 8
    int bitIndex = offset & 7;      // offset % 8

    byte src = from[byteIndex];
    byte mask = (byte)(1 << bitIndex);

    bool res = (src & mask) != 0;
    return res;

  }

  // ------------------------------------------------------------------------
  internal static void SetBit(byte[] to, bool val, int offset)
  {
    if (!val) { ClearBit(to, offset); }
    else
    {
      int byteIndex = offset >> 3;     // offset / 8
      int bitIndex = offset & 7;      // offset % 8

      byte dest = to[byteIndex];
      dest |= (byte)(1 << bitIndex);
      to[byteIndex] = dest;
    }
  }

  // ------------------------------------------------------------------------
  internal static void ClearBit(byte[] to, int offset)
  {
    int byteIndex = offset >> 3;     // offset / 8
    int bitIndex = offset & 7;      // offset % 8

    byte dest = to[byteIndex];
    dest &= (byte)~(1 << bitIndex);
    to[byteIndex] = dest;
  }

  // ------------------------------------------------------------------------
  internal static bool ReadBit(byte[] from, int offset)
  {
    int byteIndex = offset >> 3;     // offset / 8
    int bitIndex = offset & 7;      // offset % 8

    byte src = from[byteIndex];
    byte mask = (byte)(1 << bitIndex);

    bool res = (src & mask) != 0;
    return res;
  }

  // ------------------------------------------------------------------------
  internal static void InitLogging(GGPOLogOptions options_)
  {
    LogOptions = options_;
    IsLogActive = LogOptions.LogToFile;

    if (IsLogActive && !string.IsNullOrWhiteSpace(LogOptions.FilePath))
    {
      LogStream = File.OpenWrite(LogOptions.FilePath);

      // Write the init message...
      // TODO: Maybe we could add some more information about the current GGPO settings?  delay, etc.?
      WriteString(LogStream, "# GGPO-LOG\n");
      WriteString(LogStream, "# VERSION:%d\n", LOG_VERSION);

      int len = LogOptions.ActiveCategories.Length;
      WriteString(LogStream, "# ACTIVE: %s\n", len == 0 ? "[ALL]" : LogOptions.ActiveCategories);
      WriteString(LogStream, "# START:%d\n", Clock.ElapsedMilliseconds);
    }
  }

  // ------------------------------------------------------------------------
  internal static void WriteString(Stream to, string fmt, params object[] args)
  {
    string toWrite = string.Format(fmt, args);
    var data = Encoding.UTF8.GetBytes(toWrite);
    to.Write(data, 0, data.Length);
  }

  // ------------------------------------------------------------------------
  internal static void CloseLog()
  {
    if (LogStream != null)
    {
      LogStream.Close();
      LogStream = null;
    }
  }

  //#region Logging Overloads

  //// ------------------------------------------------------------------------
  //// A bunch of overaloaded versions of the logging so that we don't create garbage when calling logging functions, but log/categoey is disabled.
  //internal static void LogIt(string category, string fmt)
  //{
  //  if (!IsLogActive || !IsCategoryActive(category)) { return; }
  //  LogIt_Internal(category, fmt);
  //}

  //// ------------------------------------------------------------------------
  //// A bunch of overaloaded versions of the logging so that we don't create garbage when calling logging functions, but log/categoey is disabled.
  //internal static void LogIt(string category, string fmt, int val1)
  //{
  //  if (!IsLogActive || !IsCategoryActive(category)) { return; }
  //  LogIt_Internal(category, fmt, val1);
  //}

  //// ------------------------------------------------------------------------
  //// A bunch of overaloaded versions of the logging so that we don't create garbage when calling logging functions, but log/categoey is disabled.
  //internal static void LogIt(string category, string fmt, uint val1)
  //{
  //  if (!IsLogActive || !IsCategoryActive(category)) { return; }
  //  LogIt_Internal(category, fmt, val1);
  //}

  //// ------------------------------------------------------------------------
  //internal static void LogIt(string category, string fmt, int val1, int val2)
  //{
  //  if (!IsLogActive || !IsCategoryActive(category)) { return; }
  //  LogIt_Internal(category, fmt, val1, val2);
  //}

  //// ------------------------------------------------------------------------
  //internal static void LogIt(string category, string fmt, uint val1, uint val2)
  //{
  //  if (!IsLogActive || !IsCategoryActive(category)) { return; }
  //  LogIt_Internal(category, fmt, val1, val2);
  //}

  //// ------------------------------------------------------------------------
  //internal static void LogIt(string category, string fmt, int val1, int val2, int val3, int val4)
  //{
  //  if (!IsLogActive || !IsCategoryActive(category)) { return; }
  //  LogIt_Internal(category, fmt, val1, val2, val3, val4);
  //}

  //// ------------------------------------------------------------------------
  //internal static void LogIt(string category, string fmt, int val1, int val2, int val3, int val4, int val5)
  //{
  //  if (!IsLogActive || !IsCategoryActive(category)) { return; }
  //  LogIt_Internal(category, fmt, val1, val2, val3, val4, val5);
  //}

  //// ------------------------------------------------------------------------
  //internal static void LogIt(string category, string fmt, int val1, int val2, int val3)
  //{
  //  if (!IsLogActive || !IsCategoryActive(category)) { return; }
  //  LogIt_Internal(category, fmt, val1, val2, val3);
  //}

  //// ------------------------------------------------------------------------
  //internal static void LogIt(string category, string fmt, bool val1, int val2, int val3)
  //{
  //  if (!IsLogActive || !IsCategoryActive(category)) { return; }
  //  LogIt_Internal(category, fmt, val1, val2, val3);
  //}



  //#endregion

  // ------------------------------------------------------------------------
  internal static bool IsCategoryActive(string category)
  {
    bool res = LogOptions.ActiveCategories.Contains(category);
    return res;
  }


  //// ------------------------------------------------------------------------
  //private static void LogIt_Internal(string category, string fmt, params object[] args)
  //{
  //  // if (!IsLogActive || !IsCategoryActive(category)) { return; }

  //  long time = Clock.ElapsedMilliseconds;
  //  string msg = $"{time}|{category}|{string.Format(fmt, args)}\n";
  //  WriteString(LogStream!, msg);
  //}

  //// ------------------------------------------------------------------------
  //internal static void LogEvent(string v, UdpEvent evt)
  //{
  //  if (!IsLogActive || !IsCategoryActive(LogCategories.EVENT)) { return; }

  //  LogIt_Internal(LogCategories.EVENT, "%d", (int)evt.type);
  //}

  //// ----------------------------------------------------------------------------------------------------------------
  //internal static void LogMsg(EMsgDirection dir, ref UdpMsg msg)
  //{
  //  if (!IsLogActive || !IsCategoryActive(LogCategories.MESSAGE)) { return; }

  //  string msgBuf = $"{(int)dir}:{msg.header.type}:{msg.header.sequence_number}";

  //  // Original....
  //  switch (msg.header.type)
  //  {
  //    case EMsgType.SyncRequest:
  //      msgBuf += msg.u.sync_request.random_request;
  //      break;

  //    case EMsgType.SyncReply:
  //      msgBuf += msg.u.sync_reply.random_reply;
  //      break;

  //    case EMsgType.Input:
  //      msgBuf += $"{msg.u.input.start_frame}:{msg.u.input.num_bits}";
  //      break;

  //    case EMsgType.QualityReport:
  //      break;
  //    case EMsgType.QualityReply:
  //      break;
  //    case EMsgType.KeepAlive:
  //      break;
  //    case EMsgType.InputAck:
  //      break;
  //    case EMsgType.Datagram:
  //      break;

  //    default:
  //      ASSERT(false, "Unknown UdpMsg type.");
  //      break;
  //  }

  //  LogIt_Internal(LogCategories.MESSAGE, msgBuf);
  //}


  //// ----------------------------------------------------------------------------------------------------------------
  //internal static void LogNetworkStats(int totalBytesSent, int totalPacketsSent, int ping)
  //{
  //  if (!IsLogActive || !IsCategoryActive(LogCategories.NETWORK)) { return; }

  //  LogIt_Internal(LogCategories.NETWORK, "%d:%d:%d", totalBytesSent, totalPacketsSent, ping);
  //}

  // ----------------------------------------------------------------------------------------
  public static unsafe bool MemMatches(byte* data1, byte* data2, int size)
  {
    // probably not as fast as memcmp, but that is OK for now...
    for (int i = 0; i < size; i++)
    {
      if (data1[i] != data2[i])
      {
        return false;
      }
    }
    return true;
  }

  // -------------------------------------------------------------------------------------
  public static void ASSERT(bool condition, string? msg = null)
  {
    if (!condition)
    {
      string errMsg = "Assert failed!";
      if (msg != null)
      {
        errMsg += (" " + msg);
      }
      throw new InvalidOperationException(errMsg);
    }
  }

  // -------------------------------------------------------------------------------------
  internal static unsafe void ClearMem(byte* output, int size)
  {
    // TODO: There is probably a more efficient way to do this....
    for (int i = 0; i < size; i++)
    {
      output[i] = 0;
    }
  }

  // -------------------------------------------------------------------------------------
  public unsafe static void CopyMem(byte[] dest, int destOffset, byte* src, uint size)
  {
    for (int i = 0; i < size; i++)
    {
      dest[destOffset + i] = src[i];
    }
  }

  // -------------------------------------------------------------------------------------
  public static unsafe void CopyMem(void* dest, void* src, uint size)
  {
    Unsafe.CopyBlock(dest, src, size);
  }

  // -------------------------------------------------------------------------------------
  internal static unsafe int GetInt(byte* data, int v)
  {
    int* pSrc = (int*)data;

    int res = *pSrc;
    return res;
  }
}


// ====================================================================================================================
public enum EMsgDirection
{
  Send = 0,
  Receive = 1
}

// ====================================================================================================================
public static class LogCategories
{
  public const string GENERAL = "NA";
  public const string MESSAGE = "MSG";
  public const string ENDPOINT = "EP";
  public const string EVENT = "EVT";
  public const string SYNC = "SYNC";
  public const string RUNNING = "RUN";
  public const string CONNECTION = "CONN";
  public const string ERROR = "ERR";
  public const string NETWORK = "NET";
  public const string INPUT = "INP";
  public const string TEST = "TEST";
  public const string UDP = "UDP";
  public const string INPUT_QUEUE = "INPQ";
  public const string TIMESYNC = "TIME";

  /// <summary>This happens when we attempt to retrieve an input from 'SynchronizeInputs' but frame data
  /// from the remote is not available.  It isn't a real input, and may get rolled back!
  /// </summary>
  public const string CATEGORY_PREDICTED_INPUT = "PI";

}
