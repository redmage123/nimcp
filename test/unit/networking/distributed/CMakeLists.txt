# ============================================================================
# Distributed Cognition Unit Tests
# ============================================================================

# Test: Distributed Cognition Refactored (Async API)
add_test_binary(
    unit_networking_distributed_test_distributed_cognition_refactored
    ${CMAKE_CURRENT_SOURCE_DIR}/test_distributed_cognition_refactored.cpp
    unit
)
