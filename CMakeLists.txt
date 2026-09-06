cmake_minimum_required(VERSION 3.24)
project(SengokuRTS LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "This prototype requires Windows and DirectX 12.")
endif()

enable_language(RC)
configure_file(src/app.rc.in "${CMAKE_CURRENT_BINARY_DIR}/app.rc" @ONLY)
set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/app.rc" PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/sengoku-rts-v2.ico")

add_library(sengoku_scene src/scene.cpp src/sprite_sheet.cpp src/simulation.cpp src/soldier_visuals.cpp src/audio.cpp src/historical_scenario.cpp src/historical_scene.cpp)
target_link_libraries(sengoku_scene PUBLIC windowscodecs ole32 xaudio2 gdi32)
target_include_directories(sengoku_scene PUBLIC src)
target_compile_features(sengoku_scene PUBLIC cxx_std_20)
target_compile_definitions(sengoku_scene PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

add_executable(sengoku_rts WIN32 src/main.cpp src/renderer.cpp src/historical_window.cpp "${CMAKE_CURRENT_BINARY_DIR}/app.rc")
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
add_custom_command(TARGET sengoku_assets POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/sprites/ashigaru_attack.png"
        "$<TARGET_FILE_DIR:sengoku_rts>/assets/sprites/ashigaru_attack.png")
add_dependencies(sengoku_rts sengoku_assets)
add_custom_command(TARGET sengoku_assets POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:sengoku_rts>/assets/ui/emotions")
foreach(emotion motivation anxiety fear)
    add_custom_command(TARGET sengoku_assets POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/emotions/${emotion}.png"
            "$<TARGET_FILE_DIR:sengoku_rts>/assets/ui/emotions/${emotion}.png")
endforeach()
foreach(unit samurai archer cavalry)
    foreach(animation idle walk attack)
        add_custom_command(TARGET sengoku_assets POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/sprites/${unit}_${animation}.png"
                "$<TARGET_FILE_DIR:sengoku_rts>/assets/sprites/${unit}_${animation}.png")
    endforeach()
endforeach()
# HLSLは実行時にコンパイルする。編集時にもコピーを更新する。
set_property(TARGET sengoku_rts APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/battlefield.hlsl")

if(MSVC)
    target_compile_options(sengoku_scene PUBLIC "$<$<COMPILE_LANGUAGE:CXX>:/utf-8>" PRIVATE /W4 /permissive-)
    target_compile_options(sengoku_rts PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/W4;/permissive-;/utf-8>")
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(flank_tests tests/flank_tests.cpp)
    target_link_libraries(flank_tests PRIVATE sengoku_scene)
    add_test(NAME flank_deployment COMMAND flank_tests)
    add_executable(cavalry_tests tests/cavalry_tests.cpp)
    target_link_libraries(cavalry_tests PRIVATE sengoku_scene)
    add_test(NAME cavalry_combat COMMAND cavalry_tests)
    add_executable(central_relief_tests tests/central_relief_tests.cpp)
    target_link_libraries(central_relief_tests PRIVATE sengoku_scene)
    add_test(NAME central_relief COMMAND central_relief_tests)
    add_executable(emotion_tests tests/emotion_tests.cpp)
    target_link_libraries(emotion_tests PRIVATE sengoku_scene)
    add_test(NAME emotions COMMAND emotion_tests "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/emotions")
    add_executable(recovery_tests tests/recovery_tests.cpp)
    target_link_libraries(recovery_tests PRIVATE sengoku_scene)
    add_test(NAME recovery COMMAND recovery_tests)
    add_executable(ranged_tests tests/ranged_tests.cpp)
    target_link_libraries(ranged_tests PRIVATE sengoku_scene)
    add_test(NAME ranged_combat COMMAND ranged_tests)
    add_executable(melee_profile_tests tests/melee_profile_tests.cpp)
    target_link_libraries(melee_profile_tests PRIVATE sengoku_scene)
    add_test(NAME melee_profiles COMMAND melee_profile_tests)
    add_test(NAME dx12_sword_battle COMMAND sengoku_rts --sword-battle --combat-test --capture "${CMAKE_BINARY_DIR}/sword-battle.bmp")
    set_tests_properties(dx12_sword_battle PROPERTIES TIMEOUT 120)
    add_test(NAME dx12_mixed_battle COMMAND sengoku_rts --mixed-battle --combat-test --capture "${CMAKE_BINARY_DIR}/mixed-battle.bmp")
    set_tests_properties(dx12_mixed_battle PROPERTIES TIMEOUT 120)
    add_executable(formation_drill_tests tests/formation_drill_tests.cpp)
    target_link_libraries(formation_drill_tests PRIVATE sengoku_scene)
    add_test(NAME formation_drill COMMAND formation_drill_tests)
    add_test(NAME dx12_formation COMMAND sengoku_rts --smoke-test --formation --unit cavalry --march --capture "${CMAKE_BINARY_DIR}/formation.bmp")
    set_tests_properties(dx12_formation PROPERTIES TIMEOUT 60)
    add_executable(unit_assets_tests tests/unit_assets_tests.cpp)
    target_link_libraries(unit_assets_tests PRIVATE sengoku_scene)
    add_test(NAME unit_assets COMMAND unit_assets_tests "${CMAKE_CURRENT_SOURCE_DIR}/assets/sprites")
    foreach(unit samurai archer cavalry)
        add_test(NAME dx12_${unit} COMMAND sengoku_rts --smoke-test --unit ${unit} --inspect-attack --capture "${CMAKE_BINARY_DIR}/${unit}.bmp")
        set_tests_properties(dx12_${unit} PROPERTIES TIMEOUT 60)
    endforeach()
    add_executable(historical_tests tests/historical_tests.cpp)
    target_link_libraries(historical_tests PRIVATE sengoku_scene)
    add_test(NAME historical_scenario COMMAND historical_tests)
    add_test(NAME dx12_sekigahara COMMAND sengoku_rts --sekigahara --smoke-test --capture "${CMAKE_BINARY_DIR}/sekigahara.bmp")
    set_tests_properties(dx12_sekigahara PROPERTIES TIMEOUT 60)
    add_executable(contact_fronts_tests tests/contact_fronts_tests.cpp)
    target_link_libraries(contact_fronts_tests PRIVATE sengoku_scene)
    add_test(NAME contact_fronts COMMAND contact_fronts_tests)
    add_executable(audio_tests tests/audio_tests.cpp)
    target_link_libraries(audio_tests PRIVATE sengoku_scene)
    add_test(NAME battle_audio COMMAND audio_tests)
    add_executable(simulation_tests tests/simulation_tests.cpp)
    target_link_libraries(simulation_tests PRIVATE sengoku_scene)
    if(MSVC)
        # 固定刻み比較用に多数の戦闘状態を同時保持するテストだけ、スタックを4MiB確保する。
        target_link_options(simulation_tests PRIVATE /STACK:4194304)
    endif()
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
    set_tests_properties(dx12_combat PROPERTIES TIMEOUT 120)
    add_test(NAME dx12_attack COMMAND sengoku_rts --smoke-test --inspect-attack --capture "${CMAKE_BINARY_DIR}/attack.bmp")
    set_tests_properties(dx12_attack PROPERTIES TIMEOUT 60)
endif()
