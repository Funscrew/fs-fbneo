#pragma once

struct IDirect3DDevice9Ex;

void DetectorLoad(const char *game, bool debug, int seed);
void DetectorUpdate();
void DetectorSetState(int state, int score1, int score2, int start1 = 0, int start2 = 0);
void DetectorGetState(int &state, int &score1, int &score2, int &start1, int &start2);

void VidOverlayInit(IDirect3DDevice9Ex *device);
void VidOverlayEnd();
void VidOverlayQuit();
void VidOverlayRender(const RECT& dest, int gameWidth, int gameHeight, int scan_intensity);

void VidOverlaySetGameInfo(const wchar_t *p1, const wchar_t *p2, int spectator, int ranked, UINT16 playerIndex);
void VidOverlaySetGameScores(int score1, int score2, int winner = 0);
void VidOverlaySetGameSpectators(int num);
void VidOverlaySetSystemMessage(const wchar_t *text);
void VidOverlaySetStats(double fps, int ping, int delay);
void VidOverlaySetWarning(int warning, int line);
void VidOverlayShowVolume(const wchar_t *text);
void VidOverlaySetChatInput(const wchar_t *text);
void VidOverlayAddChatLine(const wchar_t *name, const wchar_t *text);
void VidOverlaySaveFiles(bool save_info, bool save_scores, bool save_characters, int save_winner);
void VidOverlaySaveInfo();
void VidOverlaySaveChatHistory(const wchar_t *text);
bool VidOverlayCanReset();

void VidDebug(const wchar_t *text, float a, float b);
void VidDisplayInputs(int slot, int state);
void DetectTurbo();
void DetectFreeze();
