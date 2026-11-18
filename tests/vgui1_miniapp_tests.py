"""Compile the production VGUI1 MiniApp and check how it owns its name (#240).

Run: python tests/vgui1_miniapp_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
AddressSanitizer (including leak checks where supported) is enabled when the
compiler supports it; without it only the value checks run.

MiniApp.cpp is built twice, with vgui_strcpy extracted from vgui.cpp (the rest of
vgui.cpp only builds for 32-bit targets): with VGUI_ABI_COMPAT_MODE=1 (the
original vgui.dll layout, no destructor) and with VGUI_ABI_COMPAT_MODE=0 (a
virtual destructor). The compatible build cannot free the last name, so its
scenarios end with setName("") to release it; any leak left is a name that an
earlier setName or operator= lost.

- unnamed: an unnamed MiniApp reports an empty name.
- rename: renaming replaces the name and frees the previous one.
- empty-name / null-name: setName("") and setName(NULL) give an empty name.
- self-name: setName(_name) keeps the name.
- truncate: getName truncates to the buffer and stays terminated.
- copy-independent: a copy-constructed MiniApp does not share its name.
- assign-independent: the same for operator=, which frees the name it replaces.
- self-assign: assigning a MiniApp to itself keeps its name.
- destroy-frees (VGUI_ABI_COMPAT_MODE=0 only): deleting through MiniApp* frees
  the last name.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1]
VGUI = ROOT / 'GoldSrc-SDK/utils/vgui/lib-src/vgui'


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


TESTS = r'''
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

// The public VGUI_MiniApp.h makes _name protected, so derived MiniApps can pass
// it to setName; give the test the same access to the internal header.
#define private protected
#include "VGUI_MiniApp.h"
#undef private

using namespace vgui;

static const int NAME_BUFFER_SIZE = 64;
static const int SHORT_BUFFER_SIZE = 4;

static void Check(bool condition, const char* message)
{
 if (!condition) throw std::runtime_error(message);
}

class TestApp : public MiniApp
{
public:
 virtual Frame* createInstance() { return NULL; }
 void rename(const char* name) { setName(name); }
 void renameToOwnName() { setName(_name); }

 std::string name()
 {
  char buffer[NAME_BUFFER_SIZE];
  getName(buffer, NAME_BUFFER_SIZE);
  return buffer;
 }

 // The compatible build has no destructor; this frees the last name.
 void release() { setName(""); }
};

static void Run(const std::string& test)
{
 if (test == "unnamed")
 {
  TestApp app;
  Check(std::string("") == app.name(), "unnamed name is empty");
 }
 else if (test == "rename")
 {
  TestApp app;
  app.rename("first");
  app.rename("second");
  Check(std::string("second") == app.name(), "second name kept");
  app.release();
 }
 else if (test == "empty-name")
 {
  TestApp app;
  app.rename("first");
  app.rename("");
  Check(std::string("") == app.name(), "empty name");
 }
 else if (test == "null-name")
 {
  TestApp app;
  app.rename("first");
  app.rename(NULL);
  Check(std::string("") == app.name(), "null name is empty");
 }
 else if (test == "self-name")
 {
  TestApp app;
  app.rename("self");
  app.renameToOwnName();
  Check(std::string("self") == app.name(), "name kept");
  app.release();
 }
 else if (test == "truncate")
 {
  TestApp app;
  app.rename("truncated");
  char buffer[SHORT_BUFFER_SIZE];
  app.getName(buffer, SHORT_BUFFER_SIZE);
  Check(std::string("tru") == buffer, "name truncated");
  app.release();
 }
 else if (test == "copy-independent")
 {
  TestApp source;
  source.rename("source");
  TestApp copy(source);
  Check(std::string("source") == copy.name(), "copy has the name");
  copy.rename("copy");
  Check(std::string("source") == source.name(), "source name unchanged");
  source.release();
  Check(std::string("copy") == copy.name(), "copy name unchanged");
  copy.release();
 }
 else if (test == "assign-independent")
 {
  TestApp source;
  source.rename("source");
  TestApp copy;
  copy.rename("replaced");
  copy = source;
  Check(std::string("source") == copy.name(), "copy has the name");
  copy.rename("copy");
  Check(std::string("source") == source.name(), "source name unchanged");
  source.release();
  Check(std::string("copy") == copy.name(), "copy name unchanged");
  copy.release();
 }
 else if (test == "self-assign")
 {
  TestApp app;
  app.rename("self");
  TestApp& alias = app;
  app = alias;
  Check(std::string("self") == app.name(), "name kept");
  app.release();
 }
#if VGUI_ABI_COMPAT_MODE != 1
 else if (test == "destroy-frees")
 {
  MiniApp* app = new TestApp;
  static_cast<TestApp*>(app)->rename("destroyed");
  delete app;
 }
#endif
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

SCENARIOS = ['unnamed', 'rename', 'empty-name', 'null-name', 'self-name', 'truncate', 'copy-independent',
             'assign-independent', 'self-assign']
# VGUI_ABI_COMPAT_MODE value -> scenarios run against that build.
MODES = {'1': SCENARIOS, '0': SCENARIOS + ['destroy-frees']}


def main():
    failed = []
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui1-miniapp-') as folder:
        source_file = Path(folder) / 'test.cpp'
        strcpy = function((VGUI / 'vgui.cpp').read_text(), 'void vgui_strcpy(')
        source_file.write_text(TESTS + '\nnamespace vgui\n{\n' + strcpy + '\n}\n')
        for mode, scenarios in MODES.items():
            executable = Path(folder) / f'test-compat{mode}.exe'
            sources = [source_file, VGUI / 'MiniApp.cpp']
            # VGUI_EXPORTS matches the vgui target, so VGUIAPI is not dllimport on Windows.
            executable = compile_program(sources, executable, includes=[VGUI / 'include'],
                                         defines=['VGUI_EXPORTS', f'VGUI_ABI_COMPAT_MODE={mode}'], debug=True,
                                         sanitizers=['address'])
            for scenario in scenarios:
                result = subprocess.run([str(executable), scenario])
                if result.returncode:
                    failed.append(f'{scenario} (VGUI_ABI_COMPAT_MODE={mode})')
    if failed:
        raise SystemExit('FAILED: ' + ', '.join(failed))


if __name__ == '__main__':
    main()
