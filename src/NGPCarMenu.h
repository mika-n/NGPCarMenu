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
#include <memory>					// unique_ptr and smart_ptr
//#include <forward_list>				// std::forward_list
#include <list>

#include "PluginHelpers.h"

#include "SimpleINI\SimpleIni.h"    // INI file handling (uses https://github.com/brofield/simpleini library)
#include "Detourxs\detourxs.h"		// Detour API handling (uses https://github.com/DominicTobias/detourxs library)

#include "D3D9Helpers.h"			// Various text and D3D9 support functions created for this RBR plugin
#include "RBRAPI.h"					// RBR memory addresses and data structures


#define C_PLUGIN_TITLE_FORMATSTR "NGPCarMenu Plugin (%s) by MIKA-N"	// %s is replaced with version tag strign. Remember to tweak it in NGPCarMenu.rc when making a new release
#define C_PLUGIN_FOOTER_STR      "https://github.com/mika-n/NGPCarMenu"

#define C_DEBUGTEXT_COLOR   D3DCOLOR_ARGB(255, 255,255,255)      // White

#define C_CARSPECTEXT_COLOR			D3DCOLOR_ARGB(255, 0xE0,0xE0,0xE0)   // Grey-White
#define C_CARMODELTITLETEXT_COLOR	D3DCOLOR_ARGB(255, 0x7F,0x7F,0xFE)   // Light-blue

#define C_REPLAYFILENAME_SCREENSHOT   "_NGPCarMenu.rpl"  // Name of the temporary RBR replay file (char and wchar version)
#define C_REPLAYFILENAME_SCREENSHOTW L"_NGPCarMenu.rpl"

#define C_RBRTM_PLUGIN_NAME     "RBR Tournament"
#define C_MAX_RECENT_RBRTM_MAPS 5

//
// The state of the plugin enum
//
enum class T_PLUGINSTATE
{
	PLUGINSTATE_UNINITIALIZED = 0,
	PLUGINSTATE_INITIALIZING = 1,
	PLUGINSTATE_INITIALIZED = 2,
	PLUGINSTATE_CLOSING = 3
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

	WCHAR wszCarPhysicsRevision[128];  // physics/CARNAME Revision: 02 / 2019-06-03 (Written below car spec data using a bit smaller font size)
	WCHAR wszCarPhysicsSpecYear[128];  // Specification date:
	WCHAR wszCarPhysics3DModel[128];   // 3D model:
	WCHAR wszCarPhysicsLivery[128];    // Livery (Liver and Livery credits)
	WCHAR wszCarPhysicsCustomTxt[128]; // If physics/CARNAME file has a 5th text line, otherwise blank text. The 5th line in car model file can be used to show any custom text

	WCHAR wszCarFMODBank[128];		   // Custom FMOD sound bank
} RBRCarSelectionMenuEntry;
typedef RBRCarSelectionMenuEntry* PRBRCarSelectionMenuEntry;

//------------------------------------------------------------------------------------------------

#define RBRTMMENUIDX_CARSELECTION		0x15978614	// RBRTMMenuObj.menuID values. RBRTM car selection screen (either below Shakedown or OnlineTournament menu tree)
#define RBRTMMENUIDX_MAIN				0x159785F8  // RBRTM main menu, so the RBRTM menu is no longer below Shakedown or OnlineTournament menu tree
#define RBRTMMENUIDX_ONLINEOPTION1		0x159786F8	// RBRTM OnlineTournament options 1 menu (options for online rally lookups)
#define RBRTMMENUIDX_ONLINEOPTION2		0x15978684	// RBRTM OnlineTournament options 2 menu (list of online rallies)
#define RBRTMMENUIDX_SHAKEDOWNSTAGES	0x15978154	// RBRTM Shakedown Stage selection screen
#define RBRTMMENUIDX_SHAKEDOWNOPTION1	0x159786A0	// RBRTM Shakedown options 1 menu (the menu just before the car selection screen)

typedef struct {
#pragma pack(push,1)
	BYTE unknown1;
	BYTE unknown2;
	BYTE serviceMins;    // 0 = Shakedown without service parks. >1 = Mins of the next service park
	BYTE selectedCarID;  // 00..07 = Selected car slot#
#pragma pack(pop)
} RBRTMStageOptions1;
typedef RBRTMStageOptions1* PRBRTMStageOptions1;

typedef struct {
#pragma pack(push,1)
	const WCHAR*  wszMenuItemName; // 0x00
	BYTE    pad1[0x2C - 0x00 - sizeof(WCHAR*)];
	__int32 mapID;			 // 0x2C
	BYTE    pad2[0x38 - 0x2C - sizeof(__int32)]; // 0x38 = Total size of RBRTM menu item struct
#pragma pack(pop)
} RBRTMMenuItem;
typedef RBRTMMenuItem* PRBRTMMenuItem;

typedef struct {
#pragma pack(push,1)
	__int32 unknown1;			// 0x00
	PRBRTMMenuItem pMenuItems;	// 0x04 Pointer to array of menuItem structs
	__int32 numOfItems;			// 0x08 Num of items in pMenuItems array (not including the "Back" menu item)
#pragma pack(pop)
} RBRTMMenuData;
typedef RBRTMMenuData* PRBRTMMenuData;

typedef struct {
#pragma pack(push,1)
	DWORD menuID;				// 0x00	(0x159785F8=RBRTM main menu, 0x15978614=RBRTM CarSelection screen, 0x159786F8=Online options1 screen, 0x15978684=Online options2 screen, 0x15978154=Shakedown stage selection, 0x159786A0=Shakedown options screen)
	PRBRTMMenuData pMenuData;	// 0X04
#pragma pack(pop)
} RBRTMMenuObj;
typedef RBRTMMenuObj* PRBRTMMenuObj;

// Offset 0x1597F128?
typedef struct {
#pragma pack(push,1)
	__int32 unknown1;				// 0x00
	__int32 unknown2;				// 0x04
	__int32 unknown3;				// 0x08
	PRBRTMMenuObj pCurrentRBRTMMenuObj;	// 0x0C - Pointer to the current RBRTM menu object
	__int32 selectedItemIdx;		// 0x10 - Currently selected menu item line
	__int32 selectedStage;			// 0x14 - Currently selected shakedown stage# (mapID). Set after the stage was selected from the list of RBRTM stages
	BYTE pad1[0xA08 - 0x14 - sizeof(__int32)];
	__int32 numOfMenuItems;			// 0xA08 - Num of menu rows + 1 (back) in Shakedown stage selection menu list. This controls the max movement of focus line (not the num of shown menu items). RBRTMMenuData.numOfItems controls the num of shown menu items, but not focus line movements.
	BYTE pad2[0xBA2 - 0xA08 - sizeof(__int32)];
	PRBRTMStageOptions1 pRBRTMStageOptions1;	// 0xBA2
#pragma pack(pop)
} RBRTMPlugin;
typedef RBRTMPlugin* PRBRTMPlugin;

struct RBRTM_MapInfo {
	int			  mapID;			// MapID
	int			  mapIDMenuIdx;		// Menu index (used when the previous menu line is automatically selected when user navigates back to Shakedown menu)
	std::wstring  name;
	double		  length;
	int			  surface;
	std::wstring  previewImageFile;
	IMAGE_TEXTURE imageTexture;

	RBRTM_MapInfo() 
	{
		name.reserve(64);
		mapID = -1;
		mapIDMenuIdx = -1;
		length = -1;
		surface = -1;
		ZeroMemory(&imageTexture, sizeof(IMAGE_TEXTURE));
	}

	~RBRTM_MapInfo()
	{
		SAFE_RELEASE(imageTexture.pTexture);
	}
};


//------------------------------------------------------------------------------------------------

typedef struct {
#pragma pack(push,1)
	char szTrackName[256];
	char szTrackFolder[260];
#pragma pack(pop)
} RBRRXMenuItem;
typedef RBRRXMenuItem* PRBRRXMenuItem;

typedef struct {
#pragma pack(push,1)
	BYTE pad[0x08];				// 0x00
	__int32 selectedItemIdx;	// 0x08
#pragma pack(pop)
} RBRRXMenuData;
typedef RBRRXMenuData* PRBRRXMenuData;

// Offset rbr_rx base
typedef struct {
#pragma pack(push,1)
	BYTE pad1[0x608E4];			// 0x00	
	PRBRRXMenuItem pMenuItems;	// 0x608E4
	__int32 numOfItems;			// 0x608E8
	BYTE pad2[0x60918- 0x608E8 - sizeof(__int32)];
	__int32 menuID;				// 0x60918			0=main, 1=stages, 2=replay
	BYTE pad3[0x665F4 - 0x60918- sizeof(__int32)];
	PRBRRXMenuData pMenuData;	// 0x665F4 
#pragma pack(pop)
} RBRRXPlugin;
typedef RBRRXPlugin* PRBRRXPlugin;

struct RBRRX_MapInfo {
	int			  mapIDMenuIdx;		// Menu index (used when the previous menu line is automatically selected when user navigates back to Shakedown menu)
	std::string   name;
	std::string   folderName;
	//double length;
	std::wstring  surface;
	std::wstring  previewImageFile;
	IMAGE_TEXTURE imageTexture;

	RBRRX_MapInfo()
	{
		//name.reserve(256);
		mapIDMenuIdx = -1;
		//length = -1;
		surface = L"";
		ZeroMemory(&imageTexture, sizeof(IMAGE_TEXTURE));
	}

	~RBRRX_MapInfo()
	{
		SAFE_RELEASE(imageTexture.pTexture);
	}
};


//------------------------------------------------------------------------------------------------

#define C_SCREENSHOTAPITYPE_DIRECTX 0
#define C_SCREENSHOTAPITYPE_GDI     1

class CNGPCarMenu;

extern BOOL         g_bRBRHooksInitialized;
extern CNGPCarMenu* g_pRBRPlugin;

extern HRESULT __fastcall CustomRBRDirectXBeginScene(void* objPointer);
extern HRESULT __fastcall CustomRBRDirectXEndScene(void* objPointer);


//------------------------------------------------------------------------------------------------//
// CRBRPluginIntegratorLink. Integrates the custom image drawing with a specified plugin.
// The linked "other plugin" doesn't have to worry about directX textures, vertex, surface and bitmap objects because this NGPCarMenu plugin takes care of those.
// DrawFrontEndPage method of the "other plugin" can use simple API functions exported by NGPCarMenu 
// plugin to load and draw custom images at specified location with or without scaling (PNG and BMP files).
// See src/SamplePluginIntegration/NGPCarMenuAPI.h header. Include NGPCarMenuAPI.h header file in your own plugin and use those CNGPCarMenuAPI class methods to register a plugin link and to draw custom images.
//
class CRBRPluginIntegratorLinkImage
{
public:
	int m_iImageID;
	std::string m_sImageFileName;
	bool  m_bShowImage;
	POINT m_imagePos;
	SIZE  m_imageSize;
	DWORD m_dwImageFlags;
	
	IMAGE_TEXTURE m_imageTexture;

	CRBRPluginIntegratorLinkImage()
	{
		m_iImageID = -1;
		m_bShowImage = false;
		m_imagePos.x = m_imagePos.y = 0;
		m_imageSize.cx = m_imageSize.cy = 0;
		m_dwImageFlags = 0;
		ZeroMemory(&m_imageTexture, sizeof(m_imageTexture));
	}

	~CRBRPluginIntegratorLinkImage()
	{
		SAFE_RELEASE(m_imageTexture.pTexture);
	}
};

class CRBRPluginIntegratorLink
{
public:
	int m_iCustomPluginMenuIdx;			// Index to custom plugin
	bool m_bCustomPluginActive;			// Is the custom plugin active in RBR menu system?
	std::string m_sCustomPluginName;	// Name of the custom plugin (as shown in RBR Plugins menu)
	std::vector<std::unique_ptr<CRBRPluginIntegratorLinkImage>> m_imageList; // Image cache (DX9 textures)

	CRBRPluginIntegratorLink() 
	{ 
		m_iCustomPluginMenuIdx = 0; 
		m_bCustomPluginActive = FALSE;
	}

	~CRBRPluginIntegratorLink()
	{
		m_iCustomPluginMenuIdx = -1;
		m_bCustomPluginActive = FALSE;
		m_imageList.clear();
	}
};


//------------------------------------------------------------------------------------------------//
// CNGPCarMenu class. Custom car preview images in RBR and RBRTM "select a car" menus.
//
class CNGPCarMenu : public IPlugin
{
protected:
	std::string m_sPluginTitle;		// Title of the plugin shown in NGPCarMenu menus

	CSimpleIniW* m_pLangIniFile;	// NGPCarMenu language localization file

	int	m_iCarMenuNameLen; // Max char space reserved for the current car menu name menu items (calculated in CalculateMaxLenCarMenuName method)

	DetourXS* gtcDirect3DBeginScene;
	DetourXS* gtcDirect3DEndScene;

	void InitCarSpecData_RBRCIT();
	void InitCarSpecData_EASYRBR();
	void InitCarSpecAudio();

	std::wstring InitCarModelNameFromCarsFile(CSimpleIniW* stockCarListINIFile, int menuIdx);
	bool InitCarSpecDataFromPhysicsFile(const std::string& folderName, PRBRCarSelectionMenuEntry pRBRCarSelectionMenuEntry, int* outNumOfGears);
	bool InitCarSpecDataFromNGPFile(CSimpleIniW* ngpCarListINIFile, PRBRCarSelectionMenuEntry pRBRCarSelectionMenuEntry, int numOfGears);

	int  CalculateMaxLenCarMenuName();
	void ClearCachedCarPreviewImages();

	std::string m_sMenuStatusText1;	// Status text message 
	std::string m_sMenuStatusText2;
	std::string m_sMenuStatusText3;

public:
	IRBRGame*     m_pGame;
	T_PLUGINSTATE m_PluginState;

	bool m_bMenuSelectCarCustomized;	// TRUE - The SelectCar is in customized state, FALSE - Various original values restored (ie. tyre brand names)

	int m_iMenuSelection;		// Currently selected plugin menu item idx
	int	m_iMenuCreateOption;	// 0 = Generate all car images, 1 = Generate only missing car images
	int	m_iMenuImageOption;		// 0 = Use PNG preview file format to read and create image files, 1 = BMP file format
	int m_iMenuRBRTMOption;		// 0 = RBRTM integration disabled, 1 = Enabled
	int m_iMenuRBRRXOption;		// 0 = RBRRX integration disabled, 1 = Enabled

	bool m_bRBRFullscreenDX9;			// Is RBR running in fullscreen or windows DX9 mode? TRUE-fullscreen, FALSE=windowed
	bool m_bPacenotePluginInstalled;	// Is Pacenote plugin used? RBR exit logic handles font cleanup a bit differently in fullscreen mode IF pacenote plugin is missing

	std::string  m_sRBRRootDir;  // RBR app path, multibyte (or normal ASCII) string
	std::wstring m_sRBRRootDirW; // RBR app path, widechar string

	std::wstring m_screenshotPath;				// Path to car preview screenshot images (by default AppPath + \plugins\NGPCarMenu\preview\XResxYRes\)
	int m_screenshotAPIType;					// Uses DIRECTX or GDI API technique to generate a new screenshot file. 0=DirectX (default), 1=GDI. No GUI option, so tweak this in NGPCarMenu.ini file.
	std::wstring m_screenshotReplayFileName;	// Name of the RBR replay file used when car preview images are generated

	std::wstring m_rbrCITCarListFilePath;		// Path to RBRCIT carList.ini file (the file has NGP car details and specs information)
	std::wstring m_easyRBRFilePath;				// Path to EesyRBR installation folder (if RBRCIT car manager is not used)

	RECT  m_screenshotCroppingRect;				// Cropping rect of a screenshot (in RBR window coordinates)
	RECT  m_carSelectLeftBlackBarRect;			// Black bar on the left and right side of the "Select Car" menu (used to hide the default background image)
	RECT  m_carSelectRightBlackBarRect;			// (see above)
	POINT m_car3DModelInfoPosition;				// X Y position of the car 3D info textbox. If Y is 0 then the plugin uses the default Y location (few lines above the car preview image).
	int   m_carPictureScale;					// Keep aspect ratio or stretch the image to fill the picture rectangle area (1=keep aspect, 0=stretch to fill)
	BOOL  m_carPictureUseTransparent;			// 0=Do not try to draw alpha channel (transparency), 1=If PNG file has an alpha channel then draw the image using the alpha channel blending

	RECT  m_carRBRTMPictureRect;				// Output rect of RBRTM car preview image (re-scaled pic area)
	RECT  m_carRBRTMPictureCropping;			// Optional cropping area of the normal car preview image to be used as RBRTM preview image (0 0 0 0 = Re-scales the whole picture to fit the RBRTM pic rect)
	int   m_carRBRTMPictureScale;				// Keep aspect ratio or stretch the image to fill the picture rectangle area (1=keep aspect, 0=stretch to fill)
	//POINT m_carRBRTM3DModelInfoPosition;		// X Y position of the car info textbox (FIA Category, HP, Transmission, Weight, Year)
	BOOL  m_carRBRTMPictureUseTransparent;		// 0=Do not try to draw alpha channel (transparency), 1=If PNG file has an alpha channel then draw the image using the alpha channel blending

	LPDIRECT3DVERTEXBUFFER9 m_screenshotCroppingRectVertexBuffer; // Screeshot rect vertex to highlight the current capture area on screen while capturing preview img

	int  m_iCustomReplayState;					// 0 = No custom replay (default RBR behaviour), 1 = Custom replay process is running. This plugin takes screenshots of car models
	std::chrono::steady_clock::time_point m_tCustomReplayStateStartTime;

	int  m_iCustomReplayCarID;					// 0..7 = The current carID used in a custom screenshot replay file
	int  m_iCustomReplayScreenshotCount;		// Num of screenshots taken so far during the last "CREATE car preview images" command
	bool m_bCustomReplayShowCroppingRect;		// Show the current car preview screenshot cropping rect area on screen (ie. few secs before the screenshot is taken)

	IMAGE_TEXTURE m_carPreviewTexture[8];		// 0..7 car preview image data (or NULL if missing/not loaded yet)	
	IMAGE_TEXTURE m_carRBRTMPreviewTexture[8];  // 0..7 car preview image for RBRTM plugin integration if RBRTM integration is enabled (the same pic as in standard preview image texture, but re-scaled to fit the smaller picture are in RBRTM screen)

	CSimpleIniW* m_pTracksIniFile;				// maps\Tracks.ini file (if RBRTM integration is enabled then splash/preview images are shown in Shakedown map selection list)


	RECT         m_mapRBRTMPictureRect;			// Output rect of RBRTM map preview image (re-scaled pic area)
	std::wstring m_screenshotPathMapRBRTM;		// Custom map preview image path
	RBRTM_MapInfo m_latestMapRBRTM;				// The latest selected stage (mapID) in RBRTM Shakedown menu (if the current mapID is still the same then no need to re-load the same stage preview image)

	int m_recentMapsMaxCountRBRTM;				// Max num of recent stages/maps added to recentMaps vector (default 5 if not set in INI file. 0 disabled the custom Shakedown menu feature)
	std::list<std::unique_ptr<RBRTM_MapInfo>> m_recentMapsRBRTM; // Recent maps shown on top of the Shakedown stage list
	bool m_bRecentMapsRBRTMModified;			// Is the recent maps list modified since the last INI file saving?

	PRBRTMMenuData m_pOrigMapMenuDataRBRTM;		// The original RBRTM Shakedown menu data struct
	PRBRTMMenuItem m_pOrigMapMenuItemsRBRTM;	// The original RBRTM Shakedown stages menu item array
	int            m_origNumOfItemsMenuItemsRBRTM; // The original num of menu items in Stages menu list
	PRBRTMMenuItem m_pCustomMapMenuRBRTM;		// Custom "list of stages" menu in RBRTM Shakedown menu (contains X recent shortcuts and the original list of stages)
	int m_numOfItemsCustomMapMenuRBRTM;			// Num of items in m_pCustomMapMenuRBRTM (dynamic) array


	RECT         m_mapRBRRXPictureRect;			// Output rect of RBRRX map preview image (re-scaled pic area)
	std::wstring m_screenshotPathMapRBRRX;		// Custom map preview image path
	RBRRX_MapInfo m_latestMapRBRRX;				// The latest selected stage in RBRRX menu (if the current mapID is still the same then no need to re-load the same stage preview image)

	int m_recentMapsMaxCountRBRRX;				// Max num of recent stages/maps added to recentMaps vector (default 5 if not set in INI file. 0 disabled the custom Shakedown menu feature)
	std::list<std::unique_ptr<RBRRX_MapInfo>> m_recentMapsRBRRX; // Recent maps shown on top of the RBR_RX stage list
	bool m_bRecentMapsRBRRXModified;			// Is the recent maps list modified since the last INI file saving?

	PRBRRXMenuItem m_pOrigMapMenuItemsRBRRX;	// The original RBRTM Shakedown stages menu item array
	int            m_origNumOfItemsMenuItemsRBRRX; // The original num of menu items in Stages menu list
	PRBRRXMenuItem m_pCustomMapMenuRBRRX;		// Custom "list of stages" menu in RBRRX
	int m_numOfItemsCustomMapMenuRBRRX;			// Num of items in m_pCustomMapMenuRBRRX (dynamic) array
	int m_currentCustomMapSelectedItemIdxRBRRX;	// "Virtual" selected row running from 0..m_numOfItemsCustomMapMenuRBRRX-1 (the real row number. m_pRBRRXPlugin->pMenuData->selectedItemIdx is always between 0..16)
	int m_prevCustomMapSelectedItemIdxRBRRX;

	std::string m_sRBRTMPluginTitle;			// "RBR Tournament" is the RBRTM plugin name by default, but in theory it is possible that this str is translated in RBRTM language files. The plugin name in use is stored here because the RBRTM integration routine needs this name.
	int    m_iRBRTMPluginMenuIdx;				// Index of the RBRTM plugin in the RBR Plugins menu list (this way we know when RBRTM custom plugin in Nth index position is activated)
	bool   m_bRBRTMPluginActive;				// TRUE/FALSE if the current active custom plugin is RBRTM (active = The RBRTM plugin handler is running in foreground)
	int    m_iRBRTMCarSelectionType;			// 0=No car selection menu shown, 1=Online Tournament selection, 2=Shakedown car selection

	int    m_iRBRRXPluginMenuIdx;				// Index of the RBR_RX plugin in the RBR Plugins menu list (this way we know when RBRRX custom plugin in Nth index position is activated)
	bool   m_bRBRRXPluginActive;				// TRUE/FALSE if the current active custom plugin is RBR_RX (active = The RBRTM plugin handler is running in foreground)

	PRBRMenuObj  m_pRBRPrevCurrentMenu;			// If RBRTM or RBRRX integration is enabled then NGPCarMenu must try to identify Plugins menuobj and RBRTM/RBRRX plugin. This is just a "previous currentMenu" in order to optimize the check routine (ie. don't re-check if the plugin is RBRTM until new menu/plugin is activated)
	
	PRBRTMPlugin m_pRBRTMPlugin;				// Pointer to RBRTM plugin or nullptr if not found or RBRTM integration is disabled	
	PRBRRXPlugin m_pRBRRXPlugin;				// Pointer to RBRRX plugin or nullptr

	CD3D9RenderStateCache* m_pD3D9RenderStateCache; // DirectX cache class to restore DX9 render state back to original settings after tweaking render state

	//------------------------------------------------------------------------------------------------

	CNGPCarMenu(IRBRGame* pGame);
	virtual ~CNGPCarMenu(void);

	int GetNextScreenshotCarID(int currentCarID);
	static bool PrepareScreenshotReplayFile(int carID);

	std::wstring ReplacePathVariables(const std::wstring& sPath, int selectedCarIdx = -1, bool rbrtmplugin = false, int mapID = -1, const WCHAR* mapName = nullptr, const std::string& folderName = "");
	bool ReadCarPreviewImageFromFile(int selectedCarIdx, float x, float y, float cx, float cy, IMAGE_TEXTURE* pOutImageTexture, DWORD dwFlags = 0, bool isRBRTMPlugin = false);

	int InitPluginIntegration(const std::string& customPluginName, bool bInitRBRTM);
	int InitAllNewCustomPluginIntegrations();

	void RefreshSettingsFromPluginINIFile(bool addMissingSections = false);
	void SaveSettingsToPluginINIFile();


	int FindRBRTMMenuItemIdxByMapID(PRBRTMMenuData pMenuData, int mapID)
	{
		// Find the RBRTM menu item by mapID. Return index to the menuItem struct or -1
		if (pMenuData != nullptr && mapID > 0)
		{
			for (int idx = 0; idx < pMenuData->numOfItems; idx++)
				if (pMenuData->pMenuItems[idx].mapID == mapID)
					return idx;
		}

		return -1;
	}

	int FindRBRRXMenuItemIdxByFolderName(PRBRRXMenuItem pMapMenuItemsRBRRX, int numOfItemsMenuItemsRBRRX, const std::string& folderName)
	{
		// Find the RBRRX menu item by folderName (map identifier because BTB tracks don't have unique ID numbers). Return index to the menuItem struct or -1
		if (pMapMenuItemsRBRRX != nullptr && !folderName.empty())
		{
			std::string trackFolder;
			trackFolder.reserve(260); // Max length of folder name in RBR_RX plugin
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

	int CalculateNumOfValidMapsInRecentList(PRBRTMMenuData pMenuData = nullptr)
	{
		int numOfRecentMaps = 0;
		int menuIdx;

		auto it = m_recentMapsRBRTM.begin();
		while (it != m_recentMapsRBRTM.end()) 
		{
			if ((*it)->mapID > 0 && numOfRecentMaps < m_recentMapsMaxCountRBRTM)
			{
				menuIdx = FindRBRTMMenuItemIdxByMapID(pMenuData, (*it)->mapID);

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

	int CalculateNumOfValidMapsInRecentList(PRBRRXMenuItem pMapMenuItemsRBRRX = nullptr, int numOfItemsMenuItemsRBRRX = 0)
	{
		int numOfRecentMaps = 0;
		int menuIdx;

		auto it = m_recentMapsRBRRX.begin();
		while (it != m_recentMapsRBRRX.end())
		{
			if (!(*it)->folderName.empty() && numOfRecentMaps < m_recentMapsMaxCountRBRRX)
			{
				menuIdx = FindRBRRXMenuItemIdxByFolderName(pMapMenuItemsRBRRX, numOfItemsMenuItemsRBRRX, (*it)->folderName);
				//DebugPrint("DEBUG: CalculateNumOfValidMapsInRecentList=%s %d", (*it)->folderName.c_str(), menuIdx);

				if (menuIdx >= 0 || pMapMenuItemsRBRRX == nullptr)
				{
					// The mapID in recent list is valid. Refresh the menu entry name as "[recent idx] Stage name"
					numOfRecentMaps++;
					if (pMapMenuItemsRBRRX != nullptr)
					{
						(*it)->name = "[" + std::to_string(numOfRecentMaps) + "] ";
						(*it)->name.append(pMapMenuItemsRBRRX[menuIdx].szTrackName);
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

	void AddMapToRecentList(int mapID)
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

	void AddMapToRecentList(std::string folderName)
	{
		if (folderName.empty()) return;
		_ToLowerCase(folderName); // RecentRBRRX vector list should have lowercase folder names

		// Add map folderName to top of the recentMaps list (if already in the list then move it to top, otherwise add as a new item and remove the last item if the list is full)
		for (auto& iter = m_recentMapsRBRRX.begin(); iter != m_recentMapsRBRRX.end(); ++iter)
		{
			if(folderName.compare((*iter)->folderName) == 0)
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


	inline const WCHAR* GetLangStr(const WCHAR* szStrKey) 
	{ 
		// Return localized version of the strKey string value (or the original value if no localization available)
		if (m_pLangIniFile == nullptr || szStrKey == nullptr || szStrKey[0] == L'\0')
			return szStrKey; 

		const WCHAR* szResult = m_pLangIniFile->GetValue(L"Strings", szStrKey, nullptr);
		/*
		if (szResult == nullptr && szStrKey != nullptr)
		{
			// No match, but let's try again without leading and/or trailing whitespace chars
			std::wstring sStrWithoutTrailingWhitespace(szStrKey);
			_Trim(sStrWithoutTrailingWhitespace);
			szResult = m_pLangIniFile->GetValue(L"Strings", sStrWithoutTrailingWhitespace.c_str(), szStrKey);
		}
		*/
		return (szResult != nullptr ? szResult : szStrKey);
	}

	inline const void AddLangStr(const WCHAR* szStrKey, const WCHAR* szResult)
	{
		if (m_pLangIniFile == nullptr || szStrKey == nullptr || szStrKey[0] == L'\0' || szResult == nullptr)
			return;

		// Add translation key only if it doesn't exist already
		if(m_pLangIniFile->GetValue(L"Strings", szStrKey, nullptr) == nullptr)
			m_pLangIniFile->SetValue(L"Strings", szStrKey, szResult);
	}

	inline const void AddLangStr(const WCHAR* szStrKey, const char* szResult)
	{
		if(szResult != nullptr)
			AddLangStr(szStrKey, _ToWString(szResult).c_str());
	}


	void /*HRESULT*/ CustomRBRDirectXBeginScene( /*void* objPointer*/ );
	HRESULT CustomRBRDirectXEndScene(void* objPointer);

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
