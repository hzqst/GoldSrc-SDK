//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "iviewport.h"
#include "Frame.h"

class KeyValues;
namespace vgui2 { class SectionedListPanel; }

class IScoreBoardInterface
{
public:
	virtual void Reset() = 0;
	virtual void Update( const char* servername, bool teamplay, bool spectator ) = 0;
	virtual void RebuildTeams( const char* teamname, bool teamplay, bool spectator ) = 0;
	virtual void DeathMsg( int killer, int victim ) = 0;
	virtual void Activate( bool spectatorUIVisible ) = 0;
	virtual void MoveToFront() = 0;
	virtual bool IsVisible() = 0;
	virtual void SetVisible( bool state ) = 0;
	virtual void SetParent( vgui2::VPANEL parent ) = 0;
	virtual void SetMouseInputEnabled( bool state ) = 0;
	virtual void SetTeamName( int index, const char* name ) = 0;
	virtual void SetTeamDetails( int index, int frags, int deaths ) = 0;
	virtual const char* GetTeamName( int index ) = 0;
	virtual VGuiLibraryTeamInfo_t GetPlayerTeamInfo( int playerIndex ) = 0;
};

struct ScoreBoardTeamInfo : VGuiLibraryTeamInfo_t
{
	bool	already_drawn;
	bool	scores_overriden;
	bool	ownteam;
};

//-----------------------------------------------------------------------------
// Purpose: Game ScoreBoard
//-----------------------------------------------------------------------------
class CClientScoreBoardDialog : public vgui2::Frame, public IScoreBoardInterface
{
public:
	CClientScoreBoardDialog( vgui2::Panel* parent );
	void Reset() override;
	void Update( const char* servername, bool teamplay, bool spectator ) override;
	void RebuildTeams( const char* teamname, bool teamplay, bool spectator ) override;
	void DeathMsg( int killer, int victim ) override;
	void Activate( bool spectatorUIVisible ) override;
	void MoveToFront() override;
	bool IsVisible() override;
	void SetVisible( bool state ) override;
	void SetParent( vgui2::VPANEL parent ) override;
	void SetMouseInputEnabled( bool state ) override;
	void SetTeamName( int index, const char* name ) override;
	void SetTeamDetails( int index, int frags, int deaths ) override;
	const char* GetTeamName( int index ) override;
	VGuiLibraryTeamInfo_t GetPlayerTeamInfo( int playerIndex ) override;
	virtual bool GetPlayerScoreInfo( int playerIndex, KeyValues* kv );
	virtual void InitScoreboardSections();
	virtual void UpdateTeamInfo();
	virtual void UpdatePlayerInfo();
	virtual Color GetTeamColor( int teamNumber );
	virtual void AddHeader(); // add the start header of the scoreboard
	virtual void AddSection( int teamType, VGuiLibraryTeamInfo_t* team_info ); // add a new section header for a team

	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
	void FillScoreBoard();

	// finds the player in the scoreboard
	int FindItemIDForPlayerIndex( int playerIndex );
	void MoveLabelToFront( const char* textEntryName );

protected:
	static bool StaticPlayerSortFunc( vgui2::SectionedListPanel* list, int itemID1, int itemID2 );

	int						m_iNumTeams;
	vgui2::SectionedListPanel*	m_pPlayerList;
	int						m_iSectionId; // the current section we are entering into
	int						s_VoiceImage[5];
	int						TrackerImage;
	ScoreBoardTeamInfo		m_TeamInfo[5];
	ScoreBoardTeamInfo		m_BlankTeamInfo;
	int						m_iPlayerIndexSymbol;
	int						m_iDesiredHeight;
};
