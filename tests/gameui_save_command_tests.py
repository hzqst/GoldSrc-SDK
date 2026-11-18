"""Exercise the production GameUI save / load command formatting with stubs.

Run: python tests/gameui_save_command_tests.py
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


def constants(source):
    return ''.join(re.findall(r'^static const char \w+\[\] = .*;\n', source, re.M))


STUBS = r'''
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
'''

TESTS = r'''
// the dialogs format into char[256]; names are save file names including ".sav"
static const size_t COMMAND_BUFFER_SIZE = 256;

static std::string SaveName(size_t length) { return std::string(length - 4, 'a') + ".sav"; }

static bool Format(const char* command, const std::string& name, std::string* result) {
 char* buffer = new char[COMMAND_BUFFER_SIZE];
 bool formatted = FormatSaveGameCommand(buffer, COMMAND_BUFFER_SIZE, command, name.c_str());
 if (formatted) *result = buffer;
 delete[] buffer;
 return formatted;
}

static void ShortNamesAreFormatted() {
 std::string result;
 assert(Format("load", "quick.sav", &result));
 assert(std::string("load quick.sav\n") == result);
 assert(Format("save", "Half-Life-000.sav", &result));
 assert(std::string("save Half-Life-000.sav\n") == result);
 puts("PASS: short save names are formatted");
}

static void LongestFittingNameIsKept() {
 std::string result;
 assert(Format("load", SaveName(249), &result));
 assert("load " + SaveName(249) + "\n" == result);
 puts("PASS: a 249 byte save name fits in the command");
}

static void LongNamesAreRejected() {
 std::string result;
 for (size_t length : {250, 255}) {
  assert(!Format("load", SaveName(length), &result));
  assert(!Format("save", SaveName(length), &result));
 }
 puts("PASS: save names that do not fit are rejected instead of truncated");
}

static void CommandSeparatorsAreRejected() {
 std::string result;
 for (const char* name : {"a;quit.sav", "a\"b.sav", "a\nquit.sav", "a\rquit.sav"}) {
  assert(!Format("load", name, &result));
  assert(!Format("save", name, &result));
 }
 puts("PASS: save names that could split the console command are rejected");
}
'''


def main():
    utils = (ROOT / 'common/StringUtils.cpp').read_text(encoding='utf-8')
    source = (ROOT / 'GameUI/SaveGameDialog.cpp').read_text(encoding='utf-8')
    code = STUBS + constants(utils) + function(utils, 'bool UTIL_FormatString(') + function(utils, 'bool UTIL_IsUnquotedCommandArgument(')
    code += function(source, 'bool FormatSaveGameCommand(') + TESTS
    code += ('\nint main() { ShortNamesAreFormatted(); LongestFittingNameIsKept(); '
             'LongNamesAreRejected(); CommandSeparatorsAreRejected(); }\n')
    with tempfile.TemporaryDirectory(prefix='goldsrc-gameui-save-command-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([path / 'test.cpp'], path / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=path)


if __name__ == '__main__':
    main()
