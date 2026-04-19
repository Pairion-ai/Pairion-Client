cmake_minimum_required(VERSION 3.25)
project(pairion VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# --------------------------------------------------------------------------
# Version header — injected from project VERSION at configure time
# --------------------------------------------------------------------------
configure_file(
    src/core/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/src/core/version.h
    @ONLY
)

# --------------------------------------------------------------------------
# Dependencies
# --------------------------------------------------------------------------
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Quick Network WebSockets Multimedia Test)

find_package(PkgConfig REQUIRED)
pkg_check_modules(Opus REQUIRED IMPORTED_TARGET opus)

find_package(onnxruntime REQUIRED)

# --------------------------------------------------------------------------
# Library: pairion_core (all src/ except main.cpp — testable)
# --------------------------------------------------------------------------
add_library(pairion_core STATIC
    # protocol
    src/protocol/envelope_codec.cpp
    src/protocol/binary_codec.cpp
    # core
    src/core/device_identity.cpp
    src/core/onnx_session.cpp
    src/core/model_downloader.cpp
    # state
    src/state/connection_state.cpp
    # settings
    src/settings/settings.cpp
    # audio
    src/audio/pairion_opus_encoder.cpp
    src/audio/pairion_opus_decoder.cpp
    src/audio/pairion_audio_capture.cpp
    src/audio/pairion_audio_playback.cpp
    # wake
    src/wake/open_wakeword_detector.cpp
    # vad
    src/vad/silero_vad.cpp
    # pipeline
    src/pipeline/audio_session_orchestrator.cpp
    # ws
    src/ws/pairion_websocket_client.cpp
    # util
    src/util/logger.cpp
)
target_include_directories(pairion_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/src/core
)
target_link_libraries(pairion_core PUBLIC
    Qt6::Core
    Qt6::Quick
    Qt6::Network
    Qt6::WebSockets
    Qt6::Multimedia
    PkgConfig::Opus
    onnxruntime::onnxruntime
)

# --------------------------------------------------------------------------
# Executable: pairion
# --------------------------------------------------------------------------
qt_add_executable(pairion src/main.cpp resources.qrc)
target_link_libraries(pairion PRIVATE pairion_core)
set_target_properties(pairion PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/platform/macos/Info.plist
)

# --------------------------------------------------------------------------
# Optional: verbose wake score logging (default OFF)
# --------------------------------------------------------------------------
option(PAIRION_WAKE_DIAGNOSTICS "Enable Info-level wake classifier score logging" OFF)
if(PAIRION_WAKE_DIAGNOSTICS)
    target_compile_definitions(pairion_core PUBLIC PAIRION_WAKE_DIAGNOSTICS=1)
endif()

# --------------------------------------------------------------------------
# Coverage (opt-in via -DPAIRION_COVERAGE=ON)
# --------------------------------------------------------------------------
option(PAIRION_COVERAGE "Enable code coverage instrumentation" OFF)
if(PAIRION_COVERAGE)
    target_compile_options(pairion_core PRIVATE --coverage)
    target_link_options(pairion_core PUBLIC --coverage)
endif()

# --------------------------------------------------------------------------
# Native ONNX model tests (opt-in)
# --------------------------------------------------------------------------
option(PAIRION_CLIENT_NATIVE_TESTS "Enable tests requiring real ONNX model files" OFF)
if(PAIRION_CLIENT_NATIVE_TESTS)
    target_compile_definitions(pairion_core PUBLIC PAIRION_CLIENT_NATIVE_TESTS=1)
endif()

# --------------------------------------------------------------------------
# Tests
# --------------------------------------------------------------------------
enable_testing()

# M0 tests
add_executable(tst_message_codec test/tst_message_codec.cpp)
target_link_libraries(tst_message_codec PRIVATE pairion_core Qt6::Test)
add_test(NAME tst_message_codec COMMAND tst_message_codec)

add_executable(tst_log_batching test/tst_log_batching.cpp)
target_link_libraries(tst_log_batching PRIVATE pairion_core Qt6::Test)
add_test(NAME tst_log_batching COMMAND tst_log_batching)

add_executable(tst_ws_client_state_machine
    test/tst_ws_client_state_machine.cpp
    test/mock_server.cpp
)
target_link_libraries(tst_ws_client_state_machine PRIVATE pairion_core Qt6::Test)
add_test(NAME tst_ws_client_state_machine COMMAND tst_ws_client_state_machine)

add_executable(tst_device_identity test/tst_device_identity.cpp)
target_link_libraries(tst_device_identity PRIVATE pairion_core Qt6::Test)
add_test(NAME tst_device_identity COMMAND tst_device_identity)

add_executable(tst_integration
    test/tst_integration.cpp
    test/mock_server.cpp
)
target_link_libraries(tst_integration PRIVATE pairion_core Qt6::Test)
add_test(NAME tst_integration COMMAND tst_integration)

# M1 tests
add_executable(tst_binary_frame test/tst_binary_frame.cpp)
target_link_libraries(tst_binary_frame PRIVATE pairion_core Qt6::Test)
add_test(NAME tst_binary_frame COMMAND tst_binary_frame)

add_executable(tst_settings test/tst_settings.cpp)
target_link_libraries(tst_settings PRIVATE pairion_core Qt6::Test)
add_test(NAME tst_settings COMMAND tst_settings)

add_executable(tst_ring_buffer test/tst_ring_buffer.cpp)
target_link_libraries(tst_ring_buffer PRIVATE Qt6::Core Qt6::Test)
target_include_directories(tst_ring_buffer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_test(NAME tst_ring_buffer COMMAND tst_ring_buffer)

add_executable(tst_opus_codec test/tst_opus_codec.cpp)
target_link_libraries(tst_opus_codec PRIVATE pairion_core Qt6::Test)
add_test(NAME tst_opus_codec COMMAND tst_opus_codec)

add_executable(tst_wake_detector test/tst_wake_detector.cpp)
target_link_libraries(tst_wake_detector PRIVATE pairion_core Qt6::Test)
add_test(NAME tst_wake_detector COMMAND tst_wake_detector)

add_executable(tst_silero_vad test/tst_silero_vad.cpp)
target_link_libraries(tst_silero_vad PRIVATE pairion_core Qt6::Test)
add_test(NAME tst_silero_vad COMMAND tst_silero_vad)

add_executable(tst_model_downloader test/tst_model_downloader.cpp)
target_link_libraries(tst_model_downloader PRIVATE pairion_core Qt6::Test)
add_test(NAME tst_model_downloader COMMAND tst_model_downloader)

add_executable(tst_audio_session_orchestrator
    test/tst_audio_session_orchestrator.cpp
    test/mock_server.cpp
)
target_link_libraries(tst_audio_session_orchestrator PRIVATE pairion_core Qt6::Test)
add_test(NAME tst_audio_session_orchestrator COMMAND tst_audio_session_orchestrator)

add_executable(tst_inbound_playback
    test/tst_inbound_playback.cpp
    test/mock_server.cpp
)
target_link_libraries(tst_inbound_playback PRIVATE pairion_core Qt6::Test)
add_test(NAME tst_inbound_playback COMMAND tst_inbound_playback)

add_executable(tst_constants test/tst_constants.cpp)
target_link_libraries(tst_constants PRIVATE pairion_core Qt6::Test)
add_test(NAME tst_constants COMMAND tst_constants)

# --------------------------------------------------------------------------
# Coverage target
# --------------------------------------------------------------------------
set(PAIRION_COVERAGE_THRESHOLD 100 CACHE STRING "Minimum required line coverage percentage")

if(PAIRION_COVERAGE)
    if(UNIX)
        find_program(LCOV lcov)
        find_program(GENHTML genhtml)
        if(LCOV AND GENHTML)
            add_custom_target(coverage
                COMMAND ${LCOV} --directory . --zerocounters
                COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
                COMMAND ${LCOV} --directory . --capture --output-file coverage.info
                    --ignore-errors inconsistent
                # Use a CMake script for the filter step so that glob patterns like
                # */moc_* are passed directly to lcov without shell glob-expansion.
                COMMAND ${CMAKE_COMMAND}
                    -DLCOV=${LCOV}
                    -DINPUT_FILE=${CMAKE_BINARY_DIR}/coverage.info
                    -DOUTPUT_FILE=${CMAKE_BINARY_DIR}/coverage_filtered.info
                    -P ${CMAKE_SOURCE_DIR}/cmake/filter_coverage.cmake
                COMMAND ${GENHTML} coverage_filtered.info --output-directory coverage_report
                    --ignore-errors inconsistent,unmapped,corrupt,category
                COMMAND ${CMAKE_COMMAND} -E echo
                    "Coverage report: ${CMAKE_BINARY_DIR}/coverage_report/index.html"
                COMMAND ${CMAKE_COMMAND}
                    -DCOVERAGE_FILE=${CMAKE_BINARY_DIR}/coverage_filtered.info
                    -DTHRESHOLD=${PAIRION_COVERAGE_THRESHOLD}
                    -P ${CMAKE_SOURCE_DIR}/cmake/check_coverage.cmake
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Generating code coverage report and enforcing ${PAIRION_COVERAGE_THRESHOLD}% threshold"
            )
        else()
            message(WARNING "lcov/genhtml not found — coverage target unavailable on this platform")
        endif()
    else()
        message(STATUS "Coverage target disabled on Windows")
    endif()
endif()
