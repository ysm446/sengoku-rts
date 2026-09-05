cmake_minimum_required(VERSION 3.24)
project(SengokuRTS LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "This prototype requires Windows and DirectX 12.")
endif()

enable_language(RC)
configure_file(src/app.rc.in "${CMAKE_CURRENT_BINARY_DIR}/app.rc" @ONLY)
set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/app.rc" PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/sengoku-rts-v2.ico")

add_library(sengoku_scene src/scene.cpp src/sprite_sheet.cpp src/simulation.cpp)
target_link_libraries(sengoku_scene PUBLIC windowscodecs ole32)
target_include_directories(sengoku_scene PUBLIC src)
target_compile_features(sengoku_scene PUBLIC cxx_std_20)
target_compile_definitions(sengoku_scene PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

add_executable(sengoku_rts WIN32 src/main.cpp src/renderer.cpp "${CMAKE_CURRENT_BINARY_DIR}/app.rc")
target_link_libraries(sengoku_rts PRIVATE sengoku_scene d3d12 dxgi d3dcompiler shell32)
add_custom_command(TARGET sengoku_rts POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:sengoku_rts>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/battlefield.hlsl"
        "$<TARGET_FILE_DIR:sengoku_rts>/shaders/battlefield.hlsl")
add_custom_target(sengoku_assets ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:sengoku_rts>/assets/sprites"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/sprites/ashigaru_idle.png"
        "$<TARGET_FILE_DIR:sengoku_rts>/assets/sprites/ashigaru_idle.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/sprites/ashigaru_walk.png"
        "$<TARGET_FILE_DIR:sengoku_rts>/assets/sprites/ashigaru_walk.png")
add_dependencies(sengoku_rts sengoku_assets)
# HLSLは実行時にコンパイルする。編集時にもコピーを更新する。
set_property(TARGET sengoku_rts APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/battlefield.hlsl")

if(MSVC)
    target_compile_options(sengoku_scene PUBLIC "$<$<COMPILE_LANGUAGE:CXX>:/utf-8>" PRIVATE /W4 /permissive-)
    target_compile_options(sengoku_rts PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/W4;/permissive-;/utf-8>")
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(simulation_tests tests/simulation_tests.cpp)
    target_link_libraries(simulation_tests PRIVATE sengoku_scene)
    add_test(NAME battle_simulation COMMAND simulation_tests)
    add_executable(scene_tests tests/scene_tests.cpp)
    target_link_libraries(scene_tests PRIVATE sengoku_scene)
    add_test(NAME scene_invariants COMMAND scene_tests "${CMAKE_CURRENT_SOURCE_DIR}/assets/sprites/ashigaru_idle.png")
    add_test(NAME dx12_smoke COMMAND sengoku_rts --smoke-test --capture "${CMAKE_BINARY_DIR}/smoke.bmp")
    set_tests_properties(dx12_smoke PROPERTIES TIMEOUT 60)
    add_test(NAME dx12_inspect COMMAND sengoku_rts --smoke-test --inspect --capture "${CMAKE_BINARY_DIR}/inspect.bmp")
    set_tests_properties(dx12_inspect PROPERTIES TIMEOUT 60)
    add_test(NAME dx12_motion COMMAND sengoku_rts --motion-test --capture "${CMAKE_BINARY_DIR}/motion.bmp")
    set_tests_properties(dx12_motion PROPERTIES TIMEOUT 60)
    add_test(NAME dx12_combat COMMAND sengoku_rts --combat-test --capture "${CMAKE_BINARY_DIR}/combat.bmp")
    set_tests_properties(dx12_combat PROPERTIES TIMEOUT 60)
endif()
