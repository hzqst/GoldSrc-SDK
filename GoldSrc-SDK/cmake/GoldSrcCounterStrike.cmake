# Shared settings for the Counter-Strike family of game modules (cstrike,
# czero and czeror), which build from one source tree per game with the same
# compiler settings.

include_guard(GLOBAL)

# goldsrc_add_counterstrike_module(<target>
#   SOURCE_DIRECTORY <dir>   Game source tree (cstrike or czeror).
#   GAME_DIRECTORY <dir>     Runtime game directory (cstrike, czero, czeror).
#   BINARY_DIRECTORY <dlls|cl_dlls>
#   OUTPUT_NAME <name>
#   [LINUX_OUTPUT_NAME <name>]
#   [WINDOWS_ONLY]
#   [CXX_STANDARD <14|17>]    Default 17.
#   [NO_ENGINE_ASSERTS]       Do not define UTL_ENGINE_ASSERTS.
#   [WHOLE_PROGRAM]
#   [MSVC_OPTIONS ...]
#   SOURCES ...)
function(goldsrc_add_counterstrike_module target)
	cmake_parse_arguments(PARSE_ARGV 1 arg "WINDOWS_ONLY;NO_ENGINE_ASSERTS;WHOLE_PROGRAM"
		"SOURCE_DIRECTORY;GAME_DIRECTORY;BINARY_DIRECTORY;OUTPUT_NAME;LINUX_OUTPUT_NAME;CXX_STANDARD"
		"MSVC_OPTIONS;SOURCES")

	set(src "${arg_SOURCE_DIRECTORY}")
	set(includes
		${src}/engine
		${src}/dlls
		${src}/dlls/bot
		${src}/dlls/hostage
		${src}/cl_dll
		${src}/cl_dll/hl
		${src}/cl_dll/VGUI
		${src}/common
		${src}/pm_shared
		${src}/game_shared
		${src}/game_shared/bot
		${src}/public/vgui_controls
		${src}/public
		${src}/public/vgui
		${src}/public/game_controls
		${src}/vgui2/game_controls
		dlls
		common
		pm_shared
		game_shared
		game_shared/bot
		public
		engine
		vgui2/controls
		vgui2/src/public
		external
	)
	set(defines CLIENT_WEAPONS)
	set(windows_defines _ITERATOR_DEBUG_LEVEL=0)
	set(linux_defines)
	set(link)
	set(windows_link)
	set(linux_link)

	if(arg_BINARY_DIRECTORY STREQUAL "dlls")
		list(APPEND defines VALVE_DLL)
	else()
		list(APPEND defines CLIENT_DLL HL_DLL)
		# The C++ client code defines these; unlike MSVC, GCC does not mangle
		# C++ variable names, so the C definitions would clash.
		list(APPEND linux_defines DISABLE_JUMP_ORIGIN DISABLE_VEC_ORIGIN)
		list(APPEND link goldsrc::SDL2)
		list(APPEND linux_link ${CMAKE_DL_LIBS})
		if(src STREQUAL "cstrike")
			list(PREPEND includes external/steamworks)
			list(APPEND link goldsrc::steam_api)
		elseif(src STREQUAL "czeror")
			list(APPEND windows_link ws2_32)
		endif()
	endif()
	if(NOT arg_NO_ENGINE_ASSERTS)
		list(APPEND windows_defines UTL_ENGINE_ASSERTS)
	endif()

	set(cxx_standard 17)
	if(arg_CXX_STANDARD)
		set(cxx_standard ${arg_CXX_STANDARD})
	endif()

	set(options)
	if(arg_WINDOWS_ONLY)
		list(APPEND options WINDOWS_ONLY)
	endif()
	if(arg_WHOLE_PROGRAM)
		list(APPEND options WHOLE_PROGRAM)
	endif()
	if(arg_LINUX_OUTPUT_NAME)
		list(APPEND options LINUX_OUTPUT_NAME ${arg_LINUX_OUTPUT_NAME})
	endif()

	goldsrc_add_module(${target}
		TYPE SHARED
		${options}
		OUTPUT_NAME ${arg_OUTPUT_NAME}
		OUTPUT_DIR ${arg_GAME_DIRECTORY}/${arg_BINARY_DIRECTORY}
		CXX_STANDARD ${cxx_standard}
		WARNING_LEVEL 4
		WARNINGS_AS_ERRORS
		LINKER_WARNINGS_AS_ERRORS
		KEEP_ASSERTS
		DEF_FILE ${arg_SOURCE_DIRECTORY}/${target}.def
		SOURCES ${arg_SOURCES}
		INCLUDES ${includes}
		DEFINES ${defines}
		WINDOWS_DEFINES ${windows_defines}
		LINUX_DEFINES ${linux_defines}
		LINK ${link}
		WINDOWS_LINK ${windows_link}
		LINUX_LINK ${linux_link}
		MSVC_OPTIONS ${arg_MSVC_OPTIONS}
	)
endfunction()
