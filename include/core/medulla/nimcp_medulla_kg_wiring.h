/**
 * @file nimcp_medulla_kg_wiring.h
 * @brief Knowledge Graph wiring for Medulla module
 *
 * WHAT: Registers medulla concepts (arousal, protection, circadian, vital functions)
 *       as nodes in the brain's internal Knowledge Graph.
 *
 * WHY:  KG integration enables:
 *       - Semantic queries about medulla state ("what is current arousal?")
 *       - Cross-module reasoning about consciousness and protection
 *       - Introspection of circadian and arousal relationships
 *       - History tracking of state transitions
 *
 * HOW:  Creates nodes and edges in the brain KG representing:
 *       - Arousal level changes (with timestamps)
 *       - Protection level transitions
 *       - Circadian phase transitions
 *       - Emergency shutdown events
 *       - State history for analysis
 *
 * @author NIMCP Development Team
 * @date 2026-02-01
 * @version 1.0.0
 */

#ifndef NIMCP_MEDULLA_KG_WIRING_H
#define NIMCP_MEDULLA_KG_WIRING_H

#include "core/medulla/nimcp_medulla.h"
#include "core/brain/nimcp_brain_kg.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Types
//=============================================================================

/**
 * @brief Opaque handle to medulla KG wiring instance
 */
typedef struct medulla_kg_wiring* medulla_kg_wiring_t;

/**
 * @brief Medulla KG wiring configuration
 */
typedef struct {
    bool enable_state_logging;        /**< Log state changes to KG */
    bool enable_arousal_history;      /**< Track arousal level history */
    bool enable_protection_history;   /**< Track protection level history */
    bool enable_circadian_history;    /**< Track circadian phase history */
    uint32_t history_depth;           /**< Maximum history entries to keep */
    float log_threshold;              /**< Only log changes above this threshold */
} medulla_kg_config_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default KG wiring configuration
 *
 * DEFAULTS:
 * - enable_state_logging: true
 * - enable_arousal_history: true
 * - enable_protection_history: true
 * - enable_circadian_history: true
 * - history_depth: 100
 * - log_threshold: 0.01f
 *
 * @return Default configuration
 */
medulla_kg_config_t medulla_kg_default_config(void);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create medulla KG wiring instance
 *
 * WHAT: Create wiring between medulla and knowledge graph
 * WHY:  Enable KG-based state tracking and queries
 * HOW:  Allocate structure, register medulla node in KG
 *
 * @param config Configuration (NULL for defaults)
 * @param medulla Medulla instance to wire
 * @param kg Knowledge graph to wire to
 * @return KG wiring handle or NULL on failure
 */
medulla_kg_wiring_t medulla_kg_create(
    const medulla_kg_config_t* config,
    medulla_t medulla,
    brain_kg_t* kg
);

/**
 * @brief Destroy medulla KG wiring instance
 *
 * WHAT: Clean up KG wiring resources
 * WHY:  Proper resource deallocation
 * HOW:  Unregister from KG, free memory
 *
 * @param wiring KG wiring handle to destroy (NULL safe)
 */
void medulla_kg_destroy(medulla_kg_wiring_t wiring);

//=============================================================================
// State Logging API
//=============================================================================

/**
 * @brief Log current medulla state to KG
 *
 * WHAT: Record complete medulla state snapshot to KG
 * WHY:  Enable state queries and historical analysis
 * HOW:  Read medulla state, update KG node metadata
 *
 * @param wiring KG wiring handle
 * @return 0 on success, -1 on error
 */
int medulla_kg_log_state(medulla_kg_wiring_t wiring);

/**
 * @brief Log arousal level change to KG
 *
 * WHAT: Record arousal transition in KG history
 * WHY:  Track arousal dynamics over time
 * HOW:  Add timestamped entry to arousal history
 *
 * @param wiring KG wiring handle
 * @param old_arousal Previous arousal level
 * @param new_arousal New arousal level
 * @return 0 on success, -1 on error
 */
int medulla_kg_log_arousal_change(
    medulla_kg_wiring_t wiring,
    float old_arousal,
    float new_arousal
);

/**
 * @brief Log protection level change to KG
 *
 * WHAT: Record protection level transition in KG history
 * WHY:  Track protection state dynamics
 * HOW:  Add timestamped entry to protection history
 *
 * @param wiring KG wiring handle
 * @param old_level Previous protection level
 * @param new_level New protection level
 * @return 0 on success, -1 on error
 */
int medulla_kg_log_protection_change(
    medulla_kg_wiring_t wiring,
    protection_level_t old_level,
    protection_level_t new_level
);

/**
 * @brief Log emergency shutdown event to KG
 *
 * WHAT: Record emergency shutdown in KG
 * WHY:  Critical event tracking for analysis
 * HOW:  Add timestamped emergency event with reason
 *
 * @param wiring KG wiring handle
 * @param reason Reason for emergency shutdown
 * @return 0 on success, -1 on error
 */
int medulla_kg_log_emergency(
    medulla_kg_wiring_t wiring,
    const char* reason
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get arousal history from KG
 *
 * WHAT: Retrieve historical arousal values
 * WHY:  Enable analysis of arousal dynamics
 * HOW:  Query KG for arousal history entries
 *
 * @param wiring KG wiring handle
 * @param history Output array for arousal values
 * @param max_entries Maximum entries to retrieve
 * @param count Output: actual number of entries retrieved
 * @return 0 on success, -1 on error
 */
int medulla_kg_get_arousal_history(
    medulla_kg_wiring_t wiring,
    float* history,
    size_t max_entries,
    size_t* count
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MEDULLA_KG_WIRING_H */
