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

foreach(name IN ITEMS hunk_temp_alloc sound_resample stream_sound spectate_connection serverbrowser_sort serverbrowser_filter_data serverbrowser_join_game serverbrowser_filter_string serverbrowser_last_played game_controls commandmenu viewport vgui_font_glyph vgui1_font_pages vgui1_font_fallback engine_font_fallback vgui1_surface_mode_info vgui1_dar vgui1_miniapp vgui1_textimage vgui1_panel vgui1_client_ownership vgui1_widget_ownership vgui1_signal_ownership vgui1_surface_fullscreen vgui1_surface_gdi
		hashpak_remove mixer_failure server_model voice_callback voice_wavefile vox waveout_failure localize_utf8 localize_save gameui_save_command gameui_string_utils gameui_career_path gameui_save_read gameui_career_profile_key gameui_create_server gameui_music_manager gameui_keyboard_list gameui_options_command gameui_start_game gameui_career_command serverbrowser_password gameui_condump gameui_career_difficulty gameui_demo_events gameui_list_keyvalues dedicated_mod_list spectator_command_menu viewport_command_menu czeror_material_file tfc_text_window gameui_platform_modules gameui_logo_wad vgui2_textimage vgui2_menuitem_userdata vgui2_combobox_key vgui2_combobox_unicode vgui2_posix_clipboard vgui2_wide_copy localize_construct gameui_career_task_string dod_vgui2print vgui2_child_panel gameui_career_task_name vgui2_label_variable vgui2_sectioned_header vgui1_app_focus vgui1_cursor_bitmap vgui1_font_ownership vgui2_focus_nav_group vgui2_menu_scroll_range vgui2_richtext_scroll vgui2_textentry_scroll vgui2_cross_module_parent vgui1_string vgui2_listpanel_imagelist cstrike_avatar_image vgui1_focus_nav_group vgui1_button_group vgui1_build_group vgui1_scrollbar vgui2_propertydialog_tabs cxx_toolchain)
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
