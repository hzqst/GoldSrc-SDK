# Game launcher.

goldsrc_add_module(launcher
	TYPE EXECUTABLE
	OUTPUT_NAME hl
	LINUX_OUTPUT_NAME hl_linux
	WARNING_LEVEL 4
	CHARSET MBCS
	DEF_FILE launcher/hl.def
	GUI
	WARNINGS_AS_ERRORS
	LINKER_WARNINGS_AS_ERRORS
	KEEP_ASSERTS
	WHOLE_PROGRAM
	LARGE_ADDRESS_AWARE
	SOURCES
		launcher/launcher.cpp
		launcher/common/commandline.cpp
		launcher/public/interface.cpp
		launcher/public/registry.cpp
	WINDOWS_SOURCES
		launcher/hl.rc
	INCLUDES
		launcher
	DEFINES
		WIN32_LEAN_AND_MEAN
		NOMINMAX
	WINDOWS_LINK
		ws2_32
	LINUX_LINK
		${CMAKE_DL_LIBS}
)
