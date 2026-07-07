using drewCo.Tools.Logging;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace funscrew;

// ==============================================================================================================================
/// <summary>
/// Wrapper for native implementation of CReplayFile.
/// </summary>
public class ReplayFile : IDisposable
{


  #region P/Invoke

#if OS_LINUX
  const string LIB_NAME = "libNetcodeInterop.so";
#elif OS_WINDOWS
  const string LIB_NAME = "NetcodeInterop.dll";
#else
  unsupported OS....  check .csproj to add support
#endif

  //[DllImport("NetcodeCore.dll", CallingConvention = CallingConvention.Cdecl)]
  //private static extern IntPtr ReplayFile_OpenRead([MarshalAs(UnmanagedType.LPUTF8Str)] string path);

  [DllImport(LIB_NAME, EntryPoint = "ReplayFile_OpenWrite", CallingConvention = CallingConvention.Cdecl)]
  private static extern int ReplayFile_OpenWrite(ref CGameData gameData, IntPtr gameState, byte[] path, ref IntPtr replayFile);

  [DllImport(LIB_NAME, EntryPoint = "ReplayFile_OpenRead", CallingConvention = CallingConvention.Cdecl)]
  private static extern int ReplayFile_OpenRead(byte[] path, ref IntPtr replayFile);

  [DllImport(LIB_NAME, EntryPoint = "ReplayFile_Close", CallingConvention = CallingConvention.Cdecl)]
  private static extern int ReplayFile_Close(IntPtr replayFile);

  [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
  private static extern int CompleteReplay(IntPtr replayFile, byte completionReason, byte errReason, byte[] message, byte messageSize);

  [DllImport(LIB_NAME, EntryPoint = "ReplayFile_AddInput", CallingConvention = CallingConvention.Cdecl)]
  private static extern int ReplayFile_AddInput(IntPtr file, ref GameInput input);

  [DllImport(LIB_NAME, EntryPoint = "ReplayFile_AddChat", CallingConvention = CallingConvention.Cdecl)]
  private static extern int ReplayFile_AddChat(IntPtr file, ref CChatData chat);

  [DllImport(LIB_NAME, EntryPoint = "ReplayFile_GetNextInput", CallingConvention = CallingConvention.Cdecl)]
  private static extern int ReplayFile_GetNextInput(IntPtr file, ref GameInput input);

  [DllImport(LIB_NAME, EntryPoint = "ReplayFile_GetGameData", CallingConvention = CallingConvention.Cdecl)]
  private static extern int ReplayFile_GetGameData(IntPtr file, ref CGameData gameData);

  [DllImport(LIB_NAME, EntryPoint = "ReplayFile_GetFooter", CallingConvention = CallingConvention.Cdecl)]
  private static extern int ReplayFile_GetFooter(IntPtr file, ref CFooterData gameData);

  [DllImport(LIB_NAME, EntryPoint = "ReplayFile_GetGameState", CallingConvention = CallingConvention.Cdecl)]
  private static extern int ReplayFile_GetGameState(IntPtr file, ref CGameState gameState);


  [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
  private static extern void TestError();

  [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
  private static extern int LastError(byte[] buffer, int bufferSize);

  #endregion

  private IntPtr ReplayHandle = IntPtr.Zero;
  private CGameData _GameData = default;

  private bool IsModeWrite = false;

  private const int ERR_BUF_SIZE = 0x400;
  private byte[] ErrMsgBuffer = new byte[ERR_BUF_SIZE];

  public string Path { get; private set; }

  private CGameState _GameState = default;
  public CGameState GameState { get { return _GameState; } }

  private CFooterData _FooterData = default;


  public uint FrameCount { get { return _FooterData.FrameCount; } }


  // --------------------------------------------------------------------------------------------------------------------------
  /// <summary>
  /// Open a replay file for writing at the given path, using given data + state.
  /// </summary>
  public ReplayFile(string path_, CGameData gameData_, CGameState? state)
  {
    _GameData = gameData_;

    Path = path_;

    byte[] usePath = Encoding.UTF8.GetBytes(Path);
    GCHandle gch = GCHandle.Alloc(state, GCHandleType.Pinned);
    IntPtr useState = gch.AddrOfPinnedObject();

    try
    {
      int code = ReplayFile_OpenWrite(ref _GameData, useState, usePath, ref this.ReplayHandle);
      ThrowIfNotOK(code);
    }
    finally
    {
      gch.Free();
    }

    IsModeWrite = true;
  }


  // --------------------------------------------------------------------------------------------------------------------------
  /// <summary>
  /// Open a replay file at the given path for reading.
  /// </summary>
  public ReplayFile(string path_)
  {
    IsModeWrite = false;
    Path = path_;

    {
      byte[] usePath = Encoding.UTF8.GetBytes(Path);
      int code = ReplayFile_OpenRead(usePath, ref ReplayHandle);
      ThrowIfNotOK(code);
    }
    {
      int code = ReplayFile_GetGameData(ReplayHandle, ref _GameData);
      ThrowIfNotOK(code);
    }
    {
      int code = ReplayFile_GetGameState(ReplayHandle, ref _GameState);
      ThrowIfNotOK(code);
    }
    {
      int code = ReplayFile_GetFooter(ReplayHandle, ref _FooterData);
      ThrowIfNotOK(code);
    }
  }

  // --------------------------------------------------------------------------------------------------------------------------
  public void Dispose()
  {
    if (ReplayHandle != IntPtr.Zero)
    {
      ReplayFile_Close(ReplayHandle);
      ReplayHandle = IntPtr.Zero;

      if (IsModeWrite)
      {
        throw new ReplayFileException("Disposing write mode replay file before 'CompleteWrite' was called!");
      }
    }
  }

  #region Properties 

  public CGameData GameData { get { return _GameData; } }

  #endregion

  // --------------------------------------------------------------------------------------------------------------------------
  public bool GetNextInput(ref GameInput input)
  {
    int code = ReplayFile_GetNextInput(ReplayHandle, ref input);
    if (code == (byte)EErrorCodes.ERRORCODE_NO_GAMEINPUT)
    {
      return false;
    }
    ThrowIfNotOK(code);

    // All good!
    return true;
  }

  // --------------------------------------------------------------------------------------------------------------------------
  public void AddInput(ref GameInput input)
  {
    int code = ReplayFile_AddInput(ReplayHandle, ref input);
    ThrowIfNotOK(code);
  }

  // --------------------------------------------------------------------------------------------------------------------------
  public void AddChat(ref CChatData chat)
  {
    int code = ReplayFile_AddChat(ReplayHandle, ref chat);
    ThrowIfNotOK(code);
  }

  // --------------------------------------------------------------------------------------------------------------------------
  /// <summary>
  /// Complete the writing of a replay file.
  /// </summary>
  public void CompleteWrite(ECompletionReason completionReason, EErrorReason errReason, string message)
  {
    Debug.Assert(message != null);

    if (ReplayHandle == IntPtr.Zero) { 
      Log.Warning($"The replay handle is null! TID: {Thread.CurrentThread.ManagedThreadId}");
      return;
    }

    byte[] msgData = Encoding.UTF8.GetBytes(message);
    byte msgSize = (byte)msgData.Length;

    int closeCode = CompleteReplay(ReplayHandle, (byte)ECompletionReason.NormalDisconnect, (byte)EErrorReason.None, msgData, msgSize);
    if (closeCode != 0)
    {
      Console.WriteLine("Could not close the replay file properly!");
      return;
    }

    ReplayHandle = IntPtr.Zero;
  }

  // --------------------------------------------------------------------------------------------------------------------------
  private void ThrowIfNotOK(int code)
  {
    if (code != 0)
    {
      int msgSize = LastError(ErrMsgBuffer, ERR_BUF_SIZE);
      string libErrMsg = Encoding.UTF8.GetString(ErrMsgBuffer, 0, msgSize);
      string useErrMsg = libErrMsg != string.Empty ? libErrMsg : "<no-message>";
      throw new ReplayFileException(useErrMsg, code, libErrMsg);
    }
  }

  // --------------------------------------------------------------------------------------------------------------------------
  /// <summary>
  /// Return the remainder of the game inputs, one by one, until they are all expired.
  /// </summary>
  public IEnumerable<GameInput> GetInputs()
  {
    GameInput nextInput = new GameInput();

    while (true)
    {
      bool hasInput = GetNextInput(ref nextInput);
      if (!hasInput) { break; }
      yield return nextInput;
    }
  }
}


// --------------------------------------------------------------------------------------------------------------------------
public enum EErrorCodes : byte
{
  ERRORCODE_OK = 0,
  ERRORCODE_NOTIMPLEMENTED = 1,
  ERRORCODE_UNHANDLED = 2,
  ERRORCODE_FILENOTFOUND = 3,

  /// <summary>
  /// Indicates that there is no game input.  This should only be used when calling 'GetNextInput' or similar functions.
  /// </summary>
  ERRORCODE_NO_GAMEINPUT = 4
};




// ==============================================================================================================================
[Serializable]
public class ReplayFileException : Exception
{
  /// <summary>
  /// Return code as reported by the interop library.
  /// </summary>
  public readonly int ErrorCode = 0;


  public ReplayFileException() { }
  public ReplayFileException(string message) : base(message) { }

  // --------------------------------------------------------------------------------------------------------------------------
  public ReplayFileException(string message, int errCode_, string msg_) : base(message)
  {
    ErrorCode = errCode_;
  }

  public ReplayFileException(string message, Exception inner) : base(message, inner) { }
  protected ReplayFileException(
  System.Runtime.Serialization.SerializationInfo info,
  System.Runtime.Serialization.StreamingContext context) : base(info, context) { }
}