/*
** -------------------------------------------------------------------------------------------------
** Rally 7
** -------------------------------------------------------------------------------------------------
**
** $Workfile: PluginHelpers.h $
** $Archive: /Rally_7/SourceMaterial/CodeBase/Game/Pc/Plugin/RBRTestPlugin/PluginHelpers.h $
**
** $Author: Tjohansson $
** $Date: 04-10-13 11:55 $
** $Revision: 2 $
**
*/

#ifndef __PLUGINHELPERS_H_INCLUDED
#define __PLUGINHELPERS_H_INCLUDED

/* includes --------------------------------------------------------------------------------------*/

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
-- SECTION: Helper functions for RBR plugins
--
====================================================================================================
====================================================================================================
====================================================================================================
*/

//------------------------------------------------------------------------------------------------//
// Return name of a stage (returns NULL for invalid map id's)
const char*		GetStageName	( int iMap );

//------------------------------------------------------------------------------------------------//
// Return name of car (returns NULL for invalid car id's)
const char*		GetCarName		( int iCar );

//------------------------------------------------------------------------------------------------//
/**
 * Format a string to reflect a time in a 00:00:00 format
 * @param <ptxtBuffer>	The buffer into which to format the string. Needs to be at least 9 bytes long - preferably more
 * @param <fTime>		The time, in seconds
 * @return				The buffer string, for use in a function
 */
char* FormatTimeString( char* ptxtBuffer, const float fTime, bool bAlwaysSign = false );

//------------------------------------------------------------------------------------------------//
}; // namespace NPlugin

#endif // __PLUGINHELPERS_H_INCLUDED

/*
 * $History: PluginHelpers.h $
 * 
 * *****************  Version 2  *****************
 * User: Tjohansson   Date: 04-10-13   Time: 11:55
 * Updated in $/Rally_7/SourceMaterial/CodeBase/Game/Pc/Plugin/RBRTestPlugin
 * 
 * *****************  Version 1  *****************
 * User: Tjohansson   Date: 04-09-03   Time: 14:29
 * Created in $/Rally_7/SourceMaterial/CodeBase/Game/Pc/Plugin
 * 
 * *****************  Version 4  *****************
 * User: Tjohansson   Date: 04-08-27   Time: 14:04
 * Updated in $/Rally_7/SourceMaterial/CodeBase/Game/Pc/Plugin
 * 
 * *****************  Version 3  *****************
 * User: Pdervall     Date: 8/27/04    Time: 8:54a
 * Updated in $/Rally_7/SourceMaterial/CodeBase/Game/Pc/Plugin
 * 
 * *****************  Version 2  *****************
 * User: Pdervall     Date: 8/23/04    Time: 1:30p
 * Updated in $/Rally_7/SourceMaterial/CodeBase/Game/Pc/Plugin
 */