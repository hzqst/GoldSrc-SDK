"""Exercise the production cstrike CAvatarImage texture lifetime and the VGUI2 ImageList image ownership with tracked textures and images.

Run: python tests/cstrike_avatar_image_tests.py
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
#include <cmath>
#include <cstdio>
#include <set>
#include <vector>

// every texture and avatar image alive, so leaked and freed twice ones are counted
static std::set<int> liveTextures;
static std::set<const void*> liveAvatars;
static int nextTexture = 1;

struct Color {
 Color() {}
 Color(int, int, int, int) {}
};

enum EUniverse { k_EUniverseInvalid };
enum EAccountType { k_EAccountTypeInvalid };
static const unsigned k_unSteamUserDefaultInstance = 1;
class CSteamID {
public:
 CSteamID() {}
 CSteamID(unsigned, unsigned, EUniverse, EAccountType) {}
};

namespace vgui2 {
class IImage {
public:
 virtual void Paint() = 0;
 virtual void SetPos(int x, int y) = 0;
 virtual void GetContentSize(int& width, int& height) = 0;
 virtual void GetSize(int& width, int& height) = 0;
 virtual void SetSize(int width, int height) = 0;
 virtual void SetColor(Color color) = 0;
 virtual ~IImage() {}
 virtual void SetAdditive(bool additive) = 0;
};

// texture ids are never 0, like GL texture names
class ISurface {
public:
 int CreateNewTextureID(bool) { liveTextures.insert(nextTexture); return nextTexture++; }
 bool DeleteTextureByID(int texture) { assert(liveTextures.erase(texture)); return true; }
 void DrawSetTextureRGBA(int texture, const unsigned char*, int, int, bool, bool) { assert(liveTextures.count(texture)); }
};
static ISurface theSurface;
static ISurface* surface() { return &theSurface; }
}

template <class T> class CUtlVector {
public:
 int Count() const { return (int)items.size(); }
 int AddToTail(const T& item) { items.push_back(item); return Count() - 1; }
 T& operator[](int i) { return items[i]; }
private:
 std::vector<T> items;
};
'''

TESTS = r'''
using vgui2::ImageList;

// counts itself alive so the image list deleting it is seen
struct TrackedAvatar : CAvatarImage {
 TrackedAvatar() : CAvatarImage(nullptr, nullptr) { liveAvatars.insert(this); }
 ~TrackedAvatar() override { assert(liveAvatars.erase(this)); }
};

static const unsigned char RGBA[4] = { 255, 255, 255, 255 };

static void AnAvatarFreesItsTexture() {
 {
  CAvatarImage image(nullptr, nullptr);
  assert(0 == liveTextures.size());
 }
 {
  CAvatarImage image(nullptr, nullptr);
  image.InitFromRGBA(RGBA, 1, 1);
  assert(1 == liveTextures.size());
 }
 assert(0 == liveTextures.size());
 puts("PASS: an avatar image frees the texture it created, and one without avatar data frees none");
}

static void AReinitializedAvatarFreesItsPreviousTexture() {
 {
  CAvatarImage image(nullptr, nullptr);
  image.InitFromRGBA(RGBA, 1, 1);
  image.InitFromRGBA(RGBA, 1, 1);
  image.InitFromRGBA(RGBA, 1, 1);
  assert(1 == liveTextures.size());
 }
 assert(0 == liveTextures.size());
 puts("PASS: an avatar image given new avatar data frees the texture of its previous avatar");
}

static void AnOwningImageListDeletesItsAvatars() {
 {
  // the scoreboard list, which holds only the avatar images it creates
  ImageList list(true);
  for (int i = 0; i < 3; ++i) {
   TrackedAvatar* avatar = new TrackedAvatar;
   avatar->InitFromRGBA(RGBA, 1, 1);
   assert(list.IsValidIndex(list.AddImage(avatar)));
  }
  assert(3 == liveAvatars.size() && 3 == liveTextures.size());
 }
 assert(0 == liveAvatars.size() && 0 == liveTextures.size());
 TrackedAvatar* kept = new TrackedAvatar;
 {
  ImageList list(false);
  list.AddImage(kept);
 }
 assert(1 == liveAvatars.count(kept));
 delete kept;
 puts("PASS: an image list made with deleteImagesWhenDone deletes its avatar images and their textures, and one without keeps them");
}

int main() {
 setvbuf(stdout, nullptr, _IONBF, 0);
 AnAvatarFreesItsTexture();
 AReinitializedAvatarFreesItsPreviousTexture();
 AnOwningImageListDeletesItsAvatars();
}
'''


def main():
    image_list_header = (ROOT / 'vgui2/controls/ImageList.h').read_text(encoding='utf-8')
    image_list = (ROOT / 'vgui2/controls/ImageList.cpp').read_text(encoding='utf-8')
    avatar_header = (ROOT / 'cstrike/public/game_controls/vgui_avatarimage.h').read_text(encoding='utf-8')
    avatar = (ROOT / 'cstrike/vgui2/game_controls/vgui_avatarimage.cpp').read_text(encoding='utf-8')
    code = STUBS
    code += 'namespace vgui2 {\n' + declaration(image_list_header, 'class ImageList') + '\n}\n'
    code += declaration(image_list, 'class BlankImage : public vgui2::IImage') + '\n'
    code += function(image_list, 'void BlankImage::GetContentSize( int& width, int& height )') + '\n'
    code += function(image_list, 'void BlankImage::GetSize( int& width, int& height )') + '\n'
    code += 'namespace vgui2 {\n'
    for signature in ('ImageList::ImageList( bool deleteImagesWhenDone )', 'ImageList::~ImageList()',
                      'int ImageList::GetImageCount()', 'IImage* ImageList::GetImage( int index )',
                      'bool ImageList::IsValidIndex( int index )', 'int ImageList::AddImage( IImage* image )'):
        code += function(image_list, signature) + '\n'
    code += '}\n'
    code += 'class CCSClientScoreBoardDialog;\nclass CAvatarImagePanel;\n'
    code += declaration(avatar_header, 'class CAvatarImage : public vgui2::IImage') + '\n'
    for signature in ('CAvatarImage::CAvatarImage( vgui2::IImage* blockedImage, vgui2::IImage* friendImage )',
                      'CAvatarImage::~CAvatarImage()', 'void CAvatarImage::ClearAvatarSteamID()',
                      'void CAvatarImage::InitFromRGBA( const unsigned char* rgba, int width, int height )',
                      'void CAvatarImage::SetPos( int x, int y )', 'void CAvatarImage::GetContentSize( int& width, int& height )',
                      'void CAvatarImage::GetSize( int& width, int& height )', 'void CAvatarImage::SetSize( int width, int height )',
                      'void CAvatarImage::SetColor( Color color )', 'void CAvatarImage::SetAdditive( bool additive )'):
        code += function(avatar, signature) + '\n'
    # painting draws with the surface and is not under test
    code += 'void CAvatarImage::Paint() {}\n'
    code += TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-cstrike-avatar-image-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
