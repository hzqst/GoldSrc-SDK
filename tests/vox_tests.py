"""Exercise production VOX parsers with empty and directory-only sentences."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


def main():
    source = (ROOT / 'GoldSrc-SDK/engine/snd_mix.cpp').read_text()
    code = r'''
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <cstdio>
#define Q_strlen std::strlen
#define Q_strcpy std::strcpy
#define Q_memcpy std::memcpy
#define Q_memset std::memset
#define Q_atoi std::atoi
#define Q_strncpy std::strncpy
#define _snprintf std::snprintf
static void Con_DPrintf(const char*, ...) {}
constexpr int MAX_VOX_WORDS = 32;
static char* rgpparseword[MAX_VOX_WORDS];
static char voxperiod[] = "_period", voxcomma[] = "_comma";
struct sfx_t {};
struct sfxcache_t {};
struct voxword_t { int fKeepCached, pitch, volume, start, end, timecompress; sfx_t* sfx; };
constexpr int MAX_VOX_SENTENCES = 16, SOUND_NAME_BYTES = 64;
struct channel_t { int isentence = -1, iword = 0; sfx_t* sfx = nullptr; };
static voxword_t rgrgvoxword[MAX_VOX_SENTENCES][MAX_VOX_WORDS];
static char* sentenceText;
static int loads;
static sfx_t sound;
static sfxcache_t cache;
static char* VOX_LookupString(char*, int*) { return sentenceText; }
static sfx_t* S_FindName(char*, int*) { return &sound; }
static sfxcache_t* S_LoadSound(sfx_t* s, channel_t*) { assert(&sound == s); ++loads; return &cache; }
static void S_FreeChannel(channel_t*) { assert(false); }
'''
    for name in ('char* VOX_GetDirectory(', 'void VOX_ParseString(', 'int VOX_ParseWordParams(',
                 'int VOX_IFindEmptySentence(', 'sfxcache_t* VOX_LoadSound('):
        code += function(source, 'extern "C" ' + name) + '\n'
    code += r'''
int main() {
 char name[] = "test";
 for (const char* text : {"", "(p120)", "(p120) (v50)", "hgrunt/", "hgrunt/ clik", "clik"}) {
  sentenceText = const_cast<char*>(text); channel_t channel;
  int before = loads;
  bool hasWord = nullptr != std::strstr(text, "clik");
  assert((hasWord ? &cache : nullptr) == VOX_LoadSound(&channel, name));
  assert(before + hasWord == loads);
  if (!hasWord) { assert(-1 == channel.isentence); assert(nullptr == channel.sfx); }
 }
 char directory[32];
 for (const char* text : {"", " clik", "hgrunt/", "hgrunt/ clik", "clik(p120) clik"}) {
  char* sentence = new char[std::strlen(text) + 1]; std::strcpy(sentence, text);
  char* words = VOX_GetDirectory(directory, sentence, sizeof(directory));
  assert(0 == std::strcmp(directory, std::strchr(text, '/') ? "hgrunt/" : "vox/"));
  VOX_ParseString(words);
  int count = 0; voxword_t parameters;
  for (int i = 0; rgpparseword[i]; ++i) {
   int result = VOX_ParseWordParams(rgpparseword[i], &parameters, i == 0);
   if (!*rgpparseword[i]) { assert(0 == result); assert(100 == parameters.volume); }
   count += result;
  }
  assert((std::strstr(text, "clik(p120)") ? 2 : std::strstr(text, "clik") ? 1 : 0) == count);
  delete[] sentence;
 }
}
'''
    code = '#include <initializer_list>\n' + code
    with tempfile.TemporaryDirectory(prefix='goldsrc-vox-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code)
        compiler = shlex.split(os.environ.get('CXX', 'c++'))
        subprocess.run(compiler + ['-std=c++17', '-fsanitize=address,undefined',
                                  str(path / 'test.cpp'), '-o', str(path / 'test')], check=True)
        subprocess.run([str(path / 'test')], check=True)
    print('PASS: empty, leading-space, directory-only and normal VOX words')


if __name__ == '__main__':
    main()
