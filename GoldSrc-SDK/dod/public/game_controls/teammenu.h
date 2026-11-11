//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "Frame.h"
#include "utlvector.h"

namespace vgui2
{
class Button;
class RichText;
class Label;
}

const char* GetStringTeamColor( int team );

class CTeamMenu : public vgui2::Frame
{
	DECLARE_CLASS_SIMPLE( CTeamMenu, vgui2::Frame );

public:
	CTeamMenu( vgui2::Panel* parent );
	~CTeamMenu() override;
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
	void Activate() override;
	void OnKeyCodePressed( vgui2::KeyCode code ) override;

private:
	CUtlVector<vgui2::Button*> m_pTeamButtons;

public:
	MESSAGE_FUNC_INT( OnTeamButton, "TeamButton", team );
	void AutoAssign();
	void LoadMapPage( const char* mapName );
	void MakeTeamButtons( const char** teamNames, int numTeams );
	void Update( const char* mapName, bool allowSpectators, const char** teamNames, int numTeams );

	// helper functions
	void SetLabelText( const char* textEntryName, const char* text );

private:
	vgui2::RichText*	m_pMapInfo;
	vgui2::Label*	m_pMapName;
	int				m_iNumTeams;
	int				m_iJumpKey;
	int				m_iScoreBoardKey;
};
