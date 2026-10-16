# ============================================================================
# Mesh Network Regression Tests
# ============================================================================
# Tests for stability issues, race conditions, edge cases, and backward compat
# Catch regressions in mesh network behavior
# ============================================================================

# DISABLED: Tests 1-2 need API updates to match current mesh API
# TODO: Update mesh_coordinator_pool_* API usage
#
# # Test 1: Mesh Stability Regression - Race conditions, resource exhaustion, edge cases
# add_test_binary(
#     test_mesh_stability_regression
#     ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_stability_regression.cpp
#     regression
# )
#
# # Test 2: Mesh Consensus Regression - BFT edge cases, split-brain, partition recovery
# add_test_binary(
#     test_mesh_consensus_regression
#     ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_consensus_regression.cpp
#     regression
# )

# DISABLED: Tests 3-4 need API updates to match current mesh API
# TODO: Update mesh_transaction_* and mesh_channel_* API usage
#
# # Test 3: Mesh Transaction Regression - EOV edge cases, timeout handling, retry logic
# add_test_binary(
#     test_mesh_transaction_regression
#     ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_transaction_regression.cpp
#     regression
# )
#
# # Test 4: Mesh Performance Regression - Throughput, latency, memory usage benchmarks
# add_test_binary(
#     test_mesh_performance_regression
#     ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_performance_regression.cpp
#     regression
# )

# ============================================================================
# Bootstrap Regression Tests
# ============================================================================

# Test 5: Mesh Bootstrap Regression - Stability, edge cases, race conditions
add_test_binary(
    test_mesh_bootstrap_regression
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_bootstrap_regression.cpp
    regression
)
set_tests_properties(test_mesh_bootstrap_regression PROPERTIES
    TIMEOUT 120 RESOURCE_LOCK "brain_heavy"
)

# ============================================================================
# Enhanced Pattern Routing Regression Tests
# ============================================================================

# Test 6: Enhanced Routing Regression - SAT solver + pattern cache stability
add_test_binary(
    test_mesh_enhanced_routing_regression
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_enhanced_routing_regression.cpp
    regression
)

# ============================================================================
# KG-Mesh Routing Bridge Regression Tests
# ============================================================================

# Test 7: KG Routing Bridge Regression - Hybrid routing stability and edge cases
add_test_binary(
    test_mesh_kg_routing_bridge_regression
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_kg_routing_bridge_regression.cpp
    regression
)

# ============================================================================
# Phase 15: Bidirectional Integration Regression Tests
# ============================================================================
# WHAT: Regression tests for bidirectional mesh integration components
# WHY:  Catch regressions in registration, routing, and security

# DISABLED: Tests 8-10 need API updates to match current mesh API
# TODO: Update NIMCP_ERROR_*, mesh_cross_router_*, mesh_exception_* API usage
#
# # Test 8: Registration Regression - Scale, cycles, memory pressure, invalid pointers
# add_test_binary(
#     test_mesh_registration_regression
#     ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_registration_regression.cpp
#     regression
# )
#
# # Test 9: Routing Regression - High volume, cross-channel, concurrency, failures
# add_test_binary(
#     test_mesh_routing_regression
#     ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_routing_regression.cpp
#     regression
# )
#
# # Test 10: Security Regression - Exception flood, BBB attack, quarantine at scale
# add_test_binary(
#     test_mesh_security_regression
#     ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_security_regression.cpp
#     regression
# )

# ============================================================================
# Legacy Deep Integration Regression Tests
# ============================================================================
# WHAT: Legacy regression tests for deep integration components
# WHY:  Catch regressions in brain, security, and resilience mesh integration

# Test 11: Brain Integration Regression - Brain-mesh stability and edge cases
add_test_binary(
    test_mesh_brain_integration_regression
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_brain_integration_regression.cpp
    regression
)

# Test 12: Security Integration Regression - Security mesh stability and edge cases
add_test_binary(
    test_mesh_security_integration_regression
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_security_integration_regression.cpp
    regression
)

# Test 13: Resilience Integration Regression - Fault tolerance stability and edge cases
add_test_binary(
    test_mesh_resilience_integration_regression
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_resilience_integration_regression.cpp
    regression
)

# ============================================================================
# Phase 16: Cycle Coordinator Regression Tests
# ============================================================================
# WHAT: Regression tests for mesh-cycle coordinator integration
# WHY:  Catch regressions in brain oscillation synchronization and timing

# Test 14: Cycle Coordinator Regression - Timing drift, phase lock stability, edge cases
add_test_binary(
    test_mesh_cycle_coordinator_regression
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_cycle_coordinator_regression.cpp
    regression
)
set_tests_properties(test_mesh_cycle_coordinator_regression PROPERTIES
    TIMEOUT 120 RESOURCE_LOCK "brain_heavy"
)

# ============================================================================
# Walkthrough Fix Verification Tests
# ============================================================================
# WHAT: Verify P1, P2, P3 fixes across mesh module files
# WHY:  Ensure walkthrough-identified issues are properly fixed and don't regress

# Test 15: Walkthrough Fixes - Thread safety, false positive removal, dead code fixes
add_test_binary(
    test_mesh_walkthrough_fixes
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_walkthrough_fixes.cpp
    regression
)
