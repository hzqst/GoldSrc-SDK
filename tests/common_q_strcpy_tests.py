"""Exercise production Q_strcpy and Q_strcat with NULL and regular arguments."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK/engine'


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


TESTS = r'''
#include <cassert>
#include <cstring>
int main() {
 // A NULL destination is ignored instead of receiving the terminator
 Q_strcpy(nullptr,"text");Q_strcpy(nullptr,nullptr);
 Q_strcat(nullptr,"text");Q_strcat(nullptr,nullptr);

 char buffer[16];
 std::memset(buffer,'x',sizeof(buffer));
 Q_strcpy(buffer,"abc");assert(0==std::strcmp("abc",buffer));
 Q_strcat(buffer,"de");assert(0==std::strcmp("abcde",buffer));
 Q_strcat(buffer,"");assert(0==std::strcmp("abcde",buffer));

 // A NULL source copies an empty string
 Q_strcat(buffer,nullptr);assert(0==std::strcmp("abcde",buffer));
 Q_strcpy(buffer,nullptr);assert(0==std::strcmp("",buffer));
 assert('x'==buffer[sizeof(buffer)-1]);
}
'''


def main():
    source = (ROOT / 'common.cpp').read_text()
    code = '#include <cstddef>\n'
    code += function(source, 'void Q_strcpy( char* destination, const char* source )')
    code += function(source, 'void Q_strcat( char* destination, const char* source )') + TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-q-strcpy-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code)
        executable = compile_program([path / 'test.cpp'], path / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=path)
    print('PASS: NULL destinations are ignored, NULL sources copy an empty string')


if __name__ == '__main__':
    main()
