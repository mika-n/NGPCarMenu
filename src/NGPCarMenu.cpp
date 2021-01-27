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

#include <set>					// std::set
#include <filesystem>			// fs::directory_iterator
#include <fstream>				// std::ifstream
#include <sstream>				// std::stringstream
#include <chrono>				// std::chrono::steady_clock

#include <locale>				// UTF8 locales
#include <codecvt>

#include <wincodec.h>			// GUID_ContainerFormatPng 

#include <xinput.h>				// xinput controller inputs DEBUG to split xbox gamepad axis
//#pragma comment(lib, "Xinput")			// xinput1_4.dll
#pragma comment(lib, "xinput9_1_0")	// xinput1_3.dll

#include "NGPCarMenu.h"
//#include "FileWatcher.h"

namespace fs = std::filesystem;

//------------------------------------------------------------------------------------------------
// Global "RBR plugin" variables (yes, globals are not always the recommended way to do it, but RBR plugin has only one instance of these variables
// and sometimes class member method and ordinary functions should have an easy access to these variables).
//

BOOL g_bRBRHooksInitialized = FALSE; // TRUE-RBR main memory and DX9 function hooks initialized. Ready to rock! FALSE=Hooks and variables not yet initialized.

tRBRDirectXBeginScene Func_OrigRBRDirectXBeginScene = nullptr;  // Re-routed built-in DX9 RBR function pointers
tRBRDirectXEndScene   Func_OrigRBRDirectXEndScene = nullptr;

tRBRReplay            Func_OrigRBRReplay = nullptr;				// Re-routed RBR replay class method
tRBRSaveReplay        Func_OrigRBRSaveReplay = nullptr;		    // 

tRBRControllerAxisData Func_OrigRBRControllerAxisData = nullptr;// Controller axis data, the custom method is used to fix inverted pedal RBR bug

tRBRCallForHelp Func_OrigRBRCallForHelp = nullptr; // CallForHelp handler

//------------------------------------------------------------------------------------------------

wchar_t* g_pOrigLoadReplayStatusText = nullptr;        // The original (localized) "Loading Replay" text
wchar_t  g_wszCustomLoadReplayStatusText[256] = L"\0"; // Custom LoadReplay text (Loading Replay <rpl file name>)

wchar_t* g_pOrigRallySchoolMenuText = nullptr;			  // The original (localized) "Rally School" menu text (could be that the menu is replacemend with custom menu name)
wchar_t  g_wszRallySchoolMenuReplacementText[128] = L"Rally School"; // Custom rallySchool menu text

#if USE_DEBUG == 1
CD3DFont* g_pFontDebug = nullptr;
#endif 

CD3DFont* g_pFontCarSpecCustom = nullptr;
CD3DFont* g_pFontCarSpecModel  = nullptr;

CD3DFont* g_pFontRBRRXLoadTrack = nullptr;
CD3DFont* g_pFontRBRRXLoadTrackSpec = nullptr;

CNGPCarMenu*         g_pRBRPlugin = nullptr;			// The one and only RBRPlugin instance
PRBRPluginMenuSystem g_pRBRPluginMenuSystem = nullptr;  // Pointer to RBR plugin menu system (for some reason Plugins menu is not part of the std menu arrays)

std::vector<std::unique_ptr<CRBRPluginIntegratorLink>>* g_pRBRPluginIntegratorLinkList = nullptr;	// List of custom plugin integration definitions (other plugin can use NGPCarMenu API to draw custom images)
std::vector<std::unique_ptr<CD3DFont>>* g_pRBRPluginIntegratorFontList = nullptr;					// List of custom plugin font definitions (other plugin can use NGPCarMenu API to draw custom text using other than RBR fonts also)

bool g_bNewCustomPluginIntegrations = false;														// TRUE if there are new custom plugin integrations waiting for to be initialized


WCHAR* g_pOrigCarSpecTitleWeight = nullptr;				// The original RBR Weight and Transmission title string values
WCHAR* g_pOrigCarSpecTitleTransmission = nullptr;
WCHAR* g_pOrigCarSpecTitleHorsepower = nullptr;

std::vector<std::string>* g_pRBRRXTrackNameListAlreadyInitialized = nullptr; // List of RBRRX track folder names with a missing track.ini splashscreen option and already scanned for default JPG/PNG preview image during this RBR process life time


int g_iInvertedPedalsStartupFixFlag = 0;               // Bit1=Throttle (0x01), Bit2=Brake (0x02), Bit3=Clutch (0x04), Bit4=Handbrake (0x08). Fix the inverted pedal bug in RBR when the game is started or alt-tabbed to desktop

int g_iXInputSplitThrottleBrakeAxis = -1; // -1=disabled, 0>=Split xinput controller# left and right analog triggers (by default RBR sees xbox xinput triggers as one axis)
int g_iXInputThrottle = 1;				  // 1=Right trigger
int g_iXInputBrake = 0;                   // 0=Left trigger

float g_fControllerAxisDeadzone[] =
{   0.0f,    // idx 0 = steering (idx is the RBR control idx)
    0, 0,    // (unused)
	0.0f,    // idx 3 = throttle
	0,       // (unused)
	0.0f,    // idx 5 = brake
	0.0f,    // idx 6 = clutch
	0, 0, 0, 0, // (unused)
	0.0f     // idx 11 = handbrake
};


CppSQLite3DB* g_pRaceStatDB = nullptr;	// Race statistics database

//--------------------------------------------------------------------------------------------------------------------------
// Class to listen for new rbr\Replahs\*.rpl files after RBRRX/BTB racing has ended.
//
/*
class RBRReplayFileWatcherListener : public IFileWatcherListener
{
protected:
	std::list<std::wstring> replayFileQueue;

	void AddReplayFileToQueue(const std::wstring& fileName)
	{
		std::filesystem::path fileNamePath(fileName);
		if (_iEqual(fileNamePath.extension().wstring(), L".rpl", true))
		{
			// Add the filename if it is not yet in the list
			if (std::find(replayFileQueue.begin(), replayFileQueue.end(), fileName) == replayFileQueue.end())
				replayFileQueue.push_front(fileName);
		}
	}

public:
	virtual void OnError(const int errorCode) override
	{
		//DebugPrint(L"OnError %d", errorCode);

		// zero here is not an error, but just a notification that watcher thread has ended
		if (errorCode != FILEWATCHER_ERR_NOERROR_CLOSING)
			LogPrint("ERROR. RBRReplayFileWatcherListener.OnError %d", errorCode);
	}

	virtual void OnFileChange(const std::wstring& fileName) override
	{
		//DebugPrint(L"OnFileChange %s", fileName.c_str());
		AddReplayFileToQueue(fileName);
	}

	//virtual void OnFileAdded(const std::wstring& fileName) override
	//{
		//DebugPrint(L"OnFileAdded %s", fileName.c_str());
	//	AddReplayFileToQueue(fileName);
	//}

	//virtual void OnFileRenamed(const std::wstring& fileName) override
	//{
		//DebugPrint(L"OnFileRenamed%s", fileName.c_str());
	//	AddReplayFileToQueue(fileName);
	//}

	// Flush the queue and write out all replayFileName.ini files with the current track and car metadata
	void DoCompletion(BOOL onlyForceCleanup)
	{
		if (!onlyForceCleanup)
			g_pRBRPlugin->CompleteSaveReplayProcess(replayFileQueue);

		replayFileQueue.clear();
	}
};

CFileSystemWatcher    g_watcherNewReplayFiles;
IFileWatcherListener* g_watcherNewReplayFileListener;
*/


//--------------------------------------------------------------------------------------------------------------------------
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
	{ 0x4a0dd9, 0x4a0c59, /* Slot#5 */ "", L"car1", L"", "", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ 0x4a0dc9, 0x4a0c49, /* Slot#3 */ "", L"car2", L"", "", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ 0x4a0de1, 0x4a0c61, /* Slot#6 */ "", L"car3",	L"", "", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ 0x4a0db9, 0x4a0c39, /* Slot#1 */ "", L"car4",	L"", "", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ 0x4a0dd1, 0x4a0c51, /* Slot#4 */ "", L"car5",	L"", "", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ 0x4a0de9, 0x4a0c69, /* Slot#7 */ "", L"car6",	L"", "", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ 0x4a0db1, 0x4a0c31, /* Slot#0 */ "", L"car7",	L"", "", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ 0x4a0dc1, 0x4a0c41, /* Slot#2 */ "", L"car8",	L"", "", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"" }
};


//
// Menu item command ID and names (custom plugin menu). The ID should match the g_RBRPluginMenu array index (0...n)
//
#define C_MENUCMD_CREATEOPTION					0
#define C_MENUCMD_IMAGEOPTION					1
#define C_MENUCMD_RBRTMOPTION					2
#define C_MENUCMD_RBRRXOPTION					3
#define C_MENUCMD_AUTOLOGONOPTION				4
#define C_MENUCMD_RALLYSCHOOLMENUOPTION			5
#define C_MENUCMD_SPLITCOMBINEDTHROTTLEBRAKE	6
#define C_MENUCMD_COCKPIT_CAMERASHAKING			7
#define C_MENUCMD_COCKPIT_STEERINGWHEEL			8
#define C_MENUCMD_COCKPIT_WIPERS				9
#define C_MENUCMD_COCKPIT_WINDSCREEN			10
#define C_MENUCMD_COCKPIT_OVERRIDEFOV			11
#define C_MENUCMD_RENAMEDRIVER					12
#define C_MENUCMD_RELOAD						13
#define C_MENUCMD_CREATE						14

#if USE_DEBUG == 1
#define C_MENUCMD_SELECTCARSKIN		15
#endif 

typedef struct {
	char* szMenuName;
	char* szMenuTooltip;
} PluginMenuItemDef;
typedef PluginMenuItemDef* PPluginMenuItemDef;

PluginMenuItemDef g_NGPCarMenu_PluginMenu[] = {
{ "> Create option", nullptr }		// CreateOptions
,{"> Image option", nullptr }			// ImageOptions
,{"> RBRTM integration", nullptr }		// EnableDisableOptions
,{"> RBRRX integration", nullptr }		// EnableDisableOptions
,{"> Auto logon", "Navigates automatically to this menu when RBR is started" }				// AutoLogon option
,{"> RallySchool menu replacement", "Replaces the RallySchool menu item in the main menu with this shortcut" }   // Replace RallySchool menu entry with another menu shortcut
,{"> Split combined ThrottleBrake", "Split combined xbox triggers. See https://github.com/mika-n/NGPCarMenu/issues/15" }   // Gamepad# 1-4 (will be used as 0-3 index value) used to split combined gamepad trigger keys
,{"> Cockpit - Camera shaking", "Cockpit camera shaking. If disabled USE THE CAM_BONNET cam as non-shaking cockpit view" }  // The internal cockpit camera shaking (default behavior is to shake. When disabled then cam_internal and cam_bonnet camera values are switched)
,{"> Cockpit - Steering wheel", nullptr }  // Show or hide steeringWheel in the internal cockpit camera view (Default, Hidden, Shown)
,{"> Cockpit - Wipers", nullptr }
,{"> Cockpit - Windscreen", nullptr }
,{"> Cockpit - Override FOV", "Force override cockpit FOV value. See Plugins\\NGPCarMenu.ini CockpitOverrideFOVValue" }      // Override FOV in the internal cockpit camera view (Default, the value used to override the FOV in the car model)
,{"RENAME driver profile", "Renames the current driver name without loosing settings" } // "Rename driver MULLIGATAWNY -> SpeedRacer" in the profile to match the loaded pfXXXX.rbr profile filename (creates a new profile file. Take a backup copy of the old profile file)
,{"RELOAD settings", "Reloads Plugins\\NGPCarMenu.ini settings file" }		 // Clear cached car images to force re-loading of new images and settings
,{"CREATE car images", "Creates car preview images" }	 // Create new car images (all or only missing car iamges)

#if USE_DEBUG == 1
,{"SELECT car textures", "Selects customized car skins and options" }   // Select custom car skins and textures
#endif
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

// Enable/Disable of various options
char* g_NGPCarMenu_EnableDisableOptions[2] = {
	"Disabled"
	,"Enabled"
};

// Default/Hide/Show 
char* g_NGPCarMenu_HiddenShownOptions[3] = {
	"Default"
	,"Hidden"
	,"Shown"
};

// Default/Disabled/Enabled
char* g_NGPCarMenu_DefaultEnableDisableOptions[3] = {
	"Default"
	,"Disabled"
	,"Enabled"
};


// AutoLogon options: Disabled, Main, Plugins and N custom plugin names (dynamically looked up from the current list of RBR plugins)
std::vector<LPCSTR> g_NGPCarMenu_AutoLogonOptions;


//-----------------------------------------------------------------------------------------------------------------------------
#if USE_DEBUG == 1
void DebugDumpBufferToScreen(byte* pBuffer, int iPreOffset = 0, int iBytesToDump = 64, int posX = 850, int posY = 1)
{
	WCHAR txtBuffer[64] = { L'\0' } ;

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

CRBRPluginIntegratorLink* GetPluginIDIntegrationLink(DWORD pluginID)
{
	CRBRPluginIntegratorLink* pPluginIntegrationLink = nullptr;
	if (g_pRBRPluginIntegratorLinkList != nullptr && pluginID != 0)
	{
		for (auto& item : *g_pRBRPluginIntegratorLinkList)
		{
			if ((DWORD)item.get() == pluginID)
			{
				pPluginIntegrationLink = item.get();
				break;
			}
		}
	}
	return pPluginIntegrationLink;
}


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

	CRBRPluginIntegratorLink* pPluginIntegrationLink = ::GetPluginIDIntegrationLink(pluginID);
	if (pPluginIntegrationLink == nullptr || g_pRBRPlugin == nullptr)
		return FALSE;

	// ImageID -100 is the custom trackLoad img (pre-defined imageID, not to be used with any of the other custom images)
	if (imageID == -100)
		return g_pRBRPlugin->SetupCustomTrackLoadImage(_ToWString(szFileName), pImagePos, pImageSize, dwImageFlags);

	if (pImagePos == nullptr || pImageSize == nullptr)
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
		// Clear existing image or minimap data (only one of these defined at the same time)
		SAFE_RELEASE(pPluginLinkImage->m_imageTexture.pTexture);
		pPluginLinkImage->m_minimapData.vectMinimapPoint.clear();

		pPluginLinkImage->m_sImageFileName = sFileName;
		pPluginLinkImage->m_imagePos = *pImagePos;
		pPluginLinkImage->m_imageSize = *pImageSize;
		pPluginLinkImage->m_dwImageFlags = dwImageFlags;

		if (szFileName != nullptr)
		{
			std::string sFileExtension = fs::path(pPluginLinkImage->m_sImageFileName).extension().string();
			if (_iEqual(sFileExtension, ".trk", true))
			{
				// 
				// The image file has .TRK extension. Draw a minimap for a classic map format
				//
				try
				{
					// Img source is track map data  (ex maps\track-71.trk). Create a minimap vector graph (read split positions from DLS and the actual track layout from TRK file)
					CDrivelineSource drivelineSource;
					g_pRBRPlugin->ReadStartSplitsFinishPacenoteDistances(fs::path(pPluginLinkImage->m_sImageFileName).replace_extension(".dls"), &drivelineSource.startDistance, &drivelineSource.split1Distance, &drivelineSource.split2Distance, &drivelineSource.finishDistance);
					g_pRBRPlugin->ReadDriveline(_ToWString(pPluginLinkImage->m_sImageFileName), drivelineSource);

					pPluginLinkImage->m_minimapData.trackFolder = pPluginLinkImage->m_sImageFileName;
					pPluginLinkImage->m_minimapData.minimapRect.left = pImagePos->x;
					pPluginLinkImage->m_minimapData.minimapRect.top = pImagePos->y;
					pPluginLinkImage->m_minimapData.minimapRect.right = pImagePos->x + pImageSize->cx;
					pPluginLinkImage->m_minimapData.minimapRect.bottom = pImagePos->y + pImageSize->cy;

					g_pRBRPlugin->RescaleDrivelineToFitOutputRect(drivelineSource, pPluginLinkImage->m_minimapData);
				}
				catch (...)
				{
					pPluginLinkImage->m_imageTexture.imgSize.cx = -1;
					pPluginLinkImage->m_minimapData.vectMinimapPoint.clear();
					bRetValue = FALSE;
				}
			}
			else if (_iEqual(sFileExtension, ".ini", true))
			{
				//
				// BTB minimaps are identified by driveline.ini file name (pacenotes.ini is expected to be in the same folder)
				//
				try
				{
					// Img source is BTB driveline.ini file
					CDrivelineSource drivelineSource;
					g_pRBRPlugin->ReadDriveline(pPluginLinkImage->m_sImageFileName, drivelineSource);

					pPluginLinkImage->m_minimapData.trackFolder = pPluginLinkImage->m_sImageFileName;
					pPluginLinkImage->m_minimapData.minimapRect.left = pImagePos->x;
					pPluginLinkImage->m_minimapData.minimapRect.top = pImagePos->y;
					pPluginLinkImage->m_minimapData.minimapRect.right = pImagePos->x + pImageSize->cx;
					pPluginLinkImage->m_minimapData.minimapRect.bottom = pImagePos->y + pImageSize->cy;

					g_pRBRPlugin->RescaleDrivelineToFitOutputRect(drivelineSource, pPluginLinkImage->m_minimapData);
				}
				catch (...)
				{
					pPluginLinkImage->m_imageTexture.imgSize.cx = -1;
					pPluginLinkImage->m_minimapData.vectMinimapPoint.clear();
					bRetValue = FALSE;
				}
			}
			else			
			{
				//
				// Normal image file
				//
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
	}

	return bRetValue;
}

void APIENTRY API_ShowHideImage(DWORD pluginID, int imageID, bool showImage)
{
	CRBRPluginIntegratorLink* pPluginIntegrationLink = ::GetPluginIDIntegrationLink(pluginID);
	if (pPluginIntegrationLink != nullptr)
	{
		for (auto& imageItem : pPluginIntegrationLink->m_imageList)
		{
			if (imageItem->m_iImageID == imageID)
			{
				imageItem->m_bShowImage = showImage;
				break;
			}
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

BOOL APIENTRY API_GetVersionInfo(char* pOutTextBuffer, size_t textBufferSize, int* pOutMajor, int* pOutMinor, int* pOutPatch, int* pOutBuild)
{
	BOOL bResult; 

	// Init app path value using RBR.EXE executable path
	wchar_t  szModulePath[_MAX_PATH];
	::GetModuleFileNameW(NULL, szModulePath, COUNT_OF_ITEMS(szModulePath));
	::PathRemoveFileSpecW(szModulePath);

	std::wstring sModulePath = std::wstring(szModulePath) + L"\\Plugins\\" L"" VS_PROJECT_NAME L".dll";

	bResult = GetFileVersionInformationAsNumber(sModulePath, (UINT*)pOutMajor, (UINT*)pOutMinor, (UINT*)pOutPatch, (UINT*)pOutBuild);
	if (pOutTextBuffer != nullptr && textBufferSize >= 32)
	{
		std::string sAPIVersionInfoTextBuffer;
		sAPIVersionInfoTextBuffer = GetFileVersionInformationAsString(sModulePath);
		strncpy_s(pOutTextBuffer, textBufferSize, sAPIVersionInfoTextBuffer.c_str(), 31);
	}

	return bResult;
}


//-------------------------------------------------------------------------------------------------
// Initialize a new font for other custom plugins (all plugins share the same identical font object). Returns fontID value which is used in DrawText API functions or 0 if fails to create a font.
//
DWORD APIENTRY API_InitializeFont(const char* fontName, DWORD fontSize, DWORD fontStyle)
{
	if (fontName == nullptr || fontName[0] == '\0' || fontSize == 0)
		return 0;

	if(g_pRBRPluginIntegratorFontList == nullptr) 
		g_pRBRPluginIntegratorFontList = new std::vector<std::unique_ptr<CD3DFont>>();

	std::wstring sFontName = _ToWString(std::string(fontName));
	CD3DFont* pFont = nullptr;

	// Check if the font style already exists
	for (auto& item : *g_pRBRPluginIntegratorFontList)
	{
		if (item->IsFontIdentical(sFontName, fontSize, fontStyle))
		{
			pFont = item.get();
			break;
		}
	}

	if (pFont == nullptr)
	{
		auto newFont= std::make_unique<CD3DFont>(sFontName, fontSize, fontStyle);
		pFont = newFont.get();
		g_pRBRPluginIntegratorFontList->push_back(std::move(newFont));
	}

	return (DWORD) pFont;
}

// CHAR and WCHAR handler to add a new custom text drawing using a custom DirectX font (private method, not exported. DrawTextA and DrawTextW calls this)
BOOL API_AddLinkText(DWORD pluginID, int textID, int posX, int posY, const char* szText, const wchar_t* wszText, DWORD fontID, DWORD color, DWORD drawOptions)
{
	CRBRPluginIntegratorLink* pPluginIntegrationLink = ::GetPluginIDIntegrationLink(pluginID);

	// Unknown plugin integration. Can't do anything
	if (pPluginIntegrationLink == nullptr)
		return FALSE;

	CRBRPluginIntegratorLinkText* pPluginLinkText = nullptr;
	for (auto& item : pPluginIntegrationLink->m_textList)
	{
		if (item->m_textID == textID)
		{
			pPluginLinkText = item.get();
			break;
		}
	}

	if (pPluginLinkText == nullptr)
	{
		auto newText = std::make_unique<CRBRPluginIntegratorLinkText>();
		newText->m_textID = textID;
		pPluginLinkText = newText.get();
		pPluginIntegrationLink->m_textList.push_back(std::move(newText));
	}

	if (szText != nullptr)
	{
		pPluginLinkText->m_wsText.clear();
		pPluginLinkText->m_sText = szText;
	}
	else
	{
		pPluginLinkText->m_sText.clear();
		if (wszText == nullptr) pPluginLinkText->m_wsText.clear();
		else pPluginLinkText->m_wsText = wszText;
	}

	pPluginLinkText->m_posX = posX;
	pPluginLinkText->m_posY = posY;
	pPluginLinkText->m_dwColor = color;
	pPluginLinkText->m_dwDrawOptions = drawOptions;
	pPluginLinkText->m_fontID = fontID;

	// Initialize the font to use RBR D3D9 device
	for (auto& fontItem : *g_pRBRPluginIntegratorFontList)
	{
		if ((DWORD)fontItem.get() == fontID)
		{
			if (fontItem->getDevice() == nullptr)
			{
				fontItem->InitDeviceObjects(g_pRBRIDirect3DDevice9);
				fontItem->RestoreDeviceObjects();
			}
			break;
		}
	}

	return TRUE;
}

BOOL APIENTRY API_DrawTextA(DWORD pluginID, int textID, int posX, int posY, const char* szText, DWORD fontID, DWORD color, DWORD drawOptions)
{
	return API_AddLinkText(pluginID, textID, posX, posY, szText, nullptr, fontID, color, drawOptions);
}

BOOL APIENTRY API_DrawTextW(DWORD pluginID, int textID, int posX, int posY, const wchar_t* wszText, DWORD fontID, DWORD color, DWORD drawOptions)
{
	return API_AddLinkText(pluginID, textID, posX, posY, nullptr, wszText, fontID, color, drawOptions);
}

// Prepare to launch a rally (rallyType defines if this is a shakedown or online rally, szRallyName may be a stage name (shakedown) or name of the online rally, rallyOptions has additional options (not used, always zero at the moment)
BOOL APIENTRY API_PrepareTrackLoad(DWORD pluginID, int rallyType, LPCSTR szRallyName, DWORD rallyOptions)
{
	CRBRPluginIntegratorLink* pPluginIntegrationLink = ::GetPluginIDIntegrationLink(pluginID);
	if (pPluginIntegrationLink == nullptr || g_pRBRPlugin == nullptr)
		return FALSE;

	return g_pRBRPlugin->OnPrepareTrackLoad(pPluginIntegrationLink->m_sCustomPluginName, rallyType, szRallyName, rallyOptions);
}


// Prepare track ¤41 for a BTB track loading. Custom plugin should call m_pGame->StartGame(...) IRBR method to start a rally after calling this prepare function
BOOL APIENTRY API_PrepareBTBTrackLoad(DWORD pluginID, LPCSTR szBTBTrackName, LPCSTR szBTBTrackFolder)
{
	CRBRPluginIntegratorLink* pPluginIntegrationLink = ::GetPluginIDIntegrationLink(pluginID);

	// Unknown plugin integration. Can't do anything
	if (pPluginIntegrationLink == nullptr || g_pRBRPlugin == nullptr || szBTBTrackName == nullptr || szBTBTrackFolder == nullptr)
		return FALSE;

	return g_pRBRPlugin->RBRRX_PrepareLoadTrack(std::string(szBTBTrackName), std::string(szBTBTrackFolder));
}

BOOL APIENTRY API_CheckBTBTrackLoadStatus(DWORD /*pluginID*/, LPCSTR szBTBTrackName, LPCSTR szBTBTrackFolder)
{	
	BOOL bResult = FALSE;
	if (g_pRBRPlugin != nullptr && g_pRBRPlugin->GetActiveRacingType() == 2)
		bResult = (g_pRBRPlugin->RBRRX_CheckTrackLoadStatus(std::string(szBTBTrackName), std::string(szBTBTrackFolder), FALSE) >= 0);

	if (bResult == FALSE)
		LogPrint("The BTB track loading failed");

	return bResult;
}


//-----------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------

BOOL APIENTRY DllMain( HANDLE hModule,  DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	return TRUE;
}

IPlugin* RBR_CreatePlugin( IRBRGame* pGame )
{
//#if USE_DEBUG == 1	
	DebugClearFile();
//#endif
	DebugPrint("--------------------------------------------\nRBR_CreatePlugin");

	if (g_pRBRPlugin == nullptr) g_pRBRPlugin = new CNGPCarMenu(pGame);
	//DebugPrint("NGPCarMenu=%08x", (DWORD)g_pRBRPlugin);
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

	m_pGame = pGame;

	m_bPacenotePluginInstalled = m_bRBRFullscreenDX9 = m_bRallySimFansPluginInstalled = m_bRBRProInstalled = m_bRBRTMPluginInstalled = FALSE;

	// Init plugin title text with version tag of NGPCarMenu.dll file
	char szTxtBuf[COUNT_OF_ITEMS(C_PLUGIN_TITLE_FORMATSTR) + 32];
	if (sprintf_s(szTxtBuf, COUNT_OF_ITEMS(szTxtBuf) - 1, C_PLUGIN_TITLE_FORMATSTR, GetFileVersionInformationAsString((m_sRBRRootDirW + L"\\Plugins\\" L"" VS_PROJECT_NAME L".dll")).c_str()) <= 0)
		szTxtBuf[0] = '\0';
	m_sPluginTitle = szTxtBuf;

	m_pLangIniFile = nullptr;

	m_bFirstTimeWndInitialization = true;

	m_bMapLoadedCalled = m_bMapUnloadedCalled = false;

	m_iAutoLogonMenuState = -1;			// AutoLogon sequence is not yet run
	m_dwAutoLogonEventStartTick = 0;
	m_bShowAutoLogonProgressText = true;

	m_pTracksIniFile = nullptr;

	ZeroMemory(&m_mapRBRTMPictureRect, sizeof(m_mapRBRTMPictureRect));
	m_latestMapRBRTM.mapID = -1;
	m_recentMapsMaxCountRBRTM = 5;		// Default num of recent maps/stages on top of the RBRTM Shakedown stages menu list
	m_bRecentMapsRBRTMModified = FALSE;	
	ZeroMemory(m_minimapRBRTMPictureRect, sizeof(m_minimapRBRTMPictureRect));

	ZeroMemory(&m_mapRBRRXPictureRect, sizeof(m_mapRBRRXPictureRect));
	m_latestMapRBRRX.folderName = "";
	m_recentMapsMaxCountRBRRX = 5;		// Default num of recent maps/stages on top of the RBRRX stages menu list
	m_bRecentMapsRBRRXModified = FALSE;
	ZeroMemory(m_minimapRBRRXPictureRect, sizeof(m_minimapRBRRXPictureRect));

	m_pOrigMapMenuDataRBRTM = nullptr;
	m_pOrigMapMenuItemsRBRTM = nullptr;
	m_pCustomMapMenuRBRTM = nullptr;

	m_pOrigMapMenuItemsRBRRX = nullptr;
	m_pCustomMapMenuRBRRX = nullptr;

	m_origNumOfItemsMenuItemsRBRTM = 0;
	m_numOfItemsCustomMapMenuRBRTM = 0;

	m_origNumOfItemsMenuItemsRBRRX = 0;
	m_numOfItemsCustomMapMenuRBRRX = 0;
	m_currentCustomMapSelectedItemIdxRBRRX = 0;
	m_prevCustomMapSelectedItemIdxRBRRX = 0;
	//m_prevKeyCodeRBRRX = 0;

	m_iCarMenuNameLen = 0;
	
	m_iCustomReplayCarID = 0;
	m_iCustomReplayState = 0;
	m_iCustomReplayScreenshotCount = 0;
	m_bCustomReplayShowCroppingRect = false;

	m_iCustomSelectCarSkinState = 0;

	m_minimapVertexBuffer = nullptr;

	m_screenshotCroppingRectVertexBuffer = m_screenshotCroppingRectVertexBufferRSF = nullptr;
	m_screenshotCarPosition = 0;

	ZeroMemory(m_carPreviewTexture, sizeof(m_carPreviewTexture));
	ZeroMemory(m_carRBRTMPreviewTexture, sizeof(m_carRBRTMPreviewTexture));

	ZeroMemory(&m_screenshotCroppingRect, sizeof(m_screenshotCroppingRect));
	ZeroMemory(&m_carSelectLeftBlackBarRect, sizeof(m_carSelectLeftBlackBarRect));
	ZeroMemory(&m_carSelectRightBlackBarRect, sizeof(m_carSelectRightBlackBarRect));
	ZeroMemory(&m_car3DModelInfoPosition, sizeof(m_car3DModelInfoPosition));
	
	ZeroMemory(&m_screenshotCroppingRectRSF, sizeof(m_screenshotCroppingRectRSF));

	ZeroMemory(&m_customTrackLoadImg, sizeof(m_customTrackLoadImg));

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

	m_iMenuCurrentScreen = 0;

	m_iMenuSelection = 0;
	m_iMenuCreateOption = 0;
	m_iMenuImageOption = 0;
	m_iMenuRBRTMOption = 0;
	m_iMenuRBRRXOption = 0;
	m_iMenuAutoLogonOption = 0;	
	m_iMenuRallySchoolMenuOption = 0; 

	m_iMenuCockpitCameraShaking = m_iMenuCockpitSteeringWheel = m_iMenuCockpitWipers = m_iMenuCockpitWindscreen = 0;
	m_iMenuCockpitOverrideFOV = 0;
	m_fMenuCockpitOverrideFOVValue = 1.3050f;

	m_bAutoLogonWaitProfile = FALSE;
	
	m_bAutoExitAfterReplay = FALSE;

	m_screenshotAPIType = C_SCREENSHOTAPITYPE_DIRECTX;

	m_iRBRTMPluginMenuIdx = 0;		// Index (Nth item) to RBRTM plugin in RBR in-game Plugins menu list (0=Not yet initialized, -1=Initialized but RBRTM plugin missing or wrong version)
	m_pRBRTMPlugin = nullptr;		// Pointer to RBRTM plugin object
	m_pRBRPrevCurrentMenu = nullptr;// Previous "current menu obj" (used in RBRTM integration initialization routine)
	m_bRBRTMPluginActive = false;	// Is RBRTM plugin currently the active custom frontend plugin
	m_iRBRTMCarSelectionType = 0;   // If RBRTM is activated then is it at 1=Shakedown or 2=OnlineTournament car selection menu state

	m_pRBRRXPlugin = nullptr;		// Pointer to RBRRX plugin object
	m_iRBRRXPluginMenuIdx = 0;		// Index (Nth item) to RBRRX plugin in RBR in-game Plugins menu list (0=Not yet initialized, -1=Initialized but not found, >0=Initialized and found)
	m_bRBRRXPluginActive = false;

	m_bRBRRXReplayActive = false;
	m_bRBRRXRacingActive = false;
	//m_bRBRRXReplayOrRacingEnding = false;

	m_bRBRRXLoadingNewTrack = true;

	m_bRBRReplayOrRacingEnding = false;
	m_bRBRRacingActive = false;
	m_bRBRReplayActive = false;
	m_iRBRCustomRaceType = 0;
	m_dwRBRCustomRallyOptions = 0;

	g_mapRBRRXRightBlackBarVertexBuffer = nullptr;

	m_bShowCustomLoadTrackScreenRBRRX = true;

	m_bRenameDriverNameActive = FALSE;
	m_iProfileMenuPrevSelectedIdx = 0;

	m_latestMapID = m_latestCarID = -1;
	m_latestFalseStartPenaltyTime = m_latestOtherPenaltyTime = m_prevRaceTimeClock = 0.0f;
	m_latestCallForHelpCount = 0;
	m_latestCallForHelpTick = 0;

	m_bGenerateReplayMetadataFile = TRUE;

	m_rbrWindowPosition.x = m_rbrWindowPosition.y = -1;
	m_recentResultsPosition.x = m_recentResultsPosition.y = m_recentResultsPosition_RBRRX.x = m_recentResultsPosition_RBRRX.y = m_recentResultsPosition_RBRTM.x = m_recentResultsPosition_RBRTM.y = 0;
	m_rbrLatestStageResultsBackground = nullptr;

	m_iPhysicsNGMajorVer = m_iPhysicsNGMinorVer = m_iPhysicsNGPatchVer = m_iPhysicsNGBuildVer = 0;

	m_bOrigPacenotes = m_bOrig3DPacenotes = m_bOrigPacenoteDistanceCountdown = TRUE;
	m_iOrigPacenoteStack = 0;

	m_pD3D9RenderStateCache = nullptr; 
	gtcDirect3DBeginScene = nullptr;
	gtcDirect3DEndScene = nullptr;
	gtcRBRReplay = nullptr;
	gtcRBRSaveReplay = nullptr;
	gtcRBRControllerAxisData = nullptr;
	gtcRBRCallForHelp = nullptr;

	//RefreshSettingsFromPluginINIFile();

	DebugPrint("Exit CNGPCarMenu.Constructor");
}

// Destructor
CNGPCarMenu::~CNGPCarMenu(void)
{
	g_bRBRHooksInitialized = FALSE;
	m_PluginState = T_PLUGINSTATE::PLUGINSTATE_CLOSING;

	try
	{
		DebugPrint("Enter CNGPCarMenu.Destructor");

		g_bD3DFontReleaseStateBlocks = m_bPacenotePluginInstalled; // CD3DFont bug workaround when Pacenote plugin is NOT installed (releaseStateBlocks should be FALSE if pacenotes plugin is not installed)

		SAFE_DELETE(gtcDirect3DBeginScene);
		SAFE_DELETE(gtcDirect3DEndScene);
		SAFE_DELETE(gtcRBRReplay);
		SAFE_DELETE(gtcRBRSaveReplay);
		SAFE_DELETE(gtcRBRControllerAxisData);
		SAFE_DELETE(gtcRBRCallForHelp);

#if USE_DEBUG == 1
		SAFE_DELETE(g_pFontDebug);
#endif
		SAFE_DELETE(g_pFontCarSpecCustom);
		SAFE_DELETE(g_pFontCarSpecModel);

		SAFE_DELETE(g_pFontRBRRXLoadTrack);
		SAFE_DELETE(g_pFontRBRRXLoadTrackSpec);

		SAFE_RELEASE(m_minimapVertexBuffer);
		SAFE_RELEASE(m_screenshotCroppingRectVertexBuffer);
		SAFE_RELEASE(m_screenshotCroppingRectVertexBufferRSF);
		
		ClearCachedCarPreviewImages();
		ClearCustomTrackLoadImage();

		SAFE_RELEASE(m_rbrLatestStageResultsBackground);

		SAFE_DELETE(g_pRBRPluginMenuSystem);
		SAFE_DELETE(m_pLangIniFile);
		SAFE_DELETE(m_pTracksIniFile);

		if (m_pCustomMapMenuRBRTM != nullptr) delete[] m_pCustomMapMenuRBRTM;
		if (m_pCustomMapMenuRBRRX != nullptr) delete[] m_pCustomMapMenuRBRRX;

		SAFE_DELETE(g_pRBRPluginIntegratorLinkList);
		SAFE_DELETE(g_pRBRPluginIntegratorFontList);

		SAFE_DELETE(g_pRBRRXTrackNameListAlreadyInitialized);
		SAFE_RELEASE(g_mapRBRRXRightBlackBarVertexBuffer);
		//SAFE_DELETE(g_watcherNewReplayFileListener);

		SAFE_DELETE(g_pRaceStatDB);

		SAFE_DELETE(m_pD3D9RenderStateCache);

		DebugPrint("Exit CNGPCarMenu.Destructor");
		DebugCloseFile();

		if (g_pRBRPlugin == this) g_pRBRPlugin = nullptr;
	}
	catch (...)
	{
		// Do nothing. Just eat all potential exceptions (should never happen at this point, but you never know)
	}
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
void CNGPCarMenu::RefreshSettingsFromPluginINIFile(bool fistTimeRefresh)
{
	WCHAR szResolutionText[16];

	DebugPrint("Enter CNGPCarMenu.RefreshSettingsFromPluginINIFile");

	int iFileFormat;
	CSimpleIniWEx pluginINIFile;
	std::wstring sTextValue;
	std::string  sIniFileName = m_sRBRRootDir + "\\Plugins\\" VS_PROJECT_NAME ".ini";

	m_sMenuStatusText1.clear();

	try
	{
		// If NGPCarMenu.ini file is missing, but the NGPCarMenu.ini.sample exists then copy the sample as the official version.
		// Upgrade version doesn't overwrite the official (possible user tweaked) NGPCarMenu.ini file.
		if (!fs::exists(sIniFileName) && fs::exists(sIniFileName + ".sample"))
			fs::copy_file(sIniFileName + ".sample", sIniFileName);

		RBRAPI_RefreshWndRect();
		swprintf_s(szResolutionText, COUNT_OF_ITEMS(szResolutionText), L"%dx%d", g_rectRBRWndClient.right, g_rectRBRWndClient.bottom);

		LogPrint("RBR wnd resolution %dx%d", g_rectRBRWndClient.right, g_rectRBRWndClient.bottom);
		LogPrint("RBR game resolution %dx%d", g_pRBRGameConfig->resolutionX, g_pRBRGameConfig->resolutionY);

		pluginINIFile.SetUnicode(true);
		pluginINIFile.LoadFile(sIniFileName.c_str());

		if (fistTimeRefresh)
		{
			//
			// If [XresxYres] INI section is missing then add it now with a default value in ScreenshotCropping option
			//
			bool bIniFileModified = false;
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
						bIniFileModified = true;
					else
						LogPrint("ERROR CNGPCarMenu.RefreshSettingsFromPluginINIFile. Failed to add %s section to %s INI file. You should modify the file in Notepad and to add new resolution INI section", _ToString(std::wstring(szResolutionText)).c_str(), sIniFileName.c_str());
				}
				catch (...)
				{
					LogPrint("ERROR CNGPCarMenu.RefreshSettingsFromPluginINIFile. Failed to add %s section to %s INI file. You should modify the file in Notepad and to add new resolution INI section", _ToString(std::wstring(szResolutionText)).c_str(), sIniFileName.c_str());
				}
			}

			if (pluginINIFile.GetValueEx(L"Default", L"", L"CockpitOverrideFOVValue", L"").empty())
			{
				pluginINIFile.SetValue(L"Default", L"CockpitOverrideFOVValue", std::to_wstring(m_fMenuCockpitOverrideFOVValue).c_str());
				bIniFileModified = true;
			}

			if(bIniFileModified)
				pluginINIFile.SaveFile(sIniFileName.c_str());
		}

		// The latest INI fileFormat is 2
		iFileFormat = pluginINIFile.GetValueEx(L"Default", L"", L"FileFormat", 2);
	
		//this->m_screenshotReplayFileName = pluginINIFile.GetValueEx(L"Default", L"", L"ScreenshotReplay", L"");

		this->m_screenshotPath = pluginINIFile.GetValueEx(L"Default", L"", L"ScreenshotPath", L"");

		// Read RSF specific screenshot path. If the value is "0" then disable rsf image generation
		if (pluginINIFile.GetValue(L"Default", L"RSF_ScreenshotPath", nullptr) == nullptr)
			this->m_screenshotPathRSF = L"rsfdata\\images\\car_images\\%carRSFID%.%fileType%";
		else
			this->m_screenshotPathRSF = pluginINIFile.GetValueEx(L"Default", L"", L"RSF_ScreenshotPath", L"");

		DebugPrint(L"RSF_ScreenshotPath=%s", this->m_screenshotPathRSF.c_str());

		// FleFormat=1 has a bit different logic in ScreenshotPath value. The "%resolution%" variable value was the default postfix. 
		// The current fileFormat expects to see %% variables in the option value, so append the default %resolution% variable at the end of the existing path value.
		if (iFileFormat < 2 && this->m_screenshotPath.find_first_of(L'%') == std::wstring::npos)
		{
			this->m_screenshotPath += L"\\%resolution%\\%carModelName%.%fileType%";
			pluginINIFile.SetValue(L"Default", L"ScreenshotPath", this->m_screenshotPath.c_str());
		}

		if (this->m_screenshotPath.length() >= 2 && this->m_screenshotPath[0] != L'\\' && this->m_screenshotPath[1] != L':' && g_rectRBRWndClient.right > 0 && g_rectRBRWndClient.bottom > 0)
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

		if (this->m_screenshotPathRSF.length() >= 2 && this->m_screenshotPathRSF[0] != L'\\' && this->m_screenshotPathRSF[1] != L':')
			this->m_screenshotPathRSF = this->m_sRBRRootDirW + L"\\" + this->m_screenshotPathRSF;

		pluginINIFile.GetValueEx(szResolutionText, L"Default", L"RBRWindowPosition", L"-1 -1", &m_rbrWindowPosition, -1);

		this->m_rbrCITCarListFilePath = pluginINIFile.GetValueEx(L"Default", L"", L"RBRCITCarListPath", L"");
		if (this->m_rbrCITCarListFilePath.length() >= 2 && this->m_rbrCITCarListFilePath[0] != L'\\' && this->m_rbrCITCarListFilePath[1] != L':')
			this->m_rbrCITCarListFilePath = this->m_sRBRRootDirW + L"\\" + this->m_rbrCITCarListFilePath;

		this->m_easyRBRFilePath = pluginINIFile.GetValueEx(L"Default", L"", L"EASYRBRPath", L"");
		if (this->m_easyRBRFilePath.length() >= 2 && this->m_easyRBRFilePath[0] != L'\\' && this->m_easyRBRFilePath[1] != L':')
			this->m_easyRBRFilePath = this->m_sRBRRootDirW + L"\\" + this->m_easyRBRFilePath;


		// TODO: carPosition, camPosition reading from INI file (now the car and cam position is hard-coded in this plugin code)

		pluginINIFile.GetValueEx(szResolutionText, L"Default", L"ScreenshotCropping", L"", &this->m_screenshotCroppingRect);

		if (GetRBRInstallType() == "RSF")
		{
			if (m_screenshotPathRSF.empty())
				this->m_screenshotCroppingRectRSF.bottom = -1;
			else
			{
				pluginINIFile.GetValueEx(szResolutionText, L"Default", L"RSF_ScreenshotCropping", L"", &this->m_screenshotCroppingRectRSF);
				if (_IsRectZero(this->m_screenshotCroppingRectRSF))
				{
					RBRAPI_MapRBRPointToScreenPoint(165.0f, 80.0f, (int*)&m_screenshotCroppingRectRSF.left, (int*)&m_screenshotCroppingRectRSF.top);
					RBRAPI_MapRBRPointToScreenPoint(550.0f, 368.0f, (int*)&m_screenshotCroppingRectRSF.right, (int*)&m_screenshotCroppingRectRSF.bottom);

					LogPrint("RSF_ScreenshotCropping value is empty. Using the default value RSF_ScreenshotCropping=%d %d %d %d", m_screenshotCroppingRectRSF.left, m_screenshotCroppingRectRSF.top, m_screenshotCroppingRectRSF.right, m_screenshotCroppingRectRSF.bottom);
				}
			}
		}


		this->m_screenshotCarPosition = pluginINIFile.GetValueEx(szResolutionText, L"Default", L"ScreenshotCarPosition", 0); // 0=default screeshot car position, 1=on the sky in the middle of nowhere

		pluginINIFile.GetValueEx(szResolutionText, L"Default", L"CarSelectLeftBlackBar",  L"", &this->m_carSelectLeftBlackBarRect);
		pluginINIFile.GetValueEx(szResolutionText, L"Default", L"CarSelectRightBlackBar", L"", &this->m_carSelectRightBlackBarRect);

		// Custom location of 3D model info textbox or default location (0,0 = default)
		pluginINIFile.GetValueEx(szResolutionText, L"Default", L"Car3DModelInfoPosition", L"", &this->m_car3DModelInfoPosition);

		// Scale the car picture in a car selection screen (0=no scale, stretch to fill the picture rect area, bit 1 = keep aspect ratio, bit 2 = place the pic to the bottom of the rect area)
		// Default 0 is to stretch the image to fill the drawing rect area (or if the original pic is already in the same size then scaling is not necessary)
		this->m_carPictureScale = pluginINIFile.GetValueEx(szResolutionText, L"Default", L"CarPictureScale", -1);

		this->m_carPictureUseTransparent = pluginINIFile.GetValueEx(szResolutionText, L"Default", L"CarPictureUseTransparent", 1);

		// DirectX (0) or GDI (1) screenshot logic
		this->m_screenshotAPIType = pluginINIFile.GetValueEx(szResolutionText, L"Default", L"ScreenshotAPIType", 0);

		// Screenshot image format option. One of the values in g_RBRPluginMenu_ImageOptions array (the default is the item in the first index)
		this->m_iMenuImageOption = 0;
		sTextValue = pluginINIFile.GetValueEx(szResolutionText, L"Default", L"ScreenshotFileType", L"");
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


		// Optional language file to customize label texts (English by default). 
		// Initialize the language dictionary only once when this method is called for the first time (changes to language translation strings take effect when RBR game is restarted)
		if (m_pLangIniFile == nullptr)
		{
			m_pLangIniFile = new CSimpleIniWEx();
			m_pLangIniFile->SetUnicode(true);

			sTextValue = pluginINIFile.GetValueEx(L"Default", L"", L"LanguageFile", L"");
			if (!sTextValue.empty())
			{
				// Append the root of RBR game location if the path is relative value
				if (sTextValue.length() >= 2 && sTextValue[0] != '\\' && sTextValue[1] != ':')
					sTextValue = this->m_sRBRRootDirW + L"\\" + sTextValue;

				if (fs::exists(sTextValue))
				{
					// Load customized language translation strings
					m_pLangIniFile->LoadFile(sTextValue.c_str());
				}
			}
		}


		//
		// RBRTM integration properties
		//
		DebugPrint("Reading RBRTM_Integration settings");

		try
		{
			m_iMenuRBRTMOption = (pluginINIFile.GetValueEx(L"Default", L"", L"RBRTM_Integration", 1) >= 1 ? 1 : 0);

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
						//rbrTMLangINI.SetUnicode(true);
						rbrTMLangINI.LoadFile(sRBRTMLangFile.c_str());
						m_sRBRTMPluginTitle = rbrTMLangINI.GetValue("Strings", "1", "");

						// FIXME: Disabled because RBRTM lang file should be read using the file specific codepage (CP). Not yet implemented. For now surface names can be translated via NGPCarMenu lang file.
						/*
						if (m_pLangIniFile != nullptr)
						{
							// Get surface (gravel, tarmac, snow) translations from RBRTM language file if the NGPCarMenu translation file didn't define these keywords
							AddLangStr(L"tarmac", rbrTMLangINI.GetValue("Strings", "360", nullptr));							
							AddLangStr(L"gravel", rbrTMLangINI.GetValue("Strings", "361", nullptr));
							AddLangStr(L"snow",   rbrTMLangINI.GetValue("Strings", "362", nullptr));
						}
						*/
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
		pluginINIFile.GetValueEx(szResolutionText, L"Default", L"RBRTM_CarPictureRect", L"", &this->m_carRBRTMPictureRect);
		if(_IsRectZero(m_carRBRTMPictureRect))
		{
			// Default rectangle area of RBRTM car preview picture if RBRTM_CarPictureRect is not set in INI file
			RBRAPI_MapRBRPointToScreenPoint(000.0f, 244.0f, (int*)&m_carRBRTMPictureRect.left, (int*)&m_carRBRTMPictureRect.top);
			RBRAPI_MapRBRPointToScreenPoint(451.0f, 461.0f, (int*)&m_carRBRTMPictureRect.right, (int*)&m_carRBRTMPictureRect.bottom);

			LogPrint("RBRTM_CarPictureRect value is empty. Using the default value RBRTM_CarPictureRect=%d %d %d %d", m_carRBRTMPictureRect.left, m_carRBRTMPictureRect.top, m_carRBRTMPictureRect.right, m_carRBRTMPictureRect.bottom);
		}

		// RBRTM_CarPictureCropping (not yet implemented)
		//pluginINIFile.GetValueEx(szResolutionText, L"Default", L"RBRTM_CarPictureCropping", L"", &this->m_carRBRTMPictureCropping);

		// Scale the car picture in RBRTM screen (0=no scale, stretch to fill the picture rect area, bit 1 = keep aspect ratio, bit 2 = place the pic to the bottom of the rect area)
		// Default 3 is to keep the aspect ratio and place the pic to bottom of the rect area.
		this->m_carRBRTMPictureScale = pluginINIFile.GetValueEx(szResolutionText, L"Default", L"RBRTM_CarPictureScale", 3);
		this->m_carRBRTMPictureUseTransparent = pluginINIFile.GetValueEx(szResolutionText, L"Default", L"RBRTM_CarPictureUseTransparent", 1);

		// Read Tracks.ini map file because some custom plugins list stage names there for mapIDs
		try
		{
			if (m_pTracksIniFile == nullptr)
			{
				m_pTracksIniFile = new CSimpleIniWEx();
				sTextValue = m_sRBRRootDirW + L"\\Maps\\Tracks.ini";
				m_pTracksIniFile->SetUnicode(true);
				if (fs::exists(sTextValue)) m_pTracksIniFile->LoadFile(sTextValue.c_str());
			}
		}
		catch (...)
		{
			LogPrint("ERROR CNGPCarMenu.RefreshSettingsFromPluginINIFile. Maps\\Tracks.ini reading failed");
		}


		if (m_iMenuRBRTMOption)
		{			
			m_bRBRTMTrackLoadBugFixWhenNotActive = pluginINIFile.GetBoolValue(L"Default", L"RBRTM_TrackLoadBugFixWhenNotActive", true);

			try
			{
				std::string sIniFileNameRBRTMRecentMaps = m_sRBRRootDir + "\\Plugins\\" VS_PROJECT_NAME "\\RBRTMRecentMaps.ini";
				
				CSimpleIni rbrtmRecentMapsINI;
				rbrtmRecentMapsINI.LoadFile(sIniFileNameRBRTMRecentMaps.c_str());

				// Read customized RBRTM Shakedown stages menu settings (recent maps)
				m_recentMapsMaxCountRBRTM = min(pluginINIFile.GetLongValue(L"Default", L"RBRTM_RecentMapsMaxCount", 5), 500);

				for (int idx = m_recentMapsMaxCountRBRTM; idx > 0; idx--)
					RBRTM_AddMapToRecentList(rbrtmRecentMapsINI.GetLongValue("Default", (std::string("RBRTM_RecentMap").append(std::to_string(idx)).c_str()), -1));

				// Set "not modified" when the recent map was modifed because of reading the current INI file
				m_bRecentMapsRBRTMModified = FALSE;
			}
			catch (...)
			{
				LogPrint("ERROR CNGPCarMenu.RefreshSettingsFromPluginINIFile. Invalid values in RBRTM_RecentMaps configurations");
			}

			// Custom map preview image path. NGPCarMenu checks first this folder+image. If the file doesn't exit then the plugin checks a map specific SplashScreen option in Maps\Tracks.ini file
			this->m_screenshotPathMapRBRTM = pluginINIFile.GetValueEx(szResolutionText, L"Default", L"RBRTM_MapScreenshotPath", L"Plugins\\NGPCarMenu\\preview\\maps\\%mapID%.png");
			if (this->m_screenshotPathMapRBRTM.length() >= 2 && this->m_screenshotPathMapRBRTM[0] != L'\\' && this->m_screenshotPathMapRBRTM[1] != L':')
				this->m_screenshotPathMapRBRTM = this->m_sRBRRootDirW + L"\\" + this->m_screenshotPathMapRBRTM; // Path relative to the root of RBR app path

			// RBRTM_MapPictureRect
			pluginINIFile.GetValueEx(szResolutionText, L"Default", L"RBRTM_MapPictureRect", L"", &this->m_mapRBRTMPictureRect[0]);
			if(_IsRectZero(m_mapRBRTMPictureRect[0]))
			{
				// Default rectangle area of RBRTM map preview picture if RBRTM_MapPictureRect is not set in INI file
				RBRAPI_MapRBRPointToScreenPoint(295.0f, 230.0f, (int*)&m_mapRBRTMPictureRect[0].left, (int*)&m_mapRBRTMPictureRect[0].top);
				RBRAPI_MapRBRPointToScreenPoint(628.0f, 455.0f, (int*)&m_mapRBRTMPictureRect[0].right, (int*)&m_mapRBRTMPictureRect[0].bottom);

				LogPrint("RBRTM_MapPictureRect value is empty. Using the default value RBRTM_MapPictureRect=%d %d %d %d", m_mapRBRTMPictureRect[0].left, m_mapRBRTMPictureRect[0].top, m_mapRBRTMPictureRect[0].right, m_mapRBRTMPictureRect[0].bottom);
			}

			pluginINIFile.GetValueEx(szResolutionText, L"Default", L"RBRTM_MapPictureRectOpt", L"", &this->m_mapRBRTMPictureRect[1]);
			if (_IsRectZero(m_mapRBRTMPictureRect[1]))
			{
				// Default rectangle area of RBRTM map preview picture if RBRTM_MapPictureRect is not set in INI file
				RBRAPI_MapRBRPointToScreenPoint(200.0f, 140.0f, (int*)&m_mapRBRTMPictureRect[1].left, (int*)&m_mapRBRTMPictureRect[1].top);
				RBRAPI_MapRBRPointToScreenPoint(640.0f, 462.0f, (int*)&m_mapRBRTMPictureRect[1].right, (int*)&m_mapRBRTMPictureRect[1].bottom);

				LogPrint("RBRTM_MapPictureRectOpt value is empty. Using the default value RBRTM_MapPictureRectOpt=%d %d %d %d", m_mapRBRTMPictureRect[1].left, m_mapRBRTMPictureRect[1].top, m_mapRBRTMPictureRect[1].right, m_mapRBRTMPictureRect[1].bottom);
			}

			// RBRTM_MinimapPictureRect
			pluginINIFile.GetValueEx(szResolutionText, L"Default", L"RBRTM_MinimapPictureRect", L"", &this->m_minimapRBRTMPictureRect[0]);
			if (_IsRectZero(m_minimapRBRTMPictureRect[0]))
			{
				if(m_mapRBRTMPictureRect[0].bottom != -1)
					m_minimapRBRTMPictureRect[0] = m_mapRBRTMPictureRect[0];
				else
				{
					// Default rectangle area of RBRTM minimap if the map preview is disabled
					RBRAPI_MapRBRPointToScreenPoint(295.0f, 230.0f, (int*)&m_minimapRBRTMPictureRect[0].left, (int*)&m_minimapRBRTMPictureRect[0].top);
					RBRAPI_MapRBRPointToScreenPoint(628.0f, 480.0f, (int*)&m_minimapRBRTMPictureRect[0].right, (int*)&m_minimapRBRTMPictureRect[0].bottom);
					m_minimapRBRTMPictureRect[0].bottom -= (g_pFontCarSpecModel->GetTextHeight()); // +(g_pFontCarSpecModel->GetTextHeight() / 2));
				}								

				LogPrint("RBRTM_MinimapPictureRect value is empty. Using the default value RBRTM_MinimapPictureRect=%d %d %d %d", m_minimapRBRTMPictureRect[0].left, m_minimapRBRTMPictureRect[0].top, m_minimapRBRTMPictureRect[0].right, m_minimapRBRTMPictureRect[0].bottom);
			}			

			pluginINIFile.GetValueEx(szResolutionText, L"Default", L"RBRTM_MinimapPictureRectOpt", L"", &this->m_minimapRBRTMPictureRect[1]);
			if (_IsRectZero(m_minimapRBRTMPictureRect[1]))
			{
				// Default rectangle area of RBRTM minimap on stage options screen if the map preview is disabled
				RBRAPI_MapRBRPointToScreenPoint(5.0f, 185.0f, (int*)&m_minimapRBRTMPictureRect[1].left, (int*)&m_minimapRBRTMPictureRect[1].top);
				RBRAPI_MapRBRPointToScreenPoint(635.0f, 480.0f, (int*)&m_minimapRBRTMPictureRect[1].right, (int*)&m_minimapRBRTMPictureRect[1].bottom);
				m_minimapRBRTMPictureRect[1].bottom -= (g_pFontCarSpecModel->GetTextHeight() +(g_pFontCarSpecModel->GetTextHeight() / 2));

				LogPrint("RBRTM_MinimapPictureRectOpt value is empty. Using the default value RBRTM_MinimapPictureRectOpt=%d %d %d %d", m_minimapRBRTMPictureRect[1].left, m_minimapRBRTMPictureRect[1].top, m_minimapRBRTMPictureRect[1].right, m_minimapRBRTMPictureRect[1].bottom);
			}
		}

		//
		// RBRRX integration properties
		//
		try
		{
			m_iMenuRBRRXOption = (pluginINIFile.GetValueEx(L"Default", L"", L"RBRRX_Integration", 1) >= 1 ? 1 : 0);
			if (m_iMenuRBRRXOption)
				g_bNewCustomPluginIntegrations = TRUE;  // If RBRRX integration is enabled then signal initialization of "custom plugin integration"
		}
		catch (...)
		{
			LogPrint("WARNING. Invalid value %s in RBRRX_Integration option", sTextValue.c_str());
			m_iMenuRBRRXOption = 0;
		}


		DebugPrint("Reading RBRRX_Integration settings");

		if (m_iMenuRBRRXOption)
		{
			try
			{
				std::string sIniFileNameRBRRXRecentMaps = m_sRBRRootDir + "\\Plugins\\" VS_PROJECT_NAME "\\RBRRXRecentMaps.ini";

				CSimpleIni rbrrxRecentMapsINI;
				//rbrrxRecentMapsINI.SetUnicode(TRUE);
				rbrrxRecentMapsINI.LoadFile(sIniFileNameRBRRXRecentMaps.c_str());
			
				// Read customized RBRRX stages menu settings (recent maps)
				m_recentMapsMaxCountRBRRX = min(pluginINIFile.GetLongValue(L"Default", L"RBRRX_RecentMapsMaxCount", 5), 500);

				for (int idx = m_recentMapsMaxCountRBRRX; idx > 0; idx--)
					RBRRX_AddMapToRecentList(std::string(rbrrxRecentMapsINI.GetValue("Default", (std::string("RBRRX_RecentMap").append(std::to_string(idx)).c_str()), "")));

				// Set "not modified" when the recent map was modifed because of reading the current INI file
				m_bRecentMapsRBRRXModified = FALSE;
			}
			catch (...)
			{
				LogPrint("ERROR CNGPCarMenu.RefreshSettingsFromPluginINIFile. Invalid values in RBRRX_RecentMaps configurations");
			}

			// Custom map preview image path. NGPCarMenu checks first this folder+image. If the file doesn't exit then the plugin checks a map specific SplashScreen option in Maps\Tracks.ini file
			this->m_screenshotPathMapRBRRX = pluginINIFile.GetValueEx(szResolutionText, L"Default", L"RBRRX_MapScreenshotPath", L"Plugins\\NGPCarMenu\\preview\\maps\\%mapfolder%.png");
			if (this->m_screenshotPathMapRBRRX.length() >= 2 && this->m_screenshotPathMapRBRRX[0] != L'\\' && this->m_screenshotPathMapRBRRX[1] != L':')
				this->m_screenshotPathMapRBRRX = this->m_sRBRRootDirW + L"\\" + this->m_screenshotPathMapRBRRX; // Path relative to the root of RBR app path

			// RBRRX_MapPictureRect
			pluginINIFile.GetValueEx(szResolutionText, L"Default", L"RBRRX_MapPictureRect", L"", &this->m_mapRBRRXPictureRect[0]);
			if(_IsRectZero(m_mapRBRRXPictureRect[0]))
			{
				// Default rectangle area of RBRRX map preview picture if RBRRX_MapPictureRect is not set in INI file
				RBRAPI_MapRBRPointToScreenPoint(390.0f, 320.0f, (int*)&m_mapRBRRXPictureRect[0].left, (int*)&m_mapRBRRXPictureRect[0].top);
				RBRAPI_MapRBRPointToScreenPoint(630.0f, 470.0f, (int*)&m_mapRBRRXPictureRect[0].right, (int*)&m_mapRBRRXPictureRect[0].bottom);

				LogPrint("RBRRX_MapPictureRect value is empty. Using the default value RBRRX_MapPictureRect=%d %d %d %d", m_mapRBRRXPictureRect[0].left, m_mapRBRRXPictureRect[0].top, m_mapRBRRXPictureRect[0].right, m_mapRBRRXPictureRect[0].bottom);
			}

			pluginINIFile.GetValueEx(szResolutionText, L"Default", L"RBRRX_MapPictureRectOpt", L"", &this->m_mapRBRRXPictureRect[1]);
			if (_IsRectZero(m_mapRBRRXPictureRect[1]))
			{
				// Default rectangle area of RBRRX map preview picture if RBRRX_MapPictureRect is not set in INI file
				RBRAPI_MapRBRPointToScreenPoint(5.0f, 145.0f, (int*)&m_mapRBRRXPictureRect[1].left, (int*)&m_mapRBRRXPictureRect[1].top);
				RBRAPI_MapRBRPointToScreenPoint(635.0f, 480.0f, (int*)&m_mapRBRRXPictureRect[1].right, (int*)&m_mapRBRRXPictureRect[1].bottom);

				LogPrint("RBRRX_MapPictureRectOpt value is empty. Using the default value RBRRX_MapPictureRectOpt=%d %d %d %d", m_mapRBRRXPictureRect[1].left, m_mapRBRRXPictureRect[1].top, m_mapRBRRXPictureRect[1].right, m_mapRBRRXPictureRect[1].bottom);
			}

			pluginINIFile.GetValueEx(szResolutionText, L"Default", L"RBRRX_MapPictureRectLoadTrack", L"", &this->m_mapRBRRXPictureRect[2]);
			if (_IsRectZero(m_mapRBRRXPictureRect[2]))
			{
				// Default rectangle area of RBRRX loadTrack preview picture if the option is not set in INI file
				RBRAPI_MapRBRPointToScreenPoint(0.0f, 0.0f, (int*)&m_mapRBRRXPictureRect[2].left, (int*)&m_mapRBRRXPictureRect[2].top);
				RBRAPI_MapRBRPointToScreenPoint(640.0f, 370.0f, (int*)&m_mapRBRRXPictureRect[2].right, (int*)&m_mapRBRRXPictureRect[2].bottom);

				LogPrint("RBRRX_MapPictureRectLoadTrack value is empty. Using the default value RBRRX_MapPictureRectLoadTrack=%d %d %d %d", m_mapRBRRXPictureRect[2].left, m_mapRBRRXPictureRect[2].top, m_mapRBRRXPictureRect[2].right, m_mapRBRRXPictureRect[2].bottom);
			}


			// RBRRX_MinimapPictureRect
			pluginINIFile.GetValueEx(szResolutionText, L"Default", L"RBRRX_MinimapPictureRect", L"", &this->m_minimapRBRRXPictureRect[0]);
			if(_IsRectZero(m_minimapRBRRXPictureRect[0]))
			{
				if (m_mapRBRRXPictureRect[0].bottom != -1)
					m_minimapRBRRXPictureRect[0] = m_mapRBRRXPictureRect[0];	
				else
				{
					// Default rectangle area of RBRRX minimap preview picture if the map itself is disabled (ie. coordinates not set)
					RBRAPI_MapRBRPointToScreenPoint(390.0f, 320.0f, (int*)&m_minimapRBRRXPictureRect[0].left, (int*)&m_minimapRBRRXPictureRect[0].top);
					RBRAPI_MapRBRPointToScreenPoint(630.0f, 480.0f, (int*)&m_minimapRBRRXPictureRect[0].right, (int*)&m_minimapRBRRXPictureRect[0].bottom);					
				}				
				m_minimapRBRRXPictureRect[0].bottom -= (g_pFontCarSpecModel->GetTextHeight()); // +(g_pFontCarSpecModel->GetTextHeight() / 2));

				LogPrint("RBRRX_MinimapPictureRect value is empty. Using the default value RBRRX_MinimapPictureRect=%d %d %d %d", m_minimapRBRRXPictureRect[0].left, m_minimapRBRRXPictureRect[0].top, m_minimapRBRRXPictureRect[0].right, m_minimapRBRRXPictureRect[0].bottom);
			}

			pluginINIFile.GetValueEx(szResolutionText, L"Default", L"RBRRX_MinimapPictureRectOpt", L"", &this->m_minimapRBRRXPictureRect[1]);
			if (_IsRectZero(m_minimapRBRRXPictureRect[1]))
			{
				if (m_mapRBRRXPictureRect[1].bottom != -1)
					m_minimapRBRRXPictureRect[1] = m_mapRBRRXPictureRect[1];
				else
				{
					// Default rectangle area of RBRRX minimap preview picture if the map itself is disabled (ie. coordinates not set)
					RBRAPI_MapRBRPointToScreenPoint(5.0f, 145.0f, (int*)&m_minimapRBRRXPictureRect[1].left, (int*)&m_minimapRBRRXPictureRect[1].top);
					RBRAPI_MapRBRPointToScreenPoint(635.0f, 480.0f, (int*)&m_minimapRBRRXPictureRect[1].right, (int*)&m_minimapRBRRXPictureRect[1].bottom);
				}
				m_minimapRBRRXPictureRect[1].bottom -= g_pFontCarSpecModel->GetTextHeight();

				LogPrint("RBRRX_MinimapPictureRectOpt value is empty. Using the default value RBRRX_MinimapPictureRectOpt=%d %d %d %d", m_minimapRBRRXPictureRect[1].left, m_minimapRBRRXPictureRect[1].top, m_minimapRBRRXPictureRect[1].right, m_minimapRBRRXPictureRect[1].bottom);
			}

			pluginINIFile.GetValueEx(szResolutionText, L"Default", L"RBRRX_MinimapPictureRectLoadTrack", L"", &this->m_minimapRBRRXPictureRect[2]);
			if (_IsRectZero(m_minimapRBRRXPictureRect[2]))
			{
				// Default rectangle area of RBRRX minimap preview picture on LoadTrack screen if the map itself is disabled (ie. coordinates not set)
				RBRAPI_MapRBRPointToScreenPoint(5.0f, 5.0f, (int*)&m_minimapRBRRXPictureRect[2].left, (int*)&m_minimapRBRRXPictureRect[2].top);
				RBRAPI_MapRBRPointToScreenPoint(635.0f, 372.0f, (int*)&m_minimapRBRRXPictureRect[2].right, (int*)&m_minimapRBRRXPictureRect[2].bottom);

				LogPrint("RBRRX_MinimapPictureRectLoadTrack value is empty. Using the default value RBRRX_MinimapPictureRectLoadTrack=%d %d %d %d", m_minimapRBRRXPictureRect[2].left, m_minimapRBRRXPictureRect[2].top, m_minimapRBRRXPictureRect[2].right, m_minimapRBRRXPictureRect[2].bottom);
			}

			m_bShowCustomLoadTrackScreenRBRRX = (pluginINIFile.GetValueEx(L"Default", L"", L"RBRRX_CustomLoadTrackScreen", 1) >= 1 ? true : false);
		}

		DebugPrint("Reading inverted pedal settings input.ini");

		m_bGenerateReplayMetadataFile = (pluginINIFile.GetValueEx(L"Default", L"", L"GenerateReplayMetadataFile", 1) != 0);


		// Check if "inverted pedals RBR bug fix workaround" is enabled and for which pedals (0=disabled. Bit flag 1..4)
		g_iInvertedPedalsStartupFixFlag = pluginINIFile.GetValueEx(L"Default", L"", L"InvertedPedalsStartupFix", 1);
		if (g_iInvertedPedalsStartupFixFlag != 0)
		{
			CSimpleIniEx inputIniFile;

			g_iInvertedPedalsStartupFixFlag = 0;
			if (fs::exists(m_sRBRRootDir + "\\input.ini"))
			{
				inputIniFile.LoadFile((m_sRBRRootDir + "\\input.ini").c_str());

				if (inputIniFile.GetBoolValue("Settings", "InvertThrottleAxis", false))  g_iInvertedPedalsStartupFixFlag = 0x01;
				if (inputIniFile.GetBoolValue("Settings", "InvertBrakeAxis", false))     g_iInvertedPedalsStartupFixFlag |= 0x02;
				if (inputIniFile.GetBoolValue("Settings", "InvertClutchAxis", false))    g_iInvertedPedalsStartupFixFlag |= 0x04;
				if (inputIniFile.GetBoolValue("Settings", "InvertHandbrakeAxis", false)) g_iInvertedPedalsStartupFixFlag |= 0x08;

				if (g_iInvertedPedalsStartupFixFlag != 0)
					LogPrint("InvertedPedalsStartupFix enabled and inverted pedals bitflag=%d", g_iInvertedPedalsStartupFixFlag);
			}
		}


		// Split combined throttle and brake axis as separate throttle and brake (ie set CombinedThrottleBrake axis in RBR to analog trigger and set separate brake and throttle to some unused keyboard keys)
		g_iXInputSplitThrottleBrakeAxis = pluginINIFile.GetValueEx(L"Default", L"", L"SplitCombinedThrottleBrakeAxis", 0);
		if (g_iXInputSplitThrottleBrakeAxis >= 1)
		{
			LogPrint("SplitCombinedThrottleBrakeAxis #%d. Assign an analog gamepad trigger key to the combined Accelerate&Brake and some otherwise unused keyboard or gamepad keys to Throttle and Brake options in RBR controller setup screen", g_iXInputSplitThrottleBrakeAxis);
			g_iXInputSplitThrottleBrakeAxis--;

			g_iXInputBrake = 0;
			if (_iEqual(pluginINIFile.GetValueEx(L"Default", L"", L"SplitBrakeAxis", L"lt"), L"rt", true))
				g_iXInputBrake = 1;

			g_iXInputThrottle = 1;
			if (_iEqual(pluginINIFile.GetValueEx(L"Default", L"", L"SplitThrottleAxis", L"rt"), L"lt", true))
				g_iXInputThrottle = 0;

			LogPrint("SplitBrakeAxis=%d SplitThrottleAxis=%d", g_iXInputBrake, g_iXInputThrottle);
		}
		else
			g_iXInputSplitThrottleBrakeAxis = -1;


		// Deadzone value for steering, throttle, brake, clutch, handbrake
		g_fControllerAxisDeadzone[0] = static_cast<float>(pluginINIFile.GetValueEx(L"Default", L"", L"DeadzoneSteering", 0)) / 100.0f;
		g_fControllerAxisDeadzone[3] = static_cast<float>(pluginINIFile.GetValueEx(L"Default", L"", L"DeadzoneThrottle", 0)) / 100.0f;
		g_fControllerAxisDeadzone[5] = static_cast<float>(pluginINIFile.GetValueEx(L"Default", L"", L"DeadzoneBrake", 0)) / 100.0f;
		g_fControllerAxisDeadzone[6] = static_cast<float>(pluginINIFile.GetValueEx(L"Default", L"", L"DeadzoneHandbrake", 0)) / 100.0f;
		g_fControllerAxisDeadzone[11] = static_cast<float>(pluginINIFile.GetValueEx(L"Default", L"", L"DeadzoneClutch", 0)) / 100.0f;

		if (g_fControllerAxisDeadzone[0] != 0.0f || g_fControllerAxisDeadzone[3] != 0.0f || g_fControllerAxisDeadzone[5] != 0.0f || g_fControllerAxisDeadzone[6] != 0.0f || g_fControllerAxisDeadzone[11] != 0.0f)
			LogPrint("DeadzoneSteering=%.2f DeadzoneThrottle=%.2f DeadzoneBrake=%.2f DeadzoneHandbrake=%.2f DeadzoneClutch=%.2f", g_fControllerAxisDeadzone[0], g_fControllerAxisDeadzone[3], g_fControllerAxisDeadzone[5], g_fControllerAxisDeadzone[6], g_fControllerAxisDeadzone[11]);

		if (fistTimeRefresh)
		{
			m_sRallySchoolMenuReplacement = _ToString(pluginINIFile.GetValueEx(L"Default", L"", L"RallySchoolMenuReplacement", L"Disabled"));
			if (_iEqual(m_sRallySchoolMenuReplacement, "main", true)) m_sRallySchoolMenuReplacement = "Disabled";

			if (!_iEqual(m_sRallySchoolMenuReplacement, "disabled", true))
			{
				if (_iEqual(m_sRallySchoolMenuReplacement, "plugins", true))
					m_iMenuRallySchoolMenuOption = 2; // Plugins auto-logon menu entry
				else
					m_iMenuRallySchoolMenuOption = 3; // Place-holder custom plugin value until the list of custom plugins is initialized

				wcsncpy_s(g_wszRallySchoolMenuReplacementText, _ToWString(m_sRallySchoolMenuReplacement).c_str(), COUNT_OF_ITEMS(g_wszRallySchoolMenuReplacementText));
			}
			else
				m_iMenuRallySchoolMenuOption = 0;
		}


		//
		// Race stat db (enabled by default unless RBR was launched via RBRPro manager. To use raceStatDB under RBRPro the RaceStatDB INI option should be explicitly set)
		//
		if (g_pRaceStatDB == nullptr)
		{
			m_raceStatDBFilePath = _ToString(pluginINIFile.GetValueEx(L"Default", L"", L"RaceStatDB", (!m_bRBRProInstalled ? L"Plugins\\NGPCarMenu\\RaceStat\\RaceStatDB.sqlite3" : L"0")));
			if (m_raceStatDBFilePath == "0" || _iEqual(m_raceStatDBFilePath, "disabled", true))
				m_raceStatDBFilePath.clear();
			else if (m_raceStatDBFilePath.length() >= 2 && m_raceStatDBFilePath[0] != '\\' && m_raceStatDBFilePath[1] != ':')
			{
				std::string defaultRaceStatPath = m_sRBRRootDir + "\\Plugins\\NGPCarMenu\\RaceStat";
				if (!fs::exists(defaultRaceStatPath))
					fs::create_directory(defaultRaceStatPath);

				m_raceStatDBFilePath = m_sRBRRootDir + "\\" + m_raceStatDBFilePath;
			}

			RaceStatDB_Initialize();
		}

		// Default location for "recent results" data table (if the option is "0 -1" (or just "0") then the result list is disabled)
		pluginINIFile.GetValueEx(szResolutionText, L"Default", L"RecentResultsPosition", L"-1 -1", &m_recentResultsPosition, -1);
		if (!(m_recentResultsPosition.x == 0 && m_recentResultsPosition.y == -1))
		{
			SIZE textSize;
			g_pFontCarSpecCustom->GetTextExtent(std::wstring(45, L'X').c_str(), &textSize);

			if (m_recentResultsPosition.x == -1 && m_recentResultsPosition.y == -1)
			{
				RBRAPI_MapRBRPointToScreenPoint(330.0f, 5.0f, &m_recentResultsPosition);
				LogPrint("RecentResultsPosition value is empty. Using the default value RecentResultsPosition=%d %d", m_recentResultsPosition.x, m_recentResultsPosition.y);
			}

			D3D9CreateRectangleVertexBuffer(g_pRBRIDirect3DDevice9,
				static_cast<float>(m_recentResultsPosition.x - 5),
				static_cast<float>(m_recentResultsPosition.y - 5),
				min(static_cast<float>(g_rectRBRWndClient.right - (m_recentResultsPosition.x - 5)), static_cast<float>(textSize.cx)),
				static_cast<float>(textSize.cy * 14),
				&m_rbrLatestStageResultsBackground, D3DCOLOR_ARGB(0xF0, 0x10, 0x10, 0x10));
		}

		pluginINIFile.GetValueEx(szResolutionText, L"Default", L"RBRTM_RecentResultsPosition", L"-1 -1", &m_recentResultsPosition_RBRTM, -1);
		if (!(m_recentResultsPosition_RBRTM.x == 0 && m_recentResultsPosition_RBRTM.y == -1))
		{
			if (m_recentResultsPosition_RBRTM.x == -1 && m_recentResultsPosition_RBRTM.y == -1)
			{
				RBRAPI_MapRBRPointToScreenPoint(340.0f, 150.0f, &m_recentResultsPosition_RBRTM);
				LogPrint("RBRTM_RecentResultsPosition value is empty. Using the default value RBRTM_RecentResultsPosition=%d %d", m_recentResultsPosition_RBRTM.x, m_recentResultsPosition_RBRTM.y);
			}
		}

		pluginINIFile.GetValueEx(szResolutionText, L"Default", L"RBRRX_RecentResultsPosition", L"-1 -1", &m_recentResultsPosition_RBRRX, -1);
		if (!(m_recentResultsPosition_RBRRX.x == 0 && m_recentResultsPosition_RBRRX.y == -1))
		{
			if (m_recentResultsPosition_RBRRX.x == -1 && m_recentResultsPosition_RBRRX.y == -1)
			{
				RBRAPI_MapRBRPointToScreenPoint(390.0f, 22.0f, &m_recentResultsPosition_RBRRX);
				LogPrint("RBRRX_RecentResultsPosition value is empty. Using the default value RBRRX_RecentResultsPosition=%d %d", m_recentResultsPosition_RBRRX.x, m_recentResultsPosition_RBRRX.y);

				// Actually, the default location is based on the BTB metadata title text location
				m_recentResultsPosition_RBRRX.x = 0;
				m_recentResultsPosition_RBRRX.y = 0;
			}
		}


		m_iMenuCockpitCameraShaking = pluginINIFile.GetValueEx(L"Default", L"", L"CockpitCameraShaking", 0);
		m_iMenuCockpitSteeringWheel = pluginINIFile.GetValueEx(L"Default", L"", L"CockpitSteeringWheel", 0);
		m_iMenuCockpitWipers = pluginINIFile.GetValueEx(L"Default", L"", L"CockpitWipers", 0);
		m_iMenuCockpitWindscreen = pluginINIFile.GetValueEx(L"Default", L"", L"CockpitWindscreen", 0);

		m_iMenuCockpitOverrideFOV = pluginINIFile.GetValueEx(L"Default", L"", L"CockpitOverrideFOV", 0);
		m_fMenuCockpitOverrideFOVValue = pluginINIFile.GetValueExFloat(L"Default", L"", L"CockpitOverrideFOVValue", m_fMenuCockpitOverrideFOVValue);


		//
		// If the existing INI file format is an old version1 then save the file using the new format specifier
		//
		if (iFileFormat < 2)
		{
			DebugPrint("Upgrading NGPCarMenu.ini file format");

			pluginINIFile.SetValue(L"Default", L"FileFormat", L"2");
			pluginINIFile.SaveFile(sIniFileName.c_str());
		}


		//
		// Set status text msg (or if the status1 is already set then it must be an error msg set by this method)
		//
		if(m_sMenuStatusText1.empty()) m_sMenuStatusText1 = _ReplaceStr(_ToString(ReplacePathVariables(m_screenshotPath)), "%", "");
		m_sMenuStatusText2 = _ToString(std::wstring(szResolutionText)) + " native resolution detected";
		m_sMenuStatusText3 = (m_easyRBRFilePath.empty() ? "RBRCIT " + _ToString(m_rbrCITCarListFilePath) : "EasyRBR " + _ToString(m_easyRBRFilePath));


		//
		// Check AutoLogon option. Do this as the last step in this method
		//
		if (m_iAutoLogonMenuState < 0)
		{
			DebugPrint("Reading AutoLogon settings");

			// RBR bootup autoLogon option is read only at first time when this method is called (ie RBR launch time).
			// Check cmdline option first and then INI file value.
			m_sAutoLogon = GetCmdLineArgValue("-autologon");
			if(m_sAutoLogon.empty())
				m_sAutoLogon = _ToString(pluginINIFile.GetValueEx(L"Default", L"", L"AutoLogon", L"Disabled"));

			if (!_iEqual(m_sAutoLogon, "disabled", true) && m_sAutoLogon != "0")
			{				
				// Autologon navigation needs a initialization of plugin menu in case the target menu is a plugin
				g_bNewCustomPluginIntegrations = TRUE;

				m_bAutoLogonWaitProfile = pluginINIFile.GetLongValue(L"Default", L"AutoLogonWaitProfileSelection", 0) == 1;

				m_autoLogonSequenceSteps.clear();
				m_autoLogonSequenceSteps.push_back("main");
				if (_iStarts_With(m_sAutoLogon, "replay", true))
				{
					std::wstring sAutoLogonParam1;

					m_autoLogonSequenceSteps.push_back("replay");

					// Check if cmdline set the AutoLogonParam1 value, otherwise read the value from INI file
					sAutoLogonParam1 = GetCmdLineArgValue(L"-autologonparam1");
					if (sAutoLogonParam1.empty())
						sAutoLogonParam1 = pluginINIFile.GetValueEx(L"Default", L"", L"AutoLogonParam1", L"");

					if (!sAutoLogonParam1.empty())
						m_autoLogonSequenceSteps.push_back(_ToString(sAutoLogonParam1));

					// Special replay mode where RBR is automatically closed when replay is finished and focus moves back to the RBR main menu (RBRPro ReplayManager uses this method)
					if (_iEqual(m_sAutoLogon, "replayandexit", true))
					{
						LogPrint(L"AutoLogon ReplayAndExit %s", sAutoLogonParam1.c_str());
						m_bAutoExitAfterReplay = TRUE;
					}
				}
				else if (!_iEqual(m_sAutoLogon, "main", true))
				{
					m_autoLogonSequenceSteps.push_back("options");
					m_autoLogonSequenceSteps.push_back("plugins");
					if (!_iEqual(m_sAutoLogon, "plugins", true))
						m_autoLogonSequenceSteps.push_back(m_sAutoLogon);
				}

				StartNewAutoLogonSequence();
				LogPrint(L"AutoLogon sequence enabled. %s", m_autoLogonSequenceLabel.str().c_str());

				// Autologon enabled. Do the trick and navigate automatically to the specified menu (Main, Plugins or custom plugin).
				// If user chooses the profile manually (NGPCarMenu AutoLogon waits for a profile) then skip autoLogon states 1-2 and go straight to wait for main menu (state 3)
				//m_dwAutoLogonEventStartTick = GetTickCount();
				//m_iAutoLogonMenuState = (m_bAutoLogonWaitProfile ? 3 : 1);
			}
			else
			{
				m_iAutoLogonMenuState = 0;			// Autologon disabled
				m_autoLogonSequenceSteps.clear();
			}
		}

	}
	catch (...)
	{
		LogPrint("ERROR CNGPCarMenu.RefreshSettingsFromPluginINIFile. %s INI reading failed", sIniFileName.c_str());
		m_sMenuStatusText1 = sIniFileName + " file access or file format error";
		m_iAutoLogonMenuState = 0;
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

	DebugPrint("Enter SaveSettingsToPluginINIFile");

	try
	{
		sIniFileName = CNGPCarMenu::m_sRBRRootDir + "\\Plugins\\" VS_PROJECT_NAME ".ini";
		pluginINIFile.LoadFile(sIniFileName.c_str());

		sOptionValue = g_NGPCarMenu_ImageOptions[this->m_iMenuImageOption];
		wsOptionValue = _ToWString(sOptionValue);
		pluginINIFile.SetValue(L"Default", L"ScreenshotFileType", wsOptionValue.c_str());

		pluginINIFile.SetValue(L"Default", L"RBRTM_Integration", std::to_wstring(this->m_iMenuRBRTMOption).c_str());
		pluginINIFile.SetValue(L"Default", L"RBRRX_Integration", std::to_wstring(this->m_iMenuRBRRXOption).c_str());

		pluginINIFile.SetValue(L"Default", L"SplitCombinedThrottleBrakeAxis", std::to_wstring(g_iXInputSplitThrottleBrakeAxis + 1).c_str());

		pluginINIFile.SetValue(L"Default", L"AutoLogon", _ToWString(std::string( m_iMenuAutoLogonOption < (int)g_NGPCarMenu_AutoLogonOptions.size() ? g_NGPCarMenu_AutoLogonOptions[m_iMenuAutoLogonOption] : m_sAutoLogon.c_str())).c_str());

		pluginINIFile.SetValue(L"Default", L"RallySchoolMenuReplacement", _ToWString(m_sRallySchoolMenuReplacement).c_str());
		if (g_pOrigRallySchoolMenuText != nullptr)
		{
			// Set RallySchool menu replacement up-to-date in case the INI file had a new RallySchoolMenuReplacement value
			if (m_iMenuRallySchoolMenuOption >= 2)
				wcsncpy_s(g_wszRallySchoolMenuReplacementText, _ToWString(m_sRallySchoolMenuReplacement).c_str(), COUNT_OF_ITEMS(g_wszRallySchoolMenuReplacementText));
			else
				wcsncpy_s(g_wszRallySchoolMenuReplacementText, g_pOrigRallySchoolMenuText, COUNT_OF_ITEMS(g_wszRallySchoolMenuReplacementText));
		}

		pluginINIFile.SetValue(L"Default", L"CockpitCameraShaking", std::to_wstring(this->m_iMenuCockpitCameraShaking).c_str());
		pluginINIFile.SetValue(L"Default", L"CockpitSteeringWheel", std::to_wstring(this->m_iMenuCockpitSteeringWheel).c_str());
		pluginINIFile.SetValue(L"Default", L"CockpitWipers", std::to_wstring(this->m_iMenuCockpitWipers).c_str());
		pluginINIFile.SetValue(L"Default", L"CockpitWindscreen", std::to_wstring(this->m_iMenuCockpitWindscreen).c_str());
		pluginINIFile.SetValue(L"Default", L"CockpitOverrideFOV", std::to_wstring(this->m_iMenuCockpitOverrideFOV).c_str());

		pluginINIFile.SaveFile(sIniFileName.c_str());
	}
	catch (...)
	{
		LogPrint("ERROR CNGPCarMenu.SaveSettingsToPluginINIFile. %s INI writing failed", sIniFileName.c_str());
		m_sMenuStatusText1 = sIniFileName + " INI writing failed";
	}

	DebugPrint("Exit SaveSettingsToPluginINIFile");
}

void CNGPCarMenu::SaveSettingsToRBRTMRecentMaps()
{
	// Save RBRTM shakedown recent maps entries
	if (this->m_iMenuRBRTMOption > 0)
	{
		std::string sIniFileNameRBRTMRecentMaps = m_sRBRRootDir + "\\Plugins\\" VS_PROJECT_NAME "\\RBRTMRecentMaps.ini";

		CSimpleIni rbrtmRecentMapsINI;
		//rbrtmRecentMapsINI.SetUnicode(TRUE);
		rbrtmRecentMapsINI.LoadFile(sIniFileNameRBRTMRecentMaps.c_str());

		int idx = 0;
		for (auto& item : m_recentMapsRBRTM)
		{
			if (idx >= m_recentMapsMaxCountRBRTM) break;

			if (item->mapID > 0)
			{
				idx++;
				rbrtmRecentMapsINI.SetLongValue("Default", (std::string("RBRTM_RecentMap").append(std::to_string(idx)).c_str()), item->mapID);
			}
		}

		rbrtmRecentMapsINI.SaveFile(sIniFileNameRBRTMRecentMaps.c_str());
	}
}

void CNGPCarMenu::SaveSettingsToRBRRXRecentMaps()
{
	// Save RBRRX recent maps entries
	if (this->m_iMenuRBRRXOption > 0)
	{
		std::string sIniFileNameRBRRXRecentMaps = m_sRBRRootDir + "\\Plugins\\" VS_PROJECT_NAME "\\RBRRXRecentMaps.ini";

		CSimpleIni rbrrxRecentMapsINI;
		//rbrrxRecentMapsINI.SetUnicode(TRUE);
		rbrrxRecentMapsINI.LoadFile(sIniFileNameRBRRXRecentMaps.c_str());

		int idx = 0;
		for (auto& item : m_recentMapsRBRRX)
		{
			if (idx >= m_recentMapsMaxCountRBRRX) break;

			if (!item->folderName.empty())
			{
				idx++;
				rbrrxRecentMapsINI.SetValue("Default", (std::string("RBRRX_RecentMap").append(std::to_string(idx)).c_str()), item->folderName.c_str());
			}
		}

		rbrrxRecentMapsINI.SaveFile(sIniFileNameRBRRXRecentMaps.c_str());
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
/*
#if USE_DEBUG == 1
					for (int idx = g_pRBRPluginMenuSystem->pluginsMenuObj->firstSelectableItemIdx; idx < g_pRBRPluginMenuSystem->pluginsMenuObj->numOfItems - 1; idx++)
					{
						pItemArr = (PRBRPluginMenuItemObj3)g_pRBRPluginMenuSystem->pluginsMenuObj->pItemObj[idx];
						DebugPrint("PluginMenuIdx=%d %s", idx, pItemArr->szItemName);
					}
#endif
*/
					//
					// Check if custom plugin is installed. If this is "RBRTM" integration call then check that RBRTM is a supported version
					//
					for (int idx = g_pRBRPluginMenuSystem->pluginsMenuObj->firstSelectableItemIdx; idx < g_pRBRPluginMenuSystem->pluginsMenuObj->numOfItems - 1; idx++)
					{
						pItemArr = (PRBRPluginMenuItemObj3)g_pRBRPluginMenuSystem->pluginsMenuObj->pItemObj[idx];

						if (/*g_pRBRPluginMenuSystem->pluginsMenuObj->pItemObj[idx]*/ pItemArr != nullptr && strncmp(pItemArr->szItemName, customPluginName.c_str(), customPluginName.length()) == 0)
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
						iResultPluginIdx = -1;

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
		if (m_iMenuRBRTMOption == 1 && m_iRBRTMPluginMenuIdx == 0)
			// RBRTM integration is enabled, but the RBRTM linking is not yet initialized. Do it now when RBR has already loaded all custom plugins.
			// If initialization succeeds then m_iRBRTMPluginMenuIdx > 0 and m_pRBRTMPlugin != NULL and g_pRBRPluginMenuSystem->customPluginMenuObj != NULL, otherwise PluginMenuIdx=-1
			if (InitPluginIntegration(m_sRBRTMPluginTitle, TRUE) != 0)
				iInitCount++;
			else
				iStillWaitingInit++;

		if (m_iMenuRBRRXOption == 1 && m_iRBRRXPluginMenuIdx == 0)
		{
			// RBRRX integration is enabled, but the linking is not yet initialized. Do it now
			m_iRBRRXPluginMenuIdx = InitPluginIntegration("RBR_RX", FALSE);
			if (m_iRBRRXPluginMenuIdx != 0)
			{
				iInitCount++;

				if (m_iRBRRXPluginMenuIdx > 0)
				{
					try
					{
						m_pRBRRXPlugin = (PRBRRXPlugin)::GetModuleBaseAddr("RBR_RX.DLL");
						if(m_pRBRRXPlugin != nullptr)
						{
							m_pOrigMapMenuItemsRBRRX = m_pRBRRXPlugin->pMenuItems;
							m_origNumOfItemsMenuItemsRBRRX = m_pRBRRXPlugin->numOfItems;

							// Make the red focus bar a bit wider and move all menus few chars to the left to make more room for longer BTB stage names
							DWORD dwValue;
							dwValue = 0x43C00000;
							WriteOpCodeBuffer(&m_pRBRRXPlugin->menuFocusWidth, (const BYTE*)&dwValue, sizeof(DWORD));
							dwValue = 0x41800000;
							WriteOpCodeBuffer(&m_pRBRRXPlugin->menuPosX, (const BYTE*)&dwValue, sizeof(DWORD));
						}
					}
					catch (...)
					{
					}					

					if (m_pRBRRXPlugin == nullptr)
					{
						m_iRBRRXPluginMenuIdx = -1;
						LogPrint("ERROR. Failed to read the base address of RBR_RX plugin. For some reason RBR_RX.DLL library is unavailable");
					}
				}
			}
			else
				iStillWaitingInit++;
		}

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


//-------------------------------------------------------------------------------------------------------------------
// AutoLogin feature enabled. Do the logon sequence and take the RBR menu automatically to the defined menu (Default profile, Main, Options, Plugins, Custom plugin)
//
void CNGPCarMenu::StartNewAutoLogonSequence(bool showAutoLogonProgressText)
{
	m_bShowAutoLogonProgressText = showAutoLogonProgressText;
	m_autoLogonSequenceLabel.clear();
	m_autoLogonSequenceLabel.str(std::wstring());
	for (auto& item : m_autoLogonSequenceSteps)
		m_autoLogonSequenceLabel << (m_autoLogonSequenceLabel.tellp() != std::streampos(0) ? L"/" : L"") << _ToWString(item);

	m_dwAutoLogonEventStartTick = GetTickCount32();
	m_iAutoLogonMenuState = (m_bAutoLogonWaitProfile || m_iAutoLogonMenuState != -1 ? 3 : 1); // 3=Wait main menu (do not autoLoad a profile on bootup) 1=AutoLoad profile on RBR bootup
}

void CNGPCarMenu::DoAutoLogonSequence()
{
	if (m_iAutoLogonMenuState > 1 && ((GetTickCount32() - m_dwAutoLogonEventStartTick) >= (DWORD) (m_bAutoLogonWaitProfile ? 15000 : 4000) || m_autoLogonSequenceSteps.size() <= 0) )
	{
		// Autologon sequence step took too long to complete (more than 15 secs if waitProfile enabled, otherwise 4 secs). Abort it. Maybe user pressed some keys to mess up it or RBR is waiting for something strange thing to happen
		m_iAutoLogonMenuState = 0;
		if(m_autoLogonSequenceSteps.size() <= 0) LogPrint("Menu navigation queue is empty. Navigation completed");
		else LogPrint("WARNING. Automatic menu navigation aborted because of timeout");
		return;
	} 

	if (m_iAutoLogonMenuState == 1 && g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_STARTUP])
	{
		// Auto-load the first default profile. If there are no profiles then abort the logon sequence
		if (g_pRBRMenuSystem->currentMenuObj->pExtMenuObj != nullptr)
		{
			if (g_pRBRMenuSystem->currentMenuObj->pExtMenuObj->numOfItems >= 1 /*&& g_pRBRMenuSystem->currentMenuObj->pExtMenuObj->selectedItemIdx == 1*/)
			{
				// Choose the first profile automatically if the first profile is not yet selected
				if(g_pRBRMenuSystem->currentMenuObj->pExtMenuObj->selectedItemIdx == 0)
					g_pRBRMenuSystem->currentMenuObj->pExtMenuObj->selectedItemIdx = 1;

				SendMessage(g_hRBRWnd, WM_KEYDOWN, VK_RETURN, 0);
				SendMessage(g_hRBRWnd, WM_KEYUP, VK_RETURN, 0);
				m_dwAutoLogonEventStartTick = GetTickCount32();
				m_iAutoLogonMenuState++;
			}
			else
			{
				// Abort auto-logon sequence because there are no profiles availabe. User should create the MULLIGATAWNY profile
				m_iAutoLogonMenuState = 0;
				LogPrint("WARNING. Autologon sequence aborted because there are no profiles available. Please create a driver profile");
			}
		}
	}
	else if (m_iAutoLogonMenuState == 2 && g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_STARTUP])
	{
		// Accept "OK profile load" screen, but give RBR few msecs time to complete the profile loading
		if ( (GetTickCount32() - m_dwAutoLogonEventStartTick) >= 100)
		{
			SendMessage(g_hRBRWnd, WM_KEYDOWN, VK_RETURN, 0);
			SendMessage(g_hRBRWnd, WM_KEYUP, VK_RETURN, 0);
			m_dwAutoLogonEventStartTick = GetTickCount32();
			m_iAutoLogonMenuState++;
		}
	}
	else
	{
		// LogonMenuState==3 (profile is already loaded, so the actual menu navigation can proceed)

		if (m_bAutoLogonWaitProfile && g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_MAIN])
		{
			// The plugin waited user to choose a profile. Now when user selected the profile continue the normal autologon sequence
			m_dwAutoLogonEventStartTick = GetTickCount32();
			m_bAutoLogonWaitProfile = FALSE;
			return;
		}

		if ((GetTickCount32() - m_dwAutoLogonEventStartTick) >= 150)
		{
			int  newSelectedItemIdx = -1;
			//bool bRBRRXAutoLogon = FALSE;

			// If auto-logon sequence has Plugings menu and the menu is already there then remove unnecessary Main/Options root path (no need to navigate there and then back to plugins menu)
			if (g_pRBRMenuSystem->currentMenuObj == g_pRBRPluginMenuSystem->pluginsMenuObj)
			{
				if (_iEqual(m_autoLogonSequenceSteps[0], "main", true) && _iEqual(m_autoLogonSequenceSteps[1], "options", true) && _iEqual(m_autoLogonSequenceSteps[2], "plugins", true))
				{
					m_autoLogonSequenceSteps.pop_front();
					m_autoLogonSequenceSteps.pop_front();
				}
			}

			if (_iEqual(m_autoLogonSequenceSteps[0], "main", true) && g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_MAIN])
			{
				m_autoLogonSequenceSteps.pop_front();
				if (m_autoLogonSequenceSteps.size() > 0)
				{
					// The next menu in mainMenu will be DriverProfile or Options or Replay
					if (_iEqual(m_autoLogonSequenceSteps[0], "driverprofile", true)) newSelectedItemIdx = 0x08;
					else if (_iEqual(m_autoLogonSequenceSteps[0], "options", true)) newSelectedItemIdx = 0x09;
					else if (_iEqual(m_autoLogonSequenceSteps[0], "replay", true)) newSelectedItemIdx = -1;
					else newSelectedItemIdx = -2;
				}
			}
			else if (_iEqual(m_autoLogonSequenceSteps[0], "replay", true) && g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_MAIN])
			{
				// Special "Replay" autoLogon option. The next step is always the replay filename (or if the RPL param name is missing then do nothing).
				// External apps can use AutoLogon=Replay and AutoLogonParam1=someFileName.rpl options to force RBR to replay the RPL file automatically at RBR startup.
				m_autoLogonSequenceSteps.pop_front();
				if (m_autoLogonSequenceSteps.size() > 0)
				{
					std::string sReplayFileName = m_autoLogonSequenceSteps[0];
					m_autoLogonSequenceSteps.clear();
					::RBRAPI_Replay(m_sRBRRootDir, sReplayFileName.c_str());
				}
			}
			else if (_iEqual(m_autoLogonSequenceSteps[0], "driverprofile", true) && g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_DRIVERPROFILE])
			{
				m_autoLogonSequenceSteps.pop_front();
				if (m_autoLogonSequenceSteps.size() > 0)
				{
					// The next menu in driverProfile will be SaveProfile or LoadReplay menu screen
					if (_iEqual(m_autoLogonSequenceSteps[0], "saveprofile", true)) newSelectedItemIdx = 0x05;
					else if (_iEqual(m_autoLogonSequenceSteps[0], "loadreplay", true)) { newSelectedItemIdx = 0x06; m_autoLogonSequenceSteps.clear(); }
					else newSelectedItemIdx = -2;
				}
			}
			else if (_iEqual(m_autoLogonSequenceSteps[0], "saveprofile", true) && g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_DRIVERPROFILE])
			{				
				// Restore focus line back to default selection and complete the sequence (profile menu screen resets always back to the first line when opened)
				g_pRBRMenuSystem->menuObj[RBRMENUIDX_DRIVERPROFILE]->selectedItemIdx = g_pRBRMenuSystem->menuObj[RBRMENUIDX_DRIVERPROFILE]->firstSelectableItemIdx;
				m_iProfileMenuPrevSelectedIdx = g_pRBRMenuSystem->menuObj[RBRMENUIDX_DRIVERPROFILE]->selectedItemIdx;
				m_autoLogonSequenceSteps.clear();
			}
			else if (_iEqual(m_autoLogonSequenceSteps[0], "options", true) && g_pRBRMenuSystem->currentMenuObj == g_pRBRPluginMenuSystem->optionsMenuObj)
			{
				m_autoLogonSequenceSteps.pop_front();
				if (m_autoLogonSequenceSteps.size() > 0)
				{
					// The next menu in options will be Plugins
					if (_iEqual(m_autoLogonSequenceSteps[0], "plugins", true)) newSelectedItemIdx = 0x0A;
					else newSelectedItemIdx = -2;
				}
			}
			else if (_iEqual(m_autoLogonSequenceSteps[0], "plugins", true) && g_pRBRMenuSystem->currentMenuObj == g_pRBRPluginMenuSystem->pluginsMenuObj)
			{
				m_autoLogonSequenceSteps.pop_front();
				if (m_autoLogonSequenceSteps.size() > 0)
				{
					// The next menu in plugins will be a named custom plugin
					for (int idx = g_pRBRPluginMenuSystem->pluginsMenuObj->firstSelectableItemIdx; idx < g_pRBRPluginMenuSystem->pluginsMenuObj->numOfItems - 1; idx++)
					{
						PRBRPluginMenuItemObj3 pItemArr = (PRBRPluginMenuItemObj3)g_pRBRPluginMenuSystem->pluginsMenuObj->pItemObj[idx];
						if (pItemArr != nullptr && pItemArr->szItemName != nullptr && _iEqual(pItemArr->szItemName, m_autoLogonSequenceSteps[0]))
						{
							if(m_bShowAutoLogonProgressText)
								LogPrint("Auto navigating to the custom plugin %s", m_autoLogonSequenceSteps[0].c_str());

							//bRBRRXAutoLogon = _iEqual(m_autoLogonSequenceSteps[0], "rbr_rx", true);
							newSelectedItemIdx = idx;
							break;
						}
					}					

					if(newSelectedItemIdx < 0)
						LogPrint("WARNING. Menu navigation to %s plugin failed because the plugin is not installed", m_autoLogonSequenceSteps[0].c_str());

					// Complete the sequence when custom plugin is activated
					m_autoLogonSequenceSteps.clear();
				}
			}

			if (newSelectedItemIdx >= 0)
			{
				g_pRBRMenuSystem->currentMenuObj->selectedItemIdx = newSelectedItemIdx;
				SendMessage(g_hRBRWnd, WM_KEYDOWN, VK_RETURN, 0);
				SendMessage(g_hRBRWnd, WM_KEYUP, VK_RETURN, 0);				
				m_dwAutoLogonEventStartTick = GetTickCount32();
			}
			else if (newSelectedItemIdx == -2 && m_autoLogonSequenceSteps.size() > 0)
			{
				LogPrint("WARNING: Invalid menu navigation sequence. Unexpected submenu entry %s. Navigation aborted", m_autoLogonSequenceSteps[0].c_str());
				m_iAutoLogonMenuState = 0;
			}
			
			if (m_autoLogonSequenceSteps.size() <= 0)
			{
				// Autlogon completed, all menu steps executed
				m_iAutoLogonMenuState = 0;
				m_dwAutoLogonEventStartTick = 0;

				if (m_bShowAutoLogonProgressText)
					LogPrint("Automatic menu navigation completed");
			}
		}
	}
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
void CNGPCarMenu::InitCarSpecData_RBRCIT(int updatedCarSlotMenuIdx)
{
	DebugPrint("Enter CNGPCarMenu.InitCarSpecData_RBRCIT");

	CSimpleIniWEx ngpCarListINIFile;
	CSimpleIniWEx customCarSpecsINIFile;

	CSimpleIniWEx stockCarListINIFile;
	std::wstring sStockCarModelName;

	RSFJsonData rsfJsonData;

	static const char* szPhysicsCarFolder[8] = { "s_i2003" , "m_lancer", "t_coroll", "h_accent", "p_206", "s_i2000", "c_xsara", "mg_zr" };

	std::string sPath;
	sPath.reserve(_MAX_PATH);

	try
	{
		int iNumOfGears;

		if (fs::exists(m_rbrCITCarListFilePath))
			ngpCarListINIFile.LoadFileEx(m_rbrCITCarListFilePath.c_str());
		else if (!m_bRallySimFansPluginInstalled)
			// Add warning about missing RBRCIT carList.ini file
			wcsncpy_s(g_RBRCarSelectionMenuEntry[0].wszCarPhysicsCustomTxt, (m_rbrCITCarListFilePath + L" missing. Cannot show car specs").c_str(), COUNT_OF_ITEMS(g_RBRCarSelectionMenuEntry[0].wszCarPhysicsCustomTxt));

		// Load std RBR cars list and custom carSpec file (fex original car specs are set here). These are used if the phystics\<CarFolder> doesn't have NGP car description file
		customCarSpecsINIFile.LoadFile((m_sRBRRootDirW + L"\\Plugins\\" L"" VS_PROJECT_NAME L"\\CustomCarSpecs.ini").c_str());
		stockCarListINIFile.LoadFile((m_sRBRRootDirW + L"\\cars\\Cars.ini").c_str());

		// The loop uses menu idx order, not in car slot# idx order
		for (int idx = 0; idx < 8; idx++)
		{
			if (updatedCarSlotMenuIdx != -1 && updatedCarSlotMenuIdx != idx)
				continue;

			iNumOfGears = -1;
			sPath = CNGPCarMenu::m_sRBRRootDir + "\\physics\\" + szPhysicsCarFolder[idx];

			// Use rbr\cars\Cars.ini file to lookup the car model name ([CarXX] where xx=00..07 and CarName attribute)
			InitCarModelNameFromCarsFile(&stockCarListINIFile, &g_RBRCarSelectionMenuEntry[idx], idx);

			// Car model names are in WCHAR, but RBR uses CHAR strings in car menu names. Convert the wchar car model name to char string and use only N first chars.
			size_t len = 0;
			wcstombs_s(&len, g_RBRCarSelectionMenuEntry[idx].szCarMenuName, sizeof(g_RBRCarSelectionMenuEntry[idx].szCarMenuName), g_RBRCarSelectionMenuEntry[idx].wszCarModel, _TRUNCATE);

			if (!InitCarSpecDataFromPhysicsFile(sPath, &g_RBRCarSelectionMenuEntry[idx], &iNumOfGears) && g_RBRCarSelectionMenuEntry[idx].wszCarModel[0] == L'\0')
			{
				if (!m_bRallySimFansPluginInstalled)
				{
					LogPrint("Warning. Car in [Car0%d] slot doesn't have CarName attribute in Cars\\Cars.ini file or NGP model file in %s folder", ::RBRAPI_MapMenuIdxToCarID(idx), sPath.c_str());
				}
				else
				{
					DebugPrint("Notice. RallySimFans plugin doesn't install cars in all 0-7 slots. The slot [Car0%d] doesn't have a CarName option in Cars\\Cars.ini file", ::RBRAPI_MapMenuIdxToCarID(idx));
				}
			}

			// NGP car model or stock car model lookup to RBCIT/carList/carList.ini file. If the car specs are missing then try to use NGPCarMenu custom carspec INI file
			if (!InitCarSpecDataFromNGPFile(&ngpCarListINIFile, &g_RBRCarSelectionMenuEntry[idx], iNumOfGears))
			{
				if (InitCarSpecDataFromNGPFile(&customCarSpecsINIFile, &g_RBRCarSelectionMenuEntry[idx], iNumOfGears))
				{
					g_RBRCarSelectionMenuEntry[idx].wszCarPhysics3DModel[0] = L'\0'; // Clear warning about missing NGP car desc file because the custom file had missing specs (original cars?)
				}
				else if (m_bRallySimFansPluginInstalled)
				{
					// By default RSF doesn't need RBRCIT tool, so drivers probably don't have carlist.ini file from RBRCIT tool.
					// In that case try to read car specs from RSF cars.json (car_id) -> car_group_map.json (group_id) -> cargroups.json (group_id)
					if(!InitCarSpecDataFromRSFFile(&rsfJsonData, &g_RBRCarSelectionMenuEntry[idx], iNumOfGears))
						LogPrint(L"Warning. Car model %s not found from RSF cars.json files. Car details are missing", g_RBRCarSelectionMenuEntry[idx].wszCarModel);
				}
				else 
				{					
					LogPrint(L"Warning. Car model %s not found from NGP carList.ini or %s file. Car details are missing",
						g_RBRCarSelectionMenuEntry[idx].wszCarModel,
						(m_sRBRRootDirW + L"\\Plugins\\" L"" VS_PROJECT_NAME L"\\CustomCarSpecs.ini").c_str()
					);
				}
			}

			if (updatedCarSlotMenuIdx != -1)
				break;
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
	//DebugPrint("Enter CNGPCarMenu.InitCarSpecData_EASYRBR");

	std::wstring sTextValue;

	CSimpleIniW carsINIFile;
	CSimpleIniWEx ngpCarListINIFile;
	CSimpleIniWEx customCarSpecsINIFile;

	CSimpleIniWEx stockCarListINIFile;
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

			InitCarModelNameFromCarsFile(&stockCarListINIFile, &g_RBRCarSelectionMenuEntry[idx], idx);

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

	//DebugPrint("Exit CNGPCarMenu.InitCarSpecData_EASYRBR");
}


//-------------------------------------------------------------------------------------------------
// AudioFMOD support: Init the custom name of the installed AudioFMOD sound bank
// Step 1: Read rbr\AudioFMOD\AudioFMOD.ini and bankName attribute in [CarXX] INI section
//
void CNGPCarMenu::InitCarSpecAudio()
{
	//DebugPrint("Enter CNGPCarMenu.InitCarSpecAudio");

	CSimpleIniW audioFMODINIFile;
	std::string sPath;
	std::string sPathReadme;
	std::wstring sTextLine;
	sTextLine.reserve(128);

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
					{
						wcsncpy_s(g_RBRCarSelectionMenuEntry[idx].wszCarFMODBank, (std::wstring(g_pRBRPlugin->GetLangStr(L"FMOD")) + L" " + sTextValue).c_str(), COUNT_OF_ITEMS(g_RBRCarSelectionMenuEntry[idx].wszCarFMODBank));

						// Read the name of FMOD authors (readme.fmodBankName.txt and Authors line there)
						sPathReadme = m_sRBRRootDir + "\\AudioFMOD\\readme." + _ToString(sTextValue) + ".txt";
						if (fs::exists(sPathReadme))
						{
							std::wifstream fmodReadmeFile(sPathReadme);
							while (std::getline(fmodReadmeFile, sTextLine))
							{
								_Trim(sTextLine);
								if (_iStarts_With(sTextLine, L"authors", true))
								{
									sTextLine.erase(0, 7);
									_Trim(sTextLine);
									wcsncpy_s(g_RBRCarSelectionMenuEntry[idx].wszCarFMODBankAuthors, sTextLine.c_str(), COUNT_OF_ITEMS(g_RBRCarSelectionMenuEntry[idx].wszCarFMODBankAuthors));
									break;
								}
							}
						}
					}						
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

	//DebugPrint("Exit CNGPCarMenu.InitCarSpecAudio");
}


//-------------------------------------------------------------------------------------------------
// Read the car model name from RBR cars\Cars.ini file and pretify the name if necessary.
// 
bool CNGPCarMenu::InitCarModelNameFromCarsFile(CSimpleIniWEx* stockCarListINIFile, PRBRCarSelectionMenuEntry pRBRCarSelectionMenuEntry, int menuIdx)
{
	std::wstring sStockCarModelName;
	std::wstring sTextValue;

	WCHAR wszStockCarINISection[8];

	try
	{
		swprintf_s(wszStockCarINISection, COUNT_OF_ITEMS(wszStockCarINISection), L"Car0%d", ::RBRAPI_MapMenuIdxToCarID(menuIdx));
		sStockCarModelName = stockCarListINIFile->GetValueEx(wszStockCarINISection, L"", L"CarName", L"");
		
		// Read "Cars\YARIS_WRC18\yaris_wrc.sgc" model name, then use only the parent path value (ie. Cars\YARIS_WRC18)
		sTextValue = stockCarListINIFile->GetValueEx(wszStockCarINISection, L"", L"FileName", L"");
		sTextValue = fs::path(sTextValue).parent_path().c_str();
		wcsncpy_s(pRBRCarSelectionMenuEntry->wszCarModelFolder, sTextValue.c_str(), COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarModelFolder));

		if (sStockCarModelName.length() >= 4)
		{
			// If the car name ends in "xxx #2]" tag then remove it as unecessary trailing tag (some cars like "Renault Twingo R1 #2]" or "Open ADAM R2 #2]" has this weird tag in NGP car name value in Cars.ini file)
			if (sStockCarModelName[sStockCarModelName.length() - 3] == L'#' && sStockCarModelName[sStockCarModelName.length() - 1] == L']')
			{
				sStockCarModelName.erase(sStockCarModelName.length() - 3);
				_Trim(sStockCarModelName);
			}

			// If the car name has "xxx (2)" type of trailing tag (original car names set by RBRCIT in Cars.ini has the slot number) then remove that unnecessary tag
			if (sStockCarModelName[sStockCarModelName.length() - 3] == L'(' && sStockCarModelName[sStockCarModelName.length() - 1] == L')' && iswdigit(sStockCarModelName[sStockCarModelName.length() - 2]) )
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

	if (sStockCarModelName.empty() /*|| (m_bRallySimFansPluginInstalled && updatedCarSlotMenuIdx != -1)*/)
		pRBRCarSelectionMenuEntry->wszCarModel[0] = L'\0';
	else
		wcsncpy_s(pRBRCarSelectionMenuEntry->wszCarModel, sStockCarModelName.c_str(), COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarModel));

	return true;
}


//-------------------------------------------------------------------------------------------------
// Read rbr\physics\<carSlotNameFolder>\ngpCarNameFile file and init revision/3dModel/SpecYear attributes
//
bool CNGPCarMenu::InitCarSpecDataFromPhysicsFile(const std::string &folderName, PRBRCarSelectionMenuEntry pRBRCarSelectionMenuEntry, int* outNumOfGears)
{
	const fs::path fsFolderName(folderName);

	//std::wstring wfsFileName;
	std::string  fsFileName;
	fsFileName.reserve(256);
	
	std::string  sTextLine;
	std::wstring wsTextLine;
	sTextLine.reserve(256);
	wsTextLine.reserve(256);

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
		if (fs::exists(fsFolderName))
		{
			for (const auto& entry : fs::directory_iterator(fsFolderName))
			{
				if (entry.is_regular_file() && entry.path().extension().compare(".lsp") != 0)
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
						if (wsTextLine.length() >= 2)
							wsTextLine.erase(std::unique(wsTextLine.begin(), wsTextLine.end(), [](WCHAR a, WCHAR b) { return iswspace(a) && iswspace(b); }), wsTextLine.end());

						if (_iStarts_With(wsTextLine, L"revision", TRUE))
						{
							bResult = TRUE;

							// Remove the revision tag and replace it with a language translated str version (or if missing then re-add the original "revision" text)
							wsTextLine.erase(0, COUNT_OF_ITEMS(L"revision") - 1);
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
						wcsncpy_s(pRBRCarSelectionMenuEntry->wszCarPhysics, (_ToWString(fsFileName)).c_str(), COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarPhysics));

						// If NGP car model file found then set carModel string value if it was not already set based on Cars.ini CarName attribute values and no need to iterate through other files (without file extension)
						if (pRBRCarSelectionMenuEntry->wszCarModel[0] == L'\0')
						{
							wcsncpy_s(pRBRCarSelectionMenuEntry->wszCarModel, pRBRCarSelectionMenuEntry->wszCarPhysics, COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarModel));
							// Make sure the str is nullterminated in case wcsncpy had to truncate the string because of out-of-buffer space
							pRBRCarSelectionMenuEntry->wszCarModel[COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarModel) - 1] = '\0';
						}
						break;
					}
				}
			}
		}

		if (!bResult)
		{
			if(!m_bRallySimFansPluginInstalled)
				LogPrint("Missing NGP model description file in %s folder. It is recommended to use RBRCIT or EasyRBR tool to setup custom NGP cars", folderName.c_str());

			std::wstring wFolderName = _ToWString(folderName);
			// Show warning that RBRCIT/NGP carModel file is missing
			wcsncpy_s(pRBRCarSelectionMenuEntry->wszCarPhysics3DModel, (wFolderName + L" NGP model description missing").substr(0, COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarPhysics3DModel)-1).c_str(), COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarPhysics3DModel));
		}

		//DebugPrint("DEBUG InitCarSpecDataFromPhysicsFile. bResult=%d folderName=%s CarModel=%s", bResult, folderName.c_str(), _ToString(pRBRCarSelectionMenuEntry->wszCarModel).c_str());
	}
	catch (const fs::filesystem_error& ex)
	{
		DebugPrint("ERROR CNGPCarMenu.InitCarSpecDataFromPhysicsFile. %s %s", folderName.c_str(), ex.what());
		bResult = FALSE;
	}
	catch(...)
	{
		LogPrint("ERROR CNGPCarMenu.InitCarSpecDataFromPhysicsFile. %s doesn't exist or error while reading NGP model spec file", folderName.c_str());
		bResult = FALSE;
	}

	return bResult;
}


//-------------------------------------------------------------------------------------------------
// Init car spec data from NGP ini files (HP, year, transmissions, drive wheels, FIA category).
// Name of the NGP carList.ini entry is in pRBRCarSelectionMenuEntry->wszCarModel field.
//
bool CNGPCarMenu::InitCarSpecDataFromNGPFile(CSimpleIniWEx* ngpCarListINIFile, PRBRCarSelectionMenuEntry pRBRCarSelectionMenuEntry, int numOfGears)
{
	bool bResult = FALSE;

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
				wcsncpy_s(pRBRCarSelectionMenuEntry->wszCarListSectionName, iter->pItem, COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarListSectionName));

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
		}
	}
	catch (...)
	{
		// Hmmm...Something went wrong.
		LogPrint("ERROR CNGPCarMenu.InitCarSpecDataFromNGPFile. carList.ini INI file reading error");
		bResult = FALSE;
	}

	return bResult;;
}


//-------------------------------------------------------------------------------------------------
// Read car details (FIA category) from RSF json files (cars.json (car_id) -> car_group_map.json (group_id) -> carGroups.json (group_id and name)
//
bool CNGPCarMenu::InitCarSpecDataFromRSFFile(PRSFJsonData rsfJsonData, PRBRCarSelectionMenuEntry pRBRCarSelectionMenuEntry, int numOfGears)
{
	int rsfCarID = 0;
	std::vector<int> rsfCarGroupMap;
	const char* pszCarGroupName;

	if (rsfJsonData == nullptr || pRBRCarSelectionMenuEntry == nullptr)
		return FALSE;

	try
	{
		// If rsfJsonData is not yet read and parsed then do it now
/*		if (rsfJsonData->carsJson.IsNull())
		{
			std::ifstream jsonFile(m_sRBRRootDir + "\\rsfdata\\cache\\cars.json");
			rapidjson::IStreamWrapper isw(jsonFile);
			rsfJsonData->carsJson.ParseStream(isw);
		}

		if (rsfJsonData->carsJson.HasParseError() || !rsfJsonData->carsJson.IsArray())
			return FALSE;
*/
		rsfCarID = GetCarIDFromRSFFile(rsfJsonData, pRBRCarSelectionMenuEntry->szCarMenuName);
		if (rsfCarID <= 0)
			return FALSE;

		if (rsfJsonData->carGroupMapJson.IsNull())
		{
			std::ifstream jsonFile(m_sRBRRootDir + "\\rsfdata\\cache\\car_group_map.json");
			rapidjson::IStreamWrapper isw(jsonFile);
			rsfJsonData->carGroupMapJson.ParseStream(isw);
		}

		if (rsfJsonData->carGroupMapJson.HasParseError() || !rsfJsonData->carGroupMapJson.IsArray())
			return FALSE;

		if (rsfJsonData->carGroupsJson.IsNull())
		{
			std::ifstream jsonFile(m_sRBRRootDir + "\\rsfdata\\cache\\cargroups.json");
			rapidjson::IStreamWrapper isw(jsonFile);
			rsfJsonData->carGroupsJson.ParseStream(isw);
		}

		if (rsfJsonData->carGroupsJson.HasParseError() || !rsfJsonData->carGroupsJson.IsArray())
			return FALSE;

		// Find the rsf carID
/*		for (rapidjson::Value::ConstValueIterator itr = rsfJsonData->carsJson.Begin(); itr != rsfJsonData->carsJson.End(); ++itr)
		{
			if ((*itr).HasMember("name") && _iEqual(((*itr)["name"].GetString()), pRBRCarSelectionMenuEntry->szCarMenuName))
			{
				if ((*itr).HasMember("id")) rsfCarID = atoi((*itr)["id"].GetString());
				break;
			}
		}

		if (rsfCarID == 0)
			return FALSE;
*/

		// Find groups of the car (one or more)
		rsfCarGroupMap.reserve(5);
		for (rapidjson::Value::ConstValueIterator itr = rsfJsonData->carGroupMapJson.Begin(); itr != rsfJsonData->carGroupMapJson.End(); ++itr)
		{
			if ((*itr).HasMember("car_id") && atoi((*itr)["car_id"].GetString()) == rsfCarID)
			{
				if ((*itr).HasMember("group_id")) rsfCarGroupMap.push_back(atoi((*itr)["group_id"].GetString()));
			}
		}

		if (rsfCarGroupMap.empty())
			return FALSE;

		// Find name of the car groups (ignore "ALL" and the name of car itself
		for (rapidjson::Value::ConstValueIterator itr = rsfJsonData->carGroupsJson.Begin(); itr != rsfJsonData->carGroupsJson.End(); ++itr)
		{
			if ((*itr).HasMember("id") && find(rsfCarGroupMap.begin(), rsfCarGroupMap.end(), atoi((*itr)["id"].GetString())) != rsfCarGroupMap.end())
			{
				if ((*itr).HasMember("main") && atoi((*itr)["main"].GetString()) != 0)
				{
					if ((*itr).HasMember("name"))
					{
						pszCarGroupName = (*itr)["name"].GetString();
						if (!_iEqual(pszCarGroupName, "all", true))
						{
							// Set the FIACategory for this car
							strncpy_s(pRBRCarSelectionMenuEntry->szCarCategory, pszCarGroupName, COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->szCarCategory));
							return TRUE;
						}
					}
				}
			}
		}
	}
	catch (...)
	{
		LogPrint("ERROR. Failed to read car spec details from RSF car json files");
	}
	
	return FALSE;
}

int CNGPCarMenu::GetCarIDFromRSFFile(PRSFJsonData rsfJsonData, std::string carModelName)
{
	int rsfCarID = 0;

	if (carModelName.empty())
		return 0;

	try
	{
		std::string rsfCarModelName;
		RSFJsonData tempRsfJsonData;

		if (rsfJsonData == nullptr)
			rsfJsonData = &tempRsfJsonData;

		// If rsfJsonData is not yet read and parsed then do it now
		if (rsfJsonData->carsJson.IsNull())
		{
			std::ifstream jsonFile(m_sRBRRootDir + "\\rsfdata\\cache\\cars.json");
			rapidjson::IStreamWrapper isw(jsonFile);
			rsfJsonData->carsJson.ParseStream(isw);
		}

		if (rsfJsonData->carsJson.HasParseError() || !rsfJsonData->carsJson.IsArray())
			return 0;

		// Find the rsf carID
		_ToLowerCase(carModelName);
		for (rapidjson::Value::ConstValueIterator itr = rsfJsonData->carsJson.Begin(); itr != rsfJsonData->carsJson.End(); ++itr)
		{
			if ((*itr).HasMember("name"))
			{
				rsfCarModelName = ((*itr)["name"].GetString());
				_Trim(rsfCarModelName);
				if (_iEqual(rsfCarModelName, carModelName, true))
				{
					if ((*itr).HasMember("id")) 
						rsfCarID = atoi((*itr)["id"].GetString());						

					break;
				}
			}
		}
	}
	catch (...)
	{
		LogPrint("ERROR. Failed to read carID from RSF car json files");
	}

	return rsfCarID;
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

	// The loop uses menu order, not slot# order
	for (int idx = 0; idx < 8; idx++)
	{
		// Car model names are in WCHAR, but RBR uses CHAR strings in car menu names. Convert the wchar car model name to char string and use only N first chars.
		wcstombs_s(&len, g_RBRCarSelectionMenuEntry[idx].szCarMenuName, sizeof(g_RBRCarSelectionMenuEntry[idx].szCarMenuName), g_RBRCarSelectionMenuEntry[idx].wszCarModel, _TRUNCATE);
		
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
// Modify the model files of the current car (hide/show windscreen/wipers/steeringWheel and override internal cockpit FOV)
//
bool CNGPCarMenu::ModifyCarModelIniFile(CSimpleIniWEx* carModelIniFile, const std::wstring& section, const std::wstring& key, const std::wstring& newOptionValue, bool restoreBackupValue, bool forceBackupValue)
{
	bool bModifiedIniFile = false;
	std::wstring sTextValue;
	std::wstring sBackupOptionKey; 

	if (carModelIniFile == nullptr)
		return false;

	// NGPCarMenuComment option doesn't need backup key
	if(!_iEqual(key, L"ngpcarmenucomment", true))
		sBackupOptionKey = std::wstring(L"Backup_") + key;

	if (!restoreBackupValue)
	{
		bool bNewOpenEqual; 
		sTextValue = carModelIniFile->GetValueEx(section, L"", key, L"");
		bNewOpenEqual = _iEqual(sTextValue, newOptionValue);

		if (forceBackupValue || !bNewOpenEqual)
		{
			if (!sBackupOptionKey.empty() && carModelIniFile->GetValueEx(section, L"", sBackupOptionKey, L"").empty())
			{
				carModelIniFile->SetValue(section.c_str(), sBackupOptionKey.c_str(), sTextValue.c_str());
				bModifiedIniFile = true;
			}

			if (!bNewOpenEqual)
			{
				carModelIniFile->SetValue(section.c_str(), key.c_str(), newOptionValue.c_str());
				bModifiedIniFile = true;
			}
		}
	}
	else
	{
		std::wstring sBackupOptionValue;
		sBackupOptionValue = carModelIniFile->GetValueEx(section, L"", sBackupOptionKey, L"");
		if (!sBackupOptionValue.empty())
		{
			carModelIniFile->SetValue(section.c_str(), sBackupOptionKey.c_str(), L"");
			carModelIniFile->SetValue(section.c_str(), key.c_str(), sBackupOptionValue.c_str());
			bModifiedIniFile = true;
		}
	}

	return bModifiedIniFile;
}

bool CNGPCarMenu::CopyCarModelIniCamSection(CSimpleIniWEx* carModelIniFile, const std::wstring& fromSection, const std::wstring& toSection, bool restoreBackupValue)
{
	bool bModifiedIniFile = false;
	std::wstring sFromTextValue;
	std::wstring sToTextValue;

	if (carModelIniFile == nullptr)
		return false;

	if (!restoreBackupValue)
	{
		sFromTextValue = carModelIniFile->GetValueEx(fromSection, L"", L"Pos", L"");
		bModifiedIniFile = ModifyCarModelIniFile(carModelIniFile, toSection, L"Pos", sFromTextValue, false, true) || bModifiedIniFile;

		sFromTextValue = carModelIniFile->GetValueEx(fromSection, L"", L"Target", L"");
		bModifiedIniFile = ModifyCarModelIniFile(carModelIniFile, toSection, L"Target", sFromTextValue, false, true) || bModifiedIniFile;

		sFromTextValue = carModelIniFile->GetValueEx(fromSection, L"", L"Up", L"");
		bModifiedIniFile = ModifyCarModelIniFile(carModelIniFile, toSection, L"Up", sFromTextValue, false, true) || bModifiedIniFile;

		sFromTextValue = carModelIniFile->GetValueEx(fromSection, L"", L"Near", L"");
		bModifiedIniFile = ModifyCarModelIniFile(carModelIniFile, toSection, L"Near", sFromTextValue, false, true) || bModifiedIniFile;

		sFromTextValue = carModelIniFile->GetValueEx(fromSection, L"", L"FOV", L"");
		bModifiedIniFile = ModifyCarModelIniFile(carModelIniFile, toSection, L"FOV", sFromTextValue, false, true) || bModifiedIniFile;

		sFromTextValue = carModelIniFile->GetValueEx(fromSection, L"", L"showExterior", L"0");
		bModifiedIniFile = ModifyCarModelIniFile(carModelIniFile, toSection, L"showExterior", sFromTextValue, false, true) || bModifiedIniFile;
	}
	else
	{
		bModifiedIniFile = ModifyCarModelIniFile(carModelIniFile, toSection, L"Pos", L"", true) || bModifiedIniFile;
		bModifiedIniFile = ModifyCarModelIniFile(carModelIniFile, toSection, L"Target", L"", true) || bModifiedIniFile;
		bModifiedIniFile = ModifyCarModelIniFile(carModelIniFile, toSection, L"Up", L"", true) || bModifiedIniFile;
		bModifiedIniFile = ModifyCarModelIniFile(carModelIniFile, toSection, L"Near", L"", true) || bModifiedIniFile;
		bModifiedIniFile = ModifyCarModelIniFile(carModelIniFile, toSection, L"FOV", L"", true) || bModifiedIniFile;
		bModifiedIniFile = ModifyCarModelIniFile(carModelIniFile, toSection, L"showExterior", L"", true) || bModifiedIniFile;
	}

	return bModifiedIniFile;
}

bool CNGPCarMenu::ModifyCarModelFiles(int carSlotID)
{	
	bool bModifiedIniFile = false;

	std::wstring sCarIniFileName;
	std::wstring sCarIniFileNameFullPath;

	std::wstring sTextValue;
	std::wstring sOptionValue;

	WCHAR wszStockCarINISection[8];

	//if (m_iMenuCockpitSteeringWheel <= 0 && m_iMenuCockpitWipers <= 0 && m_iMenuCockpitWindscreen <= 0 && m_iMenuCockpitOverrideFOV <= 0)
	//	return FALSE; 

	try
	{
		CSimpleIniWEx stockCarListINIFile;
		CSimpleIniWEx carModelIniFile;

		stockCarListINIFile.LoadFile((m_sRBRRootDirW + L"\\cars\\Cars.ini").c_str());
		swprintf_s(wszStockCarINISection, COUNT_OF_ITEMS(wszStockCarINISection), L"Car0%d", carSlotID);
		sCarIniFileName = stockCarListINIFile.GetValueEx(wszStockCarINISection, L"", L"IniFile", L"");
		sCarIniFileNameFullPath = m_sRBRRootDirW + L"\\" + sCarIniFileName;

		if (!sCarIniFileName.empty() && fs::exists(sCarIniFileNameFullPath))
		{			
			carModelIniFile.LoadFile(sCarIniFileNameFullPath.c_str());
			
			if (m_iMenuCockpitCameraShaking == 1)
			{
				// 0=default as set in model files, 1=disable shaking (bonnet = internal view), 2=enable shaking (bonnet = normal bonnet)
				// If internal cameraShaking is disabled then copy cam_internal values to cam_bonnet if the copied values are not there already
				bModifiedIniFile = CopyCarModelIniCamSection(&carModelIniFile, L"Cam_internal", L"Cam_bonnet", false) || bModifiedIniFile;
				bModifiedIniFile = ModifyCarModelIniFile(&carModelIniFile, L"Cam_bonnet", L"NGPCarMenuComment", L"cam_bonnet has internal cockpit cam values without shaking", false, false) || bModifiedIniFile;
			}
			else
			{
				bModifiedIniFile = CopyCarModelIniCamSection(&carModelIniFile, L"Cam_internal", L"Cam_bonnet", true) || bModifiedIniFile;
				bModifiedIniFile = ModifyCarModelIniFile(&carModelIniFile, L"Cam_bonnet", L"NGPCarMenuComment", L"", false, false) || bModifiedIniFile;
			}

			if (m_iMenuCockpitOverrideFOV > 0)
			{
				sOptionValue = std::to_wstring(m_fMenuCockpitOverrideFOVValue);
				bModifiedIniFile = ModifyCarModelIniFile(&carModelIniFile, L"Cam_internal", L"FOV", sOptionValue, false) || bModifiedIniFile;

				// 0=default as set in model files, 1=disable shaking (bonnet = internal view), 2=enable shaking (bonnet = normal bonnet)
				if(m_iMenuCockpitCameraShaking == 1)
					bModifiedIniFile = ModifyCarModelIniFile(&carModelIniFile, L"Cam_bonnet", L"FOV", sOptionValue, false) || bModifiedIniFile;
			}
			else
			{
				bModifiedIniFile = ModifyCarModelIniFile(&carModelIniFile, L"Cam_internal", L"FOV", L"", true) || bModifiedIniFile;
				
				if (m_iMenuCockpitCameraShaking != 1)
					bModifiedIniFile = ModifyCarModelIniFile(&carModelIniFile, L"Cam_bonnet", L"FOV", L"", true) || bModifiedIniFile;
			}

			if (m_iMenuCockpitSteeringWheel > 0)
			{
				sOptionValue = (m_iMenuCockpitSteeringWheel == 1 ? L"true" : L"false");
				bModifiedIniFile = ModifyCarModelIniFile(&carModelIniFile, L"i_steeringwheel", L"Switch", sOptionValue, false) || bModifiedIniFile;
			}
			else
			{
				bModifiedIniFile = ModifyCarModelIniFile(&carModelIniFile, L"i_steeringwheel", L"Switch", L"", true) || bModifiedIniFile;
			}

			if (m_iMenuCockpitWipers > 0)
			{
				sOptionValue = (m_iMenuCockpitWipers == 1 ? L"true" : L"false");
				bModifiedIniFile = ModifyCarModelIniFile(&carModelIniFile, L"i_wiper_r", L"Switch", sOptionValue, false) || bModifiedIniFile;
				bModifiedIniFile = ModifyCarModelIniFile(&carModelIniFile, L"i_wiper_l", L"Switch", sOptionValue, false) || bModifiedIniFile;
			}
			else
			{
				bModifiedIniFile = ModifyCarModelIniFile(&carModelIniFile, L"i_wiper_r", L"Switch", L"", true) || bModifiedIniFile;
				bModifiedIniFile = ModifyCarModelIniFile(&carModelIniFile, L"i_wiper_l", L"Switch", L"", true) || bModifiedIniFile;
			}

			if (m_iMenuCockpitWindscreen > 0)
			{
				sOptionValue = (m_iMenuCockpitWindscreen == 1 ? L"true" : L"false");
				bModifiedIniFile = ModifyCarModelIniFile(&carModelIniFile, L"i_window_f", L"Switch", sOptionValue, false) || bModifiedIniFile;
				bModifiedIniFile = ModifyCarModelIniFile(&carModelIniFile, L"cam_internal", L"showExterior", L"", false) || bModifiedIniFile;

				if (m_iMenuCockpitCameraShaking == 1)
					bModifiedIniFile = ModifyCarModelIniFile(&carModelIniFile, L"Cam_bonnet", L"showExterior", L"", false) || bModifiedIniFile;
			}
			else
			{
				bModifiedIniFile = ModifyCarModelIniFile(&carModelIniFile, L"i_window_f", L"Switch", L"", true) || bModifiedIniFile;
				bModifiedIniFile = ModifyCarModelIniFile(&carModelIniFile, L"cam_internal", L"showExterior", L"", true) || bModifiedIniFile;

				if (m_iMenuCockpitCameraShaking == 1)
					bModifiedIniFile = ModifyCarModelIniFile(&carModelIniFile, L"Cam_bonnet", L"showExterior", L"", true) || bModifiedIniFile;
			}

			if (bModifiedIniFile)
				carModelIniFile.SaveFile(sCarIniFileNameFullPath.c_str());
		}
	}
	catch (...)
	{
		LogPrint("ERROR. Failed to set steeringWheel/wipers/windscreen/FOV attributes for a car in slot %d", carSlotID);
		return FALSE;
	}

	return TRUE;
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
					outputFileName = ReplacePathVariables(m_screenshotPath, RBRAPI_MapCarIDToMenuIdx(currentCarID), false);

					if (!fs::exists(outputFileName)) 
						break;

					if (GetRBRInstallType() == "RSF" && m_screenshotCroppingRectRSF.bottom != -1)
					{
						outputFileName = ReplacePathVariables(m_screenshotPathRSF, RBRAPI_MapCarIDToMenuIdx(currentCarID), false);
						if (!fs::exists(outputFileName))
							break;
					}

					// Car img already exists, do not overwrite it (skip the img generation for this car)
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


#if 0
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
		// If rbrAppPath\Replays folder is missing then create it now
		if (!fs::exists(g_pRBRPlugin->m_sRBRRootDirW + L"\\Replays\\"))
			fs::create_directory(g_pRBRPlugin->m_sRBRRootDirW + L"\\Replays\\");

		std::wstring screenshotReplayFileNameWithoutExt = fs::path(g_pRBRPlugin->m_screenshotReplayFileName).replace_extension();

		// Lookup the replay template used to generate a car preview image. The replay file is chosen based on carModel name, category name or finally the "global" default file if more specific version was not found
		// (1) rbr\Plugins\NGPCarMenu\Replays\NGPCarName_<CarModelName>.rpl (fex "NGPCarMenu_Citroen C3 WRC 2017.rpl")
		// (2) rbr\Plugins\NGPCarMenu\Replays\NGPCarName_<FIACategoryName>.rpl (fex "NGPCarMenu_Group R1.rpl")
		// (3) rbr\Plugins\NGPCarMenu\Replays\NGPCarName.rpl (this file should always exists as a fallback rpl template)

		LogPrint(L"Preparing a car preview image creation for a car %s (%s) (car#=%d menu#=%d)", carModelName.c_str(), carCategoryName.c_str(), carID, carMenuIdx);

		inputReplayFileName = inputReplayFilePath + L"\\" + screenshotReplayFileNameWithoutExt + L"_" + carModelName + L".rpl";
		LogPrint(L"  Checking existence of template file %s", inputReplayFileName.c_str());
		if (!fs::exists(inputReplayFileName))
		{
			inputReplayFileName = inputReplayFilePath + L"\\" + screenshotReplayFileNameWithoutExt + L"_" + carCategoryName + L".rpl";
			LogPrint(L"  Checking existence of template file %s", inputReplayFileName.c_str());
			if (!fs::exists(inputReplayFileName))
			{
				// No carModel or carCategory specific replay template file (fex "NGPCarMenu_Group R1.rpl" file is used with GroupR1 cars). Use the default generic replay file (NGPCarMenu.rpl by default, but INI file can re-define this filename).
				inputReplayFileName = inputReplayFilePath + L"\\" + g_pRBRPlugin->m_screenshotReplayFileName;
				LogPrint(L"  Checking existence of template file %s", inputReplayFileName.c_str());
				if (!fs::exists(inputReplayFileName))
					inputReplayFileName = g_pRBRPlugin->m_sRBRRootDirW + L"\\Replays\\" + g_pRBRPlugin->m_screenshotReplayFileName;
			}
		}

		LogPrint(L"  Using template file %s to generate the car preview image", inputReplayFileName.c_str());

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
#endif

//-----------------------------------------------------------------------------------------------
// Read start/spli1/split2/finish distances from the currently loaded map
//
BOOL CNGPCarMenu::ReadStartSplitFinishPacenoteDistances(double* pStartDistance, double* pSplit1Distance, double* pSplit2Distance, double* pFinishDistance)
{
	BOOL bResult = FALSE;

	*pStartDistance  = -1;
	*pSplit1Distance = -1;
	*pSplit2Distance = -1;
	*pFinishDistance = -1;

	if (g_pRBRGameMode->gameMode == 0x01 || g_pRBRGameMode->gameMode == 0x0A || g_pRBRGameMode->gameMode == 0x0D || g_pRBRGameMode->gameMode == 0x02)
	{		
		if (g_pRBRPacenotes != nullptr && g_pRBRPacenotes->pPacenotes != nullptr)
		{
			for (int idx = 0; idx < g_pRBRPacenotes->numPacenotes; idx++)
			{
				if (g_pRBRPacenotes->pPacenotes[idx].type == 21) 
					*pStartDistance = static_cast<double>(g_pRBRPacenotes->pPacenotes[idx].distance);
				else if (g_pRBRPacenotes->pPacenotes[idx].type == 22) 
					*pFinishDistance = static_cast<double>(g_pRBRPacenotes->pPacenotes[idx].distance);
				else if (g_pRBRPacenotes->pPacenotes[idx].type == 23)
				{
					if(*pSplit1Distance < 0)
						*pSplit1Distance = static_cast<double>(g_pRBRPacenotes->pPacenotes[idx].distance);
					else if (*pSplit2Distance < 0)
					{
						*pSplit2Distance = static_cast<double>(g_pRBRPacenotes->pPacenotes[idx].distance);

						if(*pSplit1Distance > *pSplit2Distance)
						{
							// Splits in wrong order. Swap the order
							double temp = *pSplit1Distance;
							*pSplit1Distance = *pSplit2Distance;
							*pSplit2Distance = temp;
						}
					}
				}

				if (*pStartDistance >= 0 && *pSplit1Distance >= 0 && *pSplit2Distance >= 0 && *pFinishDistance >= 0)
					break;
			}

			// Some maps don't have the start note. Weird. Set it as zero
			if (*pStartDistance < 0) *pStartDistance = 0.0;

			bResult = (*pStartDistance >= 0 && *pFinishDistance >= 0);
		}
	}

	return bResult;
}


//-----------------------------------------------------------------------------------------------
// Read start/spli1/split2/finish distances from a pacenote DLS map file
//
BOOL CNGPCarMenu::ReadStartSplitsFinishPacenoteDistances(const std::wstring& sPacenoteFileName, float* startDistance, float* split1Distance, float* split2Distance, float* finishDistance)
{
	__int32 numOfPacenoteRecords = 0;

	*startDistance = -1;
	*split1Distance = -1;
	*split2Distance = -1;
	*finishDistance = -1;

	try
	{
		__int32 numOfPacenotesOffset;
		__int32 offsetFingerPrint[3];
		__int32 fingerPrintType;

		if (!fs::exists(sPacenoteFileName)) return FALSE;

		// Read pacenote data from the DLS file into vector buffer
		std::ifstream srcFile(sPacenoteFileName, std::ifstream::binary | std::ios::in);
		if (!srcFile) return FALSE;

		//srcFile.seekg(0x5C);
		//srcFile.read((char*)&numOfPacenoteRecords, sizeof(__int32));
		//if (srcFile.fail()) numOfPacenoteRecords = 0;

		// Find the "Num of pacenote records" offset. 
		// FIXME: Usually this is at 0x5C offset, but not always. Don't know yet the exact logic, so this code tries to identify certain "fingerprints" (not foolproof and definetly not the correct way to do it)
		//  If @0x38 == 01 then numOfPacenote offset is always 0x5C
		//  otherwise try to find nonZeroValue-0x00-0x1C fingerprint record (where the nonZeroValue is the num of pacenotes)
		//

		srcFile.seekg(0x38);
		srcFile.read((char*)&offsetFingerPrint, sizeof(__int32));
		fingerPrintType = (offsetFingerPrint[0] == 0x01 ? 0 : 1);

		numOfPacenotesOffset = 0x5C;
		while (numOfPacenotesOffset < 0x200)
		{
			srcFile.seekg(numOfPacenotesOffset);
			srcFile.read((char*)&offsetFingerPrint, sizeof(offsetFingerPrint));
			if (srcFile.fail())
			{
				numOfPacenoteRecords = 0;
				break;
			}

			if (fingerPrintType == 0 || (offsetFingerPrint[0] != 0x00 && offsetFingerPrint[1] == 0x00 && offsetFingerPrint[2] == 0x1C))
			{
				numOfPacenoteRecords = offsetFingerPrint[0];
				break;
			}

			numOfPacenotesOffset += sizeof(__int32);
		}

		//DebugPrint("RBRTM_ReadStartSplitsFinishPacenoteDistances. NumOfPacenoteRecords=%d", numOfPacenoteRecords);
		if (numOfPacenoteRecords <= 0 || numOfPacenoteRecords >= 50000)
		{
			LogPrint("ERROR CNGPCarMenu::ReadStartSplitsFinishPacenoteDistances. Invalid number of records %d", numOfPacenoteRecords);
			numOfPacenoteRecords = 0;
		}
		else
		{
			// Offset to pacenote records (always +0x20 to the offset of num of pacenote)
			__int32 dataOffset = numOfPacenotesOffset + 0x20;
			//srcFile.seekg(0x7C);
			srcFile.seekg(dataOffset);
			srcFile.read((char*)&dataOffset, sizeof(__int32));
			if (srcFile.fail() || dataOffset <= 0)
			{
				LogPrint("ERROR CNGPCarMenu::ReadStartSplitsFinishPacenoteDistances. Invalid pacenote data offset %d", dataOffset);
				numOfPacenoteRecords = 0;
			}

			if (numOfPacenoteRecords > 0)
			{
				std::vector<RBRPacenote> vectPacenoteData(numOfPacenoteRecords);

				srcFile.seekg(dataOffset);
				srcFile.read(reinterpret_cast<char*>(&vectPacenoteData[0]), (static_cast<std::streamsize>(numOfPacenoteRecords) * sizeof(RBRPacenote)));
				if (srcFile.fail())
				{
					LogPrint("ERROR CNGPCarMenu::ReadStartSplitsFinishPacenoteDistances. Failed to read %d pacenote data records", numOfPacenoteRecords);
					numOfPacenoteRecords = 0;
				}

				// Read "type and distance" values of pacenote records
				for (int idx = 0; idx < numOfPacenoteRecords; idx++)
				{
					if (vectPacenoteData[idx].type == 21 && *startDistance < 0)
					{
						*startDistance = vectPacenoteData[idx].distance;
						if (*startDistance >= 0 && *split1Distance >= 0 && *split2Distance >= 0 && *finishDistance >= 0) break;
					}
					else if (vectPacenoteData[idx].type == 23)
					{
						if (*split1Distance < 0)
							*split1Distance = vectPacenoteData[idx].distance;
						else
						{
							// Sometimes DLS file may have splits in "wrong order", so make sure the distance of split1 < split2
							if (vectPacenoteData[idx].distance > *split1Distance)
								*split2Distance = vectPacenoteData[idx].distance;
							else
							{
								*split2Distance = *split1Distance;
								*split1Distance = vectPacenoteData[idx].distance;
							}
						}
						if (*startDistance >= 0 && *split1Distance >= 0 && *split2Distance >= 0 && *finishDistance >= 0) break;
					}
					else if (vectPacenoteData[idx].type == 22 && *finishDistance < 0)
					{
						*finishDistance = vectPacenoteData[idx].distance;
						if (*startDistance >= 0 && *split1Distance >= 0 && *split2Distance >= 0 && *finishDistance >= 0) break;
					}
				}
			}
		}
		srcFile.close();

		if (*startDistance < 0) *startDistance = 0.0f;
	}
	catch (...)
	{
		*startDistance = -1;
		LogPrint(L"ERROR CNGPCarMenu::ReadStartSplitsFinishPacenoteDistances. Failed to read pacenote data from %s", sPacenoteFileName.c_str());
	}

	//DebugPrint("ReadStartSplitsFinishPacenoteDistances. Distance start=%f s1=%f s2=%f finish=%f", *startDistance, *split1Distance, *split2Distance, *finishDistance);

	return (*startDistance >= 0);
}


//----------------------------------------------------------------------------------------------------
// Read driveline data from TRK file or from BTB driveline.ini/pacenote.ini files
//
// Classic maps track-xx.trk:
//    Offset 0x10 = Num of driveline records (DWORD)
//        0x14 = Driveline record 8 x DWORD (x y z cx cy cz distance zero)
//        ...N num of driveline records...
//
// BTB maps driveline.ini:
//    Driveline.ini for a track layout and Pacenotes.ini file for split positions (expected to be in the same folder with driveline.ini file)
//
int CNGPCarMenu::ReadDriveline(const std::string& sDrivelineFileName, CDrivelineSource& drivelineSource)
{
	if (_iEqual(fs::path(sDrivelineFileName).filename().string(), "driveline.ini", true))
		return RBRRX_ReadDriveline(sDrivelineFileName, drivelineSource);
	else
		return ReadDriveline(_ToWString(sDrivelineFileName), drivelineSource);
}

// Read driveline from classic map (TRK file)
int CNGPCarMenu::ReadDriveline(const std::wstring& sDrivelineFileName, CDrivelineSource& drivelineSource)
{
	__int32 numOfDrivelineRecords = 0;

	try
	{
		drivelineSource.vectDrivelinePoint.clear();

		drivelineSource.pointMin.x = drivelineSource.pointMin.y = 9999999.0f;
		drivelineSource.pointMax.x = drivelineSource.pointMax.y = -9999999.0f;

		// Read driveline data from the TRK file into vector buffer
		std::ifstream srcFile(sDrivelineFileName, std::ifstream::binary | std::ios::in);
		if (!srcFile) return 0;

		srcFile.seekg(0x10);
		srcFile.read((char*)&numOfDrivelineRecords, sizeof(__int32));
		if (srcFile.fail()) numOfDrivelineRecords = 0;

		//DebugPrint("RBRTM_ReadDriveline. NumOfDrivelineRecords=%d", numOfDrivelineRecords);
		if (numOfDrivelineRecords <= 0 || numOfDrivelineRecords >= 100000)
		{
			LogPrint("ERROR CNGPCarMenu::ReadDriveline. Invalid number of records %d", numOfDrivelineRecords);
			numOfDrivelineRecords = 0;
		}
		else
		{
			std::vector<float> vectDrivelineData(numOfDrivelineRecords * 8);
			srcFile.read(reinterpret_cast<char*>(&vectDrivelineData[0]), (static_cast<std::streamsize>(numOfDrivelineRecords) * 8 * sizeof(float)));
			if (srcFile.fail())
			{
				LogPrint("ERROR CNGPCarMenu::ReadDriveline. Failed to read data");
				numOfDrivelineRecords = 0;
			}

			// Read "x y z cx cy cz distance zero" record (8 x floats)
			for (int idx = 0; idx < numOfDrivelineRecords; idx++)
			{
				float x, y, distance;
				x = vectDrivelineData[idx * 8 + 0];
				y = vectDrivelineData[idx * 8 + 1];
				distance = vectDrivelineData[idx * 8 + 6];

				//DebugPrint("%d: %f %f %f", idx, x, y, distance);

				if (distance < drivelineSource.startDistance)
					continue;

				if (drivelineSource.finishDistance > 0 && distance > drivelineSource.finishDistance)
					break;

				if (x < drivelineSource.pointMin.x) drivelineSource.pointMin.x = x;
				if (x > drivelineSource.pointMax.x) drivelineSource.pointMax.x = x;
				if (y < drivelineSource.pointMin.y) drivelineSource.pointMin.y = y;
				if (y > drivelineSource.pointMax.y) drivelineSource.pointMax.y = y;

				drivelineSource.vectDrivelinePoint.push_back({ {x, y}, distance });
			}

			// Sort driveline data in ascending order by distance (hmm... the DLS data seems to be already in ascending order)
			//drivelineSource.vectDrivelinePoint.sort();
		}
		srcFile.close();
	}
	catch (...)
	{
		drivelineSource.vectDrivelinePoint.clear();
		LogPrint(L"ERROR CNGPCarMenu::ReadDriveline. Failed to read driveline data from %s", sDrivelineFileName.c_str());
	}

	return drivelineSource.vectDrivelinePoint.size();
}


//-----------------------------------------------------------------------------------------------
// Replace all %variableName% RBR path variables with the actual value
//
std::wstring CNGPCarMenu::ReplacePathVariables(const std::wstring& sPath, int selectedCarIdx, bool rbrtmplugin, int mapID, const WCHAR* mapName, const std::string& folderName)
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
/*
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
*/

		// Remove leading path from "Cars\Yaris_WRC18" value and leave only "Yaris_WRC18" model folder name
		std::filesystem::path sCarFolderPath(g_RBRCarSelectionMenuEntry[selectedCarIdx].wszCarModelFolder);
		sResult = _ReplaceStr(sResult, L"%carfolder%", sCarFolderPath.filename());

		// If this is RSF installation then replace carRSFID by the rsfdata\cache\cars.json "ID" value for the carModel name
		if (_iFind(sPath, L"%carrsfid%", true) != std::wstring::npos)
		{
			std::wstring carRSFID = L"00";

			// RSF rbr installation
			try
			{
				if (GetRBRInstallType() == "RSF")
				{
					std::ifstream jsonFile(m_sRBRRootDir + "\\rsfdata\\cache\\cars.json");
					rapidjson::Document carsJsonData;
					rapidjson::IStreamWrapper isw(jsonFile);
					carsJsonData.ParseStream(isw);

					if (!carsJsonData.HasParseError() && carsJsonData.IsArray())
					{
						for (rapidjson::Value::ConstValueIterator itr = carsJsonData.Begin(); itr != carsJsonData.End(); ++itr)
						{
							if ((*itr).HasMember("name") && _iEqual(_ToWString((*itr)["name"].GetString()), g_RBRCarSelectionMenuEntry[selectedCarIdx].wszCarModel, false))
							{
								if ((*itr).HasMember("id")) carRSFID = _ToWString((*itr)["id"].GetString());
								break;
							}
						}
					}
				}
			}
			catch (...)
			{
				// Eat all exceptions
				LogPrint("WARNING. Failed to read carRSFID value for %s from rsfdata\\cache\\cars.json", g_RBRCarSelectionMenuEntry[selectedCarIdx].szCarMenuName);
			}

			sResult = _ReplaceStr(sResult, L"%carrsfid%", carRSFID);
		}
	}

	std::wstring imgExtension;
	imgExtension = _ToWString(g_NGPCarMenu_ImageOptions[this->m_iMenuImageOption]);
	sResult = _ReplaceStr(sResult, L"%filetype%", imgExtension);

	sResult = _ReplaceStr(sResult, L"%plugin%", (rbrtmplugin ? L"TM" : (m_bRallySimFansPluginInstalled ? L"RSF" : L"RBR")));

	if (mapID > 0) sResult = _ReplaceStr(sResult, L"%mapid%", std::to_wstring(mapID));
	if (mapName != nullptr) sResult = _ReplaceStr(sResult, L"%mapname%", mapName);	

	if (!folderName.empty())
	{
		if(_iStarts_With(folderName, "tracks\\", true))
			sResult = _ReplaceStr(sResult, L"%mapfolder%", _ToWString(folderName.substr(7)));
		else
			sResult = _ReplaceStr(sResult, L"%mapfolder%", _ToWString(folderName));
	}

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


//-----------------------------------------------------------------------------------------------
// Initialize custom track load image
//
BOOL CNGPCarMenu::SetupCustomTrackLoadImage(std::wstring& sFileName, const POINT* pImagePos, const SIZE* pImageSize, DWORD dwImageFlags)
{
	float x, y, cx, cy;
	if (pImagePos == nullptr)
	{
		x = y = 0.0f;
	}
	else
	{
		x = static_cast<float>(pImagePos->x);
		y = static_cast<float>(pImagePos->y);
	}

	if (pImageSize == nullptr)
	{
		cx = static_cast<float>(g_rectRBRWndClient.right);
		RBRAPI_MapRBRPointToScreenPoint(0, 390.0f, nullptr, &cy);
	}
	else
	{
		cx = static_cast<float>(pImageSize->cx);
		cy = static_cast<float>(pImageSize->cy);
	}

	DebugPrint(L"SetupCustomTrackLoadImage. File=%s (%f, %f)(%f, %f)", sFileName.c_str(), x, y, cx, cy);

	try
	{
		std::vector<std::wstring> fileNameList;
		if (_FindMatchingFileNames(sFileName, fileNameList, true, 1) > 0)
		{
			if(!SUCCEEDED(D3D9CreateRectangleVertexTexBufferFromFile(g_pRBRIDirect3DDevice9, fileNameList[0], x, y, cx, cy, &m_customTrackLoadImg, dwImageFlags)))
				SAFE_RELEASE(m_customTrackLoadImg.pTexture);
		}
		else
			SAFE_RELEASE(m_customTrackLoadImg.pTexture);
	}
	catch (...)
	{
		// Eat all exceptions
		SAFE_RELEASE(m_customTrackLoadImg.pTexture);
	}

	return m_customTrackLoadImg.pTexture != nullptr;
}


//------------------------------------------------------------------------------------------------
// Rescales the driveline coordinate data of a map to fit an output rectangle area
//
int CNGPCarMenu::RescaleDrivelineToFitOutputRect(CDrivelineSource& drivelineSource, CMinimapData& minimapData)
{
	DebugPrint("RescaleDrivelineToFitOutputRect. CountT1=%d", drivelineSource.vectDrivelinePoint.size());

	BOOL bFlipMinimap = FALSE;		// If TRUE then flips X and Y axis because the minimap graph takes more vertical than horizontal space (draw area has more room vertically)

	POINT_float sourceSize = { 0,0 };
	float sourceAspectRatio;

	POINT_float  outputSize = { 0,0 };
	float outputAspectRatio;

	POINT_float outputRange;

	std::vector<POINT_DRIVELINE_int> vectTempMinimapPoint;

	if(drivelineSource.vectDrivelinePoint.size() > 0)
		vectTempMinimapPoint.reserve(drivelineSource.vectDrivelinePoint.size());

	minimapData.vectMinimapPoint.clear();
	minimapData.minimapSize.x = minimapData.minimapSize.y = INT_MIN;

	// Keep the aspect ratio correct even after rescaling
	sourceSize.x = drivelineSource.pointMax.x - drivelineSource.pointMin.x;
	sourceSize.y = drivelineSource.pointMax.y - drivelineSource.pointMin.y;

	outputSize.x = static_cast<float>(minimapData.minimapRect.right - minimapData.minimapRect.left);
	outputSize.y = static_cast<float>(minimapData.minimapRect.bottom - minimapData.minimapRect.top);

	if ((outputSize.x > outputSize.y) && (sourceSize.y > sourceSize.x) || (outputSize.x < outputSize.y) && (sourceSize.y < sourceSize.x))
	{
		// The width is more then height, so flip the output graph 90-degree because the output space has also more space vertically (the minimap scales better)
		float tmpX;
		tmpX = sourceSize.x;
		sourceSize.x = sourceSize.y;
		sourceSize.y = tmpX;

		tmpX = drivelineSource.pointMax.x;
		drivelineSource.pointMax.x = drivelineSource.pointMax.y;
		drivelineSource.pointMax.y = tmpX;

		tmpX = drivelineSource.pointMin.x;
		drivelineSource.pointMin.x = drivelineSource.pointMin.y;
		drivelineSource.pointMin.y = tmpX;

		bFlipMinimap = TRUE;
	}

	if (sourceSize.y != 0) sourceAspectRatio = sourceSize.x / sourceSize.y;
	else sourceAspectRatio = 1.0f;

	if (outputSize.y != 0) outputAspectRatio = outputSize.x / outputSize.y;
	else outputAspectRatio = 1.0f;

	outputRange = outputSize;

	if (sourceAspectRatio > outputAspectRatio)
		outputRange.y = outputSize.x / sourceAspectRatio;
	else
		outputRange.x = outputSize.y * sourceAspectRatio;

	//
	// Re-scale coordinates to fit the minimap into the output rect and return the scaled minimap coordinate vector and the total width/height of the minimap graph
	//
	int newX, newY, splitType;
	POINT_DRIVELINE_int prevPoint;

	prevPoint.drivelineCoord.x = prevPoint.drivelineCoord.y = prevPoint.splitType = INT_MIN;
	for (auto& item : drivelineSource.vectDrivelinePoint)
	{				
		//item.y = /*outputRange.y - */C_RANGE_REMAP(item.y, pointMin.y, pointMax.y, 0, outputRange.y);
		if (bFlipMinimap)
		{
			newX = static_cast<int>(outputRange.x - C_RANGE_REMAP(item.drivelineCoord.y, drivelineSource.pointMin.x, drivelineSource.pointMax.x, 0, outputRange.x));
			newY = static_cast<int>(outputRange.y - C_RANGE_REMAP(item.drivelineCoord.x, drivelineSource.pointMin.y, drivelineSource.pointMax.y, 0, outputRange.y));
		}
		else
		{
			newX = static_cast<int>(C_RANGE_REMAP(item.drivelineCoord.x, drivelineSource.pointMin.x, drivelineSource.pointMax.x, 0, outputRange.x));
			newY = static_cast<int>(outputRange.y - C_RANGE_REMAP(item.drivelineCoord.y, drivelineSource.pointMin.y, drivelineSource.pointMax.y, 0, outputRange.y));
		}

		// Set the split type of the coordinate (0=start->split1, 1=split1->split2, 2=split2->finish)
		if (item.drivelineDistance < drivelineSource.split1Distance)
			splitType = 0;
		else if (item.drivelineDistance < drivelineSource.split2Distance && drivelineSource.split2Distance > 0)
			splitType = 1;
		else if (item.drivelineDistance < drivelineSource.finishDistance)
			splitType = (drivelineSource.split2Distance > 0 ? 2 : 1);
		else
			splitType = 0;

		// Don't add duplicated consequtive points
		if (prevPoint.drivelineCoord.x != newX || prevPoint.drivelineCoord.y != newY || prevPoint.splitType != splitType)
		{
			prevPoint.drivelineCoord.x = newX;
			prevPoint.drivelineCoord.y = newY;
			prevPoint.splitType = splitType;

			//minimapData.vectMinimapPoint.push_back({ {newX, newY}, splitType});
			vectTempMinimapPoint.push_back({ {newX, newY}, splitType });

			if (newX > minimapData.minimapSize.x) minimapData.minimapSize.x = newX;
			if (newY > minimapData.minimapSize.y) minimapData.minimapSize.y = newY;
		}
	}

/*
	// Remove duplicated consequtive coords (usually as a result of downscaling some source points end up into the same output point)
	if (minimapData.vectMinimapPoint.size() >= 2)
	{
		// for (auto it = vectDrivelinePoint.begin()+1; it != minimapData.vectMinimapPoint.end(); )
		for (auto it = std::next(minimapData.vectMinimapPoint.begin(),1); it != minimapData.vectMinimapPoint.end(); )
		{
			// if (*it == *((it - 1)))
			if (*it == *((std::prev(it, 1))))
				it = minimapData.vectMinimapPoint.erase(it);
			else 
				it++;
		}
	}
*/

	//DebugPrint("RescaleDrivelineToFitOutputRect. CountT2=%d", vectTempMinimapPoint.size());

/*
#if USE_DEBUG == 1
	std::ofstream outputFile;
	outputFile.open("c:\\temp\\minimap.txt");
	outputFile << "DrivelinePoints=" << drivelineSource.vectDrivelinePoint.size() << std::endl;
	outputFile << "MinimapPointsT1=" << vectTempMinimapPoint.size() << std::endl;
	for (auto& item : vectTempMinimapPoint)
	{
		outputFile << item.drivelineCoord.x << "," << item.drivelineCoord.y << std::endl;
	}
	outputFile << "-------------" << std::endl;
#endif
*/

	// Minimap lines are drawn by the lazy way using inter-connected small circles. Interpolate missing coordinates to create a complete line representing the road line
	// TODO. Implement (or find a lightweight open-source version) a real trianglefan/polyline vertex generator routine with smooth edges (directx9 compatible). 
	//       But, for now this code uses the lazy way because the minimap doesn't need "perfect" smooth and curved lines.
	if (vectTempMinimapPoint.size() >= 2)
	{
		POINT_DRIVELINE_int startPoint;

		startPoint.drivelineCoord = vectTempMinimapPoint[0].drivelineCoord;
		startPoint.splitType = vectTempMinimapPoint[0].splitType;
		for (auto& item : vectTempMinimapPoint)
		{
			// Interpolate points between the prev point and the new point
			float lenX = static_cast<float>((item.drivelineCoord.x - startPoint.drivelineCoord.x));
			float lenY = static_cast<float>((item.drivelineCoord.y - startPoint.drivelineCoord.y));
			float len = ((lenX * lenX) + (lenY * lenY)) / 2.0f;
			if (len != 0)
			{
				float dt = 4 / len;
				float t;

				t = dt;
				while (t < 1.0)
				{
					newX = static_cast<int>((1.0f - t) * startPoint.drivelineCoord.x + t * item.drivelineCoord.x);
					newY = static_cast<int>((1.0f - t) * startPoint.drivelineCoord.y + t * item.drivelineCoord.y);

					if (newX == item.drivelineCoord.x && newY == item.drivelineCoord.y)
						break;

					if (prevPoint.drivelineCoord.x != newX || prevPoint.drivelineCoord.y != newY)
					{
						// Add interpolated point between start-end points
						prevPoint.drivelineCoord.x = newX;
						prevPoint.drivelineCoord.y = newY;
						minimapData.vectMinimapPoint.push_back({ {newX, newY}, startPoint.splitType });
					}
					t += dt;
				}
			}

			startPoint.drivelineCoord = item.drivelineCoord;
			startPoint.splitType = item.splitType;

			// Add the new target point
			minimapData.vectMinimapPoint.push_back({ {item.drivelineCoord.x, item.drivelineCoord.y}, item.splitType });
		}
	}

	DebugPrint("RescaleDrivelineToFitOutputRect. CountT3=%d", minimapData.vectMinimapPoint.size());

	return minimapData.vectMinimapPoint.size();
}


//------------------------------------------------------------------------------------------------
// Draw the recent results table
//
void CNGPCarMenu::DrawRecentResultsTable(int posX, int posY, std::vector<RaceStatDBStageResult>& latestStageResults, bool drawStageRecordTitleRow)
{
	int iMapInfoPrintRow = 0;
	int iFontHeight = g_pFontCarSpecCustom->GetTextHeight();

	if (latestStageResults.size() <= 0)
		return;

	size_t iMapNameLen, iCarNameLen;
	std::string mapName;
	std::string carName;
	std::string carGroup;

	std::wstringstream sStrStream;
	sStrStream << std::fixed << std::setprecision(1);

	if (drawStageRecordTitleRow)
	{
		g_pFontCarSpecCustom->DrawText(posX, posY + (iMapInfoPrintRow++ * iFontHeight), C_CARSPECTEXT_COLOR,
			(GetLangWString(L"SS record", true) + 
				_ToWString(GetSecondsAsMISSMS(latestStageResults[0].stageRecord, 0))
				+ L" (" 
				+ _ToWString(GetSecondsAsKMh(latestStageResults[0].stageRecord, latestStageResults[0].stageLength, true, 1))
				+ L")").c_str()
		);

		iMapInfoPrintRow += 1;
	}

	SIZE textSize{ 8, 21 };

	g_pFontCarSpecCustom->DrawText(posX, posY + (iMapInfoPrintRow++ * iFontHeight), C_CARMODELTITLETEXT_COLOR,
		(GetLangWString(L"Recent results") + L" / " + GetLangWString(L"Diff record by car (group)")).c_str(), 0);

	for (auto& item : latestStageResults)
	{
		mapName = item.mapName;
		carName = item.carModel;

		// Clean up map and car names (remove extra grp tags because the FIACategory is shown as a separate value)
		if (_iEnds_With(carName, " grpa", true) || _iEnds_With(carName, " grpb", true) || _iEnds_With(carName, " grpn", true))
			carName = carName.substr(0, carName.length() - 5);
		else if (_iEnds_With(carName, " wrc", true) || _iEnds_With(carName, " rgt", true))
			carName = carName.substr(0, carName.length() - 4);
		else if (_iEnds_With(carName, " r1", true) || _iEnds_With(carName, " r2", true) || _iEnds_With(carName, " r3", true) || _iEnds_With(carName, " r4", true) || _iEnds_With(carName, " r5", true))
			carName = carName.substr(0, carName.length() - 3);

		iMapNameLen = mapName.length();
		iCarNameLen = carName.length();

		if (iMapNameLen + iCarNameLen > 45)
		{
			mapName = mapName.substr(0, 20);
			iMapNameLen = mapName.length();
		}
		if (iMapNameLen + iCarNameLen > 45)
			carName = carName.substr(0, 45 - iMapNameLen);

		// Cleanup category value (Group R5 -> R5, Super 1600 -> S1600)
		if(_iStarts_With(item.carFIACategory, "group ", true))
			carGroup = item.carFIACategory.substr(6);
		else if(_iStarts_With(item.carFIACategory, "super ", true))
			carGroup = "S" + item.carFIACategory.substr(6);
		else
			carGroup = item.carFIACategory;

		sStrStream.clear();
		sStrStream.str(std::wstring());
		sStrStream << _ToWString(mapName) << (!mapName.empty() ? L": " : L"") << _ToWString(carName) << L" (" << _ToWString(carGroup) << L")";
		g_pFontCarSpecCustom->DrawText(posX, posY + (iMapInfoPrintRow++ * iFontHeight), C_CARSPECTEXT_COLOR, sStrStream.str().c_str(), 0);

		sStrStream.clear();
		sStrStream.str(std::wstring());
		sStrStream << _ToWString(GetSecondsAsMISSMS(item.finishTime));
		if(item.finishTime != 0.0f && item.stageLength != 0.0f) sStrStream << L" (" << _ToWString(GetSecondsAsKMh(item.finishTime, item.stageLength, true, 1)) << L")";
		g_pFontCarSpecCustom->DrawText(posX, posY + (iMapInfoPrintRow * iFontHeight), C_CARSPECTEXT_COLOR, sStrStream.str().c_str(), 0);

		if (item.totalPenaltyTime != 0)
		{
			g_pFontCarSpecCustom->GetTextExtent(sStrStream.str().c_str(), &textSize);

			sStrStream.clear();
			sStrStream.str(std::wstring());
			sStrStream << L" " << _ToWString(GetSecondsAsMISSMS(item.totalPenaltyTime, false, true));
			
			g_pFontCarSpecCustom->DrawText(posX + textSize.cx, posY + (iMapInfoPrintRow * iFontHeight), C_REDSEPARATORLINE_COLOR, sStrStream.str().c_str(), 0);
		}

		g_pFontCarSpecCustom->GetTextExtent(L"00:00,0 (000,0 km/h) +00:00,0   ", &textSize);

		sStrStream.clear();
		sStrStream.str(std::wstring());
		sStrStream << _ToWString(GetSecondsAsMISSMS(item.finishTime - item.carStageRecord, false, true));
		g_pFontCarSpecCustom->DrawText(posX + textSize.cx, posY + (iMapInfoPrintRow * iFontHeight), C_CARSPECTEXT_COLOR, sStrStream.str().c_str(), 0);

		int prevColumnX = textSize.cx;
		g_pFontCarSpecCustom->GetTextExtent(L"+00:00,0 ", &textSize);
		textSize.cx += prevColumnX;

		sStrStream.clear();
		sStrStream.str(std::wstring());
		sStrStream << L"(" << _ToWString(GetSecondsAsMISSMS(item.finishTime - item.fiaCatStageRecord, false, true)) << L")";
		g_pFontCarSpecCustom->DrawText(posX + textSize.cx, posY + (iMapInfoPrintRow++ * iFontHeight), C_CARSPECTEXT_COLOR, sStrStream.str().c_str(), 0);

		posY += g_pFontCarSpecCustom->GetTextHeight() / 2;
	}
}


//------------------------------------------------------------------------------------------------
//
BOOL CALLBACK CNGPCarMenu::MonitorEnumCallback(HMONITOR hMon, HDC hdc, LPRECT lprcMonitor, LPARAM pData)
{
	MONITORINFOEX monInfo;
	ZeroMemory(&monInfo, sizeof(MONITORINFOEX));
	monInfo.cbSize = sizeof(MONITORINFOEX);

	DebugPrint("MonitorEnumCallback. hMon=%x hdc=%x", hMon, hdc);

	// Ignore invalid and mirroring monitors
	if (pData == 0 || lprcMonitor == nullptr || hMon == nullptr || !GetMonitorInfo(hMon, &monInfo))
		return TRUE;

	if (monInfo.dwFlags & DISPLAY_DEVICE_MIRRORING_DRIVER)
		return TRUE; 

	// Add monitor coordinates to vector (the vector idx is the monitor#)
	std::vector<RECT>* pMonitorVector = (std::vector<RECT>*) pData;
	pMonitorVector->push_back( { *lprcMonitor } );
	LogPrint("Monitor %d. Rect=%d %d %d %d", pMonitorVector->size(), lprcMonitor->left, lprcMonitor->top, lprcMonitor->right, lprcMonitor->bottom);

	return TRUE;
}


//------------------------------------------------------------------------------------------------
//
const char* CNGPCarMenu::GetName(void)
{
	//DebugPrint("Enter CNGPCarMenu.GetName");

	// Re-route RBR DXEndScene function to our custom function
	if (g_bRBRHooksInitialized == FALSE && m_PluginState == T_PLUGINSTATE::PLUGINSTATE_UNINITIALIZED)
	{
		m_PluginState = T_PLUGINSTATE::PLUGINSTATE_INITIALIZING;

		DebugPrint("CNGPCarMenu.GetName. First time initialization of NGPCarMenu options and RBR interfaces");

/*
		try
		{
			std::string sReplayTemplateFileName;

			// Remove the temporary replay template file when the plugin is initialized (this way the temp RPL file won't be left around for too long)
			//sReplayTemplateFileName = g_pRBRPlugin->m_sRBRRootDir + "\\Replays\\" + C_REPLAYFILENAME_SCREENSHOT;
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
*/

		// Check the RBR DX9 fullscreen vs windows mode option
		CSimpleIni rbrINI;
		rbrINI.SetUnicode(true);
		rbrINI.LoadFile((m_sRBRRootDir + "\\RichardBurnsRally.ini").c_str());
		std::string sRBRFullscreenOption = rbrINI.GetValue("Settings", "Fullscreen", "false");
		_ToLowerCase(sRBRFullscreenOption);
		if (sRBRFullscreenOption == "true" || sRBRFullscreenOption == "1")
			m_bRBRFullscreenDX9 = TRUE;
		rbrINI.Reset();

		m_bPacenotePluginInstalled = fs::exists(m_sRBRRootDir + "\\Plugins\\PaceNote.dll");
		m_bRallySimFansPluginInstalled = fs::exists(m_sRBRRootDir + "\\Plugins\\RSFstub.dll");
		m_bRBRTMPluginInstalled = fs::exists(m_sRBRRootDir + "\\Plugins\\RBRTM.dll");

		GetFileVersionInformationAsNumber(m_sRBRRootDirW + L"\\Plugins\\PhysicsNG.dll", &m_iPhysicsNGMajorVer, &m_iPhysicsNGMinorVer, &m_iPhysicsNGPatchVer, &m_iPhysicsNGBuildVer); 

		m_bRBRProInstalled = fs::exists(m_sRBRRootDir + "\\..\\RBRProManager.exe") || fs::exists(m_sRBRRootDir + "\\..\\RBRPro.API.dll");

		LogPrint("RBR FullscreenMode=%d PacenotePluginInstalled=%d TMPluginInstalled=%d RSFPluginInstalled=%d RBRProManagerInstalled=%d", m_bRBRFullscreenDX9, m_bPacenotePluginInstalled, m_bRBRTMPluginInstalled, m_bRallySimFansPluginInstalled, m_bRBRProInstalled);
		LogPrint("NGP version %d.%d.%d.%d", m_iPhysicsNGMajorVer, m_iPhysicsNGMinorVer, m_iPhysicsNGPatchVer, m_iPhysicsNGBuildVer);

		if (m_bRBRProInstalled)
		{
			m_sRBRProVersion = ::GetFileVersionInformationAsString(m_sRBRRootDirW + L"\\..\\RBRProManager.exe");
			LogPrint("RBRPro version %s", m_sRBRProVersion.c_str());
		}

		if (m_bRallySimFansPluginInstalled)
		{
			m_sRSFVersion = ::GetFileVersionInformationAsString(m_sRBRRootDirW + L"\\rsfdata\\Rallysimfans.hu.dll");
			LogPrint("RSF version %s", m_sRSFVersion.c_str());
		}

		// Init RBR API objects
		RBRAPI_InitializeObjReferences();

		//RBRAPI_RefreshWndRect();
		m_pD3D9RenderStateCache = new CD3D9RenderStateCache(g_pRBRIDirect3DDevice9, false);				

		// Init font to draw custom car spec info text (3D model and custom livery text)
		int iFontSize = 14;
		if (g_rectRBRWndClient.bottom < 600) iFontSize = 7;
		else if (g_rectRBRWndClient.bottom < 768) iFontSize = 9;
		else if (g_rectRBRWndClient.bottom == 768) iFontSize = 12;

		// Font to draw car spec details in QuickRally, RBRTM and RBRRX car selection screens (FIA Category, HP, Transmission etc)
		g_pFontCarSpecCustom = new CD3DFont(L"Trebuchet MS", iFontSize, 0 /*D3DFONT_BOLD*/);
		g_pFontCarSpecCustom->InitDeviceObjects(g_pRBRIDirect3DDevice9);
		g_pFontCarSpecCustom->RestoreDeviceObjects();	

		// Font to draw car and track model author/credit detail text (a bit smaller font than CarSpecCustom font)
		g_pFontCarSpecModel = new CD3DFont(L"Trebuchet MS", (iFontSize >= 14 ? iFontSize - 2 : iFontSize), 0 /*D3DFONT_BOLD*/);
		g_pFontCarSpecModel->InitDeviceObjects(g_pRBRIDirect3DDevice9);
		g_pFontCarSpecModel->RestoreDeviceObjects();

		RefreshSettingsFromPluginINIFile(true);

		if (!fs::exists(m_sRBRRootDir + "\\physics\\") && fs::exists(m_sRBRRootDir + "\\physics.rbz"))
		{
			LogPrint("Physics folder missing, but Physics.rbz file exists. Creating the Physics folder to fix QuickRally and RBRRX racing (ie. racing outside the RSF plugin)");

			STARTUPINFO si;
			PROCESS_INFORMATION pi;
			ZeroMemory(&si, sizeof(STARTUPINFO));
			ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

			si.cb = sizeof(STARTUPINFO);
			if (::CreateProcessA(nullptr,
				(LPSTR)(std::string("\"") + m_sRBRRootDir + "\\7za.exe\"" +
				" x \"" + m_sRBRRootDir + "\\physics.rbz\"").c_str(), 
				nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, m_sRBRRootDir.c_str(), &si, &pi))
			{
				// Wait 7Zip extract process to complete its operation or wait max 5 minutes to avoid infinite lock
				WaitForSingleObject(pi.hProcess, 5*60*1000);
				CloseHandle(pi.hProcess);	
				CloseHandle(pi.hThread);

				LogPrint("Completed the creation of Physics folder");
			}
		}

		// If EASYRBRPath is not set then assume cars have been setup using RBRCIT car manager tool
		if (m_easyRBRFilePath.empty()) InitCarSpecData_RBRCIT();
		else InitCarSpecData_EASYRBR();
		CalculateMaxLenCarMenuName();

		DebugPrint("GetName. Reading FMOD audio settings");

		InitCarSpecAudio();		// FMOD bank names per car

#if USE_DEBUG == 1
		DebugPrint("GetName. Creating debug font");

		g_pFontDebug = new CD3DFont(L"Courier New", 11, 0);
		g_pFontDebug->InitDeviceObjects(g_pRBRIDirect3DDevice9);
		g_pFontDebug->RestoreDeviceObjects();
#endif 

		// Initialize custom car selection menu name and model name strings (change RBR string pointer to our custom string)
		int idx;
		for (idx = 0; idx < 8; idx++)
		{
			DebugPrint("GetName. Updating RBR car menu names. %d=%s", idx, g_RBRCarSelectionMenuEntry[idx].szCarMenuName);

			// Use custom car menu selection name and car description (taken from the current NGP config files)
			WriteOpCodePtr((LPVOID)g_RBRCarSelectionMenuEntry[idx].ptrCarMenuName, &g_RBRCarSelectionMenuEntry[idx].szCarMenuName[0]);
			WriteOpCodePtr((LPVOID)g_RBRCarSelectionMenuEntry[idx].ptrCarDescription, &g_RBRCarSelectionMenuEntry[idx].szCarMenuName[0]); // the default car description is CHAR and not WCHAR string
		}

		// If any of the car menu names is longer than 20 chars then move the whole menu list to left to make more room for longer text
		//if (m_iCarMenuNameLen > 20)
		{
			// Default car selection menu X-position 0x0920. Minimum X-pos is 0x0020
			//int menuXPos = max(0x920 - ((m_iCarMenuNameLen - 20 + 1) * 0x100), 0x0020);
			int menuXPos = 0x0020;

			for (idx = 0; idx < 8; idx++)
			{
				//DebugPrint("GetName. Tweaking RBR menu line %d position", idx);

				g_pRBRMenuSystem->menuObj[RBRMENUIDX_QUICKRALLY_CARS]->pItemPosition[g_pRBRMenuSystem->menuObj[RBRMENUIDX_QUICKRALLY_CARS]->firstSelectableItemIdx + idx].x = menuXPos;
				g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P1]->pItemPosition[g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P1]->firstSelectableItemIdx + idx].x = menuXPos;
				g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P2]->pItemPosition[g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P2]->firstSelectableItemIdx + idx].x = menuXPos;
				g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P3]->pItemPosition[g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P3]->firstSelectableItemIdx + idx].x = menuXPos;
				g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P4]->pItemPosition[g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P4]->firstSelectableItemIdx + idx].x = menuXPos;
				g_pRBRMenuSystem->menuObj[RBRMENUIDX_RBRCHALLENGE_CARS]->pItemPosition[g_pRBRMenuSystem->menuObj[RBRMENUIDX_RBRCHALLENGE_CARS]->firstSelectableItemIdx + idx].x = menuXPos;
			}
		}

		DebugPrint("GetName. Preparing RBR DX handlers");

		//
		// Ready to rock! Re-route and store the original function address for later use. At this point all variables used in custom Dx0 functions should be initialized already
		// ("TRUE" as ignore trampoline restoration process fixes the bug with GaugerPlugin where RBR generated a crash dump at app exit. Both plugins worked just fine, but RBR exit generated crash dump).
		//
		gtcDirect3DBeginScene = new DetourXS((LPVOID)0x0040E880, ::CustomRBRDirectXBeginScene, TRUE);
		Func_OrigRBRDirectXBeginScene = (tRBRDirectXBeginScene)gtcDirect3DBeginScene->GetTrampoline();

		gtcDirect3DEndScene = new DetourXS((LPVOID)0x0040E890, ::CustomRBRDirectXEndScene, TRUE);
		Func_OrigRBRDirectXEndScene = (tRBRDirectXEndScene)gtcDirect3DEndScene->GetTrampoline();	 

		gtcRBRReplay = new DetourXS((LPVOID)0x4999B0, ::CustomRBRReplay, TRUE);
		Func_OrigRBRReplay = (tRBRReplay)gtcRBRReplay->GetTrampoline();

		if (m_bGenerateReplayMetadataFile)
		{
			gtcRBRSaveReplay = new DetourXS((LPVOID)0x0049B030, ::CustomRBRSaveReplay, TRUE);
			Func_OrigRBRSaveReplay = (tRBRSaveReplay)gtcRBRSaveReplay->GetTrampoline();
		}

		if (g_iInvertedPedalsStartupFixFlag != 0 
			|| g_iXInputSplitThrottleBrakeAxis >= 0 
// FIXME. Deadzone feature disable. It is broken with inverted pedals
/*			|| g_fControllerAxisDeadzone[0] > 0.0f 
			|| g_fControllerAxisDeadzone[3] > 0.0f 
			|| g_fControllerAxisDeadzone[5] > 0.0f
			|| g_fControllerAxisDeadzone[6] > 0.0f 
			|| g_fControllerAxisDeadzone[11] > 0.0f
*/
		)
		{
			// Inverted pedal bugfix, combined Throttle&Brake axis split or deadzone set. Enable custom handle of controller events
			gtcRBRControllerAxisData = new DetourXS((LPVOID)0x4C2610, ::CustomRBRControllerAxisData, TRUE);
			Func_OrigRBRControllerAxisData = (tRBRControllerAxisData)gtcRBRControllerAxisData->GetTrampoline();
		}

		gtcRBRCallForHelp = new DetourXS((LPVOID)0x56E6D0, ::CustomRBRCallForHelp, TRUE);
		Func_OrigRBRCallForHelp = (tRBRCallForHelp)gtcRBRCallForHelp->GetTrampoline();

		// Override the gray RBRRX "loading track debug msg screen" with a real map preview img. Do this only one time when this plugin was initialized for the first time
		if (m_iMenuRBRRXOption && m_bShowCustomLoadTrackScreenRBRRX)
			RBRRX_OverrideLoadTrackScreen();

		// If RBRWindowPosition was set, but it defined just a monitor index (1..n). Enumerate all physical display monitors and get their coordinates. The RBR wnd is then moved to the coordinate set by the specific monitor#
		if (m_rbrWindowPosition.x > 0 && m_rbrWindowPosition.y == -1)
		{
			int iMonitorIdx = m_rbrWindowPosition.x - 1;
			std::vector<RECT> systemMonitorDescriptionVect;
			systemMonitorDescriptionVect.reserve(3);

			DebugPrint("GetName. Enumerating display monitors");
			::EnumDisplayMonitors(0, 0, MonitorEnumCallback, (LPARAM)&systemMonitorDescriptionVect);

			if ((size_t)iMonitorIdx < systemMonitorDescriptionVect.size())
			{
				m_rbrWindowPosition.x = systemMonitorDescriptionVect[iMonitorIdx].left;
				m_rbrWindowPosition.y = systemMonitorDescriptionVect[iMonitorIdx].top;
			}
			else
				m_rbrWindowPosition.x = m_rbrWindowPosition.y = -1;
		}

		// RBR memory and DX9 function hooks in place. Ready to do customized RBR logic
		g_bRBRHooksInitialized = TRUE;
		m_PluginState = T_PLUGINSTATE::PLUGINSTATE_INITIALIZED;

		//for (int idx = 0; idx < 8; idx++)
		//	DebugPrint(ReplacePathVariables(L"Resolution=%resolution% CarModelName=%carModelName% CarFolder=%carFolder% CarSlotNum=%carSlotNum% CarMenuNum=%carMenuNum% Cat=%FIACategory% FileType=%fileType% Plugin=%plugin%", idx, false).c_str());

		LogPrint("Completed the plugin initialization");
	}
	//DebugPrint("Exit CNGPCarMenu.GetName");

	return "NGPCarMenu";
}


//------------------------------------------------------------------------------------------------
// If profile renaming was active. Complete the process (either accept the new profile name or restore the previous name depending on if the new savedGames\pfProfileName.rbr file exists)
//
void CNGPCarMenu::CompleteProfileRenaming()
{
	if (m_bRenameDriverNameActive)
	{
		std::string sNewProfileFileNameWithoutPostfix;
		std::string sPrevProfileFileName;
		std::string sBakPrefix;

		m_bRenameDriverNameActive = FALSE;

		try
		{
			sNewProfileFileNameWithoutPostfix = m_sRBRRootDir + "\\SavedGames\\pf" + g_pRBRProfile->szProfileName;

			// If the new profile file exists then backup the previous profile before removing it (RBR uses profileName as a savedGames file name).
			// If the new file does NOT exist then restore the previous profile name.
			if (fs::exists(sNewProfileFileNameWithoutPostfix + ".rbr") /*&& fs::exists(sNewProfileFileNameWithoutPostfix + ".acm")*/)
			{
				LogPrint("Renaming a driver profile from %s to %s", m_sMenuPrevDriverName.c_str(), g_pRBRProfile->szProfileName);

				int idx = 1;
				while (idx < 200)
				{
					// Find unique backup profile filename
					sBakPrefix = "z_" + std::to_string(idx) + "_";
					sPrevProfileFileName = m_sRBRRootDir + "\\SavedGames\\" + sBakPrefix + "pf" + m_sMenuPrevDriverName;
					if (!fs::exists(sPrevProfileFileName + ".rbr.bak"))
						break;
					idx++;
				}

				LogPrint("Old profile stored in %s backup file", (sPrevProfileFileName + ".rbr.bak").c_str());

				// The new profile filename exists (the new profile name must be valid and creation succeeded). Backup the previous profile file just-in-case
				fs::rename(m_sRBRRootDir + "\\SavedGames\\pf" + m_sMenuPrevDriverName + ".rbr", sPrevProfileFileName + ".rbr.bak");
				fs::rename(m_sRBRRootDir + "\\SavedGames\\pf" + m_sMenuPrevDriverName + ".acm", sPrevProfileFileName + ".acm.bak");
			}
			else
			{
				// Restore the previous profile name because renaming was canceled or profile file creation failed (invalid filename chars? out of disk space?)
				strncpy_s(g_pRBRProfile->szProfileName, m_sMenuPrevDriverName.c_str(), COUNT_OF_ITEMS(g_pRBRProfile->szProfileName));
			}
		}
		catch (const fs::filesystem_error& ex)
		{
			LogPrint("ERROR. Failed to backup a profile file. %s.rbr.bak. %s", sPrevProfileFileName, ex.what());
		}
		catch (...)
		{
			// Eat all exceptions
		}
	}
}


//------------------------------------------------------------------------------------------------
//
void CNGPCarMenu::DrawResultsUI(void)
{
	// Do nothing
}


//------------------------------------------------------------------------------------------------
//
void CNGPCarMenu::DrawFrontEndPage(void)
{
	char szTextBuf[128]; // This should be enough to hold the longest g_RBRPlugin menu string + the logest g_RBRPluginMenu_xxxOptions option string
	float posY;
	int iRow;

	if (g_NGPCarMenu_AutoLogonOptions.size() == 0)
	{
		// First time initialization of NGPCarMenu plugin menu options. When RBR calls this method for the first time the Options and Plugins and custom plugin menu objects have been created by RBR game engine (those are not initialized until users navigates there for the first time)
		g_NGPCarMenu_AutoLogonOptions.push_back("Disabled");
		g_NGPCarMenu_AutoLogonOptions.push_back("Main");
		g_NGPCarMenu_AutoLogonOptions.push_back("Plugins");

		m_iMenuRallySchoolMenuOption = 0;

		if (g_pRBRPluginMenuSystem != nullptr)
		{
			for (int idx = g_pRBRPluginMenuSystem->pluginsMenuObj->firstSelectableItemIdx; idx < g_pRBRPluginMenuSystem->pluginsMenuObj->numOfItems - 1; idx++)
			{
				PRBRPluginMenuItemObj3 pItemArr = (PRBRPluginMenuItemObj3)g_pRBRPluginMenuSystem->pluginsMenuObj->pItemObj[idx];
				if (pItemArr != nullptr && pItemArr->szItemName != nullptr && pItemArr->szItemName[0] != '\0')
				{
					g_NGPCarMenu_AutoLogonOptions.push_back(pItemArr->szItemName);

					// Set the AutoLogon menu idx to the custom plugin idx in this vector
					if (_iEqual(m_sAutoLogon, pItemArr->szItemName))
						m_iMenuAutoLogonOption = max((idx - g_pRBRPluginMenuSystem->pluginsMenuObj->firstSelectableItemIdx) + 3, 0);

					// RallySchool menu option replacement idx if the menu is replaced with a custom plugin name
					if (_iEqual(m_sRallySchoolMenuReplacement, pItemArr->szItemName))
						m_iMenuRallySchoolMenuOption = max((idx - g_pRBRPluginMenuSystem->pluginsMenuObj->firstSelectableItemIdx) + 3, 0);
				}
			}
		}	

		if (m_iMenuAutoLogonOption <= 0)
		{
			if (_iEqual(m_sAutoLogon, "disabled", true)) m_iMenuAutoLogonOption = 0;
			else if (_iEqual(m_sAutoLogon, "main", true)) m_iMenuAutoLogonOption = 1;
			else if (_iEqual(m_sAutoLogon, "plugins", true)) m_iMenuAutoLogonOption = 2;
			else m_iMenuAutoLogonOption = (int)g_NGPCarMenu_AutoLogonOptions.size();
		}

		if (m_iMenuRallySchoolMenuOption <= 0)
		{
			if (_iEqual(m_sRallySchoolMenuReplacement, "disabled", true)) m_iMenuRallySchoolMenuOption = 0;
			else if (_iEqual(m_sRallySchoolMenuReplacement, "main", true)) m_iMenuRallySchoolMenuOption = 0; // Main is not relevant in menu replacement
			else if (_iEqual(m_sRallySchoolMenuReplacement, "plugins", true)) m_iMenuRallySchoolMenuOption = 2;
			else m_iMenuRallySchoolMenuOption = (int)g_NGPCarMenu_AutoLogonOptions.size();
		}
	}

	// Draw blackout (coordinates specify the 'window' where you don't want black background but the "RBR world" to be visible)
	m_pGame->DrawBlackOut(520.0f, 0.0f, 190.0f, 480.0f);

	// Draw custom plugin header line
	m_pGame->SetMenuColor(IRBRGame::MENU_HEADING);
	m_pGame->SetFont(IRBRGame::FONT_BIG);
	m_pGame->WriteText(65.0f, 19.0f, m_sPluginTitle.c_str());

	if (m_iMenuCurrentScreen == C_MENUCMD_RENAMEDRIVER)
	{
		// Draw gray background in the "text editbox" area
		m_pGame->SetColor(0x20, 0x20, 0x20, 180);
		m_pGame->DrawFlatBox(63.0f, 38.0f + (2 * 21.0f), 160.0f, 23.0f);

		m_pGame->SetMenuColor(IRBRGame::MENU_TEXT);
		
		sprintf_s(szTextBuf, sizeof(szTextBuf), "Rename driver profile: '%s'", g_pRBRProfile->szProfileName);
		m_pGame->WriteText(65.0f, 40.0f, szTextBuf);

		m_pGame->WriteText(65.0f, 40.0f + (1 * 21.0f), "Enter the new driver name:");
		m_pGame->WriteText(65.0f, 40.0f + (2 * 21.0f), m_sMenuNewDriverName.c_str());
	}
	else
	{
		// The red menu selection line (background color of the focused menu line)
		m_pGame->DrawSelection(0.0f, 38.0f + (static_cast<float>(m_iMenuSelection) * 21.0f), 440.0f);
		
		m_pGame->SetMenuColor(IRBRGame::MENU_TEXT);
		for (unsigned int i = 0; i < COUNT_OF_ITEMS(g_NGPCarMenu_PluginMenu); ++i)
		{
			if (i == C_MENUCMD_CREATEOPTION)
				sprintf_s(szTextBuf, sizeof(szTextBuf), "%s: %s", g_NGPCarMenu_PluginMenu[i].szMenuName, g_NGPCarMenu_CreateOptions[m_iMenuCreateOption]);
			else if (i == C_MENUCMD_IMAGEOPTION)
				sprintf_s(szTextBuf, sizeof(szTextBuf), "%s: %s", g_NGPCarMenu_PluginMenu[i].szMenuName, g_NGPCarMenu_ImageOptions[m_iMenuImageOption]);
			else if (i == C_MENUCMD_RBRTMOPTION)
				sprintf_s(szTextBuf, sizeof(szTextBuf), "%s: %s", g_NGPCarMenu_PluginMenu[i].szMenuName, g_NGPCarMenu_EnableDisableOptions[m_iMenuRBRTMOption]);
			else if (i == C_MENUCMD_RBRRXOPTION)
				sprintf_s(szTextBuf, sizeof(szTextBuf), "%s: %s", g_NGPCarMenu_PluginMenu[i].szMenuName, g_NGPCarMenu_EnableDisableOptions[m_iMenuRBRRXOption]);
			else if (i == C_MENUCMD_AUTOLOGONOPTION)
				sprintf_s(szTextBuf, sizeof(szTextBuf), "%s: %s", g_NGPCarMenu_PluginMenu[i].szMenuName, (m_iMenuAutoLogonOption < (int)g_NGPCarMenu_AutoLogonOptions.size() ? g_NGPCarMenu_AutoLogonOptions[m_iMenuAutoLogonOption] : "<unknown>"));
			else if (i == C_MENUCMD_RALLYSCHOOLMENUOPTION)
				sprintf_s(szTextBuf, sizeof(szTextBuf), "%s: %s", g_NGPCarMenu_PluginMenu[i].szMenuName, (m_iMenuRallySchoolMenuOption < (int)g_NGPCarMenu_AutoLogonOptions.size() ? g_NGPCarMenu_AutoLogonOptions[m_iMenuRallySchoolMenuOption] : "<unknown>"));
			else if (i == C_MENUCMD_RENAMEDRIVER)
				sprintf_s(szTextBuf, sizeof(szTextBuf), "%s: '%s'", g_NGPCarMenu_PluginMenu[i].szMenuName, g_pRBRProfile->szProfileName);
			else if (i == C_MENUCMD_SPLITCOMBINEDTHROTTLEBRAKE)
			{
				sprintf_s(szTextBuf, sizeof(szTextBuf), "%s: %s", g_NGPCarMenu_PluginMenu[i].szMenuName, (g_iXInputSplitThrottleBrakeAxis < 0 ? "Disabled" : std::to_string(g_iXInputSplitThrottleBrakeAxis + 1).c_str()));
				
				if (gtcRBRControllerAxisData == nullptr && g_iXInputSplitThrottleBrakeAxis >= 0)
					m_sMenuStatusText1 = "ATTENTION! You need to restart RBR to activate SplitCombinedThrottleBrake for the first time";
			}
			else if (i == C_MENUCMD_COCKPIT_CAMERASHAKING)
				sprintf_s(szTextBuf, sizeof(szTextBuf), "%s: %s%s", g_NGPCarMenu_PluginMenu[i].szMenuName, g_NGPCarMenu_DefaultEnableDisableOptions[m_iMenuCockpitCameraShaking], (m_iMenuCockpitCameraShaking == 1 ? " (use CAM_BONNET cam)" : ""));
			else if (i == C_MENUCMD_COCKPIT_STEERINGWHEEL)
				sprintf_s(szTextBuf, sizeof(szTextBuf), "%s: %s", g_NGPCarMenu_PluginMenu[i].szMenuName, g_NGPCarMenu_HiddenShownOptions[m_iMenuCockpitSteeringWheel]);
			else if (i == C_MENUCMD_COCKPIT_WIPERS)
				sprintf_s(szTextBuf, sizeof(szTextBuf), "%s: %s", g_NGPCarMenu_PluginMenu[i].szMenuName, g_NGPCarMenu_HiddenShownOptions[m_iMenuCockpitWipers]);
			else if (i == C_MENUCMD_COCKPIT_WINDSCREEN)
				sprintf_s(szTextBuf, sizeof(szTextBuf), "%s: %s", g_NGPCarMenu_PluginMenu[i].szMenuName, g_NGPCarMenu_HiddenShownOptions[m_iMenuCockpitWindscreen]);
			else if (i == C_MENUCMD_COCKPIT_OVERRIDEFOV)
			{
				if(m_iMenuCockpitOverrideFOV > 0)
					sprintf_s(szTextBuf, sizeof(szTextBuf), "%s: %s (%f)", g_NGPCarMenu_PluginMenu[i].szMenuName, g_NGPCarMenu_EnableDisableOptions[m_iMenuCockpitOverrideFOV], m_fMenuCockpitOverrideFOVValue);
				else
					sprintf_s(szTextBuf, sizeof(szTextBuf), "%s: %s", g_NGPCarMenu_PluginMenu[i].szMenuName, g_NGPCarMenu_EnableDisableOptions[m_iMenuCockpitOverrideFOV]);
			}
			else
				sprintf_s(szTextBuf, sizeof(szTextBuf), "%s", g_NGPCarMenu_PluginMenu[i].szMenuName);

			m_pGame->WriteText(65.0f, 40.0f + (static_cast<float>(i) * 21.0f), szTextBuf);
		}
	}

	m_pGame->SetFont(IRBRGame::FONT_SMALL);

	posY = 40.0f + (static_cast<float>COUNT_OF_ITEMS(g_NGPCarMenu_PluginMenu)) * 21.0f;
	iRow = 1;

	if (!m_sMenuStatusText1.empty()) m_pGame->WriteText(10.0f, posY + (static_cast<float>(iRow++) * 18.0f), m_sMenuStatusText1.c_str());
	if (!m_sMenuStatusText2.empty()) m_pGame->WriteText(10.0f, posY + (static_cast<float>(iRow++) * 18.0f), m_sMenuStatusText2.c_str());
	if (!m_sMenuStatusText3.empty()) m_pGame->WriteText(10.0f, posY + (static_cast<float>(iRow++) * 18.0f), m_sMenuStatusText3.c_str());

	m_pGame->WriteText(10.0f, /*posY + (static_cast<float>(iRow++) * 18.0f)*/480 - 21.0f, C_PLUGIN_FOOTER_STR);
}


//------------------------------------------------------------------------------------------------
//
//#if USE_DEBUG == 1
//typedef IPlugin* ( *tRBR_CreatePlugin)(IRBRGame* pGame);
//#endif

#define DO_MENUSELECTION_LEFTRIGHT(OptionID, OptionValueVariable, OptionArray, GlobalDirtyFlag, OptionDirtyFlag) \
   { OptionDirtyFlag = false; \
   if (m_iMenuSelection == OptionID) \
   { \
	  if(bLeft && OptionValueVariable > 0) { OptionValueVariable--; OptionDirtyFlag = GlobalDirtyFlag = true; } \
      else if (bRight && OptionValueVariable < COUNT_OF_ITEMS(OptionArray)-1) { OptionValueVariable++; OptionDirtyFlag = GlobalDirtyFlag = true; } \
   } }

   //if (m_iMenuSelection == OptionID && bLeft && (--OptionValueVariable) < 0) OptionValueVariable = 0; \
   //else if (m_iMenuSelection == OptionID && bRight && (++OptionValueVariable) >= COUNT_OF_ITEMS(OptionArray)) OptionValueVariable = COUNT_OF_ITEMS(OptionArray)-1

#define DO_MENUSELECTION_LEFTRIGHT_BOUNDS(OptionID, OptionValueVariable, OptionValueMin, OptionValueMax, GlobalDirtyFlag, OptionDirtyFlag) \
   { OptionDirtyFlag = false; \
   if (m_iMenuSelection == OptionID) \
   { \
	  if(bLeft && OptionValueVariable > OptionValueMin) { OptionValueVariable--; OptionDirtyFlag = GlobalDirtyFlag = true; } \
      else if (bRight && OptionValueVariable < OptionValueMax) { OptionValueVariable++; OptionDirtyFlag = GlobalDirtyFlag = true; } \
   } }

   //if (m_iMenuSelection == OptionID && bLeft && (--OptionValueVariable) < OptionValueMin) OptionValueVariable = OptionValueMin; \
   //else if (m_iMenuSelection == OptionID && bRight && (++OptionValueVariable) > OptionValueMax) OptionValueVariable = OptionValueMax

void CNGPCarMenu::HandleFrontEndEvents(char txtKeyboard, bool bUp, bool bDown, bool bLeft, bool bRight, bool bSelect)
{
	static std::string sWinInvalidFileChars = "<>:""/\\|?*";

	//DebugPrint("FrontEvent: Kbd=%d Up=%d Dow=%d Left=%d Right=%d Select=%d", (BYTE)txtKeyboard, bUp, bDown, bLeft, bRight, bSelect);

	// Clear the previous status text when menu selection changes
	if (m_iMenuCurrentScreen == 0 && (bUp || bDown || bSelect))
		m_sMenuStatusText1.clear();

	if (m_iMenuCurrentScreen == C_MENUCMD_RENAMEDRIVER)
	{
		if (bSelect)
		{
			//
			// Save new profile
			//

			// Set NGPCarMenu back to main menu before saving a new profile
			m_iMenuCurrentScreen = 0;

			m_sMenuStatusText1.clear();
			m_sMenuStatusText2.clear();
			m_sMenuStatusText3.clear();

			if (m_sMenuNewDriverName.empty())
				m_sMenuStatusText1 = "Profile renaming canceled";
			else if (fs::exists(m_sRBRRootDir + "\\SavedGames\\pf" + m_sMenuNewDriverName + ".rbr") /*|| fs::exists(m_sRBRRootDir + "\\SavedGames\\pf" + m_sMenuNewDriverName + ".acm")*/)
				m_sMenuStatusText1 = std::string("WARNING. Failed to rename driver profile. The profile ") + m_sMenuNewDriverName + " already exists";
			else
			{
				// Auto-navigate to SaveProfile YES/NO screen
				m_autoLogonSequenceSteps.clear();
				m_autoLogonSequenceSteps.push_back("main");
				m_autoLogonSequenceSteps.push_back("driverprofile");
				m_autoLogonSequenceSteps.push_back("saveprofile");

				m_sMenuPrevDriverName = g_pRBRProfile->szProfileName;
				strncpy_s(g_pRBRProfile->szProfileName, m_sMenuNewDriverName.c_str(), COUNT_OF_ITEMS(g_pRBRProfile->szProfileName));
				StartNewAutoLogonSequence(false);
				//LogPrint(L"SaveProfile sequence. %s", m_autoLogonSequenceLabel.str().c_str());

				// Navigate to main menu to start a new autologon sequence
				g_pRBRMenuSystem->currentMenuObj = g_pRBRMenuSystem->currentMenuObj2 = g_pRBRMenuSystem->menuObj[RBRMENUIDX_MAIN];
				m_bRenameDriverNameActive = TRUE;
			}
		}
		else if (bLeft)
		{
			// Left arrow is the "backspace" key because the real backspace closes the custom plugin (TODO. Find out how to trick RBR not to close the custom plugin in backspace key)
			if (m_sMenuNewDriverName.length() > 0)
				m_sMenuNewDriverName.pop_back();
		}
		else if (bUp || bDown)
		{
			// Change the ascii code of the last char (this way it is possible to set those chars not supported by RBR txtKeyboard events)
			if (m_sMenuNewDriverName.length() <= 0)
				m_sMenuNewDriverName += ' ';

			BYTE lastChar = m_sMenuNewDriverName[m_sMenuNewDriverName.length() - 1];
			while (TRUE)
			{
				lastChar += (bUp ? 1 : -1);
				if (lastChar <= 32) lastChar = 254;
				else if (lastChar >= 255) lastChar = 33;

				if (sWinInvalidFileChars.find_first_of(lastChar) == std::string::npos)
					break;
			}

			m_sMenuNewDriverName[m_sMenuNewDriverName.length() - 1] = lastChar;
		}
		else
		{
			// Add new char to a new driver name string (if valid char and the string is not yet 15 chars and char is not invalid Win filename char. 
			// The RBR profile name char array is 16 bytes=15 chars+nullTerminator)
			if (txtKeyboard >= 32 && m_sMenuNewDriverName.length() < 15 && sWinInvalidFileChars.find_first_of(txtKeyboard) == std::string::npos)
				m_sMenuNewDriverName += txtKeyboard;
		}
	}
	else
	{
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
				m_bCustomReplayShowCroppingRect = false;
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
				//if (m_iCustomReplayCarID >= 0 && CNGPCarMenu::PrepareScreenshotReplayFile(m_iCustomReplayCarID))
				if (m_iCustomReplayCarID >= 0)
				{
					ClearCachedCarPreviewImages();

					// Create the preview rectangle around the screenshot cropping area to highlight the screenshot area (user can tweak INI file settings if cropping area is not perfect)
					D3D9CreateRectangleVertexBuffer(g_pRBRIDirect3DDevice9, (float)this->m_screenshotCroppingRect.left, (float)this->m_screenshotCroppingRect.top, (float)(this->m_screenshotCroppingRect.right - this->m_screenshotCroppingRect.left), (float)(this->m_screenshotCroppingRect.bottom - this->m_screenshotCroppingRect.top), &m_screenshotCroppingRectVertexBuffer);
					
					if(m_screenshotCroppingRectRSF.bottom != -1)
						D3D9CreateRectangleVertexBuffer(g_pRBRIDirect3DDevice9, (float)this->m_screenshotCroppingRectRSF.left, (float)this->m_screenshotCroppingRectRSF.top, (float)(this->m_screenshotCroppingRectRSF.right - this->m_screenshotCroppingRectRSF.left), (float)(this->m_screenshotCroppingRectRSF.bottom - this->m_screenshotCroppingRectRSF.top), &m_screenshotCroppingRectVertexBufferRSF);

					// Set a flag that custom replay generation is active during replays. If this is zero then replay plays the file normally
					m_iCustomReplayState = 1;

					//::RBRAPI_Replay(this->m_sRBRRootDir, C_REPLAYFILENAME_SCREENSHOT);
					g_pRBRGameMode->gameStatus = 0x01;
					g_pRBRGameMode->gameMode = 0x03;
					g_pRBRMapSettingsEx->trackID = 0x04;
					m_pGame->StartGame(71, m_iCustomReplayCarID, IRBRGame::GOOD_WEATHER, IRBRGame::TYRE_GRAVEL_DRY, nullptr);
				}
				else
				{
					m_iCustomReplayState = 0;
					m_sMenuStatusText1 = "All cars already have a preview image. Did not create any new images.";
				}

			}
			else if (m_iMenuSelection == C_MENUCMD_RENAMEDRIVER)
			{
				m_bRenameDriverNameActive = FALSE;	// Renaming and saving a new profile is not yet active (user could cancel the process)
				m_sMenuNewDriverName.clear();
				m_iMenuCurrentScreen = C_MENUCMD_RENAMEDRIVER;

				m_sMenuStatusText1 = "BACKSPACE key closes this screen. Use LEFT ARROW key as backspace.";
				m_sMenuStatusText2 = "Use UP/DOWN ARROW keys to choose characters not supported by the original RBR profile name editor.";
				m_sMenuStatusText3 = "The profile name should have only valid WinOS filename characters.";
			}

#if USE_DEBUG == 1
			else if (m_iMenuSelection == C_MENUCMD_SELECTCARSKIN)
			{
/*				__asm {
					nop
					nop
					int 3
					nop
					nop
				}

				D3DPRESENT_PARAMETERS pp;
				ZeroMemory(&pp, sizeof(pp));
				//g_pRBRIDirect3DDevice9->Reset(&pp);
				IDirect3D9* pDD9;
				//IDirect3DDevice9* pIDD9;
				D3DDEVTYPE devType = D3DDEVTYPE_HAL;
				g_pRBRIDirect3DDevice9->GetDirect3D(&pDD9);
				pDD9->CreateDevice(0, devType, 0, 0, &pp, &g_pRBRIDirect3DDevice9);
*/
			}
#endif
		}


		//
		// Menu focus line moved up or down
		//
		if (bUp || bDown)
		{
			if (bUp && (--m_iMenuSelection) < 0)
			{
				//m_iMenuSelection = COUNT_OF_ITEMS(g_RBRPluginMenu) - 1;  // Wrap around logic
				m_iMenuSelection = 0;  // No wrapping logic
			}

			if (bDown && (++m_iMenuSelection) >= COUNT_OF_ITEMS(g_NGPCarMenu_PluginMenu))
			{
				//m_iMenuSelection = 0; // Wrap around logic
				m_iMenuSelection = COUNT_OF_ITEMS(g_NGPCarMenu_PluginMenu) - 1;
			}

			m_sMenuStatusText1 = (g_NGPCarMenu_PluginMenu[m_iMenuSelection].szMenuTooltip != nullptr ? g_NGPCarMenu_PluginMenu[m_iMenuSelection].szMenuTooltip : "");
		}


		//
		// Menu options changed in the current menu line. Options don't wrap around.
		// Note! Not all menu lines have any additional options.
		//
		bool bDirtyFlag = false;
		bool bOptionDirtyFlag = false;

		DO_MENUSELECTION_LEFTRIGHT(C_MENUCMD_CREATEOPTION, m_iMenuCreateOption, g_NGPCarMenu_CreateOptions, bDirtyFlag, bOptionDirtyFlag);
		DO_MENUSELECTION_LEFTRIGHT(C_MENUCMD_IMAGEOPTION,  m_iMenuImageOption, g_NGPCarMenu_ImageOptions, bDirtyFlag, bOptionDirtyFlag);

		DO_MENUSELECTION_LEFTRIGHT(C_MENUCMD_RBRTMOPTION, m_iMenuRBRTMOption, g_NGPCarMenu_EnableDisableOptions, bDirtyFlag, bOptionDirtyFlag);
		if (m_iMenuRBRTMOption == 1 && bOptionDirtyFlag)
			g_bNewCustomPluginIntegrations = TRUE;

		DO_MENUSELECTION_LEFTRIGHT(C_MENUCMD_RBRRXOPTION, m_iMenuRBRRXOption, g_NGPCarMenu_EnableDisableOptions, bDirtyFlag, bOptionDirtyFlag);
		if (m_iMenuRBRRXOption == 1 && bOptionDirtyFlag)
			g_bNewCustomPluginIntegrations = TRUE;

		DO_MENUSELECTION_LEFTRIGHT_BOUNDS(C_MENUCMD_AUTOLOGONOPTION, m_iMenuAutoLogonOption, 0, ((int)g_NGPCarMenu_AutoLogonOptions.size() - 1), bDirtyFlag, bOptionDirtyFlag);

		DO_MENUSELECTION_LEFTRIGHT_BOUNDS(C_MENUCMD_RALLYSCHOOLMENUOPTION, m_iMenuRallySchoolMenuOption, 0, ((int)g_NGPCarMenu_AutoLogonOptions.size() - 1), bDirtyFlag, bOptionDirtyFlag);
		if (bOptionDirtyFlag)
			m_sRallySchoolMenuReplacement = g_NGPCarMenu_AutoLogonOptions[m_iMenuRallySchoolMenuOption];

		DO_MENUSELECTION_LEFTRIGHT_BOUNDS(C_MENUCMD_SPLITCOMBINEDTHROTTLEBRAKE, g_iXInputSplitThrottleBrakeAxis, -1, 3, bDirtyFlag, bOptionDirtyFlag);

		DO_MENUSELECTION_LEFTRIGHT(C_MENUCMD_COCKPIT_CAMERASHAKING, m_iMenuCockpitCameraShaking, g_NGPCarMenu_DefaultEnableDisableOptions, bDirtyFlag, bOptionDirtyFlag);
		DO_MENUSELECTION_LEFTRIGHT(C_MENUCMD_COCKPIT_STEERINGWHEEL, m_iMenuCockpitSteeringWheel, g_NGPCarMenu_HiddenShownOptions, bDirtyFlag, bOptionDirtyFlag);
		DO_MENUSELECTION_LEFTRIGHT(C_MENUCMD_COCKPIT_WIPERS, m_iMenuCockpitWipers, g_NGPCarMenu_HiddenShownOptions, bDirtyFlag, bOptionDirtyFlag);
		DO_MENUSELECTION_LEFTRIGHT(C_MENUCMD_COCKPIT_WINDSCREEN, m_iMenuCockpitWindscreen, g_NGPCarMenu_HiddenShownOptions, bDirtyFlag, bOptionDirtyFlag);

		DO_MENUSELECTION_LEFTRIGHT(C_MENUCMD_COCKPIT_OVERRIDEFOV, m_iMenuCockpitOverrideFOV, g_NGPCarMenu_EnableDisableOptions, bDirtyFlag, bOptionDirtyFlag);

		//
		// Save settings if any of the persistent options was changed
		//
		if(bDirtyFlag)
		{
			SaveSettingsToPluginINIFile();
		}
	}
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
	// Do nothing
}


//------------------------------------------------------------------------------------------------
// Is called when the player timer starts (after GO! or in case of a false start)
//
void CNGPCarMenu::StageStarted(int iMap, const char* ptxtPlayerName, bool bWasFalseStart)
{
	// Do nothing
}


//---------------------------------------------------
std::string CNGPCarMenu::GetRBRInstallType()
{
	if (m_bRBRTMPluginInstalled) return "TM";			// TM rbr installation
	if (m_bRallySimFansPluginInstalled) return "RSF";	// RallySimFans rbr installation
	return "RBR"; // Vanilla rbr or some other online plugin installation?
}


//---------------------------------------------------
// RBR=Normal RBR (ie. no custom plugin), RBRTM, RBRRX, RSF
//
std::string CNGPCarMenu::GetActivePluginName()
{
	if (m_bRBRTMPluginActive)
		return "TM";

	if (m_bRBRRXPluginActive)
		return "RX";			// At some point RBRRX may be active even when the custom plugin is not the current menu obj (ie. stage selected in RBRRX stages menu and the RBR stage options menu is open)

	if (g_pRBRPluginIntegratorLinkList != nullptr)
	{
		for (auto& item : *g_pRBRPluginIntegratorLinkList)
			if (item->m_bCustomPluginActive)
				return item->m_sCustomPluginName;
	}

	// Check if the game is in standard "RBR" state or unknown custom plugin
	if ( g_pRBRMenuSystem != nullptr && g_pRBRMenuSystem->currentMenuObj != nullptr && g_pRBRMenuSystem->currentMenuObj == g_pRBRPluginMenuSystem->customPluginMenuObj )
		return "UNK"; // Unknown custom plugin
	
	return "RBR";
}

// 0=no replay, 1=Normal RBR/TM/RSF (classic map), 2=BTB (rbrrx/btb map)
int CNGPCarMenu::GetActiveReplayType()
{
	if (m_bRBRRXReplayActive) 
		return 2;
	else if (m_bRBRReplayActive)
		return 1;

	return 0;
}

// 0=no racing, 1=Normal RBR/TM/RSF (classic map), 2=BTB (rbrrx/rsf with BTB map)
int CNGPCarMenu::GetActiveRacingType()
{
	if (m_bRBRRXRacingActive) 
		return 2;
	else if (m_bRBRRacingActive)
		return 1;

	return 0;
}


//----------------------------------------------------------------------------------------------------
// Draw red progress bar to visualize some waiting logic (fex in RBR LoadTrack screen)
//
void CNGPCarMenu::DrawProgressBar(D3DRECT rec, float progressValue, LPDIRECT3DDEVICE9 pOutputD3DDevice)
{
	int iBarWidth;
	int iCompletedIdx;

	if (pOutputD3DDevice == nullptr)
		pOutputD3DDevice = g_pRBRIDirect3DDevice9;

	iCompletedIdx = static_cast<int>(progressValue * 20.0f);
	iBarWidth = (rec.x2 - rec.x1) / 20;	

	//DebugPrint("DrawProgressBar. progressValue=%f iCompletedIdx=%d", progressValue, iCompletedIdx);

	for (int idx = 0; idx < 20; idx++)
	{
		rec.x2 = rec.x1 + 1;
		pOutputD3DDevice->Clear(1, &rec, D3DCLEAR_TARGET, C_DARKGREYBACKGROUND_COLOR, 0, 0);
		
		rec.x1 = rec.x2;
		rec.x2 = rec.x1 + (iBarWidth - 1);
		pOutputD3DDevice->Clear(1, &rec, D3DCLEAR_TARGET, (idx < iCompletedIdx ? C_REDSEPARATORLINE_COLOR : C_LIGHTGREYBACKGROUND_COLOR), 0, 0);

		rec.x1 += (iBarWidth - 2);
	}
}


//----------------------------------------------------------------------------------------------------
// Plugin activated/deactivated handler
//
void CNGPCarMenu::OnPluginActivated(const std::string& pluginName)
{
	DebugPrint("OnPluginActivated %s", pluginName.c_str());

	if(pluginName == "TM")
		m_bRBRTMPluginActive = true;
	else if (pluginName == "RX")
		m_bRBRRXPluginActive = true;
}

void CNGPCarMenu::OnPluginDeactivated(const std::string& pluginName)
{
	DebugPrint("OnPluginDeactivated %s", pluginName.c_str());

	if (pluginName == "TM")
	{
		m_bRBRTMPluginActive = false;

		// When RBRTM plugin is deactivated then fix a RBRTM bug where QuickRally and RBRRX sometimes crashes or cannot start a stage if the previously driven stage was a RBRTM stage.
		// The bug is because of RBRTM "hijacking" the stage start routine and expecting QuickRally and BTB rally launch to provide certain RBRTM specific identifiers. Restore temporarly
		// the default RBR behaviour and re-initialize it when RBRTM launches a new stage the next time.
		if (m_bRBRTMTrackLoadBugFixWhenNotActive && m_pRBRTMPlugin != nullptr)
		{
			// Set back the default RBR behavior in this RBR func handler
			BYTE buffer[2] = { 0xC2, 0x04 };
			WriteOpCodeBuffer((LPVOID)0x57157C, buffer, sizeof(buffer));
			m_pRBRTMPlugin->activeStatus = 0x00;
		}
	}
	else if (pluginName == "RX")
		m_bRBRRXPluginActive = false;
}


//----------------------------------------------------------------------------------------------------
// Prepare for track loading
//
BOOL CNGPCarMenu::OnPrepareTrackLoad(const std::string& pluginName, int rallyType, const std::string& rallyName, DWORD rallyOptions)
{
	DebugPrint("OnPrepareTrackLoad. %s %d %s %x", pluginName.c_str(), rallyType, rallyName.c_str(), rallyOptions);

	m_iRBRCustomRaceType = rallyType;
	m_dwRBRCustomRallyOptions = rallyOptions;

	if (m_dwRBRCustomRallyOptions != 0)
	{
		// Store original config values because forceDisable rallyOptions may modify these. These are restored on OnRaceEnded call
		m_bOrig3DPacenotes = g_pRBRGameConfigEx->show3DPacenotes;
		m_bOrigPacenoteDistanceCountdown = g_pRBRGameConfigEx->showPacenoteDistanceCountdown;
		m_bOrigPacenotes = g_pRBRGameConfigEx->showPacenotes;
		m_iOrigPacenoteStack = g_pRBRGameConfigEx->pacenoteStack;
	}

	return TRUE;
}


//----------------------------------------------------------------------------------------------------
// A map for racing or replaying successfully loaded (note! OnRaceStarted/OnReplayStarted called already before this handler)
//
void CNGPCarMenu::OnMapLoaded()
{	
	static int prevRBRTMMapID = -1;

	DebugPrint("DEBUG: CustomRallyOptions=%x", m_dwRBRCustomRallyOptions);

	m_bMapLoadedCalled = TRUE;
	m_bMapUnloadedCalled = FALSE;

	RBRAPI_InitializeRaceTimeObjReferences();

	// Racing is about to begin. Store the current racing carID and trackID because replay metadata INI file needs this information
	m_latestCarID = g_pRBRMapSettings->carID;
	m_latestMapID = g_pRBRGameModeExt2->trackID;
	//m_latestMapID = g_pRBRMapSettings->trackID;

	// TODO: Move these to "latest" struct
	m_latestFalseStartPenaltyTime = m_latestOtherPenaltyTime = 0.0f;
	m_latestCallForHelpCount = 0;
	m_latestCallForHelpTick = 0;

	m_prevRaceTimeClock = g_pRBRCarInfo->raceTime;

	if (GetActivePluginName() == "TM")
	{
		// RBRTM doesn't update the RBR stage name string value, TM uses always "Rally HQ" string, so read the stageName option from Tracks.ini file
		if (prevRBRTMMapID != m_latestMapID)
		{
			m_latestMapName = GetMapNameByMapID(m_latestMapID);
			prevRBRTMMapID = m_latestMapID;
		}

		RBRTM_OnMapLoaded();
	}
	else
	{
		prevRBRTMMapID  = -1;
		m_latestMapName = (g_pRBRMapLocationName != nullptr ? g_pRBRMapLocationName : L"");

		if (GetActivePluginName() == "RX")
			RBRRX_OnMapLoaded();
	}

	DebugPrint(L"OnMapLoaded. CarID=%d MapID=%d %d  MapName=%s ActivePlugin=%s", m_latestCarID, m_latestMapID, g_pRBRGameModeExt2->trackID, m_latestMapName.c_str(), _ToWString(GetActivePluginName()).c_str());
}

void CNGPCarMenu::OnMapUnloaded()
{
	DebugPrint("OnMapUnloaded. RBRRacingActive=%d  RBRRXRacingActive=%d  Map=%s (%d %d)", m_bRBRRacingActive, m_bRBRRXRacingActive, _ToString(m_latestMapName).c_str(), m_latestMapID, g_pRBRGameModeExt->trackID);
	m_bMapLoadedCalled = FALSE;
	m_bMapUnloadedCalled = TRUE;

	if (GetActiveRacingType() >= 1)
	{
		std::string sMapName = _ToString(m_latestMapName);
		int mapKey = RaceStatDB_GetMapKey(m_latestMapID, sMapName, GetActiveRacingType());
		int carKey = RaceStatDB_GetCarKey(m_latestCarID);

		if (mapKey >= 0 && carKey >= 0)
		{
			if (RaceStatDB_AddCurrentRallyResult(mapKey, m_latestMapID, carKey, m_latestCarID, sMapName, m_iRBRCustomRaceType) < 0)
				LogPrint("WARNING. Failed to save a new RaceStatDB rally result record. Check %s db file", m_raceStatDBFilePath.c_str());
		}
	}
}

void CNGPCarMenu::OnRaceStarted()
{
	ModifyCarModelFiles(g_pRBRMapSettings->carID);

	m_rbrLatestStageResults.clear(); // Notify rbr recent list to update the list of recent races when the RBR returns to the main screen

#if USE_DEBUG == 1
	// DEBUG. Test custom trackLoad img
	//SetupCustomTrackLoadImage(std::wstring(L"c:\\apps\\rbr\\Textures\\splash\\121-*.dds"), nullptr, nullptr, IMAGE_TEXTURE_POSITION_HORIZONTAL_CENTER | IMAGE_TEXTURE_SCALE_PRESERVE_ASPECTRATIO);
#endif

	m_bRBRRacingActive = TRUE;
	m_bMapLoadedCalled = m_bMapUnloadedCalled = FALSE;

	// If custom BTB PrepareBTBTrack API call was called then RBRRX racing flag is already set, but if RBRRX plugin was used to start a BTB track then check if RBRRX plugin is active to determine if this rally is with BTB track (and not the normal RBR #41 Cortez racing)
	if (!m_bRBRRXRacingActive)
		m_bRBRRXRacingActive = (g_pRBRPlugin->GetActivePluginName() == "RX");

	if (m_bRallySimFansPluginInstalled && (g_pRBRMapSettings->carID >= 0 && g_pRBRMapSettings->carID <= 7))
	{
		int carMenuIdx = RBRAPI_MapCarIDToMenuIdx(g_pRBRMapSettings->carID);

		// RSF plugin modifies car slots on the fly. Update the standard RBR car name to match with the RSF car selection because some other plugins and external tools expect to see the car name in RBR data structures
		InitCarSpecData_RBRCIT(carMenuIdx);

		// Car model names are in WCHAR, but RBR uses CHAR strings in car menu names. Convert the wchar car model name to char string and use only N first chars.
		//size_t len = 0;
		//wcstombs_s(&len, g_RBRCarSelectionMenuEntry[carMenuIdx].szCarMenuName, sizeof(g_RBRCarSelectionMenuEntry[carMenuIdx].szCarMenuName), g_RBRCarSelectionMenuEntry[carMenuIdx].wszCarModel, _TRUNCATE);
	}

/*
	// Racing started. Start a watcher thread to receive notifications of new rbr\Replays\fileName.rpl replay files if NGPCarMenu generated RPL metadata file generation is enabled
	if (!g_watcherNewReplayFiles.Running() && m_bGenerateReplayMetadataFile)
	{
		if (g_watcherNewReplayFileListener == nullptr)
		{
			g_watcherNewReplayFiles.SetDir(m_sRBRRootDirW + L"\\Replays");
			g_watcherNewReplayFileListener = new RBRReplayFileWatcherListener();
			g_watcherNewReplayFiles.AddFileChangeListener(g_watcherNewReplayFileListener);
		}

		// Make sure the fileQueue is empty before starting a new watcher thread (usually at this point the queue should be empty)
		g_watcherNewReplayFileListener->DoCompletion(TRUE);
		g_watcherNewReplayFiles.Start();
	}
*/

#if USE_DEBUG == 1
	// DEBUG. test forceDisable options
	//OnPrepareTrackLoad("", 0, "", RALLYOPTION_FORCEDISABLE_PACENOTESYMBOLS);
	//OnPrepareTrackLoad("", 0, "", RALLYOPTION_FORCEDISABLE_3DPACENOTES | RALLYOPTION_FORCEDISABLE_PACENOTEDISTANCECOUNTDOWN);
#endif

	DebugPrint("OnRaceStarted. GameMode=%d  RBRRacingActive=%d  RBRRXRacingActive=%d", g_pRBRGameMode->gameMode, m_bRBRRacingActive, m_bRBRRXRacingActive);
}

void CNGPCarMenu::OnRaceEnded()
{
	DebugPrint("OnRaceEnded. RBRRacingActive=%d  RBRRXRacingActive=%d  Map=%s (%d %d)", m_bRBRRacingActive, m_bRBRRXRacingActive, _ToString(m_latestMapName).c_str(), m_latestMapID, g_pRBRGameModeExt->trackID);

	m_bRBRRacingActive = m_bRBRRXRacingActive = m_bRBRReplayOrRacingEnding = FALSE;
	m_iRBRCustomRaceType = 0;

	if (m_dwRBRCustomRallyOptions != 0)
	{
		// Restore force-disabled pacenote settings
		g_pRBRGameConfigEx->show3DPacenotes = m_bOrig3DPacenotes;
		g_pRBRGameConfigEx->showPacenoteDistanceCountdown = m_bOrigPacenoteDistanceCountdown;
		g_pRBRGameConfigEx->showPacenotes = m_bOrigPacenotes;
		g_pRBRGameConfigEx->pacenoteStack = m_iOrigPacenoteStack;
		m_dwRBRCustomRallyOptions = 0;
	}

	ClearCustomTrackLoadImage();
}

void CNGPCarMenu::OnReplayStarted()
{
	m_iRBRCustomRaceType = 0;
	m_bRBRReplayActive = TRUE;
	DebugPrint("OnReplayStarted. RBRReplayActive=%d  RBRRXReplayActive=%d", m_bRBRReplayActive, m_bRBRRXReplayActive);
}

void CNGPCarMenu::OnReplayEnded()
{
	DebugPrint("OnReplayEnded. RBRReplayActive=%d  RBRRXReplayActive=%d  CurrentMnuIdx=%d", m_bRBRReplayActive, m_bRBRRXReplayActive, RBRAPI_MapRBRMenuObjToID(g_pRBRMenuSystem->currentMenuObj));

	if (GetActiveReplayType() == 2)
	{
		// To complete RBRRX replay we need to wait until the current menu is back to customPlugin or if the menu is empty (null, RBRRX bug in certain scenarios)
		if (g_pRBRMenuSystem->currentMenuObj == nullptr || (g_pRBRMenuSystem->currentMenuObj == g_pRBRPluginMenuSystem->customPluginMenuObj))
		{
			DebugPrint("OnReplayEnded. RBRRXReplay. currentMenuObj=%08x", (DWORD)g_pRBRMenuSystem->currentMenuObj);

			//
			// RBRRX replay was active, but now menu is back to mainmenu or a custom plugin menu or a blank menu (RBRRX bug). 
			// Complete the BTB replaying and jump back to RBR main menu if RBRRX bug left the game in the blank menu and threw back to a custom plugin
			//
			g_pRBRMenuSystem->currentMenuObj = g_pRBRMenuSystem->currentMenuObj2 = g_pRBRMenuSystem->menuObj[RBRMENUIDX_MAIN];
			m_bRBRReplayActive = m_bRBRRXReplayActive = m_bRBRReplayOrRacingEnding = FALSE;
		}
	}
	else
		m_bRBRReplayActive = m_bRBRRXReplayActive = m_bRBRReplayOrRacingEnding = FALSE;

	if (m_bAutoExitAfterReplay)
	{
		LogPrint("Automatically exiting RBR");
		::PostMessage(g_hRBRWnd, WM_CLOSE, 0, 0);
		m_bAutoExitAfterReplay = FALSE;
	}
}


//----------------------------------------------------------------------------------------------------
//
inline void CNGPCarMenu::CustomRBRDirectXBeginScene()
{
	if (g_pRBRGameMode->gameMode == 03)
	{
		if (g_pOrigRallySchoolMenuText == nullptr && m_iMenuRallySchoolMenuOption >= 2)
		{
			g_pOrigRallySchoolMenuText = (wchar_t*) ((PRBRPluginMenuItemObj2)g_pRBRMenuSystem->menuObj[RBRMENUIDX_MAIN]->pItemObj[3])->wszMenuTitleName;
			if (g_pOrigRallySchoolMenuText != nullptr)
				((PRBRPluginMenuItemObj2)g_pRBRMenuSystem->menuObj[RBRMENUIDX_MAIN]->pItemObj[3])->wszMenuTitleName = g_wszRallySchoolMenuReplacementText;
		}

		if (m_bRenameDriverNameActive)
		{
			// If the current menu or the focused profile menu line has changed then user has either accepted or canceled the profile saving (renaming)
			if (m_iAutoLogonMenuState == 0 && (g_pRBRMenuSystem->currentMenuObj != g_pRBRMenuSystem->menuObj[RBRMENUIDX_DRIVERPROFILE] || g_pRBRMenuSystem->currentMenuObj->selectedItemIdx != m_iProfileMenuPrevSelectedIdx) )
				CompleteProfileRenaming();
		}

		if (g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_RALLYSCHOOL] && m_iMenuRallySchoolMenuOption >= 2 && m_iAutoLogonMenuState == 0)
		{
			// RallySchool menu is replaced with a custom menu option (2=Plugins menu, 2>Custom plugin idx)
			if (g_pRBRPluginMenuSystem->pluginsMenuObj != nullptr)
				g_pRBRMenuSystem->currentMenuObj = g_pRBRPluginMenuSystem->pluginsMenuObj;
			else
				g_pRBRMenuSystem->currentMenuObj = g_pRBRMenuSystem->menuObj[RBRMENUIDX_MAIN];
				
			m_autoLogonSequenceSteps.clear();
			m_autoLogonSequenceSteps.push_back("main");
			m_autoLogonSequenceSteps.push_back("options");
			m_autoLogonSequenceSteps.push_back("plugins");
			if (m_iMenuRallySchoolMenuOption > 2)
				m_autoLogonSequenceSteps.push_back(m_sRallySchoolMenuReplacement);

			StartNewAutoLogonSequence(false);
		}
		else if (g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_QUICKRALLY_CARS]
			|| g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P1]
			|| g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P2]
			|| g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P3]
			|| g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P4]
			|| g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_RBRCHALLENGE_CARS])
		{
			//
			// RBR is in "Menu open" state (gameMode=3) and the current menu object is showing "select a car" menu.
			// Show customized car model name, specs, NGP physics and 3D model details and a car preview picture.
			//

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
					// Find where these menu specific pos/width/height values are originally stored. Now tweak the height everytime car menu selection is about to be shown.
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

						pCarMenuSpecTexts->wszModelTitle = (WCHAR*)GetLangStr(L"FIA Category:");                // FIACategory label
						pCarMenuSpecTexts->wszHorsepowerTitle = (WCHAR*)GetLangStr(g_pOrigCarSpecTitleHorsepower); // Horsepower text
						pCarMenuSpecTexts->wszTorqueTitle = (WCHAR*)GetLangStr(g_pOrigCarSpecTitleWeight);	     // Re-use rbr localized Weight title in Torque spec line
						pCarMenuSpecTexts->wszEngineTitle = (WCHAR*)GetLangStr(g_pOrigCarSpecTitleTransmission);// Re-use rbr localized transmission title in Weight spec line
						pCarMenuSpecTexts->wszTyresTitle = (WCHAR*)GetLangStr(L"Year:");						 // Year text

						// Original Weight and Transmission title and value lines are hidden
						pCarMenuSpecTexts->wszWeightTitle = pCarMenuSpecTexts->wszWeightValue =
							pCarMenuSpecTexts->wszTransmissionTitle = pCarMenuSpecTexts->wszTransmissionValue = L"";
					}

					// Change default carSpec values each time a new car menu line was selected or SelectCar menu was opened
					if (pCarMenuSpecTexts->wszTechSpecValue != pCarSelectionMenuEntry->wszCarModel || m_bMenuSelectCarCustomized == false)
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

					m_bMenuSelectCarCustomized = true;
				}
			}
		}
		else if (m_bMenuSelectCarCustomized)
		{
			// "CarSelect" menu closed and the tyre brand string was modified. Restore the original tyre brand string value
			WriteOpCodeBuffer((LPVOID)PTR_TYREVALUE_WCHAR_FIRESTONE, (const BYTE*)L"Firestone", 9 * sizeof(WCHAR));  // No need to write NULL char because it is already there 
			WriteOpCodeBuffer((LPVOID)PTR_TYREVALUE_WCHAR_BRIDGESTONE, (const BYTE*)L"Bridgestone", 11 * sizeof(WCHAR));
			WriteOpCodeBuffer((LPVOID)PTR_TYREVALUE_WCHAR_PIRELLI, (const BYTE*)L"Pirelli", 7 * sizeof(WCHAR));
			WriteOpCodeBuffer((LPVOID)PTR_TYREVALUE_WCHAR_MICHELIN, (const BYTE*)L"Michelin", 8 * sizeof(WCHAR));

			// Restore car description to show car name instead of FIA category as shown in "Select Car" menu
			for (int selectedCarIdx = 0; selectedCarIdx < 8; selectedCarIdx++)
				WriteOpCodePtr((LPVOID)g_RBRCarSelectionMenuEntry[selectedCarIdx].ptrCarDescription, g_RBRCarSelectionMenuEntry[selectedCarIdx].szCarMenuName);

			m_bMenuSelectCarCustomized = false;
		}


		// If there are new custom plugin integration requests then initialize those now
		if (g_bNewCustomPluginIntegrations)
			InitAllNewCustomPluginIntegrations();

		if (g_pRBRPluginMenuSystem->customPluginMenuObj != nullptr)
		{
			// Check if RBRTM plugin is active
			if (m_iMenuRBRTMOption == 1)
			{
				if (!m_bRBRTMPluginActive && g_pRBRMenuSystem->currentMenuObj == g_pRBRPluginMenuSystem->customPluginMenuObj && g_pRBRPluginMenuSystem->pluginsMenuObj->selectedItemIdx == m_iRBRTMPluginMenuIdx)
					OnPluginActivated("TM");
				else if (m_bRBRTMPluginActive && (g_pRBRMenuSystem->currentMenuObj == g_pRBRPluginMenuSystem->optionsMenuObj || g_pRBRMenuSystem->currentMenuObj == g_pRBRPluginMenuSystem->pluginsMenuObj || g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_MAIN]))
					// Menu is back in RBR Plugins/Options/Main menu, so RBRTM cannot be the foreground plugin anymore
					OnPluginDeactivated("TM");
			}

			// Check if RBRRX plugin is active
			if (m_iMenuRBRRXOption == 1)
			{
				if (!m_bRBRRXPluginActive && g_pRBRMenuSystem->currentMenuObj == g_pRBRPluginMenuSystem->customPluginMenuObj && m_iRBRRXPluginMenuIdx == g_pRBRPluginMenuSystem->pluginsMenuObj->selectedItemIdx)
					OnPluginActivated("RX");
				else if (m_bRBRRXPluginActive && (g_pRBRMenuSystem->currentMenuObj == g_pRBRPluginMenuSystem->optionsMenuObj || g_pRBRMenuSystem->currentMenuObj == g_pRBRPluginMenuSystem->pluginsMenuObj || g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_MAIN]))
					OnPluginDeactivated("RX");
			}

			// Check if any of the custom plugins are active
			if (g_pRBRPluginIntegratorLinkList != nullptr)
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
	}
	else if (m_iCustomReplayState > 0)
	{
		//
		// NGPCarMenu plugin is generating car preview screenshot images. Draw a car model and take a screenshot from a specified cropping screen area
		//

		m_bCustomReplayShowCroppingRect = true;
		//g_pRBRCarInfo->stageStartCountdown = 1.0f;
		g_pRBRCarInfo->stageStartCountdown = 7.0f;

		if (m_iCustomReplayState == 2)
		{
			// Move car into a screenshot position
			g_pRBRGameMode->gameMode = 0x0A;

			m_iCustomReplayState = 4;
			m_tCustomReplayStateStartTime = std::chrono::steady_clock::now();

			g_pRBRCameraInfo = g_pRBRCarInfo->pCamera->pCameraInfo;
			g_pRBRCarMovement = (PRBRCarMovement) * (DWORD*)(0x008EF660);

			g_pRBRCameraInfo->cameraType = 3;

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

			if (m_screenshotCarPosition != 1)
			{
				// Normal car position (screenshotCarPosition==0)
				g_pRBRCarMovement->carMapLocation.m[0][0] = -0.929464f;
				g_pRBRCarMovement->carMapLocation.m[0][1] = -0.367257f;
				g_pRBRCarMovement->carMapLocation.m[0][2] = -0.032216f;
				g_pRBRCarMovement->carMapLocation.m[0][3] = 0.366664f;
			}
			else
			{
				// Car in the middle of nowhere, in the sky (screenshotCarPosition==1)
				g_pRBRCarMovement->carMapLocation.m[0][0] = 4000.929464f;
				g_pRBRCarMovement->carMapLocation.m[0][1] = 4000.367257f;
				g_pRBRCarMovement->carMapLocation.m[0][2] = 4000.032216f;
				g_pRBRCarMovement->carMapLocation.m[0][3] = 4000.366664f;
			}

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
/*
		else if (false && m_iCustomReplayState == 6)
		{
			// Screenshot taken. Prepare the next car for a screenshot
			m_bCustomReplayShowCroppingRect = false;
			g_pRBRCarInfo->stageStartCountdown = 7.0f;

			m_iCustomReplayState = 0;
			m_iCustomReplayCarID = GetNextScreenshotCarID(m_iCustomReplayCarID);

			if (m_iCustomReplayCarID >= 0)
			{
				DebugPrint("m_iCustomReplayState=6. NextReplayCarID");

				//if (CNGPCarMenu::PrepareScreenshotReplayFile(m_iCustomReplayCarID))
				//	m_iCustomReplayState = 1;
				m_iCustomReplayState = 1;

				g_pRBRGameMode->gameMode = 0x03;
				g_pRBRGameMode->trackID = 0x04;
				g_pRBRGameMode->gameStatus = 0x01;

				__asm int 3
				__asm nop 
				__asm nop 

				m_pGame->StartGame(71, m_iCustomReplayCarID, IRBRGame::GOOD_WEATHER, IRBRGame::TYRE_GRAVEL_DRY, nullptr);
			}
			else
			{
				// TODO quit the stage
			}

			//::RBRAPI_Replay(m_sRBRRootDir, C_REPLAYFILENAME_SCREENSHOT);
		}
*/

/*
		if (g_pRBRGameMode->gameMode == 0x08)
		{
			// Don't start the normal replay logic of RBR
			g_pRBRGameMode->gameMode = 0x0A;
			//g_pRBRGameModeExt->gameModeExt = 0x04;
		}

		if (g_pRBRGameMode->gameMode == 0x0A && g_pRBRGameModeExt->gameModeExt == 0x01 && m_iCustomReplayState == 1)
		{
			m_iCustomReplayState = 2;
			m_tCustomReplayStateStartTime = std::chrono::steady_clock::now();
		}
		else if (g_pRBRGameMode->gameMode == 0x0A && g_pRBRGameModeExt->gameModeExt == 0x01 && m_iCustomReplayState == 3)
		{
			m_iCustomReplayState = 4;
			m_tCustomReplayStateStartTime = std::chrono::steady_clock::now();

			g_pRBRGameMode->gameMode = 0x0A;
			g_pRBRGameModeExt->gameModeExt = 0x04;

			g_pRBRCameraInfo = g_pRBRCarInfo->pCamera->pCameraInfo;
			g_pRBRCarMovement = (PRBRCarMovement) * (DWORD*)(0x008EF660);

			g_pRBRCameraInfo->cameraType = 3;

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
			
			if (m_screenshotCarPosition != 1)
			{
				// Normal car position (screenshotCarPosition==0)
				g_pRBRCarMovement->carMapLocation.m[0][0] = -0.929464f;
				g_pRBRCarMovement->carMapLocation.m[0][1] = -0.367257f;
				g_pRBRCarMovement->carMapLocation.m[0][2] = -0.032216f;
				g_pRBRCarMovement->carMapLocation.m[0][3] = 0.366664f;
			}
			else
			{
				// Car in the middle of nowhere, in the sky (screenshotCarPosition==1)
				g_pRBRCarMovement->carMapLocation.m[0][0] = 4000.929464f;
				g_pRBRCarMovement->carMapLocation.m[0][1] = 4000.367257f;
				g_pRBRCarMovement->carMapLocation.m[0][2] = 4000.032216f;
				g_pRBRCarMovement->carMapLocation.m[0][3] = 4000.366664f;
			}

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
		else if (m_iCustomReplayState == 6)
		{
			// Screenshot taken. Prepare the next car for a screenshot
			m_bCustomReplayShowCroppingRect = false;
			g_pRBRCarInfo->stageStartCountdown = 7.0f;

			m_iCustomReplayState = 0;
			m_iCustomReplayCarID = GetNextScreenshotCarID(m_iCustomReplayCarID);

			if (m_iCustomReplayCarID >= 0)
			{
				if (CNGPCarMenu::PrepareScreenshotReplayFile(m_iCustomReplayCarID))
					m_iCustomReplayState = 1;
			}

			::RBRAPI_Replay(m_sRBRRootDir, C_REPLAYFILENAME_SCREENSHOT);
		}
*/

	}
}


//----------------------------------------------------------------------------------------------------
//
inline HRESULT CNGPCarMenu::CustomRBRDirectXEndScene(void* objPointer)
{
	HRESULT hResult;


#if USE_DEBUG == 1
/*
	static int prevMode = -1;
	static int prevModeExt = -1;
	static PRBRMenuObj prevMenu = nullptr;
	if (g_pRBRGameMode->gameMode != prevMode || prevMenu != g_pRBRMenuSystem->currentMenuObj || g_pRBRGameModeExt->gameModeExt != prevModeExt)
	{
		int menuIdx;
		for (menuIdx = 0; menuIdx < RBRMENUSYSTEM_NUM_OF_MENUS; menuIdx++)
			if (g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[menuIdx])
				break;

		if (menuIdx >= RBRMENUSYSTEM_NUM_OF_MENUS) 
			menuIdx = -1;

		DebugPrint("Mode=%x ModeExt=%x Mnu=%08x MnuIdx=%d %s", 
			g_pRBRGameMode->gameMode, 
			g_pRBRGameModeExt->gameModeExt, 
			(DWORD)g_pRBRMenuSystem->currentMenuObj, 
			menuIdx,
			(g_pRBRMenuSystem->currentMenuObj == g_pRBRPluginMenuSystem->customPluginMenuObj ? "customPlugin" : 
			 g_pRBRMenuSystem->currentMenuObj == g_pRBRPluginMenuSystem->optionsMenuObj ? "optionsMenu" :
			 g_pRBRMenuSystem->currentMenuObj == g_pRBRPluginMenuSystem->pluginsMenuObj ? "pluginsMenu" : ""
			));

		prevMode = g_pRBRGameMode->gameMode;
		prevModeExt = g_pRBRGameModeExt->gameModeExt;
		prevMenu = g_pRBRMenuSystem->currentMenuObj;
	}
*/
#endif

/*
	if (g_pRBRGameMode->gameMode == 0x02 || g_pRBRGameMode->gameMode == 0x09)
	{
		// Racing is about to end (but RBR is still in "Restart/SaveReplay" menu). 
		// Start a watcher thread to receive notifications of new rbr\Replays\fileName.rpl replay files
		if (!g_watcherNewReplayFiles.Running() && m_bGenerateReplayMetadataFile)
		{
			if (g_watcherNewReplayFileListener == nullptr)
			{
				g_watcherNewReplayFiles.SetDir(m_sRBRRootDirW + L"\\Replays");
				g_watcherNewReplayFileListener = new RBRReplayFileWatcherListener();
				g_watcherNewReplayFiles.AddFileChangeListener(g_watcherNewReplayFileListener);
			}

			// Make sure the fileQueue is empty before starting a new watcher thread (usually at this point the queue should be empty)
			g_watcherNewReplayFileListener->DoCompletion(TRUE);
			g_watcherNewReplayFiles.Start();
		}
	}

	else
*/ 
	
	if (g_pRBRGameMode->gameMode == 0x0D)
	{
		if(!m_bMapLoadedCalled) OnMapLoaded();
	}

	else if (g_pRBRGameMode->gameMode == 0x09 || g_pRBRGameMode->gameMode == 0x06)
	{
		if (!m_bMapUnloadedCalled) OnMapUnloaded();
	}

	else if (g_pRBRGameMode->gameMode == 0x05)
	{
		// Map is being loaded (prepare OnMapLoaded handler call when the map loading is completed)
		if (m_bMapLoadedCalled)
		{
			// If this was a restart then call MapUnloaded to "finalize" the previous race before restarting the new race
			if(!m_bMapUnloadedCalled) OnMapUnloaded();
			m_bMapLoadedCalled = FALSE;
		}

		// Draw custom track loading img if it was set
		if (GetActiveRacingType() == 1 && m_customTrackLoadImg.pTexture != nullptr)
		{
			if (!(m_customTrackLoadImg.dwImageFlags & IMAGE_TEXTURE_ALPHA_BLEND))
			{
				D3DRECT rect = { static_cast<long>(m_customTrackLoadImg.x), static_cast<long>(m_customTrackLoadImg.y), static_cast<long>(m_customTrackLoadImg.x + m_customTrackLoadImg.cx), static_cast<long>(m_customTrackLoadImg.y + m_customTrackLoadImg.cy) };
				g_pRBRIDirect3DDevice9->Clear(1, &rect, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 0, 0, 0), 0, 0);
			}
			else 
				m_pD3D9RenderStateCache->EnableTransparentAlphaBlending();

			D3D9DrawVertexTex2D(g_pRBRIDirect3DDevice9, m_customTrackLoadImg.pTexture, m_customTrackLoadImg.vertexes2D, m_pD3D9RenderStateCache);
			m_pD3D9RenderStateCache->RestoreState();
		}
	}

	else if (g_pRBRGameMode->gameMode == 0x0C)
	{
		if (!m_bRBRReplayOrRacingEnding && (GetActiveRacingType() > 0 || GetActiveReplayType() > 0))
			m_bRBRReplayOrRacingEnding = TRUE;

		if (m_bFirstTimeWndInitialization)
		{
			m_bFirstTimeWndInitialization = FALSE;

			// If INI file defines a custom RBR wnd location then move the wnd now
			if (m_rbrWindowPosition.x != -1 || m_rbrWindowPosition.y != -1)
			{
				LogPrint("RBR window position %d %d", m_rbrWindowPosition.x, m_rbrWindowPosition.y);
				::MoveWindow(g_hRBRWnd, m_rbrWindowPosition.x, m_rbrWindowPosition.y, g_rectRBRWnd.right - g_rectRBRWnd.left, g_rectRBRWnd.bottom - g_rectRBRWnd.top, TRUE);
			}
		}
	}

	else if (g_pRBRGameMode->gameMode == 0x0A)
	{
		if (m_iCustomReplayState >= 2 && m_bCustomReplayShowCroppingRect && m_iCustomReplayState != 4)
		{
			// Draw rectangle to highlight the screenshot capture area (except when state == 4 because then this plugin takes the car preview screenshot and we don't want to see the gray box in a preview image)
			if(m_screenshotCroppingRectVertexBuffer != nullptr)
				D3D9DrawVertex2D(g_pRBRIDirect3DDevice9, m_screenshotCroppingRectVertexBuffer);

			if(m_screenshotCroppingRectVertexBufferRSF != nullptr)
				D3D9DrawVertex2D(g_pRBRIDirect3DDevice9, m_screenshotCroppingRectVertexBufferRSF);
		}
	}

	else if (g_pRBRGameMode->gameMode == 0x08)
	{
		int iRacingType = GetActiveRacingType();
		if (iRacingType > 0)
		{
			// Racing ended and replay started from the racing menu. Call OnRaceEnded handler and set a replay status
			OnRaceEnded();
			if (iRacingType == 2) m_bRBRRXReplayActive = TRUE;
			OnReplayStarted();
		}
	}

	else if (g_pRBRGameMode->gameMode == 0x02)
	{
		if (m_iRBRCustomRaceType == 1 && GetActiveRacingType() > 0 && g_pRBRRaceTimePauseMenuSystem->currentMenuObj != nullptr)
		{
			if(g_pRBRRaceTimePauseMenuSystem->currentMenuObj->numOfItems >= 8)
				((PRBRPluginMenuItemObj2)g_pRBRRaceTimePauseMenuSystem->currentMenuObj->pItemObj[7])->menuItemStatus |= 0x01;
		}
	}

	else if (g_pRBRGameMode->gameMode == 0x03)
	{
		int posX;
		int posY;
		int iFontHeight;

		if (m_iAutoLogonMenuState > 0)
		{
			if (m_bShowAutoLogonProgressText)
			{
				std::wstringstream sLogonSequenceText;
				sLogonSequenceText << L"AUTOLOGON sequence of NGPCarMenu activated. ";
				if (m_bAutoLogonWaitProfile) sLogonSequenceText << L"Choose a profile to continue. ";
				sLogonSequenceText << m_autoLogonSequenceLabel.str();

				RBRAPI_MapRBRPointToScreenPoint(50.0f, 0, &posX, nullptr);
				g_pFontCarSpecCustom->DrawText(posX, 10, C_CARSPECTEXT_COLOR, sLogonSequenceText.str().c_str(), D3DFONT_CLEARTARGET);
			}

			DoAutoLogonSequence();
		} 
		else if (g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_QUICKRALLY_CARS]
			|| g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P1]
			|| g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P2]
			|| g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P3]
			|| g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P4]
			|| g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_RBRCHALLENGE_CARS]
			)
		{
			//
			// Show custom car details in "SelectCar" menu (RBR menu screen)
			//

			int selectedCarIdx = g_pRBRMenuSystem->currentMenuObj->selectedItemIdx - g_pRBRMenuSystem->currentMenuObj->firstSelectableItemIdx;

			if (selectedCarIdx >= 0 && selectedCarIdx <= 7)
			{
				if (m_carPreviewTexture[selectedCarIdx].pTexture == nullptr && m_carPreviewTexture[selectedCarIdx].imgSize.cx >= 0 && g_RBRCarSelectionMenuEntry[selectedCarIdx].wszCarModel[0] != '\0')
				{
					float posYf;
					float cx, cy;

					::RBRAPI_MapRBRPointToScreenPoint(0.0f, g_pRBRMenuSystem->menuImagePosY - 1.0f, nullptr, &posYf);

					if (m_carPictureScale == -1)
					{
						// The old default behaviour when CarPictureScale is not set. The image is drawn using the original size without scaling and stretching
						cx = cy = 0.0f;
					}
					else
					{
						// Define the exact drawing area rect and optionally scale the picture within the rect or stretch it to fill the area
						if (m_carSelectRightBlackBarRect.right != 0)
							cx = (float)(m_carSelectRightBlackBarRect.right - m_carSelectLeftBlackBarRect.left);
						else
							cx = (float)(g_rectRBRWndClient.right - m_carSelectLeftBlackBarRect.left);

						if (m_carSelectRightBlackBarRect.bottom != 0)
							cy = (float)(m_carSelectLeftBlackBarRect.bottom - posYf);
						else
							cy = (float)(g_rectRBRWndClient.bottom - posYf);
					}

					ReadCarPreviewImageFromFile(selectedCarIdx,
						(float)m_carSelectLeftBlackBarRect.left,
						posYf,
						cx, cy, //0, 0, 
						&m_carPreviewTexture[selectedCarIdx],
						(m_carPictureScale == -1 ? 0 : m_carPictureScale),
						false);
				}

				// If the car preview image is successfully initialized (imgSize.cx >= 0) and texture (=image) is prepared then draw it on the screen
				if (m_carPreviewTexture[selectedCarIdx].imgSize.cx >= 0 && m_carPreviewTexture[selectedCarIdx].pTexture != nullptr)
				{
					D3DRECT rec;

					//int r, g, b, a;
					//RBRAPI_MapRBRColorToRGBA(IRBRGame::EMenuColors::MENU_BKGROUND, &r, &g, &b, &a);

					// Draw black side bars (if set in INI file)
					rec.x1 = m_carSelectLeftBlackBarRect.left;
					rec.y1 = m_carSelectLeftBlackBarRect.top;
					rec.x2 = m_carSelectLeftBlackBarRect.right;
					rec.y2 = m_carSelectLeftBlackBarRect.bottom;
					g_pRBRIDirect3DDevice9->Clear(1, &rec, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 0, 0, 0) /*D3DCOLOR_ARGB(a, r, g, b)*/ , 0, 0);

					rec.x1 = m_carSelectRightBlackBarRect.left;
					rec.y1 = m_carSelectRightBlackBarRect.top;
					rec.x2 = m_carSelectRightBlackBarRect.right;
					rec.y2 = m_carSelectRightBlackBarRect.bottom;
					g_pRBRIDirect3DDevice9->Clear(1, &rec, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 0, 0, 0) /*D3DCOLOR_ARGB(a, r, g, b)*/, 0, 0);

					// Draw car preview image (use transparent alpha channel bits if those are set in PNG file)
					if (m_carPictureUseTransparent)
						m_pD3D9RenderStateCache->EnableTransparentAlphaBlending();

					D3D9DrawVertexTex2D(g_pRBRIDirect3DDevice9, m_carPreviewTexture[selectedCarIdx].pTexture, m_carPreviewTexture[selectedCarIdx].vertexes2D);

					if (m_carPictureUseTransparent)
						m_pD3D9RenderStateCache->RestoreState();
				}


				//
				// Car 3D Model info textbo
				//
				iFontHeight = g_pFontCarSpecCustom->GetTextHeight();

				//RBRAPI_MapRBRPointToScreenPoint(rbrPosX, g_pRBRMenuSystem->menuImagePosY, &posX, &posY);
				RBRAPI_MapRBRPointToScreenPoint(218.0f, g_pRBRMenuSystem->menuImagePosY, &posX, &posY);
				posY -= 6 * iFontHeight;
				if (m_car3DModelInfoPosition.x != 0) posX = m_car3DModelInfoPosition.x;  // Custom X-position
				if (m_car3DModelInfoPosition.y != 0) posY = m_car3DModelInfoPosition.y;  // Custom Y-position

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
					if(pCarSelectionMenuEntry->wszCarFMODBankAuthors[0] != L'\0')
						g_pFontCarSpecCustom->DrawText(posX, (iCarSpecPrintRow++) * iFontHeight + posY, C_CARSPECTEXT_COLOR, (std::wstring(pCarSelectionMenuEntry->wszCarFMODBank) + L" by " + pCarSelectionMenuEntry->wszCarFMODBankAuthors).c_str(), 0);
					else
						g_pFontCarSpecCustom->DrawText(posX, (iCarSpecPrintRow++) * iFontHeight + posY, C_CARSPECTEXT_COLOR, pCarSelectionMenuEntry->wszCarFMODBank, 0);
			}
		}
		else
		{
			RBRTM_EndScene();
			RBRRX_EndScene();

			if (g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_MAIN])
			{
				if (m_rbrLatestStageResults.size() <= 0 && m_recentResultsPosition.y != -1)
					RaceStatDB_QueryLastestStageResults(-1, "", 0, m_rbrLatestStageResults);

				if (m_rbrLatestStageResults.size() > 0)
				{
					if (m_rbrLatestStageResultsBackground != nullptr)
						D3D9DrawVertex2D(g_pRBRIDirect3DDevice9, m_rbrLatestStageResultsBackground);

					DrawRecentResultsTable(m_recentResultsPosition.x, m_recentResultsPosition.y, m_rbrLatestStageResults);
				}
			}
		}


		//
		// Draw images and text objects set by custom plugin integrations (if the custom plugin is activated)
		//
		if (g_pRBRPluginIntegratorLinkList != nullptr)
		{
			for (auto& item : *g_pRBRPluginIntegratorLinkList)
			{
				if (item->m_bCustomPluginActive)
				{
					for (auto& imageItem : item->m_imageList)
					{
						if (imageItem->m_bShowImage)
						{
							if (imageItem->m_imageTexture.pTexture != nullptr && imageItem->m_imageSize.cx != -1)
							{
								// Image item
								if (imageItem->m_dwImageFlags & IMAGE_TEXTURE_ALPHA_BLEND)
									m_pD3D9RenderStateCache->EnableTransparentAlphaBlending();

								D3D9DrawVertexTex2D(g_pRBRIDirect3DDevice9, imageItem->m_imageTexture.pTexture, imageItem->m_imageTexture.vertexes2D, m_pD3D9RenderStateCache);

								m_pD3D9RenderStateCache->RestoreState();
							}
							else if (imageItem->m_minimapData.vectMinimapPoint.size() >= 2)
							{
								// Minimap item
								int centerPosX, centerPosY;
								if (imageItem->m_dwImageFlags & IMAGE_TEXTURE_POSITION_HORIZONTAL_CENTER)
									centerPosX = max(((imageItem->m_minimapData.minimapRect.right - imageItem->m_minimapData.minimapRect.left) / 2) - (imageItem->m_minimapData.minimapSize.x / 2), 0);
								else if (imageItem->m_dwImageFlags & IMAGE_TEXTURE_POSITION_HORIZONTAL_RIGHT)
									centerPosX = max((imageItem->m_minimapData.minimapRect.right - imageItem->m_minimapData.minimapRect.left) - imageItem->m_minimapData.minimapSize.x, 0);
								else
									centerPosX = 0;

								if (imageItem->m_dwImageFlags & IMAGE_TEXTURE_POSITION_VERTICAL_CENTER)
									centerPosY = max(((imageItem->m_minimapData.minimapRect.bottom - imageItem->m_minimapData.minimapRect.top) / 2) - (imageItem->m_minimapData.minimapSize.y / 2), 0);
								else if (imageItem->m_dwImageFlags & IMAGE_TEXTURE_POSITION_BOTTOM)
									centerPosY = max((imageItem->m_minimapData.minimapRect.bottom - imageItem->m_minimapData.minimapRect.top) - imageItem->m_minimapData.minimapSize.y, 0);
								else
									centerPosY = 0;

								m_pD3D9RenderStateCache->EnableTransparentAlphaBlending();
								//g_pFontCarSpecCustom->DrawTextA(1, 1, D3DCOLOR_ARGB(255, 0xF0, 0xF0, 0xF0), "minimap",0);

								// Draw the minimap driveline
								for (auto& item : imageItem->m_minimapData.vectMinimapPoint)
								{
									DWORD dwColor;
									switch (item.splitType)
									{
									case 1:  dwColor = D3DCOLOR_ARGB(180, 0xF0, 0xCD, 0x30); break;	// Color for split1->split2 part of the track (yellow)
									//default: dwColor = D3DCOLOR_ARGB(180, 0xF0, 0xF0, 0xF0); break; // Color for start->split1 and split2->finish (white)
									default: dwColor = D3DCOLOR_ARGB(180, 0x60, 0xA8, 0x60); break; // Color for start->split1 and split2->finish (green)
									}

									D3D9DrawPrimitiveCircle(g_pRBRIDirect3DDevice9,
										(float)(centerPosX + imageItem->m_minimapData.minimapRect.left + item.drivelineCoord.x),
										(float)(centerPosY + imageItem->m_minimapData.minimapRect.top + item.drivelineCoord.y),
										2.0f, dwColor);
								}
								
								// Draw the finish line as bigger red circle
								D3D9DrawPrimitiveCircle(g_pRBRIDirect3DDevice9,
									(float)(centerPosX + imageItem->m_minimapData.minimapRect.left + imageItem->m_minimapData.vectMinimapPoint.back().drivelineCoord.x),
									(float)(centerPosY + imageItem->m_minimapData.minimapRect.top + imageItem->m_minimapData.vectMinimapPoint.back().drivelineCoord.y),
									5.0f, D3DCOLOR_ARGB(255, 0xC0, 0x10, 0x10));

								// Draw the start line as bigger green circle
								D3D9DrawPrimitiveCircle(g_pRBRIDirect3DDevice9,
									(float)(centerPosX + imageItem->m_minimapData.minimapRect.left + imageItem->m_minimapData.vectMinimapPoint.front().drivelineCoord.x),
									(float)(centerPosY + imageItem->m_minimapData.minimapRect.top + imageItem->m_minimapData.vectMinimapPoint.front().drivelineCoord.y),
									5.0f, D3DCOLOR_ARGB(255, 0x20, 0xF0, 0x20));

								m_pD3D9RenderStateCache->RestoreState();
							}
						}
					}
					
					for (auto& textItem : item->m_textList)
					{
						CD3DFont* pFont = nullptr;
						if (!textItem->m_sText.empty() || !textItem->m_wsText.empty())
						{
							for (auto& fontItem : *g_pRBRPluginIntegratorFontList)
							{
								if ((DWORD)fontItem.get() == textItem->m_fontID)
								{
									pFont = fontItem.get();
									break;
								}
							}

							if (pFont != nullptr)
							{
								// Draw char or wchar str version (only one of these strings set at any time per textItem
								if(!textItem->m_sText.empty())
									pFont->DrawText(textItem->m_posX, textItem->m_posY, textItem->m_dwColor, textItem->m_sText.c_str(), textItem->m_dwDrawOptions);
								else
									pFont->DrawText(textItem->m_posX, textItem->m_posY, textItem->m_dwColor, textItem->m_wsText.c_str(), textItem->m_dwDrawOptions);
							}
						}
					}
				}
			}
		}

		if (m_bRBRReplayOrRacingEnding)
		{
			/*
			if (g_watcherNewReplayFiles.Running())
			{
				// Racing has ended. Check if there are new replay files and create replayFileName.ini metadata file if necessary
				g_watcherNewReplayFiles.Stop();
				g_watcherNewReplayFileListener->DoCompletion();
			}
			*/

			// Complete racing or replaying
			if (GetActiveReplayType() == 0) OnRaceEnded();
			else OnReplayEnded();		
		}

		// Sometimes RBRRX plugin has a bug and it ends up in empty menu. Jump back to main menu if this happens
		if (g_pRBRMenuSystem->currentMenuObj == nullptr && g_pRBRMenuSystem->currentMenuObj2 == nullptr)
		{
			DebugPrint("EndScene. currentMenuObj and currentMenuObj2 null. Jump back to main menu");
			g_pRBRMenuSystem->currentMenuObj = g_pRBRMenuSystem->currentMenuObj2 = g_pRBRMenuSystem->menuObj[RBRMENUIDX_MAIN];
		}
	}

#if USE_DEBUG == 1
	else if (m_iCustomSelectCarSkinState > 0)
	{
		g_pFontCarSpecModel->DrawText(5, 5, C_CARMODELTITLETEXT_COLOR, g_RBRCarSelectionMenuEntry[RBRAPI_MapCarIDToMenuIdx(g_pRBRGameMode->carID)].wszCarModel, D3DFONT_CLEARTARGET);

	}
#endif

#if USE_DEBUG == 1
/*
		WCHAR szTxtBuffer[512];

		if (m_pRBRRXPlugin != nullptr)
		{
			int iRBRRX_NumOfItems = m_pRBRRXPlugin->numOfItems;
			int iRBRRX_SelectedItemIdx = (m_pRBRRXPlugin->pMenuData != nullptr ? m_pRBRRXPlugin->pMenuData->selectedItemIdx : -1);

			swprintf_s(szTxtBuffer, COUNT_OF_ITEMS(szTxtBuffer), L"A=%d RX=%08x RXMID=%d Idx=%d/%d Stge=%s",
				m_bRBRRXPluginActive,
				(DWORD)m_pRBRRXPlugin,
				m_pRBRRXPlugin->menuID,
				iRBRRX_SelectedItemIdx,
				iRBRRX_NumOfItems,
				((iRBRRX_NumOfItems > 0 && iRBRRX_SelectedItemIdx >= 0 && iRBRRX_SelectedItemIdx < iRBRRX_NumOfItems) ? _ToWString(m_pRBRRXPlugin->pMenuItems[iRBRRX_SelectedItemIdx].szTrackName).c_str() : L"")
			);
			g_pFontDebug->DrawText(1, 1 * 20, C_DEBUGTEXT_COLOR, szTxtBuffer, D3DFONT_CLEARTARGET);

			swprintf_s(szTxtBuffer, COUNT_OF_ITEMS(szTxtBuffer), L"CurrTotalIdx=%d", m_currentCustomMapSelectedItemIdxRBRRX);
			g_pFontDebug->DrawText(1, 2 * 20, C_DEBUGTEXT_COLOR, szTxtBuffer, D3DFONT_CLEARTARGET);
		}
*/

/*
		if (g_pRBRPlugin->m_pRBRTMPlugin != nullptr && g_pRBRPlugin->m_pRBRTMPlugin->pCurrentRBRTMMenuObj != nullptr && g_pRBRPlugin->m_pRBRTMPlugin->pCurrentRBRTMMenuObj->menuID == 0x15978154)
		{
			int iRBRTM_NumOfItems = ((g_pRBRPlugin->m_pRBRTMPlugin->pCurrentRBRTMMenuObj->pMenuData != nullptr) ? g_pRBRPlugin->m_pRBRTMPlugin->pCurrentRBRTMMenuObj->pMenuData->numOfItems : -1);
			int iRBRTM_SelectedItemIdx = g_pRBRPlugin->m_pRBRTMPlugin->selectedItemIdx;

			swprintf_s(szTxtBuffer, COUNT_OF_ITEMS(szTxtBuffer), L"M=%08x TM=%08x TM=%08x Idx=%d/%d Stge=%d",
				(DWORD)g_pRBRMenuSystem->currentMenuObj,
				(DWORD)g_pRBRPlugin->m_pRBRTMPlugin,
				g_pRBRPlugin->m_pRBRTMPlugin->pCurrentRBRTMMenuObj->menuID,
				iRBRTM_SelectedItemIdx,
				iRBRTM_NumOfItems,
				((iRBRTM_NumOfItems > 0 && iRBRTM_SelectedItemIdx >= 0 && iRBRTM_SelectedItemIdx < iRBRTM_NumOfItems) ? g_pRBRPlugin->m_pRBRTMPlugin->pCurrentRBRTMMenuObj->pMenuData->pMenuItems[g_pRBRPlugin->m_pRBRTMPlugin->selectedItemIdx].mapID : -1)
			);

			g_pFontDebug->DrawText(1, 1 * 20, C_DEBUGTEXT_COLOR, szTxtBuffer, D3DFONT_CLEARTARGET);
		}
*/
		

	/*
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
	*/

	/*
		int x, y;
		RBRAPI_MapRBRPointToScreenPoint(451.0f, 244.0f, &x, &y);
		g_pFontDebug->DrawText(x, y, C_DEBUGTEXT_COLOR, "XX", D3DFONT_CLEARTARGET);
		swprintf_s(szTxtBuffer, COUNT_OF_ITEMS(szTxtBuffer), L"(451,244)=(%d,%d)", x, y);
		g_pFontDebug->DrawText(x, y + (20 * 1), C_DEBUGTEXT_COLOR, szTxtBuffer, D3DFONT_CLEARTARGET);
	*/
#endif

	// Call original RBR DXEndScene function and let it to do whatever needed to complete the drawing of DX framebuffer (needs to be done before screenshot preview image is generated)
	hResult = Func_OrigRBRDirectXEndScene(objPointer);

	// If the plugin is not generating preview images then complete the EndScene method now
	if (m_iCustomReplayState <= 0)
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
	
	//g_pRBRCarInfo->stageStartCountdown = 7.0f;

/*	if (m_iCustomReplayState == 2)
	{
		DebugPrint("m_iCustomReplayState=2");

		std::chrono::steady_clock::time_point tCustomReplayStateNowTime = std::chrono::steady_clock::now();
		auto iTimeElapsedSec = std::chrono::duration_cast<std::chrono::milliseconds>(tCustomReplayStateNowTime - m_tCustomReplayStateStartTime).count();
		//if (iTimeElapsedSec >= 1200)
		//	m_iCustomReplayState = 3;
		m_iCustomReplayState = 3;
	}
	else 
*/
	if (m_iCustomReplayState == 1)
	{
		if(g_pRBRGameMode->gameMode == 0x0A)
			m_iCustomReplayState = 2;
	}
	else if (m_iCustomReplayState == 4)
	{
		std::chrono::steady_clock::time_point tCustomReplayStateNowTime = std::chrono::steady_clock::now();
		auto iTimeElapsedSec = std::chrono::duration_cast<std::chrono::milliseconds>(tCustomReplayStateNowTime - m_tCustomReplayStateStartTime).count();
		if (iTimeElapsedSec >= 500)
		{
			// Take a RBR car preview screenshot and save it as PNG preview file.
			// At this point the cropping highlight rectangle is hidden, so it is not shown in the screenshot.
			std::wstring outputFileName = ReplacePathVariables(m_screenshotPath, RBRAPI_MapCarIDToMenuIdx(m_iCustomReplayCarID), false);

			// Save the screenshot img if "overwrite files" create option is set or the output img file doesn't exist yet
			if (m_iMenuCreateOption > 0 || !fs::exists(outputFileName))
			{
				LogPrint(L"RBR screenshot: ScreenshotPath=%s -> %s", m_screenshotPath.c_str(), outputFileName.c_str());
				D3D9SaveScreenToFile((m_screenshotAPIType == C_SCREENSHOTAPITYPE_DIRECTX ? g_pRBRIDirect3DDevice9 : nullptr), g_hRBRWnd, m_screenshotCroppingRect, outputFileName);
			}

			// If this RBR is running under RSF plugin then generate RSF compatible 4:3 pictures also
			if (GetRBRInstallType() == "RSF" && m_screenshotCroppingRectRSF.bottom != -1)
			{
				outputFileName = ReplacePathVariables(m_screenshotPathRSF, RBRAPI_MapCarIDToMenuIdx(m_iCustomReplayCarID), false);

				if (m_iMenuCreateOption > 0 || !fs::exists(outputFileName))
				{
					LogPrint(L"RSF screenshot: RSF_ScreenshotPath=%s -> %s", m_screenshotPathRSF.c_str(), outputFileName.c_str());
					D3D9SaveScreenToFile((m_screenshotAPIType == C_SCREENSHOTAPITYPE_DIRECTX ? g_pRBRIDirect3DDevice9 : nullptr), g_hRBRWnd, m_screenshotCroppingRectRSF, outputFileName);
				}
			}

			m_iCustomReplayScreenshotCount++;
			m_iCustomReplayState = 5;
			m_tCustomReplayStateStartTime = std::chrono::steady_clock::now();
		}
	}
	else if (m_iCustomReplayState == 5)
	{
		//std::chrono::steady_clock::time_point tCustomReplayStateNowTime = std::chrono::steady_clock::now();
		//auto iTimeElapsedSec = std::chrono::duration_cast<std::chrono::milliseconds>(tCustomReplayStateNowTime - m_tCustomReplayStateStartTime).count();
		//if (m_iCustomReplayScreenshotCount > 1 || iTimeElapsedSec >= 3000)
		if (m_iCustomReplayScreenshotCount > 1 || (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_tCustomReplayStateStartTime).count()) >= 3000)
		{
			// Completing the screenshot state for this carID (the first image waits 3 secs before completing it, this is just to give a bit more time to see the grey highlighted screenshot rectangle area)
			m_iCustomReplayState = 6; 
		}
	}
	else if (m_iCustomReplayState == 6)
	{
		// Screenshot taken. Prepare the next car for a screenshot
		m_bCustomReplayShowCroppingRect = false;
		//g_pRBRCarInfo->stageStartCountdown = 7.0f;

		m_iCustomReplayCarID = GetNextScreenshotCarID(m_iCustomReplayCarID);

		if (m_iCustomReplayCarID >= 0)
		{
			//if (CNGPCarMenu::PrepareScreenshotReplayFile(m_iCustomReplayCarID))
			//	m_iCustomReplayState = 1;
			m_iCustomReplayState = 1;

			// Force reload the stage and the next car
			g_pRBRGameMode->gameStatus = 0x01;
			g_pRBRGameMode->gameMode = 0x03;
			g_pRBRMapSettingsEx->trackID = 0x04;
			m_pGame->StartGame(71, m_iCustomReplayCarID, IRBRGame::GOOD_WEATHER, IRBRGame::TYRE_GRAVEL_DRY, nullptr);
		}
		else
		{
			// Quit back to the game menu from racing mode
			m_iCustomReplayState = 0;
			g_pRBRGameMode->gameMode = 0x02;
			g_pRBRRaceTimePauseMenuSystem->menuStatus = 0x03;
			g_pRBRGameMode->gameStatus = 0x01;
		}
	}

	return hResult;
}


//----------------------------------------------------------------------------------------------------
// Custom RBR save replay method. Signal the replay RPL filename that NGPCarMenu plugin should create INI metadata file for this replay file
//
BOOL CNGPCarMenu::CustomRBRSaveReplay(const char* szReplayFileName)
{
	DebugPrint("CustomRBRSaveReplay. %s", szReplayFileName);
	CompleteSaveReplayProcess(std::string(szReplayFileName));
	return TRUE;
}


//----------------------------------------------------------------------------------------------------
// Custom RBR replay method. This custom replay method supports replay files for both traditional RBR tracks and RBRX/BTB tracks.
// Return: 0=Failed to load replay file, 1=OK
//
BOOL CNGPCarMenu::CustomRBRReplay(const char* szReplayFileName)
{
	BOOL bResult = TRUE;
	std::wstring sReplayFileName = _ToWString(szReplayFileName);

	if (szReplayFileName == nullptr || !fs::exists(g_pRBRPlugin->m_sRBRRootDir + "\\Replays\\" + szReplayFileName))
	{
		if(szReplayFileName != nullptr) LogPrint("ERROR. RBR replay file missing. %s", szReplayFileName);
		return FALSE;
	}

	DebugPrint("DEBUG. Loading %s replay file", szReplayFileName);

	// Customize the "Loading Replay" label text to include the RPL filename (unless the special "generate preview img" replay state is active)
	if (g_pRBRPlugin->m_iCustomReplayState > 0 || _iEqual(sReplayFileName, C_REPLAYFILENAME_SCREENSHOTW))
		g_wszCustomLoadReplayStatusText[0] = L'\0';
	else if(g_pOrigLoadReplayStatusText != nullptr)
		swprintf_s(g_wszCustomLoadReplayStatusText, COUNT_OF_ITEMS(g_wszCustomLoadReplayStatusText) - 1, L"%s %s", g_pOrigLoadReplayStatusText, sReplayFileName.c_str());
	else
		swprintf_s(g_wszCustomLoadReplayStatusText, COUNT_OF_ITEMS(g_wszCustomLoadReplayStatusText) - 1, L"%s", sReplayFileName.c_str());

	try
	{
		// If RBRRX integration is enabled and the replay INI metadata file exists then check if the RPL is for BTB track
		std::string sReplayINIFile = g_pRBRPlugin->m_sRBRRootDir + "\\Replays\\" + fs::path(szReplayFileName).replace_extension().generic_string() + ".ini";
		if (m_iMenuRBRRXOption && fs::exists(sReplayINIFile))
		{
			std::string sTextValue;

			LogPrint("Replay INI file %s exists. Checking if the replay file uses a BTB track", sReplayINIFile.c_str());

			CSimpleIni replayINIFile;
			replayINIFile.LoadFile(sReplayINIFile.c_str());

			sTextValue = replayINIFile.GetValue("Replay", "Type", "");
			_Trim(sTextValue);
			if (_iEqual(sTextValue, "btb", true))
			{
				sTextValue = replayINIFile.GetValue("Replay", "Name", "");
				_Trim(sTextValue);
				bResult = RBRRX_PrepareReplayTrack(sTextValue);
			}
			else
			{
				LogPrint("Replay is a standard RBR replay file, not the type of BTB. TYPE option in the INI file was %s", sTextValue.c_str());
			}
		}

		OnReplayStarted();
	}
	catch (const fs::filesystem_error& ex)
	{
		LogPrint("ERROR. CustomRBRReplay. File access error in %s %s", szReplayFileName, ex.what());
		bResult = FALSE;
	}
	catch (...)
	{
		LogPrint("ERROR. Exception in CustomRBRReplay");
		bResult = FALSE;
	}

	if (bResult == FALSE)
		m_bRBRReplayActive = m_bRBRRXReplayActive = FALSE;

	return bResult;
}


//----------------------------------------------------------------------------------------------------
// Complete RBRRX/BTB replay saving. Create replay INI definition files
//
//void CNGPCarMenu::CompleteSaveReplayProcess(const std::list<std::wstring>& replayFileQueue)
void CNGPCarMenu::CompleteSaveReplayProcess(const std::string& replayFileName)
{
	std::wstring fileNameWithoutExt;
	std::wstring fileNameMapIDPart;
	//bool bWriteMapName = true;
	bool bBTBReplayFile;

	std::string sReplayINIFileName;
	CSimpleIni replayINIFile;

	CSimpleIniWEx stockCarListINIFile;
	WCHAR wszStockCarINISection[8];
	
	std::string sCarModelName;
	//std::string sCarModelFolder;
	std::string sCarPhysicsFolder;

	if (!m_bGenerateReplayMetadataFile)
		return;

	try
	{
		bBTBReplayFile = (GetActiveRacingType() == 2 || GetActivePluginName() == "RX");

		// If this is RallysimFans version of RBR installation then RSF plugin modifies Cars\Cars.ini file on the fly. Re-read car model names before trying to write replay metadata.
		// Also, the actual mapID is part of the replay save filename (xxxx_rsf_xxxx_157.rpl)
		//if (m_bRallySimFansPluginInstalled)
		//	InitCarSpecData_RBRCIT();

		// Read the current path of the car model (the folder below rbrAppPath\cars folder)
		stockCarListINIFile.LoadFile((m_sRBRRootDirW + L"\\cars\\Cars.ini").c_str());
		swprintf_s(wszStockCarINISection, COUNT_OF_ITEMS(wszStockCarINISection), L"Car0%d", m_latestCarID);

/*
		sCarModelFolder = _ToString(stockCarListINIFile.GetValueEx(wszStockCarINISection, L"", L"FileName", L""));
		sCarModelFolder = fs::path(sCarModelFolder).remove_filename().generic_string();
		sCarModelFolder = _RemoveEnclosingChar(sCarModelFolder, '/', FALSE);
		sCarModelFolder = _RemoveEnclosingChar(sCarModelFolder, '\\', FALSE);
*/

		//fileNameWithoutExt = fs::path(fileNameItem).replace_extension().generic_wstring();
		fileNameWithoutExt = _ToWString(replayFileName);
		sReplayINIFileName = m_sRBRRootDir + "\\Replays\\" + _ToString(fileNameWithoutExt) + ".ini";

		DebugPrint("CompleteSaveReplayProcess. RPL=%s  INI=%s", replayFileName.c_str(), sReplayINIFileName.c_str());

		// Re-create the replay metadata INI file in case the old file has some garbage left overs from an old version
		std::ofstream recreatedFile(sReplayINIFileName, std::ios::out | std::ios::trunc);

		if(bBTBReplayFile)
			recreatedFile << "; RBR replay metadata file generated by NGPCarMenu plugin. TYPE and NAME options are required in BTB replays. Other options are just for informative purposes" << std::endl;
		else
			recreatedFile << "; RBR replay metadata file generated by NGPCarMenu plugin. All options are just for informative purposes" << (m_bRBRTMPluginActive ? " in RBRTM replays" : "") << std::endl;

		recreatedFile.close();

		replayINIFile.LoadFile(sReplayINIFileName.c_str());
		replayINIFile.SetValue("Replay", "Type", (bBTBReplayFile ? "BTB" : (m_bRBRTMPluginActive ? "TM" : (m_bRallySimFansPluginInstalled ? "RSF" : "RBR"))));
			
		if (bBTBReplayFile)
		{
			replayINIFile.SetValue("Replay", "Name",        m_latestMapRBRRX.name.c_str());
			replayINIFile.SetValue("Replay", "TrackFolder", m_latestMapRBRRX.folderName.c_str());
			//bWriteMapName = FALSE;
		}
		else
		{
			std::wstring wsStageName = GetMapNameByMapID(m_latestMapID);
			if (!wsStageName.empty()) replayINIFile.SetValue("Replay", "Name", _ToString(wsStageName).c_str());
		}

/*
		if (m_bRallySimFansPluginInstalled)
		{
			// Hotlap and Online rallies in RSF replay file has the map name in the filename already (and the RBR runtime mapID is often just a stub placeholer)
			bWriteMapName = false;

			if (fileNameWithoutExt.find(L"_rsf_practice_") != std::wstring::npos)
			{
				// Take the actual RSF mapID from the end of the practice replay filename because sometimes RSF re-uses existing RBR mapIDs (uses existing map slots to load RFS specific custom map)
				size_t iLastDashPos = fileNameWithoutExt.find_last_of(L'_');
				if (iLastDashPos != std::wstring::npos && iLastDashPos + 1 < fileNameWithoutExt.length())
				{
					fileNameMapIDPart = fileNameWithoutExt.substr(iLastDashPos + 1);
					if (_IsAllDigit(fileNameMapIDPart))
					{
						m_latestMapID = std::stoi(fileNameMapIDPart);
						bWriteMapName = true;
					}
				}
			}
		}

		if (bWriteMapName)
		{
			std::wstring wsStageName = GetMapNameByMapID(m_latestMapID);
			if (!wsStageName.empty()) replayINIFile.SetValue("Replay", "Name", _ToString(wsStageName).c_str());
		}
*/

		replayINIFile.SetValue("Replay", "MapID", std::to_string(m_latestMapID).c_str());

		if (m_latestCarID >= 0 && m_latestCarID <= 7)
		{
			DWORD ptrValue;

			if (m_bRallySimFansPluginInstalled)
			{
				// RSF version of the car physics folder (the folder below rsfdata\cars folder)
				sCarPhysicsFolder = _ToString(stockCarListINIFile.GetValueEx(wszStockCarINISection, L"", L"RSFCarPhysics", L""));
				sCarPhysicsFolder = _RemoveEnclosingChar(sCarPhysicsFolder, '/', FALSE);
				sCarPhysicsFolder = _RemoveEnclosingChar(sCarPhysicsFolder, '\\', FALSE);
			}

			// If physicsFolder is still empty at this point then use the NGP filename from Physics\<carSubFolder> folder as physics folder name (non-RSF installations)
			if (sCarPhysicsFolder.empty())
				sCarPhysicsFolder = _ToString(g_RBRCarSelectionMenuEntry[RBRAPI_MapCarIDToMenuIdx(m_latestCarID)].wszCarPhysics);

			ptrValue = *((DWORD*)g_RBRCarSelectionMenuEntry[RBRAPI_MapCarIDToMenuIdx(m_latestCarID)].ptrCarDescription);
			if (/*!m_bRallySimFansPluginInstalled &&*/ ptrValue != 0 && ((const char*)ptrValue)[0] != '\0')
				//replayINIFile.SetValue("Replay", "CarModel", (const char*)ptrValue);
				sCarModelName = (const char*)ptrValue;
			else
				// If the RBR carMenuDescription is missing then take the CarModel directly from the local data structure (well, usually it is the same as ptrCarDescription name)
				sCarModelName = _ToString(g_RBRCarSelectionMenuEntry[RBRAPI_MapCarIDToMenuIdx(m_latestCarID)].wszCarModel);

			replayINIFile.SetValue("Replay", "CarModel", sCarModelName.c_str());
			replayINIFile.SetLongValue("Replay", "CarSlot", m_latestCarID);

			// Write out finish time (if the rally was completed)
			if (g_pRBRCarInfo != nullptr && g_pRBRCarInfo->raceTime != 0.0f && (g_pRBRCarInfo->finishLinePassed > 0 || g_pRBRCarInfo->distanceToFinish <= 0.0f))
				replayINIFile.SetValue("Replay", "FinishTimeSecs", std::to_string(g_pRBRCarInfo->raceTime).c_str());

			replayINIFile.SetValue("Replay", "CarModelFolder", _ToString(g_RBRCarSelectionMenuEntry[RBRAPI_MapCarIDToMenuIdx(m_latestCarID)].wszCarModelFolder).c_str() /*sCarModelFolder.c_str()*/ );
			replayINIFile.SetValue("Replay", "CarPhysicsFolder", sCarPhysicsFolder.c_str());
		}

		replayINIFile.SetValue("Replay", "NGP", (
			std::to_string(m_iPhysicsNGMajorVer) + "." +
			std::to_string(m_iPhysicsNGMinorVer) + "." +
			std::to_string(m_iPhysicsNGPatchVer) + "." +
			std::to_string(m_iPhysicsNGBuildVer) ).c_str());


		if (m_bRBRProInstalled)
		{
			// RBRPro manager customization. Add skin and lights metadata tags
			std::string rbrProListNumber;
			std::string sTextValue;
			std::string sCarSlot;
			CSimpleIniEx rbrProCarListsIniFile;

			replayINIFile.SetValue("Replay", "RBRPro", m_sRBRProVersion.c_str());
				
			rbrProCarListsIniFile.SetUnicode(TRUE);
			rbrProCarListsIniFile.LoadFileEx((m_sRBRRootDir + "\\..\\CarLists.ini").c_str());				

			rbrProListNumber = rbrProCarListsIniFile.GetValueEx("RBRProManager", "", "ListNumber", "");			
			if (!rbrProListNumber.empty())
			{
				sCarSlot = std::to_string(m_latestCarID);

				sTextValue = rbrProCarListsIniFile.GetValueEx(rbrProListNumber, "", "CarSkin" + sCarSlot, "");
				if (!sTextValue.empty()) replayINIFile.SetValue("Replay", "RBRProCarSkin", sTextValue.c_str());

				sTextValue = rbrProCarListsIniFile.GetValueEx(rbrProListNumber, "", "CarLightsOption" + sCarSlot, "");
				if (!sTextValue.empty()) replayINIFile.SetValue("Replay", "RBRProCarLightsOption", sTextValue.c_str());
			}
		}

		if (m_bRallySimFansPluginInstalled)
		{
			replayINIFile.SetValue("Replay", "RSF", m_sRSFVersion.c_str());
			//replayINIFile.SetValue("Replay", "RSFCarPhysics", sCarPhysicsFolder.c_str());
			replayINIFile.SetLongValue("Replay", "RSFCarID", GetCarIDFromRSFFile(nullptr, sCarModelName));
		}

		replayINIFile.SaveFile(sReplayINIFileName.c_str());
		replayINIFile.Reset();
	}
	catch (...)
	{
		replayINIFile.Reset();
	}
}


//--------------------------------------------------------------------------------------------
// If call for help was called then set the tick when it was called (increases the callForHelp counter on the next penalty)
//
void CNGPCarMenu::CustomRBRCallForHelp()
{
	m_latestCallForHelpTick = GetTickCount32();
}


//--------------------------------------------------------------------------------------------
// D3D9 BeginScene callback handler. 
// If current menu is "SelectCar" then show custom car details or if custom replay video is playing then generate preview images.
//
HRESULT __fastcall CustomRBRDirectXBeginScene(void* objPointer)
{
	if (!g_bRBRHooksInitialized) 
		return S_OK;

	// Call the origial RBR BeginScene and let it to initialize the new D3D scene
	HRESULT hResult = ::Func_OrigRBRDirectXBeginScene(objPointer);

	// If racing is active and not custom rallyOptions enabled then no need to do any custom things
	if (g_pRBRGameMode->gameMode == 0x01)
	{
		if (g_pRBRPlugin->m_dwRBRCustomRallyOptions != 0)
		{
			// Set custom "force disable" rallyOptions (pacenote plugin may be used to enabel these at raceTime, so do the force disable always during racing)
			if ((g_pRBRPlugin->m_dwRBRCustomRallyOptions & RALLYOPTION_FORCEDISABLE_3DPACENOTES))
				g_pRBRGameConfigEx->show3DPacenotes = 0;

			if ((g_pRBRPlugin->m_dwRBRCustomRallyOptions & RALLYOPTION_FORCEDISABLE_PACENOTEDISTANCECOUNTDOWN))
				g_pRBRGameConfigEx->showPacenoteDistanceCountdown = 0;

			if ((g_pRBRPlugin->m_dwRBRCustomRallyOptions & RALLYOPTION_FORCEDISABLE_PACENOTESYMBOLS))
				g_pRBRGameConfigEx->showPacenotes = g_pRBRGameConfigEx->show3DPacenotes = g_pRBRGameConfigEx->pacenoteStack = 0;
		}
		return hResult;
	}

	if (g_pRBRGameMode->gameMode == 0x03 || g_pRBRPlugin->m_iCustomReplayState > 0)
	{
		g_pRBRPlugin->CustomRBRDirectXBeginScene();
	}
	else if (g_pRBRGameMode->gameMode == 0x05)
	{
		// If status is 0x05 (loading a track) and "RacingActive" status is not set already and a replay is not active then the track is loaded for racing. Call OnRaceStarted handler
		if (!g_pRBRPlugin->m_bRBRRacingActive && g_pRBRPlugin->GetActiveReplayType() == 0)
			g_pRBRPlugin->OnRaceStarted();

		else if (g_pOrigLoadReplayStatusText == nullptr && g_pRBRStatusText->wszLoadReplayTitleName != nullptr && g_pRBRPlugin->GetActiveReplayType() > 0)
		{
			std::wstring sTmpText = g_wszCustomLoadReplayStatusText;

			// First time initialization of custom "Load Replay" text label. At this point g_wszCustomLoadReplayStatusText had only RPL filename, so insert the localized "Loading Replay" text
			g_pOrigLoadReplayStatusText = g_pRBRStatusText->wszLoadReplayTitleName;
			swprintf_s(g_wszCustomLoadReplayStatusText, COUNT_OF_ITEMS(g_wszCustomLoadReplayStatusText) - 1, L"%s %s", g_pOrigLoadReplayStatusText, sTmpText.c_str());

			WriteOpCodePtr((LPVOID)&g_pRBRStatusText->wszLoadReplayTitleName, g_wszCustomLoadReplayStatusText);
		}
	}

	return hResult;
}


//----------------------------------------------------------------------------------------------------------------------------
// D3D9 EndScene callback handler. 
//
HRESULT __fastcall CustomRBRDirectXEndScene(void* objPointer)
{
	if (!g_bRBRHooksInitialized)
		return S_OK;

#if USE_DEBUG == 1
	static int iPrevMode = -1;
	wchar_t szTxtBuf[512];
	swprintf_s(szTxtBuf, COUNT_OF_ITEMS(szTxtBuf) - 1, L"Mode=%d RaceType=%d", g_pRBRGameMode->gameMode, g_pRBRPlugin->GetActiveRacingType());
	//g_pFontDebug->DrawText(5, 0 * 15, C_DEBUGTEXT_COLOR, szTxtBuf, D3DFONT_CLEARTARGET);

	if (iPrevMode != g_pRBRGameMode->gameMode)
	{
		iPrevMode = g_pRBRGameMode->gameMode;
		DebugPrint(szTxtBuf);
	}
	
	//swprintf_s(szTxtBuf, COUNT_OF_ITEMS(szTxtBuf) - 1, L"CurMenu=%08x  CustPlugin=%08x", (DWORD)g_pRBRMenuSystem->currentMenuObj, (DWORD)g_pRBRPluginMenuSystem->customPluginMenuObj );
	//swprintf_s(szTxtBuf, COUNT_OF_ITEMS(szTxtBuf) - 1, L"Mode=%d", g_pRBRGameMode->gameMode);
	//g_pFontDebug->DrawText(5, 1 * 15, C_DEBUGTEXT_COLOR, szTxtBuf, D3DFONT_CLEARTARGET);
#endif

	// Do RBRTM/RBRRX/RBR/RallySimFans menu and replay things only when racing is not active
	//if (g_pRBRPlugin->m_iCustomReplayState > 0 
	//|| (g_pRBRGameMode->gameMode != 01 && !((g_pRBRGameMode->gameMode == 0x0A || (g_pRBRGameMode->gameMode == 0x02 && g_watcherNewReplayFiles.Running()))))
	//)
	if (g_pRBRPlugin->GetActiveRacingType() > 0)
	{
		float timeDelta = g_pRBRCarInfo->raceTime - g_pRBRPlugin->m_prevRaceTimeClock;
		if (timeDelta >= 1.0f)
		{
			if (g_pRBRCarInfo->falseStart && g_pRBRCarInfo->stageStartCountdown > -1.0f && g_pRBRPlugin->m_latestFalseStartPenaltyTime == 0.0f)
				g_pRBRPlugin->m_latestFalseStartPenaltyTime = timeDelta;
			else
			{
				g_pRBRPlugin->m_latestOtherPenaltyTime += timeDelta;

				if (g_pRBRPlugin->m_latestCallForHelpTick != 0 && timeDelta >= 20.0f && (GetTickCount32() - g_pRBRPlugin->m_latestCallForHelpTick) < 3000)
				{
					g_pRBRPlugin->m_latestCallForHelpTick = 0;
					g_pRBRPlugin->m_latestCallForHelpCount++;
				}
			}
		}

		g_pRBRPlugin->m_prevRaceTimeClock = g_pRBRCarInfo->raceTime;
	}


#if USE_DEBUG == 1
	// DEBUG
	static DWORD dwResetTick = 0;
	//if (g_pRBRGameMode->gameMode == 0x01)
	if (false && (g_pRBRGameMode->gameMode == 0x03 || g_pRBRGameMode->gameMode == 0x01))
	{
		if (dwResetTick == 0) dwResetTick = GetTickCount32();
		else if (GetTickCount32() - dwResetTick > 10000)
		{
			DebugPrint("DEBUG: ForceQuitRally");
			//g_pRBRGameMode->gameMode = 0x0C;
			g_pRBRGameMode->gameMode = 0x03;
			g_pRBRGameMode->trackID = 0x04;
			g_pRBRGameMode->gameStatus = 0x01;
			g_pRBRPlugin->m_pGame->StartGame(4, 0 + rand() % 7, IRBRGame::GOOD_WEATHER, IRBRGame::TYRE_TARMAC_DRY, nullptr);
			//dwResetTick = GetTickCount32();
			dwResetTick = 0;
		}
	}
#endif
/*
	else if (g_pRBRGameMode->gameMode == 0x03)
	{
		if (dwResetTick != 0 && GetTickCount32() - dwResetTick > 10000)
		{
			DebugPrint("DEBUG: StartRally");
			// TODO: Set 0x7c36a8+0x10=0x04
			g_pRBRPlugin->m_pGame->StartGame(4, 5, IRBRGame::GOOD_WEATHER, IRBRGame::TYRE_GRAVEL_DRY, nullptr);
			dwResetTick = 0;
		}
	}
*/

	if (g_pRBRGameMode->gameMode != 01 || g_pRBRPlugin->m_iCustomReplayState > 0)
		return g_pRBRPlugin->CustomRBRDirectXEndScene(objPointer);
	else
		return ::Func_OrigRBRDirectXEndScene(objPointer); // Racing is active. No need to do any NGPCarMenu special things
}


//----------------------------------------------------------------------------------------------------------------------------
// RBR replay callback handlers
//
int __fastcall CustomRBRReplay(void* objPointer, DWORD /*ignoreEDX*/, const char* szReplayFileName, __int32* pUnknown1, __int32* pUnknown2, size_t iReplayFileSize)
{
	if (!g_bRBRHooksInitialized || !g_pRBRPlugin->CustomRBRReplay(szReplayFileName))
		return 0; // ERROR 

	return Func_OrigRBRReplay(objPointer, szReplayFileName, pUnknown1, pUnknown2, iReplayFileSize);
}

int __fastcall CustomRBRSaveReplay(void* objPointer, DWORD /*ignoreEDX*/, const char* szReplayFileName, __int32 mapID, __int32 carID, __int32 unknown1)
{
	if (!g_bRBRHooksInitialized || !g_pRBRPlugin->CustomRBRSaveReplay(szReplayFileName /*, mapID, carID, unknown1*/))
		return 0; // ERROR 

	return Func_OrigRBRSaveReplay(objPointer, szReplayFileName, mapID, carID, unknown1);
}


//----------------------------------------------------------------------------------------------------------------------------
// RBR controller axis data handler. Workaround the RBR bug with inverted pedals (ie. throttle goes to 100% at starting line until user presses the inverted pedal at least once)
//
#define RBRAXIS_DEADZONE(axisValue, deadZoneValue) ((axisValue) - (deadZoneValue) <= 0.0f ? 0.0f : RANGE_REMAP((axisValue) - (deadZoneValue), 0.0f, 1.0f - (deadZoneValue), 0.0f, 1.0f))
#define RBRAXIS_DEADZONE_WIDERANGE(axisValue, deadZoneValue) ((axisValue) - ((deadZoneValue)*2.0f) <= -1.0f ? -1.0f : RANGE_REMAP(((axisValue)+1.0f) - ((deadZoneValue)*2.0f), 0.0f, 2.0f - ((deadZoneValue)*2.0f), 0.0f, 2.0f) - 1.0f)
#define RBRAXIS_DEADZONE_WIDERANGE_INVERTED(axisValue, deadZoneValue) ((axisValue) + ((deadZoneValue)*2.0f) >= 1.0f ? 1.0f : (2.0f - RANGE_REMAP((2.0f - ((axisValue)+1.0f)) - ((deadZoneValue)*2.0f), 0.0f, 2.0f - ((deadZoneValue)*2.0f), 0.0f, 2.0f)) - 1.0f)

#define RBRAXIS_DEADZONE_POSITIVE(axisValue, deadZoneValue) ((axisValue) - (deadZoneValue) <= 0.0f ? 0.0f : RANGE_REMAP((axisValue) - (deadZoneValue), 0.0f, 1.0f - (deadZoneValue), 0.0f, 1.0f))
#define RBRAXIS_DEADZONE_NEGATIVE(axisValue, deadZoneValue) ((axisValue) + (deadZoneValue) >= 0.0f ? 0.0f : -RANGE_REMAP(fabs(axisValue) - (deadZoneValue), 0.0f, 1.0f - (deadZoneValue), 0.0f, 1.0f))

float __fastcall CustomRBRControllerAxisData(void* objPointer, DWORD /*ignoreEDX*/, __int32 axisID)
{
	const static int invertedPedalBitFlag[] = 
	{   0,0,0, 
		0x01,    // idx 3 = throttle bit1
		0, 
		0x02,    // idx 5 = brake bit2
		0x08,    // idx 6 = handbrake bit4
		0,0,0,0,
		0x04     // idx 11 = clutch bit3
	};

#if USE_DEBUG == 1
	float prevValue = -1.0f;
	float newValue = -1.0f;
#endif

	switch (axisID)
	{
	case 0: // Steering
/*
		if (((PRBRControllerAxis)objPointer)[0].controllerAxisData != nullptr)
		{ 
			if (g_fControllerAxisDeadzone[0] > 0.0f)
			{
				if(((PRBRControllerAxis)objPointer)[0].controllerAxisData->axisValue < 0.0f)
					((PRBRControllerAxis)objPointer)[0].controllerAxisData->axisValue = RBRAXIS_DEADZONE_NEGATIVE(((PRBRControllerAxis)objPointer)[0].controllerAxisData->axisValue, g_fControllerAxisDeadzone[0]);
				else
					((PRBRControllerAxis)objPointer)[0].controllerAxisData->axisValue = RBRAXIS_DEADZONE_POSITIVE(((PRBRControllerAxis)objPointer)[0].controllerAxisData->axisValue, g_fControllerAxisDeadzone[0]);
			}
		}
*/
		return Func_OrigRBRControllerAxisData(objPointer, 0);

	case 3:  // Throttle
	case 5:  // Brake
		if (((PRBRControllerAxis)objPointer)[axisID].controllerAxisData != nullptr)
		{
			if ( (g_iInvertedPedalsStartupFixFlag & invertedPedalBitFlag[axisID]) && ((PRBRControllerAxis)objPointer)[axisID].controllerAxisData->dinputStatus != 0 )
				return 1.0f;// Inverted pedal bugfix at starting line when the pedal is not yet pressed at all.  @0x795e14 max value. Could it be something else than 1.0f?

/*
#if USE_DEBUG == 1
			prevValue = ((PRBRControllerAxis)objPointer)[axisID].controllerAxisData->axisValue;
#endif

			if (g_fControllerAxisDeadzone[axisID] > 0.0f)
			{
				if(g_iXInputSplitThrottleBrakeAxis >= 0)
					// Splitted trigger throttle/brake with 0..1 range
					((PRBRControllerAxis)objPointer)[axisID].controllerAxisData->axisValue = RBRAXIS_DEADZONE(((PRBRControllerAxis)objPointer)[axisID].controllerAxisData->axisValue, g_fControllerAxisDeadzone[axisID]);
				else
					// Normal analog axis with -1..1 range
					((PRBRControllerAxis)objPointer)[axisID].controllerAxisData->axisValue = RBRAXIS_DEADZONE_WIDERANGE(((PRBRControllerAxis)objPointer)[axisID].controllerAxisData->axisValue, g_fControllerAxisDeadzone[axisID]);

				//((PRBRControllerAxis)objPointer)[axisID].controllerAxisData->axisRawValue2 = 0;
				//((PRBRControllerAxis)objPointer)[axisID].controllerAxisData->dinputStatus = 2;
			}

#if USE_DEBUG == 1
			if (axisID == 5 && prevValue != 0.0f && ((PRBRControllerAxis)objPointer)[axisID].controllerAxisData->axisValue != 0.0f)
			{
				DebugPrint("axisID=%d  Before=%f  Value=%f  Raw=%x  DInputStatus=%d  Value2=%f  Raw2=%x",
					axisID, prevValue,
					((PRBRControllerAxis)objPointer)[axisID].controllerAxisData->axisValue,
					((PRBRControllerAxis)objPointer)[axisID].controllerAxisData->axisRawValue,
					((PRBRControllerAxis)objPointer)[axisID].controllerAxisData->dinputStatus,
					((PRBRControllerAxis)objPointer)[axisID].controllerAxisData->axisValue2,
					((PRBRControllerAxis)objPointer)[axisID].controllerAxisData->axisRawValue2

				);
			}
#endif
*/
		}
		return Func_OrigRBRControllerAxisData(objPointer, axisID);

	case 6:  // Handbrake
	case 11: // Clutch
		if (((PRBRControllerAxis)objPointer)[axisID].controllerAxisData != nullptr)
		{
			if ((g_iInvertedPedalsStartupFixFlag & invertedPedalBitFlag[axisID]) && ((PRBRControllerAxis)objPointer)[axisID].controllerAxisData->dinputStatus != 0)
				return 1.0f;// Inverted pedal bugfix at starting line when the pedal is not yet pressed at all.  @0x795e14 max value. Could it be something else than 1.0f?

/*
			if (g_fControllerAxisDeadzone[axisID] > 0.0f)
				((PRBRControllerAxis)objPointer)[axisID].controllerAxisData->axisValue = RBRAXIS_DEADZONE_WIDERANGE(((PRBRControllerAxis)objPointer)[axisID].controllerAxisData->axisValue, g_fControllerAxisDeadzone[axisID]);
*/
		}
		return Func_OrigRBRControllerAxisData(objPointer, axisID);

	case 4: // Combined Throttle+Brake if the source is xbox/xinput controller. Workaround the issue and handle those xinput triggers as separate throttle and brake
		if(g_iXInputSplitThrottleBrakeAxis >= 0 && ((PRBRControllerAxis)objPointer)[4].controllerAxisData != nullptr)
		{ 
			// Clear combinedThrottleBrake activity and then split throttle and brake values to a dedicated throttle and brake controls
			((PRBRControllerAxis)objPointer)[4].controllerAxisData->axisValue = 0.0f;
			((PRBRControllerAxis)objPointer)[4].controllerAxisData->axisRawValue = 0;

			XINPUT_STATE state;
			ZeroMemory(&state, sizeof(XINPUT_STATE));
			XInputGetState(g_iXInputSplitThrottleBrakeAxis, &state);

			if (((PRBRControllerAxis)objPointer)[3].controllerAxisData != nullptr)
			{
				// Throttle
				((PRBRControllerAxis)objPointer)[3].controllerAxisData->axisValue = RANGE_REMAP(static_cast<float>((g_iXInputThrottle == 0 ? state.Gamepad.bLeftTrigger : state.Gamepad.bRightTrigger)), 0.0f, 255.0f, 0.0f, 1.0f);
				((PRBRControllerAxis)objPointer)[3].controllerAxisData->axisRawValue = 0;
			}

			if (((PRBRControllerAxis)objPointer)[5].controllerAxisData != nullptr)
			{
				// Brake
				((PRBRControllerAxis)objPointer)[5].controllerAxisData->axisValue = RANGE_REMAP(static_cast<float>((g_iXInputBrake == 0 ? state.Gamepad.bLeftTrigger : state.Gamepad.bRightTrigger)), 0.0f, 255.0f, 0.0f, 1.0f);
				((PRBRControllerAxis)objPointer)[5].controllerAxisData->axisRawValue = 0;
			}			
		}
		return Func_OrigRBRControllerAxisData(objPointer, 4);

	default:
		return Func_OrigRBRControllerAxisData(objPointer, axisID);
	}
}


//----------------------------------------------------------------------------------------------------------------------------
// RBR CallForHelp
//
void __fastcall CustomRBRCallForHelp(void* objPointer, DWORD /*ignoreEDX*/)
{
	if (!g_bRBRHooksInitialized)
		return;
	
	g_pRBRPlugin->CustomRBRCallForHelp();
	Func_OrigRBRCallForHelp(objPointer);
}

