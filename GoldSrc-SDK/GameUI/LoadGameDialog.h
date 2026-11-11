//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "TaskFrame.h"

class KeyValues;

namespace vgui2
{
class ListPanel;
}

class CLoadGameDialog : public CTaskFrame
{
public:
	CLoadGameDialog( vgui2::Panel* parent );
	~CLoadGameDialog() override;

protected:
	void OnClose() override;
	void OnCommand( const char* command ) override;

private:
	void CreateSavedGamesList();
	bool ParseSaveData( const char* filename, const char* shortName, KeyValues* data );
	void ScanSavedGames();

	vgui2::ListPanel*	m_pGameList;
};
