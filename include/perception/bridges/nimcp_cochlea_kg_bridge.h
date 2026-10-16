/**
 * @file nimcp_cochlea_kg_bridge.h
 * @brief Cochlea-Knowledge Graph integration bridge
 *
 * WHAT: Connect cochlear processing to brain's knowledge graph
 * WHY:  Enable semantic understanding and auditory memory
 * HOW:  Register cochlear concepts, link to auditory memories
 *
 * KNOWLEDGE GRAPH INTEGRATION:
 * - Register cochlea as sensory module in KG
 * - Link frequency patterns to semantic concepts
 * - Store auditory signatures for recognition
 * - Query related sounds and contexts
 *
 * SELF-AWARENESS:
 * - Cochlea can introspect its own state via KG
 * - Report health, capabilities, current processing
 * - Enable recursive cognition about hearing
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#ifndef NIMCP_COCHLEA_KG_BRIDGE_H
#define NIMCP_COCHLEA_KG_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/error/nimcp_error_codes.h"
#include "perception/nimcp_cochlea.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct kg_engine kg_engine_t;
typedef uint64_t kg_node_id_t;

//=============================================================================
// Constants
//=============================================================================

#define COCHLEA_KG_MAX_SIGNATURES   256     /**< Max stored signatures */
#define COCHLEA_KG_FEATURE_DIM      64      /**< Signature feature dimension */

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Auditory signature for KG storage
 */
typedef struct {
    kg_node_id_t node_id;           /**< KG node for this signature */
    char label[64];                 /**< Human-readable label */

    float* spectral_template;       /**< Spectral signature */
    float* temporal_template;       /**< Temporal signature */
    uint32_t feature_dim;

    float confidence;               /**< Recognition confidence */
    uint64_t last_heard_ms;         /**< Last recognition time */
    uint32_t recognition_count;     /**< Times recognized */
} auditory_signature_t;

/**
 * @brief Recognition result
 */
typedef struct {
    bool recognized;                /**< Something recognized */
    kg_node_id_t matched_node;      /**< Best matching node */
    char matched_label[64];         /**< Label of match */
    float match_confidence;         /**< Match confidence [0-1] */

    kg_node_id_t* related_nodes;    /**< Related concepts */
    uint32_t num_related;
} recognition_result_t;

/**
 * @brief Cochlea self-representation in KG
 */
typedef struct {
    kg_node_id_t cochlea_node;      /**< This cochlea's node */
    kg_node_id_t health_node;       /**< Health status node */
    kg_node_id_t capability_node;   /**< Capabilities node */

    float current_health;           /**< Reported health */
    uint32_t num_channels;          /**< Reported channels */
    float freq_range_low_hz;        /**< Min frequency */
    float freq_range_high_hz;       /**< Max frequency */
} cochlea_kg_self_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Registration */
    const char* cochlea_name;       /**< Name in KG */
    bool register_on_create;        /**< Auto-register with KG */

    /* Recognition */
    bool enable_recognition;        /**< Enable sound recognition */
    float recognition_threshold;    /**< Min confidence for match */
    uint32_t max_signatures;        /**< Max stored signatures */

    /* Self-awareness */
    bool enable_self_awareness;     /**< Report state to KG */
    float update_interval_ms;       /**< How often to update KG */
} cochlea_kg_config_t;

/**
 * @brief Bridge instance (opaque)
 */
typedef struct cochlea_kg_bridge cochlea_kg_bridge_t;

//=============================================================================
// Configuration
//=============================================================================

cochlea_kg_config_t cochlea_kg_config_default(void);

//=============================================================================
// Core API
//=============================================================================

cochlea_kg_bridge_t* cochlea_kg_bridge_create(
    cochlea_t* cochlea,
    kg_engine_t* kg,
    const cochlea_kg_config_t* config
);

void cochlea_kg_bridge_destroy(cochlea_kg_bridge_t* bridge);

nimcp_error_t cochlea_kg_bridge_update(
    cochlea_kg_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    float dt_ms
);

nimcp_error_t cochlea_kg_bridge_reset(cochlea_kg_bridge_t* bridge);

//=============================================================================
// Signature Management
//=============================================================================

nimcp_error_t cochlea_kg_add_signature(
    cochlea_kg_bridge_t* bridge,
    const char* label,
    const float* spectral_features,
    const float* temporal_features,
    uint32_t feature_dim
);

nimcp_error_t cochlea_kg_remove_signature(
    cochlea_kg_bridge_t* bridge,
    kg_node_id_t node_id
);

nimcp_error_t cochlea_kg_get_signature(
    const cochlea_kg_bridge_t* bridge,
    kg_node_id_t node_id,
    auditory_signature_t* signature
);

//=============================================================================
// Recognition
//=============================================================================

nimcp_error_t cochlea_kg_recognize(
    cochlea_kg_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    recognition_result_t* result
);

nimcp_error_t cochlea_kg_learn_current(
    cochlea_kg_bridge_t* bridge,
    const char* label
);

//=============================================================================
// Self-Awareness
//=============================================================================

nimcp_error_t cochlea_kg_get_self(
    const cochlea_kg_bridge_t* bridge,
    cochlea_kg_self_t* self
);

nimcp_error_t cochlea_kg_update_self(
    cochlea_kg_bridge_t* bridge
);

kg_node_id_t cochlea_kg_get_node(
    const cochlea_kg_bridge_t* bridge
);

//=============================================================================
// Bidirectional Verification
//=============================================================================

bool cochlea_kg_verify_bidirectional(const cochlea_kg_bridge_t* bridge);
uint64_t cochlea_kg_get_last_outbound(const cochlea_kg_bridge_t* bridge);
uint64_t cochlea_kg_get_last_inbound(const cochlea_kg_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COCHLEA_KG_BRIDGE_H */
