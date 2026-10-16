# ============================================================================
# Fuzzy GPU Regression Tests
# ============================================================================
# WHAT: Regression tests for GPU-accelerated fuzzy logic
# WHY:  Prevent reintroduction of fuzzy GPU bugs
# HOW:  Test specific bug scenarios and numerical stability
# ============================================================================

# DISABLED: Test includes missing header utils/fuzzy/nimcp_fuzzy_mf.h
# TODO: Fix header path

# add_test_binary(
#     regression_fuzzy_gpu
#     ${CMAKE_CURRENT_SOURCE_DIR}/test_fuzzy_gpu_regression.cpp
#     regression
# )
