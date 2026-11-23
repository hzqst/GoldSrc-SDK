"""Exercise VGui_LoadBMP row-pitch alignment and bounds validation."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK/engine'


def extract(filename, signature):
    source = (ROOT / filename).read_text(encoding='utf-8')
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

using byte = unsigned char;

#pragma pack(push, 1)
struct BITMAPFILEHEADER {
    std::uint16_t bfType;
    std::uint32_t bfSize;
    std::uint16_t bfReserved1;
    std::uint16_t bfReserved2;
    std::uint32_t bfOffBits;
};
struct BITMAPINFOHEADER {
    std::uint32_t biSize;
    std::int32_t  biWidth;
    std::int32_t  biHeight;
    std::uint16_t biPlanes;
    std::uint16_t biBitCount;
    std::uint32_t biCompression;
    std::uint32_t biSizeImage;
    std::int32_t  biXPelsPerMeter;
    std::int32_t  biYPelsPerMeter;
    std::uint32_t biClrUsed;
    std::uint32_t biClrImportant;
};
struct RGBQUAD {
    byte rgbBlue;
    byte rgbGreen;
    byte rgbRed;
    byte rgbReserved;
};
#pragma pack(pop)

struct MemFile {
    std::vector<byte> data;
    size_t pos = 0;
    bool closed = false;
};
using FileHandle_t = MemFile*;

static unsigned int FS_Size(FileHandle_t file) { return static_cast<unsigned int>(file->data.size()); }
static int FS_Read(void* dest, int size, int count, FileHandle_t file)
{
    const size_t wanted = static_cast<size_t>(size) * static_cast<size_t>(count);
    const size_t avail = file->pos < file->data.size() ? file->data.size() - file->pos : 0;
    const size_t take = wanted < avail ? wanted : avail;
    if (take > 0)
    {
        std::memcpy(dest, file->data.data() + file->pos, take);
        file->pos += take;
    }
    return static_cast<int>(take);
}
static void FS_Close(FileHandle_t file) { file->closed = true; }
'''

TESTS = r'''
static std::vector<byte> MakeBMP8(int width, int height, int paletteEntries, const std::vector<byte>& topDownIndices)
{
    const int pitch = (width + 3) & ~3;
    BITMAPFILEHEADER bmfh = {};
    BITMAPINFOHEADER bmih = {};
    bmfh.bfType = 0x4D42;
    bmfh.bfOffBits = static_cast<std::uint32_t>(sizeof(bmfh) + sizeof(bmih) + paletteEntries * sizeof(RGBQUAD));
    bmfh.bfSize = static_cast<std::uint32_t>(bmfh.bfOffBits + pitch * height);

    bmih.biSize = sizeof(bmih);
    bmih.biWidth = width;
    bmih.biHeight = height;
    bmih.biPlanes = 1;
    bmih.biBitCount = 8;
    bmih.biCompression = 0;
    bmih.biClrUsed = static_cast<std::uint32_t>(paletteEntries);

    std::vector<byte> out(bmfh.bfSize, 0xAA);
    std::memcpy(out.data(), &bmfh, sizeof(bmfh));
    std::memcpy(out.data() + sizeof(bmfh), &bmih, sizeof(bmih));
    for (int i = 0; i < paletteEntries; ++i)
    {
        RGBQUAD q = { static_cast<byte>(i * 3), static_cast<byte>(i * 2), static_cast<byte>(i), 0 };
        std::memcpy(out.data() + sizeof(bmfh) + sizeof(bmih) + i * sizeof(RGBQUAD), &q, sizeof(q));
    }
    byte* pixels = out.data() + bmfh.bfOffBits;
    for (int y = 0; y < height; ++y)
    {
        byte* row = pixels + (height - y - 1) * pitch;
        for (int x = 0; x < width; ++x)
            row[x] = topDownIndices[y * width + x];
    }
    return out;
}

int main()
{
    // Test all width mod 4 cases: 1, 2, 3, 4, 5 (widths 1, 3, 5 failed under pitch += (pitch + 4) % 4)
    for (int w : {1, 2, 3, 4, 5})
    {
        const int h = 2;
        std::vector<byte> indices;
        for (int i = 0; i < w * h; ++i)
            indices.push_back(static_cast<byte>(i + 1));

        MemFile file{MakeBMP8(w, h, 16, indices)};
        std::vector<byte> rgba(w * h * 4, 0);
        int outW = 0, outH = 0;
        assert(1 == VGui_LoadBMP(&file, rgba.data(), static_cast<int>(rgba.size()), &outW, &outH));
        assert(file.closed);
        assert(w == outW && h == outH);
        for (int i = 0; i < w * h; ++i)
        {
            const byte idx = indices[i];
            assert(idx == rgba[i * 4 + 0]);
            assert(static_cast<byte>(idx * 2) == rgba[i * 4 + 1]);
            assert(static_cast<byte>(idx * 3) == rgba[i * 4 + 2]);
            assert(255 == rgba[i * 4 + 3]);
        }
    }

    // Reject buffer too small
    {
        MemFile file{MakeBMP8(3, 2, 16, {1, 2, 3, 4, 5, 6})};
        byte smallBuf[8] = {};
        int outW = 0, outH = 0;
        assert(0 == VGui_LoadBMP(&file, smallBuf, sizeof(smallBuf), &outW, &outH));
        assert(file.closed);
    }

    // Reject truncated pixel data
    {
        auto bmp = MakeBMP8(3, 2, 16, {1, 2, 3, 4, 5, 6});
        bmp.pop_back();
        MemFile file{bmp};
        byte rgba[3 * 2 * 4] = {};
        int outW = 0, outH = 0;
        assert(0 == VGui_LoadBMP(&file, rgba, sizeof(rgba), &outW, &outH));
        assert(file.closed);
    }

    // Reject invalid biClrUsed > 256
    {
        auto bmp = MakeBMP8(2, 2, 4, {1, 2, 3, 0});
        BITMAPINFOHEADER bmih;
        std::memcpy(&bmih, bmp.data() + sizeof(BITMAPFILEHEADER), sizeof(bmih));
        bmih.biClrUsed = 300;
        std::memcpy(bmp.data() + sizeof(BITMAPFILEHEADER), &bmih, sizeof(bmih));
        MemFile file{bmp};
        byte rgba[16] = {};
        int outW = 0, outH = 0;
        assert(0 == VGui_LoadBMP(&file, rgba, sizeof(rgba), &outW, &outH));
        assert(file.closed);
    }
}
'''


def main():
    production = extract('vgui_int.cpp', 'int VGui_LoadBMP( FileHandle_t file, byte* buffer, int bufferSize, int* width, int* height )')
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui-bmp-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
    print('PASS: VGui_LoadBMP aligns row pitch and validates bounds')


if __name__ == '__main__':
    main()
