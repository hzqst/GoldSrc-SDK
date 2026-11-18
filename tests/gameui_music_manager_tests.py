"""Exercise the production GameUI MusicManager playlist loading and cd commands with stubs.

Run: python tests/gameui_music_manager_tests.py
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


def block(source, start_pattern):
    start = re.search(start_pattern, source, re.M).start()
    return source[start:source.index('\n};', start) + 3] + '\n'


STUBS = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#ifndef _WIN32
#include <limits.h>
#endif
#define override

namespace vgui2 { struct IFileSystem {}; IFileSystem* filesystem() { return nullptr; } }

// the playlist.txt that the next KeyValues::LoadFromFile reads
static std::map<std::string, std::string> g_playlist;

struct KeyValues {
 explicit KeyValues(const char*) {}
 bool LoadFromFile(vgui2::IFileSystem*, const char*) { values = g_playlist; return true; }
 const char* GetString(const char* key) {
  auto it = values.find(key);
  return it == values.end() ? "" : it->second.c_str();
 }
 int GetInt(const char* key, int defaultValue) {
  auto it = values.find(key);
  return it == values.end() ? defaultValue : atoi(it->second.c_str());
 }
 void deleteThis() { delete this; }
 std::map<std::string, std::string> values;
};

struct Engine {
 static void PrimeMusicStream(char* filename, int) { primed.push_back(filename); }
 // const so that the string literals in StopTrack compile under /permissive-
 static int ClientCmd(const char* command) { commands.push_back(command); return 0; }
 void (*pfnPrimeMusicStream)(char*, int) = PrimeMusicStream;
 int (*pfnClientCmd)(const char*) = ClientCmd;
 static std::vector<std::string> primed;
 static std::vector<std::string> commands;
};
std::vector<std::string> Engine::primed;
std::vector<std::string> Engine::commands;
static Engine g_engine;
static Engine* engine = &g_engine;

void Career_Printf(const char*, ...) {}
#include "StringUtils.h"
'''

TESTS = r'''
static const size_t PATH_SIZE = sizeof(((MusicManager::TrackInfo*)nullptr)->t_path);

static void Play(MusicManager& music, TrackType track, bool looping) {
 Engine::primed.clear();
 Engine::commands.clear();
 music.StartTrack(track, looping);
}

static void LongestPathIsPlayedWithoutTruncation() {
 const std::string path = "media/" + std::string(PATH_SIZE - 1 - 6, 'a');
 g_playlist = {{"Splash", path}};
 MusicManager music;
 music.Init();
 Play(music, TRACKTYPE_SPLASH, true);
 assert(1 == Engine::primed.size());
 assert(path == Engine::primed[0]);
 assert(1 == Engine::commands.size());
 // the command is longer than t_path and must still reach the engine whole
 assert("cd loopfile " + path + "\n" == Engine::commands[0]);
 puts("PASS: a path that fills t_path is played and its cd command is not truncated");
}

static void PathThatDoesNotFitIsNotPlayed() {
 for (size_t length : {PATH_SIZE, PATH_SIZE + 200}) {
  g_playlist = {{"Splash", std::string(length, 'a')}, {"Credits", "media/credits.mp3"}};
  MusicManager music;
  music.Init();
  Play(music, TRACKTYPE_SPLASH, true);
  assert(Engine::primed.empty());
  assert(Engine::commands.empty());
  // the other tracks are still loaded
  Play(music, TRACKTYPE_CREDITS, false);
  assert(1 == Engine::commands.size());
  assert(std::string("cd playfile media/credits.mp3\n") == Engine::commands[0]);
 }
 puts("PASS: a path that does not fit in t_path is dropped instead of truncated");
}

static void PathThatCouldAddCommandsIsNotPlayed() {
 for (const char* path : {"media/a.mp3;quit", "media/a.mp3\nquit", "media/a.mp3\rquit", "media/a\".mp3"}) {
  g_playlist = {{"Splash", path}, {"Credits", "media/credits.mp3"}};
  MusicManager music;
  music.Init();
  Play(music, TRACKTYPE_SPLASH, true);
  assert(Engine::primed.empty());
  assert(Engine::commands.empty());
  Play(music, TRACKTYPE_CREDITS, false);
  assert(1 == Engine::commands.size());
  assert(std::string("cd playfile media/credits.mp3\n") == Engine::commands[0]);
 }
 puts("PASS: a path with a command separator, quote or line break is not played");
}

static void EveryTrackIsLoadedWithItsLoopingDefault() {
 struct Case { TrackType track; const char* key; const char* command; };
 g_playlist = {{"Version", "2"}, {"Splash", "s"}, {"RoundWin", "rw"}, {"RoundLose", "rl"},
  {"MatchWin", "mw"}, {"MatchLose", "ml"}, {"Credits", "c"}, {"RoundWinLoop", "1"}, {"CreditsLoop", "0"}};
 MusicManager music;
 music.Init();
 for (const Case& c : {Case{TRACKTYPE_SPLASH, "s", "cd loopfile s\n"}, Case{TRACKTYPE_ROUNDWIN, "rw", "cd loopfile rw\n"},
   Case{TRACKTYPE_ROUNDLOSE, "rl", "cd playfile rl\n"}, Case{TRACKTYPE_MATCHWIN, "mw", "cd playfile mw\n"},
   Case{TRACKTYPE_MATCHLOSE, "ml", "cd playfile ml\n"}, Case{TRACKTYPE_CREDITS, "c", "cd playfile c\n"}}) {
  // the looping argument is replaced by the playlist value in version 2 playlists
  Play(music, c.track, false);
  assert(1 == Engine::commands.size());
  assert(std::string(c.command) == Engine::commands[0]);
  assert(std::string(c.key) == Engine::primed[0]);
 }
 puts("PASS: every playlist track is loaded with its looping key and default");
}

static void MissingTrackIsNotPlayed() {
 g_playlist = {};
 MusicManager music;
 music.Init();
 Play(music, TRACKTYPE_MATCHWIN, false);
 assert(Engine::commands.empty());
 puts("PASS: a track missing from the playlist is not played");
}
'''


def main():
    header = (ROOT / 'GameUI/MusicManager.h').read_text(encoding='utf-8')
    interface = (ROOT / 'common/GameUI/IMusicManager.h').read_text(encoding='utf-8')
    source = (ROOT / 'GameUI/MusicManager.cpp').read_text(encoding='utf-8')
    manager = block(header, r'^class MusicManager\b').replace(' : public IMusicManager', '').replace('private:', 'public:')
    code = STUBS + block(interface, r'^enum TrackType\b') + manager
    code += block(source, r'^static const struct\b')
    code += ''.join(function(source, signature) for signature in (
        'MusicManager::MusicManager(', 'MusicManager::~MusicManager(', 'void MusicManager::Reset(',
        'void MusicManager::Init(', 'void MusicManager::StartTrack(', 'void MusicManager::StopTrack('))
    code += TESTS
    code += ('\nint main() { LongestPathIsPlayedWithoutTruncation(); PathThatDoesNotFitIsNotPlayed(); PathThatCouldAddCommandsIsNotPlayed(); '
             'EveryTrackIsLoadedWithItsLoopingDefault(); MissingTrackIsNotPlayed(); }\n')
    with tempfile.TemporaryDirectory(prefix='goldsrc-gameui-music-manager-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([path / 'test.cpp', ROOT / 'GameUI/StringUtils.cpp'], path / 'test',
                                     includes=[ROOT / 'GameUI'], sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=path)


if __name__ == '__main__':
    main()
