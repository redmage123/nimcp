//=============================================================================
// nimcp_pfc_kg_wiring.h - Prefrontal Cortex Knowledge Graph Registration
//=============================================================================
/**
 * @file nimcp_pfc_kg_wiring.h
 * @brief Knowledge Graph registration for Prefrontal Cortex (PFC) module
 *
 * WHAT: Registers PFC concepts (executive control, working memory, planning,
 *       decision-making) as nodes in the brain's internal Knowledge Graph.
 *
 * WHY:  KG integration enables:
 *       - Semantic queries about executive state ("what is cognitive load?")
 *       - Cross-module reasoning about planning and control
 *       - Introspection of working memory and attention systems
 *       - Graph-based analysis of executive dynamics
 *
 * HOW:  Creates hierarchical node structure:
 *       - PFC root node
 *         ├── Dorsolateral PFC (dlPFC)
 *         │   ├── Working memory maintenance
 *         │   └── Cognitive control
 *         ├── Ventromedial PFC (vmPFC)
 *         │   ├── Value-based decisions
 *         │   └── Emotion regulation
 *         ├── Anterior Cingulate (ACC)
 *         │   ├── Conflict monitoring
 *         │   └── Error detection
 *         ├── Lateral PFC
 *         │   └── Rule representation
 *         ├── Executive Function nodes
 *         │   ├── Working memory
 *         │   ├── Attention control
 *         │   ├── Inhibition
 *         │   ├── Cognitive flexibility
 *         │   └── Planning
 *         └── Decision nodes
 *             ├── Goal selection
 *             ├── Action selection
 *             └── Strategy selection
 *
 * EDGES: Represent causal/functional relationships:
 *       - Working memory -> Decision (INFORMS)
 *       - ACC -> Cognitive control (TRIGGERS)
 *       - dlPFC -> Goal maintenance (MAINTAINS)
 *       - vmPFC -> Value computation (INTEGRATES_WITH)
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_PFC_KG_WIRING_H
#define NIMCP_PFC_KG_WIRING_H

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
#define PFC_KG_MODULE_NAME            "pfc_kg_wiring"

/** PFC root node name */
#define PFC_KG_ROOT_NAME              "prefrontal_cortex"

/** Dorsolateral PFC node name */
#define PFC_KG_DLPFC_NAME             "dorsolateral_pfc"

/** Ventromedial PFC node name */
#define PFC_KG_VMPFC_NAME             "ventromedial_pfc"

/** Anterior Cingulate node name */
#define PFC_KG_ACC_NAME               "anterior_cingulate"

/** Lateral PFC node name */
#define PFC_KG_LPFC_NAME              "lateral_pfc"

/** Executive system node name */
#define PFC_KG_EXECUTIVE_NAME         "executive_system"

/** Decision system node name */
#define PFC_KG_DECISION_NAME          "decision_system"

//=============================================================================
// Node Type Extensions (for PFC-specific concepts)
//=============================================================================

/**
 * @brief PFC-specific KG node types
 *
 * These extend the base brain_kg_node_type_t with PFC-specific types.
 * Values start at 0x2300 to avoid conflicts with core and other region types.
 */
typedef enum {
    /** PFC subregion (dlPFC, vmPFC, ACC, etc.) */
    PFC_KG_NODE_SUBREGION = 0x2300,

    /** Executive function (WM, attention, inhibition, etc.) */
    PFC_KG_NODE_EXECUTIVE_FUNCTION,

    /** Cognitive control process */
    PFC_KG_NODE_CONTROL_PROCESS,

    /** Decision type (goal, action, strategy) */
    PFC_KG_NODE_DECISION_TYPE,

    /** Monitoring process (conflict, error) */
    PFC_KG_NODE_MONITORING,

    /** Working memory component */
    PFC_KG_NODE_WM_COMPONENT,

    /** Attention component */
    PFC_KG_NODE_ATTENTION_COMPONENT
} pfc_kg_node_type_t;

/**
 * @brief PFC-specific edge types
 */
typedef enum {
    /** Working memory informs decision */
    PFC_KG_EDGE_INFORMS = 0x2300,

    /** Control adjusts processing */
    PFC_KG_EDGE_ADJUSTS,

    /** Conflict triggers control */
    PFC_KG_EDGE_TRIGGERS_CONTROL,

    /** Goal maintains focus */
    PFC_KG_EDGE_MAINTAINS,

    /** Rule guides action */
    PFC_KG_EDGE_GUIDES,

    /** Inhibition suppresses */
    PFC_KG_EDGE_SUPPRESSES,

    /** Flexibility switches */
    PFC_KG_EDGE_SWITCHES,

    /** Planning sequences */
    PFC_KG_EDGE_SEQUENCES
} pfc_kg_edge_type_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief PFC KG wiring configuration
 */
typedef struct {
    /** Register dlPFC nodes */
    bool register_dlpfc;

    /** Register vmPFC nodes */
    bool register_vmpfc;

    /** Register ACC nodes */
    bool register_acc;

    /** Register lateral PFC nodes */
    bool register_lpfc;

    /** Register executive function nodes */
    bool register_executive_nodes;

    /** Register decision nodes */
    bool register_decision_nodes;

    /** Register inter-subsystem edges */
    bool register_cross_edges;

    /** Include state metadata */
    bool include_state_metadata;

    /** Register control-related edges */
    bool register_control_edges;
} pfc_kg_config_t;

/**
 * @brief PFC KG wiring state (node IDs for reference)
 */
typedef struct {
    /** PFC root node ID */
    brain_kg_node_id_t root_id;

    /* Subregion node IDs */
    /** Dorsolateral PFC node ID */
    brain_kg_node_id_t dlpfc_id;

    /** Ventromedial PFC node ID */
    brain_kg_node_id_t vmpfc_id;

    /** Anterior cingulate node ID */
    brain_kg_node_id_t acc_id;

    /** Lateral PFC node ID */
    brain_kg_node_id_t lpfc_id;

    /* Executive system node IDs */
    /** Executive system node ID */
    brain_kg_node_id_t executive_system_id;

    /** Working memory node ID */
    brain_kg_node_id_t working_memory_id;

    /** WM maintenance node ID */
    brain_kg_node_id_t wm_maintenance_id;

    /** WM manipulation node ID */
    brain_kg_node_id_t wm_manipulation_id;

    /** Attention control node ID */
    brain_kg_node_id_t attention_id;

    /** Selective attention node ID */
    brain_kg_node_id_t selective_attn_id;

    /** Sustained attention node ID */
    brain_kg_node_id_t sustained_attn_id;

    /** Inhibition node ID */
    brain_kg_node_id_t inhibition_id;

    /** Cognitive flexibility node ID */
    brain_kg_node_id_t flexibility_id;

    /** Planning node ID */
    brain_kg_node_id_t planning_id;

    /* Monitoring node IDs */
    /** Conflict monitoring node ID */
    brain_kg_node_id_t conflict_id;

    /** Error detection node ID */
    brain_kg_node_id_t error_id;

    /** Performance monitoring node ID */
    brain_kg_node_id_t performance_id;

    /* Decision system node IDs */
    /** Decision system node ID */
    brain_kg_node_id_t decision_system_id;

    /** Goal selection node ID */
    brain_kg_node_id_t goal_selection_id;

    /** Action selection node ID */
    brain_kg_node_id_t action_selection_id;

    /** Strategy selection node ID */
    brain_kg_node_id_t strategy_selection_id;

    /** Rule representation node ID */
    brain_kg_node_id_t rule_id;

    /* Counters */
    /** Number of nodes registered */
    uint32_t node_count;

    /** Number of edges registered */
    uint32_t edge_count;

    /** Registration successful */
    bool registered;
} pfc_kg_state_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default PFC KG wiring configuration
 *
 * WHAT: Initializes configuration with sensible defaults
 * WHY:  Provides consistent starting point for KG registration
 * HOW:  Sets all registration flags to true
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pfc_kg_default_config(pfc_kg_config_t* config);

//=============================================================================
// Registration API
//=============================================================================

/**
 * @brief Register all PFC nodes in KG
 *
 * WHAT: Creates nodes for PFC concepts in the brain's KG
 * WHY:  Enables semantic queries and reasoning about executive function
 * HOW:  Creates hierarchical node structure with typed edges
 *
 * @param kg Knowledge graph to register in
 * @param config Registration configuration (NULL for defaults)
 * @param state Output registration state (optional, may be NULL)
 * @param admin_token Admin token for write access
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pfc_kg_register_all(
    brain_kg_t* kg,
    const pfc_kg_config_t* config,
    pfc_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register PFC subregion nodes
 *
 * WHAT: Creates nodes for dlPFC, vmPFC, ACC, lPFC
 * WHY:  Represents anatomical structure of prefrontal cortex
 * HOW:  Creates subregion nodes linked to PFC root
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (PFC root)
 * @param config Configuration options
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pfc_kg_register_subregions(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    const pfc_kg_config_t* config,
    pfc_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register executive function nodes
 *
 * WHAT: Creates nodes for WM, attention, inhibition, flexibility
 * WHY:  Represents PFC's core executive functions
 * HOW:  Creates executive nodes with causal relationships
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (PFC root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pfc_kg_register_executive_nodes(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    pfc_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register decision-related nodes
 *
 * WHAT: Creates nodes for goal/action/strategy selection
 * WHY:  Represents PFC's decision-making functions
 * HOW:  Creates decision nodes linked to executive system
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (PFC root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pfc_kg_register_decision_nodes(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    pfc_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register monitoring nodes
 *
 * WHAT: Creates nodes for conflict, error, performance monitoring
 * WHY:  Represents PFC's monitoring functions (primarily ACC)
 * HOW:  Creates monitoring nodes linked to executive system
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (PFC root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pfc_kg_register_monitoring_nodes(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    pfc_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register cross-subsystem edges
 *
 * WHAT: Creates edges between PFC subsystems
 * WHY:  Represents causal relationships across modules
 * HOW:  WM -> Decision, ACC -> Control, etc.
 *
 * @param kg Knowledge graph
 * @param state State with node IDs
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pfc_kg_register_cross_edges(
    brain_kg_t* kg,
    pfc_kg_state_t* state,
    uint64_t admin_token
);

//=============================================================================
// State Synchronization API
//=============================================================================

/**
 * @brief Update PFC node metadata with current state
 *
 * WHAT: Synchronizes KG node metadata with PFC state
 * WHY:  Enables queries about current executive state
 * HOW:  Updates node metadata fields
 *
 * @param kg Knowledge graph
 * @param state KG wiring state
 * @param wm_load Current working memory load (0-1)
 * @param control_demand Current cognitive control demand (0-1)
 * @param conflict_level Current conflict level (0-1)
 * @param attention_focus Current attention focus strength (0-1)
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pfc_kg_update_state(
    brain_kg_t* kg,
    const pfc_kg_state_t* state,
    float wm_load,
    float control_demand,
    float conflict_level,
    float attention_focus,
    uint64_t admin_token
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get PFC root node ID
 *
 * @param kg Knowledge graph
 * @return Root node ID or BRAIN_KG_INVALID_NODE
 */
NIMCP_EXPORT brain_kg_node_id_t pfc_kg_get_root(brain_kg_t* kg);

/**
 * @brief Find PFC subsystem by name
 *
 * @param kg Knowledge graph
 * @param name Subsystem name ("dorsolateral_pfc", "anterior_cingulate", etc.)
 * @return Node ID or BRAIN_KG_INVALID_NODE
 */
NIMCP_EXPORT brain_kg_node_id_t pfc_kg_find_subsystem(
    brain_kg_t* kg,
    const char* name
);

/**
 * @brief Get all executive function nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* pfc_kg_get_executive_nodes(brain_kg_t* kg);

/**
 * @brief Get all decision-related nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* pfc_kg_get_decision_nodes(brain_kg_t* kg);

/**
 * @brief Get PFC subregion nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* pfc_kg_get_subregions(brain_kg_t* kg);

/**
 * @brief Unregister all PFC nodes (cleanup)
 *
 * WHAT: Removes all PFC nodes from KG
 * WHY:  Clean shutdown and resource release
 * HOW:  Removes nodes in reverse creation order
 *
 * @param kg Knowledge graph
 * @param state State with node IDs
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pfc_kg_unregister_all(
    brain_kg_t* kg,
    pfc_kg_state_t* state,
    uint64_t admin_token
);

/* Forward decl for runtime event emit API (W2). */
struct brain_struct;

/**
 * @brief Emit a runtime prefrontal event into the brain's internal KG
 *
 * Supported kinds: "goal_update". Silent no-op if brain/KG unavailable.
 * Creates `prefrontal_cortex_event_<kind>_<ts_us>` node + edge to
 * `prefrontal_cortex`.
 */
NIMCP_EXPORT void pfc_kg_emit_event(
    struct brain_struct* brain,
    const char* kind,
    float intensity,
    uint64_t ts_us
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PFC_KG_WIRING_H */
