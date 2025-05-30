// FinalBurn Neo - Emulator for MC68000/Z80 based arcade games
//            Refer to the "license.txt" file for more info

// Main module

// #define USE_SDL					// define if SDL is used
//#define APP_DEBUG_LOG			// log debug messages to zzBurnDebug.html

#ifdef USE_SDL
#include "SDL.h"
#endif

#include "burner.h"

#ifdef _MSC_VER
//  #include <winable.h>
#ifdef _DEBUG
#include <crtdbg.h>
#endif
#endif

#include <wininet.h>
#include <winsock.h>

#if defined (FBNEO_DEBUG)
bool bDisableDebugConsole = true;
#endif

#include "version.h"

HINSTANCE hAppInst = NULL;			// Application Instance
HANDLE hMainThread;
long int nMainThreadID;
int nAppProcessPriority = HIGH_PRIORITY_CLASS;	// High priority class
int nAppShowCmd;

static TCHAR szCmdLine[1024] = _T("");

HACCEL hAccel = NULL;

int nAppVirtualFps = 6000;			// App fps * 100

TCHAR szAppBurnVer[EXE_NAME_SIZE] = _T("");
TCHAR szAppExeName[EXE_NAME_SIZE + 1];

bool bAlwaysProcessKeyboardInput = false;
bool bAlwaysCreateSupportFolders = true;
bool bAutoLoadGameList = false;

bool bQuietLoading = false;

bool bNoChangeNumLock = 1;
static bool bNumlockStatus;

bool bMonitorAutoCheck = true;
bool bKeypadVolume = true;
bool bFixDiagonals = false;
int nEnableSOCD = 0;

// Used for the load/save dialog in commdlg.h (savestates, input replay, wave logging)
TCHAR szChoice[MAX_PATH] = _T("");
OPENFILENAME ofn;

#if defined (UNICODE)
char* TCHARToANSI(const TCHAR* pszInString, char* pszOutString, int nOutSize)
{

	static char szStringBuffer[1024];
	memset(szStringBuffer, 0, sizeof(szStringBuffer));

	char* pszBuffer = pszOutString ? pszOutString : szStringBuffer;
	int nBufferSize = pszOutString ? nOutSize * 2 : sizeof(szStringBuffer);

	if (WideCharToMultiByte(CP_ACP, 0, pszInString, -1, pszBuffer, nBufferSize, NULL, NULL)) {
		return pszBuffer;
	}

	return NULL;
}

TCHAR* ANSIToTCHAR(const char* pszInString, TCHAR* pszOutString, int nOutSize)
{
	static TCHAR szStringBuffer[1024];

	TCHAR* pszBuffer = pszOutString ? pszOutString : szStringBuffer;
	int nBufferSize = pszOutString ? nOutSize * 2 : sizeof(szStringBuffer);

	if (MultiByteToWideChar(CP_ACP, 0, pszInString, -1, pszBuffer, nBufferSize)) {
		return pszBuffer;
	}

	return NULL;
}
#else
char* TCHARToANSI(const TCHAR* pszInString, char* pszOutString, int /*nOutSize*/)
{
	if (pszOutString) {
		strcpy(pszOutString, pszInString);
		return pszOutString;
	}

	return (char*)pszInString;
}

TCHAR* ANSIToTCHAR(const char* pszInString, TCHAR* pszOutString, int /*nOutSize*/)
{
	if (pszOutString) {
		_tcscpy(pszOutString, pszInString);
		return pszOutString;
	}

	return (TCHAR*)pszInString;
}
#endif

CHAR* astring_from_utf8(const char* utf8string)
{
	WCHAR* wstring;
	int char_count;
	CHAR* result;

	// convert MAME string (UTF-8) to UTF-16
	char_count = MultiByteToWideChar(CP_UTF8, 0, utf8string, -1, NULL, 0);
	wstring = (WCHAR*)malloc(char_count * sizeof(*wstring));
	MultiByteToWideChar(CP_UTF8, 0, utf8string, -1, wstring, char_count);

	// convert UTF-16 to "ANSI code page" string
	char_count = WideCharToMultiByte(CP_ACP, 0, wstring, -1, NULL, 0, NULL, NULL);
	result = (CHAR*)malloc(char_count * sizeof(*result));
	if (result != NULL)
		WideCharToMultiByte(CP_ACP, 0, wstring, -1, result, char_count, NULL, NULL);

	if (wstring) {
		free(wstring);
		wstring = NULL;
	}
	return result;
}

char* utf8_from_astring(const CHAR* astring)
{
	WCHAR* wstring;
	int char_count;
	CHAR* result;

	// convert "ANSI code page" string to UTF-16
	char_count = MultiByteToWideChar(CP_ACP, 0, astring, -1, NULL, 0);
	wstring = (WCHAR*)malloc(char_count * sizeof(*wstring));
	MultiByteToWideChar(CP_ACP, 0, astring, -1, wstring, char_count);

	// convert UTF-16 to MAME string (UTF-8)
	char_count = WideCharToMultiByte(CP_UTF8, 0, wstring, -1, NULL, 0, NULL, NULL);
	result = (CHAR*)malloc(char_count * sizeof(*result));
	if (result != NULL)
		WideCharToMultiByte(CP_UTF8, 0, wstring, -1, result, char_count, NULL, NULL);

	if (wstring) {
		free(wstring);
		wstring = NULL;
	}
	return result;
}

WCHAR* wstring_from_utf8(const char* utf8string)
{
	int char_count;
	WCHAR* result;

	// convert MAME string (UTF-8) to UTF-16
	char_count = MultiByteToWideChar(CP_UTF8, 0, utf8string, -1, NULL, 0);
	result = (WCHAR*)malloc(char_count * sizeof(*result));
	if (result != NULL)
		MultiByteToWideChar(CP_UTF8, 0, utf8string, -1, result, char_count);

	return result;
}

char* utf8_from_wstring(const WCHAR* wstring)
{
	int char_count;
	char* result;

	// convert UTF-16 to MAME string (UTF-8)
	char_count = WideCharToMultiByte(CP_UTF8, 0, wstring, -1, NULL, 0, NULL, NULL);
	result = (char*)malloc(char_count * sizeof(*result));
	if (result != NULL)
		WideCharToMultiByte(CP_UTF8, 0, wstring, -1, result, char_count, NULL, NULL);

	return result;
}

#if defined (FBNEO_DEBUG)
static TCHAR szConsoleBuffer[1024];
static int nPrevConsoleStatus = -1;

static HANDLE DebugBuffer;
static FILE* DebugLog = NULL;
static bool bEchoLog = true; // false;
#endif

void tcharstrreplace(TCHAR* pszSRBuffer, const TCHAR* pszFind, const TCHAR* pszReplace)
{
	if (pszSRBuffer == NULL || pszFind == NULL || pszReplace == NULL)
		return;

	int lenFind = _tcslen(pszFind);
	int lenReplace = _tcslen(pszReplace);
	int lenSRBuffer = _tcslen(pszSRBuffer) + 1;

	for (int i = 0; (lenSRBuffer > lenFind) && (i < lenSRBuffer - lenFind); i++) {
		if (!memcmp(pszFind, &pszSRBuffer[i], lenFind * sizeof(TCHAR))) {
			if (lenFind == lenReplace) {
				memcpy(&pszSRBuffer[i], pszReplace, lenReplace * sizeof(TCHAR));
				i += lenReplace - 1;
			}
			else if (lenFind > lenReplace) {
				memcpy(&pszSRBuffer[i], pszReplace, lenReplace * sizeof(TCHAR));
				i += lenReplace;
				int delta = lenFind - lenReplace;
				lenSRBuffer -= delta;
				memmove(&pszSRBuffer[i], &pszSRBuffer[i + delta], (lenSRBuffer - i) * sizeof(TCHAR));
				i--;
			}
			else { /* this part only works on dynamic buffers - the replacement string length must be smaller or equal to the find string length if this is commented out!
			 int delta = lenReplace - lenFind;
			 pszSRBuffer = (TCHAR *)realloc(pszSRBuffer, (lenSRBuffer + delta) * sizeof(TCHAR));
			 memmove(&pszSRBuffer[i + lenReplace], &pszSRBuffer[i + lenFind], (lenSRBuffer - i - lenFind) * sizeof(TCHAR));
			 lenSRBuffer += delta;
			 memcpy(&pszSRBuffer[i], pszReplace, lenReplace * sizeof(TCHAR));
			 i += lenReplace - 1; */
			}
		}
	}
}

#if defined (FBNEO_DEBUG)
// Debug printf to a file
static int __cdecl AppDebugPrintf(int nStatus, TCHAR* pszFormat, ...)
{
	va_list vaFormat;

	va_start(vaFormat, pszFormat);

	if (DebugLog) {

		if (nStatus != nPrevConsoleStatus) {
			switch (nStatus) {
			case PRINT_ERROR:
				_ftprintf(DebugLog, _T("</div><div class=\"error\">"));
				break;
			case PRINT_IMPORTANT:
				_ftprintf(DebugLog, _T("</div><div class=\"important\">"));
				break;
			case PRINT_LEVEL1:
				_ftprintf(DebugLog, _T("</div><div class=\"level1\">"));
				break;
			case PRINT_LEVEL2:
				_ftprintf(DebugLog, _T("</div><div class=\"level2\">"));
				break;
			case PRINT_LEVEL3:
				_ftprintf(DebugLog, _T("</div><div class=\"level3\">"));
				break;
			case PRINT_LEVEL4:
				_ftprintf(DebugLog, _T("</div><div class=\"level4\">"));
				break;
			case PRINT_LEVEL5:
				_ftprintf(DebugLog, _T("</div><div class=\"level5\">"));
				break;
			case PRINT_LEVEL6:
				_ftprintf(DebugLog, _T("</div><div class=\"level6\">"));
				break;
			case PRINT_LEVEL7:
				_ftprintf(DebugLog, _T("</div><div class=\"level7\">"));
				break;
			case PRINT_LEVEL8:
				_ftprintf(DebugLog, _T("</div><div class=\"level8\">"));
				break;
			case PRINT_LEVEL9:
				_ftprintf(DebugLog, _T("</div><div class=\"level9\">"));
				break;
			case PRINT_LEVEL10:
				_ftprintf(DebugLog, _T("</div><div class=\"level10\">"));
				break;
			default:
				_ftprintf(DebugLog, _T("</div><div class=\"normal\">"));
			}
		}
		_vftprintf(DebugLog, pszFormat, vaFormat);
		fflush(DebugLog);
	}

	if (!bDisableDebugConsole && (!DebugLog || bEchoLog)) {
		_vsntprintf(szConsoleBuffer, 1024, pszFormat, vaFormat);

		if (nStatus != nPrevConsoleStatus) {
			switch (nStatus) {
			case PRINT_UI:
				SetConsoleTextAttribute(DebugBuffer, FOREGROUND_INTENSITY);
				break;
			case PRINT_IMPORTANT:
			case PRINT_LEVEL1:
			case PRINT_LEVEL2:
			case PRINT_LEVEL3:
			case PRINT_LEVEL4:
			case PRINT_LEVEL5:
			case PRINT_LEVEL6:
			case PRINT_LEVEL7:
			case PRINT_LEVEL8:
			case PRINT_LEVEL9:
			case PRINT_LEVEL10:
				SetConsoleTextAttribute(DebugBuffer, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
				break;
			case PRINT_ERROR:
				SetConsoleTextAttribute(DebugBuffer, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
				break;
			default:
				SetConsoleTextAttribute(DebugBuffer, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
			}
		}

		tcharstrreplace(szConsoleBuffer, _T(SEPERATOR_1), _T(" * "));
		WriteConsole(DebugBuffer, szConsoleBuffer, _tcslen(szConsoleBuffer), NULL, NULL);
	}

	nPrevConsoleStatus = nStatus;

	va_end(vaFormat);

	return 0;
}
#endif

int debugPrintf(TCHAR* pszFormat, ...)
{
#if defined (FBNEO_DEBUG)
	va_list vaFormat;
	va_start(vaFormat, pszFormat);

	_vsntprintf(szConsoleBuffer, 1024, pszFormat, vaFormat);

	if (nPrevConsoleStatus != PRINT_UI) {
		if (DebugLog) {
			_ftprintf(DebugLog, _T("</div><div class=\"ui\">"));
		}
		if (!bDisableDebugConsole) {
			SetConsoleTextAttribute(DebugBuffer, FOREGROUND_INTENSITY);
		}
		nPrevConsoleStatus = PRINT_UI;
	}

	if (DebugLog) {
		_ftprintf(DebugLog, szConsoleBuffer);
		fflush(DebugLog);
	}

	tcharstrreplace(szConsoleBuffer, _T(SEPERATOR_1), _T(" * "));
	if (!bDisableDebugConsole) {
		WriteConsole(DebugBuffer, szConsoleBuffer, _tcslen(szConsoleBuffer), NULL, NULL);
	}
	va_end(vaFormat);
#else
	(void)pszFormat;
#endif

	return 0;
}

void CloseDebugLog()
{
#if defined (FBNEO_DEBUG)
	if (DebugLog) {

		_ftprintf(DebugLog, _T("</pre></body></html>"));

		fclose(DebugLog);
		DebugLog = NULL;
	}

	if (!bDisableDebugConsole) {
		if (DebugBuffer) {
			CloseHandle(DebugBuffer);
			DebugBuffer = NULL;
		}

		FreeConsole();
	}
#endif
}

int OpenDebugLog()
{
#if defined (FBNEO_DEBUG)

	time_t nTime;
	tm* tmTime;

	time(&nTime);
	tmTime = localtime(&nTime);

	{
		// Initialise the debug log file

#ifdef _UNICODE
		DebugLog = _tfopen(_T("zzBurnDebug.html"), _T("wb"));

		if (ftell(DebugLog) == 0) {
			WRITE_UNICODE_BOM(DebugLog);

			_ftprintf(DebugLog, _T("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">"));
			_ftprintf(DebugLog, _T("<html><head><meta http-equiv=Content-Type content=\"text/html; charset=unicode\"><script src=\"https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js\"></script>"));
			_ftprintf(DebugLog, _T("<style>body{color:#333333;}.error,.error_chb{color:#ff3f3f;}.important,.important_chb{color:#000000;}.normal,.level1,.level2,.level3,.level4,.level5,.level6,.level7,.level8,.level9,.level10,.normal_chb,.level1_chb,.level2_chb,.level3_chb,.level4_chb,.level5_chb,.level6_chb,.level7_chb,.level8_chb,.level9_chb,.level10_chb{color:#009f00;}.ui{color:9f9f9f;}</style>"));
			_ftprintf(DebugLog, _T("</head><body><pre>"));
		}
#else
		DebugLog = _tfopen(_T("zzBurnDebug.html"), _T("wt"));

		if (ftell(DebugLog) == 0) {
			_ftprintf(DebugLog, _T("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">"));
			_ftprintf(DebugLog, _T("<html><head><meta http-equiv=Content-Type content=\"text/html; charset=windows-%i\"></head><body><pre>"), GetACP());
		}
#endif

		_ftprintf(DebugLog, _T("<div style=\"font-size:16px;font-weight:bold;\">"));
		_ftprintf(DebugLog, _T("Debug log created by ") _T(APP_TITLE) _T(" v%.20s on %s"), szAppBurnVer, _tasctime(tmTime));

		_ftprintf(DebugLog, _T("</div><div style=\"margin-top:12px;\"><input type=\"checkbox\" id=\"chb_all\" onclick=\"$('input:checkbox').not(this).prop('checked', this.checked);\" checked>&nbsp;<label for=\"chb_all\">(Un)check All</label>"));
		_ftprintf(DebugLog, _T("</div><div style=\"margin-top:12px;\"><input type=\"checkbox\" id=\"chb_ui\" onclick=\"if(this.checked==true){$(\'.ui\').show();}else{$(\'.ui\').hide();}\" checked>&nbsp;<label for=\"chb_ui\" class=\"ui_chb\">UI</label>"));
		_ftprintf(DebugLog, _T("</div><div><input type=\"checkbox\" id=\"chb_normal\" onclick=\"if(this.checked==true){$(\'.normal\').show();}else{$(\'.normal\').hide();}\" checked>&nbsp;<label for=\"chb_normal\" class=\"normal_chb\">Normal</label>"));
		_ftprintf(DebugLog, _T("</div><div><input type=\"checkbox\" id=\"chb_important\" onclick=\"if(this.checked==true){$(\'.important\').show();}else{$(\'.important\').hide();}\" checked>&nbsp;<label for=\"chb_important\" class=\"important_chb\">Important</label>"));
		_ftprintf(DebugLog, _T("</div><div><input type=\"checkbox\" id=\"chb_error\" onclick=\"if(this.checked==true){$(\'.error\').show();}else{$(\'.error\').hide();}\" checked>&nbsp;<label for=\"chb_error\" class=\"error_chb\">Error</label>"));
		_ftprintf(DebugLog, _T("</div><div><input type=\"checkbox\" id=\"chb_level1\" onclick=\"if(this.checked==true){$(\'.level1\').show();}else{$(\'.level1\').hide();}\" checked>&nbsp;<label for=\"chb_level1\" class=\"level1_chb\">Level 1</label>"));
		_ftprintf(DebugLog, _T("</div><div><input type=\"checkbox\" id=\"chb_level2\" onclick=\"if(this.checked==true){$(\'.level2\').show();}else{$(\'.level2\').hide();}\" checked>&nbsp;<label for=\"chb_level2\" class=\"level2_chb\">Level 2</label>"));
		_ftprintf(DebugLog, _T("</div><div><input type=\"checkbox\" id=\"chb_level3\" onclick=\"if(this.checked==true){$(\'.level3\').show();}else{$(\'.level3\').hide();}\" checked>&nbsp;<label for=\"chb_level3\" class=\"level3_chb\">Level 3</label>"));
		_ftprintf(DebugLog, _T("</div><div><input type=\"checkbox\" id=\"chb_level4\" onclick=\"if(this.checked==true){$(\'.level4\').show();}else{$(\'.level4\').hide();}\" checked>&nbsp;<label for=\"chb_level4\" class=\"level4_chb\">Level 4</label>"));
		_ftprintf(DebugLog, _T("</div><div><input type=\"checkbox\" id=\"chb_level5\" onclick=\"if(this.checked==true){$(\'.level5\').show();}else{$(\'.level5\').hide();}\" checked>&nbsp;<label for=\"chb_level5\" class=\"level5_chb\">Level 5</label>"));
		_ftprintf(DebugLog, _T("</div><div><input type=\"checkbox\" id=\"chb_level6\" onclick=\"if(this.checked==true){$(\'.level6\').show();}else{$(\'.level6\').hide();}\" checked>&nbsp;<label for=\"chb_level6\" class=\"level6_chb\">Level 6</label>"));
		_ftprintf(DebugLog, _T("</div><div><input type=\"checkbox\" id=\"chb_level7\" onclick=\"if(this.checked==true){$(\'.level7\').show();}else{$(\'.level7\').hide();}\" checked>&nbsp;<label for=\"chb_level7\" class=\"level7_chb\">Level 7</label>"));
		_ftprintf(DebugLog, _T("</div><div><input type=\"checkbox\" id=\"chb_level8\" onclick=\"if(this.checked==true){$(\'.level8\').show();}else{$(\'.level8\').hide();}\" checked>&nbsp;<label for=\"chb_level8\" class=\"level8_chb\">Level 8</label>"));
		_ftprintf(DebugLog, _T("</div><div><input type=\"checkbox\" id=\"chb_level9\" onclick=\"if(this.checked==true){$(\'.level9\').show();}else{$(\'.level9\').hide();}\" checked>&nbsp;<label for=\"chb_level9\" class=\"level9_chb\">Level 9</label>"));
		_ftprintf(DebugLog, _T("</div><div style=\"margin-bottom:20px;\"><input type=\"checkbox\" id=\"chb_level10\" onclick=\"if(this.checked==true){$(\'.level10\').show();}else{$(\'.level10\').hide();}\" checked>&nbsp;<label for=\"chb_level10\" class=\"level10_chb\">Level 10</label>"));
	}
#endif

	if (!bDisableDebugConsole)
	{
		// Initialise the debug console

		COORD DebugBufferSize = { 80, 1000 };

		{

			// Since AttachConsole is only present in Windows XP, import it manually

#if _WIN32_WINNT >= 0x0500 && defined (_MSC_VER)
// #error Manually importing AttachConsole() function, but compiling with _WIN32_WINNT >= 0x0500
			if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
				AllocConsole();
			}
#else

#ifdef ATTACH_PARENT_PROCESS
#undef ATTACH_PARENT_PROCESS
#endif
#define ATTACH_PARENT_PROCESS ((DWORD)-1)

			BOOL(WINAPI * pAttachConsole)(DWORD dwProcessId) = NULL;
			HINSTANCE hKernel32DLL = LoadLibrary(_T("kernel32.dll"));

			if (hKernel32DLL) {
				pAttachConsole = (BOOL(WINAPI*)(DWORD))GetProcAddress(hKernel32DLL, "AttachConsole");
			}
			if (pAttachConsole) {
				if (!pAttachConsole(ATTACH_PARENT_PROCESS)) {
					AllocConsole();
				}
			}
			else {
				AllocConsole();
			}
			if (hKernel32DLL) {
				FreeLibrary(hKernel32DLL);
			}

#undef ATTACH_PARENT_PROCESS
#endif

		}

		DebugBuffer = CreateConsoleScreenBuffer(GENERIC_WRITE, FILE_SHARE_READ, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
		SetConsoleScreenBufferSize(DebugBuffer, DebugBufferSize);
		SetConsoleActiveScreenBuffer(DebugBuffer);
		SetConsoleTitle(_T(APP_TITLE) _T(" Debug console"));

		SetConsoleTextAttribute(DebugBuffer, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
		_sntprintf(szConsoleBuffer, 1024, _T("Welcome to the ") _T(APP_TITLE) _T(" debug console.\n"));
		WriteConsole(DebugBuffer, szConsoleBuffer, _tcslen(szConsoleBuffer), NULL, NULL);

		SetConsoleTextAttribute(DebugBuffer, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
		if (DebugLog) {
			_sntprintf(szConsoleBuffer, 1024, _T("Debug messages are logged in zzBurnDebug.html"));
			if (!DebugLog || bEchoLog) {
				_sntprintf(szConsoleBuffer + _tcslen(szConsoleBuffer), 1024 - _tcslen(szConsoleBuffer), _T(", and echod to this console"));
			}
			_sntprintf(szConsoleBuffer + _tcslen(szConsoleBuffer), 1024 - _tcslen(szConsoleBuffer), _T(".\n\n"));
		}
		else {
			_sntprintf(szConsoleBuffer, 1024, _T("Debug messages are echod to this console.\n\n"));
		}
		WriteConsole(DebugBuffer, szConsoleBuffer, _tcslen(szConsoleBuffer), NULL, NULL);
	}

	nPrevConsoleStatus = -1;

	bprintf = AppDebugPrintf;							// Redirect Burn library debug to our function

	return 0;
}

void GetAspectRatio(int x, int y, int* AspectX, int* AspectY)
{
	float aspect_ratio = (float)x / (float)y;

	// Horizontal

	// 4:3
	if (fabs(aspect_ratio - 4.f / 3.f) < 0.01f) {
		*AspectX = 4;
		*AspectY = 3;
		return;
	}

	// 5:4
	if (fabs(aspect_ratio - 5.f / 4.f) < 0.01f) {
		*AspectX = 5;
		*AspectY = 4;
		return;
	}

	// 16:9
	if (fabs(aspect_ratio - 16.f / 9.f) < 0.1f) {
		*AspectX = 16;
		*AspectY = 9;
		return;
	}

	// 16:10
	if (fabs(aspect_ratio - 16.f / 10.f) < 0.1f) {
		*AspectX = 16;
		*AspectY = 10;
		return;
	}

	// 21:9
	if (fabs(aspect_ratio - 21.f / 9.f) < 0.1f) {
		*AspectX = 21;
		*AspectY = 9;
		return;
	}

	// 32:9
	if (fabs(aspect_ratio - 32.f / 9.f) < 0.1f) {
		*AspectX = 32;
		*AspectY = 9;
		return;
	}

	// Vertical

	// 3:4
	if (fabs(aspect_ratio - 3.f / 4.f) < 0.01f) {
		*AspectX = 3;
		*AspectY = 4;
		return;
	}

	// 4:5
	if (fabs(aspect_ratio - 4.f / 5.f) < 0.01f) {
		*AspectX = 4;
		*AspectY = 5;
		return;
	}

	// 9:16
	if (fabs(aspect_ratio - 9.f / 16.f) < 0.1f) {
		*AspectX = 9;
		*AspectY = 16;
		return;
	}

	// 10:16
	if (fabs(aspect_ratio - 9.f / 16.f) < 0.1f) {
		*AspectX = 10;
		*AspectY = 16;
		return;
	}

	// 9:21
	if (fabs(aspect_ratio - 9.f / 21.f) < 0.1f) {
		*AspectX = 9;
		*AspectY = 21;
		return;
	}

	// 9:32
	if (fabs(aspect_ratio - 9.f / 32.f) < 0.1f) {
		*AspectX = 9;
		*AspectY = 32;
		return;
	}
}

static BOOL CALLBACK MonInfoProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
	MONITORINFOEX iMonitor;
	int width, height;

	iMonitor.cbSize = sizeof(MONITORINFOEX);
	GetMonitorInfo(hMonitor, &iMonitor);

	width = iMonitor.rcMonitor.right - iMonitor.rcMonitor.left;
	height = iMonitor.rcMonitor.bottom - iMonitor.rcMonitor.top;

	if ((HorScreen[0] && !_wcsicmp(HorScreen, iMonitor.szDevice)) ||
		(!HorScreen[0] && iMonitor.dwFlags & MONITORINFOF_PRIMARY)) {

		// Set values for horizontal monitor
		nVidHorWidth = width;
		nVidHorHeight = height;

		// also add this to the presets
		VidPreset[3].nWidth = width;
		VidPreset[3].nHeight = height;

		GetAspectRatio(width, height, &nVidScrnAspectX, &nVidScrnAspectY);
	}

	if ((VerScreen[0] && !_wcsicmp(VerScreen, iMonitor.szDevice)) ||
		(!VerScreen[0] && iMonitor.dwFlags & MONITORINFOF_PRIMARY)) {

		// Set values for vertical monitor
		nVidVerWidth = width;
		nVidVerHeight = height;

		// also add this to the presets
		VidPresetVer[3].nWidth = width;
		VidPresetVer[3].nHeight = height;

		GetAspectRatio(width, height, &nVidVerScrnAspectX, &nVidVerScrnAspectY);
	}

	return TRUE;
}

void MonitorAutoCheck()
{
	//RECT rect;
	int numScreens;

	//SystemParametersInfo(SPI_GETWORKAREA, 0, &rect, 0);

	numScreens = GetSystemMetrics(SM_CMONITORS);

	// If only one monitor or not using a DirectX9 blitter, only use primary monitor
	if (numScreens == 1 || nVidSelect < 3) {
		int x, y;

		x = GetSystemMetrics(SM_CXSCREEN);
		y = GetSystemMetrics(SM_CYSCREEN);

		// default full-screen resolution to this size
		nVidHorWidth = x;
		nVidHorHeight = y;
		nVidVerWidth = x;
		nVidVerHeight = y;

		// also add this to the presets
		VidPreset[3].nWidth = x;
		VidPreset[3].nHeight = y;
		VidPresetVer[3].nWidth = x;
		VidPresetVer[3].nHeight = y;

		GetAspectRatio(x, y, &nVidScrnAspectX, &nVidScrnAspectY);
		nVidVerScrnAspectX = nVidScrnAspectX;
		nVidVerScrnAspectY = nVidScrnAspectY;
	}
	else {
		EnumDisplayMonitors(NULL, NULL, MonInfoProc, 0);
	}
}

bool SetNumLock(bool bState)
{
	BYTE keyState[256];

	if (bNoChangeNumLock) return 0;

	GetKeyboardState(keyState);
	if ((bState && !(keyState[VK_NUMLOCK] & 1)) || (!bState && (keyState[VK_NUMLOCK] & 1))) {
		keybd_event(VK_NUMLOCK, 0, KEYEVENTF_EXTENDEDKEY, 0);

		keybd_event(VK_NUMLOCK, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
	}

	return keyState[VK_NUMLOCK] & 1;
}

static int AppInit()
{
#if defined (_MSC_VER) && defined (_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_CHECK_ALWAYS_DF);			// Check for memory corruption
	_CrtSetDbgFlag(_CRTDBG_DELAY_FREE_MEM_DF);			//
	_CrtSetDbgFlag(_CRTDBG_LEAK_CHECK_DF);				//
#endif

	// Create a handle to the main thread of execution
	DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &hMainThread, 0, false, DUPLICATE_SAME_ACCESS);

	// Init the Burn library
	BurnLibInit();

	// Load config for the application
	ConfigAppLoad();

	// @FC force fightcade application settings
	bSkipStartupCheck = 1; // skip startup check
	bVidHardwareVertex = 1; // use hardware vertex
	bVidMotionBlur = 0; // no motion blur please
	nVidSDisplayStatus = 0; // no display status for old kaillera (custom overlay)
	bVidTripleBuffer = 0; // no triple buffer
	bVidScanlines = 0; // no ddraw scanlines
	bVidDWMSync = 0; // no dwm sync
	bHardwareGammaOnly = 0; // hardware gamma only
	bSaveInputs = 1; // save inputs for each game
	nSplashTime = 0; // no splash time please
	bAlwaysCreateSupportFolders = 1; // always create support folders
	EnableHiscores = 1; // save hiscores
	bEnableIcons = 0; // no driver icons (faster load)
	bCheatsAllowed = 0; // no cheats

#if defined (FBNEO_DEBUG)
	OpenDebugLog();
#endif

	FBALocaliseInit(szLocalisationTemplate);
	BurnerDoGameListLocalisation();

	if (bMonitorAutoCheck)
		MonitorAutoCheck();

	//#if 1 || !defined (FBNEO_DEBUG)
#if 0
	// print a warning if we're running for the 1st time
	if (nIniVersion < nBurnVer) {
		ScrnInit();
		//SplashDestroy(1);
		FirstUsageCreate();

		ConfigAppSave();								// Create initial config file
	}
#endif

	switch (nAppProcessPriority) {
	case HIGH_PRIORITY_CLASS:
	case ABOVE_NORMAL_PRIORITY_CLASS:
	case NORMAL_PRIORITY_CLASS:
	case BELOW_NORMAL_PRIORITY_CLASS:
	case IDLE_PRIORITY_CLASS:
		// nothing to change, we're good.
		break;
	default:
		// invalid priority class, set to normal.
		nAppProcessPriority = NORMAL_PRIORITY_CLASS;
		break;
	}

	// Set the process priority
	SetPriorityClass(GetCurrentProcess(), nAppProcessPriority);

#ifdef USE_SDL
	SDL_Init(0);
#endif

	ComputeGammaLUT();

	if (VidSelect(nVidSelect)) {
		nVidSelect = 4;
		VidSelect(nVidSelect);
	}

	hAccel = LoadAccelerators(hAppInst, MAKEINTRESOURCE(IDR_ACCELERATOR));

	// Build the ROM information
	CreateROMInfo(NULL);

	// Write a clrmame dat file if we are verifying roms
#if defined (ROM_VERIFY)
	create_datfile(_T("fbneo.dat"), 0);
#endif

	bNumlockStatus = SetNumLock(false);

	if (bEnableIcons && !bIconsLoaded) {
		// load driver icons
		LoadDrvIcons();
		bIconsLoaded = 1;
	}

	return 0;
}

static int AppExit()
{
	if (bIconsLoaded) {
		// unload driver icons
		UnloadDrvIcons();
		bIconsLoaded = 0;
	}

	SetNumLock(bNumlockStatus);

	DrvExit();						// Make sure any game driver is exitted
	FreeROMInfo();
	MediaExit(true);
	BurnLibExit();					// Exit the Burn library

	DisableHighResolutionTiming();

#ifdef USE_SDL
	SDL_Quit();
#endif

	FBALocaliseExit();
	BurnerExitGameListLocalisation();

	if (hAccel) {
		DestroyAcceleratorTable(hAccel);
		hAccel = NULL;
	}

	SplashDestroy(1);

	CloseHandle(hMainThread);

	CloseDebugLog();

	return 0;
}

void AppCleanup()
{
	StopReplay();
	WaveLogStop();

	AppExit();
}

int AppMessage(MSG* pMsg)
{
	if (IsDialogMessage(hInpdDlg, pMsg))	 return 0;
	if (IsDialogMessage(hInpCheatDlg, pMsg)) return 0;
	if (IsDialogMessage(hInpDIPSWDlg, pMsg)) return 0;
	if (IsDialogMessage(hDbgDlg, pMsg))		 return 0;
	if (IsDialogMessage(LuaConsoleHWnd, pMsg)) return 0;

	if (IsDialogMessage(hInpsDlg, pMsg))	 return 0;
	if (IsDialogMessage(hInpcDlg, pMsg))	 return 0;

	return 1; // Didn't process this message
}

bool AppProcessKeyboardInput()
{
	if (bEditActive) {
		return false;
	}

	return true;
}

int ProcessCmdLine()
{
	INT32 len = wcslen(szCmdLine);
	TCHAR* szEnd = szCmdLine + len;
	TCHAR* szCommand = szCmdLine;
	TCHAR szOption[MAX_PATH];

	while (szCommand < szEnd) {
		INT32 size = 0;
		if (szCommand[0] == _T('"')) {
			// quotes command
			size++;
			while (szCommand[size] != _T('"') && (szCommand + size) < szEnd) {
				size++;
			}
			wcsncpy(szOption, szCommand + 1, size - 2);
			szOption[size - 2] = 0;
			size++;
		}
		else {
			// regular command
			while (szCommand[size] != _T(' ') && (szCommand + size) < szEnd) {
				size++;
			}
			wcsncpy(szOption, szCommand, size);
			szOption[size] = 0;
		}
		// skip whites
		szCommand += size;
		while (*szCommand == _T(' ') && szCommand < szEnd) {
			szCommand++;
		}

		if (wcslen(szOption) > 0) {
			if (szOption[0] == _T('-')) {
				// Command: option
				if (_tcscmp(szOption, _T("-listinfo")) == 0) {
					write_datfile(DAT_ARCADE_ONLY, stdout);
					return 1;
				}
				else if (_tcscmp(szOption, _T("-listinfomdonly")) == 0) {
					write_datfile(DAT_MEGADRIVE_ONLY, stdout);
					return 1;
				}
				else if (_tcscmp(szOption, _T("-listinfopceonly")) == 0) {
					write_datfile(DAT_PCENGINE_ONLY, stdout);
					return 1;
				}
				else if (_tcscmp(szOption, _T("-listinfotg16only")) == 0) {
					write_datfile(DAT_TG16_ONLY, stdout);
					return 1;
				}
				else if (_tcscmp(szOption, _T("-listinfosgxonly")) == 0) {
					write_datfile(DAT_SGX_ONLY, stdout);
					return 1;
				}
				else if (_tcscmp(szOption, _T("-listinfosg1000only")) == 0) {
					write_datfile(DAT_SG1000_ONLY, stdout);
					return 1;
				}
				else if (_tcscmp(szOption, _T("-listinfocolecoonly")) == 0) {
					write_datfile(DAT_COLECO_ONLY, stdout);
					return 1;
				}
				else if (_tcscmp(szOption, _T("-listinfosmsonly")) == 0) {
					write_datfile(DAT_MASTERSYSTEM_ONLY, stdout);
					return 1;
				}
				else if (_tcscmp(szOption, _T("-listinfoggonly")) == 0) {
					write_datfile(DAT_GAMEGEAR_ONLY, stdout);
					return 1;
				}
				else if (_tcscmp(szOption, _T("-listinfomsxonly")) == 0) {
					write_datfile(DAT_MSX_ONLY, stdout);
					return 1;
				}
				else if (_tcscmp(szOption, _T("-listinfospectrumonly")) == 0) {
					write_datfile(DAT_SPECTRUM_ONLY, stdout);
					return 1;
				}
				else if (_tcscmp(szOption, _T("-listinfonesonly")) == 0) {
					write_datfile(DAT_NES_ONLY, stdout);
					return 1;
				}
				else if (_tcscmp(szOption, _T("-listinfofdsonly")) == 0) {
					write_datfile(DAT_FDS_ONLY, stdout);
					return 1;
				}
				else if (_tcscmp(szOption, _T("-listextrainfo")) == 0) {
					int nWidth;
					int nHeight;
					int nAspectX;
					int nAspectY;
					for (INT32 i = 0; i < nBurnDrvCount; i++) {
						nBurnDrvActive = i;
						BurnDrvGetVisibleSize(&nWidth, &nHeight);
						BurnDrvGetAspect(&nAspectX, &nAspectY);
						printf("%s\t%ix%i\t%i:%i\t0x%08X\t\"%s\"\t%i\t%i\t%x\t%x\t\"%s\"\n", BurnDrvGetTextA(DRV_NAME), nWidth, nHeight, nAspectX, nAspectY, BurnDrvGetHardwareCode(), BurnDrvGetTextA(DRV_SYSTEM), BurnDrvIsWorking(), BurnDrvGetMaxPlayers(), BurnDrvGetGenreFlags(), BurnDrvGetFamilyFlags(), BurnDrvGetTextA(DRV_COMMENT));
					}
					return 1;
				}
				else if (_tcscmp(szOption, _T("-a")) == 0) {
					bVidArcaderes = 1;
				}
				else if (_tcscmp(szOption, _T("-w")) == 0) {
					bVidAutoSwitchFullDisable = true;
				}
				else if (_tcscmp(szOption, _T("-q")) == 0) {
					QuarkRecordReplay();
				}
			}
			else {
				if (wcsstr(szOption, _T(".lua")) != 0) {
					// Command: lua file
					FBA_LoadLuaCode(TCHARToANSI(szOption, NULL, NULL));
					bVidAutoSwitchFullDisable = true;
				}
				else if (wcsstr(szOption, _T(".fs")) != 0) {
					// Command: savestate
					if (BurnStateLoad(szOption, 1, &DrvInitCallback)) {
						return 1;
					}
				}
				else if (wcsstr(szOption, _T(".fr")) != 0) {
					// Command: record file
					if (StartReplay(szOption)) {
						return 1;
					}
				}
				else if (wcsstr(szOption, _T("quark:")) != 0) {
					// Command: quark netplay
					QuarkInit(szOption);
				}
				else {
					// Command: load game
					bQuietLoading = true;
					INT32 i;
					for (i = 0; i < nBurnDrvCount; i++) {
						nBurnDrvActive = i;
						if ((_tcscmp(BurnDrvGetText(DRV_NAME), szOption) == 0) && (!(BurnDrvGetFlags() & BDF_BOARDROM))) {
							if (DrvInit(i, true)) { // failed (bad romset, etc.)
								nVidFullscreen = 0; // Don't get stuck in fullscreen mode
							}
							break;
						}
					}

					bQuietLoading = false;

					if (i == nBurnDrvCount) {
						FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_UI_NOSUPPORT), szOption, _T(APP_TITLE));
						FBAPopupDisplay(PUF_TYPE_ERROR);
						return 1;
					}
				}
			}
		}
	}

	POST_INITIALISE_MESSAGE;

	if (!nVidFullscreen) {
		MenuEnableItems();
	}

	return 0;
}

static void CreateSupportFolders()
{
	TCHAR szSupportDirs[][MAX_PATH] = {
		{_T("support/")},
		{_T("support/previews/")},
		{_T("support/titles/")},
		{_T("support/icons/")},
		{_T("support/cheats/")},
		{_T("support/hiscores/")},
		{_T("support/samples/")},
		{_T("support/hdd/")},
		{_T("support/ips/")},
		{_T("support/neocdz/")},
		{_T("support/blend/")},
		{_T("support/select/")},
		{_T("support/versus/")},
		{_T("support/howto/")},
		{_T("support/scores/")},
		{_T("support/bosses/")},
		{_T("support/gameover/")},
		{_T("support/flyers/")},
		{_T("support/marquees/")},
		{_T("support/cpanel/")},
		{_T("support/cabinets/")},
		{_T("support/pcbs/")},
		{_T("support/history/")},
		{_T("neocdiso/")},
		// rom directories
		{_T("roms/megadrive/")},
		{_T("roms/pce/")},
		{_T("roms/sgx/")},
		{_T("roms/tg16/")},
		{_T("roms/sg1000/")},
		{_T("roms/coleco/")},
		{_T("roms/sms/")},
		{_T("roms/gamegear/")},
		{_T("roms/msx/")},
		{_T("roms/spectrum/")},
		{_T("roms/nes/")},
		{_T("roms/nes_fds/")},
		{_T("roms/nes_hb/")},
		{_T("\0")} // END of list
	};

	for (int x = 0; szSupportDirs[x][0] != '\0'; x++) {
		CreateDirectory(szSupportDirs[x], NULL);
	}
}

// Main program entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nShowCmd)
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	DSCore_Init();
	DDCore_Init();

	// Provide a custom exception handler
	SetUnhandledExceptionFilter(ExceptionFilter);

	hAppInst = hInstance;

	// Make version string (@FC version at the end)
	if (nBurnVer & 0xFF) {
		// private version (alpha)
		_stprintf(szAppBurnVer, _T("%x.%x.%x.%02x-%d"), nBurnVer >> 20, (nBurnVer >> 16) & 0x0F, (nBurnVer >> 8) & 0xFF, nBurnVer & 0xFF, FS_VERSION);
	}
	else {
		// public version
		_stprintf(szAppBurnVer, _T("%x.%x.%x-%d"), nBurnVer >> 20, (nBurnVer >> 16) & 0x0F, (nBurnVer >> 8) & 0xFF, FS_VERSION);
	}

	nAppShowCmd = nShowCmd;

	AppDirectory();								// Set current directory to be the applications directory

	// Make sure there are roms and cfg subdirectories
	TCHAR szDirs[][MAX_PATH] = {
		{_T("config")},
		{_T("config/games")},
		{_T("config/ips")},
		{_T("config/localisation")},
		{_T("config/presets")},
		{_T("recordings")},
		{_T("roms")},
		{_T("savestates")},
		{_T("screenshots")},
		{_T("fightcade")},
#ifdef INCLUDE_AVI_RECORDING
		{_T("avi")},
#endif
		{_T("\0")} // END of list
	};

	for (int x = 0; szDirs[x][0] != '\0'; x++) {
		CreateDirectory(szDirs[x], NULL);
	}

	{                                           // Init Win* Common Controls lib
		INITCOMMONCONTROLSEX initCC = {
			sizeof(INITCOMMONCONTROLSEX),
			ICC_BAR_CLASSES | ICC_COOL_CLASSES | ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS | ICC_TREEVIEW_CLASSES,
		};
		InitCommonControlsEx(&initCC);
	}

	if (lpCmdLine) {
		_tcscpy(szCmdLine, ANSIToTCHAR(lpCmdLine, NULL, 0));
	}

	if (!(AppInit())) {							// Init the application
		if (bAlwaysCreateSupportFolders) CreateSupportFolders();
		if (!(ProcessCmdLine())) {
			DetectWindowsVersion();
			EnableHighResolutionTiming();

			MediaInit();

			RunMessageLoop();					// Run the application message loop
		}
	}

	NeoCDZRateChangeback();                     // Change back temp CDZ rate before saving

	ConfigAppSave();							// Save config for the application

	AppExit();									// Exit the application

	return 0;
}
