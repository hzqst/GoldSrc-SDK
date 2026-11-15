# HLTV console launcher.

goldsrc_add_module(HLTV
	TYPE EXECUTABLE
	OUTPUT_NAME hltv
	CXX_STANDARD 17
	WARNING_LEVEL 4
	GUI
	WARNINGS_AS_ERRORS
	LINKER_WARNINGS_AS_ERRORS
	KEEP_ASSERTS
	LARGE_ADDRESS_AWARE
	EXPORTS
		CreateInterface,@1
	SOURCES
		HLTV/Console/System.cpp
		HLTV/common/common.cpp
		HLTV/common/random.cpp
		HLTV/Core/private/common/BaseSystemModule.cpp
		common/ObjectList.cpp
		common/TokenLine.cpp
		engine/interface.cpp
	WINDOWS_SOURCES
		# VGUI2 console window (Windows only).
		engine/KeyValues.cpp
		engine/utlbuffer.cpp
		engine/utlsymbol.cpp
		engine/MemPool.cpp
		engine/controls.cpp
		vgui2/controls/controls.cpp
		vgui2/controls/Panel.cpp
		vgui2/controls/Tooltip.cpp
		vgui2/controls/TextEntry.cpp
		vgui2/controls/Menu.cpp
		vgui2/controls/ScrollBar.cpp
		vgui2/controls/MenuItem.cpp
		vgui2/controls/TextImage.cpp
		vgui2/controls/ScrollBarSlider.cpp
		vgui2/controls/Label.cpp
		vgui2/controls/Button.cpp
		vgui2/controls/Image.cpp
		HLTV/Console/TextConsole.cpp
		HLTV/Console/TextConsoleWin32.cpp
		HLTV/Console/hltv.rc
	LINUX_SOURCES
		common/textconsole.cpp
		common/TextConsoleUnix.cpp
	INCLUDES
		HLTV/Core/private
		HLTV/Core/private/common
		HLTV/Console
		HLTV/common
		engine
		vgui2/src/public
		public
		common
		.
	DEFINES
		HLTV_CONSOLE
		CLIENT_DLL
		NOMINMAX
	WINDOWS_DEFINES
		UTL_ENGINE_ASSERTS
	LINUX_LINK
		${CMAKE_DL_LIBS}
		Threads::Threads
)
