/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#include "types.h"

static GGPOLogOptions _logOps;
static bool _logActive = false;

static FILE* logHandle = nullptr;
static bool logInitialized = false;

// OBSOLETE:  This is just here to keep the build in order while the logging gets updated!
void Log(const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  // Logv(fmt, args);
  Utils::LogIt_v(fmt, args);
  va_end(args);
}

// ----------------------------------------------------------------------------------------------------------------
// Simple check to see if the given category is active.
// Might get weird if you don't use categories defined in log.h
bool CategoryActive(const char* category) {

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
  if (!CategoryActive(category)) { return; }

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
void Utils::LogEvent(const char* msg, const UdpEvent& evt)
{
  if (!_logActive) { return; }

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
void Utils::LogNetworkStats(int totalBytesSent, int totalPacketsSent, int ping)
{
  if (!_logActive) { return; }

  //int total_bytes_sent = bytesSent + (UDP_HEADER_SIZE * packetsSent);
  // float seconds = (float)((now - _stats_start_time) / 1000.0);
  // float bytes_sec = totalBytesSent / elapsed;

  //float nonOverheadBytes = totalBytesSent - (UDP_HEADER_SIZE * totalPacketsSent);
  //float udp_overhead = 1.0f - (nonOverheadBytes / totalBytesSent);

  // float udp_overhead = (float)(100.0 * (UDP_HEADER_SIZE * packetsSent) / totalPacketsSent);

  // NOTE: A quality report can be computed from all of the data that we log.
  LogIt(CATEGORY_NETWORK, "%d-%d-%d", totalBytesSent, totalPacketsSent, ping);


  //// NOTE: This might be a good place to write some stats..?
  //Log("Network Stats -- Bandwidth: %.2f KBps   Packets Sent: %5d (%.2f pps)   "
  //  "KB Sent: %.2f    UDP Overhead: %.2f pct.\n",
  //  _kbps_sent,
  //  _packets_sent,
  //  (float)_packets_sent * 1000 / (now - _stats_start_time),
  //  total_bytes_sent / 1024.0,
  //  udp_overhead);


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
void Utils::LogIt_v(const char* category, const char* fmt, va_list args) 
{

  if (!_logActive) { return; }


  const size_t BUFFER_SIZE = 1024;
  char buf[BUFFER_SIZE];

  vsnprintf(buf, BUFFER_SIZE - 1, fmt, args);

  // Now we can write the buffer to console / disk....
  // TODO: Do it in hex for less chars?
  fprintf(logHandle, "%d:%s:%s\n", Platform::GetCurrentTimeMS(), category, buf);

  fflush(logHandle);

}

