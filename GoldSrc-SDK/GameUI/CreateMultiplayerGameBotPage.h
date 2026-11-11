//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "PropertyPage.h"

class CCvarToggleCheckButton;
class KeyValues;

namespace vgui2
{
class ComboBox;
class TextEntry;
}

//-----------------------------------------------------------------------------
// Purpose: advanced bot properties page of the create game server dialog
//-----------------------------------------------------------------------------
class CCreateMultiplayerGameBotPage : public vgui2::PropertyPage
{
	typedef vgui2::PropertyPage BaseClass;

public:
	CCreateMultiplayerGameBotPage( vgui2::Panel* parent, const char* name, KeyValues* botKeys );
	~CCreateMultiplayerGameBotPage() override;

	// put all cvars and their current values into 'botKeys'
	void UpdateKeys( KeyValues* botKeys );

protected:
	virtual void OnResetChanges();
	void OnApplyChanges() override;

private:
	void SetJoinTeamCombo( const char* team );
	void SetChatterCombo( const char* chatter );

	CCvarToggleCheckButton*	m_joinAfterPlayer;
	CCvarToggleCheckButton*	m_allowRogues;
	CCvarToggleCheckButton*	m_allowPistols;
	CCvarToggleCheckButton*	m_allowShotguns;
	CCvarToggleCheckButton*	m_allowSubmachineGuns;
	CCvarToggleCheckButton*	m_allowMachineGuns;
	CCvarToggleCheckButton*	m_allowRifles;
	CCvarToggleCheckButton*	m_allowGrenades;
	CCvarToggleCheckButton*	m_allowShields;
	CCvarToggleCheckButton*	m_allowSnipers;
	CCvarToggleCheckButton*	m_deferToHuman;
	vgui2::ComboBox*			m_joinTeamCombo;
	vgui2::ComboBox*			m_chatterCombo;
	vgui2::TextEntry*		m_quotaEntry;
	vgui2::TextEntry*		m_prefixEntry;
};
