"""Exercise the production career task ConstructStringWithReordering with reordered, repeated, odd and overflowing formats.

Run: python tests/gameui_career_task_string_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'
# a format the function never finishes fails instead of hanging the run
TIMEOUT_SECONDS = 60


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>

namespace vgui2 {
// ASCII only, truncated and terminated like the production conversion
struct Localize {
 int ConvertANSIToUnicode(const char* ansi, wchar_t* unicode, int bytes) {
  int capacity = bytes / (int)sizeof(wchar_t), written = 0;
  while (ansi[written] && written < capacity - 1) { unicode[written] = (wchar_t)ansi[written]; ++written; }
  unicode[written] = 0;
  return written + 1;
 }
};
static Localize table;
Localize* localize() { return &table; }
}
'''

TESTS = r'''
// the output buffer is exactly capacity characters on the heap, so writing past it is reported
static std::wstring Construct(const wchar_t* format, int capacity, const wchar_t* first, const wchar_t* second) {
 std::wstring formatText = format ? format : L"";
 wchar_t* output = new wchar_t[capacity];
 std::wmemset(output, L'x', capacity);
 ConstructStringWithReordering(output, capacity * (int)sizeof(wchar_t), format ? &formatText[0] : nullptr, 2, first, second);
 assert(std::wmemchr(output, 0, capacity));
 std::wstring text = output;
 delete[] output;
 return text;
}

static void ParametersFollowTheirPlaceholders() {
 assert(std::wstring(L"a and b") == Construct(L"%s1 and %s2", 32, L"a", L"b"));
 assert(std::wstring(L"b then a") == Construct(L"%s2 then %s1", 32, L"a", L"b"));
 assert(std::wstring(L"x a %s3") == Construct(L"x %s1 %s3", 32, L"a", L"b"));
 assert(std::wstring(L"plain") == Construct(L"plain", 32, L"a", L"b"));
 assert(std::wstring(L"") == Construct(nullptr, 32, L"a", L"b"));
 puts("PASS: parameters follow their placeholders, unknown placeholders are kept and a NULL format is empty");
}

static void RepeatedPlaceholdersTakeTheirOwnParameter() {
 assert(std::wstring(L"a a") == Construct(L"%s1 %s1", 32, L"a", L"b"));
 assert(std::wstring(L"b a b") == Construct(L"%s2 %s1 %s2", 32, L"a", L"b"));
 assert(std::wstring(L"a b a b") == Construct(L"%s1 %s2 %s1 %s2", 32, L"a", L"b"));
 assert(std::wstring(L"b b a") == Construct(L"%s2 %s2 %s1", 32, L"a", L"b"));
 assert(std::wstring(L"abab") == Construct(L"%s1%s1%s1", 5, L"ab", L"b"));
 puts("PASS: a placeholder that appears more than once takes its own parameter every time, also when cut");
}

static void OtherPercentSequencesAreKept() {
 assert(std::wstring(L"100%s done a") == Construct(L"100%s done %s1", 32, L"a", L"b"));
 assert(std::wstring(L"%sx%s") == Construct(L"%sx%s", 32, L"a", L"b"));
 assert(std::wstring(L"%s0 then a") == Construct(L"%s0 then %s1", 32, L"a", L"b"));
 puts("PASS: %s without a digit and %s0 are kept as text and do not stop or index the parameters");
}

static void LongOutputIsCutBeforeTheLastCharacter() {
 assert(std::wstring(L"abcxyz") == Construct(L"abc%s1", 7, L"xyz", L"b"));
 assert(std::wstring(L"abcxy") == Construct(L"abc%s1def", 6, L"xyz", L"b"));
 assert(std::wstring(L"abc") == Construct(L"abc%s1def", 4, L"xyz", L"b"));
 assert(std::wstring(L"ab") == Construct(L"abc%s1def", 3, L"xyz", L"b"));
 assert(std::wstring(L"abc") == Construct(L"abcdef", 4, L"a", L"b"));
 assert(std::wstring(L"") == Construct(L"abc%s1", 1, L"xyz", L"b"));
 puts("PASS: text and parameters that fill or overflow the buffer are cut and terminated inside it");
}

static void MissingParametersAndTinyBuffersStaySafe() {
 assert(std::wstring(L"ab") == Construct(L"ab%s1cd", 32, nullptr, L"b"));
 wchar_t untouched = L'x';
 wchar_t format[] = L"%s1";
 ConstructStringWithReordering(&untouched, (int)sizeof(wchar_t) - 1, format, 2, L"a", L"b");
 ConstructStringWithReordering(&untouched, 0, nullptr, 2, L"a", L"b");
 assert(L'x' == untouched);
 puts("PASS: a NULL parameter ends a terminated string and a buffer smaller than a character is not touched");
}

int main() {
 ParametersFollowTheirPlaceholders();
 RepeatedPlaceholdersTakeTheirOwnParameter();
 OtherPercentSequencesAreKept();
 LongOutputIsCutBeforeTheLastCharacter();
 MissingParametersAndTinyBuffersStaySafe();
}
'''


def main():
    source = (ROOT / 'GameUI/CareerGameTasks.cpp').read_text(encoding='utf-8')
    code = STUBS + function(source, 'void ConstructStringWithReordering( wchar_t* output, int bufferSizeInBytes,') + '\n' + TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-gameui-career-task-string-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder, timeout=TIMEOUT_SECONDS)


if __name__ == '__main__':
    main()
