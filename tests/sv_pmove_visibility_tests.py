"""Exercise the server's visible physent insertion at capacity."""
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
constexpr int MAX_PHYSENTS = 3;
using physent_t = int;
using edict_t = int;
static struct { int numvisent; physent_t visents[MAX_PHYSENTS]; int sentinel; } move, *pmove = &move;
static void SV_CopyEdictToPhysent(physent_t* dest, int index, edict_t*) { *dest = index; }
'''

TESTS = r'''
int main()
{
    move.numvisent = MAX_PHYSENTS - 1;
    move.sentinel = 13;
    AddVisible(42, nullptr);
    assert(MAX_PHYSENTS == move.numvisent);
    assert(42 == move.visents[MAX_PHYSENTS - 1]);
    AddVisible(99, nullptr);
    assert(MAX_PHYSENTS == move.numvisent);
    assert(13 == move.sentinel && 42 == move.visents[MAX_PHYSENTS - 1]);
}
'''


def main():
    source_text = (ROOT / 'sv_user.cpp').read_text(encoding='utf-8')
    start = source_text.index('int entityIndex = NUM_FOR_EDICT( entity );', source_text.index('static void SV_AddLinksToPM_( areanode_t* node, float* mins, float* maxs )\n{'))
    start = source_text.index('\n', start)
    end = source_text.index('\n\t\t\tif ( entity->v.solid', start)
    production = ['void AddVisible(int entityIndex, edict_t* entity) {' + source_text[start:end] + '\n}']
    with tempfile.TemporaryDirectory(prefix='goldsrc-sv-pmove-visibility-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, *production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)


if __name__ == '__main__':
    main()
