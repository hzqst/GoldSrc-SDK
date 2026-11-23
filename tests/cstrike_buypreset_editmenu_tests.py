"""Regression tests for CBuyPresetEditMenu::OnCommand with a NULL edit preset (#505)."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

SDK_ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'


def extract(relpath, signature):
    source = (SDK_ROOT / relpath).read_text(encoding='utf-8')
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifndef _WIN32
#define _snprintf snprintf
#define stricmp strcasecmp
#else
#define stricmp _stricmp
#endif

static constexpr int REOPEN_DONTCHANGE = 2;

static std::string g_lastToken;

static const char* SharedParse(const char* data) {
    g_lastToken.clear();
    if (!data)
        return nullptr;
    while (*data && std::isspace(static_cast<unsigned char>(*data)))
        ++data;
    if (!*data)
        return nullptr;
    while (*data && !std::isspace(static_cast<unsigned char>(*data))) {
        g_lastToken.push_back(*data);
        ++data;
    }
    return data;
}

static const char* SharedGetToken() {
    return g_lastToken.c_str();
}

struct BuyPreset {
    std::wstring name;
    int numSets = 3;
    int swapCalls = 0;
    int lastSwapA = -1;
    int lastSwapB = -1;

    void SetName(const wchar_t* n) { name = n ? n : L""; }
    int GetNumSets() const { return numSets; }
    void SwapSet(int a, int b) { ++swapCalls; lastSwapA = a; lastSwapB = b; }
};

struct BuyPresetManager {
    BuyPreset* currentPreset = nullptr;
    BuyPreset* GetEditPreset(int) { return currentPreset; }
};

static BuyPresetManager g_buyPresets;
static BuyPresetManager* TheBuyPresets = &g_buyPresets;
static int thePresetIndex = 0;

static int g_showMainMenuCalls = 0;
static std::vector<std::string> g_frameCommands;
static int g_popupShown = 0;

static void ShowBuyPresetMainMenu(bool, int) {
    ++g_showMainMenuCalls;
}

struct Frame {
    void OnCommand(const char* command) {
        g_frameCommands.emplace_back(command ? command : "");
    }
};

struct TextEntry {
    std::wstring text = L"MyPreset";
    void GetText(wchar_t* buf, int bufLen) {
        int i = 0;
        for (; i + 1 < bufLen && i < static_cast<int>(text.size()); ++i)
            buf[i] = text[i];
        if (bufLen > 0)
            buf[i] = L'\0';
    }
};

struct WeaponSet {};

struct CCareerBaseBox {
    virtual ~CCareerBaseBox() = default;
    void AddActionSignalTarget(void*) {}
    void ShowWindow() { ++g_popupShown; delete this; }
};

struct CCareerQueryBox : CCareerBaseBox {
    CCareerQueryBox(void*, const char*) {}
};
struct CWeaponSelectBox : CCareerBaseBox {
    CWeaponSelectBox(void*, WeaponSet*, bool) {}
};
struct CGrenadeSelectBox : CCareerBaseBox {
    CGrenadeSelectBox(void*, WeaponSet*) {}
};
struct CEquipmentSelectBox : CCareerBaseBox {
    CEquipmentSelectBox(void*, WeaponSet*) {}
};

struct CBuyPresetEditMenu : Frame {
    bool m_isDirty = false;
    int m_selectedIndex = -1;
    BuyPreset m_originalBuyPreset;
    WeaponSet m_weaponSet;
    TextEntry m_nameEntry;
    TextEntry* m_pPresetName = &m_nameEntry;
    int updateButtonsCalls = 0;
    int editFallbackCalls = 0;
    int finishEditingCalls = 0;

    void UpdateButtons() { ++updateButtonsCalls; }
    void EditFallback(int) { ++editFallbackCalls; }
    void FinishEditingWeaponSet() { ++finishEditingCalls; }
    void OnCommand(const char* command);
};
'''

TESTS = r'''
static void ResetCounters() {
    g_showMainMenuCalls = 0;
    g_frameCommands.clear();
    g_popupShown = 0;
}

int main() {
    // 1. NULL edit preset: every command that touches preset must close to main menu safely.
    {
        TheBuyPresets->currentPreset = nullptr;
        const char* commands[] = {
            "open_edit_main_menu save",
            "open_edit_main_menu",
            "move up 1",
            "move down 0",
            "vguicancel",
            "close_with_save_check",
            "primary 0",
            "popup_ok",
        };
        for (const char* cmd : commands) {
            ResetCounters();
            CBuyPresetEditMenu menu;
            menu.m_isDirty = true;
            menu.m_selectedIndex = 1;
            menu.OnCommand(cmd);
            assert(1 == g_showMainMenuCalls);
            assert(1 == g_frameCommands.size());
            assert("vguicancel" == g_frameCommands[0]);
            assert(0 == g_popupShown);
            assert(0 == menu.editFallbackCalls);
            assert(0 == menu.finishEditingCalls);
        }
    }

    // 2. NULL edit preset with Cancel / Cancel clear still updates buttons and forwards Cancel.
    {
        TheBuyPresets->currentPreset = nullptr;
        ResetCounters();
        CBuyPresetEditMenu menu;
        menu.m_selectedIndex = 2;
        menu.OnCommand("Cancel clear");
        assert(-1 == menu.m_selectedIndex);
        assert(1 == menu.updateButtonsCalls);
        assert(0 == g_showMainMenuCalls);
        assert(1 == g_frameCommands.size());
        assert("Cancel clear" == g_frameCommands[0]);
    }

    // 3. Valid edit preset: save, move, and vguicancel behave normally.
    {
        BuyPreset active;
        active.name = L"Edited";
        active.numSets = 3;
        TheBuyPresets->currentPreset = &active;

        ResetCounters();
        CBuyPresetEditMenu menu;
        menu.m_originalBuyPreset.name = L"Original";
        menu.m_nameEntry.text = L"SavedName";
        menu.OnCommand("open_edit_main_menu save");
        assert(L"SavedName" == active.name);
        assert(1 == g_showMainMenuCalls);

        ResetCounters();
        menu.m_selectedIndex = 1;
        menu.OnCommand("move up 1");
        assert(1 == active.swapCalls);
        assert(1 == active.lastSwapA);
        assert(0 == active.lastSwapB);

        ResetCounters();
        menu.OnCommand("vguicancel");
        assert(L"Original" == active.name);
        assert(1 == g_showMainMenuCalls);
    }

    return 0;
}
'''


def main():
    production = extract(
        'cstrike/cl_dll/VGUI/buypreset_editmenu.cpp',
        'void CBuyPresetEditMenu::OnCommand( const char* command )',
    )
    with tempfile.TemporaryDirectory(prefix='goldsrc-buypreset-editmenu-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
    print('PASS: CBuyPresetEditMenu::OnCommand handles NULL edit preset safely')


if __name__ == '__main__':
    main()
