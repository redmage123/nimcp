//=============================================================================
// nimcp_ofc_kg_wiring.h - OFC Knowledge Graph Registration
//=============================================================================
/**
 * @file nimcp_ofc_kg_wiring.h
 * @brief Knowledge Graph registration for Orbitofrontal Cortex (OFC) module
 *
 * WHAT: Registers OFC concepts (value computation, decision-making, emotion
 *       integration) as nodes in the brain's internal Knowledge Graph.
 *
 * WHY:  KG integration enables:
 *       - Semantic queries about OFC state ("what is current value estimate?")
 *       - Cross-module reasoning about value-based decisions
 *       - Introspection of reward and emotion relationships
 *       - Graph-based analysis of decision dynamics
 *
 * HOW:  Creates hierarchical node structure:
 *       - OFC root node
 *         ├── Lateral OFC (stimulus-reward associations)
 *         ├── Medial OFC (value comparison, choice)
 *         ├── Anterior OFC (abstract/social rewards)
 *         ├── Posterior OFC (primary reward processing)
 *         ├── Value computation nodes
 *         │   ├── Expected value
 *         │   ├── Received value
 *         │   ├── Prediction error
 *         │   ├── Probability
 *         │   ├── Magnitude
 *         │   ├── Delay
 *         │   ├── Risk
 *         │   └── Social value
 *         └── Decision nodes
 *             ├── Binary choice
 *             ├── Multi-alternative
 *             ├── Sequential
 *             ├── Social decision
 *             └── Moral judgment
 *
 * EDGES: Represent causal/functional relationships:
 *       - Value computation -> Decision (DRIVES)
 *       - Emotion -> Value (MODULATES)
 *       - Prediction error -> Learning (TRIGGERS)
 *       - Risk -> Decision threshold (INFLUENCES)
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_OFC_KG_WIRING_H
#define NIMCP_OFC_KG_WIRING_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"
#include "core/brain/nimcp_brain_kg.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define OFC_KG_MODULE_NAME        "ofc_kg_wiring"

/** OFC root node name */
#define OFC_KG_ROOT_NAME          "orbitofrontal_cortex"

/** Lateral OFC subsystem node name */
#define OFC_KG_LATERAL_NAME       "lateral_ofc"

/** Medial OFC subsystem node name */
#define OFC_KG_MEDIAL_NAME        "medial_ofc"

/** Anterior OFC subsystem node name */
#define OFC_KG_ANTERIOR_NAME      "anterior_ofc"

/** Posterior OFC subsystem node name */
#define OFC_KG_POSTERIOR_NAME     "posterior_ofc"

/** Value computation subsystem node name */
#define OFC_KG_VALUE_NAME         "value_computation"

/** Decision subsystem node name */
#define OFC_KG_DECISION_NAME      "decision_system"

//=============================================================================
// Node Type Extensions (for OFC-specific concepts)
//=============================================================================

/**
 * @brief OFC-specific KG node types
 *
 * These extend the base brain_kg_node_type_t with OFC-specific types.
 * Values start at 0x2000 to avoid conflicts with core and physics types.
 */
typedef enum {
    /** OFC subdivision (lateral, medial, anterior, posterior) */
    OFC_KG_NODE_SUBDIVISION = 0x2000,

    /** Value signal type (expected, received, RPE, etc.) */
    OFC_KG_NODE_VALUE_SIGNAL,

    /** Decision type (binary, multi, sequential, etc.) */
    OFC_KG_NODE_DECISION_TYPE,

    /** Emotion state influencing value */
    OFC_KG_NODE_EMOTION_STATE,

    /** Reward representation */
    OFC_KG_NODE_REWARD,

    /** Learning signal (RPE-driven) */
    OFC_KG_NODE_LEARNING_SIGNAL,

    /** Risk assessment node */
    OFC_KG_NODE_RISK_ASSESSMENT,

    /** Social value computation */
    OFC_KG_NODE_SOCIAL_VALUE
} ofc_kg_node_type_t;

/**
 * @brief OFC-specific edge types
 */
typedef enum {
    /** Value signal drives decision */
    OFC_KG_EDGE_DRIVES = 0x2000,

    /** Emotion modulates value */
    OFC_KG_EDGE_MODULATES_VALUE,

    /** RPE triggers learning */
    OFC_KG_EDGE_TRIGGERS_LEARNING,

    /** Risk influences threshold */
    OFC_KG_EDGE_INFLUENCES_THRESHOLD,

    /** Reward updates expectation */
    OFC_KG_EDGE_UPDATES_EXPECTATION,

    /** Social context shapes value */
    OFC_KG_EDGE_SHAPES_VALUE,

    /** Temporal discounting applies to value */
    OFC_KG_EDGE_DISCOUNTS,

    /** Reversal signal resets associations */
    OFC_KG_EDGE_RESETS
} ofc_kg_edge_type_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief OFC KG wiring configuration
 */
typedef struct {
    /** Register lateral OFC nodes */
    bool register_lateral;

    /** Register medial OFC nodes */
    bool register_medial;

    /** Register anterior OFC nodes */
    bool register_anterior;

    /** Register posterior OFC nodes */
    bool register_posterior;

    /** Register value computation nodes */
    bool register_value_nodes;

    /** Register decision nodes */
    bool register_decision_nodes;

    /** Register inter-subsystem edges */
    bool register_cross_edges;

    /** Include state metadata */
    bool include_state_metadata;

    /** Register emotion integration edges */
    bool register_emotion_edges;
} ofc_kg_config_t;

/**
 * @brief OFC KG wiring state (node IDs for reference)
 */
typedef struct {
    /** OFC root node ID */
    brain_kg_node_id_t root_id;

    /* Subdivision node IDs */
    /** Lateral OFC node ID */
    brain_kg_node_id_t lateral_id;

    /** Medial OFC node ID */
    brain_kg_node_id_t medial_id;

    /** Anterior OFC node ID */
    brain_kg_node_id_t anterior_id;

    /** Posterior OFC node ID */
    brain_kg_node_id_t posterior_id;

    /* Value computation node IDs */
    /** Value computation subsystem node ID */
    brain_kg_node_id_t value_system_id;

    /** Expected value node ID */
    brain_kg_node_id_t expected_value_id;

    /** Received value node ID */
    brain_kg_node_id_t received_value_id;

    /** Prediction error node ID */
    brain_kg_node_id_t prediction_error_id;

    /** Probability node ID */
    brain_kg_node_id_t probability_id;

    /** Magnitude node ID */
    brain_kg_node_id_t magnitude_id;

    /** Delay (temporal discount) node ID */
    brain_kg_node_id_t delay_id;

    /** Risk node ID */
    brain_kg_node_id_t risk_id;

    /** Social value node ID */
    brain_kg_node_id_t social_id;

    /* Decision node IDs */
    /** Decision system node ID */
    brain_kg_node_id_t decision_system_id;

    /** Binary choice node ID */
    brain_kg_node_id_t binary_choice_id;

    /** Multi-alternative choice node ID */
    brain_kg_node_id_t multi_choice_id;

    /** Sequential decision node ID */
    brain_kg_node_id_t sequential_id;

    /** Social decision node ID */
    brain_kg_node_id_t social_decision_id;

    /** Moral judgment node ID */
    brain_kg_node_id_t moral_id;

    /* Counters */
    /** Number of nodes registered */
    uint32_t node_count;

    /** Number of edges registered */
    uint32_t edge_count;

    /** Registration successful */
    bool registered;
} ofc_kg_state_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default OFC KG wiring configuration
 *
 * WHAT: Initializes configuration with sensible defaults
 * WHY:  Provides consistent starting point for KG registration
 * HOW:  Sets all registration flags to true
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ofc_kg_default_config(ofc_kg_config_t* config);

//=============================================================================
// Registration API
//=============================================================================

/**
 * @brief Register all OFC nodes in KG
 *
 * WHAT: Creates nodes for OFC concepts in the brain's KG
 * WHY:  Enables semantic queries and reasoning about OFC
 * HOW:  Creates hierarchical node structure with typed edges
 *
 * @param kg Knowledge graph to register in
 * @param config Registration configuration (NULL for defaults)
 * @param state Output registration state (optional, may be NULL)
 * @param admin_token Admin token for write access
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ofc_kg_register_all(
    brain_kg_t* kg,
    const ofc_kg_config_t* config,
    ofc_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register OFC subdivision nodes
 *
 * WHAT: Creates nodes for lateral, medial, anterior, posterior OFC
 * WHY:  Represents functional specialization within OFC
 * HOW:  Creates subdivision nodes linked to OFC root
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (OFC root)
 * @param config Configuration options
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ofc_kg_register_subdivisions(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    const ofc_kg_config_t* config,
    ofc_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register value computation nodes
 *
 * WHAT: Creates nodes for value signals (expected, received, RPE, etc.)
 * WHY:  Represents OFC's core value computation functions
 * HOW:  Creates value nodes with causal relationships
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (OFC root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ofc_kg_register_value_nodes(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    ofc_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register decision-related nodes
 *
 * WHAT: Creates nodes for decision types (binary, multi, social, etc.)
 * WHY:  Represents OFC's decision-making functions
 * HOW:  Creates decision nodes linked to value system
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (OFC root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ofc_kg_register_decision_nodes(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    ofc_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register cross-subsystem edges
 *
 * WHAT: Creates edges between OFC subsystems
 * WHY:  Represents causal relationships across modules
 * HOW:  Value -> Decision, Emotion -> Value, RPE -> Learning
 *
 * @param kg Knowledge graph
 * @param state State with node IDs
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ofc_kg_register_cross_edges(
    brain_kg_t* kg,
    ofc_kg_state_t* state,
    uint64_t admin_token
);

//=============================================================================
// State Synchronization API
//=============================================================================

/**
 * @brief Update OFC node metadata with current state
 *
 * WHAT: Synchronizes KG node metadata with OFC state
 * WHY:  Enables queries about current OFC values
 * HOW:  Updates node metadata fields
 *
 * @param kg Knowledge graph
 * @param state KG wiring state
 * @param expected_value Current expected value
 * @param prediction_error Current prediction error
 * @param decision_confidence Current decision confidence
 * @param risk_level Current risk assessment
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ofc_kg_update_state(
    brain_kg_t* kg,
    const ofc_kg_state_t* state,
    float expected_value,
    float prediction_error,
    float decision_confidence,
    float risk_level,
    uint64_t admin_token
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get OFC root node ID
 *
 * @param kg Knowledge graph
 * @return Root node ID or BRAIN_KG_INVALID_NODE
 */
NIMCP_EXPORT brain_kg_node_id_t ofc_kg_get_root(brain_kg_t* kg);

/**
 * @brief Find OFC subsystem by name
 *
 * @param kg Knowledge graph
 * @param name Subsystem name ("lateral_ofc", "medial_ofc", etc.)
 * @return Node ID or BRAIN_KG_INVALID_NODE
 */
NIMCP_EXPORT brain_kg_node_id_t ofc_kg_find_subsystem(
    brain_kg_t* kg,
    const char* name
);

/**
 * @brief Get all value computation nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* ofc_kg_get_value_nodes(brain_kg_t* kg);

/**
 * @brief Get all decision-related nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* ofc_kg_get_decision_nodes(brain_kg_t* kg);

/**
 * @brief Get OFC subdivision nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* ofc_kg_get_subdivisions(brain_kg_t* kg);

/**
 * @brief Unregister all OFC nodes (cleanup)
 *
 * WHAT: Removes all OFC nodes from KG
 * WHY:  Clean shutdown and resource release
 * HOW:  Removes nodes in reverse creation order
 *
 * @param kg Knowledge graph
 * @param state State with node IDs
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ofc_kg_unregister_all(
    brain_kg_t* kg,
    ofc_kg_state_t* state,
    uint64_t admin_token
);

/* Forward decl for runtime event emit API (W2). */
struct brain_struct;

/**
 * @brief Emit a runtime OFC event into the brain's internal KG
 *
 * Supported kinds: "decision_outcome". Silent no-op if brain/KG unavailable.
 * Creates `orbitofrontal_cortex_event_<kind>_<ts_us>` node + edge to
 * `orbitofrontal_cortex`.
 */
NIMCP_EXPORT void ofc_kg_emit_event(
    struct brain_struct* brain,
    const char* kind,
    float intensity,
    uint64_t ts_us
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OFC_KG_WIRING_H */
