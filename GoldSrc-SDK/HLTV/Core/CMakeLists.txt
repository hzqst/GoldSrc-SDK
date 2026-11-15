# HLTV core module.

goldsrc_add_module(Core
	TYPE SHARED
	LINUX_OUTPUT_NAME core
	CXX_STANDARD 17
	WARNING_LEVEL 4
	WARNINGS_AS_ERRORS
	LINKER_WARNINGS_AS_ERRORS
	EXPORTS
		CreateInterface,@1
	SOURCES
		HLTV/Core/BSPModel.cpp
		HLTV/Core/Core.cpp
		HLTV/Core/delta.cpp
		HLTV/Core/NetSocket.cpp
		HLTV/Core/Network.cpp
		HLTV/Core/Server.cpp
		HLTV/Core/World.cpp
		HLTV/common/BitBuffer.cpp
		HLTV/common/byteorder.cpp
		HLTV/common/common.cpp
		HLTV/common/DemoFile.cpp
		HLTV/common/DirectorCmd.cpp
		HLTV/common/InfoString.cpp
		HLTV/common/mathlib.cpp
		HLTV/common/md5.cpp
		HLTV/common/munge.cpp
		HLTV/common/NetAddress.cpp
		HLTV/common/NetChannel.cpp
		HLTV/common/random.cpp
		HLTV/Core/private/common/BaseSystemModule.cpp
		HLTV/Core/private/common/ObjectDictionary.cpp
		HLTV/Core/private/public/interface.cpp
		common/ObjectList.cpp
		common/TokenLine.cpp
		HLTV/Core/private/utils/bzip2/blocksort.c
		HLTV/Core/private/utils/bzip2/bzlib.c
		HLTV/Core/private/utils/bzip2/compress.c
		HLTV/Core/private/utils/bzip2/crctable.c
		HLTV/Core/private/utils/bzip2/decompress.c
		HLTV/Core/private/utils/bzip2/huffman.c
		HLTV/Core/private/utils/bzip2/randtable.c
	INCLUDES
		HLTV/Core/private
		HLTV/Core/private/public
		HLTV/Core/private/common
		HLTV/common
		launcher/public
		.
	DEFINES
		NOMINMAX
	WINDOWS_LINK
		ws2_32
	LINUX_LINK
		${CMAKE_DL_LIBS}
)

goldsrc_source_properties(HLTV/Core/private/utils/bzip2/bzlib.c
	MSVC_OPTIONS /wd6031
)
