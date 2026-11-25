"""Exercise CBaseMonster::IRelationship handling of CLASS_VEHICLE (14)."""
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


STUBS = r'''
#include <cassert>

enum
{
    R_AL = -2,
    R_FR = -1,
    R_NO = 0,
    R_DL = 1,
    R_HT = 2,
    R_NM = 3,
};

enum
{
    CLASS_NONE = 0,
    CLASS_MACHINE = 1,
    CLASS_PLAYER = 2,
    CLASS_HUMAN_PASSIVE = 3,
    CLASS_HUMAN_MILITARY = 4,
    CLASS_ALIEN_MILITARY = 5,
    CLASS_ALIEN_PASSIVE = 6,
    CLASS_ALIEN_MONSTER = 7,
    CLASS_ALIEN_PREY = 8,
    CLASS_ALIEN_PREDATOR = 9,
    CLASS_INSECT = 10,
    CLASS_PLAYER_ALLY = 11,
    CLASS_PLAYER_BIOWEAPON = 12,
    CLASS_ALIEN_BIOWEAPON = 13,
    CLASS_VEHICLE = 14,
};

class CBaseEntity
{
public:
    explicit CBaseEntity(int cls) : m_class(cls) {}
    virtual ~CBaseEntity() = default;
    virtual int Classify() { return m_class; }

private:
    int m_class;
};

class CBaseMonster : public CBaseEntity
{
public:
    explicit CBaseMonster(int cls) : CBaseEntity(cls) {}
    int IRelationship(CBaseEntity *pTarget);
};
'''

TESTS = r'''
int main()
{
    CBaseEntity vehicle(CLASS_VEHICLE);

    // Every valid table class, including player/alien bioweapons (12 and 13),
    // must get R_NO against CLASS_VEHICLE without reading past iEnemy[14][14].
    for (int cls = CLASS_NONE; cls <= CLASS_ALIEN_BIOWEAPON; ++cls)
    {
        CBaseMonster monster(cls);
        assert(R_NO == monster.IRelationship(&vehicle));
    }

    // Existing table relationships stay unchanged.
    CBaseEntity player(CLASS_PLAYER);
    CBaseEntity alienMilitary(CLASS_ALIEN_MILITARY);
    CBaseMonster playerMonster(CLASS_PLAYER);
    CBaseMonster alienBioweapon(CLASS_ALIEN_BIOWEAPON);
    CBaseMonster playerBioweapon(CLASS_PLAYER_BIOWEAPON);

    assert(R_DL == playerMonster.IRelationship(&alienMilitary));
    assert(R_AL == alienBioweapon.IRelationship(&alienMilitary));
    assert(R_DL == alienBioweapon.IRelationship(&player));
    assert(R_DL == playerBioweapon.IRelationship(&alienMilitary));
}
'''


def main():
    source = (ROOT / 'valve/dlls/monsters.cpp').read_text(encoding='utf-8')
    production = extract(source, 'int CBaseMonster::IRelationship ( CBaseEntity *pTarget )')
    with tempfile.TemporaryDirectory(prefix='goldsrc-monster-relationship-vehicle-') as folder:
        work = Path(folder)
        test_source = work / 'test.cpp'
        test_source.write_text('\n'.join([STUBS, production, TESTS]), encoding='utf-8')
        executable = compile_program([test_source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
    print('PASS: valve CBaseMonster::IRelationship returns R_NO for CLASS_VEHICLE')


if __name__ == '__main__':
    main()
