// Functions for recording & replaying input
#include "burner.h"
#include <commdlg.h>
#include <io.h>
#include <filesystem>
#include "version.h"

#include "../../ggpo/src/lib/CGameRecorder.h"

CGameRecorder* _GameRecorder = nullptr;
CReplayFile* _ReplayFile = nullptr;

#ifndef W_OK
#define W_OK 4
#endif

#define MAX_METADATA 1024

wchar_t wszMetadata[MAX_METADATA];
wchar_t wszStartupGame[MAX_PATH];
wchar_t wszAuthorInfo[MAX_METADATA - 64];


EReplayStatus nReplayStatus = REPLAY_STATUS_NONE; // 1 record, 2 replay, 0 nothing
bool bReplayReadOnly = false;
bool bReplayShowMovement = false;
bool bReplayDontClose = false;
INT32 nReplayUndoCount = 0;
bool bReplayFrameCounterDisplay = 1;
TCHAR szFilter[1024];
INT32 movieFlags = 0;
// bool bStartFromReset = true;
TCHAR szCurrentMovieFilename[MAX_PATH] = _T("");      // TODO: Convert to ASCII for linux compatibility
UINT32 nTotalFrames = 0;
UINT32 nReplayCurrentFrame = 0;

#define MOVIE_FLAG_FROM_POWERON (1<<1)

const UINT32 nMovieVersion = 0x0401;
UINT32 nThisMovieVersion = 0;
UINT32 nThisFBVersion = 0;

UINT32 nStartFrame = 0;
static UINT32 nEndFrame;

uint32_t TotalInputSize = 0;
uint32_t PlayerInputSize = 0;

// static FILE* fp = NULL;
// static INT32 nSizeOffset;

static int16_t nPrevInputs[0x0100];

static INT32 nPrintInputsActive[2] = { 0, 0 };

struct MovieExtInfo
{
  // date & time
  UINT32 year, month, day;
  UINT32 hour, minute, second;
};

struct MovieExtInfo MovieInfo = { 0, 0, 0, 0, 0, 0 };

static INT32 ReplayDialog();
static INT32 RecordDialog();

// burner_win32.h
unsigned char nControls[INPUTSIZE];

// -------------------------------------------------------------------------------------------------------------------------
void RecordInput(int packedInputSize)
{
  // TEMP: Return

  // We shouldn't be here w/o an instance!
  if (!_GameRecorder) { return; }

  auto frame = GetCurrentFrame();
  _GameRecorder->AddInputs(frame, (uint8_t*)(&nControls), packedInputSize);

  return;

  // LEGACY:
  //struct BurnInputInfo bii;
  //memset(&bii, 0, sizeof(bii));

  //for (UINT32 i = 0; i < nGameInpCount; i++) {
  //  BurnDrvGetInputInfo(&bii, i);
  //  if (bii.pVal) {
  //    if (bii.nType & BIT_GROUP_ANALOG) {
  //      if (*bii.pShortVal != nPrevInputs[i]) {
  //        EncodeBuffer(i);
  //        EncodeBuffer(*bii.pShortVal >> 8);
  //        EncodeBuffer(*bii.pShortVal & 0xFF);
  //        nPrevInputs[i] = *bii.pShortVal;
  //      }
  //    }
  //    else {
  //      if (*bii.pVal != nPrevInputs[i]) {
  //        EncodeBuffer(i);
  //        EncodeBuffer(*bii.pVal);
  //        nPrevInputs[i] = *bii.pVal;
  //      }
  //    }
  //  }
  //}
  //EncodeBuffer(0xFF);

  //if (nReplayExternalDataCount && ReplayExternalData) {
  //  for (INT32 i = 0; i < nReplayExternalDataCount; i++) {
  //    EncodeBuffer(ReplayExternalData[i]);
  //  }
  //}

  //if (bReplayFrameCounterDisplay) {
  //  wchar_t framestring[15];
  //  swprintf(framestring, L"%d", GetCurrentFrame() - nStartFrame);
  //  VidSNewTinyMsg(framestring);
  //}

  //// return 0;
}

// -------------------------------------------------------------------------------------------------------------------------
static void CheckRedraw()
{
  if (bRunPause) {
    VidRedraw();                                // Redraw screen so status doesn't get clobbered
    VidPaint(0);                                // ""
  }
}

static void PrintInputsReset()
{
  nPrintInputsActive[0] = nPrintInputsActive[1] = -(60 * 5);
}

static inline void PrintInputsSetActive(UINT8 plyrnum)
{
  nPrintInputsActive[plyrnum] = GetCurrentFrame();
}

static void PrintInputs()
{
  struct BurnInputInfo bii;

  UINT8 UDLR[2][4]; // P1 & P2 joystick movements
  UINT8 BUTTONS[2][8]; // P1 and P2 buttons
  UINT8 OFFBUTTONS[2][8]; // P1 and P2 buttons
  wchar_t lines[3][64];

  memset(&lines, 0, sizeof(lines));
  memset(UDLR, 0, sizeof(UDLR));
  memset(BUTTONS, ' ', sizeof(BUTTONS));
  memset(OFFBUTTONS, ' ', sizeof(OFFBUTTONS));

  for (UINT32 i = 0; i < nGameInpCount; i++) {
    memset(&bii, 0, sizeof(bii));
    BurnDrvGetInputInfo(&bii, i);
    if (bii.pVal && bii.szInfo && bii.szInfo[0]) {
      // Translate X/Y axis to UDLR, TODO: (maybe) support mouse axis/buttons
      if ((bii.nType & BIT_GROUP_ANALOG) && bii.pShortVal && *bii.pShortVal) {
        if (stricmp(bii.szInfo + 2, " x-axis") == 0 && bii.nType == BIT_GROUP_ANALOG) {
          if (bii.szInfo[1] == '1' || bii.szInfo[1] == '2') {
            if ((INT16)*bii.pShortVal > 0x80)
              UDLR[(bii.szInfo[1] - '1')][3] = 1;  // Right
            if ((INT16)*bii.pShortVal < -0x80)
              UDLR[(bii.szInfo[1] - '1')][2] = 1;  // Left
            PrintInputsSetActive(bii.szInfo[1] - '1');
          }
        }
        if (stricmp(bii.szInfo + 2, " y-axis") == 0 && bii.nType == BIT_GROUP_ANALOG) {
          if (bii.szInfo[1] == '1' || bii.szInfo[1] == '2') {
            if ((INT16)*bii.pShortVal > 0x80)
              UDLR[(bii.szInfo[1] - '1')][1] = 1;  // Down
            if ((INT16)*bii.pShortVal < -0x80)
              UDLR[(bii.szInfo[1] - '1')][0] = 1;  // Up
            PrintInputsSetActive(bii.szInfo[1] - '1');
          }
        }
      }
      if (*bii.pVal) { // Button pressed
        if (stricmp(bii.szInfo + 2, " Up") == 0) {
          if (bii.szInfo[1] == '1' || bii.szInfo[1] == '2') {
            UDLR[(bii.szInfo[1] - '1')][0] = 1;
            PrintInputsSetActive(bii.szInfo[1] - '1');
          }
        }
        if (stricmp(bii.szInfo + 2, " Down") == 0) {
          if (bii.szInfo[1] == '1' || bii.szInfo[1] == '2') {
            UDLR[(bii.szInfo[1] - '1')][1] = 1;
            PrintInputsSetActive(bii.szInfo[1] - '1');
          }
        }
        if (stricmp(bii.szInfo + 2, " Left") == 0) {
          if (bii.szInfo[1] == '1' || bii.szInfo[1] == '2') {
            UDLR[(bii.szInfo[1] - '1')][2] = 1;
            PrintInputsSetActive(bii.szInfo[1] - '1');
          }
        }
        if (stricmp(bii.szInfo + 2, " Right") == 0) {
          if (bii.szInfo[1] == '1' || bii.szInfo[1] == '2') {
            UDLR[(bii.szInfo[1] - '1')][3] = 1;
            PrintInputsSetActive(bii.szInfo[1] - '1');
          }
        }
        if (strnicmp(bii.szInfo + 2, " fire ", 6) == 0) {
          if (bii.szInfo[1] == '1' || bii.szInfo[1] == '2') {
            if (bii.szInfo[8] - '1' < 6) { // avoid overflow
              BUTTONS[(bii.szInfo[1] - '1')][bii.szInfo[8] - '1'] = bii.szInfo[8];
              PrintInputsSetActive(bii.szInfo[1] - '1');
            }
          }
        }
      }
      else { // get "off" buttons
        if (strnicmp(bii.szInfo + 2, " fire ", 6) == 0) {
          if (bii.szInfo[1] == '1' || bii.szInfo[1] == '2') {
            if (bii.szInfo[8] - '1' < 6) // avoid overflow
              OFFBUTTONS[(bii.szInfo[1] - '1')][bii.szInfo[8] - '1'] = bii.szInfo[8];
          }
        }
      }
    }
  }

  VidSNewJoystickMsg(NULL); // Clear surface.
  // Draw shadows
  if (GetCurrentFrame() < nPrintInputsActive[0] + (60 * 5)) {
    swprintf(lines[0], L"  ^   %c%c  ", OFFBUTTONS[0][0], OFFBUTTONS[0][1]);
    swprintf(lines[1], L" < >  %c%c  ", OFFBUTTONS[0][2], OFFBUTTONS[0][3]);
    swprintf(lines[2], L"  v   %c%c  ", OFFBUTTONS[0][4], OFFBUTTONS[0][5]);
    VidSNewJoystickMsg(lines[0], 0x404040, 20, 0);
    VidSNewJoystickMsg(lines[1], 0x404040, 20, 1);
    VidSNewJoystickMsg(lines[2], 0x404040, 20, 2);
  }

  if (GetCurrentFrame() < nPrintInputsActive[1] + (60 * 5)) {  // time out np2active after 200 frames or so...
    swprintf(lines[0], L"            ^   %c%c  ", OFFBUTTONS[1][0], OFFBUTTONS[1][1]);
    swprintf(lines[1], L"           < >  %c%c  ", OFFBUTTONS[1][2], OFFBUTTONS[1][3]);
    swprintf(lines[2], L"            v   %c%c  ", OFFBUTTONS[1][4], OFFBUTTONS[1][5]);
    VidSNewJoystickMsg(lines[0], 0x404040, 20, 0);
    VidSNewJoystickMsg(lines[1], 0x404040, 20, 1);
    VidSNewJoystickMsg(lines[2], 0x404040, 20, 2);
  }

  // Draw active buttons
  INT32 nLen = 0;
  for (INT32 i = 0; i < 2; i++) {
    if (i == 1) nLen = _tcslen(lines[0]); // Create the textual mini-joystick icons
    swprintf(lines[0] + nLen, L"  %c   %c%c  ", UDLR[i][0] ? '^' : ' ', BUTTONS[i][0], BUTTONS[i][1]);
    swprintf(lines[1] + nLen, L" %c %c  %c%c  ", UDLR[i][2] ? '<' : ' ', UDLR[i][3] ? '>' : ' ', BUTTONS[i][2], BUTTONS[i][3]);
    swprintf(lines[2] + nLen, L"  %c  %c%c  ", UDLR[i][1] ? 'v' : ' ', BUTTONS[i][4], BUTTONS[i][5]);
  }
  VidSNewJoystickMsg(lines[0], 0xffffff, 20, 0); // Draw them
  VidSNewJoystickMsg(lines[1], 0xffffff, 20, 1);
  VidSNewJoystickMsg(lines[2], 0xffffff, 20, 2);
}

// ----------------------------------------------------------------------------------------------------------
INT32 ReplayInput()
{
  struct BurnInputInfo bii;
  memset(&bii, 0, sizeof(bii));

  // Just to be safe, restore the inputs to the known correct settings.
  // Just to be safe?  I guess there is some concern that some other step in the process
  // is corrupting the inputs, but.... we are setting them again in the next block.....
  // This code may not be needed...
  for (UINT32 i = 0; i < nGameInpCount; i++) {
    BurnDrvGetInputInfo(&bii, i);
    if (bii.pVal) {
      if (bii.nType & BIT_GROUP_ANALOG) {
        *bii.pShortVal = nPrevInputs[i];
      }
      else {
        *bii.pVal = nPrevInputs[i];
      }
    }
  }

  // Now read all inputs that need to change from the replay file.
  // nCurrentFrame
  GameInput gi;
  bool hasInput = _ReplayFile->GetNextInput(gi);

  if (hasInput) {

    // TODO: Frame check for parity....
    if (gi.frame != nCurrentFrame) {
      // NOTE: this is temp check while we are getting the most basic version of the feature
      // running the way that we would expect it to.
      throw runtime_error("next input is wrong!");
    }

    memcpy_s(nControls, INPUTSIZE, gi.bits, TotalInputSize);
    UnpackGameInputs(PlayerInputSize);

    // This is where we will unpack the inputs and shove them back into the driver memory.

    //UINT8 n;
  //while ((n = DecodeBuffer()) != 0xFF) {
  //  BurnDrvGetInputInfo(&bii, n);
  //  if (bii.pVal) {
  //    if (bii.nType & BIT_GROUP_ANALOG) {
  //      *bii.pShortVal = nPrevInputs[n] = (DecodeBuffer() << 8) | DecodeBuffer();
  //    }
  //    else {
  //      *bii.pVal = nPrevInputs[n] = DecodeBuffer();
  //    }
  //  }
  //  else {
  //    DecodeBuffer();
  //  }
  //}



    if (bReplayFrameCounterDisplay) {
      wchar_t framestring[32];
      swprintf(framestring, L"%d / %d", GetCurrentFrame() - nStartFrame, nTotalFrames);
      VidSNewTinyMsg(framestring);
    }

    if (bReplayShowMovement) {
      PrintInputs();
    }
  }
  else {
    StopReplay();
    return 1;
  }

  return 0;

  //UINT8 n;
  //while ((n = DecodeBuffer()) != 0xFF) {
  //  BurnDrvGetInputInfo(&bii, n);
  //  if (bii.pVal) {
  //    if (bii.nType & BIT_GROUP_ANALOG) {
  //      *bii.pShortVal = nPrevInputs[n] = (DecodeBuffer() << 8) | DecodeBuffer();
  //    }
  //    else {
  //      *bii.pVal = nPrevInputs[n] = DecodeBuffer();
  //    }
  //  }
  //  else {
  //    DecodeBuffer();
  //  }
  //}

  // NOTE: "ReplayExternalData" looks to be used exclusively for the MSX driver, which
  // I am not really interested in supporting.  It seems that the 'external data' stuff
  // is kind of a hack around not wanting to define every keyboard key in the normal input
  // system.  Since I care about making the future input system better, I am going to punt
  // and we will just live with breaking MSX replay support.
  // Not really worried about it since it seems to only be enabled for win32 builds anyway....
  //if (nReplayExternalDataCount && ReplayExternalData) {
  //  for (INT32 i = 0; i < nReplayExternalDataCount; i++) {
  //    ReplayExternalData[i] = DecodeBuffer();
  //  }
  //}

//
//#if 0
//  if ((GetCurrentFrame() - nStartFrame) == (nTotalFrames - 1)) {
//    bRunPause = 1; // pause at the last recorded frame? causes weird issues when pauses.  investigate later.. -dink
//  }
//#endif
//
//  if (end_of_buffer) {
//  }
//  else {
//    return 0;
//  }

}

static void MakeOfn(TCHAR* pszFilter)
{
  _stprintf(pszFilter, FBALoadStringEx(hAppInst, IDS_DISK_FILE_REPLAY, true), _T(APP_TITLE));
  memcpy(pszFilter + _tcslen(pszFilter), _T(" (*.fr)\0*.fr\0\0"), 14 * sizeof(TCHAR));

  memset(&ofn, 0, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = hScrnWnd;
  ofn.lpstrFilter = pszFilter;
  ofn.lpstrFile = szChoice;
  ofn.nMaxFile = sizeof(szChoice) / sizeof(TCHAR);
  ofn.lpstrInitialDir = _T(".\\recordings");
  ofn.Flags = OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
  ofn.lpstrDefExt = _T("fr");

  return;
}

// -------------------------------------------------------------------------------------------------------------------------
INT32 StartRecord()
{
  INT32 nRet;
  INT32 bOldPause;

  movieFlags = 0;

  bOldPause = bRunPause;
  bRunPause = 1;
  nRet = RecordDialog();
  bRunPause = bOldPause;

  if (nRet == 0) {
    return 1;
  }

  bReplayReadOnly = false;
  bReplayShowMovement = false;

  // We always reset the emulator to do a recording.  This is the way!
  // Later on, I guess we could record inputs at some arbitrary point, but I don't really see the purpose....
  // OPTIONS: TODO: Sync this with the other inline constant of the same name (~replay.cpp:883)
  const bool START_FROM_RESET = true;
  if (START_FROM_RESET) {
    movieFlags |= MOVIE_FLAG_FROM_POWERON;
    if (!StartFromReset(NULL)) {
      bprintf(0, _T("*** Replay(record): error starting game.\n"));
      movieFlags = 0;
      return 1;
    }
  }

  {
    // This is where we will setup the game recording....
    _tcscpy(szCurrentMovieFilename, szChoice);

    char converted[MAX_PATH];
    TCHARToANSI(szChoice, converted, MAX_PATH);
    string usePath(converted);

    //std::string version;
    //version.append(
    char version[16];
    memset(version, 0, 16);

    // TODO: Check for this formatting on the window as well.
    sprintf_s(version, 16, "%d.%d.%d-%d", VER_MAJOR, VER_MINOR, VER_REVISION, VER_GGPO);


    TCHAR* unicodeRomName = BurnDrvGetText(DRV_NAME);
    char romName[CGameData::MAX_GAME_NAME_SIZE];

    WideCharToMultiByte(CP_UTF8, 0, unicodeRomName, -1, romName, CGameData::MAX_GAME_NAME_SIZE, NULL, NULL);

    // This will tell us the correct size for the inputs for the current game.
    // May be a better way to do this in the future, like from the gamedef directly....
    TotalInputSize = PackGameInputs();
    PlayerInputSize = TotalInputSize / nMaxPlayers;

    CGameData gd;
    gd.SetGameName(romName);
    gd.SetVersion(version);
    gd.MaxPlayerCount = nMaxPlayers;
    gd.TotalInputSize = TotalInputSize;


    // gd.StartFrame = GetCurrentFrame();
    // TODO: Add player names, etc. to CGameData!
    // NOTE: If we are adding state / statefiles, then it needs to happen here!
    // We will pass it into the game recorder...
    // TODO: We should probably just use a replay file directly.  I don't think that we need the game
    // recorder functionality for the emulator... that is more of an appliance kind of thing?

    _GameRecorder = new CGameRecorder(gd, usePath, true);
  }

  nReplayCurrentFrame = GetCurrentFrame();
  nReplayStatus = REPLAY_STATUS_RECORD;
  nReplayUndoCount = 0;

}

// -------------------------------------------------------------------------------------------------------------------------
INT32 StartReplay(const TCHAR* szFileName)
{
  // TEMP: This is currently disabled:
  // return 0;

  // LEGACY:
  INT32 nRet;
  INT32 bOldPause;

  PrintInputsReset();

  if (szFileName) {
    _tcscpy(szChoice, szFileName);
    if (!bReplayDontClose) {
      // if bStartFromReset, get file "wszStartupGame" from metadata!!
      DisplayReplayProperties(0, false);
    }
  }
  else {
    bOldPause = bRunPause;
    bRunPause = 1;
    nRet = ReplayDialog();
    bRunPause = bOldPause;

    if (nRet == 0) {
      return 1;
    }

  }
  _tcscpy(szCurrentMovieFilename, szChoice);

  nReplayStatus = EReplayStatus::REPLAY_STATUS_REPLAY;
  CheckRedraw();

  MenuEnableItems();

  nCurrentFrame = 0;
  nReplayCurrentFrame = 0;

  CGameData gameData = _ReplayFile->GameData();
  TotalInputSize = gameData.TotalInputSize;
  PlayerInputSize = gameData.TotalInputSize / gameData.MaxPlayerCount;

  // TotalInputSize = _ReplayFile->TotalInputSize();

  // NOTE: We are not taking the legacy approach of setting the inputs here.
  // I don't think that it should be necessary, but I guess we will find out!

  //// LEGACY:  This is setting the initial value of the inputs... is it for frame #1?  I think so since
  //struct BurnInputInfo bii;
  //memset(&bii, 0, sizeof(bii));
  // LoadCompressedFile();
  // it is also assigning 'nprevinputs'...
  //// I guess that this is required for proper playback in the old system....
  //// Get the baseline
  //for (UINT32 i = 0; i < nGameInpCount; i++) {
  //  BurnDrvGetInputInfo(&bii, i);
  //  if (bii.pVal) {
  //    if (bii.nType & BIT_GROUP_ANALOG) {
  //      *bii.pShortVal = nPrevInputs[i] = (DecodeBuffer() << 8) | DecodeBuffer();

  //    }
  //    else {
  //      *bii.pVal = nPrevInputs[i] = DecodeBuffer();
  //    }
  //  }
  //  else {
  //    DecodeBuffer();
  //  }
  //}



//#ifdef FBNEO_DEBUG
//  debugPrintf(_T("*** Replay of file %s started.\n"), szChoice);
//#endif
//
//  return 0;
}

// -------------------------------------------------------------------------------------------------------------------------
static void CloseRecord()
{
  // INT32 nFrames = GetCurrentFrame() - nStartFrame;
  auto curFrame = GetCurrentFrame();

  _GameRecorder->CompleteReplay(curFrame, ECompletionReason::NormalDisconnect, EErrorReason::None, "");
  // TODO: _DEL
  delete(_GameRecorder);
  _GameRecorder = nullptr;

  // LEGACY:
  // WriteCompressedFile();

  //fseek(fp, 0, SEEK_END);
  //INT32 nMetadataOffset = ftell(fp);
  //INT32 nChunkSize = ftell(fp) - 4 - nSizeOffset;		// Fill in chunk size and no of recorded frames
  //fseek(fp, nSizeOffset, SEEK_SET);
  //fwrite(&nChunkSize, 1, 4, fp);
  //fwrite(&nFrames, 1, 4, fp);
  //fwrite(&nReplayUndoCount, 1, 4, fp);

  //// NOTE: chunk should be aligned here, since the compressed
  //// file code writes 4 bytes at a time

  //// write metadata
  //INT32 nMetaLen = wcslen(wszMetadata);
  //if (nMetaLen > 0) {
  //  fseek(fp, nMetadataOffset, SEEK_SET);
  //  const char szChunkHeader[] = "FRM1";
  //  fwrite(szChunkHeader, 1, 4, fp);
  //  INT32 nMetaSize = nMetaLen * 2;
  //  fwrite(&nMetaSize, 1, 4, fp);
  //  UINT8* metabuf = (UINT8*)malloc(nMetaSize);
  //  INT32 i;
  //  for (i = 0; i < nMetaLen; ++i) {
  //    metabuf[i * 2 + 0] = wszMetadata[i] & 0xff;
  //    metabuf[i * 2 + 1] = (wszMetadata[i] >> 8) & 0xff;
  //  }
  //  fwrite(metabuf, 1, nMetaSize, fp);
  //  free(metabuf);
  //}

  //fclose(fp);
  //fp = NULL;
  //if (bReplayDontClose) {
  //  if (!StartReplay(szCurrentMovieFilename)) return;
  //}
}

// -------------------------------------------------------------------------------------------------------------------------
static void CloseReplay()
{
}

// -------------------------------------------------------------------------------------------------------------------------
void StopReplay()
{
  if (nReplayStatus) {
    if (nReplayStatus == REPLAY_STATUS_RECORD) {

#ifdef FBNEO_DEBUG
      debugPrintf(_T(" ** Recording stopped, recorded %d frames.\n"), GetCurrentFrame() - nStartFrame);
#endif
      CloseRecord();
    }
    else {
#ifdef FBNEO_DEBUG
      debugPrintf(_T(" ** Replay stopped, replayed %d frames.\n"), GetCurrentFrame() - nStartFrame);
#endif

      CloseReplay();
    }
    nReplayStatus = REPLAY_STATUS_NONE;
    nStartFrame = 0;
    // memset(&MovieInfo, 0, sizeof(MovieInfo));
    CheckRedraw();
    MenuEnableItems();
  }
}


//#
//#             Input Status Freezing
//#
//##############################################################################

static inline void Write32(UINT8*& ptr, const unsigned long v)
{
  *ptr++ = (UINT8)(v & 0xff);
  *ptr++ = (UINT8)((v >> 8) & 0xff);
  *ptr++ = (UINT8)((v >> 16) & 0xff);
  *ptr++ = (UINT8)((v >> 24) & 0xff);
}

static inline UINT32 Read32(const UINT8*& ptr)
{
  UINT32 v;
  v = (UINT32)(*ptr++);
  v |= (UINT32)((*ptr++) << 8);
  v |= (UINT32)((*ptr++) << 16);
  v |= (UINT32)((*ptr++) << 24);
  return v;
}

static inline void Write16(UINT8*& ptr, const UINT16 v)
{
  *ptr++ = (UINT8)(v & 0xff);
  *ptr++ = (UINT8)((v >> 8) & 0xff);
}

static inline UINT16 Read16(const UINT8*& ptr)
{
  UINT16 v;
  v = (UINT16)(*ptr++);
  v |= (UINT16)((*ptr++) << 8);
  return v;
}

// NOTE: These functions are used when saving + loading a game state, which of course means that it is assumed that every
// game will use the same inputs.  Fine for now, but future input system will need to take this into account.
// NOTE: These function do essentially the same thing as 'PackGameInputs, but use a lot more space.
INT32 SaveInputState(UINT8** buf, INT32* size)
{
  *size = 4 + 2 * nGameInpCount;
  *buf = (UINT8*)malloc(*size);
  if (!*buf)
  {
    return -1;
  }

  UINT8* ptr = *buf;
  Write32(ptr, nGameInpCount);

  for (UINT32 i = 0; i < nGameInpCount; i++)
  {
    Write16(ptr, nPrevInputs[i]);
  }

  return 0;
}

INT32 LoadInputState(const UINT8* buf, INT32 size)
{
  UINT32 n = Read32(buf);
  if (n > 0x100 || (unsigned)size < (4 + 2 * n))
  {
    return -1;
  }

  for (UINT32 i = 0; i < n; i++)
  {
    nPrevInputs[i] = Read16(buf);
  }

  return 0;
}

//------------------------------------------------------

static void GetRecordingPath(wchar_t* szPath)
{
  wchar_t szDrive[MAX_PATH];
  wchar_t szDirectory[MAX_PATH];
  wchar_t szFilename[MAX_PATH];
  wchar_t szExt[MAX_PATH];
  szDrive[0] = '\0';
  szDirectory[0] = '\0';
  szFilename[0] = '\0';
  szExt[0] = '\0';
  _wsplitpath(szPath, szDrive, szDirectory, szFilename, szExt);
  if (szDrive[0] == '\0' && szDirectory[0] == '\0') {
    wchar_t szTmpPath[MAX_PATH];
    wcscpy(szTmpPath, L"recordings\\");
    wcsncpy(szTmpPath + wcslen(szTmpPath), szPath, MAX_PATH - wcslen(szTmpPath));
    szTmpPath[MAX_PATH - 1] = '\0';
    wcscpy(szPath, szTmpPath);
  }
}

static void DisplayPropertiesError(HWND hDlg, INT32 nErrType)
{
  if (hDlg != 0) {
    switch (nErrType) {
    case 0:
      SetDlgItemTextW(hDlg, IDC_METADATA, _T("ERROR: Not a FBAlpha input recording file.\0"));
      break;
    case 1:
      SetDlgItemTextW(hDlg, IDC_METADATA, _T("ERROR: Incompatible file-type.  Try playback with an earlier version of FBAlpha.\0"));
      break;
    case 2:
      SetDlgItemTextW(hDlg, IDC_METADATA, _T("ERROR: Recording is corrupt :(\0"));
      break;
    }
  }
}

// -------------------------------------------------------------------------------------------------------------------------
void DisplayReplayProperties(HWND hDlg, bool bClear)
{

  if (hDlg != 0) {
    // save status of read only checkbox
    static bool bReadOnlyStatus = true;
    if (IsWindowEnabled(GetDlgItem(hDlg, IDC_READONLY))) {
      bReadOnlyStatus = (BST_CHECKED == SendDlgItemMessage(hDlg, IDC_READONLY, BM_GETCHECK, 0, 0));
    }

    //bReplayShowMovement = false;
    if (IsWindowEnabled(GetDlgItem(hDlg, IDC_SHOWMOVEMENT))) {
      if (BST_CHECKED == SendDlgItemMessage(hDlg, IDC_SHOWMOVEMENT, BM_GETCHECK, 0, 0)) {
        bReplayShowMovement = true;
      }
    }

    bReplayReadOnly = bReadOnlyStatus;

    // set default values
    SetDlgItemTextA(hDlg, IDC_LENGTH, "");
    SetDlgItemTextA(hDlg, IDC_FRAMES, "");
    SetDlgItemTextA(hDlg, IDC_UNDO, "");
    SetDlgItemTextA(hDlg, IDC_METADATA, "");
    SetDlgItemTextA(hDlg, IDC_REPLAYRESET, "");
    SetDlgItemTextA(hDlg, IDC_REPLAYTIME, "");
    EnableWindow(GetDlgItem(hDlg, IDC_READONLY), FALSE);
    SendDlgItemMessage(hDlg, IDC_READONLY, BM_SETCHECK, BST_UNCHECKED, 0);

    EnableWindow(GetDlgItem(hDlg, IDC_SHOWMOVEMENT), FALSE);
    SendDlgItemMessage(hDlg, IDC_SHOWMOVEMENT, BM_SETCHECK, BST_UNCHECKED, 0);

    EnableWindow(GetDlgItem(hDlg, IDOK), FALSE);

    if (bClear) {
      return;
    }

    long lCount = SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_GETCOUNT, 0, 0);
    long lIndex = SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_GETCURSEL, 0, 0);
    if (lIndex == CB_ERR) {
      return;
    }

    if (lIndex == lCount - 1) {							// Last item is "Browse..."
      EnableWindow(GetDlgItem(hDlg, IDOK), TRUE);		// Browse is selectable
      return;
    }

    long lStringLength = SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_GETLBTEXTLEN, (WPARAM)lIndex, 0);
    if (lStringLength + 1 > MAX_PATH) {
      return;
    }

    SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_GETLBTEXT, (WPARAM)lIndex, (LPARAM)szChoice);

    // check relative path
    GetRecordingPath(szChoice);
  }

  //const char szFileHeader[] = "FB1 ";					// File identifier
  //const char szSavestateHeader[] = "FS1 ";			// Chunk identifier
  //const char szRecordingHeader[] = "FR1 ";			// Chunk identifier
  //const char szMetadataHeader[] = "FRM1";				// Chunk identifier
  //char ReadHeader[4];
  //INT32 nChunkSize = 0;
  //INT32 nChunkDataPosition = 0;
  //INT32 nFileVer = 0;
  //INT32 nFileMin = 0;
  //INT32 t1 = 0, t2 = 0;
  //INT32 nFrames = 0;
  //INT32 nUndoCount = 0;
  //wchar_t* local_metadata = NULL;

  memset(&wszStartupGame, 0, sizeof(wszStartupGame));
  memset(&wszAuthorInfo, 0, sizeof(wszAuthorInfo));

  // Open the replay file:
  bool exists = false;
  try {
    if (_ReplayFile == nullptr) {
      // GetRecordingPath(szChoice);

      // TODO: We would convert this to a UTF-8 string correctly.
      const UINT UTF8_SIZE = MAX_PATH * 2;
      char utf8Path[UTF8_SIZE]; //   = WideCharToMultiByte
      WideCharToMultiByte(CP_UTF8, 0, szChoice, -1, utf8Path, UTF8_SIZE, nullptr, nullptr);

      auto path = filesystem::path(utf8Path);
      if (std::filesystem::exists(path))
      {
        exists = true;
        _ReplayFile = new CReplayFile(path);
      }

    }
  }
  catch (std::exception err) {
    _ReplayFile == nullptr;
  }
  if (!exists) { return; }

  // if (_ReplayFile == nullptr) { return; }
  //FILE* fd = _wfopen(szChoice, L"r+b");
  //if (!fd) {
  //  return;
  //}

  if (hDlg != 0) {
    if (_waccess(szChoice, W_OK)) {
      SendDlgItemMessage(hDlg, IDC_READONLY, BM_SETCHECK, BST_CHECKED, 0);
    }
    else {
      EnableWindow(GetDlgItem(hDlg, IDC_READONLY), TRUE);
      SendDlgItemMessage(hDlg, IDC_READONLY, BM_SETCHECK, (bReplayReadOnly) ? BST_CHECKED : BST_UNCHECKED, 0); //read-only by default
    }

    EnableWindow(GetDlgItem(hDlg, IDC_SHOWMOVEMENT), TRUE);
    SendDlgItemMessage(hDlg, IDC_SHOWMOVEMENT, BM_SETCHECK, (bReplayShowMovement) ? BST_CHECKED : BST_UNCHECKED, 0);
  }

  if (_ReplayFile == nullptr) {
    // File exists, but there was an error reading it.
    // TOOD: We could add more error info, but who cares.
    DisplayPropertiesError(hDlg, 0 /* not our file */);
    return;
  }

  // TEMP:OPTIONS:
  const bool START_FROM_RESET = true;  //(movieFlagsTemp & MOVIE_FLAG_FROM_POWERON) ? 1 : 0; // Starts from reset

  if (hDlg != 0) {
    // file exists and is the correct format,
    // so enable the "Ok" button
    EnableWindow(GetDlgItem(hDlg, IDOK), TRUE);

    // turn nFrames into a length string
    int nFrames = _ReplayFile->TotalFrames();

    INT32 nSeconds = (nFrames * 100 + (nBurnFPS >> 1)) / nBurnFPS;
    INT32 nMinutes = nSeconds / 60;
    INT32 nHours = nSeconds / 3600;

    // write strings to dialog
    char szFramesString[32];
    char szLengthString[32];
    char szUndoCountString[32];
    char szRecordedFrom[32];
    char szRecordedTime[32] = { 0 };

    sprintf(szFramesString, "%d", nFrames);
    sprintf(szLengthString, "%02d:%02d:%02d", nHours, nMinutes % 60, nSeconds % 60);
    sprintf(szUndoCountString, "%d", 0);

    //if (nThisFBVersion && !0) { nFileVer = nThisFBVersion; }
    //if (nFileVer)
    //  sprintf(szRecordedFrom, "%s, v%x.%x.%x.%02x", (START_FROM_RESET) ? "Power-On" : "Savestate", nFileVer >> 20, (nFileVer >> 16) & 0x0F, (nFileVer >> 8) & 0xFF, nFileVer & 0xFF);
    //else

    // TODO: File version can be printed somewhere else.....
    sprintf(szRecordedFrom, "%s", (START_FROM_RESET) ? "Power-On" : "Savestate");

    if (nThisMovieVersion >= 0x0401) {
      sprintf(szRecordedTime, "%02d/%02d/%04d @ %02d:%02d:%02d%s", 0, 0, 0, 0, 0, 0, "xm");
    }

    SetDlgItemTextA(hDlg, IDC_LENGTH, szLengthString);
    SetDlgItemTextA(hDlg, IDC_FRAMES, szFramesString);
    // SetDlgItemTextA(hDlg, IDC_UNDO, szUndoCountString);
    // SetDlgItemTextW(hDlg, IDC_METADATA, wszAuthorInfo);
    SetDlgItemTextA(hDlg, IDC_REPLAYRESET, szRecordedFrom);
    // SetDlgItemTextA(hDlg, IDC_REPLAYTIME, szRecordedTime);
  }
}

static BOOL CALLBACK ReplayDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM)
{
  if (Msg == WM_INITDIALOG) {
    wchar_t szFindPath[MAX_PATH] = L"recordings\\*.fr";
    WIN32_FIND_DATA wfd;
    HANDLE hFind;
    INT32 i = 0;

    SendDlgItemMessage(hDlg, IDC_READONLY, BM_SETCHECK, BST_CHECKED, 0);

    memset(&wfd, 0, sizeof(WIN32_FIND_DATA));
    if (bDrvOkay) {
      _stprintf(szFindPath, _T("recordings\\%s*.fr"), BurnDrvGetText(DRV_NAME));
    }

    hFind = FindFirstFile(szFindPath, &wfd);
    if (hFind != INVALID_HANDLE_VALUE) {
      do {
        if (!(wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
          SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_INSERTSTRING, i++, (LPARAM)wfd.cFileName);
      } while (FindNextFile(hFind, &wfd));
      FindClose(hFind);
    }
    SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_INSERTSTRING, i, (LPARAM)_T("Browse..."));
    SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_SETCURSEL, i, 0);

    if (i >= 1) {
      DisplayReplayProperties(hDlg, false);
      SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_SETCURSEL, i, 0);
    }

    SetFocus(GetDlgItem(hDlg, IDC_CHOOSE_LIST));
    return FALSE;
  }

  if (Msg == WM_COMMAND) {
    if (HIWORD(wParam) == CBN_SELCHANGE) {
      LONG lCount = SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_GETCOUNT, 0, 0);
      LONG lIndex = SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_GETCURSEL, 0, 0);
      if (lIndex != CB_ERR) {
        DisplayReplayProperties(hDlg, (lIndex == lCount - 1));		// Selecting "Browse..." will clear the replay properties display
      }
    }
    else if (HIWORD(wParam) == CBN_CLOSEUP) {
      LONG lCount = SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_GETCOUNT, 0, 0);
      LONG lIndex = SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_GETCURSEL, 0, 0);
      if (lIndex != CB_ERR) {
        if (lIndex == lCount - 1) {
          // send an OK notification to open the file browser
          SendMessage(hDlg, WM_COMMAND, (WPARAM)IDOK, 0);
        }
      }
    }
    else {
      INT32 wID = LOWORD(wParam);
      switch (wID) {
      case IDOK:
      {
        LONG lCount = SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_GETCOUNT, 0, 0);
        LONG lIndex = SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_GETCURSEL, 0, 0);
        if (lIndex != CB_ERR) {
          if (lIndex == lCount - 1) {
            MakeOfn(szFilter);
            ofn.lpstrTitle = FBALoadStringEx(hAppInst, IDS_REPLAY_REPLAY, true);
            //ofn.Flags &= ~OFN_HIDEREADONLY;

            INT32 nRet = GetOpenFileName(&ofn); // Browse...
            if (nRet != 0) {
              LONG lOtherIndex = SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_FINDSTRING, (WPARAM)-1, (LPARAM)szChoice);
              if (lOtherIndex != CB_ERR) {
                // select already existing string
                SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_SETCURSEL, lOtherIndex, 0);
              }
              else {
                SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_INSERTSTRING, lIndex, (LPARAM)szChoice);
                SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_SETCURSEL, lIndex, 0);
              }
              // restore focus to the dialog
              SetFocus(GetDlgItem(hDlg, IDC_CHOOSE_LIST));
              DisplayReplayProperties(hDlg, false);
              if (ofn.Flags & OFN_READONLY || bReplayReadOnly) {
                SendDlgItemMessage(hDlg, IDC_READONLY, BM_SETCHECK, BST_CHECKED, 0);
              }
              else {
                SendDlgItemMessage(hDlg, IDC_READONLY, BM_SETCHECK, BST_UNCHECKED, 0);
              }
            }
          }
          else {
            // get readonly status
            bReplayReadOnly = false;
            if (BST_CHECKED == SendDlgItemMessage(hDlg, IDC_READONLY, BM_GETCHECK, 0, 0)) {
              bReplayReadOnly = true;
            }

            // get show movements status
            bReplayShowMovement = false;
            if (BST_CHECKED == SendDlgItemMessage(hDlg, IDC_SHOWMOVEMENT, BM_GETCHECK, 0, 0)) {
              bReplayShowMovement = true;
            }

            EndDialog(hDlg, 1);					// only allow OK if a valid selection was made
          }
        }
      }
      return TRUE;
      case IDCANCEL:
        szChoice[0] = '\0';
        EndDialog(hDlg, 0);
        return FALSE;
      }
    }
  }

  return FALSE;
}

static INT32 ReplayDialog()
{
  return FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_REPLAYINP), hScrnWnd, (DLGPROC)ReplayDialogProc);
}

static INT32 VerifyRecordingAccessMode(wchar_t* szFilename, INT32 mode)
{
  GetRecordingPath(szFilename);
  if (_waccess(szFilename, mode)) {
    return 0;							// not writeable, return failure
  }

  return 1;
}

static void VerifyRecordingFilename(HWND hDlg)
{
  wchar_t szFilename[MAX_PATH];
  GetDlgItemText(hDlg, IDC_FILENAME, szFilename, MAX_PATH);

  // if filename null, or, file exists and is not writeable
  // then disable the dialog controls
  if (szFilename[0] == '\0' ||
    (VerifyRecordingAccessMode(szFilename, 0) != 0 && VerifyRecordingAccessMode(szFilename, W_OK) == 0)) {
    EnableWindow(GetDlgItem(hDlg, IDOK), FALSE);
    EnableWindow(GetDlgItem(hDlg, IDC_METADATA), FALSE);
  }
  else {
    EnableWindow(GetDlgItem(hDlg, IDOK), TRUE);
    EnableWindow(GetDlgItem(hDlg, IDC_METADATA), TRUE);
  }
}

static BOOL CALLBACK RecordDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM)
{
  wchar_t szAuthInfo[MAX_METADATA];

  if (Msg == WM_INITDIALOG) {
    // come up with a unique name
    wchar_t szPath[MAX_PATH];
    wchar_t szFilename[MAX_PATH];

    INT32 i = 0;
    _stprintf(szFilename, _T("%s.fr"), BurnDrvGetText(DRV_NAME));
    wcscpy(szPath, szFilename);
    while (VerifyRecordingAccessMode(szPath, 0) == 1) {
      _stprintf(szFilename, _T("%s-%d.fr"), BurnDrvGetText(DRV_NAME), ++i);
      wcscpy(szPath, szFilename);
    }

    SetDlgItemText(hDlg, IDC_FILENAME, szFilename);
    SetDlgItemTextW(hDlg, IDC_METADATA, L"");
    CheckDlgButton(hDlg, IDC_REPLAYRESET, BST_UNCHECKED);

    VerifyRecordingFilename(hDlg);

    SetFocus(GetDlgItem(hDlg, IDC_METADATA));
    return FALSE;
  }

  if (Msg == WM_COMMAND) {
    if (HIWORD(wParam) == EN_CHANGE) {
      VerifyRecordingFilename(hDlg);
    }
    else {
      INT32 wID = LOWORD(wParam);
      switch (wID) {
      case IDC_BROWSE:
      {
        _stprintf(szChoice, _T("%s"), BurnDrvGetText(DRV_NAME));
        MakeOfn(szFilter);
        ofn.lpstrTitle = FBALoadStringEx(hAppInst, IDS_REPLAY_RECORD, true);
        ofn.Flags |= OFN_OVERWRITEPROMPT;
        INT32 nRet = GetSaveFileName(&ofn);
        if (nRet != 0) {
          // this should trigger an EN_CHANGE message
          SetDlgItemText(hDlg, IDC_FILENAME, szChoice);
        }
      }
      return TRUE;
      case IDOK:
        GetDlgItemText(hDlg, IDC_FILENAME, szChoice, MAX_PATH);
        GetDlgItemTextW(hDlg, IDC_METADATA, szAuthInfo, MAX_METADATA - 64 - 1);

        // NOTE: The restart flag is always set now...
         //bStartFromReset = false;
        //if (BST_CHECKED == SendDlgItemMessage(hDlg, IDC_REPLAYRESET, BM_GETCHECK, 0, 0)) {
        //  bStartFromReset = true;
          // add "romset," to beginning of metadata
        _stprintf(wszMetadata, _T("%s,%s"), BurnDrvGetText(DRV_NAME), szAuthInfo);
        //}
        //else {
        //  _tcscpy(wszMetadata, szAuthInfo);
        //}
        wszMetadata[MAX_METADATA - 1] = L'\0';

        // ensure a relative path has the "recordings\" path in prepended to it
        GetRecordingPath(szChoice);
        EndDialog(hDlg, 1);
        return TRUE;
      case IDCANCEL:
        szChoice[0] = '\0';
        EndDialog(hDlg, 0);
        return FALSE;
      }
    }
  }

  return FALSE;
}

static INT32 RecordDialog()
{
  return FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_RECORDINP), hScrnWnd, (DLGPROC)RecordDialogProc);
}
