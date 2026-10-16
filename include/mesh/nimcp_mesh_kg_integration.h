/**
 * @file nimcp_mesh_kg_integration.h
 * @brief Knowledge Graph and Mesh Topology Integration
 *
 * WHAT: Integrates Knowledge Graph module wiring with mesh network topology
 * WHY:  Synchronize KG module relationships with mesh endorsement and routing paths
 * HOW:  Bi-directional sync between KG topology and mesh fractal topology
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────────┐
 * │                     KG-MESH TOPOLOGY INTEGRATION                            │
 * ├─────────────────────────────────────────────────────────────────────────────┤
 * │                                                                              │
 * │  ┌──────────────────────────────┐    ┌──────────────────────────────┐       │
 * │  │     KNOWLEDGE GRAPH          │    │        MESH NETWORK          │       │
 * │  │  ┌────────────────────────┐  │    │  ┌────────────────────────┐  │       │
 * │  │  │ Module Wiring          │  │<──>│  │ Mesh Topology         │  │       │
 * │  │  │ - Inputs/Outputs       │  │    │  │ - Participant Graph   │  │       │
 * │  │  │ - Handler Priority     │  │    │  │ - Hub Detection       │  │       │
 * │  │  │ - Connection Weights   │  │    │  │ - Centrality Scores   │  │       │
 * │  │  └────────────────────────┘  │    │  └────────────────────────┘  │       │
 * │  │                              │    │                              │       │
 * │  │  ┌────────────────────────┐  │    │  ┌────────────────────────┐  │       │
 * │  │  │ Module Registry        │  │<──>│  │ Endorsement Paths     │  │       │
 * │  │  │ - Module Discovery     │  │    │  │ - Required Endorsers  │  │       │
 * │  │  │ - Hierarchy            │  │    │  │ - Veto Authority      │  │       │
 * │  │  │ - Dependencies         │  │    │  │ - Policy Mapping      │  │       │
 * │  │  └────────────────────────┘  │    │  └────────────────────────┘  │       │
 * │  │                              │    │                              │       │
 * │  │  ┌────────────────────────┐  │    │  ┌────────────────────────┐  │       │
 * │  │  │ KG Query Engine        │  │───>│  │ Mesh Channel Routing  │  │       │
 * │  │  │ - Find modules by type │  │    │  │ - Route updates       │  │       │
 * │  │  │ - Traverse connections │  │    │  │ - Propagate changes   │  │       │
 * │  │  └────────────────────────┘  │    │  └────────────────────────┘  │       │
 * │  └──────────────────────────────┘    └──────────────────────────────┘       │
 * │                                                                              │
 * └─────────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * BIOLOGICAL MOTIVATION:
 * - Brain regions form hierarchical processing networks
 * - Module connectivity mirrors synaptic wiring patterns
 * - Hub neurons are critical for integration (like coordinators)
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_KG_INTEGRATION_H
#define NIMCP_MESH_KG_INTEGRATION_H

#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_topology.h"
#include "utils/error/nimcp_error_codes.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct mesh_kg_integration mesh_kg_integration_t;
typedef struct mesh_bootstrap mesh_bootstrap_t;
typedef struct mesh_integration mesh_integration_t;
typedef struct kg_module_wiring kg_module_wiring_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Magic number for structure validation */
#define MESH_KG_INTEGRATION_MAGIC       0x4B474D53  /* "KGMS" */

/** @brief Maximum modules tracked in KG integration */
#define MESH_KG_MAX_MODULES             2048

/** @brief Maximum hub nodes for coordinator placement */
#define MESH_KG_MAX_HUBS                64

/** @brief Maximum endorsement path length */
#define MESH_KG_MAX_PATH_LEN            16

/** @brief Maximum KG update batch size */
#define MESH_KG_UPDATE_BATCH_SIZE       64

/* ============================================================================
 * Synchronization Mode
 * ============================================================================ */

/**
 * @brief KG-Mesh synchronization mode
 */
typedef enum mesh_kg_sync_mode {
    MESH_KG_SYNC_DISABLED = 0,          /**< No synchronization */
    MESH_KG_SYNC_KG_TO_MESH,            /**< KG topology drives mesh */
    MESH_KG_SYNC_MESH_TO_KG,            /**< Mesh topology drives KG */
    MESH_KG_SYNC_BIDIRECTIONAL,         /**< Full bidirectional sync */
} mesh_kg_sync_mode_t;

/**
 * @brief Module discovery source
 */
typedef enum mesh_kg_discovery_source {
    MESH_KG_DISCOVER_NONE = 0,          /**< No discovery */
    MESH_KG_DISCOVER_KG_ONLY,           /**< Discover from KG wiring */
    MESH_KG_DISCOVER_MESH_ONLY,         /**< Discover from mesh participants */
    MESH_KG_DISCOVER_BOTH,              /**< Merge both sources */
} mesh_kg_discovery_source_t;

/* ============================================================================
 * Module Mapping Entry
 * ============================================================================ */

/**
 * @brief Mapping between KG module wiring and mesh participant
 */
typedef struct mesh_kg_module_mapping {
    char module_name[MESH_MAX_NAME_LEN];    /**< Module name from KG */
    char module_type[32];                   /**< Module type */
    mesh_participant_id_t participant_id;   /**< Mesh participant ID */
    mesh_channel_id_t primary_channel;      /**< Primary channel assignment */

    /* Topology metrics */
    bool is_hub;                            /**< Identified as topology hub */
    float betweenness_centrality;           /**< Centrality score */
    uint32_t connection_count;              /**< Number of connections */

    /* KG wiring info */
    uint32_t input_count;                   /**< KG inputs */
    uint32_t output_count;                  /**< KG outputs */
    uint32_t handler_count;                 /**< KG handlers */

    /* Endorsement role */
    bool is_endorser;                       /**< Registered as endorser */
    bool is_required_endorser;              /**< Required for policies */
    bool has_veto;                          /**< Has veto authority */

    /* Status */
    bool active;                            /**< Currently active */
    uint64_t last_sync_ns;                  /**< Last synchronization time */
} mesh_kg_module_mapping_t;

/**
 * @brief Hub assignment for coordinator placement
 */
typedef struct mesh_kg_hub_assignment {
    mesh_participant_id_t hub_id;           /**< Hub participant */
    mesh_channel_id_t channel;              /**< Assigned channel */
    mesh_pool_id_t coordinator_pool;        /**< Target coordinator pool */
    float centrality;                       /**< Centrality score */
    uint32_t assigned_modules;              /**< Modules routed through hub */
} mesh_kg_hub_assignment_t;

/**
 * @brief Endorsement path derived from KG relationships
 */
typedef struct mesh_kg_endorsement_path {
    char policy_name[64];                   /**< Policy name */
    mesh_tx_type_t tx_type;                 /**< Transaction type */
    mesh_participant_id_t endorsers[MESH_KG_MAX_PATH_LEN];
    size_t endorser_count;                  /**< Number of endorsers */
    mesh_participant_id_t required[MESH_KG_MAX_PATH_LEN];
    size_t required_count;                  /**< Required endorsers */
    mesh_participant_id_t veto_holders[8];
    size_t veto_count;                      /**< Veto authorities */
} mesh_kg_endorsement_path_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief KG integration configuration
 */
typedef struct mesh_kg_integration_config {
    /* Synchronization mode */
    mesh_kg_sync_mode_t sync_mode;          /**< Sync direction */
    mesh_kg_discovery_source_t discovery;   /**< Discovery source */

    /* Sync behavior */
    bool auto_sync_on_change;               /**< Auto-sync on changes */
    uint64_t sync_interval_ms;              /**< Periodic sync interval */
    bool propagate_updates_through_mesh;    /**< Route KG updates via mesh */

    /* Hub identification */
    float hub_centrality_threshold;         /**< Min centrality for hub */
    float hub_degree_percentile;            /**< Degree percentile for hubs */

    /* Endorsement mapping */
    bool derive_endorsers_from_kg;          /**< Map KG inputs to endorsers */
    bool map_handlers_to_policies;          /**< Map handlers to policies */

    /* Logging */
    bool verbose_logging;
} mesh_kg_integration_config_t;

/**
 * @brief KG integration statistics
 */
typedef struct mesh_kg_integration_stats {
    uint64_t modules_synced;                /**< Total modules synchronized */
    uint64_t hubs_identified;               /**< Topology hubs found */
    uint64_t endorsement_paths_created;     /**< Endorsement paths from KG */
    uint64_t kg_updates_routed;             /**< KG updates sent via mesh */
    uint64_t mesh_updates_to_kg;            /**< Mesh changes to KG */
    uint64_t sync_operations;               /**< Total sync operations */
    uint64_t sync_failures;                 /**< Sync failures */
    uint64_t last_sync_ns;                  /**< Last sync timestamp */
    float avg_sync_duration_ms;             /**< Average sync duration */
} mesh_kg_integration_stats_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default KG integration configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_kg_integration_default_config(
    mesh_kg_integration_config_t* config
);

/**
 * @brief Create KG-Mesh integration context
 *
 * WHAT: Initialize bidirectional KG-mesh synchronization
 * WHY:  Align KG module topology with mesh network structure
 * HOW:  Create mapping registry, initialize sync mechanisms
 *
 * @param bootstrap Mesh bootstrap handle
 * @param config Configuration (NULL for defaults)
 * @return Integration handle or NULL on failure
 */
mesh_kg_integration_t* mesh_kg_integration_create(
    mesh_bootstrap_t* bootstrap,
    const mesh_kg_integration_config_t* config
);

/**
 * @brief Destroy KG integration context
 *
 * @param integration Integration to destroy (NULL-safe)
 */
void mesh_kg_integration_destroy(mesh_kg_integration_t* integration);

/* ============================================================================
 * Module Registration API
 * ============================================================================ */

/**
 * @brief Register module wiring from KG
 *
 * WHAT: Add KG module wiring to integration for mesh synchronization
 * WHY:  Track KG modules and their mesh participant mappings
 * HOW:  Create mapping entry, optionally register in mesh
 *
 * @param integration Integration handle
 * @param wiring KG module wiring descriptor
 * @return Assigned participant ID or 0 on failure
 */
mesh_participant_id_t mesh_kg_integration_register_wiring(
    mesh_kg_integration_t* integration,
    const kg_module_wiring_t* wiring
);

/**
 * @brief Unregister module from integration
 *
 * @param integration Integration handle
 * @param module_name Module name
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_kg_integration_unregister_module(
    mesh_kg_integration_t* integration,
    const char* module_name
);

/**
 * @brief Get module mapping by name
 *
 * @param integration Integration handle
 * @param module_name Module name
 * @param mapping Output mapping
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_kg_integration_get_mapping(
    const mesh_kg_integration_t* integration,
    const char* module_name,
    mesh_kg_module_mapping_t* mapping
);

/**
 * @brief Get module mapping by participant ID
 *
 * @param integration Integration handle
 * @param participant_id Participant ID
 * @param mapping Output mapping
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_kg_integration_get_mapping_by_id(
    const mesh_kg_integration_t* integration,
    mesh_participant_id_t participant_id,
    mesh_kg_module_mapping_t* mapping
);

/* ============================================================================
 * Topology Synchronization API
 * ============================================================================ */

/**
 * @brief Synchronize KG topology to mesh topology
 *
 * WHAT: Transfer KG module connections to mesh participant graph
 * WHY:  Mesh topology should reflect KG wiring patterns
 * HOW:  Extract connections from KG, add to mesh topology context
 *
 * @param integration Integration handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_kg_integration_sync_kg_to_mesh(
    mesh_kg_integration_t* integration
);

/**
 * @brief Synchronize mesh topology to KG
 *
 * WHAT: Update KG with discovered mesh relationships
 * WHY:  KG should be aware of runtime mesh connections
 * HOW:  Query mesh topology, update KG wiring entries
 *
 * @param integration Integration handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_kg_integration_sync_mesh_to_kg(
    mesh_kg_integration_t* integration
);

/**
 * @brief Perform full bidirectional sync
 *
 * @param integration Integration handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_kg_integration_sync_all(
    mesh_kg_integration_t* integration
);

/**
 * @brief Identify hub modules from KG topology
 *
 * WHAT: Find high-centrality modules that should be coordinators
 * WHY:  Hub nodes are optimal for coordinator placement
 * HOW:  Analyze KG connections, compute centrality, rank
 *
 * @param integration Integration handle
 * @param hubs Output hub assignments (caller allocates)
 * @param max_hubs Maximum hubs to return
 * @param hub_count Output: actual hub count
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_kg_integration_identify_hubs(
    mesh_kg_integration_t* integration,
    mesh_kg_hub_assignment_t* hubs,
    size_t max_hubs,
    size_t* hub_count
);

/* ============================================================================
 * KG Query Routing API
 * ============================================================================ */

/**
 * @brief Route KG query through mesh channel
 *
 * WHAT: Submit KG topology query as mesh transaction
 * WHY:  Distributed KG queries benefit from mesh routing
 * HOW:  Create query transaction, submit to channel
 *
 * @param integration Integration handle
 * @param query_type Type of KG query
 * @param query_data Query parameters
 * @param query_size Query data size
 * @param channel Target channel
 * @return Transaction ID on success or empty struct on failure
 */
mesh_tx_id_t mesh_kg_integration_route_query(
    mesh_kg_integration_t* integration,
    uint32_t query_type,
    const void* query_data,
    size_t query_size,
    mesh_channel_id_t channel
);

/**
 * @brief Route KG update through mesh
 *
 * WHAT: Propagate KG wiring changes via mesh transactions
 * WHY:  Ensure distributed modules receive KG updates
 * HOW:  Create update transaction, collect endorsements, commit
 *
 * @param integration Integration handle
 * @param module_name Updated module
 * @param update_type Type of update
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_kg_integration_route_update(
    mesh_kg_integration_t* integration,
    const char* module_name,
    uint32_t update_type
);

/* ============================================================================
 * Endorsement Path API
 * ============================================================================ */

/**
 * @brief Derive endorsement path from KG relationships
 *
 * WHAT: Extract endorsement requirements from KG module wiring
 * WHY:  KG inputs indicate which modules should endorse
 * HOW:  Analyze required inputs, map to endorsement policy
 *
 * @param integration Integration handle
 * @param module_name Target module
 * @param tx_type Transaction type
 * @param path Output endorsement path
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_kg_integration_derive_endorsement_path(
    mesh_kg_integration_t* integration,
    const char* module_name,
    mesh_tx_type_t tx_type,
    mesh_kg_endorsement_path_t* path
);

/**
 * @brief Sync endorsement policies with KG wiring
 *
 * WHAT: Update mesh endorsement policies based on KG
 * WHY:  Endorsement requirements should match module dependencies
 * HOW:  For each transaction type, derive endorsers from KG
 *
 * @param integration Integration handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_kg_integration_sync_endorsement_policies(
    mesh_kg_integration_t* integration
);

/* ============================================================================
 * Module Discovery API
 * ============================================================================ */

/**
 * @brief Discover modules from KG wiring
 *
 * WHAT: Find all modules registered in KG
 * WHY:  Build complete module inventory
 * HOW:  Query KG registry, return module list
 *
 * @param integration Integration handle
 * @param modules Output module names (caller allocates)
 * @param max_modules Maximum modules
 * @param module_count Output: actual count
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_kg_integration_discover_kg_modules(
    mesh_kg_integration_t* integration,
    char (*modules)[MESH_MAX_NAME_LEN],
    size_t max_modules,
    size_t* module_count
);

/**
 * @brief Discover modules from mesh participants
 *
 * WHAT: Find all mesh participants and their module names
 * WHY:  Build inventory from mesh perspective
 * HOW:  Query participant registry, extract names
 *
 * @param integration Integration handle
 * @param modules Output module names (caller allocates)
 * @param max_modules Maximum modules
 * @param module_count Output: actual count
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_kg_integration_discover_mesh_modules(
    mesh_kg_integration_t* integration,
    char (*modules)[MESH_MAX_NAME_LEN],
    size_t max_modules,
    size_t* module_count
);

/**
 * @brief Find module by type in KG
 *
 * @param integration Integration handle
 * @param module_type Module type to search
 * @param modules Output module names
 * @param max_modules Maximum modules
 * @param module_count Output: actual count
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_kg_integration_find_by_type(
    mesh_kg_integration_t* integration,
    const char* module_type,
    char (*modules)[MESH_MAX_NAME_LEN],
    size_t max_modules,
    size_t* module_count
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get integration statistics
 *
 * @param integration Integration handle
 * @param stats Output statistics
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_kg_integration_get_stats(
    const mesh_kg_integration_t* integration,
    mesh_kg_integration_stats_t* stats
);

/**
 * @brief Reset integration statistics
 *
 * @param integration Integration handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_kg_integration_reset_stats(
    mesh_kg_integration_t* integration
);

/* ============================================================================
 * Bootstrap Integration
 * ============================================================================ */

/**
 * @brief Get KG integration from bootstrap
 *
 * @param bootstrap Bootstrap handle
 * @return KG integration or NULL
 */
mesh_kg_integration_t* mesh_bootstrap_get_kg_integration(
    mesh_bootstrap_t* bootstrap
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Convert sync mode to string
 *
 * @param mode Sync mode
 * @return Mode string
 */
const char* mesh_kg_sync_mode_to_string(mesh_kg_sync_mode_t mode);

/**
 * @brief Convert discovery source to string
 *
 * @param source Discovery source
 * @return Source string
 */
const char* mesh_kg_discovery_source_to_string(mesh_kg_discovery_source_t source);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_KG_INTEGRATION_H */
