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
int ReplayFile_OpenWrite(const CGameData* gameData, const CGameState* state, char* path, CReplayFile** file) {
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
int ReplayFile_GetGameData(CReplayFile* file, CGameData& data) {
  try {
    data = file->GameData();
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
int ReplayFile_GetFooter(CReplayFile* file, CFooterData& data) {
  try {
    data = file->FooterData();
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
int ReplayFile_GetNextInput(CReplayFile* file, GameInput& input) {
  try {
    bool hasInput = file->GetNextInput(input);
    if (!hasInput) { 
      return ERRORCODE_NO_GAMEINPUT;
    }
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
int ReplayFile_Close(CReplayFile* file) {

  try
  {
    if (file != nullptr) {
      delete file;
      file = nullptr;
    }
    return 0;
  }
  catch (const std::exception& ex)
  {
    _LastError.assign(ex.what());
    return ERRORCODE_UNHANDLED;
  }
}

// ---------------------------------------------------------------------------------------------------------
API_EXPORT
int ReplayFile_AddInput(CReplayFile* file, GameInput* input) {
  try
  {
    file->AddInputSegment(*input);
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
int ReplayFile_AddChat(CReplayFile* file, const CChatData& chat) { 
  try
  {
    file->AddChatSegment(chat);
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
int CompleteReplay(CReplayFile* target, ECompletionReason reason, EErrorReason errReason, char* message, uint8_t messageSize) {

  try
  {
    // NOTE: We may drop the usage of std::string since we really only care about passing bytes around?
    std::string useMsg;
    useMsg.assign(message, messageSize);
    target->CompleteReplayFile(reason, errReason, useMsg);
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
    res = bufferSize;
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