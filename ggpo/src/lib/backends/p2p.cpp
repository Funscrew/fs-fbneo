/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#include "p2p.h"
#include <algorithm>




static const int RECOMMENDATION_INTERVAL = 240;

// TEMP: We are hard coding the system to never time out for now.
static const int NEVER_TIMEOUT = 0;
static const int DEFAULT_DISCONNECT_TIMEOUT = NEVER_TIMEOUT;
static const int DEFAULT_DISCONNECT_NOTIFY_START = NEVER_TIMEOUT;

// ----------------------------------------------------------------------------------------------------------
Peer2PeerBackend::Peer2PeerBackend(GGPOSessionCallbacks* cb,
  const char* gamename,
  uint16 localport,
  char* remoteIp,
  uint16 remotePort,
  PlayerID playerIndex,
  std::string playerName)
  :
  _num_players(PLAYER_COUNT),
  _input_size(INPUT_SIZE),
  _sync(_local_connect_status),
  _disconnect_timeout(DEFAULT_DISCONNECT_TIMEOUT),
  _disconnect_notify_start(DEFAULT_DISCONNECT_NOTIFY_START)
{
  _callbacks = *cb;
  _synchronizing = true;
  _next_recommended_sleep = 0;

  // initialize base class members....
  _playerIndex = playerIndex;
  strcpy_s(_PlayerNames[_playerIndex], playerName.data());


  inet_pton(AF_INET, remoteIp, &_RemoteAddr);
  _RemotePort = htons(remotePort);


  /*
   * Initialize the synchronziation layer
   */
  Sync::Config config = { 0 };
  config.num_players = PLAYER_COUNT;
  config.input_size = INPUT_SIZE;
  config.callbacks = _callbacks;
  config.num_prediction_frames = MAX_PREDICTION_FRAMES;
  _sync.Init(config);

  /*
   * Initialize the UDP port
   */
  _udp.Init(localport, &_pollMgr, this);

  _endpoints = new UdpProtocol[_num_players];
  memset(_local_connect_status, 0, sizeof(_local_connect_status));
  for (int i = 0; i < ARRAY_SIZE(_local_connect_status); i++) {
    _local_connect_status[i].last_frame = -1;
  }


  /*
   * Preload the ROM
   */
  _callbacks.begin_game(gamename);


  SetDisconnectTimeout(DEFAULT_DISCONNECT_TIMEOUT);
  SetDisconnectNotifyStart(DEFAULT_DISCONNECT_TIMEOUT);


  // Add the players.  Any player that doesn't match _playerIndex is the remote player.
  for (uint16 i = 0; i < PLAYER_COUNT; i++)
  {
    GGPOPlayer p;
    bool isLocal = i == _playerIndex;
    if (isLocal) {
      p.type = GGPO_PLAYERTYPE_LOCAL;
      // Don't set any network info for local players.
    }
    else {
      p.type = GGPO_PLAYERTYPE_REMOTE;

      // Remote address....
      // Maybe use some safety here.....
      auto& ipa = p.u.remote.ip_address;
      memcpy(&ipa, remoteIp, (std::min)(ARRAYSIZE(ipa), strlen(remoteIp) + 1));
      p.u.remote.port = remotePort;
    }
    p.player_index = i;
    p.size = sizeof(GGPOPlayer);

    AddPlayer(&p);
  }


}

// ----------------------------------------------------------------------------------------------------------
Peer2PeerBackend::~Peer2PeerBackend()
{
  delete[] _endpoints;
}


// -------------------------------------------------------------------------------------------------------------------
// NOTE: We already know what our player playerIndex is so we don't have to pass it in.
// That could be different if we had two local players, but we can figure that out later....
GGPOErrorCode Peer2PeerBackend::AddLocalInput(PlayerID playerIndex, void* values, int isize) {

  GameInput input;

  // REDUNDANT CHECK
  if (_sync.InRollback()) {
    return GGPO_ERRORCODE_IN_ROLLBACK;
  }
  // REDUNDANT CHECK
  if (_synchronizing) {
    return GGPO_ERRORCODE_NOT_SYNCHRONIZED;
  }

  input.init(-1, (char*)values, isize);

  // Feed the input for the current frame into the synchronzation layer.
  if (!_sync.AddLocalInput(playerIndex, input)) {
    // TODO: Log this!
    return GGPO_ERRORCODE_PREDICTION_THRESHOLD;
  }

  if (input.frame != GameInput::NullFrame) { // xxx: <- comment why this is the case
    // Update the local connect status state to indicate that we've got a
    // confirmed local frame for this player.  this must come first so it
    // gets incorporated into the next packet we send.

    // NOTE: All endpoints send out the _local_connect_status data with each message.
    // An ideal implemetation would have a single 'client' that we set this data on,
    // and then all endpoints would also be contained internally.
    Log("setting local connect status for local player %d to %d", playerIndex, input.frame);
    _local_connect_status[playerIndex].last_frame = input.frame;

    // Send the input to all the remote players.
    // NOTE: This queues input, and it gets pumped out later....
    // NOTE: In a two player game, only one of these endpoints has the 'udp' member set, and so
    // only one of them will actully do anything.....
    for (int i = 0; i < _num_players; i++) {
      if (_endpoints[i].IsInitialized()) {
        _endpoints[i].SendInput(input);
      }
    }
  }

  return GGPO_OK;
}
// ----------------------------------------------------------------------------------------------------------
void Peer2PeerBackend::AddRemotePlayer(char* ip, uint16 port, int queue)
{
  /*
   * Start the state machine (xxx: no)
   */
  _synchronizing = true;

  _endpoints[queue].Init(&_udp, _pollMgr, queue, ip, port, _local_connect_status);
  _endpoints[queue].SetDisconnectTimeout(_disconnect_timeout);
  _endpoints[queue].SetDisconnectNotifyStart(_disconnect_notify_start);
  _endpoints[queue].Synchronize();
  _endpoints[queue].SetPlayerName(_PlayerNames[_playerIndex]);
}

// ----------------------------------------------------------------------------------------------------------
GGPOErrorCode Peer2PeerBackend::DoPoll(int timeout)
{
  if (!_sync.InRollback()) {
    _pollMgr.Pump(0);

    PollUdpProtocolEvents();

    if (!_synchronizing) {
      _sync.CheckSimulation(timeout);

      // notify all of our endpoints of their local frame number for their
      // next connection quality report
      int current_frame = _sync.GetFrameCount();
      for (int i = 0; i < _num_players; i++) {
        _endpoints[i].SetLocalFrameNumber(current_frame);
      }

      int total_min_confirmed;
      if (_num_players <= 2) {
        total_min_confirmed = Poll2Players(current_frame);
      }
      else {
        total_min_confirmed = PollNPlayers(current_frame);
      }

      Log("last confirmed frame in p2p backend is %d.\n", total_min_confirmed);
      if (total_min_confirmed >= 0) {
        ASSERT(total_min_confirmed != INT_MAX);

        Log("setting confirmed frame in sync to %d.\n", total_min_confirmed);
        _sync.SetLastConfirmedFrame(total_min_confirmed);
      }

      // send timesync notifications if now is the proper time
      if (current_frame > _next_recommended_sleep) {
        int interval = 0;
        for (int i = 0; i < _num_players; i++) {
          interval = MAX(interval, _endpoints[i].RecommendFrameDelay());
        }

        if (interval > 0) {
          GGPOEvent info;
          info.code = GGPO_EVENTCODE_TIMESYNC;
          info.u.timesync.frames_ahead = interval;
          _callbacks.on_event(&info);
          _next_recommended_sleep = current_frame + RECOMMENDATION_INTERVAL;
        }
      }
      // XXX: this is obviously a farce...
      if (timeout) {
        Sleep(1);
      }
    }
  }
  return GGPO_OK;
}

// ----------------------------------------------------------------------------------------------------------
int Peer2PeerBackend::Poll2Players(int current_frame)
{
  uint16  i;

  // discard confirmed frames as appropriate
  int total_min_confirmed = MAX_INT;
  for (i = 0; i < _num_players; i++) {
    bool queue_connected = true;
    if (_endpoints[i].IsRunning()) {
      int ignore;
      queue_connected = _endpoints[i].GetPeerConnectStatus(i, &ignore);
    }
    if (!_local_connect_status[i].disconnected) {
      total_min_confirmed = MIN(_local_connect_status[i].last_frame, total_min_confirmed);
    }
    Log("  local endp: connected = %d, last_received = %d, total_min_confirmed = %d.\n", !_local_connect_status[i].disconnected, _local_connect_status[i].last_frame, total_min_confirmed);
    if (!queue_connected && !_local_connect_status[i].disconnected) {
      Log("disconnecting i %d by remote request.\n", i);
      DisconnectPlayer(i, total_min_confirmed);
    }
    Log("  total_min_confirmed = %d.\n", total_min_confirmed);
  }
  return total_min_confirmed;
}

// ----------------------------------------------------------------------------------------------------------
int Peer2PeerBackend::PollNPlayers(int current_frame)
{
  uint16 i, queue;
  int last_received;

  // discard confirmed frames as appropriate
  int total_min_confirmed = MAX_INT;
  for (queue = 0; queue < _num_players; queue++) {
    bool queue_connected = true;
    int queue_min_confirmed = MAX_INT;
    Log("considering playerIndex %d.\n", queue);
    for (i = 0; i < _num_players; i++) {
      // we're going to do a lot of logic here in consideration of endpoint i.
      // keep accumulating the minimum confirmed point for all n*n packets and
      // throw away the rest.
      if (_endpoints[i].IsRunning()) {
        bool connected = _endpoints[i].GetPeerConnectStatus((int)queue, &last_received);

        queue_connected = queue_connected && connected;
        queue_min_confirmed = MIN(last_received, queue_min_confirmed);
        Log("  endpoint %d: connected = %d, last_received = %d, queue_min_confirmed = %d.\n", i, connected, last_received, queue_min_confirmed);
      }
      else {
        Log("  endpoint %d: ignoring... not running.\n", i);
      }
    }
    // merge in our local status only if we're still connected!
    if (!_local_connect_status[queue].disconnected) {
      queue_min_confirmed = MIN(_local_connect_status[queue].last_frame, queue_min_confirmed);
    }
    Log("  local endp: connected = %d, last_received = %d, queue_min_confirmed = %d.\n", !_local_connect_status[queue].disconnected, _local_connect_status[queue].last_frame, queue_min_confirmed);

    if (queue_connected) {
      total_min_confirmed = MIN(queue_min_confirmed, total_min_confirmed);
    }
    else {
      // check to see if this disconnect notification is further back than we've been before.  If
      // so, we need to re-adjust.  This can happen when we detect our own disconnect at frame n
      // and later receive a disconnect notification for frame n-1.
      if (!_local_connect_status[queue].disconnected || _local_connect_status[queue].last_frame > queue_min_confirmed) {
        Log("disconnecting playerIndex %d by remote request.\n", queue);
        DisconnectPlayer(queue, queue_min_confirmed);
      }
    }
    Log("  total_min_confirmed = %d.\n", total_min_confirmed);
  }
  return total_min_confirmed;
}

// -------------------------------------------------------------------------------------------------------------------
GGPOErrorCode Peer2PeerBackend::AddPlayer(GGPOPlayer* player)
{
  // Spectator support will be removed!
  if (player->type == GGPO_PLAYERTYPE_SPECTATOR) {
    return GGPO_ERRORCODE_GENERAL_FAILURE;
  }

  PlayerID playerIndex = player->player_index;
  if (player->player_index > _num_players) {
    return GGPO_ERRORCODE_PLAYER_OUT_OF_RANGE;
  }

  if (player->type == GGPO_PLAYERTYPE_REMOTE) {
    AddRemotePlayer(player->u.remote.ip_address, player->u.remote.port, playerIndex);
  }

  return GGPO_OK;
}


// -------------------------------------------------------------------------------------------------------------------
bool Peer2PeerBackend::ChatCommand(char* text) {

  for (int i = 0; i < _num_players; i++) {
    // if (i == _playerIndex) { continue; }      // Don't chat to ourselves....

    if (_endpoints[i].IsInitialized()) {
      _endpoints[i].SendChat(text);
    }
  }

  return true;
}

// -------------------------------------------------------------------------------------------------------------------
GGPOErrorCode Peer2PeerBackend::SyncInput(void* values, int isize, int playerCount) {
  // Wait until we've started to return inputs.
  if (_synchronizing) {
    return GGPO_ERRORCODE_NOT_SYNCHRONIZED;
  }

  // If we are rolling back, there is no need to attempt to add a local input.
  // The call will result in an error code anyway....
  if (!_sync.InRollback())
  {
    GGPOErrorCode code = AddLocalInput(_playerIndex, values, isize);
    if (code != GGPO_OK) {
      return code;
    }
  }

  // NOTE: We aren't doing anything with the flags... I think the system is probably using the event codes
  // to playerIndex this kind of thing......
  _sync.SynchronizeInputs(values, isize * playerCount);

  return GGPO_OK;
}

// ----------------------------------------------------------------------------------------------------------
GGPOErrorCode Peer2PeerBackend::IncrementFrame(void)
{
  Log("End of frame (%d)...\n", _sync.GetFrameCount());
  _sync.IncrementFrame();
  DoPoll(0);
  PollSyncEvents();

  return GGPO_OK;
}


// ----------------------------------------------------------------------------------------------------------
void Peer2PeerBackend::PollSyncEvents(void)
{
  Sync::Event e;
  while (_sync.GetEvent(e)) {
    OnSyncEvent(e);
  }
  return;
}

// ----------------------------------------------------------------------------------------------------------
void Peer2PeerBackend::PollUdpProtocolEvents(void)
{
  UdpEvent evt;
  for (uint16 i = 0; i < _num_players; i++) {
    while (_endpoints[i].GetEvent(evt)) {
      OnUdpProtocolPeerEvent(evt, i);
    }
  }
}

// ----------------------------------------------------------------------------------------------------------
void Peer2PeerBackend::OnUdpProtocolPeerEvent(UdpEvent& evt, PlayerID playerIndex)
{
  OnUdpProtocolEvent(evt, playerIndex);
  switch (evt.type) {
  case UdpEvent::Input:
    if (!_local_connect_status[playerIndex].disconnected) {

      int current_remote_frame = _local_connect_status[playerIndex].last_frame;
      int new_remote_frame = evt.u.input.input.frame;
      ASSERT(current_remote_frame == -1 || new_remote_frame == (current_remote_frame + 1));

      _sync.AddRemoteInput(playerIndex, evt.u.input.input);
      // Notify the other endpoints which frame we received from a peer
      Log("setting remote connect status for playerIndex %d to %d\n", playerIndex, evt.u.input.input.frame);
      _local_connect_status[playerIndex].last_frame = evt.u.input.input.frame;
    }
    break;

  case UdpEvent::Disconnected:
    DisconnectPlayer(playerIndex);
    break;

  }
}

// ----------------------------------------------------------------------------------------------------------
void Peer2PeerBackend::OnUdpProtocolEvent(UdpEvent& evt, PlayerID playerIndex)
{
  GGPOEvent info;

  switch (evt.type) {
  case UdpEvent::Connected:
    info.code = GGPO_EVENTCODE_CONNECTED_TO_PEER;
    info.u.connected.player_index = playerIndex;

    strcpy_s(_PlayerNames[playerIndex], evt.u.connected.playerName);

    // strcpy_s(info.u.connected.playerName, evt.u.connected.playerName);

    _callbacks.on_event(&info);
    break;
  case UdpEvent::Synchronizing:
    info.code = GGPO_EVENTCODE_SYNCHRONIZING_WITH_PEER;
    info.u.synchronizing.player_index = playerIndex;
    info.u.synchronizing.count = evt.u.synchronizing.count;
    info.u.synchronizing.total = evt.u.synchronizing.total;
    _callbacks.on_event(&info);
    break;
  case UdpEvent::Synchronized:
    info.code = GGPO_EVENTCODE_SYNCHRONIZED_WITH_PEER;
    info.u.synchronized.player_index = playerIndex;
    _callbacks.on_event(&info);

    CheckInitialSync();
    break;

  case UdpEvent::NetworkInterrupted:
    info.code = GGPO_EVENTCODE_CONNECTION_INTERRUPTED;
    info.u.connection_interrupted.player_index = playerIndex;
    info.u.connection_interrupted.disconnect_timeout = evt.u.network_interrupted.disconnect_timeout;
    _callbacks.on_event(&info);
    break;

  case UdpEvent::NetworkResumed:
    info.code = GGPO_EVENTCODE_CONNECTION_RESUMED;
    info.u.connection_resumed.player_index = playerIndex;
    _callbacks.on_event(&info);
    break;

  case UdpEvent::ChatCommand:

    char text[MAX_GGPOCHAT_SIZE + 1];
    auto userName = _PlayerNames[playerIndex];

    strcpy_s(text, evt.u.chat.text);

    info.code = GGPO_EVENTCODE_CHATCOMMAND;
    info.u.chat.username = userName;
    info.u.chat.text = text;

    _callbacks.on_event(&info);

    break;

  }



}

/*
 * Called only as the result of a local decision to disconnect.  The remote
 * decisions to disconnect are a result of us parsing the peer_connect_settings
 * blob in every endpoint periodically.
 */
GGPOErrorCode Peer2PeerBackend::DisconnectPlayer(PlayerID player)
{
  uint16 queue = player;
  //	GGPOErrorCode result;

    // if (player > MAX_PLA
    //result = PlayerHandleToQueue(player, &playerIndex);
    //if (!GGPO_SUCCEEDED(result)) {
    //	return result;
    //}

  if (_local_connect_status[queue].disconnected) {
    return GGPO_ERRORCODE_PLAYER_DISCONNECTED;
  }

  if (!_endpoints[queue].IsInitialized()) {
    int current_frame = _sync.GetFrameCount();
    // xxx: we should be tracking who the local player is, but for now assume
    // that if the endpoint is not initalized, this must be the local player.
    Log("Disconnecting local player %d at frame %d by user request.\n", queue, _local_connect_status[queue].last_frame);
    for (uint16 i = 0; i < _num_players; i++) {
      if (_endpoints[i].IsInitialized()) {
        DisconnectPlayer(i, current_frame);
      }
    }
  }
  else {
    Log("Disconnecting playerIndex %d at frame %d by user request.\n", queue, _local_connect_status[queue].last_frame);
    DisconnectPlayer(queue, _local_connect_status[queue].last_frame);
  }
  return GGPO_OK;
}

// --------------------------------------------------------------------------------------------------------------
void Peer2PeerBackend::DisconnectPlayer(PlayerID playerIndex, int syncto)
{
  GGPOEvent info;
  int framecount = _sync.GetFrameCount();

  _endpoints[playerIndex].Disconnect();

  Log("Changing playerIndex %d local connect status for last frame from %d to %d on disconnect request (current: %d).\n",
    playerIndex, _local_connect_status[playerIndex].last_frame, syncto, framecount);

  _local_connect_status[playerIndex].disconnected = 1;
  _local_connect_status[playerIndex].last_frame = syncto;

  if (syncto < framecount) {
    Log("adjusting simulation to account for the fact that %d disconnected @ %d.\n", playerIndex, syncto);
    _sync.AdjustSimulation(syncto);
    Log("finished adjusting simulation.\n");
  }

  info.code = GGPO_EVENTCODE_DISCONNECTED_FROM_PEER;
  info.u.disconnected.player_index = playerIndex;
  _callbacks.on_event(&info);

  CheckInitialSync();
}


// --------------------------------------------------------------------------------------------------------------
bool Peer2PeerBackend::GetNetworkStats(GGPONetworkStats* stats, PlayerID playerIndex)
{
  memset(stats, 0, sizeof * stats);
  _endpoints[playerIndex].GetNetworkStats(stats);

  return true;
}

// --------------------------------------------------------------------------------------------------------------
void Peer2PeerBackend::SetFrameDelay(int delay) {
  _sync.SetFrameDelay(_playerIndex, delay);
}

// --------------------------------------------------------------------------------------------------------------
GGPOErrorCode
Peer2PeerBackend::SetDisconnectTimeout(int timeout)
{
  _disconnect_timeout = timeout;
  for (int i = 0; i < _num_players; i++) {
    if (_endpoints[i].IsInitialized()) {
      _endpoints[i].SetDisconnectTimeout(_disconnect_timeout);
    }
  }
  return GGPO_OK;
}

GGPOErrorCode
Peer2PeerBackend::SetDisconnectNotifyStart(int timeout)
{
  _disconnect_notify_start = timeout;
  for (int i = 0; i < _num_players; i++) {
    if (_endpoints[i].IsInitialized()) {
      _endpoints[i].SetDisconnectNotifyStart(_disconnect_notify_start);
    }
  }
  return GGPO_OK;
}

//// OBSOLETE: This will be removed as it just removes 1 from the player playerIndex.
//GGPOErrorCode
//Peer2PeerBackend::PlayerHandleToQueue(PlayerID player, int* playerIndex)
//{
//  int offset = ((int)player - 1);
//  if (offset < 0 || offset >= _num_players) {
//    return GGPO_ERRORCODE_INVALID_PLAYER_HANDLE;
//  }
//  *playerIndex = offset;
//  return GGPO_OK;
//}

// ----------------------------------------------------------------------------------------------------------
void Peer2PeerBackend::OnMsg(sockaddr_in& from, UdpMsg* msg, int len)
{
  for (int i = 0; i < _num_players; i++) {
    if (_endpoints[i].HandlesMsg(from, msg)) {
      _endpoints[i].OnMsg(msg, len);
      return;
    }
  }
}

// ----------------------------------------------------------------------------------------------------------
void Peer2PeerBackend::CheckInitialSync()
{
  int i;

  if (_synchronizing) {
    // Check to see if everyone is now synchronized.  If so,
    // go ahead and tell the client that we're ok to accept input.
    for (i = 0; i < _num_players; i++) {
      // xxx: IsInitialized() must go... we're actually using it as a proxy for "represents the local player"
      if (_endpoints[i].IsInitialized() && !_endpoints[i].IsSynchronized() && !_local_connect_status[i].disconnected) {
        return;
      }
    }

    GGPOEvent info;
    info.code = GGPO_EVENTCODE_RUNNING;
    _callbacks.on_event(&info);
    _synchronizing = false;
  }
}
