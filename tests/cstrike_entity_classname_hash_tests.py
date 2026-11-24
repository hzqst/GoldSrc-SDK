"""Regression tests for cstrike MAKE_STRING_CLASS, CFireAndDie::Spawn, and CC4::KeyValue."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

SDK_ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'


def extract(relpath, signature):
    source = (SDK_ROOT / relpath).read_text(encoding='utf-8')
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

using uint64 = std::uint64_t;
using string_t = std::intptr_t;

enum { FALSE = 0, TRUE = 1 };

enum hash_types_e {
    CLASSNAME = 0
};

struct globalvars_t {
    const char* pStringBase = nullptr;
};

static char g_stringBase[1] = {};
static globalvars_t g_globals{ g_stringBase };
static globalvars_t* gpGlobals = &g_globals;

struct entvars_t {
    string_t classname = 0;
    string_t target = 0;
    string_t noise1 = 0;
    float speed = 0.0f;
};

struct KeyValueData {
    const char* szKeyName = nullptr;
    const char* szValue = nullptr;
    int fHandled = FALSE;
};

#define STRING(offset) ((const char*)(gpGlobals->pStringBase + (offset)))
#define MAKE_STRING(str) ((std::intptr_t)(str) - (std::intptr_t)(STRING(0)))
#define ALLOC_STRING(str) MAKE_STRING(str)

static bool FStrEq(const char* a, const char* b) {
    return std::strcmp(a, b) == 0;
}

static std::vector<std::pair<std::string, std::string>> g_hashLog;

static void RemoveEntityHashValue(entvars_t*, const char* value, hash_types_e type) {
    assert(CLASSNAME == type);
    g_hashLog.emplace_back("remove", value);
}

static void AddEntityHashValue(entvars_t*, const char* value, hash_types_e type) {
    assert(CLASSNAME == type);
    g_hashLog.emplace_back("add", value);
}

static int g_baseKeyValueCalls = 0;

struct CBaseEntity {
    entvars_t ev;
    entvars_t* pev = &ev;

    virtual void KeyValue(KeyValueData* pkvd) {
        ++g_baseKeyValueCalls;
        pkvd->fHandled = FALSE;
    }
};

struct CFireAndDie : CBaseEntity {
    void Spawn(void);
};

struct CC4 : CBaseEntity {
    void KeyValue(KeyValueData* data) override;
};
'''

TESTS = r'''
int main() {
    // 1. Fresh CFireAndDie::Spawn updates classname and adds to CLASSNAME hash table.
    {
        g_hashLog.clear();
        CFireAndDie entity;
        entity.Spawn();

        assert(0 == std::strcmp("fireanddie", STRING(entity.pev->classname)));
        assert(1 == g_hashLog.size());
        assert("add" == g_hashLog[0].first);
        assert("fireanddie" == g_hashLog[0].second);
    }

    // 2. Respawning an entity with an existing classname removes the old hash entry first.
    {
        g_hashLog.clear();
        CFireAndDie entity;
        entity.pev->classname = MAKE_STRING("previous_class");
        entity.Spawn();

        assert(0 == std::strcmp("fireanddie", STRING(entity.pev->classname)));
        assert(2 == g_hashLog.size());
        assert("remove" == g_hashLog[0].first);
        assert("previous_class" == g_hashLog[0].second);
        assert("add" == g_hashLog[1].first);
        assert("fireanddie" == g_hashLog[1].second);
    }

    // 3. CC4::KeyValue handles its known keys and delegates unknown keys to CBaseEntity::KeyValue.
    {
        g_baseKeyValueCalls = 0;
        CC4 c4;

        KeyValueData delay{ "detonatedelay", "45", FALSE };
        c4.KeyValue(&delay);
        assert(TRUE == delay.fHandled);
        assert(45.0f == c4.pev->speed);
        assert(0 == g_baseKeyValueCalls);

        KeyValueData unknown{ "origin", "0 0 0", TRUE };
        c4.KeyValue(&unknown);
        assert(FALSE == unknown.fHandled);
        assert(1 == g_baseKeyValueCalls);
    }

    return 0;
}
'''


def main():
    make_string_class = extract('cstrike/dlls/util.h', 'inline void MAKE_STRING_CLASS(')
    fire_and_die_spawn = extract('cstrike/dlls/triggers.cpp', 'void CFireAndDie::Spawn( void )')
    c4_keyvalue = extract('cstrike/dlls/wpn_shared/wpn_c4.cpp', 'void CC4::KeyValue( KeyValueData* data )')

    with tempfile.TemporaryDirectory(prefix='goldsrc-cstrike-entity-hash-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text(
            '\n'.join([STUBS, make_string_class, fire_and_die_spawn, c4_keyvalue, TESTS]),
            encoding='utf-8',
        )
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True, timeout=20)
    print('PASS: cstrike entity classname hash and CC4::KeyValue tests')


if __name__ == '__main__':
    main()
