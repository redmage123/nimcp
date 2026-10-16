// nimcp_fep_orchestrator_part_helpers.c - helpers functions
// Part of nimcp_fep_orchestrator.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_fep_orchestrator.c


/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Find bridge entry by ID
 */
static fep_bridge_entry_t* find_bridge_by_id(
    fep_orchestrator_t* orchestrator,
    uint32_t bridge_id
) {
    if (!orchestrator) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "orchestrator is NULL");

        return NULL;

    }
    for (uint32_t i = 0; i < orchestrator->bridge_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && orchestrator->bridge_count > 256) {
            fep_orchestrator_heartbeat("fep_orchestr_loop",
                             (float)(i + 1) / (float)orchestrator->bridge_count);
        }

        if (orchestrator->bridges[i].bridge_id == bridge_id) {
            return &orchestrator->bridges[i];
        }
    }
    return NULL;
}


/**
 * @brief Update single bridge and track statistics
 */
static int update_single_bridge(
    fep_orchestrator_t* orchestrator,
    fep_bridge_entry_t* entry,
    uint64_t current_time_ms
) {
    if (!entry->enabled || !entry->update_fn) return 0;

    /* Check if bridge's category is enabled */
    if (!orchestrator->config.categories[entry->category].enabled) return 0;

    uint64_t start_us = nimcp_platform_time_monotonic_us();

    int result = entry->update_fn(entry->handle);

    uint64_t elapsed_us = nimcp_platform_time_monotonic_us() - start_us;

    entry->last_update_time = current_time_ms;
    entry->update_count++;
    entry->total_update_time_us += elapsed_us;

    if (result != 0) {
        orchestrator->stats.update_errors++;
        if (orchestrator->config.enable_logging) {
            NIMCP_LOGGING_WARN("FEP bridge update failed: %s (id=%u)",
                entry->bridge_name, entry->bridge_id);
        }
    }

    return result == 0 ? 1 : 0;
}
