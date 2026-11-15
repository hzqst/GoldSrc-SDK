#include "DemoPlayerFileDialog.h"
#include "CreateMultiplayerGameServerPage.h"

#include "Button.h"
#include "ListPanel.h"
#include "filesystem.h"
#include "utlrbtree.h"
#include "utlsymbol.h"
#include "vgui/ISurface.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"

#include <string.h>

CDemoPlayerFileDialog::CDemoPlayerFileDialog( vgui2::Panel* parent, const char* name ) :
		vgui2::Frame( parent, name, true, false )
{
	SetHDProportional( true );
	SetBounds( 0, 0, GetProportionalScaledValue( 278 ), GetProportionalScaledValue( 380 ) );
	SetSizeable( false );
	vgui2::surface()->CreatePopup( GetVPanel(), false, true, false, true, true );
	SetTitle( "#GameUI_LoadDemo", true );
	m_pDemoList = new vgui2::ListPanel( this, "DemoList" );
	new vgui2::Button( this, "LoadButton", "#GameUI_Load" );
	new vgui2::Button( this, "CancelButton", "#GameUI_Cancel" );
	LoadControlSettings( "Resource/DemoPlayerFileDialog.res", NULL );
	m_pDemoList->AddColumnHeader( 0, "demoname", "#GameUI_DemoFile", m_pDemoList->GetWide(), 0 );
	LoadDemoList();
}

CDemoPlayerFileDialog::~CDemoPlayerFileDialog()
{
}

void CDemoPlayerFileDialog::LoadDemoList()
{
	CUtlRBTree<CUtlSymbol, short> demoNames( 0, 0, CaselessStringLessThan );
	m_pDemoList->DeleteAllItems();
	// iterate the filesystem getting the list of all the files
	// UNDONE: steam wants this done in a special way, need to support that
	FileFindHandle_t findHandle = 0;
	const char* filename = vgui2::filesystem()->FindFirst( "*.dem", &findHandle, NULL );
	while ( filename )
	{
		if ( demoNames.Find( CUtlSymbol( filename ) ) == demoNames.InvalidIndex() )
			demoNames.Insert( CUtlSymbol( filename ) );

		// get the next file
		filename = vgui2::filesystem()->FindNext( findHandle );
	}
	vgui2::filesystem()->FindClose( findHandle );

	for ( short index = demoNames.FirstInorder(); index != demoNames.InvalidIndex();
			index = demoNames.NextInorder( index ) )
	{
		KeyValues* data = new KeyValues( "data", "demoname", demoNames[index].String() );
		m_pDemoList->AddItem( data, 0, false, false );
	}
	demoNames.RemoveAll();
	if ( m_pDemoList->GetItemCount() > 0 )
		m_pDemoList->SetSingleSelectedItem( m_pDemoList->GetItemIDFromRow( 0 ) );
}

void CDemoPlayerFileDialog::OnClose()
{
	BaseClass::OnClose();
	MarkForDeletion();
}

void CDemoPlayerFileDialog::OnCommand( const char* command )
{
	if ( !strcmp( command, "load" ) )
	{
		// get first selected item
		int selected = m_pDemoList->GetSelectedItem( 0 );
		if ( m_pDemoList->IsValidItemID( selected ) )
		{
			KeyValues* item = m_pDemoList->GetItem( selected );
			PostActionSignal( new KeyValues( "DemoSelected", "demoname", item->GetString( "demoname", "" ) ) );
		}
		OnClose();
	}
	BaseClass::OnCommand( command );
}

void CDemoPlayerFileDialog::OnItemDoubleClicked()
{
	OnCommand( "load" );
}
