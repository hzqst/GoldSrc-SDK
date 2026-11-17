#ifndef BUY_PRESETS_H
#define BUY_PRESETS_H

#include "weapontype.h"
#include "wrect.h"
#include "ammo.h"
#include <vector>

class KeyValues;

enum BuyPresetStringSizes
{
	BUY_PRESET_COMMAND_LEN = 256,
	MaxBuyPresetName = 64,
	MaxBuyPresetImageFname = 64
};

enum AmmoSizeType
{
	AMMO_PERCENT,
	AMMO_CLIPS,
	AMMO_ROUNDS
};

enum
{
	NUM_PRESETS = 4
};

enum
{
	REOPEN_YES,
	REOPEN_NO,
	REOPEN_DONTCHANGE
};

enum
{
	NUM_CLIPS_FOR_CURRENT = 4
};

class BuyPresetWeapon
{
public:
	BuyPresetWeapon();
	BuyPresetWeapon( WeaponIdType weaponID );
	BuyPresetWeapon& operator=( const BuyPresetWeapon& other );

	const wchar_t* GetName() const { return m_name; }
	WeaponIdType GetWeaponID() const { return m_weaponID; }
	void SetWeaponID( WeaponIdType weaponID );
	void SetAmmoType( AmmoSizeType ammoType ) { m_ammoType = ammoType; }
	void SetAmmoAmount( int ammo ) { m_ammoAmount = ammo; }
	void SetFillAmmo( bool fill ) { m_fillAmmo = fill; }
	AmmoSizeType GetAmmoType() const { return m_ammoType; }
	int GetAmmoAmount() const { return m_ammoAmount; }
	bool GetFillAmmo() const { return m_fillAmmo; }

protected:
	const wchar_t*	m_name;
	WeaponIdType	m_weaponID;
	AmmoSizeType	m_ammoType;
	int				m_ammoAmount;
	bool			m_fillAmmo;
};

class WeaponSet
{
public:
	WeaponSet();
	void GetCurrent( int& cost, WeaponSet& current, bool withCareer ) const;
	void GetFromScratch( int& cost, WeaponSet& current, bool full ) const;
	void GenerateBuyCommands( char* commands ) const;
	int FullCost() const;
	void Reset();
	const BuyPresetWeapon& GetPrimaryWeapon() const { return m_primaryWeapon; }
	const BuyPresetWeapon& GetSecondaryWeapon() const { return m_secondaryWeapon; }

	BuyPresetWeapon	m_primaryWeapon;
	BuyPresetWeapon	m_secondaryWeapon;
	bool			m_isRebuy;
	int				m_armor;
	bool			m_helmet;
	bool			m_smokeGrenade;
	bool			m_HEGrenade;
	int				m_flashbangs;
	bool			m_defuser;
	bool			m_nightvision;
	int				m_extraCash;
	bool			m_isSecondaryOptional;
	bool			m_isArmorOptional;
	bool			m_isSmokeOptional;
	bool			m_isHEGrenadeOptional;
	bool			m_isFlashbangOptional;
	bool			m_isDefuserOptional;
	bool			m_isNightvisionOptional;
	bool			m_isCashOptional;
};

typedef std::vector<WeaponSet> WeaponSetList;

class BuyPreset
{
public:
	BuyPreset();
	~BuyPreset();
	void SetName( const wchar_t* name );
	const wchar_t* GetName() const { return m_name; }
	void Parse( KeyValues* values );
	void Save( KeyValues* values );
	int GetNumSets() const { return (int)m_weaponList.size(); }
	const WeaponSet* GetSet( int index ) const;
	int FullCost() const;
	void DeleteSet( int index );
	void SwapSet( int first, int second );
	void ReplaceSet( int index, const WeaponSet& set );
	bool IsUsedForAutobuy() const { return m_isAutobuy; }
	void SetAutobuy( bool autobuy ) { m_isAutobuy = autobuy; }

private:
	wchar_t			m_name[MaxBuyPresetName];
	bool			m_isAutobuy;
	WeaponSetList	m_weaponList;
};

typedef std::vector<BuyPreset> BuyPresetList;

class BuyPresetManager
{
public:
	BuyPresetManager();
	void Reset();
	void ResetItems()
	{
		m_hasNightvision = false;
		m_hasDefuser = false;
	}
	void Save();
	void PurchasePreset( int presetIndex, bool withCareer );
	int GetNumPresets() const { return (int)m_presets.size(); }
	const BuyPreset* GetPreset( int index ) const;
	int GetAutobuyPresetIndex() const;
	void SetNightvision( bool hasNightvision ) { m_hasNightvision = hasNightvision; }
	void SetDefuser( bool hasDefuser ) { m_hasDefuser = hasDefuser; }
	bool HasNightvision() const { return m_hasNightvision; }
	bool HasDefuser() const { return m_hasDefuser; }
	void SetPresets( const BuyPresetList& presets ) { m_presets = presets; }
	void SetEditPresets( const BuyPresetList& presets ) { m_editPresets = presets; }
	int GetNumEditPresets() const { return (int)m_editPresets.size(); }
	BuyPreset* GetEditPreset( int index );
	const BuyPresetList& GetEditPresets() const { return m_editPresets; }
	void ResetEditPresets() { m_editPresets = m_presets; }
	void ResetEditToDefaults();
	const char* GetRebuyString();

private:
	BuyPresetList	m_presets;
	BuyPresetList	m_editPresets;
	bool			m_hasNightvision;
	bool			m_hasDefuser;
	char*			m_rebuyString;
};

extern BuyPresetManager* TheBuyPresets;

bool IsASMapType();
bool IsDEMapType();

void ShowBuyPresetMainMenu( bool runUpdate, int reopenBuyMenu );

const char* ImageFnameFromWeaponID( WeaponIdType weaponID, bool isPrimary );

const wchar_t* WeaponIDToDisplayName( WeaponIdType weaponID );
std::vector<BuyPresetWeapon> CareerWeaponList( const std::vector<BuyPresetWeapon>& source,
	bool isPrimary, ClientWeaponIdType currentClientID );
int CalcClipsNeeded( const BuyPresetWeapon* weapon, const WeaponInfoStruct* info, const int* ammo );
void FillClientAmmo( int* ammo );
ClientWeaponIdType GetClientWeaponID( bool primary );
bool CanBuyWeapon( ClientWeaponIdType currentPrimaryID, ClientWeaponIdType currentSecondaryID,
	WeaponIdType weaponID );
WeaponIdType ClientWeaponIDToGameWeaponID( ClientWeaponIdType weaponID );

#endif
