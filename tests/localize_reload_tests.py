"""Exercise the production ReloadLocalizationFiles loop against the production file list update in AddFile."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


def span(source, first, last):
    start = source.index(first)
    return source[start:source.index(last, start) + len(last)]


def main():
    table = (ROOT / 'GoldSrc-SDK/vgui2/src/LocalizedStringTable.cpp').read_text(encoding='utf-8')
    code = r'''
#include <cassert>
#include <map>
#include <set>
#include <string>
#include <vector>

struct IFileSystem {};

template <class T>
struct CUtlVector {
 std::vector<T> items;
 int Count() const { return (int)items.size(); }
 T& operator[](int i) { return items[i]; }
 int AddToTail(const T& src) { items.push_back(src); return Count() - 1; }
 void Remove(int i) { items.erase(items.begin() + i); }
};

struct CUtlSymbol {
 std::string text;
 CUtlSymbol() {}
 CUtlSymbol(const char* value) : text(value) {}
 bool operator==(const CUtlSymbol& other) const { return text == other.text; }
 const char* String() const { return text.c_str(); }
};

// each stub file defines the same token; the last file read wins
static std::map<std::string, std::string> contents = {
 {"resource/valve_english.txt", "Settings"},
 {"resource/valve_french.txt", "Parametres"},
 {"resource/valve_german.txt", "Einstellungen"},
};

struct CLocalizedStringTable {
 CUtlVector<CUtlSymbol> m_LocalizationFiles;
 CUtlSymbol m_CurrentFile;
 std::vector<std::string> visits;
 std::set<std::string> missing;
 std::string token;
 bool AddFile(IFileSystem* fileSystem, const char* szFileName);
 void ReloadLocalizationFiles(IFileSystem* filesystem);
};

bool CLocalizedStringTable::AddFile(IFileSystem*, const char* szFileName) {
 const char* fileName = szFileName;
 visits.push_back(fileName);
 if (missing.count(fileName))
  return false;
'''
    code += span(table, 'm_CurrentFile = CUtlSymbol( fileName );',
                 'm_LocalizationFiles.AddToTail( m_CurrentFile );') + '\n'
    code += r'''
 token = contents.at(fileName);
 return true;
}
'''
    code += function(table, 'void CLocalizedStringTable::ReloadLocalizationFiles(') + '\n'
    code += r'''
static std::vector<std::string> names(CLocalizedStringTable& table) {
 std::vector<std::string> out;
 for (int i = 0; i < table.m_LocalizationFiles.Count(); i++)
  out.push_back(table.m_LocalizationFiles[i].String());
 return out;
}

int main() {
 IFileSystem fileSystem;
 const std::vector<std::string> pair = {"resource/valve_english.txt", "resource/valve_french.txt"};
 const std::vector<std::string> three = {"resource/valve_english.txt", "resource/valve_french.txt",
  "resource/valve_german.txt"};

 // the override file is read after the base file and its value survives the reload
 CLocalizedStringTable table;
 for (const std::string& name : pair)
  assert(table.AddFile(&fileSystem, name.c_str()));
 assert(pair == table.visits);
 assert(std::string("Parametres") == table.token);
 table.visits.clear();
 table.ReloadLocalizationFiles(&fileSystem);
 assert(pair == table.visits);
 assert(pair == names(table));
 assert(std::string("Parametres") == table.token);

 // a second reload keeps the same order
 table.visits.clear();
 table.ReloadLocalizationFiles(&fileSystem);
 assert(pair == table.visits);
 assert(pair == names(table));

 // every file is read once, in load order, for longer lists
 CLocalizedStringTable longer;
 for (const std::string& name : three)
  assert(longer.AddFile(&fileSystem, name.c_str()));
 longer.visits.clear();
 longer.ReloadLocalizationFiles(&fileSystem);
 assert(three == longer.visits);
 assert(three == names(longer));
 assert(std::string("Einstellungen") == longer.token);

 // a file that fails to open stays listed, and the others are still each read once
 longer.missing.insert("resource/valve_french.txt");
 longer.visits.clear();
 longer.ReloadLocalizationFiles(&fileSystem);
 assert(three == longer.visits);
 const std::vector<std::string> reordered = {"resource/valve_french.txt", "resource/valve_english.txt",
  "resource/valve_german.txt"};
 assert(reordered == names(longer));
 assert(std::string("Einstellungen") == longer.token);
}
'''
    with tempfile.TemporaryDirectory(prefix='goldsrc-localize-reload-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([path / 'test.cpp'], path / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True)
    print('PASS: ReloadLocalizationFiles reads each file once in load order')


if __name__ == '__main__':
    main()
