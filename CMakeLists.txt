cmake_minimum_required(VERSION 3.24)
project(SengokuRTS LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "This prototype requires Windows and DirectX 12.")
endif()

add_library(sengoku_scene src/scene.cpp)
target_include_directories(sengoku_scene PUBLIC src)
target_compile_features(sengoku_scene PUBLIC cxx_std_20)
target_compile_definitions(sengoku_scene PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

add_executable(sengoku_rts WIN32 src/main.cpp src/renderer.cpp)
target_link_libraries(sengoku_rts PRIVATE sengoku_scene d3d12 dxgi d3dcompiler shell32)
add_custom_command(TARGET sengoku_rts POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:sengoku_rts>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/battlefield.hlsl"
        "$<TARGET_FILE_DIR:sengoku_rts>/shaders/battlefield.hlsl")
# HLSLは実行時にコンパイルする。編集時にもコピーを更新する。
set_property(TARGET sengoku_rts APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/battlefield.hlsl")

if(MSVC)
    target_compile_options(sengoku_scene PUBLIC /utf-8 PRIVATE /W4 /permissive-)
    target_compile_options(sengoku_rts PRIVATE /W4 /permissive- /utf-8)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(scene_tests tests/scene_tests.cpp)
    target_link_libraries(scene_tests PRIVATE sengoku_scene)
    add_test(NAME scene_invariants COMMAND scene_tests)
    add_test(NAME dx12_smoke COMMAND sengoku_rts --smoke-test --capture "${CMAKE_BINARY_DIR}/smoke.bmp")
    set_tests_properties(dx12_smoke PROPERTIES TIMEOUT 60)
endif()
