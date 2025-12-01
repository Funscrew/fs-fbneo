/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#include "network/udp_proto.h"

static GGPOLogOptions _logOps;
static bool _logActive = false;

static FILE* logHandle = nullptr;
static bool logInitialized = false;

// ----------------------------------------------------------------------------------------------------------------
// Simple check to see if the given category is active.
// Might get weird if you don't use categories defined in log.h
bool IsCategoryActive(const char* category) {

  // Log everything.
  if (_logOps.AllowedCategories.length() == 0) { return true; }

  // Log some things.
  int match = _logOps.AllowedCategories.find(category);
  return match != std::string::npos;
}


// ----------------------------------------------------------------------------------------------------------------
void Utils::InitLogger(GGPOLogOptions& options_) {
  if (logInitialized) { throw new std::exception("The log has already been initialized!"); }
  logInitialized = true;

  _logOps = options_;
  _logActive = _logOps.LogToFile;

  // Fire up the log file, if needed....
  if (_logOps.LogToFile) {
    fopen_s(&logHandle, _logOps.FilePath.data(), "w");
  }

  // Write the init message...
  // TODO: Maybe we could add some more information about the current GGPO settings?  delay, etc.?
  Utils::LogIt("INITIALIZED");

}

// ----------------------------------------------------------------------------------------------------------------
void Utils::FlushLog()
{
  if (!_logActive || !logHandle) { return; }
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
  if (!_logActive) { return; }
  if (!IsCategoryActive(category)) { return; }

  va_list args;
  va_start(args, fmt);

  LogIt_v(category, fmt, args);

  va_end(args);
}

// ----------------------------------------------------------------------------------------------------------------
void Utils::LogIt(const char* fmt, ...)
{
  if (!_logActive) { return; }

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
  if (!_logActive) { return; }

  const int MSG_SIZE = 1024;
  char buf[MSG_SIZE];
  memset(buf, 0, MSG_SIZE);

  LogIt(CATEGORY_EVENT, "%d", evt.type);
}

// ----------------------------------------------------------------------------------------------------------------
void Utils::LogNetworkStats(int totalBytesSent, int totalPacketsSent, int ping)
{
  if (!_logActive) { return; }

  LogIt(CATEGORY_NETWORK, "%d-%d-%d", totalBytesSent, totalPacketsSent, ping);
}

// ----------------------------------------------------------------------------------------------------------------
void Utils::LogMsg(const char* direction, UdpMsg* msg)
{
  const int MSG_SIZE = 1024;
  char buf[MSG_SIZE];
  memset(buf, 0, MSG_SIZE);

  // TODO: Add the other data...
  sprintf_s(buf, MSG_SIZE, "%s: %d", direction, msg->header.type);

  LogIt(CATEGORY_MESSAGE, buf);

  // Original....
  switch (msg->header.type) {
  case UdpMsg::SyncRequest:
    LogIt(CATEGORY_MESSAGE,"%d", msg->u.sync_request.random_request);
    break;
  case UdpMsg::SyncReply:
    LogIt(CATEGORY_MESSAGE,"%d", msg->u.sync_reply.random_reply);
    break;
  case UdpMsg::QualityReport:
    break;
  case UdpMsg::QualityReply:
    break;
  case UdpMsg::KeepAlive:
    break;
  case UdpMsg::Input:
    LogIt(CATEGORY_MESSAGE,"%d:%d", msg->u.input.start_frame, msg->u.input.num_bits);
    break;
  case UdpMsg::InputAck:
    break;
  case UdpMsg::ChatCommand:
    break;

  default:
    ASSERT(false && "Unknown UdpMsg type.");
  }
}

// ----------------------------------------------------------------------------------------------------------------
void Utils::LogIt_v(const char* category, const char* fmt, va_list args)
{

  if (!_logActive) { return; }


  const size_t BUFFER_SIZE = 1024;
  char buf[BUFFER_SIZE];

  vsnprintf(buf, BUFFER_SIZE - 1, fmt, args);

  // Now we can write the buffer to console / disk....
  // TODO: Do it in hex for less chars?
  fprintf(logHandle, "%d|%s|%s\n", Platform::GetCurrentTimeMS(), category, buf);

  fflush(logHandle);

}

