"""Exercise ClientDLL precache lookups and the player info write path."""
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
#include <cstring>
constexpr int MAX_MODELS = 4, MAX_EVENT_SCRIPTS = 4;
struct model_t { const char* name; };
struct event_t { const char* filename; };
static struct { model_t* model_precache[MAX_MODELS]; event_t event_precache[MAX_EVENT_SCRIPTS]; } cl;
static struct { char userinfo[64]; int state; struct { int message; } netchan; } cls;
static struct
{
    void PlayerInfo_SetValueForKey(const char**, const char**) {}
    void CL_LoadModel(const char**, int**) {}
    void pfnPrecacheEvent(int*, const char**) {}
} g_engdstAddrs;
constexpr int ca_connecting = 1, clc_stringcmd = 3;
static int Q_strcasecmp(const char* left, const char* right) { return std::strcmp(left, right); }
static model_t* CL_GetModelByIndex(int n) { return cl.model_precache[n]; }
static const char* Info_ValueForKey(const char* info, const char*) { return info; }
static void Info_SetValueForStarKey(char* info, const char*, const char* value, size_t) { std::strcpy(info, value); }
static void Info_SetValueForKey(char*, const char*, const char*, size_t) {}
static void MSG_WriteByte(int*, int) {}
static void SZ_Print(int*, const char*) {}
static const char* va(const char*, ...) { return ""; }
'''

TESTS = r'''
int main()
{
    model_t models[] = {{"first"}, {"middle"}, {"last"}};
    for (int n = 1; n < MAX_MODELS; ++n) cl.model_precache[n] = &models[n - 1];
    cl.event_precache[1] = {"first"};
    cl.event_precache[2] = {"middle"};
    cl.event_precache[3] = {"last"};
    int index = 0;
    assert(&models[2] == CL_LoadModel("last", &index));
    assert(3 == index);
    assert(nullptr == CL_LoadModel("missing", &index));
    assert(-1 == index);
    assert(3 == hudPrecacheEvent(0, "last"));
    assert(0 == hudPrecacheEvent(0, "missing"));
    PlayerInfo_SetValueForKey("*fid", "42");
    assert(0 == std::strcmp("42", cls.userinfo));
}
'''


def main():
    production = [
        extract("cdll_int.cpp", "void PlayerInfo_SetValueForKey("),
        extract("cdll_int.cpp", "model_t* CL_LoadModel("),
        extract("cdll_int.cpp", "unsigned short hudPrecacheEvent("),
    ]
    with tempfile.TemporaryDirectory(prefix='goldsrc-engine-client-api-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, *production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)


if __name__ == '__main__':
    main()
