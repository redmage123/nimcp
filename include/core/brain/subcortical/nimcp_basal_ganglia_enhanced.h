//=============================================================================
// nimcp_basal_ganglia_enhanced.h - Enhanced Basal Ganglia System
//=============================================================================
/**
 * @file nimcp_basal_ganglia_enhanced.h
 * @brief Enhanced basal ganglia integrating all enhancement modules
 *
 * WHAT: Unified enhanced basal ganglia with all advanced features
 * WHY:  Provides biologically-complete action selection with advanced mechanisms
 * HOW:  Integrates beta oscillations, neuromodulators, HRL, model-based planning,
 *       nucleus accumbens, superior colliculus, interneurons, and more
 *
 * ENHANCEMENTS INTEGRATED:
 * 1. Beta oscillations (pathological states, movement suppression)
 * 2. Multi-neuromodulator system (DA, 5HT, ACh, NE, adenosine)
 * 3. Hierarchical RL with options framework
 * 4. Model-based planning with arbitration
 * 5. Nucleus accumbens (motivation, wanting/liking)
 * 6. Superior colliculus (gaze, orienting)
 * 7. Striatal interneurons (FSI, TAN, LTS)
 * 8. Cerebellar coordination
 * 9. Outcome devaluation (goal vs habit testing)
 * 10. Temporal credit assignment (TD-lambda)
 *
 * @version 1.0.0
 * @date 2025-12-30
 */

#ifndef NIMCP_BASAL_GANGLIA_ENHANCED_H
#define NIMCP_BASAL_GANGLIA_ENHANCED_H

#include "core/brain/subcortical/nimcp_basal_ganglia.h"
#include "core/brain/subcortical/nimcp_bg_beta_oscillations.h"
#include "core/brain/subcortical/nimcp_bg_neuromodulators.h"
#include "core/brain/subcortical/nimcp_bg_hierarchical_rl.h"
#include "core/brain/subcortical/nimcp_bg_model_based.h"
#include "core/brain/subcortical/nimcp_nucleus_accumbens.h"
#include "core/brain/subcortical/nimcp_superior_colliculus.h"
#include "core/brain/subcortical/nimcp_striatal_interneurons.h"
#include "core/brain/subcortical/nimcp_bg_cerebellar_coord.h"
#include "core/brain/subcortical/nimcp_bg_outcome_devaluation.h"
#include "core/brain/subcortical/nimcp_bg_temporal_credit.h"
#include "core/brain/subcortical/nimcp_basal_ganglia_training_bridge.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

#define BGE_VERSION_MAJOR 1
#define BGE_VERSION_MINOR 0
#define BGE_VERSION_PATCH 0

/* ============================================================================
 * FEATURE FLAGS
 * ============================================================================ */

/**
 * @brief Enhanced features available
 */
typedef struct {
    bool enable_beta_oscillations;      /**< Beta oscillation dynamics */
    bool enable_multi_neuromod;         /**< Multi-neuromodulator system */
    bool enable_hierarchical_rl;        /**< Hierarchical RL with options */
    bool enable_model_based;            /**< Model-based planning */
    bool enable_nucleus_accumbens;      /**< NAc motivation system */
    bool enable_superior_colliculus;    /**< SC gaze/orienting */
    bool enable_interneurons;           /**< Striatal interneurons */
    bool enable_cerebellar_coord;       /**< Cerebellar coordination */
    bool enable_outcome_deval;          /**< Outcome devaluation */
    bool enable_temporal_credit;        /**< Temporal credit assignment */
    bool enable_training_plasticity;    /**< Training pipeline plasticity */
} bg_enhanced_features_t;

/* ============================================================================
 * CONFIGURATION
 * ============================================================================ */

/**
 * @brief Enhanced BG configuration
 */
typedef struct {
    /* Core BG config */
    basal_ganglia_config_t core_config;

    /* Feature flags */
    bg_enhanced_features_t features;

    /* Module configurations (only used if feature enabled) */
    bg_beta_config_t beta_config;
    bg_neuromod_config_t neuromod_config;
    bg_hrl_config_t hrl_config;
    bg_mb_config_t model_based_config;
    nac_config_t nac_config;
    sc_config_t sc_config;
    sint_config_t interneuron_config;
    bgcb_config_t cerebellar_config;
    bgod_config_t outcome_deval_config;
    bgtc_config_t temporal_credit_config;
    bgtr_bridge_config_t training_config;
} bg_enhanced_config_t;

/* ============================================================================
 * STATISTICS
 * ============================================================================ */

/**
 * @brief Enhanced BG statistics
 */
typedef struct {
    basal_ganglia_stats_t core_stats;

    /* Beta oscillations */
    float current_beta_power;
    bg_beta_state_t beta_state;

    /* Neuromodulators */
    bg_neuromod_levels_t neuromod_levels;
    bg_motive_state_t motive_state;

    /* HRL */
    uint32_t active_options;
    uint32_t option_completions;

    /* Model-based */
    float model_based_weight;
    uint32_t planning_steps;

    /* NAc */
    float wanting_level;
    float liking_level;
    nac_motivation_state_t motivation_state;

    /* Interneurons */
    sint_stats_t interneuron_state;

    /* Goal vs habit */
    bgod_behavior_type_t behavior_type;
    float goal_weight;
    float habit_weight;

    /* Training plasticity */
    bgtr_bridge_stats_t training_stats;
    float last_rpe;
    uint32_t active_traces;
} bg_enhanced_stats_t;

/* ============================================================================
 * MAIN STRUCTURE
 * ============================================================================ */

/**
 * @brief Enhanced basal ganglia system
 */
typedef struct bg_enhanced bg_enhanced_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

/**
 * @brief Initialize default enhanced configuration
 */
void bg_enhanced_default_config(bg_enhanced_config_t* config);

/**
 * @brief Create enhanced BG with all enabled modules
 */
bg_enhanced_t* bg_enhanced_create(const bg_enhanced_config_t* config);

/**
 * @brief Destroy enhanced BG
 */
void bg_enhanced_destroy(bg_enhanced_t* bge);

/**
 * @brief Reset enhanced BG
 */
int bg_enhanced_reset(bg_enhanced_t* bge);

/* ============================================================================
 * CORE ACCESS API
 * ============================================================================ */

/**
 * @brief Get underlying core BG
 */
basal_ganglia_t* bg_enhanced_get_core(bg_enhanced_t* bge);

/**
 * @brief Get beta oscillation system
 */
bg_beta_system_t* bg_enhanced_get_beta(bg_enhanced_t* bge);

/**
 * @brief Get neuromodulator system
 */
bg_neuromod_system_t* bg_enhanced_get_neuromod(bg_enhanced_t* bge);

/**
 * @brief Get hierarchical RL system
 */
bg_hrl_system_t* bg_enhanced_get_hrl(bg_enhanced_t* bge);

/**
 * @brief Get model-based planning system
 */
bg_model_based_t* bg_enhanced_get_model_based(bg_enhanced_t* bge);

/**
 * @brief Get nucleus accumbens
 */
nucleus_accumbens_t* bg_enhanced_get_nac(bg_enhanced_t* bge);

/**
 * @brief Get superior colliculus
 */
superior_colliculus_t* bg_enhanced_get_sc(bg_enhanced_t* bge);

/**
 * @brief Get striatal interneurons
 */
striatal_interneurons_t* bg_enhanced_get_interneurons(bg_enhanced_t* bge);

/**
 * @brief Get cerebellar coordinator
 */
bg_cerebellar_coord_t* bg_enhanced_get_cerebellar(bg_enhanced_t* bge);

/**
 * @brief Get outcome devaluation system
 */
bg_outcome_deval_t* bg_enhanced_get_outcome_deval(bg_enhanced_t* bge);

/**
 * @brief Get temporal credit system
 */
bg_temporal_credit_t* bg_enhanced_get_temporal_credit(bg_enhanced_t* bge);

/**
 * @brief Get training bridge
 */
bgtr_bridge_t* bg_enhanced_get_training_bridge(bg_enhanced_t* bge);

/* ============================================================================
 * ACTION SELECTION API
 * ============================================================================ */

/**
 * @brief Enhanced action selection with all modules
 *
 * This integrates:
 * - Interneuron modulation of striatal activity
 * - Beta oscillation gating
 * - Neuromodulator effects on pathways
 * - HRL option selection (if in option)
 * - Model-based arbitration
 * - NAc motivation influence
 * - Outcome devaluation effects
 *
 * @param bge Enhanced BG system
 * @param cortical_input Cortical input (size num_actions)
 * @param selected_action Output: selected action
 * @return 0 on success
 */
int bg_enhanced_select_action(bg_enhanced_t* bge,
                               const float* cortical_input,
                               uint32_t* selected_action);

/**
 * @brief Select action considering a specific goal
 */
int bg_enhanced_select_action_for_goal(bg_enhanced_t* bge,
                                        const float* cortical_input,
                                        uint32_t goal_id,
                                        uint32_t* selected_action);

/**
 * @brief Get action value considering all factors
 */
float bg_enhanced_get_action_value(const bg_enhanced_t* bge,
                                    uint32_t action_id);

/* ============================================================================
 * HIERARCHICAL RL API
 * ============================================================================ */

/**
 * @brief Start executing an option
 */
int bg_enhanced_start_option(bg_enhanced_t* bge, uint32_t option_id);

/**
 * @brief Check if currently executing an option
 */
bool bg_enhanced_in_option(const bg_enhanced_t* bge);

/**
 * @brief Get primitive action from current option policy
 */
int bg_enhanced_get_option_action(bg_enhanced_t* bge,
                                   uint32_t state,
                                   uint32_t* action);

/* ============================================================================
 * MOTIVATION API
 * ============================================================================ */

/**
 * @brief Process reward through all systems
 *
 * Updates:
 * - Core BG dopamine
 * - Neuromodulator levels
 * - NAc wanting/liking
 * - Temporal credit traces
 *
 * @param bge Enhanced BG system
 * @param reward Reward value
 * @param predicted_reward Expected reward
 * @return 0 on success
 */
int bg_enhanced_process_reward(bg_enhanced_t* bge,
                                float reward,
                                float predicted_reward);

/**
 * @brief Get current motivation level
 */
float bg_enhanced_get_motivation(const bg_enhanced_t* bge);

/**
 * @brief Get wanting level for a reward
 */
float bg_enhanced_get_wanting(const bg_enhanced_t* bge, uint32_t reward_id);

/**
 * @brief Get liking level for a reward
 */
float bg_enhanced_get_liking(const bg_enhanced_t* bge, uint32_t reward_id);

/* ============================================================================
 * MODEL-BASED PLANNING API
 * ============================================================================ */

/**
 * @brief Plan action sequence to goal
 */
int bg_enhanced_plan_to_goal(bg_enhanced_t* bge,
                              uint32_t current_state,
                              uint32_t goal_state,
                              uint32_t* action_sequence,
                              uint32_t* sequence_length);

/**
 * @brief Get model-based vs model-free weight
 */
float bg_enhanced_get_mb_weight(const bg_enhanced_t* bge);

/**
 * @brief Set arbitration between model-based and model-free
 */
int bg_enhanced_set_mb_weight(bg_enhanced_t* bge, float weight);

/* ============================================================================
 * GAZE/ORIENTING API
 * ============================================================================ */

/**
 * @brief Generate saccade command to target
 */
int bg_enhanced_generate_saccade(bg_enhanced_t* bge,
                                  float target_x,
                                  float target_y,
                                  float* saccade_x,
                                  float* saccade_y);

/**
 * @brief Trigger orienting response
 */
int bg_enhanced_orient_to_target(bg_enhanced_t* bge,
                                  float target_x,
                                  float target_y);

/* ============================================================================
 * CEREBELLAR COORDINATION API
 * ============================================================================ */

/**
 * @brief Send action timing to cerebellum
 */
int bg_enhanced_coordinate_timing(bg_enhanced_t* bge,
                                   uint32_t action_id,
                                   float* timing_adjustment);

/**
 * @brief Receive cerebellar prediction
 */
int bg_enhanced_receive_cerebellar_prediction(bg_enhanced_t* bge,
                                               const float* prediction,
                                               uint32_t prediction_dim);

/* ============================================================================
 * OUTCOME DEVALUATION API
 * ============================================================================ */

/**
 * @brief Devalue an outcome (satiety or aversion)
 */
int bg_enhanced_devalue_outcome(bg_enhanced_t* bge,
                                 uint32_t outcome_id,
                                 bgod_method_t method,
                                 float magnitude);

/**
 * @brief Test if behavior is goal-directed or habitual
 */
bgod_behavior_type_t bg_enhanced_test_behavior(bg_enhanced_t* bge,
                                                 uint32_t action_id);

/* ============================================================================
 * PROCESSING API
 * ============================================================================ */

/**
 * @brief Step all enhanced BG subsystems
 */
int bg_enhanced_step(bg_enhanced_t* bge, float dt_ms);

/**
 * @brief Process input through full enhanced pipeline
 */
int bg_enhanced_process_input(bg_enhanced_t* bge,
                               const float* cortical_input);

/* ============================================================================
 * QUERY API
 * ============================================================================ */

/**
 * @brief Get comprehensive statistics
 */
int bg_enhanced_get_stats(const bg_enhanced_t* bge,
                           bg_enhanced_stats_t* stats);

/**
 * @brief Check if a feature is enabled
 */
bool bg_enhanced_feature_enabled(const bg_enhanced_t* bge,
                                  const char* feature_name);

/**
 * @brief Get current beta oscillation power
 */
float bg_enhanced_get_beta_power(const bg_enhanced_t* bge);

/**
 * @brief Get current beta state
 */
bg_beta_state_t bg_enhanced_get_beta_state(const bg_enhanced_t* bge);

/**
 * @brief Get neuromodulator level
 */
float bg_enhanced_get_neuromod_level(const bg_enhanced_t* bge,
                                      bg_neuromod_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BASAL_GANGLIA_ENHANCED_H */
