# ============================================================================
# Mesh Network Unit Tests
# ============================================================================
# Hyperledger-inspired mesh network architecture unit tests
# Phase 1-8 component validation
# ============================================================================

# Test 1: Mesh Types - Core types, ID encoding, string conversions
add_test_binary(
    test_mesh_types
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_types.cpp
    unit
)

# Test 2: Mesh Participant - Registry, credentials, channel membership
add_test_binary(
    test_mesh_participant
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_participant.cpp
    unit
)

# Test 3: Mesh Transaction - Execute-Order-Validate lifecycle
add_test_binary(
    test_mesh_transaction
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_transaction.cpp
    unit
)

# ============================================================================
# Phase 2: Channels & World State
# ============================================================================

# Test 4: Mesh Channel - World state, private data, gossip, convergence
add_test_binary(
    test_mesh_channel
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_channel.cpp
    unit
)

# ============================================================================
# Phase 3: Coordinator Pools
# ============================================================================

# Test 5: Mesh Coordinator - Single coordinator with role, state, participant assignment
add_test_binary(
    test_mesh_coordinator
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_coordinator.cpp
    unit
)

# Test 6: Mesh Coordinator Pool - BFT election, load balancing, failover
add_test_binary(
    test_mesh_coordinator_pool
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_coordinator_pool.cpp
    unit
)

# ============================================================================
# Phase 4: Ordering Service
# ============================================================================

# Test 7: Mesh Ordering - Raft-based ordering, batching, block creation
add_test_binary(
    test_mesh_ordering
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_ordering.cpp
    unit
)

# ============================================================================
# Phase 5: MSP & Endorsement
# ============================================================================

# Test 8: Mesh MSP - Identity, quarantine, credential lifecycle
add_test_binary(
    test_mesh_msp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_msp.cpp
    unit
)

# Test 9: Mesh Endorsement - Policy evaluation, collection, validation
add_test_binary(
    test_mesh_endorsement
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_endorsement.cpp
    unit
)

# ============================================================================
# Phase 6: Topology & Timing
# ============================================================================

# Test 10: Mesh Topology - Hub detection, centrality, coordinator placement
add_test_binary(
    test_mesh_topology
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_topology.cpp
    unit
)

# Test 11: Mesh Timing - Hierarchical pink noise, jitter, synchronization
add_test_binary(
    test_mesh_timing
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_timing.cpp
    unit
)

# ============================================================================
# Phase 7: GPU Channel
# ============================================================================

# Test 12: Mesh GPU - GPU channel, batch processing, multi-GPU coordination
add_test_binary(
    test_mesh_gpu
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_gpu.cpp
    unit
)

# ============================================================================
# Phase 8: Cross-Channel & System Coordinator
# ============================================================================

# Test 13: Mesh Cross-Channel - Routing, arbitration, conflict resolution
add_test_binary(
    test_mesh_cross_channel
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_cross_channel.cpp
    unit
)

# ============================================================================
# Inference Question Tests
# ============================================================================
# WHAT: Tests validating mesh network per inference questions document
# WHY:  Comprehensive code walkthrough validation
# REF:  docs/mesh_inference_questions.md

# Q1: Simple Fact Recall - "What is 2 + 2?"
# Components: Participant, Channel, Transaction (single channel)
add_test_binary(
    test_mesh_q1_simple_recall
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_q1_simple_recall.cpp
    unit
)

# ============================================================================
# Phase 10: Full System Bootstrap
# ============================================================================
# WHAT: Tests for the complete NIMCP mesh network bootstrap system
# WHY:  Validates all modules integrated into unified mesh network

add_test_binary(
    test_mesh_bootstrap
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_bootstrap.cpp
    unit
)

# ============================================================================
# Phase 11: Brain-Inspired Pattern Routing
# ============================================================================
# WHAT: Tests for pattern-based self-selection routing
# WHY:  Validates brain-like routing without predefined transaction types

add_test_binary(
    test_mesh_pattern_routing
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_pattern_routing.cpp
    unit
)

# ============================================================================
# Phase 12: Enhanced Pattern Routing Components
# ============================================================================
# WHAT: Tests for SAT solver and pattern cache with CoW
# WHY:  Validates constraint satisfaction and caching infrastructure

# Test: SAT Solver - DPLL algorithm, endorsement constraints
add_test_binary(
    test_mesh_sat_solver
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_sat_solver.cpp
    unit
)

# Test: Pattern Cache - CoW caching with nimcp_cache integration
add_test_binary(
    test_mesh_pattern_cache
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_pattern_cache.cpp
    unit
)

# ============================================================================
# Phase 13: KG-Mesh Routing Bridge
# ============================================================================
# WHAT: Tests for hybrid KG + pattern routing
# WHY:  Validates dual-process routing combining structure and similarity

# Test: KG Routing Bridge - Hybrid routing, topology filtering, learning
add_test_binary(
    test_mesh_kg_routing_bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_kg_routing_bridge.cpp
    unit
)

# ============================================================================
# Phase 14: Full Bidirectional Mesh Integration
# ============================================================================
# WHAT: Tests for complete bidirectional mesh integration
# WHY:  Validates type-safe registration, bio-async bridge, health/exception

# Test: Module Registry - Type-safe module registration, magic validation
add_test_binary(
    test_mesh_module_registry
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_module_registry.cpp
    unit
)

# Test: Bio Bridge - Bio-async to mesh translation, pattern extraction
add_test_binary(
    test_mesh_bio_bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_bio_bridge.cpp
    unit
)

# Test: Receptive Fields - Pre-defined patterns, neuromodulator gains
add_test_binary(
    test_mesh_receptive_fields
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_receptive_fields.cpp
    unit
)

# Test: Exception Bridge - Exception to antigen routing, immune integration
add_test_binary(
    test_mesh_exception_bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_exception_bridge.cpp
    unit
)

# Test: Health Bridge - Heartbeat aggregation, distributed health monitoring
add_test_binary(
    test_mesh_health_bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_health_bridge.cpp
    unit
)

# Test: BBB Validation - Blood-Brain Barrier validation for mesh network security
add_test_binary(
    test_mesh_bbb_validation
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_bbb_validation.cpp
    unit
)

# ============================================================================
# Phase 15: Deep Integration Components
# ============================================================================
# WHAT: Tests for brain, bio, security, KG, and resilience integration
# WHY:  Validates complete bidirectional mesh integration with all NIMCP subsystems

# Test: Brain Integration - Brain-mesh bidirectional communication
add_test_binary(
    test_mesh_brain_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_brain_integration.cpp
    unit
)

# Test: Bio Integration - Bio-async subsystem mesh integration
add_test_binary(
    test_mesh_bio_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_bio_integration.cpp
    unit
)

# Test: Security Integration - Security subsystem mesh integration
add_test_binary(
    test_mesh_security_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_security_integration.cpp
    unit
)

# Test: KG Integration - Knowledge graph mesh integration
add_test_binary(
    test_mesh_kg_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_kg_integration.cpp
    unit
)

# Test: Resilience Integration - Fault tolerance and resilience mesh integration
add_test_binary(
    test_mesh_resilience_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_resilience_integration.cpp
    unit
)

# ============================================================================
# Phase 16: Cycle Coordinator Integration
# ============================================================================
# WHAT: Tests for mesh-cycle coordinator integration
# WHY:  Validates brain oscillation synchronization with mesh network timing

# Test: Cycle Coordinator - Brain oscillation sync, phase alignment, frequency coordination
add_test_binary(
    test_mesh_cycle_coordinator
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_cycle_coordinator.cpp
    unit
)

# ============================================================================
# Phase 17: Brain Region Mesh Integration
# ============================================================================
# WHAT: Tests for individual brain region mesh integration
# WHY:  Validates amygdala, basal ganglia, hippocampus, thalamus mesh coordination

add_subdirectory(integration)
