// nimcp_brain_immune_part_helpers.c - helpers functions
// Part of nimcp_brain_immune.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_brain_immune.c


/* ============================================================================
 * Internal Helpers - Implementation
 * ============================================================================ */

/**
 * @brief Get current timestamp in milliseconds
 */
static uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    /* P1 fix: Cast to uint64_t before multiplication to prevent overflow on 32-bit time_t */
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}


/**
 * @brief Find antigen by ID
 */
static brain_antigen_t* find_antigen_by_id(brain_immune_system_t* system, uint32_t id) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;

    }
    for (size_t i = 0; i < system->antigen_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->antigen_count > 256) {
            brain_immune_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)system->antigen_count);
        }

        if (system->antigens[i].id == id) {
            return &system->antigens[i];
        }
    }
    return NULL;
}


/**
 * @brief Find B cell by ID
 */
static brain_b_cell_t* find_b_cell_by_id(brain_immune_system_t* system, uint32_t id) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;

    }
    for (size_t i = 0; i < system->b_cell_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->b_cell_count > 256) {
            brain_immune_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)system->b_cell_count);
        }

        if (system->b_cells[i].id == id) {
            return &system->b_cells[i];
        }
    }
    return NULL;
}


/**
 * @brief Find T cell by ID
 */
static brain_t_cell_t* find_t_cell_by_id(brain_immune_system_t* system, uint32_t id) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;

    }
    for (size_t i = 0; i < system->t_cell_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->t_cell_count > 256) {
            brain_immune_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)system->t_cell_count);
        }

        if (system->t_cells[i].id == id) {
            return &system->t_cells[i];
        }
    }
    return NULL;
}


/**
 * @brief Find antibody by ID
 */
static brain_antibody_t* find_antibody_by_id(brain_immune_system_t* system, uint32_t id) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;

    }
    for (size_t i = 0; i < system->antibody_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->antibody_count > 256) {
            brain_immune_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)system->antibody_count);
        }

        if (system->antibodies[i].id == id) {
            return &system->antibodies[i];
        }
    }
    return NULL;
}


/**
 * @brief Find inflammation site by ID
 */
static brain_inflammation_site_t* find_inflammation_by_id(brain_immune_system_t* system, uint32_t id) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;

    }
    for (size_t i = 0; i < system->inflammation_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->inflammation_count > 256) {
            brain_immune_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)system->inflammation_count);
        }

        if (system->inflammation_sites[i].id == id) {
            return &system->inflammation_sites[i];
        }
    }
    return NULL;  /* Not found - normal lookup behavior */
}


/**
 * @brief Update immune phase based on system state
 */
static void update_immune_phase(brain_immune_system_t* system) {
    if (!system) return;

    size_t active_antigens = 0;
    size_t neutralized = 0;

    for (size_t i = 0; i < system->antigen_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->antigen_count > 256) {
            brain_immune_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)system->antigen_count);
        }

        if (!system->antigens[i].neutralized) {
            active_antigens++;
        } else {
            neutralized++;
        }
    }

    /* Determine phase based on activity */
    if (active_antigens == 0 && system->inflammation_count == 0) {
        system->phase = IMMUNE_PHASE_SURVEILLANCE;
    } else if (active_antigens > 0 && system->antibody_count == 0) {
        system->phase = IMMUNE_PHASE_RECOGNITION;
    } else if (system->b_cell_count > 0 || system->t_cell_count > 0) {
        if (system->antibody_count > 0) {
            system->phase = IMMUNE_PHASE_EFFECTOR;
        } else {
            system->phase = IMMUNE_PHASE_ACTIVATION;
        }
    } else if (neutralized > 0 && active_antigens == 0) {
        system->phase = IMMUNE_PHASE_RESOLUTION;
    }
}


/**
 * @brief Process pending antigens and auto-activate responses
 */
static void process_pending_antigens(brain_immune_system_t* system) {
    if (!system) return;

    /* NOTE: This is called while mutex is already held by brain_immune_update()
     * So we do NOT call public API functions here - we do the work directly */

    for (size_t i = 0; i < system->antigen_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->antigen_count > 256) {
            brain_immune_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)system->antigen_count);
        }

        brain_antigen_t* antigen = &system->antigens[i];
        if (antigen->neutralized || antigen->processed) continue;

        /* Check if danger signal exceeds threshold */
        if (antigen->danger_signal >= system->config.activation_threshold) {
            /* Auto-activate B cell (inline, no mutex) */
            if (system->b_cell_count < system->b_cell_capacity) {
                brain_b_cell_t* b_cell = &system->b_cells[system->b_cell_count];
                memset(b_cell, 0, sizeof(*b_cell));

                b_cell->id = system->next_b_cell_id++;
                b_cell->state = B_CELL_ACTIVATED;
                b_cell->receptor_len = antigen->epitope_len;
                memcpy(b_cell->receptor, antigen->epitope, b_cell->receptor_len);
                b_cell->affinity = antigen->confidence;
                b_cell->bound_antigen_id = antigen->id;
                b_cell->activation_time = get_timestamp_ms();

                system->b_cell_count++;
                system->stats.active_b_cells++;

                /* If severe, also activate killer T */
                if (antigen->severity >= 7 && system->t_cell_count < system->t_cell_capacity) {
                    brain_t_cell_t* t_cell = &system->t_cells[system->t_cell_count];
                    memset(t_cell, 0, sizeof(*t_cell));

                    t_cell->id = system->next_t_cell_id++;
                    t_cell->type = T_CELL_KILLER;
                    t_cell->receptor_len = antigen->epitope_len;
                    memcpy(t_cell->receptor, antigen->epitope, t_cell->receptor_len);
                    t_cell->recognized_antigen_id = antigen->id;
                    t_cell->activation_level = 1.0f;
                    t_cell->activation_time = get_timestamp_ms();

                    system->t_cell_count++;
                    system->stats.active_t_cells++;
                }
            }
            antigen->processed = true;
        }
    }
}


/**
 * @brief Decay antibodies based on half-life
 *
 * Uses delta_ms (simulated time) rather than real timestamps
 * to allow predictable testing behavior.
 */
static void decay_antibodies(brain_immune_system_t* system, uint64_t delta_ms) {
    if (!system || delta_ms == 0) return;

    float half_life = (float)system->config.antibody_half_life_ms;
    if (half_life <= 0) return;

    /* Calculate decay factor based on delta time passed */
    float decay_factor = powf(0.5f, (float)delta_ms / half_life);

    for (size_t i = 0; i < system->antibody_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->antibody_count > 256) {
            brain_immune_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)system->antibody_count);
        }

        brain_antibody_t* ab = &system->antibodies[i];
        if (!ab->active) continue;

        ab->effectiveness *= decay_factor;

        /* Deactivate if too weak */
        if (ab->effectiveness < 0.1f) {
            ab->active = false;
            /* Guard against unsigned underflow before decrementing */
            if (system->stats.active_antibodies > 0) {
                system->stats.active_antibodies--;
            }
        }
    }
}


/**
 * @brief Update inflammation sites (progress resolution)
 *
 * WHAT: Progress inflammation resolution and notify imagination engine
 * WHY:  Inflammation affects imagination vividness/coherence (sickness behavior)
 * HOW:  Track level changes and send modulation when inflammation changes
 *
 * NOTE: Called while mutex is held (from brain_immune_update).
 *       send_imagination_modulation_unlocked() performs I/O (bio_router_send)
 *       while the caller's mutex is held. This is a known architectural issue
 *       — the bio_router_send is non-blocking but still performs I/O under lock.
 *       The fix below copies needed data and unlocks before sending.
 */
static void update_inflammation_sites(brain_immune_system_t* system, uint64_t delta_ms) {
    if (!system) return;

    bool level_changed = false;

    for (size_t i = 0; i < system->inflammation_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->inflammation_count > 256) {
            brain_immune_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)system->inflammation_count);
        }

        brain_inflammation_site_t* site = &system->inflammation_sites[i];

        /* Progress resolution if active — continuous decay */
        if (site->resolution_progress > 0 && site->resolution_progress < 1.0f) {
            brain_inflammation_level_t old_label = site->level;
            site->resolution_progress += 0.001f * delta_ms;
            if (site->resolution_progress >= 1.0f) {
                site->resolution_progress = 1.0f;
                site->inflammation_level = 0.0f;
            } else {
                /* Smoothly decay continuous level toward 0 during resolution */
                float decay = site->resolution_progress;
                site->inflammation_level *= (1.0f - decay * 0.01f);
                if (site->inflammation_level < 0.0f) site->inflammation_level = 0.0f;
            }
            /* Keep discrete label in sync */
            site->level = inflammation_level_from_continuous(site->inflammation_level);
            if (site->level != old_label) {
                level_changed = true;
            }
        }
    }

    /* Avoid I/O under lock: collect the data needed, unlock before sending,
     * re-lock after. bio_router_send() must not be called under system->mutex. */
    if (level_changed && system->bio_context) {
        /* Snapshot the data needed to build the modulation message */
        float inflammation = compute_inflammation_float_unlocked(system);
        void* bio_ctx = system->bio_context;

        /* Unlock before performing I/O */
        nimcp_mutex_unlock(system->mutex);

        /* Build and send modulation message without holding the lock */
        float vividness_modifier = expf(-2.0f * inflammation);
        float coherence_modifier = expf(-1.5f * inflammation);
        if (vividness_modifier < 0.1f) vividness_modifier = 0.1f;
        if (coherence_modifier < 0.2f) coherence_modifier = 0.2f;

        bio_msg_imagination_modulation_t msg;
        memset(&msg, 0, sizeof(msg));
        msg.header.type = BIO_MSG_IMAGINATION_VIVIDNESS_UPDATE;
        msg.header.source_module = BIO_MODULE_IMMUNE_BRAIN;
        msg.header.target_module = BIO_MODULE_IMAGINATION;
        msg.header.payload_size = sizeof(msg) - sizeof(bio_message_header_t);
        msg.header.channel = BIO_CHANNEL_SEROTONIN;
        msg.modulation_type = 0;
        msg.modifier = vividness_modifier;
        msg.source_level = inflammation;
        msg.secondary_level = coherence_modifier;

        bio_router_send(bio_ctx, &msg, sizeof(msg), 0);

        /* Re-lock for caller's continued use */
        nimcp_mutex_lock(system->mutex);
    }
}


/* ============================================================================
 * BFT Callback Handlers (static)
 * ============================================================================ */

/**
 * @brief BFT accusation callback handler
 *
 * WHAT: Callback invoked by BFT on accusations
 * WHY:  Enable automatic antigen presentation
 * HOW:  Forward to immune handler
 */
static void bft_accusation_cb(
    uint32_t accuser_id,
    uint32_t accused_id,
    bft_behavior_t behavior,
    const bft_evidence_t* evidence,
    uint32_t evidence_count,
    void* user_data
) {
    brain_immune_system_t* system = (brain_immune_system_t*)user_data;
    if (system) {
        brain_immune_handle_bft_accusation(
            system, accuser_id, accused_id, behavior, evidence, evidence_count
        );
    }
}


/**
 * @brief BFT quarantine callback handler
 */
static void bft_quarantine_cb(
    uint32_t node_id,
    uint64_t duration_ms,
    float trust_score,
    void* user_data
) {
    brain_immune_system_t* system = (brain_immune_system_t*)user_data;
    if (system) {
        brain_immune_handle_bft_quarantine(system, node_id, duration_ms, trust_score);
    }
}


/**
 * @brief BFT trust recovery callback handler
 */
static void bft_trust_recovery_cb(
    uint32_t node_id,
    float old_trust,
    float new_trust,
    void* user_data
) {
    brain_immune_system_t* system = (brain_immune_system_t*)user_data;
    if (system) {
        brain_immune_handle_bft_trust_recovery(system, node_id, old_trust, new_trust);
    }
}


/**
 * @brief Hierarchical recovery completion callback
 *
 * WHAT: Release IL-10 on successful recovery
 * WHY:  Anti-inflammatory response to recovery completion
 * HOW:  Triggered by HR success, releases IL-10 cytokine
 */
static void hr_completion_cb(
    const hr_recovery_request_t* request,
    const hr_recovery_response_t* response,
    void* user_data
) {
    brain_immune_system_t* system = (brain_immune_system_t*)user_data;
    if (!system) return;

    (void)request;   /* Unused */
    (void)response;  /* Unused */

    /* Release anti-inflammatory IL-10 cytokine */
    uint32_t cytokine_id = 0;
    brain_immune_release_cytokine(
        system,
        CYTOKINE_IL10,
        0,       /* source: recovery system */
        0.8f,    /* high concentration */
        0,       /* broadcast */
        &cytokine_id
    );

    if (system->config.enable_logging) {
        LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME,
            "Recovery completed -> IL-10 anti-inflammatory release (cytokine %u)", cytokine_id);
    }
}


/* ============================================================================
 * Imagination Engine Integration
 * ============================================================================ */

/**
 * @brief Compute inflammation level as normalized float
 *
 * WHAT: Convert inflammation level enum to [0.0-1.0] scale
 * WHY:  Needed for modulation calculations
 * HOW:  Map enum values to float range
 *
 * NOTE: Called while mutex is held - assumes system is valid
 */
static float compute_inflammation_float_unlocked(brain_immune_system_t* system) {
    float max_level = 0.0f;

    for (size_t i = 0; i < system->inflammation_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->inflammation_count > 256) {
            brain_immune_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)system->inflammation_count);
        }

        if (system->inflammation_sites[i].resolution_progress > 0.0f) {
            continue;  /* Skip resolving sites */
        }
        if (system->inflammation_sites[i].inflammation_level > max_level) {
            max_level = system->inflammation_sites[i].inflammation_level;
        }
    }

    return max_level;  /* Already in [0.0, 1.0] continuous range */
}


/**
 * @brief Send imagination modulation message (internal, mutex NOT held)
 *
 * WHAT: Compute and send vividness/coherence modifiers to imagination engine
 * WHY:  Sickness behavior includes reduced imaginative capacity
 * HOW:  Higher inflammation = lower vividness/coherence, via bio-async message
 *
 * BIOLOGICAL BASIS:
 * Inflammation triggers "sickness behavior" including cognitive changes:
 * - Reduced creativity and imagination vividness
 * - Lower working memory capacity
 * - Impaired cognitive flexibility
 * Pro-inflammatory cytokines (IL-1, IL-6, TNF-alpha) cross BBB and
 * affect hippocampus and prefrontal cortex, impairing imagination.
 *
 * NOTE: This function must NOT be called while system->mutex is held,
 * as bio_router_send performs I/O. Call after unlocking if modulation
 * is needed from within a locked section (see update_inflammation_sites).
 */
static void send_imagination_modulation_unlocked(brain_immune_system_t* system) {
    if (!system) return;
    if (!system->bio_context) return;  /* Bio-async not connected */

    /* Lock to safely read state for computing modulation */
    nimcp_mutex_lock(system->mutex);
    float inflammation = compute_inflammation_float_unlocked(system);
    void* bio_ctx = system->bio_context;
    nimcp_mutex_unlock(system->mutex);

    /* Compute modifiers: higher inflammation = lower vividness/coherence
     * Using exponential decay: modifier = e^(-k * inflammation)
     * k=2 gives: inflammation=0 -> modifier=1.0
     *            inflammation=0.5 -> modifier=0.37
     *            inflammation=1.0 -> modifier=0.14
     */
    float vividness_modifier = expf(-2.0f * inflammation);
    float coherence_modifier = expf(-1.5f * inflammation);  /* Coherence degrades slower */

    /* Clamp to reasonable bounds */
    if (vividness_modifier < 0.1f) vividness_modifier = 0.1f;
    if (coherence_modifier < 0.2f) coherence_modifier = 0.2f;

    /* Build modulation message */
    bio_msg_imagination_modulation_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.header.type = BIO_MSG_IMAGINATION_VIVIDNESS_UPDATE;
    msg.header.source_module = BIO_MODULE_IMMUNE_BRAIN;
    msg.header.target_module = BIO_MODULE_IMAGINATION;
    msg.header.payload_size = sizeof(msg) - sizeof(bio_message_header_t);
    msg.header.channel = BIO_CHANNEL_SEROTONIN;  /* Slow, modulatory signal */

    msg.modulation_type = 0;  /* 0 = vividness modulation */
    msg.modifier = vividness_modifier;
    msg.source_level = inflammation;
    msg.secondary_level = coherence_modifier;  /* Coherence in secondary field */

    /* Send via bio-async (non-blocking) without holding the lock */
    bio_router_send(bio_ctx, &msg, sizeof(msg), 0);
}
