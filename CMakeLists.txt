cmake_minimum_required(VERSION 3.24)
project(termite VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(termite_dsp
    src/dsp/eq_profile.cpp
)
target_include_directories(termite_dsp PUBLIC src)
target_compile_features(termite_dsp PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(termite_dsp PRIVATE /W4 /permissive-)
else()
    target_compile_options(termite_dsp PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(termite_audio_core
    src/audio/audio_pipeline.cpp
    src/audio/audio_pipeline.h
    src/audio/routing_policy.cpp
    src/audio/routing_policy.h
)
target_include_directories(termite_audio_core PUBLIC src)
target_compile_features(termite_audio_core PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(termite_audio_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(termite_audio_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

option(TERMITE_BUILD_APP "Build the Windows application" ON)
option(TERMITE_BUILD_TESTS "Build DSP tests" ON)

if(TERMITE_BUILD_APP)
    if(NOT WIN32)
        message(FATAL_ERROR "The Termite application targets Windows only.")
    endif()

    add_executable(termite WIN32)
    target_sources(termite PRIVATE
        src/app/windows_main.cpp
        src/app/console_layout.cpp
        src/app/console_layout.h
        src/app/console_skin.cpp
        src/app/console_skin.h
        src/app/console_state.cpp
        src/app/console_state.h
        src/app/settings_store.cpp
        src/app/settings_store.h
        src/app/console_window.cpp
        src/app/console_window.h
        src/app/termite.rc
        src/audio/wasapi_audio_engine.cpp
        src/audio/wasapi_audio_engine.h
        src/audio/app_audio_policy.cpp
        src/audio/app_audio_policy.h
        src/audio/session_router.cpp
        src/audio/session_router.h
    )
    target_link_libraries(termite PRIVATE termite_dsp termite_audio_core ole32 uuid avrt shell32 runtimeobject d2d1 dwrite dwmapi windowscodecs user32 gdi32)
    if(MSVC)
        # CMake's Visual Studio generator recognizes /MANIFESTUAC as a generic
        # boolean and otherwise emits the default asInvoker manifest. Import a
        # small MSBuild property sheet to set the concrete UAC mode instead.
        set_target_properties(termite PROPERTIES
            VS_USER_PROPS "${CMAKE_CURRENT_SOURCE_DIR}/src/app/termite_elevation.props"
        )
    endif()
    target_compile_definitions(termite PRIVATE
        UNICODE
        _UNICODE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _WIN32_WINNT=0x0A00
    )
    set_target_properties(termite PROPERTIES OUTPUT_NAME "Termite")

    install(TARGETS termite RUNTIME DESTINATION . COMPONENT runtime)
    if(EXISTS "${CMAKE_SOURCE_DIR}/assets")
        add_custom_command(TARGET termite POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:termite>/assets"
            COMMENT "Copying optional Termite background assets"
        )
        install(DIRECTORY assets DESTINATION . COMPONENT runtime)
    endif()
endif()

if(TERMITE_BUILD_TESTS)
    enable_testing()
    add_executable(termite_dsp_tests tests/test_eq_profile.cpp)
    target_link_libraries(termite_dsp_tests PRIVATE termite_dsp)
    add_test(NAME termite_dsp_tests COMMAND termite_dsp_tests)

    add_executable(termite_audio_tests tests/test_audio_pipeline.cpp)
    target_link_libraries(termite_audio_tests PRIVATE termite_audio_core)
    target_include_directories(termite_audio_tests PRIVATE src)
    add_test(NAME termite_audio_tests COMMAND termite_audio_tests)

    add_executable(termite_ui_tests
        tests/test_console.cpp
        src/app/console_layout.cpp
        src/app/console_state.cpp
        src/app/settings_store.cpp
    )
    target_link_libraries(termite_ui_tests PRIVATE termite_dsp shell32)
    target_include_directories(termite_ui_tests PRIVATE src)
    add_test(NAME termite_ui_tests COMMAND termite_ui_tests)

    if(WIN32)
        add_test(
            NAME termite_installer_layout_tests
            COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
                -File "${CMAKE_SOURCE_DIR}/tests/test_installer_layout.ps1"
                -root "${CMAKE_SOURCE_DIR}"
        )
    endif()
endif()
