"""Exercise the production wide/UTF-8 conversions with exact, short and invalid input."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


def main():
    source = (ROOT / 'GoldSrc-SDK/vgui2/src/LocalizedStringTable.cpp').read_text(encoding='utf-8')
    code = r'''
#include <cassert>
#include <cstring>
#include <cwchar>
struct CLocalizedStringTable {
 int ConvertANSIToUnicode(const char*, wchar_t*, int);
 int ConvertUnicodeToANSI(const wchar_t*, char*, int);
};
'''
    for name in ('static int DecodeUTF8CodePoint(', 'static int EncodeWideCodePoint(',
                 'int CLocalizedStringTable::ConvertANSIToUnicode(',
                 'static int DecodeWideCodePoint(', 'static int EncodeUTF8CodePoint(',
                 'int CLocalizedStringTable::ConvertUnicodeToANSI('):
        code += function(source, name) + '\n'
    code += r'''
static CLocalizedStringTable table;

// converts into a buffer prefilled with stale bytes and checks the result and the text
static void check(const wchar_t* input, int size, int expectedResult, const char* expectedText) {
 char buffer[64];
 std::memset(buffer, 'x', sizeof(buffer));
 assert(expectedResult == table.ConvertUnicodeToANSI(input, buffer, size));
 assert(0 == std::strcmp(expectedText, buffer));
}

// same as check, for UTF-8 input; size counts wide characters
static void checkWide(const char* input, int size, int expectedResult, const wchar_t* expectedText) {
 wchar_t buffer[64];
 for (wchar_t& unit : buffer) unit = L'x';
 assert(expectedResult == table.ConvertANSIToUnicode(input, buffer, size * (int)sizeof(wchar_t)));
 assert(0 == std::wcscmp(expectedText, buffer));
}

static void testANSIToUnicode() {
 const int pairUnits = sizeof(wchar_t) == 2 ? 2 : 1;

 // length query counts wide characters including the terminator and never touches the buffer
 assert(1 == table.ConvertANSIToUnicode("", nullptr, 0));
 assert(6 == table.ConvertANSIToUnicode("hello", nullptr, 0));
 assert(2 == table.ConvertANSIToUnicode("\xE4\xB8\xAD", nullptr, 0));
 assert(1 + pairUnits == table.ConvertANSIToUnicode("\xF0\x9F\x98\x80", nullptr, 0));
 wchar_t untouched[2] = {L'x', L'x'};
 assert(6 == table.ConvertANSIToUnicode("hello", untouched, (int)sizeof(wchar_t) - 1));
 assert(L'x' == untouched[0]);
 assert(0 == table.ConvertANSIToUnicode(nullptr, nullptr, 0));

 // fits exactly, one wide character short cuts at a whole character
 checkWide("hello", 6, 6, L"hello");
 checkWide("hello", 5, 0, L"hell");
 checkWide("hello", 1, 0, L"");
 checkWide("", 1, 1, L"");
 checkWide(nullptr, 8, 0, L"");
 wchar_t odd[4];
 for (wchar_t& unit : odd) unit = L'x';
 assert(0 == table.ConvertANSIToUnicode("hi", odd, 3 * (int)sizeof(wchar_t) - 1));
 assert(0 == std::wcscmp(L"h", odd));

 // multibyte characters, and supplementary characters are never split into half a pair
 checkWide("a\xC3\xA9\xE4\xB8\xAD", 4, 4, L"a\u00E9\u4E2D");
 checkWide("a\xF0\x9F\x98\x80", 1 + pairUnits, 0, L"a");
 checkWide("a\xF0\x9F\x98\x80", 2 + pairUnits, 2 + pairUnits, L"a\U0001F600");

 // invalid input becomes U+FFFD, one per maximal subpart, and the terminator is never consumed
 checkWide("\x80" "a", 8, 3, L"\uFFFDa");
 checkWide("\xE4\xB8" "a", 8, 3, L"\uFFFDa");
 checkWide("a\xE4\xB8", 8, 3, L"a\uFFFD");
 checkWide("\xC0\xAF", 8, 3, L"\uFFFD\uFFFD");
 checkWide("\xE0\x80\xAF", 8, 4, L"\uFFFD\uFFFD\uFFFD");
 checkWide("\xED\xA0\x80", 8, 4, L"\uFFFD\uFFFD\uFFFD");
 checkWide("\xF4\x90\x80\x80", 8, 5, L"\uFFFD\uFFFD\uFFFD\uFFFD");
 checkWide("\xF5\xFF", 8, 3, L"\uFFFD\uFFFD");
 checkWide("\xEF\xBF\xBF\xF4\x8F\xBF\xBF", 8, 2 + pairUnits, L"\uFFFF\U0010FFFF");

 // a round trip through both conversions keeps the text
 const wchar_t original[] = L"a\u00E9\u4E2D\U0001F600";
 char utf8[16];
 wchar_t wide[8];
 assert(11 == table.ConvertUnicodeToANSI(original, utf8, sizeof(utf8)));
 assert(3 + pairUnits + 1 == table.ConvertANSIToUnicode(utf8, wide, sizeof(wide)));
 assert(0 == std::wcscmp(original, wide));
}

int main() {
 testANSIToUnicode();

 // length query includes the terminator and never touches the buffer
 assert(1 == table.ConvertUnicodeToANSI(L"", nullptr, 0));
 assert(6 == table.ConvertUnicodeToANSI(L"hello", nullptr, 0));
 char untouched = 'x';
 assert(4 == table.ConvertUnicodeToANSI(L"\u4E2D", &untouched, 0));
 assert('x' == untouched);
 assert(0 == table.ConvertUnicodeToANSI(nullptr, nullptr, 0));

 // fits exactly, and one byte short cuts at a whole character
 check(L"hello", 6, 6, "hello");
 check(L"hello", 5, 0, "hell");
 check(L"hello", 1, 0, "");
 check(L"", 1, 1, "");
 check(nullptr, 8, 0, "");

 // 11 CJK characters need 34 bytes; 31 bytes keep 10 of them
 const wchar_t cjk[] = L"\u4E2D\u4E2D\u4E2D\u4E2D\u4E2D\u4E2D\u4E2D\u4E2D\u4E2D\u4E2D\u4E2D";
 assert(34 == table.ConvertUnicodeToANSI(cjk, nullptr, 0));
 char cjkUtf8[34] = {};
 for (int i = 0; i < 11; ++i) std::memcpy(cjkUtf8 + i * 3, "\xE4\xB8\xAD", 3);
 check(cjk, 34, 34, cjkUtf8);
 cjkUtf8[30] = 0;
 check(cjk, 31, 0, cjkUtf8);

 // two and four byte sequences are never split
 check(L"a\u00E9", 3, 0, "a");
 check(L"a\u00E9", 4, 4, "a\xC3\xA9");
 check(L"\U0001F600", 4, 0, "");
 check(L"\U0001F600", 5, 5, "\xF0\x9F\x98\x80");

 // unpaired surrogates become U+FFFD
 if (sizeof(wchar_t) == 2) {
  const wchar_t lone[] = {(wchar_t)0xD800, L'a', 0};
  check(lone, 8, 5, "\xEF\xBF\xBD" "a");
 }
}
'''
    with tempfile.TemporaryDirectory(prefix='goldsrc-localize-utf8-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([path / 'test.cpp'], path / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True)
    print('PASS: wide and UTF-8 length queries, exact fit, whole-character truncation and invalid input')


if __name__ == '__main__':
    main()
