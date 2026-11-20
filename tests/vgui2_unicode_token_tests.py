"""Exercise the production ReadUnicodeToken with tokens that fill or overflow the buffer."""
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
    source = (ROOT / 'GoldSrc-SDK/vgui2/src/public/UnicodeFileHelpers.cpp').read_text(encoding='utf-8')
    code = r'''
#include <cassert>
#include <cwctype>
#include <vector>
typedef unsigned short ucs2;
'''
    for name in ('ucs2* AdvanceOverWhitespace(', 'ucs2* ReadUnicodeToken('):
        code += function(source, name) + '\n'
    code += r'''
static const int tokenSize = 4;
static const ucs2 guard = 0xABCD;

static std::vector<ucs2> text(const char* ascii) {
 std::vector<ucs2> out;
 for (; *ascii; ++ascii) out.push_back((ucs2)*ascii);
 out.push_back(0);
 return out;
}

// reads one token into a buffer followed by a guard unit and checks the token and what is left
static void check(const char* input, bool expectedQuoted, const char* expectedToken, const char* expectedRest) {
 std::vector<ucs2> data = text(input);
 ucs2 token[tokenSize + 1];
 for (ucs2& unit : token) unit = guard;
 bool quoted = !expectedQuoted;
 ucs2* rest = ReadUnicodeToken(data.data(), token, tokenSize, quoted);
 assert(guard == token[tokenSize]);
 assert(expectedQuoted == quoted);
 assert(text(expectedToken) == std::vector<ucs2>(token, token + text(expectedToken).size()));
 assert(text(expectedRest) == std::vector<ucs2>(rest, rest + text(expectedRest).size()));
}

int main() {
 // short tokens are copied whole
 check("abc def", false, "abc", " def");
 check("\"abc\" def", true, "abc", " def");
 check("  \"a\\\"b\"", true, "a\"b", "");
 // a token that fills the buffer is cut so the terminator stays inside it
 check("abcd efg", false, "abc", "d efg");
 check("\"abcd\" efg", true, "abc", "d\" efg");
 check("\"abcdefgh\"", true, "abc", "defgh\"");
 check("", false, "", "");
}
'''
    with tempfile.TemporaryDirectory(prefix='goldsrc-unicode-token-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([path / 'test.cpp'], path / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True)
    print('PASS: ReadUnicodeToken keeps the terminator inside the buffer for tokens that fill or overflow it')


if __name__ == '__main__':
    main()
