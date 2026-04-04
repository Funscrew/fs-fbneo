#include "ReplayEndpoint.h"


// --------------------------------------------------------------------------------------------------------------------------  
ReplayEndpoint::ReplayEndpoint(Udp* udp, PollManager& p, uint8_t playerIndex_, char* ip, u_short port, UdpMsg::connect_status* status, uint32_t clientVersion)
  : GGPOEndpoint()
{
  Init(udp, p, playerIndex_, ip, port, status, clientVersion, 0, 0);
  // this.Appliance = this.Client as ReplayAppliance;
}


// ----------------------------------------------------------------------------------------------------------
bool ReplayEndpoint::OnLoopPoll(void* cookie)
{
  bool res = GGPOEndpoint::OnLoopPoll(cookie);
  SendPendingAcks();

  return res;
}

// ----------------------------------------------------------------------------------------------------------
bool ReplayEndpoint::OnInput(UdpMsg* msg, int msgLen)
{

  // The replay client needs to do that same thing as the normal endpoint by keeping
  // a set of ACKS to send back to the client. 
  // In this case our parent is going to be a replay appliance so we may need to keep a separate list
  // of stuff that needs to be acked / resolved on our end.....
  bool res = GGPOEndpoint::OnInput(msg, msgLen);

  // Housekeeping.  We can get rid of all confirmed acks.
  // TODO: I'd like to log the size of these ring buffers to see what is typical.  Is there really a certain amount of 'overdraw' in the system always?
  while (_PendingAcks.size() > 0 && _PendingAcks.front().frame < msg->u.input.ack_frame)
  {
    Utils::LogIt(CATEGORY_INPUT, "ACK: Throwing away pending ACK frame %d", _PendingAcks.front().frame);
    _last_acked_input = _PendingAcks.front();
    _PendingAcks.pop();

    // NOTE: C++ client version will never have a ref to the replay appliance.
    // NOTE: As iof 3/29/2026 there is no C++ replay appliance anyway!
    //if (this.Appliance != null)
    //{
    //  this.Appliance.MergeInput(ref _last_acked_input, this.PlayerIndex);
    //}
  }

  return res;
}

// ----------------------------------------------------------------------------------------------------------
void ReplayEndpoint::SendPendingAcks()
{
  return;
  // int x = 10;

  //// GameInput last;
  //// NEW:
  //// We will collect all of the pending acks and send a message for each:
  //// In the future we can combine them all into a single message to ACK mulitple inputs.
  //if (_PendingAcks.size() > 0)
  //{
  //  auto last = _last_acked_input;
  //  auto front = _PendingAcks.front();
  //  ASSERT(last.frame == -1 || last.frame + 1 == front.frame);

  //  for (int i = 0; i < _PendingAcks.size(); i++)
  //  {
  //    auto ack = _PendingAcks.item(i);

  //    auto msg = new UdpMsg(UdpMsg::MsgType::InputAck);
  //    msg->u.input_ack.ack_frame = ack.frame;

  //    SendMsg(msg);
  //  }
  //}

}
