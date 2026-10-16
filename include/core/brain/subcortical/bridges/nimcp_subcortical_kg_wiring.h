//=============================================================================
// nimcp_subcortical_kg_wiring.h - Subcortical Knowledge Graph Registration
//=============================================================================
/**
 * @file nimcp_subcortical_kg_wiring.h
 * @brief Knowledge Graph registration for Subcortical structures
 *
 * WHAT: Registers subcortical structures (basal ganglia, thalamus, nucleus accumbens)
 *       as nodes in the brain's internal Knowledge Graph.
 *
 * WHY:  KG integration enables:
 *       - Semantic queries about action selection and motor control
 *       - Cross-module reasoning about reward processing
 *       - Introspection of direct/indirect/hyperdirect pathways
 *       - Graph-based analysis of subcortical dynamics
 *
 * HOW:  Creates hierarchical node structure:
 *       - Subcortical root node
 *         |-- Basal Ganglia
 *         |   |-- Striatum (D1/D2 pathways)
 *         |   |-- Globus Pallidus (GPe, GPi)
 *         |   |-- Subthalamic Nucleus
 *         |   |-- Substantia Nigra (SNc, SNr)
 *         |   |-- Pathways
 *         |       |-- Direct pathway
 *         |       |-- Indirect pathway
 *         |       |-- Hyperdirect pathway
 *         |-- Thalamus
 *         |   |-- LGN (visual relay)
 *         |   |-- MGN (auditory relay)
 *         |   |-- VPL/VPM (somatosensory)
 *         |   |-- VA/VL (motor)
 *         |   |-- Pulvinar (attention)
 *         |   |-- MD (executive)
 *         |   |-- TRN (reticular nucleus - gating)
 *         |-- Nucleus Accumbens
 *             |-- Core (goal-directed)
 *             |-- Shell (hedonic/motivation)
 *
 * EDGES: Represent functional connections:
 *       - Striatum -> GPe (indirect)
 *       - Striatum -> GPi/SNr (direct)
 *       - Cortex -> STN (hyperdirect)
 *       - GPi/SNr -> Thalamus (output)
 *       - SNc -> Striatum (dopamine)
 *
 * @author NIMCP Development Team
 * @date 2026-02-03
 * @version 1.0.0
 */

#ifndef NIMCP_SUBCORTICAL_KG_WIRING_H
#define NIMCP_SUBCORTICAL_KG_WIRING_H

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
#define SUBCORTICAL_KG_MODULE_NAME    "subcortical_kg_wiring"

/** Subcortical root node name */
#define SUBCORTICAL_KG_ROOT_NAME      "subcortical"

/* Basal Ganglia node names */
#define SUBCORTICAL_KG_BG_NAME              "basal_ganglia"
#define SUBCORTICAL_KG_STRIATUM_NAME        "striatum"
#define SUBCORTICAL_KG_D1_PATHWAY_NAME      "d1_direct_pathway"
#define SUBCORTICAL_KG_D2_PATHWAY_NAME      "d2_indirect_pathway"
#define SUBCORTICAL_KG_GPE_NAME             "globus_pallidus_externa"
#define SUBCORTICAL_KG_GPI_NAME             "globus_pallidus_interna"
#define SUBCORTICAL_KG_STN_NAME             "subthalamic_nucleus"
#define SUBCORTICAL_KG_SNC_NAME             "substantia_nigra_compacta"
#define SUBCORTICAL_KG_SNR_NAME             "substantia_nigra_reticulata"
#define SUBCORTICAL_KG_VTA_NAME             "ventral_tegmental_area"

/* Striatal subdivision node names */
#define SUBCORTICAL_KG_DORSAL_STRIATUM_NAME "dorsal_striatum"
#define SUBCORTICAL_KG_VENTRAL_STRIATUM_NAME "ventral_striatum"
#define SUBCORTICAL_KG_CAUDATE_NAME         "caudate_nucleus"
#define SUBCORTICAL_KG_PUTAMEN_NAME         "putamen"

/* Pathway node names */
#define SUBCORTICAL_KG_DIRECT_PATHWAY_NAME    "direct_pathway"
#define SUBCORTICAL_KG_INDIRECT_PATHWAY_NAME  "indirect_pathway"
#define SUBCORTICAL_KG_HYPERDIRECT_NAME       "hyperdirect_pathway"
#define SUBCORTICAL_KG_MESOLIMBIC_NAME        "mesolimbic_pathway"
#define SUBCORTICAL_KG_NIGROSTRIATAL_NAME     "nigrostriatal_pathway"

/* Thalamus node names */
#define SUBCORTICAL_KG_THALAMUS_NAME    "thalamus"
#define SUBCORTICAL_KG_LGN_NAME         "lateral_geniculate_nucleus"
#define SUBCORTICAL_KG_MGN_NAME         "medial_geniculate_nucleus"
#define SUBCORTICAL_KG_VPL_NAME         "ventral_posterolateral"
#define SUBCORTICAL_KG_VPM_NAME         "ventral_posteromedial"
#define SUBCORTICAL_KG_VA_NAME          "ventral_anterior"
#define SUBCORTICAL_KG_VL_NAME          "ventral_lateral"
#define SUBCORTICAL_KG_PULVINAR_NAME    "pulvinar"
#define SUBCORTICAL_KG_MD_NAME          "mediodorsal"
#define SUBCORTICAL_KG_AN_NAME          "anterior_nucleus"
#define SUBCORTICAL_KG_IL_NAME          "intralaminar_nuclei"
#define SUBCORTICAL_KG_CM_PF_NAME       "centromedian_parafascicular"
#define SUBCORTICAL_KG_TRN_NAME         "thalamic_reticular_nucleus"

/* Nucleus Accumbens node names */
#define SUBCORTICAL_KG_NAC_NAME         "nucleus_accumbens"
#define SUBCORTICAL_KG_NAC_CORE_NAME    "nac_core"
#define SUBCORTICAL_KG_NAC_SHELL_NAME   "nac_shell"

//=============================================================================
// Node Type Extensions (for Subcortical-specific concepts)
//=============================================================================

/**
 * @brief Subcortical-specific KG node types
 *
 * These extend the base brain_kg_node_type_t with subcortical-specific types.
 * Values start at 0x2200 to avoid conflicts with other region types.
 */
typedef enum {
    /** Basal ganglia component (striatum, GP, STN, SN) */
    SUBCORTICAL_KG_NODE_BG_COMPONENT = 0x2200,

    /** Pathway type (direct, indirect, hyperdirect) */
    SUBCORTICAL_KG_NODE_PATHWAY,

    /** Thalamic nucleus */
    SUBCORTICAL_KG_NODE_THALAMIC_NUCLEUS,

    /** Nucleus accumbens component */
    SUBCORTICAL_KG_NODE_NAC_COMPONENT,

    /** Dopaminergic component */
    SUBCORTICAL_KG_NODE_DOPAMINERGIC,

    /** Motor control component */
    SUBCORTICAL_KG_NODE_MOTOR_CONTROL,

    /** Reward/motivation component */
    SUBCORTICAL_KG_NODE_REWARD,

    /** Sensory relay component */
    SUBCORTICAL_KG_NODE_SENSORY_RELAY,

    /** Gating mechanism */
    SUBCORTICAL_KG_NODE_GATING,

    /** Striatal subregion (caudate, putamen, dorsal/ventral) */
    SUBCORTICAL_KG_NODE_STRIATAL_REGION,

    /** Limbic component */
    SUBCORTICAL_KG_NODE_LIMBIC,

    /** Arousal/attention component */
    SUBCORTICAL_KG_NODE_AROUSAL
} subcortical_kg_node_type_t;

/**
 * @brief Subcortical-specific edge types
 */
typedef enum {
    /** Inhibitory projection (GABAergic) */
    SUBCORTICAL_KG_EDGE_INHIBITS_VIA_GABA = 0x2200,

    /** Excitatory projection (glutamatergic) */
    SUBCORTICAL_KG_EDGE_EXCITES_VIA_GLUT,

    /** Dopaminergic modulation */
    SUBCORTICAL_KG_EDGE_MODULATES_VIA_DA,

    /** Disinhibitory effect */
    SUBCORTICAL_KG_EDGE_DISINHIBITS,

    /** Relay signal to cortex */
    SUBCORTICAL_KG_EDGE_RELAYS_TO,

    /** Gates signal */
    SUBCORTICAL_KG_EDGE_GATES,

    /** Part of pathway */
    SUBCORTICAL_KG_EDGE_PATHWAY_COMPONENT,

    /** Provides reward signal */
    SUBCORTICAL_KG_EDGE_SIGNALS_REWARD,

    /** Mesolimbic pathway connection */
    SUBCORTICAL_KG_EDGE_MESOLIMBIC,

    /** Nigrostriatal pathway connection */
    SUBCORTICAL_KG_EDGE_NIGROSTRIATAL
} subcortical_kg_edge_type_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Subcortical KG wiring configuration
 */
typedef struct {
    /** Register basal ganglia nodes */
    bool register_basal_ganglia;

    /** Register thalamus nodes */
    bool register_thalamus;

    /** Register nucleus accumbens nodes */
    bool register_nucleus_accumbens;

    /** Register pathway nodes */
    bool register_pathways;

    /** Register inter-structure edges */
    bool register_cross_edges;

    /** Include state metadata */
    bool include_state_metadata;

    /** Register dopamine system edges */
    bool register_dopamine_edges;

    /** Register sensory relay edges */
    bool register_sensory_edges;

    /** Register striatal subdivisions (caudate, putamen, dorsal/ventral) */
    bool register_striatal_subdivisions;

    /** Register VTA dopamine system */
    bool register_vta;

    /** Register extended thalamic nuclei (AN, IL, CM-PF) */
    bool register_extended_thalamus;
} subcortical_kg_config_t;

/**
 * @brief Subcortical KG wiring state (node IDs for reference)
 */
typedef struct {
    /** Subcortical root node ID */
    brain_kg_node_id_t root_id;

    /* Basal Ganglia node IDs */
    brain_kg_node_id_t bg_id;
    brain_kg_node_id_t striatum_id;
    brain_kg_node_id_t d1_pathway_id;
    brain_kg_node_id_t d2_pathway_id;
    brain_kg_node_id_t gpe_id;
    brain_kg_node_id_t gpi_id;
    brain_kg_node_id_t stn_id;
    brain_kg_node_id_t snc_id;
    brain_kg_node_id_t snr_id;
    brain_kg_node_id_t vta_id;
    brain_kg_node_id_t dorsal_striatum_id;
    brain_kg_node_id_t ventral_striatum_id;
    brain_kg_node_id_t caudate_id;
    brain_kg_node_id_t putamen_id;

    /* Pathway node IDs */
    brain_kg_node_id_t direct_pathway_id;
    brain_kg_node_id_t indirect_pathway_id;
    brain_kg_node_id_t hyperdirect_pathway_id;
    brain_kg_node_id_t mesolimbic_pathway_id;
    brain_kg_node_id_t nigrostriatal_pathway_id;

    /* Thalamus node IDs */
    brain_kg_node_id_t thalamus_id;
    brain_kg_node_id_t lgn_id;
    brain_kg_node_id_t mgn_id;
    brain_kg_node_id_t vpl_id;
    brain_kg_node_id_t vpm_id;
    brain_kg_node_id_t va_id;
    brain_kg_node_id_t vl_id;
    brain_kg_node_id_t pulvinar_id;
    brain_kg_node_id_t md_id;
    brain_kg_node_id_t an_id;
    brain_kg_node_id_t il_id;
    brain_kg_node_id_t cm_pf_id;
    brain_kg_node_id_t trn_id;

    /* Nucleus Accumbens node IDs */
    brain_kg_node_id_t nac_id;
    brain_kg_node_id_t nac_core_id;
    brain_kg_node_id_t nac_shell_id;

    /* Counters */
    uint32_t node_count;
    uint32_t edge_count;

    /** Registration successful */
    bool registered;
} subcortical_kg_state_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default Subcortical KG wiring configuration
 *
 * WHAT: Initializes configuration with sensible defaults
 * WHY:  Provides consistent starting point for KG registration
 * HOW:  Sets all registration flags to true
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int subcortical_kg_default_config(subcortical_kg_config_t* config);

//=============================================================================
// Registration API
//=============================================================================

/**
 * @brief Register all Subcortical nodes in KG
 *
 * WHAT: Creates nodes for subcortical structures in the brain's KG
 * WHY:  Enables semantic queries and reasoning about action selection
 * HOW:  Creates hierarchical node structure with typed edges
 *
 * @param kg Knowledge graph to register in
 * @param config Registration configuration (NULL for defaults)
 * @param state Output registration state (optional, may be NULL)
 * @param admin_token Admin token for write access
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int subcortical_kg_register_all(
    brain_kg_t* kg,
    const subcortical_kg_config_t* config,
    subcortical_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register Basal Ganglia nodes
 *
 * WHAT: Creates nodes for striatum, GP, STN, SN
 * WHY:  Represents core action selection circuitry
 * HOW:  Creates component nodes linked to BG root
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (subcortical root)
 * @param config Configuration options
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int subcortical_kg_register_basal_ganglia(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    const subcortical_kg_config_t* config,
    subcortical_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register Thalamus nodes
 *
 * WHAT: Creates nodes for thalamic nuclei
 * WHY:  Represents sensory relay and cortical gating
 * HOW:  Creates nucleus nodes linked to thalamus root
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (subcortical root)
 * @param config Configuration options
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int subcortical_kg_register_thalamus(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    const subcortical_kg_config_t* config,
    subcortical_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register Nucleus Accumbens nodes
 *
 * WHAT: Creates nodes for NAc core and shell
 * WHY:  Represents reward and motivation processing
 * HOW:  Creates component nodes linked to NAc root
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (subcortical root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int subcortical_kg_register_nucleus_accumbens(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    subcortical_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register pathway nodes
 *
 * WHAT: Creates nodes for direct/indirect/hyperdirect pathways
 * WHY:  Represents functional circuit organization
 * HOW:  Creates pathway nodes with component edges
 *
 * @param kg Knowledge graph
 * @param state State with BG node IDs
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int subcortical_kg_register_pathways(
    brain_kg_t* kg,
    subcortical_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register cross-structure edges
 *
 * WHAT: Creates edges between subcortical structures
 * WHY:  Represents inter-structure connectivity
 * HOW:  BG -> Thalamus, SNc -> Striatum, etc.
 *
 * @param kg Knowledge graph
 * @param state State with node IDs
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int subcortical_kg_register_cross_edges(
    brain_kg_t* kg,
    subcortical_kg_state_t* state,
    uint64_t admin_token
);

//=============================================================================
// State Synchronization API
//=============================================================================

/**
 * @brief Update subcortical node metadata with current state
 *
 * WHAT: Synchronizes KG node metadata with subcortical state
 * WHY:  Enables queries about current action selection state
 * HOW:  Updates node metadata fields
 *
 * @param kg Knowledge graph
 * @param state KG wiring state
 * @param dopamine_level Current dopamine level
 * @param action_selection_entropy Current selection entropy
 * @param thalamic_gating Thalamic gating level
 * @param motivation_level NAc motivation level
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int subcortical_kg_update_state(
    brain_kg_t* kg,
    const subcortical_kg_state_t* state,
    float dopamine_level,
    float action_selection_entropy,
    float thalamic_gating,
    float motivation_level,
    uint64_t admin_token
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get subcortical root node ID
 *
 * @param kg Knowledge graph
 * @return Root node ID or BRAIN_KG_INVALID_NODE
 */
NIMCP_EXPORT brain_kg_node_id_t subcortical_kg_get_root(brain_kg_t* kg);

/**
 * @brief Find subcortical subsystem by name
 *
 * @param kg Knowledge graph
 * @param name Subsystem name ("striatum", "thalamus", etc.)
 * @return Node ID or BRAIN_KG_INVALID_NODE
 */
NIMCP_EXPORT brain_kg_node_id_t subcortical_kg_find_subsystem(
    brain_kg_t* kg,
    const char* name
);

/**
 * @brief Get all basal ganglia nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* subcortical_kg_get_basal_ganglia_nodes(brain_kg_t* kg);

/**
 * @brief Get all thalamic nuclei nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* subcortical_kg_get_thalamus_nodes(brain_kg_t* kg);

/**
 * @brief Get all pathway nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* subcortical_kg_get_pathway_nodes(brain_kg_t* kg);

/**
 * @brief Unregister all subcortical nodes (cleanup)
 *
 * WHAT: Removes all subcortical nodes from KG
 * WHY:  Clean shutdown and resource release
 * HOW:  Removes nodes in reverse creation order
 *
 * @param kg Knowledge graph
 * @param state State with node IDs
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int subcortical_kg_unregister_all(
    brain_kg_t* kg,
    subcortical_kg_state_t* state,
    uint64_t admin_token
);

/* Forward decl for runtime event emit API (W2). */
struct brain_struct;

/**
 * @brief Emit a runtime subcortical event into the brain's internal KG
 *
 * Supported kinds: "action_selected". Silent no-op if brain/KG unavailable.
 * Creates `subcortical_event_<kind>_<ts_us>` node + edge to `subcortical`.
 */
NIMCP_EXPORT void subcortical_kg_emit_event(
    struct brain_struct* brain,
    const char* kind,
    float intensity,
    uint64_t ts_us
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SUBCORTICAL_KG_WIRING_H */
