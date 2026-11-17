"""Compile the production VGUI1 SurfaceBase mode-info list handling (#243).

Run: python tests/vgui1_surface_mode_info_tests.py
Requires a C++17 compiler (CXX, or clang++ on Windows / c++ elsewhere).
AddressSanitizer (including leak checks where supported) is enabled when the
compiler supports it.

The mode-info methods and destructor are extracted from SurfaceBase.cpp and run
against the real VGUI_Dar.h. strdup/_strdup and free are counted, so each
scenario checks that every mode name addModeInfo allocated is freed.

- empty-reset: resetModeInfo on an empty list keeps it empty.
- reset-frees: resetModeInfo frees the names it drops.
- repeat-enumerate: the platform getModeInfoCount pattern (reset + add) does
  not grow the number of live names, and the reused slots read back correctly.
- destroy-frees: destroying the surface frees the remaining names.
- double-reset: a second resetModeInfo does not free a name twice.
"""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
VGUI = ROOT / 'GoldSrc-SDK/utils/vgui/lib-src/vgui'


def block(source, start_marker, end_marker):
    start = source.index(start_marker)
    return source[start:source.index(end_marker, start) + len(end_marker)]


PRELUDE = r'''
#include "VGUI_Dar.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

using namespace vgui;

static void Check(bool condition, const char* message)
{
 if (!condition) throw std::runtime_error(message);
}

static int liveNames = 0;

static char* CountedStrdup(const char* text)
{
 char* copy = static_cast<char*>(std::malloc(std::strlen(text) + 1));
 std::strcpy(copy, text);
 ++liveNames;
 return copy;
}

static void CountedFree(void* memory)
{
 if (memory != NULL) --liveNames;
 std::free(memory);
}

#undef strdup
#undef _strdup
#undef free
#define strdup CountedStrdup
#define _strdup CountedStrdup
#define free CountedFree

class App
{
public:
 static App* getInstance() { static App app; return &app; }
 void surfaceBaseDeleted(class SurfaceBase*) {}
};

class SurfaceBase
{
public:
 ~SurfaceBase();
 void resetModeInfo();
 int getModeInfoCount();
 bool getModeInfo(int index, int& wide, int& tall, int& bpp);
 void addModeInfo(int wide, int tall, int bpp);
private:
 Dar<char*> _modeInfoDar;
};
'''

TESTS = r'''
#undef free

static const int ENUMERATE_REPEATS = 100;

// Same sequence as Surface::getModeInfoCount on Win32 and Linux.
static void Enumerate(SurfaceBase& surface)
{
 surface.resetModeInfo();
 surface.addModeInfo(640, 480, 16);
 surface.addModeInfo(800, 600, 16);
}

static void Run(const std::string& test)
{
 SurfaceBase* surface = new SurfaceBase();
 if (test == "empty-reset")
 {
  surface->resetModeInfo();
  Check(surface->getModeInfoCount() == 0, "empty list stays empty");
  Check(liveNames == 0, "no names allocated");
 }
 else if (test == "reset-frees")
 {
  Enumerate(*surface);
  Check(liveNames == 2, "two names allocated");
  surface->resetModeInfo();
  Check(surface->getModeInfoCount() == 0, "reset empties the list");
  Check(liveNames == 0, "reset frees the names");
 }
 else if (test == "repeat-enumerate")
 {
  for (int i = 0; i < ENUMERATE_REPEATS; ++i)
  {
   Enumerate(*surface);
  }
  Check(surface->getModeInfoCount() == 2, "two modes listed");
  Check(liveNames == 2, "only the current names are live");
  int wide = 0, tall = 0, bpp = 0;
  Check(surface->getModeInfo(1, wide, tall, bpp), "second mode readable");
  Check(wide == 800 && tall == 600 && bpp == 16, "second mode reads back");
 }
 else if (test == "destroy-frees")
 {
  Enumerate(*surface);
  surface->addModeInfo(1024, 768, 32);
  Check(liveNames == 3, "three names allocated");
 }
 else if (test == "double-reset")
 {
  Enumerate(*surface);
  surface->resetModeInfo();
  surface->resetModeInfo();
  Check(liveNames == 0, "names freed once");
 }
 else Check(false, "unknown scenario");
 delete surface;
 Check(liveNames == 0, "destroying the surface frees the names");
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


def surface_methods(source):
    return '\n'.join(block(source, signature, '\n}\n') for signature in (
        'void freeModeInfo(',
        'SurfaceBase::~SurfaceBase()',
        'void SurfaceBase::resetModeInfo()',
        'int SurfaceBase::getModeInfoCount()',
        'bool SurfaceBase::getModeInfo(',
        'void SurfaceBase::addModeInfo('))


def supports_asan(compiler, folder):
    probe, output = Path(folder) / 'probe.cpp', Path(folder) / 'probe.exe'
    probe.write_text('int main() { return 0; }\n')
    result = subprocess.run(compiler + ['-fsanitize=address', str(probe), '-o', str(output)],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return result.returncode == 0


def main():
    source = (VGUI / 'SurfaceBase.cpp').read_text()
    program = '\n'.join([PRELUDE, surface_methods(source), TESTS])

    scenarios = ['empty-reset', 'reset-frees', 'repeat-enumerate', 'destroy-frees', 'double-reset']
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui1-mode-info-') as folder:
        source_file, executable = Path(folder) / 'test.cpp', Path(folder) / 'test.exe'
        source_file.write_text(program)
        compiler = shlex.split(os.environ.get('CXX', 'clang++' if os.name == 'nt' else 'c++'))
        flags = ['-std=c++17', '-g', f'-I{VGUI / "include"}']
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
