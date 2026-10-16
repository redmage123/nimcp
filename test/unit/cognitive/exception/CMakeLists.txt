# ============================================================================
# Cognitive Exception Handling Unit Tests
# ============================================================================
# WHAT: Unit tests for cognitive module exception handling
# WHY:  Verify exception creation, dispatch, and immune integration for cognitive errors
# HOW:  Test error code mapping, handler chains, recovery strategies
# ============================================================================

# Cognitive Exception Handling Unit Tests
add_executable(unit_cognitive_exception_handling
    test_cognitive_exception_handling.cpp
)

target_link_libraries(unit_cognitive_exception_handling
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_exception_handling
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_exception_handling PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_exception_handling
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;exception;handling"
        TIMEOUT 120
)
