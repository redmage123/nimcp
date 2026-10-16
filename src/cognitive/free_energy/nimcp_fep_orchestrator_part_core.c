// nimcp_fep_orchestrator_part_core.c - core functions
// Part of nimcp_fep_orchestrator.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_fep_orchestrator.c


int fep_orchestrator_start(fep_orchestrator_t* orchestrator) {
    /* Phase 8: Heartbeat at operation start */
    fep_orchestrator_heartbeat("fep_orchestr_start", 0.0f);


    NIMCP_CHECK_THROW(orchestrator, NIMCP_ERROR_NULL_POINTER, "orchestrator is NULL");
    
    nimcp_platform_mutex_lock(orchestrator->mutex);
    
    if (orchestrator->state == FEP_ORCHESTRATOR_RUNNING) {
        nimcp_platform_mutex_unlock(orchestrator->mutex);
        return 0;  /* Already running */
    }
    
    orchestrator->state = FEP_ORCHESTRATOR_STARTING;
    orchestrator->start_time = nimcp_platform_time_monotonic_ms();
    orchestrator->last_update_time = orchestrator->start_time;
    
    /* Initialize category last update times */
    for (int i = 0; i < FEP_BRIDGE_CATEGORY_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && FEP_BRIDGE_CATEGORY_COUNT > 256) {
            fep_orchestrator_heartbeat("fep_orchestr_loop",
                             (float)(i + 1) / (float)FEP_BRIDGE_CATEGORY_COUNT);
        }

        orchestrator->config.categories[i].last_update_time = orchestrator->start_time;
    }
    
    orchestrator->state = FEP_ORCHESTRATOR_RUNNING;
    
    if (orchestrator->config.enable_logging) {
        NIMCP_LOGGING_INFO("FEP orchestrator started: bridges=%u", orchestrator->bridge_count);
    }
    
    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return 0;
}


int fep_orchestrator_stop(fep_orchestrator_t* orchestrator) {
    /* Phase 8: Heartbeat at operation start */
    fep_orchestrator_heartbeat("fep_orchestr_stop", 0.0f);


    NIMCP_CHECK_THROW(orchestrator, NIMCP_ERROR_NULL_POINTER, "orchestrator is NULL");
    
    nimcp_platform_mutex_lock(orchestrator->mutex);
    
    if (orchestrator->state == FEP_ORCHESTRATOR_STOPPED) {
        nimcp_platform_mutex_unlock(orchestrator->mutex);
        return 0;  /* Already stopped */
    }
    
    orchestrator->state = FEP_ORCHESTRATOR_STOPPING;
    orchestrator->state = FEP_ORCHESTRATOR_STOPPED;
    
    if (orchestrator->config.enable_logging) {
        NIMCP_LOGGING_INFO("FEP orchestrator stopped");
    }
    
    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return 0;
}


int fep_orchestrator_pause(fep_orchestrator_t* orchestrator) {
    /* Phase 8: Heartbeat at operation start */
    fep_orchestrator_heartbeat("fep_orchestr_pause", 0.0f);


    NIMCP_CHECK_THROW(orchestrator, NIMCP_ERROR_NULL_POINTER, "orchestrator is NULL");
    
    nimcp_platform_mutex_lock(orchestrator->mutex);
    
    if (orchestrator->state != FEP_ORCHESTRATOR_RUNNING) {
        nimcp_platform_mutex_unlock(orchestrator->mutex);
        return NIMCP_ERROR_INVALID_STATE;
    }
    
    orchestrator->state = FEP_ORCHESTRATOR_PAUSED;
    
    if (orchestrator->config.enable_logging) {
        NIMCP_LOGGING_INFO("FEP orchestrator paused");
    }
    
    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return 0;
}


int fep_orchestrator_resume(fep_orchestrator_t* orchestrator) {
    /* Phase 8: Heartbeat at operation start */
    fep_orchestrator_heartbeat("fep_orchestr_resume", 0.0f);


    NIMCP_CHECK_THROW(orchestrator, NIMCP_ERROR_NULL_POINTER, "orchestrator is NULL");
    
    nimcp_platform_mutex_lock(orchestrator->mutex);
    
    if (orchestrator->state != FEP_ORCHESTRATOR_PAUSED) {
        nimcp_platform_mutex_unlock(orchestrator->mutex);
        return NIMCP_ERROR_INVALID_STATE;
    }
    
    /* Reset last update times to prevent immediate burst of updates */
    uint64_t now = nimcp_platform_time_monotonic_ms();
    for (int i = 0; i < FEP_BRIDGE_CATEGORY_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && FEP_BRIDGE_CATEGORY_COUNT > 256) {
            fep_orchestrator_heartbeat("fep_orchestr_loop",
                             (float)(i + 1) / (float)FEP_BRIDGE_CATEGORY_COUNT);
        }

        orchestrator->config.categories[i].last_update_time = now;
    }
    
    orchestrator->state = FEP_ORCHESTRATOR_RUNNING;
    
    if (orchestrator->config.enable_logging) {
        NIMCP_LOGGING_INFO("FEP orchestrator resumed");
    }
    
    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return 0;
}


/* ============================================================================
 * Bridge Registration API Implementation
 * ============================================================================ */

int fep_orchestrator_register_bridge(
    fep_orchestrator_t* orchestrator,
    const char* name,
    fep_bridge_category_t category,
    fep_bridge_handle_t handle,
    fep_bridge_update_fn_t update_fn,
    fep_bridge_destroy_fn_t destroy_fn,
    uint32_t* bridge_id_out
) {
    if (!orchestrator || !name || !handle || !update_fn) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    /* Phase 8: Heartbeat at operation start */
    fep_orchestrator_heartbeat("fep_orchestr_register_bridge", 0.0f);


    if (category >= FEP_BRIDGE_CATEGORY_COUNT) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Validate orchestrator is properly initialized */
    if (!orchestrator->mutex || !orchestrator->bridges) {
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    nimcp_platform_mutex_lock(orchestrator->mutex);

    if (orchestrator->bridge_count >= orchestrator->bridge_capacity) {
        nimcp_platform_mutex_unlock(orchestrator->mutex);
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Create entry */
    fep_bridge_entry_t* entry = &orchestrator->bridges[orchestrator->bridge_count];
    entry->bridge_id = orchestrator->next_bridge_id++;
    entry->bridge_name = name;
    entry->category = category;
    entry->handle = handle;
    entry->update_fn = update_fn;
    entry->destroy_fn = destroy_fn;
    entry->last_update_time = 0;
    entry->update_count = 0;
    entry->total_update_time_us = 0;
    entry->enabled = true;
    
    orchestrator->bridge_count++;
    orchestrator->stats.total_bridges++;
    orchestrator->stats.active_bridges++;
    orchestrator->stats.categories[category].bridge_count++;
    
    if (bridge_id_out) {
        *bridge_id_out = entry->bridge_id;
    }
    
    if (orchestrator->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Registered FEP bridge: %s (id=%u, category=%s)", 
            name, entry->bridge_id, CATEGORY_NAMES[category]);
    }
    
    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return 0;
}


int fep_orchestrator_unregister_bridge(
    fep_orchestrator_t* orchestrator,
    uint32_t bridge_id
) {
    /* Phase 8: Heartbeat at operation start */
    fep_orchestrator_heartbeat("fep_orchestr_unregister_bridge", 0.0f);


    NIMCP_CHECK_THROW(orchestrator, NIMCP_ERROR_NULL_POINTER, "orchestrator is NULL");

    /* Validate orchestrator is properly initialized */
    if (!orchestrator->mutex || !orchestrator->bridges) {
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    nimcp_platform_mutex_lock(orchestrator->mutex);
    
    /* Find bridge */
    int found_idx = -1;
    for (uint32_t i = 0; i < orchestrator->bridge_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && orchestrator->bridge_count > 256) {
            fep_orchestrator_heartbeat("fep_orchestr_loop",
                             (float)(i + 1) / (float)orchestrator->bridge_count);
        }

        if (orchestrator->bridges[i].bridge_id == bridge_id) {
            found_idx = (int)i;
            break;
        }
    }
    
    if (found_idx < 0) {
        nimcp_platform_mutex_unlock(orchestrator->mutex);
        return NIMCP_ERROR_INVALID_PARAM;
    }
    
    fep_bridge_entry_t* entry = &orchestrator->bridges[found_idx];
    fep_bridge_category_t category = entry->category;
    
    /* Update stats */
    orchestrator->stats.total_bridges--;
    if (entry->enabled) {
        orchestrator->stats.active_bridges--;
    }
    orchestrator->stats.categories[category].bridge_count--;
    
    /* Shift remaining entries */
    for (uint32_t i = (uint32_t)found_idx; i < orchestrator->bridge_count - 1; i++) {
        orchestrator->bridges[i] = orchestrator->bridges[i + 1];
    }
    orchestrator->bridge_count--;
    
    if (orchestrator->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Unregistered FEP bridge: id=%u", bridge_id);
    }
    
    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return 0;
}


/* ============================================================================
 * Integration API Implementation
 * ============================================================================ */

int fep_orchestrator_connect_brain_immune(
    fep_orchestrator_t* orchestrator,
    brain_immune_system_t* immune
) {
    /* Phase 8: Heartbeat at operation start */
    fep_orchestrator_heartbeat("fep_orchestr_connect_brain_immune", 0.0f);


    NIMCP_CHECK_THROW(orchestrator, NIMCP_ERROR_NULL_POINTER, "orchestrator is NULL");
    
    nimcp_platform_mutex_lock(orchestrator->mutex);
    
    orchestrator->brain_immune = immune;
    orchestrator->immune_connected = (immune != NULL);
    
    if (orchestrator->config.enable_logging && immune) {
        NIMCP_LOGGING_INFO("FEP orchestrator connected to brain immune system");
    }
    
    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return 0;
}


int fep_orchestrator_disconnect_brain_immune(fep_orchestrator_t* orchestrator) {
    /* Phase 8: Heartbeat at operation start */
    fep_orchestrator_heartbeat("fep_orchestr_disconnect_brain_imm", 0.0f);


    NIMCP_CHECK_THROW(orchestrator, NIMCP_ERROR_NULL_POINTER, "orchestrator is NULL");
    
    nimcp_platform_mutex_lock(orchestrator->mutex);
    
    orchestrator->brain_immune = NULL;
    orchestrator->immune_connected = false;
    
    if (orchestrator->config.enable_logging) {
        NIMCP_LOGGING_INFO("FEP orchestrator disconnected from brain immune system");
    }
    
    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return 0;
}


int fep_orchestrator_connect_bio_async(fep_orchestrator_t* orchestrator) {
    /* Phase 8: Heartbeat at operation start */
    fep_orchestrator_heartbeat("fep_orchestr_connect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(orchestrator, NIMCP_ERROR_NULL_POINTER, "orchestrator is NULL");
    
    if (!bio_router_is_initialized()) {
        if (orchestrator->config.enable_logging) {
            NIMCP_LOGGING_WARN("Bio-async router not initialized, skipping FEP orchestrator registration");
        }
        return 0;  /* Non-fatal */
    }
    
    nimcp_platform_mutex_lock(orchestrator->mutex);
    
    if (orchestrator->bio_async_connected) {
        nimcp_platform_mutex_unlock(orchestrator->mutex);
        return 0;  /* Already connected */
    }
    
    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_ORCHESTRATOR,
        .module_name = FEP_ORCHESTRATOR_MODULE_NAME,
        .inbox_capacity = 64,
        .user_data = orchestrator
    };
    
    orchestrator->bio_context = bio_router_register_module(&info);
    if (orchestrator->bio_context) {
        orchestrator->bio_async_connected = true;
        if (orchestrator->config.enable_logging) {
            NIMCP_LOGGING_INFO("FEP orchestrator connected to bio-async router");
        }
    }
    
    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return 0;
}


int fep_orchestrator_disconnect_bio_async(fep_orchestrator_t* orchestrator) {
    /* Phase 8: Heartbeat at operation start */
    fep_orchestrator_heartbeat("fep_orchestr_disconnect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(orchestrator, NIMCP_ERROR_NULL_POINTER, "orchestrator is NULL");

    nimcp_platform_mutex_lock(orchestrator->mutex);

    if (orchestrator->bio_async_connected && orchestrator->bio_context) {
        bio_router_unregister_module(orchestrator->bio_context);
        orchestrator->bio_context = NULL;
        orchestrator->bio_async_connected = false;

        if (orchestrator->config.enable_logging) {
            NIMCP_LOGGING_INFO("FEP orchestrator disconnected from bio-async router");
        }
    }

    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return 0;
}


/* ============================================================================
 * Internal Knowledge Graph Integration
 * ============================================================================ */

int fep_orchestrator_connect_internal_kg(
    fep_orchestrator_t* orchestrator,
    brain_t brain)
{
    /* Phase 8: Heartbeat at operation start */
    fep_orchestrator_heartbeat("fep_orchestr_connect_internal_kg", 0.0f);


    NIMCP_CHECK_THROW(orchestrator, NIMCP_ERROR_NULL_POINTER, "orchestrator is NULL");

    nimcp_platform_mutex_lock(orchestrator->mutex);

    /* Initialize KG context */
    if (kg_module_init(&orchestrator->kg_context, brain, "fep_orchestrator") != 0) {
        nimcp_platform_mutex_unlock(orchestrator->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "fep_orchestrator_connect_internal_kg: validation failed");
        return -1;
    }

    orchestrator->kg_connected = orchestrator->kg_context.kg_available;

    if (orchestrator->kg_connected) {
        /* Update node state to active */
        if (kg_has_node(&orchestrator->kg_context)) {
            uint64_t admin_token = brain ? brain->internal_kg_admin_token : 0;
            kg_module_update_state(&orchestrator->kg_context,
                                   BRAIN_KG_STATE_ACTIVE, admin_token);
            kg_module_set_ptr(&orchestrator->kg_context, orchestrator, admin_token);
        }

        if (orchestrator->config.enable_logging) {
            NIMCP_LOGGING_INFO("FEP orchestrator connected to internal KG");
        }
    }

    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return 0;
}


int fep_orchestrator_disconnect_internal_kg(fep_orchestrator_t* orchestrator) {
    /* Phase 8: Heartbeat at operation start */
    fep_orchestrator_heartbeat("fep_orchestr_disconnect_internal_", 0.0f);


    NIMCP_CHECK_THROW(orchestrator, NIMCP_ERROR_NULL_POINTER, "orchestrator is NULL");

    nimcp_platform_mutex_lock(orchestrator->mutex);

    if (orchestrator->kg_connected) {
        orchestrator->kg_context.kg = NULL;
        orchestrator->kg_context.kg_available = false;
        orchestrator->kg_connected = false;

        if (orchestrator->config.enable_logging) {
            NIMCP_LOGGING_INFO("FEP orchestrator disconnected from internal KG");
        }
    }

    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return 0;
}


/* ============================================================================
 * KG Self-Awareness Integration
 * ============================================================================ */

int fep_orchestrator_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    fep_orchestrator_heartbeat("fep_orchestr_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "FEP_Orchestrator");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                fep_orchestrator_heartbeat("fep_orchestr_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("FEP Orchestrator self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "FEP_Orchestrator");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "FEP_Orchestrator");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}


/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int fep_orchestrator_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fep_orchestrator_training_begin: NULL argument");
        return -1;
    }
    fep_orchestrator_heartbeat_instance(g_fep_orchestrator_health_agent, "fep_orchestrator_training_begin", 0.0f);
    return 0;
}


int fep_orchestrator_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fep_orchestrator_training_end: NULL argument");
        return -1;
    }
    fep_orchestrator_heartbeat_instance(g_fep_orchestrator_health_agent, "fep_orchestrator_training_end", 1.0f);
    return 0;
}
