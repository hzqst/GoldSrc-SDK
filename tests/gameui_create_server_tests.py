"""Exercise the production create-server setting copies and bot cvar commands with stubs.

Run: python tests/gameui_create_server_tests.py
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
    start = re.search(re.escape(signature) + r'[^\n;]*\n\{', source).start()
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#ifdef _WIN32
#define stricmp _stricmp
#else
#include <strings.h>
#define stricmp strcasecmp
#endif
#include "StringUtils.h"

struct Engine {
 static int ClientCmd(char* command) { commands.push_back(command); return 0; }
 int (*pfnClientCmd)(char*) = ClientCmd;
 static std::vector<std::string> commands;
};
std::vector<std::string> Engine::commands;
static Engine g_engine;
static Engine* engine = &g_engine;

// a flat KeyValues with string and int settings, like CSBotConfig.vdf
struct KeyValues {
 enum types_t { TYPE_STRING = 1, TYPE_INT = 2 };
 KeyValues(const char* name, const char* text) : name(name), text(text), type(TYPE_STRING) {}
 KeyValues(const char* name, int number) : name(name), number(number), type(TYPE_INT) {}
 KeyValues* GetFirstSubKey() { return children.empty() ? nullptr : &children[0]; }
 KeyValues* GetNextKey() { return next; }
 const char* GetName() { return name.c_str(); }
 types_t GetDataType() { return type; }
 const char* GetString() { return text.c_str(); }
 int GetInt() { return number; }
 void Link() { for (size_t i = 0; i + 1 < children.size(); i++) children[i].next = &children[i + 1]; }
 std::string name, text;
 int number = 0;
 types_t type;
 KeyValues* next = nullptr;
 std::vector<KeyValues> children;
};
'''

TESTS = r'''
// CCreateMultiplayerGameDialog::StartServer copies into char[64]
static const size_t SETTING_BUFFER_SIZE = 64;
static const bool MAP = false;
static const bool QUOTED = true;
static const char SENTINEL = 'x';

static bool Copy(const char* value, bool quoted, std::string* result) {
 char* buffer = new char[SETTING_BUFFER_SIZE];
 memset(buffer, SENTINEL, SETTING_BUFFER_SIZE);
 bool copied = CopyServerSetting(buffer, SETTING_BUFFER_SIZE, value, quoted);
 if (copied) *result = buffer;
 else for (size_t i = 0; i < SETTING_BUFFER_SIZE; i++) assert(SENTINEL == buffer[i]);
 delete[] buffer;
 return copied;
}

static void LongestFittingValueIsKept() {
 std::string result;
 std::string value(63, 'a');
 assert(Copy(value.c_str(), QUOTED, &result));
 assert(value == result);
 assert(Copy(value.c_str(), MAP, &result));
 assert(value == result);
 assert(Copy("", QUOTED, &result));
 assert(std::string() == result);
 puts("PASS: a 63 byte value and an empty value are copied with their terminator");
}

static void LongValuesAreRejected() {
 std::string result;
 for (size_t length : {64, 127}) {
  std::string value(length, 'a');
  assert(!Copy(value.c_str(), QUOTED, &result));
  assert(!Copy(value.c_str(), MAP, &result));
 }
 puts("PASS: 64 and 127 byte values are rejected instead of truncated");
}

static void Utf8ValuesAreCountedInBytes() {
 std::string result;
 std::string fits, overflows = "a";
 // 21 three-byte characters are 63 bytes
 for (int i = 0; i < 21; i++) fits += "\xE4\xB8\xAD";
 overflows += fits;
 assert(Copy(fits.c_str(), QUOTED, &result));
 assert(fits == result);
 assert(!Copy(overflows.c_str(), QUOTED, &result));
 puts("PASS: UTF-8 values are limited by bytes and never split");
}

static void MissingMapIsRejected() {
 std::string result;
 assert(!Copy(nullptr, MAP, &result));
 puts("PASS: a missing map is rejected");
}

static void CommandSeparatorsAreRejected() {
 std::string result;
 for (const char* value : {"a\"\nquit\n\"", "a\nquit", "a\rquit"}) {
  assert(!Copy(value, QUOTED, &result));
  assert(!Copy(value, MAP, &result));
 }
 assert(!Copy("crossfire;quit", MAP, &result));
 // a quoted value keeps its ';' because the command buffer only splits outside quotes
 assert(Copy("Bob's; server", QUOTED, &result));
 assert(std::string("Bob's; server") == result);
 puts("PASS: values that could split the console command are rejected");
}

static void BotCvarsThatCouldAddCommandsAreSkipped() {
 const std::string longPrefix(200, 'p');
 KeyValues data("CSBotConfig", "");
 data.children = {KeyValues("bot_difficulty", 2), KeyValues("map", "de_dust"), KeyValues("bot_prefix", "[CZ];bot"),
  KeyValues("bot_chatter", "x\";quit;\""), KeyValues("bot_join_team", "any\nquit"), KeyValues("sv_cheats 1;bot_quota", 5),
  KeyValues("bot_quota", -1), KeyValues("bot_prefix", longPrefix.c_str())};
 data.Link();
 Engine::commands.clear();
 SendBotCvars(&data);
 assert(4 == Engine::commands.size());
 assert(std::string("bot_difficulty 2\n") == Engine::commands[0]);
 // ';' inside the quotes cannot split the command
 assert(std::string("bot_prefix \"[CZ];bot\"\n") == Engine::commands[1]);
 assert(std::string("bot_quota -1\n") == Engine::commands[2]);
 // a long prefix is no longer dropped by a fixed buffer
 assert("bot_prefix \"" + longPrefix + "\"\n" == Engine::commands[3]);
 puts("PASS: bot cvars that could add commands are skipped and the others are sent whole");
}
'''


def main():
    source = (ROOT / 'GameUI/CreateMultiplayerGameDialog.cpp').read_text(encoding='utf-8')
    code = STUBS + function(source, 'static bool CopyServerSetting(') + function(source, 'static void SendBotCvars(') + TESTS
    code += ('\nint main() { LongestFittingValueIsKept(); LongValuesAreRejected(); Utf8ValuesAreCountedInBytes(); '
             'MissingMapIsRejected(); CommandSeparatorsAreRejected(); BotCvarsThatCouldAddCommandsAreSkipped(); }\n')
    with tempfile.TemporaryDirectory(prefix='goldsrc-gameui-create-server-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([path / 'test.cpp', ROOT / 'common/StringUtils.cpp'], path / 'test',
                                     includes=[ROOT / 'common'], sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=path)


if __name__ == '__main__':
    main()
