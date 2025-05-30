// DirectX9 Enhanced video output
#include "burner.h"
#include "vid_softfx.h"
#include "vid_overlay.h"
#include "vid_effect.h"

//#ifdef _MSC_VER
#pragma comment(lib, "d3d9")
#pragma comment(lib, "d3dx9")

//#define PRINT_DEBUG_INFO
//#define ENABLE_PROFILING FBNEO_DEBUG
//#define LOAD_EFFECT_FROM_FILE

#include <InitGuid.h>
#define DIRECT3D_VERSION 0x0900							// Use this Direct3D version
#define D3D_OVERLOADS
#include <d3d9.h>
#include <d3dx9effect.h>

const float PI = 3.14159265358979323846f;

typedef struct _D3DLVERTEX2 {
	union { FLOAT x;  FLOAT dvX; };
	union { FLOAT y;  FLOAT dvY; };
	union { FLOAT z;  FLOAT dvZ; };
	union { D3DCOLOR color; D3DCOLOR dcColor; };
	union { D3DCOLOR specular; D3DCOLOR dcSpecular; };
	union { FLOAT tu;  FLOAT dvTU; };
	union { FLOAT tv;  FLOAT dvTV; };
	union { FLOAT tu1; FLOAT dvTU1; };
	union { FLOAT tv1; FLOAT dvTV1; };
#if(DIRECT3D_VERSION >= 0x0500)
#if (defined __cplusplus) && (defined D3D_OVERLOADS)
	_D3DLVERTEX2() { }
	_D3DLVERTEX2(FLOAT _x, FLOAT _y, FLOAT _z, D3DCOLOR _color, D3DCOLOR _specular, FLOAT _tu, FLOAT _tv, FLOAT _tu1, FLOAT _tv1)
	{
		x = _x; y = _y; z = _z;
		color = _color; specular = _specular;
		tu = _tu; tv = _tv;
		tu1 = _tu1; tv1 = _tv1;
	}
#endif
#endif
} D3DLVERTEX2, *LPD3DLVERTEX2;

#define D3DFVF_LVERTEX2 ( D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX2 | D3DFVF_TEXCOORDSIZE2(0) | D3DFVF_TEXCOORDSIZE2(1) | D3DFVF_TEXCOORDSIZE2(2) | D3DFVF_TEXCOORDSIZE2(3) )

#define RENDER_STRIPS (4)

#define DX9_SHADERPRECISION  ((nVidBlitterOpt[nVidSelect] & (7 << 28)) >> 28)
#define DX9_FILTER           ((nVidBlitterOpt[nVidSelect] & (3 << 24)) >> 24)
#define DX9_USE_PS20		 ( nVidBlitterOpt[nVidSelect] & (1 <<  9))
#define DX9_USE_FPTEXTURE    ( nVidBlitterOpt[nVidSelect] & (1 <<  8))

static IDirect3D9Ex* pD3D = NULL;							// Direct3D interface
static D3DPRESENT_PARAMETERS d3dpp;
static IDirect3DDevice9Ex* pD3DDevice = NULL;
static IDirect3DVertexBuffer9* pVB[RENDER_STRIPS] = { NULL, };
static IDirect3DTexture9* pTexture = NULL;
static IDirect3DTexture9* pScanlineTexture[2] = { NULL, };
static int nTextureWidth = 0, nTextureHeight = 0;
static IDirect3DSurface9* pSurface = NULL;

static IDirect3DVertexBuffer9* pIntermediateVB = NULL;
static IDirect3DTexture9* pIntermediateTexture = NULL;
static int nIntermediateTextureWidth, nIntermediateTextureHeight;

static ID3DXEffect* pEffect = NULL;
static ID3DXTextureShader* pEffectShader = NULL;
static D3DXHANDLE hTechnique = NULL;
//static D3DXHANDLE hScanIntensity = NULL;
static IDirect3DTexture9* pEffectTexture = NULL;
static VidEffect* pVidEffect = NULL;
static int nDX9HardFX = -1;

static double dPrevCubicB, dPrevCubicC;

static bool bUsePS14;

static int nGameWidth = 0, nGameHeight = 0;				// Game screen size
static int nGameImageWidth, nGameImageHeight;

static int nRotateGame;
static int nImageWidth, nImageHeight/*, nImageZoom*/;

static RECT Dest;

static unsigned int nD3DAdapter;

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

bool LoadD3DTextureFromFile(IDirect3DDevice9* device, const char* filename, IDirect3DTexture9*& texture, int& width, int& height)
{
	bool res = false;
	int comp;

	if (unsigned char* image = stbi_load(filename, &width, &height, &comp, 4))
	{
		device->CreateTexture(width, height, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &texture, 0);
		if (texture)
		{
			D3DLOCKED_RECT rect;
			texture->LockRect(0, &rect, 0, D3DLOCK_DISCARD);
			unsigned char* dst = static_cast<unsigned char*>(rect.pBits);
			unsigned char* src = image;
			for (int i = 0; i < height; i++)
			{
				for (int j = 0; j < width; j++)
				{
					*dst++ = src[2];
					*dst++ = src[1];
					*dst++ = src[0];
					*dst++ = src[3];
					src+= 4;
				}
				dst+= (rect.Pitch)-width*4;
			}
			texture->UnlockRect(0);
			res = true;
		}
		stbi_image_free(image);
	}
	return res;
}

// ----------------------------------------------------------------------------
#ifdef PRINT_DEBUG_INFO
static TCHAR* TextureFormatString(D3DFORMAT nFormat)
{
	switch (nFormat) {
		case D3DFMT_X1R5G5B5:
			return _T("16-bit xRGB 1555");
		case D3DFMT_R5G6B5:
			return _T("16-bit RGB 565");
		case D3DFMT_X8R8G8B8:
			return _T("32-bit xRGB 8888");
		case D3DFMT_A8R8G8B8:
			return _T("32-bit ARGB 8888");
		case D3DFMT_A16B16G16R16F:
			return _T("64-bit ARGB 16161616fp");
		case D3DFMT_A32B32G32R32F:
			return _T("128-bit ARGB 32323232fp");
		default:
			return _T("unknown format");
	}

	return _T("unknown format");
}
#endif
// ----------------------------------------------------------------------------

static int GetTextureSize(int nSize)
{
	int nTextureSize = 128;

	while (nTextureSize < nSize) {
		nTextureSize <<= 1;
	}

	return nTextureSize;
}

/*
static void PutPixel(unsigned char** ppSurface, unsigned int nColour)
{
	switch (nVidScrnDepth) {
		case 15:
			*((unsigned short*)(*ppSurface)) = ((nColour >> 9) & 0x7C00) | ((nColour >> 6) & 0x03E0) | ((nColour >> 3) & 0x001F);
			*ppSurface += 2;
			break;
		case 16:
			*((unsigned short*)(*ppSurface)) = ((nColour >> 8) & 0xF800) | ((nColour >> 5) & 0x07E0) | ((nColour >> 3) & 0x001F);
			*ppSurface += 2;
			break;
		case 24:
			(*ppSurface)[0] = (nColour >> 16) & 0xFF;
			(*ppSurface)[1] = (nColour >>  8) & 0xFF;
			(*ppSurface)[2] = (nColour >>  0) & 0xFF;
			*ppSurface += 3;
			break;
		case 32:
			*((unsigned int*)(*ppSurface)) = nColour;
			*ppSurface += 4;
			break;
	}
}
*/

static UINT dx9GetAdapter(wchar_t* name)
{
	for (int a = pD3D->GetAdapterCount() - 1; a >= 0; a--)
	{
		D3DADAPTER_IDENTIFIER9 identifier;

		pD3D->GetAdapterIdentifier(a, 0, &identifier);
		if (identifier.DeviceName[11] == name[11]) {
			return a;
		}
	}

	return 0;
}

// Select optimal full-screen resolution
int dx9SelectFullscreenMode(VidSDisplayScoreInfo* pScoreInfo)
{
	//int nWidth, nHeight;

	if (bVidArcaderes) {
		if (!VidSGetArcaderes((int*)&(pScoreInfo->nBestWidth), (int*)&(pScoreInfo->nBestHeight))) {
			return 1;
		}
	} else {
		if (nScreenSize) {
			D3DFORMAT nFormat = (nVidDepth == 16) ? D3DFMT_R5G6B5 : D3DFMT_X8R8G8B8;
			D3DDISPLAYMODE dm;

			memset(pScoreInfo, 0, sizeof(VidSDisplayScoreInfo));
			pScoreInfo->nRequestedZoom = nScreenSize;
			VidSInitScoreInfo(pScoreInfo);

			// Enumerate the available screenmodes
			for (int i = pD3D->GetAdapterModeCount(nD3DAdapter, nFormat) - 1; i >= 0; i--) {
				if (FAILED(pD3D->EnumAdapterModes(nD3DAdapter, nFormat, i, &dm))) {
					return 1;
				}
				pScoreInfo->nModeWidth = dm.Width;
				pScoreInfo->nModeHeight = dm.Height;

				VidSScoreDisplayMode(pScoreInfo);
			}

			if (pScoreInfo->nBestWidth == -1U) {
				FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_UI_FULL_NOMODE));
				FBAPopupDisplay(PUF_TYPE_ERROR);
				return 1;
			}
		} else {
			pScoreInfo->nBestWidth = nVidWidth;
			pScoreInfo->nBestHeight = nVidHeight;
		}
	}

	if (!bDrvOkay && (pScoreInfo->nBestWidth < 640 || pScoreInfo->nBestHeight < 480)) {
		return 1;
	}

	return 0;
}

// ----------------------------------------------------------------------------

static void FillEffectTexture()
{
	FLOAT B = (FLOAT)dVidCubicB;
	FLOAT C = (FLOAT)dVidCubicC;

	if (pEffect == NULL) {
		return;
	}

/*
	if (dVidCubicSharpness > 0.5) {
		C = dVidCubicSharpness;
		B = 0.0;
	} else {
		C = dVidCubicSharpness;
		B = 1.0 - 2.0 * C;
	}
*/

	if (DX9_SHADERPRECISION == 4 || bUsePS14) {
		B = 0.0;
	}

	if (pEffectShader && pEffectTexture) {
		pEffectShader->SetFloat("B", B);
		pEffectShader->SetFloat("C", C);

		D3DXFillTextureTX(pEffectTexture, pEffectShader);
	}

	pEffect->SetFloat("B", B);
	pEffect->SetFloat("C", C);
}

// ----------------------------------------------------------------------------

static void dx9ReleaseResources()
{
	RELEASE(pEffect);
	RELEASE(pEffectShader);
	RELEASE(pEffectTexture);

	RELEASE(pScanlineTexture[0]);
	RELEASE(pScanlineTexture[1]);
	RELEASE(pIntermediateTexture);

	RELEASE(pSurface);
	RELEASE(pTexture);

	for (int y = 0; y < RENDER_STRIPS; y++) {
		RELEASE(pVB[y]);
	}
	RELEASE(pIntermediateVB);
}

static int dx9Exit()
{
	dx9ReleaseResources();

	VidOverlayEnd();
	VidSFreeVidImage();

	RELEASE(pD3DDevice);
	RELEASE(pD3D);

	nRotateGame = 0;

	return 0;
}

static int dx9SurfaceInit()
{
	D3DFORMAT nFormat = D3DFMT_UNKNOWN;

	if (nRotateGame & 1) {
		nVidImageWidth = nGameHeight;
		nVidImageHeight = nGameWidth;
	} else {
		nVidImageWidth = nGameWidth;
		nVidImageHeight = nGameHeight;
	}

	nGameImageWidth = nVidImageWidth;
	nGameImageHeight = nVidImageHeight;

	if (bDrvOkay && (BurnDrvGetFlags() & BDF_16BIT_ONLY)) {
		nVidImageDepth = 15;
	} else {
		nVidImageDepth = nVidScrnDepth;
	}

	// wtf
	nVidImageDepth = nVidScrnDepth;

	switch (nVidImageDepth) {
		case 32:
			nFormat = D3DFMT_X8R8G8B8;
			break;
		case 24:
			nFormat = D3DFMT_R8G8B8;
			break;
		case 16:
			nFormat = D3DFMT_R5G6B5;
			break;
		case 15:
			nFormat = D3DFMT_X1R5G5B5;
			break;
	}

	nVidImageBPP = (nVidImageDepth + 7) >> 3;
	nBurnBpp = nVidImageBPP;					// Set Burn library Bytes per pixel

	// Use our callback to get colors:
	SetBurnHighCol(nVidImageDepth);

	// Make the normal memory buffer
	if (VidSAllocVidImage()) {
		dx9Exit();
		return 1;
	}

	if (FAILED(pD3DDevice->CreateOffscreenPlainSurface(nVidImageWidth, nVidImageHeight, nFormat, D3DPOOL_DEFAULT, &pSurface, NULL))) {
#ifdef PRINT_DEBUG_INFO
		debugPrintf(_T("  * Error: Couldn't create surface.\n"));
#endif
		return 1;
	}
#ifdef PRINT_DEBUG_INFO
	debugPrintf(_T("  * Allocated a %i x %i (%s) surface.\n"), nVidImageWidth, nVidImageHeight, TextureFormatString(nFormat));
#endif

	nTextureWidth = GetTextureSize(nGameImageWidth);
	nTextureHeight = GetTextureSize(nGameImageHeight);

	if (FAILED(pD3DDevice->CreateTexture(nTextureWidth, nTextureHeight, 1, D3DUSAGE_RENDERTARGET, nFormat, D3DPOOL_DEFAULT, &pTexture, NULL))) {
#ifdef PRINT_DEBUG_INFO
		debugPrintf(_T("  * Error: Couldn't create texture.\n"));
#endif
		return 1;
	}
#ifdef PRINT_DEBUG_INFO
	debugPrintf(_T("  * Allocated a %i x %i (%s) image texture.\n"), nTextureWidth, nTextureHeight, TextureFormatString(nFormat));
#endif

	return 0;
}

static int dx9EffectSurfaceInit()
{
	D3DFORMAT nFormat;

	if (DX9_FILTER == 2 && (bUsePS14 || (DX9_SHADERPRECISION != 0 && DX9_SHADERPRECISION != 2))) {
		switch (DX9_SHADERPRECISION) {
			case 0:
				nFormat = D3DFMT_A32B32G32R32F;
				break;
			case 1:
				nFormat = D3DFMT_A16B16G16R16F;
				break;
			case 2:
				nFormat = D3DFMT_A32B32G32R32F;
				break;
			case 3:
				nFormat = D3DFMT_A16B16G16R16F;
				break;
			default:
				nFormat = D3DFMT_A8R8G8B8;
		}

		if (bUsePS14) {
			nFormat = D3DFMT_A8R8G8B8;
		}

		if (FAILED(pD3DDevice->CreateTexture(1024, 1, 1, D3DUSAGE_RENDERTARGET, nFormat, D3DPOOL_DEFAULT, &pEffectTexture, NULL))) {
#ifdef PRINT_DEBUG_INFO
			debugPrintf(_T("  * Error: Couldn't create effects texture.\n"));
#endif
			return 1;
		}
#ifdef PRINT_DEBUG_INFO
		debugPrintf(_T("  * Allocated a %i x %i (%s) LUT texture.\n"), 1024, 1, TextureFormatString(nFormat));
#endif
	}

	if ((DX9_FILTER == 2 && (bUsePS14 || DX9_SHADERPRECISION >= 2))) {
		if (nVidFullscreen) {
			nIntermediateTextureWidth = GetTextureSize(nRotateGame ? nVidScrnHeight : nVidScrnWidth);
		}
		else {
			nIntermediateTextureWidth = GetTextureSize(nRotateGame ? SystemWorkArea.bottom - SystemWorkArea.top : SystemWorkArea.right - SystemWorkArea.left);
		}
		nIntermediateTextureHeight = nTextureHeight;

		nFormat = D3DFMT_A8R8G8B8;
		if ((DX9_FILTER == 2) && !bUsePS14 && DX9_USE_FPTEXTURE) {
#if 0
			if ((DX9_SHADERPRECISION == 0 || DX9_SHADERPRECISION == 2)) {
				nFormat = D3DFMT_A32B32G32R32F;
			}
			else {
				nFormat = D3DFMT_A16B16G16R16F;
			}
#else
			nFormat = D3DFMT_A16B16G16R16F;
#endif
		}

		if (FAILED(pD3DDevice->CreateTexture(nIntermediateTextureWidth, nIntermediateTextureHeight, 1, D3DUSAGE_RENDERTARGET, nFormat, D3DPOOL_DEFAULT, &pIntermediateTexture, NULL))) {
#ifdef PRINT_DEBUG_INFO
			debugPrintf(_T("  * Error: Couldn't create intermediate texture.\n"));
#endif
			return 1;
		}
#ifdef PRINT_DEBUG_INFO
		debugPrintf(_T("  * Allocated a %i x %i (%s) intermediate texture.\n"), nIntermediateTextureWidth, nIntermediateTextureHeight, TextureFormatString(nFormat));
#endif
	}

	{
		// Clear textures and set border colour

		IDirect3DSurface9* pSurf;

		pTexture->GetSurfaceLevel(0, &pSurf);
		pD3DDevice->ColorFill(pSurf, 0, D3DCOLOR_XRGB(0, 0, 0));
		RELEASE(pSurf);

		if (pIntermediateTexture) {
			pIntermediateTexture->GetSurfaceLevel(0, &pSurf);
			pD3DDevice->ColorFill(pSurf, 0, D3DCOLOR_XRGB(0, 0, 0));
			RELEASE(pSurf);
		}

		pD3DDevice->SetSamplerState(0, D3DSAMP_BORDERCOLOR, D3DCOLOR_XRGB(0, 0, 0));
		pD3DDevice->SetSamplerState(1, D3DSAMP_BORDERCOLOR, D3DCOLOR_XRGB(0, 0, 0));
	}

	// Allocate scanline textures
	for (int i = 0, nSize = 2; i < 2; i++, nSize <<= 1) {
		RECT rect = { 0, 0, nSize, nSize };
		IDirect3DSurface9* pSurf = NULL;

		unsigned int scan2x2[] = { 0xFFFFFF, 0xFFFFFF,
									0x000000, 0x000000 };

		unsigned int scan4x4[] = { 0x9F9F9F, 0x9F9F9F, 0x9F9F9F, 0x9F9F9F,
									0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF,
									0x9F9F9F, 0x9F9F9F, 0x9F9F9F, 0x9F9F9F,
									0x000000, 0x000000, 0x000000, 0x000000 };

		unsigned int* scandata[] = { scan2x2, scan4x4 };

		if (FAILED(pD3DDevice->CreateTexture(nSize, nSize, 1, D3DUSAGE_DYNAMIC, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &(pScanlineTexture[i]), NULL))) {
#ifdef PRINT_DEBUG_INFO
			debugPrintf(_T("  * Error: Couldn't create texture.\n"));
#endif
			return 1;
		}
		if (FAILED(pScanlineTexture[i]->GetSurfaceLevel(0, &pSurf))) {
#ifdef PRINT_DEBUG_INFO
			debugPrintf(_T("  * Error: Couldn't get texture surface.\n"));
#endif
		}

		if (FAILED(D3DXLoadSurfaceFromMemory(pSurf, NULL, &rect, scandata[i], D3DFMT_X8R8G8B8, nSize * sizeof(int), NULL, &rect, D3DX_FILTER_NONE, 0))) {
#ifdef PRINT_DEBUG_INFO
			debugPrintf(_T("  * Error: D3DXLoadSurfaceFromMemory failed.\n"));
#endif
		}

		RELEASE(pSurf);
	}

	return 0;
}

int dx9GeometryInit()
{
	for (int y = 0; y < RENDER_STRIPS; y++) {

		D3DLVERTEX2 vScreen[4];

		// The polygons for the emulator image on the screen
		{
			int nWidth = pIntermediateTexture ? nImageWidth : nGameImageWidth;
			int nHeight = nGameImageHeight;
			int nTexWidth = pIntermediateTexture ? nIntermediateTextureWidth : nTextureWidth;
			int nTexHeight = nTextureHeight;

			// Add 0.0000001 to ensure consistent rounding
			double dTexCoordXl = 0.0000001;
			double dTexCoordXr = (double)nWidth / (double)nTexWidth + 0.0000001;
			double dTexCoordYt = (double)nHeight * (double)(y + 0) / (double)(nTexHeight * RENDER_STRIPS) + 0.0000001;
			double dTexCoordYb = (double)nHeight * (double)(y + 1) / (double)(nTexHeight * RENDER_STRIPS) + 0.0000001;

			double dLeft = 1.0 - 2.0;
			double dRight = 1.0 - 0.0;
			double dTop = 1.0 - (((double)((nRotateGame & 1) ? (RENDER_STRIPS - y - 0) : (y + 0))) * 2.0 / RENDER_STRIPS);
			double dBottom = 1.0 - (((double)((nRotateGame & 1) ? (RENDER_STRIPS - y - 1) : (y + 1))) * 2.0 / RENDER_STRIPS);

			// ugly fix for flipped game, modified by regret
			if ((nRotateGame & 1) && (nRotateGame & 2)) {
				dTop = 1.0 - (((double)(y + 0)) * 2.0 / RENDER_STRIPS);
				dBottom = 1.0 - (((double)(y + 1)) * 2.0 / RENDER_STRIPS);
			}
			else if (nRotateGame & 2) {
				dTop = 1.0 - (((double)(RENDER_STRIPS - y - 0)) * 2.0 / RENDER_STRIPS);
				dBottom = 1.0 - (((double)(RENDER_STRIPS - y - 1)) * 2.0 / RENDER_STRIPS);
			}

			if (pIntermediateTexture) {
				dTexCoordXl += 0.5 / (double)nIntermediateTextureWidth;
				dTexCoordXr += 0.5 / (double)nIntermediateTextureWidth;
				dTexCoordYt += 1.0 / (double)nIntermediateTextureHeight;
				dTexCoordYb += 1.0 / (double)nIntermediateTextureHeight;
			}

#if 0
			if (nPreScaleEffect) {
				if (nPreScale & 1) {
					nWidth *= nPreScaleZoom;
					nTexWidth = nPreScaleTextureWidth;
				}
				if (nPreScale & 2) {
					nHeight *= nPreScaleZoom;
					nTexHeight = nPreScaleTextureHeight;
				}
			}
#endif

			// Set up the vertices for the game image, including the texture coordinates for the game image only
			// ugly fix for flipped game, modified by regret
			if (nRotateGame & 1) {
				vScreen[(nRotateGame & 2) ? 3 : 0] = D3DLVERTEX2(dTop, dLeft, 0.0, 0xFFFFFFFF, 0, (nRotateGame & 2) ? dTexCoordXr : dTexCoordXl, dTexCoordYt, 0, 0);
				vScreen[(nRotateGame & 2) ? 2 : 1] = D3DLVERTEX2(dTop, dRight, 0.0, 0xFFFFFFFF, 0, (nRotateGame & 2) ? dTexCoordXl : dTexCoordXr, dTexCoordYt, 0, 0);
				vScreen[(nRotateGame & 2) ? 1 : 2] = D3DLVERTEX2(dBottom, dLeft, 0.0, 0xFFFFFFFF, 0, (nRotateGame & 2) ? dTexCoordXr : dTexCoordXl, dTexCoordYb, 0, 0);
				vScreen[(nRotateGame & 2) ? 0 : 3] = D3DLVERTEX2(dBottom, dRight, 0.0, 0xFFFFFFFF, 0, (nRotateGame & 2) ? dTexCoordXl : dTexCoordXr, dTexCoordYb, 0, 0);
			}
			else {
				vScreen[(nRotateGame & 2) ? 3 : 0] = D3DLVERTEX2(dLeft, dTop, 0.0, 0xFFFFFFFF, 0, (nRotateGame & 2) ? dTexCoordXr : dTexCoordXl, dTexCoordYt, 0, 0);
				vScreen[(nRotateGame & 2) ? 2 : 1] = D3DLVERTEX2(dRight, dTop, 0.0, 0xFFFFFFFF, 0, (nRotateGame & 2) ? dTexCoordXl : dTexCoordXr, dTexCoordYt, 0, 0);
				vScreen[(nRotateGame & 2) ? 1 : 2] = D3DLVERTEX2(dLeft, dBottom, 0.0, 0xFFFFFFFF, 0, (nRotateGame & 2) ? dTexCoordXr : dTexCoordXl, dTexCoordYb, 0, 0);
				vScreen[(nRotateGame & 2) ? 0 : 3] = D3DLVERTEX2(dRight, dBottom, 0.0, 0xFFFFFFFF, 0, (nRotateGame & 2) ? dTexCoordXl : dTexCoordXr, dTexCoordYb, 0, 0);
			}

			{
				// Set up the 2nd pair of texture coordinates, used for scanlines or the high performance version of the cubic filter
				double dScanOffset = nGameImageHeight / -2.0;

				dScanOffset += 0.5;

				// Set the texture coordinates for the scanlines
				if (nRotateGame & 1) {
					vScreen[nRotateGame & 2 ? 3 : 0].tu1 = dScanOffset + 0.0;					vScreen[nRotateGame & 2 ? 3 : 0].tv1 = dScanOffset + 0.0 + 0.0000001;
					vScreen[nRotateGame & 2 ? 2 : 1].tu1 = dScanOffset + (double)nGameHeight;	vScreen[nRotateGame & 2 ? 2 : 1].tv1 = dScanOffset + 0.0 + 0.0000001;
					vScreen[nRotateGame & 2 ? 1 : 2].tu1 = dScanOffset + 0.0;					vScreen[nRotateGame & 2 ? 1 : 2].tv1 = dScanOffset + (double)nGameWidth / RENDER_STRIPS + 0.0000001;
					vScreen[nRotateGame & 2 ? 0 : 3].tu1 = dScanOffset + (double)nGameHeight;	vScreen[nRotateGame & 2 ? 0 : 3].tv1 = dScanOffset + (double)nGameWidth / RENDER_STRIPS + 0.0000001;
				}
				else {
					vScreen[nRotateGame & 2 ? 3 : 0].tu1 = dScanOffset + 0.0;					vScreen[nRotateGame & 2 ? 3 : 0].tv1 = dScanOffset + 0.0 + 0.0000001;
					vScreen[nRotateGame & 2 ? 2 : 1].tu1 = dScanOffset + (double)nGameWidth;	vScreen[nRotateGame & 2 ? 2 : 1].tv1 = dScanOffset + 0.0 + 0.0000001;
					vScreen[nRotateGame & 2 ? 1 : 2].tu1 = dScanOffset + 0.0;					vScreen[nRotateGame & 2 ? 1 : 2].tv1 = dScanOffset + (double)nGameHeight / RENDER_STRIPS + 0.0000001;
					vScreen[nRotateGame & 2 ? 0 : 3].tu1 = dScanOffset + (double)nGameWidth;	vScreen[nRotateGame & 2 ? 0 : 3].tv1 = dScanOffset + (double)nGameHeight / RENDER_STRIPS + 0.0000001;
				}
			}
		}

		{
			D3DLVERTEX2* pVertexData;

			if (FAILED(pVB[y]->Lock(0, 4 * sizeof(D3DLVERTEX2), (void**)&pVertexData, 0))) {
#ifdef PRINT_DEBUG_INFO
				debugPrintf(_T("  * Error: Couldn't create vertex buffer.\n"));
#endif
				return 1;
			}
			memcpy(pVertexData, &vScreen, 4 * sizeof(D3DLVERTEX2));
			pVB[y]->Unlock();
		}
	}

	if (pIntermediateTexture) {
		D3DLVERTEX2* pVertexData;
		D3DLVERTEX2 vTemp[4];

		double dTexCoordXl = 0.0;
		double dTexCoordXr = (double)nGameImageWidth / (double)nTextureWidth;
		double dTexCoordYt = 0.0 + 0.0000001;
		double dTexCoordYb = 1.0 + 0.0000001;

		if (DX9_FILTER == 1) {
			dTexCoordYt -= 0.5 / nTextureHeight;
			dTexCoordYb -= 0.5 / nTextureHeight;
		}

		vTemp[0] = D3DLVERTEX2(-1.0, 1.0, 0.0, 0xFFFFFFFF, 0, dTexCoordXl, dTexCoordYt, 0.5, 0.5);
		vTemp[1] = D3DLVERTEX2(1.0, 1.0, 0.0, 0xFFFFFFFF, 0, dTexCoordXr, dTexCoordYt, 0.5 + nGameImageWidth, 0.5);
		vTemp[2] = D3DLVERTEX2(-1.0, -1.0, 0.0, 0xFFFFFFFF, 0, dTexCoordXl, dTexCoordYb, 0.5, 0.5 + nGameImageHeight);
		vTemp[3] = D3DLVERTEX2(1.0, -1.0, 0.0, 0xFFFFFFFF, 0, dTexCoordXr, dTexCoordYb, 0.5 + nGameImageWidth, 0.5 + nGameImageHeight);

		if (FAILED(pIntermediateVB->Lock(0, 4 * sizeof(D3DLVERTEX2), (void**)&pVertexData, 0))) {
#ifdef PRINT_DEBUG_INFO
			debugPrintf(_T("  * Error: Couldn't create vertex buffer.\n"));
#endif
			return 1;
		}

		memcpy(pVertexData, &vTemp, 4 * sizeof(D3DLVERTEX2));
		pIntermediateVB->Unlock();
	}

	return 0;
}

int dx9EffectInit()
{
	//	Set up the bicubic filter

	//	char szB[MAX_PATH] = "(1.0)";
	//	char szC[MAX_PATH] = "(0.0)";
	//	D3DXMACRO d3dxm[] = {{ "USE_BC_MACRO", "1", }, { "B", szB, }, { "C", szC, }, { NULL, NULL, }};

	char* pszTechniqueOpt[] = { "ScanBilinear", "Bilinear", "ScanPoint", "Point" };
	char* pszTechnique = NULL;

	ID3DXBuffer* pErrorBuffer = NULL;
	FLOAT pFloatArray[2];

	pEffect = NULL;
	pEffectShader = NULL;

	D3DXCreateBuffer(0x10000, &pErrorBuffer);

#ifdef LOAD_EFFECT_FROM_FILE
	if (FAILED(D3DXCreateEffectFromFile(pD3DDevice, _T("bicubic.fx"), NULL /* d3dxm */, NULL, D3DXSHADER_ENABLE_BACKWARDS_COMPATIBILITY, NULL, &pEffect, &pErrorBuffer))) {
#else
	if (FAILED(D3DXCreateEffectFromResource(pD3DDevice, NULL, MAKEINTRESOURCE(ID_DX9EFFECT), NULL /* d3dxm */, NULL, D3DXSHADER_ENABLE_BACKWARDS_COMPATIBILITY, NULL, &pEffect, &pErrorBuffer))) {
#endif
#ifdef PRINT_DEBUG_INFO
		debugPrintf(_T("  * Error: Couldn't compile effect.\n"));
		debugPrintf(_T("\n%hs\n\n"), pErrorBuffer->GetBufferPointer());
#endif
		goto HANDLE_ERROR;
	}

	// Check if we need to use PS1.4 (instead of 2.0)
	bUsePS14 = !DX9_USE_PS20;
	if (pEffect->GetTechniqueByName("SinglePassHQBicubic") == NULL) {
		bUsePS14 = true;
	}

	if (dx9EffectSurfaceInit()) {
		goto HANDLE_ERROR;
	}

	switch (DX9_FILTER) {
	case 1: {
		pszTechnique = pszTechniqueOpt[0];
		break;
	}
	case 2: {
		char* pszFunction[8] = { "SinglePassHQBicubic", "SinglePassBicubic", "MultiPassHQBicubic", "MultiPassBicubic", "MultiPassHP20Bicubic", "MultiPassHP14Bicubic", "MultiPassHP14Bicubic", "MultiPassHP14Bicubic" };
		pszTechnique = pszFunction[bUsePS14 ? 7 : DX9_SHADERPRECISION];
		break;
	}
	default: {
		pszTechnique = pszTechniqueOpt[3];
		break;
	}
	}

	pEffect->SetTexture("imageTexture", pTexture);
	pEffect->SetTexture("intermediateTexture", pIntermediateTexture);
	pEffect->SetTexture("scanTexture", pScanlineTexture[(nImageHeight / nGameImageHeight >= 4) ? 1 : 0]);

	pFloatArray[0] = nTextureWidth; pFloatArray[1] = nTextureHeight;
	pEffect->SetFloatArray("imageSize", pFloatArray, 2);
	pFloatArray[0] = 1.0 / nTextureWidth; pFloatArray[1] = 1.0 / nTextureHeight;
	pEffect->SetFloatArray("texelSize", pFloatArray, 2);

	/*
		{
			FLOAT fScanIntensity[4] = { (float)((nVidScanIntensity >> 16) & 255) / 255.0,
										(float)((nVidScanIntensity >>  8) & 255) / 255.0,
										(float)((nVidScanIntensity >>  0) & 255) / 255.0,
										(float)((nVidScanIntensity >> 24) & 255) / 255.0 };

			pEffect->SetFloatArray("scanIntensity", fScanIntensity, 4);
		}
	*/
	hTechnique = pEffect->GetTechniqueByName(pszTechnique);
	if (FAILED(pEffect->SetTechnique(hTechnique))) {
#ifdef PRINT_DEBUG_INFO
		debugPrintf(_T("  * Error: Couldn't set technique.\n"));
#endif
		goto HANDLE_ERROR;
	}

	pEffect->SetTexture("weightTex", pEffectTexture);

	{
		ID3DXBuffer* pEffectShaderBuffer = NULL;
		D3DXCreateBuffer(0x10000, &pEffectShaderBuffer);

#ifdef LOAD_EFFECT_FROM_FILE
		if (FAILED(D3DXCompileShaderFromFile(_T("bicubic.fx"), NULL, NULL, (DX9_SHADERPRECISION < 4 && !bUsePS14) ? "genWeightTex20" : "genWeightTex14", "tx_1_0", 0, &pEffectShaderBuffer, &pErrorBuffer, NULL))) {
#else
		if (FAILED(D3DXCompileShaderFromResource(NULL, MAKEINTRESOURCE(ID_DX9EFFECT), NULL, NULL, (DX9_SHADERPRECISION < 4 && !bUsePS14) ? "genWeightTex20" : "genWeightTex14", "tx_1_0", 0, &pEffectShaderBuffer, &pErrorBuffer, NULL))) {
#endif
#ifdef PRINT_DEBUG_INFO
			debugPrintf(_T("  * Error: Couldn't compile shader.\n"));
			debugPrintf(_T("\n%hs\n\n"), pErrorBuffer->GetBufferPointer());
#endif
			goto HANDLE_ERROR;
		}

		if (FAILED(D3DXCreateTextureShader((DWORD*)(pEffectShaderBuffer->GetBufferPointer()), &pEffectShader))) {
#ifdef PRINT_DEBUG_INFO
			debugPrintf(_T("  * Error: Couldn't create texture shader.\n"));
#endif
			RELEASE(pEffectShaderBuffer);

			goto HANDLE_ERROR;
		}

		FillEffectTexture();

		RELEASE(pEffectShaderBuffer);
	}

	RELEASE(pErrorBuffer);

	return 0;

HANDLE_ERROR:

	RELEASE(pErrorBuffer);

	return 1;
}

static void dx9DrawOSD()
{
	extern bool bEditActive;
	if (bEditActive) {
		VidOverlaySetChatInput(EditText);
	}

	VidOverlayRender(Dest, nGameWidth, nGameHeight, 0); // bVidDX9Scanlines ? (nVidScanIntensity & 0xFF) : 0.f);
}

static int dx9Init()
{
	if (nVidFullscreen && !hScrnWnd) {
		return 1;
	}

#ifdef ENABLE_PROFILING
	ProfileInit();
#endif

#ifdef PRINT_DEBUG_INFO
	debugPrintf(_T("*** Initialising Direct3D 9 blitter.\n"));
#endif

	hVidWnd = hScrnWnd;								// Use Screen window for video

	// Get pointer to Direct3D
	if (bVidDX9LegacyRenderer) {
		pD3D = (IDirect3D9Ex *)Direct3DCreate9(D3D_SDK_VERSION);
	}
	else {
		Direct3DCreate9Ex(D3D_SDK_VERSION, &pD3D);
	}
	if (pD3D == NULL) {
#ifdef PRINT_DEBUG_INFO
		debugPrintf(_T("  * Error: Couldn't initialise Direct3D.\n"));
#endif
		dx9Exit();
		return 1;
	}

	nRotateGame = 0;
	if (bDrvOkay) {
		if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) {
			if (nVidRotationAdjust & 1) {
				nRotateGame |= (nVidRotationAdjust & 2);
			}
			else {
				nRotateGame |= 1;
			}
		}

		if (BurnDrvGetFlags() & BDF_ORIENTATION_FLIPPED) {
			nRotateGame ^= 2;
		}
	}

	nD3DAdapter = D3DADAPTER_DEFAULT;
	if (nRotateGame & 1 && VerScreen[0]) {
		nD3DAdapter = dx9GetAdapter(VerScreen);
	}
	else {
		if (HorScreen[0]) {
			nD3DAdapter = dx9GetAdapter(HorScreen);
		}
	}

	// check selected adapter
	D3DDISPLAYMODE dm;
	pD3D->GetAdapterDisplayMode(nD3DAdapter, &dm);

	memset(&d3dpp, 0, sizeof(d3dpp));
	if (nVidFullscreen) {
		VidSDisplayScoreInfo ScoreInfo;
		if (dx9SelectFullscreenMode(&ScoreInfo)) {
			dx9Exit();
			return 1;
		}
		d3dpp.BackBufferWidth = ScoreInfo.nBestWidth;
		d3dpp.BackBufferHeight = ScoreInfo.nBestHeight;
		d3dpp.BackBufferFormat = (nVidDepth == 16) ? D3DFMT_R5G6B5 : D3DFMT_X8R8G8B8;
		d3dpp.SwapEffect = D3DSWAPEFFECT_FLIP;
		d3dpp.BackBufferCount = bVidTripleBuffer ? 2 : 1;
		d3dpp.hDeviceWindow = hVidWnd;
		d3dpp.Windowed = bVidDX9WinFullscreen;
		d3dpp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
		d3dpp.PresentationInterval = bVidVSync ? D3DPRESENT_INTERVAL_DEFAULT : D3DPRESENT_INTERVAL_IMMEDIATE;
		if (bVidDX9WinFullscreen) {
			d3dpp.SwapEffect = bVidVSync || bVidDX9LegacyRenderer ? D3DSWAPEFFECT_COPY : D3DSWAPEFFECT_FLIPEX;
			MoveWindow(hScrnWnd, 0, 0, d3dpp.BackBufferWidth, d3dpp.BackBufferHeight, TRUE);
		}
	} else {
		RECT rect;
		GetClientRect(hVidWnd, &rect);
		d3dpp.BackBufferWidth = rect.right - rect.left;
		d3dpp.BackBufferHeight = rect.bottom - rect.top;
		d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
		d3dpp.SwapEffect = bVidVSync || bVidDX9LegacyRenderer ? D3DSWAPEFFECT_COPY : D3DSWAPEFFECT_FLIPEX;
		d3dpp.BackBufferCount = 1;
		d3dpp.hDeviceWindow = hVidWnd;
		d3dpp.Windowed = TRUE;
		d3dpp.PresentationInterval = bVidVSync ? D3DPRESENT_INTERVAL_DEFAULT : D3DPRESENT_INTERVAL_IMMEDIATE;
	}

	D3DDISPLAYMODEEX dmex;
	dmex.Format = d3dpp.BackBufferFormat;
	dmex.Width = d3dpp.BackBufferWidth;
	dmex.Height = d3dpp.BackBufferHeight;
	dmex.RefreshRate = d3dpp.FullScreen_RefreshRateInHz;
	dmex.ScanLineOrdering = D3DSCANLINEORDERING_UNKNOWN;
	dmex.Size = sizeof(D3DDISPLAYMODEEX);

	DWORD dwBehaviorFlags = D3DCREATE_FPU_PRESERVE;
	dwBehaviorFlags |= bVidHardwareVertex ? D3DCREATE_HARDWARE_VERTEXPROCESSING : D3DCREATE_SOFTWARE_VERTEXPROCESSING;
#ifdef _DEBUG
	dwBehaviorFlags |= D3DCREATE_DISABLE_DRIVER_MANAGEMENT;
#endif

	HRESULT hRet;
	if (bVidDX9LegacyRenderer) {
		hRet = pD3D->CreateDevice(nD3DAdapter, D3DDEVTYPE_HAL, hVidWnd, dwBehaviorFlags, &d3dpp, (IDirect3DDevice9 **)&pD3DDevice);
	} else {
		hRet = pD3D->CreateDeviceEx(nD3DAdapter, D3DDEVTYPE_HAL, hVidWnd, dwBehaviorFlags, &d3dpp, d3dpp.Windowed ? NULL : &dmex, &pD3DDevice);
	}

	if (FAILED(hRet)) {
		if (nVidFullscreen) {
			FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_UI_FULL_PROBLEM), d3dpp.BackBufferWidth, d3dpp.BackBufferHeight, d3dpp.BackBufferFormat, d3dpp.FullScreen_RefreshRateInHz);
			if (bVidArcaderes && (d3dpp.BackBufferWidth != 320 && d3dpp.BackBufferHeight != 240)) {
				FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_UI_FULL_CUSTRES));
			}
			FBAPopupDisplay(PUF_TYPE_ERROR);
		}

		dx9Exit();
		return 1;
	}

	{
		D3DDISPLAYMODE dm;
		pD3D->GetAdapterDisplayMode(nD3DAdapter, &dm);
		nVidScrnWidth = dm.Width; nVidScrnHeight = dm.Height;
		nVidScrnDepth = (dm.Format == D3DFMT_R5G6B5) ? 16 : 32;
	}

	nGameWidth = nVidImageWidth; nGameHeight = nVidImageHeight;

	nRotateGame = 0;
	if (bDrvOkay) {
		// Get the game screen size
		BurnDrvGetVisibleSize(&nGameWidth, &nGameHeight);

		if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) {
			if (nVidRotationAdjust & 1) {
				int n = nGameWidth;
				nGameWidth = nGameHeight;
				nGameHeight = n;
				nRotateGame |= (nVidRotationAdjust & 2);
			}
			else {
				nRotateGame |= 1;
			}
		}

		if (BurnDrvGetFlags() & BDF_ORIENTATION_FLIPPED) {
			nRotateGame ^= 2;
		}
	}

	//	VidSSetupGamma(BlitFXPrim);

		// Initialize the buffer surfaces
	if (dx9SurfaceInit()) {
		dx9Exit();
		return 1;
	}
	if (dx9EffectInit()) {
		dx9Exit();
		return 1;
	}

	for (int y = 0; y < RENDER_STRIPS; y++) {
		if (FAILED(pD3DDevice->CreateVertexBuffer(4 * sizeof(D3DLVERTEX2), D3DUSAGE_WRITEONLY, D3DFVF_LVERTEX2, D3DPOOL_DEFAULT, &pVB[y], NULL))) {
			dx9Exit();
			return 1;
		}
	}
	if (FAILED(pD3DDevice->CreateVertexBuffer(4 * sizeof(D3DLVERTEX2), D3DUSAGE_WRITEONLY, D3DFVF_LVERTEX2, D3DPOOL_DEFAULT, &pIntermediateVB, NULL))) {
		dx9Exit();
		return 1;
	}

	nImageWidth = 0;
	nImageHeight = 0;

	dPrevCubicB = dPrevCubicC = -999;

	pD3DDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
	pD3DDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
	pD3DDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	pD3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);

	// Overlay
	VidOverlayInit(pD3DDevice);

#ifdef PRINT_DEBUG_INFO
	{
		debugPrintf(_T("  * Initialisation complete: %.2lfMB texture memory free (total).\n"), (double)pD3DDevice->GetAvailableTextureMem() / (1024 * 1024));
		debugPrintf(_T("    Displaying and rendering in %i-bit mode, emulation running in %i-bit mode.\n"), nVidScrnDepth, nVidImageDepth);
		if (nVidFullscreen) {
			debugPrintf(_T("    Running in fullscreen mode (%i x %i), "), nVidScrnWidth, nVidScrnHeight);
			debugPrintf(_T("using a %s buffer.\n"), bVidTripleBuffer ? _T("triple") : _T("double"));
		}
		else {
			debugPrintf(_T("    Running in windowed mode, using D3DSWAPEFFECT_COPY to present the image.\n"));
		}
	}
#endif

	return 0;
}

static int dx9Reset()
{
	dx9ReleaseResources();

	if (FAILED(pD3DDevice->Reset(&d3dpp))) {
		return 1;
	}

	dx9SurfaceInit();
	dx9EffectInit();

	for (int y = 0; y < RENDER_STRIPS; y++) {
		pD3DDevice->CreateVertexBuffer(4 * sizeof(D3DLVERTEX2), D3DUSAGE_WRITEONLY, D3DFVF_LVERTEX2, D3DPOOL_DEFAULT, &pVB[y], NULL);
	}
	pD3DDevice->CreateVertexBuffer(4 * sizeof(D3DLVERTEX2), D3DUSAGE_WRITEONLY, D3DFVF_LVERTEX2, D3DPOOL_DEFAULT, &pIntermediateVB, NULL);

	pD3DDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
	pD3DDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	pD3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);

	nImageWidth = 0; nImageHeight = 0;

	dPrevCubicB = dPrevCubicC = -999;

	return 0;
}

static int dx9Scale(RECT* rect, int nWidth, int nHeight)
{
	return VidSScaleImage(rect, nWidth, nHeight, bVidScanRotate);
}

// Copy BlitFXsMem to pddsBlitFX
static int dx9Render()
{
	GetClientRect(hVidWnd, &Dest);

	pD3DDevice->SetSamplerState(0, D3DSAMP_MINFILTER, bVidDX9Bilinear ? D3DTEXF_LINEAR : D3DTEXF_POINT);
	pD3DDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, bVidDX9Bilinear ? D3DTEXF_LINEAR : D3DTEXF_POINT);

	if (bVidArcaderes) {
		Dest.left = (Dest.right + Dest.left) / 2;
		Dest.left -= nGameWidth / 2;
		Dest.right = Dest.left + nGameWidth;

		Dest.top = (Dest.top + Dest.bottom) / 2;
		Dest.top -= nGameHeight / 2;
		Dest.bottom = Dest.top + nGameHeight;
	}
	else {
		dx9Scale(&Dest, nGameWidth, nGameHeight);
	}

	{
		int nNewImageWidth = nRotateGame ? (Dest.bottom - Dest.top) : (Dest.right - Dest.left);
		int nNewImageHeight = nRotateGame ? (Dest.right - Dest.left) : (Dest.bottom - Dest.top);

		if (nImageWidth != nNewImageWidth || nImageHeight != nNewImageHeight) {
			nImageWidth = nNewImageWidth;
			nImageHeight = nNewImageHeight;
			dx9GeometryInit();
		}
	}

	{
		if (dPrevCubicB != dVidCubicB || dPrevCubicC != dVidCubicC) {
			dPrevCubicB = dVidCubicB;
			dPrevCubicC = dVidCubicC;
			FillEffectTexture();
		}
	}

	{
		// Copy the game image to a surface in video memory

		D3DLOCKED_RECT lr;

		if (FAILED(pSurface->LockRect(&lr, NULL, 0))) {
#ifdef PRINT_DEBUG_INFO
			debugPrintf(_T("  * Error: Couldn't lock surface.\n"));
#endif
			return 1;
		}

		unsigned char* ps = pVidImage + nVidImageLeft * nVidImageBPP;
		unsigned char* pd = (unsigned char*)lr.pBits;
		int p = lr.Pitch;
		int s = nVidImageWidth * nVidImageBPP;

		for (int y = 0; y < nVidImageHeight; y++, pd += p, ps += nVidImagePitch) {
			memcpy(pd, ps, s);
		}

		if (kNetLua) {
			FBA_LuaGui(pd, nVidImageWidth, nVidImageHeight, nVidImageBPP, p);
		}

		pSurface->UnlockRect();
	}

	{
		// Copy the game image onto a texture for rendering

		RECT rect = { 0, 0, nVidImageWidth, nVidImageHeight };
		IDirect3DSurface9* pDest;

		if (FAILED(pTexture->GetSurfaceLevel(0, &pDest))) {
#ifdef PRINT_DEBUG_INFO
			debugPrintf(_T("  * Error: Couldn't get texture surface.\n"));
			return 1;
#endif
		}

		if (FAILED(pD3DDevice->StretchRect(pSurface, &rect, pDest, &rect, D3DTEXF_NONE))) {
#ifdef PRINT_DEBUG_INFO
			debugPrintf(_T("  * Error: Couldn't copy image.\n"));
#endif
		}

		pDest->Release();
	}

#ifdef ENABLE_PROFILING
	{
		// Force the D3D pipeline to be flushed

		D3DLVERTEX2* pVertexData;

		if (SUCCEEDED(pIntermediateVB->Lock(0, 4 * sizeof(D3DLVERTEX2), (void**)&pVertexData, 0))) {
			pIntermediateVB->Unlock();
		}

		ProfileProfileStart(2);
	}
#endif

	//pD3DDevice->SetRenderState(D3DRS_TEXTUREFACTOR, nVidScanIntensity);

	UINT nTotalPasses, nPass = 0;
	pEffect->Begin(&nTotalPasses, D3DXFX_DONOTSAVESTATE);

	if (nTotalPasses > 1) {
		IDirect3DSurface9* pPreviousTarget;
		IDirect3DSurface9* pNewTarget;

		pD3DDevice->GetRenderTarget(0, &pPreviousTarget);
		pIntermediateTexture->GetSurfaceLevel(0, &pNewTarget);
		pD3DDevice->SetRenderTarget(0, pNewTarget);

		D3DVIEWPORT9 vp;
		// Set the size of the image on the PC screen
		vp.X = 0;
		vp.Y = 0;
		vp.Width = nImageWidth;
		vp.Height = nTextureHeight;
		vp.MinZ = 0.0f;
		vp.MaxZ = 1.0f;
		pD3DDevice->SetViewport(&vp);

		pD3DDevice->BeginScene();

		pD3DDevice->SetFVF(D3DFVF_LVERTEX2);

		pEffect->BeginPass(nPass);
		pEffect->CommitChanges();

		pD3DDevice->SetStreamSource(0, pIntermediateVB, 0, sizeof(D3DLVERTEX2));
		pD3DDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
		pEffect->EndPass();

		pD3DDevice->EndScene();

		pD3DDevice->SetRenderTarget(0, pPreviousTarget);

		RELEASE(pPreviousTarget);
		RELEASE(pNewTarget);

		nPass++;
	}

#if 0
	{
		// blit the intermediate texture to the screen

		RECT srect = { 0, 0, Dest.right - Dest.left, 256 };
		RECT drect = { 0, nMenuHeight, Dest.right - Dest.left, nMenuHeight + 256 };
		IDirect3DSurface9* pDest;
		IDirect3DSurface9* pSrc;

		pD3DDevice->GetRenderTarget(0, &pDest);
		pIntermediateTexture->GetSurfaceLevel(0, &pSrc);

		pD3DDevice->StretchRect(pSrc, &srect, pDest, &drect, D3DTEXF_NONE);

		pSrc->Release();
		pDest->Release();

		pEffect->End();

		return 0;
	}
#endif

#ifdef ENABLE_PROFILING
	{
		// Force the D3D pipeline to be flushed

		D3DLVERTEX2* pVertexData;

		if (SUCCEEDED(pIntermediateVB->Lock(0, 4 * sizeof(D3DLVERTEX2), (void**)&pVertexData, 0))) {
			pIntermediateVB->Unlock();
		}

		ProfileProfileEnd(2);
	}
	ProfileProfileStart(0);
#endif

	{
		D3DVIEWPORT9 vp;
		vp.X = Dest.left;
		vp.Y = Dest.top;
		vp.Width = Dest.right - Dest.left;
		vp.Height = Dest.bottom - Dest.top;
		vp.MinZ = 0.0f;
		vp.MaxZ = 1.0f;

		pD3DDevice->SetViewport(&vp);

		pD3DDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 0.0f, 0);

		pD3DDevice->BeginScene();

		pD3DDevice->SetFVF(D3DFVF_LVERTEX2);

		//	pD3DDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);

		pEffect->BeginPass(nPass);
		pEffect->CommitChanges();

		for (int y = 0; y < RENDER_STRIPS; y++) {
			pD3DDevice->SetStreamSource(0, pVB[y], 0, sizeof(D3DLVERTEX2));
			pD3DDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
		}
		pEffect->EndPass();

		pD3DDevice->SetSamplerState(0, D3DSAMP_MINFILTER, bVidDX9Bilinear ? D3DTEXF_LINEAR : D3DTEXF_POINT);
		pD3DDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, bVidDX9Bilinear ? D3DTEXF_LINEAR : D3DTEXF_POINT);

		// draw osd text
		dx9DrawOSD();

		pD3DDevice->EndScene();
	}

	pEffect->End();

#ifdef ENABLE_PROFILING
	{
		// Force the D3D pipeline to be flushed

		D3DLVERTEX2* pVertexData;

		if (SUCCEEDED(pVB[RENDER_STRIPS - 1]->Lock(0, 4 * sizeof(D3DLVERTEX2), (void**)&pVertexData, 0))) {
			pVB[RENDER_STRIPS - 1]->Unlock();
		}

		ProfileProfileEnd(0);
	}
#endif

	return 0;
}

// Run one frame and render the screen
static int dx9Frame(bool bRedraw)					// bRedraw = 0
{
	if (pVidImage == NULL) {
		return 1;
	}

	HRESULT nCoopLevel = pD3DDevice->TestCooperativeLevel();
	if (nCoopLevel != D3D_OK) {						// We've lost control of the screen
		if (nCoopLevel != D3DERR_DEVICENOTRESET) {
			return 1;
		}

		if (dx9Reset()) {
			return 1;
		}

		return 1;                                   // Skip this frame, pBurnDraw pointer not valid (dx9Reset() -> dx9SurfaceInit -> VidSAllocVidImage())
	}

#ifdef ENABLE_PROFILING
	//	ProfileProfileStart(0);
#endif

	if (bDrvOkay) {
		if (bRedraw) {								// Redraw current frame
			if (BurnDrvRedraw()) {
				BurnDrvFrame();						// No redraw function provided, advance one frame
			}
		}
		else {
			BurnDrvFrame();							// Run one frame and draw the screen
		}

		if ((BurnDrvGetFlags() & BDF_16BIT_ONLY) && pVidTransCallback)
			pVidTransCallback();
	}

	return 0;
}

// Paint the BlitFX surface onto the primary surface
static int dx9Paint(int bValidate)
{
	if (!bDrvOkay) {
		return 0;
	}

	if (pD3DDevice->TestCooperativeLevel()) {		// We've lost control of the screen
		return 1;
	}

	dx9Render();									// Copy the memory buffer to the directdraw buffer for later blitting

	pD3DDevice->PresentEx(NULL, NULL, NULL, NULL, 0);

	return 0;
}

// ----------------------------------------------------------------------------

static int dx9GetSettings(InterfaceInfo* pInfo)
{
	TCHAR szString[MAX_PATH] = _T("");

	if (nVidFullscreen) {
		if (bVidTripleBuffer) {
			IntInfoAddStringModule(pInfo, _T("Using a triple buffer"));
		}
		else {
			IntInfoAddStringModule(pInfo, _T("Using a double buffer"));
		}
	}
	else {
		IntInfoAddStringModule(pInfo, _T("Using D3DSWAPEFFECT_COPY to present the image"));
	}
	switch (DX9_FILTER) {
		case 1: {
			IntInfoAddStringModule(pInfo, _T("Applying linear filter"));
			break;
		}
		case 2: {
			_sntprintf(szString, MAX_PATH, _T("Applying cubic filter using VS1.1 & %s"), bUsePS14 ? _T("PS1.4") : _T("PS2.0"));
			IntInfoAddStringModule(pInfo, szString);

			if (bUsePS14) {
				IntInfoAddStringModule(pInfo, _T("Using high-performance implementation"));
				break;
			}

			switch (DX9_SHADERPRECISION) {
			case 0:
				IntInfoAddStringModule(pInfo, _T("Using single-pass reference implementation"));
				break;
			case 1:
				IntInfoAddStringModule(pInfo, _T("Using partial precision single-pass implementation"));
				break;
			case 2:
				IntInfoAddStringModule(pInfo, _T("Using full precision multi-pass implementation"));
				break;
			case 3:
				IntInfoAddStringModule(pInfo, _T("Using partial precision multi-pass implementation"));
				break;
			case 4:
				IntInfoAddStringModule(pInfo, _T("Using high-performance multi-pass implementation"));
				break;
			}

			if (DX9_USE_FPTEXTURE) {
				_sntprintf(szString, MAX_PATH, _T("Using floating-point textures where applicable"));
				IntInfoAddStringModule(pInfo, szString);
			}
			break;
		}
		default: {
			IntInfoAddStringModule(pInfo, _T("Applying point filter"));
		}
	}

	return 0;
}

// The Video Output plugin:
struct VidOut VidOutDX9 = { dx9Init, dx9Exit, dx9Frame, dx9Paint, dx9Scale, dx9GetSettings, _T("DirectX9 Experimental video output") };
//#else
//struct VidOut VidOutDX9 = { NULL, NULL, NULL, NULL, NULL, NULL, _T("DirectX9 Experimental video output") };
//#endif











// Direct3D9 video output
// rewritten by regret (Motion Blur source from VBA-M)

struct d3dvertex {
	float x, y, z, rhw;	//screen coords
	float u, v;			//texture coords
};

struct transp_vertex {
	float x, y, z, rhw;
	D3DCOLOR color;
	float u, v;
};

char *HardFXFilenames[] = {
	"ui/shaders/crt_aperture.fx",
	"ui/shaders/crt_caligari.fx",
	"ui/shaders/crt_cgwg_fast.fx",
	"ui/shaders/crt_easymode.fx",
	"ui/shaders/crt_standard.fx",
	"ui/shaders/crt_bicubic.fx",
	"ui/shaders/crt_cga.fx"
};

#undef D3DFVF_LVERTEX2
#define D3DFVF_LVERTEX2 (D3DFVF_XYZRHW | D3DFVF_TEX1)

IDirect3DTexture9* emuTexture;
static d3dvertex vertex[4];
static transp_vertex transpVertex[4];

D3DFORMAT textureFormat;

static int nPreScale = 0;
static int nPreScaleZoom = 0;
static int nPreScaleEffect = 0;

static int dx9AltScale(RECT* rect, int width, int height);

// Select optimal full-screen resolution
static int dx9AltSelectFullscreenMode(VidSDisplayScoreInfo* pScoreInfo)
{
	if (bVidArcaderes) {
		if (!VidSGetArcaderes((int*)&(pScoreInfo->nBestWidth), (int*)&(pScoreInfo->nBestHeight))) {
			return 1;
		}
	}
	else {
		pScoreInfo->nBestWidth = nVidWidth;
		pScoreInfo->nBestHeight = nVidHeight;
	}

	if (!bDrvOkay && (pScoreInfo->nBestWidth < 640 || pScoreInfo->nBestHeight < 480)) {
		return 1;
	}

	return 0;
}

static void dx9AltReleaseTexture()
{
	RELEASE(emuTexture)
}

static int dx9AltExit()
{
	VidSoftFXExit();

	dx9AltReleaseTexture();

	VidOverlayEnd();
	VidSFreeVidImage();

	if (pVidEffect) {
		delete pVidEffect;
		pVidEffect = NULL;
	}

	RELEASE(pEffect)
	RELEASE(pD3DDevice)
	RELEASE(pD3D)

	nRotateGame = 0;

	return 0;
}

static int dx9AltResize(int width, int height)
{
	if (!emuTexture) {
		if (FAILED(pD3DDevice->CreateTexture(width, height, 1, D3DUSAGE_DYNAMIC, textureFormat, D3DPOOL_DEFAULT, &emuTexture, NULL))) {
			return 1;
		}
	}

	return 0;
}

static int dx9AltTextureInit()
{
	if (nRotateGame & 1) {
		nVidImageWidth = nGameHeight;
		nVidImageHeight = nGameWidth;
	}
	else {
		nVidImageWidth = nGameWidth;
		nVidImageHeight = nGameHeight;
	}

	nGameImageWidth = nVidImageWidth;
	nGameImageHeight = nVidImageHeight;

	//nVidImageDepth = nVidScrnDepth;
	nVidImageDepth = 16;

	// Determine if we should use a texture format different from the screen format
	if ((bDrvOkay && VidSoftFXCheckDepth(nPreScaleEffect, 32) != 32) || (bDrvOkay && bVidForce16bitDx9Alt)) {
		nVidImageDepth = 16;
	}

	switch (nVidImageDepth) {
	case 32:
		textureFormat = D3DFMT_X8R8G8B8;
		break;
	case 24:
		textureFormat = D3DFMT_R8G8B8;
		break;
	case 16:
		textureFormat = D3DFMT_R5G6B5;
		break;
	case 15:
		textureFormat = D3DFMT_X1R5G5B5;
		break;
	}

	nVidImageBPP = (nVidImageDepth + 7) >> 3;
	nBurnBpp = nVidImageBPP;	// Set Burn library Bytes per pixel

	// Use our callback to get colors:
	SetBurnHighCol(nVidImageDepth);

	// Make the normal memory buffer
	if (VidSAllocVidImage()) {
		dx9AltExit();
		return 1;
	}

	nTextureWidth = GetTextureSize(nGameImageWidth * nPreScaleZoom);
	nTextureHeight = GetTextureSize(nGameImageHeight * nPreScaleZoom);

	if (dx9AltResize(nTextureWidth, nTextureHeight)) {
		return 1;
	}

	return 0;
}

// Vertex format:
//
// 0---------1
// |        /|
// |      /  |
// |    /    |
// |  /      |
// |/        |
// 2---------3
//
// (x,y) screen coords, in pixels
// (u,v) texture coords, betweeen 0.0 (top, left) to 1.0 (bottom, right)
static int dx9AltSetVertex(unsigned int px, unsigned int py, unsigned int pw, unsigned int ph, unsigned int tw, unsigned int th, unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
	// configure triangles
	// -0.5f is necessary in order to match texture alignment to display pixels
	float diff = -0.5f;
	if (nRotateGame & 1) {
		if (nRotateGame & 2) {
			vertex[2].x = vertex[3].x = (float)(y)+diff;
			vertex[0].x = vertex[1].x = (float)(y + h) + diff;
			vertex[1].y = vertex[3].y = (float)(x + w) + diff;
			vertex[0].y = vertex[2].y = (float)(x)+diff;
		} else {
			vertex[0].x = vertex[1].x = (float)(y)+diff;
			vertex[2].x = vertex[3].x = (float)(y + h) + diff;
			vertex[1].y = vertex[3].y = (float)(x)+diff;
			vertex[0].y = vertex[2].y = (float)(x + w) + diff;
		}
	}
	else {
		if (nRotateGame & 2) {
			vertex[1].x = vertex[3].x = (float)(y)+diff;
			vertex[0].x = vertex[2].x = (float)(y + h) + diff;
			vertex[2].y = vertex[3].y = (float)(x)+diff;
			vertex[0].y = vertex[1].y = (float)(x + w) + diff;
		} else {
			vertex[0].x = vertex[2].x = (float)(x)+diff;
			vertex[1].x = vertex[3].x = (float)(x + w) + diff;
			vertex[0].y = vertex[1].y = (float)(y)+diff;
			vertex[2].y = vertex[3].y = (float)(y + h) + diff;
		}
	}

	float rw = (float)w / (float)pw * (float)tw;
	float rh = (float)h / (float)ph * (float)th;
	vertex[0].u = vertex[2].u = (float)(px) / rw;
	vertex[1].u = vertex[3].u = (float)(px + w) / rw;
	vertex[0].v = vertex[1].v = (float)(py) / rh;
	vertex[2].v = vertex[3].v = (float)(py + h) / rh;

	// Z-buffer and RHW are unused for 2D blit, set to normal values
	vertex[0].z = vertex[1].z = vertex[2].z = vertex[3].z = 0.0f;
	vertex[0].rhw = vertex[1].rhw = vertex[2].rhw = vertex[3].rhw = 1.0f;

	// configure semi-transparent triangles
	if (bVidMotionBlur) {
		D3DCOLOR semiTrans = D3DCOLOR_ARGB(0x7F, 0xFF, 0xFF, 0xFF);
		transpVertex[0].x = vertex[0].x;
		transpVertex[0].y = vertex[0].y;
		transpVertex[0].z = vertex[0].z;
		transpVertex[0].rhw = vertex[0].rhw;
		transpVertex[0].color = semiTrans;
		transpVertex[0].u = vertex[0].u;
		transpVertex[0].v = vertex[0].v;
		transpVertex[1].x = vertex[1].x;
		transpVertex[1].y = vertex[1].y;
		transpVertex[1].z = vertex[1].z;
		transpVertex[1].rhw = vertex[1].rhw;
		transpVertex[1].color = semiTrans;
		transpVertex[1].u = vertex[1].u;
		transpVertex[1].v = vertex[1].v;
		transpVertex[2].x = vertex[2].x;
		transpVertex[2].y = vertex[2].y;
		transpVertex[2].z = vertex[2].z;
		transpVertex[2].rhw = vertex[2].rhw;
		transpVertex[2].color = semiTrans;
		transpVertex[2].u = vertex[2].u;
		transpVertex[2].v = vertex[2].v;
		transpVertex[3].x = vertex[3].x;
		transpVertex[3].y = vertex[3].y;
		transpVertex[3].z = vertex[3].z;
		transpVertex[3].rhw = vertex[3].rhw;
		transpVertex[3].color = semiTrans;
		transpVertex[3].u = vertex[3].u;
		transpVertex[3].v = vertex[3].v;
	}

	return 0;
}

static void dx9AltDrawOSD()
{
	extern bool bEditActive;
	if (bEditActive) {
		VidOverlaySetChatInput(EditText);
	}

	VidOverlayRender(Dest, nGameWidth, nGameHeight, bVidDX9Scanlines ? (nVidScanIntensity & 0xFF) : 0.f);
}

static int dx9AltSetHardFX(int nHardFX)
{
	// cutre reload
	//static bool reload = true; if (GetAsyncKeyState(VK_CONTROL)) { if (reload) { nDX9HardFX = 0; reload = false; } } else reload = true;
	
	if (nHardFX == nDX9HardFX)
	{
		return 0;
	}
	

	nDX9HardFX = nHardFX;

	if (pVidEffect) {
		delete pVidEffect;
		pVidEffect = NULL;
	}
	
	if (nDX9HardFX == 0)
	{
		return 0;
	}

	// HardFX
	pVidEffect = new VidEffect(pD3DDevice);
	int r = pVidEffect->Load(HardFXFilenames[nHardFX - 1]);

	if (r == 0)
	{
		// common parameters
		pVidEffect->SetParamFloat2("texture_size", nTextureWidth, nTextureHeight);
		pVidEffect->SetParamFloat2("video_size", (nRotateGame ? nGameHeight : nGameWidth) + 0.5f, nRotateGame ? nGameWidth : nGameHeight + 0.5f);
	}

	return r;
}


static int dx9AltInit()
{
	if (hScrnWnd == NULL) {
		return 1;
	}

	hVidWnd = hScrnWnd;

	// Get pointer to Direct3D
	if (bVidDX9LegacyRenderer) {
		pD3D = (IDirect3D9Ex *)Direct3DCreate9(D3D_SDK_VERSION);
	}
	else {
		Direct3DCreate9Ex(D3D_SDK_VERSION, &pD3D);
	}
	if (pD3D == NULL) {
		dx9AltExit();
		return 1;
	}

	nRotateGame = 0;
	nGameWidth = 640;
	nGameHeight = 480;
	if (bDrvOkay) {
		if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) {
			if (nVidRotationAdjust & 1) {
				nRotateGame |= (nVidRotationAdjust & 2);
			}
			else {
				nRotateGame |= 1;
			}
		}

		if (BurnDrvGetFlags() & BDF_ORIENTATION_FLIPPED) {
			nRotateGame ^= 2;
		}

		// Get the game screen size
		BurnDrvGetVisibleSize(&nGameWidth, &nGameHeight);
	}
	else {
		return 0;
	}

	nD3DAdapter = D3DADAPTER_DEFAULT;
	if (nRotateGame & 1 && VerScreen[0]) {
		nD3DAdapter = dx9GetAdapter(VerScreen);
	}
	else {
		if (HorScreen[0]) {
			nD3DAdapter = dx9GetAdapter(HorScreen);
		}
	}

	// check selected adapter
	D3DDISPLAYMODE dm;
	pD3D->GetAdapterDisplayMode(nD3DAdapter, &dm);

	memset(&d3dpp, 0, sizeof(d3dpp));
	if (nVidFullscreen) {
		VidSDisplayScoreInfo ScoreInfo;
		if (dx9AltSelectFullscreenMode(&ScoreInfo)) {
			dx9AltExit();
			return 1;
		}
		d3dpp.BackBufferWidth = ScoreInfo.nBestWidth;
		d3dpp.BackBufferHeight = ScoreInfo.nBestHeight;
		d3dpp.BackBufferFormat = (nVidDepth == 16) ? D3DFMT_R5G6B5 : D3DFMT_X8R8G8B8;
		d3dpp.SwapEffect = D3DSWAPEFFECT_FLIP;
		d3dpp.BackBufferCount = bVidTripleBuffer ? 2 : 1;
		d3dpp.hDeviceWindow = hVidWnd;
		d3dpp.Windowed = bVidDX9WinFullscreen;
		d3dpp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
		d3dpp.PresentationInterval = bVidVSync ? D3DPRESENT_INTERVAL_DEFAULT : D3DPRESENT_INTERVAL_IMMEDIATE;
		if (bVidDX9WinFullscreen) {
			d3dpp.SwapEffect = bVidVSync || bVidDX9LegacyRenderer ? D3DSWAPEFFECT_COPY : D3DSWAPEFFECT_FLIPEX;
			MoveWindow(hScrnWnd, 0, 0, d3dpp.BackBufferWidth, d3dpp.BackBufferHeight, TRUE);
		}
	} else {
		RECT rect;
		GetClientRect(hVidWnd, &rect);
		d3dpp.BackBufferWidth = rect.right - rect.left;
		d3dpp.BackBufferHeight = rect.bottom - rect.top;
		d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
		d3dpp.SwapEffect = bVidVSync || bVidDX9LegacyRenderer ? D3DSWAPEFFECT_COPY : D3DSWAPEFFECT_FLIPEX;
		d3dpp.BackBufferCount = 1;
		d3dpp.hDeviceWindow = hVidWnd;
		d3dpp.Windowed = TRUE;
		d3dpp.PresentationInterval = bVidVSync ? D3DPRESENT_INTERVAL_DEFAULT : D3DPRESENT_INTERVAL_IMMEDIATE;
	}

	D3DDISPLAYMODEEX dmex;
	dmex.Format = d3dpp.BackBufferFormat;
	dmex.Width = d3dpp.BackBufferWidth;
	dmex.Height = d3dpp.BackBufferHeight;
	dmex.RefreshRate = d3dpp.FullScreen_RefreshRateInHz;
	dmex.ScanLineOrdering = D3DSCANLINEORDERING_UNKNOWN;
	dmex.Size = sizeof(D3DDISPLAYMODEEX);

	DWORD dwBehaviorFlags = D3DCREATE_FPU_PRESERVE;
	dwBehaviorFlags |= bVidHardwareVertex ? D3DCREATE_HARDWARE_VERTEXPROCESSING : D3DCREATE_SOFTWARE_VERTEXPROCESSING;
#ifdef _DEBUG
	dwBehaviorFlags |= D3DCREATE_DISABLE_DRIVER_MANAGEMENT;
#endif

	HRESULT hRet;
	if (bVidDX9LegacyRenderer) {
		hRet = pD3D->CreateDevice(nD3DAdapter, D3DDEVTYPE_HAL, hVidWnd, dwBehaviorFlags, &d3dpp, (IDirect3DDevice9 **)&pD3DDevice);
	} else {
		hRet = pD3D->CreateDeviceEx(nD3DAdapter, D3DDEVTYPE_HAL, hVidWnd, dwBehaviorFlags, &d3dpp, d3dpp.Windowed ? NULL : &dmex, &pD3DDevice);
	}

	if (FAILED(hRet)) {
		if (nVidFullscreen) {
			FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_UI_FULL_PROBLEM), d3dpp.BackBufferWidth, d3dpp.BackBufferHeight, d3dpp.BackBufferFormat, d3dpp.FullScreen_RefreshRateInHz);
			if (bVidArcaderes && (d3dpp.BackBufferWidth != 320 && d3dpp.BackBufferHeight != 240)) {
				FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_UI_FULL_CUSTRES));
			}
			FBAPopupDisplay(PUF_TYPE_ERROR);
		}

		dx9AltExit();
		return 1;
	}

	{
		nVidScrnWidth = d3dpp.BackBufferWidth;
		nVidScrnHeight = d3dpp.BackBufferHeight;
		nVidScrnDepth = (dm.Format == D3DFMT_R5G6B5) ? 16 : 32;
	}

	nRotateGame = 0;
	nDX9HardFX = -1;

	if (bDrvOkay) {
		// Get the game screen size
		BurnDrvGetVisibleSize(&nGameWidth, &nGameHeight);

		if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) {
			if (nVidRotationAdjust & 1) {
				int n = nGameWidth;
				nGameWidth = nGameHeight;
				nGameHeight = n;
				nRotateGame |= (nVidRotationAdjust & 2);
			}
			else {
				nRotateGame |= 1;
			}
		}

		if (BurnDrvGetFlags() & BDF_ORIENTATION_FLIPPED) {
			nRotateGame ^= 2;
		}
	}

	// enable vertex alpha blending
	pD3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	pD3DDevice->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_COLOR1);
	pD3DDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	pD3DDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	// apply vertex alpha values to texture
	pD3DDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);

	// for filter
	nPreScale = 3;
	nPreScaleZoom = 2;
	nPreScaleEffect = 0;
	if (bDrvOkay) {
		nPreScaleEffect = nVidBlitterOpt[nVidSelect] & 0xFF;
		nPreScaleZoom = VidSoftFXGetZoom(nPreScaleEffect);
	}

	// Initialize the buffer surfaces
	if (dx9AltTextureInit()) {
		dx9AltExit();
		return 1;
	}

	if (nPreScaleEffect) {
		if (VidSoftFXInit(nPreScaleEffect, 0)) {
			dx9AltExit();
			return 1;
		}
	}

	nImageWidth = 0;
	nImageHeight = 0;

	pD3DDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
	pD3DDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
	pD3DDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	pD3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);

	// Overlay
	VidOverlayInit(pD3DDevice);

	return 0;
}

static int dx9AltReset()
{
	dx9AltReleaseTexture();

	if (FAILED(pD3DDevice->Reset(&d3dpp))) {
		return 1;
	}

	pD3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	pD3DDevice->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_COLOR1);
	pD3DDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	pD3DDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	// apply vertex alpha values to texture
	pD3DDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);

	dx9AltTextureInit();

	nImageWidth = 0;
	nImageHeight = 0;

	return 0;
}

static int dx9AltScale(RECT* rect, int width, int height)
{
	if (nVidBlitterOpt[nVidSelect] & 0x0100) {
		return VidSoftFXScale(rect, width, height);
	}
	return VidSScaleImage(rect, width, height, bVidScanRotate);
}

static void VidSCpyImg32(unsigned char* dst, unsigned int dstPitch, unsigned char *src, unsigned int srcPitch, unsigned short width, unsigned short height)
{
	// fast, iterative C version
	// copies an width*height array of visible pixels from src to dst
	// srcPitch and dstPitch are the number of garbage bytes after a scanline
	register unsigned short lineSize = width << 2;

	while (height--) {
		memcpy(dst, src, lineSize);
		src += srcPitch;
		dst += dstPitch;
	}
}

static void VidSCpyImg16(unsigned char* dst, unsigned int dstPitch, unsigned char *src, unsigned int srcPitch, unsigned short width, unsigned short height)
{
	register unsigned short lineSize = width << 1;

	while (height--) {
		memcpy(dst, src, lineSize);
		src += srcPitch;
		dst += dstPitch;
	}
}

// Copy BlitFXsMem to pddsBlitFX
static int dx9AltRender()
{
	GetClientRect(hVidWnd, &Dest);

	if (bVidArcaderes) {
		Dest.left = (Dest.right + Dest.left) / 2;
		Dest.left -= nGameWidth / 2;
		Dest.right = Dest.left + nGameWidth;

		Dest.top = (Dest.top + Dest.bottom) / 2;
		Dest.top -= nGameHeight / 2;
		Dest.bottom = Dest.top + nGameHeight;
	}
	else {
		dx9AltScale(&Dest, nGameWidth, nGameHeight);
	}

	{
		int nNewImageWidth = nRotateGame ? (Dest.bottom - Dest.top) : (Dest.right - Dest.left);
		int nNewImageHeight = nRotateGame ? (Dest.right - Dest.left) : (Dest.bottom - Dest.top);

		if (nImageWidth != nNewImageWidth || nImageHeight != nNewImageHeight) {
			nImageWidth = nNewImageWidth;
			nImageHeight = nNewImageHeight;

			int nWidth = nGameImageWidth;
			int nHeight = nGameImageHeight;

			if (nPreScaleEffect) {
				if (nPreScale & 1) {
					nWidth *= nPreScaleZoom;
				}
				if (nPreScale & 2) {
					nHeight *= nPreScaleZoom;
				}
			}

			dx9AltSetVertex(0, 0, nWidth, nHeight, nTextureWidth, nTextureHeight, nRotateGame ? Dest.top : Dest.left, nRotateGame ? Dest.left : Dest.top, nImageWidth, nImageHeight);

			if (pVidEffect && pVidEffect->IsValid())
				pVidEffect->SetParamFloat2("output_size", nImageWidth, nImageHeight);
		}
	}

	{
		// Copy the game image onto a texture for rendering
		D3DLOCKED_RECT d3dlr;
		emuTexture->LockRect(0, &d3dlr, 0, 0);
		if (d3dlr.pBits) {
			int pitch = d3dlr.Pitch;
			unsigned char* pd = (unsigned char*)d3dlr.pBits;

			if (nPreScaleEffect) {
				VidFilterApplyEffect(pd, pitch);
			}
			else {
				unsigned char* ps = pVidImage + nVidImageLeft * nVidImageBPP;
				int s = nVidImageWidth * nVidImageBPP;

				switch (nVidImageDepth) {
				case 32:
					VidSCpyImg32(pd, pitch, ps, s, nVidImageWidth, nVidImageHeight);
					break;
				case 16:
					VidSCpyImg16(pd, pitch, ps, s, nVidImageWidth, nVidImageHeight);
					break;
				}
			}

			if (kNetLua) {
				FBA_LuaGui(pd, nVidImageWidth, nVidImageHeight, nVidImageBPP, pitch);
			}

			emuTexture->UnlockRect(0);
		}
	}

	pD3DDevice->SetSamplerState(0, D3DSAMP_MINFILTER, bVidDX9Bilinear ? D3DTEXF_LINEAR : D3DTEXF_POINT);
	pD3DDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, bVidDX9Bilinear ? D3DTEXF_LINEAR : D3DTEXF_POINT);

	// draw the current frame to the screen
	pD3DDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 0.0f, 0);

	pD3DDevice->BeginScene();
	{
		pD3DDevice->SetTexture(0, emuTexture);
		pD3DDevice->SetFVF(D3DFVF_LVERTEX2);
		if (pVidEffect && pVidEffect->IsValid()) {
			pVidEffect->Begin();
			pD3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertex, sizeof(d3dvertex));
			pVidEffect->End();
		}
		else {
			pD3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertex, sizeof(d3dvertex));
		}
		dx9AltDrawOSD();
	}
	pD3DDevice->EndScene();

	dx9AltSetHardFX(nVidDX9HardFX);

	return 0;
}

// Run one frame and render the screen
static int dx9AltFrame(bool bRedraw)	// bRedraw = 0
{
	if (pVidImage == NULL) {
		return 1;
	}

	HRESULT nCoopLevel = pD3DDevice->TestCooperativeLevel();
	if (nCoopLevel != D3D_OK) {		// We've lost control of the screen
		if (nCoopLevel != D3DERR_DEVICENOTRESET) {
			return 1;
		}

		if (dx9AltReset()) {
			return 1;
		}

		return 1;            // Skip this frame, pBurnDraw pointer not valid (dx9AltReset() -> dx9AltTextureInit() -> VidSAllocVidImage())
	}

	if (bDrvOkay) {
		if (bRedraw) {				// Redraw current frame
			if (BurnDrvRedraw()) {
				BurnDrvFrame();		// No redraw function provided, advance one frame
			}
		}
		else {
			BurnDrvFrame();			// Run one frame and draw the screen
		}

		if ((BurnDrvGetFlags() & BDF_16BIT_ONLY) && pVidTransCallback)
			pVidTransCallback();
	}

	return 0;
}

// Paint the BlitFX surface onto the primary surface
static int dx9AltPaint(int bValidate)
{
	if (!bDrvOkay) {
		return 0;
	}

	if (pD3DDevice->TestCooperativeLevel()) {	// We've lost control of the screen
		return 1;
	}

	dx9AltRender();

	pD3DDevice->Present(NULL, NULL, NULL, NULL);

	return 0;
}

static int dx9AltGetSettings(InterfaceInfo* pInfo)
{
	if (nVidFullscreen) {
		if (bVidTripleBuffer) {
			IntInfoAddStringModule(pInfo, _T("Using a triple buffer"));
		}
		else {
			IntInfoAddStringModule(pInfo, _T("Using a double buffer"));
		}
	}

	if (bDrvOkay) {
		TCHAR szString[MAX_PATH] = _T("");

		_sntprintf(szString, MAX_PATH, _T("Prescaling using %s (%ix zoom)"), VidSoftFXGetEffect(nPreScaleEffect), nPreScaleZoom);
		IntInfoAddStringModule(pInfo, szString);
	}

	if (bVidDX9Bilinear) {
		IntInfoAddStringModule(pInfo, _T("Applying linear filter"));
	}
	else {
		IntInfoAddStringModule(pInfo, _T("Applying point filter"));
	}

	if (bVidMotionBlur) {
		IntInfoAddStringModule(pInfo, _T("Applying motion blur effect"));
	}

	if (bVidDX9Scanlines) {
		IntInfoAddStringModule(pInfo, _T("Drawing scanlines"));
	}

	return 0;
}

// The Video Output plugin:
struct VidOut VidOutDX9Alt = { dx9AltInit, dx9AltExit, dx9AltFrame, dx9AltPaint, dx9AltScale, dx9AltGetSettings, _T("DirectX9 Alternate video output") };
