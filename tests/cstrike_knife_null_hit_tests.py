"""Regression tests for CKnife::Stab and CKnife::Swing when CBaseEntity::Instance returns NULL (#507)."""
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
#include <cmath>
#include <cstring>

struct Vector2D {
    float x = 0.0f, y = 0.0f;
    Vector2D() = default;
    Vector2D(float a, float b) : x(a), y(b) {}
    Vector2D Normalize() const {
        float len = std::sqrt(x * x + y * y);
        return len > 0.0f ? Vector2D(x / len, y / len) : Vector2D(0.0f, 0.0f);
    }
};

static float DotProduct(const Vector2D& a, const Vector2D& b) {
    return a.x * b.x + a.y * b.y;
}

struct Vector {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    Vector() = default;
    Vector(float a, float b, float c) : x(a), y(b), z(c) {}
    Vector operator+(const Vector& o) const { return Vector(x + o.x, y + o.y, z + o.z); }
    Vector operator-(const Vector& o) const { return Vector(x - o.x, y - o.y, z - o.z); }
    Vector operator*(float s) const { return Vector(x * s, y * s, z * s); }
    Vector2D Make2D() const { return Vector2D(x, y); }
};

struct edict_t {
    void* pvPrivateData = nullptr;
};

struct entvars_t {
    Vector v_angle;
    Vector angles;
    float nextthink = 0.0f;
};

struct TraceResult {
    float flFraction = 1.0f;
    Vector vecEndPos;
    edict_t* pHit = nullptr;
};

enum {
    dont_ignore_monsters = 0,
    head_hull = 1,
    PLAYER_ATTACK1 = 5,
    KNIFE_IDLE = 0,
    KNIFE_MIDATTACK1HIT = 4,
    KNIFE_MIDATTACK2HIT = 5,
    KNIFE_STABHIT = 6,
    KNIFE_STABMISS = 7,
    KNIFE_SHIELD_ATTACKHIT = 9,
    CHAN_WEAPON = 1,
    CHAN_ITEM = 3,
    ATTN_NORM = 1,
    DMG_BULLET = (1 << 1),
    DMG_NEVERGIB = (1 << 12),
    CLASS_NONE = 0,
    CLASS_MACHINE = 1,
    CLASS_PLAYER = 2,
    KNIFE_BODYHIT_VOLUME = 128,
    KNIFE_WALLHIT_VOLUME = 512,
    BULLET_PLAYER_CROWBAR = 12,
};

struct Globals {
    Vector v_forward{1.0f, 0.0f, 0.0f};
};
static Globals g_globals;
static Globals* gpGlobals = &g_globals;

struct CBaseEntity {
    entvars_t ev;
    entvars_t* pev = &ev;
    bool bspModel = false;
    bool isPlayer = false;
    bool isAlive = true;
    int classify = CLASS_NONE;
    int traceAttackCalls = 0;
    float lastDamage = 0.0f;

    static CBaseEntity* Instance(edict_t* pent) {
        return pent ? static_cast<CBaseEntity*>(pent->pvPrivateData) : nullptr;
    }

    bool IsBSPModel() const { return bspModel; }
    bool IsPlayer() const { return isPlayer; }
    bool IsAlive() const { return isAlive; }
    int Classify() const { return classify; }
    void TraceAttack(entvars_t*, float flDamage, Vector, TraceResult*, int) {
        ++traceAttackCalls;
        lastDamage = flDamage;
    }
};

static edict_t g_hitEdict;
static float g_traceLineFraction = 0.5f;
static int g_clearMultiDamageCalls = 0;
static int g_applyMultiDamageCalls = 0;
static int g_textureSoundCalls = 0;
static int g_emitSoundCalls = 0;

static void UTIL_MakeVectors(const Vector&) {
    gpGlobals->v_forward = Vector(1.0f, 0.0f, 0.0f);
}

static void UTIL_TraceLine(const Vector&, const Vector& end, int, edict_t*, TraceResult* tr) {
    tr->flFraction = g_traceLineFraction;
    tr->vecEndPos = end;
    tr->pHit = &g_hitEdict;
}

static void UTIL_TraceHull(const Vector&, const Vector& end, int, int, edict_t*, TraceResult* tr) {
    tr->flFraction = 0.5f;
    tr->vecEndPos = end;
    tr->pHit = &g_hitEdict;
}

static void FindHullIntersection(const Vector&, TraceResult&, Vector, Vector, edict_t*) {}
static int RANDOM_LONG(int lo, int) { return lo; }
static void EMIT_SOUND(edict_t*, int, const char*, float, float) { ++g_emitSoundCalls; }
static void EMIT_SOUND_DYN(edict_t*, int, const char*, float, float, int, int) { ++g_emitSoundCalls; }
static void TEXTURETYPE_PlaySound(TraceResult*, Vector, Vector, int) { ++g_textureSoundCalls; }
static void ClearMultiDamage() { ++g_clearMultiDamageCalls; }
static void ApplyMultiDamage(entvars_t*, entvars_t*) { ++g_applyMultiDamageCalls; }

struct CBasePlayer {
    entvars_t ev;
    entvars_t* pev = &ev;
    edict_t ed;
    int m_iWeaponVolume = 0;
    bool hasShield = false;

    edict_t* edict() { return &ed; }
    Vector GetGunPosition() const { return Vector(0.0f, 0.0f, 0.0f); }
    void SetAnimation(int) {}
    bool HasShield() const { return hasShield; }
};

struct CKnife {
    entvars_t ev;
    entvars_t* pev = &ev;
    CBasePlayer* m_pPlayer = nullptr;
    TraceResult m_trHit;
    float m_flNextPrimaryAttack = 0.0f;
    float m_flNextSecondaryAttack = 0.0f;
    float m_flTimeWeaponIdle = 0.0f;
    int m_iSwing = 0;

    bool UseDecrement() const { return false; }
    void SendWeaponAnim(int, bool) {}
    float GetNextAttackDelay(float d) const { return d; }
    void SetThink(void (CKnife::*)()) {}
    void Smack() {}
    void SetPlayerShieldAnim() {}
    void ResetPlayerShieldAnim() {}

    int Swing( int fFirst );
    int Stab( int fFirst );
};
'''

TESTS = r'''
static void ResetCounters() {
    g_clearMultiDamageCalls = 0;
    g_applyMultiDamageCalls = 0;
    g_textureSoundCalls = 0;
    g_emitSoundCalls = 0;
}

int main() {
    // 1. Stab hitting an edict with NULL pvPrivateData (Instance returns NULL):
    //    must not dereference NULL, skips TraceAttack, plays wall-hit sound.
    {
        ResetCounters();
        g_hitEdict.pvPrivateData = nullptr;
        g_traceLineFraction = 0.5f;

        CBasePlayer player;
        CKnife knife;
        knife.m_pPlayer = &player;

        int res = knife.Stab(1);
        assert(1 == res);
        assert(0 == g_clearMultiDamageCalls);
        assert(0 == g_applyMultiDamageCalls);
        assert(1 == g_textureSoundCalls);
        assert(KNIFE_WALLHIT_VOLUME == player.m_iWeaponVolume);
    }

    // 2. Swing hitting an edict with NULL pvPrivateData:
    //    must not dereference NULL, skips TraceAttack, plays wall-hit sound.
    {
        ResetCounters();
        g_hitEdict.pvPrivateData = nullptr;
        g_traceLineFraction = 0.5f;

        CBasePlayer player;
        CKnife knife;
        knife.m_pPlayer = &player;

        int res = knife.Swing(1);
        assert(1 == res);
        assert(0 == g_clearMultiDamageCalls);
        assert(0 == g_applyMultiDamageCalls);
        assert(1 == g_textureSoundCalls);
        assert(KNIFE_WALLHIT_VOLUME == player.m_iWeaponVolume);
    }

    // 3. Stab hitting a valid enemy player (backstab) still applies 3x damage.
    {
        ResetCounters();
        CBaseEntity target;
        target.isPlayer = true;
        target.isAlive = true;
        target.classify = CLASS_PLAYER;
        g_hitEdict.pvPrivateData = &target;
        g_traceLineFraction = 0.5f;

        CBasePlayer player;
        CKnife knife;
        knife.m_pPlayer = &player;

        int res = knife.Stab(1);
        assert(1 == res);
        assert(1 == g_clearMultiDamageCalls);
        assert(1 == g_applyMultiDamageCalls);
        assert(1 == target.traceAttackCalls);
        assert(195.0f == target.lastDamage);
    }

    // 4. Swing hitting a valid entity still applies normal damage.
    {
        ResetCounters();
        CBaseEntity target;
        target.isPlayer = false;
        target.isAlive = true;
        target.classify = CLASS_PLAYER;
        g_hitEdict.pvPrivateData = &target;
        g_traceLineFraction = 0.5f;

        CBasePlayer player;
        CKnife knife;
        knife.m_pPlayer = &player;

        int res = knife.Swing(1);
        assert(1 == res);
        assert(1 == g_clearMultiDamageCalls);
        assert(1 == g_applyMultiDamageCalls);
        assert(1 == target.traceAttackCalls);
        assert(15.0f == target.lastDamage);
    }

    return 0;
}
'''


def main():
    swing = extract('cstrike/dlls/wpn_shared/wpn_knife.cpp', 'int CKnife::Swing( int fFirst )')
    stab = extract('cstrike/dlls/wpn_shared/wpn_knife.cpp', 'int CKnife::Stab( int fFirst )')
    with tempfile.TemporaryDirectory(prefix='goldsrc-cstrike-knife-null-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, swing, stab, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
    print('PASS: CKnife::Stab and CKnife::Swing guard NULL hit before TraceAttack')


if __name__ == '__main__':
    main()
