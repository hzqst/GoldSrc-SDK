#pragma once

#include "vgui/IScheme.h"
#include "Frame.h"
#include "Label.h"
#include "weapontype.h"

class WeaponSet;
class BuyPresetListBox;
namespace vgui2
{
class Button;
class Label;
class ComboBox;
class TextEntry;
class CBitmapImagePanel;
}
class EquipmentLabel;

class CCareerBaseBox : public vgui2::Frame
{
public:
	CCareerBaseBox( vgui2::Panel* parent, const char* panelName, bool loadResources );
	~CCareerBaseBox() override;
	virtual void ShowWindow();
	void DoModal() override;
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
	void PaintBackground() override;
	void Paint() override;
	void PerformLayout() override;
	void OnClose() override;
	void OnCommand( const char* command ) override;
	void SetLabelText( const wchar_t* text );
	void SetLabelText( const char* text );
	static void CloseOpenBoxes( bool force );
	static void BringToFront();
	static bool HasVisibleChildPopup();
	static void UndoModal();
	void ActivateBuildMode() override;
	void SetAutoClose( bool autoClose ) { m_shouldAutoClose = autoClose; }
	void FriendLoadResources( const char* resourceName );
	static bool IsBoxOpen() { return s_currentBox != NULL; }

protected:
	void OnKeyCodeTyped( vgui2::KeyCode code ) override;
	void SetLabelVisible( bool visible ) { m_pTextLabel->SetVisible( visible ); }
	vgui2::Button* m_pOkButton;
	vgui2::CBitmapImagePanel* m_pTopLeftPanel;
	vgui2::CBitmapImagePanel* m_pTopRightPanel;
	vgui2::CBitmapImagePanel* m_pBottomLeftPanel;
	vgui2::CBitmapImagePanel* m_pBottomRightPanel;
	vgui2::CBitmapImagePanel* m_pTopLeftBorderPanel;
	vgui2::CBitmapImagePanel* m_pTopRightBorderPanel;
	vgui2::CBitmapImagePanel* m_pBottomLeftBorderPanel;
	vgui2::CBitmapImagePanel* m_pBottomRightBorderPanel;
	Color m_bgColor;
	Color m_borderColor;
	vgui2::Label* m_pTextLabel;
	vgui2::Panel* m_pGapPanel;
	vgui2::CBitmapImagePanel* m_pIconPanel;
	bool m_shouldAutoClose;
	static CCareerBaseBox* s_currentBox;
};

class CBaseSelectBox : public CCareerBaseBox
{
public:
	CBaseSelectBox( vgui2::Panel* parent, const char* name, bool loadResources )
		: CCareerBaseBox( parent, name, loadResources ) {}
	virtual void OnControlChanged() = 0;
};

class CEquipmentSelectBox : public CBaseSelectBox
{
public:
	CEquipmentSelectBox( vgui2::Panel* parent, WeaponSet* weaponSet );
	void OnControlChanged() override;
private:
	void OnCommand( const char* command ) override;
	WeaponSet* m_pWeaponSet;
	vgui2::ComboBox* m_pKevlar;
	vgui2::Label* m_pKevlarLabel;
	EquipmentLabel* m_pKevlarImage;
	vgui2::ComboBox* m_pHelmet;
	vgui2::Label* m_pHelmetLabel;
	EquipmentLabel* m_pHelmetImage;
	vgui2::ComboBox* m_pDefuser;
	vgui2::Label* m_pDefuserLabel;
	EquipmentLabel* m_pDefuserImage;
	vgui2::ComboBox* m_pNightvision;
	vgui2::Label* m_pNightvisionLabel;
	EquipmentLabel* m_pNightvisionImage;
};

class CGrenadeSelectBox : public CBaseSelectBox
{
public:
	CGrenadeSelectBox( vgui2::Panel* parent, WeaponSet* weaponSet );
	void OnControlChanged() override;
private:
	void OnCommand( const char* command ) override;
	WeaponSet* m_pWeaponSet;
	vgui2::ComboBox* m_pHEGrenade;
	EquipmentLabel* m_pHEGrenadeImage;
	vgui2::Label* m_pHELabel;
	vgui2::ComboBox* m_pSmokeGrenade;
	EquipmentLabel* m_pSmokeGrenadeImage;
	vgui2::Label* m_pSmokeLabel;
	vgui2::ComboBox* m_pFlashbangs;
	EquipmentLabel* m_pFlashbangImage;
	vgui2::Label* m_pFlashLabel;
};

class CWeaponSelectBox : public CCareerBaseBox
{
public:
	CWeaponSelectBox( vgui2::Panel* parent, WeaponSet* weaponSet, bool secondary );
	~CWeaponSelectBox() override;
	void ActivateBuildMode() override;
	void UpdateClips();
protected:
	void OnCommand( const char* command ) override;
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
private:
	void PopulateControls();
	void SetClipsVisible( bool visible );
	WeaponIdType GetSelectedWeaponID();
	vgui2::ComboBox* m_pClips;
	vgui2::Label* m_pBullets;
	WeaponSet* m_pWeaponSet;
	bool m_isSecondary;
	BuyPresetListBox* m_pListBox;
	int m_numWeapons;
	WeaponIdType* m_weaponIDs;
};

class CCareerTextEntryBox : public CCareerBaseBox
{
public:
	CCareerTextEntryBox( vgui2::Panel* parent, const char* name, const char* text, const char* command,
		void ( *callback )( vgui2::TextEntry* ) );
	~CCareerTextEntryBox() override;
	void ShowWindow() override;
	void SetCommand( const char* command );
	void SetCallback( void ( *callback )( vgui2::TextEntry* ) );
	vgui2::TextEntry* GetTextEntry();
protected:
	void OnCommand( const char* command ) override;
	void OnKeyCodeTyped( vgui2::KeyCode code ) override;
private:
	vgui2::TextEntry* m_pEntry;
	char* m_command;
	void ( *m_callback )( vgui2::TextEntry* );
};

class CCareerQueryBox : public CCareerBaseBox
{
public:
	CCareerQueryBox( const char* title, const char* labelText, const char* name, vgui2::Panel* parent );
	CCareerQueryBox( const wchar_t* title, const wchar_t* labelText, const char* name, vgui2::Panel* parent );
	CCareerQueryBox( vgui2::Panel* parent, const char* name );
	~CCareerQueryBox() override;
};
