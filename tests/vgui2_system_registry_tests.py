"""Exercise the production VGUI2 CSystem::SetRegistryString with a NULL value, which deletes the registry value, and with a string value, which writes it (#485).

Run: python tests/vgui2_system_registry_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

// only the Win32 registry calls the extracted functions make; each records what it was given
typedef void* HKEY;
typedef long LONG;
typedef unsigned long DWORD;
typedef unsigned char BYTE;
typedef DWORD REGSAM;
#define ERROR_SUCCESS 0L
#define ERROR_FILE_NOT_FOUND 2L
#define ERROR_ACCESS_DENIED 5L
#define REG_SZ 1
#define REG_OPTION_NON_VOLATILE 0
#define KEY_WRITE 0x20006
#define KEY_ALL_ACCESS 0xF003F
static char currentUserTag, localMachineTag, openedTag;
#define HKEY_CURRENT_USER ((HKEY)&currentUserTag)
#define HKEY_LOCAL_MACHINE ((HKEY)&localMachineTag)

static HKEY createdSlot;
static std::string createdKey, setName, setData, deletedName;
static REGSAM createdAccess;
static DWORD setType, setSize;
static LONG deleteResult;
static int sets, deletes, closes;

static LONG RegCreateKeyExA(HKEY slot, const char* key, DWORD, char*, DWORD, REGSAM access, void*, HKEY* result, DWORD*) {
 createdSlot = slot;
 createdKey = key;
 createdAccess = access;
 *result = (HKEY)&openedTag;
 return ERROR_SUCCESS;
}
static LONG RegSetValueExA(HKEY key, const char* name, DWORD, DWORD type, const BYTE* data, DWORD size) {
 assert((HKEY)&openedTag == key);
 ++sets;
 setName = name;
 setType = type;
 setSize = size;
 setData.assign((const char*)data, size);
 return ERROR_SUCCESS;
}
static LONG RegDeleteValueA(HKEY key, const char* name) {
 assert((HKEY)&openedTag == key);
 ++deletes;
 deletedName = name;
 return deleteResult;
}
static LONG RegCloseKey(HKEY key) {
 assert((HKEY)&openedTag == key);
 ++closes;
 return ERROR_SUCCESS;
}

static void vgui_strcpy(char* dst, int dstLen, const char* src) {
 strncpy(dst, src, dstLen - 1);
 dst[dstLen - 1] = 0;
}

namespace vgui2 {

// only the member the extracted function defines
class CSystem {
public:
 bool SetRegistryString(const char* key, const char* value);
};
'''

TESTS = r'''
}
using namespace vgui2;

static void Reset(LONG result) {
 createdSlot = nullptr;
 createdKey.clear();
 setName.clear();
 setData.clear();
 deletedName.clear();
 createdAccess = 0;
 setType = setSize = 0;
 deleteResult = result;
 sets = deletes = closes = 0;
}

static void NullValueDeletesTheValue() {
 Reset(ERROR_SUCCESS);
 CSystem system;
 assert(system.SetRegistryString("HKEY_LOCAL_MACHINE\\Software\\Valve\\Token", nullptr));
 assert(HKEY_LOCAL_MACHINE == createdSlot);
 // the split keeps the separator at the end of the parent key
 assert(std::string("Software\\Valve\\") == createdKey);
 assert((REGSAM)KEY_ALL_ACCESS == createdAccess);
 assert(1 == deletes);
 assert(std::string("Token") == deletedName);
 assert(0 == sets);
 assert(1 == closes);
 puts("PASS: CSystem::SetRegistryString with a NULL value deletes the value instead of calling strlen on it");
}

static void NullValueAlreadyGoneCountsAsDeleted() {
 Reset(ERROR_FILE_NOT_FOUND);
 CSystem system;
 assert(system.SetRegistryString("Software\\Valve\\Token", nullptr));
 assert(HKEY_CURRENT_USER == createdSlot);
 assert(1 == deletes);
 assert(1 == closes);
 puts("PASS: CSystem::SetRegistryString with a NULL value succeeds when the value does not exist");
}

static void NullValueDeleteFailureFails() {
 Reset(ERROR_ACCESS_DENIED);
 CSystem system;
 assert(!system.SetRegistryString("Software\\Valve\\Token", nullptr));
 assert(1 == deletes);
 assert(1 == closes);
 puts("PASS: CSystem::SetRegistryString with a NULL value fails and still closes the key when the delete fails");
}

static void StringValueWritesIt() {
 Reset(ERROR_SUCCESS);
 CSystem system;
 assert(system.SetRegistryString("Software\\Valve\\Token", "abc"));
 assert((REGSAM)KEY_WRITE == createdAccess);
 assert(1 == sets);
 assert(0 == deletes);
 assert(std::string("Token") == setName);
 assert((DWORD)REG_SZ == setType);
 assert((DWORD)4 == setSize);
 assert(std::string("abc", 4) == setData);
 assert(1 == closes);
 puts("PASS: CSystem::SetRegistryString writes a string value with its terminator");
}

int main() {
 NullValueDeletesTheValue();
 NullValueAlreadyGoneCountsAsDeleted();
 NullValueDeleteFailureFails();
 StringValueWritesIt();
}
'''


def main():
    source = (ROOT / 'vgui2/src/System.cpp').read_text(encoding='utf-8')
    signatures = ('static bool staticSplitRegistryKey( const char* key, char* key0, int key0Len, char* key1, int key1Len )',
                  'static HKEY staticRegistrySlot( const char*& key )',
                  'bool CSystem::SetRegistryString( const char* key, const char* value )')
    code = STUBS + ''.join(function(source, signature) + '\n' for signature in signatures) + TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-system-registry-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
