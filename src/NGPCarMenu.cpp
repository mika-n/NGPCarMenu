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
CD3DFont* pFontDebug = nullptr;
#endif 

CD3DFont* pFontCarSpecCustom = nullptr;

CNGPCarMenu*         g_pRBRPlugin = nullptr;				// The one and only RBRPlugin instance

PRBRGameConfig		 g_pRBRGameConfig = nullptr;			// Various RBR-API struct pointers
PRBRGameMode		 g_pRBRGameMode = nullptr;
PRBRGameModeExt		 g_pRBRGameModeExt = nullptr;
PRBRCarInfo			 g_pRBRCarInfo = nullptr;
PRBRCameraInfo		 g_pRBRCameraInfo = nullptr;
PRBRCarControls		 g_pRBRCarControls = nullptr;

PRBRCarMovement		 g_pRBRCarMovement = nullptr;
PRBRMapInfo			 g_pRBRMapInfo = nullptr;
PRBRMapSettings		 g_pRBRMapSettings = nullptr;
PRBRGhostCarMovement g_pRBRGhostCarMovement = nullptr;

PRBRMenuSystem		g_pRBRMenuSystem = nullptr;

WCHAR* g_pOrigCarSpecTitleWeight = nullptr;				// The original RBR Weight and Transmission string values
WCHAR* g_pOrigCarSpecTitleTransmission = nullptr;

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
	{ 0x4a0dd9, 0x4a0c59, /* Slot#5 */ "", L"car1", "", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ 0x4a0dc9, 0x4a0c49, /* Slot#3 */ "", L"car2", "", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ 0x4a0de1, 0x4a0c61, /* Slot#6 */ "", L"car3",	"", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ 0x4a0db9, 0x4a0c39, /* Slot#1 */ "", L"car4",	"", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ 0x4a0dd1, 0x4a0c51, /* Slot#4 */ "", L"car5",	"", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ 0x4a0de9, 0x4a0c69, /* Slot#7 */ "", L"car6",	"", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ 0x4a0db1, 0x4a0c31, /* Slot#0 */ "", L"car7",	"", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ 0x4a0dc1, 0x4a0c41, /* Slot#2 */ "", L"car8",	"", L"", L"", L"", L"", L"", L"", L"", L"" }
};


// Menu item names (custom plugin menu)
char* g_RBRPluginMenu[3] = {
	 "RELOAD car images"	// Clear cached car images to force re-loading of new images
	,"Create option"		// CreateOptions
	,"CREATE car images"	// Create new car images (all or only missing car iamges)
};

// CreateOption menu option names (option values in "Create option")
char* g_RBRPluginMenu_CreateOptions[2] = {
	"Only missing car images"
	,"All car images"
};


//-----------------------------------------------------------------------------------------------------------------------------
BOOL APIENTRY DllMain( HANDLE hModule,  DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    return TRUE;
}

IPlugin* RBR_CreatePlugin( IRBRGame* pGame )
{
#if USE_DEBUG == 1	
	DebugPrintEmptyFile();
#endif
	DebugPrint("--------------------------------------------\nEnter RBR_CreatePlugin");

	if (g_pRBRPlugin == nullptr) g_pRBRPlugin = new CNGPCarMenu(pGame);

	DebugPrint("Exit RBR_CreatePlugin");

	return g_pRBRPlugin;
}

//----------------------------------------------------------------------------------------------------------------------------

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

	m_iCarMenuNameLen = 0;
	
	m_iCustomReplayCarID = 0;
	m_iCustomReplayState = 0;
	m_iCustomReplayScreenshotCount = 0;
	m_bCustomReplayShowCroppingRect = false;

	m_screenshotCroppingRectVertexBuffer = nullptr;
	memset(m_carPreviewTexture, 0, sizeof(m_carPreviewTexture));

	m_pGame = pGame;
	m_iMenuSelection = 0;
	m_iMenuCreateOption = 0;

	RefreshSettingsFromPluginINIFile();
	InitCarSpecData();
	CalculateMaxLenCarMenuName();

	DebugPrint("Exit CNGPCarMenu.Constructor");
}

CNGPCarMenu::~CNGPCarMenu(void)
{
	g_bRBRHooksInitialized = FALSE;
	m_PluginState = T_PLUGINSTATE::PLUGINSTATE_CLOSING;

	DebugPrint("Enter CNGPCarMenu.Destructor");

	if (gtcDirect3DBeginScene != nullptr) delete gtcDirect3DBeginScene;
	if (gtcDirect3DEndScene != nullptr) delete gtcDirect3DEndScene;

#if USE_DEBUG == 1
	if (pFontDebug != nullptr) delete pFontDebug;
#endif 

	if (pFontCarSpecCustom != nullptr) delete pFontCarSpecCustom;

	SAFE_RELEASE(m_screenshotCroppingRectVertexBuffer);
	ClearCachedCarPreviewImages();

	if (g_pRBRPlugin == this) g_pRBRPlugin = nullptr;

	DebugPrint("Exit CNGPCarMenu.Destructor");
}

void CNGPCarMenu::ClearCachedCarPreviewImages()
{
	for (int idx = 0; idx < 8; idx++)
		SAFE_RELEASE(m_carPreviewTexture[idx].pTexture);

	memset(m_carPreviewTexture, 0, sizeof(m_carPreviewTexture));
}

//-------------------------------------------------------------------------------------------------
// Refresh pluging settings from INI file. This method can be called at any time to refresh values even at plugin runtime.
//
void CNGPCarMenu::RefreshSettingsFromPluginINIFile()
{
	WCHAR szResolutionText[16];

	CSimpleIniW  pluginINIFile;
	std::wstring sTextValue;

	std::string  sIniFileName = CNGPCarMenu::m_sRBRRootDir + "\\Plugins\\NGPCarMenu.ini";
	pluginINIFile.LoadFile(sIniFileName.c_str());

	this->m_screenshotReplayFileName = pluginINIFile.GetValue(L"Default", L"ScreenshotReplay", L"");
	_Trim(this->m_screenshotReplayFileName);

	swprintf_s(szResolutionText, COUNT_OF_ITEMS(szResolutionText), L"%dx%d", g_rectRBRWndClient.right, g_rectRBRWndClient.bottom);

	this->m_screenshotPath = pluginINIFile.GetValue(L"Default", L"ScreenshotPath", L"");
	_Trim(this->m_screenshotPath);
	if (this->m_screenshotPath.length() >= 2 && this->m_screenshotPath[0] != '\\' && this->m_screenshotPath[1] != ':')
	{
		this->m_screenshotPath = this->m_sRBRRootDirW + L"\\" + this->m_screenshotPath + L"\\" + szResolutionText;

		try
		{
			// If the rbr\plugins\NGPCarMenu\preview\<resolution>\ subfolder is missing then create it now
			if (!this->m_screenshotPath.empty() && !fs::exists(this->m_screenshotPath))
				fs::create_directory(this->m_screenshotPath);
		}
		catch (...)
		{
			// Do nothing.
			// TODO. Add some error logging to release build
		}
	}

	this->m_rbrCITCarListFilePath = pluginINIFile.GetValue(L"Default", L"RBRCITCarListPath", L"");
	_Trim(this->m_rbrCITCarListFilePath);
	if (this->m_rbrCITCarListFilePath.length() >= 2 && this->m_rbrCITCarListFilePath[0] != '\\' && this->m_rbrCITCarListFilePath[1] != ':')
		this->m_rbrCITCarListFilePath = this->m_sRBRRootDirW + L"\\" + this->m_rbrCITCarListFilePath;


	// TODO: carPosition, camPosition reading from INI file (now the car and cam position is hard-coded in this plugin code)

	sTextValue = pluginINIFile.GetValue(szResolutionText, L"ScreenshotCropping", L"");
	_StringToRect(sTextValue, &this->m_screenshotCroppingRect);

	sTextValue = pluginINIFile.GetValue(szResolutionText, L"CarSelectLeftBlackBar", L"");
	_StringToRect(sTextValue, &this->m_carSelectLeftBlackBarRect);

	sTextValue = pluginINIFile.GetValue(szResolutionText, L"CarSelectRightBlackBar", L"");
	_StringToRect(sTextValue, &this->m_carSelectRightBlackBarRect);

	m_sMenuStatusText1 = _ToString(m_screenshotPath);
	m_sMenuStatusText2 = _ToString(std::wstring(szResolutionText)) + " native resolution detected";
}


//-------------------------------------------------------------------------------------------------
// Init car spec data by reading it from NGP plugin and RBRCIT config files
// - Read the current NGP physics car name from physics\c_xsara h_accent m_lancer mg_zr p_206 s_i2000 s_i2003 t_coroll RBR folder (the filename without extensions and revision/3D model tags)
//   - Physics revision / specification date / 3D model / plus optional 4th text line
// - Read Common.lsp file 
//   - "NumberOfGears" tag
// - Use the file name (or the first line in the file) as key to rbrcit/carlist/carList.ini file NAME=xxxx line
//   - cat / year / weight / power / trans
//
void CNGPCarMenu::InitCarSpecData()
{
	CSimpleIniW* ngpCarListINIFile = nullptr;
	static const char* szPhysicsCarFolder[8] = { "s_i2003" , "m_lancer", "t_coroll", "h_accent", "p_206", "s_i2000", "c_xsara", "mg_zr" };

	std::string sPath;
	sPath.reserve(_MAX_PATH);

	try
	{
		int iNumOfGears = -1;

		ngpCarListINIFile = new CSimpleIniW();
		if (fs::exists(m_rbrCITCarListFilePath))
			ngpCarListINIFile->LoadFile(m_rbrCITCarListFilePath.c_str());
		else
			// Add warning about missing RBRCIT carList.ini file
			wcsncpy_s(g_RBRCarSelectionMenuEntry[0].wszCarPhysicsCustomTxt, (m_rbrCITCarListFilePath + L" missing. Cannot show car specs").c_str(), COUNT_OF_ITEMS(g_RBRCarSelectionMenuEntry[0].wszCarPhysicsCustomTxt));

		for (int idx = 0; idx < 8; idx++)
		{
			sPath = CNGPCarMenu::m_sRBRRootDir + "\\physics\\" + szPhysicsCarFolder[idx];
			if (InitCarSpecDataFromPhysicsFile(sPath, &g_RBRCarSelectionMenuEntry[idx], &iNumOfGears))
				InitCarSpecDataFromNGPFile(ngpCarListINIFile, &g_RBRCarSelectionMenuEntry[idx], iNumOfGears);
		}
	}
	catch (const fs::filesystem_error& ex)
	{
		DebugPrint("ERROR in InitCarSpecData. %s", ex.what());
	}
	catch (...)
	{
		// Hmmm... Something went wrong
	}

	SAFE_DELETE(ngpCarListINIFile);
}


//-------------------------------------------------------------------------------------------------
// Read rbr\phystics\<carSlotNameFolder>\ngpCarNameFile file and init revision/3dModel/SpecYear attributes
//
bool CNGPCarMenu::InitCarSpecDataFromPhysicsFile(const std::string &folderName, PRBRCarSelectionMenuEntry pRBRCarSelectionMenuEntry, int* outNumOfGears)
{
	const fs::path fsFolderName(folderName);

	std::wstring wfsFileName;
	std::string  fsFileName;
	fsFileName.reserve(128);
	
	std::wstring sTextLine;
	sTextLine.reserve(128);

	int iReadRowCount;
	bool bResult = FALSE;

	try
	{
		// Read common.lsp file and "NumberOfGears" option
		if ( outNumOfGears != nullptr && fs::exists(folderName + "\\common.lsp") )
		{
			*outNumOfGears = -1;

			std::wifstream commoLspFile(folderName + "\\common.lsp");
			while (std::getline(commoLspFile, sTextLine))
			{				
				std::replace(sTextLine.begin(), sTextLine.end(), L'\t', L' ');
				_Trim(sTextLine);

				// Remove double whitechars from the string value
				sTextLine.erase(std::unique(sTextLine.begin(), sTextLine.end(), [](WCHAR a, WCHAR b) { return isspace(a) && isspace(b); }), sTextLine.end());

				if (_iStarts_With(sTextLine, L"numberofgears", true))
				{
					try
					{
						*outNumOfGears = StrToIntW(sTextLine.substr(14).c_str());
					}
					catch (...)
					{
						// Do nothing. eat all exceptions.
					}
					break;
				}
			}
		}

		// Find all files without file extensions and see it the file is the NGP car model file
		for (const auto& entry : fs::directory_iterator(fsFolderName))
		{
			if (entry.is_regular_file() && entry.path().extension().compare("") == 0)
			{
				fsFileName = entry.path().filename().string();
				//DebugPrint(fsFileName.c_str());

				wfsFileName = _ToWString(fsFileName);

				std::wifstream rbrFile(folderName + "\\" + fsFileName);
				iReadRowCount = 0;

				while (std::getline(rbrFile, sTextLine) && iReadRowCount <= 6)
				{	
					iReadRowCount++;
					_Trim(sTextLine);

					/*
					if (_iStarts_With(sTextLine, wfsFileName))
					{
						bResult = TRUE;

						// Usually the first line of NGP physics description file has the filename string (car model name)
						wcsncpy_s(pRBRCarSelectionMenuEntry->wszCarModel, wfsFileName.c_str(), COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarModel));
						pRBRCarSelectionMenuEntry->wszCarModel[COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarModel) - 1] = '\0';

						continue;
					}
					*/

					// Remove double whitechars from the string value
					sTextLine.erase(std::unique(sTextLine.begin(), sTextLine.end(), [](WCHAR a, WCHAR b) { return isspace(a) && isspace(b); }), sTextLine.end());

					if (_iStarts_With(sTextLine, L"revision", TRUE))
					{						
						bResult = TRUE;

						wcsncpy_s(pRBRCarSelectionMenuEntry->wszCarPhysicsRevision, sTextLine.c_str(), COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarPhysicsRevision));
						pRBRCarSelectionMenuEntry->wszCarPhysicsRevision[COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarPhysicsRevision) - 1] = '\0';
					}
					else if (_iStarts_With(sTextLine, L"specification date", TRUE))
					{
						bResult = TRUE;

						wcsncpy_s(pRBRCarSelectionMenuEntry->wszCarPhysicsSpecYear, sTextLine.c_str(), COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarPhysicsSpecYear));
						pRBRCarSelectionMenuEntry->wszCarPhysicsSpecYear[COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarPhysicsSpecYear) - 1] = '\0';
					}
					else if (_iStarts_With(sTextLine, L"3d model", TRUE))
					{
						bResult = TRUE;

						wcsncpy_s(pRBRCarSelectionMenuEntry->wszCarPhysics3DModel, sTextLine.c_str(), COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarPhysics3DModel));
						pRBRCarSelectionMenuEntry->wszCarPhysics3DModel[COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarPhysics3DModel) - 1] = '\0';
					}
					else if (bResult && !sTextLine.empty())
					{
						wcsncpy_s(pRBRCarSelectionMenuEntry->wszCarPhysicsCustomTxt, sTextLine.c_str(), COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarPhysicsCustomTxt));
						pRBRCarSelectionMenuEntry->wszCarPhysicsCustomTxt[COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarPhysicsCustomTxt) - 1] = '\0';
					}
				}

				if (bResult)
				{
					// If NGP car model file found then set carModel string value and no need to iterate through other files (without file extension)
					wcsncpy_s(pRBRCarSelectionMenuEntry->wszCarModel, wfsFileName.c_str(), COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarModel));
					pRBRCarSelectionMenuEntry->wszCarModel[COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarModel) - 1] = '\0';

					break;
				}
			}
		}

		if (!bResult)
		{
			std::wstring wFolderName = _ToWString(folderName);
			// Show warning that RBRCIT/NGP carModel file is missing
			wcsncpy_s(pRBRCarSelectionMenuEntry->wszCarPhysics3DModel, (wFolderName + L"\\<carModelName> NGP file missing").c_str(), COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarPhysics3DModel));
		}

	}
	catch (const fs::filesystem_error& ex)
	{
		DebugPrint("ERROR in InitCarSpecDataFromPhysicsFile. %s", ex.what());
		bResult = FALSE;
	}
	catch(...)
	{
		bResult = FALSE;
	}

	return bResult;
}


//-------------------------------------------------------------------------------------------------
// Init car spec data from NGP ini files (HP, year, transmissions, drive wheels, FIA category).
// Name of the NGP carList.ini entry is in pRBRCarSelectionMenuEntry->wszCarModel field.
//
bool CNGPCarMenu::InitCarSpecDataFromNGPFile(CSimpleIniW* ngpCarListINIFile, PRBRCarSelectionMenuEntry pRBRCarSelectionMenuEntry, int numOfGears)
{
	if (ngpCarListINIFile == nullptr || pRBRCarSelectionMenuEntry->wszCarModel[0] == '\0')
		return FALSE;

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
				//int len = wcstombs(pRBRCarSelectionMenuEntry->szCarCategory, szIniItemValue, COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->szCarCategory));
				wcstombs_s(/*&len*/ nullptr, pRBRCarSelectionMenuEntry->szCarCategory, sizeof(pRBRCarSelectionMenuEntry->szCarCategory), wszIniItemValue, _TRUNCATE);

				/*if (len == COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->szCarCategory))
					// Make sure the converted string is NULL terminated in case of truncation
					pRBRCarSelectionMenuEntry->szCarCategory[--len] = '\0';
				*/

				//DebugPrint(szIniItemValue);

				wszIniItemValue = ngpCarListINIFile->GetValue(iter->pItem, L"year", nullptr);
				wcsncpy_s(pRBRCarSelectionMenuEntry->wszCarYear, wszIniItemValue, COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarYear));

				wszIniItemValue = ngpCarListINIFile->GetValue(iter->pItem, L"weight", nullptr);
				swprintf_s(pRBRCarSelectionMenuEntry->wszCarWeight, COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarWeight), L"%s kg", wszIniItemValue);

				wszIniItemValue = ngpCarListINIFile->GetValue(iter->pItem, L"power", nullptr);
				wcsncpy_s(pRBRCarSelectionMenuEntry->wszCarPower, wszIniItemValue, COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarPower));

				// TODO. Localize "gears" label
				wszIniItemValue = ngpCarListINIFile->GetValue(iter->pItem, L"trans", nullptr);
				if (numOfGears > 0)
					swprintf_s(pRBRCarSelectionMenuEntry->wszCarTrans, COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarTrans), L"%d gears, %s", numOfGears, wszIniItemValue);
				else
					wcsncpy_s(pRBRCarSelectionMenuEntry->wszCarTrans, wszIniItemValue, COUNT_OF_ITEMS(pRBRCarSelectionMenuEntry->wszCarTrans));

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
		return FALSE;
	}

	return TRUE;
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
// Return the carID of the next screenshot (the next carID or the next carID with missing image file.
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
				if (m_iMenuCreateOption == 0)
				{
					// If the output PNG preview image file already exists then don't re-generate the screenshot (menu option "Only missing car images")
					outputFileName = m_screenshotPath + L"\\" + g_RBRCarSelectionMenuEntry[RBRAPI_MapCarIDToMenuIdx(currentCarID)].wszCarModel + L".png";
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
	std::wstring inputReplayFileName;
	std::wstring outputReplayFileName;

	inputReplayFileName  = g_pRBRPlugin->m_sRBRRootDirW + L"\\Replays\\" + g_pRBRPlugin->m_screenshotReplayFileName;
	outputReplayFileName = g_pRBRPlugin->m_sRBRRootDirW + L"\\Replays\\" + C_REPLAYFILENAME_SCREENSHOTW;
	
	try
	{
		// Open input replay file, modify car model ID on the fly and write out the replay file used to generate screenshots

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
		DebugPrint("ERROR in PrepareScreenshotReplayFile. %s", ex.what());
		return FALSE;
	}
	catch (...)
	{
		DebugPrint("ERROR in PrepareScreenshotReplayFile");
		return FALSE;
	}
}


//------------------------------------------------------------------------------------------------
//
const char* CNGPCarMenu::GetName(void)
{
	DebugPrint("Enter CNGPCarMenu.GetName");

	// Re-route RBR DXEndScene function to our custom function
	if (g_bRBRHooksInitialized == FALSE && m_PluginState == T_PLUGINSTATE::PLUGINSTATE_UNINITIALIZED)
	{
		// Get a pointer to DX9 device handler before re-routing the RBR function
		g_pRBRIDirect3DDevice9 = (LPDIRECT3DDEVICE9) *(DWORD*)(*(DWORD*)(*(DWORD*)0x007EA990 + 0x28) + 0xF4);

		// Initialize true screen resolutions. Internally RBR uses 640x480 4:3 resolution and aspect ratio
		D3DDEVICE_CREATION_PARAMETERS d3dCreationParameters;
		g_pRBRIDirect3DDevice9->GetCreationParameters(&d3dCreationParameters);
		g_hRBRWnd = d3dCreationParameters.hFocusWindow;

		GetWindowRect(g_hRBRWnd, &g_rectRBRWnd);							// Window size and position (including potential WinOS window decorations)
		GetClientRect(g_hRBRWnd, &g_rectRBRWndClient);						// The size or the D3D9 client area (without window decorations and left-top always 0)
		CopyRect(&g_rectRBRWndMapped, &g_rectRBRWndClient);					
		MapWindowPoints(g_hRBRWnd, NULL, (LPPOINT)&g_rectRBRWndMapped, 2);	// The client area mapped as physical screen position (left-top relative to screen)

		// Pointer 0x493980 -> rbrHwnd? Can it be used to re-route WM messages to our own windows handler and this way to "listen" RBR key presses?

		RefreshSettingsFromPluginINIFile();

		// Initialize D3D9 compatible font classes
#if USE_DEBUG == 1
		pFontDebug = new CD3DFont(L"Courier New", 11, 0);
		pFontDebug->InitDeviceObjects(g_pRBRIDirect3DDevice9);
		pFontDebug->RestoreDeviceObjects();
#endif 

		int iFontSize = 12;
		if (g_rectRBRWndClient.bottom < 600) iFontSize = 7;
		else if (g_rectRBRWndClient.bottom < 768) iFontSize = 9;

		pFontCarSpecCustom = new CD3DFont(L"Trebuchet MS", iFontSize, 0);
		pFontCarSpecCustom->InitDeviceObjects(g_pRBRIDirect3DDevice9);
		pFontCarSpecCustom->RestoreDeviceObjects();

		if (g_pRBRGameConfig == NULL)  g_pRBRGameConfig = (PRBRGameConfig) *(DWORD*)(0x007EAC48);
		if (g_pRBRGameMode == NULL)    g_pRBRGameMode = (PRBRGameMode) * (DWORD*)(0x007EAC48);
		if (g_pRBRGameModeExt == NULL) g_pRBRGameModeExt = (PRBRGameModeExt) * (DWORD*)(0x00893634);

		if (g_pRBRCarInfo == NULL)     g_pRBRCarInfo = (PRBRCarInfo)  *(DWORD*)(0x0165FC68);
		if (g_pRBRCarControls == NULL) g_pRBRCarControls = (PRBRCarControls) *(DWORD*)(0x007EAC48); // +0x738 + 0x5C;

		//if (pRBRCarMovement == NULL) pRBRCarMovement = (PRBRCarMovement) *(DWORD*)(0x008EF660);  // This pointer is valid only when replay or stage is starting
		if (g_pRBRGhostCarMovement == NULL) g_pRBRGhostCarMovement = (PRBRGhostCarMovement)(DWORD*)(0x00893060);

		if (g_pRBRMenuSystem == NULL) g_pRBRMenuSystem = (PRBRMenuSystem) *(DWORD*)(0x0165FA48);

		// Fixed location to mapSettings struct (ie. not a pointer reference). 
		g_pRBRMapSettings = (PRBRMapSettings)(0x1660800);

		// Initialize custom car selection menu name and model name strings (change RBR string pointer to our custom string)
		int idx;
		for (idx = 0; idx < 8; idx++)
		{
			// Use custom car menu selection name and car description (taken from the current NGP config files)
			WriteOpCodePtr((LPVOID)g_RBRCarSelectionMenuEntry[idx].ptrCarMenuName, &g_RBRCarSelectionMenuEntry[idx].szCarMenuName[0]);
			WriteOpCodePtr((LPVOID)g_RBRCarSelectionMenuEntry[idx].ptrCarDescription, &g_RBRCarSelectionMenuEntry[idx].wszCarModel[0]);
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

		// RBR memory and DX9 function hooks in place. Ready to do customized RBR logic
		g_bRBRHooksInitialized = TRUE;
		m_PluginState = T_PLUGINSTATE::PLUGINSTATE_INITIALIZED;
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
	// Draw blackout (coordinates specify the 'window' where you don't want black)
	m_pGame->DrawBlackOut(420.0f, 0.0f, 420.0f, 480.0f);

	// Draw custom plugin header line
	m_pGame->SetMenuColor(IRBRGame::MENU_HEADING);
	m_pGame->SetFont(IRBRGame::FONT_BIG);
	m_pGame->WriteText(65.0f, 49.0f, C_PLUGIN_TITLE);

	// The red menu selection line
	m_pGame->DrawSelection(0.0f, 68.0f + (static_cast< float >(m_iMenuSelection) * 21.0f), 360.0f);

	m_pGame->SetMenuColor(IRBRGame::MENU_TEXT);
	for (unsigned int i = 0; i < COUNT_OF_ITEMS(g_RBRPluginMenu); ++i)
	{
		if (i == 1)
		{
			char szTextBuf[128];
			sprintf_s(szTextBuf, sizeof(szTextBuf), "%s: %s", g_RBRPluginMenu[i], g_RBRPluginMenu_CreateOptions[m_iMenuCreateOption]);
			m_pGame->WriteText(65.0f, 70.0f + (static_cast< float >(i) * 21.0f), szTextBuf);
		}
		else
			m_pGame->WriteText(65.0f, 70.0f + (static_cast< float >(i) * 21.0f), g_RBRPluginMenu[i]);
	}

	if (!m_sMenuStatusText1.empty())
	{
		m_pGame->SetFont(IRBRGame::FONT_SMALL);
		m_pGame->WriteText(10.0f, 70.0f + (static_cast<float>(COUNT_OF_ITEMS(g_RBRPluginMenu) + 3) * 21.0f), m_sMenuStatusText1.c_str());
	}

	if (!m_sMenuStatusText2.empty())
	{
		m_pGame->SetFont(IRBRGame::FONT_SMALL);
		m_pGame->WriteText(10.0f, 70.0f + (static_cast<float>(COUNT_OF_ITEMS(g_RBRPluginMenu) + 4) * 21.0f), m_sMenuStatusText2.c_str());
	}
}

//------------------------------------------------------------------------------------------------
//
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

		if (m_iMenuSelection == 0)
		{
			// Re-read car preview images
			ClearCachedCarPreviewImages();
			RefreshSettingsFromPluginINIFile();

			// DEBUG. Show/Hide cropping area
			bool bNewStatus = !m_bCustomReplayShowCroppingRect;
			m_bCustomReplayShowCroppingRect = false;
			D3D9CreateRectangleVertexBuffer(g_pRBRIDirect3DDevice9, (float)this->m_screenshotCroppingRect.left, (float)this->m_screenshotCroppingRect.top, (float)(this->m_screenshotCroppingRect.right - this->m_screenshotCroppingRect.left), (float)(this->m_screenshotCroppingRect.bottom - this->m_screenshotCroppingRect.top), &m_screenshotCroppingRectVertexBuffer);
			m_bCustomReplayShowCroppingRect = bNewStatus;
		}
		else if (m_iMenuSelection == 1)
		{
			// Do nothing when "Create option" line is selected
		}
		else if (m_iMenuSelection == 2)
		{
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

	if (bDown && (++m_iMenuSelection) >= COUNT_OF_ITEMS(g_RBRPluginMenu))
		//m_iMenuSelection = 0; // Wrap around logic
		m_iMenuSelection = COUNT_OF_ITEMS(g_RBRPluginMenu) - 1;


	//
	// Menu options changed in the current menu line. Options don't wrap around.
	// Note! Not all menu lines have any additional options.
	//
	if (m_iMenuSelection == 1 && bLeft && (--m_iMenuCreateOption) < 0)
		m_iMenuCreateOption = 0;

	if (m_iMenuSelection == 1 && bRight && (++m_iMenuCreateOption) >= COUNT_OF_ITEMS(g_RBRPluginMenu_CreateOptions))
		m_iMenuCreateOption = COUNT_OF_ITEMS(g_RBRPluginMenu_CreateOptions) - 1;

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
	g_pRBRMapInfo     = (PRBRMapInfo) *(DWORD*)(0x1659184);
	g_pRBRCarMovement = (PRBRCarMovement) *(DWORD*)(0x008EF660);
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
							// Store the original localized weight and transmission title values. These titles are re-used in customized carSpec info screen.
							g_pOrigCarSpecTitleWeight = pCarMenuSpecTexts->wszWeightTitle;
							g_pOrigCarSpecTitleTransmission = pCarMenuSpecTexts->wszTransmissionTitle;
						}

						// Move carSpec text block few lines up to align "Select Car" and "car model" specLine text. 0x01701B20  (default carSpec pos-y value 0x06201B20)
						g_pRBRMenuSystem->currentMenuObj->pItemPosition[12].y = 0x0170;

						pCarMenuSpecTexts->wszModelTitle = L"FIA Category:";   // TODO: Localize FIACategory label
						pCarMenuSpecTexts->wszTorqueTitle = g_pOrigCarSpecTitleWeight;		 // Re-use localized Weight title in Torque spec line
						pCarMenuSpecTexts->wszEngineTitle = g_pOrigCarSpecTitleTransmission; // Re-use localized transmission title in Weight spec line
						pCarMenuSpecTexts->wszTyresTitle = L"Year:"; // TODO: Localized Year text

						// Original Weight and Transmission title and value lines are hidden
						pCarMenuSpecTexts->wszWeightTitle = pCarMenuSpecTexts->wszWeightValue =
						pCarMenuSpecTexts->wszTransmissionTitle = pCarMenuSpecTexts->wszTransmissionValue = L"";
					}

					// Change default carSpec values each time a new car menu line was selected 
					if (pCarMenuSpecTexts->wszTechSpecValue != pCarSelectionMenuEntry->wszCarModel)
					{
						pCarMenuSpecTexts->wszTechSpecValue = pCarSelectionMenuEntry->wszCarModel;
						pCarMenuSpecTexts->wszHorsepowerValue = pCarSelectionMenuEntry->wszCarPower;
						pCarMenuSpecTexts->wszTorqueValue = pCarSelectionMenuEntry->wszCarWeight;
						pCarMenuSpecTexts->wszEngineValue = pCarSelectionMenuEntry->wszCarTrans;

						// TODO: Restore model name pointer when carSelection menu is closed
						WriteOpCodePtr((LPVOID)pCarSelectionMenuEntry->ptrCarDescription, pCarSelectionMenuEntry->szCarCategory);

						// TODO: Restore original tyre name wchar values when SelectCar menu is closed
						WriteOpCodeBuffer((LPVOID)PTR_TYREVALUE_WCHAR_FIRESTONE, (const BYTE*)pCarSelectionMenuEntry->wszCarYear, 5 * sizeof(WCHAR));
						WriteOpCodeBuffer((LPVOID)PTR_TYREVALUE_WCHAR_BRIDGESTONE, (const BYTE*)pCarSelectionMenuEntry->wszCarYear, 5 * sizeof(WCHAR));
						WriteOpCodeBuffer((LPVOID)PTR_TYREVALUE_WCHAR_PIRELLI, (const BYTE*)pCarSelectionMenuEntry->wszCarYear, 5 * sizeof(WCHAR));
						WriteOpCodeBuffer((LPVOID)PTR_TYREVALUE_WCHAR_MICHELIN, (const BYTE*)pCarSelectionMenuEntry->wszCarYear, 5 * sizeof(WCHAR));
					}
				}
			}
		}
	}
	else if (g_pRBRPlugin->m_iCustomReplayState > 0)
	{	
		// NGPCarMenu plugin is generating car preview screenshot images. Draw a car model and take a screenshot from a specified cropping screen area

		g_pRBRCarInfo->stageStartCountdown = 1.0f;

		if (g_pRBRGameMode->gameMode == 0x08 /*&& g_pRBRGameModeExt->gameModeExt == 0x04*/)
		{
			// Move around the car and camera during replay and pause the replay. Don't start the normal replay logic of RBR
			g_pRBRGameMode->gameMode = 0x0A;
			g_pRBRGameModeExt->gameModeExt = 0x04;
		}

		if(g_pRBRGameMode->gameMode == 0x0A && g_pRBRGameModeExt->gameModeExt == 0x01)
		{ 
			// Prepare a new screenshot. Wait few secs to let RBR to finalize the 3D car drawing and finishing the replay starting animation

			g_pRBRPlugin->m_iCustomReplayState++;
			if (g_pRBRPlugin->m_iCustomReplayState > 100)
			{
				g_pRBRPlugin->m_iCustomReplayState = 1;
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
		}
		else if (g_pRBRGameMode->gameMode == 0x0A && g_pRBRGameModeExt->gameModeExt == 0x04)
		{
			// Car and camera is set in a custom location. Show the screenshot cropping rectangle for a few secs before taking the actual screenshot

			g_pRBRPlugin->m_iCustomReplayState++;		

			if (g_pRBRPlugin->m_iCustomReplayState <= 8 || g_pRBRPlugin->m_iCustomReplayState > 10)
				g_pRBRPlugin->m_bCustomReplayShowCroppingRect = true;
			else
				g_pRBRPlugin->m_bCustomReplayShowCroppingRect = false;
			
			if (g_pRBRPlugin->m_iCustomReplayState == 10 && !g_pRBRPlugin->m_screenshotPath.empty() && g_RBRCarSelectionMenuEntry[RBRAPI_MapCarIDToMenuIdx(g_pRBRPlugin->m_iCustomReplayCarID)].wszCarModel[0] != '\0')
			{
				// Take a RBR car preview screenshot and save it as PNG preview file.
				// At this point the cropping highlight rectangle is hidden, so it is not shown in the screenshot.
				std::wstring outputFileName = g_pRBRPlugin->m_screenshotPath + L"\\" + g_RBRCarSelectionMenuEntry[RBRAPI_MapCarIDToMenuIdx(g_pRBRPlugin->m_iCustomReplayCarID)].wszCarModel + L".png";
				D3D9SaveScreenToFile(g_pRBRIDirect3DDevice9, g_hRBRWnd, g_pRBRPlugin->m_screenshotCroppingRect, outputFileName);

				g_pRBRPlugin->m_iCustomReplayScreenshotCount++;
			}
			else if (g_pRBRPlugin->m_iCustomReplayState >= (g_pRBRPlugin->m_iCustomReplayScreenshotCount <= 1 ? 100 : 15))  // Show the cropping rect few secs longer during the first screenshot
			{
				g_pRBRPlugin->m_bCustomReplayShowCroppingRect = false;
				g_pRBRCarInfo->stageStartCountdown = 7.0f;

				g_pRBRPlugin->m_iCustomReplayState = 0;
				g_pRBRPlugin->m_iCustomReplayCarID = g_pRBRPlugin->GetNextScreenshotCarID(g_pRBRPlugin->m_iCustomReplayCarID);

				if (g_pRBRPlugin->m_iCustomReplayCarID >= 0)
				{
					if (CNGPCarMenu::PrepareScreenshotReplayFile(g_pRBRPlugin->m_iCustomReplayCarID))
						g_pRBRPlugin->m_iCustomReplayState = 1;
				}

				RBRAPI_Replay(g_pRBRPlugin->m_sRBRRootDir, C_REPLAYFILENAME_SCREENSHOT);
			}
		}
	}

	return hResult;
}


//----------------------------------------------------------------------------------------------------------------------------
// D3D9 EndScene handler. 
//
HRESULT __fastcall CustomRBRDirectXEndScene(void* objPointer)
{
	//HRESULT hResult;

	if (!g_bRBRHooksInitialized) 
		return S_OK;

	if (g_pRBRGameMode->gameMode == 03
		&& (g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_QUICKRALLY_CARS]
			|| g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P1]
			|| g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P2]
			|| g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P3]
			|| g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_MULTIPLAYER_CARS_P4]
			|| g_pRBRMenuSystem->currentMenuObj == g_pRBRMenuSystem->menuObj[RBRMENUIDX_RBRCHALLENGE_CARS]
			)
		)
	{
		// Show custom car details in "SelectCar" menu

		int selectedCarIdx = g_pRBRMenuSystem->currentMenuObj->selectedItemIdx - g_pRBRMenuSystem->currentMenuObj->firstSelectableItemIdx;

		if (selectedCarIdx >= 0 && selectedCarIdx <= 7)
		{
			float rbrPosX;
			int posX;
			int posY;
			int iFontHeight;

			// X pos of additional custom car spec data scaled per resolution
			switch (g_rectRBRWndClient.right)
			{
				case 640: 
				case 800:
				case 1024: rbrPosX = 218; break;
								
				case 1440:
				case 1680: rbrPosX = 235; break;
				
				default:   rbrPosX = 245; break;
			}

			iFontHeight = pFontCarSpecCustom->GetTextHeight();

			// TODO: Better re-scaler function to map game resolution to screen resolution
			RBRAPI_MapRBRPointToScreenPoint(rbrPosX, g_pRBRMenuSystem->menuImagePosY, &posX, &posY);
			posY -= 5 * iFontHeight;
			
			PRBRCarSelectionMenuEntry pCarSelectionMenuEntry = &g_RBRCarSelectionMenuEntry[selectedCarIdx];

			pFontCarSpecCustom->DrawText(posX, 0 * iFontHeight + posY, C_CARSPECTEXT_COLOR, pCarSelectionMenuEntry->wszCarPhysicsRevision, 0);
			pFontCarSpecCustom->DrawText(posX, 1 * iFontHeight + posY, C_CARSPECTEXT_COLOR, pCarSelectionMenuEntry->wszCarPhysicsSpecYear, 0);
			pFontCarSpecCustom->DrawText(posX, 2 * iFontHeight + posY, C_CARSPECTEXT_COLOR, pCarSelectionMenuEntry->wszCarPhysics3DModel, 0);
			pFontCarSpecCustom->DrawText(posX, 3 * iFontHeight + posY, C_CARSPECTEXT_COLOR, pCarSelectionMenuEntry->wszCarPhysicsCustomTxt, 0);

			if (g_pRBRPlugin->m_carPreviewTexture[selectedCarIdx].pTexture == nullptr && g_pRBRPlugin->m_carPreviewTexture[selectedCarIdx].imgSize.cx >= 0 && g_RBRCarSelectionMenuEntry[selectedCarIdx].wszCarModel[0] != '\0')
			{
				// Car preview image is not yet read from a file and cached as D3D9 texture in this plugin. Do it now.
				float posYf;
				RBRAPI_MapRBRPointToScreenPoint(0, g_pRBRMenuSystem->menuImagePosY-1, nullptr, &posYf);

				// TODO. Read image size and re-scale and center the image if it is not in native resolution folder

				HRESULT hResult = D3D9CreateRectangleVertexTexBufferFromFile(g_pRBRIDirect3DDevice9,
					g_pRBRPlugin->m_screenshotPath + L"\\" + g_RBRCarSelectionMenuEntry[selectedCarIdx].wszCarModel + L".png",
					0, posYf, 0, 0,
					&g_pRBRPlugin->m_carPreviewTexture[selectedCarIdx]);

				// Image not available. Do not try to re-load it again (set cx=-1 to indicate that the image loading failed, so no need to try to re-load it in every frame even when texture is null)
				if (!SUCCEEDED(hResult)) g_pRBRPlugin->m_carPreviewTexture[selectedCarIdx].imgSize.cx = -1; 
			}

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

				// Draw car preview image
				D3D9DrawVertexTex2D(g_pRBRIDirect3DDevice9, g_pRBRPlugin->m_carPreviewTexture[selectedCarIdx].pTexture, g_pRBRPlugin->m_carPreviewTexture[selectedCarIdx].vertexes2D);
			}
		}
	}
	//else if (g_pRBRPlugin->m_iCustomReplayState > 0 && g_pRBRPlugin->m_bCustomReplayShowCroppingRect && g_pRBRPlugin->m_screenshotCroppingRectVertexBuffer != nullptr)
	else if (g_pRBRPlugin->m_bCustomReplayShowCroppingRect)
	{
		// Draw rectangle to highlight the screenshot capture area
		D3D9DrawVertex2D(g_pRBRIDirect3DDevice9, g_pRBRPlugin->m_screenshotCroppingRectVertexBuffer);
	}

#if USE_DEBUG == 1
	WCHAR szTxtBuffer[200];

	swprintf_s(szTxtBuffer, COUNT_OF_ITEMS(szTxtBuffer), L"Mode %d %d.  Img (%f,%f)(%f,%f)  Timer=%f", g_pRBRGameMode->gameMode, g_pRBRGameModeExt->gameModeExt, g_pRBRMenuSystem->menuImagePosX, g_pRBRMenuSystem->menuImagePosY, g_pRBRMenuSystem->menuImageWidth, g_pRBRMenuSystem->menuImageHeight, g_pRBRCarInfo->stageStartCountdown);
	pFontDebug->DrawText(1, 1 * 20, C_DEBUGTEXT_COLOR, szTxtBuffer, 0);

	RECT wndRect;
	RECT wndClientRect;
	RECT wndMappedRect;
	D3DDEVICE_CREATION_PARAMETERS creationParameters;
	g_pRBRIDirect3DDevice9->GetCreationParameters(&creationParameters);
	GetWindowRect(creationParameters.hFocusWindow, &wndRect);
	GetClientRect(creationParameters.hFocusWindow, &wndClientRect);
	CopyRect(&wndMappedRect, &wndClientRect);
	MapWindowPoints(creationParameters.hFocusWindow, NULL, (LPPOINT)&wndMappedRect, 2);

	swprintf_s(szTxtBuffer, COUNT_OF_ITEMS(szTxtBuffer), L"hWnd=%x Win=(%d,%d)(%d,%d) Client=(%d,%d)(%d,%d)", (int)creationParameters.hFocusWindow,
		wndRect.left, wndRect.top, wndRect.right, wndRect.bottom,
		wndClientRect.left, wndClientRect.top, wndClientRect.right, wndClientRect.bottom);
	pFontDebug->DrawText(1, 2 * 20, C_DEBUGTEXT_COLOR, szTxtBuffer, 0);

	swprintf_s(szTxtBuffer, COUNT_OF_ITEMS(szTxtBuffer), L"Mapped=(%d,%d)(%d,%d) GameRes=(%d,%d)", wndMappedRect.left, wndMappedRect.top, wndMappedRect.right, wndMappedRect.bottom, g_pRBRGameConfig->resolutionX, g_pRBRGameConfig->resolutionY);
	pFontDebug->DrawText(1, 3 * 20, C_DEBUGTEXT_COLOR, szTxtBuffer, 0);
#endif

	// Call original RBR DXEndScene function and let it to do whatever needed to complete the drawing of DX framebuffer
	return Func_OrigRBRDirectXEndScene(objPointer);
}
