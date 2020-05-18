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

#ifndef _D3D9HELPERS_H
#define _D3D9HELPERS_H

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

extern void DebugCloseFile();
extern void DebugClearFile();
extern void DebugPrintFunc(LPCSTR lpszFormat, ...);
extern void DebugPrintFunc(LPCWSTR lpszFormat, ...);

//------------------------------------------------------------------------------------------------

#ifndef SAFE_RELEASE
#define SAFE_RELEASE( p ) if( p ){ p->Release(); p = nullptr; }
#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE( p ) if( p ){ delete p; p = nullptr; }
#endif

extern bool _iStarts_With(std::wstring s1, std::wstring s2, bool s2AlreadyInLowercase = FALSE);			// Case-insensitive starts_with string comparison
//extern bool _iEqual(std::wstring const& s1, std::wstring const& s2, bool s2AlreadyInLowercase = FALSE); // Case-insensitive string comparison

extern void _Trim(std::wstring & s);  // Trim wstring (in-place, modify the s)
//extern std::wstring _TrimCopy(std::wstring s);  // Trim wstring (return new string, the s unmodified)

extern std::wstring _ToWString(const std::string & s);	// Convert std::string to std::wstring
extern std::string  _ToString(const std::wstring & s);  // Convert std::wstring to std:string
extern inline void _ToLowerCase(std::string & s);       // Convert string to lowercase letters (in-place, so the original str in the parameter is converted)
extern inline void _ToLowerCase(std::wstring & s);      // Convert wstring to lowercase letters (in-place)

extern bool _StringToRect(const std::wstring & s, RECT * outRect, const wchar_t separatorChar = L' '); // String in "0 50 200 400" format is converted as RECT struct value 

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

//extern HRESULT D3D9CreateRectangleVertexTex2D(float x, float y, float cx, float cy, CUSTOM_VERTEX_TEX_2D* pOutVertexes2D, int iVertexesSize); // Create D3D9 rect texture vertex to be used with a texture
//extern HRESULT D3D9CreateRectangleVertex2D(float x, float y, float cx, float cy, CUSTOM_VERTEX_2D* pOutVertexes2D, int iVertexesSize); // Create D3D9 rect vertex to be used to draw rectangle (no texture, transparent color)

extern HRESULT D3D9CreateRectangleVertexTexBufferFromFile(const LPDIRECT3DDEVICE9 pD3Device, const std::wstring& fileName, float x, float y, float cx, float cy, IMAGE_TEXTURE* pOutImageTexture); // Create D3D9 textured rect vertex by loading the texture (image) from a file
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
