# Dedicated server launcher.

# Windows-only: Win32 GUI front end.
goldsrc_add_module(hlds
	TYPE EXECUTABLE
	WINDOWS_ONLY
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
		dedicated/sys_ded.cpp
		dedicated/CreateMultiplayerGameServerPage.cpp
		dedicated/AdminServer.cpp
		dedicated/console/conproc.cpp
		common/textconsole.cpp
		common/TextConsoleWin32.cpp
		common/commandline.cpp
		common/netapi.cpp
		common/ObjectList.cpp
		engine/interface.cpp
		engine/mem.c
		engine/KeyValues.cpp
		engine/utlbuffer.cpp
		engine/utlsymbol.cpp
		engine/MemPool.cpp
		engine/controls.cpp
		vgui2/controls/controls.cpp
		vgui2/controls/AnimatingImagePanel.cpp
		vgui2/controls/BitmapImagePanel.cpp
		vgui2/controls/BuildGroup.cpp
		vgui2/controls/BuildModeDialog.cpp
		vgui2/controls/Button.cpp
		vgui2/controls/CheckButton.cpp
		vgui2/controls/ComboBox.cpp
		vgui2/controls/Divider.cpp
		vgui2/controls/EditablePanel.cpp
		vgui2/controls/FocusNavGroup.cpp
		vgui2/controls/Frame.cpp
		vgui2/controls/HTML.cpp
		vgui2/controls/Image.cpp
		vgui2/controls/ImageList.cpp
		vgui2/controls/ImagePanel.cpp
		vgui2/controls/Label.cpp
		vgui2/controls/ListPanel.cpp
		vgui2/controls/Menu.cpp
		vgui2/controls/MenuButton.cpp
		vgui2/controls/MenuItem.cpp
		vgui2/controls/MessageBox.cpp
		vgui2/controls/Panel.cpp
		vgui2/controls/PanelListPanel.cpp
		vgui2/controls/ProgressBar.cpp
		vgui2/controls/ProgressBox.cpp
		vgui2/controls/PropertyPage.cpp
		vgui2/controls/PropertySheet.cpp
		vgui2/controls/QueryBox.cpp
		vgui2/controls/RadioButton.cpp
		vgui2/controls/RichText.cpp
		vgui2/controls/ScrollBar.cpp
		vgui2/controls/ScrollBarSlider.cpp
		vgui2/controls/TextEntry.cpp
		vgui2/controls/TextImage.cpp
		vgui2/controls/ToggleButton.cpp
		vgui2/controls/Tooltip.cpp
		vgui2/controls/URLLabel.cpp
	WINDOWS_SOURCES
		dedicated/hlds.rc
	INCLUDES
		dedicated
		cstrike/common
		cstrike/public
		vgui2/controls
		cl_dll
		engine
		vgui2/src/public
		common
		public
		.
	DEFINES
		CLIENT_DLL
		HLDS_LAUNCHER
		UTL_ENGINE_ASSERTS
	LINK
		ws2_32
		winmm
	MSVC_LINK_OPTIONS
		/MANIFEST:NO
)

goldsrc_source_properties(engine/mem.c
	LANGUAGE CXX
)

# Text console front end for Linux (no VGUI administration window).
if(NOT WIN32)
	goldsrc_add_module(hlds_linux
		TYPE EXECUTABLE
		CXX_STANDARD 17
		KEEP_ASSERTS
		SOURCES
			dedicated/sys_ded_linux.cpp
			common/textconsole.cpp
			common/TextConsoleUnix.cpp
			common/commandline.cpp
			common/ObjectList.cpp
			engine/interface.cpp
			engine/mem.c
		INCLUDES
			dedicated
			engine
			common
			public
			.
		DEFINES
			HLDS_LAUNCHER
		LINK
			${CMAKE_DL_LIBS}
	)
endif()
