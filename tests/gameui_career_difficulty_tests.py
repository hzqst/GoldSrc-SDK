"""Exercise the production Career mission pack and difficulty definition load and release with stubs and tracking allocation operators.

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
#include <cstdarg>
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
#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#endif
char* SharedVarArgs(const char* format, ...) {
 static char text[CAREER_MAX_PATH];
 va_list args;
 va_start(args, format);
 vsnprintf(text, sizeof(text), format, args);
 va_end(args);
 return text;
}
class Color {
public:
 Color() {}
 Color(int r, int g, int b, int a) { m_color[0] = r; m_color[1] = g; m_color[2] = b; m_color[3] = a; }
 int GetRawColor() const { return m_color[0] | m_color[1] << 8 | m_color[2] << 16 | m_color[3] << 24; }
 unsigned char& operator[](int index) { return m_color[index]; }
private:
 unsigned char m_color[4] = {};
};
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
// every existing overview names its title and the Bots.db profile; every existing difficulty file
// has 2 characters, costs 5 and 3, and one group of 3 maps with a bot each
std::string overviewTitle;
class KeyValues {
public:
 explicit KeyValues(const char* name, const char* value = "") : m_name(name), m_value(value), m_wvalue(m_value.begin(), m_value.end()) {}
 ~KeyValues() { for (KeyValues* key : m_keys) delete key; }
 bool LoadFromFile(vgui2::FileSystem*, const char* path, const char*) {
  if (!files.count(path)) return false;
  if (strstr(path, "overview.vdf")) {
   Add(new KeyValues("Title", overviewTitle.c_str()));
   Add(new KeyValues("Author", "Author"));
   Add(new KeyValues("BotProfile", "Bots.db"));
   return true;
  }
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
 const wchar_t* GetWString(const char* name, const wchar_t* value) { KeyValues* key = FindKey(name, false); return key ? key->m_wvalue.c_str() : value; }
 Color GetColor(const char*) { return Color(); }
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
 std::wstring m_wvalue;
 KeyValues* m_parent = nullptr;
 std::vector<KeyValues*> m_keys;
};
'''

TESTS = r'''
// the characters of the file and the bot of each of its 3 maps
static const int LOADED_CHARACTERS = 2 + 3;
static const size_t LOADED_COSTS = 3;

static void UseFiles(bool vdf) {
 files = {"MissionPacks/Test/Bots.db", "MissionPacks/Test/overview.vdf"};
 if (vdf) files.insert("MissionPacks/Test/Easy.vdf");
 overviewTitle = "First";
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

static void MissionPackInitAgainReplacesThePreviousLoad() {
 UseFiles(true);
 {
  CareerDefinition definition;
  assert(definition.Init("Test"));
  const int loadedArrays = LiveArrays();
  overviewTitle = "Second";
  assert(definition.Init("Test"));
  assert(loadedArrays == LiveArrays() && MAX_CAREER_DIFFICULTY == liveManagers);
  assert(0 == wcscmp(L"Second", definition.GetTitle()) && 0 == strcmp("Second", definition.GetAsciiTitle()));
  assert(0 == strcmp("MissionPacks/Test/Save", definition.GetSaveDirectory()));
 }
 AssertReleased();
 puts("PASS: mission pack Init without Reset frees the previous strings and difficulties");
}

static void MissingOverviewDropsThePreviousMissionPack() {
 UseFiles(true);
 {
  CareerDefinition definition;
  assert(definition.Init("Test"));
  files.erase("MissionPacks/Test/overview.vdf");
  assert(!definition.Init("Test"));
  assert(!definition.GetTitle() && !definition.GetAsciiTitle() && !definition.GetAuthor() && !definition.GetBotFilename());
  assert(0 == liveManagers && 0 == liveCharacters && 0 == liveMaps);
 }
 AssertReleased();
 puts("PASS: mission pack Init that cannot load the overview keeps nothing from the previous load");
}

static void MissingBotProfileDropsThePreviousBotFilename() {
 UseFiles(true);
 {
  CareerDefinition definition;
  assert(definition.Init("Test"));
  assert(0 == strcmp("Bots.db", definition.GetBotFilename()));
  files.erase("MissionPacks/Test/Bots.db");
  assert(definition.Init("Test"));
  assert(!definition.GetBotFilename());
 }
 AssertReleased();
 puts("PASS: mission pack Init without the bot profile drops the previous bot filename");
}
'''


def main():
    definitions = (ROOT / 'GameUI/CareerDefinitions.cpp').read_text(encoding='utf-8')
    header = (ROOT / 'GameUI/CareerDefinitions.h').read_text(encoding='utf-8')
    career = (ROOT / 'common/GameUI/ICareerUI.h').read_text(encoding='utf-8')
    code = STUBS
    code += re.search(r'enum CareerDifficultyType\n\{[^}]*\};', career).group(0) + '\n'
    for name in ['class CareerDifficultyDefinition\n', 'class CareerDefinition\n']:
        start = header.index(name)
        code += header[start:header.index('\n};', start) + 3] + '\n'
    for signature in ['CareerDifficultyDefinition::CareerDifficultyDefinition(',
                      'CareerDifficultyDefinition::~CareerDifficultyDefinition(',
                      'void CareerDifficultyDefinition::Reset(',
                      'void CareerDifficultyDefinition::Init(',
                      'CareerDefinition::CareerDefinition(',
                      'void CareerDefinition::Reset(',
                      'CareerDefinition::~CareerDefinition(',
                      'bool CareerDefinition::Init(']:
        code += function(definitions, signature) + '\n'
    code += TESTS
    code += ('\nint main() { ResetFreesTheBotFilenameAsAnArray(); InitAgainReplacesThePreviousLoad(); '
             'ReloadKeepsOneCostList(); MissingFileDropsThePreviousLoad(); DestructorFreesEveryLoad(); '
             'MissionPackInitAgainReplacesThePreviousLoad(); MissingOverviewDropsThePreviousMissionPack(); '
             'MissingBotProfileDropsThePreviousBotFilename(); }\n')
    with tempfile.TemporaryDirectory(prefix='goldsrc-gameui-career-difficulty-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([path / 'test.cpp', ROOT / 'common/StringUtils.cpp'], path / 'test',
                                     includes=[ROOT / 'game_shared', ROOT / 'common'])
        subprocess.run([str(executable)], check=True, cwd=path)


if __name__ == '__main__':
    main()
