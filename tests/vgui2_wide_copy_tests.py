"""Exercise the production VGUI2 wide string copies into fixed buffers with texts that fit, fill or overflow them.

Covers Frame::SetTitle, the ProgressBox constructor, TextImage::GetText, TextEntry::GetText and CLoadingDialog::DisplayGenericError.

Run: python tests/vgui2_wide_copy_tests.py
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
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


def members(header):
    return '\n'.join(re.findall(r'wchar_t\s+m_wsz\w+\[\d+\];', header))


STUBS = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>
#include <utility>
#include <vector>

#define ASSERT_AT( expression, line ) assert( expression )

namespace vgui2 {
typedef int StringIndex_t;
typedef unsigned int VPANEL;

// tokens are registered without the leading '#'; Find accepts both forms
struct Localize {
 std::vector<std::pair<std::string, std::wstring>> strings;
 StringIndex_t FindIndex(const char* name) {
  for (size_t i = 0; i < strings.size(); ++i)
   if (strings[i].first == name) return (StringIndex_t)i;
  return -1;
 }
 const wchar_t* GetValueByIndex(StringIndex_t index) { return strings[index].second.c_str(); }
 wchar_t* Find(const char* name) {
  StringIndex_t index = FindIndex(*name == '#' ? name + 1 : name);
  return index < 0 ? nullptr : (wchar_t*)strings[index].second.c_str();
 }
 // ASCII only, truncated and terminated like the production conversion
 int ConvertANSIToUnicode(const char* ansi, wchar_t* unicode, int bytes) {
  int capacity = bytes / (int)sizeof(wchar_t), written = 0;
  while (ansi[written] && written < capacity - 1) { unicode[written] = (wchar_t)ansi[written]; ++written; }
  unicode[written] = 0;
  return written + 1;
 }
 // reading both strings finds a missing terminator
 void ConstructString(wchar_t* output, int bytes, const wchar_t* format, int, const wchar_t* parameter) {
  lastFormat = format;
  lastParameter = parameter;
  output[0] = 0;
  (void)bytes;
 }
 std::wstring lastFormat, lastParameter;
};
static Localize table;
Localize* localize() { return &table; }

struct Surface {
 void SetTitle(VPANEL, const wchar_t* title) { titles.push_back(title); }
 std::vector<std::wstring> titles;
};
static Surface windows;
Surface* surface() { return &windows; }
typedef Surface ISurface;

class TextImage {
public:
 TextImage(const char*) {}
 void SetText(const char*) {}
 void GetText(wchar_t* buffer, int bufferLength);
 wchar_t* _utext = nullptr;
};

// the text stream TextEntry::GetText reads, it is not terminated like CUtlVector
struct TextStream {
 int Count() const { return (int)text.size(); }
 wchar_t* Base() { return text.data(); }
 std::vector<wchar_t> text;
};

class TextEntry {
public:
 void GetText(wchar_t* wbuf, int bufLen);
 TextStream m_TextStream;
};

class Panel {};
class Label {
public:
 Label(Panel*, const char*, const char*) {}
};

// the members SetTitle and the ProgressBox constructor use
class Frame : public Panel {
public:
 Frame() {}
 Frame(Panel*, const char*, bool, bool) {}
 ~Frame() { delete _title; }
 void SetTitle(const char* title, bool surfaceTitle);
 VPANEL GetVPanel() { return 1; }
 void Repaint() {}
 TextImage* _title = nullptr;
};

class ProgressBox : public Frame {
public:
 ProgressBox(const char* title, const char* info, const char* text, Panel* parent);
 ~ProgressBox() { delete m_pMessageLabel; }
 void Init() {}
 Label* m_pMessageLabel;
 MEMBERS
};
'''

LOADING_STUBS = r'''
}

// the members DisplayGenericError uses
class CLoadingDialog {
public:
 void DisplayGenericError(const char* failureReason, const char* extendedReason);
 void SetupControlSettingsForErrorDisplay(const char*) {}
 struct InfoLabel {
  void SetText(const char*) {}
  void SetText(const wchar_t*) {}
 } infoLabel;
 InfoLabel* m_pInfoLabel = &infoLabel;
 bool m_bShowingVACError = false;
};
'''

TESTS = r'''
using vgui2::Frame;
using vgui2::ProgressBox;
using vgui2::TextImage;
using vgui2::TextEntry;

static const size_t TitleCapacity = 128;

static std::wstring Text(size_t length) {
 std::wstring text;
 for (size_t i = 0; i < length; ++i) text += (wchar_t)(L'A' + i % 26);
 return text;
}

static void Register(const char* name, const std::wstring& value) { vgui2::table.strings.push_back({name, value}); }

static void LocalizedTitlesAreCutBeforeTheLastCharacter() {
 for (size_t length : {127, 128, 129, 300}) {
  std::string token = "Title" + std::to_string(length);
  Register(token.c_str(), Text(length));
  Frame frame;
  vgui2::windows.titles.clear();
  frame.SetTitle(("#" + token).c_str(), true);
  assert(1 == vgui2::windows.titles.size());
  assert(Text(length < TitleCapacity ? length : TitleCapacity - 1) == vgui2::windows.titles[0]);
 }
 puts("PASS: localized titles of 127, 128, 129 and 300 characters reach the surface terminated, cut to 127");
}

static void NarrowAndMissingTitlesKeepTheirBehaviour() {
 Frame frame;
 vgui2::windows.titles.clear();
 frame.SetTitle(std::string(300, 'n').c_str(), true);
 frame.SetTitle("#NoSuchToken", true);
 frame.SetTitle("short", true);
 assert(3 == vgui2::windows.titles.size());
 assert(std::wstring(TitleCapacity - 1, L'n') == vgui2::windows.titles[0]);
 assert(L"" == vgui2::windows.titles[1]);
 assert(L"short" == vgui2::windows.titles[2]);
 puts("PASS: narrow titles are cut to 127, unknown tokens give an empty title and short titles are kept");
}

static void TitlesWithoutTheSurfaceAreNotSent() {
 Register("Hidden", Text(200));
 Frame frame;
 vgui2::windows.titles.clear();
 frame.SetTitle("#Hidden", false);
 assert(vgui2::windows.titles.empty());
 puts("PASS: a localized title without surfaceTitle is not sent to the surface");
}

static bool Terminated(const wchar_t* buffer, size_t capacity) { return std::wmemchr(buffer, 0, capacity) != nullptr; }

static void ProgressBoxTextsAreCutBeforeTheLastCharacter() {
 Register("BoxTitle", Text(128));
 Register("BoxInfo", Text(129));
 Register("BoxText", Text(300));
 ProgressBox box("#BoxTitle", "#BoxInfo", "#BoxText", nullptr);
 const size_t capacity = sizeof(box.m_wszTitle) / sizeof(box.m_wszTitle[0]);
 for (const wchar_t* buffer : {box.m_wszTitle, box.m_wszInfo, box.m_wszText}) {
  assert(Terminated(buffer, capacity));
  assert(Text(capacity - 1) == buffer);
 }
 Register("Short", L"short");
 ProgressBox small("#Short", "#Missing", "#Short", nullptr);
 assert(std::wstring(L"short") == small.m_wszTitle);
 assert(std::wstring(L"") == small.m_wszInfo);
 assert(std::wstring(L"short") == small.m_wszText);
 puts("PASS: ProgressBox title, info and text of 128, 129 and 300 characters are cut to 127 and terminated");
}

static void TextImageGetTextTerminatesTheBuffer() {
 wchar_t text[] = L"abcdefghij";
 TextImage image("");
 image._utext = text;
 wchar_t buffer[16];
 std::wmemset(buffer, L'x', 16);
 image.GetText(buffer, 4);
 assert(std::wstring(L"abc") == buffer);
 std::wmemset(buffer, L'x', 16);
 image.GetText(buffer, 10);
 assert(std::wstring(L"abcdefghi") == buffer);
 std::wmemset(buffer, L'x', 16);
 image.GetText(buffer, 11);
 assert(std::wstring(L"abcdefghij") == buffer);
 std::wmemset(buffer, L'x', 16);
 image.GetText(buffer, 1);
 assert(L'\0' == buffer[0]);
 std::wmemset(buffer, L'x', 16);
 image.GetText(buffer, 0);
 assert(L'x' == buffer[0]);
 puts("PASS: TextImage::GetText cuts at the buffer, terminates it and leaves an empty buffer alone");
}

static void TextEntryGetTextTerminatesTheBuffer() {
 TextEntry entry;
 const wchar_t text[] = L"abcdefghij";
 entry.m_TextStream.text.assign(text, text + 10);
 wchar_t buffer[16];
 std::wmemset(buffer, L'x', 16);
 entry.GetText(buffer, 4);
 assert(std::wstring(L"abc") == buffer);
 std::wmemset(buffer, L'x', 16);
 entry.GetText(buffer, 10);
 assert(std::wstring(L"abcdefghi") == buffer);
 std::wmemset(buffer, L'x', 16);
 entry.GetText(buffer, 11);
 assert(std::wstring(L"abcdefghij") == buffer);
 std::wmemset(buffer, L'x', 16);
 entry.GetText(buffer, 1);
 assert(L'\0' == buffer[0]);
 assert(L'x' == buffer[1]);
 std::wmemset(buffer, L'x', 16);
 entry.GetText(buffer + 1, 0);
 assert(L'x' == buffer[0]);
 assert(L'x' == buffer[1]);
 entry.GetText(buffer + 1, -1);
 assert(L'x' == buffer[0]);
 assert(L'x' == buffer[1]);
 entry.GetText(nullptr, 16);
 TextEntry empty;
 empty.GetText(buffer + 1, 0);
 assert(L'x' == buffer[1]);
 empty.GetText(buffer, 16);
 assert(L'\0' == buffer[0]);
 puts("PASS: TextEntry::GetText cuts at the buffer, terminates it and leaves an empty or missing buffer alone");
}

static void LoadingErrorsAreCutBeforeTheLastCharacter() {
 const size_t capacity = 256;
 Register("LongReason", Text(300));
 Register("LongFormat", Text(256));
 CLoadingDialog dialog;
 dialog.DisplayGenericError("#LongFormat", "#LongReason");
 assert(Text(capacity - 1) == vgui2::table.lastFormat);
 assert(Text(capacity - 1) == vgui2::table.lastParameter);
 Register("FitReason", Text(255));
 dialog.DisplayGenericError("plain %s1", "#FitReason");
 assert(L"plain %s1" == vgui2::table.lastFormat);
 assert(Text(capacity - 1) == vgui2::table.lastParameter);
 puts("PASS: loading error reason and format of 255, 256 and 300 characters are cut to 255 and terminated");
}

int main() {
 LocalizedTitlesAreCutBeforeTheLastCharacter();
 NarrowAndMissingTitlesKeepTheirBehaviour();
 TitlesWithoutTheSurfaceAreNotSent();
 ProgressBoxTextsAreCutBeforeTheLastCharacter();
 TextImageGetTextTerminatesTheBuffer();
 TextEntryGetTextTerminatesTheBuffer();
 LoadingErrorsAreCutBeforeTheLastCharacter();
}
'''


def main():
    controls = ROOT / 'vgui2/controls'
    frame = (controls / 'Frame.cpp').read_text(encoding='utf-8')
    box = (controls / 'ProgressBox.cpp').read_text(encoding='utf-8')
    image = (controls / 'TextImage.cpp').read_text(encoding='utf-8')
    entry = (controls / 'TextEntry.cpp').read_text(encoding='utf-8')
    loading = (ROOT / 'GameUI/LoadingDialog.cpp').read_text(encoding='utf-8')
    code = STUBS.replace('MEMBERS', members((controls / 'ProgressBox.h').read_text(encoding='utf-8')))
    code += function(frame, 'void Frame::SetTitle( const char* title, bool surfaceTitle )') + '\n'
    code += function(box, 'ProgressBox::ProgressBox( const char* title, const char* info, const char* text, Panel* parent )') + '\n'
    code += function(image, 'void TextImage::GetText( wchar_t* buffer, int bufferLength )') + '\n'
    code += function(entry, 'void TextEntry::GetText( wchar_t* wbuf, int bufLen )') + '\n'
    code += LOADING_STUBS
    code += function(loading, 'void CLoadingDialog::DisplayGenericError( const char* failureReason, const char* extendedReason )') + '\n'
    code += TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-wide-copy-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
