// nimcp_mirror_neurons_part_accessors.c - accessors functions
// Part of nimcp_mirror_neurons.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_mirror_neurons.c


//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get default configuration
 *
 * WHAT: Return sensible default configuration
 * WHY:  Provide good starting point for most use cases
 * HOW:  Return pre-configured struct
 */
mirror_neuron_config_t mirror_neurons_get_default_config(void)
{
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_get_default_config", 0.0f);


    mirror_neuron_config_t config = {
        .num_mirror_neurons = 1000,
        .max_actions = 100,
        .max_agents = 10,
        .learning_rate = 0.01F,
        .decay_rate = 0.05F,
        .match_threshold = 0.7F,
        .enable_working_memory = true,
        .enable_theory_of_mind = true,
        .enable_prediction = true,
        .observation_window = 1000,
        .replay_capacity = 100,
        // Phase 10.11.1: Glial cell support (default: all enabled)
        .enable_glial_modulation = true,
        .enable_astrocytes = true,
        .enable_oligodendrocytes = true,
        .enable_microglia = true,
        // Phase 10.11.2: Substrate integration (default: disabled for backward compat)
        .enable_substrate = false,
        .enable_myelination = true,
        .enable_dendrite_plasticity = true,
        .enable_axon_timing = true,
        .enable_substrate_pool = true,
        .substrate_pool_size = NIMCP_MIRROR_SUBSTRATE_POOL_SIZE
    };
    return config;
}


/**
 * @brief Set brain reference for neuromodulator integration
 *
 * WHAT: Associate mirror neurons with brain for ACh modulation
 * WHY:  Enable acetylcholine-gated social learning
 * HOW:  Store brain reference in system structure
 *
 * COMPLEXITY: O(1)
 *
 * @param mirror Mirror neuron system
 * @param brain Brain handle (can be NULL to disable neuromodulation)
 */
void mirror_neurons_set_brain(mirror_neurons_t mirror, brain_t brain)
{
    // Guard: Validate mirror system
    if (!mirror) {
        MIRROR_LOG_ERROR("Mirror neurons: NULL system in set_brain");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_set_brain", 0.0f);


    mirror->brain = brain;
}


/**
 * @brief Get activation for action
 */
float mirror_neurons_get_activation(mirror_neurons_t mirror, uint32_t action_id)
{
    if (!mirror || !mirror->initialized) {
        return -1.0F;
    }

    // Find action
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_get_activation", 0.0f);


    uint32_t action_idx = UINT32_MAX;
    for (uint32_t i = 0; i < mirror->num_actions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mirror->num_actions > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)mirror->num_actions);
        }

        if (mirror->actions[i].action_id == action_id) {
            action_idx = i;
            break;
        }
    }

    if (action_idx == UINT32_MAX) {
        return -1.0F;  // Action not found
    }

    // Sum activations across neurons
    float total_activation = 0.0F;
    action_mapping_t* mapping = &mirror->actions[action_idx];

    for (uint32_t i = 0; i < mapping->num_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mapping->num_neurons > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)mapping->num_neurons);
        }

        uint32_t neuron_idx = mapping->neuron_indices[i];
        mirror_neuron_unit_t* neuron = &mirror->neurons[neuron_idx];

        // Combined activation (max of observation and execution)
        float activation = (neuron->observation_activation > neuron->execution_activation) ?
                          neuron->observation_activation : neuron->execution_activation;
        total_activation += activation;
    }

    // Average activation
    if (mapping->num_neurons > 0) {
        return total_activation / mapping->num_neurons;
    }

    return 0.0F;
}


//=============================================================================
// Query & Analysis API Implementation
//=============================================================================

/**
 * @brief Get system statistics
 */
bool mirror_neurons_get_stats(mirror_neurons_t mirror, mirror_neuron_stats_t* stats)
{
    if (!mirror || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_neurons_get_stats: required parameter is NULL (mirror, stats)");
        return false;
    }

    // Copy current statistics
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_get_stats", 0.0f);


    memcpy(stats, &mirror->stats, sizeof(mirror_neuron_stats_t));

    // Update dynamic metrics
    stats->num_active_neurons = 0;
    for (uint32_t i = 0; i < mirror->num_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mirror->num_neurons > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)mirror->num_neurons);
        }

        if (mirror->neurons[i].observation_activation > 0.01F ||
            mirror->neurons[i].execution_activation > 0.01F) {
            stats->num_active_neurons++;
        }
    }

    stats->num_learned_actions = mirror->num_actions;
    stats->num_observed_agents = mirror->num_agents;

    // Calculate average match quality
    float total_quality = 0.0F;
    uint32_t count = 0;
    for (uint32_t i = 0; i < mirror->num_actions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mirror->num_actions > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)mirror->num_actions);
        }

        if (mirror->actions[i].avg_similarity > 0.0F) {
            total_quality += mirror->actions[i].avg_similarity;
            count++;
        }
    }
    stats->avg_match_quality = (count > 0) ? (total_quality / count) : 0.0F;

    stats->last_update_time = mirror->last_update_time;

    return true;
}


/**
 * @brief Get activation record for action
 */
bool mirror_neurons_get_activation_record(
    mirror_neurons_t mirror,
    uint32_t action_id,
    mirror_activation_t* activation)
{
    if (!mirror || !activation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_neurons_get_activation_record: required parameter is NULL (mirror, activation)");
        return false;
    }

    // Find action
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_get_activation_recor", 0.0f);


    uint32_t action_idx = UINT32_MAX;
    for (uint32_t i = 0; i < mirror->num_actions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mirror->num_actions > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)mirror->num_actions);
        }

        if (mirror->actions[i].action_id == action_id) {
            action_idx = i;
            break;
        }
    }

    if (action_idx == UINT32_MAX) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_get_activation_record: validation failed");
        return false;
    }

    action_mapping_t* mapping = &mirror->actions[action_idx];

    // Aggregate activation across neurons
    activation->action_id = action_id;
    activation->observation_activation = 0.0F;
    activation->execution_activation = 0.0F;
    activation->association_strength = 0.0F;
    activation->observation_count = mapping->total_observations;
    activation->execution_count = mapping->total_executions;
    activation->last_activation = 0;

    if (mapping->num_neurons == 0) {
        return true;  // Valid but no neurons yet
    }

    // Average across neurons
    for (uint32_t i = 0; i < mapping->num_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mapping->num_neurons > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)mapping->num_neurons);
        }

        uint32_t neuron_idx = mapping->neuron_indices[i];
        mirror_neuron_unit_t* neuron = &mirror->neurons[neuron_idx];

        activation->observation_activation += neuron->observation_activation;
        activation->execution_activation += neuron->execution_activation;
        activation->association_strength += neuron->association_weight;

        if (neuron->last_observation_time > activation->last_activation) {
            activation->last_activation = neuron->last_observation_time;
        }
        if (neuron->last_execution_time > activation->last_activation) {
            activation->last_activation = neuron->last_execution_time;
        }
    }

    activation->observation_activation /= mapping->num_neurons;
    activation->execution_activation /= mapping->num_neurons;
    activation->association_strength /= mapping->num_neurons;

    return true;
}


//=============================================================================
// Bidirectional Feedback Functions (Phase 10.11.3)
//=============================================================================

/**
 * @brief Get social salience for current context
 *
 * WHAT: Query importance of social cues
 * WHY:  Visual cortex can boost attention to social stimuli
 * HOW:  Return agent detection confidence × observation activation
 *
 * BIOLOGY: STS (superior temporal sulcus) modulates V1 for social stimuli
 *          High social salience → enhanced processing of faces, biological motion
 *
 * COMPLEXITY: O(n) where n = number of actions
 *
 * @param mirror Mirror neuron system
 * @return Social salience [0, 1]
 */
float mirror_neurons_get_social_salience(mirror_neurons_t mirror)
{
    // Guard: Validate mirror system
    if (!mirror || !mirror->initialized) {
        return 0.0F;
    }

    // WHAT: Compute average activation across all mirror neurons
    // WHY:  High activation suggests social context is salient
    // HOW:  Average observation activation across neurons
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_get_social_salience", 0.0f);


    float total_activation = 0.0F;
    uint32_t active_count = 0;

    for (uint32_t i = 0; i < mirror->num_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mirror->num_neurons > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)mirror->num_neurons);
        }

        mirror_neuron_unit_t* neuron = &mirror->neurons[i];
        if (neuron->observation_activation > 0.01F) {
            total_activation += neuron->observation_activation;
            active_count++;
        }
    }

    if (active_count == 0) {
        return 0.0F;
    }

    float avg_activation = total_activation / active_count;

    // Scale by number of observed agents
    float agent_factor = (mirror->num_agents > 0) ? 1.0F : 0.5F;

    // Combine activation and agent presence
    float social_salience = avg_activation * agent_factor;

    // Clamp to [0, 1]
    return fminf(fmaxf(social_salience, 0.0F), 1.0F);
}


/**
 * @brief Check for recent observations
 *
 * WHAT: Determine if observations occurred recently
 * WHY:  Enable Theory of Mind predictions
 * HOW:  Compare last update time with current time
 *
 * COMPLEXITY: O(1)
 */
bool mirror_neurons_has_recent_observations(mirror_neurons_t mirror)
{
    // Guard: Validate mirror system
    if (!mirror) {
        return false;
    }

    // Check if never updated
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_has_recent_observati", 0.0f);


    if (mirror->last_update_time == 0) {
        return false;
    }

    // Check if observation within last 5 seconds (5000ms)
    uint64_t current_time = nimcp_time_get_ms();
    uint64_t time_since_last = current_time - mirror->last_update_time;

    return time_since_last < 5000;
}


/**
 * @brief Get all mirror neuron activations for ToM integration
 *
 * WHAT: Extract activation patterns across all mirror neurons
 * WHY:  Enable Theory of Mind to infer intentions from observed actions
 * HOW:  Aggregate observation + execution activations per action
 *
 * BIOLOGICAL RATIONALE:
 * Mirror neurons encode both observed and executed actions, enabling
 * understanding of others' intentions (Rizzolatti & Craighero, 2004).
 * ToM uses this shared representation to infer "why" from "what."
 *
 * COMPLEXITY: O(n) where n = num_actions
 */
bool mirror_neurons_get_all_activations(
    mirror_neurons_t mirror,
    float* activations,
    uint32_t max_size,
    uint32_t* out_size)
{
    // Guard: Validate inputs
    if (!mirror || !activations || !out_size) {
        if (out_size) *out_size = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_get_all_activations: validation failed");
        return false;
    }

    // Guard: Check initialized
    if (!mirror->initialized) {
        MIRROR_LOG_ERROR("Mirror neurons: system not initialized");
        *out_size = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_neurons_get_all_activations: mirror->initialized is NULL");
        return false;
    }

    // WHAT: Compute combined activation for each action
    // WHY:  ToM needs overall action strength, not separate obs/exec
    // HOW:  Average observation and execution activations per action

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_get_all_activations", 0.0f);


    uint32_t count = 0;
    for (uint32_t i = 0; i < mirror->num_actions && count < max_size; i++) {
        action_mapping_t* action = &mirror->actions[i];

        // Sum activations across all neurons for this action
        float obs_activation = 0.0F;
        float exec_activation = 0.0F;
        uint32_t neuron_count = 0;

        for (uint32_t j = 0; j < action->num_neurons; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && action->num_neurons > 256) {
                mirror_neurons_heartbeat("mirror_neuro_loop",
                                 (float)(j + 1) / (float)action->num_neurons);
            }

            uint32_t neuron_idx = action->neuron_indices[j];
            if (neuron_idx < mirror->num_neurons) {
                mirror_neuron_unit_t* neuron = &mirror->neurons[neuron_idx];
                obs_activation += neuron->observation_activation;
                exec_activation += neuron->execution_activation;
                neuron_count++;
            }
        }

        // Average and combine (observation + execution weighted)
        // Higher weight on observation since ToM focuses on observing others
        if (neuron_count > 0) {
            float avg_obs = obs_activation / neuron_count;
            float avg_exec = exec_activation / neuron_count;
            // Weight: 70% observation, 30% execution (empathy focus)
            activations[count] = 0.7F * avg_obs + 0.3F * avg_exec;
            count++;
        }
    }

    *out_size = count;
    return true;
}


/**
 * @brief Get recognition delay for action (substrate-aware)
 *
 * WHAT: Calculate delay for action recognition including substrate effects
 * WHY:  Myelination and axon properties affect recognition speed
 * HOW:  Query substrate backing for delay, fall back to base if no substrate
 */
float mirror_neurons_get_recognition_delay(mirror_neurons_t mirror, uint32_t action_id)
{
    if (!mirror) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_get_recognition_delay: mirror is NULL");
        return NIMCP_MIRROR_BASE_DELAY_MS;
    }

    /* Find action mapping */
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_get_recognition_dela", 0.0f);


    for (uint32_t i = 0; i < mirror->num_actions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mirror->num_actions > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)mirror->num_actions);
        }

        if (mirror->actions[i].action_id == action_id) {
            /* Get average delay across neurons for this action */
            float total_delay = 0.0F;
            uint32_t count = 0;

            for (uint32_t j = 0; j < mirror->actions[i].num_neurons; j++) {
                uint32_t neuron_idx = mirror->actions[i].neuron_indices[j];
                if (neuron_idx < mirror->num_neurons) {
                    mirror_neuron_unit_t* unit = &mirror->neurons[neuron_idx];

                    if (unit->has_substrate && unit->substrate) {
                        total_delay += mirror_substrate_get_observation_delay(unit->substrate);
                    } else {
                        total_delay += NIMCP_MIRROR_BASE_DELAY_MS;
                    }
                    count++;
                }
            }

            return (count > 0) ? (total_delay / count) : NIMCP_MIRROR_BASE_DELAY_MS;
        }
    }

    return NIMCP_MIRROR_BASE_DELAY_MS;
}


/**
 * @brief Get association strength from spine weights
 *
 * WHAT: Query observation-execution association based on spine plasticity
 * WHY:  Spines encode learned associations in substrate mode
 * HOW:  Sum spine weights for the action's mirror units
 */
float mirror_neurons_get_spine_association(mirror_neurons_t mirror, uint32_t action_id)
{
    if (!mirror) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_get_spine_association: mirror is NULL");
        return 0.0F;
    }
    if (!mirror->substrate_enabled) return 0.0F;

    /* Find action mapping */
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_get_spine_associatio", 0.0f);


    for (uint32_t i = 0; i < mirror->num_actions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mirror->num_actions > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)mirror->num_actions);
        }

        if (mirror->actions[i].action_id == action_id) {
            float total_weight = 0.0F;

            for (uint32_t j = 0; j < mirror->actions[i].num_neurons; j++) {
                uint32_t neuron_idx = mirror->actions[i].neuron_indices[j];
                if (neuron_idx < mirror->num_neurons) {
                    mirror_neuron_unit_t* unit = &mirror->neurons[neuron_idx];

                    if (unit->has_substrate && unit->substrate) {
                        total_weight += mirror_substrate_get_total_spine_weight(unit->substrate);
                    }
                }
            }

            return total_weight;
        }
    }

    return 0.0F;
}


/**
 * @brief Check if substrate is enabled
 */
bool mirror_neurons_has_substrate(mirror_neurons_t mirror)
{
    if (!mirror) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_has_substrate", 0.0f);


    return mirror->substrate_enabled;
}


/**
 * @brief Get STDP system handle
 */
mirror_stdp_t mirror_neurons_get_stdp(mirror_neurons_t mirror)
{
    if (!mirror) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_get_stdp: mirror is NULL");
        return NULL;
    }
    if (!mirror->stdp_enabled) return NULL;
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_get_stdp", 0.0f);


    return (mirror_stdp_t)mirror->stdp_system;
}


/**
 * @brief Set dopamine level for STDP learning
 */
void mirror_neurons_set_stdp_dopamine(mirror_neurons_t mirror, float level)
{
    if (!mirror) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_set_stdp_dopamine: mirror is NULL");
        return;
    }
    if (!mirror->stdp_enabled || !mirror->stdp_system) return;

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_set_stdp_dopamine", 0.0f);


    mirror_stdp_set_dopamine((mirror_stdp_t)mirror->stdp_system, level);
}


/**
 * @brief Get motor resonance system handle
 */
motor_resonance_t mirror_neurons_get_resonance(mirror_neurons_t mirror)
{
    if (!mirror) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_get_resonance: mirror is NULL");
        return NULL;
    }
    if (!mirror->resonance_enabled) return NULL;
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_get_resonance", 0.0f);


    return (motor_resonance_t)mirror->resonance_system;
}


/**
 * @brief Set learning context for motor resonance
 */
void mirror_neurons_set_learning_context(mirror_neurons_t mirror, float learning_strength)
{
    if (!mirror) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_set_learning_context: mirror is NULL");
        return;
    }
    if (!mirror->resonance_enabled || !mirror->resonance_system) return;

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_set_learning_context", 0.0f);


    motor_resonance_release_for_learning((motor_resonance_t)mirror->resonance_system,
                                          -1, learning_strength);
}


/**
 * @brief Set social context for motor resonance
 */
void mirror_neurons_set_social_context(mirror_neurons_t mirror, float social_strength)
{
    if (!mirror) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_set_social_context: mirror is NULL");
        return;
    }
    if (!mirror->resonance_enabled || !mirror->resonance_system) return;

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_set_social_context", 0.0f);


    motor_resonance_release_for_social((motor_resonance_t)mirror->resonance_system,
                                        -1, social_strength);
}


/**
 * @brief Get hierarchy system handle
 */
mirror_hierarchy_t mirror_neurons_get_hierarchy(mirror_neurons_t mirror)
{
    if (!mirror) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_get_hierarchy: mirror is NULL");
        return NULL;
    }
    if (!mirror->hierarchy_enabled) return NULL;
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_get_hierarchy", 0.0f);


    return (mirror_hierarchy_t)mirror->hierarchy_system;
}


/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void mirror_neurons_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_mirror_neurons_health_agent = agent;
    }
}
