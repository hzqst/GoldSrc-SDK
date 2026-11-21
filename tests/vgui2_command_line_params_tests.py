"""Exercise the VGUI2 CommandLineArgs helpers and the production CSystem command line accessors of this platform on the test process's own command line, so a missing parameter or value reports failure and values with spaces stay whole.

Run: python tests/vgui2_command_line_params_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'
TIMEOUT_SECONDS = 60
# the arguments the test process is started with, after the executable
PROCESS_ARGS = ['-game', 'cstrike', '-name', 'a b', '-empty', '', '-flag']


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

#include "CommandLineArgs.h"

class CSystem {
public:
 bool CommandLineParamExists(const char* paramName);
 bool GetCommandLineParamValue(const char* paramName, char* value, int valueBufferSize);
 const char* GetFullCommandLine();
};
'''

TESTS = r'''

static const char SENTINEL[] = "sentinel";
static const int SMALL_BUFFER = 3;

static void Expect(bool condition, const char* message) {
 if (!condition) {
  printf("FAIL: %s\n", message);
  exit(1);
 }
}

static std::vector<std::string> Args(std::initializer_list<const char*> list) {
 return std::vector<std::string>(list.begin(), list.end());
}

static void SplitsWindowsCommandLine() {
 Expect(Args({"hl.exe", "-game", "cstrike"}) == SplitCommandLine("hl.exe  -game\tcstrike "), "spaces and tabs separate arguments");
 Expect(Args({"C:\\Half Life\\hl.exe", "-name", "a b", "-flag"}) == SplitCommandLine("\"C:\\Half Life\\hl.exe\" -name \"a b\" -flag"), "quotes keep spaces in one argument");
 Expect(Args({"hl.exe", "-empty", "", "-flag"}) == SplitCommandLine("hl.exe -empty \"\" -flag"), "empty quotes are an empty argument");
 Expect(Args({"-ab c"}) == SplitCommandLine("-a\"b c\""), "quotes inside an argument join it with what follows");
 Expect(SplitCommandLine("").empty() && SplitCommandLine("  ").empty(), "a blank command line has no arguments");
 puts("PASS: SplitCommandLine splits a Windows command line");
}

static void SplitsNulSeparatedArgs() {
 const char data[] = "hl_linux\0-name\0a b\0\0-flag";
 Expect(Args({"hl_linux", "-name", "a b", "", "-flag"}) == SplitNulSeparatedArgs(data, sizeof(data) - 1), "every NUL ends an argument and the last needs none");
 Expect(Args({"hl_linux", ""}) == SplitNulSeparatedArgs("hl_linux\0\0", 10), "an empty last argument is kept");
 Expect(SplitNulSeparatedArgs("", 0).empty(), "no data has no arguments");
 puts("PASS: SplitNulSeparatedArgs splits /proc/self/cmdline");
}

static void JoinsCommandLine() {
 Expect(std::string("hl_linux -name \"a b\" -empty \"\" -flag") == JoinCommandLine(Args({"hl_linux", "-name", "a b", "-empty", "", "-flag"})), "arguments with spaces and empty ones are quoted");
 Expect(std::string() == JoinCommandLine(std::vector<std::string>()), "no arguments join into an empty line");
 puts("PASS: JoinCommandLine quotes arguments with spaces");
}

static void FindsExactArgs() {
 std::vector<std::string> args = Args({"hl", "-game", "cstrike", "-flag"});
 Expect(CommandLineArgExists(args, "-game"), "a given parameter exists");
 Expect(!CommandLineArgExists(args, "-gam"), "a prefix of a parameter does not exist");
 Expect(!CommandLineArgExists(args, "strike"), "part of a value does not exist");
 Expect(!CommandLineArgExists(args, NULL), "a null name does not exist");
 puts("PASS: CommandLineArgExists matches whole arguments");
}

static void CopiesValues() {
 std::vector<std::string> args = Args({"hl", "-game", "cstrike", "-name", "a b", "-flag"});
 char value[64];
 Expect(CopyCommandLineArgValue(args, "-name", value, sizeof(value)) && std::string("a b") == value, "a value with spaces is copied whole");

 // not "small": windows.h defines it as char through rpcndr.h
 char cut[SMALL_BUFFER];
 Expect(CopyCommandLineArgValue(args, "-game", cut, sizeof(cut)) && std::string("cs") == cut, "a long value is cut and terminated");

 char one[1] = {'x'};
 Expect(CopyCommandLineArgValue(args, "-game", one, sizeof(one)) && 0 == one[0], "a one byte buffer gets only the terminator");

 struct { const char* name; char* buffer; int size; const char* message; } failures[] = {
  {"-missing", value, (int)sizeof(value), "a missing parameter fails"},
  {"-flag", value, (int)sizeof(value), "a parameter without a value fails"},
  {"-gam", value, (int)sizeof(value), "a prefix of a parameter fails"},
  {NULL, value, (int)sizeof(value), "a null name fails"},
  {"-game", value, 0, "an empty buffer fails"},
  {"-game", value, -1, "a negative buffer size fails"},
 };
 for (const auto& failure : failures) {
  strcpy(value, SENTINEL);
  Expect(!CopyCommandLineArgValue(args, failure.name, failure.buffer, failure.size), failure.message);
  Expect(std::string(SENTINEL) == value, "a failed lookup leaves the buffer untouched");
 }
 Expect(!CopyCommandLineArgValue(args, "-game", NULL, (int)sizeof(value)), "a null buffer fails");
 puts("PASS: CopyCommandLineArgValue copies the following argument or fails");
}

// the process was started as <exe> -game cstrike -name "a b" -empty "" -flag
static void ReadsProcessCommandLine(const char* exe) {
 CSystem system;
 Expect(system.CommandLineParamExists("-game") && system.CommandLineParamExists("-flag"), "given parameters exist");
 Expect(!system.CommandLineParamExists("-gam") && !system.CommandLineParamExists("-missing"), "a prefix or a missing parameter does not exist");

 char value[64];
 Expect(system.GetCommandLineParamValue("-game", value, sizeof(value)) && std::string("cstrike") == value, "the value of -game is read");
 Expect(system.GetCommandLineParamValue("-name", value, sizeof(value)) && std::string("a b") == value, "a value with spaces is read whole");
 strcpy(value, SENTINEL);
 Expect(system.GetCommandLineParamValue("-empty", value, sizeof(value)) && std::string() == value, "an empty value is read");
 strcpy(value, SENTINEL);
 Expect(!system.GetCommandLineParamValue("-missing", value, sizeof(value)) && std::string(SENTINEL) == value, "a missing parameter fails and keeps the buffer");
 Expect(!system.GetCommandLineParamValue("-flag", value, sizeof(value)) && std::string(SENTINEL) == value, "the last parameter has no value");

 std::string cmdLine = system.GetFullCommandLine();
 Expect(std::string::npos != cmdLine.find("-game cstrike -name \"a b\" -empty \"\" -flag"), "the full command line holds every argument");
#ifndef _WIN32
 Expect(0 == cmdLine.find(exe), "the full command line starts with the executable");
 Expect(ReadCommandLineArgs("/nonexistent/cmdline").empty(), "an unreadable file has no arguments");
#else
 (void)exe;
#endif
 puts("PASS: CSystem reads the parameters of the process command line");
}

int main(int, char** argv) {
 SplitsWindowsCommandLine();
 SplitsNulSeparatedArgs();
 JoinsCommandLine();
 FindsExactArgs();
 CopiesValues();
 ReadsProcessCommandLine(argv[0]);
 return 0;
}
'''


def main():
    if os.name == 'nt':
        source = (ROOT / 'vgui2/src/System.cpp').read_text(encoding='utf-8')
        functions = []
    else:
        source = (ROOT / 'vgui2/src/system_posix.cpp').read_text(encoding='utf-8')
        functions = ['static std::vector<std::string> ReadCommandLineArgs(']
    functions += [
        'static const std::vector<std::string>& GetCommandLineArgs()',
        'bool CSystem::CommandLineParamExists(',
        'bool CSystem::GetCommandLineParamValue(',
        'const char* CSystem::GetFullCommandLine()',
    ]
    code = STUBS + ''.join(function(source, signature) + '\n' for signature in functions) + TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-command-line-params-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', includes=[ROOT / 'vgui2/src'],
                                     sanitizers=['address', 'undefined'], strict_warnings=True,
                                     defines=['_CRT_SECURE_NO_WARNINGS'])
        subprocess.run([str(executable)] + PROCESS_ARGS, check=True, cwd=folder, timeout=TIMEOUT_SECONDS)


if __name__ == '__main__':
    main()
