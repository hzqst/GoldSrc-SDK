"""Exercise the production GameUI bounded string helpers and the liblist.gam parser with stubs.

Run: python tests/gameui_string_utils_tests.py
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
#include <clocale>
#include <cstdarg>
#include <ctype.h>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
struct KeyValues {
 std::map<std::string, std::string> values;
 void SetString(const char* key, const char* value) { values[key] = value; }
};
// whitespace-separated tokens, like COM_ParseFile for unquoted liblist.gam words
struct Engine {
 char* COM_ParseFile(char* data, char* token) {
  token[0] = 0;
  while (*data == ' ' || *data == '\n') data++;
  if (!*data) return nullptr;
  int length = 0;
  while (*data && *data != ' ' && *data != '\n') token[length++] = *data++;
  token[length] = 0;
  return data;
 }
} engineStub, *engine = &engineStub;
class CModInfo {
public:
 void LoadGameInfoFromBuffer(const char* buffer, int bufferSize);
 KeyValues data, *m_pModData = &data;
};
'''

TESTS = r'''
static void FormatFitsUpToTheTerminator() {
 char buffer[8];
 assert(UTIL_FormatString(buffer, sizeof(buffer), "%s", "1234567"));
 assert(std::string("1234567") == buffer);
 assert(UTIL_FormatString(buffer, sizeof(buffer), "%s %d", "ab", 12));
 assert(std::string("ab 12") == buffer);
 puts("PASS: formatted text that fits is kept");
}

static void FormatReportsTruncation() {
 char buffer[8];
 memset(buffer, 'x', sizeof(buffer));
 assert(!UTIL_FormatString(buffer, sizeof(buffer), "%s", "12345678"));
 // MSVC _vsnprintf leaves no terminator when the text fills the buffer
 assert(0 == buffer[sizeof(buffer) - 1]);
 assert(!UTIL_FormatString(buffer, sizeof(buffer), "%s", "a much longer text"));
 assert(0 == buffer[sizeof(buffer) - 1]);
 assert(!UTIL_FormatString(buffer, 0, "%s", ""));
 puts("PASS: truncated formatting fails and stays terminated");
}

static void CopyFitsUpToTheTerminator() {
 char buffer[8];
 assert(UTIL_CopyString(buffer, sizeof(buffer), "1234567"));
 assert(std::string("1234567") == buffer);
 assert(UTIL_CopyString(buffer, sizeof(buffer), ""));
 assert(std::string("") == buffer);
 puts("PASS: copies that fit are kept");
}

static void CopyRejectsTextThatDoesNotFit() {
 char buffer[8] = "keep";
 assert(!UTIL_CopyString(buffer, sizeof(buffer), "12345678"));
 assert(std::string("keep") == buffer);
 puts("PASS: copies that do not fit fail and leave the buffer unchanged");
}

static void UppercaseKeepsUTF8Bytes() {
#if defined(_WIN32)
 // vgui2 switches LC_CTYPE to the user locale; code page 1252 uppercases 0xE0-0xFE
 bool latin1 = setlocale(LC_CTYPE, ".1252") != nullptr;
 if (latin1) assert(0xC4 == toupper(0xE4));
#endif
 // "bot" followed by U+4E2D (E4 B8 AD) and U+00FF (C3 BF)
 char text[] = "bot_\xE4\xB8\xAD\xC3\xBFz";
 UTIL_UppercaseASCII(text);
 assert(std::string("BOT_\xE4\xB8\xAD\xC3\xBFZ") == text);
 char empty[] = "";
 UTIL_UppercaseASCII(empty);
 assert(0 == empty[0]);
 setlocale(LC_CTYPE, "C");
 puts("PASS: uppercasing only changes a-z and keeps UTF-8 bytes");
}

static void UnquotedArgumentsCannotAddCommands() {
 for (const char* text : {"", "c1a0", "media/Half-Life01.mp3", "a b", "\xE4\xB8\xAD"})
  assert(UTIL_IsUnquotedCommandArgument(text));
 for (const char* text : {";", "a;quit", "a\"b", "a\nquit", "a\rquit", "\""})
  assert(!UTIL_IsUnquotedCommandArgument(text));
 puts("PASS: text with a command separator, quote or line break is not an unquoted argument");
}

static void ModInfoSkipsPairsThatDoNotFit() {
 const std::string longToken(256, 'a'), longestToken(255, 'b');
 const std::string text = "game Half-Life\n" + longToken + " dropped\nstartmap " + longToken + "\n"
  + longestToken + " " + longestToken + "\ntrainmap t0a0\n";
 CModInfo info;
 info.LoadGameInfoFromBuffer(text.c_str(), (int)text.size());
 assert(3 == info.data.values.size());
 assert(std::string("Half-Life") == info.data.values["game"]);
 assert(longestToken == info.data.values[longestToken]);
 assert(std::string("t0a0") == info.data.values["trainmap"]);
 assert(0 == info.data.values.count("startmap"));
 puts("PASS: liblist.gam pairs longer than 255 bytes are skipped");
}
'''


def main():
    utils = (ROOT / 'GameUI/StringUtils.cpp').read_text(encoding='utf-8')
    modinfo = (ROOT / 'GameUI/ModInfo.cpp').read_text(encoding='utf-8')
    code = STUBS
    code += function(utils, 'bool UTIL_FormatString(') + function(utils, 'bool UTIL_CopyString(') + function(utils, 'void UTIL_UppercaseASCII(')
    code += function(utils, 'bool UTIL_IsUnquotedCommandArgument(')
    code += function(modinfo, 'void CModInfo::LoadGameInfoFromBuffer(')
    code += TESTS
    code += ('\nint main() { FormatFitsUpToTheTerminator(); FormatReportsTruncation(); CopyFitsUpToTheTerminator(); '
             'CopyRejectsTextThatDoesNotFit(); UppercaseKeepsUTF8Bytes(); UnquotedArgumentsCannotAddCommands(); ModInfoSkipsPairsThatDoNotFit(); }\n')
    with tempfile.TemporaryDirectory(prefix='goldsrc-gameui-string-utils-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([path / 'test.cpp'], path / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=path)


if __name__ == '__main__':
    main()
