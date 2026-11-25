"""Exercise CBasePlayer ammo inventory accessors and CBasePlayerWeapon::IsUseable across all 9 mods (#651)."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'

MODS = [
    ('valve', 'valve/dlls/player.h', 'valve/dlls/player.cpp', 'valve/cl_dll/hl/hl_baseentity.cpp', 'valve/dlls/weapons.cpp', 'BOOL CBasePlayerWeapon :: IsUseable( void )'),
    ('bshift', 'bshift/dlls/player.h', 'bshift/dlls/player.cpp', 'bshift/cl_dll/hl/hl_baseentity.cpp', 'bshift/dlls/weapons.cpp', 'BOOL CBasePlayerWeapon :: IsUseable( void )'),
    ('cstrike', 'cstrike/dlls/player.h', 'cstrike/dlls/player.cpp', 'cstrike/cl_dll/hl/hl_baseentity.cpp', 'cstrike/dlls/weapons.cpp', 'BOOL CBasePlayerWeapon::IsUseable()'),
    ('czeror', 'czeror/dlls/player.h', 'czeror/dlls/player.cpp', 'czeror/cl_dll/hl/hl_baseentity.cpp', 'czeror/dlls/weapons.cpp', 'BOOL CBasePlayerWeapon::IsUseable()'),
    ('dmc', 'dmc/dlls/player.h', 'dmc/dlls/player.cpp', 'dmc/cl_dll/quake/quake_baseentity.cpp', 'dmc/dlls/weapons.cpp', 'BOOL CBasePlayerWeapon :: IsUseable( void )'),
    ('dod', 'dod/dlls/player.h', 'dod/dlls/player.cpp', 'dod/cl_dll/hl/hl_baseentity.cpp', 'dod/dlls/weapons.cpp', 'BOOL CBasePlayerWeapon :: IsUseable( void )'),
    ('gearbox', 'gearbox/dlls/player.h', 'gearbox/dlls/player.cpp', 'gearbox/cl_dll/hl/hl_baseentity.cpp', 'gearbox/dlls/weapons.cpp', 'BOOL CBasePlayerWeapon :: IsUseable( void )'),
    ('ricochet', 'ricochet/dlls/player.h', 'ricochet/dlls/player.cpp', 'ricochet/cl_dll/hl/hl_baseentity.cpp', 'ricochet/dlls/weapons.cpp', 'BOOL CBasePlayerWeapon :: IsUseable( void )'),
    ('tfc', 'tfc/player.h', 'tfc/player.cpp', 'tfc/cl_dll/tfc/tf_baseentity.cpp', 'tfc/weapons.cpp', 'BOOL CBasePlayerWeapon :: IsUseable( void )'),
]


def extract(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


def extract_indented_method(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n\t}', start) + 3]


STUB_HEADER = r'''
#include <cassert>
#include <cstddef>

typedef int BOOL;
enum { FALSE = 0, TRUE = 1, MAX_AMMO_SLOTS = 32 };

struct ItemInfo
{
    int iAmmo1;
    int iMaxAmmo1;
};

class CBasePlayer
{
public:
    int m_rgAmmo[MAX_AMMO_SLOTS] = {};

    int AmmoInventory(int iAmmoIndex){AMMO_CONST};
'''

STUB_BODY = r'''
};

class CBasePlayerWeapon
{
public:
    CBasePlayer *m_pPlayer = nullptr;
    int m_iClip = -1;
    int m_iPrimaryAmmoType = -1;
    int m_maxAmmo1 = -1;
    int *current_ammo = nullptr;

    int PrimaryAmmoIndex() { return m_iPrimaryAmmoType; }
    int iMaxAmmo1() { return m_maxAmmo1; }
    BOOL CanDeploy() { return FALSE; }
    int GetItemInfo(ItemInfo *p)
    {
        p->iAmmo1 = m_maxAmmo1;
        p->iMaxAmmo1 = m_maxAmmo1;
        return 1;
    }

    BOOL IsUseable();
};
'''

TESTS = r'''
int main()
{
    CBasePlayer player;
    for (int i = 0; i < MAX_AMMO_SLOTS; ++i)
        player.m_rgAmmo[i] = 10 + i;

    // Valid indices
    assert(10 == player.AmmoInventory(0));
    assert(41 == player.AmmoInventory(MAX_AMMO_SLOTS - 1));
    assert(player.SetAmmoInventory(3, 99));
    assert(99 == player.AmmoInventory(3));
    assert(player.AddAmmoInventory(3, -9));
    assert(90 == player.AmmoInventory(3));
    assert(&player.m_rgAmmo[3] == player.AmmoInventoryPtr(3));

    // Invalid indices must not read or write m_rgAmmo out of bounds
    const int badIndices[] = { -1, -2, -100, MAX_AMMO_SLOTS, MAX_AMMO_SLOTS + 1, 9999 };
    for (int bad : badIndices)
    {
        assert(-1 == player.AmmoInventory(bad));
        assert(!player.SetAmmoInventory(bad, 123));
        assert(!player.AddAmmoInventory(bad, 5));
        assert(nullptr == player.AmmoInventoryPtr(bad));
    }

    // Melee / ammo-less weapon (m_iClip == -1, PrimaryAmmoIndex() == -1, iMaxAmmo1() == -1)
    CBasePlayerWeapon melee;
    melee.m_pPlayer = &player;
    melee.m_iClip = -1;
    melee.m_iPrimaryAmmoType = -1;
    melee.m_maxAmmo1 = -1;
    assert(TRUE == melee.IsUseable());

    // Ammo-using weapon with empty clip and 0 reserve
    CBasePlayerWeapon gun;
    gun.m_pPlayer = &player;
    gun.m_iClip = 0;
    gun.m_iPrimaryAmmoType = 2;
    gun.m_maxAmmo1 = 100;
    assert(player.SetAmmoInventory(2, 0));
    assert(FALSE == gun.IsUseable());

    // Ammo-using weapon with reserve > 0
    assert(player.SetAmmoInventory(2, 12));
    assert(TRUE == gun.IsUseable());

    // Ammo-using weapon with invalid ammo index (-1 or >= MAX_AMMO_SLOTS) and empty clip
    gun.m_iPrimaryAmmoType = -1;
    assert(FALSE == gun.IsUseable());
    gun.m_iPrimaryAmmoType = MAX_AMMO_SLOTS;
    assert(FALSE == gun.IsUseable());
}
'''


def main():
    for mod, player_h, player_cpp, client_base, weapons_cpp, is_useable_sig in MODS:
        header_src = (ROOT / player_h).read_text(encoding='utf-8')
        assert 'm_rgAmmo[MAX_AMMO_SLOTS]' in header_src, f'{player_h} should size m_rgAmmo with MAX_AMMO_SLOTS'
        set_fn = extract_indented_method(header_src, '\tbool SetAmmoInventory(')
        add_fn = extract_indented_method(header_src, '\tbool AddAmmoInventory(')
        ptr_fn = extract_indented_method(header_src, '\tint *AmmoInventoryPtr(')

        weapons_src = (ROOT / weapons_cpp).read_text(encoding='utf-8')
        is_useable_fn = extract(weapons_src, is_useable_sig)

        for label, impl_path in [('server', player_cpp), ('client', client_base)]:
            impl_src = (ROOT / impl_path).read_text(encoding='utf-8')
            ammo_inv_fn = extract(impl_src, 'int CBasePlayer::AmmoInventory(')
            ammo_const = ' const' if ') const' in ammo_inv_fn.splitlines()[0] else ''
            with tempfile.TemporaryDirectory(prefix=f'goldsrc-ammo-bounds-{mod}-{label}-') as folder:
                work = Path(folder)
                test_source = work / 'test.cpp'
                test_source.write_text(
                    '\n'.join([STUB_HEADER.replace('{AMMO_CONST}', ammo_const), set_fn, add_fn, ptr_fn, STUB_BODY, ammo_inv_fn, is_useable_fn, TESTS]),
                    encoding='utf-8',
                )
                executable = compile_program([test_source], work / 'test', sanitizers=['address', 'undefined'])
                subprocess.run([str(executable)], cwd=work, check=True)
        print(f'PASS: {mod} server+client AmmoInventory/SetAmmoInventory and IsUseable')


if __name__ == '__main__':
    main()
