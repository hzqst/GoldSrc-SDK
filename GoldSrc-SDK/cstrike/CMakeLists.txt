# Counter-Strike and Counter-Strike: Condition Zero game servers and clients.
#
# Both games build from the cstrike source tree; only the runtime game
# directory differs.

include(GoldSrcCounterStrike)
include(cstrike_sources.cmake)

foreach(game IN ITEMS cstrike czero)
	# Not yet ported to Linux.
	goldsrc_add_counterstrike_module(${game}dll
		WINDOWS_ONLY
		SOURCE_DIRECTORY cstrike
		GAME_DIRECTORY ${game}
		BINARY_DIRECTORY dlls
		OUTPUT_NAME mp
		LINUX_OUTPUT_NAME cs
		WHOLE_PROGRAM
		SOURCES ${CSTRIKE_SERVER_SOURCES}
	)

	# Not yet ported to Linux.
	goldsrc_add_counterstrike_module(${game}_cdll
		WINDOWS_ONLY
		SOURCE_DIRECTORY cstrike
		GAME_DIRECTORY ${game}
		BINARY_DIRECTORY cl_dlls
		OUTPUT_NAME client
		SOURCES ${CSTRIKE_CLIENT_SOURCES}
	)
endforeach()
