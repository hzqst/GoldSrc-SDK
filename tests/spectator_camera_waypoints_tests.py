"""Exercise CHudSpectator::GetDirectorCamera waypoint successor bounds."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'
CLIENTS = ['valve', 'bshift', 'tfc', 'cstrike']


def extract(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cstddef>
#include <vector>

enum { SOLID_NOT = 0, MAX_CAM_WAYPOINTS = 32 };

struct vec3_t
{
    float v[3];
    float &operator[](int i) { return v[i]; }
    const float &operator[](int i) const { return v[i]; }
    vec3_t operator-(const vec3_t &o) const { return { v[0] - o.v[0], v[1] - o.v[1], v[2] - o.v[2] }; }
};

struct entity_state_t
{
    vec3_t origin;
    int solid;
    int usehull;
};

struct cl_entity_t
{
    entity_state_t curstate;
};

struct cameraWayPoint_t
{
    float time;
    vec3_t position;
    vec3_t angle;
    float fov;
    int flags;
};

struct CInterpolation
{
    void Interpolate(float, vec3_t &pos, vec3_t &ang, float *fov)
    {
        pos = { 0.0f, 0.0f, 0.0f };
        ang = { 0.0f, 0.0f, 0.0f };
        if (fov) *fov = 90.0f;
    }
};

static struct { float m_flTime; } gHUD = { 0.0f };

static struct
{
    cl_entity_t *(*GetEntityByIndex)(int);
    int (*GetMaxClients)();
    void (*Con_Printf)(const char *, ...);
} gEngfuncs = {
    [](int) -> cl_entity_t * { return NULL; },
    []() { return 32; },
    [](const char *, ...) {},
};

static void VectorAngles(const vec3_t &, vec3_t &) {}

struct InterpCall
{
    const cameraWayPoint_t *prev;
    const cameraWayPoint_t *start;
    const cameraWayPoint_t *end;
    const cameraWayPoint_t *next;
};

class CHudSpectator
{
public:
    int m_ChaseEntity = 0;
    bool m_IsInterpolating = false;
    int m_WayPoint = 0;
    int m_NumWayPoints = 0;
    cameraWayPoint_t m_CamPath[MAX_CAM_WAYPOINTS] = {};
    CInterpolation m_WayInterpolation;
    std::vector<InterpCall> calls;

    void SetWayInterpolation(cameraWayPoint_t *prev, cameraWayPoint_t *start,
                             cameraWayPoint_t *end, cameraWayPoint_t *next)
    {
        // Dereference next when provided, mirroring production SetWayInterpolation.
        if (next)
        {
            volatile float x = next->position[0];
            (void)x;
        }
        calls.push_back({ prev, start, end, next });
    }

    void SetCameraView(vec3_t &, vec3_t &, float) {}
    bool GetDirectorCamera(vec3_t &position, vec3_t &angle);
};
'''

TESTS = r'''
static void PopulatePath(CHudSpectator &spec, int count)
{
    spec.m_ChaseEntity = 0;
    spec.m_IsInterpolating = true;
    spec.m_WayPoint = 0;
    spec.m_NumWayPoints = count;
    spec.calls.clear();
    for (int i = 0; i < MAX_CAM_WAYPOINTS; ++i)
    {
        spec.m_CamPath[i].time = (float)(i + 1);
        spec.m_CamPath[i].position = { (float)i, 0.0f, 0.0f };
        spec.m_CamPath[i].angle = { 0.0f, 0.0f, 0.0f };
        spec.m_CamPath[i].fov = 90.0f;
        spec.m_CamPath[i].flags = 0;
    }
}

int main()
{
    vec3_t pos = { 0.0f, 0.0f, 0.0f };
    vec3_t ang = { 0.0f, 0.0f, 0.0f };

    // Full MAX_CAM_WAYPOINTS (32) path: advancing onto the last segment (30 -> 31)
    // must pass NULL as successor instead of reading m_CamPath[32] out of bounds.
    {
        CHudSpectator spec;
        PopulatePath(spec, MAX_CAM_WAYPOINTS);
        spec.m_WayPoint = MAX_CAM_WAYPOINTS - 3; // 29 -> 30
        gHUD.m_flTime = spec.m_CamPath[MAX_CAM_WAYPOINTS - 2].time + 0.5f;
        assert(spec.GetDirectorCamera(pos, ang));
        assert(MAX_CAM_WAYPOINTS - 2 == spec.m_WayPoint);
        assert(1u == spec.calls.size());
        assert(&spec.m_CamPath[MAX_CAM_WAYPOINTS - 3] == spec.calls[0].prev);
        assert(&spec.m_CamPath[MAX_CAM_WAYPOINTS - 2] == spec.calls[0].start);
        assert(&spec.m_CamPath[MAX_CAM_WAYPOINTS - 1] == spec.calls[0].end);
        assert(NULL == spec.calls[0].next);
    }

    // Shorter path (4 waypoints): middle segment (1 -> 2) gets successor [3];
    // final segment (2 -> 3) gets NULL instead of leftover m_CamPath[4].
    {
        CHudSpectator spec;
        PopulatePath(spec, 4);
        gHUD.m_flTime = spec.m_CamPath[1].time + 0.5f; // advance 0->1 to 1->2
        assert(spec.GetDirectorCamera(pos, ang));
        assert(1 == spec.m_WayPoint);
        assert(1u == spec.calls.size());
        assert(&spec.m_CamPath[0] == spec.calls[0].prev);
        assert(&spec.m_CamPath[1] == spec.calls[0].start);
        assert(&spec.m_CamPath[2] == spec.calls[0].end);
        assert(&spec.m_CamPath[3] == spec.calls[0].next);

        spec.calls.clear();
        gHUD.m_flTime = spec.m_CamPath[2].time + 0.5f; // advance 1->2 to 2->3
        assert(spec.GetDirectorCamera(pos, ang));
        assert(2 == spec.m_WayPoint);
        assert(1u == spec.calls.size());
        assert(&spec.m_CamPath[1] == spec.calls[0].prev);
        assert(&spec.m_CamPath[2] == spec.calls[0].start);
        assert(&spec.m_CamPath[3] == spec.calls[0].end);
        assert(NULL == spec.calls[0].next);
    }

    // Past the last waypoint: interpolation stops cleanly.
    {
        CHudSpectator spec;
        PopulatePath(spec, 3);
        spec.m_WayPoint = 1;
        gHUD.m_flTime = spec.m_CamPath[2].time + 0.5f;
        assert(!spec.GetDirectorCamera(pos, ang));
        assert(!spec.m_IsInterpolating);
        assert(spec.calls.empty());
    }
}
'''


def main():
    for client in CLIENTS:
        source = (ROOT / client / 'cl_dll/hud_spectator.cpp').read_text(encoding='utf-8')
        production = extract(source, 'bool CHudSpectator::GetDirectorCamera(vec3_t &position, vec3_t &angle)')
        with tempfile.TemporaryDirectory(prefix='goldsrc-spectator-waypoints-') as folder:
            work = Path(folder)
            test_source = work / 'test.cpp'
            test_source.write_text('\n'.join([STUBS, production, TESTS]), encoding='utf-8')
            executable = compile_program([test_source], work / 'test', sanitizers=['address', 'undefined'])
            subprocess.run([str(executable)], cwd=work, check=True)
        print(f'PASS: {client} CHudSpectator::GetDirectorCamera bounds successor waypoints')


if __name__ == '__main__':
    main()
