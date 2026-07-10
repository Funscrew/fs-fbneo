#include "burner.h"

static void MakeOfn()
{
	memset(&openFileName, 0, sizeof(openFileName));
	openFileName.lStructSize = sizeof(openFileName);
	openFileName.hwndOwner = hScrnWnd;
	openFileName.lpstrFilter = _T("FB Alpha skin files (*.bmp,*.png)\0*.bmp;*.png\0\0)");
	openFileName.lpstrFile = szChoice;
	openFileName.nMaxFile = sizeof(szChoice) / sizeof(TCHAR);
	openFileName.lpstrInitialDir = _T("");
	openFileName.Flags = OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
	openFileName.lpstrDefExt = _T("png");
	return;
}

int SelectPlaceHolder()
{
	int nRet;
	int bOldPause;

	MakeOfn();
	openFileName.lpstrTitle = FBALoadStringEx(hAppInst, IDS_PLACEHOLDER_LOAD, true);

	bOldPause = bRunPause;
	bRunPause = 1;
	nRet = GetOpenFileName(&openFileName);
	bRunPause = bOldPause;

	if (nRet == 0) {		// Error
		return 1;
	}

	szPlaceHolder[0] = _T('\0');
	memcpy(szPlaceHolder, szChoice, sizeof(szChoice));

	return nRet;
}

void ResetPlaceHolder()
{
	szPlaceHolder[0] = _T('\0');
}
