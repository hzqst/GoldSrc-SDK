"""Exercise the production VGUI2 BuildModeDialog edit commands with no panel selected and with an editable panel.

Run: python tests/vgui2_build_mode_dialog_tests.py
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
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#if !defined( _WIN32 )
#define _snprintf snprintf
#endif

#define BUILD_TYPE_STRING		0
#define BUILD_TYPE_AUTORESIZE	4
#define BUILD_TYPE_CORNER		5

namespace vgui2 {
static int liveKeys = 0;

class KeyValues {
public:
 explicit KeyValues(const char* name) : name(name) { ++liveKeys; }
 void SetInt(const char* key, int value) { values[key] = std::to_string(value); }
 void SetString(const char* key, const char* value) { values[key] = value; }
 void deleteThis() { --liveKeys; delete this; }
 std::string name;
 std::map<std::string, std::string> values;
};

class Panel {
public:
 Panel(const char* name, bool editable) : name(name), editable(editable) {}
 bool IsBuildModeEditable() { return editable; }
 const char* GetName() { return name.c_str(); }
 const char* GetClassName() { return "Label"; }
 void ApplySettings(KeyValues* settings) { applied = settings->values; }
 std::string name;
 bool editable;
 std::map<std::string, std::string> applied;
};

class TextEntry {
public:
 explicit TextEntry(const char* text) : text(text) {}
 void GetText(char* buf, int size) { snprintf(buf, size, "%s", text.c_str()); }
 std::string text;
};

class Button {
public:
 void GetText(char* buf, int size) { snprintf(buf, size, "%s", ""); }
 void SetEnabled(bool state) { enabled = state; }
 bool enabled = false;
};

struct PanelItem_t {
 TextEntry* m_EditPanel;
 Button* m_EditButton;
 char m_szName[64];
 int m_iType;
};

template <typename T> struct CUtlVector {
 int Count() const { return (int)items.size(); }
 T& operator[](int i) { assert(i >= 0 && i < Count()); return items[i]; }
 std::vector<T> items;
};

class BuildGroup {
public:
 void RemoveSettings() { ++removed; }
 Panel* GetCurrentPanel() { return nullptr; }
 Panel* FieldNameTaken(const char*) { return nullptr; }
 int removed = 0;
};

class MessageBox {
public:
 MessageBox(const char*, const char*, Panel*) {}
 void DoModal(Panel*) {}
};

struct IFileSystem {};
struct ILocalize { void ReloadLocalizationFiles(IFileSystem*) {} };
static IFileSystem fileSystem;
static ILocalize localization;
IFileSystem* filesystem() { return &fileSystem; }
ILocalize* localize() { return &localization; }

// only the members the edit commands use
class BuildModeDialog {
public:
 ~BuildModeDialog() {
  if (_undoSettings) _undoSettings->deleteThis();
  if (_copySettings) _copySettings->deleteThis();
 }
 struct PanelList { CUtlVector<PanelItem_t> m_PanelList; };

 void SetActiveControl(Panel* panel) {
  m_pCurrentPanel = panel;
  if (!panel) m_pPanelList->m_PanelList.items.clear();
 }
 void UpdateControlData(Panel*) { ++updates; }
 KeyValues* StoreSettings();
 void StoreUndoSettings();
 void ApplyDataToControls();
 void OnDeletePanel();
 void DoUndo();
 void DoCopy();

 Panel* m_pCurrentPanel = nullptr;
 BuildGroup* m_pBuildGroup = nullptr;
 PanelList* m_pPanelList = nullptr;
 Button* m_pSaveButton = nullptr;
 Button* m_pApplyButton = nullptr;
 KeyValues* _undoSettings = nullptr;
 KeyValues* _copySettings = nullptr;
 char _copyClassName[255] = "";
 int updates = 0;
};
'''

TESTS = r'''
}
using namespace vgui2;

struct Fixture {
 Fixture() {
  dialog.m_pBuildGroup = &group;
  dialog.m_pPanelList = &list;
  dialog.m_pSaveButton = &save;
  dialog.m_pApplyButton = &apply;
  list.m_PanelList.items.push_back({&nameEntry, nullptr, "fieldName", BUILD_TYPE_STRING});
  list.m_PanelList.items.push_back({&wideEntry, nullptr, "wide", BUILD_TYPE_AUTORESIZE});
 }
 TextEntry nameEntry{"MyLabel"};
 TextEntry wideEntry{"64"};
 BuildGroup group;
 BuildModeDialog::PanelList list;
 Button save, apply;
 BuildModeDialog dialog;
};

static void CommandsWithNothingSelectedDoNothing() {
 {
  Fixture f;
  Panel old("Old", true);
  f.dialog.SetActiveControl(&old);
  f.dialog.StoreUndoSettings();
  f.dialog.DoCopy();
  assert(nullptr != f.dialog._undoSettings);
  KeyValues* copy = f.dialog._copySettings;
  assert(nullptr != copy);

  f.dialog.SetActiveControl(nullptr);
  assert(0 == f.list.m_PanelList.Count());
  f.dialog.ApplyDataToControls();
  f.dialog.DoUndo();
  f.dialog.DoCopy();
  f.dialog.OnDeletePanel();
  assert(copy == f.dialog._copySettings);
  assert(0 == f.group.removed);
  assert(0 == f.dialog.updates);
  assert(old.applied.empty());
  f.dialog.StoreUndoSettings();
  assert(nullptr == f.dialog._undoSettings);
 }
 assert(0 == liveKeys);
 puts("PASS: apply, undo, copy, delete and store undo with nothing selected leave the dialog and the last copy alone");
}

static void DeletingWithoutUndoSettingsRemovesThePanel() {
 {
  Fixture f;
  Panel panel("MyLabel", true);
  f.dialog.SetActiveControl(&panel);
  assert(nullptr == f.dialog._undoSettings);
  f.dialog.OnDeletePanel();
  assert(1 == f.group.removed);
  assert(nullptr == f.dialog.m_pCurrentPanel);
  assert(nullptr == f.dialog._undoSettings);
  assert(f.save.enabled);
 }
 assert(0 == liveKeys);
 puts("PASS: deleting an editable panel before any undo settings were stored removes it");
}

static void ApplyAndUndoChangeTheSelectedPanel() {
 {
  Fixture f;
  Panel panel("MyLabel", true);
  f.dialog.SetActiveControl(&panel);
  f.dialog.StoreUndoSettings();
  f.wideEntry.text = "128";
  f.dialog.ApplyDataToControls();
  assert(std::string("128") == panel.applied["wide"]);
  assert(f.save.enabled);
  assert(!f.apply.enabled);

  f.dialog.DoUndo();
  assert(std::string("64") == panel.applied["wide"]);
  assert(nullptr == f.dialog._undoSettings);
  assert(1 == f.dialog.updates);
 }
 assert(0 == liveKeys);
 puts("PASS: apply stores the edited settings on the selected panel and undo restores the stored ones");
}

static void ApplyOnAReadOnlyPanelOnlyRefreshesTheControls() {
 Fixture f;
 Panel panel("Locked", false);
 f.dialog.SetActiveControl(&panel);
 f.dialog.ApplyDataToControls();
 assert(panel.applied.empty());
 assert(1 == f.dialog.updates);
 puts("PASS: apply on a panel that is not build mode editable only refreshes the controls");
}

int main() {
 CommandsWithNothingSelectedDoNothing();
 DeletingWithoutUndoSettingsRemovesThePanel();
 ApplyAndUndoChangeTheSelectedPanel();
 ApplyOnAReadOnlyPanelOnlyRefreshesTheControls();
}
'''

SIGNATURES = (
    'void BuildModeDialog::DoUndo()',
    'KeyValues* BuildModeDialog::StoreSettings()',
    'void BuildModeDialog::DoCopy()',
    'void BuildModeDialog::StoreUndoSettings()',
    'void BuildModeDialog::OnDeletePanel()',
    'void BuildModeDialog::ApplyDataToControls()',
)


def main():
    source = (ROOT / 'vgui2/controls/BuildModeDialog.cpp').read_text(encoding='utf-8')
    code = STUBS + ''.join(function(source, signature) + '\n' for signature in SIGNATURES) + TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-buildmode-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
