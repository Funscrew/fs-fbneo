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
  virtual ~GGPOSession() {}
  virtual GGPOErrorCode DoPoll(int timeout) { return GGPO_OK; }
  virtual GGPOErrorCode AddPlayer(GGPOPlayer* player) = 0;
  virtual GGPOErrorCode AddLocalInput(uint8_t playerIndex, void* values, int totalSize) = 0;
  virtual GGPOErrorCode SyncInput(void* values, int totalSize, int playerCount) = 0;
  virtual GGPOErrorCode IncrementFrame(void) { return GGPO_OK; }

  virtual bool SendChat(char* text) { return true; }
  virtual bool SendData(uint8_t code, void* data, uint8_t dataSize) { throw std::exception("NOT IMPLEMENTED!"); }

  virtual GGPOErrorCode DisconnectPlayer(uint8_t handle) { return GGPO_OK; }
  virtual void DisconnectEx() { throw std::exception("NOT IMPLEMENTED!"); }

  virtual bool GetNetworkStats(GGPONetworkStats* stats, uint8_t playerIndex) { return GGPO_OK; }
  virtual GGPOErrorCode Logv(const char* fmt, va_list list) { Utils::LogIt_v(fmt, list); return GGPO_OK; }

  virtual void SetFrameDelay(int delay, int runahead) { throw std::exception("not supported!"); }
  virtual GGPOErrorCode SetDisconnectTimeout(int timeout) { return GGPO_ERRORCODE_UNSUPPORTED; }
  virtual GGPOErrorCode SetDisconnectNotifyStart(int timeout) { return GGPO_ERRORCODE_UNSUPPORTED; }

  char* GetPlayerName(UINT16 index) { return _PlayerNames[index]; }

  // Additions:
protected:
  IN_ADDR _RemoteAddr;
  uint16_t _RemotePort = 0;
  uint8_t _playerIndex = 0;

  char _PlayerNames[2][MAX_NAME_SIZE];

};

#endif

