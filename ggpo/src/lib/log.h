/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#include <string>
 // #include "network/udp_proto.h"

 //struct UdpProtocol::Event;
 //class UdpProtocol;
 //struct UdpProtocol::Event;

struct UdpEvent;
struct UdpMsg;

#ifndef _LOG_H
#define _LOG_H

static const char* CATEGORY_GENERAL = "NA";


// =======================================================================================
struct GGPOLogOptions {
  bool LogToFile = false;
  std::string FilePath;
};

namespace Utils {

  static GGPOLogOptions _logOps;
  static bool _logActive = false;
  void InitLogger(GGPOLogOptions& options_);
  // void FlushLog();

//  extern void Log(const std::string msg);
  extern void LogIt(const char* category, const char* fmt, va_list args);
  extern void LogIt(const char* fmt, ...);
//  extern void LogIt(const std::string fmt, va_list args);
  extern void Log(const std::string msg, UdpEvent& event);
}


// LEGACY:
extern void Log(const char* fmt, ...);
extern void Logv(const char* fmt, va_list list);
extern void Logv(FILE* fp, const char* fmt, va_list args);
//extern void LogFlush();
//extern void LogFlushOnLog(bool flush);

#endif
