/**
 * @file nimcp_triplet_stdp.h
 * @brief Triplet Spike-Timing-Dependent Plasticity (Pfister & Gerstner 2006)
 *
 * WHAT: Triplet-based STDP model with fast and slow spike traces
 * WHY:  Pairwise STDP fails to capture frequency-dependent plasticity and nonlinear
 *       interactions between spike triplets. Triplet model explains experimental
 *       data on frequency dependence and triplet interactions.
 * HOW:  Track two pre-synaptic traces (r1_pre: fast, r2_pre: slow) and two
 *       post-synaptic traces (o1_post: fast, o2_post: slow), implementing
 *       nearest-neighbor triplet interactions.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * TRIPLET STDP MODEL (Pfister & Gerstner, 2006):
 * -----------------------------------------------
 * Standard pairwise STDP:
 *   - dw_LTP = A2_plus * r1_pre(t_post)
 *   - dw_LTD = A2_minus * o1_post(t_pre)
 *
 * Triplet extension:
 *   - dw_LTP = A2_plus * r1_pre(t_post) + A3_plus * r2_pre(t_post) * o1_post(t_post)
 *   - dw_LTD = A2_minus * o1_post(t_pre) + A3_minus * r1_pre(t_pre) * o2_post(t_pre)
 *
 * Where:
 *   - r1_pre(t): Fast pre-synaptic trace (tau_plus ~ 16.8 ms)
 *   - r2_pre(t): Slow pre-synaptic trace (tau_x ~ 101 ms)
 *   - o1_post(t): Fast post-synaptic trace (tau_minus ~ 33.7 ms)
 *   - o2_post(t): Slow post-synaptic trace (tau_y ~ 125 ms)
 *   - A2_plus, A2_minus: Pairwise amplitude parameters
 *   - A3_plus, A3_minus: Triplet amplitude parameters
 *
 * FREQUENCY DEPENDENCE:
 * ---------------------
 * The triplet model captures frequency-dependent plasticity:
 *   - Low frequency (< 10 Hz): Pairwise dominates
 *   - High frequency (> 40 Hz): Triplet terms amplify LTP
 *   - Quadruplet+ interactions: Slow traces accumulate
 *
 * Reference: Pfister & Gerstner (2006) "Triplets of spikes in a model of
 *            spike timing-dependent plasticity", J. Neurosci. 26(38):9673-9682
 *
 * EXPERIMENTAL VALIDATION:
 * ------------------------
 * 1. Visual Cortex (Sjöström et al., 2001):
 *    - Frequency dependence: 10 Hz LTD, 40 Hz LTP
 *    - Triplet model fits data (R^2 > 0.9)
 *    - Pairwise model fails (R^2 < 0.5)
 *
 * 2. Hippocampus (Wang et al., 2005):
 *    - Triplet/quadruplet interactions
 *    - Nonlinear summation of spike pairs
 *    - Slow trace accumulation explains burst effects
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    TRIPLET STDP SYNAPSE                                    ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   PRE-SYNAPTIC TRACES:          POST-SYNAPTIC TRACES:                     ║
 * ║   ┌─────────────────┐           ┌─────────────────┐                       ║
 * ║   │ r1_pre (fast)   │           │ o1_post (fast)  │                       ║
 * ║   │ tau_plus=16.8ms │           │ tau_minus=33.7ms│                       ║
 * ║   └─────────────────┘           └─────────────────┘                       ║
 * ║   ┌─────────────────┐           ┌─────────────────┐                       ║
 * ║   │ r2_pre (slow)   │           │ o2_post (slow)  │                       ║
 * ║   │ tau_x=101ms     │           │ tau_y=125ms     │                       ║
 * ║   └─────────────────┘           └─────────────────┘                       ║
 * ║                                                                            ║
 * ║   WEIGHT UPDATE RULES:                                                    ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │ POST SPIKE:                                                         │  ║
 * ║   │   dw_LTP = A2_plus * r1_pre + A3_plus * r2_pre * o1_post          │  ║
 * ║   │            └─ pairwise ─┘     └───── triplet ──────┘               │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │ PRE SPIKE:                                                          │  ║
 * ║   │   dw_LTD = -A2_minus * o1_post - A3_minus * r1_pre * o2_post      │  ║
 * ║   │            └── pairwise ──┘      └───── triplet ──────┘            │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * NIMCP INTEGRATION:
 * - Sleep modulation: Trace time constants modulated by sleep state
 * - Immune modulation: Learning rates reduced by inflammation
 * - Bio-async: Event notifications for plasticity events
 * - Callbacks: Custom handlers for state changes
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 * @date 2025-12-19
 */

#ifndef NIMCP_TRIPLET_STDP_H
#define NIMCP_TRIPLET_STDP_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>

/* Thread safety - use spinlock for consistency with classic STDP */
#include "utils/thread/nimcp_thread.h"

/* Bio-async integration */
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

/* Sleep integration */
#include "cognitive/nimcp_sleep_wake.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/* Sleep bridge forward declaration */
struct triplet_stdp_sleep_bridge_struct;
typedef struct triplet_stdp_sleep_bridge_struct* triplet_stdp_sleep_bridge_t;

/* Immune bridge forward declaration */
struct triplet_stdp_immune_bridge_struct;
typedef struct triplet_stdp_immune_bridge_struct* triplet_stdp_immune_bridge_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Default parameters from Pfister & Gerstner (2006) - Visual Cortex */
#define TRIPLET_STDP_DEFAULT_A2_PLUS      0.005f     /**< Pairwise LTP amplitude */
#define TRIPLET_STDP_DEFAULT_A3_PLUS      0.0062f    /**< Triplet LTP amplitude */
#define TRIPLET_STDP_DEFAULT_A2_MINUS     0.007f     /**< Pairwise LTD amplitude */
#define TRIPLET_STDP_DEFAULT_A3_MINUS     0.00023f   /**< Triplet LTD amplitude */
#define TRIPLET_STDP_DEFAULT_TAU_PLUS     16.8f      /**< Fast pre-trace (ms) */
#define TRIPLET_STDP_DEFAULT_TAU_MINUS    33.7f      /**< Fast post-trace (ms) */
#define TRIPLET_STDP_DEFAULT_TAU_X        101.0f     /**< Slow pre-trace (ms) */
#define TRIPLET_STDP_DEFAULT_TAU_Y        125.0f     /**< Slow post-trace (ms) */
#define TRIPLET_STDP_DEFAULT_W_MAX        1.0f       /**< Maximum weight */
#define TRIPLET_STDP_DEFAULT_W_MIN        0.0f       /**< Minimum weight */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Triplet STDP event types for callbacks
 */
typedef enum {
    TRIPLET_STDP_EVENT_LTP_PAIRWISE = 0,    /**< Pairwise LTP occurred */
    TRIPLET_STDP_EVENT_LTP_TRIPLET,         /**< Triplet LTP occurred */
    TRIPLET_STDP_EVENT_LTD_PAIRWISE,        /**< Pairwise LTD occurred */
    TRIPLET_STDP_EVENT_LTD_TRIPLET,         /**< Triplet LTD occurred */
    TRIPLET_STDP_EVENT_WEIGHT_SATURATE_MAX, /**< Weight hit max limit */
    TRIPLET_STDP_EVENT_WEIGHT_SATURATE_MIN, /**< Weight hit min limit */
    TRIPLET_STDP_EVENT_COUNT                /**< Number of event types */
} triplet_stdp_event_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Triplet STDP synapse state
 *
 * WHAT: Per-synapse state for triplet STDP learning
 * WHY:  Track weight, traces, and statistics
 * HOW:  Maintain four exponential traces (r1, r2, o1, o2)
 */
typedef struct {
    /* Synaptic weight */
    float weight;               /**< Current weight [w_min, w_max] */
    float w_max;                /**< Maximum weight */
    float w_min;                /**< Minimum weight */

    /* Learning parameters */
    float A2_plus;              /**< Pairwise LTP amplitude */
    float A3_plus;              /**< Triplet LTP amplitude */
    float A2_minus;             /**< Pairwise LTD amplitude */
    float A3_minus;             /**< Triplet LTD amplitude */

    /* Time constants */
    float tau_plus;             /**< Fast pre-trace decay (ms) */
    float tau_minus;            /**< Fast post-trace decay (ms) */
    float tau_x;                /**< Slow pre-trace decay (ms) */
    float tau_y;                /**< Slow post-trace decay (ms) */

    /* Spike traces */
    float r1_pre;               /**< Fast pre-synaptic trace */
    float r2_pre;               /**< Slow pre-synaptic trace */
    float o1_post;              /**< Fast post-synaptic trace */
    float o2_post;              /**< Slow post-synaptic trace */

    /* Last spike times (for trace updates) */
    float last_pre_spike_time;  /**< Last pre-synaptic spike (ms) */
    float last_post_spike_time; /**< Last post-synaptic spike (ms) */

    /* Sleep state modulation */
    sleep_state_t current_sleep_state; /**< Current sleep/wake state */

    /* Statistics */
    uint64_t num_ltp_pairwise_events;   /**< Pairwise LTP count */
    uint64_t num_ltp_triplet_events;    /**< Triplet LTP count */
    uint64_t num_ltd_pairwise_events;   /**< Pairwise LTD count */
    uint64_t num_ltd_triplet_events;    /**< Triplet LTD count */
    float total_ltp_pairwise;           /**< Cumulative pairwise LTP */
    float total_ltp_triplet;            /**< Cumulative triplet LTP */
    float total_ltd_pairwise;           /**< Cumulative pairwise LTD */
    float total_ltd_triplet;            /**< Cumulative triplet LTD */

    /* Weight saturation tracking (for detecting potential learning issues) */
    uint64_t num_saturate_max_events;   /**< Times weight hit w_max limit */
    uint64_t num_saturate_min_events;   /**< Times weight hit w_min limit */

    /* Thread safety - use spinlock for short critical sections (consistent with classic STDP) */
    nimcp_spinlock_t lock;      /**< Spinlock for thread-safe updates */
} triplet_stdp_synapse_t;

/**
 * @brief Triplet STDP configuration
 *
 * WHAT: Initialization parameters for triplet STDP
 * WHY:  Different brain regions may use different parameters
 * HOW:  Factory functions provide presets (visual cortex, hippocampus)
 */
typedef struct {
    /* Learning amplitudes */
    float A2_plus;              /**< Pairwise LTP amplitude */
    float A3_plus;              /**< Triplet LTP amplitude */
    float A2_minus;             /**< Pairwise LTD amplitude */
    float A3_minus;             /**< Triplet LTD amplitude */

    /* Time constants */
    float tau_plus;             /**< Fast pre-trace decay (ms) */
    float tau_minus;            /**< Fast post-trace decay (ms) */
    float tau_x;                /**< Slow pre-trace decay (ms) */
    float tau_y;                /**< Slow post-trace decay (ms) */

    /* Weight bounds */
    float w_max;                /**< Maximum weight */
    float w_min;                /**< Minimum weight */

    /* Integration flags */
    bool enable_bio_async;      /**< Enable bio-async notifications */
    bool enable_sleep_modulation; /**< Enable sleep state modulation */
    bool enable_immune_modulation; /**< Enable immune system modulation */
} triplet_stdp_config_t;

/**
 * @brief Triplet STDP callback function type
 *
 * WHAT: User-defined callback for plasticity events
 * WHY:  Allow external monitoring and custom responses
 * HOW:  Invoked after each plasticity event
 *
 * @param synapse Synapse that changed
 * @param event Event type
 * @param weight_change Magnitude of weight change
 * @param user_data User-provided context
 */
typedef void (*triplet_stdp_callback_t)(
    triplet_stdp_synapse_t* synapse,
    triplet_stdp_event_t event,
    float weight_change,
    void* user_data
);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default triplet STDP configuration
 *
 * WHAT: Return Pfister & Gerstner (2006) visual cortex parameters
 * WHY:  Provide validated starting point
 * HOW:  Return struct with default constants
 *
 * @return Default configuration
 */
triplet_stdp_config_t triplet_stdp_config_default(void);

/**
 * @brief Get hippocampal triplet STDP configuration
 *
 * WHAT: Return hippocampus-specific parameters
 * WHY:  Hippocampus has different triplet dynamics than cortex
 * HOW:  Return struct with hippocampal constants
 *
 * @return Hippocampal configuration
 */
triplet_stdp_config_t triplet_stdp_config_hippocampal(void);

/**
 * @brief Initialize triplet STDP synapse
 *
 * WHAT: Create and initialize triplet STDP synapse
 * WHY:  Proper initialization with default parameters
 * HOW:  Allocate, set defaults, initialize mutex
 *
 * @param config Configuration (NULL for defaults)
 * @param initial_weight Starting weight [w_min, w_max]
 * @return New synapse or NULL on failure
 */
triplet_stdp_synapse_t* triplet_stdp_synapse_create(
    const triplet_stdp_config_t* config,
    float initial_weight
);

/**
 * @brief Destroy triplet STDP synapse
 *
 * WHAT: Free synapse resources
 * WHY:  Proper cleanup
 * HOW:  Destroy mutex, free memory
 *
 * @param synapse Synapse to destroy
 */
void triplet_stdp_synapse_destroy(triplet_stdp_synapse_t* synapse);

/**
 * @brief Reset synapse traces and statistics
 *
 * WHAT: Clear all traces and counters
 * WHY:  Restart learning from clean state
 * HOW:  Set traces to 0, reset counters, keep weight
 *
 * @param synapse Synapse to reset
 * @return 0 on success, -1 on error
 */
int triplet_stdp_synapse_reset(triplet_stdp_synapse_t* synapse);

/* ============================================================================
 * Core Plasticity API
 * ============================================================================ */

/**
 * @brief Update spike traces (exponential decay)
 *
 * WHAT: Decay all four traces by dt
 * WHY:  Traces decay exponentially between spikes
 * HOW:  r *= exp(-dt/tau) for each trace
 *
 * @param synapse Synapse to update
 * @param dt Time step (ms)
 * @return 0 on success, -1 on error
 */
int triplet_stdp_update_traces(triplet_stdp_synapse_t* synapse, float dt);

/**
 * @brief Process presynaptic spike
 *
 * WHAT: Handle pre-synaptic spike arrival
 * WHY:  Triggers LTD based on post-synaptic traces
 * HOW:  Compute dw_LTD = -A2_minus*o1 - A3_minus*r1*o2, update traces
 *
 * BIOLOGICAL: Pre-before-post → LTD (depression)
 *   - Pairwise term: A2_minus * o1_post(t_pre)
 *   - Triplet term: A3_minus * r1_pre(t_pre) * o2_post(t_pre)
 *
 * @param synapse Synapse to update
 * @param spike_time Current time (ms)
 * @return Weight change applied
 */
float triplet_stdp_pre_spike(triplet_stdp_synapse_t* synapse, float spike_time);

/**
 * @brief Process postsynaptic spike
 *
 * WHAT: Handle post-synaptic spike arrival
 * WHY:  Triggers LTP based on pre-synaptic traces
 * HOW:  Compute dw_LTP = A2_plus*r1 + A3_plus*r2*o1, update traces
 *
 * BIOLOGICAL: Post-after-pre → LTP (potentiation)
 *   - Pairwise term: A2_plus * r1_pre(t_post)
 *   - Triplet term: A3_plus * r2_pre(t_post) * o1_post(t_post)
 *
 * @param synapse Synapse to update
 * @param spike_time Current time (ms)
 * @return Weight change applied
 */
float triplet_stdp_post_spike(triplet_stdp_synapse_t* synapse, float spike_time);

/* ============================================================================
 * Sleep Integration API
 * ============================================================================ */

/**
 * @brief Set sleep state for synapse
 *
 * WHAT: Update sleep state for modulation
 * WHY:  Sleep modulates trace time constants
 * HOW:  Store state, applied during next update
 *
 * @param synapse Synapse to update
 * @param state New sleep state
 * @return 0 on success, -1 on error
 */
int triplet_stdp_set_sleep_state(triplet_stdp_synapse_t* synapse, sleep_state_t state);

/**
 * @brief Connect to sleep bridge
 *
 * WHAT: Link synapse to sleep modulation system
 * WHY:  Enable sleep-dependent plasticity changes
 * HOW:  Register with sleep bridge
 *
 * @param synapse Synapse to connect
 * @param sleep_bridge Sleep bridge instance
 * @return 0 on success, -1 on error
 */
int triplet_stdp_connect_sleep_bridge(
    triplet_stdp_synapse_t* synapse,
    triplet_stdp_sleep_bridge_t sleep_bridge
);

/* ============================================================================
 * Immune Integration API
 * ============================================================================ */

/**
 * @brief Connect to immune bridge
 *
 * WHAT: Link synapse to immune modulation system
 * WHY:  Enable inflammation-dependent plasticity changes
 * HOW:  Register with immune bridge
 *
 * @param synapse Synapse to connect
 * @param immune_bridge Immune bridge instance
 * @return 0 on success, -1 on error
 */
int triplet_stdp_connect_immune_bridge(
    triplet_stdp_synapse_t* synapse,
    triplet_stdp_immune_bridge_t immune_bridge
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Register callback for plasticity events
 *
 * WHAT: Set user callback for event notifications
 * WHY:  Allow external monitoring and custom logic
 * HOW:  Store callback, invoke on events
 *
 * @param synapse Synapse to monitor
 * @param callback Callback function
 * @param user_data User context passed to callback
 * @return 0 on success, -1 on error
 */
int triplet_stdp_register_callback(
    triplet_stdp_synapse_t* synapse,
    triplet_stdp_callback_t callback,
    void* user_data
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current weight
 *
 * @param synapse Synapse to query
 * @return Current weight or -1.0f on error
 */
float triplet_stdp_get_weight(const triplet_stdp_synapse_t* synapse);

/**
 * @brief Get fast pre-synaptic trace
 *
 * @param synapse Synapse to query
 * @return r1_pre value or -1.0f on error
 */
float triplet_stdp_get_r1_pre(const triplet_stdp_synapse_t* synapse);

/**
 * @brief Get slow pre-synaptic trace
 *
 * @param synapse Synapse to query
 * @return r2_pre value or -1.0f on error
 */
float triplet_stdp_get_r2_pre(const triplet_stdp_synapse_t* synapse);

/**
 * @brief Get fast post-synaptic trace
 *
 * @param synapse Synapse to query
 * @return o1_post value or -1.0f on error
 */
float triplet_stdp_get_o1_post(const triplet_stdp_synapse_t* synapse);

/**
 * @brief Get slow post-synaptic trace
 *
 * @param synapse Synapse to query
 * @return o2_post value or -1.0f on error
 */
float triplet_stdp_get_o2_post(const triplet_stdp_synapse_t* synapse);

/**
 * @brief Get total LTP (pairwise + triplet)
 *
 * @param synapse Synapse to query
 * @return Total cumulative LTP or -1.0f on error
 */
float triplet_stdp_get_total_ltp(const triplet_stdp_synapse_t* synapse);

/**
 * @brief Get total LTD (pairwise + triplet)
 *
 * @param synapse Synapse to query
 * @return Total cumulative LTD or -1.0f on error
 */
float triplet_stdp_get_total_ltd(const triplet_stdp_synapse_t* synapse);

/**
 * @brief Print synapse statistics
 *
 * WHAT: Display detailed synapse state
 * WHY:  Debugging and monitoring
 * HOW:  Print weight, traces, statistics
 *
 * @param synapse Synapse to print
 */
void triplet_stdp_print_stats(const triplet_stdp_synapse_t* synapse);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TRIPLET_STDP_H */
