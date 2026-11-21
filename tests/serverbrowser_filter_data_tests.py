"""Exercise production server browser filter loading, saving and destruction with stubs.

Run: python tests/serverbrowser_filter_data_tests.py
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


# KeyValues keeps the production RemoveSubKey / AddSubKey / Clear and counts the
# live nodes; ServerBrowser.vdf is a KeyValues tree kept in memory.
STUBS = r'''
#include <cassert>
#include <cstdio>
#include <map>
#include <string>
struct IFileSystem;
namespace vgui2 { IFileSystem* filesystem() { return nullptr; } }
class KeyValues {
public:
 static int s_LiveCount;
 explicit KeyValues(const char* name) : m_Name(name) { ++s_LiveCount; }
 ~KeyValues();
 void Destroy();
 const char* GetName() const { return m_Name.c_str(); }
 KeyValues* GetNextKey() { return m_pPeer; }
 void SetNextKey(KeyValues* key);
 KeyValues* GetFirstSubKey() { return m_pSub; }
 void RemoveSubKey(KeyValues* key);
 void AddSubKey(KeyValues* key);
 void Clear();
 void deleteThis() { delete this; }
 KeyValues* FindKey(const char* name, bool create) {
  for (KeyValues* child=m_pSub; child; child=child->m_pPeer) if (child->m_Name==name) return child;
  if (!create) return nullptr;
  KeyValues* child=new KeyValues(name); AddSubKey(child); return child;
 }
 KeyValues* MakeCopy() const {
  KeyValues* copy=new KeyValues(GetName()); copy->m_Value=m_Value;
  for (KeyValues* child=m_pSub; child; child=child->m_pPeer) copy->AddSubKey(child->MakeCopy());
  return copy;
 }
 void SetString(const char* name, const char* value) { FindKey(name, true)->m_Value=value; }
 const char* GetString(const char* name, const char* def) { KeyValues* key=FindKey(name, false); return key?key->m_Value.c_str():def; }
 bool LoadFromFile(IFileSystem*, const char* file, const char*);
 bool SaveToFile(IFileSystem*, const char* file, const char*);
private:
 void FreeAllocatedValue() { m_Value.clear(); }
 enum { TYPE_NONE };
 int m_iDataType=TYPE_NONE;
 std::string m_Name, m_Value;
 KeyValues* m_pSub=nullptr;
 KeyValues* m_pPeer=nullptr;
};
int KeyValues::s_LiveCount=0;
std::map<std::string, std::string> g_Disk;
// stores "name/sub=value" lines so the disk copy is not counted as live nodes
static void Flatten(KeyValues* key, const std::string& prefix, std::string& out) {
 for (KeyValues* child=key->GetFirstSubKey(); child; child=child->GetNextKey()) {
  std::string path=prefix+child->GetName();
  if (!child->GetFirstSubKey()) out+=path+"="+key->GetString(child->GetName(), "")+"\n";
  Flatten(child, path+"/", out);
 }
}
bool KeyValues::SaveToFile(IFileSystem*, const char* file, const char*) {
 std::string out; Flatten(this, "", out); g_Disk[file]=out; return true;
}
bool KeyValues::LoadFromFile(IFileSystem*, const char* file, const char*) {
 auto entry=g_Disk.find(file); if (entry==g_Disk.end()) return false;
 size_t start=0; const std::string& text=entry->second;
 while (start<text.size()) {
  size_t end=text.find('\n', start), eq=text.find('=', start);
  std::string path=text.substr(start, eq-start), value=text.substr(eq+1, end-eq-1);
  KeyValues* key=this; size_t part=0, slash;
  while ((slash=path.find('/', part))!=std::string::npos) { key=key->FindKey(path.substr(part, slash-part).c_str(), true); part=slash+1; }
  key->SetString(path.substr(part).c_str(), value.c_str());
  start=end+1;
 }
 return true;
}
class CServerContextMenu {};
struct Page {
 int filterLoads=0, configSaves=0;
 void LoadFilterSettings() { ++filterLoads; }
 void SaveUserConfig() { ++configSaves; }
 void LoadHistoryList() {} void LoadFavoritesList() {}
 bool IsVisible() { return false; } void StartRefresh() {}
};
typedef Page CFavoriteGames, CHistoryGames, CInternetGames, CSpectateGames, CLanGames, CFriendsGames, IGameList;
class CServerBrowserDialog {
public:
 ~CServerBrowserDialog();
 void LoadUserData();
 void SaveUserData();
 KeyValues* GetFilterSaveData(const char* filterSet);
 void ReloadFilterSettings();
 bool IsVisible() { return false; } void InvalidateLayout(bool, bool) {} void Repaint() {}
 Page pages[6];
 IGameList* m_pGameList=&pages[1];
 CFavoriteGames* m_pFavorites=nullptr; CHistoryGames* m_pHistory=nullptr;
 CInternetGames* m_pInternetGames=&pages[0]; CSpectateGames* m_pSpectateGames=&pages[3];
 CLanGames* m_pLanGames=&pages[4]; CFriendsGames* m_pFriendsGames=&pages[5];
 KeyValues* m_pSavedData=nullptr; KeyValues* m_pFilterData=nullptr;
 CServerContextMenu* m_pContextMenu=nullptr;
};
'''

TESTS = r'''
// Runs the constructor's LoadUserData() before the pages exist, then attaches them.
static CServerBrowserDialog* Create() {
 CServerBrowserDialog* dialog=new CServerBrowserDialog;
 dialog->LoadUserData();
 dialog->m_pFavorites=&dialog->pages[1]; dialog->m_pHistory=&dialog->pages[2];
 dialog->m_pContextMenu=new CServerContextMenu;
 return dialog;
}

static void ReloadAndSaveKeepOneFilterTree() {
 g_Disk.clear(); KeyValues::s_LiveCount=0;
 g_Disk["ServerBrowser.vdf"]="GameList=lan\nFilters/InternetGames/game=cstrike\nFilters/LanGames/map=de_dust\n";
 CServerBrowserDialog* dialog=Create();
 assert(std::string("cstrike")==dialog->GetFilterSaveData("InternetGames")->GetString("game", ""));
 assert(dialog->m_pSavedData->FindKey("Filters", false)==nullptr);
 int liveAfterLoad=KeyValues::s_LiveCount;
 for (int i=0;i<4;++i) { dialog->LoadUserData(); assert(liveAfterLoad==KeyValues::s_LiveCount); }
 assert(6*4==dialog->pages[0].filterLoads+dialog->pages[1].filterLoads+dialog->pages[2].filterLoads
  +dialog->pages[3].filterLoads+dialog->pages[4].filterLoads+dialog->pages[5].filterLoads);

 dialog->GetFilterSaveData("InternetGames")->SetString("game", "valve");
 dialog->SaveUserData();
 int liveAfterSave=KeyValues::s_LiveCount;
 for (int i=0;i<4;++i) { dialog->SaveUserData(); assert(liveAfterSave==KeyValues::s_LiveCount); }
 // the saved data holds its own copy, not the filter tree the pages edit
 KeyValues* saved=dialog->m_pSavedData->FindKey("Filters", false);
 assert(saved && saved!=dialog->m_pFilterData);
 assert(std::string("valve")==saved->FindKey("InternetGames", false)->GetString("game", ""));

 delete dialog;
 assert(0==KeyValues::s_LiveCount);
 assert(std::string("GameList=favorites\nFilters/InternetGames/game=valve\nFilters/LanGames/map=de_dust\n")
  ==g_Disk["ServerBrowser.vdf"]);
 puts("PASS: reloading and saving filters keep one owned filter tree and free it on destruction");
}

static void MissingFiltersAreCreated() {
 g_Disk.clear(); KeyValues::s_LiveCount=0;
 CServerBrowserDialog* dialog=Create();
 dialog->LoadUserData(); dialog->LoadUserData();
 dialog->GetFilterSaveData("LanGames")->SetString("map", "crossfire");
 delete dialog;
 assert(0==KeyValues::s_LiveCount);
 assert(std::string("GameList=favorites\nFilters/LanGames/map=crossfire\n")==g_Disk["ServerBrowser.vdf"]);
 puts("PASS: a config without filters gets a new filter tree that is saved and freed");
}

static void SaveWithoutLoadIsIgnored() {
 g_Disk.clear(); KeyValues::s_LiveCount=0;
 CServerBrowserDialog dialog;
 dialog.SaveUserData();
 assert(g_Disk.empty() && 0==dialog.pages[0].configSaves);
 puts("PASS: saving before any load does nothing");
}
'''


def main():
    keyvalues = (ROOT / 'vgui2/src/public/KeyValues.cpp').read_text()
    source = (ROOT / 'serverbrowser/ServerBrowserDialog.cpp').read_text()
    code = STUBS
    for signature in ['void KeyValues::RemoveSubKey(', 'void KeyValues::AddSubKey(',
                      'void KeyValues::SetNextKey(', 'void KeyValues::Clear()']:
        code += function(keyvalues, signature)
    # the production destructor frees the children; wrap it to count the node
    code += function(keyvalues, 'KeyValues::~KeyValues()').replace('KeyValues::~KeyValues()', 'void KeyValues::Destroy()')
    code += 'KeyValues::~KeyValues() { --s_LiveCount; Destroy(); }\n'
    for signature in ['CServerBrowserDialog::~CServerBrowserDialog()',
                      'void CServerBrowserDialog::LoadUserData()',
                      'void CServerBrowserDialog::SaveUserData()',
                      'KeyValues *CServerBrowserDialog::GetFilterSaveData(',
                      'void CServerBrowserDialog::ReloadFilterSettings()']:
        code += function(source, signature)
    code += TESTS
    code += '\nint main() { ReloadAndSaveKeepOneFilterTree(); MissingFiltersAreCreated(); SaveWithoutLoadIsIgnored(); }\n'
    with tempfile.TemporaryDirectory(prefix='goldsrc-serverbrowser-filters-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code)
        executable = compile_program([path / 'test.cpp'], path / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=path)


if __name__ == '__main__':
    main()
