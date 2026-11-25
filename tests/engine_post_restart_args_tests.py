"""Exercise production Host_SetGameDir_f and Host_AppendPostRestartArgs against the
launcher's restart argument buffer, followed by guard bytes."""
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
    start = source.index(signature + '\n{')
    return source[start:source.index('\n}', start) + 2]


PRELUDE = r'''
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
'''

STUBS = r'''
static const int GUARD_BYTES = 64;
static const char GUARD = '\x7f';
static struct { char args[POST_RESTART_COMMAND_LENGTH]; char guard[GUARD_BYTES]; } launcher;
static char* g_pPostRestartCmdLineArgs;
static std::vector<std::string> argv;
static int printed;
static int Cmd_Argc() { return static_cast<int>(argv.size()); }
static const char* Cmd_Argv(int arg) { return arg < Cmd_Argc() ? argv[arg].c_str() : ""; }
static void Con_Printf(const char*, ...) { ++printed; }
static const char* Q_strstr(const char* text, const char* search) { return std::strstr(text, search); }
'''

TESTS = r'''
#include <cassert>
static void Reset() {
 std::memset(&launcher, 0, sizeof(launcher));
 std::memset(launcher.guard, GUARD, sizeof(launcher.guard));
 g_pPostRestartCmdLineArgs = launcher.args; printed = 0;
}
static bool GuardIntact() {
 for (char byte : launcher.guard) if (byte != GUARD) return false;
 return true;
}
static void SetGameDir(std::vector<std::string> arguments) { argv = arguments; Host_SetGameDir_f(); }
int main() {
 // _setgamedir appends -game with the directory; without a directory nothing is appended
 Reset();
 SetGameDir({"_setgamedir", "cstrike"});
 assert(std::string(" -game cstrike") == launcher.args);
 SetGameDir({"_setgamedir"});
 assert(std::string(" -game cstrike") == launcher.args);

 // Repeated long directories stop at the buffer instead of writing past it
 Reset();
 const std::string longDir(POST_RESTART_COMMAND_LENGTH / 3, 'd');
 for (int i = 0; i < 4; ++i) SetGameDir({"_setgamedir", longDir});
 assert(GuardIntact());
 assert(" -game " + longDir + " -game " + longDir == launcher.args);
 assert(2 == printed);

 // An argument that fills the buffer up to its terminator still fits; one more byte does not
 Reset();
 const std::string fillDir(POST_RESTART_COMMAND_LENGTH - 1 - std::strlen(" -game "), 'f');
 SetGameDir({"_setgamedir", fillDir});
 assert(" -game " + fillDir == launcher.args);
 assert(!Host_AppendPostRestartArgs(" +load quick", ""));
 assert(" -game " + fillDir == launcher.args);
 assert(GuardIntact());

 // Directories outside the base directory are still refused
 Reset();
 for (const char* dir : {"/valve", "c:valve", "../valve", "a\\b"}) SetGameDir({"_setgamedir", dir});
 assert(std::string("") == launcher.args);

 // Without a launcher buffer nothing is written
 Reset(); g_pPostRestartCmdLineArgs = nullptr;
 assert(!Host_AppendPostRestartArgs(" +load quick", ""));
 SetGameDir({"_setgamedir", "cstrike"});
 assert(std::string("") == launcher.args);
}
'''


def main():
    source = (ROOT / 'engine/host_cmd.cpp').read_text()
    launcher = (ROOT / 'launcher/launcher.cpp').read_text()
    size = re.search(r'^#define POST_RESTART_COMMAND_LENGTH (\d+)$', source, re.M)
    launcher_size = re.search(r'^#define NEW_COMMAND_LENGTH\s+(\d+)$', launcher, re.M)
    assert size and launcher_size and size[1] == launcher_size[1], (size, launcher_size)
    code = PRELUDE + size[0] + '\n' + STUBS
    code += function(source, 'static bool Host_AppendPostRestartArgs( const char* args, const char* value )')
    code += function(source, 'void Host_SetGameDir_f()') + TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-post-restart-args-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code)
        executable = compile_program([path / 'test.cpp'], path / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=path)
    print('PASS: restart arguments stay inside the launcher buffer and match its size')


if __name__ == '__main__':
    main()
