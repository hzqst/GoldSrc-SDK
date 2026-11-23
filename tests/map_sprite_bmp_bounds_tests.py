"""Exercise R_LoadMapSprite rejecting oversized 8-bit BMP images before expanding pixels."""
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


PREAMBLE = r'''
#include <cassert>
#include <cctype>
#include <cstdlib>
#include <cstring>

using byte = unsigned char;
using qboolean = int;
constexpr qboolean FALSE = 0;
constexpr int MAX_PATH = 260;
constexpr int MAP_IMAGE_SIZE = 1024 * 1024 * 3;
using FileHandle_t = void*;

struct model_t { bool needload = true; };
static model_t g_model;
static int g_overviewCalls = 0;
static int g_bmpWidth = 0;
static int g_bmpHeight = 0;
static bool g_bmpOk = true;
static int g_liveAllocs = 0;

static void Q_strncpy(char* dst, const char* src, int n) { std::strncpy(dst, src, n); }
static int Q_strlen(const char* s) { return static_cast<int>(std::strlen(s)); }
static int Q_strcmp(const char* a, const char* b) { return std::strcmp(a, b); }
static void Q_memset(void* p, int v, int n) { std::memset(p, v, n); }
static void Q_memcpy(void* d, const void* s, int n) { std::memcpy(d, s, n); }
static char* Q_strlwr(char* s)
{
    for (char* p = s; *p; ++p) *p = static_cast<char>(std::tolower(static_cast<unsigned char>(*p)));
    return s;
}
static void* Mem_Malloc(size_t n) { ++g_liveAllocs; return std::malloc(n); }
static void Mem_Free(void* p) { if (p) { --g_liveAllocs; std::free(p); } }
static void Con_Printf(const char*, ...) {}
static void Sys_Error(const char*, ...) { assert(false && "Sys_Error called"); }
static model_t* Mod_FindName(bool, const char*) { return &g_model; }
static FileHandle_t FS_Open(const char*, const char*) { return reinterpret_cast<FileHandle_t>(1); }
static int LoadTGA(const char*, byte*, int, int*, int*) { return 0; }
static void LoadBMP8(FileHandle_t, byte** palette, int* numcolors, byte** indexed, int* width, int* height)
{
    *width = g_bmpWidth;
    *height = g_bmpHeight;
    *numcolors = 256;
    if (!g_bmpOk)
    {
        *palette = nullptr;
        *indexed = nullptr;
        return;
    }
    *palette = static_cast<byte*>(Mem_Malloc(256 * 3));
    std::memset(*palette, 128, 256 * 3);
    const size_t count = g_bmpWidth > 0 && g_bmpHeight > 0 ? static_cast<size_t>(g_bmpWidth) * static_cast<size_t>(g_bmpHeight) : 1;
    *indexed = static_cast<byte*>(Mem_Malloc(count));
    std::memset(*indexed, 1, count);
}

static struct {
    void LoadMapSprite(const char**) {}
} g_engdstAddrs;

namespace hw_ns {
static void R_LoadOverviewSpriteModel(model_t*, byte*, int, int) { ++g_overviewCalls; }
'''

MID = r'''
} // namespace hw_ns

namespace sw_ns {
static void R_LoadOverviewSpriteModel(model_t*, byte*, byte*, int, int) { ++g_overviewCalls; }
'''

TESTS = r'''
} // namespace sw_ns

int main()
{
    // 1. Oversized 1024x1024 BMP (4 MiB RGBA > 3 MiB MAP_IMAGE_SIZE, and 1 MiB > 0xC0000 in SW) must be rejected without heap overwrite or leaks.
    g_bmpOk = true;
    g_bmpWidth = 1024;
    g_bmpHeight = 1024;
    g_overviewCalls = 0;
    assert(nullptr == hw_ns::R_LoadMapSprite("overviews/big.bmp"));
    assert(0 == g_overviewCalls);
    assert(0 == g_liveAllocs);

    assert(nullptr == sw_ns::R_LoadMapSprite("overviews/big.bmp"));
    assert(0 == g_overviewCalls);
    assert(0 == g_liveAllocs);

    // 2. Valid 1024x768 BMP (exact max: 786432 pixels -> 3 MiB RGBA / 0xC0000 indexed) succeeds in both renderers.
    g_bmpWidth = 1024;
    g_bmpHeight = 768;
    assert(&g_model == hw_ns::R_LoadMapSprite("overviews/ok.bmp"));
    assert(1 == g_overviewCalls);
    assert(0 == g_liveAllocs);

    assert(&g_model == sw_ns::R_LoadMapSprite("overviews/ok.bmp"));
    assert(2 == g_overviewCalls);
    assert(0 == g_liveAllocs);

    // 3. Failed LoadBMP8 cleanly returns NULL with no leaks.
    g_bmpOk = false;
    assert(nullptr == hw_ns::R_LoadMapSprite("overviews/bad.bmp"));
    assert(nullptr == sw_ns::R_LoadMapSprite("overviews/bad.bmp"));
    assert(0 == g_liveAllocs);
}
'''


def main():
    hw = extract('gl_rmain.cpp', 'model_t* R_LoadMapSprite( const char* filename )')
    sw = extract('r_main.cpp', 'model_t* R_LoadMapSprite( const char* filename )')
    with tempfile.TemporaryDirectory(prefix='goldsrc-map-sprite-bmp-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([PREAMBLE, hw, MID, sw, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
    print('PASS: R_LoadMapSprite rejects oversized 8-bit BMPs without heap overrun')


if __name__ == '__main__':
    main()
