# ============================================================================
# Mesh Network End-to-End Tests
# ============================================================================
# Full system integration tests for complete mesh network workflows
# Tests realistic multi-channel scenarios with all components
# ============================================================================

# DISABLED: Tests need API updates to match current mesh API
# TODO: Update these tests to use current mesh_transaction_create(type, proposer, channel) API
#
# # Test 1: Full Transaction E2E - Complete EOV flow with all components
# add_e2e_test(
#     e2e_test_mesh_full_transaction
#     ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_full_transaction_e2e.cpp
# )
# add_test(NAME e2e_mesh_full_transaction COMMAND e2e_test_mesh_full_transaction)
# set_tests_properties(e2e_mesh_full_transaction PROPERTIES LABELS "e2e;mesh")
#
# # Test 2: MSP Security E2E - Credential lifecycle, BBB, immune integration
# add_e2e_test(
#     e2e_test_mesh_msp_security
#     ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_msp_security_e2e.cpp
# )
# add_test(NAME e2e_mesh_msp_security COMMAND e2e_test_mesh_msp_security)
# set_tests_properties(e2e_mesh_msp_security PROPERTIES LABELS "e2e;mesh;security")
#
# # Test 3: System Integration E2E - Full brain hemisphere integration
# add_e2e_test(
#     e2e_test_mesh_system_integration
#     ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_system_integration_e2e.cpp
# )
# add_test(NAME e2e_mesh_system_integration COMMAND e2e_test_mesh_system_integration)
# set_tests_properties(e2e_mesh_system_integration PROPERTIES LABELS "e2e;mesh;integration")

# ============================================================================
# Bootstrap E2E Tests
# ============================================================================

# Test 4: Bootstrap E2E - Full brain simulation scenarios
add_e2e_test(
    e2e_test_mesh_bootstrap
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_bootstrap_e2e.cpp
)

add_test(NAME e2e_mesh_bootstrap COMMAND e2e_test_mesh_bootstrap)
set_tests_properties(e2e_mesh_bootstrap PROPERTIES LABELS "e2e;mesh;bootstrap")

# ============================================================================
# Enhanced Pattern Routing E2E Tests
# ============================================================================

# Test 5: Enhanced Routing E2E - SAT solver + pattern cache full pipeline
add_e2e_test(
    e2e_test_mesh_enhanced_routing
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_enhanced_routing_e2e.cpp
)

add_test(NAME e2e_mesh_enhanced_routing COMMAND e2e_test_mesh_enhanced_routing)
set_tests_properties(e2e_mesh_enhanced_routing PROPERTIES LABELS "e2e;mesh;routing")

# ============================================================================
# KG-Mesh Routing Bridge E2E Tests
# ============================================================================

# Test 6: KG Routing Bridge E2E - Full hybrid routing pipeline
add_e2e_test(
    e2e_test_mesh_kg_routing_bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_kg_routing_bridge_e2e.cpp
)

add_test(NAME e2e_mesh_kg_routing_bridge COMMAND e2e_test_mesh_kg_routing_bridge)
set_tests_properties(e2e_mesh_kg_routing_bridge PROPERTIES LABELS "e2e;mesh;routing;kg")

# ============================================================================
# Phase 15: Bidirectional Integration E2E Tests
# ============================================================================
# WHAT: End-to-end tests for bidirectional mesh integration
# WHY:  Validates complete brain simulation, bio-pipeline, and security resilience

# DISABLED: Test needs API updates to match current mesh API
# TODO: Update this test to use current mesh_transaction_create(type, proposer, channel) API
#
# # Test 7: Complete Brain E2E - Full brain with all regions through mesh
# add_e2e_test(
#     e2e_test_mesh_complete_brain
#     ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_complete_brain_e2e.cpp
# )
#
# add_test(NAME e2e_mesh_complete_brain COMMAND e2e_test_mesh_complete_brain)
# set_tests_properties(e2e_mesh_complete_brain PROPERTIES LABELS "e2e;mesh;brain;complete")

# DISABLED: Test needs API updates to match current mesh API
# TODO: Update to use current mesh_transaction_create(type, proposer, channel) API
#
# # Test 8: Bio Transaction Pipeline E2E - Bio-async to mesh full pipeline
# add_e2e_test(
#     e2e_test_mesh_bio_transaction_pipeline
#     ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_bio_transaction_pipeline_e2e.cpp
# )
#
# add_test(NAME e2e_mesh_bio_transaction_pipeline COMMAND e2e_test_mesh_bio_transaction_pipeline)
# set_tests_properties(e2e_mesh_bio_transaction_pipeline PROPERTIES LABELS "e2e;mesh;bio;pipeline")

# DISABLED: Test needs API updates to match current mesh coordinator pool API
# TODO: Update to use current mesh_coordinator_pool_create(config, registry, channel) API
#
# # Test 9: Security Resilience E2E - Exception to immune recovery full flow
# add_e2e_test(
#     e2e_test_mesh_security_resilience
#     ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_security_resilience_e2e.cpp
# )
#
# add_test(NAME e2e_mesh_security_resilience COMMAND e2e_test_mesh_security_resilience)
# set_tests_properties(e2e_mesh_security_resilience PROPERTIES LABELS "e2e;mesh;security;resilience")

# ============================================================================
# Legacy Deep Integration E2E Tests
# ============================================================================
# WHAT: Legacy end-to-end tests for deep integration components
# WHY:  Validates complete brain, security, and resilience mesh pipelines

# Test 10: Brain Integration E2E - Full brain-mesh integration pipeline
add_e2e_test(
    e2e_test_mesh_brain_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_brain_integration_e2e.cpp
)

add_test(NAME e2e_mesh_brain_integration COMMAND e2e_test_mesh_brain_integration)
set_tests_properties(e2e_mesh_brain_integration PROPERTIES LABELS "e2e;mesh;brain;integration")

# Test 11: Security Integration E2E - Full security-mesh integration pipeline
add_e2e_test(
    e2e_test_mesh_security_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_security_integration_e2e.cpp
)

add_test(NAME e2e_mesh_security_integration COMMAND e2e_test_mesh_security_integration)
set_tests_properties(e2e_mesh_security_integration PROPERTIES LABELS "e2e;mesh;security;integration")

# Test 12: Resilience Integration E2E - Full resilience-mesh integration pipeline
add_e2e_test(
    e2e_test_mesh_resilience_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_resilience_integration_e2e.cpp
)

add_test(NAME e2e_mesh_resilience_integration COMMAND e2e_test_mesh_resilience_integration)
set_tests_properties(e2e_mesh_resilience_integration PROPERTIES LABELS "e2e;mesh;resilience;integration")

# ============================================================================
# Phase 16: Cycle Coordinator E2E Tests
# ============================================================================
# WHAT: End-to-end tests for mesh-cycle coordinator integration
# WHY:  Validates complete brain oscillation synchronization pipeline

# Test 13: Cycle Coordinator E2E - Full mesh-brain oscillation sync pipeline
add_e2e_test(
    e2e_test_mesh_cycle_coordinator
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_cycle_coordinator_e2e.cpp
)

add_test(NAME e2e_mesh_cycle_coordinator COMMAND e2e_test_mesh_cycle_coordinator)
set_tests_properties(e2e_mesh_cycle_coordinator PROPERTIES LABELS "e2e;mesh;cycle;coordinator")

# ============================================================================
# Mesh Coordination E2E Test (GTest C++)
# ============================================================================
# DISABLED: Test needs API updates to match current mesh coordinator pool API
# TODO: Update to use current mesh_coordinator_pool_create(config, registry, channel) API
#
# WHAT: End-to-end tests for multi-region mesh coordination
# WHY:  Verify mesh communication, leader election, failover, load balancing
# HOW:  Tests full mesh coordination pipeline with stress testing
#
# TESTS:
# 1. CoordinatorLifecycle - Create, configure, start, stop, destroy
# 2. MultiRegionCommunication - Region message routing
# 3. LeaderElectionSimulation - Raft-style consensus
# 4. HeartbeatMechanism - Heartbeat monitoring
# 5. CoordinatorFailureDetection - Failure detection and recovery
# 6. LeaderFailover - Leader failover handling
# 7. LoadBalancing - Message distribution and metrics
# 8. HighVolumeMessages - High-volume message handling
# 9. ConcurrentCoordinatorUpdates - Concurrent update handling
# 10. CompleteMeshPipeline - Complete pipeline test
# ============================================================================
# if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_coordination_e2e.cpp)
#     add_executable(e2e_test_mesh_coordination
#         ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_coordination_e2e.cpp
#         ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
#     )
#     target_include_directories(e2e_test_mesh_coordination PRIVATE
#         ${CMAKE_SOURCE_DIR}/include
#         ${CMAKE_SOURCE_DIR}/test/e2e
#         ${CMAKE_SOURCE_DIR}/src/lib
#     )
#     target_link_libraries(e2e_test_mesh_coordination PRIVATE
#         nimcp
#         GTest::gtest
#         GTest::gtest_main
#         Threads::Threads
#         m
#     )
#     target_compile_features(e2e_test_mesh_coordination PRIVATE cxx_std_17)
#     target_compile_definitions(e2e_test_mesh_coordination PRIVATE
#         NIMCP_TESTING
#         NIMCP_E2E_TESTING
#     )
#     add_test(NAME e2e_test_mesh_coordination COMMAND e2e_test_mesh_coordination)
#     set_tests_properties(e2e_test_mesh_coordination PROPERTIES
#         LABELS "e2e;mesh;coordination;failover"
#         TIMEOUT 300
#     )
#     message(STATUS "    Added: e2e_test_mesh_coordination (GTest C++)")
# endif()
