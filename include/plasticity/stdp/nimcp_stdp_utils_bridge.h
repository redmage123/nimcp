/**
 * @file nimcp_stdp_utils_bridge.h
 * @brief STDP Utils Integration - Ring Buffer, Metrics, Memory Pools, Math
 * @version 1.0.0
 * @date 2026-01-12
 *
 * Integrates NIMCP utilities into STDP module for enhanced performance:
 * - Ring buffer for spike history (O(1) temporal queries)
 * - Metrics collection for learning analytics
 * - Memory pools for synapse allocation
 * - Numerical integration (RK4) for accurate trace decay
 * - Complex math for phase-gated STDP
 */

#ifndef NIMCP_STDP_UTILS_BRIDGE_H
#define NIMCP_STDP_UTILS_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "plasticity/stdp/nimcp_stdp.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define STDP_SPIKE_HISTORY_SIZE     256     /* Spike events per synapse group */
#define STDP_METRICS_BUFFER_SIZE    1000    /* Metrics buffer before flush */
#define STDP_SYNAPSE_POOL_SIZE      10000   /* Pre-allocated synapses */
#define STDP_TRACE_INTEGRATION_DT   0.001f  /* 1ms integration timestep */

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

/* Opaque handles for internal structures */
typedef struct stdp_utils_ctx_internal* stdp_utils_ctx_t;
typedef struct stdp_spike_buffer_internal* stdp_spike_buffer_t;
typedef struct stdp_synapse_pool_internal* stdp_synapse_pool_t;

/*=============================================================================
 * SPIKE EVENT STRUCTURE
 *===========================================================================*/

/**
 * @brief Spike event for ring buffer storage
 */
typedef struct {
    uint32_t source_id;         /**< Source neuron ID */
    uint32_t target_id;         /**< Target neuron ID */
    float timestamp;            /**< Spike time (ms) */
    float amplitude;            /**< Spike amplitude */
    bool is_pre;                /**< true=presynaptic, false=postsynaptic */
} stdp_spike_event_t;

/*=============================================================================
 * METRICS STRUCTURES
 *===========================================================================*/

/**
 * @brief STDP learning metrics
 */
typedef struct {
    /* Counters */
    uint64_t total_ltp_events;          /**< Long-term potentiation events */
    uint64_t total_ltd_events;          /**< Long-term depression events */
    uint64_t total_spikes_processed;    /**< Total spikes processed */

    /* Gauges */
    float mean_weight;                  /**< Average synaptic weight */
    float weight_variance;              /**< Weight distribution variance */
    float ltp_ltd_ratio;                /**< LTP/LTD balance */
    float mean_learning_rate;           /**< Average effective learning rate */
    float dopamine_sensitivity;         /**< DA modulation factor */

    /* Histograms (bin counts) */
    uint32_t weight_histogram[20];      /**< Weight distribution bins [0-1] */
    uint32_t timing_histogram[20];      /**< Spike timing delta bins */

    /* Timers */
    double update_latency_us;           /**< Avg spike→weight update latency */
    double batch_processing_time_ms;    /**< Batch processing time */

    /* Timestamp */
    uint64_t last_update_ms;            /**< Last metrics update time */
} stdp_metrics_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief STDP utils bridge configuration
 */
typedef struct {
    /* Ring buffer */
    uint32_t spike_history_size;        /**< Spike events per buffer */
    bool enable_spike_buffering;        /**< Enable spike ring buffer */

    /* Metrics */
    bool enable_metrics;                /**< Enable metrics collection */
    uint32_t metrics_flush_interval_ms; /**< Auto-flush interval */
    char metrics_output_dir[256];       /**< Metrics output directory */

    /* Memory pools */
    bool enable_synapse_pool;           /**< Enable synapse memory pool */
    uint32_t synapse_pool_size;         /**< Pre-allocated synapses */

    /* Numerical integration */
    bool use_rk4_trace_decay;           /**< Use RK4 instead of Euler for traces */
    float trace_integration_dt;         /**< Integration timestep (ms) */

    /* Phase-gated STDP */
    bool enable_phase_gating;           /**< Enable theta phase gating */
    float encoding_phase_start;         /**< Encoding window start (degrees) */
    float encoding_phase_end;           /**< Encoding window end (degrees) */
} stdp_utils_config_t;

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Get default configuration
 */
stdp_utils_config_t stdp_utils_default_config(void);

/**
 * @brief Create STDP utils context
 * @param config Configuration (NULL for defaults)
 * @return Utils context or NULL on failure
 */
stdp_utils_ctx_t stdp_utils_create(const stdp_utils_config_t* config);

/**
 * @brief Destroy STDP utils context
 * @param ctx Utils context
 */
void stdp_utils_destroy(stdp_utils_ctx_t ctx);

/**
 * @brief Reset utils context (clear buffers, reset metrics)
 * @param ctx Utils context
 */
void stdp_utils_reset(stdp_utils_ctx_t ctx);

/*=============================================================================
 * SPIKE BUFFER API (Ring Buffer Integration)
 *===========================================================================*/

/**
 * @brief Record spike event to ring buffer
 * @param ctx Utils context
 * @param event Spike event
 * @return true on success
 */
bool stdp_utils_record_spike(stdp_utils_ctx_t ctx, const stdp_spike_event_t* event);

/**
 * @brief Get spikes in time window [t_start, t_end]
 * @param ctx Utils context
 * @param t_start Window start (ms)
 * @param t_end Window end (ms)
 * @param out_events Output array (caller allocated)
 * @param max_events Maximum events to return
 * @param num_found Number of events found
 * @return true on success
 */
bool stdp_utils_get_spikes_in_window(
    stdp_utils_ctx_t ctx,
    float t_start,
    float t_end,
    stdp_spike_event_t* out_events,
    uint32_t max_events,
    uint32_t* num_found
);

/**
 * @brief Get most recent N spikes
 * @param ctx Utils context
 * @param n Number of spikes to get
 * @param out_events Output array
 * @param num_found Actual number returned
 * @return true on success
 */
bool stdp_utils_get_recent_spikes(
    stdp_utils_ctx_t ctx,
    uint32_t n,
    stdp_spike_event_t* out_events,
    uint32_t* num_found
);

/**
 * @brief Find spikes matching pre-post pair
 * @param ctx Utils context
 * @param pre_id Presynaptic neuron ID
 * @param post_id Postsynaptic neuron ID
 * @param time_window_ms Window around current time
 * @param out_events Output array
 * @param max_events Maximum to return
 * @param num_found Number found
 * @return true on success
 */
bool stdp_utils_find_spike_pairs(
    stdp_utils_ctx_t ctx,
    uint32_t pre_id,
    uint32_t post_id,
    float time_window_ms,
    stdp_spike_event_t* out_events,
    uint32_t max_events,
    uint32_t* num_found
);

/*=============================================================================
 * METRICS API
 *===========================================================================*/

/**
 * @brief Record LTP event
 * @param ctx Utils context
 * @param weight_change Amount of weight change
 * @param timing_delta Pre-post timing (ms)
 */
void stdp_utils_record_ltp(stdp_utils_ctx_t ctx, float weight_change, float timing_delta);

/**
 * @brief Record LTD event
 * @param ctx Utils context
 * @param weight_change Amount of weight change (negative)
 * @param timing_delta Pre-post timing (ms)
 */
void stdp_utils_record_ltd(stdp_utils_ctx_t ctx, float weight_change, float timing_delta);

/**
 * @brief Update weight statistics
 * @param ctx Utils context
 * @param weights Array of current weights
 * @param num_weights Number of weights
 */
void stdp_utils_update_weight_stats(
    stdp_utils_ctx_t ctx,
    const float* weights,
    uint32_t num_weights
);

/**
 * @brief Get current metrics snapshot
 * @param ctx Utils context
 * @param metrics Output metrics structure
 * @return true on success
 */
bool stdp_utils_get_metrics(stdp_utils_ctx_t ctx, stdp_metrics_t* metrics);

/**
 * @brief Flush metrics to disk
 * @param ctx Utils context
 * @return Number of records flushed
 */
int32_t stdp_utils_flush_metrics(stdp_utils_ctx_t ctx);

/**
 * @brief Export metrics to CSV (Tableau compatible)
 * @param ctx Utils context
 * @param filename Output filename
 * @return true on success
 */
bool stdp_utils_export_csv(stdp_utils_ctx_t ctx, const char* filename);

/**
 * @brief Export metrics to JSON (PowerBI compatible)
 * @param ctx Utils context
 * @param filename Output filename
 * @return true on success
 */
bool stdp_utils_export_json(stdp_utils_ctx_t ctx, const char* filename);

/*=============================================================================
 * MEMORY POOL API
 *===========================================================================*/

/**
 * @brief Allocate synapse from pool
 * @param ctx Utils context
 * @return Allocated synapse or NULL if pool exhausted
 */
stdp_synapse_t* stdp_utils_alloc_synapse(stdp_utils_ctx_t ctx);

/**
 * @brief Return synapse to pool
 * @param ctx Utils context
 * @param synapse Synapse to free
 */
void stdp_utils_free_synapse(stdp_utils_ctx_t ctx, stdp_synapse_t* synapse);

/**
 * @brief Allocate batch of synapses
 * @param ctx Utils context
 * @param count Number of synapses to allocate
 * @param out_synapses Output array of synapse pointers
 * @return Number actually allocated
 */
uint32_t stdp_utils_alloc_synapse_batch(
    stdp_utils_ctx_t ctx,
    uint32_t count,
    stdp_synapse_t** out_synapses
);

/**
 * @brief Get pool statistics
 * @param ctx Utils context
 * @param total_capacity Total pool capacity
 * @param used_count Currently allocated
 * @param free_count Available for allocation
 */
void stdp_utils_pool_stats(
    stdp_utils_ctx_t ctx,
    uint32_t* total_capacity,
    uint32_t* used_count,
    uint32_t* free_count
);

/*=============================================================================
 * NUMERICAL INTEGRATION API (RK4 Trace Decay)
 *===========================================================================*/

/**
 * @brief Update trace with RK4 integration (more accurate than Euler)
 * @param trace Current trace value (modified in-place)
 * @param tau Time constant (ms)
 * @param dt Time step (ms)
 * @param spike_contribution Spike contribution (typically 1.0)
 */
void stdp_utils_rk4_trace_update(
    float* trace,
    float tau,
    float dt,
    float spike_contribution
);

/**
 * @brief Batch update multiple traces with RK4
 * @param traces Array of trace values (modified in-place)
 * @param taus Array of time constants (can be single value broadcast)
 * @param num_traces Number of traces
 * @param dt Time step
 * @param spike_mask Bitmask of traces that received spikes (NULL = none)
 */
void stdp_utils_rk4_trace_batch(
    float* traces,
    const float* taus,
    uint32_t num_traces,
    float dt,
    const uint8_t* spike_mask
);

/*=============================================================================
 * PHASE-GATED STDP API (Complex Math Integration)
 *===========================================================================*/

/**
 * @brief Check if current theta phase is in encoding window
 * @param ctx Utils context
 * @param theta_phase Current theta phase (degrees, 0-360)
 * @return true if in encoding window
 */
bool stdp_utils_in_encoding_window(stdp_utils_ctx_t ctx, float theta_phase);

/**
 * @brief Get phase-dependent learning rate modulation
 * @param ctx Utils context
 * @param theta_phase Current theta phase (degrees)
 * @return Modulation factor [0, 1] (1.0 = full learning)
 */
float stdp_utils_phase_modulation(stdp_utils_ctx_t ctx, float theta_phase);

/**
 * @brief Compute phase-amplitude coupling index
 * @param theta_phases Array of theta phases
 * @param gamma_amplitudes Array of gamma amplitudes
 * @param num_samples Number of samples
 * @return Modulation index (0 = no coupling, 1 = perfect coupling)
 */
float stdp_utils_compute_pac(
    const float* theta_phases,
    const float* gamma_amplitudes,
    uint32_t num_samples
);

/**
 * @brief Extract instantaneous phase from signal (Hilbert transform)
 * @param signal Input signal
 * @param num_samples Number of samples
 * @param phases Output phases (radians)
 * @return true on success
 */
bool stdp_utils_extract_phase(
    const float* signal,
    uint32_t num_samples,
    float* phases
);

/*=============================================================================
 * ENHANCED STDP OPERATIONS (Combining All Utilities)
 *===========================================================================*/

/**
 * @brief Process presynaptic spike with full utils integration
 *
 * Combines: spike buffering, RK4 trace update, phase gating, metrics
 *
 * @param ctx Utils context
 * @param synapse STDP synapse
 * @param timestamp Current time (ms)
 * @param theta_phase Current theta phase (degrees, or -1 to disable)
 * @return Weight change applied
 */
float stdp_utils_pre_spike_enhanced(
    stdp_utils_ctx_t ctx,
    stdp_synapse_t* synapse,
    float timestamp,
    float theta_phase
);

/**
 * @brief Process postsynaptic spike with full utils integration
 *
 * @param ctx Utils context
 * @param synapse STDP synapse
 * @param timestamp Current time (ms)
 * @param theta_phase Current theta phase (degrees, or -1 to disable)
 * @return Weight change applied
 */
float stdp_utils_post_spike_enhanced(
    stdp_utils_ctx_t ctx,
    stdp_synapse_t* synapse,
    float timestamp,
    float theta_phase
);

/**
 * @brief Batch process spike events (optimized)
 *
 * @param ctx Utils context
 * @param synapses Array of synapses
 * @param events Array of spike events
 * @param num_events Number of events
 * @param theta_phase Current theta phase
 * @param weight_changes Output weight changes (can be NULL)
 * @return Number of events processed
 */
uint32_t stdp_utils_batch_process(
    stdp_utils_ctx_t ctx,
    stdp_synapse_t* synapses,
    const stdp_spike_event_t* events,
    uint32_t num_events,
    float theta_phase,
    float* weight_changes
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_STDP_UTILS_BRIDGE_H */
