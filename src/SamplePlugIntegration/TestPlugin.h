#ifndef __TESTPLUGIN_H_INCLUDED
#define __TESTPLUGIN_H_INCLUDED

// You need to define this before the includes, or use /D __SUPPORT_PLUGIN__=1 
// in the C/C++ / Commandline project configuration
#define __SUPPORT_PLUGIN__ 1

#include "IPlugin.h"
#include "IRBRGame.h"
#include "PluginHelpers.h"

//#include "Gendef.h" // 2D Gfx header
#include <stdio.h>

#include "NGPCarMenuAPI.h"

#define RAND_MAX_MIN(max, min) (rand() % (max - min + 1) + min)

#define C_CMD_STAGESTART	0
#define C_CMD_STAGEBTBSTART	1
#define C_CMD_MAP			2
#define C_CMD_DECORATION	3
#define C_CMD_TEXTNOTICE	4

#define NUM_SELECTIONS	(sizeof(g_szMenuSelections) / sizeof(g_szMenuSelections[0]))
const char* g_szMenuSelections[] =
{
	"Start Stage",
	"Start BTB Stage",
	"> Map",
	"> Click here for image decoration",
	"> Click here for custom text"
};

#define NUM_DECORATIONIMAGES (sizeof(g_szDecorationImages) / sizeof(g_szDecorationImages[0]))
const char* g_szDecorationImages[] =
{
	"C:\\Windows\\System32\\SecurityAndMaintenance.png",				// Just sample images, hopefully all PCs have these WinOS image files
	"C:\\Windows\\System32\\SecurityAndMaintenance_Alert.png",
	"C:\\Windows\\System32\\SecurityAndMaintenance_Error.png",
	"C:\\Apps\\rbr\\Plugins\\NGPCarMenu\\preview\\maps\\BTB_SS08 MÖKKIPERÄ 11km 1.0.3 (SSTF RXRBR.png"
};

#define C_SAMPLEIMAGE_ID  100	// Anything above zero value. Each cached image is uniquely identified by ID
#define C_DECORIMAGE_ID   101
#define C_MINIMAP_ID      102


#ifndef D3DCOLOR_DEFINED
typedef DWORD D3DCOLOR;
#define D3DCOLOR_DEFINED
#endif

#ifndef D3DCOLOR_ARGB
#define D3DCOLOR_ARGB(a,r,g,b) ((D3DCOLOR)((((a)&0xff)<<24)|(((r)&0xff)<<16)|(((g)&0xff)<<8)|((b)&0xff)))
#endif


//------------------------------------------------------------------------------------------------//

class CTestPlugin : public IPlugin
{
private:
	IRBRGame*       m_pGame;

	int				m_iSelection;
	int				m_iMap;

	float			m_fResults[3];
	char			m_szPlayerName[32];

	int				m_iDecorationImage;
	bool			m_bShowImage;
	bool			m_bShowText;

	int             m_iVersionMajor, m_iVersionMinor, m_iVersionBatch, m_iVersionBuild;
	char		    m_szVersionText[32];

	CNGPCarMenuAPI* m_pNGPCarMenuAPI;
	DWORD			m_dwPluginID;

	DWORD			m_dwFont1ID;
	DWORD			m_dwFont2ID;

public:
	CTestPlugin	( IRBRGame* pGame )	
		:	m_pGame	( pGame )
		,	m_iSelection( 0 )
		,	m_iMap	( 10 )
	{
		m_pNGPCarMenuAPI = nullptr;
		m_iDecorationImage = 0;
		m_dwPluginID = 0;
		m_bShowImage = true;
		m_bShowText = false;

		m_iVersionMajor = m_iVersionMinor = m_iVersionBatch = m_iVersionBuild = 0;
		m_szVersionText[0] = '\0';
	}

	virtual ~CTestPlugin( void )	
	{
		if (m_pNGPCarMenuAPI != nullptr)
			delete m_pNGPCarMenuAPI;
	}

	//------------------------------------------------------------------------------------------------//

	virtual const char* GetName( void )	
	{
		if (m_pNGPCarMenuAPI == nullptr)
		{
			// Initialize NGPCarMenu API functions to draw custom directX images in this sample plugin
			m_pNGPCarMenuAPI = new CNGPCarMenuAPI();

			// Get the version of NGPCarMenu plugin. Custom plugin can check that the plugin is at minimum version level (newer NGPCarMenu API is always backward compatible).
			// If the version is not the expected version then custom plugin can skip calling of InitializePluginIntegration and not to use NGPCarMenu API services.
			m_pNGPCarMenuAPI->GetVersionInfo(m_szVersionText, sizeof(m_szVersionText), &m_iVersionMajor, &m_iVersionMinor, &m_iVersionBatch, &m_iVersionBuild);

			m_dwPluginID = m_pNGPCarMenuAPI->InitializePluginIntegration("SamplePlug1");

			// Show a sample image at 450,200 SCREEN coordinate position. If position should follow the DrawFrontEndPage RBR coordinates then see MapRBRPointToScreenPoint API function
			m_pNGPCarMenuAPI->LoadCustomImage(m_dwPluginID, C_SAMPLEIMAGE_ID, g_szDecorationImages[0], 450, 200, 40, 40, IMAGE_TEXTURE_PRESERVE_ASPECTRATIO_TOP);
			m_pNGPCarMenuAPI->ShowHideImage(m_dwPluginID, C_SAMPLEIMAGE_ID, true);

			// Initialize two different kind of custom fonts (fontID used in DrawTextA/DrawTextW methods)
			m_dwFont1ID = m_pNGPCarMenuAPI->InitializeFont("Comic Sans MS", 20, D3DFONT_ITALIC | D3DFONT_BOLD);
			m_dwFont2ID = m_pNGPCarMenuAPI->InitializeFont("Verdana", 12, 0);
		}

		return "SamplePlug1";
	}

	//------------------------------------------------------------------------------------------------//
	virtual void DrawResultsUI( void )
	{
		m_pGame->SetMenuColor( IRBRGame::MENU_HEADING );	
		m_pGame->SetFont  ( IRBRGame::FONT_BIG );
		m_pGame->WriteText( 130.0f, 49.0f, "Results" );

		m_pGame->SetFont  ( IRBRGame::FONT_SMALL );
		m_pGame->SetMenuColor( IRBRGame::MENU_TEXT );
		if( m_fResults[ 2 ] <= 0.0f )
		{
			m_pGame->WriteText( 200.0f, 100.0f, "DNF" );
		}
		else
		{
			char txtCP1[ 32 ];			
			char txtTimeString[ 32 ];
			char txtBuffer[ 128 ];

			sprintf_s( txtBuffer, "Stage Result for \"%s\" ", m_szPlayerName );
			m_pGame->WriteText( 130.0f, 100.0f, txtBuffer );
			sprintf_s( txtBuffer, "CheckPoint1 = %s", NPlugin::FormatTimeString( txtCP1, m_fResults[ 0 ] ) );
			m_pGame->WriteText( 130.0f, 125.0f, txtBuffer );
			sprintf_s( txtBuffer, "CheckPoint2 = %s", NPlugin::FormatTimeString( txtCP1, m_fResults[ 1 ] ) );
			m_pGame->WriteText( 130.0f, 150.0f, txtBuffer );
			sprintf_s( txtBuffer, "Finish = %s", NPlugin::FormatTimeString( txtTimeString, m_fResults[ 2 ] ) );
			m_pGame->WriteText( 130.0f, 175.0f, txtBuffer );
		}
	}

	//------------------------------------------------------------------------------------------------//
	virtual void DrawFrontEndPage( void )
	{
		char szTextBuf[200];
		const char* pszMenuText;

		// Draw blackout (coordinates specify the 'window' where you don't want black)
		m_pGame->DrawBlackOut( 600.0f, 0.0f, 30.0f, 480.0f );

		// Draw heading
		m_pGame->SetMenuColor( IRBRGame::MENU_HEADING );	
		m_pGame->SetFont( IRBRGame::FONT_BIG );
		m_pGame->WriteText( 10.0f, 49.0f, "Sample using NGPCarMenuAPI to draw custom images");

		// -2.0f is a good y offset for selection
		m_pGame->DrawSelection( 0.0f, 68.0f + ( static_cast< float >( m_iSelection ) * 21.0f ), 280.0f );

		m_pGame->SetMenuColor( IRBRGame::MENU_TEXT );
		for( unsigned int i = 0; i < NUM_SELECTIONS; ++i )
		{
			if (i == C_CMD_MAP)
			{
				if (NPlugin::GetStageName(m_iMap))
					sprintf_s(szTextBuf, "%s: %s", g_szMenuSelections[i], NPlugin::GetStageName(m_iMap));
				else
					sprintf_s(szTextBuf, "%s: %d", g_szMenuSelections[i], m_iMap);

				pszMenuText = szTextBuf;
			}
			else if (i == C_CMD_DECORATION)
			{
				sprintf_s(szTextBuf, "%s: %s", g_szMenuSelections[i], g_szDecorationImages[m_iDecorationImage]);
				pszMenuText = szTextBuf;
			}
			else
				pszMenuText = g_szMenuSelections[i];

			m_pGame->WriteText(73.0f, 70.0f + (static_cast<float>(i) * 21.0f), pszMenuText);
		}		

		m_pGame->DrawFlatBox(105.0f, 220.0f, 155.0f, 135.0f);

		m_pGame->SetMenuColor(IRBRGame::MENU_HEADING);
		m_pGame->SetFont(IRBRGame::FONT_BIG);
		m_pGame->WriteText(30.0f, 170.0f, "This text should be behind the image");

		//m_pGame->DrawBox(GEN_BOX_LOGOS_CITROEN, 400.0f, 300.0f );
	}


	//------------------------------------------------------------------------------------------------//
	virtual void HandleFrontEndEvents( char txtKeyboard, bool bUp, bool bDown, bool bLeft, bool bRight, bool bSelect )
	{
		char szTextBuf[260];
		wchar_t wszTextBuf[260];

		long remappedX = 0;
		long remappedY = 0;
		long remappedCX = 0;
		long remappedCY = 0;
	
		if( bSelect )
		{
			if( m_iSelection == C_CMD_STAGESTART && NPlugin::GetStageName( m_iMap ) )
			{
				m_pGame->StartGame( m_iMap, 5, IRBRGame::GOOD_WEATHER, IRBRGame::TYRE_GRAVEL_DRY, NULL );
			}
			else if (m_iSelection == C_CMD_STAGEBTBSTART)
			{
				// rbrAppPath\RX_Content\Tracks\ folder should have the "Vyskälä SS1" BTB stage. 
				// You can download it from https://vileska.blogspot.com/p/vyskala-ss1.html website or use any other BTB stage as an example here.
				m_pNGPCarMenuAPI->PrepareBTBTrackLoad(m_dwPluginID, "Vyskälä SS1", "VyskalaSS1");

				// Remember that BTB tracks are loaded through track #41. Also, the tyre is recommended to match the surface of the track (ie. a snow BTB track with a gravel tyre is probably not a good idea)
				m_pGame->StartGame(41, 2, IRBRGame::GOOD_WEATHER, IRBRGame::TYRE_GRAVEL_DRY, NULL);
			}			
			else if (m_iSelection == C_CMD_DECORATION)
			{
				//
				// Toggle show/hide image
				//
				m_bShowImage = !m_bShowImage;
				m_pNGPCarMenuAPI->ShowHideImage(m_dwPluginID, C_DECORIMAGE_ID, m_bShowImage);
			}
			else if (m_iSelection == C_CMD_TEXTNOTICE)
			{
				//
				// Example of drawing custom text on RBR screen. DrawTextA/DrawTextW methods identify certain text with a textID value (the second parameter). TextID identifier can be used to change the text string of existing label.
				//

				m_bShowText = !m_bShowText;

				if (m_bShowText)
				{
					POINT   textPos;

					sprintf_s (szTextBuf,  sizeof(szTextBuf) / sizeof(char),     "Ipsolum %s absolum %s", NPlugin::GetStageName(m_iMap), m_szVersionText);
					swprintf_s(wszTextBuf, sizeof(szTextBuf) / sizeof(wchar_t), L"This unicode widechar %d should be front of the image", rand());

					// Draw a custom text with custom font style (text background is transparent), char string
					m_pNGPCarMenuAPI->DrawTextA(m_dwPluginID, 1, 5, 20, szTextBuf, m_dwFont1ID, D3DCOLOR_ARGB(255, 0x7F, 0x7F, 0x0), 0);
					

					// Draw a custom text on top of the custom image (tip! You may want to use MapRBRPointToScreenPoint method to map RBR menu coordinates to physical screen coordinates. DrawText methods take physical screen coordinates, not RBR in-game menu coordinates)
					// Uses random opaque (alpha color), random "blue" color tint and a random sliding X-position, widechar string
					int randomNum = RAND_MAX_MIN(255, 50);
					m_pNGPCarMenuAPI->MapRBRPointToScreenPoint(10 + (randomNum / 2), 210, &textPos.x, &textPos.y);
					m_pNGPCarMenuAPI->DrawTextW(m_dwPluginID, 2, textPos.x, textPos.y, wszTextBuf, m_dwFont2ID, D3DCOLOR_ARGB(randomNum, 0xF0, 0x00, randomNum), 0);


					// Draw a custom text where the text background is not transparent (clearTarget, overwrites everything behind the text area)
					m_pNGPCarMenuAPI->DrawTextA(m_dwPluginID, 3, textPos.x-10, textPos.y + 30, szTextBuf, m_dwFont1ID, D3DCOLOR_ARGB(255, 0x10, 0xCC, 0x10), D3DFONT_CLEARTARGET);
				}
				else
				{
					// Remove all custom text labels (nullptr string value in a specific textID label)
					m_pNGPCarMenuAPI->DrawTextA(m_dwPluginID, 1, 0, 0, nullptr, 0, 0, 0);
					m_pNGPCarMenuAPI->DrawTextA(m_dwPluginID, 2, 0, 0, nullptr, 0, 0, 0);
					m_pNGPCarMenuAPI->DrawTextW(m_dwPluginID, 3, 0, 0, nullptr, 0, 0, 0);
				}
			}
		}

		if( bUp )
		{
			--m_iSelection;
			if( m_iSelection < 0 )
				m_iSelection = NUM_SELECTIONS - 1;
		}
		
		if( bDown )
		{
			++m_iSelection;
			if( m_iSelection >= NUM_SELECTIONS )
				m_iSelection  = 0;
		}

		if (m_iSelection == C_CMD_MAP)
		{
			if (bLeft)
			{
				while (!NPlugin::GetStageName(--m_iMap))
				{
					if (m_iMap < 0)
						m_iMap = 99;
				}
			}

			if (bRight)
			{
				while (!NPlugin::GetStageName(++m_iMap))
				{
					if (m_iMap > 99)
						m_iMap = 0;
				}
			}

			//
			// MINIMAP drawing example (GetModulePath returns the RBR app path and m_iMap is used as a track filename identifier)
			// When map selection is changed then refresh the minimap to show track-mapID_M.trk track layout.
			// Note! This is a quick-and-dirty example because the code assumes that all tracks have overcast (O) trk file (which is not necessarily true. Production code should check which TRK file actually exists)
			// 
			m_pNGPCarMenuAPI->MapRBRPointToScreenPoint(250, 130, &remappedX, &remappedY);    // Map RBR in-game menu coordinates to native screen point coordinates
			m_pNGPCarMenuAPI->MapRBRPointToScreenPoint(380, 340, &remappedCX, &remappedCY);

			//m_pNGPCarMenuAPI->LoadCustomImage(m_dwPluginID, C_DECORIMAGE_ID, g_szDecorationImages[m_iDecorationImage],
			//	remappedX, remappedY, remappedCX, remappedCY,
			//	IMAGE_TEXTURE_STRETCH_TO_FILL | IMAGE_TEXTURE_ALPHA_BLEND);

			if(m_iMap ==  12)
				// When mapID==12 then draw a BTB minimap as an example even when map 12 is not really a BTB track (the minimap img filename should be driveline.ini file in BTB track folder)
				sprintf_s(szTextBuf, sizeof(szTextBuf) / sizeof(char), "%s\\RX_Content\\Tracks\\VyskalaSS1\\driveline.ini", m_pNGPCarMenuAPI->GetModulePath());
			else
				// Draw a minimap for the specified track (a map in Maps folder)
				sprintf_s(szTextBuf, sizeof(szTextBuf) / sizeof(char), "%s\\Maps\\track-%d_O.trk", m_pNGPCarMenuAPI->GetModulePath(), m_iMap);

			m_pNGPCarMenuAPI->LoadCustomImage(m_dwPluginID, C_MINIMAP_ID, szTextBuf, remappedX, remappedY, remappedCX, remappedCY, IMAGE_TEXTURE_POSITION_HORIZONTAL_RIGHT | IMAGE_TEXTURE_POSITION_VERTICAL_BOTTOM);
			m_pNGPCarMenuAPI->ShowHideImage(m_dwPluginID, C_MINIMAP_ID, true);
		}
		else if (m_iSelection == C_CMD_DECORATION)
		{
			if (bLeft)
			{
				if (--m_iDecorationImage < 0)
					m_iDecorationImage = NUM_DECORATIONIMAGES - 1;
			}

			if (bRight)
			{
				if (++m_iDecorationImage >= NUM_DECORATIONIMAGES)
					m_iDecorationImage = 0;
			}
		}

		if (m_iSelection == C_CMD_DECORATION)
		{
			// Remap RBR "game coordinates" to the actual screen coordinates (relative to screen resolution and menu coordinate fix in Fixup plugin).
			// Internally RBR uses 640x480 coordinate system while drawing a frontend menu. DX9 images use screen coordinates, so "RBR coordinates" need to be remapped.
			m_pNGPCarMenuAPI->MapRBRPointToScreenPoint(110, 155, &remappedX, &remappedY);
			m_pNGPCarMenuAPI->MapRBRPointToScreenPoint(140, 125, &remappedCX, &remappedCY);

			// Decoration menu line focused. Refresh image properties of the imageID. If properties are still the same then do nothing (ie. re-use the existing DX9 texture)
			m_pNGPCarMenuAPI->LoadCustomImage(m_dwPluginID, C_DECORIMAGE_ID, g_szDecorationImages[m_iDecorationImage],
					remappedX, remappedY, remappedCX, remappedCY,
					IMAGE_TEXTURE_STRETCH_TO_FILL | IMAGE_TEXTURE_ALPHA_BLEND);

			m_pNGPCarMenuAPI->ShowHideImage(m_dwPluginID, C_DECORIMAGE_ID, m_bShowImage);
		}
		else
			// Hide custom image because "Decoration" menu line is not focused
			m_pNGPCarMenuAPI->ShowHideImage(m_dwPluginID, C_DECORIMAGE_ID, false);
	}

	//------------------------------------------------------------------------------------------------//
	virtual void TickFrontEndPage( float fTimeDelta )
	{
		// Do nothing
	}

	//------------------------------------------------------------------------------------------------//
	/// Is called when the player timer starts (after GO! or in case of a false start)
	virtual void StageStarted ( int iMap, const char* ptxtPlayerName, bool bWasFalseStart )
	{
		// Do nothing
	}

	//------------------------------------------------------------------------------------------------//
	/// Is called when player finishes stage ( fFinishTime is 0.0f if player failed the stage )
	virtual void HandleResults ( float fCheckPoint1, float fCheckPoint2, float fFinishTime, const char* ptxtPlayerName )
	{
		// Do nothing
	}

	//------------------------------------------------------------------------------------------------//
	// Is called when a player passed a checkpoint 
	virtual void CheckPoint ( float fCheckPointTime, int iCheckPointID, const char* ptxtPlayerName )
	{
		// Do nothing
	}
};

#endif
