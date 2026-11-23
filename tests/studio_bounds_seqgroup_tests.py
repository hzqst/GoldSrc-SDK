"""Exercise R_StudioComputeBounds seqgroup > 0 loading and bounds validation in SW and HW renderers."""
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
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using byte = unsigned char;
using qboolean = int;
using vec_t = float;
using vec3_t = vec_t[3];
using int32 = std::int32_t;
constexpr qboolean FALSE = 0;
constexpr qboolean TRUE = 1;
constexpr int STUDIO_VERSION = 10;
constexpr int MAXSTUDIOBONES = 128;
constexpr int MAXSTUDIOBODYPARTS = 32;
constexpr int MAXSTUDIOSEQUENCES = 2048;
constexpr int MAXSTUDIOGROUPS = 16;

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

struct studioseqhdr_t {
    int id;
    int version;
    char name[64];
    int length;
};

struct mstudiobone_t {
    char name[32];
    int parent;
    int flags;
    int bonecontroller[6];
    float value[6];
    float scale[6];
};

struct mstudioseqgroup_t {
    char label[32];
    char name[64];
    int32 unused1;
    int unused2;
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

static int g_liveFileAllocs = 0;
static int g_loadCalls = 0;
static std::string g_lastLoadedName;
static std::map<std::string, std::vector<byte>> g_virtualFiles;

static byte* COM_LoadFileForMe(char* filename, int* pLength)
{
    ++g_loadCalls;
    g_lastLoadedName = filename ? filename : "";
    auto it = g_virtualFiles.find(g_lastLoadedName);
    if (it == g_virtualFiles.end())
    {
        if (pLength) *pLength = 0;
        return nullptr;
    }
    const auto& data = it->second;
    byte* copy = static_cast<byte*>(std::malloc(data.size() ? data.size() : 1));
    if (!data.empty())
        std::memcpy(copy, data.data(), data.size());
    if (pLength) *pLength = static_cast<int>(data.size());
    ++g_liveFileAllocs;
    return copy;
}
static void COM_FreeFile(byte* p)
{
    if (p)
    {
        --g_liveFileAllocs;
        std::free(p);
    }
}
'''

TESTS = r'''
static void CheckClose(float expected, float actual)
{
    assert(std::fabs(expected - actual) < 1e-4f);
}

// Build a main model whose single sequence references seqgroup = 1 with a large animindex
// that lies far past the end of the main model buffer (catching any read from buffer instead of groupBuffer).
static std::vector<byte> BuildMainModel(int seqgroup, int animindex, int numframes, bool nonContiguousModel = false)
{
    const size_t bodypartOff = sizeof(studiohdr_t);
    const size_t padOff = bodypartOff + sizeof(mstudiobodyparts_t);
    const size_t modelOff = nonContiguousModel ? (padOff + 128) : padOff;
    const size_t vertOff = modelOff + sizeof(mstudiomodel_t);
    const size_t boneOff = vertOff + sizeof(vec3_t) * 2;
    const size_t seqGroupOff = boneOff + sizeof(mstudiobone_t);
    const size_t seqOff = seqGroupOff + sizeof(mstudioseqgroup_t) * 2;
    const size_t totalSize = seqOff + sizeof(mstudioseqdesc_t);

    std::vector<byte> buf(totalSize, 0);

    auto* hdr = reinterpret_cast<studiohdr_t*>(buf.data());
    std::memcpy(&hdr->id, "IDST", 4);
    hdr->version = STUDIO_VERSION;
    hdr->length = static_cast<int>(totalSize);
    hdr->numbodyparts = 1;
    hdr->bodypartindex = static_cast<int>(bodypartOff);
    hdr->numbones = 1;
    hdr->boneindex = static_cast<int>(boneOff);
    hdr->numseqgroups = 2;
    hdr->seqgroupindex = static_cast<int>(seqGroupOff);
    hdr->numseq = 1;
    hdr->seqindex = static_cast<int>(seqOff);

    auto* bp = reinterpret_cast<mstudiobodyparts_t*>(buf.data() + bodypartOff);
    bp->nummodels = 1;
    bp->base = 1;
    bp->modelindex = static_cast<int>(modelOff);

    auto* mdl = reinterpret_cast<mstudiomodel_t*>(buf.data() + modelOff);
    mdl->numverts = 2;
    mdl->vertindex = static_cast<int>(vertOff);

    auto* verts = reinterpret_cast<vec3_t*>(buf.data() + vertOff);
    verts[0][0] = -2.0f; verts[0][1] = -2.0f; verts[0][2] = -2.0f;
    verts[1][0] =  2.0f; verts[1][1] =  2.0f; verts[1][2] =  2.0f;

    auto* bone = reinterpret_cast<mstudiobone_t*>(buf.data() + boneOff);
    for (int c = 0; c < 6; ++c)
        bone->bonecontroller[c] = -1;
    bone->scale[0] = bone->scale[1] = bone->scale[2] = 1.0f;

    auto* groups = reinterpret_cast<mstudioseqgroup_t*>(buf.data() + seqGroupOff);
    std::strcpy(groups[0].label, "default");
    std::strcpy(groups[1].label, "group01");
    std::strcpy(groups[1].name, "models/test01.mdl");

    auto* seq = reinterpret_cast<mstudioseqdesc_t*>(buf.data() + seqOff);
    seq->numframes = numframes;
    seq->numblends = 1;
    seq->seqgroup = seqgroup;
    seq->animindex = animindex;

    return buf;
}

static std::vector<byte> BuildSeqGroupFile(int animindex, byte valid, byte total, short val0, short val1, bool badMagic = false)
{
    const size_t animValOff = static_cast<size_t>(animindex) + sizeof(mstudioanim_t);
    const size_t spanElems = static_cast<size_t>(valid) + 1;
    const size_t totalSize = animValOff + spanElems * sizeof(mstudioanimvalue_t);

    std::vector<byte> buf(totalSize, 0);
    auto* shdr = reinterpret_cast<studioseqhdr_t*>(buf.data());
    std::memcpy(&shdr->id, badMagic ? "NOPE" : "IDSQ", 4);
    shdr->version = STUDIO_VERSION;
    shdr->length = static_cast<int>(totalSize);

    auto* anim = reinterpret_cast<mstudioanim_t*>(buf.data() + animindex);
    anim->offset[0] = static_cast<unsigned short>(sizeof(mstudioanim_t));

    auto* vals = reinterpret_cast<mstudioanimvalue_t*>(buf.data() + animValOff);
    vals[0].num.valid = valid;
    vals[0].num.total = total;
    if (valid >= 1) vals[1].value = val0;
    if (valid >= 2) vals[2].value = val1;

    return buf;
}

template <typename ComputeFn>
static void RunSuite(ComputeFn computeBounds)
{
    // 1. Valid seqgroup = 1 with animindex > main buffer size reads from models/test01.mdl.
    {
        g_virtualFiles.clear();
        g_loadCalls = 0;
        g_lastLoadedName.clear();

        const int groupAnimIndex = 4096;
        auto mainBuf = BuildMainModel(1, groupAnimIndex, 2, true);
        assert(static_cast<size_t>(groupAnimIndex) > mainBuf.size());
        g_virtualFiles["models/test01.mdl"] = BuildSeqGroupFile(groupAnimIndex, 2, 2, 0, 120);

        vec3_t mins, maxs;
        assert(1 == computeBounds(mainBuf.data(), static_cast<int>(mainBuf.size()), mins, maxs));
        assert(1 == g_loadCalls);
        assert(std::string("models/test01.mdl") == g_lastLoadedName);
        CheckClose(-60.0f, mins[0]);
        CheckClose( 60.0f, maxs[0]);
        CheckClose( -2.0f, mins[1]);
        CheckClose(  2.0f, maxs[1]);
        assert(0 == g_liveFileAllocs);
    }

    // 2a. Repeating span ending at exact EOF of sequence group file (valid=1, total=2, numframes=2):
    //     R_StudioCalcBonePosition with s == 0.0f must not read panimvalue[valid + 2] past EOF.
    {
        g_virtualFiles.clear();
        const int groupAnimIndex = sizeof(studioseqhdr_t);
        auto mainBuf = BuildMainModel(1, groupAnimIndex, 2);
        g_virtualFiles["models/test01.mdl"] = BuildSeqGroupFile(groupAnimIndex, 1, 2, 80, 0);

        vec3_t mins, maxs;
        assert(1 == computeBounds(mainBuf.data(), static_cast<int>(mainBuf.size()), mins, maxs));
        CheckClose(-2.0f, mins[0]);
        CheckClose( 2.0f, maxs[0]);
        assert(0 == g_liveFileAllocs);
    }

    // 2b. Repeating span ending at exact EOF of the main MDL buffer (seqgroup == 0):
    //     Must succeed identically to external sequence groups without OOB read under ASan.
    {
        g_virtualFiles.clear();
        auto base = BuildMainModel(0, 0, 3);
        const size_t animOff = base.size();
        // Two spans: [frame 0 -> val 0] then [frames 1..2 repeating val 100], ending at exact EOF.
        const size_t animBytes = sizeof(mstudioanim_t) + 4 * sizeof(mstudioanimvalue_t);
        base.resize(animOff + animBytes, 0);

        auto* hdr = reinterpret_cast<studiohdr_t*>(base.data());
        hdr->length = static_cast<int>(base.size());
        auto* seq = reinterpret_cast<mstudioseqdesc_t*>(base.data() + hdr->seqindex);
        seq->animindex = static_cast<int>(animOff);

        auto* anim = reinterpret_cast<mstudioanim_t*>(base.data() + animOff);
        anim->offset[0] = static_cast<unsigned short>(sizeof(mstudioanim_t));
        auto* vals = reinterpret_cast<mstudioanimvalue_t*>(base.data() + animOff + sizeof(mstudioanim_t));
        vals[0].num.valid = 1;
        vals[0].num.total = 1;
        vals[1].value = 0;
        vals[2].num.valid = 1;
        vals[2].num.total = 2;
        vals[3].value = 100;

        vec3_t mins, maxs;
        assert(1 == computeBounds(base.data(), static_cast<int>(base.size()), mins, maxs));
        CheckClose(-50.0f, mins[0]);
        CheckClose( 50.0f, maxs[0]);
        assert(0 == g_liveFileAllocs);
    }

    // 3. Missing sequence group file returns 0 cleanly without leaks.
    {
        g_virtualFiles.clear();
        auto mainBuf = BuildMainModel(1, sizeof(studioseqhdr_t), 2);
        vec3_t mins, maxs;
        assert(0 == computeBounds(mainBuf.data(), static_cast<int>(mainBuf.size()), mins, maxs));
        assert(0 == g_liveFileAllocs);
    }

    // 4. Invalid seqgroup index (>= numseqgroups or < 0) returns 0.
    {
        g_virtualFiles.clear();
        vec3_t mins, maxs;
        auto mainTooHigh = BuildMainModel(2, sizeof(studioseqhdr_t), 2);
        assert(0 == computeBounds(mainTooHigh.data(), static_cast<int>(mainTooHigh.size()), mins, maxs));
        auto mainNeg = BuildMainModel(-1, sizeof(studioseqhdr_t), 2);
        assert(0 == computeBounds(mainNeg.data(), static_cast<int>(mainNeg.size()), mins, maxs));
        assert(0 == g_liveFileAllocs);
    }

    // 5. Bad IDSQ signature or truncated sequence group file returns 0 without leaks.
    {
        g_virtualFiles.clear();
        const int groupAnimIndex = sizeof(studioseqhdr_t);
        auto mainBuf = BuildMainModel(1, groupAnimIndex, 2);
        vec3_t mins, maxs;

        g_virtualFiles["models/test01.mdl"] = BuildSeqGroupFile(groupAnimIndex, 2, 2, 0, 50, true);
        assert(0 == computeBounds(mainBuf.data(), static_cast<int>(mainBuf.size()), mins, maxs));

        // Truncate valid group file so animation samples extend past EOF.
        auto truncGroup = BuildSeqGroupFile(groupAnimIndex, 2, 2, 0, 50, false);
        truncGroup.pop_back();
        g_virtualFiles["models/test01.mdl"] = truncGroup;
        assert(0 == computeBounds(mainBuf.data(), static_cast<int>(mainBuf.size()), mins, maxs));
        assert(0 == g_liveFileAllocs);
    }

    // 6. Malformed RLE span (total == 0) in sequence group is rejected cleanly instead of looping forever.
    {
        g_virtualFiles.clear();
        const int groupAnimIndex = sizeof(studioseqhdr_t);
        auto mainBuf = BuildMainModel(1, groupAnimIndex, 2);
        auto badSpan = BuildSeqGroupFile(groupAnimIndex, 1, 2, 0, 0);
        auto* vals = reinterpret_cast<mstudioanimvalue_t*>(badSpan.data() + groupAnimIndex + sizeof(mstudioanim_t));
        vals[0].num.valid = 0;
        vals[0].num.total = 0;
        g_virtualFiles["models/test01.mdl"] = badSpan;

        vec3_t mins, maxs;
        assert(0 == computeBounds(mainBuf.data(), static_cast<int>(mainBuf.size()), mins, maxs));
        assert(0 == g_liveFileAllocs);
    }

    // 7. Malformed main model (seqgroup == 0) with out-of-bounds animindex or vertindex returns 0.
    {
        g_virtualFiles.clear();
        vec3_t mins, maxs;

        auto badAnim = BuildMainModel(0, 99999, 2);
        assert(0 == computeBounds(badAnim.data(), static_cast<int>(badAnim.size()), mins, maxs));

        auto badVerts = BuildMainModel(0, 0, 0);
        auto* hdr = reinterpret_cast<studiohdr_t*>(badVerts.data());
        auto* bp = reinterpret_cast<mstudiobodyparts_t*>(badVerts.data() + hdr->bodypartindex);
        auto* mdl = reinterpret_cast<mstudiomodel_t*>(badVerts.data() + bp->modelindex);
        mdl->vertindex = static_cast<int>(badVerts.size());
        assert(0 == computeBounds(badVerts.data(), static_cast<int>(badVerts.size()), mins, maxs));

        assert(0 == computeBounds(badVerts.data(), static_cast<int>(sizeof(studiohdr_t)) - 1, mins, maxs));
        assert(0 == g_liveFileAllocs);
    }
}

int main()
{
    RunSuite(sw_bounds::R_StudioComputeBounds);
    RunSuite(hw_bounds::R_StudioComputeBounds);
    return 0;
}
'''


def main():
    # Verify both SW and HW copies of the bounds helpers stay identical.
    for sig in (
        'static qboolean R_StudioBoundsRangeValid(',
        'static qboolean R_StudioBoundsValidateSeqAnim(',
        'int R_StudioComputeBounds( byte* buffer,',
    ):
        assert extract('r_studio.cpp', sig, 0) == extract('r_studio.cpp', sig, 1), f'SW/HW mismatch for {sig}'

    calc_pos_sig = 'void R_StudioCalcBonePosition( int frame, float s, mstudiobone_t* pbone, mstudioanim_t* panim, float* adj, float* pos )\n{'
    sw_helpers = '\n'.join([
        extract('r_studio.cpp', calc_pos_sig, 0),
        extract('r_studio.cpp', 'void R_StudioBoundBone(', 0),
        extract('r_studio.cpp', 'vec_t* R_StudioBoundVertex(', 0),
        extract('r_studio.cpp', 'void R_StudioAccumulateBoneVerts(', 0),
        extract('r_studio.cpp', 'static qboolean R_StudioBoundsRangeValid(', 0),
        extract('r_studio.cpp', 'static qboolean R_StudioBoundsValidateSeqAnim(', 0),
        extract('r_studio.cpp', 'int R_StudioComputeBounds( byte* buffer,', 0),
    ])
    hw_helpers = '\n'.join([
        extract('r_studio.cpp', calc_pos_sig, 1),
        extract('r_studio.cpp', 'void R_StudioBoundBone(', 1),
        extract('r_studio.cpp', 'vec_t* R_StudioBoundVertex(', 1),
        extract('r_studio.cpp', 'void R_StudioAccumulateBoneVerts(', 1),
        extract('r_studio.cpp', 'static qboolean R_StudioBoundsRangeValid(', 1),
        extract('r_studio.cpp', 'static qboolean R_StudioBoundsValidateSeqAnim(', 1),
        extract('r_studio.cpp', 'int R_StudioComputeBounds( byte* buffer,', 1),
    ])

    wrapped = [
        STUBS,
        'namespace sw_bounds {\n' + sw_helpers + '\n}',
        'namespace hw_bounds {\n' + hw_helpers + '\n}',
        TESTS,
    ]

    with tempfile.TemporaryDirectory(prefix='goldsrc-studio-bounds-seqgroup-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join(wrapped), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
    print('PASS: R_StudioComputeBounds loads seqgroup > 0 files and validates all offsets in SW and HW renderers')


if __name__ == '__main__':
    main()
