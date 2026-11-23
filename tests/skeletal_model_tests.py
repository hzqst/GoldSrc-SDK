"""Exercise the Studio bounds loader's ownership and gait index checks."""
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
#include <cstddef>
#include <cstring>
using qboolean = bool;
using byte = unsigned char;
using FileHandle_t = int;
constexpr int IDSTUDIOHEADER = 1234;
struct studiohdr_t { int signature, otherFields[16]; };
struct entity_state_t { int sequence, gaitsequence; };
static struct { entity_state_t curstate; } object, *currententity = &object;
static struct { int numseq; } studio, *pstudiohdr = &studio;
static studiohdr_t input;
static int size = sizeof(input);
static bool mapped;
static int closed, released, freed, computed;
static constexpr float vec3_origin[3] = {};
static void VectorCopy(const float* src, float* dst) { std::memcpy(dst, src, sizeof(vec3_origin)); }
#define Q_strstr std::strstr
static FileHandle_t FS_Open(const char*, const char*) { return 1; }
static void* FS_GetReadBuffer(FileHandle_t, int* length) { *length = size; return mapped ? &input : nullptr; }
static byte* COM_LoadFileForMe(char*, int* length) { *length = size; return reinterpret_cast<byte*>(&input); }
static int LittleLong(int v) { return v; }
static int R_StudioComputeBounds(byte*, int, float*, float*) { ++computed; return 1; }
static void FS_ReleaseReadBuffer(FileHandle_t, byte*) { ++released; }
static void COM_FreeFile(byte*) { ++freed; }
static void FS_Close(FileHandle_t) { ++closed; }
'''

TESTS = r'''
int main()
{
    float mins[3], maxs[3];
    mapped = true;
    assert(0 == R_GetStudioBounds("models/test.mdl", mins, maxs));
    assert(1 == released && 0 == freed && 1 == closed);
    mapped = false;
    assert(0 == R_GetStudioBounds("models/test.mdl", mins, maxs));
    assert(1 == freed && 2 == closed);
    input.signature = IDSTUDIOHEADER;
    size = sizeof(input.signature);
    assert(0 == R_GetStudioBounds("models/test.mdl", mins, maxs));
    assert(0 == computed && 2 == freed);
    size = sizeof(input);
    assert(1 == R_GetStudioBounds("models/test.mdl", mins, maxs));
    assert(1 == computed && 3 == freed);

    studio.numseq = 3;
    object.curstate.sequence = -1;
    entity_state_t player{0, 3};
    CheckGait(&player);
    assert(0 == object.curstate.sequence && 0 == player.gaitsequence);
    player.gaitsequence = 2;
    CheckGait(&player);
    assert(2 == player.gaitsequence);
}
'''


def main():
    renderer_source = (ROOT / 'r_studio.cpp').read_text(encoding='utf-8')
    bounds_signature = 'int R_GetStudioBounds( const char* filename,'
    assert extract('r_studio.cpp', bounds_signature, 0) == extract('r_studio.cpp', bounds_signature, 1)
    gait_guards = []
    sections = renderer_source.split('void R_StudioProcessGait( entity_state_t* pplayer )\n{')[1:]
    for section in sections:
        start = section.index('\n\n') + 2
        end = section.index('\n\tpseqdesc =', start)
        gait_guards.append(section[start:end])
    assert 2 == len(gait_guards) and gait_guards[0] == gait_guards[1]
    production = [
        'void CheckGait(entity_state_t* pplayer) {\n' + gait_guards[0] + '\n}',
        extract("r_studio.cpp", "int R_GetStudioBounds( const char* filename,", 0),
    ]
    with tempfile.TemporaryDirectory(prefix='goldsrc-skeletal-model-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, *production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)


if __name__ == '__main__':
    main()
