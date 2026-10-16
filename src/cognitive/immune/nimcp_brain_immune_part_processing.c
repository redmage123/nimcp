// nimcp_brain_immune_part_processing.c - processing functions
// Part of nimcp_brain_immune.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_brain_immune.c

/* Named constant for the immune module ID offset */
#define BIO_MODULE_IMMUNE_OFFSET 0x50

/**
 * @brief Execute antibody response
 */
int brain_immune_execute_antibody(brain_immune_system_t* system, uint32_t antibody_id) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_execute_antibody: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_execute_antibody", 0.0f);


    nimcp_mutex_lock(system->mutex);

    /* Find and validate antibody inside critical section */
    brain_antibody_t* antibody = find_antibody_by_id(system, antibody_id);
    if (!antibody || !antibody->active) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_execute_antibody: required parameter is NULL (antibody, antibody->active)");
        return -1;
    }

    /* Get associated antigen for BBB coordination */
    brain_antigen_t* antigen = find_antigen_by_id(system, antibody->target_antigen_id);

    /* Execute swarm response if connected */
    if (system->swarm_immune) {
        /* Would call nimcp_swarm_immune_generate_response */
        antibody->swarm_response_id = 1;  /* Placeholder */
    }

    /* Copy data needed after unlock */
    bbb_action_t bbb_action = antibody->bbb_action;
    brain_antibody_class_t ab_class = antibody->ab_class;
    NimcpSwarmResponseType swarm_response = antibody->swarm_response;
    uint32_t source_node_id = antigen ? antigen->source_node_id : 0;
    bool antigen_from_bbb = antigen && antigen->source == ANTIGEN_SOURCE_BBB;
    void* bbb_system = system->bbb_system;
    bool enable_logging = system->config.enable_logging;

    nimcp_mutex_unlock(system->mutex);

    /* Coordinate BBB action if antigen came from BBB (outside lock - BBB has own locking) */
    if (antigen_from_bbb && bbb_system) {
        /* BBB action is already stored in antibody->bbb_action */
        /* Execute coordinated BBB action based on antibody class */
        if (ab_class == ANTIBODY_IGG || ab_class == ANTIBODY_IGE) {
            /* For mature/emergency antibodies, ensure BBB takes strong action */
            if (bbb_action == BBB_ACTION_LOG || bbb_action == BBB_ACTION_BLOCK) {
                /* Escalate to quarantine for high-affinity antibody responses */
                if (source_node_id != 0) {
                    void* threat_addr = (void*)(uintptr_t)source_node_id;
                    bbb_quarantine_region(bbb_system, threat_addr, 1);
                }
            }
        }
    }

    if (enable_logging) {
        LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME,
            "Antibody %u executed response type %d with BBB action %d",
            antibody_id, swarm_response, bbb_action);
    }

    return 0;
}


/* ============================================================================
 * Update and Query API
 * ============================================================================ */

/**
 * @brief Update immune system state
 */
int brain_immune_update(brain_immune_system_t* system, uint64_t delta_ms) {
    if (!system || !system->running) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_update: required parameter is NULL (system, system->running)");
        /* Return -1 to match FEP/immune bridge convention (not error code enum) */
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_update", 0.0f);


    nimcp_mutex_lock(system->mutex);

    /* Process pending antigens */
    process_pending_antigens(system);

    /* Decay antibodies */
    decay_antibodies(system, delta_ms);

    /* Update inflammation sites */
    update_inflammation_sites(system, delta_ms);

    /* Update phase */
    update_immune_phase(system);

    /* Calculate health metric */
    float health = 1.0f;
    if (system->antigen_count > 0) {
        size_t neutralized = 0;
        for (size_t i = 0; i < system->antigen_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && system->antigen_count > 256) {
                brain_immune_heartbeat("brain_immune_loop",
                                 (float)(i + 1) / (float)system->antigen_count);
            }

            if (system->antigens[i].neutralized) neutralized++;
        }
        health = (float)neutralized / system->antigen_count;
    }
    system->stats.system_health = health;

    nimcp_mutex_unlock(system->mutex);

    return 0;
}


/* ============================================================================
 * BFT Integration Handlers
 * ============================================================================ */

/**
 * @brief Handle BFT accusation event
 *
 * WHAT: Auto-present Byzantine accusation as immune antigen
 * WHY:  Trigger immune response for Byzantine threats
 * HOW:  Create antigen from evidence, activate immune cells
 */
int brain_immune_handle_bft_accusation(
    brain_immune_system_t* system,
    uint32_t accuser_id,
    uint32_t accused_id,
    bft_behavior_t behavior,
    const bft_evidence_t* evidence,
    uint32_t evidence_count
) {
    if (!system || !evidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_handle_bft_accusation: required parameter is NULL (system, evidence)");
        return -1;
    }

    // Create antigen from BFT accusation
    uint32_t antigen_id = 0;
    int result = brain_immune_present_byzantine(
        system, accused_id, behavior, evidence, evidence_count, &antigen_id
    );

    if (result != 0) return result;

    if (system->config.enable_logging) {
        LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME,
            "BFT accusation: node %u accused %u of %s -> antigen %u",
            accuser_id, accused_id, bft_behavior_to_string(behavior), antigen_id);
    }

    return 0;
}


/**
 * @brief Handle BFT quarantine action
 *
 * WHAT: Coordinate killer T cell with BFT quarantine
 * WHY:  Unified immune-BFT threat isolation
 * HOW:  Activate killer T, track in inflammation system
 */
int brain_immune_handle_bft_quarantine(
    brain_immune_system_t* system,
    uint32_t node_id,
    uint64_t duration_ms,
    float trust_score
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_handle_bft_quarantine: system is NULL");
        return -1;
    }

    /* Find antigen for this node if exists */
    nimcp_mutex_lock(system->mutex);
    uint32_t antigen_id = 0;
    for (size_t i = 0; i < system->antigen_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->antigen_count > 256) {
            brain_immune_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)system->antigen_count);
        }

        if (system->antigens[i].source_node_id == node_id &&
            system->antigens[i].source == ANTIGEN_SOURCE_BFT) {
            antigen_id = system->antigens[i].id;
            break;
        }
    }
    nimcp_mutex_unlock(system->mutex);

    /* If we have an antigen for this node, activate killer T cell */
    if (antigen_id > 0) {
        uint32_t t_cell_id = 0;
        brain_immune_activate_killer_t(system, antigen_id, &t_cell_id);
        brain_immune_t_cell_kill(system, t_cell_id, node_id);

        /* Initiate inflammation for severe cases (low trust) */
        if (trust_score < 20.0f) {
            uint32_t site_id = 0;
            brain_immune_initiate_inflammation(system, node_id, antigen_id, &site_id);
        }
    }

    if (system->config.enable_logging) {
        LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME,
            "BFT quarantine: node %u (trust %.1f%%) -> killer T cell activated",
            node_id, trust_score);
    }

    return 0;
}


/**
 * @brief Handle BFT trust recovery
 *
 * WHAT: Form immune memory on trust restoration
 * WHY:  Map BFT trust recovery to immune memory
 * HOW:  Convert B cells to memory, release IL-10
 */
int brain_immune_handle_bft_trust_recovery(
    brain_immune_system_t* system,
    uint32_t node_id,
    float old_trust,
    float new_trust
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_handle_bft_trust_recovery: system is NULL");
        return -1;
    }

    /* Collect B cell IDs to transition under lock, then process outside lock.
     * This avoids unlock/relock race where b_cells array can change between
     * unlock and relock, invalidating the loop index. */
    uint32_t ids_to_transition[BRAIN_IMMUNE_MAX_B_CELLS];
    uint32_t transition_count = 0;

    nimcp_mutex_lock(system->mutex);
    for (size_t i = 0; i < system->b_cell_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->b_cell_count > 256) {
            brain_immune_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)system->b_cell_count);
        }

        if (system->b_cells[i].state == B_CELL_PLASMA ||
            system->b_cells[i].state == B_CELL_ACTIVATED) {
            /* Check if B cell is bound to antigen from this node */
            brain_antigen_t* antigen = find_antigen_by_id(system, system->b_cells[i].bound_antigen_id);
            if (antigen && antigen->source_node_id == node_id &&
                antigen->source == ANTIGEN_SOURCE_BFT) {
                if (transition_count < BRAIN_IMMUNE_MAX_B_CELLS) {
                    ids_to_transition[transition_count++] = system->b_cells[i].id;
                }
            }
        }
    }
    nimcp_mutex_unlock(system->mutex);

    /* Process transitions outside lock (function handles its own locking) */
    for (uint32_t i = 0; i < transition_count; i++) {
        brain_immune_b_cell_to_memory(system, ids_to_transition[i]);
    }

    /* Release anti-inflammatory cytokine IL-10 (recovery signal) */
    uint32_t cytokine_id = 0;
    brain_immune_release_cytokine(
        system,
        CYTOKINE_IL10,
        node_id,
        0.7f,  /* moderate concentration */
        0,     /* broadcast */
        &cytokine_id
    );

    /* G2 SECURITY: Present recovery pattern as benign antigen for immune memory.
     * WHAT: Record the recovery signature so the immune system remembers this pattern.
     * WHY:  Future encounters with the same recovery pattern should trigger a faster,
     *       attenuated response (secondary immune response) instead of full alarm.
     * HOW:  Present a low-severity antigen with recovery metadata so memory B cells
     *       can match it on subsequent BFT events from this node. */
    {
        uint8_t recovery_epitope[32];
        memset(recovery_epitope, 0, sizeof(recovery_epitope));
        /* Encode node_id and trust delta into recovery signature */
        memcpy(recovery_epitope, &node_id, sizeof(node_id));
        float trust_delta = new_trust - old_trust;
        memcpy(recovery_epitope + sizeof(node_id), &trust_delta, sizeof(trust_delta));
        /* Tag as recovery pattern (0xRE prefix) */
        recovery_epitope[8] = 0xAE;  /* Recovery marker byte 1 */
        recovery_epitope[9] = 0xC0;  /* Recovery marker byte 2 */

        uint32_t recovery_antigen_id = 0;
        brain_immune_present_antigen(
            system,
            ANTIGEN_SOURCE_BFT,
            recovery_epitope,
            sizeof(recovery_epitope),
            1,  /* Low severity — this is a recovery, not a threat */
            node_id,
            &recovery_antigen_id
        );
    }

    if (system->config.enable_logging) {
        LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME,
            "BFT trust recovery: node %u (%.1f%% -> %.1f%%) -> memory formation + IL-10 + recovery pattern recorded",
            node_id, old_trust, new_trust);
    }

    return 0;
}


/**
 * @brief Handler for imagination-related bio-async messages
 *
 * WHAT: Process incoming imagination engine messages
 * WHY:  Bidirectional communication between immune and imagination
 * HOW:  Dispatch based on message type
 *
 * @param msg Incoming message
 * @param msg_size Message size
 * @param response_promise Promise for response (may be NULL)
 * @param user_data Brain immune system pointer
 * @return NIMCP_SUCCESS on success, error code on failure
 */
static nimcp_error_t imagination_message_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    (void)response_promise;  /* Unused - no response needed */

    if (!msg || msg_size < sizeof(bio_message_header_t) || !user_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_message_handler: required parameter is NULL (msg, user_data)");
        return -1;
    }

    brain_immune_system_t* system = (brain_immune_system_t*)user_data;
    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    /* Handle imagination engine queries about immune state */
    switch (header->type) {
        case BIO_MSG_IMAGINATION_REQUEST:
            /* Imagination engine requesting current immune state */
            /* Send current modulation values */
            brain_immune_send_imagination_modulation(system);
            break;

        default:
            /* Unknown message type - ignore */
            break;
    }

    return NIMCP_SUCCESS;
}


/**
 * @brief KG-driven wiring callback for brain immune module
 */
static int brain_immune_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data)
{
    (void)user_data;

    int registered = 0;
    for (uint32_t i = 0; i < message_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && message_count > 256) {
            brain_immune_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)message_count);
        }

        switch (message_types[i]) {
            case BIO_MSG_IMAGINATION_REQUEST:
                bio_router_register_handler(ctx, message_types[i], imagination_message_handler);
                registered++;
                break;
            default:
                LOG_MODULE_DEBUG(BRAIN_IMMUNE_MODULE_NAME,
                    "brain_immune: unknown message type %d in wiring callback", message_types[i]);
                break;
        }
    }

    LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME,
        "Brain immune: registered %d handlers via KG wiring", registered);
    return 0;
}


/**
 * @brief Register imagination handler with bio-async router
 *
 * WHAT: Register handler for imagination-related messages
 * WHY:  Enable bidirectional communication with imagination engine
 * HOW:  Register handler for imagination message types
 *
 * NOTE: User data comes from bio_module_info_t.user_data set during
 *       bio_router_register_module() call in brain_immune_connect_bio_async()
 *
 * @param system Brain immune system
 * @return 0 on success, -1 on error
 */
int brain_immune_register_imagination_handler(brain_immune_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_register_imagination_handler: system is NULL");
        return -1;
    }
    if (!system->bio_context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_register_imagination_handler: system->bio_context is NULL");
        return -1;
    }

    /* Module ID for brain immune: use named constant for the offset */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_register_imagination", 0.0f);


    bio_module_id_t module_id = BIO_MODULE_INTROSPECTION + BIO_MODULE_IMMUNE_OFFSET;

    /* Try KG-driven wiring callback registration first */
    nimcp_error_t wiring_result = bio_router_register_wiring_callback(
        module_id,
        (void*)brain_immune_wiring_handler_callback,
        system
    );

    if (wiring_result == NIMCP_SUCCESS) {
        if (system->config.enable_logging) {
            LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME,
                "Brain immune: KG-driven wiring callback registered");
        }
        return 0;
    }

    /* Legacy fallback - register handlers directly */
    LEGACY_HANDLER_REGISTRATION(
        nimcp_error_t result = bio_router_register_handler(
            system->bio_context,
            BIO_MSG_IMAGINATION_REQUEST,
            imagination_message_handler
        )
    );

    if (result != NIMCP_SUCCESS) {
        if (system->config.enable_logging) {
            LOG_MODULE_WARN(BRAIN_IMMUNE_MODULE_NAME,
                "Failed to register imagination handler");
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_immune_register_imagination_handler: validation failed");
        return -1;
    }

    if (system->config.enable_logging) {
        LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME,
            "Registered imagination engine handler (legacy)");
    }

    return 0;
}


int brain_immune_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "brain_immune_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    /* Pass instance as context instead of NULL for future API use.
     * brain_immune_heartbeat_instance currently ignores instance param;
     * pass it for correctness when the API is updated to use it */
    brain_immune_heartbeat_instance(instance, "brain_immune_training_step", progress);
    return 0;
}
