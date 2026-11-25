"""Exercise CFuncTank / COFFuncTank controller ownership and null-controller safety (#487)."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'

TARGETS = [
    ('valve/dlls/func_tank.cpp', 'CFuncTank', 'CFuncTank :: ', True, False, False, False, False),
    ('cstrike/dlls/func_tank.cpp', 'CFuncTank', 'CFuncTank :: ', True, True, False, False, False),
    ('czeror/dlls/func_tank.cpp', 'CFuncTank', 'CFuncTank :: ', True, False, True, False, False),
    ('bshift/dlls/func_tank.cpp', 'CFuncTank', 'CFuncTank :: ', True, False, False, False, False),
    ('dmc/dlls/func_tank.cpp', 'CFuncTank', 'CFuncTank :: ', True, False, False, False, False),
    ('gearbox/dlls/func_tank.cpp', 'CFuncTank', 'CFuncTank :: ', True, False, False, False, False),
    ('gearbox/dlls/func_tank_of.cpp', 'COFFuncTank', 'COFFuncTank::', True, False, False, True, False),
    ('ricochet/dlls/func_tank.cpp', 'CFuncTank', 'CFuncTank :: ', True, False, False, False, False),
    ('tfc/func_tank.cpp', 'CFuncTank', 'CFuncTank :: ', False, False, False, False, True),
]


def extract(source, signature):
    start = source.index(signature)
    depth = 0
    in_body = False
    for index in range(start, len(source)):
        ch = source[index]
        if ch == '{':
            depth += 1
            in_body = True
        elif ch == '}':
            depth -= 1
            if in_body and depth == 0:
                return source[start:index + 1]
    raise RuntimeError(f'Failed to find closing brace for {signature}')


def build_test_source(class_name, production_code, has_master, has_reset_max_speed, has_uses_model, has_fixed_owner, has_tfc_activation):
    stubs = f'''
#include <cassert>
#include <cstddef>

typedef int BOOL;
#define TRUE 1
#define FALSE 0

enum USE_TYPE {{ USE_OFF = 0, USE_ON = 1, USE_SET = 2, USE_TOGGLE = 3 }};

enum
{{
    CLASS_NONE = 0,
    CLASS_PLAYER = 2,
}};

#define SF_TANK_ACTIVE      0x0001
#define SF_TANK_CANCONTROL  0x0020
#define SF_TANK_FIXED_OWNER 0x0080
#define HIDEHUD_WEAPONS     (1 << 0)
#define IN_ATTACK           (1 << 0)
#define LOUD_GUN_VOLUME     1000
#define at_console          0

struct Vector
{{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    Vector() = default;
    Vector(float ax, float ay, float az) : x(ax), y(ay), z(az) {{}}
}};

struct entvars_t
{{
    int spawnflags = 0;
    int button = 0;
    int weaponmodel = 1;
    int viewmodel = 1;
    float ltime = 10.0f;
    float nextthink = 0.0f;
    Vector origin;
    Vector angles;
}};

struct globalvars_t
{{
    float time = 10.0f;
}};

static globalvars_t g_globals;
static globalvars_t *gpGlobals = &g_globals;

static void ALERT(int, const char *, ...) {{}}
static void UTIL_MakeVectorsPrivate(const Vector &, Vector &fwd, float *, float *) {{ fwd = Vector(1.0f, 0.0f, 0.0f); }}

class CBaseEntity;
class CBasePlayer;
class {class_name};

class EHANDLE
{{
public:
    CBaseEntity *ptr = nullptr;
    EHANDLE() = default;
    EHANDLE(CBaseEntity *p) : ptr(p) {{}}
    EHANDLE &operator=(CBaseEntity *p) {{ ptr = p; return *this; }}
    operator CBaseEntity *() const {{ return ptr; }}
}};

class CBaseEntity
{{
public:
    entvars_t storage;
    entvars_t *pev = &storage;
    int m_class = CLASS_NONE;

    virtual ~CBaseEntity() = default;
    virtual int Classify() {{ return m_class; }}
    virtual BOOL IsPlayer() {{ return m_class == CLASS_PLAYER ? TRUE : FALSE; }}
}};

class CBasePlayerItem
{{
public:
    int holsterCount = 0;
    int deployCount = 0;
    {class_name} *tankToCheckOnDeploy = nullptr;
    CBasePlayer *owner = nullptr;
    bool sawClearedControllerOnDeploy = false;
    bool sawClearedTankOnDeploy = false;

    void Holster() {{ ++holsterCount; }}
    void Deploy();
}};

class CBasePlayer : public CBaseEntity
{{
public:
    EHANDLE m_pTank;
    CBasePlayerItem *m_pActiveItem = nullptr;
    int m_iHideHUD = 0;
    int m_iWeaponVolume = 0;
    int resetMaxSpeedCount = 0;

    CBasePlayer() {{ m_class = CLASS_PLAYER; }}
    void ResetMaxSpeed() {{ ++resetMaxSpeedCount; }}
}};

static BOOL g_masterTriggered = TRUE;
static BOOL UTIL_IsMasterTriggered(int, CBaseEntity *) {{ return g_masterTriggered; }}

static BOOL g_activationSucceeded = TRUE;
static BOOL ActivationSucceeded(CBaseEntity *, CBasePlayer *, void *) {{ return g_activationSucceeded; }}

class {class_name} : public CBaseEntity
{{
public:
    CBasePlayer *m_pController = nullptr;
    float m_flNextAttack = 0.0f;
    float m_fireLast = 0.0f;
    float m_fireRate = 2.0f;
    Vector m_vecControllerUsePos;
    int m_iszMaster = 0;
    int m_usesModel = 0;
    int fireCount = 0;
    entvars_t *lastFireAttacker = nullptr;
    int activateCount = 0;
    int deactivateCount = 0;

    BOOL IsActive() {{ return (pev->spawnflags & SF_TANK_ACTIVE) ? TRUE : FALSE; }}
    BOOL ShouldToggle(USE_TYPE, BOOL) {{ return TRUE; }}
    void TankActivate() {{ ++activateCount; pev->spawnflags |= SF_TANK_ACTIVE; }}
    void TankDeactivate() {{ ++deactivateCount; pev->spawnflags &= ~SF_TANK_ACTIVE; }}
    Vector BarrelPosition() {{ return Vector(0.0f, 0.0f, 0.0f); }}
    void Fire(const Vector &, const Vector &, entvars_t *pevAttacker)
    {{
        ++fireCount;
        lastFireAttacker = pevAttacker;
    }}

    BOOL StartControl(CBasePlayer *pController);
    void StopControl(void);
    void ControllerPostFrame(void);
    void Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value);
}};

void CBasePlayerItem::Deploy()
{{
    ++deployCount;
    if (tankToCheckOnDeploy != nullptr && owner != nullptr)
    {{
        sawClearedControllerOnDeploy = (tankToCheckOnDeploy->m_pController == nullptr);
        sawClearedTankOnDeploy = ((CBaseEntity *)owner->m_pTank == nullptr);
    }}
}}
'''

    tests = f'''
int main()
{{
    constexpr bool kHasMaster = {'true' if has_master else 'false'};
    constexpr bool kHasResetMaxSpeed = {'true' if has_reset_max_speed else 'false'};
    constexpr bool kHasUsesModel = {'true' if has_uses_model else 'false'};
    constexpr bool kHasFixedOwner = {'true' if has_fixed_owner else 'false'};
    constexpr bool kHasTfcActivation = {'true' if has_tfc_activation else 'false'};
    (void)kHasUsesModel;
    (void)kHasFixedOwner;
    (void)kHasTfcActivation;

    {class_name} tank;
    tank.pev->spawnflags = SF_TANK_CANCONTROL;

    CBasePlayer player1;
    CBasePlayer player2;
    CBasePlayerItem weapon1;
    weapon1.owner = &player1;
    weapon1.tankToCheckOnDeploy = &tank;
    player1.m_pActiveItem = &weapon1;

    // 1. Direct ControllerPostFrame with NULL controller must not crash or fire.
    tank.ControllerPostFrame();
    assert(0 == tank.fireCount);

    // 2. Use with NULL activator must not crash.
    tank.Use(nullptr, nullptr, USE_TOGGLE, 0.0f);
    assert(nullptr == tank.m_pController);

    // 3. Failed StartControl (locked master) must not leave player1.m_pTank set.
    if (kHasMaster)
    {{
        tank.m_iszMaster = 1;
        g_masterTriggered = FALSE;
        tank.Use(&player1, &player1, USE_TOGGLE, 0.0f);
        assert(nullptr == tank.m_pController);
        assert(nullptr == (CBaseEntity *)player1.m_pTank);

        player1.pev->button = IN_ATTACK;
        tank.Use(&player1, &player1, USE_SET, 2.0f);
        assert(0 == tank.fireCount);

        g_masterTriggered = TRUE;
    }}

    // 4. Player1 takes control; player2 pressing +use or USE_OFF cannot kick player1 off.
    tank.Use(&player1, &player1, USE_TOGGLE, 0.0f);
    assert(&player1 == tank.m_pController);
    assert(&tank == (CBaseEntity *)player1.m_pTank);
    assert(1 == weapon1.holsterCount);
    assert(0 != (player1.m_iHideHUD & HIDEHUD_WEAPONS));

    tank.Use(&player2, &player2, USE_TOGGLE, 0.0f);
    assert(&player1 == tank.m_pController);
    assert(&tank == (CBaseEntity *)player1.m_pTank);
    assert(nullptr == (CBaseEntity *)player2.m_pTank);
    assert(0 == weapon1.deployCount);

    tank.Use(&player2, &player2, USE_OFF, 0.0f);
    assert(&player1 == tank.m_pController);
    assert(&tank == (CBaseEntity *)player1.m_pTank);

    // Non-controller USE_SET(2) must not fire the tank.
    player1.pev->button = IN_ATTACK;
    tank.Use(&player2, &player2, USE_SET, 2.0f);
    assert(0 == tank.fireCount);

    // 5. Controller USE_SET(2) fires normally.
    tank.Use(&player1, &player1, USE_SET, 2.0f);
    assert(1 == tank.fireCount);
    assert(LOUD_GUN_VOLUME == player1.m_iWeaponVolume);

    // 6. Controller releasing control clears both sides before Deploy().
    tank.Use(&player1, &player1, USE_TOGGLE, 0.0f);
    assert(nullptr == tank.m_pController);
    assert(nullptr == (CBaseEntity *)player1.m_pTank);
    assert(1 == weapon1.deployCount);
    assert(weapon1.sawClearedControllerOnDeploy);
    assert(weapon1.sawClearedTankOnDeploy);
    assert(0 == (player1.m_iHideHUD & HIDEHUD_WEAPONS));
    if (kHasResetMaxSpeed)
        assert(1 == player1.resetMaxSpeedCount);

    // Subsequent USE_SET(2) after release must be a no-op.
    gpGlobals->time += 5.0f;
    tank.Use(&player1, &player1, USE_SET, 2.0f);
    assert(1 == tank.fireCount);

    // 7. Direct StopControl() (e.g. PreKillTarget) clears controller's m_pTank.
    tank.Use(&player1, &player1, USE_TOGGLE, 0.0f);
    assert(&player1 == tank.m_pController);
    assert(&tank == (CBaseEntity *)player1.m_pTank);
    tank.StopControl();
    assert(nullptr == tank.m_pController);
    assert(nullptr == (CBaseEntity *)player1.m_pTank);
    tank.ControllerPostFrame();
    assert(1 == tank.fireCount);

    // 8. StopControl() must not clear m_pTank if the player already references a different tank.
    {class_name} otherTank;
    tank.Use(&player1, &player1, USE_TOGGLE, 0.0f);
    player1.m_pTank = &otherTank;
    tank.StopControl();
    assert(nullptr == tank.m_pController);
    assert(&otherTank == (CBaseEntity *)player1.m_pTank);

    return 0;
}}
'''
    return '\n'.join([stubs, production_code, tests])


def main():
    with tempfile.TemporaryDirectory(prefix='goldsrc-func-tank-controller-') as folder:
        work = Path(folder)
        for index, (rel_path, class_name, prefix, has_master, has_reset_max_speed, has_uses_model, has_fixed_owner, has_tfc_activation) in enumerate(TARGETS):
            source = (ROOT / rel_path).read_text(encoding='utf-8')
            functions = [
                extract(source, f'BOOL {prefix}StartControl('),
                extract(source, f'void {prefix}StopControl('),
                extract(source, f'void {prefix}ControllerPostFrame('),
                extract(source, f'void {prefix}Use('),
            ]
            test_source = work / f'test_{index}.cpp'
            test_source.write_text(
                build_test_source(
                    class_name,
                    '\n\n'.join(functions),
                    has_master,
                    has_reset_max_speed,
                    has_uses_model,
                    has_fixed_owner,
                    has_tfc_activation,
                ),
                encoding='utf-8',
            )
            executable = compile_program([test_source], work / f'test_{index}', sanitizers=['address', 'undefined'])
            subprocess.run([str(executable)], cwd=work, check=True)
    print(f'PASS: verified {len(TARGETS)} func_tank implementations for controller lifecycle and null safety (#487)')


if __name__ == '__main__':
    main()
