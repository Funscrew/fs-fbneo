#pragma once

#include <cstdint>
#include <string>
#include <array>
#include <fstream>
#include <memory>
#include "game_input.h"
#include "ring_buffer.h"
#include "EZQ.h"

struct GameInput;
using namespace std;

// template <typename T>;

enum class EErrorReason
{
  None = 0,
  InputBufferFull
};

enum class ECompletionReason
{
  Invalid = 0,
  NormalDisconnect,
  Error
};

enum class EDataSegmentType
{
  Invalid = 0,
  GameData,
  InputData,
  ChatData,
  Complete
};

class ChatData
{
public:
  static constexpr int CHAT_DATA_MAX = 128;

  int FromPlayerIndex = 0;
  int Frame = 0;
  string Message;
  int ToPlayerIndex = -1;
};

class CGameData
{
public:
  static constexpr int MAX_GAME_NAME_SIZE = 32;
  static constexpr int MAX_VERSION_SIZE = 16;

  string GameName;
  string GameVersion = "<n/a>";

  int PlayerCount = 0;
  int TotalInputSize = 0;

  static constexpr uint16_t DataSize = MAX_GAME_NAME_SIZE + MAX_VERSION_SIZE + sizeof(int) + sizeof(int);
};

class EZWriterEx {
private:
  ostream* _Stream = nullptr;

public:
  EZWriterEx(ostream* toStream_);

  void Write(const uint8_t data);

  template <typename T>
  void Write(ostream& stream, const T& value);
};

class GameRecorder
{
private:
  // uint64_t SessionId = 0;
  // string DataDir;

  RingBuffer<GameInput, 64> MergedInputs;

  static constexpr int PLAYER_INPUT_BUFFER_SIZE = 0x70;
  static constexpr int MAX_PLAYERS = 2;

  int SyncedBaseFrame = 0;

  // array<unique_ptr<EZQ<GameInput>>, MAX_PLAYERS> PlayerBuffers;
  EZQ<GameInput, PLAYER_INPUT_BUFFER_SIZE> PlayerBuffers[MAX_PLAYERS];

  array<int, MAX_PLAYERS> BaseFrames;
  array<GameInput, MAX_PLAYERS> MergeBuffer;

  ofstream DataStream;
  CGameData GameData;

  array<uint8_t, 0x800> WriteBuffer = {};

  void Init(const filesystem::path& toPath, bool overwriteExisting);

public:
  string FilePath;

  bool RecordingComplete = false;
  string ErrorMessage;
  EErrorReason ErrorReason = EErrorReason::None;

  GameRecorder(const CGameData& gameData_, const string& dataDir, uint64_t sessionId, bool overwriteExisting = false);
  GameRecorder(const CGameData& gameData_, const string& toPath, bool overwriteExisting);


  ~GameRecorder();

  bool HasError() const;

  void Flush();

  void CompleteReplay(
    int frame,
    ECompletionReason reason,
    EErrorReason errReason,
    const string& message);

  void CompleteReplay(
    int frame,
    ECompletionReason reason,
    EErrorReason errReason,
    const char* message);

  void AddChatSegment(ChatData& chat);

  // Add a complete set of inputs, no guessing.
  bool AddInputs(int frame, uint8_t* data, int dataSize);

  // Add an input from a single player.
  bool AddInput(int playerIndex, GameInput& input);

private:
  void CloseStream();
  void CreateStream(const string& path);
  void WriteHeader(ostream& res);
  void CheckComplete();

  void WriteInputSegment(GameInput& input);
  void WriteGameDataSegment(const CGameData& gameData);
  void WriteSegmentData(EDataSegmentType segmentType, stringstream& fromStream);

  int CopyFixedString(
    const string& data,
    int maxSize,
    uint8_t* toBuffer,
    int offset);

  void OnError(EErrorReason errReason, const string& message);
  void MergeInputs();
};