//
// RBRAPI. RBR-API interface to work with RBR game engine.
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
//----------------------------------------------------------------------------------------------------------------------------------------
//
// The information in this RBR-API interface is a result of painstaking investigations with the RBR game engine and the information published
// in other web pages (especially the following web pages).
//
// - https://web.archive.org/web/20181105131638/http://www.geocities.jp/v317mt/index.html
// - https://suxin.space/notes/rbr-play-replay/
// - http://www.tocaedit.com/2013/05/gran-turismo-guages-for-rbr-v12-update.html
// - http://www.tocaedit.com/2018/03/sources-online.html
// - https://vauhtimurot.blogspot.com/p/in-english.html  (in most part the web page is in Finnish)
// - http://rbr.onlineracing.cz/?setlng=eng
//
// Also, various forum posts by WorkerBee, Kegetys, Racer_S, Mandosukai (and I'm sure many others) here and there have been useful.
// Thank you all for the good work with the RBR game engine (RBR still has the best rally racing physics engine and WorkeBeer's NGP plugin makes it even greater).
//

//#define WIN32_LEAN_AND_MEAN			// Exclude rarely-used stuff from Windows headers
//#include <windows.h>
//#include <assert.h>
#include "stdafx.h"

#include <filesystem>

#include "RBRAPI.h"

#ifdef _DEBUG
#include "D3D9Helpers.h"
#endif


namespace fs = std::filesystem;

//----------------------------------------------------------------------------------------------------------------------------
LPDIRECT3DDEVICE9 g_pRBRIDirect3DDevice9 = nullptr;  // RBR D3D device

HWND			  g_hRBRWnd = nullptr;            // RBR D3D hWnd handle
RECT			  g_rectRBRWnd;					  // RBR wnd top,left,right,bottom coordinates (including windows borders in windowed mode)
RECT			  g_rectRBRWndClient;		      // RBR client area coordinates (resolutionX=right-left, resolutionY=bottom-top)
RECT			  g_rectRBRWndMapped;			  // RBR client area re-mapped to screen points (normally client area top,left is always relative to physical wndRect coordinates)


//----------------------------------------------------------------------------------------------------------------------------
// Helper functions to modify RBR memory locations on the fly
//

// Convert hex char to int value
int char2int(const char input)
{
	if (input >= '0' && input <= '9') return input - '0';
	if (input >= 'A' && input <= 'F') return input - 'A' + 10;
	if (input >= 'a' && input <= 'f') return input - 'a' + 10;
	
	// Should never come here
	assert(1 == 0);
	return 0;
}

// Convert string with HEX number values as byteBuffer (the string must have even number of chars and valid hex chars)
void hexString2byteArray(LPCSTR sHexText, BYTE* byteBuffer)
{
	while (*sHexText && sHexText[1])
	{
		*(byteBuffer++) = char2int(*sHexText) * 0x10 + char2int(sHexText[1]);
		sHexText += 2;
	}
}

// Write buffer to specified memory location
BOOL WriteOpCodeBuffer(const LPVOID writeAddr, const BYTE* buffer, const int iBufLen)
{
	HANDLE hProcess;
	DWORD  dwOldProtectValue;
	BOOL bResult;

	hProcess = OpenProcess(C_PROCESS_READ_WRITE_QUERY, FALSE, GetCurrentProcessId());
	bResult = VirtualProtectEx(hProcess, writeAddr, iBufLen, PAGE_READWRITE, &dwOldProtectValue);
	if (bResult) bResult = WriteProcessMemory(hProcess, writeAddr, buffer, iBufLen, 0);
	CloseHandle(hProcess);

	return bResult;

}

// Read the value of specified memory location and return in buffer
BOOL ReadOpCodeBuffer(const LPVOID readAddr, BYTE* buffer, const int iBufLen)
{
	HANDLE hProcess;
	DWORD  dwOldProtectValue;
	BOOL bResult;

	hProcess = OpenProcess(C_PROCESS_READ_WRITE_QUERY, FALSE, GetCurrentProcessId());
	bResult = VirtualProtectEx(hProcess, readAddr, iBufLen, PAGE_READWRITE, &dwOldProtectValue);
	if (bResult) bResult = ReadProcessMemory(hProcess, readAddr, buffer, iBufLen, 0);
	CloseHandle(hProcess);

	return bResult;

}

BOOL WriteOpCodeHexString(const LPVOID writeAddr, LPCSTR sHexText)
{
	BYTE byteBuffer[64];
	int iLen = strlen(sHexText) / 2;

	assert(iLen <= 64);
	hexString2byteArray(sHexText, byteBuffer);

	return WriteOpCodeBuffer(writeAddr, byteBuffer, iLen);
}

BOOL WriteOpCodeInt32(const LPVOID writeAddr, const __int32 iValue)
{
	BYTEBUFFER_INT32 dataUnionInt32;
	dataUnionInt32.iValue = iValue;

	return WriteOpCodeBuffer(writeAddr, dataUnionInt32.byteBuffer, sizeof(__int32));
}

BOOL ReadOpCodeInt32(const LPVOID readAddr, __int32* iValue)
{
	BYTEBUFFER_INT32 dataUnionInt32;
	if (ReadOpCodeBuffer(readAddr, dataUnionInt32.byteBuffer, sizeof(__int32)))
	{
		*iValue = dataUnionInt32.iValue;
		return TRUE;
	}
	else return FALSE;
}

BOOL WriteOpCodeFloat(const LPVOID writeAddr, const float fValue)
{
	BYTEBUFFER_FLOAT dataUnionFloat;
	dataUnionFloat.fValue = fValue;

	return WriteOpCodeBuffer(writeAddr, dataUnionFloat.byteBuffer, sizeof(float));
}

BOOL ReadOpCodeFloat(const LPVOID readAddr, float* fValue)
{
	BYTEBUFFER_FLOAT dataUnionFloat;
	if (ReadOpCodeBuffer(readAddr, dataUnionFloat.byteBuffer, sizeof(float)))
	{
		*fValue = dataUnionFloat.fValue;
		return TRUE;
	}
	else return FALSE;
}

BOOL WriteOpCodePtr(const LPVOID writeAddr, const LPVOID ptrValue)
{
	BYTEBUFFER_PTR dataUnionPtr;
	dataUnionPtr.ptrValue = ptrValue;

	return WriteOpCodeBuffer(writeAddr, dataUnionPtr.byteBuffer, sizeof(LPVOID));
}

BOOL ReadOpCodePtr(const LPVOID readAddr, LPVOID* ptrValue)
{
	BYTEBUFFER_PTR dataUnionPtr;
	if (ReadOpCodeBuffer(readAddr, dataUnionPtr.byteBuffer, sizeof(LPVOID)))
	{
		*ptrValue = dataUnionPtr.ptrValue;
		return TRUE;
	}
	else return FALSE;
}

BOOL WriteOpCodeByte(const LPVOID writeAddr, const BYTE byteValue)
{
	return WriteOpCodeBuffer(writeAddr, &byteValue, sizeof(BYTE));
}

BOOL ReadOpCodeByte(const LPVOID readAddr, BYTE byteValue)
{
	return ReadOpCodeBuffer(readAddr, &byteValue, sizeof(BYTE));
}


// Map RBR carID to menu order index (ie. internal RBR car slot numbers are not in the same order as "Select car" menu items)
int RBRAPI_MapCarIDToMenuIdx(int carID)
{	
	// Map carID (slot#) to menu order 0..7
	switch (carID)
	{
		case 0: return 6;
		case 1: return 3;
		case 2: return 7;
		case 3: return 1;
		case 4: return 4;
		case 5: return 0;
		case 6: return 2;
		case 7: return 5;
		default: return -1;
	}
}

// Map RBR game point to screen point
void RBRAPI_MapRBRPointToScreenPoint(const float srcX, const float srcY, int* trgX, int* trgY)
{
	if(trgX != nullptr)
		*trgX = static_cast<int>( srcX * (g_rectRBRWndClient.right / 640.0f /*g_pRBRGameConfig->resolutionX*/) );

	if (trgY != nullptr)
		*trgY = static_cast<int>( srcY  * (g_rectRBRWndClient.bottom / 480.0f  /*g_pRBRGameConfig->resolutionY*/ ) );
}

void RBRAPI_MapRBRPointToScreenPoint(const float srcX, const float srcY, float* trgX, float* trgY)
{
	if (trgX != nullptr)
		*trgX = static_cast<float>(srcX * (g_rectRBRWndClient.right / 640.0f /*g_pRBRGameConfig->resolutionX*/));

	if (trgY != nullptr)
		*trgY = static_cast<float>(srcY * (g_rectRBRWndClient.bottom / 480.0f  /*g_pRBRGameConfig->resolutionY*/));
}

BOOL RBRAPI_Replay(const std::string rbrAppFolder, LPCSTR szReplayFileName)
{
	// https://suxin.space/notes/rbr-play-replay/ (Sasha / Suxin)
	// RBR replay function call entry point to call it programmatically. Thanks Sasha for documenting this trick.

	__int32 iNotUsed = 0;
	void** objRBRThis = (void**)0x893634;			  // "Master" RBR object pointer
	tRBRReplay func_RBRReplay = (tRBRReplay)0x4999B0; // RBR replay func entry address

	try
	{
		std::string fullReplayFilePath = rbrAppFolder + "\\Replays\\" + szReplayFileName;

		if (fs::exists(fullReplayFilePath))
		{
			size_t iReplayFileSizeInBytes = (size_t) std::filesystem::file_size(fullReplayFilePath);
			func_RBRReplay(*objRBRThis, szReplayFileName, &iNotUsed, &iNotUsed, iReplayFileSizeInBytes);

			// TODO. Check error status if replay loading failed
			return TRUE;
		}
		else
			// The replay file doesn't exist.
			return FALSE;
	}
	catch (...)
	{
		return FALSE;
	}
}
