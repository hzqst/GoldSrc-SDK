# Global compiler and linker settings shared by every GoldSrc target.
#
# Per-target differences (warning level, runtime library, language standard,
# ...) are expressed through goldsrc_add_module() in GoldSrcTargets.cmake.

if(WIN32)
	if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
		message(FATAL_ERROR "GoldSrc targets Win32 (x86) only. Configure with -A Win32.")
	endif()
	set(GOLDSRC_PLATFORM_DIR Win32)
else()
	if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
		message(FATAL_ERROR "GoldSrc targets 32-bit x86 only. Configure with "
			"-DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-i686.cmake (see CMakePresets.json).")
	endif()
	set(GOLDSRC_PLATFORM_DIR Linux32)
endif()

# Runtime binaries land in bin/<platform>/<config>/..., matching the layout of
# the former Visual Studio projects. Import libraries, PDBs and other linker
# by-products go to bin/obj/<target>/<platform>/<config>/.
set(GOLDSRC_BIN_DIR "${GOLDSRC_ROOT}/bin/${GOLDSRC_PLATFORM_DIR}/$<CONFIG>")
set(GOLDSRC_OBJ_DIR "${GOLDSRC_ROOT}/bin/obj")

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
	)
	set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)
endif()
