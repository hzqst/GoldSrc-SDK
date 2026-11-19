"""Exercise production CBaseFileSystem::Close on pack entry, pack and loose file handles."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK/filesystem'


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


def handle_class(header):
    start = header.index('\tclass CFileHandle')
    end = header.index('\t};', start)
    # Count handle lifetimes through class allocation functions
    return header[start:end] + COUNTERS + header[end:end + 3]


COUNTERS = r'''
		static void* operator new(size_t bytes) {++liveHandles;return ::operator new(bytes);}
		static void operator delete(void* pointer) {--liveHandles;::operator delete(pointer);}
'''

STUBS = r'''
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>
using FileHandle_t = void*;
enum { FILESYSTEM_WARNING = 1 };
static int liveHandles, closedFiles, warnings;
struct FileList {
 std::vector<FILE*> items;
 int Find(FILE* file) const {auto it=std::find(items.begin(),items.end(),file);return it==items.end()?-1:int(it-items.begin());}
 void AddToTail(FILE* file) {items.push_back(file);}
 void FindAndRemove(FILE* file) {items.erase(std::remove(items.begin(),items.end(),file),items.end());}
};
class CBaseFileSystem {
public:
HANDLE_CLASS
 void Close(FileHandle_t file);
 void Trace_FClose(FILE* fp) {assert(fp);++closedFiles;}
 void Warning(int, const char*, ...) {++warnings;}
 FileList m_PackFileHandles;
};
'''

TESTS = r'''
static CBaseFileSystem::CFileHandle* NewHandle(FILE* file, bool pack) {
 CBaseFileSystem::CFileHandle* handle=new CBaseFileSystem::CFileHandle;handle->m_pFile=file;handle->m_bPack=pack;return handle;
}
int main() {
 CBaseFileSystem fs;
 FILE* pak=reinterpret_cast<FILE*>(0x1000);
 FILE* loose=reinterpret_cast<FILE*>(0x2000);
 CBaseFileSystem::CFileHandle* pack=NewHandle(pak,false);
 fs.m_PackFileHandles.AddToTail(pak);

 // Pack entries share the pack FILE*: the handle is freed, the FILE* stays open
 for(int i=0;i<3;++i) fs.Close(NewHandle(pak,true));
 assert(1==liveHandles);assert(0==closedFiles);

 // A pack entry closed after its pack was torn down must not close the FILE* again
 CBaseFileSystem::CFileHandle* late=NewHandle(pak,true);
 fs.m_PackFileHandles.FindAndRemove(pak);
 fs.Close(pack);
 assert(1==liveHandles);assert(1==closedFiles);
 fs.Close(late);
 assert(0==liveHandles);assert(1==closedFiles);

 // A loose file closes its FILE* and frees the handle
 fs.Close(NewHandle(loose,false));
 assert(0==liveHandles);assert(2==closedFiles);

 // A NULL handle only warns; a NULL FILE* warns and still frees the handle
 fs.Close(nullptr);
 fs.Close(NewHandle(nullptr,false));
 assert(2==warnings);assert(2==closedFiles);
 assert(0==liveHandles);
}
'''


def main():
    source = (ROOT / 'BaseFileSystem.cpp').read_text()
    header = (ROOT / 'basefilesystem.h').read_text()
    code = STUBS.replace('HANDLE_CLASS', handle_class(header))
    code += function(source, 'void CBaseFileSystem::Close( FileHandle_t file )') + TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-fs-close-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code)
        executable = compile_program([path / 'test.cpp'], path / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=path)
    print('PASS: Close frees pack entry, pack, loose and NULL FILE* handles, closing only owned FILE*s')


if __name__ == '__main__':
    main()
