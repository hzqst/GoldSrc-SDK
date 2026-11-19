"""Exercise the production VGUI2 ComboBox character and arrow key navigation, the dropdown highlight and Menu::DeleteAllItems on stub menu items.

Run: python tests/vgui2_combobox_key_tests.py
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
# a search that reads more item texts than this has looped forever
TEXT_READ_LIMIT = 1000
TIMEOUT_SECONDS = 60


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <cwctype>
#include <string>
#include <vector>

static int textReads = 0;

namespace vgui2 {
class MenuItem {
public:
 explicit MenuItem(const wchar_t* text) : text(text) {}
 void GetText(wchar_t* buffer, int size) {
  if (++textReads > TEXT_READ_LIMIT) {
   puts("FAIL: the item search does not end");
   exit(1);
  }
  wcsncpy(buffer, text.c_str(), size - 1);
  buffer[size - 1] = 0;
 }
 bool IsEnabled() { return true; }
 void DoClick() { ++clicks; }
 void MarkForDeletion() { deleted = true; }
 std::wstring text;
 int clicks = 0;
 bool deleted = false;
};

// IDs are reused from 0 after RemoveAll like CUtlLinkedList, items are owned by the test
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
 T& operator[](I i) { return elements[i]; }
 T& Element(I i) { return elements[i]; }
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
 bool IsValidIndex(int i) const { return i >= 0 && i < Count(); }
 T& operator[](int i) { return items[i]; }
 T& Element(int i) { return items[i]; }
 void AddToTail(T t) { items.push_back(t); }
 void RemoveAll() { items.clear(); }
 std::vector<T> items;
};

// only the members the navigation and DeleteAllItems use
class Menu {
public:
 void DeleteAllItems();
 int GetItemCount();
 int GetMenuID(int index);
 MenuItem* GetMenuItem(int itemID);
 void ActivateItem(int itemID);
 void ActivateItemByRow(int row);
 int GetActiveItem();
 void InvalidateLayout(bool, bool) {}
 void SetCurrentlyHighlightedItem(int itemID) { highlighted = itemID; }
 bool IsVisible() { return false; }
 bool IsEnabled() { return true; }
 void PerformLayout() {}
 void SetVisible(bool) {}
 void RequestFocus(int) {}
 int AddItem(MenuItem* item) {
  int itemID = m_MenuItems.AddToTail(item);
  m_SortedItems.AddToTail(itemID);
  return itemID;
 }
 CUtlLinkedList<MenuItem*, int> m_MenuItems;
 CUtlVector<int> m_SortedItems;
 int m_iCurrentlySelectedItemID = -1;
 int m_iActivatedItem = 0;
 int highlighted = -1;
};

struct ComboBoxButton { void SetArmed(bool) {} };

class TextEntry {
public:
 void OnKeyTyped(wchar_t unichar) { typed.push_back(unichar); }
 std::wstring typed;
};

class ComboBox : public TextEntry {
public:
 void OnKeyTyped(wchar_t unichar);
 void MoveAlongMenuItemList(int direction);
 void DoClick();
 bool IsEditable() { return false; }
 void OnSetText(const wchar_t* value) { text = value; }
 void GetText(wchar_t* buffer, int size) {
  wcsncpy(buffer, text.c_str(), size - 1);
  buffer[size - 1] = 0;
 }
 void SelectAllText(bool) {}
 void SelectNoText() {}
 void HideMenu() {}
 void DoMenuLayout() {}
 void MoveToFront() {}
 void OnShowMenu(Menu*) {}
 void Repaint() {}
 Menu* m_pDropDown = nullptr;
 ComboBoxButton* m_pButton = nullptr;
 std::wstring text;
};
'''

TESTS = r'''
}
using vgui2::ComboBox;
using vgui2::Menu;
using vgui2::MenuItem;

struct Fixture {
 Fixture() { box.m_pDropDown = &menu; box.m_pButton = &button; }
 void Fill(const std::vector<const wchar_t*>& texts) {
  for (const wchar_t* text : texts) {
   items.push_back(new MenuItem(text));
   menu.AddItem(items.back());
  }
 }
 ~Fixture() { for (MenuItem* item : items) delete item; }
 Menu menu;
 vgui2::ComboBoxButton button;
 ComboBox box;
 std::vector<MenuItem*> items;
};

static void ClearingTheMenuResetsItsState() {
 Fixture f;
 f.Fill({L"a", L"b", L"c", L"d", L"e", L"f"});
 f.menu.ActivateItem(5);
 f.menu.m_iCurrentlySelectedItemID = 5;
 f.menu.DeleteAllItems();
 assert(-1 == f.menu.GetActiveItem());
 assert(-1 == f.menu.m_iCurrentlySelectedItemID);
 assert(f.items[0]->deleted && f.items[5]->deleted);
 puts("PASS: DeleteAllItems clears the active and highlighted item");
}

static void TypingIntoAnEmptyMenuPassesTheKeyOn() {
 Fixture f;
 f.Fill({L"a", L"b", L"c", L"d", L"e", L"f"});
 f.menu.ActivateItem(5);
 f.menu.DeleteAllItems();
 f.box.OnKeyTyped(L'x');
 assert(std::wstring(L"x") == f.box.typed);
 assert(std::wstring() == f.box.text);
 puts("PASS: typing into a cleared menu passes the key on");
}

static void TypingAnUnmatchedCharacterAfterShrinkingEnds() {
 Fixture f;
 f.Fill({L"a", L"b", L"c", L"d", L"e", L"f"});
 f.menu.ActivateItem(5);
 f.menu.DeleteAllItems();
 f.Fill({L"apple", L"banana"});
 textReads = 0;
 f.box.OnKeyTyped(L'z');
 assert(2 == textReads);
 assert(std::wstring() == f.box.text);
 assert(0 == f.items[6]->clicks && 0 == f.items[7]->clicks);
 puts("PASS: an unmatched character in a shrunk menu reads each item once and selects nothing");
}

static void TypingWalksRowsAfterTheActiveItem() {
 Fixture f;
 f.Fill({L"Alpha", L"beta", L"apple", L"avocado"});
 // a new menu has item 0 active, so the search starts at row 1
 f.box.OnKeyTyped(L'A');
 assert(std::wstring(L"apple") == f.box.text);
 assert(2 == f.menu.GetActiveItem());
 f.box.OnKeyTyped(L'a');
 assert(std::wstring(L"avocado") == f.box.text);
 f.box.OnKeyTyped(L'a');
 assert(std::wstring(L"Alpha") == f.box.text);
 assert(0 == f.menu.GetActiveItem());
 f.box.OnKeyTyped(L'b');
 assert(std::wstring(L"beta") == f.box.text);
 f.box.OnKeyTyped(L'b');
 assert(std::wstring(L"beta") == f.box.text);
 assert(2 == f.items[1]->clicks);
 assert(1 == f.items[0]->clicks);
 puts("PASS: typing cycles through the matching rows and wraps around");
}

// rows sorted in another order than the item IDs: ID 2 is row 0, ID 0 is row 2
static void ReorderedRows(Fixture& f) {
 f.Fill({L"cherry", L"banana", L"apple"});
 f.menu.m_SortedItems.items = {2, 1, 0};
}

static void TypingActivatesTheItemOfTheMatchedRow() {
 Fixture f;
 ReorderedRows(f);
 f.menu.ActivateItem(2);
 f.box.OnKeyTyped(L'c');
 assert(std::wstring(L"cherry") == f.box.text);
 assert(0 == f.menu.GetActiveItem());
 assert(1 == f.items[0]->clicks);
 puts("PASS: typing starts after the active item's row and activates the matched row's item");
}

static void ArrowKeysActivateTheItemOfTheNextRow() {
 Fixture f;
 ReorderedRows(f);
 f.box.text = L"apple";
 f.box.MoveAlongMenuItemList(1);
 assert(std::wstring(L"banana") == f.box.text);
 assert(1 == f.menu.GetActiveItem());
 f.box.MoveAlongMenuItemList(1);
 assert(std::wstring(L"cherry") == f.box.text);
 assert(0 == f.menu.GetActiveItem());
 f.box.MoveAlongMenuItemList(1);
 assert(std::wstring(L"cherry") == f.box.text);
 f.box.MoveAlongMenuItemList(-1);
 assert(std::wstring(L"banana") == f.box.text);
 assert(1 == f.menu.GetActiveItem());
 puts("PASS: the arrow keys move by row and activate that row's item, stopping at the ends");
}

static void OpeningHighlightsTheItemOfTheShownText() {
 Fixture f;
 ReorderedRows(f);
 f.box.text = L"apple";
 f.box.DoClick();
 assert(2 == f.menu.highlighted);
 puts("PASS: opening the dropdown highlights the item ID of the row showing the text");
}

int main() {
 ClearingTheMenuResetsItsState();
 TypingIntoAnEmptyMenuPassesTheKeyOn();
 TypingAnUnmatchedCharacterAfterShrinkingEnds();
 TypingWalksRowsAfterTheActiveItem();
 TypingActivatesTheItemOfTheMatchedRow();
 ArrowKeysActivateTheItemOfTheNextRow();
 OpeningHighlightsTheItemOfTheShownText();
}
'''


def main():
    menu = (ROOT / 'vgui2/controls/Menu.cpp').read_text(encoding='utf-8')
    combo = (ROOT / 'vgui2/controls/ComboBox.cpp').read_text(encoding='utf-8')
    code = f'#define TEXT_READ_LIMIT {TEXT_READ_LIMIT}\n' + STUBS
    code += ''.join(function(menu, signature) + '\n' for signature in (
        'void Menu::DeleteAllItems()', 'int Menu::GetItemCount()', 'int Menu::GetMenuID( int index )',
        'MenuItem* Menu::GetMenuItem( int itemID )', 'void Menu::ActivateItem( int itemID )',
        'void Menu::ActivateItemByRow( int row )', 'int Menu::GetActiveItem()'))
    code += ''.join(function(combo, signature) + '\n' for signature in (
        'void ComboBox::OnKeyTyped( wchar_t unichar )', 'void ComboBox::MoveAlongMenuItemList( int direction )',
        'void ComboBox::DoClick()'))
    code += TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-combobox-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder, timeout=TIMEOUT_SECONDS)


if __name__ == '__main__':
    main()
