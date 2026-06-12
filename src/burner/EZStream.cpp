#include "EZStream.h"

// ----------------------------------------------------------------------------------------------------
void EZStream::Write(ostream& stream, const uint8_t data)
{
  stream.write(reinterpret_cast<const char*>(&data), static_cast<streamsize>(1));
}

// ----------------------------------------------------------------------------------------------------
void EZStream::Write(ostream& stream, const uint8_t* data, size_t size) {
  stream.write(reinterpret_cast<const char*>(data), static_cast<streamsize>(size));
}

// ----------------------------------------------------------------------------------------------------
void EZStream::Write(ostream& stream, const vector<uint8_t>& bytes)
{
  stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<streamsize>(bytes.size()));
}

// ----------------------------------------------------------------------------------------------------
void EZStream::WriteRawString(ostream& stream, const string& value)
{
  stream.write(value.data(), static_cast<streamsize>(value.size()));
}

// ----------------------------------------------------------------------------------------------------
void EZStream::ReadRawString(istream& stream, string& value, size_t size)
{
  char* buffer = new char[size + 1];
  stream.read(buffer, size);
  buffer[size] = 0;

  value.assign(buffer);
  delete[](buffer);
}


// ----------------------------------------------------------------------------------------------------
void EZStream::ReadBytes(istream& stream, uint8_t* buffer, int count) {
  //if (stream.tellg() + count > stream.) {
  //  throw std::runtime_error("can't read past end of stream!");
  //}
  stream.read(reinterpret_cast<char*>(buffer), count);
}

// ----------------------------------------------------------------------------------------------------
uint8_t EZStream::ReadUint8(istream& stream) {
  uint8_t res = 0;
  stream.read(reinterpret_cast<char*>(&res), 1);
  return res;
}
