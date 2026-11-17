#include "TutorTextWindow.h"
#include "Frame.h"
#include "Label.h"
#include "BitmapImagePanel.h"
#include "../controls/controls.h"
#include "../../engine/controls.h"
#include "iviewport.h"
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include <vgui/IVGui.h>
#include <stdio.h>
#include <stdarg.h>

using namespace vgui2;

void DebugLog( const char* format, ... )
{
	char message[512];
	va_list arguments;
	va_start( arguments, format );
	vsnprintf( message, sizeof( message ), format, arguments );
	va_end( arguments );
	ivgui()->DPrintf( "%s", message );
}

class CTutorMessageBox : public Frame
{
public:
	CTutorMessageBox( const wchar_t* message, int msgClass, int hardwareMode );
	void PaintBackground() override;
	void PerformLayout() override;
	void ApplySchemeSettings( IScheme* pScheme ) override;
	void OnThink() override;
	void SetAnimating( bool animating ) { m_isAnimating = animating; }

private:
	int					m_msgClass;
	Label*				m_pMessageLabel;
	bool				m_isAnimating;
	int					m_hardwareMode;
	CBitmapImagePanel*	m_pTopLeftPanel;
	CBitmapImagePanel*	m_pTopRightPanel;
	CBitmapImagePanel*	m_pBottomLeftPanel;
	CBitmapImagePanel*	m_pBottomRightPanel;
	Panel*				m_pGapPanel;
	CBitmapImagePanel*	m_pIconPanel;
	Color				m_bgColor;
	Color				m_borderColor;
};

CTutorMessageBox::CTutorMessageBox( const wchar_t* message, int msgClass, int hardwareMode )
	: Frame( NULL, "TutorTextWindow", false, false )
{
	m_hardwareMode = hardwareMode;
	m_msgClass = msgClass;
	m_bgColor = Color( 0, 0, 0, 0 );
	m_borderColor = Color( 0, 0, 0, 0 );
	m_pMessageLabel = new Label( this, "TutorMessageLabel", message );
	m_pTopLeftPanel = new CBitmapImagePanel( this, "TopLeftPanel", "gfx/career/round_corner_nw" );
	m_pTopRightPanel = new CBitmapImagePanel( this, "TopRightPanel", "gfx/career/round_corner_ne" );
	m_pBottomLeftPanel = new CBitmapImagePanel( this, "BottomLeftPanel", "gfx/career/round_corner_sw" );
	m_pBottomRightPanel = new CBitmapImagePanel( this, "BottomRightPanel", "gfx/career/round_corner_se" );
	m_pGapPanel = new Panel( this, "GapPanel" );
	m_pIconPanel = new CBitmapImagePanel( this, "IconPanel",
		msgClass == 2 || msgClass == 4 ? "gfx/career/icon_skulls" : "gfx/career/icon_i" );
	SetTitle( "", true );
	SetMoveable( false );
	SetSizeable( false );
	SetTitleBarVisible( false );
	SetScheme( "TutorScheme" );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
	SetMenuButtonVisible( false );
	SetVisible( true );
	SetEnabled( true );
	m_isAnimating = false;
	LoadControlSettings( "Resource/UI/TutorTextWindow.res", NULL );
	m_pMessageLabel->SetText( message );
	MoveToFront();
	InvalidateLayout( false, false );
}

void CTutorMessageBox::OnThink()
{
	Panel::OnThink();
	int x, y, left, top, screenwidth, screenheight;
	GetPos( x, y );
	surface()->GetWorkspaceBounds( left, top, screenwidth, screenheight );
	int xpos = screenwidth - GetWide() - 10;
	int ypos = 10;
	if ( gViewPortInterface->IsSpectatorGUIVisible() )
		ypos = gViewPortInterface->GetSpectatorTopBarHeight() + 10;
	if ( m_isAnimating )
	{
		if ( ypos != y )
			SetPos( x, ypos );
	}
	else if ( xpos != x || ypos != y )
	{
		SetPos( xpos, ypos );
	}
}

void CTutorMessageBox::PerformLayout()
{
	int x, y, w, h, oldWide, oldTall, newWide, newTall;
	GetBounds( x, y, w, h );
	m_pMessageLabel->GetSize( oldWide, oldTall );
	m_pMessageLabel->SizeToContents();
	m_pMessageLabel->GetSize( newWide, newTall );
	if ( m_pIconPanel->GetTall() > newTall )
	{
		newTall = m_pIconPanel->GetTall();
		m_pMessageLabel->SetSize( m_pMessageLabel->GetWide(), newTall );
	}
	int diffWide = newWide - oldWide;
	int diffTall = newTall - oldTall;
	if ( diffWide )
	{
		m_pTopRightPanel->GetPos( x, y );
		m_pTopRightPanel->SetPos( x + diffWide, y );
		m_pBottomRightPanel->GetPos( x, y );
		m_pBottomRightPanel->SetPos( x + diffWide, y );
	}
	if ( diffTall )
	{
		m_pBottomLeftPanel->GetPos( x, y );
		m_pBottomLeftPanel->SetPos( x, y + diffTall );
		m_pBottomRightPanel->GetPos( x, y );
		m_pBottomRightPanel->SetPos( x, y + diffTall );
		m_pGapPanel->SetTall( m_pGapPanel->GetTall() + diffTall );
	}
	SetSize( w + diffWide, h + diffTall );
}

void CTutorMessageBox::ApplySchemeSettings( IScheme* pScheme )
{
	Frame::ApplySchemeSettings( pScheme );
	char colorName[64];
	_snprintf( colorName, sizeof( colorName ), "TutorTextClass%d", m_msgClass );
	Color textColor = pScheme->GetColor( colorName,
		pScheme->GetColor( "TutorTextClassDefault", Color( 0, 0, 0, 0 ) ) );
	m_pMessageLabel->SetFgColor( textColor );
	m_pMessageLabel->SetContentAlignment( Label::a_west );
	m_bgColor = pScheme->GetColor( "WindowBG", Color( 0, 0, 0, 200 ) );
	m_borderColor = pScheme->GetColor( "BorderBright", Color( 0, 0, 0, 0 ) );
	Color cornerColor( 255, 255, 255, m_bgColor.a() );
	m_pTopLeftPanel->m_bgColor = cornerColor;
	m_pTopRightPanel->m_bgColor = cornerColor;
	m_pBottomLeftPanel->m_bgColor = cornerColor;
	m_pBottomRightPanel->m_bgColor = cornerColor;
	m_pIconPanel->m_bgColor = textColor;
	m_pTopLeftPanel->SetVisible( m_hardwareMode != 0 );
	m_pTopRightPanel->SetVisible( m_hardwareMode != 0 );
	m_pBottomLeftPanel->SetVisible( m_hardwareMode != 0 );
	m_pBottomRightPanel->SetVisible( m_hardwareMode != 0 );
	SetBorder( NULL );
}

void CTutorMessageBox::PaintBackground()
{
	int x1, y1, x2, y2;
	surface()->DrawSetColor( m_bgColor );
	if ( !m_hardwareMode )
	{
		m_pTopLeftPanel->GetPos( x1, y1 );
		m_pBottomRightPanel->GetPos( x2, y2 );
		x2 += m_pBottomRightPanel->GetWide();
		y2 += m_pBottomRightPanel->GetTall();
		surface()->DrawFilledRect( x1, y1, x2, y2 );
		surface()->DrawSetColor( m_borderColor );
		surface()->DrawOutlinedRect( x1, y1, x2, y2 );
		return;
	}

	if ( m_pGapPanel->GetTall() > m_pGapPanel->GetWide() )
	{
		m_pTopLeftPanel->GetPos( x1, y1 );
		m_pBottomLeftPanel->GetPos( x2, y2 );
		y1 += m_pTopLeftPanel->GetTall();
		x2 += m_pBottomLeftPanel->GetWide();
		surface()->DrawFilledRect( x1, y1, x2, y2 );
		m_pTopLeftPanel->GetPos( x1, y1 );
		m_pGapPanel->GetPos( x2, y2 );
		x1 += m_pTopLeftPanel->GetWide();
		y2 += m_pGapPanel->GetTall();
		surface()->DrawFilledRect( x1, y1, x2, y2 );
		m_pGapPanel->GetPos( x1, y1 );
		m_pBottomRightPanel->GetPos( x2, y2 );
		x1 += m_pGapPanel->GetWide();
		y2 += m_pBottomRightPanel->GetTall();
		surface()->DrawFilledRect( x1, y1, x2, y2 );
		m_pTopRightPanel->GetPos( x1, y1 );
		m_pBottomRightPanel->GetPos( x2, y2 );
		y1 += m_pTopRightPanel->GetTall();
		x2 += m_pBottomRightPanel->GetWide();
	}
	else
	{
		m_pTopLeftPanel->GetPos( x1, y1 );
		m_pTopRightPanel->GetPos( x2, y2 );
		x1 += m_pTopLeftPanel->GetWide();
		y2 += m_pTopRightPanel->GetTall();
		surface()->DrawFilledRect( x1, y1, x2, y2 );
		m_pTopLeftPanel->GetPos( x1, y1 );
		m_pGapPanel->GetPos( x2, y2 );
		y1 += m_pTopLeftPanel->GetTall();
		x2 += m_pGapPanel->GetWide();
		surface()->DrawFilledRect( x1, y1, x2, y2 );
		m_pGapPanel->GetPos( x1, y1 );
		m_pBottomRightPanel->GetPos( x2, y2 );
		y1 += m_pGapPanel->GetTall();
		x2 += m_pBottomRightPanel->GetWide();
		surface()->DrawFilledRect( x1, y1, x2, y2 );
		m_pBottomLeftPanel->GetPos( x1, y1 );
		m_pBottomRightPanel->GetPos( x2, y2 );
		x1 += m_pBottomLeftPanel->GetWide();
		y2 += m_pBottomRightPanel->GetTall();
	}
	surface()->DrawFilledRect( x1, y1, x2, y2 );
}

CTutorTextWindow::CTutorTextWindow( int hardwareMode )
{
	m_messageBox = NULL;
	m_spectatorPosition = 0;
	m_hardwareMode = hardwareMode;
}

CTutorTextWindow::~CTutorTextWindow()
{
	if ( m_messageBox )
	{
		int wide = 0, tall = 0;
		m_messageBox->GetSize( wide, tall );
		m_messageBox->MarkForDeletion();
		m_messageBox = NULL;
	}
}

void CTutorTextWindow::Activate( const wchar_t* message, int msgClass, int isSpectator )
{
	int x, y, screenwidth, screenheight;
	surface()->GetWorkspaceBounds( x, y, screenwidth, screenheight );
	m_messageBox = new CTutorMessageBox( message, msgClass, m_hardwareMode );
	if ( !m_messageBox )
		return;

	m_spectatorPosition = isSpectator;
	int wide = 0, tall = 0;
	m_messageBox->GetSize( wide, tall );
	int ypos = ComputeWindowHeight( screenheight );
	if ( ypos )
		m_messageBox->SetPos( screenwidth - wide - 10, ypos );
}

void CTutorTextWindow::AnimateOffscreen()
{
	if ( !m_messageBox )
		return;

	int x, y, screenwidth, screenheight;
	surface()->GetWorkspaceBounds( x, y, screenwidth, screenheight );
	int ypos = ComputeWindowHeight( screenheight );
	if ( ypos )
	{
		int wide = 0, tall = 0;
		m_messageBox->GetSize( wide, tall );
		m_messageBox->SetAnimating( true );
		m_messageBox->SetPos( screenwidth - wide / 2, ypos );
	}
}

void CTutorTextWindow::GetPosition( int& x, int& y )
{
	if ( m_messageBox )
		m_messageBox->GetPos( x, y );
	else
		x = y = -1;
}

int CTutorTextWindow::ComputeWindowHeight( int screenheight )
{
	if ( !m_messageBox )
		return 0;

	int wide = 0, tall = 0;
	m_messageBox->GetSize( wide, tall );
	if ( m_spectatorPosition )
	{
		int x, y, screenwidth;
		surface()->GetWorkspaceBounds( x, y, screenwidth, screenheight );
		return screenheight / 8;
	}
	return 10;
}
