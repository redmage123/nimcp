#=============================================================================
# CMakeLists.txt - Networking Exception Handling Unit Tests
#=============================================================================
# WHAT: Build configuration for networking exception handling unit tests
# WHY:  Enable GTest-based testing of network error to exception mapping
# HOW:  Use NIMCP standard test framework configuration
#
# TEST COVERAGE:
# - Protocol error exception handling
# - P2P node connection failure exceptions
# - Distributed cognition exception handling
# - NLP session exception handling
# - Message framing error exceptions
# - Exception chaining and aggregation
# - Immune system integration
#
# @author NIMCP Development Team
# @date 2026-01-16
#=============================================================================

# Guard: Check GTest availability
if(NOT GTest_FOUND)
    message(STATUS "GTest not found - skipping networking exception unit tests")
    return()
endif()

# Test source files
set(NETWORKING_EXCEPTION_UNIT_TEST_SOURCES
    test_networking_exception_handling.cpp
)

# Create test executables using NIMCP standard configuration
foreach(test_src ${NETWORKING_EXCEPTION_UNIT_TEST_SOURCES})
    get_filename_component(test_name ${test_src} NAME_WE)
    set(test_target "unit_networking_exception_${test_name}")

    # Check if source file exists
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${test_src})
        add_executable(${test_target} ${test_src})

        target_include_directories(${test_target} PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/networking
            ${CMAKE_SOURCE_DIR}/src/utils/exception
            ${GTEST_INCLUDE_DIRS}
        )

        # Enable C++17 for NIMCP tests
        target_compile_features(${test_target} PRIVATE cxx_std_17)

        # Enable test-only APIs
        target_compile_definitions(${test_target} PRIVATE NIMCP_TESTING)

        target_link_libraries(${test_target} PRIVATE
            nimcp
            GTest::GTest
            GTest::Main
            pthread
        )

        add_test(NAME ${test_target} COMMAND ${test_target})

        # Set test timeout (exception handling tests may involve async operations)
        set_tests_properties(${test_target} PROPERTIES TIMEOUT 60)

        # Add labels for test filtering
        set_tests_properties(${test_target} PROPERTIES
            LABELS "unit;networking;exception"
        )

        message(STATUS "  Added networking exception unit test: ${test_target}")
    else()
        message(STATUS "Networking exception test source not found: ${test_src}")
    endif()
endforeach()
