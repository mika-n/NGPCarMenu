#ifndef __NGPCARMENUAPI_H_INCLUDED
#define __NGPCARMENUAPI_H_INCLUDED

//
// NGPCarMenuAPI. API interface to draw custom images from another plugins without worrying about DirectX and RBR texture/bitmap handling requirements. 
// How to use this API?
//  - Include this NGPCarMenuAPI.h file in your own plugin code
//  - Instantiate CNGPCarMenuAPI class object within your plugin code
//  - Register a link between your plugin and NGPCarMenu backend plugin by calling InitializePluginIntegration method (ie. you need to have NGPCarMenu.dll installed in RBR\Plugins\ folder)
//  - Load images (PNG/BMP/JPG) and specify size and location by calling LoadCustomImage method (each image should have an unique imageID identifier. It is up to you to generate this ID number)
//  - Show or hide a specified image (imageID) based on events in your own plugin by calling ShowHideImage method
//
//  - The same with custom text (plugin integration needs to be registered first then initialize a font and finally call drawTextA/drawTextW methods).
//  - To clear/hide a text call DrawTextA/W method (specify textID) with nullptr string pointer or with an empty string.
//
//  - See the sample plugin code for more details (TestPlugin.h file).
//
// Copyright 2020, MIKA-N. https://github.com/mika-n
//
// This NGPCarMenuAPI.h file and interface is provided by MIKA-N free of charge. Use at your own risk. No warranty given whatsoever in any direct or in-direct consequences. 
// You can modify this file and CNGPCarMenuAPI class as long the original copyright notice is also included in the derived work and those changes in this file are published as an open-source results (it is up to you to decide where to publish those changes).
// 

#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#include <windows.h>

#include <shlwapi.h>		// PathRemoveFileSpec
#pragma comment(lib, "Shlwapi.lib")

#include <string.h>

typedef DWORD (APIENTRY *tAPI_InitializePluginIntegration)(LPCSTR szPluginName);
typedef BOOL  (APIENTRY *tAPI_LoadCustomImage)(DWORD pluginID, int imageID, LPCSTR szFileName, const POINT* pImagePos, const SIZE* pImageSize, DWORD dwImageFlags);
typedef void  (APIENTRY *tAPI_ShowHideImage)(DWORD pluginID, int imageID, bool showImage);
typedef void  (APIENTRY *tAPI_RemovePluginIntegration)(DWORD pluginID);
typedef void  (APIENTRY *tAPI_MapRBRPointToScreenPoint)(const float srcX, const float srcY, float* trgX, float* trgY);

typedef BOOL  (APIENTRY *tAPI_GetVersionInfo)(char* pOutTextBuffer, size_t textBufferSize, int* pOutMajor, int* pOutMinor, int* pOutPatch, int* pOutBuild);

typedef DWORD (APIENTRY *tAPI_InitializeFont)(const char* fontName, DWORD fontSize, DWORD fontStyle);
typedef BOOL  (APIENTRY *tAPI_DrawTextA)(DWORD pluginID, int textID, int posX, int posY, const char* szText, DWORD fontID, DWORD color, DWORD drawOptions);
typedef BOOL  (APIENTRY* tAPI_DrawTextW)(DWORD pluginID, int textID, int posX, int posY, const wchar_t* wszText, DWORD fontID, DWORD color, DWORD drawOptions);

typedef BOOL  (APIENTRY* tAPI_PrepareBTBTrackLoad)(DWORD pluginID, LPCSTR szBTBTrackName, LPCSTR szBTBTrackFolderName);     // Prepare track #41 (CortezArbroz) for a BTB track loading. Custom plugin should call m_pGame->StartGame(...) IRBR method to start a rally after calling this prepare function
typedef BOOL  (APIENTRY* tAPI_CheckBTBTrackLoadStatus)(DWORD pluginID, LPCSTR szBTBTrackName, LPCSTR szBTBTrackFolderName); // Check the status of BTB track loading to make sure driver didn't end up in #41 (CortezArbroz) original track


#define C_NGPCARMENU_DLL_FILENAME "\\Plugins\\NGPCarMenu.dll"

// Image flags to finetune how the image is scaled and positioned
// Value 0 = Stretch the image to fill the rectangle area (X,Y, X+CX,Y+CY)
// Value 1 = Keep the original aspect ratio of the image and place the img to top of the rectangle area
// Value 2 = (not relevant, because stretching would fill the rectangle area anyway, so doesn't matter if the img is aligned with top or bottom edge)
// Value 3 = Keep the aspect ratio and place the img to bottom of the rectangle area
#define IMAGE_TEXTURE_SCALE_PRESERVE_ASPECTRATIO 0x01  // Bit1: 1=KeepAspectRatio, 0=Stretch the imaeg to fill the rendering rectangle area
#define IMAGE_TEXTURE_POSITION_BOTTOM			 0x02  // Bit2: 1=Image positioned on the bottom of the area, 0=Top of the area
#define IMAGE_TEXTURE_ALPHA_BLEND				 0x04  // Bit3: 1=Use alpha blending if PNG has alpha channel (usually transparent background color), 0=No alpha blending
#define IMAGE_TEXTURE_POSITION_HORIZONTAL_CENTER 0x08  // Bit4: 1=Align the picture horizontally in center position in the drawing rectangle area. 0=Left align
#define IMAGE_TEXTURE_POSITION_VERTICAL_CENTER   0x10  // Bit5: 1=Align the picture vertically in center position in the drawing rectangle area. 0=Top align (unless POSITION_BOTTOM is set)
#define IMAGE_TEXTURE_SCALE_PRESERVE_ORIGSIZE    0x20  // Bit6: 1=Keep the original picture size but optionally center it in drawing rectangle. 0=If drawing rect is defined then scale the picture (keeping aspect ratio or ignoring aspect ratio)
#define IMAGE_TEXTURE_POSITION_HORIZONTAL_RIGHT  0x40  // Bit7: 1=Align the picture horizontally to right (0=left align if neither horizontal_center is set)

#define IMAGE_TEXTURE_POSITION_VERTICAL_BOTTOM   IMAGE_TEXTURE_POSITION_BOTTOM   // Alias name because vertical-bottom img alignment flag is the same as POSITION_BOTTOM flag

#define IMAGE_TEXTURE_STRETCH_TO_FILL			 0x00  // Default behaviour is to stretch the image to fill the specified draw area, no alpha blending
#define IMAGE_TEXTURE_PRESERVE_ASPECTRATIO_TOP	  (IMAGE_TEXTURE_SCALE_PRESERVE_ASPECTRATIO)
#define IMAGE_TEXTURE_PRESERVE_ASPECTRATIO_BOTTOM (IMAGE_TEXTURE_SCALE_PRESERVE_ASPECTRATIO | IMAGE_TEXTURE_POSITION_BOTTOM)
#define IMAGE_TEXTURE_PRESERVE_ASPECTRATIO_CENTER (IMAGE_TEXTURE_SCALE_PRESERVE_ASPECTRATIO | IMAGE_TEXTURE_POSITION_HORIZONTAL_CENTER | IMAGE_TEXTURE_POSITION_VERTICAL_CENTER)


// Font initialization style flags
#define D3DFONT_BOLD        0x0001
#define D3DFONT_ITALIC      0x0002
#define D3DFONT_ZENABLE     0x0004

// Text drawing options flags
#define D3DFONT_CENTERED_X  0x0001
#define D3DFONT_CENTERED_Y  0x0002
#define D3DFONT_TWOSIDED    0x0004
#define D3DFONT_FILTERED    0x0008
#define D3DFONT_BORDER		0x0010
#define D3DFONT_COLORTABLE	0x0020
#define D3DFONT_CLEARTARGET 0x0080	// Clear the target area where font will be drawn (ie. font background is not transparent)


class CNGPCarMenuAPI
{
protected:
	char szModulePath[_MAX_PATH];
	HMODULE hDLLModule;

	tAPI_InitializePluginIntegration fp_API_InitializePluginIntegration;
	tAPI_LoadCustomImage fp_API_LoadCustomImage;
	tAPI_ShowHideImage fp_API_ShowHideImage;
	tAPI_RemovePluginIntegration fp_API_RemovePluginIntegration;
	tAPI_MapRBRPointToScreenPoint fp_API_MapRBRPointToScreenPoint;

	tAPI_GetVersionInfo fp_API_GetVersionInfo;

	tAPI_InitializeFont fp_API_InitializeFont;
	tAPI_DrawTextA fp_API_DrawTextA;
	tAPI_DrawTextW fp_API_DrawTextW;

	tAPI_PrepareBTBTrackLoad     fp_API_PrepareBTBTrackLoad;
	tAPI_CheckBTBTrackLoadStatus fp_API_CheckBTBTrackLoadStatus;

	void Cleanup()
	{
		fp_API_InitializePluginIntegration = nullptr;
		fp_API_LoadCustomImage = nullptr;
		fp_API_ShowHideImage = nullptr;
		fp_API_RemovePluginIntegration = nullptr;
		fp_API_MapRBRPointToScreenPoint = nullptr;
		fp_API_GetVersionInfo = nullptr;
		fp_API_InitializeFont = nullptr;
		fp_API_DrawTextA = nullptr;
		fp_API_DrawTextW = nullptr;
		fp_API_PrepareBTBTrackLoad = nullptr;
		fp_API_CheckBTBTrackLoadStatus = nullptr;

		if (hDLLModule) ::FreeLibrary(hDLLModule);
		hDLLModule = nullptr;
	}

public:
	CNGPCarMenuAPI()
	{
		hDLLModule = nullptr;
		szModulePath[0] = '\0';
		Cleanup();
	}

	~CNGPCarMenuAPI()
	{
		Cleanup();
	}

	// Return the root folder path of RBR executable (fex c:\games\richardBurnsRally)
	const char* GetModulePath() { return szModulePath; }

	// Initialize custom image support for szPluginName RBR plugin. Optionally provide path to NGPCarMenu.dll library, but if this param is NULL then use the default Plugins\NGPCarMenu.dll library.
	// Return value: <= 0 Error, >0 = pluginID handle used in other API calls
	DWORD InitializePluginIntegration(LPCSTR szPluginName, LPCSTR szPathToNGPCarMenuDLL = nullptr)
	{
		if (hDLLModule == nullptr)
		{
			char szDLLFilePath[_MAX_PATH];

			// Use default path to NGPCarMenu.dll file (ie. rbr\Plugins\NGPCarMenu.dll)
			::GetModuleFileNameA(NULL, szModulePath, sizeof(szModulePath));
			::PathRemoveFileSpecA(szModulePath);

			if (szPathToNGPCarMenuDLL == nullptr)
			{
				szDLLFilePath[0] = '\0';
				strncat_s(szDLLFilePath, _MAX_PATH, szModulePath, sizeof(szModulePath));
				strncat_s(szDLLFilePath, _MAX_PATH, C_NGPCARMENU_DLL_FILENAME, sizeof(C_NGPCARMENU_DLL_FILENAME));
				szPathToNGPCarMenuDLL = szDLLFilePath;
			}

			hDLLModule = ::LoadLibraryA(szPathToNGPCarMenuDLL);
			if (hDLLModule)
			{
				// Map dynamically API functions. If the DLL library or method is missing then this plugin still works (doesn't fail in "cannot load DLL library" error)
				fp_API_InitializePluginIntegration = (tAPI_InitializePluginIntegration)GetProcAddress(hDLLModule, "API_InitializePluginIntegration");
				fp_API_LoadCustomImage = (tAPI_LoadCustomImage)GetProcAddress(hDLLModule, "API_LoadCustomImage");
				fp_API_ShowHideImage = (tAPI_ShowHideImage)GetProcAddress(hDLLModule, "API_ShowHideImage");
				fp_API_RemovePluginIntegration = (tAPI_RemovePluginIntegration)GetProcAddress(hDLLModule, "API_RemovePluginIntegration");
				fp_API_MapRBRPointToScreenPoint = (tAPI_MapRBRPointToScreenPoint)GetProcAddress(hDLLModule, "API_MapRBRPointToScreenPoint");
				fp_API_GetVersionInfo = (tAPI_GetVersionInfo)GetProcAddress(hDLLModule, "API_GetVersionInfo");
				fp_API_InitializeFont = (tAPI_InitializeFont)GetProcAddress(hDLLModule, "API_InitializeFont");
				fp_API_DrawTextA = (tAPI_DrawTextA)GetProcAddress(hDLLModule, "API_DrawTextA");
				fp_API_DrawTextW = (tAPI_DrawTextW)GetProcAddress(hDLLModule, "API_DrawTextW");
				fp_API_PrepareBTBTrackLoad = (tAPI_PrepareBTBTrackLoad)GetProcAddress(hDLLModule, "API_PrepareBTBTrackLoad");
				fp_API_CheckBTBTrackLoadStatus = (tAPI_CheckBTBTrackLoadStatus)GetProcAddress(hDLLModule, "API_CheckBTBTrackLoadStatus");
			}
		}

		if (fp_API_InitializePluginIntegration != nullptr && szPluginName != nullptr)
			return fp_API_InitializePluginIntegration(szPluginName);
		else
			return 0;
	}

	// Load an image from szFileName and convert it as DirectX texture object and draw at speficied location and usign the specified size
	void LoadCustomImage(DWORD pluginID, int imageID, LPCSTR szFileName, const POINT* pImagePos, const SIZE* pImageSize, DWORD dwImageFlags)
	{
		if (fp_API_LoadCustomImage != nullptr)
			fp_API_LoadCustomImage(pluginID, imageID, szFileName, pImagePos, pImageSize, dwImageFlags);
	}

	void LoadCustomImage(DWORD pluginID, int imageID, LPCSTR szFileName, long x, long y, long cx, long cy, DWORD dwImageFlags)
	{
		POINT imagePos = { x, y };
		SIZE  imageSize = { cx, cy };
		LoadCustomImage(pluginID, imageID, szFileName, &imagePos, &imageSize, dwImageFlags);
	}

	void LoadCustomImageF(DWORD pluginID, int imageID, LPCSTR szFileName, float x, float y, float cx, float cy, DWORD dwImageFlags)
	{
		POINT imagePos = { static_cast<long>(x), static_cast<long>(y) };
		SIZE  imageSize = { static_cast<long>(cx), static_cast<long>(cy) };
		LoadCustomImage(pluginID, imageID, szFileName, &imagePos, &imageSize, dwImageFlags);
	}

	// Hide or Show specified image (imageID) without unloading the DirectX texture object (ie. faster to re-draw when the texture obj is still valid)
	void ShowHideImage(DWORD pluginID, int imageID, bool showImage)
	{
		if (fp_API_ShowHideImage != nullptr)
			fp_API_ShowHideImage(pluginID, imageID, showImage);
	}

	// Remove support for custom images (usually no need to call because NGPCarMenu plugin takes care of it when RBR app is closed)
	void RemovePluginIntegration(DWORD pluginID)
	{
		if (fp_API_RemovePluginIntegration != nullptr)
			fp_API_RemovePluginIntegration(pluginID);
	}

	// Map RBR game coordinate point to screen point
	void MapRBRPointToScreenPoint(const float srcX, const float srcY, float* trgX, float* trgY)
	{
		if (fp_API_MapRBRPointToScreenPoint != nullptr)
			fp_API_MapRBRPointToScreenPoint(srcX, srcY, trgX, trgY);
	}

	void MapRBRPointToScreenPoint(const int srcX, const int srcY, int* trgX, int* trgY)
	{
		float fTrgX = 0, fTrgY = 0;
		this->MapRBRPointToScreenPoint(static_cast<float>(srcX), static_cast<float>(srcY), (trgX != nullptr ? &fTrgX : nullptr), (trgY != nullptr ? &fTrgY : nullptr));
		if (trgX != nullptr) *trgX = static_cast<int>(fTrgX); 
		if (trgY != nullptr) *trgY = static_cast<int>(fTrgY);
	}

	void MapRBRPointToScreenPoint(const int srcX, const int srcY, long* trgX, long* trgY)
	{
		float fTrgX = 0, fTrgY = 0;
		this->MapRBRPointToScreenPoint(static_cast<float>(srcX), static_cast<float>(srcY), (trgX != nullptr ? &fTrgX : nullptr), (trgY != nullptr ? &fTrgY : nullptr));
		if (trgX != nullptr) *trgX = static_cast<long>(fTrgX);
		if (trgY != nullptr) *trgY = static_cast<long>(fTrgY);
	}

	// Version tag of NGPCarMenu plugin. Returns numerical and/or text string version tags (parameters can be optionally nullptr to ignore the version info). 
	// If pOutTextBuffer is set then the buffer must have at least the size of 32 bytes.
	// For example: GetVersionInfo(nullptr, 0, &verMajor, &verMinor, &verPatch, &verBuild)
	// For example: GetVersionInfo(myBuffer, sizeof(myBuffer), &verMajor, &verMinor, nullptr, nullptr)
	BOOL GetVersionInfo(char* pOutTextBuffer, size_t textBufferSize, int* pOutMajor, int* pOutMinor, int* pOutPatch, int* pOutBuild, LPCSTR szPathToNGPCarMenuDLL = nullptr)
	{
		// Initialize just API function pointers, but don't link this custom plugin with NGPCarMenu plugin just because of version check
		InitializePluginIntegration(nullptr, szPathToNGPCarMenuDLL);

		if (fp_API_GetVersionInfo != nullptr)
			return fp_API_GetVersionInfo(pOutTextBuffer, textBufferSize, pOutMajor, pOutMinor, pOutPatch, pOutBuild);
		else
			return FALSE;
	}

	// Initialize a custom font (font typeface name, size and style). Style can be 0 (default style) or combination of D3DFONT_BOLD | D3DFONT_ITALIC | D3DFONT_ZENABLE.
	// The return value is fontID and it is used in DrawTextA/DrawTextW methods to specify a specific font for a text.
	// For example: InitializeFont("Trebuchet MS", 16, D3DFONT_ITALIC | D3DFONT_BOLD)
	DWORD InitializeFont(const char* fontName, DWORD fontSize, DWORD fontStyle)
	{
		if (fp_API_InitializeFont != nullptr)
			return fp_API_InitializeFont(fontName, fontSize, fontStyle);
		else
			return 0;
	}

	// Draw text (char) at specified x,y screen location using fontID font style, color and draw options.
	// DrawOptions can be combination of D3DFONT_CENTERED_X | D3DFONT_CENTERED_Y | D3DFONT_TWOSIDED | D3DFONT_FILTERED | D3DFONT_BORDER | D3DFONT_COLORTABLE | D3DFONT_CLEARTARGET
	BOOL DrawTextA(DWORD pluginID, int textID, int posX, int posY, const char* szText, DWORD fontID, DWORD color, DWORD drawOptions)
	{
		if (fp_API_DrawTextA != nullptr)
			return fp_API_DrawTextA(pluginID, textID, posX, posY, szText, fontID, color, drawOptions);
		else
			return FALSE;
	}

	// Draw text (wchar). See DrawTextA comment
	BOOL DrawTextW(DWORD pluginID, int textID, int posX, int posY, const wchar_t* wszText, DWORD fontID, DWORD color, DWORD drawOptions)
	{
		if (fp_API_DrawTextW != nullptr)
			return fp_API_DrawTextW(pluginID, textID, posX, posY, wszText, fontID, color, drawOptions);
		else
			return FALSE;
	}

	// Prepare RBR track #41 (CortezArbroz) for BTB track loading. szBTBTrackName param should be the name of the BTB track (ie. the value of rx_content\tracks\someBTBTrack\track.ini NAME option and as shown in RBRRX stages menu list)
	BOOL PrepareBTBTrackLoad(DWORD pluginID, LPCSTR szBTBTrackName, LPCSTR szBTBTrackFolderName)
	{
		if (fp_API_PrepareBTBTrackLoad != nullptr && fp_API_CheckBTBTrackLoadStatus != nullptr)
			return fp_API_PrepareBTBTrackLoad(pluginID, szBTBTrackName, szBTBTrackFolderName);
		else
			return FALSE;
	}

	// Check BTB track loading status (make sure the track is not the #41 original track). TRUE=BTB was loaded successfully, FALSE=BTB track loading failed and the map is the original #41 Cote
	// This method can be called only when the camera is spinning around the car in a starting line and the countdown timer is about to start.
	BOOL CheckBTBTrackLoadStatus(DWORD pluginID, LPCSTR szBTBTrackName, LPCSTR szBTBTrackFolderName)
	{
		if (fp_API_PrepareBTBTrackLoad != nullptr && fp_API_CheckBTBTrackLoadStatus != nullptr)
			return fp_API_CheckBTBTrackLoadStatus(pluginID, szBTBTrackName, szBTBTrackFolderName);
		else
			return FALSE;
	}
};

#endif
