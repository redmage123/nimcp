/**
 * @file nimcp_kg_hierarchy.h
 * @brief Hierarchical View of Brain Knowledge Graph
 * @version 1.1.0
 * @date 2025-01-15
 *
 * WHAT: Four-level hierarchical view of the running brain's module topology
 * WHY:  Modules need dynamic real-time view of brain state for introspection
 * HOW:  Overlay on brain_kg with efficient hierarchical queries
 *
 * HIERARCHY LEVELS:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                      BRAIN HIERARCHY VIEW (4 Levels)                      ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   Level 0 (Brain)                                                          ║
 * ║   ─────────────────                                                        ║
 * ║   ┌───────────────────────────────────────────────────────────────────┐   ║
 * ║   │                         NIMCP BRAIN                                │   ║
 * ║   │    Total Modules: 200+   Running: 180   Health: 0.95              │   ║
 * ║   └───────────────────────────────────────────────────────────────────┘   ║
 * ║                                   │                                        ║
 * ║              ┌────────────────────┴────────────────────┐                  ║
 * ║              ▼                                         ▼                  ║
 * ║   Level 1 (Hemispheres)                                                    ║
 * ║   ─────────────────────                                                    ║
 * ║   ┌─────────────────────────┐     ┌─────────────────────────┐            ║
 * ║   │   LEFT HEMISPHERE        │     │   RIGHT HEMISPHERE       │            ║
 * ║   │   Language, Logic,       │     │   Spatial, Pattern,      │            ║
 * ║   │   Sequential Processing  │     │   Holistic Processing    │            ║
 * ║   │   95 modules             │     │   85 modules             │            ║
 * ║   └───────────┬─────────────┘     └───────────┬─────────────┘            ║
 * ║               │                               │                           ║
 * ║               ▼                               ▼                           ║
 * ║   Level 2 (Layers per Hemisphere)                                          ║
 * ║   ────────────────────────────                                             ║
 * ║   ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ║
 * ║   │Layer I  │ │Layer II │ │Layer III│ │Layer IV │ │Layer V  │ │Layer VI │ ║
 * ║   │Top-Down │ │ Local   │ │Cortical │ │ Input   │ │ Output  │ │Feedback │ ║
 * ║   │  12 mod │ │  20 mod │ │  45 mod │ │  35 mod │ │  28 mod │ │  18 mod │ ║
 * ║   └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘ ║
 * ║        │           │           │           │           │           │      ║
 * ║                                   ▼                                        ║
 * ║   Level 3 (Modules)                                                        ║
 * ║   ─────────────────                                                        ║
 * ║   Each module has: ID, Name, Hemisphere, Layer, State, Health, Stats       ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * HEMISPHERE ASSIGNMENT:
 * - Left:      Language (Broca, Wernicke), Logic, Math, Sequential processing
 * - Right:     Spatial, Creativity, Pattern recognition, Holistic processing
 * - Bilateral: Core systems, Coordination, Global workspace (spans both)
 *
 * LAYER ASSIGNMENT (per hemisphere):
 * - Layer I:   HIGHLEVEL, COORDINATOR (top-down modulation, global workspace)
 * - Layer II:  COGNITIVE upper (lateral integration, feature binding)
 * - Layer III: COGNITIVE lower (cortico-cortical connections)
 * - Layer IV:  PERCEPTION, MIDDLEWARE (input processing, feature detection)
 * - Layer V:   CORE, SWARM (output pathways, action commands)
 * - Layer VI:  PLASTICITY, IMMUNE, GLIAL (feedback, homeostasis)
 *
 * THREAD SAFETY: Reader-writer lock for concurrent read access
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_KG_HIERARCHY_H
#define NIMCP_KG_HIERARCHY_H

#include "core/brain/nimcp_brain_kg.h"
#include "async/nimcp_bio_async_orchestrator.h"
#include "async/nimcp_wiring_diagram.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Number of cortical layers in the hierarchy */
#define KG_HIERARCHY_LAYER_COUNT    6

/** Number of hemispheres */
#define KG_HIERARCHY_HEMISPHERE_COUNT  3  /* LEFT, RIGHT, BILATERAL */

/** Maximum modules per layer */
#define KG_HIERARCHY_MAX_MODULES_PER_LAYER  256

/** Maximum total modules in hierarchy */
#define KG_HIERARCHY_MAX_MODULES    1024

/** Maximum callbacks for state changes */
#define KG_HIERARCHY_MAX_CALLBACKS  16

/* ============================================================================
 * Hierarchy Level Enumeration
 * ============================================================================ */

/**
 * @brief Hierarchy level identifiers (4-level system)
 */
typedef enum {
    KG_LEVEL_BRAIN = 0,            /**< Level 0: Full brain */
    KG_LEVEL_HEMISPHERE,           /**< Level 1: Brain hemispheres */
    KG_LEVEL_LAYER,                /**< Level 2: Cortical layers */
    KG_LEVEL_MODULE                /**< Level 3: Individual modules */
} kg_hierarchy_level_t;

/**
 * @brief Brain hemisphere identifiers
 *
 * Modules are assigned to hemispheres based on their function:
 * - LEFT:     Language (Broca, Wernicke), logic, math, sequential
 * - RIGHT:    Spatial processing, creativity, pattern recognition
 * - BILATERAL: Core systems, coordination, global workspace
 */
typedef enum {
    KG_HEMISPHERE_LEFT = 0,        /**< Left hemisphere (language, logic) */
    KG_HEMISPHERE_RIGHT,           /**< Right hemisphere (spatial, creative) */
    KG_HEMISPHERE_BILATERAL,       /**< Bilateral (spans both hemispheres) */
    KG_HEMISPHERE_COUNT
} kg_hemisphere_t;

/**
 * @brief Cortical layer identifiers (biological 6-layer model)
 */
typedef enum {
    KG_LAYER_I = 0,                /**< Molecular layer (top-down modulation) */
    KG_LAYER_II,                   /**< External granular (local processing) */
    KG_LAYER_III,                  /**< External pyramidal (cortico-cortical) */
    KG_LAYER_IV,                   /**< Internal granular (thalamic input) */
    KG_LAYER_V,                    /**< Internal pyramidal (subcortical output) */
    KG_LAYER_VI,                   /**< Multiform (corticothalamic feedback) */
    KG_LAYER_COUNT
} kg_cortical_layer_t;

/**
 * @brief Module operational state for real-time view
 */
typedef enum {
    KG_MODULE_STATE_UNKNOWN = 0,   /**< State not determined */
    KG_MODULE_STATE_STOPPED,       /**< Not running */
    KG_MODULE_STATE_STARTING,      /**< Initialization in progress */
    KG_MODULE_STATE_RUNNING,       /**< Active and processing */
    KG_MODULE_STATE_PAUSED,        /**< Temporarily paused */
    KG_MODULE_STATE_DEGRADED,      /**< Running with reduced capability */
    KG_MODULE_STATE_ERROR          /**< Error state */
} kg_module_state_t;

/* ============================================================================
 * Data Structures - Level 2 (Module Info)
 * ============================================================================ */

/**
 * @brief Connection info between modules
 */
typedef struct {
    brain_kg_node_id_t target_id;  /**< Connected module node ID */
    brain_kg_edge_type_t edge_type; /**< Connection type */
    float weight;                  /**< Connection strength [0-1] */
    bool bidirectional;            /**< Is connection bidirectional */
} kg_connection_t;

/**
 * @brief Message flow statistics for a module
 */
typedef struct {
    uint64_t messages_sent;        /**< Total messages sent */
    uint64_t messages_received;    /**< Total messages received */
    uint64_t messages_dropped;     /**< Messages dropped */
    float throughput_msgs_per_sec; /**< Current throughput */
    float avg_latency_us;          /**< Average message latency */
    float max_latency_us;          /**< Max observed latency */
} kg_message_stats_t;

/**
 * @brief Level 3: Module-level detail view
 */
typedef struct {
    /* Identity */
    brain_kg_node_id_t node_id;    /**< KG node ID */
    char name[BRAIN_KG_MAX_NAME_LEN]; /**< Module name */
    brain_kg_node_type_t node_type; /**< KG node type */
    bio_module_category_t category; /**< Orchestrator category */

    /* State */
    kg_module_state_t state;       /**< Operational state */
    bio_module_health_t health;    /**< Health status (from orchestrator) */
    uint64_t state_change_time;    /**< Last state change timestamp (ms) */

    /* Hierarchical placement */
    kg_hemisphere_t hemisphere;    /**< Assigned hemisphere (left/right/bilateral) */
    kg_cortical_layer_t layer;     /**< Assigned cortical layer */
    uint32_t startup_phase;        /**< Startup phase (0-5) */

    /* Message statistics */
    kg_message_stats_t msg_stats;

    /* Flags */
    bool enabled;                  /**< Whether module is enabled */
    bool has_anomaly;              /**< Anomaly detected */
} kg_module_info_t;

/* ============================================================================
 * Data Structures - Level 1 (Hemisphere Info)
 * ============================================================================ */

/**
 * @brief Level 1: Hemisphere-level view
 */
typedef struct {
    kg_hemisphere_t hemisphere_id; /**< Hemisphere identifier */
    const char* name;              /**< Hemisphere name (e.g., "Left Hemisphere") */
    const char* description;       /**< Functional description */

    /* Aggregate state */
    uint32_t total_modules;        /**< Total modules in this hemisphere */
    uint32_t running_modules;      /**< Running modules */
    uint32_t stopped_modules;      /**< Stopped modules */
    uint32_t error_modules;        /**< Modules in error state */

    /* Layer breakdown */
    uint32_t modules_per_layer[KG_LAYER_COUNT]; /**< Module count per layer */

    /* Inter-hemisphere connectivity */
    uint32_t interhemispheric_connections; /**< Connections to other hemisphere */
    uint32_t intrahemispheric_connections; /**< Connections within hemisphere */

    /* Aggregate health */
    bio_module_health_t overall_health; /**< Worst health in hemisphere */
    float health_score;            /**< Health score [0-1] */
} kg_hemisphere_info_t;

/* ============================================================================
 * Data Structures - Level 2 (Layer Info)
 * ============================================================================ */

/**
 * @brief Level 2: Layer-level view
 */
typedef struct {
    kg_cortical_layer_t layer_id;  /**< Layer identifier */
    const char* name;              /**< Layer name (e.g., "Layer IV") */
    const char* description;       /**< Biological description */

    /* Parent hemisphere */
    kg_hemisphere_t hemisphere;    /**< Hemisphere this layer belongs to */

    /* Aggregate state */
    uint32_t total_modules;        /**< Total modules in this layer */
    uint32_t running_modules;      /**< Running modules */
    uint32_t stopped_modules;      /**< Stopped modules */
    uint32_t error_modules;        /**< Modules in error state */

    /* Inter-layer connectivity */
    uint32_t feedforward_connections; /**< Connections to higher layers */
    uint32_t feedback_connections;    /**< Connections to lower layers */
    uint32_t lateral_connections;     /**< Connections within layer */

    /* Aggregate health */
    bio_module_health_t overall_health; /**< Worst health in layer */
} kg_layer_info_t;

/* ============================================================================
 * Data Structures - Level 0 (Brain Info)
 * ============================================================================ */

/**
 * @brief Level 0: Brain-level statistics
 */
typedef struct {
    uint32_t total_modules;        /**< Total modules */
    uint32_t running_modules;      /**< Running modules */
    uint32_t total_connections;    /**< Total edges in KG */
    uint64_t total_messages;       /**< Total messages routed */
    bio_module_health_t brain_health; /**< Overall brain health */
    float health_score;            /**< Health score [0-1] */
    uint64_t uptime_ms;            /**< Brain uptime */

    /* Hemisphere breakdown */
    uint32_t left_modules;         /**< Modules in left hemisphere */
    uint32_t right_modules;        /**< Modules in right hemisphere */
    uint32_t bilateral_modules;    /**< Modules spanning both hemispheres */
    uint32_t interhemispheric_conn; /**< Connections between hemispheres */
} kg_brain_stats_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Hierarchy configuration
 */
typedef struct {
    bool lazy_rebuild;             /**< Lazy rebuild on query (vs immediate) */
    uint32_t cache_ttl_ms;         /**< Cache time-to-live (0 = no expiry) */
    bool subscribe_orchestrator;   /**< Auto-subscribe to orchestrator events */
    bool subscribe_wiring;         /**< Auto-subscribe to wiring changes */
} kg_hierarchy_config_t;

/* ============================================================================
 * State Change Notification
 * ============================================================================ */

/**
 * @brief State change event for real-time updates
 */
typedef struct {
    brain_kg_node_id_t module_id;  /**< Module that changed */
    kg_module_state_t old_state;   /**< Previous state */
    kg_module_state_t new_state;   /**< New state */
    bio_module_health_t health;    /**< Current health */
    uint64_t timestamp;            /**< Change timestamp */
    char reason[128];              /**< Reason for change */
} kg_state_change_event_t;

/**
 * @brief State change callback type
 */
typedef void (*kg_state_change_callback_t)(
    const kg_state_change_event_t* event,
    void* user_data
);

/* ============================================================================
 * Opaque Handle
 * ============================================================================ */

/**
 * @brief Hierarchy handle (opaque)
 */
typedef struct kg_hierarchy kg_hierarchy_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success
 */
int kg_hierarchy_default_config(kg_hierarchy_config_t* config);

/**
 * @brief Create hierarchical view of brain KG
 *
 * WHAT: Initialize hierarchy structure from existing KG
 * WHY:  Provide organized view of module topology
 * HOW:  Scan KG nodes, classify into layers, build indices
 *
 * @param kg Brain knowledge graph
 * @param config Configuration (NULL for defaults)
 * @return Hierarchy handle or NULL on error
 */
kg_hierarchy_t* kg_hierarchy_create(
    brain_kg_t* kg,
    const kg_hierarchy_config_t* config
);

/**
 * @brief Destroy hierarchy structure
 *
 * @param hier Hierarchy to destroy (NULL safe)
 * @note Does NOT destroy underlying KG
 */
void kg_hierarchy_destroy(kg_hierarchy_t* hier);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Connect hierarchy to orchestrator
 *
 * WHAT: Integrate with bio-async orchestrator for automatic updates
 * WHY:  Orchestrator has authoritative module state
 * HOW:  Register for orchestrator events, sync state
 *
 * @param hier Hierarchy handle
 * @param orchestrator Bio-async orchestrator
 * @return 0 on success
 */
int kg_hierarchy_connect_orchestrator(
    kg_hierarchy_t* hier,
    bio_async_orchestrator_t* orchestrator
);

/**
 * @brief Connect hierarchy to wiring diagram
 *
 * WHAT: Integrate with wiring diagram for connection info
 * WHY:  Wiring diagram has module connectivity
 * HOW:  Query wiring diagram to populate connections
 *
 * @param hier Hierarchy handle
 * @param wd Wiring diagram
 * @return 0 on success
 */
int kg_hierarchy_connect_wiring(
    kg_hierarchy_t* hier,
    wiring_diagram_t* wd
);

/* ============================================================================
 * Level 0 Queries (Brain Level)
 * ============================================================================ */

/**
 * @brief Get brain-level statistics
 *
 * @param hier Hierarchy handle
 * @param stats Output statistics
 * @return 0 on success
 */
int kg_hierarchy_get_brain_stats(
    const kg_hierarchy_t* hier,
    kg_brain_stats_t* stats
);

/**
 * @brief Get all hemisphere information
 *
 * @param hier Hierarchy handle
 * @param hemispheres Output array (caller allocated, size KG_HEMISPHERE_COUNT)
 * @return Number of hemispheres populated
 */
uint32_t kg_hierarchy_get_hemispheres(
    const kg_hierarchy_t* hier,
    kg_hemisphere_info_t* hemispheres
);

/**
 * @brief Get all layer information (across all hemispheres)
 *
 * @param hier Hierarchy handle
 * @param layers Output array (caller allocated, size KG_LAYER_COUNT)
 * @return Number of layers populated
 */
uint32_t kg_hierarchy_get_layers(
    const kg_hierarchy_t* hier,
    kg_layer_info_t* layers
);

/**
 * @brief Get brain-wide health status
 *
 * @param hier Hierarchy handle
 * @return Overall health status
 */
bio_module_health_t kg_hierarchy_get_brain_health(const kg_hierarchy_t* hier);

/* ============================================================================
 * Level 1 Queries (Hemisphere Level)
 * ============================================================================ */

/**
 * @brief Get hemisphere information
 *
 * @param hier Hierarchy handle
 * @param hemisphere Hemisphere identifier (LEFT, RIGHT, or BILATERAL)
 * @param info Output hemisphere info
 * @return 0 on success, -1 if hemisphere invalid
 */
int kg_hierarchy_get_hemisphere_info(
    const kg_hierarchy_t* hier,
    kg_hemisphere_t hemisphere,
    kg_hemisphere_info_t* info
);

/**
 * @brief Get all layers in a hemisphere
 *
 * @param hier Hierarchy handle
 * @param hemisphere Hemisphere identifier
 * @param layers Output array (caller allocated, size KG_LAYER_COUNT)
 * @return Number of layers populated
 */
uint32_t kg_hierarchy_get_hemisphere_layers(
    const kg_hierarchy_t* hier,
    kg_hemisphere_t hemisphere,
    kg_layer_info_t* layers
);

/**
 * @brief Get all module IDs in a hemisphere
 *
 * @param hier Hierarchy handle
 * @param hemisphere Hemisphere identifier
 * @param module_ids Output array (caller allocated)
 * @param max_modules Array capacity
 * @return Number of modules found
 */
uint32_t kg_hierarchy_get_hemisphere_modules(
    const kg_hierarchy_t* hier,
    kg_hemisphere_t hemisphere,
    brain_kg_node_id_t* module_ids,
    uint32_t max_modules
);

/**
 * @brief Get inter-hemispheric connection count
 *
 * @param hier Hierarchy handle
 * @return Number of connections between left and right hemispheres
 */
uint32_t kg_hierarchy_get_interhemispheric_connections(
    const kg_hierarchy_t* hier
);

/**
 * @brief Get hemisphere health status
 *
 * @param hier Hierarchy handle
 * @param hemisphere Hemisphere identifier
 * @return Health status for the hemisphere
 */
bio_module_health_t kg_hierarchy_get_hemisphere_health(
    const kg_hierarchy_t* hier,
    kg_hemisphere_t hemisphere
);

/* ============================================================================
 * Level 2 Queries (Layer Level)
 * ============================================================================ */

/**
 * @brief Get layer information
 *
 * @param hier Hierarchy handle
 * @param layer Layer identifier
 * @param info Output layer info
 * @return 0 on success, -1 if layer not found
 */
int kg_hierarchy_get_layer_info(
    const kg_hierarchy_t* hier,
    kg_cortical_layer_t layer,
    kg_layer_info_t* info
);

/**
 * @brief Get all module IDs in a layer
 *
 * @param hier Hierarchy handle
 * @param layer Layer identifier
 * @param module_ids Output array (caller allocated)
 * @param max_modules Array capacity
 * @return Number of modules found
 */
uint32_t kg_hierarchy_get_layer_modules(
    const kg_hierarchy_t* hier,
    kg_cortical_layer_t layer,
    brain_kg_node_id_t* module_ids,
    uint32_t max_modules
);

/**
 * @brief Get inter-layer connection count
 *
 * @param hier Hierarchy handle
 * @param from_layer Source layer
 * @param to_layer Target layer
 * @return Number of connections between layers
 */
uint32_t kg_hierarchy_get_interlayer_connections(
    const kg_hierarchy_t* hier,
    kg_cortical_layer_t from_layer,
    kg_cortical_layer_t to_layer
);

/* ============================================================================
 * Level 3 Queries (Module Level)
 * ============================================================================ */

/**
 * @brief Get detailed module information
 *
 * @param hier Hierarchy handle
 * @param module_id Module KG node ID
 * @param info Output module info
 * @return 0 on success, -1 if not found
 */
int kg_hierarchy_get_module_info(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t module_id,
    kg_module_info_t* info
);

/**
 * @brief Find module by name
 *
 * @param hier Hierarchy handle
 * @param name Module name
 * @return Module node ID or BRAIN_KG_INVALID_NODE if not found
 */
brain_kg_node_id_t kg_hierarchy_find_module_by_name(
    const kg_hierarchy_t* hier,
    const char* name
);

/**
 * @brief Get modules by category
 *
 * @param hier Hierarchy handle
 * @param category Module category
 * @param module_ids Output array (caller allocated)
 * @param max_modules Array capacity
 * @return Number of modules found
 */
uint32_t kg_hierarchy_get_modules_by_category(
    const kg_hierarchy_t* hier,
    bio_module_category_t category,
    brain_kg_node_id_t* module_ids,
    uint32_t max_modules
);

/**
 * @brief Get hemisphere containing a module
 *
 * @param hier Hierarchy handle
 * @param module_id Module node ID
 * @return Hemisphere (LEFT/RIGHT/BILATERAL) or -1 if not found
 */
int kg_hierarchy_get_module_hemisphere(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t module_id
);

/**
 * @brief Get module input connections
 *
 * @param hier Hierarchy handle
 * @param module_id Module node ID
 * @param connections Output array (caller allocated)
 * @param max_connections Array capacity
 * @return Number of inputs found
 */
uint32_t kg_hierarchy_get_module_inputs(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t module_id,
    kg_connection_t* connections,
    uint32_t max_connections
);

/**
 * @brief Get module output connections
 *
 * @param hier Hierarchy handle
 * @param module_id Module node ID
 * @param connections Output array (caller allocated)
 * @param max_connections Array capacity
 * @return Number of outputs found
 */
uint32_t kg_hierarchy_get_module_outputs(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t module_id,
    kg_connection_t* connections,
    uint32_t max_connections
);

/* ============================================================================
 * Hierarchical Traversal
 * ============================================================================ */

/**
 * @brief Get parent of a node in the hierarchy
 *
 * @param hier Hierarchy handle
 * @param node_id Node ID
 * @return Parent node ID or BRAIN_KG_INVALID_NODE if root
 */
brain_kg_node_id_t kg_hierarchy_get_parent(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t node_id
);

/**
 * @brief Get layer containing a module
 *
 * @param hier Hierarchy handle
 * @param module_id Module node ID
 * @return Layer index or -1 if not found
 */
int kg_hierarchy_get_module_layer(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t module_id
);

/**
 * @brief Get hierarchy level of a node
 *
 * @param hier Hierarchy handle
 * @param node_id Node ID
 * @return Hierarchy level (0=brain, 1=hemisphere, 2=layer, 3=module)
 */
kg_hierarchy_level_t kg_hierarchy_get_level(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t node_id
);

/**
 * @brief Get dependent modules (modules that depend on this one)
 *
 * @param hier Hierarchy handle
 * @param module_id Module node ID
 * @param dependents Output array (caller allocated)
 * @param max_dependents Array capacity
 * @return Number of dependents
 */
uint32_t kg_hierarchy_get_dependents(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t module_id,
    brain_kg_node_id_t* dependents,
    uint32_t max_dependents
);

/**
 * @brief Get dependencies (modules this one depends on)
 *
 * @param hier Hierarchy handle
 * @param module_id Module node ID
 * @param dependencies Output array (caller allocated)
 * @param max_deps Array capacity
 * @return Number of dependencies
 */
uint32_t kg_hierarchy_get_dependencies(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t module_id,
    brain_kg_node_id_t* dependencies,
    uint32_t max_deps
);

/* ============================================================================
 * Real-Time State Updates
 * ============================================================================ */

/**
 * @brief Report module state change
 *
 * WHAT: Module reports its state change to hierarchy
 * WHY:  Enable real-time dynamic view
 * HOW:  Update cached state, invoke callbacks, update statistics
 *
 * @param hier Hierarchy handle
 * @param module_id Module reporting change
 * @param new_state New operational state
 * @param reason Reason for change (can be NULL)
 * @return 0 on success
 */
int kg_hierarchy_report_state_change(
    kg_hierarchy_t* hier,
    brain_kg_node_id_t module_id,
    kg_module_state_t new_state,
    const char* reason
);

/**
 * @brief Report module health change
 *
 * @param hier Hierarchy handle
 * @param module_id Module reporting
 * @param health New health status
 * @return 0 on success
 */
int kg_hierarchy_report_health_change(
    kg_hierarchy_t* hier,
    brain_kg_node_id_t module_id,
    bio_module_health_t health
);

/**
 * @brief Report message statistics update
 *
 * @param hier Hierarchy handle
 * @param module_id Module reporting
 * @param sent Messages sent since last update
 * @param received Messages received since last update
 * @return 0 on success
 */
int kg_hierarchy_report_message_stats(
    kg_hierarchy_t* hier,
    brain_kg_node_id_t module_id,
    uint64_t sent,
    uint64_t received
);

/**
 * @brief Report anomaly detection
 *
 * @param hier Hierarchy handle
 * @param module_id Module with anomaly
 * @param has_anomaly True if anomaly detected, false to clear
 * @return 0 on success
 */
int kg_hierarchy_report_anomaly(
    kg_hierarchy_t* hier,
    brain_kg_node_id_t module_id,
    bool has_anomaly
);

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

/**
 * @brief Register callback for state changes
 *
 * @param hier Hierarchy handle
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success, -1 if max callbacks reached
 */
int kg_hierarchy_register_state_callback(
    kg_hierarchy_t* hier,
    kg_state_change_callback_t callback,
    void* user_data
);

/**
 * @brief Unregister state change callback
 *
 * @param hier Hierarchy handle
 * @param callback Callback to remove
 * @return 0 on success, -1 if not found
 */
int kg_hierarchy_unregister_state_callback(
    kg_hierarchy_t* hier,
    kg_state_change_callback_t callback
);

/* ============================================================================
 * Thread-Safe Access
 * ============================================================================ */

/**
 * @brief Acquire read lock on hierarchy
 *
 * WHAT: Acquire read lock for consistent queries
 * WHY:  Thread-safe access during multi-query operations
 * HOW:  Reader-writer lock pattern
 *
 * @param hier Hierarchy handle
 * @return 0 on success
 */
int kg_hierarchy_read_lock(kg_hierarchy_t* hier);

/**
 * @brief Release read lock on hierarchy
 *
 * @param hier Hierarchy handle
 * @return 0 on success
 */
int kg_hierarchy_read_unlock(kg_hierarchy_t* hier);

/* ============================================================================
 * Rebuild / Sync
 * ============================================================================ */

/**
 * @brief Force rebuild of hierarchy from KG
 *
 * WHAT: Rebuild entire hierarchy from current KG state
 * WHY:  Recovery after major changes or corruption
 * HOW:  Re-scan all KG nodes, rebuild indices
 *
 * @param hier Hierarchy handle
 * @return 0 on success
 */
int kg_hierarchy_rebuild(kg_hierarchy_t* hier);

/**
 * @brief Sync hierarchy from all sources
 *
 * WHAT: Full synchronization from KG, orchestrator, wiring
 * WHY:  Ensure hierarchy reflects current system state
 * HOW:  Query all sources and merge
 *
 * @param hier Hierarchy handle
 * @return 0 on success
 */
int kg_hierarchy_sync_all(kg_hierarchy_t* hier);

/**
 * @brief Invalidate hierarchy cache
 *
 * WHAT: Mark hierarchy as needing rebuild
 * WHY:  Called when underlying data changes
 * HOW:  Sets dirty flag, next query triggers rebuild
 *
 * @param hier Hierarchy handle
 */
void kg_hierarchy_invalidate(kg_hierarchy_t* hier);

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

/**
 * @brief Convert hemisphere to string
 */
const char* kg_hemisphere_to_string(kg_hemisphere_t hemisphere);

/**
 * @brief Convert cortical layer to string
 */
const char* kg_cortical_layer_to_string(kg_cortical_layer_t layer);

/**
 * @brief Convert module state to string
 */
const char* kg_module_state_to_string(kg_module_state_t state);

/**
 * @brief Convert hierarchy level to string
 */
const char* kg_hierarchy_level_to_string(kg_hierarchy_level_t level);

/* ============================================================================
 * Graph Algorithm Integration (nimcp_sort utilities)
 * ============================================================================ */

/**
 * @brief Topological sort result for module startup ordering
 */
typedef struct {
    brain_kg_node_id_t* order;     /**< Sorted module IDs (caller-allocated) */
    uint32_t count;                /**< Number of modules in order */
    bool has_cycle;                /**< True if dependency cycle detected */
    brain_kg_node_id_t* cycle_nodes; /**< Nodes participating in cycle (if any) */
    uint32_t cycle_count;          /**< Number of nodes in cycle */
} kg_topo_sort_result_t;

/**
 * @brief BFS/DFS traversal visitor callback
 *
 * @param module_id Current module being visited
 * @param depth Distance from start node
 * @param user_data User-provided context
 * @return true to continue, false to stop traversal
 */
typedef bool (*kg_traversal_visitor_fn)(
    brain_kg_node_id_t module_id,
    uint32_t depth,
    void* user_data
);

/**
 * @brief Connected component information
 */
typedef struct {
    uint32_t component_id;         /**< Component identifier (0-indexed) */
    brain_kg_node_id_t* modules;   /**< Module IDs in this component */
    uint32_t module_count;         /**< Number of modules */
    kg_hemisphere_t primary_hemisphere; /**< Dominant hemisphere in component */
    bool spans_hemispheres;        /**< True if crosses hemisphere boundary */
} kg_component_info_t;

/**
 * @brief Connected components result
 */
typedef struct {
    kg_component_info_t* components; /**< Array of components */
    uint32_t component_count;        /**< Number of components */
    uint32_t* module_to_component;   /**< Mapping: module index -> component ID */
    uint32_t largest_component;      /**< Index of largest component */
    uint32_t isolated_count;         /**< Number of isolated (single-node) components */
} kg_components_result_t;

/* ============================================================================
 * Topological Sort API
 * ============================================================================ */

/**
 * @brief Get topologically sorted module startup order
 *
 * WHAT: Sort modules so dependencies start before dependents
 * WHY:  Required for correct brain initialization sequence
 * HOW:  Uses Kahn's algorithm via nimcp_topological_sort
 *
 * Modules with DEPENDS_ON edges are sorted such that dependencies
 * appear before the modules that depend on them.
 *
 * @param hier Hierarchy handle
 * @param order Output array for sorted module IDs (caller-allocated)
 * @param max_order Capacity of order array
 * @param sorted_count Output: number of modules sorted
 * @return 0 on success, -1 on error, -2 if cycle detected
 */
int kg_hierarchy_topological_sort(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t* order,
    uint32_t max_order,
    uint32_t* sorted_count
);

/**
 * @brief Check if module dependency graph has cycles
 *
 * @param hier Hierarchy handle
 * @return true if cycles exist, false if acyclic
 */
bool kg_hierarchy_has_dependency_cycle(const kg_hierarchy_t* hier);

/**
 * @brief Find modules participating in dependency cycles
 *
 * @param hier Hierarchy handle
 * @param cycle_modules Output array for cycle module IDs
 * @param max_modules Capacity of output array
 * @param cycle_count Output: number of modules in cycles
 * @return 0 on success
 */
int kg_hierarchy_find_cycle_modules(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t* cycle_modules,
    uint32_t max_modules,
    uint32_t* cycle_count
);

/**
 * @brief Get startup phase for a module based on topological order
 *
 * Modules are grouped into phases where all dependencies of a phase
 * are satisfied by previous phases.
 *
 * @param hier Hierarchy handle
 * @param module_id Module to query
 * @return Phase number (0 = no dependencies), or -1 if not found
 */
int kg_hierarchy_get_startup_phase(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t module_id
);

/* ============================================================================
 * Binary Search / Optimized Lookup API
 * ============================================================================ */

/**
 * @brief Find module index in sorted array using binary search
 *
 * WHAT: O(log n) lookup in sorted module ID array
 * WHY:  Faster than linear scan for large module sets
 * HOW:  Uses nimcp_binary_search_u32
 *
 * @param hier Hierarchy handle
 * @param sorted_ids Sorted array of module IDs
 * @param count Number of IDs in array
 * @param target_id Module ID to find
 * @return Index in array if found, UINT32_MAX if not found
 */
uint32_t kg_hierarchy_binary_search_module(
    const kg_hierarchy_t* hier,
    const brain_kg_node_id_t* sorted_ids,
    uint32_t count,
    brain_kg_node_id_t target_id
);

/**
 * @brief Get sorted array of all module IDs
 *
 * Returns module IDs sorted for efficient binary search.
 *
 * @param hier Hierarchy handle
 * @param sorted_ids Output array (caller-allocated)
 * @param max_ids Capacity of output array
 * @return Number of module IDs written
 */
uint32_t kg_hierarchy_get_sorted_module_ids(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t* sorted_ids,
    uint32_t max_ids
);

/**
 * @brief Check if module ID array is sorted
 *
 * @param ids Array of module IDs
 * @param count Number of IDs
 * @return true if sorted in ascending order
 */
bool kg_hierarchy_is_sorted(
    const brain_kg_node_id_t* ids,
    uint32_t count
);

/* ============================================================================
 * Graph Traversal API (BFS/DFS)
 * ============================================================================ */

/**
 * @brief Breadth-first traversal from a starting module
 *
 * WHAT: Visit modules in BFS order following connections
 * WHY:  Find shortest paths, level-order processing
 * HOW:  Uses nimcp_bfs with hierarchy callbacks
 *
 * @param hier Hierarchy handle
 * @param start_module Starting module ID
 * @param visitor Callback invoked for each visited module
 * @param user_data Context passed to visitor
 * @return 0 on success, -1 on error
 */
int kg_hierarchy_bfs(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t start_module,
    kg_traversal_visitor_fn visitor,
    void* user_data
);

/**
 * @brief Depth-first traversal from a starting module
 *
 * WHAT: Visit modules in DFS order following connections
 * WHY:  Connectivity analysis, path finding
 * HOW:  Uses nimcp_dfs with hierarchy callbacks
 *
 * @param hier Hierarchy handle
 * @param start_module Starting module ID
 * @param visitor Callback invoked for each visited module
 * @param user_data Context passed to visitor
 * @return 0 on success, -1 on error
 */
int kg_hierarchy_dfs(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t start_module,
    kg_traversal_visitor_fn visitor,
    void* user_data
);

/**
 * @brief Find shortest path between two modules
 *
 * Uses BFS to find the shortest connection path.
 *
 * @param hier Hierarchy handle
 * @param from_module Source module
 * @param to_module Target module
 * @param path Output array for path (from -> ... -> to)
 * @param max_path Capacity of path array
 * @param path_length Output: actual path length
 * @return 0 if path found, -1 if no path exists
 */
int kg_hierarchy_shortest_path(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t from_module,
    brain_kg_node_id_t to_module,
    brain_kg_node_id_t* path,
    uint32_t max_path,
    uint32_t* path_length
);

/**
 * @brief Get distance (hop count) between two modules
 *
 * @param hier Hierarchy handle
 * @param from_module Source module
 * @param to_module Target module
 * @return Distance (number of hops), or UINT32_MAX if not connected
 */
uint32_t kg_hierarchy_get_distance(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t from_module,
    brain_kg_node_id_t to_module
);

/**
 * @brief Find all modules reachable from a starting module
 *
 * @param hier Hierarchy handle
 * @param start_module Starting module
 * @param reachable Output array of reachable module IDs
 * @param max_reachable Capacity of output array
 * @return Number of reachable modules
 */
uint32_t kg_hierarchy_get_reachable(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t start_module,
    brain_kg_node_id_t* reachable,
    uint32_t max_reachable
);

/* ============================================================================
 * Connected Components API
 * ============================================================================ */

/**
 * @brief Find all connected components in the module graph
 *
 * WHAT: Group modules into disconnected subgraphs
 * WHY:  Identify isolated module clusters, validate connectivity
 * HOW:  Uses nimcp_find_components with hierarchy callbacks
 *
 * @param hier Hierarchy handle
 * @param component_ids Output: component ID for each module (by index)
 * @param num_components Output: total number of components
 * @return 0 on success
 */
int kg_hierarchy_find_components(
    const kg_hierarchy_t* hier,
    uint32_t* component_ids,
    uint32_t* num_components
);

/**
 * @brief Get detailed information about a specific component
 *
 * @param hier Hierarchy handle
 * @param component_id Component to query
 * @param info Output component info
 * @return 0 on success, -1 if component not found
 */
int kg_hierarchy_get_component_info(
    const kg_hierarchy_t* hier,
    uint32_t component_id,
    kg_component_info_t* info
);

/**
 * @brief Get all modules in a specific component
 *
 * @param hier Hierarchy handle
 * @param component_id Component to query
 * @param modules Output array for module IDs
 * @param max_modules Capacity of output array
 * @return Number of modules in component
 */
uint32_t kg_hierarchy_get_component_modules(
    const kg_hierarchy_t* hier,
    uint32_t component_id,
    brain_kg_node_id_t* modules,
    uint32_t max_modules
);

/**
 * @brief Check if two modules are in the same connected component
 *
 * @param hier Hierarchy handle
 * @param module_a First module
 * @param module_b Second module
 * @return true if connected (same component), false otherwise
 */
bool kg_hierarchy_are_connected(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t module_a,
    brain_kg_node_id_t module_b
);

/**
 * @brief Get the largest connected component
 *
 * @param hier Hierarchy handle
 * @param modules Output array for module IDs in largest component
 * @param max_modules Capacity of output array
 * @return Number of modules in largest component
 */
uint32_t kg_hierarchy_get_largest_component(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t* modules,
    uint32_t max_modules
);

/**
 * @brief Count isolated modules (components of size 1)
 *
 * @param hier Hierarchy handle
 * @return Number of isolated modules
 */
uint32_t kg_hierarchy_count_isolated(const kg_hierarchy_t* hier);

/**
 * @brief Free component result resources
 *
 * @param result Result to free (NULL safe)
 */
void kg_hierarchy_free_components_result(kg_components_result_t* result);

/* ============================================================================
 * Edge Iteration API (for algorithm integration)
 * ============================================================================ */

/**
 * @brief Simple edge structure for algorithm processing
 */
typedef struct {
    brain_kg_node_id_t source;  /**< Source node ID */
    brain_kg_node_id_t target;  /**< Target node ID */
    float weight;               /**< Edge weight */
} kg_edge_t;

/**
 * @brief Edge iterator (opaque)
 */
typedef struct kg_edge_iterator kg_edge_iterator_t;

/**
 * @brief Get total node count in hierarchy
 *
 * @param hier Hierarchy handle
 * @param count Output node count
 * @return 0 on success
 */
int kg_hierarchy_get_node_count(const kg_hierarchy_t* hier, uint32_t* count);

/**
 * @brief Create edge iterator for all edges
 *
 * @param hier Hierarchy handle
 * @return Iterator or NULL on error
 */
kg_edge_iterator_t* kg_hierarchy_edge_iterator(const kg_hierarchy_t* hier);

/**
 * @brief Get next edge from iterator
 *
 * @param iter Iterator handle
 * @param edge Output edge
 * @return 0 on success, -1 when exhausted
 */
int kg_edge_iterator_next(kg_edge_iterator_t* iter, kg_edge_t* edge);

/**
 * @brief Free edge iterator
 *
 * @param iter Iterator to free (NULL safe)
 */
void kg_edge_iterator_free(kg_edge_iterator_t* iter);

/**
 * @brief Set integer metadata on an edge
 *
 * @param hier Hierarchy handle
 * @param from Source node
 * @param to Target node
 * @param key Metadata key
 * @param value Integer value
 * @return 0 on success
 */
int kg_hierarchy_set_edge_metadata_int(
    kg_hierarchy_t* hier,
    brain_kg_node_id_t from,
    brain_kg_node_id_t to,
    const char* key,
    int32_t value
);

/**
 * @brief Get integer metadata from an edge
 *
 * @param hier Hierarchy handle
 * @param from Source node
 * @param to Target node
 * @param key Metadata key
 * @param value Output value
 * @return 0 on success, -1 if not found
 */
int kg_hierarchy_get_edge_metadata_int(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t from,
    brain_kg_node_id_t to,
    const char* key,
    int32_t* value
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KG_HIERARCHY_H */
