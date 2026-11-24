"""Regression tests for non-throwing particle allocation when the pool is exhausted or rendering is disabled.

Run: python -I tests/particleman_allocation_tests.py
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


def extract_class_operators():
    header = (SDK_ROOT / 'public' / 'particleman.h').read_text(encoding='utf-8')
    start = header.index('void * operator new( size_t size )')
    end = header.index('\n};', start)
    return header[start:end]


STUBS = r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>

struct Vector {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    Vector() = default;
    Vector(float a, float b, float c) : x(a), y(b), z(c) {}
    Vector(const float* v) : x(v[0]), y(v[1]), z(v[2]) {}
    Vector operator+(const Vector& o) const { return Vector(x + o.x, y + o.y, z + o.z); }
    Vector operator-(const Vector& o) const { return Vector(x - o.x, y - o.y, z - o.z); }
    Vector operator*(float s) const { return Vector(x * s, y * s, z * s); }
    operator float*() { return &x; }
    operator const float*() const { return &x; }
    float Length() const { return std::sqrt(x * x + y * y + z * z); }
    Vector Normalize() const {
        float l = Length();
        return l > 0.0f ? Vector(x / l, y / l, z / l) : Vector();
    }
};

static void VectorMA(const float* a, float s, const Vector& b, Vector& out) {
    out.x = a[0] + s * b.x;
    out.y = a[1] + s * b.y;
    out.z = a[2] + s * b.z;
}

struct model_s {
    int numframes = 1;
};
using model_t = model_s;

struct cvar_t {
    float value = 1.0f;
};

struct entity_state_t {
    float origin[3] = {0.0f, 0.0f, 0.0f};
};

struct cl_entity_t {
    entity_state_t curstate;
};

struct pmtrace_t {
    float endpos[3] = {0.0f, 0.0f, 0.0f};
};

static cl_entity_t g_player;
static pmtrace_t g_trace;
static cvar_t g_particlefx{1.0f};
static cvar_t g_weatherdis{500.0f};
static cvar_t* cl_particlefx = &g_particlefx;
static cvar_t* cl_weatherdis = &g_weatherdis;
static Vector v_origin;
static int g_iRenderMode = 1;
static int g_initCalls = 0;

struct EngineStudio {
    int IsHardware() const { return 1; }
};
static EngineStudio IEngineStudio;

struct WeatherStub {
    model_t splash;
    model_t* GetSplashSprite() { return &splash; }
};
struct HudStub {
    WeatherStub m_Weather;
};
static HudStub gHUD;

struct EngineFuncs {
    void (*Con_DPrintf)(const char*, ...);
    cl_entity_t* (*GetLocalPlayer)();
    float (*GetClientTime)();
    float (*pfnRandomFloat)(float, float);
    long (*pfnRandomLong)(long, long);
    int (*pfnSPR_Load)(const char*);
    const model_s* (*GetSpritePointer)(int);
    pmtrace_t* (*PM_TraceLine)(float*, float*, int, int, int);
};

static model_s g_stubSprite;
static void StubDPrintf(const char*, ...) {}
static cl_entity_t* StubLocalPlayer() { return &g_player; }
static float StubTime() { return 10.0f; }
static float StubRandFloat(float a, float) { return a; }
static long StubRandLong(long a, long) { return a; }
static int StubSprLoad(const char*) { return 1; }
static const model_s* StubGetSprite(int) { return &g_stubSprite; }
static pmtrace_t* StubTraceLine(float*, float*, int, int, int) { return &g_trace; }

static EngineFuncs gEngfuncs = {
    StubDPrintf, StubLocalPlayer, StubTime, StubRandFloat, StubRandLong,
    StubSprLoad, StubGetSprite, StubTraceLine
};

enum {
    kRenderTransAdd = 5,
    kRenderTransAlpha = 4,
    LIGHT_NONE = 1 << 4,
    LIGHT_INTENSITY = 1 << 6,
    CULL_FRUSTUM_SPHERE = 1 << 1,
    CULL_FRUSTUM_PLANE = 1 << 2,
    CULL_PVS = 1 << 3,
    RENDER_FACEPLAYER = 1 << 7,
    RENDER_FACEPLAYER_ROTATEZ = 1 << 8,
    TRI_COLLIDEWORLD = 0x20,
    TRI_COLLIDEKILL = 0x4000,
    TRI_ANIMATEDIE = 0x16000,
    TRI_WATERTRACE = 0x32000,
    DOD_PARTICLE_FIXED_PITCH = 0x80,
    DOD_PARTICLE_WATER_RIPPLE = 0x400,
    DOD_PARTICLE_RAIN_ANGLE = 0x2000,
    DOD_PARTICLE_WATER_SPLASH = 0x4000
};

struct CMiniMem {
    static constexpr int kCapacity = 2;
    alignas(16) char storage[kCapacity][512];
    int used = 0;

    static CMiniMem* Instance() {
        static CMiniMem pool;
        return &pool;
    }
    void Reset() { used = 0; }
    char* newBlock() {
        if (used >= kCapacity)
            return nullptr;
        return storage[used++];
    }
};

struct CBaseParticle;

struct IParticleMan {
    virtual ~IParticleMan() = default;
    virtual char* RequestNewMemBlock(int size) = 0;
    virtual CBaseParticle* CreateParticle(Vector origin, Vector normal, model_s* sprite, float size,
        float brightness, const char* classname) = 0;
};

static IParticleMan* g_pParticleMan = nullptr;

struct CBaseParticle {
    float m_flSize = 0.0f;
    float m_flScaleSpeed = 0.0f;
    float m_flStretchY = 1.0f;
    float m_flBrightness = 0.0f;
    float m_flFadeSpeed = 0.0f;
    float m_flTimeCreated = 1.0f;
    float m_flDieTime = 0.0f;
    float m_flGravity = 0.0f;
    float m_flMass = 1.0f;
    int m_iFramerate = 0;
    int m_iNumFrames = 0;
    int m_iRendermode = 0;
    Vector m_vOrigin;
    Vector m_vVelocity;
    Vector m_vAVelocity;
    Vector m_vColor;
    char m_szClassname[32] = {};
    bool m_bAffectedByForce = false;

    virtual ~CBaseParticle() = default;
    virtual void InitializeSprite(Vector, Vector, model_s*, float, float) {
        ++g_initCalls;
        std::strcpy(m_szClassname, "particle");
    }
    void SetLightFlag(int) {}
    void SetCullFlag(int) {}
    void SetCollisionFlags(int) {}
    void SetRenderFlag(int) {}

    CLASS_OPERATORS
};

struct IParticleMan_Active : IParticleMan {
    char* RequestNewMemBlock(int size) override;
    CBaseParticle* CreateParticle(Vector origin, Vector normal, model_s* sprite, float size,
        float brightness, const char* classname) override;
};

struct TriangleWallPuff : CBaseParticle {
    float m_flActivateTime = 0.0f;
};

struct CDoDParticle : CBaseParticle {
    bool m_bInsideSmoke = false;
    bool m_bSpawnInside = false;
    bool m_bDampMod = false;
    bool m_bAffectedByForce = false;
    int m_iPFlags = 0;
    static Vector m_vGlobalWind;

    void Touch(Vector pos, Vector normal, int index);
    void AddGlobalWind() {}
    static CDoDParticle* Create(Vector org, Vector normal, model_s* sprite, float size,
        float brightness, const char* classname, bool distCull);
};
Vector CDoDParticle::m_vGlobalWind;

struct CDoDRocketTrail : CBaseParticle {
    static CDoDRocketTrail* Create(Vector org, Vector normal, model_s* sprite, float size,
        float brightness, const char* classname);
};

struct CDoDDirtExploDust : CBaseParticle {
    static CDoDDirtExploDust* Create(Vector org, Vector normal, model_s* sprite, float size,
        float brightness, const char* classname);
};

struct CDoDSnowFlake : CDoDParticle {
    float m_flOldTime = 0.0f;
    static CDoDSnowFlake* Create(Vector org, Vector normal, model_s* sprite, float size,
        float brightness, const char* classname);
};

struct CDoDRainDrop : CDoDParticle {};

struct CWeatherManager {
    model_t* m_pRainSprite = &g_stubSprite;
    void CreateRainParticle(float* origin);
};

struct CPartSmokeGrenade : CBaseParticle {
    static CPartSmokeGrenade* Create(Vector origin, Vector normal, model_s* sprite,
        float size, float brightness, const char* classname);
};
'''

TESTS = r'''
int main() {
    IParticleMan_Active pman;
    g_pParticleMan = &pman;
    model_s sprite;

    // 1. Exhaust pool through CreateParticle, then verify every allocation path returns null safely without calling InitializeSprite.
    CMiniMem::Instance()->Reset();
    g_iRenderMode = 1;
    g_initCalls = 0;

    CBaseParticle* p1 = pman.CreateParticle(Vector(), Vector(), &sprite, 1.0f, 255.0f, "p1");
    CBaseParticle* p2 = pman.CreateParticle(Vector(), Vector(), &sprite, 1.0f, 255.0f, "p2");
    assert(p1 != nullptr && p2 != nullptr);
    assert(g_initCalls == 2);

    // Pool is now full (capacity 2).
    assert(pman.CreateParticle(Vector(), Vector(), &sprite, 1.0f, 255.0f, "p3") == nullptr);
    assert(CDoDParticle::Create(Vector(), Vector(), &sprite, 1.0f, 255.0f, "dod", false) == nullptr);
    assert(CDoDRocketTrail::Create(Vector(), Vector(), &sprite, 1.0f, 255.0f, "trail") == nullptr);
    assert(CDoDDirtExploDust::Create(Vector(), Vector(), &sprite, 1.0f, 255.0f, "dust") == nullptr);
    assert(CDoDSnowFlake::Create(Vector(), Vector(), &sprite, 1.0f, 255.0f, "snow") == nullptr);
    assert(CPartSmokeGrenade::Create(Vector(), Vector(), &sprite, 1.0f, 255.0f, "smoke") == nullptr);

    CreateDebrisWallPuff(Vector(), Vector(), Vector(), 0);
    CDoDParticle splashOwner;
    splashOwner.m_iPFlags = DOD_PARTICLE_WATER_SPLASH;
    splashOwner.Touch(Vector(), Vector(0.0f, 0.0f, 1.0f), 0);
    CWeatherManager weather;
    float rainOrigin[3] = {0.0f, 0.0f, 0.0f};
    weather.CreateRainParticle(rainOrigin);
    assert(g_initCalls == 2);
    std::puts("PASS: exhausted pool returns null safely across particleman, DoD, and cstrike");

    // 2. Rendering disabled (g_iRenderMode == 0) must also fail cleanly even with free blocks.
    CMiniMem::Instance()->Reset();
    g_iRenderMode = 0;
    assert(pman.CreateParticle(Vector(), Vector(), &sprite, 1.0f, 255.0f, "disabled") == nullptr);
    assert(CDoDParticle::Create(Vector(), Vector(), &sprite, 1.0f, 255.0f, "disabled", false) == nullptr);
    assert(CMiniMem::Instance()->used == 0);
    assert(g_initCalls == 2);
    std::puts("PASS: disabled render mode returns null without consuming blocks or initializing");

    // 3. CPartSmokeGrenade::Create must check null sprite before consuming a pool block.
    CMiniMem::Instance()->Reset();
    g_iRenderMode = 1;
    assert(CPartSmokeGrenade::Create(Vector(), Vector(), nullptr, 1.0f, 255.0f, "smoke") == nullptr);
    assert(CMiniMem::Instance()->used == 0);
    std::puts("PASS: CPartSmokeGrenade::Create rejects null sprite before allocation");

    // 4. Null g_pParticleMan must return null safely from operator new.
    g_pParticleMan = nullptr;
    assert((new (std::nothrow) CBaseParticle) == nullptr);
    assert((new CBaseParticle) == nullptr);
    std::puts("PASS: null g_pParticleMan returns null safely from both operator new overloads");

    return 0;
}
'''


def main():
    source = '\n'.join([
        STUBS.replace('CLASS_OPERATORS', extract_class_operators()),
        extract_function('particleman/pman_main.cpp', 'char* IParticleMan_Active::RequestNewMemBlock('),
        extract_function('particleman/pman_main.cpp', 'CBaseParticle* IParticleMan_Active::CreateParticle('),
        extract_function('dod/cl_dll/tri.cpp', 'void CreateDebrisWallPuff('),
        extract_function('dod/cl_dll/tri.cpp', 'void CDoDParticle::Touch('),
        extract_function('dod/cl_dll/tri.cpp', 'CDoDParticle *CDoDParticle::Create('),
        extract_function('dod/cl_dll/tri.cpp', 'CDoDRocketTrail *CDoDRocketTrail::Create('),
        extract_function('dod/cl_dll/tri.cpp', 'CDoDDirtExploDust *CDoDDirtExploDust::Create('),
        extract_function('dod/cl_dll/weather.cpp', 'CDoDSnowFlake *CDoDSnowFlake::Create('),
        extract_function('dod/cl_dll/weather.cpp', 'void CWeatherManager::CreateRainParticle('),
        extract_function('cstrike/cl_dll/tri.cpp', 'CPartSmokeGrenade* CPartSmokeGrenade::Create('),
        TESTS,
    ])
    with tempfile.TemporaryDirectory() as tmp:
        src = Path(tmp) / 'particleman_allocation_tests.cpp'
        src.write_text(source, encoding='utf-8')
        exe = compile_program([src], Path(tmp) / 'particleman_allocation_tests',
                              defines=['_CRT_SECURE_NO_WARNINGS'],
                              sanitizers=('address', 'undefined'))
        result = subprocess.run([str(exe)], check=True, text=True, capture_output=True)
        print(result.stdout, end='')


if __name__ == '__main__':
    main()
