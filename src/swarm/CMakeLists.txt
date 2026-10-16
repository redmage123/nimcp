# =============================================================================
# NIMCP Swarm Module - CMakeLists.txt
# =============================================================================
# Swarm brain coordination for drone/IoT collective intelligence
# =============================================================================

set(SWARM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_swarm_protocol.c
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_swarm_signal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_collective_workspace.c
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_swarm_emergence.c
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_swarm_consensus.c
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_swarm_brain.c
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_swarm_gateway.c
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_swarm_logic_bridge.c
    # Immune bridge sources
    ${CMAKE_CURRENT_SOURCE_DIR}/immune/nimcp_swarm_brain_immune_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/immune/nimcp_swarm_consciousness_immune_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/immune/nimcp_swarm_consensus_immune_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/immune/nimcp_swarm_emergence_immune_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/immune/nimcp_swarm_flocking_immune_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/immune/nimcp_swarm_immune_immune_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/immune/nimcp_swarm_memory_immune_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/immune/nimcp_swarm_pheromone_immune_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/immune/nimcp_swarm_quorum_immune_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/immune/nimcp_swarm_signal_immune_bridge.c
)

# Export to parent scope for inclusion in main library
set(SWARM_SOURCES ${SWARM_SOURCES} PARENT_SCOPE)

# Include directories
target_include_directories(nimcp PUBLIC
    ${CMAKE_SOURCE_DIR}/include/swarm
)
