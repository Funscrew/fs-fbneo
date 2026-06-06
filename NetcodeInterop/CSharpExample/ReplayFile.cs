using System;
using System.Collections.Generic;
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

  [DllImport(LIB_NAME, EntryPoint = "ReplayFile_OpenWrite")]
  private static extern int ReplayFile_OpenWrite(ref CGameData gameData, IntPtr gameState, byte[] path, ref IntPtr replayFile);

  [DllImport(LIB_NAME, EntryPoint = "ReplayFile_OpenRead")]
  private static extern int ReplayFile_OpenRead(byte[] path, ref IntPtr replayFile);

  [DllImport(LIB_NAME, EntryPoint = "ReplayFile_Close")]
  private static extern int ReplayFile_Close(IntPtr replayFile);

  [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
  private static extern int CompleteReplay(IntPtr replayFile, int frame, byte completionReason, byte errReason, byte[] message, byte messageSize);

  // CReplayFile* target, int frame, ECompletionReason reason, EErrorReason errReason, char* message, uint8_t messageSize) {
  // CompleteReplay

  [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
  private static extern void TestError();

  [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
  private static extern int LastError(byte[] buffer, int bufferSize);

  #endregion

  private IntPtr ReplayHandle = IntPtr.Zero;
  private CGameData GameData = default;

  private bool IsModeWrite = false;

  // --------------------------------------------------------------------------------------------------------------------------
  /// <summary>
  /// Open a replay file for writing at the given path, using given data + state.
  /// </summary>
  public ReplayFile(string path, CGameData gameData_, CGameState? state)
  {
    GameData = gameData_;

    byte[] usePath = Encoding.UTF8.GetBytes(path);
    IntPtr useState = IntPtr.Zero;
    GCHandle gch = GCHandle.Alloc(state, GCHandleType.Pinned);

    try
    {
      int code = ReplayFile_OpenWrite(ref GameData, useState, usePath, ref this.ReplayHandle);
      if (code != 0)
      {
        throw new InvalidOperationException($"Could not open replay file for write!  Code = {code}");
      }
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
    throw new InvalidOperationException();
  }

  // --------------------------------------------------------------------------------------------------------------------------
  public void Dispose()
  {
    if (ReplayHandle != IntPtr.Zero)
    {
      ReplayFile_Close(ReplayHandle);
      ReplayHandle =IntPtr.Zero;

      if (IsModeWrite) {
        throw new ReplayFileException("Disposing write mode replay file before 'CompleteWrite' was called!");
      }
    }

    //try
    //{

    //}
    //catch (Exception)
    //{

    //  throw;
    //}
  }


  // --------------------------------------------------------------------------------------------------------------------------
  public void AddInput() { }


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

}



// ==============================================================================================================================
[Serializable]
public class ReplayFileException : Exception
{
  public ReplayFileException() { }
  public ReplayFileException(string message) : base(message) { }
  public ReplayFileException(string message, Exception inner) : base(message, inner) { }
  protected ReplayFileException(
  System.Runtime.Serialization.SerializationInfo info,
  System.Runtime.Serialization.StreamingContext context) : base(info, context) { }
}