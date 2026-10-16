/**
 * @file nimcp_temporal_thalamic_bridge.h
 * @brief Bridge between temporal cortex and thalamic router
 *
 * WHAT: Links temporal cortex to thalamic signal routing
 * WHY: Sensory signals flow through thalamus (MGN) to temporal cortex
 * HOW: Routes auditory through MGN, modulates attention gating
 *
 * BIOLOGICAL BASIS:
 * - Medial Geniculate Nucleus (MGN) relays auditory to A1
 * - Pulvinar modulates visual-temporal attention
 * - Reticular nucleus gates sensory flow based on attention
 * - Feedback from temporal cortex modulates thalamic processing
 *
 * PATHWAYS:
 * - Auditory: Cochlea -> MGN -> A1 (primary auditory cortex)
 * - Visual: V4/IT -> Pulvinar -> Temporal association areas
 * - Semantic: Prefrontal -> MD -> Temporal (top-down modulation)
 *
 * @author NIMCP Team
 * @date 2025-12-30
 */

#ifndef NIMCP_TEMPORAL_THALAMIC_BRIDGE_H
#define NIMCP_TEMPORAL_THALAMIC_BRIDGE_H

#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define BIO_MODULE_THALAMIC_TEMPORAL 0x1270

/**
 * @brief Thalamic nucleus types for temporal routing
 */
typedef enum {
    TEMPORAL_THALAMIC_MGN = 0,           /**< Medial Geniculate Nucleus (auditory) */
    TEMPORAL_THALAMIC_PULVINAR,          /**< Pulvinar (visual-temporal) */
    TEMPORAL_THALAMIC_MD,                /**< Mediodorsal (semantic-prefrontal) */
    TEMPORAL_THALAMIC_RETICULAR,         /**< Reticular (gating) */
    TEMPORAL_THALAMIC_COUNT
} temporal_thalamic_nucleus_t;

//=============================================================================
// Types
//=============================================================================

/**
 * @brief Thalamic routing state
 */
typedef struct {
    float mgn_gain;                      /**< MGN relay gain [0-2] */
    float pulvinar_attention;            /**< Pulvinar attention level [0-1] */
    float md_feedback;                   /**< MD feedback strength [0-1] */
    float reticular_gate;                /**< Reticular gating [0-1] */
    bool auditory_pathway_active;        /**< Auditory pathway is active */
    bool visual_pathway_active;          /**< Visual pathway is active */
    bool semantic_feedback_active;       /**< Semantic feedback is active */
} temporal_thalamic_state_t;

/**
 * @brief Signal routing request
 */
typedef struct {
    temporal_thalamic_nucleus_t source;  /**< Source nucleus */
    float* signal;                       /**< Signal vector */
    uint32_t signal_dim;                 /**< Signal dimension */
    float urgency;                       /**< Routing urgency [0-1] */
    float attention_boost;               /**< Attention modulation [-1, 1] */
    double timestamp_ms;                 /**< Request timestamp */
} temporal_thalamic_request_t;

/**
 * @brief Signal routing response
 */
typedef struct {
    float* routed_signal;                /**< Routed signal vector */
    uint32_t signal_dim;                 /**< Signal dimension */
    float effective_gain;                /**< Applied gain */
    float gating_applied;                /**< Gating factor applied */
    bool was_suppressed;                 /**< Signal was suppressed */
    double latency_ms;                   /**< Routing latency */
} temporal_thalamic_response_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    bool enable_attention_gating;        /**< Enable attention-based gating */
    bool enable_auditory_priority;       /**< Prioritize auditory signals */
    bool enable_visual_priority;         /**< Prioritize visual signals */
    bool enable_feedback_modulation;     /**< Enable top-down feedback */
    float min_urgency_threshold;         /**< Minimum urgency for routing */
    float auditory_boost;                /**< Auditory pathway boost [0-2] */
    float visual_boost;                  /**< Visual pathway boost [0-2] */
    float attention_decay_rate;          /**< Attention decay per step */
} temporal_thalamic_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t signals_routed;             /**< Total signals routed */
    uint64_t auditory_signals;           /**< Auditory signals processed */
    uint64_t visual_signals;             /**< Visual signals processed */
    uint64_t signals_suppressed;         /**< Signals suppressed by gate */
    float avg_mgn_gain;                  /**< Average MGN gain */
    float avg_pulvinar_attention;        /**< Average pulvinar attention */
    float avg_routing_latency_ms;        /**< Average routing latency */
} temporal_thalamic_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct temporal_thalamic_bridge temporal_thalamic_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default configuration
 * @return Default config with sensible values
 */
temporal_thalamic_config_t temporal_thalamic_default_config(void);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create temporal thalamic bridge
 * @param temporal Temporal adapter handle
 * @param router Thalamic router handle (can be NULL)
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
temporal_thalamic_bridge_t* temporal_thalamic_bridge_create(
    void* temporal,
    void* router,
    const temporal_thalamic_config_t* config
);

/**
 * @brief Destroy bridge
 * @param bridge Bridge to destroy
 */
void temporal_thalamic_bridge_destroy(temporal_thalamic_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int temporal_thalamic_bridge_reset(temporal_thalamic_bridge_t* bridge);

//=============================================================================
// Routing API
//=============================================================================

/**
 * @brief Route signal through thalamus
 * @param bridge Bridge handle
 * @param request Routing request
 * @param response Output: routing response
 * @return 0 on success, -1 on error
 */
int temporal_thalamic_route_signal(
    temporal_thalamic_bridge_t* bridge,
    const temporal_thalamic_request_t* request,
    temporal_thalamic_response_t* response
);

/**
 * @brief Route auditory signal through MGN
 * @param bridge Bridge handle
 * @param auditory_signal Auditory feature vector
 * @param signal_dim Signal dimension
 * @param response Output: routing response
 * @return 0 on success, -1 on error
 */
int temporal_thalamic_route_auditory(
    temporal_thalamic_bridge_t* bridge,
    const float* auditory_signal,
    uint32_t signal_dim,
    temporal_thalamic_response_t* response
);

/**
 * @brief Route visual signal through pulvinar
 * @param bridge Bridge handle
 * @param visual_signal Visual feature vector
 * @param signal_dim Signal dimension
 * @param response Output: routing response
 * @return 0 on success, -1 on error
 */
int temporal_thalamic_route_visual(
    temporal_thalamic_bridge_t* bridge,
    const float* visual_signal,
    uint32_t signal_dim,
    temporal_thalamic_response_t* response
);

//=============================================================================
// Attention Modulation API
//=============================================================================

/**
 * @brief Set attention level for nucleus
 * @param bridge Bridge handle
 * @param nucleus Target nucleus
 * @param attention Attention level [0-1]
 * @return 0 on success, -1 on error
 */
int temporal_thalamic_set_attention(
    temporal_thalamic_bridge_t* bridge,
    temporal_thalamic_nucleus_t nucleus,
    float attention
);

/**
 * @brief Get current thalamic state
 * @param bridge Bridge handle
 * @param state Output: current state
 * @return 0 on success, -1 on error
 */
int temporal_thalamic_get_state(
    const temporal_thalamic_bridge_t* bridge,
    temporal_thalamic_state_t* state
);

/**
 * @brief Apply top-down feedback modulation
 * @param bridge Bridge handle
 * @param feedback_signal Feedback signal from higher areas
 * @param signal_dim Signal dimension
 * @return 0 on success, -1 on error
 */
int temporal_thalamic_apply_feedback(
    temporal_thalamic_bridge_t* bridge,
    const float* feedback_signal,
    uint32_t signal_dim
);

//=============================================================================
// Bio-Async API
//=============================================================================

/**
 * @brief Register with bio-async router
 * @param bridge Bridge handle
 * @param router Bio router handle
 * @return 0 on success, -1 on error
 */
int temporal_thalamic_bridge_register_bio_async(
    temporal_thalamic_bridge_t* bridge,
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
int temporal_thalamic_bridge_get_stats(
    const temporal_thalamic_bridge_t* bridge,
    temporal_thalamic_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 */
void temporal_thalamic_bridge_reset_stats(temporal_thalamic_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TEMPORAL_THALAMIC_BRIDGE_H */
