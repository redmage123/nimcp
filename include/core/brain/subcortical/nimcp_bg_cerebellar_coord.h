//=============================================================================
// nimcp_bg_cerebellar_coord.h - Basal Ganglia-Cerebellar Coordination
//=============================================================================
/**
 * @file nimcp_bg_cerebellar_coord.h
 * @brief Coordination between basal ganglia and cerebellum
 *
 * BIOLOGICAL BASIS:
 * BG and cerebellum traditionally viewed as parallel, but now known to interact:
 * - BG: Reward-based learning, action selection
 * - Cerebellum: Error-based learning, motor timing
 * - Interaction via thalamus (VL nucleus)
 *
 * COORDINATION MECHANISMS:
 * - Complementary learning: BG selects, cerebellum refines
 * - Sequential handoff: BG initiates, cerebellum executes
 * - Error sharing: Cerebellar errors inform BG RPE
 * - Timing integration: Cerebellum provides timing for BG
 *
 * @version 1.0.0
 * @date 2025-12-30
 */

#ifndef NIMCP_BG_CEREBELLAR_COORD_H
#define NIMCP_BG_CEREBELLAR_COORD_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

#define BGCB_MAX_MOTOR_CHANNELS     32
#define BGCB_MAX_TIMING_INTERVALS   16

/** Coordination weights */
#define BGCB_DEFAULT_BG_WEIGHT      0.6f
#define BGCB_DEFAULT_CB_WEIGHT      0.4f
#define BGCB_HANDOFF_THRESHOLD      0.7f

/* ============================================================================
 * ENUMERATIONS
 * ============================================================================ */

/**
 * @brief Coordination mode
 */
typedef enum {
    BGCB_MODE_PARALLEL,             /**< Both systems active simultaneously */
    BGCB_MODE_SEQUENTIAL,           /**< BG selects, cerebellum refines */
    BGCB_MODE_COMPETITIVE,          /**< Winner-take-all between systems */
    BGCB_MODE_HIERARCHICAL,         /**< BG high-level, cerebellum low-level */
    BGCB_MODE_COUNT
} bgcb_coord_mode_t;

/**
 * @brief Learning signal type
 */
typedef enum {
    BGCB_LEARN_REWARD,              /**< Reward-based (BG primary) */
    BGCB_LEARN_ERROR,               /**< Error-based (cerebellum primary) */
    BGCB_LEARN_COMBINED,            /**< Both signals combined */
    BGCB_LEARN_COUNT
} bgcb_learn_type_t;

/**
 * @brief Motor phase
 */
typedef enum {
    BGCB_PHASE_SELECTION,           /**< Action selection (BG) */
    BGCB_PHASE_PREPARATION,         /**< Motor preparation (both) */
    BGCB_PHASE_EXECUTION,           /**< Execution (cerebellum) */
    BGCB_PHASE_MONITORING,          /**< Error monitoring (both) */
    BGCB_PHASE_COUNT
} bgcb_motor_phase_t;

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================ */

/**
 * @brief Motor command from BG
 */
typedef struct {
    uint32_t action_id;
    float strength;                 /**< Selection strength */
    float confidence;               /**< Selection confidence */
    float expected_reward;
    float timing_hint_ms;           /**< Suggested timing */
} bgcb_bg_command_t;

/**
 * @brief Motor refinement from cerebellum
 */
typedef struct {
    float* motor_gains;             /**< Per-muscle gains */
    uint32_t num_channels;
    float timing_correction_ms;     /**< Timing adjustment */
    float predicted_error;          /**< Predicted execution error */
    float coordination_pattern;     /**< Multi-joint coordination */
} bgcb_cb_refinement_t;

/**
 * @brief Thalamic relay state (VL nucleus)
 */
typedef struct {
    float* bg_contribution;         /**< BG input contribution */
    float* cb_contribution;         /**< Cerebellar input contribution */
    float* combined_output;         /**< Combined to motor cortex */
    uint32_t num_channels;
    float bg_weight;
    float cb_weight;
} bgcb_thalamic_state_t;

/**
 * @brief Error sharing state
 */
typedef struct {
    float bg_rpe;                   /**< BG reward prediction error */
    float cb_motor_error;           /**< Cerebellar motor error */
    float shared_error;             /**< Combined error signal */
    float error_correlation;        /**< Correlation between errors */
} bgcb_error_state_t;

/**
 * @brief Timing coordination
 */
typedef struct {
    float* interval_predictions;    /**< Predicted intervals */
    float* interval_actuals;        /**< Actual intervals */
    uint32_t num_intervals;
    float timing_accuracy;          /**< Overall timing accuracy */
    float sync_phase;               /**< Phase sync between systems */
} bgcb_timing_state_t;

/**
 * @brief Configuration
 */
typedef struct {
    bgcb_coord_mode_t mode;
    uint32_t num_motor_channels;

    float bg_weight;
    float cb_weight;
    float handoff_threshold;

    float error_sharing_weight;
    float timing_integration_rate;

    bool enable_adaptive_weighting;
    bool enable_error_sharing;
    bool enable_timing_sync;
} bgcb_config_t;

/**
 * @brief Statistics
 */
typedef struct {
    bgcb_motor_phase_t current_phase;
    float bg_contribution_avg;
    float cb_contribution_avg;
    float coordination_quality;
    float timing_accuracy;
    uint32_t handoffs;
    float error_correlation;
} bgcb_stats_t;

/**
 * @brief Main handle
 */
typedef struct bg_cerebellar_coord bg_cerebellar_coord_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

void bgcb_default_config(bgcb_config_t* config);
bg_cerebellar_coord_t* bgcb_create(const bgcb_config_t* config);
void bgcb_destroy(bg_cerebellar_coord_t* coord);
int bgcb_reset(bg_cerebellar_coord_t* coord);

/* ============================================================================
 * INPUT API
 * ============================================================================ */

/**
 * @brief Receive BG motor command
 */
int bgcb_receive_bg_command(bg_cerebellar_coord_t* coord,
                             const bgcb_bg_command_t* command);

/**
 * @brief Receive cerebellar refinement
 */
int bgcb_receive_cb_refinement(bg_cerebellar_coord_t* coord,
                                const bgcb_cb_refinement_t* refinement);

/**
 * @brief Update BG reward prediction error
 */
int bgcb_update_bg_rpe(bg_cerebellar_coord_t* coord, float rpe);

/**
 * @brief Update cerebellar motor error
 */
int bgcb_update_cb_error(bg_cerebellar_coord_t* coord, float error);

/* ============================================================================
 * COORDINATION API
 * ============================================================================ */

/**
 * @brief Combine BG and cerebellar contributions
 */
int bgcb_coordinate(bg_cerebellar_coord_t* coord);

/**
 * @brief Get thalamic output (to motor cortex)
 */
int bgcb_get_motor_output(const bg_cerebellar_coord_t* coord,
                           float* output,
                           uint32_t* num_channels);

/**
 * @brief Get current motor phase
 */
bgcb_motor_phase_t bgcb_get_phase(const bg_cerebellar_coord_t* coord);

/**
 * @brief Trigger handoff from BG to cerebellum
 */
int bgcb_trigger_handoff(bg_cerebellar_coord_t* coord);

/**
 * @brief Get shared error signal
 */
int bgcb_get_shared_error(const bg_cerebellar_coord_t* coord,
                           bgcb_error_state_t* error);

/* ============================================================================
 * TIMING API
 * ============================================================================ */

/**
 * @brief Set timing prediction from cerebellum
 */
int bgcb_set_timing_prediction(bg_cerebellar_coord_t* coord,
                                float interval_ms);

/**
 * @brief Report actual timing
 */
int bgcb_report_actual_timing(bg_cerebellar_coord_t* coord,
                               float interval_ms);

/**
 * @brief Get timing state
 */
int bgcb_get_timing_state(const bg_cerebellar_coord_t* coord,
                           bgcb_timing_state_t* timing);

/* ============================================================================
 * PROCESSING API
 * ============================================================================ */

/**
 * @brief Step coordination dynamics
 */
int bgcb_step(bg_cerebellar_coord_t* coord, float dt_ms);

/**
 * @brief Adapt coordination weights based on performance
 */
int bgcb_adapt_weights(bg_cerebellar_coord_t* coord, float performance);

/**
 * @brief Get statistics
 */
int bgcb_get_stats(const bg_cerebellar_coord_t* coord, bgcb_stats_t* stats);

/* ============================================================================
 * LEARNING API
 * ============================================================================ */

/**
 * @brief Update coordination learning
 */
int bgcb_update_learning(bg_cerebellar_coord_t* coord,
                          bgcb_learn_type_t type,
                          float signal);

/**
 * @brief Get learning signal for BG
 */
float bgcb_get_bg_learning_signal(const bg_cerebellar_coord_t* coord);

/**
 * @brief Get learning signal for cerebellum
 */
float bgcb_get_cb_learning_signal(const bg_cerebellar_coord_t* coord);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BG_CEREBELLAR_COORD_H */
