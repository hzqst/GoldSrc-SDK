"""Exercise the production wide-to-UTF-8 conversion with exact and short buffers."""
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
struct CLocalizedStringTable { int ConvertUnicodeToANSI(const wchar_t*, char*, int); };
'''
    for name in ('static int DecodeWideCodePoint(', 'static int EncodeUTF8CodePoint(',
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

int main() {
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
    print('PASS: UTF-8 length query, exact fit and whole-character truncation')


if __name__ == '__main__':
    main()
