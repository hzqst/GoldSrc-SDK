"""Exercise the production spectator player lists and Condition Zero command menu with copying controls and counted KeyValues.

Run: python tests/spectator_command_menu_tests.py
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
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>
#ifndef _WIN32
#include <strings.h>
#define _snprintf snprintf
#define _strnicmp strncasecmp
#endif
#define ASSERT_AT(condition, line) assert(condition)
static int liveKeyValues = 0;
class KeyValues {
public:
 KeyValues(const char* name, const char* key, const char* value) : m_name(name), m_keys{key}, m_values{value} { ++liveKeyValues; }
 KeyValues(const char* name, const char* firstKey, const char* firstValue, const char* secondKey, const char* secondValue)
  : m_name(name), m_keys{firstKey, secondKey}, m_values{firstValue, secondValue} { ++liveKeyValues; }
 KeyValues(const KeyValues& other) : m_name(other.m_name), m_keys(other.m_keys), m_values(other.m_values) { ++liveKeyValues; }
 ~KeyValues() { --liveKeyValues; }
 KeyValues* MakeCopy() const { return new KeyValues(*this); }
 void deleteThis() { delete this; }
 const char* GetString(const char* key) const {
  for (size_t i = 0; i < m_keys.size(); ++i)
   if (m_keys[i] == key) return m_values[i].c_str();
  return "";
 }
 std::string m_name;
private:
 std::vector<std::string> m_keys, m_values;
};
namespace vgui2 {
class Panel {
public:
 virtual ~Panel() {}
 Panel* GetParent() { return m_parent; }
 Panel* m_parent = nullptr;
};
// keeps its own copy of the user data like ComboBox::AddItem and UpdateItem
class ComboBox {
public:
 ~ComboBox() { DeleteAllItems(); }
 int AddItem(const wchar_t* text, const KeyValues* userData) {
  m_texts.push_back(text);
  m_items.push_back(userData->MakeCopy());
  return (int)m_items.size() - 1;
 }
 bool IsItemIDValid(int itemID) { return itemID >= 0 && itemID < (int)m_items.size(); }
 void UpdateItem(int itemID, const wchar_t* text, const KeyValues* userData) {
  m_texts[itemID] = text;
  m_items[itemID]->deleteThis();
  m_items[itemID] = userData->MakeCopy();
 }
 void DeleteAllItems() { for (KeyValues* item : m_items) item->deleteThis(); m_items.clear(); m_texts.clear(); }
 std::vector<std::wstring> m_texts;
 std::vector<KeyValues*> m_items;
};
// takes over the command and copies the user data like MenuItem::SetCommand and SetUserData
class Menu;
class MenuItem {
public:
 MenuItem(const char* text, KeyValues* command, const KeyValues* userData, Menu* cascade)
  : m_text(text), m_command(command), m_userData(userData ? userData->MakeCopy() : nullptr), m_cascade(cascade) {}
 ~MenuItem() { if (m_command) m_command->deleteThis(); if (m_userData) m_userData->deleteThis(); }
 void SetChecked(bool checked) { m_checked = checked; }
 std::string m_text;
 KeyValues* m_command;
 KeyValues* m_userData;
 Menu* m_cascade;
 bool m_checkable = false, m_checked = false;
};
static std::vector<Menu*> menus;
class Menu : public Panel {
public:
 Menu(Panel* parent, const char*) { m_parent = parent; menus.push_back(this); }
 ~Menu() { for (MenuItem* item : m_items) delete item; }
 void SetVisible(bool) {}
 int AddMenuItem(const char* text, KeyValues* command, Panel*, const KeyValues* userData) { return Add(new MenuItem(text, command, userData, nullptr)); }
 int AddCheckableMenuItem(const char* text, KeyValues* command, Panel* target, const KeyValues* userData) {
  int itemID = AddMenuItem(text, command, target, userData);
  m_items[itemID]->m_checkable = true;
  return itemID;
 }
 int AddCascadingMenuItem(const char* text, Panel*, Menu* cascade, const KeyValues* userData) { return Add(new MenuItem(text, nullptr, userData, cascade)); }
 int GetItemCount() { return (int)m_items.size(); }
 int GetMenuID(int index) { return index; }
 MenuItem* GetMenuItem(int itemID) { return m_items[itemID]; }
 std::vector<MenuItem*> m_items;
private:
 int Add(MenuItem* item) { m_items.push_back(item); return (int)m_items.size() - 1; }
};
// the player text is the first name passed to the format
struct ILocalize {
 wchar_t m_found[64];
 wchar_t* Find(const char* token) {
  if (strncmp(token, "#Spec_", 6)) return nullptr;
  mbstowcs(m_found, token, 64);
  return m_found;
 }
 void ConvertANSIToUnicode(const char* text, wchar_t* output, int size) { mbstowcs(output, text, size / sizeof(wchar_t)); }
 void ConstructString(wchar_t* output, int size, const wchar_t*, int count, ...) {
  va_list args;
  va_start(args, count);
  const wchar_t* name = va_arg(args, const wchar_t*);
  va_end(args);
  const size_t length = size / sizeof(wchar_t);
  wcsncpy(output, name, length - 1);
  output[length - 1] = 0;
 }
};
static ILocalize g_localize;
ILocalize* localize() { return &g_localize; }
}
using namespace vgui2;
struct BottomBarStub {
 bool IsVisible() { return true; }
 ComboBox* m_pPlayerList = new ComboBox;
 ~BottomBarStub() { delete m_pPlayerList; }
};
'''

GAME_CONTROLS = r'''
namespace game_controls {
struct VGuiLibraryPlayer_t { const char* name; bool thisplayer; const char* teamname; int teamnumber; bool dead; };
static std::vector<VGuiLibraryPlayer_t> players;
struct ClientDllStub {
 int GetMaxPlayers() { return (int)players.size(); }
 VGuiLibraryPlayer_t CallEnghudGetPlayerInfo(int player) { return players[player - 1]; }
} clientDll;
struct ViewPortStub { ClientDllStub* GetClientDllInterface() { return &clientDll; } } viewPort, *gViewPortInterface = &viewPort;
// only the members UpdateSpectatorPlayerList uses
class CSpectatorGUI {
public:
 ~CSpectatorGUI() { delete m_pBottomBar; }
 void UpdateSpectatorPlayerList();
 bool CanSpectateTeam(int team) { return team != 3; }
 BottomBarStub* m_pBottomBar = new BottomBarStub;
};
'''

CZEROR = r'''
namespace czeror {
#define MAX_PLAYERS 32
struct PlayerInfo { const char* name; };
struct ExtraPlayerInfo { bool dead; int teamnumber; char teamname[16]; };
static PlayerInfo g_PlayerInfoList[MAX_PLAYERS + 1];
static ExtraPlayerInfo g_PlayerExtraInfo[MAX_PLAYERS + 1];
struct cl_entity_t { int index; };
static cl_entity_t localPlayer = {3};
static std::string menuText;
static bool menuExists = true;
static char* loadedFile = nullptr;
static int wrongFrees = 0;
static int maxClients = 0;
struct EngineStub {
 int GetMaxClients() { return maxClients; }
 cl_entity_t* GetLocalPlayer() { return &localPlayer; }
 void* COM_LoadFile(const char*, int, int*) {
  if (!menuExists) return nullptr;
  loadedFile = new char[menuText.size() + 1];
  memcpy(loadedFile, menuText.c_str(), menuText.size() + 1);
  return loadedFile;
 }
 // frees only the buffer COM_LoadFile returned, and counts any other pointer instead of corrupting the heap
 void COM_FreeFile(void* file) {
  if (!file) return;
  if (file != loadedFile) { ++wrongFrees; return; }
  delete[] loadedFile;
  loadedFile = nullptr;
 }
 void Con_DPrintf(const char*, ...) {}
 // whitespace separated tokens or quoted strings, and NULL with an empty token at the end
 char* COM_ParseFile(char* data, char* token) {
  token[0] = 0;
  if (!data) return nullptr;
  while (*data && isspace((unsigned char)*data)) ++data;
  if (!*data) return nullptr;
  int length = 0;
  if (*data == '"') {
   for (++data; *data && *data != '"'; ++data) token[length++] = *data;
   if (*data) ++data;
  } else {
   for (; *data && !isspace((unsigned char)*data); ++data) token[length++] = *data;
  }
  token[length] = 0;
  return data;
 }
} gEngfuncs;
static const char* sTFClasses[] = {"", "SCOUT", "SNIPER", "SOLDIER", "DEMOMAN", "MEDIC", "HWGUY", "PYRO", "SPY", "ENGINEER", "CIVILIAN"};
class CBottomBar : public BottomBarStub {
public:
 int PlayerAddItem(int item, wchar_t* text, KeyValues* data);
};
// only the members UpdateSpectatorPlayerList uses
class CSpectatorGUI {
public:
 ~CSpectatorGUI() { delete m_pBottomBar; }
 void UpdateSpectatorPlayerList();
 bool CanSpectateTeam(int team) { return team != 3; }
 CBottomBar* m_pBottomBar = new CBottomBar;
};
'''

TESTS = r'''
static void AssertPlayers(ComboBox* list) {
 assert(2 == list->m_items.size() && 2 == liveKeyValues);
 assert(std::wstring(L"Alpha") == list->m_texts[0] && std::string("Alpha") == list->m_items[0]->GetString("player"));
 assert(std::wstring(L"Bravo") == list->m_texts[1] && std::string("Bravo") == list->m_items[1]->GetString("player"));
}

static void GameControlsPlayerListKeepsOnlyTheComboBoxCopies() {
 using namespace game_controls;
 players = {{"Alpha", false, "CT", 2, false}, {nullptr, false, nullptr, 2, false}, {"Local", true, nullptr, 2, false},
            {"Dead", false, nullptr, 2, true}, {"Bravo", false, nullptr, 1, false}, {"Hidden", false, nullptr, 3, false}};
 {
  CSpectatorGUI gui;
  for (int update = 0; update < 3; ++update) {
   gui.UpdateSpectatorPlayerList();
   AssertPlayers(gui.m_pBottomBar->m_pPlayerList);
  }
 }
 assert(0 == liveKeyValues);
 puts("PASS: updating the game_controls spectator player list frees each source KeyValues");
}

static void CzerorPlayerListKeepsOnlyTheComboBoxCopies() {
 using namespace czeror;
 const char* names[] = {"Alpha", nullptr, "Local", "Dead", "Bravo", "Hidden"};
 const int teams[] = {2, 2, 2, 2, 1, 3};
 maxClients = 6;
 for (int player = 1; player <= maxClients; ++player) {
  g_PlayerInfoList[player].name = names[player - 1];
  g_PlayerExtraInfo[player] = {player == 4, teams[player - 1], "CT"};
 }
 {
  CSpectatorGUI gui;
  for (int update = 0; update < 3; ++update) {
   gui.UpdateSpectatorPlayerList();
   AssertPlayers(gui.m_pBottomBar->m_pPlayerList);
  }
 }
 assert(0 == liveKeyValues);
 puts("PASS: updating the Condition Zero spectator player list frees each source KeyValues");
}

static void DestroyMenus() {
 for (Menu* menu : menus) delete menu;
 menus.clear();
}

static void CommandMenuKeepsTheCommandsAndUserDataCopies() {
 using namespace czeror;
 menuText = "\"1\" \"Plain\" \"say plain\"\n"
            "MAP \"de_dust\" \"2\" \"Dust\" \"map de_dust\"\n"
            "TOGGLE \"true\" \"3\" \"Toggle\" \"toggle_cmd\"\n"
            "TEAM1 \"4\" \"Team\" \"team_cmd\"\n"
            "TEAM2 \"5\" \"Other\" \"other_cmd\"\n"
            "\"6\" \"Sub\" \"{\"\n"
            "\"7\" \"Inner\" \"inner_cmd\"\n"
            "\"}\"\n";
 Panel parent;
 char menuFile[] = "commandmenu.txt";
 Menu* root = CreateCommandMenu(menuFile, 1, &parent);
 assert(!loadedFile && 0 == wrongFrees);
 // each command item holds the command it took over and its own user data copy
 assert(5 == root->GetItemCount() && 10 == liveKeyValues);
 MenuItem* dust = root->GetMenuItem(1);
 assert(std::string("de_dust") == dust->m_userData->GetString("map") && std::string("map de_dust") == dust->m_userData->GetString("command"));
 assert(std::string("Dust") == dust->m_command->GetString("text"));
 assert(root->GetMenuItem(2)->m_checkable && root->GetMenuItem(2)->m_checked);
 assert(std::string("team_cmd") == root->GetMenuItem(3)->m_userData->GetString("command"));
 Menu* sub = root->GetMenuItem(4)->m_cascade;
 assert(sub && 1 == sub->GetItemCount() && std::string("inner_cmd") == sub->GetMenuItem(0)->m_userData->GetString("command"));
 DestroyMenus();
 assert(0 == liveKeyValues);
 puts("PASS: building the Condition Zero command menu frees each source user data KeyValues after the menu copies it");
 puts("PASS: building the Condition Zero command menu frees the loaded menu file");
}

static void CommandMenuStoppedByAnEmptyTokenFreesTheMenuFile() {
 using namespace czeror;
 // parsing stops at the empty token, in the middle of the file
 menuText = "\"1\" \"Plain\" \"say plain\"\n\"\"\n\"2\" \"After\" \"say after\"\n";
 Panel parent;
 char menuFile[] = "commandmenu.txt";
 Menu* root = CreateCommandMenu(menuFile, 1, &parent);
 assert(1 == root->GetItemCount());
 assert(!loadedFile && 0 == wrongFrees);
 DestroyMenus();
 puts("PASS: a command menu that stops at an empty token frees the loaded menu file and not the parse position");
}

static void MissingCommandMenuFileBuildsAnEmptyMenu() {
 using namespace czeror;
 menuExists = false;
 Panel parent;
 char menuFile[] = "missing.txt";
 Menu* root = CreateCommandMenu(menuFile, 1, &parent);
 assert(0 == root->GetItemCount() && 0 == wrongFrees);
 DestroyMenus();
 menuExists = true;
 puts("PASS: a missing command menu file builds an empty menu");
}
'''


def main():
    spectator = (ROOT / 'vgui2/game_controls/SpectatorGUI.cpp').read_text(encoding='utf-8')
    czeror_spectator = (ROOT / 'czeror/cl_dll/vgui/hl_base/SpectatorGUI.cpp').read_text(encoding='utf-8')
    command_menu = (ROOT / 'czeror/cl_dll/vgui/hl_base/CreateCommandMenu.cpp').read_text(encoding='utf-8')
    code = STUBS
    code += GAME_CONTROLS + function(spectator, 'void CSpectatorGUI::UpdateSpectatorPlayerList(') + '\n}\n'
    code += CZEROR
    code += function(czeror_spectator, 'int CBottomBar::PlayerAddItem(') + '\n'
    code += function(czeror_spectator, 'void CSpectatorGUI::UpdateSpectatorPlayerList(') + '\n'
    code += function(command_menu, 'Menu* CreateCommandMenu(') + '\n}\n'
    code += TESTS
    code += ('\nint main() { GameControlsPlayerListKeepsOnlyTheComboBoxCopies(); CzerorPlayerListKeepsOnlyTheComboBoxCopies(); '
             'CommandMenuKeepsTheCommandsAndUserDataCopies(); CommandMenuStoppedByAnEmptyTokenFreesTheMenuFile(); '
             'MissingCommandMenuFileBuildsAnEmptyMenu(); }\n')
    with tempfile.TemporaryDirectory(prefix='goldsrc-spectator-command-menu-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([path / 'test.cpp'], path / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=path)


if __name__ == '__main__':
    main()
