# ============================================================================
# Platform Tier Unit Tests
# ============================================================================

# Test 1: Platform Tier Detection
add_test_binary(
    test_platform_tier
    ${CMAKE_CURRENT_SOURCE_DIR}/test_platform_tier.cpp
    unit
)

# Test 2: Tier Optimization Constants and Brain-Size Scaling (Opt 1, 8)
add_test_binary(
    test_tier_optimization
    ${CMAKE_CURRENT_SOURCE_DIR}/test_tier_optimization.cpp
    unit
)
