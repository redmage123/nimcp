//=============================================================================
// nimcp_pag_kg_wiring.h - PAG Knowledge Graph Registration
//=============================================================================
/**
 * @file nimcp_pag_kg_wiring.h
 * @brief Knowledge Graph registration for Periaqueductal Gray (PAG) module
 *
 * WHAT: Registers PAG concepts (columns, defense states, pain pathways,
 *       emotions, vocalizations, autonomic outputs) as nodes in the brain's
 *       internal Knowledge Graph.
 *
 * WHY:  KG integration enables:
 *       - Semantic queries about PAG state ("which column is active?")
 *       - Cross-module reasoning about survival behaviors
 *       - Introspection of defense/pain relationships
 *       - Graph-based analysis of emotional responses
 *
 * HOW:  Creates hierarchical node structure:
 *       - PAG root node
 *         +-- Column subsystem
 *         |   +-- Dorsolateral (fight/flight)
 *         |   +-- Lateral (vocalization)
 *         |   +-- Dorsomedial (defensive attention)
 *         |   +-- Ventrolateral (freeze/fawn)
 *         +-- Defense subsystem
 *         |   +-- Fight response
 *         |   +-- Flight response
 *         |   +-- Freeze response
 *         |   +-- Fawn response
 *         +-- Pain modulation subsystem
 *         |   +-- Opioid pathway
 *         |   +-- Non-opioid pathway
 *         |   +-- Cannabinoid pathway
 *         |   +-- Serotonergic pathway
 *         |   +-- Noradrenergic pathway
 *         +-- Emotion subsystem
 *         |   +-- Fear, Rage, Pain, Panic, Maternal, Reproductive
 *         +-- Vocalization subsystem
 *         |   +-- Alarm, Aggression, Submission, Distress, Pleasure, Startle
 *         +-- Autonomic subsystem
 *             +-- Cardiovascular output
 *             +-- Respiratory output
 *
 * EDGES: Represent causal/functional relationships:
 *       - Column -> Behavior (ACTIVATES)
 *       - Pain pathway -> Analgesia (PRODUCES)
 *       - Emotion -> Column (MODULATES)
 *       - Defense -> Autonomic (TRIGGERS)
 *       - Threat -> Column (ACTIVATES)
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_PAG_KG_WIRING_H
#define NIMCP_PAG_KG_WIRING_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"
#include "core/brain/nimcp_brain_kg.h"
#include "core/brain/nimcp_brain_kg_helpers.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define PAG_KG_MODULE_NAME       "pag_kg_wiring"

/** PAG root node name */
#define PAG_KG_ROOT_NAME         "periaqueductal_gray"

/** Column subsystem node name */
#define PAG_KG_COLUMNS_NAME      "pag_columns"

/** Defense subsystem node name */
#define PAG_KG_DEFENSE_NAME      "pag_defense"

/** Pain modulation subsystem node name */
#define PAG_KG_PAIN_NAME         "pag_pain_modulation"

/** Emotion subsystem node name */
#define PAG_KG_EMOTION_NAME      "pag_emotion"

/** Vocalization subsystem node name */
#define PAG_KG_VOCAL_NAME        "pag_vocalization"

/** Autonomic subsystem node name */
#define PAG_KG_AUTONOMIC_NAME    "pag_autonomic"

//=============================================================================
// Node Type Extensions (for PAG-specific concepts)
//=============================================================================

/**
 * @brief PAG-specific KG node types
 *
 * These extend the base brain_kg_node_type_t with PAG-specific types.
 * Values start at 0x3000 to avoid conflicts with core and physics types.
 */
typedef enum {
    /** PAG anatomical column (dlPAG, lPAG, dmPAG, vlPAG) */
    PAG_KG_NODE_COLUMN = 0x3000,

    /** Defensive behavior state (fight, flight, freeze, fawn) */
    PAG_KG_NODE_DEFENSE_STATE,

    /** Pain modulation pathway (opioid, cannabinoid, etc.) */
    PAG_KG_NODE_PAIN_PATHWAY,

    /** Emotional state (fear, rage, panic, etc.) */
    PAG_KG_NODE_EMOTION_STATE,

    /** Vocalization type (alarm, aggression, etc.) */
    PAG_KG_NODE_VOCALIZATION,

    /** Autonomic output (cardiovascular, respiratory) */
    PAG_KG_NODE_AUTONOMIC_OUTPUT,

    /** Threat signal */
    PAG_KG_NODE_THREAT,

    /** Analgesia state */
    PAG_KG_NODE_ANALGESIA,

    /** Coping strategy (active/passive) */
    PAG_KG_NODE_COPING_STRATEGY
} pag_kg_node_type_t;

/**
 * @brief PAG-specific edge types
 */
typedef enum {
    /** Column activates behavior (dlPAG -> fight) */
    PAG_KG_EDGE_ACTIVATES = 0x3000,

    /** Pathway produces effect (opioid -> analgesia) */
    PAG_KG_EDGE_PRODUCES,

    /** Emotion modulates column activity */
    PAG_KG_EDGE_MODULATES,

    /** Defense triggers autonomic response */
    PAG_KG_EDGE_TRIGGERS,

    /** Threat activates column */
    PAG_KG_EDGE_THREAT_ACTIVATES,

    /** Column inhibits other column */
    PAG_KG_EDGE_INHIBITS,

    /** Pain pathway descends to spinal cord */
    PAG_KG_EDGE_DESCENDS_TO,

    /** Emotion elicits vocalization */
    PAG_KG_EDGE_ELICITS,

    /** Column controls coping strategy */
    PAG_KG_EDGE_CONTROLS,

    /** Stress induces analgesia */
    PAG_KG_EDGE_INDUCES
} pag_kg_edge_type_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief PAG KG wiring configuration
 */
typedef struct {
    /** Register column nodes */
    bool register_columns;

    /** Register defense state nodes */
    bool register_defense;

    /** Register pain pathway nodes */
    bool register_pain;

    /** Register emotion nodes */
    bool register_emotions;

    /** Register vocalization nodes */
    bool register_vocalizations;

    /** Register autonomic output nodes */
    bool register_autonomic;

    /** Register inter-subsystem edges */
    bool register_cross_edges;

    /** Include state metadata */
    bool include_state_metadata;
} pag_kg_config_t;

/**
 * @brief PAG KG wiring state (node IDs for reference)
 */
typedef struct {
    /** PAG root node ID */
    brain_kg_node_id_t root_id;

    /** Column subsystem node ID */
    brain_kg_node_id_t columns_subsystem_id;

    /** Individual column node IDs */
    brain_kg_node_id_t dorsolateral_id;
    brain_kg_node_id_t lateral_id;
    brain_kg_node_id_t dorsomedial_id;
    brain_kg_node_id_t ventrolateral_id;

    /** Defense subsystem node ID */
    brain_kg_node_id_t defense_subsystem_id;

    /** Individual defense state node IDs */
    brain_kg_node_id_t fight_id;
    brain_kg_node_id_t flight_id;
    brain_kg_node_id_t freeze_id;
    brain_kg_node_id_t fawn_id;

    /** Pain modulation subsystem node ID */
    brain_kg_node_id_t pain_subsystem_id;

    /** Individual pain pathway node IDs */
    brain_kg_node_id_t opioid_pathway_id;
    brain_kg_node_id_t non_opioid_pathway_id;
    brain_kg_node_id_t cannabinoid_pathway_id;
    brain_kg_node_id_t serotonergic_pathway_id;
    brain_kg_node_id_t noradrenergic_pathway_id;

    /** Analgesia state node ID */
    brain_kg_node_id_t analgesia_id;

    /** Emotion subsystem node ID */
    brain_kg_node_id_t emotion_subsystem_id;

    /** Individual emotion node IDs */
    brain_kg_node_id_t fear_id;
    brain_kg_node_id_t rage_id;
    brain_kg_node_id_t pain_emotion_id;
    brain_kg_node_id_t panic_id;
    brain_kg_node_id_t maternal_id;
    brain_kg_node_id_t reproductive_id;

    /** Vocalization subsystem node ID */
    brain_kg_node_id_t vocal_subsystem_id;

    /** Individual vocalization node IDs */
    brain_kg_node_id_t alarm_vocal_id;
    brain_kg_node_id_t aggression_vocal_id;
    brain_kg_node_id_t submission_vocal_id;
    brain_kg_node_id_t distress_vocal_id;
    brain_kg_node_id_t pleasure_vocal_id;
    brain_kg_node_id_t startle_vocal_id;

    /** Autonomic subsystem node ID */
    brain_kg_node_id_t autonomic_subsystem_id;

    /** Individual autonomic output node IDs */
    brain_kg_node_id_t cardiovascular_id;
    brain_kg_node_id_t respiratory_id;

    /** Coping strategy node IDs */
    brain_kg_node_id_t active_coping_id;
    brain_kg_node_id_t passive_coping_id;

    /** Number of nodes registered */
    uint32_t node_count;

    /** Number of edges registered */
    uint32_t edge_count;

    /** Registration successful */
    bool registered;
} pag_kg_state_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default PAG KG wiring configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_kg_default_config(pag_kg_config_t* config);

//=============================================================================
// Registration API
//=============================================================================

/**
 * @brief Register all PAG nodes in KG
 *
 * WHAT: Creates nodes for PAG concepts in the brain's KG
 * WHY:  Enables semantic queries and reasoning about survival behaviors
 * HOW:  Creates hierarchical node structure with typed edges
 *
 * @param kg Knowledge graph to register in
 * @param config Registration configuration (NULL for defaults)
 * @param state Output registration state (optional, may be NULL)
 * @param admin_token Admin token for write access
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_kg_register_all(
    brain_kg_t* kg,
    const pag_kg_config_t* config,
    pag_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register PAG column nodes
 *
 * WHAT: Creates nodes for PAG columns (dlPAG, lPAG, dmPAG, vlPAG)
 * WHY:  Columns control different behavioral outputs
 * HOW:  Creates column nodes with functional descriptions
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (PAG root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_kg_register_columns(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    pag_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register defense state nodes
 *
 * WHAT: Creates nodes for defense states (fight, flight, freeze, fawn)
 * WHY:  Defense states are primary PAG outputs
 * HOW:  Creates defense nodes with behavioral descriptions
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (PAG root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_kg_register_defense(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    pag_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register pain modulation pathway nodes
 *
 * WHAT: Creates nodes for pain pathways (opioid, cannabinoid, etc.)
 * WHY:  PAG is major component of descending pain inhibition
 * HOW:  Creates pathway nodes with mechanism descriptions
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (PAG root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_kg_register_pain(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    pag_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register emotion nodes
 *
 * WHAT: Creates nodes for PAG-mediated emotions
 * WHY:  PAG generates affective expressions
 * HOW:  Creates emotion nodes with column associations
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (PAG root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_kg_register_emotions(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    pag_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register vocalization nodes
 *
 * WHAT: Creates nodes for vocalization types
 * WHY:  PAG controls emotional vocalizations
 * HOW:  Creates vocalization nodes with trigger descriptions
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (PAG root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_kg_register_vocalizations(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    pag_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register autonomic output nodes
 *
 * WHAT: Creates nodes for autonomic outputs
 * WHY:  PAG controls cardiovascular and respiratory responses
 * HOW:  Creates autonomic nodes with output descriptions
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (PAG root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_kg_register_autonomic(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    pag_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register cross-subsystem edges
 *
 * WHAT: Creates edges between PAG subsystems
 * WHY:  Represents causal relationships across modules
 * HOW:  Column -> behavior, pain -> analgesia, emotion -> column, etc.
 *
 * @param kg Knowledge graph
 * @param state State with node IDs
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_kg_register_cross_edges(
    brain_kg_t* kg,
    pag_kg_state_t* state,
    uint64_t admin_token
);

//=============================================================================
// State Synchronization API
//=============================================================================

/**
 * @brief Update PAG node metadata with current state
 *
 * WHAT: Synchronizes KG node metadata with PAG state
 * WHY:  Enables queries about current PAG values
 * HOW:  Updates node metadata fields
 *
 * @param kg Knowledge graph
 * @param state KG wiring state
 * @param column_activities Array of column activity levels [0-1]
 * @param defense_active Active defense type index (-1 if none)
 * @param analgesia_level Current analgesia level [0-1]
 * @param dominant_emotion Dominant emotion type index
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_kg_update_state(
    brain_kg_t* kg,
    const pag_kg_state_t* state,
    const float* column_activities,
    int defense_active,
    float analgesia_level,
    int dominant_emotion,
    uint64_t admin_token
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get PAG root node ID
 *
 * @param kg Knowledge graph
 * @return Root node ID or BRAIN_KG_INVALID_NODE
 */
NIMCP_EXPORT brain_kg_node_id_t pag_kg_get_root(brain_kg_t* kg);

/**
 * @brief Find PAG subsystem by name
 *
 * @param kg Knowledge graph
 * @param name Subsystem name ("pag_columns", "pag_defense", etc.)
 * @return Node ID or BRAIN_KG_INVALID_NODE
 */
NIMCP_EXPORT brain_kg_node_id_t pag_kg_find_subsystem(
    brain_kg_t* kg,
    const char* name
);

/**
 * @brief Get all PAG column nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* pag_kg_get_columns(brain_kg_t* kg);

/**
 * @brief Get all pain pathway nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* pag_kg_get_pain_pathways(brain_kg_t* kg);

/**
 * @brief Get all defense state nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* pag_kg_get_defense_states(brain_kg_t* kg);

/**
 * @brief Get all emotion nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* pag_kg_get_emotions(brain_kg_t* kg);

/**
 * @brief Get all vocalization nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* pag_kg_get_vocalizations(brain_kg_t* kg);

/**
 * @brief Get column that activates a specific defense
 *
 * @param kg Knowledge graph
 * @param defense_id Defense state node ID
 * @return Column node ID or BRAIN_KG_INVALID_NODE
 */
NIMCP_EXPORT brain_kg_node_id_t pag_kg_get_column_for_defense(
    brain_kg_t* kg,
    brain_kg_node_id_t defense_id
);

/**
 * @brief Unregister all PAG nodes (cleanup)
 *
 * @param kg Knowledge graph
 * @param state State with node IDs
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_kg_unregister_all(
    brain_kg_t* kg,
    pag_kg_state_t* state,
    uint64_t admin_token
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PAG_KG_WIRING_H */
