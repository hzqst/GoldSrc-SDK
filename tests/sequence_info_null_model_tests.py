"""Regression tests for GetSequenceInfo zeroing outputs when model is NULL (#497)."""
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


HEADER = r'''
#include <cassert>
#include <cmath>
#include <cstring>

typedef unsigned char byte;

struct Vector {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};
using vec3_t = float[3];

static float Length(const Vector& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

static float Length(const float v[3]) {
    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

struct client_anim_state_t {
    int sequence = 0;
};

struct mstudioseqdesc_t {
    float fps = 0.0f;
    int flags = 0;
    int numframes = 0;
    float linearmovement[3] = {0.0f, 0.0f, 0.0f};
};

struct studiohdr_t {
    int numseq = 0;
    int seqindex = 0;
};

struct FakeModel {
    studiohdr_t hdr;
    mstudioseqdesc_t seqs[2];
};

static FakeModel MakeModel() {
    FakeModel m{};
    m.hdr.numseq = 2;
    m.hdr.seqindex = static_cast<int>(offsetof(FakeModel, seqs));
    m.seqs[0].fps = 30.0f;
    m.seqs[0].flags = 1;
    m.seqs[0].numframes = 11;
    m.seqs[0].linearmovement[0] = 30.0f;
    m.seqs[0].linearmovement[1] = 40.0f;
    m.seqs[0].linearmovement[2] = 0.0f;

    m.seqs[1].fps = 24.0f;
    m.seqs[1].flags = 0;
    m.seqs[1].numframes = 1;
    return m;
}
'''

TESTS = r'''
template <typename Fn>
static void CheckGetSequenceInfo(Fn fn) {
    FakeModel model = MakeModel();
    client_anim_state_t state;
    state.sequence = 0;

    // 1. NULL model must zero both output parameters instead of leaving garbage.
    {
        float fr = 1234.5f;
        float gs = 6789.0f;
        fn(nullptr, &state, &fr, &gs);
        assert(0.0f == fr);
        assert(0.0f == gs);
    }

    // 2. Negative and out-of-range sequence indices must zero both outputs.
    {
        float fr = 99.0f;
        float gs = 88.0f;
        state.sequence = -1;
        fn(&model, &state, &fr, &gs);
        assert(0.0f == fr);
        assert(0.0f == gs);

        fr = 77.0f;
        gs = 66.0f;
        state.sequence = 2;
        fn(&model, &state, &fr, &gs);
        assert(0.0f == fr);
        assert(0.0f == gs);
    }

    // 3. Valid multi-frame sequence computes expected frameRate and groundSpeed.
    {
        float fr = 0.0f;
        float gs = 0.0f;
        state.sequence = 0;
        fn(&model, &state, &fr, &gs);
        assert(std::fabs(fr - 768.0f) < 0.001f);
        assert(std::fabs(gs - 150.0f) < 0.001f);
    }

    // 4. Single-frame sequence sets frameRate = 256 and groundSpeed = 0.
    {
        float fr = 0.0f;
        float gs = 42.0f;
        state.sequence = 1;
        fn(&model, &state, &fr, &gs);
        assert(std::fabs(fr - 256.0f) < 0.001f);
        assert(0.0f == gs);
    }
}

int main() {
    CheckGetSequenceInfo(cstrike_ns::GetSequenceInfo);
    CheckGetSequenceInfo(dod_ns::GetSequenceInfo);
    CheckGetSequenceInfo(ricochet_ns::GetSequenceInfo);

    // Ricochet GetSequenceFlags must also reject NULL model and negative sequence.
    {
        FakeModel model = MakeModel();
        client_anim_state_t state;
        state.sequence = -1;
        assert(0 == ricochet_ns::GetSequenceFlags(nullptr, &state));
        assert(0 == ricochet_ns::GetSequenceFlags(&model, &state));
        state.sequence = 0;
        assert(1 == ricochet_ns::GetSequenceFlags(&model, &state));
    }
    return 0;
}
'''


def main():
    cstrike_fn = extract(
        'cstrike/cl_dll/GameStudioModelRenderer.cpp',
        'void GetSequenceInfo( void* model, client_anim_state_t* state, float* frameRate, float* groundSpeed )',
    )
    dod_fn = extract(
        'dod/cl_dll/GameStudioModelRenderer.cpp',
        'void GetSequenceInfo( void* pmodel, client_anim_state_t* state, float* frameRate, float* groundSpeed )',
    )
    ricochet_fn = extract(
        'ricochet/cl_dll/GameStudioModelRenderer.cpp',
        'void GetSequenceInfo( void *pmodel, client_anim_state_t *pev, float *pflFrameRate, float *pflGroundSpeed )',
    )
    ricochet_flags_fn = extract(
        'ricochet/cl_dll/GameStudioModelRenderer.cpp',
        'int GetSequenceFlags( void *pmodel, client_anim_state_t *pev )',
    )

    source_text = '\n'.join([
        HEADER,
        'namespace cstrike_ns {', cstrike_fn, '}',
        'namespace dod_ns {', dod_fn, '}',
        'namespace ricochet_ns {', ricochet_fn, ricochet_flags_fn, '}',
        TESTS,
    ])

    with tempfile.TemporaryDirectory(prefix='goldsrc-seq-info-null-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text(source_text, encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
    print('PASS: GetSequenceInfo zeroes outputs for NULL model and invalid sequence')


if __name__ == '__main__':
    main()
