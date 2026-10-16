/**
 * @file nimcp_gw_imagination_bridge.h
 * @brief Global Workspace-Imagination Bidirectional Bridge
 * @version 1.0.0
 * @date 2026-01-03
 *
 * WHAT: Bidirectional bridge connecting global workspace (conscious access) with imagination
 * WHY:  Imagination content competes for conscious broadcast; conscious attention boosts imagination
 * HOW:  Full bridge pattern with effects in both directions
 *
 * BIOLOGICAL BASIS:
 * The global workspace and imagination systems are tightly coupled:
 * - Conscious attention amplifies and stabilizes imagined content
 * - Vivid imaginations "ignite" and enter conscious awareness
 * - Imagination content competes with perception for workspace access
 * - Conscious goals direct imagination toward relevant simulations
 *
 * ARCHITECTURE:
 * ```
 * ┌──────────────────────┐                    ┌──────────────────────┐
 * │   GLOBAL WORKSPACE   │                    │  IMAGINATION ENGINE  │
 * │                      │                    │                      │
 * │ • Conscious access   │◄── ignition ──────►│ • Scenario manager   │
 * │ • Broadcast content  │    competition     │ • Latent space       │
 * │ • Salience scoring   │                    │ • World model        │
 * │ • Subscriber notify  │◄── attention ─────│ • Visual generation  │
 * │ • Competition pool   │      boost         │ • Prospective sim    │
 * │                      │                    │                      │
 * └──────────────────────┘                    └──────────────────────┘
 *           │                                           │
 *           └───────────── BRIDGE ──────────────────────┘
 *                    (bidirectional effects)
 * ```
 *
 * EFFECTS:
 * - GW → Imagination:
 *   • Conscious attention boosts imagination vividness
 *   • Broadcast goals direct imagination scenarios
 *   • Salience signals prioritize certain imagined content
 *
 * - Imagination → GW:
 *   • Vivid imaginations compete for workspace access
 *   • Imagination content enters broadcast pool
 *   • Novel/surprising content gets salience boost
 *
 * USAGE:
 * ```c
 * gw_imagination_bridge_t* bridge = gw_imagination_bridge_create(NULL);
 * gw_imagination_connect_global_workspace(bridge, gw);
 * gw_imagination_connect_imagination(bridge, imagination_engine);
 *
 * // In update loop:
 * gw_imagination_update(bridge, delta_time);
 *
 * // Submit imagination content for conscious broadcast
 * gw_imagination_submit_for_broadcast(bridge, scenario_id, salience);
 * ```
 */

#ifndef NIMCP_GW_IMAGINATION_BRIDGE_H
#define NIMCP_GW_IMAGINATION_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/tensor/nimcp_tensor.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
/* global_workspace_t is already a pointer typedef in nimcp_global_workspace.h */
#ifndef GLOBAL_WORKSPACE_T_DEFINED
#define GLOBAL_WORKSPACE_T_DEFINED
typedef struct global_workspace_struct* global_workspace_t;
#endif
struct imagination_engine;
struct imagination_scenario;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Maximum scenarios tracked for workspace competition */
#define GW_IMAG_MAX_COMPETING_SCENARIOS     4

/** Default attention boost factor for conscious imagination */
#define GW_IMAG_DEFAULT_ATTENTION_BOOST     1.5f

/** Default ignition threshold for imagination content */
#define GW_IMAG_DEFAULT_IGNITION_THRESHOLD  0.6f

/** Default salience decay rate per second */
#define GW_IMAG_DEFAULT_SALIENCE_DECAY      0.1f

/*=============================================================================
 * EFFECTS STRUCTURES
 *===========================================================================*/

/**
 * @brief Effects flowing from global workspace to imagination
 *
 * WHAT: Conscious attention and directive signals for imagination
 * WHY:  Conscious focus amplifies and directs imagination
 */
typedef struct {
    /* Attention effects */
    float attention_boost;               /**< Boost factor for imagined content [0.0-2.0] */
    float focus_strength;                /**< How focused attention is [0.0-1.0] */
    bool conscious_goal_active;          /**< Whether there's a conscious goal directing */

    /* Broadcast signals */
    float broadcast_salience;            /**< Current workspace broadcast salience */
    uint32_t broadcast_source_module;    /**< Which module currently broadcasting */
    bool broadcast_relevant_to_imagination; /**< Is broadcast content imagination-relevant? */

    /* Competition context */
    float competition_threshold;         /**< Current workspace ignition threshold */
    uint32_t num_competitors;            /**< How many competing for workspace */
    float top_competitor_strength;       /**< Strength of leading competitor */

    /* Goal direction */
    nimcp_tensor_t* goal_embedding;      /**< Embedding of conscious goal (if any) */
    float goal_relevance;                /**< How relevant current imagination is to goal */
} gw_to_imagination_effects_t;

/**
 * @brief Effects flowing from imagination to global workspace
 *
 * WHAT: Imagination content competing for conscious broadcast
 * WHY:  Vivid/important imaginations should enter consciousness
 */
typedef struct {
    /* Competition entry */
    float submission_strength;           /**< Strength of imagination submission [0.0-1.0] */
    nimcp_tensor_t* content_embedding;   /**< Content to broadcast if wins */
    uint32_t source_scenario_id;         /**< Which scenario this content is from */

    /* Salience signals */
    float novelty_salience;              /**< How novel the imagined content is */
    float emotional_salience;            /**< Emotional importance of content */
    float goal_relevance_salience;       /**< Relevance to current goals */
    float urgency;                       /**< Time-sensitive content urgency */

    /* Content characteristics */
    float vividness;                     /**< How vivid/clear the imagination [0.0-1.0] */
    float coherence;                     /**< Internal consistency [0.0-1.0] */
    bool is_prospective;                 /**< Future-oriented simulation */
    bool is_counterfactual;              /**< What-if scenario */

    /* Workspace request */
    bool requesting_broadcast;           /**< Actively requesting workspace access */
    float request_priority;              /**< Priority of request [0.0-1.0] */
} imagination_to_gw_effects_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Attention parameters */
    float attention_boost_factor;        /**< Multiplier for attention boost [1.0-3.0] */
    float focus_decay_rate;              /**< How fast focus decays [0.0-1.0] */

    /* Competition parameters */
    float ignition_threshold;            /**< Minimum strength for broadcast [0.0-1.0] */
    float salience_weight_novelty;       /**< Weight for novelty salience [0.0-1.0] */
    float salience_weight_emotional;     /**< Weight for emotional salience [0.0-1.0] */
    float salience_weight_goal;          /**< Weight for goal relevance [0.0-1.0] */

    /* Broadcast parameters */
    bool enable_automatic_submission;    /**< Auto-submit vivid imaginations */
    float auto_submit_threshold;         /**< Vividness threshold for auto-submit */
    uint32_t max_pending_submissions;    /**< Max submissions in queue */

    /* Update frequency */
    float update_interval_ms;            /**< Minimum time between updates */

    /* Bio-async */
    bool enable_bio_async;               /**< Enable bio-async messaging */
} gw_imagination_config_t;

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Competition stats */
    uint64_t submissions_made;           /**< Total submissions to workspace */
    uint64_t broadcasts_won;             /**< Times imagination won competition */
    uint64_t broadcasts_lost;            /**< Times imagination lost competition */
    float avg_submission_strength;       /**< Average submission strength */

    /* Attention stats */
    uint64_t attention_boosts_applied;   /**< Times attention boosted imagination */
    float avg_attention_boost;           /**< Average boost magnitude */

    /* Timing */
    uint64_t total_updates;              /**< Total update calls */
    float avg_update_time_ms;            /**< Average update time */
} gw_imagination_stats_t;

/*=============================================================================
 * MAIN BRIDGE STRUCTURE
 *===========================================================================*/

/**
 * @brief Global Workspace-Imagination bridge
 *
 * Coordinates bidirectional communication between global workspace and imagination.
 */
typedef struct gw_imagination_bridge {
    bridge_base_t base;                  /**< MUST be first - base bridge infrastructure */

    /* Connected systems (typed for convenience, also in base) */
    global_workspace_t* global_workspace;
    struct imagination_engine* imagination;

    /* Bidirectional effects */
    gw_to_imagination_effects_t gw_to_imag;
    imagination_to_gw_effects_t imag_to_gw;

    /* Configuration */
    gw_imagination_config_t config;

    /* Pending submissions */
    struct {
        uint32_t scenario_id;
        float strength;
        bool pending;
    } pending_submissions[GW_IMAG_MAX_COMPETING_SCENARIOS];
    uint32_t num_pending;

    /* Statistics */
    gw_imagination_stats_t stats;

    /* Timing */
    uint64_t last_update_time_ms;
} gw_imagination_bridge_t;

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int gw_imagination_default_config(gw_imagination_config_t* config);

/**
 * @brief Validate configuration
 *
 * @param config Configuration to validate
 * @return 0 if valid, -1 if invalid
 */
int gw_imagination_validate_config(const gw_imagination_config_t* config);

/**
 * @brief Create bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on error
 */
gw_imagination_bridge_t* gw_imagination_bridge_create(
    const gw_imagination_config_t* config);

/**
 * @brief Destroy bridge
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void gw_imagination_bridge_destroy(gw_imagination_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * Clears effects and pending submissions, keeps connections and config.
 *
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 */
int gw_imagination_reset(gw_imagination_bridge_t* bridge);

/*=============================================================================
 * CONNECTION API
 *===========================================================================*/

/**
 * @brief Connect global workspace
 *
 * @param bridge Bridge
 * @param gw Global workspace to connect
 * @return 0 on success, -1 on error
 */
int gw_imagination_connect_global_workspace(
    gw_imagination_bridge_t* bridge,
    global_workspace_t* gw);

/**
 * @brief Connect imagination engine
 *
 * @param bridge Bridge
 * @param imagination Imagination engine to connect
 * @return 0 on success, -1 on error
 */
int gw_imagination_connect_imagination(
    gw_imagination_bridge_t* bridge,
    struct imagination_engine* imagination);

/**
 * @brief Disconnect global workspace
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int gw_imagination_disconnect_global_workspace(gw_imagination_bridge_t* bridge);

/**
 * @brief Disconnect imagination
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int gw_imagination_disconnect_imagination(gw_imagination_bridge_t* bridge);

/**
 * @brief Check if bridge is fully connected
 *
 * @param bridge Bridge
 * @return true if both systems connected
 */
bool gw_imagination_is_connected(const gw_imagination_bridge_t* bridge);

/*=============================================================================
 * UPDATE API
 *===========================================================================*/

/**
 * @brief Main update function
 *
 * Computes and applies effects in both directions.
 *
 * @param bridge Bridge
 * @param delta_time_ms Time since last update in milliseconds
 * @return 0 on success, -1 on error
 */
int gw_imagination_update(
    gw_imagination_bridge_t* bridge,
    float delta_time_ms);

/**
 * @brief Compute GW → imagination effects only
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int gw_imagination_compute_gw_effects(gw_imagination_bridge_t* bridge);

/**
 * @brief Compute imagination → GW effects only
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int gw_imagination_compute_imag_effects(gw_imagination_bridge_t* bridge);

/**
 * @brief Apply all computed effects to connected systems
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int gw_imagination_apply_effects(gw_imagination_bridge_t* bridge);

/*=============================================================================
 * WORKSPACE COMPETITION API
 *===========================================================================*/

/**
 * @brief Submit imagination content for workspace broadcast competition
 *
 * Queues imagination scenario for competition in global workspace.
 * Content will compete based on salience and current workspace state.
 *
 * @param bridge Bridge
 * @param scenario_id Source scenario ID
 * @param strength Signal strength for competition [0.0-1.0]
 * @return 0 on success, -1 on error
 */
int gw_imagination_submit_for_broadcast(
    gw_imagination_bridge_t* bridge,
    uint32_t scenario_id,
    float strength);

/**
 * @brief Request conscious attention boost for imagination
 *
 * Requests that conscious attention be directed to imagination,
 * boosting vividness and coherence.
 *
 * @param bridge Bridge
 * @param boost_level Requested boost level [0.0-1.0]
 * @return 0 on success, -1 on error
 */
int gw_imagination_request_attention_boost(
    gw_imagination_bridge_t* bridge,
    float boost_level);

/**
 * @brief Set conscious goal for directed imagination
 *
 * Provides a goal embedding that will bias imagination toward
 * goal-relevant content.
 *
 * @param bridge Bridge
 * @param goal_embedding Goal representation (will be cloned)
 * @return 0 on success, -1 on error
 */
int gw_imagination_set_conscious_goal(
    gw_imagination_bridge_t* bridge,
    const nimcp_tensor_t* goal_embedding);

/**
 * @brief Clear conscious goal
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int gw_imagination_clear_conscious_goal(gw_imagination_bridge_t* bridge);

/**
 * @brief Check if imagination is currently broadcasting
 *
 * @param bridge Bridge
 * @return true if imagination content is in workspace
 */
bool gw_imagination_is_broadcasting(const gw_imagination_bridge_t* bridge);

/**
 * @brief Get current broadcast strength
 *
 * @param bridge Bridge
 * @return Current broadcast strength, 0.0 if not broadcasting
 */
float gw_imagination_get_broadcast_strength(const gw_imagination_bridge_t* bridge);

/*=============================================================================
 * QUERY API
 *===========================================================================*/

/**
 * @brief Get GW → imagination effects
 *
 * @param bridge Bridge
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int gw_imagination_get_gw_effects(
    const gw_imagination_bridge_t* bridge,
    gw_to_imagination_effects_t* effects);

/**
 * @brief Get imagination → GW effects
 *
 * @param bridge Bridge
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int gw_imagination_get_imag_effects(
    const gw_imagination_bridge_t* bridge,
    imagination_to_gw_effects_t* effects);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int gw_imagination_get_stats(
    const gw_imagination_bridge_t* bridge,
    gw_imagination_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int gw_imagination_reset_stats(gw_imagination_bridge_t* bridge);

/**
 * @brief Get number of pending submissions
 *
 * @param bridge Bridge
 * @return Number of pending submissions
 */
uint32_t gw_imagination_get_pending_count(const gw_imagination_bridge_t* bridge);

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int gw_imagination_connect_bio_async(gw_imagination_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int gw_imagination_disconnect_bio_async(gw_imagination_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge
 * @return true if connected
 */
bool gw_imagination_is_bio_async_connected(const gw_imagination_bridge_t* bridge);

/**
 * @brief Process pending bio-async messages
 *
 * @param bridge Bridge
 * @return Number of messages processed
 */
int gw_imagination_process_messages(gw_imagination_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GW_IMAGINATION_BRIDGE_H */
