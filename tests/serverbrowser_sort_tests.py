"""Exercise production server browser filtering, display and sorting with stubs.

Run: python tests/serverbrowser_sort_tests.py
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
#include "serverbrowser/igamelist.h"
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <map>
#include <string>
#include <utility>
#include <vector>
#if defined(_WIN32)
#define stricmp _stricmp
#else
#include <strings.h>
#define stricmp strcasecmp
#endif
typedef void* HServerListRequest;
struct KeyValues {
 std::map<std::string, std::string> values;
 explicit KeyValues(const char*) {}
 void SetString(const char* key, const char* value) { values[key] = value; }
 void SetInt(const char* key, int value) { values[key] = std::to_string(value); }
 void SetWString(const char* key, const wchar_t*) { values[key] = "<localized>"; }
 void deleteThis() { delete this; }
};
struct netadr_t {
 servernetadr_t adr;
 netadr_t(unsigned int ip, unsigned short port) { adr.Init(ip, port, port); }
 const char* ToString(bool) const { return adr.GetConnectionAddressString(); }
 bool operator==(const netadr_t& other) const { return adr.GetIP()==other.adr.GetIP() && adr.GetConnectionPort()==other.adr.GetConnectionPort(); }
};
template <typename K, typename V> struct Map {
 std::vector<std::pair<K, V>> items; std::vector<bool> valid;
 int Find(const K& key) { for (size_t i=0;i<items.size();++i) if (valid[i] && items[i].first==key) return int(i); return -1; }
 int Insert(const K& key, const V& value) { items.emplace_back(key, value); valid.push_back(true); return int(items.size())-1; }
 void RemoveAt(int i) { valid[i]=false; }
 V& operator[](int i) { return items[i].second; }
 int MaxElement() { return int(items.size()); }
 bool IsValidIndex(int i) { return i>=0 && i<int(items.size()) && valid[i]; }
};
struct GameList {
 std::map<int, KeyValues> items; std::map<int, bool> visible; int next=0;
 bool IsValidItemID(int id) { return items.count(id)!=0; }
 void RemoveItem(int id) { items.erase(id); visible.erase(id); }
 KeyValues* GetItem(int id) { return &items.at(id); }
 void SetUserData(int, unsigned int) {}
 int AddItem(KeyValues* kv, unsigned int, bool, bool) { items.emplace(next, *kv); visible[next]=true; return next++; }
 int GetItemCount() { return int(items.size()); }
 void SetSingleSelectedItem(int) {} void ApplyItemChanges(int) {}
 void SetItemVisible(int id, bool value) { visible[id]=value; }
 void SortList() {}
};
std::map<int, gameserveritem_t> steamServers;
struct Matchmaking {
 gameserveritem_t* GetServerDetails(HServerListRequest, int id) { auto entry=steamServers.find(id); return entry==steamServers.end()?nullptr:&entry->second; }
} matchmaking;
Matchmaking* SteamMatchmakingServers() { return &matchmaking; }
struct Browser { bool IsVACBannedFromGame(int) { return false; } } browser;
Browser& ServerBrowser() { return browser; }
namespace vgui2 {
 struct Localize { const wchar_t* Find(const char*) { return L""; } } localizer;
 Localize* localize() { return &localizer; }
 struct ListPanel {};
 struct ListPanelItem { unsigned int userData; };
}
'''

CLASS = r'''
class CBaseGamesPage {
public:
 Map<int, serverdisplay_t> m_mapServers; Map<netadr_t, int> m_mapServerIP;
 GameList list, *m_pGameList=&list;
 EMatchMakingType m_eMatchMakingType=eInternetServer;
 HServerListRequest m_hServerRequest=nullptr;
 bool m_bAutoSelectFirstItemInGameList=false; int m_iServerRefreshCount=0;
 char m_szGameFilter[32]={};
 serverdisplay_t* GetServer(int serverID);
 void ServerResponded(int iServer, gameserveritem_t* pServerItem);
 void ApplyGameFilters();
 bool CheckPrimaryFilters(gameserveritem_t& server);
 bool CheckSecondaryFilters(gameserveritem_t&) { return true; }
 bool BShowServer(serverdisplay_t& server);
 int GetInvalidServerListID() { return -1; }
 void UpdateStatus() {} void InvalidateLayout(bool, bool) {} void Repaint() {}
 static int MapCompare(vgui2::ListPanel*, const vgui2::ListPanelItem&, const vgui2::ListPanelItem&);
 static int GameCompare(vgui2::ListPanel*, const vgui2::ListPanelItem&, const vgui2::ListPanelItem&);
 static int ServerNameCompare(vgui2::ListPanel*, const vgui2::ListPanelItem&, const vgui2::ListPanelItem&);
};
'''

TESTS = r'''
typedef int (*CompareFunc)(vgui2::ListPanel*, const vgui2::ListPanelItem&, const vgui2::ListPanelItem&);
static gameserveritem_t Server(int id, const char* name, const char* map, const char* desc) {
 gameserveritem_t server; server.m_NetAdr.Init(0x7f000001u + id, 27015, 27015);
 server.SetName(name); strcpy(server.m_szMap, map); strcpy(server.m_szGameDescription, desc);
 server.m_nAppID=70; server.m_nPlayers=4; server.m_nMaxPlayers=16; return server;
}
static int Compare(CompareFunc compare, int id1, int id2) {
 vgui2::ListPanel panel; vgui2::ListPanelItem p1{(unsigned int)id1}, p2{(unsigned int)id2};
 int result=compare(&panel, p1, p2), reverse=compare(&panel, p2, p1);
 assert((result>0)-(result<0) == -((reverse>0)-(reverse<0)));
 return result;
}
static KeyValues& Item(CBaseGamesPage& page, int id) {
 return page.list.items.at(page.GetServer(id)->m_iListID);
}

static void SanitizedStrings() {
 char buf[16];
 SanitizeServerString(buf, sizeof(buf), "\x01\x7f  Name\t\x1b ");
 assert(std::string("Name")==buf);
 SanitizeServerString(buf, sizeof(buf), "\xd0\xa1\xd0\xb5 \xe2\x9c\x93");
 assert(std::string("\xd0\xa1\xd0\xb5 \xe2\x9c\x93")==buf);
 SanitizeServerString(buf, sizeof(buf), "\x01\x02\x7f \t");
 assert(std::string("")==buf);
 SanitizeServerString(buf, 4, "\x01" "abcdef");
 assert(std::string("abc")==buf);
 puts("PASS: control characters and surrounding spaces are removed; UTF-8 is kept");
}

static void DisplayUsesSanitizedData() {
 CBaseGamesPage page; g_CurrentGamesPage=&page;
 gameserveritem_t server=Server(1, "\x01\x7f Alpha", "\x02" "de_dust", "\x1b" "Counter-Strike");
 page.ServerResponded(1, &server);
 assert(Item(page,1).values["name"]=="Alpha" && Item(page,1).values["map"]=="de_dust");
 assert(Item(page,1).values["GameDesc"]=="Counter-Strike");
 server.SetName("\x01\x02"); page.ServerResponded(1, &server);
 assert(Item(page,1).values["name"]==server.m_NetAdr.GetConnectionAddressString());
 assert(std::string(page.GetServer(1)->m_szName)==server.m_NetAdr.GetConnectionAddressString());

 // Re-adding a hidden server must use the same sanitized strings.
 steamServers[1]=Server(1, "\x03" "Bravo", "\x04" "cs_office", "");
 page.list.RemoveItem(page.GetServer(1)->m_iListID); page.GetServer(1)->m_bDoNotRefresh=true;
 page.ApplyGameFilters();
 assert(Item(page,1).values["name"]=="Bravo" && Item(page,1).values["map"]=="cs_office");
 assert(Item(page,1).values["GameDesc"]=="<localized>");
 assert(std::string(page.GetServer(1)->m_szName)=="Bravo");
 steamServers.clear(); g_CurrentGamesPage=nullptr;
 puts("PASS: initial entry, refresh and re-added entries display sanitized strings");
}

static void SortUsesSanitizedData() {
 CBaseGamesPage page; g_CurrentGamesPage=&page;
 gameserveritem_t servers[]={
  Server(1, "\x01zulu", "", ""), Server(2, "alpha", "\x01" "de_dust", "\x01Zeta"),
  Server(3, "  bravo", "cs_office", "Alpha"),
 };
 // Friends servers use negative IDs that the matchmaking interface cannot resolve.
 for (int i=0;i<3;++i) page.ServerResponded(i==2?-3:i+1, &servers[i]);
 assert(Compare(CBaseGamesPage::ServerNameCompare, 2, -3)<0);
 assert(Compare(CBaseGamesPage::ServerNameCompare, -3, 1)<0);
 assert(Compare(CBaseGamesPage::MapCompare, 2, 1)<0 && Compare(CBaseGamesPage::MapCompare, -3, 1)<0);
 assert(Compare(CBaseGamesPage::MapCompare, -3, 2)<0);
 assert(Compare(CBaseGamesPage::GameCompare, -3, 2)<0 && Compare(CBaseGamesPage::GameCompare, 2, 1)<0);
 assert(Compare(CBaseGamesPage::GameCompare, 1, 1)==0);
 assert(Compare(CBaseGamesPage::ServerNameCompare, 2, 99)>0);
 g_CurrentGamesPage=nullptr;
 puts("PASS: sort functions use sanitized strings, put empty fields last and resolve friends servers");
}

static bool Listed(EMatchMakingType type, int players, int bots, int maxPlayers) {
 CBaseGamesPage page; page.m_eMatchMakingType=type;
 gameserveritem_t server=Server(1, "Server", "map", "desc");
 server.m_nPlayers=players; server.m_nBotPlayers=bots; server.m_nMaxPlayers=maxPlayers;
 page.ServerResponded(1, &server);
 bool listed=page.list.IsValidItemID(page.GetServer(1)->m_iListID);
 assert(listed!=page.GetServer(1)->m_bDoNotRefresh);
 return listed;
}

static void CountValidation() {
 for (EMatchMakingType type : {eInternetServer, eLANServer, eFriendsServer}) {
  assert(Listed(type, 4, 2, 16) && Listed(type, 32, 32, 32) && Listed(type, 0, 0, 0));
  // sv_visiblemaxplayers reports fewer slots than connected players
  assert(Listed(type, 32, 0, 30));
  assert(!Listed(type, 4, 5, 16) && !Listed(type, -1, 0, 16) && !Listed(type, 4, -1, 16));
  assert(!Listed(type, 4, 0, -1) && !Listed(type, 4, 0, 33) && !Listed(type, 33, 0, 32));
 }
 for (EMatchMakingType type : {eSpectatorServer, eFavoritesServer, eHistoryServer}) {
  assert(Listed(type, 120, 0, 255) && Listed(type, 255, 0, 255) && Listed(type, 4, 2, 16));
  assert(!Listed(type, 4, 0, 256) && !Listed(type, 256, 0, 255) && !Listed(type, 4, 5, 16));
 }
 // Re-applying filters hides a listed server whose refreshed counts became invalid.
 CBaseGamesPage page; gameserveritem_t server=Server(1, "Server", "map", "desc");
 page.ServerResponded(1, &server); steamServers[1]=server; steamServers[1].m_nBotPlayers=9;
 page.ApplyGameFilters();
 assert(!page.list.visible[page.GetServer(1)->m_iListID] && page.GetServer(1)->m_bDoNotRefresh);
 steamServers.clear();
 puts("PASS: inconsistent counts are rejected; HLTV pages allow proxy capacity, game pages MAX_CLIENTS");
}
'''


def main():
    header = (ROOT / 'serverbrowser/BaseGamesPage.h').read_text()
    source = (ROOT / 'serverbrowser/BaseGamesPage.cpp').read_text()
    code = STUBS + re.search(r'enum EMatchMakingType\n\{.*?\n\};\n', header, re.S).group(0)
    code += CLASS
    code += re.search(r'// page currently being shown.*?\n\n(?://[^\n]*\n|static const int [^\n]*\n)*', source, re.S).group(0)
    for signature in ['static void SetServerLastPlayed(', 'static void SanitizeServerString(',
                      'static void UpdateServerStrings(', 'static int ServerStringCompare(',
                      'serverdisplay_t *CBaseGamesPage::GetServer(',
                      'void CBaseGamesPage::ServerResponded( int iServer',
                      'void CBaseGamesPage::ApplyGameFilters()',
                      'bool CBaseGamesPage::CheckPrimaryFilters(',
                      'bool CBaseGamesPage::BShowServer(',
                      'int CBaseGamesPage::MapCompare(', 'int CBaseGamesPage::GameCompare(',
                      'int CBaseGamesPage::ServerNameCompare(']:
        code += function(source, signature)
    code += TESTS
    code += '\nint main() { SanitizedStrings(); DisplayUsesSanitizedData(); SortUsesSanitizedData(); CountValidation(); }\n'
    with tempfile.TemporaryDirectory(prefix='goldsrc-serverbrowser-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code)
        executable = compile_program([path / 'test.cpp'], path / 'test', includes=[ROOT])
        subprocess.run([str(executable)], check=True, cwd=path)


if __name__ == '__main__':
    main()
