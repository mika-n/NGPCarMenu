#ifndef __NGPCARMENUAPI_H_INCLUDED
#define __NGPCARMENUAPI_H_INCLUDED

//
// NGPCarMenuAPI. API interface to draw custom images from another plugins without worrying about DirectX and RBR texture/bitmap handling requirements. 
// How to use this API?
//  - Include this NGPCarMenuAPI.h file in your own plugin code
//  - Instantiate CNGPCarMenuAPI class object within your plugin code
//  - Register a link between your plugin and NGPCarMenu backend plugin by calling InitializePluginIntegration method (ie. you need to have NGPCarMenu.dll installed in RBR\Plugins\ folder)
//  - Load images (PNG/BMP) and specify size and location by calling LoadCustomImage method (each image should have an unique imageID identifier. It is up to you to generate this ID number)
//  - Show or hide a specified image (imageID) based on events in your own plugin by calling ShowHideImage method
//  - See the sample plugin code for more details.
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

#define IMAGE_TEXTURE_STRETCH_TO_FILL			 0x00  // Default behaviour is to stretch the image to fill the specified draw area, no alpha blending
#define IMAGE_TEXTURE_PRESERVE_ASPECTRATIO_TOP	  (IMAGE_TEXTURE_SCALE_PRESERVE_ASPECTRATIO)
#define IMAGE_TEXTURE_PRESERVE_ASPECTRATIO_BOTTOM (IMAGE_TEXTURE_SCALE_PRESERVE_ASPECTRATIO | IMAGE_TEXTURE_POSITION_BOTTOM)
#define IMAGE_TEXTURE_PRESERVE_ASPECTRATIO_CENTER (IMAGE_TEXTURE_SCALE_PRESERVE_ASPECTRATIO | IMAGE_TEXTURE_POSITION_HORIZONTAL_CENTER | IMAGE_TEXTURE_POSITION_VERTICAL_CENTER)

class CNGPCarMenuAPI
{
protected:
	HMODULE hDLLModule;

	tAPI_InitializePluginIntegration fp_API_InitializePluginIntegration;
	tAPI_LoadCustomImage fp_API_LoadCustomImage;
	tAPI_ShowHideImage fp_API_ShowHideImage;
	tAPI_RemovePluginIntegration fp_API_RemovePluginIntegration;
	tAPI_MapRBRPointToScreenPoint fp_API_MapRBRPointToScreenPoint;

	void Cleanup()
	{
		fp_API_InitializePluginIntegration = nullptr;
		fp_API_LoadCustomImage = nullptr;
		fp_API_ShowHideImage = nullptr;
		fp_API_RemovePluginIntegration = nullptr;
		fp_API_MapRBRPointToScreenPoint = nullptr;

		if (hDLLModule) ::FreeLibrary(hDLLModule);
		hDLLModule = nullptr;
	}

public:
	CNGPCarMenuAPI()
	{
		hDLLModule = nullptr;
		Cleanup();
	}

	~CNGPCarMenuAPI()
	{
		Cleanup();
	}

	// Initialize custom image support for szPluginName RBR plugin. Optionally provide path to NGPCarMenu.dll library, but if this param is NULL then use the default Plugins\NGPCarMenu.dll library.
	// Return value: <= 0 Error, >0 = pluginID handle used in other API calls
	DWORD InitializePluginIntegration(LPCSTR szPluginName, LPCSTR szPathToNGPCarMenuDLL = nullptr)
	{
		if (hDLLModule == nullptr)
		{
			char szModulePath[_MAX_PATH];

			if (szPathToNGPCarMenuDLL == nullptr)
			{
				// Use default path to NGPCarMenu.dll file (ie. rbr\Plugins\NGPCarMenu.dll)
				::GetModuleFileNameA(NULL, szModulePath, sizeof(szModulePath));
				::PathRemoveFileSpecA(szModulePath);
				strncat_s(szModulePath, _MAX_PATH, C_NGPCARMENU_DLL_FILENAME, sizeof(C_NGPCARMENU_DLL_FILENAME));
				szPathToNGPCarMenuDLL = szModulePath;
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
			}
		}

		if (fp_API_InitializePluginIntegration != nullptr)
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
};

#endif
