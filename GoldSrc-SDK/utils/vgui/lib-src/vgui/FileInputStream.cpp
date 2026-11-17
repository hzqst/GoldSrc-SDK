#include "VGUI_FileInputStream.h"

#include <stdio.h>

using namespace vgui;

FileInputStream::FileInputStream( const char* fileName, bool textMode )
{
#if defined( _MSC_VER )
#pragma warning( push )
#pragma warning( disable : 4996 )
#endif
	if ( textMode )
	{
		_fp = fopen( fileName, "rt" );
	}
	else
	{
		_fp = fopen( fileName, "rb" );
	}
#if defined( _MSC_VER )
#pragma warning( pop )
#endif
}

void FileInputStream::seekStart( bool& success )
{
	if ( _fp == NULL )
	{
		success = false;
		return;
	}

	success = fseek( _fp, SEEK_SET, 0 ) != 0;
}

void FileInputStream::seekRelative( int count, bool& success )
{
	if ( _fp == NULL )
	{
		success = false;
		return;
	}

	success = fseek( _fp, SEEK_CUR, count ) != 0;
}

void FileInputStream::seekEnd( bool& success )
{
	if ( _fp == NULL )
	{
		success = false;
		return;
	}

	success = fseek( _fp, SEEK_END, 0 ) != 0;
}

int FileInputStream::getAvailable( bool& success )
{
	success = false;
	return 0;
}

uchar FileInputStream::readUChar( bool& success )
{
	if ( _fp == NULL )
	{
		success = false;
		return 0;
	}

	uchar ret;

	success = fread( &ret, 1, 1, _fp ) == 1;
	return ret;
}

void FileInputStream::readUChar( uchar* buf, int count, bool& success )
{
	if ( _fp == NULL )
	{
		success = false;
		return;
	}

	success = fread( buf, count, 1, _fp ) == 1;
}

void FileInputStream::close( bool& success )
{
	FILE* fp = _fp;
	_fp = NULL;

	if ( fp == NULL )
	{
		success = false;
		return;
	}

	success = fclose( fp ) == 0;
}

void FileInputStream::close()
{
	bool success;

	close( success );
}
