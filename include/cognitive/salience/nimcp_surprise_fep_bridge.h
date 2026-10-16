/**
 * @file nimcp_surprise_fep_bridge.h
 * @brief Bridge between Surprise Amplifier and Free Energy Principle system
 * @version 1.0.0
 * @date 2026-01-27
 *
 * WHAT: Bidirectional integration between surprise amplifier and FEP system
 * WHY:  FEP prediction errors are a primary source of surprise signals;
 *       high surprise should modulate FEP precision weighting (attention as precision)
 * HOW:  FEP PE → surprise amplifier input; surprise → FEP precision modulation
 *
 * BIOLOGICAL BASIS:
 * ==========================================================================
 *
 * FEP → SURPRISE:
 * - Prediction errors from the FEP system are the primary driver of surprise
 * - Large PE signals trigger the surprise amplifier's re-evaluation cascade
 * - Bayesian surprise (KL divergence) maps directly to information gain
 * - Reference: Itti & Baldi (2009) "Bayesian surprise attracts attention"
 *
 * SURPRISE → FEP:
 * - Surprise modulates FEP precision (attention = precision weighting)
 * - High surprise → increased precision on surprising stimuli
 * - Low surprise → default/reduced precision weighting
 * - Reference: Feldman & Friston (2010) "Attention, uncertainty, free energy"
 *
 * SOCIETY OF THOUGHT CONNECTION:
 * - Kim et al. (2026) showed amplifying surprise feature nearly doubled accuracy
 * - This bridge ensures FEP prediction errors feed into the amplification pipeline
 * - Amplified surprise feeds back to increase FEP precision on salient information
 *
 * ERROR CODE RANGE: 28100-28199 (Module-specific)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SURPRISE_FEP_BRIDGE_H
#define NIMCP_SURPRISE_FEP_BRIDGE_H

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
 * Error Codes (Range: 28100-28199)
 * ============================================================================ */

#define NIMCP_SURPRISE_FEP_ERROR_BASE           28100
#define NIMCP_SURPRISE_FEP_ERROR_NULL_POINTER   (NIMCP_SURPRISE_FEP_ERROR_BASE + 1)
#define NIMCP_SURPRISE_FEP_ERROR_INVALID_PARAM  (NIMCP_SURPRISE_FEP_ERROR_BASE + 2)
#define NIMCP_SURPRISE_FEP_ERROR_NO_MEMORY      (NIMCP_SURPRISE_FEP_ERROR_BASE + 3)
#define NIMCP_SURPRISE_FEP_ERROR_NOT_CONNECTED  (NIMCP_SURPRISE_FEP_ERROR_BASE + 4)

/* ============================================================================
 * Constants
 * ============================================================================ */

#define SURPRISE_FEP_DEFAULT_PRECISION_GAIN      1.5f
#define SURPRISE_FEP_DEFAULT_PE_WEIGHT           1.0f
#define SURPRISE_FEP_DEFAULT_BAYESIAN_WEIGHT     1.1f
#define SURPRISE_FEP_DEFAULT_PRECISION_FLOOR     0.1f
#define SURPRISE_FEP_DEFAULT_PRECISION_CEILING   3.0f
#define SURPRISE_FEP_DEFAULT_PE_THRESHOLD        0.2f

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for the surprise-FEP bridge
 */
typedef struct {
    float precision_gain;           /**< Surprise → precision boost multiplier [1.5] */
    float pe_weight;                /**< Weight of PE-sourced surprise [1.0] */
    float bayesian_weight;          /**< Weight of Bayesian divergence surprise [1.1] */
    float precision_floor;          /**< Min precision multiplier [0.1] */
    float precision_ceiling;        /**< Max precision multiplier [3.0] */
    float pe_threshold;             /**< Min PE to forward to amplifier [0.2] */
    bool enable_precision_modulation; /**< Surprise → FEP precision [true] */
    bool enable_pe_forwarding;      /**< FEP PE → surprise amplifier [true] */
    bool enable_bayesian_forwarding; /**< FEP KL → surprise amplifier [true] */
    bool enable_bio_async;          /**< Bio-async messaging [true] */
    bool enable_logging;            /**< Diagnostic logging [true] */
} surprise_fep_config_t;

/**
 * @brief Effects computed by the bridge
 */
typedef struct {
    float current_precision_boost;  /**< Current FEP precision multiplier */
    float last_pe_forwarded;        /**< Most recent PE sent to amplifier */
    float last_bayesian_forwarded;  /**< Most recent KL sent to amplifier */
    float integrated_surprise;      /**< Running integrated surprise level */
} surprise_fep_effects_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t pe_events_forwarded;       /**< PE events sent to amplifier */
    uint64_t bayesian_events_forwarded; /**< KL events sent to amplifier */
    uint64_t precision_modulations;     /**< Times precision was modulated */
    uint64_t pe_below_threshold;        /**< PE events below threshold (dropped) */
    float avg_pe_forwarded;             /**< Running avg of forwarded PE magnitude */
    float avg_precision_boost;          /**< Running avg precision multiplier */
    float max_precision_boost;          /**< Peak precision boost applied */
    uint64_t total_updates;             /**< Total update cycles */
} surprise_fep_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct surprise_fep_bridge surprise_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/** @brief Create default configuration */
surprise_fep_config_t surprise_fep_bridge_default_config(void);

/** @brief Create bridge (NULL config = defaults) */
surprise_fep_bridge_t* surprise_fep_bridge_create(
    const surprise_fep_config_t* config);

/** @brief Destroy bridge (NULL-safe) */
void surprise_fep_bridge_destroy(surprise_fep_bridge_t* bridge);

/** @brief Reset state, preserving config and connections */
int surprise_fep_bridge_reset(surprise_fep_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/** @brief Connect to surprise amplifier */
int surprise_fep_bridge_connect_amplifier(
    surprise_fep_bridge_t* bridge,
    struct surprise_amplifier* amp);

/** @brief Connect to FEP system */
int surprise_fep_bridge_connect_fep(
    surprise_fep_bridge_t* bridge,
    void* fep);

/* ============================================================================
 * Operations API
 * ============================================================================ */

/**
 * @brief Forward FEP prediction error to surprise amplifier
 * @param bridge Bridge handle
 * @param prediction_error PE magnitude [0-1]
 * @param source_module Module that produced the PE
 * @return 0 on success, error code otherwise
 */
int surprise_fep_forward_pe(
    surprise_fep_bridge_t* bridge,
    float prediction_error,
    uint32_t source_module);

/**
 * @brief Forward Bayesian surprise (KL divergence) to surprise amplifier
 * @param bridge Bridge handle
 * @param kl_divergence KL divergence value (non-negative)
 * @param source_module Module that produced the KL
 * @return 0 on success, error code otherwise
 */
int surprise_fep_forward_bayesian(
    surprise_fep_bridge_t* bridge,
    float kl_divergence,
    uint32_t source_module);

/**
 * @brief Modulate FEP precision based on current surprise level
 * @param bridge Bridge handle
 * @return 0 on success, error code otherwise
 */
int surprise_fep_modulate_precision(surprise_fep_bridge_t* bridge);

/**
 * @brief Get current precision boost multiplier
 * @return Precision multiplier, 1.0f on error
 */
float surprise_fep_get_precision_boost(surprise_fep_bridge_t* bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/** @brief Update bridge: poll surprise level, apply precision modulation */
int surprise_fep_bridge_update(surprise_fep_bridge_t* bridge, float dt_seconds);

/* ============================================================================
 * Query API
 * ============================================================================ */

/** @brief Get current effects */
int surprise_fep_bridge_get_effects(
    const surprise_fep_bridge_t* bridge,
    surprise_fep_effects_t* effects_out);

/** @brief Get accumulated statistics */
int surprise_fep_bridge_get_stats(
    const surprise_fep_bridge_t* bridge,
    surprise_fep_stats_t* stats_out);

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

/** @brief Set health agent for heartbeat monitoring */
int surprise_fep_bridge_set_health_agent(
    surprise_fep_bridge_t* bridge,
    struct nimcp_health_agent* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SURPRISE_FEP_BRIDGE_H */
