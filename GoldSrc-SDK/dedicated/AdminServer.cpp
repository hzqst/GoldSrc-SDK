//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "AdminServer.h"

#include "AnimatingImagePanel.h"
#include "Button.h"
#include "CheckButton.h"
#include "ComboBox.h"
#include "ImageList.h"
#include "ImagePanel.h"
#include "Label.h"
#include "ListPanel.h"
#include "MessageBox.h"
#include "PropertySheet.h"
#include "QueryBox.h"
#include "RadioButton.h"
#include "RichText.h"
#include "controls.h"
#include "../engine/utlbuffer.h"
#include "../engine/controls.h"
#include "vgui/Cursor.h"
#include "vgui/IInput.h"
#include "vgui/ILocalize.h"
#include "vgui/IPanel.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"
#include "vgui/KeyValues.h"

#include <direct.h>
#include <io.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

class TokenLine
{
public:
	TokenLine();
	virtual ~TokenLine();

	bool SetLine( const char* newLine );
	int CountToken();
	char* GetToken( int i );

private:
	enum
	{
		MAX_LINE_CHARS = 2048,
		MAX_LINE_TOKENS = 128
	};

	char	m_tokenBuffer[MAX_LINE_CHARS];
	char	m_fullLine[MAX_LINE_CHARS];
	char*	m_token[MAX_LINE_TOKENS];
	int		m_tokenNumber;
};

class FileOpenDialog : public vgui2::Frame
{
	DECLARE_CLASS_SIMPLE_NAMED( FileOpenDialog, vgui2::Frame, "vgui::Frame" );

public:
	FileOpenDialog( vgui2::Panel* parent, const char* title, bool bOpenOnly );
	~FileOpenDialog() override;

	void SetStartDirectory( const char* dir );
	void AddFilter( const char* filter, const char* filterName, bool bActive );
	void DoModal( bool isSaveAsDialog );

protected:
	void ApplySchemeSettings( vgui2::IScheme* pScheme ) override;
	void OnClose() override;
	void OnCommand( const char* command ) override;

	MESSAGE_FUNC( OnItemSelected, "ItemSelected" );
	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", kv );

private:
	void GetDirectory( char* buf, int bufSize );
	void GetSelectedFileName( char* buf, int bufSize );
	void OnOpen();
	void MoveUpFolder();
	void ValidatePath();
	void PopulateFileList();
	void PopulateDriveList();

	vgui2::ComboBox*	m_pFullPathEdit;
	vgui2::ListPanel*	m_pFileList;
	vgui2::TextEntry*	m_pFileNameEdit;
	vgui2::ComboBox*	m_pFileTypeCombo;
	vgui2::Button*		m_pOpenButton;
	vgui2::Button*		m_pCancelButton;
	vgui2::Button*		m_pFolderUpButton;
	char				m_szLastPath[1024];
	bool				m_bOpenOnly;
};

// expose the server browser interfaces
CAdminServer g_AdminServerSingleton;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CAdminServer, IAdminServer, ADMINSERVER_INTERFACE_VERSION, g_AdminServerSingleton );
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CAdminServer, IVGuiModule, "VGuiModuleAdminServer001", g_AdminServerSingleton );

IGameServerData* g_pGameServerData = NULL;

static char s_ServerResponse[65536];

static const Color CPU_COLOR( 0, 255, 0, 255 );
static const Color FPS_COLOR( 255, 0, 0, 255 );
static const Color IN_COLOR( 255, 255, 0, 255 );
static const Color OUT_COLOR( 0, 255, 255, 255 );
static const Color PLAYERS_COLOR( 255, 0, 255, 255 );
static const Color PING_COLOR( 0, 0, 0, 255 );

static void PutBoundedString( CUtlBuffer& buffer, const char* text, int maxLength )
{
	int length = (int)strlen( text );
	if ( length > maxLength )
		length = maxLength;

	if ( length > 0 )
		buffer.Put( text, length );
	buffer.PutChar( 0 );
}

static int ReadResponseInt( const char* data, int size, int& pos )
{
	int value = 0;
	if ( pos + (int)sizeof( int ) <= size )
	{
		memcpy( &value, data + pos, sizeof( int ) );
		pos += (int)sizeof( int );
	}
	else
	{
		pos = size;
	}
	return value;
}

CRemoteServer& RemoteServer()
{
	static CRemoteServer s_RemoteServer;
	return s_RemoteServer;
}

CRemoteServer::CRemoteServer() : m_ResponseHandlers( 0, 0 ), m_MessageHandlers( 0, 0 )
{
	m_iCurrentRequestID = 0;
	m_bInitialized = false;
}

CRemoteServer::~CRemoteServer()
{
}

void CRemoteServer::Initialize()
{
	m_bInitialized = true;
}

void CRemoteServer::WriteRequest( int requestType, const char* variable, int variableLength, const char* value,
	int valueLength )
{
	if ( !g_pGameServerData )
		return;

	CUtlBuffer cmd( 0, 512, false );
	cmd.PutInt( m_iCurrentRequestID++ );
	cmd.PutInt( requestType );
	PutBoundedString( cmd, variable, variableLength );
	PutBoundedString( cmd, value, valueLength );

	g_pGameServerData->WriteDataRequest( cmd.Base(), cmd.TellPut() );
}

void CRemoteServer::RequestValue( IServerDataResponse* requester, const char* variable )
{
	int i = m_ResponseHandlers.AddToTail();
	m_ResponseHandlers[i].requestID = m_iCurrentRequestID;
	m_ResponseHandlers[i].handler = requester;

	WriteRequest( SERVERDATA_REQUESTVALUE, variable, 511, "", 511 );
}

void CRemoteServer::SetValue( const char* variable, const char* value )
{
	WriteRequest( SERVERDATA_SETVALUE, variable, 255, value, 511 );
}

void CRemoteServer::SendCommand( const char* commandString )
{
	WriteRequest( SERVERDATA_EXECCOMMAND, commandString, 511, "", 511 );
}

void CRemoteServer::AddServerMessageHandler( IServerDataResponse* handler, const char* update )
{
	int i = m_MessageHandlers.AddToTail();
	strncpy( m_MessageHandlers[i].messageName, update, sizeof( m_MessageHandlers[i].messageName ) - 1 );
	m_MessageHandlers[i].messageName[sizeof( m_MessageHandlers[i].messageName ) - 1] = 0;
	m_MessageHandlers[i].handler = handler;
}

void CRemoteServer::RemoveServerDataResponseTarget( IServerDataResponse* invalidTarget )
{
	int i = m_ResponseHandlers.Head();
	while ( m_ResponseHandlers.IsValidIndex( i ) )
	{
		int next = m_ResponseHandlers.Next( i );
		if ( m_ResponseHandlers[i].handler == invalidTarget )
			m_ResponseHandlers.Remove( i );
		i = next;
	}

	i = m_MessageHandlers.Head();
	while ( m_MessageHandlers.IsValidIndex( i ) )
	{
		int next = m_MessageHandlers.Next( i );
		if ( m_MessageHandlers[i].handler == invalidTarget )
			m_MessageHandlers.Remove( i );
		i = next;
	}
}

bool CRemoteServer::ProcessServerResponse()
{
	if ( !g_pGameServerData )
		return false;

	bool bProcessedAnyPackets = false;
	for ( ;; )
	{
		int bytesRead = g_pGameServerData->ReadDataResponse( s_ServerResponse, sizeof( s_ServerResponse ) );
		if ( bytesRead < 1 )
			break;

		bProcessedAnyPackets = true;

		int pos = 0;
		int requestID = ReadResponseInt( s_ServerResponse, bytesRead, pos );
		int responseType = ReadResponseInt( s_ServerResponse, bytesRead, pos );

		char variable[64];
		int nameLength = 0;
		while ( pos < bytesRead && s_ServerResponse[pos] )
		{
			if ( nameLength < (int)sizeof( variable ) - 1 )
				variable[nameLength++] = s_ServerResponse[pos];
			pos++;
		}
		variable[nameLength] = 0;
		if ( pos < bytesRead )
			pos++;

		if ( responseType == SERVERDATA_RESPONSE_VALUE )
		{
			int valueSize = ReadResponseInt( s_ServerResponse, bytesRead, pos );
			if ( valueSize > bytesRead - pos )
				valueSize = bytesRead - pos;

			CUtlBuffer value( 0, valueSize > 0 ? valueSize + 1 : 1, false );
			if ( valueSize > 0 )
				value.Put( s_ServerResponse + pos, valueSize );
			value.PutChar( 0 );

			for ( int i = m_ResponseHandlers.Head(); m_ResponseHandlers.IsValidIndex( i ); i = m_ResponseHandlers.Next( i ) )
			{
				if ( m_ResponseHandlers[i].requestID == requestID )
				{
					IServerDataResponse* handler = m_ResponseHandlers[i].handler;
					m_ResponseHandlers.Remove( i );
					handler->OnServerDataResponse( variable, (const char*)value.Base() );
					break;
				}
			}
		}
		else if ( responseType == SERVERDATA_UPDATE )
		{
			int i = m_MessageHandlers.Head();
			while ( m_MessageHandlers.IsValidIndex( i ) )
			{
				int next = m_MessageHandlers.Next( i );
				if ( !_stricmp( m_MessageHandlers[i].messageName, variable ) )
					m_MessageHandlers[i].handler->OnServerDataResponse( variable, "" );
				i = next;
			}
		}
	}

	return bProcessedAnyPackets;
}

TokenLine::TokenLine()
{
	m_tokenBuffer[0] = 0;
	m_fullLine[0] = 0;
	memset( m_token, 0, sizeof( m_token ) );
	m_tokenNumber = 0;
}

TokenLine::~TokenLine()
{
}

bool TokenLine::SetLine( const char* newLine )
{
	m_tokenNumber = 0;

	if ( !newLine || strlen( newLine ) >= MAX_LINE_CHARS - 1 )
	{
		memset( m_fullLine, 0, MAX_LINE_CHARS );
		memset( m_tokenBuffer, 0, MAX_LINE_CHARS );
		return false;
	}

	strncpy( m_fullLine, newLine, MAX_LINE_CHARS - 1 );
	m_fullLine[MAX_LINE_CHARS - 1] = 0;

	strncpy( m_tokenBuffer, newLine, MAX_LINE_CHARS - 1 );
	m_tokenBuffer[MAX_LINE_CHARS - 1] = 0;

	char* charPointer = m_tokenBuffer;
	while ( *charPointer && m_tokenNumber < MAX_LINE_TOKENS )
	{
		while ( *charPointer && ( *charPointer <= ' ' || *charPointer > '~' ) )
			charPointer++;

		if ( !*charPointer )
			break;

		if ( *charPointer == '\"' )
		{
			charPointer++;
			m_token[m_tokenNumber] = charPointer;
			while ( *charPointer && *charPointer != '\"' )
				charPointer++;
		}
		else
		{
			m_token[m_tokenNumber] = charPointer;
			while ( *charPointer && *charPointer > ' ' && *charPointer <= '~' )
				charPointer++;
		}

		m_tokenNumber++;

		if ( !*charPointer )
			break;

		*charPointer = 0;
		charPointer++;
	}

	return m_tokenNumber != MAX_LINE_TOKENS;
}

int TokenLine::CountToken()
{
	int count = 0;
	for ( int i = 0; i < m_tokenNumber; i++ )
	{
		if ( m_token[i] )
			count++;
	}
	return count;
}

char* TokenLine::GetToken( int i )
{
	if ( i >= 0 && i < m_tokenNumber )
		return m_token[i];
	return NULL;
}

static int __cdecl PlayerTimeCompare( vgui2::ListPanel*, const vgui2::ListPanelItem& item1,
	const vgui2::ListPanelItem& item2 )
{
	KeyValues* p1 = item1.kv;
	KeyValues* p2 = item2.kv;

	if ( !p1 || !p2 )  // No meaningful comparison
	{
		return 0;
	}

	int a1 = 0, b1 = 0, c1 = 0, a2 = 0, b2 = 0, c2 = 0;
	int count1 = sscanf( p1->GetString( "time" ), "%i:%i:%i", &a1, &b1, &c1 );
	int count2 = sscanf( p2->GetString( "time" ), "%i:%i:%i", &a2, &b2, &c2 );

	int h1 = 0, m1 = 0, s1 = 0;
	if ( count1 == 1 )
	{
		s1 = a1;
	}
	else if ( count1 == 2 )
	{
		m1 = a1;
		s1 = b1;
	}
	else if ( count1 > 2 )
	{
		h1 = a1;
		m1 = b1;
		s1 = c1;
	}

	int h2 = 0, m2 = 0, s2 = 0;
	if ( count2 == 1 )
	{
		s2 = a2;
	}
	else if ( count2 == 2 )
	{
		m2 = a2;
		s2 = b2;
	}
	else if ( count2 > 2 )
	{
		h2 = a2;
		m2 = b2;
		s2 = c2;
	}

	float t1 = (float)( s1 + 60 * ( m1 + 60 * h1 ) );
	float t2 = (float)( s2 + 60 * ( m2 + 60 * h2 ) );

	if ( t1 < t2 )
		return -1;
	else if ( t1 > t2 )
		return 1;

	return 0;
}

static bool ParseIPAddress( const char* text )
{
	int ip1, ip2, ip3, ip4;
	return sscanf( text, "%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4 ) == 4;
}

static bool IsMultiplayerMap( const char* name )
{
	bool singlePlayer = ( name[0] == 'c' || name[0] == 't' ) && name[1] >= '0' && name[1] <= '5' &&
		name[2] == 'a';
	return !singlePlayer && strncmp( name, "hldemo", 6 );
}

static void RemoveUnlistedMaps( CUtlVector<CUtlSymbol>& maps )
{
	for ( int i = 0; i < maps.Count(); )
	{
		bool keep = IsMultiplayerMap( maps[i].String() );
		for ( int j = 0; keep && j < i; j++ )
		{
			if ( !_stricmp( maps[j].String(), maps[i].String() ) )
				keep = false;
		}

		if ( keep )
			i++;
		else
			maps.Remove( i );
	}
}

static void UseUnscaledLayout( vgui2::QueryBox* box )
{
	box->SetHDProportional( vgui2::Panel::HD_PROPORTIONAL_FORCE_DISABLED );
	box->SetProportional( false );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CPlayerContextMenu::CPlayerContextMenu( vgui2::Panel* parent ) : Menu( parent, "ServerContextMenu" )
{
	CPlayerContextMenu::parent = parent;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CPlayerContextMenu::~CPlayerContextMenu()
{
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBanContextMenu::CBanContextMenu( vgui2::Panel* parent ) : Menu( parent, "BanContextMenu" )
{
	CBanContextMenu::parent = parent;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CBanContextMenu::~CBanContextMenu()
{
}

CVarEditDialog::CVarEditDialog( vgui2::Panel* parent, const char* name ) : Frame( parent, name, true, false )
{
	SetSize( 280, 180 );
	SetSizeable( false );

	m_pOKButton = new vgui2::Button( this, "OKButton", "OK" );
	m_pCancelButton = new vgui2::Button( this, "CancelButton", "Cancel" );
	m_pStringEdit = new vgui2::TextEntry( this, "StringEdit" );
	m_pComboEdit = new vgui2::ComboBox( this, "ComboEdit", 12, false );
	m_pRules = NULL;
}

CVarEditDialog::~CVarEditDialog()
{
	if ( m_pRules )
		m_pRules->deleteThis();
}

void CVarEditDialog::Activate( vgui2::Panel* actionSignalTarget, KeyValues* rules )
{
	AddActionSignalTarget( actionSignalTarget );
	m_pRules = rules->MakeCopy();

	const char* type = m_pRules->GetString( "type" );
	if ( !_stricmp( type, "enumeration" ) )
	{
		LoadControlSettings( "Admin/VarEditDialog_ComboBox.res", "PLATFORM" );
		m_pStringEdit->SetVisible( false );

		for ( KeyValues* kv = m_pRules->FindKey( "list", true )->GetFirstSubKey(); kv != NULL; kv = kv->GetNextKey() )
			m_pComboEdit->AddItem( kv->GetString(), NULL );

		m_pComboEdit->ActivateItemByRow( m_pRules->GetInt( "enum" ) );
	}
	else if ( !_stricmp( type, "customlist" ) )
	{
		LoadControlSettings( "Admin/VarEditDialog_ComboBox.res", "PLATFORM" );
		m_pStringEdit->SetVisible( false );

		const char* currentValue = m_pRules->GetString( "value" );
		const char* parse = m_pRules->GetString( "stringlist" );
		while ( *parse )
		{
			if ( *parse == '\n' )
			{
				parse++;
				continue;
			}

			const char* end = strstr( parse, "\n" );
			if ( !end )
				break;

			int nameSize = (int)( end - parse );
			char customString[64];
			if ( nameSize >= (int)sizeof( customString ) )
				nameSize = (int)sizeof( customString ) - 1;

			strncpy( customString, parse, nameSize );
			customString[nameSize] = 0;
			parse = end;

			int itemID = m_pComboEdit->AddItem( customString, NULL );
			if ( !_stricmp( customString, currentValue ) )
				m_pComboEdit->ActivateItem( itemID );
		}
	}
	else
	{
		LoadControlSettings( "Admin/VarEditDialog_String.res", "PLATFORM" );
		m_pComboEdit->SetVisible( false );
		m_pStringEdit->SelectAllOnFirstFocus( true );
		m_pStringEdit->SetText( m_pRules->GetString( "value" ) );
	}

	char title[256];
	_snprintf( title, sizeof( title ) - 1, "Change %s", m_pRules->GetString( "name" ) );
	title[sizeof( title ) - 1] = 0;
	SetTitle( title, false );

	MoveToCenterOfScreen();
	BaseClass::Activate();
}

void CVarEditDialog::OnCommand( const char* command )
{
	if ( !_stricmp( command, "OK" ) )
	{
		ApplyChanges();
		Close();
	}
	else if ( !_stricmp( command, "Cancel" ) )
	{
		Close();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

void CVarEditDialog::OnClose()
{
	BaseClass::OnClose();
	MarkForDeletion();
}

void CVarEditDialog::ApplyChanges()
{
	const char* type = m_pRules->GetString( "type" );
	if ( !_stricmp( type, "enumeration" ) )
	{
		char value[32];
		_snprintf( value, sizeof( value ) - 1, "%d", m_pComboEdit->GetActiveItem() );
		value[sizeof( value ) - 1] = 0;
		RemoteServer().SetValue( m_pRules->GetName(), value );
	}
	else if ( !_stricmp( type, "customlist" ) )
	{
		char value[512];
		m_pComboEdit->GetText( value, sizeof( value ) );
		RemoteServer().SetValue( m_pRules->GetName(), value );
	}
	else
	{
		char value[512];
		m_pStringEdit->GetText( value, sizeof( value ) );
		RemoteServer().SetValue( m_pRules->GetName(), value );
	}

	PostActionSignal( new KeyValues( "VarChanged", "var", m_pRules->GetName() ) );
}

CMapCycleEditDialog::CMapCycleEditDialog( vgui2::Panel* parent, const char* name ) : Frame( parent, name, true, false )
{
	SetSize( 480, 320 );
	SetSizeable( false );

	m_pAvailableMapList = new vgui2::ListPanel( this, "AvailableMapList" );
	m_pAvailableMapList->AddColumnHeader( 0, "Map", "#Available_Maps", 128, 0 );
	m_pAvailableMapList->SetColumnSortable( 0, false );

	m_pMapCycleList = new vgui2::ListPanel( this, "MapCycleList" );
	m_pMapCycleList->AddColumnHeader( 0, "Map", "#Map_Cycle", 128, 0 );
	m_pMapCycleList->SetColumnSortable( 0, false );

	m_RightArrow = new vgui2::Button( this, "RightButton", "" );
	m_LeftArrow = new vgui2::Button( this, "LeftButton", "" );
	m_UpArrow = new vgui2::Button( this, "UpButton", "" );
	m_DownArrow = new vgui2::Button( this, "DownButton", "" );

	LoadControlSettings( "Admin/MapCycleEditDialog.res", "PLATFORM" );
}

CMapCycleEditDialog::~CMapCycleEditDialog()
{
}

void CMapCycleEditDialog::Activate( vgui2::Panel* updateTarget, CUtlVector<CUtlSymbol>& availableMaps,
	CUtlVector<CUtlSymbol>& mapCycle )
{
	AddActionSignalTarget( updateTarget );
	m_pAvailableMapList->DeleteAllItems();
	m_pMapCycleList->DeleteAllItems();

	for ( int i = 0; i < availableMaps.Count(); i++ )
	{
		bool found = false;
		for ( int j = 0; j < mapCycle.Count(); j++ )
		{
			if ( !_stricmp( mapCycle[j].String(), availableMaps[i].String() ) )
			{
				found = true;
				break;
			}
		}

		if ( !found )
		{
			KeyValues* item = new KeyValues( "MapItem", "Map", availableMaps[i].String() );
			m_pAvailableMapList->AddItem( item, 0, false, false );
			item->deleteThis();
		}
	}

	for ( int i = 0; i < mapCycle.Count(); i++ )
	{
		KeyValues* item = new KeyValues( "MapItem", "Map", mapCycle[i].String() );
		m_pMapCycleList->AddItem( item, 0, false, false );
		item->deleteThis();
	}

	SetTitle( "Change Map Cycle", false );
	MoveToCenterOfScreen();
	BaseClass::Activate();
}

void CMapCycleEditDialog::PerformLayout()
{
	m_LeftArrow->SetEnabled( false );
	m_RightArrow->SetEnabled( false );
	m_UpArrow->SetEnabled( false );
	m_DownArrow->SetEnabled( false );

	if ( m_pMapCycleList->GetSelectedItemsCount() > 0 )
	{
		m_LeftArrow->SetEnabled( true );
		m_LeftArrow->SetAsDefaultButton( 1 );

		if ( m_pMapCycleList->GetSelectedItemsCount() == 1 )
		{
			int row = m_pMapCycleList->GetSelectedItem( 0 );
			if ( row > 0 )
				m_UpArrow->SetEnabled( true );
			if ( row + 1 < m_pMapCycleList->GetItemCount() )
				m_DownArrow->SetEnabled( true );
		}
	}
	else if ( m_pAvailableMapList->GetSelectedItemsCount() > 0 )
	{
		m_RightArrow->SetEnabled( true );
		m_RightArrow->SetAsDefaultButton( 1 );
	}

	BaseClass::PerformLayout();
}

void CMapCycleEditDialog::OnItemSelected( vgui2::Panel* panel )
{
	if ( panel == m_pAvailableMapList && m_pAvailableMapList->GetSelectedItemsCount() > 0 )
		m_pMapCycleList->ClearSelectedItems();
	else if ( panel == m_pMapCycleList && m_pMapCycleList->GetSelectedItemsCount() > 0 )
		m_pAvailableMapList->ClearSelectedItems();

	InvalidateLayout( false, false );
}

void CMapCycleEditDialog::OnCommand( const char* command )
{
	if ( !_stricmp( command, "ArrowLeft" ) )
	{
		while ( m_pMapCycleList->GetSelectedItemsCount() > 0 )
		{
			int itemID = m_pMapCycleList->GetSelectedItem( 0 );
			KeyValues* data = m_pMapCycleList->GetItem( itemID );
			if ( !data )
				return;

			KeyValues* item = new KeyValues( "MapItem", "Map", data->GetString( "Map" ) );
			m_pAvailableMapList->AddItem( item, 0, true, false );
			item->deleteThis();
			m_pMapCycleList->RemoveItem( itemID );
		}
	}
	else if ( !_stricmp( command, "ArrowRight" ) )
	{
		while ( m_pAvailableMapList->GetSelectedItemsCount() > 0 )
		{
			int itemID = m_pAvailableMapList->GetSelectedItem( 0 );
			KeyValues* data = m_pAvailableMapList->GetItem( itemID );
			if ( !data )
				return;

			KeyValues* item = new KeyValues( "MapItem", "Map", data->GetString( "Map" ) );
			m_pMapCycleList->AddItem( item, 0, true, false );
			item->deleteThis();
			m_pAvailableMapList->RemoveItem( itemID );
		}
	}
	else if ( !_stricmp( command, "ArrowUp" ) || !_stricmp( command, "ArrowDown" ) )
	{
		bool up = !_stricmp( command, "ArrowUp" );
		int itemID = m_pMapCycleList->GetSelectedItem( 0 );
		int row = m_pMapCycleList->GetItemCurrentRow( itemID ) + ( up ? -1 : 1 );
		if ( row < 0 || row >= m_pMapCycleList->GetItemCount() )
			return;

		int otherItemID = m_pMapCycleList->GetItemIDFromRow( row );
		KeyValues* d1 = m_pMapCycleList->GetItem( itemID );
		KeyValues* d2 = m_pMapCycleList->GetItem( otherItemID );
		if ( !d1 || !d2 )
			return;

		CUtlSymbol tempString = d1->GetString( "Map" );
		d1->SetString( "Map", d2->GetString( "Map" ) );
		d2->SetString( "Map", tempString.String() );

		m_pMapCycleList->ApplyItemChanges( itemID );
		m_pMapCycleList->ApplyItemChanges( otherItemID );

		PostMessage( m_pMapCycleList, new KeyValues( "KeyCodePressed", "code", up ? vgui2::KEY_UP : vgui2::KEY_DOWN ), 0.0f );
	}
	else if ( !_stricmp( command, "Cancel" ) )
	{
		Close();
	}
	else if ( !_stricmp( command, "OK" ) )
	{
		CUtlBuffer msg( 0, 1024, true );
		for ( int i = 0; i < m_pMapCycleList->GetItemCount(); i++ )
		{
			KeyValues* kv = m_pMapCycleList->GetItem( m_pMapCycleList->GetItemIDFromRow( i ) );
			if ( kv )
			{
				msg.PutString( kv->GetString( "Map" ) );
				msg.PutChar( '\n' );
			}
		}
		msg.PutChar( 0 );

		RemoteServer().SetValue( "mapcycle", (const char*)msg.Base() );
		PostActionSignal( new KeyValues( "VarChanged", "var", "mapcycle" ) );
		Close();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

CDialogAddBan::CDialogAddBan( vgui2::Panel* parent ) : Frame( parent, "DialogAddBan", true, false )
{
	SetSize( 320, 200 );
	SetTitle( "#Game_Ban_Add_Title", false );

	m_pIDTextEntry = new vgui2::TextEntry( this, "IDTextEntry" );
	m_pOkayButton = new vgui2::Button( this, "OkayButton", "#Okay_Button" );
	m_pPermBanRadio = new vgui2::RadioButton( this, "PermBanRadio", "#Add_Ban_Time_Permanent" );
	m_pTempBanRadio = new vgui2::RadioButton( this, "TempBanRadio", "#Add_Ban_Time_Temporary" );
	m_pPermBanRadio->SetSelected( true );
	m_pTimeTextEntry = new vgui2::TextEntry( this, "TimeTextEntry" );
	m_pTimeCombo = new vgui2::ComboBox( this, "TimeCombo", 3, false );
	int defaultItem = m_pTimeCombo->AddItem( "#Add_Ban_Period_Minutes", NULL );
	m_pTimeCombo->AddItem( "#Add_Ban_Period_Hours", NULL );
	m_pTimeCombo->AddItem( "#Add_Ban_Period_Days", NULL );
	m_pTimeCombo->ActivateItem( defaultItem );
	m_cType = "";

	LoadControlSettings( "Admin\\DialogAddBan.res", "PLATFORM" );
	SetTitle( "#Add_Ban_Title", true );
	SetSizeable( false );

	MoveToCenterOfScreen();
}

CDialogAddBan::~CDialogAddBan()
{
}

void CDialogAddBan::Activate( const char* type, const char* player, const char* authid )
{
	m_cType = type;

	m_pOkayButton->SetAsDefaultButton( 1 );
	MakePopup( true, false );
	MoveToFront();
	RequestFocus( 0 );
	m_pIDTextEntry->RequestFocus( 0 );
	SetVisible( true );

	vgui2::TextEntry* playerEntry = dynamic_cast<vgui2::TextEntry*>( FindChildByName( "PlayerTextEntry", false ) );
	if ( playerEntry )
		playerEntry->SetText( player );

	vgui2::TextEntry* idEntry = dynamic_cast<vgui2::TextEntry*>( FindChildByName( "IDTextEntry", false ) );
	if ( idEntry )
		idEntry->SetText( authid );

	BaseClass::Activate();
}

void CDialogAddBan::PerformLayout()
{
	BaseClass::PerformLayout();
}

bool CDialogAddBan::IsIPCheck()
{
	char buf[64];
	m_pIDTextEntry->GetText( buf, sizeof( buf ) - 1 );
	int dotCount = 0;
	for ( size_t i = 0; i < strlen( buf ); i++ )
	{
		if ( buf[i] == '.' )
			dotCount++;
	}
	return dotCount > 0;
}

void CDialogAddBan::OnButtonToggled( vgui2::Panel* panel )
{
	if ( panel == m_pPermBanRadio )
	{
		m_pTimeTextEntry->SetEnabled( false );
		m_pTimeCombo->SetEnabled( false );
	}
	else
	{
		m_pTimeTextEntry->SetEnabled( true );
		m_pTimeCombo->SetEnabled( true );
	}

	Repaint();
}

void CDialogAddBan::OnCommand( const char* command )
{
	if ( !_stricmp( command, "Okay" ) )
	{
		char buf[64];
		char idbuf[64];
		float time = 0.0f;
		m_pIDTextEntry->GetText( idbuf, sizeof( idbuf ) );
		m_pTimeTextEntry->GetText( buf, sizeof( buf ) );

		if ( strlen( idbuf ) <= 0 )
		{
			vgui2::MessageBox* dlg = new vgui2::MessageBox( "#Add_Ban_Error", "#Add_Ban_ID_Invalid", NULL );
			dlg->DoModal( NULL );
			return;
		}

		if ( strlen( buf ) <= 0 && !m_pPermBanRadio->IsSelected() )
		{
			vgui2::MessageBox* dlg = new vgui2::MessageBox( "#Add_Ban_Error", "#Add_Ban_Time_Empty", NULL );
			dlg->DoModal( NULL );
			return;
		}

		if ( !m_pPermBanRadio->IsSelected() )
		{
			if ( sscanf( buf, "%f", &time ) != 1 )
				time = 0.0f;
			m_pTimeCombo->GetText( buf, sizeof( buf ) );
			if ( strstr( buf, "hour" ) )
				time *= 60.0f;
			else if ( strstr( buf, "day" ) )
				time *= 60.0f * 24.0f;

			if ( time < 0.0f )
			{
				vgui2::MessageBox* dlg = new vgui2::MessageBox( "#Add_Ban_Error", "#Add_Ban_Time_Invalid", NULL );
				dlg->DoModal( NULL );
				return;
			}
		}

		KeyValues* msg = new KeyValues( "AddBanValue" );
		msg->SetFloat( "time", time );
		msg->SetString( "id", idbuf );
		msg->SetString( "type", m_cType );
		msg->SetInt( "ipcheck", IsIPCheck() );
		PostActionSignal( msg );
		Close();
	}
	else if ( !_stricmp( command, "Close" ) )
	{
		Close();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

CDialogCvarChange::CDialogCvarChange( vgui2::Panel* parent ) : Frame( parent, "DialogCvarChange", true, false )
{
	SetSize( 320, 200 );
	m_bAddCvarText = true;
	m_cType = "";

	m_pInfoLabel = new vgui2::Label( this, "InfoLabel", "" );
	m_pCvarLabel = new vgui2::Label( this, "CvarLabel", "" );
	m_pCvarEntry = new vgui2::TextEntry( this, "CvarEntry" );
	m_pOkayButton = new vgui2::Button( this, "OkayButton", "#Okay_Button" );

	LoadControlSettings( "Admin/DialogCvarChange.res", "PLATFORM" );
	SetTitle( "#Cvar_Title", true );
	SetSizeable( false );

	MoveToCenterOfScreen();
}

CDialogCvarChange::~CDialogCvarChange()
{
}

void CDialogCvarChange::Activate( const char* cvarName, const char* curValue, const char* type,
	const char* question )
{
	m_pCvarLabel->SetText( cvarName );
	if ( !m_bAddCvarText )
		m_pCvarLabel->SetVisible( false );
	m_pInfoLabel->SetText( question );

	m_cType = type;

	m_pOkayButton->SetAsDefaultButton( 1 );
	MakePopup( true, false );
	MoveToFront();

	m_pCvarEntry->SetText( curValue );
	m_pCvarEntry->RequestFocus( 0 );
	RequestFocus( 0 );
	vgui2::input()->SetAppModalSurface( GetVPanel() );
	SetVisible( true );
	BaseClass::Activate();
}

void CDialogCvarChange::OnCommand( const char* command )
{
	if ( !_stricmp( command, "Okay" ) )
	{
		KeyValues* msg = new KeyValues( "CvarChangeValue" );
		char buf[64];
		m_pCvarLabel->GetText( buf, sizeof( buf ) );
		msg->SetString( "player", buf );
		m_pCvarEntry->GetText( buf, sizeof( buf ) - 1 );
		msg->SetString( "value", buf );
		msg->SetString( "type", m_cType );
		PostActionSignal( msg );
		Close();
	}
	else if ( !_stricmp( command, "Close" ) )
	{
		Close();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

void CDialogCvarChange::OnClose()
{
	BaseClass::OnClose();
	MarkForDeletion();
}

CVarListPropertyPage::CVarListPropertyPage( vgui2::Panel* parent, const char* name )
	: PropertyPage( parent, name, true )
{
	SetBuildGroup( GetBuildGroup() );

	m_pRulesList = new vgui2::ListPanel( this, "RulesList" );
	m_pRulesList->AddColumnHeader( 0, "name", "Variable", 256, 0 );
	m_pRulesList->AddColumnHeader( 1, "value", "Value", 256, 0 );

	m_pEditButton = new vgui2::Button( this, "EditButton", "Edit..." );
	m_pEditButton->SetCommand( new KeyValues( "EditVariable" ) );
}

CVarListPropertyPage::~CVarListPropertyPage()
{
	RemoteServer().RemoveServerDataResponseTarget( this );
}

bool CVarListPropertyPage::LoadVarList( const char* varfile )
{
	bool success = false;

	m_pRulesList->DeleteAllItems();

	KeyValues* dat = new KeyValues( "VarList" );
	if ( dat->LoadFromFile( vgui2::filesystem(), varfile, NULL ) )
	{
		for ( KeyValues* rule = dat->GetFirstSubKey(); rule != NULL; rule = rule->GetNextKey() )
			m_pRulesList->AddItem( rule, 0, false, false );

		success = true;
	}

	dat->deleteThis();
	return success;
}

void CVarListPropertyPage::RefreshVarList()
{
	for ( int i = 0; i < m_pRulesList->GetItemCount(); i++ )
	{
		KeyValues* row = m_pRulesList->GetItem( i );
		if ( !row )
			continue;

		RemoteServer().RequestValue( this, row->GetName() );
	}

	RemoteServer().ProcessServerResponse();
}

void CVarListPropertyPage::OnServerDataResponse( const char* value, const char* response )
{
	SetVarString( value, response );
}

void CVarListPropertyPage::SetVarString( const char* varName, const char* value )
{
	int itemID = m_pRulesList->GetItem( varName );
	KeyValues* rule = m_pRulesList->GetItem( itemID );
	if ( !rule )
		return;

	const char* type = rule->GetString( "type" );
	if ( !_stricmp( type, "enumeration" ) )
	{
		int iValue = atoi( value );
		const char* result = rule->FindKey( "list", true )->GetString( value, "" );
		rule->SetString( "value", result );
		rule->SetInt( "enum", iValue );
	}
	else
	{
		rule->SetString( "value", value );
	}

	m_pRulesList->ApplyItemChanges( itemID );
}

const char* CVarListPropertyPage::GetVarString( const char* varName )
{
	int itemID = m_pRulesList->GetItem( varName );
	KeyValues* rule = m_pRulesList->GetItem( itemID );
	if ( !rule )
		return "";

	return rule->GetString( "value" );
}

void CVarListPropertyPage::SetCustomStringList( const char* varName, const char* stringList )
{
	int itemID = m_pRulesList->GetItem( varName );
	KeyValues* rule = m_pRulesList->GetItem( itemID );
	if ( !rule )
		return;

	rule->SetString( "stringlist", stringList );
}

void CVarListPropertyPage::OnEditVariable( KeyValues* rule )
{
	CVarEditDialog* dlg = new CVarEditDialog( this, "VarEditDialog" );
	dlg->Activate( this, rule );
}

void CVarListPropertyPage::EditVariable()
{
	int itemID = m_pRulesList->GetSelectedItem( 0 );
	KeyValues* rule = m_pRulesList->GetItem( itemID );
	if ( !rule )
		return;

	OnEditVariable( rule );
}

void CVarListPropertyPage::OnVarChanged( const char* var )
{
	RemoteServer().RequestValue( this, var );
	RemoteServer().ProcessServerResponse();
}

void CVarListPropertyPage::OnItemSelected()
{
	if ( m_pRulesList->GetSelectedItemsCount() > 0 )
		m_pEditButton->SetEnabled( true );
	else
		m_pEditButton->SetEnabled( false );
}

void CVarListPropertyPage::PerformLayout()
{
	BaseClass::PerformLayout();
	OnItemSelected();
}

void CVarListPropertyPage::OnKeyCodeTyped( vgui2::KeyCode code )
{
	if ( code == vgui2::KEY_F5 )
		OnResetData();
	else
		BaseClass::OnKeyCodeTyped( code );
}

CServerInfoPanel::CServerInfoPanel( vgui2::Panel* parent, const char* name ) : CVarListPropertyPage( parent, name )
{
	m_iPlayerCount = 0;
	m_iMaxPlayers = 0;
	m_flLastUptimeReceiveTime = 0.0f;
	m_iLastUptimeReceived = 0;

	LoadControlSettings( "Admin/GamePanelInfo.res", "PLATFORM" );
	LoadVarList( "Admin/MainServerConfig.vdf" );

	m_iLastUptimeDisplayed = 0;
	m_flUpdateTime = 0.0f;
	m_bMapListRetrieved = false;
	RemoteServer().AddServerMessageHandler( this, "UpdatePlayers" );
	RemoteServer().AddServerMessageHandler( this, "UpdateMap" );
}

CServerInfoPanel::~CServerInfoPanel()
{
	RemoteServer().RemoveServerDataResponseTarget( this );
}

const char* CServerInfoPanel::GetHostname()
{
	return GetVarString( "hostname" );
}

void CServerInfoPanel::OnResetData()
{
	if ( !m_bMapListRetrieved )
	{
		RemoteServer().RequestValue( this, "maplist" );
		m_bMapListRetrieved = true;
	}

	RefreshVarList();

	RemoteServer().RequestValue( this, "playercount" );
	RemoteServer().RequestValue( this, "maxplayers" );
	RemoteServer().RequestValue( this, "gamedescription" );
	RemoteServer().RequestValue( this, "uptime" );
	RemoteServer().RequestValue( this, "ipaddress" );

	m_flUpdateTime = (float)( vgui2::system()->GetFrameTime() + 60.0 );
}

void CServerInfoPanel::OnThink()
{
	if ( vgui2::system()->GetFrameTime() > m_flUpdateTime )
		OnResetData();

	int time = m_iLastUptimeReceived + (int)( vgui2::system()->GetFrameTime() - m_flLastUptimeReceiveTime );
	if ( time != m_iLastUptimeDisplayed )
	{
		m_iLastUptimeDisplayed = time;
		char timeText[64];
		_snprintf( timeText, sizeof( timeText ), "%0.1i:%0.2i:%0.2i:%0.2i", ( time / 3600 ) / 24, time / 3600,
			( time / 60 ) % 60, time % 60 );
		timeText[sizeof( timeText ) - 1] = 0;
		SetControlString( "UpTimeText", timeText );
	}
}

void CServerInfoPanel::OnEditVariable( KeyValues* rule )
{
	if ( !_stricmp( rule->GetName(), "mapcycle" ) )
	{
		CMapCycleEditDialog* dlg = new CMapCycleEditDialog( this, "MapCycleEditDialog" );
		dlg->Activate( this, m_AvailableMaps, m_MapCycle );
	}
	else
	{
		BaseClass::OnEditVariable( rule );
	}
}

void CServerInfoPanel::OnServerDataResponse( const char* value, const char* response )
{
	if ( !_stricmp( value, "playercount" ) )
	{
		m_iPlayerCount = atoi( response );
	}
	else if ( !_stricmp( value, "maxplayers" ) )
	{
		m_iMaxPlayers = atoi( response );

		char buf[128];
		buf[0] = 0;
		if ( m_iMaxPlayers > 0 )
			sprintf( buf, "%d / %d", m_iPlayerCount, m_iMaxPlayers );
		SetControlString( "PlayersText", buf );
	}
	else if ( !_stricmp( value, "gamedescription" ) )
	{
		SetControlString( "GameText", response );
	}
	else if ( !_stricmp( value, "hostname" ) )
	{
		PostActionSignal( new KeyValues( "UpdateTitle" ) );
	}
	else if ( !_stricmp( value, "UpdateMap" ) || !_stricmp( value, "UpdatePlayers" ) )
	{
		m_flUpdateTime = 0.0f;
	}
	else if ( !_stricmp( value, "maplist" ) )
	{
		ParseIntoMapList( response, m_AvailableMaps );
		RemoveUnlistedMaps( m_AvailableMaps );

		CUtlBuffer mapList( 0, 1024, true );
		for ( int i = 0; i < m_AvailableMaps.Count(); i++ )
		{
			mapList.PutString( m_AvailableMaps[i].String() );
			mapList.PutChar( '\n' );
		}
		mapList.PutChar( 0 );

		SetCustomStringList( "map", (const char*)mapList.Base() );
		return;
	}
	else if ( !_stricmp( value, "uptime" ) )
	{
		m_iLastUptimeReceived = atoi( response );
		m_flLastUptimeReceiveTime = (float)vgui2::system()->GetFrameTime();
	}
	else if ( !_stricmp( value, "ipaddress" ) )
	{
		SetControlString( "ServerIPText", response );
	}
	else if ( !_stricmp( value, "mapcycle" ) )
	{
		ParseIntoMapList( response, m_MapCycle );
		UpdateMapCycleValue();
		return;
	}

	BaseClass::OnServerDataResponse( value, response );

	if ( !_stricmp( value, "map" ) )
		UpdateMapCycleValue();
}

void CServerInfoPanel::ParseIntoMapList( const char* maplist, CUtlVector<CUtlSymbol>& mapArray )
{
	mapArray.RemoveAll();

	const char* parse = maplist;
	while ( *parse )
	{
		if ( isspace( (unsigned char)*parse ) )
		{
			parse++;
			continue;
		}

		const char* end = strstr( parse, "\n" );
		const char* end2 = strstr( parse, "\r" );
		if ( !end )
			break;
		if ( end2 && end2 < end )
			end = end2;

		int nameSize = (int)( end - parse );
		char customString[64];
		if ( nameSize >= (int)sizeof( customString ) )
			nameSize = (int)sizeof( customString ) - 1;

		strncpy( customString, parse, nameSize );
		customString[nameSize] = 0;
		parse = end;

		if ( customString[0] == '/' && customString[1] == '/' )
			continue;

		int index = mapArray.AddToTail();
		mapArray[index] = customString;
	}
}

void CServerInfoPanel::UpdateMapCycleValue()
{
	CUtlSymbol currentMap = GetVarString( "map" );
	if ( !currentMap.IsValid() )
		return;

	int listPoint = -1;
	for ( int i = 0; i < m_MapCycle.Count(); i++ )
	{
		if ( !_stricmp( m_MapCycle[i].String(), currentMap.String() ) )
			listPoint = i;
	}

	char nextMaps[512];
	nextMaps[0] = 0;
	bool needComma = false;
	for ( int i = 0; i < 2; i++ )
	{
		int point = listPoint + i + 1;
		if ( point >= m_MapCycle.Count() )
			point -= m_MapCycle.Count();

		if ( m_MapCycle.IsValidIndex( point ) )
		{
			if ( needComma )
				strcat( nextMaps, ", " );
			strcat( nextMaps, m_MapCycle[point].String() );
			needComma = true;
		}
	}

	if ( needComma )
		strcat( nextMaps, ", " );
	strcat( nextMaps, "..." );

	SetVarString( "mapcycle", nextMaps );
}

CServerConfigPanel::CServerConfigPanel( vgui2::Panel* parent, const char* name, const char* )
	: CVarListPropertyPage( parent, name )
{
	SetBounds( 0, 0, 500, 170 );
	LoadControlSettings( "Admin\\ServerConfigPanel.res", "PLATFORM" );

	LoadVarList( "GameServerConfig.vdf" );

	m_flUpdateTime = 0.0f;
}

CServerConfigPanel::~CServerConfigPanel()
{
}

void CServerConfigPanel::OnResetData()
{
	RefreshVarList();
	m_flUpdateTime = (float)( vgui2::system()->GetFrameTime() + 60.0 );
}

void CServerConfigPanel::OnThink()
{
	if ( m_flUpdateTime < vgui2::system()->GetFrameTime() )
		OnResetData();
}

CPlayerPanel::CPlayerPanel( vgui2::Panel* parent, const char* name ) : PropertyPage( parent, name, true )
{
	SetBuildGroup( GetBuildGroup() );

	m_pPlayerListPanel = new vgui2::ListPanel( this, "Players list" );
	m_pPlayerListPanel->AddColumnHeader( 0, "name", "#Player_Panel_Name", 200,
		vgui2::ListPanel::COLUMN_RESIZEWITHWINDOW );
	m_pPlayerListPanel->AddColumnHeader( 1, "authid", "#Player_Panel_ID", 100, 0 );
	m_pPlayerListPanel->AddColumnHeader( 2, "ping", "#Player_Panel_Ping", 50, 0 );
	m_pPlayerListPanel->AddColumnHeader( 3, "loss", "#Player_Panel_Loss", 50, 0 );
	m_pPlayerListPanel->AddColumnHeader( 4, "frags", "#Player_Panel_Frags", 50, 0 );
	m_pPlayerListPanel->AddColumnHeader( 5, "time", "#Player_Panel_Time", 75, 0 );
	m_pPlayerListPanel->SetSortFunc( 5, PlayerTimeCompare );
	m_pPlayerListPanel->SetEmptyListText( "#Player_Panel_No_Players" );
	m_pPlayerListPanel->SetSortColumn( 0 );

	m_pKickButton = new vgui2::Button( this, "Kick", "#Player_Panel_Kick" );
	m_pBanButton = new vgui2::Button( this, "Ban", "#Player_Panel_Ban" );

	m_pPlayerContextMenu = new CPlayerContextMenu( this );
	m_pPlayerContextMenu->SetVisible( false );

	LoadControlSettings( "Admin/PlayerPanel.res", "PLATFORM" );

	m_pKickButton->SetCommand( new KeyValues( "KickPlayer" ) );
	m_pBanButton->SetCommand( new KeyValues( "BanPlayer" ) );

	OnItemSelected();

	m_flUpdateTime = 0.0f;
	RemoteServer().AddServerMessageHandler( this, "UpdatePlayers" );
}

CPlayerPanel::~CPlayerPanel()
{
	RemoteServer().RemoveServerDataResponseTarget( this );
}

void CPlayerPanel::OnResetData()
{
	RemoteServer().RequestValue( this, "playerlist" );
	m_flUpdateTime = (float)( vgui2::system()->GetFrameTime() + 60.0 );
}

void CPlayerPanel::OnThink()
{
	if ( vgui2::system()->GetFrameTime() > m_flUpdateTime )
		OnResetData();
}

void CPlayerPanel::OnKeyCodeTyped( vgui2::KeyCode code )
{
	if ( code == vgui2::KEY_F5 )
		OnResetData();
	else
		BaseClass::OnKeyCodeTyped( code );
}

void CPlayerPanel::OnServerDataResponse( const char* value, const char* response )
{
	if ( !_stricmp( value, "UpdatePlayers" ) )
	{
		m_flUpdateTime = 0.0f;
		return;
	}

	if ( _stricmp( value, "playerlist" ) )
		return;

	m_pPlayerListPanel->DeleteAllItems();

	const char* parse = response;
	while ( parse && *parse )
	{
		char name[64];
		char authID[64];
		char netAdr[32];
		int ping = 0;
		int packetLoss = 0;
		int frags = 0;
		int connectTime = 0;
		name[0] = 0;
		authID[0] = 0;
		netAdr[0] = 0;

		vgui2::ivgui()->DPrintf( "orig:  %s\n", parse );

		if ( *parse != '\"' )
			break;

		++parse;
		int pos = 0;
		while ( *parse && *parse != '\"' )
		{
			if ( pos < (int)sizeof( name ) - 1 )
				name[pos++] = *parse;
			parse++;
		}
		name[pos] = 0;
		if ( *parse )
			parse++;

		if ( sscanf( parse, " %63s %31s %d %d %d %d\n", authID, netAdr, &ping, &packetLoss, &frags, &connectTime ) != 6 )
			break;

		static char timeString[64];
		if ( connectTime / 60 > 0 && connectTime / 60 / 60 > 0 )
			_snprintf( timeString, sizeof( timeString ), "%2i:%02i:%02i", connectTime / 60 / 60, ( connectTime / 60 ) % 60,
				connectTime % 60 );
		else
			_snprintf( timeString, sizeof( timeString ), "%02i:%02i", connectTime / 60, connectTime % 60 );
		timeString[sizeof( timeString ) - 1] = 0;

		vgui2::ivgui()->DPrintf( "pars:  \"%s\" %s %s %d %d %d %s\n", name, authID, netAdr, ping, packetLoss, frags,
			timeString );

		KeyValues* player = new KeyValues( "Player" );
		player->SetString( "name", name );
		player->SetString( "authID", authID );
		player->SetString( "netAdr", netAdr );
		player->SetInt( "ping", ping );
		player->SetInt( "loss", packetLoss );
		player->SetInt( "frags", frags );
		player->SetString( "time", timeString );
		m_pPlayerListPanel->AddItem( player, 0, false, false );
		player->deleteThis();

		parse = strchr( parse, '\n' );
		if ( parse )
			parse++;
	}
}

void CPlayerPanel::OnOpenContextMenu( int )
{
}

void CPlayerPanel::OnItemSelected()
{
	bool state = m_pPlayerListPanel->GetSelectedItemsCount() != 0;
	m_pKickButton->SetEnabled( state );
	m_pBanButton->SetEnabled( state );

	if ( m_pPlayerListPanel->GetSelectedItemsCount() > 1 )
		m_pBanButton->SetEnabled( false );
}

void CPlayerPanel::OnKickButtonPressed()
{
	if ( m_pPlayerListPanel->GetSelectedItemsCount() < 1 )
		return;

	vgui2::QueryBox* box = NULL;
	if ( m_pPlayerListPanel->GetSelectedItemsCount() <= 1 )
	{
		KeyValues* kv = m_pPlayerListPanel->GetItem( m_pPlayerListPanel->GetSelectedItem( 0 ) );
		if ( !kv )
			return;

		wchar_t playerName[64];
		vgui2::localize()->ConvertANSIToUnicode( kv->GetString( "name" ), playerName, sizeof( playerName ) );
		wchar_t msg[512];
		vgui2::localize()->ConstructString( msg, sizeof( msg ), vgui2::localize()->Find( "Kick_Single_Player_Question" ), 1,
			playerName );
		box = new vgui2::QueryBox( vgui2::localize()->Find( "#Kick_Single_Player_Title" ), msg, NULL );
	}
	else
	{
		box = new vgui2::QueryBox( "#Kick_Multiple_Players_Title", "#Kick_Multiple_Players_Question", NULL );
	}

	UseUnscaledLayout( box );
	box->AddActionSignalTarget( this );
	box->SetOKCommand( new KeyValues( "KickSelectedPlayers" ) );
	box->DoModal( NULL );
}

void CPlayerPanel::KickSelectedPlayers()
{
	for ( int i = 0; i < m_pPlayerListPanel->GetSelectedItemsCount(); i++ )
	{
		KeyValues* pl = m_pPlayerListPanel->GetItem( m_pPlayerListPanel->GetSelectedItem( i ) );
		if ( !pl )
			continue;

		char cmd[512];
		_snprintf( cmd, sizeof( cmd ), "kick \"%s\"", pl->GetString( "name" ) );
		cmd[sizeof( cmd ) - 1] = 0;
		RemoteServer().SendCommand( cmd );
	}

	m_pPlayerListPanel->ClearSelectedItems();
	m_pKickButton->SetEnabled( false );
	m_pBanButton->SetEnabled( false );
	OnResetData();
}

void CPlayerPanel::OnBanButtonPressed()
{
	if ( m_pPlayerListPanel->GetSelectedItemsCount() != 1 )
		return;

	KeyValues* pl = m_pPlayerListPanel->GetItem( m_pPlayerListPanel->GetSelectedItem( 0 ) );
	if ( !pl )
		return;

	const char* player = pl->GetString( "name" );
	const char* authid = pl->GetString( "authid" );
	const char* netAdr = pl->GetString( "netAdr" );

	char buf[64];
	const char* banID = authid;
	int ip1, ip2, ip3, ip4;
	if ( !strcmp( authid, "UNKNOWN" ) && sscanf( netAdr, "%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4 ) == 4 )
	{
		_snprintf( buf, sizeof( buf ), "%d.%d.%d.%d", ip1, ip2, ip3, ip4 );
		buf[sizeof( buf ) - 1] = 0;
		banID = buf;
	}

	CDialogAddBan* box = new CDialogAddBan( this );
	box->AddActionSignalTarget( this );
	box->Activate( "addban", player, banID );
}

void CPlayerPanel::AddBanByID( const char* id, const char* newtime )
{
	if ( !id || !*id )
		return;

	if ( !newtime || atof( newtime ) < 0.001 )
		newtime = "0";

	bool isIPAddress = ParseIPAddress( id );

	char cmd[512];
	_snprintf( cmd, sizeof( cmd ) - 1, "%s %s %s\n", isIPAddress ? "addip" : "banid", newtime, id );
	cmd[sizeof( cmd ) - 1] = 0;
	RemoteServer().SendCommand( cmd );
	RemoteServer().SendCommand( isIPAddress ? "writeid" : "writeip" );

	OnResetData();
}

CBanPanel::CBanPanel( vgui2::Panel* parent, const char* name ) : PropertyPage( parent, name, true )
{
	SetBuildGroup( GetBuildGroup() );

	m_pBanListPanel = new vgui2::ListPanel( this, "BanList" );
	m_pBanListPanel->AddColumnHeader( 0, "type", "#Ban_List_Type", 150, 0 );
	m_pBanListPanel->AddColumnHeader( 1, "id", "#Ban_List_ID", 200, 0 );
	m_pBanListPanel->AddColumnHeader( 2, "time", "#Ban_List_Time", 200, 0 );
	m_pBanListPanel->SetSortColumn( 2 );
	m_pBanListPanel->SetEmptyListText( "#Ban_List_Empty" );

	m_pAddButton = new vgui2::Button( this, "Add", "#Ban_List_Add" );
	m_pRemoveButton = new vgui2::Button( this, "Remove", "#Ban_List_Remove" );
	m_pChangeButton = new vgui2::Button( this, "Change", "#Ban_List_Edit" );
	m_pImportButton = new vgui2::Button( this, "Import", "#Ban_List_Import" );

	m_pAddButton->SetCommand( new KeyValues( "addban" ) );
	m_pRemoveButton->SetCommand( new KeyValues( "removeban" ) );
	m_pChangeButton->SetCommand( new KeyValues( "changeban" ) );
	m_pImportButton->SetCommand( new KeyValues( "importban" ) );

	m_pBanContextMenu = new CBanContextMenu( this );
	m_pBanContextMenu->SetVisible( false );

	m_flUpdateTime = 0.0f;
	m_bPageViewed = false;

	LoadControlSettings( "Admin/BanPanel.res", "PLATFORM" );
}

CBanPanel::~CBanPanel()
{
	RemoteServer().RemoveServerDataResponseTarget( this );
}

void CBanPanel::OnResetData()
{
	RemoteServer().RequestValue( this, "banlist" );
	m_flUpdateTime = (float)( vgui2::system()->GetFrameTime() + 300.0 );
}

void CBanPanel::OnPageShow()
{
	OnItemSelected();
	if ( !m_bPageViewed )
	{
		m_bPageViewed = true;
		m_flUpdateTime = 0.0f;
	}
}

void CBanPanel::OnThink()
{
	if ( vgui2::system()->GetFrameTime() > m_flUpdateTime )
		OnResetData();
}

void CBanPanel::OnKeyCodeTyped( vgui2::KeyCode code )
{
	if ( code == vgui2::KEY_F5 )
		OnResetData();
	else
		BaseClass::OnKeyCodeTyped( code );
}

bool CBanPanel::IsIPAddress( const char* text )
{
	return ParseIPAddress( text );
}

void CBanPanel::OnServerDataResponse( const char* value, const char* response )
{
	if ( _stricmp( value, "banlist" ) )
		return;

	m_pBanListPanel->DeleteAllItems();

	int item = 0;
	float banTime = 0.0f;
	char id[64];
	memset( id, 0, sizeof( id ) );

	const char* start = response;
	while ( sscanf( start, "%i %63s : %f min\n", &item, id, &banTime ) == 3 )
	{
		KeyValues* ban = new KeyValues( "ban" );

		if ( IsIPAddress( id ) )
		{
			const wchar_t* type = vgui2::localize()->Find( "#Ban_IP" );
			ban->SetWString( "type", type ? type : L"IP Address" );
		}
		else
		{
			const wchar_t* type = vgui2::localize()->Find( "#Ban_Auth_ID" );
			ban->SetWString( "type", type ? type : L"AuthID" );
		}
		ban->SetString( "id", id );

		if ( banTime > 0.0f )
		{
			ban->SetFloat( "time", banTime );
		}
		else
		{
			const wchar_t* permanent = vgui2::localize()->Find( "#Ban_Permanent" );
			ban->SetWString( "time", permanent ? permanent : L"permanent" );
		}

		m_pBanListPanel->AddItem( ban, 0, false, false );
		ban->deleteThis();

		start = strchr( start, '\n' );
		if ( !start )
			break;
		start++;
	}
}

void CBanPanel::OnAddBan()
{
	CDialogAddBan* box = new CDialogAddBan( this );
	box->AddActionSignalTarget( this );
	box->Activate( "addban", "", "" );
}

void CBanPanel::OnRemoveBan()
{
	int itemID = m_pBanListPanel->GetSelectedItem( 0 );
	if ( itemID == -1 )
		return;

	KeyValues* kv = m_pBanListPanel->GetItem( itemID );
	if ( !kv )
		return;

	wchar_t banName[256];
	vgui2::localize()->ConvertANSIToUnicode( kv->GetString( "id" ), banName, sizeof( banName ) );
	wchar_t msg[256];
	vgui2::localize()->ConstructString( msg, sizeof( msg ), vgui2::localize()->Find( "#Ban_Remove_Msg" ), 1, banName );

	vgui2::QueryBox* box = new vgui2::QueryBox( vgui2::localize()->Find( "#Ban_Title_Remove" ), msg, NULL );
	UseUnscaledLayout( box );
	box->SetOKCommand( new KeyValues( "removebanbyid", "id", kv->GetString( "id" ) ) );
	box->AddActionSignalTarget( this );
	box->DoModal( NULL );
}

void CBanPanel::OnChangeBan()
{
	int itemID = m_pBanListPanel->GetSelectedItem( 0 );
	if ( itemID == -1 )
		return;

	KeyValues* kv = m_pBanListPanel->GetItem( itemID );
	if ( !kv )
		return;

	char timeText[20];
	float time = kv->GetFloat( "time" );
	_snprintf( timeText, sizeof( timeText ), "%0.2f", time );
	timeText[sizeof( timeText ) - 1] = 0;

	CDialogCvarChange* box = new CDialogCvarChange( this );
	box->AddActionSignalTarget( this );
	box->SetTitle( "#Ban_Title_Change", true );
	box->Activate( kv->GetString( "id" ), timeText, "changeban", "#Ban_Change_Time" );
}

void CBanPanel::RemoveBanByID( const char* id )
{
	if ( !id || !*id )
		return;

	bool isIPAddress = IsIPAddress( id );

	char cmd[512];
	_snprintf( cmd, sizeof( cmd ) - 1, "%s %s\n", isIPAddress ? "removeip" : "removeid", id );
	cmd[sizeof( cmd ) - 1] = 0;
	RemoteServer().SendCommand( cmd );
	RemoteServer().SendCommand( IsIPAddress( id ) ? "writeip" : "writeid" );

	OnResetData();
}

void CBanPanel::AddBanByID( const char* id, const char* newtime )
{
	if ( !id || !*id )
		return;

	if ( !newtime || atof( newtime ) < 0.001 )
		newtime = "0";

	bool isIPAddress = IsIPAddress( id );

	char cmd[512];
	_snprintf( cmd, sizeof( cmd ) - 1, "%s %s %s\n", isIPAddress ? "addip" : "banid", newtime, id );
	cmd[sizeof( cmd ) - 1] = 0;
	RemoteServer().SendCommand( cmd );
	RemoteServer().SendCommand( IsIPAddress( id ) ? "writeip" : "writeid" );

	OnResetData();
}

void CBanPanel::OnCvarChangeValue( KeyValues* kv )
{
	const char* idText = kv->GetString( "player", "" );
	const char* durationText = kv->GetString( "value", "0" );
	AddBanByID( idText, durationText );
}

void CBanPanel::OnOpenContextMenu( int )
{
}

void CBanPanel::OnItemSelected()
{
	if ( m_pBanListPanel->GetSelectedItem( 0 ) == -1 )
	{
		m_pRemoveButton->SetEnabled( false );
		m_pChangeButton->SetEnabled( false );
	}
	else
	{
		m_pRemoveButton->SetEnabled( true );
		m_pChangeButton->SetEnabled( true );
	}
}

void CBanPanel::OnImportBanList()
{
	FileOpenDialog* dlg = new FileOpenDialog( this, "#Ban_Find_Ban_File", true );
	dlg->AddFilter( "*.cfg", "#Config_files", true );
	dlg->AddFilter( "*.*", "#All_files", false );
	dlg->DoModal( true );
	dlg->Activate();
}

void CBanPanel::OnFileSelected( const char* fullpath )
{
	char line[255];
	TokenLine tok;

	vgui2::surface()->SetCursor( vgui2::dc_hourglass );

	FILE* f = fopen( fullpath, "rb" );
	if ( f )
	{
		while ( !feof( f ) && fgets( line, sizeof( line ), f ) )
		{
			tok.SetLine( line );
			if ( tok.CountToken() == 3 )
				AddBanByID( tok.GetToken( 2 ), "0" );
		}
	}

	vgui2::surface()->SetCursor( vgui2::dc_user );
	if ( f )
		fclose( f );
}

CGraphPanel::CGraphsImage::CGraphsImage() : vgui2::Image(), points()
{
	x = 0;
	y = 0;
	maxIn = maxOut = minIn = minOut = minFPS = maxFPS = minPing = maxPing = 0.0f;
	maxPlayers = minPlayers = 0.0f;
	cpu = fps = net_i = ping = net_o = players = false;
	timeBetween = MINUTES;
	numAvgs = 0;
	memset( &avgPoint, 0x0, sizeof( Points_t ) );
	bgColor = Color( 0, 0, 0, 0 );
	lineColor = Color( 0, 0, 0, 0 );
}

void CGraphPanel::CGraphsImage::SetDraw( bool cpu_in, bool fps_in, bool net_in, bool net_out, bool ping_in,
	bool players_in )
{
	cpu = cpu_in;
	fps = fps_in;
	net_i = net_in;
	net_o = net_out;
	ping = ping_in;
	players = players_in;
}

void CGraphPanel::CGraphsImage::CheckBounds( Points_t p )
{
	if ( p.in > maxIn )
		maxIn = avgPoint.in;
	if ( p.out > maxOut )
		maxOut = avgPoint.out;

	if ( p.in < minIn )
		minIn = avgPoint.in;
	if ( p.out < minOut )
		minOut = avgPoint.out;

	if ( p.fps > maxFPS )
		maxFPS = avgPoint.fps;
	if ( p.fps < minFPS )
		minFPS = avgPoint.fps;

	if ( p.ping > maxPing )
		maxPing = avgPoint.ping;
	if ( p.ping < minPing )
		minPing = avgPoint.ping;

	if ( p.players > maxPlayers )
		maxPlayers = avgPoint.players;
	if ( p.players < minPlayers )
		minPlayers = avgPoint.players;
}

bool CGraphPanel::CGraphsImage::AddPoint( Points_t p )
{
	int width, height;
	bool recalcBounds = false;

	GetSize( width, height );

	if ( avgPoint.cpu > 1 )
		return false;

	float window = 60.0f;
	if ( timeBetween == DAYS )
		window = 3600.0f;
	else if ( timeBetween == HOURS )
		window = 86400.0f;

	while ( points.Count() && ( p.time - points[0].time ) > window )
		points.Remove( 0 );

	numAvgs++;
	float scale = 1.0f / (float)numAvgs;
	avgPoint.cpu = ( p.cpu + avgPoint.cpu ) * scale;
	avgPoint.fps = ( p.fps + avgPoint.fps ) * scale;
	avgPoint.in = ( p.in + avgPoint.in ) * scale;
	avgPoint.out = ( p.out + avgPoint.out ) * scale;
	avgPoint.ping = ( p.ping + avgPoint.ping ) * scale;
	avgPoint.players = ( p.players + avgPoint.players ) * scale;
	avgPoint.time = p.time;
	numAvgs = 0;

	if ( width != 0 && points.Count() > width / 2 )
	{
		int i = 0;
		while ( points.Count() > width / 2 )
		{
			if ( points[0].in == maxIn || points[0].out == maxOut || points[0].fps == maxFPS ||
				points[0].ping == maxPing || points[0].players == maxPlayers )
			{
				recalcBounds = true;
			}

			if ( i < points.Count() )
				points.Remove( i );
			else
				points.Remove( points.Count() - 1 );

			i += 2;
			if ( i > points.Count() )
				i = 0;
		}

		if ( recalcBounds )
		{
			for ( int j = 0; j < points.Count(); j++ )
				CheckBounds( points[j] );
		}
	}

	if ( avgPoint.in > maxIn )
		maxIn = avgPoint.in;
	if ( avgPoint.out > maxOut )
		maxOut = avgPoint.out;
	if ( minIn > avgPoint.in )
		minIn = avgPoint.in;
	if ( minOut > avgPoint.out )
		minOut = avgPoint.out;
	if ( avgPoint.fps > maxFPS )
		maxFPS = avgPoint.fps;
	if ( minFPS > avgPoint.fps )
		minFPS = avgPoint.fps;
	if ( avgPoint.ping > maxPing )
		maxPing = avgPoint.ping;
	if ( minPing > avgPoint.ping )
		minPing = avgPoint.ping;
	if ( avgPoint.players > maxPlayers )
		maxPlayers = avgPoint.players;
	if ( minPlayers > avgPoint.players )
		minPlayers = avgPoint.players;

	points.AddToTail( avgPoint );

	memset( &avgPoint, 0x0, sizeof( Points_t ) );

	return true;
}

void CGraphPanel::CGraphsImage::SetScale( intervals time )
{
	timeBetween = time;

	points.RemoveAll();

	maxIn = maxOut = minIn = minOut = minFPS = maxFPS = minPing = maxPing = maxPlayers = minPlayers = 0.0f;
}

void CGraphPanel::CGraphsImage::Paint()
{
	int width, height;
	GetSize( width, height );

	SetColor( bgColor );
	SetBkColor( bgColor );
	DrawFilledRect( 0, 0, width, height );

	height -= 4;
	width -= 4;

	if ( !cpu && !fps && !net_i && !net_o && !ping && !players )
		return;

	int count = points.Count();
	if ( count < 2 )
		return;

	if ( width <= 200 || height <= 100 )
		return;

	float distPoints = (float)width / (float)( count - 1 );
	if ( distPoints <= 0.0f )
		distPoints = 1.0f;

	SetColor( lineColor );
	SetBkColor( lineColor );
	DrawLine( 4, height / 2, width, height / 2 );

	const int bottom = 5;

	float RangePing = maxPing;
	float RangeFPS = maxFPS;
	float RangePlayers = maxPlayers;
	float Range = 0.0f;

	CUtlVector<int> cpuX, cpuY, inX, inY, outX, outY, fpsX, fpsY, pingX, pingY, playersX, playersY;

	if ( ping )
	{
		RangePing += (float)( maxPing * 0.1 );
		if ( RangePing <= 1.0f )
			RangePing = 1.0f;
		pingX.AddMultipleToTail( count );
		pingY.AddMultipleToTail( count );
	}

	if ( cpu )
	{
		cpuX.AddMultipleToTail( count );
		cpuY.AddMultipleToTail( count );
	}

	if ( fps )
	{
		RangeFPS += (float)( maxFPS * 0.1 );
		if ( RangeFPS <= 1.0f )
			RangeFPS = 1.0f;
		fpsX.AddMultipleToTail( count );
		fpsY.AddMultipleToTail( count );
	}

	if ( net_i || net_o )
	{
		Range = maxIn > maxOut ? maxIn : maxOut;
		Range += (float)( Range * 0.1 );
		if ( Range <= 1.0f )
			Range = 1.0f;
	}

	if ( net_i )
	{
		inX.AddMultipleToTail( count );
		inY.AddMultipleToTail( count );
	}

	if ( net_o )
	{
		outX.AddMultipleToTail( count );
		outY.AddMultipleToTail( count );
	}

	if ( players )
	{
		RangePlayers += (float)( maxPlayers * 0.1 );
		playersX.AddMultipleToTail( count );
		playersY.AddMultipleToTail( count );
	}

	for ( int i = 0; i < count; i++ )
	{
		int pointX = (int)( (float)i * distPoints ) + 2;

		if ( cpu )
		{
			cpuX[i] = pointX;
			cpuY[i] = (int)( ( 1.0f - points[i].cpu ) * (float)height );
		}

		if ( net_i )
		{
			inX[i] = pointX;
			inY[i] = (int)( ( ( Range - points[i].in ) / Range ) * (float)height - (float)bottom );
		}

		if ( net_o )
		{
			outX[i] = pointX;
			outY[i] = (int)( ( ( Range - points[i].out ) / Range ) * (float)height - (float)bottom );
		}

		if ( fps )
		{
			fpsX[i] = pointX;
			fpsY[i] = (int)( ( ( RangeFPS - points[i].fps ) / RangeFPS ) * (float)height - (float)bottom );
		}

		if ( ping )
		{
			pingX[i] = pointX;
			pingY[i] = (int)( ( ( RangePing - points[i].ping ) / RangePing ) * (float)height - (float)bottom );
		}

		if ( players )
		{
			playersX[i] = pointX;
			playersY[i] = (int)( ( ( RangePlayers - points[i].players ) / RangePlayers ) * (float)height - (float)bottom );
		}
	}

	if ( cpu )
	{
		SetColor( CPU_COLOR );
		DrawPolyLine( cpuX.Base(), cpuY.Base(), count );
	}

	if ( net_i )
	{
		SetColor( IN_COLOR );
		DrawPolyLine( inX.Base(), inY.Base(), count );
	}

	if ( net_o )
	{
		SetColor( OUT_COLOR );
		DrawPolyLine( outX.Base(), outY.Base(), count );
	}

	if ( fps )
	{
		SetColor( FPS_COLOR );
		DrawPolyLine( fpsX.Base(), fpsY.Base(), count );
	}

	if ( ping )
	{
		SetColor( PING_COLOR );
		DrawPolyLine( pingX.Base(), pingY.Base(), count );
	}

	if ( players )
	{
		SetColor( PLAYERS_COLOR );
		DrawPolyLine( playersX.Base(), playersY.Base(), count );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGraphPanel::CGraphPanel( vgui2::Panel* parent, const char* name ) : PropertyPage( parent, name, true )
{
	SetBuildGroup( GetBuildGroup() );
	SetMinimumSize( 300, 200 );

	m_pGraphsPanel = new vgui2::ImagePanel( this, "Graphs" );
	m_pGraphs = new CGraphsImage();
	m_pGraphsPanel->SetImage( m_pGraphs );

	m_pInButton = new vgui2::CheckButton( this, "InCheck", "#Graph_In" );
	m_pOutButton = new vgui2::CheckButton( this, "OutCheck", "#Graph_Out" );
	m_pFPSButton = new vgui2::CheckButton( this, "FPSCheck", "#Graph_FPS" );
	m_pCPUButton = new vgui2::CheckButton( this, "CPUCheck", "#Graph_CPU" );
	m_pPINGButton = new vgui2::CheckButton( this, "PingCheck", "#Graph_Ping" );
	m_pPlayerButton = new vgui2::CheckButton( this, "PlayersCheck", "#Graph_Players" );

	m_pTimeCombo = new vgui2::ComboBox( this, "TimeCombo", 3, false );
	m_pTimeCombo->AddItem( "#Graph_Minutes", NULL );
	int defaultItem = m_pTimeCombo->AddItem( "#Graph_Hours", NULL );
	m_pTimeCombo->AddItem( "#Graph_Day", NULL );
	m_pTimeCombo->ActivateItem( defaultItem );

	m_pVertCombo = new vgui2::ComboBox( this, "VertCombo", 6, false );
	m_pVertCombo->AddItem( "#Graph_In", NULL );
	m_pVertCombo->AddItem( "#Graph_Out", NULL );
	m_pVertCombo->AddItem( "#Graph_FPS", NULL );
	defaultItem = m_pVertCombo->AddItem( "#Graph_CPU", NULL );
	m_pVertCombo->AddItem( "#Graph_Ping", NULL );
	m_pVertCombo->AddItem( "#Graph_Players", NULL );
	m_pVertCombo->ActivateItem( defaultItem );

	m_pCPUButton->SetSelected( true );
	m_pInButton->SetSelected( false );
	m_pOutButton->SetSelected( false );
	m_pFPSButton->SetSelected( false );
	m_pPINGButton->SetSelected( false );

	LoadControlSettings( "Admin/GraphPanel.res", "PLATFORM" );

	int w, h;
	m_pGraphsPanel->GetSize( w, h );
	m_pGraphs->SaveSize( w, h );
	m_pGraphs->SetDraw( m_pCPUButton->IsSelected(), m_pFPSButton->IsSelected(), m_pInButton->IsSelected(),
		m_pOutButton->IsSelected(), m_pPINGButton->IsSelected(), m_pPlayerButton->IsSelected() );

	m_pPINGButton->SetFgColor( PING_COLOR );
	m_pCPUButton->SetFgColor( CPU_COLOR );
	m_pFPSButton->SetFgColor( FPS_COLOR );
	m_pInButton->SetFgColor( IN_COLOR );
	m_pOutButton->SetFgColor( OUT_COLOR );
	m_pPlayerButton->SetFgColor( PLAYERS_COLOR );

	vgui2::ivgui()->AddTickSignal( GetVPanel(), 0 );
	m_flNextStatsUpdateTime = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGraphPanel::~CGraphPanel()
{
	RemoteServer().RemoveServerDataResponseTarget( this );
}

//-----------------------------------------------------------------------------
// Purpose: Activates the page
//-----------------------------------------------------------------------------
void CGraphPanel::OnPageShow()
{
}

//-----------------------------------------------------------------------------
// Purpose: Hides the page
//-----------------------------------------------------------------------------
void CGraphPanel::OnPageHide()
{
}

void CGraphPanel::OnTick()
{
	if ( m_flNextStatsUpdateTime > vgui2::system()->GetFrameTime() )
		return;

	m_flNextStatsUpdateTime = (float)( vgui2::system()->GetFrameTime() + 5.0 );
	RemoteServer().RequestValue( this, "stats" );
}

void CGraphPanel::OnServerDataResponse( const char* value, const char* response )
{
	if ( _stricmp( value, "stats" ) )
		return;

	Points_t p;
	memset( &p, 0x0, sizeof( Points_t ) );
	float uptime = 0.0f;
	float users = 0.0f;
	if ( sscanf( response, "%f %f %f %f %f %f %f", &p.cpu, &p.in, &p.out, &uptime, &users, &p.fps, &p.players ) < 0 )
		return;

	p.cpu = p.cpu / 100;
	p.ping = 0;
	p.time = (float)vgui2::system()->GetCurrentTime();
	m_pGraphs->AddPoint( p );

	char buf[64];
	_snprintf( buf, sizeof( buf ), "%i", (int)p.players );
	buf[sizeof( buf ) - 1] = 0;
	SetControlString( "TotalUsersLabel", buf );

	m_pVertCombo->GetText( buf, 64 );
	OnTextChanged( m_pVertCombo, buf );
}

//-----------------------------------------------------------------------------
// Purpose: Relayouts the data
//-----------------------------------------------------------------------------
void CGraphPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	int x, y, w, h;
	m_pGraphsPanel->GetBounds( x, y, w, h );
	m_pGraphs->SaveSize( w, h );

	vgui2::Label* mid = dynamic_cast<vgui2::Label*>( FindChildByName( "AxisMid", false ) );
	if ( mid )
	{
		int cx, cy;
		mid->GetPos( cx, cy );
		mid->SetPos( cx, y - 8 + h / 2 );
	}
}

void CGraphPanel::ApplySchemeSettings( vgui2::IScheme* pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	m_pGraphsPanel->SetBorder( pScheme->GetBorder( "ButtonDepressedBorder" ) );
	m_pGraphs->SetBgColor( GetSchemeColor( "WindowBG", pScheme ) );
	m_pGraphs->SetAxisColor( Color( 76, 88, 68, 255 ) );
}

void CGraphPanel::SetAxisLabels( Color c, const char* max, const char* mid, const char* min )
{
	vgui2::Label* lab = dynamic_cast<vgui2::Label*>( FindChildByName( "AxisMax", false ) );
	if ( lab )
	{
		lab->SetFgColor( c );
		lab->SetText( max );
	}

	lab = dynamic_cast<vgui2::Label*>( FindChildByName( "AxisMid", false ) );
	if ( lab )
	{
		lab->SetFgColor( c );
		lab->SetText( mid );
	}

	lab = dynamic_cast<vgui2::Label*>( FindChildByName( "AxisMin", false ) );
	if ( lab )
	{
		lab->SetFgColor( c );
		lab->SetText( min );
	}
}

void CGraphPanel::OnTextChanged( vgui2::Panel* panel, const char* text )
{
	if ( panel == m_pTimeCombo )
	{
		if ( strstr( text, "Hour" ) )
			m_pGraphs->SetScale( HOURS );
		else if ( strstr( text, "Day" ) )
			m_pGraphs->SetScale( DAYS );
		else
			m_pGraphs->SetScale( MINUTES );
		return;
	}

	if ( panel != m_pVertCombo )
		return;

	float maxVal = 0.0f;
	float minVal = 0.0f;
	if ( strstr( text, "CPU" ) )
	{
		SetAxisLabels( CPU_COLOR, "100%", "50%", "0%" );
		return;
	}

	Color color;
	if ( strstr( text, "FPS" ) )
	{
		m_pGraphs->GetFPSLimits( maxVal, minVal );
		color = FPS_COLOR;
	}
	else if ( strstr( text, "In" ) )
	{
		m_pGraphs->GetInLimits( maxVal, minVal );
		color = IN_COLOR;
	}
	else if ( strstr( text, "Out" ) )
	{
		m_pGraphs->GetOutLimits( maxVal, minVal );
		color = OUT_COLOR;
	}
	else if ( strstr( text, "Ping" ) )
	{
		m_pGraphs->GetPingLimits( maxVal, minVal );
		color = PING_COLOR;
	}
	else if ( strstr( text, "Players" ) )
	{
		m_pGraphs->GetPlayerLimits( maxVal, minVal );
		color = PLAYERS_COLOR;
	}
	else
	{
		return;
	}

	char maxText[20];
	char midText[20];
	char minText[20];
	sprintf( maxText, "%0.2f", maxVal );
	sprintf( midText, "%0.2f", ( maxVal - minVal ) * 0.5 );
	sprintf( minText, "%0.2f", minVal );
	SetAxisLabels( color, maxText, midText, minText );
}

void CGraphPanel::OnCheckButton()
{
	m_pGraphs->SetDraw( m_pCPUButton->IsSelected(), m_pFPSButton->IsSelected(), m_pInButton->IsSelected(),
		m_pOutButton->IsSelected(), m_pPINGButton->IsSelected(), m_pPlayerButton->IsSelected() );
}

//-----------------------------------------------------------------------------
// Purpose: clear button handler, clears the current points
//-----------------------------------------------------------------------------
void CGraphPanel::OnClearButton()
{
	m_pGraphs->RemovePoints();
}

CNonFocusableMenu::CNonFocusableMenu( vgui2::Panel* parent, const char* panelName )
	: BaseClass( parent, panelName ), m_pFocus( NULL )
{
}

vgui2::VPANEL CNonFocusableMenu::GetCurrentKeyFocus()
{
	if ( !m_pFocus )
		return GetVPanel();

	return m_pFocus->GetVPanel();
}

TabCatchingTextEntry::TabCatchingTextEntry( vgui2::Panel* parent, const char* name, vgui2::VPANEL comp )
	: vgui2::TextEntry( parent, name ), m_pCompletionList( comp )
{
	SetAllowNonAsciiCharacters( true );
}

void TabCatchingTextEntry::OnKeyCodeTyped( vgui2::KeyCode code )
{
	if ( code == vgui2::KEY_TAB || code == vgui2::KEY_UP || code == vgui2::KEY_DOWN )
		GetParent()->OnKeyCodeTyped( code );
	else
		vgui2::TextEntry::OnKeyCodeTyped( code );
}

void TabCatchingTextEntry::OnKillFocus()
{
	if ( vgui2::input()->GetFocus() != m_pCompletionList )
		PostMessage( GetParent(), new KeyValues( "CloseCompletionList" ), 0.0f );

	vgui2::TextEntry::OnKillFocus();
}

CConsolePanel::CHistoryItem::CHistoryItem() : m_text( NULL ), m_extraText( NULL ), m_bHasExtra( false )
{
}

CConsolePanel::CHistoryItem::CHistoryItem( const char* text, const char* extra )
	: m_text( NULL ), m_extraText( NULL ), m_bHasExtra( false )
{
	SetText( text, extra );
}

CConsolePanel::CHistoryItem::CHistoryItem( const CHistoryItem& src )
	: m_text( NULL ), m_extraText( NULL ), m_bHasExtra( false )
{
	SetText( src.GetText(), src.GetExtra() );
}

CConsolePanel::CHistoryItem& CConsolePanel::CHistoryItem::operator=( const CHistoryItem& src )
{
	if ( this != &src )
		SetText( src.GetText(), src.GetExtra() );
	return *this;
}

CConsolePanel::CHistoryItem::~CHistoryItem()
{
	delete[] m_text;
	delete[] m_extraText;
}

const char* CConsolePanel::CHistoryItem::GetText() const
{
	return m_text ? m_text : "";
}

const char* CConsolePanel::CHistoryItem::GetExtra() const
{
	return m_bHasExtra ? m_extraText : NULL;
}

void CConsolePanel::CHistoryItem::SetText( const char* text, const char* extra )
{
	char* newText = new char[strlen( text ) + 1];
	strcpy( newText, text );

	char* newExtra = NULL;
	if ( extra )
	{
		newExtra = new char[strlen( extra ) + 1];
		strcpy( newExtra, extra );
	}

	delete[] m_text;
	delete[] m_extraText;
	m_text = newText;
	m_extraText = newExtra;
	m_bHasExtra = extra != NULL;
}

CConsolePanel::CompletionItem::CompletionItem() : m_bIsCommand( false ), m_pText( NULL )
{
}

CConsolePanel::CompletionItem::~CompletionItem()
{
	delete m_pText;
}

const char* CConsolePanel::CompletionItem::GetItemText()
{
	static char text[256];
	text[0] = 0;
	if ( m_pText )
	{
		if ( m_pText->HasExtra() )
			_snprintf( text, sizeof( text ), "%s %s", m_pText->GetText(), m_pText->GetExtra() );
		else
			strncpy( text, m_pText->GetText(), sizeof( text ) );
		text[sizeof( text ) - 1] = 0;
	}
	return text;
}

CConsolePanel::CConsolePanel( vgui2::Panel* parent, const char* panelName, bool bStatusVersion )
	: BaseClass( parent, panelName ), m_bStatusVersion( bStatusVersion )
{
	SetKeyBoardInputEnabled( true );

	if ( !m_bStatusVersion )
		SetMinimumSize( 100, 100 );

	m_pHistory = new vgui2::RichText( this, "ConsoleHistory", true );
	m_pHistory->SetVerticalScrollbar( !m_bStatusVersion );
	m_pHistory->GotoTextEnd();

	m_pSubmit = new vgui2::Button( this, "ConsoleSubmit", "#Console_Submit" );
	m_pSubmit->SetCommand( "submit" );
	m_pSubmit->SetVisible( !m_bStatusVersion );

	m_pCompletionList = new CNonFocusableMenu( this, "CompletionList" );
	m_pCompletionList->SetVisible( false );

	m_pEntry = new TabCatchingTextEntry( this, "ConsoleEntry", m_pCompletionList->GetVPanel() );
	m_pEntry->AddActionSignalTarget( this );
	m_pEntry->SendNewLine( true );
	m_pCompletionList->SetFocusPanel( m_pEntry );

	m_PrintColor = Color( 216, 222, 211, 255 );
	m_DPrintColor = Color( 196, 181, 80, 255 );

	m_pEntry->SetTabPosition( 1 );

	m_bAutoCompleteMode = false;
	m_szPartialText[0] = 0;
	m_szPreviousPartialText[0] = 0;
	m_iNextCompletion = 0;
}

CConsolePanel::~CConsolePanel()
{
	ClearCompletionList();
	m_CommandHistory.RemoveAll();
}

void CConsolePanel::ColorPrint( const Color& clr, const char* msg )
{
	if ( m_bStatusVersion )
	{
		m_pHistory->SetText( "" );
		m_pHistory->GotoTextEnd();
	}

	m_pHistory->InsertColorChange( clr );
	m_pHistory->InsertString( msg );
}

void CConsolePanel::Print( const char* msg )
{
	ColorPrint( m_PrintColor, msg );
}

void CConsolePanel::DPrint( const char* msg )
{
	ColorPrint( m_DPrintColor, msg );
}

void CConsolePanel::ClearCompletionList()
{
	for ( int i = m_CompletionList.Count() - 1; i >= 0; i-- )
		delete m_CompletionList[i];
	m_CompletionList.RemoveAll();
}

void CConsolePanel::RebuildCompletionList( const char* text )
{
	ClearCompletionList();

	int len = (int)strlen( text );
	if ( len >= 1 )
		return;

	for ( int i = m_CommandHistory.Count() - 1; i >= 0; i-- )
	{
		CHistoryItem* item = &m_CommandHistory[i];
		CompletionItem* comp = new CompletionItem();
		m_CompletionList.AddToTail( comp );
		comp->m_bIsCommand = false;
		comp->m_pText = new CHistoryItem( *item );
	}
}

void CConsolePanel::OnAutoComplete( bool reverse )
{
	if ( !m_bAutoCompleteMode )
	{
		m_iNextCompletion = 0;
		m_bAutoCompleteMode = true;
	}

	if ( reverse )
	{
		m_iNextCompletion -= 2;
		if ( m_iNextCompletion < 0 )
			m_iNextCompletion = m_CompletionList.Count() - 1;
	}

	if ( !m_CompletionList.IsValidIndex( m_iNextCompletion ) )
		m_iNextCompletion = 0;

	if ( !m_CompletionList.IsValidIndex( m_iNextCompletion ) )
		return;

	char completedText[256];
	strncpy( completedText, m_CompletionList[m_iNextCompletion]->GetItemText(), sizeof( completedText ) - 2 );
	completedText[sizeof( completedText ) - 2] = 0;

	if ( !strstr( completedText, " " ) )
		strcat( completedText, " " );

	m_pEntry->SetText( completedText );
	m_pEntry->SelectNone();
	m_pEntry->GotoTextEnd();

	m_iNextCompletion++;
}

void CConsolePanel::OnTextChanged( vgui2::Panel* panel )
{
	if ( panel != m_pEntry )
		return;

	strncpy( m_szPreviousPartialText, m_szPartialText, sizeof( m_szPreviousPartialText ) );
	m_szPreviousPartialText[sizeof( m_szPreviousPartialText ) - 1] = 0;

	m_pEntry->GetText( m_szPartialText, sizeof( m_szPartialText ) );

	int len = (int)strlen( m_szPartialText );

	bool hitTilde = len > 0 && ( m_szPartialText[len - 1] == '~' || m_szPartialText[len - 1] == '`' );
	bool altKeyDown = vgui2::input()->IsKeyDown( vgui2::KEY_LALT ) || vgui2::input()->IsKeyDown( vgui2::KEY_RALT );
	bool ctrlKeyDown = vgui2::input()->IsKeyDown( vgui2::KEY_LCONTROL ) ||
		vgui2::input()->IsKeyDown( vgui2::KEY_RCONTROL );

	if ( hitTilde )
	{
		m_szPartialText[len - 1] = 0;

		if ( altKeyDown || ctrlKeyDown )
		{
			m_pEntry->SetText( m_szPartialText );
		}
		else
		{
			m_pEntry->SetText( "" );
			PostMessage( this, new KeyValues( "Close" ), 0.0f );
			PostActionSignal( new KeyValues( "ClosedByHittingTilde" ) );
		}
		return;
	}

	m_bAutoCompleteMode = false;

	RebuildCompletionList( m_szPartialText );

	if ( m_CompletionList.Count() < 1 )
	{
		m_pCompletionList->SetVisible( false );
	}
	else
	{
		m_pCompletionList->SetVisible( true );
		m_pCompletionList->DeleteAllItems();
		const int MAX_MENU_ITEMS = 10;

		for ( int i = 0; i < m_CompletionList.Count() && i < MAX_MENU_ITEMS; i++ )
		{
			char text[256];
			text[0] = 0;
			if ( i == MAX_MENU_ITEMS - 1 )
				strncpy( text, "...", sizeof( text ) );
			else
				strncpy( text, m_CompletionList[i]->GetItemText(), sizeof( text ) );
			text[sizeof( text ) - 1] = 0;

			KeyValues* kv = new KeyValues( "CompletionCommand" );
			kv->SetString( "command", text );
			m_pCompletionList->AddMenuItem( text, kv, this, NULL );
		}

		UpdateCompletionListPosition();
	}

	RequestFocus( 0 );
	m_pEntry->RequestFocus( 0 );
}

void CConsolePanel::UpdateCompletionListPosition()
{
	int ex, ey;
	m_pEntry->GetPos( ex, ey );

	if ( !m_bStatusVersion )
	{
		ey += m_pEntry->GetTall();
	}
	else
	{
		int menuwide, menutall;
		m_pCompletionList->GetSize( menuwide, menutall );
		ey -= menutall + 4;
	}

	LocalToScreen( ex, ey );
	m_pCompletionList->SetPos( ex, ey );

	if ( m_pCompletionList->IsVisible() )
	{
		m_pEntry->RequestFocus( 0 );
		MoveToFront();
		m_pCompletionList->MoveToFront();
	}
}

void CConsolePanel::CloseCompletionList()
{
	m_pCompletionList->SetVisible( false );
}

void CConsolePanel::OnMenuItemSelected( const char* command )
{
	if ( strstr( command, "..." ) )
	{
		m_pCompletionList->SetVisible( true );
	}
	else
	{
		m_pEntry->SetText( command );
		m_pEntry->GotoTextEnd();
		m_pEntry->InsertChar( ' ' );
		m_pEntry->GotoTextEnd();
	}
}

void CConsolePanel::OnThink()
{
	BaseClass::OnThink();

	if ( !IsVisible() )
		return;

	if ( !m_pCompletionList->IsVisible() )
		return;

	UpdateCompletionListPosition();
}

void CConsolePanel::OnKeyCodeTyped( vgui2::KeyCode code )
{
	if ( vgui2::input()->GetFocus() == m_pEntry->GetVPanel() )
	{
		if ( code == vgui2::KEY_TAB )
		{
			bool reverse = false;
			if ( vgui2::input()->IsKeyDown( vgui2::KEY_LSHIFT ) || vgui2::input()->IsKeyDown( vgui2::KEY_RSHIFT ) )
				reverse = true;

			OnAutoComplete( reverse );
			m_pEntry->RequestFocus( 0 );
			return;
		}

		if ( code == vgui2::KEY_DOWN )
		{
			OnAutoComplete( false );
			m_pEntry->RequestFocus( 0 );
			return;
		}

		if ( code == vgui2::KEY_UP )
		{
			OnAutoComplete( true );
			m_pEntry->RequestFocus( 0 );
			return;
		}
	}

	BaseClass::OnKeyCodeTyped( code );
}

void CConsolePanel::PerformLayout()
{
	BaseClass::PerformLayout();

	GetFocusNavGroup().SetDefaultButton( m_pSubmit );

	vgui2::IScheme* pScheme = vgui2::scheme()->GetIScheme( GetScheme() );
	m_pEntry->SetBorder( pScheme->GetBorder( "DepressedButtonBorder" ) );
	m_pHistory->SetBorder( pScheme->GetBorder( "DepressedButtonBorder" ) );

	int wide, tall;
	GetSize( wide, tall );

	if ( !m_bStatusVersion )
	{
		const int inset = 8;
		const int entryHeight = 24;
		const int topHeight = 4;
		const int entryInset = 4;
		const int submitWide = 64;
		const int submitInset = 7;

		m_pHistory->SetPos( inset, inset + topHeight );
		m_pHistory->SetSize( wide - inset * 2, tall - ( entryInset * 2 + inset * 2 + topHeight + entryHeight ) );
		m_pHistory->InvalidateLayout( false, false );

		int nSubmitXPos = wide - ( inset + submitWide + submitInset );
		m_pSubmit->SetPos( nSubmitXPos, tall - ( entryInset * 2 + entryHeight ) );
		m_pSubmit->SetSize( submitWide, entryHeight );

		m_pEntry->SetPos( inset, tall - ( entryInset * 2 + entryHeight ) );
		m_pEntry->SetSize( nSubmitXPos - entryInset - 2 * inset, entryHeight );
	}
	else
	{
		const int inset = 2;
		int entryWidth = wide / 2;
		if ( wide > 400 )
			entryWidth = 200;

		m_pEntry->SetBounds( inset, inset, entryWidth, tall - 2 * inset );
		m_pHistory->SetBounds( inset + entryWidth + inset, inset, wide - entryWidth - inset, tall - 2 * inset );
	}

	UpdateCompletionListPosition();
}

void CConsolePanel::ApplySchemeSettings( vgui2::IScheme* pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_PrintColor = GetSchemeColor( "Console.TextColor", pScheme );
	m_DPrintColor = GetSchemeColor( "Console.DevTextColor", pScheme );

	vgui2::HFont historyFont = pScheme->GetFont( "ConsoleText", IsProportional() );
	if ( historyFont )
		m_pHistory->SetFont( historyFont );

	vgui2::HFont listFont = pScheme->GetFont( "DefaultSmall", IsProportional() );
	if ( listFont )
		m_pCompletionList->SetFont( listFont );

	InvalidateLayout( false, false );
}

void CConsolePanel::OnCommand( const char* command )
{
	if ( !_stricmp( command, "Submit" ) )
	{
		char szCommand[256];
		m_pEntry->GetText( szCommand, sizeof( szCommand ) );
		PostActionSignal( new KeyValues( "CommandSubmitted", "command", szCommand ) );

		Print( "] " );
		Print( szCommand );
		Print( "\n" );

		m_pEntry->SetText( "" );

		OnTextChanged( m_pEntry );

		m_pHistory->GotoTextEnd();

		char* extra = strchr( szCommand, ' ' );
		if ( extra )
		{
			*extra = 0;
			extra++;
		}

		if ( strlen( szCommand ) > 0 )
			AddToHistory( szCommand, extra );

		m_pCompletionList->SetVisible( false );
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

void CConsolePanel::AddToHistory( const char* commandText, const char* extraText )
{
	while ( m_CommandHistory.Count() >= 100 )
		m_CommandHistory.Remove( 0 );

	char command[256];
	strncpy( command, commandText, sizeof( command ) - 1 );
	command[sizeof( command ) - 1] = 0;
	size_t commandLength = strlen( command );
	if ( commandLength > 0 && command[commandLength - 1] == ' ' )
		command[commandLength - 1] = 0;

	char extra[256];
	const char* extraString = NULL;
	if ( extraText )
	{
		strncpy( extra, extraText, sizeof( extra ) - 1 );
		extra[sizeof( extra ) - 1] = 0;
		int i = (int)strlen( extra ) - 1;
		while ( i >= 0 && extra[i] == ' ' )
		{
			extra[i] = 0;
			i--;
		}
		extraString = extra;
	}

	for ( int i = m_CommandHistory.Count() - 1; i >= 0; i-- )
	{
		CHistoryItem* item = &m_CommandHistory[i];

		if ( _stricmp( item->GetText(), command ) )
			continue;

		if ( extraString || item->GetExtra() )
		{
			if ( !extraString || !item->GetExtra() )
				continue;

			if ( _stricmp( item->GetExtra(), extraString ) )
				continue;
		}

		m_CommandHistory.Remove( i );
	}

	int idx = m_CommandHistory.AddToTail();
	m_CommandHistory[idx].SetText( command, extraString );

	m_iNextCompletion = 0;
	RebuildCompletionList( m_szPartialText );
}

CRawLogPanel::CRawLogPanel( vgui2::Panel* parent, const char* name ) : PropertyPage( parent, name, true )
{
	SetBuildGroup( GetBuildGroup() );
	SetSize( 200, 100 );
	m_pServerLogPanel = new CConsolePanel( this, "Console", false );
	LoadControlSettings( "Admin\\RawLogPanel.res", "PLATFORM" );
}

CRawLogPanel::~CRawLogPanel()
{
}

void CRawLogPanel::OnPageShow()
{
}

void CRawLogPanel::OnPageHide()
{
}

//-----------------------------------------------------------------------------
// Purpose: inserts a new string into the main chat panel
//-----------------------------------------------------------------------------
void CRawLogPanel::DoInsertString( const char* str )
{
	if ( str )
		m_pServerLogPanel->Print( str );
}

void CRawLogPanel::OnCommandSubmitted( const char* command )
{
	if ( command && *command )
		RemoteServer().SendCommand( command );
}

static int __cdecl FileListCompare( vgui2::ListPanel*, const vgui2::ListPanelItem& item1,
	const vgui2::ListPanelItem& item2 )
{
	KeyValues* kv1 = item1.kv;
	KeyValues* kv2 = item2.kv;
	if ( !kv1 || !kv2 )
		return 0;

	bool dir1 = kv1->GetInt( "directory" ) == 1;
	bool dir2 = kv2->GetInt( "directory" ) == 1;

	if ( dir1 != dir2 )
		return dir1 ? -1 : 1;

	return _stricmp( kv1->GetString( "text" ), kv2->GetString( "text" ) );
}

FileOpenDialog::FileOpenDialog( vgui2::Panel* parent, const char* title, bool bOpenOnly )
	: Frame( parent, "FileOpenDialog", true, false )
{
	m_bOpenOnly = bOpenOnly;
	SetTitle( title, true );
	SetMinimizeButtonVisible( false );
	SetDeleteSelfOnClose( true );
	strcpy( m_szLastPath, "c:\\" );

	m_pFullPathEdit = new vgui2::ComboBox( this, "FullPathEdit", 6, false );

	m_pFileList = new vgui2::ListPanel( this, "FileList" );
	m_pFileList->AddColumnHeader( 0, "image", "#FileOpenDialog_Icon", 20, vgui2::ListPanel::COLUMN_IMAGE );
	m_pFileList->AddColumnHeader( 1, "text", "#FileOpenDialog_Name", 300, vgui2::ListPanel::COLUMN_RESIZEWITHWINDOW );
	m_pFileList->SetSortFunc( 1, FileListCompare );
	m_pFileList->SetSortColumn( 1 );
	m_pFileList->SetMultiSelectEnabled( false );

	m_pFileNameEdit = new vgui2::TextEntry( this, "FileNameEdit" );
	m_pFileNameEdit->AddActionSignalTarget( this );

	m_pFileTypeCombo = new vgui2::ComboBox( this, "FileTypeCombo", 6, false );

	m_pOpenButton = new vgui2::Button( this, "OpenButton", "#FileOpenDialog_Open" );
	m_pCancelButton = new vgui2::Button( this, "CancelButton", "#FileOpenDialog_Cancel" );
	m_pFolderUpButton = new vgui2::Button( this, "FolderUpButton", "" );
	vgui2::Label* lookIn = new vgui2::Label( this, "LookInLabel", "#FileOpenDialog_Look_in" );
	vgui2::Label* fileName = new vgui2::Label( this, "FileNameLabel", "#FileOpenDialog_File_name" );
	vgui2::Label* fileType = new vgui2::Label( this, "FileTypeLabel", "#FileOpenDialog_File_type" );

	SetSize( 426, 260 );

	m_pFullPathEdit->SetBounds( 67, 32, 310, 24 );
	m_pFolderUpButton->SetBounds( 392, 32, 24, 24 );
	m_pFileList->SetBounds( 10, 60, 406, 130 );
	m_pFileNameEdit->SetBounds( 84, 194, 238, 24 );
	m_pFileTypeCombo->SetBounds( 84, 224, 238, 24 );
	m_pOpenButton->SetBounds( 336, 194, 74, 24 );
	m_pCancelButton->SetBounds( 336, 224, 74, 24 );
	lookIn->SetBounds( 10, 32, 55, 24 );
	fileName->SetBounds( 10, 194, 72, 24 );
	fileType->SetBounds( 10, 224, 72, 24 );

	m_pFullPathEdit->SetAutoResize( vgui2::Panel::AUTORESIZE_RIGHT );
	m_pFileNameEdit->SetAutoResize( vgui2::Panel::AUTORESIZE_RIGHT );
	m_pFileTypeCombo->SetAutoResize( vgui2::Panel::AUTORESIZE_RIGHT );
	m_pFileList->SetAutoResize( vgui2::Panel::AUTORESIZE_DOWNANDRIGHT );

	m_pFolderUpButton->SetPinCorner( vgui2::Panel::PIN_TOPRIGHT );
	m_pFileNameEdit->SetPinCorner( vgui2::Panel::PIN_BOTTOMLEFT );
	m_pFileTypeCombo->SetPinCorner( vgui2::Panel::PIN_BOTTOMLEFT );
	m_pOpenButton->SetPinCorner( vgui2::Panel::PIN_BOTTOMRIGHT );
	m_pCancelButton->SetPinCorner( vgui2::Panel::PIN_BOTTOMRIGHT );
	fileName->SetPinCorner( vgui2::Panel::PIN_BOTTOMLEFT );
	fileType->SetPinCorner( vgui2::Panel::PIN_BOTTOMLEFT );

	lookIn->SetContentAlignment( vgui2::Label::a_west );
	fileName->SetContentAlignment( vgui2::Label::a_west );
	fileType->SetContentAlignment( vgui2::Label::a_west );

	lookIn->SetAssociatedControl( m_pFullPathEdit );
	fileName->SetAssociatedControl( m_pFileNameEdit );
	fileType->SetAssociatedControl( m_pFileTypeCombo );

	GetFocusNavGroup().SetDefaultButton( m_pOpenButton );

	m_pFileNameEdit->SetTabPosition( 1 );
	m_pFileTypeCombo->SetTabPosition( 2 );
	m_pOpenButton->SetTabPosition( 3 );
	m_pCancelButton->SetTabPosition( 4 );
	m_pFullPathEdit->SetTabPosition( 5 );
	m_pFileList->SetTabPosition( 6 );

	m_pOpenButton->SetCommand( "Open" );
	m_pCancelButton->SetCommand( "Close" );
	m_pFolderUpButton->SetCommand( "FolderUp" );

	SetSize( 426, 384 );

	char localPath[1024];
	if ( !_getcwd( localPath, sizeof( localPath ) - 1 ) )
		strcpy( localPath, m_szLastPath );
	localPath[sizeof( localPath ) - 1] = 0;
	SetStartDirectory( localPath );

	PopulateDriveList();
}

FileOpenDialog::~FileOpenDialog()
{
}

void FileOpenDialog::ApplySchemeSettings( vgui2::IScheme* pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	m_pFolderUpButton->AddImage( vgui2::scheme()->GetImage( "resource/icon_folderup", false ), -3 );

	vgui2::ImageList* imageList = new vgui2::ImageList( false );
	imageList->AddImage( vgui2::scheme()->GetImage( "resource/icon_file", false ) );
	imageList->AddImage( vgui2::scheme()->GetImage( "resource/icon_folder", false ) );
	imageList->AddImage( vgui2::scheme()->GetImage( "resource/icon_folder_selected", false ) );

	m_pFileList->SetImageList( imageList, true );
}

void FileOpenDialog::PopulateDriveList()
{
	char fullpath[1024];
	char subDirPath[1024];
	GetDirectory( fullpath, sizeof( fullpath ) - 260 );
	strcpy( subDirPath, fullpath );

	m_pFullPathEdit->DeleteAllItems();

	char buf[512];
	int len = vgui2::system()->GetAvailableDrives( buf, sizeof( buf ) );
	char* pBuf = buf;
	for ( int i = 0; i < len / 4; i++ )
	{
		m_pFullPathEdit->AddItem( pBuf, NULL );

		if ( !_strnicmp( pBuf, fullpath, 2 ) )
		{
			int indent = 0;
			char* pData = fullpath;
			while ( *pData )
			{
				if ( *pData == '\\' )
				{
					if ( indent > 0 )
					{
						memset( subDirPath, ' ', indent );
						memcpy( subDirPath + indent, fullpath, pData - fullpath );
						subDirPath[indent + ( pData - fullpath )] = 0;

						m_pFullPathEdit->AddItem( subDirPath, NULL );
					}
					indent += 2;
				}
				pData++;
			}
		}
		pBuf += 4;
	}
}

void FileOpenDialog::OnClose()
{
	m_pFileNameEdit->SetText( "" );

	BaseClass::OnClose();
}

void FileOpenDialog::OnCommand( const char* command )
{
	if ( !_stricmp( command, "Open" ) )
		OnOpen();
	else if ( !_stricmp( command, "FolderUp" ) )
		MoveUpFolder();
	else
		BaseClass::OnCommand( command );
}

void FileOpenDialog::SetStartDirectory( const char* dir )
{
	m_pFullPathEdit->SetText( dir );

	ValidatePath();
	PopulateDriveList();
}

void FileOpenDialog::AddFilter( const char* filter, const char* filterName, bool bActive )
{
	KeyValues* kv = new KeyValues( "item" );
	kv->SetString( "filter", filter );
	int itemID = m_pFileTypeCombo->AddItem( filterName, kv );
	kv->deleteThis();
	if ( bActive )
		m_pFileTypeCombo->ActivateItem( itemID );
}

void FileOpenDialog::DoModal( bool )
{
	int swide, stall, wide, tall;
	vgui2::surface()->GetScreenSize( swide, stall );
	GetSize( wide, tall );
	SetPos( ( swide - wide ) / 2, ( stall - tall ) / 2 );

	SetVisible( true );
	SetEnabled( true );
	MoveToFront();

	m_pFileNameEdit->RequestFocus( 0 );

	PopulateFileList();
	InvalidateLayout( false, false );
	RequestFocus( 0 );
}

void FileOpenDialog::GetDirectory( char* buf, int bufSize )
{
	m_pFullPathEdit->GetText( buf, bufSize );
}

void FileOpenDialog::GetSelectedFileName( char* buf, int bufSize )
{
	m_pFileNameEdit->GetText( buf, bufSize );
}

void FileOpenDialog::MoveUpFolder()
{
	char fullpath[1024];
	GetDirectory( fullpath, sizeof( fullpath ) - 260 );

	char* pos = strrchr( fullpath, '\\' );
	if ( pos )
	{
		*pos = 0;

		if ( !pos[1] )
		{
			pos = strrchr( fullpath, '\\' );
			if ( pos )
				*pos = 0;
		}
	}

	strcat( fullpath, "\\" );

	SetStartDirectory( fullpath );
	PopulateFileList();
	InvalidateLayout( false, false );
	Repaint();
}

void FileOpenDialog::ValidatePath()
{
	char fullpath[1024];
	GetDirectory( fullpath, sizeof( fullpath ) - 260 );

	char* pos = strrchr( fullpath, '\\' );
	if ( !( pos && !pos[1] ) )
		strcat( fullpath, "\\" );

	strcat( fullpath, "*" );
	char* pData = fullpath;
	while ( *pData == ' ' )
		pData++;

	_finddata_t findData;
	intptr_t findHandle = _findfirst( pData, &findData );
	if ( findHandle != -1 )
	{
		char* star = strrchr( pData, '*' );
		if ( star )
			*star = 0;
		strncpy( m_szLastPath, pData, sizeof( m_szLastPath ) - 1 );
		m_szLastPath[sizeof( m_szLastPath ) - 1] = 0;
		_findclose( findHandle );
	}
	else
	{
		strcpy( pData, m_szLastPath );
	}

	m_pFullPathEdit->SetText( pData );
}

void FileOpenDialog::PopulateFileList()
{
	m_pFileList->DeleteAllItems();

	char dir[1024];
	GetDirectory( dir, sizeof( dir ) - 260 );

	KeyValues* combokv = m_pFileTypeCombo->GetActiveItemUserData();
	if ( combokv )
		strcat( dir, combokv->GetString( "filter", "*" ) );
	else
		strcat( dir, "*" );

	KeyValues* kv = new KeyValues( "item" );

	_finddata_t findData;
	intptr_t findHandle = _findfirst( dir, &findData );
	while ( findHandle != -1 )
	{
		kv->SetString( "text", findData.name );

		if ( findData.attrib & _A_SUBDIR )
		{
			kv->SetInt( "image", 2 );
			kv->SetInt( "directory", 1 );
		}
		else
		{
			kv->SetInt( "image", 1 );
			kv->SetInt( "directory", 0 );
		}

		if ( strcmp( findData.name, "." ) && strcmp( findData.name, ".." ) )
			m_pFileList->AddItem( kv, 0, false, false );

		if ( _findnext( findHandle, &findData ) != 0 )
			break;
	}
	if ( findHandle != -1 )
		_findclose( findHandle );

	kv->deleteThis();
	m_pFileList->SortList();
}

void FileOpenDialog::OnOpen()
{
	ValidatePath();

	char filename[260];
	GetSelectedFileName( filename, sizeof( filename ) );
	char fullpath[1024];
	GetDirectory( fullpath, sizeof( fullpath ) - 260 );
	strcat( fullpath, filename );

	if ( !_stricmp( filename, ".." ) )
	{
		MoveUpFolder();
		m_pFileNameEdit->SetText( "" );
		return;
	}

	_finddata_t findData;
	intptr_t findHandle = _findfirst( fullpath, &findData );
	if ( findHandle != -1 )
	{
		_findclose( findHandle );

		if ( findData.attrib & _A_SUBDIR )
		{
			strcat( fullpath, "\\" );
			SetStartDirectory( fullpath );
			m_pFileNameEdit->SetText( "" );
		}
		else
		{
			PostActionSignal( new KeyValues( "FileSelected", "fullpath", fullpath ) );
			PostMessage( this, new KeyValues( "Close" ), 0.0f );
			return;
		}
	}
	else if ( m_bOpenOnly && strlen( filename ) > 0 )
	{
		PostActionSignal( new KeyValues( "FileSelected", "fullpath", fullpath ) );
		PostMessage( this, new KeyValues( "Close" ), 0.0f );
		return;
	}

	PopulateDriveList();
	PopulateFileList();
	InvalidateLayout( false, false );
}

void FileOpenDialog::OnItemSelected()
{
	if ( m_pFileList->GetSelectedItemsCount() != 1 )
	{
		m_pFileNameEdit->SetText( "" );
	}
	else
	{
		KeyValues* data = m_pFileList->GetItem( m_pFileList->GetSelectedItem( 0 ) );
		if ( data )
			m_pFileNameEdit->SetText( data->GetString( "text" ) );
	}

	InvalidateLayout( false, false );
}

void FileOpenDialog::OnTextChanged( KeyValues* kv )
{
	vgui2::Panel* pPanel = (vgui2::Panel*)kv->GetPtr( "panel", NULL );

	if ( pPanel == m_pFullPathEdit )
	{
		m_pFileNameEdit->SetText( "" );
		OnOpen();
	}
	else if ( pPanel == m_pFileTypeCombo )
	{
		PopulateFileList();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGamePanelInfo::CGamePanelInfo( vgui2::Panel* parent, const char* name, const char* mod )
	: Frame( parent, name, true, false )
{
	SetSize( 560, 420 );
	SetMinimumSize( 560, 420 );

	m_bRemoteServer = false;
	m_bShuttingDown = false;

	m_pAnimImagePanel = new vgui2::AnimatingImagePanel( this, "AnAnimatingImagePanel" );
	m_pAnimImagePanel->LoadAnimation( "resource\\steam\\g", 12 );
	m_pAnimImagePanel->SetVisible( false );

	m_pDetailsSheet = new vgui2::PropertySheet( this, "Panels" );

	m_pPlayerListPanel = new CPlayerPanel( this, "Player List" );
	m_pBanListPanel = new CBanPanel( this, "Ban List" );
	m_pServerLogPanel = new CRawLogPanel( this, "ServerLog" );
	m_pServerConfigPanel = new CServerConfigPanel( this, "ServerConfigPanel", mod );
	m_pGraphsPanel = new CGraphPanel( this, "GraphsPanel" );
	m_pServerInfoPanel = new CServerInfoPanel( this, "ServerInfo" );

	m_pServerInfoPanel->AddActionSignalTarget( this );

	m_pDetailsSheet->AddPage( m_pServerInfoPanel, "#Game_Main_Settings" );
	m_pDetailsSheet->AddPage( m_pServerConfigPanel, "#Game_Configure" );
	m_pDetailsSheet->AddPage( m_pGraphsPanel, "#Game_Server_Statistics" );
	m_pDetailsSheet->AddPage( m_pPlayerListPanel, "#Game_Current_Players" );
	m_pDetailsSheet->AddPage( m_pBanListPanel, "#Game_Bans" );
	m_pDetailsSheet->AddPage( m_pServerLogPanel, "#Game_Console" );

	// let us be ticked every frame
	vgui2::ivgui()->AddTickSignal( GetVPanel(), 0 );

	LoadControlSettingsAndUserConfig( "Admin\\DialogGamePanelInfo.res", 0 );

	wchar_t serverName[256];
	vgui2::localize()->ConvertANSIToUnicode( name, serverName, sizeof( serverName ) );
	wchar_t title[256];
	vgui2::localize()->ConstructString( title, sizeof( title ),
		vgui2::localize()->Find( m_bRemoteServer ? "#Game_RemoteTitle" : "Game_LocalTitle" ), 1, serverName );
	SetTitle( title, true );

	SetVisible( true );
	MoveToCenterOfScreen();
	RequestFocus( 0 );
	MoveToFront();

	PostMessage( m_pServerInfoPanel, new KeyValues( "ResetData" ), 0.1f );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGamePanelInfo::~CGamePanelInfo()
{
}

void CGamePanelInfo::ShowPage()
{
	Activate();
}

void CGamePanelInfo::SetAsRemoteServer( bool remote )
{
	m_bRemoteServer = remote;
}

void CGamePanelInfo::AddToConsole( const char* msg )
{
	if ( !m_pServerLogPanel )
		return;

	if ( *msg == 3 && !strncmp( msg + 1, "MasterRequestRestart", strlen( "MasterRequestRestart" ) ) )
	{
		if ( !m_hRestartQueryBox.Get() )
		{
			m_hRestartQueryBox = new vgui2::QueryBox( "Server restart needed",
				"Your server is out of date, and will not be listed\non the master server until you restart.\n\n"
				"Do you wish to shutdown now?\n", NULL );
			UseUnscaledLayout( m_hRestartQueryBox );
			m_hRestartQueryBox->AddActionSignalTarget( this );
			m_hRestartQueryBox->SetOKCommand( new KeyValues( "RestartServer" ) );
			m_hRestartQueryBox->DoModal( NULL );
		}
		else
		{
			m_hRestartQueryBox->Activate();
		}
	}
	else if ( *msg == 3 && !strncmp( msg + 1, "MasterOutOfDate", strlen( "MasterOutOfDate" ) ) )
	{
		const char* details = strstr( msg, "MasterOutOfDate" );
		if ( details )
			ShowMasterOutOfDateMessage( details + strlen( "MasterOutOfDate" ) );
	}
	else
	{
		m_pServerLogPanel->DoInsertString( msg );
	}
}

void CGamePanelInfo::ShowMasterOutOfDateMessage( const char* details )
{
	if ( m_hOutOfDateQueryBox.Get() )
	{
		m_hOutOfDateQueryBox->Activate();
		return;
	}

	CUtlVector<char> fullmsg;
	int size = (int)strlen( details ) + 32;
	fullmsg.AddMultipleToTail( size );
	_snprintf( fullmsg.Base(), size, "%s\n\nDo you wish to shutdown now?\n", details );
	fullmsg[size - 1] = 0;

	m_hOutOfDateQueryBox = new vgui2::QueryBox( "Server restart pending", fullmsg.Base(), NULL );
	UseUnscaledLayout( m_hOutOfDateQueryBox );
	m_hOutOfDateQueryBox->AddActionSignalTarget( this );
	m_hOutOfDateQueryBox->SetOKCommand( new KeyValues( "RestartServer" ) );
	m_hOutOfDateQueryBox->DoModal( NULL );
}

void CGamePanelInfo::OnTick()
{
	RemoteServer().ProcessServerResponse();
}

void CGamePanelInfo::OnCommand( const char* command )
{
	if ( !_stricmp( command, "stop2" ) )
	{
		RemoteServer().SendCommand( "quit" );
		m_bShuttingDown = true;
		Close();
		return;
	}

	BaseClass::OnCommand( command );
}

void CGamePanelInfo::OnClose()
{
	if ( m_bRemoteServer || m_bShuttingDown )
	{
		BaseClass::OnClose();
		return;
	}

	vgui2::QueryBox* box = new vgui2::QueryBox( "#Game_Stop_Server_Title", "#Game_Restart_Server", NULL );
	UseUnscaledLayout( box );
	box->AddActionSignalTarget( this );
	box->SetOKButtonText( "#Game_Stop_Server" );
	box->SetOKCommand( new KeyValues( "Command", "command", "stop2" ) );
	box->DoModal( NULL );
}

void CGamePanelInfo::ActivateBuildMode()
{
	vgui2::EditablePanel* pg = dynamic_cast<vgui2::EditablePanel*>( m_pDetailsSheet->GetActivePage() );
	if ( pg )
		pg->ActivateBuildMode();
}

//-----------------------------------------------------------------------------
// Purpose: displays the help page
//-----------------------------------------------------------------------------
void CGamePanelInfo::OnHelp()
{
	vgui2::system()->ShellExecute( "open", "Admin\\Admin.html" );
}

void CGamePanelInfo::OnRestartServer()
{
	RemoteServer().SendCommand( "quit" );
	m_bShuttingDown = true;
	Close();
}

void CGamePanelInfo::OnUpdateTitle()
{
	wchar_t serverName[256];
	vgui2::localize()->ConvertANSIToUnicode( m_pServerInfoPanel->GetHostname(), serverName, sizeof( serverName ) );
	wchar_t title[256];
	vgui2::localize()->ConstructString( title, sizeof( title ),
		vgui2::localize()->Find( m_bRemoteServer ? "#Game_RemoteTitle" : "Game_LocalTitle" ), 1, serverName );
	SetTitle( title, true );
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CAdminServer::CAdminServer()
{
	m_hParent = 0;
	OpenedManageDialog_t empty = { 0, NULL };
	m_OpenedManageDialog.AddToTail( empty );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CAdminServer::~CAdminServer()
{
}

//-----------------------------------------------------------------------------
// Purpose: links to vgui and engine interfaces
//-----------------------------------------------------------------------------
bool CAdminServer::Initialize( CreateInterfaceFn* factorylist, int factoryCount )
{
	for ( int i = 0; i < factoryCount; i++ )
	{
		if ( !g_pGameServerData && factorylist[i] )
			g_pGameServerData = (IGameServerData*)( factorylist[i] )( GAMESERVERDATA_INTERFACE_VERSION, NULL );
	}

	RemoteServer().Initialize();
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: links to other modules interfaces (tracker)
//-----------------------------------------------------------------------------
bool CAdminServer::PostInitialize( CreateInterfaceFn*, int )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAdminServer::IsValid()
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAdminServer::Activate()
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: returns direct handle to main server browser dialog
//-----------------------------------------------------------------------------
vgui2::VPANEL CAdminServer::GetPanel()
{
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Closes down the server browser for good
//-----------------------------------------------------------------------------
void CAdminServer::Shutdown()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAdminServer::Deactivate()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAdminServer::Reactivate()
{
}

void CAdminServer::SetParent( vgui2::VPANEL parent )
{
	m_hParent = parent;
}

void CAdminServer::RunFrame()
{
}

ManageServerUIHandle_t CAdminServer::OpenManageServerDialog( const char* serverName, const char* gameDir )
{
	CGamePanelInfo* tab = new CGamePanelInfo( NULL, serverName, gameDir );
	tab->SetParent( m_hParent );

	int i = m_OpenedManageDialog.AddToTail();
	m_OpenedManageDialog[i].handle = vgui2::ivgui()->PanelToHandle( tab->GetVPanel() );
	m_OpenedManageDialog[i].manageInterface = tab;

	// return safe handle to the panel
	return (ManageServerUIHandle_t)i;
}

ManageServerUIHandle_t CAdminServer::OpenManageServerDialog( unsigned int, unsigned int, const char* )
{
	return (ManageServerUIHandle_t)0;
}

void CAdminServer::CloseManageServerDialog( ManageServerUIHandle_t )
{
}

IManageServer* CAdminServer::GetManageServerInterface( ManageServerUIHandle_t handle )
{
	if ( handle > 0 && (int)handle < m_OpenedManageDialog.Count() &&
		vgui2::ivgui()->HandleToPanel( m_OpenedManageDialog[handle].handle ) )
	{
		return m_OpenedManageDialog[handle].manageInterface;
	}

	return NULL;
}
