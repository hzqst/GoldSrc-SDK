//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "clientmotd.h"
#include "HTML.h"
#include "../../vgui2/controls/Label.h"
#include "../../vgui2/controls/RichText.h"
#include "../controls/controls.h"
#include "../../engine/controls.h"
#include "../../public/FileSystem.h"
#include "iviewport.h"
#include "igameuifuncs.h"
#include "../../engine/ILocalize.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <ctype.h>
#include <string>

extern IGameUIFuncs* gameuifuncs; // for key binding details

// UTF-8 needs at most four bytes for each wide character
static const size_t MaxMultiByteCharLength = 4;

class ClientMOTDHTML : public vgui2::HTML
{
public:
	ClientMOTDHTML( CClientMOTD* parent, const char* name ) : vgui2::HTML( parent, name, false, false ), m_pMOTD( parent )
	{
	}

	bool OnStartRequest( const char* url, const char*, const char*, bool ) override
	{
		return _strnicmp( "steam://", url, 8 ) != 0;
	}

	void OnFinishRequest( const char* url, const char* pageTitle ) override
	{
		vgui2::HTML::OnFinishRequest( url, pageTitle );
		m_pMOTD->OnPageLoaded();
	}

private:
	CClientMOTD* m_pMOTD;
};

// Elements whose content is not text of the page
static bool IsHiddenElement( const std::string& tag )
{
	return tag == "head" || tag == "style" || tag == "script" || tag == "title";
}

// Elements that start a new line
static bool IsLineBreakElement( const std::string& tag )
{
	return tag == "br" || tag == "p" || tag == "/p" || tag == "div" || tag == "/div" || tag == "li" || tag == "tr"
		|| tag == "/tr" || tag == "hr" || ( tag.size() == 2 && tag[0] == 'h' && isdigit( (unsigned char)tag[1] ) )
		|| ( tag.size() == 3 && tag[0] == '/' && tag[1] == 'h' && isdigit( (unsigned char)tag[2] ) );
}

// Reads the entity at text (after the '&'), appends its character and returns its length, or 0 if it is not one
static size_t AppendEntity( const char* text, std::string& out )
{
	static const struct
	{
		const char* name;
		char character;
	} Entities[] = { { "amp;", '&' }, { "lt;", '<' }, { "gt;", '>' }, { "quot;", '"' }, { "#39;", '\'' },
		{ "apos;", '\'' }, { "nbsp;", ' ' } };
	for ( const auto& entity : Entities )
	{
		const size_t length = strlen( entity.name );
		if ( !_strnicmp( text, entity.name, length ) )
		{
			out += entity.character;
			return length;
		}
	}
	return 0;
}

// The text of an HTML message, with line breaks where the page would break lines; plain text is kept as it is
static std::string MOTDPlainText( const char* message )
{
	if ( !strchr( message, '<' ) )
		return message;

	std::string out;
	bool pendingSpace = false;
	const char* text = message;
	while ( *text )
	{
		if ( *text == '<' )
		{
			const char* end = strchr( text, '>' );
			if ( !end )
				break;
			std::string tag;
			for ( const char* scan = text + 1; scan < end && !isspace( (unsigned char)*scan ) && *scan != '>'; scan++ )
				tag += (char)tolower( (unsigned char)*scan );
			if ( !tag.empty() && tag.back() == '/' )
				tag.pop_back();
			text = end + 1;
			if ( IsHiddenElement( tag ) )
			{
				// skip to the closing tag
				const std::string closing = "</" + tag;
				const char* scan = text;
				while ( *scan && _strnicmp( scan, closing.c_str(), closing.size() ) )
					scan++;
				const char* close = *scan ? strchr( scan, '>' ) : NULL;
				text = close ? close + 1 : scan;
			}
			else if ( IsLineBreakElement( tag ) )
			{
				if ( !out.empty() && out.back() != '\n' )
					out += '\n';
				pendingSpace = false;
			}
			continue;
		}
		if ( isspace( (unsigned char)*text ) )
		{
			// runs of white space are one space, and none at the start of a line
			pendingSpace = !out.empty() && out.back() != '\n';
			text++;
			continue;
		}
		if ( pendingSpace )
		{
			out += ' ';
			pendingSpace = false;
		}
		if ( *text == '&' )
		{
			const size_t length = AppendEntity( text + 1, out );
			if ( length )
			{
				text += length + 1;
				continue;
			}
		}
		out += *text++;
	}
	return out;
}

CClientMOTD::CClientMOTD( vgui2::Panel* parent ) : vgui2::Frame( parent, "ClientMOTD", true, false )
{
	strcpy( m_szTempFileName, "motd_temp.html" );
	m_bFileWritten = false;
	m_iScoreBoardKey = 0;

	// initialize dialog
	SetTitle( "", true );

	// load the new scheme early!!
	SetScheme( "ClientScheme" );
	SetMoveable( false );
	SetSizeable( false );
	SetProportional( true );
	m_pMessage = new ClientMOTDHTML( this, "Message" );
	LoadControlSettings( "Resource/UI/MOTD.res", NULL );
	// created after the HTML control so that it draws over it; PerformLayout gives it the HTML control's bounds
	m_pTextMessage = new vgui2::RichText( this, "TextMessage", false ); // the server sends the text
	m_pTextMessage->SetVerticalScrollbar( true );
	// the frame handles the keys: RichText keeps the space and scoreboard keys from reaching OnKeyCodeTyped
	m_pTextMessage->SetKeyBoardInputEnabled( false );
	m_pTextMessage->SetVisible( false );
	InvalidateLayout( false, false );
	m_pServerName = new vgui2::Label( this, "serverName", "" );
}

void CClientMOTD::PerformLayout()
{
	vgui2::Frame::PerformLayout();
	int x, y, wide, tall;
	m_pMessage->GetBounds( x, y, wide, tall );
	m_pTextMessage->SetBounds( x, y, wide, tall );
}

void CClientMOTD::OnPageLoaded()
{
	m_pTextMessage->SetVisible( false );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CClientMOTD::~CClientMOTD()
{
	if ( vgui2::filesystem()->FileExists( m_szTempFileName ) )
		vgui2::filesystem()->RemoveFile( m_szTempFileName, "GAMECONFIG" );
}

bool CClientMOTD::IsURL( const char* text )
{
	static const char HttpScheme[] = "http://";
	static const char HttpsScheme[] = "https://";
	const size_t length = strlen( text );
	return ( length > sizeof( HttpScheme ) - 1 && !_strnicmp( text, HttpScheme, sizeof( HttpScheme ) - 1 ) )
		|| ( length > sizeof( HttpsScheme ) - 1 && !_strnicmp( text, HttpsScheme, sizeof( HttpsScheme ) - 1 ) );
}

void CClientMOTD::Activate( const char* title, const char* message )
{
	vgui2::Frame::Activate();
	SetTitle( title, false );
	SetControlString( "serverName", title );
	if ( !ShowMessage( message ) )
		return;
	if ( !m_iScoreBoardKey )
		m_iScoreBoardKey = gameuifuncs->GetVGUI2KeyCodeForBind( "showscores" );
}

void CClientMOTD::Activate( const wchar_t* title, const wchar_t* message )
{
	vgui2::Frame::Activate();
	SetTitle( title, false );
	m_pServerName->SetText( title );

	// convert the whole message so local pages are written as text, not raw wide characters
	const size_t capacity = wcslen( message ) * MaxMultiByteCharLength + 1;
	if ( capacity > INT_MAX )
		return;
	char* text = (char*)malloc( capacity );
	if ( !text )
		return;
	vgui2::localize()->ConvertUnicodeToANSI( message, text, (int)capacity );
	text[capacity - 1] = 0;
	const bool shown = ShowMessage( text );
	free( text );
	if ( shown )
		SetVisible( true );
}

//-----------------------------------------------------------------------------
// Purpose: opens a URL directly, or writes the message to a local page and opens that
//-----------------------------------------------------------------------------
bool CClientMOTD::ShowMessage( const char* message )
{
	// shown until the HTML control loads the page, which it never does without an HTML renderer
	m_pTextMessage->SetText( MOTDPlainText( message ).c_str() );
	m_pTextMessage->GotoTextStart();
	m_pTextMessage->SetVisible( true );

	if ( IsURL( message ) )
	{
		m_pMessage->OpenURL( message, NULL, false );
		return true;
	}

	if ( vgui2::filesystem()->FileExists( m_szTempFileName ) )
		vgui2::filesystem()->RemoveFile( m_szTempFileName, "GAMECONFIG" );
	if ( strstr( message, "img src=\"view-source:" ) || strstr( message, "<style>;@/*" ) )
	{
		m_pTextMessage->SetVisible( false );
		return false;
	}

	FileHandle_t file = vgui2::filesystem()->Open( m_szTempFileName, "w+", "GAMECONFIG" );
	if ( file )
	{
		vgui2::filesystem()->Write( message, (int)strlen( message ), file );
		vgui2::filesystem()->Close( file );
		char localURL[4103] = "file:///";
		size_t length = strlen( localURL );
		vgui2::filesystem()->GetLocalPath( m_szTempFileName, localURL + length,
			(int)( sizeof( localURL ) - length ) );
		m_pMessage->OpenURL( localURL, NULL, false );
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: called when a button is pressed
//-----------------------------------------------------------------------------
void CClientMOTD::OnCommand( const char* command )
{
	if ( !_stricmp( command, "okay" ) )
	{
		if ( vgui2::filesystem()->FileExists( m_szTempFileName ) )
			vgui2::filesystem()->RemoveFile( m_szTempFileName, "GAMECONFIG" );
		Close();
	}
	vgui2::Frame::OnCommand( command );
}

void CClientMOTD::OnKeyCodeTyped( vgui2::KeyCode key )
{
	if ( key == vgui2::KEY_SPACE || key == vgui2::KEY_PAD_ENTER )
		OnCommand( "okay" );
	else if ( m_iScoreBoardKey && m_iScoreBoardKey == key )
	{
		if ( !gViewPortInterface->IsScoreBoardVisible() )
		{
			gViewPortInterface->HideBackGround();
			gViewPortInterface->ShowScoreBoard();
			SetVisible( false );
		}
	}
	else
		vgui2::Frame::OnKeyCodeTyped( key );
}

void CClientMOTD::SetLabelText( const char* name, const wchar_t* text )
{
	vgui2::Label* label = dynamic_cast<vgui2::Label*>( FindChildByName( name, false ) );
	if ( label )
		label->SetText( text );
}
