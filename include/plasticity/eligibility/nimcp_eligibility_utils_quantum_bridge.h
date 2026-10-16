/**
 * @file nimcp_eligibility_utils_quantum_bridge.h
 * @brief Eligibility Utils-Quantum Bidirectional Bridge
 * @version 1.0.0
 * @date 2026-01-12
 *
 * Bidirectional integration between Phase 4 (Utils) and Phase 5 (Quantum)
 * eligibility modules, enabling closed-loop optimization where metrics
 * trigger quantum processing and quantum results feed back to improve
 * utils operations.
 *
 * BIDIRECTIONAL COMMUNICATION:
 * =============================================================================
 *
 *   Utils -> Quantum (Forward Direction):
 *   +-----------------------------------------------------------------------+
 *   | Trigger              | Condition              | Quantum Action        |
 *   +----------------------+------------------------+-----------------------+
 *   | LTP/LTD imbalance    | Ratio < 0.3 or > 3.0   | Quantum annealing     |
 *   | Pool pressure        | Utilization > 90%      | Quantum walk priority |
 *   | Bottleneck detected  | Info deficit > 60%     | Quantum-Shannon       |
 *   | History ready        | > 100 samples          | Init quantum walk     |
 *   | Latency spike        | > 1ms avg latency      | Param optimization    |
 *   +-----------------------------------------------------------------------+
 *
 *   Quantum -> Utils (Backward Direction):
 *   +-----------------------------------------------------------------------+
 *   | Feedback             | Source                 | Utils Update          |
 *   +----------------------+------------------------+-----------------------+
 *   | Optimized params     | Annealing              | Adjust RK4 dt/tol     |
 *   | Credit fractions     | QMC                    | Update metrics        |
 *   | Diffused priorities  | Quantum walk           | Pool allocation       |
 *   | Resolution results   | Quantum-Shannon        | Clear bottleneck      |
 *   +-----------------------------------------------------------------------+
 *
 * FEEDBACK LOOPS:
 *   - Metrics -> Quantum Optimization -> New Parameters -> Better Metrics
 *   - History -> Quantum Walk -> Diffused Eligibility -> Updated History
 *   - Bottlenecks -> Quantum-Shannon -> Adjustments -> Reduced Bottlenecks
 *
 * BIOLOGICAL ANALOGY:
 *   - Homeostatic metaplasticity: metrics drive parameter adjustment
 *   - Synaptic tagging and capture: history informs credit assignment
 *   - Activity-dependent scaling: diffusion priorities resource allocation
 */

#ifndef NIMCP_ELIGIBILITY_UTILS_QUANTUM_BRIDGE_H
#define NIMCP_ELIGIBILITY_UTILS_QUANTUM_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "plasticity/eligibility/nimcp_eligibility_trace.h"
#include "plasticity/eligibility/nimcp_eligibility_utils_bridge.h"
#include "plasticity/eligibility/nimcp_eligibility_quantum_bridge.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Utils -> Quantum trigger thresholds */
#define ELIG_UQ_METRIC_TRIGGER_THRESHOLD    0.7f    /**< Metric anomaly threshold */
#define ELIG_UQ_LTP_LTD_RATIO_MIN           0.3f    /**< Min acceptable LTP/LTD ratio */
#define ELIG_UQ_LTP_LTD_RATIO_MAX           3.0f    /**< Max acceptable LTP/LTD ratio */
#define ELIG_UQ_POOL_EXHAUSTION_THRESHOLD   0.9f    /**< Pool usage triggering optimization */
#define ELIG_UQ_BOTTLENECK_ESCALATION       0.6f    /**< Deficit threshold for quantum analysis */
#define ELIG_UQ_HISTORY_MIN_SAMPLES         100     /**< Min history for quantum walk */
#define ELIG_UQ_LATENCY_SPIKE_US            1000.0  /**< Latency spike threshold (1ms) */

/** Quantum -> Utils feedback factors */
#define ELIG_UQ_CREDIT_METRICS_WEIGHT       0.3f    /**< Weight of quantum credit in metrics */
#define ELIG_UQ_PARAM_INTEGRATION_RATE      0.1f    /**< Rate of param integration into RK4 */
#define ELIG_UQ_DIFFUSION_PRIORITY_SCALE    2.0f    /**< Priority scaling from diffusion */
#define ELIG_UQ_STEP_ADJUSTMENT_MAX         0.5f    /**< Max step size adjustment factor */

/** Coherence and stability */
#define ELIG_UQ_COHERENCE_DECAY             0.99f   /**< Coherence decay per update */
#define ELIG_UQ_MIN_COHERENCE               0.1f    /**< Minimum coherence threshold */
#define ELIG_UQ_STABILITY_WINDOW            100     /**< Updates for stability assessment */

/*=============================================================================
 * TYPE DEFINITIONS
 *===========================================================================*/

/**
 * @brief Trigger type for Utils -> Quantum events
 */
typedef enum {
    ELIG_UQ_TRIGGER_NONE = 0,             /**< No trigger */
    ELIG_UQ_TRIGGER_METRIC_ANOMALY,       /**< Metrics indicate problem */
    ELIG_UQ_TRIGGER_LTP_LTD_IMBALANCE,    /**< LTP/LTD ratio out of range */
    ELIG_UQ_TRIGGER_POOL_PRESSURE,        /**< Memory pool exhaustion */
    ELIG_UQ_TRIGGER_BOTTLENECK,           /**< Shannon bottleneck detected */
    ELIG_UQ_TRIGGER_HISTORY_READY,        /**< Sufficient history for walk */
    ELIG_UQ_TRIGGER_LATENCY_SPIKE,        /**< Processing latency spike */
    ELIG_UQ_TRIGGER_COUNT
} elig_uq_trigger_type_t;

/**
 * @brief Feedback type for Quantum -> Utils events
 */
typedef enum {
    ELIG_UQ_FEEDBACK_NONE = 0,              /**< No feedback */
    ELIG_UQ_FEEDBACK_CREDIT_ASSIGNMENT,     /**< QMC credit results */
    ELIG_UQ_FEEDBACK_PARAM_OPTIMIZATION,    /**< Annealed parameters */
    ELIG_UQ_FEEDBACK_DIFFUSION_RESULT,      /**< Quantum walk diffusion */
    ELIG_UQ_FEEDBACK_BOTTLENECK_RESOLVED,   /**< Quantum-Shannon resolution */
    ELIG_UQ_FEEDBACK_STEP_SIZE,             /**< Adaptive step size */
    ELIG_UQ_FEEDBACK_COUNT
} elig_uq_feedback_type_t;

/**
 * @brief Configuration for Utils-Quantum bridge
 */
typedef struct {
    /* Forward triggers (Utils -> Quantum) */
    bool enable_metric_triggers;          /**< Enable metrics-based triggers */
    float metric_trigger_threshold;       /**< Threshold for metric anomaly */
    float ltp_ltd_ratio_min;              /**< Min acceptable LTP/LTD ratio */
    float ltp_ltd_ratio_max;              /**< Max acceptable LTP/LTD ratio */
    float pool_exhaustion_threshold;      /**< Pool usage for optimization */
    float bottleneck_escalation_threshold; /**< Deficit for quantum analysis */
    uint32_t history_min_samples;         /**< Min history for quantum walk */
    float latency_spike_threshold_us;     /**< Latency spike threshold */

    /* Backward feedback (Quantum -> Utils) */
    bool enable_credit_feedback;          /**< Enable QMC credit -> metrics */
    bool enable_param_feedback;           /**< Enable param -> RK4 feedback */
    bool enable_diffusion_feedback;       /**< Enable diffusion -> priority */
    bool enable_step_feedback;            /**< Enable annealing -> step size */
    float credit_metrics_weight;          /**< Weight of credit in metrics */
    float param_integration_rate;         /**< Rate of param integration */
    float diffusion_priority_scale;       /**< Priority scaling from diffusion */
    float step_adjustment_max;            /**< Max step size adjustment */

    /* Feedback loop control */
    bool enable_auto_feedback_loop;       /**< Enable automatic feedback */
    uint32_t feedback_loop_interval_ms;   /**< Feedback loop period */
    float coherence_threshold;            /**< Min coherence for feedback */
    uint32_t stability_window;            /**< Updates for stability */

    /* Bio-async */
    bool enable_bio_async;                /**< Enable async messaging */
} elig_uq_bridge_config_t;

/**
 * @brief Forward effect: Utils -> Quantum
 *
 * Encapsulates the effect of utils state on quantum operations.
 */
typedef struct {
    elig_uq_trigger_type_t trigger_type;  /**< What triggered the event */

    /* Metrics-based trigger data */
    float ltp_ltd_ratio;                  /**< Current LTP/LTD ratio */
    float mean_trace_value;               /**< Mean trace magnitude */
    float information_efficiency;         /**< Current info efficiency */
    uint32_t bottleneck_count;            /**< Detected bottlenecks */

    /* Pool state */
    float pool_utilization;               /**< Pool used/total ratio */
    uint32_t pool_free_count;             /**< Available pool slots */

    /* Integration state */
    float current_integration_dt;         /**< Current RK4 timestep */
    float adaptive_error;                 /**< Adaptive integration error */

    /* History summary */
    uint32_t history_samples;             /**< Available history samples */
    float history_variance;               /**< Trace history variance */

    /* Latency */
    double avg_latency_us;                /**< Average update latency */

    /* Recommended quantum action */
    bool request_annealing;               /**< Request parameter annealing */
    bool request_qmc_credit;              /**< Request QMC credit assignment */
    bool request_quantum_walk;            /**< Request quantum diffusion */
    bool request_quantum_shannon;         /**< Request quantum bottleneck analysis */

    /* Timing */
    uint64_t timestamp_ms;                /**< Event timestamp */
} elig_uq_forward_effect_t;

/**
 * @brief Backward effect: Quantum -> Utils
 *
 * Encapsulates quantum results feeding back to utils.
 */
typedef struct {
    elig_uq_feedback_type_t feedback_type; /**< Type of feedback */

    /* Credit assignment feedback */
    float avg_credit_fraction;            /**< Average credit fraction */
    float credit_confidence;              /**< Confidence in assignment */
    float credit_entropy;                 /**< Entropy of credit distribution */

    /* Parameter optimization feedback */
    float optimized_tau_fast;             /**< Optimized fast trace tau */
    float optimized_tau_slow;             /**< Optimized slow trace tau */
    float optimized_learning_rate;        /**< Optimized learning rate */
    float optimization_energy;            /**< Final objective value */

    /* Diffusion feedback */
    float diffusion_speedup;              /**< Achieved sqrt(N) speedup */
    float max_diffused_priority;          /**< Maximum diffused priority */
    float mean_diffused_priority;         /**< Mean diffused priority */

    /* Step size feedback */
    float recommended_dt;                 /**< Recommended integration dt */
    float recommended_tolerance;          /**< Recommended error tolerance */

    /* Bottleneck resolution */
    uint32_t bottlenecks_resolved;        /**< Number resolved */
    float resolution_effectiveness;       /**< Resolution quality [0,1] */

    /* Timing */
    uint64_t processing_time_us;          /**< Quantum processing time */
    uint64_t timestamp_ms;                /**< Event timestamp */
} elig_uq_backward_effect_t;

/**
 * @brief Bridge state tracking
 */
typedef struct {
    /* Forward state */
    float current_ltp_ltd_ratio;          /**< Latest LTP/LTD ratio */
    float current_pool_utilization;       /**< Latest pool utilization */
    float current_info_efficiency;        /**< Latest info efficiency */
    uint32_t pending_triggers;            /**< Pending forward triggers */

    /* Backward state */
    float current_optimized_dt;           /**< Current optimized timestep */
    float current_optimized_tolerance;    /**< Current optimized tolerance */
    float current_diffusion_priority;     /**< Current priority factor */
    float cumulative_credit_feedback;     /**< Accumulated credit feedback */
    uint32_t pending_feedbacks;           /**< Pending backward feedbacks */

    /* Coherence metrics */
    float utils_quantum_coherence;        /**< Bridge coherence [0,1] */
    float feedback_loop_gain;             /**< Effective feedback gain */
    float stability_metric;               /**< System stability [0,1] */

    /* Timing */
    uint64_t last_forward_ms;             /**< Last forward event */
    uint64_t last_backward_ms;            /**< Last backward event */
    uint64_t last_feedback_loop_ms;       /**< Last feedback loop */
} elig_uq_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Forward statistics */
    uint64_t total_forward_events;        /**< Total forward events */
    uint64_t metric_triggers;             /**< Metric-based triggers */
    uint64_t ltp_ltd_triggers;            /**< LTP/LTD imbalance triggers */
    uint64_t pool_triggers;               /**< Pool pressure triggers */
    uint64_t bottleneck_triggers;         /**< Bottleneck escalations */
    uint64_t history_triggers;            /**< History-ready triggers */
    uint64_t latency_triggers;            /**< Latency spike triggers */

    /* Backward statistics */
    uint64_t total_backward_events;       /**< Total backward events */
    uint64_t credit_feedbacks;            /**< Credit assignment feedbacks */
    uint64_t param_feedbacks;             /**< Parameter optimization feedbacks */
    uint64_t diffusion_feedbacks;         /**< Diffusion result feedbacks */
    uint64_t step_feedbacks;              /**< Step size feedbacks */
    uint64_t bottleneck_resolutions;      /**< Bottleneck resolutions */

    /* Feedback loop statistics */
    uint64_t feedback_loop_iterations;    /**< Total feedback loop iterations */
    uint64_t successful_optimizations;    /**< Successful param optimizations */

    /* Effectiveness */
    float avg_optimization_improvement;   /**< Average param improvement */
    float avg_bottleneck_resolution;      /**< Average resolution rate */
    float cumulative_speedup;             /**< Total diffusion speedup */

    /* Performance */
    double total_forward_time_ms;         /**< Total forward processing */
    double total_backward_time_ms;        /**< Total backward processing */
    double avg_round_trip_us;             /**< Average feedback loop latency */

    /* Coherence history */
    float min_coherence;                  /**< Minimum coherence observed */
    float max_coherence;                  /**< Maximum coherence observed */
    float avg_coherence;                  /**< Average coherence */
} elig_uq_bridge_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct elig_uq_bridge_struct* elig_uq_bridge_t;

/*=============================================================================
 * CONFIGURATION API
 *===========================================================================*/

/**
 * @brief Get default bridge configuration
 *
 * Returns sensible defaults with all features enabled:
 * - Forward triggers: metric anomaly, LTP/LTD, pool pressure, bottleneck
 * - Backward feedback: credit, params, diffusion, step size
 * - Auto feedback loop: enabled with 100ms interval
 *
 * @return Default configuration struct
 */
elig_uq_bridge_config_t elig_uq_bridge_default_config(void);

/**
 * @brief Validate bridge configuration
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 */
bool elig_uq_bridge_validate_config(const elig_uq_bridge_config_t* config);

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Create Utils-Quantum bridge
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
elig_uq_bridge_t elig_uq_bridge_create(const elig_uq_bridge_config_t* config);

/**
 * @brief Destroy Utils-Quantum bridge
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void elig_uq_bridge_destroy(elig_uq_bridge_t bridge);

/**
 * @brief Attach utils context
 *
 * @param bridge Bridge handle
 * @param utils_ctx Eligibility utils context
 * @return 0 on success, -1 on failure
 */
int elig_uq_bridge_attach_utils(elig_uq_bridge_t bridge,
                                eligibility_utils_ctx_t utils_ctx);

/**
 * @brief Attach quantum context
 *
 * @param bridge Bridge handle
 * @param quantum_ctx Eligibility quantum context
 * @return 0 on success, -1 on failure
 */
int elig_uq_bridge_attach_quantum(elig_uq_bridge_t bridge,
                                  eligibility_quantum_ctx_t quantum_ctx);

/**
 * @brief Check if bridge is fully connected
 *
 * @param bridge Bridge handle
 * @return true if both contexts attached
 */
bool elig_uq_bridge_is_connected(const elig_uq_bridge_t bridge);

/**
 * @brief Reset bridge state
 *
 * Clears state and statistics but preserves attached contexts.
 *
 * @param bridge Bridge handle
 */
void elig_uq_bridge_reset(elig_uq_bridge_t bridge);

/*=============================================================================
 * FORWARD DIRECTION: Utils -> Quantum
 *===========================================================================*/

/**
 * @brief Evaluate utils metrics and generate quantum triggers
 *
 * Analyzes current utils metrics and determines if quantum optimization
 * is needed based on configured thresholds.
 *
 * @param bridge Bridge handle
 * @param effect Output: forward effect describing triggers
 * @return 0 on success, -1 on failure
 */
int elig_uq_evaluate_metrics(elig_uq_bridge_t bridge,
                             elig_uq_forward_effect_t* effect);

/**
 * @brief Notify quantum of LTP/LTD ratio imbalance
 *
 * Triggers quantum annealing if ratio is outside acceptable range.
 *
 * @param bridge Bridge handle
 * @param ltp_count LTP events count
 * @param ltd_count LTD events count
 * @param effect Output: forward effect
 * @return 0 on success, -1 on failure
 */
int elig_uq_notify_ltp_ltd_imbalance(elig_uq_bridge_t bridge,
                                     uint64_t ltp_count,
                                     uint64_t ltd_count,
                                     elig_uq_forward_effect_t* effect);

/**
 * @brief Notify quantum of pool pressure
 *
 * Triggers quantum walk for allocation priority when pool is near exhaustion.
 *
 * @param bridge Bridge handle
 * @param utilization Pool utilization [0,1]
 * @param free_count Available slots
 * @param effect Output: forward effect
 * @return 0 on success, -1 on failure
 */
int elig_uq_notify_pool_pressure(elig_uq_bridge_t bridge,
                                 float utilization,
                                 uint32_t free_count,
                                 elig_uq_forward_effect_t* effect);

/**
 * @brief Escalate classical bottleneck to quantum-Shannon analysis
 *
 * When classical Shannon analysis finds bottlenecks above threshold,
 * escalate to quantum-Shannon for deeper analysis.
 *
 * @param bridge Bridge handle
 * @param bottlenecks Classical bottlenecks from utils
 * @param num_bottlenecks Number of bottlenecks
 * @param effect Output: forward effect
 * @return 0 on success, -1 on failure
 */
int elig_uq_escalate_bottleneck(elig_uq_bridge_t bridge,
                                const eligibility_bottleneck_t* bottlenecks,
                                uint32_t num_bottlenecks,
                                elig_uq_forward_effect_t* effect);

/**
 * @brief Provide history data for quantum walk initialization
 *
 * Ring buffer history is used to set quantum walk initial conditions.
 *
 * @param bridge Bridge handle
 * @param traces Recent trace history
 * @param num_samples Number of history samples
 * @param effect Output: forward effect
 * @return 0 on success, -1 on failure
 */
int elig_uq_provide_history(elig_uq_bridge_t bridge,
                            const eligibility_trace_t* traces,
                            uint32_t num_samples,
                            elig_uq_forward_effect_t* effect);

/**
 * @brief Request quantum parameter optimization
 *
 * Explicitly request quantum annealing based on current utils state.
 *
 * @param bridge Bridge handle
 * @param current_params Current eligibility parameters
 * @param effect Output: forward effect
 * @return 0 on success, -1 on failure
 */
int elig_uq_request_optimization(elig_uq_bridge_t bridge,
                                 const elig_quantum_params_t* current_params,
                                 elig_uq_forward_effect_t* effect);

/*=============================================================================
 * BACKWARD DIRECTION: Quantum -> Utils
 *===========================================================================*/

/**
 * @brief Apply quantum credit assignment to utils metrics
 *
 * Feed QMC credit results back to metrics collection for
 * improved credit assignment tracking.
 *
 * @param bridge Bridge handle
 * @param credits Quantum credit assignments
 * @param num_credits Number of assignments
 * @param effect Output: backward effect
 * @return 0 on success, -1 on failure
 */
int elig_uq_apply_credit_feedback(elig_uq_bridge_t bridge,
                                  const elig_quantum_credit_t* credits,
                                  uint32_t num_credits,
                                  elig_uq_backward_effect_t* effect);

/**
 * @brief Apply optimized parameters to RK4 integration
 *
 * Blend quantum-optimized parameters into utils integration settings.
 *
 * @param bridge Bridge handle
 * @param optimized Optimized parameters from annealing
 * @param effect Output: backward effect
 * @return 0 on success, -1 on failure
 */
int elig_uq_apply_param_feedback(elig_uq_bridge_t bridge,
                                 const elig_quantum_params_t* optimized,
                                 elig_uq_backward_effect_t* effect);

/**
 * @brief Apply quantum diffusion results to pool priorities
 *
 * Use diffused eligibility values to inform allocation priority.
 *
 * @param bridge Bridge handle
 * @param diffused_eligibility Diffused eligibility values
 * @param num_synapses Number of synapses
 * @param effect Output: backward effect
 * @return 0 on success, -1 on failure
 */
int elig_uq_apply_diffusion_feedback(elig_uq_bridge_t bridge,
                                     const float* diffused_eligibility,
                                     uint32_t num_synapses,
                                     elig_uq_backward_effect_t* effect);

/**
 * @brief Apply annealing schedule to integration step size
 *
 * Use annealing state to guide adaptive integration parameters.
 *
 * @param bridge Bridge handle
 * @param anneal_state Current annealing state
 * @param effect Output: backward effect
 * @return 0 on success, -1 on failure
 */
int elig_uq_apply_step_feedback(elig_uq_bridge_t bridge,
                                const elig_quantum_anneal_state_t* anneal_state,
                                elig_uq_backward_effect_t* effect);

/**
 * @brief Get recommended integration parameters
 *
 * Computes optimal RK4 parameters based on quantum feedback.
 *
 * @param bridge Bridge handle
 * @param dt Output: recommended timestep
 * @param tolerance Output: recommended error tolerance
 * @return 0 on success, -1 on failure
 */
int elig_uq_get_integration_params(elig_uq_bridge_t bridge,
                                   float* dt,
                                   float* tolerance);

/*=============================================================================
 * FEEDBACK LOOP API
 *===========================================================================*/

/**
 * @brief Execute one feedback loop iteration
 *
 * Complete one round of Utils -> Quantum -> Utils feedback:
 * 1. Evaluate utils metrics, determine triggers
 * 2. Execute quantum operations based on triggers
 * 3. Apply quantum results back to utils
 *
 * @param bridge Bridge handle
 * @param traces Current eligibility traces
 * @param weights Current synaptic weights
 * @param num_synapses Number of synapses
 * @param forward_effect Output: forward effect (may be NULL)
 * @param backward_effect Output: backward effect (may be NULL)
 * @return 0 on success, -1 on failure
 */
int elig_uq_feedback_loop_tick(elig_uq_bridge_t bridge,
                               eligibility_trace_t* traces,
                               float* weights,
                               uint32_t num_synapses,
                               elig_uq_forward_effect_t* forward_effect,
                               elig_uq_backward_effect_t* backward_effect);

/**
 * @brief Enable/disable automatic feedback loop
 *
 * @param bridge Bridge handle
 * @param enabled Enable state
 */
void elig_uq_set_auto_feedback(elig_uq_bridge_t bridge, bool enabled);

/**
 * @brief Check if auto feedback is enabled
 *
 * @param bridge Bridge handle
 * @return true if auto feedback enabled
 */
bool elig_uq_is_auto_feedback_enabled(const elig_uq_bridge_t bridge);

/**
 * @brief Get feedback loop coherence
 *
 * Measures how well utils and quantum are synchronized.
 * Low coherence indicates feedback loop instability.
 *
 * @param bridge Bridge handle
 * @return Coherence [0,1] or -1.0f on error
 */
float elig_uq_get_coherence(const elig_uq_bridge_t bridge);

/**
 * @brief Get feedback loop stability
 *
 * Measures stability of the feedback loop over the stability window.
 *
 * @param bridge Bridge handle
 * @return Stability [0,1] or -1.0f on error
 */
float elig_uq_get_stability(const elig_uq_bridge_t bridge);

/*=============================================================================
 * STATE AND STATISTICS API
 *===========================================================================*/

/**
 * @brief Get current bridge state
 *
 * @param bridge Bridge handle
 * @param state Output: current state
 * @return 0 on success, -1 on failure
 */
int elig_uq_bridge_get_state(const elig_uq_bridge_t bridge,
                             elig_uq_bridge_state_t* state);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output: statistics
 * @return 0 on success, -1 on failure
 */
int elig_uq_bridge_get_stats(const elig_uq_bridge_t bridge,
                             elig_uq_bridge_stats_t* stats);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int elig_uq_bridge_reset_stats(elig_uq_bridge_t bridge);

/**
 * @brief Update bridge state
 *
 * Call periodically to decay coherence and update timing.
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on failure
 */
int elig_uq_bridge_update(elig_uq_bridge_t bridge, float dt_ms);

/**
 * @brief Print bridge summary
 *
 * @param bridge Bridge handle
 */
void elig_uq_bridge_print_summary(const elig_uq_bridge_t bridge);

/*=============================================================================
 * DIAGNOSTIC API
 *===========================================================================*/

/**
 * @brief Verify bridge integrity
 *
 * @param bridge Bridge handle
 * @return true if healthy
 */
bool elig_uq_bridge_verify(const elig_uq_bridge_t bridge);

/**
 * @brief Export bridge metrics to CSV
 *
 * @param bridge Bridge handle
 * @param filename Output file path
 * @return true on success
 */
bool elig_uq_bridge_export_csv(const elig_uq_bridge_t bridge,
                               const char* filename);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ELIGIBILITY_UTILS_QUANTUM_BRIDGE_H */
