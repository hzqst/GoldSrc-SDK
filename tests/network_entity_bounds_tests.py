"""Exercise entity lookup and temporary effects at client table boundaries."""
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
#include <cstddef>
#include <deque>
#define ARRAYSIZE(a) (sizeof(a) / sizeof((a)[0]))
#define BEAMENT_ENTITY(index) ((index) & 0xFFF)
struct cl_entity_t { float origin[3], attachment[4][3]; };
static struct { int max_edicts, viewentity; } cl;
static cl_entity_t* cl_entities;
static std::deque<int> fields;
static int userEntityIndex;
static cl_entity_t userEntity;
static int MSG_ReadShort()
{
    assert(!fields.empty());
    const int value = fields.front();
    fields.pop_front();
    return value;
}
static int MSG_ReadByte() { return MSG_ReadShort(); }
#define TE_FIZZ 1
static cl_entity_t* fizzTarget;
static struct
{
    void R_FizzEffect(cl_entity_t* target, int model, int density)
    {
        assert(12 == model && 3 == density);
        fizzTarget = target;
    }
} efx;
static void Con_DPrintf(const char*, ...) {}
static cl_entity_t* ClientDLL_GetUserEntity(int n) { userEntityIndex = n; return &userEntity; }
'''

TESTS = r'''
int main()
{
    cl_entity_t entities[2] = {};
    cl.max_edicts = 2;
    assert(nullptr == CL_GetEntityIfValid(0));
    cl_entities = entities;
    assert(&entities[0] == CL_GetEntityIfValid(0));
    assert(nullptr == CL_GetEntityIfValid(-1));
    assert(nullptr == CL_GetEntityIfValid(2));
    assert(entities[1].attachment[3] == R_GetAttachmentPoint(1, 4));
    assert(entities[1].origin == R_GetAttachmentPoint(1, 5));
    assert(entities[1].origin == R_GetAttachmentPoint(1, -1));
    assert(nullptr == R_GetAttachmentPoint(2, 1));

    fields = {1};
    CL_Parse_SetView();
    assert(1 == cl.viewentity);
    fields = {2};
    CL_Parse_SetView();
    assert(1 == cl.viewentity);
    assert(&entities[1] == R_GetBeamAttachmentEntity(1));
    assert(nullptr == R_GetBeamAttachmentEntity(2));
    assert(&userEntity == R_GetBeamAttachmentEntity(-5));
    assert(5 == userEntityIndex);

    fields = {2, 12, 3, 25};
    ParseFizz(TE_FIZZ);
    assert(nullptr == fizzTarget);
    assert(25 == MSG_ReadShort());
    fields = {1, 12, 3};
    ParseFizz(TE_FIZZ);
    assert(&entities[1] == fizzTarget && fields.empty());
}
'''


def main():
    production = [
        extract("client.h", "inline cl_entity_t* CL_GetEntityIfValid("),
        extract("cl_tent.cpp", "float* R_GetAttachmentPoint("),
        extract("cl_parsefn.cpp", "void CL_Parse_SetView()"),
        extract("r_part.cpp", "cl_entity_t* R_GetBeamAttachmentEntity( int index )\n{"),
        'void ParseFizz(int type) { switch (type) {\n'
        + extract('cl_tent.cpp', 'case TE_FIZZ:\n\t{', end='\n\t}')
        + '\n}}',
    ]
    with tempfile.TemporaryDirectory(prefix='goldsrc-network-entity-bounds-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, *production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)


if __name__ == '__main__':
    main()
