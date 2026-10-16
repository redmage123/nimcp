# Integration Layer CMakeLists.txt
# Layer infrastructure for NIMCP brain architecture

set(INTEGRATION_SOURCES
    # Core infrastructure
    core/nimcp_layer_coordinator.c
    core/nimcp_layer_registry.c
    core/nimcp_inter_layer_router.c

    # Intra-layer coordinators (9 layers)
    intra/physics/nimcp_physics_intra_coordinator.c
    intra/chemistry/nimcp_chemistry_intra_coordinator.c
    intra/biology/nimcp_biology_intra_coordinator.c
    intra/neuromodulatory/nimcp_neuromod_intra_coordinator.c
    intra/sensory/nimcp_sensory_intra_coordinator.c
    intra/memory/nimcp_memory_intra_coordinator.c
    intra/executive/nimcp_executive_intra_coordinator.c
    intra/integration_layer/nimcp_integration_intra_coordinator.c
    intra/superhuman/nimcp_superhuman_intra_coordinator.c
)

# Inter-layer bridges will be added as they are implemented
set(INTEGRATION_BRIDGE_SOURCES
    # Placeholder - bridges to be added
)

# Combine all sources
set(INTEGRATION_ALL_SOURCES
    ${INTEGRATION_SOURCES}
    ${INTEGRATION_BRIDGE_SOURCES}
)

# Export sources to parent scope
set(INTEGRATION_SOURCES ${INTEGRATION_ALL_SOURCES} PARENT_SCOPE)
