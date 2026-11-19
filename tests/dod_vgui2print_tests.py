"""Exercise the production DoD CHudVGUI2Print::VGUI2HudPrint with localized texts that are short, long or follow a longer one.

Run: python tests/dod_vgui2print_tests.py
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
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


def buffer_member(header):
    return re.search(r'wchar_t m_wCharBuf\[\d+\];', header).group(0)


STUBS = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <map>
#include <string>

namespace vgui2 {
struct Localize {
 wchar_t* Find(const char* name) {
  auto found = strings.find(name);
  return found == strings.end() ? nullptr : &found->second[0];
 }
 std::map<std::string, std::wstring> strings;
};
static Localize table;
Localize* localize() { return &table; }
}

struct EngineFunctions {
 float (*GetClientTime)();
};
static float ClientTime() { return 10; }
static EngineFunctions gEngfuncs = {ClientTime};

// the members VGUI2HudPrint uses; the text buffer comes last so writing past it leaves the heap object
class CHudVGUI2Print {
public:
 void VGUI2HudPrint(char* text, int x, int y, float red, float green, float blue);
 float m_flVGUI2StringTime = 0;
 float m_fR = 0;
 float m_fG = 0;
 float m_fB = 0;
 int m_iX = 0;
 int m_iY = 0;
 BUFFER
};
'''

TESTS = r'''
static std::wstring Text(size_t length) {
 std::wstring text;
 for (size_t i = 0; i < length; ++i) text += (wchar_t)(L'A' + i % 26);
 return text;
}

static void Print(CHudVGUI2Print& print, const char* token, const std::wstring& text) {
 vgui2::table.strings[token] = text;
 print.VGUI2HudPrint((char*)token, 1, 2, 0.5f, 0.25f, 1);
}

static void LongTextsAreCutBeforeTheLastCharacter() {
 for (size_t length : {511, 512, 513, 600}) {
  CHudVGUI2Print* print = new CHudVGUI2Print;
  const size_t capacity = sizeof(print->m_wCharBuf) / sizeof(print->m_wCharBuf[0]);
  std::wmemset(print->m_wCharBuf, L'x', capacity);
  Print(*print, "#Long", Text(length));
  assert(std::wmemchr(print->m_wCharBuf, 0, capacity));
  assert(Text(length < capacity ? length : capacity - 1) == print->m_wCharBuf);
  delete print;
 }
 puts("PASS: localized texts of 511, 512, 513 and 600 characters are cut to 511 and terminated inside the buffer");
}

static void ShorterTextsReplaceTheWholeOldText() {
 CHudVGUI2Print* print = new CHudVGUI2Print;
 Print(*print, "#Hello", L"Hello world");
 Print(*print, "#Bye", L"Bye");
 assert(std::wstring(L"Bye") == print->m_wCharBuf);
 assert(1 == print->m_iX && 2 == print->m_iY);
 assert(0.5f == print->m_fR && 0.25f == print->m_fG && 1 == print->m_fB);
 assert(14 == print->m_flVGUI2StringTime);
 delete print;
 puts("PASS: a shorter text replaces the whole previous text and the position, color and time are set");
}

static void UnknownTokensKeepTheOldText() {
 CHudVGUI2Print* print = new CHudVGUI2Print;
 Print(*print, "#Kept", L"kept");
 print->m_flVGUI2StringTime = 0;
 print->VGUI2HudPrint((char*)"#Missing", 5, 6, 0, 0, 0);
 assert(std::wstring(L"kept") == print->m_wCharBuf);
 assert(0 == print->m_flVGUI2StringTime && 1 == print->m_iX);
 delete print;
 puts("PASS: an unknown token leaves the text and the state alone");
}

int main() {
 LongTextsAreCutBeforeTheLastCharacter();
 ShorterTextsReplaceTheWholeOldText();
 UnknownTokensKeepTheOldText();
}
'''


def main():
    source = (ROOT / 'dod/cl_dll/hud_vgui2print.cpp').read_text(encoding='utf-8')
    header = (ROOT / 'dod/cl_dll/hud.h').read_text(encoding='utf-8')
    code = STUBS.replace('BUFFER', buffer_member(header))
    code += function(source, 'void CHudVGUI2Print::VGUI2HudPrint( char *text,') + '\n' + TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-dod-vgui2print-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
