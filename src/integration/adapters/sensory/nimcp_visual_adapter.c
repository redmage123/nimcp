/**
 * @file nimcp_visual_adapter.c
 * @brief Visual Processing Adapter Implementation
 * @version 1.0.0
 * @date 2026-01-10
 */

#include "integration/adapters/sensory/nimcp_visual_adapter.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(visual_adapter)

struct nimcp_visual_adapter_struct {
    nimcp_visual_adapter_config_t config;
    nimcp_module_interface_t interface;
    float* feature_buffer;
    uint32_t num_features;
    nimcp_visual_adapter_state_t state;
    nimcp_visual_adapter_stats_t stats;
    bool is_initialized;
};

static nimcp_layer_error_t visual_init(void* module, void* config) {
    nimcp_visual_adapter_t adapter = (nimcp_visual_adapter_t)module;
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    if (config) adapter->config = *(nimcp_visual_adapter_config_t*)config;
    adapter->num_features = 128;
    adapter->feature_buffer = (float*)nimcp_calloc(adapter->num_features, sizeof(float));
    if (!adapter->feature_buffer) return NIMCP_LAYER_ERR_NO_MEMORY;
    adapter->is_initialized = true;
    adapter->state.is_active = true;
    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t visual_shutdown(void* module) {
    nimcp_visual_adapter_t adapter = (nimcp_visual_adapter_t)module;
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    nimcp_free(adapter->feature_buffer);
    adapter->feature_buffer = NULL;
    adapter->is_initialized = false;
    adapter->state.is_active = false;
    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t visual_update(void* module, float dt) {
    nimcp_visual_adapter_t adapter = (nimcp_visual_adapter_t)module;
    if (!adapter || !adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    (void)dt;
    adapter->stats.updates_processed++;
    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t visual_handle_message(void* module, const nimcp_layer_msg_t* msg) {
    nimcp_visual_adapter_t adapter = (nimcp_visual_adapter_t)module;
    if (!adapter || !msg) return NIMCP_LAYER_ERR_NULL_PTR;
    adapter->stats.messages_handled++;
    if (msg->header.msg_type == NIMCP_LAYER_MSG_DATA_PUSH && msg->payload) {
        adapter->stats.frames_processed++;
    }
    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t visual_get_state(void* module, void* state_out, size_t* size) {
    nimcp_visual_adapter_t adapter = (nimcp_visual_adapter_t)module;
    if (!adapter || !state_out || !size) return NIMCP_LAYER_ERR_NULL_PTR;
    if (*size < sizeof(nimcp_visual_adapter_state_t)) {
        *size = sizeof(nimcp_visual_adapter_state_t);
        return NIMCP_LAYER_ERR_CAPACITY;
    }
    *(nimcp_visual_adapter_state_t*)state_out = adapter->state;
    *size = sizeof(nimcp_visual_adapter_state_t);
    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t visual_set_state(void* module, const void* state, size_t size) {
    nimcp_visual_adapter_t adapter = (nimcp_visual_adapter_t)module;
    if (!adapter || !state) return NIMCP_LAYER_ERR_NULL_PTR;
    if (size < sizeof(nimcp_visual_adapter_state_t)) return NIMCP_LAYER_ERR_INVALID_MSG;
    adapter->state = *(const nimcp_visual_adapter_state_t*)state;
    return NIMCP_LAYER_OK;
}

static const char* visual_get_name(void* module) {
    (void)module;
    return "Visual_Adapter";
}

nimcp_visual_adapter_config_t nimcp_visual_adapter_default_config(void) {
    nimcp_visual_adapter_config_t config = {
        .width = 640, .height = 480, .num_channels = 3,
        .enable_uv = false, .enable_nir = false, .enable_thermal = false, .enable_logging = false
    };
    return config;
}

nimcp_visual_adapter_t nimcp_visual_adapter_create(const nimcp_visual_adapter_config_t* config) {
    nimcp_visual_adapter_t adapter = (nimcp_visual_adapter_t)nimcp_calloc(1, sizeof(struct nimcp_visual_adapter_struct));
    NIMCP_API_CHECK_ALLOC(adapter, "Failed to allocate visual adapter");
    adapter->config = config ? *config : nimcp_visual_adapter_default_config();
    adapter->interface.init = visual_init;
    adapter->interface.shutdown = visual_shutdown;
    adapter->interface.update = visual_update;
    adapter->interface.handle_message = visual_handle_message;
    adapter->interface.get_state = visual_get_state;
    adapter->interface.set_state = visual_set_state;
    adapter->interface.get_name = visual_get_name;
    return adapter;
}

void nimcp_visual_adapter_destroy(nimcp_visual_adapter_t adapter) {
    if (!adapter) return;
    if (adapter->is_initialized) visual_shutdown(adapter);
    nimcp_free(adapter);
}

nimcp_module_interface_t* nimcp_visual_adapter_get_interface(nimcp_visual_adapter_t adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_visual_adapter_get_interface: adapter is NULL");
        return NULL;
    }
    return &adapter->interface;
}

nimcp_layer_error_t nimcp_visual_adapter_process_frame(nimcp_visual_adapter_t adapter, const float* pixels, uint32_t size) {
    if (!adapter || !pixels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_visual_adapter_process_frame: required parameter is NULL");
        return NIMCP_LAYER_ERR_NULL_PTR;
    }
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    float sum = 0.0f;
    for (uint32_t i = 0; i < size && i < 1000; i++) sum += pixels[i];
    adapter->state.mean_luminance = sum / (float)(size > 1000 ? 1000 : size);
    adapter->stats.frames_processed++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_visual_adapter_get_features(nimcp_visual_adapter_t adapter, float* features_out, uint32_t max_features, uint32_t* count_out) {
    if (!adapter || !features_out || !count_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_visual_adapter_get_features: required parameter is NULL");
        return NIMCP_LAYER_ERR_NULL_PTR;
    }
    uint32_t count = adapter->num_features < max_features ? adapter->num_features : max_features;
    memcpy(features_out, adapter->feature_buffer, count * sizeof(float));
    *count_out = count;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_visual_adapter_get_state(nimcp_visual_adapter_t adapter, nimcp_visual_adapter_state_t* state_out) {
    if (!adapter || !state_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_visual_adapter_get_state: required parameter is NULL");
        return NIMCP_LAYER_ERR_NULL_PTR;
    }
    *state_out = adapter->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_visual_adapter_get_stats(nimcp_visual_adapter_t adapter, nimcp_visual_adapter_stats_t* stats_out) {
    if (!adapter || !stats_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_visual_adapter_get_stats: required parameter is NULL");
        return NIMCP_LAYER_ERR_NULL_PTR;
    }
    *stats_out = adapter->stats;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_visual_adapter_reset_stats(nimcp_visual_adapter_t adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_visual_adapter_reset_stats: adapter is NULL");
        return NIMCP_LAYER_ERR_NULL_PTR;
    }
    memset(&adapter->stats, 0, sizeof(adapter->stats));
    return NIMCP_LAYER_OK;
}
