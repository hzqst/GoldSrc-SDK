"""Exercise engine va() rotating 16 independent 1024-byte buffers with null termination."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK/engine'


def extract(filename, signature):
    source = (ROOT / filename).read_text(encoding='utf-8')
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifndef _WIN32
#define _vsnprintf vsnprintf
#endif
'''

TESTS = r'''
int main()
{
    // 16 consecutive calls with >16-byte strings must not overwrite one another.
    std::vector<char*> results;
    std::vector<std::string> expected;
    for (int i = 0; i < 16; ++i)
    {
        std::string payload = "slot_" + std::to_string(i) + "_" + std::string(40, static_cast<char>('a' + i));
        expected.push_back(payload);
        results.push_back(va("%s", payload.c_str()));
    }
    for (int i = 0; i < 16; ++i)
        assert(expected[i] == results[i]);

    // Overlong input (>= 1024 bytes) must truncate cleanly and be null-terminated.
    std::string huge(2048, 'Z');
    char* truncated = va("%s", huge.c_str());
    assert(1023 == std::strlen(truncated));
    assert('Z' == truncated[1022]);
    assert('\0' == truncated[1023]);
}
'''


def main():
    production = extract('common.cpp', 'char* va( const char* format, ... )')
    with tempfile.TemporaryDirectory(prefix='goldsrc-common-va-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
    print('PASS: va() keeps 16 independent 1024-byte buffers and null-terminates on truncation')


if __name__ == '__main__':
    main()
