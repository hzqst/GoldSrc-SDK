#pragma once

#ifdef _WIN32
#include <Windows.h>
#elif !defined( HMODULE ) // public/tier1/interface.h defines it as a macro
typedef void* HMODULE;
#endif

#if defined( _WIN32 )
typedef int BlobLoadMode_t;
#else
typedef char BlobLoadMode_t;
#endif

typedef struct BlobFootprint_s
{
	HMODULE m_hDll;
} BlobFootprint_t;

void*	NLoadBlobFile( const char* filename, BlobFootprint_t* footprint, void* context, BlobLoadMode_t fLoadAsDll );
void	FreeBlob( BlobFootprint_t* footprint );
