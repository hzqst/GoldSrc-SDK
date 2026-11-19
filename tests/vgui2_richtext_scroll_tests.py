"""Exercise the production VGUI2 RichText vertical scroll bar layout with the production ScrollBarSlider value clamping.

Run: python tests/vgui2_richtext_scroll_tests.py
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

namespace vgui2 {
typedef int HFont;

// a line is FONT_TALL + DRAW_OFFSET_Y = 10 pixels tall
static const int FONT_TALL = 9;
static const int DRAW_OFFSET_Y = 1;
static const int LINE_TALL = FONT_TALL + DRAW_OFFSET_Y;

struct Surface { int GetFontTall(HFont) { return FONT_TALL; } };
static Surface theSurface;
static Surface* surface() { return &theSurface; }

// only the members the value, range and window setters use
class ScrollBarSlider {
public:
 void SetValue(int value);
 void SetRange(int minimum, int maximum);
 void SetRangeWindow(int rangeWindow);
 int GetValue() { return _value; }
 void GetRange(int& minimum, int& maximum) { minimum = _range[0]; maximum = _range[1]; }
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
 bool IsVisible() { return true; }
 int GetValue() { return slider.GetValue(); }
 void SetValue(int value) { slider.SetValue(value); }
 void GetRange(int& minimum, int& maximum) { slider.GetRange(minimum, maximum); }
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

struct LineBreaks {
 int Count() { return count; }
 int count = 0;
};

// only the members the vertical scroll bar layout uses
class RichText {
public:
 void LayoutVerticalScrollBarSlider();
 void GetSize(int& wide, int& tall) { wide = 200; tall = height; }
 void GetInset(int& left, int& top, int& right, int& bottom) { left = top = right = bottom = 0; }
 bool _invalidateVerticalScrollbarSlider = false;
 ScrollBar* _vertScrollBar = nullptr;
 int _scrollBarSize = 16;
 int _drawOffsetY = DRAW_OFFSET_Y;
 HFont _font = 0;
 LineBreaks m_LineBreaks;
 int height = 0;
};
'''

TESTS = r'''
}
using vgui2::LINE_TALL;
using vgui2::RichText;
using vgui2::ScrollBar;

static const int LINE_COUNT = 20;
static const int VISIBLE_LINES = 5;

struct Fixture {
 explicit Fixture(int lineCount) {
  text._vertScrollBar = &scrollBar;
  text.m_LineBreaks.count = lineCount;
 }
 void Layout(int height) {
  text.height = height;
  text.LayoutVerticalScrollBarSlider();
 }
 int Value() { return scrollBar.GetValue(); }
 RichText text;
 ScrollBar scrollBar;
};

static void ShrinkingBelowOneLineKeepsAMiddlePosition() {
 for (int shortHeight = 1; shortHeight < LINE_TALL; ++shortHeight) {
  Fixture f(LINE_COUNT);
  f.Layout(VISIBLE_LINES * LINE_TALL);
  f.scrollBar.SetValue(7);
  f.Layout(shortHeight);
  assert(f.scrollBar.enabled);
  assert(7 == f.Value());
  f.Layout(VISIBLE_LINES * LINE_TALL);
  assert(f.scrollBar.enabled);
  assert(7 == f.Value());
 }
 puts("PASS: shrinking below one line and growing again keeps a position in the middle of the text");
}

static void ShrinkingBelowOneLineKeepsTheBottom() {
 Fixture f(LINE_COUNT);
 f.Layout(VISIBLE_LINES * LINE_TALL);
 f.scrollBar.SetValue(LINE_COUNT);
 assert(LINE_COUNT - VISIBLE_LINES == f.Value());
 f.Layout(LINE_TALL / 2);
 assert(LINE_COUNT - 1 == f.Value());
 f.Layout(VISIBLE_LINES * LINE_TALL);
 assert(LINE_COUNT - VISIBLE_LINES == f.Value());
 puts("PASS: shrinking below one line and growing again stays at the bottom");
}

static void TextThatFitsFollowsTheBottomWhenItGrows() {
 Fixture f(3);
 f.Layout(VISIBLE_LINES * LINE_TALL);
 assert(!f.scrollBar.enabled);
 assert(0 == f.Value());
 f.text.m_LineBreaks.count = LINE_COUNT;
 f.Layout(VISIBLE_LINES * LINE_TALL);
 assert(f.scrollBar.enabled);
 assert(LINE_COUNT - VISIBLE_LINES == f.Value());
 puts("PASS: text that fits disables the scroll bar and follows the bottom once it grows past the window");
}

static void ShortHeightsWithOneLineOrNoHeightDisableTheScrollBar() {
 Fixture single(1);
 single.Layout(LINE_TALL / 2);
 assert(!single.scrollBar.enabled);
 assert(0 == single.Value());

 Fixture hidden(LINE_COUNT);
 hidden.Layout(VISIBLE_LINES * LINE_TALL);
 hidden.scrollBar.SetValue(7);
 hidden.Layout(0);
 assert(!hidden.scrollBar.enabled);
 assert(0 == hidden.Value());
 puts("PASS: a single line below one line height and a zero height still disable the scroll bar and reset it");
}

int main() {
 ShrinkingBelowOneLineKeepsAMiddlePosition();
 ShrinkingBelowOneLineKeepsTheBottom();
 TextThatFitsFollowsTheBottomWhenItGrows();
 ShortHeightsWithOneLineOrNoHeightDisableTheScrollBar();
}
'''


def main():
    slider = (ROOT / 'vgui2/controls/ScrollBarSlider.cpp').read_text(encoding='utf-8')
    rich_text = (ROOT / 'vgui2/controls/RichText.cpp').read_text(encoding='utf-8')
    code = STUBS + ''.join(function(slider, signature) + '\n' for signature in (
        'void ScrollBarSlider::SetValue( int value )', 'void ScrollBarSlider::SetRange( int minimum, int maximum )',
        'void ScrollBarSlider::SetRangeWindow( int rangeWindow )'))
    code += function(rich_text, 'void RichText::LayoutVerticalScrollBarSlider()') + '\n'
    code += TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-richtext-scroll-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
