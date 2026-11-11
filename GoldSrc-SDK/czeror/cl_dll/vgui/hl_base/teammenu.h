//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef TEAMMENU_H
#define TEAMMENU_H

#include "Frame.h"
#include "utlvector.h"

namespace vgui2
{
class Button;
class RichText;
}

const char* GetStringTeamColor( int team );

class CTeamMenu : public vgui2::Frame
{
public:
	CTeamMenu( vgui2::Panel* parent );
	~CTeamMenu() override;
	void Activate() override;
	void Update( const char* mapName, bool allowSpectators, const char teamNames[][32], int numTeams );
	void AutoAssign();
	int GetNumTeams();
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
	void SetLabelText( const char* entryName, const char* text );
	void LoadMapPage( const char* mapName );
	void MakeTeamButtons( const char teamNames[][32], int numTeams );

	// command callbacks
	void OnTeamButton( int team );
	void OnKeyCodePressed( vgui2::KeyCode code ) override;
	vgui2::PanelMap_t* GetPanelMap() override;
	static vgui2::PanelMap_t m_PanelMap;
	static vgui2::MessageMapItem_t m_MessageMap[1];

private:
	CUtlVector<vgui2::Button*> m_pTeamButtons;
	vgui2::RichText* m_pMapInfo;
	int m_iNumTeams;
	int m_iJumpKey;
};

#endif
