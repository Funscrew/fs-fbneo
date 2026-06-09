using drewCo.Tools;
using System.Collections.Generic;
using System.Text;
using System.Xml.Serialization;

namespace funscrew.Clients
{
  // ==============================================================================================================================
  /// <summary>
  /// Represents a replay file
  /// </summary>
  public class ReplayFile
  {
    private Stream DataStream = null!;

    public int Version { get; private set; }
    public GameData GameData { get; private set; }

    private byte[] ReadBuffer = new byte[256];

    public static readonly char[] Preamble = { 'f', 's', 'n', 'e', 'o', '-', 'r', 'f' };
    public static readonly char[] Footer = { 'r', 'r', 'x', '-' };

    private long DataStart = 0;
    private long DataEnd = 0; 

    // ------------------------------------------------------------------------------------------------------------
    public ReplayFile(string path)
    {
      if (!File.Exists(path))
      {
        throw new FileNotFoundException($"There is no file at path: {path}!");
      }

      DataStream = File.OpenRead(path);

      // Validate file contents....
      // Footer is read first to reduce amount of seeks required.
      ReadFooter();
      ReadHeader();

      // Now we can read in the different data segments...
      // First one should be the game data.
      ReadGameData();
      DataStart = DataStream.Position;

      // From here we will read the inputs as an IEnumerable or whatever....
    }

    // ------------------------------------------------------------------------------------------------------------
    private void ReadGameData()
    {
      // Make sure that the segment is correct.
      byte b = EZReader.ReadByte(DataStream);
      var sType = (EDataSegmentType)b;
      if (sType != EDataSegmentType.GameData)
      {
        throw new InvalidOperationException($"Should have game data segment marker but got: {b} instead!");
      }

      // This is our segment size....
      UInt16 size = EZReader.ReadUInt16(DataStream);
      if (size != GameData.DataSize)
      {
        throw new InvalidOperationException("Invalid segment size for game data!");
      }

      //GameData = new GameData();
      DataStream.Read(ReadBuffer, 0, GameData.MAX_GAME_NAME_SIZE);
      string name = ReadFixedString(ReadBuffer, 0, GameData.MAX_GAME_NAME_SIZE);

      DataStream.Read(ReadBuffer, 0, GameData.MAX_VERSION_SIZE);
      string version = ReadFixedString(ReadBuffer, 0, GameData.MAX_VERSION_SIZE);

      int playerCount = EZReader.ReadInt32(DataStream);
      int inputSize = EZReader.ReadInt32(DataStream);

      GameData = new GameData()
      {
        GameName = name,
        GameVersion = version,
        PlayerCount = playerCount,
        TotalInputSize = inputSize
      };
    }

    // ------------------------------------------------------------------------------------------------------------
    private string ReadFixedString(byte[] readBuffer, int offset, int bufferSize)
    {
      int useLen = 0;
      for (int i = 0; i < bufferSize; i++)
      {
        if (readBuffer[i] == 0)
        {
          break;
        }
        useLen++;
      }

      string res = Encoding.UTF8.GetString(readBuffer, 0, useLen);
      return res;
    }

    // ------------------------------------------------------------------------------------------------------------
    private void ReadHeader()
    {
      // Let's make sure that the header is correct.
      DataStream.Seek(0, SeekOrigin.Begin);
      DataStream.Read(ReadBuffer, 0, Preamble.Length);
      bool preambleMatch = AreSame(ReadBuffer, Preamble, Preamble.Length);
      if (!preambleMatch)
      {
        throw new InvalidOperationException("Invalid header data in ReplayFile!");
      }

      // Now get the version....
      this.Version = EZReader.ReadByte(DataStream);
      if (this.Version != 1)
      {
        throw new InvalidOperationException($"Unsupported file version: {this.Version}!");
      }
    }

    // ------------------------------------------------------------------------------------------------------------
    private void ReadFooter()
    {
      // Then we can make sure that the file size is correct + correctly terminated...
      long expectedSize = DataStream.Length;

      DataEnd = DataStream.Length - (ReplayFile.Footer.Length + sizeof(long));
      DataStream.Seek(DataEnd, SeekOrigin.Begin);

      DataStream.Read(ReadBuffer, 0, Footer.Length);
      bool footerMatch = AreSame(ReadBuffer, Footer, Footer.Length);
      if (!footerMatch)
      {
        throw new InvalidOperationException("Invalid footer data!");
      }

      long size = EZReader.ReadInt64(DataStream);
      if (size != expectedSize)
      {
        throw new InvalidOperationException($"The expected size is incorrect or the file was not properly closed!");
      }
    }

    // ------------------------------------------------------------------------------------------------------------
    private bool AreSame(byte[] readBuffer, char[] toMatch, int count)
    {
      for (int i = 0; i < count; i++)
      {
        if ((char)readBuffer[i] != toMatch[i])
        {
          return false;
        }
      }
      return true;
    }

    // ------------------------------------------------------------------------------------------------------------
    // TEMP: For now I am going to expose the inputs as a list, but keep the IEnumberable interface in
    // place so that we can have an actual stream type approach later on...
    // NOTE: Streaming the segments later on will be useful too, I think....
    public unsafe IEnumerable<GameInput> GetInputs()
    {
      var res = new List<GameInput>();

      // IEnumerable < GameInput >
      DataStream.Position = DataStart;


      while (DataStream.Position < DataEnd)
      {
        // Now let's read the segments, one by one.
        long start = DataStream.Position;

        EDataSegmentType sType = ReadSegmentType();
        UInt16 size = EZReader.ReadUInt16(DataStream);
        long end = start + size;

        var segment = new DataSegment()
        {
          SegmentType = sType,
          StartPos = start,
          EndPos = end,
        };

        if (segment.SegmentType == EDataSegmentType.InputData)
        {
          // We will hydrate a 'GameInput' instance from the data.....
          var gi = new GameInput();
          gi.frame = EZReader.ReadInt32(DataStream);
          int useInputSize = size; // - sizeof(int);

          gi.size = useInputSize;

          for (int i = 0; i < useInputSize; i++)
          {
            gi.data[i] = EZReader.ReadByte(DataStream);
          }

          res.Add(gi);
        }
        else {
          // I think we need to skip ahead here...
          DataStream.Seek(size, SeekOrigin.Current);
        }
      }

      return res;
    }

    // ------------------------------------------------------------------------------------------------------------
    private EDataSegmentType ReadSegmentType()
    {
      byte b = EZReader.ReadByte(DataStream);
      var res = (EDataSegmentType)b;
      return res;
    }

  }

  // ================================================================================================================
  /// <summary>
  /// Represents a segment of data in a stream.
  /// This minimal version is used to help reduce the amount of memory needed when
  /// scanning through the replay files.
  /// </summary>
  public struct DataSegment
  {
    public EDataSegmentType SegmentType { get; set; }

    /// <summary>
    /// Where does the the data begin? (includes segment type!)
    /// </summary>
    public long StartPos { get; set; }
    public long EndPos { get; set; }

    public long Size { get { return EndPos - StartPos; } }
  }

}
