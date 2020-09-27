//
// D3D9Helpers. Various helper functions for D3D9 interfaces and string/wstring helper funcs
//
// Copyright 2020, MIKA-N. https://github.com/mika-n
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this softwareand associated
// documentation files(the "Software"), to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense, and to permit persons to whom the Software
// is furnished to do so, subject to the following conditions :
// 
// -The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// - The derived work or derived parts are also "open sourced" and the source code or part of the work using components
// from this project is made publicly available with modifications to this base work.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS
// OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//

#ifndef _D3D9HELPERS_H_
#define _D3D9HELPERS_H_

//#define WIN32_LEAN_AND_MEAN			// Exclude rarely-used stuff from Windows headers
//#include <windows.h>
//#include <string>


//------------------------------------------------------------------------------------------------

#ifdef _DEBUG
#define USE_DEBUG 1		// 1=Custom debug outputs and calculations (spends few cycles of precious CPU time), 0=No custom debugging, run the game code without extra debug code
#endif

#ifndef _DEBUG 
#undef USE_DEBUG
#endif

#if USE_DEBUG == 1
#define DebugPrint DebugPrintFunc // DEBUG version to dump logfile messages
#else
#define DebugPrint  // RELEASE version of DebugPrint doing nothing
#endif

#define LogPrint DebugPrintFunc   // LogPrint prints out a txt message both in retail and debug builds

extern unsigned long g_iLogMsgCount; // Num of printed log messages (if message count exceeds X messages then messages are no longer written to a logfile to avoid flooding)

extern void DebugReleaseResources();
extern void DebugCloseFile();
extern void DebugClearFile();
extern void DebugPrintFunc(LPCSTR lpszFormat, ...);
extern void DebugPrintFunc(LPCWSTR lpszFormat, ...);

//------------------------------------------------------------------------------------------------
#define COUNT_OF_ITEMS(array) (sizeof(array)/sizeof(array[0]))  // If a string array uses WCHAR then sizeof is not the same as number of chars (UTF8/Unicode has 2 bytes per char). This macro calculates the real num of items in statically allocated array

#ifndef SAFE_RELEASE
#define SAFE_RELEASE( p ) if( p ){ p->Release(); p = nullptr; }
#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE( p ) if( p ){ delete p; p = nullptr; }
#endif

extern bool _iStarts_With(std::wstring s1, std::wstring s2, bool s2AlreadyInLowercase = FALSE);	 // Case-insensitive starts_with string comparison
extern bool _iEnds_With(std::wstring s1, std::wstring s2, bool s2AlreadyInLowercase = FALSE);    // Case-insensitive ends_with string comparison

extern bool _iStarts_With(std::string s1, std::string s2, bool s2AlreadyInLowercase = FALSE);    // Case-insensitive starts_with string comparison
extern bool _iEnds_With(std::string s1, std::string s2, bool s2AlreadyInLowercase = FALSE);      // Case-insensitive ends_with string comparison

extern bool _iEqual(const std::string& s1, const std::string& s2, bool s2AlreadyInLowercase = FALSE);   // Case-insensitive string comparison
extern bool _iEqual(const std::wstring& s1, const std::wstring& s2, bool s2AlreadyInLowercase = FALSE); // Case-insensitive string comparison

extern void _Trim(std::wstring & s);  // Trim wstring (in-place, modify the s)
extern void _Trim(std::string & s);  // Trim string (in-place, modify the s)
//extern std::wstring _TrimCopy(std::wstring s);  // Trim wstring (return new string, the s unmodified)

extern std::wstring _ReplaceStr(const std::wstring& str, const std::wstring& searchKeyword, const std::wstring& replaceValue, bool caseInsensitive = TRUE);
extern std::string  _ReplaceStr(const std::string& str, const std::string& searchKeyword, const std::string& replaceValue, bool caseInsensitive = TRUE);

extern std::wstring _RemoveEnclosingChar(const std::wstring& str, const WCHAR searchChar, bool caseInsensitive = TRUE);
extern std::string  _RemoveEnclosingChar(const std::string& str, const char searchChar, bool caseInsensitive = TRUE);

extern std::wstring _ToWString(const std::string & s);	   // Convert std::string to std::wstring
extern std::string  _ToString(const std::wstring & s);     // Convert std::wstring to std:string

extern std::string  _ToUTF8String(const std::wstring & s); // Convert widechar std::wstring(UTF8) to multibyte string value (WinOS specific implementation)
extern std::wstring _ToUTF8WString(const std::string & s); // Convert multibyte std::string(UTF8) to widechar UTF8 string value (WinOS specific implementation) 
extern std::wstring _ToUTF8WString(const std::wstring & s);// Convert multibyte std::wstring(UTF8) to widechar UTF8 string value (WinOS specific implementation) 

extern std::wstring _DecodeUtf8String(const std::wstring & s_encoded); // Decode UTF8 encoded string to "normal" string value

extern inline void _ToLowerCase(std::string& s);       // Convert string to lowercase letters (in-place, so the original str in the parameter is converted)
extern inline void _ToLowerCase(std::wstring& s);      // Convert wstring to lowercase letters (in-place)

extern int _SplitString(const std::string& s, std::vector<std::string>& splittedTokens, std::string sep, bool caseInsensitiveSep = true, bool sepAlreadyLowercase = false, int maxTokens = 64); // Split string to several tokens by using sep string as separator
extern int _SplitInHalf(const std::string& s, std::vector<std::string>& splittedTokens, std::string sep, bool caseInsensitiveSep = true, bool sepAlreadyLowercase = false); // Split string to two tokens by using sep string as separator

extern std::string _ToBinaryBitString(BYTE byteValue); // Convert BYTE value to binary "bit field" string (usually some log messages want to show an integer as binary bit field)

extern std::string GetFileVersionInformationAsString(const std::wstring & fileName); // Return file version info as "major.minor.patch.build" string value
extern BOOL GetFileVersionInformationAsNumber(const std::wstring & fileName, UINT* outMajorVer, UINT* outMinorVer, UINT* outPatchVer, UINT* outBuildVer); // Return file version info

extern bool _StringToRect (const std::wstring & s, RECT * outRect, const wchar_t separatorChar = L' '); // String in "0 50 200 400" format is converted as RECT struct value 
extern bool _StringToRect(const std::string & s, RECT * outRect, const wchar_t separatorChar = L' ');
extern bool _StringToPoint(const std::wstring & s, POINT * outPoint, const wchar_t separatorChar = L' '); // String in "0 50" format is converted as POINT struct value 
extern bool _StringToPoint(const std::string & s, POINT * outPoint, const char separatorChar);

//-----------------------------------------------------------------------------------------------------------------------
// Simple DX9 render state change cache and restoration class
//
typedef struct {
	D3DRENDERSTATETYPE stateType;
	DWORD value;
}D3D9RENDERSTATECACHEITEM;
typedef D3D9RENDERSTATECACHEITEM* PD3D9RENDERSTATECACHEITEM;

class CD3D9RenderStateCache
{
protected:
	LPDIRECT3DDEVICE9 m_pD3Device;
	BOOL m_bAutoRestore;
	std::vector<D3D9RENDERSTATECACHEITEM> m_stateCacheList;

public:
	CD3D9RenderStateCache(LPDIRECT3DDEVICE9 pD3Device, BOOL autoRestore = true)
	{
		m_pD3Device = pD3Device;
		m_bAutoRestore = autoRestore; // Restore DX9 render state automatically in destructor if the state is not yet restored
	}

	HRESULT SetRenderState(D3DRENDERSTATETYPE stateType, DWORD value)
	{
		DWORD oldValue;
		if (SUCCEEDED(m_pD3Device->GetRenderState(stateType, &oldValue)))
			m_stateCacheList.push_back({ stateType, oldValue });

		return m_pD3Device->SetRenderState(stateType, value);
	}

	void Clear()
	{
		m_stateCacheList.clear();
	}

	void RestoreState()
	{
		for (auto& cacheItem: m_stateCacheList)
			m_pD3Device->SetRenderState(cacheItem.stateType, cacheItem.value);

		m_stateCacheList.clear();
	}

	void EnableTransparentAlphaBlending()
	{
/*		this->SetRenderState(D3DRS_ALPHAREF, (DWORD)0x000000F0);
		this->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
		this->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
		this->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
		this->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
*/
		this->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		this->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		this->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	}

	~CD3D9RenderStateCache()
	{
		if (m_bAutoRestore) RestoreState();
	}
};


//-----------------------------------------------------------------------------------------------------------------------
// Special flags for D3D9CreateRectangleVertexTexBufferFromFile method to scale or re-position the image within the specifier rectangle area
//
#define IMAGE_TEXTURE_SCALE_PRESERVE_ASPECTRATIO 0x01		// Bit1: 1=KeepAspectRatio, 0=Stretch the img to fill the rendering rectangle area
#define IMAGE_TEXTURE_POSITION_BOTTOM			 0x02		// Bit2: 1=Image positioned on the bottom of the area, 0=Top of the area
#define IMAGE_TEXTURE_ALPHA_BLEND				 0x04		// Bit3: 1=Use alpha blending if PNG has alpha channel (usually transparent background color), 0=No alpha blending
#define IMAGE_TEXTURE_POSITION_HORIZONTAL_CENTER 0x08		// Bit4: 1=Align the picture horizontally in center position in the drawing rectangle area. 0=Left align
#define IMAGE_TEXTURE_POSITION_VERTICAL_CENTER   0x10		// Bit5: 1=Align the picture vertically in center position in the drawing rectangle area. 0=Top align (unless POSITION_BOTTOM is set)
#define IMAGE_TEXTURE_SCALE_PRESERVE_ORIGSIZE    0x20		// Bit6: 1=Keep the original picture size but optionally center it in drawing rectangle. 0=If drawing rect is defined then scale the picture (keeping aspect ratio or ignoring aspect ratio)

#define IMAGE_TEXTURE_STRETCH_TO_FILL			 0x00  // Default behaviour is to stretch the image to fill the specified draw area
#define IMAGE_TEXTURE_PRESERVE_ASPECTRATIO_TOP	  (IMAGE_TEXTURE_SCALE_PRESERVE_ASPECTRATIO)
#define IMAGE_TEXTURE_PRESERVE_ASPECTRATIO_BOTTOM (IMAGE_TEXTURE_SCALE_PRESERVE_ASPECTRATIO | IMAGE_TEXTURE_POSITION_BOTTOM)
#define IMAGE_TEXTURE_PRESERVE_ASPECTRATIO_CENTER (IMAGE_TEXTURE_SCALE_PRESERVE_ASPECTRATIO | IMAGE_TEXTURE_POSITION_HORIZONTAL_CENTER | IMAGE_TEXTURE_POSITION_VERTICAL_CENTER)

//-----------------------------------------------------------------------------------------------------------------------
//
// Drawing 2D images on D3D9. Code and idea borrowed from http://www.vbforums.com/showthread.php?856957-RESOLVED-DirectX-9-on-C-how-draw-an-image-transparent (Jacob Roman)
// Thanks Jacob for a nice example of drawing 2D bitmap images into D3D9 devices. 
// Borrowed code snippets from the linked page and modified to work with RBR plugin.
//

//#define CUSTOM_VERTEX_FORMAT_3D (D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1)
#define CUSTOM_VERTEX_FORMAT_TEX_2D (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1)
#define CUSTOM_VERTEX_FORMAT_2D     (D3DFVF_XYZRHW | D3DFVF_DIFFUSE)

typedef struct
{
	float x, y, z;
	float rhw;
	DWORD color;
	float tu, tv;
} CUSTOM_VERTEX_TEX_2D;
typedef CUSTOM_VERTEX_TEX_2D* PCUSTOM_VERTEX_TEX_2D;

typedef struct
{
	float x, y, z;
	float rhw;
	DWORD color;
} CUSTOM_VERTEX_2D;
typedef CUSTOM_VERTEX_2D* PCUSTOM_VERTEX_2D;

/*
typedef struct
{
	float x, y, z;
	DWORD color;
	float tu, tv;
} CUSTOM_VERTEX_3D;
typedef CUSTOM_VERTEX_3D PCUSTOM_VERTEX_3D;
*/

typedef struct
{
	IDirect3DTexture9* pTexture;		//   D3D9 texture (fex loaded from a PNG or BMP file)
	CUSTOM_VERTEX_TEX_2D vertexes2D[4];	//   Rectangle vertex at specified pixel position and size
	SIZE imgSize;						//   The original size of the loaded PNG file image
} IMAGE_TEXTURE;						// RBR car preview picture (texture and rect vertex)
typedef IMAGE_TEXTURE* PIMAGE_TEXTURE;

extern HRESULT D3D9CreateRectangleVertexTexBufferFromFile(const LPDIRECT3DDEVICE9 pD3Device, const std::wstring& fileName, float x, float y, float cx, float cy, IMAGE_TEXTURE* pOutImageTexture, DWORD dwFlags = 0); // Create D3D9 textured rect vertex by loading the texture (image) from a file
extern HRESULT D3D9CreateRectangleVertexBuffer(const LPDIRECT3DDEVICE9 pD3Device, float x, float y, float cx, float cy, LPDIRECT3DVERTEXBUFFER9* pOutVertexBuffer);	// Create D3D9 rect vertex (no texture fill)

extern void    D3D9DrawVertexTex2D(const LPDIRECT3DDEVICE9 pD3Device, IDirect3DTexture9* pTexture, const CUSTOM_VERTEX_TEX_2D* vertexes2D); // Draw rectangle vertex using the specified texture
extern void    D3D9DrawVertex2D(const LPDIRECT3DDEVICE9 pD3Device, const LPDIRECT3DVERTEXBUFFER9 pVertexBuffer); // Draw 2D graphical rectangle vertex


//--------------------------------------------------------------------------------------------------------------------------------
//
// DX9 compatible "bitmap to PNG/GIF/DDP file saving" logic without using the legacy DX9 SDK library (which is no longer available in new WinOS SDKs). Thanks Simon.
// https://stackoverflow.com/questions/30021274/capture-screen-using-directx (Simon Mourier)
// Borrowed code snippets about converting DX9 surface to bitmap array and then converting and saving in PNG/DDS file format. Modified to work with RBR plugin.
//
// Contains also code snippets about creating an empty D3D9 surface and then populating it with bitmap data coming from, for example, external PNG/BMP file (Thanks Ivan for an idea of using empty surface at first).
// The code and idea borrowed from https://stackoverflow.com/questions/16172508/how-can-i-create-texture-in-directx-9-without-d3dx9 (Ivan Aksamentov).
// Modified the code snipped to work with RBR plugin.
//

extern HRESULT D3D9SaveScreenToFile(const LPDIRECT3DDEVICE9 pD3Device, const HWND hAppWnd, RECT wndCaptureRect, const std::wstring& outputFileName);
extern HRESULT D3D9LoadTextureFromFile(const LPDIRECT3DDEVICE9 pD3Device, const std::wstring& filePath, IDirect3DTexture9** pOutTexture, SIZE* pOutImageSize); // Populate and load D3D9 texture object from a PNG/GIF/JPEG file

#endif
