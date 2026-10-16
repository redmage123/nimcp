/**
 * @file nimcp_prefrontal_imagination_bridge.h
 * @brief Prefrontal-Imagination Bidirectional Bridge
 * @version 1.0.0
 * @date 2026-01-03
 *
 * WHAT: Bidirectional bridge connecting prefrontal cortex (executive) with imagination engine
 * WHY:  Executive control guides imagination; imagination provides options for decision-making
 * HOW:  Full bridge pattern with effects in both directions
 *
 * BIOLOGICAL BASIS:
 * The prefrontal cortex and default mode network (imagination) interact during:
 * - Goal-directed imagination: PFC constrains and guides imaginative scenarios
 * - Decision-making: Imagination generates options for PFC to evaluate
 * - Prospective planning: PFC uses imagination to simulate future outcomes
 * - Inhibitory control: PFC can suppress inappropriate imagined actions
 * - Working memory: Imagination updates and PFC-maintained representations interact
 *
 * ARCHITECTURE:
 * ```
 * ┌──────────────────────┐                    ┌──────────────────────┐
 * │  PREFRONTAL CORTEX   │                    │  IMAGINATION ENGINE  │
 * │                      │                    │                      │
 * │ • Goal maintenance   │◄─── constraints ───│ • Scenario manager   │
 * │ • Decision-making    │      & mode sel    │ • Latent space       │
 * │ • Working memory     │                    │ • World model        │
 * │ • Inhibitory control │◄──── options ──────│ • Option generation  │
 * │ • Planning           │      & updates     │ • Prospective sim    │
 * │                      │                    │                      │
 * └──────────────────────┘                    └──────────────────────┘
 *           │                                           │
 *           └───────────── BRIDGE ──────────────────────┘
 *                    (bidirectional effects)
 * ```
 *
 * EFFECTS:
 * - Prefrontal → Imagination:
 *   • Goal constraints filter and guide scenario generation
 *   • Mode selection (exploratory vs focused imagination)
 *   • Inhibition control suppresses inappropriate scenarios
 *   • Working memory context seeds imagination
 *
 * - Imagination → Prefrontal:
 *   • Imagined options for decision-making evaluation
 *   • Working memory updates from scenario outcomes
 *   • Novel goal suggestions from creative exploration
 *   • Risk/reward estimates from simulated futures
 *
 * USAGE:
 * ```c
 * prefrontal_imagination_bridge_t* bridge = prefrontal_imagination_bridge_create(NULL);
 * prefrontal_imagination_connect_prefrontal(bridge, prefrontal);
 * prefrontal_imagination_connect_imagination(bridge, imagination_engine);
 *
 * // In update loop:
 * prefrontal_imagination_update(bridge, delta_time);
 *
 * // Request goal-directed imagination
 * prefrontal_imagination_request_goal_options(bridge, goal, num_options);
 * ```
 */

#ifndef NIMCP_PREFRONTAL_IMAGINATION_BRIDGE_H
#define NIMCP_PREFRONTAL_IMAGINATION_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/tensor/nimcp_tensor.h"
#include "async/nimcp_bio_messages.h"
#include "cognitive/imagination/nimcp_imagination_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct prefrontal_adapter;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Maximum options to generate per decision request */
#define PFC_IMAG_MAX_OPTIONS                8

/** Maximum goals to track for imagination guidance */
#define PFC_IMAG_MAX_TRACKED_GOALS          4

/** Maximum working memory items to consider */
#define PFC_IMAG_MAX_WM_ITEMS               7

/** Default inhibition threshold for scenario filtering */
#define PFC_IMAG_DEFAULT_INHIBITION_THRESHOLD   0.6f

/** Default goal relevance threshold */
#define PFC_IMAG_DEFAULT_GOAL_RELEVANCE         0.4f

/*=============================================================================
 * PFC-IMAGINATION MODE CONTROL
 *===========================================================================*/

/**
 * @brief PFC imagination control mode
 *
 * NOTE: This maps to pfc_imagination_mode_t but provides PFC-centric semantics
 */
typedef enum {
    PFC_IMAG_MODE_EXPLORATORY = IMAGINATION_MODE_PASSIVE,   /**< Free exploration, low constraints */
    PFC_IMAG_MODE_FOCUSED = IMAGINATION_MODE_PROSPECTIVE,   /**< Goal-focused, high constraints */
    PFC_IMAG_MODE_EVALUATIVE = IMAGINATION_MODE_PROSPECTIVE,/**< Option evaluation mode */
    PFC_IMAG_MODE_PROSPECTIVE = IMAGINATION_MODE_PROSPECTIVE,/**< Future simulation mode */
    PFC_IMAG_MODE_INHIBITED = IMAGINATION_MODE_PASSIVE      /**< Imagination suppressed (special handling) */
} pfc_imagination_mode_t;

/*=============================================================================
 * EFFECTS STRUCTURES
 *===========================================================================*/

/**
 * @brief Effects flowing from prefrontal to imagination
 *
 * WHAT: Executive control signals for imagination guidance
 * WHY:  Imagination needs goal-directed constraints
 */
typedef struct {
    /* Goal constraints */
    float goal_relevance_threshold;      /**< Minimum goal relevance [0.0-1.0] */
    uint32_t active_goal_ids[PFC_IMAG_MAX_TRACKED_GOALS]; /**< Currently active goals */
    uint32_t num_active_goals;           /**< Number of active goals */
    nimcp_tensor_t* goal_embeddings;     /**< Concatenated goal representations */

    /* Mode control */
    pfc_imagination_mode_t current_mode;     /**< PFC-selected imagination mode */
    float exploration_level;             /**< Exploration vs exploitation [0.0-1.0] */
    float creativity_bound;              /**< Upper bound on novelty [0.0-1.0] */

    /* Inhibition signals */
    float inhibition_strength;           /**< Global inhibition level [0.0-1.0] */
    bool suppress_negative_scenarios;    /**< Filter negative/threatening content */
    bool suppress_off_task;              /**< Filter goal-irrelevant content */

    /* Working memory context */
    uint32_t wm_item_ids[PFC_IMAG_MAX_WM_ITEMS]; /**< Relevant WM items */
    uint32_t num_wm_items;               /**< Number of WM items */
    nimcp_tensor_t* wm_context;          /**< Working memory context embedding */

    /* Decision support request */
    bool options_requested;              /**< Whether options generation is active */
    uint32_t num_options_requested;      /**< Number of options to generate */
} prefrontal_to_imagination_effects_t;

/**
 * @brief Effects flowing from imagination to prefrontal
 *
 * WHAT: Imagination outputs for executive evaluation
 * WHY:  PFC needs options and simulated outcomes for decisions
 */
typedef struct {
    /* Generated options for decision-making */
    uint32_t num_options_generated;      /**< Number of options available */
    uint32_t option_scenario_ids[PFC_IMAG_MAX_OPTIONS]; /**< Scenario IDs for options */
    float option_values[PFC_IMAG_MAX_OPTIONS];   /**< Expected value per option */
    float option_risks[PFC_IMAG_MAX_OPTIONS];    /**< Risk estimate per option */
    float option_novelty[PFC_IMAG_MAX_OPTIONS];  /**< Novelty score per option */

    /* Best option summary */
    uint32_t best_option_idx;            /**< Index of highest-value option */
    nimcp_tensor_t* best_option_embedding; /**< Embedding of best option */

    /* Working memory update suggestions */
    bool wm_update_suggested;            /**< Whether WM update is recommended */
    nimcp_tensor_t* wm_update_content;   /**< Content to add to WM */
    float wm_update_priority;            /**< Priority for WM update [0.0-1.0] */

    /* Goal suggestions */
    bool new_goal_suggested;             /**< Whether a new goal emerged */
    float suggested_goal_value;          /**< Value of suggested goal */
    nimcp_tensor_t* suggested_goal_embedding; /**< Embedding of suggested goal */

    /* Simulation outcomes */
    float simulated_reward;              /**< Average simulated reward */
    float simulated_risk;                /**< Average simulated risk */
    float confidence;                    /**< Confidence in simulations [0.0-1.0] */
} imagination_to_prefrontal_effects_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Goal guidance parameters */
    float goal_relevance_threshold;      /**< Minimum goal relevance [0.0-1.0] */
    float goal_constraint_weight;        /**< How much goals constrain imagination */
    bool enable_goal_tracking;           /**< Track active goals for guidance */

    /* Mode control parameters */
    pfc_imagination_mode_t default_mode;     /**< Default imagination mode */
    float exploration_default;           /**< Default exploration level [0.0-1.0] */
    float creativity_default;            /**< Default creativity bound [0.0-1.0] */

    /* Inhibition parameters */
    float inhibition_threshold;          /**< Threshold for scenario filtering */
    bool enable_negative_filtering;      /**< Filter negative scenarios by default */
    bool enable_off_task_filtering;      /**< Filter off-task scenarios by default */

    /* Option generation */
    uint32_t default_num_options;        /**< Default number of options to generate */
    float option_diversity_weight;       /**< Weight for option diversity [0.0-1.0] */

    /* Working memory integration */
    bool enable_wm_context;              /**< Use WM context for imagination */
    bool enable_wm_updates;              /**< Allow imagination to suggest WM updates */
    uint32_t max_wm_items;               /**< Maximum WM items to consider */

    /* Update frequency */
    float update_interval_ms;            /**< Minimum time between updates */

    /* Bio-async */
    bool enable_bio_async;               /**< Enable bio-async messaging */
} prefrontal_imagination_config_t;

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Option generation stats */
    uint64_t option_requests;            /**< Total option generation requests */
    uint64_t options_generated;          /**< Total options generated */
    float avg_options_per_request;       /**< Average options per request */

    /* Mode usage stats */
    uint64_t mode_changes;               /**< Total mode changes */
    uint64_t time_in_focused_ms;         /**< Time spent in focused mode */
    uint64_t time_in_exploratory_ms;     /**< Time spent in exploratory mode */

    /* Inhibition stats */
    uint64_t scenarios_inhibited;        /**< Scenarios filtered by inhibition */
    uint64_t inhibition_triggers;        /**< Times inhibition was triggered */

    /* Working memory stats */
    uint64_t wm_updates_suggested;       /**< WM update suggestions */
    uint64_t wm_updates_accepted;        /**< WM updates that were accepted */

    /* Goal suggestion stats */
    uint64_t goals_suggested;            /**< New goals suggested */

    /* Timing */
    uint64_t total_updates;              /**< Total update calls */
    float avg_update_time_ms;            /**< Average update time */
} prefrontal_imagination_stats_t;

/*=============================================================================
 * MAIN BRIDGE STRUCTURE
 *===========================================================================*/

/**
 * @brief Prefrontal-Imagination bridge
 *
 * Coordinates bidirectional communication between prefrontal cortex and imagination.
 */
typedef struct prefrontal_imagination_bridge {
    bridge_base_t base;                  /**< MUST be first - base bridge infrastructure */

    /* Connected systems (typed for convenience, also in base) */
    struct prefrontal_adapter* prefrontal;
    struct imagination_engine* imagination;

    /* Bidirectional effects */
    prefrontal_to_imagination_effects_t pfc_to_imag;
    imagination_to_prefrontal_effects_t imag_to_pfc;

    /* Configuration */
    prefrontal_imagination_config_t config;

    /* State tracking */
    pfc_imagination_mode_t current_mode;
    uint32_t active_scenarios[PFC_IMAG_MAX_OPTIONS];
    uint32_t num_active_scenarios;

    /* Pending requests */
    bool options_request_pending;
    uint32_t pending_num_options;

    /* Statistics */
    prefrontal_imagination_stats_t stats;

    /* Timing */
    uint64_t last_update_time_ms;
    uint64_t mode_start_time_ms;
} prefrontal_imagination_bridge_t;

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int prefrontal_imagination_default_config(prefrontal_imagination_config_t* config);

/**
 * @brief Validate configuration
 *
 * @param config Configuration to validate
 * @return 0 if valid, -1 if invalid
 */
int prefrontal_imagination_validate_config(const prefrontal_imagination_config_t* config);

/**
 * @brief Create bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on error
 */
prefrontal_imagination_bridge_t* prefrontal_imagination_bridge_create(
    const prefrontal_imagination_config_t* config);

/**
 * @brief Destroy bridge
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void prefrontal_imagination_bridge_destroy(prefrontal_imagination_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 */
int prefrontal_imagination_reset(prefrontal_imagination_bridge_t* bridge);

/*=============================================================================
 * CONNECTION API
 *===========================================================================*/

/**
 * @brief Connect prefrontal cortex
 *
 * @param bridge Bridge
 * @param prefrontal Prefrontal adapter to connect
 * @return 0 on success, -1 on error
 */
int prefrontal_imagination_connect_prefrontal(
    prefrontal_imagination_bridge_t* bridge,
    struct prefrontal_adapter* prefrontal);

/**
 * @brief Connect imagination engine
 *
 * @param bridge Bridge
 * @param imagination Imagination engine to connect
 * @return 0 on success, -1 on error
 */
int prefrontal_imagination_connect_imagination(
    prefrontal_imagination_bridge_t* bridge,
    struct imagination_engine* imagination);

/**
 * @brief Disconnect prefrontal
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int prefrontal_imagination_disconnect_prefrontal(
    prefrontal_imagination_bridge_t* bridge);

/**
 * @brief Disconnect imagination
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int prefrontal_imagination_disconnect_imagination(
    prefrontal_imagination_bridge_t* bridge);

/**
 * @brief Check if bridge is fully connected
 *
 * @param bridge Bridge
 * @return true if both systems connected
 */
bool prefrontal_imagination_is_connected(const prefrontal_imagination_bridge_t* bridge);

/*=============================================================================
 * UPDATE API
 *===========================================================================*/

/**
 * @brief Main update function
 *
 * @param bridge Bridge
 * @param delta_time_ms Time since last update in milliseconds
 * @return 0 on success, -1 on error
 */
int prefrontal_imagination_update(
    prefrontal_imagination_bridge_t* bridge,
    float delta_time_ms);

/**
 * @brief Compute prefrontal → imagination effects only
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int prefrontal_imagination_compute_pfc_effects(
    prefrontal_imagination_bridge_t* bridge);

/**
 * @brief Compute imagination → prefrontal effects only
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int prefrontal_imagination_compute_imag_effects(
    prefrontal_imagination_bridge_t* bridge);

/**
 * @brief Apply all computed effects to connected systems
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int prefrontal_imagination_apply_effects(prefrontal_imagination_bridge_t* bridge);

/*=============================================================================
 * EXECUTIVE CONTROL API
 *===========================================================================*/

/**
 * @brief Set imagination mode
 *
 * @param bridge Bridge
 * @param mode New imagination mode
 * @return 0 on success, -1 on error
 */
int prefrontal_imagination_set_mode(
    prefrontal_imagination_bridge_t* bridge,
    pfc_imagination_mode_t mode);

/**
 * @brief Get current imagination mode
 *
 * @param bridge Bridge
 * @return Current mode
 */
pfc_imagination_mode_t prefrontal_imagination_get_mode(
    const prefrontal_imagination_bridge_t* bridge);

/**
 * @brief Set inhibition strength
 *
 * @param bridge Bridge
 * @param strength Inhibition strength [0.0-1.0]
 * @return 0 on success, -1 on error
 */
int prefrontal_imagination_set_inhibition(
    prefrontal_imagination_bridge_t* bridge,
    float strength);

/**
 * @brief Update goal constraints
 *
 * @param bridge Bridge
 * @param goal_ids Array of active goal IDs
 * @param num_goals Number of goals
 * @return 0 on success, -1 on error
 */
int prefrontal_imagination_update_goals(
    prefrontal_imagination_bridge_t* bridge,
    const uint32_t* goal_ids,
    uint32_t num_goals);

/*=============================================================================
 * OPTION GENERATION API
 *===========================================================================*/

/**
 * @brief Request decision options from imagination
 *
 * @param bridge Bridge
 * @param goal_id Goal to generate options for
 * @param num_options Number of options to generate
 * @return 0 on success, -1 on error
 */
int prefrontal_imagination_request_options(
    prefrontal_imagination_bridge_t* bridge,
    uint32_t goal_id,
    uint32_t num_options);

/**
 * @brief Get generated options
 *
 * @param bridge Bridge
 * @param scenario_ids Output array of scenario IDs
 * @param values Output array of expected values
 * @param risks Output array of risk estimates
 * @param num_options Output number of options
 * @return 0 on success, -1 on error
 */
int prefrontal_imagination_get_options(
    const prefrontal_imagination_bridge_t* bridge,
    uint32_t* scenario_ids,
    float* values,
    float* risks,
    uint32_t* num_options);

/**
 * @brief Get best option
 *
 * @param bridge Bridge
 * @param scenario_id Output scenario ID
 * @param value Output expected value
 * @return 0 on success, -1 on error
 */
int prefrontal_imagination_get_best_option(
    const prefrontal_imagination_bridge_t* bridge,
    uint32_t* scenario_id,
    float* value);

/*=============================================================================
 * WORKING MEMORY INTEGRATION API
 *===========================================================================*/

/**
 * @brief Update working memory context for imagination
 *
 * @param bridge Bridge
 * @param wm_context Working memory context tensor
 * @return 0 on success, -1 on error
 */
int prefrontal_imagination_update_wm_context(
    prefrontal_imagination_bridge_t* bridge,
    const nimcp_tensor_t* wm_context);

/**
 * @brief Get suggested working memory update
 *
 * @param bridge Bridge
 * @param content Output content tensor
 * @param priority Output priority
 * @return 0 if update available, -1 if none
 */
int prefrontal_imagination_get_wm_suggestion(
    const prefrontal_imagination_bridge_t* bridge,
    nimcp_tensor_t** content,
    float* priority);

/**
 * @brief Accept working memory update suggestion
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int prefrontal_imagination_accept_wm_update(
    prefrontal_imagination_bridge_t* bridge);

/*=============================================================================
 * QUERY API
 *===========================================================================*/

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int prefrontal_imagination_get_stats(
    const prefrontal_imagination_bridge_t* bridge,
    prefrontal_imagination_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int prefrontal_imagination_reset_stats(prefrontal_imagination_bridge_t* bridge);

/**
 * @brief Get current effects from prefrontal
 *
 * @param bridge Bridge
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int prefrontal_imagination_get_pfc_effects(
    const prefrontal_imagination_bridge_t* bridge,
    prefrontal_to_imagination_effects_t* effects);

/**
 * @brief Get current effects from imagination
 *
 * @param bridge Bridge
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int prefrontal_imagination_get_imag_effects(
    const prefrontal_imagination_bridge_t* bridge,
    imagination_to_prefrontal_effects_t* effects);

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int prefrontal_imagination_connect_bio_async(
    prefrontal_imagination_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int prefrontal_imagination_disconnect_bio_async(
    prefrontal_imagination_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge
 * @return true if connected
 */
bool prefrontal_imagination_is_bio_async_connected(
    const prefrontal_imagination_bridge_t* bridge);

/**
 * @brief Process pending bio-async messages
 *
 * @param bridge Bridge
 * @return Number of messages processed
 */
int prefrontal_imagination_process_messages(
    prefrontal_imagination_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PREFRONTAL_IMAGINATION_BRIDGE_H */
