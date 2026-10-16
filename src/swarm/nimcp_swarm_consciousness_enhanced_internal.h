/**
 * @file nimcp_swarm_consciousness_enhanced_internal.h
 * @brief Internal shared types and functions for swarm consciousness enhanced module
 *
 * WHAT: Private API shared between split files of consciousness_enhanced module
 * WHY:  Enable SRP refactoring while keeping public API unchanged
 * HOW:  Internal structures, helper functions, cross-module declarations
 *
 * @author NIMCP Development Team
 * @date 2026-02-16
 * @version 2.6.3
 */

#ifndef NIMCP_SWARM_CONSCIOUSNESS_ENHANCED_INTERNAL_H
#define NIMCP_SWARM_CONSCIOUSNESS_ENHANCED_INTERNAL_H

#include "swarm/nimcp_swarm_consciousness_enhanced.h"
#include "swarm/nimcp_swarm_brain.h"
#include "swarm/nimcp_swarm_signal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/statistics/nimcp_statistics.h"
#include "security/nimcp_bbb_helpers.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_bio_async.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Module Constants
 * ============================================================================ */

#define MODULE_NAME "swarm_consciousness_enhanced"

/** Magic value for context validation */
#define ENHANCED_CONSCIOUSNESS_MAGIC 0x45434F4E  // 'ECON'

/** Minimum samples for geometry computation */
#define MIN_GEOMETRY_SAMPLES 10

/** Minimum samples for dynamics computation */
#define MIN_DYNAMICS_SAMPLES 20

/** Default entropy bins for mutual information */
#define DEFAULT_ENTROPY_BINS 16

/** Critical slowing autocorrelation threshold */
#define CRITICAL_AUTOCORRELATION_THRESHOLD 0.8f

/** Phase coherence epsilon for binding detection */
#define PHASE_COHERENCE_EPSILON 0.01f

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * Remote phi storage for collected values
 */
typedef struct {
    uint16_t drone_id;
    float phi_value;
    uint64_t timestamp_ms;
    bool valid;
} remote_phi_entry_t;

/**
 * Enhanced consciousness context internal structure
 */
struct swarm_consciousness_enhanced_ctx {
    uint32_t magic;                          /**< Validation magic */
    swarm_consciousness_enhanced_config_t config;  /**< Configuration */
    nimcp_mutex_t lock;                    /**< Thread safety */

    /* Swarm reference */
    swarm_brain_t* attached_swarm;           /**< Attached swarm brain */
    bool attached;                           /**< Is attached to swarm? */

    /* Remote phi collection */
    remote_phi_entry_t remote_phi[SWARM_CONSCIOUSNESS_MAX_DRONES];
    uint32_t remote_phi_count;
    phi_request_t pending_requests[SWARM_CONSCIOUSNESS_MAX_PHI_REQUESTS];
    uint32_t pending_request_count;
    float local_phi;                         /**< Local drone's phi */

    /* Phi history for dynamics */
    float phi_history[SWARM_CONSCIOUSNESS_PHI_HISTORY_SIZE];
    float phi_timestamps[SWARM_CONSCIOUSNESS_PHI_HISTORY_SIZE];
    uint32_t history_count;
    uint32_t history_index;

    /* Current state */
    consciousness_phase_t current_phase;
    neural_binding_t current_binding;
    bool binding_active;

    /* Callbacks */
    peer_event_callback_t peer_callback;
    void* peer_callback_data;
    phase_transition_callback_t phase_callback;
    void* phase_callback_data;
    binding_event_callback_t binding_callback;
    void* binding_callback_data;

    /* Statistics */
    uint64_t total_phi_requests;
    uint64_t total_phi_responses;
    uint64_t peer_join_count;
    uint64_t peer_leave_count;
    uint64_t phase_transitions;
    uint64_t binding_events;

    /* Bio-async */
    bool bio_async_registered;

    /* Signal adapter for phi messaging */
    nimcp_swarm_signal_adapter_t* signal_adapter;

    /* Creation time */
    uint64_t creation_time_ms;
};

/* ============================================================================
 * Cross-Module Function Declarations
 * ============================================================================ */

/* From enhanced_compute.c */
swarm_consciousness_enhanced_metrics_t* enhanced_compute_metrics_impl(
    swarm_consciousness_enhanced_ctx_t* ctx,
    swarm_brain_t* swarm);

/* From enhanced_stats.c */
bool enhanced_compute_geometry_impl(
    swarm_consciousness_enhanced_ctx_t* ctx,
    information_geometry_t* geometry);

bool enhanced_compute_dynamics_impl(
    swarm_consciousness_enhanced_ctx_t* ctx,
    consciousness_dynamics_t* dynamics);

bool enhanced_compute_binding_impl(
    swarm_consciousness_enhanced_ctx_t* ctx,
    neural_binding_t* binding);

/* From enhanced_hierarchy.c */
bool enhanced_compute_hierarchy_impl(
    swarm_consciousness_enhanced_ctx_t* ctx,
    swarm_brain_t* swarm,
    hierarchical_consciousness_t* hierarchy);

bool enhanced_compute_resilience_impl(
    swarm_consciousness_enhanced_ctx_t* ctx,
    swarm_brain_t* swarm,
    consciousness_resilience_t* resilience);

/* From enhanced_core.c */
void enhanced_add_phi_to_history(swarm_consciousness_enhanced_ctx_t* ctx, float phi);
void enhanced_invoke_peer_callback(swarm_consciousness_enhanced_ctx_t* ctx,
                                    peer_event_type_t type, uint16_t drone_id,
                                    float phi, uint32_t new_count);
void enhanced_invoke_phase_callback(swarm_consciousness_enhanced_ctx_t* ctx,
                                     consciousness_phase_t old_phase,
                                     consciousness_phase_t new_phase,
                                     const swarm_consciousness_enhanced_metrics_t* metrics);
void enhanced_invoke_binding_callback(swarm_consciousness_enhanced_ctx_t* ctx,
                                       const neural_binding_t* binding);

/* ============================================================================
 * Shared Utility Functions
 * ============================================================================ */

static inline uint64_t enhanced_get_time_ms(void) {
    return nimcp_time_get_ms();
}

static inline float enhanced_compute_mean(const float* data, uint32_t count) {
    if (!data || count == 0) return 0.0f;
    float sum = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        sum += data[i];
    }
    return sum / count;
}

static inline float enhanced_compute_variance(const float* data, uint32_t count) {
    if (!data || count < 2) return 0.0f;
    float mean = enhanced_compute_mean(data, count);
    float variance = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        float diff = data[i] - mean;
        variance += diff * diff;
    }
    return variance / count;
}

float enhanced_compute_autocorrelation(const float* data, uint32_t count, uint32_t lag);
float enhanced_estimate_entropy(const float* data, uint32_t count, uint32_t bins, float bin_width);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_CONSCIOUSNESS_ENHANCED_INTERNAL_H */
