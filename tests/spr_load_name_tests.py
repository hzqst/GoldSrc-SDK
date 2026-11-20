"""Exercise production SPR_Load on sprites that fail to load and on the slots they leave behind."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK/engine'


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
typedef int qboolean;
typedef int HSPRITE;
enum { FALSE, TRUE };
#define SPR_MAX_SPRITES 4
struct model_t { int frames; };
typedef struct spritelist_s
{
	model_t* pSprite;
	char* pName;
	int frameCount;
} SPRITELIST;
static SPRITELIST gSpriteList[SPR_MAX_SPRITES];
static SPRITELIST* gSpriteListPointer = gSpriteList;
#define gSpriteList gSpriteListPointer
static int gSpriteCount = SPR_MAX_SPRITES;
static qboolean gSpriteMipMap = TRUE;
static int liveNames;
static model_t loadedModel = { 3 };
struct { void ( *pfnSPR_Load )( const char** ); } g_engdstAddrs = { []( const char** ) {} };
static void* Mem_Malloc( size_t bytes ) {++liveNames;return std::malloc(bytes);}
static void Mem_Free( void* pointer ) {--liveNames;std::free(pointer);}
static int Q_strlen( const char* s ) {return int(std::strlen(s));}
static void Q_strcpy( char* d, const char* s ) {std::strcpy(d,s);}
static int Q_strcasecmp( const char* a, const char* b ) {return std::strcmp(a,b);}
static void Sys_Error( const char* format, ... ) {std::fprintf(stderr,"%s",format);std::abort();}
// Names starting with "missing" fail to load
static model_t* Mod_ForName( const char* name, qboolean, qboolean ) {return std::strncmp(name,"missing",7)?&loadedModel:nullptr;}
static int ModelFrameCount( model_t* model ) {return model->frames;}
'''

TESTS = r'''
int main() {
 // A failed load returns 0 and keeps one name in the slot
 assert(0==SPR_Load("missing.spr"));assert(1==liveNames);

 // Loading it again frees the previous name instead of leaking it
 assert(0==SPR_Load("missing.spr"));assert(1==liveNames);

 // A new name reuses the slot without leaking the failed name
 assert(1==SPR_Load("a.spr"));assert(1==liveNames);
 assert(0==std::strcmp("a.spr",gSpriteList[0].pName));assert(3==gSpriteList[0].frameCount);

 // A loaded sprite is found again without allocating
 assert(1==SPR_Load("a.spr"));assert(1==liveNames);

 // A failure after a loaded slot uses the next slot, and the slot after it is reused the same way
 assert(0==SPR_Load("missing2.spr"));assert(2==liveNames);
 assert(2==SPR_Load("b.spr"));assert(2==liveNames);
 assert(0==std::strcmp("b.spr",gSpriteList[1].pName));

 for(int i=0;i<SPR_MAX_SPRITES;++i) if(gSpriteList[i].pName) Mem_Free(gSpriteList[i].pName);
 assert(0==liveNames);
}
'''


def main():
    source = (ROOT / 'cl_draw.cpp').read_text()
    code = STUBS + function(source, 'HSPRITE SPR_Load( const char* name )') + TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-spr-load-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code)
        executable = compile_program([path / 'test.cpp'], path / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=path)
    print('PASS: SPR_Load frees the name of a slot whose sprite failed to load before reusing it')


if __name__ == '__main__':
    main()
