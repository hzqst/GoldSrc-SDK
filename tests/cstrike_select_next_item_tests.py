"""Regression tests for CBasePlayer::SelectNextItem when m_pActiveItem is NULL (#506)."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

SDK_ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'


def extract(relpath, signature):
    source = (SDK_ROOT / relpath).read_text(encoding='utf-8')
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>

static constexpr int WPNSTATE_SHIELD_DRAWN = (1 << 5);
static constexpr int HIDEHUD_CROSSHAIR = (1 << 6);

struct CBasePlayerItem {
    CBasePlayerItem* m_pNext = nullptr;
    int holsterCalls = 0;
    int deployCalls = 0;
    int updateItemInfoCalls = 0;

    void Holster() { ++holsterCalls; }
    void Deploy() { ++deployCalls; }
    void UpdateItemInfo() { ++updateItemInfoCalls; }
};

struct CBasePlayerWeapon : CBasePlayerItem {
    int m_iWeaponState = 0;
};

struct CBasePlayer {
    CBasePlayerItem* m_rgpPlayerItems[6] = {};
    CBasePlayerItem* m_pActiveItem = nullptr;
    CBasePlayerItem* m_pLastItem = nullptr;
    bool m_bOwnsShield = false;
    bool m_bShieldDrawn = false;
    int m_iHideHUD = 0;
    int resetAutoaimCalls = 0;
    int resetMaxSpeedCalls = 0;

    void ResetAutoaim() { ++resetAutoaimCalls; }
    void ResetMaxSpeed() { ++resetMaxSpeedCalls; }
    void SelectNextItem( int slot );
};
'''

TESTS = r'''
int main() {
    // 1. Owns shield with m_pActiveItem == NULL: must not dereference NULL,
    //    must clear m_bShieldDrawn and deploy the selected slot item.
    {
        CBasePlayer player;
        CBasePlayerWeapon pistol;
        player.m_rgpPlayerItems[2] = &pistol;
        player.m_pActiveItem = nullptr;
        player.m_bOwnsShield = true;
        player.m_bShieldDrawn = true;
        player.m_iHideHUD = HIDEHUD_CROSSHAIR;

        player.SelectNextItem(2);

        assert(!player.m_bShieldDrawn);
        assert(nullptr == player.m_pLastItem);
        assert(&pistol == player.m_pActiveItem);
        assert(0 == (player.m_iHideHUD & HIDEHUD_CROSSHAIR));
        assert(1 == pistol.deployCalls);
        assert(1 == pistol.updateItemInfoCalls);
        assert(1 == player.resetAutoaimCalls);
        assert(1 == player.resetMaxSpeedCalls);
    }

    // 2. Owns shield with non-NULL active item: holsters old item, clears WPNSTATE_SHIELD_DRAWN,
    //    and deploys the new item.
    {
        CBasePlayer player;
        CBasePlayerWeapon pistol;
        CBasePlayerWeapon knife;
        pistol.m_iWeaponState = WPNSTATE_SHIELD_DRAWN | 0x1;
        player.m_rgpPlayerItems[2] = &pistol;
        player.m_rgpPlayerItems[3] = &knife;
        player.m_pActiveItem = &pistol;
        player.m_bOwnsShield = true;
        player.m_bShieldDrawn = true;

        player.SelectNextItem(3);

        assert(1 == pistol.holsterCalls);
        assert(0x1 == pistol.m_iWeaponState);
        assert(!player.m_bShieldDrawn);
        assert(&pistol == player.m_pLastItem);
        assert(&knife == player.m_pActiveItem);
        assert(1 == knife.deployCalls);
        assert(1 == knife.updateItemInfoCalls);
    }

    return 0;
}
'''


def main():
    production = extract(
        'cstrike/dlls/player.cpp',
        'void CBasePlayer::SelectNextItem( int slot )',
    )
    with tempfile.TemporaryDirectory(prefix='goldsrc-cstrike-select-next-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
    print('PASS: CBasePlayer::SelectNextItem handles NULL m_pActiveItem when owning shield')


if __name__ == '__main__':
    main()
