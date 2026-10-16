//=============================================================================
// nimcp_amygdala_kg_wiring.h - Amygdala Knowledge Graph Registration
//=============================================================================
/**
 * @file nimcp_amygdala_kg_wiring.h
 * @brief Knowledge Graph registration for Amygdala module
 *
 * WHAT: Registers Amygdala concepts (fear processing, emotional learning,
 *       threat detection) as nodes in the brain's internal Knowledge Graph.
 *
 * WHY:  KG integration enables:
 *       - Semantic queries about emotional state ("what is current threat level?")
 *       - Cross-module reasoning about fear and emotional responses
 *       - Introspection of conditioning and extinction processes
 *       - Graph-based analysis of emotional dynamics
 *
 * HOW:  Creates hierarchical node structure:
 *       - Amygdala root node
 *         ├── Basolateral Complex (BLA)
 *         │   ├── Lateral nucleus (sensory input)
 *         │   └── Basal nucleus (context/hippocampal input)
 *         ├── Central Nucleus (CeA) - output
 *         │   ├── Medial division (fear responses)
 *         │   └── Lateral division (attention/arousal)
 *         ├── Intercalated Cells (extinction)
 *         ├── Emotional Processing nodes
 *         │   ├── Fear detection
 *         │   ├── Threat assessment
 *         │   ├── Emotional valence
 *         │   ├── Arousal modulation
 *         │   └── Social emotion
 *         └── Learning nodes
 *             ├── Fear conditioning
 *             ├── Fear extinction
 *             └── Emotional memory
 *
 * EDGES: Represent causal/functional relationships:
 *       - Threat -> Fear response (TRIGGERS)
 *       - BLA -> CeA (SENDS_TO)
 *       - Extinction -> Fear (INHIBITS)
 *       - Arousal -> Attention (MODULATES)
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_AMYGDALA_KG_WIRING_H
#define NIMCP_AMYGDALA_KG_WIRING_H

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
#define AMYGDALA_KG_MODULE_NAME       "amygdala_kg_wiring"

/** Amygdala root node name */
#define AMYGDALA_KG_ROOT_NAME         "amygdala"

/** Basolateral complex node name */
#define AMYGDALA_KG_BLA_NAME          "basolateral_complex"

/** Lateral nucleus node name */
#define AMYGDALA_KG_LA_NAME           "lateral_nucleus"

/** Basal nucleus node name */
#define AMYGDALA_KG_BA_NAME           "basal_nucleus"

/** Central nucleus node name */
#define AMYGDALA_KG_CEA_NAME          "central_nucleus"

/** Intercalated cells node name */
#define AMYGDALA_KG_ITC_NAME          "intercalated_cells"

/** Emotional processing system node name */
#define AMYGDALA_KG_EMOTION_NAME      "emotional_processing"

/** Learning system node name */
#define AMYGDALA_KG_LEARNING_NAME     "emotional_learning"

//=============================================================================
// Node Type Extensions (for Amygdala-specific concepts)
//=============================================================================

/**
 * @brief Amygdala-specific KG node types
 *
 * These extend the base brain_kg_node_type_t with Amygdala-specific types.
 * Values start at 0x2200 to avoid conflicts with core and other region types.
 */
typedef enum {
    /** Amygdala nucleus (BLA, CeA, LA, BA, etc.) */
    AMYGDALA_KG_NODE_NUCLEUS = 0x2200,

    /** Emotion type (fear, anxiety, reward, social) */
    AMYGDALA_KG_NODE_EMOTION_TYPE,

    /** Emotional process (detection, assessment, response) */
    AMYGDALA_KG_NODE_EMOTION_PROCESS,

    /** Learning process (conditioning, extinction) */
    AMYGDALA_KG_NODE_LEARNING_PROCESS,

    /** Output pathway (autonomic, behavioral, hormonal) */
    AMYGDALA_KG_NODE_OUTPUT_PATHWAY,

    /** Threat level (low, medium, high, critical) */
    AMYGDALA_KG_NODE_THREAT_LEVEL,

    /** Arousal state */
    AMYGDALA_KG_NODE_AROUSAL_STATE
} amygdala_kg_node_type_t;

/**
 * @brief Amygdala-specific edge types
 */
typedef enum {
    /** Threat detection triggers fear */
    AMYGDALA_KG_EDGE_TRIGGERS_FEAR = 0x2200,

    /** Extinction inhibits fear */
    AMYGDALA_KG_EDGE_INHIBITS_FEAR,

    /** Arousal modulates processing */
    AMYGDALA_KG_EDGE_MODULATES_AROUSAL,

    /** Conditioning strengthens association */
    AMYGDALA_KG_EDGE_STRENGTHENS,

    /** Context gates response */
    AMYGDALA_KG_EDGE_GATES,

    /** Autonomic output generates */
    AMYGDALA_KG_EDGE_GENERATES_RESPONSE,

    /** Social emotion influences */
    AMYGDALA_KG_EDGE_INFLUENCES_SOCIAL
} amygdala_kg_edge_type_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Amygdala KG wiring configuration
 */
typedef struct {
    /** Register BLA nodes */
    bool register_bla;

    /** Register CeA nodes */
    bool register_cea;

    /** Register intercalated cells */
    bool register_itc;

    /** Register emotional processing nodes */
    bool register_emotion_nodes;

    /** Register learning nodes */
    bool register_learning_nodes;

    /** Register output pathway nodes */
    bool register_output_nodes;

    /** Register inter-subsystem edges */
    bool register_cross_edges;

    /** Include state metadata */
    bool include_state_metadata;

    /** Register extinction-related edges */
    bool register_extinction_edges;
} amygdala_kg_config_t;

/**
 * @brief Amygdala KG wiring state (node IDs for reference)
 */
typedef struct {
    /** Amygdala root node ID */
    brain_kg_node_id_t root_id;

    /* Nucleus node IDs */
    /** Basolateral complex node ID */
    brain_kg_node_id_t bla_id;

    /** Lateral nucleus node ID */
    brain_kg_node_id_t la_id;

    /** Basal nucleus node ID */
    brain_kg_node_id_t ba_id;

    /** Central nucleus node ID */
    brain_kg_node_id_t cea_id;

    /** Central nucleus medial division node ID */
    brain_kg_node_id_t cea_m_id;

    /** Central nucleus lateral division node ID */
    brain_kg_node_id_t cea_l_id;

    /** Intercalated cells node ID */
    brain_kg_node_id_t itc_id;

    /* Emotional processing node IDs */
    /** Emotional processing system node ID */
    brain_kg_node_id_t emotion_system_id;

    /** Fear detection node ID */
    brain_kg_node_id_t fear_detect_id;

    /** Threat assessment node ID */
    brain_kg_node_id_t threat_id;

    /** Emotional valence node ID */
    brain_kg_node_id_t valence_id;

    /** Arousal modulation node ID */
    brain_kg_node_id_t arousal_id;

    /** Social emotion node ID */
    brain_kg_node_id_t social_id;

    /* Learning node IDs */
    /** Learning system node ID */
    brain_kg_node_id_t learning_system_id;

    /** Fear conditioning node ID */
    brain_kg_node_id_t conditioning_id;

    /** Fear extinction node ID */
    brain_kg_node_id_t extinction_id;

    /** Emotional memory node ID */
    brain_kg_node_id_t emotional_memory_id;

    /* Output pathway node IDs */
    /** Autonomic output node ID */
    brain_kg_node_id_t autonomic_id;

    /** Behavioral output node ID */
    brain_kg_node_id_t behavioral_id;

    /** Hormonal output node ID */
    brain_kg_node_id_t hormonal_id;

    /* Counters */
    /** Number of nodes registered */
    uint32_t node_count;

    /** Number of edges registered */
    uint32_t edge_count;

    /** Registration successful */
    bool registered;
} amygdala_kg_state_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default Amygdala KG wiring configuration
 *
 * WHAT: Initializes configuration with sensible defaults
 * WHY:  Provides consistent starting point for KG registration
 * HOW:  Sets all registration flags to true
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int amygdala_kg_default_config(amygdala_kg_config_t* config);

//=============================================================================
// Registration API
//=============================================================================

/**
 * @brief Register all Amygdala nodes in KG
 *
 * WHAT: Creates nodes for Amygdala concepts in the brain's KG
 * WHY:  Enables semantic queries and reasoning about emotion
 * HOW:  Creates hierarchical node structure with typed edges
 *
 * @param kg Knowledge graph to register in
 * @param config Registration configuration (NULL for defaults)
 * @param state Output registration state (optional, may be NULL)
 * @param admin_token Admin token for write access
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int amygdala_kg_register_all(
    brain_kg_t* kg,
    const amygdala_kg_config_t* config,
    amygdala_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register Amygdala nuclei nodes
 *
 * WHAT: Creates nodes for BLA, CeA, LA, BA, ITC
 * WHY:  Represents anatomical structure of amygdala
 * HOW:  Creates nucleus nodes linked to amygdala root
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (amygdala root)
 * @param config Configuration options
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int amygdala_kg_register_nuclei(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    const amygdala_kg_config_t* config,
    amygdala_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register emotional processing nodes
 *
 * WHAT: Creates nodes for emotion types and processes
 * WHY:  Represents amygdala's core emotional functions
 * HOW:  Creates emotion nodes with causal relationships
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (amygdala root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int amygdala_kg_register_emotion_nodes(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    amygdala_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register learning-related nodes
 *
 * WHAT: Creates nodes for conditioning and extinction
 * WHY:  Represents amygdala's fear learning functions
 * HOW:  Creates learning nodes linked to emotional system
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (amygdala root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int amygdala_kg_register_learning_nodes(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    amygdala_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register output pathway nodes
 *
 * WHAT: Creates nodes for autonomic, behavioral, hormonal outputs
 * WHY:  Represents amygdala's effector outputs
 * HOW:  Creates output nodes linked to CeA
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (amygdala root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int amygdala_kg_register_output_nodes(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    amygdala_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register cross-subsystem edges
 *
 * WHAT: Creates edges between amygdala subsystems
 * WHY:  Represents causal relationships across modules
 * HOW:  Threat -> Fear, BLA -> CeA, Extinction -> Inhibition
 *
 * @param kg Knowledge graph
 * @param state State with node IDs
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int amygdala_kg_register_cross_edges(
    brain_kg_t* kg,
    amygdala_kg_state_t* state,
    uint64_t admin_token
);

//=============================================================================
// State Synchronization API
//=============================================================================

/**
 * @brief Update amygdala node metadata with current state
 *
 * WHAT: Synchronizes KG node metadata with amygdala state
 * WHY:  Enables queries about current emotional state
 * HOW:  Updates node metadata fields
 *
 * @param kg Knowledge graph
 * @param state KG wiring state
 * @param threat_level Current threat level (0-1)
 * @param fear_strength Current fear response strength (0-1)
 * @param arousal_level Current arousal level (0-1)
 * @param extinction_progress Current extinction progress (0-1)
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int amygdala_kg_update_state(
    brain_kg_t* kg,
    const amygdala_kg_state_t* state,
    float threat_level,
    float fear_strength,
    float arousal_level,
    float extinction_progress,
    uint64_t admin_token
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get amygdala root node ID
 *
 * @param kg Knowledge graph
 * @return Root node ID or BRAIN_KG_INVALID_NODE
 */
NIMCP_EXPORT brain_kg_node_id_t amygdala_kg_get_root(brain_kg_t* kg);

/**
 * @brief Find amygdala subsystem by name
 *
 * @param kg Knowledge graph
 * @param name Subsystem name ("basolateral_complex", "central_nucleus", etc.)
 * @return Node ID or BRAIN_KG_INVALID_NODE
 */
NIMCP_EXPORT brain_kg_node_id_t amygdala_kg_find_subsystem(
    brain_kg_t* kg,
    const char* name
);

/**
 * @brief Get all emotion-related nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* amygdala_kg_get_emotion_nodes(brain_kg_t* kg);

/**
 * @brief Get all learning-related nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* amygdala_kg_get_learning_nodes(brain_kg_t* kg);

/**
 * @brief Get amygdala nucleus nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* amygdala_kg_get_nuclei(brain_kg_t* kg);

/**
 * @brief Unregister all amygdala nodes (cleanup)
 *
 * WHAT: Removes all amygdala nodes from KG
 * WHY:  Clean shutdown and resource release
 * HOW:  Removes nodes in reverse creation order
 *
 * @param kg Knowledge graph
 * @param state State with node IDs
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int amygdala_kg_unregister_all(
    brain_kg_t* kg,
    amygdala_kg_state_t* state,
    uint64_t admin_token
);

/* Forward decl for runtime event emit API (W2). */
struct brain_struct;

/**
 * @brief Emit a runtime amygdala event into the brain's internal KG
 *
 * Supported kinds: "fear", "reward". Silent no-op if brain/KG unavailable.
 * Creates `amygdala_event_<kind>_<ts_us>` node + edge to `amygdala`.
 * Self-elevates admin token to write, restores READ on exit.
 */
NIMCP_EXPORT void amygdala_kg_emit_event(
    struct brain_struct* brain,
    const char* kind,
    float intensity,
    uint64_t ts_us
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AMYGDALA_KG_WIRING_H */
