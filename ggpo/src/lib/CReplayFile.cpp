#include <string>

#include "CReplayFile.h"
#include <sstream>

namespace EZWriter
{
  template <typename T>
  void Write(ostream& stream, const T& value)
  {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
  }

  void Write(ostream& stream, const uint8_t data)
  {
    stream.write(reinterpret_cast<const char*>(&data), static_cast<streamsize>(1));
  }

  void Write(ostream& stream, const vector<uint8_t>& bytes)
  {
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<streamsize>(bytes.size()));
  }

  void RawString(ostream& stream, const string& value)
  {
    stream.write(value.data(), static_cast<streamsize>(value.size()));
  }
}

namespace ReplayData
{
  const vector<uint8_t> Preamble = { 'f', 's', 'n', 'e', 'o', '-', 'r', 'f' };
  extern const vector<uint8_t> Footer = { 'r', 'r', 'x', '-' };
}

namespace StringTools
{
  string Truncate(const string& value, size_t maxLen)
  {
    if (value.size() <= maxLen)
    {
      return value;
    }

    return value.substr(0, maxLen);
  }
}


// ------------------------------------------------------------------------------------------------------------------------
CReplayFile::CReplayFile(const std::filesystem::path& path) {
  Init(path, REPLAY_FILE_MODE_READ);
}

// ------------------------------------------------------------------------------------------------------------------------
CReplayFile::CReplayFile(const std::filesystem::path& path, const CGameData& gameData_)
  : GameData(gameData_)
{
  Init(path, REPLAY_FILE_MODE_WRITE);
}

// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::Init(const filesystem::path& path, EReplayFileMode mode_) {
  // TODO: Check to see if the file exists....

  _Mode = mode_;

  auto openMode = ios::binary;
  switch (_Mode) {
  case REPLAY_FILE_MODE_READ:
    openMode |= ios::in;
    break;

  case REPLAY_FILE_MODE_WRITE:
    openMode |= (ios::out | ios::trunc);
    break;

  default:
    throw runtime_error("invalid mode for replay file!");
  }

  DataStream.open(path, openMode);
  if (!DataStream.is_open()) {
    throw runtime_error("Unable to create replay file stream.");
  }

  switch (_Mode) {
  case REPLAY_FILE_MODE_READ:
    ReadHeader();
    break;

  case REPLAY_FILE_MODE_WRITE:
    WriteHeader();

    break;
  default:
    throw runtime_error("invalid mode for replay file!");
  }
}


// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::CompleteReplay(int frame, ECompletionReason reason, EErrorReason errReason, const std::string& message) {

  CheckComplete();

  const int COMPLETE_MSG_LEN = 64;

  stringstream ms(ios::in | ios::out | ios::binary);

  EZWriter::Write(ms, static_cast<uint8_t>(reason));
  EZWriter::Write(ms, static_cast<uint8_t>(errReason));
  EZWriter::Write(ms, frame);

  CopyFixedString(message, COMPLETE_MSG_LEN, WriteBuffer, 0);
  ms.write(reinterpret_cast<const char*>(WriteBuffer), COMPLETE_MSG_LEN);

  EZWriter::Write(ms, ReplayData::Footer);

  WriteSegmentData(EDataSegmentType::Complete, ms);

  int64_t finalSize = static_cast<int64_t>(DataStream.tellp()) + static_cast<int64_t>(sizeof(int64_t));

  EZWriter::Write(DataStream, finalSize);

  CloseStream();
}

// --------------------------------------------------------------------------------------------------
int CReplayFile::CopyFixedString(const string& data, int maxSize, uint8_t* toBuffer, int offset)
{
  if (static_cast<int>(data.size()) > maxSize)
  {
    throw runtime_error("Fixed string exceeds maximum size.");
  }

  int len = static_cast<int>(data.size());
  int extra = (std::max)(maxSize - len, 0);

  for (int i = 0; i < len; i++)
  {
    toBuffer[offset + i] = static_cast<uint8_t>(data[i]);
  }

  for (int i = 0; i < extra; i++)
  {
    toBuffer[offset + len + i] = 0;
  }

  return maxSize;
}


// ------------------------------------------------------------------------------------------------------
void CReplayFile::WriteSegmentData(EDataSegmentType segmentType, stringstream& data)
{
  CheckComplete();

  streampos start = DataStream.tellp();

  data.seekp(0, ios::end);
  int segmentSize = static_cast<int>(data.tellp());

  EZWriter::Write(DataStream, static_cast<uint8_t>(segmentType));
  EZWriter::Write(DataStream, static_cast<uint16_t>(segmentSize));

  data.seekg(0, ios::beg);
  DataStream << data.rdbuf();

  streampos end = DataStream.tellp();
  int64_t total = static_cast<int64_t>(end - start);
  int expected = segmentSize + 3;

  if (total != expected)
  {
    throw runtime_error("Data size mismatch on write!");
  }

  Flush();
}



// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::CloseStream()
{
  DataStream.close();
  _Mode = REPLAY_FILE_MODE_COMPLETE;
}

// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::WriteHeader()
{
  EZWriter::Write(DataStream, ReplayData::Preamble);
  EZWriter::Write(DataStream, vector<uint8_t> { 1 });

  WriteGameData();
}

// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::WriteGameData() {
  CheckComplete();

  streampos start = DataStream.tellp();

  uint16_t segmentSize = CGameData::DataSize;

  EZWriter::Write(DataStream, static_cast<uint8_t>(EDataSegmentType::GameData));
  EZWriter::Write(DataStream, segmentSize);

  {
    int size = CopyFixedString(GameData.GameName, CGameData::MAX_GAME_NAME_SIZE, WriteBuffer, 0);
    DataStream.write(reinterpret_cast<const char*>(WriteBuffer), size);
  }

  {
    int size = CopyFixedString(GameData.GameVersion, CGameData::MAX_VERSION_SIZE, WriteBuffer, 0);
    DataStream.write(reinterpret_cast<const char*>(WriteBuffer), size);
  }

  EZWriter::Write(DataStream, GameData.PlayerCount);
  EZWriter::Write(DataStream, GameData.TotalInputSize);

  streampos end = DataStream.tellp();
  int64_t total = static_cast<int64_t>(end - start);
  int expected = segmentSize + 3;

  if (total != expected)
  {
    throw runtime_error("Data size mismatch on write!");
  }
}

// ----------------------------------------------------------------------------------------------------------
void CReplayFile::AddChatSegment(ChatData& chat)
{
  CheckComplete();

  if (chat.Message.empty())
  {
    return;
  }

  chat.Message = StringTools::Truncate(chat.Message, ChatData::CHAT_DATA_MAX);

  int segmentSize = static_cast<int>(chat.Message.size()) + sizeof(int) + sizeof(int);

  streampos start = DataStream.tellp();

  EZWriter::Write(DataStream, static_cast<uint8_t>(EDataSegmentType::ChatData));
  EZWriter::Write(DataStream, static_cast<uint16_t>(segmentSize));
  EZWriter::Write(DataStream, chat.FromPlayerIndex);
  EZWriter::Write(DataStream, chat.ToPlayerIndex);
  EZWriter::RawString(DataStream, chat.Message);

  streampos end = DataStream.tellp();
  int64_t total = static_cast<int64_t>(end - start);
  int expected = segmentSize + 3;

  if (total != expected)
  {
    throw runtime_error("Data size mismatch on write!");
  }

  DataStream.flush();
}

// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::WriteInputSegment(const GameInput& input) {
  CheckComplete();
  streampos start = DataStream.tellp();

  int inputSize = GameData.TotalInputSize;
  int segmentSize = inputSize + sizeof(int);

  EZWriter::Write(DataStream, static_cast<uint8_t>(EDataSegmentType::InputData));
  EZWriter::Write(DataStream, static_cast<uint16_t>(inputSize));
  EZWriter::Write(DataStream, input.frame);

  for (int i = 0; i < inputSize; i++)
  {
    WriteBuffer[i] = input.bits[i];
  }

  DataStream.write(reinterpret_cast<const char*>(WriteBuffer), inputSize);

  streampos end = DataStream.tellp();
  int64_t total = static_cast<int64_t>(end - start);
  int expected = segmentSize + 3;

  if (total != expected)
  {
    throw runtime_error("Data size mismatch on write!");
  }

  Flush();
}


// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::ReadHeader()
{
  throw runtime_error("not implemented!");
}

// ----------------------------------------------------------------------------------------------------------
void CReplayFile::Flush()
{
  if (DataStream.is_open())
  {
    DataStream.flush();
  }
}

// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::CheckComplete() {
  if (_Mode == REPLAY_FILE_MODE_COMPLETE || _Mode == REPLAY_FILE_MODE_READ) {
    throw runtime_error("Invalid replay file mode:");
  }
}
