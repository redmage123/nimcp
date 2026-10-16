# =============================================================================
# Exception Regression Tests
# =============================================================================

# Signal Exception Regression Tests
add_executable(regression_exception_test_signal_exception_regression
    test_signal_exception_regression.cpp
)

target_link_libraries(regression_exception_test_signal_exception_regression
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        pthread
        m
)

target_compile_features(regression_exception_test_signal_exception_regression PRIVATE cxx_std_17)

add_test(
    NAME regression_exception_test_signal_exception_regression
    COMMAND regression_exception_test_signal_exception_regression
)

set_tests_properties(regression_exception_test_signal_exception_regression
    PROPERTIES
    LABELS "regression;exception;signal"
    TIMEOUT 120
)

# =============================================================================
# Exception API Regression Tests
# Tests that API contracts (enum values, constants, function signatures) remain stable
# =============================================================================

add_executable(regression_exception_test_exception_api_regression
    test_exception_api_regression.cpp
)

target_link_libraries(regression_exception_test_exception_api_regression
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        pthread
        m
)

target_compile_features(regression_exception_test_exception_api_regression PRIVATE cxx_std_17)

add_test(
    NAME regression_exception_test_exception_api_regression
    COMMAND regression_exception_test_exception_api_regression
)

set_tests_properties(regression_exception_test_exception_api_regression
    PROPERTIES
    LABELS "regression;exception;api"
    TIMEOUT 120
)

# =============================================================================
# Exception Contracts Regression Tests
# Tests handler registration/unregistration, priority ordering, filtering
# =============================================================================

add_executable(regression_exception_test_exception_contracts_regression
    test_exception_contracts_regression.cpp
)

target_link_libraries(regression_exception_test_exception_contracts_regression
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        pthread
        m
)

target_compile_features(regression_exception_test_exception_contracts_regression PRIVATE cxx_std_17)

add_test(
    NAME regression_exception_test_exception_contracts_regression
    COMMAND regression_exception_test_exception_contracts_regression
)

set_tests_properties(regression_exception_test_exception_contracts_regression
    PROPERTIES
    LABELS "regression;exception;contracts;handlers"
    TIMEOUT 120
)

# =============================================================================
# Exception Immune Integration Regression Tests
# Tests exception-immune system integration contracts
# =============================================================================

add_executable(regression_exception_test_exception_immune_regression
    test_exception_immune_regression.cpp
)

target_link_libraries(regression_exception_test_exception_immune_regression
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        pthread
        m
)

target_compile_features(regression_exception_test_exception_immune_regression PRIVATE cxx_std_17)

add_test(
    NAME regression_exception_test_exception_immune_regression
    COMMAND regression_exception_test_exception_immune_regression
)

set_tests_properties(regression_exception_test_exception_immune_regression
    PROPERTIES
    LABELS "regression;exception;immune;integration"
    TIMEOUT 120
)

# =============================================================================
# Exception API Stability Tests
# Verify API contracts remain stable across versions:
# - Error code values unchanged
# - Enum values unchanged
# - Function signatures stable
# - Backwards compatibility maintained
# =============================================================================

add_executable(regression_exception_api_stability
    test_exception_api_stability.cpp
)

target_link_libraries(regression_exception_api_stability
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        pthread
        m
)

target_compile_features(regression_exception_api_stability PRIVATE cxx_std_17)

add_test(
    NAME regression_exception_api_stability
    COMMAND regression_exception_api_stability
)

set_tests_properties(regression_exception_api_stability
    PROPERTIES
    LABELS "regression;exception;stability;api"
    TIMEOUT 120
)

# =============================================================================
# Exception Error Codes Tests
# Verify correct error codes are returned:
# - Error code ranges are correct
# - Error to category mapping correct
# - Error to severity mapping correct
# - String conversions work
# =============================================================================

add_executable(regression_exception_error_codes
    test_exception_error_codes.cpp
)

target_link_libraries(regression_exception_error_codes
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        pthread
        m
)

target_compile_features(regression_exception_error_codes PRIVATE cxx_std_17)

add_test(
    NAME regression_exception_error_codes
    COMMAND regression_exception_error_codes
)

set_tests_properties(regression_exception_error_codes
    PROPERTIES
    LABELS "regression;exception;stability;error_codes"
    TIMEOUT 120
)

# =============================================================================
# Exception Performance Tests
# Benchmark exception handling overhead:
# - Exception creation/destruction time
# - Handler dispatch overhead
# - Epitope computation time
# - Concurrent performance
# - Full exception path timing
# =============================================================================

add_executable(regression_exception_performance
    test_exception_performance.cpp
)

target_link_libraries(regression_exception_performance
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        pthread
        m
)

target_compile_features(regression_exception_performance PRIVATE cxx_std_17)

add_test(
    NAME regression_exception_performance
    COMMAND regression_exception_performance
)

set_tests_properties(regression_exception_performance
    PROPERTIES
    LABELS "regression;exception;performance;benchmark"
    TIMEOUT 300
)

# =============================================================================
# Exception API Compatibility Tests (NEW)
# Tests that all public API functions return documented error codes:
# - NULL inputs return NIMCP_ERROR_NULL_POINTER
# - Invalid params return NIMCP_ERROR_INVALID_PARAM
# - Security, cognitive, perception, and core module APIs
# =============================================================================

add_executable(regression_exception_api_compatibility
    test_exception_api_compatibility.cpp
)

target_link_libraries(regression_exception_api_compatibility
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        pthread
        m
)

target_compile_features(regression_exception_api_compatibility PRIVATE cxx_std_17)

add_test(
    NAME regression_exception_api_compatibility
    COMMAND regression_exception_api_compatibility
)

set_tests_properties(regression_exception_api_compatibility
    PROPERTIES
    LABELS "regression;exception;api;compatibility"
    TIMEOUT 300
    RESOURCE_LOCK "brain_heavy"
)

# =============================================================================
# Exception Error Code Stability Tests (NEW)
# Verify error code numeric values haven't changed:
# - All NIMCP_ERROR_* codes have expected values
# - Error code ranges are correct
# - Backward compatibility maintained
# =============================================================================

add_executable(regression_exception_error_code_stability
    test_exception_error_code_stability.cpp
)

target_link_libraries(regression_exception_error_code_stability
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        pthread
        m
)

target_compile_features(regression_exception_error_code_stability PRIVATE cxx_std_17)

add_test(
    NAME regression_exception_error_code_stability
    COMMAND regression_exception_error_code_stability
)

set_tests_properties(regression_exception_error_code_stability
    PROPERTIES
    LABELS "regression;exception;stability;error_codes"
    TIMEOUT 120
)

# =============================================================================
# Exception Performance Regression Tests (NEW)
# Benchmark exception handling overhead:
# - Error check macro overhead vs direct returns
# - Hot path performance (< 1% overhead)
# - Memory allocation patterns on error paths
# - Error string conversion performance
# =============================================================================

add_executable(regression_exception_performance_regression
    test_exception_performance_regression.cpp
)

target_link_libraries(regression_exception_performance_regression
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        pthread
        m
)

target_compile_features(regression_exception_performance_regression PRIVATE cxx_std_17)

add_test(
    NAME regression_exception_performance_regression
    COMMAND regression_exception_performance_regression
)

set_tests_properties(regression_exception_performance_regression
    PROPERTIES
    LABELS "regression;exception;performance;overhead"
    TIMEOUT 300
    RESOURCE_LOCK "brain_heavy"
)

# =============================================================================
# Typed Exception Regression Tests
# Tests type-safe exception wrapping and propagation
# =============================================================================

add_executable(regression_exception_typed_regression
    test_typed_exception_regression.cpp
)

target_link_libraries(regression_exception_typed_regression
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        pthread
        m
)

target_compile_features(regression_exception_typed_regression PRIVATE cxx_std_17)

add_test(
    NAME regression_exception_typed_regression
    COMMAND regression_exception_typed_regression
)

set_tests_properties(regression_exception_typed_regression
    PROPERTIES
    LABELS "regression;exception;typed"
    TIMEOUT 120
)

# =============================================================================
# Exception Macro Regression Tests
# Tests exception handling macros (NIMCP_CHECK, NIMCP_PROPAGATE_ERROR, etc.)
# =============================================================================

add_executable(regression_exception_macro_regression
    test_exception_macro_regression.cpp
)

target_link_libraries(regression_exception_macro_regression
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        pthread
        m
)

target_compile_features(regression_exception_macro_regression PRIVATE cxx_std_17)

add_test(
    NAME regression_exception_macro_regression
    COMMAND regression_exception_macro_regression
)

set_tests_properties(regression_exception_macro_regression
    PROPERTIES
    LABELS "regression;exception;macro"
    TIMEOUT 120
)

# =============================================================================
# Exception Module API Stability Tests (NEW)
# Comprehensive regression tests for exception API stability across modules:
# - Swarm module exception API consistency
# - Security module exception API stability
# - Training module exception interface
# - Perception module exception contracts
# - Memory module exception behavior
# - Bio-async exception routing API
# =============================================================================

add_executable(regression_exception_module_api_stability
    test_exception_module_api_stability.cpp
)

target_link_libraries(regression_exception_module_api_stability
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        pthread
        m
)

target_compile_features(regression_exception_module_api_stability PRIVATE cxx_std_17)

add_test(
    NAME regression_exception_module_api_stability
    COMMAND regression_exception_module_api_stability
)

set_tests_properties(regression_exception_module_api_stability
    PROPERTIES
    LABELS "regression;exception;stability;api;module"
    TIMEOUT 180
)

# =============================================================================
# Custom Target: Run All Exception Regression Tests
# =============================================================================
add_custom_target(test_exception_regression
    COMMAND ${CMAKE_CTEST_COMMAND} -L "regression" -L "exception" --output-on-failure
    DEPENDS
        regression_exception_test_signal_exception_regression
        regression_exception_test_exception_api_regression
        regression_exception_test_exception_contracts_regression
        regression_exception_test_exception_immune_regression
        regression_exception_api_stability
        regression_exception_error_codes
        regression_exception_performance
        regression_exception_api_compatibility
        regression_exception_error_code_stability
        regression_exception_performance_regression
        regression_exception_typed_regression
        regression_exception_macro_regression
        regression_exception_module_api_stability
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all exception regression tests"
)

# =============================================================================
# Custom Target: Run Exception Stability Tests Only
# =============================================================================
add_custom_target(test_exception_stability
    COMMAND ${CMAKE_CTEST_COMMAND} -L "stability" --output-on-failure
    DEPENDS
        regression_exception_api_stability
        regression_exception_error_codes
        regression_exception_error_code_stability
        regression_exception_module_api_stability
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running exception stability tests"
)

# =============================================================================
# Custom Target: Run Exception Performance Tests Only
# =============================================================================
add_custom_target(test_exception_performance_only
    COMMAND ${CMAKE_CTEST_COMMAND} -L "performance" -L "exception" --output-on-failure
    DEPENDS
        regression_exception_performance
        regression_exception_performance_regression
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running exception performance benchmarks"
)

# =============================================================================
# Exception API Comprehensive Regression Tests (NEW)
# Comprehensive regression tests for complete exception handling API stability:
# - Error code values remain stable
# - Exception structure layout compatibility
# - Function signatures unchanged
# - Callback interface stability
# - Thread-safety guarantees
# - Memory layout of exception types
# =============================================================================

add_executable(regression_exception_api_comprehensive
    test_exception_api_comprehensive.cpp
)

target_link_libraries(regression_exception_api_comprehensive
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        pthread
        m
)

target_compile_features(regression_exception_api_comprehensive PRIVATE cxx_std_17)

add_test(
    NAME regression_exception_api_comprehensive
    COMMAND regression_exception_api_comprehensive
)

set_tests_properties(regression_exception_api_comprehensive
    PROPERTIES
    LABELS "regression;exception;api;comprehensive"
    TIMEOUT 180
)
