/**
 * @file nimcp_temporal_substrate_bridge.h
 * @brief Bridge between temporal cortex and neural substrate
 *
 * WHAT: Links auditory/object/semantic processing to metabolic state
 * WHY: Perception and memory require sustained neural activation
 * HOW: Monitors ATP/fatigue; modulates recognition accuracy, memory retrieval
 *
 * BIOLOGICAL BASIS:
 * - Temporal cortex has high metabolic demands for sensory processing
 * - ATP depletion causes perceptual deficits (prosopagnosia-like symptoms)
 * - Fatigue reduces object recognition accuracy and semantic access
 * - Metabolic stress impairs auditory discrimination
 * - Memory retrieval requires sustained prefrontal-temporal activation
 *
 * DEFICIT EFFECTS:
 * - Low ATP: Slower object recognition, impaired face processing
 * - High fatigue: Reduced semantic spreading, retrieval failures
 * - Metabolic stress: Auditory discrimination errors, missed speech
 *
 * @author NIMCP Team
 * @date 2025-12-30
 */

#ifndef NIMCP_TEMPORAL_SUBSTRATE_BRIDGE_H
#define NIMCP_TEMPORAL_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define BIO_MODULE_SUBSTRATE_TEMPORAL 0x1260

//=============================================================================
// Types
//=============================================================================

/**
 * @brief Metabolic effects on temporal cortex
 */
typedef struct {
    float auditory_acuity;            /**< Auditory discrimination [0-1] */
    float speech_recognition;         /**< Speech recognition accuracy [0-1] */
    float object_recognition;         /**< Object recognition speed [0-1] */
    float face_processing;            /**< Face processing capacity [0-1] */
    float semantic_retrieval;         /**< Semantic memory access [0-1] */
    float spreading_activation;       /**< Spreading activation range [0-1] */
    float overall_capacity;           /**< Combined perception capacity [0-1] */
} temporal_substrate_effects_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    bool enable_atp_modulation;       /**< ATP affects acuity/recognition */
    bool enable_fatigue_modulation;   /**< Fatigue affects retrieval/processing */
    bool enable_bio_async;            /**< Enable bio-async messaging */
    float atp_sensitivity;            /**< How much ATP affects output [0-2] */
    float fatigue_sensitivity;        /**< How much fatigue affects output [0-2] */
    float min_capacity;               /**< Minimum capacity floor [0-1] */
    float auditory_atp_weight;        /**< ATP weight for auditory (default 1.0) */
    float semantic_fatigue_weight;    /**< Fatigue weight for semantic (default 1.0) */
} temporal_substrate_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t updates_processed;       /**< Total substrate updates */
    uint64_t low_atp_events;          /**< Times ATP dropped below threshold */
    uint64_t high_fatigue_events;     /**< Times fatigue exceeded threshold */
    float avg_auditory_acuity;        /**< Average auditory acuity */
    float avg_object_recognition;     /**< Average recognition accuracy */
    float avg_semantic_retrieval;     /**< Average semantic access */
    float min_observed_capacity;      /**< Minimum capacity observed */
} temporal_substrate_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct temporal_substrate_bridge temporal_substrate_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default configuration
 * @return Default config with sensible values
 */
temporal_substrate_config_t temporal_substrate_default_config(void);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create temporal substrate bridge
 * @param temporal Temporal adapter handle (void* for flexibility)
 * @param substrate Neural substrate handle
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
temporal_substrate_bridge_t* temporal_substrate_bridge_create(
    void* temporal,
    neural_substrate_t* substrate,
    const temporal_substrate_config_t* config
);

/**
 * @brief Destroy bridge
 * @param bridge Bridge to destroy
 */
void temporal_substrate_bridge_destroy(temporal_substrate_bridge_t* bridge);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update effects from current substrate state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 *
 * Reads ATP/fatigue from substrate, computes modulation effects
 */
int temporal_substrate_bridge_update(temporal_substrate_bridge_t* bridge);

/**
 * @brief Get current metabolic effects
 * @param bridge Bridge handle
 * @param effects Output: current effects
 * @return 0 on success, -1 on error
 */
int temporal_substrate_bridge_get_effects(
    const temporal_substrate_bridge_t* bridge,
    temporal_substrate_effects_t* effects
);

/**
 * @brief Apply effects to temporal adapter
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 *
 * Modulates temporal adapter parameters based on metabolic state
 */
int temporal_substrate_bridge_apply_effects(temporal_substrate_bridge_t* bridge);

//=============================================================================
// Bio-Async API
//=============================================================================

/**
 * @brief Register with bio-async router
 * @param bridge Bridge handle
 * @param router Bio router handle
 * @return 0 on success, -1 on error
 */
int temporal_substrate_bridge_register_bio_async(
    temporal_substrate_bridge_t* bridge,
    bio_router_t* router
);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output: statistics
 * @return 0 on success, -1 on error
 */
int temporal_substrate_bridge_get_stats(
    const temporal_substrate_bridge_t* bridge,
    temporal_substrate_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 */
void temporal_substrate_bridge_reset_stats(temporal_substrate_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TEMPORAL_SUBSTRATE_BRIDGE_H */
