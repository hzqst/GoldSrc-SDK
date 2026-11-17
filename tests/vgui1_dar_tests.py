"""Compile the production VGUI1 Dar container and check its ownership (#287).

Run: python tests/vgui1_dar_tests.py
Requires a C++17 compiler (CXX, or clang++ on Windows / c++ elsewhere).
AddressSanitizer (including leak checks where supported) is enabled when the
compiler supports it; without it only the value checks run.

The internal VGUI_Dar.h is included as-is and Dar<char*> / Dar<int> are
explicitly instantiated, as the vgui.dll exports do.

- destroy-frees: destroying a Dar frees its backing array (leak check).
- copy-independent: a copy-constructed Dar does not share elements.
- copy-grow: growing a copy leaves the source readable (use-after-free check).
- assign-independent / assign-grow: the same for operator=.
- assign-replaces: assigning over a Dar frees the array it held before.
- self-assign: assigning a Dar to itself keeps its elements.
- copy-empty: copying a Dar created with no capacity gives an empty Dar.
"""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
VGUI = ROOT / 'GoldSrc-SDK/utils/vgui/lib-src/vgui'

TESTS = r'''
#include "VGUI_Dar.h"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

using namespace vgui;

// VGUI_Dar.h already instantiates these on Windows.
#ifndef _WIN32
template class vgui::Dar<char*>;
template class vgui::Dar<int>;
#endif

static const int SOURCE_COUNT = 3;
static const int GROW_COUNT = 64;
static const int CHANGED_VALUE = 99;

static void Check(bool condition, const char* message)
{
 if (!condition) throw std::runtime_error(message);
}

static void Fill(Dar<int>& dar)
{
 for (int i = 0; i < SOURCE_COUNT; ++i)
 {
  dar.addElement(i);
 }
}

static void CheckSource(Dar<int>& dar)
{
 Check(dar.getCount() == SOURCE_COUNT, "source count unchanged");
 for (int i = 0; i < SOURCE_COUNT; ++i)
 {
  Check(dar[i] == i, "source elements unchanged");
 }
}

static void Grow(Dar<int>& dar)
{
 for (int i = 0; i < GROW_COUNT; ++i)
 {
  dar.addElement(i);
 }
}

static void Run(const std::string& test)
{
 if (test == "destroy-frees")
 {
  Dar<char*>* dar = new Dar<char*>();
  dar->addElement(NULL);
  delete dar;
 }
 else if (test == "copy-independent")
 {
  Dar<int> source;
  Fill(source);
  Dar<int> copy(source);
  CheckSource(copy);
  copy.setElementAt(CHANGED_VALUE, 0);
  CheckSource(source);
 }
 else if (test == "copy-grow")
 {
  Dar<int> source;
  Fill(source);
  Dar<int> copy(source);
  Grow(copy);
  CheckSource(source);
 }
 else if (test == "assign-independent")
 {
  Dar<int> source;
  Fill(source);
  Dar<int> copy;
  copy = source;
  CheckSource(copy);
  copy.setElementAt(CHANGED_VALUE, 0);
  CheckSource(source);
 }
 else if (test == "assign-grow")
 {
  Dar<int> source;
  Fill(source);
  Dar<int> copy;
  copy = source;
  Grow(copy);
  CheckSource(source);
 }
 else if (test == "assign-replaces")
 {
  Dar<int> source;
  Fill(source);
  Dar<int> copy;
  Grow(copy);
  copy = source;
  CheckSource(copy);
 }
 else if (test == "self-assign")
 {
  Dar<int> source;
  Fill(source);
  Dar<int>& alias = source;
  source = alias;
  CheckSource(source);
 }
 else if (test == "copy-empty")
 {
  Dar<int> source(0);
  Dar<int> copy(source);
  Check(copy.getCount() == 0, "copy is empty");
  copy.addElement(1);
  Check(copy.getCount() == 1 && copy[0] == 1, "copy can grow");
 }
 else Check(false, "unknown scenario");
}

int main(int argc, char** argv)
{
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


def supports_asan(compiler, folder):
    probe, output = Path(folder) / 'probe.cpp', Path(folder) / 'probe.exe'
    probe.write_text('int main() { return 0; }\n')
    result = subprocess.run(compiler + ['-fsanitize=address', str(probe), '-o', str(output)],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return result.returncode == 0


def main():
    scenarios = ['destroy-frees', 'copy-independent', 'copy-grow', 'assign-independent',
                 'assign-grow', 'assign-replaces', 'self-assign', 'copy-empty']
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui1-dar-') as folder:
        source_file, executable = Path(folder) / 'test.cpp', Path(folder) / 'test.exe'
        source_file.write_text(TESTS)
        compiler = shlex.split(os.environ.get('CXX', 'clang++' if os.name == 'nt' else 'c++'))
        # VGUI_EXPORTS matches the vgui target, so VGUIAPI is not dllimport on Windows.
        flags = ['-std=c++17', '-g', '-DVGUI_EXPORTS', f'-I{VGUI / "include"}']
        if supports_asan(compiler, folder):
            flags.append('-fsanitize=address')
        subprocess.run(compiler + flags + [str(source_file), '-o', str(executable)], check=True)
        failed = []
        for scenario in scenarios:
            result = subprocess.run([str(executable), scenario])
            if result.returncode:
                failed.append(scenario)
        if failed:
            raise SystemExit('FAILED: ' + ', '.join(failed))


if __name__ == '__main__':
    main()
