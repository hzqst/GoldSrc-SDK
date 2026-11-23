"""Exercise Q_memcmp comparing tail bytes and COM_ExpandFilename checking FS_GetLocalPath."""
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
#include <cstdint>
#include <cstring>

constexpr int MEMORY_WORD_BYTES = sizeof(std::uint32_t);
constexpr int MEMORY_WORD_SHIFT = 2;
constexpr int STRING_NOT_EQUAL = -1;
constexpr int GAME_DIRECTORY_BYTES = 256;

static bool g_found = false;
static const char* g_expanded = "";

static const char* FS_GetLocalPath(const char*, char* out, int outSize)
{
    if (!g_found)
        return nullptr;
    std::strncpy(out, g_expanded, outSize - 1);
    out[outSize - 1] = 0;
    return out;
}
'''

TESTS = r'''
int main()
{
    // 1. Q_memcmp must compare the trailing 1..3 bytes past the 4-byte word loop.
    {
        const unsigned char a[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
        unsigned char b[10];
        std::memcpy(b, a, sizeof(a));
        assert(0 == Q_memcmp(a, b, 10));

        // Differ only in the 5th byte (remainder 1)
        b[4] = 99;
        assert(0 != Q_memcmp(a, b, 5));
        assert(0 == Q_memcmp(a, b, 4));
        b[4] = a[4];

        // Differ only in the last byte of a 10-byte IPX-sized buffer (remainder 2)
        b[9] = 99;
        assert(0 != Q_memcmp(a, b, 10));
        assert(0 == Q_memcmp(a, b, 9));
        b[9] = a[9];

        // Differ in 7th byte (remainder 3)
        b[6] = 99;
        assert(0 != Q_memcmp(a, b, 7));
        assert(0 == Q_memcmp(a, b, 6));
    }

    // 2. COM_ExpandFilename preserves filename and returns 0 when FS_GetLocalPath fails.
    {
        char buf[GAME_DIRECTORY_BYTES] = "cl_dlls/client.dll";
        g_found = false;
        assert(0 == COM_ExpandFilename(buf));
        assert(0 == std::strcmp("cl_dlls/client.dll", buf));

        g_found = true;
        g_expanded = "C:/game/valve/cl_dlls/client.dll";
        assert(1 == COM_ExpandFilename(buf));
        assert(0 == std::strcmp("C:/game/valve/cl_dlls/client.dll", buf));
    }
}
'''


def main():
    production = [
        extract('common.cpp', 'int Q_memcmp( const void* first, const void* second, int count )'),
        extract('common.cpp', 'int COM_ExpandFilename( char* filename )'),
    ]
    with tempfile.TemporaryDirectory(prefix='goldsrc-common-memcmp-expand-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, *production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
    print('PASS: Q_memcmp compares tail bytes and COM_ExpandFilename checks FS_GetLocalPath')


if __name__ == '__main__':
    main()
