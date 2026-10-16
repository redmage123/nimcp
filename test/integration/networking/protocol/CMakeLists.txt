# ============================================================================
# Protocol Integration Tests
# ============================================================================

# Test: Protocol Multi-Node Integration
add_test_binary(
    test_protocol_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_protocol_integration.cpp
    integration
)
