/**
 * @file nimcp_triplet_stdp.c
 * @brief Implementation of Triplet STDP (Pfister & Gerstner 2006)
 *
 * NIMCP Phase: Plasticity Module Enhancement
 * Date: 2025-12-19
 */

#include "plasticity/stdp/nimcp_triplet_stdp.h"
#include "plasticity/nimcp_plasticity_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "plasticity/stdp/nimcp_triplet_stdp_sleep_bridge.h"
#include "plasticity/stdp/nimcp_triplet_stdp_immune_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define LOG_MODULE "triplet_stdp"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(triplet_stdp)

/* ============================================================================
 * Configuration Presets
 * ============================================================================ */

triplet_stdp_config_t triplet_stdp_config_default(void) {
    /* WHAT: Return Pfister & Gerstner (2006) visual cortex parameters
     * WHY:  Provides validated starting point from experimental data
     * HOW:  Use published constants from visual cortex experiments
     */
    triplet_stdp_config_t config = {
        .A2_plus = TRIPLET_STDP_DEFAULT_A2_PLUS,
        .A3_plus = TRIPLET_STDP_DEFAULT_A3_PLUS,
        .A2_minus = TRIPLET_STDP_DEFAULT_A2_MINUS,
        .A3_minus = TRIPLET_STDP_DEFAULT_A3_MINUS,
        .tau_plus = TRIPLET_STDP_DEFAULT_TAU_PLUS,
        .tau_minus = TRIPLET_STDP_DEFAULT_TAU_MINUS,
        .tau_x = TRIPLET_STDP_DEFAULT_TAU_X,
        .tau_y = TRIPLET_STDP_DEFAULT_TAU_Y,
        .w_max = TRIPLET_STDP_DEFAULT_W_MAX,
        .w_min = TRIPLET_STDP_DEFAULT_W_MIN,
        .enable_bio_async = false,
        .enable_sleep_modulation = false,
        .enable_immune_modulation = false
    };
    return config;
}

triplet_stdp_config_t triplet_stdp_config_hippocampal(void) {
    /* WHAT: Return hippocampus-specific triplet parameters
     * WHY:  Hippocampus has different time constants and amplitudes
     * HOW:  Adjusted parameters for CA3-CA1 synapses
     */
    triplet_stdp_config_t config = triplet_stdp_config_default();

    /* Hippocampal synapses have faster dynamics */
    config.tau_plus = 15.0f;   /* Slightly faster */
    config.tau_minus = 30.0f;  /* Slightly faster */
    config.tau_x = 80.0f;      /* Faster slow trace */
    config.tau_y = 100.0f;     /* Faster slow trace */

    /* Stronger triplet terms in hippocampus */
    config.A3_plus = 0.01f;    /* More triplet LTP */
    config.A3_minus = 0.0005f; /* More triplet LTD */

    return config;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

triplet_stdp_synapse_t* triplet_stdp_synapse_create(
    const triplet_stdp_config_t* config,
    float initial_weight
) {
    /* WHAT: Allocate and initialize triplet STDP synapse
     * WHY:  Proper resource allocation with NIMCP memory management
     * HOW:  Use nimcp_malloc, initialize fields, create mutex
     */

    /* Use default config if not provided */
    triplet_stdp_config_t default_config;
    if (!config) {
        default_config = triplet_stdp_config_default();
        config = &default_config;
    }

    /* Allocate synapse */
    triplet_stdp_synapse_t* synapse = (triplet_stdp_synapse_t*)nimcp_malloc(
        sizeof(triplet_stdp_synapse_t)
    );
    if (!synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "triplet_stdp_synapse_create: failed to allocate synapse");
        NIMCP_LOGGING_ERROR("Failed to allocate triplet STDP synapse");
        return NULL;
    }

    /* Initialize fields */
    memset(synapse, 0, sizeof(triplet_stdp_synapse_t));

    /* Weight bounds */
    synapse->w_max = config->w_max;
    synapse->w_min = config->w_min;
    synapse->weight = initial_weight;

    /* Clamp initial weight */
    if (synapse->weight < synapse->w_min) synapse->weight = synapse->w_min;
    if (synapse->weight > synapse->w_max) synapse->weight = synapse->w_max;

    /* Learning parameters */
    synapse->A2_plus = config->A2_plus;
    synapse->A3_plus = config->A3_plus;
    synapse->A2_minus = config->A2_minus;
    synapse->A3_minus = config->A3_minus;

    /* Time constants */
    synapse->tau_plus = config->tau_plus;
    synapse->tau_minus = config->tau_minus;
    synapse->tau_x = config->tau_x;
    synapse->tau_y = config->tau_y;

    /* Initialize traces to zero */
    synapse->r1_pre = 0.0f;
    synapse->r2_pre = 0.0f;
    synapse->o1_post = 0.0f;
    synapse->o2_post = 0.0f;

    /* Initialize spike times */
    synapse->last_pre_spike_time = -1000.0f;  /* Long ago */
    synapse->last_post_spike_time = -1000.0f;

    /* Sleep state */
    synapse->current_sleep_state = SLEEP_STATE_AWAKE;

    /* Statistics */
    synapse->num_ltp_pairwise_events = 0;
    synapse->num_ltp_triplet_events = 0;
    synapse->num_ltd_pairwise_events = 0;
    synapse->num_ltd_triplet_events = 0;
    synapse->total_ltp_pairwise = 0.0f;
    synapse->total_ltp_triplet = 0.0f;
    synapse->total_ltd_pairwise = 0.0f;
    synapse->total_ltd_triplet = 0.0f;

    /* Weight saturation tracking */
    synapse->num_saturate_max_events = 0;
    synapse->num_saturate_min_events = 0;

    /* Initialize spinlock for thread safety (consistent with classic STDP)
     * WHAT: Use spinlock instead of mutex
     * WHY:  Short critical sections (weight updates) benefit from spinlock
     * HOW:  nimcp_spinlock_init initializes the spinlock in-place
     */
    nimcp_spinlock_init(&synapse->lock);

    NIMCP_LOGGING_INFO("Created triplet STDP synapse (w=%.3f, A2+/A3+/A2-/A3-=%.4f/%.4f/%.4f/%.4f)",
                       initial_weight, config->A2_plus, config->A3_plus,
                       config->A2_minus, config->A3_minus);

    return synapse;
}

void triplet_stdp_synapse_destroy(triplet_stdp_synapse_t* synapse) {
    /* WHAT: Free synapse resources
     * WHY:  Proper cleanup to prevent memory leaks
     * HOW:  Free structure (spinlock is embedded, no separate destroy needed)
     */
    if (!synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "triplet_stdp_synapse_destroy: synapse is NULL");
        return;
    }

    /* Spinlock is embedded in struct - no separate destroy needed
     * (unlike mutex which was dynamically allocated)
     */

    /* Free structure */
    nimcp_free(synapse);

    NIMCP_LOGGING_DEBUG("Destroyed triplet STDP synapse");
}

int triplet_stdp_synapse_reset(triplet_stdp_synapse_t* synapse) {
    /* WHAT: Reset traces and statistics, keep parameters and weight
     * WHY:  Start fresh learning without recreating synapse
     * HOW:  Zero out traces and counters
     */
    if (!synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "triplet_stdp_synapse_reset: synapse is NULL");
        NIMCP_LOGGING_ERROR("NULL synapse in reset");
        return -1;
    }

    nimcp_spinlock_lock(&synapse->lock);

    /* Reset traces */
    synapse->r1_pre = 0.0f;
    synapse->r2_pre = 0.0f;
    synapse->o1_post = 0.0f;
    synapse->o2_post = 0.0f;

    /* Reset spike times */
    synapse->last_pre_spike_time = -1000.0f;
    synapse->last_post_spike_time = -1000.0f;

    /* Reset statistics */
    synapse->num_ltp_pairwise_events = 0;
    synapse->num_ltp_triplet_events = 0;
    synapse->num_ltd_pairwise_events = 0;
    synapse->num_ltd_triplet_events = 0;
    synapse->total_ltp_pairwise = 0.0f;
    synapse->total_ltp_triplet = 0.0f;
    synapse->total_ltd_pairwise = 0.0f;
    synapse->total_ltd_triplet = 0.0f;

    /* Reset saturation tracking */
    synapse->num_saturate_max_events = 0;
    synapse->num_saturate_min_events = 0;

    nimcp_spinlock_unlock(&synapse->lock);

    NIMCP_LOGGING_DEBUG("Reset triplet STDP synapse");
    return 0;
}

/* ============================================================================
 * Core Plasticity Functions
 * ============================================================================ */

int triplet_stdp_update_traces(triplet_stdp_synapse_t* synapse, float dt) {
    /* WHAT: Decay all four traces exponentially
     * WHY:  Traces decay between spikes according to time constants
     * HOW:  Multiply each trace by exp(-dt/tau)
     */
    if (!synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "triplet_stdp_update_traces: synapse is NULL");
        NIMCP_LOGGING_ERROR("NULL synapse in update_traces");
        return -1;
    }

    if (dt <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "triplet_stdp_update_traces: dt must be positive");
        return 0;  /* No time passed */
    }

    nimcp_spinlock_lock(&synapse->lock);

    /* NUMERICAL STABILITY: Clamp exponential arguments to prevent underflow
     * WHY:  exp(-x) becomes denormal for x > ~88, negligible for x > 20
     * HOW:  Pre-compute clamped exp arguments to avoid repeated computation
     */
    float exp_arg_plus = -dt / synapse->tau_plus;
    float exp_arg_x = -dt / synapse->tau_x;
    float exp_arg_minus = -dt / synapse->tau_minus;
    float exp_arg_y = -dt / synapse->tau_y;

    /* Clamp to prevent underflow - exp(-20) ≈ 2e-9 which is negligible */
    if (exp_arg_plus < -20.0f) exp_arg_plus = -20.0f;
    if (exp_arg_x < -20.0f) exp_arg_x = -20.0f;
    if (exp_arg_minus < -20.0f) exp_arg_minus = -20.0f;
    if (exp_arg_y < -20.0f) exp_arg_y = -20.0f;

    /* Decay fast pre-trace (tau_plus) */
    synapse->r1_pre *= expf(exp_arg_plus);

    /* Decay slow pre-trace (tau_x) */
    synapse->r2_pre *= expf(exp_arg_x);

    /* Decay fast post-trace (tau_minus) */
    synapse->o1_post *= expf(exp_arg_minus);

    /* Decay slow post-trace (tau_y) */
    synapse->o2_post *= expf(exp_arg_y);

    /* P1 fix: Flush subnormal traces to zero to prevent denormal slowdown
     * WHY:  Subnormal floats (< ~1e-38) cause 10-100x performance degradation
     */
    if (synapse->r1_pre < NIMCP_DENORMAL_THRESHOLD) synapse->r1_pre = 0.0f;
    if (synapse->r2_pre < NIMCP_DENORMAL_THRESHOLD) synapse->r2_pre = 0.0f;
    if (synapse->o1_post < NIMCP_DENORMAL_THRESHOLD) synapse->o1_post = 0.0f;
    if (synapse->o2_post < NIMCP_DENORMAL_THRESHOLD) synapse->o2_post = 0.0f;

    nimcp_spinlock_unlock(&synapse->lock);

    return 0;
}

float triplet_stdp_pre_spike(triplet_stdp_synapse_t* synapse, float spike_time) {
    /* WHAT: Process presynaptic spike (triggers LTD)
     * WHY:  Pre-before-post → depression
     * HOW:  Compute pairwise + triplet LTD, update traces
     *
     * BIOLOGICAL:
     *   dw_LTD = -A2_minus * o1_post(t_pre) - A3_minus * r1_pre(t_pre) * o2_post(t_pre)
     */
    if (!synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "triplet_stdp_pre_spike: synapse is NULL");
        NIMCP_LOGGING_ERROR("NULL synapse in pre_spike");
        return 0.0f;
    }

    nimcp_spinlock_lock(&synapse->lock);

    /* Update traces if time has passed */
    if (synapse->last_pre_spike_time > 0.0f) {
        float dt = spike_time - synapse->last_pre_spike_time;
        if (dt > 0.0f) {
            /* NUMERICAL STABILITY: Clamp exponential arguments to prevent underflow */
            float exp_arg_plus = -dt / synapse->tau_plus;
            float exp_arg_x = -dt / synapse->tau_x;
            if (exp_arg_plus < -20.0f) exp_arg_plus = -20.0f;
            if (exp_arg_x < -20.0f) exp_arg_x = -20.0f;
            synapse->r1_pre *= expf(exp_arg_plus);
            synapse->r2_pre *= expf(exp_arg_x);
            /* P1 fix: Flush subnormal traces to zero to prevent denormal slowdown
             * WHY:  Subnormal floats (< ~1e-38) cause 10-100x performance degradation
             */
            if (synapse->r1_pre < NIMCP_DENORMAL_THRESHOLD) synapse->r1_pre = 0.0f;
            if (synapse->r2_pre < NIMCP_DENORMAL_THRESHOLD) synapse->r2_pre = 0.0f;
        }
    }

    /* Compute pairwise LTD term */
    float dw_pairwise = -synapse->A2_minus * synapse->o1_post;

    /* Compute triplet LTD term */
    float dw_triplet = -synapse->A3_minus * synapse->r1_pre * synapse->o2_post;

    /* Total weight change */
    float total_dw = dw_pairwise + dw_triplet;

    /* Apply weight change with bounds and saturation tracking
     * WHAT: Update weight and detect saturation
     * WHY:  Frequent saturation indicates potential learning issues
     * HOW:  Track when weights hit bounds, log periodic warnings
     */
    float old_weight = synapse->weight;
    synapse->weight += total_dw;

    bool saturated = false;
    if (synapse->weight < synapse->w_min) {
        synapse->weight = synapse->w_min;
        synapse->num_saturate_min_events++;
        saturated = true;
    }
    if (synapse->weight > synapse->w_max) {
        synapse->weight = synapse->w_max;
        synapse->num_saturate_max_events++;
        saturated = true;
    }

    /* Log warning if saturation is frequent (every 100 events) */
    if (saturated) {
        uint64_t total_sat = synapse->num_saturate_min_events + synapse->num_saturate_max_events;
        if (total_sat % 100 == 0) {
            NIMCP_LOGGING_WARN("Triplet STDP weight saturation: min=%lu max=%lu events "
                                  "(w=%.4f, w_min=%.4f, w_max=%.4f). Consider adjusting "
                                  "learning rates or weight bounds.",
                                  (unsigned long)synapse->num_saturate_min_events,
                                  (unsigned long)synapse->num_saturate_max_events,
                                  synapse->weight, synapse->w_min, synapse->w_max);
        }
    }

    float actual_dw = synapse->weight - old_weight;

    /* Update statistics */
    if (dw_pairwise < 0.0f) {
        synapse->num_ltd_pairwise_events++;
        synapse->total_ltd_pairwise += fabsf(dw_pairwise);
    }
    if (dw_triplet < 0.0f) {
        synapse->num_ltd_triplet_events++;
        synapse->total_ltd_triplet += fabsf(dw_triplet);
    }

    /* Increment pre-synaptic traces */
    synapse->r1_pre += 1.0f;
    synapse->r2_pre += 1.0f;

    /* Update last spike time */
    synapse->last_pre_spike_time = spike_time;

    nimcp_spinlock_unlock(&synapse->lock);

    return actual_dw;
}

float triplet_stdp_post_spike(triplet_stdp_synapse_t* synapse, float spike_time) {
    /* WHAT: Process postsynaptic spike (triggers LTP)
     * WHY:  Post-after-pre → potentiation
     * HOW:  Compute pairwise + triplet LTP, update traces
     *
     * BIOLOGICAL:
     *   dw_LTP = A2_plus * r1_pre(t_post) + A3_plus * r2_pre(t_post) * o1_post(t_post)
     */
    if (!synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "triplet_stdp_post_spike: synapse is NULL");
        NIMCP_LOGGING_ERROR("NULL synapse in post_spike");
        return 0.0f;
    }

    nimcp_spinlock_lock(&synapse->lock);

    /* Update traces if time has passed */
    if (synapse->last_post_spike_time > 0.0f) {
        float dt = spike_time - synapse->last_post_spike_time;
        if (dt > 0.0f) {
            /* NUMERICAL STABILITY: Clamp exponential arguments to prevent underflow */
            float exp_arg_minus = -dt / synapse->tau_minus;
            float exp_arg_y = -dt / synapse->tau_y;
            if (exp_arg_minus < -20.0f) exp_arg_minus = -20.0f;
            if (exp_arg_y < -20.0f) exp_arg_y = -20.0f;
            synapse->o1_post *= expf(exp_arg_minus);
            synapse->o2_post *= expf(exp_arg_y);
            /* P1 fix: Flush subnormal traces to zero to prevent denormal slowdown
             * WHY:  Subnormal floats (< ~1e-38) cause 10-100x performance degradation
             */
            if (synapse->o1_post < NIMCP_DENORMAL_THRESHOLD) synapse->o1_post = 0.0f;
            if (synapse->o2_post < NIMCP_DENORMAL_THRESHOLD) synapse->o2_post = 0.0f;
        }
    }

    /* Compute pairwise LTP term */
    float dw_pairwise = synapse->A2_plus * synapse->r1_pre;

    /* Compute triplet LTP term */
    float dw_triplet = synapse->A3_plus * synapse->r2_pre * synapse->o1_post;

    /* Total weight change */
    float total_dw = dw_pairwise + dw_triplet;

    /* Apply weight change with bounds and saturation tracking
     * WHAT: Update weight and detect saturation
     * WHY:  Frequent saturation indicates potential learning issues
     * HOW:  Track when weights hit bounds, log periodic warnings
     */
    float old_weight = synapse->weight;
    synapse->weight += total_dw;

    bool saturated = false;
    if (synapse->weight < synapse->w_min) {
        synapse->weight = synapse->w_min;
        synapse->num_saturate_min_events++;
        saturated = true;
    }
    if (synapse->weight > synapse->w_max) {
        synapse->weight = synapse->w_max;
        synapse->num_saturate_max_events++;
        saturated = true;
    }

    /* Log warning if saturation is frequent (every 100 events) */
    if (saturated) {
        uint64_t total_sat = synapse->num_saturate_min_events + synapse->num_saturate_max_events;
        if (total_sat % 100 == 0) {
            NIMCP_LOGGING_WARN("Triplet STDP weight saturation: min=%lu max=%lu events "
                                  "(w=%.4f, w_min=%.4f, w_max=%.4f). Consider adjusting "
                                  "learning rates or weight bounds.",
                                  (unsigned long)synapse->num_saturate_min_events,
                                  (unsigned long)synapse->num_saturate_max_events,
                                  synapse->weight, synapse->w_min, synapse->w_max);
        }
    }

    float actual_dw = synapse->weight - old_weight;

    /* Update statistics */
    if (dw_pairwise > 0.0f) {
        synapse->num_ltp_pairwise_events++;
        synapse->total_ltp_pairwise += dw_pairwise;
    }
    if (dw_triplet > 0.0f) {
        synapse->num_ltp_triplet_events++;
        synapse->total_ltp_triplet += dw_triplet;
    }

    /* Increment post-synaptic traces */
    synapse->o1_post += 1.0f;
    synapse->o2_post += 1.0f;

    /* Update last spike time */
    synapse->last_post_spike_time = spike_time;

    nimcp_spinlock_unlock(&synapse->lock);

    return actual_dw;
}

/* ============================================================================
 * Sleep Integration
 * ============================================================================ */

int triplet_stdp_set_sleep_state(triplet_stdp_synapse_t* synapse, sleep_state_t state) {
    /* WHAT: Update sleep state for modulation
     * WHY:  Sleep modulates trace time constants
     * HOW:  Store state, will be applied by sleep bridge during updates
     */
    if (!synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "triplet_stdp_set_sleep_state: synapse is NULL");
        NIMCP_LOGGING_ERROR("NULL synapse in set_sleep_state");
        return -1;
    }

    nimcp_spinlock_lock(&synapse->lock);
    synapse->current_sleep_state = state;
    nimcp_spinlock_unlock(&synapse->lock);

    return 0;
}

int triplet_stdp_connect_sleep_bridge(
    triplet_stdp_synapse_t* synapse,
    triplet_stdp_sleep_bridge_t sleep_bridge
) {
    /* WHAT: Link synapse to sleep modulation system
     * WHY:  Enable sleep-dependent plasticity changes
     * HOW:  Register with sleep bridge (implementation in bridge)
     */
    if (!synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "triplet_stdp_connect_sleep_bridge: synapse is NULL");
        NIMCP_LOGGING_ERROR("NULL pointer in connect_sleep_bridge");
        return -1;
    }
    if (!sleep_bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "triplet_stdp_connect_sleep_bridge: sleep_bridge is NULL");
        NIMCP_LOGGING_ERROR("NULL pointer in connect_sleep_bridge");
        return -1;
    }

    /* Bridge will handle registration */
    NIMCP_LOGGING_DEBUG("Connected triplet STDP synapse to sleep bridge");
    return 0;
}

/* ============================================================================
 * Immune Integration
 * ============================================================================ */

int triplet_stdp_connect_immune_bridge(
    triplet_stdp_synapse_t* synapse,
    triplet_stdp_immune_bridge_t immune_bridge
) {
    /* WHAT: Link synapse to immune modulation system
     * WHY:  Enable inflammation-dependent plasticity changes
     * HOW:  Register with immune bridge (implementation in bridge)
     */
    if (!synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "triplet_stdp_connect_immune_bridge: synapse is NULL");
        NIMCP_LOGGING_ERROR("NULL pointer in connect_immune_bridge");
        return -1;
    }
    if (!immune_bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "triplet_stdp_connect_immune_bridge: immune_bridge is NULL");
        NIMCP_LOGGING_ERROR("NULL pointer in connect_immune_bridge");
        return -1;
    }

    /* Bridge will handle registration */
    NIMCP_LOGGING_DEBUG("Connected triplet STDP synapse to immune bridge");
    return 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int triplet_stdp_register_callback(
    triplet_stdp_synapse_t* synapse,
    triplet_stdp_callback_t callback,
    void* user_data
) {
    /* WHAT: Register callback for plasticity events
     * WHY:  Allow external monitoring and custom responses
     * HOW:  Store callback, invoke on events (future implementation)
     */
    if (!synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "triplet_stdp_register_callback: synapse is NULL");
        NIMCP_LOGGING_ERROR("NULL synapse in register_callback");
        return -1;
    }
    if (!callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "triplet_stdp_register_callback: callback is NULL");
        return -1;
    }

    (void)callback;    /* To be implemented */
    (void)user_data;

    NIMCP_LOGGING_DEBUG("Registered callback for triplet STDP events");
    return 0;
}

/* ============================================================================
 * Query Functions
 * ============================================================================ */

float triplet_stdp_get_weight(const triplet_stdp_synapse_t* synapse) {
    if (!synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "triplet_stdp_get_weight: synapse is NULL");
        return -1.0f;
    }
    return synapse->weight;
}

float triplet_stdp_get_r1_pre(const triplet_stdp_synapse_t* synapse) {
    if (!synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "triplet_stdp_get_r1_pre: synapse is NULL");
        return -1.0f;
    }
    return synapse->r1_pre;
}

float triplet_stdp_get_r2_pre(const triplet_stdp_synapse_t* synapse) {
    if (!synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "triplet_stdp_get_r2_pre: synapse is NULL");
        return -1.0f;
    }
    return synapse->r2_pre;
}

float triplet_stdp_get_o1_post(const triplet_stdp_synapse_t* synapse) {
    if (!synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "triplet_stdp_get_o1_post: synapse is NULL");
        return -1.0f;
    }
    return synapse->o1_post;
}

float triplet_stdp_get_o2_post(const triplet_stdp_synapse_t* synapse) {
    if (!synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "triplet_stdp_get_o2_post: synapse is NULL");
        return -1.0f;
    }
    return synapse->o2_post;
}

float triplet_stdp_get_total_ltp(const triplet_stdp_synapse_t* synapse) {
    if (!synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "triplet_stdp_get_total_ltp: synapse is NULL");
        return -1.0f;
    }
    return synapse->total_ltp_pairwise + synapse->total_ltp_triplet;
}

float triplet_stdp_get_total_ltd(const triplet_stdp_synapse_t* synapse) {
    if (!synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "triplet_stdp_get_total_ltd: synapse is NULL");
        return -1.0f;
    }
    return synapse->total_ltd_pairwise + synapse->total_ltd_triplet;
}

void triplet_stdp_print_stats(const triplet_stdp_synapse_t* synapse) {
    /* WHAT: Display detailed synapse state
     * WHY:  Debugging and monitoring
     * HOW:  Print weight, traces, statistics
     */
    if (!synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "triplet_stdp_print_stats: synapse is NULL");
        printf("NULL synapse\n");
        return;
    }

    printf("Triplet STDP Synapse Statistics:\n");
    printf("  Weight: %.4f [%.4f, %.4f]\n", synapse->weight, synapse->w_min, synapse->w_max);
    printf("  Traces: r1=%.4f, r2=%.4f, o1=%.4f, o2=%.4f\n",
           synapse->r1_pre, synapse->r2_pre, synapse->o1_post, synapse->o2_post);
    printf("  LTP Pairwise: %lu events, total=%.6f\n",
           (unsigned long)synapse->num_ltp_pairwise_events, synapse->total_ltp_pairwise);
    printf("  LTP Triplet:  %lu events, total=%.6f\n",
           (unsigned long)synapse->num_ltp_triplet_events, synapse->total_ltp_triplet);
    printf("  LTD Pairwise: %lu events, total=%.6f\n",
           (unsigned long)synapse->num_ltd_pairwise_events, synapse->total_ltd_pairwise);
    printf("  LTD Triplet:  %lu events, total=%.6f\n",
           (unsigned long)synapse->num_ltd_triplet_events, synapse->total_ltd_triplet);
    printf("  Total LTP: %.6f, Total LTD: %.6f\n",
           synapse->total_ltp_pairwise + synapse->total_ltp_triplet,
           synapse->total_ltd_pairwise + synapse->total_ltd_triplet);
    printf("  Net change: %.6f\n",
           (synapse->total_ltp_pairwise + synapse->total_ltp_triplet) -
           (synapse->total_ltd_pairwise + synapse->total_ltd_triplet));
}
