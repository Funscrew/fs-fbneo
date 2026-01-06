/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#ifndef _UDP_MSG_H
#define _UDP_MSG_H

#define MAX_COMPRESSED_BITS       4096    // Why is this so huge?
#define UDP_MSG_MAX_PLAYERS          4

#include "ggponet.h"

#pragma pack(push, 1)


struct UdpMsg
{
  enum MsgType {
    Invalid = 0,
    SyncRequest = 1,
    SyncReply = 2,
    Input = 3,
    QualityReport = 4,
    QualityReply = 5,
    KeepAlive = 6,
    InputAck = 7,
    Datagram = 8      // REFACTOR: -> 'Datagram' -> update related nomeclature too!
  };

  // This struct saves us one byte of space.
  // Makes ports to other languages a bit annoying to deal with, so
  // do we really need to save one byte on the packets?
  // NOTE: The implementation in fbneo doesn't really get used at all, so I am going to remix the whole thing...
  // I think that broadcasting a datagram that indicates we are disconnecting is the cleanest way to handle this
  // as the current approach to disconnect requires that the disconnecting player queues input packets, and then sends them
  // out to indicate that they have disconnected.  Not really ideal in the scenario where someone slaps the 'x' button and
  // quits immediately.  With the datagram approach, they can quickly and easily blast out the disconnect message to all listening endpoints.
  struct connect_status {
    unsigned int   disconnected : 1;
    int            last_frame : 31;
  };

  struct {
    uint16         magic;
    uint16         sequence_number;
    uint8          type;            /* packet type.  Corresponds to: 'MsgType' enum */
  } header;

  union {
    struct {
      uint32      random_request;  /* please reply back with this random data */
      uint16      remote_magic;
      uint8       remote_endpoint;
    } sync_request;

    struct {
      uint32      random_reply;           /* OK, here's your random data back */
      uint32      client_version;       // Version of this client, in 8 byte chunks: MAJOR - MINOR - REVISION - GGPO (protocol version)
      char playerName[MAX_NAME_SIZE];   /* The name of the player we synced to: */
    } sync_reply;

    struct {
      int8        frame_advantage; /* what's the other guy's frame advantage? */
      uint32      ping;
    } quality_report;

    struct {
      uint32      pong;
    } quality_reply;

    struct {
      connect_status    peer_connect_status[UDP_MSG_MAX_PLAYERS];

      uint32            start_frame;

      // _flags
      int               disconnect_requested : 1;
      int               ack_frame : 31;

      uint16            num_bits;
      uint8             input_size; // XXX: shouldn't be in every single packet!
      uint8             bits[MAX_COMPRESSED_BITS]; /* must be last */
    } input;

    struct {
      int               ack_frame : 31;
    } input_ack;

    struct {
      uint8_t code;
      uint8_t dataSize;
      char data[MAX_GGPO_DATA_SIZE];
    } datagram;

  } u;

public:
  int PacketSize() {
    return sizeof(header) + PayloadSize();
  }

  int PayloadSize() {
    int size;
    int size2;

    switch (header.type) {
    case SyncRequest:   return sizeof(u.sync_request);
    case SyncReply:
    {
      int res = sizeof(u.sync_reply);
      return res;
    }

    case QualityReport: return sizeof(u.quality_report);
    case QualityReply:  return sizeof(u.quality_reply);
    case InputAck:      return sizeof(u.input_ack);
    case KeepAlive:     return 0;

    case Input:
      // NOTE: This is a really WACKY way to compute
      // the size of the input packet!
      // Line 1 looks at a relative offset, and then line2
      // decides how many bytes from 'bits' will actually be included!
      size = (int)((char*)&u.input.bits - (char*)&u.input);
      size += (u.input.num_bits + 7) / 8;

      // A more intelligible way....
      // If this doesn't blow up we are good to go!
      size2 = sizeof(u.input) - (sizeof(uint8) * MAX_COMPRESSED_BITS);
      size2 += (u.input.num_bits + 7) / 8;
      ASSERT(size == size2);

      return size;

    case Datagram:
      size = sizeof(uint8_t) * 2;     // code + dataSize
      size += u.datagram.dataSize;

      // size = strnlen_s(u.chat.data, MAX_GGPO_DATA_SIZE) + 1;
      return size;
    }


    ASSERT(false);
    return 0;
  }

  UdpMsg(MsgType t) { header.type = (uint8)t; }
};

#pragma pack(pop)

#endif   
