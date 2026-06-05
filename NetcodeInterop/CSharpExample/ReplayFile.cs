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
public class ReplayFile
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

  // --------------------------------------------------------------------------------------------------------------------------
  /// <summary>
  /// Open a replay file for writing at the given path, using given data + state.
  /// </summary>
  public ReplayFile(string path, CGameData gameData, CGameState state)
  {
    throw new InvalidOperationException();
  }

  // --------------------------------------------------------------------------------------------------------------------------
  /// <summary>
  /// Open a replay file at the given path for reading.
  /// </summary>
  public ReplayFile(string path) { 
  }
}

