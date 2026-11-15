# HLTV proxy module.

goldsrc_add_module(Proxy
	TYPE SHARED
	OUTPUT_NAME proxy
	CXX_STANDARD 17
	WARNING_LEVEL 4
	WARNINGS_AS_ERRORS
	LINKER_WARNINGS_AS_ERRORS
	KEEP_ASSERTS
	EXPORTS
		CreateInterface,@1
	SOURCES
		HLTV/common/BaseClient.cpp
		HLTV/common/BitBuffer.cpp
		HLTV/common/byteorder.cpp
		HLTV/common/common.cpp
		HLTV/common/DemoFile.cpp
		HLTV/common/DirectorCmd.cpp
		HLTV/common/InfoString.cpp
		HLTV/common/mathlib.cpp
		HLTV/common/munge.cpp
		HLTV/common/NetAddress.cpp
		HLTV/common/NetChannel.cpp
		HLTV/common/random.cpp
		HLTV/common/md5.cpp
		HLTV/Proxy/DemoClient.cpp
		HLTV/Proxy/hltv.cpp
		HLTV/Proxy/Master.cpp
		HLTV/Proxy/Proxy.cpp
		HLTV/Proxy/ProxyClient.cpp
		HLTV/Proxy/Status.cpp
		HLTV/Proxy/FakeClient.cpp
		HLTV/Core/private/common/BaseSystemModule.cpp
		HLTV/Core/private/common/ObjectDictionary.cpp
		common/ObjectList.cpp
		common/TokenLine.cpp
		HLTV/Core/private/public/interface.cpp
		HLTV/Director/Director.cpp
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
		HLTV/Director
		HLTV/common
		.
		game_shared
		public
	DEFINES
		NOMINMAX
	LINK
		goldsrc::steam_api
	WINDOWS_LINK
		ws2_32
	LINUX_LINK
		${CMAKE_DL_LIBS}
)

# Route bzip2's bz_internal_error to its built-in assert handler.
goldsrc_source_properties(
	HLTV/Core/private/utils/bzip2/blocksort.c
	HLTV/Core/private/utils/bzip2/bzlib.c
	HLTV/Core/private/utils/bzip2/compress.c
	HLTV/Core/private/utils/bzip2/crctable.c
	HLTV/Core/private/utils/bzip2/decompress.c
	HLTV/Core/private/utils/bzip2/huffman.c
	HLTV/Core/private/utils/bzip2/randtable.c
	DEFINES bz_internal_error=BZ2_bz__AssertH__fail
)
goldsrc_source_properties(HLTV/Core/private/utils/bzip2/bzlib.c
	MSVC_OPTIONS /wd6031
)
