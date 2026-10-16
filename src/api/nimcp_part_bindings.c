// nimcp_part_bindings.c - Public C API wrappers for binding languages (C#, Go, etc.)
// Part of nimcp.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp.c
//
// These wrappers expose internal brain subsystem accessors through the public
// nimcp_brain_t handle, so language bindings can P/Invoke without touching
// internal_brain directly.

#include "cognitive/training/nimcp_training_integration.h"
#include "cognitive/nimcp_sleep_wake.h"
#include "lnn/nimcp_lnn.h"
#include "lnn/nimcp_lnn_training.h"
#include "snn/nimcp_snn.h"
#include "perception/nimcp_visual_cortex.h"
#include "training/nimcp_cortex_cnn.h"
#include "training/nimcp_cnn_training.h"  /* cnn_get_layer_count, cnn_count_parameters */

// =========================================================================
// Group 0 — Cortex CNN Initialization
// =========================================================================

/**
 * @brief Create all 4 cortex CNN processors with FNO spectral processing.
 * Call this once after brain creation + enable_multi_network to initialize
 * perceptual cortices. Does NOT require staged sensory data or learning.
 */
nimcp_status_t nimcp_brain_init_cortex_cnns(nimcp_brain_t brain) {
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain");
    API_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "NULL internal_brain");
    brain_t ib = brain->internal_brain;

    extern void* fno_audio_create(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
    extern void cortex_cnn_set_fno_audio(cortex_cnn_processor_t*, void*);
    extern void cortex_cnn_set_fno_visual(cortex_cnn_processor_t*, void*);
    extern void cortex_cnn_set_fno_speech(cortex_cnn_processor_t*, void*);

    int created = 0;
    if (!ib->cortex_cnns[0]) {
        ib->cortex_cnns[0] = cortex_cnn_create(CORTEX_CNN_VISUAL, 0);
        if (ib->cortex_cnns[0]) {
            void* fno = fno_audio_create(1024, 64, 16, 32, 2);
            if (fno) cortex_cnn_set_fno_visual(ib->cortex_cnns[0], fno);
            created++;
        }
    }
    if (!ib->cortex_cnns[1]) {
        ib->cortex_cnns[1] = cortex_cnn_create(CORTEX_CNN_AUDIO, 0);
        if (ib->cortex_cnns[1]) {
            void* fno = fno_audio_create(128, 64, 16, 32, 2);
            if (fno) cortex_cnn_set_fno_audio(ib->cortex_cnns[1], fno);
            created++;
        }
    }
    if (!ib->cortex_cnns[2]) {
        ib->cortex_cnns[2] = cortex_cnn_create(CORTEX_CNN_SPEECH, 0);
        if (ib->cortex_cnns[2]) {
            void* fno = fno_audio_create(128, 64, 16, 32, 2);
            if (fno) cortex_cnn_set_fno_speech(ib->cortex_cnns[2], fno);
            created++;
        }
    }
    if (!ib->cortex_cnns[3]) {
        ib->cortex_cnns[3] = cortex_cnn_create(CORTEX_CNN_SOMATO, 0);
        if (ib->cortex_cnns[3]) created++;
    }

    NIMCP_LOGGING_INFO("nimcp_brain_init_cortex_cnns: created %d cortex CNN processors", created);
    return NIMCP_OK;
}

// =========================================================================
// Group 1 — Sensory / Multimodal
// =========================================================================

/**
 * @brief Submit sensory data for a specific modality
 *
 * Stages sensory data into the brain's internal buffers for the next
 * decide_full() call. Supported modalities: "visual", "audio", "speech",
 * "somatosensory" (or "somato").
 *
 * @param brain Brain handle
 * @param modality Modality string
 * @param data Float array of sensory data
 * @param num_elements Number of elements in data
 * @param width Image width (visual only, 0 otherwise)
 * @param height Image height (visual only, 0 otherwise)
 * @param channels Image channels (visual only, 0 otherwise)
 * @param n_segments Number of segments (somatosensory only, 0 otherwise)
 * @return NIMCP_OK on success
 */
nimcp_status_t nimcp_brain_submit_sensory(
    nimcp_brain_t brain,
    const char* modality,
    const float* data, uint32_t num_elements,
    uint32_t width, uint32_t height, uint32_t channels,
    uint32_t n_segments)
{
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain");
    API_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "NULL internal_brain");
    API_CHECK_THROW(modality, NIMCP_ERROR_NULL_ARG, "NULL modality");
    API_CHECK_THROW(data, NIMCP_ERROR_NULL_ARG, "NULL data");
    API_CHECK_THROW(num_elements > 0, NIMCP_ERROR_INVALID, "Empty data");

    brain_t ib = brain->internal_brain;

    if (strcmp(modality, "somatosensory") == 0 || strcmp(modality, "somato") == 0) {
        if (ib->staged_sensory.somato_data) {
            nimcp_free(ib->staged_sensory.somato_data);
        }
        float* copy = (float*)nimcp_malloc(num_elements * sizeof(float));
        API_CHECK_THROW(copy, NIMCP_ERROR_MEMORY, "OOM staging somatosensory");
        memcpy(copy, data, num_elements * sizeof(float));
        ib->staged_sensory.somato_data = copy;
        ib->staged_sensory.somato_segments = (n_segments > 0) ? n_segments : num_elements;
    } else if (strcmp(modality, "visual") == 0) {
        if (ib->staged_sensory.visual_frame) {
            nimcp_free(ib->staged_sensory.visual_frame);
        }
        uint8_t* pixels = (uint8_t*)nimcp_malloc((size_t)num_elements);
        API_CHECK_THROW(pixels, NIMCP_ERROR_MEMORY, "OOM staging visual");
        for (uint32_t i = 0; i < num_elements; i++) {
            float v = data[i];
            if (v <= 1.0f && v >= 0.0f) v *= 255.0f;
            pixels[i] = (uint8_t)(v > 255.0f ? 255 : (v < 0.0f ? 0 : v));
        }
        ib->staged_sensory.visual_frame = pixels;
        ib->staged_sensory.visual_width  = (width > 0) ? width : 32;
        ib->staged_sensory.visual_height = (height > 0) ? height : 32;
        ib->staged_sensory.visual_channels = (channels > 0) ? channels : 3;
    } else if (strcmp(modality, "audio") == 0) {
        if (ib->staged_sensory.audio_data) {
            nimcp_free(ib->staged_sensory.audio_data);
        }
        float* copy = (float*)nimcp_malloc(num_elements * sizeof(float));
        API_CHECK_THROW(copy, NIMCP_ERROR_MEMORY, "OOM staging audio");
        memcpy(copy, data, num_elements * sizeof(float));
        ib->staged_sensory.audio_data = copy;
        ib->staged_sensory.audio_size = num_elements;
    } else if (strcmp(modality, "speech") == 0) {
        if (ib->staged_sensory.speech_data) {
            nimcp_free(ib->staged_sensory.speech_data);
        }
        float* copy = (float*)nimcp_malloc(num_elements * sizeof(float));
        API_CHECK_THROW(copy, NIMCP_ERROR_MEMORY, "OOM staging speech");
        memcpy(copy, data, num_elements * sizeof(float));
        ib->staged_sensory.speech_data = copy;
        ib->staged_sensory.speech_size = num_elements;
    } else {
        set_error("Unknown modality: %s", modality);
        return NIMCP_ERROR_INVALID;
    }

    set_error("No error");
    return NIMCP_OK;
}

/**
 * @brief Process image through the brain's visual cortex
 *
 * @param brain Brain handle
 * @param pixels Float pixel values ([0-1] or [0-255])
 * @param num_pixels Number of pixel values
 * @param width Image width
 * @param height Image height
 * @param channels Number of channels (1=grayscale, 3=RGB)
 * @param out_features Output feature array (caller-allocated)
 * @param max_features Maximum features to write
 * @param out_feature_count Actual features written
 * @return NIMCP_OK on success
 */
nimcp_status_t nimcp_brain_visual_cortex_process(
    nimcp_brain_t brain,
    const float* pixels, uint32_t num_pixels,
    uint32_t width, uint32_t height, uint32_t channels,
    float* out_features, uint32_t max_features,
    uint32_t* out_feature_count)
{
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain");
    API_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "NULL internal_brain");
    API_CHECK_THROW(pixels, NIMCP_ERROR_NULL_ARG, "NULL pixels");
    API_CHECK_THROW(out_features, NIMCP_ERROR_NULL_ARG, "NULL out_features");
    API_CHECK_THROW(out_feature_count, NIMCP_ERROR_NULL_ARG, "NULL out_feature_count");

    brain_t ib = brain->internal_brain;
    if (!ib->visual_cortex) {
        *out_feature_count = 0;
        set_error("Visual cortex not initialized");
        return NIMCP_OK;  /* non-fatal: just return empty */
    }

    uint32_t feat_dim = visual_cortex_get_feature_dim(ib->visual_cortex);
    if (feat_dim == 0) feat_dim = 128;
    if (feat_dim > max_features) feat_dim = max_features;

    /* Convert float [0,1] to uint8 [0,255] */
    uint8_t* px = (uint8_t*)nimcp_malloc((size_t)num_pixels);
    API_CHECK_THROW(px, NIMCP_ERROR_MEMORY, "OOM converting pixels");
    for (uint32_t i = 0; i < num_pixels; i++) {
        float v = pixels[i];
        if (v <= 1.0f && v >= 0.0f) v *= 255.0f;
        px[i] = (uint8_t)(v > 255.0f ? 255 : (v < 0.0f ? 0 : v));
    }

    float* features = (float*)nimcp_calloc(feat_dim, sizeof(float));
    if (!features) { nimcp_free(px); return NIMCP_ERROR_MEMORY; }

    bool ok = visual_cortex_process(ib->visual_cortex, px, width, height, channels, features);
    nimcp_free(px);

    if (!ok) {
        nimcp_free(features);
        *out_feature_count = 0;
        return NIMCP_OK;
    }

    memcpy(out_features, features, feat_dim * sizeof(float));
    *out_feature_count = feat_dim;
    nimcp_free(features);

    set_error("No error");
    return NIMCP_OK;
}

// =========================================================================
// Group 2 — Avatar / Metrics (get_avatar_state, get_network_metrics already public)
// =========================================================================

/**
 * @brief Get per-cortex CNN processor metrics
 *
 * Returns metrics for each cortex CNN processor as flat arrays.
 *
 * @param brain Brain handle
 * @param out_types Array of 4 type IDs
 * @param out_losses Array of 4 EMA losses
 * @param out_fwd_steps Array of 4 forward step counts
 * @param out_bwd_steps Array of 4 backward step counts
 * @param out_embed_norms Array of 4 embedding norms
 * @param out_count Number of active processors written
 * @return NIMCP_OK on success
 */
nimcp_status_t nimcp_brain_get_cortex_cnn_metrics(
    nimcp_brain_t brain,
    int* out_types,
    float* out_losses,
    uint64_t* out_fwd_steps,
    uint64_t* out_bwd_steps,
    float* out_embed_norms,
    uint32_t* out_count)
{
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain");
    API_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "NULL internal_brain");

    brain_t ib = brain->internal_brain;
    uint32_t cnt = 0;

    for (int ci = 0; ci < 4; ci++) {
        if (!ib->cortex_cnns[ci]) continue;

        cortex_cnn_metrics_t m = {0};

        if (cortex_cnn_get_metrics(ib->cortex_cnns[ci], &m) != 0) continue;

        if (out_types)      out_types[cnt]      = m.type;
        if (out_losses)     out_losses[cnt]     = m.ema_loss;
        if (out_fwd_steps)  out_fwd_steps[cnt]  = m.forward_steps;
        if (out_bwd_steps)  out_bwd_steps[cnt]  = m.backward_steps;
        if (out_embed_norms) out_embed_norms[cnt] = m.embedding_norm;
        cnt++;
    }

    if (out_count) *out_count = cnt;
    set_error("No error");
    return NIMCP_OK;
}

// =========================================================================
// Group 4 — LNN / SNN / CNN
// =========================================================================

/**
 * @brief Create NCP-architecture LNN temporal processor
 */
nimcp_status_t nimcp_brain_lnn_create(
    nimcp_brain_t brain,
    uint32_t n_sensory, uint32_t n_inter,
    uint32_t n_command, uint32_t n_output)
{
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain");
    API_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "NULL internal_brain");

    brain_t ib = brain->internal_brain;

    if (ib->lnn_network) {
        /* Already created — idempotent */
        set_error("No error");
        return NIMCP_OK;
    }

    if (!lnn_is_initialized()) {
        lnn_init(1);
    }

    ib->lnn_network = lnn_network_create_ncp(n_sensory, n_inter, n_command, n_output);
    API_CHECK_THROW(ib->lnn_network, NIMCP_ERROR_MEMORY, "Failed to create LNN network");
    lnn_network_init_weights(ib->lnn_network, 42);

    lnn_training_config_t cfg;
    lnn_training_config_default(&cfg);
    cfg.learning_rate = 0.01f;
    cfg.gradient_clip_norm = 100.0f;
    cfg.enable_plasticity_integration = true;
    cfg.lnn_train_mode = LNN_TRAIN_ADJOINT;
    cfg.track_statistics = true;

    if (ib->lnn_training_ctx) {
        lnn_training_destroy(ib->lnn_training_ctx);
        ib->lnn_training_ctx = NULL;
    }
    ib->lnn_training_ctx = lnn_training_create(ib->lnn_network, &cfg);

    set_error("No error");
    return NIMCP_OK;
}

/**
 * @brief Get LNN network statistics
 *
 * @param brain Brain handle
 * @param out_forward_steps Forward step count
 * @param out_backward_steps Backward step count
 * @param out_ode_evals Total ODE evaluations
 * @param out_avg_tau Average tau across network
 * @param out_state_norm State vector norm
 * @param out_gradient_norm Gradient norm
 * @param out_nan_count NaN detection count
 * @param out_inf_count Inf detection count
 * @return NIMCP_OK, or NIMCP_ERROR_INVALID if LNN not initialized
 */
nimcp_status_t nimcp_brain_lnn_get_stats(
    nimcp_brain_t brain,
    uint64_t* out_forward_steps, uint64_t* out_backward_steps,
    uint64_t* out_ode_evals, float* out_avg_tau,
    float* out_state_norm, float* out_gradient_norm,
    uint32_t* out_nan_count, uint32_t* out_inf_count)
{
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain");
    API_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "NULL internal_brain");

    brain_t ib = brain->internal_brain;
    if (!ib->lnn_network) {
        set_error("LNN not initialized");
        return NIMCP_ERROR_INVALID;
    }

    lnn_network_stats_t stats;
    int r = lnn_get_stats(ib->lnn_network, &stats);
    if (r != 0) {
        set_error("lnn_get_stats failed");
        return NIMCP_ERROR_INVALID;
    }

    if (out_forward_steps)  *out_forward_steps  = stats.forward_steps;
    if (out_backward_steps) *out_backward_steps = stats.backward_steps;
    if (out_ode_evals)      *out_ode_evals      = stats.ode_evaluations;
    if (out_avg_tau)        *out_avg_tau         = stats.avg_tau_network;
    if (out_state_norm)     *out_state_norm      = stats.state_norm;
    if (out_gradient_norm)  *out_gradient_norm   = stats.gradient_norm;
    if (out_nan_count)      *out_nan_count       = stats.nan_count;
    if (out_inf_count)      *out_inf_count       = stats.inf_count;

    set_error("No error");
    return NIMCP_OK;
}

/**
 * @brief Get SNN network statistics
 */
nimcp_status_t nimcp_brain_snn_get_stats(
    nimcp_brain_t brain,
    uint64_t* out_total_steps, uint64_t* out_total_spikes,
    float* out_mean_firing_rate, float* out_sparsity,
    float* out_synchrony, uint32_t* out_silent_neurons,
    uint32_t* out_hyperactive_neurons, int* out_health,
    size_t* out_memory_bytes)
{
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain");
    API_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "NULL internal_brain");

    brain_t ib = brain->internal_brain;
    if (!ib->snn_network) {
        set_error("SNN not initialized");
        return NIMCP_ERROR_INVALID;
    }

    snn_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    int r = snn_network_get_stats(ib->snn_network, &stats);
    if (r != 0) {
        set_error("snn_network_get_stats failed");
        return NIMCP_ERROR_INVALID;
    }

    if (out_total_steps)        *out_total_steps        = stats.total_steps;
    if (out_total_spikes)       *out_total_spikes       = stats.total_spikes;
    if (out_mean_firing_rate)   *out_mean_firing_rate   = stats.mean_firing_rate;
    if (out_sparsity)           *out_sparsity           = stats.sparsity;
    if (out_synchrony)          *out_synchrony          = stats.synchrony;
    if (out_silent_neurons)     *out_silent_neurons     = stats.silent_neurons;
    if (out_hyperactive_neurons) *out_hyperactive_neurons = stats.hyperactive_neurons;
    if (out_health)             *out_health             = (int)stats.health;
    if (out_memory_bytes)       *out_memory_bytes       = stats.memory_usage_bytes;

    set_error("No error");
    return NIMCP_OK;
}

/**
 * @brief Get CNN trainer statistics
 */
nimcp_status_t nimcp_brain_cnn_get_stats(
    nimcp_brain_t brain,
    uint32_t* out_num_layers, size_t* out_num_parameters,
    uint32_t* out_num_labels, bool* out_active)
{
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain");
    API_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "NULL internal_brain");

    brain_t ib = brain->internal_brain;
    if (!ib->cnn_trainer) {
        set_error("CNN trainer not initialized");
        return NIMCP_ERROR_INVALID;
    }

    if (out_num_layers)     *out_num_layers     = cnn_get_layer_count(ib->cnn_trainer);
    if (out_num_parameters) *out_num_parameters = cnn_count_parameters(ib->cnn_trainer);
    if (out_num_labels)     *out_num_labels     = ib->num_output_labels;
    if (out_active)         *out_active         = true;

    set_error("No error");
    return NIMCP_OK;
}

// =========================================================================
// Group 5 — Configuration
// =========================================================================

/**
 * @brief Set fast training mode on/off
 */
nimcp_status_t nimcp_brain_set_fast_training(nimcp_brain_t brain, bool enabled)
{
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain");
    API_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "NULL internal_brain");
    brain->internal_brain->config.fast_training_mode = enabled;
    set_error("No error");
    return NIMCP_OK;
}

/**
 * @brief Set the brain's task strategy
 *
 * @param brain Brain handle
 * @param task_type "regression", "classification", "pattern", or "association"
 */
extern task_strategy_t* strategy_create(brain_task_t task);
nimcp_status_t nimcp_brain_set_task_type(nimcp_brain_t brain, const char* task_type)
{
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain");
    API_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "NULL internal_brain");
    API_CHECK_THROW(task_type, NIMCP_ERROR_NULL_ARG, "NULL task_type");

    brain_t ib = brain->internal_brain;
    brain_task_t task;

    if (strcmp(task_type, "regression") == 0)        task = BRAIN_TASK_REGRESSION;
    else if (strcmp(task_type, "classification") == 0) task = BRAIN_TASK_CLASSIFICATION;
    else if (strcmp(task_type, "pattern") == 0)      task = BRAIN_TASK_PATTERN_MATCHING;
    else if (strcmp(task_type, "association") == 0)   task = BRAIN_TASK_ASSOCIATION;
    else {
        set_error("Unknown task type: %s", task_type);
        return NIMCP_ERROR_INVALID;
    }

    task_strategy_t* new_strategy = strategy_create(task);
    API_CHECK_THROW(new_strategy, NIMCP_ERROR_MEMORY, "Failed to create strategy");

    if (ib->strategy && !ib->is_cow_clone) {
        nimcp_free(ib->strategy);
    }
    ib->strategy = new_strategy;
    ib->config.task = task;

    set_error("No error");
    return NIMCP_OK;
}

/**
 * @brief Enable/disable biological plasticity (TPB+EDP+coordinator)
 */
nimcp_status_t nimcp_brain_enable_biological_plasticity(nimcp_brain_t brain, bool enabled)
{
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain");
    API_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "NULL internal_brain");
    brain_t ib = brain->internal_brain;
    ib->enable_plasticity_bridge = enabled;
    ib->enable_event_driven_plasticity = enabled;
    ib->plasticity_coordinator_enabled = enabled;
    set_error("No error");
    return NIMCP_OK;
}

/**
 * @brief Enable multi-network ensemble training (LNN + CNN + Adaptive)
 */
nimcp_status_t nimcp_brain_enable_multi_network(nimcp_brain_t brain)
{
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain");
    API_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "NULL internal_brain");
    int rc = brain_enable_multi_network_training(brain->internal_brain);
    if (rc < 0) {
        set_error("Failed to enable multi-network training");
        return NIMCP_ERROR_INVALID;
    }
    set_error("No error");
    return NIMCP_OK;
}

// =========================================================================
// Group 6 — Brain State
// =========================================================================

/**
 * @brief Get medulla arousal level
 */
float nimcp_brain_medulla_get_arousal(nimcp_brain_t brain)
{
    if (!brain || !brain->internal_brain) return 0.0f;
    return brain_ti_get_arousal(brain->internal_brain);
}

/**
 * @brief Get sleep pressure
 */
float nimcp_brain_sleep_get_pressure(nimcp_brain_t brain)
{
    if (!brain || !brain->internal_brain) return 0.0f;
    sleep_system_t ss = brain_get_sleep_system(brain->internal_brain);
    if (!ss) return 0.0f;
    return sleep_get_pressure(ss);
}

/**
 * @brief Get basal ganglia dopamine level
 */
float nimcp_brain_bg_get_dopamine(nimcp_brain_t brain)
{
    if (!brain || !brain->internal_brain) return 0.0f;
    return brain_ti_get_dopamine(brain->internal_brain);
}

/**
 * @brief Get substrate health status string
 *
 * @param brain Brain handle
 * @param out_status Buffer for status string
 * @param max_len Max buffer length
 */
nimcp_status_t nimcp_brain_substrate_get_health(
    nimcp_brain_t brain,
    char* out_status, uint32_t max_len)
{
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain");
    API_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "NULL internal_brain");
    API_CHECK_THROW(out_status, NIMCP_ERROR_NULL_ARG, "NULL out_status");

    brain_t ib = brain->internal_brain;
    const char* status = ib->substrate_gpu_ctx ? "OPTIMAL" : "UNKNOWN";
    snprintf(out_status, max_len, "%s", status);
    set_error("No error");
    return NIMCP_OK;
}

/**
 * @brief Focus attention on a modality (acknowledged, actual gating is
 *        automatic via thalamic bridges during decide_full)
 */
nimcp_status_t nimcp_brain_focus_attention(
    nimcp_brain_t brain,
    const char* modality)
{
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain");
    API_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "NULL internal_brain");
    API_CHECK_THROW(modality, NIMCP_ERROR_NULL_ARG, "NULL modality");
    /* Attention is managed automatically by thalamic bridges.
     * This call is acknowledged — the next decide_full() will process
     * the requested modality with standard thalamic gating. */
    (void)modality;
    set_error("No error");
    return NIMCP_OK;
}

// =========================================================================
// Group 7 — Memory Store / OOD / Audit Bridge (for C#, Rust, Perl, C++)
// =========================================================================

#include "memory/nimcp_memory_store.h"
#include "cognitive/nimcp_ood_detector.h"

/**
 * @brief Get memory store statistics through brain handle.
 */
int nimcp_brain_memory_store_stats(
    nimcp_brain_t brain, nimcp_memory_store_stats_t* stats)
{
    if (!brain || !brain->internal_brain || !brain->internal_brain->memory_store || !stats) return -1;
    return nimcp_memory_store_get_stats(
        (nimcp_memory_store_t*)brain->internal_brain->memory_store, stats);
}

/**
 * @brief Full-text search on engram labels through brain handle.
 * @param out_ids Pre-allocated array for result IDs
 * @param max_results Max entries to return
 * @param out_count Actual count returned
 */
int nimcp_brain_memory_search_text(
    nimcp_brain_t brain, const char* query, uint32_t max_results,
    uint64_t* out_ids, uint32_t* out_count)
{
    if (!brain || !brain->internal_brain || !brain->internal_brain->memory_store) {
        if (out_count) *out_count = 0;
        return 0;
    }
    nimcp_memory_search_result_t* res = nimcp_memory_store_engram_search_text(
        (nimcp_memory_store_t*)brain->internal_brain->memory_store, query, max_results);
    uint32_t count = 0;
    if (res) {
        count = res->count < max_results ? res->count : max_results;
        for (uint32_t i = 0; i < count; i++) {
            if (out_ids) out_ids[i] = res->ids[i];
        }
        nimcp_memory_search_result_destroy(res);
    }
    if (out_count) *out_count = count;
    return 0;
}

/**
 * @brief Vector similarity search on engram embeddings through brain handle.
 * @param out_ids Pre-allocated array for result IDs
 * @param out_distances Pre-allocated array for distances
 * @param out_count Actual count returned
 */
int nimcp_brain_memory_search_similar(
    nimcp_brain_t brain, const float* embedding, uint32_t dim,
    uint32_t top_k, uint64_t* out_ids, float* out_distances, uint32_t* out_count)
{
    if (!brain || !brain->internal_brain || !brain->internal_brain->memory_store) {
        if (out_count) *out_count = 0;
        return 0;
    }
    nimcp_memory_search_result_t* res = nimcp_memory_store_engram_search_similar(
        (nimcp_memory_store_t*)brain->internal_brain->memory_store,
        embedding, dim, top_k, 0.0f);
    uint32_t count = 0;
    if (res) {
        count = res->count < top_k ? res->count : top_k;
        for (uint32_t i = 0; i < count; i++) {
            if (out_ids) out_ids[i] = res->ids[i];
            if (out_distances) out_distances[i] = res->distances[i];
        }
        nimcp_memory_search_result_destroy(res);
    }
    if (out_count) *out_count = count;
    return 0;
}

/**
 * @brief Check if memory store is healthy through brain handle.
 */
bool nimcp_brain_memory_is_healthy(nimcp_brain_t brain)
{
    if (!brain || !brain->internal_brain || !brain->internal_brain->memory_store)
        return true; /* No store = nothing unhealthy */
    return nimcp_memory_store_is_healthy(
        (nimcp_memory_store_t*)brain->internal_brain->memory_store);
}

/**
 * @brief Get OOD detector statistics through brain handle.
 */
int nimcp_brain_ood_stats(nimcp_brain_t brain, nimcp_ood_stats_t* stats)
{
    if (!brain || !brain->internal_brain || !brain->internal_brain->ood_detector || !stats)
        return -1;
    return nimcp_ood_get_stats(
        (const nimcp_ood_detector_t*)brain->internal_brain->ood_detector, stats);
}

/**
 * @brief Log an audit event through brain handle.
 */
int nimcp_brain_audit_log(
    nimcp_brain_t brain, const char* description,
    uint32_t severity, const char* details)
{
    if (!brain || !brain->internal_brain || !brain->internal_brain->memory_store)
        return -1;
    nimcp_memory_audit_event_t event = {0};
    extern uint64_t nimcp_time_get_us(void);
    event.timestamp_us = nimcp_time_get_us();
    event.event_type = severity;
    if (description) strncpy(event.description, description, sizeof(event.description) - 1);
    if (details) strncpy(event.details, details, sizeof(event.details) - 1);
    return nimcp_memory_store_audit_log(
        (nimcp_memory_store_t*)brain->internal_brain->memory_store, &event);
}

/**
 * @brief Search audit trail through brain handle.
 * @param out_ids Pre-allocated array for result IDs
 * @param out_severities Pre-allocated array for severity values
 * @param out_count Actual count returned
 */
int nimcp_brain_audit_search(
    nimcp_brain_t brain, uint32_t min_severity, uint32_t max_results,
    uint64_t* out_ids, float* out_severities, uint32_t* out_count)
{
    if (!brain || !brain->internal_brain || !brain->internal_brain->memory_store) {
        if (out_count) *out_count = 0;
        return 0;
    }
    nimcp_memory_search_result_t* res = nimcp_memory_store_audit_search(
        (nimcp_memory_store_t*)brain->internal_brain->memory_store,
        min_severity, 0, UINT64_MAX, max_results);
    uint32_t count = 0;
    if (res) {
        count = res->count < max_results ? res->count : max_results;
        for (uint32_t i = 0; i < count; i++) {
            if (out_ids) out_ids[i] = res->ids[i];
            if (out_severities) out_severities[i] = res->distances[i];
        }
        nimcp_memory_search_result_destroy(res);
    }
    if (out_count) *out_count = count;
    return 0;
}
