#include "CGameRecorder.h"

#include <vector>
#include <filesystem>
#include <stdexcept>
#include <sstream>


using namespace std;

namespace SessionService
{
  constexpr uint64_t TEST_SESSION_ID = 12345;
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


// ------------------------------------------------------------------------------------------------------------------------------
CGameRecorder::CGameRecorder(const CGameData& gameData_, const string& dataDir, uint64_t sessionId, bool overwriteExisting)
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
CGameRecorder::CGameRecorder(const CGameData& gameData_, const string& toPath, bool overwriteExisting)
{
  GameData = gameData_;
  Init(toPath, overwriteExisting);
}

// ----------------------------------------------------------------------------------------------------------
CGameRecorder::~CGameRecorder()
{
  // _File->CloseStream();
  delete(_File);
  _File = nullptr;
}

// ----------------------------------------------------------------------------------------------------------
void CGameRecorder::Init(const filesystem::path& toPath, bool overwriteExisting)
{
  if (filesystem::exists(toPath) && !overwriteExisting)
  {
    throw runtime_error("Data file for session id already exists!");
  }

  _File = new CReplayFile(toPath, GameData, nullptr);

  BaseFrames.fill(-1);
}

// ----------------------------------------------------------------------------------------------------------
bool CGameRecorder::HasError() const
{
  return ErrorReason != EErrorReason::None;
}

// ----------------------------------------------------------------------------------------------------------
void CGameRecorder::CompleteReplay(ECompletionReason reason, EErrorReason errReason, const string& message)
{
  _File->CompleteReplayFile(reason, errReason, message);
  RecordingComplete = true;
}

// ------------------------------------------------------------------------------------------------------
void CGameRecorder::OnError(EErrorReason errReason, const string& message)
{
  ErrorReason = errReason;
  ErrorMessage = message;

  CompleteReplay(ECompletionReason::Error, errReason, message);
}

// ----------------------------------------------------------------------------------------------------------
bool CGameRecorder::AddInput(int playerIndex, GameInput& input)
{
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

  // TODO: We don't need to use a const here!
  int len = MAX_PLAYERS; 

  while (true)
  {
    bool popIt = true;
    int startMergeFrame = SyncedBaseFrame;

    for (int i = 0; i < len; i++)
    {
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

// ----------------------------------------------------------------------------------------------------------
bool CGameRecorder::AddInputs(int frame, uint8_t* data, int dataSize) {

  if (frame != SyncedBaseFrame + 1) {
    throw runtime_error("Unexpected frame number when adding inputs.");
  }

  GameInput merged;
  merged.size = dataSize;
  merged.frame = frame;

  // Copy the memory over.....
  memcpy(merged.bits, data, dataSize);

  _File->AddInputSegment(merged);

  MergedInputs.Push(merged);

  SyncedBaseFrame = frame;

  return true;
}

// ------------------------------------------------------------------------------------------------------
void CGameRecorder::MergeInputs()
{
  // PlayerBuffers.count() == MAX_PLAYERS

  int len = static_cast<int>(MAX_PLAYERS);
  int offset = GameData.TotalInputSize / len;

  GameInput merged;
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

  _File->AddInputSegment(merged);

  MergedInputs.Push(merged);
}
