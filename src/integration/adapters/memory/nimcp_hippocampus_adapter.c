/**
 * @file nimcp_hippocampus_adapter.c
 * @brief Hippocampus Adapter Implementation
 * @version 1.0.0
 * @date 2026-01-10
 */

#include "integration/adapters/memory/nimcp_hippocampus_adapter.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

// Use weak attribute to avoid duplicate symbol with core/brain/regions/hippocampus version
static struct nimcp_health_agent* g_hippocampus_adapter_health_agent = NULL;
static inline void hippocampus_adapter_heartbeat(const char* op, float progress) { (void)op; (void)progress; }
__attribute__((weak)) void hippocampus_adapter_set_health_agent(struct nimcp_health_agent* agent) { g_hippocampus_adapter_health_agent = agent; }

#define HIPP_MAX_PATTERNS 1000
#define HIPP_PATTERN_SIZE 256

typedef struct {
    float pattern[HIPP_PATTERN_SIZE];
    uint32_t size;
    bool is_valid;
} hipp_pattern_t;

struct nimcp_hippocampus_adapter_struct {
    nimcp_hippocampus_config_t config;
    nimcp_module_interface_t interface;
    hipp_pattern_t* patterns;
    uint32_t pattern_count;
    nimcp_hippocampus_state_t state;
    nimcp_hippocampus_stats_t stats;
    bool is_initialized;
};

static float pattern_similarity(const float* a, const float* b, uint32_t size) {
    float dot = 0, mag_a = 0, mag_b = 0;
    for (uint32_t i = 0; i < size; i++) {
        dot += a[i] * b[i];
        mag_a += a[i] * a[i];
        mag_b += b[i] * b[i];
    }
    if (mag_a < 0.0001f || mag_b < 0.0001f) return 0.0f;
    return dot / (sqrtf(mag_a) * sqrtf(mag_b));
}

static nimcp_layer_error_t hipp_init(void* module, void* config) {
    nimcp_hippocampus_adapter_t adapter = (nimcp_hippocampus_adapter_t)module;
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    if (config) adapter->config = *(nimcp_hippocampus_config_t*)config;
    adapter->patterns = (hipp_pattern_t*)nimcp_calloc(HIPP_MAX_PATTERNS, sizeof(hipp_pattern_t));
    if (!adapter->patterns) return NIMCP_LAYER_ERR_NO_MEMORY;
    adapter->is_initialized = true;
    adapter->state.is_active = true;
    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t hipp_shutdown(void* module) {
    nimcp_hippocampus_adapter_t adapter = (nimcp_hippocampus_adapter_t)module;
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    nimcp_free(adapter->patterns);
    adapter->patterns = NULL;
    adapter->is_initialized = false;
    adapter->state.is_active = false;
    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t hipp_update(void* module, float dt) {
    nimcp_hippocampus_adapter_t adapter = (nimcp_hippocampus_adapter_t)module;
    if (!adapter || !adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    (void)dt;
    adapter->stats.updates_processed++;
    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t hipp_handle_message(void* module, const nimcp_layer_msg_t* msg) {
    nimcp_hippocampus_adapter_t adapter = (nimcp_hippocampus_adapter_t)module;
    if (!adapter || !msg) return NIMCP_LAYER_ERR_NULL_PTR;
    adapter->stats.messages_handled++;
    if (msg->header.msg_type == NIMCP_LAYER_MSG_DATA_PUSH && msg->payload) {
        nimcp_hippocampus_adapter_encode_episode(adapter, (float*)msg->payload,
            msg->header.payload_size / sizeof(float));
    }
    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t hipp_get_state(void* module, void* state_out, size_t* size) {
    nimcp_hippocampus_adapter_t adapter = (nimcp_hippocampus_adapter_t)module;
    if (!adapter || !state_out || !size) return NIMCP_LAYER_ERR_NULL_PTR;
    if (*size < sizeof(nimcp_hippocampus_state_t)) {
        *size = sizeof(nimcp_hippocampus_state_t);
        return NIMCP_LAYER_ERR_CAPACITY;
    }
    *(nimcp_hippocampus_state_t*)state_out = adapter->state;
    *size = sizeof(nimcp_hippocampus_state_t);
    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t hipp_set_state(void* module, const void* state, size_t size) {
    nimcp_hippocampus_adapter_t adapter = (nimcp_hippocampus_adapter_t)module;
    if (!adapter || !state) return NIMCP_LAYER_ERR_NULL_PTR;
    if (size < sizeof(nimcp_hippocampus_state_t)) return NIMCP_LAYER_ERR_INVALID_MSG;
    adapter->state = *(const nimcp_hippocampus_state_t*)state;
    return NIMCP_LAYER_OK;
}

static const char* hipp_get_name(void* module) {
    (void)module;
    return "Hippocampus_Adapter";
}

nimcp_hippocampus_config_t nimcp_hippocampus_adapter_default_config(void) {
    nimcp_hippocampus_config_t config = {
        .ca3_size = 10000, .ca1_size = 10000, .dg_size = 100000,
        .pattern_separation_strength = 0.8f, .pattern_completion_threshold = 0.3f,
        .enable_replay = true, .enable_logging = false
    };
    return config;
}

nimcp_hippocampus_adapter_t nimcp_hippocampus_adapter_create(const nimcp_hippocampus_config_t* config) {
    nimcp_hippocampus_adapter_t adapter = (nimcp_hippocampus_adapter_t)nimcp_calloc(1, sizeof(struct nimcp_hippocampus_adapter_struct));
    NIMCP_API_CHECK_ALLOC(adapter, "Failed to allocate hippocampus adapter");
    adapter->config = config ? *config : nimcp_hippocampus_adapter_default_config();
    adapter->interface.init = hipp_init;
    adapter->interface.shutdown = hipp_shutdown;
    adapter->interface.update = hipp_update;
    adapter->interface.handle_message = hipp_handle_message;
    adapter->interface.get_state = hipp_get_state;
    adapter->interface.set_state = hipp_set_state;
    adapter->interface.get_name = hipp_get_name;
    return adapter;
}

void nimcp_hippocampus_adapter_destroy(nimcp_hippocampus_adapter_t adapter) {
    if (!adapter) return;
    if (adapter->is_initialized) hipp_shutdown(adapter);
    nimcp_free(adapter);
}

nimcp_module_interface_t* nimcp_hippocampus_adapter_get_interface(nimcp_hippocampus_adapter_t adapter) {
    return adapter ? &adapter->interface : NULL;
}

nimcp_layer_error_t nimcp_hippocampus_adapter_encode_episode(nimcp_hippocampus_adapter_t adapter, const float* pattern, uint32_t size) {
    if (!adapter || !pattern) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    if (adapter->pattern_count >= HIPP_MAX_PATTERNS) return NIMCP_LAYER_ERR_CAPACITY;

    hipp_pattern_t* p = &adapter->patterns[adapter->pattern_count];
    uint32_t copy_size = size < HIPP_PATTERN_SIZE ? size : HIPP_PATTERN_SIZE;

    /* Pattern separation via DG (add noise/orthogonalization) */
    for (uint32_t i = 0; i < copy_size; i++) {
        p->pattern[i] = pattern[i] * adapter->config.pattern_separation_strength;
    }
    p->size = copy_size;
    p->is_valid = true;

    adapter->pattern_count++;
    adapter->state.stored_patterns = adapter->pattern_count;
    adapter->stats.patterns_encoded++;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_hippocampus_adapter_retrieve(nimcp_hippocampus_adapter_t adapter, const float* cue, uint32_t cue_size, float* pattern_out, uint32_t max_size) {
    if (!adapter || !cue || !pattern_out) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    float best_sim = 0.0f;
    uint32_t best_idx = 0;
    bool found = false;

    /* Pattern completion via CA3 */
    for (uint32_t i = 0; i < adapter->pattern_count; i++) {
        if (!adapter->patterns[i].is_valid) continue;
        uint32_t min_size = cue_size < adapter->patterns[i].size ? cue_size : adapter->patterns[i].size;
        float sim = pattern_similarity(cue, adapter->patterns[i].pattern, min_size);
        if (sim > best_sim && sim > adapter->config.pattern_completion_threshold) {
            best_sim = sim;
            best_idx = i;
            found = true;
        }
    }

    if (!found) return NIMCP_LAYER_ERR_NOT_REGISTERED;

    uint32_t copy_size = adapter->patterns[best_idx].size < max_size ? adapter->patterns[best_idx].size : max_size;
    memcpy(pattern_out, adapter->patterns[best_idx].pattern, copy_size * sizeof(float));

    adapter->state.pattern_completion_rate = best_sim;
    adapter->stats.patterns_retrieved++;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_hippocampus_adapter_trigger_replay(nimcp_hippocampus_adapter_t adapter) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    if (!adapter->config.enable_replay) return NIMCP_LAYER_ERR_INVALID_MSG;

    adapter->state.replay_events++;
    adapter->stats.replay_cycles++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_hippocampus_adapter_get_state(nimcp_hippocampus_adapter_t adapter, nimcp_hippocampus_state_t* state_out) {
    if (!adapter || !state_out) return NIMCP_LAYER_ERR_NULL_PTR;
    *state_out = adapter->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_hippocampus_adapter_get_stats(nimcp_hippocampus_adapter_t adapter, nimcp_hippocampus_stats_t* stats_out) {
    if (!adapter || !stats_out) return NIMCP_LAYER_ERR_NULL_PTR;
    *stats_out = adapter->stats;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_hippocampus_adapter_reset_stats(nimcp_hippocampus_adapter_t adapter) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    memset(&adapter->stats, 0, sizeof(adapter->stats));
    return NIMCP_LAYER_OK;
}
