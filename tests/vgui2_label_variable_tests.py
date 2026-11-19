"""Exercise the production VGUI2 Label::ApplySettings variable labelText handling with variables that fit, fill or overflow the token buffer, and the labelText Label::GetSettings saves back.

ApplySettings covers both the Windows legacy _snprintf, which leaves a filled buffer unterminated and returns -1 when it truncates, and C99 snprintf.

Run: python tests/vgui2_label_variable_tests.py
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
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <map>
#include <string>
#include <vector>

#if !defined( _WIN32 )
#include <strings.h>
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#endif

// the Windows legacy _snprintf or C99 snprintf, picked per test
static bool legacySnprintf = false;
#define _snprintf TestSnprintf
static int TestSnprintf(char* buffer, size_t count, const char* format, ...) {
 char full[4096];
 va_list arguments;
 va_start(arguments, format);
 int length = vsnprintf(full, sizeof(full), format, arguments);
 va_end(arguments);
 if (!legacySnprintf) {
  snprintf(buffer, count, "%s", full);
  return length;
 }
 if ((size_t)length > count) {
  memcpy(buffer, full, count);
  return -1;
 }
 // a result of exactly count characters is not terminated
 memcpy(buffer, full, (size_t)length == count ? count : length + 1);
 return length;
}

namespace vgui2 {
typedef int HScheme;
typedef int HFont;
typedef int StringIndex_t;
static const StringIndex_t INVALID_STRING_INDEX = -1;

// AddString replaces the value of a name that is already registered like the production table
struct Localize {
 std::vector<std::pair<std::string, std::wstring>> strings;
 int adds = 0;
 StringIndex_t FindIndex(const char* name) {
  for (size_t i = 0; i < strings.size(); ++i)
   if (strings[i].first == name) return (StringIndex_t)i;
  return INVALID_STRING_INDEX;
 }
 const char* GetNameByIndex(StringIndex_t index) { return strings[index].first.c_str(); }
 const wchar_t* GetValueByIndex(StringIndex_t index) { return strings[index].second.c_str(); }
 std::wstring Value(const std::string& name) {
  StringIndex_t index = FindIndex(name.c_str());
  return index == INVALID_STRING_INDEX ? L"" : strings[index].second;
 }
 // ASCII only, cut and terminated like the production conversions
 int ConvertUnicodeToANSI(const wchar_t* unicode, char* ansi, int bytes) {
  int written = 0;
  while (unicode[written] && written < bytes - 1) { ansi[written] = (char)unicode[written]; ++written; }
  ansi[written] = 0;
  return written + 1;
 }
 int ConvertANSIToUnicode(const char* ansi, wchar_t* unicode, int bytes) {
  int capacity = bytes / (int)sizeof(wchar_t), written = 0;
  while (ansi[written] && written < capacity - 1) { unicode[written] = (wchar_t)ansi[written]; ++written; }
  unicode[written] = 0;
  return written + 1;
 }
 void AddString(const char* name, wchar_t* value, const char*) {
  StringIndex_t index = FindIndex(name);
  if (index == INVALID_STRING_INDEX) strings.push_back({name, value});
  else strings[index].second = value;
  ++adds;
 }
};
static Localize table;
Localize* localize() { return &table; }

struct KeyValues {
 std::map<std::string, std::string> values;
 const char* GetString(const char* name, const char* defaultValue) {
  auto found = values.find(name);
  return found == values.end() ? defaultValue : found->second.c_str();
 }
 int GetInt(const char*, int defaultValue) { return defaultValue; }
 void SetString(const char* name, const char* value) { values[name] = value; }
 void SetInt(const char*, int) {}
};

struct IScheme { HFont GetFontEx(const char*, bool, bool) { return 0; } };
struct ISchemeManager {
 IScheme fonts;
 IScheme* GetIScheme(HScheme) { return &fonts; }
};
static ISchemeManager schemes;
ISchemeManager* scheme() { return &schemes; }

// only the members the labelText round trip uses
class TextImage {
public:
 ~TextImage() { delete[] _utext; }
 void SetText(const char* text);
 void SetText(const wchar_t* text) {
  delete[] _utext;
  _utext = new wchar_t[wcslen(text) + 1];
  wcscpy(_utext, text);
 }
 void GetText(char* buffer, int bufferSize);
 void GetUnlocalizedText(char* buffer, int bufferSize);
 void SetWrap(bool) {}
 wchar_t* _utext = nullptr;
 StringIndex_t _unlocalizedTextSymbol = INVALID_STRING_INDEX;
};

class Panel {
public:
 void ApplySettings(KeyValues*) {}
 void GetSettings(KeyValues*) {}
 const char* GetName() { return "panel"; }
};

// only the members ApplySettings and GetSettings use
class Label : public Panel {
public:
 enum Alignment { a_northwest, a_north, a_northeast, a_west, a_center, a_east, a_southwest, a_south, a_southeast };
 enum EColorState { CS_NORMAL, CS_DULL, CS_BRIGHT };
 ~Label() { delete[] _associateName; delete[] _fontOverrideName; }
 void ApplySettings(KeyValues* settings);
 void GetSettings(KeyValues* settings);
 void SetText(const char* text) {
  this->text = text;
  _textImage->SetText(text);
 }
 void SetContentAlignment(Alignment) {}
 void SetTextColorState(EColorState) {}
 HScheme GetScheme() { return 0; }
 bool IsHDProportional() { return false; }
 bool IsProportional() { return false; }
 void SetFont(HFont) {}
 void InvalidateLayout(bool, bool) {}
 Alignment _contentAlignment = a_west;
 EColorState _textColorState = CS_NORMAL;
 Panel* _associate = nullptr;
 char* _associateName = nullptr;
 char* _fontOverrideName = nullptr;
 bool m_bWrap = false;
 TextImage image;
 TextImage* _textImage = &image;
 std::string text;
};
'''

TESTS = r'''
using vgui2::KeyValues;
using vgui2::Label;

static const size_t TokenCapacity = 256;
static const size_t TokenPrefixLength = 5;
// TextImage::SetText converts labelText into wchar_t[1024]
static const size_t TextImageCapacity = 1024;

// a %...% variable of length characters
static std::string Variable(size_t length, char last = 'A') {
 std::string text = "%";
 for (size_t i = 0; i + 3 < length; ++i) text += (char)('A' + i % 26);
 return text + last + "%";
}

static std::string Apply(const std::string& labelText) {
 KeyValues settings;
 settings.values["labelText"] = labelText;
 Label label;
 label.ApplySettings(&settings);
 return label.text;
}

// the labelText GetSettings saves after ApplySettings loaded labelText
static std::string RoundTrip(const std::string& labelText) {
 KeyValues settings;
 settings.values["labelText"] = labelText;
 Label label;
 label.ApplySettings(&settings);
 KeyValues saved;
 label.GetSettings(&saved);
 return saved.values["labelText"];
}

static void Reset() {
 vgui2::table.strings.clear();
 vgui2::table.adds = 0;
}

static void VariablesThatFitAreRegistered(const char* mode) {
 for (size_t length : {(size_t)3, (size_t)100, TokenCapacity - TokenPrefixLength - 1}) {
  Reset();
  std::string text = Variable(length);
  assert("#var_" + text == Apply(text));
  assert(1 == vgui2::table.adds);
  assert(std::wstring(text.begin(), text.end()) == vgui2::table.Value("var_" + text));
 }
 printf("PASS: %s variables of 3, 100 and 250 characters are registered as var_ tokens\n", mode);
}

static void VariablesThatDoNotFitArePlainText(const char* mode) {
 for (size_t length : {TokenCapacity - TokenPrefixLength, TokenCapacity - TokenPrefixLength + 1, (size_t)300}) {
  Reset();
  std::string text = Variable(length);
  assert(text == Apply(text));
  assert(0 == vgui2::table.adds);
 }
 printf("PASS: %s variables of 251, 252 and 300 characters are shown as plain text and not registered\n", mode);
}

static void LongVariablesDoNotCollide(const char* mode) {
 Reset();
 std::string first = Variable(260, 'X');
 std::string second = Variable(260, 'Y');
 assert(first == Apply(first));
 assert(second == Apply(second));
 assert(0 == vgui2::table.adds);
 printf("PASS: %s variables that share their first 250 characters keep their own text\n", mode);
}

static void PlainTextIsKept(const char* mode) {
 Reset();
 assert("plain" == Apply("plain"));
 printf("PASS: %s plain labelText is set as is\n", mode);
}

static void SavedLabelTextIsKept() {
 for (size_t length : {(size_t)3, TokenCapacity - TokenPrefixLength - 1, TokenCapacity - TokenPrefixLength, (size_t)300,
       TextImageCapacity - 1}) {
  Reset();
  std::string variable = Variable(length);
  assert(variable == RoundTrip(variable));
  std::string plain = Variable(length + 1, 'P').substr(1);
  assert(plain == RoundTrip(plain));
 }
 puts("PASS: variables and plain labelText of 3, 250, 251, 300 and 1023 characters are saved back unchanged");
}

static void TextBeyondTheTextImageIsCutOnLoad() {
 Reset();
 std::string plain = Variable(TextImageCapacity + 101, 'P').substr(1);
 assert(plain.substr(0, TextImageCapacity - 1) == RoundTrip(plain));
 puts("PASS: plain labelText longer than 1023 characters is saved as the 1023 characters TextImage keeps");
}

int main() {
 for (bool legacy : {true, false}) {
  legacySnprintf = legacy;
  const char* mode = legacy ? "legacy _snprintf:" : "C99 snprintf:";
  VariablesThatFitAreRegistered(mode);
  VariablesThatDoNotFitArePlainText(mode);
  LongVariablesDoNotCollide(mode);
  PlainTextIsKept(mode);
 }
 SavedLabelTextIsKept();
 TextBeyondTheTextImageIsCutOnLoad();
}
'''


def main():
    label = (ROOT / 'vgui2/controls/Label.cpp').read_text(encoding='utf-8')
    image = (ROOT / 'vgui2/controls/TextImage.cpp').read_text(encoding='utf-8')
    code = STUBS
    code += function(image, 'void TextImage::SetText( const char* text )') + '\n'
    code += function(image, 'void TextImage::GetText( char* buffer, int bufferSize )') + '\n'
    code += function(image, 'void TextImage::GetUnlocalizedText( char* buffer, int bufferSize )') + '\n'
    code += function(label, 'void Label::ApplySettings( KeyValues* settings )') + '\n'
    code += function(label, 'void Label::GetSettings( KeyValues* settings )') + '\n'
    code += '}\n'
    code += TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-label-variable-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
