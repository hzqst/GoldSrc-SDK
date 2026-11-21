"""Check OpenGL texture format rejection and the sprite format range."""
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


if __name__ == '__main__':
    main()
