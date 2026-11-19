"""Exercise the production GameUI list loaders with copying list controls and counted KeyValues.

Run: python tests/gameui_list_keyvalues_tests.py
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
#include <algorithm>
#include <cassert>
#include <cctype>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>
#ifdef _WIN32
#define stricmp _stricmp
#define strnicmp _strnicmp
#else
#include <strings.h>
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif
static int liveKeyValues = 0;
class KeyValues {
public:
 explicit KeyValues(const char* name) : m_name(name) { ++liveKeyValues; }
 KeyValues(const char* name, const char* key, const char* value) : m_name(name) { ++liveKeyValues; SetString(key, value); }
 KeyValues(const KeyValues& other) : m_name(other.m_name), m_keys(other.m_keys), m_values(other.m_values) { ++liveKeyValues; }
 ~KeyValues() { --liveKeyValues; }
 KeyValues* MakeCopy() const { return new KeyValues(*this); }
 void deleteThis() { delete this; }
 void SetString(const char* key, const char* value) {
  for (size_t i = 0; i < m_keys.size(); ++i)
   if (m_keys[i] == key) { m_values[i] = value; return; }
  m_keys.push_back(key);
  m_values.push_back(value);
 }
 void SetInt(const char* key, int value) { SetString(key, std::to_string(value).c_str()); }
 const char* GetString(const char* key, const char* value = "") const {
  for (size_t i = 0; i < m_keys.size(); ++i)
   if (m_keys[i] == key) return m_values[i].c_str();
  return value;
 }
 std::string m_name;
private:
 std::vector<std::string> m_keys, m_values;
};
namespace vgui2 {
// both controls keep their own copy of the data like ListPanel::AddItem and ComboBox::AddItem
class ListPanel {
public:
 ~ListPanel() { DeleteAllItems(); }
 int AddItem(const KeyValues* data, unsigned int, bool, bool) { m_items.push_back(data->MakeCopy()); return (int)m_items.size() - 1; }
 void DeleteAllItems() { for (KeyValues* item : m_items) item->deleteThis(); m_items.clear(); }
 int GetItemCount() { return (int)m_items.size(); }
 int GetItemIDFromRow(int row) { return row >= 0 && row < (int)m_items.size() ? row : -1; }
 void SetSingleSelectedItem(int itemID) { m_selected = itemID; }
 void SortList() {}
 std::vector<KeyValues*> m_items;
 int m_selected = -1;
};
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
// files returned by the file system search, in order
static std::vector<std::string> foundFiles;
typedef int FileFindHandle_t;
namespace vgui2 {
struct IFileSystem {
 size_t m_next = 0;
 const char* FindFirst(const char*, FileFindHandle_t*, const char*) { m_next = 0; return FindNext(0); }
 const char* FindNext(FileFindHandle_t) { return m_next < foundFiles.size() ? foundFiles[m_next++].c_str() : nullptr; }
 void FindClose(FileFindHandle_t) {}
};
static IFileSystem g_fileSystem;
IFileSystem* filesystem() { return &g_fileSystem; }
}
// sorted, case insensitive and unique, like the CUtlRBTree<CUtlSymbol> the loaders use
class CUtlSymbol {
public:
 CUtlSymbol(const char* text) : m_text(text) {}
 const char* String() const { return m_text.c_str(); }
private:
 std::string m_text;
};
bool CaselessStringLessThan(const CUtlSymbol& left, const CUtlSymbol& right) { return stricmp(left.String(), right.String()) < 0; }
template <class T, class I> class CUtlRBTree {
public:
 CUtlRBTree(int, int, bool (*less)(const T&, const T&)) : m_less(less) {}
 I InvalidIndex() const { return -1; }
 I Find(const T& value) const {
  for (size_t i = 0; i < m_values.size(); ++i)
   if (!m_less(value, m_values[i]) && !m_less(m_values[i], value)) return (I)i;
  return InvalidIndex();
 }
 void Insert(const T& value) {
  if (Find(value) == InvalidIndex()) m_values.insert(std::upper_bound(m_values.begin(), m_values.end(), value, m_less), value);
 }
 I FirstInorder() const { return m_values.empty() ? InvalidIndex() : 0; }
 I NextInorder(I index) const { return index + 1 < (I)m_values.size() ? index + 1 : InvalidIndex(); }
 T& operator[](I index) { return m_values[index]; }
 T& Element(I index) { return m_values[index]; }
 void RemoveAll() { m_values.clear(); }
private:
 bool (*m_less)(const T&, const T&);
 std::vector<T> m_values;
};
class CModInfo {
public:
 void LoadGameInfoFromBuffer(const char* buffer, int) { m_description = buffer; }
 const char* GetGameDescription() { return m_description.c_str(); }
 const char* GetFallbackDir() { return ""; }
 bool UseFallbackDirMaps() { return false; }
 std::string m_description = "Half-Life";
};
static CModInfo currentMod;
CModInfo* ModInfo() { return &currentMod; }
// the members each loader uses, without the rest of its dialog
class CDemoPlayerFileDialog {
public:
 ~CDemoPlayerFileDialog() { delete m_pDemoList; }
 void LoadDemoList();
 vgui2::ListPanel* m_pDemoList = new vgui2::ListPanel;
};
static std::vector<std::string> mapFiles;
class CCreateMultiplayerGameServerPage {
public:
 CCreateMultiplayerGameServerPage() : m_MapNames(0, 0, CaselessStringLessThan) {}
 ~CCreateMultiplayerGameServerPage() { delete m_pMapList; }
 void LoadMapList();
 void LoadMaps(const char*) { for (const std::string& map : mapFiles) m_MapNames.Insert(CUtlSymbol(map.c_str())); }
 vgui2::ComboBox* m_pMapList = new vgui2::ComboBox;
 CUtlRBTree<CUtlSymbol, short> m_MapNames;
};
static std::vector<const char*> playerNames;
struct EngineStub {
 int GetMaxClients() { return (int)playerNames.size(); }
 const char* PlayerInfo_ValueForKey(int playerIndex, const char*) { return playerNames[playerIndex - 1]; }
} engineStub, *engine = &engineStub;
struct FrameStub { void Activate() {} };
class CPlayerListDialog : public FrameStub {
public:
 typedef FrameStub BaseClass;
 ~CPlayerListDialog() { delete m_pPlayerList; }
 void Activate();
 void RefreshPlayerProperties() {}
 void OnItemSelected() {}
 vgui2::ListPanel* m_pPlayerList = new vgui2::ListPanel;
};
#define SAVEGAME_PATH_MAX 260
class CSaveGameDialog {
public:
 ~CSaveGameDialog() { delete m_pGameList; }
 void ScanSavedGames();
 // every save but the ones named bad is readable
 bool ParseSaveData(const char*, const char* shortName, KeyValues* data) {
  data->SetString("ShortName", shortName);
  return !strstr(shortName, "bad");
 }
 const char* FindSaveSlot() { return "half-life-002.sav"; }
 vgui2::ListPanel* m_pGameList = new vgui2::ListPanel;
};
// the Win32 directory search LoadModList uses
#define MAX_PATH 260
#define _MAX_PATH 260
#define FILE_ATTRIBUTE_DIRECTORY 0x10
typedef void* HANDLE;
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
struct WIN32_FIND_DATA { unsigned int dwFileAttributes; char cFileName[MAX_PATH]; };
static std::vector<std::string> gameDirectories;
static size_t nextDirectory = 0;
static int FindNextFile(HANDLE, WIN32_FIND_DATA* data) {
 if (nextDirectory >= gameDirectories.size()) return 0;
 data->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
 strcpy(data->cFileName, gameDirectories[nextDirectory++].c_str());
 return 1;
}
static HANDLE FindFirstFile(const char*, WIN32_FIND_DATA* data) {
 nextDirectory = 0;
 return FindNextFile(nullptr, data) ? (HANDLE)&gameDirectories : INVALID_HANDLE_VALUE;
}
static void FindClose(HANDLE) {}
static bool UTIL_IsUnquotedCommandArgument(const char*) { return true; }
static char* strlwr(char* text) { for (char* c = text; *c; ++c) *c = (char)tolower((unsigned char)*c); return text; }
class CChangeGameDialog {
public:
 ~CChangeGameDialog() { delete m_pModList; }
 void LoadModList();
 vgui2::ListPanel* m_pModList = new vgui2::ListPanel;
};
'''

TESTS = r'''
static void DemoListKeepsOnlyTheListCopies() {
 foundFiles = {"b.dem", "A.dem", "a.DEM"};
 {
  CDemoPlayerFileDialog dialog;
  for (int load = 0; load < 3; ++load) {
   dialog.LoadDemoList();
   assert(2 == liveKeyValues && 2 == dialog.m_pDemoList->GetItemCount());
  }
  assert(std::string("A.dem") == dialog.m_pDemoList->m_items[0]->GetString("demoname"));
  assert(std::string("b.dem") == dialog.m_pDemoList->m_items[1]->GetString("demoname"));
  assert(0 == dialog.m_pDemoList->m_selected);
 }
 assert(0 == liveKeyValues);
 puts("PASS: loading the demo list frees each source KeyValues and keeps the sorted, unique list copies");
}

static void MapListKeepsOnlyTheComboBoxCopies() {
 mapFiles = {"de_dust", "cs_office"};
 {
  CCreateMultiplayerGameServerPage page;
  for (int load = 0; load < 3; ++load) {
   page.LoadMapList();
   assert(3 == liveKeyValues && 3 == page.m_pMapList->m_items.size());
  }
  assert(std::string("#GameUI_RandomMap") == page.m_pMapList->m_texts[0]);
  assert(std::string("< Random Map >") == page.m_pMapList->m_items[0]->GetString("mapname"));
  assert(std::string("cs_office") == page.m_pMapList->m_items[1]->GetString("mapname"));
  assert(0 == page.m_pMapList->m_active);
  mapFiles.clear();
  page.LoadMapList();
  assert(1 == liveKeyValues);
 }
 assert(0 == liveKeyValues);
 puts("PASS: loading the map list frees the random map and every map KeyValues after the combo box copies them");
}

static void PlayerListSkipsEmptySlotsWithoutLeaking() {
 playerNames = {"Alpha", nullptr, "", "Bravo"};
 {
  CPlayerListDialog dialog;
  for (int activate = 0; activate < 3; ++activate) {
   dialog.Activate();
   assert(2 == liveKeyValues && 2 == dialog.m_pPlayerList->GetItemCount());
  }
  KeyValues* bravo = dialog.m_pPlayerList->m_items[1];
  assert(std::string("4") == bravo->m_name && std::string("Bravo") == bravo->GetString("Name"));
  assert(std::string("4") == bravo->GetString("index"));
 }
 assert(0 == liveKeyValues);
 puts("PASS: refreshing the player list frees each source KeyValues and allocates none for empty slots");
}

static void SaveListFreesTheNewSaveItem() {
 foundFiles = {"one.sav", "HLSave.sav", "bad.sav", "two.sav"};
 {
  CSaveGameDialog dialog;
  dialog.ScanSavedGames();
  assert(3 == liveKeyValues && 3 == dialog.m_pGameList->GetItemCount());
  KeyValues* newSave = dialog.m_pGameList->m_items[2];
  assert(std::string("#GameUI_SaveGame_NewSavedGame") == newSave->GetString("Game"));
  assert(std::string("half-life-002.sav") == newSave->GetString("ShortName"));
 }
 assert(0 == liveKeyValues);
 puts("PASS: scanning saved games frees the new saved game KeyValues after the list copies it");
}

static void WriteGameInfo(const char* directory, const char* description) {
 std::filesystem::create_directory(directory);
 char path[MAX_PATH + 16];
 // the same name LoadModList opens; on POSIX the backslash stays part of the file name
 sprintf(path, "%s\\liblist.gam", directory);
 FILE* f = fopen(path, "wb");
 assert(f);
 fputs(description, f);
 fclose(f);
}

static void ModListKeepsOnlyTheListCopies() {
 WriteGameInfo("ModA", "Mod A");
 WriteGameInfo("valve", "Half-Life");
 std::filesystem::create_directory("nogame");
 gameDirectories = {"ModA", "valve", "nogame"};
 {
  CChangeGameDialog dialog;
  dialog.LoadModList();
  assert(1 == liveKeyValues && 1 == dialog.m_pModList->GetItemCount());
  assert(std::string("Mod A") == dialog.m_pModList->m_items[0]->GetString("ModName"));
  assert(std::string("moda") == dialog.m_pModList->m_items[0]->GetString("ModDir"));
 }
 assert(0 == liveKeyValues);
 puts("PASS: loading the mod list frees each source KeyValues after the list copies it");
}
'''


def main():
    code = STUBS
    for path, signature in [('GameUI/DemoPlayerFileDialog.cpp', 'void CDemoPlayerFileDialog::LoadDemoList('),
                            ('GameUI/CreateMultiplayerGameServerPage.cpp', 'void CCreateMultiplayerGameServerPage::LoadMapList('),
                            ('GameUI/PlayerListDialog.cpp', 'void CPlayerListDialog::Activate('),
                            ('GameUI/SaveGameDialog.cpp', 'void CSaveGameDialog::ScanSavedGames('),
                            ('GameUI/ChangeGameDialog.cpp', 'void CChangeGameDialog::LoadModList(')]:
        body = function((ROOT / path).read_text(encoding='utf-8'), signature)
        # the mod list is only built on Windows; the stubs above stand in for its Win32 search
        code += body.replace('#ifdef _WIN32', '#if 1') + '\n'
    code += TESTS
    code += ('\nint main() { DemoListKeepsOnlyTheListCopies(); MapListKeepsOnlyTheComboBoxCopies(); '
             'PlayerListSkipsEmptySlotsWithoutLeaking(); SaveListFreesTheNewSaveItem(); ModListKeepsOnlyTheListCopies(); }\n')
    with tempfile.TemporaryDirectory(prefix='goldsrc-gameui-list-keyvalues-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([path / 'test.cpp'], path / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=path)


if __name__ == '__main__':
    main()
