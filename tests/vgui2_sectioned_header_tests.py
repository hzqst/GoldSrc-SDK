"""Exercise the production VGUI2 SectionedListPanel section header and item column images with tracked TextImages.

Run: python tests/vgui2_sectioned_header_tests.py
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


def declaration(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n};', start) + 3]


STUBS = r'''
#include <cassert>
#include <cstdio>
#include <set>
#include <vector>

#define DECLARE_CLASS_SIMPLE( className, baseClassName ) typedef baseClassName BaseClass
#define ASSERT_AT( expression, line ) assert( expression )

// every TextImage alive, so leaked, freed twice and dangling images are counted
static std::set<const void*> liveImages;
static int created = 0;

namespace vgui2 {
typedef unsigned long HFont;
typedef unsigned long VPANEL;
typedef int MouseCode;
struct Color {};
struct IScheme {};

template <class T> class CUtlVector {
public:
 int Count() const { return (int)items.size(); }
 int AddToTail(const T& item) { items.push_back(item); return Count() - 1; }
 void RemoveAll() { items.clear(); }
 T& operator[](int i) { return items[i]; }
private:
 std::vector<T> items;
};

class IImage {
public:
 virtual ~IImage() {}
 virtual void GetContentSize(int& wide, int& tall) { wide = 10; tall = 10; }
};

class TextImage : public IImage {
public:
 explicit TextImage(const char*) { liveImages.insert(this); ++created; }
 ~TextImage() override { assert(liveImages.erase(this)); }
 void SetFont(HFont) {}
 void SetUseFallbackFont(bool, HFont) {}
 void SetColor(Color) {}
 void SetText(const char*) {}
 void SetText(const wchar_t*) {}
 void ResizeImageToContentMaxWidth(int) {}
};

class KeyValues {
public:
 KeyValues* MakeCopy() const { return new KeyValues; }
 void deleteThis() { delete this; }
 int GetInt(const char*, int) { return 0; }
 const char* GetString(const char*, const char*) { return "cell"; }
};

class ImageList {
public:
 bool IsValidIndex(int index) { return index == 0; }
 IImage* GetImage(int) { return &image; }
 IImage image;
};

struct IInput { VPANEL GetFocus() { return 0; } };
struct IPanel { bool HasParent(VPANEL, VPANEL) { return false; } };
static IInput inputStub;
static IPanel panelStub;
IInput* input() { return &inputStub; }
IPanel* ipanel() { return &panelStub; }

class Panel {
public:
 virtual ~Panel() {}
 virtual void OnSetFocus() {}
 virtual void OnKillFocus() {}
 virtual void OnMousePressed(MouseCode) {}
 virtual void OnMouseDoublePressed(MouseCode) {}
 virtual void PaintBackground() {}
 void InvalidateLayout(bool, bool) {}
 void Repaint() {}
 void RequestFocus(int) {}
 bool HasFocus() { return false; }
 VPANEL GetVParent() { return 0; }
};

// only the members the header and item use; like Label, the image list does not own its images
class Label : public Panel {
public:
 Label(class SectionedListPanel*, const char*, const char*) {}
 virtual void ApplySchemeSettings(IScheme*) {}
 virtual void Paint() {}
 virtual void PerformLayout() {}
 virtual void ClearImages() { images.clear(); }
 void SetText(const char*) {}
 void SetText(const wchar_t*) {}
 void SetVisible(bool) {}
 int SetTextImageIndex(int) { return -1; }
 void SetProportionalScaleImages(bool) {}
 void SetPaintBackgroundEnabled(bool) {}
 void MakeReadyForUse() {}
 void SetFgColor(Color) {}
 Color GetFgColor() { return Color(); }
 HFont GetFont() { return 0; }
 int GetImageCount() { return (int)images.size(); }
 IImage* GetImageAtIndex(int index) { return images[index]; }
 void SetImageAtIndex(int index, IImage* image, int) {
  while ((int)images.size() <= index) AddImage(nullptr, 0);
  images[index] = image;
 }
 int AddImage(IImage* image, int) { images.push_back(image); return (int)images.size() - 1; }
 void SetImageBounds(int, int, int) {}
 std::vector<IImage*> images;
};

class SectionedListPanel {
public:
 enum { COLUMN_HEADER_IMAGE = 0x01, COLUMN_IMAGE = 0x02, COLUMN_BRIGHT = 0x04, COLUMN_CENTER = 0x08, COLUMN_RIGHT = 0x10 };
 int GetColumnCountBySection(int section) { return section == 0 ? (int)flags.size() : 0; }
 int GetColumnFlagsBySection(int, int column) { return flags[column]; }
 int GetColumnWidthBySection(int, int) { return 50; }
 HFont GetColumnFallbackFontBySection(int, int) { return 0; }
 const char* GetColumnNameBySection(int, int) { return "column"; }
 const wchar_t* GetColumnTextBySection(int, int) { return L"column"; }
 bool IsInEditMode() { return false; }
 std::vector<int> flags;
 ImageList* m_pImageList = nullptr;
};
'''

TESTS = r'''
}
using vgui2::CItemButton;
using vgui2::CSectionHeader;
using vgui2::SectionedListPanel;

// the header and item, defined in the production file, are reached through these helpers
struct Header : CSectionHeader {
 Header(SectionedListPanel* list) : CSectionHeader(list, "header", 0) {}
 void Layout() { PerformLayout(); }
};

struct Item : CItemButton {
 Item(SectionedListPanel* list) : CItemButton(list, 0) {
  vgui2::KeyValues data;
  SetSectionID(0);
  SetData(&data);
 }
 void Layout() { PerformLayout(); }
};

// every image in the list is NULL, the image list image or a live TextImage
static void CheckImages(vgui2::Label& label, SectionedListPanel& list) {
 for (vgui2::IImage* image : label.images)
  assert(!image || liveImages.count(image) || (list.m_pImageList && image == &list.m_pImageList->image));
}

static void DestroyingTheHeaderFreesItsColumnImages() {
 SectionedListPanel list;
 list.flags = { 0, SectionedListPanel::COLUMN_RIGHT, 0 };
 {
  Header header(&list);
  header.Layout();
  assert(3 == header.GetImageCount());
  assert(3 == liveImages.size());
  CheckImages(header, list);
 }
 assert(0 == liveImages.size());
 puts("PASS: destroying a laid out section header frees its column text images");
}

static void AddingAColumnRebuildsTheHeaderWithoutLeaking() {
 SectionedListPanel list;
 list.flags = { 0, 0 };
 {
  Header header(&list);
  header.Layout();
  assert(2 == liveImages.size());
  list.flags.push_back(0);
  header.Layout();
  assert(3 == header.GetImageCount());
  assert(3 == liveImages.size());
  CheckImages(header, list);
 }
 assert(0 == liveImages.size());
 puts("PASS: adding a column frees the old header images before rebuilding them");
}

static void HeaderImageColumnsGetNoTextImage() {
 SectionedListPanel list;
 list.flags = { SectionedListPanel::COLUMN_HEADER_IMAGE, 0 };
 {
  Header header(&list);
  header.Layout();
  assert(2 == header.GetImageCount());
  assert(!header.GetImageAtIndex(0));
  assert(1 == liveImages.size());
  CheckImages(header, list);
 }
 assert(0 == liveImages.size());
 puts("PASS: header image columns stay NULL and only text columns are freed");
}

static void LayoutWithTheSameColumnsKeepsTheHeaderImages() {
 SectionedListPanel list;
 list.flags = { 0, 0 };
 {
  Header header(&list);
  header.Layout();
  int before = created;
  vgui2::IImage* first = header.GetImageAtIndex(0);
  header.Layout();
  header.Layout();
  assert(before == created);
  assert(first == header.GetImageAtIndex(0));
  assert(2 == liveImages.size());
 }
 assert(0 == liveImages.size());
 puts("PASS: laying out the header again with the same columns creates no new images");
}

static void RecyclingAnItemFreesItsColumnImages() {
 SectionedListPanel list;
 list.flags = { 0, 0 };
 {
  Item item(&list);
  item.Layout();
  assert(2 == liveImages.size());
  // DeleteAllItems clears the item onto the free list, which leaves it without a section
  item.Clear();
  assert(0 == item.GetImageCount());
  assert(0 == liveImages.size());
 }
 assert(0 == liveImages.size());
 puts("PASS: a recycled item frees its column images, and destroying it unused frees nothing again");
}

static void AddingAColumnRebuildsTheItemWithoutLeaking() {
 SectionedListPanel list;
 list.flags = { 0, 0 };
 {
  Item item(&list);
  item.Layout();
  list.flags.push_back(0);
  item.Layout();
  assert(3 == item.GetImageCount());
  assert(3 == liveImages.size());
  CheckImages(item, list);
 }
 assert(0 == liveImages.size());
 puts("PASS: adding a column frees the old item images before rebuilding them");
}

static void ItemImageColumnsKeepTheImageListImage() {
 SectionedListPanel list;
 vgui2::ImageList images;
 list.m_pImageList = &images;
 list.flags = { SectionedListPanel::COLUMN_IMAGE, 0 };
 {
  Item item(&list);
  item.Layout();
  assert(2 == item.GetImageCount());
  assert(&images.image == item.GetImageAtIndex(0));
  assert(1 == liveImages.size());
  list.flags.push_back(SectionedListPanel::COLUMN_IMAGE);
  item.Layout();
  assert(3 == item.GetImageCount());
  assert(&images.image == item.GetImageAtIndex(2));
  assert(1 == liveImages.size());
  CheckImages(item, list);
 }
 assert(0 == liveImages.size());
 puts("PASS: item image columns show the image list image, which is never freed");
}

int main() {
 DestroyingTheHeaderFreesItsColumnImages();
 AddingAColumnRebuildsTheHeaderWithoutLeaking();
 HeaderImageColumnsGetNoTextImage();
 LayoutWithTheSameColumnsKeepsTheHeaderImages();
 RecyclingAnItemFreesItsColumnImages();
 AddingAColumnRebuildsTheItemWithoutLeaking();
 ItemImageColumnsKeepTheImageListImage();
}
'''


def main():
    source = (ROOT / 'vgui2/controls/SectionedListPanel.cpp').read_text(encoding='utf-8')
    code = STUBS + function(source, 'static void FreeTextImages( Label* label, CUtlVector<TextImage*>& textImages )') + '\n'
    code += declaration(source, 'class CSectionHeader : public Label') + '\n'
    code += declaration(source, 'class CItemButton : public Label') + '\n'
    code += function(source, 'void CSectionHeader::PerformLayout()') + '\n'
    code += function(source, 'void CItemButton::PerformLayout()') + '\n'
    # the scheme, paint and mouse overrides draw or send messages and are not under test
    code += r'''void CSectionHeader::ApplySchemeSettings( IScheme* ) {}
void CSectionHeader::Paint() {}
void CItemButton::ApplySchemeSettings( IScheme* ) {}
void CItemButton::PaintBackground() {}
void CItemButton::OnMousePressed( MouseCode ) {}
void CItemButton::OnMouseDoublePressed( MouseCode ) {}
'''
    code += TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-sectioned-header-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
