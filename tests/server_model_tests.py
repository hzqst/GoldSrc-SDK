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
 int modelindex, solid, classname, rendermode, skin, sequence, movetype, iuser1, iuser2, iuser3, iuser4, flags;
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
static void TestPhysents() {
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

STUDIO_STUBS = r'''
#include <cmath>
#include <cstddef>
using byte = unsigned char;
using qboolean = int;
using vec3_t = float[3];
constexpr int TRUE = 1, FL_CLIENT = 8, PITCH = 0, YAW = 1, ROLL = 2, mod_sprite = 2;
#define VectorSubtract(a,b,c) for(int axis=0;axis<3;++axis) c[axis]=a[axis]-b[axis]
#define DotProduct(a,b) ((a)[0]*(b)[0]+(a)[1]*(b)[1]+(a)[2]*(b)[2])
struct studiohdr_t {int seqindex, attachmentindex;};
struct mstudioseqdesc_t {float bbmin[3], bbmax[3];};
struct mstudioattachment_t {int bone; float org[3];};
static struct Blob { studiohdr_t header; mstudioseqdesc_t sequence; mstudioattachment_t attachment; } blob;
static studiohdr_t* pstudiohdr;
static float bonetransform[2][3][4];
static int extraReads, boneSetups;
static void* Mod_Extradata(model_t* model) {
 assert(nullptr!=model); assert(mod_studio==model->type); ++extraReads; return &blob;
}
static void SetupBones(model_t* model, float, int, const float*, const float*,
                       const unsigned char*, const unsigned char*, int bone, const edict_t*) {
 assert(mod_studio==model->type); ++boneSetups;
 for(int r=0;r<3;++r) {bonetransform[bone][r][r]=1;bonetransform[bone][r][3]=float((r+1)*11);}
}
struct API {decltype(&SetupBones) SV_StudioSetupBones;};
static API api = {SetupBones}, *g_pSvBlendingAPI=&api;
static void VectorTransform(const float* input, const float matrix[3][4], float* output) {
 for(int r=0;r<3;++r) output[r]=DotProduct(input,matrix[r])+matrix[r][3];
}
'''

STUDIO_TESTS = r'''
int main() {
 TestPhysents();
 model_t studio = {"studio",mod_studio,0}, brush = {"brush",mod_brush,0}, sprite = {"sprite",mod_sprite,0};
 std::memset(&sv,0,sizeof(sv));sv.models[1]=&studio;sv.models[2]=&brush;sv.models[3]=&sprite;
 edict_t entity={};entity.v.flags=FL_CLIENT;
 for(int a=0;a<3;++a) entity.v.origin[a]=float(a+7);
 const float start[3]={-10,0,0}, end[3]={10,0,0};
 float angles[3]={9,9,9};
 for(int index : {INT_MIN,-1,0,2,3,4,MAX_MODELS,INT_MAX}) {
  entity.v.modelindex=index;
  assert(TRUE==SV_CheckSphereIntersection(&entity,start,end));
  float output[3]={}; GetBonePosition(&entity,0,output,angles);
  for(int a=0;a<3;++a) assert(entity.v.origin[a]==output[a]);
  GetAttachment(&entity,0,output,angles);
  for(int a=0;a<3;++a) assert(entity.v.origin[a]==output[a]);
  GetBonePosition(&entity,0,nullptr,nullptr);GetAttachment(&entity,0,nullptr,nullptr);
 }
 assert(0==extraReads);assert(0==boneSetups);assert(9==angles[0]);
 blob.header={offsetof(Blob,sequence),offsetof(Blob,attachment)};
 blob.sequence={{-1,-1,-1},{1,1,1}};blob.attachment={1,{1,2,3}};
 entity.v.modelindex=1;std::memset(entity.v.origin,0,sizeof(entity.v.origin));
 assert(TRUE==SV_CheckSphereIntersection(&entity,start,end));
 const float outsideStart[3]={-10,10,0},outsideEnd[3]={10,10,0};
 assert(0==SV_CheckSphereIntersection(&entity,outsideStart,outsideEnd));
 float output[3];GetBonePosition(&entity,0,output,angles);
 for(int a=0;a<3;++a) assert(float((a+1)*11)==output[a]);
 GetAttachment(&entity,0,output,angles);
 for(int a=0;a<3;++a) assert(float((a+1)*12)==output[a]);
 assert(4==extraReads);assert(2==boneSetups);
}
'''


def main():
    main_source = (ROOT / 'sv_main.cpp').read_text()
    user = (ROOT / 'sv_user.cpp').read_text()
    code = STUBS + function(main_source, 'model_t* SV_ModelForIndex(')
    code += function(user, 'void SV_CopyEdictToPhysent(') + TESTS + STUDIO_STUBS
    studio = (ROOT / 'r_studio.cpp').read_text()
    with tempfile.TemporaryDirectory(prefix='goldsrc-server-model-') as folder:
        path = Path(folder)
        compiler = shlex.split(os.environ.get('CXX', 'c++'))
        for variant in range(2):
            functions = ''
            for signature in ('qboolean SV_CheckSphereIntersection(', 'void GetBonePosition(', 'void GetAttachment('):
                parts = studio.split(signature)
                assert len(parts) == 3
                functions += signature + parts[variant + 1].split('\n}', 1)[0] + '\n}\n'
            (path / 'test.cpp').write_text(code + functions + STUDIO_TESTS)
            subprocess.run(compiler + ['-std=c++17', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                                      str(path / 'test.cpp'), '-o', str(path / 'test')], check=True)
            subprocess.run([str(path / 'test')], check=True)
            print('PASS: physents and studio model guards, variant', variant)


if __name__ == '__main__':
    main()
