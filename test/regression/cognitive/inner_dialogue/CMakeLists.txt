# ============================================================================
# Inner Dialogue Engine Regression Tests
# ============================================================================
# WHAT: Regression tests for API contract stability and boundary conditions
# WHY:  Prevent regressions in error handling, NULL safety, constants
# HOW:  GoogleTest with comprehensive NULL, boundary, error code, invariant tests
# ============================================================================

add_executable(regression_cognitive_inner_dialogue
    test_inner_dialogue_regression.cpp
)

target_link_libraries(regression_cognitive_inner_dialogue
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_inner_dialogue
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(regression_cognitive_inner_dialogue PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_inner_dialogue
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;inner_dialogue"
        TIMEOUT 180
)
