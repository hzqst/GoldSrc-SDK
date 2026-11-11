//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "global_consts.h"
#include "vgui_CTFViewport.h"

#include <VGUI_HeaderPanel.h>
#include <VGUI_TablePanel.h>
#include <VGUI_TextGrid.h>

class ScoreTablePanel : public vgui::TablePanel
{
private:
	vgui::TextGrid*	_textGrid;
	vgui::Label*	m_pLabel;

public:
	int		m_iRows;
	int		m_iSortedRows[MAX_PLAYERS * 2];
	int		m_iIsATeam[MAX_PLAYERS * 2];
	bool	m_bHasBeenSorted[MAX_PLAYERS];
	int		m_iLastKilledBy;
	int		m_fLastKillTime;

	ScoreTablePanel( int x, int y, int wide, int tall, int columnCount );
	int getRowCount() override;
	vgui::Panel* startCellEditing( int column, int row ) override;
	vgui::Panel* getCellRenderer( int column, int row, bool columnSelected, bool rowSelected, bool cellSelected ) override;
	int getCellTall( int row ) override;
};

class ScorePanel : public CMenuPanel
{
private:
	vgui::HeaderPanel*	_headerPanel;
	ScoreTablePanel*		_tablePanel;
	vgui::Label*		m_pTitleLabel;
	CommandButton*		m_pStatsButton;

public:
	int	m_iNumTeams;
	int	m_iPlayerNum;
	int	m_iShowscoresHeld;

	ScorePanel( int iTrans, int x, int y, int wide, int tall );
	void setSize( int wide, int tall ) override;
	void Update();
	void SortTeams();
	void SortPlayers( int iTeam, char* team );
	int RebuildTeams();
	void DeathMsg( int killer, int victim );
	void Initialize();
	void Open() override;
};
