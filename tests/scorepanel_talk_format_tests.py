"""Regression tests for ScorePanel FormatSingleStringToken and PrintLocalTalkMessage
across valve, dmc, ricochet, and tfc (ported from HLND2T PR #1694).

Run: python -I tests/scorepanel_talk_format_tests.py
"""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'

MODS = [
    ('valve', 'valve/cl_dll/vgui_ScorePanel.cpp'),
    ('dmc', 'dmc/cl_dll/vgui_ScorePanel.cpp'),
    ('ricochet', 'ricochet/cl_dll/vgui_ScorePanel.cpp'),
    ('tfc', 'tfc/cl_dll/vgui_ScorePanel.cpp'),
]


def extract_function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    for idx in range(brace, len(source)):
        ch = source[idx]
        if ch == '{':
            depth += 1
        elif ch == '}':
            depth -= 1
            if depth == 0:
                return source[start:idx + 1]
    raise RuntimeError(f'Unclosed function: {signature}')


HARNESS = r'''
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#define FALSE 0
#define TRUE 1

static bool g_allowedToPrint = true;
struct ViewportStub {
    int AllowedToPrintText() const { return g_allowedToPrint ? TRUE : FALSE; }
};
static ViewportStub g_viewportObj;
static ViewportStub* gViewPort = &g_viewportObj;

struct SayTextStub {
    std::vector<std::string> printed;
    std::vector<int> sizes;
    void SayTextPrint(const char* buf, int size) {
        printed.emplace_back(buf ? buf : "");
        sizes.push_back(size);
    }
};
static struct {
    SayTextStub m_SayText;
} gHUD;

__EXTRACTED__

int main() {
    // 1. Player name containing printf specifiers stays literal
    {
        char out[256] = {};
        FormatSingleStringToken(out, sizeof(out), "You have muted %s.", "Evil%s%n%x%p");
        assert(std::string(out) == "You have muted Evil%s%n%x%p.");
    }

    // 2. Template with %%, %d, and multiple %s expands only %% and the first %s
    {
        char out[256] = {};
        FormatSingleStringToken(out, sizeof(out), "100%% %s (%d) %s", "Alice");
        assert(std::string(out) == "100% Alice (%d) %s");
    }

    // 3. Truncation strictly respects buffer size and NUL-terminates
    {
        char out[10] = {};
        FormatSingleStringToken(out, sizeof(out), "Muted %s!", "VeryLongPlayerName");
        assert(std::string(out) == "Muted Ver");
        assert(out[9] == '\0');
    }

    // 4. Null/empty edge cases do not crash
    {
        FormatSingleStringToken(nullptr, 10, "%s", "x");
        char out[4] = "abc";
        FormatSingleStringToken(out, 0, "%s", "x");
        assert(out[0] == 'a');
        FormatSingleStringToken(out, sizeof(out), nullptr, nullptr);
        assert(out[0] == '\0');
    }

    // 5. PrintLocalTalkMessage converts CR to LF, outputs literal %specifiers, and respects AllowedToPrintText
    {
        gHUD.m_SayText.printed.clear();
        gHUD.m_SayText.sizes.clear();
        g_allowedToPrint = true;
        char msg[] = "** Muted Evil%n%s\r\n";
        PrintLocalTalkMessage(msg);
        assert(gHUD.m_SayText.printed.size() == 1);
        assert(gHUD.m_SayText.printed[0] == "** Muted Evil%n%s\n\n");
        assert(gHUD.m_SayText.sizes[0] == (int)strlen(msg) + 1);

        g_allowedToPrint = false;
        char msg2[] = "** Suppressed\n";
        PrintLocalTalkMessage(msg2);
        assert(gHUD.m_SayText.printed.size() == 1);
    }

    return 0;
}
'''


def main():
    for mod, rel_path in MODS:
        source = (ROOT / rel_path).read_text(encoding='latin-1')
        extracted = '\n\n'.join([
            extract_function(source, 'static void FormatSingleStringToken('),
            extract_function(source, 'static void PrintLocalTalkMessage('),
        ])
        code = HARNESS.replace('__EXTRACTED__', extracted)
        with tempfile.TemporaryDirectory(prefix=f'goldsrc-scorepanel-talk-{mod}-') as folder:
            folder = Path(folder)
            test_cpp = folder / 'test.cpp'
            test_cpp.write_text(code, encoding='utf-8')
            exe = compile_program([test_cpp], folder / 'test', sanitizers=['address', 'undefined'])
            subprocess.run([str(exe)], check=True, cwd=folder)
        print(f'PASS: {mod} ScorePanel FormatSingleStringToken & PrintLocalTalkMessage')


if __name__ == '__main__':
    main()
