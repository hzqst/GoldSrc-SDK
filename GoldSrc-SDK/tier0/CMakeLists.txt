# Tier0 runtime library.

goldsrc_add_module(tier0
	TYPE SHARED
	LINUX_OUTPUT_NAME libtier0
	WARNING_LEVEL 4
	EXCEPTIONS ASYNC
	CHARSET MBCS
	DEF_FILE tier0/tier0.def
	WARNINGS_AS_ERRORS
	LINKER_WARNINGS_AS_ERRORS
	KEEP_ASSERTS
	SOURCES
		tier0/assert_dialog.cpp
		tier0/cpu.cpp
		tier0/cpu_posix.cpp
		tier0/dbg.cpp
		tier0/fasttimer.cpp
		tier0/mem.cpp
		tier0/mem_helpers.cpp
		tier0/memstd.cpp
		tier0/minidump.cpp
		tier0/platform_posix.cpp
		tier0/pme_posix.cpp
		tier0/pme.cpp
		tier0/testthread.cpp
		tier0/thread.cpp
		tier0/threadtools.cpp
		tier0/vcrmode_posix.cpp
		tier0/vprof.cpp
		tier0/validator.cpp
		tier0/valobject.cpp
	WINDOWS_SOURCES
		tier0/assert_dialog.rc
	INCLUDES
		public
		.
	DEFINES
		TIER0_DLL
		NOMINMAX
		WIN32_LEAN_AND_MEAN
	LINUX_LINK
		Threads::Threads
		${CMAKE_DL_LIBS}
)
