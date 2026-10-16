/**
 * @file nimcp_portia_classification.h
 * @brief Target Classification System for Portia Spider
 *
 * WHAT: Lightweight target classification and tracking system
 * WHY:  Portia spiders classify prey types and track multiple targets
 * HOW:  Fast registration, classification, and threat assessment
 *
 * BIOLOGICAL MODEL:
 * ```
 * BIOLOGICAL                          NIMCP IMPLEMENTATION
 * ─────────────────────────────────────────────────────────────────
 * Visual prey recognition            → Target classification
 * Movement pattern analysis          → Velocity tracking
 * Threat assessment                  → Classification confidence
 * Multi-target tracking              → Target registry
 * Memory of prey types               → Historical observation
 * ```
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║           TARGET CLASSIFICATION SYSTEM                        ║
 * ║  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐            ║
 * ║  │   Target    │ │Classification│ │   Threat    │            ║
 * ║  │  Registry   │ │   Engine     │ │ Assessment  │            ║
 * ║  └─────────────┘ └─────────────┘ └─────────────┘            ║
 * ╚═══════════════════════════════════════════════════════════════╝
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe operations
 * - BBB security validation
 *
 * @author NIMCP Portia Team
 * @date 2025-12-08
 */

#ifndef NIMCP_PORTIA_CLASSIFICATION_H
#define NIMCP_PORTIA_CLASSIFICATION_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Target Classification Types
//=============================================================================

/**
 * @brief Target classification categories
 */
typedef enum {
    TARGET_CLASS_UNKNOWN = 0,    /**< Classification uncertain */
    TARGET_CLASS_FRIENDLY,       /**< Known friendly entity */
    TARGET_CLASS_NEUTRAL,        /**< No threat, no interest */
    TARGET_CLASS_THREAT,         /**< Potential danger */
    TARGET_CLASS_PREY,           /**< Hunting target */
    TARGET_CLASS_OBSTACLE        /**< Non-entity obstruction */
} target_class_t;

/**
 * @brief Individual target information
 */
typedef struct {
    uint32_t id;                 /**< Unique target identifier */
    target_class_t classification; /**< Current classification */
    float confidence;            /**< Classification confidence (0-1) */
    float x, y, z;              /**< Position in 3D space */
    float vx, vy, vz;           /**< Velocity vector */
    float size;                 /**< Target size estimate */
    uint64_t first_seen_ms;     /**< First observation timestamp */
    uint64_t last_seen_ms;      /**< Last observation timestamp */
    uint32_t observation_count; /**< Number of observations */
    bool active;                /**< Currently tracked */
} target_info_t;

/**
 * @brief Target registry for multi-target tracking
 */
typedef struct {
    target_info_t* targets;     /**< Array of tracked targets */
    uint32_t target_count;      /**< Current target count */
    uint32_t target_capacity;   /**< Registry capacity */
    uint32_t next_id;           /**< Next target ID to assign */
} target_registry_t;

/**
 * @brief Configuration for classification system
 */
typedef struct {
    float classification_threshold;  /**< Min confidence to classify (0-1) */
    uint32_t max_targets;           /**< Maximum simultaneous targets */
    uint32_t retention_time_ms;     /**< How long to track lost targets */
    bool enable_prediction;         /**< Predict future positions */
    bool enable_bio_async;          /**< Enable bio-async messaging */
} portia_classification_config_t;

/**
 * @brief Opaque classifier handle
 */
typedef struct portia_classifier_struct* portia_classifier_t;

//=============================================================================
// Classifier Lifecycle
//=============================================================================

/**
 * @brief Create target classification system
 *
 * WHAT: Initialize classifier with configuration
 * WHY:  Set up target tracking and classification
 * HOW:  Allocate registry and configure parameters
 *
 * @param config Configuration parameters
 * @return Classifier handle or NULL on error
 */
NIMCP_EXPORT portia_classifier_t portia_classification_init(
    const portia_classification_config_t* config);

/**
 * @brief Destroy classification system
 *
 * WHAT: Free all resources
 * WHY:  Clean shutdown
 * HOW:  Free registry and internal structures
 *
 * @param classifier Classifier to destroy
 */
NIMCP_EXPORT void portia_classification_destroy(portia_classifier_t classifier);

//=============================================================================
// Target Management
//=============================================================================

/**
 * @brief Register new target
 *
 * WHAT: Add target to tracking registry
 * WHY:  Begin monitoring new entity
 * HOW:  Allocate ID, store initial observation
 *
 * @param classifier Classifier handle
 * @param x Initial X position
 * @param y Initial Y position
 * @param z Initial Z position
 * @param size Target size estimate
 * @return Target ID or 0 on error
 */
NIMCP_EXPORT uint32_t portia_classification_add_target(
    portia_classifier_t classifier,
    float x, float y, float z,
    float size);

/**
 * @brief Update target information
 *
 * WHAT: Update position and velocity for existing target
 * WHY:  Track target movement over time
 * HOW:  Update registry entry, compute velocity
 *
 * @param classifier Classifier handle
 * @param target_id Target to update
 * @param x New X position
 * @param y New Y position
 * @param z New Z position
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int portia_classification_update(
    portia_classifier_t classifier,
    uint32_t target_id,
    float x, float y, float z);

/**
 * @brief Classify target based on observations
 *
 * WHAT: Determine target classification
 * WHY:  Identify threats, prey, or neutrals
 * HOW:  Analyze movement patterns and characteristics
 *
 * @param classifier Classifier handle
 * @param target_id Target to classify
 * @param classification Classification result (output)
 * @param confidence Classification confidence (output)
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int portia_classification_classify(
    portia_classifier_t classifier,
    uint32_t target_id,
    target_class_t* classification,
    float* confidence);

/**
 * @brief Get all threat targets
 *
 * WHAT: Retrieve list of classified threats
 * WHY:  Quick threat assessment
 * HOW:  Filter registry for THREAT classification
 *
 * @param classifier Classifier handle
 * @param threats Output array for threat IDs
 * @param max_threats Size of output array
 * @return Number of threats found
 */
NIMCP_EXPORT uint32_t portia_classification_get_threats(
    portia_classifier_t classifier,
    uint32_t* threats,
    uint32_t max_threats);

/**
 * @brief Remove stale targets
 *
 * WHAT: Prune targets not observed recently
 * WHY:  Free resources from lost targets
 * HOW:  Check retention time, mark inactive
 *
 * @param classifier Classifier handle
 * @return Number of targets pruned
 */
NIMCP_EXPORT uint32_t portia_classification_prune(
    portia_classifier_t classifier);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get target information
 *
 * WHAT: Retrieve full target state
 * WHY:  Access detailed target data
 * HOW:  Copy registry entry to output
 *
 * @param classifier Classifier handle
 * @param target_id Target to query
 * @param info Output structure for target info
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int portia_classification_get_target(
    portia_classifier_t classifier,
    uint32_t target_id,
    target_info_t* info);

/**
 * @brief Get count of active targets
 *
 * WHAT: Count currently tracked targets
 * WHY:  Monitor system load
 * HOW:  Return active target count
 *
 * @param classifier Classifier handle
 * @return Number of active targets
 */
NIMCP_EXPORT uint32_t portia_classification_get_count(
    portia_classifier_t classifier);

/**
 * @brief Get last error message
 *
 * @return Error string or NULL
 */
NIMCP_EXPORT const char* portia_classification_get_error(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PORTIA_CLASSIFICATION_H
