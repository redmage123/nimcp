/**
 * @file nimcp_eligibility_quantum_bridge.h
 * @brief Eligibility Trace Quantum Integration - Shannon Bottleneck Detection + QMC
 * @version 1.0.0
 * @date 2026-01-12
 *
 * Integrates quantum algorithms with eligibility trace module:
 * - Quantum-Shannon for information flow bottleneck detection
 * - Quantum Monte Carlo for credit assignment estimation
 * - Quantum annealing for learning rate optimization
 * - Quantum walk for temporal credit diffusion
 *
 * QUANTUM APPLICATIONS:
 * - Bottleneck Detection: Shannon capacity analysis identifies learning bottlenecks
 * - Credit Assignment: QMC estimates optimal credit distribution
 * - Parameter Optimization: Quantum annealing finds optimal trace parameters
 * - Temporal Diffusion: Quantum walk spreads eligibility signal sqrt(N) faster
 *
 * BIOLOGICAL ANALOGY:
 * - Synaptic tagging and capture mechanisms
 * - Dopamine-gated retrograde signaling
 * - Activity-dependent structural plasticity
 * - Metaplasticity regulation
 *
 * MATHEMATICAL FOUNDATION:
 * - Shannon capacity: C = B * log2(1 + SNR) bits/second
 * - Information bottleneck: I(X;T) - beta * I(T;Y) minimization
 * - Quantum walk: O(sqrt(N)) diffusion speedup
 * - Quantum annealing: Escape local minima in parameter space
 */

#ifndef NIMCP_ELIGIBILITY_QUANTUM_BRIDGE_H
#define NIMCP_ELIGIBILITY_QUANTUM_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "plasticity/eligibility/nimcp_eligibility_trace.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define ELIG_QUANTUM_BOTTLENECK_THRESHOLD   0.5f    /* Information deficit threshold */
#define ELIG_QUANTUM_MC_SAMPLES             1000    /* Monte Carlo samples */
#define ELIG_QUANTUM_WALK_STEPS             50      /* Quantum walk evolution steps */
#define ELIG_QUANTUM_ANNEAL_ITERATIONS      200     /* Annealing iterations */

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

typedef struct eligibility_quantum_ctx_internal* eligibility_quantum_ctx_t;

/*=============================================================================
 * BOTTLENECK STRUCTURES
 *===========================================================================*/

/**
 * @brief Information bottleneck for eligibility traces
 */
typedef struct {
    uint32_t synapse_id;                /**< Synapse with bottleneck */
    float information_deficit;          /**< (demand - capacity) / demand */
    float current_trace;                /**< Current eligibility trace value */
    float effective_learning_rate;      /**< Effective learning rate */
    float capacity;                     /**< Channel capacity bits/s */
    float demand;                       /**< Information demand bits/s */
    float suggested_trace_tau;          /**< Recommended trace time constant */
    float suggested_learning_rate;      /**< Recommended learning rate */
} elig_quantum_bottleneck_t;

/**
 * @brief Credit assignment result from quantum analysis
 */
typedef struct {
    uint32_t synapse_id;                /**< Synapse ID */
    float credit_fraction;              /**< Fraction of reward credit [0,1] */
    float confidence;                   /**< Confidence in assignment */
    float temporal_weight;              /**< Temporal contribution factor */
    float causal_strength;              /**< Causal relationship strength */
} elig_quantum_credit_t;

/*=============================================================================
 * QUANTUM OPTIMIZATION STRUCTURES
 *===========================================================================*/

/**
 * @brief Quantum-optimized eligibility parameters
 */
typedef struct {
    float tau_fast;                     /**< Fast trace time constant */
    float tau_slow;                     /**< Slow trace time constant */
    float learning_rate;                /**< Base learning rate */
    float dopamine_sensitivity;         /**< DA modulation factor */
    float burst_threshold;              /**< Burst detection threshold */
    float consolidation_threshold;      /**< Consolidation threshold */
    float energy;                       /**< Objective function value */
    float amplitude;                    /**< Quantum amplitude */
} elig_quantum_params_t;

/**
 * @brief Quantum annealing state for eligibility
 */
typedef struct {
    float temperature;                  /**< Current temperature */
    float tunneling_probability;        /**< Tunneling probability */
    uint32_t iteration;                 /**< Current iteration */
    uint32_t tunneling_events;          /**< Total tunneling events */
    float best_energy;                  /**< Best energy found */
} elig_quantum_anneal_state_t;

/*=============================================================================
 * METRICS STRUCTURES
 *===========================================================================*/

/**
 * @brief Quantum bridge comprehensive metrics
 */
typedef struct {
    /* Bottleneck detection */
    uint64_t bottleneck_analyses;       /**< Total bottleneck analyses */
    uint32_t total_bottlenecks_found;   /**< Total bottlenecks detected */
    float avg_bottleneck_severity;      /**< Average deficit ratio */
    float information_efficiency;       /**< Current info efficiency [0,1] */

    /* Credit assignment */
    uint64_t credit_assignments;        /**< Total credit assignments */
    float avg_assignment_confidence;    /**< Average confidence */
    float credit_distribution_entropy;  /**< Entropy of credit distribution */

    /* Quantum optimization */
    uint64_t optimization_steps;        /**< Total optimization steps */
    uint64_t tunneling_events;          /**< Total tunneling events */
    float current_temperature;          /**< Current annealing temperature */
    float best_objective_value;         /**< Best objective found */

    /* Monte Carlo */
    uint64_t mc_samples_total;          /**< Total MC samples */
    float avg_mc_variance;              /**< Average estimation variance */

    /* Quantum walk */
    uint64_t diffusion_operations;      /**< Total diffusion operations */
    float avg_diffusion_speedup;        /**< Average speedup achieved */

    /* Performance */
    double total_processing_time_ms;    /**< Total processing time */
    double avg_analysis_latency_us;     /**< Average analysis latency */

    /* Timestamp */
    uint64_t last_update_ms;
} elig_quantum_metrics_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Eligibility quantum bridge configuration
 */
typedef struct {
    /* Feature toggles */
    bool enable_bottleneck_detection;
    bool enable_credit_assignment;
    bool enable_quantum_optimization;
    bool enable_quantum_walk;

    /* Bottleneck detection */
    float bottleneck_threshold;         /**< Deficit threshold for flagging */
    uint32_t bottleneck_check_interval; /**< Check every N updates */
    uint32_t synapse_sample_size;       /**< Synapses to sample */

    /* Credit assignment */
    uint32_t mc_samples;                /**< Monte Carlo samples */
    bool use_importance_sampling;       /**< Use importance sampling */
    float temporal_discount;            /**< Temporal discounting factor */

    /* Quantum optimization */
    float initial_temperature;          /**< Starting temperature */
    float final_temperature;            /**< Ending temperature */
    float tunneling_rate;               /**< Initial tunneling probability */
    uint32_t anneal_iterations;         /**< Annealing iterations per step */
    uint32_t objective;                 /**< Optimization objective type */

    /* Quantum walk */
    uint32_t walk_steps;                /**< Quantum walk evolution steps */
    float decoherence_rate;             /**< Decoherence rate [0,1] */

    /* Metrics */
    bool enable_metrics;
    uint32_t metrics_flush_interval_ms;
    char metrics_output_dir[256];
} elig_quantum_config_t;

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Get default quantum bridge configuration
 */
elig_quantum_config_t elig_quantum_default_config(void);

/**
 * @brief Create eligibility quantum bridge context
 */
eligibility_quantum_ctx_t elig_quantum_create(const elig_quantum_config_t* config);

/**
 * @brief Destroy eligibility quantum bridge context
 */
void elig_quantum_destroy(eligibility_quantum_ctx_t ctx);

/**
 * @brief Reset quantum bridge context
 */
void elig_quantum_reset(eligibility_quantum_ctx_t ctx);

/**
 * @brief Check if quantum bridge is enabled
 */
bool elig_quantum_is_enabled(const eligibility_quantum_ctx_t ctx);

/**
 * @brief Enable/disable quantum bridge
 */
void elig_quantum_set_enabled(eligibility_quantum_ctx_t ctx, bool enabled);

/*=============================================================================
 * BOTTLENECK DETECTION API (Quantum-Shannon)
 *===========================================================================*/

/**
 * @brief Analyze information flow through eligibility traces
 *
 * Uses Shannon entropy and channel capacity to identify bottlenecks
 * where credit assignment information is being lost.
 *
 * @param ctx Quantum bridge context
 * @param traces Array of eligibility traces
 * @param weights Array of synaptic weights
 * @param num_synapses Number of synapses
 * @return Information efficiency [0, 1] where 1 = perfect
 */
float elig_quantum_analyze_information_flow(
    eligibility_quantum_ctx_t ctx,
    const eligibility_trace_t* traces,
    const float* weights,
    uint32_t num_synapses
);

/**
 * @brief Detect information bottlenecks in eligibility system
 *
 * @param ctx Quantum bridge context
 * @param traces Array of eligibility traces
 * @param weights Array of synaptic weights
 * @param num_synapses Number of synapses
 * @param bottlenecks Output array of bottlenecks
 * @param max_bottlenecks Maximum bottlenecks to return
 * @param num_found Number of bottlenecks found
 * @return true on success
 */
bool elig_quantum_detect_bottlenecks(
    eligibility_quantum_ctx_t ctx,
    const eligibility_trace_t* traces,
    const float* weights,
    uint32_t num_synapses,
    elig_quantum_bottleneck_t* bottlenecks,
    uint32_t max_bottlenecks,
    uint32_t* num_found
);

/**
 * @brief Compute Shannon entropy of trace distribution
 *
 * @param traces Array of eligibility traces
 * @param num_traces Number of traces
 * @return Entropy in bits
 */
float elig_quantum_compute_trace_entropy(
    const eligibility_trace_t* traces,
    uint32_t num_traces
);

/**
 * @brief Optimize parameters based on bottleneck analysis
 *
 * @param ctx Quantum bridge context
 * @param bottlenecks Detected bottlenecks
 * @param num_bottlenecks Number of bottlenecks
 * @param param_adjustments Output parameter adjustments
 * @return true on success
 */
bool elig_quantum_optimize_from_bottlenecks(
    eligibility_quantum_ctx_t ctx,
    const elig_quantum_bottleneck_t* bottlenecks,
    uint32_t num_bottlenecks,
    elig_quantum_params_t* param_adjustments
);

/*=============================================================================
 * CREDIT ASSIGNMENT API (Quantum Monte Carlo)
 *===========================================================================*/

/**
 * @brief Compute credit assignment using Monte Carlo estimation
 *
 * Uses importance sampling to efficiently estimate credit distribution
 * across synapses based on eligibility traces and temporal patterns.
 *
 * @param ctx Quantum bridge context
 * @param traces Array of eligibility traces
 * @param num_synapses Number of synapses
 * @param reward Reward signal
 * @param credits Output credit assignments
 * @param max_credits Maximum credits to return
 * @param num_credits Number of credits assigned
 * @return true on success
 */
bool elig_quantum_assign_credit(
    eligibility_quantum_ctx_t ctx,
    const eligibility_trace_t* traces,
    uint32_t num_synapses,
    float reward,
    elig_quantum_credit_t* credits,
    uint32_t max_credits,
    uint32_t* num_credits
);

/**
 * @brief Estimate optimal credit distribution entropy
 *
 * @param ctx Quantum bridge context
 * @param traces Array of eligibility traces
 * @param num_synapses Number of synapses
 * @return Estimated optimal entropy
 */
float elig_quantum_estimate_optimal_entropy(
    eligibility_quantum_ctx_t ctx,
    const eligibility_trace_t* traces,
    uint32_t num_synapses
);

/**
 * @brief Compute causal contribution strength
 *
 * Uses counterfactual analysis via Monte Carlo to estimate
 * causal contribution of each synapse.
 *
 * @param ctx Quantum bridge context
 * @param traces Array of eligibility traces
 * @param weights Array of synaptic weights
 * @param num_synapses Number of synapses
 * @param causal_strengths Output causal strength array
 * @return true on success
 */
bool elig_quantum_compute_causal_strength(
    eligibility_quantum_ctx_t ctx,
    const eligibility_trace_t* traces,
    const float* weights,
    uint32_t num_synapses,
    float* causal_strengths
);

/*=============================================================================
 * QUANTUM OPTIMIZATION API (Quantum Annealing)
 *===========================================================================*/

/**
 * @brief Perform quantum optimization step for eligibility parameters
 *
 * Uses quantum annealing to find optimal trace parameters based on
 * current network activity and learning objectives.
 *
 * @param ctx Quantum bridge context
 * @param traces Array of eligibility traces
 * @param num_synapses Number of synapses
 * @param current_params Current parameters
 * @param optimized_params Output optimized parameters
 * @return true on success
 */
bool elig_quantum_optimize_params(
    eligibility_quantum_ctx_t ctx,
    const eligibility_trace_t* traces,
    uint32_t num_synapses,
    const elig_quantum_params_t* current_params,
    elig_quantum_params_t* optimized_params
);

/**
 * @brief Get current annealing state
 */
bool elig_quantum_get_anneal_state(
    eligibility_quantum_ctx_t ctx,
    elig_quantum_anneal_state_t* state
);

/**
 * @brief Reset annealing state
 */
void elig_quantum_reset_anneal(eligibility_quantum_ctx_t ctx);

/**
 * @brief Set optimization objective
 *
 * @param ctx Quantum bridge context
 * @param objective 0=stability, 1=sparsity, 2=balance, 3=information, 4=homeostasis
 */
void elig_quantum_set_objective(eligibility_quantum_ctx_t ctx, uint32_t objective);

/*=============================================================================
 * QUANTUM WALK DIFFUSION API
 *===========================================================================*/

/**
 * @brief Diffuse eligibility signal using quantum walk
 *
 * Achieves sqrt(N) speedup for spreading eligibility information
 * through the network connectivity graph.
 *
 * @param ctx Quantum bridge context
 * @param source_synapse Source synapse for diffusion
 * @param initial_eligibility Initial eligibility value
 * @param adjacency Network adjacency (NULL = use attached network)
 * @param num_synapses Number of synapses
 * @param diffused_eligibility Output eligibility distribution
 * @return true on success
 */
bool elig_quantum_diffuse(
    eligibility_quantum_ctx_t ctx,
    uint32_t source_synapse,
    float initial_eligibility,
    const uint8_t* adjacency,
    uint32_t num_synapses,
    float* diffused_eligibility
);

/**
 * @brief Multi-source eligibility diffusion
 */
bool elig_quantum_diffuse_multi(
    eligibility_quantum_ctx_t ctx,
    const uint32_t* source_synapses,
    const float* initial_eligibilities,
    uint32_t num_sources,
    const uint8_t* adjacency,
    uint32_t num_synapses,
    float* diffused_eligibility
);

/**
 * @brief Get quantum walk statistics
 */
bool elig_quantum_get_walk_stats(
    eligibility_quantum_ctx_t ctx,
    float* speedup_factor,
    float* spreading_distance,
    float* entropy
);

/*=============================================================================
 * ENHANCED ELIGIBILITY OPERATIONS
 *===========================================================================*/

/**
 * @brief Enhanced trace update with quantum integration
 *
 * Combines: trace dynamics, bottleneck detection, quantum optimization
 *
 * @param ctx Quantum bridge context
 * @param trace Eligibility trace (modified in place)
 * @param config Eligibility configuration
 * @param current_time Current simulation time
 * @param spike_occurred Whether spike occurred
 * @param weight Current synaptic weight
 */
void elig_quantum_update_trace_enhanced(
    eligibility_quantum_ctx_t ctx,
    eligibility_trace_t* trace,
    const eligibility_config_t* config,
    uint64_t current_time,
    bool spike_occurred,
    float weight
);

/**
 * @brief Enhanced reward consolidation with quantum credit assignment
 *
 * @param ctx Quantum bridge context
 * @param synapses Array of synapses (opaque)
 * @param traces Array of eligibility traces
 * @param num_synapses Number of synapses
 * @param config Configuration
 * @param reward Reward signal
 * @param dopamine_level Dopamine concentration
 * @return Number of synapses updated
 */
int elig_quantum_consolidate_enhanced(
    eligibility_quantum_ctx_t ctx,
    void* synapses,
    const eligibility_trace_t* traces,
    uint32_t num_synapses,
    const eligibility_config_t* config,
    float reward,
    float dopamine_level
);

/**
 * @brief Full learning tick with all quantum integration
 *
 * @param ctx Quantum bridge context
 * @param traces Array of eligibility traces
 * @param weights Array of synaptic weights (modified)
 * @param num_synapses Number of synapses
 * @param config Configuration
 * @param reward Current reward signal
 * @param current_time Current simulation time
 * @return Number of weights updated
 */
uint32_t elig_quantum_learning_tick(
    eligibility_quantum_ctx_t ctx,
    eligibility_trace_t* traces,
    float* weights,
    uint32_t num_synapses,
    const eligibility_config_t* config,
    float reward,
    uint64_t current_time
);

/*=============================================================================
 * METRICS API
 *===========================================================================*/

/**
 * @brief Get comprehensive metrics snapshot
 */
bool elig_quantum_get_metrics(eligibility_quantum_ctx_t ctx, elig_quantum_metrics_t* metrics);

/**
 * @brief Flush metrics to disk
 */
int32_t elig_quantum_flush_metrics(eligibility_quantum_ctx_t ctx);

/**
 * @brief Export metrics to CSV
 */
bool elig_quantum_export_csv(eligibility_quantum_ctx_t ctx, const char* filename);

/**
 * @brief Export metrics to JSON
 */
bool elig_quantum_export_json(eligibility_quantum_ctx_t ctx, const char* filename);

/**
 * @brief Reset metrics counters
 */
void elig_quantum_reset_metrics(eligibility_quantum_ctx_t ctx);

/*=============================================================================
 * DIAGNOSTIC API
 *===========================================================================*/

/**
 * @brief Print quantum bridge status
 */
void elig_quantum_print_status(const eligibility_quantum_ctx_t ctx);

/**
 * @brief Verify quantum bridge integrity
 */
bool elig_quantum_verify(const eligibility_quantum_ctx_t ctx);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ELIGIBILITY_QUANTUM_BRIDGE_H */
