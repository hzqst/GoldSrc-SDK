"""Page the text the halflife-cli VGUI2 automation hooks report (vgui2/controls/VGUI2TextPage.h, USE_HLCLI).

Run: python tests/vgui2_automation_text_tests.py
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

TESTS = r'''
#include "VGUI2TextPage.h"

#include <cstdio>
#include <cstdlib>
#include <string>

using vgui2automation::SliceText;

static void Require(bool condition, const char* message) {
 if (condition) return;
 printf("FAIL: %s\n", message);
 exit(1);
}

int main() {
 auto first = SliceText(L"abcdef", 6, 0, 3);
 Require(first.text == L"abc" && first.length == 6 && first.next == 3, "the first page holds the limit and points at the next one");
 auto last = SliceText(L"abcdef", 6, 3, 3);
 Require(last.text == L"def" && last.next == -1, "the last page has no next page");
 auto empty = SliceText(L"", 0, 0, 3);
 Require(empty.text.empty() && empty.length == 0 && empty.next == -1, "empty text gives an empty page");
 auto past = SliceText(L"abc", 3, 99, 3);
 Require(past.text.empty() && past.length == 3 && past.next == -1, "an offset past the end gives an empty page and the full length");
 // U+1F600 is one scalar value: a surrogate pair where wchar_t is UTF-16, one unit where it is UTF-32
 const bool utf16 = sizeof(wchar_t) == 2;
 const wchar_t unicode[] = { L'A', static_cast<wchar_t>(utf16 ? 0xd83d : 0x1f600), static_cast<wchar_t>(utf16 ? 0xde00 : L'B'), L'B', 0 };
 const size_t unicodeLength = utf16 ? 4 : 3;
 auto astral = SliceText(unicode, unicodeLength, 1, 1);
 Require(astral.text.size() == (utf16 ? 2u : 1u) && astral.length == 3 && astral.next == 2, "a page keeps a surrogate pair together and counts it once");
 auto after = SliceText(unicode, unicodeLength, 2, 1);
 Require(after.text == L"B" && after.next == -1, "the page after a surrogate pair starts at the next scalar value");
 auto nul = SliceText(L"abc\0tail", 8, 1, 9);
 Require(nul.text == L"bc" && nul.length == 3, "text ends at the first NUL even when the buffer is longer");
 auto clamped = SliceText(L"abc", 3, -1, 0);
 Require(clamped.text == L"a" && clamped.next == 1, "a negative offset and a zero limit are clamped to 0 and 1");
 puts("PASS: VGUI2 automation text pages");
}
'''


def main():
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-automation-text-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(TESTS, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', includes=[ROOT / 'vgui2/controls'],
                                     defines=['USE_HLCLI=1'], sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
