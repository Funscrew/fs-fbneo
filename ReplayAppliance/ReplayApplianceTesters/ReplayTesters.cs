using drewCo.Tools;
using funscrew;
using funscrew.Clients;
using System;
using System.Collections.Generic;
using System.Diagnostics.Contracts;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace funscrewTesters
{

  // ==============================================================================================================================
  public class ReplayTesters : TestBase
  {
    private const string TEST_DATA_DIR = "ReplayData";

    // -----------------------------------------------------------------------------------------------------------------------
    public ReplayTesters()
    {
      FileTools.CreateDirectory(TEST_DATA_DIR);
    }

    // -----------------------------------------------------------------------------------------------------------------------
    /// <summary>
    /// This test case shows that we can merge game inputs (and other) messages that come along and record them to disk or wherever.
    /// </summary>
    [Test]
    public unsafe void CanRecordAndLoadReplayInformation()
    {

      var sessionId = GetNextSessionId();
      const string TEST_GAME_NAME = "MyGame"; //"MyGame -€";     // NOTE: Use unicode character to show proper UTF-8 support!

      const int INPUT_SIZE = 5;

      var recorder = new GameRecorder(new CGameData()
      {
        GameName = TEST_GAME_NAME,
        MaxPlayerCount = 2,
        TotalInputSize = 2 * INPUT_SIZE
      }, TEST_DATA_DIR, sessionId, true);


      // Add some inputs for the players...
      GameInput p1Input = new GameInput();
      GameInput p2Input = new GameInput();

      const string CHAT1_MSG = "hello";
      const string CHAT2_MSG = "hi";

      int chatsAdded = 0;

      const int FRAME_COUNT = 50;
      for (int i = 0; i < FRAME_COUNT; i++)
      {
        int frameNumber = i + 1;
        p1Input.frame = frameNumber;
        p1Input.data[0] = (byte)(i % 256);

        p2Input.frame = frameNumber;
        p2Input.data[0] = (byte)((i + 1) % 256);

        recorder.AddInput(0, ref p1Input);
        recorder.AddInput(1, ref p2Input);

        // Add some chitchat....
        if (frameNumber % 11 == 0)
        {
          var chat1 = new CChatData()
          {
            Frame = frameNumber,
            Message = CHAT1_MSG,
            FromPlayerIndex = 0,
            ToPlayerIndex = 1
          };
          recorder.AddChatSegment(ref chat1);
          ++chatsAdded;
        }

        if (frameNumber % 17 == 0)
        {
          var chat2 = new CChatData()
          {
            Frame = frameNumber,
            Message = CHAT2_MSG,
            FromPlayerIndex = 1,
            ToPlayerIndex = 0,
          };
          recorder.AddChatSegment(ref chat2);
          ++chatsAdded;
        }

      }
      recorder.CompleteReplay(p1Input.frame + 1, ECompletionReason.NormalDisconnect, EErrorReason.None, "OK");
      recorder.Dispose();

      var replayFile = new ReplayFile(recorder.FilePath);

      // TODO: Best way to show that this is OK is to read the file back
      // and enure that the data is what we expect it to be.
      Assert.That(replayFile.GameData.GameName, Is.EqualTo(TEST_GAME_NAME), "Incorrect game name!");

      // Let's grab the inputs and see what they actually are...
      var allInputs = replayFile.GetInputs().ToList();
      Assert.That(allInputs.Count, Is.EqualTo(FRAME_COUNT), "Incorrect number of inputs!");

      // Then we will confirm that the frame numbers are correct, ordinal, and that the data is what we expect!
      for (int i = 0; i < FRAME_COUNT; i++)
      {
        int frameNumber = i + 1;
        GameInput gi = allInputs[i];
        Assert.That(gi.frame, Is.EqualTo(frameNumber), $"Incorrect frame # for index: {i}");

        // Make sure that the data is correct...
        byte p1Data = gi.data[0];
        byte p2Data = gi.data[INPUT_SIZE];

        byte ep1 = (byte)(i % 256);
        byte ep2 = (byte)((i + 1) % 265);

        Assert.That(p1Data, Is.EqualTo(ep1), $"Invalid input information for p1 @ frame: {frameNumber}!");
        Assert.That(p2Data, Is.EqualTo(ep2), $"Invalid input information for p2 @ frame: {frameNumber}!");
      }
    }

  }


}
