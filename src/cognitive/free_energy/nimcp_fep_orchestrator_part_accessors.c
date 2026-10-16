// nimcp_fep_orchestrator_part_accessors.c - accessors functions
// Part of nimcp_fep_orchestrator.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_fep_orchestrator.c


/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int fep_orchestrator_default_config(fep_orchestrator_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    fep_orchestrator_heartbeat("fep_orchestr_default_config", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");
    
    memset(config, 0, sizeof(fep_orchestrator_config_t));
    
    /* Global settings */
    config->max_bridges = FEP_ORCHESTRATOR_MAX_BRIDGES;
    config->enable_auto_update = true;
    config->global_update_interval_ms = 0;  /* Use per-category intervals */
    
    /* Integration enables */
    config->enable_bio_async = true;
    config->enable_brain_immune = true;
    config->enable_statistics = true;
    config->enable_logging = true;
    
    /* Performance tuning */
    config->max_updates_per_cycle = 0;  /* All bridges */
    config->update_time_budget_ms = 0;  /* Unlimited */
    
    /* Category-specific intervals (biologically-inspired timescales) */
    config->categories[FEP_BRIDGE_CATEGORY_COGNITIVE].enabled = true;
    config->categories[FEP_BRIDGE_CATEGORY_COGNITIVE].update_interval_ms = FEP_UPDATE_INTERVAL_COGNITIVE;
    
    config->categories[FEP_BRIDGE_CATEGORY_SWARM].enabled = true;
    config->categories[FEP_BRIDGE_CATEGORY_SWARM].update_interval_ms = FEP_UPDATE_INTERVAL_SWARM;
    
    config->categories[FEP_BRIDGE_CATEGORY_SECURITY].enabled = true;
    config->categories[FEP_BRIDGE_CATEGORY_SECURITY].update_interval_ms = FEP_UPDATE_INTERVAL_SECURITY;
    
    config->categories[FEP_BRIDGE_CATEGORY_PLASTICITY].enabled = true;
    config->categories[FEP_BRIDGE_CATEGORY_PLASTICITY].update_interval_ms = FEP_UPDATE_INTERVAL_PLASTICITY;
    
    config->categories[FEP_BRIDGE_CATEGORY_MIDDLEWARE].enabled = true;
    config->categories[FEP_BRIDGE_CATEGORY_MIDDLEWARE].update_interval_ms = FEP_UPDATE_INTERVAL_MIDDLEWARE;
    
    config->categories[FEP_BRIDGE_CATEGORY_PERCEPTION].enabled = true;
    config->categories[FEP_BRIDGE_CATEGORY_PERCEPTION].update_interval_ms = FEP_UPDATE_INTERVAL_PERCEPTION;
    
    config->categories[FEP_BRIDGE_CATEGORY_ASYNC].enabled = true;
    config->categories[FEP_BRIDGE_CATEGORY_ASYNC].update_interval_ms = FEP_UPDATE_INTERVAL_ASYNC;
    
    config->categories[FEP_BRIDGE_CATEGORY_GLIAL].enabled = true;
    config->categories[FEP_BRIDGE_CATEGORY_GLIAL].update_interval_ms = FEP_UPDATE_INTERVAL_GLIAL;
    
    config->categories[FEP_BRIDGE_CATEGORY_CORE].enabled = true;
    config->categories[FEP_BRIDGE_CATEGORY_CORE].update_interval_ms = FEP_UPDATE_INTERVAL_CORE;

    config->categories[FEP_BRIDGE_CATEGORY_JEPA].enabled = true;
    config->categories[FEP_BRIDGE_CATEGORY_JEPA].update_interval_ms = FEP_UPDATE_INTERVAL_JEPA;

    /* Continuous scheduling defaults (disabled by default for backward compat) */
    config->continuous_schedule.enabled = false;
    config->continuous_schedule.min_interval_ms = FEP_CONTINUOUS_MIN_INTERVAL_MS;
    config->continuous_schedule.max_interval_ms = FEP_CONTINUOUS_MAX_INTERVAL_MS;
    config->continuous_schedule.decay_rate = FEP_CONTINUOUS_DECAY_RATE;
    config->continuous_schedule.fe_scale = FEP_CONTINUOUS_FE_SCALE;

    return 0;
}


int fep_orchestrator_set_bridge_enabled(
    fep_orchestrator_t* orchestrator,
    uint32_t bridge_id,
    bool enabled
) {
    /* Phase 8: Heartbeat at operation start */
    fep_orchestrator_heartbeat("fep_orchestr_set_bridge_enabled", 0.0f);


    NIMCP_CHECK_THROW(orchestrator, NIMCP_ERROR_NULL_POINTER, "orchestrator is NULL");
    
    nimcp_platform_mutex_lock(orchestrator->mutex);
    
    fep_bridge_entry_t* entry = find_bridge_by_id(orchestrator, bridge_id);
    if (!entry) {
        nimcp_platform_mutex_unlock(orchestrator->mutex);
        return NIMCP_ERROR_INVALID_PARAM;
    }
    
    if (entry->enabled != enabled) {
        entry->enabled = enabled;
        if (enabled) {
            orchestrator->stats.active_bridges++;
        } else {
            orchestrator->stats.active_bridges--;
        }
    }
    
    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return 0;
}


const fep_bridge_entry_t* fep_orchestrator_get_bridge(
    const fep_orchestrator_t* orchestrator,
    uint32_t bridge_id
) {
    if (!orchestrator) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "orchestrator is NULL");

        return NULL;

    }
    
    /* Phase 8: Heartbeat at operation start */
    fep_orchestrator_heartbeat("fep_orchestr_get_bridge", 0.0f);


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
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_orchestrator_get_bridge: validation failed");
    return NULL;
}


uint32_t fep_orchestrator_get_bridges_by_category(
    const fep_orchestrator_t* orchestrator,
    fep_bridge_category_t category,
    const fep_bridge_entry_t** bridges,
    uint32_t max_bridges
) {
    if (!orchestrator || !bridges || category >= FEP_BRIDGE_CATEGORY_COUNT) return 0;
    
    /* Phase 8: Heartbeat at operation start */
    fep_orchestrator_heartbeat("fep_orchestr_get_bridges_by_categ", 0.0f);


    uint32_t count = 0;
    for (uint32_t i = 0; i < orchestrator->bridge_count && count < max_bridges; i++) {
        if (orchestrator->bridges[i].category == category) {
            bridges[count++] = &orchestrator->bridges[i];
        }
    }
    return count;
}


int fep_orchestrator_set_category_enabled(
    fep_orchestrator_t* orchestrator,
    fep_bridge_category_t category,
    bool enabled
) {
    /* Phase 8: Heartbeat at operation start */
    fep_orchestrator_heartbeat("fep_orchestr_set_category_enabled", 0.0f);


    NIMCP_CHECK_THROW(orchestrator, NIMCP_ERROR_NULL_POINTER, "orchestrator is NULL");
    NIMCP_CHECK_THROW(category < FEP_BRIDGE_CATEGORY_COUNT, NIMCP_ERROR_INVALID_PARAM, "invalid bridge category");
    
    nimcp_platform_mutex_lock(orchestrator->mutex);
    orchestrator->config.categories[category].enabled = enabled;
    nimcp_platform_mutex_unlock(orchestrator->mutex);
    
    return 0;
}


int fep_orchestrator_get_category_config(
    const fep_orchestrator_t* orchestrator,
    fep_bridge_category_t category,
    fep_category_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    fep_orchestrator_heartbeat("fep_orchestr_get_category_config", 0.0f);


    NIMCP_CHECK_THROW(orchestrator && config, NIMCP_ERROR_NULL_POINTER, "orchestrator or config is NULL");
    NIMCP_CHECK_THROW(category < FEP_BRIDGE_CATEGORY_COUNT, NIMCP_ERROR_INVALID_PARAM, "invalid bridge category");
    
    *config = orchestrator->config.categories[category];
    return 0;
}


uint32_t fep_orchestrator_get_bridges_for_module(
    const fep_orchestrator_t* orchestrator,
    const char* module_name,
    const fep_bridge_entry_t** bridges,
    uint32_t max_bridges)
{
    if (!orchestrator || !module_name || !bridges || max_bridges == 0) {
        return 0;
    }

    /* If KG not connected, fall back to searching all bridges by name */
    /* Phase 8: Heartbeat at operation start */
    fep_orchestrator_heartbeat("fep_orchestr_get_bridges_for_modu", 0.0f);


    fep_orchestrator_t* mutable_orch = (fep_orchestrator_t*)orchestrator;
    nimcp_platform_mutex_lock(mutable_orch->mutex);

    uint32_t count = 0;

    if (orchestrator->kg_connected && kg_is_available(&orchestrator->kg_context)) {
        /* Query KG for module node */
        brain_kg_node_id_t module_node = kg_find_node_safe(
            &orchestrator->kg_context, module_name);

        if (module_node != BRAIN_KG_INVALID_NODE) {
            /* Find bridges connected to this module */
            for (uint32_t i = 0; i < orchestrator->bridge_count && count < max_bridges; i++) {
                const fep_bridge_entry_t* entry = &orchestrator->bridges[i];
                if (!entry->enabled) continue;

                /* Check if bridge is connected to module via KG */
                if (entry->kg_node_id != BRAIN_KG_INVALID_NODE) {
                    if (brain_kg_are_connected(orchestrator->kg_context.kg,
                                               entry->kg_node_id, module_node)) {
                        bridges[count++] = entry;
                    }
                }
            }
        }
    } else {
        /* Fallback: search bridges by name substring */
        for (uint32_t i = 0; i < orchestrator->bridge_count && count < max_bridges; i++) {
            const fep_bridge_entry_t* entry = &orchestrator->bridges[i];
            if (!entry->enabled) continue;

            if (entry->bridge_name && strstr(entry->bridge_name, module_name)) {
                bridges[count++] = entry;
            }
        }
    }

    nimcp_platform_mutex_unlock(mutable_orch->mutex);
    return count;
}


int fep_orchestrator_get_topology_summary(
    const fep_orchestrator_t* orchestrator,
    char* summary,
    size_t summary_size)
{
    if (!orchestrator || !summary || summary_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_orchestrator_get_topology_summary: required parameter is NULL (orchestrator, summary)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_orchestrator_heartbeat("fep_orchestr_get_topology_summary", 0.0f);


    fep_orchestrator_t* mutable_orch = (fep_orchestrator_t*)orchestrator;
    nimcp_platform_mutex_lock(mutable_orch->mutex);

    int written = 0;

    if (orchestrator->kg_connected && kg_is_available(&orchestrator->kg_context)) {
        /* Get neighbors via KG */
        brain_kg_node_list_t* neighbors = kg_get_neighbors_safe(&orchestrator->kg_context);
        if (neighbors) {
            written = snprintf(summary, summary_size,
                "FEP Orchestrator Topology:\n"
                "  Bridges: %u/%u\n"
                "  KG Neighbors: %u\n"
                "  State: %s\n",
                orchestrator->stats.active_bridges,
                orchestrator->bridge_count,
                neighbors->count,
                orchestrator->state == FEP_ORCHESTRATOR_RUNNING ? "Running" : "Stopped");

            brain_kg_node_list_destroy(neighbors);
        }
    } else {
        written = snprintf(summary, summary_size,
            "FEP Orchestrator Topology:\n"
            "  Bridges: %u/%u\n"
            "  KG: Not connected\n"
            "  State: %s\n",
            orchestrator->stats.active_bridges,
            orchestrator->bridge_count,
            orchestrator->state == FEP_ORCHESTRATOR_RUNNING ? "Running" : "Stopped");
    }

    nimcp_platform_mutex_unlock(mutable_orch->mutex);
    return written;
}


/* ============================================================================
 * Statistics and Monitoring API Implementation
 * ============================================================================ */

int fep_orchestrator_get_stats(
    const fep_orchestrator_t* orchestrator,
    fep_orchestrator_stats_t* stats
) {
    /* Phase 8: Heartbeat at operation start */
    fep_orchestrator_heartbeat("fep_orchestr_get_stats", 0.0f);


    NIMCP_CHECK_THROW(orchestrator && stats, NIMCP_ERROR_NULL_POINTER, "orchestrator or stats is NULL");

    /* Thread-safe copy under lock for consistency */
    fep_orchestrator_t* mutable_orch = (fep_orchestrator_t*)orchestrator;
    nimcp_platform_mutex_lock(mutable_orch->mutex);
    *stats = orchestrator->stats;
    nimcp_platform_mutex_unlock(mutable_orch->mutex);

    return 0;
}


float fep_orchestrator_get_load(const fep_orchestrator_t* orchestrator) {
    if (!orchestrator) return 0.0f;
    
    /* Phase 8: Heartbeat at operation start */
    fep_orchestrator_heartbeat("fep_orchestr_get_load", 0.0f);


    if (orchestrator->config.update_time_budget_ms <= 0) {
        return 0.0f;  /* No budget = no load metric */
    }
    
    float avg_cycle_ms = orchestrator->stats.avg_cycle_time_us / 1000.0f;
    return avg_cycle_ms / orchestrator->config.update_time_budget_ms;
}


fep_orchestrator_state_t fep_orchestrator_get_state(
    const fep_orchestrator_t* orchestrator
) {
    if (!orchestrator) return FEP_ORCHESTRATOR_STOPPED;
    /* Phase 8: Heartbeat at operation start */
    fep_orchestrator_heartbeat("fep_orchestr_get_state", 0.0f);


    return orchestrator->state;
}


/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void fep_orchestrator_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_fep_orchestrator_health_agent = agent;
    }
}
