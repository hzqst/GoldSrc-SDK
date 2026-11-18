"""Exercise the production GameUI save header parser with an in-memory file system.

Run: python tests/gameui_save_read_tests.py
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


def defines(source, names):
    return ''.join(re.search(r'^#define\s+' + name + r'\b.*\n', source, re.M).group(0) for name in names)


STUBS = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <new>
#include <string>
#include <vector>
#ifdef _WIN32
#define stricmp _stricmp
#else
#include <strings.h>
#define stricmp strcasecmp
#endif

static int g_allocations = 0;
void* operator new(size_t size) { ++g_allocations; if (void* p = malloc(size ? size : 1)) return p; throw std::bad_alloc(); }
void* operator new[](size_t size) { return operator new(size); }
void operator delete(void* p) noexcept { if (p) { --g_allocations; free(p); } }
void operator delete[](void* p) noexcept { operator delete(p); }
void operator delete(void* p, size_t) noexcept { operator delete(p); }
void operator delete[](void* p, size_t) noexcept { operator delete(p); }

typedef void* FileHandle_t;

struct MemoryFile { std::string bytes; size_t position; };

namespace vgui2 {
struct IFileSystem {
 int Read(void* output, int size, FileHandle_t file) {
  MemoryFile* memory = (MemoryFile*)file;
  size_t count = memory->bytes.size() - memory->position;
  if ((size_t)size < count) count = size;
  memcpy(output, memory->bytes.data() + memory->position, count);
  memory->position += count;
  return (int)count;
 }
 unsigned int Size(FileHandle_t file) { return (unsigned int)((MemoryFile*)file)->bytes.size(); }
};
static IFileSystem g_fileSystem;
IFileSystem* filesystem() { return &g_fileSystem; }
}
'''

TESTS = r'''
static const int TOKEN_GAME_HEADER = 1;
static const int TOKEN_MAP_NAME = 3;
static const int TOKEN_COMMENT = 4;
static const int TOKEN_EMPTY = 2;
static const int TOKEN_COUNT = 6;

static void Int(std::string& out, int value) { out.append((const char*)&value, sizeof(value)); }
static void Short(std::string& out, int value) { short s = (short)value; out.append((const char*)&s, sizeof(s)); }
static void Field(std::string& out, int size, int token, const std::string& payload) { Short(out, size); Short(out, token); out += payload; }

// an engine-like token hash table: slots 0, 2 and 5 are empty
static std::string Tokens() { return std::string("\0GameHeader\0\0mapName\0comment\0\0", 30); }

struct Save {
 std::string tokens = Tokens();
 int tokenCount = TOKEN_COUNT;
 int gameHeaderToken = TOKEN_GAME_HEADER;
 int gameHeaderSize = 4;
 int mapNameToken = TOKEN_MAP_NAME;
 int commentToken = TOKEN_COMMENT;
 int sizeAdjust = 0;
 size_t truncate = 0;
 // appends fields after the three default ones and raises the GameHeader field count
 std::function<void(std::string&)> extraFields;
 static const size_t gameHeaderFieldCountOffset = 2 * sizeof(short);

 std::string Fields() const {
  std::string fields;
  std::string header(4, '\0');
  header[0] = 2;
  Field(fields, gameHeaderSize, gameHeaderToken, header);
  Field(fields, 6, mapNameToken, std::string("c1a0\0\0", 6));
  Field(fields, 12, commentToken, std::string("Hazard 01:23", 12));
  if (extraFields) {
   extraFields(fields);
   fields[gameHeaderFieldCountOffset] = 3;
  }
  return fields;
 }

 std::string Bytes() const {
  std::string fields = Fields();
  std::string out;
  Int(out, SAVEGAME_HEADER);
  Int(out, SAVEGAME_VERSION);
  Int(out, (int)((unsigned int)fields.size() + (unsigned int)sizeAdjust));
  Int(out, tokenCount);
  Int(out, (int)tokens.size());
  out += tokens + fields;
  out.resize(out.size() - truncate);
  return out;
 }
};

static int Parse(const std::string& bytes, std::string& name, std::string& comment) {
 int before = g_allocations;
 {
  MemoryFile file = { bytes, 0 };
  std::string scratchName, scratchComment;
  SaveReadNameAndComment(&file, scratchName, scratchComment);
 }
 // once the results are gone, every buffer the parser used is freed on every path
 assert(before == g_allocations);
 // results from an earlier parse must not leak into this one
 name = "stale map";
 comment = "stale comment";
 MemoryFile file = { bytes, 0 };
 return SaveReadNameAndComment(&file, name, comment);
}

static bool Rejects(const Save& save) {
 std::string name, comment;
 return Parse(save.Bytes(), name, comment) == 0;
}

static void ValidSaveIsParsed() {
 std::string name, comment;
 assert(1 == Parse(Save().Bytes(), name, comment));
 assert(std::string("c1a0") == name);
 // the comment field has no NUL, so the text runs to the end of the field
 assert(std::string("Hazard 01:23") == comment);
 puts("PASS: a save with empty token slots is parsed");
}

static void FieldTextEndsAtTheFirstNulOrTheFieldEnd() {
 std::string name, comment;
 Save save;
 save.extraFields = [](std::string& fields) {
  // a second, shorter comment replaces the first one entirely
  Field(fields, 9, TOKEN_COMMENT, std::string("Gate\0junk", 9));
 };
 assert(1 == Parse(save.Bytes(), name, comment));
 assert(std::string("Gate") == comment);
 const std::string longComment(300, 'c');
 Save longSave;
 longSave.extraFields = [&](std::string& fields) { Field(fields, (int)longComment.size(), TOKEN_COMMENT, longComment); };
 assert(1 == Parse(longSave.Bytes(), name, comment));
 assert(longComment == comment);
 Save emptyComment;
 emptyComment.extraFields = [](std::string& fields) { Field(fields, 3, TOKEN_COMMENT, std::string("\0ab", 3)); };
 assert(0 == Parse(emptyComment.Bytes(), name, comment));
 puts("PASS: field text ends at its first NUL or at the end of the field, without a length cap");
}

static void ElapsedTimeIsSplitFromTheComment() {
 struct Case { const char* input; const char* comment; const char* elapsed; };
 for (const Case& c : {Case{"Hazard 01:23", "Hazard", "01:23"}, Case{"Hazard    01:23", "Hazard", "01:23"},
   Case{"x01:23", "x", "01:23"}, Case{" 01:23", " ", "01:23"}, Case{"     01:23", " ", "01:23"},
   Case{"01:23", "01:23", "??"}, Case{"", "", "??"}, Case{"a b 12:34", "a b", "12:34"}}) {
  std::string comment = c.input, elapsed = "stale";
  SplitSaveComment(comment, elapsed);
  assert(std::string(c.comment) == comment);
  assert(std::string(c.elapsed) == elapsed);
 }
 puts("PASS: the elapsed time is split from the comment and the spaces before it are dropped");
}

static void MissingTokenTableIsRejected() {
 Save save;
 save.tokens.clear();
 save.tokenCount = 0;
 assert(Rejects(save));
 save.tokenCount = TOKEN_COUNT;
 assert(Rejects(save));
 puts("PASS: a save without a token table is rejected");
}

static void EmptyOrOutOfRangeTokensAreRejected() {
 for (int token : {TOKEN_EMPTY, -1, TOKEN_COUNT, 0x7fff}) {
  Save header;
  header.gameHeaderToken = token;
  assert(Rejects(header));
  Save field;
  field.commentToken = token;
  assert(Rejects(field));
 }
 Save wrongHeader;
 wrongHeader.gameHeaderToken = TOKEN_MAP_NAME;
 assert(Rejects(wrongHeader));
 puts("PASS: empty, out of range or wrong field tokens are rejected");
}

static void UnterminatedTokenIsRejected() {
 Save save;
 save.tokens = "GameHeader";
 save.tokenCount = 1;
 save.gameHeaderToken = 0;
 std::string name, comment;
 std::string bytes = save.Bytes();
 bytes.resize(SAVEGAME_HEADER_SIZE + save.tokens.size());
 int size = 0;
 memcpy(&bytes[2 * sizeof(int)], &size, sizeof(size));
 assert(0 == Parse(bytes, name, comment));
 puts("PASS: a token that runs to the end of the data is rejected");
}

static void BadFieldSizesAreRejected() {
 for (int size : {-1, -0x7fff, 0x7fff}) {
  Save save;
  save.gameHeaderSize = size;
  assert(Rejects(save));
 }
 puts("PASS: a GameHeader size outside the data is rejected");
}

static void ShortDataIsRejected() {
 // one GameHeader token and no field data: the table is built, then the field header is missing
 Save save;
 save.tokens = std::string("GameHeader\0", 11);
 save.tokenCount = 1;
 save.sizeAdjust = -(int)save.Fields().size();
 save.truncate = save.Fields().size();
 assert(Rejects(save));
 for (int adjust : {1, 0x7fffffff, -0x7fffffff}) {
  Save larger;
  larger.sizeAdjust = adjust;
  assert(Rejects(larger));
 }
 for (size_t truncate : {(size_t)1, Save().Bytes().size() - 3 * sizeof(int)}) {
  Save cut;
  cut.truncate = truncate;
  assert(Rejects(cut));
 }
 puts("PASS: sizes beyond the file and truncated files are rejected");
}
'''


def main():
    source = (ROOT / 'GameUI/SaveGameDialog.cpp').read_text(encoding='utf-8')
    code = STUBS + defines(source, ['SAVEGAME_HEADER', 'SAVEGAME_VERSION', 'MAX_SAVE_TOKENS',
                                    'SAVEGAME_ELAPSED_TIME_SIZE', 'SAVEGAME_HEADER_SIZE'])
    code += ''.join(function(source, signature) for signature in (
        'static bool SaveReadExact(', 'static const char* SaveTokenAt(', 'int SaveReadNameAndComment(',
        'void SplitSaveComment('))
    code += TESTS
    code += ('\nint main() { ValidSaveIsParsed(); FieldTextEndsAtTheFirstNulOrTheFieldEnd(); ElapsedTimeIsSplitFromTheComment(); MissingTokenTableIsRejected(); '
             'EmptyOrOutOfRangeTokensAreRejected(); UnterminatedTokenIsRejected(); '
             'BadFieldSizesAreRejected(); ShortDataIsRejected(); }\n')
    with tempfile.TemporaryDirectory(prefix='goldsrc-gameui-save-read-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([path / 'test.cpp'], path / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=path)


if __name__ == '__main__':
    main()
