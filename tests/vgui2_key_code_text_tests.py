"""Exercise the production VGUI2 key code text table shared by both input backends.

Run: python tests/vgui2_key_code_text_tests.py
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
TRANSLATION = 'vgui2/src/vgui_surfacelib/vgui_key_translation.cpp'
BACKENDS = ('vgui2/src/InputWin32.cpp', 'vgui2/src/inputosx.cpp')


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cstdio>
#include <cstring>

#include "vgui/KeyCode.h"

using namespace vgui2;

static const char* s_pKeyCodeText[KEY_LAST];
'''

BACKEND_STUBS = r'''
// only the member GetKeyCodeText needs
class CInputWin32 {
public:
 void GetKeyCodeText(KeyCode code, char* buf, int buflen);
};
'''

TESTS = r'''
static const char FILL = '#';

static void ExpectText(KeyCode code, int buflen, const char* expected) {
 char buf[32];
 memset(buf, FILL, sizeof(buf));
 CInputWin32 input;
 input.GetKeyCodeText(code, buf, buflen);
 assert(0 == strcmp(expected, buf));
 for (int i = buflen; i < (int)sizeof(buf); i++) assert(FILL == buf[i]);
}

static void PrintableKeysReturnTheirNames() {
 ExpectText(KEY_A, 32, "KEY_A");
 ExpectText(KEY_0, 32, "KEY_0");
 ExpectText(KEY_BACKSLASH, 32, "KEY_BACKSLASH");
 ExpectText(KEY_SPACE, 32, "KEY_SPACE");
 puts("PASS: printable keys return their full key names");
}

static void SpecialKeysReturnTheirNames() {
 ExpectText(KEY_PAD_ENTER, 32, "KEY_PAD_ENTER");
 ExpectText(KEY_ESCAPE, 32, "KEY_ESCAPE");
 ExpectText(KEY_F12, 32, "KEY_F12");
 puts("PASS: keys without a character return their full key names");
}

static void KeysWithoutANameReturnAnEmptyString() {
 ExpectText(KEY_NONE, 32, "");
 ExpectText(KEY_CAPSLOCKTOGGLE, 32, "");
 ExpectText(KEY_SCROLLLOCKTOGGLE, 32, "");
 ExpectText((KeyCode)-1, 32, "");
 ExpectText(KEY_LAST, 32, "");
 puts("PASS: key codes without a name or out of range return an empty string");
}

static void SmallBuffersAreTruncatedAndTerminated() {
 ExpectText(KEY_A, 1, "");
 ExpectText(KEY_A, 4, "KEY");
 ExpectText(KEY_A, 5, "KEY_");
 ExpectText(KEY_A, 6, "KEY_A");
 puts("PASS: small buffers are truncated and always NUL terminated");
}

static void InvalidBuffersAreLeftAlone() {
 char buf[4] = { FILL, FILL, FILL, FILL };
 CInputWin32 input;
 input.GetKeyCodeText(KEY_A, buf, 0);
 input.GetKeyCodeText(KEY_A, buf, -1);
 for (char ch : buf) assert(FILL == ch);
 input.GetKeyCodeText(KEY_A, nullptr, 32);
 puts("PASS: a null buffer or a non-positive length writes nothing");
}

int main() {
 PrintableKeysReturnTheirNames();
 SpecialKeysReturnTheirNames();
 KeysWithoutANameReturnAnEmptyString();
 SmallBuffersAreTruncatedAndTerminated();
 InvalidBuffersAreLeftAlone();
}
'''

TRANSLATION_SIGNATURES = (
    'static void InitKeyCodeTextTable()',
    'void KeyCode_GetText( KeyCode code, char* buf, int buflen )',
)


def main():
    translation = (ROOT / TRANSLATION).read_text(encoding='utf-8')
    table = ''.join(function(translation, signature) + '\n' for signature in TRANSLATION_SIGNATURES)
    for backend in BACKENDS:
        print(backend, flush=True)
        source = (ROOT / backend).read_text(encoding='utf-8')
        forward = function(source, 'void CInputWin32::GetKeyCodeText( KeyCode code, char* buf, int buflen )')
        code = STUBS + table + BACKEND_STUBS + forward + '\n' + TESTS
        with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-key-code-text-') as folder:
            folder = Path(folder)
            (folder / 'test.cpp').write_text(code, encoding='utf-8')
            executable = compile_program([folder / 'test.cpp'], folder / 'test',
                                         includes=[ROOT / 'public'], sanitizers=['address', 'undefined'])
            subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
