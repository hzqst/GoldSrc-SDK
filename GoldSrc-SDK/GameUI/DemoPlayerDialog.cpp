// 
//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: DemoPlayerDialog.cpp: implementation of the CDemoPlayerDialog class.
//
// $NoKeywords: $
//=============================================================================

#include "DemoPlayerDialog.h"
#include "DemoEventsDialog.h"
#include "DemoPlayerFileDialog.h"
#include "GameUI_Interface.h"

#include "Button.h"
#include "IDemoPlayer.h"
#include "Label.h"
#include "Slider.h"
#include "SystemWrapper.h"
#include "ToggleButton.h"
#include "vgui/ILocalize.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"
#include "../common/const.h"
#include "../HLTV/common/IWorld.h"

#include <stdio.h>
#include <string.h>

#ifdef PostMessage
#undef PostMessage
#endif

CDemoPlayerDialog::CDemoPlayerDialog( vgui2::Panel* parent )
	: BaseClass( parent, "DemoPlayerDialog", true, false )
{
	SetHDProportional( true );
	m_Engine = NULL;
	m_DemoPlayer = NULL;
	m_System = NULL;
	m_World = NULL;
	m_Serial = 4001;	//HACK HACK, assuming we will never load more than 4000 modules
	m_SystemTime = 0.0;
	if ( !LoadModules() )
	{
		SetVisible( false );
		return;
	}

	SetBounds( 0, 0, GetProportionalScaledValue( 464 ),
		GetProportionalScaledValue( m_DemoPlayer->IsEditMode() ? 128 : 96 ) );
	SetSizeable( false );
	SetMoveable( true );
	vgui2::surface()->CreatePopup( GetVPanel(), false, true, false, true, true );
	SetVisible( true );
	SetTitle( "#GameUI_DemoPlayer", true );
	// main panel

	m_pLableTimeCode = new vgui2::Label( this, "TimeLabel", "00:00:00" );
	m_pTimeSlider = new vgui2::Slider( this, "TimeSlider" );
	m_pTimeSlider->SetRange( 0, 60 );
	m_pTimeSlider->AddActionSignalTarget( this );
//	Button *button;

	// Demo player buttons
	m_pButtonLoad = new vgui2::Button( this, "LoadButton", "" );
	m_pButtonPlay = new vgui2::Button( this, "PlayButton", "" );
	m_pButtonPause = new vgui2::Button( this, "PauseButton", "" );
	m_pButtonStepF = new vgui2::Button( this, "StepFButton", "" );
	m_pButtonStepB = new vgui2::Button( this, "StepBButton", "" );
	m_pButtonStart = new vgui2::Button( this, "StartButton", "" );
	m_pButtonEnd = new vgui2::Button( this, "EndButton", "" );
	m_pButtonSlower = new vgui2::Button( this, "SlowerButton", "" );
	m_pButtonFaster = new vgui2::Button( this, "FasterButton", "" );
	m_pButtonStop = new vgui2::Button( this, "StopButton", "" );
	// Demo editor buttons

	m_MasterButton = new vgui2::ToggleButton( this, "MasterButton", "Master" );
	m_MasterButton->AddActionSignalTarget( this );
	new vgui2::Button( this, "EventsButton", "#GameUI_Events" );
	new vgui2::Button( this, "SaveButton", "#GameUI_Save" );
	m_NextTimeScale = 2.0f; // = x2
	m_lastSliderTime = -1;
	LoadControlSettings( "Resource\\DemoPlayerDialog.res", NULL );
	m_hDemoEventsDialog = new CDemoEventsDialog( this, "DemoEventsDialog", m_Engine, m_DemoPlayer );
	m_hDemoEventsDialog->AddActionSignalTarget( this );
	m_hDemoPlayerFileDialog = NULL;
}

CDemoPlayerDialog::~CDemoPlayerDialog()
{
	if ( m_DemoPlayer )
		m_DemoPlayer->RemoveListener( this );
}

void CDemoPlayerDialog::OnThink()
{
	BaseClass::OnThink();
	if ( m_DemoPlayer && m_DemoPlayer->IsActive() )
		Update();
}

void CDemoPlayerDialog::OnClose()
{
	BaseClass::OnClose();
	MarkForDeletion();
}

void CDemoPlayerDialog::Update()
{
	if ( !m_DemoPlayer )
		return;

	if ( m_DemoPlayer->IsActive() )
	{
		if ( m_DemoPlayer->IsLoading() )
		{
			wchar_t title[256];
			wchar_t filename[128];
			vgui2::localize()->ConvertANSIToUnicode( m_DemoPlayer->GetFileName(), filename, sizeof( filename ) );
			vgui2::localize()->ConstructString( title, sizeof( title ),
				vgui2::localize()->Find( "#GameUI_LoadingFilename" ), 1, filename );
			SetTitle( title, false );
		}
		else
		{
			wchar_t title[128];
			vgui2::localize()->ConvertANSIToUnicode( m_DemoPlayer->GetFileName(), title, sizeof( title ) );
			SetTitle( title, false );
		}
	}
	else
	{
		SetTitle( "#GameUI_DemoPlayer", false );
	}

	frame_t* firstFrame = m_World->GetFirstFrame();
	frame_t* lastFrame = m_World->GetLastFrame();
	if ( firstFrame && lastFrame )
	{
		m_pTimeSlider->SetRange( (int)firstFrame->time, (int)lastFrame->time );
		m_pTimeSlider->InvalidateLayout( false, false );
	}

	double worldTime = m_DemoPlayer->GetWorldTime();
	float timeScale = m_DemoPlayer->GetTimeScale();
	int sliderTime = m_pTimeSlider->GetValue();
	if ( sliderTime != m_lastSliderTime )
	{
		// slider has been moved by user, apply changes to editor
		m_DemoPlayer->SetWorldTime( sliderTime, false );
		if ( m_lastSliderTime > 0 )
			OnPause();	// pause game while brwosing tru time
		m_lastSliderTime = sliderTime;
	}
	else
	{
		m_lastSliderTime = (int)worldTime;
		m_pTimeSlider->SetValue( (int)worldTime, true );
	}

	wchar_t* wDescription = vgui2::localize()->Find( "#Valve_Playing" );
	if ( !m_DemoPlayer->IsActive() )
		wDescription = vgui2::localize()->Find( "#Valve_Stopped" );
	else if ( m_DemoPlayer->IsPaused() )
		wDescription = vgui2::localize()->Find( "#Valve_Paused" );

	wchar_t* wTimeScaleString;
	if ( timeScale == 0.25f )
		wTimeScaleString = vgui2::localize()->Find( "#Valve_x_1_4" );
	else if ( timeScale == 0.5f )
		wTimeScaleString = vgui2::localize()->Find( "#Valve_x_1_2" );
	else if ( timeScale == 1.0f )
		wTimeScaleString = vgui2::localize()->Find( "#Valve_x_1" );
	else if ( timeScale == 2.0f )
		wTimeScaleString = vgui2::localize()->Find( "#Valve_x_2" );
	else if ( timeScale == 4.0f )
		wTimeScaleString = vgui2::localize()->Find( "#Valve_x_4" );
	else
		wTimeScaleString = vgui2::localize()->Find( "#Valve_x_1" );

	char szTimeCode[32]; // mins:secs:msecs	
	wchar_t wTimeCode[32];
	wchar_t wTitle[128];
#ifdef _WIN32
	_snprintf( szTimeCode, sizeof( szTimeCode ) - 1, "%02u:%02u:%02u",
#else
	snprintf( szTimeCode, sizeof( szTimeCode ) - 1, "%02u:%02u:%02u",
#endif
		(unsigned int)( (int)worldTime / 60 ), (unsigned int)( (int)worldTime % 60 ),
		(unsigned int)( (int)( worldTime * 100.0 ) % 100 ) );
	szTimeCode[sizeof( szTimeCode ) - 1] = 0;
	vgui2::localize()->ConvertANSIToUnicode( szTimeCode, wTimeCode, sizeof( wTimeCode ) );
	wchar_t* wTemp = vgui2::localize()->Find( "#Valve_ViewDemoStatus" );
	vgui2::localize()->ConstructString( wTitle, sizeof( wTitle ), wTemp, 3, wTimeCode, wTimeScaleString, wDescription );
	m_pLableTimeCode->SetText( wTitle );
}

bool CDemoPlayerDialog::LoadModules()
{
	m_System = g_pSystemWrapper;
	if ( !m_System )
		return false;

	m_Engine = (IEngineWrapper*)m_System->GetModule( "enginewrapper002", "", NULL );
	if ( !m_Engine )
	{
		m_System->Printf( "CDemoPlayerDialog::LoadModules: couldn't get engine interface.\n" );
		return false;
	}

	m_DemoPlayer = (IDemoPlayer*)m_System->GetModule( "demoplayer001", "", NULL );
	if ( !m_DemoPlayer )
	{
		m_System->Printf( "CDemoPlayerDialog::LoadModules: couldn't load demo player module.\n" );
		return false;
	}

	m_DemoPlayer->RegisterListener( this );
	m_World = m_DemoPlayer->GetWorld();
	if ( !m_World )
	{
		m_System->Printf( "CDemoPlayerDialog::LoadModules: couldn't get world module.\n" );
		return false;
	}

	return true;
}

void CDemoPlayerDialog::OnLoadDemo( const char* demoname )
{
	char filteredname[270];
	char fullstring[270];
	strncpy( filteredname, demoname, sizeof( filteredname ) );
	filteredname[sizeof( filteredname ) - 1] = 0;
	filteredname[strcspn( filteredname, ";\n" )] = 0;
#ifdef _WIN32
	_snprintf( fullstring, sizeof( fullstring ), "viewdemo %s\n", filteredname );
#else
	snprintf( fullstring, sizeof( fullstring ), "viewdemo %s\n", filteredname );
#endif
	m_DemoPlayer->Stop();
	m_Engine->Cbuf_AddText( fullstring );
}

void CDemoPlayerDialog::OnPause()
{
	m_DemoPlayer->SetPaused( true );
	m_Engine->Cbuf_AddText( "stopsound\n" ); // Stop sound
}

void CDemoPlayerDialog::OnPlay()
{
	m_DemoPlayer->SetPaused( false );
	m_Engine->SetCvar( "spec_autodirector", "1" ); // enable director mode
}

void CDemoPlayerDialog::OnStart()
{
	frame_t* frame = m_World->GetFirstFrame();
	if ( !frame )
		return;

	m_DemoPlayer->SetWorldTime( frame->time - 0.01f, false );
	m_DemoPlayer->SetTimeScale( 1.0f );
	m_DemoPlayer->SetPaused( true );
}

void CDemoPlayerDialog::OnEnd()
{
	frame_t* frame = m_World->GetLastFrame();
	if ( !frame )
		return;

	m_DemoPlayer->SetWorldTime( frame->time, false );
	OnPause();
}

void CDemoPlayerDialog::OnSlower()
{
	float timeScale = m_DemoPlayer->GetTimeScale();
	if ( timeScale > 0.25f )
		m_DemoPlayer->SetTimeScale( timeScale * 0.5f );
}

void CDemoPlayerDialog::OnFaster()
{
	float timeScale = m_DemoPlayer->GetTimeScale();
	if ( timeScale < 4.0f )
		m_DemoPlayer->SetTimeScale( timeScale * 2.0f );
}

void CDemoPlayerDialog::OnLoad()
{
	if ( !m_hDemoPlayerFileDialog )
	{
		m_hDemoPlayerFileDialog = new CDemoPlayerFileDialog( this, "DemoPlayerFileDialog" );
		m_hDemoPlayerFileDialog->AddActionSignalTarget( this );
	}

	m_hDemoPlayerFileDialog->Activate();
}

void CDemoPlayerDialog::OnStop()
{
	m_DemoPlayer->Stop();
	m_Engine->Cbuf_AddText( "stopdemo\n" );
	Update();
}

void CDemoPlayerDialog::OnNextFrame( int direction )
{
	float worldTime = (float)m_DemoPlayer->GetWorldTime();
	frame_t* frame = m_World->GetFrameByTime( worldTime );
	if ( !frame )
		return;

	frame = m_World->GetFrameBySeqNr( frame->seqnr + direction );
	if ( !frame )
		return;

	m_DemoPlayer->SetWorldTime( frame->time, false );
	m_DemoPlayer->SetPaused( true );
}

void CDemoPlayerDialog::OnEvents()
{
	if ( !m_hDemoEventsDialog )
	{
		m_hDemoEventsDialog = new CDemoEventsDialog( this, "DemoEventsDialog", m_Engine, m_DemoPlayer );
		m_hDemoEventsDialog->AddActionSignalTarget( this );
	}

	m_hDemoEventsDialog->Activate();
	PostMessage( m_hDemoEventsDialog->GetVPanel(), new KeyValues( "UpdateCmdList" ), 0.0f ); // update event list
}

void CDemoPlayerDialog::OnSave()
{
	m_DemoPlayer->SaveGame( NULL ); // TODO, allow other name
}

void CDemoPlayerDialog::ReceiveSignal( ISystemModule* module, unsigned int signal, void* )
{
	if ( !m_DemoPlayer )
		return;

#ifdef _WIN32
	int serial = m_DemoPlayer->GetSerial();
	if ( module->GetSerial() != serial )
#else
	int serial = module->GetSerial();
	if ( serial != m_DemoPlayer->GetSerial() )
#endif
		return;

	if ( signal == 1 )
	{
		if ( m_DemoPlayer->IsEditMode() )
			PostMessage( m_hDemoEventsDialog->GetVPanel(), new KeyValues( "UpdateCmdList" ), 0.0f );
	}
	else if ( signal == 2 )
	{
		if ( m_DemoPlayer->IsEditMode() )
			PostMessage( m_hDemoEventsDialog->GetVPanel(), new KeyValues( "UpdateLastCmd" ), 0.0f );
	}
	else
	{
		m_System->Printf( "CDemoPlayerDialog::ReceiveSignal: unknown signal %i.\n", signal );
	}
}

void CDemoPlayerDialog::OnCommand( const char* command )
{
	if ( m_DemoPlayer && m_World && m_Engine )
	{
		if ( !strcmp( command, "pause" ) )
			OnPause();
		else if ( !strcmp( command, "load" ) )
			OnLoad();
		else if ( !strcmp( command, "play" ) )
			OnPlay();
		else if ( !strcmp( command, "stepf" ) )
			OnNextFrame( 1 );
		else if ( !strcmp( command, "stepb" ) )
			OnNextFrame( -1 );
		else if ( !strcmp( command, "start" ) )
			OnStart();
		else if ( !strcmp( command, "end" ) )
			OnEnd();
		else if ( !strcmp( command, "slower" ) )
			OnSlower();
		else if ( !strcmp( command, "faster" ) )
			OnFaster();
		else if ( !strcmp( command, "load" ) )
			OnLoad();
		else if ( !strcmp( command, "stop" ) )
			OnStop();
		else if ( !strcmp( command, "events" ) )
			OnEvents();
		else if ( !strcmp( command, "save" ) )
			OnSave();
	}

	BaseClass::OnCommand( command );
}

void CDemoPlayerDialog::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	m_pButtonLoad->SetImageAtIndex( 0, vgui2::scheme()->GetImage( "Resource\\icon_load", false ), 0 );
	m_pButtonPlay->SetImageAtIndex( 0, vgui2::scheme()->GetImage( "Resource\\icon_play", false ), 0 );
	m_pButtonPause->SetImageAtIndex( 0, vgui2::scheme()->GetImage( "Resource\\icon_pause", false ), 0 );
	m_pButtonStepF->SetImageAtIndex( 0, vgui2::scheme()->GetImage( "Resource\\icon_stepf", false ), 0 );
	m_pButtonStepB->SetImageAtIndex( 0, vgui2::scheme()->GetImage( "Resource\\icon_stepb", false ), 0 );
	m_pButtonStart->SetImageAtIndex( 0, vgui2::scheme()->GetImage( "Resource\\icon_start", false ), 0 );
	m_pButtonEnd->SetImageAtIndex( 0, vgui2::scheme()->GetImage( "Resource\\icon_end", false ), 0 );
	m_pButtonStop->SetImageAtIndex( 0, vgui2::scheme()->GetImage( "Resource\\icon_stop", false ), 0 );
	m_pButtonFaster->SetImageAtIndex( 0, vgui2::scheme()->GetImage( "Resource\\icon_faster", false ), 0 );
	m_pButtonSlower->SetImageAtIndex( 0, vgui2::scheme()->GetImage( "Resource\\icon_slower", false ), 0 );
}
