// nimcp_neuromodulators_part_accessors.c - accessors functions
// Part of nimcp_neuromodulators.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_neuromodulators.c


//=============================================================================
// Concentration Getters and Setters
//=============================================================================

bool neuromodulator_get_levels(neuromodulator_system_t system, neuromodulator_pool_t* pool) {
    /* WHAT: Get current neuromodulator concentrations (thread-safe read)
     * WHY:  Read-only access to system state with concurrent access support
     * COMPLEXITY: O(1)
     * THREAD SAFETY: Acquires read lock (allows parallel readers)
     * PERFORMANCE: Multiple threads can read simultaneously
     */
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_get_levels: system is NULL");
        return false;
    }
    if (!pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_get_levels: pool is NULL");
        return false;
    }

    /* Ensure pool has tensor storage */
    if (!pool->concentrations) {
        *pool = neuromodulator_pool_create();
    }

    /* WHAT: Acquire read lock for consistent snapshot using NIMCP platform
     * WHY:  Prevent reading while update() is modifying concentrations
     * PERFORMANCE: Read lock allows multiple concurrent readers
     * PATTERN: RAII-style (lock, copy, unlock)
     * USES: NIMCP platform rwlock for cross-platform compatibility
     */
    nimcp_platform_rwlock_rdlock(&system->rwlock);

    /* WHAT: Copy concentrations to output tensor
     * WHY:  Prevents direct manipulation of internal state
     * NOTE: Copy happens under lock for consistency
     */
    neuromodulator_pool_set_dopamine(pool, system->concentrations[NEUROMOD_DOPAMINE]);
    neuromodulator_pool_set_serotonin(pool, system->concentrations[NEUROMOD_SEROTONIN]);
    neuromodulator_pool_set_acetylcholine(pool, system->concentrations[NEUROMOD_ACETYLCHOLINE]);
    neuromodulator_pool_set_norepinephrine(pool, system->concentrations[NEUROMOD_NOREPINEPHRINE]);
    neuromodulator_pool_set_gaba(pool, system->concentrations[NEUROMOD_GABA]);
    neuromodulator_pool_set_glutamate(pool, system->concentrations[NEUROMOD_GLUTAMATE]);

    /* WHAT: Copy decay rates to tensor
     * WHY:  Useful for computing time-to-baseline
     * NOTE: These are read-only after initialization, but included for completeness
     */
    for (uint32_t i = 0; i < NEUROMOD_COUNT; i++) {
        uint32_t idx[1] = {i};
        nimcp_tensor_set(pool->decay_rates, idx, system->decay_times[i]);
    }
    pool->last_update = system->last_update_time;

    /* WHAT: Release read lock using NIMCP platform
     * WHY:  Allow other threads to access
     * CORRECTNESS: Always unlock before return
     * USES: NIMCP platform rwlock for cross-platform compatibility
     */
    nimcp_platform_rwlock_rdunlock(&system->rwlock);

    return true;
}


float neuromodulator_get_level(neuromodulator_system_t system, neuromodulator_type_t type) {
    /* WHAT: Get current level of a single neuromodulator
     * WHY:  Convenience function for querying one concentration
     * COMPLEXITY: O(1)
     */

    /* WHAT: Guard clause - validate system
     * WHY:  Prevent null pointer dereference
     */
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_get_level: system is NULL");
        return 0.0F;
    }

    /* WHAT: Guard clause - validate type
     * WHY:  Prevent array out of bounds access
     */
    if (type >= NEUROMOD_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromodulator_get_level: invalid type");
        return 0.0F;
    }

    /* WHAT: Return current concentration
     * WHY:  Direct access to concentration array
     */
    return system->concentrations[type];
}


bool neuromodulator_set_level(neuromodulator_system_t system, neuromodulator_type_t type,
                              float level) {
    /* WHAT: Directly set neuromodulator concentration
     * WHY:  For testing, manual control, or external simulation
     * COMPLEXITY: O(1)
     */
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_set_level: system is NULL");
        return false;
    }
    if (type >= NEUROMOD_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromodulator_set_level: invalid type");
        return false;
    }

    /* WHAT: Clamp to [0, 1] range
     * WHY:  Biological constraint - concentrations cannot be negative
     *       and receptors saturate at high levels
     */
    system->concentrations[type] = nimcp_clampf(level, MIN_CONCENTRATION, MAX_CONCENTRATION);

    return true;
}


float neuromodulator_get_learning_weight(neuromodulator_system_t system,
                                        const receptor_profile_t* receptors) {
    /* WHAT: Compute effective learning weight from current neuromodulator state
     * WHY:  Converts neuromodulator levels to single training weight
     * HOW:  Weighted combination matching biological effects
     *
     * FORMULA:
     * weight = 0.3 × DA (reward) +
     *          0.3 × ACh (attention) +
     *          0.2 × (1 - 5-HT) (not inhibited) +
     *          0.2 × NE (aroused)
     *
     * COMPLEXITY: O(1)
     */
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_get_learning_weight: system is NULL");
        return 0.5F;
    }
    if (!receptors) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_get_learning_weight: receptors is NULL");
        return 0.5F;  // Default moderate weight
    }

    float da = system->concentrations[NEUROMOD_DOPAMINE];
    float ach = system->concentrations[NEUROMOD_ACETYLCHOLINE];
    float serotonin = system->concentrations[NEUROMOD_SEROTONIN];
    float ne = system->concentrations[NEUROMOD_NOREPINEPHRINE];

    /* WHAT: Weight by receptor expression
     * WHY:  Same global concentration affects neurons differently
     */
    float da_effect = da * receptor_profile_get_d1_density(receptors);
    float ach_effect = ach * receptor_profile_get_nicotinic_density(receptors);
    float serotonin_effect = serotonin * receptor_profile_get_serotonin_density(receptors);
    float ne_effect = ne * receptor_profile_get_beta_density(receptors);

    float weight =
        0.3F * da_effect +                    // Reward enhances learning
        0.3F * ach_effect +                   // Attention enhances encoding
        0.2F * (1.0F - serotonin_effect) +    // Inhibition suppresses learning
        0.2F * ne_effect;                     // Arousal enhances consolidation

    return nimcp_clampf(weight, 0.0F, 1.0F);
}


//=============================================================================
// Statistics and Monitoring
//=============================================================================

bool neuromodulator_get_stats(neuromodulator_system_t system, neuromodulator_stats_t* stats) {
    /* WHAT: Get system statistics
     * WHY:  Monitor performance, debug, analyze behavior
     * COMPLEXITY: O(1)
     */
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_get_stats: system is NULL");
        return false;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_get_stats: stats is NULL");
        return false;
    }

    stats->current_dopamine = system->concentrations[NEUROMOD_DOPAMINE];
    stats->current_serotonin = system->concentrations[NEUROMOD_SEROTONIN];
    stats->current_acetylcholine = system->concentrations[NEUROMOD_ACETYLCHOLINE];
    stats->current_norepinephrine = system->concentrations[NEUROMOD_NOREPINEPHRINE];

    stats->avg_dopamine = system->stats.moving_averages[NEUROMOD_DOPAMINE];
    stats->avg_serotonin = system->stats.moving_averages[NEUROMOD_SEROTONIN];
    stats->avg_acetylcholine = system->stats.moving_averages[NEUROMOD_ACETYLCHOLINE];
    stats->avg_norepinephrine = system->stats.moving_averages[NEUROMOD_NOREPINEPHRINE];

    stats->dopamine_releases = system->stats.release_counts[NEUROMOD_DOPAMINE];
    stats->serotonin_releases = system->stats.release_counts[NEUROMOD_SEROTONIN];
    stats->acetylcholine_releases = system->stats.release_counts[NEUROMOD_ACETYLCHOLINE];
    stats->norepinephrine_releases = system->stats.release_counts[NEUROMOD_NOREPINEPHRINE];

    stats->dopamine_variance = system->stats.variances[NEUROMOD_DOPAMINE];

    /* WHAT: Compute reward prediction accuracy
     * WHY:  Measure how well system predicts rewards
     * HOW:  Average absolute RPE (lower = better predictions)
     */
    if (system->stats.rpe_count > 0) {
        float avg_rpe_error = system->stats.reward_prediction_error_sum / system->stats.rpe_count;
        stats->reward_prediction_accuracy = 1.0F - nimcp_clampf(avg_rpe_error, 0.0F, 1.0F);
    } else {
        stats->reward_prediction_accuracy = 0.5F;  // Unknown
    }

    return true;
}


//=============================================================================
// Sleep Integration Functions
//=============================================================================

bool neuromodulator_set_sleep_state(neuromodulator_system_t system,
                                    sleep_state_t sleep_state) {
    /* WHAT: Set current sleep state for neuromodulator modulation
     * WHY:  Sleep states fundamentally alter neuromodulator release and decay
     * HOW:  Store state, used in next update to apply sleep-based modulation
     *
     * BIOLOGICAL: ACh, NE, 5-HT, DA all vary dramatically across sleep stages
     */

    /* Guard: Validate input */
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_set_sleep_state: system is NULL");
        return false;
    }

    /* Acquire write lock for state modification */
    nimcp_platform_rwlock_wrlock(&system->rwlock);

    system->current_sleep_state = sleep_state;

    nimcp_platform_rwlock_unlock(&system->rwlock);

    return true;
}


sleep_state_t neuromodulator_get_sleep_state(neuromodulator_system_t system) {
    /* WHAT: Get current sleep state
     * WHY:  Query what modulation is being applied
     */

    /* Guard: Validate input */
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_get_sleep_state: system is NULL");
        return SLEEP_STATE_AWAKE;
    }

    /* Acquire read lock for state query */
    nimcp_platform_rwlock_rdlock(&system->rwlock);

    sleep_state_t state = system->current_sleep_state;

    nimcp_platform_rwlock_unlock(&system->rwlock);

    return state;
}


float neuromodulator_pool_get_dopamine(const neuromodulator_pool_t* pool) {
    if (!pool) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_pool_get_dopamine: pool is NULL"); return 0.0f; }
    if (!pool->concentrations) return 0.0f;
    uint32_t idx[1] = {NEUROMOD_DOPAMINE};
    return (float)nimcp_tensor_get(pool->concentrations, idx);
}


float neuromodulator_pool_get_serotonin(const neuromodulator_pool_t* pool) {
    if (!pool) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_pool_get_serotonin: pool is NULL"); return 0.0f; }
    if (!pool->concentrations) return 0.0f;
    uint32_t idx[1] = {NEUROMOD_SEROTONIN};
    return (float)nimcp_tensor_get(pool->concentrations, idx);
}


float neuromodulator_pool_get_acetylcholine(const neuromodulator_pool_t* pool) {
    if (!pool) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_pool_get_acetylcholine: pool is NULL"); return 0.0f; }
    if (!pool->concentrations) return 0.0f;
    uint32_t idx[1] = {NEUROMOD_ACETYLCHOLINE};
    return (float)nimcp_tensor_get(pool->concentrations, idx);
}


float neuromodulator_pool_get_norepinephrine(const neuromodulator_pool_t* pool) {
    if (!pool) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_pool_get_norepinephrine: pool is NULL"); return 0.0f; }
    if (!pool->concentrations) return 0.0f;
    uint32_t idx[1] = {NEUROMOD_NOREPINEPHRINE};
    return (float)nimcp_tensor_get(pool->concentrations, idx);
}


float neuromodulator_pool_get_gaba(const neuromodulator_pool_t* pool) {
    if (!pool) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_pool_get_gaba: pool is NULL"); return 0.0f; }
    if (!pool->concentrations) return 0.0f;
    uint32_t idx[1] = {NEUROMOD_GABA};
    return (float)nimcp_tensor_get(pool->concentrations, idx);
}


float neuromodulator_pool_get_glutamate(const neuromodulator_pool_t* pool) {
    if (!pool) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_pool_get_glutamate: pool is NULL"); return 0.0f; }
    if (!pool->concentrations) return 0.0f;
    uint32_t idx[1] = {NEUROMOD_GLUTAMATE};
    return (float)nimcp_tensor_get(pool->concentrations, idx);
}


void neuromodulator_pool_set_dopamine(neuromodulator_pool_t* pool, float value) {
    if (!pool) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_pool_set_dopamine: pool is NULL"); return; }
    if (!pool->concentrations) return;
    uint32_t idx[1] = {NEUROMOD_DOPAMINE};
    nimcp_tensor_set(pool->concentrations, idx, value);
}


void neuromodulator_pool_set_serotonin(neuromodulator_pool_t* pool, float value) {
    if (!pool) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_pool_set_serotonin: pool is NULL"); return; }
    if (!pool->concentrations) return;
    uint32_t idx[1] = {NEUROMOD_SEROTONIN};
    nimcp_tensor_set(pool->concentrations, idx, value);
}


void neuromodulator_pool_set_acetylcholine(neuromodulator_pool_t* pool, float value) {
    if (!pool) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_pool_set_acetylcholine: pool is NULL"); return; }
    if (!pool->concentrations) return;
    uint32_t idx[1] = {NEUROMOD_ACETYLCHOLINE};
    nimcp_tensor_set(pool->concentrations, idx, value);
}


void neuromodulator_pool_set_norepinephrine(neuromodulator_pool_t* pool, float value) {
    if (!pool) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_pool_set_norepinephrine: pool is NULL"); return; }
    if (!pool->concentrations) return;
    uint32_t idx[1] = {NEUROMOD_NOREPINEPHRINE};
    nimcp_tensor_set(pool->concentrations, idx, value);
}


void neuromodulator_pool_set_gaba(neuromodulator_pool_t* pool, float value) {
    if (!pool) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_pool_set_gaba: pool is NULL"); return; }
    if (!pool->concentrations) return;
    uint32_t idx[1] = {NEUROMOD_GABA};
    nimcp_tensor_set(pool->concentrations, idx, value);
}


void neuromodulator_pool_set_glutamate(neuromodulator_pool_t* pool, float value) {
    if (!pool) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_pool_set_glutamate: pool is NULL"); return; }
    if (!pool->concentrations) return;
    uint32_t idx[1] = {NEUROMOD_GLUTAMATE};
    nimcp_tensor_set(pool->concentrations, idx, value);
}


float neuromodulator_pool_get_by_type(const neuromodulator_pool_t* pool, neuromodulator_type_t type) {
    if (!pool) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_pool_get_by_type: pool is NULL"); return 0.0f; }
    if (!pool->concentrations) return 0.0f;
    if (type >= NEUROMOD_COUNT) return 0.0f;
    uint32_t idx[1] = {(uint32_t)type};
    return (float)nimcp_tensor_get(pool->concentrations, idx);
}


void neuromodulator_pool_set_by_type(neuromodulator_pool_t* pool, neuromodulator_type_t type, float value) {
    if (!pool) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_pool_set_by_type: pool is NULL"); return; }
    if (!pool->concentrations) return;
    if (type >= NEUROMOD_COUNT) return;
    uint32_t idx[1] = {(uint32_t)type};
    nimcp_tensor_set(pool->concentrations, idx, value);
}


float receptor_profile_get_density(const receptor_profile_t* profile, receptor_type_t type) {
    if (!profile || !profile->densities) return 0.0f;
    if (type >= RECEPTOR_COUNT) return 0.0f;
    uint32_t idx[1] = {(uint32_t)type};
    return (float)nimcp_tensor_get(profile->densities, idx);
}


void receptor_profile_set_density(receptor_profile_t* profile, receptor_type_t type, float value) {
    if (!profile || !profile->densities) return;
    if (type >= RECEPTOR_COUNT) return;
    uint32_t idx[1] = {(uint32_t)type};
    nimcp_tensor_set(profile->densities, idx, value);
}


float receptor_profile_get_d1_density(const receptor_profile_t* profile) {
    return receptor_profile_get_density(profile, RECEPTOR_D1);
}


float receptor_profile_get_d2_density(const receptor_profile_t* profile) {
    return receptor_profile_get_density(profile, RECEPTOR_D2);
}


float receptor_profile_get_serotonin_density(const receptor_profile_t* profile) {
    return receptor_profile_get_density(profile, RECEPTOR_5HT1A);
}


float receptor_profile_get_nicotinic_density(const receptor_profile_t* profile) {
    return receptor_profile_get_density(profile, RECEPTOR_NICOTINIC);
}


float receptor_profile_get_alpha_density(const receptor_profile_t* profile) {
    return receptor_profile_get_density(profile, RECEPTOR_ALPHA1);
}


float receptor_profile_get_beta_density(const receptor_profile_t* profile) {
    return receptor_profile_get_density(profile, RECEPTOR_BETA);
}


void receptor_profile_set_d1_density(receptor_profile_t* profile, float value) {
    receptor_profile_set_density(profile, RECEPTOR_D1, value);
}


void receptor_profile_set_d2_density(receptor_profile_t* profile, float value) {
    receptor_profile_set_density(profile, RECEPTOR_D2, value);
}


void receptor_profile_set_serotonin_density(receptor_profile_t* profile, float value) {
    receptor_profile_set_density(profile, RECEPTOR_5HT1A, value);
}


void receptor_profile_set_nicotinic_density(receptor_profile_t* profile, float value) {
    receptor_profile_set_density(profile, RECEPTOR_NICOTINIC, value);
}


void receptor_profile_set_alpha_density(receptor_profile_t* profile, float value) {
    receptor_profile_set_density(profile, RECEPTOR_ALPHA1, value);
}


void receptor_profile_set_beta_density(receptor_profile_t* profile, float value) {
    receptor_profile_set_density(profile, RECEPTOR_BETA, value);
}


float modulation_effects_get_learning_rate_multiplier(const modulation_effects_t* effects) {
    if (!effects || !effects->effects) return 1.0f;
    uint32_t idx[1] = {MODULATION_EFFECT_LEARNING_RATE};
    return (float)nimcp_tensor_get(effects->effects, idx);
}


float modulation_effects_get_transmission_gain(const modulation_effects_t* effects) {
    if (!effects || !effects->effects) return 1.0f;
    uint32_t idx[1] = {MODULATION_EFFECT_TRANSMISSION};
    return (float)nimcp_tensor_get(effects->effects, idx);
}


float modulation_effects_get_excitability_shift(const modulation_effects_t* effects) {
    if (!effects || !effects->effects) return 0.0f;
    uint32_t idx[1] = {MODULATION_EFFECT_EXCITABILITY};
    return (float)nimcp_tensor_get(effects->effects, idx);
}


float modulation_effects_get_attention_weight(const modulation_effects_t* effects) {
    if (!effects || !effects->effects) return 0.5f;
    uint32_t idx[1] = {MODULATION_EFFECT_ATTENTION};
    return (float)nimcp_tensor_get(effects->effects, idx);
}


void modulation_effects_set_learning_rate_multiplier(modulation_effects_t* effects, float value) {
    if (!effects || !effects->effects) return;
    uint32_t idx[1] = {MODULATION_EFFECT_LEARNING_RATE};
    nimcp_tensor_set(effects->effects, idx, value);
}


void modulation_effects_set_transmission_gain(modulation_effects_t* effects, float value) {
    if (!effects || !effects->effects) return;
    uint32_t idx[1] = {MODULATION_EFFECT_TRANSMISSION};
    nimcp_tensor_set(effects->effects, idx, value);
}


void modulation_effects_set_excitability_shift(modulation_effects_t* effects, float value) {
    if (!effects || !effects->effects) return;
    uint32_t idx[1] = {MODULATION_EFFECT_EXCITABILITY};
    nimcp_tensor_set(effects->effects, idx, value);
}


void modulation_effects_set_attention_weight(modulation_effects_t* effects, float value) {
    if (!effects || !effects->effects) return;
    uint32_t idx[1] = {MODULATION_EFFECT_ATTENTION};
    nimcp_tensor_set(effects->effects, idx, value);
}
