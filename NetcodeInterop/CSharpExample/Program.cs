using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;



internal class Program
{

  // --------------------------------------------------------------------------------------------------------------------------
  private static void Main(string[] args)
  {
    Console.WriteLine("Hello, World!");

    // ErrorMessageTest();

    Console.WriteLine("Making a replay file....");

    IntPtr replayFile = IntPtr.Zero;
    IntPtr state = IntPtr.Zero;

    string testPath = "test-replay-file.replay";
    var usePath = Encoding.UTF8.GetBytes(testPath);


    CGameData gameData = new CGameData();
    gameData.GameName = "sf3iiinr1";
    gameData.GameVersion = "0.5-1a";
    gameData.MaxPlayerCount = 2;
    gameData.TotalInputSize = 10;

    gameData.SetPlayerName(0, "Funky Dave");
    gameData.SetPlayerName(1, "Spicy Sam");

    Console.WriteLine("Opening replay file.....");
    int openCode = ReplayFile_OpenWrite(ref gameData, state, usePath, ref replayFile);
    if (openCode != 0)
    {
      Console.WriteLine("There was an error!");
      return;
    }
    Console.WriteLine("The file is open....");

    string msg = "WOWEE!";
    byte[] msgData = Encoding.UTF8.GetBytes(msg);
    byte msgSize = (byte)msgData.Length;
    int closeCode = CompleteReplay(replayFile, 1, (byte)ECompletionReason.NormalDisconnect, (byte)EErrorReason.None, msgData, msgSize);
    if (closeCode != 0)
    {
      Console.WriteLine("Could not close the replay file!");
      return;
    }

    Console.WriteLine("complete!");
  }

  // --------------------------------------------------------------------------------------------------------------------------
  private static void ErrorMessageTest()
  {
    TestError();
    byte[] msg = new byte[0x400];
    int msgSize = LastError(msg, 0x400);

    string hexCodes = "";
    for (int i = 0; i < msgSize; i++)
    {
      hexCodes += $"{msg[i]:x} ".ToUpper();
    }
    Console.WriteLine(hexCodes);

    string errMsg = Encoding.UTF8.GetString(msg, 0, msgSize);

    Console.OutputEncoding = Encoding.UTF8;
    Console.WriteLine($"The error message is: {errMsg ?? "<null>"}");
  }
}
