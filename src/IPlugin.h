/*
** -------------------------------------------------------------------------------------------------
** Rally 7
** -------------------------------------------------------------------------------------------------
**
** $Workfile: IPlugin.h $
** $Archive: /Rally_7/SourceMaterial/CodeBase/Game/Pc/Plugin/IPlugin.h $
**
** $Author: Tjohansson $
** $Date: 04-09-14 10:58 $
** $Revision: 4 $
**
*/
#if __SUPPORT_PLUGIN__

#ifndef __IPLUGIN_H_INCLUDED
#define __IPLUGIN_H_INCLUDED

/* includes --------------------------------------------------------------------------------------*/

/* macros ----------------------------------------------------------------------------------------*/

/* types -----------------------------------------------------------------------------------------*/

class IPlugin
{
public:
	virtual					~IPlugin			( void ) {}

	/// Get the name of the plugin, for display purposes.
	virtual const char*		GetName				( void ) = 0;

	/// Draw the plugins own frontend page
	virtual void			DrawFrontEndPage	( void ) = 0;
	// Draw the result list at end of race
	virtual void			DrawResultsUI		( void ) = 0;

	/// Event handler for the frontend menu input
	virtual void			HandleFrontEndEvents(
		char	txtKeyboard,	// Keyboard input, single character
		bool	bUp,			// User pushed up
		bool	bDown,			// User pushed down
		bool	bLeft,			// User pushed left
		bool	bRight,			// User pushed right
		bool	bSelect			// User pushed the select button
	) = 0;

	/// Progress time spent in frontend page, called every frame.
	virtual void			TickFrontEndPage	( float fTimeDelta ) = 0;

	/// Is called when the player timer starts (after GO! or in case of a false start)
	virtual void			StageStarted		( int iMap, 
												  const char* ptxtPlayerName,
												  bool bWasFalseStart ) = 0;

	/// Is called when player finishes stage ( fFinishTime is 0.0f if player failed the stage )
	virtual void			HandleResults		( float fCheckPoint1, 
												  float fCheckPoint2,
												  float fFinishTime,
												  const char* ptxtPlayerName ) = 0;

	/// Is called when a player passed a checkpoint 
	/**
	  * <fCheckPointTime> Time passed in seconds since stage start
	  * <iCheckPointID> Which checkpoint passed ( 1 or 2 )
	  * <ptxtPlayerName> Name of player
	  *
	  */
	virtual void			CheckPoint			( float fCheckPointTime,
												  int   iCheckPointID,
												  const char* ptxtPlayerName ) = 0;
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
 * $History: IPlugin.h $
 * 
 * *****************  Version 4  *****************
 * User: Tjohansson   Date: 04-09-14   Time: 10:58
 * Updated in $/Rally_7/SourceMaterial/CodeBase/Game/Pc/Plugin
 * 
 * *****************  Version 3  *****************
 * User: Tjohansson   Date: 04-08-27   Time: 14:04
 * Updated in $/Rally_7/SourceMaterial/CodeBase/Game/Pc/Plugin
 * 
 * *****************  Version 2  *****************
 * User: Tjohansson   Date: 04-08-24   Time: 10:25
 * Updated in $/Rally_7/SourceMaterial/CodeBase/Game/Pc/Plugin
 * 
 * *****************  Version 1  *****************
 * User: Pdervall     Date: 8/23/04    Time: 1:30p
 * Created in $/Rally_7/SourceMaterial/CodeBase/Game/Pc/Plugin
 */