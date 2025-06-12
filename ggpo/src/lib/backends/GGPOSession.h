/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#ifndef _BACKEND_H
#define _BACKEND_H

#include "ggponet.h"
#include "types.h"
#include <string>


struct GGPOSession {
   virtual ~GGPOSession() { }
   virtual GGPOErrorCode DoPoll(int timeout) { return GGPO_OK; }
   virtual GGPOErrorCode AddPlayer(GGPOPlayer *player) = 0;
   virtual GGPOErrorCode AddLocalInput(PlayerID playerIndex, void *values, int totalSize) = 0;
   virtual GGPOErrorCode SyncInput(void *values, int totalSize, int playerCount) = 0;
   virtual GGPOErrorCode IncrementFrame(void) { return GGPO_OK; }
   virtual bool ChatCommand(char *text) { return true; }
   virtual GGPOErrorCode DisconnectPlayer(PlayerID handle) { return GGPO_OK; }
   virtual bool GetNetworkStats(GGPONetworkStats *stats) { return GGPO_OK; }
   virtual GGPOErrorCode Logv(const char *fmt, va_list list) { ::Logv(fmt, list); return GGPO_OK; }

   virtual uint32 SetFrameDelay(int delay) { return GGPO_ERRORCODE_UNSUPPORTED; }
   virtual GGPOErrorCode SetDisconnectTimeout(int timeout) { return GGPO_ERRORCODE_UNSUPPORTED; }
   virtual GGPOErrorCode SetDisconnectNotifyStart(int timeout) { return GGPO_ERRORCODE_UNSUPPORTED; }

   char* GetPlayerName(UINT16 index) { return _PlayerNames[index]; }

   // Additions:
protected:
	IN_ADDR _RemoteAddr;
	uint16 _RemotePort = 0;
  PlayerID _playerIndex = 0;

  char _PlayerNames[2][MAX_NAME_SIZE];

};

#endif

