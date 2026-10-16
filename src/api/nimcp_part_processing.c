// nimcp_part_processing.c - processing functions
// Part of nimcp.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp.c


nimcp_status_t nimcp_brain_train_step(
    nimcp_brain_t brain,
    const float* features,
    uint32_t num_features,
    const float* targets,
    uint32_t num_targets,
    nimcp_training_result_t* result)
{
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "Brain handle is NULL");
    NIMCP_CHECK_THROW(features, NIMCP_ERROR_NULL_ARG, "Features array is NULL");
    NIMCP_CHECK_THROW(targets, NIMCP_ERROR_NULL_ARG, "Targets array is NULL");

    brain_t internal = brain->internal_brain;
    NIMCP_CHECK_THROW(internal, NIMCP_ERROR_NULL_ARG, "Internal brain is NULL");

    // Validate dimensions
    NIMCP_CHECK_THROW(num_features == internal->config.num_inputs, NIMCP_ERROR_INVALID,
                      "Feature count mismatch");
    NIMCP_CHECK_THROW(num_targets == internal->config.num_outputs, NIMCP_ERROR_INVALID,
                      "Target count mismatch");

    // Get training state
    training_pipeline_state_t* state = get_training_state(brain);
    if (!state || !state->configured) {
        // Auto-configure with defaults if not configured
        nimcp_training_config_t default_config = nimcp_training_config_default();
        nimcp_status_t config_res = nimcp_brain_configure_training(brain, &default_config);
        if (config_res != NIMCP_OK) {
            return config_res;
        }
        state = get_training_state(brain);
    }

    nimcp_brain_training_ctx_t* training_ctx = internal->training_ctx;
    NIMCP_CHECK_THROW(training_ctx, NIMCP_ERROR_INVALID, "Training context not available");

    // === DISPATCHER: Try specialized training first (SNN/LNN/CNN) ===
    // For non-ADAPTIVE network types, the dispatcher handles training
    if (internal->active_network_type != NIMCP_NETWORK_ADAPTIVE) {
        training_dispatch_result_t dispatch_result = {0};
        int dispatch_rc = training_dispatch_step(internal, features, num_features,
                                                  targets, num_targets, &dispatch_result);

        if (dispatch_rc == 0) {
            // Dispatcher handled training successfully
            state->step_count++;

            if (result) {
                result->loss = dispatch_result.loss;
                result->learning_rate = dispatch_result.learning_rate;
                result->step = state->step_count;
                result->early_stopped = dispatch_result.early_stopped;
                result->gradient_norm = dispatch_result.gradient_norm;
            }

            set_error("No error");
            return NIMCP_OK;
        }

        // If dispatch_rc == -2, fall through to standard backprop
        // If dispatch_rc == -1, error - also fall through to backprop as fallback
        if (dispatch_rc == -1) {
            LOG_WARN("Training dispatch failed, falling back to adaptive backprop");
        }
    }

    // === ADAPTIVE PATH: Standard backpropagation ===

    // Get the adaptive network
    adaptive_network_t network = internal->network;
    if (!network) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID, "Brain has no neural network");
    }

    // Get base neural network
    neural_network_t base_net = adaptive_network_get_base_network(network);
    if (!base_net) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR, "Failed to get base network");
    }

    // === STEP 1: Create or get backprop context ===
    if (!state->backprop) {
        state->backprop = backprop_create(base_net);
        if (!state->backprop) {
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_MEMORY, "Failed to create backprop context");
        }
    }

    // Allocate predictions buffer
    float* predictions = nimcp_malloc(num_targets * sizeof(float));
    if (!predictions) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_MEMORY, "Failed to allocate predictions buffer");
    }

    // === STEP 2: Forward pass with activation recording ===
    // Use network's forward pass for predictions (more reliable)
    backprop_clear(state->backprop);
    // Note: returns uint32_t active count (0 is valid for untrained brain), not bool
    adaptive_network_forward(network, features, num_features,
                             predictions, num_targets, 0);

    // Store activations in backprop context from neuron states
    // The forward pass updated neuron->state for each neuron
    backprop_store_activations_from_network(state->backprop, features, num_features);

    // === STEP 3: Compute loss and output gradients ===
    float loss_value = 0.0F;
    float* output_gradients = nimcp_malloc(num_targets * sizeof(float));
    if (!output_gradients) {
        nimcp_free(predictions);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_MEMORY, "Failed to allocate output gradients");
    }

    // Compute loss (MSE or cross-entropy)
    nimcp_loss_context_t* loss_ctx = nimcp_brain_training_get_loss(training_ctx, state->loss_id);
    if (loss_ctx) {
        nimcp_loss_result_t loss_result = {0};
        nimcp_loss_forward(loss_ctx, predictions, targets, 1, num_targets, &loss_result);
        loss_value = loss_result.loss_value;
        nimcp_loss_backward(loss_ctx, predictions, targets, 1, num_targets, output_gradients);
    } else {
        // Fallback: compute MSE loss and gradients manually
        for (uint32_t i = 0; i < num_targets; i++) {
            float diff = predictions[i] - targets[i];
            loss_value += diff * diff;
            output_gradients[i] = 2.0F * diff / (float)num_targets;
        }
        loss_value /= (float)num_targets;
    }


    // === STEP 4: Backward pass to compute weight gradients ===
    if (!backprop_backward(state->backprop, output_gradients, num_targets)) {
        // If backprop fails, use output gradients as-is (limited training)
        LOG_WARN("Backprop backward failed, using limited gradient update");
    }

    // Get total weight count
    uint32_t num_neurons = neural_network_get_num_neurons(base_net);
    size_t total_weights = backprop_get_weight_count(state->backprop);
    if (total_weights == 0) {
        // Count weights manually
        for (uint32_t n = 0; n < num_neurons; n++) {
            neuron_t* neuron = neural_network_get_neuron(base_net, n);
            if (neuron) {
                total_weights += NEURON_OUT_COUNT(neuron);
            }
        }
    }

    if (total_weights == 0) {
        nimcp_free(predictions);
        nimcp_free(output_gradients);
        set_error("Network has no weights");
        return NIMCP_ERROR;
    }

    // === STEP 5: Get weight gradients from backprop ===
    float* weight_gradients = nimcp_malloc(total_weights * sizeof(float));
    if (!weight_gradients) {
        nimcp_free(predictions);
        nimcp_free(output_gradients);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_MEMORY, "Failed to allocate weight gradients");
    }

    size_t grad_count = backprop_get_weight_gradients(state->backprop, weight_gradients, total_weights);

    if (grad_count == 0 || !backprop_has_valid_gradients(state->backprop)) {
        // Fallback: use absolute output gradients distributed across weights
        // Sum absolute values to get total gradient magnitude, then distribute evenly
        float total_grad = 0.0F;
        for (uint32_t i = 0; i < num_targets; i++) {
            total_grad += fabsf(output_gradients[i]);
        }
        float grad_per_weight = total_grad / (float)total_weights;
        for (size_t i = 0; i < total_weights; i++) {
            weight_gradients[i] = grad_per_weight;
            // Clamp to [-1.0, 1.0] to prevent exploding gradients
            if (weight_gradients[i] > 1.0F) weight_gradients[i] = 1.0F;
            if (weight_gradients[i] < -1.0F) weight_gradients[i] = -1.0F;
        }
    }

    // Extract current weights
    float* params = nimcp_malloc(total_weights * sizeof(float));
    if (!params) {
        nimcp_free(predictions);
        nimcp_free(output_gradients);
        nimcp_free(weight_gradients);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_MEMORY, "Failed to allocate params buffer");
    }

    size_t weight_idx = 0;
    for (uint32_t n = 0; n < num_neurons; n++) {
        neuron_t* neuron = neural_network_get_neuron(base_net, n);
        if (neuron) {
            uint32_t nsyn = NEURON_OUT_COUNT(neuron);
            for (uint32_t s = 0; s < nsyn; s++) {
                synapse_handle_t* h = NEURON_OUT_HANDLE(neuron, s);
                params[weight_idx++] = h ? h->weight : 0.0F;
            }
        }
    }

    // === STEP 6: Apply optimizer with weight gradients ===
    float current_lr = 0.0F;
    nimcp_lr_scheduler_ctx_t* sched = nimcp_brain_training_get_scheduler(
        training_ctx, state->scheduler_id);
    if (sched) {
        current_lr = nimcp_lr_scheduler_get_lr(sched);
    }

    // Gradient clipping
    float gradient_norm = 0.0F;
    for (size_t i = 0; i < total_weights; i++) {
        gradient_norm += weight_gradients[i] * weight_gradients[i];
    }
    gradient_norm = sqrtf(gradient_norm);

    float clip_value = 1.0F;  // Default gradient clip
    if (gradient_norm > clip_value) {
        float scale = clip_value / gradient_norm;
        for (size_t i = 0; i < total_weights; i++) {
            weight_gradients[i] *= scale;
        }
    }

    // Per-element gradient clipping to prevent exploding gradients
    for (size_t i = 0; i < total_weights; i++) {
        if (weight_gradients[i] > 1.0F) weight_gradients[i] = 1.0F;
        if (weight_gradients[i] < -1.0F) weight_gradients[i] = -1.0F;
    }

    // Apply optimizer step
    nimcp_result_t res = nimcp_brain_training_optimize(
        training_ctx, state->optimizer_id, params, weight_gradients, total_weights);

    // === STEP 7: Fire callbacks ===
    tcb_action_t cb_action = TCB_ACTION_CONTINUE;
    if (state->callbacks && state->callbacks_enabled) {
        tcb_update_metrics(state->callbacks, loss_value, current_lr,
                          state->step_count + 1, gradient_norm);

        tcb_event_t loss_event = {
            .event_type = TCB_EVENT_LOSS_COMPUTED,
            .metrics = {
                .step = state->step_count + 1,
                .loss = loss_value,
                .learning_rate = current_lr,
                .gradient_norm = gradient_norm
            },
            .user_data = NULL,
            .checkpoint_path = NULL,
            .timestamp_ns = 0
        };
        cb_action = tcb_fire(state->callbacks, &loss_event);

        if (cb_action == TCB_ACTION_SKIP_STEP) {
            nimcp_free(params);
            nimcp_free(weight_gradients);
            nimcp_free(output_gradients);
            nimcp_free(predictions);
            state->step_count++;
            if (result) {
                result->loss = loss_value;
                result->step = state->step_count;
                result->early_stopped = false;
                result->learning_rate = current_lr;
                result->gradient_norm = gradient_norm;
            }
            set_error("No error");
            return NIMCP_OK;
        }

        if (cb_action == TCB_ACTION_STOP_TRAINING) {
            nimcp_free(params);
            nimcp_free(weight_gradients);
            nimcp_free(output_gradients);
            nimcp_free(predictions);
            state->step_count++;
            if (result) {
                result->loss = loss_value;
                result->step = state->step_count;
                result->early_stopped = true;
                result->learning_rate = current_lr;
                result->gradient_norm = gradient_norm;
            }
            set_error("No error");
            return NIMCP_OK;
        }
    }

    // === STEP 8: Write updated weights back to network ===
    // NOTE: Must update BOTH outgoing handles AND incoming handles via peer_index
    // The forward pass reads from incoming handles
    if (res == NIMCP_SUCCESS || res == NIMCP_TRAINING_ERROR_EARLY_STOP) {
        weight_idx = 0;
        for (uint32_t n = 0; n < num_neurons; n++) {
            neuron_t* neuron = neural_network_get_neuron(base_net, n);
            if (neuron) {
                uint32_t nsyn = NEURON_OUT_COUNT(neuron);
                for (uint32_t s = 0; s < nsyn; s++) {
                    float new_weight = params[weight_idx++];
                    synapse_handle_t* out_h = NEURON_OUT_HANDLE(neuron, s);
                    if (!out_h) continue;
                    out_h->weight = new_weight;

                    // Also update corresponding incoming handle via peer_index (O(1))
                    if (out_h->peer_index != SPARSE_SYNAPSE_NO_PEER) {
                        uint32_t target_id = out_h->target_neuron_id;
                        if (target_id < num_neurons) {
                            neuron_t* target_neuron = neural_network_get_neuron(base_net, target_id);
                            if (target_neuron) {
                                synapse_handle_t* in_h = NEURON_IN_HANDLE(target_neuron, out_h->peer_index);
                                if (in_h) {
                                    in_h->weight = new_weight;
                                }
                            }
                        }
                    }
                }
            }
        }

        // Fire weights updated callback
        if (state->callbacks && state->callbacks_enabled) {
            tcb_event_t weights_event = {
                .event_type = TCB_EVENT_WEIGHTS_UPDATED,
                .metrics = {
                    .step = state->step_count + 1,
                    .loss = loss_value,
                    .learning_rate = current_lr,
                    .gradient_norm = gradient_norm
                },
                .user_data = NULL,
                .checkpoint_path = NULL,
                .timestamp_ns = 0
            };
            cb_action = tcb_fire(state->callbacks, &weights_event);
        }
    }

    // Step scheduler
    if (state->scheduler_id > 0 && sched) {
        nimcp_lr_scheduler_step(sched);
    }

    // === STEP 9: Increment step count and fill result ===
    state->step_count++;

    // Update training context stats (for nimcp_brain_get_training_stats)
    nimcp_brain_training_update_stats(training_ctx, 1, loss_value);

    if (result) {
        result->loss = loss_value;
        result->step = state->step_count;
        result->early_stopped = (res == NIMCP_TRAINING_ERROR_EARLY_STOP) ||
                               (cb_action == TCB_ACTION_STOP_TRAINING);
        result->learning_rate = current_lr;
        result->gradient_norm = gradient_norm;
        result->rubric_evaluated = false;
        result->rubric_score = 0.0f;
        result->rubric_grade = ' ';
        result->rubric_grade_modifier = ' ';
    }

    /* Rubric evaluation (at configured interval) */
    if (result && state->rubric_enabled && state->rubric_interval > 0 &&
        (state->step_count % state->rubric_interval) == 0) {

        const float* rub_feat = state->rubric_validation_features
                                ? state->rubric_validation_features
                                : features;
        uint32_t rub_nfeat = state->rubric_validation_features
                             ? state->rubric_validation_num_features
                             : num_features;

        /* Full cognitive pipeline -> rubric */
        char rub_label[NIMCP_MAX_LABEL_SIZE];
        float rub_conf;
        nimcp_status_t pred_rc = nimcp_brain_predict(brain, rub_feat, rub_nfeat,
                                                      rub_label, &rub_conf);
        if (pred_rc == NIMCP_OK) {
            nimcp_rubric_t rubric;
            if (nimcp_brain_rubric(brain, &rubric) == NIMCP_OK) {
                /* Update stats */
                state->rubric_eval_count++;
                state->rubric_score_sum += rubric.overall_score;
                if (rubric.overall_score < state->rubric_min_observed)
                    state->rubric_min_observed = rubric.overall_score;
                if (rubric.overall_score > state->rubric_max_observed)
                    state->rubric_max_observed = rubric.overall_score;
                state->rubric_last = rubric;

                result->rubric_evaluated = true;
                result->rubric_score = rubric.overall_score;
                result->rubric_grade = rubric.grade;
                result->rubric_grade_modifier = rubric.grade_modifier;

                /* Broadcast (best-effort) */
                nimcp_brain_broadcast_rubric(brain);

                /* Threshold check */
                if (state->rubric_stop_on_threshold &&
                    state->rubric_min_score > 0.0f &&
                    rubric.overall_score < state->rubric_min_score) {
                    result->early_stopped = true;
                }
            }
        }
    }

    // Fire step complete callback
    if (state->callbacks && state->callbacks_enabled) {
        tcb_event_t step_event = {
            .event_type = TCB_EVENT_STEP_COMPLETE,
            .metrics = {
                .step = state->step_count,
                .loss = loss_value,
                .learning_rate = current_lr,
                .gradient_norm = gradient_norm
            },
            .user_data = NULL,
            .checkpoint_path = NULL,
            .timestamp_ns = 0
        };
        cb_action = tcb_fire(state->callbacks, &step_event);

        if (cb_action == TCB_ACTION_STOP_TRAINING && result) {
            result->early_stopped = true;
        }
    }

    // Cleanup
    nimcp_free(params);
    nimcp_free(weight_gradients);
    nimcp_free(output_gradients);
    nimcp_free(predictions);

    if (res != NIMCP_SUCCESS && res != NIMCP_TRAINING_ERROR_EARLY_STOP) {
        set_error("Training step failed");
        return NIMCP_ERROR;
    }

    set_error("No error");
    return NIMCP_OK;
}


float nimcp_brain_step_scheduler(nimcp_brain_t brain, float validation_metric) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_step_scheduler: brain is NULL");
        return 0.0F;
    }

    brain_t internal = brain->internal_brain;
    if (!internal || !internal->training_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_brain_step_scheduler: training not enabled");
        return 0.0F;
    }

    training_pipeline_state_t* state = get_training_state(brain);
    if (!state || !state->configured) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_brain_step_scheduler: training not configured");
        return 0.0F;
    }

    // Step scheduler and update optimizer
    float new_lr = nimcp_brain_training_step_scheduler_metric(
        internal->training_ctx,
        state->scheduler_id,
        state->optimizer_id,
        validation_metric
    );

    set_error("No error");
    return new_lr;
}
