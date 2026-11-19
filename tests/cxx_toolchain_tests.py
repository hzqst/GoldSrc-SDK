"""Check that cxx_toolchain.py builds sources with the same name, such as
win32/Font.cpp and Font.cpp, into one program, and splits them into object
batches without two sources of the same name.

Run: python tests/cxx_toolchain_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program, object_batches

MAIN = '''int Value();
int OtherValue();
// 0 only when both sources of the same name were linked.
int main() { return Value() * 10 + OtherValue() == 12 ? 0 : 1; }
'''


def batches_split_sources_of_the_same_name():
    batches = object_batches(['a/Font.cpp', 'b/font.cpp', 'c/Panel.cpp', 'd/FONT.cpp'])
    assert [['a/Font.cpp', 'c/Panel.cpp'], ['b/font.cpp'], ['d/FONT.cpp']] == batches, batches
    assert [['a/Font.cpp', 'c/Panel.cpp']] == object_batches(['a/Font.cpp', 'c/Panel.cpp'])
    print('PASS: sources of the same name, in any case, go to separate object batches in order')


def sources_of_the_same_name_link_together():
    with tempfile.TemporaryDirectory(prefix='goldsrc-cxx-toolchain-') as folder:
        folder = Path(folder)
        for name, text in (('main.cpp', MAIN), ('win32/Font.cpp', 'int Value() { return 1; }\n'),
                           ('font.cpp', 'int OtherValue() { return 2; }\n')):
            (folder / name).parent.mkdir(parents=True, exist_ok=True)
            (folder / name).write_text(text)
        executable = compile_program([folder / 'main.cpp', folder / 'win32/Font.cpp', folder / 'font.cpp'],
                                     folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)
    print('PASS: win32/Font.cpp and font.cpp are both linked into the program')


if __name__ == '__main__':
    batches_split_sources_of_the_same_name()
    sources_of_the_same_name_link_together()
