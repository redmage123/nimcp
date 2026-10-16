# ============================================================================
# Locus Coeruleus Regression Tests
# ============================================================================
# WHAT: Regression tests for LC approach_target math bug
# WHY:  Prevent recurrence of (target-target) = 0 delta bug
# HOW:  Verify firing rate and NE concentration actually respond to mode changes
# ============================================================================

add_test_binary(
    regression_lc_math_regression
    ${CMAKE_CURRENT_SOURCE_DIR}/test_locus_coeruleus_regression.cpp
    regression
)
