/**
 * @file nimcp_gust_quantum_bridge.h
 * @brief Gustatory Cortex Quantum Integration Bridge
 * @version 1.0.0
 * @date 2026-01-12
 *
 * WHAT: Quantum algorithm integration for gustatory cortex that leverages
 *       quantum Monte Carlo simulation, quantum walks, and quantum annealing
 *       for optimized taste processing and food evaluation.
 *
 * WHY: Quantum algorithms can enhance gustatory processing:
 *      - QMC for taste receptor threshold calibration
 *      - Quantum walks for flavor space exploration
 *      - Quantum annealing for optimal food preference learning
 *      - MCTS with quantum guidance for food sampling strategies
 *
 * HOW: Provides bridge APIs that connect gustatory processing requests
 *      to quantum backend implementations, managing problem encoding and
 *      result interpretation.
 *
 * QUANTUM APPLICATIONS FOR GUSTATORY:
 * ====================================
 *
 * 1. TASTE THRESHOLD CALIBRATION (QMC):
 *    - Sample taste receptor responses
 *    - Optimize detection thresholds
 *    - Model adaptation dynamics
 *
 * 2. FLAVOR SPACE SEARCH (QUANTUM WALK):
 *    - Food-flavor association search
 *    - Preference prediction
 *    - Nutritional value estimation
 *
 * 3. PREFERENCE OPTIMIZATION (QUANTUM ANNEALING):
 *    - Learned preference optimization
 *    - Disgust threshold tuning
 *    - Reward prediction
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_GUST_QUANTUM_BRIDGE_H
#define NIMCP_GUST_QUANTUM_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module dependencies */
#include "core/brain/regions/gustatory/nimcp_gustatory.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum qubits for gustatory problems */
#define GUST_QUANTUM_MAX_QUBITS          32

/** Default QMC samples */
#define GUST_QUANTUM_DEFAULT_QMC_SAMPLES     500

/** Default quantum walk steps */
#define GUST_QUANTUM_DEFAULT_WALK_STEPS      50

/** Maximum flavor dimensions */
#define GUST_QUANTUM_MAX_FLAVOR_DIM          64

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Quantum algorithm types for gustatory
 */
typedef enum {
    GUST_QUANTUM_ALG_QMC = 0,           /**< Quantum Monte Carlo */
    GUST_QUANTUM_ALG_WALK,              /**< Quantum Walk */
    GUST_QUANTUM_ALG_ANNEAL,            /**< Quantum Annealing */
    GUST_QUANTUM_ALG_VQE,               /**< Variational Quantum Eigensolver */
    GUST_QUANTUM_ALG_COUNT
} gust_quantum_algorithm_t;

/**
 * @brief Problem types for quantum optimization
 */
typedef enum {
    GUST_QUANTUM_PROB_THRESHOLD_CAL = 0,  /**< Threshold calibration */
    GUST_QUANTUM_PROB_FLAVOR_SEARCH,      /**< Flavor space search */
    GUST_QUANTUM_PROB_PREFERENCE_OPT,     /**< Preference optimization */
    GUST_QUANTUM_PROB_REWARD_PRED,        /**< Reward prediction */
    GUST_QUANTUM_PROB_DISGUST_OPT,        /**< Disgust threshold optimization */
    GUST_QUANTUM_PROB_COUNT
} gust_quantum_problem_t;

/**
 * @brief Quantum computation status
 */
typedef enum {
    GUST_QUANTUM_STATUS_IDLE = 0,
    GUST_QUANTUM_STATUS_ENCODING,
    GUST_QUANTUM_STATUS_COMPUTING,
    GUST_QUANTUM_STATUS_DECODING,
    GUST_QUANTUM_STATUS_COMPLETE,
    GUST_QUANTUM_STATUS_ERROR
} gust_quantum_status_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief QMC threshold calibration result
 */
typedef struct {
    float thresholds[TASTE_COUNT];      /**< Calibrated thresholds per taste */
    float sensitivity[TASTE_COUNT];     /**< Computed sensitivity */
    float variance;                      /**< Estimation variance */
    uint32_t samples_used;              /**< Samples used */
    float calibration_quality;          /**< Quality metric [0, 1] */
} gust_qmc_threshold_result_t;

/**
 * @brief Quantum walk flavor search result
 */
typedef struct {
    uint32_t* similar_foods;            /**< Similar food IDs */
    float* similarity_scores;           /**< Similarity scores */
    uint32_t num_similar;               /**< Number of similar */
    food_category_t best_category;      /**< Best matching category */
    float best_score;                   /**< Best similarity score */
    uint32_t steps_taken;               /**< Steps executed */
} gust_quantum_flavor_result_t;

/**
 * @brief Quantum annealing preference result
 */
typedef struct {
    float preferences[TASTE_COUNT];     /**< Optimized preferences */
    float predicted_palatability;       /**< Predicted palatability */
    float predicted_reward;             /**< Predicted reward value */
    float final_energy;                 /**< Final optimization energy */
    bool converged;                      /**< Converged to optimum */
    uint32_t iterations;                /**< Iterations performed */
} gust_quantum_preference_result_t;

/**
 * @brief Reward prediction result
 */
typedef struct {
    float predicted_reward;             /**< Predicted reward [0, 1] */
    float novelty_bonus;                /**< Novelty contribution */
    float satiety_penalty;              /**< Satiety penalty */
    float confidence;                   /**< Prediction confidence */
    float* contribution_weights;        /**< Per-taste contributions */
} gust_quantum_reward_result_t;

/**
 * @brief Problem specification for threshold calibration
 */
typedef struct {
    float current_thresholds[TASTE_COUNT]; /**< Current thresholds */
    float* stimulus_history;              /**< Recent stimulus history */
    uint32_t history_length;              /**< History length */
    float target_sensitivity;             /**< Target sensitivity */
    float adaptation_rate;                /**< Adaptation rate */
} gust_threshold_cal_spec_t;

/**
 * @brief Problem specification for preference optimization
 */
typedef struct {
    float current_preferences[TASTE_COUNT]; /**< Current preferences */
    float recent_rewards[16];               /**< Recent reward history */
    uint32_t num_recent_rewards;            /**< Number of recent rewards */
    float learning_rate;                    /**< Learning rate */
    float satiety_level;                    /**< Current satiety */
} gust_preference_opt_spec_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Quantum bridge configuration
 */
typedef struct {
    /* Algorithm parameters */
    uint32_t qmc_samples;               /**< QMC sample count */
    uint32_t walk_steps;                /**< Quantum walk steps */
    uint32_t anneal_steps;              /**< Annealing steps */
    float anneal_initial_temp;          /**< Initial temperature */
    float anneal_final_temp;            /**< Final temperature */

    /* Precision settings */
    uint32_t max_qubits;                /**< Maximum qubits */
    float convergence_threshold;        /**< Convergence threshold */
    uint32_t max_iterations;            /**< Maximum iterations */

    /* Feature flags */
    bool enable_qmc;                    /**< Enable QMC */
    bool enable_walks;                  /**< Enable quantum walks */
    bool enable_annealing;              /**< Enable annealing */
    bool enable_vqe;                    /**< Enable VQE */
    bool use_classical_fallback;        /**< Fallback to classical */

    /* Performance */
    bool async_computation;             /**< Async computation */
    uint32_t timeout_ms;                /**< Timeout */

    /* Logging */
    bool enable_logging;                /**< Enable logging */
} gust_quantum_config_t;

/**
 * @brief Quantum bridge statistics
 */
typedef struct {
    uint64_t qmc_computations;
    uint64_t walk_computations;
    uint64_t anneal_computations;
    uint64_t vqe_computations;

    uint64_t successful_computations;
    uint64_t failed_computations;
    uint64_t timeouts;

    float avg_qmc_time_ms;
    float avg_walk_time_ms;
    float avg_anneal_time_ms;

    float avg_solution_quality;
    float best_solution_quality;

    uint64_t classical_fallbacks;
} gust_quantum_stats_t;

/* ============================================================================
 * Handle
 * ============================================================================ */

typedef struct gust_quantum_bridge_struct gust_quantum_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int gust_quantum_default_config(gust_quantum_config_t* config);
gust_quantum_bridge_t* gust_quantum_bridge_create(const gust_quantum_config_t* config);
void gust_quantum_bridge_destroy(gust_quantum_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

int gust_quantum_connect(gust_quantum_bridge_t* bridge, nimcp_gustatory_t* gust);
int gust_quantum_disconnect(gust_quantum_bridge_t* bridge);
bool gust_quantum_is_connected(const gust_quantum_bridge_t* bridge);
gust_quantum_status_t gust_quantum_get_status(const gust_quantum_bridge_t* bridge);

/* ============================================================================
 * QMC API
 * ============================================================================ */

int gust_quantum_calibrate_thresholds(gust_quantum_bridge_t* bridge, const gust_threshold_cal_spec_t* spec, gust_qmc_threshold_result_t* result);
int gust_quantum_sample_taste_response(gust_quantum_bridge_t* bridge, basic_taste_t taste, float concentration, uint32_t num_samples, float* responses);

/* ============================================================================
 * Quantum Walk API
 * ============================================================================ */

int gust_quantum_search_flavors(gust_quantum_bridge_t* bridge, const taste_stimulus_t* stimulus, gust_quantum_flavor_result_t* result);
int gust_quantum_predict_food_category(gust_quantum_bridge_t* bridge, const taste_stimulus_t* stimulus, food_category_t* category, float* confidence);

/* ============================================================================
 * Quantum Annealing API
 * ============================================================================ */

int gust_quantum_optimize_preferences(gust_quantum_bridge_t* bridge, const gust_preference_opt_spec_t* spec, gust_quantum_preference_result_t* result);
int gust_quantum_predict_reward(gust_quantum_bridge_t* bridge, const taste_stimulus_t* stimulus, float satiety, gust_quantum_reward_result_t* result);
int gust_quantum_optimize_disgust_threshold(gust_quantum_bridge_t* bridge, float current_threshold, float* optimal);

/* ============================================================================
 * Result Management
 * ============================================================================ */

void gust_qmc_threshold_result_free(gust_qmc_threshold_result_t* result);
void gust_quantum_flavor_result_free(gust_quantum_flavor_result_t* result);
void gust_quantum_preference_result_free(gust_quantum_preference_result_t* result);
void gust_quantum_reward_result_free(gust_quantum_reward_result_t* result);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

int gust_quantum_get_stats(const gust_quantum_bridge_t* bridge, gust_quantum_stats_t* stats);
int gust_quantum_reset_stats(gust_quantum_bridge_t* bridge);
const char* gust_quantum_algorithm_name(gust_quantum_algorithm_t alg);
const char* gust_quantum_problem_name(gust_quantum_problem_t prob);
void gust_quantum_print_summary(const gust_quantum_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GUST_QUANTUM_BRIDGE_H */
