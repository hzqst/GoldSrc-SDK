"""Compile the production VGUI1 BuildGroup and Panel and check group membership (#222).

Run: python tests/vgui1_build_group_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
AddressSanitizer (including leak checks where supported) is enabled when the
compiler supports it; without it only the value checks run.

BuildGroup and the Panel sources from vgui1_panel_tests.py are built with its
platform stubs, twice: with VGUI_ABI_COMPAT_MODE=1 (the original vgui.dll
layout, no destructors, whose runs check memory errors only) and with
VGUI_ABI_COMPAT_MODE=0. A TestApp keeps the clipboard text.
tests/vgui1_cursor_bitmap_tests.py covers the cursors and names a BuildGroup
owns and its copies.

- set-null: setBuildGroup with no group gives the panel no group.
With VGUI_ABI_COMPAT_MODE=0 only:
- delete-panel: a deleted panel leaves the list with its name; deleting a
  panel that is not the current one fires no change signal.
- delete-current: a deleted current panel stops being the current panel, and
  the change signal fires once.
- switch-group: a panel moved to another group leaves the first one, with its
  name, and stops being its current panel.
- same-group: setting the same group again lists the panel once, with the new
  name.
- group-deleted-first: deleting a group leaves its panels without a group, so
  using, moving and deleting them does not use it.
- panel-copy: a copied panel is in no group, and an assigned panel keeps its
  own group.
- copy-properties: copyPropertiesToClipboard after a panel was deleted lists
  only the panels left.
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
#include <cstring>
#include <stdexcept>
#include <string>

// The test reads the group's lists and current panel.
#define private public
#define protected public
#include "VGUI_App.h"
#include "VGUI_BuildGroup.h"
#include "VGUI_ChangeSignal.h"
#include "VGUI_Panel.h"
#undef protected
#undef private

using namespace vgui;

class TestApp : public App
{
public:
 TestApp() : App( true ) {}
 virtual void main( int, char** ) override {}
 virtual void setClipboardText( const char* text, int textLen ) override { clipboard.assign( text, textLen ); }

 std::string clipboard;
};

static TestApp* app;

class CountingSignal : public ChangeSignal
{
public:
 CountingSignal() : changes( 0 ) {}
 virtual void valueChanged( Panel* ) override { ++changes; }

 int changes;
};

static void Check(bool condition, const char* message)
{
 if (!condition) throw std::runtime_error(message);
}

static const int PANEL_COUNT = 3;
static const char* const NAMES[PANEL_COUNT] = { "first", "second", "third" };

static void Join(BuildGroup& group, Panel** panels)
{
 for (int i = 0; i < PANEL_COUNT; ++i)
 {
  panels[i] = new Panel( 0, 0, 10, 10 );
  panels[i]->setBuildGroup( &group, NAMES[i] );
 }
}

static void DeleteAll(Panel** panels)
{
 for (int i = 0; i < PANEL_COUNT; ++i) delete panels[i];
}

// Whether group lists exactly panel with name at index.
static bool Lists(BuildGroup& group, int index, Panel* panel, const char* name)
{
 return index < group._panelDar.getCount() && panel == group._panelDar[index]
  && group._panelDar.getCount() == group._panelNameDar.getCount() && 0 == std::strcmp( name, group._panelNameDar[index] );
}

static void Run(const std::string& test)
{
 Panel* p[PANEL_COUNT];
 if (test == "set-null")
 {
  BuildGroup group;
  group.setEnabled( true );
  Panel* panel = new Panel( 0, 0, 10, 10 );
  panel->setBuildGroup( NULL, "none" );
  Check(!panel->isBuildGroupEnabled(), "no group");
  panel->setBuildGroup( &group, "panel" );
  Check(panel->isBuildGroupEnabled(), "in the group");
  panel->setBuildGroup( NULL, "none" );
  Check(!panel->isBuildGroupEnabled(), "no group again");
#if VGUI_ABI_COMPAT_MODE != 1
  Check(0 == group._panelDar.getCount() && 0 == group._panelNameDar.getCount(), "the panel left its group");
  delete panel;
#endif
 }
#if VGUI_ABI_COMPAT_MODE != 1
 else if (test == "delete-panel")
 {
  BuildGroup group;
  CountingSignal signal;
  group.addCurrentPanelChangeSignal( &signal );
  Join(group, p);
  group._currentPanel = p[2];
  delete p[1];
  Check(2 == group._panelDar.getCount(), "the deleted panel left");
  Check(Lists(group, 0, p[0], NAMES[0]) && Lists(group, 1, p[2], NAMES[2]), "the names follow their panels");
  Check(p[2] == group.getCurrentPanel() && 0 == signal.changes, "the current panel is kept");
  delete p[0];
  delete p[2];
  Check(0 == group._panelDar.getCount() && 0 == group._panelNameDar.getCount(), "every panel left");
 }
 else if (test == "delete-current")
 {
  BuildGroup group;
  CountingSignal signal;
  group.addCurrentPanelChangeSignal( &signal );
  Join(group, p);
  group._currentPanel = p[1];
  delete p[1];
  Check(NULL == group.getCurrentPanel(), "no current panel");
  Check(1 == signal.changes, "the change signal fired once");
  delete p[0];
  delete p[2];
  Check(1 == signal.changes, "deleting the others fires nothing");
 }
 else if (test == "switch-group")
 {
  BuildGroup group;
  BuildGroup other;
  Join(group, p);
  group._currentPanel = p[0];
  p[0]->setBuildGroup( &other, "moved" );
  Check(2 == group._panelDar.getCount() && Lists(group, 0, p[1], NAMES[1]), "the panel left the first group");
  Check(NULL == group.getCurrentPanel(), "it is not the first group's current panel");
  Check(1 == other._panelDar.getCount() && Lists(other, 0, p[0], "moved"), "the panel joined the other group");
  DeleteAll(p);
  Check(0 == group._panelDar.getCount() && 0 == other._panelDar.getCount(), "deleted panels left both groups");
 }
 else if (test == "same-group")
 {
  BuildGroup group;
  Join(group, p);
  p[0]->setBuildGroup( &group, "renamed" );
  Check(PANEL_COUNT == group._panelDar.getCount(), "the panel is listed once");
  Check(Lists(group, PANEL_COUNT - 1, p[0], "renamed"), "with the new name");
  DeleteAll(p);
 }
 else if (test == "group-deleted-first")
 {
  BuildGroup* group = new BuildGroup();
  BuildGroup other;
  group->setEnabled( true );
  Join(*group, p);
  delete group;
  Check(NULL == p[0]->_buildGroup && !p[0]->isBuildGroupEnabled(), "a panel of a deleted group has no group");
  p[1]->setBuildGroup( &other, "moved" );
  Check(Lists(other, 0, p[1], "moved"), "a panel of a deleted group joins another");
  DeleteAll(p);
  Check(0 == other._panelDar.getCount(), "deleted panels left the other group");
 }
 else if (test == "panel-copy")
 {
  BuildGroup group;
  BuildGroup other;
  Join(group, p);
  Panel* copy = new Panel( *p[0] );
  Check(NULL == copy->_buildGroup && PANEL_COUNT == group._panelDar.getCount(), "a copied panel is in no group");
  Panel* assigned = new Panel( 0, 0, 10, 10 );
  assigned->setBuildGroup( &other, "own" );
  *assigned = *p[0];
  Check(&other == assigned->_buildGroup && Lists(other, 0, assigned, "own"), "an assigned panel keeps its group");
  delete copy;
  delete assigned;
  Check(PANEL_COUNT == group._panelDar.getCount() && 0 == other._panelDar.getCount(), "only the own group changed");
  DeleteAll(p);
 }
 else if (test == "copy-properties")
 {
  BuildGroup group;
  Join(group, p);
  delete p[1];
  group.copyPropertiesToClipboard();
  Check(std::string::npos != app->clipboard.find( NAMES[0] ) && std::string::npos != app->clipboard.find( NAMES[2] ),
        "the panels left are listed");
  Check(std::string::npos == app->clipboard.find( NAMES[1] ), "the deleted panel is not");
  delete p[0];
  delete p[2];
 }
#endif
 else Check(false, "unknown scenario");
}

int main(int argc, char** argv)
{
 // Static so that the scheme App never frees stays reachable for leak checks.
 static TestApp testApp;
 app = &testApp;
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

SCENARIOS = ['set-null']
NONCOMPAT_SCENARIOS = ['delete-panel', 'delete-current', 'switch-group', 'same-group', 'group-deleted-first',
                       'panel-copy', 'copy-properties']
# VGUI_ABI_COMPAT_MODE value -> scenarios run against that build.
MODES = {'1': SCENARIOS, '0': SCENARIOS + NONCOMPAT_SCENARIOS}

# The test reads private and protected members; MSVC's standard library
# refuses keyword macros unless _ALLOW_KEYWORD_MACROS is set.
ACCESS_DEFINES = ['private=public', 'protected=public', '_ALLOW_KEYWORD_MACROS']


def main():
    failed = []
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui1-build-group-') as folder:
        test_file = Path(folder) / 'test.cpp'
        test_file.write_text(TESTS)
        sources = [test_file] + vgui1_sources(folder)
        for mode, scenarios in MODES.items():
            executable = Path(folder) / f'test-compat{mode}.exe'
            # VGUI_STATIC keeps VGUIAPI from exporting on Windows: MSVC would emit the whole
            # vtable of an exported class in every file, also for sources the test leaves out.
            executable = compile_program(sources, executable, includes=[VGUI / 'include', VGUI],
                                         defines=['VGUI_STATIC', f'VGUI_ABI_COMPAT_MODE={mode}'] + ACCESS_DEFINES,
                                         debug=True,
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
