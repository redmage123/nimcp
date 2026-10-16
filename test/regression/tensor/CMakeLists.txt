# ============================================================================
# Tensor Regression Tests
# ============================================================================
# WHAT: Regression tests for tensor overflow detection
# WHY:  Prevent dimension and element count overflow bugs
# HOW:  Test boundary conditions, overflow scenarios
# ============================================================================

# Tensor Overflow Regression Test
add_test_binary(
    regression_tensor_overflow
    ${CMAKE_CURRENT_SOURCE_DIR}/test_tensor_overflow_regression.cpp
    regression
)
