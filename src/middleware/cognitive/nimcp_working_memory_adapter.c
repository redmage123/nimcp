#include <stddef.h>

#define LOG_MODULE "nimcp_working_memory_adapter"
#define LOG_MODULE_ID 0x0516
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(working_memory_adapter)

/**
 * @file nimcp_working_memory_adapter.c
 * @brief Working memory adapter implementation (stub for testing)
 */

#include "middleware/cognitive/nimcp_working_memory_adapter.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "utils/memory/nimcp_memory.h"
#include <string.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"


struct working_memory_adapter_struct {
    brain_temporal_buffer_t* buffer;
    brain_feature_normalizer_t* normalizer;
    brain_spike_feature_extractor_t spike_extractor;
    working_memory_adapter_config_t config;

    /* Thread safety: mutex protects buffer, normalizer, and spike_extractor access.
     * Added to fix HIGH PRIORITY thread-safety issue - concurrent calls to
     * working_memory_adapter_update() could corrupt internal state. */
    nimcp_mutex_t* mutex;
};

working_memory_adapter_config_t working_memory_adapter_default_config(void) {
    return (working_memory_adapter_config_t){
        .num_channels = 100,
        .buffer_size = BUFFER_SIZE_100MS,
        .norm_type = NORMALIZE_ZSCORE,
        .max_features = 50,
        .enable_spike_features = false,
        .enable_oscillations = true
    };
}

working_memory_adapter_t working_memory_adapter_create(
    const working_memory_adapter_config_t* config
) {
    if (!config || config->num_channels == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "working_memory_adapter_create: config is NULL");
        return NULL;
    }

    struct working_memory_adapter_struct* adapter =
        nimcp_calloc(1, sizeof(struct working_memory_adapter_struct));
    if (!adapter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;

    }

    adapter->config = *config;

    /* Thread safety: Create mutex to protect adapter state.
     * This fixes HIGH PRIORITY thread-safety issue where concurrent calls
     * to working_memory_adapter_update() could corrupt internal state. */
    adapter->mutex = nimcp_mutex_create(NULL);
    if (!adapter->mutex) {
        working_memory_adapter_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "working_memory_adapter_create: adapter->mutex is NULL");
        return NULL;
    }

    adapter->buffer = brain_create_temporal_buffer(
        config->num_channels, config->buffer_size
    );
    if (!adapter->buffer) {
        working_memory_adapter_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "working_memory_adapter_create: adapter->buffer is NULL");
        return NULL;
    }

    // Only create normalizer if max_features > 0
    if (config->max_features > 0) {
        adapter->normalizer = brain_create_feature_normalizer(
            config->max_features, config->norm_type
        );
        if (!adapter->normalizer) {
            working_memory_adapter_destroy(adapter);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "working_memory_adapter_create: adapter->normalizer is NULL");
            return NULL;
        }
    }

    if (config->enable_spike_features) {
        adapter->spike_extractor = brain_create_spike_feature_extractor(
            config->num_channels, config->enable_oscillations, true
        );
        if (!adapter->spike_extractor) {
            working_memory_adapter_destroy(adapter);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "working_memory_adapter_create: adapter->spike_extractor is NULL");
            return NULL;
        }
    }

    return adapter;
}

void working_memory_adapter_destroy(working_memory_adapter_t adapter) {
    if (!adapter) return;
    brain_destroy_temporal_buffer(adapter->buffer);
    brain_destroy_feature_normalizer(adapter->normalizer);
    brain_destroy_spike_feature_extractor(adapter->spike_extractor);

    /* Thread safety: Clean up mutex */
    if (adapter->mutex) {
        nimcp_mutex_free(adapter->mutex);
    }

    nimcp_free(adapter);
}

/**
 * WHAT: Update adapter with new neural activity
 * WHY:  Process new timestep of neural data
 * HOW:  Buffer activity, extract and normalize features
 *
 * THREAD SAFETY: This function is now protected by a mutex to prevent
 * concurrent access from corrupting internal buffer and normalizer state.
 * This fixes a HIGH PRIORITY thread-safety issue identified in code review.
 */
uint32_t working_memory_adapter_update(
    working_memory_adapter_t adapter,
    const float* activity,
    uint32_t num_channels,
    uint64_t timestamp,
    float* features_out
) {
    if (!adapter || !activity || !features_out) return 0;
    if (num_channels != adapter->config.num_channels) return 0;

    /* Thread safety: Lock mutex to protect buffer and normalizer access.
     * This prevents concurrent calls from corrupting internal state. */
    nimcp_mutex_lock(adapter->mutex);

    // Buffer activity
    if (!brain_buffer_activity(adapter->buffer, activity, num_channels, timestamp)) {
        nimcp_mutex_unlock(adapter->mutex);
        return 0;
    }

    // If no normalizer (max_features == 0), return 0 features
    if (!adapter->normalizer || adapter->config.max_features == 0) {
        nimcp_mutex_unlock(adapter->mutex);
        return 0;
    }

    // Extract and normalize features
    uint32_t result = brain_extract_and_normalize(
        adapter->buffer, adapter->normalizer, features_out, adapter->config.max_features
    );

    nimcp_mutex_unlock(adapter->mutex);
    return result;
}
