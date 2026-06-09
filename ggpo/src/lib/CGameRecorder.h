#pragma once

#include <cstdint>
#include <string>
#include <array>
#include <fstream>
#include <memory>
//#include "game_input.h"
#include "ring_buffer.h"
#include "EZQ.h"
#include "EZRing.h"

#include "CReplayFile.h"

struct GameInput;
using namespace std;



// ========================================================================================================================
class EZWriterEx {
private:
  ostream* _Stream = nullptr;

public:
  EZWriterEx(ostream* toStream_);

  void Write(const uint8_t data);

  template <typename T>
  void Write(ostream& stream, const T& value);
};



// ========================================================================================================================
class CGameRecorder
{
private:
  // uint64_t SessionId = 0;
  // string DataDir;

  EZRing<GameInput, 64> MergedInputs;

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

  CReplayFile* _File = nullptr;

  void Init(const filesystem::path& toPath, bool overwriteExisting);



public:
  //  string FilePath;

  bool RecordingComplete = false;
  string ErrorMessage;
  EErrorReason ErrorReason = EErrorReason::None;

  CGameRecorder(const CGameData& gameData_, const string& dataDir, uint64_t sessionId, bool overwriteExisting = false);
  CGameRecorder(const CGameData& gameData_, const string& toPath, bool overwriteExisting);
  ~CGameRecorder();

  bool HasError() const;

  //  void Flush();
  void CompleteReplay(int frame, ECompletionReason reason, EErrorReason errReason, const string& message);
 // void CompleteReplay(int frame, ECompletionReason reason, EErrorReason errReason, const char* message);

  void AddChatSegment(CChatData& chat);

  // Add a complete set of inputs, no guessing.
  bool AddInputs(int frame, uint8_t* data, int dataSize);

  // Add an input from a single player.
  bool AddInput(int playerIndex, GameInput& input);

private:
  //void CloseStream();
  //void CreateStream(const string& path);
  //void WriteHeader(ostream& res);
  //void CheckComplete();

  //void WriteInputSegment(GameInput& input);
  //void WriteGameDataSegment(const CGameData& gameData);
  void WriteSegmentData(EDataSegmentType segmentType, stringstream& fromStream);

  //int CopyFixedString(
  //  const string& data,
  //  int maxSize,
  //  uint8_t* toBuffer,
  //  int offset);

  void OnError(EErrorReason errReason, const string& message);
  void MergeInputs();
};