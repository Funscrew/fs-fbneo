using System.Runtime.InteropServices;
using System.Text;



internal class Program
{
  #if OS_LINUX
  const string LIB_NAME = "libNetcodeCore.so";
  #elif OS_WINDOWS
  const string LIB_NAME = "NetcodeCore.dll";
  #else
  unsupported OS....  check .csproj to add support
  #endif

  //[DllImport("NetcodeCore.dll", CallingConvention = CallingConvention.Cdecl)]
  //private static extern IntPtr ReplayFile_OpenRead([MarshalAs(UnmanagedType.LPUTF8Str)] string path);

  [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
  private static extern void TestError();

  //[DllImport("NetcodeCore.dll", CallingConvention = CallingConvention.Cdecl)]
  //private static extern IntPtr LastError();

  [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
  private static extern int LastError(byte[] buffer, int bufferSize);

  private static void Main(string[] args)
  {
    Console.WriteLine("Hello, World!");

    Console.WriteLine("Making a replay file....");

    TestError();
    byte[] msg = new byte[0x400];
    int msgSize = LastError(msg, 0x400);

    string hexCodes = "";
    for (int i = 0; i < msgSize; i++)
    {
      hexCodes += $"{msg[i]:x} ".ToUpper();
    }
    Console.WriteLine(hexCodes);

    //foreach (int i in msgSize) {
    //}

    string errMsg = Encoding.UTF8.GetString(msg, 0, msgSize - 1);

    Console.OutputEncoding = Encoding.UTF8;
    Console.WriteLine($"The error message is: {errMsg ?? "<null>"}");

    //var rpf = ReplayFile_OpenRead("MyPath.fr1");
    //Console.WriteLine($"replay file is: {rpf}");
  }
}
