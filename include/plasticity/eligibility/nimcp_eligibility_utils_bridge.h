/**
 * @file nimcp_eligibility_utils_bridge.h
 * @brief Eligibility Trace Utils Integration - Metrics, Memory Pools, RK4, Shannon
 * @version 1.0.0
 * @date 2026-01-12
 *
 * Integrates NIMCP utilities into eligibility trace module for enhanced performance:
 * - Metrics collection for credit assignment analytics
 * - Memory pools for trace allocation
 * - Numerical integration (RK4/Adaptive) for accurate trace decay
 * - Quantum-Shannon integration for bottleneck detection
 */

#ifndef NIMCP_ELIGIBILITY_UTILS_BRIDGE_H
#define NIMCP_ELIGIBILITY_UTILS_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "plasticity/eligibility/nimcp_eligibility_trace.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define ELIG_TRACE_POOL_SIZE        50000   /* Pre-allocated traces */
#define ELIG_METRICS_BUFFER_SIZE    5000    /* Metrics buffer */
#define ELIG_INTEGRATION_DT         0.001f  /* 1ms integration timestep */
#define ELIG_BOTTLENECK_THRESHOLD   0.5f    /* Information deficit threshold */

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

typedef struct eligibility_utils_ctx_internal* eligibility_utils_ctx_t;

/*=============================================================================
 * METRICS STRUCTURES
 *===========================================================================*/

/**
 * @brief Eligibility trace learning metrics
 */
typedef struct {
    /* Counters */
    uint64_t total_traces_updated;      /**< Total trace updates */
    uint64_t total_consolidations;      /**< Total consolidation events */
    uint64_t burst_triggered_count;     /**< Burst-triggered consolidations */
    uint64_t traces_above_threshold;    /**< Traces > threshold */

    /* Gauges */
    float mean_trace_value;             /**< Average trace magnitude */
    float max_trace_value;              /**< Maximum trace value */
    float mean_weight_change;           /**< Average weight update */
    float trace_decay_rate;             /**< Effective decay rate */
    float dopamine_sensitivity;         /**< DA modulation factor */

    /* Histograms */
    uint32_t trace_histogram[20];       /**< Trace value distribution [0-1] */
    uint32_t weight_change_histogram[20]; /**< Weight change distribution */

    /* Timing */
    double avg_update_latency_us;       /**< Trace update latency */
    double avg_consolidation_time_ms;   /**< Consolidation processing time */

    /* Information theory */
    float information_efficiency;       /**< Bits transmitted / bits available */
    uint32_t bottleneck_count;          /**< Number of detected bottlenecks */

    /* Timestamp */
    uint64_t last_update_ms;
} eligibility_metrics_t;

/**
 * @brief Bottleneck information for synapses
 */
typedef struct {
    uint32_t synapse_id;                /**< Synapse with bottleneck */
    float information_deficit;          /**< (demand - capacity) / demand */
    float suggested_weight;             /**< Recommended weight adjustment */
    float current_trace;                /**< Current trace value */
} eligibility_bottleneck_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Eligibility utils bridge configuration
 */
typedef struct {
    /* Metrics */
    bool enable_metrics;
    uint32_t metrics_flush_interval_ms;
    char metrics_output_dir[256];

    /* Memory pools */
    bool enable_trace_pool;
    uint32_t trace_pool_size;

    /* Numerical integration */
    bool use_adaptive_integration;      /**< Use adaptive RK45 vs fixed RK4 */
    float integration_dt;               /**< Fixed timestep for RK4 */
    float adaptive_error_tolerance;     /**< Error tolerance for adaptive */

    /* Information theory */
    bool enable_bottleneck_detection;
    float bottleneck_threshold;         /**< Deficit threshold for flagging */
    uint32_t bottleneck_check_interval; /**< Check every N updates */
} eligibility_utils_config_t;

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Get default configuration
 */
eligibility_utils_config_t eligibility_utils_default_config(void);

/**
 * @brief Create eligibility utils context
 */
eligibility_utils_ctx_t eligibility_utils_create(const eligibility_utils_config_t* config);

/**
 * @brief Destroy eligibility utils context
 */
void eligibility_utils_destroy(eligibility_utils_ctx_t ctx);

/**
 * @brief Reset utils context
 */
void eligibility_utils_reset(eligibility_utils_ctx_t ctx);

/*=============================================================================
 * MEMORY POOL API
 *===========================================================================*/

/**
 * @brief Allocate eligibility trace from pool
 */
eligibility_trace_t* eligibility_utils_alloc_trace(eligibility_utils_ctx_t ctx);

/**
 * @brief Free eligibility trace to pool
 */
void eligibility_utils_free_trace(eligibility_utils_ctx_t ctx, eligibility_trace_t* trace);

/**
 * @brief Batch allocate traces
 */
uint32_t eligibility_utils_alloc_trace_batch(
    eligibility_utils_ctx_t ctx,
    uint32_t count,
    eligibility_trace_t** out_traces
);

/**
 * @brief Get pool statistics
 */
void eligibility_utils_pool_stats(
    eligibility_utils_ctx_t ctx,
    uint32_t* total_capacity,
    uint32_t* used_count,
    uint32_t* free_count
);

/*=============================================================================
 * METRICS API
 *===========================================================================*/

/**
 * @brief Record trace update event
 */
void eligibility_utils_record_update(
    eligibility_utils_ctx_t ctx,
    float trace_value,
    float weight_change
);

/**
 * @brief Record consolidation event
 */
void eligibility_utils_record_consolidation(
    eligibility_utils_ctx_t ctx,
    uint32_t num_synapses,
    float total_weight_change,
    bool burst_triggered
);

/**
 * @brief Update trace statistics
 */
void eligibility_utils_update_trace_stats(
    eligibility_utils_ctx_t ctx,
    const eligibility_trace_t* traces,
    uint32_t num_traces
);

/**
 * @brief Get current metrics snapshot
 */
bool eligibility_utils_get_metrics(eligibility_utils_ctx_t ctx, eligibility_metrics_t* metrics);

/**
 * @brief Flush metrics to disk
 */
int32_t eligibility_utils_flush_metrics(eligibility_utils_ctx_t ctx);

/**
 * @brief Export metrics to CSV
 */
bool eligibility_utils_export_csv(eligibility_utils_ctx_t ctx, const char* filename);

/*=============================================================================
 * NUMERICAL INTEGRATION API
 *===========================================================================*/

/**
 * @brief Update trace with RK4 integration
 *
 * @param trace Trace to update (modified in-place)
 * @param config Eligibility configuration
 * @param dt Time elapsed since last update (ms)
 * @param spike_contribution Spike contribution (typically 1.0 or 0.0)
 */
void eligibility_utils_rk4_update(
    eligibility_trace_t* trace,
    const eligibility_config_t* config,
    float dt,
    float spike_contribution
);

/**
 * @brief Update trace with adaptive integration
 *
 * Automatically adjusts timestep for accuracy.
 *
 * @param trace Trace to update
 * @param config Eligibility configuration
 * @param dt_initial Initial timestep guess
 * @param spike_contribution Spike contribution
 * @param ctx Utils context (for error tolerance settings)
 * @return Actual timestep used
 */
float eligibility_utils_adaptive_update(
    eligibility_trace_t* trace,
    const eligibility_config_t* config,
    float dt_initial,
    float spike_contribution,
    eligibility_utils_ctx_t ctx
);

/**
 * @brief Batch update traces with optimal integration
 *
 * Automatically selects RK4 or adaptive based on trace dynamics.
 *
 * @param traces Array of traces
 * @param configs Array of configs (or single broadcast)
 * @param num_traces Number of traces
 * @param dt Time step
 * @param spike_mask Bitmask of spiking traces
 * @param ctx Utils context
 */
void eligibility_utils_batch_update(
    eligibility_trace_t* traces,
    const eligibility_config_t* configs,
    uint32_t num_traces,
    float dt,
    const uint8_t* spike_mask,
    eligibility_utils_ctx_t ctx
);

/*=============================================================================
 * INFORMATION THEORY API (Shannon-based Bottleneck Detection)
 *===========================================================================*/

/**
 * @brief Analyze information flow through synapses
 *
 * Uses Shannon entropy to detect capacity bottlenecks.
 *
 * @param ctx Utils context
 * @param traces Array of eligibility traces
 * @param weights Array of synaptic weights
 * @param num_synapses Number of synapses
 * @return Information efficiency [0, 1]
 */
float eligibility_utils_analyze_information_flow(
    eligibility_utils_ctx_t ctx,
    const eligibility_trace_t* traces,
    const float* weights,
    uint32_t num_synapses
);

/**
 * @brief Detect bottleneck synapses
 *
 * Identifies synapses where information capacity is limiting learning.
 *
 * @param ctx Utils context
 * @param traces Array of traces
 * @param weights Array of weights
 * @param num_synapses Number of synapses
 * @param bottlenecks Output bottleneck array
 * @param max_bottlenecks Maximum to return
 * @param num_found Number found
 * @return true on success
 */
bool eligibility_utils_detect_bottlenecks(
    eligibility_utils_ctx_t ctx,
    const eligibility_trace_t* traces,
    const float* weights,
    uint32_t num_synapses,
    eligibility_bottleneck_t* bottlenecks,
    uint32_t max_bottlenecks,
    uint32_t* num_found
);

/**
 * @brief Get suggested weight adjustments for bottlenecks
 *
 * @param ctx Utils context
 * @param bottlenecks Array of bottlenecks
 * @param num_bottlenecks Number of bottlenecks
 * @param weight_adjustments Output weight adjustments
 * @return true on success
 */
bool eligibility_utils_suggest_adjustments(
    eligibility_utils_ctx_t ctx,
    const eligibility_bottleneck_t* bottlenecks,
    uint32_t num_bottlenecks,
    float* weight_adjustments
);

/**
 * @brief Compute Shannon entropy of trace distribution
 *
 * @param traces Array of traces
 * @param num_traces Number of traces
 * @return Entropy in bits
 */
float eligibility_utils_compute_entropy(
    const eligibility_trace_t* traces,
    uint32_t num_traces
);

/*=============================================================================
 * ENHANCED ELIGIBILITY OPERATIONS
 *===========================================================================*/

/**
 * @brief Enhanced trace update with full utils integration
 *
 * Combines: RK4 integration, metrics, bottleneck detection
 *
 * @param ctx Utils context
 * @param trace Eligibility trace
 * @param config Eligibility configuration
 * @param current_time Current time (ms)
 * @param spike_contribution Spike contribution
 */
void eligibility_utils_update_enhanced(
    eligibility_utils_ctx_t ctx,
    eligibility_trace_t* trace,
    const eligibility_config_t* config,
    uint64_t current_time,
    float spike_contribution
);

/**
 * @brief Enhanced reward-based learning with metrics
 *
 * @param ctx Utils context
 * @param synapse Synapse to update (opaque pointer)
 * @param trace Eligibility trace
 * @param config Configuration
 * @param reward Reward signal
 * @param dopamine_level Dopamine concentration
 * @return Weight change applied
 */
float eligibility_utils_apply_reward_enhanced(
    eligibility_utils_ctx_t ctx,
    void* synapse,
    const eligibility_trace_t* trace,
    const eligibility_config_t* config,
    float reward,
    float dopamine_level
);

/**
 * @brief Enhanced burst consolidation with analytics
 *
 * @param ctx Utils context
 * @param synapses Array of synapses (opaque)
 * @param traces Array of traces
 * @param num_synapses Number of synapses
 * @param config Configuration
 * @param phasic_tonic Dopamine state
 * @param reward Reward signal
 * @return Number of synapses consolidated
 */
int eligibility_utils_consolidate_enhanced(
    eligibility_utils_ctx_t ctx,
    void* synapses,
    const eligibility_trace_t* traces,
    int num_synapses,
    const eligibility_config_t* config,
    const void* phasic_tonic,
    float reward
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ELIGIBILITY_UTILS_BRIDGE_H */
