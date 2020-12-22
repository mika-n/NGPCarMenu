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
#include <forward_list>
#include <deque>

#include "PluginHelpers.h"

#include "SimpleINI\SimpleIni.h"    // INI file handling (uses https://github.com/brofield/simpleini library)
#include "Detourxs\detourxs.h"		// Detour API handling (uses https://github.com/DominicTobias/detourxs library)

#include "D3D9Helpers.h"			// Various text and D3D9 support functions created for this RBR plugin
#include "RBRAPI.h"					// RBR memory addresses and data structures

#include "SQLite/CppSQLite3U.h"

#define C_PLUGIN_TITLE_FORMATSTR "NGPCarMenu Plugin (%s) by MIKA-N"	// %s is replaced with version tag strign. Remember to tweak it in NGPCarMenu.rc when making a new release
#define C_PLUGIN_FOOTER_STR      "https://github.com/mika-n/NGPCarMenu"

#define C_DEBUGTEXT_COLOR   D3DCOLOR_ARGB(255, 255,255,255)      // White

#define C_CARSPECTEXT_COLOR			D3DCOLOR_ARGB(255, 0xE0,0xE0,0xE0)   // Grey-White
#define C_CARMODELTITLETEXT_COLOR	D3DCOLOR_ARGB(255, 0x7F,0x7F,0xFE)   // Light-blue

#define C_DARKGREYBACKGROUND_COLOR  D3DCOLOR_ARGB(255, 0x3D, 0x3D, 0x3D) // Dark grey
#define C_LIGHTGREYBACKGROUND_COLOR D3DCOLOR_ARGB(255, 0x7E, 0x7E, 0x7E) // Light grey
#define C_REDSEPARATORLINE_COLOR    D3DCOLOR_ARGB(255, 0xB2, 0x2B, 0x2B) // Dark red

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
	WCHAR wszCarModelFolder[128];	// Folder of the car physics (rbrApp\Cars\carModelFolder\)

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
	WCHAR wszCarPhysics[128];		   // Car physics name (NGP physics folder name)

	WCHAR wszCarFMODBank[128];		   // Custom FMOD sound bank
	WCHAR wszCarFMODBankAuthors[128];  // Custom FMOD sound bank

	WCHAR wszCarListSectionName[32];   // NGP carList.ini section name (Car_xxx or carID as string value if RSF installation)

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
	__int32 activeStatus;			// 0x04   0=The loaded track is not RBRTM track, 1=RBRTM plugin active and the loaded track is RBRTM track
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
	BOOL          shakedownOptionsFirstTimeSetup;  // Shakedown map selection shows a map preview. The shakedown options screen after the map selection shows also a map preview image. When this is TRUE then the option image is re-initialized.

	RBRTM_MapInfo() 
	{
		name.reserve(64);
		mapID = -1;
		mapIDMenuIdx = -1;
		length = -1;
		surface = -1;
		shakedownOptionsFirstTimeSetup = TRUE;
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
	char szTrackFolder[256];
	__int32 physicsID;			// BTB physics 0=gravel, 1=tarmac, 2=snow (derived from track.ini physics option)
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

// Offset rbr_rx.dll base addr
typedef struct {
#pragma pack(push,1)
	BYTE pad1[0x52588];			// 0x00	
	__int32 menuFocusWidth;		// 0x52588  (width of the red focus line in RBRRX menus)
	BYTE pad2[0x52590 - 0x52588  - sizeof(__int32)];
	__int32 menuPosX;			// 0x52590	(X pos of RBRRX menus)

	BYTE pad21[0x608D0 - 0x52590 - sizeof(__int32)];

	BYTE loadTrackStatusD0;		// 0x608D0  (Set to 0x01 when loading a track, otherwise 0x00)
	BYTE pad3[0x608D4 - 0x608D0 - sizeof(BYTE)];
	__int32 unknown1;			// 0x608D4
	__int32 loadTrackStatusD8;	// 0x608D8  (Set to 0x01 when loading a track, otherwise 0x00)
	__int32 loadTrackID;		// 0x608DC (menuIdx to the currently loaded BTB track)

	BYTE pad31[0x608E4 - 0x608DC - sizeof(__int32)];
	PRBRRXMenuItem pMenuItems;	// 0x608E4
	__int32 numOfItems;			// 0x608E8

	BYTE pad4[0x60904 - 0x608E8 - sizeof(__int32)];

	LPDIRECT3DDEVICE9 pRBRRXIDirect3DDevice9; // 0x60904
	
	BYTE pad5[0x60914 - 0x60904 - sizeof(LPDIRECT3DDEVICE9)];

	__int32 keyCode;			// 0x60914  The keycode of the last pressed key (key down) (37=Left arrowkey, 39=Right arrowkey, 36=Home, 35=End)
	__int32 menuID;				// 0x60918  0=main, 1=stages, 2=replay
	BYTE pad6[0x609A0 - 0x60918 - sizeof(__int32)];

								// 0x60958  Ptr to struct of BTB track definition (file names to various INI files of the loaded BTB track)?

	__int32 currentPhysicsID;	// 0x609A0  The current physicsID while loading a BTB track (0..2)
	__int32 loadTrackStatusA4;  // 0x609A4  LoadTrack status (0x01 = Loading track, 0x00 = Not loading)
	BYTE pad7[0x66528 - 0x609A4 - sizeof(__int32)];

	WCHAR wszTrackName[60];		// 0x66528  The name of the last selected track (multibyteToWideChar converted)
	BYTE pad8[0x665F4 - 0x66528 - sizeof(WCHAR)*60];
	PRBRRXMenuData pMenuData;	// 0x665F4  Ptr to selectedItemIdx struct
#pragma pack(pop)
} RBRRXPlugin;
typedef RBRRXPlugin* PRBRRXPlugin;

struct RBRRX_MapInfo {
	int			  mapIDMenuIdx;		// Menu index (or -1 if this struct is not valid)
	std::string   folderName;		// BTB map data folder (this folder should have track.ini file)
	int			  physicsID;		// BTB physics 0=gravel, 1=tarmac, 2=snow (track.ini physics option)

	std::string   name;				// Track.ini metadata. name=
	std::wstring  surface;			//		physics=
	std::string   author;			//		author=
	std::string   version;			//		version=
	std::string   date;				//      date=
	std::string   comment;			//      comment=

	double length;					// The length of the stage set by track.ini length option or via BTB track data (split1-split2-finish distance in RBR data structure)
	int    numOfPacenotes;			// Num of pacenotes entries in pacenotes.ini file (usually <20 notes means that notes are not set)

	std::wstring  previewImageFile;
	IMAGE_TEXTURE imageTexture;
	IMAGE_TEXTURE imageTextureLoadTrack;
	BOOL          trackOptionsFirstTimeSetup;  // Map selection shows a map preview. The track options screen after the map selection shows also a map preview image. When this is TRUE then the option image is re-initialized.

	RBRRX_MapInfo()
	{
		mapIDMenuIdx = -1;
		length = -1;
		numOfPacenotes = 0;

		physicsID = 0;
		name.reserve(64);
		folderName.reserve(256);
		surface.reserve(16);
		previewImageFile.reserve(260);
		trackOptionsFirstTimeSetup = TRUE;

		ZeroMemory(&imageTexture, sizeof(IMAGE_TEXTURE));
		ZeroMemory(&imageTextureLoadTrack, sizeof(IMAGE_TEXTURE));
	}

	~RBRRX_MapInfo()
	{
		SAFE_RELEASE(imageTexture.pTexture);
		SAFE_RELEASE(imageTextureLoadTrack.pTexture);
	}

	void Clear()
	{
		length = -1;
		numOfPacenotes = 0;
		surface.clear();
		author.clear();
		version.clear();
		date.clear();
	}
};


//------------------------------------------------------------------------------------------------
// Minimap structs
//
typedef struct _POINT_DRIVELINE_float
{
	POINT_float	drivelineCoord;	// X,Y coordinate from the original driveline data
	float drivelineDistance;	// Distance (from the beginning of driveline data)

	bool operator<(const struct _POINT_DRIVELINE_float& b) const
	{
		return (drivelineDistance < b.drivelineDistance);
	}

	bool operator==(const struct _POINT_DRIVELINE_float& b) const
	{
		return (drivelineCoord == b.drivelineCoord && drivelineDistance == b.drivelineDistance);
	}
} POINT_DRIVELINE_float;

class CDrivelineSource
{
public:
	std::list<POINT_DRIVELINE_float> vectDrivelinePoint;	// Driveline data points x,y and distance
	POINT_float pointMin;	// Minimum and maximum x and y values in the driveline data (used to calculate the size and aspect ratio of the minimap graph)
	POINT_float pointMax;	//

	float startDistance;	// The distance of start line from the beginning of the driveline data (pacenote type21).
	float split1Distance;	//   split 1 (pacenote type23). Some BTB tracks don't have this split point.
	float split2Distance;	//   split 2 (pacenote type23). Some BTB tracks don't have this split point.
	float finishDistance;	// The distance of finish line from the beginning of the driveline data (pacenote type22)

	CDrivelineSource()
	{
		startDistance = split1Distance = split2Distance = finishDistance = -1;
		ZeroMemory(&pointMin, sizeof(POINT_float));
		ZeroMemory(&pointMax, sizeof(POINT_float));
	}
};


typedef struct _POINT_DRIVELINE_int
{
	POINT_int drivelineCoord;
	int splitType;				// 0 = Start->split1, 1 = Split1->Split2, 2=Split2->Finish

	bool operator<(const struct _POINT_DRIVELINE_int& b) const
	{
		return (drivelineCoord < b.drivelineCoord);
	}

	bool operator==(const struct _POINT_DRIVELINE_int& b) const
	{
		return (drivelineCoord == b.drivelineCoord && splitType == b.splitType);
	}
} POINT_DRIVELINE_int;

class CMinimapData
{
public:
	std::list<POINT_DRIVELINE_int> vectMinimapPoint;	// Scaled-down coordinates of the minimap (INT)
	RECT minimapRect;									// Scaled-down rectangle area
	POINT_int minimapSize;								// The actual width and height of the minimap

	std::string trackFolder;							// The name of the BTB track folder (cached minimap identifier if the map is for RBRRX/BTB)
	int mapID;											// MapID of the RBRTM track (cached minimap ID if the map is for RBRTM)

	CMinimapData()
	{
		mapID = -1;
		ZeroMemory(&minimapSize, sizeof(POINT_int));
		ZeroMemory(&minimapRect, sizeof(RECT));
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

extern int     __fastcall CustomRBRReplay(void* objPointer, DWORD dummyEDX, const char* szReplayFileName, __int32* pUnknown1, __int32* pUnknown2, size_t iReplayFileSize);
extern int     __fastcall CustomRBRSaveReplay(void* objPointer, DWORD dummyEDX, const char* szReplayFileName, __int32 mapID, __int32 carID, __int32 unknown1);

extern float   __fastcall CustomRBRControllerAxisData(void* objPointer, DWORD dummyEDX, __int32 axisID);

extern RBRCarSelectionMenuEntry g_RBRCarSelectionMenuEntry[];

extern wchar_t* g_pOrigLoadReplayStatusText;
extern wchar_t  g_wszCustomLoadReplayStatusText[];


#if USE_DEBUG == 1
extern CD3DFont* g_pFontDebug;
#endif
extern CD3DFont* g_pFontCarSpecCustom;	// Custom car spec text font style
extern CD3DFont* g_pFontCarSpecModel;	// Custom model spec font style (author of car and map model, a bit smaller than SpecCustom font style)

extern CD3DFont* g_pFontRBRRXLoadTrack;	    // Custom RBRRX LoadingTrack font style
extern CD3DFont* g_pFontRBRRXLoadTrackSpec; //

extern PRBRPluginMenuSystem g_pRBRPluginMenuSystem;

extern std::vector<std::string>* g_pRBRRXTrackNameListAlreadyInitialized;

extern CppSQLite3DB* g_pRaceStatDB;


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
	CMinimapData m_minimapData; // if imageFileName is *.trk then this is a minimap image

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

class CRBRPluginIntegratorLinkText
{
public:
	int   m_textID;
	DWORD m_fontID;
	
	int          m_posX, m_posY;
	std::wstring m_wsText;
	std::string  m_sText;	
	DWORD        m_dwColor;
	DWORD        m_dwDrawOptions;

	CRBRPluginIntegratorLinkText()
	{
		m_textID = -1;
		m_fontID = 0;
		m_dwColor = C_CARSPECTEXT_COLOR;
		m_dwDrawOptions = 0;
		m_posX = m_posY = 0;
	}

	~CRBRPluginIntegratorLinkText()
	{
		// Do nothing
	}
};

class CRBRPluginIntegratorLink
{
public:
	int m_iCustomPluginMenuIdx;			// Index to custom plugin
	bool m_bCustomPluginActive;			// Is the custom plugin active in RBR menu system?
	std::string m_sCustomPluginName;	// Name of the custom plugin (as shown in RBR Plugins menu)

	std::vector<std::unique_ptr<CRBRPluginIntegratorLinkImage>> m_imageList; // Image cache (DX9 textures)
	std::vector<std::unique_ptr<CRBRPluginIntegratorLinkText>> m_textList;   // Text cache

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
		m_textList.clear();
	}
};


//------------------------------------------------------------------------------------------------
//
typedef struct {
	int  MapKey;
	std::string StageName;	// len 128
	char Surface;
	int  Length;
	std::string Format;		// 3		RBR / BTB
	int  MapID;
	std::string Folder;		// 128
} RaceStat_Map;
typedef RaceStat_Map* PRaceStat_Map;

typedef struct {
	int  CarKey;
	std::string ModelName;		// Len 128
	std::string FIACategory;	// 128
	std::string Physics;		// 128
	std::string INIFile;		// 128
	std::string Folder;			// 128
	std::string NGPRevision;	// 128
	int NGPVersion;
	int NGPCarID;
} RaceStat_Car;
typedef RaceStat_Car* PRaceStat_Car;


//------------------------------------------------------------------------------------------------
// CNGPCarMenu class. Custom car preview images in RBR and RBRTM "select a car" menus.
//
class CNGPCarMenu : public IPlugin
{
protected:
	std::string m_sPluginTitle;		// Title of the plugin shown in NGPCarMenu menus
	CSimpleIniW* m_pLangIniFile;	// NGPCarMenu language localization file

	int	m_iCarMenuNameLen;			// Max char space reserved for the current car menu name menu items (calculated in CalculateMaxLenCarMenuName method)

	std::wstringstream m_autoLogonSequenceLabel;		// Text label of the latest autoLogon sequene. For example "main/options/plugins/rbr_rx"
	std::deque<std::string> m_autoLogonSequenceSteps;	// Sequence steps of the autologon
	int   m_iAutoLogonMenuState;        // State step of the autologon procedure (0=completed or not used, 1-2=LoadProfile steps, 3=MainMenu navigation steps)
	DWORD m_dwAutoLogonEventStartTick;  // The start tick of the previous auto-logon event. If autologon takes too long then it is aborted (timeout)
	bool  m_bShowAutoLogonProgressText; // Show the title of auto-logon sequence

	bool m_bMenuSelectCarCustomized;	// TRUE - The SelectCar is in customized state, FALSE - Various original values restored (ie. tyre brand names)

	int m_iMenuCurrentScreen;	// The current menu screen (0=Main, C_CMD_RENAMEDRIVER)

	int m_iMenuSelection;		// Currently selected menu line idx
	int	m_iMenuCreateOption;	// 0 = Generate all car images, 1 = Generate only missing car images
	int	m_iMenuImageOption;		// 0 = Use PNG preview file format to read and create image files, 1 = BMP file format
	int m_iMenuRBRTMOption;		// 0 = RBRTM integration disabled, 1 = Enabled
	int m_iMenuRBRRXOption;		// 0 = RBRRX integration disabled, 1 = Enabled
	int m_iMenuAutoLogonOption; // 0 = Disabled, 1=Main, 2=Plugins, 3+ custom plugin
	int m_iMenuRallySchoolMenuOption; // 0 = Disabled, 1=Main, 2=Plugins, 3+ custom plugin

	bool m_bFirstTimeWndInitialization; // Do "first time RBR initializations" in EndScene

	bool m_bMapLoadedCalled;	// TRUE-MapLoaded event handler already called for this racing or replying
	bool m_bMapUnloadedCalled;  // TRUE-MapUnloaded event handler already called for this racing or replaying

	bool m_bRenameDriverNameActive;		// TRUE=Renaming of driver profile process is active (NGPCarMenu checks if the creation succeeded and takes a backup of the prev profile before completing the renaming)
	int  m_iProfileMenuPrevSelectedIdx; //
	std::string m_sMenuPrevDriverName;	// Previous driver name
	std::string m_sMenuNewDriverName;	// New driver name

	std::string m_sMenuStatusText1;	// Status text message 
	std::string m_sMenuStatusText2;
	std::string m_sMenuStatusText3;

	std::forward_list<std::unique_ptr<CMinimapData>> m_cacheRBRRXMinimapData; // Cached minimap data (when a minimap is generated at runtime it is cached here to speed up the next calculation)
	std::forward_list<std::unique_ptr<CMinimapData>> m_cacheRBRTMMinimapData; // 

	POINT m_rbrWindowPosition;			// Custom position of RBR wnd (or if x=-1 and y=-1 then RBR wnd is opened at default location on the primary monitor)

	std::string m_raceStatDBFilePath;	// Path to race stat DB folder or empty if feature is disabled

	DetourXS* gtcDirect3DBeginScene;
	DetourXS* gtcDirect3DEndScene;
	DetourXS* gtcRBRReplay;
	DetourXS* gtcRBRSaveReplay;
	DetourXS* gtcRBRControllerAxisData;

	static BOOL CALLBACK MonitorEnumCallback(HMONITOR hMon, HDC hdc, LPRECT lprcMonitor, LPARAM pData);

	void StartNewAutoLogonSequence(bool showAutoLogonProgressText = true);
	void DoAutoLogonSequence();

	void CompleteProfileRenaming();

	void InitCarSpecData_RBRCIT(int updatedCarSlotMenuIdx = -1); // If updatedCarSlot==-1 then update all slots, otherwise only the specified slot
	void InitCarSpecData_EASYRBR();
	void InitCarSpecAudio();

	bool InitCarModelNameFromCarsFile(CSimpleIniWEx* stockCarListINIFile, PRBRCarSelectionMenuEntry pRBRCarSelectionMenuEntry, int menuIdx);
	bool InitCarSpecDataFromPhysicsFile(const std::string& folderName, PRBRCarSelectionMenuEntry pRBRCarSelectionMenuEntry, int* outNumOfGears);
	bool InitCarSpecDataFromNGPFile(CSimpleIniWEx* ngpCarListINIFile, PRBRCarSelectionMenuEntry pRBRCarSelectionMenuEntry, int numOfGears);

	void RefreshSettingsFromPluginINIFile(bool addMissingSections = false);
	void SaveSettingsToPluginINIFile();
	void SaveSettingsToRBRTMRecentMaps();
	void SaveSettingsToRBRRXRecentMaps();

	int  CalculateMaxLenCarMenuName();
	void ClearCachedCarPreviewImages();

	int InitPluginIntegration(const std::string& customPluginName, bool bInitRBRTM);
	int InitAllNewCustomPluginIntegrations();

	bool RaceStatDB_Initialize(); // Initialize and open raceStat DB
	int	 RaceStatDB_GetMapKey(int mapID, const std::string& mapName, int racingType);	// Return MapKey for the map or add new D_Map record
	int	 RaceStatDB_GetCarKey(int carSlotID); // Return CarKey for the car defined in carSlotID or add new D_Car record
	int	 RaceStatDB_AddMap(int mapID, const std::string& mapName, int racingType);		// Add a new map to D_Map table
	int	 RaceStatDB_AddCar(int carSlotID); // Add a new car to D_Car table with details of the car in carSlotID
	int  RaceStatDB_AddCurrentRallyResult(int mapKey, int mapID, int carKey, int carSlotID, const std::string& mapName); // Add the current rally data to raceStatDB

	int GetNextScreenshotCarID(int currentCarID);
	static bool PrepareScreenshotReplayFile(int carID);

	std::wstring ReplacePathVariables(const std::wstring& sPath, int selectedCarIdx = -1, bool rbrtmplugin = false, int mapID = -1, const WCHAR* mapName = nullptr, const std::string& folderName = "");
	bool ReadCarPreviewImageFromFile(int selectedCarIdx, float x, float y, float cx, float cy, IMAGE_TEXTURE* pOutImageTexture, DWORD dwFlags = 0, bool isRBRTMPlugin = false);

	std::wstring GetMapNameByMapID(int mapID)
	{
		std::wstring sResult;
		if (m_pTracksIniFile != nullptr)
		{
			WCHAR wszMapINISection[16];
			swprintf_s(wszMapINISection, COUNT_OF_ITEMS(wszMapINISection), L"Map%02d", mapID);
			sResult = m_pTracksIniFile->GetValueEx(wszMapINISection, L"", L"StageName", L"");
		}

		return sResult;
	}

	void DrawProgressBar(D3DRECT rec, float progressValue, LPDIRECT3DDEVICE9 pOutputD3DDevice = nullptr);

	BOOL ReadStartSplitFinishPacenoteDistances(double* pStartDistance, double* pSplit1Distance, double* pSplit2Distance, double* pFinishDistance); // Read start/splits/finish notes from the currently loaded track

	void RBRRX_EndScene();
	void RBRTM_EndScene();

	void RBRRX_OnMapLoaded();
	void RBRRX_OverrideLoadTrackScreen();
	BOOL RBRRX_PrepareReplayTrack(const std::string& mapName);
	BOOL RBRRX_PrepareLoadTrack(int mapMenuIdx);

	void   RBRRX_FocusNthMenuIdxRow(int menuIdx);
	//bool   RBRRX_ReadStageStartAndEndPacenote(const std::string& pacenoteFileName, double* pStartDistance, double* pFinishDistance);
	void   RBRRX_UpdateMapInfo(int menuIdx, RBRRX_MapInfo* pRBRRXMapInfo);
	double RBRRX_UpdateINILengthOption(const std::string& sFolderName, double newLength);

	std::string RBRRX_FindFolderNameByMapName(std::string mapName);
	int  RBRRX_FindMenuItemIdxByFolderName(PRBRRXMenuItem pMapMenuItemsRBRRX, int numOfItemsMenuItemsRBRRX, const std::string& folderName);
	int  RBRRX_FindMenuItemIdxByMapName(PRBRRXMenuItem pMapMenuItemsRBRRX, int numOfItemsMenuItemsRBRRX, std::string mapName);
	int  RBRRX_CalculateNumOfValidMapsInRecentList(PRBRRXMenuItem pMapMenuItemsRBRRX = nullptr, int numOfItemsMenuItemsRBRRX = 0);
	void RBRRX_AddMapToRecentList(std::string folderName);

	BOOL RBRRX_ReadStartSplitsFinishPacenoteDistances(const std::string& folderName, double* startDistance, double* split1Distance, double* split2Distance, double* finishDistance);
	int  RBRRX_ReadDriveline(const std::string& folderName, CDrivelineSource& drivelineSource );
	void RBRRX_DrawMinimap(const std::string& folderName, int screenID, LPDIRECT3DDEVICE9 pOutputD3DDevice = nullptr);


	void RBRTM_OnMapLoaded();
	int  RBRTM_FindMenuItemIdxByMapID(PRBRTMMenuItem pMenuItems, int numOfItems, int mapID);
	int  RBRTM_FindMenuItemIdxByMapID(PRBRTMMenuData pMenuData, int mapID);
	int  RBRTM_CalculateNumOfValidMapsInRecentList(PRBRTMMenuData pMenuData = nullptr);
	void RBRTM_AddMapToRecentList(int mapID);
	BOOL RBRTM_ReadStartSplitsFinishPacenoteDistances(const std::wstring& trackFileName, float* startDistance, float* split1Distance, float* split2Distance, float* finishDistance);
	int  RBRTM_ReadDriveline(int mapID, CDrivelineSource& drivelineSource);
	void RBRTM_DrawMinimap(int mapID, int screenID);

public:
	IRBRGame*     m_pGame;
	T_PLUGINSTATE m_PluginState;

	bool m_bRBRFullscreenDX9;			// Is RBR running in fullscreen or windows DX9 mode? TRUE-fullscreen, FALSE=windowed

	bool m_bPacenotePluginInstalled;	// Is Pacenote plugin used? RBR exit logic handles font cleanup a bit differently in fullscreen mode IF pacenote plugin is missing
	bool m_bRallySimFansPluginInstalled;// Is this RallySimFans plugin version of RBR? It modifies Cars\Cars.ini and physics file on the fly to change the list of installed cars, so NGPCarMenu needs to refresh car specs when RSF modifies those
	bool m_bRBRTMPluginInstalled;		// Is this RBRTM plugin version of RBR installation?

	UINT m_iPhysicsNGMajorVer;				// Version of the NGP physics library (PhysicsNG.dll)
	UINT m_iPhysicsNGMinorVer;
	UINT m_iPhysicsNGPatchVer;
	UINT m_iPhysicsNGBuildVer;

	bool m_bRBRProInstalled;		// Is this RBRPro RBR game instance setup by RBRPro manager?
	std::string m_sRBRProVersion;	// Version of the RBRProManager.exe
	std::string m_sRSFVersion;		// Version of the RSF plugin

	std::string  m_sRBRRootDir;  // RBR app path, multibyte (or normal ASCII) string
	std::wstring m_sRBRRootDirW; // RBR app path, widechar string

	std::string m_sRallySchoolMenuReplacement;  // RallySchool menu replacement (Disabled, Main, Plugins or custom plugin name)

	std::string m_sAutoLogon;					// The current auto-logon option name (Disabled, Main, Plugins or custom plugin name)
	bool m_bAutoLogonWaitProfile;				// TRUE=Wait for user to choose a profile until continuing the autoprofile process (multiprofile scenarios may need this)
	
	bool m_bAutoExitAfterReplay;				// TRUE=Auto-exit RBR when focus returns to RBR main menu (used only in AutoLogon=ReplayAndExit replay mode, not really relevant in other cases)

	std::wstring m_screenshotPath;				// Path to car preview screenshot images (by default AppPath + \plugins\NGPCarMenu\preview\XResxYRes\)
	int m_screenshotAPIType;					// Uses DIRECTX or GDI API technique to generate a new screenshot file. 0=DirectX (default), 1=GDI. No GUI option, so tweak this in NGPCarMenu.ini file.
	std::wstring m_screenshotReplayFileName;	// Name of the RBR replay file used when car preview images are generated

	std::wstring m_rbrCITCarListFilePath;		// Path to RBRCIT carList.ini file (the file has NGP car details and specs information)
	std::wstring m_easyRBRFilePath;				// Path to EesyRBR installation folder (if RBRCIT car manager is not used)

	RECT  m_screenshotCroppingRect;				// Cropping rect of a screenshot (in RBR window coordinates)
	int   m_screenshotCarPosition;				// 0=The default screenshot location for a car (on the road), 1=The car is moved "out-of-scope" in the middle of nowhere (night mode of RBR can be used to create screenshots with black background)

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

	LPDIRECT3DVERTEXBUFFER9 m_minimapVertexBuffer; // Minimap rect vertex to visualize map layout as 2D overview map

	int  m_iCustomReplayState;					// 0 = No custom replay (default RBR behaviour), 1 = Custom replay process is running. This plugin takes screenshots of car models
	std::chrono::steady_clock::time_point m_tCustomReplayStateStartTime;

	int  m_iCustomReplayCarID;					// 0..7 = The current carID used in a custom screenshot replay file
	int  m_iCustomReplayScreenshotCount;		// Num of screenshots taken so far during the last "CREATE car preview images" command
	bool m_bCustomReplayShowCroppingRect;		// Show the current car preview screenshot cropping rect area on screen (ie. few secs before the screenshot is taken)

	IMAGE_TEXTURE m_carPreviewTexture[8];		// 0..7 car preview image data (or NULL if missing/not loaded yet)	
	IMAGE_TEXTURE m_carRBRTMPreviewTexture[8];  // 0..7 car preview image for RBRTM plugin integration if RBRTM integration is enabled (the same pic as in standard preview image texture, but re-scaled to fit the smaller picture are in RBRTM screen)

	CSimpleIniWEx* m_pTracksIniFile;			// maps\Tracks.ini file (if RBRTM integration is enabled then splash/preview images are shown in Shakedown map selection list)

	std::wstring m_screenshotPathMapRBRTM;		// Custom map preview image path
	RBRTM_MapInfo m_latestMapRBRTM;				// The latest selected stage (mapID) in RBRTM Shakedown menu (if the current mapID is still the same then no need to re-load the same stage preview image)

	RECT m_mapRBRTMPictureRect[2];			// Output rect of RBRTM map preview image (re-scaled pic area) (idx 0=stages menu list, 1=stage options screen)
	RECT m_minimapRBRTMPictureRect[2];		// Location of the minimap in RBRRX stages list (screenID=0 menu lsit, screenID=1 stage options screen)

	int m_recentMapsMaxCountRBRTM;				// Max num of recent stages/maps added to recentMaps vector (default 5 if not set in INI file. 0 disabled the custom Shakedown menu feature)
	std::list<std::unique_ptr<RBRTM_MapInfo>> m_recentMapsRBRTM; // Recent maps shown on top of the Shakedown stage list
	bool m_bRecentMapsRBRTMModified;			// Is the recent maps list modified since the last INI file saving?

	PRBRTMMenuData m_pOrigMapMenuDataRBRTM;		// The original RBRTM Shakedown menu data struct
	PRBRTMMenuItem m_pOrigMapMenuItemsRBRTM;	// The original RBRTM Shakedown stages menu item array
	int            m_origNumOfItemsMenuItemsRBRTM; // The original num of menu items in Stages menu list
	PRBRTMMenuItem m_pCustomMapMenuRBRTM;		// Custom "list of stages" menu in RBRTM Shakedown menu (contains X recent shortcuts and the original list of stages)
	int m_numOfItemsCustomMapMenuRBRTM;			// Num of items in m_pCustomMapMenuRBRTM (dynamic) array

	bool m_bShowCustomLoadTrackScreenRBRRX;     // Show custom LoadTrack RBRRX screen (TRUE) or the original grey RBRRX debug msg screen (FALSE)
	LPDIRECT3DVERTEXBUFFER9 g_mapRBRRXRightBlackBarVertexBuffer;
	std::wstring m_screenshotPathMapRBRRX;		// Custom map preview image path
	RBRRX_MapInfo m_latestMapRBRRX;				// The latest selected stage in RBRRX menu (if the current mapID is still the same then no need to re-load the same stage preview image)

	RECT m_mapRBRRXPictureRect[3];		// Output rect of RBRRX map preview image (re-scaled pic area) (idx 0=stages menu list, 1=stage options, 2=stage loading screen)
	RECT m_minimapRBRRXPictureRect[3];	// Location of the minimap in RBRRX stages list

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
	bool   m_bRBRTMTrackLoadBugFixWhenNotActive;// TRUE=When RBRTM is no longer active then fix RBRTM bug where non-RBRTM (quickRally or RBRRX/BTB) track loading doesn't work anymore once RBRTM tracks were loaded, FALSE=Don't do the fix (other tracks won't work after using RBRTM unless RBR is restarted)

	int    m_iRBRRXPluginMenuIdx;				// Index of the RBR_RX plugin in the RBR Plugins menu list (this way we know when RBRRX custom plugin in Nth index position is activated)
	bool   m_bRBRRXPluginActive;				// TRUE/FALSE if the current active custom plugin is RBR_RX (active = The RBRTM plugin handler is running in foreground)
	bool   m_bRBRRXReplayActive;				// TRUE=Replay is playing RBRRX BTB track, FALSE = standard RBR replay file and track
	bool   m_bRBRRXRacingActive;				// TRUE=Racing on track #41 is RBRX BTB track
	bool   m_bRBRRXLoadingNewTrack;				// TRUE=Loading RBRRX track and first-time custom LoadTrack screen initialization


	bool   m_bRBRReplayOrRacingEnding;
	bool   m_bRBRRacingActive;					// TRUE=Racing active (if m_bRBRRXRacingActive is TRUE also then it is BTB racing and not standard RBR classic track racing)
	bool   m_bRBRReplayActive;					// TRUE=Track loading for the replay purposes (if m_bRBRRXReplayActive is TRUE also then it is BTB replay and not standard track replay), FALSE=No active replay

	float  m_prevRaceTimeClock;					// The previous value of race clock (used to check if there are new time penalties)
	float  m_latestFalseStartPenaltyTime;		// If there was a false start then this has the false start penalty time (10 sec base penalty + extra time depending on how much early)
	float  m_latestOtherPenaltyTime;			// Other penalties during the latest rally (not including false start penalty, but including callForHelp, cut penalties and so on...)

	int    m_latestCarID;						// The latest carID in racing mode (slot#, not menuIdx)
	int    m_latestMapID;						// The latest mapID in racing mode
	//int    m_latestMapLength;					// The latest map length in meters in racing mode
	std::wstring m_latestMapName;				// The latest map (if the map is BTB then the value is NAME value from btb track.ini file, otherwise the classic map Tracks.ini name)

	bool   m_bGenerateReplayMetadataFile;		// Generate replayFileName.ini metadata files

	PRBRMenuObj  m_pRBRPrevCurrentMenu;			// If RBRTM or RBRRX integration is enabled then NGPCarMenu must try to identify Plugins menuobj and RBRTM/RBRRX plugin. This is just a "previous currentMenu" in order to optimize the check routine (ie. don't re-check if the plugin is RBRTM until new menu/plugin is activated)

	PRBRTMPlugin m_pRBRTMPlugin;				// Pointer to RBRTM plugin or nullptr if not found or RBRTM integration is disabled	
	PRBRRXPlugin m_pRBRRXPlugin;				// Pointer to RBRRX plugin or nullptr

	CD3D9RenderStateCache* m_pD3D9RenderStateCache; // DirectX cache class to restore DX9 render state back to original settings after tweaking render state

	//------------------------------------------------------------------------------------------------

	CNGPCarMenu(IRBRGame* pGame);
	virtual ~CNGPCarMenu(void);

	BOOL ReadStartSplitsFinishPacenoteDistances(const std::wstring& sPacenoteFileName, float* startDistance, float* split1Distance, float* split2Distance, float* finishDistance);
	int  ReadDriveline(const std::wstring& sDrivelineFileName, CDrivelineSource& drivelineSource);
	int  ReadDriveline(const std::string& sDrivelineFileName, CDrivelineSource& drivelineSource);
	int  RescaleDrivelineToFitOutputRect(CDrivelineSource& drivelineSource, CMinimapData& minimapData);

	std::string GetRBRInstallType();	// TM=RBR using RBRTM plugin, RSF=RBR using RSF plugin, UNK=Unknown installation type

	std::string GetActivePluginName();	// RBR=Normal RBR (ie. no custom plugin), RBRTM, RBRRX, RSF
	int GetActiveReplayType();			// 0=no replay, 1=Normal RBR/TM/RSF (classic map), 2=BTB (rbrrx/btb map)
	int GetActiveRacingType();			// 0=no racing, 1=Normal RBR/TM/RSF (classic map), 2=BTB (rbrrx/btb map)
	
	void OnPluginActivated(const std::string& pluginName);
	void OnPluginDeactivated(const std::string& pluginName);

	void OnRaceStarted();		// Race started
	void OnMapLoaded();			// Map (replay or racing) loading completed and countdown to zero starts soon
	void OnMapUnloaded();		// Map is about to be unloaded (if racing was active then the race is about to end also)
	void OnRaceEnded();			// Race ended

	void OnReplayStarted();		// Replay started
	void OnReplayEnded();		// Replay ended

	inline const WCHAR* GetLangStr(const WCHAR* szStrKey) 
	{ 
		// Return localized version of the strKey string value (or the original value if no localization available)
		if (m_pLangIniFile == nullptr || szStrKey == nullptr || szStrKey[0] == L'\0')
			return szStrKey; 

		const WCHAR* szResult = m_pLangIniFile->GetValue(L"Strings", szStrKey, nullptr);
		return (szResult != nullptr ? szResult : szStrKey);
	}

	inline const std::wstring GetLangWString(const WCHAR* szStrKey, bool autoTrailingSpace = false)
	{
		std::wstring sResult = GetLangStr(szStrKey);
		if (autoTrailingSpace && !sResult.empty()) sResult += L" ";
		return sResult;
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
		if(szResult != nullptr) AddLangStr(szStrKey, _ToWString(szResult).c_str());
	}

	void /*HRESULT*/ CustomRBRDirectXBeginScene( /*void* objPointer*/ );
	HRESULT CustomRBRDirectXEndScene(void* objPointer);

	BOOL CustomRBRReplay(const char* szReplayFileName);
	BOOL CustomRBRSaveReplay(const char* szReplayFileName);

	//void CompleteSaveReplayProcess(const std::list<std::wstring>& replayFileQueue);
	void CompleteSaveReplayProcess(const std::string& replayFileName);

	void RBRRX_CustomLoadTrackScreen();
	BOOL RBRRX_PrepareLoadTrack(const std::string& mapName, std::string mapFolderName);
	int  RBRRX_CheckTrackLoadStatus(const std::string& mapName, std::string mapFolderName, BOOL preLoadCheck = FALSE);

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
