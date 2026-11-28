# VGUI1 runtime.

# Not yet ported to Linux; Linux builds link the prebuilt linux/release/vgui.so.
goldsrc_add_module(vgui
	TYPE SHARED
	WINDOWS_ONLY
	WARNING_LEVEL 4
	RUNTIME STATIC
	DEF_FILE utils/vgui/vgui.def
	WARNINGS_AS_ERRORS
	LINKER_WARNINGS_AS_ERRORS
	SOURCES
		utils/vgui/lib-src/vgui/App.cpp
		utils/vgui/lib-src/vgui/Bitmap.cpp
		utils/vgui/lib-src/vgui/BitmapTGA.cpp
		utils/vgui/lib-src/vgui/Border.cpp
		utils/vgui/lib-src/vgui/BorderLayout.cpp
		utils/vgui/lib-src/vgui/BorderPair.cpp
		utils/vgui/lib-src/vgui/BuildGroup.cpp
		utils/vgui/lib-src/vgui/Button.cpp
		utils/vgui/lib-src/vgui/ButtonGroup.cpp
		utils/vgui/lib-src/vgui/CheckButton.cpp
		utils/vgui/lib-src/vgui/Color.cpp
		utils/vgui/lib-src/vgui/common/fileimage.cpp
		utils/vgui/lib-src/vgui/common/vfontdata.cpp
		utils/vgui/lib-src/vgui/ConfigWizard.cpp
		utils/vgui/lib-src/vgui/Cursor.cpp
		utils/vgui/lib-src/vgui/Dar.cpp
		utils/vgui/lib-src/vgui/DataInputStream.cpp
		utils/vgui/lib-src/vgui/DefaultCursor.cpp
		utils/vgui/lib-src/vgui/Desktop.cpp
		utils/vgui/lib-src/vgui/DesktopIcon.cpp
		utils/vgui/lib-src/vgui/EditPanel.cpp
		utils/vgui/lib-src/vgui/EtchedBorder.cpp
		utils/vgui/lib-src/vgui/Exports.cpp
		utils/vgui/lib-src/vgui/FileInputStream.cpp
		utils/vgui/lib-src/vgui/FlowLayout.cpp
		utils/vgui/lib-src/vgui/FocusNavGroup.cpp
		utils/vgui/lib-src/vgui/Font.cpp
		utils/vgui/lib-src/vgui/Frame.cpp
		utils/vgui/lib-src/vgui/HeaderPanel.cpp
		utils/vgui/lib-src/vgui/Image.cpp
		utils/vgui/lib-src/vgui/ImagePanel.cpp
		utils/vgui/lib-src/vgui/IntLabel.cpp
		utils/vgui/lib-src/vgui/Label.cpp
		utils/vgui/lib-src/vgui/Layout.cpp
		utils/vgui/lib-src/vgui/LineBorder.cpp
		utils/vgui/lib-src/vgui/ListPanel.cpp
		utils/vgui/lib-src/vgui/LoweredBorder.cpp
		utils/vgui/lib-src/vgui/Menu.cpp
		utils/vgui/lib-src/vgui/MenuItem.cpp
		utils/vgui/lib-src/vgui/MenuSeparator.cpp
		utils/vgui/lib-src/vgui/MessageBox.cpp
		utils/vgui/lib-src/vgui/MiniApp.cpp
		utils/vgui/lib-src/vgui/Panel.cpp
		utils/vgui/lib-src/vgui/PopupMenu.cpp
		utils/vgui/lib-src/vgui/ProgressBar.cpp
		utils/vgui/lib-src/vgui/RadioButton.cpp
		utils/vgui/lib-src/vgui/RaisedBorder.cpp
		utils/vgui/lib-src/vgui/Scheme.cpp
		utils/vgui/lib-src/vgui/ScrollBar.cpp
		utils/vgui/lib-src/vgui/ScrollPanel.cpp
		utils/vgui/lib-src/vgui/Slider.cpp
		utils/vgui/lib-src/vgui/StackLayout.cpp
		utils/vgui/lib-src/vgui/String.cpp
		utils/vgui/lib-src/vgui/Surface.cpp
		utils/vgui/lib-src/vgui/SurfaceBase.cpp
		utils/vgui/lib-src/vgui/TablePanel.cpp
		utils/vgui/lib-src/vgui/TabPanel.cpp
		utils/vgui/lib-src/vgui/TaskBar.cpp
		utils/vgui/lib-src/vgui/TextEntry.cpp
		utils/vgui/lib-src/vgui/TextGrid.cpp
		utils/vgui/lib-src/vgui/TextImage.cpp
		utils/vgui/lib-src/vgui/TextPanel.cpp
		utils/vgui/lib-src/vgui/ToggleButton.cpp
		utils/vgui/lib-src/vgui/TreeFolder.cpp
		utils/vgui/lib-src/vgui/UTF8Text.cpp
		utils/vgui/lib-src/vgui/vgui.cpp
		utils/vgui/lib-src/vgui/win32/App.cpp
		utils/vgui/lib-src/vgui/win32/Debug.cpp
		utils/vgui/lib-src/vgui/win32/Font.cpp
		utils/vgui/lib-src/vgui/win32/Surface.cpp
		utils/vgui/lib-src/vgui/WizardPanel.cpp
	INCLUDES
		utils/vgui/lib-src/vgui/include
		utils/vgui/lib-src/vgui
		utils/vgui/lib-src/vgui/common
	DEFINES
		VGUI_EXPORTS
	MSVC_LINK_OPTIONS
		/EXPORT:VguiEmptyString,@395,DATA,EXPORTAS,??_C@_00A@?$AA@
)

if(TARGET vgui)
	set(GOLDSRC_VGUI_IMPORTS_DEF "${CMAKE_CURRENT_SOURCE_DIR}/vgui_imports.def")
	if(NOT GOLDSRC_VGUI_ABI_COMPAT_MODE)
		# Exports the original vgui.dll does not have.
		set(GOLDSRC_VGUI_EXTRA_EXPORTS
			??1MiniApp@vgui@@UAE@XZ
			??1Image@vgui@@UAE@XZ
			??2Image@vgui@@SAPAXI@Z
			??3Image@vgui@@SAXPAX@Z
			??1Bitmap@vgui@@UAE@XZ
			??1Cursor@vgui@@UAE@XZ
			??2Cursor@vgui@@SAPAXI@Z
			??3Cursor@vgui@@SAXPAX@Z
			??1Scheme@vgui@@UAE@XZ
			??2Scheme@vgui@@SAPAXI@Z
			??3Scheme@vgui@@SAXPAX@Z
			??1BuildGroup@vgui@@UAE@XZ
			??2BuildGroup@vgui@@SAPAXI@Z
			??3BuildGroup@vgui@@SAXPAX@Z
			??0OwnScheme@App@vgui@@QAE@ABV012@@Z
			??1OwnScheme@App@vgui@@QAE@XZ
			??4OwnScheme@App@vgui@@QAEAAV012@ABV012@@Z
			??1Font@vgui@@UAE@XZ
			??2Font@vgui@@SAPAXI@Z
			??3Font@vgui@@SAXPAX@Z
			??1TextImage@vgui@@UAE@XZ
			??1Panel@vgui@@UAE@XZ
			??2Panel@vgui@@SAPAXI@Z
			??3Panel@vgui@@SAXPAX@Z
			??1Label@vgui@@UAE@XZ
			??1Button@vgui@@UAE@XZ
			??1TextEntry@vgui@@UAE@XZ
			??1TextPanel@vgui@@UAE@XZ
			??1TextGrid@vgui@@UAE@XZ
			??1EditPanel@vgui@@UAE@XZ
			??1CheckButton@vgui@@UAE@XZ
			??1RadioButton@vgui@@UAE@XZ
			??1WizardPage@WizardPanel@vgui@@UAE@XZ
			??1Frame@vgui@@UAE@XZ
			??1TabPanel@vgui@@UAE@XZ
			??1TablePanel@vgui@@UAE@XZ
			??1ActionSignal@vgui@@UAE@XZ
			??1ChangeSignal@vgui@@UAE@XZ
			??1FocusChangeSignal@vgui@@UAE@XZ
			??1FrameSignal@vgui@@UAE@XZ
			??1IntChangeSignal@vgui@@UAE@XZ
			??1TickSignal@vgui@@UAE@XZ
			??1FocusNavGroup@vgui@@UAE@XZ
			??1ButtonGroup@vgui@@UAE@XZ
			?removeButton@ButtonGroup@vgui@@QAEXPAVButton@2@@Z
			?removeTickSignal@App@vgui@@QAEXPAVTickSignal@2@@Z
			?frameDeleted@FrameSignal@vgui@@UAEXPAVFrame@2@@Z
			?removeFrame@TaskBar@vgui@@QAEXPAVFrame@2@@Z)
		file(READ "${GOLDSRC_VGUI_IMPORTS_DEF}" GOLDSRC_VGUI_IMPORTS)
		foreach(export IN LISTS GOLDSRC_VGUI_EXTRA_EXPORTS)
			string(APPEND GOLDSRC_VGUI_IMPORTS "    ${export}\n")
		endforeach()
		set(GOLDSRC_VGUI_IMPORTS_DEF "${CMAKE_CURRENT_BINARY_DIR}/vgui_imports.def")
		file(WRITE "${GOLDSRC_VGUI_IMPORTS_DEF}" "${GOLDSRC_VGUI_IMPORTS}")
	endif()

	# Rebuild the import library from the names-only definition so that clients
	# import by name, as the reference hw.dll and sw.dll do.
	add_custom_command(TARGET vgui POST_BUILD
		COMMAND "${CMAKE_AR}" /nologo /machine:x86 "/def:${GOLDSRC_VGUI_IMPORTS_DEF}"
			"/out:$<TARGET_LINKER_FILE:vgui>"
		COMMENT "Rebuilding the vgui import library from vgui_imports.def"
		VERBATIM)
	target_link_libraries(goldsrc_vgui1 INTERFACE vgui)
endif()
