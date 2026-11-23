"""Exercise SV_CreateResourceList clamping missing FS_FileSize (-1) to 0."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK/engine'


def extract(filename, signature):
    source = (ROOT / filename).read_text(encoding='utf-8')
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using qboolean = int;
constexpr qboolean FALSE = 0;
constexpr qboolean TRUE = 1;
constexpr int MAX_EVENT_SCRIPTS = 8;
constexpr int RES_FATALIFMISSING = 1;
enum resourcetype_t { t_sound = 0, t_skin, t_model, t_decal, t_generic, t_eventscript };

struct event_t {
    const char* filename = nullptr;
    const char* pszScript = nullptr;
    int filesize = 0;
};

struct decalname_t { const char* name; };
static decalname_t sv_decalnames[2] = {};
static int sv_decalnamecount = 0;
static int Draw_DecalSize(int) { return 0; }

static struct {
    int maxclients = 16;
} svs;

static struct {
    int num_resources = 0;
    const char* generic_precache[8] = {};
    const char* sound_precache[8] = {};
    const char* model_precache[8] = {};
    unsigned char model_precache_flags[8] = {};
    event_t event_precache[MAX_EVENT_SCRIPTS] = {};
} sv;

static std::map<std::string, int> g_fileSizes;
static int FS_FileSize(const char* name)
{
    auto it = g_fileSizes.find(name);
    return it == g_fileSizes.end() ? -1 : it->second;
}

static char* va(const char* fmt, ...)
{
    static char buf[256];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    return buf;
}

struct Recorded {
    resourcetype_t type;
    std::string name;
    int size;
    unsigned char flags;
    int index;
};
static std::vector<Recorded> g_resources;

static void SV_AddResource(resourcetype_t type, const char* name, int size, unsigned char flags, int index)
{
    g_resources.push_back({type, name, size, flags, index});
    ++sv.num_resources;
}
'''

TESTS = r'''
int main()
{
    g_fileSizes["maps/ok.txt"] = 1234;
    g_fileSizes["sound/weapons/ok.wav"] = 4567;
    g_fileSizes["models/ok.mdl"] = 8901;

    sv.generic_precache[1] = "maps/missing.txt";
    sv.generic_precache[2] = "maps/ok.txt";
    sv.sound_precache[1] = "weapons/missing.wav";
    sv.sound_precache[2] = "weapons/ok.wav";
    sv.model_precache[1] = "models/missing.mdl";
    sv.model_precache[2] = "models/ok.mdl";
    sv.model_precache[3] = "*1";

    SV_CreateResourceList();

    assert(7 == g_resources.size());
    // Missing files must have size 0, never -1
    assert(t_generic == g_resources[0].type && 0 == g_resources[0].size);
    assert(t_generic == g_resources[1].type && 1234 == g_resources[1].size);
    assert(t_sound == g_resources[2].type && 0 == g_resources[2].size);
    assert(t_sound == g_resources[3].type && 4567 == g_resources[3].size);
    assert(t_model == g_resources[4].type && 0 == g_resources[4].size);
    assert(t_model == g_resources[5].type && 8901 == g_resources[5].size);
    assert(t_model == g_resources[6].type && 0 == g_resources[6].size);
}
'''


def main():
    production = extract('sv_main.cpp', 'void SV_CreateResourceList()')
    with tempfile.TemporaryDirectory(prefix='goldsrc-server-resource-list-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
    print('PASS: SV_CreateResourceList clamps missing file sizes to 0')


if __name__ == '__main__':
    main()
