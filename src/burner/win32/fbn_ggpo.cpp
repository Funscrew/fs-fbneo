#include "version.h"
#include "burner.h"
#include "ggpoclient.h"
#include "ggpo_perfmon.h"
// #include "GGPOSession.h";

extern "C" {
#include "ggponet.h"
}

#include "../../ggpo/src/lib/log.h"


const size_t MAX_HOST = 128;

GGPOSession* ggpo = nullptr;
bool bSkipPerfmonUpdates = false;

void QuarkInitPerfMon();
void QuarkPerfMonUpdate(GGPONetworkStats* stats);

namespace Utils {
  extern void InitLogger(GGPOLogOptions& options_);
}

extern int nAcbVersion;
extern int nAcbLoadState;
extern int bMediaExit;

uint8_t _playerIndex = PLAYER_NOT_SET;
uint8_t _otherPlayerIndex = PLAYER_NOT_SET;

// rollback counter
// NOTE: These are only used in the video overlay. --> so they should be part of the overlay vars?
//int totalRollbackFrames = 0;              // The number of frames that were rolled back.
//int rollbackCount = 0;               // The total number of rollbacks.
//int lastRollbackFrame = 0;            // Frame number where the last rollback was encountered.
//int avgRollbackFrames = 0;

// rollback counter
int totalRollbackFrames = 0;
int totalRollbacks = 0;



static char pGameName[MAX_PATH];
static bool bDelayLoad = false;
static bool bDirect = false;
static bool bReplaySupport = false;
static bool bReplayStarted = false;
static bool bReplayRecord = false;
static bool bReplayRecording = false;
static int iRanked = 0;     // REFACTOR: boolean - 'isRanked'
//static int _playerIndex = 0;     // REFACTOR: uint16 'playerindex' --> NOTE: This is currently 1-based, and should be zero based!  --> NOTE: will be replaced with _playerIndex!
static int iDelay = 0;
static int iSeed = 0;

const int ggpo_state_header_size = 6 * sizeof(int);

int GetHash(const char* id, int len)
{
  unsigned int hash = 1315423911;
  for (int i = 0; i < len; i++) {
    hash ^= ((hash << 5) + id[i] + (hash >> 2));
  }
  return (hash & 0x7FFFFFFF);
}

void SetBurnFPS(const char* name, int version)
{
  if (kNetVersion < NET_VERSION_60FPS) {
    // Version 1: use old framerate (59.94fps)
    bForce60Hz = 1;
    nBurnFPS = 5994;
    nAppVirtualFps = nBurnFPS;
    return;
  }

  // UMK3UC at 60fps rate
  if (kNetVersion >= NET_VERSION_UMK3UC_FRAMERATE) {
    if (!strcmp(name, "umk3uc")) {
      bForce60Hz = 1;
      nBurnFPS = 6000;
      nAppVirtualFps = nBurnFPS;
      return;
    }
  }

  if (kNetVersion < NET_VERSION_DISABLE_FORCE_60HZ) {
    bForce60Hz = 1;
    // MK Framerate at original rate (54fps)
    if (kNetVersion >= NET_VERSION_MK_FRAMERATE) {
      if (!strcmp(name, "mk") || !strcmp(name, "mk2") || !strcmp(name, "mk2p") || !strcmp(name, "mk3") ||
        !strcmp(name, "umk3") || !strcmp(name, "umk3p") || !strcmp(name, "umk3uc") || !strcmp(name, "umk3uk") ||
        !strcmp(name, "wwfman")) {
        bForce60Hz = 0;
        return;
      }
    }
    nBurnFPS = 6000;
    nAppVirtualFps = nBurnFPS;
  }
}

// [OBSOLETE] - This will be removed / parted out!
bool __cdecl ggpo_on_client_event_callback(GGPOClientEvent* info)
{
  switch (info->code)
  {
  case GGPOCLIENT_EVENTCODE_CONNECTING:
    VidOverlaySetSystemMessage(_T("Connecting..."));
    VidSSetSystemMessage(_T("Connecting..."));
    break;

  case GGPOCLIENT_EVENTCODE_CONNECTED:
    VidOverlaySetSystemMessage(_T("Connected"));
    VidSSetSystemMessage(_T("Connected"));
    break;

  case GGPOCLIENT_EVENTCODE_DISCONNECTED:
    VidOverlaySetSystemMessage(_T("Disconnected!"));
    VidSSetSystemMessage(_T("Disconnected!"));
    QuarkFinishReplay();
    break;

  case GGPOCLIENT_EVENTCODE_RETREIVING_MATCHINFO:
    VidOverlaySetSystemMessage(_T("Retrieving Match Info..."));
    VidSSetSystemMessage(_T("Retrieving Match Info..."));
    break;

  case GGPOCLIENT_EVENTCODE_MATCHINFO: {
    VidOverlaySetSystemMessage(_T(""));
    VidSSetSystemMessage(_T(""));
    if (kNetSpectator) {
      // NOTE: The net version could also be retrieved by accepting the V* chat command data....
      kNetVersion = strlen(info->u.matchinfo.netVersion) > 0 ? atoi(info->u.matchinfo.netVersion) : NET_VERSION;
      SetBurnFPS(pGameName, kNetVersion);
    }
    TCHAR szUser1[128];
    TCHAR szUser2[128];
    VidOverlaySetGameInfo(ANSIToTCHAR(info->u.matchinfo.p1, szUser1, 128), ANSIToTCHAR(info->u.matchinfo.p2, szUser2, 128), kNetSpectator, iRanked, _playerIndex);
    VidSSetGameInfo(ANSIToTCHAR(info->u.matchinfo.p1, szUser1, 128), ANSIToTCHAR(info->u.matchinfo.p2, szUser2, 128), kNetSpectator, iRanked, _playerIndex);
    break;
  }

  case GGPOCLIENT_EVENTCODE_SPECTATOR_COUNT_CHANGED:
    VidOverlaySetGameSpectators(info->u.spectator_count_changed.count);
    VidSSetGameSpectators(info->u.spectator_count_changed.count);
    break;

  default:
    break;
  }
  return true;
}

// --------------------------------------------------------------------------------------------------------------------
void __cdecl ggpo_on_rollback(int onFrame, int frameCount)
{
  totalRollbacks++;
  totalRollbackFrames += frameCount;
}

// --------------------------------------------------------------------------------------------------------------------
bool __cdecl ggpo_on_event_callback(GGPOEvent* info)
{

  switch (info->event_code) {
  case GGPO_EVENTCODE_CONNECTED_TO_PEER:
  {
    VidOverlaySetSystemMessage(_T("Connected to Peer"));
    VidSSetSystemMessage(_T("Connected to Peer"));

    // NOTE: We can probably update the game info here....
    char* p1 = ggpo_get_playerName(ggpo, 0);
    char* p2 = ggpo_get_playerName(ggpo, 1);

    char p1Final[16 * 2];   // NOTE: NAME_MAX * 2 for formatting chars....
    char p2Final[16 * 2];   // NOTE: NAME_MAX * 2 for formatting chars....

    // TODO: REFACTOR:  In no universe should be encoding args into strings, and passing them to a function that will pull them back out via scanf.
    // This is a pretty gigundo inefficiency!
    sprintf(p1Final, "%s#0,0", p1);
    sprintf(p2Final, "%s#0,0", p2);

    TCHAR p1w[16 * 2];
    TCHAR p2w[16 * 2];
    TCHAR* buffer = ANSIToTCHAR(p1Final, NULL, NULL);
    wcscpy(p1w, buffer);

    buffer = ANSIToTCHAR(p2Final, NULL, NULL);
    wcscpy(p2w, buffer);


    VidOverlaySetGameInfo(p1w, p2w, false, false, _playerIndex);

    VidOverlaySetRemoteStats(info->u.connected.delay, info->u.connected.runahead);

  }
  break;

  case GGPO_EVENTCODE_SYNCHRONIZING_WITH_PEER:
    //_stprintf(status, _T("Synchronizing with Peer (%d/%d)..."), info->u.synchronizing.count, info->u.synchronizing.total);
    VidOverlaySetSystemMessage(_T("Synchronizing with Peer..."));
    VidSSetSystemMessage(_T("Synchronizing with Peer..."));
    break;

  case GGPO_EVENTCODE_RUNNING: {
    VidOverlaySetSystemMessage(_T(""));
    VidSSetSystemMessage(_T(""));

    // NOTE: Version exchange happens during sync now.
    break;
  }

  case GGPO_EVENTCODE_DISCONNECTED_FROM_PEER:
    VidOverlaySetSystemMessage(_T("Disconnected from Peer"));
    VidSSetSystemMessage(_T("Disconnected from Peer"));
    if (bReplayRecording) {
      AviStop();
      bMediaExit = true;
    }
    break;

  case GGPO_EVENTCODE_TIMESYNC:
    break;


  case GGPO_EVENTCODE_DATAGRAM:

    switch (info->u.datagram.code)
    {
    case DATAGRAM_CODE_GGPO_SETTINGS:
    {
      auto data = info->u.datagram.data;

      // DEFINED in vid_overlay.cpp
      uint8_t delay = data[0];
      uint8_t runahead = data[1];
      VidOverlaySetRemoteStats(delay, runahead);
    }
    break;

    case DATAGRAM_CODE_CHAT:
    {
      TCHAR szUser[MAX_CHAT_SIZE];
      TCHAR szText[MAX_CHAT_SIZE];

      auto msg = info->u.datagram.data;

      // NOTE: I have the player index, but not the actual lookup table for them... that should come from the client...
      // NOTE: I can't include 'ggposession.h' at this time because it will break the compilation.  I will go back and
      // find a proper way to include it later......
      char* playerName =  ggpo_get_playerName(ggpo, info->player_index);
      ANSIToTCHAR(playerName, szUser, MAX_NAME_SIZE);

      ANSIToTCHAR(msg, szText, info->u.datagram.dataSize);

      // Chat messages must be zero terminated...
      szText[info->u.datagram.dataSize] = 0;

      // NOTE: Kind of silly that we have to come up with another string when we already have the 'C' command code.
      // TCHAR* useName = first == 'C' ? _T("Command") : szUser;
      VidOverlayAddChatLine(szUser, szText);

      // ummmm.... do we know what this is all about?
      // --> It appears that there is some other overlay / OSD layer that isn't used.  Might be for the DDraw7 stuff which we don't care about.
      // I think in 2025 that such a rendering approach can be ignored / removed.
      //TCHAR szTemp[MAX_CHAT_SIZE];
      //_sntprintf(szTemp, MAX_CHAT_SIZE, _T("«%.32hs» "), info->u.chat.username);
      //VidSAddChatLine(szTemp, 0XFFA000, ANSIToTCHAR(info->u.chat.text, NULL, 0), 0xEEEEEE);
    }
    break;

    default:
      break;
    }

    break;

  default:
    break;
  }

  return true;
}

bool __cdecl ggpo_begin_game_callback(const char* name)
{
  WIN32_FIND_DATA fd;
  TCHAR tfilename[MAX_PATH];
  TCHAR tname[MAX_PATH];

  strcpy(pGameName, name);
  ANSIToTCHAR(name, tname, MAX_PATH);
  SetBurnFPS(name, kNetVersion);

  if (!kNetSpectator)
  {
    // ranked savestate
    if (iRanked) {
      _stprintf(tfilename, _T("savestates\\%s_fbneo_ranked.fs"), tname);
      if (FindFirstFile(tfilename, &fd) != INVALID_HANDLE_VALUE) {
        // Load our save-state file (freeplay, event mode, etc.)
        BurnStateLoad(tfilename, 1, &DrvInitCallback);
        DetectorLoad(name, false, iSeed);
        // if playing a direct game, we never get match information, so put anonymous
        if (bDirect) {
          //VidOverlaySetGameInfo(_T("Player1#0,0"), _T("Player2#0,0"), false, iRanked, _playerIndex);
          VidSSetGameInfo(_T("Player1#0,0"), _T("Player2#0,0"), false, iRanked, _playerIndex);
        }
        return 0;
      }
    }

    // regular savestate
    _stprintf(tfilename, _T("savestates\\%s_fbneo.fs"), tname);
    if (FindFirstFile(tfilename, &fd) != INVALID_HANDLE_VALUE) {
      // Load our save-state file (freeplay, event mode, etc.)
      BurnStateLoad(tfilename, 1, &DrvInitCallback);
      DetectorLoad(name, false, iSeed);
      // if playing a direct game, we never get match information, so put anonymous
      if (bDirect) {
        //VidOverlaySetGameInfo(_T("Player1#0,0"), _T("Player2#0,0"), false, iRanked, _playerIndex);
        VidSSetGameInfo(_T("Player1#0,0"), _T("Player2#0,0"), false, iRanked, _playerIndex);
      }
      return 0;
    }
  }

  // no savestate or replay
  UINT32 i;
  for (i = 0; i < nBurnDrvCount; i++) {
    nBurnDrvActive = i;
    if ((_tcscmp(BurnDrvGetText(DRV_NAME), tname) == 0) && (!(BurnDrvGetFlags() & BDF_BOARDROM))) {
      if (!kNetSpectator) {
        MediaInit();

        // NOTE: If this is not a kNetGame, then the default game state will be loaded in the DrvInit call.
        // In the block above (~line 288) we are loading a different state.  Since we are in a GGPO
        // callback, we can safely assume that kNetGame == true.
        DrvInit(i, true);
      }
      else {
        // I'm guessing we load this later so that sync up game state for the spectators later....?
        bDelayLoad = true;
      }
      DetectorLoad(name, false, iSeed);
      // if playing a direct game, we never get match information, so play anonymous
      if (bDirect) {
        // VidOverlaySetGameInfo(_T("player 1#0,0"), _T("player 2#0,0"), false, iRanked, _playerIndex);
        // VidSSetGameInfo(_T("Player1#0,0"), _T("Player2#0,0"), false, iRanked, _playerIndex);
      }
      return 1;
    }
  }

  return 0;
}

bool __cdecl ggpo_rollback_frame_callback(int flags)
{
  bSkipPerfmonUpdates = true;
  nFramesEmulated--;

  // Run the frame.  This will not sync inputs and will draw / do sound, etc.
  // NOTE: The reference implementation syncs inputs on rollback....
  RunFrame(0, 0, false);
  bSkipPerfmonUpdates = false;
  return true;
}

static char gAcbBuffer[16 * 1024 * 1024];
static char* gAcbScanPointer;
static int gAcbChecksum;
static FILE* gAcbLogFp;

void ComputeIncrementalChecksum(struct BurnArea* pba)
{
  /*
   * Ignore checksums in release builds for now.  It takes a while.
   */
#if defined(FBA_DEBUG)
  int i;

#if 0
  static char* soundAreas[] = {
     "Z80",
     "YM21",
     "nTicksDone",
     "nCyclesExtra",
  };
  /*
   * This is a really crappy checksum routine, but it will do for
   * our purposes
   */
  for (i = 0; i < ARRAYSIZE(soundAreas); i++) {
    if (!strncmp(soundAreas[i], pba->szName, strlen(soundAreas[i]))) {
      return;
    }
  }
#endif

  for (i = 0; i < pba->nLen; i++) {
    int b = ((unsigned char*)pba->Data)[i];
    if (b) {
      if (i % 2)
        gAcbChecksum *= b;
      else
        gAcbChecksum += b * 317;
    }
    else
      gAcbChecksum++;
  }
#endif
}

static int QuarkLogAcb(struct BurnArea* pba)
{
  fprintf(gAcbLogFp, "%s:", pba->szName);
  int col = 10, row = 30;
  for (int i = 0; i < (int)pba->nLen; i++) {
    if ((i % row) == 0)
      fprintf(gAcbLogFp, "\noffset %9d :", i);
    else if ((i % col) == 0)
      fprintf(gAcbLogFp, " - ");
    fprintf(gAcbLogFp, " %02x", ((unsigned char*)pba->Data)[i]);
  }
  fprintf(gAcbLogFp, "\n");
  ComputeIncrementalChecksum(pba);
  return 0;
}

static int QuarkReadAcb(struct BurnArea* pba)
{
  //ComputeIncrementalChecksum(pba);
  memcpy(gAcbScanPointer, pba->Data, pba->nLen);
  gAcbScanPointer += pba->nLen;
  return 0;
}
static int QuarkWriteAcb(struct BurnArea* pba)
{
  memcpy(pba->Data, gAcbScanPointer, pba->nLen);
  gAcbScanPointer += pba->nLen;
  return 0;
}

bool __cdecl ggpo_save_game_state_callback(unsigned char** buffer, int* len, int* checksum, int frame)
{
  int payloadsize;

  gAcbChecksum = 0;
  gAcbScanPointer = gAcbBuffer;
  BurnAcb = QuarkReadAcb;
  BurnAreaScan(ACB_FULLSCANL | ACB_READ, NULL);
  payloadsize = gAcbScanPointer - gAcbBuffer;

  *checksum = gAcbChecksum;
  *len = payloadsize + ggpo_state_header_size;
  *buffer = (unsigned char*)malloc(*len);

  int* data = (int*)*buffer;
  data[0] = 'GGPO';
  data[1] = ggpo_state_header_size;
  data[2] = nBurnVer;
  data[3] = 0;
  data[4] = 0;
  data[5] = 0;
  // save game state in ranked match (so spectators can get the actual score)
  if (!kNetSpectator) {
    // 32 bits for state, scores and ranked
    int state, score1, score2, start1, start2;
    DetectorGetState(state, score1, score2, start1, start2);
    data[3] = state | ((score1 & 0xff) << 8) | ((score2 & 0xff) << 16) | (iRanked << 24);
    data[4] = (start1 & 0xff) | ((start2 & 0xff) << 8);
  }
  memcpy((*buffer) + ggpo_state_header_size, gAcbBuffer, payloadsize);
  return false;
}

bool __cdecl ggpo_load_game_state_callback(unsigned char* buffer, int len)
{
  // Used for initial loading.  Connecting + starting up spectator stuff and so on....
  if (bDelayLoad) {
    DrvInit(nBurnDrvActive, true);
    MediaInit();
    RunInit();
    bDelayLoad = false;
  }

  // The buffer is all of the game data that we are interested in.
  // It includes state?, scores, ranked, etc.
  int* data = (int*)buffer;
  if (data[0] == 'GGPO') {
    int headersize = data[1];
    int num = headersize / sizeof(int);
    // version
    nAcbVersion = data[2];
    int state = (data[3]) & 0xff;
    int score1 = (data[3] >> 8) & 0xff;
    int score2 = (data[3] >> 16) & 0xff;
    int ranked = (data[3] >> 24) & 0xff;
    int start1 = 0;
    int start2 = 0;
    if (num > 4) {
      start1 = (data[4]) & 0xff;
      start2 = (data[4] >> 8) & 0xff;
    }
    // if spectating, set ranked flag and score
    if (kNetSpectator) {
      iRanked = ranked;
      DetectorSetState(state, score1, score2, start1, start2);
      VidOverlaySetGameInfo(0, 0, kNetSpectator, iRanked, 0);
      VidOverlaySetGameScores(score1, score2);
      VidSSetGameInfo(0, 0, kNetSpectator, iRanked, 0);
      VidSSetGameScores(score1, score2);
    }

    // We move the pointer ahead to include the actual game / ROM data.
    // This gets loaded into the game state via 'BurnAreaScan' calls below.
    buffer += headersize;
  }
  gAcbScanPointer = (char*)buffer;
  BurnAcb = QuarkWriteAcb;
  nAcbLoadState = kNetSpectator;
  BurnAreaScan(ACB_FULLSCANL | ACB_WRITE, NULL);
  nAcbLoadState = 0;
  nAcbVersion = nBurnVer;

  return true;
}

bool __cdecl ggpo_log_game_state_callback(char* filename, unsigned char* buffer, int len)
{
  /*
   * Note: this is destructive since it relies on loading game
   * state before scanning!  Luckily, we only call the logging
   * routine for fatal errors (we should still fix this, though).
   */
  ggpo_load_game_state_callback(buffer, len);

  gAcbLogFp = fopen(filename, "w");

  gAcbChecksum = 0;
  BurnAcb = QuarkLogAcb;
  BurnAreaScan(ACB_FULLSCANL | ACB_READ, NULL);
  fprintf(gAcbLogFp, "\n");
  fprintf(gAcbLogFp, "Checksum:       %d\n", gAcbChecksum);
  fprintf(gAcbLogFp, "Buffer Pointer: %p\n", buffer);
  fprintf(gAcbLogFp, "Buffer Len:     %d\n", len);

  fclose(gAcbLogFp);

  return true;
}

// ----------------------------------------------------------------------------------------------------------------------------------------------
// NOTE: This looks like it could directly in the lib....
void __cdecl ggpo_free_buffer_callback(void* buffer)
{
  free(buffer);
}


// ----------------------------------------------------------------------------------------------------------------------------------------------
// Parse the address of the form [host]:port into host (ip) and port.
void ParseAddress(const char* addr, char* host, UINT16* port)
{
  memset(host, 0, MAX_HOST);

  int pos = -1;
  size_t len = strnlen_s(addr, MAX_HOST);
  for (UINT i = 0; i < len; i++)
  {
    if (addr[i] == ':') { pos = i; break; }
  }
  if (pos == -1) {
    memcpy(host, addr, len);
  }
  else {
    if (pos > 0) {
      memcpy(host, addr, pos);
    }
    char portPart[MAX_HOST];
    strncpy_s(portPart, len - (pos + 1) + 1, addr + pos + 1, MAX_HOST);

    UINT16 usePort = std::atoi(portPart);
    if (usePort != 0) {
      *port = usePort;
    }

  }
}

// ----------------------------------------------------------------------------------------------------------------------------------------------
int InitDirectConnection(DirectConnectionOptions& ops, GGPOLogOptions& logOps)
{

  Utils::InitLogger(logOps);

  const UINT16 DEFAULT_LOCAL_PORT = 7000;
  const UINT16 DEFAULT_REMOTE_PORT = 7000;

  // Let's parse out the ips/ports...
  char remoteIP[MAX_HOST];
  UINT16 localPort = DEFAULT_LOCAL_PORT;
  UINT16 remotePort = DEFAULT_REMOTE_PORT;
  UINT16 replayPort = 0;

  char localHost[MAX_HOST];
  char remoteHost[MAX_HOST];
  char replayHost[MAX_HOST];

  try
  {
    ParseAddress(ops.localAddr.data(), localHost, &localPort);
    ParseAddress(ops.remoteAddr.data(), remoteHost, &remotePort);
    ParseAddress(ops.replayAddr.data(), replayHost, &replayPort);
  }
  catch (const std::exception&)
  {
    throw std::exception("Could not parse local, remote, or replay address!");
  }

  kNetVersion = NET_VERSION;
  kNetGame = 1;
  kNetLua = 0;
  kNetSpectator = 0;
  kNetQuarkId[0] = 0;
  bForce60Hz = 0;
  iRanked = 0;
  _playerIndex = PLAYER_NOT_SET;
  iDelay = 0;

#ifdef _DEBUG
  kNetLua = 1;
#endif

  GGPOSessionCallbacks cb = { 0 };
  cb.begin_game = ggpo_begin_game_callback;
  cb.load_game_state = ggpo_load_game_state_callback;
  cb.save_game_state = ggpo_save_game_state_callback;
  cb.log_game_state = ggpo_log_game_state_callback;
  cb.free_buffer = ggpo_free_buffer_callback;
  cb.rollback_frame = ggpo_rollback_frame_callback;
  cb.on_event = ggpo_on_event_callback;
  cb.on_rollback = ggpo_on_rollback;


  // SET GLOBALS
  kNetLua = 1;
  bDirect = true;
  iRanked = 0;
  _playerIndex = ops.playerNumber - 1;
  _otherPlayerIndex = _playerIndex == 0 ? 1 : 0;

  iDelay = ops.frameDelay;
  iSeed = 0;

  char* useReplayIp = nullptr;
  uint64_t useSessionId = 0;

  if (ops.replayAddr.length() > 0) { 
    useReplayIp = ops.replayAddr.data();

    if (ops.replayId == 0) { 
      throw std::exception("Invalid or missing replay id!");
    }
    useSessionId = ops.replayId;
  }

  ggpo = ggpo_start_session(&cb, ops.romName.data(), localPort, remoteHost, remotePort, _playerIndex, ops.playerName.data(), FS_VERSION, useReplayIp, replayPort, useSessionId);

  ggpo_set_frame_delay(ggpo, ops.frameDelay);
  VidOverlaySetSystemMessage(_T("Connecting..."));

  return 0;


}

// ----------------------------------------------------------------------------------------------------------------------------------------------
// [OBSOLETE] -- This will be parted out into individual functions as we go.  Leaving it here for historical purposes.
void QuarkInit(TCHAR* tconnect)
{
  char connect[MAX_PATH];
  TCHARToANSI(tconnect, connect, MAX_PATH);
  char gameName[128], quarkid[128], remoteIp[128];
  int port = 0;
  int delay = 0;
  int ranked = 0;
  int live = 0;
  int frames = 0;
  int playerNumber = 0;
  int localPort, remotePort;

  kNetVersion = NET_VERSION;
  kNetGame = 1;
  kNetLua = 0;
  kNetSpectator = 0;
  kNetQuarkId[0] = 0;
  bForce60Hz = 0;
  iRanked = 0;
  _playerIndex = 0;
  iDelay = 0;

#ifdef _DEBUG
  kNetLua = 1;
#endif

  GGPOSessionCallbacks cb = { 0 };

  cb.begin_game = ggpo_begin_game_callback;
  cb.load_game_state = ggpo_load_game_state_callback;
  cb.save_game_state = ggpo_save_game_state_callback;
  cb.log_game_state = ggpo_log_game_state_callback;
  cb.free_buffer = ggpo_free_buffer_callback;
  cb.rollback_frame = ggpo_rollback_frame_callback;
  cb.on_event = ggpo_on_event_callback;

  // This path is used for connecting to other players via FC servers.
  // We need a centralized server for it.....
  //if (strncmp(connect, "quark:served", strlen("quark:served")) == 0) {
  //	sscanf(connect, "quark:served,%[^,],%[^,],%d,%d,%d", game, quarkid, &port, &delay, &ranked);
  //	iRanked = ranked;
  //	_playerIndex = atoi(&quarkid[strlen(quarkid) - 1]);
  //	if (nVidRunahead == 3 && delay < 2) {delay = 2;}
  //	iDelay = delay;
  //	iSeed = GetHash(quarkid, strlen(quarkid) - 2);
  //	ggpo = ggpo_client_connect(&cb, game, quarkid, port);
  //	ggpo_set_frame_delay(ggpo, delay);
  //	strcpy(kNetQuarkId, quarkid);
  //	VidOverlaySetSystemMessage(_T("Connecting..."));
  //}
  // Also not used....
  //if (strncmp(connect, "quark:training", strlen("quark:training")) == 0) {
  //	sscanf(connect, "quark:training,%[^,],%[^,],%d,%d", game, quarkid, &port, &delay);
  //	iRanked = 0;
  //	_playerIndex = atoi(&quarkid[strlen(quarkid) - 1]);
  //	if (nVidRunahead == 3 && delay < 2) {delay = 2;}
  //	iDelay = delay;
  //	iSeed = GetHash(quarkid, strlen(quarkid) - 2);
  //	ggpo = ggpo_client_connect(&cb, game, quarkid, port);
  //	ggpo_set_frame_delay(ggpo, delay);
  //	strcpy(kNetQuarkId, quarkid);
  //	VidOverlaySetSystemMessage(_T("Connecting..."));
  //	kNetLua = 1;
  //	FBA_LoadLuaCode("fbneo-training-mode/fbneo-training-mode.lua");
  //}
  if (strncmp(connect, "quark:direct", strlen("quark:direct")) == 0) {

    throw std::exception("obsolete branch!  don't come here!");
    //// REFERENCE for sscanf: https://en.cppreference.com/w/c/io/fscanf
    //int scanCount = sscanf(connect, "quark:direct,%[^,],%d,%[^,],%d,%d,%d,%d", gameName, &localPort, remoteIp, &remotePort, &playerNumber, &delay, &ranked);
    //if (scanCount != 7) {
    //  // TODO: Find the best way to handle bad CLI inputs.
    //  // A proper CLI parser might be nice at some point too.
    //  throw std::exception("bad command line!");
    //}
    //if (playerNumber < 1 || playerNumber > 2) {
    //  throw std::exception("Invalid player number.  Use 1 or 2!");
    //}


    //kNetLua = 1;
    //bDirect = true;
    //iRanked = 0;
    //_playerIndex = playerNumber;
    //iDelay = delay;
    //iSeed = 0;
    //// ggpo = ggpo_start_session(&cb, game, localPort, host, remotePort, playerNumber);
    //ggpo = ggpo_start_session(&cb, gameName, localPort, remoteIp, remotePort, playerNumber - 1);
    //ggpo_set_frame_delay(ggpo, delay);
    //VidOverlaySetSystemMessage(_T("Connecting..."));
  }
  /*
  else if (strncmp(connect, "quark:synctest", strlen("quark:synctest")) == 0) {
    sscanf(connect, "quark:synctest,%[^,],%d", game, &frames);
    ggpo = ggpo_start_synctest(&cb, game, frames);
  }
  */

  // NOTE: Streaming is not possible at this time.  We need a server to connect to.
  // quark:stream is how replay + spectating is handled.
  //else if (strncmp(connect, "quark:stream", strlen("quark:stream")) == 0) {
  //	sscanf(connect, "quark:stream,%[^,],%[^,],%d", game, quarkid, &remotePort);
  //	bVidAutoSwitchFullDisable = true;
  //	kNetSpectator = 1;
  //	kNetLua = 1;
  //	iSeed = 0;

  //	// NOTE: ggpo_start_spectating is what the original call probably was/is.
  //	ggpo = ggpo_start_streaming(&cb, game, quarkid, remotePort);
  //	strcpy(kNetQuarkId, quarkid);
  //	VidOverlaySetSystemMessage(_T("Connecting..."));
  //}
  // It appears that the replay functionality is not used.
  // Spectating and replay in FC is handled via 'quark:stream'

  //else if (strncmp(connect, "quark:replay", strlen("quark:replay")) == 0) {
  //	bVidAutoSwitchFullDisable = true;
  //	kNetSpectator = 1;
  //	kNetLua = 1;
  //	iSeed = 0;
  //	ggpo = ggpo_start_replay(&cb, connect + strlen("quark:replay,"));
  //	strcpy(kNetQuarkId, quarkid);
  //	VidOverlaySetSystemMessage(_T("Connecting..."));
  //}
  else if (strncmp(connect, "quark:debugdetector", strlen("quark:debugdetector")) == 0) {
    sscanf(connect, "quark:debugdetector,%[^,]", gameName);
    kNetGame = 0;
    iRanked = 1;
    _playerIndex = 0;
    iSeed = 0x133;
    kNetLua = 1;
    // load game
    TCHAR tgame[128];
    ANSIToTCHAR(gameName, tgame, 128);
    for (UINT32 i = 0; i < nBurnDrvCount; i++) {
      nBurnDrvActive = i;
      if ((_tcscmp(BurnDrvGetText(DRV_NAME), tgame) == 0) && (!(BurnDrvGetFlags() & BDF_BOARDROM))) {
        // Load game
        MediaInit();
        DrvInit(i, true);
        // Load our save-state file (freeplay, event mode, etc.)
        WIN32_FIND_DATA fd;
        TCHAR tfilename[MAX_PATH];
        _stprintf(tfilename, _T("savestates\\%s_fbneo.fs"), tgame);
        if (FindFirstFile(tfilename, &fd) != INVALID_HANDLE_VALUE) {
          BurnStateLoad(tfilename, 1, &DrvInitCallback);
        }
        DetectorLoad(gameName, true, iSeed);
        VidOverlaySetGameInfo(_T("Detector1#0,0,0"), _T("Detector2#0,0,0"), false, iRanked, _playerIndex);
        VidOverlaySetGameSpectators(0);
        VidSSetGameInfo(_T("Detector1"), _T("Detector2"), false, iRanked, _playerIndex);
        VidSSetGameSpectators(0);
        break;
      }
    }
  }
}

// -------------------------------------------------------------------------------------------------------------------
void QuarkEnd()
{
  ConfigGameSave(bSaveInputs);
  ggpo_close_session(ggpo);
  kNetGame = 0;
  bMediaExit = true;

}

// -------------------------------------------------------------------------------------------------------------------
void QuarkTogglePerfMon()
{
  static bool initialized = false;
  if (!initialized) {
    ggpoutil_perfmon_init(hScrnWnd);
  }
  ggpoutil_perfmon_toggle();
}

// -------------------------------------------------------------------------------------------------------------------
void QuarkRunIdle(int ms)
{
  ggpo_idle(ggpo, ms);
}

// -------------------------------------------------------------------------------------------------------------------
// Disconnect ourselves from the system...
void QuarkDisconnect()
{
  ggpo_disconnect(ggpo);
}

// -------------------------------------------------------------------------------------------------------------------
bool QuarkGetInput(void* values, int isize, int playerIndex)
{
  // NOTE: This call is handling both the addition of the local inputs, and the sync call....
  bool res = ggpo_synchronize_input(ggpo, values, isize, GGPO_MAX_PLAYERS) == GGPO_OK;
  return res;
}

// -------------------------------------------------------------------------------------------------------------------
bool QuarkIncrementFrame()
{
  // start auto replay
  if (bReplayRecord) {
    bReplayRecord = false;
    bReplayRecording = true;
    AviStart();
  }

  ggpo_advance_frame(ggpo);

  if (!bSkipPerfmonUpdates) {
    GGPONetworkStats stats;

    ggpo_get_stats(ggpo, &stats, _otherPlayerIndex);
    ggpoutil_perfmon_update(ggpo, stats);
  }

  if (!bReplaySupport && !bReplayStarted) {
    bReplayStarted = true;
  }

  return true;
}

// --------------------------------------------------------------------------------------------------------
void QuarkSendChat(char* text)
{
  ggpo_send_chat(ggpo, text);
}


// --------------------------------------------------------------------------------------------------------
void QuarkSendData(uint8_t code, void* data, uint8_t dataSize)
{
  if (code == 'T' && _playerIndex != PLAYER_NOT_SET && ggpo && !isChatMuted)
  {
    static char msgBuffer[MAX_GGPO_DATA_SIZE]; // command chat  +1 for command char +1 for zero termination.
    auto useSize = (std::min)(dataSize, (uint8_t)(MAX_GGPO_DATA_SIZE - 1));
    memcpy_s(msgBuffer, MAX_GGPO_DATA_SIZE, data, useSize);
    msgBuffer[useSize] = 0;

    auto playerName = ggpo_get_playerName(ggpo, _playerIndex);
    wchar_t nameBuffer[16 * 2];
    wcscpy(nameBuffer, ANSIToTCHAR(playerName, NULL, NULL));

    VidOverlayAddChatLine(nameBuffer, ANSIToTCHAR(msgBuffer + 1, NULL, NULL));
  }


  ggpo_send_data(ggpo, code, data, dataSize);
}

// --------------------------------------------------------------------------------------------------------
// [OBSOLETE:  THIS DOES NOTHING!]
void QuarkSendChatCmd(char* text, char code)
{
  return;

  static char msgBuffer[MAX_CHAT_SIZE]; // command chat  +1 for command char +1 for zero termination.
  memset(msgBuffer, 0, MAX_CHAT_SIZE);

  msgBuffer[0] = code;
  strncpy(&msgBuffer[1], text, MAX_CHAT_SIZE - 1);

  // Print the chat line on our local:
  if (code == 'T' && _playerIndex != PLAYER_NOT_SET && ggpo && !isChatMuted) {
    auto playerName = ggpo_get_playerName(ggpo, _playerIndex);
    wchar_t nameBuffer[16 * 2];
    wcscpy(nameBuffer, ANSIToTCHAR(playerName, NULL, NULL));

  }
  ggpo_send_chat(ggpo, msgBuffer);
}

// --------------------------------------------------------------------------------------------------------
void QuarkUpdateStats(double fps)
{
  GGPONetworkStats stats;
  ggpo_get_stats(ggpo, &stats, _otherPlayerIndex);
  // VidSSetStats(fps, stats.network.ping, iDelay);

  // NOTE: This is where the rollback, etc. data is sent.  Pretty sure that 'VidSSetStats' doesn't get used anymore, or is for other overlay systems that we will never see again.....
  VidOverlaySetStats(fps, stats.network.ping, iDelay);
}

// --------------------------------------------------------------------------------------------------------
void QuarkRecordReplay()
{
  bReplayRecord = true;
  bReplayRecording = false;
}

// --------------------------------------------------------------------------------------------------------
void QuarkFinishReplay()
{
  if (!bReplaySupport && bReplayStarted) {
    bReplayStarted = false;
    kNetGame = 0;
    if (bReplayRecording) {
      AviStop();
      bMediaExit = true;
    }
  }
}
