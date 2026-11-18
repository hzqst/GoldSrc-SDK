"""Exercise the production Career bot portrait paths with stubs.

Run: python tests/gameui_career_path_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'


def function(source, signature):
    start = re.search(re.escape(signature) + r'[^\n;]*\n\{', source).start()
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
class CBotButton {
public:
 enum { IMAGE_CHAR = 0, IMAGE_WEAPON, IMAGE_MAX };
 void SetImgTexture(int num, const char* filename);
 void ForceImgUpload(int num) { uploads.push_back(m_szImgTexture[num]); m_bImgUploaded[num] = true; }
 void ForceImgReload(int num) { reloads.push_back(m_szImgTexture[num]); }
 bool m_bImgUploaded[IMAGE_MAX] = {};
 char m_szImgTexture[IMAGE_MAX][128] = {};
 std::vector<std::string> uploads, reloads;
};
struct BotProfile { int skin; int GetSkin() const { return skin; } };
struct BotProfileManager {
 std::string model;
 const char* GetCustomSkinModelname(int index) const { return index == 6 ? model.c_str() : nullptr; }
} profilesStub, *TheBotProfiles = &profilesStub;
struct CareerGame { bool PlayAsCT() const { return true; } } gameStub, *TheCareerGame = &gameStub;
// every thumbnail exists, so only the path length decides which one is used
std::vector<std::string> checkedFiles;
namespace vgui2 {
struct FileSystem { bool FileExists(const char* path) { checkedFiles.push_back(path); return true; } } fileSystemStub;
FileSystem* filesystem() { return &fileSystemStub; }
}
class CCareerCharacter {
public:
 const char* GetSkinThumbnailFname() const;
 const BotProfile* m_pProfile;
};
'''

TESTS = r'''
#ifdef _WIN32
static const size_t THUMBNAIL_PATH_SIZE = _MAX_PATH;
#else
static const size_t THUMBNAIL_PATH_SIZE = PATH_MAX;
#endif
// "gfx/thumbnails/skins/" + model + ".tga" + NUL
static const size_t THUMBNAIL_PATH_OVERHEAD = 21 + 4 + 1;

static std::string Path(size_t length) { return "gfx/" + std::string(length - 4, 'p'); }

static void TextureFitsUpToTheTerminator() {
 CBotButton button;
 button.SetImgTexture(CBotButton::IMAGE_CHAR, Path(127).c_str());
 assert(1 == button.uploads.size());
 assert(Path(127) == button.uploads[0]);
 puts("PASS: a 127-byte texture path is uploaded");
}

static void TextureThatDoesNotFitIsCleared() {
 CBotButton button;
 button.SetImgTexture(CBotButton::IMAGE_CHAR, Path(128).c_str());
 // ForceImgUpload skips an empty path in the game, so nothing is loaded
 assert(1 == button.uploads.size());
 assert(button.uploads[0].empty());
 button.m_bImgUploaded[CBotButton::IMAGE_WEAPON] = true;
 strcpy(button.m_szImgTexture[CBotButton::IMAGE_WEAPON], "gfx/career/weapons/640_ak47");
 button.SetImgTexture(CBotButton::IMAGE_WEAPON, Path(300).c_str());
 assert(1 == button.reloads.size());
 assert(button.reloads[0].empty());
 puts("PASS: texture paths of 128 bytes or more clear the image instead of overflowing");
}

static void CustomThumbnailIsUsedWhenItFits() {
 BotProfile profile = {6};
 CCareerCharacter character = {&profile};
 profilesStub.model = std::string(THUMBNAIL_PATH_SIZE - THUMBNAIL_PATH_OVERHEAD, 'm');
 checkedFiles.clear();
 assert("gfx/thumbnails/skins/" + profilesStub.model == character.GetSkinThumbnailFname());
 assert(1 == checkedFiles.size());
 puts("PASS: the longest custom skin thumbnail path that fits is used");
}

static void CustomThumbnailThatDoesNotFitFallsBack() {
 BotProfile profile = {6};
 CCareerCharacter character = {&profile};
 for (size_t length : {THUMBNAIL_PATH_SIZE - THUMBNAIL_PATH_OVERHEAD + 1, THUMBNAIL_PATH_SIZE + 100}) {
  profilesStub.model = std::string(length, 'm');
  checkedFiles.clear();
  assert(std::string("gfx/thumbnails/skins/ct_random") == character.GetSkinThumbnailFname());
  assert(checkedFiles.empty());
 }
 puts("PASS: custom skin thumbnail paths that do not fit fall back without checking a truncated path");
}
'''


def main():
    utils = (ROOT / 'GameUI/StringUtils.cpp').read_text(encoding='utf-8')
    frame = (ROOT / 'GameUI/CareerBotFrame.cpp').read_text(encoding='utf-8')
    game = (ROOT / 'GameUI/CareerGame.cpp').read_text(encoding='utf-8')
    code = STUBS
    code += function(utils, 'bool UTIL_FormatString(') + function(utils, 'bool UTIL_CopyString(')
    code += function(frame, 'void CBotButton::SetImgTexture(')
    code += function(game, 'const char* CCareerCharacter::GetSkinThumbnailFname(')
    code += TESTS
    code += ('\nint main() { TextureFitsUpToTheTerminator(); TextureThatDoesNotFitIsCleared(); '
             'CustomThumbnailIsUsedWhenItFits(); CustomThumbnailThatDoesNotFitFallsBack(); }\n')
    with tempfile.TemporaryDirectory(prefix='goldsrc-gameui-career-path-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([path / 'test.cpp'], path / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=path)


if __name__ == '__main__':
    main()
