"""Exercise the production VGUI2 BuildModeDialog selection, edit, undo and copy/paste commands.

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
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#if !defined( _WIN32 )
#include <strings.h>
#define _snprintf snprintf
#define _stricmp strcasecmp
#endif

#define BUILD_TYPE_STRING		0
#define BUILD_TYPE_ALIGNMENT	3
#define BUILD_TYPE_AUTORESIZE	4
#define BUILD_TYPE_CORNER		5
#define BUILD_TYPE_LOCALIZE		6

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
 explicit Panel(const char* name = "", bool editable = true) : name(name), editable(editable) {}
 virtual ~Panel() {}
 bool IsBuildModeEditable() { return editable; }
 bool IsBuildModeDeletable() { return true; }
 const char* GetName() { return name.c_str(); }
 void SetName(const char* value) { name = value; }
 const char* GetClassName() { return "Label"; }
 const char* GetDescription() { return "string fieldName, string wide"; }
 void ApplySettings(KeyValues* settings) { applied = settings->values; }
 void SetParent(Panel*) {}
 void AddActionSignalTarget(Panel*) {}
 void SetTabPosition(int) {}
 void SetSize(int, int) {}
 void SetPos(int x, int y) { posX = x; posY = y; }
 void ScreenToLocal(int&, int&) {}
 void InvalidateLayout(bool, bool) {}
 void Repaint() {}
 int GetProportionalScaledValue(int value) { return value; }
 std::string name;
 bool editable;
 int posX = 0, posY = 0;
 std::map<std::string, std::string> applied;
};

class Label : public Panel {
public:
 enum Alignment { a_east };
 enum ColorState { CS_DULL };
 Label(Panel*, const char*, const char*) {}
 void SetContentAlignment(Alignment) {}
 void SetAssociatedControl(Panel*) {}
 void SetText(const char*) {}
 void SetTextColorState(ColorState) {}
};

class TextEntry : public Panel {
public:
 TextEntry(Panel*, const char*) {}
 void GetText(char* buf, int size) { snprintf(buf, size, "%s", text.c_str()); }
 void SetText(const char* value) { text = value; }
 std::string text;
};

class ComboBox : public TextEntry {
public:
 ComboBox(Panel* parent, const char* name, int, bool) : TextEntry(parent, name) {}
 void AddItem(const char*, KeyValues*) {}
};

class Button : public Panel {
public:
 Button() {}
 Button(Panel*, const char*, const char*) {}
 void GetText(char* buf, int size) { snprintf(buf, size, "%s", ""); }
 void SetEnabled(bool state) { enabled = state; }
 bool enabled = false;
};

struct PanelListPanel {
 void MoveScrollBarToTop() {}
 void AddItem(Panel*, Panel*) {}
 void RemoveAll() {}
};

int EditorControlHeight(Panel*) { return 0; }

struct PanelItem_t {
 Panel* m_EditLabel;
 TextEntry* m_EditPanel;
 ComboBox* m_pCombo;
 Button* m_EditButton;
 char m_szName[64];
 int m_iType;
};

template <typename T> struct CUtlVector {
 int Count() const { return (int)items.size(); }
 T& operator[](int i) { assert(i >= 0 && i < Count()); return items[i]; }
 void AddToTail(const T& item) { items.push_back(item); }
 void RemoveAll() { items.clear(); }
 std::vector<T> items;
};

class BuildGroup {
public:
 void RemoveSettings() { ++removed; }
 Panel* GetCurrentPanel() { return nullptr; }
 Panel* GetContextPanel() { return &context; }
 Panel* FieldNameTaken(const char*) { return nullptr; }
 const char* GetResourceName() { return "resource/test.res"; }
 void GetNewFieldName(char* name, Panel*) { strcpy(name, "Label1"); }
 Panel context;
 int removed = 0;
};

class MessageBox {
public:
 MessageBox(const char*, const char*, Panel*) {}
 void DoModal(Panel*) {}
};

struct IInput { void GetCursorPos(int& x, int& y) { x = 10; y = 20; } };
struct IFileSystem {};
struct ILocalize { void ReloadLocalizationFiles(IFileSystem*) {} };
static IInput inputs;
static IFileSystem fileSystem;
static ILocalize localization;
IInput* input() { return &inputs; }
IFileSystem* filesystem() { return &fileSystem; }
ILocalize* localize() { return &localization; }

// only the members the tested functions use
class BuildModeDialog : public Panel {
public:
 struct PanelList {
  void AddItem(Panel* label, TextEntry* edit, ComboBox* combo, Button* button, const char* name, int type);
  void RemoveAll();
  CUtlVector<PanelItem_t> m_PanelList;
  PanelListPanel* m_pControls;
 };

 ~BuildModeDialog() override {
  RemoveAllControls();
  if (_undoSettings) _undoSettings->deleteThis();
  if (_copySettings) _copySettings->deleteThis();
  for (Panel* panel : newPanels) delete panel;
 }
 void SetActiveControl(Panel* controlToEdit);
 void UpdateControlData(Panel*) { ++updates; }
 void RemoveAllControls();
 Panel* OnNewControl(const char* name, int, int) {
  newClassNames.push_back(name);
  newPanels.push_back(new Panel(name));
  return newPanels.back();
 }
 KeyValues* StoreSettings();
 void StoreUndoSettings();
 void ApplyDataToControls();
 void OnDeletePanel();
 void DoUndo();
 void DoCopy();
 void DoPaste();

 Panel* m_pCurrentPanel = nullptr;
 BuildGroup* m_pBuildGroup = nullptr;
 Label* m_pStatusLabel = nullptr;
 ComboBox* m_pFileSelectionCombo = nullptr;
 PanelList* m_pPanelList = nullptr;
 Button* m_pSaveButton = nullptr;
 Button* m_pApplyButton = nullptr;
 Button* m_pDeleteButton = nullptr;
 KeyValues* _undoSettings = nullptr;
 KeyValues* _copySettings = nullptr;
 char _copyClassName[255] = "";
 int updates = 0;
 std::vector<std::string> newClassNames;
 std::vector<Panel*> newPanels;
};
} // namespace vgui2
'''

TESTS = r'''
}
using namespace vgui2;

struct Fixture {
 Fixture() {
  list.m_pControls = &controls;
  dialog.m_pBuildGroup = &group;
  dialog.m_pStatusLabel = &status;
  dialog.m_pFileSelectionCombo = &files;
  dialog.m_pPanelList = &list;
  dialog.m_pSaveButton = &save;
  dialog.m_pApplyButton = &apply;
  dialog.m_pDeleteButton = &remove;
 }
 // the edit field of the selected panel's property, filled the way UpdateControlData would
 TextEntry& Field(int i) { return *list.m_PanelList[i].m_EditPanel; }
 void Select(Panel* panel) {
  dialog.SetActiveControl(panel);
  if (panel) {
   Field(0).SetText(panel->GetName());
   Field(1).SetText("64");
  }
 }
 PanelListPanel controls;
 BuildGroup group;
 Label status{nullptr, nullptr, nullptr};
 ComboBox files{nullptr, nullptr, 0, false};
 BuildModeDialog::PanelList list;
 Button save, apply, remove;
 BuildModeDialog dialog;
};

static void CommandsWithNothingSelectedDoNothing() {
 {
  Fixture f;
  Panel old("Old", true);
  f.Select(&old);
  f.dialog.StoreUndoSettings();
  f.dialog.DoCopy();
  assert(nullptr != f.dialog._undoSettings);
  KeyValues* copy = f.dialog._copySettings;
  assert(nullptr != copy);

  f.Select(nullptr);
  assert(0 == f.list.m_PanelList.Count());
  assert(nullptr == f.dialog._undoSettings);
  int updates = f.dialog.updates;
  f.dialog.ApplyDataToControls();
  f.dialog.DoUndo();
  f.dialog.DoCopy();
  f.dialog.OnDeletePanel();
  f.dialog.StoreUndoSettings();
  assert(copy == f.dialog._copySettings);
  assert(nullptr == f.dialog._undoSettings);
  assert(0 == f.group.removed);
  assert(updates == f.dialog.updates);
  assert(old.applied.empty());
 }
 assert(0 == liveKeys);
 puts("PASS: apply, undo, copy, delete and store undo with nothing selected leave the dialog and the last copy alone");
}

static void DeletingWithoutUndoSettingsRemovesThePanel() {
 {
  Fixture f;
  Panel panel("MyLabel", true);
  f.Select(&panel);
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
  f.Select(&panel);
  f.dialog.StoreUndoSettings();
  // selecting the same panel again, as a mouse press does, keeps its undo settings
  f.dialog.SetActiveControl(&panel);
  assert(nullptr != f.dialog._undoSettings);

  f.Field(1).SetText("128");
  f.dialog.ApplyDataToControls();
  assert(std::string("128") == panel.applied["wide"]);
  assert(f.save.enabled);
  assert(!f.apply.enabled);

  f.dialog.DoUndo();
  assert(std::string("64") == panel.applied["wide"]);
  assert(nullptr == f.dialog._undoSettings);
 }
 assert(0 == liveKeys);
 puts("PASS: apply stores the edited settings on the selected panel and undo restores the stored ones");
}

static void UndoAfterSelectingAnotherPanelLeavesItAlone() {
 {
  Fixture f;
  Panel first("First", true), second("Second", true);
  f.Select(&first);
  f.dialog.StoreUndoSettings();
  f.Select(&second);
  assert(nullptr == f.dialog._undoSettings);
  f.dialog.DoUndo();
  assert(second.applied.empty());
  assert(first.applied.empty());
 }
 assert(0 == liveKeys);
 puts("PASS: undo after selecting another panel does not apply the previous panel's settings to it");
}

static void ApplyOnAReadOnlyPanelOnlyRefreshesTheControls() {
 Fixture f;
 Panel panel("Locked", false);
 f.Select(&panel);
 int updates = f.dialog.updates;
 f.dialog.ApplyDataToControls();
 assert(panel.applied.empty());
 assert(updates + 1 == f.dialog.updates);
 puts("PASS: apply on a panel that is not build mode editable only refreshes the controls");
}

static void PastingBeforeAnythingWasCopiedDoesNothing() {
 {
  Fixture f;
  f.dialog.DoPaste();
  assert(f.dialog.newClassNames.empty());
 }
 assert(0 == liveKeys);
 puts("PASS: paste before anything was copied creates no panel");
}

static void PastingACopyCreatesAPanelWithItsSettings() {
 {
  Fixture f;
  Panel panel("MyLabel", true);
  f.Select(&panel);
  f.dialog.DoCopy();
  f.dialog.DoPaste();
  assert(1 == f.dialog.newPanels.size());
  Panel* pasted = f.dialog.newPanels[0];
  assert(std::string("Label") == f.dialog.newClassNames[0]);
  assert(std::string("64") == pasted->applied["wide"]);
  assert(std::string("Label1") == pasted->GetName());
  assert(10 == pasted->posX && 20 == pasted->posY);
 }
 assert(0 == liveKeys);
 puts("PASS: paste after a copy creates a panel of the copied class with the copied settings");
}

int main() {
 CommandsWithNothingSelectedDoNothing();
 DeletingWithoutUndoSettingsRemovesThePanel();
 ApplyAndUndoChangeTheSelectedPanel();
 UndoAfterSelectingAnotherPanelLeavesItAlone();
 ApplyOnAReadOnlyPanelOnlyRefreshesTheControls();
 PastingBeforeAnythingWasCopiedDoesNothing();
 PastingACopyCreatesAPanelWithItsSettings();
}
'''

FREE_FUNCTIONS = (
    'const char* ParseTokenFromString( const char** string )',
)

MEMBER_FUNCTIONS = (
    'void BuildModeDialog::DoUndo()',
    'KeyValues* BuildModeDialog::StoreSettings()',
    'void BuildModeDialog::DoCopy()',
    'void BuildModeDialog::StoreUndoSettings()',
    'void BuildModeDialog::OnDeletePanel()',
    'void BuildModeDialog::DoPaste()',
    'void BuildModeDialog::PanelList::AddItem(',
    'void BuildModeDialog::PanelList::RemoveAll()',
    'void BuildModeDialog::RemoveAllControls()',
    'void BuildModeDialog::SetActiveControl( Panel* controlToEdit )',
    'void BuildModeDialog::ApplyDataToControls()',
)


def main():
    source = (ROOT / 'vgui2/controls/BuildModeDialog.cpp').read_text(encoding='utf-8')
    code = STUBS + ''.join(function(source, signature) + '\n' for signature in FREE_FUNCTIONS)
    code += 'namespace vgui2 {\n' + ''.join(function(source, signature) + '\n' for signature in MEMBER_FUNCTIONS) + TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-buildmode-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
