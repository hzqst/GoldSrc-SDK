"""Exercise the production new game and change game commands with stubs.

Run: python tests/gameui_start_game_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
The mod list scan uses the Win32 file API, so its case only runs on Windows.
"""
from pathlib import Path
import os
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


def defines(source, names):
    return ''.join(re.search(r'^#define\s+' + name + r'\b.*\n', source, re.M).group(0) for name in names)


STUBS = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#define stricmp _stricmp
#else
#include <strings.h>
#define stricmp strcasecmp
#endif
#include "StringUtils.h"

struct Engine {
 // const so that the string literals in OnCommand compile under /permissive-
 static int ClientCmd(const char* command) { commands.push_back(command); return 0; }
 static void ConPrintf(const char*, ...) { printed++; }
 int (*pfnClientCmd)(const char*) = ClientCmd;
 void (*Con_Printf)(const char*, ...) = ConPrintf;
 static std::vector<std::string> commands;
 static int printed;
};
std::vector<std::string> Engine::commands;
int Engine::printed = 0;
static Engine g_engine;
static Engine* engine = &g_engine;

struct KeyValues {
 explicit KeyValues(const char*) {}
 void SetString(const char* key, const char* value) { values[key] = value; }
 const char* GetString(const char* key, const char* defaultValue) {
  auto it = values.find(key);
  return it == values.end() ? defaultValue : it->second.c_str();
 }
 std::map<std::string, std::string> values;
};

// liblist.gam holds only the game description in these tests
class CModInfo {
public:
 void LoadGameInfoFromBuffer(const char* buffer, int size) { description.assign(buffer, size); }
 const char* GetGameDescription() { return description.c_str(); }
 std::string description = "Half-Life";
};
static CModInfo g_modInfo;
CModInfo* ModInfo() { return &g_modInfo; }

struct ListPanel {
 void AddItem(KeyValues* data, int, bool, bool) { items.push_back(data); }
 int GetSelectedItemsCount() { return selected < 0 ? 0 : 1; }
 int GetSelectedItem(int) { return selected; }
 KeyValues* GetItem(int index) { return items[index]; }
 std::vector<KeyValues*> items;
 int selected = -1;
};

struct Frame { void OnCommand(const char*) {} };
class CChangeGameDialog : public Frame {
 typedef Frame BaseClass;
public:
 void LoadModList();
 void OnCommand(const char* command);
 void Close() {}
 ListPanel list, *m_pModList = &list;
};
'''

TESTS = r'''
static void NewGameMapsThatCouldAddCommandsDoNotStart() {
 char buffer[512];
 assert(FormatNewGameCommand(buffer, sizeof(buffer), "c1a0", 2));
 assert(std::string("disconnect\nmaxplayers 1\ndeathmatch 0\nskill 2\nmap c1a0\n") == buffer);
 assert(FormatNewGameCommand(buffer, sizeof(buffer), "t0a0", NEWGAME_NO_SKILL));
 assert(std::string("disconnect\nmaxplayers 1\ndeathmatch 0\nmap t0a0\n") == buffer);
 Engine::printed = 0;
 for (const char* map : {"c1a0;quit", "c1a0\nquit", "c1a0\rquit", "c1\"a0"})
  assert(!FormatNewGameCommand(buffer, sizeof(buffer), map, 1));
 assert(4 == Engine::printed);
 const std::string longMap(600, 'm');
 assert(!FormatNewGameCommand(buffer, sizeof(buffer), longMap.c_str(), NEWGAME_NO_SKILL));
 puts("PASS: new game maps from liblist.gam that could add commands or do not fit do not start");
}

static void ModDirectoriesThatCouldAddCommandsAreNotSent() {
 for (const char* dir : {"cstrike", "a;quit", "b\"c", "d\ne"}) {
  CChangeGameDialog dialog;
  KeyValues item("Mod");
  item.SetString("ModDir", dir);
  dialog.list.items.push_back(&item);
  dialog.list.selected = 0;
  Engine::commands.clear();
  dialog.OnCommand("OK");
  if (!strcmp(dir, "cstrike")) {
   assert(2 == Engine::commands.size());
   assert(std::string("_setgamedir cstrike\n") == Engine::commands[0]);
  } else {
   assert(Engine::commands.empty());
  }
 }
 puts("PASS: mod directories that could add commands are not sent to _setgamedir");
}

static void ModDirectoriesThatCouldAddCommandsAreNotListed() {
#ifdef _WIN32
 // the test runs in a folder holding cstrike, a;quit and valve, each with a liblist.gam
 CChangeGameDialog dialog;
 dialog.LoadModList();
 assert(1 == dialog.list.items.size());
 assert(std::string("cstrike") == dialog.list.items[0]->GetString("ModDir", ""));
 puts("PASS: mod directories that could add commands are not listed");
#endif
}
'''


def main():
    new_game = (ROOT / 'GameUI/NewGameDialog.cpp').read_text(encoding='utf-8')
    change_game = (ROOT / 'GameUI/ChangeGameDialog.cpp').read_text(encoding='utf-8')
    code = STUBS + defines(new_game, ['NEWGAME_NO_SKILL']) + function(new_game, 'static bool FormatNewGameCommand(')
    code += function(change_game, 'void CChangeGameDialog::LoadModList(') + function(change_game, 'void CChangeGameDialog::OnCommand(')
    code += TESTS
    code += ('\nint main() { NewGameMapsThatCouldAddCommandsDoNotStart(); ModDirectoriesThatCouldAddCommandsAreNotSent(); '
             'ModDirectoriesThatCouldAddCommandsAreNotListed(); }\n')
    with tempfile.TemporaryDirectory(prefix='goldsrc-gameui-start-game-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code, encoding='utf-8')
        games = path / 'games'
        if os.name == 'nt':
            for directory, description in (('cstrike', 'Counter-Strike'), ('a;quit', 'Injected'), ('valve', 'Half-Life')):
                (games / directory).mkdir(parents=True)
                (games / directory / 'liblist.gam').write_bytes(description.encode('ascii'))
        else:
            games.mkdir()
        executable = compile_program([path / 'test.cpp', ROOT / 'common/StringUtils.cpp'], path / 'test',
                                     includes=[ROOT / 'common'], sanitizers=['address', 'undefined'],
                                     defines=['_CRT_SECURE_NO_WARNINGS', '_CRT_NONSTDC_NO_DEPRECATE'])
        subprocess.run([str(executable)], check=True, cwd=games)


if __name__ == '__main__':
    main()
