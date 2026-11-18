"""Exercise production CServerBrowserDialog::JoinGame for game list servers with stubs.

Run: python tests/serverbrowser_join_game_tests.py
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
#include <cstdio>
#include <vector>
namespace vgui2 { typedef unsigned int VPANEL; class Panel; }
class CServerBrowserDialog;
class CDialogGameInfo {
public:
 static int s_Created, s_Connects;
 unsigned int ip; uint16 queryPort, connPort; bool hltvOnly; vgui2::VPANEL parent=0; int runs=0;
 CDialogGameInfo(vgui2::Panel*, unsigned int ip_, uint16 query, uint16 conn, bool hltv=false)
  : ip(ip_), queryPort(query), connPort(conn), hltvOnly(hltv) { ++s_Created; }
 void SetParent(vgui2::VPANEL value) { parent=value; }
 void AddActionSignalTarget(CServerBrowserDialog*) {}
 void Run(const char*) { ++runs; }
 void Connect() { ++s_Connects; }
};
int CDialogGameInfo::s_Created=0, CDialogGameInfo::s_Connects=0;
struct DialogList {
 std::vector<CDialogGameInfo*> items;
 int AddToTail() { items.push_back(nullptr); return int(items.size())-1; }
 CDialogGameInfo*& operator[](int i) { return items[i]; }
};
class GameList : public IGameList {
public:
 serverdisplay_t server; bool found=false; int lookups=0, lastID=-1;
 bool SupportsItem(IGameList::InterfaceItem_e) override { return false; }
 void StartRefresh() override {} void GetNewServerList() override {} void StopRefresh() override {}
 bool IsRefreshing() override { return false; }
 serverdisplay_t* GetServer(int id) override { ++lookups; lastID=id; return found?&server:nullptr; }
 gameserveritem_t* GetServerInfo(int) override { return nullptr; }
 void OnBeginConnect() override {}
 int GetInvalidServerListID() override { return -1; }
};
class CServerBrowserDialog {
public:
 CDialogGameInfo* JoinGame(IGameList* gameList, unsigned int serverIndex, bool hltvOnly=false);
 vgui2::VPANEL GetVParent() { return 7; }
 DialogList m_GameInfoDialogs;
};
'''

TESTS = r'''
static void Reset() { CDialogGameInfo::s_Created=0; CDialogGameInfo::s_Connects=0; }

static void MissingServerIsNotJoined() {
 Reset(); CServerBrowserDialog dialog; GameList list;
 assert(nullptr==dialog.JoinGame(&list, 42, true));
 assert(1==list.lookups && 42==list.lastID);
 assert(0==CDialogGameInfo::s_Created && 0==CDialogGameInfo::s_Connects);
 assert(dialog.m_GameInfoDialogs.items.empty());
 puts("PASS: a server ID that is not in the list returns NULL without opening or connecting a dialog");
}

static void FoundServerIsJoined() {
 Reset(); CServerBrowserDialog dialog; GameList list; list.found=true;
 list.server.m_netAdr.Init(0x7f000001u, 27016, 27015);
 CDialogGameInfo* game=dialog.JoinGame(&list, 3, true);
 assert(game && 1==CDialogGameInfo::s_Created && 1==CDialogGameInfo::s_Connects);
 assert(0x7f000001u==game->ip && 27016==game->queryPort && 27015==game->connPort && game->hltvOnly);
 assert(7u==game->parent && 1==game->runs);
 assert(1==dialog.m_GameInfoDialogs.items.size() && game==dialog.m_GameInfoDialogs.items[0]);
 delete game;
 puts("PASS: a listed server opens one registered dialog and connects it");
}
'''


def main():
    source = (ROOT / 'serverbrowser/ServerBrowserDialog.cpp').read_text()
    code = STUBS + function(source, 'CDialogGameInfo *CServerBrowserDialog::JoinGame( IGameList *gameList') + TESTS
    code += '\nint main() { MissingServerIsNotJoined(); FoundServerIsJoined(); }\n'
    with tempfile.TemporaryDirectory(prefix='goldsrc-serverbrowser-join-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code)
        executable = compile_program([path / 'test.cpp'], path / 'test', includes=[ROOT],
                                     sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=path)


if __name__ == '__main__':
    main()
