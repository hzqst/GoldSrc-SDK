"""Exercise the shared CUtlVector, CUtlRBTree, CUtlLinkedList, CUtlSymbolTable and CMemoryPool under vgui2/src/public.

Run: python tests/utl_containers_tests.py
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

TESTS = r'''
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include "utlvector.h"
#include "utlrbtree.h"
#include "utllinkedlist.h"
#include "utlsymbol.h"
#include "MemPool.h"

#define CHECK(condition) do { if (!(condition)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); std::exit(1); } } while (0)

static bool IntLess(const int& left, const int& right) { return left < right; }

static_assert(std::is_same<CUtlRBTree<int>::IndexType_t, unsigned short>::value, "the tree index defaults to unsigned short");
static_assert(std::is_same<CUtlLinkedList<int>::IndexType_t, int>::value, "the list index defaults to int");

static void VectorKeepsOrder()
{
 CUtlVector<int> vector;
 for (int i = 0; i < 100; ++i)
  CHECK(i == vector.AddToTail(i));
 CHECK(0 == vector.InsertBefore(0, -1));
 CHECK(101 == vector.Count());
 CHECK(-1 == vector[0]);
 CHECK(51 == vector.Find(50));
 CHECK(vector.HasElement(99));
 vector.FindAndRemove(50);
 CHECK(!vector.HasElement(50));
 vector.RemoveMultiple(0, 10);
 CHECK(90 == vector.Count());
 CHECK(9 == vector[0]);
 CHECK(-1 == vector.Find(1000));
 vector.Purge();
 CHECK(0 == vector.Count());
 std::printf("PASS VectorKeepsOrder\n");
}

static void TreeStaysSortedAndBalanced()
{
 CUtlRBTree<int> tree(0, 0, IntLess);
 const int count = 500;
 for (int i = 0; i < count; ++i)
  tree.Insert((i * 37) % count);
 CHECK(count == tree.Count());
 CHECK(tree.IsValid());
 int expected = 0;
 for (unsigned short index = tree.FirstInorder(); index != tree.InvalidIndex(); index = tree.NextInorder(index))
  CHECK(expected++ == tree[index]);
 CHECK(count == expected);
 for (unsigned short index = tree.LastInorder(); index != tree.InvalidIndex(); index = tree.PrevInorder(index))
  CHECK(--expected == tree[index]);
 for (int i = 0; i < count; i += 2)
  tree.RemoveAt(tree.Find(i));
 CHECK(count / 2 == tree.Count());
 CHECK(tree.IsValid());
 CHECK(tree.InvalidIndex() == tree.Find(10));
 CHECK(tree.InvalidIndex() != tree.Find(11));
 tree.RemoveAll();
 CHECK(0 == tree.Count());
 std::printf("PASS TreeStaysSortedAndBalanced\n");
}

static void TreeWithoutLessFuncFindsNothing()
{
 CUtlRBTree<int> tree;
 CHECK(tree.InvalidIndex() == tree.Find(1));
 std::printf("PASS TreeWithoutLessFuncFindsNothing\n");
}

static void ListLinksInOrder()
{
 CUtlLinkedList<int> list;
 int middle = 0;
 for (int i = 0; i < 10; ++i)
 {
  int index = list.AddToTail(i);
  if (i == 5)
   middle = index;
 }
 list.Remove(middle);
 CHECK(9 == list.Count());
 int expected = 0;
 for (int index = list.Head(); index != list.InvalidIndex(); index = list.Next(index))
 {
  if (expected == 5)
   ++expected;
  CHECK(expected++ == list[index]);
 }
 CHECK(8 == list[list.Previous(list.Tail())]);
 list.RemoveAll();
 CHECK(0 == list.Count());
 std::printf("PASS ListLinksInOrder\n");
}

static void SymbolsMapBothWays()
{
 CUtlSymbolTable table;
 CUtlSymbol first = table.AddString("Panel");
 CHECK(first.IsValid());
 CHECK((UtlSymId_t)first == (UtlSymId_t)table.AddString("Panel"));
 CHECK(!table.Find("panel").IsValid());
 CHECK(0 == std::strcmp("Panel", table.String(first)));
 CHECK(0 == std::strcmp("", table.String(CUtlSymbol())));

 CUtlSymbolTable caseless(0, 32, true);
 CUtlSymbol label = caseless.AddString("Label");
 CHECK((UtlSymId_t)label == (UtlSymId_t)caseless.Find("LABEL"));

 CUtlSymbol global("GlobalName");
 CHECK(global == "GlobalName");
 CHECK(!(global == "Other"));
 CHECK(0 == std::strcmp("GlobalName", global.String()));
 table.RemoveAll();
 CHECK(!table.Find("Panel").IsValid());
 std::printf("PASS SymbolsMapBothWays\n");
}

static void PoolReusesFreedBlocks()
{
 CMemoryPool pool(16, 4);
 void* blocks[64];
 for (int i = 0; i < 64; ++i)
 {
  blocks[i] = pool.Alloc(16);
  CHECK(blocks[i] != NULL);
  std::memset(blocks[i], i, 16);
 }
 for (int i = 0; i < 64; ++i)
  for (int j = i + 1; j < 64; ++j)
   CHECK(blocks[i] != blocks[j]);
 CHECK(NULL == pool.Alloc(17));
 pool.Free(blocks[10]);
 CHECK(blocks[10] == pool.Alloc(8));
 pool.Free(NULL);
 for (int i = 0; i < 64; ++i)
  pool.Free(blocks[i]);
 std::printf("PASS PoolReusesFreedBlocks\n");
}

int main()
{
 VectorKeepsOrder();
 TreeStaysSortedAndBalanced();
 TreeWithoutLessFuncFindsNothing();
 ListLinksInOrder();
 SymbolsMapBothWays();
 PoolReusesFreedBlocks();
 return 0;
}
'''


def main():
    public = ROOT / 'vgui2/src/public'
    with tempfile.TemporaryDirectory(prefix='goldsrc-utl-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(TESTS, encoding='utf-8')
        executable = compile_program([path / 'test.cpp', public / 'utlsymbol.cpp', public / 'MemPool.cpp'],
                                     path / 'test', includes=[public], sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=path)


if __name__ == '__main__':
    main()
