"""Compile the VGUI1 Cursor, Bitmap, Scheme, BuildGroup and App and check what they own (#209, #210).

Run: python tests/vgui1_cursor_bitmap_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
AddressSanitizer (including leak checks where supported) is enabled when the
compiler supports it; without it only the value checks run.

Cursor, DefaultCursor, Bitmap, BitmapTGA and BuildGroup are built with the
VGUI1 sources and platform stubs from vgui1_panel_tests.py, without its Cursor
stub, twice: with
VGUI_ABI_COMPAT_MODE=1 (the original vgui.dll layout, no destructors) and with
VGUI_ABI_COMPAT_MODE=0, where a default Cursor owns the bitmap it makes, a
Bitmap owns its pixels, a Scheme and a BuildGroup own the cursors they make,
and App owns the scheme it makes. The test replaces the global operator new
and delete to count the blocks still allocated. The compatible build frees
nothing, so its runs check memory errors only.

- app-reset: after App::reset the App has a scheme with the default colors.
- default-cursors: every default cursor has a bitmap and the hotspot of its
  bitmap.
- user-cursor: a user cursor keeps the bitmap and hotspot it is given.
With VGUI_ABI_COMPAT_MODE=0 only:
- default-cursor-delete: deleting a default cursor, dc_none included, frees
  the bitmap it made and its pixels.
- user-cursor-borrows: deleting a user cursor leaves its bitmap alone.
- cursor-copy: a copied or assigned default cursor has its own bitmap, a user
  cursor's copy uses the same bitmap, and all are freed once.
- bitmap-delete: deleting a Bitmap through Image* frees its pixels.
- bitmap-copy: a copied or assigned Bitmap has its own pixels with the same
  contents and uploads them again.
- tga-delete: deleting a loaded BitmapTGA, or one whose data ends early,
  frees its pixels.
- scheme-delete: deleting a Scheme frees the fonts and cursors it made, also
  ones that setFont and setCursor replaced, and leaves the font and cursor
  given to them alone.
- scheme-copy: a copied or assigned Scheme shows its own fonts and cursors
  where the other shows its own, and the other's given ones; assignment frees
  none, and all are freed once.
- build-group-delete: deleting a BuildGroup frees its cursors and panel
  names and leaves its panels alone, without a group (#222).
- build-group-copy: a copied or assigned BuildGroup has its own cursors; a
  copy starts with no panels and an assigned group keeps its own panels and
  names (#222). All are freed once.
- app-reset-restores: App::reset gives back the App's own scheme with the
  colors, fonts and cursors its constructor set, allocating nothing, so the
  cursor override still points to a live cursor.
- app-set-scheme: App::reset replaces a scheme given to setScheme with the
  App's own and leaves the given one alone.
- app-delete: deleting an App frees its scheme.
- app-copy: a copied App makes its own scheme and an assigned App keeps its
  own; each is freed once.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program
from vgui1_panel_tests import VGUI, vgui1_sources

CURSOR_STUB = '''// A default cursor without a bitmap, which ~Cursor can free.
Cursor::Cursor( DefaultCursor dc ) { privateInit( NULL, 0, 0 ); _dc = dc; }
'''
SOURCES = ['DefaultCursor.cpp', 'Bitmap.cpp', 'BitmapTGA.cpp', 'DataInputStream.cpp']

TESTS = r'''
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>

// The test reads the bitmaps and pixels the cursors and bitmaps keep.
#define private public
#define protected public
#include "VGUI_App.h"
#include "VGUI_Bitmap.h"
#include "VGUI_BitmapTGA.h"
#include "VGUI_BuildGroup.h"
#include "VGUI_Cursor.h"
#include "VGUI_Font.h"
#include "VGUI_InputStream.h"
#include "VGUI_Panel.h"
#include "VGUI_Scheme.h"
#undef protected
#undef private

using namespace vgui;

static int liveBlocks;

void* operator new( size_t size )
{
 void* memory = std::malloc( size != 0 ? size : 1 );
 if ( memory == NULL )
  throw std::bad_alloc();
 ++liveBlocks;
 return memory;
}

void* operator new[]( size_t size ) { return operator new( size ); }

void operator delete( void* memory ) noexcept
{
 if ( memory == NULL )
  return;
 --liveBlocks;
 std::free( memory );
}

void operator delete[]( void* memory ) noexcept { operator delete( memory ); }
void operator delete( void* memory, size_t ) noexcept { operator delete( memory ); }
void operator delete[]( void* memory, size_t ) noexcept { operator delete( memory ); }

class TestApp : public App
{
public:
 TestApp() : App( true ) {}
 virtual void main( int, char** ) override {}
};

class MemoryInputStream : public InputStream
{
public:
 MemoryInputStream( const uchar* data, int size ) : _data( data ), _size( size ), _pos( 0 ) {}
 virtual void seekStart( bool& success ) override { _pos = 0; success = true; }
 virtual void seekRelative( int count, bool& success ) override
 {
  success = _pos + count >= 0 && _pos + count <= _size;
  if ( success )
   _pos += count;
 }
 virtual void seekEnd( bool& success ) override { _pos = _size; success = true; }
 virtual int getAvailable( bool& success ) override { success = true; return _size - _pos; }
 virtual uchar readUChar( bool& success ) override
 {
  success = _pos < _size;
  return success ? _data[_pos++] : 0;
 }
 virtual void readUChar( uchar* buf, int count, bool& success ) override
 {
  success = _pos + count <= _size;
  if ( success )
  {
   memcpy( buf, _data + _pos, count );
   _pos += count;
  }
 }
 virtual void close( bool& success ) override { success = true; }

private:
 const uchar* _data;
 int _size;
 int _pos;
};

static const int CURSOR_SIZE = 16;
static const int CURSOR_HOTSPOT = CURSOR_SIZE / 2;
static const int USER_HOTSPOT_X = 3;
static const int USER_HOTSPOT_Y = 5;
static const int BITMAP_WIDE = 4;
static const int BITMAP_TALL = 2;
static const int TEXTURE_ID = 7;
static const uchar PIXEL = 200;

// A 2x1 24-bit uncompressed TGA.
static const uchar TGA[] = {
 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 1, 0, 24, 0,
 1, 2, 3, 4, 5, 6,
};
static const int TGA_TRUNCATED_SIZE = sizeof( TGA ) - 3;

static TestApp* mainApp;

static void Check(bool condition, const char* message)
{
 if (!condition)
  throw std::runtime_error(message);
}

static Bitmap* MakeBitmap()
{
 Bitmap* bitmap = new Bitmap();
 bitmap->setSize( BITMAP_WIDE, BITMAP_TALL );
 bitmap->setRGBA( 1, 1, PIXEL, 0, 0, PIXEL );
 return bitmap;
}

static void CheckDefaultColors( Scheme* scheme )
{
 int r = 0, g = 0, b = 0, a = 0;
 scheme->getColor( Scheme::sc_primary1, r, g, b, a );
 Check(102 == r && 102 == g && 153 == b && 0 == a, "default primary1 color");
}

#if VGUI_ABI_COMPAT_MODE != 1
// Deletes an App made for a test and makes the test App the instance again.
static void DeleteApp( App* other )
{
 delete other;
 App::_instance = mainApp;
}
#endif

static void CheckDefaultCursor( Cursor& cursor, Cursor::DefaultCursor dc )
{
 Check(dc == cursor.getDefaultCursor(), "default cursor kind");
 Check(cursor.getBitmap() != NULL, "default cursor bitmap");
 int x = 0, y = 0;
 cursor.getHotspot( x, y );
 Check(CURSOR_HOTSPOT == x && CURSOR_HOTSPOT == y, "default cursor hotspot");
}

static void Run(const char* scenario)
{
 const int baseline = liveBlocks;
 if (!std::strcmp(scenario, "app-reset"))
 {
  mainApp->getScheme()->setColor( Scheme::sc_primary1, 1, 2, 3, 4 );
  mainApp->reset();
  Check(mainApp->getScheme() != NULL, "reset scheme");
  CheckDefaultColors( mainApp->getScheme() );
 }
 else if (!std::strcmp(scenario, "default-cursors"))
 {
  for ( int dc = Cursor::dc_none; dc < Cursor::dc_last; ++dc )
  {
   Cursor* cursor = new Cursor( (Cursor::DefaultCursor)dc );
   CheckDefaultCursor( *cursor, (Cursor::DefaultCursor)dc );
   Check((dc == Cursor::dc_none) == (cursor->getBitmap()->_rgba == NULL), "default cursor pixels");
#if VGUI_ABI_COMPAT_MODE != 1
   delete cursor;
#endif
  }
 }
 else if (!std::strcmp(scenario, "user-cursor"))
 {
  Bitmap bitmap;
  Cursor cursor( &bitmap, USER_HOTSPOT_X, USER_HOTSPOT_Y );
  Check(&bitmap == cursor.getBitmap(), "user cursor bitmap");
  Check(Cursor::dc_user == cursor.getDefaultCursor(), "user cursor kind");
  int x = 0, y = 0;
  cursor.getHotspot( x, y );
  Check(USER_HOTSPOT_X == x && USER_HOTSPOT_Y == y, "user cursor hotspot");
 }
#if VGUI_ABI_COMPAT_MODE != 1
 else if (!std::strcmp(scenario, "default-cursor-delete"))
 {
  delete new Cursor( Cursor::dc_none );
  Check(baseline == liveBlocks, "dc_none cursor freed");
  delete new Cursor( Cursor::dc_arrow );
  Check(baseline == liveBlocks, "dc_arrow cursor, bitmap and pixels freed");
 }
 else if (!std::strcmp(scenario, "user-cursor-borrows"))
 {
  {
   Bitmap bitmap;
   bitmap.setSize( BITMAP_WIDE, BITMAP_TALL );
   delete new Cursor( &bitmap, USER_HOTSPOT_X, USER_HOTSPOT_Y );
   Check(bitmap._rgba != NULL, "stack bitmap kept its pixels");
   bitmap.setRGBA( 0, 0, PIXEL, PIXEL, PIXEL, PIXEL );
  }
  Check(baseline == liveBlocks, "stack bitmap freed its pixels");

  Bitmap* bitmap = MakeBitmap();
  Cursor* first = new Cursor( bitmap, 0, 0 );
  Cursor* second = new Cursor( bitmap, 0, 0 );
  delete first;
  Check(bitmap == second->getBitmap() && bitmap->_rgba != NULL, "shared bitmap kept");
  delete second;
  delete bitmap;
  Check(baseline == liveBlocks, "shared bitmap freed by its owner");
 }
 else if (!std::strcmp(scenario, "cursor-copy"))
 {
  Bitmap userBitmap;
  {
   Cursor original( Cursor::dc_arrow );
   Cursor copy( original );
   CheckDefaultCursor( copy, Cursor::dc_arrow );
   Check(original.getBitmap() != copy.getBitmap(), "copied default cursor has its own bitmap");

   Cursor assigned( Cursor::dc_ibeam );
   assigned = original;
   CheckDefaultCursor( assigned, Cursor::dc_arrow );
   Check(original.getBitmap() != assigned.getBitmap(), "assigned default cursor has its own bitmap");

   Cursor self( Cursor::dc_hand );
   Bitmap* selfBitmap = self.getBitmap();
   Cursor& alias = self;
   self = alias;
   Check(selfBitmap == self.getBitmap(), "self assignment keeps the bitmap");

   Cursor user( &userBitmap, USER_HOTSPOT_X, USER_HOTSPOT_Y );
   Cursor userCopy( user );
   Check(&userBitmap == userCopy.getBitmap() && Cursor::dc_user == userCopy.getDefaultCursor(),
         "copied user cursor uses the same bitmap");

   // A default cursor that takes a user cursor frees its own bitmap, and
   // a user cursor that takes a default cursor makes one.
   assigned = user;
   Check(&userBitmap == assigned.getBitmap(), "default cursor assigned a user cursor");
   userCopy = original;
   CheckDefaultCursor( userCopy, Cursor::dc_arrow );
   Check(original.getBitmap() != userCopy.getBitmap(), "user cursor assigned a default cursor");
  }
  Check(baseline == liveBlocks, "copied cursors freed once");
 }
 else if (!std::strcmp(scenario, "bitmap-delete"))
 {
  Image* image = MakeBitmap();
  delete image;
  Check(baseline == liveBlocks, "bitmap pixels freed");
  delete (Image*)new Bitmap();
  Check(baseline == liveBlocks, "empty bitmap freed");
 }
 else if (!std::strcmp(scenario, "bitmap-copy"))
 {
  {
   Bitmap* original = MakeBitmap();
   original->_id = TEXTURE_ID;
   original->_uploaded = true;

   Bitmap copy( *original );
   int wide = 0, tall = 0;
   copy.getSize( wide, tall );
   Check(BITMAP_WIDE == wide && BITMAP_TALL == tall, "copy size");
   Check(copy._rgba != NULL && copy._rgba != original->_rgba, "copy has its own pixels");
   Check(0 == memcmp( original->_rgba, copy._rgba, VGUI_RGBA_BYTES * wide * tall ), "copy pixels");
   Check(0 == copy._id && !copy._uploaded, "copy has its own texture");

   Bitmap assigned;
   assigned.setSize( 1, 1 );
   assigned._id = TEXTURE_ID + 1;
   assigned._uploaded = true;
   assigned = *original;
   Check(assigned._rgba != NULL && assigned._rgba != original->_rgba, "assigned bitmap has its own pixels");
   Check(0 == memcmp( original->_rgba, assigned._rgba, VGUI_RGBA_BYTES * wide * tall ), "assigned pixels");
   Check(TEXTURE_ID + 1 == assigned._id && !assigned._uploaded, "assigned bitmap uploads to its texture");

   uchar* pixels = assigned._rgba;
   Bitmap& alias = assigned;
   assigned = alias;
   Check(pixels == assigned._rgba, "self assignment keeps the pixels");

   Bitmap empty;
   assigned = empty;
   Check(assigned._rgba == NULL, "assigned an empty bitmap");

   delete original;
   Check(PIXEL == copy._rgba[VGUI_RGBA_BYTES * ( BITMAP_WIDE + 1 )], "copy outlives the original");
  }
  Check(baseline == liveBlocks, "copied bitmaps freed once");
 }
 else if (!std::strcmp(scenario, "tga-delete"))
 {
  MemoryInputStream stream( TGA, sizeof( TGA ) );
  Image* image = new BitmapTGA( &stream, false );
  int wide = 0, tall = 0;
  image->getSize( wide, tall );
  Check(2 == wide && 1 == tall, "loaded TGA size");
  delete image;
  Check(baseline == liveBlocks, "loaded TGA freed");

  MemoryInputStream truncated( TGA, TGA_TRUNCATED_SIZE );
  delete (Image*)new BitmapTGA( &truncated, false );
  Check(baseline == liveBlocks, "truncated TGA freed");
 }
 else if (!std::strcmp(scenario, "scheme-delete"))
 {
  delete new Scheme();
  Check(baseline == liveBlocks, "scheme fonts and cursors freed");

  Font givenFont( "given", NULL, 0, 0, 0, 0.0f, 0, false, false, false, false );
  Cursor given( Cursor::dc_arrow );
  const int givenBaseline = liveBlocks;
  Scheme* scheme = new Scheme();
  Cursor* replaced = scheme->getCursor( Scheme::scu_arrow );
  CheckDefaultCursor( *replaced, Cursor::dc_arrow );
  scheme->setFont( Scheme::sf_primary1, &givenFont );
  Check(&givenFont == scheme->getFont( Scheme::sf_primary1 ), "given font shown");
  scheme->setCursor( Scheme::scu_arrow, &given );
  Check(&given == scheme->getCursor( Scheme::scu_arrow ), "given cursor shown");
  CheckDefaultCursor( *replaced, Cursor::dc_arrow );
  delete scheme;
  Check(givenBaseline == liveBlocks, "replaced scheme font and cursor freed");
  CheckDefaultCursor( given, Cursor::dc_arrow );
 }
 else if (!std::strcmp(scenario, "scheme-copy"))
 {
  Cursor given( Cursor::dc_hand );
  Font givenFont( "given", NULL, 0, 0, 0, 0.0f, 0, false, false, false, false );
  const int givenBaseline = liveBlocks;
  {
   Scheme original;
   original.setCursor( Scheme::scu_hand, &given );
   original.setFont( Scheme::sf_primary3, &givenFont );

   Scheme copy( original );
   for ( int i = Scheme::scu_none; i < Scheme::scu_hand; ++i )
   {
    Cursor* cursor = copy.getCursor( (Scheme::SchemeCursor)i );
    Check(cursor != NULL && cursor != original.getCursor( (Scheme::SchemeCursor)i ), "copy has its own cursor");
    Check(original.getCursor( (Scheme::SchemeCursor)i )->getDefaultCursor() == cursor->getDefaultCursor(),
          "copy cursor kind");
   }
   Check(&given == copy.getCursor( Scheme::scu_hand ), "copy shows the given cursor");
   Check(copy.getFont( Scheme::sf_primary1 ) != NULL
         && original.getFont( Scheme::sf_primary1 ) != copy.getFont( Scheme::sf_primary1 ), "copy has its own font");
   Check(&givenFont == copy.getFont( Scheme::sf_primary3 ), "copy shows the given font");

   Scheme assigned;
   Font* ownFont = assigned.getFont( Scheme::sf_primary1 );
   Cursor* ownArrow = assigned.getCursor( Scheme::scu_arrow );
   Cursor* ownHand = assigned.getCursor( Scheme::scu_hand );
   assigned = original;
   Check(ownArrow == assigned.getCursor( Scheme::scu_arrow ), "assigned scheme keeps its own cursor");
   Check(ownFont == assigned.getFont( Scheme::sf_primary1 ), "assigned scheme keeps its own font");
   Check(&givenFont == assigned.getFont( Scheme::sf_primary3 ), "assigned scheme shows the given font");
   Check(&given == assigned.getCursor( Scheme::scu_hand ), "assigned scheme shows the given cursor");
   CheckDefaultCursor( *ownHand, Cursor::dc_hand );
   assigned = copy;
   Check(&given == assigned.getCursor( Scheme::scu_hand ), "assigned scheme shows the copy's given cursor");

   Scheme& alias = assigned;
   assigned = alias;
   Check(ownArrow == assigned.getCursor( Scheme::scu_arrow ), "self assignment keeps the cursors");
  }
  Check(givenBaseline == liveBlocks, "copied schemes freed once");
 }
 else if (!std::strcmp(scenario, "build-group-delete"))
 {
  Panel panel;
  const int panelBaseline = liveBlocks;
  BuildGroup* group = new BuildGroup();
  panel.setBuildGroup( group, "panel" );
  Check(1 == group->_panelNameDar.getCount(), "panel name kept");
  delete group;
  Check(NULL == panel._buildGroup, "the deleted group left its panel without a group");
  Check(panelBaseline == liveBlocks, "build group cursors and names freed");
 }
 else if (!std::strcmp(scenario, "build-group-copy"))
 {
  {
   Panel panel;
   BuildGroup original;
   panel.setBuildGroup( &original, "panel" );

   BuildGroup copy( original );
   Check(copy._cursor_sizeall != NULL && copy._cursor_sizeall != original._cursor_sizeall,
         "copy has its own cursors");
   Check(Cursor::dc_sizeall == copy._cursor_sizeall->getDefaultCursor(), "copy cursor kind");
   // The other group's panels point to it (#222).
   Check(0 == copy._panelDar.getCount() && 0 == copy._panelNameDar.getCount() && NULL == copy._currentPanel,
         "copy starts with no panels");

   BuildGroup assigned;
   Panel own;
   own.setBuildGroup( &assigned, "own" );
   Cursor* ownCursor = assigned._cursor_sizewe;
   assigned = original;
   Check(ownCursor == assigned._cursor_sizewe, "assigned group keeps its own cursors");
   Check(1 == assigned._panelDar.getCount() && &own == assigned._panelDar[0]
         && 0 == strcmp( "own", assigned._panelNameDar[0] ), "assigned group keeps its own panels and names");
   Check(1 == original._panelDar.getCount() && &panel == original._panelDar[0], "the other group keeps its panels");
   assigned = copy;
   Check(1 == assigned._panelNameDar.getCount(), "assigned again");

   BuildGroup& alias = assigned;
   assigned = alias;
   Check(ownCursor == assigned._cursor_sizewe && 1 == assigned._panelNameDar.getCount(),
         "self assignment keeps the cursors and names");
  }
  Check(baseline == liveBlocks, "copied build groups freed once");
 }
 else if (!std::strcmp(scenario, "app-reset-restores"))
 {
  Scheme* scheme = mainApp->getScheme();
  Font* font = scheme->getFont( Scheme::sf_primary1 );
  Cursor* arrow = scheme->getCursor( Scheme::scu_arrow );
  Font clientFont( "client", NULL, 0, 0, 0, 0.0f, 0, false, false, false, false );
  Cursor clientCursor( Cursor::dc_arrow );
  scheme->setColor( Scheme::sc_primary1, 1, 2, 3, 4 );
  scheme->setFont( Scheme::sf_primary1, &clientFont );
  scheme->setCursor( Scheme::scu_arrow, &clientCursor );
  mainApp->setCursorOveride( arrow );

  const int before = liveBlocks;
  mainApp->reset();
  mainApp->reset();
  Check(before == liveBlocks, "reset allocates nothing");
  Check(scheme == mainApp->getScheme(), "reset keeps the own scheme");
  CheckDefaultColors( scheme );
  Check(font == scheme->getFont( Scheme::sf_primary1 ), "reset gives back the default font");
  Check(arrow == scheme->getCursor( Scheme::scu_arrow ), "reset gives back the default cursor");
  Check(NULL == scheme->getCursor( Scheme::scu_user ) && NULL == scheme->getFont( Scheme::sf_user ),
        "user slots stay empty");
  CheckDefaultCursor( *mainApp->getCursorOveride(), Cursor::dc_arrow );
  mainApp->setCursorOveride( NULL );
 }
 else if (!std::strcmp(scenario, "app-set-scheme"))
 {
  Scheme* own = mainApp->getScheme();
  Scheme* given = new Scheme();
  mainApp->setScheme( given );
  Check(given == mainApp->getScheme(), "given scheme shown");
  mainApp->reset();
  Check(own == mainApp->getScheme(), "reset shows the own scheme");
  CheckDefaultCursor( *given->getCursor( Scheme::scu_arrow ), Cursor::dc_arrow );
  delete given;
  Check(baseline == liveBlocks, "given scheme freed by its owner");
 }
 else if (!std::strcmp(scenario, "app-delete"))
 {
  TestApp* other = new TestApp();
  Check(other->getScheme() == other->_ownScheme.scheme && other->getScheme() != mainApp->getScheme(),
        "an App makes its own scheme");
  DeleteApp( other );
  Check(baseline == liveBlocks, "App scheme freed");
 }
 else if (!std::strcmp(scenario, "app-copy"))
 {
  TestApp* other = new TestApp();
  TestApp* copy = new TestApp( *other );
  Check(copy->_ownScheme.scheme != NULL && copy->_ownScheme.scheme != other->_ownScheme.scheme,
        "copy makes its own scheme");
  Scheme* own = copy->_ownScheme.scheme;
  *copy = *other;
  Check(own == copy->_ownScheme.scheme, "assigned App keeps its own scheme");
  copy->reset();
  Check(own == copy->getScheme(), "copy reset shows its own scheme");
  DeleteApp( copy );
  DeleteApp( other );
  Check(baseline == liveBlocks, "copied App schemes freed once");
 }
#endif
 else Check(false, "unknown scenario");
}

int main(int argc, char** argv)
{
 // Static so that the scheme App never frees stays reachable for leak checks.
 static TestApp app;
 mainApp = &app;
 try
 {
  Check(argc == 2, "usage: test <scenario>");
  Run(argv[1]);
 }
 catch (const std::exception& error)
 {
  std::fprintf(stderr, "%s\n", error.what());
  return EXIT_FAILURE;
 }
 std::printf("PASS: %s\n", argv[1]);
 return EXIT_SUCCESS;
}
'''

SCENARIOS = ['app-reset', 'default-cursors', 'user-cursor']
NONCOMPAT_SCENARIOS = ['default-cursor-delete', 'user-cursor-borrows', 'cursor-copy', 'bitmap-delete',
                       'bitmap-copy', 'tga-delete', 'scheme-delete', 'scheme-copy', 'build-group-delete',
                       'build-group-copy', 'app-reset-restores', 'app-set-scheme', 'app-delete', 'app-copy']
# VGUI_ABI_COMPAT_MODE value -> scenarios run against that build.
MODES = {'1': SCENARIOS, '0': SCENARIOS + NONCOMPAT_SCENARIOS}


def main():
    failed = []
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui1-cursor-') as folder:
        test_file = Path(folder) / 'test.cpp'
        test_file.write_text(TESTS)
        sources = vgui1_sources(folder)
        # The real default Cursor constructor replaces the stub.
        stubs_file = sources[0]
        stubs = stubs_file.read_text()
        if CURSOR_STUB not in stubs:
            raise SystemExit('the platform stubs no longer match: ' + CURSOR_STUB)
        stubs_file.write_text(stubs.replace(CURSOR_STUB, ''))
        sources = [test_file] + sources + [VGUI / name for name in SOURCES]
        for mode, scenarios in MODES.items():
            executable = Path(folder) / f'test-compat{mode}.exe'
            # VGUI_EXPORTS matches the vgui target, so VGUIAPI is not dllimport on Windows.
            executable = compile_program(sources, executable, includes=[VGUI / 'include', VGUI],
                                         defines=['VGUI_EXPORTS', f'VGUI_ABI_COMPAT_MODE={mode}'], debug=True,
                                         sanitizers=['address'])
            for scenario in scenarios:
                # Without destructors the compatible build frees nothing; only
                # check its memory errors.
                env = dict(os.environ)
                if mode == '1':
                    env['ASAN_OPTIONS'] = 'detect_leaks=0:' + env.get('ASAN_OPTIONS', '')
                result = subprocess.run([str(executable), scenario], env=env)
                if result.returncode:
                    failed.append(f'{scenario} (VGUI_ABI_COMPAT_MODE={mode})')
    if failed:
        raise SystemExit('FAILED: ' + ', '.join(failed))


if __name__ == '__main__':
    main()
