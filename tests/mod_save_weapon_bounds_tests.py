"""Regression tests for AddAmmoNameToAmmoRegistry overflow guard and CRestore::ReadFields
bounds/null/Empty guards across all 9 mods (ported from HLND2T PR #1694).

Run: python -I tests/mod_save_weapon_bounds_tests.py
"""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'

MODS = [
    ('valve', 'valve/dlls/weapons.cpp', 'valve/dlls/util.cpp'),
    ('bshift', 'bshift/dlls/weapons.cpp', 'bshift/dlls/util.cpp'),
    ('cstrike', 'cstrike/dlls/weapons.cpp', 'cstrike/dlls/util.cpp'),
    ('czeror', 'czeror/dlls/weapons.cpp', 'czeror/dlls/util.cpp'),
    ('dmc', 'dmc/dlls/weapons.cpp', 'dmc/dlls/util.cpp'),
    ('dod', 'dod/dlls/weapons.cpp', 'dod/dlls/util.cpp'),
    ('gearbox', 'gearbox/dlls/weapons.cpp', 'gearbox/dlls/util.cpp'),
    ('ricochet', 'ricochet/dlls/weapons.cpp', 'ricochet/dlls/util.cpp'),
    ('tfc', 'tfc/weapons.cpp', 'tfc/util.cpp'),
]


def extract_function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    for idx in range(brace, len(source)):
        ch = source[idx]
        if ch == '{':
            depth += 1
        elif ch == '}':
            depth -= 1
            if depth == 0:
                return source[start:idx + 1]
    raise RuntimeError(f'Unclosed function: {signature}')


HARNESS = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#define _stricmp strcasecmp
#endif

#define ASSERT(x) ((void)0)
#define MAX_AMMO_SLOTS 32
#define FTYPEDESC_GLOBAL 1

enum ALERT_TYPE { at_notice, at_console, at_aiconsole, at_warning, at_error, at_logged };
static int g_alertErrors = 0;
static void ALERT(ALERT_TYPE t, const char*, ...) {
    if (t == at_error) ++g_alertErrors;
}

struct AmmoInfo {
    const char* pszName;
    int iId;
};

struct CBasePlayerItem {
    static AmmoInfo AmmoInfoArray[MAX_AMMO_SLOTS];
};
AmmoInfo CBasePlayerItem::AmmoInfoArray[MAX_AMMO_SLOTS] = {};
static int giAmmoIndex = 0;

struct TYPEDESCRIPTION {
    int fieldType;
    const char* fieldName;
    int fieldOffset;
    short fieldSize;
    short flags;
};

static int gSizes[4] = { 4, 4, 4, 4 };

struct SAVERESTOREDATA {
    char* pBaseData;
    char* pCurrentData;
    int size;
    int bufferSize;
    int tokenSize;
    int tokenCount;
    char** pTokens;
};

struct HEADER {
    unsigned short size;
    unsigned short token;
    char* pData;
};

struct CRestore {
    SAVERESTOREDATA* m_pdata = nullptr;
    int m_global = 0;
    int readFieldCalls = 0;
    int headerReads = 0;

    int Empty() const { return !m_pdata || m_pdata->size >= m_pdata->bufferSize; }

    void BufferReadBytes(char* pOutput, int size) {
        if (!m_pdata || Empty()) {
            memset(pOutput, 0, (size_t)size);
            return;
        }
        int avail = m_pdata->bufferSize - m_pdata->size;
        if (size > avail) {
            memset(pOutput, 0, (size_t)size);
            m_pdata->size = m_pdata->bufferSize;
            m_pdata->pCurrentData = m_pdata->pBaseData + m_pdata->bufferSize;
            return;
        }
        memcpy(pOutput, m_pdata->pCurrentData, (size_t)size);
        m_pdata->pCurrentData += size;
        m_pdata->size += size;
    }

    short ReadShort() {
        short v = 0;
        BufferReadBytes(reinterpret_cast<char*>(&v), sizeof(short));
        return v;
    }

    int ReadInt() {
        int v = 0;
        BufferReadBytes(reinterpret_cast<char*>(&v), sizeof(int));
        return v;
    }

    void BufferRewind(int size) {
        if (!m_pdata) return;
        if (m_pdata->size < size) size = m_pdata->size;
        m_pdata->pCurrentData -= size;
        m_pdata->size -= size;
    }

    char* BufferPointer() { return m_pdata ? m_pdata->pCurrentData : nullptr; }

    void BufferSkipBytes(int bytes) {
        if (!m_pdata) return;
        int avail = m_pdata->bufferSize - m_pdata->size;
        if (bytes > avail) bytes = avail;
        m_pdata->pCurrentData += bytes;
        m_pdata->size += bytes;
    }

    void BufferReadHeader(HEADER* h) {
        ++headerReads;
        h->size = (unsigned short)ReadShort();
        h->token = (unsigned short)ReadShort();
        h->pData = BufferPointer();
        BufferSkipBytes(h->size);
    }

    unsigned short TokenHash(const char* name) {
        if (!m_pdata || !m_pdata->pTokens) return 0;
        for (int i = 0; i < m_pdata->tokenCount; ++i) {
            if (m_pdata->pTokens[i] && strcmp(m_pdata->pTokens[i], name) == 0)
                return (unsigned short)i;
        }
        return 0xFFFF;
    }

    int ReadField(void*, TYPEDESCRIPTION*, int, int startField, int, char*, void*) {
        ++readFieldCalls;
        return startField;
    }

    int ReadFields(const char* pname, void* pBaseData, TYPEDESCRIPTION* pFields, int fieldCount);
};

__EXTRACTED__

int main() {
    // 1. AddAmmoNameToAmmoRegistry stops at MAX_AMMO_SLOTS - 1 and never wraps to slot 0
    {
        memset(CBasePlayerItem::AmmoInfoArray, 0, sizeof(CBasePlayerItem::AmmoInfoArray));
        giAmmoIndex = 0;
        g_alertErrors = 0;

        static char names[40][16];
        for (int i = 1; i < MAX_AMMO_SLOTS; ++i) {
            snprintf(names[i], sizeof(names[i]), "ammo_%d", i);
            AddAmmoNameToAmmoRegistry(names[i]);
            assert(giAmmoIndex == i);
            assert(CBasePlayerItem::AmmoInfoArray[i].pszName == names[i]);
            assert(CBasePlayerItem::AmmoInfoArray[i].iId == i);
        }
        assert(CBasePlayerItem::AmmoInfoArray[0].pszName == nullptr);
        assert(g_alertErrors == 0);

        // Overflow attempt must NOT wrap to slot 0 or overwrite slot 1
        snprintf(names[35], sizeof(names[35]), "ammo_overflow");
        AddAmmoNameToAmmoRegistry(names[35]);
        assert(giAmmoIndex == MAX_AMMO_SLOTS - 1);
        assert(CBasePlayerItem::AmmoInfoArray[0].pszName == nullptr);
        assert(CBasePlayerItem::AmmoInfoArray[1].pszName == names[1]);
        assert(g_alertErrors == 1);
    }

    // 2. CRestore::ReadFields null pdata / null pTokens guard
    {
        int dummyObj = 42;
        TYPEDESCRIPTION desc = { 0, "x", 0, 1, 0 };
        CRestore r;
        r.m_pdata = nullptr;
        assert(r.ReadFields("Ent", &dummyObj, &desc, 1) == 0);

        SAVERESTOREDATA sd = {};
        sd.tokenCount = 2;
        sd.pTokens = nullptr;
        r.m_pdata = &sd;
        assert(r.ReadFields("Ent", &dummyObj, &desc, 1) == 0);
    }

    // 3. CRestore::ReadFields breaks immediately on Empty() for huge bogus fileCount and skips invalid tokens
    {
        std::vector<unsigned char> buf;
        auto pushU16 = [&](unsigned short v) {
            const unsigned char* p = reinterpret_cast<const unsigned char*>(&v);
            buf.insert(buf.end(), p, p + 2);
        };
        auto pushI32 = [&](int v) {
            const unsigned char* p = reinterpret_cast<const unsigned char*>(&v);
            buf.insert(buf.end(), p, p + 4);
        };

        // Group header: size=4, token=0 ("Ent"), fileCount=1000000
        pushU16(4);
        pushU16(0);
        pushI32(1000000);
        // Field 1: out-of-range token (99)
        pushU16(4);
        pushU16(99);
        pushI32(123);
        // Field 2: null token slot (1)
        pushU16(4);
        pushU16(1);
        pushI32(456);
        // Field 3: valid token (2 -> "x")
        pushU16(4);
        pushU16(2);
        pushI32(789);
        // Buffer ends here while fileCount claims 1,000,000 fields

        char tok0[] = "Ent";
        char tok2[] = "x";
        char* tokens[3] = { tok0, nullptr, tok2 };

        SAVERESTOREDATA sd = {};
        sd.pBaseData = reinterpret_cast<char*>(buf.data());
        sd.pCurrentData = sd.pBaseData;
        sd.size = 0;
        sd.bufferSize = (int)buf.size();
        sd.tokenCount = 3;
        sd.pTokens = tokens;

        int dummyObj = 42;
        TYPEDESCRIPTION desc = { 0, "x", 0, 1, 0 };
        CRestore r;
        r.m_pdata = &sd;
        assert(r.ReadFields("Ent", &dummyObj, &desc, 1) == 1);
        assert(r.headerReads == 3);
        assert(r.readFieldCalls == 1);
    }

    return 0;
}
'''


def main():
    for mod, weapons_rel, util_rel in MODS:
        weapons_src = (ROOT / weapons_rel).read_text(encoding='latin-1')
        util_src = (ROOT / util_rel).read_text(encoding='latin-1')
        extracted = '\n\n'.join([
            extract_function(weapons_src, 'void AddAmmoNameToAmmoRegistry('),
            extract_function(util_src, 'int CRestore::ReadFields('),
        ])
        code = HARNESS.replace('__EXTRACTED__', extracted)
        with tempfile.TemporaryDirectory(prefix=f'goldsrc-mod-save-weapon-{mod}-') as folder:
            folder = Path(folder)
            test_cpp = folder / 'test.cpp'
            test_cpp.write_text(code, encoding='utf-8')
            exe = compile_program([test_cpp], folder / 'test', sanitizers=['address', 'undefined'])
            subprocess.run([str(exe)], check=True, cwd=folder)
        print(f'PASS: {mod} AddAmmoNameToAmmoRegistry & CRestore::ReadFields')


if __name__ == '__main__':
    main()
