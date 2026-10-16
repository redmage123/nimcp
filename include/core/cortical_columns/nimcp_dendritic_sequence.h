/**
 * @file nimcp_dendritic_sequence.h
 * @brief HTM-inspired Dendritic Sequence Prediction
 *
 * WHAT: Distal dendritic segments learn temporal sequences. Predicted cells
 *       fire with burst output (BAC mechanism), unpredicted cells fire single
 *       spikes (surprise signal).
 * WHY:  Gives predicted sequences temporal advantage — predicted inputs are
 *       processed faster than surprising ones. Enables sequence learning,
 *       phoneme prediction, temporal pattern recognition.
 * HOW:  Each cell has distal segments with presynaptic connections + permanences.
 *       Active segments = prediction. Correct prediction → strengthen. Surprise
 *       → create new segment. Wrong prediction → weaken.
 *
 * Based on Hawkins' HTM sequence memory (Numenta, 2016).
 */

#ifndef NIMCP_DENDRITIC_SEQUENCE_H
#define NIMCP_DENDRITIC_SEQUENCE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Constants
 * ========================================================================= */

#define DENDRITE_SEQ_MAX_SEGMENTS    32   /**< Max predictive segments per cell */
#define DENDRITE_SEQ_SEGMENT_SIZE    20   /**< Synapses per segment (HTM standard) */
#define DENDRITE_SEQ_DEFAULT_CELLS   256
#define DENDRITE_SEQ_DEFAULT_CPC     32   /**< Cells per column */

/* =========================================================================
 * Types
 * ========================================================================= */

/**
 * @brief A single distal dendritic segment that recognizes a temporal pattern.
 *
 * Each segment connects to a set of presynaptic cells. When enough of those
 * cells are active, the segment activates — meaning this cell is "predicted."
 */
typedef struct {
    uint32_t presynaptic_cells[DENDRITE_SEQ_SEGMENT_SIZE]; /**< Source cell IDs */
    float permanences[DENDRITE_SEQ_SEGMENT_SIZE];           /**< Connection strengths [0,1] */
    uint32_t num_synapses;          /**< Active synapse count (≤ SEGMENT_SIZE) */
    float activation_threshold;     /**< Fraction of connected synapses needed (default 0.5) */
    float permanence_threshold;     /**< Min permanence to be "connected" (default 0.5) */
    bool is_active;                 /**< Currently predicting */
} predictive_segment_t;

/**
 * @brief Per-cell predictive state.
 */
typedef struct {
    uint32_t cell_id;
    predictive_segment_t segments[DENDRITE_SEQ_MAX_SEGMENTS];
    uint32_t num_segments;
    bool is_predicted;              /**< At least one segment is active */
    float prediction_strength;      /**< Strongest segment activation [0,1] */
    uint64_t last_predicted_time_us;
    uint64_t last_active_time_us;
} cell_predictive_state_t;

/**
 * @brief Configuration for the sequence prediction manager.
 */
typedef struct {
    uint32_t num_cells;             /**< Total cells (default 256) */
    uint32_t cells_per_column;      /**< Cells per minicolumn (default 32) */
    float permanence_increment;     /**< +delta for active synapse (default 0.1) */
    float permanence_decrement;     /**< -delta for inactive synapse (default 0.02) */
    float initial_permanence;       /**< Starting permanence (default 0.21) */
    float activation_threshold;     /**< Segment activation threshold (default 0.5) */
    float permanence_threshold;     /**< Connected permanence threshold (default 0.5) */
    float predicted_cell_boost;     /**< Apical boost for predicted cells (default 0.8) */
    float latency_advantage_ms;     /**< SNN latency advantage for predicted (default 5.0) */
    uint32_t max_segments_per_cell; /**< Cap on segments (default 32) */
    uint32_t max_synapses_per_segment; /**< Cap on synapses (default 20) */
} dendritic_seq_config_t;

/**
 * @brief Statistics for sequence prediction.
 */
typedef struct {
    uint64_t total_predictions;     /**< Cells predicted before activation */
    uint64_t correct_predictions;   /**< Predicted cell was actually activated */
    uint64_t total_bursts;          /**< Unpredicted activations (surprise) */
    uint64_t total_segments_created;
    uint64_t total_segments_destroyed;
    float prediction_accuracy;      /**< Running EMA of correct/total */
    float surprise_rate;            /**< total_bursts / (bursts + correct) */
} dendritic_seq_stats_t;

/**
 * @brief Sequence prediction manager — manages all cells and their segments.
 */
typedef struct dendritic_sequence_mgr {
    cell_predictive_state_t* cells; /**< Array of cells [num_cells] */
    uint32_t num_cells;
    uint32_t cells_per_column;
    uint32_t num_columns;           /**< = num_cells / cells_per_column */

    /* Previous timestep state (for learning) */
    uint32_t* prev_active_cells;    /**< Cell IDs active last step */
    uint32_t num_prev_active;
    uint32_t* prev_winner_cells;    /**< Winner cells last step */
    uint32_t num_prev_winners;
    uint32_t prev_active_capacity;

    /* Current timestep */
    uint32_t* cur_active_cells;     /**< Cell IDs active this step */
    uint32_t num_cur_active;
    uint32_t* cur_winner_cells;
    uint32_t num_cur_winners;

    /* Learning parameters */
    float permanence_increment;
    float permanence_decrement;
    float initial_permanence;
    float predicted_cell_boost;
    float latency_advantage_ms;

    /* Statistics */
    dendritic_seq_stats_t stats;

    /* Thread safety */
    void* mutex;                    /**< nimcp_mutex_t* */
} dendritic_sequence_mgr_t;

/* =========================================================================
 * API
 * ========================================================================= */

/** Set default configuration values */
void dendritic_seq_config_default(dendritic_seq_config_t* config);

/** Create sequence prediction manager */
dendritic_sequence_mgr_t* dendritic_seq_create(const dendritic_seq_config_t* config);

/** Destroy manager and free all resources */
void dendritic_seq_destroy(dendritic_sequence_mgr_t* mgr);

/**
 * @brief Full timestep: activate columns → compute predictions → learn → advance.
 *
 * @param mgr            Manager
 * @param active_columns Array of active column indices
 * @param num_active     Number of active columns
 * @return 0 on success, -1 on error
 */
int dendritic_seq_step(dendritic_sequence_mgr_t* mgr,
                       const uint32_t* active_columns, uint32_t num_active);

/** Activate columns — predicted cell wins, unpredicted = burst */
int dendritic_seq_activate_columns(dendritic_sequence_mgr_t* mgr,
                                   const uint32_t* active_columns, uint32_t num_active);

/** Compute predictions for next timestep from current active cells */
int dendritic_seq_compute_predictions(dendritic_sequence_mgr_t* mgr);

/** Learn: strengthen correct, create for surprise, weaken incorrect */
int dendritic_seq_learn(dendritic_sequence_mgr_t* mgr);

/** Advance: swap current → previous state */
int dendritic_seq_advance_timestep(dendritic_sequence_mgr_t* mgr);

/** Get list of currently predicted cells */
int dendritic_seq_get_predicted_cells(const dendritic_sequence_mgr_t* mgr,
                                      uint32_t* predicted_cells, uint32_t max_cells,
                                      uint32_t* num_predicted);

/** Check if a specific cell is predicted */
bool dendritic_seq_is_cell_predicted(const dendritic_sequence_mgr_t* mgr,
                                     uint32_t cell_id);

/** Get prediction accuracy (running EMA) */
float dendritic_seq_get_prediction_accuracy(const dendritic_sequence_mgr_t* mgr);

/** Get surprise rate (burst_rate = bursts / (bursts + correct)) */
float dendritic_seq_get_surprise_rate(const dendritic_sequence_mgr_t* mgr);

/** Get full statistics */
int dendritic_seq_get_stats(const dendritic_sequence_mgr_t* mgr,
                            dendritic_seq_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DENDRITIC_SEQUENCE_H */
