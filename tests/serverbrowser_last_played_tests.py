"""Exercise production server browser last played date formatting with stubs.

Run: python tests/serverbrowser_last_played_tests.py
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
#include <clocale>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cwchar>
#include <ctype.h>
#include <string>
#include <time.h>
#include <wchar.h>
typedef unsigned int uint32;
// records how LastPlayed was stored
struct KeyValues {
 int narrowWrites=0, wideWrites=0; std::wstring wide;
 void SetString(const char*, const char*) { ++narrowWrites; }
 void SetWString(const char* name, const wchar_t* value) { assert(!strcmp("LastPlayed", name)); ++wideWrites; wide=value; }
};
'''

TESTS = r'''
// 2026-10-14 is a Wednesday; noon UTC keeps the date in every time zone
static const uint32 TIME=1791979200u;

static std::wstring Expected(const wchar_t* format) {
 time_t value=TIME; wchar_t buf[64];
 size_t len=wcsftime(buf, 64, format, localtime(&value));
 assert(len); return std::wstring(buf, len);
}

static void EnglishDateUsesLowercaseMarker() {
 setlocale(LC_TIME, "C");
 KeyValues kv; SetServerLastPlayed(&kv, TIME);
 assert(0==kv.narrowWrites && 1==kv.wideWrites);
 time_t value=TIME;
 std::wstring expected=Expected(L"%a %d %b %I:%M")+(localtime(&value)->tm_hour<12?L"am":L"pm");
 assert(expected==kv.wide);
 puts("PASS: a C locale date is stored as wide text with a lowercase am / pm");
}

static void LocalizedDateIsKept() {
#if defined(_WIN32)
 if (!setlocale(LC_TIME, ".936")) { puts("SKIP: code page 936 locale is not available"); return; }
 KeyValues kv; SetServerLastPlayed(&kv, TIME);
 assert(0==kv.narrowWrites && 1==kv.wideWrites);
 assert(Expected(L"%a %d %b %I:%M%p")==kv.wide);
 bool nonASCII=false; for (wchar_t ch : kv.wide) nonASCII|=ch>0x7f;
 assert(nonASCII);
 setlocale(LC_TIME, "C");
 puts("PASS: a Chinese locale date is stored as unchanged wide text");
#endif
}

static void ZeroTimeIsNotStored() {
 KeyValues kv; SetServerLastPlayed(&kv, 0);
 assert(0==kv.narrowWrites && 0==kv.wideWrites);
 puts("PASS: servers never played have no date");
}
'''


def main():
    source = (ROOT / 'serverbrowser/BaseGamesPage.cpp').read_text(encoding='utf-8')
    code = STUBS + function(source, 'static void SetServerLastPlayed(') + TESTS
    code += '\nint main() { EnglishDateUsesLowercaseMarker(); LocalizedDateIsKept(); ZeroTimeIsNotStored(); }\n'
    with tempfile.TemporaryDirectory(prefix='goldsrc-serverbrowser-last-played-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([path / 'test.cpp'], path / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=path)


if __name__ == '__main__':
    main()
