// nimcp_mirror_neurons_part_helpers.c - helpers functions
// Part of nimcp_mirror_neurons.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_mirror_neurons.c


/**
 * @brief Compute cosine similarity between feature vectors
 *
 * WHAT: Calculate similarity between two feature vectors
 * WHY:  Determine how similar two actions are
 * HOW:  Compute dot product / (norm1 * norm2)
 *
 * @return Similarity in range [0.0, 1.0], or -1.0 on error
 */
static float compute_feature_similarity(const float* f1, const float* f2, uint32_t n)
{
    if (!f1 || !f2 || n == 0) {
        return -1.0F;
    }

    float dot_product = 0.0F;
    float norm1 = 0.0F;
    float norm2 = 0.0F;

    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)n);
        }

        dot_product += f1[i] * f2[i];
        norm1 += f1[i] * f1[i];
        norm2 += f2[i] * f2[i];
    }

    if (norm1 == 0.0F || norm2 == 0.0F) {
        return 0.0F;
    }

    float similarity = dot_product / (sqrtf(norm1) * sqrtf(norm2));

    // Normalize to [0, 1] from [-1, 1]
    similarity = (similarity + 1.0F) / 2.0F;

    return (similarity < 0.0F) ? 0.0F : ((similarity > 1.0F) ? 1.0F : similarity);
}


/**
 * @brief Activate neurons for an action
 *
 * WHAT: Set activation levels for neurons representing an action
 * WHY:  Implement observation or execution pathway
 * HOW:  Find neurons for action, update activations
 */
/**
 * @brief Get acetylcholine modulation for mirror neuron observation
 *
 * WHAT: Compute ACh-based gating factor for observed actions
 * WHY:  Acetylcholine gates social learning and action observation
 * HOW:  Read ACh level, map to modulation factor [0.6, 1.4]
 *
 * BIOLOGY: ACh enhances attention to observed actions
 *          High ACh (0.7) → 1.4× observation strength (focused social learning)
 *          Low ACh (0.3) → 0.6× observation strength (inattentive, autism-like)
 *
 * COMPLEXITY: O(1)
 *
 * @param mirror Mirror neuron system
 * @return Modulation factor [0.6, 1.4], or 1.0 if no brain
 */
static float get_mirror_ach_modulation(mirror_neurons_t mirror)
{
    // Guard: Early return if no brain
    if (!mirror || !mirror->brain) {
        return 1.0F;
    }

    neuromodulator_system_t neuromod = brain_get_neuromodulator_system(mirror->brain);
    if (!neuromod) {
        return 1.0F;
    }

    // Read acetylcholine level
    float ach = neuromodulator_get_level(neuromod, NEUROMOD_ACETYLCHOLINE);

    // Map ACh range [0.3, 0.7] to modulation [0.6, 1.4]
    float modulation = 0.6F + (ach - 0.3F) * 2.0F;

    // Clamp to safe range [0.0, 2.0]
    if (modulation < 0.0f) modulation = 0.0f;
    if (modulation > 2.0f) modulation = 2.0f;

    return modulation;
}


static void activate_neurons_for_action(
    mirror_neurons_t mirror,
    uint32_t action_idx,
    float strength,
    bool is_observation)
{
    if (!mirror || action_idx >= mirror->num_actions) {
        return;
    }

    action_mapping_t* mapping = &mirror->actions[action_idx];
    uint64_t current_time = nimcp_time_get_ms();

    // Allocate neurons if this is first activation
    if (mapping->num_neurons == 0) {
        // Assign a small population of neurons to this action
        uint32_t neurons_per_action = mirror->config.num_mirror_neurons /
                                     (mirror->config.max_actions + 1);
        neurons_per_action = (neurons_per_action < 5) ? 5 : neurons_per_action;

        for (uint32_t i = 0; i < neurons_per_action && mapping->num_neurons < mapping->capacity; i++) {
            // Find an available neuron
            uint32_t neuron_idx = (action_idx * neurons_per_action + i) % mirror->num_neurons;
            if (mirror->neurons[neuron_idx].action_id == 0 ||
                mirror->neurons[neuron_idx].action_id == mapping->action_id) {

                mirror->neurons[neuron_idx].action_id = mapping->action_id;
                mirror->neurons[neuron_idx].neuron_id = neuron_idx;
                mapping->neuron_indices[mapping->num_neurons++] = neuron_idx;
            }
        }
    }

    // Apply acetylcholine modulation to observation pathway
    float ach_modulation = is_observation ? get_mirror_ach_modulation(mirror) : 1.0F;

    // Activate neurons
    for (uint32_t i = 0; i < mapping->num_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mapping->num_neurons > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)mapping->num_neurons);
        }

        uint32_t neuron_idx = mapping->neuron_indices[i];
        mirror_neuron_unit_t* neuron = &mirror->neurons[neuron_idx];

        if (is_observation) {
            // Apply ACh gating: enhances social learning when attentive
            neuron->observation_activation += strength * ach_modulation;
            if (neuron->observation_activation > 1.0F) {
                neuron->observation_activation = 1.0F;
            }
            neuron->observation_count++;
            neuron->last_observation_time = current_time;
        } else {
            neuron->execution_activation += strength;
            if (neuron->execution_activation > 1.0F) {
                neuron->execution_activation = 1.0F;
            }
            neuron->execution_count++;
            neuron->last_execution_time = current_time;
        }
    }
}


/**
 * @brief Update action statistics
 *
 * WHAT: Recalculate statistics for an action
 * WHY:  Track learning progress and matching quality
 * HOW:  Aggregate neuron-level metrics
 */
static void update_action_statistics(mirror_neurons_t mirror, uint32_t action_idx)
{
    if (!mirror || action_idx >= mirror->num_actions) {
        return;
    }

    // Process pending bio-async messages (after NULL check)
    if (mirror->bio_async_enabled && mirror->bio_ctx) {
        bio_router_process_inbox(mirror->bio_ctx, 5);
    }

    action_mapping_t* mapping = &mirror->actions[action_idx];

    if (mapping->num_neurons == 0) {
        return;
    }

    // Calculate average similarity across neurons
    float total_similarity = 0.0F;
    uint32_t count = 0;

    for (uint32_t i = 0; i < mapping->num_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mapping->num_neurons > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)mapping->num_neurons);
        }

        uint32_t neuron_idx = mapping->neuron_indices[i];
        mirror_neuron_unit_t* neuron = &mirror->neurons[neuron_idx];

        if (neuron->observation_count > 0 && neuron->execution_count > 0) {
            // Similarity based on co-activation
            float obs_norm = neuron->observation_activation;
            float exec_norm = neuron->execution_activation;
            float similarity = (obs_norm * exec_norm) /
                             (obs_norm + exec_norm + 0.001F);  // Prevent div by 0
            total_similarity += similarity;
            count++;
        }
    }

    if (count > 0) {
        mapping->avg_similarity = total_similarity / count;
    }
}


//=============================================================================
// BIO-ASYNC MESSAGE HANDLERS
//=============================================================================

/**
 * @brief Bio-async message handler: Handle mirror neuron activation
 */
static nimcp_error_t handle_mirror_activation(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    (void)msg_size;
    (void)response_promise;

    if (!msg || !user_data) {
        return NIMCP_ERROR_NULL_ARG;
    }

    const bio_msg_introspection_query_t* activation = (const bio_msg_introspection_query_t*)msg;
    mirror_neurons_t mirror = (mirror_neurons_t)user_data;
    (void)mirror;  // Will be used for actual processing
    (void)activation;

    LOG_DEBUG(LOG_MODULE, "Received mirror neuron activation via bio-async");

    return NIMCP_SUCCESS;
}


/**
 * @brief Broadcast mirror neuron fire event via bio-async
 */
static void bio_broadcast_mirror_fire(mirror_neurons_t mirror,
                                       uint32_t action_id,
                                       float activation) {
    if (!mirror || !mirror->bio_async_enabled || !mirror->bio_ctx) {
        return;
    }

    bio_msg_introspection_response_t msg = {0};
    bio_msg_init_header(&msg.header, BIO_MSG_MIRROR_NEURON_ACTIVATION,
                        bio_module_context_get_id(mirror->bio_ctx), 0, sizeof(msg));
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg.query_type = action_id;
    msg.confidence = activation;
    msg.matched_pattern_count = 1;

    bio_router_broadcast(mirror->bio_ctx, &msg, sizeof(msg));
    LOG_DEBUG(LOG_MODULE, "Broadcast mirror fire: action=%u, activation=%.2f",
              action_id, activation);
}
