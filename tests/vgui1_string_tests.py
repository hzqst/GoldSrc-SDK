"""Compile the production VGUI1 String and check how it owns its text (#208).

Run: python tests/vgui1_string_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
AddressSanitizer (including leak checks where supported) is enabled when the
compiler supports it; without it only the value checks run.

String.cpp is built once against the library header and once against the
public SDK header, so both declare the same members, including operator=.
The test replaces the global new / delete to count live allocations; every
scenario must end with none left.

- default-empty: a default String is empty and owns its buffer.
- text-copied: String(const char*) copies the text into its own buffer.
- null-text: String(NULL) gives an empty text.
- copy-independent: a copy-constructed String has its own buffer.
- assign-independent: operator= copies the text and frees the buffer it replaces.
- assign-literal: assigning a const char* goes through operator= as well.
- self-assign: assigning a String to itself keeps its text.
- concat-string / concat-chars: both operator+ overloads return the joined
  text without leaving a String wrapper or buffer behind.
A static_assert also checks that the object still holds only the text pointer.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1]
VGUI = ROOT / 'GoldSrc-SDK/utils/vgui'

TESTS = r'''
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string>

// The test reads the private text buffer.
#define private public
#include "VGUI_String.h"
#undef private

using namespace vgui;

static_assert(sizeof(String) == sizeof(char*), "String layout changed");

static int liveArrays;
static int liveObjects;

void* operator new[](size_t size)
{
 void* memory = std::malloc(size);
 if (memory == NULL) throw std::bad_alloc();
 ++liveArrays;
 return memory;
}

void operator delete[](void* memory) noexcept
{
 if (memory != NULL) --liveArrays;
 std::free(memory);
}

void operator delete[](void* memory, size_t) noexcept
{
 operator delete[](memory);
}

void* operator new(size_t size)
{
 void* memory = std::malloc(size);
 if (memory == NULL) throw std::bad_alloc();
 ++liveObjects;
 return memory;
}

void operator delete(void* memory) noexcept
{
 if (memory != NULL) --liveObjects;
 std::free(memory);
}

void operator delete(void* memory, size_t) noexcept
{
 operator delete(memory);
}

static void Check(bool condition, const char* message)
{
 if (!condition) throw std::runtime_error(message);
}

static std::string Text(String& text)
{
 return text.getChars();
}

static void Run(const std::string& test)
{
 // A long scenario name allocates its std::string before the scenario starts.
 const int objects = liveObjects;

 if (test == "default-empty")
 {
  String text;
  Check(std::string("") == Text(text), "default text is empty");
  Check(0 == text.getCount(), "default count is zero");
  Check(1 == liveArrays, "default text owns its buffer");
 }
 else if (test == "text-copied")
 {
  char source[] = "abc";
  String text(source);
  Check(source != text._text, "text has its own buffer");
  source[0] = 'x';
  Check(std::string("abc") == Text(text), "text kept");
  Check(3 == text.getCount(), "count");
 }
 else if (test == "null-text")
 {
  String text(NULL);
  Check(std::string("") == Text(text), "null text is empty");
 }
 else if (test == "copy-independent")
 {
  String source("source");
  String copy(source);
  Check(source._text != copy._text, "copy has its own buffer");
  Check(std::string("source") == Text(copy), "copy has the text");
  Check(2 == liveArrays, "two live buffers");
 }
 else if (test == "assign-independent")
 {
  String source("source");
  String copy("replaced text");
  copy = source;
  Check(source._text != copy._text, "copy has its own buffer");
  Check(std::string("source") == Text(copy), "copy has the text");
  Check(2 == liveArrays, "replaced buffer freed");
 }
 else if (test == "assign-literal")
 {
  String text("replaced text");
  text = "literal";
  Check(std::string("literal") == Text(text), "literal assigned");
  Check(1 == liveArrays, "replaced and temporary buffers freed");
 }
 else if (test == "self-assign")
 {
  String text("self");
  String& alias = text;
  text = alias;
  Check(std::string("self") == Text(text), "text kept");
  Check(1 == liveArrays, "one live buffer");
 }
 else if (test == "concat-string")
 {
  String left("abc");
  String right("def");
  {
   String joined = left + right;
   Check(std::string("abcdef") == Text(joined), "joined text");
   Check(3 == liveArrays, "only the joined buffer added");
  }
  Check(std::string("abc") == Text(left), "left unchanged");
  Check(std::string("def") == Text(right), "right unchanged");
 }
 else if (test == "concat-chars")
 {
  String left("abc");
  {
   String joined = left + "def";
   Check(std::string("abcdef") == Text(joined), "joined text");
   Check(2 == liveArrays, "only the joined buffer added");
  }
  Check(std::string("abc") == Text(left), "left unchanged");
 }
 else Check(false, "unknown scenario");

 Check(0 == liveArrays, "no text buffer left");
 Check(objects == liveObjects, "no String object left");
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

SCENARIOS = ['default-empty', 'text-copied', 'null-text', 'copy-independent', 'assign-independent', 'assign-literal',
             'self-assign', 'concat-string', 'concat-chars']
# Header set name -> include directory String.cpp and the test are built against.
HEADERS = {'library': VGUI / 'lib-src/vgui/include', 'public': VGUI / 'include'}


def main():
    failed = []
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui1-string-') as folder:
        test_file = Path(folder) / 'test.cpp'
        test_file.write_text(TESTS)
        for name, include in HEADERS.items():
            executable = compile_program([test_file, VGUI / 'lib-src/vgui/String.cpp'],
                                         Path(folder) / f'test-{name}.exe', includes=[include],
                                         defines=['VGUI_STATIC'], debug=True, sanitizers=['address'])
            for scenario in SCENARIOS:
                result = subprocess.run([str(executable), scenario])
                if result.returncode:
                    failed.append(f'{scenario} ({name} header)')
    if failed:
        raise SystemExit('FAILED: ' + ', '.join(failed))


if __name__ == '__main__':
    main()
