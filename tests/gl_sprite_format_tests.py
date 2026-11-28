"""Check OpenGL texture formats and compact sprite palettes in both renderers."""
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
#include <stdexcept>
constexpr int SPR_NORMAL = 0, SPR_ALPHTEST = 3;
enum { TEX_TYPE_NONE, TEX_TYPE_ALPHA, TEX_TYPE_ALPHA_GRADIENT, TEX_TYPE_LUM, TEX_TYPE_RGBA };
enum { GL_RGBA, GL_RGB8, GL_RGBA8, GL_RGBA4, GL_LUMINANCE8, GL_LUMINANCE, GL_RGB5_A1 };
struct Sprite { int texFormat; };
struct Model { const char* name; };
static int LittleLong(int n) { return n; }
static void Sys_Error(const char*, ...) { throw std::runtime_error("invalid texture format"); }
'''

TESTS = r'''
static bool AcceptedSprite(int value)
{
    Sprite sprite{value};
    Model model{"test"};
    try { ValidateSprite(&model, &sprite); return true; }
    catch (const std::runtime_error&) { return false; }
}

int main()
{
    for (int type = TEX_TYPE_NONE; type <= TEX_TYPE_RGBA; ++type)
        assert(InternalFormat(type, 32) >= 0);
    try
    {
        InternalFormat(TEX_TYPE_RGBA + 1, 32);
        assert(false);
    }
    catch (const std::runtime_error&) {}

    assert(AcceptedSprite(SPR_NORMAL));
    assert(AcceptedSprite(SPR_ALPHTEST));
    assert(!AcceptedSprite(-1));
    assert(!AcceptedSprite(65536));
}
'''

PALETTE_STUBS = r'''
#include <cassert>
#include <cstring>
#include <stdexcept>
#include <vector>
#include "spritegn.h"
using byte = unsigned char;
using synctype_t = decltype(dsprite_t::synctype);
struct PackedColorVec { unsigned short r, g, b, a; };
struct mspriteframe_t {};
struct mspriteframedesc_t { spriteframetype_t type; mspriteframe_t* frameptr; };
struct msprite_t {
    int type, texFormat, maxwidth, maxheight, numframes, paloffset;
    float beamlength;
    mspriteframedesc_t frames[1];
};
enum { mod_sprite = 1 };
struct model_t {
    const char* name = "sprite";
    struct { void* data; } cache;
    synctype_t synctype;
    float mins[3], maxs[3];
    int numframes, flags, type;
};
static byte* pspritepal;
static int gSpriteTextureFormat;
static const char* loadname;
static std::vector<byte> storage;
static void* frameData;
static void* Hunk_AllocName(int size, const char*) { storage.assign(size, 0xFF); return storage.data(); }
static void Sys_Error(const char*, ...) { throw std::runtime_error("invalid sprite"); }
static int LittleLong(int n) { return n; }
static float LittleFloat(float n) { return n; }
#define Q_memcpy std::memcpy
#define Q_memset std::memset
static void* Mod_LoadSpriteFrame(void* frame, mspriteframe_t**, int = 0) { return frameData = frame; }
static void* Mod_LoadSpriteGroup(void*, mspriteframe_t**, int = 0) { assert(false); return nullptr; }
'''

PALETTE_TESTS = r'''
int main()
{
    for (int count : {2, SPRITE_PALETTE_COLORS})
    {
        const int frameOffset = SPRITE_PALETTE_OFFSET + count * SPRITE_COLOR_BYTES;
        std::vector<byte> input(frameOffset + sizeof(dspriteframetype_t));
        dsprite_t header{};
        header.version = SPRITE_VERSION;
        header.numframes = 1;
        std::memcpy(input.data(), &header, sizeof(header));
        input[sizeof(header)] = count & 255;
        input[sizeof(header) + 1] = count >> 8;
        input[SPRITE_PALETTE_OFFSET + 1] = 42;
        model_t model;
        Mod_LoadSpriteModel(&model, input.data(), static_cast<int>(input.size()));
        assert(frameData == input.data() + input.size());
        const auto* sprite = static_cast<const msprite_t*>(model.cache.data);
        const auto* colors = storage.data() + sprite->paloffset;
#ifdef TEST_GL
        assert(colors[1] == 42 && colors[SPRITE_PALETTE_COLORS * SPRITE_COLOR_BYTES - 1] == 0);
#else
        const auto* packed = reinterpret_cast<const PackedColorVec*>(colors);
        assert(packed[0].g == 42 && packed[SPRITE_PALETTE_COLORS - 1].b == 0);
#endif
    }
}
'''


def main():
    upload = extract('gl_draw.cpp', 'void GL_Upload32( unsigned int* data,', 1)
    start = upload.index('switch ( type )')
    stop = upload.index('\n\n', start)
    loader = extract('gl_model.cpp', 'void Mod_LoadSpriteModel(')
    start_sprite = loader.index('const int textureFormat')
    end_sprite = loader.index('\n\n', start_sprite)
    production = [
        'int InternalFormat(int type, int bpp) { int internalformat = -1, format;\n'
        + upload[start:stop].replace('return;', 'return -1;')
        + '\nreturn internalformat; }',
        'void ValidateSprite(Model* mod, Sprite* pin) {\n' + loader[start_sprite:end_sprite] + '\n}',
    ]
    with tempfile.TemporaryDirectory(prefix='goldsrc-gl-sprite-format-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, *production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
        for filename, defines in [('gl_model.cpp', ['TEST_GL']), ('model.cpp', [])]:
            source.write_text(PALETTE_STUBS + extract(filename, 'void Mod_LoadSpriteModel(')
                              + PALETTE_TESTS, encoding='utf-8')
            executable = compile_program([source], work / ('palette_' + Path(filename).stem),
                                         includes=[ROOT], defines=defines, sanitizers=['address'])
            subprocess.run([str(executable)], cwd=work, check=True)


if __name__ == '__main__':
    main()
