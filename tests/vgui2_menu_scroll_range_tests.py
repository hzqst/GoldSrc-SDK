"""Exercise the production VGUI2 Menu scroll range layout, AddMenuItem and DeleteAllItems with containers that reject invalid item IDs.

Run: python tests/vgui2_menu_scroll_range_tests.py
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
# a layout that changes more item visibilities than this has looped forever
VISIBILITY_CHANGE_LIMIT = 100000
TIMEOUT_SECONDS = 60
# the scroll range is only laid out up to here, the rest of PerformLayout sizes and places the items
LAYOUT_END = 'LayoutMenuBorder();'


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


def function_prefix(source, signature, end):
    start = source.index(signature)
    stop = source.index(end, start) + len(end)
    if source.find('\n}', start) < stop:
        raise ValueError(f'{end} is not in {signature}')
    return source[start:stop] + '\n}\n'


STUBS = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>

#define ASSERT_AT( expression, line ) assert( expression )

static int visibilityChanges = 0;

static void Require(bool condition, const char* message) {
 if (condition) return;
 printf("FAIL: %s\n", message);
 exit(1);
}

namespace vgui2 {
typedef int HFont;

struct TextImage { void SetUseFallbackFont(bool, HFont) {} };

class MenuItem {
public:
 void SetParent(const void*) {}
 void SetFont(HFont) {}
 TextImage* GetTextImage() { return nullptr; }
 void MarkForDeletion() { deleted = true; }
 void SetVisible(bool state) {
  Require(++visibilityChanges <= VISIBILITY_CHANGE_LIMIT, "the scroll range layout does not end");
  visible = state;
 }
 bool visible = false;
 bool deleted = false;
};

// IDs are reused from 0 after RemoveAll like CUtlLinkedList, reading a free or out of range slot fails the test
template <class T, class I> class CUtlLinkedList {
public:
 I InvalidIndex() const { return -1; }
 bool IsValidIndex(I i) const { return i >= 0 && i < (I)used.size() && used[i]; }
 I Head() const { return Next(-1); }
 I Next(I i) const {
  for (++i; i < (I)used.size(); ++i)
   if (used[i]) return i;
  return -1;
 }
 int Count() const { int count = 0; for (bool u : used) count += u; return count; }
 T& operator[](I i) { Require(IsValidIndex(i), "a sorted item ID is not a used menu item slot"); return elements[i]; }
 I AddToTail(T t) {
  for (I i = 0; i < (I)used.size(); ++i)
   if (!used[i]) { used[i] = true; elements[i] = t; return i; }
  used.push_back(true);
  elements.push_back(t);
  return (I)used.size() - 1;
 }
 void RemoveAll() { for (size_t i = 0; i < used.size(); ++i) used[i] = false; }
private:
 std::vector<T> elements;
 std::vector<bool> used;
};

template <class T> class CUtlVector {
public:
 int Count() const { return (int)items.size(); }
 T& operator[](int i) { Require(i >= 0 && i < Count(), "a sorted item row is out of range"); return items[i]; }
 void AddToTail(T t) { items.push_back(t); }
 void RemoveAll() { items.clear(); }
 std::vector<T> items;
};

struct ScrollBar {
 int GetValue() { return value; }
 int value = 0;
};

// only the members the scroll range layout, AddMenuItem and DeleteAllItems use
class Menu {
public:
 void MakeItemsVisibleInScrollRange();
 void DeleteAllItems();
 int AddMenuItem(MenuItem* item);
 void PerformLayout();
 void InvalidateLayout(bool, bool) {}
 void AddScrollBar() { scrollBar = true; }
 void RemoveScrollBar() { scrollBar = false; }
 void LayoutMenuBorder() {}
 int m_iNumVisibleLines = 0;
 ScrollBar* m_pScroller = nullptr;
 CUtlLinkedList<MenuItem*, int> m_MenuItems;
 CUtlVector<int> m_SortedItems;
 bool _recalculateWidth = false;
 bool m_bUseFallbackFont = false;
 HFont m_hItemFont = 0;
 HFont m_hFallbackItemFont = 0;
 int m_iCurrentlySelectedItemID = -1;
 int m_iActivatedItem = -1;
 bool scrollBar = false;
};
'''

TESTS = r'''
}
using vgui2::Menu;
using vgui2::MenuItem;

struct Fixture {
 explicit Fixture(int visibleLines) { menu.m_pScroller = &scroller; menu.m_iNumVisibleLines = visibleLines; }
 void Fill(int count) {
  for (int i = 0; i < count; ++i) {
   items.push_back(new MenuItem);
   menu.AddMenuItem(items.back());
  }
 }
 // the rows of the items added since the last DeleteAllItems that are visible, in order
 std::vector<int> VisibleRows(size_t first) {
  std::vector<int> rows;
  for (size_t i = first; i < items.size(); ++i)
   if (items[i]->visible) rows.push_back((int)(i - first));
  return rows;
 }
 ~Fixture() { for (MenuItem* item : items) delete item; }
 Menu menu;
 vgui2::ScrollBar scroller;
 std::vector<MenuItem*> items;
};

static std::vector<int> Rows(int first, int count) {
 std::vector<int> rows;
 for (int i = 0; i < count; ++i) rows.push_back(first + i);
 return rows;
}

static void MenusThatFitDoNotLayOutTheScrollRange() {
 for (int count = 0; count <= 4; ++count) {
  for (int visibleLines = count; visibleLines <= count + 2; ++visibleLines) {
   Fixture f(visibleLines);
   f.Fill(count);
   f.menu.scrollBar = true;
   visibilityChanges = 0;
   f.menu.PerformLayout();
   Require(!f.menu.scrollBar, "a menu that fits keeps its scroll bar");
   Require(0 == visibilityChanges, "a menu that fits lays out the scroll range");
  }
 }
 puts("PASS: empty menus and menus with no more items than visible lines remove the scroll bar and skip the scroll range");
}

static void ClearingAndRebuildingKeepsTheSortedIDsValid() {
 Fixture f(3);
 f.Fill(6);
 f.scroller.value = 3;
 f.menu.PerformLayout();
 assert(Rows(3, 3) == f.VisibleRows(0));

 f.menu.DeleteAllItems();
 assert(0 == f.menu.m_MenuItems.Count());
 assert(0 == f.menu.m_SortedItems.Count());
 for (int i = 0; i < 6; ++i) assert(f.items[i]->deleted);
 f.menu.PerformLayout();
 Require(!f.menu.scrollBar, "a cleared menu keeps its scroll bar");

 // the rebuilt menu reuses the IDs from 0 and is shorter than the old scroll value plus the visible lines
 f.Fill(5);
 for (int row = 0; row < 5; ++row) assert(row == f.menu.m_SortedItems[row]);
 f.menu.PerformLayout();
 Require(f.menu.scrollBar, "a rebuilt menu with more items than visible lines has no scroll bar");
 assert(Rows(2, 3) == f.VisibleRows(6));
 puts("PASS: clearing and rebuilding the menu reuses valid IDs and the stale scroll value shows the last page");
}

static void TheLastPageStartsNoLowerThanCountMinusVisibleLines() {
 for (int count = 2; count <= 8; ++count) {
  for (int visibleLines = 1; visibleLines < count; ++visibleLines) {
   for (int start = 0; start < count; ++start) {
    Fixture f(visibleLines);
    f.Fill(count);
    f.scroller.value = start;
    visibilityChanges = 0;
    f.menu.PerformLayout();
    int first = start < count - visibleLines ? start : count - visibleLines;
    Require(Rows(first, visibleLines) == f.VisibleRows(0), "the scroll range shows the wrong rows");
   }
  }
 }
 puts("PASS: scrolling into the last page moves the start back to the count minus the visible lines and no further");
}

int main() {
 MenusThatFitDoNotLayOutTheScrollRange();
 ClearingAndRebuildingKeepsTheSortedIDsValid();
 TheLastPageStartsNoLowerThanCountMinusVisibleLines();
}
'''


def main():
    source = (ROOT / 'vgui2/controls/Menu.cpp').read_text(encoding='utf-8')
    code = STUBS.replace('VISIBILITY_CHANGE_LIMIT', str(VISIBILITY_CHANGE_LIMIT))
    code += ''.join(function(source, signature) + '\n' for signature in (
        'void Menu::MakeItemsVisibleInScrollRange()', 'void Menu::DeleteAllItems()',
        'int Menu::AddMenuItem( MenuItem* item )'))
    code += function_prefix(source, 'void Menu::PerformLayout()', LAYOUT_END)
    code += TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-menu-scroll-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder, timeout=TIMEOUT_SECONDS)


if __name__ == '__main__':
    main()
