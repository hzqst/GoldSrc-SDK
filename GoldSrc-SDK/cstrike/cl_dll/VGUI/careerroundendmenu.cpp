#include "careerroundendmenu.h"
#include "careermenus.h"
#include "CareerBox.h"
#include "hud.h"
#include "GameUI_Interfaces.h"
#include "GameUI/ICareerUI.h"
#include "GameUI/IMusicManager.h"
#include "Button.h"
#include "Label.h"
#include "game_controls/iviewport.h"
#include "vgui_controls/Controls.h"
#include "vgui/ILocalize.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void IN_AttackUp();
extern void IN_ScoreUp();
extern char* SharedVarArgs( char* format, ... );

CareerMatchStatus TheCareerMatchStatus;

class TaskLabel : public vgui2::Label
{
public:
	TaskLabel( vgui2::Panel* parent, const char* name, const char* text );
	~TaskLabel() override;
	void SetTaskStatus( bool complete );
	void OnThink() override;
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
private:
	Color m_flashColor;
	Color m_completeColor;
	Color m_incompleteColor;
	float m_interval;
	float m_lastTransition;
	int m_transitionsLeft;
	bool m_isComplete;
	bool m_isFlashColor;
};

TaskLabel::TaskLabel( vgui2::Panel* parent, const char* name, const char* text )
	: vgui2::Label( parent, name, text )
{
	m_isComplete = false;
	m_isFlashColor = false;
	m_interval = 0;
	m_lastTransition = 0;
	m_transitionsLeft = 0;
}

TaskLabel::~TaskLabel()
{
}

void TaskLabel::SetTaskStatus( bool complete )
{
	m_isComplete = complete;
	m_isFlashColor = false;
	m_lastTransition = (float)gEngfuncs.GetAbsoluteTime();
	m_transitionsLeft = -1;
	SetFgColor( complete ? m_completeColor : m_incompleteColor );
}

void TaskLabel::OnThink()
{
	if ( m_isComplete )
		return;
	float now = (float)gEngfuncs.GetAbsoluteTime();
	if ( now >= m_lastTransition + m_interval && m_transitionsLeft > 0 )
	{
		m_lastTransition = now;
		SetFgColor( m_isFlashColor ? m_flashColor : m_incompleteColor );
		m_isFlashColor = !m_isFlashColor;
		--m_transitionsLeft;
	}
	else if ( m_transitionsLeft <= 0 )
	{
		SetFgColor( m_flashColor );
		m_isFlashColor = true;
	}
}

void TaskLabel::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	m_completeColor = scheme->GetColor( "TaskComplete", Color( 0, 0, 255, 255 ) );
	m_incompleteColor = scheme->GetColor( "TaskIncomplete", Color( 255, 0, 0, 255 ) );
	m_flashColor = scheme->GetColor( "TaskFlash", Color( 255, 255, 255, 0 ) );
	m_interval = (float)atof( scheme->GetResourceString( "TaskFlashPeriod" ) );
	SetFont( scheme->GetFont( "Task", IsProportional() ) );
	if ( m_transitionsLeft < 0 )
		m_transitionsLeft = atoi( scheme->GetResourceString( "TaskNumFlashes" ) );
	if ( m_isComplete )
		SetFgColor( m_completeColor );
}

void OnRoundEndMenuOpen( bool won )
{
	if ( g_pCareerUI != NULL && g_pCareerUI->IsPlayingMatch() )
	{
		g_pCareerUI->OnRoundEndMenuOpen( won );
	}
	else if ( g_pMusicManager != NULL )
	{
		if ( won )
			g_pMusicManager->StartTrack( TRACKTYPE_ROUNDWIN, false );
		else
			g_pMusicManager->StartTrack( TRACKTYPE_ROUNDLOSE, false );
	}
}

void OnRoundEndMenuClose( bool playing )
{
	if ( g_pCareerUI != NULL && g_pCareerUI->IsPlayingMatch() )
		g_pCareerUI->OnRoundEndMenuClose( playing );
	else if ( g_pMusicManager != NULL )
		g_pMusicManager->FadeOutTrack();
}

void ShowCareerRoundEndMenu( int ctWins, int tWins, int neededWins, int winBy, int roundResult )
{
	TheCareerMatchStatus.ctWins = ctWins;
	TheCareerMatchStatus.tWins = tWins;
	TheCareerMatchStatus.neededWins = neededWins;
	TheCareerMatchStatus.winBy = winBy;
	TheCareerMatchStatus.roundResult = roundResult;
	if ( gViewPortInterface != NULL )
	{
		CCareerBaseBox::CloseOpenBoxes( false );
		for ( int menu = 1; menu < 40; ++menu )
		{
			if ( menu != 2 && menu != 27 && menu != 26 )
				gViewPortInterface->HideVGUIMenu( menu );
		}
		gViewPortInterface->ShowVGUIMenu( 35 );
	}
}

void ShowCareerMatchEndMenu( int ctWins, int tWins, int neededWins, int winBy, int roundResult )
{
	TheCareerMatchStatus.ctWins = ctWins;
	TheCareerMatchStatus.tWins = tWins;
	TheCareerMatchStatus.neededWins = neededWins;
	TheCareerMatchStatus.winBy = winBy;
	TheCareerMatchStatus.roundResult = roundResult;
	if ( gViewPortInterface != NULL )
	{
		CCareerBaseBox::CloseOpenBoxes( false );
		gViewPortInterface->HideAllVGUIMenu();
		gViewPortInterface->ShowVGUIMenu( 36 );
	}
}

wchar_t* GetTaskStatusString( ICareerTask* task )
{
	static wchar_t partialStr[512];
	char ascii[512];
	wchar_t tmpBuf[512];

	partialStr[0] = 0;

	if ( task->IsComplete() )
	{
		vgui2::localize()->ConstructString( partialStr, sizeof( partialStr ), vgui2::localize()->Find( "#Career_Task_Complete" ), 0 );
	}
	else
	{
		int numLeft = task->GetRepeat() - task->GetPartial();
		if ( numLeft > 0 && task->GetRepeat() > 1 )
		{
			if ( task->IsMaxRoundTime() )
			{
				vgui2::localize()->ConstructString( partialStr, sizeof( partialStr ), vgui2::localize()->Find( "#Career_Task_PartialIncomplete" ), 0 );
			}
			else
			{
				sprintf( ascii, "%d", numLeft );
				vgui2::localize()->ConvertANSIToUnicode( ascii, tmpBuf, sizeof( tmpBuf ) );
				vgui2::localize()->ConstructString( partialStr, sizeof( partialStr ),
					vgui2::localize()->Find( "#Career_Task_PartialRemaining" ), 1, tmpBuf );
			}
		}
		else if ( task->MustSurvive() && numLeft <= 0 )
		{
			vgui2::localize()->ConstructString( partialStr, sizeof( partialStr ), vgui2::localize()->Find( "#Career_Task_PartialMustLive" ), 0 );
		}
		else
		{
			vgui2::localize()->ConstructString( partialStr, sizeof( partialStr ), vgui2::localize()->Find( "#Career_Task_PartialIncomplete" ), 0 );
		}
	}

	return partialStr;
}

void PopulateTaskLabels( CUtlVector<TaskLabel*>& taskLabel, CUtlVector<TaskLabel*>& statusLabel,
	vgui2::Label* summary, bool showTasksRemaining )
{
	int numTaskLabels = taskLabel.Count();
	for ( int i = 0; i < taskLabel.Count(); ++i )
	{
		taskLabel[i]->SetText( "" );
		statusLabel[i]->SetText( "" );
	}
	for ( int i = 0; i < statusLabel.Count(); ++i )
	{
		taskLabel[i]->SetText( "" );
		statusLabel[i]->SetText( "" );
	}
	if ( summary != NULL )
		summary->SetText( "" );
	if ( g_pCareerUI == NULL || numTaskLabels != statusLabel.Count() )
		return;

	int tasksRemaining = 0;
	wchar_t tmpBuf[128];
	wchar_t ibuf[1024];
	ibuf[0] = 0;

	ITaskVec* tv = g_pCareerUI->GetCurrentTaskVec();
	int taskNum = 0;
	for ( ITaskVecIt it = tv->begin(); it != tv->end(); ++it )
	{
		ICareerTask* task = *it;

		const wchar_t* unicodeTaskName = task->GetLocalizedTaskName();
		if ( unicodeTaskName != NULL )
		{
			if ( !task->IsComplete() )
				++tasksRemaining;

			if ( taskNum >= numTaskLabels )
				continue;

			taskLabel[taskNum]->SetTaskStatus( task->IsComplete() );
			taskLabel[taskNum]->SetText( unicodeTaskName );
			statusLabel[taskNum]->SetTaskStatus( task->IsComplete() );
			statusLabel[taskNum]->SetText( GetTaskStatusString( task ) );
			++taskNum;
		}
	}

	if ( summary != NULL )
	{
		if ( tasksRemaining )
		{
			if ( showTasksRemaining )
			{
				const char* lookup = ( tasksRemaining == 1 ) ? "#Career_TaskStatusUnfinishedSingle" : "#Career_TaskStatusUnfinished";
				char ascii[64];
				_snprintf( ascii, sizeof( ascii ), "%d", tasksRemaining );
				vgui2::localize()->ConvertANSIToUnicode( ascii, tmpBuf, sizeof( tmpBuf ) );
				vgui2::localize()->ConstructString( ibuf, sizeof( ibuf ), vgui2::localize()->Find( lookup ), 1, tmpBuf );
				summary->SetText( ibuf );
			}
		}
		else
		{
			summary->SetText( vgui2::localize()->Find( "#Career_TaskStatusDone" ) );
		}
	}
}

void PopulateWinsAndLosses( bool playingCT, vgui2::Label* wins, vgui2::Label* losses )
{
	wchar_t string1[1024];
	wchar_t wInt[2][6];
	if ( playingCT )
	{
		_snwprintf( wInt[0], 6, L"%d", TheCareerMatchStatus.ctWins );
		wInt[0][5] = 0;
		vgui2::localize()->ConstructString( string1, 1024, vgui2::localize()->Find( "#Career_RoundWinsCT" ), 1, wInt[0] );
		wins->SetText( string1 );

		_snwprintf( wInt[0], 6, L"%d", TheCareerMatchStatus.tWins );
		wInt[0][5] = 0;
		vgui2::localize()->ConstructString( string1, 1024, vgui2::localize()->Find( "#Career_RoundWinsT" ), 1, wInt[0] );
		losses->SetText( string1 );
	}
	else
	{
		_snwprintf( wInt[0], 6, L"%d", TheCareerMatchStatus.ctWins );
		wInt[0][5] = 0;
		vgui2::localize()->ConstructString( string1, 1024, vgui2::localize()->Find( "#Career_RoundWinsCT" ), 1, wInt[0] );
		losses->SetText( string1 );

		_snwprintf( wInt[0], 6, L"%d", TheCareerMatchStatus.tWins );
		wInt[0][5] = 0;
		vgui2::localize()->ConstructString( string1, 1024, vgui2::localize()->Find( "#Career_RoundWinsT" ), 1, wInt[0] );
		wins->SetText( string1 );
	}
}

#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
CCSCareerRoundEndMenu::CCSCareerRoundEndMenu( vgui2::Panel* parent )
	: CCSCareerBaseMenu( parent, "CSCareerRoundEnd", false )
{
	vgui2::Frame::SetTitle( "#Career_Round_End_Menu", true );
	m_pWinsLabel = new vgui2::Label( this, "Wins", "" );
	m_pLossesLabel = new vgui2::Label( this, "Losses", "" );
	m_pNeededLabel = new vgui2::Label( this, "Needed", "" );
	m_pRoundResultLabel = new vgui2::Label( this, "RoundResult", "" );
	m_pImminentVictoryLabel = new vgui2::Label( this, "ImminentVictory", "" );
	m_pTaskNoticeLabel = new vgui2::Label( this, "TaskNotice", "" );
	m_pContinueButton = new vgui2::Button( this, "ok", "" );
	m_pSurrenderButton = new vgui2::Button( this, "surrender", "" );
	LoadControlSettings( "Resource/UI/CSCareerRoundEnd.res", NULL );
	int taskNum = 0;
	TaskLabel* task = dynamic_cast<TaskLabel*>( FindChildByName( SharedVarArgs( (char*)"Task%d", taskNum ), true ) );
	TaskLabel* status = dynamic_cast<TaskLabel*>( FindChildByName( SharedVarArgs( (char*)"TaskStatus%d", taskNum ), true ) );
	while ( task != NULL && status != NULL )
	{
		m_pTaskLabels.AddToTail( task );
		m_pTaskStatusLabels.AddToTail( status );
		++taskNum;
		task = dynamic_cast<TaskLabel*>( FindChildByName( SharedVarArgs( (char*)"Task%d", taskNum ), true ) );
		status = dynamic_cast<TaskLabel*>( FindChildByName( SharedVarArgs( (char*)"TaskStatus%d", taskNum ), true ) );
	}
}

CCSCareerRoundEndMenu::~CCSCareerRoundEndMenu()
{
}

vgui2::Panel* CCSCareerRoundEndMenu::CreateControlByName( const char* name )
{
	if ( !_stricmp( name, "TaskLabel" ) )
		return new TaskLabel( this, NULL, "" );
	return vgui2::EditablePanel::CreateControlByName( name );
}

void CCSCareerRoundEndMenu::Update()
{
	IN_AttackUp();
	IN_ScoreUp();
	gEngfuncs.pfnClientCmd( (char*)"-score; -attack; -attack2; -voicerecord\n" );
	if ( g_pCareerUI == NULL || !g_pCareerUI->IsPlayingMatch() )
	{
		m_pSurrenderButton->SetVisible( false );
	}
	else
	{
		m_pSurrenderButton->SetVisible( true );
	}

	bool playingCT = true;
	if ( g_pCareerUI != NULL )
		playingCT = g_pCareerUI->PlayAsCT();

	PopulateWinsAndLosses( playingCT, m_pWinsLabel, m_pLossesLabel );
	wchar_t string1[1024];
	wchar_t wInt[2][6];
	_snwprintf( wInt[0], 6, L"%d", TheCareerMatchStatus.neededWins );
	_snwprintf( wInt[1], 6, L"%d", TheCareerMatchStatus.winBy );
	vgui2::localize()->ConstructString( string1, 2048, vgui2::localize()->Find( "#Career_MatchTasks" ), 2, wInt[0], wInt[1] );
	m_pNeededLabel->SetText( string1 );
	switch ( TheCareerMatchStatus.roundResult )
	{
	case 1:
		if ( playingCT )
		{
			m_pRoundResultLabel->SetText( "#Career_RoundWin" );
			OnRoundEndMenuOpen( true );
		}
		else
		{
			m_pRoundResultLabel->SetText( "#Career_RoundLose" );
			OnRoundEndMenuOpen( false );
		}
		break;

	case 2:
		if ( playingCT )
		{
			m_pRoundResultLabel->SetText( "#Career_RoundLose" );
			OnRoundEndMenuOpen( false );
		}
		else
		{
			m_pRoundResultLabel->SetText( "#Career_RoundWin" );
			OnRoundEndMenuOpen( true );
		}
		break;

	default:
		m_pRoundResultLabel->SetText( "#Career_RoundDraw" );
		break;
	}
	PopulateTaskLabels( m_pTaskLabels, m_pTaskStatusLabels, m_pTaskNoticeLabel, true );
	int wins = playingCT ? TheCareerMatchStatus.ctWins : TheCareerMatchStatus.tWins;
	int losses = playingCT ? TheCareerMatchStatus.tWins : TheCareerMatchStatus.ctWins;
	if ( wins + 1 >= TheCareerMatchStatus.neededWins && wins - losses >= TheCareerMatchStatus.winBy - 1 )
		m_pImminentVictoryLabel->SetText( "#Career_ImminentVictory" );
	else if ( losses + 1 >= TheCareerMatchStatus.neededWins && losses - wins >= TheCareerMatchStatus.winBy - 1 )
		m_pImminentVictoryLabel->SetText( "#Career_ImminentDefeat" );
	else
		m_pImminentVictoryLabel->SetText( L"" );
	m_pContinueButton->RequestFocus( 0 );
	gEngfuncs.pfnClientCmd( (char*)"setpause\n" );
	gViewPortInterface->CloseTutorTextWindow();
}

void CCSCareerRoundEndMenu::OnKeyCodePressed( vgui2::KeyCode code )
{
	if ( code != vgui2::KEY_ESCAPE )
		CCSCareerBaseMenu::OnKeyCodePressed( code );
}

void CCSCareerRoundEndMenu::OnKeyCodeTyped( vgui2::KeyCode code )
{
	if ( code != vgui2::KEY_ESCAPE )
		CCSCareerBaseMenu::OnKeyCodeTyped( code );
}

void CCSCareerRoundEndMenu::OnCommand( const char* command )
{
	if ( !_stricmp( command, "Close" ) || !_stricmp( command, "Cancel" ) )
		return;

	if ( !_stricmp( command, "disconnect" ) )
	{
		CCareerQueryBox* box = new CCareerQueryBox( "#Career_Surrender", "#GameUI_QuitConfirmationText", "CareerBoxSurrender", this );
		box->AddActionSignalTarget( this );
		box->DoModal();
	}
	else
	{
		if ( !_stricmp( command, "surrender" ) )
			command = "disconnect";
		gEngfuncs.pfnClientCmd( (char*)"unpause\n" );
		if ( !_stricmp( command, "vguicancel" ) )
			gViewPortInterface->HideBackGround();
		else
			gEngfuncs.pfnClientCmd( (char*)command );
		OnClose();
		vgui2::Frame::OnCommand( command );
		OnRoundEndMenuClose( _stricmp( command, "disconnect" ) != 0 );
	}
}
