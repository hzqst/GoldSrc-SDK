"""Exercise the production CCareerUITask constructor with localized task names longer than half of the name buffers.

Run: python tests/gameui_career_task_name_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


def members(header):
    start = header.index('private:', header.index('class CCareerUITask')) + len('private:')
    return header[start:header.index('};', start)]


STUBS = r'''
#include <cassert>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <map>
#include <string>
#include <vector>

static int stricmp(const char* left, const char* right) {
 for (; *left && tolower((unsigned char)*left) == tolower((unsigned char)*right); ++left, ++right) {}
 return tolower((unsigned char)*left) - tolower((unsigned char)*right);
}

// splits on spaces like the shared parser does for task names
static char token[256];
static bool SharedTokenWaiting(const char* buffer) {
 while (*buffer == ' ') ++buffer;
 return *buffer != 0;
}
static const char* SharedParse(const char* data) {
 while (*data == ' ') ++data;
 size_t length = 0;
 while (data[length] && data[length] != ' ') ++length;
 memcpy(token, data, length);
 token[length] = 0;
 return data + length;
}
static char* SharedGetToken() { return token; }

static bool UTIL_FormatString(char* buffer, size_t bufferSize, const char* format, ...) {
 va_list args;
 va_start(args, format);
 int written = vsnprintf(buffer, bufferSize, format, args);
 va_end(args);
 return written >= 0 && (size_t)written < bufferSize;
}

namespace vgui2 {
// ConstructString records the buffer size it is given and fills the buffer with the first parameter, cut to that size
struct Localize {
 wchar_t* Find(const char* name) {
  auto found = strings.find(name);
  return found == strings.end() ? nullptr : &found->second[0];
 }
 void ConstructString(wchar_t* output, int bytes, const wchar_t*, int count, ...) {
  va_list args;
  va_start(args, count);
  const wchar_t* parameter = va_arg(args, const wchar_t*);
  va_end(args);
  sizes.push_back(bytes);
  int capacity = bytes / (int)sizeof(wchar_t);
  wcsncpy(output, parameter, capacity - 1);
  output[capacity - 1] = 0;
 }
 std::map<std::string, std::wstring> strings;
 std::vector<int> sizes;
};
static Localize table;
Localize* localize() { return &table; }
}

void ConstructStringWithReordering(wchar_t* output, int bufferSizeInBytes, wchar_t* formatString, int numFormatParameters, ...);

// the members the constructor uses, with the production member declarations
class CCareerUITask {
public:
 CCareerUITask(const char* taskName);
 void Reset() {}
MEMBERS
};
'''

TESTS = r'''
static std::wstring Text(size_t length) {
 std::wstring text;
 for (size_t i = 0; i < length; ++i) text += (wchar_t)(L'A' + i % 26);
 return text;
}

static const size_t NameCapacity = 512;
static const size_t LongName = 400;

static void LongTaskNamesUseTheWholeBuffer() {
 vgui2::table.strings["#Career_Task_kill"] = Text(LongName);
 vgui2::table.strings["#Career_Task_kill_multi"] = Text(NameCapacity + 100);
 for (const char* name : {"kill ak47", "kill ak47 inarow", "kill ak47 survive"}) {
  vgui2::table.sizes.clear();
  CCareerUITask task(name);
  assert(Text(LongName) == task.m_localizedName);
  assert(Text(LongName) == task.m_localizedNameCompleted);
  for (int size : vgui2::table.sizes) assert((int)sizeof(task.m_localizedName) == size);
 }
 CCareerUITask task("3 kill ak47");
 assert(Text(NameCapacity - 1) == task.m_localizedName);
 puts("PASS: task names of 400 characters are kept whole, also in a row or surviving, and longer names are cut to 511");
}

int main() {
 LongTaskNamesUseTheWholeBuffer();
}
'''


def main():
    source = (ROOT / 'GameUI/CareerGameTasks.cpp').read_text(encoding='utf-8')
    header = (ROOT / 'GameUI/CareerGameTasks.h').read_text(encoding='utf-8')
    code = STUBS.replace('MEMBERS', members(header))
    code += function(source, 'CCareerUITask::CCareerUITask( const char* taskName )') + '\n'
    code += function(source, 'void ConstructStringWithReordering( wchar_t* output, int bufferSizeInBytes,') + '\n'
    code += TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-gameui-career-task-name-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
