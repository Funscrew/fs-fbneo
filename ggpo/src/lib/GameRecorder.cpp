#include "GameRecorder.h"

#include <vector>
#include <filesystem>
#include <stdexcept>
#include <sstream>


using namespace std;

namespace SessionService
{
  constexpr uint64_t TEST_SESSION_ID = 0;
}

namespace ReplayFile
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

// ----------------------------------------------------------------------------------------------------
EZWriterEx::EZWriterEx(ostream* toStream_) {
  _Stream = toStream_;
}

// ----------------------------------------------------------------------------------------------------
void EZWriterEx::Write(const uint8_t data) {
  _Stream->write(reinterpret_cast<const char*>(data), static_cast<streamsize>(1));
}

// ----------------------------------------------------------------------------------------------------
template <typename T>
void Write(ostream& stream, const T& value) {
  _Stream->write(reinterpret_cast<const char*>(&value), sizeof(T));
}


namespace EZWriter
{
  template <typename T>
  void Write(ostream& stream, const T& value)
  {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
  }

  void Write(ostream& stream, const uint8_t data)
  {
    stream.write(reinterpret_cast<const char*>(data), static_cast<streamsize>(1));
  }

  void Write(ostream& stream, const vector<uint8_t>& bytes)
  {
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<streamsize>(bytes.size()));
  }

  //void WriteBytes(ostream& stream, const vector<uint8_t>& bytes)
  //{
  //  stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<streamsize>(bytes.size()));
  //}

  void RawString(ostream& stream, const string& value)
  {
    stream.write(value.data(), static_cast<streamsize>(value.size()));
  }
}

// ------------------------------------------------------------------------------------------------------------------------------
GameRecorder::GameRecorder(const CGameData& gameData_, const string& dataDir, uint64_t sessionId, bool overwriteExisting)
{
  GameData = gameData_;

  if (sessionId == SessionService::TEST_SESSION_ID)
  {
    // TODO: Some kind of passed in or better logger?
    // Log::Debug("Magic session id was used, replay overwrite is enabled!");
    overwriteExisting = true;
  }

  filesystem::path path = filesystem::path(dataDir) / (to_string(sessionId) + ".replay");

  Init(path, overwriteExisting);
}

// ------------------------------------------------------------------------------------------------------------------------------
GameRecorder::GameRecorder(const CGameData& gameData_, const string& toPath, bool overwriteExisting)
{
  GameData = gameData_;
  Init(toPath, overwriteExisting);
}

// ----------------------------------------------------------------------------------------------------------
GameRecorder::~GameRecorder()
{
  CloseStream();
}

// ----------------------------------------------------------------------------------------------------------
void GameRecorder::Init(const filesystem::path& toPath, bool overwriteExisting)
{
  if (filesystem::exists(toPath) && !overwriteExisting)
  {
    throw runtime_error("Data file for session id already exists!");
  }

  
  // int x = gameData.DataSize;

  CreateStream(toPath.string());
  FilePath = toPath.string();
  BaseFrames.fill(-1);
}


// ----------------------------------------------------------------------------------------------------------
bool GameRecorder::HasError() const
{
  return ErrorReason != EErrorReason::None;
}

// ----------------------------------------------------------------------------------------------------------
void GameRecorder::Flush()
{
  if (DataStream.is_open())
  {
    DataStream.flush();
  }
}

// ----------------------------------------------------------------------------------------------------------
void GameRecorder::CompleteReplay(  int frame,  ECompletionReason reason,  EErrorReason errReason,  const string& message)
{
  CheckComplete();

  const int COMPLETE_MSG_LEN = 64;
  // string useMsg = StringTools::Truncate(message, COMPLETE_MSG_LEN);

  stringstream ms(ios::in | ios::out | ios::binary);

  EZWriter::Write(ms, static_cast<uint8_t>(reason));
  EZWriter::Write(ms, static_cast<uint8_t>(errReason));
  EZWriter::Write(ms, frame);

  CopyFixedString(message, COMPLETE_MSG_LEN, WriteBuffer.data(), 0);
  ms.write(reinterpret_cast<const char*>(WriteBuffer.data()), COMPLETE_MSG_LEN);

  EZWriter::Write(ms, ReplayFile::Footer);

  WriteSegmentData(EDataSegmentType::Complete, ms);

  int64_t finalSize = static_cast<int64_t>(DataStream.tellp()) + static_cast<int64_t>(sizeof(int64_t));

  EZWriter::Write(DataStream, finalSize);

  CloseStream();

  RecordingComplete = true;
}

void GameRecorder::CompleteReplay(
  int frame,
  ECompletionReason reason,
  EErrorReason errReason,
  const char* message)
{
  CompleteReplay(frame, reason, errReason, message == nullptr ? string() : string(message));
}

void GameRecorder::AddChatSegment(ChatData& chat)
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

bool GameRecorder::AddInput(int playerIndex, GameInput& input)
{
  // EZQ<GameInput>& buf = *PlayerBuffers[playerIndex];
  auto& buf = PlayerBuffers[playerIndex];

  if (buf.IsFull())
  {
    OnError(EErrorReason::InputBufferFull, "Too many unmerged inputs!: " + to_string(playerIndex));
    return false;
  }

  int startFrame = BaseFrames[playerIndex];

  if (input.frame == startFrame)
  {
    return false;
  }

  if (input.frame != startFrame + 1)
  {
    throw runtime_error("Invalid frame number!");
  }

  buf.Push(input);
  startFrame++;
  BaseFrames[playerIndex] = startFrame;

  int len = MAX_PLAYERS; // static_cast<int>(PlayerBuffers.size());

  while (true)
  {
    bool popIt = true;
    int startMergeFrame = SyncedBaseFrame;

    for (int i = 0; i < len; i++)
    {
      // EZQ<GameInput>& pBuf = *PlayerBuffers[i];
      auto& pBuf = PlayerBuffers[i];

      if (pBuf.Count() == 0)
      {
        popIt = false;
        break;
      }

      GameInput giBuf;
      pBuf.First(giBuf);

      if (giBuf.frame != startMergeFrame)
      {
        throw runtime_error("Invalid frame number at player index!");
      }
    }

    if (!popIt)
    {
      break;
    }

    for (int i = 0; i < len; i++)
    {
      GameInput giBuf;
      PlayerBuffers[i].First(giBuf);
      MergeBuffer[i] = giBuf;

      PlayerBuffers[i].Pop();
    }

    MergeInputs();

    startMergeFrame++;
    SyncedBaseFrame = startMergeFrame;
  }

  return true;
}

void GameRecorder::CloseStream()
{
  if (DataStream.is_open())
  {
    DataStream.close();
  }
}

void GameRecorder::CreateStream(const string& path)
{
  DataStream.open(path, ios::binary | ios::out | ios::trunc);

  if (!DataStream.is_open())
  {
    throw runtime_error("Unable to create replay data stream.");
  }

  WriteHeader(DataStream);
}

void GameRecorder::WriteHeader(ostream& res)
{
  EZWriter::Write(res, ReplayFile::Preamble);
  EZWriter::Write(res, vector<uint8_t> { 1 });

  WriteGameDataSegment(GameData);
}

void GameRecorder::CheckComplete()
{
  if (RecordingComplete)
  {
    throw runtime_error("Recording is complete! Can't write anymore!");
  }
}

void GameRecorder::WriteInputSegment(GameInput& input)
{
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

  DataStream.write(reinterpret_cast<const char*>(WriteBuffer.data()), inputSize);

  streampos end = DataStream.tellp();
  int64_t total = static_cast<int64_t>(end - start);
  int expected = segmentSize + 3;

  if (total != expected)
  {
    throw runtime_error("Data size mismatch on write!");
  }

  Flush();
}

void GameRecorder::WriteGameDataSegment(const CGameData& gameData)
{
  CheckComplete();

  streampos start = DataStream.tellp();

  uint16_t segmentSize = CGameData::DataSize;

  EZWriter::Write(DataStream, static_cast<uint8_t>(EDataSegmentType::GameData));
  EZWriter::Write(DataStream, segmentSize);

  {
    int size = CopyFixedString(gameData.GameName, CGameData::MAX_GAME_NAME_SIZE, WriteBuffer.data(), 0);
    DataStream.write(reinterpret_cast<const char*>(WriteBuffer.data()), size);
  }

  {
    int size = CopyFixedString(gameData.GameVersion, CGameData::MAX_VERSION_SIZE, WriteBuffer.data(), 0);
    DataStream.write(reinterpret_cast<const char*>(WriteBuffer.data()), size);
  }

  EZWriter::Write(DataStream, gameData.PlayerCount);
  EZWriter::Write(DataStream, gameData.TotalInputSize);

  streampos end = DataStream.tellp();
  int64_t total = static_cast<int64_t>(end - start);
  int expected = segmentSize + 3;

  if (total != expected)
  {
    throw runtime_error("Data size mismatch on write!");
  }
}

// ------------------------------------------------------------------------------------------------------
void GameRecorder::WriteSegmentData(EDataSegmentType segmentType, stringstream& data)
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

// --------------------------------------------------------------------------------------------------
int GameRecorder::CopyFixedString(const string& data, int maxSize, uint8_t* toBuffer, int offset)
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

void GameRecorder::OnError(EErrorReason errReason, const string& message)
{
  ErrorReason = errReason;
  ErrorMessage = message;

  CompleteReplay(SyncedBaseFrame, ECompletionReason::Error, errReason, message);
}

void GameRecorder::MergeInputs()
{
  // PlayerBuffers.count() == MAX_PLAYERS

  int len = static_cast<int>(MAX_PLAYERS);
  int offset = GameData.TotalInputSize / len;

  GameInput merged{};
  merged.size = GameData.TotalInputSize;
  merged.frame = MergeBuffer[0].frame;

  for (int i = 0; i < len; i++)
  {
    if (MergeBuffer[i].frame != merged.frame)
    {
      throw runtime_error("Unexpected frame number from merge buffer.");
    }

    for (int j = 0; j < offset; j++)
    {
      uint8_t d = MergeBuffer[i].bits[j];
      merged.bits[(i * offset) + j] = d;
    }
  }

  WriteInputSegment(merged);

  MergedInputs.push(merged);
}