"""Regression tests for stretched particle visibility culling matching Draw() quad bounds.

Run: python -I tests/particleman_visibility_tests.py
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

struct Vector {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    Vector() = default;
    Vector(float a, float b, float c) : x(a), y(b), z(c) {}
    Vector operator+(const Vector& o) const { return Vector(x + o.x, y + o.y, z + o.z); }
    Vector operator-(const Vector& o) const { return Vector(x - o.x, y - o.y, z - o.z); }
    Vector operator*(float s) const { return Vector(x * s, y * s, z * s); }
    float& operator[](int i) { return i == 0 ? x : (i == 1 ? y : z); }
    float operator[](int i) const { return i == 0 ? x : (i == 1 ? y : z); }
    float Length() const { return std::sqrt(x * x + y * y + z * z); }
};

enum {
    CULL_FRUSTUM_POINT = 1 << 0,
    CULL_FRUSTUM_SPHERE = 1 << 1,
    CULL_FRUSTUM_PLANE = 1 << 2,
    CULL_PVS = 1 << 3,
    RENDER_FACEPLAYER = 1 << 7,
    RENDER_FACEPLAYER_ROTATEZ = 1 << 8
};

enum PlaneData { A = 0, B = 1, C = 2, D = 3 };

class CFrustum {
public:
    float g_flFrustum[6][4] = {};

    void SetAxisAlignedBoxFrustum(float minX, float maxX, float minY, float maxY, float minZ, float maxZ) {
        // Plane inward normals: A*x + B*y + C*z + D > 0 is inside.
        g_flFrustum[0][A] =  1.0f; g_flFrustum[0][B] =  0.0f; g_flFrustum[0][C] =  0.0f; g_flFrustum[0][D] = -minX;
        g_flFrustum[1][A] = -1.0f; g_flFrustum[1][B] =  0.0f; g_flFrustum[1][C] =  0.0f; g_flFrustum[1][D] =  maxX;
        g_flFrustum[2][A] =  0.0f; g_flFrustum[2][B] =  1.0f; g_flFrustum[2][C] =  0.0f; g_flFrustum[2][D] = -minY;
        g_flFrustum[3][A] =  0.0f; g_flFrustum[3][B] = -1.0f; g_flFrustum[3][C] =  0.0f; g_flFrustum[3][D] =  maxY;
        g_flFrustum[4][A] =  0.0f; g_flFrustum[4][B] =  0.0f; g_flFrustum[4][C] =  1.0f; g_flFrustum[4][D] = -minZ;
        g_flFrustum[5][A] =  0.0f; g_flFrustum[5][B] =  0.0f; g_flFrustum[5][C] = -1.0f; g_flFrustum[5][D] =  maxZ;
    }

    bool PointInsideFrustum(float x, float y, float z);
    bool SphereInsideFrustum(float x, float y, float z, float radius);
    bool BoxInsideFrustum(float minX, float minY, float minZ, float maxX, float maxY, float maxZ);
};

static CFrustum g_cFrustum;
static Vector g_vViewAngles;
static float g_clientTime = 1.0f;
static Vector g_lastPvsMins;
static Vector g_lastPvsMaxs;
static int g_pvsResult = 1;

struct TriAPI {
    int (*BoxInPVS)(Vector, Vector);
};

static int StubBoxInPVS(Vector mins, Vector maxs) {
    g_lastPvsMins = mins;
    g_lastPvsMaxs = maxs;
    return g_pvsResult;
}
static TriAPI g_triAPI = {StubBoxInPVS};

static void StubAngleVectors(Vector angles, Vector& forward, Vector& right, Vector& up) {
    constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
    float sp = std::sin(angles.x * kDegToRad);
    float cp = std::cos(angles.x * kDegToRad);
    float sy = std::sin(angles.y * kDegToRad);
    float cy = std::cos(angles.y * kDegToRad);
    float sr = std::sin(angles.z * kDegToRad);
    float cr = std::cos(angles.z * kDegToRad);

    forward = Vector(cp * cy, cp * sy, -sp);
    right = Vector(-sr * sp * cy + cr * sy, -sr * sp * sy - cr * cy, -sr * cp);
    up = Vector(cr * sp * cy + sr * sy, cr * sp * sy - sr * cy, cr * cp);
}

struct EngineFuncs {
    TriAPI* pTriAPI;
    float (*GetClientTime)();
    void (*pfnAngleVectors)(Vector, Vector&, Vector&, Vector&);
};

static float StubGetClientTime() { return g_clientTime; }
static EngineFuncs gEngfuncs = {&g_triAPI, StubGetClientTime, StubAngleVectors};

struct CCoreTriangleEffect {
    int m_iRenderFlags = 0;
    float m_flNextPVSCheck = 0.0f;
    bool m_bInPVS = true;
    float m_flSize = 2.0f;
    float m_flStretchX = 1.0f;
    float m_flStretchY = 1.0f;
    Vector m_vOrigin;
    Vector m_vAngles;

    bool CheckVisibility();
};
'''

TESTS = r'''
static bool Near(float a, float b) {
    return std::fabs(a - b) < 1e-3f;
}

int main() {
    // Frustum volume: x in [10, 500], y in [-100, 100], z in [0, 200].
    g_cFrustum.SetAxisAlignedBoxFrustum(10.0f, 500.0f, -100.0f, 100.0f, 0.0f, 200.0f);
    g_pvsResult = 1;

    // 1. Condition Zero raindrop (size = 2, stretchY = 40 -> spans [origin.z - 1, origin.z + 79]).
    //    Origin at z = -50 is outside the frustum bottom plane (z = 0), but the upper 29 units
    //    extend into z in [0, 29], so CULL_FRUSTUM_PLANE must keep it visible and BoxInPVS must cover it.
    {
        CCoreTriangleEffect rain;
        rain.m_flSize = 2.0f;
        rain.m_flStretchX = 1.0f;
        rain.m_flStretchY = 40.0f;
        rain.m_vOrigin = Vector(100.0f, 0.0f, -50.0f);
        rain.m_vAngles = Vector(0.0f, 0.0f, 0.0f);
        rain.m_iRenderFlags = CULL_FRUSTUM_PLANE | CULL_PVS;
        rain.m_flNextPVSCheck = 0.0f;

        assert(rain.CheckVisibility() == true);
        assert(Near(g_lastPvsMins.z, -51.0f) && Near(g_lastPvsMaxs.z, 29.0f));
        assert(Near(g_lastPvsMins.y, -1.0f) && Near(g_lastPvsMaxs.y, 1.0f));
        std::puts("PASS: stretched CZ raindrop straddling the bottom frustum edge stays visible (CULL_FRUSTUM_PLANE)");
    }

    // 2. Same stretched raindrop shifted completely below the frustum (z in [-91, -11]) must be culled.
    {
        CCoreTriangleEffect rain;
        rain.m_flSize = 2.0f;
        rain.m_flStretchX = 1.0f;
        rain.m_flStretchY = 40.0f;
        rain.m_vOrigin = Vector(100.0f, 0.0f, -90.0f);
        rain.m_vAngles = Vector(0.0f, 0.0f, 0.0f);
        rain.m_iRenderFlags = CULL_FRUSTUM_PLANE | CULL_PVS;

        assert(rain.CheckVisibility() == false);
        std::puts("PASS: stretched raindrop completely outside frustum is culled (CULL_FRUSTUM_PLANE)");
    }

    // 3. Thin vertical raindrop placed 10 units outside the right frustum plane (y = 110, width = 2 -> y in [109, 111])
    //    must be culled (tight AABB avoids over-expanding horizontal extent to +-40).
    {
        CCoreTriangleEffect rain;
        rain.m_flSize = 2.0f;
        rain.m_flStretchX = 1.0f;
        rain.m_flStretchY = 40.0f;
        rain.m_vOrigin = Vector(100.0f, 110.0f, 50.0f);
        rain.m_vAngles = Vector(0.0f, 0.0f, 0.0f);
        rain.m_iRenderFlags = CULL_FRUSTUM_PLANE | CULL_PVS;

        assert(rain.CheckVisibility() == false);
        std::puts("PASS: tall thin raindrop outside horizontal frustum plane is culled tightly");
    }

    // 4. CULL_FRUSTUM_SPHERE also uses the stretched quad center & half-diagonal radius.
    {
        CCoreTriangleEffect rain;
        rain.m_flSize = 2.0f;
        rain.m_flStretchX = 1.0f;
        rain.m_flStretchY = 40.0f;
        rain.m_vOrigin = Vector(100.0f, 0.0f, -50.0f);
        rain.m_vAngles = Vector(0.0f, 0.0f, 0.0f);
        rain.m_iRenderFlags = CULL_FRUSTUM_SPHERE | CULL_PVS;

        assert(rain.CheckVisibility() == true);

        rain.m_vOrigin = Vector(100.0f, 0.0f, -90.0f);
        assert(rain.CheckVisibility() == false);
        std::puts("PASS: CULL_FRUSTUM_SPHERE reflects stretched quad center and radius");
    }

    // 5. RENDER_FACEPLAYER uses g_vViewAngles for bounds without mutating m_vAngles.
    {
        g_vViewAngles = Vector(0.0f, 0.0f, 0.0f);
        CCoreTriangleEffect bill;
        bill.m_flSize = 2.0f;
        bill.m_flStretchX = 1.0f;
        bill.m_flStretchY = 40.0f;
        bill.m_vOrigin = Vector(100.0f, 0.0f, -50.0f);
        bill.m_vAngles = Vector(90.0f, 0.0f, 0.0f); // would point up along X if not overridden by RENDER_FACEPLAYER
        bill.m_iRenderFlags = CULL_FRUSTUM_PLANE | RENDER_FACEPLAYER;

        assert(bill.CheckVisibility() == true);
        assert(Near(bill.m_vAngles.x, 90.0f));
        std::puts("PASS: RENDER_FACEPLAYER computes visibility bounds using view angles without mutating m_vAngles");
    }

    return 0;
}
'''


def main():
    source = '\n'.join([
        STUBS,
        extract_function('particleman/pman_frustum.cpp', 'bool CFrustum::PointInsideFrustum('),
        extract_function('particleman/pman_frustum.cpp', 'bool CFrustum::SphereInsideFrustum('),
        extract_function('particleman/pman_frustum.cpp', 'bool CFrustum::BoxInsideFrustum('),
        extract_function('particleman/pman_triangleeffect.cpp', 'bool CCoreTriangleEffect::CheckVisibility('),
        TESTS,
    ])
    with tempfile.TemporaryDirectory() as tmp:
        src = Path(tmp) / 'particleman_visibility_tests.cpp'
        src.write_text(source, encoding='utf-8')
        exe = compile_program([src], Path(tmp) / 'particleman_visibility_tests',
                              defines=['_CRT_SECURE_NO_WARNINGS'],
                              sanitizers=('address', 'undefined'))
        subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    main()
