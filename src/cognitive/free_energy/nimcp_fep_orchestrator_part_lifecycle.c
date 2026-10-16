// nimcp_fep_orchestrator_part_lifecycle.c - lifecycle functions
// Part of nimcp_fep_orchestrator.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_fep_orchestrator.c


fep_orchestrator_t* fep_orchestrator_create(const fep_orchestrator_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    fep_orchestrator_heartbeat("fep_orchestr_create", 0.0f);


    fep_orchestrator_t* orchestrator = (fep_orchestrator_t*)nimcp_calloc(1, sizeof(fep_orchestrator_t));
    if (!orchestrator) return NULL;
    NIMCP_API_CHECK_ALLOC(orchestrator, "Failed to allocate FEP orchestrator");
    
    /* Apply configuration */
    if (config) {
        orchestrator->config = *config;
    } else {
        fep_orchestrator_default_config(&orchestrator->config);
    }
    
    /* Allocate bridge registry */
    orchestrator->bridge_capacity = orchestrator->config.max_bridges;
    orchestrator->bridges = (fep_bridge_entry_t*)nimcp_calloc(
        orchestrator->bridge_capacity, sizeof(fep_bridge_entry_t));
    if (!orchestrator->bridges) {
        nimcp_free(orchestrator);
        orchestrator = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fep_orchestrator_create: orchestrator->bridges is NULL");
        return NULL;
    }
    
    /* Initialize mutex */
    orchestrator->mutex = nimcp_platform_mutex_create();
    if (!orchestrator->mutex) {
        nimcp_free(orchestrator->bridges);
        nimcp_free(orchestrator);
        orchestrator = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fep_orchestrator_create: orchestrator->mutex is NULL");
        return NULL;
    }
    
    /* Initialize state */
    orchestrator->state = FEP_ORCHESTRATOR_STOPPED;
    orchestrator->bridge_count = 0;
    orchestrator->next_bridge_id = 1;
    orchestrator->bio_async_connected = false;
    orchestrator->immune_connected = false;

    /* Initialize statistics */
    memset(&orchestrator->stats, 0, sizeof(fep_orchestrator_stats_t));

    /* Initialize continuous scheduling metrics to zero (calm state) */
    memset(&orchestrator->fep_metrics, 0, sizeof(fep_scheduling_metrics_t));
    
    if (orchestrator->config.enable_logging) {
        NIMCP_LOGGING_INFO("FEP orchestrator created: max_bridges=%u", 
            orchestrator->bridge_capacity);
    }
    
    return orchestrator;
}


void fep_orchestrator_destroy(fep_orchestrator_t* orchestrator) {
    if (!orchestrator) return;
    
    /* Stop if running */
    /* Phase 8: Heartbeat at operation start */
    fep_orchestrator_heartbeat("fep_orchestr_destroy", 0.0f);


    if (orchestrator->state == FEP_ORCHESTRATOR_RUNNING) {
        fep_orchestrator_stop(orchestrator);
    }
    
    /* Disconnect integrations */
    if (orchestrator->bio_async_connected) {
        fep_orchestrator_disconnect_bio_async(orchestrator);
    }
    if (orchestrator->immune_connected) {
        fep_orchestrator_disconnect_brain_immune(orchestrator);
    }
    
    /* Destroy bridges if destroy_fn provided */
    for (uint32_t i = 0; i < orchestrator->bridge_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && orchestrator->bridge_count > 256) {
            fep_orchestrator_heartbeat("fep_orchestr_loop",
                             (float)(i + 1) / (float)orchestrator->bridge_count);
        }

        fep_bridge_entry_t* entry = &orchestrator->bridges[i];
        if (entry->destroy_fn && entry->handle) {
            entry->destroy_fn(entry->handle);
            entry->handle = NULL;
        }
    }
    
    /* Free resources */
    if (orchestrator->mutex) {
        nimcp_platform_mutex_destroy(orchestrator->mutex);
        nimcp_free(orchestrator->mutex);
        orchestrator->mutex = NULL;
    }
    if (orchestrator->bridges) {
        nimcp_free(orchestrator->bridges);
    }
    
    if (orchestrator->config.enable_logging) {
        NIMCP_LOGGING_INFO("FEP orchestrator destroyed");
    }
    
    nimcp_free(orchestrator);
    orchestrator = NULL;
}


void fep_orchestrator_reset_stats(fep_orchestrator_t* orchestrator) {
    if (!orchestrator) return;
    
    /* Phase 8: Heartbeat at operation start */
    fep_orchestrator_heartbeat("fep_orchestr_reset_stats", 0.0f);


    nimcp_platform_mutex_lock(orchestrator->mutex);
    
    /* Preserve bridge counts */
    uint32_t total = orchestrator->stats.total_bridges;
    uint32_t active = orchestrator->stats.active_bridges;
    uint32_t cat_counts[FEP_BRIDGE_CATEGORY_COUNT];
    for (int i = 0; i < FEP_BRIDGE_CATEGORY_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && FEP_BRIDGE_CATEGORY_COUNT > 256) {
            fep_orchestrator_heartbeat("fep_orchestr_loop",
                             (float)(i + 1) / (float)FEP_BRIDGE_CATEGORY_COUNT);
        }

        cat_counts[i] = orchestrator->stats.categories[i].bridge_count;
    }
    
    memset(&orchestrator->stats, 0, sizeof(fep_orchestrator_stats_t));
    
    orchestrator->stats.total_bridges = total;
    orchestrator->stats.active_bridges = active;
    for (int i = 0; i < FEP_BRIDGE_CATEGORY_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && FEP_BRIDGE_CATEGORY_COUNT > 256) {
            fep_orchestrator_heartbeat("fep_orchestr_loop",
                             (float)(i + 1) / (float)FEP_BRIDGE_CATEGORY_COUNT);
        }

        orchestrator->stats.categories[i].bridge_count = cat_counts[i];
    }
    
    nimcp_platform_mutex_unlock(orchestrator->mutex);
}
