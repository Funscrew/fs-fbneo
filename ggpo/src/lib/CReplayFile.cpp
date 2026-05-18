#include <string>

#include "CReplayFile.h"
#include <sstream>

namespace EZStream
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

  // ----------------------------------------------------------------------------------------------------
  void RawString(ostream& stream, const string& value)
  {
    stream.write(value.data(), static_cast<streamsize>(value.size()));
  }

  // ----------------------------------------------------------------------------------------------------
  template <typename T>
  void Read(istream& stream, T& value)
  {
    stream.read(reinterpret_cast<char*>(&value), sizeof(T));
  }

  // ----------------------------------------------------------------------------------------------------
  void ReadBytes(istream& stream, uint8_t* buffer, int count) {
    //if (stream.tellg() + count > stream.) {
    //  throw new std::runtime_error("can't read past end of stream!");
    //}
    stream.read(reinterpret_cast<char*>(buffer), count);
  }

  // ----------------------------------------------------------------------------------------------------
  uint8_t ReadUint8(istream& stream) {
    uint8_t res = 0;
    stream.read(reinterpret_cast<char*>(&res), 1);
    return res;
  }
}

namespace ReplayData
{
  const vector<uint8_t> Header = { 'f', 's', 'n', 'e', 'o', '-', 'r', 'f' };
  // const vector<uint8_t> Footer = { 'r', 'r', 'x', '-' };

  const int HEADER_STUB_SIZE = 8;
  // const int FOOTER_STUB_SIZE = 4;
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
int CReplayFile::TotalFrames() {
  return _CurFrame;
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

    // The footer is read so that we can verify that the data is complete / valid.
    ReadFooter();
    break;

  case REPLAY_FILE_MODE_WRITE:
    WriteHeader();

    break;
  default:
    throw runtime_error("invalid mode for replay file!");
  }
}

// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::ReadSegmentHeader(CSegmentHeader& header) {
  uint8_t type;
  uint32_t size;

  EZStream::Read(DataStream, type);
  EZStream::Read(DataStream, size);

  header.Type = (EDataSegmentType)type;
  header.Size = size;
}

// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::ReadFooter()
{
  // We will go to the end of the replay file first to check for the appropriate markers.
  auto oldPos = DataStream.tellg();

  // Seek to the end....
  DataStream.seekg(0, ios::end);
  auto fileSize = (uint64_t)DataStream.tellg();


  // Just back a few bytes to make sure that our footer is formed correctly....
  // File Size Check:
  DataStream.seekg(-(int64_t)(sizeof(uint64_t)), ios::end);
  uint64_t checkSize;
  EZStream::Read(DataStream, checkSize);

  if (checkSize != fileSize) { 
    throw new runtime_error("Invalid check size!");
  }

  // Now to the top of the header to read in the Footer segment + data.
  // TODO: Place with other data related constants:
  const int FOOTER_SIZE = 109;
  DataStream.seekg(-FOOTER_SIZE, ios::end);

  // Read Footer Segment:
  CSegmentHeader sh;
  ReadSegmentHeader(sh);

  // More parity checking....
  if (sh.Type != EDataSegmentType::Footer)
  {
    throw new runtime_error("Invalid footer marker!");
  }
  if (sh.Size != (fileSize - (FOOTER_SIZE - sizeof(uint8_t) + sizeof(uint32_t))))
  {
    throw new runtime_error("Invalid footer segment size!");
  }

  ////int offset = sizeof(uint64_t) + ReplayData::FOOTER_STUB_SIZE;
  ////DataStream.seekg(-offset, ios::end);

  ////auto nPos = DataStream.tellg();

  ////// Read the stub + total data size....
  ////uint8_t stub[ReplayData::FOOTER_STUB_SIZE];
  ////EZStream::ReadBytes(DataStream, stub, ReplayData::FOOTER_STUB_SIZE);

  //////if (memcmp(stub, ReplayData::Footer.data(), ReplayData::FOOTER_STUB_SIZE) != 0)
  //////{
  //////  throw runtime_error("Invalid footer for replay file!");
  //////}

  ////uint64_t totalSize;
  ////EZStream::Read(DataStream, totalSize);

  ////if (fileSize != totalSize) {
  ////  throw runtime_error("Incorrect size marker in replay file footer!");
  ////}

  ////// Now it's back to where we started so we can read inputs and whatever other events may be present.
  ////DataStream.seekg(oldPos, ios::beg);

  // NOW we can read in the rest of the footer data.....  
  // const int COMPLETE_MSG_LEN = 64;

  // stringstream ms(ios::in | ios::out | ios::binary);

  //ECompletionReason reason = (ECompletionReason)EZStream::ReadUint8(DataStream);
  //EErrorReason errReason = (EErrorReason)EZStream::ReadUint8(DataStream);


  ////EZStream::Write(ms, static_cast<uint8_t>(reason));
  ////EZStream::Write(ms, static_cast<uint8_t>(errReason));
  ////EZStream::Write(ms, frame);

  //CopyFixedString(message, COMPLETE_MSG_LEN, WriteBuffer, 0);
  //ms.write(reinterpret_cast<const char*>(WriteBuffer), COMPLETE_MSG_LEN);

  //EZStream::Write(ms, ReplayData::Footer);

  //WriteSegmentData(EDataSegmentType::Complete, ms);

  //int64_t finalSize = static_cast<int64_t>(DataStream.tellp()) + static_cast<int64_t>(sizeof(int64_t));

  //EZStream::Write(DataStream, finalSize);
}

// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::CompleteReplay(int frame, ECompletionReason reason, EErrorReason errReason, const std::string& message) {

  CheckComplete();

  // TODO: Some kind of check to make sure that the frame we are ending on is at or near the current input frame.
  // WRITE FOOTER

  stringstream ms(ios::in | ios::out | ios::binary);
  EZStream::Write(ms, frame);
  EZStream::Write(ms, static_cast<uint8_t>(reason));
  EZStream::Write(ms, static_cast<uint8_t>(errReason));

  const int COMPLETE_MSG_LEN = 64;
  CopyFixedString(message, COMPLETE_MSG_LEN, WriteBuffer, 0);
  ms.write(reinterpret_cast<const char*>(WriteBuffer), COMPLETE_MSG_LEN);


  int64_t finalSize = static_cast<int64_t>(DataStream.tellp()) + static_cast<int64_t>(sizeof(int64_t));

  EZStream::Write(ms, finalSize);

  WriteSegmentData(EDataSegmentType::Footer, ms);

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
// NOTE: This will probably choke on unicode stuff..  we do want it all to be utf8 tho.
// Maybe something like this:
// https://sourceforge.net/directory/internationalization-i18n/windows/
void CGameData::SetGameName(std::string name) {
  int len = name.length();
  if (len > CGameData::MAX_GAME_NAME_SIZE) {
    throw runtime_error("game name exceed 32 bytes!");
  }
  memset(GameName, 0, CGameData::MAX_GAME_NAME_SIZE);
  memcpy(GameName, name.data(), len);
}

// ------------------------------------------------------------------------------------------------------
// NOTE: This will probably choke on unicode stuff..  we do want it all to be utf8 tho.
void CGameData::SetVersion(std::string version) {
  int len = version.length();
  if (len > CGameData::MAX_VERSION_SIZE) {
    throw runtime_error("game name exceed 32 bytes!");
  }
  memset(GameVersion, 0, CGameData::MAX_VERSION_SIZE);
  memcpy(GameVersion, version.data(), len);
}


// ------------------------------------------------------------------------------------------------------
void CReplayFile::WriteSegmentData(EDataSegmentType segmentType, stringstream& data)
{
  CheckComplete();

  streampos start = DataStream.tellp();

  data.seekp(0, ios::end);
  uint32_t segmentSize = static_cast<uint32_t>(data.tellp());

  EZStream::Write(DataStream, static_cast<uint8_t>(segmentType));
  EZStream::Write(DataStream, static_cast<uint32_t>(segmentSize));

  data.seekg(0, ios::beg);
  DataStream << data.rdbuf();

  streampos end = DataStream.tellp();
  int64_t total = static_cast<int64_t>(end - start);
  int expected = segmentSize + sizeof(uint8_t) + sizeof(uint32_t);

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
  EZStream::Write(DataStream, ReplayData::Header);
  EZStream::Write(DataStream, vector<uint8_t> { 1 });

  WriteGameData();
  Flush();
}


// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::ReadHeader()
{
  const int SIZE = 8;
  uint8_t header[SIZE];
  EZStream::ReadBytes(DataStream, header, SIZE);

  if (memcmp(header, ReplayData::Header.data(), SIZE) != 0)
  {
    throw runtime_error("Invalid header for replay file!");
  }

  uint8_t version = EZStream::ReadUint8(DataStream);
  if (version != 1) {
    throw runtime_error("Unsupported file version!");
  }

  ReadGameData();
}

// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::ReadGameData() {

  // NOTE: We should put these in their own type!
  uint8_t typeVal;
  uint16_t dataSize;

  // auto start = DataStream.tellp();

  EZStream::Read(DataStream, typeVal);
  auto segType = (EDataSegmentType)typeVal;
  if (segType != EDataSegmentType::GameData) {
    throw runtime_error("Invalid segment type for GameData!");
  }

  EZStream::Read(DataStream, dataSize);
  if (dataSize != sizeof(CGameData)) {
    throw runtime_error("Invalid data size for GameData!");
  }

  EZStream::Read<CGameData>(DataStream, GameData);

  //auto end = DataStream.tellp();

  //auto total = (end - start);
  //if (total != 0) {
  //  
  //  GameData.PlayerCount = 1;
  //  // throw runtime_error("SPLAT!");
  //}

  //// FAKE:
  //GameData.PlayerCount = 1;
}

// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::WriteGameData() {
  CheckComplete();

  streampos start = DataStream.tellp();

  uint16_t segmentSize = sizeof(CGameData);

  EZStream::Write(DataStream, static_cast<uint8_t>(EDataSegmentType::GameData));
  EZStream::Write(DataStream, segmentSize);
  EZStream::Write<CGameData>(DataStream, GameData);


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

  EZStream::Write(DataStream, static_cast<uint8_t>(EDataSegmentType::ChatData));
  EZStream::Write(DataStream, static_cast<uint16_t>(segmentSize));
  EZStream::Write(DataStream, chat.FromPlayerIndex);
  EZStream::Write(DataStream, chat.ToPlayerIndex);
  EZStream::RawString(DataStream, chat.Message);

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

  // TODO: Some kind of check to make sure that we are writing the frame numbers sequentially!
  _CurFrame = input.frame;

  streampos start = DataStream.tellp();

  int inputSize = GameData.TotalInputSize;
  int segmentSize = inputSize + sizeof(int);

  EZStream::Write(DataStream, static_cast<uint8_t>(EDataSegmentType::InputData));
  EZStream::Write(DataStream, static_cast<uint16_t>(inputSize));
  EZStream::Write(DataStream, input.frame);

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
