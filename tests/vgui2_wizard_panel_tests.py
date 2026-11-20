"""Exercise the production VGUI2 WizardPanel::Run, ActivateNextSubPanel, OnPrevButton and ActivatePrevSubPanel with no current or no valid sub panel, and the normal forward and back navigation through the history (#483).

Run: python tests/vgui2_wizard_panel_tests.py
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
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>

namespace vgui2 {
// counts the errors DPrintf reports
struct IVGui {
 void DPrintf(const char* format) { ++errors; printf("%s", format); }
 int errors = 0;
};
static IVGui gui;
static IVGui* ivgui() { return &gui; }

// only the members the wizard navigation uses
template <class T> class Dar {
public:
 int GetCount() { return static_cast<int>(elements.size()); }
 void AddElement(T element) { elements.push_back(element); }
 bool HasElement(T element) { return std::find(elements.begin(), elements.end(), element) != elements.end(); }
 void RemoveElementAt(int index) { elements.erase(elements.begin() + index); }
 T& operator[](int index) { return elements[index]; }
 std::vector<T> elements;
};

class Panel {
public:
 virtual ~Panel() {}
 void SetVisible(bool state) { visible = state; }
 void SetEnabled(bool state) { enabled = state; }
 void SetParent(Panel* panel) { parent = panel; }
 void InvalidateLayout(bool, bool) {}
 void MakeReadyForUse() {}
 void RequestFocus(int) {}
 Panel* parent = nullptr;
 bool visible = false, enabled = true;
};
typedef Panel Button;

class WizardPanel;

// a page that wants to be displayed unless told otherwise
class WizardSubPanel : public Panel {
public:
 void SetWizardPanel(WizardPanel* panel) { wizard = panel; }
 bool ShouldDisplayPanel() { return display; }
 WizardSubPanel* GetNextSubPanel() { return next; }
 WizardSubPanel* GetPrevSubPanel() { return prev; }
 bool OnPrevButton() { return true; }
 void OnDisplayAsNext() { ++displayedAsNext; }
 void OnDisplayAsPrev() { ++displayedAsPrev; }
 void OnDisplay() {}
 bool GetDesiredSize(int&, int&) { return false; }
 WizardPanel* wizard = nullptr;
 WizardSubPanel *next = nullptr, *prev = nullptr;
 bool display = true;
 int displayedAsNext = 0, displayedAsPrev = 0;
};

// only the members the wizard navigation uses
class WizardPanel : public Panel {
public:
 void Run(WizardSubPanel* panel);
 void OnPrevButton();
 void ActivateNextSubPanel(WizardSubPanel* panel);
 void ActivatePrevSubPanel();
 WizardSubPanel* FindNextValidSubPanel(WizardSubPanel* panel);
 void Activate() { ++activations; }
 void SetSize(int, int) {}
 void RecalculateTabOrdering() {}
 void Repaint() {}
 Button prev, next, cancel, finish;
 Button* _prevButton = &prev;
 Button* _nextButton = &next;
 Button* _cancelButton = &cancel;
 Button* _finishButton = &finish;
 WizardSubPanel* _currentSubPanel = nullptr;
 Dar<WizardSubPanel*> _subPanelStack;
 bool _showButtons = true;
 int activations = 0;
};
'''

TESTS = r'''
}
using namespace vgui2;

static void BackWithoutASubPanelReportsAnError() {
 WizardPanel wizard;
 int errors = gui.errors;
 wizard.OnPrevButton();
 assert(nullptr == wizard._currentSubPanel);
 assert(errors + 1 == gui.errors);
 puts("PASS: WizardPanel::OnPrevButton with no current sub panel reports an error instead of dereferencing it");
}

static void RunWithoutAValidSubPanelReportsAnError() {
 WizardPanel wizard;
 int errors = gui.errors;
 wizard.Run(nullptr);
 assert(nullptr == wizard._currentSubPanel);
 assert(errors + 1 == gui.errors);
 assert(1 == wizard.activations);
 WizardSubPanel hidden;
 hidden.display = false;
 wizard.Run(&hidden);
 assert(nullptr == wizard._currentSubPanel);
 assert(errors + 2 == gui.errors);
 assert(0 == hidden.displayedAsNext);
 puts("PASS: WizardPanel::Run with no sub panel or none to display reports an error instead of dereferencing it");
}

static void NoValidNextPanelKeepsTheCurrentOne() {
 WizardPanel wizard;
 WizardSubPanel first;
 wizard.Run(&first);
 int errors = gui.errors;
 wizard.ActivateNextSubPanel(nullptr);
 assert(&first == wizard._currentSubPanel);
 assert(first.visible);
 assert(0 == wizard._subPanelStack.GetCount());
 assert(errors + 1 == gui.errors);
 puts("PASS: WizardPanel::ActivateNextSubPanel with no panel keeps the current one shown and the history unchanged");
}

static void BackWithAnEmptyHistoryReportsAnError() {
 WizardPanel wizard;
 WizardSubPanel first;
 wizard.Run(&first);
 int errors = gui.errors;
 assert(!wizard.prev.enabled);
 wizard.OnPrevButton();
 assert(&first == wizard._currentSubPanel);
 assert(errors + 1 == gui.errors);
 puts("PASS: WizardPanel::OnPrevButton on the first page reports there is no previous panel");
}

static void NextAndBackWalkTheHistory() {
 WizardPanel wizard;
 WizardSubPanel first, skipped, second;
 skipped.display = false;
 first.next = &skipped;
 skipped.next = &second;
 second.prev = &first;
 int errors = gui.errors;
 wizard.Run(&first);
 assert(&first == wizard._currentSubPanel);
 assert(&wizard == first.parent);
 assert(first.visible);
 wizard.ActivateNextSubPanel(wizard.FindNextValidSubPanel(first.GetNextSubPanel()));
 assert(&second == wizard._currentSubPanel);
 assert(!first.visible);
 assert(second.visible);
 assert(wizard.prev.enabled);
 assert(1 == wizard._subPanelStack.GetCount());
 wizard.OnPrevButton();
 assert(&first == wizard._currentSubPanel);
 assert(first.visible);
 assert(!second.visible);
 assert(1 == first.displayedAsPrev);
 assert(!wizard.prev.enabled);
 assert(0 == wizard._subPanelStack.GetCount());
 assert(errors == gui.errors);
 puts("PASS: WizardPanel goes forward past a page not to display and back through the history");
}

int main() {
 BackWithoutASubPanelReportsAnError();
 RunWithoutAValidSubPanelReportsAnError();
 NoValidNextPanelKeepsTheCurrentOne();
 BackWithAnEmptyHistoryReportsAnError();
 NextAndBackWalkTheHistory();
}
'''


def main():
    source = (ROOT / 'vgui2/controls/WizardPanel.cpp').read_text(encoding='utf-8')
    signatures = ('void WizardPanel::Run( WizardSubPanel* panel )', 'void WizardPanel::OnPrevButton()',
                  'void WizardPanel::ActivateNextSubPanel( WizardSubPanel* panel )', 'void WizardPanel::ActivatePrevSubPanel()',
                  'WizardSubPanel* WizardPanel::FindNextValidSubPanel( WizardSubPanel* panel )')
    code = STUBS + ''.join(function(source, signature) + '\n' for signature in signatures) + TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-wizard-panel-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
