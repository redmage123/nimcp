/**
 * @file nimcp_hypothalamus_quantum_bridge.h
 * @brief Quantum bridge for hypothalamus integration
 *
 * WHAT: Bridge connecting hypothalamus to quantum reasoning system
 * WHY:  Enable quantum-accelerated homeostatic optimization and prediction
 * HOW:  Uses quantum annealing for multi-objective optimization problems
 *
 * ARCHITECTURE:
 * - Quantum Homeostatic Optimization: Find optimal setpoints across multiple parameters
 * - Parallel Autonomic Evaluation: Evaluate multiple autonomic states in superposition
 * - Circadian Phase Prediction: Quantum walk for phase trajectory optimization
 * - Stress Response Optimization: Quantum annealing for HPA axis tuning
 *
 * BIOLOGICAL BASIS:
 * - Hypothalamus must balance multiple competing homeostatic goals
 * - Trade-offs between temperature, hunger, thirst, stress require optimization
 * - Quantum superposition enables parallel evaluation of regulatory strategies
 * - Quantum annealing finds global optima in complex energy landscapes
 *
 * QUANTUM ALGORITHMS:
 * - QUBO (Quadratic Unconstrained Binary Optimization) for setpoint optimization
 * - Quantum Approximate Optimization Algorithm (QAOA) for regulatory trade-offs
 * - Grover search for optimal circadian phase adjustment
 * - Variational Quantum Eigensolver (VQE) for HPA axis parameter tuning
 *
 * @version Phase H1: Hypothalamus Brain Integration
 * @date 2025-12-30
 */

#ifndef NIMCP_HYPOTHALAMUS_QUANTUM_BRIDGE_H
#define NIMCP_HYPOTHALAMUS_QUANTUM_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_adapter.h"

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Default quantum bridge configuration values
 */
#define HYPOTHALAMUS_QUANTUM_DEFAULT_QUBITS          8
#define HYPOTHALAMUS_QUANTUM_DEFAULT_DEPTH           4
#define HYPOTHALAMUS_QUANTUM_DEFAULT_ITERATIONS      100
#define HYPOTHALAMUS_QUANTUM_DEFAULT_ANNEALING_TIME  100.0f
#define HYPOTHALAMUS_QUANTUM_DEFAULT_MIXING_RATIO    0.7f

/**
 * @brief Quantum optimization modes for hypothalamus
 */
typedef enum {
    HYPOTHALAMUS_QUANTUM_MODE_DISABLED = 0,     /**< Quantum disabled */
    HYPOTHALAMUS_QUANTUM_MODE_HOMEOSTATIC,      /**< Homeostatic optimization only */
    HYPOTHALAMUS_QUANTUM_MODE_CIRCADIAN,        /**< Circadian optimization only */
    HYPOTHALAMUS_QUANTUM_MODE_AUTONOMIC,        /**< Autonomic optimization only */
    HYPOTHALAMUS_QUANTUM_MODE_HPA,              /**< HPA axis optimization only */
    HYPOTHALAMUS_QUANTUM_MODE_FULL              /**< Full quantum integration */
} hypothalamus_quantum_mode_t;

/**
 * @brief Quantum bridge configuration
 */
typedef struct {
    /* Quantum system parameters */
    hypothalamus_quantum_mode_t mode;    /**< Optimization mode */
    uint32_t num_qubits;                 /**< Number of qubits for QUBO */
    uint32_t circuit_depth;              /**< Variational circuit depth */
    uint32_t max_iterations;             /**< Maximum optimization iterations */

    /* Annealing parameters */
    float annealing_time_us;             /**< Total annealing time */
    float initial_temperature;           /**< Initial annealing temperature */
    float final_temperature;             /**< Final annealing temperature */

    /* Quantum-classical hybrid */
    float quantum_mixing_ratio;          /**< Quantum vs classical mix [0, 1] */
    bool enable_error_mitigation;        /**< Enable quantum error mitigation */

    /* Optimization targets */
    bool optimize_temperature;           /**< Include temperature in QUBO */
    bool optimize_glucose;               /**< Include glucose in QUBO */
    bool optimize_hydration;             /**< Include hydration in QUBO */
    bool optimize_stress;                /**< Include stress response in QUBO */
    bool optimize_circadian;             /**< Include circadian in QUBO */

    /* Performance */
    uint32_t update_interval_us;         /**< Minimum interval between quantum updates */
    bool enable_caching;                 /**< Cache optimization results */
    uint32_t cache_validity_us;          /**< Cache validity duration */
} hypothalamus_quantum_config_t;

/*=============================================================================
 * OPTIMIZATION PROBLEM STRUCTURES
 *===========================================================================*/

/**
 * @brief Homeostatic optimization objective
 */
typedef struct {
    float temperature_weight;            /**< Weight for temperature deviation */
    float glucose_weight;                /**< Weight for glucose deviation */
    float hydration_weight;              /**< Weight for hydration deviation */
    float stress_weight;                 /**< Weight for stress minimization */
    float energy_weight;                 /**< Weight for energy expenditure */

    float temperature_target;            /**< Temperature target (C) */
    float glucose_target;                /**< Glucose target (mg/dL) */
    float osmolality_target;             /**< Osmolality target (mOsm/kg) */
    float cortisol_target;               /**< Cortisol target [0, 1] */
} homeostatic_objective_t;

/**
 * @brief Regulatory constraint
 */
typedef struct {
    float min_temperature;               /**< Minimum safe temperature */
    float max_temperature;               /**< Maximum safe temperature */
    float min_glucose;                   /**< Minimum safe glucose */
    float max_glucose;                   /**< Maximum safe glucose */
    float max_cortisol;                  /**< Maximum safe cortisol */
    float max_autonomic_change_rate;     /**< Maximum autonomic change per update */
} regulatory_constraint_t;

/**
 * @brief QUBO problem for homeostatic optimization
 */
typedef struct {
    homeostatic_objective_t objective;   /**< Optimization objective */
    regulatory_constraint_t constraints; /**< Safety constraints */

    /* Current state (for linearization) */
    float current_temperature;
    float current_glucose;
    float current_osmolality;
    float current_cortisol;
    float current_sympathetic;
    float current_parasympathetic;

    /* QUBO matrix (stored as upper triangular, flattened) */
    float* qubo_matrix;                  /**< QUBO matrix Q */
    uint32_t qubo_size;                  /**< Matrix dimension */
    float* linear_terms;                 /**< Linear terms h */
    float offset;                        /**< Constant offset */
} homeostatic_qubo_t;

/**
 * @brief Optimization result
 */
typedef struct {
    /* Optimal actions */
    float optimal_heat_production;       /**< Optimal heat production rate */
    float optimal_heat_loss;             /**< Optimal heat loss rate */
    float optimal_sympathetic_change;    /**< Optimal sympathetic adjustment */
    float optimal_parasympathetic_change; /**< Optimal parasympathetic adjustment */
    float optimal_crh_release;           /**< Optimal CRH release rate */
    float optimal_circadian_shift;       /**< Optimal circadian phase shift */

    /* Solution quality */
    float energy_value;                  /**< Final energy value (lower is better) */
    float constraint_violation;          /**< Total constraint violation */
    uint32_t iterations_used;            /**< Iterations used to converge */
    bool converged;                      /**< Whether optimization converged */
    float quantum_contribution;          /**< Fraction solved by quantum */
} optimization_result_t;

/*=============================================================================
 * AUTONOMIC EVALUATION STRUCTURES
 *===========================================================================*/

/**
 * @brief Autonomic state candidate for parallel evaluation
 */
typedef struct {
    float sympathetic_tone;              /**< Proposed sympathetic tone */
    float parasympathetic_tone;          /**< Proposed parasympathetic tone */
    float heart_rate_mod;                /**< Resulting HR modulation */
    float blood_pressure_mod;            /**< Resulting BP modulation */
    float energy_cost;                   /**< Metabolic energy cost */
    float stability_score;               /**< Homeostatic stability */
} autonomic_candidate_t;

/**
 * @brief Parallel autonomic evaluation input
 */
typedef struct {
    autonomic_candidate_t* candidates;   /**< Array of candidates */
    uint32_t num_candidates;             /**< Number of candidates */

    /* Evaluation criteria weights */
    float stability_weight;              /**< Weight for stability */
    float energy_efficiency_weight;      /**< Weight for energy efficiency */
    float responsiveness_weight;         /**< Weight for quick response */
    float smoothness_weight;             /**< Weight for smooth transitions */
} autonomic_evaluation_input_t;

/**
 * @brief Parallel evaluation result
 */
typedef struct {
    uint32_t best_candidate_index;       /**< Index of best candidate */
    float* candidate_scores;             /**< Score for each candidate */
    float quantum_speedup;               /**< Achieved quantum speedup */
    uint64_t evaluation_time_us;         /**< Total evaluation time */
} autonomic_evaluation_result_t;

/*=============================================================================
 * CIRCADIAN OPTIMIZATION STRUCTURES
 *===========================================================================*/

/**
 * @brief Circadian phase optimization input
 */
typedef struct {
    float current_phase;                 /**< Current circadian phase */
    float target_phase;                  /**< Desired phase (for jet lag recovery) */
    float light_intensity;               /**< Available light intensity */
    float activity_level;                /**< Current activity level */
    float melatonin_sensitivity;         /**< Individual melatonin sensitivity */
    float cortisol_sensitivity;          /**< Individual cortisol sensitivity */
    uint32_t hours_to_deadline;          /**< Hours until target must be reached */
} circadian_optimization_input_t;

/**
 * @brief Circadian optimization result
 */
typedef struct {
    /* Optimal light exposure schedule */
    float optimal_light_times[8];        /**< Optimal light exposure times (hours) */
    float optimal_light_durations[8];    /**< Light exposure durations (hours) */
    float optimal_light_intensities[8];  /**< Light intensities [0, 1] */
    uint32_t num_light_exposures;        /**< Number of light exposures */

    /* Optimal activity schedule */
    float optimal_wake_time;             /**< Optimal wake time (hours) */
    float optimal_sleep_time;            /**< Optimal sleep time (hours) */
    float optimal_exercise_time;         /**< Optimal exercise time (hours) */

    /* Predicted outcomes */
    float predicted_adaptation_time;     /**< Hours to reach target phase */
    float predicted_jet_lag_severity;    /**< Predicted residual jet lag [0, 1] */
    float phase_shift_confidence;        /**< Confidence in prediction [0, 1] */
} circadian_optimization_result_t;

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Quantum bridge statistics
 */
typedef struct {
    /* Optimization counts */
    uint64_t homeostatic_optimizations;  /**< Homeostatic optimizations performed */
    uint64_t autonomic_evaluations;      /**< Parallel autonomic evaluations */
    uint64_t circadian_optimizations;    /**< Circadian optimizations */
    uint64_t hpa_optimizations;          /**< HPA axis optimizations */

    /* Performance */
    float avg_optimization_time_us;      /**< Average optimization time */
    float avg_quantum_speedup;           /**< Average quantum speedup achieved */
    float avg_solution_quality;          /**< Average solution quality [0, 1] */

    /* Quantum utilization */
    float quantum_utilization;           /**< Fraction of time using quantum */
    uint64_t classical_fallbacks;        /**< Times fell back to classical */
    uint64_t cache_hits;                 /**< Cached result reuse count */
} hypothalamus_quantum_stats_t;

/*=============================================================================
 * OPAQUE TYPE
 *===========================================================================*/

/** @brief Opaque quantum bridge type */
typedef struct hypothalamus_quantum_bridge hypothalamus_quantum_bridge_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default quantum bridge configuration
 *
 * WHAT: Returns default configuration for quantum bridge
 * WHY:  Provide reasonable defaults for quantum-enhanced hypothalamus
 * HOW:  Initialize all fields with biologically-motivated values
 *
 * @return Default configuration structure
 */
hypothalamus_quantum_config_t hypothalamus_quantum_default_config(void);

/**
 * @brief Create quantum bridge
 *
 * WHAT: Create quantum bridge for hypothalamus integration
 * WHY:  Enable quantum-accelerated homeostatic optimization
 * HOW:  Initialize quantum optimizer, QUBO formulation, caches
 *
 * @param hypothalamus Connected hypothalamus adapter
 * @param config Configuration (NULL for defaults)
 * @return New bridge instance, or NULL on failure
 */
hypothalamus_quantum_bridge_t* hypothalamus_quantum_bridge_create(
    hypothalamus_adapter_t* hypothalamus,
    const hypothalamus_quantum_config_t* config);

/**
 * @brief Destroy quantum bridge
 *
 * WHAT: Free all quantum bridge resources
 * WHY:  Prevent memory leaks
 * HOW:  Release quantum circuits, QUBO matrices, caches
 *
 * @param bridge Bridge to destroy
 */
void hypothalamus_quantum_bridge_destroy(hypothalamus_quantum_bridge_t* bridge);

/**
 * @brief Reset quantum bridge state
 *
 * WHAT: Reset bridge to initial state
 * WHY:  Clear caches and reset optimization state
 * HOW:  Reinitialize internal state, clear cached results
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on failure
 */
int hypothalamus_quantum_bridge_reset(hypothalamus_quantum_bridge_t* bridge);

/*=============================================================================
 * HOMEOSTATIC OPTIMIZATION
 *===========================================================================*/

/**
 * @brief Optimize homeostatic regulation
 *
 * WHAT: Find optimal regulatory actions using quantum optimization
 * WHY:  Balance multiple competing homeostatic goals simultaneously
 * HOW:  Formulate QUBO and solve using quantum annealing
 *
 * @param bridge Bridge instance
 * @param objective Optimization objective with weights and targets
 * @param constraints Regulatory constraints
 * @param result Output optimization result
 * @return 0 on success, -1 on failure
 */
int hypothalamus_quantum_optimize_homeostasis(
    hypothalamus_quantum_bridge_t* bridge,
    const homeostatic_objective_t* objective,
    const regulatory_constraint_t* constraints,
    optimization_result_t* result);

/**
 * @brief Apply optimized regulatory actions
 *
 * WHAT: Apply optimization results to hypothalamus
 * WHY:  Update hypothalamus state based on quantum optimization
 * HOW:  Transfer optimal actions to adapter
 *
 * @param bridge Bridge instance
 * @param result Optimization result to apply
 * @return 0 on success, -1 on failure
 */
int hypothalamus_quantum_apply_optimization(
    hypothalamus_quantum_bridge_t* bridge,
    const optimization_result_t* result);

/**
 * @brief Get current QUBO formulation
 *
 * WHAT: Get the current QUBO problem formulation
 * WHY:  For debugging and analysis of optimization problem
 * HOW:  Copy internal QUBO to output
 *
 * @param bridge Bridge instance
 * @param qubo Output QUBO structure
 * @return 0 on success, -1 on failure
 */
int hypothalamus_quantum_get_qubo(
    const hypothalamus_quantum_bridge_t* bridge,
    homeostatic_qubo_t* qubo);

/*=============================================================================
 * PARALLEL AUTONOMIC EVALUATION
 *===========================================================================*/

/**
 * @brief Evaluate autonomic candidates in parallel
 *
 * WHAT: Evaluate multiple autonomic states using quantum parallelism
 * WHY:  Find optimal autonomic balance quickly
 * HOW:  Grover-like amplitude amplification for best candidate
 *
 * @param bridge Bridge instance
 * @param input Evaluation input with candidates and criteria
 * @param result Output evaluation result
 * @return 0 on success, -1 on failure
 */
int hypothalamus_quantum_evaluate_autonomic(
    hypothalamus_quantum_bridge_t* bridge,
    const autonomic_evaluation_input_t* input,
    autonomic_evaluation_result_t* result);

/**
 * @brief Estimate quantum speedup for autonomic evaluation
 *
 * @param bridge Bridge instance
 * @param num_candidates Number of candidates to evaluate
 * @return Expected speedup factor (>1 means faster than classical)
 */
float hypothalamus_quantum_estimate_speedup(
    const hypothalamus_quantum_bridge_t* bridge,
    uint32_t num_candidates);

/*=============================================================================
 * CIRCADIAN OPTIMIZATION
 *===========================================================================*/

/**
 * @brief Optimize circadian phase adjustment
 *
 * WHAT: Find optimal schedule for circadian phase shift
 * WHY:  Minimize jet lag, optimize sleep timing
 * HOW:  Quantum walk over possible light/activity schedules
 *
 * @param bridge Bridge instance
 * @param input Optimization input with current and target phase
 * @param result Output optimization result with schedule
 * @return 0 on success, -1 on failure
 */
int hypothalamus_quantum_optimize_circadian(
    hypothalamus_quantum_bridge_t* bridge,
    const circadian_optimization_input_t* input,
    circadian_optimization_result_t* result);

/**
 * @brief Predict circadian adaptation trajectory
 *
 * WHAT: Predict phase trajectory under given conditions
 * WHY:  Plan ahead for circadian adjustments
 * HOW:  Quantum simulation of phase oscillator dynamics
 *
 * @param bridge Bridge instance
 * @param initial_phase Starting phase
 * @param light_schedule Array of light intensities over time
 * @param schedule_length Length of schedule
 * @param predicted_phases Output array of predicted phases
 * @return 0 on success, -1 on failure
 */
int hypothalamus_quantum_predict_circadian(
    hypothalamus_quantum_bridge_t* bridge,
    float initial_phase,
    const float* light_schedule,
    uint32_t schedule_length,
    float* predicted_phases);

/*=============================================================================
 * HPA AXIS OPTIMIZATION
 *===========================================================================*/

/**
 * @brief Optimize HPA axis parameters
 *
 * WHAT: Find optimal HPA axis sensitivity and feedback gains
 * WHY:  Prevent chronic stress, optimize stress response
 * HOW:  VQE for parameter optimization in HPA dynamics
 *
 * @param bridge Bridge instance
 * @param current_hpa Current HPA state
 * @param stress_history Recent stress history (array of levels)
 * @param history_length Length of stress history
 * @param optimal_sensitivity Output optimal HPA sensitivity
 * @param optimal_feedback Output optimal feedback gain
 * @return 0 on success, -1 on failure
 */
int hypothalamus_quantum_optimize_hpa(
    hypothalamus_quantum_bridge_t* bridge,
    const hpa_axis_state_t* current_hpa,
    const float* stress_history,
    uint32_t history_length,
    float* optimal_sensitivity,
    float* optimal_feedback);

/*=============================================================================
 * INTEGRATED UPDATE
 *===========================================================================*/

/**
 * @brief Integrated quantum update
 *
 * WHAT: Perform full quantum-enhanced update cycle
 * WHY:  Single entry point for quantum optimization
 * HOW:  Run all enabled optimizations based on config
 *
 * @param bridge Bridge instance
 * @param delta_time_us Time elapsed
 * @return 0 on success, -1 on failure
 */
int hypothalamus_quantum_update(hypothalamus_quantum_bridge_t* bridge,
                                 uint64_t delta_time_us);

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

/**
 * @brief Get quantum bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int hypothalamus_quantum_get_stats(
    const hypothalamus_quantum_bridge_t* bridge,
    hypothalamus_quantum_stats_t* stats);

/**
 * @brief Check if quantum optimization is available
 *
 * @param bridge Bridge instance
 * @return true if quantum is available and enabled
 */
bool hypothalamus_quantum_is_available(
    const hypothalamus_quantum_bridge_t* bridge);

/**
 * @brief Get current quantum mode
 *
 * @param bridge Bridge instance
 * @return Current optimization mode
 */
hypothalamus_quantum_mode_t hypothalamus_quantum_get_mode(
    const hypothalamus_quantum_bridge_t* bridge);

/**
 * @brief Set quantum mode
 *
 * @param bridge Bridge instance
 * @param mode New optimization mode
 * @return 0 on success, -1 on failure
 */
int hypothalamus_quantum_set_mode(
    hypothalamus_quantum_bridge_t* bridge,
    hypothalamus_quantum_mode_t mode);

/**
 * @brief Get configuration
 *
 * @param bridge Bridge instance
 * @param config Output configuration structure
 * @return 0 on success, -1 on failure
 */
int hypothalamus_quantum_get_config(
    const hypothalamus_quantum_bridge_t* bridge,
    hypothalamus_quantum_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_QUANTUM_BRIDGE_H */
