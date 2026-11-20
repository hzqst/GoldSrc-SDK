"""Exercise the production VGUI2 CSchemeManager::ReloadSchemes against a scheme file that changes, drops a border, changes a font or can't be read.

Run: python tests/vgui2_scheme_reload_tests.py
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


STUBS = r'''
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#ifdef _MSC_VER
#include <string.h>
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#include <strings.h>
#define _snprintf snprintf
#endif

#define ASSERT_AT( exp, line ) assert( exp )

static int g_liveKeyValues = 0;
static int g_liveBorders = 0;
static int g_liveSchemes = 0;
static int g_fileReads = 0;
static bool g_failOpen = false;
static bool g_defineFrameBorder = true;
static int g_borderResets = 0;
static const char* g_fontTall = "12";
static bool g_secondGlyphSet = false;
static std::map<std::string, std::string> g_baseBorderInset; // scheme file name -> BaseBorder inset

class IFileSystem;
static IFileSystem* g_pFileSystem = nullptr;

struct KeyValuesSystem {
 int GetSymbolForString(const char* name) {
  std::string key(name);
  for (char& c : key) c = (char)tolower((unsigned char)c);
  auto it = symbols.find(key);
  if (it != symbols.end()) return it->second;
  int symbol = (int)symbols.size() + 1;
  symbols[key] = symbol;
  return symbol;
 }
 std::map<std::string, int> symbols;
} g_KeyValuesSystem, *g_pKeyValues = &g_KeyValuesSystem;

// a minimal KeyValues tree, LoadFromFile fills it from g_baseBorderInset
class KeyValues {
public:
 enum types_t { TYPE_NONE, TYPE_STRING };
 explicit KeyValues(const char* keyName, KeyValues* parentKey = nullptr) : name(keyName), parent(parentKey) { ++g_liveKeyValues; }
 ~KeyValues() { for (KeyValues* child : children) delete child; --g_liveKeyValues; }
 void deleteThis() { delete this; }
 bool LoadFromFile(IFileSystem*, const char* fileName, const char* pathID) {
  if (pathID) return false; // nothing lives in the SKIN path, the NULL path is the real read
  ++g_fileReads;
  auto it = g_baseBorderInset.find(fileName);
  if (g_failOpen || it == g_baseBorderInset.end()) return false;
  KeyValues* borders = FindKey("Borders", true);
  borders->FindKey("BaseBorder", true)->SetString("inset", it->second.c_str());
  borders->SetString("ButtonBorder", "BaseBorder");
  if (g_defineFrameBorder) borders->FindKey("FrameBorder", true)->SetString("inset", "4 4 4 4");
  KeyValues* glyph = FindKey("Fonts", true)->FindKey("Default", true)->FindKey("1", true);
  glyph->SetString("name", "Tahoma");
  glyph->SetString("tall", g_fontTall);
  if (g_secondGlyphSet) {
   KeyValues* second = FindKey("Fonts", true)->FindKey("Default", true)->FindKey("2", true);
   second->SetString("name", "Verdana");
   second->SetString("tall", "12");
  }
  return true;
 }
 KeyValues* FindKey(const char* keyName, bool create = false) {
  int symbol = g_pKeyValues->GetSymbolForString(keyName);
  if (KeyValues* found = FindKey(symbol)) return found;
  if (!create) return nullptr;
  children.push_back(new KeyValues(keyName, this));
  return children.back();
 }
 KeyValues* FindKey(int symbol) {
  for (KeyValues* child : children)
   if (child->GetNameSymbol() == symbol) return child;
  return nullptr;
 }
 KeyValues* GetFirstSubKey() { return children.empty() ? nullptr : children[0]; }
 KeyValues* GetNextKey() {
  if (!parent) return nullptr;
  for (size_t i = 0; i + 1 < parent->children.size(); ++i)
   if (parent->children[i] == this) return parent->children[i + 1];
  return nullptr;
 }
 const char* GetString(const char* keyName = nullptr, const char* defaultValue = "") {
  KeyValues* key = keyName ? FindKey(keyName) : this;
  return key && key->hasValue ? key->value.c_str() : defaultValue;
 }
 void SetString(const char* keyName, const char* text) {
  KeyValues* key = FindKey(keyName, true);
  key->value = text ? text : "";
  key->hasValue = true;
 }
 float GetFloat(const char* keyName, float defaultValue = 0.0f) { const char* s = GetString(keyName, nullptr); return s ? (float)atof(s) : defaultValue; }
 int GetInt(const char* keyName, int defaultValue = 0) { const char* s = GetString(keyName, nullptr); return s ? atoi(s) : defaultValue; }
 int GetDataType() { return hasValue ? TYPE_STRING : TYPE_NONE; }
 int GetNameSymbol() { return g_pKeyValues->GetSymbolForString(name.c_str()); }
 const char* GetName() { return name.c_str(); }
 std::string name;
 std::string value;
 bool hasValue = false;
 KeyValues* parent;
 std::vector<KeyValues*> children;
};

template <class T>
class CUtlVector {
public:
 int AddToTail() { elements.emplace_back(); return Count() - 1; }
 int AddToTail(const T& element) { elements.push_back(element); return Count() - 1; }
 int Count() const { return (int)elements.size(); }
 T& operator[](int i) { return elements[i]; }
 void Remove(int i) { elements.erase(elements.begin() + i); }
 void RemoveAll() { elements.clear(); }
 std::vector<T> elements;
};

namespace vgui2 {
typedef unsigned long HScheme;
typedef unsigned long HFont;
class IScheme {};
class IBorder {};
class Bitmap {};

class Border : public IBorder {
public:
 Border() { ++g_liveBorders; }
 ~Border() { --g_liveBorders; }
 void SetName(const char* borderName) { name = borderName; }
 const char* GetName() { return name.c_str(); }
 void ResetSettings() { inset.clear(); ++g_borderResets; }
 void ApplySchemeSettings(IScheme*, KeyValues* inResourceData) { inset = inResourceData->GetString("inset", ""); }
 std::string name;
 std::string inset;
};

class ISurface {
public:
 enum SurfaceFeature_e { ANTIALIASED_FONTS = 1, DROPSHADOW_FONTS };
 enum EFontFlags { FONTFLAG_ITALIC = 0x1, FONTFLAG_UNDERLINE = 0x2, FONTFLAG_STRIKEOUT = 0x4, FONTFLAG_SYMBOL = 0x8,
  FONTFLAG_ANTIALIAS = 0x10, FONTFLAG_DROPSHADOW = 0x40, FONTFLAG_ADDITIVE = 0x80, FONTFLAG_ROTARY = 0x200 };
 void SetProportionalBase(int, int) {}
 void SetHDProportionalBase(int, int) {}
 void AddCustomFontFile(const char*) {}
 void GetScreenSize(int& wide, int& tall) { wide = 640; tall = 480; }
 HFont CreateFont() { return ++fonts; }
 void AddGlyphSetToFont(HFont font, const char* name, int tall, int, int, int, int, int, int) {
  assert(font == fonts); // glyph sets only go to the font just created
  assert(0 == strcmp("Tahoma", name) || 0 == strcmp("Verdana", name));
  lastTall = tall;
  ++glyphSets;
 }
 bool SupportsFeature(SurfaceFeature_e) { return false; }
 HFont fonts = 0;
 int glyphSets = 0;
 int lastTall = 0;
};
}
using namespace vgui2;

static ISurface g_Surface;
static ISurface* g_pSurface = &g_Surface;

// only the members reloading touches
class CScheme : public IScheme {
public:
 struct SchemeBorder_t
 {
  Border* border;
  int borderSymbol;
  bool bSharedBorder;
 };
 struct fontalias_t
 {
  char _fontName[64];
  HFont _font;
  int _glyphNameSymbol;
  int _tall;
  int _weight;
  int _blur;
  int _scanlines;
  int _flags;
 };
 CScheme() { fileName[0] = 0; tag[0] = 0; ++g_liveSchemes; }
 ~CScheme() { --g_liveSchemes; }
 IBorder* GetBorder(const char* borderName);
 void Shutdown(bool full);
 void LoadFromFile(const char* inFilename, const char* inTag, KeyValues* inKeys);
 void Reload(KeyValues* inKeys);
 void LoadFonts();
 void LoadBorders();
 Border* TakeRetiredBorder(const char* borderName);
 HFont GetFontEx(const char* fontName, bool proportional, bool hd);
 HFont FindFontInAliasList(const char* fontName);
 HFont FindPreviousFont(int firstAlias);
 const char* GetMungedFontName(const char* fontName, const char* scheme, bool proportional, bool bHDProportional);
 int GetAdjustedFontHeightForCurrentLanguage(int tall) { return tall; }
 char fileName[256];
 char tag[64];
 KeyValues* data = nullptr;
 KeyValues* baseSettings = nullptr;
 KeyValues* colors = nullptr;
 CUtlVector<SchemeBorder_t> m_BorderList;
 Border* _baseBorder = nullptr;
 KeyValues* _borders = nullptr;
 CUtlVector<fontalias_t> _fontAliases;
 CUtlVector<fontalias_t> m_PreviousFontAliases;
 CUtlVector<Border*> m_RetiredBorders;
};

class CSchemeManager {
public:
 struct CachedBitmapHandle_t
 {
  Bitmap* bitmap;
  bool hardwareFiltered;
 };
 // no images are cached while reloading, the destructor only walks the tree
 struct BitmapTree {
  int MaxElement() const { return 0; }
  bool IsValidIndex(int) const { return false; }
  CachedBitmapHandle_t& operator[](int) { static CachedBitmapHandle_t none = {}; return none; }
  void RemoveAll() {}
 };
 CSchemeManager() { _scheme.AddToTail(new CScheme()); }
 ~CSchemeManager();
 HScheme GetScheme(const char* tag) {
  for (int i = 1; i < _scheme.Count(); ++i)
   if (!strcasecmp(tag, _scheme[i]->tag)) return i;
  return 1;
 }
 HScheme FindLoadedScheme(const char* fileName);
 HScheme LoadSchemeFromFile(const char* fileName, const char* tag);
 void ReloadSchemes();
 int GetProportionalScaledValue(int normalizedValue) { return normalizedValue; }
 int GetHDProportionalScaledValue(int normalizedValue) { return normalizedValue; }
 CUtlVector<CScheme*> _scheme;
 BitmapTree m_Bitmaps;
};
static CSchemeManager g_Scheme;
'''

TESTS = r'''
static const char* SCHEME_FILE = "resource/testscheme.res";

static CScheme* Scheme(HScheme handle) { return g_Scheme._scheme[(int)handle]; }

static Border* GetBorder(HScheme handle, const char* name) { return (Border*)Scheme(handle)->GetBorder(name); }

static void Release() {
 for (int i = 0; i < g_Scheme._scheme.Count(); ++i) {
  g_Scheme._scheme[i]->Shutdown(true);
  delete g_Scheme._scheme[i];
 }
 g_Scheme._scheme.RemoveAll();
 g_Scheme._scheme.AddToTail(new CScheme());
 g_defineFrameBorder = true;
 g_borderResets = 0;
 g_fontTall = "12";
 g_secondGlyphSet = false;
 assert(0 == g_liveBorders);
 assert(0 == g_liveKeyValues);
}

static HScheme Load(const char* inset) {
 g_fileReads = 0;
 g_failOpen = false;
 g_baseBorderInset[SCHEME_FILE] = inset;
 HScheme handle = g_Scheme.LoadSchemeFromFile(SCHEME_FILE, "TestScheme");
 assert(1 == handle);
 assert(1 == g_fileReads);
 assert(2 == g_liveBorders);
 assert(std::string(inset) == GetBorder(handle, "BaseBorder")->inset);
 return handle;
}

static void ReloadReadsTheFileAgainAndRebuildsBorders() {
 HScheme handle = Load("1 1 1 1");
 int aliases = Scheme(handle)->_fontAliases.Count();
 assert(3 == aliases);

 g_baseBorderInset[SCHEME_FILE] = "2 2 2 2";
 g_Scheme.ReloadSchemes();

 assert(2 == g_fileReads);
 assert(handle == g_Scheme.GetScheme("TestScheme"));
 assert(handle == g_Scheme.FindLoadedScheme(SCHEME_FILE));
 assert(0 == strcmp(SCHEME_FILE, Scheme(handle)->fileName));
 assert(std::string("2 2 2 2") == GetBorder(handle, "BaseBorder")->inset);
 assert(GetBorder(handle, "BaseBorder") == GetBorder(handle, "ButtonBorder"));
 assert(GetBorder(handle, "BaseBorder") == Scheme(handle)->_baseBorder);
 assert(2 == g_liveBorders);
 assert(aliases == Scheme(handle)->_fontAliases.Count());
 assert(0 != Scheme(handle)->GetFontEx("Default", true, false));

 g_Scheme.ReloadSchemes();
 assert(3 == g_fileReads);
 assert(2 == g_liveBorders);
 assert(aliases == Scheme(handle)->_fontAliases.Count());
 Release();
 puts("PASS: reloading reads the scheme file again and rebuilds borders and fonts under the same handle");
}

static void FailedReloadKeepsThePreviousScheme() {
 HScheme handle = Load("1 1 1 1");
 Border* border = GetBorder(handle, "BaseBorder");
 KeyValues* data = Scheme(handle)->data;
 int aliases = Scheme(handle)->_fontAliases.Count();
 int liveKeyValues = g_liveKeyValues;

 g_failOpen = true;
 g_Scheme.ReloadSchemes();

 assert(2 == g_fileReads);
 assert(border == GetBorder(handle, "BaseBorder"));
 assert(border == GetBorder(handle, "ButtonBorder"));
 assert(std::string("1 1 1 1") == border->inset);
 assert(data == Scheme(handle)->data);
 assert(aliases == Scheme(handle)->_fontAliases.Count());
 assert(liveKeyValues == g_liveKeyValues);
 Release();
 puts("PASS: a scheme file that can't be read keeps the previous data and borders");
}

static void ReloadKeepsTheAddressOfEveryBorder() {
 HScheme handle = Load("1 1 1 1");
 Border* base = GetBorder(handle, "BaseBorder");
 Border* frame = GetBorder(handle, "FrameBorder");
 assert(base != frame);

 g_baseBorderInset[SCHEME_FILE] = "2 2 2 2";
 g_Scheme.ReloadSchemes();

 assert(base == GetBorder(handle, "BaseBorder"));
 assert(base == GetBorder(handle, "ButtonBorder"));
 assert(base == Scheme(handle)->_baseBorder);
 assert(frame == GetBorder(handle, "FrameBorder"));
 assert(std::string("2 2 2 2") == base->inset);
 assert(std::string("4 4 4 4") == frame->inset);
 assert(2 == g_borderResets);
 assert(2 == g_liveBorders);
 assert(0 == Scheme(handle)->m_RetiredBorders.Count());
 Release();
 puts("PASS: reloading reapplies each border definition to the border object of the same name");
}

static void RemovedBorderStaysAliveUntilItIsDefinedAgain() {
 HScheme handle = Load("1 1 1 1");
 Border* frame = GetBorder(handle, "FrameBorder");

 g_defineFrameBorder = false;
 g_Scheme.ReloadSchemes();

 assert(GetBorder(handle, "BaseBorder") == GetBorder(handle, "FrameBorder"));
 assert(1 == Scheme(handle)->m_RetiredBorders.Count());
 assert(frame == Scheme(handle)->m_RetiredBorders[0]);
 assert(std::string("4 4 4 4") == frame->inset);
 assert(2 == g_liveBorders);

 g_defineFrameBorder = true;
 g_Scheme.ReloadSchemes();

 assert(frame == GetBorder(handle, "FrameBorder"));
 assert(0 == Scheme(handle)->m_RetiredBorders.Count());
 assert(2 == g_liveBorders);
 Release();
 puts("PASS: a border dropped by a reload stays alive and is reused when the definition comes back");
}

static void ReloadKeepsTheHandleOfAnUnchangedFont() {
 HScheme handle = Load("1 1 1 1");
 HFont fonts = g_Surface.fonts;
 int glyphSets = g_Surface.glyphSets;
 HFont normal = Scheme(handle)->GetFontEx("Default", false, false);
 HFont proportional = Scheme(handle)->GetFontEx("Default", true, false);
 HFont hd = Scheme(handle)->GetFontEx("Default", true, true);
 assert(0 != normal && normal != proportional && proportional != hd && normal != hd);

 g_Scheme.ReloadSchemes();
 g_Scheme.ReloadSchemes();

 assert(fonts == g_Surface.fonts);
 assert(glyphSets == g_Surface.glyphSets);
 assert(normal == Scheme(handle)->GetFontEx("Default", false, false));
 assert(proportional == Scheme(handle)->GetFontEx("Default", true, false));
 assert(hd == Scheme(handle)->GetFontEx("Default", true, true));
 assert(3 == Scheme(handle)->_fontAliases.Count());
 assert(0 == Scheme(handle)->m_PreviousFontAliases.Count());
 Release();
 puts("PASS: reloading a font with the same glyph sets keeps its handle and creates no font");
}

static void ReloadCreatesAFontWhoseGlyphSetsChanged() {
 HScheme handle = Load("1 1 1 1");
 HFont fonts = g_Surface.fonts;
 HFont normal = Scheme(handle)->GetFontEx("Default", false, false);

 g_fontTall = "16";
 g_Scheme.ReloadSchemes();

 assert(fonts + 3 == g_Surface.fonts);
 assert(16 == g_Surface.lastTall);
 HFont reloaded = Scheme(handle)->GetFontEx("Default", false, false);
 assert(0 != reloaded && normal != reloaded);
 assert(3 == Scheme(handle)->_fontAliases.Count());

 g_Scheme.ReloadSchemes();
 assert(fonts + 3 == g_Surface.fonts);
 assert(reloaded == Scheme(handle)->GetFontEx("Default", false, false));
 Release();
 puts("PASS: reloading a font whose glyph sets changed creates a new font once");
}

static void ReloadMatchesEveryGlyphSetOfAFont() {
 HScheme handle = Load("1 1 1 1");
 HFont fonts = g_Surface.fonts;
 HFont single = Scheme(handle)->GetFontEx("Default", false, false);

 g_secondGlyphSet = true;
 g_Scheme.ReloadSchemes();

 assert(fonts + 3 == g_Surface.fonts);
 assert(6 == Scheme(handle)->_fontAliases.Count());
 HFont two = Scheme(handle)->GetFontEx("Default", false, false);
 assert(single != two);
 int glyphSets = g_Surface.glyphSets;

 g_Scheme.ReloadSchemes();
 assert(fonts + 3 == g_Surface.fonts);
 assert(glyphSets == g_Surface.glyphSets);
 assert(two == Scheme(handle)->GetFontEx("Default", false, false));

 g_secondGlyphSet = false;
 g_Scheme.ReloadSchemes();
 assert(fonts + 6 == g_Surface.fonts);
 assert(3 == Scheme(handle)->_fontAliases.Count());
 Release();
 puts("PASS: a font is only reused when all of its glyph sets match, not a leading part of them");
}

static void DestroyingTheManagerDeletesEveryScheme() {
 int liveSchemes = g_liveSchemes;
 CSchemeManager* manager = new CSchemeManager();
 manager->_scheme.AddToTail(new CScheme());
 assert(liveSchemes + 2 == g_liveSchemes);

 delete manager;

 assert(liveSchemes == g_liveSchemes);
 puts("PASS: destroying the scheme manager deletes the null scheme and every loaded scheme");
}

int main() {
 ReloadReadsTheFileAgainAndRebuildsBorders();
 FailedReloadKeepsThePreviousScheme();
 ReloadKeepsTheAddressOfEveryBorder();
 RemovedBorderStaysAliveUntilItIsDefinedAgain();
 ReloadKeepsTheHandleOfAnUnchangedFont();
 ReloadCreatesAFontWhoseGlyphSetsChanged();
 ReloadMatchesEveryGlyphSetOfAFont();
 DestroyingTheManagerDeletesEveryScheme();
}
'''

SIGNATURES = (
    'static KeyValues* LoadSchemeData(',
    'void CSchemeManager::ReloadSchemes()',
    'CSchemeManager::~CSchemeManager()',
    'IBorder* CScheme::GetBorder(',
    'void CScheme::Shutdown(',
    'HScheme CSchemeManager::FindLoadedScheme(',
    'HScheme CSchemeManager::LoadSchemeFromFile(',
    'void CScheme::LoadFromFile(',
    'void CScheme::Reload(',
    'const char* CScheme::GetMungedFontName(',
    'HFont CScheme::FindFontInAliasList(',
    'HFont CScheme::FindPreviousFont(',
    'HFont CScheme::GetFontEx(',
    'void CScheme::LoadFonts()',
    'void CScheme::LoadBorders()',
    'Border* CScheme::TakeRetiredBorder(',
)


def main():
    source = (ROOT / 'vgui2/src/Scheme.cpp').read_text(encoding='utf-8')
    code = STUBS + ''.join(function(source, signature) + '\n' for signature in SIGNATURES)
    code += TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-scheme-reload-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
