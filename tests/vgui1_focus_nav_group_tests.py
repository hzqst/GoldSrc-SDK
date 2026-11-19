"""Compile the production VGUI1 FocusNavGroup and Panel and check group membership (#220).

Run: python tests/vgui1_focus_nav_group_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
AddressSanitizer (including leak checks where supported) is enabled when the
compiler supports it; without it only the value checks run.

FocusNavGroup and the Panel sources from vgui1_panel_tests.py are built with
its platform stubs, twice: with VGUI_ABI_COMPAT_MODE=1 (the original vgui.dll
layout, no destructors, whose runs check memory errors only) and with
VGUI_ABI_COMPAT_MODE=0. A TestPanel records which panel navigation focuses.

- navigate: next and previous go around the group in both directions.
With VGUI_ABI_COMPAT_MODE=0 only:
- switch-group: a panel moved to another group leaves the first one, which no
  longer focuses it; setting the same group again adds it once.
- set-null: a panel given no group leaves its group.
- delete-current: deleting the current panel makes next focus the panel after it.
- delete-first: deleting a panel before the current one keeps the current one.
- delete-last: deleting the current last panel makes next go round to the first.
- delete-all: navigating a group whose panels were all deleted does nothing.
- group-deleted-first: deleting a group leaves its panels without a group, so
  navigating, changing group and deleting them does not use it.
- group-copy: a copied group starts with no panels, an assigned group keeps
  its own, and deleting either leaves the other group's panels in it.
- panel-copy: a copied panel joins its group and leaves it when deleted.
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
#include "VGUI_FocusNavGroup.h"
#include "VGUI_Panel.h"

using namespace vgui;

class TestApp : public App
{
public:
 TestApp() : App( true ) {}
 virtual void main( int, char** ) override {}
};

static Panel* focused;

class TestPanel : public Panel
{
public:
 TestPanel() : Panel( 0, 0, 10, 10 ) {}
 virtual void requestFocus() override { focused = this; }
};

// Exposes the group's list and navigation.
class TestGroup : public FocusNavGroup
{
public:
 int count() { return _panelDar.getCount(); }
 bool has( Panel* panel ) { return _panelDar.hasElement( panel ); }
 void current( Panel* panel ) { setCurrentPanel( panel ); }
 Panel* next() { focused = NULL; requestFocusNext(); return focused; }
 Panel* prev() { focused = NULL; requestFocusPrev(); return focused; }
};

static void Check(bool condition, const char* message)
{
 if (!condition) throw std::runtime_error(message);
}

static const int PANEL_COUNT = 3;

static void Join(TestGroup& group, TestPanel** panels)
{
 for (int i = 0; i < PANEL_COUNT; ++i)
 {
  panels[i] = new TestPanel();
  panels[i]->setFocusNavGroup( &group );
 }
}

static void Run(const std::string& test)
{
 TestPanel* p[PANEL_COUNT];
 if (test == "navigate")
 {
  TestGroup group;
  Join(group, p);
  group.current( p[0] );
  Check(p[1] == group.next() && p[2] == group.next() && p[0] == group.next(), "next goes around");
  Check(p[2] == group.prev() && p[1] == group.prev(), "previous goes around");
  // A panel moves its group on from the group's current panel, p[1].
  p[0]->requestFocusNext();
  Check(p[2] == focused, "a panel moves its group on");
#if VGUI_ABI_COMPAT_MODE != 1
  for (int i = 0; i < PANEL_COUNT; ++i) delete p[i];
#endif
 }
#if VGUI_ABI_COMPAT_MODE != 1
 else if (test == "switch-group")
 {
  TestGroup group;
  TestGroup other;
  Join(group, p);
  p[1]->setFocusNavGroup( &other );
  Check(2 == group.count() && !group.has( p[1] ), "the panel left the first group");
  Check(other.has( p[1] ), "the panel joined the other group");
  for (int i = 0; i < 2 * PANEL_COUNT; ++i) Check(p[1] != group.next(), "the first group does not focus it");
  p[1]->setFocusNavGroup( &other );
  Check(1 == other.count(), "setting the same group adds it once");
  for (int i = 0; i < PANEL_COUNT; ++i) delete p[i];
  Check(0 == group.count() && 0 == other.count(), "deleted panels left both groups");
 }
 else if (test == "set-null")
 {
  TestGroup group;
  Join(group, p);
  p[0]->setFocusNavGroup( NULL );
  Check(2 == group.count() && !group.has( p[0] ), "the panel left its group");
  p[0]->requestFocusNext();
  for (int i = 0; i < PANEL_COUNT; ++i) delete p[i];
  Check(0 == group.count(), "deleted panels left the group");
 }
 else if (test == "delete-current")
 {
  TestGroup group;
  Join(group, p);
  group.current( p[1] );
  delete p[1];
  Check(2 == group.count(), "the current panel left");
  Check(p[2] == group.next() && p[0] == group.next(), "next focuses the panel after it");
  delete p[0];
  delete p[2];
 }
 else if (test == "delete-first")
 {
  TestGroup group;
  Join(group, p);
  group.current( p[1] );
  delete p[0];
  Check(2 == group.count(), "the first panel left");
  Check(p[2] == group.next(), "the current panel is kept");
  Check(p[1] == group.prev(), "previous focuses the panel before");
  delete p[1];
  delete p[2];
 }
 else if (test == "delete-last")
 {
  TestGroup group;
  Join(group, p);
  group.current( p[2] );
  delete p[2];
  Check(2 == group.count(), "the last panel left");
  Check(p[0] == group.next(), "next goes round to the first panel");
  delete p[0];
  delete p[1];
 }
 else if (test == "delete-all")
 {
  TestGroup group;
  Join(group, p);
  group.current( p[1] );
  for (int i = 0; i < PANEL_COUNT; ++i) delete p[i];
  Check(0 == group.count(), "every panel left");
  Check(NULL == group.next() && NULL == group.prev(), "an empty group focuses nothing");
  TestPanel* late = new TestPanel();
  late->setFocusNavGroup( &group );
  Check(late == group.next() && late == group.prev(), "a panel added later is focused");
  delete late;
 }
 else if (test == "group-deleted-first")
 {
  TestGroup* group = new TestGroup();
  TestGroup other;
  Join(*group, p);
  delete group;
  focused = NULL;
  p[0]->requestFocusNext();
  p[0]->requestFocusPrev();
  Check(NULL == focused, "a panel without a group focuses nothing");
  p[1]->setFocusNavGroup( &other );
  Check(other.has( p[1] ) && 1 == other.count(), "a panel of a deleted group joins another");
  for (int i = 0; i < PANEL_COUNT; ++i) delete p[i];
  Check(0 == other.count(), "deleted panels left the other group");
 }
 else if (test == "group-copy")
 {
  TestGroup group;
  Join(group, p);
  TestGroup* copy = new TestGroup( group );
  Check(0 == copy->count(), "a copy starts with no panels");
  delete copy;
  TestGroup assigned;
  TestPanel* own = new TestPanel();
  own->setFocusNavGroup( &assigned );
  assigned = group;
  Check(1 == assigned.count() && assigned.has( own ), "an assigned group keeps its own panels");
  Check(PANEL_COUNT == group.count(), "the other group keeps its panels");
  group.current( p[0] );
  Check(p[1] == group.next(), "the other group still navigates");
  delete own;
  for (int i = 0; i < PANEL_COUNT; ++i) delete p[i];
 }
 else if (test == "panel-copy")
 {
  TestGroup group;
  Join(group, p);
  TestPanel* copy = new TestPanel( *p[0] );
  Check(PANEL_COUNT + 1 == group.count() && group.has( copy ), "a copied panel joins its group");
  delete copy;
  Check(PANEL_COUNT == group.count() && !group.has( copy ), "a deleted copy leaves the group");
  for (int i = 0; i < PANEL_COUNT; ++i) delete p[i];
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

SCENARIOS = ['navigate']
NONCOMPAT_SCENARIOS = ['switch-group', 'set-null', 'delete-current', 'delete-first', 'delete-last', 'delete-all',
                       'group-deleted-first', 'group-copy', 'panel-copy']
# VGUI_ABI_COMPAT_MODE value -> scenarios run against that build.
MODES = {'1': SCENARIOS, '0': SCENARIOS + NONCOMPAT_SCENARIOS}


def main():
    failed = []
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui1-focus-nav-') as folder:
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
