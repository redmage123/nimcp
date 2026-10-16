/**
 * @file nimcp_visual_jepa_fep_bridge.h
 * @brief Visual JEPA-FEP Integration Bridge
 * @version 1.0.0
 * @date 2025-12-26
 *
 * WHAT: Connect Visual JEPA prediction errors to FEP precision-weighted updates
 * WHY:  JEPA learns by prediction → FEP provides precision weighting framework
 * HOW:  JEPA errors become FEP prediction errors, precision modulates learning
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * JEPA + FEP INTEGRATION:
 * -----------------------
 * Both JEPA and FEP are based on prediction:
 *
 * JEPA:  L = ||z_pred - z_target||²
 * FEP:   F = π × ||o - g(μ)||²  (simplified)
 *
 * Integration insight:
 * - JEPA prediction errors ↔ FEP sensory prediction errors
 * - JEPA latent space ↔ FEP generative model states
 * - FEP precision ↔ JEPA attention/confidence weighting
 *
 * PRECISION-WEIGHTED JEPA:
 * ------------------------
 * Instead of equal weighting, use FEP precision:
 *
 *   L_weighted = π × ||z_pred - z_target||²
 *
 * Where π (precision) is learned based on:
 * - Historical prediction accuracy
 * - Attention/salience at patch location
 * - Neuromodulatory state (dopamine, norepinephrine)
 *
 * BIOLOGICAL BASIS:
 * -----------------
 * Visual cortex implements hierarchical predictive coding:
 * - V1→V2→V4→IT as generative model hierarchy
 * - Prediction errors modulated by attention (precision)
 * - Dopamine signals unexpected rewards/novelty
 * - JEPA captures semantic prediction, FEP provides weighting
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                   VISUAL JEPA-FEP BRIDGE                                  ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                           ║
 * ║   Visual JEPA           JEPA-FEP Bridge            FEP System            ║
 * ║   ┌─────────────┐      ┌───────────────┐      ┌─────────────────┐       ║
 * ║   │ z_pred      │──────│ Error         │──────│ Belief Update   │       ║
 * ║   │ z_target    │      │ Computation   │      │ μ ← μ + K×ε    │       ║
 * ║   │             │      │               │      │                 │       ║
 * ║   │ Patch Mask  │──────│ Precision     │◄─────│ π (precision)   │       ║
 * ║   │             │      │ Weighting     │      │                 │       ║
 * ║   │ Attention   │──────│               │──────│ Attention Map   │       ║
 * ║   └─────────────┘      └───────────────┘      └─────────────────┘       ║
 * ║                                                                           ║
 * ║   FEP → JEPA: Precision modulates patch importance                       ║
 * ║   JEPA → FEP: Prediction errors update visual beliefs                    ║
 * ║                                                                           ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_VISUAL_JEPA_FEP_BRIDGE_H
#define NIMCP_VISUAL_JEPA_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "perception/nimcp_visual_jepa_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/jepa/nimcp_jepa_latent.h"
#include "cognitive/jepa/nimcp_jepa_predictor.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Bio-async module ID for Visual JEPA-FEP bridge */
#define BIO_MODULE_VISUAL_JEPA_FEP              0x0C20

/** @brief Default precision for JEPA predictions */
#define VISUAL_JEPA_FEP_DEFAULT_PRECISION       1.0f

/** @brief Minimum precision to prevent division by zero */
#define VISUAL_JEPA_FEP_MIN_PRECISION           0.01f

/** @brief Maximum precision to prevent overweighting */
#define VISUAL_JEPA_FEP_MAX_PRECISION           100.0f

/** @brief Precision learning rate */
#define VISUAL_JEPA_FEP_PRECISION_LR            0.01f

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for Visual JEPA-FEP bridge
 */
typedef struct {
    /* Precision parameters */
    float initial_precision;            /**< Starting precision value */
    float precision_learning_rate;      /**< How fast precision adapts */
    float precision_decay;              /**< Precision decay per update */

    /* Error propagation */
    bool enable_fep_belief_updates;     /**< Update FEP beliefs from JEPA errors */
    bool enable_precision_weighting;    /**< Use precision for JEPA loss */
    bool enable_attention_precision;    /**< Use attention for precision */

    /* Thresholds */
    float high_pe_threshold;            /**< Threshold for high prediction error */
    float novelty_threshold;            /**< Threshold for novelty detection */

    /* Neuromodulation */
    bool enable_neuromod_precision;     /**< Neuromodulators affect precision */
    float dopamine_precision_gain;      /**< Dopamine → precision boost */
    float norepinephrine_precision_gain; /**< NE → precision boost */
} visual_jepa_fep_config_t;

/**
 * @brief Per-patch precision state
 */
typedef struct {
    float* patch_precision;             /**< Precision per patch location */
    uint32_t num_patches;               /**< Number of patch locations */
    float global_precision;             /**< Overall visual precision */
} visual_jepa_fep_precision_t;

/**
 * @brief FEP effects on JEPA
 */
typedef struct {
    float precision_gain;               /**< Overall precision gain */
    float* attention_weights;           /**< Attention-based weights per patch */
    float learning_rate_modifier;       /**< Precision-based LR scaling */
    float novelty_boost;                /**< Novelty enhances learning */
} visual_jepa_fep_effects_t;

/**
 * @brief JEPA effects on FEP
 */
typedef struct {
    float* prediction_errors;           /**< JEPA errors for FEP */
    uint32_t error_dim;                 /**< Error dimension */
    float total_prediction_error;       /**< Aggregate PE magnitude */
    bool novelty_detected;              /**< High PE = novelty */
} visual_jepa_fep_signals_t;

/**
 * @brief Bridge state
 */
typedef struct {
    float avg_prediction_error;         /**< Running average PE */
    float avg_precision;                /**< Running average precision */
    uint64_t updates_processed;         /**< Total update cycles */
    uint64_t high_pe_events;            /**< Number of high PE events */
    uint64_t novelty_events;            /**< Number of novelty detections */
} visual_jepa_fep_state_t;

/**
 * @brief Statistics
 */
typedef struct {
    float min_pe;                       /**< Minimum prediction error */
    float max_pe;                       /**< Maximum prediction error */
    float avg_pe;                       /**< Average prediction error */
    float precision_variance;           /**< Variance in precision */
    uint64_t total_updates;             /**< Total bridge updates */
} visual_jepa_fep_stats_t;

/**
 * @brief Visual JEPA-FEP bridge
 */
typedef struct visual_jepa_fep_bridge {
    bridge_base_t base;                 /**< MUST be first - bridge pattern */

    /* Configuration */
    visual_jepa_fep_config_t config;

    /* Connected systems */
    visual_jepa_bridge_t* visual_jepa;  /**< Visual JEPA bridge */
    fep_system_t* fep_system;           /**< FEP system */

    /* Precision tracking */
    visual_jepa_fep_precision_t precision;

    /* Current effects */
    visual_jepa_fep_effects_t effects;
    visual_jepa_fep_signals_t signals;

    /* State and stats */
    visual_jepa_fep_state_t state;
    visual_jepa_fep_stats_t stats;

    /* Working buffers */
    float* error_buffer;                /**< Buffer for error computation */
    uint32_t buffer_size;
} visual_jepa_fep_bridge_t;

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
int visual_jepa_fep_bridge_default_config(visual_jepa_fep_config_t* config);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create Visual JEPA-FEP bridge
 *
 * WHAT: Initialize JEPA-FEP integration
 * WHY:  Connect JEPA prediction to FEP precision weighting
 * HOW:  Allocate precision state, buffers, link systems
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
visual_jepa_fep_bridge_t* visual_jepa_fep_bridge_create(
    const visual_jepa_fep_config_t* config
);

/**
 * @brief Destroy bridge
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void visual_jepa_fep_bridge_destroy(visual_jepa_fep_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge to reset
 * @return NIMCP_SUCCESS on success
 */
int visual_jepa_fep_bridge_reset(visual_jepa_fep_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect to Visual JEPA bridge
 *
 * @param bridge JEPA-FEP bridge
 * @param visual_jepa Visual JEPA bridge
 * @return NIMCP_SUCCESS on success
 */
int visual_jepa_fep_bridge_connect_jepa(
    visual_jepa_fep_bridge_t* bridge,
    visual_jepa_bridge_t* visual_jepa
);

/**
 * @brief Connect to FEP system
 *
 * @param bridge JEPA-FEP bridge
 * @param fep FEP system
 * @return NIMCP_SUCCESS on success
 */
int visual_jepa_fep_bridge_connect_fep(
    visual_jepa_fep_bridge_t* bridge,
    fep_system_t* fep
);

/**
 * @brief Check if bridge is fully connected
 *
 * @param bridge Bridge to check
 * @return true if both systems connected
 */
bool visual_jepa_fep_bridge_is_connected(const visual_jepa_fep_bridge_t* bridge);

/* ============================================================================
 * FEP → JEPA Direction
 * ============================================================================ */

/**
 * @brief Get precision weights for JEPA training
 *
 * WHAT: Compute precision weights for each patch
 * WHY:  FEP precision modulates JEPA learning
 * HOW:  Combine global precision with attention
 *
 * @param bridge JEPA-FEP bridge
 * @param patch_weights Output weights per patch
 * @param num_patches Number of patches
 * @return NIMCP_SUCCESS on success
 */
int visual_jepa_fep_get_precision_weights(
    visual_jepa_fep_bridge_t* bridge,
    float* patch_weights,
    uint32_t num_patches
);

/**
 * @brief Apply attention to patch precision
 *
 * WHAT: Boost precision for attended patches
 * WHY:  Attention = precision in FEP framework
 * HOW:  precision[i] *= (1 + attention[i] × gain)
 *
 * @param bridge JEPA-FEP bridge
 * @param attention Attention map
 * @param width Attention width
 * @param height Attention height
 * @return NIMCP_SUCCESS on success
 */
int visual_jepa_fep_apply_attention_precision(
    visual_jepa_fep_bridge_t* bridge,
    const float* attention,
    uint32_t width,
    uint32_t height
);

/**
 * @brief Get learning rate modifier from precision
 *
 * WHAT: Scale JEPA learning rate by precision
 * WHY:  High precision → faster learning on reliable signals
 *
 * @param bridge JEPA-FEP bridge
 * @return Learning rate multiplier
 */
float visual_jepa_fep_get_lr_modifier(const visual_jepa_fep_bridge_t* bridge);

/* ============================================================================
 * JEPA → FEP Direction
 * ============================================================================ */

/**
 * @brief Report JEPA prediction error to FEP
 *
 * WHAT: Convert JEPA error to FEP prediction error
 * WHY:  JEPA errors should update FEP beliefs
 * HOW:  Map latent error to FEP observation space
 *
 * @param bridge JEPA-FEP bridge
 * @param prediction Predicted latent
 * @param target Target latent
 * @return NIMCP_SUCCESS on success
 */
int visual_jepa_fep_report_prediction_error(
    visual_jepa_fep_bridge_t* bridge,
    const jepa_latent_t* prediction,
    const jepa_latent_t* target
);

/**
 * @brief Report novelty detection to FEP
 *
 * WHAT: Signal novel visual pattern to FEP
 * WHY:  High JEPA PE = unexpected = novelty
 * HOW:  Trigger FEP novelty response
 *
 * @param bridge JEPA-FEP bridge
 * @param prediction_error JEPA prediction error magnitude
 * @return NIMCP_SUCCESS on success
 */
int visual_jepa_fep_report_novelty(
    visual_jepa_fep_bridge_t* bridge,
    float prediction_error
);

/**
 * @brief Update FEP beliefs from JEPA
 *
 * WHAT: Push JEPA representation to FEP belief state
 * WHY:  JEPA latents inform FEP world model
 * HOW:  Update FEP visual hierarchy beliefs
 *
 * @param bridge JEPA-FEP bridge
 * @param latent JEPA latent representation
 * @return NIMCP_SUCCESS on success
 */
int visual_jepa_fep_update_beliefs(
    visual_jepa_fep_bridge_t* bridge,
    const jepa_latent_t* latent
);

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

/**
 * @brief Main bridge update
 *
 * WHAT: Bidirectional synchronization cycle
 * WHY:  Keep JEPA and FEP coordinated
 * HOW:  Update precision, propagate errors, sync beliefs
 *
 * @param bridge JEPA-FEP bridge
 * @param delta_ms Time since last update
 * @return NIMCP_SUCCESS on success
 */
int visual_jepa_fep_bridge_update(
    visual_jepa_fep_bridge_t* bridge,
    uint64_t delta_ms
);

/**
 * @brief Update precision from prediction errors
 *
 * WHAT: Learn precision from error history
 * WHY:  Precision tracks inverse variance of errors
 * HOW:  EMA update: π = decay × π + (1-decay) × 1/E[ε²]
 *
 * @param bridge JEPA-FEP bridge
 * @param prediction_errors Per-patch errors
 * @param num_patches Number of patches
 * @return NIMCP_SUCCESS on success
 */
int visual_jepa_fep_update_precision(
    visual_jepa_fep_bridge_t* bridge,
    const float* prediction_errors,
    uint32_t num_patches
);

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

/**
 * @brief Get current state
 *
 * @param bridge JEPA-FEP bridge
 * @param state Output state
 * @return NIMCP_SUCCESS on success
 */
int visual_jepa_fep_bridge_get_state(
    const visual_jepa_fep_bridge_t* bridge,
    visual_jepa_fep_state_t* state
);

/**
 * @brief Get statistics
 *
 * @param bridge JEPA-FEP bridge
 * @param stats Output statistics
 * @return NIMCP_SUCCESS on success
 */
int visual_jepa_fep_bridge_get_stats(
    const visual_jepa_fep_bridge_t* bridge,
    visual_jepa_fep_stats_t* stats
);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * @param bridge JEPA-FEP bridge
 * @return NIMCP_SUCCESS on success
 */
int visual_jepa_fep_bridge_connect_bio_async(visual_jepa_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge JEPA-FEP bridge
 * @return NIMCP_SUCCESS on success
 */
int visual_jepa_fep_bridge_disconnect_bio_async(visual_jepa_fep_bridge_t* bridge);

/**
 * @brief Check bio-async connection
 *
 * @param bridge JEPA-FEP bridge
 * @return true if connected
 */
bool visual_jepa_fep_bridge_is_bio_async_connected(
    const visual_jepa_fep_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VISUAL_JEPA_FEP_BRIDGE_H */
