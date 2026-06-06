using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace GGPOSharp;

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
  private static extern int CompleteReplay(IntPtr replayFile, int frame, byte completionReason, byte errReason, byte[] message, byte messageSize);

  [DllImport(LIB_NAME, EntryPoint = "ReplayFile_AddInput", CallingConvention = CallingConvention.Cdecl)]
  private static extern int ReplayFile_AddInput(IntPtr file, IntPtr input);

  [DllImport(LIB_NAME, EntryPoint = "ReplayFile_GetNextInput", CallingConvention = CallingConvention.Cdecl)]
  private static extern int ReplayFile_GetNextInput(IntPtr file, IntPtr input);

  [DllImport(LIB_NAME, EntryPoint = "ReplayFile_GetGameData", CallingConvention = CallingConvention.Cdecl)]
  private static extern int ReplayFile_GetGameData(IntPtr file, IntPtr gameData);


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

  // --------------------------------------------------------------------------------------------------------------------------
  /// <summary>
  /// Open a replay file for writing at the given path, using given data + state.
  /// </summary>
  public ReplayFile(string path, CGameData gameData_, CGameState? state)
  {
    _GameData = gameData_;

    byte[] usePath = Encoding.UTF8.GetBytes(path);
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
  public ReplayFile(string path)
  {
    IsModeWrite = false;

    {
      byte[] usePath = Encoding.UTF8.GetBytes(path);
      int code = ReplayFile_OpenRead(usePath, ref ReplayHandle);
      ThrowIfNotOK(code);
    }
    {
      GCHandle gch = GCHandle.Alloc(_GameData, GCHandleType.Pinned);
      var useData = gch.AddrOfPinnedObject();
      try
      {
        int code = ReplayFile_GetGameData(ReplayHandle, useData);
        ThrowIfNotOK(code);
      }
      finally
      {
        gch.Free();
      }
    }


    // ALL GOOD!
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
  public void GetNextInput(ref GameInput input)
  {
    GCHandle gch = GCHandle.Alloc(input, GCHandleType.Pinned);
    var useInput = gch.AddrOfPinnedObject();
    try
    {
      int code = ReplayFile_GetNextInput(ReplayHandle, useInput);
      ThrowIfNotOK(code);
    }
    finally
    {
      gch.Free();
    }
  }

  // --------------------------------------------------------------------------------------------------------------------------
  public void AddInput(ref GameInput input)
  {
    GCHandle gch = GCHandle.Alloc(input, GCHandleType.Pinned);
    var useInput = gch.AddrOfPinnedObject();
    try
    {
      int code = ReplayFile_AddInput(ReplayHandle, useInput);
      ThrowIfNotOK(code);
    }
    finally
    {
      gch.Free();
    }
  }


  // --------------------------------------------------------------------------------------------------------------------------
  /// <summary>
  /// Complete the writing of a replay file.
  /// </summary>
  public void CompleteWrite(int onFrame, ECompletionReason completionReason, EErrorReason errReason, string message)
  {
    byte[] msgData = Encoding.UTF8.GetBytes(message);
    byte msgSize = (byte)msgData.Length;

    int closeCode = CompleteReplay(ReplayHandle, onFrame, (byte)ECompletionReason.NormalDisconnect, (byte)EErrorReason.None, msgData, msgSize);
    if (closeCode != 0)
    {
      Console.WriteLine("Could not close the replay file properly!");
      return;
    }

    ReplayHandle = IntPtr.Zero;
  }



  // --------------------------------------------------------------------------------------------------------------------------
  private void ThrowIfNotOK(int code, string exMessge = "Error in ReplayFile!")
  {
    if (code != 0)
    {
      LastError(ErrMsgBuffer, ERR_BUF_SIZE);
      string errMsg = Encoding.UTF8.GetString(ErrMsgBuffer);
      throw new ReplayFileException(exMessge, code, errMsg);
    }
  }
}



// ==============================================================================================================================
[Serializable]
public class ReplayFileException : Exception
{
  /// <summary>
  /// Return code as reported by the interop library.
  /// </summary>
  public readonly int LibCode = 0;

  /// <summary>
  /// Error message as reported by the interop library
  /// </summary>
  public readonly string? LibErrMsg = null;

  public ReplayFileException() { }
  public ReplayFileException(string message) : base(message) { }

  // --------------------------------------------------------------------------------------------------------------------------
  public ReplayFileException(string message, int code_, string msg_) : base(message)
  {
    LibCode = code_;
    LibErrMsg = msg_;
  }

  public ReplayFileException(string message, Exception inner) : base(message, inner) { }
  protected ReplayFileException(
  System.Runtime.Serialization.SerializationInfo info,
  System.Runtime.Serialization.StreamingContext context) : base(info, context) { }
}