# ============================================================================
# Social Cognition Unit Tests
# ============================================================================
#
# WHAT: Unit tests for social cognition module components
# WHY:  Verify individual component functionality and API contracts
# HOW:  GTest-based tests with comprehensive coverage
#
# COMPONENTS TESTED:
# - FEP Bridge: Free energy principle integration for social cognition

message(STATUS "Adding social cognition unit tests...")

# Test 1: Social FEP Bridge
add_test_binary(
    test_social_fep_bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/test_social_fep_bridge.cpp
    unit
)

message(STATUS "  Configured 1 social cognition unit test")
