/**
 * @file nimcp_emotion_tensor_bridge.h
 * @brief Bridge between tensor-based emotions and swarm emotional contagion
 *
 * WHAT: Bidirectional conversion between emotion tensor and swarm emotional systems
 * WHY:  Individual brains use tensor representation, swarm uses emotion_type_t
 * HOW:  Mapping functions + bio-async message handlers for synchronization
 *
 * INTEGRATION ARCHITECTURE:
 * ┌─────────────────────────────────────────────────────────────────────────────┐
 * │                     EMOTION TENSOR BRIDGE                                    │
 * │                                                                              │
 * │  ┌────────────────────┐         ┌──────────────────────────────────────┐    │
 * │  │  emotion_tensor_t  │ ◄─────► │  emotional_contagion_t (swarm)       │    │
 * │  │  (8 channels)      │         │  (20 emotion types)                  │    │
 * │  └────────────────────┘         └──────────────────────────────────────┘    │
 * │           │                                    │                             │
 * │           │                                    │                             │
 * │           ▼                                    ▼                             │
 * │  ┌────────────────────┐         ┌──────────────────────────────────────┐    │
 * │  │  emotional_tag_t   │         │  BIO_MSG_EMOTION_SWARM_SYNC          │    │
 * │  │  (valence/arousal) │         │  (bio-async synchronization)         │    │
 * │  └────────────────────┘         └──────────────────────────────────────┘    │
 * └─────────────────────────────────────────────────────────────────────────────┘
 *
 * EMOTION MAPPING (Plutchik 8 ↔ Swarm 20):
 * - Direct mappings: JOY, TRUST, FEAR, SURPRISE, SADNESS, DISGUST, ANGER, ANTICIPATION
 * - Derived mappings: CURIOSITY, EXCITEMENT, HOPE, etc. → compound emotions
 *
 * CODING STANDARDS:
 * - Guard clauses (no nested ifs)
 * - Helper functions (<50 lines)
 * - WHAT-WHY-HOW documentation
 * - Single Responsibility Principle
 *
 * @author NIMCP Development Team
 * @date 2025-12-09
 * @version 1.0.0
 */

#ifndef NIMCP_EMOTION_TENSOR_BRIDGE_H
#define NIMCP_EMOTION_TENSOR_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_emotion_tensor.h"
#include "cognitive/nimcp_emotional_tagging.h"
#include "swarm/nimcp_emotional_contagion.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * TYPE DEFINITIONS
 *============================================================================*/

/**
 * @brief Bridge configuration
 */
typedef struct {
    float sync_threshold;           /**< Min change to trigger sync [0, 1] */
    float blend_factor;             /**< Default blending for sync [0, 1] */
    uint32_t broadcast_interval_ms; /**< State broadcast interval */
    bool auto_broadcast;            /**< Auto-broadcast on significant changes */
    bool enable_compound_detection; /**< Broadcast compound emotion events */
    bool enable_contradiction_detection; /**< Broadcast contradiction events */
} emotion_tensor_bridge_config_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct emotion_tensor_bridge emotion_tensor_bridge_t;

/**
 * @brief Emotion mapping result
 */
typedef struct {
    emotion_type_t swarm_emotion;   /**< Primary swarm emotion */
    float intensity;                /**< Intensity [0, 1] */
    bool is_compound;               /**< Mapped from compound emotion */
    emotion_compound_t compound;    /**< If is_compound, which compound */
} tensor_swarm_mapping_t;

/*=============================================================================
 * LIFECYCLE API
 *============================================================================*/

/**
 * @brief Create emotion tensor bridge
 *
 * WHAT: Initialize bridge between tensor and swarm systems
 * WHY:  Enable emotion synchronization
 * HOW:  Register with bio-router, initialize mapping tables
 *
 * @param tensor Emotion tensor system to bridge
 * @param contagion Swarm emotional contagion system (can be NULL)
 * @param config Configuration (NULL = defaults)
 * @return Bridge handle or NULL on error
 */
emotion_tensor_bridge_t* emotion_tensor_bridge_create(
    emotion_tensor_system_t* tensor,
    emotional_contagion_t* contagion,
    const emotion_tensor_bridge_config_t* config
);

/**
 * @brief Get default bridge configuration
 */
emotion_tensor_bridge_config_t emotion_tensor_bridge_default_config(void);

/**
 * @brief Destroy bridge
 */
void emotion_tensor_bridge_destroy(emotion_tensor_bridge_t* bridge);

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *============================================================================*/

/**
 * @brief Register bridge with bio-async router
 *
 * WHAT: Register message handlers for emotion sync
 * WHY:  Enable asynchronous emotion communication
 * HOW:  Register handlers for BIO_MSG_EMOTION_TENSOR_* messages
 *
 * @param bridge Bridge handle
 * @param router Bio-async router
 * @return NIMCP_SUCCESS or error
 */
nimcp_result_t emotion_tensor_bridge_register_bioasync(
    emotion_tensor_bridge_t* bridge,
    bio_router_t router
);

/**
 * @brief Broadcast current tensor state via bio-async
 *
 * WHAT: Send BIO_MSG_EMOTION_TENSOR_UPDATE to all subscribers
 * WHY:  Notify system of emotional state changes
 * HOW:  Convert tensor to message, broadcast on serotonin channel
 *
 * @param bridge Bridge handle
 * @return NIMCP_SUCCESS or error
 */
nimcp_result_t emotion_tensor_bridge_broadcast_state(
    emotion_tensor_bridge_t* bridge
);

/**
 * @brief Handle incoming emotion message
 *
 * WHAT: Process BIO_MSG_EMOTION_* messages
 * WHY:  React to external emotion events
 * HOW:  Dispatch to appropriate handler based on message type
 *
 * @param bridge Bridge handle
 * @param msg Message pointer
 * @param msg_size Message size
 * @return NIMCP_SUCCESS or error
 */
nimcp_result_t emotion_tensor_bridge_handle_message(
    emotion_tensor_bridge_t* bridge,
    const void* msg,
    size_t msg_size
);

/*=============================================================================
 * TENSOR ↔ SWARM CONVERSION
 *============================================================================*/

/**
 * @brief Convert tensor state to swarm emotion type
 *
 * WHAT: Map tensor channels to single swarm emotion
 * WHY:  Swarm contagion uses discrete emotion types
 * HOW:  Find dominant tensor emotion, map to swarm type
 *
 * @param tensor Tensor system
 * @param mapping Output mapping result
 * @return NIMCP_SUCCESS or error
 */
nimcp_result_t emotion_tensor_to_swarm(
    const emotion_tensor_system_t* tensor,
    tensor_swarm_mapping_t* mapping
);

/**
 * @brief Convert swarm emotion to tensor update
 *
 * WHAT: Apply swarm emotion state to tensor
 * WHY:  Receive emotions from swarm contagion
 * HOW:  Map swarm type to tensor channel(s), apply intensity
 *
 * @param tensor Tensor system to update
 * @param swarm_emotion Swarm emotion type
 * @param intensity Emotion intensity [0, 1]
 * @param blend_factor How much to blend (0=ignore, 1=replace)
 * @param timestamp_ms Current timestamp
 * @return NIMCP_SUCCESS or error
 */
nimcp_result_t emotion_tensor_from_swarm(
    emotion_tensor_system_t* tensor,
    emotion_type_t swarm_emotion,
    float intensity,
    float blend_factor,
    uint64_t timestamp_ms
);

/**
 * @brief Get tensor channel for swarm emotion
 *
 * WHAT: Map swarm emotion type to primary tensor channel
 * WHY:  Direct mapping for simple emotions
 * HOW:  Lookup table based on emotion semantics
 *
 * @param swarm_emotion Swarm emotion type
 * @return Primary tensor channel or -1 if compound needed
 */
emotion_primary_t emotion_swarm_to_tensor_channel(emotion_type_t swarm_emotion);

/**
 * @brief Get swarm emotion for tensor channel
 *
 * WHAT: Map primary tensor channel to swarm emotion
 * WHY:  Direct mapping for broadcasting
 * HOW:  Lookup table
 *
 * @param tensor_emotion Tensor primary emotion
 * @return Swarm emotion type
 */
emotion_type_t emotion_tensor_channel_to_swarm(emotion_primary_t tensor_emotion);

/*=============================================================================
 * TENSOR ↔ EMOTIONAL TAG CONVERSION
 *============================================================================*/

/**
 * @brief Convert tensor to emotional_tag_t
 *
 * WHAT: Generate backward-compatible emotional tag
 * WHY:  Support legacy systems using valence/arousal
 * HOW:  Compute scalar valence/arousal from tensor channels
 *
 * @param tensor Tensor system
 * @param tag Output emotional tag
 * @return NIMCP_SUCCESS or error
 */
nimcp_result_t emotion_tensor_to_tag(
    const emotion_tensor_system_t* tensor,
    emotional_tag_t* tag
);

/**
 * @brief Update tensor from emotional_tag_t
 *
 * WHAT: Set tensor state from scalar valence/arousal
 * WHY:  Accept input from legacy systems
 * HOW:  Infer primary emotions from valence/arousal quadrant
 *
 * @param tensor Tensor system to update
 * @param tag Source emotional tag
 * @param timestamp_ms Current timestamp
 * @return NIMCP_SUCCESS or error
 */
nimcp_result_t emotion_tensor_from_tag(
    emotion_tensor_system_t* tensor,
    const emotional_tag_t* tag,
    uint64_t timestamp_ms
);

/*=============================================================================
 * SYNCHRONIZATION API
 *============================================================================*/

/**
 * @brief Sync tensor with swarm agent emotional state
 *
 * WHAT: Bidirectional synchronization with specific agent
 * WHY:  Keep individual and collective emotions aligned
 * HOW:  Send BIO_MSG_EMOTION_SWARM_SYNC message
 *
 * @param bridge Bridge handle
 * @param agent_id Target agent ID
 * @param direction Sync direction (see BIO_EMOTION_SYNC_*)
 * @param blend_factor Blending factor [0, 1]
 * @return NIMCP_SUCCESS or error
 */
nimcp_result_t emotion_tensor_bridge_sync_agent(
    emotion_tensor_bridge_t* bridge,
    uint32_t agent_id,
    uint8_t direction,
    float blend_factor
);

/**
 * @brief Propagate tensor emotion to swarm
 *
 * WHAT: Trigger emotion propagation from individual to swarm
 * WHY:  Individual emotional events can spread to collective
 * HOW:  Convert to swarm emotion, call emotional_contagion_trigger_outbreak
 *
 * @param bridge Bridge handle
 * @param max_depth Maximum propagation depth (0 = use default)
 * @return NIMCP_SUCCESS or error
 */
nimcp_result_t emotion_tensor_bridge_propagate_to_swarm(
    emotion_tensor_bridge_t* bridge,
    uint32_t max_depth
);

/**
 * @brief Update tensor from swarm collective state
 *
 * WHAT: Apply swarm collective emotion to tensor
 * WHY:  Individual affected by collective mood
 * HOW:  Query collective state, blend with tensor
 *
 * @param bridge Bridge handle
 * @param blend_factor How much collective affects individual [0, 1]
 * @return NIMCP_SUCCESS or error
 */
nimcp_result_t emotion_tensor_bridge_update_from_collective(
    emotion_tensor_bridge_t* bridge,
    float blend_factor
);

/*=============================================================================
 * EVENT NOTIFICATION API
 *============================================================================*/

/**
 * @brief Broadcast compound emotion detection
 *
 * WHAT: Notify system of active compound emotion
 * WHY:  Complex emotions (bittersweet) are psychologically significant
 * HOW:  Send BIO_MSG_EMOTION_TENSOR_COMPOUND message
 *
 * @param bridge Bridge handle
 * @param compound Detected compound emotion
 * @param activation Activation level [0, 1]
 * @return NIMCP_SUCCESS or error
 */
nimcp_result_t emotion_tensor_bridge_notify_compound(
    emotion_tensor_bridge_t* bridge,
    emotion_compound_t compound,
    float activation
);

/**
 * @brief Broadcast emotional contradiction
 *
 * WHAT: Notify system of contradictory emotions
 * WHY:  Ambivalence indicates internal conflict
 * HOW:  Send BIO_MSG_EMOTION_TENSOR_CONTRADICTION message
 *
 * @param bridge Bridge handle
 * @param emotion_a First conflicting emotion
 * @param emotion_b Second conflicting emotion
 * @param conflict_intensity Conflict magnitude [0, 1]
 * @return NIMCP_SUCCESS or error
 */
nimcp_result_t emotion_tensor_bridge_notify_contradiction(
    emotion_tensor_bridge_t* bridge,
    emotion_primary_t emotion_a,
    emotion_primary_t emotion_b,
    float conflict_intensity
);

/*=============================================================================
 * UTILITY API
 *============================================================================*/

/**
 * @brief Check if tensor has changed significantly since last sync
 *
 * WHAT: Determine if sync is needed
 * WHY:  Avoid unnecessary sync traffic
 * HOW:  Compare against cached state using threshold
 *
 * @param bridge Bridge handle
 * @return true if sync needed
 */
bool emotion_tensor_bridge_needs_sync(const emotion_tensor_bridge_t* bridge);

/**
 * @brief Get bridge statistics
 */
typedef struct {
    uint64_t syncs_sent;
    uint64_t syncs_received;
    uint64_t broadcasts_sent;
    uint64_t compounds_detected;
    uint64_t contradictions_detected;
    float avg_sync_latency_ms;
} emotion_tensor_bridge_stats_t;

nimcp_result_t emotion_tensor_bridge_get_stats(
    const emotion_tensor_bridge_t* bridge,
    emotion_tensor_bridge_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMOTION_TENSOR_BRIDGE_H */
