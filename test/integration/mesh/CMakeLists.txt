# ============================================================================
# Mesh Network Integration Tests
# ============================================================================
# Hyperledger-inspired mesh network architecture integration tests
# Tests cross-component interactions and workflows
# ============================================================================

# DISABLED: Tests 1-5 need API updates to match current mesh API
# TODO: Update mesh_channel_*, mesh_coordinator_pool_*, mesh_msp_*, mesh_ordering_* usage
#
# # Test 1: Mesh Channel Integration - World state, gossip, participants
# add_test_binary(
#     test_mesh_channel_integration
#     ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_channel_integration.cpp
#     integration
# )
#
# # Test 2: Mesh Coordinator Pool Integration - BFT election, load balancing, failover
# add_test_binary(
#     test_mesh_coordinator_pool_integration
#     ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_coordinator_pool_integration.cpp
#     integration
# )
#
# # Test 3: Mesh Transaction Flow Integration - Complete EOV lifecycle
# add_test_binary(
#     test_mesh_transaction_flow_integration
#     ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_transaction_flow_integration.cpp
#     integration
# )
#
# # Test 4: Mesh MSP Integration - BBB, immune, credentials, quarantine
# add_test_binary(
#     test_mesh_msp_integration
#     ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_msp_integration.cpp
#     integration
# )
#
# # ============================================================================
# # Bootstrap Integration Tests
# # ============================================================================
#
# # Test 5: Mesh Bootstrap Integration - Full system bootstrap with all components
# add_test_binary(
#     test_mesh_bootstrap_integration
#     ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_bootstrap_integration.cpp
#     integration
# )

# ============================================================================
# Enhanced Pattern Routing Integration Tests
# ============================================================================

# Test 6: Enhanced Routing Integration - SAT solver + pattern cache together
add_test_binary(
    test_mesh_enhanced_routing_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_enhanced_routing_integration.cpp
    integration
)

# ============================================================================
# KG-Mesh Routing Bridge Integration Tests
# ============================================================================

# Test 7: KG Routing Bridge Integration - Hybrid routing with brain topology
add_test_binary(
    test_mesh_kg_routing_bridge_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_kg_routing_bridge_integration.cpp
    integration
)

# ============================================================================
# Phase 15: Deep Integration Component Tests
# ============================================================================
# WHAT: Integration tests for brain, bio, security, KG, and resilience
# WHY:  Validates cross-component interactions in mesh integration layer

# Test 8: Brain Integration - Brain-mesh cross-component interactions
add_test_binary(
    test_mesh_brain_integration_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_brain_integration_integration.cpp
    integration
)

# Test 9: Bio Integration - Bio-async cross-component interactions
add_test_binary(
    test_mesh_bio_integration_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_bio_integration_integration.cpp
    integration
)

# Test 10: Security Integration - Security cross-component interactions
add_test_binary(
    test_mesh_security_integration_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_security_integration_integration.cpp
    integration
)

# Test 11: KG Integration - Knowledge graph cross-component interactions
add_test_binary(
    test_mesh_kg_integration_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_kg_integration_integration.cpp
    integration
)

# Test 12: Resilience Integration - Fault tolerance cross-component interactions
add_test_binary(
    test_mesh_resilience_integration_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_resilience_integration_integration.cpp
    integration
)

# ============================================================================
# Phase 16: Mesh Bidirectional Integration Tests
# ============================================================================
# WHAT: Comprehensive integration tests for mesh bidirectional communication
# WHY:  Validates end-to-end flows between mesh and all integrated systems

# Test 13: Brain Bootstrap Integration - Full brain-mesh bidirectional flow
add_test_binary(
    test_mesh_brain_bootstrap_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_brain_bootstrap_integration.cpp
    integration
)

# Test 14: Bio Flow Integration - Bio-router to mesh message flow
add_test_binary(
    test_mesh_bio_flow_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_bio_flow_integration.cpp
    integration
)

# DISABLED: Tests 15-16 need API updates to match current mesh API
# TODO: Update NIMCP_ERROR_* codes and fix linker issues with static members
#
# # Test 15: Security Flow Integration - Exception to immune through mesh
# add_test_binary(
#     test_mesh_security_flow_integration
#     ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_security_flow_integration.cpp
#     integration
# )
#
# # Test 16: KG Sync Integration - KG and mesh topology synchronization
# add_test_binary(
#     test_mesh_kg_sync_integration
#     ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_kg_sync_integration.cpp
#     integration
# )

# DISABLED: Test needs API updates to match current mesh API
# TODO: Update mesh coordinator pool and resilience flow API usage
#
# # Test 17: Resilience Flow Integration - Health monitoring and recovery
# add_test_binary(
#     test_mesh_resilience_flow_integration
#     ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_resilience_flow_integration.cpp
#     integration
# )

# ============================================================================
# Phase 16: Cycle Coordinator Integration Tests
# ============================================================================
# WHAT: Integration tests for mesh-cycle coordinator
# WHY:  Validates cross-component interactions for brain oscillation synchronization

# Test 18: Cycle Coordinator Integration - Mesh timing with brain oscillation sync
add_test_binary(
    test_mesh_cycle_coordinator_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_cycle_coordinator_integration.cpp
    integration
)

# ============================================================================
# Phase 17: BBB Validation Integration Tests
# ============================================================================
# WHAT: Integration tests for BBB validation across mesh components
# WHY:  Validates security perimeter for mesh channel, participant, topology

# Test 19: BBB Integration - Blood-Brain Barrier validation across mesh components
add_test_binary(
    test_mesh_bbb_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_bbb_integration.cpp
    integration
)
