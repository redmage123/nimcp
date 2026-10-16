/**
 * @file nimcp_surprise_gw_bridge.h
 * @brief Bridge between Surprise Amplifier and Global Workspace
 * @version 1.0.0
 * @date 2026-01-27
 *
 * WHAT: Routes amplified surprise events to the Global Workspace for
 *       conscious broadcasting and competition-based integration
 * WHY:  Surprise signals that win GW competition become "realizations" -
 *       the key mechanism Kim et al. (2026) identified for reasoning improvement
 * HOW:  Amplified events are submitted to GW competition; winning entries
 *       are broadcast system-wide, triggering attention shifts and re-evaluation
 *
 * BIOLOGICAL BASIS:
 * ==========================================================================
 *
 * SURPRISE → GLOBAL WORKSPACE:
 * - Baars (1988) Global Workspace Theory: surprise is a strong competitor
 *   for conscious access
 * - High-salience surprise events win the competition for workspace access
 * - Once broadcast, surprise triggers system-wide attention and re-evaluation
 * - P300 ERP: neural correlate of surprise gaining workspace access
 *
 * GW → SURPRISE SENSITIVITY:
 * - Current workspace contents modulate surprise sensitivity
 * - When workspace is already processing surprise, sensitivity decreases
 *   (refractory-like mechanism at the consciousness level)
 * - When workspace is idle, surprise sensitivity is maximal
 *
 * SOCIETY OF THOUGHT CONNECTION:
 * - Kim et al. (2026): surprise/realization feature causes reasoning model
 *   to re-examine its approach, generating diverse thought threads
 * - GW broadcast of surprise → all cognitive agents receive the "realization"
 * - This is the mechanism that converts local surprise into global re-evaluation
 *
 * ERROR CODE RANGE: 28200-28299 (Module-specific)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SURPRISE_GW_BRIDGE_H
#define NIMCP_SURPRISE_GW_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

struct surprise_amplifier;
struct global_workspace_struct;
struct nimcp_health_agent;

/* ============================================================================
 * Error Codes (Range: 28200-28299)
 * ============================================================================ */

#define NIMCP_SURPRISE_GW_ERROR_BASE            28200
#define NIMCP_SURPRISE_GW_ERROR_NULL_POINTER    (NIMCP_SURPRISE_GW_ERROR_BASE + 1)
#define NIMCP_SURPRISE_GW_ERROR_INVALID_PARAM   (NIMCP_SURPRISE_GW_ERROR_BASE + 2)
#define NIMCP_SURPRISE_GW_ERROR_NO_MEMORY       (NIMCP_SURPRISE_GW_ERROR_BASE + 3)
#define NIMCP_SURPRISE_GW_ERROR_NOT_CONNECTED   (NIMCP_SURPRISE_GW_ERROR_BASE + 4)
#define NIMCP_SURPRISE_GW_ERROR_BROADCAST_FAIL  (NIMCP_SURPRISE_GW_ERROR_BASE + 5)

/* ============================================================================
 * Constants
 * ============================================================================ */

#define SURPRISE_GW_DEFAULT_BROADCAST_THRESHOLD  0.4f
#define SURPRISE_GW_DEFAULT_COMPETITION_WEIGHT   2.0f
#define SURPRISE_GW_DEFAULT_COOLDOWN_SECONDS     1.0f
#define SURPRISE_GW_MAX_PENDING_BROADCASTS       8

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for the surprise-GW bridge
 */
typedef struct {
    float broadcast_threshold;     /**< Min surprise to attempt broadcast [0.4] */
    float competition_weight;      /**< Surprise weight in GW competition [2.0] */
    float cooldown_seconds;        /**< Min seconds between broadcasts [1.0] */
    bool enable_broadcast;         /**< Submit surprise to GW [true] */
    bool enable_sensitivity_mod;   /**< GW state modulates sensitivity [true] */
    bool enable_bio_async;         /**< Bio-async messaging [true] */
    bool enable_logging;           /**< Diagnostic logging [true] */
} surprise_gw_config_t;

/**
 * @brief Effects computed by the bridge
 */
typedef struct {
    float last_broadcast_magnitude; /**< Magnitude of last broadcast surprise */
    float sensitivity_modifier;     /**< GW-derived sensitivity modifier [0-1] */
    float time_since_broadcast;     /**< Seconds since last broadcast */
    bool broadcast_pending;         /**< A broadcast is queued */
} surprise_gw_effects_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t broadcasts_submitted;  /**< Surprises submitted to GW competition */
    uint64_t broadcasts_won;        /**< Surprises that won GW competition */
    uint64_t broadcasts_lost;       /**< Surprises that lost GW competition */
    uint64_t broadcasts_cooled;     /**< Surprises dropped by cooldown */
    uint64_t below_threshold;       /**< Surprises below broadcast threshold */
    float avg_broadcast_magnitude;  /**< Avg magnitude of broadcast surprises */
    float max_broadcast_magnitude;  /**< Peak broadcast surprise */
    uint64_t total_updates;         /**< Total update cycles */
} surprise_gw_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct surprise_gw_bridge surprise_gw_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/** @brief Create default configuration */
surprise_gw_config_t surprise_gw_bridge_default_config(void);

/** @brief Create bridge (NULL config = defaults) */
surprise_gw_bridge_t* surprise_gw_bridge_create(
    const surprise_gw_config_t* config);

/** @brief Destroy bridge (NULL-safe) */
void surprise_gw_bridge_destroy(surprise_gw_bridge_t* bridge);

/** @brief Reset state, preserving config and connections */
int surprise_gw_bridge_reset(surprise_gw_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/** @brief Connect to surprise amplifier */
int surprise_gw_bridge_connect_amplifier(
    surprise_gw_bridge_t* bridge,
    struct surprise_amplifier* amp);

/** @brief Connect to global workspace */
int surprise_gw_bridge_connect_gw(
    surprise_gw_bridge_t* bridge,
    struct global_workspace_struct* gw);

/* ============================================================================
 * Operations API
 * ============================================================================ */

/**
 * @brief Submit surprise event for GW broadcasting
 * @param bridge Bridge handle
 * @param magnitude Surprise magnitude [0-1]
 * @param source_type Surprise source enum value
 * @param attention_boost Computed attention boost
 * @param curiosity_boost Computed curiosity boost
 * @return 0 on success (broadcast submitted), error code otherwise
 */
int surprise_gw_submit_broadcast(
    surprise_gw_bridge_t* bridge,
    float magnitude,
    uint32_t source_type,
    float attention_boost,
    float curiosity_boost);

/**
 * @brief Query current GW-derived sensitivity modifier
 * @return Sensitivity modifier [0-1], 1.0f = full sensitivity
 */
float surprise_gw_get_sensitivity(const surprise_gw_bridge_t* bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/** @brief Update bridge: process pending broadcasts, update cooldown */
int surprise_gw_bridge_update(surprise_gw_bridge_t* bridge, float dt_seconds);

/* ============================================================================
 * Query API
 * ============================================================================ */

/** @brief Get current effects */
int surprise_gw_bridge_get_effects(
    const surprise_gw_bridge_t* bridge,
    surprise_gw_effects_t* effects_out);

/** @brief Get accumulated statistics */
int surprise_gw_bridge_get_stats(
    const surprise_gw_bridge_t* bridge,
    surprise_gw_stats_t* stats_out);

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

/** @brief Set health agent for heartbeat monitoring */
int surprise_gw_bridge_set_health_agent(
    surprise_gw_bridge_t* bridge,
    struct nimcp_health_agent* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SURPRISE_GW_BRIDGE_H */
