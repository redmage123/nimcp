// nimcp_neuromodulators_part_helpers.c - helpers functions
// Part of nimcp_neuromodulators.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_neuromodulators.c


//=============================================================================
// Helper Functions - Inline for Performance
//=============================================================================


/**
 * WHAT: Exponential decay formula
 * WHY:  Models first-order clearance kinetics (reuptake + metabolism)
 * HOW:  c(t+Δt) = c(t) × exp(-Δt/τ) + baseline × (1 - exp(-Δt/τ))
 *
 * BIOLOGICAL: Matches neurotransmitter clearance dynamics
 * COMPLEXITY: O(1)
 *
 * @param current Current concentration
 * @param baseline Baseline concentration (attractor)
 * @param dt Time step (seconds)
 * @param tau Decay time constant (seconds)
 * @return New concentration after decay
 */
static inline float exponential_decay(float current, float baseline, float dt, float tau) {
    /* WHAT: Compute decay factor
     * WHY:  exp(-dt/tau) is fraction remaining after time dt
     */
    float decay_factor = expf(-dt / (tau + EPSILON));

    /* WHAT: Decay toward baseline, not zero
     * WHY:  Neurons maintain tonic firing rates (baseline)
     * FORMULA: c_new = c × decay + baseline × (1 - decay)
     */
    return current * decay_factor + baseline * (1.0F - decay_factor);
}


/**
 * WHAT: Update exponential moving average
 * WHY:  Smooths statistics, matches biological timescales
 * HOW:  EMA(t) = α × value + (1-α) × EMA(t-1)
 *
 * COMPLEXITY: O(1)
 *
 * @param current_avg Current average
 * @param new_value New sample
 * @param alpha Smoothing factor (0-1), typically 0.1
 * @return Updated average
 */
static inline float update_ema(float current_avg, float new_value, float alpha) {
    return alpha * new_value + (1.0F - alpha) * current_avg;
}


//=============================================================================
// Bio-Async Message Handlers
//=============================================================================

/**
 * @brief Map bio channel type to neuromodulator type
 *
 * WHAT: Converts bio-async channel enum to neuromodulator enum
 * WHY:  Message uses BIO_CHANNEL_*, system uses NEUROMOD_*
 */
static neuromodulator_type_t bio_channel_to_neuromod_type(nimcp_bio_channel_type_t channel) {
    switch (channel) {
        case BIO_CHANNEL_DOPAMINE:      return NEUROMOD_DOPAMINE;
        case BIO_CHANNEL_SEROTONIN:     return NEUROMOD_SEROTONIN;
        case BIO_CHANNEL_ACETYLCHOLINE: return NEUROMOD_ACETYLCHOLINE;
        case BIO_CHANNEL_NOREPINEPHRINE: return NEUROMOD_NOREPINEPHRINE;
        default:                        return NEUROMOD_DOPAMINE;  // Default
    }
}
