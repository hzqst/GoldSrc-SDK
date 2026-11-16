"""Exercise production server model bounds and physent conversion."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK/engine'


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cstring>
#include <climits>
#include <initializer_list>
#define Q_memcpy std::memcpy
#define Q_strncpy std::strncpy
#define Q_strcpy std::strcpy
#define VectorCopy(a,b) std::memcpy(b,a,sizeof(float)*3)
constexpr int MAX_MODELS = 8, SOLID_NOT = 0, SOLID_BBOX = 2, SOLID_SLIDEBOX = 3, SOLID_BSP = 4;
constexpr int mod_brush = 0, mod_studio = 3, STUDIO_TRACE_HITBOX = 1, DAMAGE_NO = 0;
struct model_t { char name[64]; int type, flags; };
static struct { model_t* models[MAX_MODELS]; } sv;
static struct { int maxclients; } svs;
struct Vars {
 int modelindex, solid, classname, rendermode, skin, sequence, movetype, iuser1, iuser2, iuser3, iuser4;
 float frame, fuser1, fuser2, fuser3, fuser4;
 float origin[3], angles[3], mins[3], maxs[3], vuser1[3], vuser2[3], vuser3[3], vuser4[3];
 unsigned char controller[4], blending[2];
};
struct edict_t { Vars v; };
struct physent_t : Vars { int info, player, takedamage, blooddecal; model_t *model, *studiomodel; char name[32]; };
struct sv_adjusted_positions_t { float oldorg[3]; };
static sv_adjusted_positions_t* SV_GetAdjustedPosition(int) {return nullptr;}
static const char* pr_strings = "";
'''

TESTS = r'''
int main() {
 model_t studio = {"studio", mod_studio, STUDIO_TRACE_HITBOX}, brush = {"brush", mod_brush, 0};
 sv.models[0] = sv.models[1] = &studio; sv.models[MAX_MODELS-1] = &brush;
 for(int solid : {SOLID_NOT,SOLID_BBOX,SOLID_SLIDEBOX,SOLID_BSP}) {
  for(int index : {INT_MIN,-1,0,1,2,MAX_MODELS-1,MAX_MODELS,INT_MAX}) {
   edict_t entity = {}; entity.v.solid = solid; entity.v.modelindex = index;
   for(int axis=0;axis<3;++axis) {entity.v.mins[axis]=-float(axis+1);entity.v.maxs[axis]=float(axis+1);}
   physent_t result = {}; result.model = result.studiomodel = &brush;
   for(int axis=0;axis<3;++axis) {result.mins[axis]=999;result.maxs[axis]=999;}
   SV_CopyEdictToPhysent(&result,3,&entity);
   model_t* expected = index == 1 ? &studio : index == MAX_MODELS-1 ? &brush : nullptr;
   assert(expected == SV_ModelForIndex(index));
   assert((solid==SOLID_BSP || solid==SOLID_NOT ? expected : nullptr) == result.model);
   assert(((solid==SOLID_BBOX || solid==SOLID_SLIDEBOX) && expected==&studio ? &studio : nullptr)==result.studiomodel);
   if(solid==SOLID_BBOX || solid==SOLID_SLIDEBOX || (solid==SOLID_BSP && !expected)) {
    for(int axis=0;axis<3;++axis) {assert(entity.v.mins[axis]==result.mins[axis]);assert(entity.v.maxs[axis]==result.maxs[axis]);}
   }
   if(expected && solid!=SOLID_SLIDEBOX) assert(0==std::strcmp(expected->name,result.name));
  }
 }
}
'''


def main():
    main_source = (ROOT / 'sv_main.cpp').read_text()
    user = (ROOT / 'sv_user.cpp').read_text()
    code = STUBS + function(main_source, 'model_t* SV_ModelForIndex(')
    code += function(user, 'void SV_CopyEdictToPhysent(') + TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-server-model-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code)
        compiler = shlex.split(os.environ.get('CXX', 'c++'))
        subprocess.run(compiler + ['-std=c++17', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                                  str(path / 'test.cpp'), '-o', str(path / 'test')], check=True)
        subprocess.run([str(path / 'test')], check=True)
    print('PASS: all physent solids, model bounds, missing models and reused bounds')


if __name__ == '__main__':
    main()
