#include "CReplayFile.h"

#include <string>
#include <sstream>
#include <vector>

#define WDATA(x) EZStream::Write(_Stream, x)
#define RDATA(x) EZStream::Read(_Stream, x);

#define WDATA2(stream, x) EZStream::Write(stream, x)
#define RDATA2(stream, x) EZStream::Read(stream, x);

using namespace std;


// PATCH: Workaround for no memcpy_s on linux....
// TODO: There should be a real macro like '__STDC_LIB_EXT1__' to properly identify the feautre....
#ifdef _WIN32
#define MEMCPY(dest, destSize, src, srcSize) memcpy_s(dest, destSize, src, srcSize)
#else
  // Linux, probably
#define MEMCPY(dest, destSize, src, srcSize) memcpy(dest, src, srcSize)
#endif

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
  void WriteRawString(ostream& stream, const string& value)
  {
    stream.write(value.data(), static_cast<streamsize>(value.size()));
  }

  // ----------------------------------------------------------------------------------------------------
  void ReadRawString(istream& stream, string& value, size_t size)
  {
    char* buffer = new char[size + 1];
    stream.read(buffer, size);
    buffer[size] = 0;

    value.assign(buffer);
    delete[](buffer);
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
  const vector<uint8_t> FileId = { 'f', 's', 'n', 'e', 'o', '-', 'r', 'f' };
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
// Open the replay file in read mode.
CReplayFile::CReplayFile(const std::filesystem::path& path) {
  Init(path, EReplayFileMode::REPLAY_FILE_MODE_READ);
}

// ------------------------------------------------------------------------------------------------------------------------
CReplayFile::CReplayFile(const std::filesystem::path& path, const CGameData& gameData_, const CGameState* state_)
  : _GameData(gameData_)
{
  if (state_) {
    _State = *state_;
  }
  Init(path, EReplayFileMode::REPLAY_FILE_MODE_WRITE);
}

// ------------------------------------------------------------------------------------------------------------------------
CReplayFile::~CReplayFile() { 
  CloseStream();
}

// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::GetState(CGameState& state) { 
  state = _State;
}

// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::Init(const filesystem::path& path, EReplayFileMode mode_) {
  // TODO: Check to see if the file exists....

  _Mode = mode_;

  auto openMode = ios::binary;
  switch (_Mode) {
  case EReplayFileMode::REPLAY_FILE_MODE_READ:
    openMode |= ios::in;
    break;

  case EReplayFileMode::REPLAY_FILE_MODE_WRITE:
    openMode |= (ios::out | ios::trunc);

    SetupInputDataBuffer();

    CurInputGroupCount = 0;
    InputStartFrame = 0;

    break;

  default:
    throw runtime_error("invalid mode for replay file!");
  }

  _Stream.open(path, openMode);
  if (!_Stream.is_open()) {
    throw runtime_error("Unable to create replay file stream.");
  }

  switch (_Mode) {
  case EReplayFileMode::REPLAY_FILE_MODE_READ:
    ReadHeader();
    ReadState();

    // The footer is read so that we can verify that the data is complete / valid.
    ReadFooter();
    break;

  case EReplayFileMode::REPLAY_FILE_MODE_WRITE:
    WriteHeader();
    WriteState();

    break;
  default:
    throw runtime_error("invalid mode for replay file!");
  }
}

// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::SetupInputDataBuffer()
{
  InputGroupBufSize = MAX_INPUT_GROUP_COUNT * _GameData.TotalInputSize;
  InputGroupBuffer = (uint8_t*)malloc(InputGroupBufSize);
  if (InputGroupBuffer == nullptr)
  {
    throw new runtime_error("Could not allocate space for input group buffer!");
  }
  memset(InputGroupBuffer, 0, InputGroupBufSize);
}

// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::WriteState() {

  CSegmentHeader segHeader;
  segHeader.Type = EDataSegmentType::GameState;
  segHeader.Size = _State.SizeOf();

  auto start = _Stream.tellg();

  WriteSegmentHeader(segHeader);
  _State.Write(_Stream);

  // Parity check....
  auto end = _Stream.tellg();
  auto total = end - start;
  auto expected = segHeader.Size + CSegmentHeader::SizeOf();
  if (total != expected)
  {
    throw runtime_error("Data size mismatch on write!");
  }
}

// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::ReadState() {
  CSegmentHeader header;
  ReadSegmentHeader(header);

  _State.Read(_Stream);
}

// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::ReadSegmentHeader(CSegmentHeader& header) {
  uint8_t type;
  uint32_t size;

  EZStream::Read(_Stream, type);
  EZStream::Read(_Stream, size);

  header.Type = (EDataSegmentType)type;
  header.Size = size;
}

// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::PeekSegmentHeader(CSegmentHeader& header) {
  auto pos = _Stream.tellg();
  ReadSegmentHeader(header);
  _Stream.seekg(pos, ios::beg);
}

// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::WriteSegmentHeader(CSegmentHeader& header) {
  EZStream::Write(_Stream, (uint8_t)header.Type);
  EZStream::Write(_Stream, header.Size);
}

// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::ReadFooter()
{
  // We will go to the end of the replay file first to check for the appropriate markers.
  auto oldPos = _Stream.tellg();

  // Seek to the end....
  _Stream.seekg(0, ios::end);
  auto fileSize = (uint64_t)_Stream.tellg();


  // Read in the footer....
  int seekTo = -(int)(CFooterData::SizeOf() + CSegmentHeader::SizeOf());
  _Stream.seekg(seekTo, ios::end);
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

  _Footer.Read(_Stream);
  if (_Footer.FinalFileSize != fileSize) {
    throw new runtime_error("Invalid check size!");
  }

  // Move back to where we started....
  _Stream.seekg(oldPos);

}

// ------------------------------------------------------------------------------------------------------------------------
bool CReplayFile::GetNextInput(GameInput& input) {

  // Check to see if we are currently within an input group....
  // NOTE: With this approach we can't really get chat replay data, but that is OK for now.....
  if (CurInputGroupCount > 0) {
    ReadInputFromBuffer(input);
    return true;
  }

  // NOTE: We are collecting segments until we hit the next input segment.
  auto cPos = _Stream.tellg();
  scratch = cPos;

  CSegmentHeader segHeader;
  ReadSegmentHeader(segHeader);

  while (true) {
    switch (segHeader.Type)
    {
    case EDataSegmentType::InputData:
    {
      // Here we will read in the next block of inputs...
      // Frame info.
      RDATA(InputStartFrame);
      RDATA(CurInputGroupCount);

      auto expectedFrame = LastReadFrame + 1;
      if (expectedFrame != InputStartFrame) { 
        throw new runtime_error("Unexpected input frame was encountered!");
      }

      auto readSize = CurInputGroupCount * _GameData.TotalInputSize;
      if (readSize != segHeader.Size - (sizeof(uint32_t) + sizeof(uint16_t)))
      {
        throw new runtime_error("Invalid data size in InputData segment!");
      }

      _Stream.read(reinterpret_cast<char*>(InputGroupBuffer), readSize);
      InputGroupReadIndex = 0;

      ReadInputFromBuffer(input);
      return true;
    }
    break;

    case EDataSegmentType::ChatData:
      // TODO: We can capture this data some other time....
      // Move ahead....
      _Stream.seekg(segHeader.Size, ios::cur);
      break;

    case EDataSegmentType::Footer:
      // There are no more inputs to be had!
      return false;

    default:
      throw new runtime_error("invalid segment type!");
    }
  }
}

// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::ReadInputFromBuffer(GameInput& input)
{
  input.frame = InputStartFrame + InputGroupReadIndex;
  auto readPos = InputGroupReadIndex * _GameData.TotalInputSize;
  MEMCPY(input.bits, GameInput::DATA_SIZE, InputGroupBuffer + readPos, _GameData.TotalInputSize);

  ++InputGroupReadIndex;
  if (InputGroupReadIndex >= CurInputGroupCount) {
    CurInputGroupCount = 0;
  }

  LastReadFrame = input.frame;
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

  // Write any pending inputs.
  FlushPendingInputData();


  // TODO: Some kind of check to make sure that the frame we are ending on is at or near the current input frame.
  // WRITE FOOTER
  stringstream ms(ios::in | ios::out | ios::binary);
  EZStream::Write(ms, frame);
  EZStream::Write(ms, static_cast<uint8_t>(reason));
  EZStream::Write(ms, static_cast<uint8_t>(errReason));

  const int COMPLETE_MSG_LEN = 64;
  CopyFixedString(message, COMPLETE_MSG_LEN, DataBuffer, 0);
  ms.write(reinterpret_cast<const char*>(DataBuffer), COMPLETE_MSG_LEN);

  auto footerSize = (uint64_t)ms.tellp();
  int64_t finalSize = static_cast<int64_t>((uint64_t)_Stream.tellp() + footerSize + sizeof(uint64_t) + CSegmentHeader::SizeOf());

  EZStream::Write(ms, finalSize);

  WriteSegmentData(EDataSegmentType::Footer, ms);

  auto curSize = _Stream.tellp();
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

  streampos start = _Stream.tellp();

  data.seekp(0, ios::end);

  CSegmentHeader segHeader;
  segHeader.Type = segmentType;
  segHeader.Size = static_cast<uint32_t>(data.tellp());
  WriteSegmentHeader(segHeader);

  data.seekg(0, ios::beg);
  _Stream << data.rdbuf();

  streampos end = _Stream.tellp();
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
  _Stream.close();
  _Mode = EReplayFileMode::REPLAY_FILE_MODE_COMPLETE;
}

// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::WriteHeader()
{
  EZStream::Write(_Stream, ReplayData::FileId);
  EZStream::Write(_Stream, vector<uint8_t> { 1 });

  WriteGameData();

  Flush();
}


// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::ReadHeader()
{
  const int SIZE = 8;
  uint8_t fileId[SIZE];
  EZStream::ReadBytes(_Stream, fileId, SIZE);

  if (memcmp(fileId, ReplayData::FileId.data(), SIZE) != 0)
  {
    throw runtime_error("Invalid file id for replay file!");
  }

  uint8_t version = EZStream::ReadUint8(_Stream);
  if (version != 1) {
    throw runtime_error("Unsupported file version!");
  }

  ReadGameData();

  SetupInputDataBuffer();
}

// ------------------------------------------------------------------------------------------------------------------------
void CGameData::SetPlayerName(std::string name, uint8_t index)
{
  if (name.length() <= 0) { throw new runtime_error("Name must be at least one character!"); }
  if (name.length() > CGameData::MAX_PLAYER_NAME) { throw new runtime_error("Max name length (32) exceeded!"); }
  if (index >= MaxPlayerCount) { throw new runtime_error("Invalid player index!"); }

  size_t size = name.size();
  if (size > MAX_PLAYER_NAME - 1) { size = MAX_PLAYER_NAME - 1; }
  int offset = index * MAX_PLAYER_NAME;

  memcpy(PlayerNames + offset, name.data(), size);
  *(PlayerNames + offset + size) = 0;
}

// ------------------------------------------------------------------------------------------------------------------------
bool CGameData::TryGetPlayerName(uint8_t index, std::string& to)
{
  if (!PlayerNames) { return false; }

  int offset = index * MAX_PLAYER_NAME;
  int len = strlen(PlayerNames + offset);
  to.assign(PlayerNames + offset, len + 1);

  return true;
}


// ------------------------------------------------------------------------------------------------------------------------
uint32_t CGameData::SizeOf() {
  // Game + Version strings.
  uint32_t res = (MAX_GAME_NAME_SIZE + MAX_VERSION_SIZE);
  res += sizeof(uint16_t) * 2;    // Player count, input size.

  // Player names.....
  res += sizeof(PlayerNames);

  //if (PlayerNames) {
  //  for (size_t i = 0; i < MaxPlayerCount; i++)
  //  {
  //    res += (PlayerNames[i].size());
  //  }
  //}

  return res;
}

// ------------------------------------------------------------------------------------------------------------------------
CGameData::CGameData() {
  // memset(PlayerNames, 0, sizeof(PlayerNames));
}


// ------------------------------------------------------------------------------------------------------------------------
void CGameData::Read(istream& from) {

  RDATA2(from, GameName);
  RDATA2(from, GameVersion);
  RDATA2(from, MaxPlayerCount);
  RDATA2(from, TotalInputSize);
  RDATA2(from, PlayerNames);
  //for (uint8_t i = 0; i < MaxPlayerCount; i++)
  //{
  //  uint8_t nameSize;
  //  RDATA2(from, nameSize);
  //  if (nameSize > 0) {
  //    std::string nameBuffer;
  //    EZStream::ReadRawString(from, PlayerNames[i], nameSize);
  //    SetPlayerName(nameBuffer, i);
  //  }
  //}

}


// ------------------------------------------------------------------------------------------------------------------------
void CGameData::Write(ostream& to) {

  WDATA2(to, GameName);
  WDATA2(to, GameVersion);
  WDATA2(to, MaxPlayerCount);
  WDATA2(to, TotalInputSize);
  WDATA2(to, PlayerNames);

  //if (PlayerNames) {
  //  // Write the player names.
  //  for (size_t i = 0; i < MaxPlayerCount; i++)
  //  {
  //    auto& name = PlayerNames[i];
  //    size_t size = name.size();
  //    WDATA2(to, (uint8_t)size);

  //    EZStream::WriteRawString(to, name);
  //  }
  //}
  //else {
  //  // Write the player names (empty)
  //  for (size_t i = 0; i < MaxPlayerCount; i++)
  //  {
  //    WDATA2(to, (uint8_t)0);
  //  }
  //}
}

// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::WriteGameData() {
  CheckComplete();

  //if (_GameData.StartFrame == 0) {
  //  throw new runtime_error("Inavlid start frame!  Must be > 0!");
  //}

  streampos start = _Stream.tellp();

  CSegmentHeader segHeader;
  segHeader.Type = EDataSegmentType::GameData;
  segHeader.Size = _GameData.SizeOf();

  WriteSegmentHeader(segHeader);
  // EZStream::Write<CGameData>(_Stream, _GameData);
  _GameData.Write(_Stream);




  streampos end = _Stream.tellp();
  int64_t total = static_cast<int64_t>(end - start);
  int expected = segHeader.Size + CSegmentHeader::SizeOf();

  if (total != expected)
  {
    throw runtime_error("Data size mismatch on write!");
  }
}


// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::ReadGameData() {
  CSegmentHeader segHeader;
  ReadSegmentHeader(segHeader);

  if (segHeader.Type != EDataSegmentType::GameData) {
    throw runtime_error("Invalid segment type for GameData!");
  }

  _GameData.Read(_Stream);
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

  streampos start = _Stream.tellp();

  CSegmentHeader segHeader;
  segHeader.Type = EDataSegmentType::ChatData;
  segHeader.Size = chat.SizeOf();
  // NOTE: Capturing this chat data doesn't capture who said what... the player indexes should be marked in the CGameData part of the file.

  WriteSegmentHeader(segHeader);

  EZStream::Write(_Stream, chat.FromPlayerIndex);
  EZStream::Write(_Stream, chat.ToPlayerIndex);
  EZStream::WriteRawString(_Stream, chat.Message);

  streampos end = _Stream.tellp();
  int64_t total = static_cast<int64_t>(end - start);
  int expected = segHeader.Size + segHeader.SizeOf();

  if (total != expected)
  {
    throw runtime_error("Data size mismatch on write!");
  }

  _Stream.flush();
}

// ------------------------------------------------------------------------------------------------------------------------
// Copy current input group data into the file...
void CReplayFile::FlushPendingInputData()
{
  if (CurInputGroupCount == 0) { return; }

  auto start = _Stream.tellp();

  size_t bufSize = _GameData.TotalInputSize * CurInputGroupCount;

  CSegmentHeader segHeader;
  segHeader.Type = EDataSegmentType::InputData;
  segHeader.Size = sizeof(uint32_t) + sizeof(uint16_t) + bufSize;

  WriteSegmentHeader(segHeader);

  // Frame info.
  WDATA(InputStartFrame);
  WDATA((uint16_t)CurInputGroupCount);

  // Group data.
  _Stream.write(reinterpret_cast<char*>(InputGroupBuffer), bufSize);

  // Parity check.
  auto end = _Stream.tellp();
  auto total = end - start;
  if (total != segHeader.Size + CSegmentHeader::SizeOf())
  {
    throw new runtime_error("Unexpected write size!");
  }

  Flush();

  InputStartFrame = 0;
  CurInputGroupCount = 0;
}

// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::AddInputSegment(const GameInput& input) {
  CheckComplete();

  // TODO: Some kind of check to make sure that we are writing the frame numbers sequentially!
  _Footer.Frame = input.frame;

  streampos start = _Stream.tellp();


  if (InputStartFrame == 0) {
    InputStartFrame = input.frame;
  }

  auto memOffset = CurInputGroupCount * _GameData.TotalInputSize;
  MEMCPY(InputGroupBuffer + memOffset, InputGroupBufSize, input.bits, _GameData.TotalInputSize);

  ++CurInputGroupCount;
  if (CurInputGroupCount >= MAX_INPUT_GROUP_COUNT) {
    FlushPendingInputData();
  }
  ////if (InputGroupSize >= MAX_INPUT_GROUP_SIZE) {
  ////  // Flush all inputs!
  ////}
  ////else {
  ////  // Throw it into the buffer....
  ////}

  ////int inputSize = _GameData.TotalInputSize;

  //CSegmentHeader segHeader;
  //segHeader.Type = EDataSegmentType::InputData;
  //segHeader.Size = inputSize;
  //WriteSegmentHeader(segHeader);

  //// EZStream::Write(_Stream, input.frame);
  //// TODO: memcpy
  //memcpy_s(DataBuffer, BUFFER_SIZE, input.bits, inputSize);
  ////for (int i = 0; i < inputSize; i++)
  ////{
  ////  DataBuffer[i] = input.bits[i];
  ////}

  //_Stream.write(reinterpret_cast<const char*>(DataBuffer), inputSize);

  //streampos end = _Stream.tellp();
  //int64_t total = static_cast<int64_t>(end - start);
  //int expected = segHeader.Size + CSegmentHeader::SizeOf();

  //if (total != expected)
  //{
  //  throw runtime_error("Data size mismatch on write!");
  //}

  // Flush();
}



// ----------------------------------------------------------------------------------------------------------
void CReplayFile::Flush()
{
  if (_Stream.is_open())
  {
    _Stream.flush();
  }
}

// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::CheckComplete() {
  if (_Mode == EReplayFileMode::REPLAY_FILE_MODE_COMPLETE || _Mode == EReplayFileMode::REPLAY_FILE_MODE_READ) {
    throw runtime_error("Invalid replay file mode:");
  }
}

// ------------------------------------------------------------------------------------------------------------------------
void CGameState::Read(istream& from) {
  RDATA2(from, Type);
  RDATA2(from, Frame);
  RDATA2(from, DataSize);
  RDATA2(from, CRC);

  if (Type == GAMESTATE_TYPE_NONE) {
    if (Frame != 0 || DataSize != 0 || CRC != 0) {
      throw new runtime_error("Invalid data for GAMESTATE_TYPE_NONE!");
    }

  }
  if (DataSize) {
    // TODO: We will have to internally allocate space for the state!
    throw new runtime_error("write some code so we can allocate space for the game state!");
    from.read(reinterpret_cast<char*>(Data), DataSize);
  }
}

// ------------------------------------------------------------------------------------------------------------------------
void CGameState::Write(ostream& to) {
  WDATA2(to, (uint8_t)Type);
  WDATA2(to, Frame);
  WDATA2(to, DataSize);
  WDATA2(to, CRC);

  if (Data) {
    to.write(reinterpret_cast<char*>(Data), DataSize);
  }
}