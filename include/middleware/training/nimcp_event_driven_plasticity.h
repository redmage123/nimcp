/**
 * @file nimcp_event_driven_plasticity.h
 * @brief Event-Driven Plasticity Adapter for Continuous Learning
 *
 * Phase EDP-1: Connects Event Bus to Plasticity Bridge for Dynamic Learning
 *
 * ARCHITECTURE:
 * +------------------------------------------------------------------+
 * |                  Event-Driven Plasticity Adapter                  |
 * +------------------------------------------------------------------+
 * |  Event Bus    |  Learning Signal  |  Plasticity    |  Security   |
 * |  Subscriber   |  Processor        |  Bridge        |  Integration|
 * +------------------------------------------------------------------+
 * |                     Sensory/Cognitive Events                      |
 * |  (Spike Bursts)  (Patterns)  (Errors)  (Rewards)  (Novelty)      |
 * +------------------------------------------------------------------+
 *
 * WHAT: Enables continuous learning from sensory/cognitive events
 * WHY:  Brain learns from experience, not just static datasets
 * HOW:  Subscribe to event bus → process events → trigger plasticity
 *
 * BIOLOGICAL BASIS:
 * - Visual cortex: learns through spike-timing correlations (STDP)
 * - Reward system: learns from prediction errors (dopamine)
 * - Hippocampus: learns from novelty and pattern completion (BCM)
 * - Cerebellum: learns from motor errors (supervised)
 *
 * THREAD SAFETY: All functions are thread-safe
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 */

#ifndef NIMCP_EVENT_DRIVEN_PLASTICITY_H
#define NIMCP_EVENT_DRIVEN_PLASTICITY_H

#include "middleware/training/nimcp_training_plasticity_bridge.h"
#include "middleware/training/nimcp_learning_signal_adapter.h"
#include "core/events/nimcp_event_bus.h"
#include "security/nimcp_security_integration.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/validation/nimcp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of event subscriptions */
#define EDP_MAX_SUBSCRIPTIONS 16

/** Maximum spike buffer size for STDP processing */
#define EDP_SPIKE_BUFFER_SIZE 4096

/** Maximum eligibility trace entries */
#define EDP_MAX_ELIGIBILITY_ENTRIES 8192

/** Default eligibility trace decay time constant (ms) */
#define EDP_DEFAULT_ELIGIBILITY_TAU 20.0f

/** Default learning signal integration window (ms) */
#define EDP_DEFAULT_INTEGRATION_WINDOW 50.0f

/** Security module name */
#define EDP_SECURITY_MODULE_NAME "EventDrivenPlasticity"

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Event processing modes
 *
 * WHAT: How events are processed for learning
 * WHY:  Different processing strategies for different use cases
 */
typedef enum {
    EDP_MODE_IMMEDIATE = 0,       /**< Process events immediately (lowest latency) */
    EDP_MODE_BATCHED,             /**< Batch events for efficiency */
    EDP_MODE_ASYNC,               /**< Process in background thread */
    EDP_MODE_HYBRID               /**< Immediate for critical, batched for others */
} edp_processing_mode_t;

/**
 * @brief Learning event categories
 *
 * WHAT: Categories of events that trigger learning
 * WHY:  Different events require different plasticity responses
 */
typedef enum {
    EDP_CATEGORY_SPIKE = 0,       /**< Spike timing events → STDP */
    EDP_CATEGORY_PATTERN,         /**< Pattern detection → associative learning */
    EDP_CATEGORY_ERROR,           /**< Prediction errors → supervised learning */
    EDP_CATEGORY_REWARD,          /**< Reward signals → reinforcement learning */
    EDP_CATEGORY_NOVELTY,         /**< Novelty/surprise → curiosity learning */
    EDP_CATEGORY_ATTENTION,       /**< Attention shifts → gating */
    EDP_CATEGORY_COUNT
} edp_event_category_t;

/**
 * @brief Spike timing relationship
 */
typedef enum {
    EDP_SPIKE_PRE_BEFORE_POST = 0, /**< Pre fires before post → LTP */
    EDP_SPIKE_POST_BEFORE_PRE,     /**< Post fires before pre → LTD */
    EDP_SPIKE_SYNCHRONOUS,         /**< Nearly simultaneous */
    EDP_SPIKE_UNCORRELATED         /**< No timing relationship */
} edp_spike_timing_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Spike event record for STDP processing
 */
typedef struct {
    uint32_t neuron_id;           /**< Neuron that spiked */
    uint32_t region_id;           /**< Brain region */
    uint64_t timestamp_ns;        /**< Spike timestamp (nanoseconds) */
    float amplitude;              /**< Spike amplitude/strength */
    bool is_presynaptic;          /**< True if presynaptic spike */
} edp_spike_record_t;

/**
 * @brief Spike burst data for synchronized population activity
 */
typedef struct {
    uint32_t* neuron_ids;         /**< Array of firing neuron IDs */
    uint32_t num_neurons;         /**< Number of neurons in burst */
    uint64_t timestamp_ns;        /**< Burst onset timestamp */
    float synchrony_score;        /**< Population synchrony [0,1] */
    uint32_t region_id;           /**< Brain region ID */
} spike_burst_data_t;

/**
 * @brief Eligibility trace entry
 */
typedef struct {
    uint32_t pre_neuron;          /**< Presynaptic neuron ID */
    uint32_t post_neuron;         /**< Postsynaptic neuron ID */
    float eligibility;            /**< Current eligibility value [0,1] */
    uint64_t last_update_ns;      /**< Last update timestamp */
    float accumulated_delta;      /**< Accumulated weight change (waiting for reward) */
} edp_eligibility_entry_t;

/**
 * @brief Per-category learning statistics
 */
typedef struct {
    uint64_t events_received;     /**< Total events received */
    uint64_t events_processed;    /**< Events successfully processed */
    uint64_t plasticity_updates;  /**< Plasticity updates triggered */
    float total_weight_change;    /**< Cumulative absolute weight change */
    uint64_t processing_time_ns;  /**< Total processing time */
} edp_category_stats_t;

/**
 * @brief Event-Driven Plasticity statistics
 */
typedef struct {
    /* Overall stats */
    uint64_t total_events_received;
    uint64_t total_events_processed;
    uint64_t total_plasticity_updates;
    uint64_t total_processing_time_ns;

    /* Per-category stats */
    edp_category_stats_t category_stats[EDP_CATEGORY_COUNT];

    /* Spike processing stats */
    uint64_t spike_pairs_evaluated;
    uint64_t ltp_events;           /**< Long-term potentiation */
    uint64_t ltd_events;           /**< Long-term depression */

    /* Eligibility trace stats */
    uint32_t active_eligibility_traces;
    uint64_t eligibility_consolidations; /**< Traces consolidated with reward */

    /* Error/reward stats */
    float avg_prediction_error;
    float avg_reward_signal;
    float cumulative_reward;
    float last_batch_loss;         /**< Last batch loss for reward computation */

    /* Performance metrics */
    float events_per_second;
    float avg_latency_us;
    uint64_t dropped_events;       /**< Events dropped due to overflow */
} edp_stats_t;

/**
 * @brief Event-Driven Plasticity configuration
 */
typedef struct {
    /* Processing mode */
    edp_processing_mode_t mode;
    uint32_t batch_size;           /**< Batch size for batched mode */
    uint32_t batch_timeout_ms;     /**< Max wait time for batch fill */

    /* Spike timing parameters */
    float stdp_window_ms;          /**< STDP time window (±ms) */
    float ltp_rate;                /**< LTP learning rate */
    float ltd_rate;                /**< LTD learning rate */
    float spike_threshold;         /**< Min amplitude to process */

    /* Eligibility traces */
    bool enable_eligibility;       /**< Enable eligibility traces */
    float eligibility_tau_ms;      /**< Eligibility decay time constant */
    float eligibility_threshold;   /**< Min eligibility to process */

    /* Learning signals */
    float error_gain;              /**< Gain for error signals */
    float reward_gain;             /**< Gain for reward signals */
    float novelty_gain;            /**< Gain for novelty signals */

    /* Event filtering */
    bool filter_by_region;         /**< Only process events from configured regions */
    uint32_t* region_filter;       /**< List of region IDs to process */
    uint32_t num_region_filters;   /**< Number of region filters */

    /* Memory management */
    bool use_memory_pool;          /**< Use unified memory manager */
    unified_mem_manager_t memory_mgr; /**< Memory manager (optional) */

    /* Security integration */
    bool enable_security;          /**< Register with security system */
    nimcp_sec_integration_t* security_ctx; /**< Security context (optional) */

    /* Thread safety */
    bool enable_async_processing;  /**< Enable async processing thread */
    uint32_t async_queue_size;     /**< Async processing queue size */
} edp_config_t;

/**
 * @brief Event-Driven Plasticity context (opaque)
 */
typedef struct edp_context edp_context_t;

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Get default EDP configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Easy setup for common use cases
 *
 * @return Default configuration
 */
NIMCP_EXPORT edp_config_t edp_config_default(void);

/**
 * @brief Get high-performance EDP configuration
 *
 * WHAT: Configuration optimized for throughput
 * WHY:  For high event rate scenarios
 *
 * @return High-performance configuration
 */
NIMCP_EXPORT edp_config_t edp_config_high_performance(void);

/**
 * @brief Get biologically-accurate EDP configuration
 *
 * WHAT: Configuration with realistic biological parameters
 * WHY:  For neuroscience research applications
 *
 * @return Biologically-accurate configuration
 */
NIMCP_EXPORT edp_config_t edp_config_biological(void);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create Event-Driven Plasticity adapter
 *
 * WHAT: Create and initialize EDP context
 * WHY:  Prepare for event-driven learning
 * HOW:  Allocate buffers, initialize processors
 *
 * @param config Configuration (NULL for defaults)
 * @return EDP context or NULL on failure
 *
 * THREAD SAFETY: Thread-safe
 * COMPLEXITY: O(n) where n = buffer sizes
 */
NIMCP_EXPORT edp_context_t* edp_create(const edp_config_t* config);

/**
 * @brief Connect to plasticity bridge
 *
 * WHAT: Link EDP to the plasticity bridge for weight updates
 * WHY:  Events need to trigger actual synaptic changes
 * HOW:  Store reference, configure routing
 *
 * @param ctx EDP context
 * @param bridge Plasticity bridge context
 * @return NIMCP_SUCCESS or error code
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT nimcp_result_t edp_connect_bridge(
    edp_context_t* ctx,
    tpb_context_t* bridge
);

/**
 * @brief Connect to event bus
 *
 * WHAT: Subscribe to event bus for learning events
 * WHY:  Receive sensory/cognitive events for learning
 * HOW:  Subscribe with type filters
 *
 * @param ctx EDP context
 * @param bus Event bus to subscribe to
 * @return NIMCP_SUCCESS or error code
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT nimcp_result_t edp_connect_event_bus(
    edp_context_t* ctx,
    event_bus_t bus
);

/**
 * @brief Connect to learning signal adapter
 *
 * WHAT: Link to learning signal computation
 * WHY:  Compute prediction errors, rewards, novelty
 * HOW:  Store reference for signal queries
 *
 * @param ctx EDP context
 * @param adapter Learning signal adapter
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t edp_connect_learning_signals(
    edp_context_t* ctx,
    learning_signal_adapter_t adapter
);

/**
 * @brief Start event processing
 *
 * WHAT: Begin processing events for learning
 * WHY:  Enable continuous learning
 * HOW:  Start async thread if configured, enable callbacks
 *
 * @param ctx EDP context
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t edp_start(edp_context_t* ctx);

/**
 * @brief Stop event processing
 *
 * WHAT: Stop processing events
 * WHY:  Pause learning or prepare for shutdown
 * HOW:  Stop async thread, disable callbacks
 *
 * @param ctx EDP context
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t edp_stop(edp_context_t* ctx);

/**
 * @brief Destroy EDP context
 *
 * WHAT: Clean up all resources
 * WHY:  Proper resource management
 * HOW:  Unsubscribe, free buffers, stop threads
 *
 * @param ctx EDP context (NULL is safe)
 */
NIMCP_EXPORT void edp_destroy(edp_context_t* ctx);

//=============================================================================
// Event Processing
//=============================================================================

/**
 * @brief Process a single event
 *
 * WHAT: Process an event for learning
 * WHY:  Manual event injection for testing or external sources
 * HOW:  Categorize event, apply appropriate plasticity
 *
 * @param ctx EDP context
 * @param event Event to process
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t edp_process_event(
    edp_context_t* ctx,
    const brain_event_t* event
);

/**
 * @brief Process spike burst event
 *
 * WHAT: Handle coordinated spike activity
 * WHY:  Spike timing drives STDP
 * HOW:  Extract spike pairs, compute timing, apply STDP
 *
 * @param ctx EDP context
 * @param burst Spike burst data
 * @param region_id Brain region ID
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t edp_process_spike_burst(
    edp_context_t* ctx,
    const spike_burst_data_t* burst,
    uint32_t region_id
);

/**
 * @brief Process prediction error event
 *
 * WHAT: Handle prediction error for supervised learning
 * WHY:  Errors drive cerebellar/cortical learning
 * HOW:  Convert error to learning signal, apply updates
 *
 * @param ctx EDP context
 * @param prediction_error Error magnitude and direction
 * @param region_id Brain region ID
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t edp_process_prediction_error(
    edp_context_t* ctx,
    float prediction_error,
    uint32_t region_id
);

/**
 * @brief Process reward signal
 *
 * WHAT: Handle reward/punishment signal
 * WHY:  Rewards modulate learning via dopamine
 * HOW:  Update dopamine, consolidate eligibility traces
 *
 * @param ctx EDP context
 * @param reward_signal Reward magnitude [-1, 1]
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t edp_process_reward(
    edp_context_t* ctx,
    float reward_signal
);

/**
 * @brief Process novelty signal
 *
 * WHAT: Handle novel/surprising stimulus
 * WHY:  Novelty drives curiosity-based learning
 * HOW:  Boost learning rate, enhance attention
 *
 * @param ctx EDP context
 * @param novelty_score Novelty magnitude [0, 1]
 * @param region_id Target region for enhanced learning
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t edp_process_novelty(
    edp_context_t* ctx,
    float novelty_score,
    uint32_t region_id
);

//=============================================================================
// Eligibility Traces
//=============================================================================

/**
 * @brief Update eligibility traces
 *
 * WHAT: Decay and maintain eligibility traces
 * WHY:  Temporal credit assignment for delayed rewards
 * HOW:  Exponential decay of all active traces
 *
 * @param ctx EDP context
 * @param dt Time delta in milliseconds
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t edp_update_eligibility(
    edp_context_t* ctx,
    float dt
);

/**
 * @brief Consolidate eligibility traces with reward
 *
 * WHAT: Apply accumulated changes based on reward
 * WHY:  Three-factor learning: activity × eligibility × reward
 * HOW:  For each trace: weight += eligibility × reward × accumulated_delta
 *
 * @param ctx EDP context
 * @param reward Reward signal
 * @return Number of traces consolidated
 */
NIMCP_EXPORT uint32_t edp_consolidate_eligibility(
    edp_context_t* ctx,
    float reward
);

/**
 * @brief Clear all eligibility traces
 *
 * WHAT: Reset all eligibility traces to zero
 * WHY:  Episode boundary or error condition
 *
 * @param ctx EDP context
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t edp_clear_eligibility(edp_context_t* ctx);

//=============================================================================
// Statistics and Monitoring
//=============================================================================

/**
 * @brief Get EDP statistics
 *
 * @param ctx EDP context
 * @param stats Output statistics
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t edp_get_stats(
    const edp_context_t* ctx,
    edp_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param ctx EDP context
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t edp_reset_stats(edp_context_t* ctx);

/**
 * @brief Print status summary
 *
 * @param ctx EDP context
 */
NIMCP_EXPORT void edp_print_status(const edp_context_t* ctx);

/**
 * @brief Check if EDP is active
 *
 * @param ctx EDP context
 * @return true if actively processing events
 */
NIMCP_EXPORT bool edp_is_active(const edp_context_t* ctx);

//=============================================================================
// Security Integration
//=============================================================================

/**
 * @brief Register with security system
 *
 * WHAT: Register EDP module with security integration
 * WHY:  Enable monitoring, trust tracking, anomaly detection
 *
 * @param ctx EDP context
 * @param security_ctx Security integration context
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t edp_register_security(
    edp_context_t* ctx,
    nimcp_sec_integration_t* security_ctx
);

/**
 * @brief Unregister from security system
 *
 * @param ctx EDP context
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t edp_unregister_security(edp_context_t* ctx);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get event category name
 *
 * @param category Event category
 * @return String name
 */
NIMCP_EXPORT const char* edp_category_name(edp_event_category_t category);

/**
 * @brief Get processing mode name
 *
 * @param mode Processing mode
 * @return String name
 */
NIMCP_EXPORT const char* edp_mode_name(edp_processing_mode_t mode);

/**
 * @brief Validate EDP configuration
 *
 * @param config Configuration to validate
 * @return NIMCP_SUCCESS if valid
 */
NIMCP_EXPORT nimcp_result_t edp_validate_config(const edp_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EVENT_DRIVEN_PLASTICITY_H */
