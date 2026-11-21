"""Exercise server sound message sample handling, including a NULL sample name."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK/engine'


def extract_range(filename, first, last):
    source = (ROOT / filename).read_text(encoding='utf-8')
    start = source.index(first)
    return source[start:source.index(last, start)]


STUBS = r'''
#include <cassert>
#include <cctype>
#include <cstdlib>
#include <cstring>
#ifdef _MSC_VER
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif
typedef int qboolean;
typedef float vec_t;
struct edict_t { int index; };
struct sizebuf_t { int cursize; };
#define MAX_SOUNDS 512
#define MAX_SENTENCES 1536
#define SOUND_HASH_TABLE_SIZE 1023
#define CHAR_SENTENCE '!'
#define SND_SENTENCE (1 << 4)
#define SND_SPAWNING (1 << 8)
#define PITCH_NORM 100
#define svc_spawnstaticsound 29
enum { ss_loading = 1, ss_active = 2 };
static struct
{
    int state;
    qboolean sound_precache_hashedlookup_built;
    char* sound_precache[MAX_SOUNDS];
    short sound_precache_hashedlookup[SOUND_HASH_TABLE_SIZE];
    sizebuf_t datagram;
    sizebuf_t signon;
} sv;
static int printCount;
static void Con_Printf(const char*, ...) { ++printCount; }
static void Sys_Error(const char*, ...) { std::abort(); }
static int Q_atoi(const char* text) { return std::atoi(text); }
static int Q_strcasecmp(const char* left, const char* right) { return strcasecmp(left, right); }
static void Q_memset(void* destination, int value, size_t size) { std::memset(destination, value, size); }
static int NUM_FOR_EDICT(const edict_t* entity) { return entity->index; }
static unsigned int lastBitField;
static int writtenBitFields;
static void MSG_WriteByte(sizebuf_t* buffer, int) { ++buffer->cursize; }
static void MSG_WriteShort(sizebuf_t* buffer, int) { ++buffer->cursize; }
static void MSG_WriteCoord(sizebuf_t* buffer, float) { ++buffer->cursize; }
static void MSG_StartBitWriting(sizebuf_t*) {}
static void MSG_EndBitWriting(sizebuf_t*) {}
static void MSG_WriteBits(unsigned int value, int bits)
{
    ++writtenBitFields;
    if (8 == bits || 16 == bits)
        lastBitField = value;
}
static void MSG_WriteBitVec3Coord(const vec_t*) {}
'''

TESTS = r'''
int main()
{
    static char unused[] = "";
    static char footstep[] = "player/pl_step1.wav";
    sv.state = ss_active;
    sv.sound_precache[0] = unused;
    sv.sound_precache[1] = footstep;
    edict_t entity = {3};
    vec_t origin[3] = {};
    sizebuf_t buffer = {};

    assert(0 == SV_BuildSoundMsg(&entity, 0, nullptr, 255, 1.0f, 0, PITCH_NORM, origin, &buffer));
    assert(0 == buffer.cursize && 0 == writtenBitFields && 1 == printCount);

    assert(0 == SV_BuildSoundMsg(&entity, 0, "missing.wav", 255, 1.0f, 0, PITCH_NORM, origin, &buffer));
    assert(0 == buffer.cursize);

    assert(1 == SV_BuildSoundMsg(&entity, 0, "PLAYER/pl_step1.wav", 255, 1.0f, 0, PITCH_NORM, origin, &buffer));
    assert(1 == buffer.cursize && 1u == lastBitField);

    assert(1 == SV_BuildSoundMsg(&entity, 0, "!12", 255, 1.0f, 0, PITCH_NORM, origin, &buffer));
    assert(2 == buffer.cursize && 12u == lastBitField);

    assert(1 == SV_BuildSoundMsg(&entity, 0, "#3", 255, 1.0f, 0, PITCH_NORM, origin, &buffer));
    assert(3 == buffer.cursize && static_cast<unsigned int>(MAX_SENTENCES + 3) == lastBitField);
}
'''


def main():
    production = [
        extract_range('sv_main.cpp', 'int hashstrings_collisions;', 'void SV_StartSound('),
    ]
    with tempfile.TemporaryDirectory(prefix='goldsrc-sound-msg-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, *production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)


if __name__ == '__main__':
    main()
