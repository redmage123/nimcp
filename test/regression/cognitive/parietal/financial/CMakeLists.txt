# ============================================================================
# Financial Cognitive Integration Regression Tests (Check Framework)
# ============================================================================
# WHAT: Build configuration for financial cognitive integration regression tests
# WHY:  Verify stability, NULL safety, error codes, math properties, and
#       backward compatibility of financial module cognitive integration
# HOW:  Check framework-based tests covering null safety, boundary conditions,
#       error codes, math properties, and backward compatibility
#
# Test Categories:
#   - Null Safety (~20 tests)
#   - Boundary Conditions (~15 tests)
#   - Error Codes (~15 tests)
#   - Math Properties (~10 tests)
#   - Backward Compatibility (~8 tests)
#   Total: ~68 tests
#
# @date 2026-01-29

# Find Check framework
find_package(PkgConfig REQUIRED)
pkg_check_modules(CHECK REQUIRED check)

# Financial Cognitive Integration Regression Tests (C / Check Framework)
add_executable(regression_financial_cognitive_integration_c
    test_financial_regression.c
)

target_link_libraries(regression_financial_cognitive_integration_c
    PRIVATE
        nimcp
        ${CHECK_LIBRARIES}
        Threads::Threads
        m
)

target_include_directories(regression_financial_cognitive_integration_c
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${CHECK_INCLUDE_DIRS}
)

target_compile_definitions(regression_financial_cognitive_integration_c
    PRIVATE
        NIMCP_TESTING
)

# Check framework specific compile options
target_compile_options(regression_financial_cognitive_integration_c
    PRIVATE
        ${CHECK_CFLAGS_OTHER}
)

# Add CTest integration
add_test(
    NAME regression_financial_cognitive_integration_c
    COMMAND regression_financial_cognitive_integration_c
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

set_tests_properties(regression_financial_cognitive_integration_c
    PROPERTIES
        LABELS "regression;cognitive;parietal;financial;check"
        TIMEOUT 300
)

# Optional: Add individual suite tests for more granular reporting
# These use CK_FORK=no to avoid forking issues in some environments
add_test(
    NAME regression_financial_null_safety
    COMMAND regression_financial_cognitive_integration_c
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(regression_financial_null_safety
    PROPERTIES
        LABELS "regression;cognitive;parietal;financial;null-safety"
        TIMEOUT 60
        ENVIRONMENT "CK_FORK=no"
)
