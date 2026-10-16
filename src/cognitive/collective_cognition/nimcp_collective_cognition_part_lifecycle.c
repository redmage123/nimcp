// nimcp_collective_cognition_part_lifecycle.c - lifecycle functions
// Part of nimcp_collective_cognition.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_collective_cognition.c


static registered_instance_t* find_free_slot(collective_cognition_t* cc) {
    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            collective_cognition_heartbeat("collective_c_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        if (!cc->instances[i].active) {
            return &cc->instances[i];
        }
    }
    return NULL;  /* All slots occupied is normal */
}


/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

collective_cognition_t* collective_cognition_create(
    const collective_cognition_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    collective_cognition_heartbeat("collective_c_create", 0.0f);


    collective_cognition_t* cc = nimcp_malloc(sizeof(collective_cognition_t));
    if (!cc) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate cc");

        return NULL;

    }

    memset(cc, 0, sizeof(collective_cognition_t));

    /* Apply configuration */
    if (config) {
        cc->config = *config;
    } else {
        cc->config = collective_cognition_default_config();
    }

    /* Initialize instances */
    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            collective_cognition_heartbeat("collective_c_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        cc->instances[i].active = false;
        cc->instances[i].local_phi = 0.3f;  /* Default individual phi */
        cc->instances[i].atp_level = 1.0f;
        cc->instances[i].fatigue_level = 0.0f;

        /* Initialize band states */
        for (int b = 0; b < SYNC_BAND_COUNT; b++) {
            /* Phase 8: Loop progress heartbeat */
            if ((b & 0xFF) == 0 && SYNC_BAND_COUNT > 256) {
                collective_cognition_heartbeat("collective_c_loop",
                                 (float)(b + 1) / (float)SYNC_BAND_COUNT);
            }

            cc->instances[i].band_power[b] = 0.5f;
            cc->instances[i].band_phase[b] = 0.0f;
        }
    }

    /* Create subsystem handles */
    cc->hyperscanning = hyperscanning_create(&cc->config.hyperscanning);
    cc->extended_mind = extended_mind_create(&cc->config.extended_mind);
    cc->phi_system = collective_phi_create(&cc->config.phi);
    cc->intentionality = shared_intentionality_create(&cc->config.intentionality);

    /* Check for allocation failures - subsystems are optional but recommended */
    if (!cc->hyperscanning || !cc->extended_mind ||
        !cc->phi_system || !cc->intentionality) {
        /* Clean up any successfully created subsystems */
        if (cc->hyperscanning) hyperscanning_destroy(cc->hyperscanning);
        if (cc->extended_mind) extended_mind_destroy(cc->extended_mind);
        if (cc->phi_system) collective_phi_destroy(cc->phi_system);
        if (cc->intentionality) shared_intentionality_destroy(cc->intentionality);
        nimcp_free(cc);
        cc = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_cognition_create: validation failed");
        return NULL;
    }

    cc->initialized = true;
    cc->last_update_us = get_timestamp_us();

    return cc;
}


void collective_cognition_destroy(collective_cognition_t* cc) {
    if (!cc) return;

    /* Destroy subsystem handles */
    /* Phase 8: Heartbeat at operation start */
    collective_cognition_heartbeat("collective_c_destroy", 0.0f);


    if (cc->hyperscanning) hyperscanning_destroy(cc->hyperscanning);
    if (cc->extended_mind) extended_mind_destroy(cc->extended_mind);
    if (cc->phi_system) collective_phi_destroy(cc->phi_system);
    if (cc->intentionality) shared_intentionality_destroy(cc->intentionality);

    /* Disconnect bio-async if connected */
    if (cc->bio_async_connected) {
        collective_cognition_disconnect_bio_async(cc);
    }

    nimcp_free(cc);
    cc = NULL;
}


int collective_cognition_reset(collective_cognition_t* cc) {
    if (!cc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_cognition_reset: cc is NULL");
        return -1;
    }

    /* Reset all instances */
    /* Phase 8: Heartbeat at operation start */
    collective_cognition_heartbeat("collective_c_reset", 0.0f);


    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            collective_cognition_heartbeat("collective_c_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        cc->instances[i].active = false;
    }
    cc->instance_count = 0;

    /* Reset state */
    memset(&cc->state, 0, sizeof(cc->state));
    memset(&cc->stats, 0, sizeof(cc->stats));
    memset(cc->pair_plv, 0, sizeof(cc->pair_plv));

    cc->last_update_us = get_timestamp_us();

    return 0;
}


void collective_cognition_reset_stats(collective_cognition_t* cc) {
    if (!cc) return;

    /* Phase 8: Heartbeat at operation start */
    collective_cognition_heartbeat("collective_c_reset_stats", 0.0f);


    memset(&cc->stats, 0, sizeof(cc->stats));
}
