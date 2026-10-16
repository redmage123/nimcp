// nimcp_plasticity_orchestrator_part_processing.c - processing functions
// Part of nimcp_plasticity_orchestrator.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_plasticity_orchestrator.c


/**
 * @brief Structural plasticity change callback
 *
 * WHAT: Handles spine formation/elimination events from structural module
 * WHY:  Converts structural events to orchestrator events for tracking
 * HOW:  Maps structural event types to plasticity events
 */
static void structural_change_handler(
    structural_event_t event,
    uint32_t synapse_id,
    synapse_state_t old_state,
    synapse_state_t new_state,
    void* user_data
) {
    plasticity_orchestrator_t* orch = (plasticity_orchestrator_t*)user_data;
    if (!orch) {
        NIMCP_LOGGING_DEBUG("structural_change_handler: NULL orchestrator");
        return;
    }

    switch (event) {
        case STRUCTURAL_EVENT_FORMATION:
            emit_event(orch, PLASTICITY_EVENT_SPINE_FORMED, synapse_id, 0, 0.0f, 1.0f);
            break;
        case STRUCTURAL_EVENT_ELIMINATION:
            emit_event(orch, PLASTICITY_EVENT_SPINE_ELIMINATED, synapse_id, 0, 1.0f, 0.0f);
            break;
        default:
            /* Other structural events (stabilization, potentiation, etc.) */
            break;
    }
}


int plasticity_orchestrator_register_pre_update(
    plasticity_orchestrator_t* orchestrator,
    plasticity_update_callback_t callback,
    void* user_data
) {
    if (!orchestrator || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL pointer in register_pre_update");
        return -1;
    }

    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (!orchestrator->pre_update_callbacks[i].active) {
            orchestrator->pre_update_callbacks[i].callback = callback;
            orchestrator->pre_update_callbacks[i].user_data = user_data;
            orchestrator->pre_update_callbacks[i].active = true;
            return 0;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plasticity_orchestrator_register_pre_update: orchestrator->pre_update_callbacks is NULL");
    return -1;
}


int plasticity_orchestrator_register_post_update(
    plasticity_orchestrator_t* orchestrator,
    plasticity_update_callback_t callback,
    void* user_data
) {
    if (!orchestrator || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL pointer in register_post_update");
        return -1;
    }

    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (!orchestrator->post_update_callbacks[i].active) {
            orchestrator->post_update_callbacks[i].callback = callback;
            orchestrator->post_update_callbacks[i].user_data = user_data;
            orchestrator->post_update_callbacks[i].active = true;
            return 0;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plasticity_orchestrator_register_post_update: orchestrator->post_update_callbacks is NULL");
    return -1;
}


/* ============================================================================
 * Main Update Function
 * ============================================================================ */

int plasticity_orchestrator_update(
    plasticity_orchestrator_t* orchestrator,
    uint64_t delta_ms
) {
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Orchestrator is NULL in update");
        return -1;
    }

    nimcp_platform_mutex_lock(orchestrator->mutex);

    orchestrator->current_time_ms += delta_ms;
    float dt = (float)delta_ms;

    /* Phase 8: Heartbeat at start of orchestrator update */
    plasticity_orchestrator_heartbeat("orchestrator_update", 0.0f);

    /* Copy pre-update callbacks under lock, then invoke outside lock */
    {
        plasticity_update_callback_t pre_cbs[MAX_CALLBACKS];
        void* pre_data[MAX_CALLBACKS];
        int num_pre = 0;

        for (int i = 0; i < MAX_CALLBACKS; i++) {
            if (orchestrator->pre_update_callbacks[i].active) {
                pre_cbs[num_pre] = orchestrator->pre_update_callbacks[i].callback;
                pre_data[num_pre] = orchestrator->pre_update_callbacks[i].user_data;
                num_pre++;
            }
        }

        nimcp_platform_mutex_unlock(orchestrator->mutex);
        for (int i = 0; i < num_pre; i++) {
            pre_cbs[i](orchestrator, delta_ms, pre_data[i]);
        }
        nimcp_platform_mutex_lock(orchestrator->mutex);
    }

    /* Update sleep modulation */
    orchestrator->sleep_modulation_factor = get_sleep_modulation(orchestrator->current_sleep_state);

    /* Update immune modulation */
    if (orchestrator->immune) {
        brain_inflammation_level_t inflammation = brain_immune_get_inflammation_level(
            (brain_immune_system_t*)orchestrator->immune);
        orchestrator->immune_modulation_factor = get_immune_modulation(inflammation);
    }

    /* Compute effective learning rate */
    float effective_lr = orchestrator->config.global_learning_rate *
                         orchestrator->sleep_modulation_factor *
                         orchestrator->immune_modulation_factor;

    /* ============ ORCHESTRATION ORDER ============ */

    /* Phase 8: Heartbeat before main orchestration loop */
    plasticity_orchestrator_heartbeat("orchestrator_update", 0.1f);

    /* 1. Metabolic check - ensure sufficient ATP */
    if (orchestrator->config.enabled.enable_metabolic && orchestrator->metabolic) {
        metabolic_plasticity_update(orchestrator->metabolic, delta_ms);

        float atp = metabolic_plasticity_get_atp_level(orchestrator->metabolic);
        orchestrator->stats.mean_atp = atp;
        if (atp < orchestrator->stats.min_atp) {
            orchestrator->stats.min_atp = atp;
        }

        /* Check if plasticity is blocked */
        if (!metabolic_plasticity_can_ltp(orchestrator->metabolic) &&
            !metabolic_plasticity_can_ltd(orchestrator->metabolic)) {
            orchestrator->stats.energy_blocked_events++;
            emit_event(orchestrator, PLASTICITY_EVENT_ENERGY_DEPLETED, 0, 0, atp, atp);
        }
    }

    /* 2. Calcium dynamics - update [Ca²⁺] */
    if (orchestrator->config.enabled.enable_calcium && orchestrator->calcium) {
        calcium_update(orchestrator->calcium, dt);
    }

    /* 3. STDP / Triplet STDP - handled via spike events */

    /* 3.5 Decay neuron firing rates during inactivity */
    for (size_t i = 0; i < orchestrator->num_neurons; i++) {
        neuron_entry_t* neuron = &orchestrator->neurons[i];
        if (!neuron->active) continue;

        /* Exponential decay with ~1 second time constant */
        float decay_rate = 0.001f;  /* 1/tau where tau = 1000ms */
        neuron->firing_rate *= expf(-decay_rate * dt);
    }

    /* 3.6 Decay synapse activity rates during inactivity */
    for (size_t i = 0; i < orchestrator->num_synapses; i++) {
        synapse_entry_t* syn = &orchestrator->synapses[i];
        if (!syn->active) continue;

        /* Exponential decay with ~5 second time constant for orchestrator tracking */
        float syn_decay_rate = 0.0002f;  /* 1/tau where tau = 5000ms */
        syn->recent_activity_hz *= expf(-syn_decay_rate * dt);
    }

    /* 4. Heterosynaptic - apply neighbor depression for recent potentiations */
    if (orchestrator->config.enabled.enable_heterosynaptic && orchestrator->heterosynaptic) {
        /* Depression is applied during LTP events */
    }

    /* 5. BCM - apply threshold-based selectivity */
    if (orchestrator->config.enabled.enable_bcm) {
        bcm_params_t bcm_params = bcm_params_cortical();

        for (size_t i = 0; i < orchestrator->num_synapses; i++) {
            synapse_entry_t* syn = &orchestrator->synapses[i];
            if (!syn->active) continue;

            /* Update BCM threshold */
            bcm_update_threshold(&syn->bcm_state, syn->recent_activity_hz, dt, &bcm_params);
        }

        /* Update neuron thresholds based on their firing rates (metaplasticity) */
        for (size_t i = 0; i < orchestrator->num_neurons; i++) {
            neuron_entry_t* neuron = &orchestrator->neurons[i];
            if (!neuron->active) continue;

            /* Threshold slides with average activity (BCM rule) */
            float rate = neuron->firing_rate;
            float tau = bcm_params.threshold_time_constant;

            /* Exponential sliding: dθ/dt = (rate² - θ) / tau */
            float dtheta = ((rate * rate) - neuron->bcm_threshold) * (dt / tau);
            neuron->bcm_threshold += dtheta;

            /* Clamp threshold */
            if (neuron->bcm_threshold < bcm_params.min_threshold)
                neuron->bcm_threshold = bcm_params.min_threshold;
            if (neuron->bcm_threshold > bcm_params.max_threshold)
                neuron->bcm_threshold = bcm_params.max_threshold;
        }
    }

    /* Phase 8: Heartbeat at midpoint of orchestration */
    plasticity_orchestrator_heartbeat("orchestrator_update", 0.5f);

    /* 6. Homeostatic - maintain target firing rates (periodic) */
    if (orchestrator->config.enabled.enable_homeostatic) {
        if (orchestrator->current_time_ms - orchestrator->last_homeostatic_time >=
            orchestrator->config.homeostatic_interval_ms) {

            /* Apply synaptic scaling to each neuron */
            synaptic_scaling_params_t scale_params = homeostatic_scaling_params_default();

            for (size_t i = 0; i < orchestrator->num_neurons; i++) {
                neuron_entry_t* neuron = &orchestrator->neurons[i];
                if (!neuron->active) continue;

                /* Update rate estimate */
                synaptic_scaling_update_rate(&neuron->scaling_state, false, dt, &scale_params);

                /* Compute scaling factor */
                float factor = synaptic_scaling_compute_factor(&neuron->scaling_state, &scale_params);

                if (fabsf(factor - 1.0f) > 0.01f) {
                    /* Apply scaling to input synapses */
                    emit_event(orchestrator, PLASTICITY_EVENT_HOMEOSTATIC_SCALE,
                              0, neuron->id, 1.0f, factor);
                }
            }

            orchestrator->last_homeostatic_time = orchestrator->current_time_ms;
        }
    }

    /* 7. Metaplasticity - update sliding thresholds */
    /* Note: metaplasticity controller was pre-allocated with MAX_SYNAPSES entries,
     * so we need to provide a full-size activities array to avoid buffer overread */
    if (orchestrator->config.enabled.enable_metaplasticity && orchestrator->metaplasticity) {
        /* Allocate full-size array (controller expects MAX_SYNAPSES entries) */
        float* activities = (float*)nimcp_calloc(MAX_SYNAPSES, sizeof(float));
        if (activities) {
            /* Fill in activity values for registered synapses only */
            for (size_t i = 0; i < orchestrator->num_synapses && i < MAX_SYNAPSES; i++) {
                activities[i] = orchestrator->synapses[i].recent_activity_hz;
            }

            metaplasticity_controller_update_all(
                orchestrator->metaplasticity,
                activities,
                &orchestrator->neuromod_levels,
                dt
            );

            nimcp_free(activities);
        }
    }

    /* 8. Protein synthesis - tag capture for consolidation (periodic) */
    if (orchestrator->config.enabled.enable_protein_synthesis && orchestrator->protein_synthesis) {
        protein_synthesis_update(orchestrator->protein_synthesis, delta_ms);

        if (orchestrator->current_time_ms - orchestrator->last_consolidation_time >=
            orchestrator->config.consolidation_interval_ms) {

            /* Attempt consolidation for tagged synapses */
            for (size_t i = 0; i < orchestrator->num_synapses; i++) {
                synapse_entry_t* syn = &orchestrator->synapses[i];
                if (!syn->active || !syn->consolidation_tagged) continue;

                if (protein_synthesis_can_consolidate(orchestrator->protein_synthesis, syn->id)) {
                    if (protein_synthesis_consolidate_synapse(orchestrator->protein_synthesis, syn->id) == 0) {
                        syn->consolidation_tagged = false;
                        emit_event(orchestrator, PLASTICITY_EVENT_CONSOLIDATION,
                                  syn->id, 0, syn->weight, syn->weight);
                    }
                }
            }

            orchestrator->last_consolidation_time = orchestrator->current_time_ms;
        }
    }

    /* 9. Structural plasticity - spine dynamics */
    if (orchestrator->config.enabled.enable_structural && orchestrator->structural) {
        structural_plasticity_update(orchestrator->structural, dt / 1000.0f);
    }

    /* 10. Astrocyte - glial modulation */
    if (orchestrator->config.enabled.enable_astrocyte && orchestrator->astrocyte) {
        /* Update astrocytes with average synaptic activity */
        float avg_activity = 0.0f;
        for (size_t i = 0; i < orchestrator->num_synapses; i++) {
            avg_activity += orchestrator->synapses[i].recent_activity_hz;
        }
        if (orchestrator->num_synapses > 0) {
            avg_activity /= (float)orchestrator->num_synapses;
        }

        for (uint32_t a = 0; a < MAX_ASTROCYTES && a < astrocyte_plasticity_get_num_astrocytes(orchestrator->astrocyte); a++) {
            astrocyte_plasticity_update(orchestrator->astrocyte, a, avg_activity / 100.0f, delta_ms);
        }
    }

    /* Update weight statistics */
    update_weight_statistics(orchestrator);

    /* Phase 8: Heartbeat before post-update callbacks */
    plasticity_orchestrator_heartbeat("orchestrator_update", 0.9f);

    /* Copy post-update callbacks under lock, then invoke outside lock */
    {
        plasticity_update_callback_t post_cbs[MAX_CALLBACKS];
        void* post_data[MAX_CALLBACKS];
        int num_post = 0;

        for (int i = 0; i < MAX_CALLBACKS; i++) {
            if (orchestrator->post_update_callbacks[i].active) {
                post_cbs[num_post] = orchestrator->post_update_callbacks[i].callback;
                post_data[num_post] = orchestrator->post_update_callbacks[i].user_data;
                num_post++;
            }
        }

        orchestrator->stats.total_updates++;
        orchestrator->stats.last_update_ms = orchestrator->current_time_ms;

        nimcp_platform_mutex_unlock(orchestrator->mutex);

        for (int i = 0; i < num_post; i++) {
            post_cbs[i](orchestrator, delta_ms, post_data[i]);
        }
    }

    return 0;
}
