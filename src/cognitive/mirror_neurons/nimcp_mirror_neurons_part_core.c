// nimcp_mirror_neurons_part_core.c - core functions
// Part of nimcp_mirror_neurons.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_mirror_neurons.c


//=============================================================================
// Core API Implementation - Action Processing
//=============================================================================

/**
 * @brief Process observed action
 */
bool mirror_neurons_observe_action(mirror_neurons_t mirror, const action_t* action)
{
    if (!mirror || !mirror->initialized) {
        MIRROR_LOG_ERROR("Mirror neurons: invalid system handle");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_neurons_observe_action: required parameter is NULL (mirror, mirror->initialized)");
        return false;
    }

    if (!action) {
        MIRROR_LOG_ERROR("Mirror neurons: null action");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_neurons_observe_action: action is NULL");
        return false;
    }

    // Find or create action mapping
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_observe_action", 0.0f);


    uint32_t action_idx = find_or_create_action(mirror, action);
    if (action_idx == UINT32_MAX) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_observe_action: validation failed");
        return false;
    }

    // Track agent
    if (action->agent_id != 0) {  // 0 = self
        uint32_t agent_idx = find_or_create_agent(mirror, action->agent_id);
        if (agent_idx != UINT32_MAX) {
            mirror->agents[agent_idx].observation_count++;
            mirror->agents[agent_idx].last_observation_time = nimcp_time_get_ms();
        }
    }

    // Activate observation pathway
    float activation_strength = action->confidence;
    activate_neurons_for_action(mirror, action_idx, activation_strength, true);

    // Update statistics
    mirror->actions[action_idx].total_observations++;
    mirror->stats.total_observations++;
    mirror->stats.num_observed_agents = mirror->num_agents;
    mirror->stats.num_learned_actions = mirror->num_actions;

    update_action_statistics(mirror, action_idx);

    mirror->last_update_time = nimcp_time_get_ms();

    /* W10 (2026-04-24): emit mirror-activation event + query activity bias
     * (read-path for other cognitive modules to gauge observation state). */
    if (mirror->brain) {
        wave10_mirror_emit_activation(mirror->brain, action_idx,
                                      activation_strength);
        (void)wave10_mirror_query_activity_bias(mirror->brain);
    }

    // Broadcast mirror neuron activation via bio-async
    if (activation_strength > 0.5F) {
        bio_broadcast_mirror_fire(mirror, action->action_id, activation_strength);
    }

    return true;
}


/**
 * @brief Match actions
 */
bool mirror_neurons_match_actions(
    mirror_neurons_t mirror,
    const action_t* observed_action,
    const action_t* executed_action,
    float* out_similarity)
{
    if (!mirror || !observed_action || !executed_action) {
        if (out_similarity) *out_similarity = 0.0F;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_match_actions: validation failed");
        return false;
    }

    // Compute feature similarity
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_match_actions", 0.0f);


    uint32_t num_features = (observed_action->num_features < executed_action->num_features) ?
                           observed_action->num_features : executed_action->num_features;

    if (num_features == 0) {
        if (out_similarity) *out_similarity = 0.0F;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_match_actions: validation failed");
        return false;
    }

    float similarity = compute_feature_similarity(
        observed_action->features,
        executed_action->features,
        num_features
    );

    if (out_similarity) {
        *out_similarity = similarity;
    }

    // Match if above threshold
    return (similarity >= mirror->config.match_threshold);
}


//=============================================================================
// Learning & Adaptation API Implementation
//=============================================================================

/**
 * @brief Learn from action demonstration
 */
bool mirror_neurons_learn_demonstration(
    mirror_neurons_t mirror,
    const action_t* actions,
    uint32_t num_actions,
    uint32_t demonstrator_id)
{
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_learn_demonstration", 0.0f);


    if (!mirror || !actions || num_actions == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_neurons_learn_demonstration: required parameter is NULL (mirror, actions)");
        return false;
    }

    // Track demonstrator agent if non-self
    if (demonstrator_id != 0) {
        uint32_t agent_idx = find_or_create_agent(mirror, demonstrator_id);
        if (agent_idx != UINT32_MAX) {
            mirror->agents[agent_idx].observation_count += num_actions;
            mirror->agents[agent_idx].last_observation_time = nimcp_time_get_ms();
        }
    }

    // Process each action in sequence
    for (uint32_t i = 0; i < num_actions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_actions > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)num_actions);
        }

        if (!mirror_neurons_observe_action(mirror, &actions[i])) {
            MIRROR_LOG_WARN("Mirror neurons: failed to observe action %u in demonstration", i);
        }
    }

    // TODO: Store sequence in working memory if enabled
    if (mirror->config.enable_working_memory && mirror->working_memory) {
        // Integration with working memory would go here
    }

    // Update associations across the sequence
    mirror_neurons_update_associations(mirror);

    return true;
}


/**
 * @brief Decay activations
 */
bool mirror_neurons_decay_activations(mirror_neurons_t mirror, uint32_t delta_time_ms)
{
    if (!mirror || !mirror->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_neurons_decay_activations: required parameter is NULL (mirror, mirror->initialized)");
        return false;
    }

    // Calculate decay factor based on time
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_decay_activations", 0.0f);


    float decay_factor = expf(-mirror->config.decay_rate * (delta_time_ms / 1000.0F));

    // Apply decay to all neurons
    for (uint32_t i = 0; i < mirror->num_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mirror->num_neurons > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)mirror->num_neurons);
        }

        mirror_neuron_unit_t* neuron = &mirror->neurons[i];

        neuron->observation_activation *= decay_factor;
        neuron->execution_activation *= decay_factor;

        // Threshold to zero if very small
        if (neuron->observation_activation < 0.001F) {
            neuron->observation_activation = 0.0F;
        }
        if (neuron->execution_activation < 0.001F) {
            neuron->execution_activation = 0.0F;
        }
    }

    return true;
}


/**
 * @brief Predict next action in sequence
 */
bool mirror_neurons_predict_next_action(
    mirror_neurons_t mirror,
    const action_t* previous_actions,
    uint32_t num_previous,
    action_t* predicted_action,
    float* confidence)
{
    if (!mirror || !previous_actions || num_previous == 0 || !predicted_action) {
        if (confidence) *confidence = 0.0F;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_predict_next_action: validation failed");
        return false;
    }

    // Simple prediction: find action with highest activation that follows the sequence
    // In future, this could use more sophisticated sequence learning

    // Get last action in sequence
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_predict_next_action", 0.0f);


    const action_t* last_action = &previous_actions[num_previous - 1];

    // Find action with highest activation that's different from last
    float max_activation = 0.0F;
    uint32_t best_action_id = 0;
    bool found_candidate = false;

    for (uint32_t i = 0; i < mirror->num_actions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mirror->num_actions > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)mirror->num_actions);
        }

        if (mirror->actions[i].action_id == last_action->action_id) {
            continue;  // Skip same action
        }

        float activation = mirror_neurons_get_activation(mirror, mirror->actions[i].action_id);
        if (activation > max_activation) {
            max_activation = activation;
            best_action_id = mirror->actions[i].action_id;
            found_candidate = true;
        }
    }

    if (!found_candidate) {
        if (confidence) *confidence = 0.0F;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_predict_next_action: validation failed");
        return false;
    }

    // Create predicted action (simplified - use stored features)
    for (uint32_t i = 0; i < mirror->num_actions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mirror->num_actions > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)mirror->num_actions);
        }

        if (mirror->actions[i].action_id == best_action_id) {
            predicted_action->action_id = best_action_id;
            strncpy(predicted_action->action_name, mirror->actions[i].action_name,
                   sizeof(predicted_action->action_name) - 1);
            predicted_action->num_features = 0;  // Would need to reconstruct features
            predicted_action->agent_id = 0;
            predicted_action->timestamp = nimcp_time_get_ms();

            if (confidence) {
                *confidence = max_activation;
            }

            return true;
        }
    }

    if (confidence) *confidence = 0.0F;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_predict_next_action: validation failed");
    return false;
}


//=============================================================================
// Integration API Implementation
//=============================================================================

/**
 * @brief Integrate with working memory
 */
bool mirror_neurons_integrate_working_memory(
    mirror_neurons_t mirror,
    void* working_memory)
{
    if (!mirror) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_neurons_integrate_working_memory: mirror is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_integrate_working_me", 0.0f);


    mirror->working_memory = working_memory;

    if (working_memory) {
        MIRROR_LOG_INFO("Mirror neurons: integrated with working memory");
    }

    return true;
}


/**
 * @brief Integrate with theory of mind
 */
bool mirror_neurons_integrate_theory_of_mind(
    mirror_neurons_t mirror,
    void* theory_of_mind)
{
    if (!mirror) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_neurons_integrate_theory_of_mind: mirror is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_integrate_theory_of_", 0.0f);


    mirror->theory_of_mind = theory_of_mind;

    if (theory_of_mind) {
        MIRROR_LOG_INFO("Mirror neurons: integrated with theory of mind");
    }

    return true;
}


/**
 * @brief Integrate with predictive processing
 */
bool mirror_neurons_integrate_predictive(
    mirror_neurons_t mirror,
    void* predictive_network)
{
    if (!mirror) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_neurons_integrate_predictive: mirror is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_integrate_predictive", 0.0f);


    mirror->predictive_network = predictive_network;

    if (predictive_network) {
        MIRROR_LOG_INFO("Mirror neurons: integrated with predictive network");
    }

    return true;
}


/**
 * @brief Integrate with glial cell system (Phase 10.11.1)
 *
 * WHAT: Enable glial cell modulation of mirror neurons
 * WHY:
 * - Astrocytes: Modulate association learning strength (Ca2+ dependent plasticity)
 * - Oligodendrocytes: Speed up action recognition (myelination reduces delays)
 * - Microglia: Prune weak/unused mirror neuron associations (synaptic homeostasis)
 * HOW:  Store glial integration handle, glial cells will modulate mirror neuron activity
 *
 * BIOLOGICAL RATIONALE:
 * Mirror neurons show dense glial coverage in premotor and parietal cortex.
 * Astrocytes modulate mirror neuron plasticity during observational learning.
 * Oligodendrocytes enhance temporal precision of action recognition.
 * Microglia maintain network efficiency by pruning non-matching associations.
 *
 * @param mirror Mirror neuron system
 * @param glial_integration Glial integration system handle
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1) - just stores pointer
 */
bool mirror_neurons_integrate_glial(
    mirror_neurons_t mirror,
    void* glial_integration)
{
    if (!mirror) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_neurons_integrate_glial: mirror is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_integrate_glial", 0.0f);


    mirror->glial_integration = glial_integration;

    if (glial_integration) {
        MIRROR_LOG_INFO("Mirror neurons: integrated with glial cell system (astrocytes, oligodendrocytes, microglia)");

        if (mirror->config.enable_astrocytes) {
            MIRROR_LOG_INFO("  - Astrocytes enabled: modulate association strength");
        }
        if (mirror->config.enable_oligodendrocytes) {
            MIRROR_LOG_INFO("  - Oligodendrocytes enabled: speed up recognition");
        }
        if (mirror->config.enable_microglia) {
            MIRROR_LOG_INFO("  - Microglia enabled: prune weak associations");
        }
    }

    return true;
}


/**
 * @brief Activate observation mode
 *
 * WHAT: Signal that agent detected, prepare for observation learning
 * WHY:  Visual detection of agents triggers mirror neuron activation
 * HOW:  Prime observation pathways for incoming action features
 *
 * COMPLEXITY: O(1)
 *
 * @param mirror Mirror neuron system
 */
void mirror_neurons_activate_observation_mode(mirror_neurons_t mirror)
{
    // Guard: Validate mirror system
    if (!mirror || !mirror->initialized) {
        MIRROR_LOG_ERROR("Mirror neurons: invalid system in activate_observation_mode");
        return;
    }

    // WHAT: Boost baseline activation for all neurons
    // WHY:  Prepare for incoming social observation
    // HOW:  Small activation boost to make system more sensitive
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_activate_observation", 0.0f);


    for (uint32_t i = 0; i < mirror->num_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mirror->num_neurons > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)mirror->num_neurons);
        }

        mirror_neuron_unit_t* neuron = &mirror->neurons[i];
        if (neuron->action_id != 0) {  // Skip unassigned neurons
            neuron->observation_activation = fminf(
                neuron->observation_activation + 0.1F,  // Small boost
                1.0F
            );
        }
    }

    MIRROR_LOG_INFO("Mirror neurons: observation mode activated (primed %u neurons)",
                   mirror->num_neurons);
}


//=============================================================================
// Phase 10.11.2: Substrate Integration API Implementation
//=============================================================================

/**
 * @brief Enable substrate integration for mirror neurons
 *
 * WHAT: Enable biological substrate backing for all mirror neuron units
 * WHY:  Provides myelination timing, dendrite plasticity, glial modulation
 * HOW:  Create memory pool and substrate backings for each unit
 */
bool mirror_neurons_enable_substrate(mirror_neurons_t mirror)
{
    if (!mirror) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_enable_substrate: mirror is NULL");
        return false;
    }

    /* Already enabled? */
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_enable_substrate", 0.0f);


    if (mirror->substrate_enabled) {
        MIRROR_LOG_INFO("Mirror neurons: substrate already enabled");
        return true;
    }

    /* Initialize substrate config from mirror config */
    mirror->substrate_config = mirror_substrate_get_default_config();
    mirror->substrate_config.enable_myelination = mirror->config.enable_myelination;
    mirror->substrate_config.enable_dendrites = mirror->config.enable_dendrite_plasticity;
    mirror->substrate_config.enable_axons = mirror->config.enable_axon_timing;
    mirror->substrate_config.enable_astrocytes = mirror->config.enable_astrocytes;
    mirror->substrate_config.enable_oligodendrocytes = mirror->config.enable_oligodendrocytes;
    mirror->substrate_config.enable_microglia = mirror->config.enable_microglia;
    mirror->substrate_config.enable_memory_pool = mirror->config.enable_substrate_pool;
    mirror->substrate_config.pool_capacity = mirror->config.substrate_pool_size;

    /* Create memory pool if enabled */
    if (mirror->config.enable_substrate_pool) {
        mirror->substrate_pool = mirror_substrate_pool_create(
            mirror->config.substrate_pool_size);
        if (!mirror->substrate_pool) {
            MIRROR_LOG_ERROR("Mirror neurons: failed to create substrate pool");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_neurons_enable_substrate: mirror->substrate_pool is NULL");
            return false;
        }
    }

    /* Create substrate backings for all neurons */
    for (uint32_t i = 0; i < mirror->num_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mirror->num_neurons > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)mirror->num_neurons);
        }

        mirror_neuron_unit_t* unit = &mirror->neurons[i];

        unit->substrate = mirror_substrate_backing_create(
            unit->neuron_id,
            &mirror->substrate_config,
            mirror->substrate_pool);

        if (unit->substrate) {
            unit->has_substrate = true;
        } else {
            MIRROR_LOG_WARN("Mirror neurons: failed to create substrate for unit %u", i);
        }
    }

    mirror->substrate_enabled = true;
    MIRROR_LOG_INFO("Mirror neurons: substrate enabled for %u units", mirror->num_neurons);

    return true;
}


/**
 * @brief Connect substrate to axon network
 */
bool mirror_neurons_connect_axon_network(
    mirror_neurons_t mirror,
    void* axon_network)
{
    if (!mirror) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_connect_axon_network: mirror is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_connect_axon_network", 0.0f);


    mirror->axon_network = axon_network;

    if (axon_network) {
        nimcp_result_t result = mirror_substrate_connect_axon_network(
            mirror, axon_network);
        return result == NIMCP_SUCCESS;
    }

    return true;
}


/**
 * @brief Connect substrate to dendrite network
 */
bool mirror_neurons_connect_dendrite_network(
    mirror_neurons_t mirror,
    void* dendrite_network)
{
    if (!mirror) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_connect_dendrite_network: mirror is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_connect_dendrite_net", 0.0f);


    mirror->dendrite_network = dendrite_network;

    if (dendrite_network) {
        nimcp_result_t result = mirror_substrate_connect_dendrite_network(
            mirror, dendrite_network);
        return result == NIMCP_SUCCESS;
    }

    return true;
}


/**
 * @brief Connect substrate to myelin sheath network
 */
bool mirror_neurons_connect_myelin_network(
    mirror_neurons_t mirror,
    void* myelin_network)
{
    if (!mirror) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_connect_myelin_network: mirror is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_connect_myelin_netwo", 0.0f);


    mirror->myelin_network = myelin_network;

    if (myelin_network) {
        nimcp_result_t result = mirror_substrate_connect_myelin_network(
            mirror, myelin_network);
        return result == NIMCP_SUCCESS;
    }

    return true;
}


//=============================================================================
// Enhancement Systems Implementation (Phase 10.11.4-6)
//=============================================================================

/**
 * @brief Enable STDP learning for mirror neurons
 */
bool mirror_neurons_enable_stdp(mirror_neurons_t mirror, uint32_t max_synapses)
{
    if (!mirror) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_enable_stdp: mirror is NULL");
        return false;
    }

    // Already enabled?
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_enable_stdp", 0.0f);


    if (mirror->stdp_enabled && mirror->stdp_system) {
        return true;
    }

    // Use max_actions if max_synapses not specified
    if (max_synapses == 0) {
        max_synapses = mirror->config.max_actions;
    }

    // Create STDP system with default configuration
    mirror_stdp_t stdp = mirror_stdp_create(NULL, max_synapses);
    if (!stdp) {
        MIRROR_LOG_ERROR("Failed to create STDP system\n");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_neurons_enable_stdp: stdp is NULL");
        return false;
    }

    // Create synapses for existing actions
    for (uint32_t i = 0; i < mirror->num_actions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mirror->num_actions > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)mirror->num_actions);
        }

        uint32_t action_id = mirror->actions[i].action_id;
        mirror_stdp_create_synapse(stdp, action_id, 0.5F);  // Initial weight 0.5
    }

    mirror->stdp_system = stdp;
    mirror->stdp_enabled = true;

    MIRROR_LOG_INFO("STDP learning enabled with %u synapses\n", max_synapses);
    return true;
}


/**
 * @brief Enable motor resonance for mirror neurons
 */
bool mirror_neurons_enable_resonance(mirror_neurons_t mirror, uint32_t max_channels)
{
    if (!mirror) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_enable_resonance: mirror is NULL");
        return false;
    }

    // Already enabled?
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_enable_resonance", 0.0f);


    if (mirror->resonance_enabled && mirror->resonance_system) {
        return true;
    }

    // Use max_actions if max_channels not specified
    if (max_channels == 0) {
        max_channels = mirror->config.max_actions;
    }

    // Create resonance system with default configuration
    motor_resonance_t resonance = motor_resonance_create(NULL, max_channels);
    if (!resonance) {
        MIRROR_LOG_ERROR("Failed to create motor resonance system\n");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_neurons_enable_resonance: resonance is NULL");
        return false;
    }

    // Create channels for existing actions
    for (uint32_t i = 0; i < mirror->num_actions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mirror->num_actions > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)mirror->num_actions);
        }

        uint32_t action_id = mirror->actions[i].action_id;
        motor_resonance_create_channel(resonance, action_id);
    }

    mirror->resonance_system = resonance;
    mirror->resonance_enabled = true;

    MIRROR_LOG_INFO("Motor resonance enabled with %u channels\n", max_channels);
    return true;
}


/**
 * @brief Check if action is above execution threshold
 */
bool mirror_neurons_should_imitate(mirror_neurons_t mirror, uint32_t action_id)
{
    if (!mirror) {
        return false;
    }
    if (!mirror->resonance_enabled || !mirror->resonance_system) return false;

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_should_imitate", 0.0f);


    motor_resonance_t resonance = (motor_resonance_t)mirror->resonance_system;
    uint32_t channel_id = motor_resonance_find_channel(resonance, action_id);

    if (channel_id == UINT32_MAX) {
        return false;
    }

    return motor_resonance_above_threshold(resonance, channel_id);
}


/**
 * @brief Enable hierarchical goal representation
 */
bool mirror_neurons_enable_hierarchy(mirror_neurons_t mirror)
{
    if (!mirror) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_enable_hierarchy: mirror is NULL");
        return false;
    }

    // Already enabled?
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_enable_hierarchy", 0.0f);


    if (mirror->hierarchy_enabled && mirror->hierarchy_system) {
        return true;
    }

    // Create hierarchy system with default configuration
    mirror_hierarchy_t hierarchy = mirror_hierarchy_create(NULL);
    if (!hierarchy) {
        MIRROR_LOG_ERROR("Failed to create hierarchy system\n");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_neurons_enable_hierarchy: hierarchy is NULL");
        return false;
    }

    // Create motor representations for existing actions
    for (uint32_t i = 0; i < mirror->num_actions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mirror->num_actions > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)mirror->num_actions);
        }

        mirror_hierarchy_create_motor(hierarchy, mirror->actions[i].action_name,
                                       MOTOR_TYPE_UNKNOWN);
    }

    mirror->hierarchy_system = hierarchy;
    mirror->hierarchy_enabled = true;

    MIRROR_LOG_INFO("Hierarchical goal representation enabled\n");
    return true;
}


/**
 * @brief Infer goal from observed action
 */
bool mirror_neurons_infer_goal(mirror_neurons_t mirror, uint32_t action_id,
                                uint32_t* out_goal, float* out_confidence)
{
    if (!mirror) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_infer_goal: mirror is NULL");
        return false;
    }
    if (!mirror->hierarchy_enabled || !mirror->hierarchy_system) return false;
    if (!out_goal || !out_confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_infer_goal: out_goal or out_confidence is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_infer_goal", 0.0f);


    mirror_hierarchy_t hierarchy = (mirror_hierarchy_t)mirror->hierarchy_system;

    // Find motor representation for this action
    uint32_t motor_id = UINT32_MAX;
    for (uint32_t i = 0; i < mirror->num_actions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mirror->num_actions > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)mirror->num_actions);
        }

        if (mirror->actions[i].action_id == action_id) {
            motor_id = i;  // Motor ID corresponds to action index
            break;
        }
    }

    if (motor_id == UINT32_MAX) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_infer_goal: validation failed");
        return false;
    }

    // Infer goal
    uint32_t goal_ids[4];
    float probs[4];
    uint32_t num_goals = mirror_hierarchy_infer_goal(hierarchy, motor_id,
                                                      goal_ids, probs, 4);

    if (num_goals == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_infer_goal: num_goals is zero");
        return false;
    }

    *out_goal = goal_ids[0];
    *out_confidence = probs[0];

    return true;
}


/**
 * @brief Select goal for top-down motor control
 */
void mirror_neurons_select_goal(mirror_neurons_t mirror, int32_t goal_id)
{
    if (!mirror) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_select_goal: mirror is NULL");
        return;
    }
    if (!mirror->hierarchy_enabled || !mirror->hierarchy_system) return;

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_select_goal", 0.0f);


    mirror_hierarchy_select_goal((mirror_hierarchy_t)mirror->hierarchy_system, goal_id);
}


//=============================================================================
// KG Self-Awareness Integration
//=============================================================================

int mirror_neurons_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Mirror_Neurons");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                mirror_neurons_heartbeat("mirror_neuro_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            MIRROR_LOG_INFO("Mirror neurons self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Mirror_Neurons");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Mirror_Neurons");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}


/* ============================================================================
 * Phase 8: Training stubs
 * ============================================================================ */
int mirror_neurons_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_neurons_training_begin: NULL argument");
        return -1;
    }
    mirror_neurons_heartbeat_instance(NULL, "mirror_neurons_training_begin", 0.0f);
    return 0;
}


int mirror_neurons_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_neurons_training_end: NULL argument");
        return -1;
    }
    mirror_neurons_heartbeat_instance(NULL, "mirror_neurons_training_end", 1.0f);
    return 0;
}
