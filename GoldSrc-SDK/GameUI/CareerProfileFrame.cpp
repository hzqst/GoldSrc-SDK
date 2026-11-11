#include "CareerProfileFrame.h"
#include "CareerProfileButton.h"
#include "CareerBox.h"
#include "CareerDefinitions.h"
#include "CareerGame.h"
#include "DottedLabel.h"
#include "GameUI_Interface.h"
#include "Taskbar.h"

#include "ComboBox.h"
#include "Label.h"
#include "URLLabel.h"
#include "vgui/ILocalize.h"
#include "vgui/IScheme.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"
#include "../vgui2/controls/BitmapImagePanel.h"
#include "../game_shared/shared_util.h"
#include "wrect.h"
#include "cl_dll.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#ifdef _WIN32
#define CAREER_TEXTURE_PATH_MAX	_MAX_PATH
#else
#include <strings.h>
#define stricmp				strcasecmp
#define CAREER_TEXTURE_PATH_MAX	PATH_MAX
#endif

class MissionPackComboBox : public vgui2::ComboBox
{
public:
	MissionPackComboBox( CCareerProfileFrame* parent, const char* name, int numLines, bool allowEdit )
		: vgui2::ComboBox( parent, name, numLines, allowEdit ), m_pParent( parent )
	{
	}
	void OnMenuItemSelected() override
	{
		vgui2::ComboBox::OnMenuItemSelected();
		const char* name = GetActiveItemUserData()->GetString( "MissionPackName", "" );
		CareerDefinition* active = TheCareerDefinitions->GetActiveCareerDefinition();
		if ( !name || !active || strcmp( name, active->GetCareerName() ) )
			m_pParent->SelectMissionPack( name );
		m_pParent->QueueFocusUpdate( 5 );
	}

private:
	CCareerProfileFrame* m_pParent;
};

bool HandleDifficulty( CareerDifficultyType difficulty )
{
	CCareerGame::Create();
	TheCareerGame->Reset();
	bool loaded = TheCareerGame->LoadProfile( difficulty );
	if ( loaded )
		g_pTaskbar->OnOpenResumeCareerGameDialog( difficulty );
	return loaded;
}

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 26495 )
#endif
CCareerProfileFrame::CCareerProfileFrame( vgui2::Panel* parent )
	: CCareerBaseFrame( parent, "CareerProfileFrame", false )
{
	SetHDProportional( vgui2::Panel::HD_PROPORTIONAL_FORCE_DISABLED );
	CCareerGame::Create();
	SetScheme( vgui2::scheme()->GetScheme( "CareerScheme" ) );
	SetCloseSound( "sound/UI/launch_dnmenu1.wav" );
	g_pTaskbar->AddTask( GetVPanel() );
	SetTitle( "#GameUI_NewGame", true );
	m_pProfileSelectionBackground = new CDottedBgLabel( this, "ProfileSelectionBackground", "", 0, 0, 0 );
	m_pProfileSelectionBackground->SetTextInset( 2, 2 );
	m_pProfileList = new CCareerPanelListPanel( this, "ProfileListPanel", true );
	new vgui2::CBitmapImagePanel( this, "TitlePanel", "gfx/career/Logo_CSCZ_Big" );
	new vgui2::CBitmapImagePanel( this, "SelectProfilePanel", "gfx/career/hed_selectprofile" );
	vgui2::Button* cancel = new CCareerButton( this, "Cancel", "#Career_Cancel", "gfx/career/icon_cancel", false );
	if ( m_buttons.FindElement( cancel ) < 0 )
		m_buttons.AddElement( cancel );
	m_pNewButton = new CCareerButton( this, "NewProfile", "#Career_New", "", false );
	if ( m_buttons.FindElement( m_pNewButton ) < 0 )
		m_buttons.AddElement( m_pNewButton );
	m_pResumeButton = new CCareerButton( this, "ResumeProfile", "#Career_Resume", "", false );
	if ( m_buttons.FindElement( m_pResumeButton ) < 0 )
		m_buttons.AddElement( m_pResumeButton );
	m_pDeleteButton = new CCareerButton( this, "DeleteProfile", "#Career_Delete", "", false );
	if ( m_buttons.FindElement( m_pDeleteButton ) < 0 )
		m_buttons.AddElement( m_pDeleteButton );
	m_pMissionPackBackground = new CDottedBgLabel( this, "MissionPackSelectBackground", "", 0, 0, 0 );
	m_pMissionPackBackground->SetTextInset( 2, 2 );
	m_pMissionPackAuthor = new vgui2::Label( this, "LabelMissionPackAuthor", "" );
	m_pMissionPackDescription = new vgui2::Label( this, "LabelMissionPackDescription", "" );
	m_pMissionPackURL = new vgui2::URLLabel( this, "LabelMissionPackURL", "", "" );
	m_pMissionPackLogo = new vgui2::CBitmapImagePanel( this, "ImagePanelMissionPackLogo", "" );
	m_pMissionPackComboBox = new MissionPackComboBox( this, "ComboBoxMissionPack",
		TheCareerDefinitions->GetNumCareers(), false );
	LoadControlSettings( "resource/CareerProfileFrame.res", NULL );
	m_pPlayerName->SetVisible( false );
	m_pMissionPackName->SetVisible( false );
}
#ifdef _MSC_VER
#pragma warning( pop )
#endif

CCareerProfileFrame::~CCareerProfileFrame()
{
}

void CCareerProfileFrame::PerformLayout()
{
	CCareerBaseFrame::PerformLayout();
	PopulateMissionPacks();
}

void CCareerProfileFrame::Activate()
{
	CCareerBaseFrame::Activate();
	m_pPlayerName->SetVisible( false );
	m_pMissionPackName->SetVisible( false );
	for ( int i = 0; i < MAX_CAREER_DIFFICULTY; i++ )
	{
		char filename[64];
#ifdef _WIN32
		_snprintf( filename, sizeof( filename ), "gfx/career/medals/Medal.0%d_Inactive", i + 1 );
#else
		snprintf( filename, sizeof( filename ), "gfx/career/medals/Medal.0%d_Inactive", i + 1 );
#endif
		vgui2::scheme()->GetImage( filename, false );
#ifdef _WIN32
		_snprintf( filename, sizeof( filename ), "gfx/career/medals/Medal.0%d_Active", i + 1 );
#else
		snprintf( filename, sizeof( filename ), "gfx/career/medals/Medal.0%d_Active", i + 1 );
#endif
		vgui2::scheme()->GetImage( filename, false );
	}
	PopulateMissionPacks();
	int count = PopulateProfiles();
	RestoreFocus();
	m_needFocus = 0;
	if ( !count )
		OnCommand( "NewProfile" );
}

void CCareerProfileFrame::RestoreFocus()
{
	if ( m_pResumeButton->IsEnabled() )
		m_pResumeButton->RequestFocus( 0 );
	else
		m_pNewButton->RequestFocus( 0 );
}

void CCareerProfileFrame::OnThink()
{
	CCareerBaseFrame::OnThink();
	if ( m_needFocus )
	{
		m_needFocus--;
		if ( !m_needFocus )
			RestoreFocus();
	}
}

int CCareerProfileFrame::PopulateProfiles()
{
	m_pProfileList->DeleteAllItems();
	m_profiles.RemoveAll();
	m_name.clear();
	ProfileVector profiles = TheCareerGame->BuildProfileList();
	int latestTime = 0;
	for ( ProfileVector::iterator it = profiles.begin(); it != profiles.end(); ++it )
	{
		CCareerProfileData* profile = *it;
		std::vector<wchar_t> profileName( profile->name.size() + 1, L'\0' );
		vgui2::localize()->ConvertANSIToUnicode( profile->name.c_str(), profileName.data(),
			(int)( profileName.size() * sizeof( wchar_t ) ) );
		CProfileButton* button = new CProfileButton( this, "ProfileButton", profileName.data(), profile->medals );
		button->SetProfileName( profile->name.c_str() );
		button->SetCommand( ( "SelectProfileAndFocus " + profile->name ).c_str() );
		button->AddActionSignalTarget( this );
		m_pProfileList->AddItem( button );
		m_profiles.AddElement( button );
		if ( profile->filetime >= latestTime )
		{
			m_name = profile->name;
			latestTime = profile->filetime;
		}
		delete profile;
	}
	UpdateSelectedProfile( m_name.c_str() );
	m_pResumeButton->SetEnabled( !profiles.empty() );
	m_pDeleteButton->SetEnabled( !profiles.empty() );
	m_pProfileList->SetSliderYOffset( 2 );
	return (int)profiles.size();
}

void CCareerProfileFrame::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	CCareerBaseFrame::ApplySchemeSettings( scheme );
	m_pProfileSelectionBackground->SetBgColor( scheme->GetColor( "ControlDarkBG", Color( 0, 0, 0, 0 ) ) );
	m_pMissionPackBackground->SetBgColor( scheme->GetColor( "ControlDarkBG", Color( 0, 0, 0, 0 ) ) );
	m_pMissionPackURL->SetBgColor( Color( 0, 0, 0, 0 ) );
	m_pMissionPackURL->SetBorder( NULL );
	PopulateMissionPacks();
	m_pMissionPackURL->SetFgColor( scheme->GetColor( "MissionPackURL", Color( 255, 255, 255, 255 ) ) );
	m_pMissionPackLogo->SetBorder( scheme->GetBorder( "BrowserBorder" ) );
	SetBgColor( Color( 0, 0, 0, 0 ) );
}

void CCareerProfileFrame::ChooseDifficulty()
{
	CCareerDifficultyBox* box = new CCareerDifficultyBox( "CareerBoxDifficulty", this );
	box->AddActionSignalTarget( this );
	box->ShowWindow();
}

void CCareerProfileFrame::UpdateSelectedProfile( const char* profile )
{
	m_name = profile;
	for ( int i = 0; i < m_profiles.GetCount(); i++ )
		m_profiles[i]->SetActive( m_name == m_profiles[i]->GetProfileName() );
}

void CCareerProfileFrame::PopulateMissionPacks()
{
	CareerDefinition* active = TheCareerDefinitions->GetActiveCareerDefinition();
	if ( !active )
		return;
	m_pMissionPackComboBox->DeleteAllItems();
	KeyValues* data = new KeyValues( "UserData" );
	int activeRow = 0;
	for ( int i = 0; i < TheCareerDefinitions->GetNumCareers(); i++ )
	{
		CareerDefinition* career = TheCareerDefinitions->GetCareer( i );
		data->SetString( "MissionPackName", career->GetCareerName() );
		wchar_t title[128] = {};
		int length = 128;
#ifdef _WIN32
		BufWPrintf( title, length, L"  %s", career->GetTitle() );
#else
		BufWPrintf( title, length, L"  %S", career->GetTitle() );
#endif
		m_pMissionPackComboBox->AddItem( title, data );
		if ( career == active )
			activeRow = i;
	}
	data->deleteThis();
	m_pMissionPackComboBox->ActivateItemByRow( activeRow );
	wchar_t author[256] = {};
	const wchar_t* format = vgui2::localize()->Find( "#Career_MissionPackAuthor" );
	if ( !format )
		format = L"%s1";
	vgui2::localize()->ConstructString( author, sizeof( author ), (wchar_t*)format, 1, active->GetAuthor() );
	m_pMissionPackAuthor->SetText( author );
	m_pMissionPackDescription->SetText( active->GetDescription() );
	m_pMissionPackURL->SetText( active->GetURL() );
	m_pMissionPackURL->SetURL( active->GetURL() );
	char filename[CAREER_TEXTURE_PATH_MAX];
#ifdef _WIN32
	_snprintf( filename, sizeof( filename ), "MissionPacks/%s/thumbnail", active->GetCareerName() );
#else
	snprintf( filename, sizeof( filename ), "MissionPacks/%s/thumbnail", active->GetCareerName() );
#endif
	m_pMissionPackLogo->setTexture( filename, true );
	m_bgFade.SetFade( active->GetBgStartColor(), active->GetBgEndColor(), 25.0f, ColorFade::down );
}

void CCareerProfileFrame::SelectMissionPack( const char* missionPackName )
{
	TheCareerDefinitions->SetActiveCareerDefinition( missionPackName );
	PopulateMissionPacks();
	PopulateProfiles();
}

void CCareerProfileFrame::OnCommand( const char* command )
{
	if ( !stricmp( command, "NewProfile" ) )
	{
		CCareerNewProfileBox* box = new CCareerNewProfileBox( "CareerBoxNewProfile", this );
		box->AddActionSignalTarget( this );
		box->ShowWindow();
	}
	else if ( !stricmp( command, "CreateProfile" ) )
	{
		ProfileVector profiles = TheCareerGame->BuildProfileList();
		bool exists = false;
		for ( ProfileVector::iterator it = profiles.begin(); it != profiles.end(); ++it )
		{
			if ( ( *it )->name == engine->pfnGetCvarString( (char*)"name" ) )
				exists = true;
			delete *it;
		}
		if ( !exists )
		{
			if ( !TheCareerGame->CreateBlankProfile() )
				return;
			PopulateProfiles();
			UpdateSelectedProfile( engine->pfnGetCvarString( (char*)"name" ) );
			ChooseDifficulty();
		}
		else
		{
			CCareerQueryBox* box = new CCareerQueryBox( "#Career_OverwriteConfirmationTitle",
				"#Career_OverwriteConfirmationText", "CareerBoxOverwrite", this );
			box->SetCancelButtonAsDefault();
			box->AddActionSignalTarget( this );
			box->ShowWindow();
		}
	}
	else if ( !stricmp( command, "ReallyCreateProfile" ) )
	{
		if ( !TheCareerGame->CreateBlankProfile() )
			return;
		PopulateProfiles();
		UpdateSelectedProfile( engine->pfnGetCvarString( (char*)"name" ) );
		ChooseDifficulty();
	}
#ifdef _WIN32
	else if ( !strncmp( command, "SelectProfile ", 14 ) && strlen( command ) > 14 )
#else
	else if ( !memcmp( command, "SelectProfile ", 14 ) && strlen( command ) > 14 )
#endif
	{
		UpdateSelectedProfile( command + 14 );
	}
#ifdef _WIN32
	else if ( !strncmp( command, "SelectProfileAndFocus ", 22 ) && strlen( command ) > 22 )
#else
	else if ( !memcmp( command, "SelectProfileAndFocus ", 22 ) && strlen( command ) > 22 )
#endif
	{
		UpdateSelectedProfile( command + 22 );
		RestoreFocus();
	}
	else if ( !strcmp( command, "ResumeProfile" ) )
	{
		if ( m_name.empty() )
		{
			OnCommand( "NewProfile" );
			return;
		}
		engine->Cvar_Set( (char*)"name", const_cast<char*>( m_name.c_str() ) );
		ChooseDifficulty();
	}
	else if ( !strcmp( command, "DeleteProfile" ) && !m_name.empty() )
	{
		std::vector<wchar_t> name( m_name.size() + 1, L'\0' );
		vgui2::localize()->ConvertANSIToUnicode( m_name.c_str(), name.data(), (int)( name.size() * sizeof( wchar_t ) ) );
		wchar_t* format = vgui2::localize()->Find( "#Career_DeleteProfileText" );
		if ( !format )
			format = const_cast<wchar_t*>( L"Delete %s1?" );
		std::vector<wchar_t> text( wcslen( format ) + name.size(), L'\0' );
		vgui2::localize()->ConstructString( text.data(), (int)( text.size() * sizeof( wchar_t ) ), format, 1, name.data() );
		CCareerQueryBox* box = new CCareerQueryBox( vgui2::localize()->Find( "#Career_DeleteProfileTitle" ),
			text.data(), "CareerBoxDelete", this );
		box->SetCancelButtonAsDefault();
		box->SetLabelText( text.data() );
		box->AddActionSignalTarget( this );
		box->ShowWindow();
	}
	else if ( !strcmp( command, "ReallyDeleteProfile" ) && !m_name.empty() )
	{
		TheCareerGame->DeleteProfile( m_name.c_str() );
		m_name.clear();
		PopulateProfiles();
	}
	else if ( !strcmp( command, "ChooseDifficulty0" ) && !m_name.empty() )
	{
		engine->Cvar_SetValue( (char*)"cl_career_difficulty", 0 );
		if ( HandleDifficulty( CAREER_DIFFICULTY_EASY ) )
			Close();
	}
	else if ( !strcmp( command, "ChooseDifficulty1" ) && !m_name.empty() )
	{
		engine->Cvar_SetValue( (char*)"cl_career_difficulty", 1 );
		if ( HandleDifficulty( CAREER_DIFFICULTY_NORMAL ) )
			Close();
	}
	else if ( !strcmp( command, "ChooseDifficulty2" ) && !m_name.empty() )
	{
		engine->Cvar_SetValue( (char*)"cl_career_difficulty", 2 );
		if ( HandleDifficulty( CAREER_DIFFICULTY_HARD ) )
			Close();
	}
	else if ( !strcmp( command, "ChooseDifficulty3" ) && !m_name.empty() )
	{
		engine->Cvar_SetValue( (char*)"cl_career_difficulty", 3 );
		if ( HandleDifficulty( CAREER_DIFFICULTY_EXPERT ) )
			Close();
	}
	else
	{
		CCareerBaseFrame::OnCommand( command );
	}
}

void CCareerProfileFrame::OnKeyCodeTyped( vgui2::KeyCode code )
{
	if ( code == vgui2::KEY_ESCAPE )
		OnCommand( "close" );
	else if ( code == vgui2::KEY_ENTER )
		CCareerBaseFrame::OnKeyCodeTyped( vgui2::KEY_SPACE );
	else
		CCareerBaseFrame::OnKeyCodeTyped( code );
}
