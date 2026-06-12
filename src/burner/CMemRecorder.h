#pragma once

#include <filesystem>
#include <fstream>
#include <stdint.h>

using namespace std;

// ===================================================================================================
enum class ERecordOptions : uint8_t {
  Invalid = 0,
  All
};

// ===================================================================================================
struct CMemRecordHeader {

  static constexpr const char HEADER[] = { 'f', 's', 'm', 'e', 'm' };
  static constexpr uint32_t FrameCountOffset = sizeof(HEADER);

  uint8_t Version = 0;
  uint32_t TotalFrames = 0;

  // TODO: We can care about versions later....

  void Write(ostream& to);
  void Read(const istream& from);
};

// ===================================================================================================
// Records emulator memory for analysis and other purposes.
class CMemRecorder {

public:
  // Open for write.
  CMemRecorder(const filesystem::path& path, ERecordOptions options_);
  ~CMemRecorder();

  // Add the memory for one frame.
  void AddMemory(int frame, void* data, size_t dataSize);


private:

  bool IsRead = true;
  bool IsClosed = false;

  CMemRecordHeader Header;
  int TotalFrames = 0;

  std::fstream _Stream;
  ERecordOptions Options = ERecordOptions::Invalid;


  void InitStream(const filesystem::path& path);
  void CloseStream();

};

