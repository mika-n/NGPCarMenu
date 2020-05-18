//
// D3D9Helpers. Various helper functions for D3D9 interfaces and string/wstring helper funcs.
//
//
// Copyright 2020, MIKA-N. https://github.com/mika-n
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this softwareand associated
// documentation files(the "Software"), to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense, and to permit persons to whom the Software
// is furnished to do so, subject to the following conditions :
// 
// - The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// - The derived work or derived parts are also "open sourced" and the source code or part of the work using components
//   from this project is made publicly available with modifications to this base work.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS
// OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//

#include "stdafx.h"

//#include <memory>			// make_unique
#include <algorithm>		// transform
#include <vector>			// vector
#include <sstream>			// wstringstream
#include <filesystem>		// fs::exists
#include <fstream>			// std::ifstream and ofstream

#include <wincodec.h>		// IWICxx image funcs
#include <shlwapi.h>		// PathRemoveFileSpec

#include <gdiplus.h>
#include <gdiplusimaging.h>
#pragma comment(lib,"gdiplus.lib")

#include <d3d9.h>
#pragma comment(lib, "d3d9.lib") 

#include "D3D9Helpers.h"

#if USE_DEBUG == 1
#include "NGPCarMenu.h"
#endif

namespace fs = std::filesystem;

//
// Case-insensitive str comparison functions. 
//   s1 and s2 - Strings to compare (case-insensitive)
//   s2AlreadyInLowercase - If true then s2 is already in lowercase letters, so "optimize" the comparison by converting only s1 to lowercase
//   Return TRUE=Equal, FALSE=NotEqual
//
inline bool _iStarts_With(std::wstring s1, std::wstring s2, bool s2AlreadyInLowercase)
{
	transform(s1.begin(), s1.end(), s1.begin(), ::tolower);
	if (!s2AlreadyInLowercase) transform(s2.begin(), s2.end(), s2.begin(), ::tolower);
	return s1._Starts_with(s2);
}

/*
inline bool _iEqual_wchar(wchar_t c1, wchar_t c2)
{
	return std::tolower(c1) == std::tolower(c2);
}

inline bool _iEqualS2Lower_wchar(wchar_t c1, wchar_t c2)
{
	return std::tolower(c1) == c2;
}

inline bool _iEqual(std::wstring const& s1, std::wstring const& s2, bool s2AlreadyInLowercase)
{
	if (s2AlreadyInLowercase)
		return (s1.size() == s2.size() && std::equal(s1.begin(), s1.end(), s2.begin(), _iEqualS2Lower_wchar));
	else
		return (s1.size() == s2.size() && std::equal(s1.begin(), s1.end(), s2.begin(), _iEqual_wchar));
}
*/

//
// Trim/LTrim/RTrim wstring objects (the string obj is trimmed in-place, the original string modified)
//
inline void _LTrim(std::wstring& s)
{
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](WCHAR c) { return !std::isspace(c); }));
}

inline void _RTrim(std::wstring& s)
{
	s.erase(std::find_if(s.rbegin(), s.rend(), [](WCHAR c) { return !std::isspace(c); }).base(), s.end());
}

inline void _Trim(std::wstring& s)
{
	_LTrim(s);
	_RTrim(s);
}

/*
inline std::wstring _LTrimCopy(std::wstring s)
{
	_LTrim(s);
	return s;
}

inline std::wstring _RTrimCopy(std::wstring s)
{
	_RTrim(s);
	return s;
}

inline std::wstring _TrimCopy(std::wstring s)
{
	_Trim(s);
	return s;
}
*/

//
// Convert ASCII char string to Unicode WCHAR string object or vice-versa
//
inline std::wstring _ToWString(const std::string& s)
{
	auto buf = std::make_unique<WCHAR[]>(s.length() + 1);
	mbstowcs_s(nullptr, buf.get(), s.length() + 1, s.c_str(), s.length());
	return std::wstring{ buf.get() };
}


inline std::string _ToString(const std::wstring& s)
{
	auto buf = std::make_unique<char[]>(s.length() + 1);
	wcstombs_s(nullptr, buf.get(), s.length()+1, s.c_str(), s.length());
	return std::string{ buf.get() };
}


inline void _ToLowerCase(std::string& s)
{
	transform(s.begin(), s.end(), s.begin(), ::tolower);
}

inline void _ToLowerCase(std::wstring& s)
{
	transform(s.begin(), s.end(), s.begin(), ::tolower);
}


//
// Split "12 34 54 45" string and populate RECT struct with splitted values
//
bool _StringToRect(const std::wstring& s, RECT* outRect, const wchar_t separatorChar)
{
	std::vector<std::wstring> items;

	if (outRect == nullptr) return false;

	try
	{
		std::wstring item;
		std::wstringstream wss(s);
		while (std::getline(wss, item, separatorChar))
		{
			_Trim(item);
			if (!item.empty())
				items.push_back(item);
		}

		if (items.size() >= 1) outRect->left = std::stoi(items[0]);
		else outRect->left = 0;

		if (items.size() >= 2) outRect->top = std::stoi(items[1]);
		else outRect->top = 0;

		if (items.size() >= 3) outRect->right = std::stoi(items[2]);
		else outRect->right = 0;

		if (items.size() >= 4) outRect->bottom = std::stoi(items[3]);
		else outRect->bottom = 0;
	}
	catch (...)
	{
		// String to int conversion failed. Check input string value.
		return false;
	}

	return (items.size() >= 4);
}


//---------------------------------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------------------------------------------------------
// Debug printout functions. Not used in release build.
//

std::string g_sLogFileName;
FILE* g_fpLogFile = nullptr;

void DebugOpenFile(bool bOverwriteFile = false)
{
	try
	{
		if (g_fpLogFile == nullptr)
		{
			if (g_sLogFileName.empty())
			{
				char  szModulePath[_MAX_PATH];
				::GetModuleFileName(NULL, szModulePath, sizeof(szModulePath));
				::PathRemoveFileSpec(szModulePath);

				g_sLogFileName = szModulePath;
				g_sLogFileName = g_sLogFileName + "\\Plugins\\NGPCarMenu\\NGPCarMenu.log";

#ifndef USE_DEBUG
				// Release build creates always a new empty logfile when the logfile is opened for the first time during a process run
				bOverwriteFile = true;
#endif
			}

			// Overwrite or append the logfile
			fopen_s(&g_fpLogFile, g_sLogFileName.c_str(), (bOverwriteFile ? "w+t" : "a+t"));
		}
	}
	catch (...)
	{
		// Do nothing
	}
}

void DebugCloseFile()
{
	FILE* fpLogFile = g_fpLogFile;
	g_fpLogFile = nullptr;
	if (fpLogFile != nullptr) fclose(fpLogFile);
}

void DebugPrintFunc(LPCSTR lpszFormat, ...)
{
	va_list args;
	va_start(args, lpszFormat);

	SYSTEMTIME t;
	char szTxtTimeStampBuf[32];
	char szTxtBuf[1024];

	try
	{
		GetLocalTime(&t);
		if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &t, "hh:mm:ss ", (LPSTR)szTxtTimeStampBuf, sizeof(szTxtTimeStampBuf) - 1) <= 0)
			szTxtTimeStampBuf[0] = '\0';

		if (_vsnprintf_s(szTxtBuf, sizeof(szTxtBuf) - 1, lpszFormat, args) <= 0)
			szTxtBuf[0] = '\0';

		if (g_fpLogFile == nullptr) 
			DebugOpenFile();

		if (g_fpLogFile)
		{
			fprintf(g_fpLogFile, szTxtTimeStampBuf);
			fprintf(g_fpLogFile, szTxtBuf);
			fprintf(g_fpLogFile, "\n");
			DebugCloseFile();
		}
	}
	catch (...)
	{
		// Do nothing
	}

	va_end(args);
}

void DebugPrintFunc(LPCWSTR lpszFormat, ...)
{
	va_list args;
	va_start(args, lpszFormat);

	SYSTEMTIME t;
	char szTxtTimeStampBuf[32];
	WCHAR szTxtBuf[1024];

	try
	{
		GetLocalTime(&t);
		if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &t, "hh:mm:ss ", (LPSTR)szTxtTimeStampBuf, sizeof(szTxtTimeStampBuf) - 1) <= 0)
			szTxtTimeStampBuf[0] = '\0';

		if (_vsnwprintf_s(szTxtBuf, sizeof(szTxtBuf) - 1, lpszFormat, args) <= 0)
			szTxtBuf[0] = L'\0';

		if (g_fpLogFile == nullptr)
			DebugOpenFile();

		if (g_fpLogFile)
		{
			fprintf(g_fpLogFile, szTxtTimeStampBuf);
			fwprintf(g_fpLogFile, szTxtBuf);
			fprintf(g_fpLogFile, "\n");
			DebugCloseFile();
		}
	}
	catch (...)
	{
		// Do nothing
	}

	va_end(args);
}

void DebugClearFile()
{
	DebugCloseFile();
	DebugOpenFile(true);
}


#if USE_DEBUG == 1

void DebugDumpBuffer(byte* pBuffer, int iPreOffset = 0, int iBytesToDump = 64)
{
	char txtBuffer[64];

	byte* pBuffer2 = pBuffer - iPreOffset;
	for (int idx = 0; idx < iBytesToDump; idx += 8)
	{
		int iAppendPos = 0;
		for (int idx2 = 0; idx2 < 8; idx2++)
			iAppendPos += sprintf_s(txtBuffer + iAppendPos, sizeof(txtBuffer), "%02x ", (byte)pBuffer2[idx + idx2]);

		iAppendPos += sprintf_s(txtBuffer + iAppendPos, sizeof(txtBuffer), " | ");

		for (int idx2 = 0; idx2 < 8; idx2++)
			iAppendPos += sprintf_s(txtBuffer + iAppendPos, sizeof(txtBuffer), "%c", (pBuffer2[idx + idx2] < 32 || pBuffer2[idx + idx2] > 126) ? '.' : pBuffer2[idx + idx2]);

		DebugPrint(txtBuffer);
	}
}

#endif // USE_DEBUG



//---------------------------------------------------------------------------------------------------------------
HRESULT D3D9SavePixelsToFile32bppPBGRA(UINT width, UINT height, UINT stride, LPBYTE pixels, const std::wstring& filePath, const GUID& format, WICPixelFormatGUID pixelFormat);

// Return GDI+ encoder classid.
// GetEncoderClsid method borrowed from Windows documentation (https://docs.microsoft.com/en-us/windows/win32/gdiplus/-gdiplus-retrieving-the-class-identifier-for-an-encoder-use)
int GdiPlusGetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	UINT  num = 0;          // number of image encoders
	UINT  size = 0;         // size of the image encoder array in bytes

	Gdiplus::ImageCodecInfo* pImageCodecInfo = NULL;
	Gdiplus::GetImageEncodersSize(&num, &size);
	if (size == 0)
		return -1;  // Failure

	pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
	if (pImageCodecInfo == NULL)
		return -1;  // Failure

	Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);
	for (UINT j = 0; j < num; ++j)
	{
		if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;  // Success
		}
	}

	free(pImageCodecInfo);
	return -1;  // Failure
}


// Save screenshot file using GDI services instead of DirectX services
HRESULT D3D9SavePixelsToFileGDI(const HWND hAppWnd, RECT wndCaptureRect, const std::wstring& outputFileName, const GUID& format)
{
	HRESULT hResult;

	LPSTR pBuf = nullptr;
	HDC hdcAppWnd = nullptr;
	HDC hdcMemory = nullptr;
	HBITMAP hbmAppWnd = nullptr;
	HANDLE hDIB = INVALID_HANDLE_VALUE;

	BITMAP hAppWndBitmap;
	BITMAPINFOHEADER bmpInfoHeader;
	BITMAPFILEHEADER bmpFileHeader;

	Gdiplus::Bitmap* gdiBitmap = nullptr;

	hResult = S_OK;

	try
	{
		hdcAppWnd = GetDC(hAppWnd);
		if (hdcAppWnd) hdcMemory = CreateCompatibleDC(hdcAppWnd);

		if (!hdcMemory || !hdcAppWnd) hResult = E_INVALIDARG;

		if (SUCCEEDED(hResult) && wndCaptureRect.right == 0 && wndCaptureRect.bottom == 0 && wndCaptureRect.left == 0 && wndCaptureRect.right == 0)
		{
			// Capture rect all zeros. Take a screenshot of the whole D3D9 client window area (ie. RBR game content without WinOS wnd decorations)
			if (GetClientRect(hAppWnd, &wndCaptureRect) == false)
				hResult = E_INVALIDARG;
		}

		if (SUCCEEDED(hResult)) hbmAppWnd = CreateCompatibleBitmap(hdcAppWnd, wndCaptureRect.right - wndCaptureRect.left, wndCaptureRect.bottom - wndCaptureRect.top);
		if (!hbmAppWnd) hResult = E_INVALIDARG;

		HGDIOBJ hgdiResult = SelectObject(hdcMemory, hbmAppWnd);
		if(hgdiResult == nullptr || hgdiResult == HGDI_ERROR)
			hResult = E_INVALIDARG;

		if (!BitBlt(hdcMemory, 0, 0, wndCaptureRect.right - wndCaptureRect.left, wndCaptureRect.bottom - wndCaptureRect.top, hdcAppWnd, wndCaptureRect.left, wndCaptureRect.top, SRCCOPY | CAPTUREBLT))
			hResult = E_INVALIDARG;

		ZeroMemory(&hAppWndBitmap, sizeof(BITMAP));
		if (SUCCEEDED(hResult) && GetObject(hbmAppWnd, sizeof(BITMAP), &hAppWndBitmap) == 0)
			hResult = E_INVALIDARG;

		ZeroMemory(&bmpInfoHeader, sizeof(BITMAPINFOHEADER));
		bmpInfoHeader.biSize        = sizeof(BITMAPINFOHEADER);
		bmpInfoHeader.biWidth       = hAppWndBitmap.bmWidth;
		bmpInfoHeader.biHeight      = hAppWndBitmap.bmHeight;
		bmpInfoHeader.biPlanes      = 1;
		bmpInfoHeader.biBitCount    = hAppWndBitmap.bmBitsPixel; // 32;
		bmpInfoHeader.biCompression = BI_RGB;

		DWORD dwBmpSize = ((hAppWndBitmap.bmWidth * bmpInfoHeader.biBitCount + 31) / 32) * 4 * hAppWndBitmap.bmHeight;

		if (SUCCEEDED(hResult))	hDIB = GlobalAlloc(GHND, dwBmpSize);
		if (hDIB == nullptr) hResult = E_INVALIDARG;

		if (SUCCEEDED(hResult))	pBuf = (LPSTR)GlobalLock(hDIB);
		if (pBuf == nullptr) hResult = E_INVALIDARG;

		if (SUCCEEDED(hResult) && GetDIBits(hdcAppWnd, hbmAppWnd, 0, (UINT)hAppWndBitmap.bmHeight, pBuf, (BITMAPINFO*)&bmpInfoHeader, DIB_RGB_COLORS) == 0)
			hResult = E_INVALIDARG;

		if (SUCCEEDED(hResult))
			// Remove potentially existing output file
			::_wremove(outputFileName.c_str());

		// BMP or PNG output file format
		if (SUCCEEDED(hResult) && format == GUID_ContainerFormatBmp)
		{
			// Save BMP file
			std::ofstream outFile(outputFileName, std::ofstream::binary | std::ios::out);
			if (outFile)
			{
				bmpFileHeader.bfOffBits = (DWORD)sizeof(BITMAPFILEHEADER) + (DWORD)sizeof(BITMAPINFOHEADER);
				bmpFileHeader.bfSize = dwBmpSize + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
				bmpFileHeader.bfType = 0x4D42; // BM file type tag in little endian mode

				outFile.write((LPCSTR)&bmpFileHeader, sizeof(BITMAPFILEHEADER));
				outFile.write((LPCSTR)&bmpInfoHeader, sizeof(BITMAPINFOHEADER));
				outFile.write(pBuf, dwBmpSize);
				outFile.flush();
				outFile.close();
			}
		}
		else if (SUCCEEDED(hResult))
		{
			// Save PNG file
			CLSID encoderClsid;
			Gdiplus::GdiplusStartupInput gdiplusStartupInput;
			ULONG_PTR gdiplusToken = 0;

			try
			{
				Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

				if (gdiplusToken != 0 && GdiPlusGetEncoderClsid(L"image/png", &encoderClsid) >= 0)
				{
					BITMAPINFO binfo;
					ZeroMemory(&binfo, sizeof(binfo));
					binfo.bmiHeader = bmpInfoHeader;
					binfo.bmiHeader.biSize = sizeof(binfo);
					binfo.bmiHeader.biSizeImage = dwBmpSize;

					gdiBitmap = Gdiplus::Bitmap::FromBITMAPINFO(&binfo, pBuf);
					if (gdiBitmap != nullptr)
						gdiBitmap->Save(outputFileName.c_str(), &encoderClsid);
				}
			}
			catch(...)
			{
				LogPrint("ERROR D3D9SavePixelsToFileGDI. %s GDI failed to create file", outputFileName.c_str());
			}

			SAFE_DELETE(gdiBitmap);
			if(gdiplusToken != 0) Gdiplus::GdiplusShutdown(gdiplusToken);
		}
	}
	catch (...)
	{
		hResult = E_INVALIDARG;
		LogPrint("ERROR D3D9SavePixelsToFileGDI. %s failed to create the file", outputFileName.c_str());
	}

	if (hDIB != INVALID_HANDLE_VALUE)
	{
		GlobalUnlock(hDIB);
		GlobalFree(hDIB);
	}

	if (hbmAppWnd) DeleteObject(hbmAppWnd);
	if (hdcMemory) DeleteObject(hdcMemory);
	if (hdcAppWnd) ReleaseDC(hAppWnd, hdcAppWnd);

	return hResult;
}


//
// Save bitmap buffer (taken from D3D9 surface) as PNG file
//
HRESULT D3D9SavePixelsToFile32bppPBGRA(UINT width, UINT height, UINT stride, LPBYTE pixels, const std::wstring& filePath, const GUID& format, WICPixelFormatGUID pixelFormat)
{
	if (!pixels || filePath.empty())
		return E_INVALIDARG;

	HRESULT hResult;
	HRESULT hResultCoInit = E_INVALIDARG;

	IWICImagingFactory* factory = nullptr;
	IWICBitmapEncoder* encoder = nullptr;
	IWICBitmapFrameEncode* frame = nullptr;
	IWICStream* stream = nullptr;
	//GUID pf = GUID_WICPixelFormat32bppPBGRA; // DX9 surface bitmap format
	//GUID pf = GUID_WICPixelFormat24bppBGR; ¨ // GDI bitmap format

	try
	{
		hResultCoInit = CoInitialize(nullptr);

		hResult = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));

		if (SUCCEEDED(hResult)) hResult = factory->CreateStream(&stream);
		
		// Remove potentially existing output file
		if (SUCCEEDED(hResult)) ::_wremove(filePath.c_str());
		
		if (SUCCEEDED(hResult)) hResult = stream->InitializeFromFilename(filePath.c_str(), GENERIC_WRITE);

		if (SUCCEEDED(hResult)) hResult = factory->CreateEncoder(format, nullptr, &encoder);
		if (SUCCEEDED(hResult)) hResult = encoder->Initialize(stream, WICBitmapEncoderNoCache);
		if (SUCCEEDED(hResult)) hResult = encoder->CreateNewFrame(&frame, nullptr);

		if (SUCCEEDED(hResult)) hResult = frame->Initialize(nullptr);
		if (SUCCEEDED(hResult)) hResult = frame->SetSize(width, height);
		if (SUCCEEDED(hResult)) hResult = frame->SetPixelFormat(&pixelFormat);
		if (SUCCEEDED(hResult)) hResult = frame->WritePixels(height, stride, stride * height, pixels);

		if (SUCCEEDED(hResult)) hResult = frame->Commit();
		if (SUCCEEDED(hResult)) hResult = encoder->Commit();
	}
	catch (...)
	{
		// Hmmmm.. Something went wrong...
		hResult = E_INVALIDARG;
	}

	SAFE_RELEASE(stream);
	SAFE_RELEASE(frame);
	SAFE_RELEASE(encoder);
	SAFE_RELEASE(factory);

	if (hResultCoInit == S_OK) CoUninitialize();
	
	return hResult;
}


//
// Take D3D9 screenshot of RBR screen (the whole game screen or specified capture rectangle only) and save it as PNG file.
// If the outputFileName extension defines the file format (.PNG or .BMP)
//
HRESULT D3D9SaveScreenToFile(const LPDIRECT3DDEVICE9 pD3Device, const HWND hAppWnd, RECT wndCaptureRect, const std::wstring& outputFileName)
{
	HRESULT hResult;

	IDirect3D9* d3d = nullptr;
	IDirect3DSurface9* surface = nullptr;
	LPBYTE screenshotBuffer = nullptr;
	D3DDISPLAYMODE mode;
	D3DLOCKED_RECT rc;
	UINT pitch;

	GUID cf; // GUID_ContainerFormatPng or GUID_ContainerFormatBmp

	if (/*pD3Device == nullptr ||*/ outputFileName.empty())
		return E_INVALIDARG;

	try
	{
		cf = GUID_ContainerFormatPng;
		if (fs::path(outputFileName).extension() == ".bmp")
			cf = GUID_ContainerFormatBmp;
		
		// If DX9 device is NULL then save the output image file using GDI bitmap data instead of using DX9 framebuffer data
		if (pD3Device == nullptr)
			return D3D9SavePixelsToFileGDI(hAppWnd, wndCaptureRect, outputFileName, cf);

		// Reading bitmap via DirectX frame buffers
		DebugPrint("D3D9SaveScreenToFile DirectX buffer");

		//d3d = Direct3DCreate9(D3D_SDK_VERSION);
		//d3d->GetAdapterDisplayMode(adapter, &mode)
		pD3Device->GetDirect3D(&d3d);
		hResult = d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &mode);

		if (SUCCEEDED(hResult)) hResult = pD3Device->CreateOffscreenPlainSurface(mode.Width, mode.Height, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH /*D3DPOOL_SYSTEMMEM*/, &surface, nullptr);

		if (SUCCEEDED(hResult) && wndCaptureRect.right == 0 && wndCaptureRect.bottom == 0 && wndCaptureRect.left == 0 && wndCaptureRect.right == 0)
		{
			// Capture rect all zeros. Take a screenshot of the whole D3D9 client window area (ie. RBR game content without WinOS wnd decorations)
			if(!GetClientRect(hAppWnd, &wndCaptureRect))
			//	MapWindowPoints(hAppWnd, NULL, (LPPOINT)&wndCaptureRect, 2);
			//else
				hResult = E_INVALIDARG;
		}

		if (SUCCEEDED(hResult)) MapWindowPoints(hAppWnd, NULL, (LPPOINT)&wndCaptureRect, 2);

		// Compute the required bitmap buffer size and allocate it
		rc.Pitch = 4;
		if (SUCCEEDED(hResult)) hResult = surface->LockRect(&rc, &wndCaptureRect, /*0*/ D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY);
		pitch = rc.Pitch;
		if (SUCCEEDED(hResult)) hResult = surface->UnlockRect();

		if (SUCCEEDED(hResult)) screenshotBuffer = new BYTE[pitch * (wndCaptureRect.bottom - wndCaptureRect.top) /* img height */];
		if(screenshotBuffer == nullptr) hResult = E_INVALIDARG;

		if (SUCCEEDED(hResult)) hResult = pD3Device->GetFrontBufferData(0, surface);

		// DX fullscreen screenshot (Note! Doesn't work, so run RBR in DX9 window mode, RichardBurnsRally.ini fullsreen=false and use Fixup plugin to set fullscreen wnd mode)
		// g_pRBRIDirect3DDevice9->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &surface)

		// Copy frame to byte buffer
		if (SUCCEEDED(hResult)) hResult = surface->LockRect(&rc, &wndCaptureRect, D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY );
		if (SUCCEEDED(hResult)) CopyMemory(screenshotBuffer, rc.pBits, rc.Pitch * (wndCaptureRect.bottom - wndCaptureRect.top) /* img height */);
		if (SUCCEEDED(hResult)) hResult = surface->UnlockRect();

		if (SUCCEEDED(hResult)) hResult = D3D9SavePixelsToFile32bppPBGRA((wndCaptureRect.right - wndCaptureRect.left) /* img width */, (wndCaptureRect.bottom - wndCaptureRect.top) /* img height */, pitch, screenshotBuffer, outputFileName, cf, GUID_WICPixelFormat32bppPBGRA);

		//SavePixelsToFile32bppPBGRA(..., GUID_ContainerFormatDds)
		//SavePixelsToFile32bppPBGRA(..., GUID_ContainerFormatJpeg)
	}
	catch (...)
	{
		// Hmmmm.. Something went wrong...
		hResult = E_INVALIDARG;
	}

	if (screenshotBuffer) delete[] screenshotBuffer;

	SAFE_RELEASE(surface);
	SAFE_RELEASE(d3d);

	return hResult;
}


//
// Load and populate D3D9 texture from a PNG/GIF/JPEG file. 
// Return D3D texture and the original size of the loaded image file (width, height).
//
HRESULT D3D9LoadTextureFromFile(const LPDIRECT3DDEVICE9 pD3Device, const std::wstring& filePath, IDirect3DTexture9** pOutTexture, SIZE* pOutImageSize)
{
	if (filePath.empty() || pD3Device == nullptr || pOutTexture == nullptr || pOutImageSize == nullptr)
		return E_INVALIDARG;

	HRESULT hResult;
	HRESULT hResultCoInit = E_INVALIDARG;

	IWICImagingFactory* factory = nullptr;
	IWICBitmapDecoder* decoder = nullptr;
	IWICBitmapFrameDecode* frame = nullptr;
	IWICStream* stream = nullptr;
	IWICFormatConverter* formatConverter = nullptr;

	LPBYTE imgBuffer = nullptr;
	IDirect3DTexture9* texture = nullptr;

	try
	{
		hResultCoInit = CoInitialize(nullptr);

		hResult = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));

		if (SUCCEEDED(hResult)) hResult = factory->CreateStream(&stream);
		if (SUCCEEDED(hResult)) hResult = stream->InitializeFromFilename(filePath.c_str(), GENERIC_READ);
		if (SUCCEEDED(hResult)) hResult = factory->CreateDecoderFromStream(stream, NULL, WICDecodeMetadataCacheOnLoad, &decoder);
		if (SUCCEEDED(hResult)) hResult = decoder->GetFrame(0, &frame);
		if (SUCCEEDED(hResult)) hResult = factory->CreateFormatConverter(&formatConverter);
		if (SUCCEEDED(hResult)) hResult = formatConverter->Initialize(frame, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);

		if (SUCCEEDED(hResult)) frame->GetSize((UINT*)&pOutImageSize->cx, (UINT*)&pOutImageSize->cy);
		if (SUCCEEDED(hResult))
		{
			imgBuffer = new byte[pOutImageSize->cx * pOutImageSize->cy * 4];
			hResult = formatConverter->CopyPixels(nullptr, pOutImageSize->cx * 4, pOutImageSize->cx * pOutImageSize->cy * 4, imgBuffer);
		}

		// Create empty IDirect3DTexture9 at first and then populate it with bitmap data read from a file
		if (SUCCEEDED(hResult)) hResult = pD3Device->CreateTexture(pOutImageSize->cx, pOutImageSize->cy, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, NULL);
		if (SUCCEEDED(hResult))
		{
			D3DLOCKED_RECT rect;
			hResult = texture->LockRect(0, &rect, 0, D3DLOCK_DISCARD);
			unsigned char* dest = static_cast<unsigned char*>(rect.pBits);
			memcpy(dest, imgBuffer, sizeof(unsigned char) * pOutImageSize->cx * pOutImageSize->cy * 4);
			hResult = texture->UnlockRect(0);
		}
	}
	catch (...)
	{
		// Hmmmm.. Something went wrong...
		hResult = E_INVALIDARG;
	}

	if (SUCCEEDED(hResult))
	{
		// Caller is responsible to release the returned D3D9 texture object
		*pOutTexture = texture;
	}
	else
	{
		// Failed to populate the texture. Release it (if created) and don't return any texture obj to a caller
		SAFE_RELEASE(texture);
	}

	SAFE_RELEASE(stream);
	SAFE_RELEASE(formatConverter);
	SAFE_RELEASE(frame);
	SAFE_RELEASE(decoder);
	SAFE_RELEASE(factory);
	if (imgBuffer != nullptr) delete[] imgBuffer;

	if (hResultCoInit == S_OK) CoUninitialize();

	return hResult;
}


CUSTOM_VERTEX_TEX_2D D3D9CreateCustomVertexTex2D(float x, float y, DWORD color, float tu, float tv)
{
	CUSTOM_VERTEX_TEX_2D vertex;

	vertex.x = x;
	vertex.y = y;
	vertex.z = 0.0f;
	vertex.rhw = 1.0f;
	vertex.color = color;
	vertex.tu = tu;
	vertex.tv = tv;

	return vertex;
}

CUSTOM_VERTEX_2D D3D9CreateCustomVertex2D(float x, float y, DWORD color)
{
	CUSTOM_VERTEX_2D vertex;

	vertex.x = x;
	vertex.y = y;
	vertex.z = 0.0f;
	vertex.rhw = 1.0f;
	vertex.color = color;

	return vertex;
}


//
// Create D3D9 texture rectangle to be drawn at specified x/y location with cx/cy size
//
HRESULT D3D9CreateRectangleVertexTex2D(float x, float y, float cx, float cy, CUSTOM_VERTEX_TEX_2D* pOutVertexes2D, int iVertexesSize)
{
	if (pOutVertexes2D == nullptr || iVertexesSize < sizeof(CUSTOM_VERTEX_TEX_2D) * 4)
		return E_INVALIDARG;

	// Create rectangle vertex of specified size at x/y position
	pOutVertexes2D[0] = D3D9CreateCustomVertexTex2D(x, y,       D3DCOLOR_ARGB(255, 255, 255, 255), 0.0f, 0.0f);
	pOutVertexes2D[1] = D3D9CreateCustomVertexTex2D(x+cx, y,    D3DCOLOR_ARGB(255, 255, 255, 255), 1.0f, 0.0f);
	pOutVertexes2D[2] = D3D9CreateCustomVertexTex2D(x, y+cy,    D3DCOLOR_ARGB(255, 255, 255, 255), 0.0f, 1.0f);
	pOutVertexes2D[3] = D3D9CreateCustomVertexTex2D(cx+x, cy+y, D3DCOLOR_ARGB(255, 255, 255, 255), 1.0f, 1.0f);
	
	return S_OK;
}


/*
CUSTOM_VERTEX_3D Create_Custom_Vertex_3D(float x, float y, float z, DWORD color, float tu, float tv)
{
	CUSTOM_VERTEX_3D vertex;

	vertex.x = x;
	vertex.y = y;
	vertex.z = z;
	vertex.color = color;
	vertex.tu = tu;
	vertex.tv = tv;

	return vertex;
}
*/

//
// Create D3D9 texture rectangle to be drawn at specified x/y location with cx/cy size.
//
HRESULT D3D9CreateRectangleVertex2D(float x, float y, float cx, float cy, CUSTOM_VERTEX_2D* pOutVertexes2D, int iVertexesSize)
{
	if (pOutVertexes2D == nullptr || iVertexesSize < sizeof(CUSTOM_VERTEX_2D) * 4)
		return E_INVALIDARG;

	// Create rectangle vertex of specified size at x/y position (used to highlight screenshot cropping area, so use semi-transparent color)
	pOutVertexes2D[0] = D3D9CreateCustomVertex2D(x, y,			 D3DCOLOR_ARGB(60, 255, 255, 255));
	pOutVertexes2D[1] = D3D9CreateCustomVertex2D(x + cx, y,		 D3DCOLOR_ARGB(60, 255, 255, 255));
	pOutVertexes2D[2] = D3D9CreateCustomVertex2D(x, y + cy,		 D3DCOLOR_ARGB(60, 255, 255, 255));
	pOutVertexes2D[3] = D3D9CreateCustomVertex2D(cx + x, cy + y, D3DCOLOR_ARGB(60, 255, 255, 255));

	return S_OK;
}


//
// Create D3D9 textured vertex buffer with a rect shape where texture is load from an external PNG/DDS/GIF/BMP file
//
HRESULT D3D9CreateRectangleVertexTexBufferFromFile(const LPDIRECT3DDEVICE9 pD3Device, const std::wstring& fileName, float x, float y, float cx, float cy, IMAGE_TEXTURE* pOutImageTexture)
{
	IDirect3DTexture9* pTempTexture = nullptr;

	if (pD3Device == nullptr || pOutImageTexture == nullptr || fileName.empty())
		return E_INVALIDARG;

	// Release existing texture and load a new texture from a file
	SAFE_RELEASE(pOutImageTexture->pTexture);

	if (!fs::exists(fileName))
		return E_INVALIDARG;

	HRESULT hResult = D3D9LoadTextureFromFile(pD3Device, fileName, &pTempTexture, &pOutImageTexture->imgSize);
	if (SUCCEEDED(hResult))
	{
		// If cx/cy is all zero then use the original image size instead of the supplied custom size (re-scaled image rect texture)
		if(cx == 0 && cy == 0)
			hResult = D3D9CreateRectangleVertexTex2D(x, y, (float)pOutImageTexture->imgSize.cx, (float)pOutImageTexture->imgSize.cy, pOutImageTexture->vertexes2D, sizeof(pOutImageTexture->vertexes2D));
		else
			hResult = D3D9CreateRectangleVertexTex2D(x, y, cx, cy, pOutImageTexture->vertexes2D, sizeof(pOutImageTexture->vertexes2D));
	}

	// If image file loading and texture/vertex creation failed then release the texture because it may be in invalid state
	if (SUCCEEDED(hResult))
		pOutImageTexture->pTexture = pTempTexture; // Caller's responsibility to release the texture
	else
		SAFE_RELEASE(pTempTexture);

	return hResult;
}


//
// Create D3D9 2D graphical rectangle vertex buffer (no texture, fill color alpha-transparent or opaque)
// TODO. Add color parameter
HRESULT D3D9CreateRectangleVertexBuffer(const LPDIRECT3DDEVICE9 pD3Device, float x, float y, float cx, float cy, LPDIRECT3DVERTEXBUFFER9* pOutVertexBuffer)
{
	HRESULT hResult;
	LPDIRECT3DVERTEXBUFFER9 pTempVertexBuffer = nullptr;
	CUSTOM_VERTEX_2D custRectVertex[4];

	if (pD3Device == nullptr || pOutVertexBuffer == nullptr)
		return E_INVALIDARG;

	SAFE_RELEASE( (*pOutVertexBuffer) );

	hResult = pD3Device->CreateVertexBuffer(sizeof(custRectVertex), 0, CUSTOM_VERTEX_FORMAT_2D, D3DPOOL_DEFAULT, &pTempVertexBuffer, nullptr);
	if (SUCCEEDED(hResult))
	{		
		void* pData;

		hResult = D3D9CreateRectangleVertex2D(x, y, cx, cy, custRectVertex, sizeof(custRectVertex));

		if (SUCCEEDED(hResult)) hResult = pTempVertexBuffer->Lock(0, 0, (void**)&pData, 0);
		if (SUCCEEDED(hResult)) memcpy(pData, custRectVertex, sizeof(custRectVertex));
		if (SUCCEEDED(hResult)) hResult = pTempVertexBuffer->Unlock();

		if (SUCCEEDED(hResult))
			*pOutVertexBuffer = pTempVertexBuffer; // Caller's responsibility to release the returned vertex buffer object
		else
			SAFE_RELEASE(pTempVertexBuffer);
	}

	return hResult;
}


//
// Draw D3D9 texture vertex (usually shape of rectangle) and use a supplied custom texture
//
void D3D9DrawVertexTex2D(const LPDIRECT3DDEVICE9 pD3Device, IDirect3DTexture9* pTexture, const CUSTOM_VERTEX_TEX_2D* vertexes2D)
{
	// Caller's responsibility to make sure the pD3Device and texture and vertexes2D parameters are not NULL (this method 
	// may be called several times per frame, so no need to do extra paranoid checks each time).

	pD3Device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	pD3Device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	pD3Device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	pD3Device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	pD3Device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	pD3Device->SetTextureStageState(0, D3DTSS_CONSTANT, D3DCOLOR_ARGB(255, 255, 255, 255));
	pD3Device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_CONSTANT);
	//pD3Device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	//pD3Device->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_ARGB(255, 255, 255, 255));
	pD3Device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	pD3Device->SetFVF(CUSTOM_VERTEX_FORMAT_TEX_2D);
	pD3Device->SetTexture(0, pTexture);
	pD3Device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertexes2D, sizeof(CUSTOM_VERTEX_TEX_2D));
}


//
// Draw a vertex without a texture (usually rectangle) using alpa blending transparent color
//
void D3D9DrawVertex2D(const LPDIRECT3DDEVICE9 pD3Device, const LPDIRECT3DVERTEXBUFFER9 pVertexBuffer)
{
	// Caller's responsible to make sure the buffer is not NULL

	pD3Device->SetStreamSource(0, pVertexBuffer, 0, sizeof(CUSTOM_VERTEX_2D));
	pD3Device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	pD3Device->SetFVF(CUSTOM_VERTEX_FORMAT_2D);
	pD3Device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
}

