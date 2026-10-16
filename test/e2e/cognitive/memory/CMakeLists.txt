# ============================================================================
# PR Memory E2E Tests
# ============================================================================
# WHAT: End-to-end tests for PR memory system exception handling
# WHY:  Verify complete exception handling workflows in realistic scenarios
# HOW:  GTest-based tests covering full memory lifecycle and error recovery
# ============================================================================

# PR Memory Exception Handling E2E Test
add_executable(e2e_cognitive_memory_pr_exception
    test_pr_memory_exception_e2e.cpp
)

target_include_directories(e2e_cognitive_memory_pr_exception PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/test
)

target_link_libraries(e2e_cognitive_memory_pr_exception
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(e2e_cognitive_memory_pr_exception PRIVATE cxx_std_17)
target_compile_definitions(e2e_cognitive_memory_pr_exception PRIVATE NIMCP_TESTING)

# Register test with CTest
gtest_discover_tests(e2e_cognitive_memory_pr_exception
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "e2e;cognitive;memory;pr_memory;exception"
        TIMEOUT 300
)
