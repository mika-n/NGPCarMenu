//
// NGPCarMenu. Custom plugin to improve the "Select Car" menu in stock RBR game menus (ie. for those not using external launchers).
//
// This plugin supports following features
// - Longer car menu names (up to 30 chars)
// - Shows the actual car specs information from NGP plugin instead of showing the old default car specs (which are not valid with NGP physics and car models)
// - Shows more information about installed 3D car models and authors (3D car model creators definitely deserve more attention).
// - Shows a custom car preview image to show the actual 3D model of custom car model (instead of showing the default car garage image)
// - Generates new car preview images from the currently installed 3D car models
// - Customized car and camera position to take automatic screenshots of 3D car models
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

#ifndef __NGPCARMENU_H_INCLUDED
#define __NGPCARMENU_H_INCLUDED

//#define WIN32_LEAN_AND_MEAN			// Exclude rarely-used stuff from Windows headers
//#include <windows.h>
//#include <assert.h>
//#include <stdio.h>
#include <d3d9.h>

#include "PluginHelpers.h"

#include "SimpleINI\SimpleIni.h"    // INI file handling (uses https://github.com/brofield/simpleini library)
#include "Detourxs\detourxs.h"		// Detour API handling (uses https://github.com/DominicTobias/detourxs library)

#include "D3D9Helpers.h"			// Various text and D3D9 support functions created for this RBR plugin
#include "RBRAPI.h"					// RBR memory addresses and data structures


#define C_PLUGIN_TITLE "Custom NGPCarMenu Plugin (v1.04) by MIKA-N"		// Remember to tweak the NGPCarMenu.rc version properties also


#define C_DEBUGTEXT_COLOR   D3DCOLOR_ARGB(255, 255,255,255)      // White
#define C_CARSPECTEXT_COLOR D3DCOLOR_ARGB(255, 0xE0,0xE0,0xE0)   // Grey-White

#define COUNT_OF_ITEMS(array) (sizeof(array)/sizeof(array[0]))  // If a string array uses WCHAR then sizeof is not the same as number of chars (UTF8/Unicode has 2 bytes per char). This macro calculates the real num of items in statically allocated array

#define C_REPLAYFILENAME_SCREENSHOT   "_NGPCarMenu.rpl"  // Name of the temporary RBR replay file (char and wchar version)
#define C_REPLAYFILENAME_SCREENSHOTW L"_NGPCarMenu.rpl"



//
// The state of the plugin enum
//
enum class T_PLUGINSTATE
{
	PLUGINSTATE_UNINITIALIZED = 0,
	PLUGINSTATE_INITIALIZED = 1,
	PLUGINSTATE_CLOSING = 2
};


//------------------------------------------------------------------------------------------------

#define MAX_CARMENUNAME_NORMALCHARS 28
#define MAX_CARMENUNAME_ALLCHARS    31

typedef struct {
	UINT_PTR ptrCarMenuName;		// RBR memory location of a car menu name string ptr (char pointer in RBR memory space) (car menu order#) (SSE executable)
	UINT_PTR ptrCarDescription;		// RBR memory location of a car description string ptr

	char szCarMenuName[MAX_CARMENUNAME_ALLCHARS + 1]; // Max "normal width" chars to show is 27+null. But if the text has many I or 1 letters then two of those gives one extra char (max still 31+null);

	WCHAR wszCarModel[64];			// carList.ini.Name   (WCHAR, rbr TechnicalSpec line)
	CHAR  szCarCategory[32];		// carList.ini.Cat    (CHAR, model pointer text)
	WCHAR wszCarPower[32];			// carList.ini.Power  (WCHAR, Horsepower)
	WCHAR wszCarYear[8];			// carList.ini.Year   (WCHAR, Torque)
	WCHAR wszCarWeight[8];			// carList.ini.Weight (WCHAR, Weight, position-Y moved to the original engine position-Y location)
	WCHAR wszCarTrans[32];			// physics/common.lsp.NumberOfGears, carList.ini.Trans 4WD (WCHAR, Transmission, position-Y of transmission spec item moved to TYRES title position-Y)
									// (rbr Engine and Tyre title and text txt slot empty/not used. Position-Y moved to -XXX to hide those entries)

	WCHAR wszCarPhysicsRevision[96];  // physics/CARNAME Revision: 02 / 2019-06-03 (Written below car spec data using a bit smaller font size)
	WCHAR wszCarPhysicsSpecYear[96];  // Specification date:
	WCHAR wszCarPhysics3DModel[96];   // 3D model:
	WCHAR wszCarPhysicsCustomTxt[96]; // If physics/CARNAME file has a 5th text line, otherwise blank text. The 5th line in car model file can be used to show any custom text
} RBRCarSelectionMenuEntry;
typedef RBRCarSelectionMenuEntry* PRBRCarSelectionMenuEntry;


//------------------------------------------------------------------------------------------------

#define C_SCREENSHOTAPITYPE_DIRECTX 0
#define C_SCREENSHOTAPITYPE_GDI     1

class CNGPCarMenu;

extern BOOL         g_bRBRHooksInitialized;
extern CNGPCarMenu* g_pRBRPlugin;

extern HRESULT __fastcall CustomRBRDirectXBeginScene(void* objPointer);
extern HRESULT __fastcall CustomRBRDirectXEndScene(void* objPointer);

//------------------------------------------------------------------------------------------------//

class CNGPCarMenu : public IPlugin
{
protected:
	int	m_iCarMenuNameLen; // Max char space reserved for the current car menu name menu items (calculated in CalculateMaxLenCarMenuName method)


	DetourXS* gtcDirect3DBeginScene = NULL;
	DetourXS* gtcDirect3DEndScene = NULL;

	void InitCarSpecData();
	bool InitCarSpecDataFromPhysicsFile(const std::string& folderName, PRBRCarSelectionMenuEntry pRBRCarSelectionMenuEntry, int* outNumOfGears);
	bool InitCarSpecDataFromNGPFile(CSimpleIniW* ngpCarListINIFile, PRBRCarSelectionMenuEntry pRBRCarSelectionMenuEntry, int numOfGears);
	int  CalculateMaxLenCarMenuName();
	void ClearCachedCarPreviewImages();

	std::string m_sMenuStatusText1;	// Status text message 
	std::string m_sMenuStatusText2;

public:
	IRBRGame*     m_pGame;
	T_PLUGINSTATE m_PluginState;

	bool m_bMenuSelectCarCustomized;	// TRUE - The SelectCar is in customized state, FALSE - Various original values restored (ie. tyre brand names)

	int m_iMenuSelection;		// Currently selected plugin menu item idx
	int	m_iMenuCreateOption;	// 0 = Generate all car images, 1 = Generate only missing car images
	int	m_iMenuImageOption;		// 0 = Use PNG preview file format to read and create image files, 1 = BMP file format

	std::string  m_sRBRRootDir;  // RBR app path, ASCII string
	std::wstring m_sRBRRootDirW; // RBR app path, Unicode string

	std::wstring m_screenshotPath;				// Path to car preview screenshot images (by default AppPath + \plugins\NGPCarMenu\preview\XResxYRes\)
	int m_screenshotAPIType;					// Uses DIRECTX or GDI API technique to generate a new screenshot file. 0=DirectX (default), 1=GDI. No GUI option, so tweak this in NGPCarMenu.ini file.
	std::wstring m_screenshotReplayFileName;	// Name of the RBR replay file used when car preview images are generated

	std::wstring m_rbrCITCarListFilePath;		// Path to RBRCIT carList.ini file (the file has NGP car details and specs information)

	RECT m_screenshotCroppingRect;				// Cropping rect of a screenshot (in RBR window coordinates)
	RECT m_carSelectLeftBlackBarRect;			// Black bar on the left and right side of the "Select Car" menu (used to hide the default background image)
	RECT m_carSelectRightBlackBarRect;
	POINT m_car3DModelInfoPosition;				// X Y position of the car 3D info textbox. If Y is 0 then the plugin uses the default Y location (few lines above the car preview image).

	LPDIRECT3DVERTEXBUFFER9 m_screenshotCroppingRectVertexBuffer; // Screeshot rect vertex to highlight the current capture area on screen while capturing preview img

	int  m_iCustomReplayState;					// 0 = No custom replay (default RBR behaviour), 1 = Custom replay process is running. Take screenshots of a car models.
	std::chrono::steady_clock::time_point m_tCustomReplayStateStartTime;

	int  m_iCustomReplayCarID;					// 0..7 = The current carID used in a custom screenshot replay file
	int  m_iCustomReplayScreenshotCount;		// Num of screenshots taken so far during the last "CREATE car preview images" command
	bool m_bCustomReplayShowCroppingRect;		// Show the current car preview screenshot cropping rect area on screen (ie. few secs before the screenshot is taken)

	IMAGE_TEXTURE m_carPreviewTexture[8];		// 0..7 car preview image data (or NULL if missing/not loaded yet)	


	//------------------------------------------------------------------------------------------------

	CNGPCarMenu(IRBRGame* pGame);
	virtual ~CNGPCarMenu(void);

	int GetNextScreenshotCarID(int currentCarID);
	static bool PrepareScreenshotReplayFile(int carID);

	void RefreshSettingsFromPluginINIFile();
	void SaveSettingsToPluginINIFile();

	//------------------------------------------------------------------------------------------------
	virtual const char* GetName(void);

	virtual void DrawResultsUI(void);
	virtual void DrawFrontEndPage(void);
	virtual void HandleFrontEndEvents(char txtKeyboard, bool bUp, bool bDown, bool bLeft, bool bRight, bool bSelect);
	virtual void TickFrontEndPage(float fTimeDelta);

	/// Is called when the race clock of a player starts running after GO! or false start
	virtual void StageStarted(int iMap, const char* ptxtPlayerName, bool bWasFalseStart);
	// Is called when player reaches the first or second checkpoint (iCheckPointID). Every track has only two checkpoints no matter how long the track is?
	virtual void CheckPoint(float fCheckPointTime, int   iCheckPointID, const char* ptxtPlayerName);
	// Is called when player finishes the stage ( fFinishTime is 0.0f if player failed the stage )
	virtual void HandleResults(float fCheckPoint1, float fCheckPoint2, float fFinishTime, const char* ptxtPlayerName);
};

#endif
