#include <string>

#include "CReplayFile.h"
#include <sstream>

#define WDATA(x) EZStream::Write(_Stream, x)
#define RDATA(x) EZStream::Read(_Stream, x);

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

// OPTIONS:  Max # of inputs that can be grouped together.
const int MAX_INPUT_GROUP_SIZE = 0x80;
uint32_t CurInputGroupCount = 0;
uint32_t InputStartFrame = 0;

uint8_t* InputGroupBuffer = nullptr;
size_t InputGroupBufSize = 0;


const int BUFFER_SIZE = 0x400;
uint64_t scratch = 0;
uint8_t DataBuffer[BUFFER_SIZE];
EReplayFileMode _Mode;

std::fstream _Stream;
std::string* PlayerNames = nullptr;



// ------------------------------------------------------------------------------------------------------------------------
// Open the replay file in read mode.
CReplayFile::CReplayFile(const std::filesystem::path& path) {
  Init(path, REPLAY_FILE_MODE_READ);
}

// ------------------------------------------------------------------------------------------------------------------------
CReplayFile::CReplayFile(const std::filesystem::path& path, const CGameData& gameData_)
  : _GameData(gameData_)
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

    InputGroupBufSize = MAX_INPUT_GROUP_SIZE * _GameData.TotalInputSize;
    InputGroupBuffer = (uint8_t*)malloc(InputGroupBufSize);
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

  EZStream::Read(_Stream, type);
  EZStream::Read(_Stream, size);

  header.Type = (EDataSegmentType)type;
  header.Size = size;
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
  // NOTE: We are collecting segments until we hit the next input segment.
  auto cPos = _Stream.tellg();
  scratch = cPos;

  CSegmentHeader segHeader;
  ReadSegmentHeader(segHeader);

  while (true) {
    switch (segHeader.Type)
    {
    case EDataSegmentType::InputData:
      // NOTE: If we swap to sequential inputs (we probably should) then we don't need to read this back in.
      // I am leaning in that direction as the overhead of recording the frame# is going to be more than the inputs
      // in many cases.
      EZStream::Read(_Stream, input.frame);

      EZStream::ReadBytes(_Stream, DataBuffer, _GameData.TotalInputSize);
      memcpy_s(input.bits, GameInput::DATA_SIZE, DataBuffer, _GameData.TotalInputSize);
      return true;
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
  _Mode = REPLAY_FILE_MODE_COMPLETE;
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
}

// ------------------------------------------------------------------------------------------------------------------------
void CGameData::AllocatePlayerNames() {
  if (!PlayerNames) {
    PlayerNames = new std::string[MaxPlayerCount];
  }
}

// ------------------------------------------------------------------------------------------------------------------------
void CGameData::SetPlayerName(std::string name, uint8_t index)
{
  if (name.length() <= 0) { throw new runtime_error("Name must be at least one character!"); }
  if (name.length() > CGameData::MAX_PLAYER_NAME) { throw new runtime_error("Max name length (32) exceeded!"); }
  if (index >= MaxPlayerCount) { throw new runtime_error("Invalid player index!"); }

  AllocatePlayerNames();
  PlayerNames[index].assign(name);
}

// ------------------------------------------------------------------------------------------------------------------------
bool TryGetPlayerName(uint8_t index, std::string& to)
{
  if (!PlayerNames) { return false; }

  auto& target = PlayerNames[index];
  auto len = target.length();
  if (len == 0) { return false; }

  to.assign(target);
  return true;
}


// ------------------------------------------------------------------------------------------------------------------------
uint32_t CGameData::SizeOf() {
  uint32_t res = sizeof(uint16_t) * 3;    // Player count, input size, and start frame.
  res += (MAX_GAME_NAME_SIZE + MAX_VERSION_SIZE);

  // Player names.....
  res += sizeof(uint8_t) * MaxPlayerCount;
  if (PlayerNames) {
    for (size_t i = 0; i < MaxPlayerCount; i++)
    {
      res += (PlayerNames[i].size());
    }
  }

  return res;
}

// ------------------------------------------------------------------------------------------------------------------------
CGameData::~CGameData() {
  if (PlayerNames) {
    delete[](PlayerNames);
  }
  if (InputGroupBuffer) { free(InputGroupBuffer); }
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

  WDATA(_GameData.GameName);
  WDATA(_GameData.GameVersion);
  WDATA(_GameData.MaxPlayerCount);
  WDATA(_GameData.TotalInputSize);
  WDATA(_GameData.StartFrame);

  if (PlayerNames) {
    // Write the player names.
    for (size_t i = 0; i < _GameData.MaxPlayerCount; i++)
    {
      auto& name = PlayerNames[i];
      size_t size = name.size();
      WDATA((uint8_t)size);

      EZStream::WriteRawString(_Stream, name);
    }
  }
  else {
    // Write the player names (empty)
    for (size_t i = 0; i < _GameData.MaxPlayerCount; i++)
    {
      WDATA((uint8_t)0);
    }
  }


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

  RDATA(_GameData.GameName);
  RDATA(_GameData.GameVersion);
  RDATA(_GameData.MaxPlayerCount);
  RDATA(_GameData.TotalInputSize);
  RDATA(_GameData.StartFrame);

  for (uint8_t i = 0; i < _GameData.MaxPlayerCount; i++)
  {
    uint8_t nameSize;
    RDATA(nameSize);
    if (nameSize > 0) {
      std::string nameBuffer;
      EZStream::ReadRawString(_Stream, PlayerNames[i], nameSize);
      _GameData.SetPlayerName(nameBuffer, i);
    }
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
  auto start = _Stream.tellp();

  size_t bufSize = InputGroupBufSize;

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
  memcpy_s(InputGroupBuffer, InputGroupBufSize, input.bits, _GameData.TotalInputSize);

  ++CurInputGroupCount;
  if (CurInputGroupCount >= MAX_INPUT_GROUP_SIZE) { 
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
  if (_Mode == REPLAY_FILE_MODE_COMPLETE || _Mode == REPLAY_FILE_MODE_READ) {
    throw runtime_error("Invalid replay file mode:");
  }
}
