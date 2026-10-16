/**
 * @file nimcp_surprise_self_model_bridge.h
 * @brief Bridge between Surprise Amplifier and Self-Model system
 * @version 1.0.0
 * @date 2026-01-27
 *
 * WHAT: Update self-model based on surprise about own capabilities
 * WHY:  Capability surprises trigger belief/competence revision;
 *       self-model confidence modulates surprise sensitivity
 * HOW:  Surprise → capability revision; self-model confidence → sensitivity modulation
 *
 * BIOLOGICAL BASIS:
 * ==========================================================================
 *
 * SURPRISE → SELF-MODEL:
 * - Capability surprises trigger competence re-evaluation
 * - Novel capabilities discovered through unexpected successes/failures
 * - Belief revision updates internal model of self-capabilities
 * - Reference: Fleming & Dolan (2012) "The neural basis of metacognition"
 *
 * SELF-MODEL → SURPRISE:
 * - Self-model confidence modulates surprise sensitivity
 * - High confidence in a domain reduces surprise for related events
 * - Low confidence amplifies surprise (uncertainty-driven attention)
 *
 * ERROR CODE RANGE: 29000-29099 (Module-specific)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SURPRISE_SELF_MODEL_BRIDGE_H
#define NIMCP_SURPRISE_SELF_MODEL_BRIDGE_H

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
 * Error Codes (Range: 29000-29099)
 * ============================================================================ */

#define NIMCP_SURPRISE_SELF_MODEL_ERROR_BASE           29000
#define NIMCP_SURPRISE_SELF_MODEL_ERROR_NULL_POINTER   (NIMCP_SURPRISE_SELF_MODEL_ERROR_BASE + 1)
#define NIMCP_SURPRISE_SELF_MODEL_ERROR_INVALID_PARAM  (NIMCP_SURPRISE_SELF_MODEL_ERROR_BASE + 2)
#define NIMCP_SURPRISE_SELF_MODEL_ERROR_NO_MEMORY      (NIMCP_SURPRISE_SELF_MODEL_ERROR_BASE + 3)
#define NIMCP_SURPRISE_SELF_MODEL_ERROR_NOT_CONNECTED  (NIMCP_SURPRISE_SELF_MODEL_ERROR_BASE + 4)

/* ============================================================================
 * Constants
 * ============================================================================ */

#define SURPRISE_SELF_MODEL_DEFAULT_CAP_THRESHOLD       0.6f
#define SURPRISE_SELF_MODEL_DEFAULT_COMPETENCE_RATE     0.05f
#define SURPRISE_SELF_MODEL_DEFAULT_CONFIDENCE_GAIN     0.3f
#define SURPRISE_SELF_MODEL_DEFAULT_BELIEF_RATE         0.1f
#define SURPRISE_SELF_MODEL_DEFAULT_MAX_CAPABILITIES    32

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/** @brief Capability revision type */
typedef enum {
    SURPRISE_CAPABILITY_UPGRADE = 0,    /**< Capability was better than expected */
    SURPRISE_CAPABILITY_DOWNGRADE,      /**< Capability was worse than expected */
    SURPRISE_CAPABILITY_NOVEL           /**< Entirely new capability discovered */
} surprise_capability_revision_type_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Capability revision event
 */
typedef struct {
    uint32_t capability_id;             /**< Capability identifier */
    float prior_confidence;             /**< Confidence before revision */
    float posterior_confidence;          /**< Confidence after revision */
    float surprise_magnitude;           /**< Surprise that triggered revision */
    surprise_capability_revision_type_t revision_type; /**< Type of revision */
    uint64_t timestamp_ms;              /**< Revision timestamp */
} surprise_capability_revision_t;

/**
 * @brief Configuration for surprise-self-model bridge
 */
typedef struct {
    float capability_surprise_threshold; /**< Threshold for capability revision [0.6] */
    float competence_update_rate;       /**< How fast competence updates [0.05] */
    float confidence_modulation_gain;   /**< How much confidence affects sensitivity [0.3] */
    float belief_revision_rate;         /**< Speed of belief revision [0.1] */
    uint32_t max_tracked_capabilities;  /**< Max capabilities to track [32] */
    bool enable_bio_async;              /**< Bio-async messaging [true] */
    bool enable_logging;                /**< Diagnostic logging [true] */
} surprise_self_model_config_t;

/**
 * @brief Effects computed by the bridge
 */
typedef struct {
    float confidence_modulation;        /**< Current confidence-based sensitivity modulation */
    float competence_delta;             /**< Recent competence change magnitude */
    uint32_t beliefs_revised;           /**< Number of beliefs currently revised */
    uint32_t capabilities_discovered;   /**< Novel capabilities discovered */
    bool self_model_connected;          /**< Whether self-model is connected */
} surprise_self_model_effects_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t capability_surprises;      /**< Capability surprise events */
    uint64_t competence_updates;        /**< Competence update events */
    uint64_t belief_revisions;          /**< Belief revision events */
    uint64_t discoveries;               /**< Novel capability discoveries */
    uint64_t confidence_queries;        /**< Confidence modulation queries */
    uint64_t total_updates;             /**< Total update cycles */
} surprise_self_model_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct surprise_self_model_bridge surprise_self_model_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/** @brief Create default configuration */
surprise_self_model_config_t surprise_self_model_bridge_default_config(void);

/** @brief Create bridge (NULL config = defaults) */
surprise_self_model_bridge_t* surprise_self_model_bridge_create(
    const surprise_self_model_config_t* config);

/** @brief Destroy bridge (NULL-safe) */
void surprise_self_model_bridge_destroy(surprise_self_model_bridge_t* bridge);

/** @brief Reset state, preserving config and connections */
int surprise_self_model_bridge_reset(surprise_self_model_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/** @brief Connect to surprise amplifier */
int surprise_self_model_bridge_connect_amplifier(
    surprise_self_model_bridge_t* bridge,
    struct surprise_amplifier* amp);

/** @brief Connect to self-model system */
int surprise_self_model_bridge_connect_self_model(
    surprise_self_model_bridge_t* bridge,
    void* self_model);

/** @brief Connect to bio-async router */
int surprise_self_model_bridge_connect_bio_async(
    surprise_self_model_bridge_t* bridge,
    void* router);

/** @brief Disconnect from bio-async router */
int surprise_self_model_bridge_disconnect_bio_async(
    surprise_self_model_bridge_t* bridge);

/* ============================================================================
 * Operations API
 * ============================================================================ */

/**
 * @brief Process a capability surprise event
 * @param bridge Bridge handle
 * @param capability_id Capability that was surprising
 * @param surprise_magnitude Surprise level [0-1]
 * @param revision_type Type of revision
 * @return 0 on success, error code otherwise
 */
int surprise_self_model_on_capability_surprise(
    surprise_self_model_bridge_t* bridge,
    uint32_t capability_id,
    float surprise_magnitude,
    surprise_capability_revision_type_t revision_type);

/**
 * @brief Process competence feedback from self-model
 * @param bridge Bridge handle
 * @param capability_id Capability being assessed
 * @param competence_level New competence level [0-1]
 * @return 0 on success, error code otherwise
 */
int surprise_self_model_on_competence_feedback(
    surprise_self_model_bridge_t* bridge,
    uint32_t capability_id,
    float competence_level);

/**
 * @brief Query confidence modulation for surprise sensitivity
 * @return Modulation factor (< 1.0 reduces sensitivity, > 1.0 increases it)
 */
float surprise_self_model_query_confidence_modulation(
    const surprise_self_model_bridge_t* bridge);

/** @brief Periodic update */
int surprise_self_model_bridge_update(
    surprise_self_model_bridge_t* bridge,
    float dt_seconds);

/* ============================================================================
 * Query API
 * ============================================================================ */

/** @brief Get current effects */
int surprise_self_model_bridge_get_effects(
    const surprise_self_model_bridge_t* bridge,
    surprise_self_model_effects_t* effects_out);

/** @brief Get accumulated statistics */
int surprise_self_model_bridge_get_stats(
    const surprise_self_model_bridge_t* bridge,
    surprise_self_model_stats_t* stats_out);

/** @brief Get most recent capability revision */
int surprise_self_model_get_last_revision(
    const surprise_self_model_bridge_t* bridge,
    surprise_capability_revision_t* revision_out);

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

/** @brief Set health agent for heartbeat monitoring */
int surprise_self_model_bridge_set_health_agent(
    surprise_self_model_bridge_t* bridge,
    struct nimcp_health_agent* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SURPRISE_SELF_MODEL_BRIDGE_H */
