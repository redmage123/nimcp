/**
 * @file nimcp_surprise_substrate_bridge.h
 * @brief Bridge between Surprise Amplifier and metabolic substrate
 * @version 1.0.0
 * @date 2026-01-27
 *
 * WHAT: Metabolic constraints on surprise processing
 * WHY:  Low ATP reduces surprise sensitivity; fatigue increases thresholds;
 *       high surprise increases metabolic demand
 * HOW:  Substrate state → surprise parameter modulation; surprise → metabolic cost
 *
 * BIOLOGICAL BASIS:
 * ==========================================================================
 *
 * SUBSTRATE → SURPRISE:
 * - Low ATP reduces detection sensitivity
 * - Fatigue increases refractory period and decay rate
 * - Metabolic state constrains amplification accuracy
 * - Reference: Attwell & Laughlin (2001) "Energy budget of signaling"
 *
 * SURPRISE → SUBSTRATE:
 * - Processing cost reported to metabolic system
 * - High surprise increases metabolic demand (surprise is expensive)
 * - Sustained surprise leads to substrate fatigue
 *
 * ERROR CODE RANGE: 28600-28699 (Module-specific)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SURPRISE_SUBSTRATE_BRIDGE_H
#define NIMCP_SURPRISE_SUBSTRATE_BRIDGE_H

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
 * Error Codes (Range: 28600-28699)
 * ============================================================================ */

#define NIMCP_SURPRISE_SUBSTRATE_ERROR_BASE           28600
#define NIMCP_SURPRISE_SUBSTRATE_ERROR_NULL_POINTER   (NIMCP_SURPRISE_SUBSTRATE_ERROR_BASE + 1)
#define NIMCP_SURPRISE_SUBSTRATE_ERROR_INVALID_PARAM  (NIMCP_SURPRISE_SUBSTRATE_ERROR_BASE + 2)
#define NIMCP_SURPRISE_SUBSTRATE_ERROR_NO_MEMORY      (NIMCP_SURPRISE_SUBSTRATE_ERROR_BASE + 3)
#define NIMCP_SURPRISE_SUBSTRATE_ERROR_NOT_CONNECTED  (NIMCP_SURPRISE_SUBSTRATE_ERROR_BASE + 4)

/* ============================================================================
 * Constants
 * ============================================================================ */

#define SURPRISE_SUBSTRATE_DEFAULT_DETECT_MULT     1.05f
#define SURPRISE_SUBSTRATE_DEFAULT_AMPLIFY_MULT    1.0f
#define SURPRISE_SUBSTRATE_DEFAULT_DECAY_MULT      0.95f
#define SURPRISE_SUBSTRATE_DEFAULT_REFRACT_MULT    1.1f
#define SURPRISE_SUBSTRATE_DEFAULT_MIN_CAPACITY    0.3f

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for surprise-substrate bridge
 */
typedef struct {
    float detection_sensitivity_mult;   /**< ATP impact on detection [1.05] */
    float amplification_accuracy_mult;  /**< ATP impact on amplification [1.0] */
    float decay_modulation_mult;        /**< Fatigue impact on decay rate [0.95] */
    float refractory_modulation_mult;   /**< Fatigue impact on refractory period [1.1] */
    float min_capacity;                 /**< Minimum operating capacity [0.3] */
    bool enable_bio_async;              /**< Bio-async messaging [true] */
    bool enable_logging;                /**< Diagnostic logging [true] */
} surprise_substrate_config_t;

/**
 * @brief Effects computed by the bridge
 */
typedef struct {
    float detection_sensitivity;        /**< Current detection sensitivity */
    float amplification_accuracy;       /**< Current amplification accuracy */
    float decay_modulation;             /**< Current decay rate modulation */
    float refractory_modulation;        /**< Current refractory period modulation */
    float overall_capacity;             /**< Overall operating capacity [0-1] */
} surprise_substrate_effects_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t modulation_updates;        /**< Modulation parameter updates */
    uint64_t capacity_warnings;         /**< Low capacity warnings */
    uint64_t atp_critical_events;       /**< ATP critically low events */
    uint64_t total_updates;             /**< Total update cycles */
} surprise_substrate_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct surprise_substrate_bridge surprise_substrate_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/** @brief Create default configuration */
surprise_substrate_config_t surprise_substrate_bridge_default_config(void);

/** @brief Create bridge (NULL config = defaults) */
surprise_substrate_bridge_t* surprise_substrate_bridge_create(
    const surprise_substrate_config_t* config);

/** @brief Destroy bridge (NULL-safe) */
void surprise_substrate_bridge_destroy(surprise_substrate_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/** @brief Connect to surprise amplifier */
int surprise_substrate_bridge_connect_amplifier(
    surprise_substrate_bridge_t* bridge,
    struct surprise_amplifier* amp);

/** @brief Connect to substrate system */
int surprise_substrate_bridge_connect_substrate(
    surprise_substrate_bridge_t* bridge,
    void* substrate);

/** @brief Register with bio-async router */
int surprise_substrate_bridge_register_bio_async(
    surprise_substrate_bridge_t* bridge,
    void* router);

/* ============================================================================
 * Operations API
 * ============================================================================ */

/**
 * @brief Update substrate modulation based on current metabolic state
 * @param bridge Bridge handle
 * @param atp_level Current ATP level [0-1]
 * @param fatigue_level Current fatigue level [0-1]
 * @return 0 on success, error code otherwise
 */
int surprise_substrate_bridge_update(
    surprise_substrate_bridge_t* bridge,
    float atp_level,
    float fatigue_level);

/**
 * @brief Get current modulation effects
 * @param bridge Bridge handle
 * @param effects_out Output effects
 * @return 0 on success, error code otherwise
 */
int surprise_substrate_bridge_get_effects(
    const surprise_substrate_bridge_t* bridge,
    surprise_substrate_effects_t* effects_out);

/**
 * @brief Apply computed effects to surprise amplifier parameters
 * @param bridge Bridge handle
 * @return 0 on success, error code otherwise
 */
int surprise_substrate_bridge_apply_effects(
    surprise_substrate_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/** @brief Get accumulated statistics */
int surprise_substrate_bridge_get_stats(
    const surprise_substrate_bridge_t* bridge,
    surprise_substrate_stats_t* stats_out);

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

/** @brief Set health agent for heartbeat monitoring */
int surprise_substrate_bridge_set_health_agent(
    surprise_substrate_bridge_t* bridge,
    struct nimcp_health_agent* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SURPRISE_SUBSTRATE_BRIDGE_H */
