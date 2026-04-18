# check_coverage.cmake — Enforces minimum line coverage threshold.
#
# Usage: cmake -DCOVERAGE_FILE=coverage.info -DTHRESHOLD=100 -P check_coverage.cmake
#
# Parses an LCOV tracefile, computes the aggregate line coverage percentage,
# and fails the build (via FATAL_ERROR) if coverage falls below THRESHOLD.

if(NOT DEFINED COVERAGE_FILE)
    message(FATAL_ERROR "COVERAGE_FILE not set")
endif()
if(NOT DEFINED THRESHOLD)
    message(FATAL_ERROR "THRESHOLD not set")
endif()
if(NOT EXISTS "${COVERAGE_FILE}")
    message(FATAL_ERROR "Coverage file not found: ${COVERAGE_FILE}")
endif()

file(READ "${COVERAGE_FILE}" LCOV_CONTENT)

# Count total and hit lines from DA: records
set(TOTAL_LINES 0)
set(HIT_LINES 0)

string(REPLACE "\n" ";" LCOV_LINES "${LCOV_CONTENT}")
foreach(LINE IN LISTS LCOV_LINES)
    if(LINE MATCHES "^DA:[0-9]+,([0-9]+)$")
        math(EXPR TOTAL_LINES "${TOTAL_LINES} + 1")
        if(NOT "${CMAKE_MATCH_1}" STREQUAL "0")
            math(EXPR HIT_LINES "${HIT_LINES} + 1")
        endif()
    endif()
endforeach()

if(TOTAL_LINES EQUAL 0)
    message(FATAL_ERROR "No coverage data found in ${COVERAGE_FILE}")
endif()

math(EXPR COVERAGE_PCT "(${HIT_LINES} * 100) / ${TOTAL_LINES}")

# Also compute the exact ratio for display
math(EXPR REMAINDER "(${HIT_LINES} * 1000) / ${TOTAL_LINES}")
math(EXPR FRAC "${REMAINDER} - (${COVERAGE_PCT} * 10)")

message(STATUS "Line coverage: ${HIT_LINES}/${TOTAL_LINES} = ${COVERAGE_PCT}.${FRAC}%")
message(STATUS "Required threshold: ${THRESHOLD}%")

if(COVERAGE_PCT LESS THRESHOLD)
    message(FATAL_ERROR
        "Coverage check FAILED: ${COVERAGE_PCT}.${FRAC}% < ${THRESHOLD}% required. "
        "${HIT_LINES} of ${TOTAL_LINES} lines covered.")
else()
    message(STATUS "Coverage check PASSED")
endif()
