// nimcp_brain_immune_part_lifecycle.c - lifecycle functions
// Part of nimcp_brain_immune.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_brain_immune.c


/**
 * @brief Create brain immune system
 */
brain_immune_system_t* brain_immune_create(const brain_immune_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_create", 0.0f);


    brain_immune_system_t* system = nimcp_calloc(1, sizeof(brain_immune_system_t));
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate system");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        system->config = *config;
    } else {
        brain_immune_default_config(&system->config);
    }

    /* Allocate antigen pool */
    system->antigen_capacity = system->config.max_antigens;
    system->antigens = nimcp_calloc(system->antigen_capacity, sizeof(brain_antigen_t));
    if (!system->antigens) goto cleanup;

    /* Allocate B cells */
    system->b_cell_capacity = system->config.max_b_cells;
    system->b_cells = nimcp_calloc(system->b_cell_capacity, sizeof(brain_b_cell_t));
    if (!system->b_cells) goto cleanup;

    /* Allocate T cells */
    system->t_cell_capacity = system->config.max_t_cells;
    system->t_cells = nimcp_calloc(system->t_cell_capacity, sizeof(brain_t_cell_t));
    if (!system->t_cells) goto cleanup;

    /* Allocate antibodies */
    system->antibody_capacity = system->config.max_antibodies;
    system->antibodies = nimcp_calloc(system->antibody_capacity, sizeof(brain_antibody_t));
    if (!system->antibodies) goto cleanup;

    /* Allocate cytokines */
    system->cytokine_capacity = BRAIN_IMMUNE_MAX_CYTOKINES;
    system->cytokines = nimcp_calloc(system->cytokine_capacity, sizeof(brain_cytokine_t));
    if (!system->cytokines) goto cleanup;

    /* Allocate inflammation sites */
    system->inflammation_capacity = BRAIN_IMMUNE_MAX_INFLAMMATION;
    system->inflammation_sites = nimcp_calloc(system->inflammation_capacity, sizeof(brain_inflammation_site_t));
    if (!system->inflammation_sites) goto cleanup;

    /* Initialize mutex */
    system->mutex = nimcp_mutex_create();
    if (!system->mutex) goto cleanup;

    /* Set initial state */
    system->phase = IMMUNE_PHASE_SURVEILLANCE;
    system->next_antigen_id = 1;
    system->next_b_cell_id = 1;
    system->next_t_cell_id = 1;
    system->next_antibody_id = 1;
    system->next_cytokine_id = 1;
    system->next_inflammation_id = 1;
    system->start_time = get_timestamp_ms();

    /* Phase C: Create regulatory T cell system for cytokine storm damping.
     * NULL-tolerant: if treg_create fails we fall back to no-suppression
     * (the LOG_WARN below telegraphs the degraded mode). The brain immune
     * system itself stays usable. */
    system->tregs = treg_create(NULL, system);
    system->treg_storm_engaged = false;
    if (!system->tregs) {
        LOG_MODULE_WARN(BRAIN_IMMUNE_MODULE_NAME,
            "treg_create failed — cytokine storm damping disabled");
    }

    if (system->config.enable_logging) {
        LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME, "Brain immune system created");
    }

    return system;

cleanup:
    brain_immune_destroy(system);
    /* Cleanup reached on allocation failure */
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_immune_create: allocation failed");
    return NULL;
}


/**
 * @brief Destroy brain immune system
 */
void brain_immune_destroy(brain_immune_system_t* system) {
    if (!system) return;

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_destroy", 0.0f);


    brain_immune_stop(system);

    /* Phase C: Destroy Treg system before pools so any pending callbacks
     * that touch the immune system stop firing. */
    if (system->tregs) {
        treg_destroy((treg_system_t*)system->tregs);
        system->tregs = NULL;
    }

    if (system->mutex) {
        /* Destroy mutex (handles destroy+free+NULL) */
        nimcp_mutex_destroy(system->mutex);
    }

    nimcp_free(system->antigens);
    nimcp_free(system->b_cells);
    nimcp_free(system->t_cells);
    nimcp_free(system->antibodies);
    nimcp_free(system->cytokines);
    nimcp_free(system->inflammation_sites);
    nimcp_free(system);
    /* Note: no need to NULL the local pointer after free */
}


/* ============================================================================
 * Inflammation API
 * ============================================================================ */

/**
 * @brief Initiate inflammation
 */
int brain_immune_initiate_inflammation(
    brain_immune_system_t* system,
    uint32_t region_id,
    uint32_t antigen_id,
    uint32_t* site_id
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_initiate_inflammation: system is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_initiate_inflammatio", 0.0f);

    nimcp_mutex_lock(system->mutex);

    /* Capacity check inside mutex to prevent TOCTOU race */
    if (system->inflammation_count >= system->inflammation_capacity) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "brain_immune_initiate_inflammation: capacity exceeded");
        return -1;
    }

    brain_inflammation_site_t* site = &system->inflammation_sites[system->inflammation_count];
    memset(site, 0, sizeof(*site));

    site->id = system->next_inflammation_id++;
    site->region_id = region_id;
    site->triggering_antigen_id = antigen_id;
    site->inflammation_level = 0.175f;  /* LOCAL midpoint on continuous scale */
    site->level = inflammation_level_from_continuous(site->inflammation_level);
    site->start_time = get_timestamp_ms();
    site->resource_allocation = 0.2f;
    site->resolution_progress = 0.0f;

    if (site_id) *site_id = site->id;
    system->inflammation_count++;
    system->stats.inflammation_sites++;

    /* Capture callback and data under mutex to prevent race condition */
    brain_immune_inflammation_cb_t inflammation_callback = system->on_inflammation;
    void* callback_user_data = system->on_inflammation_user_data;
    brain_inflammation_site_t site_copy = *site;  /* Copy for safe callback invocation */
    bool enable_logging = system->config.enable_logging;

    nimcp_mutex_unlock(system->mutex);

    /* Trigger callback with copied data (safe after unlock) */
    if (inflammation_callback) {
        inflammation_callback(system, &site_copy, callback_user_data);
    }

    if (enable_logging) {
        LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME,
            "Inflammation initiated at region %u", region_id);
    }

    return 0;
}
