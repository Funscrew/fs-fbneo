/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#ifndef _SPECTATOR_H
#define _SPECTATOR_H

#include "types.h"
#include "poll.h"
#include "sync.h"
#include "GGPOSession.h"
#include "timesync.h"
#include "network/udp_proto.h"

#define SPECTATOR_FRAME_BUFFER_SIZE    64

class SpectatorBackend : public GGPOSession, IPollSink, Udp::Callbacks {
public:
   SpectatorBackend(GGPOSessionCallbacks *cb, const char *gamename, uint16 localport, int num_players, int input_size, char *hostip, u_short hostport);
   virtual ~SpectatorBackend();


public:
   virtual GGPOErrorCode DoPoll(int timeout);
   virtual GGPOErrorCode AddPlayer(GGPOPlayer *player, PlayerID *handle) { return GGPO_ERRORCODE_UNSUPPORTED; }
   virtual GGPOErrorCode AddLocalInput(PlayerID player, void *values, int size) { return GGPO_OK; }
   virtual GGPOErrorCode SyncInput(void *values, int size, int *disconnect_flags);
   virtual GGPOErrorCode IncrementFrame(void);
   virtual GGPOErrorCode DisconnectPlayer(PlayerID handle) { return GGPO_ERRORCODE_UNSUPPORTED; }
   virtual bool GetNetworkStats(GGPONetworkStats *stats, PlayerID handle) { return false ; }
   virtual GGPOErrorCode SetFrameDelay(PlayerID player, int delay) { return GGPO_ERRORCODE_UNSUPPORTED; }
   virtual GGPOErrorCode SetDisconnectTimeout(int timeout) { return GGPO_ERRORCODE_UNSUPPORTED; }
   virtual GGPOErrorCode SetDisconnectNotifyStart(int timeout) { return GGPO_ERRORCODE_UNSUPPORTED; }

public:
   virtual void OnMsg(sockaddr_in &from, UdpMsg *msg, int len);

protected:
   void PollUdpProtocolEvents(void);
   void CheckInitialSync(void);

   void OnUdpProtocolEvent(UdpProtocol::Event &e);

protected:
   GGPOSessionCallbacks  _callbacks;
   PollManager                  _pollMgr;
   Udp                   _udp;
   UdpProtocol           _host;
   bool                  _synchronizing;
   int                   _input_size;
   int                   _num_players;
   int                   _next_input_to_send;
   GameInput             _inputs[SPECTATOR_FRAME_BUFFER_SIZE];
};

#endif
