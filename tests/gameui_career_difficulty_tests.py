"""Exercise the production Career difficulty definition release with tracking allocation operators.

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
class BotProfileManager {};
class CCareerCharacter {};
class CCareerMap {};
typedef std::vector<int> CharCostVector;
typedef std::vector<CCareerCharacter*> CharacterVec;
typedef std::vector<std::vector<CCareerMap*> > MapsVec;
'''

TESTS = r'''
// Init stores the bot profile path the same way
static void Load(CareerDifficultyDefinition* definition) {
 definition->Reset();
 definition->m_botManager = new BotProfileManager;
 definition->m_botFilename = CloneString("MissionPacks/Test/BotCampaignProfile.db");
 definition->m_availableCharacters.push_back(new CCareerCharacter);
 definition->m_maps.push_back(std::vector<CCareerMap*>(1, new CCareerMap));
}

static void ResetFreesTheBotFilenameAsAnArray() {
 CareerDifficultyDefinition definition;
 Load(&definition);
 definition.Reset();
 assert(0 == mismatches);
 assert(!definition.GetBotProfileDBFilename() && !definition.m_botManager && !definition.m_vdfFilename);
 definition.Reset();
 assert(0 == mismatches);
 puts("PASS: Reset frees the cloned bot filename with delete[] and can run again");
}

static void DestructorFreesTheBotFilenameAsAnArray() {
 {
  CareerDifficultyDefinition definitions[3];
  for (CareerDifficultyDefinition& definition : definitions) Load(&definition);
  Load(&definitions[0]);
 }
 assert(0 == mismatches);
 for (void* p : arrays) assert(!p);
 puts("PASS: destroying loaded difficulty definitions frees every cloned bot filename with delete[]");
}
'''


def main():
    definitions = (ROOT / 'GameUI/CareerDefinitions.cpp').read_text(encoding='utf-8')
    header = (ROOT / 'GameUI/CareerDefinitions.h').read_text(encoding='utf-8')
    declaration = header[header.index('class CareerDifficultyDefinition'):header.index('};', header.index('class CareerDifficultyDefinition')) + 2]
    code = STUBS
    # the tests load the private members the way Init does
    code += declaration.replace('private:', 'public:') + '\n'
    for signature in ['CareerDifficultyDefinition::CareerDifficultyDefinition(',
                      'CareerDifficultyDefinition::~CareerDifficultyDefinition(',
                      'void CareerDifficultyDefinition::Reset(']:
        code += function(definitions, signature) + '\n'
    code += TESTS
    code += '\nint main() { ResetFreesTheBotFilenameAsAnArray(); DestructorFreesTheBotFilenameAsAnArray(); }\n'
    with tempfile.TemporaryDirectory(prefix='goldsrc-gameui-career-difficulty-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([path / 'test.cpp'], path / 'test', includes=[ROOT / 'game_shared'])
        subprocess.run([str(executable)], check=True, cwd=path)


if __name__ == '__main__':
    main()
