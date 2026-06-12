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
    byte[][] inputSets = new byte[REPLAY_INPUT_COUNT][];

    for (int i = 0; i < REPLAY_INPUT_COUNT; i++)
    {
      byte[] toUse = RandomNumberGenerator.GetBytes(TOTAL_INPUT_SIZE);
      inputSets[i] = toUse;
    }


    const int CHAT1_FRAME = 2;
    const string CHAT_MSG_1 = "My Message!";

    const int CHAT2_FRAME = 3;
    const string CHAT_MSG_2 = "Their Message!";



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
        input.frame = i + 1;

        replay.AddInput(ref input);

        if (input.frame == CHAT1_FRAME)
        {
          var cData = new CChatData();
          cData.Message = CHAT_MSG_1;
          cData.Frame = input.frame;
          cData.FromPlayerIndex = 0;
          cData.ToPlayerIndex = 1;
          replay.AddChat(ref cData);
        }

        if (input.frame == CHAT2_FRAME)
        {
          var cData = new CChatData();
          cData.Message = CHAT_MSG_2;
          cData.Frame = input.frame;
          cData.FromPlayerIndex = 0;
          cData.ToPlayerIndex = 1;
          replay.AddChat(ref cData);
        }

      }


      replay.CompleteWrite(1, ECompletionReason.NormalDisconnect, EErrorReason.None, "ALL GOOD BEBE!");
      Console.WriteLine("Replay recording is complete!");
    }

    Console.WriteLine("Reading back replay file...");
    using (var check = new ReplayFile(testPath))
    {
      var checkGameData = check.GameData;
      if (check.GameData.GameName != gameData.GameName) { throw new Exception("Game names do not match!"); }

      for (int i = 0; i < REPLAY_INPUT_COUNT; i++)
      {
        GameInput input = new GameInput();

        // TODO:
        // I want a way to get the next "event", or all "events" for the current frame....
        // That means that the events should be abstracted somehow?
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


    Console.WriteLine("Readback is complete!");


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
