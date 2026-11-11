//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef CLIENTSCOREBOARDDIALOG_H
#define CLIENTSCOREBOARDDIALOG_H

#include "Frame.h"
#include "iscoreboardinterface.h"

struct team_info_t;
namespace vgui2
{
class SectionedListPanel;
}

class CClientScoreBoardDialog : public vgui2::Frame, public IScoreBoardInterface
{
public:
	CClientScoreBoardDialog( vgui2::Panel* parent );
	~CClientScoreBoardDialog() override;
	virtual void Reset();
	void Update( const char* serverName, bool teamplay, bool spectators ) override;
	void RebuildTeams( const char* serverName, bool teamplay, bool spectators ) override;
	void DeathMsg( int killer, int victim ) override;
	void MoveToFront() override;
	bool IsVisible() override;
	void SetVisible( bool state ) override;
	void SetParent( vgui2::VPANEL parent ) override;
	void SetMouseInput( bool state ) override;
	void Activate( bool mouseInput ) override;
	virtual bool GetPlayerScoreInfo( int player, KeyValues* data );
	virtual void InitScoreboardSections();
	virtual void UpdateTeamInfo();
	virtual void UpdatePlayerInfo();
	virtual Color GetTeamColor( int team );
	virtual void AddHeader(); // add the start header of the scoreboard
	virtual void AddSection( int team, team_info_t* teamInfo );
	static bool StaticPlayerSortFunc( vgui2::SectionedListPanel* list, int item1, int item2 );
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
	int FindItemIDForPlayerIndex( int player );
	void MoveLabelToFront( const char* name );
	void FillScoreBoard();

private:
	int m_iNumTeams;
	vgui2::SectionedListPanel* m_pPlayerList;
	int m_iSectionId; // the current section we are entering into
	int s_VoiceImage[5];
	int TrackerImage;
	int m_iPlayerIndexSymbol;
	int m_iDesiredHeight;
};

#endif
