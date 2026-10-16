# =============================================================================
# Unit Tests for PR Memory Core Exception Handling
# =============================================================================
# WHAT: CMakeLists.txt for PR memory core unit tests
# WHY:  Test NIMCP_THROW_TO_IMMUNE exception handling in PR memory modules
# HOW:  Build test executable and register with CTest
#
# MODULES TESTED:
# - pr_memory_node (Prime Resonant memory nodes)
# - procedural (Procedural memory for skills and habits)
# - prospective (Prospective memory for future intentions)
#
# @author NIMCP Development Team
# @date 2026-01-25
# =============================================================================

# PR Memory Exception Handling Unit Tests
add_executable(unit_pr_memory_exception_handling
    test_pr_memory_exception_handling.cpp
)

target_include_directories(unit_pr_memory_exception_handling
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_pr_memory_exception_handling
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_pr_memory_exception_handling PRIVATE cxx_std_17)
target_compile_definitions(unit_pr_memory_exception_handling PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_pr_memory_exception_handling
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;memory;pr_memory;exception-handling"
        TIMEOUT 120
)
