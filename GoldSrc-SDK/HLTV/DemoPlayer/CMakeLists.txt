# HLTV demo player module.

goldsrc_add_module(demoplayer
	TYPE SHARED
	OUTPUT_NAME DemoPlayer
	LINUX_OUTPUT_NAME demoplayer
	WARNING_LEVEL 4
	CHARSET MBCS
	WARNINGS_AS_ERRORS
	LINKER_WARNINGS_AS_ERRORS
	LARGE_ADDRESS_AWARE
	SOURCES
		HLTV/DemoPlayer/DemoPlayer.cpp
		HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp
		HLTV/common/byteorder.cpp
		HLTV/DemoPlayer/private/HLTV/common/common.cpp
		HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp
		HLTV/DemoPlayer/private/HLTV/common/mathlib.cpp
		HLTV/DemoPlayer/private/common/BaseSystemModule.cpp
		HLTV/DemoPlayer/private/common/ObjectDictionary.cpp
		common/ObjectList.cpp
		common/TokenLine.cpp
		HLTV/DemoPlayer/private/public/interface.cpp
	INCLUDES
		HLTV/DemoPlayer/private
		HLTV/DemoPlayer/private/common
		HLTV/DemoPlayer/private/HLTV/common
		HLTV/DemoPlayer
		HLTV/DemoPlayer/private/public
		HLTV/DemoPlayer/private/public/steam
		.
		common
	DEFINES
		_USE_MATH_DEFINES
	LINUX_LINK
		${CMAKE_DL_LIBS}
)
