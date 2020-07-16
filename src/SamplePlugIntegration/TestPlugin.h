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

#define C_CMD_STAGESTART	0
#define C_CMD_MAP			1
#define C_CMD_DECORATION	2

#define NUM_SELECTIONS	(sizeof(g_szMenuSelections) / sizeof(g_szMenuSelections[0]))
const char* g_szMenuSelections[] =
{
	"Start Stage",
	"> Map",
	"> Decoration"
};

#define NUM_DECORATIONIMAGES (sizeof(g_szDecorationImages) / sizeof(g_szDecorationImages[0]))
const char* g_szDecorationImages[] =
{
	"C:\\Windows\\System32\\SecurityAndMaintenance.png",				// Just sample images, hopefully all PCs have these WinOS image files
	"C:\\Windows\\System32\\SecurityAndMaintenance_Alert.png",
	"C:\\Windows\\System32\\SecurityAndMaintenance_Error.png"
};

#define C_SAMPLEIMAGE_ID  100	// Anything above zero value. Each cached image is uniquely identified by ID
#define C_DECORIMAGE_ID   101

//------------------------------------------------------------------------------------------------//

class CTestPlugin : public IPlugin
{
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
			m_dwPluginID = m_pNGPCarMenuAPI->InitializePluginIntegration("SamplePlug1");

			// Show a sample image at 450,200 SCREEN coordinate position. If position should follow the DrawFrontEndPage RBR coordinates then see MapRBRPointToScreenPoint API function
			m_pNGPCarMenuAPI->LoadCustomImage(m_dwPluginID, C_SAMPLEIMAGE_ID, g_szDecorationImages[0], 450, 200, 40, 40, IMAGE_TEXTURE_PRESERVE_ASPECTRATIO_TOP);
			m_pNGPCarMenuAPI->ShowHideImage(m_dwPluginID, C_SAMPLEIMAGE_ID, true);
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

		m_pGame->DrawFlatBox(105.0f, 245.0f, 155.0f, 135.0f);

		m_pGame->SetMenuColor(IRBRGame::MENU_HEADING);
		m_pGame->SetFont(IRBRGame::FONT_BIG);
		m_pGame->WriteText(80.0f, 250.0f, "This text should be behind the image");

		//m_pGame->DrawBox(GEN_BOX_LOGOS_CITROEN, 400.0f, 300.0f );
	}

	//------------------------------------------------------------------------------------------------//
	virtual void HandleFrontEndEvents( char txtKeyboard, bool bUp, bool bDown, bool bLeft, bool bRight, bool bSelect )
	{
		if( bSelect )
		{
			if( m_iSelection == C_CMD_STAGESTART && NPlugin::GetStageName( m_iMap ) )
			{
				m_pGame->StartGame( m_iMap, 5, IRBRGame::GOOD_WEATHER, IRBRGame::TYRE_GRAVEL_DRY, NULL );
			}
			else if (m_iSelection == C_CMD_DECORATION)
			{
				// Toggle show/hide image
				m_bShowImage = !m_bShowImage;
				m_pNGPCarMenuAPI->ShowHideImage(m_dwPluginID, C_DECORIMAGE_ID, m_bShowImage);
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
			float remappedX = 0.0f;
			float remappedY = 0.0f;
			float remappedCX = 0.0f;
			float remappedCY = 0.0f;

			// Remap RBR "game coordinates" to the actual screen coordinates (relative to screen resolution and menu coordinate fix in Fixup plugin).
			// Internally RBR uses 640x480 coordinate system while drawing a frontend menu. DX9 images use screen coordinates, so "RBR coordinates" need to be remapped.
			m_pNGPCarMenuAPI->MapRBRPointToScreenPoint(110.0f, 250.0f, &remappedX, &remappedY);
			m_pNGPCarMenuAPI->MapRBRPointToScreenPoint(140.0f, 125.0f, &remappedCX, &remappedCY);

			// Decoration menu line focused. Refresh image properties of the imageID. If properties are still the same then do nothing (ie. re-use the existing DX9 texture)
			m_pNGPCarMenuAPI->LoadCustomImage(m_dwPluginID, C_DECORIMAGE_ID, g_szDecorationImages[m_iDecorationImage],
					static_cast<int>(remappedX), static_cast<int>(remappedY), 
					static_cast<int>(remappedCX), static_cast<int>(remappedCY),
					IMAGE_TEXTURE_STRETCH_TO_FILL);

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
		m_pGame->SetColor( 1.0f, 1.0f, 1.0f, 1.0f );
		m_pGame->WriteGameMessage( "Stage Started", 10.0f, 50.0f, 150.0f );
	}

	//------------------------------------------------------------------------------------------------//
	/// Is called when player finishes stage ( fFinishTime is 0.0f if player failed the stage )
	virtual void HandleResults ( float fCheckPoint1, float fCheckPoint2, float fFinishTime, const char* ptxtPlayerName )
	{
		//fprintf( fp, "Stage Result for \"%s\": \n[CP1] = %s\n[CP2] = %s\n[Finish] = %s\n\n", ptxtPlayerName, 
		//			NPlugin::FormatTimeString( txtCP1, fCheckPoint1 ),
		//			NPlugin::FormatTimeString( txtCP2, fCheckPoint2 ),
		//			NPlugin::FormatTimeString( txtTimeString, fFinishTime ) );
	}

	//------------------------------------------------------------------------------------------------//
	// Is called when a player passed a checkpoint 
	virtual void CheckPoint ( float fCheckPointTime, int iCheckPointID, const char* ptxtPlayerName )
	{
		m_pGame->SetColor( 1.0f, 1.0f, 1.0f, 1.0f );
		m_pGame->WriteGameMessage( "CHECKPOINT!!!!!!!!!!", 5.0f, 50.0f, 250.0f );
	}

private:
	IRBRGame*		m_pGame;
	int				m_iSelection;
	int				m_iMap;

	float			m_fResults[ 3 ];
	char			m_szPlayerName[ 32 ];

	int				m_iDecorationImage;	
	bool			m_bShowImage;

	CNGPCarMenuAPI* m_pNGPCarMenuAPI;
	DWORD			m_dwPluginID;
};

#endif
