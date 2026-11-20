"""Exercise the production GameUI CPlayerListDialog::OnItemSelected when the selected row has no data, which disables the mute button instead of dereferencing NULL, and with real, bot, disconnected and muted players (#480).

Run: python tests/gameui_player_list_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

#ifdef _WIN32
#define stricmp _stricmp
#else
#include <strings.h>
#define stricmp strcasecmp
#endif

// only the members the extracted function reads; each records what it was given
class KeyValues {
public:
 int index;
 int GetInt(const char* name) { assert(!strcmp("index", name)); return index; }
};

namespace vgui2 {
class ListPanel {
public:
 int selectedCount;
 KeyValues* item;
 int GetSelectedItemsCount() { return selectedCount; }
 int GetSelectedItem(int selectionIndex) { assert(0 == selectionIndex); return 7; }
 KeyValues* GetItem(int itemID) { assert(7 == itemID); return item; }
};

class Button {
public:
 bool enabled;
 std::string text;
 void SetEnabled(bool state) { enabled = state; }
 void SetText(const char* value) { text = value; }
};
}

static const char* botValue;
static const char* nameValue;
static int engineCalls;

static const char* PlayerInfo_ValueForKey(int playerIndex, const char* key) {
 assert(3 == playerIndex);
 ++engineCalls;
 if (!strcmp("*bot", key))
  return botValue;
 assert(!strcmp("name", key));
 return nameValue;
}

struct cl_enginefuncs_s {
 const char* (*PlayerInfo_ValueForKey)(int playerIndex, const char* key);
};
static cl_enginefuncs_s engineFuncs = { PlayerInfo_ValueForKey };
static cl_enginefuncs_s* engine = &engineFuncs;

class IGameClientExports {
public:
 bool muted;
 bool IsPlayerGameVoiceMuted(int playerIndex) { assert(3 == playerIndex); return muted; }
};
static IGameClientExports exports;
static IGameClientExports* exportsPointer;
static IGameClientExports* GameClientExports() { return exportsPointer; }

// only the members the extracted function uses
class CPlayerListDialog {
public:
 void OnItemSelected();
 void RefreshPlayerProperties() { ++refreshes; }

 vgui2::ListPanel* m_pPlayerList;
 vgui2::Button* m_pMuteButton;
 int refreshes = 0;
};
'''

TESTS = r'''
static vgui2::ListPanel list;
static vgui2::Button button;
static KeyValues player;

static CPlayerListDialog Reset(int selectedCount, KeyValues* item) {
 list.selectedCount = selectedCount;
 list.item = item;
 button.enabled = true;
 button.text = "stale";
 player.index = 3;
 botValue = nullptr;
 nameValue = "Player";
 engineCalls = 0;
 exports.muted = false;
 exportsPointer = &exports;
 CPlayerListDialog dialog;
 dialog.m_pPlayerList = &list;
 dialog.m_pMuteButton = &button;
 return dialog;
}

static void AssertDisabledMute() {
 assert(!button.enabled);
 assert(std::string("#GameUI_MuteIngameVoice") == button.text);
}

static void NoSelectionDisablesTheButton() {
 CPlayerListDialog dialog = Reset(0, &player);
 dialog.OnItemSelected();
 assert(1 == dialog.refreshes);
 AssertDisabledMute();
 assert(0 == engineCalls);
 puts("PASS: CPlayerListDialog::OnItemSelected disables the mute button when nothing is selected");
}

static void SelectedRowWithoutDataDisablesTheButton() {
 CPlayerListDialog dialog = Reset(1, nullptr);
 dialog.OnItemSelected();
 AssertDisabledMute();
 assert(0 == engineCalls);
 puts("PASS: CPlayerListDialog::OnItemSelected disables the mute button instead of dereferencing a NULL selected row");
}

static void PlayerEnablesMute() {
 CPlayerListDialog dialog = Reset(1, &player);
 dialog.OnItemSelected();
 assert(button.enabled);
 assert(std::string("#GameUI_MuteIngameVoice") == button.text);
 puts("PASS: CPlayerListDialog::OnItemSelected enables Mute for a connected player");
}

static void MutedPlayerEnablesUnmute() {
 CPlayerListDialog dialog = Reset(1, &player);
 exports.muted = true;
 dialog.OnItemSelected();
 assert(button.enabled);
 assert(std::string("#GameUI_UnmuteIngameVoice") == button.text);
 puts("PASS: CPlayerListDialog::OnItemSelected enables Unmute for a muted player");
}

static void BotDisablesTheButton() {
 CPlayerListDialog dialog = Reset(1, &player);
 botValue = "1";
 exports.muted = true;
 dialog.OnItemSelected();
 AssertDisabledMute();
 puts("PASS: CPlayerListDialog::OnItemSelected disables the mute button for a bot");
}

static void DisconnectedPlayerDisablesTheButton() {
 CPlayerListDialog dialog = Reset(1, &player);
 nameValue = nullptr;
 dialog.OnItemSelected();
 AssertDisabledMute();
 puts("PASS: CPlayerListDialog::OnItemSelected disables the mute button for a disconnected player");
}

static void NoGameClientExportsDisablesTheButton() {
 CPlayerListDialog dialog = Reset(1, &player);
 exportsPointer = nullptr;
 dialog.OnItemSelected();
 AssertDisabledMute();
 puts("PASS: CPlayerListDialog::OnItemSelected disables the mute button without client exports");
}

int main() {
 NoSelectionDisablesTheButton();
 SelectedRowWithoutDataDisablesTheButton();
 PlayerEnablesMute();
 MutedPlayerEnablesUnmute();
 BotDisablesTheButton();
 DisconnectedPlayerDisablesTheButton();
 NoGameClientExportsDisablesTheButton();
}
'''


def main():
    source = (ROOT / 'GameUI/PlayerListDialog.cpp').read_text(encoding='utf-8')
    code = STUBS + function(source, 'void CPlayerListDialog::OnItemSelected()') + '\n' + TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-gameui-player-list-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
