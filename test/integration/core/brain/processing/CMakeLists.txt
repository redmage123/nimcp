# ============================================================================
# Cognitive Logic Integration Tests
# ============================================================================

# Test 1: Cognitive Logic Integration
add_test_binary(
    test_cognitive_logic_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_cognitive_logic_integration.cpp
    integration
)
