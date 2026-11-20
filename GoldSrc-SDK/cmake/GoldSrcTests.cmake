# Registers the standalone regression tests under ../tests with CTest.
#
# The Python tests extract production functions from the engine sources and
# compile them with the host compiler ($CXX); they do not use the build tree.

include_guard(GLOBAL)

set(GOLDSRC_TESTS_DIR "${GOLDSRC_ROOT}/../tests")

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_executable(vgui_game_controls_tests "${GOLDSRC_TESTS_DIR}/vgui_game_controls_tests.cpp")
target_include_directories(vgui_game_controls_tests PRIVATE "${GOLDSRC_ROOT}/engine" "${GOLDSRC_ROOT}/public")
set_target_properties(vgui_game_controls_tests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME vgui_game_controls COMMAND vgui_game_controls_tests)

# One test per line, in alphabetical order, so that tests added in parallel rarely conflict.
foreach(name IN ITEMS
		commandmenu
		common_q_strcpy
		cstrike_avatar_image
		cxx_toolchain
		czeror_material_file
		dedicated_mod_list
		dod_vgui2print
		engine_font_fallback
		filesystem_pack_close
		game_controls
		gameui_career_command
		gameui_career_difficulty
		gameui_career_path
		gameui_career_profile_key
		gameui_career_task_name
		gameui_career_task_string
		gameui_condump
		gameui_create_server
		gameui_demo_events
		gameui_keyboard_list
		gameui_list_keyvalues
		gameui_logo_wad
		gameui_music_manager
		gameui_options_command
		gameui_platform_modules
		gameui_save_command
		gameui_save_read
		gameui_start_game
		gameui_string_utils
		hashpak_remove
		hunk_temp_alloc
		localize_construct
		localize_reload
		localize_save
		localize_utf8
		mixer_failure
		parse_event_args
		server_model
		serverbrowser_filter_data
		serverbrowser_filter_string
		serverbrowser_join_game
		serverbrowser_last_played
		serverbrowser_password
		serverbrowser_sort
		sound_resample
		spectate_connection
		spectator_command_menu
		spr_load_name
		static_analysis
		stream_sound
		tfc_text_window
		vgui1_app_focus
		vgui1_build_group
		vgui1_button_group
		vgui1_client_ownership
		vgui1_cursor_bitmap
		vgui1_dar
		vgui1_focus_nav_group
		vgui1_font_fallback
		vgui1_font_ownership
		vgui1_font_pages
		vgui1_miniapp
		vgui1_panel
		vgui1_scrollbar
		vgui1_signal_ownership
		vgui1_string
		vgui1_surface_fullscreen
		vgui1_surface_gdi
		vgui1_surface_mode_info
		vgui1_textimage
		vgui1_widget_ownership
		vgui2_border_settings
		vgui2_child_panel
		vgui2_combobox_key
		vgui2_combobox_unicode
		vgui2_cross_module_parent
		vgui2_focus_nav_group
		vgui2_label_variable
		vgui2_listpanel_imagelist
		vgui2_menu_scroll_range
		vgui2_menuitem_userdata
		vgui2_posix_clipboard
		vgui2_propertydialog_tabs
		vgui2_propertysheet_tab_width
		vgui2_richtext_scroll
		vgui2_scheme_image_cache
		vgui2_scheme_reload
		vgui2_sectioned_header
		vgui2_textentry_scroll
		vgui2_textimage
		vgui2_unicode_token
		vgui2_wide_copy
		vgui_font_glyph
		viewport
		viewport_command_menu
		voice_callback
		voice_wavefile
		vox
		waveout_failure
	)
	add_test(NAME ${name}
		COMMAND Python3::Interpreter -I "${GOLDSRC_TESTS_DIR}/${name}_tests.py")
endforeach()

add_executable(q_ftol_tests "${GOLDSRC_TESTS_DIR}/q_ftol_tests.cpp")
set_target_properties(q_ftol_tests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
if(MSVC)
	target_compile_options(q_ftol_tests PRIVATE /W4 /WX)
endif()
add_test(NAME q_ftol COMMAND q_ftol_tests)

add_executable(cd_thread_tests "${GOLDSRC_TESTS_DIR}/cd_thread_tests.cpp")
set_target_properties(cd_thread_tests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
if(MSVC)
	target_compile_options(cd_thread_tests PRIVATE /EHsc /W4 /WX)
else()
	target_link_libraries(cd_thread_tests PRIVATE Threads::Threads)
endif()

set(cd_thread_scenarios lifecycle callback creation-failure join-failure)
if(WIN32)
	list(APPEND cd_thread_scenarios wake)
endif()
foreach(scenario IN LISTS cd_thread_scenarios)
	add_test(NAME cd_thread_${scenario} COMMAND cd_thread_tests ${scenario})
endforeach()

# The software and dedicated engines are linked with .def files that re-export
# VGUI1 symbols, some of them forwarded to vgui.dll. Check the built DLLs, so
# a VGUI header or vgui.def change that drops one of them is caught before the
# engine fails to load. Build sw_engine and swds before running CTest.
if(MSVC AND TARGET sw_engine AND TARGET swds)
	get_filename_component(GOLDSRC_MSVC_BIN_DIR "${CMAKE_LINKER}" DIRECTORY)
	find_program(GOLDSRC_DUMPBIN dumpbin HINTS "${GOLDSRC_MSVC_BIN_DIR}" REQUIRED)
	add_test(NAME engine_def_exports
		COMMAND Python3::Interpreter -I "${GOLDSRC_TESTS_DIR}/engine_def_exports_tests.py"
			--dumpbin "${GOLDSRC_DUMPBIN}"
			--module "$<TARGET_FILE:sw_engine>" "${GOLDSRC_ROOT}/engine/sw_engine.def"
			--module "$<TARGET_FILE:swds>" "${GOLDSRC_ROOT}/swds/swds.def")
endif()
