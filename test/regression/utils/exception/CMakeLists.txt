# =============================================================================
# Exception Handling Regression Tests
#
# Tests for exception handling stability:
# - Memory safety (create/destroy doesn't leak)
# - Null safety for all APIs
# - Thread safety for concurrent exception handling
# - Recovery callback registration/unregistration stability
# - Metrics accuracy after many operations
# - Circuit breaker state transitions
# - Epitope generation determinism
# =============================================================================

# Exception Handling Stability Regression Tests
add_executable(regression_utils_exception_test_exception_regression
    test_exception_regression.cpp
)

target_link_libraries(regression_utils_exception_test_exception_regression
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        pthread
        m
)

target_include_directories(regression_utils_exception_test_exception_regression
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(regression_utils_exception_test_exception_regression PRIVATE cxx_std_17)

add_test(
    NAME regression_utils_exception_test_exception_regression
    COMMAND regression_utils_exception_test_exception_regression
)

set_tests_properties(regression_utils_exception_test_exception_regression
    PROPERTIES
    LABELS "regression;utils;exception;stability"
    TIMEOUT 180
)
