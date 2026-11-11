//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "PropertyPage.h"
#include "utlsymbol.h"
#include "utlvector.h"

class KeyValues;
class VControlsListPanel;

namespace vgui2
{
class Button;
}

namespace vgui = vgui2;

class COptionsSubKeyboard : public vgui::PropertyPage
{
	DECLARE_CLASS_SIMPLE( COptionsSubKeyboard, vgui::PropertyPage );

public:
	COptionsSubKeyboard( vgui2::Panel* parent );
	~COptionsSubKeyboard() override;

protected:
	virtual void Finish( int key, int button, const char* keyName );
	MESSAGE_FUNC_INT( ItemSelected, "ItemSelected", itemID );

	void OnResetData() override;
	void OnApplyChanges() override;
	void OnCommand( const char* command ) override;
	void OnKeyCodeTyped( vgui2::KeyCode code ) override;
	void OnKeyTyped( wchar_t unichar ) override;
	void OnMousePressed( vgui2::MouseCode code ) override;
	void OnMouseDoublePressed( vgui2::MouseCode code ) override;
	void OnMouseWheeled( int delta ) override;
	void OnKeyCodePressed( vgui2::KeyCode code ) override;

private:
	void CreateKeyBindingList();
	KeyValues* GetItemForBinding( const char* binding );
	// Look up the engine's name for a key
	const char* GetKeyName( int keynum );
	int FindKeyForName( const char* keyname );
	void AddBinding( KeyValues* item, const char* keyname, const char* displayName );
	void ClearBindItems();
	// Remove all instances of a key from all bindings
	void RemoveKeyFromBindItems( const char* key );
	void FillInCurrentBindings();
	void DeleteSavedBindings();
	void SaveCurrentBindings();
	void BindKey( const char* key, const char* binding );
	void UnbindKey( const char* key );
	void ApplyAllBindings();
	void FillInDefaultBindings();
	void ParseActionDescriptions();

	VControlsListPanel*	m_pKeyBindList;
	vgui2::Button*		m_pSetBindingButton;
	vgui2::Button*		m_pClearBindingButton;
	struct
	{
		char* binding;
	} m_Bindings[256];
	// List of all the keys that need to have their binding removed
	CUtlVector<CUtlSymbol> m_KeysToUnbind;
};
