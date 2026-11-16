"""Verify production callback storage on the host; set CXX='c++ -m32' for x86."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK/engine'


def main():
    header = (ROOT / 'sound.h').read_text()
    start = header.index('typedef struct sfxcache_s')
    cache_type = header[start:header.index('} sfxcache_t;', start) + len('} sfxcache_t;')]
    store = next(line for line in (ROOT / 'voice_sound_engine_interface.cpp').read_text().splitlines()
                 if 'pCache->loopstart =' in line)
    recover = next(line for line in (ROOT / 'snd_mix.cpp').read_text().splitlines()
                   if 'soundDataCallback_t callback =' in line)
    code = '#include <stdint.h>\n#include <cassert>\nusing byte=unsigned char;\n' + cache_type
    code += r'''
int VoiceSE_GetSoundDataCallback(sfxcache_t*, char*, int, int, int) { return 42; }
using soundDataCallback_t = decltype(&VoiceSE_GetSoundDataCallback);
int main() {
 sfxcache_t storage = {}; auto* pCache = &storage; auto* cache = pCache;
'''
    code += store + '\n' + recover
    code += r'''
 assert(&VoiceSE_GetSoundDataCallback == callback);
 assert(42 == callback(cache, nullptr, 0, 0, 0));
 cache->loopstart = -1; assert(cache->loopstart < 0);
 cache->loopstart = 27; assert(27 == cache->loopstart);
}
'''
    with tempfile.TemporaryDirectory(prefix='goldsrc-callback-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code)
        compiler = shlex.split(os.environ.get('CXX', 'c++'))
        subprocess.run(compiler + ['-std=c++17', '-Wall', '-Wextra', '-Werror',
                                  str(path / 'test.cpp'), '-o', str(path / 'test')], check=True)
        subprocess.run([str(path / 'test')], check=True)
    print('PASS: voice callback pointer round-trip and ordinary loop sentinels')


if __name__ == '__main__':
    main()
