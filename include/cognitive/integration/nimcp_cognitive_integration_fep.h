/**
 * @file nimcp_cognitive_integration_fep.h
 * @brief FEP Orchestrator integration for Cognitive Integration bridges
 * @version 1.0.0
 * @date 2025
 *
 * WHAT: Provides FEP (Free Energy Principle) orchestrator registration for all
 *       cognitive integration bridges (hub and 8 bidirectional bridges).
 *
 * WHY: The FEP orchestrator coordinates all bridges in NIMCP with proper
 *      timescales and free energy minimization. Cognitive integration bridges
 *      need to participate in this coordinated update cycle.
 *
 * HOW: Each bridge provides an FEP update callback that:
 *      1. Minimizes prediction error between connected modules
 *      2. Updates internal state based on free energy gradients
 *      3. Reports surprise/prediction error metrics
 *
 * INTEGRATION ARCHITECTURE:
 *
 *   FEP Orchestrator (50ms cognitive cycle)
 *          │
 *          ├── Cognitive Integration Hub FEP Bridge
 *          │   └── Routes events, manages subscriptions
 *          │
 *          ├── Emotion-Memory FEP Bridge
 *          │   └── Predicts emotional tags, minimizes memory-emotion mismatch
 *          │
 *          ├── Attention-WM FEP Bridge
 *          │   └── Predicts attention focus, minimizes WM access errors
 *          │
 *          ├── Curiosity-Reasoning FEP Bridge
 *          │   └── Predicts exploration value, minimizes epistemic surprise
 *          │
 *          ├── Ethics-Executive FEP Bridge
 *          │   └── Predicts ethical constraints, minimizes value violations
 *          │
 *          ├── ToM-Social FEP Bridge
 *          │   └── Predicts mental states, minimizes social inference error
 *          │
 *          ├── Self-Introspection FEP Bridge
 *          │   └── Predicts self-state, minimizes self-model error
 *          │
 *          ├── Emotion-Executive FEP Bridge
 *          │   └── Predicts emotional influence, minimizes decision bias error
 *          │
 *          └── GW-Cognitive FEP Bridge
 *              └── Predicts conscious content, minimizes broadcast error
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_COGNITIVE_INTEGRATION_FEP_H
#define NIMCP_COGNITIVE_INTEGRATION_FEP_H

#include <stdint.h>
#include <stdbool.h>

/* Forward declarations */
typedef struct fep_orchestrator fep_orchestrator_t;
typedef struct cognitive_integration_hub_struct* cognitive_integration_hub_t;
typedef struct emotion_memory_bridge emotion_memory_bridge_t;
typedef struct attention_wm_bridge attention_wm_bridge_t;
typedef struct curiosity_reasoning_bridge curiosity_reasoning_bridge_t;
typedef struct ethics_executive_bridge ethics_executive_bridge_t;
typedef struct tom_social_bridge tom_social_bridge_t;
typedef struct self_introspection_bridge self_introspection_bridge_t;
typedef struct emotion_executive_bridge emotion_executive_bridge_t;
typedef struct gw_cognitive_bridge gw_cognitive_bridge_t;

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * FEP BRIDGE IDENTIFIERS
 * ============================================================================ */

/**
 * @brief FEP bridge IDs for cognitive integration bridges
 *
 * These IDs are assigned by the FEP orchestrator upon registration.
 * Use cognitive_integration_fep_get_bridge_id() to retrieve them.
 */
typedef enum {
    COG_INTEG_FEP_HUB = 0,              /**< Integration hub */
    COG_INTEG_FEP_EMOTION_MEMORY,       /**< Emotion-Memory bridge */
    COG_INTEG_FEP_ATTENTION_WM,         /**< Attention-WM bridge */
    COG_INTEG_FEP_CURIOSITY_REASONING,  /**< Curiosity-Reasoning bridge */
    COG_INTEG_FEP_ETHICS_EXECUTIVE,     /**< Ethics-Executive bridge */
    COG_INTEG_FEP_TOM_SOCIAL,           /**< ToM-Social bridge */
    COG_INTEG_FEP_SELF_INTROSPECTION,   /**< Self-Introspection bridge */
    COG_INTEG_FEP_EMOTION_EXECUTIVE,    /**< Emotion-Executive bridge */
    COG_INTEG_FEP_GW_COGNITIVE,         /**< GW-Cognitive bridge */
    COG_INTEG_FEP_COUNT                 /**< Total bridge count */
} cognitive_integration_fep_id_t;

/* ============================================================================
 * FEP METRICS STRUCTURE
 * ============================================================================ */

/**
 * @brief FEP metrics for a cognitive integration bridge
 *
 * WHAT: Tracks free energy and prediction error metrics
 * WHY: FEP orchestrator uses these to coordinate updates
 * HOW: Updated by each bridge during FEP update cycle
 */
typedef struct {
    float free_energy;              /**< Current free energy estimate */
    float prediction_error;         /**< Current prediction error */
    float surprise;                 /**< Bayesian surprise measure */
    float entropy;                  /**< State uncertainty */
    uint64_t last_update_time;      /**< Timestamp of last update */
    uint32_t update_count;          /**< Total updates performed */
} cognitive_integration_fep_metrics_t;

/**
 * @brief Aggregated FEP statistics for all cognitive integration bridges
 */
typedef struct {
    cognitive_integration_fep_metrics_t bridges[COG_INTEG_FEP_COUNT];
    float total_free_energy;        /**< Sum of all bridge free energies */
    float avg_prediction_error;     /**< Average prediction error */
    float max_surprise;             /**< Maximum surprise across bridges */
    uint32_t registered_count;      /**< Number of registered bridges */
    uint32_t active_count;          /**< Number of active bridges */
    uint64_t total_updates;         /**< Total updates across all bridges */
} cognitive_integration_fep_stats_t;

/* ============================================================================
 * UNIFIED REGISTRATION
 * ============================================================================ */

/**
 * @brief Register all cognitive integration bridges with FEP orchestrator
 *
 * WHAT: Registers the integration hub and all 8 bridges with FEP
 * WHY: Single call to wire all cognitive integration into FEP coordination
 * HOW: Calls individual registration functions for each bridge
 *
 * @param orchestrator FEP orchestrator instance
 * @param hub Cognitive integration hub (required)
 * @param emotion_memory Emotion-Memory bridge (optional, can be NULL)
 * @param attention_wm Attention-WM bridge (optional, can be NULL)
 * @param curiosity_reasoning Curiosity-Reasoning bridge (optional, can be NULL)
 * @param ethics_executive Ethics-Executive bridge (optional, can be NULL)
 * @param tom_social ToM-Social bridge (optional, can be NULL)
 * @param self_introspection Self-Introspection bridge (optional, can be NULL)
 * @param emotion_executive Emotion-Executive bridge (optional, can be NULL)
 * @param gw_cognitive GW-Cognitive bridge (optional, can be NULL)
 * @return Number of bridges successfully registered, -1 on critical error
 *
 * ERRORS:
 * - Returns -1 if orchestrator is NULL
 * - Returns -1 if hub is NULL (hub is required)
 * - Partial registration is possible (some bridges may fail)
 *
 * THREAD-SAFE: Yes
 */
int cognitive_integration_fep_register_all(
    fep_orchestrator_t* orchestrator,
    cognitive_integration_hub_t hub,
    emotion_memory_bridge_t* emotion_memory,
    attention_wm_bridge_t* attention_wm,
    curiosity_reasoning_bridge_t* curiosity_reasoning,
    ethics_executive_bridge_t* ethics_executive,
    tom_social_bridge_t* tom_social,
    self_introspection_bridge_t* self_introspection,
    emotion_executive_bridge_t* emotion_executive,
    gw_cognitive_bridge_t* gw_cognitive
);

/**
 * @brief Unregister all cognitive integration bridges from FEP orchestrator
 *
 * WHAT: Removes all cognitive integration bridges from FEP
 * WHY: Clean shutdown or reconfiguration
 * HOW: Calls unregister for each registered bridge
 *
 * @param orchestrator FEP orchestrator instance
 * @return 0 on success, -1 on error
 *
 * THREAD-SAFE: Yes
 */
int cognitive_integration_fep_unregister_all(fep_orchestrator_t* orchestrator);

/* ============================================================================
 * INDIVIDUAL BRIDGE REGISTRATION
 * ============================================================================ */

/**
 * @brief Register cognitive integration hub with FEP orchestrator
 *
 * @param orchestrator FEP orchestrator instance
 * @param hub Cognitive integration hub
 * @param bridge_id_out Output: assigned FEP bridge ID
 * @return 0 on success, -1 on error
 */
int cognitive_hub_fep_register(
    fep_orchestrator_t* orchestrator,
    cognitive_integration_hub_t hub,
    uint32_t* bridge_id_out
);

/**
 * @brief Register emotion-memory bridge with FEP orchestrator
 */
int emotion_memory_bridge_fep_register(
    fep_orchestrator_t* orchestrator,
    emotion_memory_bridge_t* bridge,
    uint32_t* bridge_id_out
);

/**
 * @brief Register attention-wm bridge with FEP orchestrator
 */
int attention_wm_bridge_fep_register(
    fep_orchestrator_t* orchestrator,
    attention_wm_bridge_t* bridge,
    uint32_t* bridge_id_out
);

/**
 * @brief Register curiosity-reasoning bridge with FEP orchestrator
 */
int curiosity_reasoning_bridge_fep_register(
    fep_orchestrator_t* orchestrator,
    curiosity_reasoning_bridge_t* bridge,
    uint32_t* bridge_id_out
);

/**
 * @brief Register ethics-executive bridge with FEP orchestrator
 */
int ethics_executive_bridge_fep_register(
    fep_orchestrator_t* orchestrator,
    ethics_executive_bridge_t* bridge,
    uint32_t* bridge_id_out
);

/**
 * @brief Register tom-social bridge with FEP orchestrator
 */
int tom_social_bridge_fep_register(
    fep_orchestrator_t* orchestrator,
    tom_social_bridge_t* bridge,
    uint32_t* bridge_id_out
);

/**
 * @brief Register self-introspection bridge with FEP orchestrator
 */
int self_introspection_bridge_fep_register(
    fep_orchestrator_t* orchestrator,
    self_introspection_bridge_t* bridge,
    uint32_t* bridge_id_out
);

/**
 * @brief Register emotion-executive bridge with FEP orchestrator
 */
int emotion_executive_bridge_fep_register(
    fep_orchestrator_t* orchestrator,
    emotion_executive_bridge_t* bridge,
    uint32_t* bridge_id_out
);

/**
 * @brief Register gw-cognitive bridge with FEP orchestrator
 */
int gw_cognitive_bridge_fep_register(
    fep_orchestrator_t* orchestrator,
    gw_cognitive_bridge_t* bridge,
    uint32_t* bridge_id_out
);

/* ============================================================================
 * FEP METRICS AND STATISTICS
 * ============================================================================ */

/**
 * @brief Get FEP metrics for a specific bridge
 *
 * @param bridge_type Bridge identifier
 * @param metrics_out Output: bridge metrics
 * @return 0 on success, -1 on error
 */
int cognitive_integration_fep_get_metrics(
    cognitive_integration_fep_id_t bridge_type,
    cognitive_integration_fep_metrics_t* metrics_out
);

/**
 * @brief Get aggregated FEP statistics for all cognitive integration bridges
 *
 * @param stats_out Output: aggregated statistics
 * @return 0 on success, -1 on error
 */
int cognitive_integration_fep_get_stats(
    cognitive_integration_fep_stats_t* stats_out
);

/**
 * @brief Get the FEP bridge ID for a registered bridge
 *
 * @param bridge_type Bridge identifier
 * @return Bridge ID if registered, 0 if not registered, (uint32_t)-1 on error
 */
uint32_t cognitive_integration_fep_get_bridge_id(
    cognitive_integration_fep_id_t bridge_type
);

/* ============================================================================
 * FEP UPDATE CALLBACKS (Internal - used by FEP orchestrator)
 * ============================================================================ */

/**
 * @brief FEP update callback for cognitive integration hub
 *
 * WHAT: Called by FEP orchestrator during cognitive update cycle
 * WHY: Process queued events, update subscriptions, minimize routing error
 * HOW: Flushes async queue, processes pending events, updates metrics
 *
 * @param handle Opaque handle (cognitive_integration_hub_t)
 * @return 0 on success, -1 on error
 */
int cognitive_hub_fep_update(void* handle);

/**
 * @brief FEP update callback for emotion-memory bridge
 */
int emotion_memory_bridge_fep_update(void* handle);

/**
 * @brief FEP update callback for attention-wm bridge
 */
int attention_wm_bridge_fep_update(void* handle);

/**
 * @brief FEP update callback for curiosity-reasoning bridge
 */
int curiosity_reasoning_bridge_fep_update(void* handle);

/**
 * @brief FEP update callback for ethics-executive bridge
 */
int ethics_executive_bridge_fep_update(void* handle);

/**
 * @brief FEP update callback for tom-social bridge
 */
int tom_social_bridge_fep_update(void* handle);

/**
 * @brief FEP update callback for self-introspection bridge
 */
int self_introspection_bridge_fep_update(void* handle);

/**
 * @brief FEP update callback for emotion-executive bridge
 */
int emotion_executive_bridge_fep_update(void* handle);

/**
 * @brief FEP update callback for gw-cognitive bridge
 */
int gw_cognitive_bridge_fep_update(void* handle);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COGNITIVE_INTEGRATION_FEP_H */
