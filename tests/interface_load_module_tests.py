"""Load modules through the production public/interface.cpp Sys_LoadModule from a working directory getcwd cannot report.

Run: python tests/interface_load_module_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
Only POSIX builds resolve relative module names against the working directory.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'

TESTS = r'''
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>
#include "tier1/interface.h"

#define CHECK(condition) do { if (!(condition)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); std::exit(1); } } while (0)

// leave no terminator where Sys_LoadModule keeps its working directory buffer
static __attribute__((noinline)) void DirtyStack()
{
 volatile char filler[16384];
 for (size_t i = 0; i < sizeof(filler); ++i)
  filler[i] = 'A';
}

static std::string g_LibraryDirectory;
static std::string g_LibraryName;

static void FindLibrary()
{
 Dl_info info;
 double (*cosine)(double) = std::cos;
 CHECK(dladdr((void*)cosine, &info) && info.dli_fname);
 std::string path = info.dli_fname;
 size_t slash = path.rfind('/');
 CHECK(slash != std::string::npos);
 g_LibraryDirectory = path.substr(0, slash);
 g_LibraryName = path.substr(slash + 1);
}

static void RelativeNameLoadsFromTheWorkingDirectory()
{
 CHECK(0 == chdir(g_LibraryDirectory.c_str()));
 CSysModule* module = Sys_LoadModule(g_LibraryName.c_str());
 CHECK(module != NULL);
 Sys_UnloadModule(module);
 std::printf("PASS RelativeNameLoadsFromTheWorkingDirectory\n");
}

static void RemovedWorkingDirectoryFailsCleanly(const char* folder)
{
 std::string removed = std::string(folder) + "/removed";
 CHECK(0 == mkdir(removed.c_str(), 0700));
 CHECK(0 == chdir(removed.c_str()));
 CHECK(0 == rmdir(removed.c_str()));
 char probe[64];
 CHECK(getcwd(probe, sizeof(probe)) == NULL);
 DirtyStack();
 CHECK(Sys_LoadModule(g_LibraryName.c_str()) == NULL);
 std::printf("PASS RemovedWorkingDirectoryFailsCleanly\n");
}

int main(int, char** argv)
{
 FindLibrary();
 RelativeNameLoadsFromTheWorkingDirectory();
 RemovedWorkingDirectoryFailsCleanly(argv[1]);
 return 0;
}
'''


def main():
    if os.name == 'nt':
        print('SKIP: Windows LoadLibrary searches for relative module names itself')
        return
    with tempfile.TemporaryDirectory(prefix='goldsrc-interface-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(TESTS, encoding='utf-8')
        executable = compile_program([path / 'test.cpp', ROOT / 'public/interface.cpp'], path / 'test',
                                     includes=[ROOT / 'public'], defines=['_LINUX'],
                                     sanitizers=['address', 'undefined'])
        subprocess.run([str(executable), folder], check=True, cwd=path)


if __name__ == '__main__':
    main()
