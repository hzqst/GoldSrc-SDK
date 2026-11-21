"""Exercise the production consistency-list lookup on a full table."""
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
#define ARRAYSIZE(a) (sizeof(a) / sizeof((a)[0]))
using qboolean = bool;
struct consistency_t { const char* filename; };
static struct { consistency_t consistency_list[3]; } sv;
static int Q_stricmp(const char* left, const char* right) { return std::strcmp(left, right); }
'''

TESTS = r'''
int main()
{
    auto& list = sv.consistency_list;
    consistency_t* found = nullptr;
    assert(!SV_FileInConsistencyList("first", &found));
    list[0] = {"first"};
    list[1] = {"middle"};
    list[2] = {"last"};
    assert(SV_FileInConsistencyList("last", &found));
    assert(&list[2] == found);
    assert(!SV_FileInConsistencyList("missing", &found));
    assert(&list[2] == found);
    assert(SV_FileInConsistencyList("first", nullptr));
}
'''


def main():
    production = [
        extract("sv_user.cpp", "qboolean SV_FileInConsistencyList"),
    ]
    with tempfile.TemporaryDirectory(prefix='goldsrc-server-file-consistency-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, *production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)


if __name__ == '__main__':
    main()
