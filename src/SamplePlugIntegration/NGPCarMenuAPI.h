#ifndef __NGPCARMENUAPI_H_INCLUDED
#define __NGPCARMENUAPI_H_INCLUDED

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

#define IMAGE_TEXTURE_STRETCH_TO_FILL			 0x00  // Default behaviour is to stretch the image to fill the specified draw area
#define IMAGE_TEXTURE_PRESERVE_ASPECTRATIO_TOP	  (IMAGE_TEXTURE_SCALE_PRESERVE_ASPECTRATIO)
#define IMAGE_TEXTURE_PRESERVE_ASPECTRATIO_BOTTOM (IMAGE_TEXTURE_SCALE_PRESERVE_ASPECTRATIO | IMAGE_TEXTURE_POSITION_BOTTOM)

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
