// nimcp_neuromodulators_part_lifecycle.c - lifecycle functions
// Part of nimcp_neuromodulators.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_neuromodulators.c


/**
 * @brief Initialize bio-async integration for neuromodulator system
 *
 * WHAT: Register with bio-router and set up message handlers
 * WHY:  Enable async communication with other NIMCP modules
 * HOW:  Register handlers for release and learning rate messages
 *
 * NOTE: If spatial neuromodulator already registered BIO_MODULE_NEUROMODULATOR,
 *       this function will fail to register (which is expected).
 *       The spatial system will handle the messages instead.
 *
 * @param system Neuromodulator system to register
 * @return NIMCP_SUCCESS or error code
 */
static nimcp_error_t neuromod_bio_async_init(neuromodulator_system_t system) {
    if (!system) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Check if bio-router is initialized
    if (!bio_router_is_initialized()) {
        LOG_DEBUG("Bio-router not initialized, skipping neuromodulator bio-async integration");
        return NIMCP_SUCCESS;
    }

    // Thread-safe initialization check with mutex
    nimcp_mutex_lock(&g_neuromod_bio_state.init_mutex);

    // If already initialized, just update the current system
    if (g_neuromod_bio_state.initialized) {
        g_neuromod_bio_state.current_system = system;
        nimcp_mutex_unlock(&g_neuromod_bio_state.init_mutex);
        LOG_DEBUG("Updated neuromodulator bio-async with new system instance");
        return NIMCP_SUCCESS;
    }

    // Register module with bio-router
    // NOTE: This may fail if spatial neuromodulator already registered
    bio_module_info_t module_info = {
        .module_id = BIO_MODULE_NEUROMODULATOR,
        .module_name = "neuromodulator",
        .inbox_capacity = 64,
        .user_data = &g_neuromod_bio_state
    };

    g_neuromod_bio_state.module_ctx = bio_router_register_module(&module_info);
    if (!g_neuromod_bio_state.module_ctx) {
        // This is expected if spatial neuromodulator is already registered
        nimcp_mutex_unlock(&g_neuromod_bio_state.init_mutex);
        LOG_DEBUG("Could not register neuromodulator module - another handler may be active");
        return NIMCP_SUCCESS;  // Not an error - spatial system handles messages
    }

    /* Try KG-driven wiring callback registration first */
    nimcp_error_t wiring_result = bio_router_register_wiring_callback(
        BIO_MODULE_NEUROMODULATOR,
        (void*)neuromodulator_wiring_handler_callback,
        system
    );

    if (wiring_result == NIMCP_SUCCESS) {
        LOG_INFO("Neuromodulator: KG-driven wiring callback registered");
    } else {
        // Legacy fallback - register handlers directly
        nimcp_error_t err;

        LEGACY_HANDLER_REGISTRATION(
            err = bio_router_register_handler(
                g_neuromod_bio_state.module_ctx,
                BIO_MSG_NEUROMODULATOR_RELEASE,
                neuromod_handle_release_message
            )
        );
        if (err != NIMCP_SUCCESS) {
            LOG_ERROR("Failed to register neuromodulator release handler: %d", err);
            bio_router_unregister_module(g_neuromod_bio_state.module_ctx);
            g_neuromod_bio_state.module_ctx = NULL;
            nimcp_mutex_unlock(&g_neuromod_bio_state.init_mutex);
            return err;
        }

        LEGACY_HANDLER_REGISTRATION(
            err = bio_router_register_handler(
                g_neuromod_bio_state.module_ctx,
                BIO_MSG_LEARNING_RATE_UPDATE,
                neuromod_handle_learning_rate_message
            )
        );
        if (err != NIMCP_SUCCESS) {
            LOG_WARN("Failed to register learning rate handler: %d (non-fatal)", err);
            // Continue anyway - release handler is more important
        }

        LOG_INFO("Neuromodulator: legacy handler registration");
    }

    g_neuromod_bio_state.current_system = system;
    g_neuromod_bio_state.initialized = true;
    atomic_init(&g_neuromod_bio_state.messages_processed, 0);

    nimcp_mutex_unlock(&g_neuromod_bio_state.init_mutex);

    LOG_INFO("Neuromodulator bio-async integration initialized");

    return NIMCP_SUCCESS;
}


/**
 * @brief Shutdown bio-async integration for neuromodulator system
 *
 * WHAT: Unregister from bio-router
 * WHY:  Clean shutdown, prevent dangling references
 */
static void neuromod_bio_async_shutdown(void) {
    nimcp_mutex_lock(&g_neuromod_bio_state.init_mutex);

    if (!g_neuromod_bio_state.initialized) {
        nimcp_mutex_unlock(&g_neuromod_bio_state.init_mutex);
        return;
    }

    if (g_neuromod_bio_state.module_ctx) {
        bio_router_unregister_module(g_neuromod_bio_state.module_ctx);
        g_neuromod_bio_state.module_ctx = NULL;
    }

    uint64_t processed = atomic_load(&g_neuromod_bio_state.messages_processed);
    g_neuromod_bio_state.current_system = NULL;
    g_neuromod_bio_state.initialized = false;

    nimcp_mutex_unlock(&g_neuromod_bio_state.init_mutex);

    LOG_INFO("Neuromodulator bio-async shutdown (processed %lu messages)", processed);
}


//=============================================================================
// System Creation and Destruction
//=============================================================================

neuromodulator_system_t neuromodulator_system_create(const neuromodulator_config_t* config) {
    /* WHAT: Allocate and initialize thread-safe neuromodulator system
     * WHY:  Factory pattern for controlled object creation with synchronization
     * COMPLEXITY: O(1)
     * THREAD SAFETY: Creates new rwlock for this instance
     */

    neuromodulator_system_t system = (neuromodulator_system_t)nimcp_calloc(
        1, sizeof(struct neuromodulator_system_struct)
    );

    /* WHAT: Guard clause - check allocation
     * WHY:  Early return prevents nested if
     */
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "neuromodulator_system_create: failed to allocate system");
        return NULL;
    }

    /* WHAT: Initialize reader-writer lock using NIMCP platform abstraction
     * WHY:  Enable thread-safe concurrent access to concentrations
     * PATTERN: Monitor Pattern (synchronized access to shared state)
     * ERROR HANDLING: If initialization fails, clean up and return NULL
     * USES: NIMCP platform rwlock for cross-platform compatibility
     */
    int rwlock_result = nimcp_platform_rwlock_init(&system->rwlock);
    if (rwlock_result != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "neuromodulator_system_create: failed to init rwlock");
        nimcp_free(system);
        return NULL;
    }

    /* WHAT: Initialize default baselines (cortical concentrations)
     * WHY:  Based on typical resting concentrations in cortex
     * NOTE: Dopamine tonic baseline ~50 nM = 0.00005 µM = 0.05 in [0,1] normalized range
     *       This matches biological tonic dopamine levels (1-5 Hz firing)
     *       Phasic bursts (10-20 Hz) will add ~0.3-0.8 on top of this baseline
     */
    system->baselines[NEUROMOD_DOPAMINE] = 0.05F;      // Tonic dopamine: 50 nM
    system->baselines[NEUROMOD_SEROTONIN] = 0.4F;      // 5-HT tonic level
    system->baselines[NEUROMOD_ACETYLCHOLINE] = 0.2F;  // ACh tonic level
    system->baselines[NEUROMOD_NOREPINEPHRINE] = 0.3F; // NE tonic level
    system->baselines[NEUROMOD_GABA] = 0.5F;           // GABA tone
    system->baselines[NEUROMOD_GLUTAMATE] = 0.6F;      // GLU tone

    /* WHAT: Initialize default decay constants (measured clearance rates)
     * WHY:  Based on in vivo measurements from neuroscience literature
     */
    system->decay_times[NEUROMOD_DOPAMINE] = 2.0F;       // Fast (DAT reuptake)
    system->decay_times[NEUROMOD_SEROTONIN] = 10.0F;     // Slow (SERT reuptake)
    system->decay_times[NEUROMOD_ACETYLCHOLINE] = 0.5F;  // Very fast (AChE hydrolysis)
    system->decay_times[NEUROMOD_NOREPINEPHRINE] = 3.0F; // Medium (NET reuptake)
    system->decay_times[NEUROMOD_GABA] = 0.1F;           // Synaptic (GAT)
    system->decay_times[NEUROMOD_GLUTAMATE] = 0.1F;      // Synaptic (EAAT)

    /* WHAT: Initialize default release gains (calibrated responses)
     * WHY:  Produces biologically realistic neuromodulator responses
     */
    system->reward_dopamine_gain = 0.5F;
    system->threat_norepinephrine_gain = 0.7F;
    system->salience_acetylcholine_gain = 0.6F;
    system->punishment_serotonin_gain = 0.4F;

    system->enable_volume_transmission = true;
    system->diffusion_rate = 0.1F;

    /* WHAT: Override defaults with user config if provided
     * WHY:  Allows customization for different brain regions
     * NOTE: No nested if - just override defaults
     */
    if (config) {
        system->baselines[NEUROMOD_DOPAMINE] = config->baseline_dopamine;
        system->baselines[NEUROMOD_SEROTONIN] = config->baseline_serotonin;
        system->baselines[NEUROMOD_ACETYLCHOLINE] = config->baseline_acetylcholine;
        system->baselines[NEUROMOD_NOREPINEPHRINE] = config->baseline_norepinephrine;

        system->decay_times[NEUROMOD_DOPAMINE] = config->dopamine_decay;
        system->decay_times[NEUROMOD_SEROTONIN] = config->serotonin_decay;
        system->decay_times[NEUROMOD_ACETYLCHOLINE] = config->acetylcholine_decay;
        system->decay_times[NEUROMOD_NOREPINEPHRINE] = config->norepinephrine_decay;

        system->reward_dopamine_gain = config->reward_dopamine_gain;
        system->threat_norepinephrine_gain = config->threat_norepinephrine_gain;
        system->salience_acetylcholine_gain = config->salience_acetylcholine_gain;
        system->punishment_serotonin_gain = config->punishment_serotonin_gain;

        system->enable_volume_transmission = config->enable_volume_transmission;
        system->diffusion_rate = config->diffusion_rate;
    }

    /* WHAT: Initialize concentrations to baselines
     * WHY:  Start at steady state (no initial transient)
     */
    memcpy(system->concentrations, system->baselines, sizeof(system->concentrations));

    /* WHAT: Initialize moving averages to baselines
     * WHY:  Prevents initial spike in variance calculation
     * NOTE: Non-atomic stats already zeroed by calloc
     */
    memcpy(system->stats.moving_averages, system->baselines,
           sizeof(system->stats.moving_averages));

    /* WHAT: Initialize atomic counters to zero
     * WHY:  Ensure well-defined initial state for atomics
     * HOW:  atomic_init() is the safe way to initialize atomics
     * PERFORMANCE: No lock required (initialization only)
     */
    for (uint32_t i = 0; i < NEUROMOD_COUNT; i++) {
        atomic_init(&system->stats.release_counts[i], 0);
    }
    atomic_init(&system->stats.update_count, 0);
    atomic_init(&system->stats.rpe_count, 0);

    system->last_update_time = 0;

    /* WHAT: No pool initialization needed
     * WHY:  Thread-local storage is initialized automatically per thread
     * PERFORMANCE: Eliminates contention on pool initialization
     */

    // ===========================================================================
    // PHASE C2.2: Initialize Phasic-Tonic Dynamics + Receptor Subtypes
    // ===========================================================================

    /* WHAT: Disable enhanced dynamics by default for backward compatibility
     * WHY:  Tests expect simple exponential decay model, not phasic-tonic
     * HOW:  Can be enabled explicitly when needed for advanced simulations
     */
    system->use_enhanced_dynamics = false;

    /* WHAT: Initialize phasic-tonic state for each neurotransmitter
     * WHY:  Models burst vs baseline release (RPE encoding, learning signals)
     * HOW:  Uses biological parameters from literature (Schultz et al.)
     */
    uint64_t current_time = 0;  // Will be set on first update

    // Dopamine: Reward learning, motivation
    phasic_tonic_config_t da_config = phasic_tonic_config_dopamine_default();
    phasic_tonic_init(&system->dopamine_phasic_tonic, &da_config, current_time);

    // Serotonin: Mood, inhibition, patience
    phasic_tonic_config_t serotonin_config = phasic_tonic_config_serotonin_default();
    phasic_tonic_init(&system->serotonin_phasic_tonic, &serotonin_config, current_time);

    // Norepinephrine: Arousal, alertness, stress
    phasic_tonic_config_t ne_config = phasic_tonic_config_norepinephrine_default();
    phasic_tonic_init(&system->norepinephrine_phasic_tonic, &ne_config, current_time);

    // Acetylcholine: Attention, encoding, salience
    // Note: Using dopamine config as template (can be customized later)
    phasic_tonic_config_t ach_config = da_config;
    ach_config.initial_tonic = 0.00004F;  // 40 nM
    ach_config.tonic_target = 0.00004F;
    ach_config.burst_decay_tau = 0.1F;    // 100ms (very fast)
    phasic_tonic_init(&system->acetylcholine_phasic_tonic, &ach_config, current_time);

    /* WHAT: Initialize receptor profiles for different brain regions
     * WHY:  Enables regional specialization (cortex excitatory, striatum inhibitory)
     * HOW:  Pre-computed profiles from literature data
     */
    system->cortical_profile = receptor_profile_cortical();
    system->striatal_profile = receptor_profile_striatal();
    // Note: hippocampal_profile can be added later if needed

    // ===========================================================================
    // PHASE C2.3: Initialize Synaptic Vesicle Packaging
    // ===========================================================================

    /* WHAT: Disable vesicle packaging by default for backward compatibility
     * WHY:  Tests expect simple concentration model, not vesicle dynamics
     * HOW:  Can be enabled explicitly when needed for advanced simulations
     */
    system->use_vesicle_packaging = false;

    /* WHAT: Initialize vesicle pools for each neurotransmitter
     * WHY:  Models quantal release, vesicle depletion, and refill dynamics
     * HOW:  Each neurotransmitter gets independent vesicle dynamics
     * BIOLOGICAL: Based on three-pool model (Rizzoli & Betz, 2005)
     */

    // Dopamine vesicles (striatal terminals, reward learning)
    vesicle_pool_init(&system->dopamine_vesicles);

    // Serotonin vesicles (raphe projections, mood regulation)
    vesicle_pool_init(&system->serotonin_vesicles);

    // Norepinephrine vesicles (locus coeruleus, arousal/attention)
    vesicle_pool_init(&system->norepinephrine_vesicles);

    // Acetylcholine vesicles (basal forebrain, attention/encoding)
    vesicle_pool_init(&system->acetylcholine_vesicles);

    /* WHAT: Initialize metabolic pathways for each neurotransmitter
     * WHY:  Models synthesis, degradation, and reuptake dynamics
     * HOW:  Each neurotransmitter gets specific metabolic parameters
     * BIOLOGICAL: Based on enzyme kinetics and transporter properties
     */

    // Dopamine metabolism (tyrosine → DA, MAO degradation, DAT reuptake)
    metabolic_config_t da_metabolic_config = metabolic_config_dopamine_default();
    metabolic_state_init_with_config(&system->dopamine_metabolism, &da_metabolic_config);

    // Serotonin metabolism (tryptophan → 5-HT, MAO degradation, SERT reuptake)
    metabolic_config_t serotonin_metabolic_config = metabolic_config_serotonin_default();
    metabolic_state_init_with_config(&system->serotonin_metabolism, &serotonin_metabolic_config);

    // Norepinephrine metabolism (DA → NE, MAO+COMT degradation, NET reuptake)
    metabolic_config_t ne_metabolic_config = metabolic_config_norepinephrine_default();
    metabolic_state_init_with_config(&system->norepinephrine_metabolism, &ne_metabolic_config);

    // Acetylcholine metabolism (choline → ACh, AChE degradation, ChT reuptake)
    metabolic_config_t ach_metabolic_config = metabolic_config_acetylcholine_default();
    metabolic_state_init_with_config(&system->acetylcholine_metabolism, &ach_metabolic_config);

    // Disable metabolic pathways by default for backward compatibility
    // (Tests expect simple concentration model, not full metabolic dynamics)
    system->use_metabolic_pathways = false;

    // Phase E1: Initialize grief system for cognitive pipeline integration
    system->grief_system = grief_system_create();
    system->use_grief_integration = (system->grief_system != NULL);

    // Phase E2: Initialize joy/euphoria system for cognitive pipeline integration
    system->joy_system = joy_system_create();
    system->use_joy_integration = (system->joy_system != NULL);

    // Phase E4: Initialize social bond system for cognitive pipeline integration
    system->social_system = social_bond_system_create();
    system->use_social_integration = (system->social_system != NULL);

    // Medulla integration: Initialize brain reference (connected externally)
    system->brain_ref = NULL;
    system->use_medulla_integration = false;

    // Sleep integration: Initialize to awake state
    system->current_sleep_state = SLEEP_STATE_AWAKE;

    // Bio-async integration: Register with bio-router for inter-module messaging
    nimcp_error_t bio_err = neuromod_bio_async_init(system);
    if (bio_err != NIMCP_SUCCESS) {
        LOG_DEBUG("Bio-async integration not available: %d (non-fatal)", bio_err);
        // Continue anyway - bio-async is optional
    }

    return system;
}


void neuromodulator_system_destroy(neuromodulator_system_t system) {
    /* WHAT: Free neuromodulator system resources including synchronization primitives
     * WHY:  Prevent memory leaks and resource leaks (lock handles)
     * COMPLEXITY: O(1)
     * THREAD SAFETY: Caller must ensure no other threads are using this system
     */

    /* WHAT: Guard clause - NULL safety
     * WHY:  Early return, no nested if
     */
    if (!system) return;

    /* WHAT: Cleanup bio-async integration if this was the registered system
     * WHY:  Prevent dangling references to destroyed system
     * NOTE: Only unregister if we're destroying the currently registered system
     */
    if (g_neuromod_bio_state.current_system == system) {
        neuromod_bio_async_shutdown();
    }

    /* WHAT: Destroy reader-writer lock using NIMCP platform abstraction
     * WHY:  Release OS resources (lock handle, kernel data structures)
     * CORRECTNESS: Must be done before freeing memory
     * ERROR HANDLING: Ignore errors (best effort cleanup)
     * USES: NIMCP platform rwlock for cross-platform compatibility
     */
    nimcp_platform_rwlock_destroy(&system->rwlock);

    /* WHAT: Phase E1 - Destroy grief system
     * WHY:  Free grief system resources
     * CORRECTNESS: Must be done before freeing system memory
     */
    if (system->grief_system) {
        grief_system_destroy(system->grief_system);
        system->grief_system = NULL;
    }

    /* WHAT: Phase E2 - Destroy joy/euphoria system
     * WHY:  Free joy system resources
     * CORRECTNESS: Must be done before freeing system memory
     */
    if (system->joy_system) {
        joy_system_destroy(system->joy_system);
        system->joy_system = NULL;
    }

    /* WHAT: Phase E4 - Destroy social bond system
     * WHY:  Free social system resources
     * CORRECTNESS: Must be done before freeing system memory
     */
    if (system->social_system) {
        social_bond_system_destroy(system->social_system);
        system->social_system = NULL;
    }

    /* WHAT: Zero out memory before free
     * WHY:  Security - prevent use-after-free exploits
     * NOTE: Atomic counters don't need explicit cleanup
     */
    memset(system, 0, sizeof(struct neuromodulator_system_struct));
    nimcp_free(system);
}


bool neuromodulator_reset(neuromodulator_system_t system) {
    /* WHAT: Reset all concentrations to baseline
     * WHY:  For testing, or simulating "sleep" (homeostatic reset)
     * COMPLEXITY: O(1)
     */
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_reset: system is NULL");
        return false;
    }

    memcpy(system->concentrations, system->baselines, sizeof(system->concentrations));

    return true;
}


//=============================================================================
// Tensor-Based Neuromodulator Pool Functions
//=============================================================================

neuromodulator_pool_t neuromodulator_pool_create(void) {
    neuromodulator_pool_t pool = {0};

    uint32_t dims[1] = {NEUROMOD_COUNT};
    pool.concentrations = nimcp_tensor_zeros(dims, 1, NIMCP_DTYPE_F32);
    pool.decay_rates = nimcp_tensor_zeros(dims, 1, NIMCP_DTYPE_F32);
    pool.last_update = 0;
    pool.owns_tensors = true;

    return pool;
}


void neuromodulator_pool_destroy(neuromodulator_pool_t* pool) {
    if (!pool) return;
    if (!pool->owns_tensors) return;

    if (pool->concentrations) {
        nimcp_tensor_destroy(pool->concentrations);
        pool->concentrations = NULL;
    }
    if (pool->decay_rates) {
        nimcp_tensor_destroy(pool->decay_rates);
        pool->decay_rates = NULL;
    }
}


//=============================================================================
// Tensor-Based Receptor Profile Functions
//=============================================================================

receptor_profile_t receptor_profile_create(void) {
    receptor_profile_t profile = {0};

    uint32_t dims[1] = {RECEPTOR_COUNT};
    profile.densities = nimcp_tensor_zeros(dims, 1, NIMCP_DTYPE_F32);
    profile.owns_tensor = true;

    return profile;
}


void receptor_profile_destroy(receptor_profile_t* profile) {
    if (!profile) return;
    if (!profile->owns_tensor) return;

    if (profile->densities) {
        nimcp_tensor_destroy(profile->densities);
        profile->densities = NULL;
    }
}


//=============================================================================
// Tensor-Based Modulation Effects Functions
//=============================================================================

modulation_effects_t modulation_effects_create(void) {
    modulation_effects_t effects = {0};

    uint32_t dims[1] = {MODULATION_EFFECT_COUNT};
    effects.effects = nimcp_tensor_zeros(dims, 1, NIMCP_DTYPE_F32);
    effects.owns_tensor = true;

    /* Initialize default values */
    if (effects.effects) {
        uint32_t idx[1];
        idx[0] = MODULATION_EFFECT_LEARNING_RATE;
        nimcp_tensor_set(effects.effects, idx, 1.0);  /* Default multiplier = 1 */
        idx[0] = MODULATION_EFFECT_TRANSMISSION;
        nimcp_tensor_set(effects.effects, idx, 1.0);  /* Default gain = 1 */
        idx[0] = MODULATION_EFFECT_EXCITABILITY;
        nimcp_tensor_set(effects.effects, idx, 0.0);  /* Default shift = 0 */
        idx[0] = MODULATION_EFFECT_ATTENTION;
        nimcp_tensor_set(effects.effects, idx, 0.5);  /* Default attention = 0.5 */
    }

    return effects;
}


void modulation_effects_destroy(modulation_effects_t* effects) {
    if (!effects) return;
    if (!effects->owns_tensor) return;

    if (effects->effects) {
        nimcp_tensor_destroy(effects->effects);
        effects->effects = NULL;
    }
}
