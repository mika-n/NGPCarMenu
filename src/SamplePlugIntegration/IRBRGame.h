/*
** -------------------------------------------------------------------------------------------------
** Rally 7
** -------------------------------------------------------------------------------------------------
**
** $Workfile: IRBRGame.h $
** $Archive: /Rally_7/SourceMaterial/CodeBase/Game/Pc/Plugin/IRBRGame.h $
**
** $Author: Tjohansson $
** $Date: 04-10-13 11:54 $
** $Revision: 7 $
**
*/
#if __SUPPORT_PLUGIN__

#ifndef __IRBRGAME_H_INCLUDED
#define __IRBRGAME_H_INCLUDED

/* includes --------------------------------------------------------------------------------------*/

/* macros ----------------------------------------------------------------------------------------*/

/* types -----------------------------------------------------------------------------------------*/

class IRBRGame
{
public:
	enum ERBRTyreTypes
	{
		TYRE_TARMAC_DRY, 
		TYRE_TARMAC_INTERMEDIATE,
		TYRE_TARMAC_WET, 
		TYRE_GRAVEL_DRY,		
		TYRE_GRAVEL_INTERMEDIATE,
		TYRE_GRAVEL_WET,
		TYRE_SNOW,
		NUM_TYRE_TYPES,
		TYRE_TYPE_FORCE_DWORD = 0x7fffffff,
	};

	enum ERBRWeatherType
	{
		GOOD_WEATHER,
		RANDOM_WEATHER,
		BAD_WEATHER,
	};

	/////////////////////////////////////////////////////////////////////////
	/// Game flow control gizmos
	/////////////////////////////////////////////////////////////////////////	

	/** 
	  * @param <iMap>		The map to load, see maplist for map id's
	  * @param <iCar>		Which car to use
	  * @param <eWeather>   Weather type
	  * @param <eTyre>		Tyre type to use
	  * @param <ptxtCarSetupFileName> The carsetup file to use (use filepath from gameroot, i.e SavedGames\\5slot0_gravelsetup.lsp ), use null for default setup
	  *
	  * @return true if game was started
	  *
	  **/
	virtual	bool	StartGame	( int iMap, int iCar, const ERBRWeatherType eWeather, const ERBRTyreTypes eTyre, const char* ptxtCarSetupFileName ) = 0;

	// Draw an ingame message
	/** 
	  * @param <ptxtMessage> Message to display
	  * @param <fTimeToDisplay> Time to display the message (0.0f displays it until the next call)
	  * @param <x, y> Display position
	  *
	  **/
	virtual void	WriteGameMessage ( const char* ptxtMessage, float fTimeToDisplay, float x, float y ) = 0;

	/////////////////////////////////////////////////////////////////////////
	/// Menu Drawing functions 
	/////////////////////////////////////////////////////////////////////////

	virtual void	WriteText	( float x, float y, const char* ptxtText )	= 0;
	// Draw a 2D Gfx box, see Genbox.h for box defines
	virtual void	DrawBox		( unsigned int iBox, float x, float y )		= 0;
	// Draw a flatcolored box
	virtual void	DrawFlatBox	( float x, float y, float w, float h )		= 0;
	// Draw a blackout (the thing behind the menus), coordinates specify the "window"
	virtual void	DrawBlackOut( float x, float y, float w, float h )		= 0;
	// Draw the red selection bar
	virtual void	DrawSelection ( float x, float y, float w, float h = 21.0f )	= 0;

	virtual void	DrawCarIcon	  ( float x, float y, int iCar ) = 0;

	#define GFX_DRAW_CENTER_X 		0x00000001
	#define GFX_DRAW_CENTER_Y		0x00000002
	#define GFX_DRAW_ALIGN_RIGHT	0x00000004
	#define GFX_DRAW_ALIGN_BOTTOM	0x00000008
	#define GFX_DRAW_FLIP_X			0x00000010
	#define GFX_DRAW_FLIP_Y			0x00000020
	#define GFX_DRAW_TEXT_SHADOW	0x00000040

	// Set draw mode for centering, alignment etc... Don't forget to reset it afterwards
	virtual void	SetDrawMode  ( unsigned int bfDrawMode ) = 0;
	virtual void	ReSetDrawMode( unsigned int bfDrawMode ) = 0;

	enum	EFonts
	{
		FONT_SMALL,
		FONT_BIG,
		FONT_DEBUG,
		FONT_HEADING, // Only uppercase
	};

	// Set font for WriteText
	virtual void	SetFont		( EFonts eFont ) = 0;

	enum EMenuColors
	{
		MENU_BKGROUND,  // Background color
		MENU_SELECTION, // Selection color
		MENU_ICON,      // icon color
		MENU_TEXT,      // text color
		MENU_HEADING,	// heading color
	};

	// Set predefined color 
	virtual void	SetMenuColor ( EMenuColors eColor ) = 0;
	// Set arbitrary color
	virtual void	SetColor	( float r, float g, float b, float a )		= 0;

	/////////////////////////////////////////////////////////////////////////
	/// Misc functions
	/////////////////////////////////////////////////////////////////////////

	enum EGameLanguage
	{
		L_ENGLISH,
		L_FRENCH,		
		L_GERMAN,
		L_SPANISH,
		L_ITALIAN,
		L_CZECH,
		L_POLISH,
		L_NUM_LANGUAGES
	};

	// Return the language the game uses
	virtual const EGameLanguage	GetLanguage			( void ) = 0;
};

/* externs ---------------------------------------------------------------------------------------*/

/* prototypes ------------------------------------------------------------------------------------*/

/* data ------------------------------------------------------------------------------------------*/

/* code ------------------------------------------------------------------------------------------*/

/*
====================================================================================================
====================================================================================================
====================================================================================================
--
-- SECTION: 
--
====================================================================================================
====================================================================================================
====================================================================================================
*/

//------------------------------------------------------------------------------------------------//

#endif

#endif // #if __SUPPORT_PLUGIN__
/*
 * $History: IRBRGame.h $
 * 
 * *****************  Version 7  *****************
 * User: Tjohansson   Date: 04-10-13   Time: 11:54
 * Updated in $/Rally_7/SourceMaterial/CodeBase/Game/Pc/Plugin
 * 
 * *****************  Version 6  *****************
 * User: Tjohansson   Date: 04-09-14   Time: 10:58
 * Updated in $/Rally_7/SourceMaterial/CodeBase/Game/Pc/Plugin
 * 
 * *****************  Version 5  *****************
 * User: Tjohansson   Date: 04-09-08   Time: 10:15
 * Updated in $/Rally_7/SourceMaterial/CodeBase/Game/Pc/Plugin
 * 
 * *****************  Version 4  *****************
 * User: Tjohansson   Date: 04-08-27   Time: 14:04
 * Updated in $/Rally_7/SourceMaterial/CodeBase/Game/Pc/Plugin
 * 
 * *****************  Version 3  *****************
 * User: Tjohansson   Date: 04-08-23   Time: 16:12
 * Updated in $/Rally_7/SourceMaterial/CodeBase/Game/Pc/Plugin
 * 
 * *****************  Version 2  *****************
 * User: Tjohansson   Date: 04-08-23   Time: 15:02
 * Updated in $/Rally_7/SourceMaterial/CodeBase/Game/Pc/Plugin
 * 
 * *****************  Version 1  *****************
 * User: Pdervall     Date: 8/23/04    Time: 1:30p
 * Created in $/Rally_7/SourceMaterial/CodeBase/Game/Pc/Plugin
 */