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
enum class EDataSegmentType : uint8_t
{
  Invalid = 0,
  GameData,
  InputData,
  ChatData,
  Footer
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
// TODO: Add the player names + indexes for chat data.  This CAN be blank for single player games, or if you don't care.
class CGameData
{
public:
  static constexpr int MAX_GAME_NAME_SIZE = 32;
  static constexpr int MAX_VERSION_SIZE = 16;

  char GameName[MAX_GAME_NAME_SIZE];
  char GameVersion[MAX_VERSION_SIZE];

  uint16_t PlayerCount = 0;
  uint16_t TotalInputSize = 0;

  // static constexpr uint16_t DataSize = MAX_GAME_NAME_SIZE + MAX_VERSION_SIZE + sizeof(int) + sizeof(int);

  void SetGameName(std::string name);
  void SetVersion(std::string version);

};

// ========================================================================================================================
class ChatData
{
public:
  static constexpr int CHAT_DATA_MAX = 128;

  // NOTE: There is an error if all indexes are the same number!
  uint8_t FromPlayerIndex = 0;
  uint8_t ToPlayerIndex = 0;
  int Frame = 0;
  std::string Message;

  inline uint32_t GetSize() { return Message.size() + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(int); }
};

// ========================================================================================================================
struct CSegmentHeader {
  EDataSegmentType Type;
  uint32_t Size;

  // This unfucks c++ not actually treating the enum as a byte even tho we told it to.  Apologists can suck it, not interested in the excuses.
  static constexpr uint32_t SizeOf() { return sizeof(uint8_t) + sizeof(uint32_t); }
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

  // Get the total frame count for the file.
  int TotalFrames();

private:
  uint8_t WriteBuffer[0x400];

  EReplayFileMode _Mode;
  CGameData GameData;

  int _CurFrame = 0;

  // REFACTOR: _Stream
  std::fstream DataStream;

  void Init(const filesystem::path& path, EReplayFileMode mode_);
  void CheckComplete();

  //
  void ReadSegmentHeader(CSegmentHeader& header);
  void WriteSegmentHeader(CSegmentHeader& header);

  // Writing funcitons:
  void WriteHeader();
  void WriteGameData();

  void WriteSegmentData(EDataSegmentType segmentType, stringstream& data);

  // Reading functions:
  void ReadHeader();
  void ReadFooter();
  void ReadGameData();

  void Flush();
};
