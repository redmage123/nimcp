# ============================================================================
# Logic Integration Tests
# ============================================================================

# Test 1: Logic Integration
add_test_binary(
    test_logic_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_logic_integration.cpp
    integration
)
