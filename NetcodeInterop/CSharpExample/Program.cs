using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;

using GGPOSharp;

internal class Program
{

  // --------------------------------------------------------------------------------------------------------------------------
  private unsafe static void Main(string[] args)
  {
    Console.WriteLine("Hello, World!");

    // ErrorMessageTest();

    Console.WriteLine("Making a replay file....");

    IntPtr replayFile = IntPtr.Zero;
    IntPtr state = IntPtr.Zero;

    string testPath = "test-replay-file.replay";
    //var usePath = Encoding.UTF8.GetBytes(testPath);

    const int TOTAL_INPUT_SIZE = 10;
    const int MAX_PLAYERS = 2;
    const int PLAYER_INPUT_SIZE = TOTAL_INPUT_SIZE / MAX_PLAYERS;

    CGameData gameData = new CGameData();
    gameData.GameName = "sf3iiinr1";
    gameData.GameVersion = "0.5-1a";
    gameData.MaxPlayerCount = MAX_PLAYERS;
    gameData.TotalInputSize = TOTAL_INPUT_SIZE;
    gameData.SetPlayerName(0, "Funky Dave");
    gameData.SetPlayerName(1, "Spicy Sam");

    //byte[] inputBuffer = new byte[TOTAL_INPUT_SIZE];
    //for (int i = 0; i < TOTAL_INPUT_SIZE; i++)
    //{
    //  inputBuffer[i] = 0;
    //}

    const int REPLAY_INPUT_COUNT = 10;

    using (var replay = new ReplayFile(testPath, gameData, null))
    {
      Console.WriteLine($"The file is open for write at:  {Path.GetFullPath(testPath)}");

      // TODO: Add at least one game input!
      for (byte i = 0; i < REPLAY_INPUT_COUNT; i++)
      {
        //inputBuffer[i] = (byte)(i + 1);
        //inputBuffer[PLAYER_INPUT_SIZE] = (byte)(i + 2);

        GameInput input = new GameInput();
        input.size = TOTAL_INPUT_SIZE;
        input.data[i] = (byte)(i + 1);
        input.data[PLAYER_INPUT_SIZE] = (byte)(i + 2);

        replay.AddInput(ref input);
      }


      replay.CompleteWrite(1, ECompletionReason.NormalDisconnect, EErrorReason.None, "ALL GOOD BEBE!");
      Console.WriteLine("file is complete!");
    }

    Console.WriteLine("Reading back replay file...");
    using(var check = new ReplayFile(testPath))
    {
      var checkGameData = check.GameData;
      if (check.GameData.GameName != gameData.GameName) { throw new Exception("Game names do not match!"); }

      for (int i = 0; i < REPLAY_INPUT_COUNT; i++)
      {
        GameInput input = new GameInput();
        check.GetNextInput(ref input);
      }
    }


    // Let's do a test where we read the file back in!
    // TODO: Read the file back in!

  }

  // --------------------------------------------------------------------------------------------------------------------------
  private static void ErrorMessageTest()
  {
    //TestError();
    //byte[] msg = new byte[0x400];
    //int msgSize = LastError(msg, 0x400);

    //string hexCodes = "";
    //for (int i = 0; i < msgSize; i++)
    //{
    //  hexCodes += $"{msg[i]:x} ".ToUpper();
    //}
    //Console.WriteLine(hexCodes);

    //string errMsg = Encoding.UTF8.GetString(msg, 0, msgSize);

    //Console.OutputEncoding = Encoding.UTF8;
    //Console.WriteLine($"The error message is: {errMsg ?? "<null>"}");
  }
}
