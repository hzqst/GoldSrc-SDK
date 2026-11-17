#ifndef WEAPONTYPE_H
#define WEAPONTYPE_H

enum WeaponIdType
{
	WEAPON_NONE = 0,
	WEAPON_P228,
	WEAPON_GLOCK,
	WEAPON_SCOUT,
	WEAPON_HEGRENADE,
	WEAPON_XM1014,
	WEAPON_C4,
	WEAPON_MAC10,
	WEAPON_AUG,
	WEAPON_SMOKEGRENADE,
	WEAPON_ELITE,
	WEAPON_FIVESEVEN,
	WEAPON_UMP45,
	WEAPON_SG550,
	WEAPON_GALIL,
	WEAPON_FAMAS,
	WEAPON_USP,
	WEAPON_GLOCK18,
	WEAPON_AWP,
	WEAPON_MP5N,
	WEAPON_M249,
	WEAPON_M3,
	WEAPON_M4A1,
	WEAPON_TMP,
	WEAPON_G3SG1,
	WEAPON_FLASHBANG,
	WEAPON_DEAGLE,
	WEAPON_SG552,
	WEAPON_AK47,
	WEAPON_KNIFE,
	WEAPON_P90,
	WEAPON_SHIELDGUN = 99
};

enum AmmoCostType
{
	AMMO_338MAG_PRICE = 125,
	AMMO_357SIG_PRICE = 50,
	AMMO_45ACP_PRICE = 25,
	AMMO_50AE_PRICE = 40,
	AMMO_556MM_PRICE = 60,
	AMMO_57MM_PRICE = 50,
	AMMO_762MM_PRICE = 80,
	AMMO_9MM_PRICE = 20,
	AMMO_BUCKSHOT_PRICE = 65
};

enum WeaponCostType
{
	AK47_PRICE = 2500,
	AWP_PRICE = 4750,
	DEAGLE_PRICE = 650,
	G3SG1_PRICE = 5000,
	SG550_PRICE = 4200,
	GLOCK18_PRICE = 400,
	M249_PRICE = 5750,
	M3_PRICE = 1700,
	M4A1_PRICE = 3100,
	AUG_PRICE = 3500,
	MP5NAVY_PRICE = 1500,
	P228_PRICE = 600,
	P90_PRICE = 2350,
	UMP45_PRICE = 1700,
	MAC10_PRICE = 1400,
	SCOUT_PRICE = 2750,
	SG552_PRICE = 3500,
	TMP_PRICE = 1250,
	USP_PRICE = 500,
	ELITE_PRICE = 800,
	FIVESEVEN_PRICE = 750,
	XM1014_PRICE = 3000,
	GALIL_PRICE = 2000,
	FAMAS_PRICE = 2250,
	SHIELDGUN_PRICE = 2200
};

enum ItemCostType
{
	ASSAULTSUIT_PRICE = 1000,
	FLASHBANG_PRICE = 200,
	HEGRENADE_PRICE = 300,
	SMOKEGRENADE_PRICE = 300,
	KEVLAR_PRICE = 650,
	HELMET_PRICE = 350,
	NVG_PRICE = 1250,
	DEFUSEKIT_PRICE = 200
};

enum AmmoType
{
	AMMO_BUCKSHOT = 0,
	AMMO_9MM,
	AMMO_556NATO,
	AMMO_556NATOBOX,
	AMMO_762NATO,
	AMMO_45ACP,
	AMMO_50AE,
	AMMO_338MAGNUM,
	AMMO_57MM,
	AMMO_357SIG,
	AMMO_MAX_TYPES
};

enum MaxAmmoType
{
	MAX_AMMO_BUCKSHOT = 32,
	MAX_AMMO_9MM = 120,
	MAX_AMMO_556NATO = 90,
	MAX_AMMO_556NATOBOX = 200,
	MAX_AMMO_762NATO = 90,
	MAX_AMMO_45ACP = 100,
	MAX_AMMO_50AE = 35,
	MAX_AMMO_338MAGNUM = 30,
	MAX_AMMO_57MM = 100,
	MAX_AMMO_357SIG = 52
};

struct WeaponInfoStruct
{
	int			id;
	int			cost;
	int			clipCost;
	int			buyClipSize;
	int			gunClipSize;
	int			maxRounds;
	int			ammoType;
	const char*	entityName;
};

struct WeaponStruct
{
	int	m_type;
	int	m_price;
	int	m_side;
	int	m_slot;
	int	m_ammoPrice;
};

enum WeaponClassType
{
	WEAPONCLASS_NONE = 0,
	WEAPONCLASS_KNIFE,
	WEAPONCLASS_PISTOL,
	WEAPONCLASS_GRENADE,
	WEAPONCLASS_SUBMACHINEGUN,
	WEAPONCLASS_SHOTGUN,
	WEAPONCLASS_MACHINEGUN,
	WEAPONCLASS_RIFLE,
	WEAPONCLASS_SNIPERRIFLE,

	WEAPONCLASS_MAX
};

enum AutoBuyClassType
{
	AUTOBUYCLASS_PRIMARY = 1,
	AUTOBUYCLASS_SECONDARY = 2,
	AUTOBUYCLASS_AMMO = 4,
	AUTOBUYCLASS_ARMOR = 8,
	AUTOBUYCLASS_DEFUSER = 16,
	AUTOBUYCLASS_PISTOL = 32,
	AUTOBUYCLASS_SMG = 64,
	AUTOBUYCLASS_RIFLE = 128,
	AUTOBUYCLASS_SNIPERRIFLE = 256,
	AUTOBUYCLASS_SHOTGUN = 512,
	AUTOBUYCLASS_MACHINEGUN = 1024,
	AUTOBUYCLASS_GRENADE = 2048,
	AUTOBUYCLASS_NIGHTVISION = 4096,
	AUTOBUYCLASS_SHIELD = 8192
};

struct AutoBuyInfoStruct
{
	AutoBuyClassType	m_class;
	char*			m_command;
	char*			m_classname;
};

extern AutoBuyInfoStruct g_autoBuyInfo[];

struct WeaponAliasInfo
{
	const char*		alias;
	WeaponIdType	id;
};

struct WeaponBuyAliasInfo
{
	const char*		alias;
	WeaponIdType	id;
	const char*		failName;
};

struct WeaponClassAliasInfo
{
	const char*		alias;
	WeaponClassType	id;
};

WeaponIdType		AliasToWeaponID( const char* alias );
const char*			BuyAliasToWeaponID( const char* alias, WeaponIdType* id );
const char*			WeaponIDToAlias( int id );
WeaponClassType	AliasToWeaponClass( const char* alias );
WeaponClassType	WeaponIDToWeaponClass( int id );
bool				IsPrimaryWeapon( int id );
bool				IsSecondaryWeapon( int id );
const WeaponInfoStruct*	GetWeaponInfo( int weaponID );
bool				CanBuyWeaponByMaptype( int playerTeam, WeaponIdType weaponID, bool useAssasinationRestrictions );

#endif
