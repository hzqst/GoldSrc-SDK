"""Exercise the production VGUI2 SectionedListPanel section header column images with tracked TextImages.

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
 void SetText(const wchar_t*) {}
 void ResizeImageToContentMaxWidth(int) {}
};

// only the members the section header uses; like Label, the image list does not own its images
class Label {
public:
 Label(class SectionedListPanel*, const char*, const char*) {}
 virtual ~Label() {}
 virtual void ApplySchemeSettings(IScheme*) {}
 virtual void Paint() {}
 virtual void PerformLayout() {}
 void SetText(const wchar_t*) {}
 void SetVisible(bool) {}
 int SetTextImageIndex(int) { return -1; }
 void SetProportionalScaleImages(bool) {}
 void SetPaintBackgroundEnabled(bool) {}
 void MakeReadyForUse() {}
 void SetFgColor(Color) {}
 Color GetFgColor() { return Color(); }
 HFont GetFont() { return 0; }
 void ClearImages() { images.clear(); }
 int GetImageCount() { return (int)images.size(); }
 IImage* GetImageAtIndex(int index) { return images[index]; }
 void SetImageAtIndex(int index, IImage* image, int) {
  if (index >= (int)images.size()) images.resize(index + 1, nullptr);
  images[index] = image;
 }
 void SetImageBounds(int, int, int) {}
 std::vector<IImage*> images;
};

class SectionedListPanel {
public:
 enum { COLUMN_HEADER_IMAGE = 0x01, COLUMN_CENTER = 0x08, COLUMN_RIGHT = 0x10 };
 int GetColumnCountBySection(int) { return (int)flags.size(); }
 int GetColumnFlagsBySection(int, int column) { return flags[column]; }
 int GetColumnWidthBySection(int, int) { return 50; }
 HFont GetColumnFallbackFontBySection(int, int) { return 0; }
 const wchar_t* GetColumnTextBySection(int, int) { return L"column"; }
 std::vector<int> flags;
};
'''

TESTS = r'''
}
using vgui2::CSectionHeader;
using vgui2::SectionedListPanel;

// the section header, defined in the production file, is reached through these helpers
struct Header : CSectionHeader {
 Header(SectionedListPanel* list) : CSectionHeader(list, "header", 0) {}
 void Layout() { PerformLayout(); }
};

// every image in the header list is NULL or a live TextImage
static void CheckImages(Header& header) {
 for (vgui2::IImage* image : header.images)
  assert(!image || liveImages.count(image));
}

static void DestroyingTheHeaderFreesItsColumnImages() {
 SectionedListPanel list;
 list.flags = { 0, SectionedListPanel::COLUMN_RIGHT, 0 };
 {
  Header header(&list);
  header.Layout();
  assert(3 == header.GetImageCount());
  assert(3 == liveImages.size());
  CheckImages(header);
 }
 assert(0 == liveImages.size());
 puts("PASS: destroying a laid out section header frees its column text images");
}

static void AddingAColumnRebuildsWithoutLeaking() {
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
  CheckImages(header);
 }
 assert(0 == liveImages.size());
 puts("PASS: adding a column frees the old column images before rebuilding them");
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
  CheckImages(header);
 }
 assert(0 == liveImages.size());
 puts("PASS: header image columns stay NULL and only text columns are freed");
}

static void LayoutWithTheSameColumnsKeepsTheImages() {
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
 puts("PASS: laying out again with the same columns creates no new images");
}

int main() {
 DestroyingTheHeaderFreesItsColumnImages();
 AddingAColumnRebuildsWithoutLeaking();
 HeaderImageColumnsGetNoTextImage();
 LayoutWithTheSameColumnsKeepsTheImages();
}
'''


def main():
    source = (ROOT / 'vgui2/controls/SectionedListPanel.cpp').read_text(encoding='utf-8')
    code = STUBS + declaration(source, 'class CSectionHeader : public Label') + '\n'
    code += function(source, 'void CSectionHeader::PerformLayout()') + '\n'
    # the scheme and paint overrides draw through the surface and are not under test
    code += 'void CSectionHeader::ApplySchemeSettings( IScheme* ) {}\nvoid CSectionHeader::Paint() {}\n'
    code += TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-sectioned-header-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
