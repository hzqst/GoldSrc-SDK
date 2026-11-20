"""Exercise production scoreboard refreshes selecting the local player row and escaping player names."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'

PRELUDE = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
struct KeyValues {
 int index = 0;
 std::string name;
 explicit KeyValues(const char*) {}
 void SetString(const char*, const char* value) { name = value; }
 const char* GetString(const char*, const char*) { return name.c_str(); }
 KeyValues* FindKey(int) { return this; }
 int GetInt(const char*, int) { return index; }
 void deleteThis() { delete this; }
};
struct Row { bool valid; KeyValues data; int section; };
struct ListPanel {
 std::vector<Row> rows;
 int selected = -1;
 int selectCalls = 0;
 int GetHighestItemID() { return (int)rows.size() - 1; }
 bool IsItemIDValid(int item) { return item >= 0 && item < (int)rows.size() && rows[item].valid; }
 KeyValues* GetItemData(int item) { return &rows[item].data; }
 int AddItem(int section, const KeyValues* data) { rows.push_back({true, *data, section}); return (int)rows.size() - 1; }
 bool ModifyItem(int item, int section, const KeyValues* data) { rows[item].data = *data; rows[item].section = section; return true; }
 void RemoveItem(int item) { rows[item].valid = false; }
 void SetItemFgColor(int, int) {}
 void SetSelectedItem(int item) { selected = item; ++selectCalls; }
};
struct VGuiLibraryPlayer_t { const char* name; bool thisplayer; int teamnumber; };
const int TestPlayers = 3;
VGuiLibraryPlayer_t players[TestPlayers + 1];
struct ClientDll {
 int GetMaxPlayers() { return TestPlayers; }
 VGuiLibraryPlayer_t CallEnghudGetPlayerInfo(int player) { return players[player]; }
};
struct TeamFortressViewport {
 ClientDll client;
 ClientDll* GetClientDllInterface() { return &client; }
 void MakeSafeName(const char* oldName, char* newName, int capacity);
} viewport, *gViewPortInterface = &viewport;
template <typename T> struct CUtlVector {
 std::vector<T> items;
 void AddMultipleToTail(int count) { items.resize(items.size() + count); }
 T* Base() { return items.data(); }
};
struct DialogBase {
 ListPanel list;
 ListPanel* m_pPlayerList = &list;
 int m_iSectionId = -1;
 int m_iPlayerIndexSymbol = 0;
 void GetPlayerScoreInfo(int player, KeyValues* data) { data->index = player; data->SetString("name", players[player].name); }
 int GetTeamColor(int) { return 0; }
 int FindItemIDForPlayerIndex(int player);
};
int DialogBase::FindItemIDForPlayerIndex(int player) {
 for (int item = 0; item <= list.GetHighestItemID(); ++item)
  if (list.IsItemIDValid(item) && list.rows[item].data.index == player)
   return item;
 return -1;
}
template <typename Dialog> void ExerciseNames() {
 players[1] = {"&&", false, 1};
 players[2] = {"", true, 2};
 players[3] = {nullptr, false, 0};
 Dialog dialog;
 dialog.UpdatePlayerInfo();
 assert(std::string("&&&&") == dialog.list.rows[0].data.name);
 assert(std::string("") == dialog.list.rows[1].data.name);
}
template <typename Dialog> void Exercise() {
 players[1] = {"other", false, 1};
 players[2] = {"local", true, 2};
 players[3] = {nullptr, false, 0};
 Dialog dialog;
 dialog.UpdatePlayerInfo();
 assert(2u == dialog.list.rows.size());
 assert(1 == dialog.list.selectCalls);
 assert(1 == dialog.list.selected);
 dialog.UpdatePlayerInfo();
 assert(2u == dialog.list.rows.size());
 assert(2 == dialog.list.selectCalls);
 assert(1 == dialog.list.selected);
 players[2].thisplayer = false;
 dialog.UpdatePlayerInfo();
 assert(2 == dialog.list.selectCalls);
}
'''

GAME_CONTROLS_STUBS = r'''
namespace game_controls {
struct CClientScoreBoardDialog : DialogBase {
 int FindItemIDForPlayerIndex(int playerIndex);
 void UpdatePlayerInfo();
};
'''

CZEROR_STUBS = r'''
namespace czeror {
#define MAX_PLAYERS 32
struct EngineFuncs {
 int GetMaxClients() { return TestPlayers; }
 void* GetEntityByIndex(int player) { return &players[player]; }
} gEngfuncs;
VGuiLibraryPlayer_t* g_PlayerInfoList = players;
VGuiLibraryPlayer_t* g_PlayerExtraInfo = players;
struct CClientScoreBoardDialog : DialogBase {
 int FindItemIDForPlayerIndex(int player);
 void UpdatePlayerInfo();
};
'''

DOD_STUBS = r'''
namespace dod {
struct CDoDClientScoreBoardDialog : DialogBase { void UpdatePlayerInfo(); };
'''

CSTRIKE_STUBS = r'''
namespace cstrike {
struct CCSClientScoreBoardDialog : DialogBase {
 void UpdatePlayerAvatar(int, KeyValues*) {}
 void UpdatePlayerInfo();
};
void CCSClientScoreBoardDialog::UpdatePlayerInfo()
{
'''

NAMESPACE_END = '}\n'

MAIN = r'''
int main() {
 Exercise<game_controls::CClientScoreBoardDialog>();
 Exercise<czeror::CClientScoreBoardDialog>();
 Exercise<dod::CDoDClientScoreBoardDialog>();
 Exercise<cstrike::CCSClientScoreBoardDialog>();
 ExerciseNames<game_controls::CClientScoreBoardDialog>();
 ExerciseNames<dod::CDoDClientScoreBoardDialog>();
 ExerciseNames<cstrike::CCSClientScoreBoardDialog>();
}
'''


def read(path):
    return (ROOT / path).read_text(encoding='utf-8')


def function(text, signature):
    start = text.index(signature)
    return text[start:text.index('\n}', start) + 2] + '\n'


def section(text, first, last):
    # The CS refresh also lays out headers and Career rows, so only its player row loop is exercised.
    start = text.index(first)
    return text[start:text.index(last, start) + len(last)] + '\n}\n'


def main():
    game_controls = read('vgui2/game_controls/ClientScoreBoardDialog.cpp')
    czeror = read('czeror/cl_dll/vgui/hl_base/ClientScoreBoardDialog.cpp')
    dod = read('dod/cl_dll/vgui/dodclientscoreboard.cpp')
    cstrike = read('cstrike/cl_dll/VGUI/cstrikeclientscoreboard.cpp')
    cstrike_update = function(cstrike, 'void CCSClientScoreBoardDialog::UpdatePlayerInfo(')
    with tempfile.TemporaryDirectory(prefix='goldsrc-scoreboard-selection-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text('\n'.join((
            PRELUDE,
            function(read('vgui2/game_controls/vgui_TeamFortressViewport.cpp'), 'void TeamFortressViewport::MakeSafeName('),
            GAME_CONTROLS_STUBS,
            function(game_controls, 'int CClientScoreBoardDialog::FindItemIDForPlayerIndex('),
            function(game_controls, 'void CClientScoreBoardDialog::UpdatePlayerInfo('),
            NAMESPACE_END,
            CZEROR_STUBS,
            function(czeror, 'int CClientScoreBoardDialog::FindItemIDForPlayerIndex('),
            function(czeror, 'void CClientScoreBoardDialog::UpdatePlayerInfo('),
            NAMESPACE_END,
            DOD_STUBS,
            function(dod, 'void CDoDClientScoreBoardDialog::UpdatePlayerInfo('),
            NAMESPACE_END,
            CSTRIKE_STUBS,
            section(cstrike_update, '\tint selectedItem = -1;', 'm_pPlayerList->SetSelectedItem( selectedItem );'),
            NAMESPACE_END,
            MAIN)))
        executable = compile_program([path / 'test.cpp'], path / 'test')
        subprocess.run([str(executable)], check=True, cwd=path)


if __name__ == '__main__':
    main()
