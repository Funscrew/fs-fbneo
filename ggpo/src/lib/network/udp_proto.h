/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#ifndef _UDP_PROTO_H_
#define _UDP_PROTO_H_

#include "poll.h"
#include "udp.h"
#include "udp_msg.h"
#include "game_input.h"
#include "timesync.h"
#include "ggponet.h"
#include "ring_buffer.h"


 // TODO: This struct will be un-nested.
struct UdpEvent {
  enum Type {
    Unknown = -1,
    Connected,
    Synchronizing,
    Synchronized,
    Input,
    Disconnected,
    NetworkInterrupted,
    NetworkResumed,
    Datagram
  };

  Type      type;
  union {
    struct {
      GameInput   input;
    } input;

    struct {
      int         total;
      int         count;
    } synchronizing;

    struct {
      char playerName[MAX_NAME_SIZE];
    } connected;

    struct {
      int         disconnect_timeout;
    } network_interrupted;

    struct {
      uint8_t code;
      uint8_t dataSize;
      char		data[MAX_GGPO_DATA_SIZE];
    } chat;   // REFACTOR: Rename to 'data' or something like that...

  } u;			// REFACTOR: Rename this to 'data'

  UdpEvent(Type t = Unknown) : type(t) {}
};



class UdpProtocol : public IPollSink
{
public:
  struct Stats {
    int                 ping;
    int                 remote_frame_advantage;
    int                 local_frame_advantage;
    int                 send_queue_len;
    Udp::Stats          udp;
  };


public:
  virtual bool OnLoopPoll(void* cookie);

public:
  UdpProtocol();
  virtual ~UdpProtocol();

  void Init(Udp* udp, PollManager& p, int queue, char* ip, u_short port, UdpMsg::connect_status* status, uint32_t clientVersion);

  void Synchronize();
  bool GetPeerConnectStatus(int id, int* frame);
  bool IsInitialized() { return _udp != NULL; }
  bool IsSynchronized() { return _current_state == Running; }
  bool IsRunning() { return _current_state == Running; }
  void SendInput(GameInput& input);
  void SendChat(char* text);
  void SendData(uint8_t command, void* data, uint8_t dataSize);
  void SendInputAck();
  bool HandlesMsg(sockaddr_in& from, UdpMsg* msg);
  void OnMsg(UdpMsg* msg, int len);
  void Disconnect();

  void GetNetworkStats(struct GGPONetworkStats* stats);
  bool GetEvent(UdpEvent& e);
  void GGPONetworkStats(Stats* stats);
  void SetLocalFrameNumber(int num);
  int RecommendFrameDelay();

  void SetDisconnectTimeout(int timeout);
  void SetDisconnectNotifyStart(int timeout);

  void SetPlayerName(char* playerName_);
protected:

  enum State {
    Syncing,
    Synchronized,
    Running,
    Disconnected
  };

  struct QueueEntry {
    int         queue_time;
    sockaddr_in dest_addr;
    UdpMsg* msg;

    QueueEntry() {}
    QueueEntry(int time, sockaddr_in& dst, UdpMsg* m) : queue_time(time), dest_addr(dst), msg(m) {}
  };

  bool CreateSocket(int retries);
  void UpdateNetworkStats(void);
  void QueueEvent(const UdpEvent& evt);
  void ClearSendQueue(void);
  // void Log(const char* fmt, ...);
  //void LogMsg(const char* prefix, UdpMsg* msg);
  // void LogEvent(const char* prefix, const UdpEvent& evt);
  void SendSyncRequest();
  void SendMsg(UdpMsg* msg);
  void PumpSendQueue();

  // REFACTOR:  All of the 'len' types should be 'size_t'
  void DispatchMsg(uint8* buffer, int len);
  void SendPendingOutput();
  bool OnInvalid(UdpMsg* msg, int len);
  bool OnSyncRequest(UdpMsg* msg, int len);
  bool OnSyncReply(UdpMsg* msg, int len);
  bool OnInput(UdpMsg* msg, int len);
  bool OnInputAck(UdpMsg* msg, int len);
  bool OnQualityReport(UdpMsg* msg, int len);
  bool OnQualityReply(UdpMsg* msg, int len);
  bool OnKeepAlive(UdpMsg* msg, int len);
  bool OnData(UdpMsg* msg, int len);

protected:
  /*
   * Network transmission information
   */
  Udp* _udp;
  sockaddr_in    _peer_addr;
  uint16         _magic_number;
  int            _queue;
  uint16         _remote_magic_number;
  bool           _connected;
  int            _send_latency;
  int            _oop_percent;

  // NOTE: This is basically the same thing as 'QueueEntry' but with 'send_time' instead of 'queue_time'
  // I think that we should just use queue time and change the prop name as needed...
  struct {
    int         send_time;
    sockaddr_in dest_addr;
    UdpMsg* msg;
  }              _oo_packet;
  RingBuffer<QueueEntry, 64> _send_queue;

  /*
   * Network Stats
   */
  int            _round_trip_time;            // REFACTOR: This is 'ping'
  int            _packets_sent;               // REFACTOR: 'totalPacketsSent'
  int            _bytes_sent;                 // REFACTOR: This is total bytes sent minus the UDP overhead... find a clever name....  'total packet bytes'?
  int            _kbps_sent;
  int            _stats_start_time;

  /*
   * The state machine
   */
  UdpMsg::connect_status* _local_connect_status;
  UdpMsg::connect_status _peer_connect_status[UDP_MSG_MAX_PLAYERS];
  uint32_t _client_version = 0;

  // TODO: This doesn't need to be a union.  We don't need to save 8 bytes of space
  // for this level of extra work.
  State          _current_state;
  union {
    struct {
      uint32   roundtrips_remaining;
      uint32   random;
    } sync;
    struct {
      uint32   last_quality_report_time;
      uint32   last_network_stats_interval;
      uint32   last_input_packet_recv_time;
    } running;
  } _state;

  /*
   * Fairness.
   */
  int               _local_frame_advantage;
  int               _remote_frame_advantage;

  /*
   * Packet loss...
   */
  RingBuffer<GameInput, 64>  _pending_output;
  GameInput                  _last_received_input;
  GameInput                  _last_sent_input;
  GameInput                  _last_acked_input;
  unsigned int               _last_send_time;
  unsigned int               _last_recv_time;
  unsigned int               _shutdown_timeout;
  bool                       _disconnect_event_sent;
  unsigned int               _disconnect_timeout;
  unsigned int               _disconnect_notify_start;
  bool                       _disconnect_notify_sent;

  uint16                     _next_send_seq;
  uint16                     _next_recv_seq;

  /*
   * Rift synchronization.
   */
  TimeSync                   _timesync;

  /*
   * Event queue
   */
  RingBuffer<UdpEvent, 64>  _event_queue;

  // Your name.  This will be exchanged with other peers on sync.
  char _playerName[MAX_NAME_SIZE];

};

#endif
