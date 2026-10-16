//=============================================================================
// nimcp_reticular_kg_wiring.h - Reticular Formation Knowledge Graph Registration
//=============================================================================
/**
 * @file nimcp_reticular_kg_wiring.h
 * @brief Knowledge Graph registration for Reticular Formation module
 *
 * WHAT: Registers reticular formation concepts (nuclei, arousal states,
 *       neuromodulators, autonomic functions, reflexes, motor control)
 *       as nodes in the brain's internal Knowledge Graph.
 *
 * WHY:  KG integration enables:
 *       - Semantic queries about reticular state ("which nuclei are active?")
 *       - Cross-module reasoning about arousal and consciousness
 *       - Introspection of neuromodulatory relationships
 *       - Graph-based analysis of sleep-wake dynamics
 *
 * HOW:  Creates hierarchical node structure:
 *       - Reticular formation root node
 *         ├── Nuclei subsystem
 *         │   ├── Serotonergic (dorsal raphe, median raphe, etc.)
 *         │   ├── Noradrenergic (locus coeruleus, lateral tegmental)
 *         │   ├── Cholinergic (PPN, LDT)
 *         │   └── Dopaminergic (VTA)
 *         ├── Arousal state subsystem
 *         │   ├── Deep sleep, Light sleep, REM
 *         │   ├── Drowsy, Relaxed, Alert
 *         │   └── Hypervigilant
 *         ├── Neuromodulators subsystem
 *         │   ├── Serotonin, Norepinephrine
 *         │   ├── Acetylcholine, Dopamine
 *         │   ├── Histamine, Orexin
 *         │   └── GABA, Glutamate
 *         ├── Autonomic functions subsystem
 *         │   ├── Cardiovascular, Respiratory
 *         │   ├── Vasomotor, Digestive
 *         ├── Reflexes subsystem
 *         │   ├── Swallowing, Coughing, Vomiting
 *         │   ├── Sneezing, Startle, Righting
 *         └── Motor control subsystem
 *             ├── Postural tone, Locomotor drive
 *             └── REM atonia
 *
 * EDGES: Represent functional relationships:
 *       - Nucleus → Modulator (RELEASES)
 *       - Arousal → Nucleus (ACTIVATES)
 *       - Modulator → Arousal (MODULATES)
 *       - Reflex → Motor (TRIGGERS)
 *       - Autonomic → Nucleus (REGULATES)
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_RETICULAR_KG_WIRING_H
#define NIMCP_RETICULAR_KG_WIRING_H

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
#define RETICULAR_KG_MODULE_NAME      "reticular_kg_wiring"

/** Reticular formation root node name */
#define RETICULAR_KG_ROOT_NAME        "reticular_formation"

/** Nuclei subsystem node name */
#define RETICULAR_KG_NUCLEI_NAME      "reticular_nuclei"

/** Arousal state subsystem node name */
#define RETICULAR_KG_AROUSAL_NAME     "arousal_states"

/** Neuromodulators subsystem node name */
#define RETICULAR_KG_MODULATORS_NAME  "neuromodulators"

/** Autonomic functions subsystem node name */
#define RETICULAR_KG_AUTONOMIC_NAME   "autonomic_functions"

/** Reflexes subsystem node name */
#define RETICULAR_KG_REFLEXES_NAME    "reflexes"

/** Motor control subsystem node name */
#define RETICULAR_KG_MOTOR_NAME       "motor_control"

//=============================================================================
// Node Type Extensions (for reticular-specific concepts)
//=============================================================================

/**
 * @brief Reticular-specific KG node types
 *
 * These extend the base brain_kg_node_type_t with reticular-layer specific types.
 * Values start at 0x3200 to avoid conflicts with other module types.
 */
typedef enum {
    /** Brain stem nucleus (raphe, LC, PPN, etc.) */
    RETICULAR_KG_NODE_NUCLEUS = 0x3200,

    /** Arousal/consciousness state */
    RETICULAR_KG_NODE_AROUSAL_STATE,

    /** Neuromodulator substance */
    RETICULAR_KG_NODE_NEUROMODULATOR,

    /** Autonomic function/center */
    RETICULAR_KG_NODE_AUTONOMIC,

    /** Reflex pattern */
    RETICULAR_KG_NODE_REFLEX,

    /** Motor control function */
    RETICULAR_KG_NODE_MOTOR_FUNCTION,

    /** Sleep stage */
    RETICULAR_KG_NODE_SLEEP_STAGE,

    /** Projection pathway */
    RETICULAR_KG_NODE_PROJECTION,

    /** Sensory gate */
    RETICULAR_KG_NODE_SENSORY_GATE,

    /** Pain modulation */
    RETICULAR_KG_NODE_PAIN_MOD
} reticular_kg_node_type_t;

/**
 * @brief Reticular-specific edge types
 */
typedef enum {
    /** Nucleus releases neuromodulator */
    RETICULAR_KG_EDGE_RELEASES = 0x3200,

    /** Arousal state activates nucleus */
    RETICULAR_KG_EDGE_ACTIVATES,

    /** Modulator affects arousal */
    RETICULAR_KG_EDGE_MODULATES_AROUSAL,

    /** Reflex triggers motor response */
    RETICULAR_KG_EDGE_TRIGGERS,

    /** Autonomic center regulates nucleus */
    RETICULAR_KG_EDGE_REGULATES,

    /** Nucleus projects to region */
    RETICULAR_KG_EDGE_PROJECTS_TO,

    /** Gate controls sensory flow */
    RETICULAR_KG_EDGE_GATES,

    /** State inhibits function */
    RETICULAR_KG_EDGE_INHIBITS_FUNCTION,

    /** Reciprocal connection */
    RETICULAR_KG_EDGE_RECIPROCAL,

    /** State transition edge */
    RETICULAR_KG_EDGE_TRANSITIONS_TO
} reticular_kg_edge_type_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief KG wiring configuration for reticular formation
 */
typedef struct {
    /** Register nuclei nodes */
    bool register_nuclei;

    /** Register arousal state nodes */
    bool register_arousal;

    /** Register neuromodulator nodes */
    bool register_modulators;

    /** Register autonomic function nodes */
    bool register_autonomic;

    /** Register reflex nodes */
    bool register_reflexes;

    /** Register motor control nodes */
    bool register_motor;

    /** Register inter-subsystem edges */
    bool register_cross_edges;

    /** Include state metadata */
    bool include_state_metadata;

    /** Register individual nucleus details */
    bool register_nucleus_details;
} reticular_kg_config_t;

/**
 * @brief KG wiring state (node IDs for reference)
 */
typedef struct {
    /** Reticular formation root node ID */
    brain_kg_node_id_t root_id;

    /** Nuclei subsystem node ID */
    brain_kg_node_id_t nuclei_root_id;

    /** Arousal state subsystem node ID */
    brain_kg_node_id_t arousal_root_id;

    /** Neuromodulators subsystem node ID */
    brain_kg_node_id_t modulators_root_id;

    /** Autonomic subsystem node ID */
    brain_kg_node_id_t autonomic_root_id;

    /** Reflexes subsystem node ID */
    brain_kg_node_id_t reflexes_root_id;

    /** Motor control subsystem node ID */
    brain_kg_node_id_t motor_root_id;

    /* Individual nuclei node IDs */
    brain_kg_node_id_t dorsal_raphe_id;
    brain_kg_node_id_t median_raphe_id;
    brain_kg_node_id_t raphe_magnus_id;
    brain_kg_node_id_t raphe_obscurus_id;
    brain_kg_node_id_t locus_coeruleus_id;
    brain_kg_node_id_t lateral_tegmental_id;
    brain_kg_node_id_t ppn_id;           /**< Pedunculopontine nucleus */
    brain_kg_node_id_t ldt_id;           /**< Laterodorsal tegmental */
    brain_kg_node_id_t pontine_oral_id;
    brain_kg_node_id_t pontine_caudal_id;
    brain_kg_node_id_t gigantocellular_id;
    brain_kg_node_id_t parvocellular_id;
    brain_kg_node_id_t paramedian_id;
    brain_kg_node_id_t ventral_medullary_id;
    brain_kg_node_id_t vta_id;

    /* Arousal state node IDs */
    brain_kg_node_id_t deep_sleep_id;
    brain_kg_node_id_t light_sleep_id;
    brain_kg_node_id_t rem_sleep_id;
    brain_kg_node_id_t drowsy_id;
    brain_kg_node_id_t relaxed_id;
    brain_kg_node_id_t alert_id;
    brain_kg_node_id_t hypervigilant_id;

    /* Neuromodulator node IDs */
    brain_kg_node_id_t serotonin_id;
    brain_kg_node_id_t norepinephrine_id;
    brain_kg_node_id_t acetylcholine_id;
    brain_kg_node_id_t dopamine_id;
    brain_kg_node_id_t histamine_id;
    brain_kg_node_id_t orexin_id;
    brain_kg_node_id_t gaba_id;
    brain_kg_node_id_t glutamate_id;

    /* Autonomic function node IDs */
    brain_kg_node_id_t cardiovascular_id;
    brain_kg_node_id_t respiratory_id;
    brain_kg_node_id_t vasomotor_id;
    brain_kg_node_id_t digestive_id;

    /* Reflex node IDs */
    brain_kg_node_id_t swallowing_id;
    brain_kg_node_id_t coughing_id;
    brain_kg_node_id_t vomiting_id;
    brain_kg_node_id_t sneezing_id;
    brain_kg_node_id_t gagging_id;
    brain_kg_node_id_t yawning_id;
    brain_kg_node_id_t startle_id;
    brain_kg_node_id_t righting_id;

    /* Motor control node IDs */
    brain_kg_node_id_t postural_tone_id;
    brain_kg_node_id_t locomotor_drive_id;
    brain_kg_node_id_t rem_atonia_id;

    /** Number of nodes registered */
    uint32_t node_count;

    /** Number of edges registered */
    uint32_t edge_count;

    /** Registration successful */
    bool registered;
} reticular_kg_state_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default KG wiring configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_kg_default_config(reticular_kg_config_t* config);

//=============================================================================
// Registration API
//=============================================================================

/**
 * @brief Register all reticular formation nodes in KG
 *
 * WHAT: Creates nodes for reticular concepts in the brain's KG
 * WHY:  Enables semantic queries and reasoning about arousal/consciousness
 * HOW:  Creates hierarchical node structure with typed edges
 *
 * @param kg Knowledge graph to register in
 * @param config Registration configuration (NULL for defaults)
 * @param state Output registration state (optional, may be NULL)
 * @param admin_token Admin token for write access
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_kg_register_all(
    brain_kg_t* kg,
    const reticular_kg_config_t* config,
    reticular_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register nuclei subsystem nodes
 *
 * WHAT: Creates nodes for all reticular nuclei
 * WHY:  Enables queries about nucleus activity and neuromodulation
 * HOW:  Creates nodes for raphe, LC, PPN, LDT, etc.
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (reticular root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_kg_register_nuclei(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    reticular_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register arousal state nodes
 *
 * WHAT: Creates nodes for arousal/consciousness states
 * WHY:  Enables queries about sleep-wake transitions
 * HOW:  Creates nodes for deep sleep through hypervigilant
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (reticular root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_kg_register_arousal(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    reticular_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register neuromodulator nodes
 *
 * WHAT: Creates nodes for neuromodulator substances
 * WHY:  Enables queries about modulatory state
 * HOW:  Creates nodes for 5-HT, NE, ACh, DA, etc.
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (reticular root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_kg_register_modulators(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    reticular_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register autonomic function nodes
 *
 * WHAT: Creates nodes for autonomic functions
 * WHY:  Enables queries about vital function regulation
 * HOW:  Creates nodes for cardiovascular, respiratory, etc.
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (reticular root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_kg_register_autonomic(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    reticular_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register reflex nodes
 *
 * WHAT: Creates nodes for brainstem reflexes
 * WHY:  Enables queries about protective reflexes
 * HOW:  Creates nodes for swallowing, coughing, etc.
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (reticular root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_kg_register_reflexes(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    reticular_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register motor control nodes
 *
 * WHAT: Creates nodes for motor control functions
 * WHY:  Enables queries about muscle tone and locomotion
 * HOW:  Creates nodes for postural tone, REM atonia, etc.
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (reticular root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_kg_register_motor(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    reticular_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register cross-subsystem edges
 *
 * WHAT: Creates edges between reticular subsystems
 * WHY:  Represents functional relationships across modules
 * HOW:  Nucleus→Modulator, Arousal→Nucleus, Reflex→Motor, etc.
 *
 * @param kg Knowledge graph
 * @param state State with node IDs
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_kg_register_cross_edges(
    brain_kg_t* kg,
    reticular_kg_state_t* state,
    uint64_t admin_token
);

//=============================================================================
// State Synchronization API
//=============================================================================

/**
 * @brief Update reticular node metadata with current state
 *
 * WHAT: Synchronizes KG node metadata with reticular state
 * WHY:  Enables queries about current arousal/modulator values
 * HOW:  Updates node metadata fields
 *
 * @param kg Knowledge graph
 * @param state KG wiring state
 * @param arousal_level Current arousal level (0-1)
 * @param arousal_state Current arousal state enum
 * @param serotonin_level Current serotonin concentration
 * @param norepinephrine_level Current norepinephrine concentration
 * @param acetylcholine_level Current acetylcholine concentration
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_kg_update_state(
    brain_kg_t* kg,
    const reticular_kg_state_t* state,
    float arousal_level,
    int arousal_state,
    float serotonin_level,
    float norepinephrine_level,
    float acetylcholine_level,
    uint64_t admin_token
);

/**
 * @brief Update nucleus activity in KG
 *
 * @param kg Knowledge graph
 * @param state KG wiring state
 * @param nucleus_type Nucleus type enum
 * @param activity Activity level (0-1)
 * @param firing_rate Firing rate (Hz)
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_kg_update_nucleus(
    brain_kg_t* kg,
    const reticular_kg_state_t* state,
    int nucleus_type,
    float activity,
    float firing_rate,
    uint64_t admin_token
);

/**
 * @brief Update autonomic state in KG
 *
 * @param kg Knowledge graph
 * @param state KG wiring state
 * @param autonomic_type Autonomic function type
 * @param sympathetic_tone Sympathetic activity (0-1)
 * @param parasympathetic_tone Parasympathetic activity (0-1)
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_kg_update_autonomic(
    brain_kg_t* kg,
    const reticular_kg_state_t* state,
    int autonomic_type,
    float sympathetic_tone,
    float parasympathetic_tone,
    uint64_t admin_token
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get reticular formation root node ID
 *
 * @param kg Knowledge graph
 * @return Root node ID or BRAIN_KG_INVALID_NODE
 */
NIMCP_EXPORT brain_kg_node_id_t reticular_kg_get_root(brain_kg_t* kg);

/**
 * @brief Find reticular subsystem by name
 *
 * @param kg Knowledge graph
 * @param name Subsystem name ("nuclei", "arousal_states", etc.)
 * @return Node ID or BRAIN_KG_INVALID_NODE
 */
NIMCP_EXPORT brain_kg_node_id_t reticular_kg_find_subsystem(
    brain_kg_t* kg,
    const char* name
);

/**
 * @brief Get all nucleus nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* reticular_kg_get_nuclei(brain_kg_t* kg);

/**
 * @brief Get all arousal state nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* reticular_kg_get_arousal_states(brain_kg_t* kg);

/**
 * @brief Get all neuromodulator nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* reticular_kg_get_modulators(brain_kg_t* kg);

/**
 * @brief Get nuclei that release a specific modulator
 *
 * @param kg Knowledge graph
 * @param modulator_id Modulator node ID
 * @return Node list of nuclei (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* reticular_kg_get_nuclei_for_modulator(
    brain_kg_t* kg,
    brain_kg_node_id_t modulator_id
);

/**
 * @brief Get modulators released by a specific nucleus
 *
 * @param kg Knowledge graph
 * @param nucleus_id Nucleus node ID
 * @return Node list of modulators (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* reticular_kg_get_modulators_from_nucleus(
    brain_kg_t* kg,
    brain_kg_node_id_t nucleus_id
);

/**
 * @brief Unregister all reticular nodes (cleanup)
 *
 * @param kg Knowledge graph
 * @param state State with node IDs
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_kg_unregister_all(
    brain_kg_t* kg,
    reticular_kg_state_t* state,
    uint64_t admin_token
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RETICULAR_KG_WIRING_H */
