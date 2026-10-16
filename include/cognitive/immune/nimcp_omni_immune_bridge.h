/**
 * @file nimcp_omni_immune_bridge.h
 * @brief Omnidirectional Inference to Immune System Bridge
 * @version 1.0.0
 * @date 2025-01-04
 *
 * WHAT: Bridge between omnidirectional inference and brain immune system
 * WHY:  Prediction errors trigger immune surveillance; free energy drives responses
 * HOW:  Bidirectional JEPA errors map to inflammation, precision maps to cytokines
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * FREE ENERGY AND IMMUNE SYSTEM:
 * ------------------------------
 * The immune system operates as a free energy minimizer:
 *
 *   1. Prediction errors (PE) from omnidirectional inference signal anomalies
 *      - High PE magnitude → immune activation
 *      - Persistent PE → chronic inflammation
 *
 *   2. Free energy across the predictive hierarchy maps to threat levels
 *      - High total FE → system-wide immune response
 *      - Level-specific FE → localized responses
 *
 *   3. Precision weighting from cytokines modulates PE impact
 *      - Pro-inflammatory → increase PE sensitivity
 *      - Anti-inflammatory → suppress PE magnitude
 *
 * OMNIDIRECTIONAL-IMMUNE MAPPING:
 * -------------------------------
 *   Omni Component          →  Immune Effect
 *   ─────────────────────────────────────────────
 *   JEPA backward PE        →  Microglia activation (cleanup)
 *   Hopfield retrieval fail →  Memory B cell search
 *   Pred hierarchy error    →  Hierarchical inflammation
 *   Temporal replay anomaly →  Consolidation immune check
 *   High free energy        →  System-wide surveillance
 *
 * INTEGRATION POINTS:
 * -------------------
 * - Bidirectional JEPA: Prediction errors trigger immune signals
 * - Hopfield Memory: Failed retrievals activate memory immunity
 * - Predictive Hierarchy: Layer-wise inflammation
 * - Temporal Replay: Replay anomalies trigger consolidation immunity
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_OMNI_IMMUNE_BRIDGE_H
#define NIMCP_OMNI_IMMUNE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct omni_immune_bridge omni_immune_bridge_t;
typedef struct jepa_bidirectional jepa_bidirectional_t;
typedef struct hopfield_memory hopfield_memory_t;
typedef struct predictive_hierarchy predictive_hierarchy_t;
typedef struct temporal_replay temporal_replay_t;
typedef struct brain_immune_system brain_immune_system_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Bio-async module ID for omni-immune bridge */
#define BIO_MODULE_OMNI_IMMUNE_BRIDGE          0x0E50

/** @brief Default prediction error threshold for immune activation */
#define OMNI_IMMUNE_DEFAULT_PE_THRESHOLD       2.0f

/** @brief Default free energy threshold for system-wide response */
#define OMNI_IMMUNE_DEFAULT_FE_THRESHOLD       5.0f

/** @brief Default inflammation scale */
#define OMNI_IMMUNE_DEFAULT_INFLAMMATION_SCALE 1.5f

/** @brief Maximum inflammation from prediction errors */
#define OMNI_IMMUNE_MAX_PE_INFLAMMATION        8.0f

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Omni component source
 */
typedef enum {
    OMNI_SOURCE_JEPA_FORWARD = 0,    /**< JEPA forward prediction */
    OMNI_SOURCE_JEPA_BACKWARD,       /**< JEPA backward prediction */
    OMNI_SOURCE_JEPA_LATERAL,        /**< JEPA lateral prediction */
    OMNI_SOURCE_HOPFIELD,            /**< Hopfield retrieval */
    OMNI_SOURCE_PRED_HIERARCHY,      /**< Predictive hierarchy */
    OMNI_SOURCE_TEMPORAL_REPLAY,     /**< Temporal replay */
    OMNI_SOURCE_AGGREGATED           /**< Aggregated across all */
} omni_immune_source_t;

/**
 * @brief Immune response type triggered
 */
typedef enum {
    OMNI_IMMUNE_NONE = 0,            /**< No immune response */
    OMNI_IMMUNE_LOCAL,               /**< Local surveillance */
    OMNI_IMMUNE_REGIONAL,            /**< Regional response */
    OMNI_IMMUNE_SYSTEMIC,            /**< System-wide response */
    OMNI_IMMUNE_MEMORY               /**< Memory immune activation */
} omni_immune_response_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Omni inference effects on immune system
 */
typedef struct {
    float pe_magnitude;              /**< Prediction error magnitude (0-10) */
    float free_energy;               /**< Total free energy */
    float inflammation_signal;       /**< Resulting inflammation signal */
    omni_immune_source_t source;     /**< Primary error source */
    omni_immune_response_t response; /**< Recommended response */
    uint32_t affected_levels;        /**< Pred hierarchy levels affected */
} omni_to_immune_effects_t;

/**
 * @brief Immune effects on omni inference
 */
typedef struct {
    float precision_modulation;      /**< Precision adjustment (0-3) */
    float pe_sensitivity;            /**< PE sensitivity multiplier */
    float retrieval_threshold;       /**< Hopfield retrieval threshold adj */
    float replay_priority;           /**< Replay priority adjustment */
    bool suppress_backward;          /**< Suppress backward inference */
    bool boost_consolidation;        /**< Boost memory consolidation */
} immune_to_omni_effects_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Thresholds */
    float pe_threshold;              /**< PE threshold for activation */
    float fe_threshold;              /**< FE threshold for system response */
    float inflammation_scale;        /**< Inflammation scaling factor */

    /* Per-source weights */
    float jepa_forward_weight;       /**< Weight for JEPA forward PE */
    float jepa_backward_weight;      /**< Weight for JEPA backward PE */
    float hopfield_weight;           /**< Weight for Hopfield errors */
    float pred_hier_weight;          /**< Weight for pred hierarchy PE */
    float replay_weight;             /**< Weight for replay anomalies */

    /* Response configuration */
    bool enable_microglia;           /**< Enable microglia activation */
    bool enable_memory_immune;       /**< Enable memory B cell activation */
    bool enable_precision_feedback;  /**< Enable precision feedback loop */

    /* Integration */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    bool enable_logging;             /**< Enable logging */
} omni_immune_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;          /**< Total bridge updates */
    uint64_t immune_activations;     /**< Total immune activations */
    uint64_t local_responses;        /**< Local response count */
    uint64_t regional_responses;     /**< Regional response count */
    uint64_t systemic_responses;     /**< Systemic response count */
    uint64_t memory_responses;       /**< Memory immune count */
    float avg_pe_magnitude;          /**< Average PE magnitude */
    float avg_inflammation;          /**< Average inflammation signal */
    float max_inflammation;          /**< Maximum inflammation signal */
    uint64_t precision_feedbacks;    /**< Precision feedback count */
} omni_immune_stats_t;

/**
 * @brief Omni-immune bridge structure
 */
struct omni_immune_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge */

    omni_immune_config_t config;     /**< Configuration */

    /* Connected systems */
    jepa_bidirectional_t* jepa;      /**< Bidirectional JEPA */
    hopfield_memory_t* hopfield;     /**< Hopfield memory */
    predictive_hierarchy_t* pred_hier; /**< Predictive hierarchy */
    temporal_replay_t* replay;       /**< Temporal replay */
    brain_immune_system_t* immune;   /**< Brain immune system */

    /* Computed effects */
    omni_to_immune_effects_t omni_effects;    /**< Omni → immune */
    immune_to_omni_effects_t immune_effects;  /**< Immune → omni */

    /* Statistics */
    omni_immune_stats_t stats;

    /* Bio-async integration */
    void* bio_context;               /**< Bio-async module context */
    bool bio_async_connected;        /**< Bio-async connection state */

    /* Thread safety */
    void* mutex;
};

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
int omni_immune_default_config(omni_immune_config_t* config);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create omni-immune bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
omni_immune_bridge_t* omni_immune_bridge_create(
    const omni_immune_config_t* config);

/**
 * @brief Destroy bridge
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void omni_immune_bridge_destroy(omni_immune_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect to bidirectional JEPA
 *
 * @param bridge Bridge
 * @param jepa Bidirectional JEPA (NULL to disconnect)
 * @return NIMCP_SUCCESS on success
 */
int omni_immune_connect_jepa(omni_immune_bridge_t* bridge,
                              jepa_bidirectional_t* jepa);

/**
 * @brief Connect to Hopfield memory
 *
 * @param bridge Bridge
 * @param hopfield Hopfield memory (NULL to disconnect)
 * @return NIMCP_SUCCESS on success
 */
int omni_immune_connect_hopfield(omni_immune_bridge_t* bridge,
                                  hopfield_memory_t* hopfield);

/**
 * @brief Connect to predictive hierarchy
 *
 * @param bridge Bridge
 * @param pred_hier Predictive hierarchy (NULL to disconnect)
 * @return NIMCP_SUCCESS on success
 */
int omni_immune_connect_pred_hier(omni_immune_bridge_t* bridge,
                                   predictive_hierarchy_t* pred_hier);

/**
 * @brief Connect to temporal replay
 *
 * @param bridge Bridge
 * @param replay Temporal replay (NULL to disconnect)
 * @return NIMCP_SUCCESS on success
 */
int omni_immune_connect_replay(omni_immune_bridge_t* bridge,
                                temporal_replay_t* replay);

/**
 * @brief Connect to brain immune system
 *
 * @param bridge Bridge
 * @param immune Brain immune system (NULL to disconnect)
 * @return NIMCP_SUCCESS on success
 */
int omni_immune_connect_immune(omni_immune_bridge_t* bridge,
                                brain_immune_system_t* immune);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update bridge (compute bidirectional effects)
 *
 * WHAT: Compute effects between omni inference and immune system
 * WHY:  Synchronize prediction errors with immune responses
 * HOW:  Aggregate PE from all sources, map to inflammation
 *
 * @param bridge Bridge
 * @return NIMCP_SUCCESS on success
 */
int omni_immune_update(omni_immune_bridge_t* bridge);

/**
 * @brief Apply omni effects to immune system
 *
 * @param bridge Bridge
 * @return NIMCP_SUCCESS on success
 */
int omni_immune_apply_to_immune(omni_immune_bridge_t* bridge);

/**
 * @brief Apply immune effects to omni inference
 *
 * @param bridge Bridge
 * @return NIMCP_SUCCESS on success
 */
int omni_immune_apply_to_omni(omni_immune_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current omni-to-immune effects
 *
 * @param bridge Bridge
 * @param effects Output effects
 * @return NIMCP_SUCCESS on success
 */
int omni_immune_get_omni_effects(omni_immune_bridge_t* bridge,
                                  omni_to_immune_effects_t* effects);

/**
 * @brief Get current immune-to-omni effects
 *
 * @param bridge Bridge
 * @param effects Output effects
 * @return NIMCP_SUCCESS on success
 */
int omni_immune_get_immune_effects(omni_immune_bridge_t* bridge,
                                    immune_to_omni_effects_t* effects);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge
 * @param stats Output statistics
 * @return NIMCP_SUCCESS on success
 */
int omni_immune_get_stats(omni_immune_bridge_t* bridge,
                           omni_immune_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge
 * @return NIMCP_SUCCESS on success
 */
int omni_immune_reset_stats(omni_immune_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Bridge
 * @return NIMCP_SUCCESS on success
 */
int omni_immune_connect_bio_async(omni_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge
 * @return NIMCP_SUCCESS on success
 */
int omni_immune_disconnect_bio_async(omni_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async connected
 *
 * @param bridge Bridge
 * @return true if connected
 */
bool omni_immune_is_bio_async_connected(const omni_immune_bridge_t* bridge);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

/**
 * @brief Convert source to string
 *
 * @param source Omni source
 * @return String representation
 */
const char* omni_immune_source_to_string(omni_immune_source_t source);

/**
 * @brief Convert response to string
 *
 * @param response Immune response
 * @return String representation
 */
const char* omni_immune_response_to_string(omni_immune_response_t response);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OMNI_IMMUNE_BRIDGE_H */
