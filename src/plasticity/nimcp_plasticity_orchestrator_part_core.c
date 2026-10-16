// nimcp_plasticity_orchestrator_part_core.c - core functions
// Part of nimcp_plasticity_orchestrator.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_plasticity_orchestrator.c

/* Eligibility trace decay factor per reward event (95% retention).
 * Matches eligibility_default_config().decay_lambda from nimcp_eligibility_trace.h */
#define ORCHESTRATOR_ELIGIBILITY_DECAY  0.95f

/* ============================================================================
 * Integration Functions
 * ============================================================================ */

int plasticity_orchestrator_connect_immune(
    plasticity_orchestrator_t* orchestrator,
    struct brain_immune_system* immune
) {
    if (!orchestrator || !immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL pointer in connect_immune");
        NIMCP_LOGGING_ERROR("NULL pointer in connect_immune");
        return -1;
    }

    nimcp_platform_mutex_lock(orchestrator->mutex);
    orchestrator->immune = immune;
    nimcp_platform_mutex_unlock(orchestrator->mutex);

    NIMCP_LOGGING_INFO("Connected orchestrator to immune system");
    return 0;
}


int plasticity_orchestrator_connect_sleep(
    plasticity_orchestrator_t* orchestrator,
    struct sleep_system_struct* sleep
) {
    if (!orchestrator || !sleep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL pointer in connect_sleep");
        NIMCP_LOGGING_ERROR("NULL pointer in connect_sleep");
        return -1;
    }

    nimcp_platform_mutex_lock(orchestrator->mutex);
    orchestrator->sleep = sleep;
    nimcp_platform_mutex_unlock(orchestrator->mutex);

    NIMCP_LOGGING_INFO("Connected orchestrator to sleep system");
    return 0;
}


int plasticity_orchestrator_connect_neuromodulators(
    plasticity_orchestrator_t* orchestrator,
    struct neuromodulator_system_struct* neuromod
) {
    if (!orchestrator || !neuromod) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL pointer in connect_neuromodulators");
        NIMCP_LOGGING_ERROR("NULL pointer in connect_neuromodulators");
        return -1;
    }

    nimcp_platform_mutex_lock(orchestrator->mutex);
    orchestrator->neuromod = neuromod;
    nimcp_platform_mutex_unlock(orchestrator->mutex);

    NIMCP_LOGGING_INFO("Connected orchestrator to neuromodulator system");
    return 0;
}


int plasticity_orchestrator_connect_bio_async(plasticity_orchestrator_t* orchestrator) {
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Orchestrator is NULL in connect_bio_async");
        return -1;
    }

    /* BUG-27 fix: Lock mutex around state mutation, matching the pattern
     * of connect_immune, connect_sleep, and connect_neuromodulators */
    nimcp_platform_mutex_lock(orchestrator->mutex);
    orchestrator->bio_async_connected = true;
    nimcp_platform_mutex_unlock(orchestrator->mutex);
    NIMCP_LOGGING_INFO("Connected orchestrator to bio-async router");
    return 0;
}


/* ============================================================================
 * Callback Registration
 * ============================================================================ */

int plasticity_orchestrator_register_event_callback(
    plasticity_orchestrator_t* orchestrator,
    plasticity_event_type_t event_type,
    plasticity_event_callback_t callback,
    void* user_data
) {
    if (!orchestrator || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL pointer in register_event_callback");
        return -1;
    }

    nimcp_platform_mutex_lock(orchestrator->mutex);

    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (!orchestrator->event_callbacks[i].active) {
            orchestrator->event_callbacks[i].callback = callback;
            orchestrator->event_callbacks[i].user_data = user_data;
            orchestrator->event_callbacks[i].event_type = event_type;
            orchestrator->event_callbacks[i].id = orchestrator->next_callback_id++;
            orchestrator->event_callbacks[i].active = true;

            int id = orchestrator->event_callbacks[i].id;
            nimcp_platform_mutex_unlock(orchestrator->mutex);
            return id;
        }
    }

    nimcp_platform_mutex_unlock(orchestrator->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "plasticity_orchestrator_register_event_callback: operation failed");
    return -1;
}


int plasticity_orchestrator_unregister_event_callback(
    plasticity_orchestrator_t* orchestrator,
    int callback_id
) {
    if (!orchestrator || callback_id <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Invalid parameters in unregister_event_callback");
        return -1;
    }

    nimcp_platform_mutex_lock(orchestrator->mutex);

    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (orchestrator->event_callbacks[i].active &&
            orchestrator->event_callbacks[i].id == callback_id) {
            orchestrator->event_callbacks[i].active = false;
            nimcp_platform_mutex_unlock(orchestrator->mutex);
            return 0;
        }
    }

    nimcp_platform_mutex_unlock(orchestrator->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "plasticity_orchestrator_unregister_event_callback: operation failed");
    return -1;
}


/* ============================================================================
 * Spike Event Handling
 * ============================================================================ */

int plasticity_orchestrator_pre_spike(
    plasticity_orchestrator_t* orchestrator,
    uint32_t synapse_id,
    uint64_t timestamp_ms
) {
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Orchestrator is NULL in pre_spike");
        return -1;
    }

    nimcp_platform_mutex_lock(orchestrator->mutex);

    synapse_entry_t* syn = get_or_create_synapse(orchestrator, synapse_id);
    if (!syn) {
        nimcp_platform_mutex_unlock(orchestrator->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plasticity_orchestrator_pre_spike: syn is NULL");
        return -1;
    }

    /* Save old spike time before overwriting for ISI computation */
    uint64_t prev_spike_time = syn->last_pre_spike_time;
    syn->last_pre_spike_time = timestamp_ms;
    syn->total_pre_spikes++;

    /* Update activity estimate */
    float time_since_last = (float)(timestamp_ms - prev_spike_time);
    /* BUG-24 fix: Clamp to minimum 1ms to prevent division by zero for
     * same-millisecond spikes and avoid Inf from float precision issues */
    if (time_since_last > 0) {
        if (time_since_last < 1.0f) time_since_last = 1.0f;
        syn->recent_activity_hz = 1000.0f / time_since_last;  /* Convert to Hz */
    }

    /* Check metabolic constraints */
    bool can_ltp = true;
    bool can_ltd = true;
    if (orchestrator->config.enabled.enable_metabolic && orchestrator->metabolic) {
        can_ltp = metabolic_plasticity_can_ltp(orchestrator->metabolic);
        can_ltd = metabolic_plasticity_can_ltd(orchestrator->metabolic);
    }

    /* Compute modulation factor from sleep and immune states */
    float modulation = orchestrator->sleep_modulation_factor *
                       orchestrator->immune_modulation_factor;

    /* Process through triplet STDP if enabled */
    if (orchestrator->config.enabled.enable_triplet_stdp && syn->triplet_stdp && can_ltd) {
        float old_weight = syn->triplet_stdp->weight;
        triplet_stdp_pre_spike(syn->triplet_stdp, (float)timestamp_ms);

        float raw_dw = syn->triplet_stdp->weight - old_weight;
        float dw = raw_dw * modulation;  /* Scale by modulation factor */

        if (raw_dw < -0.001f) {
            /* LTD occurred - apply modulated weight change */
            syn->triplet_stdp->weight = old_weight + dw;
            syn->weight = syn->triplet_stdp->weight;
            syn->ltd_accumulator += fabsf(dw);
            orchestrator->stats.ltd_count++;
            emit_event(orchestrator, PLASTICITY_EVENT_LTD,
                      synapse_id, 0, old_weight, syn->triplet_stdp->weight);

            /* Consume ATP for LTD */
            if (orchestrator->metabolic) {
                metabolic_plasticity_consume_atp(orchestrator->metabolic, METABOLIC_EVENT_LTD, fabsf(dw));
            }
        }
    }

    /* Trigger calcium influx */
    if (orchestrator->config.enabled.enable_calcium && orchestrator->calcium) {
        calcium_trigger_nmda_influx(orchestrator->calcium, 0.5f, -30.0f);
    }

    /* Update structural plasticity activity */
    if (orchestrator->config.enabled.enable_structural && orchestrator->structural && syn->structural_synapse_id > 0) {
        structural_plasticity_update_activity(orchestrator->structural, syn->structural_synapse_id, timestamp_ms);
    }

    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return 0;
}


int plasticity_orchestrator_post_spike(
    plasticity_orchestrator_t* orchestrator,
    uint32_t neuron_id,
    uint64_t timestamp_ms
) {
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Orchestrator is NULL in post_spike");
        return -1;
    }

    nimcp_platform_mutex_lock(orchestrator->mutex);

    neuron_entry_t* neuron = get_or_create_neuron(orchestrator, neuron_id);
    if (!neuron) {
        nimcp_platform_mutex_unlock(orchestrator->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plasticity_orchestrator_post_spike: neuron is NULL");
        return -1;
    }

    /* Update neuron firing rate estimate */
    neuron->firing_rate = (neuron->firing_rate * NIMCP_EMA_WEIGHT_SLOW) + NIMCP_EMA_WEIGHT_FAST;

    /* Check metabolic constraints */
    bool can_ltp = true;
    if (orchestrator->config.enabled.enable_metabolic && orchestrator->metabolic) {
        can_ltp = metabolic_plasticity_can_ltp(orchestrator->metabolic);
    }

    /* Get calcium-dependent learning rate if enabled */
    float ca_lr = 1.0f;
    if (orchestrator->config.enabled.enable_calcium && orchestrator->calcium) {
        ca_lr = calcium_compute_learning_rate(orchestrator->calcium);
    }

    /* Compute modulation factor from sleep and immune states */
    float modulation = orchestrator->sleep_modulation_factor *
                       orchestrator->immune_modulation_factor;

    /* Process LTP for all active synapses */
    for (size_t i = 0; i < orchestrator->num_synapses; i++) {
        synapse_entry_t* syn = &orchestrator->synapses[i];
        if (!syn->active) continue;

        if (orchestrator->config.enabled.enable_triplet_stdp && syn->triplet_stdp && can_ltp) {
            float old_weight = syn->triplet_stdp->weight;
            triplet_stdp_post_spike(syn->triplet_stdp, (float)timestamp_ms);

            float raw_dw = syn->triplet_stdp->weight - old_weight;
            float dw = raw_dw * modulation;  /* Scale by modulation factor */

            if (raw_dw > 0.001f) {
                /* LTP occurred - apply modulated weight change */
                syn->triplet_stdp->weight = old_weight + dw;
                syn->weight = syn->triplet_stdp->weight;
                syn->ltp_accumulator += dw;
                orchestrator->stats.ltp_count++;
                emit_event(orchestrator, PLASTICITY_EVENT_LTP,
                          syn->id, neuron_id, old_weight, syn->triplet_stdp->weight);

                /* Consume ATP for LTP */
                if (orchestrator->metabolic) {
                    metabolic_plasticity_consume_atp(orchestrator->metabolic, METABOLIC_EVENT_LTP, dw);
                }

                /* Apply heterosynaptic depression to neighbors */
                if (orchestrator->config.enabled.enable_heterosynaptic && orchestrator->heterosynaptic) {
                    hetero_apply_depression(orchestrator->heterosynaptic, syn->id, dw, timestamp_ms);
                }

                /* Set synaptic tag if strong enough */
                if (dw > 0.1f && orchestrator->config.enabled.enable_protein_synthesis &&
                    orchestrator->protein_synthesis) {
                    protein_synthesis_set_tag(orchestrator->protein_synthesis, syn->id, dw);
                    syn->consolidation_tagged = true;
                }
            }
        }
    }

    /* Trigger calcium spike for postsynaptic event */
    if (orchestrator->config.enabled.enable_calcium && orchestrator->calcium) {
        calcium_set_concentration(orchestrator->calcium, 1.0f);  /* Strong Ca spike */
        emit_event(orchestrator, PLASTICITY_EVENT_CALCIUM_SPIKE, 0, neuron_id, 0.1f, 1.0f);
    }

    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return 0;
}


int plasticity_orchestrator_reward(
    plasticity_orchestrator_t* orchestrator,
    float reward_magnitude,
    uint64_t timestamp_ms
) {
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Orchestrator is NULL in reward");
        return -1;
    }

    nimcp_platform_mutex_lock(orchestrator->mutex);

    /* Update dopamine level based on reward */
    orchestrator->neuromod_levels.dopamine = fmaxf(0.0f, fminf(1.0f,
        orchestrator->neuromod_levels.dopamine + reward_magnitude * 0.3f));

    /* Apply eligibility trace-based weight updates */
    if (orchestrator->config.enabled.enable_eligibility) {
        for (size_t i = 0; i < orchestrator->num_synapses; i++) {
            synapse_entry_t* syn = &orchestrator->synapses[i];
            if (!syn->active) continue;

            /* BUG-26 fix: Use fabsf to catch negative eligibility traces (LTD) */
            if (fabsf(syn->eligibility_trace) > 0.01f) {
                float dw = syn->eligibility_trace * reward_magnitude *
                          orchestrator->config.global_learning_rate;

                float old_weight = syn->weight;
                syn->weight = fmaxf(syn->w_min, fminf(syn->w_max, syn->weight + dw));

                if (syn->triplet_stdp) {
                    syn->triplet_stdp->weight = syn->weight;
                }

                if (dw > 0) {
                    emit_event(orchestrator, PLASTICITY_EVENT_LTP,
                              syn->id, 0, old_weight, syn->weight);
                } else if (dw < 0) {
                    emit_event(orchestrator, PLASTICITY_EVENT_LTD,
                              syn->id, 0, old_weight, syn->weight);
                }

                /* Decay eligibility trace */
                syn->eligibility_trace *= ORCHESTRATOR_ELIGIBILITY_DECAY;
            }
        }
    }

    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return 0;
}
