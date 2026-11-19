"""Compile the production VGUI1 ButtonGroup and Button and check group membership (#221).

Run: python tests/vgui1_button_group_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
AddressSanitizer (including leak checks where supported) is enabled when the
compiler supports it; without it only the value checks run.

ButtonGroup and the Panel, Label and Button sources from vgui1_panel_tests.py
are built with its platform stubs, twice: with VGUI_ABI_COMPAT_MODE=1 (the
original vgui.dll layout, no destructors, whose runs check memory errors only)
and with VGUI_ABI_COMPAT_MODE=0. tests/vgui1_widget_ownership_tests.py
tab-panel covers a TabPanel deleting its ButtonGroup before its tabs.

- single-select: selecting a button of a group deselects the others.
With VGUI_ABI_COMPAT_MODE=0 only:
- switch-group: a button moved to another group leaves the first one, which no
  longer deselects it; setting the same group again adds it once.
- set-null: a button given no group leaves its group and selects itself.
- delete-button: selecting a button after another one of its group was deleted
  does not use the deleted one.
- delete-selected: deleting the selected button leaves the group with none
  selected, and another can be selected.
- group-deleted-first: deleting a group leaves its buttons without a group, so
  selecting, changing group and deleting them does not use it.
- group-copy: a copied group starts with no buttons, an assigned group keeps
  its own, and deleting either leaves the other group's buttons in it.
- button-copy: a copied button is in no group.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program
from vgui1_panel_tests import VGUI, vgui1_sources

TESTS = r'''
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include "VGUI_App.h"
#include "VGUI_Button.h"
#include "VGUI_ButtonGroup.h"

using namespace vgui;

class TestApp : public App
{
public:
 TestApp() : App( true ) {}
 virtual void main( int, char** ) override {}
};

// Exposes the group's list.
class TestGroup : public ButtonGroup
{
public:
 int count() { return _buttonDar.getCount(); }
 bool has( Button* button ) { return _buttonDar.hasElement( button ); }
};

static void Check(bool condition, const char* message)
{
 if (!condition) throw std::runtime_error(message);
}

static const int BUTTON_COUNT = 3;

static void Join(TestGroup& group, Button** buttons)
{
 for (int i = 0; i < BUTTON_COUNT; ++i)
 {
  buttons[i] = new Button( "button", 0, 0 );
  buttons[i]->setButtonGroup( &group );
 }
}

static void DeleteAll(Button** buttons)
{
 for (int i = 0; i < BUTTON_COUNT; ++i) delete buttons[i];
}

static void Run(const std::string& test)
{
 Button* b[BUTTON_COUNT];
 if (test == "single-select")
 {
  TestGroup group;
  Join(group, b);
  b[0]->setSelected( true );
  b[1]->setSelected( true );
  Check(!b[0]->isSelected() && b[1]->isSelected() && !b[2]->isSelected(), "one button selected");
#if VGUI_ABI_COMPAT_MODE != 1
  DeleteAll(b);
#endif
 }
#if VGUI_ABI_COMPAT_MODE != 1
 else if (test == "switch-group")
 {
  TestGroup group;
  TestGroup other;
  Join(group, b);
  b[1]->setButtonGroup( &other );
  Check(2 == group.count() && !group.has( b[1] ), "the button left the first group");
  Check(other.has( b[1] ), "the button joined the other group");
  b[1]->setSelected( true );
  b[0]->setSelected( true );
  Check(b[1]->isSelected(), "the first group does not deselect it");
  b[1]->setButtonGroup( &other );
  Check(1 == other.count(), "setting the same group adds it once");
  DeleteAll(b);
  Check(0 == group.count() && 0 == other.count(), "deleted buttons left both groups");
 }
 else if (test == "set-null")
 {
  TestGroup group;
  Join(group, b);
  b[0]->setButtonGroup( NULL );
  Check(2 == group.count() && !group.has( b[0] ), "the button left its group");
  b[0]->setSelected( true );
  b[1]->setSelected( true );
  Check(b[0]->isSelected() && b[1]->isSelected(), "a button without a group selects itself");
  DeleteAll(b);
  Check(0 == group.count(), "deleted buttons left the group");
 }
 else if (test == "delete-button")
 {
  TestGroup group;
  Join(group, b);
  delete b[0];
  Check(2 == group.count(), "the deleted button left");
  b[1]->setSelected( true );
  Check(b[1]->isSelected() && !b[2]->isSelected(), "selection skips the deleted button");
  delete b[1];
  delete b[2];
 }
 else if (test == "delete-selected")
 {
  TestGroup group;
  Join(group, b);
  b[1]->setSelected( true );
  delete b[1];
  Check(2 == group.count() && !b[0]->isSelected() && !b[2]->isSelected(), "no button selected");
  b[2]->setSelected( true );
  Check(b[2]->isSelected() && !b[0]->isSelected(), "another button can be selected");
  delete b[0];
  delete b[2];
 }
 else if (test == "group-deleted-first")
 {
  TestGroup* group = new TestGroup();
  TestGroup other;
  Join(*group, b);
  delete group;
  b[0]->setSelected( true );
  b[1]->setSelected( true );
  Check(b[0]->isSelected() && b[1]->isSelected(), "buttons of a deleted group select themselves");
  b[2]->setButtonGroup( &other );
  Check(other.has( b[2] ) && 1 == other.count(), "a button of a deleted group joins another");
  DeleteAll(b);
  Check(0 == other.count(), "deleted buttons left the other group");
 }
 else if (test == "group-copy")
 {
  TestGroup group;
  Join(group, b);
  TestGroup* copy = new TestGroup( group );
  Check(0 == copy->count(), "a copy starts with no buttons");
  delete copy;
  TestGroup assigned;
  Button* own = new Button( "own", 0, 0 );
  own->setButtonGroup( &assigned );
  assigned = group;
  Check(1 == assigned.count() && assigned.has( own ), "an assigned group keeps its own buttons");
  Check(BUTTON_COUNT == group.count(), "the other group keeps its buttons");
  b[0]->setSelected( true );
  b[1]->setSelected( true );
  Check(!b[0]->isSelected() && b[1]->isSelected(), "the other group still selects one");
  delete own;
  DeleteAll(b);
 }
 else if (test == "button-copy")
 {
  TestGroup group;
  Join(group, b);
  Button* copy = new Button( *b[0] );
  Check(BUTTON_COUNT == group.count() && !group.has( copy ), "a copied button is in no group");
  delete copy;
  Check(BUTTON_COUNT == group.count(), "deleting the copy leaves the group alone");
  DeleteAll(b);
 }
#endif
 else Check(false, "unknown scenario");
}

int main(int argc, char** argv)
{
 // Static so that the scheme App never frees stays reachable for leak checks.
 static TestApp testApp;
 try
 {
  Check(argc == 2, "usage: test <scenario>");
  Run(argv[1]);
 }
 catch (const std::exception& error)
 {
  std::fprintf(stderr, "%s\n", error.what());
  return EXIT_FAILURE;
 }
 std::printf("PASS: %s\n", argv[1]);
 return EXIT_SUCCESS;
}
'''

SCENARIOS = ['single-select']
NONCOMPAT_SCENARIOS = ['switch-group', 'set-null', 'delete-button', 'delete-selected', 'group-deleted-first',
                       'group-copy', 'button-copy']
# VGUI_ABI_COMPAT_MODE value -> scenarios run against that build.
MODES = {'1': SCENARIOS, '0': SCENARIOS + NONCOMPAT_SCENARIOS}


def main():
    failed = []
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui1-button-group-') as folder:
        test_file = Path(folder) / 'test.cpp'
        test_file.write_text(TESTS)
        sources = [test_file] + vgui1_sources(folder)
        for mode, scenarios in MODES.items():
            executable = Path(folder) / f'test-compat{mode}.exe'
            # VGUI_STATIC keeps VGUIAPI from exporting on Windows: MSVC would emit the whole
            # vtable of an exported class in every file, also for sources the test leaves out.
            executable = compile_program(sources, executable, includes=[VGUI / 'include', VGUI],
                                         defines=['VGUI_STATIC', f'VGUI_ABI_COMPAT_MODE={mode}'], debug=True,
                                         sanitizers=['address'])
            # Without destructors the compatible build frees no panel; only check
            # its memory errors.
            env = dict(os.environ)
            if mode == '1':
                env['ASAN_OPTIONS'] = 'detect_leaks=0:' + env.get('ASAN_OPTIONS', '')
            for scenario in scenarios:
                result = subprocess.run([str(executable), scenario], env=env)
                if result.returncode:
                    failed.append(f'{scenario} (VGUI_ABI_COMPAT_MODE={mode})')
    if failed:
        raise SystemExit('FAILED: ' + ', '.join(failed))


if __name__ == '__main__':
    main()
