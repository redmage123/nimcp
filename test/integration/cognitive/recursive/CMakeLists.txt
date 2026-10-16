# ============================================================================
# Recursive Cognition Integration Tests
# ============================================================================
#
# WHAT: Integration tests for recursive cognition bridges
# WHY:  Verify bidirectional data flow between bridges
# HOW:  Multi-bridge scenarios with effects propagation

message(STATUS "Adding recursive cognition integration tests...")

# Integration Test: Multi-bridge interaction
add_test_binary(
    test_rcog_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_rcog_integration.cpp
    integration
)

# Integration Test: FEP bridge integration
add_test_binary(
    test_rcog_fep_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_rcog_fep_integration.cpp
    integration
)

# Integration Test: Cognitive Hub bridge integration
add_test_binary(
    test_rcog_hub_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_rcog_hub_integration.cpp
    integration
)

message(STATUS "  Configured recursive cognition integration tests")
