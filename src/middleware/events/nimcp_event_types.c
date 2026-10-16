#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_event_types.c - Event Type Implementation
//=============================================================================

#include "middleware/events/nimcp_event_types.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "middleware_event_types"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(event_types)

#include <string.h>
#include <stdio.h>
#include <stdatomic.h>

//=============================================================================
// Global Sequence Counter (Atomic)
//=============================================================================

static atomic_uint_least64_t global_sequence = ATOMIC_VAR_INIT(0);

/**
 * WHAT: Get next sequence number atomically
 * WHY:  Globally ordered events for debugging
 * HOW:  Atomic fetch_add
 */
static uint64_t get_next_sequence(void) {
    return atomic_fetch_add(&global_sequence, 1);
}

//=============================================================================
// Event Creation Helpers
//=============================================================================

/**
 * WHAT: Initialize common event metadata
 * WHY:  DRY - don't repeat in every constructor
 * HOW:  Fill in type, priority, source, timestamp, sequence
 */
static void init_event_metadata(event_t* event, event_type_t type,
                                 mw_event_priority_t priority, event_source_t source) {
    event->type = type;
    event->priority = priority;
    event->source = source;
    event->timestamp_us = nimcp_time_get_us();
    event->sequence_number = get_next_sequence();
}

//=============================================================================
// Event Constructors (Factory Functions)
//=============================================================================

event_t event_create_spike_burst(uint32_t* neuron_ids, uint32_t num_neurons,
                                  float synchrony, uint64_t duration_us,
                                  mw_event_priority_t priority, event_source_t source) {
    event_t event = {0};
    init_event_metadata(&event, EVENT_TYPE_SPIKE_BURST, priority, source);

    event.data.spike_burst.neuron_ids = neuron_ids;
    event.data.spike_burst.num_neurons = num_neurons;
    event.data.spike_burst.synchrony_score = synchrony;
    event.data.spike_burst.burst_duration_us = duration_us;

    return event;
}

event_t event_create_pattern_detected(uint32_t pattern_id, float confidence,
                                       uint32_t pattern_length, const char* pattern_name,
                                       mw_event_priority_t priority, event_source_t source) {
    event_t event = {0};
    init_event_metadata(&event, EVENT_TYPE_PATTERN_DETECTED, priority, source);

    event.data.pattern_detected.pattern_id = pattern_id;
    event.data.pattern_detected.match_confidence = confidence;
    event.data.pattern_detected.pattern_length = pattern_length;
    event.data.pattern_detected.pattern_name = pattern_name;

    return event;
}

event_t event_create_attention_shift(uint32_t prev_item, uint32_t curr_item,
                                      float attention_strength, const char* reason,
                                      mw_event_priority_t priority, event_source_t source) {
    event_t event = {0};
    init_event_metadata(&event, EVENT_TYPE_ATTENTION_SHIFT, priority, source);

    event.data.attention_shift.previous_item = prev_item;
    event.data.attention_shift.current_item = curr_item;
    event.data.attention_shift.attention_strength = attention_strength;
    event.data.attention_shift.shift_reason = reason;

    return event;
}

event_t event_create_memory_formed(uint32_t memory_id, float* memory_trace,
                                    uint32_t trace_size, float consolidation,
                                    mw_event_priority_t priority, event_source_t source) {
    event_t event = {0};
    init_event_metadata(&event, EVENT_TYPE_MEMORY_FORMED, priority, source);

    event.data.memory_formed.memory_id = memory_id;
    event.data.memory_formed.memory_trace = memory_trace;
    event.data.memory_formed.trace_size = trace_size;
    event.data.memory_formed.consolidation_strength = consolidation;

    return event;
}

event_t event_create_salience_peak(float salience, float novelty,
                                    float surprise, float urgency,
                                    mw_event_priority_t priority, event_source_t source) {
    event_t event = {0};
    init_event_metadata(&event, EVENT_TYPE_SALIENCE_PEAK, priority, source);

    event.data.salience_peak.salience_score = salience;
    event.data.salience_peak.novelty_score = novelty;
    event.data.salience_peak.surprise_score = surprise;
    event.data.salience_peak.urgency_score = urgency;

    return event;
}

event_t event_create_oscillation_change(float prev_freq, float curr_freq,
                                         float power_change, const char* band_name,
                                         mw_event_priority_t priority, event_source_t source) {
    event_t event = {0};
    init_event_metadata(&event, EVENT_TYPE_OSCILLATION_CHANGE, priority, source);

    event.data.oscillation_change.previous_freq_hz = prev_freq;
    event.data.oscillation_change.current_freq_hz = curr_freq;
    event.data.oscillation_change.power_change = power_change;
    event.data.oscillation_change.band_name = band_name;

    return event;
}

event_t event_create_error_detected(float expected, float actual,
                                     float magnitude, uint32_t location,
                                     mw_event_priority_t priority, event_source_t source) {
    event_t event = {0};
    init_event_metadata(&event, EVENT_TYPE_ERROR_DETECTED, priority, source);

    event.data.error_detected.expected_value = expected;
    event.data.error_detected.actual_value = actual;
    event.data.error_detected.error_magnitude = magnitude;
    event.data.error_detected.error_location = location;

    return event;
}

event_t event_create_decision_made(uint32_t decision_id, float confidence,
                                    float* decision_vector, uint32_t vector_size,
                                    mw_event_priority_t priority, event_source_t source) {
    event_t event = {0};
    init_event_metadata(&event, EVENT_TYPE_DECISION_MADE, priority, source);

    event.data.decision_made.decision_id = decision_id;
    event.data.decision_made.confidence = confidence;
    event.data.decision_made.decision_vector = decision_vector;
    event.data.decision_made.vector_size = vector_size;

    return event;
}

event_t event_create_custom(void* data, uint32_t data_size, const char* description,
                             mw_event_priority_t priority, event_source_t source) {
    event_t event = {0};
    init_event_metadata(&event, EVENT_TYPE_CUSTOM, priority, source);

    event.data.custom.data = data;
    event.data.custom.data_size = data_size;
    event.data.custom.description = description;

    return event;
}

//=============================================================================
// Event Utility Functions
//=============================================================================

const char* event_type_name(event_type_t type) {
    switch (type) {
        case EVENT_TYPE_NONE: return "NONE";
        case EVENT_TYPE_SPIKE_BURST: return "SPIKE_BURST";
        case EVENT_TYPE_PATTERN_DETECTED: return "PATTERN_DETECTED";
        case EVENT_TYPE_OSCILLATION_CHANGE: return "OSCILLATION_CHANGE";
        case EVENT_TYPE_ATTENTION_SHIFT: return "ATTENTION_SHIFT";
        case EVENT_TYPE_MEMORY_FORMED: return "MEMORY_FORMED";
        case EVENT_TYPE_DECISION_MADE: return "DECISION_MADE";
        case EVENT_TYPE_ERROR_DETECTED: return "ERROR_DETECTED";
        case EVENT_TYPE_SALIENCE_PEAK: return "SALIENCE_PEAK";
        case EVENT_TYPE_NOVELTY_DETECTED: return "NOVELTY_DETECTED";
        case EVENT_TYPE_SURPRISE_DETECTED: return "SURPRISE_DETECTED";
        case EVENT_TYPE_THRESHOLD_CROSSED: return "THRESHOLD_CROSSED";
        case EVENT_TYPE_STATE_CHANGED: return "STATE_CHANGED";
        case EVENT_TYPE_CUSTOM: return "CUSTOM";
        default: return "UNKNOWN";
    }
}

const char* event_source_name(event_source_t source) {
    switch (source) {
        case EVENT_SOURCE_UNKNOWN: return "UNKNOWN";
        case EVENT_SOURCE_ENCODER: return "ENCODER";
        case EVENT_SOURCE_FEATURE_EXTRACTOR: return "FEATURE_EXTRACTOR";
        case EVENT_SOURCE_PATTERN_DETECTOR: return "PATTERN_DETECTOR";
        case EVENT_SOURCE_ROUTER: return "ROUTER";
        case EVENT_SOURCE_NORMALIZER: return "NORMALIZER";
        case EVENT_SOURCE_BUFFER: return "BUFFER";
        case EVENT_SOURCE_BRAIN: return "BRAIN";
        case EVENT_SOURCE_ETHICS: return "ETHICS";
        case EVENT_SOURCE_SALIENCE: return "SALIENCE";
        case EVENT_SOURCE_WORKING_MEMORY: return "WORKING_MEMORY";
        case EVENT_SOURCE_PREDICTIVE: return "PREDICTIVE";
        case EVENT_SOURCE_CUSTOM: return "CUSTOM";
        default: return "UNKNOWN";
    }
}

const char* event_priority_name(mw_event_priority_t priority) {
    switch (priority) {
        case MW_EVENT_PRIORITY_CRITICAL: return "CRITICAL";
        case MW_EVENT_PRIORITY_HIGH: return "HIGH";
        case MW_EVENT_PRIORITY_NORMAL: return "NORMAL";
        case MW_EVENT_PRIORITY_LOW: return "LOW";
        case MW_EVENT_PRIORITY_BACKGROUND: return "BACKGROUND";
        default: return "UNKNOWN";
    }
}

bool event_copy(event_t* dest, const event_t* src) {
    if (!dest || !src) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "event_copy: required parameter is NULL (dest, src)");
        return false;
    }

    // Copy basic structure
    *dest = *src;

    // Deep copy pointer data based on type
    switch (src->type) {
        case EVENT_TYPE_SPIKE_BURST:
            if (src->data.spike_burst.neuron_ids && src->data.spike_burst.num_neurons > 0) {
                uint32_t* ids = nimcp_malloc(src->data.spike_burst.num_neurons * sizeof(uint32_t));
                if (!ids) {

                        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,

                            "if: ids is NULL");

                        return false;

                    }
                memcpy(ids, src->data.spike_burst.neuron_ids,
                       src->data.spike_burst.num_neurons * sizeof(uint32_t));
                dest->data.spike_burst.neuron_ids = ids;
            }
            break;

        case EVENT_TYPE_MEMORY_FORMED:
            if (src->data.memory_formed.memory_trace && src->data.memory_formed.trace_size > 0) {
                float* trace = nimcp_malloc(src->data.memory_formed.trace_size * sizeof(float));
                if (!trace) {

                        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,

                            "if: trace is NULL");

                        return false;

                    }
                memcpy(trace, src->data.memory_formed.memory_trace,
                       src->data.memory_formed.trace_size * sizeof(float));
                dest->data.memory_formed.memory_trace = trace;
            }
            break;

        case EVENT_TYPE_DECISION_MADE:
            if (src->data.decision_made.decision_vector && src->data.decision_made.vector_size > 0) {
                float* vec = nimcp_malloc(src->data.decision_made.vector_size * sizeof(float));
                if (!vec) {

                        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,

                            "if: vec is NULL");

                        return false;

                    }
                memcpy(vec, src->data.decision_made.decision_vector,
                       src->data.decision_made.vector_size * sizeof(float));
                dest->data.decision_made.decision_vector = vec;
            }
            break;

        case EVENT_TYPE_CUSTOM:
            if (src->data.custom.data && src->data.custom.data_size > 0) {
                void* data = nimcp_malloc(src->data.custom.data_size);
                if (!data) {

                        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,

                            "if: data is NULL");

                        return false;

                    }
                memcpy(data, src->data.custom.data, src->data.custom.data_size);
                dest->data.custom.data = data;
            }
            break;

        default:
            // Other types don't have dynamic allocations
            break;
    }

    return true;
}

void event_free(event_t* event) {
    if (!event) return;

    // Free dynamic data based on type
    switch (event->type) {
        case EVENT_TYPE_SPIKE_BURST:
            nimcp_free(event->data.spike_burst.neuron_ids);
            event->data.spike_burst.neuron_ids = NULL;
            break;

        case EVENT_TYPE_MEMORY_FORMED:
            nimcp_free(event->data.memory_formed.memory_trace);
            event->data.memory_formed.memory_trace = NULL;
            break;

        case EVENT_TYPE_DECISION_MADE:
            nimcp_free(event->data.decision_made.decision_vector);
            event->data.decision_made.decision_vector = NULL;
            break;

        case EVENT_TYPE_CUSTOM:
            nimcp_free(event->data.custom.data);
            event->data.custom.data = NULL;
            break;

        default:
            break;
    }
}

void event_print(const event_t* event) {
    if (!event) return;

    NIMCP_LOGGING_DEBUG("Event[%s] seq=%lu pri=%s src=%s ts=%lu",
           event_type_name(event->type),
           (unsigned long)event->sequence_number,
           event_priority_name(event->priority),
           event_source_name(event->source),
           (unsigned long)event->timestamp_us);

    // Print type-specific data
    switch (event->type) {
        case EVENT_TYPE_SPIKE_BURST:
            NIMCP_LOGGING_DEBUG("  Neurons=%u Sync=%.3f Duration=%lu us",
                   event->data.spike_burst.num_neurons,
                   event->data.spike_burst.synchrony_score,
                   (unsigned long)event->data.spike_burst.burst_duration_us);
            break;

        case EVENT_TYPE_PATTERN_DETECTED:
            NIMCP_LOGGING_DEBUG("  Pattern=%u \"%s\" Conf=%.3f Len=%u",
                   event->data.pattern_detected.pattern_id,
                   event->data.pattern_detected.pattern_name ?
                       event->data.pattern_detected.pattern_name : "unknown",
                   event->data.pattern_detected.match_confidence,
                   event->data.pattern_detected.pattern_length);
            break;

        case EVENT_TYPE_ATTENTION_SHIFT:
            NIMCP_LOGGING_DEBUG("  %u -> %u Strength=%.3f Reason=%s",
                   event->data.attention_shift.previous_item,
                   event->data.attention_shift.current_item,
                   event->data.attention_shift.attention_strength,
                   event->data.attention_shift.shift_reason ?
                       event->data.attention_shift.shift_reason : "unknown");
            break;

        case EVENT_TYPE_SALIENCE_PEAK:
            NIMCP_LOGGING_DEBUG("  Salience=%.3f Nov=%.3f Surp=%.3f Urg=%.3f",
                   event->data.salience_peak.salience_score,
                   event->data.salience_peak.novelty_score,
                   event->data.salience_peak.surprise_score,
                   event->data.salience_peak.urgency_score);
            break;

        case EVENT_TYPE_ERROR_DETECTED:
            NIMCP_LOGGING_DEBUG("  Expected=%.3f Actual=%.3f Mag=%.3f Loc=%u",
                   event->data.error_detected.expected_value,
                   event->data.error_detected.actual_value,
                   event->data.error_detected.error_magnitude,
                   event->data.error_detected.error_location);
            break;

        default:
            break;
    }
}
