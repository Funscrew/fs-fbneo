
#ifndef REPLAY_ENDPOINT_H
#define REPLAY_ENDPOINT_H

#include "GGPOEndpoint.h"

class ReplayEndpoint : GGPOEndpoint {


  // Udp* udp, PollManager& p, uint8_t playerIndex_, char* ip, u_short port, UdpMsg::connect_status* status, uint32_t clientVersion, uint8_t delay_, uint8_t runahead_
public:

  ReplayEndpoint(Udp* udp, PollManager& p, uint8_t playerIndex_, char* ip, u_short port, UdpMsg::connect_status* status, uint32_t clientVersion);

  virtual bool OnInput(UdpMsg* msg, int msgLen) override;
  virtual bool OnLoopPoll(void* cookie) override;

private:
  RingBuffer<GameInput, 64> _PendingAcks;
  void SendPendingAcks();
};

#endif
