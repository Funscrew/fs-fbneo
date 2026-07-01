#include "CReplayFile.h"
#include "../../../src/burner/EZStream.h"

#include <sstream>
#include <vector>

using namespace std;

// 16MB.  This is arbitrary and might be removed or otherwise enhanced....
// Should be OK for now.....
const size_t MAX_STATE_DATA_SIZE = 0x1000000;


// PATCH: Workaround for no memcpy_s on linux....
// TODO: There should be a real macro like '__STDC_LIB_EXT1__' to properly identify the feautre....
#ifdef _WIN32
#define MEMCPY(dest, destSize, src, srcSize) memcpy_s(dest, destSize, src, srcSize)
#else
  // Linux, probably
#define MEMCPY(dest, destSize, src, srcSize) memcpy(dest, src, srcSize)
#endif


namespace ReplayData
{
  const vector<uint8_t> FileId = { 'f', 's', 'n', 'e', 'o', '-', 'r', 'f' };
  const int HEADER_STUB_SIZE = 8;
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
    throw runtime_error("Could not allocate space for input group buffer!");
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
    throw runtime_error("Invalid footer marker!");
  }

  if (sh.Size != CFooterData::SizeOf()) // (fileSize - (FOOTER_SIZE - sizeof(uint8_t) + sizeof(uint32_t))))
  {
    throw runtime_error("Invalid footer segment size!");
  }

  _Footer.Read(_Stream);
  if (_Footer.FinalFileSize != fileSize) {
    throw runtime_error("Invalid check size!");
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


  while (true) {

    // NOTE: We are collecting segments until we hit the next input segment.
    auto cPos = _Stream.tellg();
    scratch = cPos;

    CSegmentHeader segHeader;
    ReadSegmentHeader(segHeader);

    switch (segHeader.Type)
    {
    case EDataSegmentType::InputData:
    {
      // Here we will read in the next block of inputs...
      // Frame info.
      RDATA(InputStartFrame);
      RDATA(CurInputGroupCount);

      auto expectedFrame = LastUsedFrame + 1;
      if (expectedFrame != InputStartFrame) {
        throw runtime_error("Unexpected input frame was encountered!");
      }

      uint16_t readSize = CurInputGroupCount * _GameData.TotalInputSize;
      if (readSize != segHeader.Size - (sizeof(uint32_t) + sizeof(uint16_t)))
      {
        throw runtime_error("Invalid data size in InputData segment!");
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
      throw runtime_error("invalid segment type!");
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

  LastUsedFrame = input.frame;
}

// ------------------------------------------------------------------------------------------------------------------------
void CFooterData::Read(istream& from) {
  EZStream::Read(from, FrameCount);
  EZStream::Read(from, CompleteReason);
  EZStream::Read(from, ErrorReason);

  EZStream::ReadBytes(from, reinterpret_cast<uint8_t*>(&Message), MSG_SIZE);

  EZStream::Read(from, FinalFileSize);
}

// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::CompleteReplayFile(ECompletionReason reason, EErrorReason errReason, const std::string& message) {

  CheckComplete();

  // Write any pending inputs.
  FlushPendingInputData();


  // TODO: Some kind of check to make sure that the frame we are ending on is at or near the current input frame.
  // WRITE FOOTER
  stringstream ms(ios::in | ios::out | ios::binary);
  EZStream::Write(ms, LastUsedFrame);
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
  if (name.length() <= 0) { throw runtime_error("Name must be at least one character!"); }
  if (name.length() > CGameData::MAX_PLAYER_NAME) { throw runtime_error("Max name length (32) exceeded!"); }
  if (index >= MaxPlayerCount) { throw runtime_error("Invalid player index!"); }

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
  return res;
}

// ------------------------------------------------------------------------------------------------------------------------
CGameData::CGameData() {
}


// ------------------------------------------------------------------------------------------------------------------------
void CGameData::Read(istream& from) {

  RDATA2(from, GameName);
  RDATA2(from, GameVersion);
  RDATA2(from, MaxPlayerCount);
  RDATA2(from, TotalInputSize);
  RDATA2(from, PlayerNames);
}


// ------------------------------------------------------------------------------------------------------------------------
void CGameData::Write(ostream& to) {

  WDATA2(to, GameName);
  WDATA2(to, GameVersion);
  WDATA2(to, MaxPlayerCount);
  WDATA2(to, TotalInputSize);
  WDATA2(to, PlayerNames);
}

// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::WriteGameData() {
  CheckComplete();

  streampos start = _Stream.tellp();

  CSegmentHeader segHeader;
  segHeader.Type = EDataSegmentType::GameData;
  segHeader.Size = _GameData.SizeOf();

  WriteSegmentHeader(segHeader);
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
void CReplayFile::AddChatSegment(const CChatData& chat)
{
  CheckComplete();

  // Make sure that the chat data is at a reasonable place in the stream...
  if (chat.Frame < LastUsedFrame || chat.Frame > LastUsedFrame + 1) {
    throw runtime_error("Invalid frame number for chat data!");
  }
  if (chat.FromPlayerIndex == chat.ToPlayerIndex) {
    throw runtime_error("from/to indexes may not be the same!");
  }

  // We want to flush all current inputs so that the chat data comes where it should:
  FlushPendingInputData();

  streampos start = _Stream.tellp();

  CSegmentHeader segHeader;
  segHeader.Type = EDataSegmentType::ChatData;
  segHeader.Size = chat.SizeOf();
  // NOTE: Capturing this chat data doesn't capture who said what... the player indexes should be marked in the CGameData part of the file.

  WriteSegmentHeader(segHeader);
  chat.Write(_Stream);

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
    throw runtime_error("Unexpected write size!");
  }

  Flush();

  InputStartFrame = 0;
  CurInputGroupCount = 0;
}

// ------------------------------------------------------------------------------------------------------------------------
void CReplayFile::AddInputSegment(const GameInput& input) {
  CheckComplete();

  if ((uint32_t)input.frame != LastUsedFrame + 1) {
    throw runtime_error("Incorrect frame #!");
  }
  LastUsedFrame = input.frame;

  // TODO: Some kind of check to make sure that we are writing the frame numbers sequentially!
  _Footer.FrameCount = input.frame;

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
void CChatData::Read(istream& from) {
  RDATA2(from, FromPlayerIndex);
  RDATA2(from, ToPlayerIndex);
  RDATA2(from, Frame);
  RDATA2(from, DataSize);

  memset(Data, 0, CChatData::CHAT_DATA_MAX);
  EZStream::ReadBytes(from, Data, DataSize);
}

// ------------------------------------------------------------------------------------------------------------------------
void CChatData::Write(ostream& to) const {
  WDATA2(to, FromPlayerIndex);
  WDATA2(to, ToPlayerIndex);
  WDATA2(to, Frame);
  WDATA2(to, DataSize);

  EZStream::Write(to, Data, DataSize);
}

// ------------------------------------------------------------------------------------------------------------------------
CGameState::~CGameState() {
  ClearData();
}

// ------------------------------------------------------------------------------------------------------------------------
void CGameState::Read(istream& from) {
  // NOTE: We might end up splitting the state data from the rest of the state information.
  // The purpose is that we might not want to immediately read in all of the state data, esp. if it is large, or
  // we are not interested in consuming it....


  RDATA2(from, Type);
  RDATA2(from, StartFrame);
  RDATA2(from, DataSize);
  RDATA2(from, CRC32);

  if (Type == (uint8_t)EGameStateType::GAMESTATE_TYPE_NONE) {
    if (StartFrame != 0 || DataSize != 0 || CRC32 != 0) {
      throw runtime_error("Invalid data for GAMESTATE_TYPE_NONE!");
    }
  }

  if (DataSize) {
    // TODO: We will have to internally allocate space for the state!
    if (DataSize > MAX_STATE_DATA_SIZE) {
      throw runtime_error("DataSize exceed MAX_STATE_DATA_SIZE!");
    }
    _Data = (uint8_t*)malloc(DataSize);
    from.read(reinterpret_cast<char*>(_Data), DataSize);
  }

  // TODO: If set, then we want to check the file or data contents to make sure'
  // that they are correct.
  if (CRC32 != 0) {
    throw runtime_error("CRC32 is not yet supported!");
  }
}

// ------------------------------------------------------------------------------------------------------------------------
void CGameState::GetData(uint8_t** buffer, uint32_t* bufferSize) {
  if (Type == (uint8_t)EGameStateType::GAMESTATE_TYPE_FILE)
  {
    // TODO: We will want to read in the file contents + present them as raw data.
    throw runtime_error("not supported yet....");
  }
  *bufferSize = DataSize;
  *buffer = _Data;
}

// ------------------------------------------------------------------------------------------------------------------------
void CGameState::GetDataAsString(char* intoBuffer, size_t bufferSize) {
  if (bufferSize < DataSize + 1) {
    throw runtime_error("buffer is not large enough to contain the string!");
  }
  memcpy(intoBuffer, _Data, DataSize);
  intoBuffer[DataSize] = 0;
}

// ------------------------------------------------------------------------------------------------------------------------
void CGameState::ClearData() {
  if (_Data) {
    free(_Data);
    _Data = nullptr;
  }
}

// ------------------------------------------------------------------------------------------------------------------------
void CGameState::Write(ostream& to) {
  WDATA2(to, (uint8_t)Type);
  WDATA2(to, StartFrame);
  WDATA2(to, DataSize);
  WDATA2(to, CRC32);

  if (_Data) {
    if (DataSize == 0 || DataSize > MAX_STATE_DATA_SIZE) {
      throw runtime_error("Invalid data size! zero or > MAX_STATE_DATA_SIZE");
    }
    to.write(reinterpret_cast<char*>(_Data), DataSize);
  }

  if (CRC32 != 0) {
    throw runtime_error("CRC32 is not yet supported!");
  }

}