"""Exercise the production VGUI2 CSchemeManager::GetImage cache with both hardware filtering modes of one image.

Run: python tests/vgui2_scheme_image_cache_tests.py
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
#include <string>
#include <vector>
#ifdef _MSC_VER
#include <string.h>
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

namespace vgui2 {
class IImage {
public:
 virtual ~IImage() {}
};

class Bitmap : public IImage {
public:
 Bitmap(const char* filename, bool hardwareFiltered) : name(filename), filtered(hardwareFiltered) {}
 const char* GetName() { return name.c_str(); }
 std::string name;
 bool filtered;
};
}
using namespace vgui2;

// a sorted vector stands in for CUtlRBTree; it finds and inserts with the same less function
template <class T, class I>
class CUtlRBTree {
public:
 typedef bool (*LessFunc_t)(const T&, const T&);
 void SetLessFunc(LessFunc_t func) { less = func; }
 I Find(const T& search) const {
  for (size_t i = 0; i < elements.size(); ++i)
   if (!less(search, elements[i]) && !less(elements[i], search)) return (I)i;
  return -1;
 }
 void Insert(const T& element) {
  size_t i = 0;
  while (i < elements.size() && !less(element, elements[i])) ++i;
  elements.insert(elements.begin() + i, element);
 }
 bool IsValidIndex(I i) const { return i >= 0 && (size_t)i < elements.size(); }
 T& operator[](I i) { return elements[i]; }
 int Count() const { return (int)elements.size(); }
 std::vector<T> elements;
 LessFunc_t less = nullptr;
};

// only the members the image cache uses
class CSchemeManager {
public:
 struct CachedBitmapHandle_t
 {
  vgui2::Bitmap* bitmap;
  bool hardwareFiltered;
 };
 static bool BitmapHandleSearchFunc(const CachedBitmapHandle_t& lhs, const CachedBitmapHandle_t& rhs);
 IImage* GetImage(const char* imageName, bool hardwareFiltered);
 CUtlRBTree<CachedBitmapHandle_t, int> m_Bitmaps;
 static const char* s_pszSearchString;
};
const char* CSchemeManager::s_pszSearchString = NULL;
'''

TESTS = r'''
static Bitmap* Get(CSchemeManager& scheme, const char* name, bool filtered) {
 return (Bitmap*)scheme.GetImage(name, filtered);
}

static void Free(CSchemeManager& scheme) {
 for (auto& handle : scheme.m_Bitmaps.elements) delete handle.bitmap;
}

static void EachFilteringModeGetsItsOwnBitmap(bool first) {
 CSchemeManager scheme;
 scheme.m_Bitmaps.SetLessFunc(&CSchemeManager::BitmapHandleSearchFunc);
 Bitmap* a = Get(scheme, "gfx/vgui/image", first);
 Bitmap* b = Get(scheme, "gfx/vgui/image", !first);
 assert(a != b);
 assert(first == a->filtered);
 assert(!first == b->filtered);
 assert(2 == scheme.m_Bitmaps.Count());
 Free(scheme);
 printf("PASS: requesting one image filtered=%d then filtered=%d creates a bitmap for each mode\n", first, !first);
}

static void RepeatedRequestsReuseTheCachedBitmap() {
 CSchemeManager scheme;
 scheme.m_Bitmaps.SetLessFunc(&CSchemeManager::BitmapHandleSearchFunc);
 Bitmap* filtered = Get(scheme, "gfx/vgui/image", true);
 Bitmap* unfiltered = Get(scheme, "gfx/vgui/image", false);
 Get(scheme, "gfx/vgui/other", true);
 assert(filtered == Get(scheme, "gfx/vgui/image", true));
 assert(unfiltered == Get(scheme, "gfx/vgui/image", false));
 assert(filtered == Get(scheme, "GFX/VGUI/Image", true));
 assert(unfiltered == Get(scheme, "GFX/VGUI/Image", false));
 assert(3 == scheme.m_Bitmaps.Count());
 Free(scheme);
 puts("PASS: repeating a name and filtering mode, in any letter case, returns the cached bitmap");
}

int main() {
 EachFilteringModeGetsItsOwnBitmap(true);
 EachFilteringModeGetsItsOwnBitmap(false);
 RepeatedRequestsReuseTheCachedBitmap();
}
'''


def main():
    source = (ROOT / 'vgui2/src/Scheme.cpp').read_text(encoding='utf-8')
    code = STUBS + ''.join(function(source, signature) + '\n' for signature in (
        'bool CSchemeManager::BitmapHandleSearchFunc(', 'IImage* CSchemeManager::GetImage('))
    code += TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-scheme-image-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
