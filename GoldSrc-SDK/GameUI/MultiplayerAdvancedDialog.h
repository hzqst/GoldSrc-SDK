//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "TaskFrame.h"
#include "PropertyPage.h"

class CInfoDescription;
class CPanelListPanel;
class mpcontrol_t;

class CMultiplayerAdvanced
{
public:
	virtual vgui2::Panel* GetPanel() = 0;

	void GatherCurrentValues();
	void DestroyControls();
	void SaveValues();
	void CreateControls();

protected:
	CInfoDescription*	m_pDescription;
	mpcontrol_t*		m_pList;
	CPanelListPanel*		m_pListPanel;
#ifdef _MSC_VER
#pragma warning( suppress: 26495 )
#endif
};

class CMultiplayerAdvancedPage : public vgui2::PropertyPage, public CMultiplayerAdvanced
{
public:
	CMultiplayerAdvancedPage( vgui2::Panel* parent );
	~CMultiplayerAdvancedPage() override;
	vgui2::Panel* GetPanel() override;

protected:
	void OnResetData() override;
	void OnApplyChanges() override;
};

class CMultiplayerAdvancedDialog : public CTaskFrame, public CMultiplayerAdvanced
{
public:
	CMultiplayerAdvancedDialog( vgui2::Panel* parent );
	~CMultiplayerAdvancedDialog() override;
	vgui2::Panel* GetPanel() override;

protected:
	void OnClose() override;
	void OnCommand( const char* command ) override;
};
