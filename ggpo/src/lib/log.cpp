/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#include "types.h"

using namespace Utils;

static FILE* logfile = nullptr;
static FILE* logHandle = nullptr;
//
//void LogFlush()
//{
//  if (logfile) {
//    fflush(logfile);
//  }
//}

static char logbuf[4 * 1024 * 1024];

void Log(const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  Logv(fmt, args);
  va_end(args);
}

void Logv(const char* fmt, va_list args)
{
  //if (!Platform::GetConfigBool(L"ggpo.log") || Platform::GetConfigBool(L"ggpo.log.ignore")) {
  //  return;
  //}
  if (!logfile) {
    sprintf_s(logbuf, ARRAY_SIZE(logbuf), "log-%d.log", Platform::GetProcessID());
    fopen_s(&logfile, logbuf, "w");
  }
  Logv(logfile, fmt, args);
}

void Logv(FILE* fp, const char* fmt, va_list args)
{
  if (Platform::GetConfigBool(L"ggpo.log.timestamps")) {
    static int start = 0;
    int t = 0;
    if (!start) {
      start = Platform::GetCurrentTimeMS();
    }
    else {
      t = Platform::GetCurrentTimeMS() - start;
    }
    fprintf(fp, "%d.%03d : ", t / 1000, t % 1000);
  }

  vfprintf(fp, fmt, args);
  fflush(fp);

  vsprintf_s(logbuf, ARRAY_SIZE(logbuf), fmt, args);
}


// ----------------------------------------------------------------------------------------------------------------
void Utils::InitLogger(GGPOLogOptions& options_) {
  _logOps = options_;
  _logActive = _logOps.LogToFile;

  // Fire up the log file, if needed....
  if (_logOps.LogToFile) {
    fopen_s(&logHandle, _logOps.FilePath.data(), "w");
  }

  // Write the init message...
  Utils::LogIt("INITIALIZED");

}


// ----------------------------------------------------------------------------------------------------------------
void Utils::LogIt(const char* category, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);

  LogIt_v(category, fmt, args);

  va_end(args);
}


// ----------------------------------------------------------------------------------------------------------------
void Utils::LogIt(const char* fmt, ...) {

  va_list args;
  va_start(args, fmt);

  LogIt(CATEGORY_GENERAL, fmt, args);

  va_end(args);

}

// ----------------------------------------------------------------------------------------------------------------
void Utils::LogEvent(const char* msg, const UdpEvent& evt)
{
  const int MSG_SIZE = 1024;
  char buf[MSG_SIZE];
  memset(buf, 0, MSG_SIZE);

  // TODO: Add some more information....

  sprintf_s(buf, MSG_SIZE, "%s:", msg);

  LogIt(CATEGORY_EVENT, buf);

  // Original:
    //switch (evt.type) {
    //case UdpEvent::Synchronized:
    //  Log("%s (event: Synchronized).\n", prefix);
    //  break;
    //}

}

// ----------------------------------------------------------------------------------------------------------------
void Utils::LogMsg(const char* direction, UdpMsg* msg)
{
  const int MSG_SIZE = 1024;
  char buf[MSG_SIZE];
  memset(buf, 0, MSG_SIZE);

  // TODO: Add the other data...
  sprintf_s(buf, MSG_SIZE, "%s:", direction);

  LogIt(CATEGORY_MESSAGE, buf);

  // Original....
  //switch (msg->header.type) {
  //case UdpMsg::SyncRequest:
  //  Log("%s sync-request (%d).\n", prefix,
  //    msg->u.sync_request.random_request);
  //  break;
  //case UdpMsg::SyncReply:
  //  Log("%s sync-reply (%d).\n", prefix,
  //    msg->u.sync_reply.random_reply);
  //  break;
  //case UdpMsg::QualityReport:
  //  Log("%s quality report.\n", prefix);
  //  break;
  //case UdpMsg::QualityReply:
  //  Log("%s quality reply.\n", prefix);
  //  break;
  //case UdpMsg::KeepAlive:
  //  Log("%s keep alive.\n", prefix);
  //  break;
  //case UdpMsg::Input:
  //  Log("%s game-compressed-input %d (+ %d bits).\n", prefix, msg->u.input.start_frame, msg->u.input.num_bits);
  //  break;
  //case UdpMsg::InputAck:
  //  Log("%s input ack.\n", prefix);
  //  break;

  //case UdpMsg::ChatCommand:
  //  Log("%s chat.\n", prefix);
  //  break;

  //default:
  //  ASSERT(FALSE && "Unknown UdpMsg type.");
  //}
}

// ----------------------------------------------------------------------------------------------------------------
void Utils::LogIt_v(const char* category, const char* fmt, va_list args) {

  if (!_logActive) { return; }

  const size_t BUFFER_SIZE = 1024;
  char buf[BUFFER_SIZE];

  vsnprintf(buf, BUFFER_SIZE - 1, fmt, args);

  // Now we can write the buffer to console / disk....
  // TODO: Do it in hex for less chars?
  fprintf(logHandle, "%d:%s:%s\n", Platform::GetCurrentTimeMS(), category, buf);
  // fprintf(logHandle, buf);

}

// ----------------------------------------------------------------------------------------------------------------
void Log(const std::string msg, UdpEvent& event)
{
  if (!_logActive) { return; }
  // _ggpoLogger.Log(msg);
}
