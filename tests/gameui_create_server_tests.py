"""Exercise the production create-server setting copies with stubs.

Run: python tests/gameui_create_server_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'


def function(source, signature):
    start = re.search(re.escape(signature) + r'[^\n;]*\n\{', source).start()
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
'''

TESTS = r'''
// CCreateMultiplayerGameDialog::StartServer copies into char[64]
static const size_t SETTING_BUFFER_SIZE = 64;
static const char* MAP_FORBIDDEN = ";\"\n\r";
static const char* QUOTED_FORBIDDEN = "\"\n\r";
static const char SENTINEL = 'x';

static bool Copy(const char* value, const char* forbidden, std::string* result) {
 char* buffer = new char[SETTING_BUFFER_SIZE];
 memset(buffer, SENTINEL, SETTING_BUFFER_SIZE);
 bool copied = CopyServerSetting(buffer, SETTING_BUFFER_SIZE, value, forbidden);
 if (copied) *result = buffer;
 else for (size_t i = 0; i < SETTING_BUFFER_SIZE; i++) assert(SENTINEL == buffer[i]);
 delete[] buffer;
 return copied;
}

static void LongestFittingValueIsKept() {
 std::string result;
 std::string value(63, 'a');
 assert(Copy(value.c_str(), QUOTED_FORBIDDEN, &result));
 assert(value == result);
 assert(Copy(value.c_str(), MAP_FORBIDDEN, &result));
 assert(value == result);
 assert(Copy("", QUOTED_FORBIDDEN, &result));
 assert(std::string() == result);
 puts("PASS: a 63 byte value and an empty value are copied with their terminator");
}

static void LongValuesAreRejected() {
 std::string result;
 for (size_t length : {64, 127}) {
  std::string value(length, 'a');
  assert(!Copy(value.c_str(), QUOTED_FORBIDDEN, &result));
  assert(!Copy(value.c_str(), MAP_FORBIDDEN, &result));
 }
 puts("PASS: 64 and 127 byte values are rejected instead of truncated");
}

static void Utf8ValuesAreCountedInBytes() {
 std::string result;
 std::string fits, overflows = "a";
 // 21 three-byte characters are 63 bytes
 for (int i = 0; i < 21; i++) fits += "\xE4\xB8\xAD";
 overflows += fits;
 assert(Copy(fits.c_str(), QUOTED_FORBIDDEN, &result));
 assert(fits == result);
 assert(!Copy(overflows.c_str(), QUOTED_FORBIDDEN, &result));
 puts("PASS: UTF-8 values are limited by bytes and never split");
}

static void MissingMapIsRejected() {
 std::string result;
 assert(!Copy(nullptr, MAP_FORBIDDEN, &result));
 puts("PASS: a missing map is rejected");
}

static void CommandSeparatorsAreRejected() {
 std::string result;
 for (const char* value : {"a\"\nquit\n\"", "a\nquit", "a\rquit"}) {
  assert(!Copy(value, QUOTED_FORBIDDEN, &result));
  assert(!Copy(value, MAP_FORBIDDEN, &result));
 }
 assert(!Copy("crossfire;quit", MAP_FORBIDDEN, &result));
 // a quoted value keeps its ';' because the command buffer only splits outside quotes
 assert(Copy("Bob's; server", QUOTED_FORBIDDEN, &result));
 assert(std::string("Bob's; server") == result);
 puts("PASS: values that could split the console command are rejected");
}
'''


def main():
    utils = (ROOT / 'GameUI/StringUtils.cpp').read_text(encoding='utf-8')
    source = (ROOT / 'GameUI/CreateMultiplayerGameDialog.cpp').read_text(encoding='utf-8')
    code = STUBS + function(utils, 'bool UTIL_CopyString(') + function(source, 'static bool CopyServerSetting(') + TESTS
    code += ('\nint main() { LongestFittingValueIsKept(); LongValuesAreRejected(); '
             'Utf8ValuesAreCountedInBytes(); MissingMapIsRejected(); CommandSeparatorsAreRejected(); }\n')
    with tempfile.TemporaryDirectory(prefix='goldsrc-gameui-create-server-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([path / 'test.cpp'], path / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=path)


if __name__ == '__main__':
    main()
