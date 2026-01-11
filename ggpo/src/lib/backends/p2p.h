/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#ifndef _P2P_H
#define _P2P_H

#include "types.h"
#include "poll.h"
#include "sync.h"
#include "GGPOSession.h"
#include "timesync.h"
#include "network/udp_proto.h"
#include <string>

 // TEMP:  Using assumed player and input sizes for now.
static const uint16 PLAYER_COUNT = 2;

// NOTE: This is the input size that 3s uses.  We should not have a hard-coded way of doing this,
// or we should actually probably just change the way we do the asserts....
static const uint16 INPUT_SIZE = 5;

// ==========================================================================================================
class Peer2PeerBackend : public GGPOSession, IPollSink, Udp::Callbacks {
public:
   Peer2PeerBackend(GGPOSessionCallbacks *cb, const char *gamename, uint16 localport, char* remoteIp, uint16 remotePort, uint8_t playerIndex, std::string playerName, uint32_t client_version, char* replayIp, uint16 replayPort);
   virtual ~Peer2PeerBackend();


public:
   virtual GGPOErrorCode DoPoll(int timeout);
   virtual GGPOErrorCode AddPlayer(GGPOPlayer *player);
   virtual GGPOErrorCode AddLocalInput(uint8_t playerIndex, void *values, int totalSize);
   virtual GGPOErrorCode SyncInput(void *values, int totalSize, int playerCount);
   virtual GGPOErrorCode IncrementFrame(void);
   virtual GGPOErrorCode DisconnectPlayer(uint8_t playerIndex);
   virtual void DisconnectEx();
   virtual bool GetNetworkStats(GGPONetworkStats *stats, uint8_t playerIndex);
   virtual void SetFrameDelay(int delay);
   virtual GGPOErrorCode SetDisconnectTimeout(int timeout);
   virtual GGPOErrorCode SetDisconnectNotifyStart(int timeout);

   virtual bool SendChat(char* text);
   virtual bool SendData(UINT8 command, void* data, UINT8 dataSize);

public:
   virtual void OnMsg(sockaddr_in &from, UdpMsg *msg, int len);

protected:
   void DisconnectPlayer(uint8_t playerIndex, int syncto);
   void PollUdpProtocolEvents(void);
   void CheckInitialSync(void);
   int Poll2Players(int current_frame);
   int PollNPlayers(int current_frame);
   
   void AddReplayEndpoint(char* remoteIp, uint16 remotePort);
   void AddRemotePlayer(char *remoteIp, uint16 remotePort, int playerIndex);
   
   virtual void OnUdpProtocolEvent(UdpEvent &e, uint8_t playerIndex);
   virtual void OnUdpProtocolPeerEvent(UdpEvent &e, uint8_t playerIndex);

   // OBSOLETE:  These functions don't actually do anything....
   void PollSyncEvents(void);
   virtual void OnSyncEvent(Sync::Event& e) {}

protected:
   GGPOSessionCallbacks  _callbacks;
   PollManager           _pollMgr;
   Sync                  _sync;
   Udp                   _udp;
   UdpProtocol           *_endpoints;

   int                   _input_size;

   bool                  _synchronizing;
   uint8_t               _num_players;
   int                   _next_recommended_sleep;

   int                   _disconnect_timeout;
   int                   _disconnect_notify_start;
   uint32_t              _client_version;
   uint8_t               _endpointCount;
   bool                  _sendsReplayData = false;

   UdpMsg::connect_status _local_connect_status[UDP_MSG_MAX_PLAYERS];


};

#endif
