// nimcp_brain_immune_part_accessors.c - accessors functions
// Part of nimcp_brain_immune.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_brain_immune.c


/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 */
int brain_immune_default_config(brain_immune_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_default_config: config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_default_config", 0.0f);


    memset(config, 0, sizeof(*config));

    /* Population limits */
    config->max_antigens = BRAIN_IMMUNE_MAX_ANTIGENS;
    config->max_b_cells = BRAIN_IMMUNE_MAX_B_CELLS;
    config->max_t_cells = BRAIN_IMMUNE_MAX_T_CELLS;
    config->max_antibodies = BRAIN_IMMUNE_MAX_ANTIBODIES;

    /* Timing */
    config->activation_delay_ms = 100;
    config->memory_formation_delay_ms = 5000;
    config->antibody_half_life_ms = 30000;

    /* Thresholds */
    config->recognition_threshold = 0.6f;
    config->activation_threshold = 0.5f;
    config->inflammation_threshold = 0.7f;
    config->cytokine_storm_threshold = 0.95f;

    /* Response tuning */
    config->memory_response_multiplier = 2.0f;
    config->helper_amplification = 1.5f;

    /* Integration enables - all on by default */
    config->enable_bbb_integration = true;
    config->enable_bft_integration = true;
    config->enable_swarm_integration = true;
    config->enable_bio_async = true;
    config->enable_logging = true;

    return 0;
}


/* ============================================================================
 * Callback Registration
 * ============================================================================ */

int brain_immune_set_antigen_callback(
    brain_immune_system_t* system,
    brain_immune_antigen_cb_t callback,
    void* user_data
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_set_antigen_callback: system is NULL");
        return -1;
    }

    /* THREAD SAFETY: Acquire mutex to prevent race with callback invocation */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_set_antigen_callback", 0.0f);


    nimcp_mutex_lock(system->mutex);
    system->on_antigen = callback;
    system->on_antigen_user_data = user_data;
    nimcp_mutex_unlock(system->mutex);

    return 0;
}


int brain_immune_set_neutralize_callback(
    brain_immune_system_t* system,
    brain_immune_neutralize_cb_t callback,
    void* user_data
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_set_neutralize_callback: system is NULL");
        return -1;
    }

    /* THREAD SAFETY: Acquire mutex to prevent race with callback invocation */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_set_neutralize_callb", 0.0f);


    nimcp_mutex_lock(system->mutex);
    system->on_neutralize = callback;
    system->on_neutralize_user_data = user_data;
    nimcp_mutex_unlock(system->mutex);

    return 0;
}


int brain_immune_set_cytokine_callback(
    brain_immune_system_t* system,
    brain_immune_cytokine_cb_t callback,
    void* user_data
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_set_cytokine_callback: system is NULL");
        return -1;
    }

    /* THREAD SAFETY: Acquire mutex to prevent race with callback invocation */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_set_cytokine_callbac", 0.0f);


    nimcp_mutex_lock(system->mutex);
    system->on_cytokine = callback;
    system->on_cytokine_user_data = user_data;
    nimcp_mutex_unlock(system->mutex);

    return 0;
}


int brain_immune_set_inflammation_callback(
    brain_immune_system_t* system,
    brain_immune_inflammation_cb_t callback,
    void* user_data
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_set_inflammation_callback: system is NULL");
        return -1;
    }

    /* THREAD SAFETY: Acquire mutex to prevent race with callback invocation */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_set_inflammation_cal", 0.0f);


    nimcp_mutex_lock(system->mutex);
    system->on_inflammation = callback;
    system->on_inflammation_user_data = user_data;
    nimcp_mutex_unlock(system->mutex);

    return 0;
}


int brain_immune_set_kill_callback(
    brain_immune_system_t* system,
    brain_immune_kill_cb_t callback,
    void* user_data
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_set_kill_callback: system is NULL");
        return -1;
    }

    /* THREAD SAFETY: Acquire mutex to prevent race with callback invocation */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_set_kill_callback", 0.0f);


    nimcp_mutex_lock(system->mutex);
    system->on_kill = callback;
    system->on_kill_user_data = user_data;
    nimcp_mutex_unlock(system->mutex);

    return 0;
}


/**
 * @brief Get statistics
 */
int brain_immune_get_stats(brain_immune_system_t* system, brain_immune_stats_t* stats) {
    if (!system || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_get_stats: required parameter is NULL (system, stats)");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_get_stats", 0.0f);


    nimcp_mutex_lock(system->mutex);
    *stats = system->stats;

    /* Compute cytokine levels from active cytokines */
    stats->cytokine_il1 = 0.0f;
    stats->cytokine_il6 = 0.0f;
    stats->cytokine_il10 = 0.0f;
    stats->cytokine_tnf = 0.0f;
    stats->cytokine_ifn_gamma = 0.0f;
    stats->cytokine_il4 = 0.0f;

    for (size_t i = 0; i < system->cytokine_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->cytokine_count > 256) {
            brain_immune_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)system->cytokine_count);
        }

        const brain_cytokine_t* cyt = &system->cytokines[i];
        if (!cyt->delivered) continue;  /* Only count active cytokines */

        switch (cyt->type) {
            case BRAIN_CYTOKINE_IL1:
                stats->cytokine_il1 += cyt->concentration;
                break;
            case BRAIN_CYTOKINE_IL6:
                stats->cytokine_il6 += cyt->concentration;
                break;
            case BRAIN_CYTOKINE_IL10:
                stats->cytokine_il10 += cyt->concentration;
                break;
            case BRAIN_CYTOKINE_TNF:
                stats->cytokine_tnf += cyt->concentration;
                break;
            case BRAIN_CYTOKINE_IFN_GAMMA:
                stats->cytokine_ifn_gamma += cyt->concentration;
                break;
            case BRAIN_CYTOKINE_IL4:
                stats->cytokine_il4 += cyt->concentration;
                break;
            default:
                break;
        }
    }

    /* Clamp cytokine levels to [0, 1] */
    if (stats->cytokine_il1 > 1.0f) stats->cytokine_il1 = 1.0f;
    if (stats->cytokine_il6 > 1.0f) stats->cytokine_il6 = 1.0f;
    if (stats->cytokine_il10 > 1.0f) stats->cytokine_il10 = 1.0f;
    if (stats->cytokine_tnf > 1.0f) stats->cytokine_tnf = 1.0f;
    if (stats->cytokine_ifn_gamma > 1.0f) stats->cytokine_ifn_gamma = 1.0f;
    if (stats->cytokine_il4 > 1.0f) stats->cytokine_il4 = 1.0f;

    /* Compute inflammation level from continuous site levels */
    float max_cont = 0.0f;
    for (size_t i = 0; i < system->inflammation_count; i++) {
        if (system->inflammation_sites[i].resolution_progress > 0.0f) continue;
        if (system->inflammation_sites[i].inflammation_level > max_cont) {
            max_cont = system->inflammation_sites[i].inflammation_level;
        }
    }
    /* Also factor in site count: multiple active sites increase overall level */
    size_t active_sites = 0;
    for (size_t i = 0; i < system->inflammation_count; i++) {
        if (system->inflammation_sites[i].resolution_progress <= 0.0f &&
            system->inflammation_sites[i].inflammation_level > 0.0f) {
            active_sites++;
        }
    }
    /* Boost: each additional active site adds 0.05 (capped at 1.0) */
    float site_boost = (active_sites > 1) ? (active_sites - 1) * 0.05f : 0.0f;
    float total_cont = max_cont + site_boost;
    if (total_cont > 1.0f) total_cont = 1.0f;

    stats->inflammation_level_continuous = total_cont;
    stats->inflammation_level = inflammation_level_from_continuous(total_cont);

    nimcp_mutex_unlock(system->mutex);

    return 0;
}


/**
 * @brief Get immune state snapshot for checkpointing
 *
 * WHAT: Extract immune state for fault tolerance checkpoints
 * WHY:  Include immune health in BFT/recovery checkpoints
 * HOW:  Copy key metrics to BFT-compatible structure
 */
int brain_immune_get_checkpoint_state(brain_immune_system_t* system, void* state) {
    if (!system || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_get_checkpoint_state: required parameter is NULL (system, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_get_checkpoint_state", 0.0f);


    /* NOTE: bft_immune_state_t is defined in nimcp_byzantine_fault_tolerance.h,
     * which is already included transitively via nimcp_brain_immune.h */
    bft_immune_state_t* immune_state = (bft_immune_state_t*)state;

    nimcp_mutex_lock(system->mutex);

    /* Count active antigens (unprocessed) */
    uint32_t active_antigens = 0;
    for (size_t i = 0; i < system->antigen_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->antigen_count > 256) {
            brain_immune_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)system->antigen_count);
        }

        if (!system->antigens[i].neutralized) active_antigens++;
    }

    /* Count memory cells */
    uint32_t memory_cells = 0;
    for (size_t i = 0; i < system->b_cell_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->b_cell_count > 256) {
            brain_immune_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)system->b_cell_count);
        }

        if (system->b_cells[i].state == B_CELL_MEMORY) memory_cells++;
    }

    /* Count active antibodies */
    uint32_t active_antibodies = 0;
    for (size_t i = 0; i < system->antibody_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->antibody_count > 256) {
            brain_immune_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)system->antibody_count);
        }

        if (system->antibodies[i].active) active_antibodies++;
    }

    /* Fill state */
    immune_state->active_antigens = active_antigens;
    immune_state->active_antibodies = active_antibodies;
    immune_state->memory_cells = memory_cells;
    immune_state->inflammation_sites = (uint32_t)system->inflammation_count;
    immune_state->system_health = system->stats.system_health;
    immune_state->immune_phase = (uint8_t)system->phase;

    nimcp_mutex_unlock(system->mutex);

    return 0;
}


/**
 * @brief Get current phase
 */
brain_immune_phase_t brain_immune_get_phase(brain_immune_system_t* system) {
    if (!system) return IMMUNE_PHASE_SURVEILLANCE;
    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_get_phase", 0.0f);


    return system->phase;
}


/**
 * @brief Check if antigen is neutralized
 *
 * WHAT: Thread-safe check of antigen neutralization state
 * WHY:  Antigen list is modified by multiple subsystems concurrently
 * HOW:  Lock immune mutex around antigen lookup and field read
 */
bool brain_immune_is_neutralized(brain_immune_system_t* system, uint32_t antigen_id) {
    if (!system) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_is_neutralized", 0.0f);

    nimcp_mutex_lock(system->mutex);
    brain_antigen_t* antigen = find_antigen_by_id(system, antigen_id);
    bool result = antigen && antigen->neutralized;
    nimcp_mutex_unlock(system->mutex);

    return result;
}


/**
 * @brief Get antigen by ID
 *
 * WHAT: Thread-safe antigen lookup by ID
 * WHY:  Antigen list is modified concurrently by emotional/cognitive subsystems;
 *       unprotected reads can race with present_antigen/neutralize modifications
 * HOW:  Lock immune mutex around antigen array traversal
 *
 * NOTE: Returned pointer is to internal data. Caller must not hold the pointer
 *       across operations that could modify the antigen list without external
 *       synchronization.
 */
const brain_antigen_t* brain_immune_get_antigen(brain_immune_system_t* system, uint32_t antigen_id) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;

    }
    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_get_antigen", 0.0f);

    nimcp_mutex_lock(system->mutex);
    const brain_antigen_t* result = find_antigen_by_id(system, antigen_id);
    nimcp_mutex_unlock(system->mutex);

    return result;
}

/**
 * Thread-safe antigen copy by ID.
 * Returns 0 on success, -1 if not found or error.
 * Caller gets a full copy — no dangling pointer risk.
 */
int brain_immune_get_antigen_copy(brain_immune_system_t* system, uint32_t antigen_id, brain_antigen_t* out) {
    if (!system || !out) return -1;

    brain_immune_heartbeat("brain_immune_get_antigen_copy", 0.0f);

    nimcp_mutex_lock(system->mutex);
    const brain_antigen_t* found = find_antigen_by_id(system, antigen_id);
    if (found) {
        *out = *found;  /* Struct copy under lock */
        nimcp_mutex_unlock(system->mutex);
        return 0;
    }
    nimcp_mutex_unlock(system->mutex);
    return -1;
}


/* ============================================================================
 * Cytokine and Inflammation Getters
 * ============================================================================ */

float brain_immune_get_cytokine_level(
    brain_immune_system_t* system,
    brain_cytokine_type_t type
) {
    if (!system) return 0.0f;
    if (!system->mutex) return 0.0f;

    /* Lock for thread safety */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_get_cytokine_level", 0.0f);


    /* Use thread-layer mutex API */
    nimcp_mutex_lock(system->mutex);

    float level = 0.0f;
    switch (type) {
        case BRAIN_CYTOKINE_IL1:
            level = system->stats.cytokine_il1;
            break;
        case BRAIN_CYTOKINE_IL6:
            level = system->stats.cytokine_il6;
            break;
        case BRAIN_CYTOKINE_IL10:
            level = system->stats.cytokine_il10;
            break;
        case BRAIN_CYTOKINE_TNF:
            level = system->stats.cytokine_tnf;
            break;
        case BRAIN_CYTOKINE_IFN_GAMMA:
            level = system->stats.cytokine_ifn_gamma;
            break;
        default:
            level = 0.0f;
            break;
    }

    nimcp_mutex_unlock(system->mutex);
    return level;
}


brain_inflammation_level_t brain_immune_get_inflammation_level(
    brain_immune_system_t* system
) {
    if (!system) return INFLAMMATION_NONE;
    if (!system->mutex) return INFLAMMATION_NONE;

    /* Derive discrete label from continuous level */
    float cont = brain_immune_get_inflammation_level_continuous(system);
    return inflammation_level_from_continuous(cont);
}


/**
 * @brief Get continuous inflammation level
 *
 * WHAT: Return max continuous inflammation across all active sites
 * WHY:  Smooth modulation for all bridge consumers
 * HOW:  Scan all non-resolving sites, return maximum
 */
float brain_immune_get_inflammation_level_continuous(
    brain_immune_system_t* system
) {
    if (!system) return 0.0f;
    if (!system->mutex) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_get_inflammation_con", 0.0f);

    /* Use thread-layer mutex API */
    nimcp_mutex_lock(system->mutex);

    float max_level = 0.0f;
    for (size_t i = 0; i < system->inflammation_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->inflammation_count > 256) {
            brain_immune_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)system->inflammation_count);
        }

        /* Skip sites that are resolving */
        if (system->inflammation_sites[i].resolution_progress > 0.0f) {
            continue;
        }
        if (system->inflammation_sites[i].inflammation_level > max_level) {
            max_level = system->inflammation_sites[i].inflammation_level;
        }
    }

    nimcp_mutex_unlock(system->mutex);

    return max_level;
}


/**
 * @brief Get full inflammation effects for current system state
 */
int brain_immune_get_inflammation_effects(
    brain_immune_system_t* system,
    inflammation_effects_t* effects
) {
    if (!system || !effects) return -1;

    float level = brain_immune_get_inflammation_level_continuous(system);
    return inflammation_compute_effects(level, effects);
}


/**
 * @brief Set exception presentation callback
 *
 * NOTE: The callback is stored globally (not per-instance) for simplicity.
 * The system parameter is validated as a NULL guard but not stored; global
 * storage is intentional design here.
 */
int brain_immune_set_exception_callback(
    brain_immune_system_t* system,
    brain_immune_exception_cb_t callback,
    void* user_data
) {
    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_set_exception_callba", 0.0f);

    /* system param is validated but unused here because the callback is stored globally.
     * The NULL check below serves as a guard. */
    if (!system) {
        /* system is unused but validate it to prevent accidental misuse */
        return -1;
    }
    g_exception_callback = callback;
    g_exception_callback_data = user_data;
    return 0;
}


/**
 * @brief Set recovery completion callback
 *
 * NOTE: The callback is stored globally (not per-instance) for simplicity.
 * The system parameter is validated as a NULL guard but not stored; global
 * storage is intentional design here.
 */
int brain_immune_set_recovery_callback(
    brain_immune_system_t* system,
    brain_immune_recovery_cb_t callback,
    void* user_data
) {
    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_set_recovery_callbac", 0.0f);

    /* system param is validated but unused here because the callback is stored globally.
     * The NULL check below serves as a guard. */
    if (!system) {
        /* system is unused but validate it to prevent accidental misuse */
        return -1;
    }
    g_recovery_callback = callback;
    g_recovery_callback_data = user_data;
    return 0;
}


/**
 * @brief Get recommended recovery action for antigen
 */
int brain_immune_get_recovery_recommendation(
    brain_immune_system_t* system,
    uint32_t antigen_id,
    int* action_out
) {
    if (!system || !action_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_get_recovery_recommendation: required parameter is NULL (system, action_out)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_get_recovery_recomme", 0.0f);


    nimcp_mutex_lock(system->mutex);

    brain_antigen_t* antigen = find_antigen_by_id(system, antigen_id);
    if (!antigen) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_get_recovery_recommendation: antigen is NULL");
        return -1;
    }

    /* Check if we have a memory cell that matches this antigen */
    for (size_t i = 0; i < system->b_cell_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->b_cell_count > 256) {
            brain_immune_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)system->b_cell_count);
        }

        brain_b_cell_t* b_cell = &system->b_cells[i];
        if (b_cell->state == B_CELL_MEMORY) {
            float affinity = brain_immune_compute_affinity(
                b_cell->receptor,
                b_cell->receptor_len,
                antigen->epitope,
                antigen->epitope_len
            );

            if (affinity >= system->config.recognition_threshold) {
                /* Found matching memory cell - recommend based on source */
                switch (antigen->source) {
                    case ANTIGEN_SOURCE_BBB:
                        *action_out = 8;  /* RECOVERY_ACTION_QUARANTINE */
                        break;
                    case ANTIGEN_SOURCE_BFT:
                        *action_out = 5;  /* RECOVERY_ACTION_RESTART_THREAD */
                        break;
                    case ANTIGEN_SOURCE_ANOMALY:
                        *action_out = 2;  /* RECOVERY_ACTION_GC */
                        break;
                    default:
                        *action_out = 1;  /* RECOVERY_ACTION_RETRY */
                        break;
                }

                nimcp_mutex_unlock(system->mutex);
                LOG_MODULE_DEBUG(BRAIN_IMMUNE_MODULE_NAME,
                    "Memory-based recovery recommendation: action=%d for antigen=%u",
                    *action_out, antigen_id);
                return 0;
            }
        }
    }

    /* Store severity before unlocking */
    uint32_t severity = antigen->severity;

    nimcp_mutex_unlock(system->mutex);

    /* No memory match - return default recommendation based on severity */
    if (severity >= 7) {
        *action_out = 4;  /* RECOVERY_ACTION_ROLLBACK */
    } else if (severity >= 5) {
        *action_out = 2;  /* RECOVERY_ACTION_GC */
    } else {
        *action_out = 1;  /* RECOVERY_ACTION_RETRY */
    }

    return 0;
}


/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void brain_immune_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    /* The health agent is stored globally (intentional design);
     * the instance parameter is intentionally unused. */
    (void)instance;  /* Global storage is intentional - instance is not used */
    g_brain_immune_health_agent = agent;
}
