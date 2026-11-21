"""Build PAS from exactly one word of leaves under AddressSanitizer."""
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
#include <cstdarg>
#include <cstdlib>
#include <cstring>
using byte = unsigned char;
struct leaf_t { const byte* compressed_vis; };
struct model_t { int numleafs; leaf_t* leafs; };
static byte *gPVS, *gPAS;
static int gPVSRowBytes, visible, audible, total;
static void CM_FreePAS() { std::free(gPVS); std::free(gPAS); }
static void* Mem_Calloc(int row, int count) { return std::calloc(count, row); }
static void CM_DecompressPVS(const byte* in, byte* out, int length) { std::memcpy(out, in, length); }
#define Q_memcpy std::memcpy
static void Con_DPrintf(const char* format, ...)
{
    if (format[0] != 'A') return;
    va_list args;
    va_start(args, format);
    visible = va_arg(args, int);
    audible = va_arg(args, int);
    total = va_arg(args, int);
    va_end(args);
}
'''

TESTS = r'''
int main()
{
    constexpr int leafCount = 32;
    const byte allVisible[] = {255, 255, 255, 255};
    leaf_t leaves[leafCount + 1];
    for (auto& leaf : leaves) leaf.compressed_vis = allVisible;
    model_t model{leafCount, leaves};
    CM_CalcPAS(&model);
    assert(leafCount + 1 == total);
    assert(31 == visible && 31 == audible);
    CM_FreePAS();
}
'''


def main():
    production = [
        extract("cmodel.cpp", "void CM_CalcPAS"),
    ]
    with tempfile.TemporaryDirectory(prefix='goldsrc-bsp-pas-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, *production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)


if __name__ == '__main__':
    main()
