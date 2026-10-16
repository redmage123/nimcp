//=============================================================================
// nimcp_bg_vigor.h - Vigor and Effort Modulation System
//=============================================================================
/**
 * @file nimcp_bg_vigor.h
 * @brief Vigor (intensity) and effort cost computation for action execution
 *
 * WHAT: Models how basal ganglia modulates the intensity/vigor of actions
 * WHY:  Same action can be performed with different intensities based on context
 * HOW:  Integrates motivation, reward expectation, and effort costs
 *
 * BIOLOGICAL BASIS:
 * - Basal ganglia doesn't just select WHAT action, but also HOW to perform it
 * - Vigor: The speed/intensity/force of movement execution
 * - Effort: The cost of performing an action (metabolic, cognitive)
 * - Dopamine modulates vigor:
 *   - High DA: More vigorous, faster movements
 *   - Low DA (Parkinson's): Bradykinesia (slow movements)
 * - Nucleus accumbens integrates effort-benefit calculations
 * - STN involved in effort-based decision making
 *
 * BIDIRECTIONAL DATA FLOW:
 * - Cortex → Vigor: Movement parameters, task demands
 * - Vigor → Cortex: Computed vigor signals for motor output scaling
 * - Motivation → Vigor: Motivational state affects willingness to expend effort
 * - Vigor → Action: Scales action intensity/speed
 * - Dopamine → Vigor: Modulates vigor computation
 * - Effort → Decision: Effort costs fed back to action selection
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#ifndef NIMCP_BG_VIGOR_H
#define NIMCP_BG_VIGOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define BGV_MAX_ACTIONS 64              /**< Maximum actions tracked */
#define BGV_DEFAULT_VIGOR 0.5f          /**< Default vigor level */
#define BGV_MIN_VIGOR 0.1f              /**< Minimum vigor (prevents complete freeze) */
#define BGV_MAX_VIGOR 1.0f              /**< Maximum vigor */
#define BGV_EFFORT_DISCOUNT 0.95f       /**< Temporal discount for delayed effort */

//=============================================================================
// Enums
//=============================================================================

/**
 * @brief Vigor state
 */
typedef enum {
    BGV_STATE_NORMAL = 0,               /**< Normal vigor */
    BGV_STATE_ENHANCED,                 /**< Enhanced (high motivation/reward) */
    BGV_STATE_REDUCED,                  /**< Reduced (fatigue/low motivation) */
    BGV_STATE_BRADYKINETIC,             /**< Very slow (Parkinson's-like) */
    BGV_STATE_HYPERKINETIC              /**< Excessive (mania-like) */
} bgv_state_t;

/**
 * @brief Effort type
 */
typedef enum {
    BGV_EFFORT_PHYSICAL = 0,            /**< Physical/motor effort */
    BGV_EFFORT_COGNITIVE,               /**< Cognitive/mental effort */
    BGV_EFFORT_EMOTIONAL,               /**< Emotional effort */
    BGV_EFFORT_TEMPORAL                 /**< Time/delay cost */
} bgv_effort_type_t;

/**
 * @brief Modulation source
 */
typedef enum {
    BGV_MOD_DOPAMINE = 0,               /**< Dopamine modulation */
    BGV_MOD_MOTIVATION,                 /**< Motivational state */
    BGV_MOD_FATIGUE,                    /**< Fatigue/resource depletion */
    BGV_MOD_URGENCY,                    /**< Task urgency */
    BGV_MOD_REWARD_PROXIMITY            /**< Proximity to reward */
} bgv_modulation_source_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Effort cost for an action
 */
typedef struct {
    uint32_t action_id;                 /**< Action identifier */
    float physical_cost;                /**< Physical effort [0-1] */
    float cognitive_cost;               /**< Cognitive effort [0-1] */
    float emotional_cost;               /**< Emotional effort [0-1] */
    float temporal_cost;                /**< Time cost [0-1] */
    float total_cost;                   /**< Combined effort cost */
    float subjective_cost;              /**< Subjective (dopamine-modulated) cost */
} bgv_effort_t;

/**
 * @brief Action vigor parameters
 */
typedef struct {
    uint32_t action_id;                 /**< Action identifier */
    float base_vigor;                   /**< Base vigor level */
    float current_vigor;                /**< Current computed vigor */
    float vigor_gain;                   /**< Vigor scaling factor */
    bgv_effort_t effort;                /**< Effort costs */

    /* Historical */
    float avg_vigor;                    /**< Average vigor for this action */
    uint32_t execution_count;           /**< Times executed */
    float avg_duration_ms;              /**< Average duration at base vigor */
} bgv_action_vigor_t;

/**
 * @brief Vigor system configuration
 */
typedef struct {
    uint32_t max_actions;               /**< Maximum tracked actions */
    float base_vigor;                   /**< Default base vigor */
    float dopamine_sensitivity;         /**< Vigor sensitivity to dopamine */
    float motivation_weight;            /**< Weight of motivation input */
    float fatigue_rate;                 /**< Rate of fatigue accumulation */
    float recovery_rate;                /**< Rate of vigor recovery */
    float effort_sensitivity;           /**< Sensitivity to effort costs */
    bool enable_effort_discounting;     /**< Enable temporal discounting */
    bool enable_fatigue;                /**< Enable fatigue modeling */
} bgv_config_t;

/**
 * @brief Vigor system statistics
 */
typedef struct {
    float avg_vigor;                    /**< Average vigor across actions */
    float avg_effort;                   /**< Average effort cost */
    bgv_state_t current_state;          /**< Overall vigor state */
    float fatigue_level;                /**< Current fatigue [0-1] */
    float motivation_level;             /**< Current motivation [0-1] */
    uint64_t total_actions;             /**< Total actions computed */
    float dopamine_level;               /**< Current dopamine level */
} bgv_stats_t;

/**
 * @brief Bidirectional data packet for vigor system
 */
typedef struct {
    /* Input (from other systems) */
    float dopamine_level;               /**< Current dopamine level [0-1] */
    float motivation_signal;            /**< Motivation from striosomes [0-1] */
    float urgency_signal;               /**< Task urgency [0-1] */
    float fatigue_input;                /**< External fatigue signal [0-1] */
    float reward_proximity;             /**< How close to reward [0-1] */
    uint32_t action_id;                 /**< Action to compute vigor for */
    bool compute_effort;                /**< Whether to compute effort costs */

    /* Output (to other systems) */
    float computed_vigor;               /**< Computed vigor for action */
    float effort_cost;                  /**< Computed effort cost */
    float motor_scaling;                /**< Scaling factor for motor output */
    float predicted_duration_ms;        /**< Predicted duration at this vigor */
    bgv_state_t vigor_state;            /**< Current vigor state */
    bool action_recommended;            /**< Whether action is worth the effort */
} bgv_bidir_data_t;

/**
 * @brief Vigor system
 */
typedef struct bgv_system bgv_system_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default configuration
 */
void bgv_default_config(bgv_config_t* config);

/**
 * @brief Create vigor system
 */
bgv_system_t* bgv_create(const bgv_config_t* config);

/**
 * @brief Destroy vigor system
 */
void bgv_destroy(bgv_system_t* system);

/**
 * @brief Reset vigor system
 */
int bgv_reset(bgv_system_t* system);

//=============================================================================
// Action Registration Functions
//=============================================================================

/**
 * @brief Register action with effort costs
 * @param system Vigor system
 * @param action_id Action identifier
 * @param physical_cost Physical effort [0-1]
 * @param cognitive_cost Cognitive effort [0-1]
 * @param base_duration_ms Base duration at normal vigor
 * @return 0 on success
 */
int bgv_register_action(bgv_system_t* system,
                         uint32_t action_id,
                         float physical_cost,
                         float cognitive_cost,
                         float base_duration_ms);

/**
 * @brief Set emotional/temporal costs (optional)
 */
int bgv_set_additional_costs(bgv_system_t* system,
                              uint32_t action_id,
                              float emotional_cost,
                              float temporal_cost);

/**
 * @brief Unregister action
 */
int bgv_unregister_action(bgv_system_t* system, uint32_t action_id);

//=============================================================================
// Vigor Computation Functions
//=============================================================================

/**
 * @brief Compute vigor for action
 * @param system Vigor system
 * @param action_id Action to compute vigor for
 * @param vigor Output: computed vigor [0-1]
 * @return 0 on success
 */
int bgv_compute_vigor(bgv_system_t* system,
                       uint32_t action_id,
                       float* vigor);

/**
 * @brief Compute effort cost for action
 * @param system Vigor system
 * @param action_id Action to compute effort for
 * @param effort Output: effort structure
 * @return 0 on success
 */
int bgv_compute_effort(bgv_system_t* system,
                        uint32_t action_id,
                        bgv_effort_t* effort);

/**
 * @brief Get motor output scaling
 * @param system Vigor system
 * @param action_id Action
 * @return Scaling factor for motor output
 */
float bgv_get_motor_scaling(const bgv_system_t* system, uint32_t action_id);

/**
 * @brief Predict action duration based on vigor
 * @param system Vigor system
 * @param action_id Action
 * @return Predicted duration in ms
 */
float bgv_predict_duration(const bgv_system_t* system, uint32_t action_id);

//=============================================================================
// Modulation Functions
//=============================================================================

/**
 * @brief Set dopamine level
 * @param system Vigor system
 * @param dopamine Dopamine level [0-1]
 * @return 0 on success
 */
int bgv_set_dopamine(bgv_system_t* system, float dopamine);

/**
 * @brief Set motivation level
 * @param system Vigor system
 * @param motivation Motivation level [0-1]
 * @return 0 on success
 */
int bgv_set_motivation(bgv_system_t* system, float motivation);

/**
 * @brief Set urgency level
 * @param system Vigor system
 * @param urgency Urgency level [0-1]
 * @return 0 on success
 */
int bgv_set_urgency(bgv_system_t* system, float urgency);

/**
 * @brief Set fatigue level
 * @param system Vigor system
 * @param fatigue Fatigue level [0-1]
 * @return 0 on success
 */
int bgv_set_fatigue(bgv_system_t* system, float fatigue);

/**
 * @brief Set reward proximity
 * @param system Vigor system
 * @param proximity Reward proximity [0-1]
 * @return 0 on success
 */
int bgv_set_reward_proximity(bgv_system_t* system, float proximity);

//=============================================================================
// Bidirectional Data Flow Functions
//=============================================================================

/**
 * @brief Process bidirectional data exchange
 *
 * This is the main interface for bidirectional data flow.
 * Input: dopamine, motivation, urgency, fatigue, action_id
 * Output: computed vigor, effort cost, motor scaling, predicted duration
 *
 * @param system Vigor system
 * @param data Bidirectional data packet (input/output)
 * @return 0 on success
 */
int bgv_process_bidir(bgv_system_t* system, bgv_bidir_data_t* data);

/**
 * @brief Get feedback to action selection (effort-benefit ratio)
 * @param system Vigor system
 * @param action_id Action
 * @param expected_reward Expected reward
 * @return Effort-benefit ratio (higher = more favorable)
 */
float bgv_get_effort_benefit_ratio(const bgv_system_t* system,
                                    uint32_t action_id,
                                    float expected_reward);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Step the vigor system
 * @param system Vigor system
 * @param dt_ms Time step in milliseconds
 * @return 0 on success
 */
int bgv_step(bgv_system_t* system, float dt_ms);

/**
 * @brief Update action statistics after execution
 * @param system Vigor system
 * @param action_id Executed action
 * @param actual_vigor Actual vigor used
 * @param actual_duration_ms Actual duration
 * @return 0 on success
 */
int bgv_update_action_stats(bgv_system_t* system,
                             uint32_t action_id,
                             float actual_vigor,
                             float actual_duration_ms);

/**
 * @brief Apply fatigue from action execution
 * @param system Vigor system
 * @param action_id Executed action
 * @return 0 on success
 */
int bgv_apply_fatigue(bgv_system_t* system, uint32_t action_id);

/**
 * @brief Process recovery (reduce fatigue)
 * @param system Vigor system
 * @param dt_ms Time for recovery
 * @return 0 on success
 */
int bgv_process_recovery(bgv_system_t* system, float dt_ms);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get vigor state
 */
bgv_state_t bgv_get_state(const bgv_system_t* system);

/**
 * @brief Get action vigor parameters
 */
const bgv_action_vigor_t* bgv_get_action(const bgv_system_t* system, uint32_t action_id);

/**
 * @brief Get current fatigue level
 */
float bgv_get_fatigue(const bgv_system_t* system);

/**
 * @brief Get system statistics
 */
int bgv_get_stats(const bgv_system_t* system, bgv_stats_t* stats);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get vigor state name
 */
const char* bgv_state_name(bgv_state_t state);

/**
 * @brief Get effort type name
 */
const char* bgv_effort_type_name(bgv_effort_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BG_VIGOR_H */
