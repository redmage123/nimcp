// nimcp_neuromodulators_part_core.c - core functions
// Part of nimcp_neuromodulators.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_neuromodulators.c


/**
 * @brief Connect brain reference for medulla integration
 *
 * WHAT: Store brain reference for medulla arousal queries
 * WHY:  Medulla arousal modulates norepinephrine release
 * HOW:  Store reference, enable medulla integration
 *
 * BIOLOGICAL: Locus coeruleus NE release scales with brainstem arousal
 *
 * @param system Neuromodulator system
 * @param brain Brain reference
 * @return true on success
 */
bool neuromodulator_connect_brain(neuromodulator_system_t system, void* brain) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_connect_brain: system is NULL");
        return false;
    }

    nimcp_platform_rwlock_wrlock(&system->rwlock);
    system->brain_ref = brain;
    system->use_medulla_integration = (brain != NULL);
    nimcp_platform_rwlock_wrunlock(&system->rwlock);

    NIMCP_LOGGING_INFO("Neuromodulators connected to brain for medulla integration");
    return true;
}


//=============================================================================
// Event-Driven Release (Phasic Responses)
//=============================================================================

float neuromodulator_release_dopamine(neuromodulator_system_t system, float reward_magnitude,
                                     float predicted_reward) {
    /* WHAT: Release dopamine based on reward prediction error (RPE) - thread-safe
     * WHY:  Implements temporal difference learning signal
     * HOW:  δ = R - V(s), dopamine ∝ δ
     *
     * BIOLOGICAL: Phasic dopamine from VTA/SNc neurons
     * REFERENCE: Schultz et al. (1997) - "A neural substrate of prediction and reward"
     * COMPLEXITY: O(1)
     * THREAD SAFETY: Acquires write lock for concentration, atomics for stats
     *
     * @param system Neuromodulator system
     * @param reward_magnitude Actual reward received (0-1)
     * @param predicted_reward Expected reward (0-1)
     * @return Reward prediction error
     */
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_release_dopamine: system is NULL");
        return 0.0F;
    }

    /* WHAT: Compute reward prediction error (read-only, no lock needed)
     * WHY:  RPE drives learning - positive for better than expected,
     *       negative for worse than expected
     */
    float rpe = reward_magnitude - predicted_reward;

    /* WHAT: Acquire write lock for concentration modification
     * WHY:  Prevent race conditions when reading and updating concentration
     * PATTERN: Brief critical section (read-modify-write)
     */
    nimcp_platform_rwlock_wrlock(&system->rwlock);

    // ===========================================================================
    // PHASE C2.3: Vesicle-Based Release Modulation
    // ===========================================================================

    float vesicle_modulation = 1.0F;  // Default: no modulation

    if (system->use_vesicle_packaging) {
        /* WHAT: Use vesicle dynamics to modulate neurotransmitter release
         * WHY:  Models short-term plasticity (facilitation & depression)
         * HOW:  Vesicle availability scales the effective release amount
         */

        // Release vesicles (quantal release)
        bool action_potential = (fabsf(rpe) > 0.1F);  // Release if significant RPE
        uint64_t current_time = system->last_update_time;

        float molecules_released = vesicle_pool_release(&system->dopamine_vesicles,
                                                        action_potential,
                                                        current_time);

        // Update vesicle pools (refill, mobilize, facilitation decay)
        vesicle_pool_update(&system->dopamine_vesicles, 0.001F);  // 1ms time step

        // Vesicle modulation = actual release / expected release
        // Expected: ~3 vesicles at Pr=0.3, each ~5000 molecules = 15000 molecules
        float expected_molecules = VESICLE_DEFAULT_RRP_SIZE * VESICLE_DEFAULT_RELEASE_PROBABILITY * VESICLE_DEFAULT_QUANTAL_SIZE;
        vesicle_modulation = (expected_molecules > 0.0F) ? (molecules_released / expected_molecules) : 0.0F;

        // Clamp modulation to [0, 2] (can facilitate up to 2x)
        vesicle_modulation = nimcp_clampf(vesicle_modulation, 0.0F, 2.0F);
    }

    // ===========================================================================
    // PHASE C2.4: Metabolic Pathway Dynamics
    // ===========================================================================

    float metabolic_concentration = 0.0F;

    if (system->use_metabolic_pathways) {
        /* WHAT: Use metabolic dynamics for complete neurotransmitter lifecycle
         * WHY:  Models synthesis, degradation, and reuptake for biological realism
         * HOW:  Integrate enzyme kinetics and transporter dynamics
         */

        // Convert normalized vesicle modulation to µM release amount
        // Typical phasic burst: 1 µM in cleft
        float release_amount_um = vesicle_modulation * fabsf(rpe);

        // Update metabolic state (synthesis + release - degradation - reuptake)
        metabolic_concentration = metabolic_update(&system->dopamine_metabolism,
                                                   0.001F,  // 1ms time step
                                                   release_amount_um);
    }

    // ===========================================================================
    // PHASE E1: Grief System Integration (Cognitive Pipeline)
    // ===========================================================================

    float grief_dopamine_factor = 1.0F;  // Default: no grief effect

    if (system->use_grief_integration && system->grief_system) {
        /* WHAT: Apply grief-induced dopamine depletion
         * WHY:  Grief causes anhedonia via reduced dopamine (60% reduction typical)
         * HOW:  Query grief system for modulation factor, multiply dopamine release
         * BIOLOGICAL: Grief disrupts reward processing
         */
        float serotonin_factor, norepinephrine_factor;
        grief_get_neuromodulator_effects(system->grief_system,
                                        &serotonin_factor,
                                        &grief_dopamine_factor,
                                        &norepinephrine_factor);

        // Apply dopamine depletion to metabolic concentration
        if (system->use_metabolic_pathways) {
            metabolic_concentration *= grief_dopamine_factor;
        }
    }

    // ===========================================================================
    // PHASE E2: Joy and Euphoria System Integration
    // ===========================================================================

    float joy_dopamine_factor = 1.0F;  // Default: no joy effect

    if (system->use_joy_integration && system->joy_system) {
        /* WHAT: Apply joy-induced dopamine enhancement
         * WHY:  Joy/euphoria boost dopamine release (up to 2x enhancement typical)
         * HOW:  Query joy system for modulation factor, multiply dopamine release
         * BIOLOGICAL: Positive emotions enhance reward processing
         */
        float joy_serotonin_factor;
        joy_get_neuromodulator_effects(system->joy_system,
                                      &joy_dopamine_factor,
                                      &joy_serotonin_factor);

        // Apply dopamine enhancement to metabolic concentration
        if (system->use_metabolic_pathways) {
            metabolic_concentration *= joy_dopamine_factor;
        }
    }

    // ===========================================================================
    // PHASE E4: Social Bond System Integration
    // ===========================================================================

    float social_dopamine_factor = 1.0F;  // Default: no social effect
    float social_oxytocin_factor = 1.0F;  // Default: no social effect

    if (system->use_social_integration && system->social_system) {
        /* WHAT: Apply social bond effects on dopamine and oxytocin
         * WHY:  Love/friendship boost dopamine (reward) and oxytocin (bonding), loneliness reduces dopamine
         * HOW:  Query social system for modulation factors, multiply dopamine/oxytocin release
         * BIOLOGICAL: Social bonds enhance reward processing, loneliness causes anhedonia
         */
        social_get_neuromodulator_effects(system->social_system,
                                         &social_dopamine_factor,
                                         &social_oxytocin_factor);

        // Apply dopamine modulation to metabolic concentration
        if (system->use_metabolic_pathways) {
            metabolic_concentration *= social_dopamine_factor;
        }
    }

    // ===========================================================================
    // PHASE C2.2: Enhanced Phasic-Tonic Dynamics
    // ===========================================================================

    if (system->use_enhanced_dynamics) {
        /* WHAT: Use phasic-tonic encoding for biologically realistic TD error signaling
         * WHY:  Replaces simple concentration with burst/baseline separation
         * HOW:  Positive RPE → phasic burst, Negative RPE → tonic dip
         */

        // Normalize RPE to [-1, +1] range for encoding
        float td_error = nimcp_clampf(rpe, -1.0F, 1.0F);

        // Get current time (will use last_update_time, or 0 if first call)
        uint64_t current_time = system->last_update_time;

        // Encode TD error as phasic burst or tonic dip
        phasic_tonic_encode_td_error(&system->dopamine_phasic_tonic, td_error, current_time);

        // Get updated concentration from phasic-tonic system
        float da_concentration = phasic_tonic_get_concentration(&system->dopamine_phasic_tonic);

        // Apply vesicle modulation to concentration
        da_concentration *= vesicle_modulation;

        // Apply metabolic dynamics if enabled
        if (system->use_metabolic_pathways) {
            // Use metabolic concentration (includes clearance dynamics)
            da_concentration = metabolic_concentration;
        }

        // Normalize to [0, 1] range for compatibility with existing code
        // Convert from µM to normalized: 1 µM (peak burst) → 1.0
        system->concentrations[NEUROMOD_DOPAMINE] = nimcp_clampf(da_concentration * 1000.0F, 0.0F, 1.0F);

    } else {
        /* WHAT: Legacy simple concentration model (fallback)
         * WHY:  For compatibility or when enhanced dynamics not needed
         * HOW:  Direct RPE → concentration mapping (original behavior)
         */

        if (system->use_metabolic_pathways) {
            // Use metabolic concentration even in legacy mode
            system->concentrations[NEUROMOD_DOPAMINE] = nimcp_clampf(metabolic_concentration * 1000.0F,
                                                              MIN_CONCENTRATION,
                                                              MAX_CONCENTRATION);
        } else {
            // Original simple model (apply grief and joy factors)
            float dopamine_change = system->reward_dopamine_gain * rpe * vesicle_modulation *
                                   grief_dopamine_factor * joy_dopamine_factor;
            float new_dopamine = system->concentrations[NEUROMOD_DOPAMINE] + dopamine_change;
            system->concentrations[NEUROMOD_DOPAMINE] = nimcp_clampf(new_dopamine,
                                                              MIN_CONCENTRATION,
                                                              MAX_CONCENTRATION);
        }
    }

    /* WHAT: Update non-atomic statistics under lock
     * WHY:  reward_prediction_error_sum is not atomic (float)
     */
    system->stats.reward_prediction_error_sum += fabsf(rpe);

    /* WHAT: Release write lock before atomic increments
     * WHY:  Atomic operations don't need lock protection
     * PERFORMANCE: Minimize critical section duration
     */
    nimcp_platform_rwlock_wrunlock(&system->rwlock);

    /* WHAT: Atomically increment counters (lock-free)
     * WHY:  Track release frequency and RPE count without lock overhead
     * THREAD SAFETY: atomic_fetch_add is lock-free
     */
    atomic_fetch_add(&system->stats.release_counts[NEUROMOD_DOPAMINE], 1);
    atomic_fetch_add(&system->stats.rpe_count, 1);

    return rpe;
}


float neuromodulator_release_serotonin(neuromodulator_system_t system, float punishment_magnitude) {
    /* WHAT: Release serotonin in response to punishment/aversion - thread-safe
     * WHY:  5-HT promotes behavioral inhibition and patience
     * HOW:  Δ5-HT ∝ punishment magnitude
     *
     * BIOLOGICAL: Raphe nuclei response to aversive outcomes
     * REFERENCE: Dayan & Huys (2008) - "Serotonin, inhibition, and negative mood"
     * COMPLEXITY: O(1)
     * THREAD SAFETY: Write lock for concentration, atomic for counter
     */
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_release_serotonin: system is NULL");
        return 0.0F;
    }

    /* WHAT: Acquire write lock for concentration modification
     * WHY:  Prevent race conditions on read-modify-write
     */
    nimcp_platform_rwlock_wrlock(&system->rwlock);

    // Phase E1: Apply grief-induced serotonin depletion
    float grief_serotonin_factor = 1.0F;
    if (system->use_grief_integration && system->grief_system) {
        float dopamine_factor, norepinephrine_factor;
        grief_get_neuromodulator_effects(system->grief_system,
                                        &grief_serotonin_factor,
                                        &dopamine_factor,
                                        &norepinephrine_factor);
    }

    // Phase E2: Apply joy-induced serotonin enhancement
    float joy_serotonin_factor = 1.0F;
    if (system->use_joy_integration && system->joy_system) {
        float joy_dopamine_factor;
        joy_get_neuromodulator_effects(system->joy_system,
                                      &joy_dopamine_factor,
                                      &joy_serotonin_factor);
    }

    float serotonin_release = system->punishment_serotonin_gain * punishment_magnitude *
                             grief_serotonin_factor * joy_serotonin_factor;

    float new_serotonin = system->concentrations[NEUROMOD_SEROTONIN] + serotonin_release;
    system->concentrations[NEUROMOD_SEROTONIN] = nimcp_clampf(new_serotonin,
                                                       MIN_CONCENTRATION,
                                                       MAX_CONCENTRATION);

    /* WHAT: Release write lock before atomic increment
     * WHY:  Minimize critical section duration
     */
    nimcp_platform_rwlock_wrunlock(&system->rwlock);

    /* WHAT: Atomically increment release counter
     * WHY:  Lock-free statistics tracking
     */
    atomic_fetch_add(&system->stats.release_counts[NEUROMOD_SEROTONIN], 1);

    return serotonin_release;
}


float neuromodulator_release_acetylcholine(neuromodulator_system_t system, float salience) {
    /* WHAT: Release acetylcholine for salient/surprising stimuli - thread-safe
     * WHY:  ACh tags important information for encoding
     * HOW:  ΔACh ∝ salience = |actual - expected|
     *
     * BIOLOGICAL: Basal forebrain (NBM) response to prediction errors
     * REFERENCE: Yu & Dayan (2005) - "Uncertainty, neuromodulation, and attention"
     * COMPLEXITY: O(1)
     * THREAD SAFETY: Write lock for concentration, atomic for counter
     */
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_release_acetylcholine: system is NULL");
        return 0.0F;
    }

    /* WHAT: Acquire write lock for concentration modification
     * WHY:  Prevent race conditions on read-modify-write
     */
    nimcp_platform_rwlock_wrlock(&system->rwlock);

    float ach_release = system->salience_acetylcholine_gain * salience;

    float new_ach = system->concentrations[NEUROMOD_ACETYLCHOLINE] + ach_release;
    system->concentrations[NEUROMOD_ACETYLCHOLINE] = nimcp_clampf(new_ach,
                                                           MIN_CONCENTRATION,
                                                           MAX_CONCENTRATION);

    /* WHAT: Release write lock before atomic increment
     * WHY:  Minimize critical section duration
     */
    nimcp_platform_rwlock_wrunlock(&system->rwlock);

    /* WHAT: Atomically increment release counter
     * WHY:  Lock-free statistics tracking
     */
    atomic_fetch_add(&system->stats.release_counts[NEUROMOD_ACETYLCHOLINE], 1);

    return ach_release;
}


float neuromodulator_release_norepinephrine(neuromodulator_system_t system, float threat_level,
                                           float uncertainty) {
    /* WHAT: Release norepinephrine for threat and uncertainty - thread-safe
     * WHY:  NE increases vigilance, alertness, and stress response
     * HOW:  ΔNE ∝ threat + uncertainty
     *
     * BIOLOGICAL: Locus coeruleus activation during threat/novelty
     * REFERENCE: Aston-Jones & Cohen (2005) - "An integrative theory of locus coeruleus-NE"
     * COMPLEXITY: O(1)
     * THREAD SAFETY: Write lock for concentration, atomic for counter
     */
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_release_norepinephrine: system is NULL");
        return 0.0F;
    }

    /* WHAT: Combine threat and uncertainty (read-only, no lock needed)
     * WHY:  Both trigger arousal/vigilance response
     * WEIGHT: Threat weighted more heavily (0.7 vs 0.3)
     */
    float arousal_signal = 0.7F * threat_level + 0.3F * uncertainty;

    /* WHAT: Acquire write lock for concentration modification
     * WHY:  Prevent race conditions on read-modify-write
     */
    nimcp_platform_rwlock_wrlock(&system->rwlock);

    // Phase E1: Apply grief-induced norepinephrine elevation
    float grief_norepinephrine_factor = 1.0F;
    if (system->use_grief_integration && system->grief_system) {
        float serotonin_factor, dopamine_factor;
        grief_get_neuromodulator_effects(system->grief_system,
                                        &serotonin_factor,
                                        &dopamine_factor,
                                        &grief_norepinephrine_factor);
    }

    /* WHAT: Apply medulla arousal modulation
     * WHY:  Locus coeruleus NE release scales with brainstem arousal
     * HOW:  Query medulla arousal, multiply into release gain
     * BIOLOGICAL: High medulla arousal = heightened stress response
     */
    float medulla_arousal_factor = 1.0F;
    if (system->use_medulla_integration && system->brain_ref) {
        brain_t brain = (brain_t)system->brain_ref;
        float medulla_arousal = nimcp_brain_get_arousal_level(brain);
        /* Scale: arousal 0.5 (neutral) = 1.0x, arousal 1.0 = 1.5x, arousal 0.0 = 0.5x */
        medulla_arousal_factor = 0.5f + medulla_arousal;
    }

    float ne_release = system->threat_norepinephrine_gain * arousal_signal * grief_norepinephrine_factor * medulla_arousal_factor;

    float new_ne = system->concentrations[NEUROMOD_NOREPINEPHRINE] + ne_release;
    system->concentrations[NEUROMOD_NOREPINEPHRINE] = nimcp_clampf(new_ne,
                                                            MIN_CONCENTRATION,
                                                            MAX_CONCENTRATION);

    /* WHAT: Release write lock before atomic increment
     * WHY:  Minimize critical section duration
     */
    nimcp_platform_rwlock_wrunlock(&system->rwlock);

    /* WHAT: Atomically increment release counter
     * WHY:  Lock-free statistics tracking
     */
    atomic_fetch_add(&system->stats.release_counts[NEUROMOD_NOREPINEPHRINE], 1);

    return ne_release;
}


//=============================================================================
// Receptor-Mediated Effects (Local Modulation)
//=============================================================================

bool neuromodulator_compute_effects(neuromodulator_system_t system,
                                   const receptor_profile_t* receptors,
                                   modulation_effects_t* effects) {
    /* WHAT: Compute local modulation effects from global concentrations
     * WHY:  Different neurons respond differently based on receptor expression
     * HOW:  effect = global_concentration × receptor_density
     *
     * BIOLOGICAL: Receptor-mediated signal transduction
     * COMPLEXITY: O(1) - pure arithmetic, no loops
     *
     * PERFORMANCE: Called per synapse, must be extremely fast
     * OPTIMIZATION: All operations are floating-point, no branches
     */
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_compute_effects: system is NULL");
        return false;
    }
    if (!receptors) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_compute_effects: receptors is NULL");
        return false;
    }
    if (!effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_compute_effects: effects is NULL");
        return false;
    }

    /* WHAT: Get current global concentrations
     * WHY:  Source signal for local modulation
     */
    float da = system->concentrations[NEUROMOD_DOPAMINE];
    float serotonin = system->concentrations[NEUROMOD_SEROTONIN];
    float ach = system->concentrations[NEUROMOD_ACETYLCHOLINE];
    float ne = system->concentrations[NEUROMOD_NOREPINEPHRINE];

    /* WHAT: Compute learning rate multiplier
     * WHY:  Neuromodulators gate plasticity
     * HOW:  Weighted sum of modulatory influences
     *
     * FORMULA:
     * lr_mult = 1.0 +
     *           D1 × DA × 0.5 (enhances LTP) -
     *           D2 × DA × 0.3 (suppresses LTP) +
     *           nACh × ACh × 0.4 (enhances encoding) +
     *           β × NE × 0.3 (enhances consolidation) -
     *           5-HT × 0.2 (suppresses impulsive learning)
     *
     * BIOLOGICAL: Each receptor type has specific effect on plasticity
     */
    float d1_dens = receptor_profile_get_d1_density(receptors);
    float d2_dens = receptor_profile_get_d2_density(receptors);
    float nic_dens = receptor_profile_get_nicotinic_density(receptors);
    float beta_dens = receptor_profile_get_beta_density(receptors);
    float ser_dens = receptor_profile_get_serotonin_density(receptors);
    float alpha_dens = receptor_profile_get_alpha_density(receptors);

    float lr_mult = 1.0F +
        d1_dens * da * 0.5F -           // D1: Enhances plasticity
        d2_dens * da * 0.3F +           // D2: Suppresses plasticity
        nic_dens * ach * 0.4F +         // nACh: Attention/encoding
        beta_dens * ne * 0.3F -         // β: Arousal/consolidation
        ser_dens * serotonin * 0.2F;    // 5-HT: Inhibits

    /* WHAT: Clamp multiplier to reasonable range [0, 2]
     * WHY:  Prevent runaway plasticity or complete suppression
     */
    modulation_effects_set_learning_rate_multiplier(effects, nimcp_clampf(lr_mult, 0.0F, 2.0F));

    /* WHAT: Compute synaptic transmission gain
     * WHY:  Attention modulates signal amplification
     * HOW:  ACh + NE increase gain, 5-HT decreases
     *
     * FORMULA:
     * gain = 1.0 +
     *        ACh × nACh × 0.5 (attention amplification) +
     *        NE × α × 0.3 (arousal amplification) -
     *        5-HT × 0.2 (behavioral inhibition)
     */
    float t_gain = 1.0F +
        ach * nic_dens * 0.5F +
        ne * alpha_dens * 0.3F -
        serotonin * ser_dens * 0.2F;

    modulation_effects_set_transmission_gain(effects, nimcp_clampf(t_gain, 0.1F, 2.0F));

    /* WHAT: Compute excitability shift (threshold modulation)
     * WHY:  Arousal changes firing threshold
     * HOW:  NE lowers threshold (negative shift), 5-HT raises (positive shift)
     *
     * FORMULA:
     * shift = -NE × α × 0.3 (arousal → more excitable) +
     *         5-HT × 0.2 (inhibition → less excitable)
     *
     * RANGE: [-0.5, +0.5] to prevent extreme shifts
     */
    float e_shift =
        -ne * alpha_dens * 0.3F +       // Negative = lower threshold
        serotonin * ser_dens * 0.2F;    // Positive = raise threshold

    modulation_effects_set_excitability_shift(effects, nimcp_clampf(e_shift, -0.5F, 0.5F));

    /* WHAT: Compute attention weight
     * WHY:  Determines importance for working memory / consolidation
     * HOW:  Primarily ACh, with NE contribution
     *
     * FORMULA: attention = ACh × 0.7 + NE × 0.3
     */
    float att_weight =
        ach * nic_dens * 0.7F +
        ne * alpha_dens * 0.3F;

    modulation_effects_set_attention_weight(effects, nimcp_clampf(att_weight, 0.0F, 1.0F));

    return true;
}


float neuromodulator_modulate_learning_rate(float base_learning_rate,
                                           const modulation_effects_t* effects) {
    /* WHAT: Apply neuromodulation to learning rate
     * WHY:  Context-dependent plasticity (only learn when appropriate)
     * COMPLEXITY: O(1)
     */
    if (!effects) return base_learning_rate;

    return base_learning_rate * modulation_effects_get_learning_rate_multiplier(effects);
}


float neuromodulator_modulate_transmission(float base_weight, const modulation_effects_t* effects) {
    /* WHAT: Apply neuromodulation to synaptic strength
     * WHY:  Attention amplifies relevant signals
     * COMPLEXITY: O(1)
     */
    if (!effects) return base_weight;

    return base_weight * modulation_effects_get_transmission_gain(effects);
}


float neuromodulator_modulate_threshold(float base_threshold, const modulation_effects_t* effects) {
    /* WHAT: Apply neuromodulation to firing threshold
     * WHY:  Arousal changes excitability
     * COMPLEXITY: O(1)
     */
    if (!effects) return base_threshold;

    return base_threshold + modulation_effects_get_excitability_shift(effects);
}


//=============================================================================
// Receptor Profile Presets (Factory Pattern)
//=============================================================================

receptor_profile_t neuromodulator_profile_cortical_excitatory(void) {
    /* WHAT: Receptor profile for cortical pyramidal neurons
     * WHY:  Based on immunohistochemistry data from prefrontal cortex
     * BIOLOGICAL: Pyramidal cells express high D1, moderate ACh, some NE
     */
    receptor_profile_t profile = receptor_profile_create();

    receptor_profile_set_d1_density(&profile, 0.7F);          /* High D1 (enhances plasticity) */
    receptor_profile_set_d2_density(&profile, 0.2F);          /* Low D2 */
    receptor_profile_set_serotonin_density(&profile, 0.3F);   /* Moderate 5-HT */
    receptor_profile_set_nicotinic_density(&profile, 0.5F);   /* Moderate nACh (attention) */
    receptor_profile_set_alpha_density(&profile, 0.4F);       /* Moderate alpha (arousal) */
    receptor_profile_set_beta_density(&profile, 0.5F);        /* Moderate beta (consolidation) */

    return profile;
}


receptor_profile_t neuromodulator_profile_cortical_inhibitory(void) {
    /* WHAT: Receptor profile for cortical interneurons
     * WHY:  Parvalbumin+ interneurons have distinct receptor expression
     * BIOLOGICAL: High D2, moderate 5-HT, low ACh
     */
    receptor_profile_t profile = receptor_profile_create();

    receptor_profile_set_d1_density(&profile, 0.2F);
    receptor_profile_set_d2_density(&profile, 0.8F);          /* High D2 (suppresses plasticity) */
    receptor_profile_set_serotonin_density(&profile, 0.6F);   /* High 5-HT (inhibitory) */
    receptor_profile_set_nicotinic_density(&profile, 0.2F);   /* Low nACh */
    receptor_profile_set_alpha_density(&profile, 0.3F);
    receptor_profile_set_beta_density(&profile, 0.3F);

    return profile;
}


receptor_profile_t neuromodulator_profile_hippocampal(void) {
    /* WHAT: Receptor profile for hippocampal CA1/CA3 pyramidal neurons
     * WHY:  Hippocampus is critical for encoding, high ACh sensitivity
     * BIOLOGICAL: Very high nACh and mACh for memory formation
     */
    receptor_profile_t profile = receptor_profile_create();

    receptor_profile_set_d1_density(&profile, 0.6F);
    receptor_profile_set_d2_density(&profile, 0.2F);
    receptor_profile_set_serotonin_density(&profile, 0.5F);
    receptor_profile_set_nicotinic_density(&profile, 0.9F);   /* Very high ACh (memory encoding) */
    receptor_profile_set_alpha_density(&profile, 0.4F);
    receptor_profile_set_beta_density(&profile, 0.7F);        /* High beta (consolidation) */

    return profile;
}


receptor_profile_t neuromodulator_profile_striatal(void) {
    /* WHAT: Receptor profile for medium spiny neurons (striatum)
     * WHY:  Striatum is primary dopamine target (motor/reward)
     * BIOLOGICAL: Highest dopamine receptor density in brain
     */
    receptor_profile_t profile = receptor_profile_create();

    receptor_profile_set_d1_density(&profile, 0.9F);          /* Very high D1 (direct pathway) */
    receptor_profile_set_d2_density(&profile, 0.9F);          /* Very high D2 (indirect pathway) */
    receptor_profile_set_serotonin_density(&profile, 0.4F);
    receptor_profile_set_nicotinic_density(&profile, 0.3F);
    receptor_profile_set_alpha_density(&profile, 0.2F);
    receptor_profile_set_beta_density(&profile, 0.3F);

    return profile;
}


receptor_profile_t neuromodulator_profile_amygdala(void) {
    /* WHAT: Receptor profile for amygdala neurons
     * WHY:  Amygdala processes threat/emotion, high NE sensitivity
     * BIOLOGICAL: High NE (threat), DA (valence), 5-HT (anxiety)
     */
    receptor_profile_t profile = receptor_profile_create();

    receptor_profile_set_d1_density(&profile, 0.7F);          /* High DA (valence coding) */
    receptor_profile_set_d2_density(&profile, 0.3F);
    receptor_profile_set_serotonin_density(&profile, 0.8F);   /* High 5-HT (anxiety modulation) */
    receptor_profile_set_nicotinic_density(&profile, 0.4F);
    receptor_profile_set_alpha_density(&profile, 0.9F);       /* Very high NE (threat detection) */
    receptor_profile_set_beta_density(&profile, 0.7F);

    return profile;
}


//=============================================================================
// Ethics Integration
//=============================================================================

bool neuromodulator_release_from_ethics(neuromodulator_system_t system, float golden_rule_score,
                                       float trustworthiness, float harm_score, float salience) {
    /* WHAT: Map ethics evaluation to neuromodulator release
     * WHY:  Ethics = value system, neuromodulators = value signals
     * HOW:  Direct biological mapping based on functional roles
     *
     * MAPPING RATIONALE:
     * - Golden Rule → Dopamine: Ethical = rewarding
     * - Harm → Norepinephrine: Threat detection
     * - Trustworthiness → Acetylcholine: Attention to reliable sources
     * - Violations → Serotonin: Inhibit processing of unethical content
     *
     * COMPLEXITY: O(1)
     */
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_release_from_ethics: system is NULL");
        return false;
    }

    /* WHAT: Map golden rule score to reward/punishment
     * WHY:  Positive ethics should feel rewarding, negative aversive
     * HOW:  Golden rule in [-1, 1], map to dopamine (positive) or serotonin (negative)
     */
    if (golden_rule_score > 0.0F) {
        /* WHAT: Positive ethics → dopamine (reward)
         * WHY:  Reinforces ethical behavior/information
         * PREDICTION: Ethics score is "reward", baseline (0.3) is "expected"
         */
        neuromodulator_release_dopamine(system, golden_rule_score, 0.0F);
    } else {
        /* WHAT: Negative ethics → serotonin (aversion/inhibition)
         * WHY:  Suppresses unethical behavior/information
         */
        neuromodulator_release_serotonin(system, fabsf(golden_rule_score));
    }

    /* WHAT: Map trustworthiness to attention (acetylcholine)
     * WHY:  High trust → attend more, low trust → attend less
     * HOW:  Trustworthiness directly maps to salience
     */
    float combined_salience = (trustworthiness * 0.6F + salience * 0.4F);
    neuromodulator_release_acetylcholine(system, combined_salience);

    /* WHAT: Map harm to threat response (norepinephrine)
     * WHY:  Harmful content triggers vigilance/arousal
     * HOW:  Harm score is threat level, low trust is uncertainty
     */
    float uncertainty = 1.0F - trustworthiness;
    neuromodulator_release_norepinephrine(system, harm_score, uncertainty);

    return true;
}
