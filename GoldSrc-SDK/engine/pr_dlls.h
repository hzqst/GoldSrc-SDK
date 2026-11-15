#pragma once

#ifdef _WIN32
#include <Windows.h>
#endif
#include <cstdint>

#define MAX_EXT_DLLS 50

struct entvars_s;

typedef void ( *ENTITYINIT )( entvars_s* );
typedef void ( *DISPATCHFUNCTION )( entvars_s*, void* );

typedef struct
{
	uint32_t pFunction;
	char* pFunctionName;
} functiontable_t;

typedef struct
{
	HINSTANCE lDLLHandle;
	functiontable_t* functionTable;
	int functionCount;
} extensiondll_t;

extern extensiondll_t g_rgextdll[MAX_EXT_DLLS];
extern int g_iextdllMac;

void				LoadEntityDLLs( char* basedir );
void				LoadThisDll( char* filename );
DISPATCHFUNCTION	GetDispatch( const char* name );
ENTITYINIT			GetEntityInit( const char* classname );
