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
// Note. RBR_RX/BTB plugin in RBR was created by black f./jharron. We all should thank him for making it possible in RBR.
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
// Find RBRRX menuItem
//
int CNGPCarMenu::RBRRX_FindMenuItemIdxByFolderName(PRBRRXMenuItem pMapMenuItemsRBRRX, int numOfItemsMenuItemsRBRRX, const std::string& folderName)
{
	// Find the RBRRX menu item by folderName (map identifier because BTB tracks don't have unique ID numbers). Return index to the menuItem struct or -1
	if (pMapMenuItemsRBRRX != nullptr && !folderName.empty())
	{
		std::string trackFolder;
		trackFolder.reserve(256); // Max length of folder name in RBR_RX plugin
		//_ToLowerCase(folderName);

		for (int idx = 0; idx < numOfItemsMenuItemsRBRRX; idx++)
		{
			trackFolder.assign(pMapMenuItemsRBRRX[idx].szTrackFolder);
			//DebugPrint("DEBUG: FindRBRRXMenuItemIdxByFolderName %d %s=%s", idx, trackFolder.c_str(), folderName.c_str());
			if (_iEqual(trackFolder, folderName, true))
				return idx;
		}
	}

	return -1;
}

int CNGPCarMenu::RBRRX_FindMenuItemIdxByMapName(PRBRRXMenuItem pMapMenuItemsRBRRX, int numOfItemsMenuItemsRBRRX, std::string mapName)
{
	// Find the RBRRX menu item by trackName. Return index to the menuItem struct or -1
	if (pMapMenuItemsRBRRX != nullptr && !mapName.empty())
	{
		std::string trackName;
		trackName.reserve(256); // Max length of folder name in RBR_RX plugin

		_ToLowerCase(mapName);
		for (int idx = 0; idx < numOfItemsMenuItemsRBRRX; idx++)
		{
			trackName.assign(pMapMenuItemsRBRRX[idx].szTrackName);
			if (_iEqual(trackName, mapName, true))
				return idx;
		}
	}

	return -1;
}


//----------------------------------------------------------------------------------------
// Find RBRRX track folder by mapName lookup key
//
std::string CNGPCarMenu::RBRRX_FindFolderNameByMapName(std::string mapName)
{
	std::string sResult;

	PRBRRXPlugin pTmpRBRRXPlugin;

	pTmpRBRRXPlugin = m_pRBRRXPlugin;
	if (pTmpRBRRXPlugin == nullptr)
		pTmpRBRRXPlugin = (PRBRRXPlugin)GetModuleBaseAddr("RBR_RX.DLL");

	if (pTmpRBRRXPlugin != nullptr)
	{
		PRBRRXMenuItem pRBRMenuItems = (m_pOrigMapMenuItemsRBRRX != nullptr ? m_pOrigMapMenuItemsRBRRX : pTmpRBRRXPlugin->pMenuItems);
		int mapMenuIdx = RBRRX_FindMenuItemIdxByMapName(pRBRMenuItems, (m_pOrigMapMenuItemsRBRRX != nullptr ? m_origNumOfItemsMenuItemsRBRRX : pTmpRBRRXPlugin->numOfItems), mapName);
		if (mapMenuIdx >= 0)
			sResult = pRBRMenuItems[mapMenuIdx].szTrackFolder;
	}

	return sResult;
}


//----------------------------------------------------------------------------------------
int CNGPCarMenu::RBRRX_CalculateNumOfValidMapsInRecentList(PRBRRXMenuItem pMapMenuItemsRBRRX, int numOfItemsMenuItemsRBRRX)
{
	int numOfRecentMaps = 0;
	int menuIdx;

	auto it = m_recentMapsRBRRX.begin();
	while (it != m_recentMapsRBRRX.end())
	{
		if (!(*it)->folderName.empty() && numOfRecentMaps < m_recentMapsMaxCountRBRRX)
		{
			menuIdx = RBRRX_FindMenuItemIdxByFolderName(pMapMenuItemsRBRRX, numOfItemsMenuItemsRBRRX, (*it)->folderName);
			if (menuIdx >= 0 || pMapMenuItemsRBRRX == nullptr)
			{
				// The mapID in recent list is valid. Refresh the menu entry name as "[recent idx] Stage name"
				numOfRecentMaps++;
				if (pMapMenuItemsRBRRX != nullptr)
				{
					(*it)->name = "[" + std::to_string(numOfRecentMaps) + "] ";
					(*it)->name.append(pMapMenuItemsRBRRX[menuIdx].szTrackName);
					(*it)->physicsID = pMapMenuItemsRBRRX[menuIdx].physicsID;
				}

				// Go to the next item in list iterator
				++it;
			}
			else
			{
				// The map in recent list is no longer in stages menu list. Invalidate the recent item (ie. it is not added as a shortcut to RBRTM Shakedown stages menu)
				it = m_recentMapsRBRRX.erase(it);
			}
		}
		else
		{
			// Invalid map or "too many items". Remove the item if the menu data was defined
			if (pMapMenuItemsRBRRX != nullptr)
				it = m_recentMapsRBRRX.erase(it);
			else
				++it;
		}
	}

	return numOfRecentMaps;
}

void CNGPCarMenu::RBRRX_AddMapToRecentList(std::string folderName)
{
	if (folderName.empty()) return;
	_ToLowerCase(folderName); // RecentRBRRX vector list should have lowercase folder names

	// Add map folderName to top of the recentMaps list (if already in the list then move it to top, otherwise add as a new item and remove the last item if the list is full)
	for (auto& iter = m_recentMapsRBRRX.begin(); iter != m_recentMapsRBRRX.end(); ++iter)
	{
		if (folderName.compare((*iter)->folderName) == 0)
		{
			// MapID already in the recent list. Move it to the top of the list (no need to re-add it to the list)
			if (iter != m_recentMapsRBRRX.begin())
			{
				m_recentMapsRBRRX.splice(m_recentMapsRBRRX.begin(), m_recentMapsRBRRX, iter, std::next(iter));
				m_bRecentMapsRBRRXModified = TRUE;
			}

			// Must return after moving the item to top of list because for-iterator is now invalid because the list was modified
			return;
		}
	}

	// BTB folderName is not yet in the recent list. Add it to the top of the list
	auto newItem = std::make_unique<RBRRX_MapInfo>();
	newItem->folderName = folderName;
	//DebugPrint("DEBUG: RBRRX_RecentMapX=%s", newItem->folderName.c_str());
	m_recentMapsRBRRX.push_front(std::move(newItem));
	m_bRecentMapsRBRRXModified = TRUE;
}


//------------------------------------------------------------------------------------------------
// Focus Nth RBRRX menu row (scroll the menu if necessary, ie. change the first visible stage entry in the menu list)
//
void CNGPCarMenu::RBRRX_FocusNthMenuIdxRow(int menuIdx)
{
	//if (m_currentCustomMapSelectedItemIdxRBRRX == menuIdx)
	//	return;
	//DebugPrint("FocusRBRRXNthMenuIdxRow: NumOfItemsCustomMenu=%d menuIdx=%d currentCustomMapSelectItem=%d", m_numOfItemsCustomMapMenuRBRRX, menuIdx, m_currentCustomMapSelectedItemIdxRBRRX);

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

void CNGPCarMenu::RBRRX_UpdateMapInfo(int menuIdx, RBRRX_MapInfo* pRBRRXMapInfo)
{
	int numOfItems;
	PRBRRXMenuItem pMenuItems;
	PRBRRXPlugin pTmpRBRRXPlugin = m_pRBRRXPlugin;

	if (pTmpRBRRXPlugin == nullptr)
		pTmpRBRRXPlugin = (PRBRRXPlugin)GetModuleBaseAddr("RBR_RX.DLL");

	if (pRBRRXMapInfo == nullptr || pTmpRBRRXPlugin == nullptr)
		return;

	if (m_pCustomMapMenuRBRRX != nullptr)
	{
		// Custom menu in RBRRX
		pMenuItems = m_pCustomMapMenuRBRRX;
		numOfItems = m_numOfItemsCustomMapMenuRBRRX;
	}
	else
	{
		// The original menu in RBRRX
		pMenuItems = pTmpRBRRXPlugin->pMenuItems;
		numOfItems = pTmpRBRRXPlugin->numOfItems;
	}

	if (menuIdx < 0 || menuIdx >= numOfItems)
	{
		pRBRRXMapInfo->mapIDMenuIdx = -1;
		pRBRRXMapInfo->folderName.clear();
		return;
	}

	pRBRRXMapInfo->mapIDMenuIdx = menuIdx;
	pRBRRXMapInfo->folderName = pMenuItems[menuIdx].szTrackFolder;
	_ToLowerCase(pRBRRXMapInfo->folderName);

	pRBRRXMapInfo->name = pMenuItems[menuIdx].szTrackName;

	// RecentMaps shortcuts are shown in the menu only when the num of orig BTB tracks is >=17
	if (m_origNumOfItemsMenuItemsRBRRX >= 8 + 1 + 8 && m_pCustomMapMenuRBRRX != nullptr)
	{
		if (menuIdx < min((int)m_recentMapsRBRRX.size(), m_recentMapsMaxCountRBRRX))
		{
			// Shortcut menu name. Remove the "[N] " leading tag because it is not part of the stage name
			size_t iPos = pRBRRXMapInfo->name.find_first_of(']');
			if (iPos != std::string::npos && iPos <= 5)
				pRBRRXMapInfo->name = (pRBRRXMapInfo->name.length() > iPos + 2 ? pRBRRXMapInfo->name.substr(iPos + 2) : "");
		}
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
		if (m_mapRBRRXPictureRect[0].bottom != -1 || m_mapRBRRXPictureRect[1].bottom != -1 || m_mapRBRRXPictureRect[2].bottom != -1)
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
double CNGPCarMenu::RBRRX_UpdateINILengthOption(const std::string& folderName, double newLengthKm)
{
	if (folderName.empty())
		return newLengthKm;

	try
	{
		std::string sIniFileName;

		if (newLengthKm < 0)
		{
			// Use pacenotes.ini file to estimate the length because RBR data structure doesn't define it or track.ini length option.
			// Calculate type22 (finish line) - type21 (start line) difference to estimate the stage length
			double startDistance = -1;
			double finishDistance = -1;

			sIniFileName = m_sRBRRootDir + "\\RX_CONTENT\\" + folderName + "\\pacenotes.ini";
			if(RBRRX_ReadStartSplitsFinishPacenoteDistances(sIniFileName, &startDistance, nullptr, nullptr, &finishDistance))
			{
				newLengthKm = (finishDistance - startDistance) / 1000.0f;
				if (newLengthKm <= 0) newLengthKm = 1.0;
			}
			else
				newLengthKm = 1.0;
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
// Callback handler for a custom RBRRX "Load Track" screen drawing
//
void __fastcall RBRRX_CustomLoadTrackScreen()
{	
	g_pRBRPlugin->RBRRX_CustomLoadTrackScreen();
}

void CNGPCarMenu::RBRRX_CustomLoadTrackScreen()
{
	static float progressValue = 0.0f;

	PRBRRXPlugin pTmpRBRRXPlugin = m_pRBRRXPlugin;
	if(pTmpRBRRXPlugin == nullptr)
		pTmpRBRRXPlugin = (PRBRRXPlugin)GetModuleBaseAddr("RBR_RX.DLL");

	LPDIRECT3DDEVICE9 pOutputD3DDevice = (pTmpRBRRXPlugin != nullptr ? pTmpRBRRXPlugin->pRBRRXIDirect3DDevice9 : nullptr);
	if (pOutputD3DDevice != nullptr)
	{				
		int posx = 0, posy = 0;
		int iFontHeight;
		int iPrintRow = 0;

		//DebugPrint(L"ProgressValue=%f  Loading=%s", progressValue, (!m_bRBRRXReplayActive ? _ToWString(m_latestMapRBRRX.name).c_str() : g_wszCustomLoadReplayStatusText));

		if (g_pFontRBRRXLoadTrack == nullptr)
		{
			// Font to draw RBRRX "Loading: stageName" text
			g_pFontRBRRXLoadTrack = new CD3DFont(L"Trebuchet MS", 14, 0 /*D3DFONT_BOLD*/);
			g_pFontRBRRXLoadTrack->InitDeviceObjects(pOutputD3DDevice);
			g_pFontRBRRXLoadTrack->RestoreDeviceObjects();
		}

		if (g_pFontRBRRXLoadTrackSpec == nullptr)
		{
			// Font to draw track details in RBRRX LoadTrack screen
			g_pFontRBRRXLoadTrackSpec = new CD3DFont(L"Trebuchet MS", 12, 0 /*D3DFONT_BOLD*/);
			g_pFontRBRRXLoadTrackSpec->InitDeviceObjects(pOutputD3DDevice);
			g_pFontRBRRXLoadTrackSpec->RestoreDeviceObjects();
		}

		iFontHeight = g_pFontRBRRXLoadTrack->GetTextHeight();

		D3DRECT rec = { 0, 0, g_rectRBRWndClient.right, g_rectRBRWndClient.bottom };
		pOutputD3DDevice->Clear(1, &rec, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 0, 0, 0), 0, 0);

		if (m_bRBRRXLoadingNewTrack)
		{
			// New BTB track loading. Initialize the custom LoadTrack screen and show the map and minimap (except if this track loading is for replaying then don't show those preview images)
			m_bRBRRXLoadingNewTrack = false;
			progressValue = 0.0f;

			SAFE_RELEASE(m_latestMapRBRRX.imageTextureLoadTrack.pTexture);
			if (!m_bRBRRXReplayActive && !m_latestMapRBRRX.previewImageFile.empty() && fs::exists(m_latestMapRBRRX.previewImageFile) && m_mapRBRRXPictureRect[2].bottom != -1)
			{
				HRESULT hResult = D3D9CreateRectangleVertexTexBufferFromFile(pOutputD3DDevice,
					m_latestMapRBRRX.previewImageFile,
					(float)m_mapRBRRXPictureRect[2].left, (float)m_mapRBRRXPictureRect[2].top, (float)(m_mapRBRRXPictureRect[2].right - m_mapRBRRXPictureRect[2].left), (float)(m_mapRBRRXPictureRect[2].bottom - m_mapRBRRXPictureRect[2].top),
					&m_latestMapRBRRX.imageTextureLoadTrack,
					IMAGE_TEXTURE_PRESERVE_ASPECTRATIO_TOP | IMAGE_TEXTURE_POSITION_HORIZONTAL_CENTER /*| IMAGE_TEXTURE_POSITION_VERTICAL_CENTER*/);

				// Image not available or loading failed
				if (!SUCCEEDED(hResult))
					SAFE_RELEASE(m_latestMapRBRRX.imageTextureLoadTrack.pTexture);
			}
		}

		RBRAPI_MapRBRPointToScreenPoint(50, 400, &posx, &posy);

		// Draw "loading progess bar"
		rec.x1 = posx - 10;
		rec.y1 = posy - (iFontHeight-2);
		rec.x2 = rec.x1 + (20 * 10);
		rec.y2 = rec.y1 + (iFontHeight-2);
		DrawProgressBar(rec, max(progressValue, 0.05f), pOutputD3DDevice);
		progressValue += 0.05f;

		// Draw RBRRX map preview img on LoadTrack screen
		if (m_latestMapRBRRX.imageTextureLoadTrack.pTexture != nullptr)
			D3D9DrawVertexTex2D(pOutputD3DDevice, m_latestMapRBRRX.imageTextureLoadTrack.pTexture, m_latestMapRBRRX.imageTextureLoadTrack.vertexes2D);

		// Vertical red bar and "Loading <trackName>" txt
		rec.x1 = posx - 10;
		rec.y1 = posy;
		rec.x2 = rec.x1 + 5;
		rec.y2 = rec.y1 + (iFontHeight * 2);
		pOutputD3DDevice->Clear(1, &rec, D3DCLEAR_TARGET, C_REDSEPARATORLINE_COLOR, 0, 0);

		// Show track details and preview images if this track is not loaded for a replay
		if (!m_bRBRRXReplayActive)
		{
			m_bRBRRXRacingActive = TRUE;

			g_pFontRBRRXLoadTrack->DrawText(posx, posy + (iFontHeight * iPrintRow++), C_CARSPECTEXT_COLOR, _ToWString(m_latestMapRBRRX.name).c_str(), 0);

			// Track details
			std::wstringstream sStrStream;
			sStrStream << std::fixed << std::setprecision(1);
			if (m_latestMapRBRRX.length > 0)
				// TODO: KM to Miles miles=km*0.621371192 config option support
				sStrStream << m_latestMapRBRRX.length << L" km";

			if (!m_latestMapRBRRX.surface.empty())
				sStrStream << (sStrStream.tellp() != std::streampos(0) ? L" " : L"") << GetLangStr(m_latestMapRBRRX.surface.c_str());

			if (m_latestMapRBRRX.numOfPacenotes >= 15)
				sStrStream << (sStrStream.tellp() != std::streampos(0) ? L" " : L"") << GetLangStr(L"pacenotes");

			//if (!m_latestMapRBRRX.comment.empty())
			//	sStrStream << (sStrStream.tellp() != std::streampos(0) ? L" " : L"") << _ToWString(m_latestMapRBRRX.comment);

			g_pFontRBRRXLoadTrackSpec->DrawText(posx, posy + (iFontHeight * iPrintRow++), C_CARMODELTITLETEXT_COLOR, sStrStream.str().c_str(), 0);
			g_pFontRBRRXLoadTrackSpec->DrawText(posx, posy + (iFontHeight * iPrintRow++), C_CARMODELTITLETEXT_COLOR, m_latestMapRBRRX.author.c_str(), 0);

			// Draw RBRRX minimap on LoadTrack screen
			RBRRX_DrawMinimap(m_latestMapRBRRX.folderName, 2, pOutputD3DDevice);
		}
		else
		{
			// Replay file loading BTB track. Show just "Replay fileName" text
			g_pFontRBRRXLoadTrack->DrawText(posx, posy + (iFontHeight * iPrintRow++), C_CARSPECTEXT_COLOR, g_wszCustomLoadReplayStatusText, 0);
		}

		pOutputD3DDevice->EndScene();
		pOutputD3DDevice->Present(0, 0, 0, 0);
	}
}


//----------------------------------------------------------------------------------------------------
// Link RBRRX to the custom LoadTrack handler in NGPCarMenu (see NGPCarMenu.ini RBRRX_CustomLoadTrackScreen option)
//
void CNGPCarMenu::RBRRX_OverrideLoadTrackScreen()
{
	PRBRRXPlugin pTmpRBRRXPlugin = (PRBRRXPlugin)GetModuleBaseAddr("RBR_RX.DLL");

	// Exit if RBRRX plugin is missing
	if (pTmpRBRRXPlugin == nullptr)
	{
		LogPrint("ERROR. RBR_RX.DLL plugin missing. Cannot override LoadTrack screen");
		return;
	}

	WriteOpCodeNearCallCmd((LPVOID)((DWORD)pTmpRBRRXPlugin + 0x97AC), &::RBRRX_CustomLoadTrackScreen);
	WriteOpCodeNearJmpCmd((LPVOID)((DWORD)pTmpRBRRXPlugin + 0x97B1), (LPVOID)((DWORD)pTmpRBRRXPlugin + 0x97DE));
}


//----------------------------------------------------------------------------------------------------
// Prepare a replay loader to load BTB track instead of tranditional RBR track in a map slot #41 (Corte D'Arbroz)
//
BOOL CNGPCarMenu::RBRRX_PrepareReplayTrack(const std::string& mapName)
{
	PRBRRXPlugin pTmpRBRRXPlugin;

	pTmpRBRRXPlugin = m_pRBRRXPlugin;
	if (pTmpRBRRXPlugin == nullptr)
		pTmpRBRRXPlugin = (PRBRRXPlugin)GetModuleBaseAddr("RBR_RX.DLL");

	// Exit if RBRRX plugin is missing
	if (pTmpRBRRXPlugin == nullptr)
		return FALSE;

	int mapMenuIdx = RBRRX_FindMenuItemIdxByMapName(pTmpRBRRXPlugin->pMenuItems, pTmpRBRRXPlugin->numOfItems, mapName);
	if (mapMenuIdx < 0)
	{
		LogPrint("WARNING. The replay file is linked to '%s' BTB track, but it is missing. Replay failed", mapName.c_str());
		return FALSE;
	}

	LogPrint("The replay file uses '%s' BTB track from %s folder", pTmpRBRRXPlugin->pMenuItems[mapMenuIdx].szTrackName, pTmpRBRRXPlugin->pMenuItems[mapMenuIdx].szTrackFolder);

	pTmpRBRRXPlugin->loadTrackID = mapMenuIdx;
	pTmpRBRRXPlugin->loadTrackStatusD8 = 0x01;
	pTmpRBRRXPlugin->loadTrackStatusD0 = 0x01; //??

	// Overrides CoreDArbox menu name in rbr menu (this is shown as a replay map name in RBR)
	std::wstring swTrackName = _ToWString(std::string(pTmpRBRRXPlugin->pMenuItems[mapMenuIdx].szTrackName));
	wcsncpy_s(pTmpRBRRXPlugin->wszTrackName, min(swTrackName.length() + 1, COUNT_OF_ITEMS(pTmpRBRRXPlugin->wszTrackName)), swTrackName.c_str(), COUNT_OF_ITEMS(pTmpRBRRXPlugin->wszTrackName));
	WriteOpCodePtr((LPVOID)0x4A1123, (LPVOID)pTmpRBRRXPlugin->wszTrackName);

	pTmpRBRRXPlugin->currentPhysicsID = pTmpRBRRXPlugin->pMenuItems[mapMenuIdx].physicsID;
	pTmpRBRRXPlugin->loadTrackStatusA4 = 0x01; //??

	// TrackID used for BTB tracks
	WriteOpCodeInt32((LPVOID)0x1660804, 41);

	m_bRBRRXLoadingNewTrack = TRUE;
	m_bRBRRXRacingActive = FALSE;
	m_bRBRRXReplayActive = TRUE;
	//m_bRBRRXReplayOrRacingEnding = FALSE; 

	return TRUE;
}


//----------------------------------------------------------------------------------------------------
// Load BTB track
//
BOOL CNGPCarMenu::RBRRX_PrepareLoadTrack(const std::string& mapName, std::string mapFolderName)
{
	int mapMenuIdx = RBRRX_CheckTrackLoadStatus(mapName, mapFolderName, TRUE);
	if (mapMenuIdx >= 0)
		return RBRRX_PrepareLoadTrack(mapMenuIdx);
	else
		return FALSE;
}

BOOL CNGPCarMenu::RBRRX_PrepareLoadTrack(int mapMenuIdx)
{
	PRBRRXPlugin pTmpRBRRXPlugin;

	pTmpRBRRXPlugin = m_pRBRRXPlugin;
	if (pTmpRBRRXPlugin == nullptr)
		pTmpRBRRXPlugin = (PRBRRXPlugin)GetModuleBaseAddr("RBR_RX.DLL");

	// Exit if RBRRX plugin is missing or no BTB tracks then exit
	if (pTmpRBRRXPlugin == nullptr || pTmpRBRRXPlugin->pMenuItems == nullptr || pTmpRBRRXPlugin->numOfItems <= 0 || pTmpRBRRXPlugin->numOfItems <= mapMenuIdx)
		return FALSE;

	DebugPrint("Loading BTB track %s", pTmpRBRRXPlugin->pMenuItems[mapMenuIdx].szTrackName);

	DebugPrint("Set load status D8 and D0");
	pTmpRBRRXPlugin->loadTrackID = mapMenuIdx;
	pTmpRBRRXPlugin->loadTrackStatusD8 = 0x01;
	pTmpRBRRXPlugin->loadTrackStatusD0 = 0x01;

	DebugPrint("Calling RBRRXLoadTrackSetup1");
	tRBRRXLoadTrackSetup1 func_RBRRXLoadTrackSetup1 = (tRBRRXLoadTrackSetup1)((DWORD)pTmpRBRRXPlugin + 0x33A80);
	func_RBRRXLoadTrackSetup1(&pTmpRBRRXPlugin->pMenuItems[mapMenuIdx]);

	// Overrides CoteDArbox menu name in rbr menu (this is shown as a "Loading" map name in RBR)
	std::wstring swTrackName = _ToWString(std::string(pTmpRBRRXPlugin->pMenuItems[mapMenuIdx].szTrackName));
	wcsncpy_s(pTmpRBRRXPlugin->wszTrackName, min(swTrackName.length() + 1, COUNT_OF_ITEMS(pTmpRBRRXPlugin->wszTrackName)), swTrackName.c_str(), COUNT_OF_ITEMS(pTmpRBRRXPlugin->wszTrackName));
	WriteOpCodePtr((LPVOID)0x4A1123, (LPVOID)pTmpRBRRXPlugin->wszTrackName);

	DebugPrint("Set physticsID and load status A4");
	pTmpRBRRXPlugin->currentPhysicsID = pTmpRBRRXPlugin->pMenuItems[mapMenuIdx].physicsID;
	pTmpRBRRXPlugin->loadTrackStatusA4  = 0x01;

	//DebugPrint("Calling RBRRXLoadTrackSetup2");
	//tRBRRXLoadTrackSetup2 func_RBRRXLoadTrackSetup2 = (tRBRRXLoadTrackSetup2)((DWORD)pTmpRBRRXPlugin + 0x13e90);
	//func_RBRRXLoadTrackSetup2(0x0A, 0x00, 0x04, -1, 0, 0, 0, 0, 0x01);

	//DebugPrint("Calling RBRRXLoadTrackSetup3");
	//tRBRRXLoadTrackSetup3 func_RBRRXLoadTrackSetup3 = (tRBRRXLoadTrackSetup3)((DWORD)pTmpRBRRXPlugin + 0x146A0);
	//func_RBRRXLoadTrackSetup3(0x01, 0x04);

	DebugPrint("Writing trackID 41 to RBR memory");
	WriteOpCodeInt32((LPVOID)0x1660804, 41);

	// RBR_RX has a bug where shorter track name is not null-terminated, so the remaining of the previous track name may be shown as a left over if the new track name is shorter.
	RBRRX_UpdateMapInfo(mapMenuIdx, &m_latestMapRBRRX);
	wcsncpy_s(pTmpRBRRXPlugin->wszTrackName, min(m_latestMapRBRRX.name.length() + 1, COUNT_OF_ITEMS(pTmpRBRRXPlugin->wszTrackName)), _ToWString(m_latestMapRBRRX.name).c_str(), COUNT_OF_ITEMS(pTmpRBRRXPlugin->wszTrackName));

	//m_pGame->StartGame(41, 0, IRBRGame::ERBRWeatherType::GOOD_WEATHER, IRBRGame::ERBRTyreTypes::TYRE_GRAVEL_DRY, nullptr);

	m_bRBRRXLoadingNewTrack = TRUE;
	m_bRBRRXRacingActive = TRUE;
	m_bRBRRXReplayActive = FALSE;
	//m_bRBRRXReplayOrRacingEnding = FALSE;

	return TRUE;
}


//---------------------------------------------------------------------------------------------------------------------------------
// Check the status of the loaded BTB track (if BTB loading failed then the map may be the original #41 Cote stage
//
int CNGPCarMenu::RBRRX_CheckTrackLoadStatus(const std::string& mapName, std::string mapFolderName, BOOL preLoadCheck)
{
	int mapMenuIdx = -1;
	std::string rbrxMapFolderName;
	PRBRRXPlugin pTmpRBRRXPlugin;

	pTmpRBRRXPlugin = m_pRBRRXPlugin;
	if (pTmpRBRRXPlugin == nullptr)
		pTmpRBRRXPlugin = (PRBRRXPlugin)GetModuleBaseAddr("RBR_RX.DLL");

	if (pTmpRBRRXPlugin != nullptr)
		mapMenuIdx = RBRRX_FindMenuItemIdxByMapName(pTmpRBRRXPlugin->pMenuItems, pTmpRBRRXPlugin->numOfItems, mapName);

	if (mapMenuIdx < 0)
	{
		LogPrint("WARNING. Tried to load '%s' BTB track, but the track is missing", mapName.c_str());
		return -1;
	}

	rbrxMapFolderName = m_sRBRRootDir + "\\RX_Content\\" + pTmpRBRRXPlugin->pMenuItems[mapMenuIdx].szTrackFolder;

	// Remap the mapFolderName to use fullpath in case the input parameter is not yet the fullpath value
	if (mapFolderName.length() > 3)
	{
		if (!(mapFolderName[1] == ':' && mapFolderName[2] == '\\') && !(mapFolderName[0] == '\\' && mapFolderName[1] == '\\'))
		{
			if (_iStarts_With(mapFolderName, "tracks\\", true))
				mapFolderName = m_sRBRRootDir + "\\RX_Content\\" + mapFolderName;
			else if (_iStarts_With(mapFolderName, "rx_content\\", true))
				mapFolderName = m_sRBRRootDir + mapFolderName;
			else
				mapFolderName = m_sRBRRootDir + "\\RX_Content\\Tracks\\" + mapFolderName;
		}
	}
	else
		mapFolderName = "";

	if (mapFolderName.empty() || !fs::exists(mapFolderName) || !_iEqual(rbrxMapFolderName, mapFolderName, false))
	{
		LogPrint("WARNING. Tried to load '%s' BTB track, but the folder is missing or wrong", mapName.c_str());
		return -1;
	}

	if(preLoadCheck)
		return mapMenuIdx;

	// The BTB map should have been loaded at this point. Make sure the loaded track didn't end up to the original #41 Cote track
	double startDistance = -1, split1Distance = -1, split2Distance = -1, finishDistance = -1;
	double startDistanceRBR = -1, split1DistanceRBR = -1, split2DistanceRBR = -1, finishDistanceRBR = -1;

	if (g_pRBRGameMode->gameMode == 0x01 || g_pRBRGameMode->gameMode == 0x0A || g_pRBRGameMode->gameMode == 0x0D || g_pRBRGameMode->gameMode == 0x02)
	{
		RBRAPI_InitializeRaceTimeObjReferences();

		if (RBRRX_ReadStartSplitsFinishPacenoteDistances(mapFolderName + "\\pacenotes.ini", &startDistance, &split1Distance, &split2Distance, &finishDistance)
			&& ReadStartSplitFinishPacenoteDistances(&startDistanceRBR, &split1DistanceRBR, &split2DistanceRBR, &finishDistanceRBR))
		{
			//DebugPrint("start=%lf %lf  split1=%lf %lf  split2=%lf %lf  finish=%lf %lf", startDistance, startDistanceRBR, split1Distance, split1DistanceRBR, split2Distance, split2DistanceRBR, finishDistance, finishDistanceRBR);

			if (abs(startDistance - startDistanceRBR) > 0.5 || abs(split1Distance - split1DistanceRBR) > 0.5 || abs(split2Distance - split2DistanceRBR) > 0.5 || abs(finishDistance - finishDistanceRBR) > 0.5)
				return -1;
		}
		else
			return -1;
	}
	else
		return -1;

	return mapMenuIdx;
}


//---------------------------------------------------------------------------------------------------------------------------------
// Read start/split1/split2/finish pacenotes (needed to calculate the distance)
//
BOOL CNGPCarMenu::RBRRX_ReadStartSplitsFinishPacenoteDistances(const std::string& sIniFileName, double* startDistance, double* split1Distance, double* split2Distance, double* finishDistance)
{
	char szKeyName[12];
	int iPacenotesIdx;
	int iPacenoteType;
	
	if(startDistance)  *startDistance  = -1;
	if(split1Distance) *split1Distance = -1;
	if(split2Distance) *split2Distance = -1;
	if(finishDistance) *finishDistance = -1;

	try
	{
		CSimpleIni btbPaceotesINIFile;
		if (fs::exists(sIniFileName))
		{
			btbPaceotesINIFile.LoadFile(sIniFileName.c_str());
			iPacenotesIdx = min(btbPaceotesINIFile.GetLongValue("PACENOTES", "count", 0) - 1, 1000000);

			for (int idx = 0; idx < iPacenotesIdx; idx++)
			{
				snprintf(szKeyName, sizeof(szKeyName), "P%d", idx);
				iPacenoteType = btbPaceotesINIFile.GetLongValue(szKeyName, "type", -1);

				if (iPacenoteType == 21 && startDistance && *startDistance < 0)
				{
					*startDistance = btbPaceotesINIFile.GetDoubleValue(szKeyName, "distance", 0);

					if ((split1Distance == nullptr && split2Distance == nullptr) && finishDistance == nullptr || *finishDistance >= 0)
						break;
				}
				else if (iPacenoteType == 23)
				{
					if (split1Distance && *split1Distance < 0) *split1Distance = btbPaceotesINIFile.GetDoubleValue(szKeyName, "distance", 0);
					else if(split2Distance) *split2Distance = btbPaceotesINIFile.GetDoubleValue(szKeyName, "distance", 0);
				}
				else if (iPacenoteType == 22 && finishDistance && *finishDistance < 0)
				{
					*finishDistance = btbPaceotesINIFile.GetDoubleValue(szKeyName, "distance", 0);

					if (startDistance == nullptr || *startDistance >= 0)
						break;
				}
			}
		}
	}
	catch (...)
	{
		return FALSE;
	}

	// Some maps don't have the start note, weird. If the startDistance is missing then set it as zero.
	if (startDistance && *startDistance < 0) *startDistance = 0.0f;

	//DebugPrint("RBRRX_ReadStartSplitsFinishPacenoteDistances. start=%lf split1=%lf split2=%lf finish=%lf", *startDistance, *split1Distance, *split2Distance, *finishDistance);

	if (startDistance && finishDistance)
		return (*startDistance >= 0 && *finishDistance >= 0);
	else
		return TRUE;
}


//----------------------------------------------------------------------------------------------------
// Read driveline data and translate float coordinates to int coordinates to speed up calculations (minimap doesn't need super precision)
// Helpful driveline.ini file structure explanation by JHarro/black.f. https://www.racedepartment.com/threads/just-a-test-track.6827/#post-418388
// - 3 floats: x, y, z coordinates
// - 3 floats: x, y, z tangent to the next point (general direction to the next point)
// - 1 float:  Distance from the start point along the road (driveline curve)
// - 1 float:  Always zero
//
int CNGPCarMenu::RBRRX_ReadDriveline(const std::string& folderName, CDrivelineSource& drivelineSource)
{
	std::string sDrivelineINIFileName;
	std::string sPacenoteINIFileName;
	std::ifstream drivelineFile;

	std::string sTextLine;
	size_t delimCharPos;
	bool coordValuesDataAlreadyReserved = false;
	std::vector<std::string> coordValues;
	float x, y, distance;

	if (_iEqual(fs::path(folderName).filename().string(), "driveline.ini", true))
	{
		// Input param is a full path to driveline.ini file
		sDrivelineINIFileName = folderName;
		sPacenoteINIFileName = fs::path(folderName).replace_filename("").string() + "\\pacenotes.ini";
	}
	else
	{
		// Input param is a BTB track folder without path (without driveline.ini/pacenotes.ini file names, so append those now)
		sDrivelineINIFileName = m_sRBRRootDir + "\\RX_Content\\" + folderName + "\\driveline.ini";
		sPacenoteINIFileName  = m_sRBRRootDir + "\\RX_Content\\" + folderName + "\\pacenotes.ini";
	}

	DebugPrint("RBRRX_ReadDriveline. Reading %s and %s files", sDrivelineINIFileName.c_str(), sPacenoteINIFileName.c_str());

	drivelineSource.vectDrivelinePoint.clear();

	if (folderName.empty() || !fs::exists(sDrivelineINIFileName))
		return 0;

	drivelineSource.pointMin.x = drivelineSource.pointMin.y = 9999999.0f;
	drivelineSource.pointMax.x = drivelineSource.pointMax.y = -9999999.0f;

	try
	{
		// Read start/split1/split2/finish distance (from the beginning of driveline data)
		double startDistance, split1Distance, split2Distance, finishDistance;
		RBRRX_ReadStartSplitsFinishPacenoteDistances(sPacenoteINIFileName, &startDistance, &split1Distance, &split2Distance, &finishDistance);

		drivelineSource.startDistance  = static_cast<float>(startDistance);
		drivelineSource.split1Distance = static_cast<float>(split1Distance);
		drivelineSource.split2Distance = static_cast<float>(split2Distance);
		drivelineSource.finishDistance = static_cast<float>(finishDistance);

		// Read driveline data (this routine expects the driveline data to be already sorted by distance in ascending order)
		drivelineFile.open(sDrivelineINIFileName);
		while (std::getline(drivelineFile, sTextLine))
		{
			// Skip all other but "Kx=coordX, coordY" lines 
			if (sTextLine.empty())
				continue;

			if (sTextLine[0] != 'K')
				continue;

			// Take the first two coordinates from the driveline data line (x,y)
			delimCharPos = sTextLine.find_first_of('=');
			if (delimCharPos == std::string::npos || sTextLine.length() <= delimCharPos + 1)
				continue;

			if (_SplitString(sTextLine.substr(delimCharPos + 1), coordValues, ",", false, true, 7) < 7)
				continue;

			// Read distance from drivelineData, but skip the coordinate if it's distance is before the startline
			distance = std::stof(coordValues[6]);
			if (distance < drivelineSource.startDistance)
				continue;

			if (drivelineSource.finishDistance > 0 && distance > drivelineSource.finishDistance)
				break;

			x = std::stof(coordValues[0]);			
			y = std::stof(coordValues[1]);			

			if (x < drivelineSource.pointMin.x) drivelineSource.pointMin.x = x;
			if (x > drivelineSource.pointMax.x) drivelineSource.pointMax.x = x;
			if (y < drivelineSource.pointMin.y) drivelineSource.pointMin.y = y;
			if (y > drivelineSource.pointMax.y) drivelineSource.pointMax.y = y;

			drivelineSource.vectDrivelinePoint.push_back({ {x, y}, distance });
		}
	}
	catch (...)
	{
		drivelineSource.vectDrivelinePoint.clear();
		LogPrint("ERROR. RBRRX_ReadDriveline failed to read or invalid values in %s", sDrivelineINIFileName.c_str());
	}

	return drivelineSource.vectDrivelinePoint.size();
}


//----------------------------------------------------------------------------------------------------
// Draw a minimap of BTB track using the driveline data
//
void CNGPCarMenu::RBRRX_DrawMinimap(const std::string& folderName, int screenID, LPDIRECT3DDEVICE9 pOutputD3DDevice)
{
	static CMinimapData* prevMinimapData = nullptr;
	RECT minimapRect;

	if (m_minimapRBRRXPictureRect[screenID].bottom == -1)
		return; // Minimap drawing disabled

	// If target D3D device is not specified then use the default RBR device (RBRRX LoadTrack custom screen has another output device)
	if (pOutputD3DDevice == nullptr)
		pOutputD3DDevice = g_pRBRIDirect3DDevice9;

	// ScreenID 0 = Stages menu list
	// ScreenID 1 = The weather settings of the chosen stage menu screen (draw the minimap in pre-defined location in top right corner)
	// ScreenID 2 = RBRRX LoadTrack custom screen
	minimapRect = m_minimapRBRRXPictureRect[screenID];

	// If the requested minimap and size is still the same then no need to re-calculate the minimap (just draw the existing minimap)
	if (prevMinimapData == nullptr || (folderName != prevMinimapData->trackFolder || !EqualRect(&minimapRect, &prevMinimapData->minimapRect) ))
	{ 
		CDrivelineSource drivelineSource;

		// Try to find existing cached minimap data
		prevMinimapData = nullptr;
		for (auto& item : m_cacheRBRRXMinimapData)
		{
			if (item->trackFolder == folderName && EqualRect(&item->minimapRect, &minimapRect))
			{
				prevMinimapData = item.get();
				break;
			}
		}

		if (prevMinimapData == nullptr)
		{
			DebugPrint("RBRRX_DrawMinimap. No minimap screen %d cache for %s track. Calculating the new minimap vector graph", screenID, folderName.c_str());

			// No existing minimap cache data. Calculate a new minimap data from the driveline data and add the final minimap data struct to cache (speeds up the next time the same minimap is needed)
			auto newMinimap = std::make_unique<CMinimapData>();
			newMinimap->trackFolder = folderName;
			newMinimap->minimapRect = minimapRect;
			prevMinimapData = newMinimap.get();
			m_cacheRBRRXMinimapData.push_front(std::move(newMinimap));

			// TODO: Cache the final minimap binary data in a disk file per track to speed up new loadings of minimap (rx_content\track\mytrack\minimap_cache1.dat and minimap_cache2.dat)
			// Read driveline data from BTB track data and take X,Y coordinates (minimap doesn't need the Z coordinate).
			// Scale down drive line coordinates to fit in minimap rect size and drop duplicate coordinate (because of down scaling some coordinates are likely to overlap)
			RBRRX_ReadDriveline(folderName, drivelineSource);
			RescaleDrivelineToFitOutputRect(drivelineSource, *prevMinimapData);
		}
	}

	if (prevMinimapData != nullptr && prevMinimapData->vectMinimapPoint.size() >= 2)
	{
		int centerPosX = max(((prevMinimapData->minimapRect.right - prevMinimapData->minimapRect.left) / 2) - (prevMinimapData->minimapSize.x / 2), 0);

		CD3D9RenderStateCache renderStateCache(pOutputD3DDevice, true);
		renderStateCache.EnableTransparentAlphaBlending();

		// Note. Disabled the gray background because minimap is drawn on top of the map (code commented out)
		//if(m_minimapVertexBuffer != nullptr)
		//	D3D9DrawVertex2D(pOutputD3DDevice, m_minimapVertexBuffer);

		// Draw the minimap driveline
		for (auto& item : prevMinimapData->vectMinimapPoint)
		{
			DWORD dwColor;
			switch (item.splitType)
			{
				case 1:  dwColor = D3DCOLOR_ARGB(180, 0xF0, 0xCD, 0x30); break;	// Color for split1->split2 part of the track (yellow)
				default: dwColor = D3DCOLOR_ARGB(180, 0xF0, 0xF0, 0xF0); break; // Color for start->split1 and split2->finish (white)
				//default: dwColor = D3DCOLOR_ARGB(180, 0xC0, 0xC0, 0xC0); break; // Color for start->split1 and split2->finish (gray)
			}

			D3D9DrawPrimitiveCircle(pOutputD3DDevice,
				(float)(centerPosX + prevMinimapData->minimapRect.left + item.drivelineCoord.x), 
				(float)(prevMinimapData->minimapRect.top + item.drivelineCoord.y), 
				2.0f, dwColor
			);
		}

		// Draw the finish line as bigger red circle
		D3D9DrawPrimitiveCircle(pOutputD3DDevice,
			(float)(centerPosX + prevMinimapData->minimapRect.left + prevMinimapData->vectMinimapPoint.back().drivelineCoord.x),
			(float)(prevMinimapData->minimapRect.top + prevMinimapData->vectMinimapPoint.back().drivelineCoord.y),
			5.0f, D3DCOLOR_ARGB(255, 0xC0, 0x10, 0x10) );

		// Draw the start line as bigger green circle
		D3D9DrawPrimitiveCircle(pOutputD3DDevice,
				(float)(centerPosX + prevMinimapData->minimapRect.left + prevMinimapData->vectMinimapPoint.front().drivelineCoord.x), 
				(float)(prevMinimapData->minimapRect.top + prevMinimapData->vectMinimapPoint.front().drivelineCoord.y), 
				5.0f, D3DCOLOR_ARGB(255, 0x20, 0xF0, 0x20) );
	}
}


//----------------------------------------------------------------------------------------------------
// BTB map loading completed
//
void CNGPCarMenu::RBRRX_OnMapLoaded()
{
	// If RBRRX custom plugin is active then add the current map to "recent RBRRX maps" shortcut list
	if (m_bRecentMapsRBRRXModified)
	{
		if (!m_latestMapRBRRX.name.empty())
		{
			if (m_latestMapRBRRX.length < 0)
			{
				// BTB track track.ini file doesn't have the Length attribute yet. Store it now when the BTB track was loaded for the first time. Round meters down to one decimal km value
				m_latestMapRBRRX.length = floor((g_pRBRCarInfo != nullptr ? g_pRBRCarInfo->distanceToFinish : 0) / 100.0f) / 10.0f;
				if (m_latestMapRBRRX.length < 1.0f)
					// For some reason the BTB track data doesn't have a valid length (some very short tracks have this issue). Use dummy value and let UpdateRBRRXINILengthOption method to decide if it can use pacenotes.ini to estimate the length
					m_latestMapRBRRX.length = -1;

				RBRRX_UpdateINILengthOption(m_latestMapRBRRX.folderName, m_latestMapRBRRX.length);
			}

			// Stage loaded while RBRRX plugin is active. Add the latest map (=stage) to the top of the recent list and update RX_CONTENt\Tracks\myMap\track.ini Length parameter if it is missing
			if (m_recentMapsMaxCountRBRRX > 0)
			{
				m_bRecentMapsRBRRXModified = FALSE;
				RBRRX_AddMapToRecentList(m_latestMapRBRRX.folderName);
				if (m_bRecentMapsRBRRXModified) SaveSettingsToRBRRXRecentMaps();
			}
		}

		m_bRecentMapsRBRRXModified = FALSE;
	}
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

					if (m_pCustomMapMenuRBRRX == nullptr)
					{
						int numOfRecentMaps;
						
						// Show shortcuts only when the number of original BTB stages is at least 17 (if the num of BTB stages is less then no need to do any scrolling, so no need to show any shortcuts either)
						if (m_origNumOfItemsMenuItemsRBRRX >= 8 + 1 + 8)
							numOfRecentMaps = RBRRX_CalculateNumOfValidMapsInRecentList(m_pOrigMapMenuItemsRBRRX, m_origNumOfItemsMenuItemsRBRRX);
						else
							numOfRecentMaps = 0;

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

						if (m_origNumOfItemsMenuItemsRBRRX >= 8 + 1 + 8)
						{
							numOfRecentMaps = 0;
							for (auto& item : m_recentMapsRBRRX)
							{
								// At this point there are only max number of valid maps in the recent list (CalculateNumOfValidMapsInRecentList removed invalid and extra items)
								strncpy_s(m_pCustomMapMenuRBRRX[numOfRecentMaps].szTrackName, item->name.c_str(), COUNT_OF_ITEMS(m_pCustomMapMenuRBRRX[numOfRecentMaps].szTrackName));
								strncpy_s(m_pCustomMapMenuRBRRX[numOfRecentMaps].szTrackFolder, item->folderName.c_str(), COUNT_OF_ITEMS(m_pCustomMapMenuRBRRX[numOfRecentMaps].szTrackFolder));
								m_pCustomMapMenuRBRRX[numOfRecentMaps].physicsID = item->physicsID;
								numOfRecentMaps++;
							}
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
								menuIdx = RBRRX_FindMenuItemIdxByFolderName(m_pCustomMapMenuRBRRX, m_numOfItemsCustomMapMenuRBRRX, m_latestMapRBRRX.folderName);

							RBRRX_FocusNthMenuIdxRow(menuIdx);
						}

						// Stages menu is open, save recentMaps INI options when a stage is chosen for racing and the recent list is modified
						m_bRecentMapsRBRRXModified = TRUE;
						
						// Initialize "LoadTrack" custom screen (new BTB track)
						m_bRBRRXLoadingNewTrack = TRUE;
					}

					if (m_pCustomMapMenuRBRRX != nullptr)
					{
						// Check normal RBR menu navigation (key up or down) and scroll the menu list if necessary
						if (m_prevCustomMapSelectedItemIdxRBRRX != m_pRBRRXPlugin->pMenuData->selectedItemIdx)
						{
							if (m_prevCustomMapSelectedItemIdxRBRRX == 0 && m_pRBRRXPlugin->pMenuData->selectedItemIdx + 1 == min(m_numOfItemsCustomMapMenuRBRRX, 8 + 1 + 8))
								// Wrap to the last menu item
								RBRRX_FocusNthMenuIdxRow(m_numOfItemsCustomMapMenuRBRRX - 1);
							else if (m_prevCustomMapSelectedItemIdxRBRRX + 1 == min(m_numOfItemsCustomMapMenuRBRRX, 8 + 1 + 8) && m_pRBRRXPlugin->pMenuData->selectedItemIdx == 0)
								// Wrap to the first menu item 
								RBRRX_FocusNthMenuIdxRow(0);
							else if (m_prevCustomMapSelectedItemIdxRBRRX < m_pRBRRXPlugin->pMenuData->selectedItemIdx)
								RBRRX_FocusNthMenuIdxRow(m_currentCustomMapSelectedItemIdxRBRRX + 1); // Next row (scroll if necessary)
							else if (m_prevCustomMapSelectedItemIdxRBRRX > m_pRBRRXPlugin->pMenuData->selectedItemIdx)
								RBRRX_FocusNthMenuIdxRow(m_currentCustomMapSelectedItemIdxRBRRX - 1); // Prev row (scroll if necessary)
						}
						else
						{
							// Check custom pgup/pgdown/home/end navigation keys
							if (m_pRBRRXPlugin->keyCode != -1)
							{
								switch (m_pRBRRXPlugin->keyCode)
								{
								case VK_PRIOR: RBRRX_FocusNthMenuIdxRow(m_currentCustomMapSelectedItemIdxRBRRX - (8 + 1 + 8)); break;	// PageUp key (move 8+1+8 rows up)
								case VK_NEXT:  RBRRX_FocusNthMenuIdxRow(m_currentCustomMapSelectedItemIdxRBRRX + (8 + 1 + 8)); break;	// PageDown key (move 8+1+8 rows down)
								case VK_END:   RBRRX_FocusNthMenuIdxRow(m_numOfItemsCustomMapMenuRBRRX - 1); break;	// End key
								case VK_HOME:  RBRRX_FocusNthMenuIdxRow(0); break;									// Home key
								case VK_LEFT:  RBRRX_FocusNthMenuIdxRow(m_currentCustomMapSelectedItemIdxRBRRX - 8); break;	// Left arrow key (move 8 rows up)
								case VK_RIGHT: RBRRX_FocusNthMenuIdxRow(m_currentCustomMapSelectedItemIdxRBRRX + 8); break;	// Right arrow key (move 8 rows down)
								}

								// Don't repeat the same key until the key is released and re-pressed
								m_pRBRRXPlugin->keyCode = -1;
							}
						}

						// If the current menu row is different than the latest menu row then load new details (stage name, length, surface, previewImage)
						if (m_latestMapRBRRX.mapIDMenuIdx != m_currentCustomMapSelectedItemIdxRBRRX || m_latestMapRBRRX.trackOptionsFirstTimeSetup == FALSE)
						{
							m_latestMapRBRRX.trackOptionsFirstTimeSetup = TRUE;
							RBRRX_UpdateMapInfo(m_currentCustomMapSelectedItemIdxRBRRX, &m_latestMapRBRRX);

							// Release previous map preview texture and read a new image file (if preview path is set and the image file exists and map preview img drawing is not disabled)
							SAFE_RELEASE(m_latestMapRBRRX.imageTexture.pTexture);
							if (!m_latestMapRBRRX.previewImageFile.empty() && fs::exists(m_latestMapRBRRX.previewImageFile) && m_mapRBRRXPictureRect[0].bottom != -1)
							{
								hResult = D3D9CreateRectangleVertexTexBufferFromFile(g_pRBRIDirect3DDevice9,
									m_latestMapRBRRX.previewImageFile,
									(float)m_mapRBRRXPictureRect[0].left, (float)m_mapRBRRXPictureRect[0].top, (float)(m_mapRBRRXPictureRect[0].right - m_mapRBRRXPictureRect[0].left), (float)(m_mapRBRRXPictureRect[0].bottom - m_mapRBRRXPictureRect[0].top),
									&m_latestMapRBRRX.imageTexture,
									0  /*IMAGE_TEXTURE_PRESERVE_ASPECTRATIO_BOTTOM | IMAGE_TEXTURE_POSITION_HORIZONTAL_CENTER*/);

								// Image not available or loading failed
								if (!SUCCEEDED(hResult))
									SAFE_RELEASE(m_latestMapRBRRX.imageTexture.pTexture);
							}

							// Read stage records for this RBRRX stage							
							if(m_recentResultsPosition_RBRRX.y != -1)
								RaceStatDB_QueryLastestStageResults(-1, m_latestMapRBRRX.name, 2, m_latestMapRBRRX.latestStageResults);
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

					// Draw RBRRX map preview img
					if (m_latestMapRBRRX.imageTexture.pTexture != nullptr)
					{
						m_pD3D9RenderStateCache->EnableTransparentAlphaBlending();
						D3D9DrawVertexTex2D(g_pRBRIDirect3DDevice9, m_latestMapRBRRX.imageTexture.pTexture, m_latestMapRBRRX.imageTexture.vertexes2D, m_pD3D9RenderStateCache);
						m_pD3D9RenderStateCache->RestoreState();
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

					g_pFontCarSpecCustom->DrawText(posX, posY + (iMapInfoPrintRow++ * iFontHeight), C_CARSPECTEXT_COLOR, sStrStream.str().c_str(), 0);

					if (!m_latestMapRBRRX.comment.empty())
						g_pFontCarSpecCustom->DrawText(posX, posY + (iMapInfoPrintRow++ * iFontHeight), C_CARSPECTEXT_COLOR, m_latestMapRBRRX.comment.c_str(), 0);

					if (m_latestMapRBRRX.latestStageResults.size() > 0)
					{
						g_pFontCarSpecCustom->DrawText(posX, posY + (iMapInfoPrintRow++ * iFontHeight), C_CARSPECTEXT_COLOR,
							(GetLangWString(L"SS record", true) +
								_ToWString(GetSecondsAsMISSMS(m_latestMapRBRRX.latestStageResults[0].stageRecord, 0))
								+ L" ("
								+ _ToWString(GetSecondsAsKMh(m_latestMapRBRRX.latestStageResults[0].stageRecord, m_latestMapRBRRX.latestStageResults[0].stageLength /*static_cast<float>(m_latestMapRBRRX.length * 1000)*/, true, 1))
								+ L")").c_str()
						);
						
						iMapInfoPrintRow += 1;
					}

					
					// Draw the list of recent race results
					if (!(m_recentResultsPosition_RBRRX.x == 0 && m_recentResultsPosition_RBRRX.y == 0))
					{
						// Position is explicitly set instead of relative to BTB metadata title texts
						posX = m_recentResultsPosition_RBRRX.x;
						posY = m_recentResultsPosition_RBRRX.y;
						iMapInfoPrintRow = 0;
					}

					DrawRecentResultsTable(posX, posY + (iMapInfoPrintRow++ * iFontHeight), m_latestMapRBRRX.latestStageResults);

					// Author, version, date printed on bottom of the screen and map preview image
					iMapInfoPrintRow = 0;
					posY = m_mapRBRRXPictureRect[0].bottom - (1 * iFontHeight);

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

					// Draw a minimap (0=stage menu list)
					RBRRX_DrawMinimap(m_latestMapRBRRX.folderName, 0);
				}
			}
			else if (g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_QUICKRALLY_WEATHER])
			{
				//
				// RBRRX track selected (showing weather options screen, it is the same as under normal QuickRally)
				//
				g_pRBRMenuSystem->menuImageHeight = g_pRBRMenuSystem->menuImageWidth = 0;

				// Fix the RBRRX bug where backspace (prevMenu key) takes back to RBR main menu. Set custom plugin (RBRRX) as the previous menu
				if(g_pRBRMenuSystem->currentMenuObj->prevMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_MAIN] && g_pRBRPluginMenuSystem->customPluginMenuObj != nullptr)
					g_pRBRMenuSystem->currentMenuObj->prevMenuObj = g_pRBRPluginMenuSystem->customPluginMenuObj;

				if (m_latestMapRBRRX.trackOptionsFirstTimeSetup)
				{
					//float posLeft, posTop, posRight, posBottom;

					m_latestMapRBRRX.trackOptionsFirstTimeSetup = FALSE;

					// RBR_RX has a bug where shorter track name is not null-terminated, so the remaining of the previous track name may be shown as a left over if the new track name is shorter.
					wcsncpy_s(m_pRBRRXPlugin->wszTrackName, COUNT_OF_ITEMS(m_pRBRRXPlugin->wszTrackName), _ToWString(m_latestMapRBRRX.name).c_str(), COUNT_OF_ITEMS(m_pRBRRXPlugin->wszTrackName)-1);

					// Map preview img and minimap on top of the img
					//RBRAPI_MapRBRPointToScreenPoint(5.0f, 145.0f, &posLeft, &posTop);
					//RBRAPI_MapRBRPointToScreenPoint(635.0f, 480.0f, &posRight, &posBottom);

					// Release previous map preview texture and read a new image file (if preview path is set and the image file exists and map preview img drawing is not disabled)
					SAFE_RELEASE(m_latestMapRBRRX.imageTexture.pTexture);
					if (!m_latestMapRBRRX.previewImageFile.empty() && fs::exists(m_latestMapRBRRX.previewImageFile) && m_mapRBRRXPictureRect[1].bottom != -1)
					{
						hResult = D3D9CreateRectangleVertexTexBufferFromFile(g_pRBRIDirect3DDevice9,
							m_latestMapRBRRX.previewImageFile,
							//((posLeft, posTop, posRight-posLeft, posBottom-posTop,
							(float)m_mapRBRRXPictureRect[1].left, (float)m_mapRBRRXPictureRect[1].top, (float)(m_mapRBRRXPictureRect[1].right - m_mapRBRRXPictureRect[1].left), (float)(m_mapRBRRXPictureRect[1].bottom - m_mapRBRRXPictureRect[1].top),
							&m_latestMapRBRRX.imageTexture,
							0  /*IMAGE_TEXTURE_PRESERVE_ASPECTRATIO_BOTTOM | IMAGE_TEXTURE_POSITION_HORIZONTAL_CENTER*/);

						// Image not available or loading failed
						if (!SUCCEEDED(hResult))
							SAFE_RELEASE(m_latestMapRBRRX.imageTexture.pTexture);
					}
				}

				iFontHeight = g_pFontCarSpecCustom->GetTextHeight();

				int iMapInfoPrintRow = 0;
				RBRAPI_MapRBRPointToScreenPoint(320.0f, 44.0f, &posX, &posY);

				g_pFontCarSpecCustom->DrawText(posX, posY + (iMapInfoPrintRow++ * iFontHeight), C_CARMODELTITLETEXT_COLOR, _ToWString(m_latestMapRBRRX.name).c_str(), 0);

				if (m_latestMapRBRRX.length > 0)
					// TODO: KM to Miles miles=km*0.621371192 config option support
					sStrStream << m_latestMapRBRRX.length << L" km";

				if (!m_latestMapRBRRX.surface.empty())
					sStrStream << (sStrStream.tellp() != std::streampos(0) ? L" " : L"") << GetLangStr(m_latestMapRBRRX.surface.c_str());

				if (m_latestMapRBRRX.numOfPacenotes >= 15)
					sStrStream << (sStrStream.tellp() != std::streampos(0) ? L" " : L"") << GetLangStr(L"pacenotes");

				g_pFontCarSpecCustom->DrawText(posX, posY + (iMapInfoPrintRow++ * iFontHeight), C_CARSPECTEXT_COLOR, sStrStream.str().c_str(), 0);

				if (!m_latestMapRBRRX.comment.empty())
					g_pFontCarSpecCustom->DrawText(posX, posY + (iMapInfoPrintRow++ * iFontHeight), C_CARSPECTEXT_COLOR, m_latestMapRBRRX.comment.c_str(), 0);

				if (!m_latestMapRBRRX.author.empty())
					g_pFontCarSpecCustom->DrawText(posX, posY + (iMapInfoPrintRow++ * iFontHeight), C_CARSPECTEXT_COLOR, (GetLangWString(L"author", true) + _ToWString(m_latestMapRBRRX.author)).c_str(), 0);

				if (!m_latestMapRBRRX.version.empty())
					g_pFontCarSpecCustom->DrawText(posX, posY + (iMapInfoPrintRow++ * iFontHeight), C_CARSPECTEXT_COLOR, (GetLangWString(L"version", true) + _ToWString(m_latestMapRBRRX.version) + L" " + _ToWString(m_latestMapRBRRX.date)).c_str(), 0);

				if (m_latestMapRBRRX.imageTexture.pTexture != nullptr)
				{
					m_pD3D9RenderStateCache->EnableTransparentAlphaBlending();
					D3D9DrawVertexTex2D(g_pRBRIDirect3DDevice9, m_latestMapRBRRX.imageTexture.pTexture, m_latestMapRBRRX.imageTexture.vertexes2D, m_pD3D9RenderStateCache);
					m_pD3D9RenderStateCache->RestoreState();
				}

				// Draw a minimap (1=weather menu list)
				RBRRX_DrawMinimap(m_latestMapRBRRX.folderName, 1);
			}
		}

		else if (/*!m_bRBRRXPluginActive &&*/ m_pRBRRXPlugin != nullptr
			&& m_pRBRRXPlugin->pMenuItems != m_pOrigMapMenuItemsRBRRX 
			&& (g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_MAIN] || GetActivePluginName() != "RBR") 
		)
		{
			//
			// RBRRX is no longer active, but the custom menu is created. Clean it up and restore the original RBRRX menu obj
			//
			m_pRBRRXPlugin->pMenuData->selectedItemIdx = 0;
			m_pRBRRXPlugin->pMenuItems = m_pOrigMapMenuItemsRBRRX;
			m_pRBRRXPlugin->numOfItems = m_origNumOfItemsMenuItemsRBRRX;

			if (m_pCustomMapMenuRBRRX != nullptr)
			{
				delete[] m_pCustomMapMenuRBRRX;
				m_pCustomMapMenuRBRRX = nullptr;
			}
		}		
	}
}

