// nimcp_collective_cognition_part_processing.c - processing functions
// Part of nimcp_collective_cognition.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_collective_cognition.c


/*=============================================================================
 * Update API
 *===========================================================================*/

int collective_cognition_update(collective_cognition_t* cc) {
    if (!cc || !cc->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_cognition_update: required parameter is NULL (cc, cc->initialized)");
        return -1;
    }

    /* Update all subsystem states */
    /* Phase 8: Heartbeat at operation start */
    collective_cognition_heartbeat("collective_c_update", 0.0f);


    update_hyperscanning_state(cc);
    compute_collective_phi(cc);
    update_we_mode_state(cc);
    update_extended_mind_state(cc);

    /* Update aggregate state */
    update_aggregate_state(cc);

    /* Update statistics */
    cc->stats.total_updates++;
    cc->last_update_us = get_timestamp_us();

    /* Simulate phase evolution for testing */
    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            collective_cognition_heartbeat("collective_c_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        if (cc->instances[i].active) {
            for (int b = 0; b < SYNC_BAND_COUNT; b++) {
                /* Phase 8: Loop progress heartbeat */
                if ((b & 0xFF) == 0 && SYNC_BAND_COUNT > 256) {
                    collective_cognition_heartbeat("collective_c_loop",
                                     (float)(b + 1) / (float)SYNC_BAND_COUNT);
                }

                /* Phase drift with some coupling */
                float base_freq = 2.0f + b * 5.0f;  /* Different freq per band */
                cc->instances[i].band_phase[b] += base_freq * 0.01f;
                if (cc->instances[i].band_phase[b] > 6.28f) {
                    cc->instances[i].band_phase[b] -= 6.28f;
                }
            }
        }
    }

    return 0;
}


int collective_cognition_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "collective_cognition_training_step: NULL argument");
        return -1;
    }

    /* Clamp progress to [0, 1] */
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    collective_cognition_heartbeat_instance(g_collective_cognition_instance_health_agent, "coll_cog_train_step", progress);

    collective_cognition_t* ctx = (collective_cognition_t*)instance;

    g_collective_cognition_training_steps++;

    /* Update module stats if context is valid */
    if (ctx) {
        ctx->stats.total_updates++;
    }

    /* Progressive adaptation: decay error accumulator */
    float decay = 1.0f - 0.1f * progress;
    if (decay < 0.5f) decay = 0.5f;
    g_collective_cognition_training_total_error *= (double)decay;

    /* Adaptive threshold adjustment based on progress */
    float threshold_adjust = 0.01f * progress;
    g_collective_cognition_training_best_error -= (double)threshold_adjust;
    if (g_collective_cognition_training_best_error < 0.0) g_collective_cognition_training_best_error = 0.0;

    return 0;
}
