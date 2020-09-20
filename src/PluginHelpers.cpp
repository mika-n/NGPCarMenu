/*
** -------------------------------------------------------------------------------------------------
** Rally 7
** -------------------------------------------------------------------------------------------------
**
** $Workfile: PluginHelpers.cpp $
** $Archive: /Rally_7/SourceMaterial/CodeBase/Game/Pc/Plugin/RBRTestPlugin/PluginHelpers.cpp $
**
** $Author: Tjohansson $
** $Date: 04-10-14 12:53 $
** $Revision: 2 $
**
*/

/* includes --------------------------------------------------------------------------------------*/

#include "PluginHelpers.h"
#include <stdio.h>

/* macros ----------------------------------------------------------------------------------------*/

/* types -----------------------------------------------------------------------------------------*/

/* externs ---------------------------------------------------------------------------------------*/

/* prototypes ------------------------------------------------------------------------------------*/

/* data ------------------------------------------------------------------------------------------*/

/* code ------------------------------------------------------------------------------------------*/

namespace NPlugin
{

/*
====================================================================================================
====================================================================================================
====================================================================================================
--
-- SECTION: Menu creation functions
--
====================================================================================================
====================================================================================================
====================================================================================================
*/

//------------------------------------------------------------------------------------------------//
// Return name of a stage
const char*		GetStageName	( int iMap )
{
	switch( iMap )
	{
		// Artic tracks
		case 10: return "Kaihuavaara";
		case 11: return "Mustaselka";
		case 12: return "Sikakama";
		case 13: return "Autiovaara";
		case 14: return "Kaihuavaara II";
		case 15: return "Mustaselka II";

		// British tracks
		case 20: return "Harwood Forest";
		case 21: return "Falstone";
		case 22: return "Chirdonhead";
		case 23: return "Shepherds Shield";
		case 24: return "Harwood Forest II";
		case 25: return "Chirdonhead II";

		// Australia
		case 31: return "NewBobs";
		case 32: return "Greenhills";
		case 33: return "Mineshaft";
		case 34: return "East-West";
		case 35: return "NewBobs II";
		case 36: return "East-West II";

		// France
		case 41: return "Cote D'Arbroz";
		case 42: return "Joux Verte";
		case 43: return "Bisanne";
		case 44: return "Joux Plane";
		case 45: return "Joux Verte II";
		case 46: return "Cote D'Arbroz II";

		// Hokkaido
		case 51: return "Noiker";
		case 52: return "Sipirkakim";
		case 53: return "Pirka Menoko";
		case 54: return "Tanner";
		case 55: return "Noiker II";
		case 56: return "Tanner II";

		// USA
		case 61: return "Fraizer Wells";
		case 62: return "Prospect Ridge";
		case 63: return "Diamond Creek";
		case 64: return "Hualapai Nation";
		case 65: return "Prospect Ridge II";
		case 66: return "Diamond Creek II";

		case 71: return "Rally School Stage";

		default: return NULL;
	};
}

//------------------------------------------------------------------------------------------------//
// Return name of car (original cars. NGPCarMenu doesn't use this method because the plugin supports customized NGP car names)
const char*		GetCarName		( int iCar )
{
	switch( iCar )
	{
		case 0: return "Citroen Xsara";
		case 1: return "Hyundai Accent";
		case 2: return "MG ZR Super 1600";
		case 3: return "Mitsubishi Lancer Evo VII";
		case 4: return "Peugeot 206";
		case 5: return "Subaru Impreza 2003";
		case 6: return "Toyota Corolla";
		case 7: return "Subaru Impreza 2000";
		default: return NULL;
	}
}

//------------------------------------------------------------------------------------------------//
// Return surface name (NGPCarMenu addition in this namespace)
//
const wchar_t* GetSurfaceName(int iSurface)
{
	switch (iSurface)
	{
	case 0: return L"tarmac";
	case 1: return L"gravel";
	case 2: return L"snow";
	default: return L"";
	}
}

/*
const char* GetSurfaceNameA(int iSurface)
{
	switch (iSurface)
	{
	case 0: return "tarmac";
	case 1: return "gravel";
	case 2: return "snow";
	default: return "";
	}
}
*/

//------------------------------------------------------------------------------------------------//
// Return surface type of the original tracks (NGPCarMenu addition in this namespace)
//
const int GetStageSurface(int iMap)
{
	// Return the surface type of original tracks and certain RBRTM tracks which doesn't have Surface attribute in Maps\Tracks.ini file
	switch (iMap)
	{
		// Tarmac tracks
		case 41:
		case 42:
		case 43:
		case 44:
		case 45:
		case 46:
		case 47:
		case 48:
			return 0;

		// Gravel tracks
		case 4:
		case 20:
		case 21:
		case 22:
		case 23:
		case 24:
		case 25:
		case 26:
		case 27:
		case 30:
		case 31:
		case 32:
		case 33:
		case 34:
		case 35:
		case 36:
		case 37:
		case 50:
		case 51:
		case 52:
		case 53:
		case 54:
		case 55:
		case 56:
		case 57:
		case 60:
		case 61:
		case 62:
		case 63:
		case 64:
		case 65:
		case 66:
		case 67:
		case 68:
		case 69:
		case 71:
		case 72:
		case 74:
		case 75:
		case 76:
		case 77:
		case 78:
		case 79:
		case 80:
		case 81:
		case 82:
		case 83:
		case 84:
		case 85:
		case 86:
		case 87:
		case 88:
		case 89:
		case 90:
			return 1;

		// Snow tracks
		case 10:
		case 11:
		case 12:
		case 13:
		case 14:
		case 15: 
		case 16:
		case 17:
			return 2;

		default: return -1; // Unknown surface. Use the surface value defined in Maps\tracks.ini file (if it is not set there then surface label is not shown in stages menu list)
	};
}

//------------------------------------------------------------------------------------------------//
/**
 * Format a string to reflect a time
 * @param <ptxtBuffer>		The buffer into which to format the string. Needs to be at least 9 bytes long - preferably more
 * @param <iTxtBufferSize>	The size of text buffer (max num of chars copied to buffer)
 * @param <fTime>			The time, in seconds
 * @return					The buffer string, for use in a function
 */
char* FormatTimeString( char* ptxtBuffer, size_t iTxtBufferSize, const float fTime, bool bAlwaysSign /* = FALSE */ )
{
	float fAbsTime = fTime < 0.0f ? -fTime : fTime;

	unsigned int iHundreds = ( unsigned int )( fAbsTime * 100.0f ) % 100;
	unsigned int iSeconds = ( unsigned int )( fAbsTime ) % 60;
	unsigned int iMinutes = ( unsigned int )( fAbsTime / 60.0f ); // % 60; 
	
	const char* ptxtPlus = bAlwaysSign ? "+" : "";

	sprintf_s( ptxtBuffer, iTxtBufferSize, "%s%02d:%02d:%02d",
		fTime < 0.0f ? "-" : ptxtPlus,
		iMinutes, iSeconds, iHundreds );

	return ptxtBuffer;
}

}; // namespace NPlugin

/*
 * $History: PluginHelpers.cpp $
 * 
 * *****************  Version 2  *****************
 * User: Tjohansson   Date: 04-10-14   Time: 12:53
 * Updated in $/Rally_7/SourceMaterial/CodeBase/Game/Pc/Plugin/RBRTestPlugin
 * 
 * *****************  Version 1  *****************
 * User: Tjohansson   Date: 04-09-03   Time: 14:29
 * Created in $/Rally_7/SourceMaterial/CodeBase/Game/Pc/Plugin
 * 
 * *****************  Version 3  *****************
 * User: Tjohansson   Date: 04-08-27   Time: 14:04
 * Updated in $/Rally_7/SourceMaterial/CodeBase/Game/Pc/Plugin
 * 
 * *****************  Version 2  *****************
 * User: Pdervall     Date: 8/27/04    Time: 8:54a
 * Updated in $/Rally_7/SourceMaterial/CodeBase/Game/Pc/Plugin
 * 
 * *****************  Version 1  *****************
 * User: Pdervall     Date: 8/23/04    Time: 1:58p
 * Created in $/Rally_7/SourceMaterial/CodeBase/Game/Pc/Plugin
 */