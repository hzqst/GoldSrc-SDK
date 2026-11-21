"""Check command-filter token boundaries and command privilege registration."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK/engine'


def extract(filename, signature, occurrence=0, end='\n}'):
    source = (ROOT / filename).read_text(encoding='utf-8')
    start = -1
    for _ in range(occurrence + 1):
        start = source.index(signature, start + 1)
    return source[start:source.index(end, start) + len(end)]


STUBS = r'''
#include <cassert>
#include <cctype>
#include <cstring>
using qboolean = bool;
static struct { bool demoplayback; } cls;
static struct { float value; } cl_filterstuffcmd;
static bool g_bIsTFC;
#ifndef _WIN32
#define _strnicmp strncasecmp
#include <strings.h>
#endif
static const char* Q_stristr(const char* text, const char* word)
{
    for (; text && *text; ++text)
        if (!_strnicmp(text, word, std::strlen(word))) return text;
    return nullptr;
}
static bool Host_IsSinglePlayerGame() { return false; }
static void Cmd_TokenizeString(const char*) {}
static const char* Cmd_Argv(int) { return "echo"; }
'''

TESTS = r'''
int main()
{
    assert(StuffTextHasCommand("unrestricted; restricted", "restricted"));
    assert(StuffTextHasCommand("echo;\t\"restricted\"", "restricted"));
    assert(!StuffTextHasCommand("unrestricted", "restricted"));
    assert(StuffTextHasArgumentCommand("echo; restricted\tvalue", "restricted"));
    assert(StuffTextHasArgumentCommand("echo; \"restricted\" value", "restricted"));
    assert(StuffTextHasArgumentCommand("echo; restricted:value", "restricted"));
    assert(!StuffTextHasArgumentCommand("echo; restricted\n", "restricted"));
    assert(ValidStuffText("echo item; reconnect"));
    assert(ValidStuffText("echo item"));
}
'''


def main():
    host_commands = (ROOT / 'host_cmd.cpp').read_text(encoding='utf-8')
    for name in ('quit', 'exit'):
        assert f'Cmd_AddCommandWithFlags( "{name}", Host_Quit_f, CMD_FLAG_PRIVILEGED );' in host_commands
    production = [
        extract("cl_parsefn.cpp", "static bool StuffTextIsSpace("),
        extract("cl_parsefn.cpp", "static bool StuffTextStartsCommand("),
        extract("cl_parsefn.cpp", "static bool StuffTextHasCommand("),
        extract("cl_parsefn.cpp", "static bool StuffTextHasArgumentCommand("),
        extract("cl_parsefn.cpp", "qboolean ValidStuffText("),
    ]
    with tempfile.TemporaryDirectory(prefix='goldsrc-engine-stufftext-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, *production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)


if __name__ == '__main__':
    main()
