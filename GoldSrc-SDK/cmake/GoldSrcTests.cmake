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
		baseui_shutdown
		bot_isintersecting2d
		bsp_pas
		cl_prediction_error
		client_joystick_hotplug
		client_statusbar_localise
		commandmenu
		common_bitstream_szprint
		common_memcmp_expand
		common_normalize_angles
		common_q_strcpy
		common_va
		cstrike_avatar_image
		cstrike_bot_chatter_leak
		cstrike_buypreset_editmenu
		cstrike_entity_classname_hash
		cstrike_knife_null_hit
		cstrike_scoreboard_columns
		cstrike_select_next_item
		cxx_toolchain
		czeror_material_file
		dedicated_mod_list
		delta_description_lookup
		demo_edit_commands
		dod_vgui2print
		engine_client_api
		engine_font_fallback
		engine_post_restart_args
		engine_save_grow
		engine_save_restore
		engine_stufftext
		file_read_length
		filesystem_pack_close
		func_tank_controller
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
		gameui_player_list
		gameui_save_command
		gameui_save_read
		gameui_start_game
		gameui_string_utils
		gl_sprite_format
		hashpak_remove
		host_profile
		hud_message_end3
		hunk_temp_alloc
		interface_load_module
		ip_rate_order
		keyvalues
		localize_construct
		localize_reload
		localize_save
		localize_set_value
		localize_utf8
		map_sprite_bmp_bounds
		mixer_failure
		monster_relationship_vehicle
		netapi_serverinfo
		netchan_file_decompress
		netchan_fragment_reassembly
		network_entity_bounds
		pack_dead_player_items
		parse_event_args
		particleman_allocation
		particleman_classname
		particleman_collideall
		particleman_collidekill
		particleman_flags
		particleman_forcelist
		particleman_processall
		particleman_visibility
		perf_counter_init
		playback_demo_commands
		scoreboard_selection
		sequence_info_null_model
		server_file_consistency
		server_model
		server_resource_list
		serverbrowser_filter_data
		serverbrowser_filter_string
		serverbrowser_join_game
		serverbrowser_last_played
		serverbrowser_password
		serverbrowser_sort
		serverinfo_mapcycle
		skeletal_model
		sound_msg
		sound_resample
		spectate_connection
		spectator_camera_waypoints
		spectator_chat_position
		spectator_command_menu
		spr_load_name
		static_analysis
		stream_sound
		studio_bounds_bone_anim
		studio_bounds_seqgroup
		sv_log_address
		sv_pmove_visibility
		teamplay_userinfo
		tfc_text_window
		transparent_entity
		utl_containers
		valve_node_graph
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
		vgui2_build_mode_dialog
		vgui2_child_panel
		vgui2_combobox_key
		vgui2_combobox_unicode
		vgui2_command_line_params
		vgui2_cross_module_parent
		vgui2_current_time_and_date
		vgui2_cursor_coordinates
		vgui2_custom_font
		vgui2_focus_nav_group
		vgui2_input_capture_context
		vgui2_input_modal_context
		vgui2_key_code_text
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
		vgui2_surface_window
		vgui2_system_registry
		vgui2_textentry_scroll
		vgui2_textimage
		vgui2_time_since_last_use
		vgui2_unicode_token
		vgui2_wide_copy
		vgui2_wizard_panel
		vgui_bmp_loading
		vgui_font_glyph
		vgui_modifier_sync
		viewport
		viewport_command_menu
		voice_callback
		voice_wavefile
		vox
		wad_miptex
		waveout_failure
		weapon_ammo_bounds
		weapon_prediction_ammo_bounds
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
