// nimcp_part_accessors.c - accessors functions
// Part of nimcp.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp.c


/* Wrapper for exception integration macros */
static void api_set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_last_error, sizeof(g_last_error), fmt, args);
    va_end(args);
}


const char* nimcp_get_error(void) {
    return g_last_error;
}


nimcp_status_t nimcp_brain_workspace_has_broadcast(
    nimcp_brain_t brain,
    bool* has_broadcast)
{
    // Guard: Validate brain and output parameter FIRST (before dereferencing subsystems)
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain provided to workspace_has_broadcast");
    NIMCP_CHECK_THROW(has_broadcast, NIMCP_ERROR_NULL_ARG, "NULL has_broadcast parameter");
    NIMCP_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain");

    // Guard: Check if global workspace enabled
    global_workspace_t* workspace = brain_get_global_workspace(brain->internal_brain);
    NIMCP_CHECK_THROW(workspace, NIMCP_ERROR_INVALID, "Global workspace not enabled in brain config");

    // Check for broadcast
    *has_broadcast = global_workspace_has_broadcast(workspace);
    set_error("No error");
    return NIMCP_OK;
}


/**
 * @brief Check if complex oscillation features are enabled
 */
bool nimcp_is_complex_oscillations_enabled(nimcp_brain_t brain) {
    if (!brain || !brain->internal_brain) {
        return false;
    }

    return brain_complex_oscillation_is_enabled(brain->internal_brain);
}


/**
 * @brief Get oscillation phasor for a specific neuron
 */
nimcp_oscillation_phasor_t nimcp_get_oscillation_phasor(
    nimcp_brain_t brain,
    uint32_t neuron_id)
{
    nimcp_oscillation_phasor_t result = {0.0F, 0.0F};

    // Guard: Validate parameters
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_get_oscillation_phasor: brain is NULL");
        return result;
    }

    if (!brain->internal_brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_get_oscillation_phasor: internal_brain is NULL");
        return result;
    }

    // Check if complex oscillations are enabled
    if (!brain_complex_oscillation_is_enabled(brain->internal_brain)) {
        set_error("Complex oscillations not enabled - call nimcp_enable_complex_oscillations first");
        return result;
    }

    // Get brain's oscillation analyzer
    brain_oscillation_analyzer_t* analyzer = brain_get_oscillations(brain->internal_brain);
    if (!analyzer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "nimcp_get_oscillation_phasor: analyzer not available");
        return result;
    }

    // BUG FIX: Previous code cast brain_oscillation_analyzer_t* to
    // brain_complex_oscillation_state_t* — these are unrelated structs.
    // This violated strict aliasing and caused undefined behavior.
    // Return safe defaults until a proper accessor is added to the internal API.
    (void)analyzer;
    set_error("Complex oscillation phasor extraction not yet wired to analyzer");
    result.amplitude = 0.0F;
    result.phase = 0.0F;
    return result;
}


/**
 * @brief Compute phase coherence across multiple neurons
 */
float nimcp_get_phase_coherence(
    nimcp_brain_t brain,
    const uint32_t* neuron_ids,
    uint32_t count)
{
    // Guard: Validate parameters
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_get_phase_coherence: brain is NULL");
        return 0.0F;
    }

    if (!brain->internal_brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_get_phase_coherence: internal_brain is NULL");
        return 0.0F;
    }

    if (!neuron_ids) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_get_phase_coherence: neuron_ids is NULL");
        return 0.0F;
    }

    if (count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_get_phase_coherence: count is 0");
        return 0.0F;
    }

    // Check if complex oscillations are enabled
    if (!brain_complex_oscillation_is_enabled(brain->internal_brain)) {
        set_error("Complex oscillations not enabled");
        return 0.0F;
    }

    // Get brain's oscillation analyzer
    brain_oscillation_analyzer_t* analyzer = brain_get_oscillations(brain->internal_brain);
    if (!analyzer) {
        set_error("Brain oscillation analyzer not available");
        return 0.0F;
    }

    // BUG FIX: Removed strict-aliasing-violating cast from unrelated
    // brain_oscillation_analyzer_t* to brain_complex_oscillation_state_t*.
    // Return safe default until a proper accessor is available.
    (void)analyzer;
    (void)neuron_ids;
    (void)count;
    set_error("Complex oscillation coherence not yet wired to analyzer");
    return 0.0F;
}


/**
 * @brief Compute phase-amplitude coupling (PAC) modulation index
 */
float nimcp_get_pac_modulation(
    nimcp_brain_t brain,
    float theta_freq,
    float gamma_freq)
{
    // Guard: Validate parameters
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_get_pac_modulation: brain is NULL");
        return 0.0F;
    }

    if (!brain->internal_brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_get_pac_modulation: internal_brain is NULL");
        return 0.0F;
    }

    // Validate frequency ranges
    if (theta_freq < 4.0F || theta_freq > 8.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_get_pac_modulation: theta_freq out of range 4-8 Hz");
        return 0.0F;
    }

    if (gamma_freq < 30.0F || gamma_freq > 100.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_get_pac_modulation: gamma_freq out of range 30-100 Hz");
        return 0.0F;
    }

    // Check if complex oscillations are enabled
    if (!brain_complex_oscillation_is_enabled(brain->internal_brain)) {
        set_error("Complex oscillations not enabled");
        return 0.0F;
    }

    // Get brain's oscillation analyzer
    brain_oscillation_analyzer_t* analyzer = brain_get_oscillations(brain->internal_brain);
    if (!analyzer) {
        set_error("Brain oscillation analyzer not available");
        return 0.0F;
    }

    // BUG FIX: Removed strict-aliasing-violating cast from unrelated
    // brain_oscillation_analyzer_t* to brain_complex_oscillation_state_t*.
    // Return safe default until a proper accessor is available.
    (void)analyzer;
    (void)theta_freq;
    (void)gamma_freq;
    set_error("PAC modulation not yet wired to analyzer");
    return 0.0F;
}


uint32_t nimcp_brain_get_neuron_count(nimcp_brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_get_neuron_count: brain is NULL");
        return 0;
    }

    if (!brain->internal_brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_get_neuron_count: internal_brain is NULL");
        return 0;
    }
    return brain_get_neuron_count(brain->internal_brain);
}


uint32_t nimcp_brain_retrofit_synapse_metadata(nimcp_brain_t brain) {
    if (!brain || !brain->internal_brain) return 0;
    brain_t b = brain->internal_brain;
    if (!b->network) return 0;
    extern uint32_t neural_network_retrofit_metadata(void*);
    return neural_network_retrofit_metadata(b->network);
}

bool nimcp_brain_get_utilization_metrics(nimcp_brain_t brain, float* utilization, float* saturation) {
    if (!brain) {
        set_error("Brain handle is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_get_utilization_metrics: brain is NULL");
        return false;
    }

    if (!brain->internal_brain) {
        set_error("Brain has NULL internal_brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_get_utilization_metrics: internal_brain is NULL");
        return false;
    }

    if (!utilization || !saturation) {
        set_error("Output parameters are NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_get_utilization_metrics: required parameter is NULL (utilization, saturation)");
        return false;
    }

    return brain_get_utilization_metrics(brain->internal_brain, utilization, saturation);
}


nimcp_training_config_t nimcp_training_config_default(void) {
    nimcp_training_config_t config = {
        .loss_type = NIMCP_API_LOSS_CROSS_ENTROPY,
        .optimizer_type = NIMCP_API_OPT_ADAM,
        .scheduler_type = NIMCP_API_SCHED_COSINE,

        .learning_rate = 0.001F,
        .weight_decay = 0.0F,
        .momentum = 0.9F,
        .beta1 = 0.9F,
        .beta2 = 0.999F,
        .epsilon = 1e-8F,

        .scheduler_step_size = 1000,
        .scheduler_gamma = 0.1F,
        .warmup_steps = 0,

        .enable_gradient_clipping = true,
        .gradient_clip_value = 1.0F,

        .enable_biological_modulation = true,
        .biological_blend = 0.5F,

        // Network type dispatch defaults
        .network_type = NIMCP_NETWORK_ADAPTIVE,

        // SNN defaults
        .snn_method = NIMCP_SNN_TRAIN_SURROGATE,
        .snn_eligibility_tau = 20.0F,
        .snn_reward_tau = 100.0F,
        .snn_surrogate_beta = 5.0F,

        // LNN defaults
        .lnn_method = NIMCP_LNN_TRAIN_ADJOINT,
        .lnn_bptt_truncation = 100,
        .lnn_use_adjoint_checkpointing = true,

        // Rubric defaults
        .enable_rubric = false,
        .rubric_interval = 0,
        .rubric_min_score = 0.0F,
        .rubric_stop_on_threshold = false
    };
    return config;
}


nimcp_status_t nimcp_brain_configure_training(
    nimcp_brain_t brain,
    const nimcp_training_config_t* config)
{
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "Brain handle is NULL");
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_ARG, "Training config is NULL");

    // Get internal brain
    brain_t internal = brain->internal_brain;
    NIMCP_CHECK_THROW(internal, NIMCP_ERROR_NULL_ARG, "Internal brain is NULL");

    // Get or create training context
    nimcp_brain_training_ctx_t* training_ctx = internal->training_ctx;
    NIMCP_CHECK_THROW(training_ctx, NIMCP_ERROR_INVALID, "Brain has no training context (training not enabled)");

    // Get training state
    training_pipeline_state_t* state = get_training_state(brain);
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_MEMORY, "Failed to allocate training state");

    // Map public loss type to internal loss type
    // Note: Public API uses nimcp_loss_type_t from nimcp.h
    // Internal uses nimcp_loss_type_t from nimcp_loss_functions.h (same name, different values)
    nimcp_loss_type_t internal_loss;
    switch (config->loss_type) {
        case 0:  internal_loss = NIMCP_LOSS_MSE; break;                    // NIMCP_LOSS_MSE
        case 1:  internal_loss = NIMCP_LOSS_CROSS_ENTROPY; break;          // NIMCP_LOSS_CROSS_ENTROPY
        case 2:  internal_loss = NIMCP_LOSS_BINARY_CROSS_ENTROPY; break;   // NIMCP_LOSS_BINARY_CE
        case 3:  internal_loss = NIMCP_LOSS_HUBER; break;                  // NIMCP_LOSS_HUBER
        case 4:  internal_loss = NIMCP_LOSS_MAE; break;                    // NIMCP_LOSS_MAE
        case 5:  internal_loss = NIMCP_LOSS_FOCAL; break;                  // NIMCP_LOSS_FOCAL
        case 6:  internal_loss = NIMCP_LOSS_KL_DIVERGENCE; break;          // NIMCP_LOSS_KL_DIV
        default: internal_loss = NIMCP_LOSS_MSE; break;
    }

    // Create loss function
    nimcp_loss_config_t loss_config = nimcp_loss_default_config(internal_loss);
    nimcp_result_t res = nimcp_brain_training_create_loss(training_ctx, &loss_config, &state->loss_id);
    NIMCP_CHECK_THROW(res == NIMCP_SUCCESS && state->loss_id != 0, NIMCP_ERROR,
                      "Failed to create loss function");

    // Map public optimizer type to internal optimizer type
    nimcp_optimizer_type_t internal_opt;
    switch (config->optimizer_type) {
        case 0:  internal_opt = NIMCP_OPTIMIZER_SGD; break;          // NIMCP_OPT_SGD
        case 1:  internal_opt = NIMCP_OPTIMIZER_SGD_MOMENTUM; break; // NIMCP_OPT_MOMENTUM
        case 2:  internal_opt = NIMCP_OPTIMIZER_ADAM; break;         // NIMCP_OPT_ADAM
        case 3:  internal_opt = NIMCP_OPTIMIZER_ADAMW; break;        // NIMCP_OPT_ADAMW
        case 4:  internal_opt = NIMCP_OPTIMIZER_RMSPROP; break;      // NIMCP_OPT_RMSPROP
        case 5:  internal_opt = NIMCP_OPTIMIZER_ADAGRAD; break;      // NIMCP_OPT_ADAGRAD
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
    NIMCP_CHECK_THROW(res == NIMCP_SUCCESS && state->optimizer_id != 0, NIMCP_ERROR,
                      "Failed to create optimizer");

    // Map public scheduler type to internal scheduler type
    nimcp_lr_scheduler_type_t internal_sched;
    switch (config->scheduler_type) {
        case 0:  internal_sched = NIMCP_LR_CONSTANT; break;           // NIMCP_LR_CONSTANT
        case 1:  internal_sched = NIMCP_LR_STEP; break;               // NIMCP_LR_STEP
        case 2:  internal_sched = NIMCP_LR_EXPONENTIAL; break;        // NIMCP_LR_EXPONENTIAL
        case 3:  internal_sched = NIMCP_LR_COSINE_ANNEALING; break;   // NIMCP_LR_COSINE
        case 4:  internal_sched = NIMCP_LR_COSINE_WARMUP; break;      // NIMCP_LR_WARMUP_COSINE
        case 5:  internal_sched = NIMCP_LR_REDUCE_ON_PLATEAU; break;  // NIMCP_LR_REDUCE_ON_PLATEAU
        case 6:  internal_sched = NIMCP_LR_CYCLIC; break;             // NIMCP_LR_CYCLIC
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
            sched_config.params.cyclic.base_lr = config->learning_rate * 0.1F;
            sched_config.params.cyclic.max_lr = config->learning_rate;
            sched_config.params.cyclic.step_size_up = config->scheduler_step_size;
            break;
        default:
            break;
    }

    res = nimcp_brain_training_create_scheduler(training_ctx, &sched_config, &state->scheduler_id);
    NIMCP_CHECK_THROW(res == NIMCP_SUCCESS && state->scheduler_id != 0, NIMCP_ERROR,
                      "Failed to create LR scheduler");

    // Use existing gradient manager if available, or create one
    state->gradmgr_id = 1;  // Default gradient manager created during brain init

    // Configure biological modulation
    if (config->enable_biological_modulation && internal->plasticity_bridge) {
        nimcp_brain_training_set_biological_modulation(training_ctx, config->biological_blend);
    }

    // Initialize training dispatcher for SNN/LNN/CNN/Adaptive routing
    // This sets up specialized training contexts based on network_type
    if (training_dispatch_init(internal, config) < 0) {
        // Dispatcher init failed - this is OK for ADAPTIVE type (returns -2)
        // For other types, log warning but continue with fallback to backprop
        if (config->network_type != NIMCP_NETWORK_ADAPTIVE) {
            LOG_WARN("Training dispatcher init failed for network_type=%d, falling back to adaptive",
                     config->network_type);
        }
    }

    // Store network type in internal brain for dispatch during train_step
    internal->active_network_type = config->network_type;

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

    set_error("No error");
    return NIMCP_OK;
}


nimcp_status_t nimcp_brain_get_training_stats(
    nimcp_brain_t brain,
    uint64_t* total_steps,
    float* total_loss,
    float* current_lr)
{
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "Brain handle is NULL");

    brain_t internal = brain->internal_brain;
    NIMCP_CHECK_THROW(internal && internal->training_ctx, NIMCP_ERROR_INVALID, "Training not enabled");

    nimcp_training_session_stats_t stats;
    nimcp_result_t res = nimcp_brain_training_get_stats(internal->training_ctx, &stats);
    NIMCP_CHECK_THROW(res == NIMCP_SUCCESS, NIMCP_ERROR, "Failed to get training stats");

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

    set_error("No error");
    return NIMCP_OK;
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
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_MEMORY, "Failed to get training state");

    /* Free previous validation features */
    if (state->rubric_validation_features) {
        nimcp_free(state->rubric_validation_features);
    }

    /* Copy features */
    state->rubric_validation_features = nimcp_malloc(num_features * sizeof(float));
    if (!state->rubric_validation_features) {
        state->rubric_validation_num_features = 0;
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_MEMORY, "Failed to allocate rubric validation features");
    }
    memcpy(state->rubric_validation_features, features, num_features * sizeof(float));
    state->rubric_validation_num_features = num_features;

    set_error("No error");
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_get_rubric_training_stats(
    nimcp_brain_t brain,
    uint64_t* eval_count, float* min_score, float* max_score,
    float* avg_score, nimcp_rubric_t* last_rubric)
{
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "Brain handle is NULL");

    training_pipeline_state_t* state = get_training_state(brain);
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_INVALID, "No training state found");

    if (eval_count) *eval_count = state->rubric_eval_count;
    if (min_score)  *min_score  = state->rubric_eval_count > 0 ? state->rubric_min_observed : 0.0f;
    if (max_score)  *max_score  = state->rubric_eval_count > 0 ? state->rubric_max_observed : 0.0f;
    if (avg_score)  *avg_score  = state->rubric_eval_count > 0
                                  ? (float)(state->rubric_score_sum / state->rubric_eval_count)
                                  : 0.0f;
    if (last_rubric && state->rubric_eval_count > 0) {
        *last_rubric = state->rubric_last;
    }

    set_error("No error");
    return NIMCP_OK;
}


nimcp_callback_config_t nimcp_callback_config_default(void) {
    nimcp_callback_config_t config = {
        .enable_auto_checkpoint = false,
        .checkpoint_interval = 1000,
        .enable_early_stopping = true,
        .patience = 100,
        .min_delta = 1e-4F,
        .divergence_threshold = 10.0F,
        .log_interval = 0  // Disabled by default
    };
    return config;
}


nimcp_status_t nimcp_brain_get_callback_stats(
    nimcp_brain_t brain,
    uint64_t* total_fired,
    float* avg_time_us,
    uint32_t* early_stops)
{
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "Brain handle is NULL");

    training_pipeline_state_t* state = get_training_state(brain);
    if (!state || !state->callbacks) {
        // No callbacks configured - return zeros
        if (total_fired) *total_fired = 0;
        if (avg_time_us) *avg_time_us = 0.0F;
        if (early_stops) *early_stops = 0;
        set_error("No error");
        return NIMCP_OK;
    }

    // Get stats from internal manager
    tcb_stats_t stats;
    tcb_get_stats(state->callbacks, &stats);

    if (total_fired) *total_fired = stats.total_callbacks_fired;
    if (avg_time_us) *avg_time_us = stats.avg_execution_time_us;
    if (early_stops) *early_stops = stats.early_stops_triggered;

    set_error("No error");
    return NIMCP_OK;
}

/* ============================================================================
 * nimcp_brain_get_theory_of_mind — accessor for swarm ToM bridge
 *
 * Returns the brain's theory_of_mind pointer as void*, so edge code
 * can call ToM functions without including brain_internal.h.
 * ============================================================================ */

void* nimcp_brain_get_theory_of_mind(void* internal_brain)
{
    if (!internal_brain) {
        return NULL;
    }
    brain_t brain = (brain_t)internal_brain;
    return (void*)brain->theory_of_mind;
}
