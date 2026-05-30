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
int ReplayFile_OpenRead(char* path, CReplayFile** file)
{
  if (!std::filesystem::exists(path)) {
    _LastError.assign("File not found!");
    return ERRORCODE_FILENOTFOUND;
  }

  *file = nullptr;
  try
  {
    auto res = new CReplayFile(path);
    *file = res;
    return ERRORCODE_NONE;
  }
  catch (const std::exception& ex)
  {
    _LastError.assign(ex.what());
    return ERRORCODE_UNHANDLED;
  }
}

API_EXPORT
void TestError()
{
  // _LastError.assign("This is a test error!");
  _LastError.assign("大犬");
}

API_EXPORT
const int LastError(char* buffer, uint32_t bufferSize)
{
  size_t res = _LastError.size() + 1; // includes null terminator

  if (buffer == nullptr || bufferSize == 0)
    return -1;

  if (bufferSize < res) {
    res = bufferSize - 1;
  }

  memcpy(buffer, _LastError.c_str(), res);

  return static_cast<int>(res);
}

API_EXPORT
void ReplayFile_Destroy(CReplayFile* replayFile)
{
  if (replayFile) {
    delete replayFile;
  }
}