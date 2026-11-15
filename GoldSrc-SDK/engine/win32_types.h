#pragma once

// The subset of <windows.h> types and constants used by the engine, for
// platforms that do not have it. Layouts match the Win32 definitions.

#if !defined( _WIN32 )

#include <cstdint>

typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int DWORD;
typedef int LONG;
typedef unsigned int ULONG;
typedef unsigned int UINT;
typedef long HRESULT;

typedef void* HANDLE;
typedef void* HWND;
typedef void* HINSTANCE;
typedef void* HDC;
typedef void* HGLRC;

// Calling conventions only matter on Windows.
#ifndef WINAPI
#define WINAPI
#endif

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

typedef struct tagRECT
{
	LONG left;
	LONG top;
	LONG right;
	LONG bottom;
} RECT;

typedef struct tagPOINT
{
	LONG x;
	LONG y;
} POINT;

#pragma pack( push, 2 )
typedef struct tagBITMAPFILEHEADER
{
	WORD bfType;
	DWORD bfSize;
	WORD bfReserved1;
	WORD bfReserved2;
	DWORD bfOffBits;
} BITMAPFILEHEADER;
#pragma pack( pop )

typedef struct tagBITMAPINFOHEADER
{
	DWORD biSize;
	LONG biWidth;
	LONG biHeight;
	WORD biPlanes;
	WORD biBitCount;
	DWORD biCompression;
	DWORD biSizeImage;
	LONG biXPelsPerMeter;
	LONG biYPelsPerMeter;
	DWORD biClrUsed;
	DWORD biClrImportant;
} BITMAPINFOHEADER;

typedef struct tagRGBQUAD
{
	BYTE rgbBlue;
	BYTE rgbGreen;
	BYTE rgbRed;
	BYTE rgbReserved;
} RGBQUAD;

typedef struct tagBITMAPINFO
{
	BITMAPINFOHEADER bmiHeader;
	RGBQUAD bmiColors[1];
} BITMAPINFO;

#define BI_RGB 0L

#pragma pack( push, 1 )
typedef struct tWAVEFORMATEX
{
	WORD wFormatTag;
	WORD nChannels;
	DWORD nSamplesPerSec;
	DWORD nAvgBytesPerSec;
	WORD nBlockAlign;
	WORD wBitsPerSample;
	WORD cbSize;
} WAVEFORMATEX;
#pragma pack( pop )

#define WAVE_FORMAT_PCM 1

static_assert( sizeof( BITMAPFILEHEADER ) == 14, "BITMAPFILEHEADER must match the file layout" );
static_assert( sizeof( BITMAPINFOHEADER ) == 40, "BITMAPINFOHEADER must match the file layout" );

#endif // !_WIN32
