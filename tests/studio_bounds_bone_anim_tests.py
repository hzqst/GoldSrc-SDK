"""Exercise R_StudioComputeBounds per-bone animation channel indexing in both SW and HW renderers."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK/engine'


def extract(filename, signature, occurrence=0, end='\n}'):
    source = (ROOT / filename).read_text(encoding='utf-8')
    start = -1
    for _ in range(occurrence + 1):
        start = source.index(signature, start + 1)
    return source[start:source.index(end, start) + len(end)]


STUBS = r'''
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

using byte = unsigned char;
using vec_t = float;
using vec3_t = vec_t[3];
using int32 = std::int32_t;

static void VectorClear(vec3_t v) { v[0] = v[1] = v[2] = 0.0f; }
static void VectorCopy(const vec3_t src, vec3_t dst) { dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; }
static void VectorSubtract(const vec3_t a, const vec3_t b, vec3_t out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}
static void VectorScale(const vec3_t in, vec_t s, vec3_t out)
{
    out[0] = in[0] * s;
    out[1] = in[1] * s;
    out[2] = in[2] * s;
}

struct studiohdr_t {
    int id;
    int version;
    char name[64];
    int length;
    vec3_t eyeposition;
    vec3_t min;
    vec3_t max;
    vec3_t bbmin;
    vec3_t bbmax;
    int flags;
    int numbones;
    int boneindex;
    int numbonecontrollers;
    int bonecontrollerindex;
    int numhitboxes;
    int hitboxindex;
    int numseq;
    int seqindex;
    int numseqgroups;
    int seqgroupindex;
    int numtextures;
    int textureindex;
    int texturedataindex;
    int numskinref;
    int numskinfamilies;
    int skinindex;
    int numbodyparts;
    int bodypartindex;
    int numattachments;
    int attachmentindex;
    int soundtable;
    int soundindex;
    int soundgroups;
    int soundgroupindex;
    int numtransitions;
    int transitionindex;
};

struct mstudiobone_t {
    char name[32];
    int parent;
    int flags;
    int bonecontroller[6];
    float value[6];
    float scale[6];
};

struct mstudioseqdesc_t {
    char label[32];
    float fps;
    int flags;
    int activity;
    int actweight;
    int numevents;
    int eventindex;
    int numframes;
    int numpivots;
    int pivotindex;
    int motiontype;
    int motionbone;
    vec3_t linearmovement;
    int automoveposindex;
    int automoveangleindex;
    vec3_t bbmin;
    vec3_t bbmax;
    int numblends;
    int animindex;
    int blendtype[2];
    float blendstart[2];
    float blendend[2];
    int blendparent;
    int seqgroup;
    int entrynode;
    int exitnode;
    int nodeflags;
    int nextseq;
};

struct mstudioanim_t {
    unsigned short offset[6];
};

union mstudioanimvalue_t {
    struct {
        byte valid;
        byte total;
    } num;
    short value;
};

struct mstudiobodyparts_t {
    char name[64];
    int nummodels;
    int base;
    int modelindex;
};

struct mstudiomodel_t {
    char name[64];
    int type;
    float boundingradius;
    int nummesh;
    int meshindex;
    int numverts;
    int vertinfoindex;
    int vertindex;
    int numnorms;
    int norminfoindex;
    int normindex;
    int numgroups;
    int groupindex;
};
'''

TESTS = r'''
struct TestModel {
    std::vector<byte> buf;
};

static TestModel BuildTwoBoneModel(
    bool animateBone0,
    short bone0Frame1Delta,
    bool animateBone1,
    short bone1Frame1Delta,
    float bone0ScaleX,
    float bone1ScaleX)
{
    TestModel m;
    const size_t bodypartOff = sizeof(studiohdr_t);
    const size_t modelOff = bodypartOff + sizeof(mstudiobodyparts_t);
    const size_t vertOff = modelOff + sizeof(mstudiomodel_t);
    const size_t boneOff = vertOff + sizeof(vec3_t) * 2;
    const size_t seqOff = boneOff + sizeof(mstudiobone_t) * 2;
    const size_t animOff = seqOff + sizeof(mstudioseqdesc_t);
    const size_t animVal0Off = animOff + sizeof(mstudioanim_t) * 2;
    const size_t animVal1Off = animVal0Off + sizeof(mstudioanimvalue_t) * 4;
    const size_t totalSize = animVal1Off + sizeof(mstudioanimvalue_t) * 4;

    m.buf.assign(totalSize, 0);

    auto* hdr = reinterpret_cast<studiohdr_t*>(m.buf.data());
    hdr->length = static_cast<int>(totalSize);
    hdr->numbodyparts = 1;
    hdr->bodypartindex = static_cast<int>(bodypartOff);
    hdr->numbones = 2;
    hdr->boneindex = static_cast<int>(boneOff);
    hdr->numseq = 1;
    hdr->seqindex = static_cast<int>(seqOff);

    auto* bp = reinterpret_cast<mstudiobodyparts_t*>(m.buf.data() + bodypartOff);
    bp->nummodels = 1;
    bp->modelindex = static_cast<int>(modelOff);

    auto* mdl = reinterpret_cast<mstudiomodel_t*>(m.buf.data() + modelOff);
    mdl->numverts = 2;
    mdl->vertindex = static_cast<int>(vertOff);

    auto* verts = reinterpret_cast<vec3_t*>(m.buf.data() + vertOff);
    verts[0][0] = -1.0f; verts[0][1] = -1.0f; verts[0][2] = -1.0f;
    verts[1][0] =  1.0f; verts[1][1] =  1.0f; verts[1][2] =  1.0f;

    auto* bones = reinterpret_cast<mstudiobone_t*>(m.buf.data() + boneOff);
    for (int b = 0; b < 2; ++b)
    {
        for (int c = 0; c < 6; ++c)
            bones[b].bonecontroller[c] = -1;
        bones[b].scale[0] = (b == 0) ? bone0ScaleX : bone1ScaleX;
        bones[b].scale[1] = 1.0f;
        bones[b].scale[2] = 1.0f;
    }

    auto* seq = reinterpret_cast<mstudioseqdesc_t*>(m.buf.data() + seqOff);
    seq->numframes = 2;
    seq->numblends = 1;
    seq->animindex = static_cast<int>(animOff);
    seq->seqgroup = 0;

    auto* anims = reinterpret_cast<mstudioanim_t*>(m.buf.data() + animOff);
    if (animateBone0)
    {
        anims[0].offset[0] = static_cast<unsigned short>(animVal0Off - animOff);
        auto* vals = reinterpret_cast<mstudioanimvalue_t*>(m.buf.data() + animVal0Off);
        vals[0].num.valid = 2;
        vals[0].num.total = 2;
        vals[1].value = 0;
        vals[2].value = bone0Frame1Delta;
    }
    if (animateBone1)
    {
        anims[1].offset[0] = static_cast<unsigned short>(animVal1Off - (animOff + sizeof(mstudioanim_t)));
        auto* vals = reinterpret_cast<mstudioanimvalue_t*>(m.buf.data() + animVal1Off);
        vals[0].num.valid = 2;
        vals[0].num.total = 2;
        vals[1].value = 0;
        vals[2].value = bone1Frame1Delta;
    }

    return m;
}

static void CheckClose(float expected, float actual)
{
    assert(std::fabs(expected - actual) < 1e-4f);
}

template <typename ComputeFn>
static void RunBoundsChecks(ComputeFn computeBounds)
{
    // 1. Bone 0 static, bone 1 moves X from 0 to 200 -> bone delta X = 200 -> bounds [-100, 100].
    //    With the old bug (passing panim instead of &panim[j]), bone 1 used bone 0's empty channel
    //    and bounds remained [-1, 1].
    {
        TestModel m = BuildTwoBoneModel(false, 0, true, 200, 1.0f, 1.0f);
        vec3_t mins, maxs;
        assert(1 == computeBounds(m.buf.data(), mins, maxs));
        CheckClose(-100.0f, mins[0]);
        CheckClose( 100.0f, maxs[0]);
        CheckClose(  -1.0f, mins[1]);
        CheckClose(   1.0f, maxs[1]);
    }

    // 2. Bone 0 moves X from 0 to 100 (scale=1), bone 1 static (scale=10) -> bone delta X = 100 -> bounds [-50, 50].
    //    With the old bug, bone 1 would inherit bone 0's channel * 10 = 1000 -> bounds [-500, 500].
    {
        TestModel m = BuildTwoBoneModel(true, 100, false, 0, 1.0f, 10.0f);
        vec3_t mins, maxs;
        assert(1 == computeBounds(m.buf.data(), mins, maxs));
        CheckClose(-50.0f, mins[0]);
        CheckClose( 50.0f, maxs[0]);
    }
}

int main()
{
    RunBoundsChecks(sw_bounds::R_StudioComputeBounds);
    RunBoundsChecks(hw_bounds::R_StudioComputeBounds);
    return 0;
}
'''


def main():
    calc_pos = extract('r_studio.cpp', 'void R_StudioCalcBonePosition(', 1)
    sw_helpers = '\n'.join([
        extract('r_studio.cpp', 'void R_StudioBoundBone(', 0),
        extract('r_studio.cpp', 'vec_t* R_StudioBoundVertex(', 0),
        extract('r_studio.cpp', 'void R_StudioAccumulateBoneVerts(', 0),
        extract('r_studio.cpp', 'int R_StudioComputeBounds( byte* buffer,', 0),
    ])
    hw_helpers = '\n'.join([
        extract('r_studio.cpp', 'void R_StudioBoundBone(', 1),
        extract('r_studio.cpp', 'vec_t* R_StudioBoundVertex(', 1),
        extract('r_studio.cpp', 'void R_StudioAccumulateBoneVerts(', 1),
        extract('r_studio.cpp', 'int R_StudioComputeBounds( byte* buffer,', 1),
    ])

    wrapped = [
        STUBS,
        calc_pos,
        'namespace sw_bounds {\n' + sw_helpers + '\n}',
        'namespace hw_bounds {\n' + hw_helpers + '\n}',
        TESTS,
    ]

    with tempfile.TemporaryDirectory(prefix='goldsrc-studio-bounds-anim-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join(wrapped), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
    print('PASS: R_StudioComputeBounds indexes per-bone animation channels in SW and HW renderers')


if __name__ == '__main__':
    main()
