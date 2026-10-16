#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_buffer_immune.c - Buffer Immune System Integration Implementation
//=============================================================================

#include "middleware/immune/nimcp_buffer_immune.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#define LOG_MODULE "middleware_buffer_immune"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(buffer_immune)

//=============================================================================
// INTERNAL STRUCTURES
//=============================================================================

/**
 * @brief Buffer immune system state
 *
 * WHAT: Tracks all registered buffers and immune integration state
 * WHY:  Manage bidirectional buffer-immune communication
 * HOW:  Array of buffer handles with health tracking
 */
struct buffer_immune_system {
    buffer_immune_config_t config;          /**< Configuration */
    brain_immune_system_t* brain_immune;    /**< Brain immune system */

    /* Buffer registry */
    buffer_immune_handle_t buffers[BUFFER_IMMUNE_MAX_BUFFERS];
    uint32_t buffer_count;                  /**< Active buffer count */
    uint32_t next_buffer_id;                /**< Next ID to assign */

    /* Statistics */
    buffer_immune_stats_t stats;

    /* Callbacks */
    buffer_immune_anomaly_cb_t on_anomaly;
    void* callback_user_data;

    /* Thread safety */
    void* mutex;                            /**< Platform mutex */
};

//=============================================================================
// HELPER FUNCTIONS
//=============================================================================

/**
 * @brief Get inflammation-based capacity multiplier
 *
 * WHAT: Map inflammation level to capacity reduction
 * WHY:  Model synaptic dysfunction during inflammation
 * HOW:  Return fixed multipliers per level
 */
static float get_capacity_multiplier(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return 1.0F;
        case INFLAMMATION_LOCAL:    return 0.9F;
        case INFLAMMATION_REGIONAL: return 0.75F;
        case INFLAMMATION_SYSTEMIC: return 0.5F;
        case INFLAMMATION_STORM:    return 0.25F;
        default:                    return 1.0F;
    }
}

/**
 * @brief Find buffer by ID
 *
 * WHAT: Locate registered buffer handle
 * WHY:  Access buffer for operations
 * HOW:  Linear search through buffer array
 */
static buffer_immune_handle_t* find_buffer(
    buffer_immune_system_t* system,
    uint32_t buffer_id
) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;

    }

    for (uint32_t i = 0; i < system->buffer_count; i++) {
        if (system->buffers[i].id == buffer_id) {
            return &system->buffers[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_buffer: validation failed");
    return NULL;
}

/**
 * @brief Calculate buffer health score
 *
 * WHAT: Compute normalized health metric
 * WHY:  Quantify buffer status
 * HOW:  Combine utilization, anomaly rate, modulation impact
 */
static float calculate_health_score(const buffer_immune_handle_t* handle) {
    if (!handle) return 0.0F;

    float score = 1.0F;

    /* Penalize capacity reduction */
    score *= handle->capacity_multiplier;

    /* Penalize recent anomalies */
    if (handle->anomaly_count > 0) {
        float anomaly_penalty = 1.0F / (1.0F + handle->anomaly_count * 0.1F);
        score *= anomaly_penalty;
    }

    /* Penalize consecutive overflows */
    if (handle->consecutive_overflows > 0) {
        float overflow_penalty = 1.0F / (1.0F + handle->consecutive_overflows * 0.2F);
        score *= overflow_penalty;
    }

    return score;
}

/**
 * @brief Release cytokine for buffer event
 *
 * WHAT: Trigger immune signaling from buffer anomaly
 * WHY:  Communicate buffer stress to immune system
 * HOW:  Call brain_immune_release_cytokine with appropriate type
 */
static int release_buffer_cytokine(
    buffer_immune_system_t* system,
    buffer_anomaly_t anomaly,
    uint32_t buffer_id
) {
    if (!system || !system->brain_immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "release_buffer_cytokine: required parameter is NULL (system, system->brain_immune)");
        return -1;
    }

    brain_cytokine_type_t cytokine_type;
    float concentration = 0.5F;

    switch (anomaly) {
        case BUFFER_ANOMALY_OVERFLOW:
            cytokine_type = CYTOKINE_IL1B;  /* Alert cytokine - using brain_immune enum */
            concentration = 0.5F;
            break;
        case BUFFER_ANOMALY_CORRUPTION:
            cytokine_type = CYTOKINE_TNFA;  /* Severe response - using brain_immune enum */
            concentration = 0.9F;
            break;
        case BUFFER_ANOMALY_COHERENCE_LOSS:
            cytokine_type = BRAIN_CYTOKINE_IFN_GAMMA;  /* Quarantine signal - using brain_immune enum */
            concentration = 0.7F;
            break;
        case BUFFER_ANOMALY_THRASHING:
            cytokine_type = CYTOKINE_IL6;  /* Escalation - using brain_immune enum */
            concentration = 0.6F;
            break;
        default:
            return 0;  /* No cytokine for other anomalies */
    }

    uint32_t cytokine_id;
    return brain_immune_release_cytokine(
        system->brain_immune,
        cytokine_type,
        buffer_id,
        concentration,
        0,  /* Broadcast */
        &cytokine_id
    );
}

//=============================================================================
// LIFECYCLE IMPLEMENTATION
//=============================================================================

int buffer_immune_default_config(buffer_immune_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_immune_default_config: config is NULL");
        return -1;
    }

    config->enable_capacity_modulation = true;
    config->enable_anomaly_detection = true;
    config->enable_auto_immune_alert = true;
    config->overflow_threshold = BUFFER_IMMUNE_OVERFLOW_THRESH;
    config->coherence_min_threshold = BUFFER_IMMUNE_COHERENCE_MIN;
    config->persistent_overflow_count = BUFFER_IMMUNE_PERSISTENT_OVERFLOWS;
    config->enable_logging = true;

    return 0;
}

buffer_immune_system_t* buffer_immune_create(
    brain_immune_system_t* brain_immune,
    const buffer_immune_config_t* config
) {
    if (!brain_immune) {
        LOG_ERROR("Brain immune system required");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_immune_create: brain_immune is NULL");
        return NULL;
    }

    buffer_immune_system_t* system = nimcp_malloc(sizeof(buffer_immune_system_t));
    if (!system) {
        LOG_ERROR("Failed to allocate buffer immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "buffer_immune_create: system is NULL");
        return NULL;
    }

    memset(system, 0, sizeof(buffer_immune_system_t));

    /* Set configuration */
    if (config) {
        system->config = *config;
    } else {
        buffer_immune_default_config(&system->config);
    }

    system->brain_immune = brain_immune;
    system->next_buffer_id = 1;

    LOG_INFO("Buffer immune system created");
    return system;
}

void buffer_immune_destroy(buffer_immune_system_t* system) {
    if (!system) return;

    /* Restore all buffers before cleanup */
    buffer_immune_restore_all(system);

    LOG_INFO("Buffer immune system destroyed");
    nimcp_free(system);
}

//=============================================================================
// BUFFER REGISTRATION IMPLEMENTATION
//=============================================================================

int buffer_immune_register_circular(
    buffer_immune_system_t* system,
    circular_buffer_t* buffer,
    const char* name,
    uint32_t* buffer_id
) {
    if (!system || !buffer || !buffer_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_immune_register_circular: required parameter is NULL (system, buffer, buffer_id)");
        return -1;
    }
    if (system->buffer_count >= BUFFER_IMMUNE_MAX_BUFFERS) {
        LOG_ERROR("Maximum buffer count reached");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "buffer_immune_register_circular: capacity exceeded");
        return -1;
    }

    buffer_immune_handle_t* handle = &system->buffers[system->buffer_count];
    memset(handle, 0, sizeof(buffer_immune_handle_t));

    handle->id = system->next_buffer_id++;
    handle->type = BUFFER_TYPE_CIRCULAR;
    handle->buffer_ptr = buffer;
    handle->health = BUFFER_HEALTH_OPTIMAL;
    handle->health_score = 1.0F;
    handle->capacity_multiplier = 1.0F;
    handle->integration_window_multiplier = 1.0F;

    if (name) {
        strncpy(handle->name, name, sizeof(handle->name) - 1);
    } else {
        snprintf(handle->name, sizeof(handle->name), "circular_%u", handle->id);
    }

    handle->original_capacity = circular_buffer_capacity(buffer);
    handle->modulated_capacity = handle->original_capacity;

    *buffer_id = handle->id;
    system->buffer_count++;

    LOG_INFO("Registered circular buffer '%s' (id=%u, capacity=%zu)",
             handle->name, handle->id, handle->original_capacity);
    return 0;
}

int buffer_immune_register_integration(
    buffer_immune_system_t* system,
    integration_buffer_t* buffer,
    const char* name,
    uint32_t* buffer_id
) {
    if (!system || !buffer || !buffer_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_immune_register_integration: required parameter is NULL (system, buffer, buffer_id)");
        return -1;
    }
    if (system->buffer_count >= BUFFER_IMMUNE_MAX_BUFFERS) {
        LOG_ERROR("Maximum buffer count reached");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "buffer_immune_register_integration: capacity exceeded");
        return -1;
    }

    buffer_immune_handle_t* handle = &system->buffers[system->buffer_count];
    memset(handle, 0, sizeof(buffer_immune_handle_t));

    handle->id = system->next_buffer_id++;
    handle->type = BUFFER_TYPE_INTEGRATION;
    handle->buffer_ptr = buffer;
    handle->health = BUFFER_HEALTH_OPTIMAL;
    handle->health_score = 1.0F;
    handle->capacity_multiplier = 1.0F;
    handle->integration_window_multiplier = 1.0F;

    if (name) {
        strncpy(handle->name, name, sizeof(handle->name) - 1);
    } else {
        snprintf(handle->name, sizeof(handle->name), "integration_%u", handle->id);
    }

    /* Integration buffers have multiple timescale capacities */
    handle->original_capacity = integration_buffer_capacity(buffer, TIMESCALE_FAST);
    handle->modulated_capacity = handle->original_capacity;

    *buffer_id = handle->id;
    system->buffer_count++;

    LOG_INFO("Registered integration buffer '%s' (id=%u)", handle->name, handle->id);
    return 0;
}

int buffer_immune_register_phase_coded(
    buffer_immune_system_t* system,
    phase_coded_buffer_t* buffer,
    const char* name,
    uint32_t* buffer_id
) {
    if (!system || !buffer || !buffer_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_immune_register_phase_coded: required parameter is NULL (system, buffer, buffer_id)");
        return -1;
    }
    if (system->buffer_count >= BUFFER_IMMUNE_MAX_BUFFERS) {
        LOG_ERROR("Maximum buffer count reached");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "buffer_immune_register_phase_coded: capacity exceeded");
        return -1;
    }

    buffer_immune_handle_t* handle = &system->buffers[system->buffer_count];
    memset(handle, 0, sizeof(buffer_immune_handle_t));

    handle->id = system->next_buffer_id++;
    handle->type = BUFFER_TYPE_PHASE_CODED;
    handle->buffer_ptr = buffer;
    handle->health = BUFFER_HEALTH_OPTIMAL;
    handle->health_score = 1.0F;
    handle->capacity_multiplier = 1.0F;
    handle->integration_window_multiplier = 1.0F;

    if (name) {
        strncpy(handle->name, name, sizeof(handle->name) - 1);
    } else {
        snprintf(handle->name, sizeof(handle->name), "phase_coded_%u", handle->id);
    }

    uint32_t capacity = 0;
    phase_buffer_get_stats(buffer, &capacity, NULL, NULL);
    handle->original_capacity = capacity;
    handle->modulated_capacity = handle->original_capacity;

    *buffer_id = handle->id;
    system->buffer_count++;

    LOG_INFO("Registered phase-coded buffer '%s' (id=%u)", handle->name, handle->id);
    return 0;
}

int buffer_immune_register_sliding_window(
    buffer_immune_system_t* system,
    sliding_window_t* window,
    const char* name,
    uint32_t* buffer_id
) {
    if (!system || !window || !buffer_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_immune_register_sliding_window: required parameter is NULL (system, window, buffer_id)");
        return -1;
    }
    if (system->buffer_count >= BUFFER_IMMUNE_MAX_BUFFERS) {
        LOG_ERROR("Maximum buffer count reached");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "buffer_immune_register_sliding_window: capacity exceeded");
        return -1;
    }

    buffer_immune_handle_t* handle = &system->buffers[system->buffer_count];
    memset(handle, 0, sizeof(buffer_immune_handle_t));

    handle->id = system->next_buffer_id++;
    handle->type = BUFFER_TYPE_SLIDING_WINDOW;
    handle->buffer_ptr = window;
    handle->health = BUFFER_HEALTH_OPTIMAL;
    handle->health_score = 1.0F;
    handle->capacity_multiplier = 1.0F;
    handle->integration_window_multiplier = 1.0F;

    if (name) {
        strncpy(handle->name, name, sizeof(handle->name) - 1);
    } else {
        snprintf(handle->name, sizeof(handle->name), "sliding_window_%u", handle->id);
    }

    handle->original_capacity = sliding_window_size(window);
    handle->modulated_capacity = handle->original_capacity;

    *buffer_id = handle->id;
    system->buffer_count++;

    LOG_INFO("Registered sliding window '%s' (id=%u)", handle->name, handle->id);
    return 0;
}

int buffer_immune_register_temporal_accumulator(
    buffer_immune_system_t* system,
    temporal_accumulator_t* accumulator,
    const char* name,
    uint32_t* buffer_id
) {
    if (!system || !accumulator || !buffer_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_immune_register_temporal_accumulator: required parameter is NULL (system, accumulator, buffer_id)");
        return -1;
    }
    if (system->buffer_count >= BUFFER_IMMUNE_MAX_BUFFERS) {
        LOG_ERROR("Maximum buffer count reached");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "buffer_immune_register_temporal_accumulator: capacity exceeded");
        return -1;
    }

    buffer_immune_handle_t* handle = &system->buffers[system->buffer_count];
    memset(handle, 0, sizeof(buffer_immune_handle_t));

    handle->id = system->next_buffer_id++;
    handle->type = BUFFER_TYPE_TEMPORAL_ACCUMULATOR;
    handle->buffer_ptr = accumulator;
    handle->health = BUFFER_HEALTH_OPTIMAL;
    handle->health_score = 1.0F;
    handle->capacity_multiplier = 1.0F;
    handle->integration_window_multiplier = 1.0F;

    if (name) {
        strncpy(handle->name, name, sizeof(handle->name) - 1);
    } else {
        snprintf(handle->name, sizeof(handle->name), "temporal_accumulator_%u", handle->id);
    }

    handle->original_capacity = temporal_accumulator_num_channels(accumulator);
    handle->modulated_capacity = handle->original_capacity;

    *buffer_id = handle->id;
    system->buffer_count++;

    LOG_INFO("Registered temporal accumulator '%s' (id=%u)", handle->name, handle->id);
    return 0;
}

int buffer_immune_unregister(
    buffer_immune_system_t* system,
    uint32_t buffer_id
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_immune_unregister: system is NULL");
        return -1;
    }

    /* Find and remove buffer */
    for (uint32_t i = 0; i < system->buffer_count; i++) {
        if (system->buffers[i].id == buffer_id) {
            /* Restore capacity before removal */
            buffer_immune_restore_capacity(system, buffer_id);

            /* Shift remaining buffers down */
            if (i < system->buffer_count - 1) {
                memmove(&system->buffers[i], &system->buffers[i + 1],
                        (system->buffer_count - i - 1) * sizeof(buffer_immune_handle_t));
            }
            system->buffer_count--;

            LOG_INFO("Unregistered buffer id=%u", buffer_id);
            return 0;
        }
    }

    LOG_WARN("Buffer id=%u not found for unregistration", buffer_id);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "buffer_immune_unregister: operation failed");
    return -1;
}

//=============================================================================
// IMMUNE → BUFFER MODULATION IMPLEMENTATION
//=============================================================================

int buffer_immune_modulate_capacity(
    buffer_immune_system_t* system,
    uint32_t buffer_id,
    brain_inflammation_level_t inflammation_level
) {
    if (!system || !system->config.enable_capacity_modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_immune_modulate_capacity: required parameter is NULL (system, system->config)");
        return -1;
    }

    buffer_immune_handle_t* handle = find_buffer(system, buffer_id);
    if (!handle) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_immune_modulate_capacity: handle is NULL");
        return -1;
    }

    float multiplier = get_capacity_multiplier(inflammation_level);
    handle->capacity_multiplier = multiplier;
    handle->integration_window_multiplier = multiplier;
    handle->modulated_capacity = (size_t)(handle->original_capacity * multiplier);

    /* Update health based on modulation */
    if (multiplier < 0.5F) {
        handle->health = BUFFER_HEALTH_CRITICAL;
    } else if (multiplier < 0.75F) {
        handle->health = BUFFER_HEALTH_DEGRADED;
    } else if (multiplier < 1.0F) {
        handle->health = BUFFER_HEALTH_STRESSED;
    } else {
        handle->health = BUFFER_HEALTH_OPTIMAL;
    }

    handle->health_score = calculate_health_score(handle);
    system->stats.capacity_reductions++;
    system->stats.current_modulation = multiplier;

    LOG_DEBUG("Modulated buffer '%s' capacity: %zu -> %zu (%.0f%%)",
              handle->name, handle->original_capacity,
              handle->modulated_capacity, multiplier * 100.0F);

    return 0;
}

int buffer_immune_modulate_all(
    buffer_immune_system_t* system,
    brain_inflammation_level_t inflammation_level
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_immune_modulate_all: system is NULL");
        return -1;
    }

    int modulated_count = 0;
    for (uint32_t i = 0; i < system->buffer_count; i++) {
        if (buffer_immune_modulate_capacity(system, system->buffers[i].id,
                                            inflammation_level) == 0) {
            modulated_count++;
        }
    }

    LOG_INFO("Modulated %d buffers for inflammation level %d",
             modulated_count, inflammation_level);
    return modulated_count;
}

int buffer_immune_restore_capacity(
    buffer_immune_system_t* system,
    uint32_t buffer_id
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_immune_restore_capacity: system is NULL");
        return -1;
    }

    buffer_immune_handle_t* handle = find_buffer(system, buffer_id);
    if (!handle) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_immune_restore_capacity: handle is NULL");
        return -1;
    }

    handle->capacity_multiplier = 1.0F;
    handle->integration_window_multiplier = 1.0F;
    handle->modulated_capacity = handle->original_capacity;
    handle->health = BUFFER_HEALTH_OPTIMAL;
    handle->health_score = 1.0F;
    handle->consecutive_overflows = 0;

    LOG_DEBUG("Restored buffer '%s' to original capacity %zu",
              handle->name, handle->original_capacity);
    return 0;
}

int buffer_immune_restore_all(buffer_immune_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_immune_restore_all: system is NULL");
        return -1;
    }

    int restored_count = 0;
    for (uint32_t i = 0; i < system->buffer_count; i++) {
        if (buffer_immune_restore_capacity(system, system->buffers[i].id) == 0) {
            restored_count++;
        }
    }

    LOG_INFO("Restored %d buffers to original capacity", restored_count);
    return restored_count;
}

//=============================================================================
// BUFFER → IMMUNE ANOMALY DETECTION IMPLEMENTATION
//=============================================================================

int buffer_immune_report_overflow(
    buffer_immune_system_t* system,
    uint32_t buffer_id,
    float utilization
) {
    if (!system || !system->config.enable_anomaly_detection) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_immune_report_overflow: required parameter is NULL (system, system->config)");
        return -1;
    }

    buffer_immune_handle_t* handle = find_buffer(system, buffer_id);
    if (!handle) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_immune_report_overflow: handle is NULL");
        return -1;
    }

    handle->consecutive_overflows++;
    handle->anomaly_count++;
    handle->last_anomaly = BUFFER_ANOMALY_OVERFLOW;
    system->stats.overflows_reported++;
    system->stats.anomalies_detected++;

    /* Check if persistent overflow */
    if (handle->consecutive_overflows >= system->config.persistent_overflow_count) {
        if (system->config.enable_auto_immune_alert) {
            /* Present as antigen */
            uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
            memset(epitope, 0, sizeof(epitope));
            memcpy(epitope, &buffer_id, sizeof(buffer_id));
            epitope[4] = BUFFER_ANOMALY_OVERFLOW;

            uint32_t antigen_id;
            brain_immune_present_antigen(
                system->brain_immune,
                ANTIGEN_SOURCE_ANOMALY,
                epitope,
                5,
                5,  /* Medium severity */
                buffer_id,
                &antigen_id
            );

            /* Release IL-1β for alert */
            release_buffer_cytokine(system, BUFFER_ANOMALY_OVERFLOW, buffer_id);
            system->stats.immune_alerts_sent++;

            LOG_WARN("Persistent overflow in buffer '%s' (antigen_id=%u)",
                     handle->name, antigen_id);
        }
    }

    handle->health = BUFFER_HEALTH_STRESSED;
    handle->health_score = calculate_health_score(handle);

    /* Trigger callback */
    if (system->on_anomaly) {
        system->on_anomaly(system, buffer_id, BUFFER_ANOMALY_OVERFLOW,
                          system->callback_user_data);
    }

    return 0;
}

int buffer_immune_report_corruption(
    buffer_immune_system_t* system,
    uint32_t buffer_id,
    const uint8_t* corruption_signature,
    size_t signature_len
) {
    if (!system || !system->config.enable_anomaly_detection) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_immune_report_corruption: required parameter is NULL (system, system->config)");
        return -1;
    }
    if (!corruption_signature || signature_len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "buffer_immune_report_corruption: corruption_signature is NULL");
        return -1;
    }

    buffer_immune_handle_t* handle = find_buffer(system, buffer_id);
    if (!handle) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_immune_report_corruption: handle is NULL");
        return -1;
    }

    handle->anomaly_count++;
    handle->last_anomaly = BUFFER_ANOMALY_CORRUPTION;
    system->stats.anomalies_detected++;

    /* Corruption is immediate high-severity threat */
    if (system->config.enable_auto_immune_alert) {
        uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
        memset(epitope, 0, sizeof(epitope));

        size_t copy_len = (signature_len < sizeof(epitope) - 1) ?
                          signature_len : sizeof(epitope) - 1;
        memcpy(epitope, corruption_signature, copy_len);
        epitope[sizeof(epitope) - 1] = BUFFER_ANOMALY_CORRUPTION;

        uint32_t antigen_id;
        brain_immune_present_antigen(
            system->brain_immune,
            ANTIGEN_SOURCE_ANOMALY,
            epitope,
            copy_len + 1,
            9,  /* High severity */
            buffer_id,
            &antigen_id
        );

        /* Release TNF-α for severe response */
        release_buffer_cytokine(system, BUFFER_ANOMALY_CORRUPTION, buffer_id);
        system->stats.immune_alerts_sent++;

        LOG_ERROR("Buffer corruption in '%s' (antigen_id=%u)",
                  handle->name, antigen_id);
    }

    handle->health = BUFFER_HEALTH_CRITICAL;
    handle->health_score = calculate_health_score(handle);

    /* Trigger callback */
    if (system->on_anomaly) {
        system->on_anomaly(system, buffer_id, BUFFER_ANOMALY_CORRUPTION,
                          system->callback_user_data);
    }

    return 0;
}

int buffer_immune_report_coherence_loss(
    buffer_immune_system_t* system,
    uint32_t buffer_id,
    float coherence
) {
    if (!system || !system->config.enable_anomaly_detection) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_immune_report_coherence_loss: required parameter is NULL (system, system->config)");
        return -1;
    }

    buffer_immune_handle_t* handle = find_buffer(system, buffer_id);
    if (!handle) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_immune_report_coherence_loss: handle is NULL");
        return -1;
    }

    /* Only alert if below threshold */
    if (coherence < system->config.coherence_min_threshold) {
        handle->anomaly_count++;
        handle->last_anomaly = BUFFER_ANOMALY_COHERENCE_LOSS;
        system->stats.anomalies_detected++;

        if (system->config.enable_auto_immune_alert) {
            uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
            memset(epitope, 0, sizeof(epitope));
            memcpy(epitope, &buffer_id, sizeof(buffer_id));
            memcpy(epitope + 4, &coherence, sizeof(coherence));
            epitope[8] = BUFFER_ANOMALY_COHERENCE_LOSS;

            uint32_t antigen_id;
            brain_immune_present_antigen(
                system->brain_immune,
                ANTIGEN_SOURCE_ANOMALY,
                epitope,
                9,
                6,  /* Medium-high severity */
                buffer_id,
                &antigen_id
            );

            /* Release IFN-γ for quarantine signal */
            release_buffer_cytokine(system, BUFFER_ANOMALY_COHERENCE_LOSS, buffer_id);
            system->stats.immune_alerts_sent++;

            LOG_WARN("Coherence loss in buffer '%s': %.3f (antigen_id=%u)",
                     handle->name, coherence, antigen_id);
        }

        handle->health = BUFFER_HEALTH_DEGRADED;
        handle->health_score = calculate_health_score(handle);

        /* Trigger callback */
        if (system->on_anomaly) {
            system->on_anomaly(system, buffer_id, BUFFER_ANOMALY_COHERENCE_LOSS,
                              system->callback_user_data);
        }
    }

    return 0;
}

int buffer_immune_check_health(
    buffer_immune_system_t* system,
    uint32_t buffer_id,
    buffer_health_t* health
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_immune_check_health: system is NULL");
        return -1;
    }

    buffer_immune_handle_t* handle = find_buffer(system, buffer_id);
    if (!handle) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_immune_check_health: handle is NULL");
        return -1;
    }

    /* Update health score */
    handle->health_score = calculate_health_score(handle);

    /* Check utilization for different buffer types */
    float utilization = 0.0F;
    switch (handle->type) {
        case BUFFER_TYPE_CIRCULAR: {
            circular_buffer_t* buf = (circular_buffer_t*)handle->buffer_ptr;
            utilization = circular_buffer_utilization(buf) / 100.0F;
            break;
        }
        case BUFFER_TYPE_SLIDING_WINDOW: {
            sliding_window_t* win = (sliding_window_t*)handle->buffer_ptr;
            size_t count = sliding_window_count(win);
            size_t size = sliding_window_size(win);
            utilization = (size > 0) ? (float)count / size : 0.0F;
            break;
        }
        default:
            /* Other buffer types don't have simple utilization */
            break;
    }

    /* Check for overflow */
    if (utilization >= system->config.overflow_threshold) {
        buffer_immune_report_overflow(system, buffer_id, utilization);
    } else {
        /* Reset consecutive overflow count if not full */
        handle->consecutive_overflows = 0;
    }

    /* Check phase coherence for phase-coded buffers */
    if (handle->type == BUFFER_TYPE_PHASE_CODED) {
        phase_coded_buffer_t* pbuf = (phase_coded_buffer_t*)handle->buffer_ptr;
        float coherence = phase_buffer_coherence(pbuf);
        buffer_immune_report_coherence_loss(system, buffer_id, coherence);
    }

    if (health) {
        *health = handle->health;
    }

    return (handle->health == BUFFER_HEALTH_OPTIMAL) ? 0 : 1;
}

int buffer_immune_check_all_health(
    buffer_immune_system_t* system,
    buffer_health_t* worst_health
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_immune_check_all_health: system is NULL");
        return -1;
    }

    buffer_health_t worst = BUFFER_HEALTH_OPTIMAL;
    int unhealthy_count = 0;

    for (uint32_t i = 0; i < system->buffer_count; i++) {
        buffer_health_t health;
        if (buffer_immune_check_health(system, system->buffers[i].id, &health) != 0) {
            unhealthy_count++;
            if (health > worst) {
                worst = health;
            }
        }
    }

    if (worst_health) {
        *worst_health = worst;
    }

    return unhealthy_count;
}

//=============================================================================
// QUERY IMPLEMENTATION
//=============================================================================

float buffer_immune_get_health_score(
    buffer_immune_system_t* system,
    uint32_t buffer_id
) {
    if (!system) return 0.0F;

    buffer_immune_handle_t* handle = find_buffer(system, buffer_id);
    if (!handle) return 0.0F;

    return handle->health_score;
}

float buffer_immune_get_capacity_multiplier(
    buffer_immune_system_t* system,
    uint32_t buffer_id
) {
    if (!system) return 1.0F;

    buffer_immune_handle_t* handle = find_buffer(system, buffer_id);
    if (!handle) return 1.0F;

    return handle->capacity_multiplier;
}

int buffer_immune_get_stats(
    buffer_immune_system_t* system,
    buffer_immune_stats_t* stats
) {
    if (!system || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_immune_get_stats: required parameter is NULL (system, stats)");
        return -1;
    }

    /* Update average health score */
    float total_health = 0.0F;
    for (uint32_t i = 0; i < system->buffer_count; i++) {
        total_health += system->buffers[i].health_score;
    }
    system->stats.avg_health_score = (system->buffer_count > 0) ?
        total_health / system->buffer_count : 1.0F;

    *stats = system->stats;
    return 0;
}

uint32_t buffer_immune_get_buffer_count(buffer_immune_system_t* system) {
    return system ? system->buffer_count : 0;
}

//=============================================================================
// CALLBACK IMPLEMENTATION
//=============================================================================

int buffer_immune_set_anomaly_callback(
    buffer_immune_system_t* system,
    buffer_immune_anomaly_cb_t callback,
    void* user_data
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_immune_set_anomaly_callback: system is NULL");
        return -1;
    }

    system->on_anomaly = callback;
    system->callback_user_data = user_data;
    return 0;
}

//=============================================================================
// UPDATE IMPLEMENTATION
//=============================================================================

int buffer_immune_update(
    buffer_immune_system_t* system,
    uint64_t delta_ms
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_immune_update: system is NULL");
        return -1;
    }

    /* Periodic health check for all buffers */
    buffer_health_t worst_health;
    buffer_immune_check_all_health(system, &worst_health);

    return 0;
}

//=============================================================================
// UTILITY IMPLEMENTATION
//=============================================================================

const char* buffer_immune_buffer_type_to_string(buffer_type_t type) {
    switch (type) {
        case BUFFER_TYPE_CIRCULAR:            return "circular";
        case BUFFER_TYPE_INTEGRATION:         return "integration";
        case BUFFER_TYPE_PHASE_CODED:         return "phase_coded";
        case BUFFER_TYPE_SLIDING_WINDOW:      return "sliding_window";
        case BUFFER_TYPE_TEMPORAL_ACCUMULATOR: return "temporal_accumulator";
        default:                              return "unknown";
    }
}

const char* buffer_immune_health_to_string(buffer_health_t health) {
    switch (health) {
        case BUFFER_HEALTH_OPTIMAL:   return "optimal";
        case BUFFER_HEALTH_STRESSED:  return "stressed";
        case BUFFER_HEALTH_DEGRADED:  return "degraded";
        case BUFFER_HEALTH_CRITICAL:  return "critical";
        case BUFFER_HEALTH_FAILED:    return "failed";
        default:                      return "unknown";
    }
}

const char* buffer_immune_anomaly_to_string(buffer_anomaly_t anomaly) {
    switch (anomaly) {
        case BUFFER_ANOMALY_NONE:          return "none";
        case BUFFER_ANOMALY_OVERFLOW:      return "overflow";
        case BUFFER_ANOMALY_CORRUPTION:    return "corruption";
        case BUFFER_ANOMALY_COHERENCE_LOSS: return "coherence_loss";
        case BUFFER_ANOMALY_STARVATION:    return "starvation";
        case BUFFER_ANOMALY_THRASHING:     return "thrashing";
        default:                           return "unknown";
    }
}
