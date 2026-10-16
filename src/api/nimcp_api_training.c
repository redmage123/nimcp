/**
 * @file nimcp_api_training.c
 * @brief Brain training pipeline API implementation
 *
 * This module handles the complete training pipeline including:
 * - Training configuration (loss, optimizer, scheduler)
 * - Training steps and batch training
 * - Training callbacks and event handling
 * - Training statistics and metrics
 *
 * Responsibilities:
 * - Configure training components (loss/optimizer/scheduler)
 * - Execute training steps and batches
 * - Manage training callbacks
 * - Track training statistics
 */

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "API_TRAINING"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_learning_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(api_training)

/* API Exception Integration (Phase 7) */
extern void set_error(const char* fmt, ...);
#define NIMCP_API_SET_ERROR(fmt, ...) set_error(fmt, ##__VA_ARGS__)
#include "api/nimcp_api_exception.h"

#include "nimcp.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "middleware/training/nimcp_brain_training_integration.h"
#include "middleware/training/nimcp_loss_functions.h"
#include "middleware/training/nimcp_optimizers.h"
#include "middleware/training/nimcp_lr_scheduler.h"
#include "middleware/training/nimcp_training_callbacks.h"
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "core/neuralnet/nimcp_neuralnet_backprop.h"
#include "utils/memory/nimcp_memory.h"
#include "security/nimcp_bbb_helpers.h"
#include "utils/thread/nimcp_thread.h"
#include <stdio.h>
#include <string.h>

//=============================================================================
// Canonical Type Definitions (from shared header)
//=============================================================================

#include "api/nimcp_api_internal.h"  /* Canonical handle struct definitions */

//=============================================================================
// External References (from nimcp.c)
//=============================================================================

// These functions are defined in nimcp.c and shared across modules
extern void set_error(const char* fmt, ...);
extern const char* nimcp_get_error(void);

// NOTE: This file is NOT compiled into the library (only nimcp.c is compiled).
// It exists as an alternative/reference implementation. The authoritative
// training_pipeline_state_t, g_training_states[], get_training_state(),
// clear_training_state(), and nimcp_api_training_cleanup_brain() are defined
// in nimcp.c via #included nimcp_part_helpers.c and nimcp_part_lifecycle.c.

//=============================================================================
// Training Configuration API
//=============================================================================

nimcp_training_config_t nimcp_training_config_default(void) {
    nimcp_training_config_t config = {
        .loss_type = NIMCP_API_LOSS_CROSS_ENTROPY,
        .optimizer_type = NIMCP_API_OPT_ADAM,
        .scheduler_type = NIMCP_API_SCHED_COSINE,

        .learning_rate = NIMCP_LEARNING_RATE_FINE,
        .weight_decay = 0.0f,
        .momentum = NIMCP_MOMENTUM_DEFAULT,
        .beta1 = NIMCP_ADAM_BETA1_DEFAULT,
        .beta2 = NIMCP_ADAM_BETA2_DEFAULT,
        .epsilon = NIMCP_EPSILON_ADAM,

        .scheduler_step_size = 1000,
        .scheduler_gamma = 0.1f,
        .warmup_steps = 0,

        .enable_gradient_clipping = true,
        .gradient_clip_value = 1.0f,

        .enable_biological_modulation = true,
        .biological_blend = 0.5f,

        .enable_rubric = false,
        .rubric_interval = 0,
        .rubric_min_score = 0.0f,
        .rubric_stop_on_threshold = false
    };
    return config;
}

nimcp_status_t nimcp_brain_configure_training(
    nimcp_brain_t brain,
    const nimcp_training_config_t* config)
{
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "Brain handle is NULL in configure_training");
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_ARG, "Training config is NULL");

    // Get internal brain
    brain_t internal = brain->internal_brain;
    NIMCP_CHECK_THROW(internal, NIMCP_ERROR_NULL_ARG, "Internal brain is NULL in configure_training");

    // Get or create training context
    nimcp_brain_training_ctx_t* training_ctx = internal->training_ctx;
    NIMCP_CHECK_THROW(training_ctx, NIMCP_ERROR_INVALID, "Brain has no training context");

    // Get training state
    training_pipeline_state_t* state = get_training_state(brain);
    if (!state) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(training_pipeline_state_t),
            "Failed to allocate training state");
    }

    // Map public loss type to internal loss type
    nimcp_loss_type_t internal_loss;
    switch (config->loss_type) {
        case 0:  internal_loss = NIMCP_LOSS_MSE; break;
        case 1:  internal_loss = NIMCP_LOSS_CROSS_ENTROPY; break;
        case 2:  internal_loss = NIMCP_LOSS_BINARY_CROSS_ENTROPY; break;
        case 3:  internal_loss = NIMCP_LOSS_HUBER; break;
        case 4:  internal_loss = NIMCP_LOSS_MAE; break;
        case 5:  internal_loss = NIMCP_LOSS_FOCAL; break;
        case 6:  internal_loss = NIMCP_LOSS_KL_DIVERGENCE; break;
        default: internal_loss = NIMCP_LOSS_MSE; break;
    }

    // Create loss function
    nimcp_loss_config_t loss_config = nimcp_loss_default_config(internal_loss);
    nimcp_result_t res = nimcp_brain_training_create_loss(training_ctx, &loss_config, &state->loss_id);
    if (res != NIMCP_SUCCESS || state->loss_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to create loss function");
        return NIMCP_ERROR;
    }

    // Map public optimizer type to internal optimizer type
    nimcp_optimizer_type_t internal_opt;
    switch (config->optimizer_type) {
        case 0:  internal_opt = NIMCP_OPTIMIZER_SGD; break;
        case 1:  internal_opt = NIMCP_OPTIMIZER_SGD_MOMENTUM; break;
        case 2:  internal_opt = NIMCP_OPTIMIZER_ADAM; break;
        case 3:  internal_opt = NIMCP_OPTIMIZER_ADAMW; break;
        case 4:  internal_opt = NIMCP_OPTIMIZER_RMSPROP; break;
        case 5:  internal_opt = NIMCP_OPTIMIZER_ADAGRAD; break;
        default: internal_opt = NIMCP_OPTIMIZER_ADAM; break;
    }

    // Create optimizer - use union-based config
    nimcp_optimizer_config_t opt_config = nimcp_optimizer_default_config(internal_opt);
    opt_config.clip_gradients = config->enable_gradient_clipping;
    opt_config.gradient_clip_value = config->gradient_clip_value;

    // Set parameters based on optimizer type
    switch (internal_opt) {
        case NIMCP_OPTIMIZER_SGD:
        case NIMCP_OPTIMIZER_SGD_MOMENTUM:
        case NIMCP_OPTIMIZER_NESTEROV:
            opt_config.params.sgd.learning_rate = config->learning_rate;
            opt_config.params.sgd.momentum = config->momentum;
            opt_config.params.sgd.weight_decay = config->weight_decay;
            break;
        case NIMCP_OPTIMIZER_ADAM:
            opt_config.params.adam.learning_rate = config->learning_rate;
            opt_config.params.adam.beta1 = config->beta1;
            opt_config.params.adam.beta2 = config->beta2;
            opt_config.params.adam.epsilon = config->epsilon;
            opt_config.params.adam.weight_decay = config->weight_decay;
            break;
        case NIMCP_OPTIMIZER_ADAMW:
            opt_config.params.adamw.learning_rate = config->learning_rate;
            opt_config.params.adamw.beta1 = config->beta1;
            opt_config.params.adamw.beta2 = config->beta2;
            opt_config.params.adamw.epsilon = config->epsilon;
            opt_config.params.adamw.weight_decay = config->weight_decay;
            break;
        case NIMCP_OPTIMIZER_RMSPROP:
            opt_config.params.rmsprop.learning_rate = config->learning_rate;
            opt_config.params.rmsprop.momentum = config->momentum;
            opt_config.params.rmsprop.weight_decay = config->weight_decay;
            opt_config.params.rmsprop.epsilon = config->epsilon;
            break;
        case NIMCP_OPTIMIZER_ADAGRAD:
            opt_config.params.adagrad.learning_rate = config->learning_rate;
            opt_config.params.adagrad.weight_decay = config->weight_decay;
            opt_config.params.adagrad.epsilon = config->epsilon;
            break;
        default:
            break;
    }

    res = nimcp_brain_training_create_optimizer(training_ctx, &opt_config, &state->optimizer_id);
    if (res != NIMCP_SUCCESS || state->optimizer_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to create optimizer");
        return NIMCP_ERROR;
    }

    // Map public scheduler type to internal scheduler type
    nimcp_lr_scheduler_type_t internal_sched;
    switch (config->scheduler_type) {
        case 0:  internal_sched = NIMCP_LR_CONSTANT; break;
        case 1:  internal_sched = NIMCP_LR_STEP; break;
        case 2:  internal_sched = NIMCP_LR_EXPONENTIAL; break;
        case 3:  internal_sched = NIMCP_LR_COSINE_ANNEALING; break;
        case 4:  internal_sched = NIMCP_LR_COSINE_WARMUP; break;
        case 5:  internal_sched = NIMCP_LR_REDUCE_ON_PLATEAU; break;
        case 6:  internal_sched = NIMCP_LR_CYCLIC; break;
        default: internal_sched = NIMCP_LR_COSINE_ANNEALING; break;
    }

    // Create scheduler - use union-based config
    nimcp_lr_scheduler_config_t sched_config = nimcp_lr_scheduler_config_from_type(
        internal_sched, config->learning_rate);

    // Set parameters based on scheduler type
    switch (internal_sched) {
        case NIMCP_LR_STEP:
            sched_config.params.step.step_size = config->scheduler_step_size;
            sched_config.params.step.gamma = config->scheduler_gamma;
            break;
        case NIMCP_LR_EXPONENTIAL:
            sched_config.params.exponential.gamma = config->scheduler_gamma;
            break;
        case NIMCP_LR_COSINE_ANNEALING:
            sched_config.params.cosine.T_max = config->scheduler_step_size;
            break;
        case NIMCP_LR_COSINE_WARMUP:
        case NIMCP_LR_LINEAR_WARMUP:
            sched_config.params.warmup.warmup_steps = config->warmup_steps;
            sched_config.params.warmup.target_lr = config->learning_rate;
            break;
        case NIMCP_LR_CYCLIC:
            sched_config.params.cyclic.base_lr = config->learning_rate * 0.1f;
            sched_config.params.cyclic.max_lr = config->learning_rate;
            sched_config.params.cyclic.step_size_up = config->scheduler_step_size;
            break;
        default:
            break;
    }

    res = nimcp_brain_training_create_scheduler(training_ctx, &sched_config, &state->scheduler_id);
    if (res != NIMCP_SUCCESS || state->scheduler_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to create LR scheduler");
        return NIMCP_ERROR;
    }

    // Use existing gradient manager if available, or create one
    state->gradmgr_id = 1;  // Default gradient manager created during brain init

    // Configure biological modulation
    if (config->enable_biological_modulation && internal->plasticity_bridge) {
        nimcp_brain_training_set_biological_modulation(training_ctx, config->biological_blend);
    }

    // Configure rubric integration
    state->rubric_enabled = config->enable_rubric;
    state->rubric_interval = config->rubric_interval;
    state->rubric_min_score = config->rubric_min_score;
    state->rubric_stop_on_threshold = config->rubric_stop_on_threshold;
    state->rubric_eval_count = 0;
    state->rubric_score_sum = 0.0;
    state->rubric_min_observed = 1.0f;
    state->rubric_max_observed = 0.0f;

    state->configured = true;
    state->step_count = 0;

    return NIMCP_OK;
}

//=============================================================================
// Training Execution API
//=============================================================================

nimcp_status_t nimcp_brain_train_step(
    nimcp_brain_t brain,
    const float* features,
    uint32_t num_features,
    const float* targets,
    uint32_t num_targets,
    nimcp_training_result_t* result)
{
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "Brain handle is NULL in train_step");
    NIMCP_CHECK_THROW(features, NIMCP_ERROR_NULL_ARG, "Features array is NULL in train_step");
    NIMCP_CHECK_THROW(targets, NIMCP_ERROR_NULL_ARG, "Targets array is NULL in train_step");

    // BBB validation: validate input data buffers
    if (!bbb_validate_buffer_access(features, 0, num_features * sizeof(float),
                                    num_features * sizeof(float), "nimcp_brain_train_step")) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_INPUT, "BBB validation failed for features buffer");
        return NIMCP_ERROR_INVALID_INPUT;
    }
    if (!bbb_validate_buffer_access(targets, 0, num_targets * sizeof(float),
                                    num_targets * sizeof(float), "nimcp_brain_train_step")) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_INPUT, "BBB validation failed for targets buffer");
        return NIMCP_ERROR_INVALID_INPUT;
    }

    brain_t internal = brain->internal_brain;
    NIMCP_CHECK_THROW(internal, NIMCP_ERROR_NULL_ARG, "Internal brain is NULL in train_step");

    // Validate dimensions
    NIMCP_CHECK_THROW(num_features == internal->config.num_inputs, NIMCP_ERROR_INVALID,
        "Feature count mismatch: expected %u, got %u", internal->config.num_inputs, num_features);
    NIMCP_CHECK_THROW(num_targets == internal->config.num_outputs, NIMCP_ERROR_INVALID,
        "Target count mismatch: expected %u, got %u", internal->config.num_outputs, num_targets);

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

    // Get the adaptive network
    adaptive_network_t network = internal->network;
    if (!network) {
        NIMCP_THROW_BRAIN(NIMCP_ERROR_OPERATION_FAILED, 0, "training",
            "Brain has no neural network");
    }

    // Get base network for backpropagation
    neural_network_t base_net = adaptive_network_get_base_network(network);
    if (!base_net) {
        NIMCP_THROW_BRAIN(NIMCP_ERROR_OPERATION_FAILED, 0, "training",
            "Failed to get base network");
    }

    // Create backprop context if not exists
    if (!state->backprop) {
        state->backprop = backprop_create(base_net);
        if (!state->backprop) {
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(backprop_ctx_t),
                "Failed to create backprop context");
        }
    }

    // Step 1: Forward pass with activation recording
    float* predictions = nimcp_malloc(num_targets * sizeof(float));
    if (!predictions) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, num_targets * sizeof(float),
            "Failed to allocate predictions buffer");
    }

    if (!backprop_forward(state->backprop, features, num_features, predictions, num_targets)) {
        // Fallback to adaptive network forward if backprop forward fails
        (void)adaptive_network_forward(network, features, num_features,
                                       predictions, num_targets, 0);
    }

    // Step 2: Compute loss and output gradients
    float loss_value = 0.0f;
    float* output_gradients = nimcp_malloc(num_targets * sizeof(float));
    if (!output_gradients) {
        nimcp_free(predictions);
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, num_targets * sizeof(float),
            "Failed to allocate output gradients");
    }

    // Compute loss and gradients using training context
    nimcp_result_t res = nimcp_brain_training_compute_loss(
        training_ctx, state->loss_id,
        predictions, targets,
        1, num_targets,  // batch_size=1
        &loss_value, output_gradients
    );

    if (res != NIMCP_SUCCESS) {
        nimcp_free(predictions);
        nimcp_free(output_gradients);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to compute loss");
        return NIMCP_ERROR;
    }

    // Step 3: Backpropagate to get weight gradients
    size_t total_weights = backprop_get_weight_count(state->backprop);

    float* weight_gradients = NULL;
    if (total_weights > 0) {
        weight_gradients = nimcp_malloc(total_weights * sizeof(float));
        if (!weight_gradients) {
            nimcp_free(predictions);
            nimcp_free(output_gradients);
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, total_weights * sizeof(float),
                "Failed to allocate weight gradients");
        }

        // Perform backpropagation
        if (backprop_backward(state->backprop, output_gradients, num_targets)) {
            backprop_get_weight_gradients(state->backprop, weight_gradients, total_weights);
        } else {
            // Fallback: use output gradients directly (old behavior, but limited)
            memset(weight_gradients, 0, total_weights * sizeof(float));
            for (size_t i = 0; i < num_targets && i < total_weights; i++) {
                weight_gradients[i] = output_gradients[i];
            }
        }
    }

    nimcp_free(output_gradients);

    // Step 4: Extract current weights into flat array and optimize
    float* params = NULL;
    float current_lr = 0.0f;
    if (total_weights == 0) {
        // No weights to optimize — skip Steps 4-7
        // This can happen with degenerate networks (no synapses)
        if (weight_gradients) nimcp_free(weight_gradients);
        nimcp_free(predictions);
        goto skip_optimization;
    }

    uint32_t num_neurons = neural_network_get_num_neurons(base_net);
    params = nimcp_malloc(total_weights * sizeof(float));
    if (!params) {
        nimcp_free(predictions);
        if (weight_gradients) nimcp_free(weight_gradients);
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, total_weights * sizeof(float),
            "Failed to allocate params buffer");
    }

    size_t weight_idx = 0;
    for (uint32_t n = 0; n < num_neurons; n++) {
        neuron_t* neuron = neural_network_get_neuron(base_net, n);
        if (neuron && neuron->synapses) {
            for (uint32_t s = 0; s < neuron->num_synapses; s++) {
                params[weight_idx++] = neuron->synapses[s].weight;
            }
        }
    }

    // Step 5: Apply optimizer with weight gradients
    nimcp_optimizer_context_t* opt = nimcp_brain_training_get_optimizer(
        training_ctx, state->optimizer_id);
    if (opt && weight_gradients && total_weights > 0) {
        res = nimcp_optimizer_step(opt, params, weight_gradients, total_weights);
    }

    // Step 6: Get current learning rate
    nimcp_lr_scheduler_ctx_t* sched = nimcp_brain_training_get_scheduler(
        training_ctx, state->scheduler_id);
    if (sched) {
        current_lr = nimcp_lr_scheduler_get_lr(sched);
    }

    // Step 7: Write updated weights back to network
    if (res == NIMCP_SUCCESS || res == NIMCP_TRAINING_ERROR_EARLY_STOP) {
        weight_idx = 0;
        for (uint32_t n = 0; n < num_neurons; n++) {
            neuron_t* neuron = neural_network_get_neuron(base_net, n);
            if (neuron && neuron->synapses) {
                for (uint32_t s = 0; s < neuron->num_synapses; s++) {
                    neuron->synapses[s].weight = params[weight_idx++];
                }
            }
        }
    }

    // Cleanup
    nimcp_free(params);
    if (weight_gradients) nimcp_free(weight_gradients);
    nimcp_free(predictions);

skip_optimization:
    ;

    // Step 8: Increment step count and fill result
    state->step_count++;

    if (result) {
        result->loss = loss_value;
        result->step = state->step_count;
        result->early_stopped = (res == NIMCP_TRAINING_ERROR_EARLY_STOP);
        result->learning_rate = current_lr;
        result->gradient_norm = 0.0f;
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

    if (res != NIMCP_SUCCESS && res != NIMCP_TRAINING_ERROR_EARLY_STOP) {
        NIMCP_THROW_BRAIN(NIMCP_ERROR_OPERATION_FAILED, 0, "training",
            "Training step failed");
        return NIMCP_ERROR;
    }

    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_train_batch(
    nimcp_brain_t brain,
    const float* features,
    const float* targets,
    uint32_t batch_size,
    uint32_t num_features,
    uint32_t num_targets,
    nimcp_training_result_t* result)
{
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "Brain handle is NULL in train_batch");
    NIMCP_CHECK_THROW(features, NIMCP_ERROR_NULL_ARG, "Features array is NULL in train_batch");
    NIMCP_CHECK_THROW(targets, NIMCP_ERROR_NULL_ARG, "Targets array is NULL in train_batch");
    NIMCP_CHECK_THROW(batch_size > 0, NIMCP_ERROR_INVALID, "Batch size cannot be zero");

    // BBB validation: validate input data buffers for batch
    size_t features_size = (size_t)batch_size * num_features * sizeof(float);
    size_t targets_size = (size_t)batch_size * num_targets * sizeof(float);
    if (!bbb_validate_buffer_access(features, 0, features_size, features_size, "nimcp_brain_train_batch")) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_INPUT, "BBB validation failed for batch features buffer");
        return NIMCP_ERROR_INVALID_INPUT;
    }
    if (!bbb_validate_buffer_access(targets, 0, targets_size, targets_size, "nimcp_brain_train_batch")) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_INPUT, "BBB validation failed for batch targets buffer");
        return NIMCP_ERROR_INVALID_INPUT;
    }

    // Train on each example and average results
    float total_loss = 0.0f;
    uint32_t processed = 0;
    nimcp_training_result_t step_result = {0};

    for (uint32_t i = 0; i < batch_size; i++) {
        const float* sample_features = features + (i * num_features);
        const float* sample_targets = targets + (i * num_targets);

        nimcp_status_t res = nimcp_brain_train_step(
            brain, sample_features, num_features,
            sample_targets, num_targets, &step_result);

        if (res != NIMCP_OK) {
            return res;
        }

        processed++;
        total_loss += step_result.loss;

        if (step_result.early_stopped) {
            break;
        }
    }

    if (result) {
        result->loss = total_loss / (processed > 0 ? processed : 1);
        result->learning_rate = step_result.learning_rate;
        result->step = step_result.step;
        result->early_stopped = step_result.early_stopped;
        result->gradient_norm = step_result.gradient_norm;
    }

    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_get_training_stats(
    nimcp_brain_t brain,
    uint64_t* total_steps,
    float* total_loss,
    float* current_lr)
{
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "Brain handle is NULL in get_training_stats");

    brain_t internal = brain->internal_brain;
    NIMCP_CHECK_THROW(internal, NIMCP_ERROR_NULL_ARG, "Internal brain is NULL in get_training_stats");
    NIMCP_CHECK_THROW(internal->training_ctx, NIMCP_ERROR_INVALID, "Training not enabled in get_training_stats");

    nimcp_training_session_stats_t stats;
    nimcp_result_t res = nimcp_brain_training_get_stats(internal->training_ctx, &stats);
    if (res != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to get training stats");
        return NIMCP_ERROR;
    }

    if (total_steps) *total_steps = stats.total_samples;
    if (total_loss) *total_loss = stats.total_loss;

    if (current_lr) {
        training_pipeline_state_t* state = get_training_state(brain);
        if (state && state->configured) {
            nimcp_lr_scheduler_ctx_t* sched = nimcp_brain_training_get_scheduler(
                internal->training_ctx, state->scheduler_id);
            if (sched) {
                *current_lr = nimcp_lr_scheduler_get_lr(sched);
            } else {
                *current_lr = internal->config.learning_rate;
            }
        } else {
            *current_lr = internal->config.learning_rate;
        }
    }

    return NIMCP_OK;
}

float nimcp_brain_step_scheduler(nimcp_brain_t brain, float validation_metric) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain handle is NULL in step_scheduler");
        return 0.0f;
    }

    brain_t internal = brain->internal_brain;
    if (!internal || !internal->training_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Training not enabled in step_scheduler");
        return 0.0f;
    }

    training_pipeline_state_t* state = get_training_state(brain);
    if (!state || !state->configured) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Training not configured in step_scheduler");
        return 0.0f;
    }

    // Step scheduler and update optimizer
    float new_lr = nimcp_brain_training_step_scheduler_metric(
        internal->training_ctx,
        state->scheduler_id,
        state->optimizer_id,
        validation_metric
    );

    return new_lr;
}

//=============================================================================
// Rubric Training Integration API
//=============================================================================

nimcp_status_t nimcp_brain_set_rubric_validation(
    nimcp_brain_t brain, const float* features, uint32_t num_features)
{
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "Brain handle is NULL");
    NIMCP_CHECK_THROW(features, NIMCP_ERROR_NULL_ARG, "Features array is NULL");
    NIMCP_CHECK_THROW(num_features > 0, NIMCP_ERROR_INVALID, "num_features must be > 0");

    training_pipeline_state_t* state = get_training_state(brain);
    if (!state) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(training_pipeline_state_t),
            "Failed to get training state for rubric validation");
    }

    /* Free previous validation features */
    if (state->rubric_validation_features) {
        nimcp_free(state->rubric_validation_features);
    }

    /* Copy features */
    state->rubric_validation_features = nimcp_malloc(num_features * sizeof(float));
    if (!state->rubric_validation_features) {
        state->rubric_validation_num_features = 0;
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, num_features * sizeof(float),
            "Failed to allocate rubric validation features");
    }
    memcpy(state->rubric_validation_features, features, num_features * sizeof(float));
    state->rubric_validation_num_features = num_features;

    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_get_rubric_training_stats(
    nimcp_brain_t brain,
    uint64_t* eval_count, float* min_score, float* max_score,
    float* avg_score, nimcp_rubric_t* last_rubric)
{
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "Brain handle is NULL");

    training_pipeline_state_t* state = get_training_state(brain);
    if (!state) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID, "No training state found");
    }

    if (eval_count) *eval_count = state->rubric_eval_count;
    if (min_score)  *min_score  = state->rubric_eval_count > 0 ? state->rubric_min_observed : 0.0f;
    if (max_score)  *max_score  = state->rubric_eval_count > 0 ? state->rubric_max_observed : 0.0f;
    if (avg_score)  *avg_score  = state->rubric_eval_count > 0
                                  ? (float)(state->rubric_score_sum / state->rubric_eval_count)
                                  : 0.0f;
    if (last_rubric && state->rubric_eval_count > 0) {
        *last_rubric = state->rubric_last;
    }

    return NIMCP_OK;
}

//=============================================================================
// Training Callbacks API
//=============================================================================

/**
 * @brief Wrapper to convert public callback to internal callback
 */
typedef struct {
    nimcp_training_callback_fn public_callback;
    void* user_data;
} callback_wrapper_t;

/**
 * @brief Internal callback that bridges public API to internal API
 */
static tcb_action_t callback_bridge(const tcb_event_t* event) {
    if (!event || !event->user_data) {
        return TCB_ACTION_CONTINUE;
    }

    callback_wrapper_t* wrapper = (callback_wrapper_t*)event->user_data;
    if (!wrapper->public_callback) {
        return TCB_ACTION_CONTINUE;
    }

    // Map internal event type to public event type
    nimcp_callback_event_t pub_event;
    switch (event->event_type) {
        case TCB_EVENT_STEP_COMPLETE:    pub_event = NIMCP_CB_STEP_COMPLETE; break;
        case TCB_EVENT_EPOCH_COMPLETE:   pub_event = NIMCP_CB_EPOCH_COMPLETE; break;
        case TCB_EVENT_LOSS_COMPUTED:    pub_event = NIMCP_CB_LOSS_COMPUTED; break;
        case TCB_EVENT_WEIGHTS_UPDATED:  pub_event = NIMCP_CB_WEIGHTS_UPDATED; break;
        case TCB_EVENT_LR_CHANGED:       pub_event = NIMCP_CB_LR_CHANGED; break;
        case TCB_EVENT_CONVERGENCE:      pub_event = NIMCP_CB_CONVERGENCE; break;
        case TCB_EVENT_DIVERGENCE:       pub_event = NIMCP_CB_DIVERGENCE; break;
        case TCB_EVENT_CHECKPOINT:       pub_event = NIMCP_CB_CHECKPOINT; break;
        default:                         pub_event = NIMCP_CB_STEP_COMPLETE; break;
    }

    // Convert metrics
    nimcp_callback_metrics_t pub_metrics = {
        .step = event->metrics.step,
        .epoch = event->metrics.epoch,
        .loss = event->metrics.loss,
        .loss_ema = event->metrics.loss_ema,
        .learning_rate = event->metrics.learning_rate,
        .gradient_norm = event->metrics.gradient_norm,
        .step_time_us = event->metrics.step_time_us,
        .is_converging = event->metrics.is_converging,
        .is_diverging = event->metrics.is_diverging
    };

    // Call public callback
    nimcp_callback_action_t pub_action = wrapper->public_callback(
        pub_event, &pub_metrics, wrapper->user_data);

    // Map public action to internal action
    switch (pub_action) {
        case NIMCP_CB_ACTION_CONTINUE:    return TCB_ACTION_CONTINUE;
        case NIMCP_CB_ACTION_STOP:        return TCB_ACTION_STOP_TRAINING;
        case NIMCP_CB_ACTION_SKIP:        return TCB_ACTION_SKIP_STEP;
        case NIMCP_CB_ACTION_ROLLBACK:    return TCB_ACTION_ROLLBACK;
        case NIMCP_CB_ACTION_REDUCE_LR:   return TCB_ACTION_REDUCE_LR;
        case NIMCP_CB_ACTION_INCREASE_LR: return TCB_ACTION_INCREASE_LR;
        default:                          return TCB_ACTION_CONTINUE;
    }
}

nimcp_callback_config_t nimcp_callback_config_default(void) {
    nimcp_callback_config_t config = {
        .enable_auto_checkpoint = false,
        .checkpoint_interval = 1000,
        .enable_early_stopping = true,
        .patience = 100,
        .min_delta = 1e-4f,
        .divergence_threshold = 10.0f,
        .log_interval = 0  // Disabled by default
    };
    return config;
}

nimcp_status_t nimcp_brain_enable_callbacks(
    nimcp_brain_t brain,
    const nimcp_callback_config_t* config)
{
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "Brain handle is NULL in enable_callbacks");

    training_pipeline_state_t* state = get_training_state(brain);
    if (!state) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(training_pipeline_state_t),
            "Failed to get training state");
    }

    // Destroy existing callbacks if present
    if (state->callbacks) {
        tcb_destroy(state->callbacks);
        state->callbacks = NULL;
    }

    // Build internal config from public config
    tcb_config_t internal_config = tcb_config_default();

    if (config) {
        internal_config.enable_auto_checkpoint = config->enable_auto_checkpoint;
        internal_config.checkpoint_interval = config->checkpoint_interval;
        internal_config.enable_early_stopping = config->enable_early_stopping;
        internal_config.patience = config->patience;
        internal_config.min_delta = config->min_delta;
        internal_config.divergence_threshold = config->divergence_threshold;
        if (config->log_interval > 0) {
            internal_config.enable_auto_logging = true;
            internal_config.log_interval = config->log_interval;
        }
    }

    // Create callback manager
    state->callbacks = tcb_create(&internal_config);
    if (!state->callbacks) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, 0, "Failed to create callback manager");
    }

    state->callbacks_enabled = true;
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_disable_callbacks(nimcp_brain_t brain) {
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "Brain handle is NULL in disable_callbacks");

    training_pipeline_state_t* state = get_training_state(brain);
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_INVALID, "No training state in disable_callbacks");

    state->callbacks_enabled = false;
    return NIMCP_OK;
}

// Track callback wrappers for cleanup (thread-safe)
#define MAX_CALLBACK_WRAPPERS 256
static callback_wrapper_t* g_callback_wrappers[MAX_CALLBACK_WRAPPERS] = {0};
static uint32_t g_next_wrapper_id = 0;
static nimcp_mutex_t g_callback_wrappers_mutex = NIMCP_MUTEX_INITIALIZER;

uint32_t nimcp_brain_register_callback(
    nimcp_brain_t brain,
    nimcp_callback_event_t event,
    nimcp_training_callback_fn callback,
    void* user_data,
    const char* name)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain handle is NULL in register_callback");
        return 0;
    }

    if (!callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Callback function is NULL in register_callback");
        return 0;
    }

    training_pipeline_state_t* state = get_training_state(brain);
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "No training state in register_callback");
        return 0;
    }

    // Auto-enable callbacks if not already enabled
    if (!state->callbacks) {
        nimcp_status_t res = nimcp_brain_enable_callbacks(brain, NULL);
        if (res != NIMCP_OK) {
            return 0;
        }
    }

    // Allocate wrapper
    callback_wrapper_t* wrapper = nimcp_malloc(sizeof(callback_wrapper_t));
    if (!wrapper) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(callback_wrapper_t),
            "Failed to allocate callback wrapper");
        return 0;
    }

    wrapper->public_callback = callback;
    wrapper->user_data = user_data;

    // Store wrapper for cleanup (thread-safe)
    nimcp_mutex_lock(&g_callback_wrappers_mutex);
    uint32_t wrapper_idx = g_next_wrapper_id % MAX_CALLBACK_WRAPPERS;
    if (g_callback_wrappers[wrapper_idx]) {
        // Mark stale wrapper as inactive before freeing to prevent use-after-free
        // if internal callback manager still holds a reference
        g_callback_wrappers[wrapper_idx]->public_callback = NULL;
        g_callback_wrappers[wrapper_idx]->user_data = NULL;
        nimcp_free(g_callback_wrappers[wrapper_idx]);
        g_callback_wrappers[wrapper_idx] = NULL;
    }
    g_callback_wrappers[wrapper_idx] = wrapper;
    g_next_wrapper_id++;
    nimcp_mutex_unlock(&g_callback_wrappers_mutex);

    // Map public event type to internal event type
    tcb_event_type_t internal_event;
    switch (event) {
        case NIMCP_CB_STEP_COMPLETE:    internal_event = TCB_EVENT_STEP_COMPLETE; break;
        case NIMCP_CB_EPOCH_COMPLETE:   internal_event = TCB_EVENT_EPOCH_COMPLETE; break;
        case NIMCP_CB_LOSS_COMPUTED:    internal_event = TCB_EVENT_LOSS_COMPUTED; break;
        case NIMCP_CB_WEIGHTS_UPDATED:  internal_event = TCB_EVENT_WEIGHTS_UPDATED; break;
        case NIMCP_CB_LR_CHANGED:       internal_event = TCB_EVENT_LR_CHANGED; break;
        case NIMCP_CB_CONVERGENCE:      internal_event = TCB_EVENT_CONVERGENCE; break;
        case NIMCP_CB_DIVERGENCE:       internal_event = TCB_EVENT_DIVERGENCE; break;
        case NIMCP_CB_CHECKPOINT:       internal_event = TCB_EVENT_CHECKPOINT; break;
        default:                        internal_event = TCB_EVENT_STEP_COMPLETE; break;
    }

    // Register with internal callback manager
    tcb_callback_info_t info = {
        .callback = callback_bridge,
        .user_data = wrapper,
        .event_type = internal_event,
        .mode = TCB_MODE_SYNC,
        .priority = TCB_PRIORITY_NORMAL,
        .name = name,
        .enabled = true
    };

    uint32_t cb_id = tcb_register(state->callbacks, &info);
    if (cb_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to register callback");
        return 0;
    }

    return cb_id;
}

nimcp_status_t nimcp_brain_unregister_callback(
    nimcp_brain_t brain,
    uint32_t callback_id)
{
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "Brain handle is NULL in unregister_callback");

    training_pipeline_state_t* state = get_training_state(brain);
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_INVALID, "No training state in unregister_callback");
    NIMCP_CHECK_THROW(state->callbacks, NIMCP_ERROR_INVALID, "Callbacks not enabled in unregister_callback");

    // Unregister from internal manager
    if (!tcb_unregister(state->callbacks, callback_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to unregister callback %u", callback_id);
        return NIMCP_ERROR;
    }

    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_get_callback_stats(
    nimcp_brain_t brain,
    uint64_t* total_fired,
    float* avg_time_us,
    uint32_t* early_stops)
{
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "Brain handle is NULL in get_callback_stats");

    training_pipeline_state_t* state = get_training_state(brain);
    if (!state || !state->callbacks) {
        // No callbacks configured - return zeros
        if (total_fired) *total_fired = 0;
        if (avg_time_us) *avg_time_us = 0.0f;
        if (early_stops) *early_stops = 0;
        return NIMCP_OK;
    }

    // Get stats from internal manager
    tcb_stats_t stats;
    tcb_get_stats(state->callbacks, &stats);

    if (total_fired) *total_fired = stats.total_callbacks_fired;
    if (avg_time_us) *avg_time_us = stats.avg_execution_time_us;
    if (early_stops) *early_stops = stats.early_stops_triggered;

    return NIMCP_OK;
}
