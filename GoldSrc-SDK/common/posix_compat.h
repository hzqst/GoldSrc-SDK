// posix_compat.h: MSVC CRT names used throughout the code base, mapped onto
// their POSIX equivalents.
//
// Force-included into every translation unit on non-Windows platforms by the
// CMake build (see cmake/GoldSrcPlatform.cmake). Keep this header limited to
// macros for CRT function names: it is seen by every source file, so it must
// not declare types or pull in project headers.

#ifndef POSIX_COMPAT_H
#define POSIX_COMPAT_H

#ifndef _WIN32

#ifndef stricmp
#define stricmp strcasecmp
#endif
#ifndef _stricmp
#define _stricmp strcasecmp
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

#endif // !_WIN32

#endif // POSIX_COMPAT_H
