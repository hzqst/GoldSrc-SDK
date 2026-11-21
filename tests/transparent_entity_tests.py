"""Exercise transparent entity sorting and visible entity registration for entities without a model."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK/engine'
RENDERER_VARIANTS = ('software', 'hardware')


def extract(filename, signature, occurrence=0, end='\n}'):
    source = (ROOT / filename).read_text(encoding='utf-8')
    start = -1
    for _ in range(occurrence + 1):
        start = source.index(signature, start + 1)
    return source[start:source.index(end, start) + len(end)]


STUBS = r'''
#include <cassert>
#include <cstdlib>
typedef float vec3_t[3];
#define VectorAdd(a, b, c) ((c)[0] = (a)[0] + (b)[0], (c)[1] = (a)[1] + (b)[1], (c)[2] = (a)[2] + (b)[2])
#define VectorSubtract(a, b, c) ((c)[0] = (a)[0] - (b)[0], (c)[1] = (a)[1] - (b)[1], (c)[2] = (a)[2] - (b)[2])
#define VectorScale(a, s, c) ((c)[0] = (a)[0] * (s), (c)[1] = (a)[1] * (s), (c)[2] = (a)[2] * (s))
#define DotProduct(a, b) ((a)[0] * (b)[0] + (a)[1] * (b)[1] + (a)[2] * (b)[2])
enum modtype_t { mod_brush, mod_sprite, mod_alias, mod_studio };
enum { kRenderNormal, kRenderTransColor, kRenderTransTexture, kRenderGlow, kRenderTransAlpha, kRenderTransAdd };
enum { ET_NORMAL, ET_PLAYER, ET_TEMPENTITY, ET_BEAM, ET_FRAGMENTED };
struct model_t { modtype_t type; vec3_t mins, maxs; };
struct entity_state_t { int rendermode; };
struct cl_entity_t { model_t* model; entity_state_t curstate; vec3_t origin; };
typedef struct { cl_entity_t* pEnt; float distance; } transObjRef;
#define MAX_VISEDICTS 512
#define MAX_BEAMENTS 64
static int maxTransObjs;
static transObjRef* transObjects;
static int numTransObjs;
static vec3_t r_origin;
static int cl_numvisedicts;
static cl_entity_t* cl_visedicts[MAX_VISEDICTS];
static int cl_numbeamentities;
static cl_entity_t* cl_beamentities[MAX_BEAMENTS];
static void Sys_Error(const char*, ...) { std::abort(); }
'''

TESTS = r'''
int main()
{
    transObjRef objects[2] = {};
    transObjects = objects;
    maxTransObjs = 2;

    cl_entity_t modelless = {};
    modelless.curstate.rendermode = kRenderTransAdd;
    AddTEntity(&modelless);
    assert(0 == numTransObjs);

    model_t sprite = {mod_sprite, {-1, -1, -1}, {1, 1, 1}};
    cl_entity_t glow = {&sprite, {kRenderGlow}, {3, 4, 0}};
    AddTEntity(&glow);
    assert(1 == numTransObjs && &glow == objects[0].pEnt && 25.0f == objects[0].distance);

    model_t brush = {mod_brush, {}, {}};
    cl_entity_t alphaBrush = {&brush, {kRenderTransAlpha}, {}};
    AddTEntity(&alphaBrush);
    assert(2 == numTransObjs && &alphaBrush == objects[0].pEnt && &glow == objects[1].pEnt);

    assert(0 == CL_AddVisibleEntity(ET_NORMAL, &modelless));
    assert(0 == cl_numvisedicts);
    assert(1 == CL_AddVisibleEntity(ET_NORMAL, &glow));
    assert(1 == cl_numvisedicts && &glow == cl_visedicts[0]);
    assert(1 == CL_AddVisibleEntity(ET_BEAM, &modelless));
    assert(1 == cl_numbeamentities && &modelless == cl_beamentities[0]);
}
'''


def main():
    visible_entity = extract('cdll_int.cpp', 'static int CL_AddVisibleEntity( int type, cl_entity_t* ent )')
    for occurrence, variant in enumerate(RENDERER_VARIANTS):
        production = [extract('r_trans.cpp', 'void AddTEntity( cl_entity_t* pEnt )', occurrence), visible_entity]
        with tempfile.TemporaryDirectory(prefix=f'goldsrc-trans-entity-{variant}-') as folder:
            work = Path(folder)
            source = work / 'test.cpp'
            source.write_text('\n'.join([STUBS, *production, TESTS]), encoding='utf-8')
            executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
            subprocess.run([str(executable)], cwd=work, check=True)


if __name__ == '__main__':
    main()
