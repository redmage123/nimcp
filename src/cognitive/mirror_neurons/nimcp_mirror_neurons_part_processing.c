// nimcp_mirror_neurons_part_processing.c - processing functions
// Part of nimcp_mirror_neurons.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_mirror_neurons.c


//=============================================================================
// KG-Driven Wiring Callback
//=============================================================================

/**
 * @brief Wiring callback for KG-driven handler registration
 *
 * WHAT: Callback invoked by orchestrator with discovered message types
 * WHY:  Enable dynamic handler registration based on KG wiring diagram
 * HOW:  Register handlers for discovered message types from handler map
 *
 * @param ctx Bio-async module context
 * @param message_types Array of discovered message types from KG
 * @param message_count Number of message types
 * @param user_data User-provided context (mirror_neurons_t)
 * @return 0 on success, -1 on error
 */
static int mirror_neurons_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    mirror_neurons_t mirror = (mirror_neurons_t)user_data;
    if (!mirror) {
        MIRROR_LOG_WARN("Wiring callback invoked with NULL user_data");
        return -1;
    }

    int registered = 0;
    for (uint32_t i = 0; i < message_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && message_count > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)message_count);
        }

        switch (message_types[i]) {
            case BIO_MSG_MIRROR_NEURON_ACTIVATION:
                bio_router_register_handler(ctx, message_types[i], handle_mirror_activation);
                registered++;
                break;
            default:
                LOG_DEBUG(LOG_MODULE, "Unknown message type %d in wiring callback", message_types[i]);
                break;
        }
    }

    MIRROR_LOG_INFO("KG-driven wiring callback registered %d handlers", registered);
    return (registered > 0) ? 0 : -1;
}


/**
 * @brief Process self-executed action
 */
bool mirror_neurons_execute_action(mirror_neurons_t mirror, const action_t* action)
{
    if (!mirror || !mirror->initialized) {
        MIRROR_LOG_ERROR("Mirror neurons: invalid system handle");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_neurons_execute_action: required parameter is NULL (mirror, mirror->initialized)");
        return false;
    }

    if (!action) {
        MIRROR_LOG_ERROR("Mirror neurons: null action");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_neurons_execute_action: action is NULL");
        return false;
    }

    // Find or create action mapping
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_execute_action", 0.0f);


    uint32_t action_idx = find_or_create_action(mirror, action);
    if (action_idx == UINT32_MAX) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_execute_action: validation failed");
        return false;
    }

    // Activate execution pathway
    float activation_strength = action->confidence;
    activate_neurons_for_action(mirror, action_idx, activation_strength, false);

    // Update statistics
    mirror->actions[action_idx].total_executions++;
    mirror->stats.total_executions++;
    mirror->stats.num_learned_actions = mirror->num_actions;

    update_action_statistics(mirror, action_idx);

    mirror->last_update_time = nimcp_time_get_ms();

    return true;
}


/**
 * @brief Update association strengths
 */
bool mirror_neurons_update_associations(mirror_neurons_t mirror)
{
    if (!mirror || !mirror->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_neurons_update_associations: required parameter is NULL (mirror, mirror->initialized)");
        return false;
    }

    // Apply Hebbian-like learning: neurons that fire together, wire together
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_update_associations", 0.0f);


    for (uint32_t i = 0; i < mirror->num_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mirror->num_neurons > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)mirror->num_neurons);
        }

        mirror_neuron_unit_t* neuron = &mirror->neurons[i];

        if (neuron->action_id == 0) {
            continue;  // Unassigned neuron
        }

        // Calculate co-activation
        float obs_act = neuron->observation_activation;
        float exec_act = neuron->execution_activation;

        if (obs_act > 0.0F && exec_act > 0.0F) {
            // Both pathways active - strengthen association
            float delta = mirror->config.learning_rate * obs_act * exec_act;
            neuron->association_weight += delta;

            // Bound to [0, 1]
            if (neuron->association_weight > 1.0F) {
                neuron->association_weight = 1.0F;
            }
        } else {
            // Weak decay if no co-activation
            neuron->association_weight *= 0.99F;
        }
    }

    // Update action-level statistics
    for (uint32_t i = 0; i < mirror->num_actions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mirror->num_actions > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)mirror->num_actions);
        }

        update_action_statistics(mirror, i);
    }

    return true;
}


/**
 * @brief Step substrate simulation forward
 *
 * WHAT: Advance all substrate states by one timestep
 * WHY:  Keep substrate synchronized with simulation time
 * HOW:  Update myelination, spine plasticity, glial states for each unit
 */
bool mirror_neurons_step_substrate(mirror_neurons_t mirror, float dt_ms)
{
    if (!mirror) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_step_substrate: mirror is NULL");
        return false;
    }
    if (!mirror->substrate_enabled) return false;

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_step_substrate", 0.0f);


    uint64_t current_time = nimcp_time_get_us();
    float dt_seconds = dt_ms / 1000.0F;

    /* Step each neuron's substrate */
    for (uint32_t i = 0; i < mirror->num_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mirror->num_neurons > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)mirror->num_neurons);
        }

        mirror_neuron_unit_t* unit = &mirror->neurons[i];

        if (unit->has_substrate && unit->substrate) {
            mirror_substrate_step(unit->substrate, current_time, dt_seconds);
        }
    }

    return true;
}


/**
 * @brief Step all enhancement systems
 */
bool mirror_neurons_step_enhancements(mirror_neurons_t mirror, float dt_ms)
{
    if (!mirror) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_step_enhancements: mirror is NULL");
        return false;
    }

    // Step STDP system
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_step_enhancements", 0.0f);


    if (mirror->stdp_enabled && mirror->stdp_system) {
        mirror_stdp_step((mirror_stdp_t)mirror->stdp_system, dt_ms);
    }

    // Step resonance system
    if (mirror->resonance_enabled && mirror->resonance_system) {
        motor_resonance_step((motor_resonance_t)mirror->resonance_system, dt_ms);
    }

    // Step hierarchy system
    if (mirror->hierarchy_enabled && mirror->hierarchy_system) {
        mirror_hierarchy_step((mirror_hierarchy_t)mirror->hierarchy_system, dt_ms);
    }

    return true;
}


int mirror_neurons_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_neurons_training_step: NULL argument");
        return -1;
    }
    mirror_neurons_heartbeat_instance(NULL, "mirror_neurons_training_step", progress);
    return 0;
}
