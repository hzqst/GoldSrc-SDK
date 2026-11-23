"""Exercise COM_NormalizeAngles keeping multi-turn positive and negative angles in [-180, 180]."""
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
#include <cmath>
using vec_t = float;
'''

TESTS = r'''
static bool Near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

int main()
{
    {
        vec_t a[3] = { 360.0f, 721.0f, -721.0f };
        COM_NormalizeAngles(a);
        assert(Near(0.0f, a[0]));
        assert(Near(1.0f, a[1]));
        assert(Near(-1.0f, a[2]));
    }
    {
        vec_t a[3] = { -360.0f, 270.0f, -270.0f };
        COM_NormalizeAngles(a);
        assert(Near(0.0f, a[0]));
        assert(Near(-90.0f, a[1]));
        assert(Near(90.0f, a[2]));
    }
    {
        vec_t a[3] = { 540.0f, -540.0f, 1085.5f };
        COM_NormalizeAngles(a);
        assert(Near(180.0f, a[0]));
        assert(Near(-180.0f, a[1]));
        assert(Near(5.5f, a[2]));
    }
    {
        vec_t a[3] = { -180.0f, 0.0f, 180.0f };
        COM_NormalizeAngles(a);
        assert(Near(-180.0f, a[0]));
        assert(Near(0.0f, a[1]));
        assert(Near(180.0f, a[2]));
    }
}
'''


def main():
    production = extract('common.cpp', 'void COM_NormalizeAngles( vec_t* angles )')
    with tempfile.TemporaryDirectory(prefix='goldsrc-normalize-angles-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
    print('PASS: COM_NormalizeAngles keeps multi-turn angles in [-180, 180]')


if __name__ == '__main__':
    main()
