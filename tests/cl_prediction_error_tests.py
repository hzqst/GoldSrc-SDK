"""Exercise CL_CheckPredictionError writing the server origin back to the acknowledged command slot."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK/engine'


def extract(filename, signature):
    source = (ROOT / filename).read_text(encoding='utf-8')
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cmath>
#include <cstring>

using vec_t = float;
using vec3_t = vec_t[3];
constexpr int CL_UPDATE_BACKUP = 64;
constexpr int CL_UPDATE_MASK = CL_UPDATE_BACKUP - 1;

static void VectorSubtract(const vec3_t a, const vec3_t b, vec3_t out)
{
    out[0] = a[0] - b[0]; out[1] = a[1] - b[1]; out[2] = a[2] - b[2];
}
static void VectorCopy(const vec3_t in, vec3_t out)
{
    out[0] = in[0]; out[1] = in[1]; out[2] = in[2];
}
static void VectorClear(vec3_t out) { out[0] = out[1] = out[2] = 0; }
static float Length(const vec3_t v) { return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }
static void Con_NPrintf(int, const char*, ...) {}

struct entity_state_t { vec3_t origin; };
struct frame_t { entity_state_t playerstate[2]; };

static struct {
    int parsecountmod = 0;
    int playernum = 0;
    int maxclients = 16;
    frame_t frames[CL_UPDATE_BACKUP] = {};
    vec3_t predicted_origins[CL_UPDATE_BACKUP] = {};
    vec3_t prediction_error = {};
} cl;

static struct {
    struct { int incoming_acknowledged = 0; } netchan;
} cls;

static struct { float value; } cl_showerror{0.0f}, cl_smoothtime{0.1f};
static float cl_correction_time = 0.0f;
'''

TESTS = r'''
int main()
{
    // Frame slot (parsecountmod = 12) differs from command slot (incoming_acknowledged & mask = 5).
    cl.parsecountmod = 12;
    cls.netchan.incoming_acknowledged = 5;
    cl.playernum = 0;

    // Seed predicted origins for both slots.
    cl.predicted_origins[5][0] = 100.0f;
    cl.predicted_origins[12][0] = 500.0f;

    // Server reports origin 105.0f for command 5 in packet slot 12.
    cl.frames[12].playerstate[0].origin[0] = 105.0f;

    cl_correction_time = 0.0f;
    CL_CheckPredictionError();

    // Command slot 5 must be updated to 105.0f; packet slot 12 must NOT be clobbered.
    assert(105.0f == cl.predicted_origins[5][0]);
    assert(500.0f == cl.predicted_origins[12][0]);
    assert(5.0f == cl.prediction_error[0]);
    assert(0.1f == cl_correction_time);

    // Next packet (slot 13) acknowledging the same command 5 with the same origin: error must be 0.
    cl.parsecountmod = 13;
    cl.frames[13].playerstate[0].origin[0] = 105.0f;
    cl_correction_time = 0.0f;
    CL_CheckPredictionError();
    assert(0.0f == cl.prediction_error[0]);
    assert(0.0f == cl_correction_time);
}
'''


def main():
    production = extract('cl_pred.cpp', 'void CL_CheckPredictionError()')
    with tempfile.TemporaryDirectory(prefix='goldsrc-cl-prediction-error-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
    print('PASS: CL_CheckPredictionError writes back to the acknowledged command slot')


if __name__ == '__main__':
    main()
