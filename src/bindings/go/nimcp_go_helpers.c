// nimcp_go_helpers.c — C helper functions for Go CGo bindings
// Wraps internal brain access for functions not in the public nimcp.h API.

#include "nimcp_go_helpers.h"
#include "api/nimcp_api_internal.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/learning/nimcp_brain_learning.h"
#include "core/brain/strategy/nimcp_brain_strategy.h"
#include "cognitive/training/nimcp_training_integration.h"
#include "cognitive/nimcp_sleep_wake.h"
#include "lnn/nimcp_lnn.h"
#include "lnn/nimcp_lnn_network.h"
#include "snn/nimcp_snn_network.h"
#include "training/nimcp_cnn_training.h"
#include "training/nimcp_cortex_cnn.h"
#include "perception/nimcp_visual_cortex.h"
#include "utils/memory/nimcp_memory.h"

#include <string.h>
#include <stdlib.h>

// ============================================================================
// Brain State Accessors
// ============================================================================

float go_brain_medulla_get_arousal(nimcp_brain_t brain) {
    if (!brain || !brain->internal_brain) return 0.5f;
    return brain_ti_get_arousal(brain->internal_brain);
}

float go_brain_sleep_get_pressure(nimcp_brain_t brain) {
    if (!brain || !brain->internal_brain) return 0.0f;
    sleep_system_t ss = brain_get_sleep_system(brain->internal_brain);
    if (!ss) return 0.0f;
    return sleep_get_pressure(ss);
}

float go_brain_bg_get_dopamine(nimcp_brain_t brain) {
    if (!brain || !brain->internal_brain) return 0.5f;
    return brain_ti_get_dopamine(brain->internal_brain);
}

const char* go_brain_substrate_get_health(nimcp_brain_t brain) {
    if (!brain || !brain->internal_brain) return "UNKNOWN";
    brain_t ib = brain->internal_brain;
    if (!ib->substrate_gpu_ctx) return "UNKNOWN";
    return "OPTIMAL";
}

// ============================================================================
// Configuration
// ============================================================================

bool go_brain_set_fast_training(nimcp_brain_t brain, bool enabled) {
    if (!brain || !brain->internal_brain) return false;
    brain->internal_brain->config.fast_training_mode = enabled;
    return true;
}

bool go_brain_set_task_type(nimcp_brain_t brain, const char* task_str) {
    if (!brain || !brain->internal_brain || !task_str) return false;
    brain_t ib = brain->internal_brain;

    brain_task_t task;
    if (strcmp(task_str, "regression") == 0)       task = BRAIN_TASK_REGRESSION;
    else if (strcmp(task_str, "classification") == 0) task = BRAIN_TASK_CLASSIFICATION;
    else if (strcmp(task_str, "pattern") == 0)     task = BRAIN_TASK_PATTERN_MATCHING;
    else if (strcmp(task_str, "association") == 0) task = BRAIN_TASK_ASSOCIATION;
    else return false;

    task_strategy_t* new_strategy = strategy_create(task);
    if (!new_strategy) return false;

    if (ib->strategy && !ib->is_cow_clone) {
        nimcp_free(ib->strategy);
    }
    ib->strategy = new_strategy;
    ib->config.task = task;
    return true;
}

bool go_brain_enable_biological_plasticity(nimcp_brain_t brain, bool enabled) {
    if (!brain || !brain->internal_brain) return false;
    brain_t ib = brain->internal_brain;
    ib->enable_plasticity_bridge = enabled;
    ib->enable_event_driven_plasticity = enabled;
    ib->plasticity_coordinator_enabled = enabled;
    return true;
}

int go_brain_enable_multi_network(nimcp_brain_t brain) {
    if (!brain || !brain->internal_brain) return -1;
    return brain_enable_multi_network_training(brain->internal_brain);
}

// ============================================================================
// LNN
// ============================================================================

bool go_brain_lnn_create(nimcp_brain_t brain,
                         uint32_t n_sensory, uint32_t n_inter,
                         uint32_t n_command, uint32_t n_output) {
    if (!brain || !brain->internal_brain) return false;
    brain_t ib = brain->internal_brain;

    if (ib->lnn_network) return true; // Already created (idempotent)

    if (!lnn_is_initialized()) {
        lnn_init(1);
    }

    ib->lnn_network = lnn_network_create_ncp(n_sensory, n_inter, n_command, n_output);
    if (!ib->lnn_network) return false;

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
    return true;
}

bool go_brain_lnn_get_stats(nimcp_brain_t brain, go_lnn_stats_t* out) {
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!brain || !brain->internal_brain) return false;
    brain_t ib = brain->internal_brain;
    if (!ib->lnn_network) return false;

    lnn_network_stats_t stats;
    int r = lnn_get_stats(ib->lnn_network, &stats);
    if (r != 0) return false;

    out->forward_steps = stats.forward_steps;
    out->backward_steps = stats.backward_steps;
    out->ode_evaluations = stats.ode_evaluations;
    out->avg_tau = stats.avg_tau_network;
    out->state_norm = stats.state_norm;
    out->gradient_norm = stats.gradient_norm;
    out->nan_count = stats.nan_count;
    out->inf_count = stats.inf_count;
    out->valid = true;
    return true;
}

// ============================================================================
// SNN
// ============================================================================

bool go_brain_snn_get_stats(nimcp_brain_t brain, go_snn_stats_t* out) {
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!brain || !brain->internal_brain) return false;
    brain_t ib = brain->internal_brain;
    if (!ib->snn_network) return false;

    snn_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    int r = snn_network_get_stats(ib->snn_network, &stats);
    if (r != 0) return false;

    out->total_steps = stats.total_steps;
    out->total_spikes = stats.total_spikes;
    out->mean_firing_rate = stats.mean_firing_rate;
    out->max_firing_rate = stats.max_firing_rate;
    out->sparsity = stats.sparsity;
    out->synchrony = stats.synchrony;
    out->spikes_per_sample = stats.spikes_per_sample;
    out->silent_neurons = stats.silent_neurons;
    out->hyperactive_neurons = stats.hyperactive_neurons;
    out->health = (int)stats.health;
    out->memory_usage_bytes = (uint64_t)stats.memory_usage_bytes;
    out->valid = true;
    return true;
}

// ============================================================================
// CNN
// ============================================================================

bool go_brain_cnn_get_stats(nimcp_brain_t brain, go_cnn_stats_t* out) {
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!brain || !brain->internal_brain) return false;
    brain_t ib = brain->internal_brain;
    if (!ib->cnn_trainer) return false;

    out->num_layers = cnn_get_layer_count(ib->cnn_trainer);
    out->num_parameters = (uint64_t)cnn_count_parameters(ib->cnn_trainer);
    out->num_labels = ib->num_output_labels;
    out->active = true;
    out->valid = true;
    return true;
}

// ============================================================================
// Sensory
// ============================================================================

nimcp_status_t go_brain_submit_sensory(nimcp_brain_t brain,
                                        const char* modality,
                                        const float* data, uint32_t data_len,
                                        uint32_t width, uint32_t height,
                                        uint32_t channels, uint32_t n_segments) {
    if (!brain || !brain->internal_brain || !modality || !data || data_len == 0)
        return 1000; // NIMCP_ERROR_GENERIC

    brain_t ib = brain->internal_brain;

    if (strcmp(modality, "somatosensory") == 0 || strcmp(modality, "somato") == 0) {
        if (ib->staged_sensory.somato_data) nimcp_free(ib->staged_sensory.somato_data);
        float* copy = (float*)nimcp_malloc(data_len * sizeof(float));
        if (!copy) return 2000; // NIMCP_ERROR_MEMORY
        memcpy(copy, data, data_len * sizeof(float));
        ib->staged_sensory.somato_data = copy;
        ib->staged_sensory.somato_segments = (n_segments > 0) ? n_segments : data_len;
    } else if (strcmp(modality, "visual") == 0) {
        if (ib->staged_sensory.visual_frame) nimcp_free(ib->staged_sensory.visual_frame);
        uint8_t* pixels = (uint8_t*)nimcp_malloc((size_t)data_len);
        if (!pixels) return 2000;
        for (uint32_t i = 0; i < data_len; i++) {
            float v = data[i];
            if (v <= 1.0f && v >= 0.0f) v *= 255.0f;
            pixels[i] = (uint8_t)(v > 255.0f ? 255 : (v < 0.0f ? 0 : (uint8_t)v));
        }
        ib->staged_sensory.visual_frame = pixels;
        ib->staged_sensory.visual_width = (width > 0) ? width : 32;
        ib->staged_sensory.visual_height = (height > 0) ? height : 32;
        ib->staged_sensory.visual_channels = (channels > 0) ? channels : 3;
    } else if (strcmp(modality, "audio") == 0) {
        if (ib->staged_sensory.audio_data) nimcp_free(ib->staged_sensory.audio_data);
        float* copy = (float*)nimcp_malloc(data_len * sizeof(float));
        if (!copy) return 2000;
        memcpy(copy, data, data_len * sizeof(float));
        ib->staged_sensory.audio_data = copy;
        ib->staged_sensory.audio_size = data_len;
    } else if (strcmp(modality, "speech") == 0) {
        if (ib->staged_sensory.speech_data) nimcp_free(ib->staged_sensory.speech_data);
        float* copy = (float*)nimcp_malloc(data_len * sizeof(float));
        if (!copy) return 2000;
        memcpy(copy, data, data_len * sizeof(float));
        ib->staged_sensory.speech_data = copy;
        ib->staged_sensory.speech_size = data_len;
    } else {
        return 1004; // NIMCP_ERROR_INVALID
    }

    return 0; // NIMCP_OK
}

uint32_t go_brain_visual_cortex_process(nimcp_brain_t brain,
                                         const float* pixels_float, uint32_t n_pixels,
                                         uint32_t width, uint32_t height,
                                         uint32_t channels,
                                         float* features_out, uint32_t max_features) {
    if (!brain || !brain->internal_brain || !pixels_float || !features_out)
        return 0;

    brain_t ib = brain->internal_brain;
    if (!ib->visual_cortex) return 0;

    uint32_t feat_dim = visual_cortex_get_feature_dim(ib->visual_cortex);
    if (feat_dim == 0) feat_dim = 128;
    if (feat_dim > max_features) feat_dim = max_features;

    // Convert float [0,1] to uint8 [0,255]
    uint8_t* pixels = (uint8_t*)nimcp_malloc((size_t)n_pixels);
    if (!pixels) return 0;
    for (uint32_t i = 0; i < n_pixels; i++) {
        float v = pixels_float[i];
        if (v <= 1.0f && v >= 0.0f) v *= 255.0f;
        pixels[i] = (uint8_t)(v > 255.0f ? 255 : (v < 0.0f ? 0 : (uint8_t)v));
    }

    memset(features_out, 0, feat_dim * sizeof(float));
    bool success = visual_cortex_process(ib->visual_cortex, pixels,
                                          width, height, channels, features_out);
    nimcp_free(pixels);

    return success ? feat_dim : 0;
}

// ============================================================================
// Cortex CNN Metrics
// ============================================================================

bool go_brain_get_cortex_cnn_metrics(nimcp_brain_t brain, int ci,
                                      go_cortex_cnn_metrics_t* out) {
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!brain || !brain->internal_brain) return false;
    if (ci < 0 || ci >= 4) return false;

    brain_t ib = brain->internal_brain;
    if (!ib->cortex_cnns[ci]) return false;

    cortex_cnn_metrics_t m;
    memset(&m, 0, sizeof(m));
    if (cortex_cnn_get_metrics(ib->cortex_cnns[ci], &m) != 0) return false;

    out->last_loss = m.last_loss;
    out->ema_loss = m.ema_loss;
    out->forward_steps = m.forward_steps;
    out->backward_steps = m.backward_steps;
    out->embedding_norm = m.embedding_norm;
    out->confidence = m.confidence;
    out->embedding_dim = m.embedding_dim;
    out->num_params = m.num_params;
    out->valid = true;
    return true;
}

// ============================================================================
// Focus Attention
// ============================================================================

bool go_brain_focus_attention(nimcp_brain_t brain, const char* modality) {
    // Attention gating is managed via thalamic bridges during decide_full.
    // This is a hint to the attention system about which modality to prioritize.
    if (!brain || !brain->internal_brain || !modality) return false;
    // Thalamus handles attention gating automatically during decide_full().
    // No direct attention API exists — this is a no-op hint for now.
    (void)modality;
    return true;
}

// ============================================================================
// Memory Store Helpers
// ============================================================================

#include "memory/nimcp_memory_store.h"
#include "cognitive/nimcp_ood_detector.h"
#include "utils/time/nimcp_time.h"

bool go_brain_memory_store_stats(nimcp_brain_t brain, go_memory_store_stats_t* out) {
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!brain || !brain->internal_brain || !brain->internal_brain->memory_store)
        return false;
    nimcp_memory_store_stats_t stats = {0};
    nimcp_memory_store_get_stats(
        (nimcp_memory_store_t*)brain->internal_brain->memory_store, &stats);
    out->total_engrams = stats.total_engrams;
    out->total_concepts = stats.total_concepts;
    out->total_relations = stats.total_relations;
    out->total_autobio = stats.total_autobio;
    out->total_writes = stats.total_writes;
    out->total_reads = stats.total_reads;
    out->cache_hits = stats.cache_hits;
    out->cache_misses = stats.cache_misses;
    out->db_size_bytes = stats.db_size_bytes;
    out->valid = true;
    return true;
}

bool go_brain_memory_is_healthy(nimcp_brain_t brain) {
    if (!brain || !brain->internal_brain || !brain->internal_brain->memory_store)
        return true; /* No store = nothing unhealthy */
    return nimcp_memory_store_is_healthy(
        (nimcp_memory_store_t*)brain->internal_brain->memory_store);
}

go_id_array_t go_brain_memory_search_text(nimcp_brain_t brain, const char* query,
                                           uint32_t max_results) {
    go_id_array_t result = {0};
    if (!brain || !brain->internal_brain || !brain->internal_brain->memory_store || !query)
        return result;
    nimcp_memory_search_result_t* res = nimcp_memory_store_engram_search_text(
        (nimcp_memory_store_t*)brain->internal_brain->memory_store, query, max_results);
    if (res && res->count > 0) {
        result.ids = (uint64_t*)malloc(res->count * sizeof(uint64_t));
        if (result.ids) {
            memcpy(result.ids, res->ids, res->count * sizeof(uint64_t));
            result.count = res->count;
        }
    }
    if (res) nimcp_memory_search_result_destroy(res);
    return result;
}

go_similarity_result_t go_brain_memory_search_similar(
    nimcp_brain_t brain, const float* embedding, uint32_t dim, uint32_t top_k) {
    go_similarity_result_t result = {0};
    if (!brain || !brain->internal_brain || !brain->internal_brain->memory_store || !embedding)
        return result;
    nimcp_memory_search_result_t* res = nimcp_memory_store_engram_search_similar(
        (nimcp_memory_store_t*)brain->internal_brain->memory_store, embedding, dim, top_k, 0.0f);
    if (res && res->count > 0) {
        result.ids = (uint64_t*)malloc(res->count * sizeof(uint64_t));
        result.distances = (float*)malloc(res->count * sizeof(float));
        if (result.ids && result.distances) {
            memcpy(result.ids, res->ids, res->count * sizeof(uint64_t));
            memcpy(result.distances, res->distances, res->count * sizeof(float));
            result.count = res->count;
        } else {
            free(result.ids); free(result.distances);
            result.ids = NULL; result.distances = NULL; result.count = 0;
        }
    }
    if (res) nimcp_memory_search_result_destroy(res);
    return result;
}

// ============================================================================
// OOD Helpers
// ============================================================================

bool go_brain_ood_stats(nimcp_brain_t brain, go_ood_stats_t* out) {
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!brain || !brain->internal_brain || !brain->internal_brain->ood_detector)
        return false;
    nimcp_ood_stats_t stats = {0};
    nimcp_ood_get_stats(
        (const nimcp_ood_detector_t*)brain->internal_brain->ood_detector, &stats);
    out->total_checks = stats.total_checks;
    out->ood_detected = stats.ood_detected;
    out->in_distribution = stats.in_distribution;
    out->avg_ood_score = stats.avg_ood_score;
    out->ood_rate = stats.ood_rate;
    out->valid = true;
    return true;
}

// ============================================================================
// Audit Helpers
// ============================================================================

int go_brain_audit_log(nimcp_brain_t brain, const char* description,
                       uint32_t severity, const char* details) {
    if (!brain || !brain->internal_brain || !brain->internal_brain->memory_store)
        return -1;
    nimcp_memory_audit_event_t event = {0};
    event.timestamp_us = nimcp_time_get_us();
    event.event_type = severity;
    if (description) strncpy(event.description, description, sizeof(event.description) - 1);
    if (details) strncpy(event.details, details, sizeof(event.details) - 1);
    return nimcp_memory_store_audit_log(
        (nimcp_memory_store_t*)brain->internal_brain->memory_store, &event);
}

go_similarity_result_t go_brain_audit_search(nimcp_brain_t brain,
                                              uint32_t min_severity, uint32_t max_results) {
    go_similarity_result_t result = {0};
    if (!brain || !brain->internal_brain || !brain->internal_brain->memory_store)
        return result;
    nimcp_memory_search_result_t* res = nimcp_memory_store_audit_search(
        (nimcp_memory_store_t*)brain->internal_brain->memory_store,
        min_severity, 0, UINT64_MAX, max_results);
    if (res && res->count > 0) {
        result.ids = (uint64_t*)malloc(res->count * sizeof(uint64_t));
        result.distances = (float*)malloc(res->count * sizeof(float));
        if (result.ids && result.distances) {
            memcpy(result.ids, res->ids, res->count * sizeof(uint64_t));
            memcpy(result.distances, res->distances, res->count * sizeof(float));
            result.count = res->count;
        } else {
            free(result.ids); free(result.distances);
            result.ids = NULL; result.distances = NULL; result.count = 0;
        }
    }
    if (res) nimcp_memory_search_result_destroy(res);
    return result;
}

void go_free_id_array(go_id_array_t* arr) {
    if (arr && arr->ids) { free(arr->ids); arr->ids = NULL; arr->count = 0; }
}

void go_free_similarity_result(go_similarity_result_t* res) {
    if (res) {
        free(res->ids); free(res->distances);
        res->ids = NULL; res->distances = NULL; res->count = 0;
    }
}
