"""Exercise the production dedicated server mod list loader with a copying combo box and counted KeyValues.

Run: python tests/dedicated_mod_list_tests.py
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
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#ifndef _WIN32
#include <strings.h>
#define _snprintf snprintf
#define _stricmp strcasecmp
#endif
static int liveKeyValues = 0;
class KeyValues {
public:
 KeyValues(const char* name, const char* key, const char* value) : m_name(name), m_key(key), m_value(value) { ++liveKeyValues; }
 KeyValues(const KeyValues& other) : m_name(other.m_name), m_key(other.m_key), m_value(other.m_value) { ++liveKeyValues; }
 ~KeyValues() { --liveKeyValues; }
 KeyValues* MakeCopy() const { return new KeyValues(*this); }
 void deleteThis() { delete this; }
 const char* GetString(const char* key) const { return m_key == key ? m_value.c_str() : ""; }
 std::string m_name;
private:
 std::string m_key, m_value;
};
namespace vgui2 {
// keeps its own copy of the user data like ComboBox::AddItem
class ComboBox {
public:
 ~ComboBox() { DeleteAllItems(); }
 int AddItem(const char* text, const KeyValues* userData) {
  m_texts.push_back(text);
  m_items.push_back(userData->MakeCopy());
  return (int)m_items.size() - 1;
 }
 void DeleteAllItems() { for (KeyValues* item : m_items) item->deleteThis(); m_items.clear(); m_texts.clear(); }
 void ActivateItem(int itemID) { m_active = itemID; }
 std::vector<std::string> m_texts;
 std::vector<KeyValues*> m_items;
 int m_active = -1;
};
}
template <class T> class CUtlVector {
public:
 void RemoveAll() { m_values.clear(); }
 void AddToTail(const T& value) { m_values.push_back(value); }
 int Count() const { return (int)m_values.size(); }
 T& Element(int index) { return m_values[index]; }
private:
 std::vector<T> m_values;
};
// directories found in the game folder, the liblist.gam paths that exist, and the liblist values of each
static std::vector<std::string> directories;
static std::set<std::string> existingFiles;
static std::map<std::string, std::map<std::string, std::string>> liblists;
typedef int FileFindHandle_t;
namespace vgui2 {
struct IFileSystem {
 size_t m_next = 0;
 const char* FindFirst(const char*, FileFindHandle_t*, const char*) { m_next = 0; return FindNext(0); }
 const char* FindNext(FileFindHandle_t) { return m_next < directories.size() ? directories[m_next++].c_str() : nullptr; }
 bool FindIsDirectory(FileFindHandle_t) { return true; }
 bool FileExists(const char* path) { return existingFiles.count(path) != 0; }
 void FindClose(FileFindHandle_t) {}
};
static IFileSystem g_fileSystem;
IFileSystem* filesystem() { return &g_fileSystem; }
}
// only the members LoadModList uses
class CCreateMultiplayerGameServerPage {
public:
 enum { GAME_DIR_LENGTH = 64 };
 struct GameDir_t { char gameDir[GAME_DIR_LENGTH]; char name[GAME_DIR_LENGTH]; };
 ~CCreateMultiplayerGameServerPage() { delete m_pModList; }
 void LoadModList();
 bool GetLiblistValue(const char* filename, const char* key, char* value, int valueLength) {
  auto liblist = liblists.find(filename);
  if (liblist == liblists.end() || !liblist->second.count(key)) return false;
  snprintf(value, valueLength, "%s", liblist->second[key].c_str());
  return true;
 }
 vgui2::ComboBox* m_pModList = new vgui2::ComboBox;
 char m_szMod[256] = {};
 int m_iCurrentGame = -1;
 CUtlVector<GameDir_t> m_GameDirs;
};
'''

TESTS = r'''
static void ModListKeepsOnlyTheComboBoxCopies() {
 directories = {"cstrike", "solo", "valve", "nogame", "dmc"};
 existingFiles = {"cstrike\\liblist.gam", "solo\\liblist.gam", "valve\\liblist.gam", "dmc\\liblist.gam"};
 liblists = {{"cstrike\\liblist.gam", {{"game", "Counter-Strike"}}},
             {"solo\\liblist.gam", {{"type", "singleplayer_only"}}},
             {"valve\\liblist.gam", {{"game", "Half-Life"}}}};
 {
  CCreateMultiplayerGameServerPage page;
  for (int load = 0; load < 3; ++load) {
   page.LoadModList();
   assert(3 == liveKeyValues && 3 == page.m_pModList->m_items.size() && 3 == page.m_GameDirs.Count());
  }
  vgui2::ComboBox* list = page.m_pModList;
  assert(std::string("Half-Life") == list->m_texts[0] && std::string("valve") == list->m_items[0]->GetString("gamedir"));
  assert(std::string("Counter-Strike") == list->m_texts[1] && std::string("Counter-Strike") == list->m_items[1]->m_name);
  assert(std::string("cstrike") == list->m_items[1]->GetString("gamedir"));
  assert(std::string("dmc") == list->m_texts[2] && std::string("dmc") == list->m_items[2]->GetString("gamedir"));
  assert(0 == list->m_active && 0 == strcmp("valve", page.m_szMod));
  assert(0 == strcmp("Half-Life", page.m_szMod + CCreateMultiplayerGameServerPage::GAME_DIR_LENGTH));
 }
 assert(0 == liveKeyValues);
 puts("PASS: loading the mod list frees the Half-Life and every mod KeyValues after the combo box copies them");
}

static void EmptyGameFolderKeepsTheHalfLifeItem() {
 directories.clear();
 {
  CCreateMultiplayerGameServerPage page;
  page.LoadModList();
  assert(1 == liveKeyValues && 1 == page.m_pModList->m_items.size());
 }
 assert(0 == liveKeyValues);
 puts("PASS: a game folder without mods lists only Half-Life and frees its source KeyValues");
}
'''


def main():
    source = (ROOT / 'dedicated/CreateMultiplayerGameServerPage.cpp').read_text(encoding='utf-8')
    code = STUBS + function(source, 'void CCreateMultiplayerGameServerPage::LoadModList(') + TESTS
    code += '\nint main() { ModListKeepsOnlyTheComboBoxCopies(); EmptyGameFolderKeepsTheHalfLifeItem(); }\n'
    with tempfile.TemporaryDirectory(prefix='goldsrc-dedicated-mod-list-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([path / 'test.cpp'], path / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=path)


if __name__ == '__main__':
    main()
