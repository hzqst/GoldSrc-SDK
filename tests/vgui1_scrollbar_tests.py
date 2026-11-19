"""Compile the production VGUI1 ScrollBar and check how it replaces its parts (#232).

Run: python tests/vgui1_scrollbar_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
AddressSanitizer (including leak checks where supported) is enabled when the
compiler supports it; without it only the value checks run.

ScrollBar and Slider are built with the Panel, Label and Button sources and
platform stubs from vgui1_panel_tests.py, twice: with VGUI_ABI_COMPAT_MODE=1
(the original vgui.dll layout, no destructors, whose runs check memory errors
only) and with VGUI_ABI_COMPAT_MODE=0. A counting signal on the bar checks how
often a slider change or button click reaches it.

- reject-invalid: setSlider and setButton ignore NULL and an index other than
  0 or 1, and getButton gives NULL for such an index.
- same-part: setting the current slider or button again, or the other button,
  changes nothing, and a change reaches the bar once.
With VGUI_ABI_COMPAT_MODE=0 only:
- replace-slider: a replaced slider, also the default one, is deleted once; the
  new slider's changes reach the bar once, and deleting the bar deletes it.
- replace-buttons: the same for both buttons; a click on a new button scrolls
  the bar.
- child-of-old: a new slider that was a child of the old one survives it.
- bar-copy: replacing a part of a copied bar leaves the original bar's parts,
  which the copy points to, alone.
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

PARTS = ['ScrollBar.cpp', 'Slider.cpp']

TESTS = r'''
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include "VGUI_App.h"
#include "VGUI_Button.h"
#include "VGUI_IntChangeSignal.h"
#include "VGUI_ScrollBar.h"
#include "VGUI_Slider.h"

using namespace vgui;

class TestApp : public App
{
public:
 TestApp() : App( true ) {}
 virtual void main( int, char** ) override {}
};

static int changes;
static int deletedParts;

class CountingSignal : public IntChangeSignal
{
public:
 virtual void intChanged( int, Panel* ) override { ++changes; }
};

class TestSlider : public Slider
{
public:
 TestSlider() : Slider( 0, 15, 15, 70, true ) {}
#if VGUI_ABI_COMPAT_MODE != 1
 virtual ~TestSlider() override { ++deletedParts; }
#endif
};

class TestButton : public Button
{
public:
 TestButton() : Button( "", 0, 0, 15, 15 ) {}
#if VGUI_ABI_COMPAT_MODE != 1
 virtual ~TestButton() override { ++deletedParts; }
#endif
};

static void Check(bool condition, const char* message)
{
 if (!condition) throw std::runtime_error(message);
}

static const int RANGE_MAX = 100;
static const int MIDDLE = 50;

static ScrollBar* NewBar()
{
 ScrollBar* bar = new ScrollBar( 0, 0, 15, 100, true );
 bar->setRange( 0, RANGE_MAX );
 bar->addIntChangeSignal( new CountingSignal() );
 return bar;
}

// Changes the slider's value once; returns how often the bar was told.
static int Change(ScrollBar* bar, int value)
{
 changes = 0;
 bar->getSlider()->setValue( value );
 return changes;
}

static void Run(const std::string& test)
{
 if (test == "reject-invalid")
 {
  ScrollBar* bar = NewBar();
  Slider* slider = bar->getSlider();
  Button* first = bar->getButton( 0 );
  Button* second = bar->getButton( 1 );
  Button* spare = new Button( "", 0, 0, 15, 15 );
  bar->setSlider( NULL );
  bar->setButton( NULL, 0 );
  bar->setButton( spare, 2 );
  bar->setButton( spare, -1 );
  Check(slider == bar->getSlider() && first == bar->getButton( 0 ) && second == bar->getButton( 1 ), "parts kept");
  Check(NULL == bar->getButton( 2 ) && NULL == bar->getButton( -1 ), "no button at a bad index");
  Check(NULL == spare->getParent(), "an ignored button is not added");
  Check(1 == Change(bar, MIDDLE), "a change reaches the bar once");
#if VGUI_ABI_COMPAT_MODE != 1
  delete spare;
  delete bar;
#endif
 }
 else if (test == "same-part")
 {
  ScrollBar* bar = NewBar();
  Slider* slider = bar->getSlider();
  Button* first = bar->getButton( 0 );
  Button* second = bar->getButton( 1 );
  bar->setSlider( slider );
  bar->setButton( first, 0 );
  bar->setButton( first, 1 );
  Check(slider == bar->getSlider() && first == bar->getButton( 0 ) && second == bar->getButton( 1 ), "parts kept");
  Check(bar == slider->getParent() && bar == first->getParent() && bar == second->getParent(), "parts on the bar");
  Check(1 == Change(bar, MIDDLE), "a change reaches the bar once");
  changes = 0;
  first->doClick();
  Check(1 == changes, "a click reaches the bar once");
#if VGUI_ABI_COMPAT_MODE != 1
  delete bar;
#endif
 }
#if VGUI_ABI_COMPAT_MODE != 1
 else if (test == "replace-slider")
 {
  ScrollBar* bar = NewBar();
  TestSlider* slider = new TestSlider();
  bar->setSlider( slider );
  Check(slider == bar->getSlider() && bar == slider->getParent(), "the slider is on the bar");
  slider->setRange( 0, RANGE_MAX );
  Check(1 == Change(bar, MIDDLE), "the new slider's change reaches the bar once");
  TestSlider* next = new TestSlider();
  bar->setSlider( next );
  Check(1 == deletedParts, "the replaced slider was deleted");
  next->setRange( 0, RANGE_MAX );
  Check(1 == Change(bar, MIDDLE), "the next slider's change reaches the bar once");
  delete bar;
  Check(2 == deletedParts, "deleting the bar deleted its slider");
 }
 else if (test == "replace-buttons")
 {
  ScrollBar* bar = NewBar();
  bar->setValue( MIDDLE );
  for (int index = 0; index < 2; ++index)
  {
   TestButton* button = new TestButton();
   bar->setButton( button, index );
   Check(button == bar->getButton( index ) && bar == button->getParent(), "the button is on the bar");
   TestButton* next = new TestButton();
   bar->setButton( next, index );
   Check(index + 1 == deletedParts, "the replaced button was deleted");
   changes = 0;
   const int before = bar->getValue();
   next->doClick();
   Check(1 == changes && before != bar->getValue(), "a click on the new button scrolls the bar");
  }
  delete bar;
  Check(4 == deletedParts, "deleting the bar deleted its buttons");
 }
 else if (test == "child-of-old")
 {
  ScrollBar* bar = NewBar();
  TestSlider* old = new TestSlider();
  bar->setSlider( old );
  TestSlider* slider = new TestSlider();
  slider->setParent( old );
  bar->setSlider( slider );
  Check(1 == deletedParts && bar == slider->getParent(), "the new slider survives the old one");
  delete bar;
  Check(2 == deletedParts, "deleting the bar deleted the new slider");
 }
 else if (test == "bar-copy")
 {
  ScrollBar* bar = NewBar();
  Slider* slider = bar->getSlider();
  Button* first = bar->getButton( 0 );
  ScrollBar* copy = new ScrollBar( *bar );
  copy->setSlider( new TestSlider() );
  copy->setButton( new TestButton(), 0 );
  Check(0 == deletedParts, "the copy deleted nothing");
  Check(bar == slider->getParent() && bar == first->getParent(), "the original bar keeps its parts");
  Check(1 == Change(bar, MIDDLE), "the original bar still works");
  delete copy;
  Check(2 == deletedParts, "the copy deleted its own parts");
  Check(bar == slider->getParent(), "the original bar still keeps its slider");
  delete bar;
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

SCENARIOS = ['reject-invalid', 'same-part']
NONCOMPAT_SCENARIOS = ['replace-slider', 'replace-buttons', 'child-of-old', 'bar-copy']
# VGUI_ABI_COMPAT_MODE value -> scenarios run against that build.
MODES = {'1': SCENARIOS, '0': SCENARIOS + NONCOMPAT_SCENARIOS}


def main():
    failed = []
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui1-scrollbar-') as folder:
        test_file = Path(folder) / 'test.cpp'
        test_file.write_text(TESTS)
        sources = [test_file] + vgui1_sources(folder) + [VGUI / name for name in PARTS]
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
