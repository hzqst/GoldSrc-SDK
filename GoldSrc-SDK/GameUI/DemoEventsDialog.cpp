#include "DemoEventsDialog.h"
#include "DemoEditDialog.h"

#include "Button.h"
#include "ListPanel.h"
#include "IDemoPlayer.h"
#include "IObjectContainer.h"
#include "SystemWrapper.h"
#include "vgui/ISurface.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"

#include <stdio.h>
#include <string.h>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CDemoEventsDialog::CDemoEventsDialog( vgui2::Panel* parent, const char* name, IEngineWrapper* engine,
		IDemoPlayer* demoPlayer ) : vgui2::Frame( parent, name, true, false )
{
	SetHDProportional( true );
	m_DemoPlayer = demoPlayer;
	m_Engine = engine;
	SetBounds( 0, 0, GetProportionalScaledValue( 464 ), GetProportionalScaledValue( 380 ) );
	SetSizeable( false );
	vgui2::surface()->CreatePopup( GetVPanel(), false, true, false, true, true );
	SetTitle( "#GameUI_EventList", true );
	m_EventList = new vgui2::ListPanel( this, "EventList" );
	new vgui2::Button( this, "AddButton", "#GameUI_Add" );
	new vgui2::Button( this, "RemoveButton", "#GameUI_Remove" );
	new vgui2::Button( this, "ModifyButton", "#GameUI_Modify" );
	new vgui2::Button( this, "GotoButton", "#GameUI_Goto" );
	LoadControlSettings( "Resource/DemoEventsDialog.res", NULL );
	m_EventList->AddColumnHeader( 0, "time", "#GameUI_Time", GetProportionalScaledValue( 32 ), 0 );
	m_EventList->AddColumnHeader( 1, "type", "#GameUI_Type", GetProportionalScaledValue( 128 ), 0 );
	m_CurrentCmd = NULL;
	m_EditDialog = new CDemoEditDialog( this, "DemoEditDialog", m_Engine, m_DemoPlayer );
	m_EditDialog->Activate();
}

CDemoEventsDialog::~CDemoEventsDialog()
{
}

void CDemoEventsDialog::OnUpdate()
{
	m_EventList->DeleteAllItems();
	IObjectContainer* commands = m_DemoPlayer->GetCommands();
	if ( !commands )
		return;

	for ( DirectorCmd* command = (DirectorCmd*)commands->GetFirst(); command;
			command = (DirectorCmd*)commands->GetNext() )
	{
		char index[8];
		char time[32];
		float hundredths = command->m_Time * 100.0f;
		int seconds = (int)command->m_Time;
		sprintf( time, "%03u:%02u:%02u", (unsigned int)( seconds / 60 ), (unsigned int)( seconds % 60 ),
				(unsigned int)( (int)hundredths % 100 ) );
		sprintf( index, "%i", command->m_Index );
		KeyValues* data = new KeyValues( index, "time", time, "type", command->ToString() );
		m_EventList->AddItem( data, 0, false, false );
		data->deleteThis();
	}

	m_CurrentCmd = m_DemoPlayer->GetLastCommand();
	int row = m_CurrentCmd ? m_CurrentCmd->m_Index - 1 : 0;
	m_EventList->SetSingleSelectedItem( m_EventList->GetItemIDFromRow( row ) );
}

void CDemoEventsDialog::GetCurrentCmd()
{
	m_CurrentCmd = NULL;
	// get first selected item
	int selected = m_EventList->GetSelectedItem( 0 );
	if ( selected < 0 )
		return;

	int row = m_EventList->GetItemCurrentRow( selected );
	IObjectContainer* commands = m_DemoPlayer->GetCommands();
	if ( !commands )
		return;

	for ( DirectorCmd* command = (DirectorCmd*)commands->GetFirst(); command;
			command = (DirectorCmd*)commands->GetNext() )
	{
		if ( command->m_Index == row + 1 )
		{
			m_CurrentCmd = command;
			return;
		}
	}
}

void CDemoEventsDialog::OnGoto()
{
	if ( !m_CurrentCmd )
		return;

	m_DemoPlayer->SetWorldTime( m_CurrentCmd->m_Time, false );
	m_DemoPlayer->ExecuteDirectorCmd( m_CurrentCmd );
	m_DemoPlayer->SetPaused( true );
}

void CDemoEventsDialog::OnModify()
{
	if ( !m_CurrentCmd )
		return;

	m_EditDialog->SetCommand( m_CurrentCmd );
	m_EditDialog->Activate();
}

void CDemoEventsDialog::OnRemove()
{
	if ( m_CurrentCmd )
		m_DemoPlayer->RemoveCommand( m_CurrentCmd->m_Index );
}

void CDemoEventsDialog::OnAdd()
{
	DirectorCmd command;
	vec3_t origin;
	vec3_t angles;
	m_DemoPlayer->SetPaused( true );
	m_Engine->GetViewOrigin( origin );
	m_Engine->GetViewAngles( angles );
	command.m_Type = DRC_CMD_CAMERA;
	command.Clear();
	command.m_Time = (float)m_DemoPlayer->GetWorldTime();
	command.SetCameraData( origin, angles, 90.0f, 0 ); // camera event as default
	command.m_Index = -1; // mark as new command
	m_EditDialog->SetCommand( &command );
	m_EditDialog->Activate();
}

void CDemoEventsDialog::OnCommand( const char* command )
{
	if ( m_DemoPlayer && m_Engine )
	{
		GetCurrentCmd();
		if ( !strcmp( command, "goto" ) )
			OnGoto();
		else if ( !strcmp( command, "remove" ) )
			OnRemove();
		else if ( !strcmp( command, "modify" ) )
			OnModify();
		else if ( !strcmp( command, "add" ) )
			OnAdd();
	}
	BaseClass::OnCommand( command );
}

void CDemoEventsDialog::OnUpdateLastCmd()
{
	OnUpdate();
}
