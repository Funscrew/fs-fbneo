#pragma once

//#include <cstdint>
//#include <string>
//#include <array>
//#include <fstream>
//#include <memory>

#include <filesystem>
#include <string>
#include <fstream>
#include <memory>

#include "game_input.h"

using namespace std;

// ========================================================================================================================
enum class EErrorReason
{
  None = 0,
  InputBufferFull
};


// ========================================================================================================================
enum class EDataSegmentType
{
  Invalid = 0,
  GameData,
  InputData,
  ChatData,
  Complete
};

// ========================================================================================================================
enum class ECompletionReason
{
  Invalid = 0,
  NormalDisconnect,
  Error
};

// ========================================================================================================================
enum EReplayFileMode {
  REPLAY_FILE_MODE_INVALID = 0,
  REPLAY_FILE_MODE_READ,
  REPLAY_FILE_MODE_WRITE,

  // The replay data is complete.  No new data can be added now.
  REPLAY_FILE_MODE_COMPLETE
};

// ========================================================================================================================
class CGameData
{
public:
  static constexpr int MAX_GAME_NAME_SIZE = 32;
  static constexpr int MAX_VERSION_SIZE = 16;

  std::string GameName;
  std::string GameVersion = "<n/a>";

  int PlayerCount = 0;
  int TotalInputSize = 0;

  static constexpr uint16_t DataSize = MAX_GAME_NAME_SIZE + MAX_VERSION_SIZE + sizeof(int) + sizeof(int);
};

// ========================================================================================================================
class ChatData
{
public:
  static constexpr int CHAT_DATA_MAX = 128;

  int FromPlayerIndex = 0;
  int Frame = 0;
  std::string Message;
  int ToPlayerIndex = -1;
};

// ========================================================================================================================
// Reads / writes data into a replay file.
class CReplayFile {

public:
  // Open a replay file in read mode.
  CReplayFile(const filesystem::path& path);

  // Open a replay file in write mode, for the given game.
  CReplayFile(const filesystem::path& path, const CGameData& gameData_);

  void AddChatSegment(ChatData& chat);
  void WriteInputSegment(const GameInput& input);
  void CompleteReplay(int frame, ECompletionReason reason, EErrorReason errReason, const std::string& message);

  void CloseStream();

  // TODO: Share
  static int CopyFixedString(const std::string& data, int maxSize, uint8_t* toBuffer, int offset);

private:
  uint8_t WriteBuffer[0x400];

  EReplayFileMode _Mode;
  CGameData GameData;

  // REFACTOR: _Stream
  std::ofstream DataStream;

  void Init(const filesystem::path& path, EReplayFileMode mode_);
  void CheckComplete();

  // Writing funcitons:
  void WriteHeader();
  void WriteGameData();
  void WriteSegmentData(EDataSegmentType segmentType, stringstream& data);

  // Reading functions:
  void ReadHeader();

  void Flush();
};
