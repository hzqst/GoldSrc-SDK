"""Regression tests for bounded particle m_szClassname copies in particleman and DoD.

Run: python -I tests/particleman_classname_tests.py
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


def extract_classname_field():
    header = (SDK_ROOT / 'public' / 'pman_triangleffect.h').read_text(encoding='utf-8')
    match = re.search(r'char\s+m_szClassname\[\d+\];', header)
    assert match, 'm_szClassname field not found in public/pman_triangleffect.h'
    return match.group(0)


STUBS = r'''
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>

struct Vector {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    Vector() = default;
    Vector(float a, float b, float c) : x(a), y(b), z(c) {}
    float Length() const { return std::sqrt(x * x + y * y + z * z); }
};

struct model_s {
    int numframes = 1;
};

struct cvar_t {
    float value = 1.0f;
};

struct entity_state_t {
    float origin[3] = {0.0f, 0.0f, 0.0f};
};

struct cl_entity_t {
    entity_state_t curstate;
};

static cl_entity_t g_player;
static cvar_t g_particlefx{1.0f};
static cvar_t g_weatherdis{500.0f};
static cvar_t* cl_particlefx = &g_particlefx;
static cvar_t* cl_weatherdis = &g_weatherdis;

struct EngineStudio {
    int IsHardware() const { return 1; }
};
static EngineStudio IEngineStudio;

struct EngineFuncs {
    void (*Con_DPrintf)(const char*, ...);
    cl_entity_t* (*GetLocalPlayer)();
};

static void StubDPrintf(const char*, ...) {}
static cl_entity_t* StubLocalPlayer() { return &g_player; }
static EngineFuncs gEngfuncs = {StubDPrintf, StubLocalPlayer};

struct CBaseParticle {
    float m_flTimeCreated = 1.0f;
    int m_iNumFrames = 0;
    CLASSNAME_FIELD
    uint32_t canary = 0xDEADBEEFu;

    void InitializeSprite(Vector, Vector, model_s*, float, float) {
        std::strcpy(m_szClassname, "particle");
    }
};

struct IParticleMan_Active {
    CBaseParticle* CreateParticle(Vector origin, Vector normal, model_s* sprite, float size,
        float brightness, const char* classname);
};

struct CDoDParticle : CBaseParticle {
    bool m_bInsideSmoke = false;
    bool m_bSpawnInside = false;
    bool m_bDampMod = false;
    bool m_bAffectedByForce = false;
    int m_iPFlags = 0;

    static CDoDParticle* Create(Vector org, Vector normal, model_s* sprite, float size,
        float brightness, const char* classname, bool distCull);
};

struct CDoDSnowFlake : CDoDParticle {
    float m_flOldTime = 0.0f;

    static CDoDSnowFlake* Create(Vector org, Vector normal, model_s* sprite, float size,
        float brightness, const char* classname);
};
'''

TESTS = r'''
static void VerifyClassname(const char* label, const char* input, const char* actual, uint32_t canary, size_t capacity) {
    assert(canary == 0xDEADBEEFu);
    assert(std::memchr(actual, '\0', capacity) != nullptr);
    if (input == nullptr) {
        assert(std::strcmp(actual, "particle") == 0);
    } else {
        std::string expected(input);
        if (expected.size() >= capacity) {
            expected.resize(capacity - 1);
        }
        assert(expected == actual);
    }
    std::printf("PASS: %s\n", label);
}

int main() {
    model_s sprite;
    IParticleMan_Active pman;
    const std::string name31(31, 'A');
    const std::string name32(32, 'B');
    const std::string name48(48, 'C');

    const char* cases[] = {"spark", name31.c_str(), name32.c_str(), name48.c_str(), nullptr};
    const char* labels[] = {"short", "31-byte", "32-byte", "48-byte", "null"};

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        CBaseParticle* p = pman.CreateParticle(Vector(), Vector(), &sprite, 1.0f, 255.0f, cases[i]);
        assert(p != nullptr);
        VerifyClassname((std::string("CreateParticle ") + labels[i]).c_str(),
            cases[i], p->m_szClassname, p->canary, sizeof(p->m_szClassname));
        delete p;

        CDoDParticle* dod = CDoDParticle::Create(Vector(), Vector(), &sprite, 1.0f, 255.0f, cases[i], false);
        assert(dod != nullptr);
        VerifyClassname((std::string("CDoDParticle::Create ") + labels[i]).c_str(),
            cases[i], dod->m_szClassname, dod->canary, sizeof(dod->m_szClassname));
        delete dod;

        CDoDSnowFlake* snow = CDoDSnowFlake::Create(Vector(), Vector(), &sprite, 1.0f, 255.0f, cases[i]);
        assert(snow != nullptr);
        VerifyClassname((std::string("CDoDSnowFlake::Create ") + labels[i]).c_str(),
            cases[i], snow->m_szClassname, snow->canary, sizeof(snow->m_szClassname));
        delete snow;
    }

    return 0;
}
'''


def main():
    source = '\n'.join([
        STUBS.replace('CLASSNAME_FIELD', extract_classname_field()),
        extract_function('particleman/pman_main.cpp', 'CBaseParticle* IParticleMan_Active::CreateParticle('),
        extract_function('dod/cl_dll/tri.cpp', 'CDoDParticle *CDoDParticle::Create('),
        extract_function('dod/cl_dll/weather.cpp', 'CDoDSnowFlake *CDoDSnowFlake::Create('),
        TESTS,
    ])
    with tempfile.TemporaryDirectory() as tmp:
        src = Path(tmp) / 'particleman_classname_tests.cpp'
        src.write_text(source, encoding='utf-8')
        exe = compile_program([src], Path(tmp) / 'particleman_classname_tests',
                              defines=['_CRT_SECURE_NO_WARNINGS'],
                              sanitizers=('address', 'undefined'))
        result = subprocess.run([str(exe)], check=True, text=True, capture_output=True)
        print(result.stdout, end='')


if __name__ == '__main__':
    main()
