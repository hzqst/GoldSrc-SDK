#include "TutorStateWindow.h"
#include "../controls/MessageBox.h"
#include "../controls/controls.h"
#include "../../engine/controls.h"
#include "vgui/ISurface.h"

using namespace vgui2;

CTutorStateWindow::CTutorStateWindow()
{
	m_messageBox = NULL;
}

CTutorStateWindow::~CTutorStateWindow()
{
	if ( m_messageBox )
	{
		int wide = 0, tall = 0;
		m_messageBox->GetSize( wide, tall );
		delete m_messageBox;
	}
}

void CTutorStateWindow::Activate( const char* message )
{
	int x, y, screenwidth, screenheight;
	surface()->GetWorkspaceBounds( x, y, screenwidth, screenheight );
	m_messageBox = new MessageBox( "", message, NULL );
	if ( !m_messageBox )
		return;

	m_messageBox->SetOKButtonVisible( false );
	m_messageBox->SetMouseInputEnabled( false );
	m_messageBox->SetKeyBoardInputEnabled( false );
	m_messageBox->SetScheme( "TutorScheme" );
	m_messageBox->SetMenuButtonVisible( false );
	m_messageBox->ShowWindow( NULL );
	int wide = 0, tall = 0;
	m_messageBox->GetSize( wide, tall );
	x = y = 0;
	ComputeWindowPos( screenwidth, screenheight, x, y );
	m_messageBox->SetPos( x, y );
}

void CTutorStateWindow::Activate( const wchar_t* message )
{
	int x, y, screenwidth, screenheight;
	surface()->GetWorkspaceBounds( x, y, screenwidth, screenheight );
	m_messageBox = new MessageBox( L"", message, NULL );
	if ( !m_messageBox )
		return;

	m_messageBox->SetOKButtonVisible( false );
	m_messageBox->SetMouseInputEnabled( false );
	m_messageBox->SetKeyBoardInputEnabled( false );
	m_messageBox->SetScheme( "TutorScheme" );
	m_messageBox->SetMenuButtonVisible( false );
	m_messageBox->ShowWindow( NULL );
	int wide = 0, tall = 0;
	m_messageBox->GetSize( wide, tall );
	x = y = 0;
	ComputeWindowPos( screenwidth, screenheight, x, y );
	m_messageBox->SetPos( x, y );
}

void CTutorStateWindow::UpdatePosition()
{
	if ( !m_messageBox )
		return;

	int x, y, screenwidth, screenheight;
	surface()->GetWorkspaceBounds( x, y, screenwidth, screenheight );
	int xpos = 0, ypos = 0;
	ComputeWindowPos( screenwidth, screenheight, xpos, ypos );
	int wide = 0, tall = 0;
	m_messageBox->GetSize( wide, tall );
	x = y = 0;
	m_messageBox->GetPos( x, y );
	if ( x != xpos || y != ypos )
		m_messageBox->SetPos( xpos, ypos );
}

void CTutorStateWindow::GetPosition( int& x, int& y )
{
	if ( m_messageBox )
		m_messageBox->GetPos( x, y );
	else
		x = y = -1;
}

void CTutorStateWindow::ComputeWindowPos( int screenwidth, int, int& xpos, int& ypos )
{
	int wide = 0, tall = 0;
	if ( m_messageBox )
	{
		m_messageBox->GetSize( wide, tall );
		xpos = screenwidth / 2 - wide / 2;
		ypos = 10;
	}
}
