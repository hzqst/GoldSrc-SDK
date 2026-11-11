//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "TaskFrame.h"
#include "FileSystem.h"

class KeyValues;

namespace vgui2
{
class ListPanel;
struct ListPanelItem;
}

class CSaveGameDialog : public CTaskFrame
{
public:
	CSaveGameDialog( vgui2::Panel* parent );
	~CSaveGameDialog() override;

protected:
	void OnClose() override;
	void OnCommand( const char* command ) override;

	void CreateSavedGamesList();
	bool ParseSaveData( const char* filename, const char* shortName, KeyValues* data );
	const char* FindSaveSlot();
	void ScanSavedGames();

private:
	vgui2::ListPanel* m_pGameList;
};

int	SaveReadNameAndComment( FileHandle_t file, char* name, char* comment );
int	TimeStampSortFunc( vgui2::ListPanel* panel, const vgui2::ListPanelItem& first,
		const vgui2::ListPanelItem& second );
