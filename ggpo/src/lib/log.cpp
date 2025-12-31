/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#include "network/udp_proto.h"

static GGPOLogOptions _logOps;
static bool _isLogActive = false;

static FILE* logHandle = nullptr;
static bool logInitialized = false;

// ----------------------------------------------------------------------------------------------------------------
// Simple check to see if the given category is active.
// Might get weird if you don't use categories defined in log.h
bool IsCategoryActive(const char* category) {

  // Log everything.
  if (_logOps.ActiveCategories.length() == 0) { return true; }

  // Log some things.
  int match = _logOps.ActiveCategories.find(category);
  return match != std::string::npos;
}


// ----------------------------------------------------------------------------------------------------------------
void Utils::InitLogger(GGPOLogOptions& options_) {
  if (logInitialized) { throw new std::exception("The log has already been initialized!"); }
  logInitialized = true;

  _logOps = options_;
  _isLogActive = _logOps.LogToFile;

  // Fire up the log file, if needed....
  if (_logOps.LogToFile) {
    fopen_s(&logHandle, _logOps.FilePath.data(), "w");

    // Write the init message...
    // TODO: Maybe we could add some more information about the current GGPO settings?  delay, etc.?
    fprintf(logHandle, "# GGPO-LOG\n");
    fprintf(logHandle, "# VERSION:%d\n", LOG_VERSION);

    size_t len = _logOps.ActiveCategories.length();
    fprintf(logHandle, "# ACTIVE: %s\n", len == 0 ? "[ALL]" : _logOps.ActiveCategories.data());
    fprintf(logHandle, "# START:%d\n", Platform::GetCurrentTimeMS());

    if (logHandle == nullptr) {
      throw std::exception("could not open log file!");
    }
  }

  
}

// ----------------------------------------------------------------------------------------------------------------
void Utils::FlushLog()
{
  if (!_isLogActive || !logHandle) { return; }
  fflush(logHandle);
}

// ----------------------------------------------------------------------------------------------------------------
void Utils::CloseLog()
{
  if (logHandle) {
    FlushLog();
    fclose(logHandle);
    logHandle = nullptr;
  }
}

// ----------------------------------------------------------------------------------------------------------------
void Utils::LogIt(const char* category, const char* fmt, ...)
{
  if (!_isLogActive || !IsCategoryActive(category)) { return; }

  va_list args;
  va_start(args, fmt);

  LogIt_v(category, fmt, args);

  va_end(args);
}

// ----------------------------------------------------------------------------------------------------------------
void Utils::LogIt(const char* fmt, ...)
{
  if (!_isLogActive) { return; }

  va_list args;
  va_start(args, fmt);
  LogIt_v(fmt, args);
  va_end(args);
}

// ----------------------------------------------------------------------------------------------------------------
void Utils::LogIt_v(const char* fmt, va_list args)
{
  LogIt(CATEGORY_GENERAL, fmt, args);
}

// ----------------------------------------------------------------------------------------------------------------
void Utils::LogEvent(const UdpEvent& evt)
{
  if (!_isLogActive || !IsCategoryActive(CATEGORY_EVENT)) { return; }

  const int MSG_SIZE = 1024;
  char buf[MSG_SIZE];
  memset(buf, 0, MSG_SIZE);

  LogIt(CATEGORY_EVENT, "%d", evt.type);
}

// ----------------------------------------------------------------------------------------------------------------
void Utils::LogNetworkStats(int totalBytesSent, int totalPacketsSent, int ping)
{
  if (!_isLogActive || !IsCategoryActive(CATEGORY_NETWORK)) { return; }

  LogIt(CATEGORY_NETWORK, "%d:%d:%d", totalBytesSent, totalPacketsSent, ping);
}

// ----------------------------------------------------------------------------------------------------------------
void Utils::LogMsg(EMsgDirection dir, UdpMsg* msg)
{
  if (!_isLogActive || !IsCategoryActive(CATEGORY_MESSAGE)) { return; }

  const int MSG_BUF_SIZE = 1024;
  char msgBuf[MSG_BUF_SIZE];

  // TODO: Is there a way be can log the sequenced?
  int pos = sprintf_s(msgBuf, MSG_BUF_SIZE, "%d:%d:%d", dir, msg->header.type, msg->header.sequence_number);


  // Original....
  switch (msg->header.type) {
  case UdpMsg::SyncRequest:
    pos += sprintf_s(msgBuf + pos, MSG_BUF_SIZE - pos - 1, ":%d", msg->u.sync_request.random_request);
    break;

  case UdpMsg::SyncReply:
    pos += sprintf_s(msgBuf + pos, MSG_BUF_SIZE - pos - 1, ":%d", msg->u.sync_reply.random_reply);
    break;

  case UdpMsg::Input:
    pos += sprintf_s(msgBuf + pos, MSG_BUF_SIZE - pos - 1, ":%d:%d", msg->u.input.start_frame, msg->u.input.num_bits);
    break;

  case UdpMsg::QualityReport:
    break;
  case UdpMsg::QualityReply:
    break;
  case UdpMsg::KeepAlive:
    break;
  case UdpMsg::InputAck:
    break;
  case UdpMsg::Datagram:
    break;

  default:
    ASSERT(false && "Unknown UdpMsg type.");
    break;
  }


  LogIt(CATEGORY_MESSAGE, "%s", msgBuf);
}

// ----------------------------------------------------------------------------------------------------------------
void Utils::LogIt_v(const char* category, const char* fmt, va_list args)
{

  if (!_isLogActive) { return; }


  const size_t BUFFER_SIZE = 1024;
  char buf[BUFFER_SIZE];

  vsnprintf(buf, BUFFER_SIZE - 1, fmt, args);

  // Now we can write the buffer to console / disk....
  // TODO: Do it in hex for less chars?
  fprintf(logHandle, "%d|%s|%s\n", Platform::GetCurrentTimeMS(), category, buf);

  fflush(logHandle);

}

