"""Exercise the production VGUI2 MenuItem user data setter, getter and destructor with tracked KeyValues.

Run: python tests/vgui2_menuitem_userdata_tests.py
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
#include <set>
#include <string>
#include <vector>

// a deleted KeyValues stays readable until the end, so using or deleting it again is counted instead of corrupting the heap
static std::set<const void*> liveKeys;
static int deadUses = 0;

namespace vgui2 {
class KeyValues {
public:
 explicit KeyValues(std::string value) : value(value) { liveKeys.insert(this); }
 ~KeyValues() { liveKeys.erase(this); }
 KeyValues* MakeCopy() const {
  if (!liveKeys.count(this)) ++deadUses;
  return new KeyValues(value);
 }
 void deleteThis() {
  if (!liveKeys.erase(this)) { ++deadUses; return; }
  graveyard.push_back(this);
 }
 static void FreeGraveyard() { for (KeyValues* key : graveyard) delete key; graveyard.clear(); }
 std::string value;
private:
 static std::vector<KeyValues*> graveyard;
};
std::vector<KeyValues*> KeyValues::graveyard;

struct TextImage {};
struct Image {};
struct Menu {
 int GetActiveItem() { return 0; }
 KeyValues* GetItemUserData(int) { return nullptr; }
};

// only the members the user data functions use
class MenuItem {
public:
 ~MenuItem();
 KeyValues* GetUserData();
 void SetUserData(const KeyValues* data);
 Menu* m_pCascadeMenu = nullptr;
 TextImage* m_pCascadeArrow = nullptr;
 Image* m_pCheck = nullptr;
 KeyValues* m_pUserData = nullptr;
};
'''

TESTS = r'''
}
using vgui2::KeyValues;
using vgui2::MenuItem;

static void Check(size_t live) {
 assert(live == liveKeys.size());
 assert(0 == deadUses);
}

static void ClearingTheUserDataLeavesNothingToFreeAgain() {
 KeyValues data("first");
 {
  MenuItem item;
  item.SetUserData(&data);
  assert(std::string("first") == item.GetUserData()->value);
  item.SetUserData(nullptr);
  assert(!item.GetUserData());
  item.SetUserData(nullptr);
  assert(!item.GetUserData());
  Check(1);
 }
 Check(1);
 puts("PASS: clearing the user data, also twice, returns NULL and the destructor frees nothing again");
}

static void ReplacingTheUserDataFreesTheOldCopy() {
 KeyValues first("first"), second("second");
 {
  MenuItem item;
  item.SetUserData(&first);
  item.SetUserData(&second);
  assert(std::string("second") == item.GetUserData()->value);
  Check(3);
 }
 Check(2);
 puts("PASS: replacing the user data frees the old copy and the destructor frees the new one");
}

static void SettingTheCurrentUserDataKeepsIt() {
 KeyValues data("same");
 {
  MenuItem item;
  item.SetUserData(&data);
  item.SetUserData(item.GetUserData());
  assert(std::string("same") == item.GetUserData()->value);
  Check(2);
 }
 Check(1);
 puts("PASS: setting the current user data again copies it before freeing it");
}

int main() {
 ClearingTheUserDataLeavesNothingToFreeAgain();
 ReplacingTheUserDataFreesTheOldCopy();
 SettingTheCurrentUserDataKeepsIt();
 KeyValues::FreeGraveyard();
}
'''


def main():
    source = (ROOT / 'vgui2/controls/MenuItem.cpp').read_text(encoding='utf-8')
    code = STUBS + ''.join(function(source, signature) + '\n' for signature in (
        'MenuItem::~MenuItem()', 'KeyValues* MenuItem::GetUserData()', 'void MenuItem::SetUserData( const KeyValues* data )'))
    code += TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-menuitem-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
