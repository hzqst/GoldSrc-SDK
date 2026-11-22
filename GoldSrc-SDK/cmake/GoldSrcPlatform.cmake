# Global compiler and linker settings shared by every GoldSrc target.
#
# Per-target differences (warning level, runtime library, language standard,
# ...) are expressed through goldsrc_add_module() in GoldSrcTargets.cmake.

# A project that embeds the SDK can build tier0, vgui2, GameUI and serverbrowser
# for Windows x64. The engine, launchers and games remain 32-bit only.
option(GOLDSRC_ALLOW_64BIT "Allow Windows x64 for the embeddable UI modules" OFF)

# The VGUI2 controls answer halflife-cli's "VGUI2.*" RequestInfo queries (HLND2T-DiligentGraphics, -hlcli).
option(GOLDSRC_USE_HLCLI "Build the halflife-cli automation hooks into the VGUI2 controls" OFF)
if(GOLDSRC_USE_HLCLI)
	add_compile_definitions(USE_HLCLI=1)
endif()

if(WIN32)
	if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND GOLDSRC_ALLOW_64BIT)
		set(GOLDSRC_PLATFORM_DIR x64)
	elseif(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
		message(FATAL_ERROR "GoldSrc targets Win32 (x86) only. Configure with -A Win32.")
	else()
		set(GOLDSRC_PLATFORM_DIR Win32)
	endif()
else()
	if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
		message(FATAL_ERROR "GoldSrc targets 32-bit x86 only. Configure with "
			"-DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-i686.cmake (see CMakePresets.json).")
	endif()
	set(GOLDSRC_PLATFORM_DIR Linux32)
endif()

# Runtime binaries land in bin/<platform>/<config>/..., matching the layout of
# the former Visual Studio projects. Import libraries, PDBs and other linker
# by-products go to bin/obj/<target>/<platform>/<config>/. An embedding project
# may set both beforehand to keep build output out of the source tree.
if(NOT DEFINED GOLDSRC_BIN_DIR)
	set(GOLDSRC_BIN_DIR "${GOLDSRC_ROOT}/bin/${GOLDSRC_PLATFORM_DIR}/$<CONFIG>")
endif()
if(NOT DEFINED GOLDSRC_OBJ_DIR)
	set(GOLDSRC_OBJ_DIR "${GOLDSRC_ROOT}/bin/obj")
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(MSVC)
	# The per-target RUNTIME option selects /MT or /MD; CMake adds the "d" suffix.
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

	# Exception handling and Release optimization are chosen per target by
	# goldsrc_add_module(); drop CMake's defaults so they are not overridden.
	string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	foreach(lang IN ITEMS C CXX)
		string(REPLACE "/O2" "" CMAKE_${lang}_FLAGS_RELEASE "${CMAKE_${lang}_FLAGS_RELEASE}")
	endforeach()

	add_compile_definitions(
		WIN32
		_WINDOWS
		_CRT_SECURE_NO_WARNINGS
		_CRT_NONSTDC_NO_WARNINGS
		_CRT_NONSTDC_NO_DEPRECATE
		_WINSOCK_DEPRECATED_NO_WARNINGS
		$<$<CONFIG:Debug>:_DEBUG>
	)
	add_compile_options(
		/MP
		/Zi
		$<$<CONFIG:Release>:/Oi>
		$<$<CONFIG:Release>:/Gy>
	)
	add_link_options(
		/DEBUG
		$<$<CONFIG:Release>:/OPT:REF>
		$<$<CONFIG:Release>:/OPT:ICF>
		$<$<CONFIG:Release>:/INCREMENTAL:NO>
	)
else()
	add_compile_definitions(
		POSIX
		_POSIX
		LINUX
		_LINUX
		GNUC
	)
	add_compile_options(
		# Match the x87 floating point used by the historical Linux builds.
		-march=pentium-m
		-mfpmath=387
		-fno-strict-aliasing
		-g
		"SHELL:-include \"${GOLDSRC_ROOT}/common/posix_compat.h\""
		-Werror=return-type
		# Like MSVC (and Valve's original Linux makefiles), accept string literals
		# passed through ternaries to the many char* engine and game APIs.
		$<$<COMPILE_LANGUAGE:CXX>:-fpermissive>
	)
	set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)
endif()
