/**
 * @file nimcp_surprise_plasticity_bridge.h
 * @brief Bridge between Surprise Amplifier and synaptic plasticity system
 * @version 1.0.0
 * @date 2026-01-27
 *
 * WHAT: Bidirectional integration between surprise amplifier and plasticity
 * WHY:  High surprise should boost learning rate, enhance STDP windows,
 *       and increase eligibility traces; learning outcomes feed back to surprise
 * HOW:  Surprise → plasticity modulation; learning outcomes → surprise sensitivity
 *
 * BIOLOGICAL BASIS:
 * ==========================================================================
 *
 * SURPRISE → PLASTICITY:
 * - High surprise increases learning rate via dopaminergic modulation
 * - STDP window expands during unexpected events (novelty-enhanced LTP)
 * - Eligibility traces strengthened by surprise (three-factor learning)
 * - Reference: Frémaux & Gerstner (2016) "Neuromodulated STDP"
 *
 * PLASTICITY → SURPRISE:
 * - Repeated learning from same source reduces surprise (habituation)
 * - Weight changes signal prediction improvement, reducing future surprise
 * - BCM threshold shifts track long-term surprise-learning interaction
 *
 * ERROR CODE RANGE: 28400-28499 (Module-specific)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SURPRISE_PLASTICITY_BRIDGE_H
#define NIMCP_SURPRISE_PLASTICITY_BRIDGE_H

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
 * Error Codes (Range: 28400-28499)
 * ============================================================================ */

#define NIMCP_SURPRISE_PLASTICITY_ERROR_BASE           28400
#define NIMCP_SURPRISE_PLASTICITY_ERROR_NULL_POINTER   (NIMCP_SURPRISE_PLASTICITY_ERROR_BASE + 1)
#define NIMCP_SURPRISE_PLASTICITY_ERROR_INVALID_PARAM  (NIMCP_SURPRISE_PLASTICITY_ERROR_BASE + 2)
#define NIMCP_SURPRISE_PLASTICITY_ERROR_NO_MEMORY      (NIMCP_SURPRISE_PLASTICITY_ERROR_BASE + 3)
#define NIMCP_SURPRISE_PLASTICITY_ERROR_NOT_CONNECTED  (NIMCP_SURPRISE_PLASTICITY_ERROR_BASE + 4)

/* ============================================================================
 * Constants
 * ============================================================================ */

#define SURPRISE_PLASTICITY_DEFAULT_LR_BOOST              2.0f
#define SURPRISE_PLASTICITY_DEFAULT_HABITUATION_RATE      0.05f
#define SURPRISE_PLASTICITY_DEFAULT_HABITUATION_RECOVERY  0.01f
#define SURPRISE_PLASTICITY_DEFAULT_STDP_EXPANSION        1.5f
#define SURPRISE_PLASTICITY_DEFAULT_ELIGIBILITY_BOOST     1.3f
#define SURPRISE_PLASTICITY_DEFAULT_BCM_SHIFT             0.1f
#define SURPRISE_PLASTICITY_DEFAULT_MIN_SURPRISE          0.3f
#define SURPRISE_PLASTICITY_DEFAULT_MAX_SOURCES           64

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for surprise-plasticity bridge
 */
typedef struct {
    float learning_rate_boost;          /**< LR multiplier when surprised [2.0] */
    float habituation_rate;             /**< Per-exposure decay for repeated sources [0.05] */
    float habituation_recovery_rate;    /**< Recovery when source absent [0.01] */
    float stdp_window_expansion;        /**< STDP window multiplier during surprise [1.5] */
    float eligibility_boost;            /**< Eligibility trace enhancement [1.3] */
    float bcm_threshold_shift;          /**< BCM threshold shift from surprise [0.1] */
    float min_surprise_for_boost;       /**< Minimum surprise to trigger [0.3] */
    uint32_t max_tracked_sources;       /**< Max source IDs for habituation [64] */
    bool enable_bio_async;              /**< Bio-async messaging [true] */
    bool enable_logging;                /**< Diagnostic logging [true] */
} surprise_plasticity_config_t;

/**
 * @brief Effects computed by the bridge
 */
typedef struct {
    float learning_rate_multiplier;     /**< Current LR multiplier */
    float stdp_window_multiplier;       /**< Current STDP window multiplier */
    float eligibility_multiplier;       /**< Current eligibility trace multiplier */
    float bcm_shift;                    /**< Current BCM threshold shift */
    float habituation_level;            /**< Average habituation across sources */
    uint32_t active_sources;            /**< Number of tracked sources */
} surprise_plasticity_effects_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t plasticity_boosts;         /**< Times plasticity was boosted */
    uint64_t habituation_events;        /**< Habituation updates applied */
    uint64_t learning_rate_updates;     /**< LR modulation events */
    uint64_t bcm_shifts;               /**< BCM threshold shifts applied */
    uint64_t total_updates;            /**< Total update cycles */
} surprise_plasticity_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct surprise_plasticity_bridge surprise_plasticity_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/** @brief Create default configuration */
surprise_plasticity_config_t surprise_plasticity_bridge_default_config(void);

/** @brief Create bridge (NULL config = defaults) */
surprise_plasticity_bridge_t* surprise_plasticity_bridge_create(
    const surprise_plasticity_config_t* config);

/** @brief Destroy bridge (NULL-safe) */
void surprise_plasticity_bridge_destroy(surprise_plasticity_bridge_t* bridge);

/** @brief Reset state, preserving config and connections */
int surprise_plasticity_bridge_reset(surprise_plasticity_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/** @brief Connect to surprise amplifier */
int surprise_plasticity_bridge_connect_amplifier(
    surprise_plasticity_bridge_t* bridge,
    struct surprise_amplifier* amp);

/** @brief Connect to plasticity system */
int surprise_plasticity_bridge_connect_plasticity(
    surprise_plasticity_bridge_t* bridge,
    void* plasticity);

/** @brief Connect to bio-async router */
int surprise_plasticity_bridge_connect_bio_async(
    surprise_plasticity_bridge_t* bridge,
    void* router);

/** @brief Disconnect from bio-async router */
int surprise_plasticity_bridge_disconnect_bio_async(
    surprise_plasticity_bridge_t* bridge);

/* ============================================================================
 * Operations API
 * ============================================================================ */

/**
 * @brief Process surprise event to modulate plasticity
 * @param bridge Bridge handle
 * @param surprise_level Current surprise magnitude [0-1]
 * @param source_id Source of the surprise event
 * @return 0 on success, error code otherwise
 */
int surprise_plasticity_on_surprise_event(
    surprise_plasticity_bridge_t* bridge,
    float surprise_level,
    uint32_t source_id);

/**
 * @brief Process learning outcome to update surprise sensitivity
 * @param bridge Bridge handle
 * @param weight_change Magnitude of weight change
 * @param source_id Source that produced the learning
 * @return 0 on success, error code otherwise
 */
int surprise_plasticity_on_learning_outcome(
    surprise_plasticity_bridge_t* bridge,
    float weight_change,
    uint32_t source_id);

/** @brief Periodic update */
int surprise_plasticity_bridge_update(
    surprise_plasticity_bridge_t* bridge,
    float dt_seconds);

/* ============================================================================
 * Query API
 * ============================================================================ */

/** @brief Get current effects */
int surprise_plasticity_bridge_get_effects(
    const surprise_plasticity_bridge_t* bridge,
    surprise_plasticity_effects_t* effects_out);

/** @brief Get accumulated statistics */
int surprise_plasticity_bridge_get_stats(
    const surprise_plasticity_bridge_t* bridge,
    surprise_plasticity_stats_t* stats_out);

/** @brief Get habituation level for a specific source */
float surprise_plasticity_get_habituation_for_source(
    const surprise_plasticity_bridge_t* bridge,
    uint32_t source_id);

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

/** @brief Set health agent for heartbeat monitoring */
int surprise_plasticity_bridge_set_health_agent(
    surprise_plasticity_bridge_t* bridge,
    struct nimcp_health_agent* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SURPRISE_PLASTICITY_BRIDGE_H */
