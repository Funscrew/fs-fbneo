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
  return _Footer.Frame;
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
void CReplayFile::WriteSegmentHeader(CSegmentHeader& header) {
  EZStream::Write(DataStream, (uint8_t)header.Type);
  EZStream::Write(DataStream, header.Size);
}

// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::ReadFooter()
{
  // We will go to the end of the replay file first to check for the appropriate markers.
  auto oldPos = DataStream.tellg();

  // Seek to the end....
  DataStream.seekg(0, ios::end);
  auto fileSize = (uint64_t)DataStream.tellg();


  // Read in the footer....
  int seekTo = -(int)(CFooterData::SizeOf() + CSegmentHeader::SizeOf());
  DataStream.seekg(seekTo, ios::end);
  CSegmentHeader sh;
  ReadSegmentHeader(sh);

  // More parity checking....
  if (sh.Type != EDataSegmentType::Footer)
  {
    throw new runtime_error("Invalid footer marker!");
  }

  if (sh.Size != CFooterData::SizeOf()) // (fileSize - (FOOTER_SIZE - sizeof(uint8_t) + sizeof(uint32_t))))
  {
    throw new runtime_error("Invalid footer segment size!");
  }

  _Footer.Read(DataStream);
  if (_Footer.FinalFileSize != fileSize) { 
    throw new runtime_error("Invalid check size!");
  }

  // Move back to where we started....
  DataStream.seekg(oldPos);

}

// ------------------------------------------------------------------------------------------------------------------------
void CFooterData::Read(istream& from) {
  EZStream::Read(from, Frame);
  EZStream::Read(from, CompleteReason);
  EZStream::Read(from, ErrorReason);

  EZStream::ReadBytes(from, reinterpret_cast<uint8_t*>(&Message), MSG_SIZE);

  EZStream::Read(from, FinalFileSize);
}

// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::CompleteReplayFile(int frame, ECompletionReason reason, EErrorReason errReason, const std::string& message) {

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

  auto footerSize = (uint64_t)ms.tellp();
  int64_t finalSize = static_cast<int64_t>((uint64_t)DataStream.tellp() + footerSize + sizeof(uint64_t) + CSegmentHeader::SizeOf());

  EZStream::Write(ms, finalSize);

  WriteSegmentData(EDataSegmentType::Footer, ms);

  auto curSize = DataStream.tellp();
  if (curSize != finalSize) {
    throw runtime_error("final size computation is incorrect!");
  }

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
void CFooterData::GetMessage(std::string& msg) {
  msg.clear();
  msg.append(Message);
}

// ------------------------------------------------------------------------------------------------------
void CFooterData::SetMessage(std::string msg) {
  int len = msg.length();
  if (len >= CFooterData::MSG_SIZE) {
    throw runtime_error("message exceeds max size!");
  }
  memset(Message, 0, CGameData::MAX_GAME_NAME_SIZE);
  memcpy(Message, msg.data(), len);
}

// ------------------------------------------------------------------------------------------------------
// NOTE: This will probably choke on unicode stuff..  we do want it all to be utf8 tho.
// Maybe something like this:
// https://sourceforge.net/directory/internationalization-i18n/windows/
void CGameData::SetGameName(std::string name) {
  int len = name.length();
  if (len >= CGameData::MAX_GAME_NAME_SIZE) {
    throw runtime_error("game name exceed max size!");
  }
  memset(GameName, 0, CGameData::MAX_GAME_NAME_SIZE);
  memcpy(GameName, name.data(), len);
}

// ------------------------------------------------------------------------------------------------------
// NOTE: This will probably choke on unicode stuff..  we do want it all to be utf8 tho.
void CGameData::SetVersion(std::string version) {
  int len = version.length();
  if (len >= CGameData::MAX_VERSION_SIZE) {
    throw runtime_error("version exceeds max size!");
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

  CSegmentHeader segHeader;
  segHeader.Type = segmentType;
  segHeader.Size = static_cast<uint32_t>(data.tellp());
  WriteSegmentHeader(segHeader);

  data.seekg(0, ios::beg);
  DataStream << data.rdbuf();

  streampos end = DataStream.tellp();
  int64_t total = static_cast<int64_t>(end - start);
  int expected = segHeader.Size + sizeof(uint8_t) + sizeof(uint32_t);

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
  CSegmentHeader segHeader;
  ReadSegmentHeader(segHeader);

  if (segHeader.Type != EDataSegmentType::GameData) {
    throw runtime_error("Invalid segment type for GameData!");
  }

  if (segHeader.Size != sizeof(CGameData)) {
    throw runtime_error("Invalid data size for GameData!");
  }

  EZStream::Read<CGameData>(DataStream, GameData);
}

// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::WriteGameData() {
  CheckComplete();

  streampos start = DataStream.tellp();


  CSegmentHeader segHeader;
  segHeader.Type = EDataSegmentType::GameData;
  segHeader.Size = sizeof(CGameData);

  WriteSegmentHeader(segHeader);
  EZStream::Write<CGameData>(DataStream, GameData);

  streampos end = DataStream.tellp();
  int64_t total = static_cast<int64_t>(end - start);
  int expected = segHeader.Size + CSegmentHeader::SizeOf();

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

  streampos start = DataStream.tellp();

  CSegmentHeader segHeader;
  segHeader.Type = EDataSegmentType::ChatData;
  segHeader.Size = chat.SizeOf();
  // NOTE: Capturing this chat data doesn't capture who said what... the player indexes should be marked in the CGameData part of the file.

  WriteSegmentHeader(segHeader);

  EZStream::Write(DataStream, chat.FromPlayerIndex);
  EZStream::Write(DataStream, chat.ToPlayerIndex);
  EZStream::RawString(DataStream, chat.Message);

  streampos end = DataStream.tellp();
  int64_t total = static_cast<int64_t>(end - start);
  int expected = segHeader.Size + segHeader.SizeOf();

  if (total != expected)
  {
    throw runtime_error("Data size mismatch on write!");
  }

  DataStream.flush();
}

// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::AddInputSegment(const GameInput& input) {
  CheckComplete();

  // TODO: Some kind of check to make sure that we are writing the frame numbers sequentially!
  _Footer.Frame = input.frame;

  streampos start = DataStream.tellp();

  int inputSize = GameData.TotalInputSize;

  CSegmentHeader segHeader;
  segHeader.Type = EDataSegmentType::InputData;
  segHeader.Size = inputSize + sizeof(int);   // all inputs + frame #
  WriteSegmentHeader(segHeader);

  streampos x = DataStream.tellp();

  EZStream::Write(DataStream, input.frame);
  // TODO: memcpy
  for (int i = 0; i < inputSize; i++)
  {
    WriteBuffer[i] = input.bits[i];
  }

  DataStream.write(reinterpret_cast<const char*>(WriteBuffer), inputSize);

  streampos end = DataStream.tellp();
  int64_t total = static_cast<int64_t>(end - start);
  int expected = segHeader.Size + CSegmentHeader::SizeOf();

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
