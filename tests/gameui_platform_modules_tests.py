"""Exercise the production GameUI LoadPlatformModules with tracked Sys_LoadModule and Sys_UnloadModule.

Run: python tests/gameui_platform_modules_tests.py
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
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#ifdef _WIN32
#define stricmp _stricmp
#define MODULE_PATH(name) name ".dll"
#else
#include <strings.h>
#define stricmp strcasecmp
#define MODULE_PATH(name) name "_linux.so"
#endif

typedef void* (*CreateInterfaceFn)(const char* name, int* returnCode);
struct CSysModule;
struct IVGuiModule { int unused; };

// only the members LoadPlatformModules uses
class KeyValues {
public:
 explicit KeyValues(const char* name) : name(name) {}
 bool LoadFromFile(void*, const char*, const char*);
 void deleteThis() { for (KeyValues* key : keys) key->deleteThis(); delete this; }
 KeyValues* FindKey(const char* key, bool create) {
  for (KeyValues* sub : keys) if (sub->name == key) return sub;
  if (!create) return nullptr;
  keys.push_back(new KeyValues(key));
  return keys.back();
 }
 KeyValues* GetFirstSubKey() { return keys.empty() ? nullptr : keys.front(); }
 KeyValues* GetNextKey() {
  for (size_t i = 0; i + 1 < parent->keys.size(); i++) if (parent->keys[i] == this) return parent->keys[i + 1];
  return nullptr;
 }
 int GetInt(const char* key, int fallback) { auto value = values.find(key); return value == values.end() ? fallback : atoi(value->second.c_str()); }
 const char* GetString(const char* key) { auto value = values.find(key); return value == values.end() ? "" : value->second.c_str(); }
 KeyValues* AddEntry(const char* dll, const char* interfaceName) {
  KeyValues* entry = new KeyValues("entry");
  entry->parent = this;
  entry->values["dll"] = dll;
  entry->values["interface"] = interfaceName;
  keys.push_back(entry);
  return entry;
 }
 std::string name;
 KeyValues* parent = nullptr;
 std::vector<KeyValues*> keys;
 std::map<std::string, std::string> values;
};

// a fake module either exports CreateInterface or not, and either exposes the requested interface or not
struct FakeModule { bool exportsFactory; bool exposesInterface; int references; };
static std::map<std::string, FakeModule> modules;
static std::vector<std::pair<const char*, const char*>> menuEntries;
static IVGuiModule g_moduleInterface;
static std::string g_requestedModule;
static int g_errors = 0;
static int g_initializeCalls = 0;

bool KeyValues::LoadFromFile(void*, const char*, const char*) {
 KeyValues* menu = FindKey("Menu", true);
 for (auto& entry : menuEntries) menu->AddEntry(entry.first, entry.second);
 return true;
}

static void* FakeCreateInterface(const char* name, int*) {
 return modules[g_requestedModule].exposesInterface && !strcmp(name, "VGuiModuleTest001") ? &g_moduleInterface : nullptr;
}

static CSysModule* Sys_LoadModule(const char* path) {
 auto module = modules.find(path);
 if (module == modules.end()) return nullptr;
 module->second.references++;
 return (CSysModule*)&module->second;
}

static CreateInterfaceFn Sys_GetFactory(CSysModule* module) {
 for (auto& entry : modules) {
  if ((CSysModule*)&entry.second != module) continue;
  g_requestedModule = entry.first;
  return entry.second.exportsFactory ? FakeCreateInterface : nullptr;
 }
 return nullptr;
}

static void Sys_UnloadModule(CSysModule* module) { ((FakeModule*)module)->references--; }

namespace vgui2 {
struct IFileSystem {
 bool GetLocalPath(const char* name, char* path, int size) {
  if (!modules.count(name)) return false;
  snprintf(path, size, "%s", name);
  return true;
 }
 void GetLocalCopy(const char*) {}
};
struct IVGui { void DPrintf2(const char*, ...) { g_errors++; } };
static IFileSystem g_fileSystem;
static IVGui g_vgui;
IFileSystem* filesystem() { return &g_fileSystem; }
IVGui* ivgui() { return &g_vgui; }
}

// only the members LoadPlatformModules uses
class CVGuiSystemModuleLoader {
public:
 bool LoadPlatformModules(CreateInterfaceFn* factories, int count, bool useSteamModules);
 bool InitializeAllModules(CreateInterfaceFn*, int) { g_initializeCalls++; return true; }
 struct module_t { CSysModule* module; IVGuiModule* moduleInterface; KeyValues* data; };
 struct Modules : std::vector<module_t> {
  int AddToTail() { push_back(module_t()); return (int)size() - 1; }
  int Count() { return (int)size(); }
 } m_Modules;
 KeyValues* m_pPlatformModuleData = nullptr;
};
'''

TESTS = r'''
static bool Load(CVGuiSystemModuleLoader& loader) {
 g_errors = 0;
 g_initializeCalls = 0;
 return loader.LoadPlatformModules(nullptr, 0, true);
}

int main() {
 modules = {{MODULE_PATH("platform/noexport"), {false, false, 0}},
            {MODULE_PATH("platform/oldversion"), {true, false, 0}},
            {MODULE_PATH("platform/valid"), {true, true, 0}}};

 // a loadable module without CreateInterface is reported and released instead of calling a NULL factory
 menuEntries = {{"platform/noexport.dll", "VGuiModuleTest001"}};
 CVGuiSystemModuleLoader noExport;
 assert(!Load(noExport));
 assert(1 == g_errors && 0 == noExport.m_Modules.Count());
 assert(0 == modules[MODULE_PATH("platform/noexport")].references);
 noExport.m_pPlatformModuleData->deleteThis();
 puts("PASS: a platform module without CreateInterface is reported and unloaded");

 // a module that does not expose the requested interface version is released too
 menuEntries = {{"platform/oldversion.dll", "VGuiModuleTest001"}};
 CVGuiSystemModuleLoader oldVersion;
 assert(!Load(oldVersion));
 assert(1 == g_errors && 0 == oldVersion.m_Modules.Count());
 assert(0 == modules[MODULE_PATH("platform/oldversion")].references);
 oldVersion.m_pPlatformModuleData->deleteThis();
 puts("PASS: a platform module with an invalid interface version is reported and unloaded");

 // a missing dll is reported without loading anything
 menuEntries = {{"platform/missing.dll", "VGuiModuleTest001"}};
 CVGuiSystemModuleLoader missing;
 assert(!Load(missing));
 assert(1 == g_errors && 0 == missing.m_Modules.Count());
 missing.m_pPlatformModuleData->deleteThis();
 puts("PASS: a missing platform module is reported");

 // a valid module stays loaded and is initialized, also after the failing entries
 menuEntries = {{"platform/noexport.dll", "VGuiModuleTest001"}, {"platform/oldversion.dll", "VGuiModuleTest001"},
                {"platform/missing.dll", "VGuiModuleTest001"}, {"platform/valid.dll", "VGuiModuleTest001"}};
 CVGuiSystemModuleLoader mixed;
 assert(!Load(mixed));
 assert(3 == g_errors && 1 == g_initializeCalls);
 assert(1 == mixed.m_Modules.Count() && &g_moduleInterface == mixed.m_Modules[0].moduleInterface);
 assert(0 == modules[MODULE_PATH("platform/noexport")].references && 0 == modules[MODULE_PATH("platform/oldversion")].references);
 assert(1 == modules[MODULE_PATH("platform/valid")].references);
 mixed.m_pPlatformModuleData->deleteThis();

 menuEntries = {{"platform/valid.dll", "VGuiModuleTest001"}};
 modules[MODULE_PATH("platform/valid")].references = 0;
 CVGuiSystemModuleLoader valid;
 assert(Load(valid));
 assert(0 == g_errors && 1 == g_initializeCalls && 1 == valid.m_Modules.Count());
 assert(1 == modules[MODULE_PATH("platform/valid")].references);
 valid.m_pPlatformModuleData->deleteThis();
 puts("PASS: a valid platform module stays loaded and is initialized");
}
'''


def main():
    source = (ROOT / 'GameUI/VGuiSystemModuleLoader.cpp').read_text(encoding='latin-1')
    code = STUBS + function(source, 'bool CVGuiSystemModuleLoader::LoadPlatformModules(') + '\n' + TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-gameui-platform-modules-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
