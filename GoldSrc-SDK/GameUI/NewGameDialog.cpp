//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "NewGameDialog.h"
#include "GameUI_Interface.h"
#include "ModInfo.h"
#include "Taskbar.h"

#include "Button.h"
#include "ComboBox.h"
#include "RadioButton.h"
#include "wrect.h"
#include "cl_dll.h"

#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#define stricmp strcasecmp
#endif

CNewGameDialog::CNewGameDialog( vgui2::Panel* parent )
	: BaseClass( parent, "NewGameDialog", true, false )
{
	SetHDProportional( true );
	SetBounds( 0, 0, 222, 160 );
	SetSizeable( false );
	MakePopup( true, false );
	SetCloseSound( "sound/UI/launch_dnmenu1.wav" );
	g_pTaskbar->AddTask( GetVPanel() );
	SetTitle( "#GameUI_NewGame", true );
	m_nPlayMode = 0;

	m_pDifficultySelection = new vgui2::ComboBox( this, "Difficulty", 3, false );
	m_pDifficultySelection->AddItem( "#GameUI_Easy", NULL );
	int medium = m_pDifficultySelection->AddItem( "#GameUI_Medium", NULL );
	m_pDifficultySelection->AddItem( "#GameUI_Hard", NULL );
	m_pDifficultySelection->ActivateItem( medium );
	m_pDifficultySelection->SetVisible( false );

	vgui2::Button* cancel = new vgui2::Button( this, "Cancel", "#GameUI_Cancel" );
	cancel->SetCommand( "Close" );

	vgui2::Button* oldPlay = new vgui2::Button( this, "Play", "#GameUI_Play" );
	oldPlay->SetEnabled( false );
	oldPlay->SetVisible( false );

	vgui2::Button* play = new vgui2::Button( this, "PlayGame", "#GameUI_PlayGame" );
	play->SetEnabled( false );
	play->SetVisible( false );

	m_pAltGameButton = new vgui2::Button( this, "PlayGame_Alt", "#GameUI_PlayGame_Alt" );
	m_pAltGameButton->SetVisible( false );

	m_pEasy = new vgui2::RadioButton( this, "Easy", "#GameUI_Easy" );
	m_pMedium = new vgui2::RadioButton( this, "Medium", "#GameUI_Medium" );
	m_pHard = new vgui2::RadioButton( this, "Hard", "#GameUI_Hard" );
	m_pTraining = new vgui2::RadioButton( this, "Training", "#GameUI_TrainingRoom" );
	m_pEasy->SetVisible( false );
	m_pMedium->SetVisible( false );
	m_pHard->SetVisible( false );
	m_pTraining->SetVisible( false );

	LoadControlSettings( "Resource\\NewGameDialog.res", NULL );
	if ( stricmp( ModInfo()->GetGameDescription(), "Half-Life" ) == 0 )
	{
		m_pAltGameButton->SetVisible( true );
	}
	else
	{
		play->SetText( "#GameUI_Play" );
		m_pAltGameButton->SetVisible( false );
		m_pAltGameButton->DeletePanel();
		m_pAltGameButton = NULL;
	}

	if ( play->IsVisible() )
	{
		play->SetEnabled( true );
		oldPlay->SetVisible( false );
		oldPlay->SetEnabled( false );
	}
	else
	{
		oldPlay->SetVisible( true );
		oldPlay->SetEnabled( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CNewGameDialog::~CNewGameDialog()
{
}

void CNewGameDialog::OnCommand( const char* command )
{
	char mapcommand[512];
	mapcommand[0] = 0;

	bool difficultyVisible = m_pDifficultySelection->IsVisible();
	if ( difficultyVisible )
	{
		m_nPlayMode = m_pDifficultySelection->GetActiveItem() + 1;
	}
	else
	{
		// Determine mode
		m_nPlayMode = 0;
		if ( m_pEasy->IsSelected() )
			m_nPlayMode = 1;
		else if ( m_pMedium->IsSelected() )
			m_nPlayMode = 2;
		else if ( m_pHard->IsSelected() )
			m_nPlayMode = 3;
	}

	if ( stricmp( command, "Play" ) == 0 )
	{
		if ( difficultyVisible || m_nPlayMode > 0 )
		{
			sprintf( mapcommand, "disconnect\nmaxplayers 1\ndeathmatch 0\nskill %i\nmap %s\n",
				m_nPlayMode, ModInfo()->GetStartMap() );
		}
		else
		{
			sprintf( mapcommand, "disconnect\nmaxplayers 1\ndeathmatch 0\nmap %s\n", ModInfo()->GetTrainMap() );
		}

		engine->pfnClientCmd( mapcommand );
		if ( stricmp( ModInfo()->GetGameDescription(), "Condition-Zero" ) == 0 )
			engine->pfnClientCmd( "cl_newgame 1\n" );
	}
	else if ( stricmp( command, "PlayGameAlt" ) == 0 )
	{
		sprintf( mapcommand, "disconnect\nmaxplayers 1\ndeathmatch 0\nskill %i\nmap hldemo1.bsp", m_nPlayMode );
		engine->pfnClientCmd( mapcommand );
	}
	else if ( stricmp( command, "PlayTraining" ) == 0 )
	{
		sprintf( mapcommand, "disconnect\nmaxplayers 1\ndeathmatch 0\nmap %s\n", ModInfo()->GetTrainMap() );
		engine->pfnClientCmd( mapcommand );
	}
	else
	{
		BaseClass::OnCommand( command );
		return;
	}

	OnClose();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNewGameDialog::OnClose()
{
	BaseClass::OnClose();
	MarkForDeletion();
}
