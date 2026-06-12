#pragma once

#include <stdint.h>
#include <fstream>
#include <vector>

using namespace std;

#ifndef EZSTREAM_H
#define EZSTREAM_H

#define WDATA(x) EZStream::Write(_Stream, x)
#define RDATA(x) EZStream::Read(_Stream, x);
#define WDATA2(stream, x) EZStream::Write(stream, x)
#define RDATA2(stream, x) EZStream::Read(stream, x);

namespace EZStream
{

  template <typename T>
  void Write(ostream& stream, const T& value)
  {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
  }

  template <typename T>
  void Read(istream& stream, T& value)
  {
    stream.read(reinterpret_cast<char*>(&value), sizeof(T));
  }

  void Write(ostream& stream, const uint8_t data);
  void Write(ostream& stream, const uint8_t* data, size_t size);
  void Write(ostream& stream, const vector<uint8_t>& bytes);
  void WriteRawString(ostream& stream, const string& value);
  void ReadRawString(istream& stream, string& value, size_t size);
  void ReadBytes(istream& stream, uint8_t* buffer, int count);
  uint8_t ReadUint8(istream& stream); 

}

#endif
