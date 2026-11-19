"""Exercise the production Condition Zero Deleted Scenes material file loaders with a tracked COM_LoadFile and COM_FreeFile.

Run: python tests/czeror_material_file_tests.py
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
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#ifndef _WIN32
#include <strings.h>
#define _stricmp strcasecmp
#endif
struct vec3_t { float x, y, z; };
struct materials_t {
 char m_szOriginalName[64], m_szTextName[64], m_szBulletImpactName[64];
 char m_chMaterialType;
 float m_flSize, m_flScaleSpeed, m_flBrightness, m_flFadeSpeed, m_flLife, m_flGravity, m_flAfterDampGravity;
 float m_flDampingVelocity, m_flVelMod, m_flDampingTime, m_flMinScale, m_flMaxScale;
 int m_iDebriSpriteNum, m_iRendermode, m_iCollisionFlags, m_iAfterDampFlags, m_iSpawnDebris, m_iSpawnSmoke;
 int m_iSpawnSparks, m_iSpawnImpacts, m_iDecalNum, m_bRandomFlip;
 vec3_t m_vColor, m_vSmokeColor;
};
struct cvar_t { float value; };
static cvar_t autocreate = {0.0f};
static cvar_t* mat_autocreate = &autocreate;
static std::map<std::string, std::string> files;
static std::set<char*> loadedFiles;
static int wrongFrees = 0;
static struct {
 void* COM_LoadFile(const char* name, int, int*) {
  auto file = files.find(name);
  if (file == files.end()) return nullptr;
  char* buffer = new char[file->second.size() + 1];
  memcpy(buffer, file->second.c_str(), file->second.size() + 1);
  loadedFiles.insert(buffer);
  return buffer;
 }
 // frees only the buffers COM_LoadFile returned, and counts any other pointer instead of corrupting the heap
 void COM_FreeFile(void* file) {
  if (!file) return;
  if (!loadedFiles.erase((char*)file)) { ++wrongFrees; return; }
  delete[] (char*)file;
 }
 // whitespace separated tokens or quoted strings, and NULL with an empty token at the end
 char* COM_ParseFile(char* data, char* token) {
  token[0] = 0;
  if (!data) return nullptr;
  while (*data && isspace((unsigned char)*data)) ++data;
  if (!*data) return nullptr;
  int length = 0;
  if (*data == '"') {
   for (++data; *data && *data != '"'; ++data) token[length++] = *data;
   if (*data) ++data;
  } else {
   for (; *data && !isspace((unsigned char)*data); ++data) token[length++] = *data;
  }
  token[length] = 0;
  return data;
 }
 void Con_Printf(const char*, ...) {}
 void Con_DPrintf(const char*, ...) {}
 const char* pfnGetLevelName() { return "maps/c1a0.bsp"; }
 void pfnAddCommand(const char*, void (*)()) {}
 void Cvar_SetValue(const char*, float value) { autocreate.value = value; }
} gEngfuncs;
static void COM_FileBase(const char* in, char* out) {
 const char* start = in;
 for (const char* c = in; *c; ++c) if (*c == '/' || *c == '\\') start = c + 1;
 const char* end = strrchr(start, '.');
 size_t length = end ? (size_t)(end - start) : strlen(start);
 memcpy(out, start, length);
 out[length] = 0;
}
extern "C" char PM_FindTextureType(char*) { return 'C'; }
static char* g_PFile;
static int loadedMaterials = 0;
// only the members LoadLevelMaterials uses; LoadMaterial reads one texture entry from g_PFile like the production one
class CMaterialSystem {
public:
 void FixTextureName(const char* in, char* out) { strcpy(out, in); }
 bool LoadMaterial(char*) {
  char token[1024];
  do g_PFile = gEngfuncs.COM_ParseFile(g_PFile, token);
  while (g_PFile && strcmp(token, "]"));
  ++loadedMaterials;
  return true;
 }
 void ClearMaterials() {}
 void CopyMaterials(CMaterialSystem*) {}
 struct { void WriteToErrorFile(const char*, const char*) {} } m_MatFIO;
 int m_iElements = 0, m_iNewMaterials = 0;
};
static CMaterialSystem g_pMaterialSystem;
static CMaterialSystem* g_pMaterialSystemPointer = &g_pMaterialSystem;
static int g_iDesiredMaterials = 0;
static bool TextureNameInList(char*) { return true; }
static void LoadTextureLump(char*) {}
static void MAT_FlushAndReload() {}
static int createdFiles = 0;
// loads the file it wrote into g_PFile like the production CreateMaterialFile
static void CreateMaterialFile(char* level) {
 ++createdFiles;
 std::string name = std::string("materials/") + level + ".mat";
 files[name] = "texture \"created\"\n[\n\tmaterial\n\t{\n\t\ttype \"metal\"\n\t}\n]\n";
 g_PFile = (char*)gEngfuncs.COM_LoadFile(name.c_str(), 5, NULL);
}
'''

TESTS = r'''
static void AssertAllFreed() {
 assert(loadedFiles.empty());
 assert(0 == wrongFrees);
}

static bool SetDefaults(const char* text) {
 files = {{"materials/defaults/metal.mat", text}};
 materials_t material = {};
 bool result = MAT_SetDefaults(2, &material);
 AssertAllFreed();
 return result;
}

static void SetDefaultsFreesTheMaterialFile() {
 files = {{"materials/defaults/metal.mat", "debris\n{\n size 2\n color 1 2 3\n sprite sprites/metal1.spr\n}\n"
                                           "smoke\n{\n allow 1\n color 4 5 6\n}\nimpacts\n{\n decalname shot\n maxscale 3\n}\n"}};
 materials_t material = {};
 assert(MAT_SetDefaults(2, &material));
 AssertAllFreed();
 assert(2.0f == material.m_flSize && 3.0f == material.m_vColor.z && 6.0f == material.m_vSmokeColor.z);
 assert(1 == material.m_iSpawnSmoke && 3.0f == material.m_flMaxScale && 'M' == material.m_chMaterialType);
 assert(std::string("sprites/metal.spr") == material.m_szTextName && std::string("shot") == material.m_szBulletImpactName);
 puts("PASS: reading the default material frees the loaded file, not the parse position");
}

static void SetDefaultsFreesTheMaterialFileOnErrors() {
 assert(!SetDefaults("debris\nsize 2\n"));
 assert(!SetDefaults("debris\n{\n unknown 2\n}\n"));
 assert(!SetDefaults("debris\n{\n size 2\n"));
 assert(!SetDefaults("smoke\nallow 1\n"));
 assert(!SetDefaults("sparks\n{\n color 1 2 3\n}\n"));
 assert(!SetDefaults("impacts\n{\n decalname shot\n"));
 // an empty file only sets the material type
 assert(SetDefaults(""));
 files.clear();
 materials_t material = {};
 assert(!MAT_SetDefaults(2, &material));
 AssertAllFreed();
 puts("PASS: a default material file that fails to parse is still freed");
}

static const char* const kMaterialList = "texture \"crate\"\n[\n\tmaterial\n\t{\n\t\ttype \"wood\"\n\t}\n]\n"
                                         "texture \"floor\"\n[\n\tmaterial\n\t{\n\t\ttype \"tile\"\n\t}\n]\n";

static void LevelMaterialsFreeBothFiles() {
 files = {{"materials/czero.mat", kMaterialList}, {"materials/c1a0.mat", kMaterialList}};
 loadedMaterials = 0;
 LoadLevelMaterials();
 assert(4 == loadedMaterials && !g_PFile);
 AssertAllFreed();
 puts("PASS: loading the level materials frees czero.mat and the level material file, not the parse position");
}

static void LevelMaterialsFreeTheCreatedFile() {
 files = {{"materials/czero.mat", kMaterialList}};
 autocreate.value = 1.0f;
 loadedMaterials = createdFiles = 0;
 LoadLevelMaterials();
 assert(1 == createdFiles && 3 == loadedMaterials && 0.0f == autocreate.value);
 AssertAllFreed();
 files.clear();
 LoadLevelMaterials();
 AssertAllFreed();
 puts("PASS: loading the level materials frees the material file it creates, and copes with no material files");
}
'''


def main():
    source = (ROOT / 'czeror/cl_dll/materials/z_material_load.cpp').read_text(encoding='latin-1')
    code = STUBS
    for signature in ('int MAT_GetRenderMode(', 'static int MAT_FileBaseLength(', 'bool MAT_SetDefaults(',
                      'void LoadMaterialFromNameList(', 'void LoadLevelMaterials('):
        code += function(source, signature) + '\n'
    code += TESTS
    code += ('\nint main() { SetDefaultsFreesTheMaterialFile(); SetDefaultsFreesTheMaterialFileOnErrors(); '
             'LevelMaterialsFreeBothFiles(); LevelMaterialsFreeTheCreatedFile(); }\n')
    with tempfile.TemporaryDirectory(prefix='goldsrc-czeror-material-file-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
