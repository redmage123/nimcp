/**
 * @file nimcp_auditory_adapter.c
 * @brief Auditory Processing Adapter Implementation
 * @version 1.0.0
 * @date 2026-01-10
 */

#include "integration/adapters/sensory/nimcp_auditory_adapter.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(auditory_adapter)

struct nimcp_auditory_adapter_struct {
    nimcp_auditory_adapter_config_t config;
    nimcp_module_interface_t interface;
    float* spectrum_buffer;
    nimcp_auditory_adapter_state_t state;
    nimcp_auditory_adapter_stats_t stats;
    bool is_initialized;
};

static nimcp_layer_error_t auditory_init(void* module, void* config) {
    nimcp_auditory_adapter_t adapter = (nimcp_auditory_adapter_t)module;
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    if (config) adapter->config = *(nimcp_auditory_adapter_config_t*)config;
    adapter->spectrum_buffer = (float*)nimcp_calloc(adapter->config.num_frequency_bands, sizeof(float));
    if (!adapter->spectrum_buffer) return NIMCP_LAYER_ERR_NO_MEMORY;
    adapter->is_initialized = true;
    adapter->state.is_active = true;
    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t auditory_shutdown(void* module) {
    nimcp_auditory_adapter_t adapter = (nimcp_auditory_adapter_t)module;
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    nimcp_free(adapter->spectrum_buffer);
    adapter->spectrum_buffer = NULL;
    adapter->is_initialized = false;
    adapter->state.is_active = false;
    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t auditory_update(void* module, float dt) {
    nimcp_auditory_adapter_t adapter = (nimcp_auditory_adapter_t)module;
    if (!adapter || !adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    (void)dt;
    adapter->stats.updates_processed++;
    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t auditory_handle_message(void* module, const nimcp_layer_msg_t* msg) {
    nimcp_auditory_adapter_t adapter = (nimcp_auditory_adapter_t)module;
    if (!adapter || !msg) return NIMCP_LAYER_ERR_NULL_PTR;
    adapter->stats.messages_handled++;
    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t auditory_get_state(void* module, void* state_out, size_t* size) {
    nimcp_auditory_adapter_t adapter = (nimcp_auditory_adapter_t)module;
    if (!adapter || !state_out || !size) return NIMCP_LAYER_ERR_NULL_PTR;
    if (*size < sizeof(nimcp_auditory_adapter_state_t)) {
        *size = sizeof(nimcp_auditory_adapter_state_t);
        return NIMCP_LAYER_ERR_CAPACITY;
    }
    *(nimcp_auditory_adapter_state_t*)state_out = adapter->state;
    *size = sizeof(nimcp_auditory_adapter_state_t);
    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t auditory_set_state(void* module, const void* state, size_t size) {
    nimcp_auditory_adapter_t adapter = (nimcp_auditory_adapter_t)module;
    if (!adapter || !state) return NIMCP_LAYER_ERR_NULL_PTR;
    if (size < sizeof(nimcp_auditory_adapter_state_t)) return NIMCP_LAYER_ERR_INVALID_MSG;
    adapter->state = *(const nimcp_auditory_adapter_state_t*)state;
    return NIMCP_LAYER_OK;
}

static const char* auditory_get_name(void* module) {
    (void)module;
    return "Auditory_Adapter";
}

nimcp_auditory_adapter_config_t nimcp_auditory_adapter_default_config(void) {
    nimcp_auditory_adapter_config_t config = {
        .sample_rate = 44100, .num_channels = 2, .num_frequency_bands = 64,
        .min_freq_hz = 20.0f, .max_freq_hz = 20000.0f,
        .enable_speech_detection = true, .enable_logging = false
    };
    return config;
}

nimcp_auditory_adapter_t nimcp_auditory_adapter_create(const nimcp_auditory_adapter_config_t* config) {
    nimcp_auditory_adapter_t adapter = (nimcp_auditory_adapter_t)nimcp_calloc(1, sizeof(struct nimcp_auditory_adapter_struct));
    NIMCP_API_CHECK_ALLOC(adapter, "Failed to allocate auditory adapter");
    adapter->config = config ? *config : nimcp_auditory_adapter_default_config();
    adapter->interface.init = auditory_init;
    adapter->interface.shutdown = auditory_shutdown;
    adapter->interface.update = auditory_update;
    adapter->interface.handle_message = auditory_handle_message;
    adapter->interface.get_state = auditory_get_state;
    adapter->interface.set_state = auditory_set_state;
    adapter->interface.get_name = auditory_get_name;
    return adapter;
}

void nimcp_auditory_adapter_destroy(nimcp_auditory_adapter_t adapter) {
    if (!adapter) return;
    if (adapter->is_initialized) auditory_shutdown(adapter);
    nimcp_free(adapter);
}

nimcp_module_interface_t* nimcp_auditory_adapter_get_interface(nimcp_auditory_adapter_t adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_auditory_adapter_get_interface: adapter is NULL");
        return NULL;
    }
    return &adapter->interface;
}

nimcp_layer_error_t nimcp_auditory_adapter_process_samples(nimcp_auditory_adapter_t adapter, const float* samples, uint32_t count) {
    if (!adapter || !samples) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_auditory_adapter_process_samples: required parameter is NULL");
        return NIMCP_LAYER_ERR_NULL_PTR;
    }
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    float sum = 0.0f;
    for (uint32_t i = 0; i < count; i++) sum += fabsf(samples[i]);
    adapter->state.mean_amplitude = sum / (float)count;
    adapter->stats.samples_processed += count;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_auditory_adapter_get_spectrum(nimcp_auditory_adapter_t adapter, float* spectrum_out, uint32_t max_bands, uint32_t* count_out) {
    if (!adapter || !spectrum_out || !count_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_auditory_adapter_get_spectrum: required parameter is NULL");
        return NIMCP_LAYER_ERR_NULL_PTR;
    }
    uint32_t count = adapter->config.num_frequency_bands < max_bands ? adapter->config.num_frequency_bands : max_bands;
    memcpy(spectrum_out, adapter->spectrum_buffer, count * sizeof(float));
    *count_out = count;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_auditory_adapter_get_state(nimcp_auditory_adapter_t adapter, nimcp_auditory_adapter_state_t* state_out) {
    if (!adapter || !state_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_auditory_adapter_get_state: required parameter is NULL");
        return NIMCP_LAYER_ERR_NULL_PTR;
    }
    *state_out = adapter->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_auditory_adapter_get_stats(nimcp_auditory_adapter_t adapter, nimcp_auditory_adapter_stats_t* stats_out) {
    if (!adapter || !stats_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_auditory_adapter_get_stats: required parameter is NULL");
        return NIMCP_LAYER_ERR_NULL_PTR;
    }
    *stats_out = adapter->stats;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_auditory_adapter_reset_stats(nimcp_auditory_adapter_t adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_auditory_adapter_reset_stats: adapter is NULL");
        return NIMCP_LAYER_ERR_NULL_PTR;
    }
    memset(&adapter->stats, 0, sizeof(adapter->stats));
    return NIMCP_LAYER_OK;
}
