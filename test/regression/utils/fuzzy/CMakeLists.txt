# ============================================================================
# NIMCP Fuzzy Logic Regression Tests
# ============================================================================
# WHAT: Regression tests for fuzzy logic backward compatibility and correctness
# WHY:  Prevent regressions in NULL safety, mathematical invariants (t-norm/
#       conorm axioms, commutativity, monotonicity), error codes, config
#       defaults, stats reset, modulation bounds, and edge cases
# HOW:  GTest-based tests with axiomatic property assertions
# ============================================================================

message(STATUS "  Adding Fuzzy Logic regression tests...")

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_fuzzy_regression.cpp)
    add_executable(regression_fuzzy_logic
        ${CMAKE_CURRENT_SOURCE_DIR}/test_fuzzy_regression.cpp
    )
    target_include_directories(regression_fuzzy_logic PRIVATE
        ${PROJECT_SOURCE_DIR}/include
    )
    target_link_libraries(regression_fuzzy_logic PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(regression_fuzzy_logic PRIVATE cxx_std_17)
    target_compile_definitions(regression_fuzzy_logic PRIVATE
        NIMCP_TESTING
    )
    add_test(NAME regression_fuzzy_logic COMMAND regression_fuzzy_logic)
    set_tests_properties(regression_fuzzy_logic PROPERTIES
        LABELS "regression;utils;fuzzy"
        TIMEOUT 60
    )
    message(STATUS "    Added: regression_fuzzy_logic")
endif()
