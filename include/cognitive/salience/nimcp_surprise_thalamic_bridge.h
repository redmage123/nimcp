/**
 * @file nimcp_surprise_thalamic_bridge.h
 * @brief Bridge between Surprise Amplifier and thalamic routing system
 * @version 1.0.0
 * @date 2026-01-27
 *
 * WHAT: Thalamic gating and routing of surprise signals
 * WHY:  Surprise signals must be routed to appropriate cortical destinations;
 *       cortical attention feedback adjusts gating weights
 * HOW:  Surprise → thalamic routing; attention → gating weight modulation
 *
 * BIOLOGICAL BASIS:
 * ==========================================================================
 *
 * SURPRISE → THALAMUS:
 * - Surprise signals routed to ACC, anterior insula, pulvinar, PFC
 * - Signal type determines routing priority and destination
 * - High urgency bypasses normal gating (thalamic override)
 * - Reference: Halassa & Kastner (2017) "Thalamic functions in attention"
 *
 * THALAMUS → SURPRISE:
 * - Cortical attention feedback adjusts surprise gating weights
 * - Attended modalities have lower surprise thresholds
 * - Thalamic state modulates surprise sensitivity
 *
 * ERROR CODE RANGE: 28700-28799 (Module-specific)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SURPRISE_THALAMIC_BRIDGE_H
#define NIMCP_SURPRISE_THALAMIC_BRIDGE_H

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
struct nimcp_health_agent;

/* ============================================================================
 * Error Codes (Range: 28700-28799)
 * ============================================================================ */

#define NIMCP_SURPRISE_THALAMIC_ERROR_BASE           28700
#define NIMCP_SURPRISE_THALAMIC_ERROR_NULL_POINTER   (NIMCP_SURPRISE_THALAMIC_ERROR_BASE + 1)
#define NIMCP_SURPRISE_THALAMIC_ERROR_INVALID_PARAM  (NIMCP_SURPRISE_THALAMIC_ERROR_BASE + 2)
#define NIMCP_SURPRISE_THALAMIC_ERROR_NO_MEMORY      (NIMCP_SURPRISE_THALAMIC_ERROR_BASE + 3)
#define NIMCP_SURPRISE_THALAMIC_ERROR_NOT_CONNECTED  (NIMCP_SURPRISE_THALAMIC_ERROR_BASE + 4)

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Signal types (bitmask) */
#define SURPRISE_THALAMIC_REALIZATION    0x01
#define SURPRISE_THALAMIC_CONFLICT       0x02
#define SURPRISE_THALAMIC_NOVELTY        0x04
#define SURPRISE_THALAMIC_HYPOTHESIS     0x08

/** @brief Routing destinations */
#define SURPRISE_THALAMIC_DEST_ACC              0x4002
#define SURPRISE_THALAMIC_DEST_ANTERIOR_INSULA  0x4001
#define SURPRISE_THALAMIC_DEST_PULVINAR         0x4003
#define SURPRISE_THALAMIC_DEST_PFC              0x4004

#define SURPRISE_THALAMIC_DEFAULT_ATTENTION_WEIGHT  1.0f

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Thalamic surprise signal
 */
typedef struct {
    uint32_t signal_type;               /**< Signal type bitmask */
    float surprise_magnitude;           /**< Surprise level [0-1] */
    uint32_t source_module;             /**< Source module ID */
    float urgency;                      /**< Urgency level [0-1] */
    float content[8];                   /**< Signal content payload */
    uint64_t timestamp_ms;              /**< Signal timestamp */
} surprise_thalamic_signal_t;

/**
 * @brief Configuration for surprise-thalamic bridge
 */
typedef struct {
    bool enable_realization;            /**< Route realization signals [true] */
    bool enable_conflict;               /**< Route conflict signals [true] */
    bool enable_novelty;                /**< Route novelty signals [true] */
    bool enable_hypothesis;             /**< Route hypothesis signals [true] */
    float threshold_realization;        /**< Min magnitude for realization routing [0.5] */
    float threshold_conflict;           /**< Min magnitude for conflict routing [0.4] */
    float threshold_novelty;            /**< Min magnitude for novelty routing [0.3] */
    float threshold_hypothesis;         /**< Min magnitude for hypothesis routing [0.6] */
    float attention_weight_default;     /**< Default attention weight [1.0] */
    bool enable_bio_async;              /**< Bio-async messaging [true] */
    bool enable_logging;                /**< Diagnostic logging [true] */
} surprise_thalamic_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t signals_routed;            /**< Total signals routed */
    uint64_t high_priority_routes;      /**< High-priority routing events */
    uint64_t overrides;                 /**< Gating override events */
    uint64_t gating_updates;            /**< Gating weight updates */
    float avg_surprise;                 /**< Running average surprise magnitude */
    uint64_t total_updates;             /**< Total update cycles */
} surprise_thalamic_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct surprise_thalamic_bridge surprise_thalamic_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/** @brief Create default configuration */
surprise_thalamic_config_t surprise_thalamic_bridge_default_config(void);

/** @brief Create bridge (NULL config = defaults) */
surprise_thalamic_bridge_t* surprise_thalamic_bridge_create(
    const surprise_thalamic_config_t* config);

/** @brief Destroy bridge (NULL-safe) */
void surprise_thalamic_bridge_destroy(surprise_thalamic_bridge_t* bridge);

/** @brief Reset state, preserving config and connections */
int surprise_thalamic_bridge_reset(surprise_thalamic_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/** @brief Connect to surprise amplifier */
int surprise_thalamic_bridge_connect_amplifier(
    surprise_thalamic_bridge_t* bridge,
    struct surprise_amplifier* amp);

/** @brief Connect to thalamic router */
int surprise_thalamic_bridge_connect_thalamic_router(
    surprise_thalamic_bridge_t* bridge,
    void* router);

/* ============================================================================
 * Operations API
 * ============================================================================ */

/**
 * @brief Route a surprise signal through the thalamus
 * @param bridge Bridge handle
 * @param signal Signal to route
 * @return 0 on success, error code otherwise
 */
int surprise_thalamic_route_surprise(
    surprise_thalamic_bridge_t* bridge,
    const surprise_thalamic_signal_t* signal);

/**
 * @brief Route a realization event (high-priority)
 * @param bridge Bridge handle
 * @param magnitude Realization magnitude [0-1]
 * @param source_module Source module
 * @return 0 on success, error code otherwise
 */
int surprise_thalamic_route_realization(
    surprise_thalamic_bridge_t* bridge,
    float magnitude,
    uint32_t source_module);

/**
 * @brief Set attention weight for a signal type
 * @param bridge Bridge handle
 * @param signal_type Signal type (single type, not bitmask)
 * @param weight Attention weight
 * @return 0 on success, error code otherwise
 */
int surprise_thalamic_set_attention_weight(
    surprise_thalamic_bridge_t* bridge,
    uint32_t signal_type,
    float weight);

/**
 * @brief Get attention weight for a signal type
 * @return Weight value, 1.0f on error
 */
float surprise_thalamic_get_attention_weight(
    const surprise_thalamic_bridge_t* bridge,
    uint32_t signal_type);

/* ============================================================================
 * Query API
 * ============================================================================ */

/** @brief Get accumulated statistics */
int surprise_thalamic_bridge_get_stats(
    const surprise_thalamic_bridge_t* bridge,
    surprise_thalamic_stats_t* stats_out);

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

/** @brief Set health agent for heartbeat monitoring */
int surprise_thalamic_bridge_set_health_agent(
    surprise_thalamic_bridge_t* bridge,
    struct nimcp_health_agent* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SURPRISE_THALAMIC_BRIDGE_H */
