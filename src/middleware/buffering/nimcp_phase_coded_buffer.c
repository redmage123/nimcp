#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_phase_coded_buffer.c - Phase-Coded Working Memory Buffer
//=============================================================================

#include "middleware/buffering/nimcp_phase_coded_buffer.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/math/nimcp_complex_math.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "middleware_phase_coded_buffer"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(phase_coded_buffer)

#include <string.h>
#include <math.h>
#include <stdlib.h>


// ============================================================================
// INTERNAL STRUCTURE
// ============================================================================

struct phase_coded_buffer {
    // Configuration
    phase_buffer_config_t config;

    // Storage
    phase_coded_item_t* items;
    uint32_t count;

    // Phase tracking
    float current_phase;        // Current auto-increment phase
    uint32_t sequence_number;   // Item sequence counter

    // Statistics
    double last_store_time_ms;
    uint64_t total_stores;
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Wrap phase to [0, 2π)
 * Using [0, 2π) instead of [-π, π] ensures ascending phase values
 * sort correctly without sign-flip artifacts at the π boundary.
 */
static float wrap_phase(float phase) {
    const float TWO_PI = 2.0F * (float)M_PI;
    while (phase >= TWO_PI) phase -= TWO_PI;
    while (phase < 0.0F) phase += TWO_PI;
    return phase;
}

/**
 * @brief Comparison function for qsort by phase
 */
static int compare_by_phase(const void* a, const void* b) {
    const phase_coded_item_t* item_a = (const phase_coded_item_t*)a;
    const phase_coded_item_t* item_b = (const phase_coded_item_t*)b;

    if (item_a->phase < item_b->phase) return -1;
    if (item_a->phase > item_b->phase) return 1;
    return 0;
}

// ============================================================================
// PUBLIC API
// ============================================================================

phase_buffer_config_t phase_buffer_default_config(void) {
    phase_buffer_config_t config;
    config.capacity = PHASE_BUFFER_DEFAULT_CAPACITY;
    config.auto_phase_increment = true;
    config.phase_increment = (2.0F * M_PI) / 8.0F;  // 8 items per cycle
    config.theta_frequency_hz = 8.0F;               // 8 Hz theta
    config.enable_coherence_sort = false;
    return config;
}

phase_coded_buffer_t* phase_buffer_create(const phase_buffer_config_t* config) {
    if (!config || config->capacity == 0 || config->capacity > PHASE_BUFFER_MAX_CAPACITY) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "phase_buffer_create: config is NULL or capacity invalid");
        return NULL;
    }

    phase_coded_buffer_t* buffer = (phase_coded_buffer_t*)nimcp_calloc(1,
                                                    sizeof(phase_coded_buffer_t));
    if (!buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "phase_buffer_create: failed to allocate buffer");
        return NULL;
    }

    buffer->config = *config;

    // Allocate item storage
    buffer->items = (phase_coded_item_t*)nimcp_calloc(config->capacity,
                                                      sizeof(phase_coded_item_t));
    if (!buffer->items) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "phase_buffer_create: failed to allocate items");
        phase_buffer_destroy(buffer);
        return NULL;
    }

    buffer->count = 0;
    buffer->current_phase = 0.0F;
    buffer->sequence_number = 0;
    buffer->last_store_time_ms = 0.0;
    buffer->total_stores = 0;

    return buffer;
}

void phase_buffer_destroy(phase_coded_buffer_t* buffer) {
    if (!buffer) return;
    nimcp_free(buffer->items);
    nimcp_free(buffer);
}

bool phase_buffer_store(phase_coded_buffer_t* buffer,
                        float data,
                        float amplitude,
                        double timestamp_ms) {
    if (!buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phase_buffer_store: buffer is NULL");
        return false;
    }

    // Check capacity
    if (buffer->count >= buffer->config.capacity) {
        return false;  // Buffer full - not an error
    }

    float phase;
    if (buffer->config.auto_phase_increment) {
        phase = buffer->current_phase;
        buffer->current_phase = wrap_phase(buffer->current_phase +
                                          buffer->config.phase_increment);
    } else {
        // Use theta-based phase from timestamp
        float theta_period_ms = 1000.0F / buffer->config.theta_frequency_hz;
        phase = fmodf((float)timestamp_ms, theta_period_ms) / theta_period_ms * 2.0F * M_PI;
        phase = wrap_phase(phase);
    }

    return phase_buffer_store_with_phase(buffer, data, phase, amplitude, timestamp_ms);
}

bool phase_buffer_store_with_phase(phase_coded_buffer_t* buffer,
                                    float data,
                                    float phase,
                                    float amplitude,
                                    double timestamp_ms) {
    if (!buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phase_buffer_store_with_phase: buffer is NULL");
        return false;
    }

    // Check capacity
    if (buffer->count >= buffer->config.capacity) {
        return false;  // Buffer full - not an error
    }

    // Store item
    phase_coded_item_t* item = &buffer->items[buffer->count];
    item->data = data;
    item->phase = wrap_phase(phase);
    item->amplitude = amplitude;
    item->timestamp_ms = timestamp_ms;

    buffer->count++;
    buffer->sequence_number++;
    buffer->last_store_time_ms = timestamp_ms;
    buffer->total_stores++;

    return true;
}

bool phase_buffer_retrieve_ordered(const phase_coded_buffer_t* buffer,
                                    phase_coded_item_t* items,
                                    uint32_t max_items,
                                    uint32_t* num_retrieved) {
    if (!buffer || !items || !num_retrieved) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phase_buffer_retrieve_ordered: required parameter is NULL (buffer, items, num_retrieved)");
        return false;
    }

    *num_retrieved = 0;

    if (buffer->count == 0) return true;

    // Copy items for sorting
    uint32_t count = (buffer->count < max_items) ? buffer->count : max_items;
    memcpy(items, buffer->items, count * sizeof(phase_coded_item_t));

    // Sort by phase
    qsort(items, count, sizeof(phase_coded_item_t), compare_by_phase);

    *num_retrieved = count;
    return true;
}

bool phase_buffer_pattern_match(const phase_coded_buffer_t* buffer,
                                 const float* pattern_phases,
                                 uint32_t pattern_count,
                                 float min_coherence,
                                 phase_pattern_match_t* result) {
    if (!buffer || !pattern_phases || !result || pattern_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phase_buffer_pattern_match: required parameter is NULL (buffer, pattern_phases, result)");
        return false;
    }

    result->indices = NULL;
    result->coherences = NULL;
    result->count = 0;
    result->mean_coherence = 0.0F;

    if (buffer->count == 0) return true;

    // Convert pattern to phasors
    neural_phasor_t* pattern_phasors = (neural_phasor_t*)nimcp_malloc(
        pattern_count * sizeof(neural_phasor_t));
    if (!pattern_phasors) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "phase_buffer_pattern_match: pattern_phasors allocation failed");
        return false;
    }

    for (uint32_t i = 0; i < pattern_count; i++) {
        pattern_phasors[i] = phasor_from_polar(1.0F, pattern_phases[i]);
    }

    // Compute pattern mean phase
    float pattern_mean_phase = phasor_array_mean_phase(pattern_phasors, pattern_count);

    // Allocate temporary arrays for matches
    uint32_t* temp_indices = (uint32_t*)nimcp_malloc(buffer->count * sizeof(uint32_t));
    float* temp_coherences = (float*)nimcp_malloc(buffer->count * sizeof(float));

    if (!temp_indices || !temp_coherences) {
        nimcp_free(pattern_phasors);
        nimcp_free(temp_indices);
        nimcp_free(temp_coherences);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "phase_buffer_pattern_match: temp arrays allocation failed");
        return false;
    }

    // Find matching items
    uint32_t match_count = 0;
    float sum_coherence = 0.0F;

    for (uint32_t i = 0; i < buffer->count; i++) {
        // Create phasor for this item
        neural_phasor_t item_phasor = phasor_from_polar(
            buffer->items[i].amplitude, buffer->items[i].phase);

        // Compute angular distance from pattern mean (shortest arc in [0, 2π))
        float raw_diff = wrap_phase(buffer->items[i].phase - pattern_mean_phase);
        float phase_diff = raw_diff;
        if (phase_diff > (float)M_PI) {
            phase_diff = 2.0F * (float)M_PI - phase_diff;
        }

        // Simple coherence: inverse of phase difference
        float coherence = 1.0F - (phase_diff / (float)M_PI);

        if (coherence >= min_coherence) {
            temp_indices[match_count] = i;
            temp_coherences[match_count] = coherence;
            sum_coherence += coherence;
            match_count++;
        }
    }

    // Allocate final result arrays
    if (match_count > 0) {
        result->indices = (uint32_t*)nimcp_malloc(match_count * sizeof(uint32_t));
        result->coherences = (float*)nimcp_malloc(match_count * sizeof(float));

        if (result->indices && result->coherences) {
            memcpy(result->indices, temp_indices, match_count * sizeof(uint32_t));
            memcpy(result->coherences, temp_coherences, match_count * sizeof(float));
            result->count = match_count;
            result->mean_coherence = sum_coherence / (float)match_count;
        } else {
            nimcp_free(result->indices);
            nimcp_free(result->coherences);
            result->indices = NULL;
            result->coherences = NULL;
            match_count = 0;
        }
    }

    nimcp_free(pattern_phasors);
    nimcp_free(temp_indices);
    nimcp_free(temp_coherences);

    return true;
}

float phase_buffer_coherence(const phase_coded_buffer_t* buffer) {
    if (!buffer || buffer->count == 0) return 0.0F;

    // Convert items to phasors
    neural_phasor_t* phasors = (neural_phasor_t*)nimcp_malloc(
        buffer->count * sizeof(neural_phasor_t));
    if (!phasors) return 0.0F;

    for (uint32_t i = 0; i < buffer->count; i++) {
        phasors[i] = phasor_from_polar(buffer->items[i].amplitude,
                                       buffer->items[i].phase);
    }

    // Compute inter-trial phase coherence using phasor utilities
    float coherence = phasor_array_coherence(phasors, buffer->count);

    nimcp_free(phasors);
    return coherence;
}

float phase_buffer_mean_phase(const phase_coded_buffer_t* buffer) {
    if (!buffer || buffer->count == 0) return 0.0F;

    // Convert items to phasors
    neural_phasor_t* phasors = (neural_phasor_t*)nimcp_malloc(
        buffer->count * sizeof(neural_phasor_t));
    if (!phasors) return 0.0F;

    for (uint32_t i = 0; i < buffer->count; i++) {
        phasors[i] = phasor_from_polar(buffer->items[i].amplitude,
                                       buffer->items[i].phase);
    }

    // Compute circular mean phase using phasor utilities
    float mean_phase = phasor_array_mean_phase(phasors, buffer->count);

    nimcp_free(phasors);
    return mean_phase;
}

void phase_buffer_clear(phase_coded_buffer_t* buffer) {
    if (!buffer) return;

    buffer->count = 0;
    buffer->current_phase = 0.0F;
    buffer->sequence_number = 0;
}

bool phase_buffer_get_stats(const phase_coded_buffer_t* buffer,
                             uint32_t* count,
                             uint32_t* capacity,
                             float* mean_coherence) {
    if (!buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phase_buffer_clear: buffer is NULL");
        return false;
    }

    if (count) *count = buffer->count;
    if (capacity) *capacity = buffer->config.capacity;
    if (mean_coherence) {
        *mean_coherence = phase_buffer_coherence(buffer);
    }

    return true;
}

void phase_pattern_match_free(phase_pattern_match_t* result) {
    if (!result) return;

    nimcp_free(result->indices);
    nimcp_free(result->coherences);
    result->indices = NULL;
    result->coherences = NULL;
    result->count = 0;
    result->mean_coherence = 0.0F;
}
