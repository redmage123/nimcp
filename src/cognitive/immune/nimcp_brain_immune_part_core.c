// nimcp_brain_immune_part_core.c - core functions
// Part of nimcp_brain_immune.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_brain_immune.c


/**
 * @brief Start immune system
 */
int brain_immune_start(brain_immune_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_start: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_start", 0.0f);


    nimcp_mutex_lock(system->mutex);
    system->running = true;
    system->start_time = get_timestamp_ms();
    system->phase = IMMUNE_PHASE_SURVEILLANCE;
    void* kg_brain = system->brain_ref;
    nimcp_mutex_unlock(system->mutex);

    if (system->config.enable_logging) {
        LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME, "Brain immune system started");
    }

    /* W11: emit KG immune lifecycle event. */
    if (kg_brain) {
        w11_emit_immune_lifecycle((struct brain_struct*)kg_brain, "start");
    }

    return 0;
}

/**
 * @brief W11: set brain back-reference for KG emission.
 */
void brain_immune_set_brain_ref(brain_immune_system_t* system, void* brain) {
    if (!system) return;
    nimcp_mutex_lock(system->mutex);
    system->brain_ref = brain;
    nimcp_mutex_unlock(system->mutex);
}


/**
 * @brief Stop immune system
 */
int brain_immune_stop(brain_immune_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_stop: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_stop", 0.0f);


    nimcp_mutex_lock(system->mutex);
    system->running = false;
    void* kg_brain = system->brain_ref;
    nimcp_mutex_unlock(system->mutex);

    if (system->config.enable_logging) {
        LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME, "Brain immune system stopped");
    }

    /* W11: emit KG immune lifecycle event. */
    if (kg_brain) {
        w11_emit_immune_lifecycle((struct brain_struct*)kg_brain, "stop");
    }

    return 0;
}


/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Connect to BBB security
 */
int brain_immune_connect_bbb(brain_immune_system_t* system, bbb_system_t bbb_system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_connect_bbb: system is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!bbb_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_connect_bbb: bbb_system is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_connect_bbb", 0.0f);


    nimcp_mutex_lock(system->mutex);
    system->bbb_system = bbb_system;
    nimcp_mutex_unlock(system->mutex);

    if (system->config.enable_logging) {
        LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME, "Connected to BBB security");
    }

    return 0;
}


/**
 * @brief Connect to BFT with enhanced callbacks
 */
int brain_immune_connect_bft(brain_immune_system_t* system, bft_context_t* bft_context) {
    if (!system || !bft_context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_connect_bft: required parameter is NULL (system, bft_context)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_connect_bft", 0.0f);


    nimcp_mutex_lock(system->mutex);
    system->bft_context = bft_context;
    nimcp_mutex_unlock(system->mutex);

    /* Register callbacks for automatic integration, check return values */
    bool accusation_ok = bft_register_accusation_callback(bft_context, bft_accusation_cb, system);
    if (!accusation_ok) {
        LOG_MODULE_WARN(BRAIN_IMMUNE_MODULE_NAME, "Failed to register BFT accusation callback");
    }

    bool quarantine_ok = bft_register_quarantine_callback(bft_context, bft_quarantine_cb, system);
    if (!quarantine_ok) {
        LOG_MODULE_WARN(BRAIN_IMMUNE_MODULE_NAME, "Failed to register BFT quarantine callback");
    }

    bool trust_ok = bft_register_trust_recovery_callback(bft_context, bft_trust_recovery_cb, system);
    if (!trust_ok) {
        LOG_MODULE_WARN(BRAIN_IMMUNE_MODULE_NAME, "Failed to register BFT trust recovery callback");
    }

    if (system->config.enable_logging) {
        LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME, "Connected to BFT with callbacks (accusation=%d, quarantine=%d, trust=%d)",
            accusation_ok, quarantine_ok, trust_ok);
    }

    return 0;
}


/**
 * @brief Connect to swarm immune with bidirectional sync
 *
 * WHAT: Connect brain immune to swarm immune with automatic threat/response sync
 * WHY:  Enable distributed immune response across swarm nodes
 * HOW:  Link systems and enable auto-sync of threats, memory cells, and responses
 */
int brain_immune_connect_swarm(brain_immune_system_t* system, NimcpSwarmImmuneSystem* swarm_immune) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_connect_swarm: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_connect_swarm", 0.0f);


    nimcp_mutex_lock(system->mutex);
    system->swarm_immune = swarm_immune;
    nimcp_mutex_unlock(system->mutex);

    if (system->config.enable_logging) {
        LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME, "Connected to swarm immune with bidirectional sync");
    }

    return 0;
}


/**
 * @brief Connect to hierarchical recovery
 */
int brain_immune_connect_hierarchical_recovery(brain_immune_system_t* system, void* hr_context) {
    if (!system || !hr_context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_connect_hierarchical_recovery: required parameter is NULL (system, hr_context)");
        return -1;
    }

    /* Register completion callback for IL-10 release */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_connect_hierarchical", 0.0f);


    hr_register_completion_callback((hr_context_t*)hr_context, hr_completion_cb, system);

    if (system->config.enable_logging) {
        LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME, "Connected to hierarchical recovery with IL-10 callback");
    }

    return 0;
}


/**
 * @brief Connect to bio-async router
 */
int brain_immune_connect_bio_async(brain_immune_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_connect_bio_async: system is NULL");
        return -1;
    }
    if (!bio_router_is_initialized()) {
        if (system->config.enable_logging) {
            LOG_MODULE_WARN(BRAIN_IMMUNE_MODULE_NAME, "Bio-async router not available, skipping registration");
        }
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_connect_bio_async", 0.0f);


    bio_module_info_t info = {
        .module_id = BIO_MODULE_INTROSPECTION + 0x50,  /* Offset for immune */
        .module_name = BRAIN_IMMUNE_MODULE_NAME,
        .inbox_capacity = 64,
        .user_data = system
    };

    nimcp_mutex_lock(system->mutex);
    system->bio_context = bio_router_register_module(&info);
    nimcp_mutex_unlock(system->mutex);

    if (system->config.enable_logging) {
        LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME, "Connected to bio-async router");
    }

    return 0;
}


/* ============================================================================
 * Enhanced Swarm Integration API
 * ============================================================================ */

/**
 * @brief Automatically sync swarm threat to brain immune antigen
 *
 * WHAT: Auto-present swarm-detected threats as brain immune antigens
 * WHY:  Ensure all swarm threats are processed by brain immune system
 * HOW:  Called automatically when swarm detects threat
 */
int brain_immune_auto_sync_swarm_threat(
    brain_immune_system_t* system,
    const NimcpSwarmThreat* threat
) {
    if (!system || !threat || !system->swarm_immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_auto_sync_swarm_threat: required parameter is NULL (system, threat, system->swarm_immune)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_auto_sync_swarm_thre", 0.0f);


    uint32_t antigen_id = 0;
    int result = brain_immune_present_swarm_threat(system, threat, &antigen_id);

    if (result == 0 && system->config.enable_logging) {
        LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME,
            "Auto-synced swarm threat %u -> brain antigen %u",
            threat->id, antigen_id);
    }

    return result;
}


/**
 * @brief Sync brain immune memory cell to swarm immune memory
 *
 * WHAT: Create swarm immune memory cell from brain immune B cell memory
 * WHY:  Share learned threat patterns across swarm
 * HOW:  Convert B cell receptor pattern to swarm threat signature
 */
int brain_immune_sync_memory_to_swarm(
    brain_immune_system_t* system,
    uint32_t b_cell_id
) {
    if (!system || !system->swarm_immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_sync_memory_to_swarm: required parameter is NULL (system, system->swarm_immune)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_sync_memory_to_swarm", 0.0f);

    /* Thread-safety fix: hold mutex while searching and using B cell / antigen data */
    nimcp_mutex_lock(system->mutex);

    brain_b_cell_t* b_cell = find_b_cell_by_id(system, b_cell_id);
    if (!b_cell || b_cell->state != B_CELL_MEMORY) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_sync_memory_to_swarm: b_cell is NULL");
        return -1;
    }

    /* Check if already synced (prevent duplicate memory cells) */
    if (b_cell->swarm_memory_cell_id != 0) {
        nimcp_mutex_unlock(system->mutex);
        return 0;  /* Already synced */
    }

    /* Find corresponding antigen to get response type */
    brain_antigen_t* antigen = find_antigen_by_id(system, b_cell->bound_antigen_id);
    if (!antigen) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_sync_memory_to_swarm: antigen is NULL");
        return -1;
    }

    /* Create swarm threat signature from B cell receptor */
    NimcpSwarmThreatSignature signature;
    memset(&signature, 0, sizeof(signature));

    signature.pattern_len = b_cell->receptor_len;
    memcpy(signature.pattern, b_cell->receptor,
           signature.pattern_len > 64 ? 64 : signature.pattern_len);
    signature.match_threshold = system->config.recognition_threshold;
    signature.type = THREAT_BYZANTINE;  /* Default threat type */
    signature.detection_count = 1;
    signature.last_seen = get_timestamp_ms();

    /* Determine response type based on antigen severity */
    NimcpSwarmResponseType response = RESPONSE_ISOLATION;
    if (antigen->severity >= 8) {
        response = RESPONSE_COUNTER_ATTACK;
    } else if (antigen->severity >= 5) {
        response = RESPONSE_ISOLATION;
    } else {
        response = RESPONSE_ALERT;
    }

    /* Add memory cell to swarm immune */
    uint32_t swarm_cell_id = 0;
    nimcp_result_t res = nimcp_swarm_immune_add_memory_cell(
        system->swarm_immune,
        &signature,
        response,
        b_cell->affinity,
        &swarm_cell_id
    );

    if (res == NIMCP_SUCCESS) {
        b_cell->swarm_memory_cell_id = swarm_cell_id;
        nimcp_mutex_unlock(system->mutex);

        if (system->config.enable_logging) {
            LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME,
                "Synced brain B cell %u -> swarm memory cell %u",
                b_cell_id, swarm_cell_id);
        }
        return 0;
    }

    nimcp_mutex_unlock(system->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_immune_sync_memory_to_swarm: validation failed");
    return -1;
}


/**
 * @brief Trigger swarm response from brain antibody
 *
 * WHAT: Execute swarm immune response when brain antibody is activated
 * WHY:  Translate brain immune action to swarm-level coordinated response
 * HOW:  Map antibody class to swarm response type and execute
 */
int brain_immune_trigger_swarm_response(
    brain_immune_system_t* system,
    uint32_t antibody_id
) {
    if (!system || !system->swarm_immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_trigger_swarm_response: required parameter is NULL (system, system->swarm_immune)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_trigger_swarm_respon", 0.0f);


    brain_antibody_t* antibody = find_antibody_by_id(system, antibody_id);
    if (!antibody || !antibody->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_trigger_swarm_response: required parameter is NULL (antibody, antibody->active)");
        return -1;
    }

    brain_antigen_t* antigen = find_antigen_by_id(system, antibody->target_antigen_id);
    if (!antigen) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_trigger_swarm_response: antigen is NULL");
        return -1;
    }

    /* If antibody already has swarm response, execute it */
    if (antibody->swarm_response_id != 0) {
        return nimcp_swarm_immune_execute_response(
            system->swarm_immune,
            antibody->swarm_response_id
        ) == NIMCP_SUCCESS ? 0 : -1;
    }

    /* Generate new swarm response based on antibody class */
    uint32_t threat_id = antigen->source_node_id;  /* Use source node as threat ID */
    uint32_t response_id = 0;

    nimcp_result_t res = nimcp_swarm_immune_generate_response(
        system->swarm_immune,
        threat_id,
        &response_id
    );

    if (res == NIMCP_SUCCESS) {
        nimcp_mutex_lock(system->mutex);
        antibody->swarm_response_id = response_id;
        nimcp_mutex_unlock(system->mutex);

        /* Execute the response */
        nimcp_swarm_immune_execute_response(system->swarm_immune, response_id);

        if (system->config.enable_logging) {
            LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME,
                "Triggered swarm response %u from brain antibody %u",
                response_id, antibody_id);
        }
        return 0;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_immune_trigger_swarm_response: validation failed");
    return -1;
}


/**
 * @brief Broadcast collective inflammation state to swarm
 *
 * WHAT: Share inflammation level across swarm nodes via consensus
 * WHY:  Enable swarm-wide coordinated inflammatory response
 * HOW:  Send cytokine message with inflammation severity, use consensus to agree
 */
int brain_immune_broadcast_inflammation_state(
    brain_immune_system_t* system,
    uint32_t site_id
) {
    if (!system || !system->swarm_immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_broadcast_inflammation_state: required parameter is NULL (system, system->swarm_immune)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_broadcast_inflammati", 0.0f);


    brain_inflammation_site_t* site = find_inflammation_by_id(system, site_id);
    if (!site) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_broadcast_inflammation_state: site is NULL");
        return -1;
    }

    /* Map inflammation level to swarm severity */
    NimcpSwarmSeverity severity;
    switch (site->level) {
        case INFLAMMATION_LOCAL:    severity = SWARM_SEVERITY_LOW; break;
        case INFLAMMATION_REGIONAL: severity = SWARM_SEVERITY_MEDIUM; break;
        case INFLAMMATION_SYSTEMIC: severity = SWARM_SEVERITY_HIGH; break;
        case INFLAMMATION_STORM:    severity = SWARM_SEVERITY_CRITICAL; break;
        default:                    severity = SWARM_SEVERITY_LOW; break;
    }

    /* Broadcast via swarm immune alert */
    nimcp_result_t res = nimcp_swarm_immune_broadcast_alert(
        system->swarm_immune,
        site->triggering_antigen_id,
        severity
    );

    /* Also release pro-inflammatory cytokine */
    if (res == NIMCP_SUCCESS && system->config.enable_bio_async) {
        uint32_t cytokine_id = 0;
        brain_cytokine_type_t type = (site->level >= INFLAMMATION_SYSTEMIC)
            ? CYTOKINE_TNFA : CYTOKINE_IL6;

        brain_immune_release_cytokine(
            system, type, 0,
            site->resource_allocation,
            0,  /* Broadcast to all */
            &cytokine_id
        );
    }

    if (system->config.enable_logging) {
        LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME,
            "Broadcast inflammation state: site %u, level %s",
            site_id, brain_immune_inflammation_to_string(site->level));
    }

    return res == NIMCP_SUCCESS ? 0 : -1;
}


/**
 * @brief Request consensus on threat severity via swarm
 *
 * WHAT: Use swarm consensus to assess threat severity collectively
 * WHY:  Prevent false positives, ensure distributed agreement on threats
 * HOW:  Each node votes on severity, weighted by confidence, use cytokine messaging
 */
int brain_immune_consensus_threat_severity(
    brain_immune_system_t* system,
    uint32_t antigen_id,
    float* agreed_severity_out
) {
    if (!system || !system->swarm_immune || !agreed_severity_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_consensus_threat_severity: required parameter is NULL (system, system->swarm_immune, agreed_severity_out)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_consensus_threat_sev", 0.0f);


    brain_antigen_t* antigen = find_antigen_by_id(system, antigen_id);
    if (!antigen) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_consensus_threat_severity: antigen is NULL");
        return -1;
    }

    /* Confirm threat via swarm consensus */
    uint32_t threat_id = antigen->id;
    uint32_t confirming_drone = system->swarm_immune->self_drone_id;

    nimcp_result_t res = nimcp_swarm_immune_confirm_threat(
        system->swarm_immune,
        threat_id,
        confirming_drone
    );

    if (res == NIMCP_SUCCESS) {
        /* Get confirmed threat info */
        const NimcpSwarmThreat* threat = NULL;
        res = nimcp_swarm_immune_get_threat(
            system->swarm_immune,
            threat_id,
            &threat
        );

        if (res == NIMCP_SUCCESS && threat) {
            /* Update antigen with consensus information */
            nimcp_mutex_lock(system->mutex);
            /* Note: brain_antigen_t uses 'processed' rather than 'confirmed' */
            antigen->processed = threat->confirmed;
            antigen->confidence = threat->confidence;

            /* Map swarm confirming drones to severity adjustment */
            float severity_factor = (float)threat->confirming_drones / 10.0f;
            if (severity_factor > 1.0f) severity_factor = 1.0f;

            *agreed_severity_out = antigen->severity * severity_factor;
            nimcp_mutex_unlock(system->mutex);

            if (system->config.enable_logging) {
                LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME,
                    "Consensus on antigen %u: confirmed=%d, drones=%u, severity=%.2f",
                    antigen_id, threat->confirmed, threat->confirming_drones,
                    *agreed_severity_out);
            }
            return 0;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_immune_consensus_threat_severity: operation failed");
    return -1;
}


/**
 * @brief Propagate secondary response across swarm when memory cell recognizes threat
 *
 * WHAT: When any node recognizes learned threat, trigger swarm-wide secondary response
 * WHY:  Collective memory - if one node remembers, entire swarm benefits
 * HOW:  Share memory cell activation, broadcast rapid response to all nodes
 */
int brain_immune_propagate_secondary_response(
    brain_immune_system_t* system,
    uint32_t memory_b_cell_id,
    uint32_t antigen_id
) {
    if (!system || !system->swarm_immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_propagate_secondary_response: required parameter is NULL (system, system->swarm_immune)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_propagate_secondary_", 0.0f);

    nimcp_mutex_lock(system->mutex);

    brain_b_cell_t* b_cell = find_b_cell_by_id(system, memory_b_cell_id);
    if (!b_cell || b_cell->state != B_CELL_MEMORY) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_propagate_secondary_response: b_cell is NULL");
        return -1;
    }

    /* Share memory cell with swarm if not already shared */
    if (b_cell->swarm_memory_cell_id == 0) {
        nimcp_mutex_unlock(system->mutex);
        brain_immune_sync_memory_to_swarm(system, memory_b_cell_id);
        nimcp_mutex_lock(system->mutex);
        /* Re-lookup after releasing lock (cell may have moved) */
        b_cell = find_b_cell_by_id(system, memory_b_cell_id);
        if (!b_cell) {
            nimcp_mutex_unlock(system->mutex);
            return -1;
        }
    }

    /* Share memory cell with entire swarm */
    uint32_t swarm_cell_id = b_cell->swarm_memory_cell_id;
    float memory_multiplier = system->config.memory_response_multiplier;
    bool logging = system->config.enable_logging;

    nimcp_mutex_unlock(system->mutex);

    if (swarm_cell_id != 0) {
        nimcp_result_t res = nimcp_swarm_immune_share_memory_cell(
            system->swarm_immune,
            swarm_cell_id
        );

        if (res == NIMCP_SUCCESS && logging) {
            LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME,
                "Propagated secondary response: memory cell %u shared across swarm",
                swarm_cell_id);
        }
    }

    /* Broadcast rapid response alert with high priority */
    brain_immune_broadcast_alert(system, antigen_id, INFLAMMATION_REGIONAL);

    /* Release coordinating cytokines */
    uint32_t cytokine_id = 0;
    brain_immune_release_cytokine(
        system, BRAIN_CYTOKINE_IFN_GAMMA,
        memory_b_cell_id,
        memory_multiplier,
        0,  /* Broadcast */
        &cytokine_id
    );

    return 0;
}


/* ============================================================================
 * Antigen Presentation API
 * ============================================================================ */

/**
 * @brief Present generic antigen
 */
int brain_immune_present_antigen(
    brain_immune_system_t* system,
    brain_antigen_source_t source,
    const uint8_t* epitope,
    size_t epitope_len,
    uint32_t severity,
    uint32_t source_node,
    uint32_t* antigen_id
) {
    if (!system || !epitope || epitope_len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_present_antigen: required parameter is NULL (system, epitope)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_present_antigen", 0.0f);


    nimcp_mutex_lock(system->mutex);

    /* TOCTOU fix: capacity check moved AFTER mutex lock */
    if (system->antigen_count >= system->antigen_capacity) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "brain_immune_present_antigen: capacity exceeded");
        return -1;
    }

    brain_antigen_t* antigen = &system->antigens[system->antigen_count];
    memset(antigen, 0, sizeof(*antigen));

    antigen->id = system->next_antigen_id++;
    antigen->source = source;
    antigen->epitope_len = (epitope_len > BRAIN_IMMUNE_EPITOPE_SIZE)
        ? BRAIN_IMMUNE_EPITOPE_SIZE : epitope_len;
    memcpy(antigen->epitope, epitope, antigen->epitope_len);

    antigen->source_node_id = source_node;
    antigen->severity = severity;
    antigen->confidence = 1.0f;
    antigen->danger_signal = severity / 10.0f;
    antigen->detection_time = get_timestamp_ms();
    antigen->processed = false;
    antigen->neutralized = false;

    /* G6 SECURITY: Immune tolerance — attenuate response for repeated antigens
     * WHAT: Count how many times the same epitope has been seen; reduce severity
     *       if the same antigen hash appears more than 10 times.
     * WHY:  Biological immune systems develop tolerance to frequently-encountered
     *       benign antigens. Without this, repeated exceptions (e.g., from a noisy
     *       sensor) can overwhelm the immune system and starve real threats.
     * HOW:  Scan existing antigens for matching epitope. If count > 10, halve
     *       the severity and confidence — simulating peripheral tolerance. */
    {
        uint32_t same_epitope_count = 0;
        for (size_t j = 0; j < system->antigen_count; j++) {
            if (&system->antigens[j] == antigen) continue; /* Skip self (just added) */
            if (system->antigens[j].epitope_len == antigen->epitope_len &&
                memcmp(system->antigens[j].epitope, antigen->epitope,
                       antigen->epitope_len) == 0) {
                same_epitope_count++;
            }
        }
        if (same_epitope_count > 10) {
            /* Tolerance: reduce response strength by 50% */
            antigen->severity = (antigen->severity + 1) / 2;  /* Integer halving, min 1 */
            antigen->confidence *= 0.5f;
            antigen->danger_signal *= 0.5f;
            if (system->config.enable_logging) {
                LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME,
                    "Immune tolerance: epitope seen %u times — attenuating response "
                    "(severity=%u, confidence=%.2f)",
                    same_epitope_count, antigen->severity, antigen->confidence);
            }
        }
    }

    uint32_t new_antigen_id = antigen->id;
    if (antigen_id) *antigen_id = new_antigen_id;
    system->antigen_count++;
    system->stats.antigens_processed++;

    /* Transition to recognition phase */
    if (system->phase == IMMUNE_PHASE_SURVEILLANCE) {
        system->phase = IMMUNE_PHASE_RECOGNITION;
    }

    /* Copy antigen data before unlocking for safe callback invocation */
    brain_antigen_t antigen_copy = *antigen;
    void* callback_user_data = system->on_antigen_user_data;
    brain_immune_antigen_cb_t antigen_callback = system->on_antigen;
    void* kg_brain = system->brain_ref;
    uint32_t kg_severity = antigen->severity;

    nimcp_mutex_unlock(system->mutex);

    /* Trigger callback with copied data (safe after unlock) */
    if (antigen_callback) {
        antigen_callback(system, &antigen_copy, callback_user_data);
    }

    if (system->config.enable_logging) {
        LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME,
            "Antigen presented: id=%u, source=%d, severity=%u",
            new_antigen_id, source, severity);
    }

    /* W11: emit KG antigen event (additive to the callback dispatch). */
    if (kg_brain) {
        const char* src_name = "generic";
        switch (source) {
            case ANTIGEN_SOURCE_BBB:     src_name = "bbb_threat";   break;
            case ANTIGEN_SOURCE_BFT:     src_name = "byzantine";    break;
            case ANTIGEN_SOURCE_ANOMALY: src_name = "anomaly";      break;
            case ANTIGEN_SOURCE_SWARM:   src_name = "swarm_threat"; break;
            case ANTIGEN_SOURCE_MANUAL:  src_name = "manual";       break;
            default: break;
        }
        /* severity ∈ [0..10] in immune convention → map to [0..1]. */
        float sev_f = (float)kg_severity / 10.0f;
        if (sev_f > 1.0f) sev_f = 1.0f;
        w11_emit_immune_antigen((struct brain_struct*)kg_brain, src_name, sev_f);
    }

    /* AUTO-RECOGNITION: Check if we've seen this threat before */
    uint32_t memory_b_cell_id = 0;
    if (brain_immune_check_memory(system, new_antigen_id, &memory_b_cell_id) == 0) {
        /* Memory match found - trigger automatic secondary response */
        if (system->config.enable_logging) {
            LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME,
                "LEARNED THREAT RECOGNIZED: Antigen %u matches memory B cell %u - triggering secondary response",
                new_antigen_id, memory_b_cell_id);
        }
        brain_immune_secondary_response(system, new_antigen_id, memory_b_cell_id);
    }

    return 0;
}


/**
 * @brief Present BBB threat as antigen
 */
int brain_immune_present_bbb_threat(
    brain_immune_system_t* system,
    bbb_threat_type_t threat_type,
    bbb_severity_t severity,
    const uint8_t* threat_data,
    size_t data_len,
    uint32_t* antigen_id
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_present_bbb_threat: system is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Map BBB severity to 1-10 scale */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_present_bbb_threat", 0.0f);


    uint32_t immune_severity = 0;
    switch (severity) {
        case BBB_SEVERITY_LOW:      immune_severity = 3; break;
        case BBB_SEVERITY_MEDIUM:   immune_severity = 5; break;
        case BBB_SEVERITY_HIGH:     immune_severity = 7; break;
        case BBB_SEVERITY_CRITICAL: immune_severity = 10; break;
        default: immune_severity = 1; break;
    }

    int result = brain_immune_present_antigen(
        system, ANTIGEN_SOURCE_BBB,
        threat_data, data_len,
        immune_severity, 0, antigen_id
    );

    if (result == 0 && antigen_id) {
        /* Set BBB-specific fields */
        brain_antigen_t* ag = find_antigen_by_id(system, *antigen_id);
        if (ag) {
            ag->bbb_threat_type = threat_type;
        }
        system->stats.bbb_threats_processed++;
    }

    return result;
}


/**
 * @brief Present Byzantine node as antigen
 */
int brain_immune_present_byzantine(
    brain_immune_system_t* system,
    uint32_t node_id,
    bft_behavior_t behavior,
    const bft_evidence_t* evidence,
    size_t evidence_len,
    uint32_t* antigen_id
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_present_byzantine: system is NULL");
        return -1;
    }

    /* Create epitope from node ID and behavior */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_present_byzantine", 0.0f);


    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    memset(epitope, 0, sizeof(epitope));
    memcpy(epitope, &node_id, sizeof(node_id));
    memcpy(epitope + sizeof(node_id), &behavior, sizeof(behavior));

    /* Byzantine nodes are always severe */
    uint32_t severity = 8;
    if (behavior == BFT_BEHAV_COLLUSION) severity = 10;

    int result = brain_immune_present_antigen(
        system, ANTIGEN_SOURCE_BFT,
        epitope, sizeof(node_id) + sizeof(behavior),
        severity, node_id, antigen_id
    );

    if (result == 0 && antigen_id) {
        brain_antigen_t* ag = find_antigen_by_id(system, *antigen_id);
        if (ag) {
            ag->bft_behavior = behavior;
        }
        system->stats.bft_byzantines_handled++;
    }

    return result;
}


/**
 * @brief Present swarm threat as antigen
 */
int brain_immune_present_swarm_threat(
    brain_immune_system_t* system,
    const NimcpSwarmThreat* threat,
    uint32_t* antigen_id
) {
    if (!system || !threat) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_present_swarm_threat: required parameter is NULL (system, threat)");
        return -1;
    }

    /* Map swarm severity */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_present_swarm_threat", 0.0f);


    uint32_t severity = 0;
    switch (threat->severity) {
        case SWARM_SEVERITY_LOW:      severity = 3; break;
        case SWARM_SEVERITY_MEDIUM:   severity = 5; break;
        case SWARM_SEVERITY_HIGH:     severity = 7; break;
        case SWARM_SEVERITY_CRITICAL: severity = 10; break;
        default: severity = 5; break;
    }

    int result = brain_immune_present_antigen(
        system, ANTIGEN_SOURCE_SWARM,
        threat->data, threat->data_len,
        severity, threat->source_drone_id, antigen_id
    );

    if (result == 0) {
        system->stats.swarm_alerts_processed++;

        /* Wire consensus: refine severity via swarm consensus if available.
         * WHAT: Use multi-agent BFT consensus to agree on threat severity
         * WHY:  Single-node severity mapping may be inaccurate; consensus
         *       leverages collective intelligence for better threat assessment
         * HOW:  Call consensus after antigen is presented; log if severity changes */
        if (antigen_id && *antigen_id != 0 && system->swarm_immune) {
            float agreed_severity = 0.0f;
            int consensus_ok = brain_immune_consensus_threat_severity(
                system, *antigen_id, &agreed_severity);
            if (consensus_ok == 0 && agreed_severity > 0.0f) {
                /* Update antigen severity with consensus value */
                brain_antigen_t* ag = find_antigen_by_id(system, *antigen_id);
                if (ag && (uint32_t)agreed_severity != severity) {
                    ag->severity = (uint32_t)agreed_severity;
                    if (system->config.enable_logging) {
                        LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME,
                            "Swarm consensus adjusted threat %u severity: %u -> %u",
                            *antigen_id, severity, (uint32_t)agreed_severity);
                    }
                }
            }
        }
    }

    return result;
}


/* ============================================================================
 * B Cell API
 * ============================================================================ */

/**
 * @brief Activate B cell for antigen
 */
int brain_immune_activate_b_cell(
    brain_immune_system_t* system,
    uint32_t antigen_id,
    uint32_t* b_cell_id
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_activate_b_cell: system is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_activate_b_cell", 0.0f);


    brain_antigen_t* antigen = find_antigen_by_id(system, antigen_id);
    if (!antigen) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_activate_b_cell: antigen is NULL");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    /* W7: Capacity check under lock to prevent TOCTOU race */
    if (system->b_cell_count >= system->b_cell_capacity) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "brain_immune_activate_b_cell: capacity exceeded");
        return -1;
    }

    /* Create new B cell */
    brain_b_cell_t* b_cell = &system->b_cells[system->b_cell_count];
    memset(b_cell, 0, sizeof(*b_cell));

    b_cell->id = system->next_b_cell_id++;
    b_cell->state = B_CELL_ACTIVATED;
    b_cell->receptor_len = antigen->epitope_len;
    memcpy(b_cell->receptor, antigen->epitope, b_cell->receptor_len);
    b_cell->affinity = antigen->confidence;
    b_cell->bound_antigen_id = antigen_id;
    b_cell->activation_time = get_timestamp_ms();

    if (b_cell_id) *b_cell_id = b_cell->id;
    system->b_cell_count++;
    system->stats.active_b_cells++;

    /* Update phase */
    if (system->phase == IMMUNE_PHASE_RECOGNITION) {
        system->phase = IMMUNE_PHASE_ACTIVATION;
    }

    nimcp_mutex_unlock(system->mutex);

    if (system->config.enable_logging) {
        LOG_MODULE_DEBUG(BRAIN_IMMUNE_MODULE_NAME,
            "B cell activated: id=%u for antigen=%u", b_cell->id, antigen_id);
    }

    return 0;
}


/**
 * @brief Convert B cell to memory
 */
int brain_immune_b_cell_to_memory(brain_immune_system_t* system, uint32_t b_cell_id) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_b_cell_to_memory: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_b_cell_to_memory", 0.0f);


    brain_b_cell_t* b_cell = find_b_cell_by_id(system, b_cell_id);
    if (!b_cell) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_b_cell_to_memory: b_cell is NULL");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    b_cell->state = B_CELL_MEMORY;
    system->stats.memory_cells++;

    /* If connected to swarm immune, create swarm memory cell */
    if (system->swarm_immune) {
        NimcpSwarmThreatSignature sig;
        memset(&sig, 0, sizeof(sig));
        memcpy(sig.pattern, b_cell->receptor, b_cell->receptor_len);
        sig.pattern_len = b_cell->receptor_len;
        sig.match_threshold = system->config.recognition_threshold;

        NimcpSwarmMemoryCell mem_cell;
        memset(&mem_cell, 0, sizeof(mem_cell));
        mem_cell.signature = sig;
        mem_cell.effectiveness = b_cell->affinity;
        mem_cell.created_time = get_timestamp_ms();

        /* Note: Would call nimcp_swarm_immune_add_memory_cell if available */
        b_cell->swarm_memory_cell_id = mem_cell.id;
    }

    nimcp_mutex_unlock(system->mutex);

    if (system->config.enable_logging) {
        LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME,
            "B cell %u converted to memory", b_cell_id);
    }

    return 0;
}


/* ============================================================================
 * T Cell API
 * ============================================================================ */

/**
 * @brief Activate helper T cell
 */
int brain_immune_activate_helper_t(
    brain_immune_system_t* system,
    uint32_t antigen_id,
    uint32_t* t_cell_id
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_activate_helper_t: system is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_activate_helper_t", 0.0f);


    brain_antigen_t* antigen = find_antigen_by_id(system, antigen_id);
    if (!antigen) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_activate_helper_t: antigen is NULL");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    /* W7: Capacity check under lock to prevent TOCTOU race */
    if (system->t_cell_count >= system->t_cell_capacity) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "brain_immune_activate_helper_t: capacity exceeded");
        return -1;
    }

    brain_t_cell_t* t_cell = &system->t_cells[system->t_cell_count];
    memset(t_cell, 0, sizeof(*t_cell));

    t_cell->id = system->next_t_cell_id++;
    t_cell->type = T_CELL_HELPER;
    t_cell->receptor_len = antigen->epitope_len;
    memcpy(t_cell->receptor, antigen->epitope, t_cell->receptor_len);
    t_cell->recognized_antigen_id = antigen_id;
    t_cell->activation_level = antigen->danger_signal;
    t_cell->activation_time = get_timestamp_ms();

    if (t_cell_id) *t_cell_id = t_cell->id;
    system->t_cell_count++;
    system->stats.active_t_cells++;

    nimcp_mutex_unlock(system->mutex);

    /* Helper T cells release cytokines on activation */
    uint32_t cytokine_id = 0;
    brain_immune_release_cytokine(system, CYTOKINE_IL6, t_cell->id,
                                   t_cell->activation_level, 0, &cytokine_id);

    if (system->config.enable_logging) {
        LOG_MODULE_DEBUG(BRAIN_IMMUNE_MODULE_NAME,
            "Helper T cell activated: id=%u", t_cell->id);
    }

    return 0;
}


/**
 * @brief Activate killer T cell
 */
int brain_immune_activate_killer_t(
    brain_immune_system_t* system,
    uint32_t antigen_id,
    uint32_t* t_cell_id
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_activate_killer_t: system is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_activate_killer_t", 0.0f);


    brain_antigen_t* antigen = find_antigen_by_id(system, antigen_id);
    if (!antigen) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_activate_killer_t: antigen is NULL");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    /* W7: Capacity check under lock to prevent TOCTOU race */
    if (system->t_cell_count >= system->t_cell_capacity) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "brain_immune_activate_killer_t: capacity exceeded");
        return -1;
    }

    brain_t_cell_t* t_cell = &system->t_cells[system->t_cell_count];
    memset(t_cell, 0, sizeof(*t_cell));

    t_cell->id = system->next_t_cell_id++;
    t_cell->type = T_CELL_KILLER;
    t_cell->receptor_len = antigen->epitope_len;
    memcpy(t_cell->receptor, antigen->epitope, t_cell->receptor_len);
    t_cell->recognized_antigen_id = antigen_id;
    t_cell->activation_level = antigen->danger_signal;
    t_cell->activation_time = get_timestamp_ms();

    if (t_cell_id) *t_cell_id = t_cell->id;
    system->t_cell_count++;
    system->stats.active_t_cells++;

    nimcp_mutex_unlock(system->mutex);

    if (system->config.enable_logging) {
        LOG_MODULE_DEBUG(BRAIN_IMMUNE_MODULE_NAME,
            "Killer T cell activated: id=%u", t_cell->id);
    }

    return 0;
}


/**
 * @brief Execute killer T cell action
 */
int brain_immune_t_cell_kill(
    brain_immune_system_t* system,
    uint32_t t_cell_id,
    uint32_t target_node
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_t_cell_kill: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_t_cell_kill", 0.0f);


    brain_t_cell_t* t_cell = find_t_cell_by_id(system, t_cell_id);
    if (!t_cell || t_cell->type != T_CELL_KILLER) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_immune_t_cell_kill: t_cell is NULL");
        return -1;
    }

    /* Execute quarantine via BFT if connected */
    if (system->bft_context) {
        bft_quarantine_node(system->bft_context, target_node,
                           system->config.antibody_half_life_ms);
    }

    /* Capture callback and data under mutex to prevent race condition */
    brain_immune_kill_cb_t kill_callback = NULL;
    void* callback_user_data = NULL;
    brain_t_cell_t t_cell_copy;

    nimcp_mutex_lock(system->mutex);
    t_cell->kills++;
    kill_callback = system->on_kill;
    callback_user_data = system->on_kill_user_data;
    t_cell_copy = *t_cell;  /* Copy for safe callback invocation */
    nimcp_mutex_unlock(system->mutex);

    /* Trigger callback with copied data (safe after unlock) */
    if (kill_callback) {
        kill_callback(system, &t_cell_copy, target_node, callback_user_data);
    }

    if (system->config.enable_logging) {
        LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME,
            "Killer T cell %u eliminated node %u", t_cell_id, target_node);
    }

    return 0;
}


/**
 * @brief Helper T provides help to B cell
 */
int brain_immune_t_help_b(
    brain_immune_system_t* system,
    uint32_t helper_id,
    uint32_t b_cell_id
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_t_help_b: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_t_help_b", 0.0f);


    brain_t_cell_t* helper = find_t_cell_by_id(system, helper_id);
    if (!helper || helper->type != T_CELL_HELPER) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_immune_t_help_b: helper is NULL");
        return -1;
    }

    brain_b_cell_t* b_cell = find_b_cell_by_id(system, b_cell_id);
    if (!b_cell) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_t_help_b: b_cell is NULL");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    b_cell->received_t_help = true;
    b_cell->affinity *= system->config.helper_amplification;
    if (b_cell->affinity > 1.0f) b_cell->affinity = 1.0f;

    helper->help_given++;

    /* Transition B cell to plasma state */
    if (b_cell->state == B_CELL_ACTIVATED) {
        b_cell->state = B_CELL_PLASMA;
    }

    nimcp_mutex_unlock(system->mutex);

    if (system->config.enable_logging) {
        LOG_MODULE_DEBUG(BRAIN_IMMUNE_MODULE_NAME,
            "Helper T %u helped B cell %u", helper_id, b_cell_id);
    }

    return 0;
}


/* ============================================================================
 * Antibody API
 * ============================================================================ */

/**
 * @brief Produce antibody from B cell
 */
int brain_immune_produce_antibody(
    brain_immune_system_t* system,
    uint32_t b_cell_id,
    brain_antibody_class_t ab_class,
    uint32_t* antibody_id
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_produce_antibody: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_produce_antibody", 0.0f);


    nimcp_mutex_lock(system->mutex);

    /* Check capacity inside lock to avoid race */
    if (system->antibody_count >= system->antibody_capacity) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "brain_immune_produce_antibody: capacity exceeded");
        return -1;
    }

    /* Find and validate B cell inside critical section */
    brain_b_cell_t* b_cell = find_b_cell_by_id(system, b_cell_id);
    if (!b_cell || b_cell->state != B_CELL_PLASMA) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_immune_produce_antibody: b_cell is NULL");
        return -1;
    }

    /* Phase D.4: IL-4-driven class switching IgM → IgG.
     * If IL-4 is present (>0.3) AND this B cell has matured enough
     * (triggered ≥3 times → 3+ prior antibodies), upgrade an incoming
     * IgM request to IgG. Mirrors biology: Th2 cytokines drive affinity
     * maturation and class switching during the germinal-center reaction.
     *
     * Uses brain_immune_get_cytokine_level which is mutex-locked itself,
     * but the inner mutex is reentrant on this platform via the platform
     * mutex layer. To be safe we read the float field from stats which
     * is already current (last recompute), avoiding nested-lock risk. */
    bool class_switched_this_call = false;
    if (ab_class == ANTIBODY_IGM) {
        float il4_level = system->stats.cytokine_il4;
        if (il4_level > 0.3f && b_cell->antibodies_produced >= 3) {
            ab_class = ANTIBODY_IGG;
            class_switched_this_call = true;
        }
    }

    brain_antibody_t* antibody = &system->antibodies[system->antibody_count];
    memset(antibody, 0, sizeof(*antibody));

    antibody->id = system->next_antibody_id++;
    antibody->ab_class = ab_class;
    antibody->target_antigen_id = b_cell->bound_antigen_id;
    antibody->producer_b_cell_id = b_cell_id;
    antibody->effectiveness = b_cell->affinity;
    antibody->creation_time = get_timestamp_ms();
    antibody->active = true;

    /* Map to swarm response based on class */
    switch (ab_class) {
        case ANTIBODY_IGM:
            antibody->swarm_response = RESPONSE_ALERT;
            break;
        case ANTIBODY_IGG:
            antibody->swarm_response = RESPONSE_ISOLATION;
            break;
        case ANTIBODY_IGE:
            antibody->swarm_response = RESPONSE_COUNTER_ATTACK;
            break;
    }

    if (antibody_id) *antibody_id = antibody->id;
    system->antibody_count++;
    system->stats.active_antibodies++;
    system->stats.responses_generated++;
    b_cell->antibodies_produced++;

    /* Update phase to effector */
    if (system->phase == IMMUNE_PHASE_ACTIVATION) {
        system->phase = IMMUNE_PHASE_EFFECTOR;
    }

    nimcp_mutex_unlock(system->mutex);

    /* Phase D.3: Emit IL-4 on IgM→IgG class switch (post-unlock to avoid
     * nested-lock risk inside release_cytokine).  Concentration 0.3 is a
     * mild reinforcing pulse — class switching itself should not trigger
     * a Treg storm response. */
    if (class_switched_this_call) {
        (void)brain_immune_release_cytokine(system, BRAIN_CYTOKINE_IL4,
                                            b_cell_id, 0.3f, 0, NULL);
    }

    if (system->config.enable_logging) {
        LOG_MODULE_DEBUG(BRAIN_IMMUNE_MODULE_NAME,
            "Antibody produced: id=%u, class=%d", antibody->id, ab_class);
    }

    return 0;
}


/**
 * @brief Mark antigen as neutralized
 */
int brain_immune_neutralize(
    brain_immune_system_t* system,
    uint32_t antigen_id,
    uint32_t antibody_id
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_neutralize: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_neutralize", 0.0f);


    nimcp_mutex_lock(system->mutex);

    /* Find and validate inside critical section */
    brain_antigen_t* antigen = find_antigen_by_id(system, antigen_id);
    if (!antigen) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_neutralize: antigen is NULL");
        return -1;
    }

    brain_antibody_t* antibody = find_antibody_by_id(system, antibody_id);
    if (!antibody) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_neutralize: antibody is NULL");
        return -1;
    }

    antigen->neutralized = true;
    antigen->response_count++;
    antibody->neutralizations++;
    system->stats.threats_neutralized++;

    /* Copy IDs before unlock for safe access after critical section */
    uint32_t producer_b_cell_id = antibody->producer_b_cell_id;
    brain_antibody_t antibody_copy = *antibody;

    /* AUTO-LEARNING: Convert producing B cell to memory for future recognition */
    /* Done inside critical section to safely access B cell state */
    brain_b_cell_t* producer = find_b_cell_by_id(system, producer_b_cell_id);
    bool should_convert_to_memory = (producer && producer->state != B_CELL_MEMORY);

    /* Copy callback info before unlock */
    void* callback_user_data = system->on_neutralize_user_data;
    brain_immune_neutralize_cb_t neutralize_callback = system->on_neutralize;
    bool enable_logging = system->config.enable_logging;

    nimcp_mutex_unlock(system->mutex);

    /* Perform memory conversion outside lock (function handles its own locking) */
    if (should_convert_to_memory) {
        brain_immune_b_cell_to_memory(system, producer_b_cell_id);

        if (enable_logging) {
            LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME,
                "Auto-learning: B cell %u converted to memory after neutralizing antigen %u",
                producer_b_cell_id, antigen_id);
        }
    }

    /* Trigger callback with copied data (safe after unlock) */
    if (neutralize_callback) {
        neutralize_callback(system, antigen_id, &antibody_copy, callback_user_data);
    }

    if (enable_logging) {
        LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME,
            "Antigen %u neutralized by antibody %u", antigen_id, antibody_id);
    }

    return 0;
}


/* ============================================================================
 * Cytokine Signaling API
 * ============================================================================ */

/**
 * @brief Release cytokine signal
 */
int brain_immune_release_cytokine(
    brain_immune_system_t* system,
    brain_cytokine_type_t type,
    uint32_t source_cell,
    float concentration,
    uint32_t target_region,
    uint32_t* cytokine_id
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_release_cytokine: system is NULL");
        return -1;
    }
    if (!system->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_release_cytokine: system->mutex is NULL");
        return -1;
    }
    if (!system->cytokines) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_release_cytokine: system->cytokines is NULL");
        return -1;
    }
    /* Validate concentration is in expected range [0.0, 1.0] */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_release_cytokine", 0.0f);


    if (concentration < 0.0f || !isfinite(concentration)) concentration = 0.0f;
    if (concentration > 1.0f) concentration = 1.0f;

    /* Phase C.3: Apply Treg suppression to pro-inflammatory cytokines.
     * IL-1, IL-6, TNF are the storm drivers — query the current Treg
     * suppression factor and damp those concentrations. IL-10 (anti-
     * inflammatory) and IL-4 (Th2/class-switching) pass through unchanged.
     * IFN-γ also passes — it's antiviral, not a generic storm driver. */
    bool is_pro_inflammatory = (type == CYTOKINE_IL1B ||
                                type == CYTOKINE_IL6  ||
                                type == CYTOKINE_TNFA);
    if (is_pro_inflammatory && system->tregs) {
        treg_system_t* treg = (treg_system_t*)system->tregs;
        float damp = treg_get_suppression_factor(treg);
        if (damp > 0.0f) {
            if (damp > 1.0f) damp = 1.0f;
            concentration *= (1.0f - damp);
            /* Observability: record that we damped a pro-inflammatory release.
             * Best-effort, not lock-protected — the counter is monotonic and
             * only used for monitoring. */
            treg->stats.cytokines_damped++;
        }
    }

    nimcp_mutex_lock(system->mutex);

    /* W7: Capacity check under lock to prevent TOCTOU race */
    if (system->cytokine_count >= system->cytokine_capacity) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "brain_immune_release_cytokine: capacity exceeded");
        return -1;
    }

    brain_cytokine_t* cytokine = &system->cytokines[system->cytokine_count];
    memset(cytokine, 0, sizeof(*cytokine));

    cytokine->id = system->next_cytokine_id++;
    cytokine->type = type;
    cytokine->source_cell_id = source_cell;
    cytokine->target_region = target_region;
    cytokine->concentration = concentration;
    cytokine->release_time = get_timestamp_ms();

    /* Determine if pro-inflammatory */
    cytokine->pro_inflammatory = (type != CYTOKINE_IL10);

    /* Map to bio-async message type */
    switch (type) {
        case CYTOKINE_IL1B:
        case CYTOKINE_IL6:
        case CYTOKINE_TNFA:
            cytokine->message_type = BIO_MSG_SECURITY_ALERT;
            break;
        case BRAIN_CYTOKINE_IFN_GAMMA:
            cytokine->message_type = BIO_MSG_SWARM_IMMUNE_ALERT;
            break;
        case CYTOKINE_IL10:
            cytokine->message_type = BIO_MSG_HEALTH_RESPONSE;
            break;
        default:
            cytokine->message_type = BIO_MSG_SECURITY_EVENT;
    }

    if (cytokine_id) *cytokine_id = cytokine->id;
    system->cytokine_count++;
    system->stats.cytokines_released++;

    /* Check for cytokine storm */
    float total_pro_inflammatory = 0;
    for (size_t i = 0; i < system->cytokine_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->cytokine_count > 256) {
            brain_immune_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)system->cytokine_count);
        }

        if (system->cytokines[i].pro_inflammatory) {
            total_pro_inflammatory += system->cytokines[i].concentration;
        }
    }
    if (total_pro_inflammatory >= system->config.cytokine_storm_threshold) {
        LOG_MODULE_WARN(BRAIN_IMMUNE_MODULE_NAME, "Cytokine storm risk detected!");

        /* Phase C.2: Engage Tregs to actively suppress further pro-cytokine
         * release. treg_update() pushes a delta-aware suppression factor
         * proportional to inflammation; we feed it the current pro-cyto
         * sum (mapped to ~storm level) so it picks the right band.
         * One-shot log on storm onset; cleared when suppression decays. */
        if (system->tregs) {
            (void)treg_update((treg_system_t*)system->tregs, 0);
            if (!system->treg_storm_engaged) {
                float damp = treg_get_suppression_factor(
                    (const treg_system_t*)system->tregs);
                LOG_MODULE_WARN(BRAIN_IMMUNE_MODULE_NAME,
                    "Treg engaged: damping pro-inflammatory cytokines by %.0f%%",
                    damp * 100.0f);
                system->treg_storm_engaged = true;
            }
        }
    } else if (system->treg_storm_engaged &&
               total_pro_inflammatory < (system->config.cytokine_storm_threshold * 0.5f)) {
        /* Storm has resolved — allow another onset log next time it spikes. */
        system->treg_storm_engaged = false;
    }

    /* Copy callback data before unlock to prevent race condition */
    brain_immune_cytokine_cb_t callback = system->on_cytokine;
    void* callback_data = system->on_cytokine_user_data;
    bio_module_context_t bio_ctx = system->bio_context;

    /* Set delivered flag BEFORE unlock to prevent race condition */
    /* Broadcast cytokines (target_region == 0) are always delivered locally.
     * Bio-async context handles remote delivery but local effects apply
     * regardless. */
    if (target_region == 0) {
        cytokine->delivered = true;
    }

    brain_cytokine_t cytokine_copy = *cytokine;

    nimcp_mutex_unlock(system->mutex);

    /* Bio-async message sending would happen here (outside lock) */
    /* The delivered flag was already set inside the critical section */

    /* Trigger callback with copied data */
    if (callback) {
        callback(system, &cytokine_copy, callback_data);
    }

    return 0;
}


/**
 * @brief Broadcast immune alert
 */
int brain_immune_broadcast_alert(
    brain_immune_system_t* system,
    uint32_t antigen_id,
    brain_inflammation_level_t severity
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_broadcast_alert: system is NULL");
        return -1;
    }

    /* Release appropriate cytokine for severity */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_broadcast_alert", 0.0f);


    /* Convert discrete severity to continuous for smooth cytokine selection */
    float cont_level = inflammation_level_to_continuous(severity);
    if (cont_level <= 0.0f) return 0;  /* NONE: nothing to broadcast */

    brain_cytokine_type_t type;
    float concentration = 0.0f;

    /* Cytokine type transitions smoothly based on continuous level */
    if (cont_level < 0.30f) {
        type = CYTOKINE_IL1B;       /* Low: IL-1b (local alert) */
    } else if (cont_level < 0.60f) {
        type = CYTOKINE_IL6;        /* Medium: IL-6 (regional) */
    } else {
        type = CYTOKINE_TNFA;       /* High: TNF-a (systemic/storm) */
    }

    /* Concentration scales linearly with continuous level */
    concentration = 0.2f + 0.7f * cont_level;  /* range [0.2, 0.9] */

    uint32_t cytokine_id = 0;
    return brain_immune_release_cytokine(system, type, 0, concentration, 0, &cytokine_id);
}


/**
 * @brief Escalate inflammation
 */
int brain_immune_escalate_inflammation(brain_immune_system_t* system, uint32_t site_id) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_escalate_inflammation: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_escalate_inflammatio", 0.0f);


    brain_inflammation_site_t* site = find_inflammation_by_id(system, site_id);
    if (!site) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_escalate_inflammation: site is NULL");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    /* Continuous escalation: increase by 0.25 (roughly one discrete step) */
    if (site->inflammation_level < 1.0f) {
        site->inflammation_level += 0.25f;
        if (site->inflammation_level > 1.0f) site->inflammation_level = 1.0f;
        site->level = inflammation_level_from_continuous(site->inflammation_level);

        inflammation_effects_t effects;
        inflammation_compute_effects(site->inflammation_level, &effects);
        site->resource_allocation = effects.resource_demand;
    }

    /* Capture callback and data under mutex to prevent race condition */
    brain_immune_inflammation_cb_t inflammation_callback = system->on_inflammation;
    void* callback_user_data = system->on_inflammation_user_data;
    brain_inflammation_site_t site_copy = *site;  /* Copy for safe callback invocation */

    nimcp_mutex_unlock(system->mutex);

    /* Notify registered listeners of escalated inflammation level */
    if (inflammation_callback) {
        inflammation_callback(system, &site_copy, callback_user_data);
    }

    /* Alert on escalation */
    brain_immune_broadcast_alert(system, site->triggering_antigen_id, site->level);

    if (system->config.enable_logging) {
        LOG_MODULE_WARN(BRAIN_IMMUNE_MODULE_NAME,
            "Inflammation escalated at site %u to level %s (continuous: %.3f)",
            site_id, brain_immune_inflammation_to_string(site->level),
            (double)site->inflammation_level);
    }

    return 0;
}


/**
 * @brief Resolve inflammation
 */
int brain_immune_resolve_inflammation(brain_immune_system_t* system, uint32_t site_id) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_resolve_inflammation: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_resolve_inflammation", 0.0f);


    brain_inflammation_site_t* site = find_inflammation_by_id(system, site_id);
    if (!site) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_resolve_inflammation: site is NULL");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);
    site->resolution_progress = 0.01f;  /* Start resolution */
    nimcp_mutex_unlock(system->mutex);

    /* Release anti-inflammatory cytokine */
    uint32_t cytokine_id = 0;
    brain_immune_release_cytokine(system, CYTOKINE_IL10, 0, 0.5f,
                                   site->region_id, &cytokine_id);

    if (system->config.enable_logging) {
        LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME,
            "Resolution started for inflammation site %u", site_id);
    }

    return 0;
}


/* ============================================================================
 * Memory Response API
 * ============================================================================ */

/**
 * @brief Check for memory cell match
 *
 * WHAT: Search memory B cells for matching antigen
 * WHY:  Enable faster secondary response to known threats
 * HOW:  Uses fuzzy affinity matching to recognize variants
 *
 * CROSS-REACTIVE IMMUNITY:
 * Even if exact match isn't found, partial matches are useful.
 * Returns the best matching memory cell above threshold.
 */
int brain_immune_check_memory(
    brain_immune_system_t* system,
    uint32_t antigen_id,
    uint32_t* b_cell_id
) {
    if (!system || !b_cell_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_check_memory: required parameter is NULL (system, b_cell_id)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_check_memory", 0.0f);


    brain_antigen_t* antigen = find_antigen_by_id(system, antigen_id);
    if (!antigen) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_check_memory: antigen is NULL");
        return -1;
    }

    float best_affinity = 0.0f;
    uint32_t best_match_id = 0;

    /* Search memory B cells for best match */
    for (size_t i = 0; i < system->b_cell_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->b_cell_count > 256) {
            brain_immune_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)system->b_cell_count);
        }

        brain_b_cell_t* b_cell = &system->b_cells[i];
        if (b_cell->state != B_CELL_MEMORY) continue;

        float affinity = brain_immune_compute_affinity(
            b_cell->receptor, b_cell->receptor_len,
            antigen->epitope, antigen->epitope_len
        );

        /* Track best match */
        if (affinity > best_affinity) {
            best_affinity = affinity;
            best_match_id = b_cell->id;
        }
    }

    /* Return best match if above threshold */
    if (best_affinity >= system->config.recognition_threshold) {
        *b_cell_id = best_match_id;

        if (system->config.enable_logging) {
            LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME,
                "Memory match found: B cell %u, affinity %.2f for antigen %u",
                best_match_id, best_affinity, antigen_id);
        }
        return 0;
    }

    /* Check for cross-reactive immunity (lower threshold) */
    float cross_reactive_threshold = system->config.recognition_threshold * 0.7f;
    if (best_affinity >= cross_reactive_threshold) {
        *b_cell_id = best_match_id;

        if (system->config.enable_logging) {
            LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME,
                "Cross-reactive match found: B cell %u, affinity %.2f (variant of known threat)",
                best_match_id, best_affinity);
        }
        return 0;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_immune_check_memory: validation failed");
    return -1;  /* No memory match */
}


/**
 * @brief Trigger secondary response
 */
int brain_immune_secondary_response(
    brain_immune_system_t* system,
    uint32_t antigen_id,
    uint32_t memory_b_cell_id
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_secondary_response: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_secondary_response", 0.0f);


    brain_b_cell_t* memory = find_b_cell_by_id(system, memory_b_cell_id);
    if (!memory || memory->state != B_CELL_MEMORY) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_secondary_response: memory is NULL");
        return -1;
    }

    /* Memory response is faster and stronger */
    nimcp_mutex_lock(system->mutex);

    /* Reactivate memory cell to plasma */
    memory->state = B_CELL_PLASMA;
    memory->bound_antigen_id = antigen_id;
    memory->affinity *= system->config.memory_response_multiplier;
    if (memory->affinity > 1.0f) memory->affinity = 1.0f;
    memory->activation_time = get_timestamp_ms();

    nimcp_mutex_unlock(system->mutex);

    /* Phase D.3: Memory secondary response releases IL-4 to drive Th2/B-cell
     * class switching for any nearby active B cells. Fired BEFORE the IGG
     * antibody is produced so the IL-4 level is already non-zero when D.4
     * checks it during plasma maturation in produce_antibody. */
    {
        uint32_t il4_id = 0;
        (void)brain_immune_release_cytokine(system, BRAIN_CYTOKINE_IL4, memory_b_cell_id,
                                            0.7f, 0, &il4_id);
    }

    /* Immediately produce antibodies */
    uint32_t antibody_id = 0;
    brain_immune_produce_antibody(system, memory_b_cell_id, ANTIBODY_IGG, &antibody_id);
    brain_immune_execute_antibody(system, antibody_id);

    if (system->config.enable_logging) {
        LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME,
            "Secondary response triggered for antigen %u", antigen_id);
    }

    return 0;
}


/**
 * @brief Compute affinity between patterns
 *
 * WHAT: Calculate pattern similarity using fuzzy matching
 * WHY:  Determine receptor-epitope binding strength for threat recognition
 * HOW:  Multi-factor scoring: exact matches, partial matches, bit similarity
 *
 * BIOLOGICAL BASIS:
 * Real antibodies don't require exact epitope matches. They can recognize
 * mutated/variant antigens with reduced but non-zero affinity. This enables
 * cross-reactive immunity against threat variants.
 *
 * SCORING COMPONENTS:
 * 1. Exact byte matches (weight: 0.5) - strongest signal
 * 2. Bit similarity for non-exact bytes (weight: 0.3) - catches mutations
 * 3. Length similarity penalty (weight: 0.2) - size matters
 */
float brain_immune_compute_affinity(
    const uint8_t* pattern1,
    size_t len1,
    const uint8_t* pattern2,
    size_t len2
) {
    if (!pattern1 || !pattern2 || len1 == 0 || len2 == 0) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_compute_affinity", 0.0f);


    size_t min_len = (len1 < len2) ? len1 : len2;
    size_t max_len = (len1 > len2) ? len1 : len2;

    /* Component 1: Exact byte matches */
    size_t exact_matches = 0;
    for (size_t i = 0; i < min_len; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && min_len > 256) {
            brain_immune_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)min_len);
        }

        if (pattern1[i] == pattern2[i]) {
            exact_matches++;
        }
    }
    float exact_score = (float)exact_matches / (float)max_len;

    /* Component 2: Bit similarity for non-exact bytes (detects mutations) */
    size_t total_bits = min_len * 8;
    size_t matching_bits = 0;
    for (size_t i = 0; i < min_len; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && min_len > 256) {
            brain_immune_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)min_len);
        }

        uint8_t xor_result = pattern1[i] ^ pattern2[i];
        /* Count matching bits (bits that are NOT different) */
        for (int bit = 0; bit < 8; bit++) {
            /* Phase 8: Loop progress heartbeat */
            if ((bit & 0xFF) == 0 && 8 > 256) {
                brain_immune_heartbeat("brain_immune_loop",
                                 (float)(bit + 1) / (float)8);
            }

            if (!(xor_result & (1 << bit))) {
                matching_bits++;
            }
        }
    }
    float bit_score = (total_bits > 0) ? (float)matching_bits / (float)total_bits : 0.0f;

    /* Component 3: Length similarity (penalize size mismatch) */
    float length_score = (float)min_len / (float)max_len;

    /* Weighted combination */
    float affinity = (0.5f * exact_score) + (0.3f * bit_score) + (0.2f * length_score);

    /* Ensure result is in [0, 1] range */
    if (affinity > 1.0f) affinity = 1.0f;
    if (affinity < 0.0f) affinity = 0.0f;

    return affinity;
}


/**
 * @brief Send imagination modulation based on current inflammation
 *
 * WHAT: Public API to send imagination modulation message
 * WHY:  Allow external triggers to update imagination engine
 * HOW:  Lock mutex, compute modulation, send via bio-async
 *
 * @param system Brain immune system
 * @return 0 on success, -1 on error
 */
int brain_immune_send_imagination_modulation(brain_immune_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_send_imagination_modulation: system is NULL");
        return -1;
    }
    if (!system->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_send_imagination_modulation: system->mutex is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_send_imagination_mod", 0.0f);


    nimcp_mutex_lock(system->mutex);
    send_imagination_modulation_unlocked(system);
    nimcp_mutex_unlock(system->mutex);

    return 0;
}


/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Query KG for module self-awareness information
 * WHY:  Enable introspective self-knowledge about brain immune system
 * HOW:  Look up entity and relations in KG
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 otherwise
 */
int brain_immune_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Brain_Immune_System");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                brain_immune_heartbeat("brain_immune_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Brain immune system self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Brain_Immune_System");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Brain_Immune_System");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

int brain_immune_present_exception(
    brain_immune_system_t* system,
    const nimcp_exception_t* exception,
    uint32_t* antigen_id_out
) {
    if (!system || !exception) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_present_exception: required parameter is NULL (system, exception)");
        return -1;
    }

    /* Use direct field access now that we include nimcp_exception.h */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_present_exception", 0.0f);


    uint32_t severity = (uint32_t)exception->severity;

    /* Get epitope directly from exception struct */
    const uint8_t* epitope = exception->epitope;
    size_t epitope_len = exception->epitope_len;

    /* If epitope not computed, use minimal epitope from error code */
    uint8_t fallback_epitope[NIMCP_EXCEPTION_EPITOPE_SIZE];
    if (epitope_len == 0) {
        memset(fallback_epitope, 0, sizeof(fallback_epitope));
        memcpy(fallback_epitope, &exception->code, sizeof(exception->code));
        memcpy(fallback_epitope + 4, &exception->category, sizeof(exception->category));
        memcpy(fallback_epitope + 8, &exception->severity, sizeof(exception->severity));
        epitope = fallback_epitope;
        epitope_len = 12;
    }

    /* Validate and clamp severity to 1-10 range */
    uint32_t immune_severity = severity;
    if (immune_severity > 10) immune_severity = 10;
    if (immune_severity < 1) immune_severity = 1;

    /* Present as generic antigen */
    int result = brain_immune_present_antigen(
        system,
        ANTIGEN_SOURCE_MANUAL,  /* Exceptions are manually presented */
        epitope,
        epitope_len,
        immune_severity,
        0,  /* source_node = 0 for exceptions */
        antigen_id_out
    );

    /* Trigger exception callback if registered */
    if (result == 0 && g_exception_callback && antigen_id_out) {
        g_exception_callback(system, exception, *antigen_id_out, g_exception_callback_data);
    }

    return result;
}


/**
 * @brief Notify immune system of recovery result
 */
int brain_immune_notify_recovery_result(
    brain_immune_system_t* system,
    uint32_t antigen_id,
    int recovery_action,
    bool success
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_notify_recovery_result: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_heartbeat("brain_immune_notify_recovery_resu", 0.0f);


    LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME,
        "Recovery result: antigen=%u, action=%d, success=%s",
        antigen_id, recovery_action, success ? "true" : "false");

    nimcp_mutex_lock(system->mutex);

    /* Find antigen */
    brain_antigen_t* antigen = find_antigen_by_id(system, antigen_id);
    if (antigen) {
        if (success) {
            antigen->neutralized = true;
            system->stats.threats_neutralized++;

            /* Release anti-inflammatory cytokine on success */
            nimcp_mutex_unlock(system->mutex);
            uint32_t cytokine_id = 0;
            brain_immune_release_cytokine(system, BRAIN_CYTOKINE_IL10, 0, 0.5f, 0, &cytokine_id);
        } else {
            nimcp_mutex_unlock(system->mutex);
        }
    } else {
        nimcp_mutex_unlock(system->mutex);
    }

    /* Trigger callback */
    if (g_recovery_callback) {
        g_recovery_callback(system, antigen_id, recovery_action, success, g_recovery_callback_data);
    }

    return 0;
}


/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int brain_immune_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "brain_immune_training_begin: NULL argument");
        return -1;
    }
    brain_immune_heartbeat_instance(NULL, "brain_immune_training_begin", 0.0f);
    return 0;
}


int brain_immune_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "brain_immune_training_end: NULL argument");
        return -1;
    }
    brain_immune_heartbeat_instance(NULL, "brain_immune_training_end", 1.0f);
    return 0;
}
