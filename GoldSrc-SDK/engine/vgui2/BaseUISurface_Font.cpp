#include "BaseUISurface_Font.h"

#include "../controls.h"
#include "../filesystem.h"
#include "../vgui/Dar.h"
#include "../../vgui2/controls/controls.h"
#include "../../vgui2/vgui_surfacelib/FontManager.h"

#include <climits>
#include <cstring>

static const int BITMAP_GLYPH_COUNT = 256;
static_assert( sizeof( VFontData::m_CharWidths ) / sizeof( int ) == BITMAP_GLYPH_COUNT, "One width per bitmap glyph" );

// Glyph lookups are single-byte: map sign-extended chars (-128..-1) back to 0x80..0xFF,
// then clamp anything else into the valid 0..BITMAP_GLYPH_COUNT-1 range.
static int NormalizeGlyphIndex( int ch )
{
	if ( ch < 0 && ch >= CHAR_MIN )
		ch = (unsigned char)ch;

	if ( ch < 0 )
		return 0;
	if ( ch >= BITMAP_GLYPH_COUNT )
		return BITMAP_GLYPH_COUNT - 1;
	return ch;
}

#pragma pack( push, 1 )
struct TGAFileHeader
{
	unsigned char m_IDLength;
	unsigned char m_ColorMapType;
	unsigned char m_ImageType;
	unsigned short m_CMapStart;
	unsigned short m_CMapLength;
	unsigned char m_CMapDepth;
	unsigned short m_XOffset;
	unsigned short m_YOffset;
	unsigned short m_Width;
	unsigned short m_Height;
	unsigned char m_PixelDepth;
	unsigned char m_ImageDescriptor;
};
#pragma pack( pop )

class FontPlatDar : public vgui2::Dar<BaseFontPlat*>
{
public:
	void setCount( int count )
	{
		if ( count >= 0 && count <= _capacity )
			_count = count;
	}
};

static int s_iFontCount;
static int staticFontId = 100;
static FontPlatDar staticFontPlatDar;

static void WriteRun( unsigned char* color, void* file, int runLength )
{
	unsigned char runCount = (unsigned char)( ( runLength - 1 ) | 0x80 );
	vgui2::filesystem()->Write( &runCount, 1, file );
	vgui2::filesystem()->Write( color, 4, file );
}

FileImageStream_Memory::FileImageStream_Memory( void* pData, int dataLen )
	: m_pData( (unsigned char*)pData ), m_DataLen( dataLen ), m_CurPos( 0 ), m_bError( false )
{
}

void FileImageStream_Memory::Read( void* pData, int len )
{
	unsigned char* out = (unsigned char*)pData;
	for ( int i = 0; i < len; i++ )
	{
		if ( m_CurPos < m_DataLen )
		{
			out[i] = m_pData[m_CurPos];
			m_CurPos++;
		}
		else
		{
			out[i] = 0;
			m_bError = true;
		}
	}
}

bool FileImageStream_Memory::ErrorStatus()
{
	bool error = m_bError;
	m_bError = false;
	return error;
}

#ifdef _MSC_VER
#pragma warning( suppress: 26495 )
#endif
VFontData::VFontData() : m_BitmapCharWidth( 0 ), m_BitmapCharHeight( 0 ), m_pBitmap( NULL )
{
}

VFontData::~VFontData()
{
	if ( m_pBitmap )
		delete[] m_pBitmap;
}

#ifdef _MSC_VER
#pragma warning( suppress: 26495 )
#endif
FontPlat::FontPlat( const char* name, int tall, int wide, float rotation, int weight, bool italic, bool underline,
	bool strikeout, bool symbol, int id )
	: _wide( wide ), _tall( tall ), _rotation( rotation ), _weight( weight ), _italic( italic ),
	  _underline( underline ), _strikeout( strikeout ), _symbol( symbol ), _id( id )
{
	_name = new char[strlen( name ) + 1];
	strcpy( _name, name );
}

FontPlat::~FontPlat()
{
	if ( _name )
		delete[] _name;
}

bool FontPlat::equals( const char* name, int tall, int wide, float rotation, int weight, bool italic, bool underline,
	bool strikeout, bool symbol )
{
	return !_stricmp( name, _name ) && _tall == tall && _wide == wide && _rotation == rotation && _weight == weight
		   && _italic == italic && _underline == underline && _strikeout == strikeout && _symbol == symbol;
}

void FontPlat::getCharRGBA( int, int, int, int, int, unsigned char* )
{
}

void FontPlat::GetCharABCwide( int, int&, int&, int& )
{
}

int FontPlat::GetTall()
{
	return 0;
}

void FontPlat::DrawSetTextFont( SurfacePlat* )
{
}

int FontPlat::getId()
{
	return _id;
}

#ifdef _MSC_VER
#pragma warning( suppress: 26495 )
#endif
FontPlat_Bitmap::FontPlat_Bitmap() : m_pName( NULL )
{
}

FontPlat_Bitmap::~FontPlat_Bitmap()
{
	if ( m_pName )
		delete[] m_pName;
}

FontPlat_Bitmap* FontPlat_Bitmap::Create( const char* name, FileImageStream* pStream, int id )
{
	FontPlat_Bitmap* bitmap = new FontPlat_Bitmap();
	if ( !bitmap )
		return NULL;

	if ( !LoadVFontDataFrom32BitTGA( pStream, &bitmap->m_FontData ) )
	{
		delete bitmap;
		return NULL;
	}

	bitmap->m_pName = new char[strlen( name ) + 1];
	if ( !bitmap->m_pName )
	{
		delete bitmap;
		return NULL;
	}

	strcpy( bitmap->m_pName, name );
	bitmap->_id = id;
	return bitmap;
}

bool FontPlat_Bitmap::equals( const char* name, int, int, float, int, bool, bool, bool, bool )
{
	return !_stricmp( name, m_pName );
}

void FontPlat_Bitmap::getCharRGBA( int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, unsigned char* rgba )
{
	const int chIndex = NormalizeGlyphIndex( ch );

	for ( int y = 0; y < m_FontData.m_BitmapCharHeight; y++ )
	{
		const int charWide = m_FontData.m_BitmapCharWidth;
		unsigned char* src = &m_FontData.m_pBitmap[y * charWide * 256 + charWide * chIndex];
		for ( int x = 0; x < charWide; x++ )
		{
			if ( rgbaWide <= rgbaX + x || rgbaTall <= y + rgbaY )
				continue;

			unsigned char* dst = &rgba[4 * ( ( rgbaY + y ) * rgbaWide + rgbaX + x )];
			unsigned char value = src[x] ? 0xFF : 0;
			dst[0] = value;
			dst[1] = value;
			dst[2] = value;
			dst[3] = value;
		}
	}
}

void FontPlat_Bitmap::GetCharABCwide( int ch, int& a, int& b, int& c )
{
	ch = NormalizeGlyphIndex( ch );

	c = 0;
	a = 0;
	b = m_FontData.m_CharWidths[ch] + 1;
}

int FontPlat_Bitmap::GetTall()
{
	return m_FontData.m_BitmapCharHeight;
}

void FontPlat_Bitmap::DrawSetTextFont( SurfacePlat* )
{
}

int FontPlat_Bitmap::getId()
{
	return _id;
}

EngineFont::EngineFont( const char* name, void* pFileData, int fileDataLen, int tall, int wide, float rotation,
	int weight, bool italic, bool underline, bool strikeout, bool symbol )
{
	Init( name, pFileData, fileDataLen, tall, wide, rotation, weight, italic, underline, strikeout, symbol );
	s_iFontCount++;
}

EngineFont::~EngineFont()
{
	if ( _name )
		delete[] _name;

	if ( --s_iFontCount == 0 )
		Font_Reset();
}

void EngineFont::Init( const char* name, void* pFileData, int fileDataLen, int tall, int wide, float rotation,
	int weight, bool italic, bool underline, bool strikeout, bool symbol )
{
	_name = new char[strlen( name ) + 1];
	strcpy( _name, name );
	_id = -1;
	_plat = NULL;

	if ( pFileData )
	{
		FileImageStream_Memory memStream( pFileData, fileDataLen );
		FontPlat_Bitmap* bitmap = FontPlat_Bitmap::Create( name, &memStream, staticFontId );
		if ( bitmap )
		{
			_id = staticFontId++;
			_plat = bitmap;
			staticFontPlatDar.AddElement( bitmap );
			return;
		}
		// Invalid bitmap data falls back to the system font below.
	}

	for ( int i = 0; i < staticFontPlatDar.GetCount(); i++ )
	{
		BaseFontPlat* plat = staticFontPlatDar[i];
		if ( plat->equals( name, tall, wide, rotation, weight, italic, underline, strikeout, symbol ) )
		{
			_plat = plat;
			_id = _plat->getId();
			break;
		}
	}

	if ( _plat )
		return;

	_id = staticFontId++;
	_plat = new FontPlat( name, tall, wide, rotation, weight, italic, underline, strikeout, symbol, _id );
	staticFontPlatDar.AddElement( _plat );
}

void EngineFont::getCharRGBA( int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, unsigned char* rgba )
{
	_plat->getCharRGBA( ch, rgbaX, rgbaY, rgbaWide, rgbaTall, rgba );
}

void EngineFont::GetCharABCwide( int ch, int& a, int& b, int& c )
{
	_plat->GetCharABCwide( ch, a, b, c );
}

void EngineFont::GetTextSize( const char* text, int& wide, int& tall )
{
	wchar_t wtext[2048];
	vgui2::localize()->ConvertANSIToUnicode( text, wtext, sizeof( wtext ) );

	FontManager().GetTextSize( 0, wtext, wide, tall );
}

int EngineFont::GetTall()
{
	return _plat->GetTall();
}

int EngineFont::getId()
{
	return _id;
}

BaseFontPlat* EngineFont::getPlat()
{
	return _plat;
}

bool Load32BitTGA( FileImageStream* fp, FileImage* pImage )
{
	TGAFileHeader hdr;
	char dummyChar;
	unsigned char packetHeader;
	unsigned int color;

	pImage->Term();
	fp->Read( &hdr, sizeof( hdr ) );
	if ( hdr.m_PixelDepth != 32 || hdr.m_ImageType != 10 )
		return false;

	for ( int i = 0; i < hdr.m_IDLength; i++ )
		fp->Read( &dummyChar, 1 );

	pImage->m_Width = hdr.m_Width;
	pImage->m_Height = hdr.m_Height;
	pImage->m_pData = new unsigned char[4 * pImage->m_Width * pImage->m_Height];
	if ( !pImage->m_pData )
		return false;

	for ( int y = pImage->m_Height - 1; y >= 0; y-- )
	{
		unsigned char* pLine = &pImage->m_pData[y * pImage->m_Width * 4];
		int curOut = 0;
		while ( curOut < pImage->m_Width )
		{
			fp->Read( &packetHeader, 1 );
			const int runLength = ( packetHeader & 0x7F ) + 1;
			if ( curOut + runLength > pImage->m_Width )
				return false;

			if ( packetHeader & 0x80 )
			{
				fp->Read( &color, 4 );
				for ( int x = 0; x < runLength; x++ )
				{
					memcpy( pLine, &color, sizeof( color ) );
					pLine += 4;
				}
			}
			else
			{
				for ( int x = 0; x < runLength; x++ )
				{
					fp->Read( &color, 4 );
					memcpy( pLine, &color, sizeof( color ) );
					pLine += 4;
				}
			}

			curOut += runLength;
		}
	}

	// A truncated stream pads its reads with zeros; reject it instead of returning a blank image.
	return !fp->ErrorStatus();
}

bool LoadVFontDataFrom32BitTGA( FileImageStream* fp, VFontData* pData )
{
	FileImage fileImage;
	memset( &fileImage, 0, sizeof( fileImage ) );

	bool success = false;
	// An image narrower than one pixel per glyph would load as a font of empty glyphs.
	if ( Load32BitTGA( fp, &fileImage ) && fileImage.m_Width >= BITMAP_GLYPH_COUNT && fileImage.m_Height > 0 )
	{
		pData->m_pBitmap = new unsigned char[fileImage.m_Height * fileImage.m_Width];
		if ( pData->m_pBitmap )
		{
			pData->m_BitmapCharWidth = fileImage.m_Width / 256;
			pData->m_BitmapCharHeight = fileImage.m_Height;

			const int charWide = pData->m_BitmapCharWidth;
			for ( int glyph = 0; glyph < 256; glyph++ )
			{
				int rightmost = 0;
				unsigned char* src = fileImage.m_pData + 4 * charWide * glyph;
				unsigned char* dst = pData->m_pBitmap + charWide * glyph;
				for ( int y = 0; y < pData->m_BitmapCharHeight; y++ )
				{
					for ( int x = 0; x < charWide; x++ )
					{
						unsigned int pixel = 0;
						memcpy( &pixel, src + 4 * x, sizeof( pixel ) );
						if ( !pixel )
						{
							dst[x] = 0;
						}
						else
						{
							dst[x] = 1;
							if ( rightmost < x )
								rightmost = x;
						}
					}

					src += 4 * 256 * charWide;
					dst += 256 * charWide;
				}

				if ( glyph == 32 )
					pData->m_CharWidths[glyph] = charWide / 4;
				else
					pData->m_CharWidths[glyph] = rightmost;
			}

			success = true;
		}
	}

	if ( fileImage.m_pData )
		delete[] fileImage.m_pData;
	return success;
}

void Save32BitTGA( void* fp, FileImage* pImage )
{
	TGAFileHeader hdr;
	memset( &hdr, 0, sizeof( hdr ) );
	hdr.m_PixelDepth = 32;
	hdr.m_ImageType = 10;
	hdr.m_Width = (unsigned short)pImage->m_Width;
	hdr.m_Height = (unsigned short)pImage->m_Height;

	vgui2::filesystem()->Write( &hdr, sizeof( hdr ), fp );

	for ( int y = pImage->m_Height - 1; y >= 0; y-- )
	{
		unsigned char* pLine = &pImage->m_pData[y * pImage->m_Width * 4];
		int runStart = 0;
		for ( int x = 0; x < pImage->m_Width; x++ )
		{
			unsigned int current, startPixel;
			memcpy( &current, pLine + x * 4, sizeof( current ) );
			memcpy( &startPixel, pLine + runStart * 4, sizeof( startPixel ) );
			if ( x - runStart >= 128 || current != startPixel )
			{
				WriteRun( pLine + runStart * 4, fp, x - runStart );
				runStart = x;
			}
		}

		if ( pImage->m_Width - runStart > 0 )
			WriteRun( pLine + runStart * 4, fp, pImage->m_Width - runStart );
	}
}

void Font_Reset()
{
	for ( int i = 0; i < staticFontPlatDar.GetCount(); i++ )
	{
		BaseFontPlat* plat = staticFontPlatDar[i];
		if ( plat )
			delete plat;
	}
	staticFontPlatDar.setCount( 0 );
}
