using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;

using GGPOSharp;

internal class Program
{
  private Random RNG = new Random();

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


    const int REPLAY_INPUT_COUNT = 10;
    //byte[] p1Vals = new byte[REPLAY_INPUT_COUNT];
    //byte[] p2Vals = new byte[REPLAY_INPUT_COUNT];
    byte[][] inputSets = new byte[REPLAY_INPUT_COUNT][];

    for (int i = 0; i < REPLAY_INPUT_COUNT; i++)
    {
      byte[] toUse = RandomNumberGenerator.GetBytes(TOTAL_INPUT_SIZE);
      inputSets[i] = toUse; // new byte[TOTAL_INPUT_SIZE];
      //for (int j = 0; j < TOTAL_INPUT_SIZE; j++)
      //{

      //}
    }

    using (var replay = new ReplayFile(testPath, gameData, null))
    {
      Console.WriteLine($"The file is open for write at:  {Path.GetFullPath(testPath)}");

      for (byte i = 0; i < REPLAY_INPUT_COUNT; i++)
      {
        GameInput input = new GameInput();
        input.size = TOTAL_INPUT_SIZE;

        // TODO: We should come up with some approach to randomized the inputs, across the board...

        for (int j = 0; j < TOTAL_INPUT_SIZE; j++)
        {
          input.data[j] = inputSets[i][j];
        }
        //input.data[(i % PLAYER_INPUT_SIZE)] = p1Vals[i];
        //input.data[(i % PLAYER_INPUT_SIZE) + PLAYER_INPUT_SIZE] = p2Vals[i];
        input.frame = i + 1;

        replay.AddInput(ref input);
      }


      replay.CompleteWrite(1, ECompletionReason.NormalDisconnect, EErrorReason.None, "ALL GOOD BEBE!");
      Console.WriteLine("file is complete!");
    }

    Console.WriteLine("Reading back replay file...");
    using (var check = new ReplayFile(testPath))
    {
      var checkGameData = check.GameData;
      if (check.GameData.GameName != gameData.GameName) { throw new Exception("Game names do not match!"); }

      for (int i = 0; i < REPLAY_INPUT_COUNT; i++)
      {
        GameInput input = new GameInput();
        check.GetNextInput(ref input);

        if (input.frame != i + 1)
        {
          throw new InvalidOperationException("Incorrect frame #!");
        }

        var compSet = inputSets[i];
        for (int j = 0; j < TOTAL_INPUT_SIZE; j++)
        {
          var id = input.data[j];
          var cd = compSet[j];

          bool match = id == cd;
          if (!match)
          {
            throw new Exception($"Input data for frame: {i + 1} does not match at index: {j}");
          }
        }
      }
    }

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
