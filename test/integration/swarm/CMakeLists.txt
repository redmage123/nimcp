# ============================================================================
# Swarm Integration Tests
# ============================================================================

# Test 1: Swarm Communication Integration
add_test_binary(
    test_swarm_communication_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_swarm_communication_integration.cpp
    integration
)

# Test 2: Swarm Workspace Integration
add_test_binary(
    test_swarm_workspace_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_swarm_workspace_integration.cpp
    integration
)

# Test 3: Swarm Consensus Integration
add_test_binary(
    test_swarm_consensus_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_swarm_consensus_integration.cpp
    integration
)

# Test 4: Swarm Brain Integration
add_test_binary(
    test_swarm_brain_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_swarm_brain_integration.cpp
    integration
)

# Test 5: Swarm Gateway Integration
add_test_binary(
    test_swarm_gateway_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_swarm_gateway_integration.cpp
    integration
)

# Test 6: Swarm Conflict Resolution Integration
add_test_binary(
    test_swarm_conflict_resolution_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_swarm_conflict_resolution_integration.cpp
    integration
)

# Test 7: Quorum and Immune Logic Integration
add_test_binary(
    test_quorum_immune_logic_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_quorum_immune_logic_integration.cpp
    integration
)

# Test 8: Swarm Consciousness Integration
add_test_binary(
    test_swarm_consciousness_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_swarm_consciousness_integration.cpp
    integration
)

# Test 9: Enhanced Swarm Consciousness Integration (Peer Callbacks, Remote Phi, Dynamics)
add_test_binary(
    test_swarm_consciousness_enhanced_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_swarm_consciousness_enhanced_integration.cpp
    integration
)

# Test 10: Brain-Swarm Immune Integration
add_test_binary(
    test_brain_swarm_immune_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_brain_swarm_immune_integration.cpp
    integration
)

# Test: Enhanced Swarm Logic Bridge with Consensus Integration
add_test_binary(
    test_swarm_logic_consensus_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_swarm_logic_consensus_integration.cpp
    integration
)

# Test: Swarm Brain Local Integration (Role-Based Asymmetric Brains)
add_test_binary(
    test_swarm_brain_local_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_swarm_brain_local_integration.cpp
    integration
)

# Test: Swarm Memory Bio-Async Integration (2025-12-31)
add_test_binary(
    test_swarm_memory_bio_async_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_swarm_memory_bio_async_integration.cpp
    integration
)

# ============================================================================
# Subdirectories
# ============================================================================

# Sleep Bridge Integration Tests
add_subdirectory(sleep)

# Immune Bridge Integration Tests
add_subdirectory(immune)
