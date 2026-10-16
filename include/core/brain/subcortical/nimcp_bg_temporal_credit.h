//=============================================================================
// nimcp_bg_temporal_credit.h - Temporal Credit Assignment
//=============================================================================
/**
 * @file nimcp_bg_temporal_credit.h
 * @brief Multi-step temporal credit assignment for basal ganglia
 *
 * BIOLOGICAL BASIS:
 * Standard TD learning uses one-step bootstrapping, but the brain needs:
 * - Multi-step credit assignment for delayed rewards
 * - Eligibility traces for temporal bridging
 * - Sequence learning for action chains
 * - Timing cells for interval encoding
 *
 * MECHANISMS:
 * - TD(λ): Eligibility traces with decay
 * - N-step returns: Forward-looking TD
 * - Successor representations: Predictive state features
 * - Interval timing: Striatal timing circuits
 *
 * @version 1.0.0
 * @date 2025-12-30
 */

#ifndef NIMCP_BG_TEMPORAL_CREDIT_H
#define NIMCP_BG_TEMPORAL_CREDIT_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

#define BGTC_MAX_TRACE_LENGTH       256     /**< Maximum trace history */
#define BGTC_MAX_STATES             512     /**< Maximum states */
#define BGTC_MAX_ACTIONS            64      /**< Maximum actions */
#define BGTC_DEFAULT_LAMBDA         0.9f    /**< Default trace decay */
#define BGTC_DEFAULT_GAMMA          0.99f   /**< Default discount */

/** Timing cell parameters */
#define BGTC_NUM_TIMING_CELLS       32      /**< Timing cell population */
#define BGTC_MAX_INTERVAL_MS        10000.0f /**< Maximum timed interval */

/* ============================================================================
 * ENUMERATIONS
 * ============================================================================ */

/**
 * @brief Credit assignment method
 */
typedef enum {
    BGTC_METHOD_TD_LAMBDA,          /**< TD(λ) with eligibility traces */
    BGTC_METHOD_N_STEP,             /**< N-step returns */
    BGTC_METHOD_MONTE_CARLO,        /**< Full episode return */
    BGTC_METHOD_GAE,                /**< Generalized advantage estimation */
    BGTC_METHOD_SUCCESSOR,          /**< Successor representations */
    BGTC_METHOD_COUNT
} bgtc_method_t;

/**
 * @brief Trace type
 */
typedef enum {
    BGTC_TRACE_ACCUMULATING,        /**< Traces accumulate */
    BGTC_TRACE_REPLACING,           /**< Traces replace */
    BGTC_TRACE_DUTCH,               /**< Dutch traces */
    BGTC_TRACE_COUNT
} bgtc_trace_type_t;

/**
 * @brief Timing cell type
 */
typedef enum {
    BGTC_TIMING_RAMPING,            /**< Ramping activity */
    BGTC_TIMING_PEAKING,            /**< Peak at specific time */
    BGTC_TIMING_DECAYING,           /**< Exponential decay */
    BGTC_TIMING_COUNT
} bgtc_timing_type_t;

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================ */

/**
 * @brief Eligibility trace entry
 */
typedef struct {
    uint32_t state;
    uint32_t action;
    float trace;                    /**< Current trace value */
    uint64_t timestamp;             /**< When trace was created */
} bgtc_trace_entry_t;

/**
 * @brief Experience tuple
 */
typedef struct {
    uint32_t state;
    uint32_t action;
    float reward;
    uint32_t next_state;
    bool terminal;
    float td_error;
    uint64_t timestamp;
} bgtc_experience_t;

/**
 * @brief N-step buffer
 */
typedef struct {
    bgtc_experience_t* buffer;
    uint32_t capacity;
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    uint32_t n_steps;
} bgtc_nstep_buffer_t;

/**
 * @brief Timing cell
 */
typedef struct {
    bgtc_timing_type_t type;
    float preferred_interval;       /**< Preferred time interval */
    float current_activity;         /**< Current firing rate */
    float time_since_start;         /**< Time since interval start */
    float precision;                /**< Temporal precision */
} bgtc_timing_cell_t;

/**
 * @brief Successor representation
 */
typedef struct {
    float** sr_matrix;              /**< State x State successor matrix */
    uint32_t num_states;
    float sr_learning_rate;
    float sr_discount;
} bgtc_successor_rep_t;

/**
 * @brief Configuration
 */
typedef struct {
    bgtc_method_t method;
    bgtc_trace_type_t trace_type;

    float lambda;                   /**< Trace decay */
    float gamma;                    /**< Discount factor */
    uint32_t n_steps;               /**< N-step parameter */

    float trace_threshold;          /**< Minimum trace to keep */
    uint32_t max_traces;            /**< Maximum active traces */

    bool enable_timing_cells;
    uint32_t num_timing_cells;
    float timing_precision;

    bool enable_successor_rep;
    float sr_learning_rate;
} bgtc_config_t;

/**
 * @brief Statistics
 */
typedef struct {
    uint32_t active_traces;
    float avg_trace_value;
    float effective_horizon;        /**< Effective credit horizon */
    float avg_td_error;
    uint32_t updates;
    float timing_accuracy;
} bgtc_stats_t;

/**
 * @brief Main handle
 */
typedef struct bg_temporal_credit bg_temporal_credit_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

void bgtc_default_config(bgtc_config_t* config);
bg_temporal_credit_t* bgtc_create(const bgtc_config_t* config);
void bgtc_destroy(bg_temporal_credit_t* tc);
int bgtc_reset(bg_temporal_credit_t* tc);

/* ============================================================================
 * TRACE MANAGEMENT API
 * ============================================================================ */

/**
 * @brief Update trace for state-action
 */
int bgtc_update_trace(bg_temporal_credit_t* tc,
                       uint32_t state,
                       uint32_t action);

/**
 * @brief Decay all traces
 */
int bgtc_decay_traces(bg_temporal_credit_t* tc);

/**
 * @brief Get trace value
 */
float bgtc_get_trace(const bg_temporal_credit_t* tc,
                      uint32_t state,
                      uint32_t action);

/**
 * @brief Clear all traces
 */
int bgtc_clear_traces(bg_temporal_credit_t* tc);

/**
 * @brief Get active traces
 */
int bgtc_get_active_traces(const bg_temporal_credit_t* tc,
                            bgtc_trace_entry_t* traces,
                            uint32_t* count);

/* ============================================================================
 * CREDIT ASSIGNMENT API
 * ============================================================================ */

/**
 * @brief Compute TD error with traces
 */
float bgtc_compute_td_error(bg_temporal_credit_t* tc,
                             float reward,
                             float current_value,
                             float next_value,
                             bool terminal);

/**
 * @brief Apply credit to all traced state-actions
 */
int bgtc_apply_credit(bg_temporal_credit_t* tc,
                       float td_error,
                       float learning_rate,
                       float* value_updates,
                       uint32_t* num_updates);

/**
 * @brief Store experience for N-step
 */
int bgtc_store_experience(bg_temporal_credit_t* tc,
                           const bgtc_experience_t* exp);

/**
 * @brief Get N-step return
 */
float bgtc_get_nstep_return(const bg_temporal_credit_t* tc,
                             uint32_t steps_back);

/**
 * @brief Compute GAE advantages
 */
int bgtc_compute_gae(bg_temporal_credit_t* tc,
                      const float* values,
                      const float* rewards,
                      uint32_t length,
                      float* advantages);

/* ============================================================================
 * TIMING API
 * ============================================================================ */

/**
 * @brief Start timing interval
 */
int bgtc_start_timing(bg_temporal_credit_t* tc);

/**
 * @brief Update timing cells
 */
int bgtc_update_timing(bg_temporal_credit_t* tc, float dt_ms);

/**
 * @brief Get timing cell activities
 */
int bgtc_get_timing_activities(const bg_temporal_credit_t* tc,
                                float* activities,
                                uint32_t* count);

/**
 * @brief Estimate elapsed time
 */
float bgtc_estimate_elapsed_time(const bg_temporal_credit_t* tc);

/**
 * @brief Learn interval timing
 */
int bgtc_learn_interval(bg_temporal_credit_t* tc,
                         float actual_interval_ms);

/* ============================================================================
 * SUCCESSOR REPRESENTATION API
 * ============================================================================ */

/**
 * @brief Update successor representation
 */
int bgtc_update_successor(bg_temporal_credit_t* tc,
                           uint32_t state,
                           uint32_t next_state);

/**
 * @brief Get successor features for state
 */
int bgtc_get_successor_features(const bg_temporal_credit_t* tc,
                                 uint32_t state,
                                 float* features);

/**
 * @brief Compute value from successor and reward weights
 */
float bgtc_successor_value(const bg_temporal_credit_t* tc,
                            uint32_t state,
                            const float* reward_weights);

/* ============================================================================
 * PROCESSING API
 * ============================================================================ */

/**
 * @brief Step temporal credit dynamics
 */
int bgtc_step(bg_temporal_credit_t* tc, float dt_ms);

/**
 * @brief Process reward event
 */
int bgtc_process_reward(bg_temporal_credit_t* tc,
                         float reward,
                         uint32_t delay_steps);

/**
 * @brief Get effective discount for delay
 */
float bgtc_get_effective_discount(const bg_temporal_credit_t* tc,
                                   uint32_t steps);

/**
 * @brief Get statistics
 */
int bgtc_get_stats(const bg_temporal_credit_t* tc, bgtc_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BG_TEMPORAL_CREDIT_H */
