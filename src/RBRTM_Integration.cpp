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
#include <sstream>				// std::stringstream
//#include <chrono>				// std::chrono::steady_clock

#include <locale>				// UTF8 locales
//#include <codecvt>
//#include <wincodec.h>			// GUID_ContainerFormatPng 

#include "NGPCarMenu.h"

namespace fs = std::filesystem;

//----------------------------------------------------------------------------------------------------
//
int CNGPCarMenu::RBRTM_FindMenuItemIdxByMapID(PRBRTMMenuItem pMenuItems, int numOfItems, int mapID)
{
	// Find the RBRTM menu item by mapID (search menuItem array directly). Return index to the menuItem struct or -1
	if (pMenuItems != nullptr)
	{
		for (int idx = 0; idx < numOfItems; idx++)
			if (pMenuItems[idx].mapID == mapID)
				return idx;
	}
	return -1;
}

int CNGPCarMenu::RBRTM_FindMenuItemIdxByMapID(PRBRTMMenuData pMenuData, int mapID)
{
	// Find the RBRTM menu item by mapID. Return index to the menuItem struct or -1
	if (pMenuData != nullptr)
		return RBRTM_FindMenuItemIdxByMapID(pMenuData->pMenuItems, pMenuData->numOfItems, mapID);
	return -1;
}

int CNGPCarMenu::RBRTM_CalculateNumOfValidMapsInRecentList(PRBRTMMenuData pMenuData)
{
	int numOfRecentMaps = 0;
	int menuIdx;

	auto it = m_recentMapsRBRTM.begin();
	while (it != m_recentMapsRBRTM.end())
	{
		if ((*it)->mapID > 0 && numOfRecentMaps < m_recentMapsMaxCountRBRTM)
		{
			menuIdx = RBRTM_FindMenuItemIdxByMapID(pMenuData, (*it)->mapID);

			if (menuIdx >= 0 || pMenuData == nullptr)
			{
				// The mapID in recent list is valid. Refresh the menu entry name as "[recent idx] Stage name"
				numOfRecentMaps++;
				if (pMenuData != nullptr)
				{
					(*it)->name = L"[" + std::to_wstring(numOfRecentMaps) + L"] ";
					(*it)->name.append(pMenuData->pMenuItems[menuIdx].wszMenuItemName);
				}

				// Go to the next item in list iterator
				++it;
			}
			else
			{
				// The map in recent list is no longer in stages menu list. Invalidate the recent item (ie. it is not added as a shortcut to RBRTM Shakedown stages menu)
				it = m_recentMapsRBRTM.erase(it);
			}
		}
		else
		{
			// Invalid mapID value or "too many items". Remove the item if the menu data was defined
			if (pMenuData != nullptr)
				it = m_recentMapsRBRTM.erase(it);
			else
				++it;
		}
	}

	return numOfRecentMaps;
}

void CNGPCarMenu::RBRTM_AddMapToRecentList(int mapID)
{
	if (mapID <= 0) return;

	// Add mapID to top of the recentMaps list (if already in the list then move it to top, otherwise add as a new item and remove the last item if the list is full)
	for (auto& iter = m_recentMapsRBRTM.begin(); iter != m_recentMapsRBRTM.end(); ++iter)
	{
		if ((*iter)->mapID == mapID)
		{
			// MapID already in the recent list. Move it to the top of the list (no need to re-add it to the list)
			if (iter != m_recentMapsRBRTM.begin())
			{
				m_recentMapsRBRTM.splice(m_recentMapsRBRTM.begin(), m_recentMapsRBRTM, iter, std::next(iter));
				m_bRecentMapsRBRTMModified = TRUE;
			}

			// Must return after moving the item to top of list because for-iterator is now invalid because the list was modified
			return;
		}
	}

	// MapID is not yet in the recent list. Add it to the top of the list (we cannot delete items at this point because the to-be-removed recentItem may be used by Shakedown stages menu)
	auto newItem = std::make_unique<RBRTM_MapInfo>();
	newItem->mapID = mapID;
	m_recentMapsRBRTM.push_front(std::move(newItem));
	m_bRecentMapsRBRTMModified = TRUE;
}


//----------------------------------------------------------------------------------------------------
// Read pacenote data
//    Offset 0x5C = Num of pacenote records   
//           0x7C = File offset to the beginning of the pacenote data structs
//
// Or some maps (fex track-61) uses offsets (how to detect which one is the correct logic?)
//    Offset 0xF4  = Num of pacenote records   
//          0x114 = File offset to the beginning of the pacenote data structs
//
// Or some maps (fex track-63) uses offsets (how to detect which one is the correct logic?)
//    Offset 0x6C = Num of pacenote records   
//           0x8C = File offset to the beginning of the pacenote data structs

BOOL CNGPCarMenu::RBRTM_ReadStartSplitsFinishPacenoteDistances(const std::wstring& trackFileName, float* startDistance, float* split1Distance, float* split2Distance, float* finishDistance)
{
	static const WCHAR* wszPacenoteFilePostfix[4] = { L"_O.dls", L"_M.dls", L"_N.dls", L"_E.dls" };

	std::wstring sPacenoteFileName;
	__int32 numOfPacenoteRecords = 0;
	
	*startDistance = -1;
	*split1Distance = -1;
	*split2Distance = -1;
	*finishDistance = -1;

	// Get the pacenote DLS data filename (fex c:\games\rbr\Maps\Track-71_O.dls)
	for (int idx = 0; idx < COUNT_OF_ITEMS(wszPacenoteFilePostfix); idx++)
	{
		sPacenoteFileName = m_sRBRRootDirW + L"\\" + trackFileName + wszPacenoteFilePostfix[idx];
		if (fs::exists(sPacenoteFileName)) break;
		sPacenoteFileName.clear();
	}

	if (sPacenoteFileName.empty())
	{
		DebugPrint(L"RBRTM_ReadStartSplitsFinishPacenoteDistances. The map %s doesn't have pacenote file. Cannot read pacenote data", trackFileName.c_str());
		return FALSE;
	}

	return ReadStartSplitsFinishPacenoteDistances(sPacenoteFileName, startDistance, split1Distance, split2Distance, finishDistance);
}


//----------------------------------------------------------------------------------------------------
// Read driveline data from TRK file
// Offset 0x10 = Num of driveline records (DWORD)
//        0x14 = Driveline record 8 x DWORD (x y z cx cy cz distance zero)
//        ...N num of driveline records...
//
int  CNGPCarMenu::RBRTM_ReadDriveline(int mapID, CDrivelineSource& drivelineSource)
{
	static const WCHAR* wszDrivelineFilePostfix[4] = { L"_O.trk", L"_M.trk", L"_N.trk", L"_E.trk" };

	WCHAR wszMapINISection[16];
	std::wstring sTrackName;
	std::wstring sDrivelineFileName;

	//__int32 numOfDrivelineRecords;

	try
	{
		//DebugPrint("RBRTM_ReadDriveline. Reading driveline for mapID %d", mapID);
		drivelineSource.vectDrivelinePoint.clear();
		if (mapID <= 0 || m_pTracksIniFile == nullptr)
			return 0;

		// Get the base name of the track map data files (fex Maps\Track-71)
		swprintf_s(wszMapINISection, COUNT_OF_ITEMS(wszMapINISection), L"Map%02d", mapID);
		sTrackName = _RemoveEnclosingChar(m_pTracksIniFile->GetValue(wszMapINISection, L"TrackName", L""), L'"', false);
		if (sTrackName.empty())
			return 0;

		// Get the driveline data filename (fex c:\games\rbr\Maps\Track-71_O.trk)
		for (int idx = 0; idx < COUNT_OF_ITEMS(wszDrivelineFilePostfix); idx++)
		{
			sDrivelineFileName = m_sRBRRootDirW + L"\\" + sTrackName + wszDrivelineFilePostfix[idx];
			if (fs::exists(sDrivelineFileName)) break;
			sDrivelineFileName.clear();
		}

		if (sDrivelineFileName.empty())
		{
			DebugPrint("RBRTM_ReadDriveline. The mapID %d doesn't have TRK file. Cannot read driveline data", mapID);
			return 0;
		}

		DebugPrint(L"RBRTM_ReadDriveline. Reading driveline from %s TRK file", sDrivelineFileName.c_str());

		RBRTM_ReadStartSplitsFinishPacenoteDistances(sTrackName, &drivelineSource.startDistance, &drivelineSource.split1Distance, &drivelineSource.split2Distance, &drivelineSource.finishDistance);
		ReadDriveline(sDrivelineFileName, drivelineSource);

/*
		drivelineSource.pointMin.x = drivelineSource.pointMin.y = 9999999.0f;
		drivelineSource.pointMax.x = drivelineSource.pointMax.y = -9999999.0f;

		//DebugPrint(L"RBRTM_ReadDriveline. Using %s TRK file", sDrivelineFileName.c_str());

		// Read driveline data from the TRK file into vector buffer
		std::ifstream srcFile(sDrivelineFileName, std::ifstream::binary | std::ios::in);
		if (!srcFile) return 0;

		srcFile.seekg(0x10);
		srcFile.read((char*)&numOfDrivelineRecords, sizeof(__int32));
		if (srcFile.fail()) numOfDrivelineRecords = 0;

		//DebugPrint("RBRTM_ReadDriveline. NumOfDrivelineRecords=%d", numOfDrivelineRecords);
		if(numOfDrivelineRecords <= 0 || numOfDrivelineRecords >= 100000)
		{
			LogPrint("ERROR CNGPCarMenu::RBRTM_ReadDriveline. Invalid number of records %d", numOfDrivelineRecords);
			numOfDrivelineRecords = 0;
		}
		else
		{
			std::vector<float> vectDrivelineData(numOfDrivelineRecords * 8);
			srcFile.read(reinterpret_cast<char*>(&vectDrivelineData[0]), (static_cast<std::streamsize>(numOfDrivelineRecords) * 8 * sizeof(float)));
			if (srcFile.fail())
			{
				LogPrint("ERROR CNGPCarMenu::RBRTM_ReadDriveline. Failed to read data");
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
*/
	}
	catch (...)
	{
		drivelineSource.vectDrivelinePoint.clear();
		LogPrint(L"ERROR CNGPCarMenu::RBRTM_ReadDriveline. Failed to read driveline data from %s", sDrivelineFileName.c_str());
	}

	return drivelineSource.vectDrivelinePoint.size();
}

void CNGPCarMenu::RBRTM_DrawMinimap(int mapID, int screenID)
{
	static CMinimapData* prevMinimapData = nullptr;
	RECT minimapRect;

	if (m_minimapRBRTMPictureRect[screenID].bottom == -1)
		return; // Minimap drawing disabled

	minimapRect = m_minimapRBRTMPictureRect[screenID];

/*
	if (screenID == 0)
	{
		if (m_minimapRBRTMPictureRect[screenID].top == 0 && m_minimapRBRTMPictureRect[screenID].right == 0 && m_minimapRBRTMPictureRect[screenID].left == 0 && m_minimapRBRTMPictureRect[screenID].bottom == 0)
		{
			// Draw minimap on top of the stage preview img
			//RBRAPI_MapRBRPointToScreenPoint(390.0f, 320.0f, (int*)&minimapRect.left, (int*)&minimapRect.top);
			//RBRAPI_MapRBRPointToScreenPoint(630.0f, 470.0f - (g_pFontCarSpecModel->GetTextHeight() * 1.5f), (int*)&minimapRect.right, (int*)&minimapRect.bottom);
			minimapRect = m_mapRBRTMPictureRect;
			minimapRect.bottom -= (g_pFontCarSpecModel->GetTextHeight() + (g_pFontCarSpecModel->GetTextHeight() / 2));
		}
		else
		{
			// INI file defines an exact size and position for the minimap
			minimapRect = m_minimapRBRTMPictureRect[screenID];
		}
	}
	else if (screenID == 1)
	{
		// Show the minimap on top of the map preview image in "Weather" screen in rbrtm
		RBRAPI_MapRBRPointToScreenPoint(5.0f, 185.0f, (int*)&minimapRect.left, (int*)&minimapRect.top);
		RBRAPI_MapRBRPointToScreenPoint(635.0f, 480.0f, (int*)&minimapRect.right, (int*)&minimapRect.bottom);
		minimapRect.bottom -= static_cast<long>(g_pFontCarSpecModel->GetTextHeight() * 1.5f);
	}
	else
	{
		//LogPrint("WARNING. Invalid screenID %d in minimap drawing", screenID);
		return;
	}
*/

	// If the requested minimap and size is still the same then no need to re-calculate the minimap (just draw the existing minimap)
	if (prevMinimapData == nullptr || (mapID != prevMinimapData->mapID|| !EqualRect(&minimapRect, &prevMinimapData->minimapRect)))
	{
		CDrivelineSource drivelineSource;

		// Try to find existing cached minimap data
		prevMinimapData = nullptr;
		for (auto& item : m_cacheRBRTMMinimapData)
		{
			if (item->mapID == mapID && EqualRect(&item->minimapRect, &minimapRect))
			{
				prevMinimapData = item.get();
				break;
			}
		}
		if (prevMinimapData == nullptr)
		{
			DebugPrint("RBRTM_DrawMinimap. No minimap screen %d cache for %d track. Calculating the new minimap vector graph", screenID, mapID);

			// No existing minimap cache data. Calculate a new minimap data from the driveline data and add the final minimap data struct to cache (speeds up the next time the same minimap is needed)
			auto newMinimap = std::make_unique<CMinimapData>();
			newMinimap->mapID = mapID;
			newMinimap->minimapRect = minimapRect;
			prevMinimapData = newMinimap.get();
			m_cacheRBRTMMinimapData.push_front(std::move(newMinimap));

			RBRTM_ReadDriveline(mapID, drivelineSource);
			RescaleDrivelineToFitOutputRect(drivelineSource, *prevMinimapData);
		}
	}

	if (prevMinimapData != nullptr && prevMinimapData->vectMinimapPoint.size() >= 2)
	{
		//int centerPosX = max(((prevMinimapData->minimapRect.right - prevMinimapData->minimapRect.left) / 2) - (prevMinimapData->minimapSize.x / 2), 0);
		int centerPosX = 0;

		m_pD3D9RenderStateCache->EnableTransparentAlphaBlending();

		// Draw the minimap driveline
		for (auto& item : prevMinimapData->vectMinimapPoint)
		{
			DWORD dwColor;
			switch (item.splitType)
			{
			case 1:  dwColor = D3DCOLOR_ARGB(180, 0xF0, 0xCD, 0x30); break;	// Color for split1->split2 part of the track (yellow)
			//default: dwColor = D3DCOLOR_ARGB(180, 0xF0, 0xF0, 0xF0); break; // Color for start->split1 and split2->finish (white)
			default: dwColor = D3DCOLOR_ARGB(180, 0x60, 0xA8, 0x60); break; // Color for start->split1 and split2->finish (green)
			}

			D3D9DrawPrimitiveCircle(g_pRBRIDirect3DDevice9,
				(float)(centerPosX + prevMinimapData->minimapRect.left + item.drivelineCoord.x),
				(float)(prevMinimapData->minimapRect.top + item.drivelineCoord.y),
				2.0f, dwColor
			);
		}

		// Draw the finishline as bigger red circle
		D3D9DrawPrimitiveCircle(g_pRBRIDirect3DDevice9,
			(float)(centerPosX + prevMinimapData->minimapRect.left + prevMinimapData->vectMinimapPoint.back().drivelineCoord.x),
			(float)(prevMinimapData->minimapRect.top + prevMinimapData->vectMinimapPoint.back().drivelineCoord.y),
			5.0f, D3DCOLOR_ARGB(255, 0xC0, 0x10, 0x10) );

		// Draw the start line as bigger green circle
		D3D9DrawPrimitiveCircle(g_pRBRIDirect3DDevice9,
			(float)(centerPosX + prevMinimapData->minimapRect.left + prevMinimapData->vectMinimapPoint.front().drivelineCoord.x),
			(float)(prevMinimapData->minimapRect.top + prevMinimapData->vectMinimapPoint.front().drivelineCoord.y),
			5.0f, D3DCOLOR_ARGB(255, 0x20, 0xF0, 0x20) );

		m_pD3D9RenderStateCache->RestoreState();
	}
}


//----------------------------------------------------------------------------------------------------
// RBRTM map loading completed
//
void CNGPCarMenu::RBRTM_OnMapLoaded()
{
	if (m_bRecentMapsRBRTMModified)
	{
		// Stage loaded in RBRTM plugin using Shakedown mode (SelectionType==2). Add the latest mapID (=stage) to the top of the recent list
		if (m_iRBRTMCarSelectionType == 2 && m_recentMapsMaxCountRBRTM > 0)
		{
			m_bRecentMapsRBRTMModified = FALSE;
			RBRTM_AddMapToRecentList(m_latestMapRBRTM.mapID);
			if (m_bRecentMapsRBRTMModified) SaveSettingsToRBRTMRecentMaps();
		}

		m_bRecentMapsRBRTMModified = FALSE;
	}
}


//----------------------------------------------------------------------------------------------------
// RBRTM integration handler (DX9 EndScene)
//
void CNGPCarMenu::RBRTM_EndScene()
{
	if (g_pRBRGameMode->gameMode == 03)
	{	
		if (m_bRBRTMPluginActive)
		{
			HRESULT hResult;

			int menuIdx;
			int iFontHeight;
			int posX;
			int posY;


			//
			// RBRTM integration enabled. Check if RMRTB is in "SelectCar" menu in Shakedown or OnlineTournament menus
			//

			bool bRBRTMCarSelectionMenu = false;
			bool bRBRTMStageSelectionMenu = false; // Is stage selection menu of Shakedown RBRTM menu active?

			if (g_pRBRPluginMenuSystem->customPluginMenuObj != nullptr && m_pRBRTMPlugin != nullptr && m_pRBRTMPlugin->pCurrentRBRTMMenuObj != nullptr)
			{
				// If the active custom plugin is RBRTM and RBR shows a custom plugin menuObj and the internal RBRTM menu is the Shakedown "car selection" menuID then prepare to render a car preview image
				if (m_bRBRTMPluginActive && g_pRBRPluginMenuSystem->customPluginMenuObj == g_pRBRMenuSystem->currentMenuObj && m_pRBRTMPlugin->pCurrentRBRTMMenuObj != nullptr)
				{
					switch (m_pRBRTMPlugin->pCurrentRBRTMMenuObj->menuID)
					{
					case RBRTMMENUIDX_CARSELECTION:							// RBRTM car selection screen (either below Shakedown or OnlineTournament menu tree)
						bRBRTMCarSelectionMenu = true;
						break;

					case RBRTMMENUIDX_MAIN:									// Back to RBRTM main menu, so the RBRTM menu is no longer below Shakedown or OnlineTournament menu tree
						m_iRBRTMCarSelectionType = 0;
						break;

					case RBRTMMENUIDX_ONLINEOPTION1:						// RBRTM OnlineTournament menu tree
						[[fallthrough]];

					case RBRTMMENUIDX_ONLINEOPTION2:
						m_iRBRTMCarSelectionType = 1;
						m_latestMapRBRTM.latestStageResults.clear();
						break;

					case RBRTMMENUIDX_SHAKEDOWNSTAGES:						// Shakedown - Stage selection screen. Let to fall through to the next case to flag the shakedown menu type
						bRBRTMStageSelectionMenu = true;
						[[fallthrough]];

					case RBRTMMENUIDX_SHAKEDOWNOPTION1:						// RBRTM Shakedown menu tree
						m_iRBRTMCarSelectionType = 2;
						m_latestMapRBRTM.latestStageResults.clear();
						break;
					}

					if (m_pRBRTMPlugin->pRBRTMStageOptions1 == nullptr || m_pRBRTMPlugin->pRBRTMStageOptions1->selectedCarID < 0 || m_pRBRTMPlugin->pRBRTMStageOptions1->selectedCarID > 7)
						bRBRTMCarSelectionMenu = false;

					// If Shakedown stages menu is no longer active then restore the original data and delete the custom stages data (RBRTM may re-create the menu each time it is opened
					if (!bRBRTMStageSelectionMenu)
					{
						if (m_pOrigMapMenuDataRBRTM != nullptr && m_pCustomMapMenuRBRTM != nullptr && m_pOrigMapMenuDataRBRTM->pMenuItems == m_pCustomMapMenuRBRTM)
						{
							m_pOrigMapMenuDataRBRTM->pMenuItems = m_pOrigMapMenuItemsRBRTM;
							m_pOrigMapMenuDataRBRTM->numOfItems = m_origNumOfItemsMenuItemsRBRTM;
						}

						if (m_pCustomMapMenuRBRTM != nullptr)
						{
							delete[] m_pCustomMapMenuRBRTM;
							m_pCustomMapMenuRBRTM = nullptr;
						}
					}
				}

				if (bRBRTMCarSelectionMenu && (m_iRBRTMCarSelectionType == 2 || m_iRBRTMCarSelectionType == 1 /*&& m_pRBRTMPlugin->selectedItemIdx == 1)*/))
				{
					//
					// RBRTM car selection in Shakedown or OnlineTournament menu
					//

					int iCarSpecPrintRow;
					int selectedCarIdx = ::RBRAPI_MapCarIDToMenuIdx(m_pRBRTMPlugin->pRBRTMStageOptions1->selectedCarID);
					PRBRCarSelectionMenuEntry pCarSelectionMenuEntry = &g_RBRCarSelectionMenuEntry[selectedCarIdx];

					if (m_carRBRTMPreviewTexture[selectedCarIdx].pTexture == nullptr && m_carRBRTMPreviewTexture[selectedCarIdx].imgSize.cx >= 0 && g_RBRCarSelectionMenuEntry[selectedCarIdx].wszCarModel[0] != '\0')
					{
						ReadCarPreviewImageFromFile(selectedCarIdx,
							(float)m_carRBRTMPictureRect.left, (float)m_carRBRTMPictureRect.top,
							(float)(m_carRBRTMPictureRect.right - m_carRBRTMPictureRect.left),
							(float)(m_carRBRTMPictureRect.bottom - m_carRBRTMPictureRect.top),
							&m_carRBRTMPreviewTexture[selectedCarIdx],
							m_carRBRTMPictureScale /*IMAGE_TEXTURE_SCALE_PRESERVE_ASPECTRATIO | IMAGE_TEXTURE_POSITION_BOTTOM*/,
							true);
					}

					// If the car preview image is successfully initialized (imgSize.cx >= 0) and texture (=image) is prepared then draw it on the screen.
					// Shakedown (m_iRBRTMCarSelectionType==2) shows the car image all the time, but online rally shows the img only when focused menuline is the car selection line.
					if (m_carRBRTMPreviewTexture[selectedCarIdx].imgSize.cx >= 0 
						&& m_carRBRTMPreviewTexture[selectedCarIdx].pTexture != nullptr
						&& (m_iRBRTMCarSelectionType == 2 || (m_iRBRTMCarSelectionType == 1 && m_pRBRTMPlugin->selectedItemIdx == 1))
					)
					{
						iCarSpecPrintRow = 0;

						iFontHeight = g_pFontCarSpecModel->GetTextHeight();

						if (m_carRBRTMPictureUseTransparent)
							m_pD3D9RenderStateCache->EnableTransparentAlphaBlending();

						D3D9DrawVertexTex2D(g_pRBRIDirect3DDevice9, m_carRBRTMPreviewTexture[selectedCarIdx].pTexture, m_carRBRTMPreviewTexture[selectedCarIdx].vertexes2D);

						if (m_carRBRTMPictureUseTransparent)
							m_pD3D9RenderStateCache->RestoreState();

						// 3D model and custom livery and FMOD authors text is drawn on top of the car preview image (bottom left corner)
						if (pCarSelectionMenuEntry->wszCarFMODBank[0] != L'\0' && pCarSelectionMenuEntry->wszCarFMODBankAuthors[0] != L'\0')
							g_pFontCarSpecModel->DrawText(m_carRBRTMPictureRect.left + 2, m_carRBRTMPictureRect.bottom - ((++iCarSpecPrintRow) * iFontHeight) - 4, 
								C_CARSPECTEXT_COLOR, 
								(std::wstring(pCarSelectionMenuEntry->wszCarFMODBank) + L" by " + pCarSelectionMenuEntry->wszCarFMODBankAuthors).c_str(), 0);

						if (pCarSelectionMenuEntry->wszCarPhysicsLivery[0] != L'\0')
							g_pFontCarSpecModel->DrawText(m_carRBRTMPictureRect.left + 2, m_carRBRTMPictureRect.bottom - ((++iCarSpecPrintRow) * iFontHeight) - 4, C_CARSPECTEXT_COLOR, pCarSelectionMenuEntry->wszCarPhysicsLivery, 0);

						if (pCarSelectionMenuEntry->wszCarPhysics3DModel[0] != L'\0')
							g_pFontCarSpecModel->DrawText(m_carRBRTMPictureRect.left + 2, m_carRBRTMPictureRect.bottom - ((++iCarSpecPrintRow) * iFontHeight) - 4, C_CARSPECTEXT_COLOR, pCarSelectionMenuEntry->wszCarPhysics3DModel, 0);
					}

					iFontHeight = g_pFontCarSpecCustom->GetTextHeight();

					// FIACategory, HP, Year, Weight, Transmission, AudioFMOD
					iCarSpecPrintRow = 0;
					//posX = g_pRBRPlugin->m_carRBRTMPictureRect.right + 16;
					RBRAPI_MapRBRPointToScreenPoint(460.0f, 0.0f, &posX, nullptr);

					if (pCarSelectionMenuEntry->wszCarFMODBank[0] != L'\0')
						g_pFontCarSpecCustom->DrawText(posX, m_carRBRTMPictureRect.bottom - ((++iCarSpecPrintRow) * iFontHeight) - 4, C_CARSPECTEXT_COLOR, pCarSelectionMenuEntry->wszCarFMODBank, 0);

					if (pCarSelectionMenuEntry->wszCarYear[0] != L'\0')
						g_pFontCarSpecCustom->DrawText(posX, m_carRBRTMPictureRect.bottom - ((++iCarSpecPrintRow) * iFontHeight) - 4, C_CARSPECTEXT_COLOR, pCarSelectionMenuEntry->wszCarYear, 0);

					if (pCarSelectionMenuEntry->wszCarTrans[0] != L'\0')
						g_pFontCarSpecCustom->DrawText(posX, m_carRBRTMPictureRect.bottom - ((++iCarSpecPrintRow) * iFontHeight) - 4, C_CARSPECTEXT_COLOR, pCarSelectionMenuEntry->wszCarTrans, 0);

					if (pCarSelectionMenuEntry->wszCarWeight[0] != L'\0')
						g_pFontCarSpecCustom->DrawText(posX, m_carRBRTMPictureRect.bottom - ((++iCarSpecPrintRow) * iFontHeight) - 4, C_CARSPECTEXT_COLOR, pCarSelectionMenuEntry->wszCarWeight, 0);

					if (pCarSelectionMenuEntry->wszCarPower[0] != L'\0')
						g_pFontCarSpecCustom->DrawText(posX, m_carRBRTMPictureRect.bottom - ((++iCarSpecPrintRow) * iFontHeight) - 4, C_CARSPECTEXT_COLOR, pCarSelectionMenuEntry->wszCarPower, 0);

					if (pCarSelectionMenuEntry->szCarCategory[0] != L'\0')
						g_pFontCarSpecCustom->DrawText(posX, m_carRBRTMPictureRect.bottom - ((++iCarSpecPrintRow) * iFontHeight) - 4, C_CARSPECTEXT_COLOR, pCarSelectionMenuEntry->szCarCategory, 0);

					if (pCarSelectionMenuEntry->wszCarModel[0] != L'\0')
						g_pFontCarSpecCustom->DrawText(posX, m_carRBRTMPictureRect.bottom - ((++iCarSpecPrintRow) * iFontHeight) - 4, C_CARMODELTITLETEXT_COLOR, pCarSelectionMenuEntry->wszCarModel, 0);

				}
				else if (bRBRTMStageSelectionMenu && m_iRBRTMCarSelectionType == 2
					&& m_pTracksIniFile != nullptr
					&& m_pRBRTMPlugin->pCurrentRBRTMMenuObj->pMenuData != nullptr
					&& m_pRBRTMPlugin->pCurrentRBRTMMenuObj->pMenuData->pMenuItems != nullptr
					&& m_pRBRTMPlugin->pCurrentRBRTMMenuObj->pMenuData->numOfItems > 0
					&& m_pRBRTMPlugin->selectedItemIdx >= 0)
				{
					//
					// RBRTM stage selection list in Shakedown menu is the active RBRTM menu
					//

					// Hide the annoying "moving background box" on the bottom right corner in RBRTM Shakedown stages menu
					g_pRBRMenuSystem->menuImageHeight = g_pRBRMenuSystem->menuImageWidth = 0;

					if (m_pRBRTMPlugin->selectedItemIdx < m_pRBRTMPlugin->pCurrentRBRTMMenuObj->pMenuData->numOfItems)
					{
						if (m_pCustomMapMenuRBRTM == nullptr)
						{
							int numOfRecentMaps = RBRTM_CalculateNumOfValidMapsInRecentList(m_pRBRTMPlugin->pCurrentRBRTMMenuObj->pMenuData);

							// Initialization of custom RBRTM stages menu list in Shakedown menu (Nth recent stages on top of the menu and then the original RBRTM stage list).
							// Use the custom list if there are recent mapIDs in the list and the feature is enabled (recent items max count > 0)
							m_pCustomMapMenuRBRTM = new RBRTMMenuItem[m_pRBRTMPlugin->pCurrentRBRTMMenuObj->pMenuData->numOfItems + numOfRecentMaps];

							if (m_pCustomMapMenuRBRTM != nullptr && numOfRecentMaps > 0 && m_recentMapsMaxCountRBRTM > 0)
							{
								m_pOrigMapMenuDataRBRTM = m_pRBRTMPlugin->pCurrentRBRTMMenuObj->pMenuData;
								m_pOrigMapMenuItemsRBRTM = m_pRBRTMPlugin->pCurrentRBRTMMenuObj->pMenuData->pMenuItems;
								m_origNumOfItemsMenuItemsRBRTM = m_pRBRTMPlugin->pCurrentRBRTMMenuObj->pMenuData->numOfItems;

								m_numOfItemsCustomMapMenuRBRTM = m_origNumOfItemsMenuItemsRBRTM + numOfRecentMaps;

								// Clear shortcut stage items and copy the original RBRTM stages list at the end of custom menu list
								ZeroMemory(m_pCustomMapMenuRBRTM, sizeof(RBRTMMenuItem) * numOfRecentMaps);
								memcpy(&m_pCustomMapMenuRBRTM[numOfRecentMaps], m_pRBRTMPlugin->pCurrentRBRTMMenuObj->pMenuData->pMenuItems, m_origNumOfItemsMenuItemsRBRTM * sizeof(RBRTMMenuItem));

								// Activate the custom stages menu
								m_pRBRTMPlugin->pCurrentRBRTMMenuObj->pMenuData->pMenuItems = m_pCustomMapMenuRBRTM;
								m_pRBRTMPlugin->pCurrentRBRTMMenuObj->pMenuData->numOfItems = m_numOfItemsCustomMapMenuRBRTM;
								m_pRBRTMPlugin->numOfMenuItems = m_numOfItemsCustomMapMenuRBRTM + 1;

								numOfRecentMaps = 0;
								for (auto& item : m_recentMapsRBRTM)
								{
									// At this point there are only max number of valid mapID values in the recent list (CalculateNumOfValidMapsInRecentList removed invalid and extra items)
									m_pCustomMapMenuRBRTM[numOfRecentMaps].mapID = item->mapID;
									m_pCustomMapMenuRBRTM[numOfRecentMaps].wszMenuItemName = item->name.c_str();
									numOfRecentMaps++;
								}
							}


							// Activate the latest mapID menu row automatically (by default RBRTM would wrap back to the first stage menu line when user navigates back to Shakedown menu, very annoying)
							if (m_latestMapRBRTM.mapID > 0 && m_pCustomMapMenuRBRTM != nullptr)
							{
								// If the latest menuIdx is set then check the array by index access before trying to search through all menu items
								if (m_latestMapRBRTM.mapIDMenuIdx >= 0 && m_latestMapRBRTM.mapIDMenuIdx < m_numOfItemsCustomMapMenuRBRTM
									&& m_pRBRTMPlugin->pCurrentRBRTMMenuObj->pMenuData->pMenuItems[m_latestMapRBRTM.mapIDMenuIdx].mapID == m_latestMapRBRTM.mapID)
								{
									m_pRBRTMPlugin->selectedItemIdx = m_latestMapRBRTM.mapIDMenuIdx;
								}
								else
								{
									menuIdx = RBRTM_FindMenuItemIdxByMapID(m_pRBRTMPlugin->pCurrentRBRTMMenuObj->pMenuData, m_latestMapRBRTM.mapID);
									if (menuIdx >= 0)
										m_pRBRTMPlugin->selectedItemIdx = menuIdx;
								}
							}

							// Shakedown stages menu is open, save recentMaps INI options when a stage is chosen for racing and the recent list is modified
							m_bRecentMapsRBRTMModified = TRUE;
						}


						// If the mapID is different than the latest mapID then load new details (stage name, length, surface, previewImage). Or if the prev map preview image was shown in Shakdown options screen then re-init stage selection image.
						if (m_latestMapRBRTM.mapID != m_pRBRTMPlugin->pCurrentRBRTMMenuObj->pMenuData->pMenuItems[m_pRBRTMPlugin->selectedItemIdx].mapID || m_latestMapRBRTM.shakedownOptionsFirstTimeSetup == FALSE)
						{
							WCHAR wszMapINISection[16];

							m_latestMapRBRTM.mapID = m_pRBRTMPlugin->pCurrentRBRTMMenuObj->pMenuData->pMenuItems[m_pRBRTMPlugin->selectedItemIdx].mapID;
							m_latestMapRBRTM.mapIDMenuIdx = m_pRBRTMPlugin->selectedItemIdx;
							m_latestMapRBRTM.shakedownOptionsFirstTimeSetup = TRUE;

							swprintf_s(wszMapINISection, COUNT_OF_ITEMS(wszMapINISection), L"Map%02d", m_latestMapRBRTM.mapID);

							// At first lookup the stage name from maps\Tracks.ini file. If the name is not set there then re-use the stage name used in RBRTM menus							
							//m_latestMapRBRTM.name = _RemoveEnclosingChar(m_pTracksIniFile->GetValue(wszMapINISection, L"StageName", L""), L'"', false);
							m_latestMapRBRTM.name = GetMapNameByMapID(m_latestMapRBRTM.mapID);
							if (m_latestMapRBRTM.name.empty())
								m_latestMapRBRTM.name = m_pRBRTMPlugin->pCurrentRBRTMMenuObj->pMenuData->pMenuItems[m_pRBRTMPlugin->selectedItemIdx].wszMenuItemName;

							// Take stage len and surface type from maps\Tracks.ini file
							m_latestMapRBRTM.length = m_pTracksIniFile->GetDoubleValue(wszMapINISection, L"Length", -1.0);

							// Check surface type for the original map. If the map was not original then check Tracks.ini file Surface option
							m_latestMapRBRTM.surface = NPlugin::GetStageSurface(m_latestMapRBRTM.mapID);
							if (m_latestMapRBRTM.surface < 0) m_latestMapRBRTM.surface = m_pTracksIniFile->GetLongValue(wszMapINISection, L"Surface", -1);

							// Skip map image initializations if the map preview img feature is disabled (RBRTM_MapPictureRect=0)
							if (m_mapRBRTMPictureRect[0].bottom != -1)
							{
								// Use custom map image path at first (set in RBRTM_MapScreenshotPath ini option). If the option or file is missing then take the stage preview image name from maps\Tracks.ini file
								m_latestMapRBRTM.previewImageFile = ReplacePathVariables(m_screenshotPathMapRBRTM, -1, TRUE, m_latestMapRBRTM.mapID, m_latestMapRBRTM.name.c_str());

								if (g_iLogMsgCount < 26)
									LogPrint(L"Custom image %s for the map #%d %s", m_latestMapRBRTM.previewImageFile.c_str(), m_latestMapRBRTM.mapID, m_latestMapRBRTM.name.c_str());

								if (m_latestMapRBRTM.previewImageFile.empty() || !fs::exists(m_latestMapRBRTM.previewImageFile))
								{
									m_latestMapRBRTM.previewImageFile = _RemoveEnclosingChar(m_pTracksIniFile->GetValue(wszMapINISection, L"SplashScreen", L""), L'"', false);

									if (m_latestMapRBRTM.previewImageFile.length() >= 2 && m_latestMapRBRTM.previewImageFile[0] != L'\\' && m_latestMapRBRTM.previewImageFile[1] != L':')
										m_latestMapRBRTM.previewImageFile = this->m_sRBRRootDirW + L"\\" + m_latestMapRBRTM.previewImageFile;

									if (g_iLogMsgCount < 26)
										LogPrint(L"Custom image not found. Using Maps\\Tracks.ini SplashScreen option %s", m_latestMapRBRTM.previewImageFile.c_str());
								}
							}

							// Release previous map preview texture and read a new image file (if preview path is set and the image file exists and map preview img drawing is not disabled)
							SAFE_RELEASE(m_latestMapRBRTM.imageTexture.pTexture);
							if (!m_latestMapRBRTM.previewImageFile.empty() && fs::exists(m_latestMapRBRTM.previewImageFile) && m_mapRBRTMPictureRect[0].bottom != -1)
							{
								hResult = D3D9CreateRectangleVertexTexBufferFromFile(g_pRBRIDirect3DDevice9,
									m_latestMapRBRTM.previewImageFile,
									(float)m_mapRBRTMPictureRect[0].left, (float)m_mapRBRTMPictureRect[0].top, (float)(m_mapRBRTMPictureRect[0].right - m_mapRBRTMPictureRect[0].left), (float)(m_mapRBRTMPictureRect[0].bottom - m_mapRBRTMPictureRect[0].top),
									&m_latestMapRBRTM.imageTexture,
									0 /*IMAGE_TEXTURE_PRESERVE_ASPECTRATIO_BOTTOM | IMAGE_TEXTURE_POSITION_HORIZONTAL_CENTER*/ );

								// Image not available or loading failed
								if (!SUCCEEDED(hResult))
									SAFE_RELEASE(m_latestMapRBRTM.imageTexture.pTexture);
							}
						}

						// Show details of the current stage (name, length, surface, previewImage)
						iFontHeight = g_pFontCarSpecCustom->GetTextHeight();

						std::wstringstream sStrStream;
						sStrStream << std::fixed << std::setprecision(1);

						//posX = m_mapRBRTMPictureRect.left;
						//posY = m_mapRBRTMPictureRect.top - (4 * iFontHeight);
						RBRAPI_MapRBRPointToScreenPoint(295.0f, 53.0f, &posX, &posY);

						//int iMapInfoPrintRow = 0;
						//g_pFontCarSpecCustom->DrawText(posX, posY + ((++iMapInfoPrintRow) * iFontHeight), C_CARMODELTITLETEXT_COLOR, (m_latestMapRBRTM.name + L"  (#" + std::to_wstring(g_pRBRPlugin->m_latestMapRBRTM.mapID) + L")").c_str(), 0);
						sStrStream << m_latestMapRBRTM.name << L"  (#" << g_pRBRPlugin->m_latestMapRBRTM.mapID << L")   ";

						if (m_latestMapRBRTM.length > 0)
							// TODO: KM to Miles miles=km*0.621371192 config option support
							sStrStream << m_latestMapRBRTM.length << L" km ";

						if (m_latestMapRBRTM.surface >= 0 && m_latestMapRBRTM.surface <= 2)
							sStrStream << GetLangStr(NPlugin::GetSurfaceName(m_latestMapRBRTM.surface));

						//g_pFontCarSpecCustom->DrawText(posX, posY + ((++iMapInfoPrintRow) * iFontHeight), C_CARSPECTEXT_COLOR, sStrStream.str().c_str(), 0);
						g_pFontCarSpecCustom->DrawText(posX, posY, C_CARMODELTITLETEXT_COLOR, sStrStream.str().c_str(), 0);
						
						if (m_latestMapRBRTM.imageTexture.pTexture != nullptr)
						{
							m_pD3D9RenderStateCache->EnableTransparentAlphaBlending();
							D3D9DrawVertexTex2D(g_pRBRIDirect3DDevice9, m_latestMapRBRTM.imageTexture.pTexture, m_latestMapRBRTM.imageTexture.vertexes2D, m_pD3D9RenderStateCache);
							m_pD3D9RenderStateCache->RestoreState();
						}
						// Draw minimap in RBRTM shakedown stages menu list
						RBRTM_DrawMinimap(m_latestMapRBRTM.mapID, 0);					
					}
					else
					{
						// "Back" menu line selected in Shakedown stage list. Clear the "latest mapID" cache value
						m_latestMapRBRTM.mapID = -1;
					}
				}
				else if (m_iRBRTMCarSelectionType == 2 && m_pTracksIniFile != nullptr && m_pRBRTMPlugin->pCurrentRBRTMMenuObj->menuID == RBRTMMENUIDX_SHAKEDOWNOPTION1)
				{
					//
					// RBRTM stage option screen in Shakedown is active (ie the screen after the Shakedown stage selection)
					//

					// Hide the annoying "moving background box" on the bottom right corner in RBRTM Shakedown stages menu
					g_pRBRMenuSystem->menuImageHeight = g_pRBRMenuSystem->menuImageWidth = 0;

					if (m_latestMapRBRTM.shakedownOptionsFirstTimeSetup)
					{
						m_latestMapRBRTM.shakedownOptionsFirstTimeSetup = FALSE;

						// Release previous map preview texture and read a new image file (if preview path is set and the image file exists and map preview img drawing is not disabled)
						SAFE_RELEASE(m_latestMapRBRTM.imageTexture.pTexture);
						if (!m_latestMapRBRTM.previewImageFile.empty() && fs::exists(m_latestMapRBRTM.previewImageFile) && m_mapRBRTMPictureRect[1].bottom != -1)
						{
							//float posLeft, posTop, posRight, posBottom;

							// Stretch to fill the whole area below menu lines
							//RBRAPI_MapRBRPointToScreenPoint(5.0f, 185.0f, &posLeft,&posTop);
							//RBRAPI_MapRBRPointToScreenPoint(635.0f, 461.0f, &posRight, &posBottom);
							
							// Slightly stretched to fill the right side of the screen below menu lines (map style=0, default)
							// dwFlags=0
							//RBRAPI_MapRBRPointToScreenPoint(200.0f, 140.0f, &posLeft, &posTop);
							//RBRAPI_MapRBRPointToScreenPoint(640.0f, 462.0f, &posRight, &posBottom);

							// Optimal for 1024x1024 keepAspectRatio images (map style=1)
							// dwFlags=IMAGE_TEXTURE_PRESERVE_ASPECTRATIO_BOTTOM | IMAGE_TEXTURE_POSITION_HORIZONTAL_RIGHT
							//RBRAPI_MapRBRPointToScreenPoint(340.0f, 130.0f, &posLeft, &posTop);
							//RBRAPI_MapRBRPointToScreenPoint(640.0f, 462.0f, &posRight, &posBottom);

							hResult = D3D9CreateRectangleVertexTexBufferFromFile(g_pRBRIDirect3DDevice9,
								m_latestMapRBRTM.previewImageFile,
								//posLeft, posTop, posRight - posLeft, posBottom - posTop,
								(float)m_mapRBRTMPictureRect[1].left, (float)m_mapRBRTMPictureRect[1].top, (float)(m_mapRBRTMPictureRect[1].right - m_mapRBRTMPictureRect[1].left), (float)(m_mapRBRTMPictureRect[1].bottom - m_mapRBRTMPictureRect[1].top),
								&m_latestMapRBRTM.imageTexture,
								0 
							    //IMAGE_TEXTURE_PRESERVE_ASPECTRATIO_BOTTOM | IMAGE_TEXTURE_POSITION_HORIZONTAL_RIGHT
							);

							// Image not available or loading failed
							if (!SUCCEEDED(hResult))
								SAFE_RELEASE(m_latestMapRBRTM.imageTexture.pTexture);
						}
					}

					std::wstringstream sStrStream;
					sStrStream << std::fixed << std::setprecision(1);
					RBRAPI_MapRBRPointToScreenPoint(200.0f, 40.0f, &posX, &posY);

					sStrStream << m_latestMapRBRTM.name << L"  (#" << g_pRBRPlugin->m_latestMapRBRTM.mapID << L")   ";

					if (m_latestMapRBRTM.length > 0)
						// TODO: KM to Miles miles=km*0.621371192 config option support
						sStrStream << m_latestMapRBRTM.length << L" km ";

					if (m_latestMapRBRTM.surface >= 0 && m_latestMapRBRTM.surface <= 2)
						sStrStream << GetLangStr(NPlugin::GetSurfaceName(m_latestMapRBRTM.surface));

					g_pFontCarSpecCustom->DrawText(posX, posY, C_CARMODELTITLETEXT_COLOR, sStrStream.str().c_str(), 0);

					if (m_latestMapRBRTM.imageTexture.pTexture != nullptr)
					{
						m_pD3D9RenderStateCache->EnableTransparentAlphaBlending();
						D3D9DrawVertexTex2D(g_pRBRIDirect3DDevice9, m_latestMapRBRTM.imageTexture.pTexture, m_latestMapRBRTM.imageTexture.vertexes2D, m_pD3D9RenderStateCache);
						m_pD3D9RenderStateCache->RestoreState();
					}

					// Draw minimap in RBRTM shakedown stages menu list
					RBRTM_DrawMinimap(m_latestMapRBRTM.mapID, 1);
				}
				else if (m_pRBRTMPlugin->pCurrentRBRTMMenuObj != nullptr && m_pRBRTMPlugin->pCurrentRBRTMMenuObj->menuID == RBRTMMENUIDX_MAIN)
				{
					//
					// RBRTM main menu
					//

					// Hide the annoying "moving background box" on the bottom right corner in RBRTM main menu
					g_pRBRMenuSystem->menuImageHeight = g_pRBRMenuSystem->menuImageWidth = 0;

					// Refresh the list of latest stages if the vector list doesn't have any results
					if(m_latestMapRBRTM.latestStageResults.size() <= 0 && m_recentResultsPosition_RBRTM.y != -1)
						RaceStatDB_QueryLastestStageResults(-1, "", 1, m_latestMapRBRTM.latestStageResults);

					// Draw the list of recent race results on RBRTM main menu screen
					if (m_latestMapRBRTM.latestStageResults.size() > 0)
					{
						int posX = m_recentResultsPosition_RBRTM.x;
						int posY = m_recentResultsPosition_RBRTM.y;

						DrawRecentResultsTable(posX, posY, m_latestMapRBRTM.latestStageResults);
					}
				}
			}
		}
		else if (/*!m_bRBRTMPluginActive &&*/ m_pRBRTMPlugin != nullptr && m_pOrigMapMenuDataRBRTM != nullptr && m_pCustomMapMenuRBRTM != nullptr
			&& m_pOrigMapMenuDataRBRTM->pMenuItems == m_pCustomMapMenuRBRTM
			&& g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_MAIN])
		{
			// RBRTM is no longer active, but the custom menu is created. Delete it and restore the original RBRTM menu objects (if RBR app is closed then these objects need to be the original pointers, because RBRTM releases the ptr memory block)
			m_pOrigMapMenuDataRBRTM->pMenuItems = m_pOrigMapMenuItemsRBRTM;
			m_pOrigMapMenuDataRBRTM->numOfItems = m_origNumOfItemsMenuItemsRBRTM;

			if (m_pCustomMapMenuRBRTM != nullptr)
			{
				delete[] m_pCustomMapMenuRBRTM;
				m_pCustomMapMenuRBRTM = nullptr;
			}
		}

	}

/*
	else if (g_pRBRGameMode->gameMode == 0x0D)
	{
		if (m_bRecentMapsRBRTMModified && m_bRBRTMPluginActive && m_iRBRTMCarSelectionType == 2)
		{
			// Stage loading while RBRTM plugin is active in Shakedown mode. Add the latest mapID (=stage) to the top of the recent list
			if (m_recentMapsMaxCountRBRTM > 0)
			{
				m_bRecentMapsRBRTMModified = FALSE;
				RBRTM_AddMapToRecentList(m_latestMapRBRTM.mapID);
				if (m_bRecentMapsRBRTMModified) SaveSettingsToRBRTMRecentMaps();
			}

			m_bRecentMapsRBRTMModified = FALSE;
		}
	}
*/
}

