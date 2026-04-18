cmake_minimum_required(VERSION 3.25)
project(pairion VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# --------------------------------------------------------------------------
# Dependencies
# --------------------------------------------------------------------------
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Quick Network WebSockets Test)
# QtKeychain removed — using QSettings for device credential persistence

# --------------------------------------------------------------------------
# Library: pairion_core (all src/ except main.cpp — testable)
# --------------------------------------------------------------------------
add_library(pairion_core STATIC
    src/protocol/envelope_codec.cpp
    src/state/connection_state.cpp
    src/ws/pairion_websocket_client.cpp
    src/util/logger.cpp
    src/core/device_identity.cpp
)
target_include_directories(pairion_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(pairion_core PUBLIC
    Qt6::Core
    Qt6::Quick
    Qt6::Network
    Qt6::WebSockets
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
# Coverage (opt-in via -DPAIRION_COVERAGE=ON)
# --------------------------------------------------------------------------
option(PAIRION_COVERAGE "Enable code coverage instrumentation" OFF)
if(PAIRION_COVERAGE)
    target_compile_options(pairion_core PRIVATE --coverage)
    target_link_options(pairion_core PUBLIC --coverage)
endif()

# --------------------------------------------------------------------------
# Tests
# --------------------------------------------------------------------------
enable_testing()

# Test: message codec
add_executable(tst_message_codec
    test/tst_message_codec.cpp
)
target_link_libraries(tst_message_codec PRIVATE pairion_core Qt6::Test)
add_test(NAME tst_message_codec COMMAND tst_message_codec)

# Test: log batching
add_executable(tst_log_batching
    test/tst_log_batching.cpp
)
target_link_libraries(tst_log_batching PRIVATE pairion_core Qt6::Test)
add_test(NAME tst_log_batching COMMAND tst_log_batching)

# Test: WebSocket client state machine
add_executable(tst_ws_client_state_machine
    test/tst_ws_client_state_machine.cpp
    test/mock_server.cpp
)
target_link_libraries(tst_ws_client_state_machine PRIVATE pairion_core Qt6::Test)
add_test(NAME tst_ws_client_state_machine COMMAND tst_ws_client_state_machine)

# Test: device identity
add_executable(tst_device_identity
    test/tst_device_identity.cpp
)
target_link_libraries(tst_device_identity PRIVATE pairion_core Qt6::Test)
add_test(NAME tst_device_identity COMMAND tst_device_identity)

# Test: integration
add_executable(tst_integration
    test/tst_integration.cpp
    test/mock_server.cpp
)
target_link_libraries(tst_integration PRIVATE pairion_core Qt6::Test)
add_test(NAME tst_integration COMMAND tst_integration)

# --------------------------------------------------------------------------
# Coverage target
# --------------------------------------------------------------------------
if(PAIRION_COVERAGE)
    find_program(LCOV lcov)
    find_program(GENHTML genhtml)
    if(LCOV AND GENHTML)
        add_custom_target(coverage
            COMMAND ${LCOV} --directory . --zerocounters
            COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
            COMMAND ${LCOV} --directory . --capture --output-file coverage.info
                --ignore-errors inconsistent
            COMMAND ${LCOV} --remove coverage.info
                '/opt/homebrew/*' '/usr/*' '*/test/*' '*/moc_*' '*/qrc_*' '*/main.cpp'
                --output-file coverage_filtered.info
                --ignore-errors unused
            COMMAND ${GENHTML} coverage_filtered.info --output-directory coverage_report
                --ignore-errors unmapped --ignore-errors category
            COMMAND echo "Coverage report: ${CMAKE_BINARY_DIR}/coverage_report/index.html"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating code coverage report"
        )
    else()
        message(WARNING "lcov/genhtml not found — coverage target unavailable")
    endif()
endif()
