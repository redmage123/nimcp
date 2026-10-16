// nimcp_collective_cognition_part_helpers.c - helpers functions
// Part of nimcp_collective_cognition.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_collective_cognition.c


/*=============================================================================
 * Helper Functions - Time
 *===========================================================================*/

/**
 * @brief Get current wall-clock timestamp in microseconds
 *
 * Uses nimcp_time_get_us() for actual wall-clock time instead of
 * a monotonic counter. Handles potential overflow by using the full
 * 64-bit range which won't overflow for ~584,000 years from epoch.
 */
static uint64_t get_timestamp_us(void) {
    return nimcp_time_get_us();
}


/*=============================================================================
 * Helper Functions - Instance Management
 *===========================================================================*/

static registered_instance_t* find_instance(
    collective_cognition_t* cc,
    uint32_t instance_id
) {
    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            collective_cognition_heartbeat("collective_c_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        if (cc->instances[i].active && cc->instances[i].instance_id == instance_id) {
            return &cc->instances[i];
        }
    }
    return NULL;  /* Not found is normal */
}


static int find_instance_index(
    const collective_cognition_t* cc,
    uint32_t instance_id
) {
    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            collective_cognition_heartbeat("collective_c_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        if (cc->instances[i].active && cc->instances[i].instance_id == instance_id) {
            return (int)i;
        }
    }
    return -1;  /* Not found is normal */
}


/*=============================================================================
 * Helper Functions - Phi Computation
 *===========================================================================*/

/**
 * @brief Compute phase-locking value between two instances for a given band
 */
static float compute_plv(
    const registered_instance_t* a,
    const registered_instance_t* b,
    sync_band_t band
) {
    /* Bounds check on band index */
    if ((int)band < 0 || (int)band >= SYNC_BAND_COUNT) {
        return 0.0f;
    }
    /* PLV = |mean(exp(i * (phase_a - phase_b)))| */
    float phase_diff = a->band_phase[band] - b->band_phase[band];
    /* Simplified: use cosine of phase difference as proxy */
    return fabsf(cosf(phase_diff));
}


/**
 * @brief Compute global synchronization across all instances
 */
static float compute_global_sync(collective_cognition_t* cc) {
    if (cc->instance_count < 2) return 0.0f;

    float total_sync = 0.0f;
    uint32_t pair_count = 0;

    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            collective_cognition_heartbeat("collective_c_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        if (!cc->instances[i].active) continue;

        for (uint32_t j = i + 1; j < COLLECTIVE_MAX_INSTANCES; j++) {
            if (!cc->instances[j].active) continue;

            /* Compute average PLV across bands */
            float pair_sync = 0.0f;
            for (int b = 0; b < SYNC_BAND_COUNT; b++) {
                /* Phase 8: Loop progress heartbeat */
                if ((b & 0xFF) == 0 && SYNC_BAND_COUNT > 256) {
                    collective_cognition_heartbeat("collective_c_loop",
                                     (float)(b + 1) / (float)SYNC_BAND_COUNT);
                }

                float plv = compute_plv(&cc->instances[i], &cc->instances[j], (sync_band_t)b);
                cc->pair_plv[i][j][b] = plv;
                cc->pair_plv[j][i][b] = plv;
                pair_sync += plv;
            }
            pair_sync /= SYNC_BAND_COUNT;
            total_sync += pair_sync;
            pair_count++;
        }
    }

    return pair_count > 0 ? total_sync / pair_count : 0.0f;
}


/**
 * @brief Compute collective phi (integrated information)
 */
static void compute_collective_phi(collective_cognition_t* cc) {
    collective_phi_t* phi = &cc->state.phi;

    /* Sum local phis */
    phi->phi_local = 0.0f;
    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            collective_cognition_heartbeat("collective_c_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        if (cc->instances[i].active) {
            phi->phi_local += cc->instances[i].local_phi;
        }
    }

    /* Network phi based on synchronization */
    float global_sync = cc->state.hyperscanning.global_sync;
    phi->phi_network = global_sync * global_sync * cc->instance_count;

    /* Total phi with synergy coefficient */
    float synergy = cc->config.phi.synergy_coefficient;
    phi->phi_total = phi->phi_local + phi->phi_network * synergy;

    /* IIT decomposition (simplified) */
    phi->information = phi->phi_local;
    phi->integration = phi->phi_network;
    phi->exclusion = cc->instance_count > 1 ? 1.0f : 0.0f;

    /* Network topology metrics */
    phi->connectivity = cc->instance_count > 1 ?
        (float)(cc->instance_count - 1) / cc->instance_count : 0.0f;
    phi->modularity = 1.0f - global_sync;  /* Higher sync = lower modularity */
    phi->small_world_index = global_sync * phi->connectivity;

    /* Update statistics */
    if (isfinite(phi->phi_total) && cc->stats.total_updates < UINT32_MAX) {
        cc->stats.avg_phi = (cc->stats.avg_phi * cc->stats.total_updates + phi->phi_total) /
                            (cc->stats.total_updates + 1);
    }
    if (phi->phi_total > cc->stats.max_phi) {
        cc->stats.max_phi = phi->phi_total;
    }
}


/**
 * @brief Determine consciousness level from phi
 */
static collective_consciousness_level_t phi_to_level(float phi) {
    if (phi < 0.1f) return COLLECTIVE_CONSCIOUSNESS_NONE;
    if (phi < 0.3f) return COLLECTIVE_CONSCIOUSNESS_MINIMAL;
    if (phi < 0.5f) return COLLECTIVE_CONSCIOUSNESS_EMERGING;
    if (phi < 0.7f) return COLLECTIVE_CONSCIOUSNESS_PARTIAL;
    if (phi < 0.9f) return COLLECTIVE_CONSCIOUSNESS_UNIFIED;
    return COLLECTIVE_CONSCIOUSNESS_TRANSCENDENT;
}


/*=============================================================================
 * Helper Functions - Hyperscanning
 *===========================================================================*/

static void update_hyperscanning_state(collective_cognition_t* cc) {
    hyperscan_state_t* hs = &cc->state.hyperscanning;

    /* Compute global sync */
    hs->global_sync = compute_global_sync(cc);

    /* Compute band-specific sync */
    float gamma_total = 0.0f, theta_total = 0.0f, beta_total = 0.0f;
    uint32_t pair_count = 0;

    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            collective_cognition_heartbeat("collective_c_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        if (!cc->instances[i].active) continue;
        for (uint32_t j = i + 1; j < COLLECTIVE_MAX_INSTANCES; j++) {
            if (!cc->instances[j].active) continue;

            gamma_total += cc->pair_plv[i][j][SYNC_BAND_GAMMA];
            theta_total += cc->pair_plv[i][j][SYNC_BAND_THETA];
            beta_total += cc->pair_plv[i][j][SYNC_BAND_BETA];
            pair_count++;
        }
    }

    if (pair_count > 0) {
        hs->gamma_binding = gamma_total / pair_count;
        hs->theta_emotional = theta_total / pair_count;
        hs->beta_coordination = beta_total / pair_count;
    } else {
        hs->gamma_binding = 0.0f;
        hs->theta_emotional = 0.0f;
        hs->beta_coordination = 0.0f;
    }

    /* Check for entrainment — save old value before overwriting */
    bool was_entrained = hs->is_entrained;
    hs->is_entrained = hs->global_sync >= cc->config.hyperscanning.sync_threshold;

    if (hs->is_entrained && !was_entrained) {
        cc->stats.entrainment_events++;
    }

    /* Find leader (highest gamma power) */
    float max_gamma = -1.0f;
    uint32_t leader_id = 0;
    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            collective_cognition_heartbeat("collective_c_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        if (cc->instances[i].active &&
            cc->instances[i].band_power[SYNC_BAND_GAMMA] > max_gamma) {
            max_gamma = cc->instances[i].band_power[SYNC_BAND_GAMMA];
            leader_id = cc->instances[i].instance_id;
        }
    }
    hs->leader_instance_id = leader_id;
    hs->leader_influence = max_gamma > 0.0f ? max_gamma : 0.0f;

    /* Update average sync statistic */
    if (isfinite(hs->global_sync) && cc->stats.total_updates < UINT32_MAX) {
        cc->stats.avg_sync = (cc->stats.avg_sync * cc->stats.total_updates + hs->global_sync) /
                             (cc->stats.total_updates + 1);
    }
}


/*=============================================================================
 * Helper Functions - We-Mode
 *===========================================================================*/

static void update_we_mode_state(collective_cognition_t* cc) {
    we_mode_state_t* wm = &cc->state.we_mode;

    /* Query actual shared_intentionality subsystem if available */
    if (cc->intentionality) {
        /* Update the subsystem first to refresh its internal state */
        shared_intentionality_update((shared_intentionality_t*)cc->intentionality);
        /* Then get the updated state */
        shared_intentionality_get_we_mode(
            (shared_intentionality_t*)cc->intentionality,
            wm
        );
    }

    /* Overlay hyperscanning-derived metrics if not set by subsystem */
    float sync = cc->state.hyperscanning.global_sync;
    float phi_norm = cc->state.phi.phi_total /
        (cc->instance_count > 0 ? cc->instance_count : 1.0f);

    /* Blend we-mode strength with sync and phi if we have active instances */
    if (cc->instance_count >= 2) {
        float blended_strength = (wm->we_mode_strength + sync + phi_norm) / 3.0f;
        if (blended_strength > wm->we_mode_strength) {
            wm->we_mode_strength = blended_strength;
        }
    }

    /* Update joint commitment from sync if not set */
    if (wm->joint_commitment < sync) {
        wm->joint_commitment = sync;
    }

    /* Use hyperscanning metrics as fallback */
    if (wm->mutual_responsiveness == 0.0f) {
        wm->mutual_responsiveness = cc->state.hyperscanning.theta_emotional;
    }
    if (wm->role_understanding == 0.0f) {
        wm->role_understanding = cc->state.hyperscanning.beta_coordination;
    }
}


/*=============================================================================
 * Helper Functions - Extended Mind
 *===========================================================================*/

static void update_extended_mind_state(collective_cognition_t* cc) {
    extended_mind_state_t* em = &cc->state.extended_mind;

    /* Query actual extended_mind subsystem if available */
    if (cc->extended_mind) {
        /* Update the subsystem first to refresh its internal state */
        extended_mind_update((extended_mind_t*)cc->extended_mind);
        /* Then get the updated state */
        extended_mind_get_state((extended_mind_t*)cc->extended_mind, em);
    } else {
        /* Fallback defaults if subsystem not available */
        em->total_cognitive_capacity = 1.0f + (cc->instance_count * 0.1f);
        em->extended_ratio = 0.0f;
        em->integration_quality = 0.0f;
        em->active_extensions = 0;
        em->degraded_extensions = 0;
    }
}


/*=============================================================================
 * Helper Functions - Aggregate State
 *===========================================================================*/

static void update_aggregate_state(collective_cognition_t* cc) {
    /* Collective capacity */
    cc->state.collective_capacity =
        cc->state.extended_mind.total_cognitive_capacity +
        (cc->state.hyperscanning.global_sync * cc->instance_count * 0.1f);

    /* Integration quality */
    cc->state.integration_quality =
        (cc->state.hyperscanning.global_sync +
         cc->state.phi.integration +
         cc->state.we_mode.we_mode_strength) / 3.0f;

    /* Overall consciousness level [0-1] */
    cc->state.consciousness_level = cc->state.phi.phi_total;
    if (cc->state.consciousness_level > 1.0f) {
        cc->state.consciousness_level = 1.0f;
    }

    /* Flow metrics */
    cc->state.information_flow_rate =
        cc->state.phi.information * cc->instance_count * 10.0f;  /* bits/sec estimate */
    cc->state.attention_coherence = cc->state.hyperscanning.gamma_binding;
    cc->state.goal_alignment = cc->state.we_mode.joint_commitment;

    /* Health indicators */
    cc->state.is_fragmented = cc->state.integration_quality <
                              cc->config.fragmentation_threshold;
    cc->state.is_overloaded = cc->state.collective_capacity > cc->config.overload_threshold;
    cc->state.active_instances = cc->instance_count;

    /* Update event stats */
    if (cc->state.is_fragmented) {
        cc->stats.fragmentation_events++;
    }
    if (cc->state.is_overloaded) {
        cc->stats.overload_events++;
    }

    cc->state.last_update_us = get_timestamp_us();
}
