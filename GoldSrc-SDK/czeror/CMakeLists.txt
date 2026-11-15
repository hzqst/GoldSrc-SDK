# Counter-Strike: Condition Zero - Deleted Scenes game server and client.

include(GoldSrcCounterStrike)
include(czeror_sources.cmake)

# Not yet ported to Linux.
goldsrc_add_counterstrike_module(czerordll
	WINDOWS_ONLY
	SOURCE_DIRECTORY czeror
	GAME_DIRECTORY czeror
	BINARY_DIRECTORY dlls
	OUTPUT_NAME cz
	CXX_STANDARD 14
	NO_ENGINE_ASSERTS
	SOURCES ${CZEROR_SERVER_SOURCES}
)

# Not yet ported to Linux.
goldsrc_add_counterstrike_module(czeror_cdll
	WINDOWS_ONLY
	SOURCE_DIRECTORY czeror
	GAME_DIRECTORY czeror
	BINARY_DIRECTORY cl_dlls
	OUTPUT_NAME client
	CXX_STANDARD 14
	MSVC_OPTIONS /wd4984
	SOURCES ${CZEROR_CLIENT_SOURCES}
)
