#include "NetcodeInterop.h"
#include "CReplayFile.h"

#ifdef _WIN32
#define API_EXPORT extern "C" __declspec(dllexport)
#else
#define API_EXPORT extern "C" __attribute__((visibility("default")))
#endif

std::string _LastError;


// ---------------------------------------------------------------------------------------------------------
API_EXPORT
int ReplayFile_OpenWrite(CGameData* gameData, const CGameState* state, char* path, CReplayFile** file) {
  *file = nullptr;

  try
  {
    auto res = new CReplayFile(path, *gameData, state);
    *file = res;

    return ERRORCODE_OK;
  }
  catch (const std::exception& ex)
  {
    _LastError.assign(ex.what());
    return ERRORCODE_UNHANDLED;
  }
}


// ---------------------------------------------------------------------------------------------------------
API_EXPORT
int ReplayFile_AddInput(CReplayFile* target, GameInput* input) {
  try
  {
    target->AddInputSegment(*input);
  }
  catch (const std::exception& ex)
  {
    _LastError.assign(ex.what());
    return ERRORCODE_UNHANDLED;
  }

  return ERRORCODE_OK;
}

// ---------------------------------------------------------------------------------------------------------
API_EXPORT
int CompleteReplay(CReplayFile* target, int frame, ECompletionReason reason, EErrorReason errReason, char* message, uint8_t messageSize) {

  try
  {
    std::string useMsg;
    useMsg.assign(message, messageSize);
    // useMsg.copy(message, messageSize, 0);
    target->CompleteReplayFile(frame, reason, errReason, useMsg);
  }
  catch (const std::exception& ex)
  {
    _LastError.assign(ex.what());
    return ERRORCODE_UNHANDLED;
  }

  return ERRORCODE_OK;
}


// ---------------------------------------------------------------------------------------------------------
API_EXPORT
int ReplayFile_OpenRead(char* path, CReplayFile** file) {
  if (!std::filesystem::exists(path)) {
    _LastError.assign("File not found!");
    return ERRORCODE_FILENOTFOUND;
  }

  *file = nullptr;
  try
  {
    auto res = new CReplayFile(path);
    *file = res;
    return ERRORCODE_OK;
  }
  catch (const std::exception& ex)
  {
    _LastError.assign(ex.what());
    return ERRORCODE_UNHANDLED;
  }
}

// ---------------------------------------------------------------------------------------------------------
API_EXPORT
void TestError() {
  // _LastError.assign("This is a test error!");
  _LastError.assign("big-doggy-大犬");
}

// ---------------------------------------------------------------------------------------------------------
API_EXPORT
const int LastError(char* buffer, uint32_t bufferSize) {
  size_t res = _LastError.size() + 1; // includes null terminator

  if (buffer == nullptr || bufferSize == 0)
    return -1;

  if (bufferSize < res) {
    res = bufferSize - 1;
  }

  memcpy(buffer, _LastError.c_str(), res);

  return static_cast<int>(res);
}

// ---------------------------------------------------------------------------------------------------------
API_EXPORT
void ReplayFile_Destroy(CReplayFile* replayFile) {
  if (replayFile) {
    delete replayFile;
  }
}