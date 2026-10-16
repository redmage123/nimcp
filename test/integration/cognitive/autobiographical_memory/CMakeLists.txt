# ============================================================================
# Autobiographical Memory Integration Tests
# ============================================================================

# Test 1: Autobiographical FEP Bridge Integration
add_test_binary(
    test_autobiographical_fep_bridge_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_autobiographical_fep_bridge_integration.cpp
    integration
)
