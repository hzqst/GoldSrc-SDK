"""Exercise CBasePlayer::PackDeadPlayerItems with ammo-less and clipped weapons."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK/dlls'


def extract(filename, signature):
    source = (ROOT / filename).read_text(encoding='utf-8')
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <algorithm>
#include <cassert>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

using std::min;
using BOOL = int;
constexpr BOOL TRUE = 1;
constexpr BOOL FALSE = 0;

constexpr int MAX_ITEM_TYPES = 6;
constexpr int MAX_AMMO_SLOTS = 32;
constexpr int WEAPON_NOCLIP = -1;
constexpr int GR_PLR_DROP_GUN_NO = 0;
constexpr int GR_PLR_DROP_GUN_ACTIVE = 1;
constexpr int GR_PLR_DROP_GUN_ALL = 2;
constexpr int GR_PLR_DROP_AMMO_NO = 0;
constexpr int GR_PLR_DROP_AMMO_ACTIVE = 1;
constexpr int GR_PLR_DROP_AMMO_ALL = 2;
constexpr int kRenderFxGlowShell = 19;

struct Vector {
    float x = 0, y = 0, z = 0;
    Vector() = default;
    Vector(float a, float b, float c) : x(a), y(b), z(c) {}
    Vector operator*(float s) const { return Vector(x * s, y * s, z * s); }
};
using vec3_t = Vector;

struct edict_t {};
struct entvars_t {
    const char* classname = "";
    Vector origin, angles, velocity, rendercolor;
    float nextthink = 0;
    int renderfx = 0, renderamt = 0;
};

static const char* STRING(const char* s) { return s ? s : ""; }
static const char* MAKE_STRING(const char* s) { return s ? s : ""; }
static bool FClassnameIs(entvars_t* pev, const char* name) { return pev && pev->classname && 0 == std::strcmp(pev->classname, name); }
static edict_t* ENT(entvars_t*) { return nullptr; }
static void SET_MODEL(edict_t*, const char*) {}

static struct { float time = 10.0f; } g_globals;
static auto* gpGlobals = &g_globals;

struct AmmoInfo { const char* pszName; };

struct CBaseEntity {
    entvars_t ev;
    entvars_t* pev = &ev;
    edict_t* edict() { return nullptr; }
    static CBaseEntity* Create(const char* name, const Vector&, const Vector&, edict_t*);
};

struct CBasePlayerItem : CBaseEntity {
    CBasePlayerItem* m_pNext = nullptr;
    static AmmoInfo AmmoInfoArray[MAX_AMMO_SLOTS];
    virtual int PrimaryAmmoIndex() { return -1; }
    virtual int SecondaryAmmoIndex() { return -1; }
};
AmmoInfo CBasePlayerItem::AmmoInfoArray[MAX_AMMO_SLOTS] = {};

struct CBasePlayerWeapon : CBasePlayerItem {
    int m_iClip = WEAPON_NOCLIP;
    int m_iPrimaryAmmoType = -1;
    int m_iMaxClip = WEAPON_NOCLIP;
    int iMaxClip() { return m_iMaxClip; }
    int PrimaryAmmoIndex() override { return m_iPrimaryAmmoType; }
};

struct CWeaponBox : CBaseEntity {
    std::vector<CBasePlayerItem*> packedWeapons;
    std::vector<std::pair<std::string, int>> packedAmmo;
    void SetThink(void (CWeaponBox::*)()) {}
    void Kill() {}
    void PackWeapon(CBasePlayerItem* item) { packedWeapons.push_back(item); }
    void PackAmmo(const char* name, int count) { packedAmmo.emplace_back(name ? name : "", count); }
};

static CWeaponBox* g_lastBox = nullptr;
CBaseEntity* CBaseEntity::Create(const char*, const Vector&, const Vector&, edict_t*)
{
    delete g_lastBox;
    g_lastBox = new CWeaponBox;
    return g_lastBox;
}

struct CBasePlayer;
struct CGameRules {
    int weaponRule = GR_PLR_DROP_GUN_ACTIVE;
    int ammoRule = GR_PLR_DROP_AMMO_ACTIVE;
    int DeadPlayerWeapons(CBasePlayer*) { return weaponRule; }
    int DeadPlayerAmmo(CBasePlayer*) { return ammoRule; }
};
static CGameRules g_rules;
static CGameRules* g_pGameRules = &g_rules;
static bool IsBustingGame() { return false; }

struct CBasePlayer : CBaseEntity {
    CBasePlayerItem* m_rgpPlayerItems[MAX_ITEM_TYPES] = {};
    CBasePlayerItem* m_pActiveItem = nullptr;
    int sentinelBeforeAmmo = 0x13572468;
    int m_rgAmmo[MAX_AMMO_SLOTS] = {};
    int tabulateCalls = 0;
    int removeAllCalls = 0;

    bool HasNamedPlayerItem(const char*) { return false; }
    void TabulateAmmo() { ++tabulateCalls; }
    void RemoveAllItems(BOOL) { ++removeAllCalls; }
    void PackDeadPlayerItems();
};
'''

TESTS = r'''
#include <string>

int main()
{
    CBasePlayerItem::AmmoInfoArray[1].pszName = "9mm";
    CBasePlayerItem::AmmoInfoArray[2].pszName = "Hand Grenade";

    // 1. Crowbar (WEAPON_NOCLIP, ammo index -1): must not read/write m_rgAmmo[-1].
    {
        CBasePlayer player;
        CBasePlayerWeapon crowbar;
        crowbar.pev->classname = "weapon_crowbar";
        crowbar.m_iClip = WEAPON_NOCLIP;
        crowbar.m_iMaxClip = WEAPON_NOCLIP;
        crowbar.m_iPrimaryAmmoType = -1;
        player.m_rgpPlayerItems[0] = &crowbar;
        player.m_pActiveItem = &crowbar;

        player.PackDeadPlayerItems();

        assert(0x13572468 == player.sentinelBeforeAmmo);
        assert(WEAPON_NOCLIP == crowbar.m_iClip);
        assert(0 == player.tabulateCalls);
        assert(1 == g_lastBox->packedWeapons.size());
        assert(&crowbar == g_lastBox->packedWeapons[0]);
    }

    // 2. Clipped weapon (glock): tops up clip from reserve ammo before packing.
    {
        CBasePlayer player;
        CBasePlayerWeapon glock;
        glock.pev->classname = "weapon_9mmhandgun";
        glock.m_iClip = 5;
        glock.m_iMaxClip = 17;
        glock.m_iPrimaryAmmoType = 1;
        player.m_rgAmmo[1] = 20;
        player.m_rgpPlayerItems[1] = &glock;
        player.m_pActiveItem = &glock;

        player.PackDeadPlayerItems();

        assert(0x13572468 == player.sentinelBeforeAmmo);
        assert(17 == glock.m_iClip);
        assert(8 == player.m_rgAmmo[1]);
        assert(1 == player.tabulateCalls);
        assert(1 == g_lastBox->packedWeapons.size());
        assert(1 == g_lastBox->packedAmmo.size());
        assert(8 == g_lastBox->packedAmmo[0].second);
    }

    // 3. No-clip weapon with ammo (grenade): skips reload so clip stays WEAPON_NOCLIP and ammo stays intact.
    {
        CBasePlayer player;
        CBasePlayerWeapon grenade;
        grenade.pev->classname = "weapon_handgrenade";
        grenade.m_iClip = WEAPON_NOCLIP;
        grenade.m_iMaxClip = WEAPON_NOCLIP;
        grenade.m_iPrimaryAmmoType = 2;
        player.m_rgAmmo[2] = 4;
        player.m_rgpPlayerItems[4] = &grenade;
        player.m_pActiveItem = &grenade;

        player.PackDeadPlayerItems();

        assert(WEAPON_NOCLIP == grenade.m_iClip);
        assert(4 == player.m_rgAmmo[2]);
        assert(0 == player.tabulateCalls);
    }

    delete g_lastBox;
    g_lastBox = nullptr;
}
'''


def main():
    production = extract('player.cpp', 'void CBasePlayer::PackDeadPlayerItems( void )')
    with tempfile.TemporaryDirectory(prefix='goldsrc-pack-dead-player-items-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
    print('PASS: PackDeadPlayerItems skips reload for ammo-less / no-clip weapons')


if __name__ == '__main__':
    main()
