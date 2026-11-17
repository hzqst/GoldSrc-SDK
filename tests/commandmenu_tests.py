"""Run the production menu parser with UI stubs and optional installed game menus.

python tests/commandmenu_tests.py --cstrike-menu <path> --dod-menu <path>
"""
import argparse
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'

STUBS = r'''
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "game_controls/CreateCommandMenu.h"
#if defined(_WIN32)
#define _snprintf snprintf
#else
#include <strings.h>
#define _strnicmp strncasecmp
#define _snprintf snprintf
#endif
#define ASSERT_AT(condition, line) assert(condition)
namespace vgui2 {
struct Panel { virtual ~Panel() = default; };
struct Menu;
struct MenuItem {
 std::string text, command; Menu* submenu = nullptr; bool checked = false;
 void SetChecked(bool state) { checked = state; }
};
int liveMenus = 0;
struct Menu : Panel {
 std::vector<MenuItem> items;
 Menu(Panel*, const char*) { ++liveMenus; }
 ~Menu() { for (auto& item : items) delete item.submenu; --liveMenus; }
 void SetVisible(bool) {}
 void AddActionSignalTarget(Panel*) {}
 void AddCascadingMenuItem(const char* text, Panel*, Menu* submenu, void*) { items.push_back({text, "", submenu, false}); }
 void AddMenuItem(const char* text, const char* command, Panel*, void*) { items.push_back({text, command, nullptr, false}); }
 void AddCheckableMenuItem(const char* text, const char* command, Panel* parent, void* data) { AddMenuItem(text, command, parent, data); }
 int GetItemCount() const { return int(items.size()); }
 int GetMenuID(int index) { return index; }
 MenuItem* GetMenuItem(int index) { return &items.at(index); }
 bool Contains(const std::string& command) const {
  for (const auto& item : items) if (item.command == command || (item.submenu && item.submenu->Contains(command))) return true;
  return false;
 }
};
struct ComboBox : Panel { void SetMenu(Menu*) {} };
}
std::map<std::string, std::string> resources;
std::string level = "maps/de_train.bsp";
char* allocation = nullptr;
int frees = 0;
struct Callbacks {
 void* COM_LoadFile(char* path, int, int*) {
  auto found = resources.find(path); if (found == resources.end()) return nullptr;
  assert(!allocation); allocation = (char*)std::malloc(found->second.size()+1);
  std::strcpy(allocation, found->second.c_str()); return allocation;
 }
 void COM_FreeFile(void* data) { assert(data == allocation); std::free(allocation); allocation = nullptr; ++frees; }
 char* COM_ParseFile(char* data, char* token) {
  token[0] = 0; if (!data) return nullptr;
  for (;;) {
   while (*data && std::isspace((unsigned char)*data)) ++data;
   if (data[0] != '/' || data[1] != '/') break;
   while (*data && *data != '\n') ++data;
  }
  if (!*data) return nullptr;
  size_t length = 0;
  if (*data == '"') {
   ++data; while (*data && *data != '"') { if (length < 1023) token[length++] = *data; ++data; }
   if (*data) ++data;
  } else if (*data == '{' || *data == '}') token[length++] = *data++;
  else while (*data && !std::isspace((unsigned char)*data) && *data != '{' && *data != '}') { if (length < 1023) token[length++] = *data; ++data; }
  token[length] = 0; return data;
 }
 int TeamNumber() { return 1; }
 const char* GetLevelName() { return level.c_str(); }
} callbacks;
struct Viewport {
 Callbacks* GetClientDllInterface() { return &callbacks; }
 int GetNumberOfTeams() { return 2; }
 char* GetTeamName(int index) { static char first[] = "one", second[] = "two"; return index == 0 ? first : second; }
 int GetValidClasses(int) { return 0; }
} viewport;
Viewport* gViewPortInterface = &viewport;
'''

TESTS = r'''
std::string ReadFile(const char* path) {
 FILE* file = std::fopen(path, "rb"); assert(file);
 std::fseek(file, 0, SEEK_END); long length = std::ftell(file); assert(length >= 0); std::rewind(file);
 std::string text(static_cast<size_t>(length), '\0');
 assert(std::fread(text.data(), 1, text.size(), file) == text.size()); std::fclose(file); return text;
}
vgui2::Menu* Parse(CCreateCommandComboMenu& parser, const char* name, int team = 1) {
 return parser.CreateCommandMenu(const_cast<char*>(name), team, nullptr);
}
struct RejectCustom : CCreateCommandComboMenu {
 vgui2::Menu* CreateCustomButton(vgui2::Panel*, vgui2::Menu*, char*, char*, char*, bool) override { return nullptr; }
};
int main(int argc, char** argv) {
 assert(argc == 3);
 resources["cs"] = ReadFile(argv[1]); resources["dod"] = ReadFile(argv[2]);
 CCreateCommandComboMenu parser;
 auto menu = Parse(parser, "cs");
 assert(menu && menu->Contains("chooseteam") && menu->Contains("timeleft"));
 assert(menu->Contains("say_team Rush the trainyard bomb site!"));
 assert(!menu->Contains("say_team Take the underground!")); delete menu;
 level = "maps/cs_siege.bsp"; menu = Parse(parser, "cs");
 assert(menu->Contains("say_team Take the underground!"));
 assert(!menu->Contains("say_team Rush the trainyard bomb site!")); delete menu;
 level = "maps/dod_caen.bsp"; menu = Parse(parser, "dod");
 assert(menu && menu->Contains("changeclass") && menu->Contains("changeteam"));
 assert(menu->Contains("say_team Bomb their tanks")); delete menu;
 assert(frees == 3 && !allocation && vgui2::liveMenus == 0);
 assert(!Parse(parser, "missing") && vgui2::liveMenus == 0);
 resources["filter"] = "TEAM1 1 one command_one TEAM2 2 two command_two TOGGLE true 3 toggle toggle_command";
 menu = Parse(parser, "filter");
 assert(menu->Contains("command_one") && !menu->Contains("command_two"));
 assert(menu->items.back().command == "toggle_command" && menu->items.back().checked); delete menu;
 resources["broken"] = "MAP other 1 ignored { 1 nested { 1 leaf command";
 menu = Parse(parser, "broken"); delete menu;
 resources["custom"] = "CUSTOM 1 rejected !UNKNOWN { 1 leaf command";
 RejectCustom reject; menu = Parse(reject, "custom"); delete menu;
 resources["unfinished"] = "1 nested { 1 leaf command";
 menu = Parse(parser, "unfinished"); delete menu;
 menu = Parse(parser, "filter", 2);
 assert(menu->Contains("command_two") && !menu->Contains("command_one")); delete menu;
 assert(!allocation && vgui2::liveMenus == 0 && frees == 8);
}
'''

CS_SAMPLE = '1 help { CUSTOM 1 map !MAPBRIEFING 2 time timeleft } 2 team chooseteam 3 maps { MAP de_train 1 train { 1 rush "say_team Rush the trainyard bomb site!" } MAP cs_siege 1 siege { 1 take "say_team Take the underground!" } }'
DOD_SAMPLE = '1 class changeclass 2 team changeteam MAP dod_caen 3 map { 1 tanks "say_team Bomb their tanks" }'


def main():
    arguments = argparse.ArgumentParser()
    arguments.add_argument('--cstrike-menu', type=Path)
    arguments.add_argument('--dod-menu', type=Path)
    args = arguments.parse_args()
    production = (ROOT / 'vgui2/game_controls/CreateCommandMenu.cpp').read_text(encoding='utf-8')
    production = re.sub(r'^#include[^\n]*\n', '', production, flags=re.M)
    with tempfile.TemporaryDirectory(prefix='goldsrc-commandmenu-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(STUBS + production + TESTS)
        (path / 'cs.txt').write_text(args.cstrike_menu.read_text() if args.cstrike_menu else CS_SAMPLE)
        (path / 'dod.txt').write_text(args.dod_menu.read_text() if args.dod_menu else DOD_SAMPLE)
        compiler = shlex.split(os.environ.get('CXX', 'clang++' if os.name == 'nt' else 'c++'))
        subprocess.run(compiler + ['-std=c++17', '-D_CRT_SECURE_NO_WARNINGS', '-I', str(ROOT / 'public'), '-I', str(ROOT / 'engine'), str(path / 'test.cpp'), '-o', str(path / 'test')], check=True)
        subprocess.run([str(path / 'test'), str(path / 'cs.txt'), str(path / 'dod.txt')], check=True, timeout=20)


if __name__ == '__main__':
    main()
