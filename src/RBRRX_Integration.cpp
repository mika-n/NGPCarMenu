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

//#include <shlwapi.h>			// PathRemoveFileSpec

#include <filesystem>			// fs::directory_iterator
#include <fstream>				// std::ifstream
#include <sstream>				// std::stringstream
#include <algorithm>			// std::clamp

#include <locale>				// UTF8 locales

#include "NGPCarMenu.h"

namespace fs = std::filesystem;

//WCHAR g_wszCurrentBTBTrackName[256]; // The current BTB wchar track name (loadTrack or replay track name)

//------------------------------------------------------------------------------------------------
// Focus Nth RBRRX menu row (scroll the menu if necessary, ie. change the first visible stage entry in the menu list)
//
void CNGPCarMenu::FocusRBRRXNthMenuIdxRow(int menuIdx)
{
	//if (m_currentCustomMapSelectedItemIdxRBRRX == menuIdx)
	//	return;
	m_latestMapRBRRX.mapIDMenuIdx = -1;

	if (menuIdx <= 0)
	{
		// Activate the first row
		m_pRBRRXPlugin->pMenuData->selectedItemIdx = m_prevCustomMapSelectedItemIdxRBRRX = m_currentCustomMapSelectedItemIdxRBRRX = 0;
		m_pRBRRXPlugin->pMenuItems = &m_pCustomMapMenuRBRRX[0];
	}
	else
	{
		if (menuIdx >= m_numOfItemsCustomMapMenuRBRRX)
			// Activate the last row
			menuIdx = m_numOfItemsCustomMapMenuRBRRX-1;

		// Activate Nth row
		m_currentCustomMapSelectedItemIdxRBRRX = menuIdx;

		if (m_currentCustomMapSelectedItemIdxRBRRX <= 8 || m_numOfItemsCustomMapMenuRBRRX <= (8 + 1 + 8))
		{
			// The new menuIdx row is the <=8 first rows or total num of BTB tracks is <=8+1+8, so no need to do any scrolling. Jump straight to the new menuIdx row and top menu row is the first stage
			m_pRBRRXPlugin->pMenuData->selectedItemIdx = m_currentCustomMapSelectedItemIdxRBRRX;
			m_pRBRRXPlugin->pMenuItems = &m_pCustomMapMenuRBRRX[0];
		}
		else if (m_numOfItemsCustomMapMenuRBRRX - m_currentCustomMapSelectedItemIdxRBRRX <= 8)
		{
			// The new menuIdx row is leq 8 rows from the end of the menu list, so move the focus line and show 8+1+8 last stages on the menu list
			m_pRBRRXPlugin->pMenuData->selectedItemIdx = (8 + 1 + 8) - (m_numOfItemsCustomMapMenuRBRRX - m_currentCustomMapSelectedItemIdxRBRRX);
			m_pRBRRXPlugin->pMenuItems = &m_pCustomMapMenuRBRRX[m_numOfItemsCustomMapMenuRBRRX - (8 + 1 + 8)];
		}
		else
		{
			// The new menuIdx row is "in the middle of the stage list", so focus the middle row and show the sliding window of stages on the menu list (8+1+8 rows visible at any time)
			m_pRBRRXPlugin->pMenuData->selectedItemIdx = 8;
			m_pRBRRXPlugin->pMenuItems = &m_pCustomMapMenuRBRRX[m_currentCustomMapSelectedItemIdxRBRRX - 8];
		}

		m_prevCustomMapSelectedItemIdxRBRRX = m_pRBRRXPlugin->pMenuData->selectedItemIdx;
	}
}


//------------------------------------------------------------------------------------------------
// Update RBRRX_MapInfo struct data up-to-date based on menuIdx or rbr track folderName
//
#define ReadAndSetDefaultINIValue(section, key, str, defaultValue) \
   { pszValue = btbTrackINIFile.GetValue(section, key, nullptr); \
   if (pszValue == nullptr) { str.clear(); btbTrackINIFile.SetValue(section, key, defaultValue); btbTrackIniModified = TRUE; } \
   else str = pszValue; }

void CNGPCarMenu::UpdateRBRRXMapInfo(int menuIdx, RBRRX_MapInfo* pRBRRXMapInfo)
{
	if (pRBRRXMapInfo == nullptr) 
		return;

	if (menuIdx < 0 || menuIdx >= m_numOfItemsCustomMapMenuRBRRX)
	{
		pRBRRXMapInfo->mapIDMenuIdx = -1;
		pRBRRXMapInfo->folderName.clear();
		return;
	}

	pRBRRXMapInfo->mapIDMenuIdx = menuIdx;
	pRBRRXMapInfo->folderName = m_pCustomMapMenuRBRRX[menuIdx].szTrackFolder;
	_ToLowerCase(pRBRRXMapInfo->folderName);

	pRBRRXMapInfo->name = m_pCustomMapMenuRBRRX[menuIdx].szTrackName;
	if (menuIdx < min((int)m_recentMapsRBRRX.size(), m_recentMapsMaxCountRBRRX))
	{
		// Shortcut menu name. Remove the "[N] " leading tag because it is not part of the stage name
		size_t iPos = pRBRRXMapInfo->name.find_first_of(']');
		if (iPos != std::string::npos && iPos <= 5)
			pRBRRXMapInfo->name = (pRBRRXMapInfo->name.length() > iPos + 2 ? pRBRRXMapInfo->name.substr(iPos + 2) : "");
	}
	
	try
	{
		const char* pszValue;
		bool  btbTrackIniModified = false;

		std::string sBtbTrackFolderPath = m_sRBRRootDir + "\\RX_CONTENT\\" + pRBRRXMapInfo->folderName;

		CSimpleIni btbPacenotesINIFile;
		btbPacenotesINIFile.LoadFile((sBtbTrackFolderPath + "\\pacenotes.ini").c_str());
		pRBRRXMapInfo->numOfPacenotes = btbPacenotesINIFile.GetLongValue("PACENOTES", "count", 0);
		btbPacenotesINIFile.Reset();

		CSimpleIni btbTrackINIFile;
		//btbTrackINIFile.SetUnicode(true);
		btbTrackINIFile.LoadFile((sBtbTrackFolderPath + "\\track.ini").c_str());
		pRBRRXMapInfo->surface = _ToWString(btbTrackINIFile.GetValue("INFO", "physics", ""));
		pRBRRXMapInfo->length  = btbTrackINIFile.GetDoubleValue("INFO", "length", -1);
	
		ReadAndSetDefaultINIValue("INFO", "author",  pRBRRXMapInfo->author, "");
		ReadAndSetDefaultINIValue("INFO", "version", pRBRRXMapInfo->version, "");
		ReadAndSetDefaultINIValue("INFO", "date",    pRBRRXMapInfo->date, "");
		ReadAndSetDefaultINIValue("INFO", "comment", pRBRRXMapInfo->comment, "");

		// Skip map image initializations if the map preview img feature is disabled (RBRRX_MapPictureRect=0)
		if (m_mapRBRRXPictureRect.bottom != -1)
		{
			std::wstring sTrackName = _ToWString(pRBRRXMapInfo->name);

			// Use custom map image path at first (set in RBRRX_MapScreenshotPath ini option). If the option or file is missing then take the stage preview image name from rx_content\tracks\mapName\track.ini file
			pRBRRXMapInfo->previewImageFile = ReplacePathVariables(m_screenshotPathMapRBRRX, -1, FALSE, -1, sTrackName.c_str(), pRBRRXMapInfo->folderName);
			if (pRBRRXMapInfo->previewImageFile.empty() || !fs::exists(pRBRRXMapInfo->previewImageFile))
			{
				// Custom image missing. Try to use the splash screen supplied by the track author (track.ini splashscreen option). If missing but trackFolder has some jpg or png files then use the first file as splashScreen
				const char* szSplashScreen = btbTrackINIFile.GetValue("INFO", "splashscreen", nullptr);			
				if(szSplashScreen != nullptr && szSplashScreen[0] != '\0')
				{ 
					pRBRRXMapInfo->previewImageFile = _ToWString(szSplashScreen);
					if (!pRBRRXMapInfo->previewImageFile.empty())
						pRBRRXMapInfo->previewImageFile = _ToWString(sBtbTrackFolderPath) + L"\\" + pRBRRXMapInfo->previewImageFile;
				}
				else
				{
					pRBRRXMapInfo->previewImageFile.clear();

					if (g_pRBRRXTrackNameListAlreadyInitialized == nullptr)
						g_pRBRRXTrackNameListAlreadyInitialized = new std::vector<std::string>();

					// If this btb folder is the first time we see it here during this RBR process run then scan for splashscreen image in btb track folder because track.ini splashscreen option doesn't specify one, yet
					if (std::find(g_pRBRRXTrackNameListAlreadyInitialized->begin(), g_pRBRRXTrackNameListAlreadyInitialized->end(), sBtbTrackFolderPath) == g_pRBRRXTrackNameListAlreadyInitialized->end())
					{
						DebugPrint("BRBRX scanning image files in %s", sBtbTrackFolderPath.c_str());

						g_pRBRRXTrackNameListAlreadyInitialized->push_back(sBtbTrackFolderPath);

						// SlashScreen option missing. If the trackFolder has an image file then use it and add it as default splashScreen value into the INI file for later use
						for (auto& dit : fs::directory_iterator(sBtbTrackFolderPath))
						{
							if (_iEqual(dit.path().extension().string(), ".jpg", true) || _iEqual(dit.path().extension().string(), ".png", true))
							{
								pRBRRXMapInfo->previewImageFile = dit.path().filename();
								break;
							}
						}

						if (szSplashScreen == nullptr || !pRBRRXMapInfo->previewImageFile.empty())
						{
							DebugPrint("BRBRX adding an empty SplashScreen option in track.ini %s", sBtbTrackFolderPath.c_str());

							// Track.ini file doesn't have splashscreen option yet. Add it now
							btbTrackINIFile.SetValue("INFO", "splashscreen", _ToString(pRBRRXMapInfo->previewImageFile).c_str());
							btbTrackIniModified = TRUE;
						}
					}
				}
			}

			if (g_iLogMsgCount < 26)
				//LogPrint(L"Custom preview image file %s for a RBRRX map %s", m_latestMapRBRRX.previewImageFile.c_str(), _ToWString(m_latestMapRBRRX.name).c_str());
				LogPrint(L"Preview image %s for the RBRRX map %s in folder %s", 
					(!pRBRRXMapInfo->previewImageFile.empty() ? pRBRRXMapInfo->previewImageFile.c_str() : L"not set in track.ini splashScreen option"), sTrackName.c_str(), _ToWString(sBtbTrackFolderPath).c_str());
		}

		// Save track.ini file because missing custom attributes were added (this is done only once when a new btb track is installed in RBR)
		if(btbTrackIniModified)
			btbTrackINIFile.SaveFile((sBtbTrackFolderPath + "\\track.ini").c_str());
	}
	catch (...)
	{
		pRBRRXMapInfo->Clear();
	}
}


//------------------------------------------------------------------------------------------------
// Update rbrrx track.ini length= option value
//
double CNGPCarMenu::UpdateRBRRXINILengthOption(const std::string& folderName, double newLengthKm)
{
	std::string sIniFileName;

	if (folderName.empty())
		return newLengthKm;

	try
	{
		if (newLengthKm < 0)
		{
			// Use pacenotes.ini file to estimate the length because RBR data structure doesn't define it or track.ini length option.
			// Calculate type22 (finish line) - type21 (start line) difference to estimate the stage length
			char szKeyName[12];
			int iPacenotesIdx;
			int iPacenoteType;
			double startDistance = -1;
			double finishDistance = -1;

			CSimpleIni btbPaceotesINIFile;
			sIniFileName = m_sRBRRootDir + "\\RX_CONTENT\\" + folderName + "\\pacenotes.ini";
			btbPaceotesINIFile.LoadFile(sIniFileName.c_str());
			iPacenotesIdx = min(btbPaceotesINIFile.GetLongValue("PACENOTES", "count", 0) - 1, 1000000);
			
			for (; iPacenotesIdx >= 0; iPacenotesIdx--)
			{
				snprintf(szKeyName, sizeof(szKeyName), "P%d", iPacenotesIdx);
				iPacenoteType = btbPaceotesINIFile.GetLongValue(szKeyName, "type", -1);

				if (iPacenoteType == 21 && startDistance < 0)
				{
					startDistance = btbPaceotesINIFile.GetDoubleValue(szKeyName, "distance", 0);
					if (startDistance >= 0 && finishDistance >= 0) break;
				}
				else if (iPacenoteType == 22 && finishDistance < 0)
				{
					finishDistance = btbPaceotesINIFile.GetDoubleValue(szKeyName, "distance", 0);
					if (startDistance >= 0 && finishDistance >= 0) break;
				}
			}

			newLengthKm = (finishDistance - startDistance) / 1000.0f;
			if (newLengthKm <= 0) newLengthKm = 1.0;
		}

		CSimpleIni btbTrackINIFile;
		std::stringstream sLengthStr;
		sLengthStr << std::setprecision(1) << std::fixed << newLengthKm;

		sIniFileName = m_sRBRRootDir + "\\RX_CONTENT\\" + folderName + "\\track.ini";
		btbTrackINIFile.LoadFile(sIniFileName.c_str());
		btbTrackINIFile.SetValue("INFO", "length", sLengthStr.str().c_str());
		btbTrackINIFile.SaveFile(sIniFileName.c_str());
	}
	catch (...)
	{
		// Do nothing
	}

	return newLengthKm;
}


//----------------------------------------------------------------------------------------------------
// RBRRX hook function types to setup BTB track loading
//
typedef void(__cdecl* tRBRRXLoadTrackSetup1)(PRBRRXMenuItem pMenuItem);
typedef void(__cdecl* tRBRRXLoadTrackSetup2)(__int32 value1, __int32 value2, __int32 value3, __int32 value4, __int32 value5, __int32 value6, __int32 value7, __int32 value8, __int32 value9);
typedef void(__cdecl* tRBRRXLoadTrackSetup3)(__int32 value1, __int32 value2);


//----------------------------------------------------------------------------------------------------
// Prepare a replay loader to load BTB track instead of tranditional RBR track in a map slot #41 (Corte D'Arbroz)
//
BOOL CNGPCarMenu::RBRRX_PrepareReplayTrack(std::string mapName)
{
	PRBRRXPlugin pTmpRBRRXPlugin;

	pTmpRBRRXPlugin = m_pRBRRXPlugin;
	if (pTmpRBRRXPlugin == nullptr)
	{
		// RBRRX custom plugin is not yet opened at all. Use a temporary plugin object until user navigates to Options/Plugins/RBR_RX menu at least once

		HMODULE hModule = nullptr;
		if (::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, "RBR_RX.DLL", &hModule))
		{
			// Get the RBR_RX base offset and check that the DLL was already loaded by RBR executable (don't accept the DLL if this LoadLibrary call was the first one)
			pTmpRBRRXPlugin = (PRBRRXPlugin)hModule;
		}
	}

	// Exit if RBRRX plugin is missing
	if (pTmpRBRRXPlugin == nullptr)
		return FALSE;

	int mapMenuIdx = FindRBRRXMenuItemIdxByMapName(pTmpRBRRXPlugin->pMenuItems, pTmpRBRRXPlugin->numOfItems, mapName);
	if (mapMenuIdx < 0)
	{
		LogPrint("WARNING. The replay file is linked to '%s' BTB track, but it is missing. Replay failed", mapName.c_str());
		return FALSE;
	}

	LogPrint("The replay file uses '%s' BTB track from %s folder", pTmpRBRRXPlugin->pMenuItems[mapMenuIdx].szTrackName, pTmpRBRRXPlugin->pMenuItems[mapMenuIdx].szTrackFolder);

	pTmpRBRRXPlugin->loadTrackID = mapMenuIdx;
	pTmpRBRRXPlugin->loadTrackStatusD8 = 0x01;
	//pTmpRBRRXPlugin->loadTrackStatusD0 = 0x01;

	// Overrides CoreDArbox menu name in rbr menu (this is shown as a replay map name in RBR)
	std::wstring swTrackName = _ToWString(std::string(pTmpRBRRXPlugin->pMenuItems[mapMenuIdx].szTrackName));
	wcsncpy_s(pTmpRBRRXPlugin->wszTrackName, min(swTrackName.length() + 1, COUNT_OF_ITEMS(pTmpRBRRXPlugin->wszTrackName)), swTrackName.c_str(), COUNT_OF_ITEMS(pTmpRBRRXPlugin->wszTrackName));
	WriteOpCodePtr((LPVOID)0x4A1123, (LPVOID)pTmpRBRRXPlugin->wszTrackName);

	pTmpRBRRXPlugin->currentPhysicsID = pTmpRBRRXPlugin->pMenuItems[mapMenuIdx].physicsID;
	//pTmpRBRRXPlugin->loadTrackStatusA4 = 0x01;

	// TrackID used for BTB tracks
	WriteOpCodeInt32((LPVOID)0x1660804, 41);

	m_bRBRRXReplayActive = TRUE;
	m_bRBRRXReplayEnding = FALSE; 

	return TRUE;
}


//----------------------------------------------------------------------------------------------------
// Load BTB track
//
void CNGPCarMenu::RBRRX_LoadTrack(int mapMenuIdx)
{
	if (m_pRBRRXPlugin == nullptr || m_pRBRRXPlugin->pMenuItems == nullptr || m_pRBRRXPlugin->numOfItems <= 0 || m_pRBRRXPlugin->numOfItems <= mapMenuIdx)
		return;

	LogPrint("Loading BTB track %s", m_pRBRRXPlugin->pMenuItems[mapMenuIdx].szTrackName);

	DebugPrint("Set load status D8 and D0");
	m_pRBRRXPlugin->loadTrackID = mapMenuIdx;
	m_pRBRRXPlugin->loadTrackStatusD8 = 0x01;
	m_pRBRRXPlugin->loadTrackStatusD0 = 0x01;

	DebugPrint("Calling RBRRXLoadTrackSetup1");
	tRBRRXLoadTrackSetup1 func_RBRRXLoadTrackSetup1 = (tRBRRXLoadTrackSetup1)((DWORD)m_pRBRRXPlugin + 0x33A80);
	func_RBRRXLoadTrackSetup1(&m_pRBRRXPlugin->pMenuItems[mapMenuIdx]);

	// Overrides CoreDArbox menu name in rbr menu (this is shown as a "Loading" map name in RBR)
	std::wstring swTrackName = _ToWString(std::string(m_pRBRRXPlugin->pMenuItems[mapMenuIdx].szTrackName));
	wcsncpy_s(m_pRBRRXPlugin->wszTrackName, min(swTrackName.length() + 1, COUNT_OF_ITEMS(m_pRBRRXPlugin->wszTrackName)), swTrackName.c_str(), COUNT_OF_ITEMS(m_pRBRRXPlugin->wszTrackName));
	WriteOpCodePtr((LPVOID)0x4A1123, (LPVOID)m_pRBRRXPlugin->wszTrackName);

	DebugPrint("Set physticsID and load status A4");
	m_pRBRRXPlugin->currentPhysicsID = m_pRBRRXPlugin->pMenuItems[mapMenuIdx].physicsID;
	m_pRBRRXPlugin->loadTrackStatusA4  = 0x01;

	DebugPrint("Calling RBRRXLoadTrackSetup2");
	tRBRRXLoadTrackSetup2 func_RBRRXLoadTrackSetup2 = (tRBRRXLoadTrackSetup2)((DWORD)m_pRBRRXPlugin + 0x13e90);
	func_RBRRXLoadTrackSetup2(0x0A, 0x00, 0x04, -1, 0, 0, 0, 0, 0x01);

	DebugPrint("Calling RBRRXLoadTrackSetup3");
	tRBRRXLoadTrackSetup3 func_RBRRXLoadTrackSetup3 = (tRBRRXLoadTrackSetup3)((DWORD)m_pRBRRXPlugin + 0x146A0);
	func_RBRRXLoadTrackSetup3(0x01, 0x04);

	DebugPrint("Writing trackID 41 to RBR memory");
	WriteOpCodeInt32((LPVOID)0x1660804, 41);

	//m_pGame->StartGame(41, 0, IRBRGame::ERBRWeatherType::GOOD_WEATHER, IRBRGame::ERBRTyreTypes::TYRE_GRAVEL_DRY, nullptr);
}


//----------------------------------------------------------------------------------------------------
// RBR_RX integration handler (DX9 EndScene)
//
void CNGPCarMenu::RBRRX_EndScene()
{
	if (g_pRBRGameMode->gameMode == 03)
	{		
		if (m_bRBRRXPluginActive)
		{
			HRESULT hResult;

			int menuIdx;
			int iFontHeight;
			int posX;
			int posY;

			//
			// RBR_RX menu
			//
			std::wstringstream sStrStream;
			sStrStream << std::fixed << std::setprecision(1);

			if (g_pRBRPluginMenuSystem->customPluginMenuObj == g_pRBRMenuSystem->currentMenuObj)
			{
				// Hide the annoying "moving background box" on RBR_RX menu screen
				g_pRBRMenuSystem->menuImageHeight = g_pRBRMenuSystem->menuImageWidth = 0;

				if (m_pRBRRXPlugin->menuID == 0)
				{
					// RBR_RX main menu

					// RBR_RX has a bug where the focus line in the RBRRX main menu is sometimes beyond the first two Race and Replay menu lines. Fix the bug.
					//if (m_pRBRRXPlugin->pMenuData->selectedItemIdx >= 2)
					//	m_pRBRRXPlugin->pMenuData->selectedItemIdx = 0;
					
					// Move the focus line always to Race menu line because the Replay menu line really doesn't do anything in RBRRX.
					// NGPCarMenu supports replaying of RBRRX/BTB replay files, but it goes through the normal RBR replay menu screen and not via RBRRX replay menu screen.
					m_pRBRRXPlugin->pMenuData->selectedItemIdx = 0;

					// Cleanup and restore the original RBRRX menu at RBRX main menu (when RBR exists then the menu needs to be original one because RBRRX tries to release that memory pointer)
					if (m_pRBRRXPlugin->pMenuItems != m_pOrigMapMenuItemsRBRRX)
					{
						m_pRBRRXPlugin->numOfItems = m_origNumOfItemsMenuItemsRBRRX;
						m_pRBRRXPlugin->pMenuItems = m_pOrigMapMenuItemsRBRRX;
						if (m_pCustomMapMenuRBRRX != nullptr)
						{
							delete[] m_pCustomMapMenuRBRRX;
							m_pCustomMapMenuRBRRX = nullptr;
						}
					}

					//if (m_pRBRRXPluginFirstTimeInitialization)
					if (!m_bRBRRXReplayActive)
					{
						if (m_dwAutoLogonEventStartTick == 0)
							m_dwAutoLogonEventStartTick = GetTickCount32();
						else if ((GetTickCount32() - m_dwAutoLogonEventStartTick) > 150)
						{
							// Navigate automatically to Race menu when RBRRX was opened for the first time and autologon was activated
							//m_pRBRRXPluginFirstTimeInitialization = FALSE;
							//m_pRBRRXPlugin->menuID = 1;
							SendMessage(g_hRBRWnd, WM_KEYDOWN, VK_RETURN, 0);
							SendMessage(g_hRBRWnd, WM_KEYUP, VK_RETURN, 0);
						}
					}
				}
				else if (m_pRBRRXPlugin->menuID == 1 && m_pRBRRXPlugin->pMenuData->selectedItemIdx >= 0 && m_pRBRRXPlugin->pMenuData->selectedItemIdx < m_pRBRRXPlugin->numOfItems)
				{
					// RBRRX tracks menu 
					//m_pRBRRXPluginFirstTimeInitialization = FALSE;

					if (m_pCustomMapMenuRBRRX == nullptr)
					{
						int numOfRecentMaps = CalculateNumOfValidMapsInRecentList(m_pOrigMapMenuItemsRBRRX, m_origNumOfItemsMenuItemsRBRRX);

						// Initialization of custom RBRTM stages menu list in Shakedown menu (Nth recent stages on top of the menu and then the original RBRTM stage list).
						// Use the custom list if there are recent mapIDs in the list and the feature is enabled (recent items max count > 0)
						m_numOfItemsCustomMapMenuRBRRX = m_origNumOfItemsMenuItemsRBRRX + numOfRecentMaps;
						m_pCustomMapMenuRBRRX = new RBRRXMenuItem[m_numOfItemsCustomMapMenuRBRRX];

						if (m_pCustomMapMenuRBRRX != nullptr /*&& numOfRecentMaps > 0 && m_recentMapsMaxCountRBRRX > 0*/)
						{
							// Clear shortcut stage items and copy the original RBRTM stages list at the end of custom menu list
							ZeroMemory(m_pCustomMapMenuRBRRX, sizeof(RBRRXMenuItem) * numOfRecentMaps);
							memcpy(&m_pCustomMapMenuRBRRX[numOfRecentMaps], m_pOrigMapMenuItemsRBRRX, m_origNumOfItemsMenuItemsRBRRX * sizeof(RBRRXMenuItem));

							m_pRBRRXPlugin->pMenuItems = m_pCustomMapMenuRBRRX;
						}

						// If there are more than 17 stages then cap the num of shown stages and scroll menu lines to show other stages
						if (m_numOfItemsCustomMapMenuRBRRX > 8 + 1 + 8)
							m_pRBRRXPlugin->numOfItems = 8 + 1 + 8;

						numOfRecentMaps = 0;
						for (auto& item : m_recentMapsRBRRX)
						{
							// At this point there are only max number of valid maps in the recent list (CalculateNumOfValidMapsInRecentList removed invalid and extra items)
							strncpy_s(m_pCustomMapMenuRBRRX[numOfRecentMaps].szTrackName, item->name.c_str(), COUNT_OF_ITEMS(m_pCustomMapMenuRBRRX[numOfRecentMaps].szTrackName));
							strncpy_s(m_pCustomMapMenuRBRRX[numOfRecentMaps].szTrackFolder, item->folderName.c_str(), COUNT_OF_ITEMS(m_pCustomMapMenuRBRRX[numOfRecentMaps].szTrackFolder));
							m_pCustomMapMenuRBRRX[numOfRecentMaps].physicsID = item->physicsID;
							numOfRecentMaps++;
						}

						// Activate the latest map menu row automatically
						if (!m_latestMapRBRRX.folderName.empty() && m_pCustomMapMenuRBRRX != nullptr)
						{
							menuIdx = -1;

							// If the latest menuIdx is set then check the array by index access before trying to search through all menu items
							if (m_latestMapRBRRX.mapIDMenuIdx >= 0 && m_latestMapRBRRX.mapIDMenuIdx < m_numOfItemsCustomMapMenuRBRRX
								&& _iEqual(m_pCustomMapMenuRBRRX[m_latestMapRBRRX.mapIDMenuIdx].szTrackFolder, m_latestMapRBRRX.folderName, true)
								)
								menuIdx = m_latestMapRBRRX.mapIDMenuIdx;
							else
								menuIdx = FindRBRRXMenuItemIdxByFolderName(m_pCustomMapMenuRBRRX, m_numOfItemsCustomMapMenuRBRRX, m_latestMapRBRRX.folderName);

							FocusRBRRXNthMenuIdxRow(menuIdx);
						}

						// Stages menu is open, save recentMaps INI options when a stage is chosen for racing and the recent list is modified
						m_bRecentMapsRBRRXModified = TRUE;
					}

					if (m_pCustomMapMenuRBRRX != nullptr)
					{
						// Check normal RBR menu navigation (key up or down) and scroll the menu list if necessary
						if (m_prevCustomMapSelectedItemIdxRBRRX != m_pRBRRXPlugin->pMenuData->selectedItemIdx)
						{
							if (m_prevCustomMapSelectedItemIdxRBRRX == 0 && m_pRBRRXPlugin->pMenuData->selectedItemIdx + 1 == min(m_numOfItemsCustomMapMenuRBRRX, 8 + 1 + 8))
								// Wrap to the last menu item
								FocusRBRRXNthMenuIdxRow(m_numOfItemsCustomMapMenuRBRRX - 1);
							else if (m_prevCustomMapSelectedItemIdxRBRRX + 1 == min(m_numOfItemsCustomMapMenuRBRRX, 8 + 1 + 8) && m_pRBRRXPlugin->pMenuData->selectedItemIdx == 0)
								// Wrap to the first menu item 
								FocusRBRRXNthMenuIdxRow(0);
							else if (m_prevCustomMapSelectedItemIdxRBRRX < m_pRBRRXPlugin->pMenuData->selectedItemIdx)
								FocusRBRRXNthMenuIdxRow(m_currentCustomMapSelectedItemIdxRBRRX + 1); // Next row (scroll if necessary)
							else if (m_prevCustomMapSelectedItemIdxRBRRX > m_pRBRRXPlugin->pMenuData->selectedItemIdx)
								FocusRBRRXNthMenuIdxRow(m_currentCustomMapSelectedItemIdxRBRRX - 1); // Prev row (scroll if necessary)
						}
						else
						{
							// Check custom pgup/pgdown/home/end navigation keys
							if (m_pRBRRXPlugin->keyCode != -1)
							{
								switch (m_pRBRRXPlugin->keyCode)
								{
								case VK_PRIOR: FocusRBRRXNthMenuIdxRow(m_currentCustomMapSelectedItemIdxRBRRX - (8 + 1 + 8)); break;	// PageUp key (move 8+1+8 rows up)
								case VK_NEXT:  FocusRBRRXNthMenuIdxRow(m_currentCustomMapSelectedItemIdxRBRRX + (8 + 1 + 8)); break;	// PageDown key (move 8+1+8 rows down)
								case VK_END:   FocusRBRRXNthMenuIdxRow(m_numOfItemsCustomMapMenuRBRRX - 1); break;	// End key
								case VK_HOME:  FocusRBRRXNthMenuIdxRow(0); break;									// Home key
								case VK_LEFT:  FocusRBRRXNthMenuIdxRow(m_currentCustomMapSelectedItemIdxRBRRX - 8); break;	// Left arrow key (move 8 rows up)
								case VK_RIGHT: FocusRBRRXNthMenuIdxRow(m_currentCustomMapSelectedItemIdxRBRRX + 8); break;	// Right arrow key (move 8 rows down)
								}

								// Don't repeat the same key until the key is released and re-pressed
								m_pRBRRXPlugin->keyCode = -1;
							}
						}

						// If the current menu row is different than the latest menu row then load new details (stage name, length, surface, previewImage)
						if (m_latestMapRBRRX.mapIDMenuIdx != m_currentCustomMapSelectedItemIdxRBRRX || m_latestMapRBRRX.trackOptionsFirstTimeSetup == FALSE)
						{
							m_latestMapRBRRX.trackOptionsFirstTimeSetup = TRUE;
							UpdateRBRRXMapInfo(m_currentCustomMapSelectedItemIdxRBRRX, &m_latestMapRBRRX);

							// Release previous map preview texture and read a new image file (if preview path is set and the image file exists and map preview img drawing is not disabled)
							SAFE_RELEASE(m_latestMapRBRRX.imageTexture.pTexture);
							if (!m_latestMapRBRRX.previewImageFile.empty() && fs::exists(m_latestMapRBRRX.previewImageFile) && m_mapRBRRXPictureRect.bottom != -1)
							{
								hResult = D3D9CreateRectangleVertexTexBufferFromFile(g_pRBRIDirect3DDevice9,
									m_latestMapRBRRX.previewImageFile,
									(float)m_mapRBRRXPictureRect.left, (float)m_mapRBRRXPictureRect.top, (float)(m_mapRBRRXPictureRect.right - m_mapRBRRXPictureRect.left), (float)(m_mapRBRRXPictureRect.bottom - m_mapRBRRXPictureRect.top),
									&m_latestMapRBRRX.imageTexture,
									0  /*IMAGE_TEXTURE_PRESERVE_ASPECTRATIO_BOTTOM | IMAGE_TEXTURE_POSITION_HORIZONTAL_CENTER*/);

								// Image not available or loading failed
								if (!SUCCEEDED(hResult))
									SAFE_RELEASE(m_latestMapRBRRX.imageTexture.pTexture);
							}
						}
					}

					// Draw right "black bar" rectangle using the menu background color and alpha channel value
					if (m_carSelectRightBlackBarRect.right != 0 && m_carSelectRightBlackBarRect.bottom != 0)
					{
						int r, g, b, a;
						if (RBRAPI_MapRBRColorToRGBA(IRBRGame::EMenuColors::MENU_BKGROUND, &r, &g, &b, &a) || g_mapRBRRXRightBlackBarVertexBuffer == nullptr)
							// MenuBackground colors changed. Re-create the potentially semi-transparent right "black" bar (color is matched with the current RBR background menu color)
							D3D9CreateRectangleVertexBuffer(g_pRBRIDirect3DDevice9, (float)m_carSelectRightBlackBarRect.left, (float)m_carSelectRightBlackBarRect.top, (float)(m_carSelectRightBlackBarRect.right - m_carSelectRightBlackBarRect.left), (float)(m_carSelectRightBlackBarRect.bottom - m_carSelectRightBlackBarRect.top), &g_mapRBRRXRightBlackBarVertexBuffer, D3DCOLOR_ARGB(a, r, g, b));

						if(g_mapRBRRXRightBlackBarVertexBuffer != nullptr)
							D3D9DrawVertex2D(g_pRBRIDirect3DDevice9, g_mapRBRRXRightBlackBarVertexBuffer);
					}

					// Show details of the current stage (name, length, surface, previewImage, author, version, date)
					// TODO: Personal track records per track per car
					iFontHeight = g_pFontCarSpecCustom->GetTextHeight();

					int iMapInfoPrintRow = 0;
					RBRAPI_MapRBRPointToScreenPoint(390.0f, 22.0f, &posX, &posY);

					g_pFontCarSpecCustom->DrawText(posX, posY + (iMapInfoPrintRow++ * iFontHeight), C_CARMODELTITLETEXT_COLOR, _ToWString(m_latestMapRBRRX.name).c_str(), 0);

					if (m_latestMapRBRRX.length > 0)
						// TODO: KM to Miles miles=km*0.621371192 config option support
						sStrStream << m_latestMapRBRRX.length << L" km";

					if (!m_latestMapRBRRX.surface.empty())
						sStrStream << (sStrStream.tellp() != std::streampos(0) ? L" " : L"") << GetLangStr(m_latestMapRBRRX.surface.c_str());

					if (m_latestMapRBRRX.numOfPacenotes >= 15)
						sStrStream << (sStrStream.tellp() != std::streampos(0) ? L" " : L"") << GetLangStr(L"pacenotes");

					//if (!m_latestMapRBRRX.comment.empty())
					//	sStrStream << (sStrStream.tellp() != std::streampos(0) ? L" " : L"") << m_latestMapRBRRX.comment.c_str();

					g_pFontCarSpecCustom->DrawText(posX, posY + (iMapInfoPrintRow++ * iFontHeight), C_CARSPECTEXT_COLOR, sStrStream.str().c_str(), 0);

					if (!m_latestMapRBRRX.comment.empty())
						g_pFontCarSpecCustom->DrawText(posX, posY + (iMapInfoPrintRow++ * iFontHeight), C_CARSPECTEXT_COLOR, m_latestMapRBRRX.comment.c_str(), 0);

					if (m_latestMapRBRRX.imageTexture.pTexture != nullptr)
					{
						m_pD3D9RenderStateCache->EnableTransparentAlphaBlending();
						D3D9DrawVertexTex2D(g_pRBRIDirect3DDevice9, m_latestMapRBRRX.imageTexture.pTexture, m_latestMapRBRRX.imageTexture.vertexes2D);
						m_pD3D9RenderStateCache->RestoreState();
					}

					// Author, version, date printed on bottom of the screen and map preview image
					iMapInfoPrintRow = 0;
					posY = m_mapRBRRXPictureRect.bottom - (1 * iFontHeight);

					sStrStream.clear();
					sStrStream.str(std::wstring());
					if (!m_latestMapRBRRX.author.empty())
						sStrStream << GetLangWString(L"author", true) << _ToWString(m_latestMapRBRRX.author);

					if (!m_latestMapRBRRX.version.empty())
						sStrStream << (sStrStream.tellp() != std::streampos(0) ? L" " : L"") << GetLangWString(L"version", true) << _ToWString(m_latestMapRBRRX.version);

					if (!m_latestMapRBRRX.date.empty())
						sStrStream << (sStrStream.tellp() != std::streampos(0) ? L" " : L"") << _ToWString(m_latestMapRBRRX.date);

					iFontHeight = g_pFontCarSpecModel->GetTextHeight();
					g_pFontCarSpecModel->DrawText(posX, posY + (iMapInfoPrintRow-- * iFontHeight), C_CARSPECTEXT_COLOR, sStrStream.str().c_str(), (m_latestMapRBRRX.imageTexture.pTexture != nullptr ? D3DFONT_CLEARTARGET : 0));
				}
			}
			else if (g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_QUICKRALLY_WEATHER])
			{
				// RBRRX track selected (showing weather options screen, it is the same as under normal QuickRally)
				g_pRBRMenuSystem->menuImageHeight = g_pRBRMenuSystem->menuImageWidth = 0;

				if (m_latestMapRBRRX.trackOptionsFirstTimeSetup)
				{
					float posLeft, posTop, posRight, posBottom;

					m_latestMapRBRRX.trackOptionsFirstTimeSetup = FALSE;

					//size_t iLen = wcslen(g_pRBRMapLocationName);
					//wcsncpy_s(g_pRBRMapLocationName, iLen + 1, _ToWString(m_latestMapRBRRX.name).c_str(), COUNT_OF_ITEMS(g_wszCurrentBTBTrackName));
					//memcpy(m_pRBRRXPlugin->wszTrackName, _ToWString(m_latestMapRBRRX.name).c_str(), min(m_latestMapRBRRX.name.length() + 1, COUNT_OF_ITEMS(m_pRBRRXPlugin->wszTrackName)) * sizeof(WCHAR));

					// RBR_RX has a bug where shorter track name is not null-terminated, so the remaining of the previous track name may be shown as a left over if the new track name is shorter.
					// Fix the wszTrackName null-termination bug in RBRRX.
					wcsncpy_s(m_pRBRRXPlugin->wszTrackName, min(m_latestMapRBRRX.name.length()+1, COUNT_OF_ITEMS(m_pRBRRXPlugin->wszTrackName)), _ToWString(m_latestMapRBRRX.name).c_str(), COUNT_OF_ITEMS(m_pRBRRXPlugin->wszTrackName));
					//wcsncpy_s(g_pRBRMapLocationName, min(m_latestMapRBRRX.name.length()+1, COUNT_OF_ITEMS(m_pRBRRXPlugin->wszTrackName)), m_pRBRRXPlugin->wszTrackName, COUNT_OF_ITEMS(m_pRBRRXPlugin->wszTrackName));

					RBRAPI_MapRBRPointToScreenPoint(5.0f, 145.0f, &posLeft, &posTop);
					RBRAPI_MapRBRPointToScreenPoint(635.0f, 480.0f, &posRight, &posBottom);

					// Release previous map preview texture and read a new image file (if preview path is set and the image file exists and map preview img drawing is not disabled)
					SAFE_RELEASE(m_latestMapRBRRX.imageTexture.pTexture);
					if (!m_latestMapRBRRX.previewImageFile.empty() && fs::exists(m_latestMapRBRRX.previewImageFile) && m_mapRBRRXPictureRect.bottom != -1)
					{
						hResult = D3D9CreateRectangleVertexTexBufferFromFile(g_pRBRIDirect3DDevice9,
							m_latestMapRBRRX.previewImageFile,
							posLeft, posTop, posRight-posLeft, posBottom-posTop,
							&m_latestMapRBRRX.imageTexture,
							0  /*IMAGE_TEXTURE_PRESERVE_ASPECTRATIO_BOTTOM | IMAGE_TEXTURE_POSITION_HORIZONTAL_CENTER*/);

						// Image not available or loading failed
						if (!SUCCEEDED(hResult))
							SAFE_RELEASE(m_latestMapRBRRX.imageTexture.pTexture);
					}
				}

				if (m_latestMapRBRRX.imageTexture.pTexture != nullptr)
				{
					m_pD3D9RenderStateCache->EnableTransparentAlphaBlending();
					D3D9DrawVertexTex2D(g_pRBRIDirect3DDevice9, m_latestMapRBRRX.imageTexture.pTexture, m_latestMapRBRRX.imageTexture.vertexes2D);
					m_pD3D9RenderStateCache->RestoreState();
				}

				sStrStream.clear();
				sStrStream.str(std::wstring());
				if (!m_latestMapRBRRX.author.empty())
					sStrStream << GetLangWString(L"author", true) << _ToWString(m_latestMapRBRRX.author);

				if (!m_latestMapRBRRX.version.empty())
					sStrStream << (sStrStream.tellp() != std::streampos(0) ? L" " : L"") << GetLangWString(L"version", true) << _ToWString(m_latestMapRBRRX.version);

				if (!m_latestMapRBRRX.date.empty())
					sStrStream << (sStrStream.tellp() != std::streampos(0) ? L" " : L"") << _ToWString(m_latestMapRBRRX.date);

				RBRAPI_MapRBRPointToScreenPoint(5.0f, 480.0f, &posX, &posY);
				g_pFontCarSpecModel->DrawText(posX, posY - g_pFontCarSpecModel->GetTextHeight() - 4, C_CARSPECTEXT_COLOR, sStrStream.str().c_str(), (m_latestMapRBRRX.imageTexture.pTexture != nullptr ? D3DFONT_CLEARTARGET : 0));
			}
		}

		else if (/*!m_bRBRRXPluginActive &&*/ m_pRBRRXPlugin != nullptr
			&& m_pRBRRXPlugin->pMenuItems != m_pOrigMapMenuItemsRBRRX && g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_MAIN])
		{
			//
			// RBRRX is no longer active, but the custom menu is created. Clean it up and restore the original RBRRX menu obj
			//
			m_pRBRRXPlugin->pMenuData->selectedItemIdx = 0;
			m_pRBRRXPlugin->numOfItems = m_origNumOfItemsMenuItemsRBRRX;
			m_pRBRRXPlugin->pMenuItems = m_pOrigMapMenuItemsRBRRX;

			if (m_pCustomMapMenuRBRRX != nullptr)
			{
				delete[] m_pCustomMapMenuRBRRX;
				m_pCustomMapMenuRBRRX = nullptr;
			}
		}		
		
		else if (m_bRBRRXReplayActive && m_bRBRRXReplayEnding)
		{
			if(g_pRBRMenuSystem->currentMenuObj == nullptr || (g_pRBRMenuSystem->currentMenuObj == g_pRBRPluginMenuSystem->customPluginMenuObj))
			{
				//
				// RBRRX replay was active, but now menu is back to mainmenu or a custom RBRX plugin menu or blank menu (RBRRX bug). 
				// Complete the BTB replaying and jump back to RBR main menu if RBRRX bug left the game in blank menu (default behavior of RBR replaying)
				//
				m_bRBRRXReplayActive = m_bRBRRXReplayEnding = FALSE;
				g_pRBRMenuSystem->currentMenuObj = g_pRBRMenuSystem->menuObj[RBRMENUIDX_MAIN];
				g_pRBRMenuSystem->currentMenuObj2 = g_pRBRMenuSystem->menuObj[RBRMENUIDX_MAIN];
			}
		}
	}

	else if (g_pRBRGameMode->gameMode == 0x0C && m_bRBRRXReplayActive)
	{
		m_bRBRRXReplayEnding = TRUE;
	}

	else if (g_pRBRGameMode->gameMode == 0x0D /*10*/)
	{
		if (m_bRecentMapsRBRRXModified && m_bRBRRXPluginActive)
		{
			if (!m_latestMapRBRRX.name.empty())
			{
				// RBR_RX has a bug where shorter track name is not null-terminated, so the remaining of the previous track name may be shown as a left over if the new track name is shorter
				//memcpy(m_pRBRRXPlugin->wszTrackName, _ToWString(m_latestMapRBRRX.name).c_str(), min(m_latestMapRBRRX.name.length() + 1, COUNT_OF_ITEMS(m_pRBRRXPlugin->wszTrackName)) * sizeof(WCHAR) );

				if (m_latestMapRBRRX.length < 0)
				{
					// BTB track track.ini file doesn't have the Length attribute yet. Store it now when the BTB track was loaded for the first time. Round meters down to one decimal km value
					m_latestMapRBRRX.length = floor((g_pRBRCarInfo != nullptr ? g_pRBRCarInfo->distanceToFinish : 0) / 100.0f) / 10.0f;
					if (m_latestMapRBRRX.length < 1.0f)
						// For some reason the BTB track data doesn't have a valid length (some very short tracks have this issue). Use dummy value and let UpdateRBRRXINILengthOption method to decide if it can use pacenotes.ini to estimate the length
						m_latestMapRBRRX.length = -1;

					UpdateRBRRXINILengthOption(m_latestMapRBRRX.folderName, m_latestMapRBRRX.length);
				}

				// Stage loading while RBRRX plugin is active. Add the latest map (=stage) to the top of the recent list and update RX_CONTENt\Tracks\myMap\track.ini Length parameter if it is missing
				if (m_recentMapsMaxCountRBRRX > 0)
				{
					m_bRecentMapsRBRRXModified = FALSE;
					AddMapToRecentList(m_latestMapRBRRX.folderName);
					if (m_bRecentMapsRBRRXModified) SaveSettingsToRBRRXRecentMaps();
				}
			}

			m_bRecentMapsRBRRXModified = FALSE;
		}
	}
}

