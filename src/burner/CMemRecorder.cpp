#include "CMemRecorder.h"
#include "EZStream.h"

// ----------------------------------------------------------------------------------------------------------
CMemRecorder::CMemRecorder(const filesystem::path& path, ERecordOptions options_)
{
  Options = options_;
  IsRead = false;

  // Let's populate our header.....

  // TODO: Open a stream for write.
  InitStream(path);
}

// ----------------------------------------------------------------------------------------------------------
CMemRecorder::~CMemRecorder() {
  CloseStream();
}

// ------------------------------------------------------------------------------------------------------------------------
void CMemRecorder::InitStream(const filesystem::path& path) {

  auto openMode = std::ios::binary |
    IsRead ? std::ios::in : (std::ios::out | std::ios::trunc);

  _Stream.open(path, openMode);
  if (IsRead) {
    Header.Read(_Stream);
  }
  else {
    Header.Write(_Stream);
  }
}

// ------------------------------------------------------------------------------------------------------------------------
void CMemRecorder::AddMemory(int frame, void* data, size_t dataSize) {
  if (frame != TotalFrames + 1) { throw runtime_error("Invalid frame #!"); }

  WDATA2(_Stream, frame);
  WDATA2(_Stream, dataSize);

  _Stream.write(reinterpret_cast<char*>(data), dataSize);
}

// ------------------------------------------------------------------------------------------------------------------------
void CMemRecorder::CloseStream()
{
  if (!IsClosed) {

    // Update the total number of written frames:
    _Stream.seekp(CMemRecordHeader::FrameCountOffset, ios::beg);
    WDATA2(_Stream, TotalFrames);

    _Stream.close();
    IsClosed = true;
  }
}



// ------------------------------------------------------------------------------------------------------------------------
void CMemRecordHeader::Write(ostream& to) {

  to.write(CMemRecordHeader::HEADER, sizeof(CMemRecordHeader::HEADER));
  EZStream::Write(to, Version);
  EZStream::Write(to, TotalFrames);
}

// ------------------------------------------------------------------------------------------------------------------------
void CMemRecordHeader::Read(const istream& from) {

  throw runtime_error("not implemented!");

}
