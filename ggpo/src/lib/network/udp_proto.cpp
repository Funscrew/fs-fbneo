/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#include "types.h"
#include "udp_proto.h"
#include "bitvector.h"

 // OPTIONS: These should all be configurable at some point....
static const int SYNC_PACKETS_COUNT = 5;
static const int SYNC_RETRY_INTERVAL = 2000;
static const int SYNC_FIRST_RETRY_INTERVAL = 500;
static const int RUNNING_RETRY_INTERVAL = 200;
static const int KEEP_ALIVE_INTERVAL = 200;
static const int QUALITY_REPORT_INTERVAL = 1000;
static const int NETWORK_STATS_INTERVAL = 1000;
static const int UDP_SHUTDOWN_TIMER = 5000;
static const int MAX_SEQ_DISTANCE = (1 << 15);

UdpProtocol::UdpProtocol() :
  _local_frame_advantage(0),
  _remote_frame_advantage(0),
  _queue(-1),
  _magic_number(0),
  _remote_magic_number(0),
  _packets_sent(0),
  _bytes_sent(0),
  _stats_start_time(0),
  _last_send_time(0),
  _shutdown_timeout(0),
  _disconnect_timeout(0),
  _disconnect_notify_start(0),
  _disconnect_notify_sent(false),
  _disconnect_event_sent(false),
  _connected(false),
  _next_send_seq(0),
  _next_recv_seq(0),
  _udp(NULL)
{
  _last_sent_input.init(-1, NULL, 1);
  _last_received_input.init(-1, NULL, 1);
  _last_acked_input.init(-1, NULL, 1);

  memset(&_state, 0, sizeof _state);
  memset(_peer_connect_status, 0, sizeof(_peer_connect_status));
  for (int i = 0; i < ARRAY_SIZE(_peer_connect_status); i++) {
    _peer_connect_status[i].last_frame = -1;
  }
  memset(&_peer_addr, 0, sizeof _peer_addr);
  _oo_packet.msg = NULL;

  _send_latency = Platform::GetConfigInt(L"ggpo.network.delay");
  _oop_percent = Platform::GetConfigInt(L"ggpo.oop.percent");

  memset(_playerName, 0, MAX_NAME_SIZE);
}

// ----------------------------------------------------------------------------------------------------------
void UdpProtocol::SetPlayerName(char* playerName_) {
  strcpy_s(_playerName, playerName_);
}

// ----------------------------------------------------------------------------------------------------------
UdpProtocol::~UdpProtocol()
{
  ClearSendQueue();
}

// ----------------------------------------------------------------------------------------------------------
void UdpProtocol::Init(Udp* udp,
  PollManager& poll,
  int queue,
  char* ip,
  u_short port,
  UdpMsg::connect_status* status)
{
  _udp = udp;
  _queue = queue;
  _local_connect_status = status;

  _peer_addr.sin_family = AF_INET;
  _peer_addr.sin_port = htons(port);
  inet_pton(AF_INET, ip, &_peer_addr.sin_addr.s_addr);

  do {
    _magic_number = (uint16)rand();
  } while (_magic_number == 0);
  poll.RegisterLoopSink(this);
}

// ----------------------------------------------------------------------------------------------------------
void UdpProtocol::SendData(uint8_t code, void* data, uint8_t dataSize) {
  if (_udp && _current_state == Running) {

    UdpMsg* msg = new UdpMsg(UdpMsg::DataExchange);

    msg->u.chat.code = code;
    msg->u.chat.dataSize = dataSize;
    if (data != nullptr) {
      memcpy_s(msg->u.chat.data, MAX_GGPO_DATA_SIZE, data, dataSize);
    }

    SendMsg(msg);
  }
}

// ----------------------------------------------------------------------------------------------------------
void UdpProtocol::SendChat(char* text) {

  if (_udp && _current_state == Running) {
    size_t len = strnlen_s(text, MAX_GGPO_DATA_SIZE);
    SendData('T', text, (uint8_t)len);
  }
}

// ----------------------------------------------------------------------------------------------------------
void UdpProtocol::SendInput(GameInput& input)
{
  if (_udp) {
    if (_current_state == Running) {
      /*
       * Check to see if this is a good time to adjust for the rift...
       */
      _timesync.rollback_frame(input, _local_frame_advantage, _remote_frame_advantage);

      /*
       * Save this input packet
       *
       * XXX: This queue may fill up for spectators who do not ack input packets in a timely
       * manner.  When this happens, we can either resize the queue (ug) or disconnect them
       * (better, but still ug).  For the meantime, make this queue really big to decrease
       * the odds of this happening...
       */
      _pending_output.push(input);
    }
    SendPendingOutput();
  }
}

// ----------------------------------------------------------------------------------------------------------
void UdpProtocol::SendPendingOutput()
{
  // This assert is checking consts.  Probably don't need to do this each time....
    // Can probably do it on program init....
  ASSERT((GAMEINPUT_MAX_BYTES * GAMEINPUT_MAX_PLAYERS * 8) < (1 << BITVECTOR_NIBBLE_SIZE));

  UdpMsg* msg = new UdpMsg(UdpMsg::Input);
  int offset = 0;

  uint8* bits;
  GameInput last;

  if (_pending_output.size()) {
    last = _last_acked_input;
    bits = msg->u.input.bits;

    msg->u.input.start_frame = _pending_output.front().frame;
    msg->u.input.input_size = (uint8)_pending_output.front().size;

    ASSERT(last.frame == -1 || last.frame + 1 == msg->u.input.start_frame);

    // This is the 'compression'.
    // I'm thinking at some point we let the end user decide how they want to do it?
    // Anyway, for now, what this does is it takes all of the pending outputs,
    // and squishes all of the bits into a single vector of bytes....
    for (int i = 0; i < _pending_output.size(); i++) {
      GameInput& current = _pending_output.item(i);

      // Only update the message if the data is different.
      if (memcmp(current.bits, last.bits, current.size) != 0) {

        for (int j = 0; j < current.size * 8; j++) {
          ASSERT(j < (1 << BITVECTOR_NIBBLE_SIZE));

          if (current.value(j) != last.value(j)) {
            BitVector_SetBit(msg->u.input.bits, &offset);
            (current.value(j) ? BitVector_SetBit : BitVector_ClearBit)(bits, &offset);
            BitVector_WriteNibblet(bits, j, &offset);
          }
        }
      }
      BitVector_ClearBit(msg->u.input.bits, &offset);
      last = _last_sent_input = current;
    }
  }
  else {
    msg->u.input.start_frame = 0;
    msg->u.input.input_size = 0;
  }
  msg->u.input.ack_frame = _last_received_input.frame;
  msg->u.input.num_bits = (uint16)offset;

  msg->u.input.disconnect_requested = _current_state == Disconnected;
  if (_local_connect_status) {
    // This pointer is set on init!  
    // It is NULL in the case of spectators, which we really dont use at this time....
    // for the C# implementation, we can consider it always set... maybe use a different flag or something....
    memcpy(msg->u.input.peer_connect_status, _local_connect_status, sizeof(UdpMsg::connect_status) * UDP_MSG_MAX_PLAYERS);
  }
  else {
    memset(msg->u.input.peer_connect_status, 0, sizeof(UdpMsg::connect_status) * UDP_MSG_MAX_PLAYERS);
  }

  ASSERT(offset < MAX_COMPRESSED_BITS);

  SendMsg(msg);
}

// ----------------------------------------------------------------------------------------------------------
void UdpProtocol::SendInputAck()
{
  UdpMsg* msg = new UdpMsg(UdpMsg::InputAck);
  msg->u.input_ack.ack_frame = _last_received_input.frame;
  SendMsg(msg);
}

// ----------------------------------------------------------------------------------------------------------
bool UdpProtocol::GetEvent(UdpEvent& e)
{
  if (_event_queue.size() == 0) {
    return false;
  }
  e = _event_queue.front();
  _event_queue.pop();
  return true;
}


// ------------------------------------------------------------------------------------------
// REFACTOR: Rename to 'DoPoll' or 'Poll' or whatever.
bool UdpProtocol::OnLoopPoll(void* cookie)
{
  if (!_udp) {
    return true;
  }

  unsigned int now = Platform::GetCurrentTimeMS();
  unsigned int next_interval;

  // Send outgoing messages.
  PumpSendQueue();

  switch (_current_state) {
  case Syncing:
    next_interval = (_state.sync.roundtrips_remaining == SYNC_PACKETS_COUNT) ? SYNC_FIRST_RETRY_INTERVAL : SYNC_RETRY_INTERVAL;
    if (_last_send_time && _last_send_time + next_interval < now) {
      Utils::LogIt(CATEGORY_SYNC, "Re-Queueing Sync"); //  "No luck syncing after %d ms... Re-queueing sync packet.\n", next_interval);
      SendSyncRequest();
    }
    break;

  case Running:
    // xxx: rig all this up with a timer wrapper
    if (!_state.running.last_input_packet_recv_time || _state.running.last_input_packet_recv_time + RUNNING_RETRY_INTERVAL < now) {
      Utils::LogIt(CATEGORY_CONNECTION, "Haven't exchanged packets in a while (last received:%d  last sent:%d).  Resending.", _last_received_input.frame, _last_sent_input.frame);
      SendPendingOutput();
      _state.running.last_input_packet_recv_time = now;
    }

    if (!_state.running.last_quality_report_time || _state.running.last_quality_report_time + QUALITY_REPORT_INTERVAL < now) {
      UdpMsg* msg = new UdpMsg(UdpMsg::QualityReport);
      msg->u.quality_report.ping = Platform::GetCurrentTimeMS();
      msg->u.quality_report.frame_advantage = (uint8)_local_frame_advantage;
      SendMsg(msg);
      _state.running.last_quality_report_time = now;
    }

    if (!_state.running.last_network_stats_interval || _state.running.last_network_stats_interval + NETWORK_STATS_INTERVAL < now) {
      UpdateNetworkStats();
      _state.running.last_network_stats_interval = now;
    }

    if (_last_send_time && _last_send_time + KEEP_ALIVE_INTERVAL < now) {
      // TODO: This gets logged again in SendMsg.  I think we should review other messages to make sure we aren't logging
      // what is essentially the same thing twice..
      // Utils::LogIt(CATEGORY_CONNECTION, "Keepalive");
      SendMsg(new UdpMsg(UdpMsg::KeepAlive));
    }

    if (_disconnect_timeout && _disconnect_notify_start && !_disconnect_notify_sent && (_last_recv_time + _disconnect_notify_start < now)) {
      Utils::LogIt(CATEGORY_CONNECTION, "Endpoint has stopped receiving packets for %d ms.  Sending notification.", _disconnect_notify_start);

      UdpEvent e(UdpEvent::NetworkInterrupted);
      e.u.network_interrupted.disconnect_timeout = _disconnect_timeout - _disconnect_notify_start;
      QueueEvent(e);
      _disconnect_notify_sent = true;
    }

    if (_disconnect_timeout && (_last_recv_time + _disconnect_timeout < now)) {
      if (!_disconnect_event_sent) {
        Utils::LogIt(CATEGORY_CONNECTION, "Endpoint has stopped receiving packets for %d ms.  Disconnecting.", _disconnect_timeout);
        QueueEvent(UdpEvent(UdpEvent::Disconnected));
        _disconnect_event_sent = true;
      }
    }
    break;

  case Disconnected:
    if (_shutdown_timeout < now) {
      Utils::LogIt(CATEGORY_CONNECTION, "Disconnect");
      _udp = NULL;
      _shutdown_timeout = 0;
    }

  }


  return true;
}

// ------------------------------------------------------------------------------------------------
void UdpProtocol::Disconnect()
{
  _current_state = Disconnected;
  _shutdown_timeout = Platform::GetCurrentTimeMS() + UDP_SHUTDOWN_TIMER;
}

// ------------------------------------------------------------------------------------------------
void UdpProtocol::SendSyncRequest()
{
  _state.sync.random = rand() & 0xFFFF;
  UdpMsg* msg = new UdpMsg(UdpMsg::SyncRequest);
  msg->u.sync_request.random_request = _state.sync.random;
  SendMsg(msg);
}

// ------------------------------------------------------------------------------------------------
void UdpProtocol::SendMsg(UdpMsg* msg)
{
  _packets_sent++;
  _last_send_time = Platform::GetCurrentTimeMS();
  _bytes_sent += msg->PacketSize();

  msg->header.magic = _magic_number;
  msg->header.sequence_number = _next_send_seq++;

  _send_queue.push(QueueEntry(Platform::GetCurrentTimeMS(), _peer_addr, msg));

  Utils::LogMsg(EMsgDirection::Send, msg);
  PumpSendQueue();
}

// ------------------------------------------------------------------------------------------------
bool UdpProtocol::HandlesMsg(sockaddr_in& from, UdpMsg* msg)
{
  if (!_udp) {
    return false;
  }

  // NOTE: Pretty sure this will always be true.....
  return _peer_addr.sin_addr.S_un.S_addr == from.sin_addr.S_un.S_addr &&
    _peer_addr.sin_port == from.sin_port;
}

// ------------------------------------------------------------------------------------------------
void UdpProtocol::OnMsg(UdpMsg* msg, int len)
{
  bool handled = false;
  typedef bool (UdpProtocol::* DispatchFn)(UdpMsg* msg, int len);

  // NOTE:  A table of function pointers is used so that the values from UdpMsg::MsgType
  // can be used to index into this.
  static const DispatchFn msgHandlers[] = {
     &UdpProtocol::OnInvalid,             /* Invalid */
     &UdpProtocol::OnSyncRequest,         /* SyncRequest */
     &UdpProtocol::OnSyncReply,           /* SyncReply */
     &UdpProtocol::OnInput,               /* Input */
     &UdpProtocol::OnQualityReport,       /* QualityReport */
     &UdpProtocol::OnQualityReply,        /* QualityReply */
     &UdpProtocol::OnKeepAlive,           /* KeepAlive */
     &UdpProtocol::OnInputAck,            /* InputAck */
     &UdpProtocol::OnData					        /* Data Exchange - A chat message or other type of data was received */
  };

  // Filter out messages that don't match what we expect
  uint16 seq = msg->header.sequence_number;
  if (msg->header.type != UdpMsg::SyncRequest && msg->header.type != UdpMsg::SyncReply) {
    if (msg->header.magic != _remote_magic_number) {
      Utils::LogIt(CATEGORY_MESSAGE, "magic-mismatch");
      return;
    }

    // filter out out-of-order packets
    uint16 skipped = (uint16)((int)seq - (int)_next_recv_seq);
    if (skipped > MAX_SEQ_DISTANCE) {
      Utils::LogIt(CATEGORY_ENDPOINT, "OOP dropped: (seq: %d, last seq:%d)", seq, _next_recv_seq);
      return;
    }
  }

  _next_recv_seq = seq;
  Utils::LogMsg(EMsgDirection::Receive, msg);
  if (msg->header.type >= ARRAY_SIZE(msgHandlers)) {
    OnInvalid(msg, len);
  }
  else {
    handled = (this->*(msgHandlers[msg->header.type]))(msg, len);
  }
  if (handled) {
    _last_recv_time = Platform::GetCurrentTimeMS();
    if (_disconnect_notify_sent && _current_state == Running) {
      QueueEvent(UdpEvent(UdpEvent::NetworkResumed));
      _disconnect_notify_sent = false;
    }
  }
}

// ----------------------------------------------------------------------------------------------------------
void UdpProtocol::UpdateNetworkStats(void)
{
  UINT32 now = Platform::GetCurrentTimeMS();

  if (_stats_start_time == 0) {
    _stats_start_time = now;
  }

  int total_bytes_sent = _bytes_sent + (UDP_HEADER_SIZE * _packets_sent);
  float seconds = (float)((now - _stats_start_time) / 1000.0);
  float bytes_sec = total_bytes_sent / seconds;
  //  float udp_overhead = (float)(100.0 * (UDP_HEADER_SIZE * _packets_sent) / _bytes_sent);

  _kbps_sent = (int)(bytes_sec / 1024);

  // TODO: Include rollback counts....
  Utils::LogNetworkStats(total_bytes_sent, _packets_sent, _round_trip_time);
}

// ----------------------------------------------------------------------------------------------------------
void UdpProtocol::QueueEvent(const UdpEvent& evt)
{
  Utils::LogEvent(evt);
  _event_queue.push(evt);
}

// ----------------------------------------------------------------------------------------------------------
void UdpProtocol::Synchronize()
{
  if (_udp) {
    _current_state = Syncing;
    _state.sync.roundtrips_remaining = SYNC_PACKETS_COUNT;
    SendSyncRequest();
  }
}

bool
UdpProtocol::GetPeerConnectStatus(int id, int* frame)
{
  *frame = _peer_connect_status[id].last_frame;
  return !_peer_connect_status[id].disconnected;
}

// ----------------------------------------------------------------------------------------
bool UdpProtocol::OnInvalid(UdpMsg* msg, int len)
{
  ASSERT(FALSE && "Invalid msg in UdpProtocol");
  return false;
}

// ----------------------------------------------------------------------------------------
bool UdpProtocol::OnSyncRequest(UdpMsg* msg, int len)
{
  if (_remote_magic_number != 0 && msg->header.magic != _remote_magic_number) {
    Utils::LogIt(CATEGORY_SYNC, "SyncRequest from unknown endpoint :%d != %d)", msg->header.magic, _remote_magic_number);
    return false;
  }
  UdpMsg* reply = new UdpMsg(UdpMsg::SyncReply);
  reply->u.sync_reply.random_reply = msg->u.sync_request.random_request;

  strcpy_s(reply->u.sync_reply.playerName, _playerName);

  SendMsg(reply);
  return true;
}

// ----------------------------------------------------------------------------------------
bool UdpProtocol::OnSyncReply(UdpMsg* msg, int len)
{
  if (_current_state != Syncing) {
    Utils::LogIt(CATEGORY_SYNC, "SyncReply while not synching");
    return msg->header.magic == _remote_magic_number;
  }

  if (msg->u.sync_reply.random_reply != _state.sync.random) {
    Utils::LogIt(CATEGORY_SYNC, "mismatched reply: %d != %d", msg->u.sync_reply.random_reply, _state.sync.random);
    return false;
  }

  if (!_connected) {
    auto evt = UdpEvent(UdpEvent::Connected);
    strcpy_s(evt.u.connected.playerName, msg->u.sync_reply.playerName);
    QueueEvent(evt);

    _connected = true;
  }

  Utils::LogIt(CATEGORY_SYNC, "%d round trips remaining", _state.sync.roundtrips_remaining);
  if (--_state.sync.roundtrips_remaining == 0) {
    QueueEvent(UdpEvent(UdpEvent::Synchronized));
    _current_state = Running;
    _last_received_input.frame = -1;
    _remote_magic_number = msg->header.magic;
  }
  else {
    UdpEvent evt(UdpEvent::Synchronizing);
    evt.u.synchronizing.total = SYNC_PACKETS_COUNT;
    evt.u.synchronizing.count = SYNC_PACKETS_COUNT - _state.sync.roundtrips_remaining;
    QueueEvent(evt);
    SendSyncRequest();
  }
  return true;
}

// ----------------------------------------------------------------------------------------------------------
bool UdpProtocol::OnData(UdpMsg* msg, int dataSize)
{
  UdpEvent evt(UdpEvent::DataExchange);
  auto dataLen = (uint8_t)(dataSize - sizeof(UdpMsg::header));

  evt.u.chat.code = msg->u.chat.code;
  evt.u.chat.dataSize = msg->u.chat.dataSize;
  memcpy_s(evt.u.chat.data, MAX_GGPO_DATA_SIZE, msg->u.chat.data, dataLen);

  QueueEvent(evt);

  return true;
}

// ----------------------------------------------------------------------------------------------------------
bool UdpProtocol::OnInput(UdpMsg* msg, int len)
{
  // If a disconnect is requested, go ahead and disconnect now.
  bool disconnect_requested = msg->u.input.disconnect_requested;
  if (disconnect_requested) {
    if (_current_state != Disconnected && !_disconnect_event_sent) {
      QueueEvent(UdpEvent(UdpEvent::Disconnected));
      _disconnect_event_sent = true;
    }
  }
  else {
    // Update the peer connection status if this peer is still considered to be part
    // of the network.
    UdpMsg::connect_status* remote_status = msg->u.input.peer_connect_status;
    for (int i = 0; i < ARRAY_SIZE(_peer_connect_status); i++) {
      ASSERT(remote_status[i].last_frame >= _peer_connect_status[i].last_frame);
      _peer_connect_status[i].disconnected = _peer_connect_status[i].disconnected || remote_status[i].disconnected;
      _peer_connect_status[i].last_frame = MAX(_peer_connect_status[i].last_frame, remote_status[i].last_frame);
    }
  }

  // TODO: Application should be responsible for compress / decompress.
  // Decompress the input.
  int last_received_frame_number = _last_received_input.frame;
  if (msg->u.input.num_bits) {
    int offset = 0;
    uint8* bits = (uint8*)msg->u.input.bits;
    int numBits = msg->u.input.num_bits;
    int currentFrame = msg->u.input.start_frame;

    _last_received_input.size = msg->u.input.input_size;
    if (_last_received_input.frame < 0) {
      _last_received_input.frame = msg->u.input.start_frame - 1;
    }
    while (offset < numBits) {
      /*
       * Keep walking through the frames (parsing bits) until we reach
       * the inputs for the frame right after the one we're on.
       */
      ASSERT(currentFrame <= (_last_received_input.frame + 1));
      bool useInputs = currentFrame == _last_received_input.frame + 1;

      while (BitVector_ReadBit(bits, &offset)) {
        int on = BitVector_ReadBit(bits, &offset);
        int button = BitVector_ReadNibblet(bits, &offset);
        if (useInputs) {
          if (on) {
            _last_received_input.set(button);
          }
          else {
            _last_received_input.clear(button);
          }
        }
      }
      ASSERT(offset <= numBits);

      /*
       * Now if we want to use these inputs, go ahead and send them to
       * the emulator.
       */
      if (useInputs) {
        /*
         * Move forward 1 frame in the stream.
         */
        ASSERT(currentFrame == _last_received_input.frame + 1);
        _last_received_input.frame = currentFrame;

        // Send the accepted input event.
        UdpEvent evt(UdpEvent::Input);
        evt.u.input.input = _last_received_input;

        //const int DESC_SIZE = 1024;
        //char desc[DESC_SIZE];
        // _last_received_input.desc(desc, DESC_SIZE);

        _state.running.last_input_packet_recv_time = Platform::GetCurrentTimeMS();

        // Log("Sending frame %d to emu queue %d (%s).\n", _last_received_input.frame, _queue, desc);
        QueueEvent(evt);

      }
      else {
        Utils::LogIt(CATEGORY_INPUT, "Skip:%d (%d)", currentFrame, _last_received_input.frame);
      }

      /*
       * Move forward 1 frame in the input stream.
       */
      currentFrame++;
    }
  }

  ASSERT(_last_received_input.frame >= last_received_frame_number);

  /*
   * Get rid of our buffered input
   */
  while (_pending_output.size() && _pending_output.front().frame < msg->u.input.ack_frame) {
    Utils::LogIt(CATEGORY_INPUT, "Nixed output:%d", _pending_output.front().frame);
    _last_acked_input = _pending_output.front();
    _pending_output.pop();
  }
  return true;
}


// ----------------------------------------------------------------------------------------------------------
// NOTE: This comes from the spectator listener...
bool UdpProtocol::OnInputAck(UdpMsg* msg, int len)
{
  // Get rid of our buffered input
  while (_pending_output.size() &&
    _pending_output.front().frame < msg->u.input_ack.ack_frame) {
    Utils::LogIt(CATEGORY_INPUT, "ACK: Throwing away pending output frame %d", _pending_output.front().frame);
    _last_acked_input = _pending_output.front();
    _pending_output.pop();
  }
  return true;
}

// ----------------------------------------------------------------------------------------------------------
bool UdpProtocol::OnQualityReport(UdpMsg* msg, int len)
{
  // send a reply so the other side can compute the round trip transmit time.
  UdpMsg* reply = new UdpMsg(UdpMsg::QualityReply);
  reply->u.quality_reply.pong = msg->u.quality_report.ping;
  SendMsg(reply);

  _remote_frame_advantage = msg->u.quality_report.frame_advantage;
  return true;
}

// ----------------------------------------------------------------------------------------------------------
bool UdpProtocol::OnQualityReply(UdpMsg* msg, int len)
{
  _round_trip_time = Platform::GetCurrentTimeMS() - msg->u.quality_reply.pong;
  return true;
}

// ----------------------------------------------------------------------------------------------------------
bool UdpProtocol::OnKeepAlive(UdpMsg* msg, int len)
{
  // Yep, we just say OK!
  return true;
}

// ----------------------------------------------------------------------------------------------------------
void UdpProtocol::GetNetworkStats(struct GGPONetworkStats* s)
{
  // TODO: Include rollback stats here too!
  s->network.ping = _round_trip_time;
  s->network.send_queue_len = _pending_output.size();
  s->network.kbps_sent = _kbps_sent;
  s->timesync.remote_frames_behind = _remote_frame_advantage;
  s->timesync.local_frames_behind = _local_frame_advantage;
}

// ----------------------------------------------------------------------------------------------------------
void UdpProtocol::SetLocalFrameNumber(int localFrame)
{
  /*
   * Estimate which frame the other guy is one by looking at the
   * last frame they gave us plus some delta for the one-way packet
   * trip time.
   */
  int remoteFrame = _last_received_input.frame + (_round_trip_time * 60 / 1000);

  /*
   * Our frame advantage is how many frames *behind* the other guy
   * we are.  Counter-intuative, I know.  It's an advantage because
   * it means they'll have to predict more often and our moves will
   * pop more frequenetly.
   */
  _local_frame_advantage = remoteFrame - localFrame;
}

// ----------------------------------------------------------------------------------------------------------
int UdpProtocol::RecommendFrameDelay()
{
  // XXX: require idle input should be a configuration parameter
  return _timesync.recommend_frame_wait_duration(false);
}


// ----------------------------------------------------------------------------------------------------------
void UdpProtocol::SetDisconnectTimeout(int timeout)
{
  _disconnect_timeout = timeout;
}

// ----------------------------------------------------------------------------------------------------------
void UdpProtocol::SetDisconnectNotifyStart(int timeout)
{
  _disconnect_notify_start = timeout;
}

// ----------------------------------------------------------------------------------------------------------
void UdpProtocol::PumpSendQueue()
{
  while (!_send_queue.empty())
  {
    QueueEntry& entry = _send_queue.front();

    // NOTE: If latency is set, then messages may not be sent this time around.
    // Like that of '_oop_percent' (below) this is probably useful for testing scenarios
    // to simulate jitter and out of order packets.
    if (_send_latency) {
      // should really come up with a gaussian distribution based on the configured
      // value, but this will do for now.
      int jitter = (_send_latency * 2 / 3) + ((rand() % _send_latency) / 3);
      if (Platform::GetCurrentTimeMS() < _send_queue.front().queue_time + jitter) {
        break;
      }
    }

    // NOTE: I believe that the purpose of this is for simulating out of order packets in
    // test scenarios.  In this case, the current entry is set as the OO packet which will
    // get sent at a later time (see below ~ line 823)
    if (_oop_percent && !_oo_packet.msg && ((rand() % 100) < _oop_percent)) {
      int delay = rand() % (_send_latency * 10 + 1000);
      Utils::LogIt(CATEGORY_TEST, "creating rogue oop (seq: %d  delay: %d)", entry.msg->header.sequence_number, delay);
      _oo_packet.send_time = Platform::GetCurrentTimeMS() + delay;
      _oo_packet.msg = entry.msg;
      _oo_packet.dest_addr = entry.dest_addr;
    }
    else
    {
      ASSERT(entry.dest_addr.sin_addr.s_addr);

      int packetSize = entry.msg->PacketSize();
      _udp->SendTo((char*)entry.msg, packetSize, 0,
        (struct sockaddr*)&entry.dest_addr, sizeof entry.dest_addr);

      delete entry.msg;
    }
    _send_queue.pop();
  }

  if (_oo_packet.msg && _oo_packet.send_time < Platform::GetCurrentTimeMS()) {
    Utils::LogIt(CATEGORY_TEST, "sending rogue oop!");

    int packetSize = _oo_packet.msg->PacketSize();
    _udp->SendTo((char*)_oo_packet.msg, packetSize, 0,
      (struct sockaddr*)&_oo_packet.dest_addr, sizeof _oo_packet.dest_addr);

    delete _oo_packet.msg;
    _oo_packet.msg = NULL;
  }
}

// ----------------------------------------------------------------------------------------------------------
void UdpProtocol::ClearSendQueue()
{
  while (!_send_queue.empty()) {
    delete _send_queue.front().msg;
    _send_queue.pop();
  }
}
