# Stdio filesystem module.

goldsrc_add_module(filesystem_stdio
	TYPE SHARED
	WARNING_LEVEL 4
	CHARSET MBCS
	WARNINGS_AS_ERRORS
	LINKER_WARNINGS_AS_ERRORS
	EXPORTS
		CreateInterface
	SOURCES
		public/characterset.cpp
		filesystem/BaseFileSystem.cpp
		filesystem/dbg.cpp
		filesystem/FileSystem_Stdio.cpp
		filesystem/interface.cpp
		vgui2/src/public/utlsymbol.cpp
	LINUX_SOURCES
		filesystem/linux_support.cpp
		filesystem/pathmatch.cpp
	INCLUDES
		filesystem
	WINDOWS_DEFINES
		UTL_ENGINE_ASSERTS
	LINUX_LINK
		${CMAKE_DL_LIBS}
)
