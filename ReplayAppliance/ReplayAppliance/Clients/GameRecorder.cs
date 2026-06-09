using drewCo.Tools;
using drewCo.Tools.Logging;
using System.ComponentModel.DataAnnotations;
using System.Runtime.InteropServices.Marshalling;

namespace funscrew.Clients
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
    const int PLAYER_INPUT_BUFFER_SIZE = 0x70;
    const int MAX_PLAYERS = 2;

    /// <summary>
    /// Used to track what should be the expected starting frame on merge operations when a given buffer is empty!
    /// </summary>
    private int SyncedBaseFrame = 0;

    private EZQ<GameInput>[] PlayerBuffers = null!;
    private int[] BaseFrames = null!;

    private GameInput[] MergeBuffer = null;

    private Stream DataStream = null!;
    private GameData GameData = null!;

    private byte[] WriteBuffer = new byte[0x800];

    public string FilePath { get; private set; } = null!;

    public bool RecordingComplete { get; private set; } = false;
    public bool HasError { get { return ErrorReason != EErrorReason.None; } }
    public string? ErrorMessage { get; private set; } = null!;
    public EErrorReason ErrorReason { get; private set; }

    // -----------------------------------------------------------------------------------------------------------------------
    // NOTE: In production environments, game data should not be allowed to be overwritten!
    public GameRecorder(GameData gameData_, string dataDir_, UInt64 sessionId_, bool overwriteExisting = false)
    {
      GameData = gameData_;
      SessionId = sessionId_;
      DataDir = dataDir_;

      if (SessionId == SessionService.TEST_SESSION_ID)
      {
        Log.Debug($"Magic session id: {SessionService.TEST_SESSION_ID} was used, replay overwrite is enabled!");
        overwriteExisting = true;
      }

      // Let's create the file.  If it already exists, then we have a problem / invalid session ID!
      string path = Path.Combine(DataDir, SessionId + ".replay");
      if (File.Exists(path) && !overwriteExisting)
      {
        throw new InvalidOperationException($"Data file for session id: {SessionId} already exists!");
      }

      // TODO: This is where we will create the stream for where the file data will go.
      CreateStream(path);
      FilePath = path;

      BaseFrames = new int[MAX_PLAYERS];
      int len = BaseFrames.Length;
      for (int i = 0; i < len; i++)
      {
        BaseFrames[i] = -1;
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
      CloseStream();
    }

    // -----------------------------------------------------------------------------------------------------------------------
    private void CloseStream()
    {
      DataStream?.Dispose();
      DataStream = null;
    }

    // -----------------------------------------------------------------------------------------------------------------------
    public void Flush()
    {
      DataStream?.Flush();
    }

    // -----------------------------------------------------------------------------------------------------------------------
    private void CreateStream(string path)
    {
      DataStream = File.Open(path, FileMode.Create, FileAccess.Write);
      WriteHeader(DataStream);
    }

    // -----------------------------------------------------------------------------------------------------------------------
    private void WriteHeader(Stream res)
    {
      // Write the data header.
      // This indicates that it is a replay file, version 1
      EZWriter.Write(res, ReplayFile.Preamble);
      EZWriter.WriteBytes(res, new[] { (byte)1 });

      // Now write the game data segment:
      WriteGameDataSegment(this.GameData);

    }

    // -----------------------------------------------------------------------------------------------------------------------
    private void CheckComplete()
    {
      if (this.RecordingComplete) { throw new InvalidOperationException("Recording is complete!  Can't write anymore!"); }
    }

    // -----------------------------------------------------------------------------------------------------------------------
    private unsafe void WriteInputSegment(ref GameInput input)
    {
      CheckComplete();

      long start = this.DataStream.Position;
      int inputSize = this.GameData.TotalInputSize;
      int segmentSize = inputSize + sizeof(int);

      EZWriter.Write(DataStream, (byte)EDataSegmentType.InputData);
      EZWriter.Write(DataStream, (UInt16)inputSize);
      EZWriter.Write(DataStream, input.frame);

      for (int i = 0; i < inputSize; i++)
      {
        WriteBuffer[i] = input.data[i];
      }
      DataStream.Write(WriteBuffer, 0, inputSize);

      // Sanity Check!
      long end = DataStream.Position;
      long total = end - start;
      int expectedSize = segmentSize + 3;
      int expected = expectedSize;
      if (total != expected)
      {
        throw new InvalidOperationException($"Data size mismatch on write: {total} - {expected}!");
      }

      Flush();
    }

    // -----------------------------------------------------------------------------------------------------------------------
    private void WriteGameDataSegment(GameData gameData)
    {
      CheckComplete();

      // V1 Segments are of the form:
      // - Type (1B)
      // - DataSize (2B)
      // - Data (DataSize)
      long start = DataStream.Position;

      UInt16 segmentSize = GameData.DataSize;
      EZWriter.Write(DataStream, (byte)EDataSegmentType.GameData);
      EZWriter.Write(DataStream, segmentSize);

      // And now for the data.....
      {
        int size = CopyFixedString(gameData.GameName, GameData.MAX_GAME_NAME_SIZE, WriteBuffer, 0);
        DataStream.Write(WriteBuffer, 0, size);
      }

      {
        int size = CopyFixedString(GameData.GameVersion, GameData.MAX_VERSION_SIZE, WriteBuffer, 0);
        DataStream.Write(WriteBuffer, 0, size);
      }

      // EZWriter.Write(DataStream, gameData.GameVersion);
      EZWriter.Write(DataStream, gameData.PlayerCount);
      EZWriter.Write(DataStream, gameData.TotalInputSize);

      // Sanity Check!
      long end = DataStream.Position;
      long total = end - start;
      int expected = segmentSize + 3;
      if (total != expected)
      {
        throw new InvalidOperationException($"Data size mismatch on write: {total} - {expected}!");
      }

    }


    // -----------------------------------------------------------------------------------------------------------------------
    private void WriteSegmentData(EDataSegmentType segmentType, Stream fromStream)
    {
      CheckComplete();

      long start = this.DataStream.Position;
      int segmentSize = (int)fromStream.Position;

      EZWriter.Write(DataStream, (byte)segmentType);
      EZWriter.Write(DataStream, (UInt16)segmentSize);
      fromStream.Seek(0, SeekOrigin.Begin);
      fromStream.CopyTo(this.DataStream);

      // Sanity Check!
      long end = DataStream.Position;
      long total = end - start;
      int expected = segmentSize + 3;
      if (total != expected)
      {
        throw new InvalidOperationException($"Data size mismatch on write: {total} - {expected}!");
      }

      Flush();
    }

    // -----------------------------------------------------------------------------------------------------------------------
    // TODO: This belongs in tools somewhere.....
    private int CopyFixedString(string data, int maxSize, byte[] toBuffer, int offset)
    {
      if (data.Length > maxSize)
      {
        throw new InvalidOperationException();
      }
      int len = data.Length;
      int extra = maxSize - len;
      for (int i = 0; i < len; i++)
      {
        toBuffer[offset + i] = (byte)data[i];
      }
      for (int i = 0; i < extra; i++)
      {
        toBuffer[len + i] = 0;
      }

      return maxSize;
    }

    // -----------------------------------------------------------------------------------------------------------------------
    /// <summary>
    /// Use this to complete the recording of the replay + indicate the
    /// reason why it was completed.  This could be through a proper disconnect,
    /// or an error, etc.
    /// </summary>
    public void CompleteReplay(int frame, ECompletionReason reason, EErrorReason errReason, string? message)
    {
      CheckComplete();

      // TODO: Some kind of sanity check for the frame #?
      const int COMPLETE_MSG_LEN = 64;
      string useMsg = message == null ? string.Empty : StringTools.Truncate(message, COMPLETE_MSG_LEN);

      using (var ms = new MemoryStream(0x100))
      {
        // NOTE: In a perfect world we use our own write buffer.
        EZWriter.Write(ms, (byte)reason);
        EZWriter.Write(ms, (byte)errReason);
        EZWriter.Write(ms, frame);

        CopyFixedString(useMsg, COMPLETE_MSG_LEN, WriteBuffer, 0);
        ms.Write(WriteBuffer, 0, COMPLETE_MSG_LEN);

        // Write the end code so that we know that the file is actually completed correctly.
        EZWriter.Write(ms, ReplayFile.Footer);

        WriteSegmentData(EDataSegmentType.Complete, ms);

        // We will put the total file size at the end, as a kind of checksum, I guess...
        long finalSize = (int)(DataStream.Position + sizeof(long));
        EZWriter.Write(DataStream, finalSize);
      }

      CloseStream();

      RecordingComplete = true;
    }

    // -----------------------------------------------------------------------------------------------------------------------
    public void AddChatSegment(ChatData chat)
    {
      CheckComplete();

      if (string.IsNullOrWhiteSpace(chat.Message)) { return; }
      chat.Message = StringTools.Truncate(chat.Message, ChatData.CHAT_DATA_MAX);

      int segmentSize = chat.Message.Length + sizeof(int) + sizeof(int);

      long start = DataStream.Position;

      EZWriter.Write(DataStream, (byte)EDataSegmentType.ChatData);
      EZWriter.Write(DataStream, (UInt16)segmentSize);
      EZWriter.Write(DataStream, chat.FromPlayerIndex);
      EZWriter.Write(DataStream, chat.ToPlayerIndex);
      EZWriter.RawString(DataStream, chat.Message);

      long end = DataStream.Position;
      long total = end - start;
      int expected = segmentSize + 3;
      if (total != expected)
      {
        throw new InvalidOperationException($"Data size mismatch on write: {total} - {expected}!");
      }

      DataStream.Flush();
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

        this.OnError(EErrorReason.InputBufferFull, $"Too many unmerged inputs!: {playerIndex}");
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
      //if (buf.Size > 0)
      //{
      //  var f = buf.Front();
      //  if (f.frame == input.frame) { return false; }
      //  // startFrame = f.frame;
      //}
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
          //// if (this.PlayerBuffers[i].Back().frame != startMergeFrame)
          //{
          //  throw new InvalidOperationException($"Invalid (back) frame number at player index: {i}!  Should be {startMergeFrame}!");
          //}
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

      // throw new Exception();
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
      WriteInputSegment(ref merged);

      // Add it to the active window of inputs (which are used for live playback)
      this.MergedInputs.Push(merged);
    }
  }

  // ==============================================================================================================================
  public class ChatData
  {
    public const int CHAT_DATA_MAX = 128;

    public int FromPlayerIndex { get; set; }
    /// <summary>
    /// What frame was the message sent on?
    /// </summary>
    public int Frame { get; set; }
    public string Message { get; set; } = null!;

    /// <summary>
    /// Determines who we are sending the message to,
    /// use -1 for all players.
    /// </summary>
    public int ToPlayerIndex { get; private set; } = -1;
  }

  // ==============================================================================================================================
  public class GameData
  {
    public const int MAX_GAME_NAME_SIZE = 32;
    public const int MAX_VERSION_SIZE = 16;

    /// <summary>
    /// Name of the game.
    /// </summary>
    [Required]
    public string GameName { get; set; } = default!;

    /// <summary>
    /// This should be a bitfield (or 8 char string) or whatever to represent the version of
    /// a game (major, minor, revision, etc.).  Implementation defined!
    /// </summary>
    [Required]
    [MaxLength(MAX_VERSION_SIZE)]
    public string GameVersion { get; set; } = "<n/a>";

    /// <summary>
    /// How many people are playing.
    /// </summary>
    public int PlayerCount { get; set; }

    /// <summary>
    /// Size of inputs for all players.
    /// </summary>
    public int TotalInputSize { get; set; }

    public static ushort DataSize { get; } = GameData.MAX_GAME_NAME_SIZE + MAX_VERSION_SIZE + sizeof(int) + sizeof(int);
  }

  // ==============================================================================================================================
  public enum EErrorReason
  {
    None = 0,
    /// <summary>
    /// This happens when one or more input buffrers are full and we try to add another.
    /// The main reason for this happening is that one or more clients are disconnected / not sending packets.
    /// </summary>
    InputBufferFull
  }

  // ==============================================================================================================================
  public enum ECompletionReason
  {
    Invalid = 0,

    /// <summary>
    /// One or more players sent a proper disconnect signal.
    /// </summary>
    NormalDisconnect,

    /// <summary>
    /// Some other error.
    /// </summary>
    Error
  }

  // ==============================================================================================================================
  /// <summary>
  /// What types of data segments are we going to write into our replay files?
  /// </summary>
  public enum EDataSegmentType
  {
    Invalid = 0,
    GameData,
    InputData,
    ChatData,
    /// <summary>
    /// A recording session is completed.  See 'ECompletionReasons' for more information.
    /// </summary>
    Complete,
  }

}
