"""Exercise the production KeyValues and CUtlBuffer with an in-memory file system and a counting allocator.

Run: python tests/keyvalues_tests.py
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
HANDWRITTEN = {'Open', 'Close', 'Size', 'Read', 'Write'}


def file_system_stubs(header):
    """Default every IFileSystem method the tests do not implement, so the stub follows the interface."""
    stubs = []
    for declaration in re.findall(r'virtual\s+([^;]*?)\s*=\s*0\s*;', header):
        name = re.search(r'(\w+)\s*\(', declaration)
        if name.group(1) not in HANDWRITTEN:
            return_type = declaration[:name.start()].strip()
            stubs.append(f' {declaration} override {{ return Default<{return_type}>(); }}\n')
    return ''.join(stubs)


STUBS = r'''
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <map>
#include <string>
#include <vector>
#include "KeyValues.h"
#include "IKeyValues.h"
#include "FileSystem.h"
#include "UtlBuffer.h"

template <class T> static T Default() { return T(); }

// pool allocations are counted, so a test can see every value and node come back
class CountingKeyValues : public IKeyValues {
public:
 int live = 0;
 std::vector<std::string> symbols;
 void RegisterSizeofKeyValues(int) override {}
 void* AllocKeyValuesMemory(int size) override { ++live; return std::malloc(size); }
 void FreeKeyValuesMemory(void* memory) override { if (memory) { --live; std::free(memory); } }
 HKeySymbol GetSymbolForString(const char* name) override {
  for (size_t i = 0; i < symbols.size(); ++i)
   if (symbols[i] == name) return (HKeySymbol)i;
  symbols.push_back(name);
  return (HKeySymbol)(symbols.size() - 1);
 }
 const char* GetStringForSymbol(HKeySymbol symbol) override { return symbols[symbol].c_str(); }
 void GetLocalizedFromANSI(const char* ansi, wchar_t* localized, int bytes) override {
  int count = bytes / (int)sizeof(wchar_t), i = 0;
  for (; i < count - 1 && ansi[i]; ++i) localized[i] = (unsigned char)ansi[i];
  localized[i] = 0;
 }
 void GetANSIFromLocalized(const wchar_t* localized, char* ansi, int bytes) override {
  int i = 0;
  for (; i < bytes - 1 && localized[i]; ++i) ansi[i] = (char)localized[i];
  ansi[i] = 0;
 }
 void AddKeyValuesToMemoryLeakList(void*, HKeySymbol) override {}
 void RemoveKeyValuesFromMemoryLeakList(void*) override {}
};

static CountingKeyValues g_KeyValues;
IKeyValues* keyvalues() { return &g_KeyValues; }

struct MemoryFile { std::string name; std::string bytes; size_t position = 0; };

class MemoryFileSystem : public IFileSystem {
public:
 std::map<std::string, std::string> files;
 FileHandle_t Open(const char* name, const char* options, const char*) override {
  bool write = std::strchr(options, 'w') != nullptr;
  if (!write && !files.count(name)) return nullptr;
  MemoryFile* file = new MemoryFile;
  file->name = name;
  if (!write) file->bytes = files[name];
  return file;
 }
 void Close(FileHandle_t handle) override { delete (MemoryFile*)handle; }
 unsigned int Size(FileHandle_t handle) override { return (unsigned int)((MemoryFile*)handle)->bytes.size(); }
 unsigned int Size(const char* name) override { return (unsigned int)files[name].size(); }
 int Read(void* output, int size, FileHandle_t handle) override {
  MemoryFile* file = (MemoryFile*)handle;
  size_t count = std::min((size_t)size, file->bytes.size() - file->position);
  std::memcpy(output, file->bytes.data() + file->position, count);
  file->position += count;
  return (int)count;
 }
 int Write(void const* input, int size, FileHandle_t handle) override {
  MemoryFile* file = (MemoryFile*)handle;
  file->bytes.append((const char*)input, size);
  files[file->name] = file->bytes;
  return size;
 }
'''

TESTS = r'''
};

static KeyValues* Load(const char* text) {
 KeyValues* data = new KeyValues("");
 data->LoadFromBuffer(text);
 return data;
}

static void ParsesBlocksCommentsAndEscapes() {
 KeyValues* data = Load(
  "// leading comment\n"
  "\"root\"\n{\n"
  " name \"quoted \\\"value\\\" \\\\ \\n\\t\" // trailing comment\n"
  " unquoted value\n"
  " \"sub\" { \"x\" \"1\" \"y\" \"2\" }\n"
  " after \"present\"\n"
  "}\n");
 assert(std::strcmp("root", data->GetName()) == 0);
 assert(std::strcmp("quoted \"value\" \\ \n\t", data->GetString("name")) == 0);
 assert(std::strcmp("value", data->GetString("unquoted")) == 0);
 assert(2 == data->GetInt("sub/y"));
 assert(std::strcmp("present", data->GetString("after")) == 0);
 KeyValues* sub = data->FindKey("sub");
 assert(sub && std::strcmp("x", sub->GetFirstSubKey()->GetName()) == 0);
 assert(std::strcmp("y", sub->GetFirstSubKey()->GetNextKey()->GetName()) == 0);
 data->deleteThis();
 assert(0 == g_KeyValues.live);
 puts("PASS: LoadFromBuffer reads nested blocks, comments, quoted and unquoted tokens and escapes");
}

static void OverlongTokensAreTruncated() {
 std::string value(600, 'v');
 std::string text = "root { key \"" + value + "\" next \"present\" }";
 KeyValues* data = Load(text.c_str());
 assert(std::string(511, 'v') == data->GetString("key"));
 assert(std::strcmp("present", data->GetString("next")) == 0);
 data->deleteThis();
 puts("PASS: a token longer than the token buffer is cut to 511 characters and parsing goes on after it");
}

static void FindsAndCreatesKeyPaths() {
 KeyValues* data = new KeyValues("root");
 assert(nullptr == data->FindKey("a/b/c"));
 KeyValues* leaf = data->FindKey("a/b/c", true);
 assert(leaf && leaf == data->FindKey("a")->FindKey("b")->FindKey("c"));
 assert(data == data->FindKey(nullptr) && data == data->FindKey(""));
 std::string longSegment(300, 's');
 data->FindKey((longSegment + "/leaf").c_str(), true);
 assert(data->FindKey(std::string(256, 's').c_str()));
 data->deleteThis();
 assert(0 == g_KeyValues.live);
 puts("PASS: FindKey walks and creates '/' paths and truncates a path segment to 256 characters");
}

static void ConvertsBetweenTypes() {
 KeyValues* data = new KeyValues("root");
 data->SetString("text", "42");
 assert(42 == data->GetInt("text") && 42.0f == data->GetFloat("text"));
 data->SetInt("number", 7);
 assert(std::strcmp("7", data->GetString("number")) == 0);
 assert(KeyValues::TYPE_STRING == data->GetDataType("number"));
 data->SetFloat("real", 1.5f);
 assert(1 == data->GetInt("real") && std::strcmp("1.500000", data->GetString("real")) == 0);
 data->SetUint64("wide", 1ull << 40);
 assert((1ull << 40) == data->GetUint64("wide") && std::strcmp("1099511627776", data->GetString("wide")) == 0);
 data->SetWString("unicode", L"abc");
 assert(std::wcscmp(L"abc", data->GetWString("unicode")) == 0 && std::strcmp("abc", data->GetString("unicode")) == 0);
 data->SetColor("color", Color(1, 2, 3, 4));
 Color color = data->GetColor("color");
 assert(1 == color.r() && 2 == color.g() && 3 == color.b() && 4 == color.a());
 data->SetString("parsed", "10 20 30 40");
 assert(Color(10, 20, 30, 40) == data->GetColor("parsed"));
 assert(-5 == data->GetInt("missing", -5) && std::strcmp("fallback", data->GetString("missing", "fallback")) == 0);
 assert(data->IsEmpty("missing") && !data->IsEmpty("text"));
 data->deleteThis();
 assert(0 == g_KeyValues.live);
 puts("PASS: values convert between string, int, float, uint64, wide string and color");
}

static void CopiesAndNumbersKeys() {
 KeyValues* data = Load("root { a \"1\" sub { b \"2\" } }");
 data->SetUint64("wide", 5);
 KeyValues* copy = data->MakeCopy();
 data->deleteThis();
 assert(1 == copy->GetInt("a") && 2 == copy->GetInt("sub/b") && 5 == copy->GetUint64("wide"));
 KeyValues* list = new KeyValues("list");
 list->FindKey("3", true);
 assert(std::strcmp("4", list->CreateNewKey()->GetName()) == 0);
 list->deleteThis();
 copy->deleteThis();
 assert(0 == g_KeyValues.live);
 puts("PASS: MakeCopy copies values and subkeys deeply and CreateNewKey takes the next number");
}

static void SavesAndLoadsFiles() {
 MemoryFileSystem files;
 KeyValues* data = new KeyValues("root");
 data->SetString("name", "a\"b\\c");
 data->SetInt("number", 5);
 data->SetFloat("real", 1.5f);
 data->SetString("sub/x", "y");
 data->SetString("empty", "");
 assert(data->SaveToFile(&files, "test.txt"));
 const char expected[] = "\"root\"\n{\n"
  "\t\"name\"\t\t\"a\\\"b\\\\c\"\n"
  "\t\"number\"\t\t\"5\"\n"
  "\t\"real\"\t\t\"1.500000\"\n"
  "\t\"sub\"\n\t{\n\t\t\"x\"\t\t\"y\"\n\t}\n"
  "}\n";
 assert(std::string(expected, sizeof(expected)) == files.files["test.txt"]);
 data->deleteThis();

 KeyValues* loaded = new KeyValues("");
 assert(loaded->LoadFromFile(&files, "test.txt"));
 assert(std::strcmp("root", loaded->GetName()) == 0);
 assert(std::strcmp("a\"b\\c", loaded->GetString("name")) == 0 && 5 == loaded->GetInt("number"));
 assert(std::strcmp("y", loaded->GetString("sub/x")) == 0 && !loaded->FindKey("empty"));
 assert(!loaded->LoadFromFile(&files, "missing.txt"));
 loaded->deleteThis();
 assert(0 == g_KeyValues.live);
 puts("PASS: SaveToFile writes quoted, escaped text that LoadFromFile reads back");
}

static void RepeatedKeysFreeTheOldValue() {
 KeyValues* data = Load("root { a \"1\" a \"22\" a \"333\" }");
 assert(std::strcmp("333", data->GetString("a")) == 0);
 assert(data->GetFirstSubKey() && !data->GetFirstSubKey()->GetNextKey());
 data->deleteThis();
 assert(0 == g_KeyValues.live);

 // the block node's own value is not the one a string value replaces; "a" already
 // exists, so loading it does not reset the block node's type
 KeyValues* block = new KeyValues("");
 block->SetString("a", "0");
 block->SetString(nullptr, "own");
 block->LoadFromBuffer("root { a \"1\" }");
 assert(std::strcmp("own", block->GetString()) == 0 && 1 == block->GetInt("a"));
 block->deleteThis();
 assert(0 == g_KeyValues.live);
 puts("PASS: a repeated key frees the value it replaces and the block node keeps its own value");
}

static void QuotedBracesAreValues() {
 MemoryFileSystem files;
 KeyValues* data = new KeyValues("root");
 data->SetString("close", "}");
 data->SetString("open", "{");
 data->SetString("next", "present");
 assert(data->SaveToFile(&files, "braces.txt"));
 data->deleteThis();

 KeyValues* loaded = new KeyValues("");
 assert(loaded->LoadFromFile(&files, "braces.txt"));
 assert(std::strcmp("}", loaded->GetString("close")) == 0 && std::strcmp("{", loaded->GetString("open")) == 0);
 assert(std::strcmp("present", loaded->GetString("next")) == 0 && !loaded->FindKey("open")->GetFirstSubKey());
 loaded->deleteThis();

 KeyValues* names = Load("root { \"}\" \"close\" \"{\" \"open\" }");
 assert(std::strcmp("close", names->GetString("}")) == 0 && std::strcmp("open", names->GetString("{")) == 0);
 names->deleteThis();

 // only an unquoted { opens the file's top block
 KeyValues* header = Load("root \"{\" a \"1\" }");
 assert(!header->FindKey("a"));
 header->deleteThis();
 assert(0 == g_KeyValues.live);
 puts("PASS: quoted { and } are names and values, and only unquoted braces open and close blocks");
}

// the copy ends at its terminator, so AddressSanitizer reports any read past it
static KeyValues* LoadExact(const char* text) {
 size_t size = std::strlen(text) + 1;
 char* copy = new char[size];
 std::memcpy(copy, text, size);
 KeyValues* data = Load(copy);
 delete[] copy;
 return data;
}

static void CompleteInputStaysInTheBuffer() {
 KeyValues* data = LoadExact("root { a \"1\" }");
 assert(1 == data->GetInt("a"));
 data->deleteThis();
 puts("PASS: a complete buffer is read up to its closing brace");
}

static void UnterminatedQuoteStaysInTheBuffer() {
 KeyValues* data = LoadExact("root { a \"1\" b \"open");
 assert(1 == data->GetInt("a"));
 data->deleteThis();
 puts("PASS: an unterminated quoted token stops at the end of the buffer");
}

static void TrailingCommentStaysInTheBuffer() {
 KeyValues* data = LoadExact("root { a \"1\" // comment");
 assert(1 == data->GetInt("a"));
 data->deleteThis();
 puts("PASS: a comment without a newline stops at the end of the buffer");
}

static void MissingCloseStaysInTheBuffer() {
 KeyValues* data = LoadExact("root { a \"1\" ");
 assert(1 == data->GetInt("a"));
 data->deleteThis();
 puts("PASS: a block without its closing brace stops at the end of the buffer");
}

static void EscapeAtTheEndStaysInTheBuffer() {
 KeyValues* data = LoadExact("root { a \"1\" b \"x\\");
 assert(1 == data->GetInt("a"));
 data->deleteThis();
 puts("PASS: a backslash before the end of the buffer stops there");
}

static void SlashAtTheEndStaysInTheBuffer() {
 KeyValues* data = LoadExact("root { a \"1\" /");
 assert(1 == data->GetInt("a"));
 data->deleteThis();
 puts("PASS: a slash before the end of the buffer stops there");
}

static void BufferWritesNumbersAndText() {
 CUtlBuffer text(0, 0, true);
 text.PutInt(-12);
 text.PutChar(' ');
 text.PutUnsignedShort(65535);
 text.PutString(" end");
 assert(std::string("-12 65535 end") == std::string((const char*)text.Base(), text.TellPut()));
 CUtlBuffer binary(0, 0, false);
 binary.PutInt(0x01020304);
 binary.PutUnsignedShort(0x0506);
 binary.Printf("%s", "ab");
 assert(4 + 2 + 3 == binary.TellPut() && 0x01020304 == binary.GetInt());
 char fixed[4];
 CUtlBuffer external(fixed, sizeof(fixed), false);
 external.PutInt(1);
 external.PutChar('x');
 assert(!external.IsValid() && 4 == external.TellPut());
 puts("PASS: CUtlBuffer writes text and binary values and stops at the end of an external buffer");
}

// sv_remoteaccess.cpp reads RCON request strings into fixed stack buffers with it
static void BufferReadsBoundedStrings() {
 const int capacity = 4;
 char out[capacity + 1];
 const char textSource[] = "  hello world";
 std::vector<char> text(textSource, textSource + sizeof(textSource) - 1);
 CUtlBuffer textBuffer(text.data(), (int)text.size(), true);
 memset(out, '#', sizeof(out));
 textBuffer.GetString(out, capacity);
 assert(std::string("hel") == out && '#' == out[capacity]);
 textBuffer.GetString(out, capacity);
 assert(std::string("wor") == out && (int)text.size() == textBuffer.TellGet());
 CUtlBuffer exactBuffer(text.data(), (int)text.size(), true);
 char exact[sizeof("hello")];
 exactBuffer.GetString(exact, sizeof(exact));
 assert(std::string("hello") == exact);
 const char binary[] = "hello\0world";
 std::vector<char> binaryBlock(binary, binary + sizeof(binary));
 CUtlBuffer binaryBuffer(binaryBlock.data(), (int)binaryBlock.size(), false);
 memset(out, '#', sizeof(out));
 binaryBuffer.GetString(out, capacity);
 assert(std::string("hel") == out && '#' == out[capacity]);
 binaryBuffer.GetString(out, sizeof(out));
 assert(std::string("worl") == out && (int)binaryBlock.size() == binaryBuffer.TellGet());
 CUtlBuffer emptyBuffer(text.data(), (int)text.size(), true);
 memset(out, '#', sizeof(out));
 emptyBuffer.GetString(out, 0);
 assert('#' == out[0] && 0 == emptyBuffer.TellGet());
 puts("PASS: CUtlBuffer::GetString with a capacity truncates, terminates and skips the rest of the string");
}

int main() {
 ParsesBlocksCommentsAndEscapes();
 OverlongTokensAreTruncated();
 FindsAndCreatesKeyPaths();
 ConvertsBetweenTypes();
 CopiesAndNumbersKeys();
 SavesAndLoadsFiles();
 RepeatedKeysFreeTheOldValue();
 QuotedBracesAreValues();
 CompleteInputStaysInTheBuffer();
 UnterminatedQuoteStaysInTheBuffer();
 TrailingCommentStaysInTheBuffer();
 MissingCloseStaysInTheBuffer();
 EscapeAtTheEndStaysInTheBuffer();
 SlashAtTheEndStaysInTheBuffer();
 BufferWritesNumbersAndText();
 BufferReadsBoundedStrings();
 return 0;
}
'''


def main():
    public = ROOT / 'vgui2/src/public'
    code = STUBS + file_system_stubs((public / 'FileSystem.h').read_text(encoding='utf-8')) + TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-keyvalues-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([path / 'test.cpp', public / 'KeyValues.cpp', public / 'UtlBuffer.cpp'],
                                     path / 'test', includes=[public, ROOT / 'public'],
                                     sanitizers=['address', 'undefined'],
                                     # the POSIX build gets this from common/posix_compat.h, which it force-includes
                                     defines=[] if os.name == 'nt' else ['_vsnprintf=vsnprintf'])
        subprocess.run([str(executable)], check=True, cwd=path)


if __name__ == '__main__':
    main()
