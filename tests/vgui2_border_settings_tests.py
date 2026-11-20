"""Exercise the production VGUI2 Border::ApplySchemeSettings and Border::ResetSettings when a border is set up more than once.

Run: python tests/vgui2_border_settings_tests.py
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
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

static int g_liveLineArrays = 0;

// a minimal KeyValues tree built by the tests
class KeyValues {
public:
 explicit KeyValues(const char* keyName, KeyValues* parentKey = nullptr) : name(keyName), parent(parentKey) {}
 ~KeyValues() { for (KeyValues* child : children) delete child; }
 KeyValues* FindKey(const char* keyName, bool create = false) {
  for (KeyValues* child : children)
   if (child->name == keyName) return child;
  if (!create) return nullptr;
  children.push_back(new KeyValues(keyName, this));
  return children.back();
 }
 KeyValues* GetFirstSubKey() { return children.empty() ? nullptr : children[0]; }
 KeyValues* GetNextKey() {
  if (!parent) return nullptr;
  for (size_t i = 0; i + 1 < parent->children.size(); ++i)
   if (parent->children[i] == this) return parent->children[i + 1];
  return nullptr;
 }
 const char* GetString(const char* keyName, const char* defaultValue = "") {
  KeyValues* key = FindKey(keyName);
  return key && key->hasValue ? key->value.c_str() : defaultValue;
 }
 void SetString(const char* keyName, const char* text) {
  KeyValues* key = FindKey(keyName, true);
  key->value = text;
  key->hasValue = true;
 }
 std::string name;
 std::string value;
 bool hasValue = false;
 KeyValues* parent;
 std::vector<KeyValues*> children;
};

class Color {
public:
 Color() { color[0] = color[1] = color[2] = color[3] = 0; }
 Color(int r, int g, int b, int a) { color[0] = (unsigned char)r; color[1] = (unsigned char)g; color[2] = (unsigned char)b; color[3] = (unsigned char)a; }
 unsigned char operator[](int i) const { return color[i]; }
 unsigned char color[4];
};

namespace vgui2 {
class IScheme {
public:
 // colors are written as "r g b a" in these tests
 Color GetColor(const char* colorName, Color defaultColor) {
  int r, g, b, a;
  if (colorName && sscanf(colorName, "%d %d %d %d", &r, &g, &b, &a) == 4) return Color(r, g, b, a);
  return defaultColor;
 }
};

class IBorder {
public:
 enum sides_e { SIDE_LEFT = 0, SIDE_TOP = 1, SIDE_RIGHT = 2, SIDE_BOTTOM = 3 };
};

// only the members settings touch
class Border : public IBorder {
public:
 Border() { _inset[0] = _inset[1] = _inset[2] = _inset[3] = 0; memset(_sides, 0, sizeof(_sides)); }
 ~Border() { for (int i = 0; i < 4; i++) delete[] _sides[i].lines; }
 void SetInset(int left, int top, int right, int bottom);
 void GetInset(int& left, int& top, int& right, int& bottom);
 void ApplySchemeSettings(IScheme* pScheme, KeyValues* inResourceData);
 void ResetSettings();
 void ParseSideSettings(int side_index, KeyValues* inResourceData, IScheme* pScheme);
 int _inset[4];
 struct line_t
 {
  Color col;
  int startOffset;
  int endOffset;
  // count the line arrays so a dropped one shows up as a leak
  static void* operator new[](size_t size) { ++g_liveLineArrays; return ::operator new[](size); }
  static void operator delete[](void* pointer) { if (pointer) --g_liveLineArrays; ::operator delete[](pointer); }
 };
 struct side_t
 {
  int count;
  line_t* lines;
 };
 side_t _sides[4];
};
}
using namespace vgui2;
'''

TESTS = r'''
static void AddLines(KeyValues* settings, const char* side, int count, int alpha) {
 KeyValues* lines = settings->FindKey(side, true);
 for (int i = 0; i < count; ++i) {
  char name[16];
  char color[32];
  snprintf(name, sizeof(name), "%d", i + 1);
  snprintf(color, sizeof(color), "%d 0 0 %d", i + 1, alpha);
  KeyValues* line = lines->FindKey(name, true);
  line->SetString("color", color);
  line->SetString("offset", "1 2");
 }
}

static void ApplyingAgainReplacesTheLinesWithoutLeaking() {
 IScheme scheme;
 KeyValues first("Border");
 AddLines(&first, "Left", 2, 10);
 AddLines(&first, "Top", 1, 10);
 KeyValues second("Border");
 AddLines(&second, "Left", 3, 20);
 AddLines(&second, "Top", 1, 20);
 {
  Border border;
  border.ApplySchemeSettings(&scheme, &first);
  assert(2 == g_liveLineArrays);
  border.ApplySchemeSettings(&scheme, &second);
  assert(2 == g_liveLineArrays);
  assert(3 == border._sides[IBorder::SIDE_LEFT].count);
  assert(20 == border._sides[IBorder::SIDE_LEFT].lines[2].col[3]);
  assert(3 == border._sides[IBorder::SIDE_LEFT].lines[2].col[0]);
  assert(1 == border._sides[IBorder::SIDE_LEFT].lines[2].startOffset);
  assert(2 == border._sides[IBorder::SIDE_LEFT].lines[2].endOffset);
 }
 assert(0 == g_liveLineArrays);
 puts("PASS: applying settings to a border twice replaces the side lines and frees the old ones");
}

static void ResetClearsTheInsetAndEverySide() {
 IScheme scheme;
 KeyValues first("Border");
 first.SetString("inset", "1 2 3 4");
 AddLines(&first, "Left", 2, 10);
 AddLines(&first, "Top", 1, 10);
 KeyValues second("Border");
 second.SetString("inset", "5 6");
 AddLines(&second, "Left", 1, 20);
 {
  Border border;
  border.ApplySchemeSettings(&scheme, &first);
  border.ResetSettings();
  assert(0 == g_liveLineArrays);
  for (int i = 0; i < 4; ++i) {
   assert(0 == border._inset[i]);
   assert(0 == border._sides[i].count);
   assert(nullptr == border._sides[i].lines);
  }

  border.ApplySchemeSettings(&scheme, &second);
  int left, top, right, bottom;
  border.GetInset(left, top, right, bottom);
  assert(5 == left && 6 == top && 0 == right && 0 == bottom);
  assert(1 == border._sides[IBorder::SIDE_LEFT].count);
  assert(0 == border._sides[IBorder::SIDE_TOP].count);
  assert(nullptr == border._sides[IBorder::SIDE_TOP].lines);
  assert(1 == g_liveLineArrays);
 }
 assert(0 == g_liveLineArrays);
 puts("PASS: resetting a border drops the previous inset and lines before the next settings apply");
}

int main() {
 ApplyingAgainReplacesTheLinesWithoutLeaking();
 ResetClearsTheInsetAndEverySide();
}
'''

SIGNATURES = (
    'void Border::SetInset(',
    'void Border::GetInset(',
    'void Border::ApplySchemeSettings(',
    'void Border::ResetSettings()',
    'void Border::ParseSideSettings(',
)


def main():
    source = (ROOT / 'vgui2/src/Border.cpp').read_text(encoding='utf-8')
    code = STUBS + ''.join(function(source, signature) + '\n' for signature in SIGNATURES)
    code += TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-border-settings-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
