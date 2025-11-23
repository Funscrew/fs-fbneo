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

#ifndef _LOG_H
#define _LOG_H

 // =======================================================================================
struct GGPOLogOptions {
  bool LogToFile = false;
  bool LogToConsole = false;
  std::string FilePath;
};


// =======================================================================================
class LogUtil {

public:
  LogUtil(GGPOLogOptions& options_);
  //  Options = options_;
  //}

  //void Log(std::string msg, UdpProtocol::UdpEvent& evt);

private:
  GGPOLogOptions Options;
};

//
//extern LogUtil* Logger = nullptr;
//extern void InitLogger(GGPOLogOptions& logOps) {
//  if (Logger != nullptr) {
//    throw std::exception("GGPO logger is already initialized!");
//  }
//
//  Logger = new LogUtil(logOps);
//}
//
//}

extern void Log(const char* fmt, ...);
extern void Logv(const char* fmt, va_list list);
extern void Logv(FILE* fp, const char* fmt, va_list args);
extern void LogFlush();
extern void LogFlushOnLog(bool flush);

#endif
