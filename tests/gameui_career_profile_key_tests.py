"""Exercise the production Career profile keys, name checks and migration with an in-memory file system.

Run: python tests/gameui_career_profile_key_tests.py
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
    # parameter lists may wrap; declarations end with ';' and are skipped
    start = re.search(re.escape(signature) + r'[^;{]*\n\{', source).start()
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "checksum_md5.h"
#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#endif
typedef void* FileHandle_t;
typedef int FileFindHandle_t;
struct CCareerProfileData { std::string name; std::string tag; long filetime = 0; };
typedef std::vector<CCareerProfileData*> ProfileVector;
class CCareerGame {
public:
 // stub profile format: "CZP" NUL name NUL tag
 bool ParseSavedProfile(const unsigned char* data, int length, CCareerProfileData* save) {
  std::string bytes((const char*)data, length);
  if (bytes.compare(0, 4, std::string("CZP\0", 4))) return false;
  size_t end = bytes.find('\0', 4);
  save->name = bytes.substr(4, end - 4);
  save->tag = end == std::string::npos ? "" : bytes.substr(end + 1);
  return true;
 }
 static std::string GetSaveGamePlayerName(const char* name);
 static bool IsValidProfileName(const char* name);
 ProfileVector BuildProfileList();
 CCareerProfileData* GetProfileForName(const char* name);
 void DeleteProfile(const char* name);
};
struct Engine {
 std::string name;
 const char* pfnGetCvarString(const char*) { return name.c_str(); }
} engineStub, *engine = &engineStub;
struct CareerDefinition { const char* GetSaveDirectory() { return "MissionPacks/Test/Save"; } };
struct CareerDefinitionManager {
 CareerDefinition active;
 CareerDefinition* GetActiveCareerDefinition() { return &active; }
} definitionsStub, *TheCareerDefinitions = &definitionsStub;
std::map<std::string, std::string> files;
struct OpenFile { std::string path, data; size_t position; bool write; };
namespace vgui2 {
struct FileSystem {
 std::vector<std::string> found;
 size_t next = 0;
 FileHandle_t Open(const char* path, const char* mode, const char*) {
  bool write = mode[0] == 'w';
  if (!write && !files.count(path)) return nullptr;
  return new OpenFile{path, write ? std::string() : files[path], 0, write};
 }
 unsigned int Size(FileHandle_t file) { return (unsigned int)((OpenFile*)file)->data.size(); }
 int Read(void* out, int size, FileHandle_t file) {
  OpenFile* open = (OpenFile*)file;
  int count = (int)std::min((size_t)size, open->data.size() - open->position);
  memcpy(out, open->data.data() + open->position, count);
  open->position += count;
  return count;
 }
 int Write(const void* in, int size, FileHandle_t file) { ((OpenFile*)file)->data.append((const char*)in, size); return size; }
 bool IsOk(FileHandle_t) { return true; }
 void Close(FileHandle_t file) {
  OpenFile* open = (OpenFile*)file;
  if (open->write) files[open->path] = open->data;
  delete open;
 }
 bool FileExists(const char* path) { return files.count(path) != 0; }
 void RemoveFile(const char* path, const char*) { files.erase(path); }
 void CreateDirHierarchy(const char*, const char*) {}
 long GetFileTime(const char*) { return 1; }
 // only the "<dir>/CZProfile-*.czp" pattern is used
 const char* FindFirst(const char* pattern, FileFindHandle_t*, const char*) {
  std::string prefix = std::string(pattern, strrchr(pattern, '/') + 1) + "CZProfile-";
  found.clear();
  next = 0;
  for (const auto& file : files) {
   const std::string& path = file.first;
   if (!path.compare(0, prefix.size(), prefix) && path.find('/', prefix.size()) == std::string::npos &&
     path.size() > 4 && !path.compare(path.size() - 4, 4, ".czp"))
    found.push_back(path.substr(strrchr(pattern, '/') + 1 - pattern));
  }
  return FindNext(0);
 }
 const char* FindNext(FileFindHandle_t) { return next < found.size() ? found[next++].c_str() : nullptr; }
 void FindClose(FileFindHandle_t) {}
 char* ParseFile(char* data, char* token, bool* quoted) {
  *quoted = false;
  token[0] = 0;
  while (*data == ' ') data++;
  if (!*data) return nullptr;
  int length = 0;
  if (*data == '"') {
   *quoted = true;
   for (data++; *data && *data != '"'; ) token[length++] = *data++;
   if (*data) data++;
  } else {
   while (*data && *data != ' ') token[length++] = *data++;
  }
  token[length] = 0;
  return data;
 }
} fileSystemStub;
FileSystem* filesystem() { return &fileSystemStub; }
}
static bool ConfigHasOwner(const std::string& path, const char* owner);
'''

TESTS = r'''
static const std::string SAVE = "MissionPacks/Test/Save/CZProfile-";

static std::string Profile(const std::string& name, const std::string& tag) {
 return std::string("CZP\0", 4) + name + std::string(1, '\0') + tag;
}

static void KeysMatchVGUI2Extension() {
 assert("u8hex-e6b58be8af9574657374" == CCareerGame::GetSaveGamePlayerName("\xe6\xb5\x8b\xe8\xaf\x95test"));
 assert("u8hex-f09f9880" == CCareerGame::GetSaveGamePlayerName("\xf0\x9f\x98\x80"));
 assert("u8hex-41" == CCareerGame::GetSaveGamePlayerName("A"));
 assert("u8hex-61" == CCareerGame::GetSaveGamePlayerName("a"));
 assert("u8hex-75386865782d3631" == CCareerGame::GetSaveGamePlayerName("u8hex-61"));
 assert("u8hex-204120" == CCareerGame::GetSaveGamePlayerName(" A "));
 assert(CCareerGame::IsValidProfileName("A / B%hs"));
 engineStub.name = "a";
 assert("u8hex-61" == CCareerGame::GetSaveGamePlayerName(nullptr));
 puts("PASS: u8hex keys match the VGUI2Extension samples");
}

static void UnsafeNamesHaveNoKey() {
 for (const std::string& name : {std::string(), std::string("a\nb"), std::string("a\rb"), std::string("a\tb"),
   std::string("a\"b"), std::string("a;b"), std::string("a\\b"), std::string("a\x7f"), std::string("a\xc2\x80"),
   std::string("a\xc2\x9f"), std::string("\xc0\x80"), std::string("\xed\xa0\x80"), std::string("\xf4\x90\x80\x80"),
   std::string("\x80"), std::string("\xe4\xb8"), std::string("\xf5\x80\x80\x80")}) {
  assert(CCareerGame::GetSaveGamePlayerName(name.c_str()).empty());
  assert(!CCareerGame::IsValidProfileName(name.c_str()));
 }
 assert(!CCareerGame::IsValidProfileName(nullptr));
 assert(CCareerGame::IsValidProfileName("a\xc2\xa0"));
 puts("PASS: empty, unsafe and invalid UTF-8 names have no key");
}

static void NameLengthFollowsTheWritecfgLimit() {
 // "Save/u8hex-" + 2 * 122 = 255 bytes; 123 bytes would make it 257
 const std::string longest(122, 'a'), tooLong(123, 'a');
 assert(CCareerGame::IsValidProfileName(longest.c_str()));
 assert(11 + 2 * longest.size() == ("Save/" + CCareerGame::GetSaveGamePlayerName(longest.c_str())).size());
 assert(!CCareerGame::IsValidProfileName(tooLong.c_str()));
 const std::string chinese = "\xe4\xb8\xad";
 std::string forty, fortyOne;
 for (int i = 0; i < 40; i++) forty += chinese;
 fortyOne = forty + chinese;
 assert(CCareerGame::IsValidProfileName(forty.c_str()));
 assert(!CCareerGame::IsValidProfileName(fortyOne.c_str()));
 puts("PASS: names up to 122 bytes are accepted and longer ones rejected");
}

static void HashedProfilesAreFoundAndMigrated() {
 files.clear();
 CCareerGame game;
 // MD5("A") from the c22ae8c key scheme
 const std::string hashed = "v2-7fc56270e7a70fa81a5935b72eacbe29";
 files[SAVE + hashed + ".czp"] = Profile("A", "hashed");
 files["Save/" + hashed + ".cfg"] = "unbindall\nname \"A\"\n";
 files[SAVE + "A.czp"] = Profile("A", "legacy");
 std::unique_ptr<CCareerProfileData> profile(game.GetProfileForName("A"));
 assert(profile && "hashed" == profile->tag);
 ProfileVector list = game.BuildProfileList();
 assert(1 == list.size() && "hashed" == list[0]->tag);
 for (CCareerProfileData* entry : list) delete entry;
 assert(MigrateLegacyProfile(&game, "A"));
 assert(Profile("A", "hashed") == files[SAVE + "u8hex-41.czp"]);
 assert(files.count("Save/u8hex-41.cfg"));
 assert(files.count(SAVE + hashed + ".czp") && files.count(SAVE + "A.czp"));
 list = game.BuildProfileList();
 assert(1 == list.size());
 for (CCareerProfileData* entry : list) delete entry;
 puts("PASS: v2 profiles are preferred over native ones, migrated to u8hex and listed once");
}

static void NativeProfilesAreMigrated() {
 files.clear();
 CCareerGame game;
 const std::string chinese = "\xe4\xb8\xad";
 files[SAVE + "___.czp"] = Profile(chinese, "legacy");
 files["Save/___.cfg"] = "name \"" + chinese + "\"\n";
 assert(MigrateLegacyProfile(&game, chinese.c_str()));
 assert(Profile(chinese, "legacy") == files[SAVE + "u8hex-e4b8ad.czp"]);
 assert(files.count("Save/u8hex-e4b8ad.cfg"));
 game.DeleteProfile(chinese.c_str());
 assert(files.empty());
 puts("PASS: native profiles are migrated, and delete removes every owned copy");
}

static void UnsafeNativeProfilesStayListedAndDeletable() {
 files.clear();
 CCareerGame game;
 files[SAVE + "a_b.czp"] = Profile("a;b", "legacy");
 ProfileVector list = game.BuildProfileList();
 assert(1 == list.size() && "a;b" == list[0]->name);
 for (CCareerProfileData* entry : list) delete entry;
 assert(!MigrateLegacyProfile(&game, "a;b"));
 assert(!CanWriteProfile(&game, "a;b"));
 game.DeleteProfile("a;b");
 assert(files.empty());
 puts("PASS: native profiles with unsafe names are listed and deletable but not migrated");
}

static void ForeignCanonicalProfileBlocksTheName() {
 files.clear();
 CCareerGame game;
 // a native profile for the literal name "u8hex-41" owns the canonical path of "A"
 files[SAVE + "u8hex-41.czp"] = Profile("u8hex-41", "legacy");
 assert(!MigrateLegacyProfile(&game, "A"));
 assert(!CanWriteProfile(&game, "A"));
 std::unique_ptr<CCareerProfileData> profile(game.GetProfileForName("A"));
 assert(!profile);
 ProfileVector list = game.BuildProfileList();
 assert(1 == list.size() && "u8hex-41" == list[0]->name);
 for (CCareerProfileData* entry : list) delete entry;
 puts("PASS: a canonical path owned by another name is never read or overwritten");
}
'''

SIGNATURES = [
    'static unsigned char* ReadFile(',
    'static std::string LegacyProfileKey(',
    'static std::string HashedProfileKey(',
    'static bool IsSafeProfileName(',
    'static std::vector<std::string> ProfileKeys(',
    'static std::string ProfilePath(',
    'static std::unique_ptr<CCareerProfileData> ReadOwnedProfile(',
    'static bool WriteProfileFile(',
    'static bool SameProfilePath(',
    'static bool CanWriteProfile(',
    'static bool ConfigHasOwner(',
    'static bool MigrateLegacyProfile(',
    'std::string CCareerGame::GetSaveGamePlayerName(',
    'bool CCareerGame::IsValidProfileName(',
    'ProfileVector CCareerGame::BuildProfileList(',
    'CCareerProfileData* CCareerGame::GetProfileForName(',
    'void CCareerGame::DeleteProfile(',
]


def main():
    profile = (ROOT / 'GameUI/CareerProfile.cpp').read_text(encoding='utf-8')
    code = STUBS
    code += '\n'.join(re.findall(r'^#define CAREER_\w+ \d+$', profile, re.M)) + '\n'
    code += ''.join(function(profile, signature) + '\n' for signature in SIGNATURES)
    code += TESTS
    code += ('\nint main() { KeysMatchVGUI2Extension(); UnsafeNamesHaveNoKey(); NameLengthFollowsTheWritecfgLimit(); '
             'HashedProfilesAreFoundAndMigrated(); NativeProfilesAreMigrated(); '
             'UnsafeNativeProfilesStayListedAndDeletable(); ForeignCanonicalProfileBlocksTheName(); }\n')
    with tempfile.TemporaryDirectory(prefix='goldsrc-gameui-career-profile-key-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([path / 'test.cpp', ROOT / 'public/checksum_md5.cpp'], path / 'test',
                                     includes=[ROOT / 'public', ROOT / 'common'], sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=path)


if __name__ == '__main__':
    main()
