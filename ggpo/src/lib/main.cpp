/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#include "types.h"
#include "backends/p2p.h"
#include "backends/synctest.h"
#include "backends/spectator.h"
#include "ggponet.h"

BOOL WINAPI
DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
  srand(Platform::GetCurrentTimeMS() + Platform::GetProcessID());
  return TRUE;
}

void
ggpo_log(GGPOSession* ggpo, const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  ggpo_logv(ggpo, fmt, args);
  va_end(args);
}

void
ggpo_logv(GGPOSession* ggpo, const char* fmt, va_list args)
{
  if (ggpo) {
    ggpo->Logv(fmt, args);
  }
}

GGPOSession* ggpo_start_session(
  GGPOSessionCallbacks* cb,
  const char* gameName,
  uint16 localPort,
  char* remoteIp,
  uint16 remotePort,
  uint8_t playerIndex,
  char* playerName,
  uint32_t clientVersion,
  char* replayIp,
  uint16_t replayPort,
  uint64_t sessionId)
{
  auto res = (GGPOSession*)new Peer2PeerBackend(cb,
    gameName,
    localPort,
    remoteIp,
    remotePort,
    playerIndex,
    playerName,
    clientVersion,
    replayIp,
    replayPort,
    sessionId);
  return res;
}


// NOTE: We may not need this function call outside of the lib....
GGPOErrorCode ggpo_add_player(GGPOSession* ggpo, GGPOPlayer* player)
{
  if (!ggpo) {
    return GGPO_ERRORCODE_INVALID_SESSION;
  }
  return ggpo->AddPlayer(player);
}



GGPOErrorCode
ggpo_start_synctest(GGPOSession** ggpo,
  GGPOSessionCallbacks* cb,
  char* game,
  int num_players,
  int input_size,
  int frames)
{
  return GGPO_ERRORCODE_UNSUPPORTED;
}

void ggpo_set_frame_delay(GGPOSession* ggpo, int frame_delay, int runahead) {
  ggpo->SetFrameDelay(frame_delay, runahead);
}

// ----------------------------------------------------------------------------------------------------------------
GGPOErrorCode
ggpo_idle(GGPOSession* ggpo, int timeout)
{
  if (!ggpo) {
    return GGPO_ERRORCODE_INVALID_SESSION;
  }
  return ggpo->DoPoll(timeout);
}

// ----------------------------------------------------------------------------------------------------------------
GGPOErrorCode ggpo_synchronize_input(GGPOSession* ggpo,
  void* values,
  int isize,
  int playerCount)
{
  if (!ggpo) {
    return GGPO_ERRORCODE_INVALID_SESSION;
  }
  return ggpo->SyncInput(values, isize, playerCount);
}

// ----------------------------------------------------------------------------------------------------------------
void ggpo_disconnect(GGPOSession* ggpo)
{
  if (!ggpo) {
    return;
  }
  return ggpo->DisconnectEx();
}

//// ----------------------------------------------------------------------------------------------------------------
//GGPOErrorCode ggpo_disconnect_player(GGPOSession* ggpo, uint8_t playerIndex)
//{
//  if (!ggpo) {
//    return GGPO_ERRORCODE_INVALID_SESSION;
//  }
//  return ggpo->DisconnectPlayer(playerIndex);
//}

// ----------------------------------------------------------------------------------------------------------------
GGPOErrorCode ggpo_advance_frame(GGPOSession* ggpo)
{
  if (!ggpo) { return GGPO_ERRORCODE_INVALID_SESSION; }
  return ggpo->IncrementFrame();
}

// ----------------------------------------------------------------------------------------------------------------
// NOTE: ChatCommand is not actually implemented in the open source GGPO code.
// We will have to figure this out, but shouldn't be too hard.....
bool ggpo_send_chat(GGPOSession* ggpo, char* text)
{
  if (!ggpo) { return false; }
  bool res = ggpo->SendChat(text);
  return res;
}

// ----------------------------------------------------------------------------------------------------------------
void ggpo_send_data(GGPOSession* ggpo, uint8_t code, void* data, uint8_t dataSize) {
  if (!ggpo) { return; }

  ggpo->SendData(code, data, dataSize);
}

// ----------------------------------------------------------------------------------------------------------------
bool ggpo_get_stats(GGPOSession* ggpo, GGPONetworkStats* stats, uint8_t playerIndex)
{
  if (!ggpo) {
    return false;
  }
  bool res = ggpo->GetNetworkStats(stats, playerIndex);
  return res;
}

// ----------------------------------------------------------------------------------------------------------------
char* ggpo_get_playerName(GGPOSession* ggpo, uint8_t playerIndex) {
  if (!ggpo) { return nullptr; }
  auto res = ggpo->GetPlayerName(playerIndex);
  return res;
}

GGPOErrorCode
ggpo_close_session(GGPOSession* ggpo)
{
  if (!ggpo) {
    return GGPO_ERRORCODE_INVALID_SESSION;
  }
  delete ggpo;
  return GGPO_OK;
}

GGPOErrorCode
ggpo_set_disconnect_timeout(GGPOSession* ggpo, int timeout)
{
  if (!ggpo) {
    return GGPO_ERRORCODE_INVALID_SESSION;
  }
  return ggpo->SetDisconnectTimeout(timeout);
}

GGPOErrorCode
ggpo_set_disconnect_notify_start(GGPOSession* ggpo, int timeout)
{
  if (!ggpo) {
    return GGPO_ERRORCODE_INVALID_SESSION;
  }
  return ggpo->SetDisconnectNotifyStart(timeout);
}
//
//GGPOErrorCode ggpo_start_spectating(GGPOSession **session,
//                                    GGPOSessionCallbacks *cb,
//                                    const char *game,
//                                    int num_players,
//                                    int input_size,
//                                    unsigned short local_port,
//                                    char *host_ip,
//                                    unsigned short host_port)
//{
//   *session= (GGPOSession *)new SpectatorBackend(cb,
//                                                 game,
//                                                 local_port,
//                                                 num_players,
//                                                 input_size,
//                                                 host_ip,
//                                                 host_port);
//   return GGPO_OK;
//}
//
