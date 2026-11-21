"""Check a packed WAD miptex and truncated mip and palette data."""
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
#include <vector>
using byte = unsigned char;
constexpr int MIPLEVELS = 4;
struct miptex_t { char name[16]; unsigned width, height, offsets[MIPLEVELS]; };
static unsigned LittleLong(unsigned value) { return value; }
static unsigned short LittleShort(unsigned short value) { return value; }
'''

TESTS = r'''
int main()
{
    constexpr unsigned side = 16;
    constexpr unsigned mipBytes = 256 + 64 + 16 + 4;
    constexpr unsigned short colors = 256;
    miptex_t header{"brick", side, side, {40, 296, 360, 376}};
    std::vector<byte> bytes(sizeof(header) + mipBytes + sizeof(colors) + colors * 3);
    std::memcpy(bytes.data(), &header, sizeof(header));
    std::memcpy(bytes.data() + sizeof(header) + mipBytes, &colors, sizeof(colors));
    assert(TEX_ValidateMiptex(bytes.data(), bytes.size()));
    assert(!TEX_ValidateMiptex(bytes.data(), sizeof(header) - 1));
    assert(!TEX_ValidateMiptex(bytes.data(), sizeof(header) + mipBytes - 1));
    assert(!TEX_ValidateMiptex(bytes.data(), bytes.size() - 1));
    header.width = 0;
    std::memcpy(bytes.data(), &header, sizeof(header));
    assert(!TEX_ValidateMiptex(bytes.data(), bytes.size()));
}
'''


def main():
    production = [
        extract("textures.cpp", "static bool TEX_ValidateMiptex"),
    ]
    with tempfile.TemporaryDirectory(prefix='goldsrc-wad-miptex-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, *production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)


if __name__ == '__main__':
    main()
