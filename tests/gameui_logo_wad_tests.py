"""Exercise the production GameUI UpdateLogoWAD and GrabMip with a tracked malloc and free.

Run: python tests/gameui_logo_wad_tests.py
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


def logo_wad(source):
    start = source.index('#define LOGO_BUFFER_SIZE')
    end = source.index('\n}', source.index('bool UpdateLogoWAD(')) + 2
    return source[start:end]


STUBS = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

typedef unsigned char byte;
typedef int32_t LONG;
typedef uint32_t COLORREF;
typedef void* FileHandle_t;
struct BITMAPINFOHEADER {
 uint32_t biSize; LONG biWidth; LONG biHeight; uint16_t biPlanes; uint16_t biBitCount;
 uint32_t biCompression; uint32_t biSizeImage; LONG biXPelsPerMeter; LONG biYPelsPerMeter;
 uint32_t biClrUsed; uint32_t biClrImportant;
};
struct RGBQUAD { byte rgbBlue, rgbGreen, rgbRed, rgbReserved; };
struct BITMAPINFO { BITMAPINFOHEADER bmiHeader; RGBQUAD bmiColors[1]; };
typedef BITMAPINFO* LPBITMAPINFO;

// frees only the buffers TrackedMalloc returned, and counts any other pointer instead of corrupting the heap
static std::set<void*> allocations;
static int wrongFrees = 0;
static void* TrackedMalloc(size_t size) {
 void* buffer = malloc(size);
 allocations.insert(buffer);
 return buffer;
}
static void TrackedFree(void* buffer) {
 if (!allocations.erase(buffer)) { ++wrongFrees; return; }
 free(buffer);
}

// only the members UpdateLogoWAD uses
class CUtlBuffer {
public:
 enum SeekType_t { SEEK_HEAD };
 CUtlBuffer(int, int, bool) {}
 void Put(const void* source, int size) {
  if (data.size() < (size_t)(put + size)) data.resize(put + size);
  memcpy(&data[put], source, size);
  put += size;
 }
 void SeekPut(SeekType_t, int offset) { put = offset; }
 int TellPut() const { return put; }
 const void* Base() const { return data.data(); }
private:
 std::vector<byte> data;
 int put = 0;
};

static std::string written;
namespace vgui2 {
struct IFileSystem {
 FileHandle_t Open(const char* name, const char*, const char*) { written.clear(); return !strcmp(name, "tempdecal.wad") ? this : nullptr; }
 int Write(const void* source, int size, FileHandle_t) { written.append((const char*)source, size); return size; }
 void Close(FileHandle_t) {}
};
static IFileSystem g_fileSystem;
IFileSystem* filesystem() { return &g_fileSystem; }
}

#define malloc TrackedMalloc
#define free TrackedFree
'''

TESTS = r'''
#undef malloc
#undef free

static const int DIB_HEADER_SIZE = (int)(sizeof(BITMAPINFOHEADER) + LOGO_PALETTE_SIZE * sizeof(RGBQUAD));

// builds an 8-bit DIB of the given size, with pixelBytes bytes of pixel data
static std::vector<byte> MakeDIB(LONG width, LONG height, int pixelBytes) {
 std::vector<byte> dib(DIB_HEADER_SIZE + pixelBytes, 1);
 BITMAPINFOHEADER* header = (BITMAPINFOHEADER*)dib.data();
 memset(header, 0, sizeof(*header));
 header->biSize = sizeof(BITMAPINFOHEADER);
 header->biWidth = width;
 header->biHeight = height;
 return dib;
}

static bool Update(std::vector<byte> dib) {
 written.clear();
 return UpdateLogoWAD(dib.data(), (int)dib.size(), 255, 128, 0);
}

static void RejectedDIBsFreeTheLump() {
 const int REPEATS = 3;
 for (int i = 0; i < REPEATS; i++) {
  assert(!Update(MakeDIB(17, 16, 17 * 16)));
  assert(!Update(MakeDIB(16, 24, 16 * 24)));
 }
 assert(!Update(MakeDIB(32, 32, 32 * 32 - 1)));
 assert(written.empty());
 assert(allocations.empty() && 0 == wrongFrees);
 puts("PASS: DIBs that GrabMip rejects for their size or missing pixel data free the lump");
}

static void SupportedLogosAreWritten() {
 for (int size : {16, 32, 64}) {
  assert(Update(MakeDIB(size, size, size * size)));
  assert(0 == memcmp("WAD3", written.data(), 4));
  int infotableofs;
  memcpy(&infotableofs, written.data() + 8, sizeof(infotableofs));
  assert(written.size() == (size_t)infotableofs + sizeof(lumpinfo_t));
 }
 assert(allocations.empty() && 0 == wrongFrees);
 puts("PASS: 16x16, 32x32 and 64x64 logos are written and free the lump");
}

static void OtherRejectionsFreeTheLump() {
 // GrabMip accepts these, but they are not a supported logo size
 assert(!Update(MakeDIB(16, 32, 16 * 32)));
 assert(!Update(MakeDIB(48, 48, 48 * 48)));
 // too large for the lump buffer, and a DIB without a complete header
 assert(!Update(MakeDIB(256, 256, 256 * 256)));
 std::vector<byte> truncated(DIB_HEADER_SIZE - 1);
 assert(!UpdateLogoWAD(truncated.data(), (int)truncated.size(), 0, 0, 0));
 assert(written.empty());
 assert(allocations.empty() && 0 == wrongFrees);
 puts("PASS: unsupported sizes, oversized and truncated DIBs free the lump");
}

int main() {
 RejectedDIBsFreeTheLump();
 SupportedLogosAreWritten();
 OtherRejectionsFreeTheLump();
}
'''


def main():
    source = (ROOT / 'GameUI/LogoFile.cpp').read_text(encoding='latin-1')
    code = STUBS + logo_wad(source) + '\n' + TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-gameui-logo-wad-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
