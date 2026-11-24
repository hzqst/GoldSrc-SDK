"""Regression tests for TRI_COLLIDEALL handling world and non-world entity hits.

Run: python -I tests/particleman_collideall_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
"""
from pathlib import Path
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


STUBS = r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

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
    int ent = -1;
};

struct physent_t {
    int info = 0;
};

enum {
    TRI_COLLIDEWORLD = 0x00000020,
    TRI_COLLIDEALL = 0x00001000,
    TRI_COLLIDEKILL = 0x00004000,
    TRI_WATERTRACE = 0x00032000,
    PM_STUDIO_BOX = 0x00000001,
    PM_WORLD_ONLY = 0x00000002,
    CONTENTS_EMPTY = -1,
    CONTENTS_WATER = -3
};

static float g_clientTime = 2.0f;
static float g_flOldTime = 1.9f;
static float g_flGravity = 800.0f;
static pmtrace_t g_nextTrace;
static physent_t g_physent;

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
static int StubPointContents(Vector, int*) { return CONTENTS_EMPTY; }
static EngineFuncs gEngfuncs = {&g_eventAPI, StubGetClientTime, StubPointContents};

struct CCoreTriangleEffect {
    int m_iCollisionFlags = 0;
    float m_flNextCollisionTime = 0.0f;
    float m_flGravity = 0.0f;
    float m_flBounceFactor = 2.0f;
    float m_flDieTime = 10.0f;
    bool m_bInWater = false;
    Vector m_vPrevOrigin;
    Vector m_vOrigin;
    Vector m_vVelocity;
    Vector m_vAngles;
    Vector m_vAVelocity;

    virtual ~CCoreTriangleEffect() = default;
    virtual void Touch(Vector pos, Vector normal, int index) = 0;
    void CheckCollision(float time);
};
'''

TESTS = r'''
struct RecordingParticle : CCoreTriangleEffect {
    int touchCalls = 0;
    Vector lastPos;
    Vector lastNormal;
    int lastIndex = -1;

    void Touch(Vector pos, Vector normal, int index) override {
        ++touchCalls;
        lastPos = pos;
        lastNormal = normal;
        lastIndex = index;
    }
};

static bool Near(float a, float b) {
    return std::fabs(a - b) < 1e-4f;
}

int main() {
    // 1. World hit (ent == 0) bounces and calls Touch() with index 0.
    {
        RecordingParticle p;
        p.m_iCollisionFlags = TRI_COLLIDEALL;
        p.m_vPrevOrigin = Vector(0.0f, 0.0f, 10.0f);
        p.m_vOrigin = Vector(10.0f, 0.0f, 10.0f);
        p.m_vVelocity = Vector(100.0f, 0.0f, 0.0f);
        p.m_flBounceFactor = 2.0f; // damp = 1.0f

        g_nextTrace = {};
        g_nextTrace.fraction = 0.5f;
        g_nextTrace.endpos[0] = 5.0f;
        g_nextTrace.endpos[1] = 0.0f;
        g_nextTrace.endpos[2] = 10.0f;
        g_nextTrace.plane.normal[0] = -1.0f;
        g_nextTrace.plane.normal[1] = 0.0f;
        g_nextTrace.plane.normal[2] = 0.0f;
        g_nextTrace.ent = 0;

        p.CheckCollision(g_clientTime);

        assert(p.touchCalls == 1);
        assert(p.lastIndex == 0);
        assert(Near(p.lastPos.x, 5.0f) && Near(p.lastNormal.x, -1.0f));
        assert(Near(p.m_vVelocity.x, -100.0f));
        std::puts("PASS: TRI_COLLIDEALL world hit bounces and invokes Touch");
    }

    // 2. Non-world entity hit (ent == 7) must also bounce and call Touch() with index 7.
    {
        RecordingParticle p;
        p.m_iCollisionFlags = TRI_COLLIDEALL;
        p.m_vPrevOrigin = Vector(0.0f, 0.0f, 10.0f);
        p.m_vOrigin = Vector(10.0f, 0.0f, 10.0f);
        p.m_vVelocity = Vector(100.0f, 0.0f, 0.0f);
        p.m_flBounceFactor = 2.0f;

        g_nextTrace = {};
        g_nextTrace.fraction = 0.25f;
        g_nextTrace.endpos[0] = 2.5f;
        g_nextTrace.endpos[1] = 0.0f;
        g_nextTrace.endpos[2] = 10.0f;
        g_nextTrace.plane.normal[0] = -1.0f;
        g_nextTrace.plane.normal[1] = 0.0f;
        g_nextTrace.plane.normal[2] = 0.0f;
        g_nextTrace.ent = 7;

        p.CheckCollision(g_clientTime);

        assert(p.touchCalls == 1);
        assert(p.lastIndex == 7);
        assert(Near(p.lastPos.x, 2.5f) && Near(p.lastNormal.x, -1.0f));
        assert(Near(p.m_vVelocity.x, -100.0f));
        std::puts("PASS: TRI_COLLIDEALL non-world entity hit bounces and invokes Touch");
    }

    // 3. No hit (fraction == 1) leaves velocity untouched and skips Touch().
    {
        RecordingParticle p;
        p.m_iCollisionFlags = TRI_COLLIDEALL;
        p.m_vPrevOrigin = Vector(0.0f, 0.0f, 10.0f);
        p.m_vOrigin = Vector(10.0f, 0.0f, 10.0f);
        p.m_vVelocity = Vector(100.0f, 0.0f, 0.0f);

        g_nextTrace = {};
        g_nextTrace.fraction = 1.0f;
        g_nextTrace.ent = -1;

        p.CheckCollision(g_clientTime);

        assert(p.touchCalls == 0);
        assert(Near(p.m_vVelocity.x, 100.0f));
        std::puts("PASS: TRI_COLLIDEALL miss skips bounce and Touch");
    }

    return 0;
}
'''


def main():
    source = '\n'.join([
        STUBS,
        extract_function('particleman/pman_triangleeffect.cpp', 'void CCoreTriangleEffect::CheckCollision('),
        TESTS,
    ])
    with tempfile.TemporaryDirectory() as tmp:
        src = Path(tmp) / 'particleman_collideall_tests.cpp'
        src.write_text(source, encoding='utf-8')
        exe = compile_program([src], Path(tmp) / 'particleman_collideall_tests',
                              defines=['_CRT_SECURE_NO_WARNINGS'],
                              sanitizers=('address', 'undefined'))
        subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    main()
