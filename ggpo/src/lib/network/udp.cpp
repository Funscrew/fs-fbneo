/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#include "types.h"
#include "udp.h"

SOCKET
CreateSocket(uint16 bind_port, int retries)
{
  SOCKET s;
  sockaddr_in sin;
  uint16 port;
  int optval = 1;

  s = socket(AF_INET, SOCK_DGRAM, 0);
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, sizeof optval);
  setsockopt(s, SOL_SOCKET, SO_DONTLINGER, (const char*)&optval, sizeof optval);

  // non-blocking...
  u_long iMode = 1;
  ioctlsocket(s, FIONBIO, &iMode);

  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = htonl(INADDR_ANY);
  for (port = bind_port; port <= bind_port + retries; port++) {
    sin.sin_port = htons(port);
    if (bind(s, (sockaddr*)&sin, sizeof sin) != SOCKET_ERROR) {
      Utils::LogIt(CATEGORY_UDP, "Udp bound to port: %d.", port);
      return s;
    }
  }
  closesocket(s);
  return INVALID_SOCKET;
}

Udp::Udp() :
  _socket(INVALID_SOCKET),
  _callbacks(NULL)
{
}

// ----------------------------------------------------------------------------------------------------------
Udp::~Udp(void)
{
  if (_socket != INVALID_SOCKET) {
    closesocket(_socket);
    _socket = INVALID_SOCKET;
  }
}

// ----------------------------------------------------------------------------------------------------------
void Udp::Init(uint16 port, PollManager* pollMgr, Callbacks* callbacks)
{
  _callbacks = callbacks;

  _pollMgr = pollMgr;
  _pollMgr->RegisterLoopSink(this);

  Utils::LogIt(CATEGORY_UDP, "binding socket to port %d", port);
  _socket = CreateSocket(port, 0);
}

// ----------------------------------------------------------------------------------------------------------
void Udp::SendTo(char* buffer, int len, int flags, struct sockaddr* dst, int destlen)
{
  // We don't support giant packets.  This should not really be a problem in the real world...
  ASSERT(len <= MAX_UDP_PACKET_SIZE);

  struct sockaddr_in* to = (struct sockaddr_in*)dst;

  int res = sendto(_socket, buffer, len, flags, dst, destlen);
  if (res == SOCKET_ERROR) {
    DWORD err = WSAGetLastError();
    Utils::LogIt(CATEGORY_UDP, "unknown error in sendto (erro: %d  wsaerr: %d).", res, err);
    ASSERT(FALSE && "Unknown error in sendto");
  }
  char dst_ip[1024];
  Utils::LogIt(CATEGORY_UDP, "sent %d to %s:%d", len, inet_ntop(AF_INET, (void*)&to->sin_addr, dst_ip, ARRAY_SIZE(dst_ip)), ntohs(to->sin_port), res);
}

// ----------------------------------------------------------------------------------------------------------
bool Udp::OnLoopPoll(void* cookie)
{
  uint8          recv_buf[MAX_UDP_PACKET_SIZE];
  sockaddr_in    recv_addr;
  int            recv_addr_len;

  for (;;) {
    recv_addr_len = sizeof(recv_addr);
    int len = recvfrom(_socket, (char*)recv_buf, MAX_UDP_PACKET_SIZE, 0, (struct sockaddr*)&recv_addr, &recv_addr_len);

    // TODO: handle len == 0... indicates a disconnect.

    if (len == -1) {
      int error = WSAGetLastError();
      if (error != WSAEWOULDBLOCK) {
        Utils::LogIt(CATEGORY_UDP, "recvfrom WSAGetLastError returned %d (%x).", error, error);
      }
      break;
    }
    else if (len > 0) {
      char src_ip[1024];
      Utils::LogIt(CATEGORY_UDP, "recvfrom returned (len:%d  from:%s:%d).", len, inet_ntop(AF_INET, (void*)&recv_addr.sin_addr, src_ip, ARRAY_SIZE(src_ip)), ntohs(recv_addr.sin_port));
      UdpMsg* msg = (UdpMsg*)recv_buf;
      _callbacks->OnMsg(recv_addr, msg, len);
    }
  }
  return true;
}
