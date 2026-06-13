using System;
using System.Diagnostics;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Text;


namespace funscrew;

// ================================================================================================================
// NOTE: These should match what is compiled in the C++ code....
public static class GGPOConsts
{
  public const int MAX_NAME_SIZE = 16;
  public const int MAX_PLAYERS = 4;
  public const int MAX_COMPRESSED_BITS = 4096;
  public const int MAX_GGPO_DATA_SIZE = 128;

  public const int MAX_PREDICTION_FRAMES = 8;
  public const int INPUT_QUEUE_LENGTH = 128;
  public const int DEFAULT_INPUT_SIZE = 4;

  public const int RECOMMENDATION_INTERVAL = 240;

  public const byte PLAYER_NOT_SET = 0xFF;
  public const byte REPLAY_APPLIANCE_PLAYER_INDEX = 0xFF;

  public const int MAX_SEQ_DISTANCE = (1 << 15);

  public const int SYNC_PACKETS_COUNT = 5;

  public const int UNLIMITED_TIME = -1;

}

// ================================================================================================================
public enum EDatagramCode : byte
{
  DATAGRAM_CODE_INVALID = 0
  , DATAGRAM_CODE_MUTED = 1
  , DATAGRAM_CODE_CHAT = 2
  , DATAGRAM_CODE_GGPO_SETTINGS = 3
  , DATAGRAM_CODE_DISCONNECT = 4        // Player is disconnecting.
};


// ================================================================================================================
public enum EEventCode
{
  Invalid = 0,
  GGPO_EVENTCODE_CONNECTED_TO_PEER = 1000,
  GGPO_EVENTCODE_SYNCHRONIZING_WITH_PEER = 1001,
  GGPO_EVENTCODE_SYNCHRONIZED_WITH_PEER = 1002,
  GGPO_EVENTCODE_RUNNING = 1003,
  GGPO_EVENTCODE_DISCONNECTED_FROM_PEER = 1004,
  GGPO_EVENTCODE_TIMESYNC = 1005,
  GGPO_EVENTCODE_CONNECTION_INTERRUPTED = 1006,
  GGPO_EVENTCODE_CONNECTION_RESUMED = 1007,
  GGPO_EVENTCODE_DATAGRAM = 1008,
}


// ================================================================================================================
[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct MessageHeader
{
  public UInt16 magic;        // magic_number
  public UInt16 sequence_number;
  public EMsgType type;                  /* packet type.  Corresponds to: 'EMsgType' enum */
}

// ================================================================================================================
public enum EMsgType : byte
{
  Invalid = 0,
  SyncRequest = 1,
  SyncReply = 2,
  Input = 3,
  QualityReport = 4,
  QualityReply = 5,
  KeepAlive = 6,
  InputAck = 7,
  Datagram = 8
};

// ================================================================================================================
// MINE:
[StructLayout(LayoutKind.Explicit)]
public struct ConnectStatus
{
  [FieldOffset(0)] private int _value;

  public bool disconnected
  {
    get => (_value & 0x1) != 0;
    set
    {
      if (value)
        this._value |= 0x1;
      else
        this._value &= ~0x1;
    }
  }

  public int last_frame
  {
    get => _value >> 1;                 // use remaining 31 bits
    set => this._value = (this._value & 0x1) | (value << 1);
  }
}

// ================================================================================================================
[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct SyncRequest
{
  public uint random_request;   // please reply back with this random data
  public byte player_index;     // Players must identify their desired index.
  public byte isReplayEndpoint; // This is from a replay appliance.   ( we use a byte to make sure that we are C++ compatible)
  public UInt64 session_id;     // Used for replay ids.  This is the form of a unix timestamp in milliseconds!  For p2p connections, this can be zero, but is ignored.

  // ---------------------------------------------------------------------------------
  internal static void FromBytes(byte[] data, int startOffset, ref SyncRequest res)
  {
    int useOffset = startOffset;
    res.random_request = BitConverter.ToUInt32(data, startOffset);
    useOffset += sizeof(uint);

    res.player_index = (byte)BitConverter.ToChar(data, useOffset);
    useOffset += sizeof(byte);

    res.isReplayEndpoint = (byte)BitConverter.ToChar(data, useOffset);
    useOffset += sizeof(byte);

    res.session_id = BitConverter.ToUInt64(data, useOffset);
  }
}

// ================================================================================================================
[StructLayout(LayoutKind.Sequential, Pack = 1, CharSet = CharSet.Ansi)]
public unsafe struct SyncReply
{
  public UInt32 random_reply;     // OK, here's your random data back
  public UInt32 client_version;   // Version of this client, in 8 byte chunks: MAJOR - MINOR - REVISION - GGPO (protocol version)
  public byte player_index;       // Index of the remote player.  Should match what we expect / not be our index!
  public byte isReplayEndpoint;   // This is from a replay appliance.   ( we use a byte to make sure that we are C++ compatible)
  public byte delay;
  public byte runahead;
  //public byte is_ready;           // Readiness flag, used in the context of hooking up to a replay client.

  public fixed byte playerName[GGPOConsts.MAX_NAME_SIZE];
  public fixed byte gameName[GGPOConsts.MAX_NAME_SIZE];

  // --------------------------------------------------------------------------------------------------------------
  public string GetGameName()
  {
    fixed (byte* p = gameName)
    {
      return StringHelpers.ReadUtf8String(p, GGPOConsts.MAX_NAME_SIZE);
    }
  }

  // --------------------------------------------------------------------------------------------------------------
  public void SetGameName(string name)
  {
    // TODO: This needs to be converted to utf8??  think about it.....
    var value = Encoding.Default.GetBytes(name);
    fixed (byte* p = gameName)
    {
      for (int i = 0; i < value.Length; i++)
      {
        gameName[i] = (byte)value[i];
      }
    }
  }

  // --------------------------------------------------------------------------------------------------------------
  public string GetPlayerName()
  {
    fixed (byte* p = playerName)
    {
      return AnsiHelpers.PtrToAnsiString(p, GGPOConsts.MAX_NAME_SIZE);
    }
  }

  // --------------------------------------------------------------------------------------------------------------
  public void SetPlayerName(char[] value)
  {
    fixed (byte* p = playerName)
    {
      for (int i = 0; i < value.Length; i++)
      {
        playerName[i] = (byte)value[i];
      }
    }
  }

  // --------------------------------------------------------------------------------------------------------------
  public void SetPlayerName(string value)
  {
    fixed (byte* p = playerName)
    {
      AnsiHelpers.WriteAnsiString(value, p, GGPOConsts.MAX_NAME_SIZE);
    }
  }
}

// ================================================================================================================
[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct QualityReport
{
  public byte frame_advantage; // what's the other guy's frame advantage?
  public uint ping;
}

// ================================================================================================================
[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct QualityReply
{
  public uint pong;
}

// ================================================================================================================
[StructLayout(LayoutKind.Sequential, Pack = 1)]
public unsafe struct InputMsg
{
  //// Raw storage for peer_connect_status[UDP_MSG_MAX_PLAYERS]
  //// Size = sizeof(ConnectStatus) * UDP_MSG_MAX_PLAYERS.
  //public const int PeerConnectStatubytes = sizeof(int) * 2 * ProtoConsts.UDP_MSG_MAX_PLAYERS; // two ints per ConnectStatus
  //public fixed byte peer_connect_status_bytes[PeerConnectStatubytes];
  // public const int CONNECT_STATUS_SIZE = sizeof(int) * ProtoConsts.UDP_MSG_MAX_PLAYERS;
  // private fixed byte peer_connect_status_bytes[CONNECT_STATUS_SIZE];
  private fixed int peer_connect_data[GGPOConsts.MAX_PLAYERS];

  public UInt32 start_frame;

  // bitfields packed into a single int
  private int _flags;
  public bool disconnect_requested
  {
    get { return (_flags & 0x1) != 0; }
    set { _flags = value ? (_flags | 0x1) : (_flags & ~0x1); }
  }

  public int ack_frame
  {
    get
    {
      // Arithmetic right shift preserves sign
      return _flags >> 1;
    }
    set
    {
      // Preserve LSB (disconnect flag), overwrite upper 31 bits
      _flags = (_flags & 0x1) | (value << 1);
    }
  }

  public ushort num_bits;
  public byte input_size;

  public fixed byte bits[GGPOConsts.MAX_COMPRESSED_BITS];



  // ------------------------------------------------------------------------------------------
  // Helpers to get/set a ConnectStatus by index (unsafe)
  internal ConnectStatus GetPeerConnectStatus(int index)
  {
    if (index >= GGPOConsts.MAX_PLAYERS)
    {
      throw new ArgumentOutOfRangeException(nameof(index));
    }

    //fixed (byte* basePtr = peer_connect_status_bytes)
    //{
    //  int stride = sizeof(int); // * 2;
    //  byte* p = basePtr + (index * stride);
    //  return *(ConnectStatus*)p;
    //}
    fixed (int* basePtr = peer_connect_data)
    {
      int* p = basePtr + index;
      return *(ConnectStatus*)p;
    }
  }

  // ------------------------------------------------------------------------------------------
  internal void SetPeerConnectStatus(int index, in ConnectStatus value)
  {
    if (index >= GGPOConsts.MAX_PLAYERS)
    {
      throw new ArgumentOutOfRangeException(nameof(index));
    }
    //fixed (byte* basePtr = peer_connect_status_bytes)
    //{
    //  int stride = sizeof(int); // * 2;
    //  *(ConnectStatus*)(basePtr + (index * stride)) = value;
    //}
    fixed (int* basePtr = peer_connect_data)
    {
      int* p = basePtr + index;
      *(ConnectStatus*)p = value;
    }
  }
}

// ================================================================================================
[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct InputAck
{
  public Int32 start_frame;      // What is the starting frame of the ACK?
  public UInt16 frame_count;      // How many total frames were ACKed?
}

// ================================================================================================
// NOTE: This is the same as 'Chat', but uses different buffer sizes.  There is probably a way
// to better unify the code.
[StructLayout(LayoutKind.Sequential, Pack = 1)]
public unsafe struct ConnectData
{
  public fixed byte text[GGPOConsts.MAX_NAME_SIZE + 1];

  public int GetTextSize()
  {
    fixed (byte* p = text)
    {
      return AnsiHelpers.PtrToAnsiStringLength(p, GGPOConsts.MAX_NAME_SIZE + 1);
    }
  }
  public string GetText()
  {
    fixed (byte* p = text)
    {
      return AnsiHelpers.PtrToAnsiString(p, GGPOConsts.MAX_NAME_SIZE + 1);
    }
  }

  public void SetText(string value)
  {
    fixed (byte* p = text)
    {
      AnsiHelpers.WriteAnsiString(value, p, GGPOConsts.MAX_NAME_SIZE + 1);
    }
  }
}

// ================================================================================================
// NOTE: This is the same as 'Chat', but uses different buffer sizes.  There is probably a way
// to better unify the code.
[StructLayout(LayoutKind.Sequential, Pack = 1)]
public unsafe struct PlayerConnectData
{
  public fixed byte playerName[GGPOConsts.MAX_NAME_SIZE + 1];
  public byte player_index;
  public byte delay;
  public int runahead;

  public int GetTextSize()
  {
    fixed (byte* p = playerName)
    {
      return AnsiHelpers.PtrToAnsiStringLength(p, GGPOConsts.MAX_NAME_SIZE + 1);
    }
  }
  public string GetPlayerName()
  {
    fixed (byte* p = playerName)
    {
      return AnsiHelpers.PtrToAnsiString(p, GGPOConsts.MAX_NAME_SIZE + 1);
    }
  }

  public void SetPlayerName(string value)
  {
    fixed (byte* p = playerName)
    {
      AnsiHelpers.WriteAnsiString(value, p, GGPOConsts.MAX_NAME_SIZE + 1);
    }
  }
}

// ================================================================================================
[StructLayout(LayoutKind.Sequential, Pack = 1)]
public unsafe struct Datagram
{
  public byte code;
  public byte dataSize;
  public int frame;
  public fixed byte data[GGPOConsts.MAX_GGPO_DATA_SIZE];

  public bool IsText { get { return code == (byte)'T'; } }

  // -------------------------------------------------------------------------------------
  public string GetText()
  {
    Debug.Assert(IsText, "This data is not text!");
    fixed (byte* p = data)
    {
      return AnsiHelpers.PtrToAnsiString(p, GGPOConsts.MAX_GGPO_DATA_SIZE);
    }
  }

  // -------------------------------------------------------------------------------------
  public void SetText(string value)
  {
    fixed (byte* p = data)
    {
      AnsiHelpers.WriteAnsiString(value, p, GGPOConsts.MAX_GGPO_DATA_SIZE + 1);
    }
  }

  // -------------------------------------------------------------------------------------
  public void SetData(byte* data, byte size)
  {
    this.dataSize = size;
    fixed (byte* p = this.data)
    {
      Utils.CopyMem(p, data, (uint)size);
    }
  }

}


// ================================================================================================================
[StructLayout(LayoutKind.Explicit, Pack = 1)]
public struct U
{
  [FieldOffset(0)] public SyncRequest sync_request;
  [FieldOffset(0)] public SyncReply sync_reply;
  [FieldOffset(0)] public QualityReport quality_report;
  [FieldOffset(0)] public QualityReply quality_reply;
  [FieldOffset(0)] public InputMsg input;
  [FieldOffset(0)] public InputAck input_ack;
  [FieldOffset(0)] public Datagram datagram;
}

// ================================================================================================================
[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct UdpMsg
{

  // public ConnectStatus connect_status;
  public MessageHeader header;

  // REFACTOR: Give the union, and the member name actual names!  Refactor in CPP version too!
  public U u;   // The actual data we care about....

  public UdpMsg() { }
  public UdpMsg(EMsgType msgType)
  {
    header.type = msgType;
  }

  // -------------------------------------------------------------------------------------------------------------
  public unsafe int PacketSize()
  {
    // Makes sure that the compiler isn't getting any ideas....
    Debug.Assert(sizeof(MessageHeader) == 5, "Bad message header size!");

    int res = sizeof(MessageHeader) + PayloadSize();
    return res;
  }

  // -------------------------------------------------------------------------------------------------------------
  public unsafe int PayloadSize()
  {
    int res = 0;


    switch (header.type)
    {
      case EMsgType.SyncRequest:
        return sizeof(SyncRequest);
      case EMsgType.SyncReply:
        {
          res = sizeof(SyncReply);
          return res;
        }

      case EMsgType.QualityReport:
        return sizeof(QualityReport);
      case EMsgType.QualityReply:
        return sizeof(QualityReply);
      case EMsgType.InputAck:
        return sizeof(InputAck);
      case EMsgType.KeepAlive:
        return 0;

      case EMsgType.Input:
        // NOTE: The calculation of the size of this data is a bit wonky.
        // It could actually be computed once when the application starts up, I think....
        // C++ way!
        //res = (int)((char*)&u.input.bits - (char*)&u.input);
        //res += (u.input.num_bits + 7) / 8;

        int size2 = sizeof(InputMsg) - (sizeof(byte) * GGPOConsts.MAX_COMPRESSED_BITS);
        size2 += (u.input.num_bits + 7) / 8;
        // Debug.Assert (size == size2);

        // Assuming that this computation is correct!
        res = size2;

        return res;

      case EMsgType.Datagram:
        // Include one extra byte to ensure zero termination.
        res = sizeof(byte) * 2;     // code + dataSize
        res += sizeof(int);         // frame
        res += u.datagram.dataSize; // GetTextSize() + 1; //  GetText()  strnlen_s(u.chat.text, MAX_GGPOCHAT_SIZE) + 1;
        return res;

      default:
        throw new InvalidOperationException($"Unsupported type: {header.type}!");
    }

  }

  // -------------------------------------------------------------------------------------------------------------
  public static unsafe void FromBytes(byte[] data, ref UdpMsg res, int len)
  {
    if (data == null) throw new ArgumentNullException(nameof(data));
    if (len < 0 || len > data.Length) throw new ArgumentOutOfRangeException(nameof(len));
    if (len > sizeof(UdpMsg)) throw new ArgumentException("len exceeds size of UdpMsg");

    // Create a stack buffer the size of UdpMsg and zero it first.
    // This way any missing bytes beyond len are safely zero.
    byte* buffer = stackalloc byte[sizeof(UdpMsg)];
    for (int i = 0; i < sizeof(UdpMsg); i++) buffer[i] = 0;

    // Copy only 'len' bytes from the source
    fixed (byte* pSrc = data)
    {
      Buffer.MemoryCopy(pSrc, buffer, sizeof(UdpMsg), len);
    }

    // Now reinterpret into UdpMsg
    res = *(UdpMsg*)buffer;
  }

  // --------------------------------------------------------------------------------------------------------------
  public static unsafe void ToBytes(in UdpMsg msg, byte[] dest, int len)
  {
    if (dest == null) throw new ArgumentNullException(nameof(dest));
    if (len < 0 || len > dest.Length) throw new ArgumentOutOfRangeException(nameof(len));
    if (len > sizeof(UdpMsg)) throw new ArgumentException("len exceeds size of UdpMsg");

    fixed (byte* pDest = dest)
    fixed (UdpMsg* pSrc = &msg)
    {
      Buffer.MemoryCopy(pSrc, pDest, len, len);
    }
  }

}

// ================================================================================================================================================================
[StructLayout(LayoutKind.Sequential)]
public struct GGPOEvent
{
  public EEventCode event_code;
  public byte player_index;
  public byte isReplayEndpoint;
  public EventUnion u;
}

// ================================================================================================================================================================
[StructLayout(LayoutKind.Explicit)]
public struct EventUnion
{
  [FieldOffset(0)] public Synchronizing synchronizing;
  [FieldOffset(0)] public EventTimeSync timesync;
  [FieldOffset(0)] public ConnectionInterrupted connection_interrupted;
  [FieldOffset(0)] public EventDatagram datagram;
}


// ================================================================================================================================================================
[StructLayout(LayoutKind.Sequential)]
public struct Synchronizing
{
  public int count;
  public int total;
}

// ================================================================================================================================================================
// struct { int frames_ahead; } timesync;
[StructLayout(LayoutKind.Sequential)]
public struct EventTimeSync
{
  public int frames_ahead;
}

// =======================================================================================
// struct { PlayerID player_index; int disconnect_timeout; } connection_interrupted;
[StructLayout(LayoutKind.Sequential)]
public struct ConnectionInterrupted
{
  public int disconnect_timeout;
}

// =======================================================================================
[StructLayout(LayoutKind.Sequential)]
public unsafe struct EventDatagram
{
  public byte player_index;
  public byte code;
  public int frame;
  public byte dataSize;
  public fixed byte data[GGPOConsts.MAX_GGPO_DATA_SIZE];
  public fixed byte text[GGPOConsts.MAX_GGPO_DATA_SIZE];
  public fixed byte username[GGPOConsts.MAX_NAME_SIZE];
}
