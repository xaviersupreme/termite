cmake_minimum_required(VERSION 3.24)
project(termite VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_library(termite_dsp
    src/sound/eq/eq_profile.cpp
    src/host/eq_bridge_profile.cpp
    src/host/eq_bridge_profile.h
    src/host/eq_bridge_protocol.h
)
target_include_directories(termite_dsp PUBLIC src)
target_compile_features(termite_dsp PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(termite_dsp PRIVATE /W4 /permissive-)
else()
    target_compile_options(termite_dsp PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(termite_audio_core
    src/sound/audio_pipeline.cpp
    src/sound/audio_pipeline.h
    src/sound/routing_policy.cpp
    src/sound/routing_policy.h
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
        src/host/windows_main.cpp
        src/host/eq_bridge_server.cpp
        src/host/eq_bridge_server.h
        src/host/route_bridge_protocol.h
        src/host/route_bridge_server.cpp
        src/host/route_bridge_server.h
        src/host/termite_host.cpp
        src/host/termite_host.h
        src/host/termite.rc
        src/sound/wasapi_audio_engine.cpp
        src/sound/wasapi_audio_engine.h
        src/sound/app_audio_policy.cpp
        src/sound/app_audio_policy.h
        src/sound/session_router.cpp
        src/sound/session_router.h
    )
    target_link_libraries(termite PRIVATE termite_dsp termite_audio_core ole32 uuid avrt shell32 runtimeobject user32 advapi32 mmdevapi)
    target_compile_definitions(termite PRIVATE
        UNICODE
        _UNICODE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _WIN32_WINNT=0x0A00
    )
    
    
    
    set_target_properties(termite PROPERTIES
        OUTPUT_NAME "Termite"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}"
    )

    
    
    
    
    
    set(TERMITE_VCL_FRONTEND "${CMAKE_SOURCE_DIR}/build/TermiteUI.exe")
    add_custom_command(TARGET termite POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DSOURCE_FILE=${TERMITE_VCL_FRONTEND}
            -DDESTINATION_FILE=$<TARGET_FILE_DIR:termite>/TermiteUI.exe
            -P ${CMAKE_SOURCE_DIR}/ui/copy_frontend.cmake
        COMMENT "Copying the VCL Termite frontend when available"
    )
    if(EXISTS "${CMAKE_SOURCE_DIR}/assets")
        add_custom_command(TARGET termite POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:termite>/assets"
            COMMENT "Copying optional Termite background assets"
        )
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

    if(WIN32)
        add_executable(termite_eq_bridge_tests
            tests/test_eq_bridge.cpp
            src/host/eq_bridge_server.cpp
            src/host/eq_bridge_server.h
        )
        target_link_libraries(termite_eq_bridge_tests PRIVATE termite_dsp advapi32 user32)
        target_include_directories(termite_eq_bridge_tests PRIVATE src)
        target_compile_definitions(termite_eq_bridge_tests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
        add_test(NAME termite_eq_bridge_tests COMMAND termite_eq_bridge_tests)
    endif()

endif()
