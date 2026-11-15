// posix_compat.h: MSVC CRT names used throughout the code base, mapped onto
// their POSIX equivalents.
//
// Force-included into every translation unit on non-Windows platforms by the
// CMake build (see cmake/GoldSrcPlatform.cmake). Keep this header limited to
// macros for MSVC CRT, keyword and Windows SDK names: it is seen by every
// source file, so it must not declare types or pull in project headers.

#ifndef POSIX_COMPAT_H
#define POSIX_COMPAT_H

#ifndef _WIN32

#ifndef stricmp
#define stricmp strcasecmp
#endif
#ifndef _stricmp
#define _stricmp strcasecmp
#endif
#ifndef strcmpi
#define strcmpi strcasecmp
#endif
#ifndef strnicmp
#define strnicmp strncasecmp
#endif
#ifndef _strnicmp
#define _strnicmp strncasecmp
#endif
#ifndef _snprintf
#define _snprintf snprintf
#endif
#ifndef _vsnprintf
#define _vsnprintf vsnprintf
#endif
#ifndef _strdup
#define _strdup strdup
#endif
#ifndef _alloca
#define _alloca alloca
#endif
#ifndef _snwprintf
#define _snwprintf swprintf
#endif
#ifndef _atoi64
#define _atoi64 atoll
#endif
#ifndef _unlink
#define _unlink unlink
#endif

// A macro rather than a typedef, so that "unsigned __int64" still works.
#ifndef __int64
#define __int64 long long
#endif
// cdecl is already the default calling convention on 32-bit x86.
#ifndef __cdecl
#define __cdecl
#endif

// Same definition as winnt.h and public/commonmacros.h.
#ifndef ARRAYSIZE
#define ARRAYSIZE(p) (sizeof(p)/sizeof(p[0]))
#endif

#endif // !_WIN32

#endif // POSIX_COMPAT_H
