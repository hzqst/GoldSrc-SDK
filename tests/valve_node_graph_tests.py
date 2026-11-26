"""Regression tests for Valve v16 node graph validation and pointer rebinding (#756).

Run: python -I tests/valve_node_graph_tests.py
"""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1]
NODES_CPP = ROOT / 'GoldSrc-SDK/valve/dlls/nodes.cpp'
NODES_H = ROOT / 'GoldSrc-SDK/valve/dlls/nodes.h'


def extract_function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    for idx in range(brace, len(source)):
        ch = source[idx]
        if ch == '{':
            depth += 1
        elif ch == '}':
            depth -= 1
            if depth == 0:
                return source[start:idx + 1]
    raise RuntimeError(f'Unclosed function: {signature}')


HARNESS = r'''
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <type_traits>
#include <vector>

typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned char byte;
#define TRUE 1
#define FALSE 0
#define MAX_PATH 260
#define MAX_NODES 1024
#define ENTRY_STATE_EMPTY -1
#define FL_GRAPHED (1 << 16)
#define FBitSet(flBitVector, bit) ((flBitVector) & (bit))
#define bits_CAP_OPEN_DOORS (1 << 5)
#define bits_CAP_AUTO_DOORS (1 << 6)
#define bits_CAP_USE (1 << 7)

#define FCAP_ACROSS_TRANSITION 0x00000010
struct KeyValueData {};
struct Vector { float x, y, z; };
struct entvars_t { int flags; };
struct edict_t { entvars_t v; };
class CBaseEntity { public: virtual int ObjectCaps( void ) { return 0; } };

static entvars_t* VARS(edict_t* e) { return &e->v; }
static bool FNullEnt(edict_t* e) { return e == NULL; }

enum { at_aiconsole, at_error };
static void ALERT(int, const char*, ...) {}

static std::vector<byte> g_fileData;
static int g_freeCount = 0;
static byte* LOAD_FILE_FOR_ME(const char*, int* length)
{
    if (g_fileData.empty())
    {
        if (length) *length = 0;
        return NULL;
    }
    byte* copy = static_cast<byte*>(malloc(g_fileData.size()));
    memcpy(copy, g_fileData.data(), g_fileData.size());
    if (length) *length = static_cast<int>(g_fileData.size());
    return copy;
}
static void FREE_FILE(byte* p)
{
    if (p)
    {
        ++g_freeCount;
        free(p);
    }
}

static edict_t g_doorEdict = {};
static edict_t* FIND_ENTITY_BY_STRING(edict_t*, const char*, const char* value)
{
    if (value && strcmp(value, "*12") == 0)
        return &g_doorEdict;
    return NULL;
}

#include "nodes.h"

__EXTRACTED__

static std::vector<byte> BuildValidGraph(
    CGraph& outGraph,
    std::vector<CNode>& outNodes,
    std::vector<CLink>& outLinks,
    std::vector<DIST_INFO>& outDi,
    std::vector<char>& outRoute,
    std::vector<short>& outHash)
{
    outGraph = {};
    outGraph.m_cNodes = 2;
    outGraph.m_cLinks = 1;
    outGraph.m_nRouteInfo = 2;
    outGraph.m_nHashLinks = 3 * outGraph.m_cLinks / 2 + 3; // 4
    for (int axis = 0; axis < 3; ++axis)
    {
        outGraph.m_RegionMin[axis] = -100.0f;
        outGraph.m_RegionMax[axis] = 100.0f;
        for (int r = 0; r < NUM_RANGES; ++r)
        {
            outGraph.m_RangeStart[axis][r] = 1;
            outGraph.m_RangeEnd[axis][r] = 0;
        }
        outGraph.m_RangeStart[axis][0] = 0;
        outGraph.m_RangeEnd[axis][0] = 1;
    }
    for (auto& c : outGraph.m_Cache)
        c.n = NO_NODE;
    for (int i = 0; i < 16; ++i)
        outGraph.m_HashPrimes[i] = 1;

    outNodes.assign(2, CNode{});
    outNodes[0].m_vecOrigin = {0.0f, 0.0f, 0.0f};
    outNodes[0].m_vecOriginPeek = {0.0f, 0.0f, 32.0f};
    outNodes[0].m_iFirstLink = 0;
    outNodes[0].m_cNumLinks = 1;
    outNodes[1].m_vecOrigin = {64.0f, 0.0f, 0.0f};
    outNodes[1].m_vecOriginPeek = {64.0f, 0.0f, 32.0f};
    outNodes[1].m_iFirstLink = 1;
    outNodes[1].m_cNumLinks = 0;
    for (int n = 0; n < 2; ++n)
        for (int h = 0; h < MAX_NODE_HULLS; ++h)
            for (int c = 0; c < 2; ++c)
                outNodes[n].m_pNextBestNode[h][c] = 0;

    outLinks.assign(1, CLink{});
    outLinks[0].m_iSrcNode = 0;
    outLinks[0].m_iDestNode = 1;
    outLinks[0].m_pLinkEnt = reinterpret_cast<entvars_t*>(static_cast<uintptr_t>(0x1234));
    memcpy(outLinks[0].m_szLinkEntModelname, "*12\0", 4);
    outLinks[0].m_flWeight = 64.0f;

    outDi.assign(2, DIST_INFO{});
    for (int axis = 0; axis < 3; ++axis)
    {
        outDi[0].m_SortedBy[axis] = 0;
        outDi[1].m_SortedBy[axis] = 1;
    }

    // Sequence of 2 nodes (-2): covers 2 nodes in 1 byte, padded to 2 bytes.
    outRoute = { static_cast<char>(-2), 0 };
    outHash = { 0, ENTRY_STATE_EMPTY, ENTRY_STATE_EMPTY, ENTRY_STATE_EMPTY };

    const int version = GRAPH_VERSION;
    std::vector<byte> buf;
    auto append = [&](const void* data, size_t n) {
        const byte* p = static_cast<const byte*>(data);
        buf.insert(buf.end(), p, p + n);
    };
    append(&version, sizeof(version));
    append(&outGraph, sizeof(outGraph));
    append(outNodes.data(), outNodes.size() * sizeof(CNode));
    append(outLinks.data(), outLinks.size() * sizeof(CLink));
    append(outDi.data(), outDi.size() * sizeof(DIST_INFO));
    append(outRoute.data(), outRoute.size());
    append(outHash.data(), outHash.size() * sizeof(short));
    return buf;
}

static std::vector<byte> Serialize(
    int version,
    const CGraph& g,
    const std::vector<CNode>& nodes,
    const std::vector<CLink>& links,
    const std::vector<DIST_INFO>& di,
    const std::vector<char>& route,
    const std::vector<short>& hash)
{
    std::vector<byte> buf;
    auto append = [&](const void* data, size_t n) {
        const byte* p = static_cast<const byte*>(data);
        buf.insert(buf.end(), p, p + n);
    };
    append(&version, sizeof(version));
    append(&g, sizeof(g));
    append(nodes.data(), nodes.size() * sizeof(CNode));
    append(links.data(), links.size() * sizeof(CLink));
    append(di.data(), di.size() * sizeof(DIST_INFO));
    append(route.data(), route.size());
    append(hash.data(), hash.size() * sizeof(short));
    return buf;
}

int main()
{
    CGraph tmpl;
    std::vector<CNode> nodes;
    std::vector<CLink> links;
    std::vector<DIST_INFO> di;
    std::vector<char> route;
    std::vector<short> hash;
    char mapName[] = "c1a0";

    // 1. Valid load + FSetGraphPointers rebinding + negative search cursors reset
    g_fileData = BuildValidGraph(tmpl, nodes, links, di, route, hash);
    tmpl.m_iLastActiveIdleSearch = -50;
    tmpl.m_iLastCoverSearch = -99;
    g_fileData = Serialize(GRAPH_VERSION, tmpl, nodes, links, di, route, hash);

    CGraph loaded = {};
    assert(loaded.FLoadGraph(NULL) == FALSE);
    assert(loaded.FLoadGraph(mapName) == TRUE);
    assert(loaded.m_pLinkPool[0].m_pLinkEnt == NULL); // stale disk pointer cleared
    assert(loaded.m_iLastActiveIdleSearch == 0 && loaded.m_iLastCoverSearch == 0);
    g_doorEdict.v.flags = 0;
    assert(loaded.FSetGraphPointers() == TRUE);
    assert(loaded.m_pLinkPool[0].m_pLinkEnt == &g_doorEdict.v);
    assert(FBitSet(g_doorEdict.v.flags, FL_GRAPHED) != 0);
    assert(loaded.NextNodeInRoute(0, 1, 0, 0) == 1);
    assert(loaded.NextNodeInRoute(-1, 1, 0, 0) == -1);
    assert(loaded.NextNodeInRoute(0, 99, 0, 0) == 0);
    assert(CALC_RANGE(0, 1, 0) == 0); // divide-by-zero guarded

    // 2. Reject inverted RegionMin > RegionMax (would divide by zero in CALC_RANGE)
    {
        CGraph bad = tmpl;
        bad.m_RegionMin[0] = 1.0f;
        bad.m_RegionMax[0] = 0.0f;
        g_fileData = Serialize(GRAPH_VERSION, bad, nodes, links, di, route, hash);
        assert(loaded.FLoadGraph(mapName) == FALSE);
        assert(loaded.m_fGraphPresent == TRUE); // existing valid graph preserved
    }

    // 3. Reject NaN region bounds, NaN node coordinates, negative link weight
    {
        CGraph bad = tmpl;
        bad.m_RegionMin[1] = std::numeric_limits<float>::quiet_NaN();
        g_fileData = Serialize(GRAPH_VERSION, bad, nodes, links, di, route, hash);
        assert(loaded.FLoadGraph(mapName) == FALSE);
    }
    {
        auto badNodes = nodes;
        badNodes[0].m_vecOrigin.x = std::numeric_limits<float>::infinity();
        g_fileData = Serialize(GRAPH_VERSION, tmpl, badNodes, links, di, route, hash);
        assert(loaded.FLoadGraph(mapName) == FALSE);
    }
    {
        auto badLinks = links;
        badLinks[0].m_flWeight = -1.0f;
        g_fileData = Serialize(GRAPH_VERSION, tmpl, nodes, badLinks, di, route, hash);
        assert(loaded.FLoadGraph(mapName) == FALSE);
    }

    // 4. Reject truncated file, bad version, bad hash step
    {
        g_fileData = Serialize(GRAPH_VERSION, tmpl, nodes, links, di, route, hash);
        g_fileData.pop_back();
        assert(loaded.FLoadGraph(mapName) == FALSE);
    }
    {
        g_fileData = Serialize(GRAPH_VERSION + 1, tmpl, nodes, links, di, route, hash);
        assert(loaded.FLoadGraph(mapName) == FALSE);
    }
    {
        CGraph bad = tmpl;
        bad.m_HashPrimes[0] = 2; // gcd(2, 4) != 1
        g_fileData = Serialize(GRAPH_VERSION, bad, nodes, links, di, route, hash);
        assert(loaded.FLoadGraph(mapName) == FALSE);
    }

    loaded.InitGraph();
    assert(loaded.m_nHashLinks == 0);
    assert(g_freeCount > 0);
    puts("PASS: Valve v16 node graph validation, pointer rebinding, and bounds checks");
    return 0;
}
'''


def main():
    source = NODES_CPP.read_text(encoding='latin-1')
    extracted = '\n\n'.join([
        extract_function(source, 'void CGraph :: InitGraph'),
        extract_function(source, 'int CGraph::NextNodeInRoute'),
        extract_function(source, 'inline int CALC_RANGE'),
        extract_function(source, 'int CGraph :: FLoadGraph'),
        extract_function(source, 'int CGraph :: FSetGraphPointers'),
    ])
    code = HARNESS.replace('__EXTRACTED__', extracted)
    with tempfile.TemporaryDirectory(prefix='goldsrc-valve-node-graph-') as folder:
        folder = Path(folder)
        test_cpp = folder / 'test.cpp'
        test_cpp.write_text(code, encoding='utf-8')
        exe = compile_program(
            [test_cpp],
            folder / 'test',
            includes=[NODES_H.parent],
            sanitizers=['address', 'undefined'],
        )
        subprocess.run([str(exe)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
