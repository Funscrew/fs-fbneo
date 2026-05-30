#include "NetcodeInterop.h"
#include "CReplayFile.h"

std::string _LastError;

// ---------------------------------------------------------------------------------------------------------
extern "C" __declspec(dllexport)
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

extern "C" __declspec(dllexport)
void TestError()
{
  // _LastError.assign("This is a test error!");
  _LastError.assign("大犬");
}

extern "C" __declspec(dllexport)
const int LastError(char* buffer, uint32_t bufferSize)
{
  size_t res = _LastError.size() + 1; // includes null terminator

  if (buffer == nullptr || bufferSize == 0)
    return -1;

  if (bufferSize < res) {
    res = bufferSize - 1;
  }

  std::memcpy(buffer, _LastError.c_str(), res);

  return static_cast<int>(res);
}

extern "C" __declspec(dllexport)
void ReplayFile_Destroy(CReplayFile* replayFile)
{
  if (replayFile) {
    delete replayFile;
  }
}