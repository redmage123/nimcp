// nimcp_neuromodulators_part_processing.c - processing functions
// Part of nimcp_neuromodulators.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_neuromodulators.c


/**
 * @brief Handle neuromodulator release message
 *
 * WHAT: Processes BIO_MSG_NEUROMODULATOR_RELEASE messages
 * WHY:  Allows other modules to trigger neuromodulator release via bio-async
 * HOW:  Extract amount and type, call appropriate release function
 */
static nimcp_error_t neuromod_handle_release_message(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    (void)user_data;

    if (!msg || msg_size < sizeof(bio_msg_neuromodulator_release_t)) {
        LOG_ERROR("Invalid neuromodulator release message size: %zu < %zu",
                  msg_size, sizeof(bio_msg_neuromodulator_release_t));
        if (response_promise) {
            nimcp_bio_promise_fail(response_promise, NIMCP_ERROR_INVALID_PARAM);
        }
        return NIMCP_ERROR_INVALID_PARAM;
    }

    neuromodulator_system_t system = g_neuromod_bio_state.current_system;
    if (!system) {
        LOG_WARN("No neuromodulator system registered for bio-async");
        if (response_promise) {
            nimcp_bio_promise_fail(response_promise, NIMCP_ERROR_NOT_INITIALIZED);
        }
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    const bio_msg_neuromodulator_release_t* release_msg =
        (const bio_msg_neuromodulator_release_t*)msg;

    neuromodulator_type_t type = bio_channel_to_neuromod_type(release_msg->neuromodulator);
    float amount = release_msg->release_amount;

    // Release neuromodulator based on type
    float released = 0.0f;
    switch (type) {
        case NEUROMOD_DOPAMINE:
            released = neuromodulator_release_dopamine(system, amount, 0.0f);
            break;
        case NEUROMOD_SEROTONIN:
            released = neuromodulator_release_serotonin(system, amount);
            break;
        case NEUROMOD_ACETYLCHOLINE:
            released = neuromodulator_release_acetylcholine(system, amount);
            break;
        case NEUROMOD_NOREPINEPHRINE:
            released = neuromodulator_release_norepinephrine(system, amount, 0.5f);
            break;
        default:
            break;
    }

    atomic_fetch_add(&g_neuromod_bio_state.messages_processed, 1);

    LOG_DEBUG("Released %.3f of neuromodulator type %d via bio-async (actual: %.3f)",
              amount, type, released);

    // Complete promise with updated concentration
    if (response_promise) {
        bio_msg_neuromodulator_release_t response;
        memcpy(&response, release_msg, sizeof(response));
        response.header.source_module = BIO_MODULE_NEUROMODULATOR;
        response.header.target_module = release_msg->header.source_module;
        response.current_concentration = neuromodulator_get_level(system, type);
        nimcp_bio_promise_complete(response_promise, &response);
    }

    return NIMCP_SUCCESS;
}


/**
 * @brief Handle learning rate update message
 *
 * WHAT: Processes BIO_MSG_LEARNING_RATE_UPDATE messages
 * WHY:  Returns modulated learning rate based on current neuromodulator levels
 */
static nimcp_error_t neuromod_handle_learning_rate_message(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    (void)user_data;

    if (!msg || msg_size < sizeof(bio_msg_learning_rate_update_t)) {
        LOG_ERROR("Invalid learning rate update message size");
        if (response_promise) {
            nimcp_bio_promise_fail(response_promise, NIMCP_ERROR_INVALID_PARAM);
        }
        return NIMCP_ERROR_INVALID_PARAM;
    }

    neuromodulator_system_t system = g_neuromod_bio_state.current_system;
    if (!system) {
        LOG_WARN("No neuromodulator system registered for bio-async");
        if (response_promise) {
            nimcp_bio_promise_fail(response_promise, NIMCP_ERROR_NOT_INITIALIZED);
        }
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    const bio_msg_learning_rate_update_t* lr_msg =
        (const bio_msg_learning_rate_update_t*)msg;

    // Get current neuromodulator levels
    float da_level = neuromodulator_get_level(system, NEUROMOD_DOPAMINE);
    float serotonin_level = neuromodulator_get_level(system, NEUROMOD_SEROTONIN);

    // Compute modulated learning rate
    // Simple modulation: high DA increases learning, high 5-HT decreases
    float modulation = 1.0f + (da_level * 0.5f) - (serotonin_level * 0.3f);
    if (modulation < 0.1f) modulation = 0.1f;
    if (modulation > 2.0f) modulation = 2.0f;

    float modulated_lr = lr_msg->base_learning_rate * modulation;

    atomic_fetch_add(&g_neuromod_bio_state.messages_processed, 1);

    // Complete promise with modulated learning rate
    if (response_promise) {
        bio_msg_learning_rate_update_t response;
        memcpy(&response, lr_msg, sizeof(response));
        response.header.source_module = BIO_MODULE_NEUROMODULATOR;
        response.header.target_module = lr_msg->header.source_module;
        response.modulated_learning_rate = modulated_lr;
        response.dopamine_level = da_level;
        response.serotonin_level = serotonin_level;
        nimcp_bio_promise_complete(response_promise, &response);
    }

    return NIMCP_SUCCESS;
}


/* ============================================================================
 * KG-Driven Wiring Callback
 * ============================================================================ */

/**
 * @brief Wiring callback for KG-driven handler registration
 *
 * WHAT: Register message handlers based on discovered wiring from KG
 * WHY:  Enables runtime assembly - module discovers its handlers from KG
 * HOW:  Orchestrator invokes this with message types from HANDLES_MESSAGE relations
 */
static int neuromodulator_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    (void)user_data;

    if (!ctx || !message_types || message_count == 0) {
        return 0;
    }

    int registered = 0;
    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_NEUROMODULATOR_RELEASE:
                bio_router_register_handler(ctx, message_types[i], neuromod_handle_release_message);
                registered++;
                LOG_DEBUG("  Registered handler for BIO_MSG_NEUROMODULATOR_RELEASE");
                break;

            case BIO_MSG_LEARNING_RATE_UPDATE:
                bio_router_register_handler(ctx, message_types[i], neuromod_handle_learning_rate_message);
                registered++;
                LOG_DEBUG("  Registered handler for BIO_MSG_LEARNING_RATE_UPDATE");
                break;

            default:
                LOG_DEBUG("Neuromodulator: unknown message type %d in wiring callback", message_types[i]);
                break;
        }
    }

    return (registered > 0) ? 0 : -1;
}


/**
 * @brief Process pending bio-async messages for neuromodulator system
 *
 * WHAT: Polls inbox and invokes handlers for pending messages
 * WHY:  Messages are queued; must be explicitly processed
 * HOW:  Delegates to bio_router_process_inbox
 *
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t neuromodulator_bio_async_process(uint32_t max_messages) {
    if (!g_neuromod_bio_state.initialized || !g_neuromod_bio_state.module_ctx) {
        return 0;
    }

    return bio_router_process_inbox(g_neuromod_bio_state.module_ctx, max_messages);
}


//=============================================================================
// Dynamics Update (Decay)
//=============================================================================

bool neuromodulator_update(neuromodulator_system_t system, float dt) {
    /* WHAT: Update all neuromodulator concentrations via exponential decay (thread-safe)
     * WHY:  Neurotransmitters are cleared by reuptake and metabolism
     * HOW:  First-order kinetics: dc/dt = -(c - baseline) / τ
     *
     * BIOLOGICAL: Models transporter-mediated clearance and enzymatic degradation
     * COMPLEXITY: O(NEUROMOD_COUNT) = O(1) since count is fixed
     * THREAD SAFETY: Acquires write lock (exclusive access, blocks readers)
     *
     * @param system Neuromodulator system
     * @param dt Time step in seconds
     * @return true on success
     */
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromodulator_update: system is NULL");
        return false;
    }
    if (dt < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromodulator_update: invalid dt");
        return false;
    }

    /* WHAT: Acquire write lock for exclusive modification access
     * WHY:  Prevent concurrent readers from seeing partial updates
     * PATTERN: RAII-style (lock, modify, unlock)
     * PERFORMANCE: Blocks all readers during update (brief critical section)
     */
    nimcp_platform_rwlock_wrlock(&system->rwlock);

    // ===========================================================================
    // PHASE C2.2: Enhanced Phasic-Tonic Dynamics Update
    // ===========================================================================

    if (system->use_enhanced_dynamics) {
        /* WHAT: Update phasic-tonic dynamics for each neurotransmitter
         * WHY:  Models burst decay and homeostatic tonic regulation
         * HOW:  Replaces simple exponential decay with dual-mode dynamics
         */

        uint64_t current_time = system->last_update_time;

        // Update dopamine phasic-tonic dynamics
        phasic_tonic_update(&system->dopamine_phasic_tonic, dt, current_time);
        float da_conc = phasic_tonic_get_concentration(&system->dopamine_phasic_tonic);
        system->concentrations[NEUROMOD_DOPAMINE] = nimcp_clampf(da_conc * 1000.0F, 0.0F, 1.0F);

        // Update serotonin phasic-tonic dynamics
        phasic_tonic_update(&system->serotonin_phasic_tonic, dt, current_time);
        float serotonin_conc = phasic_tonic_get_concentration(&system->serotonin_phasic_tonic);
        system->concentrations[NEUROMOD_SEROTONIN] = nimcp_clampf(serotonin_conc * 1000.0F, 0.0F, 1.0F);

        // Update norepinephrine phasic-tonic dynamics
        phasic_tonic_update(&system->norepinephrine_phasic_tonic, dt, current_time);
        float ne_conc = phasic_tonic_get_concentration(&system->norepinephrine_phasic_tonic);
        system->concentrations[NEUROMOD_NOREPINEPHRINE] = nimcp_clampf(ne_conc * 1000.0F, 0.0F, 1.0F);

        // Update acetylcholine phasic-tonic dynamics
        phasic_tonic_update(&system->acetylcholine_phasic_tonic, dt, current_time);
        float ach_conc = phasic_tonic_get_concentration(&system->acetylcholine_phasic_tonic);
        system->concentrations[NEUROMOD_ACETYLCHOLINE] = nimcp_clampf(ach_conc * 1000.0F, 0.0F, 1.0F);

        // Update statistics for all systems
        for (uint32_t i = 0; i < NEUROMOD_COUNT; i++) {
            float concentration = system->concentrations[i];

            system->stats.moving_averages[i] = update_ema(
                system->stats.moving_averages[i],
                concentration,
                0.1F
            );

            float delta = concentration - system->stats.moving_averages[i];
            system->stats.variances[i] = update_ema(
                system->stats.variances[i],
                delta * delta,
                0.1F
            );
        }

    } else {
        /* WHAT: Legacy simple exponential decay (fallback)
         * WHY:  For compatibility or when enhanced dynamics not needed
         * HOW:  Original behavior preserved
         */

        /* Apply sleep state modulation to neuromodulator baselines */
        float ach_factor = neuromod_sleep_get_ach_factor(system->current_sleep_state);
        float ne_factor = neuromod_sleep_get_ne_factor(system->current_sleep_state);
        float da_factor = neuromod_sleep_get_da_factor(system->current_sleep_state);
        float serotonin_factor = neuromod_sleep_get_serotonin_factor(system->current_sleep_state);

        for (uint32_t i = 0; i < NEUROMOD_COUNT; i++) {
            /* Modulate baseline based on sleep state */
            float modulated_baseline = system->baselines[i];
            if (i == NEUROMOD_ACETYLCHOLINE) {
                modulated_baseline *= ach_factor;
            } else if (i == NEUROMOD_NOREPINEPHRINE) {
                modulated_baseline *= ne_factor;
            } else if (i == NEUROMOD_DOPAMINE) {
                modulated_baseline *= da_factor;
            } else if (i == NEUROMOD_SEROTONIN) {
                modulated_baseline *= serotonin_factor;
            }

            float new_concentration = exponential_decay(
                system->concentrations[i],
                modulated_baseline,
                dt,
                system->decay_times[i]
            );

            system->concentrations[i] = new_concentration;

            system->stats.moving_averages[i] = update_ema(
                system->stats.moving_averages[i],
                new_concentration,
                0.1F
            );

            float delta = new_concentration - system->stats.moving_averages[i];
            system->stats.variances[i] = update_ema(
                system->stats.variances[i],
                delta * delta,
                0.1F
            );
        }
    }

    /* WHAT: Update timestamp under lock
     * WHY:  Maintain consistency with concentration values
     */
    system->last_update_time += (uint64_t)(dt * 1000.0F);  // Convert to ms

    /* WHAT: Release write lock before atomic increment
     * WHY:  Atomic counter doesn't need lock protection
     * PERFORMANCE: Minimize critical section duration
     */
    nimcp_platform_rwlock_wrunlock(&system->rwlock);

    /* WHAT: Atomically increment update counter
     * WHY:  Track update frequency without lock overhead
     * THREAD SAFETY: atomic_fetch_add is lock-free
     */
    atomic_fetch_add(&system->stats.update_count, 1);

    return true;
}
