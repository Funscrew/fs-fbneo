using drewCo.Tools;
using drewCo.Tools.Logging;
using System.Runtime.CompilerServices;
using System.Text;

namespace funscrew
{
  // ==============================================================================================================================
  /// <summary>
  /// This is the thing that records inputs, etc. for a game.
  /// Thre recorded files can then be used for replay, and in some cases, direct streaming.
  /// </summary>
  public class GameRecorder : IDisposable
  {
    private UInt64 SessionId = 0;
    private string DataDir = null!;

    // I need a window of currently active inputs.  We don't want to have them always hanging around.
    // this window of active inputs is also what can be used in cases where we want to stream the game out to
    // active clients.
    // TODO: I think that we can convert this to EZQ as well!
    private RingBuffer<GameInput> MergedInputs = new RingBuffer<GameInput>(64);

    // Each player will also have their inputs in their own slightly larger buffers.
    // These buffers should never overflow and if they so then there is a problem.
    // OPTIONS: Ten seconds @ 60fps: NOTE: This is maybe a bit too big in actual application....
    const int PLAYER_INPUT_BUFFER_SIZE = 60 * 10;     //  0x70;

    // OPTIONS:
    const int MAX_PLAYERS = 2;

    /// <summary>
    /// Used to track what should be the expected starting frame on merge operations when a given buffer is empty!
    /// </summary>
    private int SyncedBaseFrame = 1;

    private EZQ<GameInput>[] PlayerBuffers = null!;
    private int[] BaseFrames = null!;

    private GameInput[] MergeBuffer = null;

    private CGameData GameData = default;

    private byte[] WriteBuffer = new byte[0x800];

    public bool RecordingComplete { get; private set; } = false;
    public bool HasError { get { return ErrorReason != EErrorReason.None; } }
    public string? ErrorMessage { get; private set; } = null!;
    public EErrorReason ErrorReason { get; private set; }


    private ReplayFile ReplayFile = null!;
    public string FilePath { get { return ReplayFile.Path; } }

    // -----------------------------------------------------------------------------------------------------------------------
    // NOTE: In production environments, game data should not be allowed to be overwritten!
    public unsafe GameRecorder(CGameData gameData_, string dataDir_, UInt64 sessionId_, string stateFileName, bool overwriteExisting = false)
    {
      GameData = gameData_;
      SessionId = sessionId_;
      DataDir = dataDir_;

      if (SessionId == GGPOConsts.TEST_SESSION_ID)
      {
        Log.Debug($"Magic session id: {GGPOConsts.TEST_SESSION_ID} was used, replay overwrite is enabled!");
        overwriteExisting = true;
      }

      // Let's create the file.  If it already exists, then we have a problem / invalid session ID!
      string usePath = Path.Combine(DataDir, SessionId + ".replay");
      if (File.Exists(usePath) && !overwriteExisting)
      {
        throw new InvalidOperationException($"Data file for session id: {SessionId} already exists!");
      }

      // NOTE: The emulator will run a lookup for the state file name.
      // There are some weirdo rules / names for ranked / unranked even tho they don't appear to matter...
      // Therefore, I am not going to include any directory information at this time....
      // NOTE: There are a small handful of 'ranked' games like KOF97 that originall have both a '_fbneo' and a '_fbneo_ranked' version.
      // I have no clue what the difference is, or how we might reliably detect it...
      // Probably some kind of bullshit hard-coded lookup table?  Not really sure what to do about it long term....
      // Maybe a game distribution system can figure it out?
      var stateFilePathData = Encoding.UTF8.GetBytes(stateFileName);

      fixed (byte* buffer = stateFilePathData)
      {
        CGameState state = new CGameState();
        state.Type = (uint8_t)EGameStateType.GAMESTATE_TYPE_FILE;
        state.StartFrame = 0;   // 0 == NOT SET.
        state.DataSize = (uint32_t)stateFilePathData.Length;
        state.Data = buffer;

        // NOTE: This would be a CRC32 for the contents of the indicated file!  We use zero for now b/c I don't care about checking it.
        state.CRC32 = 0;

        // We don't need state or its memory after it is written....
        ReplayFile = new ReplayFile(usePath, GameData, state);
        stateFilePathData = null;
      }




      BaseFrames = new int[MAX_PLAYERS];
      int len = BaseFrames.Length;
      for (int i = 0; i < len; i++)
      {
        BaseFrames[i] = 0;
      }
      PlayerBuffers = new EZQ<GameInput>[MAX_PLAYERS];
      for (int i = 0; i < MAX_PLAYERS; i++)
      {
        PlayerBuffers[i] = new EZQ<GameInput>(PLAYER_INPUT_BUFFER_SIZE);
      }
      MergeBuffer = new GameInput[MAX_PLAYERS];
    }

    // -----------------------------------------------------------------------------------------------------------------------
    public void Dispose()
    {
      ReplayFile.Dispose();
    }

    // -----------------------------------------------------------------------------------------------------------------------
    /// <summary>
    /// Use this to complete the recording of the replay + indicate the
    /// reason why it was completed.  This could be through a proper disconnect,
    /// or an error, etc.
    /// </summary>
    public void CompleteReplay(int frame, ECompletionReason reason, EErrorReason errReason, string? message)
    {
      if (RecordingComplete) { return; }
      int useFrame = frame == -1 ? SyncedBaseFrame : frame;
      ReplayFile.CompleteWrite(reason, errReason, string.Empty);
      RecordingComplete = true;
    }

    // -----------------------------------------------------------------------------------------------------------------------
    public void AddChatSegment(ref CChatData chat)
    {
      if (string.IsNullOrWhiteSpace(chat.Message)) { return; }
      chat.Message = StringTools.Truncate(chat.Message, CChatData.CHAT_DATA_MAX);

      ReplayFile.AddChat(ref chat);
    }

    // -----------------------------------------------------------------------------------------------------------------------
    /// <returns>
    /// Boolean value indicating if the frame was accepted.
    /// Returns false if the frame already exists</returns>
    public unsafe bool AddInput(int playerIndex, ref GameInput input)
    {
      var buf = PlayerBuffers[playerIndex];

      if (buf.IsFull)
      {
        // TODO: Shutdown the recorder correctly!
        // Send other events!
        // We don't want to use exceptions for flow control, rather we need to set an error state on the recorder!
        // throw new InvalidOperationException($"Input buffer for player: {playerIndex} is full!");

        this.OnError(EErrorReason.InputBufferFull, $"Too many unmerged inputs from Player:{playerIndex + 1}! (size={buf.Capacity})");
        return false;
      }

      // It is OK if we add a duplicate frame.
      // NOTE: This is a copy we can probably avoid!
      // The start frame should be per-player, otherwise they have to always be in perfect sync...
      int startFrame = BaseFrames[playerIndex]; /// SyncedBaseFrame;
      if (input.frame == startFrame)
      {
        // Ignore duplicate frame.
        // TODO: Log this, we may not actually need it...
        return false;
      }

      if (input.frame != startFrame + 1)
      {
        // TODO: Close the recording here, properly!
        throw new InvalidOperationException("Invalid frame number!");
      }
      buf.Push(input);
      startFrame++;
      BaseFrames[playerIndex] = startFrame;

      int len = this.PlayerBuffers.Length;
      bool popIt = true;

      while (true)
      {
        int startMergeFrame = SyncedBaseFrame;

        // throw new InvalidOperationException("refigure how to do the sync/global base frames");

        // Now that we have added a frame, we will go to the back, and find
        // all frames that match + do the merge.
        for (int i = 0; i < len; i++)
        {
          var pBuf = this.PlayerBuffers[i];
          if (pBuf.Count == 0)
          {
            popIt = false;
            break;
          }

          GameInput giBuf = new GameInput();
          pBuf.First(ref giBuf);
          if (giBuf.frame != startMergeFrame)
          {
            throw new InvalidOperationException($"Invalid (back) frame number at player index: {i}!  Should be {startMergeFrame}!");
          }
        }

        // Nothing left to confirm!
        if (!popIt) { break; }

        // Copy data for the merge!
        for (int i = 0; i < len; i++)
        {
          var giBuf = new GameInput();
          this.PlayerBuffers[i].First(ref giBuf);
          MergeBuffer[i] = giBuf;

          this.PlayerBuffers[i].Pop();
        }

        MergeInputs();


        // Do the merge here + write the segment!
        startMergeFrame++;
        SyncedBaseFrame = startMergeFrame;
      }

      return true;
    }

    // -----------------------------------------------------------------------------------------------------------------------
    private void OnError(EErrorReason errReason, string message)
    {
      this.ErrorReason = errReason;
      this.ErrorMessage = message;
      CompleteReplay(this.SyncedBaseFrame, ECompletionReason.Error, errReason, message);
    }

    // -----------------------------------------------------------------------------------------------------------------------
    private unsafe void MergeInputs()
    {
      // We want to create a single GameInput instance from a full set.
      int len = this.PlayerBuffers.Length;
      int offset = this.GameData.TotalInputSize / len;

      // Reminder:  The original game inputs all come with the data in the P0 slot.
      // Therefore we need to be make sure that they end up in their game-correct
      // positions for playback / correct interpretation later.
      GameInput merged = new GameInput();
      merged.size = this.GameData.TotalInputSize;
      merged.frame = MergeBuffer[0].frame;

      for (int i = 0; i < len; i++)
      {
        // Make sure that the frames are correct (NOTE: This may be done in a previous step already....);
        if (MergeBuffer[i].frame != merged.frame)
        {
          // CompleteReplay(
          throw new InvalidOperationException($"Unexpected frame # from merge buffer: {i}! ({merged.frame} {MergeBuffer[i].frame})");
        }
        for (int j = 0; j < offset; j++)
        {
          byte d = MergeBuffer[i].data[j];
          merged.data[(i * offset) + j] = d;
        }
      }

      // Write that data to disk!
      ReplayFile.AddInput(ref merged);

      // Add it to the active window of inputs (which are used for live playback)
      this.MergedInputs.Push(merged);
    }
  }

}
