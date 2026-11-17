#include "czeroviewport.h"

#include "wrect.h"
#include "cl_dll.h"
#include "vgui_worldmap.h"

ICzeroViewPort* gCzeroViewPortInterface;
EXPOSE_SINGLE_INTERFACE( CZEROViewPort, IClientVGUI, CLIENTVGUI_INTERFACE_VERSION );

#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
CZEROViewPort::CZEROViewPort()
{
	gCzeroViewPortInterface = this;
}

CZEROViewPort::~CZEROViewPort()
{
}

void CZEROViewPort::Initialize( CreateInterfaceFn* factories, int count )
{
	TeamFortressViewport::Initialize( factories, count );
}

void CZEROViewPort::Start()
{
	TeamFortressViewport::Start();
	m_pWorldMap = new CWorldMap( NULL );
}

void CZEROViewPort::SetParent( vgui2::VPANEL parent )
{
	TeamFortressViewport::SetParent( parent );
	if ( m_pWorldMap )
		m_pWorldMap->SetParent( parent );
}

int CZEROViewPort::KeyInput( int, int, const char* )
{
	return 1;
}

void CZEROViewPort::UpdateSpectatorPanel()
{
}

void CZEROViewPort::OnLevelChange()
{
	TeamFortressViewport::OnLevelChange();
	HideWorldMap();
}

void CZEROViewPort::ActivateClientUI()
{
	TeamFortressViewport::ActivateClientUI();
	if ( IsWorldMapVisible() )
		gEngfuncs.pfnClientCmd( "setpause\n" );
}

void CZEROViewPort::ShowWorldMap()
{
	if ( m_pWorldMap )
		m_pWorldMap->Open();
}

void CZEROViewPort::HideWorldMap()
{
	if ( m_pWorldMap )
	{
		m_pWorldMap->SetVisible( false );
		gEngfuncs.pfnClientCmd( "unpause\n" );
	}
}

bool CZEROViewPort::IsWorldMapVisible()
{
	return m_pWorldMap && m_pWorldMap->IsVisible();
}

void CZEROViewPort::UnlockMission( const char* missionName )
{
	if ( m_pWorldMap )
		m_pWorldMap->UnlockMission( missionName );
}

void CZEROViewPort::LockMission( const char* missionName )
{
	if ( m_pWorldMap )
		m_pWorldMap->LockMission( missionName );
}

void CZEROViewPort::CurrentMission( const char* missionName )
{
	if ( m_pWorldMap )
		m_pWorldMap->CurrentMission( missionName );
}
