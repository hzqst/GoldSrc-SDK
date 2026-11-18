"""Exercise the production GameUI key binding list reader with an in-memory file system.

Run: python tests/gameui_keyboard_list_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
"""
from pathlib import Path
import os
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
#include <map>
#include <string>
#include "utlbuffer.h"

typedef void* FileHandle_t;

struct MemoryFile { std::string bytes; int readLimit; };

namespace vgui2 {
struct IFileSystem {
 std::map<std::string, MemoryFile> files;
 FileHandle_t Open(const char* name, const char*, const char*) {
  auto it = files.find(name);
  return it == files.end() ? nullptr : &it->second;
 }
 unsigned int Size(FileHandle_t file) { return (unsigned int)((MemoryFile*)file)->bytes.size(); }
 int Read(void* output, int size, FileHandle_t file) {
  MemoryFile* memory = (MemoryFile*)file;
  // a negative limit makes the read fail
  if (memory->readLimit < 0) return -1;
  int count = size < memory->readLimit ? size : memory->readLimit;
  if (count > (int)memory->bytes.size()) count = (int)memory->bytes.size();
  memcpy(output, memory->bytes.data(), count);
  return count;
 }
 void Close(FileHandle_t) {}
};
static IFileSystem g_fileSystem;
IFileSystem* filesystem() { return &g_fileSystem; }
}
'''

TESTS = r'''
static const char* LIST = "gfx/shell/kb_def.lst";
static const int NO_LIMIT = 0x7fffffff;

static std::string Read(const std::string& bytes, int readLimit = NO_LIMIT) {
 vgui2::g_fileSystem.files = {{LIST, MemoryFile{bytes, readLimit}}};
 CUtlBuffer buffer(0, 0, true);
 assert(ReadKeyBindingList(LIST, buffer));
 // strlen stops at the terminator the reader wrote; ASan reports a read past the allocation
 return std::string((const char*)buffer.Base());
}

static void DataEndsAtTheEndOfTheFile() {
 // no trailing newline, so a parser that runs past the data reads heap memory
 assert(std::string("\"w\" \"+forward\"") == Read("\"w\" \"+forward\""));
 assert(std::string("") == Read(""));
 const std::string large(4096, 'k');
 assert(large == Read(large));
 puts("PASS: the list data is terminated right after the last byte of the file");
}

static void ShortReadEndsWhereTheReadStopped() {
 assert(std::string("\"w\"") == Read("\"w\" \"+forward\"", 3));
 assert(std::string("") == Read("\"w\"", -1));
 puts("PASS: a short or failed read terminates the data where the read stopped");
}

static void MissingListIsReported() {
 vgui2::g_fileSystem.files.clear();
 CUtlBuffer buffer(0, 0, true);
 assert(!ReadKeyBindingList(LIST, buffer));
 puts("PASS: a missing list is reported");
}
'''


def main():
    source = (ROOT / 'GameUI/OptionsSubKeyboard.cpp').read_text(encoding='utf-8')
    code = STUBS + function(source, 'static bool ReadKeyBindingList(') + TESTS
    code += '\nint main() { DataEndsAtTheEndOfTheFile(); ShortReadEndsWhereTheReadStopped(); MissingListIsReported(); }\n'
    with tempfile.TemporaryDirectory(prefix='goldsrc-gameui-keyboard-list-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([path / 'test.cpp', ROOT / 'engine/utlbuffer.cpp'], path / 'test',
                                     includes=[ROOT / 'engine', ROOT / 'public'], sanitizers=['address', 'undefined'],
                                     # the POSIX build gets this from common/posix_compat.h, which it force-includes
                                     defines=[] if os.name == 'nt' else ['_vsnprintf=vsnprintf'])
        subprocess.run([str(executable)], check=True, cwd=path)


if __name__ == '__main__':
    main()
