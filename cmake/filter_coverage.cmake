# cmake/filter_coverage.cmake — Runs lcov --remove with glob patterns passed directly,
# bypassing shell glob expansion that occurs when CMake add_custom_target uses sh -c.
#
# Called as:
#   cmake -DLCOV=<path> -DINPUT_FILE=<path> -DOUTPUT_FILE=<path> -P filter_coverage.cmake

if(NOT DEFINED LCOV OR NOT DEFINED INPUT_FILE OR NOT DEFINED OUTPUT_FILE)
    message(FATAL_ERROR "Usage: cmake -DLCOV=... -DINPUT_FILE=... -DOUTPUT_FILE=... -P filter_coverage.cmake")
endif()

# Patterns are passed as COMMAND arguments without going through the shell,
# so * is NOT glob-expanded — lcov receives them as literal fnmatch patterns.
execute_process(
    COMMAND ${LCOV} --remove ${INPUT_FILE}
        /opt/homebrew/*
        /usr/local/*
        /usr/include/*
        /Library/*
        *_autogen*
        */test/*
        */moc_*
        */qrc_*
        */main.cpp
        --output-file ${OUTPUT_FILE}
        --ignore-errors inconsistent,unused
    RESULT_VARIABLE result
    OUTPUT_QUIET
    ERROR_QUIET
)

if(result)
    message(FATAL_ERROR "lcov --remove failed with exit code: ${result}")
endif()

message(STATUS "Coverage filtered: ${OUTPUT_FILE}")
