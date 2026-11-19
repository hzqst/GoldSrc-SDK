"""Exercise the production VGUI2 TextEntry display line count, vertical scroll bar layout and start line with the production ScrollBarSlider value clamping.

Run: python tests/vgui2_textentry_scroll_tests.py
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
#include <vector>

#define DRAW_OFFSET_X 3
#define DRAW_OFFSET_Y 1
#define BUFFER_SIZE 999999

static void Require(bool condition, const char* message) {
 if (condition) return;
 printf("FAIL: %s\n", message);
 exit(1);
}

namespace vgui2 {
typedef int HFont;

// a line is FONT_TALL + DRAW_OFFSET_Y = 10 pixels tall
static const int FONT_TALL = 9;
static const int LINE_TALL = FONT_TALL + DRAW_OFFSET_Y;

struct Surface { int GetFontTall(HFont) { return FONT_TALL; } };
static Surface theSurface;
static Surface* surface() { return &theSurface; }
static int getCharWidth(HFont, wchar_t) { return 8; }

// reading past the last line break, the BUFFER_SIZE sentinel, fails the test like the _WIN32 IsValidIndex check
template <class T> class CUtlVector {
public:
 int Count() const { return (int)items.size(); }
 T& operator[](int i) { Require(i >= 0 && i < Count(), "a line break index passes the last line break"); return items[i]; }
 void AddToTail(T t) { items.push_back(t); }
 std::vector<T> items;
};

// only the members the value, range and window setters use
class ScrollBarSlider {
public:
 void SetValue(int value);
 void SetRange(int minimum, int maximum);
 void SetRangeWindow(int rangeWindow);
 int GetValue() { return _value; }
 int GetRangeWindow() { return _rangeWindow; }
 void RecomputeNobPosFromValue() {}
 void SendScrollBarSliderMovedMessage() {}
 void InvalidateLayout(bool, bool) {}
 int _value = 0;
 int _range[2] = {0, 0};
 int _rangeWindow = 0;
};

// forwards to the slider like ScrollBar
class ScrollBar {
public:
 int GetValue() { return slider.GetValue(); }
 void SetValue(int value) { slider.SetValue(value); }
 void SetRange(int minimum, int maximum) { slider.SetRange(minimum, maximum); }
 int GetRangeWindow() { return slider.GetRangeWindow(); }
 void SetRangeWindow(int window) { slider.SetRangeWindow(window); }
 void SetEnabled(bool state) { enabled = state; }
 void SetButtonPressedScrollValue(int) {}
 void SetPos(int, int) {}
 void SetSize(int, int) {}
 void InvalidateLayout(bool, bool) {}
 void Repaint() {}
 ScrollBarSlider slider;
 bool enabled = false;
};

// only the members the vertical scroll bar layout, the start line and the cursor line use
class TextEntry {
public:
 void LayoutVerticalScrollBarSlider();
 int GetStartDrawIndex(int& lineBreakIndexIndex);
 int GetCursorLine();
 void GetSize(int& wide, int& tall) { wide = GetWide(); tall = height; }
 void GetInset(int& left, int& top, int& right, int& bottom) { left = top = right = bottom = 0; }
 int GetWide() { return 200; }
 int GetTall() { return height; }
 ScrollBar* _vertScrollBar = nullptr;
 int _scrollBarSize = 16;
 HFont _font = 0;
 CUtlVector<wchar_t> m_TextStream;
 CUtlVector<int> m_LineBreaks;
 int _cursorPos = 0;
 bool _putCursorAtEnd = false;
 bool _hideText = false;
 bool _mouseDragSelection = false;
 bool _multiline = true;
 int _currentStartLine = 0;
 int _currentStartIndex = 0;
 bool _horizScrollingAllowed = true;
 int height = 0;
};
'''

TESTS = r'''
}
using vgui2::LINE_TALL;
using vgui2::ScrollBar;
using vgui2::TextEntry;

static const int CHARS_PER_LINE = 4;
static const int LINE_COUNT = 20;
static const int VISIBLE_LINES = 5;

struct Fixture {
 Fixture(int lineCount, bool scrollBar) {
  if (scrollBar) text._vertScrollBar = &bar;
  for (int line = 0; line < lineCount; ++line) {
   for (int i = 0; i < CHARS_PER_LINE; ++i) text.m_TextStream.AddToTail(L'a');
   text.m_LineBreaks.AddToTail(line + 1 < lineCount ? (line + 1) * CHARS_PER_LINE : BUFFER_SIZE);
  }
  // the cursor is at the end of the text, on the last line, as after typing
  text._cursorPos = text.m_TextStream.Count();
 }
 void Layout(int height) {
  text.height = height;
  if (text._vertScrollBar) text.LayoutVerticalScrollBarSlider();
 }
 // the line the text is drawn from, after the paint loop's first read of the line breaks
 int StartLine() {
  int lineBreakIndexIndex = 0;
  int startIndex = text.GetStartDrawIndex(lineBreakIndexIndex);
  (void)text.m_LineBreaks[lineBreakIndexIndex];
  assert((lineBreakIndexIndex ? text.m_LineBreaks[lineBreakIndexIndex - 1] : 0) == startIndex);
  return lineBreakIndexIndex;
 }
 TextEntry text;
 ScrollBar bar;
};

static void ShortHeightsWithAScrollBarStartAtTheCursorLine() {
 for (int shortHeight = 0; shortHeight < LINE_TALL; ++shortHeight) {
  Fixture f(LINE_COUNT, true);
  f.Layout(VISIBLE_LINES * LINE_TALL);
  assert(LINE_COUNT - VISIBLE_LINES == f.StartLine());
  f.Layout(shortHeight);
  assert(f.bar.enabled);
  assert(LINE_COUNT - 1 == f.StartLine());
  f.Layout(VISIBLE_LINES * LINE_TALL);
  assert(LINE_COUNT - VISIBLE_LINES == f.StartLine());
 }
 puts("PASS: with a scroll bar, heights from 0 to below one line start at the cursor line and growing shows the last page");
}

static void ShortHeightsWithoutAScrollBarStartAtTheCursorLine() {
 for (int shortHeight = 0; shortHeight < LINE_TALL; ++shortHeight) {
  Fixture f(LINE_COUNT, false);
  f.text._currentStartLine = LINE_COUNT - VISIBLE_LINES;
  f.Layout(shortHeight);
  assert(LINE_COUNT - 1 == f.StartLine());
  assert(LINE_COUNT - 1 == f.StartLine());
 }
 puts("PASS: without a scroll bar, heights from 0 to below one line start at the cursor line");
}

static void ASingleLineDisablesTheScrollBar() {
 for (int height = 0; height <= LINE_TALL; ++height) {
  Fixture f(1, true);
  f.Layout(height);
  assert(!f.bar.enabled);
  assert(0 == f.StartLine());
 }
 puts("PASS: a single line disables the scroll bar and starts at the first line at any height");
}

int main() {
 ShortHeightsWithAScrollBarStartAtTheCursorLine();
 ShortHeightsWithoutAScrollBarStartAtTheCursorLine();
 ASingleLineDisablesTheScrollBar();
}
'''


def main():
    slider = (ROOT / 'vgui2/controls/ScrollBarSlider.cpp').read_text(encoding='utf-8')
    text_entry = (ROOT / 'vgui2/controls/TextEntry.cpp').read_text(encoding='utf-8')
    code = STUBS + ''.join(function(slider, signature) + '\n' for signature in (
        'void ScrollBarSlider::SetValue( int value )', 'void ScrollBarSlider::SetRange( int minimum, int maximum )',
        'void ScrollBarSlider::SetRangeWindow( int rangeWindow )'))
    code += ''.join(function(text_entry, signature) + '\n' for signature in (
        'static int getDisplayLines( vgui2::HFont font, int tall )',
        'void TextEntry::LayoutVerticalScrollBarSlider()', 'int TextEntry::GetStartDrawIndex( int& lineBreakIndexIndex )',
        'int TextEntry::GetCursorLine()'))
    code += TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-textentry-scroll-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
