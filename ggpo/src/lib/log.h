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
static const char* CATEGORY_MESSAGE = "MSG";
static const char* CATEGORY_ENDPOINT = "EP";
static const char* CATEGORY_EVENT = "EVT";
static const char* CATEGORY_SYNC = "SYNC";
static const char* CATEGORY_RUNNING = "RUN";
static const char* CATEGORY_CONNECTION = "CONN";
static const char* CATEGORY_ERROR = "ERR";
static const char* CATEGORY_NETWORK = "NET";
static const char* CATEGORY_INPUT = "INP";
static const char* CATEGORY_TEST = "TEST";
static const char* CATEGORY_UDP = "UDP";
static const char* CATEGORY_INPUT_QUEUE = "INPQ";
static const char* CATEGORY_TIMESYNC = "TIME";

// This happens when we attempt to retrieve an input from 'SynchronizeInputs' but frame data
// from the remote is not available.  It isn't a real input, and may get rolled back!
static const char* CATEGORY_PREDICTED_INPUT = "PI";

static const int LOG_VERSION = 1;

// =======================================================================================
struct GGPOLogOptions {
  bool LogToFile = false;
  std::string FilePath;

  // Comma delimited list of categories that will be logged.  All others will be ignored.
  // If empty, all categories will be logged.
  std::string ActiveCategories;
};

// =======================================================================================
enum EMsgDirection {
  Send = 0,
  Receive = 1
};


namespace Utils {


  void InitLogger(GGPOLogOptions& options_);

  void LogIt(const char* category, const char* fmt, ...);
  void LogIt_v(const char* category, const char* fmt, va_list args);
  void LogIt(const char* fmt, ...);
  void LogIt_v(const char* fmt, va_list args);

  void LogMsg(EMsgDirection dir, UdpMsg* msg);
  void LogEvent(const UdpEvent& evt);

  void LogNetworkStats(int totalBytesSent, int totalPacketsSent, int ping);

  void FlushLog();
  void CloseLog();

}


#endif
