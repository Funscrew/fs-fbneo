using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;

using int32_t = System.Int32;
using uint32_t = System.UInt32;
using uint16_t = System.UInt16;
using uint8_t = System.Byte;


namespace funscrew;

// ==============================================================================================================================
[StructLayout(LayoutKind.Sequential, Pack = 1)]
public unsafe struct CGameData
{
  public const int GAMEINPUT_MAX_PLAYERS = 4;
  public const int MAX_GAME_NAME_SIZE = 32;
  public const int MAX_VERSION_SIZE = 16;
  public const int MAX_PLAYER_NAME = 32;

  private fixed byte _GameNameData[MAX_GAME_NAME_SIZE];
  private fixed byte _GameVersionData[MAX_VERSION_SIZE];

  public uint16_t MaxPlayerCount = 0;
  public uint16_t TotalInputSize = 0;

  private fixed byte _PlayerNameData[MAX_PLAYER_NAME * GAMEINPUT_MAX_PLAYERS];

  // --------------------------------------------------------------------------------------------------------------------------
  public CGameData() { }

  public string GameName
  {
    get
    {
      fixed (byte* p = _GameNameData)
      {
        string res = StringHelpers.ReadUtf8String(p, MAX_GAME_NAME_SIZE);
        return res;
      }
    }
    set
    {
      fixed (byte* p = _GameNameData)
      {
        StringHelpers.WriteUtf8String(value, p, MAX_GAME_NAME_SIZE);
      }
    }
  }

  public string GameVersion
  {
    get
    {
      fixed (byte* p = _GameVersionData)
      {
        string res = StringHelpers.ReadUtf8String(p, MAX_VERSION_SIZE);
        return res;
      }
    }
    set
    {
      fixed (byte* p = _GameVersionData)
      {
        StringHelpers.WriteUtf8String(value, p, MAX_VERSION_SIZE);
      }
    }
  }

  // --------------------------------------------------------------------------------------------------------------------------
  public void SetPlayerName(uint8_t index, string name)
  {
    if (index >= MaxPlayerCount)
    {
      throw new InvalidOperationException("invalid player index!");
    }

    fixed (byte* p = _PlayerNameData)
    {
      int offset = index * MAX_PLAYER_NAME;
      StringHelpers.WriteUtf8String(name, p + offset, MAX_PLAYER_NAME);
    }
  }

  // --------------------------------------------------------------------------------------------------------------------------
  public string GetPlayerName(uint8_t index, string name)
  {
    if (index >= MaxPlayerCount)
    {
      throw new InvalidOperationException("invalid player index!");
    }
    fixed (byte* p = _PlayerNameData)
    {
      int offset = index * MAX_PLAYER_NAME;
      string res = StringHelpers.ReadUtf8String(p + offset, MAX_PLAYER_NAME);
      return res;
    }
  }


};

// ==============================================================================================================================
public unsafe struct CChatData
{
  public const int CHAT_DATA_MAX = 128;

  // NOTE: There is an error if all indexes are the same number!
  public uint8_t FromPlayerIndex = 0;
  public uint8_t ToPlayerIndex = 0;
  public int32_t Frame = 0;

  public uint8_t DataSize = 0;
  fixed byte _MessageData[CHAT_DATA_MAX];

  // --------------------------------------------------------------------------------------------------------------------------
  public CChatData() { }

  public string Message
  {
    get
    {
      fixed (byte* p = _MessageData)
      {
        string res = StringHelpers.ReadUtf8String(p, CHAT_DATA_MAX);
        return res;
      }
    }
    set
    {
      fixed (byte* p = _MessageData)
      {
        int size = StringHelpers.WriteUtf8String(value, p, CHAT_DATA_MAX) - 1;
        if (size < 0 || size > CHAT_DATA_MAX || size > uint8_t.MaxValue)
        {
          throw new Exception("Invalid message size!");
        }
        DataSize = (uint8_t)size;
      }
    }
  }

};

// ==============================================================================================================================
public unsafe struct CGameState
{
  // NOT CURRENTLY IMPLEMENTED / SUPPORTED!
}

// ==============================================================================================================================
public unsafe static class StringHelpers
{

  // --------------------------------------------------------------------------------------------------------------------------
  public static string ReadUtf8String(byte* p, int maxLen)
  {
    int len = 0;
    while (len < maxLen && p[len] != 0)
    {
      len++;
    }
    string res = Encoding.UTF8.GetString((byte*)p, len);
    return res;
  }


  // --------------------------------------------------------------------------------------------------------------------------
  public unsafe static int WriteUtf8String(string value, byte* dest, int maxLength, bool fillAll = true)
  {
    var bytes = Encoding.UTF8.GetBytes(value ?? string.Empty);
    var size = bytes.Length + 1;    // One extra for zero termination!
    if (size > maxLength)
    {
      throw new InvalidOperationException($"value is: {size} bytes, but only: {maxLength} are allowed!");
    }

    int n = size - 1; // Math.Min(bytes.Length, Math.Max(0, size - 1)); // leave room for NULL
    for (int i = 0; i < n; i++)
    {
      dest[i] = (byte)bytes[i];
    }

    if (fillAll)
    {
      // Fill rest of the string with empty data...
      for (int i = n; i < maxLength; i++)
      {
        dest[n] = 0;
      }
    }

    return size;
  }

}



// ========================================================================================================================
public enum EErrorReason : uint8_t
{
  None = 0,
  InputBufferFull
};


//// ========================================================================================================================
//public enum EDataSegmentType : uint8_t
//{
//  Invalid = 0,
//  GameData,
//  GameState,
//  InputData,
//  ChatData,
//  Footer
//};

// ========================================================================================================================
public enum ECompletionReason : uint8_t
{
  Invalid = 0,
  NormalDisconnect,
  Error
};

// ========================================================================================================================
enum EReplayFileMode : uint8_t
{
  REPLAY_FILE_MODE_INVALID = 0,
  REPLAY_FILE_MODE_READ,
  REPLAY_FILE_MODE_WRITE,

  // The replay data is complete.  No new data can be added now.
  REPLAY_FILE_MODE_COMPLETE
};


// ========================================================================================================
[StructLayout(LayoutKind.Sequential, Pack = 1)]
public unsafe struct GameInput
{
  public const UInt16 GAMEINPUT_MAX_BYTES = 7;
  public const UInt16 GAMEINPUT_MAX_PLAYERS = 4;    // NOTE: This probably need to be 2?
  public const int NULL_FRAME = -1;


  public int frame;
  public int size; /* size in bytes of the entire input for all players */

  // NOTE: This is probably too big in terms of copying data around locally....
  private const int BITS_SIZE = GAMEINPUT_MAX_BYTES * GAMEINPUT_MAX_PLAYERS;
  public fixed byte data[BITS_SIZE];

  // ------------------------------------------------------------------------------------------
  public GameInput() { }

  public unsafe void Clear()
  {
    frame = 0;
    size = 0;
    // NOTE: memcpy or something else is probably better here...
    for (int i = 0; i < BITS_SIZE; i++)
    {
      data[i] = 0;
    }
  }

  public bool is_null() { return frame == NULL_FRAME; }

  // ------------------------------------------------------------------------------------------
  public void init(int iframe, byte[] ibits, int isize, int offset)
  {
    Utils.ASSERT(isize < GAMEINPUT_MAX_BYTES);

    frame = iframe;
    size = isize;

    // TODO: We could probably come up with a better way to copy this data...
    for (int i = 0; i < size; i++)
    {
      data[i] = 0;
    }
    if (ibits != null)
    {
      for (int i = 0; i < size; i++)
      {
        if (i < size)
        {
          data[i + offset] = ibits[i];
        }
      }
    }

    // C++ style!
    //frame = iframe;
    //size = isize;
    //memset(bits, 0, sizeof(bits));
    //if (ibits)
    //{
    //  memcpy(bits + (offset * isize), ibits, isize);
    //}

  }

  // ------------------------------------------------------------------------------------------
  public void init(int iframe, byte[] ibits, int isize)
  {
    init(iframe, ibits, isize, 0);
  }

  // ----------------------------------------------------------------------------------------
  public bool value(int i)
  {
    return (data[i / 8] & (1 << (i % 8))) != 0;
  }

  // ----------------------------------------------------------------------------------------
  public void set(int i)
  {
    data[i / 8] |= (byte)(1 << (i % 8));
  }

  // ----------------------------------------------------------------------------------------
  public void clear(int i)
  {
    data[i / 8] &= (byte)~(1 << (i % 8));
  }

  // ----------------------------------------------------------------------------------------
  public unsafe void erase()
  {
    fixed (byte* pBits = data)
    {
      Unsafe.InitBlock(pBits, 0, BITS_SIZE);
    }
  }

  // ----------------------------------------------------------------------------------------
  public void desc(byte[] buf, int buf_size, bool show_frame = true)
  {
    // NOTE: I am not porting this as it is just some expensive logging messages
    // that can be handled in a better way, both in C++ and here.

    // Refer to C++ version for original code.
  }

  // ----------------------------------------------------------------------------------------
  public bool equal(in GameInput other)
  {
    ///bool bitsonly = true;
    //if (!bitsonly && frame != other.frame)
    //{
    //  Utils.Log("frames don't match: %d, %d", frame, other.frame);
    //}
    //if (size != other.size)
    //{
    //  Utils.Log("sizes don't match: %d, %d", size, other.size);
    //}

    bool memMatch = false;
    fixed (byte* p = data)
    fixed (byte* p2 = other.data)
    {
      memMatch = Utils.MemMatches(p, p2, size);
    }

    //if (!memMatch)
    //{
    //  Utils.Log("bits don't match");
    //}

    Utils.ASSERT(size != 0 && other.size != 0);
    return (frame == other.frame) &&
           size == other.size &&
           memMatch;
  }

}

