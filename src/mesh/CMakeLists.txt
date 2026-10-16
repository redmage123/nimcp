# ============================================================================
# NIMCP Mesh Network Source Files
# ============================================================================
# WHAT: CMake configuration for mesh network module sources
# WHY:  Organize mesh network files for modular builds
# HOW:  Define source file lists for inclusion in main library
#
# NOTE: These files are included via src/lib/CMakeLists.txt in NIMCP_CORE_SOURCES
#       This file defines the module organization for reference
# ============================================================================

# Phase 1: Foundation & Types
set(MESH_PHASE1_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_participant.c
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_transaction.c
)

# Phase 2: Channels & World State
set(MESH_PHASE2_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_channel.c
)

# Phase 3: Coordinator Pools
set(MESH_PHASE3_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_coordinator.c
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_coordinator_pool.c
)

# Phase 4: Ordering Service
set(MESH_PHASE4_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_ordering.c
)

# Phase 5: MSP & Endorsement
set(MESH_PHASE5_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_msp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_endorsement.c
)

# Phase 6: Topology & Timing
set(MESH_PHASE6_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_topology.c
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_timing.c
)

# Phase 7: GPU Channel
set(MESH_PHASE7_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_gpu.c
)

# Phase 8: Cross-Channel & System Coordinator
set(MESH_PHASE8_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_cross_channel.c
)

# Phase 9: Adapter Framework & Integration Manager
set(MESH_PHASE9_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_adapter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_integration.c
)

# Phase 10: Full System Bootstrap
set(MESH_PHASE10_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_bootstrap.c
)

# Phase 11: Brain-Inspired Pattern Routing
set(MESH_PHASE11_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_pattern_routing.c
)

# Phase 12: Enhanced Pattern Routing Components
set(MESH_PHASE12_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_sat_solver.c
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_pattern_cache.c
)

# Phase 13: KG-Mesh Routing Bridge
set(MESH_PHASE13_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_kg_routing_bridge.c
)

# Phase 14: Full Bidirectional Mesh Integration
set(MESH_PHASE14_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_module_registry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_bio_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_receptive_fields.c
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_exception_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_health_bridge.c
)

# Phase 15: Deep Integration Components
# WHAT: Brain, bio, security, KG, and resilience integration modules
# WHY:  Complete bidirectional mesh integration with all NIMCP subsystems
set(MESH_PHASE15_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_brain_integration.c
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_bio_integration.c
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_security_integration.c
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_kg_integration.c
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_resilience_integration.c
)

# Phase 16: Cycle Coordinator Integration
# WHAT: Mesh-cycle coordinator integration for brain oscillation synchronization
# WHY:  Enables mesh network timing to synchronize with neural cycle patterns
set(MESH_PHASE16_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_cycle_coordinator.c
)

# Phase 17: Core Brain Module Mesh Integrations
# WHAT: Direct mesh integrations for hippocampus, amygdala, basal ganglia, thalamus, global workspace
# WHY:  Enable coordinated brain module operations via distributed mesh consensus
set(MESH_PHASE17_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_hippocampus_integration.c
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_amygdala_integration.c
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_basal_ganglia_integration.c
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_thalamus_integration.c
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_global_workspace_integration.c
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_mesh_medulla_integration.c
)

# Combined mesh sources for export
set(NIMCP_MESH_SOURCES
    ${MESH_PHASE1_SOURCES}
    ${MESH_PHASE2_SOURCES}
    ${MESH_PHASE3_SOURCES}
    ${MESH_PHASE4_SOURCES}
    ${MESH_PHASE5_SOURCES}
    ${MESH_PHASE6_SOURCES}
    ${MESH_PHASE7_SOURCES}
    ${MESH_PHASE8_SOURCES}
    ${MESH_PHASE9_SOURCES}
    ${MESH_PHASE10_SOURCES}
    ${MESH_PHASE11_SOURCES}
    ${MESH_PHASE12_SOURCES}
    ${MESH_PHASE13_SOURCES}
    ${MESH_PHASE14_SOURCES}
    ${MESH_PHASE15_SOURCES}
    ${MESH_PHASE16_SOURCES}
    ${MESH_PHASE17_SOURCES}
    PARENT_SCOPE
)
