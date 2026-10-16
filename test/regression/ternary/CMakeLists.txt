# ============================================================================
# Ternary Regression Tests - CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for ternary regression tests
# WHY:  Verify ternary representation does not regress in memory, accuracy,
#       performance, conversion consistency, or backward compatibility
# HOW:  GoogleTest-based regression tests with quantitative metrics
#
# TEST CATEGORIES:
# - Memory:      Verify memory savings match expected ratios
# - Accuracy:    Track quantization error bounds
# - Performance: Benchmark operations against baselines
# - Conversion:  Test roundtrip and conversion consistency
# - Backward:    Ensure API backward compatibility
#
# USAGE:
#   cd build && ctest -R regression_ternary -V
#
# @author NIMCP Test Team
# @date 2025-12-31
# ============================================================================

# ============================================================================
# Test 1: Memory Regression
# ============================================================================
# WHAT: Verify memory savings match expected ratios (20x for packed)
# WHY:  Memory efficiency is primary value proposition of ternary

add_test_binary(
    test_ternary_memory_regression
    ${CMAKE_CURRENT_SOURCE_DIR}/test_ternary_memory_regression.cpp
    regression
)

# ============================================================================
# Test 2: Accuracy Regression
# ============================================================================
# WHAT: Track quantization accuracy and error bounds
# WHY:  Ensure quantization does not degrade over time

add_test_binary(
    test_ternary_accuracy_regression
    ${CMAKE_CURRENT_SOURCE_DIR}/test_ternary_accuracy_regression.cpp
    regression
)

# ============================================================================
# Test 3: Performance Regression
# ============================================================================
# WHAT: Benchmark ternary matrix operations
# WHY:  Performance must not regress with code changes

add_test_binary(
    test_ternary_performance_regression
    ${CMAKE_CURRENT_SOURCE_DIR}/test_ternary_performance_regression.cpp
    regression
)

# ============================================================================
# Test 4: Conversion Regression
# ============================================================================
# WHAT: Test float->ternary->float roundtrip and consistency
# WHY:  Conversion bugs cause data corruption

add_test_binary(
    test_ternary_conversion_regression
    ${CMAKE_CURRENT_SOURCE_DIR}/test_ternary_conversion_regression.cpp
    regression
)

# ============================================================================
# Test 5: Backward Compatibility Regression
# ============================================================================
# WHAT: Verify API backward compatibility
# WHY:  API changes must not break existing code

add_test_binary(
    test_ternary_backward_compat_regression
    ${CMAKE_CURRENT_SOURCE_DIR}/test_ternary_backward_compat_regression.cpp
    regression
)

# ============================================================================
# Summary
# ============================================================================
# Total regression tests: 5
# Coverage areas:
#   - Memory footprint and compression ratios
#   - Quantization accuracy and error bounds
#   - Operation throughput and performance baselines
#   - Conversion roundtrip and consistency
#   - API stability and backward compatibility
# ============================================================================
