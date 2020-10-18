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
						break;

					case RBRTMMENUIDX_SHAKEDOWNSTAGES:						// Shakedown - Stage selection screen. Let to fall through to the next case to flag the shakedown menu type
						bRBRTMStageSelectionMenu = true;
						[[fallthrough]];

					case RBRTMMENUIDX_SHAKEDOWNOPTION1:						// RBRTM Shakedown menu tree
						m_iRBRTMCarSelectionType = 2;
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

				if (bRBRTMCarSelectionMenu && (m_iRBRTMCarSelectionType == 2 || (m_iRBRTMCarSelectionType == 1 && m_pRBRTMPlugin->selectedItemIdx == 1)))
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

					// If the car preview image is successfully initialized (imgSize.cx >= 0) and texture (=image) is prepared then draw it on the screen
					if (m_carRBRTMPreviewTexture[selectedCarIdx].imgSize.cx >= 0 && m_carRBRTMPreviewTexture[selectedCarIdx].pTexture != nullptr)
					{
						iCarSpecPrintRow = 0;

						iFontHeight = g_pFontCarSpecModel->GetTextHeight();

						if (m_carRBRTMPictureUseTransparent)
							m_pD3D9RenderStateCache->EnableTransparentAlphaBlending();

						D3D9DrawVertexTex2D(g_pRBRIDirect3DDevice9, m_carRBRTMPreviewTexture[selectedCarIdx].pTexture, m_carRBRTMPreviewTexture[selectedCarIdx].vertexes2D);

						if (m_carRBRTMPictureUseTransparent)
							m_pD3D9RenderStateCache->RestoreState();

						// 3D model and custom livery text is drawn on top of the car preview image (bottom left corner)
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
							int numOfRecentMaps = CalculateNumOfValidMapsInRecentList(m_pRBRTMPlugin->pCurrentRBRTMMenuObj->pMenuData);

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
									menuIdx = FindRBRTMMenuItemIdxByMapID(m_pRBRTMPlugin->pCurrentRBRTMMenuObj->pMenuData, m_latestMapRBRTM.mapID);
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
							m_latestMapRBRTM.name = _RemoveEnclosingChar(m_pTracksIniFile->GetValue(wszMapINISection, L"StageName", L""), L'"', false);
							if (m_latestMapRBRTM.name.empty())
								m_latestMapRBRTM.name = m_pRBRTMPlugin->pCurrentRBRTMMenuObj->pMenuData->pMenuItems[m_pRBRTMPlugin->selectedItemIdx].wszMenuItemName;

							// Take stage len and surface type from maps\Tracks.ini file
							m_latestMapRBRTM.length = m_pTracksIniFile->GetDoubleValue(wszMapINISection, L"Length", -1.0);

							// Check surface type for the original map. If the map was not original then check Tracks.ini file Surface option
							m_latestMapRBRTM.surface = NPlugin::GetStageSurface(m_latestMapRBRTM.mapID);
							if (m_latestMapRBRTM.surface < 0) m_latestMapRBRTM.surface = m_pTracksIniFile->GetLongValue(wszMapINISection, L"Surface", -1);

							// Skip map image initializations if the map preview img feature is disabled (RBRTM_MapPictureRect=0)
							if (m_mapRBRTMPictureRect.bottom != -1)
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
							if (!m_latestMapRBRTM.previewImageFile.empty() && fs::exists(m_latestMapRBRTM.previewImageFile) && m_mapRBRTMPictureRect.bottom != -1)
							{
								hResult = D3D9CreateRectangleVertexTexBufferFromFile(g_pRBRIDirect3DDevice9,
									m_latestMapRBRTM.previewImageFile,
									(float)m_mapRBRTMPictureRect.left, (float)m_mapRBRTMPictureRect.top, (float)(m_mapRBRTMPictureRect.right - m_mapRBRTMPictureRect.left), (float)(m_mapRBRTMPictureRect.bottom - m_mapRBRTMPictureRect.top),
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
							D3D9DrawVertexTex2D(g_pRBRIDirect3DDevice9, m_latestMapRBRTM.imageTexture.pTexture, m_latestMapRBRTM.imageTexture.vertexes2D);
							m_pD3D9RenderStateCache->RestoreState();
						}
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
						if (!m_latestMapRBRTM.previewImageFile.empty() && fs::exists(m_latestMapRBRTM.previewImageFile) && m_mapRBRTMPictureRect.bottom != -1)
						{
							float posLeft, posTop, posRight, posBottom;
							RBRAPI_MapRBRPointToScreenPoint(5.0f, 185.0f, &posLeft, &posTop);
							RBRAPI_MapRBRPointToScreenPoint(635.0f, 461.0f, &posRight, &posBottom);

							hResult = D3D9CreateRectangleVertexTexBufferFromFile(g_pRBRIDirect3DDevice9,
								m_latestMapRBRTM.previewImageFile,
								posLeft, posTop, posRight - posLeft, posBottom - posTop,
								&m_latestMapRBRTM.imageTexture,
								0 /*IMAGE_TEXTURE_PRESERVE_ASPECTRATIO_BOTTOM | IMAGE_TEXTURE_POSITION_HORIZONTAL_CENTER*/);

							// Image not available or loading failed
							if (!SUCCEEDED(hResult))
								SAFE_RELEASE(m_latestMapRBRTM.imageTexture.pTexture);
						}
					}

					if (m_latestMapRBRTM.imageTexture.pTexture != nullptr)
					{
						m_pD3D9RenderStateCache->EnableTransparentAlphaBlending();
						D3D9DrawVertexTex2D(g_pRBRIDirect3DDevice9, m_latestMapRBRTM.imageTexture.pTexture, m_latestMapRBRTM.imageTexture.vertexes2D);
						m_pD3D9RenderStateCache->RestoreState();
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

	else if (g_pRBRGameMode->gameMode == 0x0D /*10*/)
	{
		if (m_bRecentMapsRBRTMModified && m_bRBRTMPluginActive && m_iRBRTMCarSelectionType == 2)
		{
			// Stage loading while RBRTM plugin is active in Shakedown mode. Add the latest mapID (=stage) to the top of the recent list
			if (m_recentMapsMaxCountRBRTM > 0)
			{
				m_bRecentMapsRBRTMModified = FALSE;
				AddMapToRecentList(m_latestMapRBRTM.mapID);
				if (m_bRecentMapsRBRTMModified) SaveSettingsToRBRTMRecentMaps();
			}

			m_bRecentMapsRBRTMModified = FALSE;
		}
	}
}

