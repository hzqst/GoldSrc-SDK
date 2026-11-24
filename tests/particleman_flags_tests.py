"""Regression tests for particle collision/animation flag bit separation and legacy compatibility.

Run: python -I tests/particleman_flags_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

SDK_ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'


def extract_function(relpath, signature):
    source = (SDK_ROOT / relpath).read_text(encoding='utf-8')
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


def extract_tri_defines():
    header = (SDK_ROOT / 'public' / 'pman_triangleffect.h').read_text(encoding='utf-8')
    lines = re.findall(r'^#define\s+TRI_\w+.*$', header, flags=re.MULTILINE)
    assert len(lines) == 6, f'Expected 6 TRI_* defines, got {len(lines)}'
    return '\n'.join(lines)


def extract_legacy_helpers():
    source = (SDK_ROOT / 'particleman' / 'pman_triangleeffect.cpp').read_text(encoding='utf-8')
    start = source.index('static const int TRI_LEGACY_FLAG_BIT')
    end = source.index('\n}', start) + 2
    return source[start:end]


STUBS = r'''
#include <cassert>
#include <cmath>
#include <cstdio>

TRI_DEFINES

struct Vector {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    Vector() = default;
    Vector(float a, float b, float c) : x(a), y(b), z(c) {}
    explicit Vector(const float* v) : x(v[0]), y(v[1]), z(v[2]) {}
    Vector operator+(const Vector& o) const { return Vector(x + o.x, y + o.y, z + o.z); }
    Vector operator*(float s) const { return Vector(x * s, y * s, z * s); }
    float& operator[](int i) { return i == 0 ? x : (i == 1 ? y : z); }
    float operator[](int i) const { return i == 0 ? x : (i == 1 ? y : z); }
    float Length() const { return std::sqrt(x * x + y * y + z * z); }
};

static const Vector g_vecZero(0.0f, 0.0f, 0.0f);

static void VectorScale(const Vector& in, float s, Vector& out) {
    out.x = in.x * s;
    out.y = in.y * s;
    out.z = in.z * s;
}

static void VectorMA(const Vector& start, float scale, const Vector& dir, Vector& out) {
    out.x = start.x + scale * dir.x;
    out.y = start.y + scale * dir.y;
    out.z = start.z + scale * dir.z;
}

struct pmplane_t {
    float normal[3] = {0.0f, 0.0f, 1.0f};
    float dist = 0.0f;
};

struct pmtrace_t {
    float fraction = 1.0f;
    float endpos[3] = {0.0f, 0.0f, 0.0f};
    pmplane_t plane;
    int ent = 0;
};

struct physent_t {
    int info = 0;
};

enum {
    PM_STUDIO_BOX = 0x00000001,
    PM_WORLD_ONLY = 0x00000002,
    CONTENTS_EMPTY = -1,
    CONTENTS_WATER = -3
};

static float g_clientTime = 5.0f;
static float g_flOldTime = 4.9f;
static float g_flGravity = 800.0f;
static pmtrace_t g_nextTrace;
static physent_t g_physent;
static int g_pointContents = CONTENTS_WATER;
static int g_pointContentsCalls = 0;

struct EventAPI {
    void (*EV_SetTraceHull)(int);
    void (*EV_PlayerTrace)(Vector, Vector, int, int, pmtrace_t*);
    physent_t* (*EV_GetPhysent)(int);
};

static void StubSetTraceHull(int) {}
static void StubPlayerTrace(Vector, Vector, int, int, pmtrace_t* tr) { *tr = g_nextTrace; }
static physent_t* StubGetPhysent(int) { return &g_physent; }
static EventAPI g_eventAPI = {StubSetTraceHull, StubPlayerTrace, StubGetPhysent};

struct EngineFuncs {
    EventAPI* pEventAPI;
    float (*GetClientTime)();
    int (*PM_PointContents)(Vector, int*);
};

static float StubGetClientTime() { return g_clientTime; }
static int StubPointContents(Vector, int*) {
    ++g_pointContentsCalls;
    return g_pointContents;
}
static EngineFuncs gEngfuncs = {&g_eventAPI, StubGetClientTime, StubPointContents};

struct CCoreTriangleEffect {
    int m_iCollisionFlags = 0;
    float m_flNextCollisionTime = 0.0f;
    float m_flGravity = 0.0f;
    float m_flBounceFactor = 1.0f;
    float m_flDieTime = 20.0f;
    bool m_bInWater = false;
    Vector m_vPrevOrigin;
    Vector m_vOrigin;
    Vector m_vVelocity;
    Vector m_vAngles;
    Vector m_vAVelocity;

    int animateCalls = 0;
    int animateAndDieCalls = 0;
    int touchCalls = 0;

    virtual ~CCoreTriangleEffect() = default;
    virtual void Animate(float) { ++animateCalls; }
    virtual void AnimateAndDie(float) { ++animateAndDieCalls; }
    virtual void Contract(float) {}
    virtual void Expand(float) {}
    virtual void Fade(float) {}
    virtual void Spin(float) {}
    virtual void CalculateVelocity(float) {}
    virtual void Touch(Vector, Vector, int) { ++touchCalls; }

    void CheckCollision(float time);
    void Think(float time);
};
'''

TESTS = r'''
int main() {
    // 1. All 6 public TRI_* flags must be non-overlapping powers of two.
    const int allFlags[] = {
        TRI_COLLIDEWORLD,
        TRI_COLLIDEALL,
        TRI_COLLIDEKILL,
        TRI_SPIRAL,
        TRI_ANIMATEDIE,
        TRI_WATERTRACE,
    };
    int seen = 0;
    for (int f : allFlags) {
        assert(f != 0 && (f & (f - 1)) == 0);
        assert((seen & f) == 0);
        seen |= f;
    }
    std::puts("PASS: all public TRI_* flags are disjoint powers of two");

    // 2. Verify all 8 combinations of {TRI_COLLIDEKILL, TRI_ANIMATEDIE, TRI_WATERTRACE}.
    for (int mask = 0; mask < 8; ++mask) {
        const bool wantKill = (mask & 1) != 0;
        const bool wantAnimDie = (mask & 2) != 0;
        const bool wantWater = (mask & 4) != 0;
        const int flags =
            (wantKill ? TRI_COLLIDEKILL : 0) |
            (wantAnimDie ? TRI_ANIMATEDIE : 0) |
            (wantWater ? TRI_WATERTRACE : 0);

        // 2a. Think() animation dispatch & watertrace check (no world trace).
        CCoreTriangleEffect p;
        p.m_iCollisionFlags = flags;
        p.m_flDieTime = 20.0f;
        g_pointContents = CONTENTS_WATER;
        g_pointContentsCalls = 0;

        p.Think(g_clientTime);

        assert(p.animateAndDieCalls == (wantAnimDie ? 1 : 0));
        assert(p.animateCalls == (wantAnimDie ? 0 : 1));
        assert(g_pointContentsCalls == (wantWater ? 1 : 0));
        assert(p.touchCalls == (wantWater ? 1 : 0));
        assert(p.m_bInWater == wantWater);
        assert((p.m_flDieTime == g_clientTime) == (wantWater && wantKill));

        // 2b. World collision kill check must depend solely on TRI_COLLIDEKILL.
        CCoreTriangleEffect w;
        w.m_iCollisionFlags = TRI_COLLIDEWORLD | flags;
        w.m_vVelocity = Vector(100.0f, 0.0f, 0.0f);
        w.m_flDieTime = 20.0f;
        g_nextTrace = {};
        g_nextTrace.fraction = 0.5f;
        g_nextTrace.plane.normal[0] = -1.0f;
        g_nextTrace.ent = 0;

        w.CheckCollision(g_clientTime);

        assert((w.m_flDieTime == g_clientTime) == wantKill);
    }
    std::puts("PASS: all 8 combinations of COLLIDEKILL, ANIMATEDIE, and WATERTRACE behave orthogonally");

    // 3. Legacy precompiled client bit values (0x00016000 / 0x00032000).
    constexpr int LEGACY_ANIMATEDIE = 0x00016000;
    constexpr int LEGACY_WATERTRACE = 0x00032000;

    // 3a. Legacy WATERTRACE alone must NOT trigger AnimateAndDie, and MUST trigger water trace.
    {
        CCoreTriangleEffect p;
        p.m_iCollisionFlags = LEGACY_WATERTRACE;
        g_pointContents = CONTENTS_WATER;
        g_pointContentsCalls = 0;
        p.Think(g_clientTime);
        assert(p.animateAndDieCalls == 0 && p.animateCalls == 1);
        assert(g_pointContentsCalls == 1 && p.m_bInWater);
    }

    // 3b. Legacy ANIMATEDIE alone MUST trigger AnimateAndDie, and must NOT trigger water trace.
    {
        CCoreTriangleEffect p;
        p.m_iCollisionFlags = LEGACY_ANIMATEDIE;
        g_pointContents = CONTENTS_WATER;
        g_pointContentsCalls = 0;
        p.Think(g_clientTime);
        assert(p.animateAndDieCalls == 1 && p.animateCalls == 0);
        assert(g_pointContentsCalls == 0 && !p.m_bInWater);
    }

    // 3c. Legacy COLLIDEKILL alone must NOT trigger AnimateAndDie or water trace.
    {
        CCoreTriangleEffect p;
        p.m_iCollisionFlags = TRI_COLLIDEKILL;
        g_pointContents = CONTENTS_WATER;
        g_pointContentsCalls = 0;
        p.Think(g_clientTime);
        assert(p.animateAndDieCalls == 0 && p.animateCalls == 1);
        assert(g_pointContentsCalls == 0 && !p.m_bInWater);
    }
    std::puts("PASS: legacy precompiled 0x16000 / 0x32000 single-flag compatibility preserved");

    return 0;
}
'''


def main():
    source = '\n'.join([
        STUBS.replace('TRI_DEFINES', extract_tri_defines()),
        extract_legacy_helpers(),
        extract_function('particleman/pman_triangleeffect.cpp', 'void CCoreTriangleEffect::CheckCollision('),
        extract_function('particleman/pman_triangleeffect.cpp', 'void CCoreTriangleEffect::Think('),
        TESTS,
    ])
    with tempfile.TemporaryDirectory() as tmp:
        src = Path(tmp) / 'particleman_flags_tests.cpp'
        src.write_text(source, encoding='utf-8')
        exe = compile_program([src], Path(tmp) / 'particleman_flags_tests',
                              defines=['_CRT_SECURE_NO_WARNINGS'],
                              sanitizers=('address', 'undefined'))
        subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    main()
