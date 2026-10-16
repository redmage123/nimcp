// nimcp_systems_consolidation_part_accessors.c - accessors functions
// Part of nimcp_systems_consolidation.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_systems_consolidation.c


//=============================================================================
// Query API
//=============================================================================

cortical_memory_node_t* systems_consolidation_get_node(
    const systems_consolidation_system_t* system,
    uint64_t node_id)
{
    // WHAT: Guard against invalid input
    if (!system || node_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "systems_consolidation_get_node: system is NULL");
        return NULL;
    }

    // WHAT: Linear search for node
    /* Phase 8: Heartbeat at operation start */
    systems_consolidation_heartbeat("consolidatio_systems_consolidatio", 0.0f);


    for (uint32_t i = 0; i < system->node_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->node_count > 256) {
            systems_consolidation_heartbeat("consolidatio_loop",
                             (float)(i + 1) / (float)system->node_count);
        }

        if (system->cortical_nodes[i]->id == node_id) {
            return system->cortical_nodes[i];
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "systems_consolidation_get_node: validation failed");
    return NULL;  // Not found
}


void systems_consolidation_get_statistics(
    const systems_consolidation_system_t* system,
    uint32_t* total_nodes_out,
    uint64_t* total_replays_out,
    uint64_t* total_transfers_out,
    uint64_t* total_forgotten_out,
    uint32_t* pending_replays_out)
{
    // WHAT: Guard against NULL system
    if (!system) {
        return;
    }

    // WHAT: Return statistics
    /* Phase 8: Heartbeat at operation start */
    systems_consolidation_heartbeat("consolidatio_systems_consolidatio", 0.0f);


    if (total_nodes_out) *total_nodes_out = system->node_count;
    if (total_replays_out) *total_replays_out = system->total_replays;
    if (total_transfers_out) *total_transfers_out = system->total_transfers;
    if (total_forgotten_out) *total_forgotten_out = system->total_forgotten;
    if (pending_replays_out) *pending_replays_out = system->replay_queue_size;
}


//=============================================================================
// Integration API
//=============================================================================

void systems_consolidation_set_engram_system(
    systems_consolidation_system_t* system,
    engram_system_t* engram_system)
{
    // WHAT: Guard against NULL system
    if (!system) {
        return;
    }

    // WHAT: Store non-owning pointer
    /* Phase 8: Heartbeat at operation start */
    systems_consolidation_heartbeat("consolidatio_systems_consolidatio", 0.0f);


    system->engram_system = engram_system;
}


void systems_consolidation_set_sleep_system(
    systems_consolidation_system_t* system,
    void* sleep_system)
{
    // WHAT: Guard against NULL system
    if (!system) {
        return;
    }

    // WHAT: Store opaque pointer
    /* Phase 8: Heartbeat at operation start */
    systems_consolidation_heartbeat("consolidatio_systems_consolidatio", 0.0f);


    system->sleep_system = sleep_system;
}


/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void systems_consolidation_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_systems_consolidation_health_agent = agent;
    }
}
