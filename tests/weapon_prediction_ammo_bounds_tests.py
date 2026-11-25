"""Exercise weapon prediction ammo slot bounds across server and client mods (#651)."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'


def extract(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


COMMON_STUBS = r'''
#include <cassert>
#include <cstring>

enum { MAX_AMMO_SLOTS = 32 };

struct Vec3
{
    float x, y, z;
    float &operator[](int i) { return i == 0 ? x : (i == 1 ? y : z); }
    const float &operator[](int i) const { return i == 0 ? x : (i == 1 ? y : z); }
};

struct clientdata_s
{
    Vec3 vuser3;
    Vec3 vuser4;
};

struct CBasePlayer
{
    int m_rgAmmo[MAX_AMMO_SLOTS];
    int ammo_762natobox;
};

struct CBasePlayerWeapon
{
    int m_iPrimaryAmmoType;
    int m_iSecondaryAmmoType;
};
'''

VALVE_FAMILY_TESTS = r'''
int main()
{
    CBasePlayer player = {};
    for (int i = 0; i < MAX_AMMO_SLOTS; ++i)
        player.m_rgAmmo[i] = 100 + i;

    // Server packing: valid indices read m_rgAmmo, out-of-range indices (-1, -50, 32, 9999) produce -1 / 0.
    {
        CBasePlayerWeapon gun = { 3, 7 };
        clientdata_s cd = {};
        PackClientDataWeaponAmmo(&cd, &player, &gun);
        assert(3.0f == cd.vuser4.x);
        assert(103.0f == cd.vuser4.y);
        assert(7.0f == cd.vuser3.z);
        assert(107.0f == cd.vuser4.z);
    }
    const int badIndices[] = { -1, -50, MAX_AMMO_SLOTS, 10000 };
    for (int bad : badIndices)
    {
        CBasePlayerWeapon gun = { bad, bad };
        clientdata_s cd = {};
        PackClientDataWeaponAmmo(&cd, &player, &gun);
        assert(-1.0f == cd.vuser4.x);
        assert(0.0f == cd.vuser4.y);
        assert(-1.0f == cd.vuser3.z);
        assert(0.0f == cd.vuser4.z);
    }

    // Client unpacking: valid indices write m_rgAmmo; invalid server values must not write out of bounds.
    {
        CBasePlayer clientPlayer = {};
        CBasePlayerWeapon wpn = { 0, 0 };
        clientdata_s cd = {};
        cd.vuser4 = { 5.0f, 42.0f, 9.0f };
        cd.vuser3 = { 0.0f, 0.0f, 6.0f };
        UnpackPredictedWeaponAmmo(clientPlayer, &wpn, cd);
        assert(5 == wpn.m_iPrimaryAmmoType);
        assert(6 == wpn.m_iSecondaryAmmoType);
        assert(42 == clientPlayer.m_rgAmmo[5]);
        assert(9 == clientPlayer.m_rgAmmo[6]);

        clientdata_s out = {};
        PackPredictedWeaponAmmo(out, clientPlayer, &wpn);
        assert(5.0f == out.vuser4.x);
        assert(42.0f == out.vuser4.y);
        assert(6.0f == out.vuser3.z);
        assert(9.0f == out.vuser4.z);
    }
    for (int bad : badIndices)
    {
        CBasePlayer clientPlayer = {};
        CBasePlayerWeapon wpn = { 1, 2 };
        clientdata_s cd = {};
        cd.vuser4 = { (float)bad, 777.0f, 888.0f };
        cd.vuser3 = { 0.0f, 0.0f, (float)bad };
        UnpackPredictedWeaponAmmo(clientPlayer, &wpn, cd);
        assert(-1 == wpn.m_iPrimaryAmmoType);
        assert(-1 == wpn.m_iSecondaryAmmoType);
        for (int i = 0; i < MAX_AMMO_SLOTS; ++i)
            assert(0 == clientPlayer.m_rgAmmo[i]);

        clientdata_s out = {};
        PackPredictedWeaponAmmo(out, clientPlayer, &wpn);
        assert(-1.0f == out.vuser4.x);
        assert(0.0f == out.vuser4.y);
        assert(-1.0f == out.vuser3.z);
        assert(0.0f == out.vuser4.z);
    }
}
'''

CZEROR_TESTS = r'''
int main()
{
    CBasePlayer player = {};
    player.ammo_762natobox = 55;
    for (int i = 0; i < MAX_AMMO_SLOTS; ++i)
        player.m_rgAmmo[i] = 200 + i;

    const int badIndices[] = { -1, -99, MAX_AMMO_SLOTS, 8192 };

    {
        CBasePlayerWeapon wpn = { 4, -1 };
        clientdata_s cd = {};
        PackClientDataWeaponAmmo(&cd, &player, &wpn);
        assert(4.0f == cd.vuser4.x);
        assert(204.0f == cd.vuser4.y);
        assert(55.0f == cd.vuser4.z);
    }
    for (int bad : badIndices)
    {
        CBasePlayerWeapon wpn = { bad, -1 };
        clientdata_s cd = {};
        PackClientDataWeaponAmmo(&cd, &player, &wpn);
        assert(-1.0f == cd.vuser4.x);
        assert(0.0f == cd.vuser4.y);
        assert(55.0f == cd.vuser4.z);
    }

    {
        CBasePlayer clientPlayer = {};
        clientPlayer.ammo_762natobox = 33;
        CBasePlayerWeapon wpn = { 0, -1 };
        clientdata_s cd = {};
        cd.vuser4 = { 10.0f, 18.0f, 33.0f };
        UnpackPredictedWeaponAmmo(clientPlayer, &wpn, cd);
        assert(10 == wpn.m_iPrimaryAmmoType);
        assert(18 == clientPlayer.m_rgAmmo[10]);

        clientdata_s out = {};
        PackPredictedWeaponAmmo(out, clientPlayer, &wpn);
        assert(10.0f == out.vuser4.x);
        assert(18.0f == out.vuser4.y);
        assert(33.0f == out.vuser4.z);
    }
    for (int bad : badIndices)
    {
        CBasePlayer clientPlayer = {};
        clientPlayer.ammo_762natobox = 33;
        CBasePlayerWeapon wpn = { 2, -1 };
        clientdata_s cd = {};
        cd.vuser4 = { (float)bad, 999.0f, 33.0f };
        UnpackPredictedWeaponAmmo(clientPlayer, &wpn, cd);
        assert(-1 == wpn.m_iPrimaryAmmoType);
        for (int i = 0; i < MAX_AMMO_SLOTS; ++i)
            assert(0 == clientPlayer.m_rgAmmo[i]);

        clientdata_s out = {};
        PackPredictedWeaponAmmo(out, clientPlayer, &wpn);
        assert(-1.0f == out.vuser4.x);
        assert(0.0f == out.vuser4.y);
        assert(33.0f == out.vuser4.z);
    }
}
'''

RICOCHET_TESTS = r'''
int main()
{
    CBasePlayer player = {};
    player.m_rgAmmo[1] = 3;

    CBasePlayerWeapon validGun = { 1, -1 };
    assert(3 == GetPredictedWeaponAmmoClip(&player, &validGun));

    const int badIndices[] = { -1, -10, MAX_AMMO_SLOTS, 4096 };
    for (int bad : badIndices)
    {
        CBasePlayerWeapon badGun = { bad, -1 };
        assert(0 == GetPredictedWeaponAmmoClip(&player, &badGun));

        CBasePlayer clientPlayer = {};
        UnpackPredictedWeaponAmmo(clientPlayer, &badGun, 99);
        for (int i = 0; i < MAX_AMMO_SLOTS; ++i)
            assert(0 == clientPlayer.m_rgAmmo[i]);
    }

    CBasePlayer clientPlayer = {};
    UnpackPredictedWeaponAmmo(clientPlayer, &validGun, 5);
    assert(5 == clientPlayer.m_rgAmmo[1]);
}
'''

DOD_TESTS = r'''
int main()
{
    CBasePlayer player = {};
    player.m_rgAmmo[2] = 60;

    CBasePlayerWeapon validGun = { 2, -1 };
    assert(60 == GetClientDataPrimaryAmmo(&player, &validGun));
    assert(0 == GetClientDataPrimaryAmmo(&player, nullptr));

    const int badIndices[] = { -1, -5, MAX_AMMO_SLOTS, 512 };
    for (int bad : badIndices)
    {
        CBasePlayerWeapon badGun = { bad, -1 };
        assert(0 == GetClientDataPrimaryAmmo(&player, &badGun));
    }
}
'''


def run_case(name, production_parts, test_body):
    with tempfile.TemporaryDirectory(prefix=f'goldsrc-weapon-pred-ammo-{name}-') as folder:
        work = Path(folder)
        test_source = work / 'test.cpp'
        test_source.write_text('\n'.join([COMMON_STUBS, *production_parts, test_body]), encoding='utf-8')
        executable = compile_program([test_source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
    print(f'PASS: {name} weapon prediction ammo bounds')


def main():
    for mod in ['valve', 'bshift', 'gearbox']:
        server_src = (ROOT / mod / 'dlls/client.cpp').read_text(encoding='utf-8')
        client_src = (ROOT / mod / 'cl_dll/hl/hl_weapons.cpp').read_text(encoding='utf-8')
        run_case(
            mod,
            [
                extract(server_src, 'static void PackClientDataWeaponAmmo('),
                extract(client_src, 'static void UnpackPredictedWeaponAmmo('),
                extract(client_src, 'static void PackPredictedWeaponAmmo('),
            ],
            VALVE_FAMILY_TESTS,
        )

    czeror_srv = (ROOT / 'czeror/dlls/client.cpp').read_text(encoding='utf-8')
    czeror_cli = (ROOT / 'czeror/cl_dll/hl/hl_weapons.cpp').read_text(encoding='utf-8')
    run_case(
        'czeror',
        [
            extract(czeror_srv, 'static void PackClientDataWeaponAmmo('),
            extract(czeror_cli, 'static void UnpackPredictedWeaponAmmo('),
            extract(czeror_cli, 'static void PackPredictedWeaponAmmo('),
        ],
        CZEROR_TESTS,
    )

    rico_srv = (ROOT / 'ricochet/dlls/client.cpp').read_text(encoding='utf-8')
    rico_cli = (ROOT / 'ricochet/cl_dll/hl/hl_weapons.cpp').read_text(encoding='utf-8')
    run_case(
        'ricochet',
        [
            extract(rico_srv, 'static int GetPredictedWeaponAmmoClip('),
            extract(rico_cli, 'static void UnpackPredictedWeaponAmmo('),
        ],
        RICOCHET_TESTS,
    )

    dod_srv = (ROOT / 'dod/dlls/client.cpp').read_text(encoding='utf-8')
    run_case(
        'dod',
        [extract(dod_srv, 'static int GetClientDataPrimaryAmmo(')],
        DOD_TESTS,
    )


if __name__ == '__main__':
    main()
