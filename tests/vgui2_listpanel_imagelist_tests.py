"""Exercise the production VGUI2 ListPanel image list ownership and column header images with tracked ImageLists.

Run: python tests/vgui2_listpanel_imagelist_tests.py
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
#include <vector>

#define ASSERT_AT( expression, line ) assert( expression )

// every ImageList and image alive, so leaked, freed twice and dangling ones are counted
static std::set<const void*> liveLists;
static std::set<const void*> liveImages;

namespace vgui2 {
class IImage {
public:
 IImage() { liveImages.insert(this); }
 virtual ~IImage() { assert(liveImages.erase(this)); }
};

template <class T> class CUtlVector {
public:
 int Count() const { return (int)items.size(); }
 int AddToTail(const T& item) { items.push_back(item); return Count() - 1; }
 T& operator[](int i) { return items[i]; }
private:
 std::vector<T> items;
};

template <class T, class I> class CUtlLinkedList {
public:
 I Head() { return items.empty() ? InvalidIndex() : 0; }
 I Next(I i) { return i + 1 < (I)items.size() ? i + 1 : InvalidIndex(); }
 I InvalidIndex() { return (I)-1; }
 I AddToTail(const T& item) { items.push_back(item); return (I)items.size() - 1; }
 void RemoveAll() { items.clear(); }
 T& operator[](I i) { return items[i]; }
private:
 std::vector<T> items;
};

// owns its blank image at index 0 like ImageList, the other images are added by the test
class ImageList {
public:
 ImageList() { liveLists.insert(this); images.push_back(new IImage); }
 ~ImageList() {
  for (IImage* image : images) delete image;
  assert(liveLists.erase(this));
 }
 int AddImage() { images.push_back(new IImage); return (int)images.size() - 1; }
 int GetImageCount() { return (int)images.size(); }
 IImage* GetImage(int index) { return images[index]; }
 bool IsValidIndex(int index) { return index >= 0 && index < (int)images.size(); }
 std::vector<IImage*> images;
};

// only the members the image list code uses; a text header shows its own text image at index 0
class Button {
public:
 Button() { images.push_back(&text); }
 int GetImageCount() { return (int)images.size(); }
 IImage* GetImageAtIndex(int index) { assert(index < (int)images.size()); return images[index]; }
 void SetImageAtIndex(int index, IImage* image, int) { images[index] = image; }
 void SetTextImageIndex(int) {}
 void MarkForDeletion() {}
 std::vector<IImage*> images;
 IImage text;
};
struct Panel { void MarkForDeletion() {} };
class TextImage {};
class ImagePanel {};
class ScrollBar {};

class ListPanel {
public:
 ~ListPanel();
 void SetImageList(ImageList* imageList, bool deleteImageListWhenDone);
 void SetColumnHeaderImage(int column, int image);
 void DeleteAllItems() {}
 // adds a column with its header
 void AddColumn(Button* header) {
  m_CurrentColumns.AddToTail(m_ColumnsData.AddToTail(column_t{ header, &resizer }));
 }
 struct column_t {
  Button* m_pHeader;
  Panel* m_pResizer;
 };
 CUtlLinkedList<column_t, unsigned char> m_ColumnsData;
 CUtlVector<unsigned char> m_CurrentColumns;
 TextImage* m_pTextImage = nullptr;
 ImagePanel* m_pImagePanel = nullptr;
 ScrollBar* m_vbar = nullptr;
 ImageList* m_pImageList = nullptr;
 bool m_bDeleteImageListWhenDone = false;
 TextImage* m_pEmptyListText = nullptr;
 Panel resizer;
};
'''

TESTS = r'''
}
using vgui2::Button;
using vgui2::IImage;
using vgui2::ImageList;
using vgui2::ListPanel;

static void AnOwnedListIsDeletedOnceAndASharedOneIsKept() {
 {
  ListPanel list;
  list.SetImageList(new ImageList, true);
  assert(1 == liveLists.size());
 }
 assert(0 == liveLists.size());
 ImageList shared;
 {
  ListPanel list;
  list.SetImageList(new ImageList, true);
  list.SetImageList(&shared, false);
  assert(1 == liveLists.size());
 }
 assert(1 == liveLists.count(&shared) && 1 == liveLists.size());
 puts("PASS: an owned image list is deleted once, with the panel or when replaced, and a shared one is kept");
}

static void SettingTheSameListAgainOnlyChangesTheOwnership() {
 ImageList* images = new ImageList;
 {
  ListPanel list;
  list.SetImageList(images, true);
  list.SetImageList(images, true);
  assert(1 == liveLists.count(images));
  assert(images == list.m_pImageList);
  list.SetImageList(images, false);
  assert(1 == liveLists.count(images));
 }
 assert(1 == liveLists.count(images));
 {
  ListPanel list;
  list.SetImageList(images, false);
  list.SetImageList(images, true);
  assert(1 == liveLists.count(images));
 }
 assert(0 == liveLists.size());
 puts("PASS: setting the same image list again keeps it and only switches who deletes it");
}

static void ReplacingAListGivesTheHeadersTheImageAtTheSameIndex() {
 Button imageHeader, blankHeader, missingHeader, textHeader;
 ListPanel list;
 list.AddColumn(&imageHeader);
 list.AddColumn(&blankHeader);
 list.AddColumn(&missingHeader);
 list.AddColumn(&textHeader);
 ImageList* first = new ImageList;
 first->AddImage();
 int second = first->AddImage();
 list.SetImageList(first, true);
 list.SetColumnHeaderImage(0, 1);
 list.SetColumnHeaderImage(1, 0);
 list.SetColumnHeaderImage(2, second);
 ImageList* next = new ImageList;
 next->AddImage();
 list.SetImageList(next, true);
 assert(0 == liveLists.count(first));
 assert(next->GetImage(1) == imageHeader.GetImageAtIndex(0));
 assert(next->GetImage(0) == blankHeader.GetImageAtIndex(0));
 assert(nullptr == missingHeader.GetImageAtIndex(0));
 assert(&textHeader.text == textHeader.GetImageAtIndex(0));
 list.SetImageList(nullptr, false);
 assert(0 == liveLists.size());
 assert(nullptr == imageHeader.GetImageAtIndex(0) && nullptr == blankHeader.GetImageAtIndex(0));
 assert(&textHeader.text == textHeader.GetImageAtIndex(0));
 for (Button* header : { &imageHeader, &blankHeader, &missingHeader, &textHeader }) {
  IImage* image = header->GetImageAtIndex(0);
  assert(!image || liveImages.count(image));
 }
 puts("PASS: replacing the image list gives the column headers the image at the same index of the new list, or none, and keeps text headers");
}

int main() {
 setvbuf(stdout, nullptr, _IONBF, 0);
 AnOwnedListIsDeletedOnceAndASharedOneIsKept();
 SettingTheSameListAgainOnlyChangesTheOwnership();
 ReplacingAListGivesTheHeadersTheImageAtTheSameIndex();
}
'''


def main():
    source = (ROOT / 'vgui2/controls/ListPanel.cpp').read_text(encoding='utf-8')
    code = STUBS
    for signature in ('ListPanel::~ListPanel()',
                      'void ListPanel::SetImageList( ImageList* imageList, bool deleteImageListWhenDone )',
                      'void ListPanel::SetColumnHeaderImage( int column, int image )'):
        code += function(source, signature) + '\n'
    code += TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-listpanel-imagelist-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
