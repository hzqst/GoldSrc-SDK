"""Exercise the production Career difficulty definition load and release with stubs and tracking allocation operators.

Run: python tests/gameui_career_difficulty_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'


def function(source, signature):
    start = re.search(re.escape(signature) + r'[^\n;]*\n\{', source).start()
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <new>
#include <set>
#include <string>
#include <vector>
// new[] blocks are remembered so a scalar delete of one is caught without a sanitizer
static const int MAX_ARRAYS = 64;
static void* arrays[MAX_ARRAYS];
static int mismatches = 0;
static bool ForgetArray(void* p) {
 for (int i = 0; i < MAX_ARRAYS; ++i)
  if (p && arrays[i] == p) { arrays[i] = nullptr; return true; }
 return false;
}
static int LiveArrays() {
 int count = 0;
 for (void* p : arrays) count += p != nullptr;
 return count;
}
static void* Allocate(size_t size) {
 void* p = malloc(size ? size : 1);
 if (!p) throw std::bad_alloc();
 return p;
}
void* operator new(size_t size) { return Allocate(size); }
void* operator new[](size_t size) {
 void* p = Allocate(size);
 for (int i = 0; i < MAX_ARRAYS; ++i)
  if (!arrays[i]) { arrays[i] = p; return p; }
 abort();
}
void operator delete(void* p) noexcept { if (ForgetArray(p)) ++mismatches; free(p); }
void operator delete(void* p, size_t) noexcept { operator delete(p); }
void operator delete[](void* p) noexcept { if (p && !ForgetArray(p)) ++mismatches; free(p); }
void operator delete[](void* p, size_t) noexcept { operator delete[](p); }
#include "shared_util.h"
#include "StringUtils.h"
#define CAREER_MAX_PATH 260
// maps have no tasks, so the task parser is never asked for a token
void SharedSetQuoteChar(char) {}
bool SharedTokenWaiting(const char* buffer) { return buffer && *buffer; }
const char* SharedParse(const char*) { abort(); }
char* SharedGetToken() { abort(); }
static int liveManagers = 0, liveCharacters = 0, liveMaps = 0;
class BotProfileManager {
public:
 BotProfileManager() { ++liveManagers; }
 ~BotProfileManager() { --liveManagers; }
 void Init(const char*, unsigned int*) {}
};
class CCareerCharacter {
public:
 CCareerCharacter() { ++liveCharacters; }
 ~CCareerCharacter() { --liveCharacters; }
};
typedef std::vector<int> CharCostVector;
typedef std::vector<CCareerCharacter*> CharacterVec;
typedef std::vector<std::vector<class CCareerMap*> > MapsVec;
class CCareerUITask { public: explicit CCareerUITask(const char*) {} };
typedef std::vector<CCareerUITask*> ITaskVec;
class CCareerMap {
public:
 CCareerMap(const char*, CharacterVec bots, bool) : m_bots(bots) { ++liveMaps; }
 ~CCareerMap() { for (CCareerCharacter* bot : m_bots) delete bot; --liveMaps; }
 void SetTasks(ITaskVec& tasks) { m_tasks = tasks; }
 ITaskVec* GetTasks() { return &m_tasks; }
 void SetMinEnemyCount(int) {}
 void SetThreshold(int) {}
 void SetTeammatesAllowed(bool) {}
 void SetExtraInitialization(const char*) {}
 CharacterVec m_bots;
 ITaskVec m_tasks;
};
static bool IsSendableCareerMap(const char*, ITaskVec&) { return true; }
// one character per space separated name
static CharacterVec ParseProfilesFromString(BotProfileManager*, const char* names) {
 CharacterVec characters;
 for (const char* name = names; *name; ++name)
  if (*name != ' ' && (name == names || name[-1] == ' ')) characters.push_back(new CCareerCharacter);
 return characters;
}
std::set<std::string> files;
namespace vgui2 {
struct FileSystem { bool FileExists(const char* path) { return files.count(path) != 0; } } fileSystemStub;
FileSystem* filesystem() { return &fileSystemStub; }
}
// every existing difficulty file has 2 characters, costs 5 and 3, and one group of 3 maps with a bot each
class KeyValues {
public:
 explicit KeyValues(const char* name, const char* value = "") : m_name(name), m_value(value) {}
 ~KeyValues() { for (KeyValues* key : m_keys) delete key; }
 bool LoadFromFile(vgui2::FileSystem*, const char* path, const char*) {
  if (!files.count(path)) return false;
  Add(new KeyValues("Characters", "Alpha Bravo"));
  KeyValues* costs = Add(new KeyValues("CostAvailability"));
  costs->Add(new KeyValues("1", "5"));
  costs->Add(new KeyValues("2", "3"));
  KeyValues* maps = Add(new KeyValues("Maps"));
  for (const char* map : {"cs_one", "de_two", "de_three"})
   maps->Add(new KeyValues(map))->Add(new KeyValues("bots", "Charlie"));
  return true;
 }
 void deleteThis() { delete this; }
 const char* GetName() { return m_name.c_str(); }
 KeyValues* FindKey(const char* name, bool) {
  for (KeyValues* key : m_keys)
   if (key->m_name == name) return key;
  return nullptr;
 }
 int GetInt(const char* name, int value) { KeyValues* key = FindKey(name, false); return key ? atoi(key->m_value.c_str()) : value; }
 const char* GetString(const char* name, const char* value) { KeyValues* key = FindKey(name, false); return key ? key->m_value.c_str() : value; }
 KeyValues* GetFirstSubKey() { return m_keys.empty() ? nullptr : m_keys[0]; }
 KeyValues* GetNextKey() {
  if (!m_parent) return nullptr;
  std::vector<KeyValues*>& keys = m_parent->m_keys;
  for (size_t i = 0; i + 1 < keys.size(); ++i)
   if (keys[i] == this) return keys[i + 1];
  return nullptr;
 }
private:
 KeyValues* Add(KeyValues* key) { key->m_parent = this; m_keys.push_back(key); return key; }
 std::string m_name, m_value;
 KeyValues* m_parent = nullptr;
 std::vector<KeyValues*> m_keys;
};
'''

TESTS = r'''
// the characters of the file and the bot of each of its 3 maps
static const int LOADED_CHARACTERS = 2 + 3;
static const size_t LOADED_COSTS = 3;

static void UseFiles(bool vdf) {
 files = {"MissionPacks/Test/Bots.db"};
 if (vdf) files.insert("MissionPacks/Test/Easy.vdf");
}

static void AssertLoaded(CareerDifficultyDefinition* definition) {
 assert(1 == liveManagers && LOADED_CHARACTERS == liveCharacters && 3 == liveMaps && 1 == LiveArrays());
 assert(0 == strcmp("MissionPacks/Test/Bots.db", definition->GetBotProfileDBFilename()));
 assert(2 == definition->GetCharacters()->size() && 1 == definition->GetMaps()->size());
 assert(LOADED_COSTS == definition->GetCharacterCostAvailibility()->size());
 assert(5 == (*definition->GetCharacterCostAvailibility())[1]);
}

static void AssertReleased() {
 assert(0 == mismatches);
 assert(0 == liveManagers && 0 == liveCharacters && 0 == liveMaps && 0 == LiveArrays());
}

static void ResetFreesTheBotFilenameAsAnArray() {
 UseFiles(true);
 CareerDifficultyDefinition definition;
 definition.Init("Test", "Easy.vdf", "Bots.db");
 AssertLoaded(&definition);
 definition.Reset();
 AssertReleased();
 assert(!definition.GetBotProfileDBFilename() && definition.GetCharacterCostAvailibility()->empty());
 definition.Reset();
 AssertReleased();
 puts("PASS: Reset frees everything Init loaded, the bot filename with delete[], and can run again");
}

static void InitAgainReplacesThePreviousLoad() {
 UseFiles(true);
 {
  CareerDifficultyDefinition definition;
  definition.Init("Test", "Easy.vdf", "Bots.db");
  definition.Init("Test", "Easy.vdf", "Bots.db");
  AssertLoaded(&definition);
 }
 AssertReleased();
 puts("PASS: Init without Reset frees the previous bot manager, bot filename, characters, maps and costs");
}

static void ReloadKeepsOneCostList() {
 UseFiles(true);
 {
  CareerDifficultyDefinition definition;
  definition.Init("Test", "Easy.vdf", "Bots.db");
  definition.Reset();
  definition.Init("Test", "Easy.vdf", "Bots.db");
  AssertLoaded(&definition);
 }
 AssertReleased();
 puts("PASS: Reset then Init, as ReloadCareerDefinition does, keeps a single cost list");
}

static void MissingFileDropsThePreviousLoad() {
 UseFiles(true);
 {
  CareerDifficultyDefinition definition;
  definition.Init("Test", "Easy.vdf", "Bots.db");
  UseFiles(false);
  definition.Init("Test", "Easy.vdf", "Bots.db");
  assert(1 == liveManagers && 0 == liveCharacters && 0 == liveMaps && 1 == LiveArrays());
  assert(definition.GetCharacters()->empty() && definition.GetMaps()->empty());
  assert(definition.GetCharacterCostAvailibility()->empty());
 }
 AssertReleased();
 puts("PASS: Init that falls back to the defaults leaves nothing from the previous load");
}

static void DestructorFreesEveryLoad() {
 UseFiles(true);
 {
  CareerDifficultyDefinition definitions[3];
  for (CareerDifficultyDefinition& definition : definitions) definition.Init("Test", "Easy.vdf", "Bots.db");
  definitions[0].Init("Test", "Easy.vdf", "Bots.db");
 }
 AssertReleased();
 puts("PASS: destroying loaded difficulty definitions frees every load");
}
'''


def main():
    definitions = (ROOT / 'GameUI/CareerDefinitions.cpp').read_text(encoding='utf-8')
    header = (ROOT / 'GameUI/CareerDefinitions.h').read_text(encoding='utf-8')
    start = header.index('class CareerDifficultyDefinition')
    code = STUBS
    code += header[start:header.index('};', start) + 2] + '\n'
    for signature in ['CareerDifficultyDefinition::CareerDifficultyDefinition(',
                      'CareerDifficultyDefinition::~CareerDifficultyDefinition(',
                      'void CareerDifficultyDefinition::Reset(',
                      'void CareerDifficultyDefinition::Init(']:
        code += function(definitions, signature) + '\n'
    code += TESTS
    code += ('\nint main() { ResetFreesTheBotFilenameAsAnArray(); InitAgainReplacesThePreviousLoad(); '
             'ReloadKeepsOneCostList(); MissingFileDropsThePreviousLoad(); DestructorFreesEveryLoad(); }\n')
    with tempfile.TemporaryDirectory(prefix='goldsrc-gameui-career-difficulty-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([path / 'test.cpp', ROOT / 'common/StringUtils.cpp'], path / 'test',
                                     includes=[ROOT / 'game_shared', ROOT / 'common'])
        subprocess.run([str(executable)], check=True, cwd=path)


if __name__ == '__main__':
    main()
