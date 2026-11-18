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

foreach(name IN ITEMS hunk_temp_alloc sound_resample stream_sound spectate_connection serverbrowser_sort game_controls commandmenu viewport vgui_font_glyph vgui1_font_pages vgui1_font_fallback engine_font_fallback vgui1_surface_mode_info vgui1_dar vgui1_surface_fullscreen vgui1_surface_gdi
		hashpak_remove mixer_failure server_model voice_callback voice_wavefile vox waveout_failure)
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
