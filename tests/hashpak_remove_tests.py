"""Exercise production HPAK removal with temporary multi-lump packages."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK/engine'


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <string>
using byte = unsigned char; using qboolean = int; using resourcetype_t = int;
using FileHandle_t = FILE*;
constexpr int MAX_PATH=260, MAX_QPATH=64, MD5_DIGEST_BYTES=16, TRUE=1, FALSE=0;
constexpr int HASHPAK_VERSION=1, MAX_HASHPAK_FILE_ENTRIES=32768, FILESYSTEM_SEEK_HEAD=SEEK_SET;
#define Q_memcmp std::memcmp
#define Q_memcpy std::memcpy
#define Q_strncmp std::strncmp
#define Q_strncpy std::strncpy
static const char* commandArg;
static const char* Cmd_Argv(int) {return commandArg;}
static void Con_Printf(const char*, ...) {}
static void HPAK_FlushHostQueue() {}
static int allocations;
static void* Mem_Malloc(size_t bytes) {++allocations;return std::malloc(bytes);}
static void Mem_Free(void* pointer) {--allocations;std::free(pointer);}
static FileHandle_t FS_Open(const char* name,const char* mode) {return std::fopen(name,mode);}
static void FS_Close(FileHandle_t file) {assert(0==std::fclose(file));}
static int FS_Read(void* output,int size,int,FileHandle_t file) {return std::fread(output,1,size,file);}
static void FS_Write(const void* input,int size,int,FileHandle_t file) {assert(size==int(std::fwrite(input,1,size,file)));}
static void FS_Seek(FileHandle_t file,int offset,int origin) {assert(0==std::fseek(file,offset,origin));}
static unsigned int FS_Tell(FileHandle_t file) {return std::ftell(file);}
static void FS_Unlink(const char* name) {assert(0==std::remove(name));}
static void FS_Rename(const char* from,const char* to) {assert(0==std::rename(from,to));}
static void COM_DefaultExtension(char* path,const char* extension) {if(!std::strrchr(path,'.'))std::strcat(path,extension);}
static void COM_StripExtension(const char* path,char* output) {std::strcpy(output,path);char* dot=std::strrchr(output,'.');if(dot)*dot=0;}
static void COM_CopyFileChunk(FileHandle_t output,FileHandle_t input,int bytes) {
 std::vector<char> data(bytes);assert(bytes==FS_Read(data.data(),bytes,1,input));FS_Write(data.data(),bytes,1,output);
}
'''

TESTS = r'''
static resource_t target;
static void Create(const char* name,int count) {
 auto* file=FS_Open(name,"wb");assert(file);
 hash_pack_header_t header={{'H','P','A','K'},HASHPAK_VERSION,0};
 FS_Write(&header,sizeof(header),1,file);
 std::vector<hash_pack_entry_t> entries(count);
 for(int i=0;i<count;++i) {
  auto& entry=entries[i];entry.nOffset=FS_Tell(file);entry.nFileLength=8;
  entry.resource.rgucMD5_hash[0]=byte(i+1);
  std::snprintf(entry.resource.szFileName,sizeof(entry.resource.szFileName),"lump%c.wad",'A'+i);
  const char payload[8]={char('A'+i),1,2,3,4,5,6,7};FS_Write(payload,sizeof(payload),1,file);
 }
 header.nDirectoryOffset=FS_Tell(file);FS_Write(&count,sizeof(count),1,file);
 FS_Write(entries.data(),sizeof(hash_pack_entry_t)*count,1,file);
 FS_Seek(file,0,SEEK_SET);FS_Write(&header,sizeof(header),1,file);FS_Close(file);
 target=entries[count==1?0:1].resource;
}
static void Check(const char* name,bool removed) {
 auto* file=FS_Open(name,"rb");assert(file);hash_pack_header_t header;
 assert(sizeof(header)==FS_Read(&header,sizeof(header),1,file));
 FS_Seek(file,header.nDirectoryOffset,SEEK_SET);int count=0;FS_Read(&count,sizeof(count),1,file);
 assert((removed?2:3)==count);
 std::vector<hash_pack_entry_t> entries(count);FS_Read(entries.data(),sizeof(hash_pack_entry_t)*count,1,file);
 for(int i=0;i<count;++i) {
  int original=removed && i==1?2:i;assert(original+1==entries[i].resource.rgucMD5_hash[0]);
  FS_Seek(file,entries[i].nOffset,SEEK_SET);char data[8];assert(8==FS_Read(data,8,1,file));
  assert(char('A'+original)==data[0]);for(int j=1;j<8;++j) assert(j==data[j]);
 }
 FS_Close(file);assert(0==allocations);
}
int main() {
 commandArg="!MD5b40d563346225b6fc371ba85fff5c82b";
 std::string decoy=std::string(commandArg)+".hpk";Create(decoy.c_str(),3);Create("custom.hpk",3);
 HPAK_RemoveLump("custom.hpk",&target);Check("custom.hpk",true);Check(decoy.c_str(),false);
 commandArg="custom";Create("custom.hpk",3);HPAK_RemoveLump("custom",&target);Check("custom.hpk",true);
 Create("custom.hpk",1);HPAK_RemoveLump("custom",&target);assert(nullptr==FS_Open("custom.hpk","rb"));
 HPAK_RemoveLump(nullptr,&target);HPAK_RemoveLump("",&target);HPAK_RemoveLump("custom",nullptr);
 assert(0==allocations);
}
'''


def main():
    source = (ROOT / 'hashpak.cpp').read_text()
    custom = (ROOT / 'custom.h').read_text()
    start = custom.index('typedef struct resource_s')
    resource = custom[start:custom.index('} resource_t;', start) + len('} resource_t;')]
    start = source.index('typedef struct hash_pack_entry_s')
    types = source[start:source.index('typedef struct hash_pack_queue_s', start)]
    code = STUBS + resource + types + '\nstatic hash_pack_header_t hash_pack_header;\n'
    code += function(source, 'qboolean HPAK_FindResource(')
    code += function(source, 'void HPAK_RemoveLump(') + TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-hpak-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code)
        executable = compile_program([path / 'test.cpp'], path / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=path)
    print('PASS: upload pakname, console default extension, surviving lump bytes and final-lump removal')


if __name__ == '__main__':
    main()
