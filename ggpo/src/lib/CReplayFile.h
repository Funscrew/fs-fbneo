#pragma once

#include <filesystem>
#include <string>
#include <fstream>
#include <memory>

#include "game_input.h"

using namespace std;

// ========================================================================================================================
enum class EErrorReason : uint8_t
{
  None = 0,
  InputBufferFull
};


// ========================================================================================================================
enum class EDataSegmentType : uint8_t
{
  Invalid = 0,
  GameData,
  GameState,
  InputData,
  ChatData,
  Footer
};

// ========================================================================================================================
enum class ECompletionReason : uint8_t
{
  Invalid = 0,
  NormalDisconnect,
  Error
};

// ========================================================================================================================
enum class EReplayFileMode : uint8_t {
  REPLAY_FILE_MODE_INVALID = 0,
  REPLAY_FILE_MODE_READ,
  REPLAY_FILE_MODE_WRITE,

  // The replay data is complete.  No new data can be added now.
  REPLAY_FILE_MODE_COMPLETE
};


// ========================================================================================================================
// TODO: Add the player names + indexes for chat data.  This CAN be blank for single player games, or if you don't care.
struct CGameData
{
  static constexpr int MAX_GAME_NAME_SIZE = 32;
  static constexpr int MAX_VERSION_SIZE = 16;
  static constexpr int MAX_PLAYER_NAME = 32;

  CGameData();

  char GameName[MAX_GAME_NAME_SIZE] = {};
  char GameVersion[MAX_VERSION_SIZE] = {};

  uint16_t MaxPlayerCount = 0;
  uint16_t TotalInputSize = 0;

  void SetGameName(std::string name);
  void SetVersion(std::string version);

  void SetPlayerName(std::string name, uint8_t index);

  // Attempt to get the player's name at the given index.
  // Player names are typically only set in network games, so names may not be available
  // for a normal playback.
  // Returns true if the player name was set on the given index.
  // Returns false if the player name is not set on the given index.
  bool TryGetPlayerName(uint8_t index, std::string& to);

  uint32_t SizeOf();

  void Read(istream& from);
  void Write(ostream& to);

private:
  char PlayerNames[MAX_PLAYER_NAME * GAMEINPUT_MAX_PLAYERS] = {};
  //std::string* PlayerNames = nullptr;
  //void AllocatePlayerNames();
};

// ========================================================================================================================
enum EGameStateType : uint8_t {
  GAMESTATE_TYPE_NONE = 0,         // No game state!
  GAMESTATE_TYPE_FILE,             // The replay data is stored in a file.
  GAMESTATE_TYPE_DATA,             // The replay data is raw data.
};

// ========================================================================================================================
struct CGameState {
  // How / where is the state data stored?
  // Interpretation of the data (where to read files from, data compresssion, etc. are implementation defined).
  EGameStateType Type = GAMESTATE_TYPE_NONE;

  // Frame # of the save state.  If zero, then Size + data should be zero as well as this indicates that the replay starts
  // at system boot.
  uint32_t Frame = 0;

  // How much data is there?  (# of bytes in path, or # of bytes for total gamestate)
  uint32_t DataSize = 0;

  // CRC of data / data in file.
  // Use zero if you don't actually care about a CRC.
  // Interpretation of the CRC is also implementation defined.
  uint32_t CRC = 0;

  // Raw data (array of byte, or a path to the file that contains the state information.
  uint8_t* Data = nullptr;

  uint8_t IsCompressed = 0;

  uint32_t SizeOf() {
    return sizeof(uint8_t)          // Type 
      + (sizeof(uint32_t) * 3)   // Frame, DataSize, CRC 
      + DataSize;
  }

  void Read(istream& from);
  void Write(ostream& to);
};

// ========================================================================================================================
struct CFooterData {
  static constexpr int MSG_SIZE = 64;

  uint32_t Frame;
  uint8_t CompleteReason;
  uint8_t ErrorReason;
  char Message[MSG_SIZE];
  uint64_t FinalFileSize;

  inline ECompletionReason GetCompleteReason() { return (ECompletionReason)CompleteReason; }
  inline EErrorReason GetErrorReason() { return (EErrorReason)ErrorReason; }

  void SetMessage(std::string msg);
  void GetMessage(std::string& msg);

  static constexpr uint32_t SizeOf() { return sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint8_t) + MSG_SIZE + sizeof(uint64_t); }

  void Read(istream& from);
  void Write(istream& to);
};

// ========================================================================================================================
struct CChatData
{
  static constexpr int CHAT_DATA_MAX = 128;

  // NOTE: There is an error if all indexes are the same number!
  uint8_t FromPlayerIndex = 0;
  uint8_t ToPlayerIndex = 0;
  int32_t Frame = 0;

  void Read(istream& from);
  void Write(ostream& to) const;
  inline uint32_t SizeOf() const { return sizeof(uint8_t) + sizeof(uint8_t) + sizeof(int32_t) + sizeof(uint8_t) + DataSize; }

private:
  uint8_t DataSize = 0;
  uint8_t Data[CHAT_DATA_MAX];

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
  CReplayFile(const filesystem::path& path, const CGameData& gameData_, const CGameState* state);
  ~CReplayFile();


  void AddChatSegment(const CChatData& chat);
  void AddInputSegment(const GameInput& input);
  void CompleteReplayFile(int frame, ECompletionReason reason, EErrorReason errReason, const std::string& message);

  // Read to the next recorded input....
  // NOTE: We should be pulling all events up to a certain frame, really.....  How else can we get timed chat data, etc.
  bool GetNextInput(GameInput& input);


  void GetState(CGameState& state);

  // TODO: Share
  static int CopyFixedString(const std::string& data, int maxSize, uint8_t* toBuffer, int offset);

  // Get the total frame count for the file.
  uint32_t TotalFrames() { return _Footer.Frame; }
  uint16_t TotalInputSize() { return _GameData.TotalInputSize; }
  CGameData GameData() { return _GameData; }

private:

  // Used to make some read/write stuff not need to allocate more data.
  CGameData _GameData;
  CGameState _State;

  CFooterData _Footer = {};


  // OPTIONS:  Max # of inputs that can be grouped together.
  const uint16_t MAX_INPUT_GROUP_COUNT = 0x80;
  uint16_t CurInputGroupCount = 0;
  uint32_t InputStartFrame = 0;
  uint8_t* InputGroupBuffer = nullptr;
  size_t InputGroupBufSize = 0;
  uint32_t LastUsedFrame = 0;

  // Index of the input group that we are reading from.
  uint32_t InputGroupReadIndex = 0;


  static const int BUFFER_SIZE = 0x400;
  uint8_t DataBuffer[BUFFER_SIZE];
  EReplayFileMode _Mode;

  std::fstream _Stream;

  uint64_t scratch = 0;

  void Init(const filesystem::path& path, EReplayFileMode mode_);
  void SetupInputDataBuffer();
  void CheckComplete();
  void ReadInputFromBuffer(GameInput& input);
  void FlushPendingInputData();

  void ReadSegmentHeader(CSegmentHeader& header);
  void WriteSegmentHeader(CSegmentHeader& header);

  // Read for / check for a segment header at current read position, but don't
  // move the actual read position.
  void PeekSegmentHeader(CSegmentHeader& header);

  // Writing funcitons:
  void WriteHeader();
  void WriteState();
  void WriteGameData();

  void WriteSegmentData(EDataSegmentType segmentType, stringstream& data);

  // Reading functions:
  void ReadHeader();
  void ReadState();
  void ReadFooter();
  void ReadGameData();

  void Flush();
  void CloseStream();

};
