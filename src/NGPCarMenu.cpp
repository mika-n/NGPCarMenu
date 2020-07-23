//
// NGPCarMenu.cpp : Defines the entry point for the DLL application.
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

//#define WIN32_LEAN_AND_MEAN			// Exclude rarely-used stuff from Windows headers
//#include <windows.h>
//#include <WinSock2.h>

#include <shlwapi.h>			// PathRemoveFileSpec

#include <filesystem>			// fs::directory_iterator
#include <fstream>				// std::ifstream
#include <chrono>				// std::chrono::steady_clock

#include <locale>				// UTF8 locales
#include <codecvt>

#include <wincodec.h>			// GUID_ContainerFormatPng 

#include "NGPCarMenu.h"

namespace fs = std::filesystem;

//------------------------------------------------------------------------------------------------
// Global "RBR plugin" variables (yes, globals are not always the recommended way to do it, but RBR plugin has only one instance of these variables
// and sometimes class member method and ordinary functions should have an easy access to these variables).
//

BOOL g_bRBRHooksInitialized = FALSE; // TRUE-RBR main memory and DX9 function hooks initialized. Ready to rock! FALSE=Hooks and variables not yet initialized.

tRBRDirectXBeginScene Func_OrigRBRDirectXBeginScene = nullptr;  // Re-routed built-in DX9 RBR function pointers
tRBRDirectXEndScene   Func_OrigRBRDirectXEndScene = nullptr;

#if USE_DEBUG == 1
CD3DFont* g_pFontDebug = nullptr;
#endif 

CD3DFont* g_pFontCarSpecCustom = nullptr;

CNGPCarMenu*         g_pRBRPlugin = nullptr;			// The one and only RBRPlugin instance
PRBRPluginMenuSystem g_pRBRPluginMenuSystem = nullptr;  // Pointer to RBR plugin menu system (for some reason Plugins menu is not part of the std menu arrays)


std::vector<std::unique_ptr<CRBRPluginIntegratorLink>>* g_pRBRPluginIntegratorLinkList = nullptr; // List of custom plugin integration definitions (other plugin can use NGPCarMenu API to draw custom images)
bool g_bNewCustomPluginIntegrations = false;  // TRUE if there are new custom plugin integrations waiting for to be initialized


WCHAR* g_pOrigCarSpecTitleWeight = nullptr;				// The original RBR Weight and Transmission title string values
WCHAR* g_pOrigCarSpecTitleTransmission = nullptr;
WCHAR* g_pOrigCarSpecTitleHorsepower = nullptr;

//
// Car details:
//   rbr\cars\cars.ini					= List of active cars (8 slots). 
//		[Car00-07]						= Car Slot# (not the same as car selection menu order)
//		CarName=carNameKeyword			= Display name of the car and key to carList.ini "name" attribute
//
//   rbr\rbrcit\carlist\carList.ini		= Details of NGP car models and physics
//      [Car_NNN]
//      name=carNameKeyword				= Matching Keyword from cars.ini. RBRCIT should also copy a file with this name to rbr\phystics\<carSubFolder>\ folder.
//      cat=WRC 1.6
//		year=2019
//		weight=1390
//		power=380@6000
//		trans=4WD
//      physics=Ford Fiesta WRC 2019	= Link to rbr\rbrcit\physicas\xxx folder where the file with Physics name has model details (revision, specification date, 3D model)
//										  
//	rbr\physics\<carSubFolder>\common.lsp = Number of gears information (NGP physics)
//

// Note! This table is in menu order and not in internal RBR car slot order. Values are initialized at plugin launch time from NGP physics and RBRCIT carList config files
RBRCarSelectionMenuEntry g_RBRCarSelectionMenuEntry[8] = {
	{ 0x4a0dd9, 0x4a0c59, /* Slot#5 */ "", L"car1", "", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ 0x4a0dc9, 0x4a0c49, /* Slot#3 */ "", L"car2", "", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ 0x4a0de1, 0x4a0c61, /* Slot#6 */ "", L"car3",	"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ 0x4a0db9, 0x4a0c39, /* Slot#1 */ "", L"car4",	"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ 0x4a0dd1, 0x4a0c51, /* Slot#4 */ "", L"car5",	"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ 0x4a0de9, 0x4a0c69, /* Slot#7 */ "", L"car6",	"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ 0x4a0db1, 0x4a0c31, /* Slot#0 */ "", L"car7",	"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ 0x4a0dc1, 0x4a0c41, /* Slot#2 */ "", L"car8",	"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"" }
};


//
// Menu item command ID and names (custom plugin menu). The ID should match the g_RBRPluginMenu array index (0...n)
//
#define C_MENUCMD_CREATEOPTION		0
#define C_MENUCMD_IMAGEOPTION		1
#define C_MENUCMD_RBRTMOPTION		2
#define C_MENUCMD_RELOAD			3
#define C_MENUCMD_CREATE			4

char* g_NGPCarMenu_PluginMenu[5] = {
	 "> Create option"		// CreateOptions
	,"> Image option"		    // ImageOptions
	,"> RBRTM integration option"    // EnableDisableOptions
	,"RELOAD car images"	// Clear cached car images to force re-loading of new images
	,"CREATE car images"	// Create new car images (all or only missing car iamges)
};

// CreateOption menu option names (option values in "Create option")
char* g_NGPCarMenu_CreateOptions[2] = {
	"Only missing car images"
	,"All car images"
};

// Image output option
char* g_NGPCarMenu_ImageOptions[2] = {
	"PNG"
	,"BMP"
};

// RMRTB integration option
char* g_NGPCarMenu_EnableDisableOptions[2] = {
	"Disabled"
	,"Enabled"
};


//-----------------------------------------------------------------------------------------------------------------------------
#if USE_DEBUG == 1
void DebugDumpBufferToScreen(byte* pBuffer, int iPreOffset = 0, int iBytesToDump = 64, int posX = 850, int posY = 1)
{
	WCHAR txtBuffer[64];

	D3DRECT rec = { posX, posY, posX + 440, posY + ((iBytesToDump / 8) * 20) };
	g_pRBRIDirect3DDevice9->Clear(1, &rec, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 50, 50, 50), 0, 0);

	byte* pBuffer2 = pBuffer - iPreOffset;
	for (int idx = 0; idx < iBytesToDump; idx += 8)
	{
		int iAppendPos = 0;

		iAppendPos += swprintf_s(txtBuffer + iAppendPos, COUNT_OF_ITEMS(txtBuffer) - iAppendPos, L"%04x ", idx);
		for (int idx2 = 0; idx2 < 8; idx2++)
			iAppendPos += swprintf_s(txtBuffer + iAppendPos, COUNT_OF_ITEMS(txtBuffer) - iAppendPos, L"%02x ", (byte)pBuffer2[idx + idx2]);

		iAppendPos += swprintf_s(txtBuffer + iAppendPos, COUNT_OF_ITEMS(txtBuffer) - iAppendPos, L" | ");

		for (int idx2 = 0; idx2 < 8; idx2++)
			iAppendPos += swprintf_s(txtBuffer + iAppendPos, COUNT_OF_ITEMS(txtBuffer) - iAppendPos, L"%c", (pBuffer2[idx + idx2] < 32 || pBuffer2[idx + idx2] > 126) ? L'.' : pBuffer2[idx + idx2]);

		g_pFontDebug->DrawText(posX, (idx / 8) * 20 + posY, D3DCOLOR_ARGB(255, 240, 250, 0), txtBuffer, 0);
		//pFontDebug->DrawText(posX, (idx / 8) * 20 + posY, D3DCOLOR_ARGB(255, 240, 250, 0), txtBuffer, 0);
	}
}
#endif


//-----------------------------------------------------------------------------------------------------------------------------
// DLL exported API functions to draw custom images in other plugins. Other plugins can use these API functions to re-use directx 
// services from NGPCarMenu plugin.
//
DWORD APIENTRY API_InitializePluginIntegration(LPCSTR szPluginName)
{
	if (szPluginName == nullptr || szPluginName[0] == '\0')
		return 0;

	if (g_pRBRPluginIntegratorLinkList == nullptr)
		g_pRBRPluginIntegratorLinkList = new std::vector<std::unique_ptr<CRBRPluginIntegratorLink>>();

	CRBRPluginIntegratorLink* pPluginIntegrationLink = nullptr;
	std::string sPluginName = szPluginName;
	_ToLowerCase(sPluginName);

	// Check if the plugin integration with the same name already exists
	for (auto& item : *g_pRBRPluginIntegratorLinkList)
	{
		if(_iEqual(item->m_sCustomPluginName, sPluginName, true))
		{
			pPluginIntegrationLink = item.get();
			break;
		}
	}

	if (pPluginIntegrationLink == nullptr)
	{
		auto newLink = std::make_unique<CRBRPluginIntegratorLink>();
		newLink->m_sCustomPluginName = szPluginName;
		pPluginIntegrationLink = newLink.get();
		g_pRBRPluginIntegratorLinkList->push_back(std::move(newLink));
		
		g_bNewCustomPluginIntegrations = TRUE;
		
		//DebugPrint("API_InitializePluginIntegration. %s", szPluginName);
	}

	return (DWORD)pPluginIntegrationLink;
}

BOOL APIENTRY API_LoadCustomImage(DWORD pluginID, int imageID, LPCSTR szFileName, const POINT* pImagePos, const SIZE* pImageSize, DWORD dwImageFlags)
{
	BOOL bRetValue = TRUE;
	CRBRPluginIntegratorLink* pPluginIntegrationLink = nullptr;

	if (pImagePos == nullptr || pImageSize == nullptr)
		return FALSE;

	for (auto& item : *g_pRBRPluginIntegratorLinkList)
	{
		if ((DWORD)item.get() == pluginID)
		{
			pPluginIntegrationLink = item.get();
			break;
		}
	}

	if (pPluginIntegrationLink == nullptr)
		return FALSE;

	CRBRPluginIntegratorLinkImage* pPluginLinkImage = nullptr;
	for (auto& item : pPluginIntegrationLink->m_imageList)
	{
		if (item->m_iImageID == imageID)
		{
			pPluginLinkImage = item.get();
			break;
		}
	}

	if (pPluginLinkImage == nullptr)
	{
		auto newImage = std::make_unique<CRBRPluginIntegratorLinkImage>();
		newImage->m_iImageID = imageID;
		pPluginLinkImage = newImage.get();
		pPluginIntegrationLink->m_imageList.push_back(std::move(newImage));
	}

	bool bRefeshImage = false;
	std::string sFileName = szFileName;
	_ToLowerCase(sFileName);

	if (pPluginLinkImage->m_sImageFileName.compare(sFileName) != 0)
		bRefeshImage = true;
	else if (pPluginLinkImage->m_imagePos.x != pImagePos->x || pPluginLinkImage->m_imagePos.y != pImagePos->y)
		bRefeshImage = true;
	else if (pPluginLinkImage->m_imageSize.cx != pImageSize->cx || pPluginLinkImage->m_imageSize.cy != pImageSize->cy)
		bRefeshImage = true;
	else if (pPluginLinkImage->m_dwImageFlags != dwImageFlags)
		bRefeshImage = true;

	if (bRefeshImage == true)
	{
		SAFE_RELEASE(pPluginLinkImage->m_imageTexture.pTexture);

		pPluginLinkImage->m_sImageFileName = sFileName;
		pPluginLinkImage->m_imagePos = *pImagePos;
		pPluginLinkImage->m_imageSize = *pImageSize;
		pPluginLinkImage->m_dwImageFlags = dwImageFlags;

		if (szFileName != nullptr)
		{
			try
			{
				HRESULT hResult = D3D9CreateRectangleVertexTexBufferFromFile(g_pRBRIDirect3DDevice9,
					_ToWString(sFileName),
					(float)pImagePos->x, (float)pImagePos->y, (float)pImageSize->cx, (float)pImageSize->cy,
					&pPluginLinkImage->m_imageTexture,
					dwImageFlags);

				// Image not available. Do not try to re-load it again (set cx=-1 to indicate that the image loading failed, so no need to try to re-load it in every frame even when texture is null)
				if (!SUCCEEDED(hResult))
				{
					pPluginLinkImage->m_imageTexture.imgSize.cx = -1;
					SAFE_RELEASE(pPluginLinkImage->m_imageTexture.pTexture);
					bRetValue = FALSE;
				}
			}
			catch (...)
			{
				pPluginLinkImage->m_imageTexture.imgSize.cx = -1;
				SAFE_RELEASE(pPluginLinkImage->m_imageTexture.pTexture);
				bRetValue = FALSE;
			}
		}
	}

	return bRetValue;
}

void APIENTRY API_ShowHideImage(DWORD pluginID, int imageID, bool showImage)
{
	if (g_pRBRPluginIntegratorLinkList == nullptr || pluginID == 0)
		return;

	for (auto& linkItem : *g_pRBRPluginIntegratorLinkList)
	{
		if ((DWORD)linkItem.get() == pluginID)
		{
			for (auto& imageItem : linkItem->m_imageList)
			{
				if (imageItem->m_iImageID == imageID)
				{
					imageItem->m_bShowImage = showImage;
					break;
				}
			}
			break;
		}
	}
}

void APIENTRY API_RemovePluginIntegration(DWORD pluginID)
{
	if (g_pRBRPluginIntegratorLinkList == nullptr || pluginID == 0)
		return;

	for (auto& item : *g_pRBRPluginIntegratorLinkList)
	{
		if ((DWORD)item.get() == pluginID)
		{
			g_pRBRPluginIntegratorLinkList->erase(std::remove(g_pRBRPluginIntegratorLinkList->begin(), g_pRBRPluginIntegratorLinkList->end(), item), g_pRBRPluginIntegratorLinkList->end());
			break;
		}
	}
}

void APIENTRY API_MapRBRPointToScreenPoint(const float srcX, const float srcY, float* trgX, float* trgY)
{
	RBRAPI_MapRBRPointToScreenPoint(srcX, srcY, trgX, trgY);
}


//-----------------------------------------------------------------------------------------------------------------------------
BOOL APIENTRY DllMain( HANDLE hModule,  DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    return TRUE;
}

IPlugin* RBR_CreatePlugin( IRBRGame* pGame )
{
#if USE_DEBUG == 1	
	DebugClearFile();
#endif
	DebugPrint("--------------------------------------------\nRBR_CreatePlugin");

	if (g_pRBRPlugin == nullptr) g_pRBRPlugin = new CNGPCarMenu(pGame);
	//DebugPrint("NGPCarMenu=%08x", g_pRBRPlugin);
	return g_pRBRPlugin;
}

//----------------------------------------------------------------------------------------------------------------------------

// Constructor
CNGPCarMenu::CNGPCarMenu(IRBRGame* pGame)
{
	DebugPrint("Enter CNGPCarMenu.Constructor");

	// Plugin is not fully initialized until GetName method is called by the RBR game for the first time
	this->m_PluginState = T_PLUGINSTATE::PLUGINSTATE_UNINITIALIZED;

	// Init app path value using RBR.EXE executable path
	char szTmpRBRRootDir[_MAX_PATH];
	::GetModuleFileName(NULL, szTmpRBRRootDir, sizeof(szTmpRBRRootDir));
	::PathRemoveFileSpec(szTmpRBRRootDir);

	CNGPCarMenu::m_sRBRRootDir = szTmpRBRRootDir;
	CNGPCarMenu::m_sRBRRootDirW = _ToWString(CNGPCarMenu::m_sRBRRootDir);

	// Init plugin title text with version tag of NGPCarMenu.dll file
	char szTxtBuf[COUNT_OF_ITEMS(C_PLUGIN_TITLE_FORMATSTR) + 32];
	if (sprintf_s(szTxtBuf, COUNT_OF_ITEMS(szTxtBuf) - 1, C_PLUGIN_TITLE_FORMATSTR, GetFileVersionInformationAsString((m_sRBRRootDirW + L"\\Plugins\\" L"" VS_PROJECT_NAME L".dll")).c_str()) <= 0)
		szTxtBuf[0] = '\0';
	m_sPluginTitle = szTxtBuf;

	m_pLangIniFile = nullptr;

	m_iCarMenuNameLen = 0;
	
	m_iCustomReplayCarID = 0;
	m_iCustomReplayState = 0;
	m_iCustomReplayScreenshotCount = 0;
	m_bCustomReplayShowCroppingRect = false;

	m_screenshotCroppingRectVertexBuffer = nullptr;
	ZeroMemory(m_carPreviewTexture, sizeof(m_carPreviewTexture));
	ZeroMemory(m_carRBRTMPreviewTexture, sizeof(m_carRBRTMPreviewTexture));

	ZeroMemory(&m_screenshotCroppingRect, sizeof(m_screenshotCroppingRect));
	ZeroMemory(&m_carSelectLeftBlackBarRect, sizeof(m_carSelectLeftBlackBarRect));
	ZeroMemory(&m_carSelectRightBlackBarRect, sizeof(m_carSelectRightBlackBarRect));
	ZeroMemory(&m_car3DModelInfoPosition, sizeof(m_car3DModelInfoPosition));
	
	// Default is the old behaviour not doing any scaling or stretching (the built-in RBR car selection
	// screen has a different default than RBRTM_CarPictureScale option because of historical reasons. Don't want to break anything in old INI settings)
	m_carPictureScale = -1;
	m_carPictureUseTransparent = true;

	ZeroMemory(&m_carRBRTMPictureRect, sizeof(m_carRBRTMPictureRect));
	ZeroMemory(&m_carRBRTMPictureCropping, sizeof(m_carRBRTMPictureCropping));
	
	// The default is to scale while keeping the aspect ratio and placing the image on the bottom of the rect area
	m_carRBRTMPictureScale = IMAGE_TEXTURE_SCALE_PRESERVE_ASPECTRATIO | IMAGE_TEXTURE_POSITION_BOTTOM;
	m_carRBRTMPictureUseTransparent = true;

	g_pRBRPluginMenuSystem = new RBRPluginMenuSystem;
	ZeroMemory(g_pRBRPluginMenuSystem, sizeof(RBRPluginMenuSystem));

	m_bMenuSelectCarCustomized = false;

	m_pGame = pGame;
	m_iMenuSelection = 0;
	m_iMenuCreateOption = 0;
	m_iMenuImageOption = 0;
	m_iMenuRBRTMOption = 0;

	m_screenshotAPIType = C_SCREENSHOTAPITYPE_DIRECTX;

	m_iRBRTMPluginMenuIdx = 0;		// Index (Nth item) to RBRTM plugin in RBR in-game Plugins menu list (0=Not yet initialized, -1=Initialized but RBRTM plugin missing or wrong version)
	m_pRBRTMPlugin = nullptr;		// Pointer to RBRTM plugin object
	m_pRBRPrevCurrentMenu = nullptr;// Previous "current menu obj" (used in RBRTM integration initialization routine)
	m_bRBRTMPluginActive = false;	// Is RBRTM plugin currently the active custom frontend plugin
	m_iRBRTMCarSelectionType = 0;   // If RBRTM is activated then is it at 1=Shakedown or 2=OnlineTournament car selection menu state

	m_pD3D9RenderStateCache = nullptr; 

	//RefreshSettingsFromPluginINIFile();

	DebugPrint("Exit CNGPCarMenu.Constructor");
}

// Destructor
CNGPCarMenu::~CNGPCarMenu(void)
{
	g_bRBRHooksInitialized = FALSE;
	m_PluginState = T_PLUGINSTATE::PLUGINSTATE_CLOSING;

	DebugPrint("Enter CNGPCarMenu.Destructor");

	if (gtcDirect3DBeginScene != nullptr) delete gtcDirect3DBeginScene;
	if (gtcDirect3DEndScene != nullptr) delete gtcDirect3DEndScene;

#if USE_DEBUG == 1
	if (g_pFontDebug != nullptr) delete g_pFontDebug;
#endif 

	if (g_pFontCarSpecCustom != nullptr) delete g_pFontCarSpecCustom;

	SAFE_RELEASE(m_screenshotCroppingRectVertexBuffer);
	ClearCachedCarPreviewImages();

	SAFE_DELETE(g_pRBRPluginMenuSystem);

	SAFE_DELETE(m_pLangIniFile);

	if (g_pRBRPluginIntegratorLinkList != nullptr)
	{
		g_pRBRPluginIntegratorLinkList->clear();
		SAFE_DELETE(g_pRBRPluginIntegratorLinkList);
	}

	SAFE_DELETE(m_pD3D9RenderStateCache);

	DebugPrint("Exit CNGPCarMenu.Destructor");
	DebugCloseFile();

	if (g_pRBRPlugin == this) g_pRBRPlugin = nullptr;
}

void CNGPCarMenu::ClearCachedCarPreviewImages()
{
	for (int idx = 0; idx < 8; idx++)
	{
		SAFE_RELEASE(m_carPreviewTexture[idx].pTexture);
		SAFE_RELEASE(m_carRBRTMPreviewTexture[idx].pTexture);
	}

	ZeroMemory(m_carPreviewTexture, sizeof(m_carPreviewTexture));
	ZeroMemory(m_carRBRTMPreviewTexture, sizeof(m_carRBRTMPreviewTexture));
}


//-------------------------------------------------------------------------------------------------
// Refresh pluging settings from INI file. This method can be called at any time to refresh values even at plugin runtime.
//
void CNGPCarMenu::RefreshSettingsFromPluginINIFile(bool addMissingSections)
{
	WCHAR szResolutionText[16];

	DebugPrint("Enter CNGPCarMenu.RefreshSettingsFromPluginINIFile");

	int iFileFormat;
	CSimpleIniW  pluginINIFile;
	std::wstring sTextValue;
	std::string  sIniFileName = CNGPCarMenu::m_sRBRRootDir + "\\Plugins\\" VS_PROJECT_NAME ".ini";

	m_sMenuStatusText1.clear();

	try
	{
		// If NGPCarMenu.ini file is missing, but the NGPCarMenu.ini.sample exists then copy the sample as the official version.
		// Upgrade version doesn't overwrite the official (possible user tweaked) NGPCarMenu.ini file.
		if (!fs::exists(sIniFileName) && fs::exists(sIniFileName + ".sample"))
			fs::copy_file(sIniFileName + ".sample", sIniFileName);

		RBRAPI_RefreshWndRect();
		swprintf_s(szResolutionText, COUNT_OF_ITEMS(szResolutionText), L"%dx%d", g_rectRBRWndClient.right, g_rectRBRWndClient.bottom);

		pluginINIFile.LoadFile(sIniFileName.c_str());

		if (addMissingSections)
		{
			//
			// If [XresxYres] INI section is missing then add it now with a default value in ScreenshotCropping option
			//

			bool bSectionFound = false;
			CSimpleIniW::TNamesDepend allSections;
			pluginINIFile.GetAllSections(allSections);
			CSimpleIniW::TNamesDepend::const_iterator iter;
			for (iter = allSections.begin(); iter != allSections.end(); ++iter)
			{
				if (wcsncmp(szResolutionText, iter->pItem, COUNT_OF_ITEMS(szResolutionText)) == 0)
				{
					bSectionFound = true;
					break;
				}
			}

			if (!bSectionFound)
			{
				try
				{
					int posY1, posY2;
					WCHAR szScreenshotCroppingDefaultValue[32];					
					
					RBRAPI_MapRBRPointToScreenPoint(0.0f, 105.0f, nullptr, &posY1);
					RBRAPI_MapRBRPointToScreenPoint(0.0f, 348.0f, nullptr, &posY2);
					swprintf_s(szScreenshotCroppingDefaultValue, COUNT_OF_ITEMS(szScreenshotCroppingDefaultValue), L"%d %d %d %d", 0, posY1, g_rectRBRWndClient.right, posY2);

					if (pluginINIFile.SetValue(szResolutionText, L"ScreenshotCropping", szScreenshotCroppingDefaultValue) >= 0)
						pluginINIFile.SaveFile(sIniFileName.c_str());
					else
						LogPrint("ERROR CNGPCarMenu.RefreshSettingsFromPluginINIFile. Failed to add %s section to %s INI file. You should modify the file in Notepad and to add new resolution INI section", _ToString(std::wstring(szResolutionText)).c_str(), sIniFileName.c_str());
				}
				catch (...)
				{
					LogPrint("ERROR CNGPCarMenu.RefreshSettingsFromPluginINIFile. Failed to add %s section to %s INI file. You should modify the file in Notepad and to add new resolution INI section", _ToString(std::wstring(szResolutionText)).c_str(), sIniFileName.c_str());
				}
			}
		}

		// The latest INI fileFormat is 2
		sTextValue = pluginINIFile.GetValue(L"Default", L"FileFormat", L"2");
		_Trim(sTextValue);
		iFileFormat = std::stoi(sTextValue);

		this->m_screenshotReplayFileName = pluginINIFile.GetValue(L"Default", L"ScreenshotReplay", L"");
		_Trim(this->m_screenshotReplayFileName);

		this->m_screenshotPath = pluginINIFile.GetValue(L"Default", L"ScreenshotPath", L"");
		_Trim(this->m_screenshotPath);

		// FleFormat=1 has a bit different logic in ScreenshotPath value. The "%resolution%" variable value was the default postfix. 
		// The current fileFormat expects to see %% variables in the option value, so append the default %resolution% variable at the end of the existing path value.
		if (iFileFormat < 2 && this->m_screenshotPath.find_first_of(L'%') == std::wstring::npos)
		{
			this->m_screenshotPath += L"\\%resolution%\\%carModelName%.%fileType%";
			pluginINIFile.SetValue(L"Default", L"ScreenshotPath", this->m_screenshotPath.c_str());
		}

		if (this->m_screenshotPath.length() >= 2 && this->m_screenshotPath[0] != '\\' && this->m_screenshotPath[1] != ':' && g_rectRBRWndClient.right > 0 && g_rectRBRWndClient.bottom > 0)
		{
			std::filesystem::path sPath("");
			this->m_screenshotPath = this->m_sRBRRootDirW + L"\\" + this->m_screenshotPath;

			try
			{
				// Create resolution specific folder if the %resolution% keyword is at the end of the path (default behaviour in the original release of NGPCarMenu plugin)
				if (_iEnds_With(this->m_screenshotPath, L"\\%resolution%\\%carmodelname%.%filetype%", true))
				{
					sPath = ReplacePathVariables( _ReplaceStr(this->m_screenshotPath, L"\\%carmodelname%.%filetype%", L""), -1);

					// If the rbr\plugins\NGPCarMenu\preview\<resolution>\ subfolder is missing then create it now
					if (!fs::exists(sPath))
						fs::create_directory(sPath);
				}
			}
			catch (...)
			{
				LogPrint(L"ERROR CNGPCarMenu.RefreshSettingsFromPluginINIFile. %s folder creation failed", sPath.c_str());
				m_sMenuStatusText1 = _ToString(sPath) + " folder creation failed";
			}
		}

		this->m_rbrCITCarListFilePath = pluginINIFile.GetValue(L"Default", L"RBRCITCarListPath", L"");
		_Trim(this->m_rbrCITCarListFilePath);
		if (this->m_rbrCITCarListFilePath.length() >= 2 && this->m_rbrCITCarListFilePath[0] != '\\' && this->m_rbrCITCarListFilePath[1] != ':')
			this->m_rbrCITCarListFilePath = this->m_sRBRRootDirW + L"\\" + this->m_rbrCITCarListFilePath;

		this->m_easyRBRFilePath = pluginINIFile.GetValue(L"Default", L"EASYRBRPath", L"");
		_Trim(this->m_easyRBRFilePath);
		if (this->m_easyRBRFilePath.length() >= 2 && this->m_easyRBRFilePath[0] != '\\' && this->m_easyRBRFilePath[1] != ':')
			this->m_easyRBRFilePath = this->m_sRBRRootDirW + L"\\" + this->m_easyRBRFilePath;


		// TODO: carPosition, camPosition reading from INI file (now the car and cam position is hard-coded in this plugin code)

		sTextValue = pluginINIFile.GetValue(szResolutionText, L"ScreenshotCropping", L"");
		_StringToRect(sTextValue, &this->m_screenshotCroppingRect);

		sTextValue = pluginINIFile.GetValue(szResolutionText, L"CarSelectLeftBlackBar", L"");
		_StringToRect(sTextValue, &this->m_carSelectLeftBlackBarRect);

		sTextValue = pluginINIFile.GetValue(szResolutionText, L"CarSelectRightBlackBar", L"");
		_StringToRect(sTextValue, &this->m_carSelectRightBlackBarRect);

		// Custom location of 3D model into textbox or default location (0,0 = default)
		sTextValue = pluginINIFile.GetValue(szResolutionText, L"Car3DModelInfoPosition", L"");
		_StringToPoint(sTextValue, &this->m_car3DModelInfoPosition);

		// Scale the car picture in a car selection screen (0=no scale, stretch to fill the picture rect area, bit 1 = keep aspect ratio, bit 2 = place the pic to the bottom of the rect area)
		// Default 0 is to stretch the image to fill the drawing rect area (or if the original pic is already in the same size then scaling is not necessary)
		sTextValue = pluginINIFile.GetValue(szResolutionText, L"CarPictureScale", L"-1");
		_Trim(sTextValue);
		if (sTextValue.empty()) this->m_carPictureScale = -1;
		else this->m_carPictureScale = std::stoi(sTextValue);

		sTextValue = pluginINIFile.GetValue(szResolutionText, L"CarPictureUseTransparent", L"1");
		_Trim(sTextValue);
		if (sTextValue.empty()) this->m_carPictureUseTransparent = 1;
		else this->m_carPictureUseTransparent = std::stoi(sTextValue);


		// DirectX (0) or GDI (1) screenshot logic
		sTextValue = pluginINIFile.GetValue(L"Default", L"ScreenshotAPIType", L"0");
		this->m_screenshotAPIType = std::stoi(sTextValue);

		// Screenshot image format option. One of the values in g_RBRPluginMenu_ImageOptions array (the default is the item in the first index)
		this->m_iMenuImageOption = 0;
		sTextValue = pluginINIFile.GetValue(L"Default", L"ScreenshotFileType", L"");
		_Trim(sTextValue);
		for (int idx = 0; idx < COUNT_OF_ITEMS(g_NGPCarMenu_ImageOptions); idx++)
		{
			std::string  sOptionValue;
			std::wstring wsOptionValue;
			sOptionValue = g_NGPCarMenu_ImageOptions[idx];
			wsOptionValue = _ToWString(sOptionValue);

			if (_wcsnicmp(sTextValue.c_str(), wsOptionValue.c_str(), wsOptionValue.length()) == 0)
			{
				this->m_iMenuImageOption = idx;
				break;
			}
		}


		// RBRTM integration properties
		sTextValue = pluginINIFile.GetValue(L"Default", L"RBRTM_Integration", L"1");
		_Trim(sTextValue);
		try
		{
			m_iMenuRBRTMOption = (std::stoi(sTextValue) >= 1 ? 1 : 0);

			// Initialize "RBRTM Tournament" title value in case user has localized it via a RBRTM language ile
			if (m_sRBRTMPluginTitle.empty())
			{
				CSimpleIni rbrINI;
				std::string sRBRTMLangFile;

				rbrINI.SetUnicode(true);
				rbrINI.LoadFile((m_sRBRRootDir + "\\RichardBurnsRally.ini").c_str());
				sRBRTMLangFile = rbrINI.GetValue("RBRTMSettings", "LanguageFile", "");
				if (!sRBRTMLangFile.empty())
				{
					if (sRBRTMLangFile.length() >= 2 && sRBRTMLangFile[0] != '\\' && sRBRTMLangFile[1] != ':')
						sRBRTMLangFile = this->m_sRBRRootDir + "\\" + sRBRTMLangFile;

					if (fs::exists(sRBRTMLangFile))
					{
						CSimpleIni rbrTMLangINI;
						rbrTMLangINI.SetUnicode(true);
						rbrTMLangINI.LoadFile(sRBRTMLangFile.c_str());
						m_sRBRTMPluginTitle = rbrTMLangINI.GetValue("Strings", "1", "");
					}
				}

				if(m_sRBRTMPluginTitle.empty())
					m_sRBRTMPluginTitle = C_RBRTM_PLUGIN_NAME; // The default name of RBRTM
			}

			if (m_iMenuRBRTMOption)
				g_bNewCustomPluginIntegrations = TRUE;  // If RBRTM integration is enabled then signal initialization of "custom plugin integration"
		}
		catch (...)
		{
			LogPrint("WARNING. Invalid value %s in RBRTM_Integration option", sTextValue.c_str());
			m_iMenuRBRTMOption = 0;
		}

		// RBRTM_CarPictureRect 
		sTextValue = pluginINIFile.GetValue(szResolutionText, L"RBRTM_CarPictureRect", L"");
		_StringToRect(sTextValue, &this->m_carRBRTMPictureRect);

		if (m_carRBRTMPictureRect.top == 0 && m_carRBRTMPictureRect.right == 0 && m_carRBRTMPictureRect.left == 0 && m_carRBRTMPictureRect.bottom == 0)
		{
			// Default rectangle area of RBRTM car preview picture if RBRTM_CarPictureRect is not set in INI file
			RBRAPI_MapRBRPointToScreenPoint(000.0f, 244.0f, (int*)&m_carRBRTMPictureRect.left, (int*)&m_carRBRTMPictureRect.top);
			RBRAPI_MapRBRPointToScreenPoint(451.0f, 461.0f, (int*)&m_carRBRTMPictureRect.right, (int*)&m_carRBRTMPictureRect.bottom);

			LogPrint("RBRTM_CarPictureRect value is empty. Using the default value RBRTM_CarPictureRect=%d %d %d %d", m_carRBRTMPictureRect.left, m_carRBRTMPictureRect.top, m_carRBRTMPictureRect.right, m_carRBRTMPictureRect.bottom);
		}

		// RBRTM_CarPictureCropping (not yet implemented)
		sTextValue = pluginINIFile.GetValue(szResolutionText, L"RBRTM_CarPictureCropping", L"");
		_StringToRect(sTextValue, &this->m_carRBRTMPictureCropping);

		// Scale the car picture in RBRTM screen (0=no scale, stretch to fill the picture rect area, bit 1 = keep aspect ratio, bit 2 = place the pic to the bottom of the rect area)
		// Default 3 is to keep the aspect ratio and place the pic to bottom of the rect area.
		sTextValue = pluginINIFile.GetValue(szResolutionText, L"RBRTM_CarPictureScale", L"3");
		_Trim(sTextValue);
		if (sTextValue.empty()) this->m_carRBRTMPictureScale = 3;
		else this->m_carRBRTMPictureScale = std::stoi(sTextValue);

		sTextValue = pluginINIFile.GetValue(szResolutionText, L"RBRTM_CarPictureUseTransparent", L"1");
		_Trim(sTextValue);
		if (sTextValue.empty()) this->m_carRBRTMPictureUseTransparent = 1;
		else this->m_carRBRTMPictureUseTransparent = std::stoi(sTextValue);


		// Optional language file to customize label texts (English by default). 
		// Initialize the language dictionary only once when this method is called for the first time (changes to language translation strings take effect when RBR game is restarted)
		if (m_pLangIniFile == nullptr)
		{
			sTextValue = pluginINIFile.GetValue(L"Default", L"LanguageFile", L"");
			_Trim(sTextValue);
			if (!sTextValue.empty())
			{
				// Append the root of RBR game location if the path is relative value
				if (sTextValue.length() >= 2 && sTextValue[0] != '\\' && sTextValue[1] != ':')
					sTextValue = this->m_sRBRRootDirW + L"\\" + sTextValue;

				if (fs::exists(sTextValue))
				{
					// Load customized language translation strings
					m_pLangIniFile = new CSimpleIniW();
					m_pLangIniFile->SetUnicode(true);
					m_pLangIniFile->LoadFile(sTextValue.c_str());
				}
			}
		}


/*		sTextValue = pluginINIFile.GetValue(szResolutionText, L"RBRTM_Car3DModelInfoPosition", L"");
		_StringToPoint(sTextValue, &this->m_carRBRTM3DModelInfoPosition);
		if (g_pRBRIDirect3DDevice9 != nullptr && m_carRBRTM3DModelInfoPosition.x == 0 && m_carRBRTM3DModelInfoPosition.y == 0)
		{
			// Default X,Y position of car spec info textbox in RBRTM screens
			RBRAPI_MapRBRPointToScreenPoint((float)m_carRBRTMPictureRect.right + 10, (float)m_carRBRTMPictureRect.bottom - (6 * g_pFontCarSpecCustom->GetTextHeight()), (int*)&m_carRBRTM3DModelInfoPosition.x, (int*)&m_carRBRTM3DModelInfoPosition.y);
		}
*/

		// If the existing INI file format is an old version1 then save the file using the new format specifier
		if (iFileFormat < 2)
		{
			pluginINIFile.SetValue(L"Default", L"FileFormat", L"2");
			pluginINIFile.SaveFile(sIniFileName.c_str());
		}


		//
		// Set status text msg (or if the status1 is already set then it must be an error msg set by this method)
		//
		if(m_sMenuStatusText1.empty()) m_sMenuStatusText1 = _ReplaceStr(_ToString(ReplacePathVariables(m_screenshotPath)), "%", "");
		m_sMenuStatusText2 = _ToString(std::wstring(szResolutionText)) + " native resolution detected";
		m_sMenuStatusText3 = (m_easyRBRFilePath.empty() ? "RBRCIT " + _ToString(m_rbrCITCarListFilePath) : "EasyRBR " + _ToString(m_easyRBRFilePath));
	}
	catch (...)
	{
		LogPrint("ERROR CNGPCarMenu.RefreshSettingsFromPluginINIFile. %s INI reading failed", sIniFileName.c_str());
		m_sMenuStatusText1 = sIniFileName + " file access error";
	}
	DebugPrint("Exit CNGPCarMenu.RefreshSettingsFromPluginINIFile");
}

void CNGPCarMenu::SaveSettingsToPluginINIFile()
{
	// Screenshot image format option. One of the values in g_RBRPluginMenu_ImageOptions array (the default is the item in the first index)
	std::string sIniFileName;
	std::string  sOptionValue;
	std::wstring wsOptionValue;
	CSimpleIniW  pluginINIFile;

	try
	{
		sIniFileName = CNGPCarMenu::m_sRBRRootDir + "\\Plugins\\" VS_PROJECT_NAME ".ini";
		pluginINIFile.LoadFile(sIniFileName.c_str());

		sOptionValue = g_NGPCarMenu_ImageOptions[this->m_iMenuImageOption];
		wsOptionValue = _ToWString(sOptionValue);
		pluginINIFile.SetValue(L"Default", L"ScreenshotFileType", wsOptionValue.c_str());

		pluginINIFile.SetValue(L"Default", L"RBRTM_Integration", std::to_wstring(this->m_iMenuRBRTMOption).c_str());

		pluginINIFile.SaveFile(sIniFileName.c_str());
	}
	catch (...)
	{
		LogPrint("ERROR CNGPCarMenu.SaveSettingsToPluginINIFile. %s INI writing failed", sIniFileName.c_str());
		m_sMenuStatusText1 = sIniFileName + " INI writing failed";
	}
}


//-------------------------------------------------------------------------------------------------
// Initialize RBRTM or custom plugin integration
//
int CNGPCarMenu::InitPluginIntegration(const std::string& customPluginName, bool bInitRBRTM)
{
	//DebugPrint("Enter CNGPCarMenu.InitPluginIntegration");
	int iResultPluginIdx = 0;

	try
	{
		// Try to lookup custom (or RBRTM) plugin pointer only when all plugins have been loaded into RBR memory and RBR DX9 device is already initialized
		if (/*m_iRBRTMPluginMenuIdx >= 0 &&*/ /*g_pRBRMenuSystem->currentMenuObj != m_pRBRPrevCurrentMenu &&*/ g_pRBRMenuSystem->currentMenuObj != nullptr)
		{		
			if (g_pRBRMenuSystem->currentMenuObj->numOfItems >= 7 && g_pRBRMenuSystem->currentMenuObj->firstSelectableItemIdx >= 4 && g_pRBRMenuSystem->currentMenuObj->pItemObj != nullptr)
			{
				PRBRPluginMenuItemObj3 pItemArr;

				if (g_pRBRPluginMenuSystem->pluginsMenuObj == nullptr)
				{
					//
					// Identify "Options" and "Plugins" RBR menuObjs
					//
					pItemArr = (PRBRPluginMenuItemObj3)g_pRBRMenuSystem->currentMenuObj->pItemObj[g_pRBRMenuSystem->currentMenuObj->firstSelectableItemIdx - 2];
					if ((DWORD)pItemArr->szMenuTitleID > 0x00001000)
					{
						if (g_pRBRPluginMenuSystem->optionsMenuObj == nullptr && strncmp(pItemArr->szMenuTitleID, "OPT_MAIN", 8) == 0)
							g_pRBRPluginMenuSystem->optionsMenuObj = g_pRBRMenuSystem->currentMenuObj;

						if (wcsncmp(pItemArr->wszMenuTitleID, L"Plugins", 7) == 0)
							g_pRBRPluginMenuSystem->pluginsMenuObj = g_pRBRMenuSystem->currentMenuObj;
					}
				}
				
				if(g_pRBRPluginMenuSystem->pluginsMenuObj != nullptr && ( (bInitRBRTM && m_iRBRTMPluginMenuIdx == 0) || !bInitRBRTM) && !customPluginName.empty())
				{
					//
					// Check if custom plugin is installed. If this is "RBRTM" integration call then check that RBRTM is a supported version
					//
					for (int idx = g_pRBRPluginMenuSystem->pluginsMenuObj->firstSelectableItemIdx; idx < g_pRBRPluginMenuSystem->pluginsMenuObj->numOfItems - 1; idx++)
					{
						pItemArr = (PRBRPluginMenuItemObj3)g_pRBRPluginMenuSystem->pluginsMenuObj->pItemObj[idx];

						//DebugPrint("MenuIdx=%d %s", idx, pItemArr->szItemName);

						if (g_pRBRPluginMenuSystem->pluginsMenuObj->pItemObj[idx] != nullptr && strncmp(pItemArr->szItemName, customPluginName.c_str(), customPluginName.length()) == 0)
						{
							iResultPluginIdx = idx; // Index to custom plugin obj within the "Plugins" menu

							if (bInitRBRTM)
							{
								// If this was a special "RBRTM" integration then check the version of RBRTM before accepting the RBRTM plugin
								std::string sRBRVersion = GetFileVersionInformationAsString(m_sRBRRootDirW + L"\\Plugins\\RBRTM.DLL");
								LogPrint("RBRTM plugin version %s detected", sRBRVersion.c_str());

								if (sRBRVersion.compare("0.8.8.0") == 0)
								{
									// Plugins menu index to RBRTM plugin (ie. RBR may load plugins in random order, so certain plugin is not always in the same position in the Plugins menu list)							
									if (::ReadOpCodePtr((LPVOID)0x1597F128, (LPVOID*)&m_pRBRTMPlugin) && m_pRBRTMPlugin != nullptr)
										m_iRBRTMPluginMenuIdx = idx;
									else
										m_pRBRTMPlugin = nullptr;
								}
							}

							break;
						}
					}

					if ( (bInitRBRTM && m_pRBRTMPlugin != nullptr) || (!bInitRBRTM && iResultPluginIdx > 0) )
					{
						LogPrint("%s plugin integration enabled", customPluginName.c_str());

						if (bInitRBRTM)
							LogPrint("RBRTM plugin is loaded and the version is supported RBRTM v0.88.0");
					}
					else
					{
						LogPrint("%s plugin integration disabled. Plugin not found", customPluginName.c_str());

						if (bInitRBRTM)
						{
							LogPrint("RBRTM plugin is not loaded or the version was not the expected RBRTM v0.88.0");
							m_iRBRTMPluginMenuIdx = -1;
						}
					}										
				}
			}
			else if (g_pRBRPluginMenuSystem->pluginsMenuObj != nullptr && g_pRBRPluginMenuSystem->customPluginMenuObj == nullptr)
			{
				//
				// Identify custom plugin menuObj (ie. menuObj where the parent menu is Plugins menuObj)
				//
				if (g_pRBRMenuSystem->currentMenuObj->prevMenuObj == g_pRBRPluginMenuSystem->pluginsMenuObj)
					g_pRBRPluginMenuSystem->customPluginMenuObj = g_pRBRMenuSystem->currentMenuObj;
			}
		}

		m_pRBRPrevCurrentMenu = g_pRBRMenuSystem->currentMenuObj;
	}
	catch (...)
	{
		iResultPluginIdx = -1;

		LogPrint("ERROR CNGPCarMenu::InitPluginIntegration. Failed to initialize %s integration", customPluginName.c_str());

		if (bInitRBRTM)
		{
			m_pRBRTMPlugin = nullptr;
			m_iRBRTMPluginMenuIdx = -1;
		}
	}

	if(bInitRBRTM)
		iResultPluginIdx = m_iRBRTMPluginMenuIdx;

	//DebugPrint("Exit CNGPCarMenu.InitPluginIntegration");

	return iResultPluginIdx; //  m_pRBRTMPlugin != nullptr;
}


// Initialize all new plugin integrations
int CNGPCarMenu::InitAllNewCustomPluginIntegrations()
{
	int iInitCount = 0;			// Num of plugins initialized
	int iStillWaitingInit = 0;	// Num of plugins still waiting for to be initialized

	if (g_pRBRMenuSystem->currentMenuObj == m_pRBRPrevCurrentMenu)
		return 0;

	try
	{
		if (g_pRBRPlugin->m_iMenuRBRTMOption == 1 && g_pRBRPlugin->m_iRBRTMPluginMenuIdx == 0)
			// RBRTM integration is enabled, but the RBRTM linking is not yet initialized. Do it now when RBR has already loaded all custom plugins.
			// If initialization succeeds then m_iRBRTMPluginMenuIdx > 0 and m_pRBRTMPlugin != NULL and g_pRBRPluginMenuSystem->customPluginMenuObj != NULL, otherwise PluginMenuIdx=-1
			if (InitPluginIntegration(g_pRBRPlugin->m_sRBRTMPluginTitle, TRUE) != 0)
				iInitCount++;
			else
				iStillWaitingInit++;

		if (g_pRBRPluginIntegratorLinkList != nullptr)
		{
			// Go through all custom plugin integration links and initialize those which are not yet initialized
			for (auto& item : *g_pRBRPluginIntegratorLinkList)
			{
				//DebugPrint("CNGPCarMenu::InitAllNewCustomPluginIntegrations. PluginIntegration=%s Idx=%d", item->m_sCustomPluginName.c_str(), item->m_iCustomPluginMenuIdx);

				if (item->m_iCustomPluginMenuIdx == 0)
				{
					// Plugin integration not yet initialized. Try to do it now.
					// Return value 0=still waiting to be initialized, >0=successully initialied, <0=init failed ignore the plugin integration
					item->m_iCustomPluginMenuIdx = InitPluginIntegration(item->m_sCustomPluginName, FALSE);

					if (item->m_iCustomPluginMenuIdx != 0)
						iInitCount++;
					else
						iStillWaitingInit++;
				}
			}
		}

		// If custom plugin integration is not yet fully initialized then try to initialize it now even when all integrated custom plugins have been found
		if (iInitCount == 0 && iStillWaitingInit == 0 && g_pRBRPluginMenuSystem->customPluginMenuObj == nullptr)
			InitPluginIntegration("", FALSE);

		// If there are no more plugins waiting for to be initialized then set the flag value to FALSE to avoid repeated (unncessary) initialization calls
		if(iStillWaitingInit == 0 && g_pRBRPluginMenuSystem->customPluginMenuObj != nullptr)
			g_bNewCustomPluginIntegrations = FALSE;
	}
	catch (...)
	{
		g_bNewCustomPluginIntegrations = FALSE;
	}

	return iInitCount;
}


//-------------------------------------------------------------------------------------------------
// RBRCIT support: Init car spec data by reading NGP plugin and RBRCIT config files
// - Step1: rbr\Physics\<c_xsara h_accent m_lancer mg_zr p_206 s_i2000 s_i2003 t_coroll>\ RBR folder and find the file without extensions and containing keywords revision/specification date/3D model
//   - The car model name is the same as the filename of the physics description file in this folder (or should this be the value in rbr\Cars\Cars.ini file?)
//   - Physics revision / specification date / 3D model / plus optional 4th text line
//   - If the RBRCIT/NGP description file is missing in this folder then look for the car model name in RBR\cars\cars.ini file (CarName attribute, original cars)
// - Step2: Read rbr\Physics\<car subfolder>\Common.lsp file 
//   - "NumberOfGears" tag
// - Step3: Use the car model name (step1) as a key to RBRCIT/carlist/carList.ini file NAME=xxxx line
//   - FIA category (cat) / year / weight / power / trans
//
void CNGPCarMenu::InitCarSpecData_RBRCIT()
{
	DebugPrint("Enter CNGPCarMenu.InitCarSpecData_RBRCIT");

	CSimpleIniW ngpCarListINIFile;
	CSimpleIniW customCarSpecsINIFile;

	CSimpleIniW stockCarListINIFile;
	std::wstring sStockCarModelName;

	static const char* szPhysicsCarFolder[8] = { "s_i2003" , "m_lancer", "t_coroll", "h_accent", "p_206", "s_i2000", "c_xsara", "mg_zr" };

	std::string sPath;
	sPath.reserve(_MAX_PATH);

	try
	{
		int iNumOfGears;

		if (fs::exists(m_rbrCITCarListFilePath))
			ngpCarListINIFile.LoadFile(m_rbrCITCarListFilePath.c_str());
		else
			// Add warning about missing RBRCIT carList.ini file
			wcsncpy_s(g_RBRCarSelectionMenuEntry[0].wszCarPhysicsCustomTxt, (m_rbrCITCarListFilePath + L" missing. Cannot show car specs").c_str(), COUNT_OF_ITEMS(g_RBRCarSelectionMenuEntry[0].wszCarPhysicsCustomTxt));

		// Load std RBR cars list and custom carSpec file (fex original car specs are set here). These are used if the phystics\<CarFolder> doesn't have NGP car description file
		customCarSpecsINIFile.LoadFile((m_sRBRRootDirW + L"\\Plugins\\" L"" VS_PROJECT_NAME L"\\CustomCarSpecs.ini").c_str());
		stockCarListINIFile.LoadFile((m_sRBRRootDirW + L"\\cars\\Cars.ini").c_str());

		// The loop uses menu idx order, not in car slot# idx order
		for (int idx = 0; idx < 8; idx++)
		{
			iNumOfGears = -1;
			sPath = CNGPCarMenu::m_sRBRRootDir + "\\physics\\" + szPhysicsCarFolder[idx];

			if (!InitCarSpecDataFromPhysicsFile(sPath, &g_RBRCarSelectionMenuEntry[idx], &iNumOfGears))
			{
				// If the Physics\<carFolder> doesn't have the NGP car description file then use rbr\cars\Cars.ini file to lookup the car model name (the car is probably original model)
				sStockCarModelName = InitCarModelNameFromCarsFile(&stockCarListINIFile, idx);
				if(!sStockCarModelName.empty())
					wcsncpy_s(g_RBRCarSelectionMenuEntry[idx].wszCarModel, sStockCarModelName.c_str(), COUNT_OF_ITEMS(g_RBRCarSelectionMenuEntry[idx].wszCarModel));
			}

			// NGP car model or stock car model lookup to RBCIT/carList/carList.ini file. If the car specs are missing then try to use NGPCarMenu custom carspec INI file
			if (!InitCarSpecDataFromNGPFile(&ngpCarListINIFile, &g_RBRCarSelectionMenuEntry[idx], iNumOfGears))
				if (InitCarSpecDataFromNGPFile(&customCarSpecsINIFile, &g_RBRCarSelectionMenuEntry[idx], iNumOfGears))
					g_RBRCarSelectionMenuEntry[idx].wszCarPhysics3DModel[0] = L'\0'; // Clear warning about missing NGP car desc file
		}
	}
	catch (const fs::filesystem_error& ex)
	{
		LogPrint("ERROR CNGPCarMenu.InitCarSpecData_RBRCIT. %s %s", m_rbrCITCarListFilePath.c_str(), ex.what());
	}
	catch (...)
	{
		// Hmmm... Something went wrong
		LogPrint("ERROR CNGPCarMenu.InitCarSpecData_RBRCIT. %s reading failed", m_rbrCITCarListFilePath.c_str());
	}

	DebugPrint("Exit CNGPCarMenu.InitCarSpecData_RBRCIT");
}


//-------------------------------------------------------------------------------------------------
// EasyRBR support: Init car spec data by reading NGP plugin and EasyRBR config files
// - Step1: rbr\Cars\cars.ini and [Car00..Car07] slots
//   - CarName attribute within the Cars.ini is the car model name
// - Step2: EasyRBR\easyrbr.ini file and read [CarSlot00..07] slots
//   - Internal EasyRBR configuration name of the car model (usually the same as carModel in step1 but not always)
//   - Livery name (used also as a keyword to easyrbr\cars\_LiveriesList.ini file)
//   - Physics=NGP5 NGP6 keyword (used as a reference keyword to physics subfolder)
// - Step3: EasyRBR\Cars\<car model from step1>\physics\<physics from step2>\ and the file without extensions and containing keywords revision/specification date/3D model
//   - Physics revision / specification date / 3D model / plus optional 4th text line
// - Step4: EasyRBR\Cars\<car model from step1>\physics\<physics from step2>\common.lsp
//   - "NumberOfGears" tag
// - Step5: EasyRBR\Cars\_LiveriesList.ini to read livery credit 
//   - Find [<car model from step1>] block
//   - Find LivX=<livery name from step2> attribute
//   - Use CreditsX= value as a livery credit
//   - If the stock LiveriesList didn't have the livery name then try to find the credit text from EasyRBR\cars\<car model from step1>\liveries\_userliveries\UserLivery.ini file
// - Step6: Use the car model name (step1) as a key to RBRCIT/carlist/carList.ini file NAME=xxxx line
//   - FIA category (cat) / year / weight / power / trans
//   - Where is this information in EasyRBR setup?
//   - Download from http://ly-racing.de/ngp/tools/rbrcit/carlist/carList.ini or install RBRCIT to get this file
//

void CNGPCarMenu::InitCarSpecData_EASYRBR()
{
	DebugPrint("Enter CNGPCarMenu.InitCarSpecData_EASYRBR");

	std::wstring sTextValue;

	CSimpleIniW carsINIFile;
	CSimpleIniW ngpCarListINIFile;
	CSimpleIniW customCarSpecsINIFile;

	CSimpleIniW stockCarListINIFile;
	std::wstring sStockCarModelName;

	CSimpleIniW  easyRBRINIFile;
	CSimpleIniW easyRBRLiveriesList;
	CSimpleIniW easyRBRUserLiveriesList;

	std::wstring sEasyRBRCarModelName;  // The display car name (StockCarModelName) is not always the same as the car folder name used in EasyRBR. This is the internal EasyRBR model name
	std::wstring sEasyRBRLiveryName;    // Name of the livery
	std::wstring sEasyRBRLiveryCredit;  // Credits (author) of the livery
	std::wstring sEasyRBRPhysicsName;   // NGP physics used in EasyRBR setups (NGP5 or NGP6)

	std::string sPath;
	sPath.reserve(_MAX_PATH);

	try
	{
		int iNumOfGears;

		if (fs::exists(m_rbrCITCarListFilePath))
			ngpCarListINIFile.LoadFile(m_rbrCITCarListFilePath.c_str());
		else
			// Add warning about missing RBRCIT carList.ini file
			wcsncpy_s(g_RBRCarSelectionMenuEntry[0].wszCarPhysicsCustomTxt, (m_rbrCITCarListFilePath + L" missing. Cannot show car specs").c_str(), COUNT_OF_ITEMS(g_RBRCarSelectionMenuEntry[0].wszCarPhysicsCustomTxt));

		// Load std RBR cars list and custom carSpec file (fex original car specs are set here). These are used if the phystics\<CarFolder> doesn't have NGP car description file
		customCarSpecsINIFile.LoadFile((m_sRBRRootDirW + L"\\Plugins\\" L"" VS_PROJECT_NAME L"\\CustomCarSpecs.ini").c_str());
		stockCarListINIFile.LoadFile((m_sRBRRootDirW + L"\\cars\\Cars.ini").c_str());

		// Load EasyRBR ini
		if (fs::exists(m_easyRBRFilePath))
			easyRBRINIFile.LoadFile(m_easyRBRFilePath.c_str());
		else
			// Add warning about missing RBRCIT carList.ini file
			wcsncpy_s(g_RBRCarSelectionMenuEntry[0].wszCarPhysicsCustomTxt, (m_easyRBRFilePath + L" missing. Cannot show car specs").c_str(), COUNT_OF_ITEMS(g_RBRCarSelectionMenuEntry[0].wszCarPhysicsCustomTxt));

		// Load EasyRBR Liveries list (credit text for a livery)
		if (fs::exists(m_easyRBRFilePath + L"\\..\\cars\\__LiveriesList.ini"))
			easyRBRLiveriesList.LoadFile((m_easyRBRFilePath + L"\\..\\cars\\__LiveriesList.ini").c_str());

		// EasyRBR physics attribute is used as a folder name when looking for car details
		sEasyRBRPhysicsName = easyRBRINIFile.GetValue(L"General", L"Physics", L"");
		_Trim(sEasyRBRPhysicsName);


		// The loop uses menu idx order, not in car slot# idx order
		for (int idx = 0; idx < 8; idx++)
		{
			WCHAR wszEasyRBRCarSlot[12];

			iNumOfGears = -1;

			sStockCarModelName = InitCarModelNameFromCarsFile(&stockCarListINIFile, idx);
			if (!sStockCarModelName.empty())
				wcsncpy_s(g_RBRCarSelectionMenuEntry[idx].wszCarModel, sStockCarModelName.c_str(), COUNT_OF_ITEMS(g_RBRCarSelectionMenuEntry[idx].wszCarModel));

			// Read the internal car config name and livery name of the 3D model in a car slot
			swprintf_s(wszEasyRBRCarSlot, COUNT_OF_ITEMS(wszEasyRBRCarSlot), L"CarSlot0%d", ::RBRAPI_MapMenuIdxToCarID(idx));
			sEasyRBRLiveryName = easyRBRINIFile.GetValue(wszEasyRBRCarSlot, L"Livery", L"");
			_Trim(sEasyRBRLiveryName);
			if (!sEasyRBRLiveryName.empty())
			{
				// EasyRBR encodes unicode chars as \xXX hex values. Decode those back to "normal" chars
				sTextValue = std::wstring(GetLangStr(L"livery")) + L" " + ::_DecodeUtf8String(sEasyRBRLiveryName);
				wcsncpy_s(g_RBRCarSelectionMenuEntry[idx].wszCarPhysicsLivery, sTextValue.c_str(), COUNT_OF_ITEMS(g_RBRCarSelectionMenuEntry[idx].wszCarPhysicsLivery));
			}

			// Read the EasyRBR internal car model name (not always the same as display car model name). This value is used as a lookup key to car specific EasyRBR config files
			sEasyRBRCarModelName = easyRBRINIFile.GetValue(wszEasyRBRCarSlot, L"EasyRbrCarName", L"");
			_Trim(sEasyRBRCarModelName);
			

			// Read NGP model details from the NGP description file
			sPath = _ToString(m_easyRBRFilePath + L"\\..\\Cars\\" + sEasyRBRCarModelName + L"\\physics\\" + sEasyRBRPhysicsName);
			InitCarSpecDataFromPhysicsFile(sPath, &g_RBRCarSelectionMenuEntry[idx], &iNumOfGears);

			// Read car spec details from NGP carList.ini file or from the NGPCarMenu custom INI file (the custom file contains specs for original cars also)
			if (!InitCarSpecDataFromNGPFile(&ngpCarListINIFile, &g_RBRCarSelectionMenuEntry[idx], iNumOfGears))
				if (InitCarSpecDataFromNGPFile(&customCarSpecsINIFile, &g_RBRCarSelectionMenuEntry[idx], iNumOfGears))
					g_RBRCarSelectionMenuEntry[idx].wszCarPhysics3DModel[0] = L'\0'; // Clear warning about missing NGP car desc file


			// Read liveries credit text from EasyRBR config files. If the livery was not found from normal liveries then try to look for credit text fom UserLivery.ini file			
			if (!sEasyRBRLiveryName.empty())
			{
				sTextValue.clear();

				int iLiveryCount = std::stoi(easyRBRLiveriesList.GetValue(sEasyRBRCarModelName.c_str(), L"Number", L"0"));
				for (int iLiveryIdx = 1; iLiveryIdx <= iLiveryCount; iLiveryIdx++)
				{
					swprintf_s(wszEasyRBRCarSlot, COUNT_OF_ITEMS(wszEasyRBRCarSlot), L"Liv%d", iLiveryIdx);
					sTextValue = easyRBRLiveriesList.GetValue(sEasyRBRCarModelName.c_str(), wszEasyRBRCarSlot, L"");
					_Trim(sTextValue);

					//DebugPrint(L"%s=%s == %s", wszEasyRBRCarSlot, sTextValue.c_str(), sEasyRBRLiveryName.c_str());

					if (sTextValue.compare(sEasyRBRLiveryName) == 0)
					{
						swprintf_s(wszEasyRBRCarSlot, COUNT_OF_ITEMS(wszEasyRBRCarSlot), L"Credits%d", iLiveryIdx);
						sTextValue = ::_DecodeUtf8String(easyRBRLiveriesList.GetValue(sEasyRBRCarModelName.c_str(), wszEasyRBRCarSlot, L""));
						_Trim(sTextValue);
						break;
					}

					sTextValue.clear();
				}

				sPath = _ToString(m_easyRBRFilePath + L"\\..\\Cars\\" + sEasyRBRCarModelName + L"\\liveries\\_userliveries\\UserLivery.ini");
				if (sTextValue.empty() && fs::exists(sPath))
				{
					// Livery was not found from the stock EasyRBR livery list. Try to find the livery credit text from a custom user liveries
					easyRBRUserLiveriesList.LoadFile(sPath.c_str());
					
					iLiveryCount = std::stoi(easyRBRUserLiveriesList.GetValue(L"General", L"Number", L"0"));
					sTextValue.clear();
					for (int iLiveryIdx = 1; iLiveryIdx <= iLiveryCount; iLiveryIdx++)
					{
						swprintf_s(wszEasyRBRCarSlot, COUNT_OF_ITEMS(wszEasyRBRCarSlot), L"User_%d", iLiveryIdx);
						sTextValue = easyRBRUserLiveriesList.GetValue(wszEasyRBRCarSlot, L"LiveryName", L"");
						_Trim(sTextValue);
						if (sTextValue.compare(sEasyRBRLiveryName) == 0)
						{
							sTextValue = ::_DecodeUtf8String(easyRBRUserLiveriesList.GetValue(wszEasyRBRCarSlot, L"Credits", L""));
							_Trim(sTextValue);
							break;
						}
						
						sTextValue.clear();
					}
				}

				// Append livery credit text (author) to the existing livery description text
				if (!sTextValue.empty())
				{
					sTextValue = std::wstring(g_RBRCarSelectionMenuEntry[idx].wszCarPhysicsLivery) + L" by " + sTextValue;
					wcsncpy_s(g_RBRCarSelectionMenuEntry[idx].wszCarPhysicsLivery, sTextValue.c_str(), COUNT_OF_ITEMS(g_RBRCarSelectionMenuEntry[idx].wszCarPhysicsLivery));
				}
			}
		}
	}
	catch (const fs::filesystem_error& ex)
	{
		LogPrint("ERROR CNGPCarMenu.InitCarSpecData_EASYRBR. %s %s", m_easyRBRFilePath.c_str(), ex.what());
	}
	catch (...)
	{
		// Hmmm... Something went wrong
		LogPrint("ERROR CNGPCarMenu.InitCarSpecData_EASYRBR. %s reading failed", m_easyRBRFilePath.c_str());
	}

	DebugPrint("Exit CNGPCarMenu.InitCarSpecData_EASYRBR");
}


//-------------------------------------------------------------------------------------------------
// AudioFMOD support: Init the custom name of the installed AudioFMOD sound bank
// Step 1: Read rbr\AudioFMOD\AudioFMOD.ini and bankName attribute in [CarXX] INI section
//
void CNGPCarMenu::InitCarSpecAudio()
{
	DebugPrint("Enter CNGPCarMenu.InitCarSpecAudio");

	CSimpleIniW audioFMODINIFile;
	std::string sPath;
	sPath = m_sRBRRootDir + "\\AudioFMOD\\AudioFMOD.ini";

	try
	{
		if (fs::exists(sPath))
		{
			WCHAR wszCarINISection[8];
			std::wstring sTextValue;

			audioFMODINIFile.LoadFile(sPath.c_str());

			sTextValue = audioFMODINIFile.GetValue(L"Settings", L"enableFMOD", L"");
			_Trim(sTextValue);
			if (sTextValue.compare(L"1") == 0 || _iStarts_With(sTextValue, L"true", true))
			{
				// The loop uses menu idx order, not in car slot# idx order
				for (int idx = 0; idx < 8; idx++)
				{
					swprintf_s(wszCarINISection, COUNT_OF_ITEMS(wszCarINISection), L"Car0%d", ::RBRAPI_MapMenuIdxToCarID(idx));
					sTextValue = audioFMODINIFile.GetValue(wszCarINISection, L"bankName", L"");
					_Trim(sTextValue);
					if (!sTextValue.empty())
						wcsncpy_s(g_RBRCarSelectionMenuEntry[idx].wszCarFMODBank, (std::wstring(g_pRBRPlugin->GetLangStr(L"FMOD")) + L" " + sTextValue).c_str(), COUNT_OF_ITEMS(g_RBRCarSelectionMenuEntry[idx].wszCarFMODBank));
						
				}
			}
		}
	}
	catch (const fs::filesystem_error& ex)
	{
		LogPrint("ERROR CNGPCarMenu.InitCarSpecAudio. %s %s", sPath.c_str(), ex.what());
	}
	catch (...)
	{
		// Hmmm... Something went wrong
		LogPrint("ERROR CNGPCarMenu.InitCarSpecAudio. %s reading failed", sPath.c_str());
	}

	DebugPrint("Exit CNGPCarMenu.InitCarSpecAudio");
}


//-------------------------------------------------------------------------------------------------
// Read the car model name from RBR cars\Cars.ini file and pretify the name if necessary.
// 
std::wstring CNGPCarMenu::InitCarModelNameFromCarsFile(CSimpleIniW* stockCarListINIFile, int menuIdx)
{
	std::wstring sStockCarModelName;
	WCHAR wszStockCarINISection[8];

	try
	{
		swprintf_s(wszStockCarINISection, COUNT_OF_ITEMS(wszStockCarINISection), L"Car0%d", ::RBRAPI_MapMenuIdxToCarID(menuIdx));
		sStockCarModelName = stockCarListINIFile->GetValue(wszStockCarINISection, L"CarName", L"");
		_Trim(sStockCarModelName);
		sStockCarModelName = _RemoveEnclosingChar(sStockCarModelName, L'"', false);

		if (sStockCarModelName.length() >= 4)
		{
			// If the car name ends in "xxx #2]" tag then remove it as unecessary trailing tag (some cars like "Renault Twingo R1 #2]" or "Open ADAM R2 #2]" has this weird tag in NGP car name value in Cars.ini file)
			if (sStockCarModelName[sStockCarModelName.length() - 3] == L'#' && sStockCarModelName[sStockCarModelName.length() - 1] == L']')
			{
				sStockCarModelName.erase(sStockCarModelName.length() - 3);
				_Trim(sStockCarModelName);
			}

			// If the car name has "xxx (2)" type of trailing tag (original car names set by RBRCIT in Cars.ini has the slot number) then remove that unnecessary tag
			if (sStockCarModelName[sStockCarModelName.length() - 3] == L'(' && sStockCarModelName[sStockCarModelName.length() - 1] == L')')
			{
				sStockCarModelName.erase(sStockCarModelName.length() - 3);
				_Trim(sStockCarModelName);
			}
		}
	}
	catch (const fs::filesystem_error& ex)
	{
		LogPrint("ERROR CNGPCarMenu.InitCarModelNameFromCarsFile. %d %s", menuIdx, ex.what());
	}
	catch (...)
	{
		// Hmmm... Something went wrong
		LogPrint("ERROR CNGPCarMenu.InitCarModelNameFromCarsFile. Failed to read Cars\\cars.ini file for a car idx %d", menuIdx);
	}

	return sStockCarModelName;
}


//-------------------------------------------------------------------------------------------------
// Read rbr\physics\<carSlotNameFolder>\ngpCarNameFile file and init revision/3dModel/SpecYear attributes
//
bool CNGPCarMenu::InitCarSpecDataFromPhysicsFile(const std::string &folderName, PRBRCarSelectionMenuEntry pRBRCarSelectionMenuEntry, int* outNumOfGears)
{
	DebugPrint("Enter CNGPCarMenu.InitCarSpecDataFromPhysicsFile");

	const fs::path fsFolderName(folderName);

	//std::wstring wfsFileName;
	std::string  fsFileName;
	fsFileName.reserve(128);
	
	std::string  sTextLine;
	std::wstring wsTextLine;
	sTextLine.reserve(128);
	wsTextLine.reserve(128);

	int iReadRowCount;
	bool bResult = FALSE;

	try
	{
		// Read common.lsp file and "NumberOfGears" option
		if ( outNumOfGears != nullptr && fs::exists(folderName + "\\common.lsp") )
		{
			*outNumOfGears = -1;

			std::ifstream commoLspFile(folderName + "\\common.lsp");
			while (std::getline(commoLspFile, sTextLine))
			{				
				std::replace(sTextLine.begin(), sTextLine.end(), '\t', ' ');
				_Trim(sTextLine);

				// Remove double whitechars from the string value
				sTextLine.erase(std::unique(sTextLine.begin(), sTextLine.end(), [](char a, char b) { return isspace(a) && isspace(b); }), sTextLine.end());

				if (_iStarts_With(sTextLine, "numberofgears", true))
				{
					try
					{
						*outNumOfGears = StrToInt(sTextLine.substr(14).c_str());
						if (*outNumOfGears > 2)
							*outNumOfGears -= 2; // Minus reverse and neutral gears because NGP data includes those in NumberOfGears value
					}
					catch (...)
					{
						// Do nothing. eat all exceptions.
					}
					break;
				}
			}
		}

		// Find all files without file extensions and see it the file is the NGP car model file (revison/specification date/3d model keyword in the beginning of line)
		for (const auto& entry : fs::directory_iterator(fsFolderName))
		{
			if (entry.is_regular_file() && entry.path().extension().compare(".lsp") != 0 )
			{			
				fsFileName = entry.path().filename().string();
				//DebugPrint(fsFileName.c_str());
				//wfsFileName = _ToWString(fsFileName);

				// Read NGP car model description file (it is in multibyte UTF8 format) and convert text lines to C++ UTF8 WCHAR strings
				std::ifstream rbrFile(folderName + "\\" + fsFileName);

				std::string sMultiByteUtf8TextLine;
				sMultiByteUtf8TextLine.reserve(128);

				iReadRowCount = 0;
				while (std::getline(rbrFile, sMultiByteUtf8TextLine) && iReadRowCount <= 6)
				{	
					iReadRowCount++;

					wsTextLine = ::_ToUTF8WString(sMultiByteUtf8TextLine); // Mbc-UTF8 to widechar-UTF8
					_Trim(wsTextLine);

					/*
					if (_iStarts_With(wsTextLine, wfsFileName))
					{
						bResult = TRUE;

						// Usually the first line of NGP physics description file has the filename string (car model name)
						wcsncpy_s(pRBRCarSelectionMenuEntry->wszCarModel, wfsFileName.c_str(), COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarModel));
						pRBRCarSelectionMenuEntry->wszCarModel[COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarModel) - 1] = '\0';

						continue;
					}
					*/

					// Remove double whitechars from the string value
					if(wsTextLine.length() >= 2)
						wsTextLine.erase(std::unique(wsTextLine.begin(), wsTextLine.end(), [](WCHAR a, WCHAR b) { return iswspace(a) && iswspace(b); }), wsTextLine.end());

					if (_iStarts_With(wsTextLine, L"revision", TRUE))
					{						
						bResult = TRUE;

						// Remove the revision tag and replace it with a language translated str version (or if missing then re-add the original "revision" text)
						wsTextLine.erase(0, COUNT_OF_ITEMS(L"revision")-1);
						wsTextLine.insert(0, GetLangStr(L"revision"));

						wcsncpy_s(pRBRCarSelectionMenuEntry->wszCarPhysicsRevision, wsTextLine.c_str(), COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarPhysicsRevision));
						pRBRCarSelectionMenuEntry->wszCarPhysicsRevision[COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarPhysicsRevision) - 1] = '\0';
					}
					else if (_iStarts_With(wsTextLine, L"specification date", TRUE))
					{
						bResult = TRUE;

						wsTextLine.erase(0, COUNT_OF_ITEMS(L"specification date") - 1);
						wsTextLine.insert(0, GetLangStr(L"specification date"));

						wcsncpy_s(pRBRCarSelectionMenuEntry->wszCarPhysicsSpecYear, wsTextLine.c_str(), COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarPhysicsSpecYear));
						pRBRCarSelectionMenuEntry->wszCarPhysicsSpecYear[COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarPhysicsSpecYear) - 1] = '\0';
					}
					else if (_iStarts_With(wsTextLine, L"3d model", TRUE))
					{
						bResult = TRUE;

						wsTextLine.erase(0, COUNT_OF_ITEMS(L"3d model") - 1);
						wsTextLine.insert(0, GetLangStr(L"3d model"));

						wcsncpy_s(pRBRCarSelectionMenuEntry->wszCarPhysics3DModel, wsTextLine.c_str(), COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarPhysics3DModel));
						pRBRCarSelectionMenuEntry->wszCarPhysics3DModel[COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarPhysics3DModel) - 1] = '\0';
					}
					else if (bResult && !wsTextLine.empty())
					{
						wcsncpy_s(pRBRCarSelectionMenuEntry->wszCarPhysicsCustomTxt, wsTextLine.c_str(), COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarPhysicsCustomTxt));
						pRBRCarSelectionMenuEntry->wszCarPhysicsCustomTxt[COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarPhysicsCustomTxt) - 1] = '\0';
					}
				}

				if (bResult)
				{
					// If NGP car model file found then set carModel string value and no need to iterate through other files (without file extension)
					wcsncpy_s(pRBRCarSelectionMenuEntry->wszCarModel, (_ToWString(fsFileName)).c_str(), COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarModel));
					pRBRCarSelectionMenuEntry->wszCarModel[COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarModel) - 1] = '\0';

					break;
				}
			}
		}

		if (!bResult)
		{
			std::wstring wFolderName = _ToWString(folderName);
			// Show warning that RBRCIT/NGP carModel file is missing
			wcsncpy_s(pRBRCarSelectionMenuEntry->wszCarPhysics3DModel, (wFolderName + L"\\<carModelName> NGP model description file missing").c_str(), COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarPhysics3DModel));
		}
	}
	catch (const fs::filesystem_error& ex)
	{
		DebugPrint("ERROR CNGPCarMenu.InitCarSpecDataFromPhysicsFile. %s %s", folderName.c_str(), ex.what());
		bResult = FALSE;
	}
	catch(...)
	{
		LogPrint("ERROR CNGPCarMenu.InitCarSpecDataFromPhysicsFile. %s folder doesn't have or error while reading NGP model spec file", folderName.c_str());
		bResult = FALSE;
	}

	DebugPrint("Exit CNGPCarMenu.InitCarSpecDataFromPhysicsFile");

	return bResult;
}


//-------------------------------------------------------------------------------------------------
// Init car spec data from NGP ini files (HP, year, transmissions, drive wheels, FIA category).
// Name of the NGP carList.ini entry is in pRBRCarSelectionMenuEntry->wszCarModel field.
//
bool CNGPCarMenu::InitCarSpecDataFromNGPFile(CSimpleIniW* ngpCarListINIFile, PRBRCarSelectionMenuEntry pRBRCarSelectionMenuEntry, int numOfGears)
{
	bool bResult = FALSE;

	DebugPrint("Enter CNGPCarMenu.InitCarSpecDataFromNGPFile");

	if (ngpCarListINIFile == nullptr || pRBRCarSelectionMenuEntry->wszCarModel[0] == '\0')
		return bResult;

	try
	{
		//std::wstring wsCarModelName(pRBRCarSelectionMenuEntry->wszCarModel);
		//std::transform(wsCarModelName.begin(), wsCarModelName.end(), wsCarModelName.begin(), ::tolower);

		std::wstring wsValue;

		CSimpleIniW::TNamesDepend allSections;
		ngpCarListINIFile->GetAllSections(allSections);

		CSimpleIniW::TNamesDepend::const_iterator iter;
		for (iter = allSections.begin(); iter != allSections.end(); ++iter)
		{
			const WCHAR* wszIniItemValue = ngpCarListINIFile->GetValue(iter->pItem, L"name", nullptr);
			if (wszIniItemValue == nullptr)
				continue;

			if (_wcsicmp(wszIniItemValue, pRBRCarSelectionMenuEntry->wszCarModel) == 0)
			{
				//size_t len;
				//DebugPrint(iter->pItem);

				wszIniItemValue = ngpCarListINIFile->GetValue(iter->pItem, L"cat", nullptr);
				wcstombs_s(nullptr, pRBRCarSelectionMenuEntry->szCarCategory, sizeof(pRBRCarSelectionMenuEntry->szCarCategory), wszIniItemValue, _TRUNCATE);

				wszIniItemValue = ngpCarListINIFile->GetValue(iter->pItem, L"year", nullptr);
				wcsncpy_s(pRBRCarSelectionMenuEntry->wszCarYear, wszIniItemValue, COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarYear));

				wszIniItemValue = ngpCarListINIFile->GetValue(iter->pItem, L"weight", nullptr);
				swprintf_s(pRBRCarSelectionMenuEntry->wszCarWeight, COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarWeight), L"%s kg", wszIniItemValue);

				wszIniItemValue = ngpCarListINIFile->GetValue(iter->pItem, L"power", nullptr);
				wcsncpy_s(pRBRCarSelectionMenuEntry->wszCarPower, wszIniItemValue, COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarPower));

				// TODO. Localize "gears" label
				wszIniItemValue = ngpCarListINIFile->GetValue(iter->pItem, L"trans", nullptr);
				if (numOfGears > 0)
					swprintf_s(pRBRCarSelectionMenuEntry->wszCarTrans, COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarTrans), GetLangStr(L"%d gears, %s"), numOfGears, wszIniItemValue);
				else
					wcsncpy_s(pRBRCarSelectionMenuEntry->wszCarTrans, wszIniItemValue, COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarTrans));

				bResult = TRUE;
				break;
			}

			//ngpCarListINIFile->GetAllKeys(iter->pItem, allSectionKeys);		
			//DebugPrint(iter->pItem);
			//DebugPrint(iter->pItem);
		}
	}
	catch (...)
	{
		// Hmmm...Something went wrong.
		LogPrint("ERROR CNGPCarMenu.InitCarSpecDataFromNGPFile. carList.ini INI file reading error");
		bResult = FALSE;
	}

	DebugPrint("Exit CNGPCarMenu.InitCarSpecDataFromNGPFile");

	return bResult;;
}


//-------------------------------------------------------------------------------------------------
// Init car menu names. This max len is used to move a car selection menu few pixels to left to make room for longer model names (if necessary).
// The menu name is take from a car model name (it should have been initialized by other methods before calling this method).
//
int CNGPCarMenu::CalculateMaxLenCarMenuName()
{
	int iCountI1;
	int iPos;
	size_t len = 0;

	for (int idx = 0; idx < 8; idx++)
	{
		// Car model names are in WCHAR, but RBR uses CHAR strings in car menu names. Convert the wchar car model name to char string and use only N first chars.
		//len = wcstombs(g_RBRCarSelectionMenuEntry[idx].szCarMenuName, g_RBRCarSelectionMenuEntry[idx].wszCarModel, sizeof(g_RBRCarSelectionMenuEntry[idx].szCarMenuName));
		wcstombs_s(&len, g_RBRCarSelectionMenuEntry[idx].szCarMenuName, sizeof(g_RBRCarSelectionMenuEntry[idx].szCarMenuName), g_RBRCarSelectionMenuEntry[idx].wszCarModel, _TRUNCATE);
		
		/*if (len == sizeof(g_RBRCarSelectionMenuEntry[idx].szCarMenuName))
		{
			len--;
			g_RBRCarSelectionMenuEntry[idx].szCarMenuName[len] = '\0';
		}
		*/

		if (len > MAX_CARMENUNAME_NORMALCHARS)
		{
			// Count number of I and 1 letters (narrow width). Two of those makes room for one more extra char. Otherwise the max car menu name len is MAX_CARMENUNAME_NORMALCHARS
			iCountI1 = 0;
			iPos = 0;
			while (g_RBRCarSelectionMenuEntry[idx].szCarMenuName[iPos] != '\0')
			{
				char currrentChar = g_RBRCarSelectionMenuEntry[idx].szCarMenuName[iPos++];
				if (currrentChar == '1' || currrentChar == 'I' || currrentChar == 'i' || currrentChar == 'l')
					iCountI1++;
			}

			// Normally use MAX_CARMENUNAME_NORMALCHARS chars, but if there was lots o "I" and "1" chars then allow few more chars per menu line up to MAX_CARMENUNAME_ALLCHARS
			iPos = min(MAX_CARMENUNAME_NORMALCHARS + ((iCountI1 + 1) / 2), MAX_CARMENUNAME_ALLCHARS);
			g_RBRCarSelectionMenuEntry[idx].szCarMenuName[iPos] = '\0';
			len = MAX_CARMENUNAME_NORMALCHARS;
		}

		if ( ((int)len) > m_iCarMenuNameLen)
			m_iCarMenuNameLen = (int) len;
	}

	return m_iCarMenuNameLen;
}


//------------------------------------------------------------------------------------------------
// Return the carID of the next screenshot (the next carID or the next carID with missing image file (currentCarID input param is slot number 0..7).
// The first call to this method should use currentCarID=-1 value to start checking from the first car. Subsequent calls can supply the existing carID.
//
int CNGPCarMenu::GetNextScreenshotCarID(int currentCarID)
{
	while (true)
	{
		currentCarID++;

		if (currentCarID >= 0 && currentCarID <= 7)
		{
			std::wstring outputFileName;

			if (!m_screenshotPath.empty() && g_RBRCarSelectionMenuEntry[RBRAPI_MapCarIDToMenuIdx(currentCarID)].wszCarModel[0] != '\0')
			{
				// If CreateOption is "only missing car images" then check the existence of output img file
				if (m_iMenuCreateOption == 0)
				{
					//std::string imgExtension = ".";
					//imgExtension = imgExtension + g_NGPCarMenu_ImageOptions[m_iMenuImageOption];
					//_ToLowerCase(imgExtension);
					// If the output PNG preview image file already exists then don't re-generate the screenshot (menu option "Only missing car images")
					//outputFileName = m_screenshotPath + L"\\" + g_RBRCarSelectionMenuEntry[RBRAPI_MapCarIDToMenuIdx(currentCarID)].wszCarModel + _ToWString(imgExtension);
					outputFileName = ReplacePathVariables(m_screenshotPath, RBRAPI_MapCarIDToMenuIdx(currentCarID), false);

					if (fs::exists(outputFileName))
						continue;
				}

				// Return the carID of the next screenshot (file missing or overwrite existing image until all car images created)
				break;
			}
		}
		else
		{
			// All screenshots generated
			// TODO. Exit to main menu?
			//::_wremove( (g_pRBRPlugin->m_sRBRRootDirW + L"\\Replays\\" + C_REPLAYFILENAME_SCREENSHOTW).c_str() );

			currentCarID = -1;
			break;
		}
	}

	// 0..7 = Valid carID, <0 no more screenshots
	return currentCarID;
}


//------------------------------------------------------------------------------------------------
// Prepare RBR "screenshot replay file" for a new car 3D model (replay was saved using certain car model, but modify it on the fly to show another car model)
// CarID is the RBR car slot# 0..7
//
bool CNGPCarMenu::PrepareScreenshotReplayFile(int carID)
{
	std::wstring inputReplayFilePath;
	std::wstring inputReplayFileName;
	std::wstring outputReplayFileName;

	std::wstring carModelName;
	std::wstring carCategoryName;

	int carMenuIdx = RBRAPI_MapCarIDToMenuIdx(carID);

	inputReplayFilePath  = g_pRBRPlugin->m_sRBRRootDirW + L"\\Plugins\\" L"" VS_PROJECT_NAME L"\\Replays"; // NGPCarMenu replay template folder
	outputReplayFileName = g_pRBRPlugin->m_sRBRRootDirW + L"\\Replays\\" + C_REPLAYFILENAME_SCREENSHOTW;  // RBR replay folder and temporary output filename
	
	carModelName = g_RBRCarSelectionMenuEntry[RBRAPI_MapCarIDToMenuIdx(carID)].wszCarModel;
	carCategoryName = _ToWString(std::string(g_RBRCarSelectionMenuEntry[RBRAPI_MapCarIDToMenuIdx(carID)].szCarCategory));

	try
	{
		std::wstring screenshotReplayFileNameWithoutExt = fs::path(g_pRBRPlugin->m_screenshotReplayFileName).replace_extension();

		// Lookup the replay template used to generate a car preview image. The replay file is chosen based on carModel name, category name or finally the "global" default file if more specific version was not found
		// (1) rbr\Plugins\NGPCarMenu\Replays\NGPCarName_<CarModelName>.rpl (fex "NGPCarMenu_Citroen C3 WRC 2017.rpl")
		// (2) rbr\Plugins\NGPCarMenu\Replays\NGPCarName_<FIACategoryName>.rpl (fex "NGPCarMenu_Group R1.rpl")
		// (3) rbr\Plugins\NGPCarMenu\Replays\NGPCarName.rpl (this file should always exists as a fallback rpl template)

		inputReplayFileName = inputReplayFilePath + L"\\" + screenshotReplayFileNameWithoutExt + L"_" + carModelName + L".rpl";
		if (!fs::exists(inputReplayFileName))
		{
			inputReplayFileName = inputReplayFilePath + L"\\" + screenshotReplayFileNameWithoutExt + L"_" + carCategoryName + L".rpl";
			if (!fs::exists(inputReplayFileName))
			{
				// No carModel or carCategory specific replay template file (fex "NGPCarMenu_Group R1.rpl" file is used with GroupR1 cars). Use the default generic replay file (NGPCarMenu.rpl by default, but INI file can re-define this filename).
				inputReplayFileName = inputReplayFilePath + L"\\" + g_pRBRPlugin->m_screenshotReplayFileName;
				if (!fs::exists(inputReplayFileName))
					inputReplayFileName = g_pRBRPlugin->m_sRBRRootDirW + L"\\Replays\\" + g_pRBRPlugin->m_screenshotReplayFileName;
			}
		}

		LogPrint(L"Preparing a car preview image creation. Using template file %s for a car %s (%s) (car#=%d menu#=%d)", inputReplayFileName.c_str(), carModelName.c_str(), carCategoryName.c_str(), carID, carMenuIdx);

		// Open input replay template file, modify car model ID on the fly and write out the temporary replay file (stored in RBR Replays folder)

		std::ifstream srcFile(inputReplayFileName, std::ifstream::binary | std::ios::in);
		if (!srcFile) return FALSE;

		std::vector<unsigned char> fileBuf(std::istreambuf_iterator<char>(srcFile), {});		
		srcFile.close();

		std::ofstream outFile(outputReplayFileName, std::ofstream::binary | std::ios::out);
		if (!outFile) return FALSE;

		fileBuf[0x20] = (unsigned char)carID; // 00..07
		std::copy(fileBuf.begin(), fileBuf.end(), std::ostreambuf_iterator<char>(outFile));
		outFile.flush();
		outFile.close();

		return TRUE;
	}
	catch (const fs::filesystem_error& ex)
	{
		LogPrint("ERROR CNGPCarMenu::PrepareScreenshotReplayFile. %s carid=%d %s", inputReplayFileName.c_str(), carID, ex.what());
		return FALSE;
	}
	catch (...)
	{
		LogPrint("ERROR CNGPCarMenu::PrepareScreenshotReplayFile. %s carid=%d", inputReplayFileName.c_str(), carID);
		return FALSE;
	}
}


//-----------------------------------------------------------------------------------------------
// Replace all %variableName% RBR path variables with the actual value
//
std::wstring CNGPCarMenu::ReplacePathVariables(const std::wstring& sPath, int selectedCarIdx, bool rbrtmplugin)
{
	WCHAR szResolutionText[16];
	std::wstring sResult = sPath;

	RBRAPI_RefreshWndRect();
	swprintf_s(szResolutionText, COUNT_OF_ITEMS(szResolutionText), L"%dx%d", g_rectRBRWndClient.right, g_rectRBRWndClient.bottom);
	sResult = _ReplaceStr(sResult, L"%resolution%", szResolutionText);

	if (selectedCarIdx >= 0 && selectedCarIdx <= 7)
	{
		sResult = _ReplaceStr(sResult, L"%carmodelname%", g_RBRCarSelectionMenuEntry[selectedCarIdx].wszCarModel);
		sResult = _ReplaceStr(sResult, L"%carslotnum%",  std::to_wstring(::RBRAPI_MapMenuIdxToCarID(selectedCarIdx)));
		sResult = _ReplaceStr(sResult, L"%carmenunum%",  std::to_wstring(selectedCarIdx));
		sResult = _ReplaceStr(sResult, L"%fiacategory%", _ToWString(g_RBRCarSelectionMenuEntry[selectedCarIdx].szCarCategory));

		// %carFolder% value is taken from Cars.ini FileName attribute (drop the trailing filename and optioal double quotes around the value)
		WCHAR wszStockCarINISection[8];
		CSimpleIniW stockCarListINIFile;
		std::wstring sCarFolder;

		stockCarListINIFile.LoadFile((m_sRBRRootDirW + L"\\cars\\Cars.ini").c_str());
		swprintf_s(wszStockCarINISection, COUNT_OF_ITEMS(wszStockCarINISection), L"Car0%d", ::RBRAPI_MapMenuIdxToCarID(selectedCarIdx));
		sCarFolder = stockCarListINIFile.GetValue(wszStockCarINISection, L"FileName", L"");
		_Trim(sCarFolder);

		// Remove extra path from "Cars\Yaris_WRC18\Yaris_WRC18.ini" value and leave only "Yaris_WRC18" folder name (ie. take parent path and use it as "filename" which is actually the parent folder name)
		std::filesystem::path sCarFolderPath(_RemoveEnclosingChar(sCarFolder, L'"', false));
		sResult = _ReplaceStr(sResult, L"%carfolder%", sCarFolderPath.parent_path().filename());
	}

	std::wstring imgExtension;
	imgExtension = _ToWString(g_NGPCarMenu_ImageOptions[this->m_iMenuImageOption]);
	sResult = _ReplaceStr(sResult, L"%filetype%", imgExtension);

	sResult = _ReplaceStr(sResult, L"%plugin%", (rbrtmplugin ? L"RBRTM" : L"RBR"));

	return sResult;
}


//-----------------------------------------------------------------------------------------------
// Initialize car preview image DX9 texture and vertex objects
// 
bool CNGPCarMenu::ReadCarPreviewImageFromFile(int selectedCarIdx, float x, float y, float cx, float cy, IMAGE_TEXTURE* pOutImageTexture, DWORD dwFlags, bool isRBRTMPlugin)
{
	//std::string imgExtension = ".";
	//imgExtension += g_NGPCarMenu_ImageOptions[this->m_iMenuImageOption];  // .PNG or .BMP img file format
	//_ToLowerCase(imgExtension);

	HRESULT hResult = D3D9CreateRectangleVertexTexBufferFromFile(g_pRBRIDirect3DDevice9,
		//this->m_screenshotPath + L"\\" + g_RBRCarSelectionMenuEntry[selectedCarIdx].wszCarModel + _ToWString(imgExtension),
		ReplacePathVariables(this->m_screenshotPath, selectedCarIdx, isRBRTMPlugin),
		x, y, cx, cy,
		pOutImageTexture,
		dwFlags);

	// Image not available. Do not try to re-load it again (set cx=-1 to indicate that the image loading failed, so no need to try to re-load it in every frame even when texture is null)
	if (!SUCCEEDED(hResult)) pOutImageTexture->imgSize.cx = -1;

	return pOutImageTexture->imgSize.cx != -1;
}


//------------------------------------------------------------------------------------------------
//
const char* CNGPCarMenu::GetName(void)
{
	DebugPrint("Enter CNGPCarMenu.GetName");

	// Re-route RBR DXEndScene function to our custom function
	if (g_bRBRHooksInitialized == FALSE && m_PluginState == T_PLUGINSTATE::PLUGINSTATE_UNINITIALIZED)
	{
		m_PluginState = T_PLUGINSTATE::PLUGINSTATE_INITIALIZING;

		DebugPrint("CNGPCarMenu.GetName. First time initialization of NGPCarMenu options and RBR interfaces");

		try
		{
			std::string sReplayTemplateFileName;

			// Remove the temporary replay template file when the plugin is initialized (this way the temp RPL file won't be left around for too long)
			sReplayTemplateFileName = g_pRBRPlugin->m_sRBRRootDir + "\\Replays\\" + C_REPLAYFILENAME_SCREENSHOT;
			if (fs::exists(sReplayTemplateFileName)) ::remove(sReplayTemplateFileName.c_str());

			// NGPCarMenu V1.0.4 and older had a replay template file rbr\Replays\NGPCarMenu.rpl. This no longer used in V1.0.5+ versions, so remove the old rpl file.
			// Nowadays the plugin uses rbr\Plugins\NGPCarMenu\Replays\ folder to read rpl template files.
			sReplayTemplateFileName = g_pRBRPlugin->m_sRBRRootDir + "\\Replays\\" VS_PROJECT_NAME ".rpl";
			if (fs::exists(sReplayTemplateFileName))
			{
					DebugPrint("CNGPCarMenu.GetName. The old obsolete %s replay template file from V1.0.4 and older versions was deleted.", sReplayTemplateFileName.c_str());
					::remove(sReplayTemplateFileName.c_str());
			}
		}
		catch (...)
		{
			// Do nothing even when removal of the replay template file failed. This is not fatal error at this point.
		}

		RBRAPI_InitializeObjReferences();
		m_pD3D9RenderStateCache = new CD3D9RenderStateCache(g_pRBRIDirect3DDevice9, false);

		RefreshSettingsFromPluginINIFile(true);

		// Init font to draw custom car spec info text (3D model and custom livery text)
		int iFontSize = 12;
		if (g_rectRBRWndClient.bottom < 600) iFontSize = 7;
		else if (g_rectRBRWndClient.bottom < 768) iFontSize = 9;

		g_pFontCarSpecCustom = new CD3DFont(L"Trebuchet MS", iFontSize, 0);
		g_pFontCarSpecCustom->InitDeviceObjects(g_pRBRIDirect3DDevice9);
		g_pFontCarSpecCustom->RestoreDeviceObjects();	

		// If EASYRBRPath is not set then assume cars have been setup using RBRCIT car manager tool
		if (m_easyRBRFilePath.empty()) InitCarSpecData_RBRCIT();
		else InitCarSpecData_EASYRBR();
		CalculateMaxLenCarMenuName();

		InitCarSpecAudio();		// FMOD bank names per car

#if USE_DEBUG == 1
		g_pFontDebug = new CD3DFont(L"Courier New", 11, 0);
		g_pFontDebug->InitDeviceObjects(g_pRBRIDirect3DDevice9);
		g_pFontDebug->RestoreDeviceObjects();
#endif 

		// Initialize custom car selection menu name and model name strings (change RBR string pointer to our custom string)
		int idx;
		for (idx = 0; idx < 8; idx++)
		{
			// Use custom car menu selection name and car description (taken from the current NGP config files)
			WriteOpCodePtr((LPVOID)g_RBRCarSelectionMenuEntry[idx].ptrCarMenuName, &g_RBRCarSelectionMenuEntry[idx].szCarMenuName[0]);
			WriteOpCodePtr((LPVOID)g_RBRCarSelectionMenuEntry[idx].ptrCarDescription, &g_RBRCarSelectionMenuEntry[idx].szCarMenuName[0]); // the default car description is CHAR and not WCHAR string
		}

		// If any of the car menu names is longer than 20 chars then move the whole menu list to left to make more room for longer text
		if (m_iCarMenuNameLen > 20)
		{
			// Default car selection menu X-position 0x0920. Minimum X-pos is 0x0020
			int menuXPos = max(0x920 - ((m_iCarMenuNameLen - 20 + 1) * 0x100), 0x0020);

			for (idx = 0; idx < 8; idx++)
			{
				g_pRBRMenuSystem->menuObj[RBRMENUIDX_QUICKRALLY_CARS]->pItemPosition[g_pRBRMenuSystem->menuObj[RBRMENUIDX_QUICKRALLY_CARS]->firstSelectableItemIdx + idx].x = menuXPos;
				g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P1]->pItemPosition[g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P1]->firstSelectableItemIdx + idx].x = menuXPos;
				g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P2]->pItemPosition[g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P2]->firstSelectableItemIdx + idx].x = menuXPos;
				g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P3]->pItemPosition[g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P3]->firstSelectableItemIdx + idx].x = menuXPos;
				g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P4]->pItemPosition[g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P4]->firstSelectableItemIdx + idx].x = menuXPos;
				g_pRBRMenuSystem->menuObj[RBRMENUIDX_RBRCHALLENGE_CARS]->pItemPosition[g_pRBRMenuSystem->menuObj[RBRMENUIDX_RBRCHALLENGE_CARS]->firstSelectableItemIdx + idx].x = menuXPos;
			}
		}


		//
		// Ready to rock! Re-route and store the original function address for later use. At this point all variables used in custom Dx0 functions should be initialized already
		//
		gtcDirect3DBeginScene = new DetourXS((LPVOID)0x0040E880, CustomRBRDirectXBeginScene);
		Func_OrigRBRDirectXBeginScene = (tRBRDirectXBeginScene)gtcDirect3DBeginScene->GetTrampoline();

		gtcDirect3DEndScene = new DetourXS((LPVOID)0x0040E890, CustomRBRDirectXEndScene);
		Func_OrigRBRDirectXEndScene = (tRBRDirectXEndScene)gtcDirect3DEndScene->GetTrampoline();	 

		DebugPrint("CRBRNetTNG.GetName. BeginSceneIsFirst=%d  EndSceneIsFirst=%d", gtcDirect3DBeginScene->IsFirstHook(), gtcDirect3DEndScene->IsFirstHook());

		// RBR memory and DX9 function hooks in place. Ready to do customized RBR logic
		g_bRBRHooksInitialized = TRUE;
		m_PluginState = T_PLUGINSTATE::PLUGINSTATE_INITIALIZED;

		//for (int idx = 0; idx < 8; idx++)
		//	DebugPrint(ReplacePathVariables(L"Resolution=%resolution% CarModelName=%carModelName% CarFolder=%carFolder% CarSlotNum=%carSlotNum% CarMenuNum=%carMenuNum% Cat=%FIACategory% FileType=%fileType% Plugin=%plugin%", idx, false).c_str());
	}

	DebugPrint("Exit CNGPCarMenu.GetName");

	return "NGPCarMenu";
}

//------------------------------------------------------------------------------------------------
//
void CNGPCarMenu::DrawResultsUI(void)
{
/*
	m_pGame->SetMenuColor(IRBRGame::MENU_HEADING);
	m_pGame->SetFont(IRBRGame::FONT_BIG);
	m_pGame->WriteText(130.0f, 49.0f, "Results");

	m_pGame->SetFont(IRBRGame::FONT_SMALL);
	m_pGame->SetMenuColor(IRBRGame::MENU_TEXT);
	if (m_fResults[2] <= 0.0f)
	{
		m_pGame->WriteText(200.0f, 100.0f, "DNF");
	}
	else
	{
		char txtCP1[32];
		char txtTimeString[32];
		char txtBuffer[128];

		sprintf(txtBuffer, "Stage Result for \"%s\" ", m_szPlayerName);
		m_pGame->WriteText(130.0f, 100.0f, txtBuffer);
		sprintf(txtBuffer, "CheckPoint1 = %s", NPlugin::FormatTimeString(txtCP1, sizeof(txtCP1), m_fResults[0]));
		m_pGame->WriteText(130.0f, 125.0f, txtBuffer);
		sprintf(txtBuffer, "CheckPoint2 = %s", NPlugin::FormatTimeString(txtCP1, sizeof(txtCP1), m_fResults[1]));
		m_pGame->WriteText(130.0f, 150.0f, txtBuffer);
		sprintf(txtBuffer, "Finish = %s", NPlugin::FormatTimeString(txtTimeString, sizeof(txtTimeString), m_fResults[2]));
		m_pGame->WriteText(130.0f, 175.0f, txtBuffer);
	}
*/
}

//------------------------------------------------------------------------------------------------
//
void CNGPCarMenu::DrawFrontEndPage(void)
{
	char szTextBuf[128]; // This should be enough to hold the longest g_RBRPlugin menu string + the logest g_RBRPluginMenu_xxxOptions option string
	float posY;
	int iRow;

	// Draw blackout (coordinates specify the 'window' where you don't want black)
	m_pGame->DrawBlackOut(420.0f, 0.0f, 420.0f, 480.0f);

	// Draw custom plugin header line
	m_pGame->SetMenuColor(IRBRGame::MENU_HEADING);
	m_pGame->SetFont(IRBRGame::FONT_BIG);
	m_pGame->WriteText(65.0f, 49.0f, m_sPluginTitle.c_str());

	// The red menu selection line
	m_pGame->DrawSelection(0.0f, 68.0f + (static_cast< float >(m_iMenuSelection) * 21.0f), 360.0f);

	m_pGame->SetMenuColor(IRBRGame::MENU_TEXT);
	for (unsigned int i = 0; i < COUNT_OF_ITEMS(g_NGPCarMenu_PluginMenu); ++i)
	{
		if (i == C_MENUCMD_CREATEOPTION)
			sprintf_s(szTextBuf, sizeof(szTextBuf), "%s: %s", g_NGPCarMenu_PluginMenu[i], g_NGPCarMenu_CreateOptions[m_iMenuCreateOption]);
		else if (i == C_MENUCMD_IMAGEOPTION)
			sprintf_s(szTextBuf, sizeof(szTextBuf), "%s: %s", g_NGPCarMenu_PluginMenu[i], g_NGPCarMenu_ImageOptions[m_iMenuImageOption]);
		else if (i == C_MENUCMD_RBRTMOPTION)
			sprintf_s(szTextBuf, sizeof(szTextBuf), "%s: %s", g_NGPCarMenu_PluginMenu[i], g_NGPCarMenu_EnableDisableOptions[m_iMenuRBRTMOption]);
		else
			sprintf_s(szTextBuf, sizeof(szTextBuf), "%s", g_NGPCarMenu_PluginMenu[i]);

		m_pGame->WriteText(65.0f, 70.0f + (static_cast< float >(i) * 21.0f), szTextBuf);
	}

	m_pGame->SetFont(IRBRGame::FONT_SMALL);

	posY = 70.0f + (static_cast<float>COUNT_OF_ITEMS(g_NGPCarMenu_PluginMenu)) * 21.0f;
	iRow = 3;

	if (!m_sMenuStatusText1.empty()) m_pGame->WriteText(10.0f, posY + (static_cast<float>(iRow++) * 18.0f), m_sMenuStatusText1.c_str());
	if (!m_sMenuStatusText2.empty()) m_pGame->WriteText(10.0f, posY + (static_cast<float>(iRow++) * 18.0f), m_sMenuStatusText2.c_str());
	if (!m_sMenuStatusText3.empty()) m_pGame->WriteText(10.0f, posY + (static_cast<float>(iRow++) * 18.0f), m_sMenuStatusText3.c_str());

	iRow += 3;
	m_pGame->WriteText(10.0f, posY + (static_cast<float>(iRow++) * 18.0f), C_PLUGIN_FOOTER_STR);
}


//------------------------------------------------------------------------------------------------
//
//#if USE_DEBUG == 1
//typedef IPlugin* ( *tRBR_CreatePlugin)(IRBRGame* pGame);
//#endif

#define DO_MENUSELECTION_LEFTRIGHT(OptionID, OptionValueVariable, OptionArray) \
   if (m_iMenuSelection == OptionID && bLeft && (--OptionValueVariable) < 0) OptionValueVariable = 0; \
   else if (m_iMenuSelection == OptionID && bRight && (++OptionValueVariable) >= COUNT_OF_ITEMS(OptionArray)) OptionValueVariable = COUNT_OF_ITEMS(OptionArray)-1


void CNGPCarMenu::HandleFrontEndEvents(char txtKeyboard, bool bUp, bool bDown, bool bLeft, bool bRight, bool bSelect)
{
	// Clear the previous status text when menu selection changes
	if (bUp || bDown || bSelect)
		m_sMenuStatusText1.clear();

	if (bSelect)
	{
		//
		// Menu item "selected". Do the action.
		//

		if (m_iMenuSelection == C_MENUCMD_RELOAD)
		{
			// Re-read car preview images from source files
			ClearCachedCarPreviewImages();
			RefreshSettingsFromPluginINIFile();

			// DEBUG. Show/Hide cropping area
			//bool bNewStatus = !m_bCustomReplayShowCroppingRect;
			m_bCustomReplayShowCroppingRect = false;
			//D3D9CreateRectangleVertexBuffer(g_pRBRIDirect3DDevice9, (float)this->m_screenshotCroppingRect.left, (float)this->m_screenshotCroppingRect.top, (float)(this->m_screenshotCroppingRect.right - this->m_screenshotCroppingRect.left), (float)(this->m_screenshotCroppingRect.bottom - this->m_screenshotCroppingRect.top), &m_screenshotCroppingRectVertexBuffer);
			//m_bCustomReplayShowCroppingRect = bNewStatus;		
		}
		//else if (m_iMenuSelection == C_MENUCMD_CREATEOPTION || m_iMenuSelection == C_MENUCMD_IMAGEOPTION)
		//{
			// Do nothing when option line is selected
		//}
		else if (m_iMenuSelection == C_MENUCMD_CREATE)
		{
			// Create new car preview images using BMP or PNG format

			m_bCustomReplayShowCroppingRect = false;
			m_iCustomReplayScreenshotCount = 0;

			RefreshSettingsFromPluginINIFile();

			// Get the first carID without an image (or the carID=0 if all images should be overwritten). Value -1 indicates that no more screenshots to generate.
			m_iCustomReplayCarID = GetNextScreenshotCarID(-1);

			// Prepare replay to show custom car 3D model and car position
			if (m_iCustomReplayCarID >= 0 && CNGPCarMenu::PrepareScreenshotReplayFile(m_iCustomReplayCarID))
			{
				ClearCachedCarPreviewImages();

				// Create the preview rectangle around the screenshot cropping area to highlight the screenshot area (user can tweak INI file settings if cropping area is not perfect)
				D3D9CreateRectangleVertexBuffer(g_pRBRIDirect3DDevice9, (float)this->m_screenshotCroppingRect.left, (float)this->m_screenshotCroppingRect.top, (float)(this->m_screenshotCroppingRect.right - this->m_screenshotCroppingRect.left), (float)(this->m_screenshotCroppingRect.bottom - this->m_screenshotCroppingRect.top), &m_screenshotCroppingRectVertexBuffer);

				// Set a flag that custom replay generation is active during replays. If this is zero then replay plays the file normally
				m_iCustomReplayState = 1;
				
				::RBRAPI_Replay(this->m_sRBRRootDir, C_REPLAYFILENAME_SCREENSHOT);
			}
			else
				m_sMenuStatusText1 = "All cars already have a preview image. Did not create any new images.";

		}
	}


	//
	// Menu focus line moved up or down
	//
	if (bUp && (--m_iMenuSelection) < 0)
		//m_iMenuSelection = COUNT_OF_ITEMS(g_RBRPluginMenu) - 1;  // Wrap around logic
		m_iMenuSelection = 0;  // No wrapping logic

	if (bDown && (++m_iMenuSelection) >= COUNT_OF_ITEMS(g_NGPCarMenu_PluginMenu))
		//m_iMenuSelection = 0; // Wrap around logic
		m_iMenuSelection = COUNT_OF_ITEMS(g_NGPCarMenu_PluginMenu) - 1;


	//
	// Menu options changed in the current menu line. Options don't wrap around.
	// Note! Not all menu lines have any additional options.
	//
	DO_MENUSELECTION_LEFTRIGHT(C_MENUCMD_CREATEOPTION, m_iMenuCreateOption, g_NGPCarMenu_CreateOptions);

	int iPrevMenuImageOptionValue = m_iMenuImageOption;
	DO_MENUSELECTION_LEFTRIGHT(C_MENUCMD_IMAGEOPTION, m_iMenuImageOption, g_NGPCarMenu_ImageOptions);

	int iPrevMenuRMRTMOptionValue = m_iMenuRBRTMOption;
	DO_MENUSELECTION_LEFTRIGHT(C_MENUCMD_RBRTMOPTION, m_iMenuRBRTMOption, g_NGPCarMenu_EnableDisableOptions);
	
	if (m_iMenuRBRTMOption == 1 && iPrevMenuRMRTMOptionValue != m_iMenuRBRTMOption)
		g_bNewCustomPluginIntegrations = TRUE;

	if(iPrevMenuImageOptionValue != m_iMenuImageOption || iPrevMenuRMRTMOptionValue != m_iMenuRBRTMOption)
		SaveSettingsToPluginINIFile();
}

//------------------------------------------------------------------------------------------------
//
void CNGPCarMenu::TickFrontEndPage(float fTimeDelta)
{
	// Do nothing
}


//------------------------------------------------------------------------------------------------
// Is called when player finishes stage ( fFinishTime is 0.0f if player failed the stage )
//
void CNGPCarMenu::HandleResults(float fCheckPoint1, float fCheckPoint2,	float fFinishTime, const char* ptxtPlayerName)
{
	// Do nothing
}

//------------------------------------------------------------------------------------------------
// Is called when a player passed a checkpoint 
//
 void CNGPCarMenu::CheckPoint(float fCheckPointTime, int iCheckPointID, const char* ptxtPlayerName)
 {
	//m_pGame->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	//m_pGame->WriteGameMessage("CHECKPOINT!!!!!!!!!!", 5.0f, 50.0f, 250.0f);
}


//------------------------------------------------------------------------------------------------
// Is called when the player timer starts (after GO! or in case of a false start)
//
void CNGPCarMenu::StageStarted(int iMap, const char* ptxtPlayerName, bool bWasFalseStart)
{
	// Do nothing
}


//--------------------------------------------------------------------------------------------
// D3D9 BeginScene handler. 
// If current menu is "SelectCar" then show custom car details or if custom replay video is playing then generate preview images.
//
HRESULT __fastcall CustomRBRDirectXBeginScene(void* objPointer)
{
	if (!g_bRBRHooksInitialized) 
		return S_OK;

	// Call the origial RBR BeginScene and let it to initialize the new D3D scene
	HRESULT hResult = Func_OrigRBRDirectXBeginScene(objPointer);

	if (g_pRBRGameMode->gameMode == 03)
	{ 
		if (g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_QUICKRALLY_CARS]
			|| g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P1]
			|| g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P2]
			|| g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P3]
			|| g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P4]
			|| g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_RBRCHALLENGE_CARS])
		{
			// RBR is in "Menu open" state (gameMode=3) and the current menu object is showing "select a car" menu.
			// Show customized car model name, specs, NGP physics and 3D model details and a car preview picture.
			int selectedCarIdx = g_pRBRMenuSystem->currentMenuObj->selectedItemIdx - g_pRBRMenuSystem->currentMenuObj->firstSelectableItemIdx;

			if (selectedCarIdx >= 0 && selectedCarIdx <= 7)
			{
				// Change car spec string pointers on the fly (car spec item idx 12)
				PRBRMenuItemCarSelectionCarSpecTexts pCarMenuSpecTexts = (PRBRMenuItemCarSelectionCarSpecTexts)g_pRBRMenuSystem->currentMenuObj->pItemObj[12];
				PRBRCarSelectionMenuEntry pCarSelectionMenuEntry = &g_RBRCarSelectionMenuEntry[selectedCarIdx];

				if (pCarMenuSpecTexts != NULL)
				{
					// Car selection menu. Make the car model preview screen few pixels bigger and modify car details based on the currently selected car menu row

					// Hide the default "SelectCar" preview image (zero height and width)
					// TODO: Find where these menu specific pos/width/height values are originally stored. Now tweak the height everytime car menu selection is about to be shown.
					g_pRBRMenuSystem->menuImageHeight = g_pRBRMenuSystem->menuImageWidth = 0;

					if (pCarMenuSpecTexts->wszTorqueTitle != g_pOrigCarSpecTitleWeight)
					{
						if (g_pOrigCarSpecTitleWeight == NULL)
						{
							DebugPrint("CustomRBRDirectXBeginScene. First time customization of SelectCar menu");

							// Store the original RBR localized weight, horsepower and transmission title values. These titles are re-used in customized carSpec info screen.
							g_pOrigCarSpecTitleWeight = pCarMenuSpecTexts->wszWeightTitle;
							g_pOrigCarSpecTitleTransmission = pCarMenuSpecTexts->wszTransmissionTitle;
							g_pOrigCarSpecTitleHorsepower = pCarMenuSpecTexts->wszHorsepowerTitle;
						}

						// Move carSpec text block few lines up to align "Select Car" and "car model" specLine text. 0x01701B20  (default carSpec pos-y value 0x06201B20)
						g_pRBRMenuSystem->currentMenuObj->pItemPosition[12].y = 0x0170;

						pCarMenuSpecTexts->wszModelTitle = (WCHAR*)g_pRBRPlugin->GetLangStr(L"FIA Category:");                // FIACategory label
						pCarMenuSpecTexts->wszHorsepowerTitle = (WCHAR*)g_pRBRPlugin->GetLangStr(g_pOrigCarSpecTitleHorsepower); // Horsepower text
						pCarMenuSpecTexts->wszTorqueTitle = (WCHAR*)g_pRBRPlugin->GetLangStr(g_pOrigCarSpecTitleWeight);	     // Re-use rbr localized Weight title in Torque spec line
						pCarMenuSpecTexts->wszEngineTitle = (WCHAR*)g_pRBRPlugin->GetLangStr(g_pOrigCarSpecTitleTransmission);// Re-use rbr localized transmission title in Weight spec line
						pCarMenuSpecTexts->wszTyresTitle = (WCHAR*)g_pRBRPlugin->GetLangStr(L"Year:");						 // Year text

						// Original Weight and Transmission title and value lines are hidden
						pCarMenuSpecTexts->wszWeightTitle = pCarMenuSpecTexts->wszWeightValue =
						pCarMenuSpecTexts->wszTransmissionTitle = pCarMenuSpecTexts->wszTransmissionValue = L"";
					}

					// Change default carSpec values each time a new car menu line was selected or SelectCar menu was opened
					if (pCarMenuSpecTexts->wszTechSpecValue != pCarSelectionMenuEntry->wszCarModel || g_pRBRPlugin->m_bMenuSelectCarCustomized == false)
					{
						pCarMenuSpecTexts->wszTechSpecValue = pCarSelectionMenuEntry->wszCarModel;
						pCarMenuSpecTexts->wszHorsepowerValue = pCarSelectionMenuEntry->wszCarPower;
						pCarMenuSpecTexts->wszTorqueValue = pCarSelectionMenuEntry->wszCarWeight;
						pCarMenuSpecTexts->wszEngineValue = pCarSelectionMenuEntry->wszCarTrans;

						WriteOpCodePtr((LPVOID)pCarSelectionMenuEntry->ptrCarDescription, pCarSelectionMenuEntry->szCarCategory);

						WriteOpCodeBuffer((LPVOID)PTR_TYREVALUE_WCHAR_FIRESTONE, (const BYTE*)pCarSelectionMenuEntry->wszCarYear, 5 * sizeof(WCHAR));
						WriteOpCodeBuffer((LPVOID)PTR_TYREVALUE_WCHAR_BRIDGESTONE, (const BYTE*)pCarSelectionMenuEntry->wszCarYear, 5 * sizeof(WCHAR));
						WriteOpCodeBuffer((LPVOID)PTR_TYREVALUE_WCHAR_PIRELLI, (const BYTE*)pCarSelectionMenuEntry->wszCarYear, 5 * sizeof(WCHAR));
						WriteOpCodeBuffer((LPVOID)PTR_TYREVALUE_WCHAR_MICHELIN, (const BYTE*)pCarSelectionMenuEntry->wszCarYear, 5 * sizeof(WCHAR));
					}

					g_pRBRPlugin->m_bMenuSelectCarCustomized = true;
				}
			}
		}
		else if (g_pRBRPlugin->m_bMenuSelectCarCustomized)
		{
			// "CarSelect" menu closed and the tyre brand string was modified. Restore the original tyre brand string value
			WriteOpCodeBuffer((LPVOID)PTR_TYREVALUE_WCHAR_FIRESTONE, (const BYTE*)L"Firestone", 9 * sizeof(WCHAR));  // No need to write NULL char because it is already there 
			WriteOpCodeBuffer((LPVOID)PTR_TYREVALUE_WCHAR_BRIDGESTONE, (const BYTE*)L"Bridgestone", 11 * sizeof(WCHAR));
			WriteOpCodeBuffer((LPVOID)PTR_TYREVALUE_WCHAR_PIRELLI, (const BYTE*)L"Pirelli", 7 * sizeof(WCHAR));
			WriteOpCodeBuffer((LPVOID)PTR_TYREVALUE_WCHAR_MICHELIN, (const BYTE*)L"Michelin", 8 * sizeof(WCHAR));

			// Restore car description to show car name instead of FIA category as shown in "Select Car" menu
			for (int selectedCarIdx = 0; selectedCarIdx < 8; selectedCarIdx++)
				WriteOpCodePtr((LPVOID)g_RBRCarSelectionMenuEntry[selectedCarIdx].ptrCarDescription, g_RBRCarSelectionMenuEntry[selectedCarIdx].szCarMenuName);

			g_pRBRPlugin->m_bMenuSelectCarCustomized = false;
		}


		// If there are new custom plugin integration requests then initialize those now
		if (g_bNewCustomPluginIntegrations)
			g_pRBRPlugin->InitAllNewCustomPluginIntegrations();

		// Check if any of the custom plugins is active
		if (g_pRBRPluginIntegratorLinkList != nullptr && g_pRBRPluginMenuSystem->customPluginMenuObj != nullptr)
		{
			for (auto& item : *g_pRBRPluginIntegratorLinkList)
			{
				if (!item->m_bCustomPluginActive && g_pRBRMenuSystem->currentMenuObj == g_pRBRPluginMenuSystem->customPluginMenuObj && item->m_iCustomPluginMenuIdx == g_pRBRPluginMenuSystem->pluginsMenuObj->selectedItemIdx)
					item->m_bCustomPluginActive = true;
				else if (item->m_bCustomPluginActive && (g_pRBRMenuSystem->currentMenuObj == g_pRBRPluginMenuSystem->optionsMenuObj || g_pRBRMenuSystem->currentMenuObj == g_pRBRPluginMenuSystem->pluginsMenuObj || g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_MAIN]))
					item->m_bCustomPluginActive = false;
			}
		}
	}
	else if (g_pRBRPlugin->m_iCustomReplayState > 0)
	{	
		// NGPCarMenu plugin is generating car preview screenshot images. Draw a car model and take a screenshot from a specified cropping screen area
		g_pRBRPlugin->m_bCustomReplayShowCroppingRect = true;
		g_pRBRCarInfo->stageStartCountdown = 1.0f;

		if (g_pRBRGameMode->gameMode == 0x08)
		{
			// Don't start the normal replay logic of RBR
			g_pRBRGameMode->gameMode = 0x0A;
			//g_pRBRGameModeExt->gameModeExt = 0x04;
		}

		if (g_pRBRGameMode->gameMode == 0x0A && g_pRBRGameModeExt->gameModeExt == 0x01 && g_pRBRPlugin->m_iCustomReplayState == 1)
		{		
			g_pRBRPlugin->m_iCustomReplayState = 2;
			g_pRBRPlugin->m_tCustomReplayStateStartTime = std::chrono::steady_clock::now();
		}
		else if (g_pRBRGameMode->gameMode == 0x0A && g_pRBRGameModeExt->gameModeExt == 0x01 && g_pRBRPlugin->m_iCustomReplayState == 3)
		{
			g_pRBRPlugin->m_iCustomReplayState = 4;
			g_pRBRPlugin->m_tCustomReplayStateStartTime = std::chrono::steady_clock::now();

			g_pRBRGameMode->gameMode = 0x0A;
			g_pRBRGameModeExt->gameModeExt = 0x04;

			g_pRBRCameraInfo = g_pRBRCarInfo->pCamera->pCameraInfo;
			g_pRBRCarMovement = (PRBRCarMovement) * (DWORD*)(0x008EF660);

			g_pRBRCameraInfo->cameraType = 3;

			// TODO. Read from INI file
			// Set car and camera position to create a cool car preview image
			g_pRBRCameraInfo->camOrientation.x = 0.664824f;
			g_pRBRCameraInfo->camOrientation.y = -0.747000f;
			g_pRBRCameraInfo->camOrientation.z = 0.000000f;
			g_pRBRCameraInfo->camPOV1.x = 0.699783f;
			g_pRBRCameraInfo->camPOV1.y = 0.622800f;
			g_pRBRCameraInfo->camPOV1.z = -0.349891f;
			g_pRBRCameraInfo->camPOV2.x = 0.261369f;
			g_pRBRCameraInfo->camPOV2.y = 0.232616f;
			g_pRBRCameraInfo->camPOV2.z = 0.936790f;
			g_pRBRCameraInfo->camPOS.x = -4.000000f;
			g_pRBRCameraInfo->camPOS.y = -4.559966f;
			g_pRBRCameraInfo->camPOS.z = 2.000000f;
			g_pRBRCameraInfo->camFOV = 75.000175f;
			g_pRBRCameraInfo->camNear = 0.150000f;

			g_pRBRCarMovement->carQuat.x = -0.017969f;
			g_pRBRCarMovement->carQuat.y = 0.008247f;
			g_pRBRCarMovement->carQuat.z = 0.982173f;
			g_pRBRCarMovement->carQuat.w = -0.186811f;
			g_pRBRCarMovement->carMapLocation.m[0][0] = -0.929464f;
			g_pRBRCarMovement->carMapLocation.m[0][1] = -0.367257f;
			g_pRBRCarMovement->carMapLocation.m[0][2] = -0.032216f;
			g_pRBRCarMovement->carMapLocation.m[0][3] = 0.366664f;
			g_pRBRCarMovement->carMapLocation.m[1][0] = -0.929974f;
			g_pRBRCarMovement->carMapLocation.m[1][1] = 0.022913f;
			g_pRBRCarMovement->carMapLocation.m[1][2] = -0.038378f;
			g_pRBRCarMovement->carMapLocation.m[1][3] = 0.009485f;
			g_pRBRCarMovement->carMapLocation.m[2][0] = 0.999218f;
			g_pRBRCarMovement->carMapLocation.m[2][1] = 0.000008f;
			g_pRBRCarMovement->carMapLocation.m[2][2] = 2.000000f;
			g_pRBRCarMovement->carMapLocation.m[2][3] = 524287.968750f;
			g_pRBRCarMovement->carMapLocation.m[3][0] = 7.236033f;
			g_pRBRCarMovement->carMapLocation.m[3][1] = 149.234146f;
			g_pRBRCarMovement->carMapLocation.m[3][2] = 0.287426f;
			g_pRBRCarMovement->carMapLocation.m[3][3] = -1.453711f;
		}
		else if (g_pRBRPlugin->m_iCustomReplayState == 6)
		{
			// Screenshot taken. Prepare the next car for a screenshot
			g_pRBRPlugin->m_bCustomReplayShowCroppingRect = false;
			g_pRBRCarInfo->stageStartCountdown = 7.0f;

			g_pRBRPlugin->m_iCustomReplayState = 0;
			g_pRBRPlugin->m_iCustomReplayCarID = g_pRBRPlugin->GetNextScreenshotCarID(g_pRBRPlugin->m_iCustomReplayCarID);

			if (g_pRBRPlugin->m_iCustomReplayCarID >= 0)
			{
				if (CNGPCarMenu::PrepareScreenshotReplayFile(g_pRBRPlugin->m_iCustomReplayCarID))
					g_pRBRPlugin->m_iCustomReplayState = 1;
			}

			::RBRAPI_Replay(g_pRBRPlugin->m_sRBRRootDir, C_REPLAYFILENAME_SCREENSHOT);
		}
	}

	return hResult;
}


//----------------------------------------------------------------------------------------------------------------------------
// D3D9 EndScene handler. 
//
HRESULT __fastcall CustomRBRDirectXEndScene(void* objPointer)
{
	HRESULT hResult;

	if (!g_bRBRHooksInitialized) 
		return S_OK;

	if (g_pRBRGameMode->gameMode == 03)
	{ 
		int posX;
		int posY;
		int iFontHeight;

		if (g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_QUICKRALLY_CARS]
			|| g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P1]
			|| g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P2]
			|| g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P3]
			|| g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P4]
			|| g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_RBRCHALLENGE_CARS]
			)
		{
			// Show custom car details in "SelectCar" menu

			int selectedCarIdx = g_pRBRMenuSystem->currentMenuObj->selectedItemIdx - g_pRBRMenuSystem->currentMenuObj->firstSelectableItemIdx;

			if (selectedCarIdx >= 0 && selectedCarIdx <= 7)
			{
				//float rbrPosX;
				if (g_pRBRPlugin->m_carPreviewTexture[selectedCarIdx].pTexture == nullptr && g_pRBRPlugin->m_carPreviewTexture[selectedCarIdx].imgSize.cx >= 0 && g_RBRCarSelectionMenuEntry[selectedCarIdx].wszCarModel[0] != '\0')
				{
					float posYf;
					float cx, cy;

					RBRAPI_MapRBRPointToScreenPoint(0.0f, g_pRBRMenuSystem->menuImagePosY - 1.0f, nullptr, &posYf);

					if (g_pRBRPlugin->m_carPictureScale == -1)
					{
						// The old default behaviour when CarPictureScale is not set. The image is drawn using the original size without scaling and stretching
						cx = cy = 0.0f;
					}
					else
					{
						// Define the exact drawing area rect and optionally scale the picture within the rect or stretch it to fill the area
						if(g_pRBRPlugin->m_carSelectRightBlackBarRect.right != 0)
							cx = (float)(g_pRBRPlugin->m_carSelectRightBlackBarRect.right - g_pRBRPlugin->m_carSelectLeftBlackBarRect.left);
						else
							cx = (float)(g_rectRBRWndClient.right - g_pRBRPlugin->m_carSelectLeftBlackBarRect.left);

						if (g_pRBRPlugin->m_carSelectRightBlackBarRect.bottom != 0)
							cy = (float)(g_pRBRPlugin->m_carSelectLeftBlackBarRect.bottom - posYf);
						else
							cy = (float)(g_rectRBRWndClient.bottom - posYf);
					}

					g_pRBRPlugin->ReadCarPreviewImageFromFile(selectedCarIdx, 
						(float)g_pRBRPlugin->m_carSelectLeftBlackBarRect.left, 
						posYf, 
						cx, cy, //0, 0, 
						&g_pRBRPlugin->m_carPreviewTexture[selectedCarIdx], 
						(g_pRBRPlugin->m_carPictureScale == -1 ? 0 : g_pRBRPlugin->m_carPictureScale),
						false);
				}

				// If the car preview image is successfully initialized (imgSize.cx >= 0) and texture (=image) is prepared then draw it on the screen
				if (g_pRBRPlugin->m_carPreviewTexture[selectedCarIdx].imgSize.cx >= 0 && g_pRBRPlugin->m_carPreviewTexture[selectedCarIdx].pTexture != nullptr)
				{
					D3DRECT rec;

					// Draw black side bars (if set in INI file)
					rec.x1 = g_pRBRPlugin->m_carSelectLeftBlackBarRect.left;
					rec.y1 = g_pRBRPlugin->m_carSelectLeftBlackBarRect.top;
					rec.x2 = g_pRBRPlugin->m_carSelectLeftBlackBarRect.right;
					rec.y2 = g_pRBRPlugin->m_carSelectLeftBlackBarRect.bottom;
					g_pRBRIDirect3DDevice9->Clear(1, &rec, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 0, 0, 0), 0, 0);

					rec.x1 = g_pRBRPlugin->m_carSelectRightBlackBarRect.left;
					rec.y1 = g_pRBRPlugin->m_carSelectRightBlackBarRect.top;
					rec.x2 = g_pRBRPlugin->m_carSelectRightBlackBarRect.right;
					rec.y2 = g_pRBRPlugin->m_carSelectRightBlackBarRect.bottom;
					g_pRBRIDirect3DDevice9->Clear(1, &rec, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 0, 0, 0), 0, 0);

					// Draw car preview image (use transparent alpha channel bits if those are set in PNG file)
					if (g_pRBRPlugin->m_carPictureUseTransparent)
					{
						g_pRBRPlugin->m_pD3D9RenderStateCache->SetRenderState(D3DRS_ALPHAREF, (DWORD)0x0000008f);
						g_pRBRPlugin->m_pD3D9RenderStateCache->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
						g_pRBRPlugin->m_pD3D9RenderStateCache->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
						g_pRBRPlugin->m_pD3D9RenderStateCache->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
						g_pRBRPlugin->m_pD3D9RenderStateCache->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
					}
					D3D9DrawVertexTex2D(g_pRBRIDirect3DDevice9, g_pRBRPlugin->m_carPreviewTexture[selectedCarIdx].pTexture, g_pRBRPlugin->m_carPreviewTexture[selectedCarIdx].vertexes2D);					

					if (g_pRBRPlugin->m_carPictureUseTransparent) 
						g_pRBRPlugin->m_pD3D9RenderStateCache->RestoreState();
				}


				//
				// Car 3D Model info textbo
				//
				iFontHeight = g_pFontCarSpecCustom->GetTextHeight();

				//RBRAPI_MapRBRPointToScreenPoint(rbrPosX, g_pRBRMenuSystem->menuImagePosY, &posX, &posY);
				RBRAPI_MapRBRPointToScreenPoint(218.0f, g_pRBRMenuSystem->menuImagePosY, &posX, &posY);
				posY -= 6 * iFontHeight;

				//if (g_pRBRPlugin->m_car3DModelInfoPosition.x != 0)
				//	posX = g_pRBRPlugin->m_car3DModelInfoPosition.x;  // Custom X-position
				//else if (rbrPosX == -1)
				//	posX = ((g_rectRBRWndClient.right - g_rectRBRWndClient.left) / 2) - 50; // Default brute-force "center of the horizontal screen line"

				if (g_pRBRPlugin->m_car3DModelInfoPosition.x != 0)
					posX = g_pRBRPlugin->m_car3DModelInfoPosition.x;  // Custom X-position

				if (g_pRBRPlugin->m_car3DModelInfoPosition.y != 0)
					posY = g_pRBRPlugin->m_car3DModelInfoPosition.y;  // Custom Y-position

				PRBRCarSelectionMenuEntry pCarSelectionMenuEntry = &g_RBRCarSelectionMenuEntry[selectedCarIdx];

				// Printout custom carSpec information (if the text line/value is not null string)
				int iCarSpecPrintRow = 0;
				if (pCarSelectionMenuEntry->wszCarPhysicsRevision[0] != L'\0')
					g_pFontCarSpecCustom->DrawText(posX, (iCarSpecPrintRow++) * iFontHeight + posY, C_CARSPECTEXT_COLOR, pCarSelectionMenuEntry->wszCarPhysicsRevision, 0);

				if (pCarSelectionMenuEntry->wszCarPhysicsSpecYear[0] != L'\0')
					g_pFontCarSpecCustom->DrawText(posX, (iCarSpecPrintRow++) * iFontHeight + posY, C_CARSPECTEXT_COLOR, pCarSelectionMenuEntry->wszCarPhysicsSpecYear, 0);

				if (pCarSelectionMenuEntry->wszCarPhysics3DModel[0] != L'\0')
					g_pFontCarSpecCustom->DrawText(posX, (iCarSpecPrintRow++) * iFontHeight + posY, C_CARSPECTEXT_COLOR, pCarSelectionMenuEntry->wszCarPhysics3DModel, 0);

				if (pCarSelectionMenuEntry->wszCarPhysicsLivery[0] != L'\0')
					g_pFontCarSpecCustom->DrawText(posX, (iCarSpecPrintRow++) * iFontHeight + posY, C_CARSPECTEXT_COLOR, pCarSelectionMenuEntry->wszCarPhysicsLivery, 0);

				if (pCarSelectionMenuEntry->wszCarPhysicsCustomTxt[0] != L'\0')
					g_pFontCarSpecCustom->DrawText(posX, (iCarSpecPrintRow++) * iFontHeight + posY, C_CARSPECTEXT_COLOR, pCarSelectionMenuEntry->wszCarPhysicsCustomTxt, 0);

				if (pCarSelectionMenuEntry->wszCarFMODBank[0] != L'\0')
					g_pFontCarSpecCustom->DrawText(posX, (iCarSpecPrintRow++) * iFontHeight + posY, C_CARSPECTEXT_COLOR, pCarSelectionMenuEntry->wszCarFMODBank, 0);
			}
		}
		else if(g_pRBRPlugin->m_iMenuRBRTMOption == 1)
		{
			//
			// RBRTM integration enabled. Check if RMRTB is in "SelectCar" menu in Shakedown or OnlineTournament menus
			//

			bool bRBRTMCarSelectionMenu = false;

			if (g_pRBRPluginMenuSystem->customPluginMenuObj != nullptr && g_pRBRPlugin->m_pRBRTMPlugin != nullptr && g_pRBRPlugin->m_pRBRTMPlugin->pCurrentRBRTMMenuObj != nullptr)
			{
				if (!g_pRBRPlugin->m_bRBRTMPluginActive && g_pRBRMenuSystem->currentMenuObj == g_pRBRPluginMenuSystem->customPluginMenuObj && g_pRBRPluginMenuSystem->pluginsMenuObj->selectedItemIdx == g_pRBRPlugin->m_iRBRTMPluginMenuIdx)
					// RBRTM plugin activated via RBR Plugins in-game menu
					g_pRBRPlugin->m_bRBRTMPluginActive = true;
				else if (g_pRBRPlugin->m_bRBRTMPluginActive && (g_pRBRMenuSystem->currentMenuObj == g_pRBRPluginMenuSystem->optionsMenuObj || g_pRBRMenuSystem->currentMenuObj == g_pRBRPluginMenuSystem->pluginsMenuObj || g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_MAIN]) )
					// Menu is back in RBR Plugins/Options/Main menu, so RBRTM cannot be the foreground plugin anymore
					g_pRBRPlugin->m_bRBRTMPluginActive = false;

				// If the active custom plugin is RBRTM and RBR shows a custom plugin menuObj and the internal RBRTM menu is the Shakedown "car selection" menuID then prepare to render a car preview image
				if (g_pRBRPlugin->m_bRBRTMPluginActive && g_pRBRPluginMenuSystem->customPluginMenuObj == g_pRBRMenuSystem->currentMenuObj && g_pRBRPlugin->m_pRBRTMPlugin->pCurrentRBRTMMenuObj != nullptr)
				{
					switch ((DWORD)*g_pRBRPlugin->m_pRBRTMPlugin->pCurrentRBRTMMenuObj)
					{
						case 0x15978614: bRBRTMCarSelectionMenu = true; break; // RBRTM car selection screen (either below Shakedown or OnlineTournament menu tree)

						case 0x159785F8: g_pRBRPlugin->m_iRBRTMCarSelectionType = 0; break; // Back to RBRTM main menu, so the RBRTM menu is no longer below Shakedown or OnlineTournament menu tree

						case 0x159786F8:
						case 0x15978684: g_pRBRPlugin->m_iRBRTMCarSelectionType = 1; break; // RBRTM OnlineTournament menu tree

						case 0x15978154:
						case 0x159786A0: g_pRBRPlugin->m_iRBRTMCarSelectionType = 2; break; // RBRTM Shakedown menu tree
					}

					if (g_pRBRPlugin->m_pRBRTMPlugin->pRBRTMStageOptions1 == nullptr || g_pRBRPlugin->m_pRBRTMPlugin->pRBRTMStageOptions1->selectedCarID < 0 || g_pRBRPlugin->m_pRBRTMPlugin->pRBRTMStageOptions1->selectedCarID > 7)
						bRBRTMCarSelectionMenu = false;
				}

				if (bRBRTMCarSelectionMenu && (g_pRBRPlugin->m_iRBRTMCarSelectionType == 2 || (g_pRBRPlugin->m_iRBRTMCarSelectionType == 1 && g_pRBRPlugin->m_pRBRTMPlugin->selectedItemIdx == 1)))
				{		
					int iCarSpecPrintRow;
					int selectedCarIdx = ::RBRAPI_MapCarIDToMenuIdx(g_pRBRPlugin->m_pRBRTMPlugin->pRBRTMStageOptions1->selectedCarID);
					PRBRCarSelectionMenuEntry pCarSelectionMenuEntry = &g_RBRCarSelectionMenuEntry[selectedCarIdx];

					iFontHeight = g_pFontCarSpecCustom->GetTextHeight();

					if (g_pRBRPlugin->m_carRBRTMPreviewTexture[selectedCarIdx].pTexture == nullptr && g_pRBRPlugin->m_carRBRTMPreviewTexture[selectedCarIdx].imgSize.cx >= 0 && g_RBRCarSelectionMenuEntry[selectedCarIdx].wszCarModel[0] != '\0')
					{
						g_pRBRPlugin->ReadCarPreviewImageFromFile(selectedCarIdx, 
							(float)g_pRBRPlugin->m_carRBRTMPictureRect.left, (float)g_pRBRPlugin->m_carRBRTMPictureRect.top, 
							(float)(g_pRBRPlugin->m_carRBRTMPictureRect.right - g_pRBRPlugin->m_carRBRTMPictureRect.left), 
							(float)(g_pRBRPlugin->m_carRBRTMPictureRect.bottom - g_pRBRPlugin->m_carRBRTMPictureRect.top),
							&g_pRBRPlugin->m_carRBRTMPreviewTexture[selectedCarIdx],
							g_pRBRPlugin->m_carRBRTMPictureScale /*IMAGE_TEXTURE_SCALE_PRESERVE_ASPECTRATIO | IMAGE_TEXTURE_POSITION_BOTTOM*/,
							true);
					}

					// If the car preview image is successfully initialized (imgSize.cx >= 0) and texture (=image) is prepared then draw it on the screen
					if (g_pRBRPlugin->m_carRBRTMPreviewTexture[selectedCarIdx].imgSize.cx >= 0 && g_pRBRPlugin->m_carRBRTMPreviewTexture[selectedCarIdx].pTexture != nullptr)
					{
						iCarSpecPrintRow = 0;				

						if (g_pRBRPlugin->m_carRBRTMPictureUseTransparent)
						{
							g_pRBRPlugin->m_pD3D9RenderStateCache->SetRenderState(D3DRS_ALPHAREF, (DWORD)0x0000008f);
							g_pRBRPlugin->m_pD3D9RenderStateCache->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
							g_pRBRPlugin->m_pD3D9RenderStateCache->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
							g_pRBRPlugin->m_pD3D9RenderStateCache->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
							g_pRBRPlugin->m_pD3D9RenderStateCache->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
						}
						D3D9DrawVertexTex2D(g_pRBRIDirect3DDevice9, g_pRBRPlugin->m_carRBRTMPreviewTexture[selectedCarIdx].pTexture, g_pRBRPlugin->m_carRBRTMPreviewTexture[selectedCarIdx].vertexes2D);

						if (g_pRBRPlugin->m_carRBRTMPictureUseTransparent)
							g_pRBRPlugin->m_pD3D9RenderStateCache->RestoreState();

						// 3D model and custom livery text is drawn on top of the car preview image (bottom left corner)
						if (pCarSelectionMenuEntry->wszCarPhysicsLivery[0] != L'\0')
							g_pFontCarSpecCustom->DrawText(g_pRBRPlugin->m_carRBRTMPictureRect.left + 2, g_pRBRPlugin->m_carRBRTMPictureRect.bottom - ((++iCarSpecPrintRow) * iFontHeight) - 4, C_CARSPECTEXT_COLOR, pCarSelectionMenuEntry->wszCarPhysicsLivery, 0);

						if (pCarSelectionMenuEntry->wszCarPhysics3DModel[0] != L'\0')
							g_pFontCarSpecCustom->DrawText(g_pRBRPlugin->m_carRBRTMPictureRect.left + 2, g_pRBRPlugin->m_carRBRTMPictureRect.bottom - ((++iCarSpecPrintRow) * iFontHeight) - 4, C_CARSPECTEXT_COLOR, pCarSelectionMenuEntry->wszCarPhysics3DModel, 0);
					}

					// FIACategory, HP, Year, Weight, Transmission, AudioFMOD
					iCarSpecPrintRow = 0;
					//posX = g_pRBRPlugin->m_carRBRTMPictureRect.right + 16;
					RBRAPI_MapRBRPointToScreenPoint(460.0f, 0.0f, &posX, nullptr);

					if (pCarSelectionMenuEntry->wszCarFMODBank[0] != L'\0')
						g_pFontCarSpecCustom->DrawText(posX, g_pRBRPlugin->m_carRBRTMPictureRect.bottom - ((++iCarSpecPrintRow) * iFontHeight) - 4, C_CARSPECTEXT_COLOR, pCarSelectionMenuEntry->wszCarFMODBank, 0);

					if (pCarSelectionMenuEntry->wszCarYear[0] != L'\0')
						g_pFontCarSpecCustom->DrawText(posX, g_pRBRPlugin->m_carRBRTMPictureRect.bottom - ((++iCarSpecPrintRow) * iFontHeight) - 4, C_CARSPECTEXT_COLOR, pCarSelectionMenuEntry->wszCarYear, 0);

					if (pCarSelectionMenuEntry->wszCarTrans[0] != L'\0')
						g_pFontCarSpecCustom->DrawText(posX, g_pRBRPlugin->m_carRBRTMPictureRect.bottom - ((++iCarSpecPrintRow) * iFontHeight) - 4, C_CARSPECTEXT_COLOR, pCarSelectionMenuEntry->wszCarTrans, 0);

					if (pCarSelectionMenuEntry->wszCarWeight[0] != L'\0')
						g_pFontCarSpecCustom->DrawText(posX, g_pRBRPlugin->m_carRBRTMPictureRect.bottom - ((++iCarSpecPrintRow) * iFontHeight) - 4, C_CARSPECTEXT_COLOR, pCarSelectionMenuEntry->wszCarWeight, 0);

					if (pCarSelectionMenuEntry->wszCarPower[0] != L'\0')
						g_pFontCarSpecCustom->DrawText(posX, g_pRBRPlugin->m_carRBRTMPictureRect.bottom - ((++iCarSpecPrintRow) * iFontHeight) - 4, C_CARSPECTEXT_COLOR, pCarSelectionMenuEntry->wszCarPower, 0);

					if (pCarSelectionMenuEntry->szCarCategory[0] != L'\0')
						g_pFontCarSpecCustom->DrawText(posX, g_pRBRPlugin->m_carRBRTMPictureRect.bottom - ((++iCarSpecPrintRow) * iFontHeight) - 4, C_CARSPECTEXT_COLOR, pCarSelectionMenuEntry->szCarCategory, 0);

					if (pCarSelectionMenuEntry->wszCarModel[0] != L'\0')
						g_pFontCarSpecCustom->DrawText(posX, g_pRBRPlugin->m_carRBRTMPictureRect.bottom - ((++iCarSpecPrintRow) * iFontHeight) - 4, C_CARMODELTITLETEXT_COLOR, pCarSelectionMenuEntry->wszCarModel, 0);

				}
			}
		}


		// Draw images set by custom plugins (if the custom plugin is activated)
		if (g_pRBRPluginIntegratorLinkList != nullptr)
		{
			for (auto& item : *g_pRBRPluginIntegratorLinkList)
			{
				if (item->m_bCustomPluginActive)
				{
					for (auto& imageItem : item->m_imageList)
					{
						if (imageItem->m_bShowImage && imageItem->m_imageTexture.pTexture != nullptr && imageItem->m_imageSize.cx != -1)
						{
							if (imageItem->m_dwImageFlags & IMAGE_TEXTURE_ALPHA_BLEND)
							{
								g_pRBRPlugin->m_pD3D9RenderStateCache->SetRenderState(D3DRS_ALPHAREF, (DWORD)0x0000008f);
								g_pRBRPlugin->m_pD3D9RenderStateCache->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
								g_pRBRPlugin->m_pD3D9RenderStateCache->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
								g_pRBRPlugin->m_pD3D9RenderStateCache->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
								g_pRBRPlugin->m_pD3D9RenderStateCache->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
							}

							D3D9DrawVertexTex2D(g_pRBRIDirect3DDevice9, imageItem->m_imageTexture.pTexture, imageItem->m_imageTexture.vertexes2D);

							if (imageItem->m_dwImageFlags & IMAGE_TEXTURE_ALPHA_BLEND)
								g_pRBRPlugin->m_pD3D9RenderStateCache->RestoreState();
						}
					}
				}
			}
		}
	}
	else if (g_pRBRPlugin->m_bCustomReplayShowCroppingRect && g_pRBRPlugin->m_iCustomReplayState >= 2 && g_pRBRPlugin->m_iCustomReplayState != 4)
	{
		// Draw rectangle to highlight the screenshot capture area (except when state == 4 because then this plugin takes the car preview screenshot)
		D3D9DrawVertex2D(g_pRBRIDirect3DDevice9, g_pRBRPlugin->m_screenshotCroppingRectVertexBuffer);
	}

#if USE_DEBUG == 1
	WCHAR szTxtBuffer[200];

	RECT wndRect;
	RECT wndClientRect;
	RECT wndMappedRect;
	D3DDEVICE_CREATION_PARAMETERS creationParameters;
	g_pRBRIDirect3DDevice9->GetCreationParameters(&creationParameters);
	GetWindowRect(creationParameters.hFocusWindow, &wndRect);
	GetClientRect(creationParameters.hFocusWindow, &wndClientRect);
	CopyRect(&wndMappedRect, &wndClientRect);
	MapWindowPoints(creationParameters.hFocusWindow, NULL, (LPPOINT)&wndMappedRect, 2);

	//swprintf_s(szTxtBuffer, COUNT_OF_ITEMS(szTxtBuffer), L"Mode %d %d.  Img (%f,%f)(%f,%f)  Timer=%f", g_pRBRGameMode->gameMode, g_pRBRGameModeExt->gameModeExt, g_pRBRMenuSystem->menuImagePosX, g_pRBRMenuSystem->menuImagePosY, g_pRBRMenuSystem->menuImageWidth, g_pRBRMenuSystem->menuImageHeight, g_pRBRCarInfo->stageStartCountdown);
	swprintf_s(szTxtBuffer, COUNT_OF_ITEMS(szTxtBuffer), L"Mode %d %d.  Img (%f,%f)(%f,%f)  hWnd=%x", g_pRBRGameMode->gameMode, g_pRBRGameModeExt->gameModeExt, g_pRBRMenuSystem->menuImagePosX, g_pRBRMenuSystem->menuImagePosY, g_pRBRMenuSystem->menuImageWidth, g_pRBRMenuSystem->menuImageHeight, (int)creationParameters.hFocusWindow);
	g_pFontDebug->DrawText(1, 1 * 20, C_DEBUGTEXT_COLOR, szTxtBuffer, D3DFONT_CLEARTARGET);

/*
	int x, y;
	RBRAPI_MapRBRPointToScreenPoint(451.0f, 244.0f, &x, &y);
	g_pFontDebug->DrawText(x, y, C_DEBUGTEXT_COLOR, "XX", D3DFONT_CLEARTARGET);
	swprintf_s(szTxtBuffer, COUNT_OF_ITEMS(szTxtBuffer), L"(451,244)=(%d,%d)", x, y);
	g_pFontDebug->DrawText(x, y + (20 * 1), C_DEBUGTEXT_COLOR, szTxtBuffer, D3DFONT_CLEARTARGET);
*/

#endif

	// Call original RBR DXEndScene function and let it to do whatever needed to complete the drawing of DX framebuffer
	hResult = Func_OrigRBRDirectXEndScene(objPointer);
	if (g_pRBRPlugin->m_iCustomReplayState <= 0) 
		return hResult;


	//
	// Custom "screenshot generation replay" process running.
	// State machine 1=Preparing (replay not yet loaded)
	//       2=Replay loaded, camera spinning around the car and blackout fading out
	//       3=The startup blackout has faded out. Prepare to move the car and cam to a custom location
	//       4=Car and cam moved to a custom location. Prepare to take a screenshot (wait 0.5s before taking the shot)
	//       5=Screenshot taken. If this was the first car then show the cropping rect for few secs (easier to check that the cropping rect is in the correct location)
	//       6=Ending the screenshot state cycle. End or start all over again with a new car
	//
	if (g_pRBRPlugin->m_iCustomReplayState == 2)
	{
		std::chrono::steady_clock::time_point tCustomReplayStateNowTime = std::chrono::steady_clock::now();
		auto iTimeElapsedSec = std::chrono::duration_cast<std::chrono::milliseconds>(tCustomReplayStateNowTime - g_pRBRPlugin->m_tCustomReplayStateStartTime).count();
		if (iTimeElapsedSec >= 1200)
			g_pRBRPlugin->m_iCustomReplayState = 3;
	}
	else if (g_pRBRPlugin->m_iCustomReplayState == 4)
	{
		std::chrono::steady_clock::time_point tCustomReplayStateNowTime = std::chrono::steady_clock::now();
		auto iTimeElapsedSec = std::chrono::duration_cast<std::chrono::milliseconds>(tCustomReplayStateNowTime - g_pRBRPlugin->m_tCustomReplayStateStartTime).count();
		if (iTimeElapsedSec >= 500)
		{
			// Take a RBR car preview screenshot and save it as PNG preview file.
			// At this point the cropping highlight rectangle is hidden, so it is not shown in the screenshot.
			//std::string imgExtension = ".";
			//imgExtension = imgExtension + g_NGPCarMenu_ImageOptions[g_pRBRPlugin->m_iMenuImageOption];  // .PNG or .BMP img file format
			//_ToLowerCase(imgExtension);
			//std::wstring outputFileName = g_pRBRPlugin->m_screenshotPath + L"\\" + g_RBRCarSelectionMenuEntry[RBRAPI_MapCarIDToMenuIdx(g_pRBRPlugin->m_iCustomReplayCarID)].wszCarModel + _ToWString(imgExtension);			
			std::wstring outputFileName = g_pRBRPlugin->ReplacePathVariables(g_pRBRPlugin->m_screenshotPath, RBRAPI_MapCarIDToMenuIdx(g_pRBRPlugin->m_iCustomReplayCarID), false);

			D3D9SaveScreenToFile((g_pRBRPlugin->m_screenshotAPIType == C_SCREENSHOTAPITYPE_DIRECTX ? g_pRBRIDirect3DDevice9 : nullptr), 
				g_hRBRWnd, g_pRBRPlugin->m_screenshotCroppingRect, outputFileName);

			g_pRBRPlugin->m_iCustomReplayScreenshotCount++;
			g_pRBRPlugin->m_iCustomReplayState = 5;
			g_pRBRPlugin->m_tCustomReplayStateStartTime = std::chrono::steady_clock::now();
		}
	}
	else if (g_pRBRPlugin->m_iCustomReplayState == 5)
	{
		std::chrono::steady_clock::time_point tCustomReplayStateNowTime = std::chrono::steady_clock::now();
		auto iTimeElapsedSec = std::chrono::duration_cast<std::chrono::milliseconds>(tCustomReplayStateNowTime - g_pRBRPlugin->m_tCustomReplayStateStartTime).count();
		if (g_pRBRPlugin->m_iCustomReplayScreenshotCount > 1 || iTimeElapsedSec >= 3000)
			g_pRBRPlugin->m_iCustomReplayState = 6; // Completing the screenshot state for this carID
	}

	return hResult;
}
