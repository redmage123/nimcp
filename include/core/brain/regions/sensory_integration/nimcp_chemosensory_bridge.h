/**
 * @file nimcp_chemosensory_bridge.h
 * @brief Chemosensory Integration Bridge (Olfactory-Gustatory Flavor Binding)
 * @version 1.0.0
 * @date 2026-01-12
 *
 * WHAT: Bridge integrating olfactory (smell) and gustatory (taste) cortices
 *       to create unified flavor perception - the binding of taste and smell.
 *
 * WHY: Flavor perception emerges from multi-sensory integration:
 *      - 80% of "taste" is actually retronasal olfaction
 *      - Cross-modal prediction errors enhance flavor detection
 *      - Congruent taste-smell pairs enhance palatability
 *      - Incongruent pairs trigger disgust/rejection
 *
 * HOW: Binds olfactory and gustatory signals temporally, computes flavor
 *      predictions, detects cross-modal congruence, and generates unified
 *      flavor percepts.
 *
 * BIOLOGICAL BASIS:
 * =================
 * - Orbitofrontal cortex (OFC) integrates taste and smell
 * - Insular cortex processes flavor identity
 * - Retronasal olfaction during eating
 * - Flavor-food associations in hippocampus
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CHEMOSENSORY_BRIDGE_H
#define NIMCP_CHEMOSENSORY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "core/brain/regions/olfactory/nimcp_olfactory.h"
#include "core/brain/regions/gustatory/nimcp_gustatory.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Type Aliases
 * ============================================================================ */

/** @brief Alias for olfactory perception (uses olfact_odor_id_t internally) */
typedef olfact_odor_id_t odor_perception_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

#define CHEMOSENSORY_FLAVOR_DIM         64
#define CHEMOSENSORY_BINDING_WINDOW_MS  500
#define CHEMOSENSORY_CONGRUENCE_THRESHOLD  0.6f

/* ============================================================================
 * Enumerations
 * ============================================================================ */

typedef enum {
    CHEMOSENSORY_CONGRUENCE_NONE = 0,    /**< No congruence */
    CHEMOSENSORY_CONGRUENCE_LOW,         /**< Low congruence */
    CHEMOSENSORY_CONGRUENCE_MEDIUM,      /**< Medium congruence */
    CHEMOSENSORY_CONGRUENCE_HIGH,        /**< High congruence */
    CHEMOSENSORY_CONGRUENCE_PERFECT      /**< Perfect match */
} chemosensory_congruence_t;

typedef enum {
    CHEMOSENSORY_STATUS_IDLE = 0,
    CHEMOSENSORY_STATUS_BINDING,
    CHEMOSENSORY_STATUS_PROCESSING,
    CHEMOSENSORY_STATUS_ERROR
} chemosensory_status_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Unified flavor percept
 */
typedef struct {
    /* Flavor identity */
    char flavor_name[64];               /**< Identified flavor name */
    float* flavor_profile;              /**< Flavor profile vector */
    uint32_t profile_dim;               /**< Profile dimensionality */

    /* Components */
    float taste_contribution;           /**< Taste contribution [0, 1] */
    float smell_contribution;           /**< Smell contribution [0, 1] */

    /* Integration quality */
    chemosensory_congruence_t congruence; /**< Taste-smell congruence */
    float binding_strength;             /**< Binding strength [0, 1] */

    /* Hedonic */
    float palatability;                 /**< Overall palatability [0, 1] */
    float familiarity;                  /**< Flavor familiarity [0, 1] */

    /* Timing */
    uint64_t onset_time;                /**< Percept onset */
    uint32_t binding_latency_ms;        /**< Binding latency */
} chemosensory_flavor_t;

/**
 * @brief Cross-modal prediction
 */
typedef struct {
    float* predicted_taste;             /**< Predicted taste from smell */
    uint32_t taste_dim;                 /**< Taste dimensions */
    float* predicted_smell;             /**< Predicted smell from taste */
    uint32_t smell_dim;                 /**< Smell dimensions */
    float prediction_confidence;        /**< Prediction confidence */
} chemosensory_prediction_t;

/**
 * @brief Flavor memory association
 */
typedef struct {
    uint32_t memory_id;                 /**< Associated memory ID */
    char food_name[64];                 /**< Associated food name */
    float association_strength;         /**< Association strength */
    float emotional_valence;            /**< Emotional valence [-1, 1] */
    bool is_learned;                    /**< Learned vs innate */
} chemosensory_memory_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

typedef struct {
    uint32_t flavor_dim;
    uint32_t binding_window_ms;
    float congruence_threshold;
    float binding_decay_rate;
    bool enable_predictions;
    bool enable_memory_associations;
    bool enable_logging;
} chemosensory_config_t;

typedef struct {
    uint64_t flavors_bound;
    uint64_t predictions_made;
    uint64_t memories_triggered;
    float avg_congruence;
    float avg_binding_strength;
    float avg_palatability;
} chemosensory_stats_t;

/* ============================================================================
 * Handle
 * ============================================================================ */

typedef struct chemosensory_bridge_struct chemosensory_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int chemosensory_default_config(chemosensory_config_t* config);
chemosensory_bridge_t* chemosensory_bridge_create(const chemosensory_config_t* config);
void chemosensory_bridge_destroy(chemosensory_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

int chemosensory_connect(chemosensory_bridge_t* bridge, nimcp_olfactory_t* olfact, nimcp_gustatory_t* gust);
int chemosensory_disconnect(chemosensory_bridge_t* bridge);
bool chemosensory_is_connected(const chemosensory_bridge_t* bridge);

/* ============================================================================
 * Binding API
 * ============================================================================ */

int chemosensory_bind_flavor(chemosensory_bridge_t* bridge, const odor_perception_t* odor, const taste_perception_t* taste, chemosensory_flavor_t* flavor);
int chemosensory_update_binding(chemosensory_bridge_t* bridge, float dt);
int chemosensory_get_current_flavor(chemosensory_bridge_t* bridge, chemosensory_flavor_t* flavor);

/* ============================================================================
 * Prediction API
 * ============================================================================ */

int chemosensory_predict_taste_from_smell(chemosensory_bridge_t* bridge, const odor_perception_t* odor, float* predicted_taste);
int chemosensory_predict_smell_from_taste(chemosensory_bridge_t* bridge, const taste_perception_t* taste, float* predicted_smell);
int chemosensory_compute_prediction_error(chemosensory_bridge_t* bridge, const chemosensory_prediction_t* prediction, float* error);

/* ============================================================================
 * Congruence API
 * ============================================================================ */

int chemosensory_evaluate_congruence(chemosensory_bridge_t* bridge, const odor_perception_t* odor, const taste_perception_t* taste, chemosensory_congruence_t* congruence, float* score);
int chemosensory_get_congruent_pairs(chemosensory_bridge_t* bridge, uint32_t* pairs, uint32_t* num_pairs, uint32_t max_pairs);

/* ============================================================================
 * Memory API
 * ============================================================================ */

int chemosensory_trigger_memory(chemosensory_bridge_t* bridge, const chemosensory_flavor_t* flavor, chemosensory_memory_t* memory);
int chemosensory_learn_association(chemosensory_bridge_t* bridge, const chemosensory_flavor_t* flavor, const char* food_name, float valence);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

int chemosensory_get_stats(const chemosensory_bridge_t* bridge, chemosensory_stats_t* stats);
int chemosensory_reset_stats(chemosensory_bridge_t* bridge);
void chemosensory_print_summary(const chemosensory_bridge_t* bridge);

/* ============================================================================
 * Cleanup
 * ============================================================================ */

void chemosensory_flavor_free(chemosensory_flavor_t* flavor);
void chemosensory_prediction_free(chemosensory_prediction_t* prediction);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CHEMOSENSORY_BRIDGE_H */
