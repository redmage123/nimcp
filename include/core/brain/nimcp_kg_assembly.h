/**
 * @file nimcp_kg_assembly.h
 * @brief Hierarchical Assembly for Brain Knowledge Graph Wiring
 * @version 1.0.0
 * @date 2025-01-16
 *
 * WHAT: Hierarchical assembly of brain wiring from module -> layer -> hemisphere -> brain
 * WHY:  Brain needs to assemble module wirings into a complete wiring diagram for KG
 * HOW:  Bottom-up assembly API that collects module wirings and converts to KG representation
 *
 * ASSEMBLY HIERARCHY:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    BRAIN WIRING ASSEMBLY HIERARCHY                         ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   Level 3: Brain Wiring (kg_brain_wiring_t)                               ║
 * ║   ────────────────────────────────────────                                 ║
 * ║   ┌─────────────────────────────────────────────────────────────────────┐ ║
 * ║   │  - Left & Right hemispheres                                         │ ║
 * ║   │  - Corpus callosum (inter-hemispheric connections)                  │ ║
 * ║   │  - System-wide metadata & search index                              │ ║
 * ║   └─────────────────────────────────────────────────────────────────────┘ ║
 * ║                                   │                                        ║
 * ║              ┌────────────────────┴────────────────────┐                  ║
 * ║              ▼                                         ▼                  ║
 * ║   Level 2: Hemisphere Wiring (kg_hemisphere_wiring_t)                     ║
 * ║   ───────────────────────────────────────────────────                     ║
 * ║   ┌─────────────────────────┐     ┌─────────────────────────┐            ║
 * ║   │   LEFT HEMISPHERE        │     │   RIGHT HEMISPHERE       │            ║
 * ║   │   - 6 cortical layers    │     │   - 6 cortical layers    │            ║
 * ║   │   - Feedforward paths    │     │   - Feedforward paths    │            ║
 * ║   │   - Feedback paths       │     │   - Feedback paths       │            ║
 * ║   └───────────┬─────────────┘     └───────────┬─────────────┘            ║
 * ║               │                               │                           ║
 * ║               ▼                               ▼                           ║
 * ║   Level 1: Layer Wiring (kg_layer_wiring_t)                               ║
 * ║   ──────────────────────────────────────────                              ║
 * ║   ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐║
 * ║   │Layer I  │ │Layer II │ │Layer III│ │Layer IV │ │Layer V  │ │Layer VI │║
 * ║   │- Modules│ │- Modules│ │- Modules│ │- Modules│ │- Modules│ │- Modules│║
 * ║   │- Intra  │ │- Intra  │ │- Intra  │ │- Intra  │ │- Intra  │ │- Intra  │║
 * ║   │- Cross  │ │- Cross  │ │- Cross  │ │- Cross  │ │- Cross  │ │- Cross  │║
 * ║   └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘║
 * ║        │           │           │           │           │           │      ║
 * ║                                   ▼                                       ║
 * ║   Level 0: Module Wiring (kg_module_wiring_t)                             ║
 * ║   ───────────────────────────────────────────                             ║
 * ║   Individual module wiring descriptors with inputs, outputs, handlers     ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * USAGE:
 * ```c
 * // Create layer and add modules
 * kg_layer_wiring_t* layer_iv = kg_assembly_create_layer(4);  // Layer IV
 * kg_assembly_add_module_to_layer(layer_iv, visual_module_wiring);
 * kg_assembly_add_module_to_layer(layer_iv, audio_module_wiring);
 * kg_assembly_finalize_layer(layer_iv);
 *
 * // Create hemisphere and add layers
 * kg_hemisphere_wiring_t* left = kg_assembly_create_hemisphere(0);  // LEFT
 * kg_assembly_add_layer_to_hemisphere(left, layer_iv);
 * // ... add other layers
 * kg_assembly_finalize_hemisphere(left);
 *
 * // Create brain and add hemispheres
 * kg_brain_wiring_t* brain = kg_assembly_create_brain();
 * kg_assembly_add_hemisphere_to_brain(brain, left);
 * kg_assembly_add_hemisphere_to_brain(brain, right);
 * kg_assembly_finalize_brain(brain);
 *
 * // Convert to KG
 * kg_assembly_write_to_kg(brain, kg);
 * ```
 *
 * BIOLOGICAL BASIS:
 * - 6-layer cortical organization mirrors mammalian neocortex
 * - Feedforward (IV->II/III->V) and feedback (V->II/III->IV) pathways
 * - Corpus callosum for inter-hemispheric communication
 * - Layer-specific functions (IV: input, V: output, VI: feedback)
 *
 * THREAD SAFETY: Assembly functions are NOT thread-safe. Assembly should be
 * performed during brain initialization before multi-threaded operation.
 *
 * @see nimcp_kg_module_wiring.h for module-level wiring descriptors
 * @see nimcp_kg_hierarchy.h for runtime hierarchical view
 * @see nimcp_brain_kg.h for the target knowledge graph
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_KG_ASSEMBLY_H
#define NIMCP_KG_ASSEMBLY_H

#include "core/brain/nimcp_brain_kg.h"
#include "core/brain/nimcp_kg_module_wiring.h"
#include "core/brain/nimcp_kg_search.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Number of cortical layers (I-VI) */
#define KG_ASSEMBLY_LAYER_COUNT         6

/** Maximum modules per layer */
#define KG_ASSEMBLY_MAX_MODULES_PER_LAYER   256

/** Maximum internal edges per layer */
#define KG_ASSEMBLY_MAX_INTERNAL_EDGES  512

/** Maximum external (cross-layer) edges per layer */
#define KG_ASSEMBLY_MAX_EXTERNAL_EDGES  256

/** Maximum corpus callosum connections */
#define KG_ASSEMBLY_MAX_CALLOSAL_CONN   1024

/** Hemisphere identifiers */
#define KG_ASSEMBLY_HEMISPHERE_LEFT     0
#define KG_ASSEMBLY_HEMISPHERE_RIGHT    1

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/* NOTE: kg_module_wiring_t, kg_layer_metadata_t, kg_hemisphere_metadata_t,
 * kg_system_metadata_t, and kg_search_index_t are now defined in
 * nimcp_kg_module_wiring.h, nimcp_kg_metadata.h, and nimcp_kg_search.h
 * which are included above. */

/* ============================================================================
 * Internal Edge Structure
 * ============================================================================ */

/**
 * @brief Intra-layer edge (connection within same layer)
 *
 * Represents a connection between two modules in the same cortical layer.
 */
typedef struct {
    brain_kg_node_id_t from;         /**< Source module node ID */
    brain_kg_node_id_t to;           /**< Target module node ID */
    char edge_type[32];              /**< Edge type (e.g., "lateral_inhibition") */
} kg_internal_edge_t;

/**
 * @brief Cross-layer edge (connection to different layer)
 *
 * Represents a feedforward or feedback connection between layers.
 */
typedef struct {
    brain_kg_node_id_t from;         /**< Source module node ID */
    uint8_t target_layer;            /**< Target layer index (0-5 for I-VI) */
    brain_kg_node_id_t to;           /**< Target module node ID */
    char edge_type[32];              /**< Edge type (e.g., "feedforward", "feedback") */
} kg_external_edge_t;

/**
 * @brief Corpus callosum connection (inter-hemispheric)
 *
 * Represents a connection between left and right hemisphere modules.
 */
typedef struct {
    brain_kg_node_id_t left_node;    /**< Module in left hemisphere */
    brain_kg_node_id_t right_node;   /**< Module in right hemisphere */
    float bandwidth;                 /**< Connection strength [0.0-1.0] */
} kg_callosal_connection_t;

/* ============================================================================
 * Layer Wiring Structure
 * ============================================================================ */

/**
 * @brief Layer wiring - assembled from module wirings
 *
 * WHAT: Aggregates all module wirings for a single cortical layer
 * WHY:  Provides layer-level view of wiring for hemisphere assembly
 * HOW:  Collects modules, intra-layer edges, cross-layer edges
 *
 * Each layer contains:
 * - Array of pointers to module wirings
 * - Intra-layer connections (lateral connections within layer)
 * - Cross-layer connections (feedforward/feedback to other layers)
 * - Searchable metadata for the layer
 */
typedef struct {
    uint8_t layer_index;             /**< Layer index (0-5 for I-VI) */

    /** Modules in this layer */
    kg_module_wiring_t** modules;    /**< Array of module wiring pointers */
    uint32_t module_count;           /**< Number of modules */
    uint32_t module_capacity;        /**< Allocated capacity */

    /** Full searchable metadata for this layer */
    kg_layer_metadata_t* metadata;   /**< Layer metadata (NULL until finalized) */

    /** Intra-layer connections (within same layer) */
    kg_internal_edge_t* internal_edges;  /**< Array of internal edges */
    uint32_t internal_edge_count;    /**< Number of internal edges */
    uint32_t internal_edge_capacity; /**< Allocated capacity */

    /** Cross-layer connections (to other layers) */
    kg_external_edge_t* external_edges;  /**< Array of external edges */
    uint32_t external_edge_count;    /**< Number of external edges */
    uint32_t external_edge_capacity; /**< Allocated capacity */

    /** Assembly state */
    bool finalized;                  /**< True if layer has been finalized */
} kg_layer_wiring_t;

/* ============================================================================
 * Hemisphere Wiring Structure
 * ============================================================================ */

/**
 * @brief Hemisphere wiring - assembled from layer wirings
 *
 * WHAT: Aggregates all 6 cortical layers for one hemisphere
 * WHY:  Provides hemisphere-level view for brain assembly
 * HOW:  Contains 6 layers with inter-layer connection statistics
 *
 * Inter-layer connectivity types:
 * - Feedforward: IV -> II/III -> V (bottom-up sensory processing)
 * - Feedback: V -> II/III -> IV (top-down modulation)
 * - Lateral: Within same layer (local processing, inhibition)
 */
typedef struct {
    uint8_t hemisphere;              /**< Hemisphere ID (LEFT=0, RIGHT=1) */

    /** All 6 cortical layers (indices 0-5 for Layers I-VI) */
    kg_layer_wiring_t layers[KG_ASSEMBLY_LAYER_COUNT];

    /** Full searchable metadata for this hemisphere */
    kg_hemisphere_metadata_t* metadata;  /**< Hemisphere metadata */

    /** Inter-layer connection statistics */
    uint32_t feedforward_count;      /**< Feedforward connections (IV->II/III->V) */
    uint32_t feedback_count;         /**< Feedback connections (V->II/III->IV) */
    uint32_t lateral_count;          /**< Lateral connections (within layers) */

    /** Total module count across all layers */
    uint32_t total_modules;          /**< Sum of modules in all layers */

    /** Assembly state */
    bool finalized;                  /**< True if hemisphere has been finalized */
} kg_hemisphere_wiring_t;

/* ============================================================================
 * Brain Wiring Structure
 * ============================================================================ */

/**
 * @brief Complete brain wiring - assembled from hemisphere wirings
 *
 * WHAT: Complete wiring diagram for entire brain
 * WHY:  Final assembly that can be written to the knowledge graph
 * HOW:  Combines both hemispheres with corpus callosum connections
 *
 * The brain wiring includes:
 * - Left hemisphere (all 6 layers)
 * - Right hemisphere (all 6 layers)
 * - Corpus callosum (inter-hemispheric connections)
 * - System-wide metadata and search index
 */
typedef struct {
    /** Both hemispheres */
    kg_hemisphere_wiring_t left;     /**< Left hemisphere wiring */
    kg_hemisphere_wiring_t right;    /**< Right hemisphere wiring */

    /** Full searchable metadata for the entire system */
    kg_system_metadata_t* metadata;  /**< System metadata */

    /** Search index for all metadata */
    kg_search_index_t* search_index; /**< Cross-hierarchy search index */

    /** Corpus callosum connections (inter-hemispheric) */
    kg_callosal_connection_t* callosal_connections;  /**< Array of callosal connections */
    uint32_t callosal_count;         /**< Number of callosal connections */
    uint32_t callosal_capacity;      /**< Allocated capacity */

    /** Assembly metadata */
    uint64_t assembly_timestamp;     /**< When assembly was completed */
    uint64_t version;                /**< Assembly version number */
    uint32_t total_modules;          /**< Total modules in brain */
    uint32_t total_connections;      /**< Total edges in brain */

    /** Assembly state */
    bool finalized;                  /**< True if brain has been finalized */
} kg_brain_wiring_t;

/* ============================================================================
 * Layer Assembly API
 * ============================================================================ */

/**
 * @brief Create a new layer wiring structure
 *
 * WHAT: Allocate and initialize a layer wiring for assembly
 * WHY:  First step in bottom-up wiring assembly
 * HOW:  Allocates arrays, sets layer index, initializes to empty
 *
 * @param layer_index Layer index (0-5 for Layers I-VI)
 * @return Allocated layer wiring or NULL on error
 *
 * @note Caller owns the returned structure. Use kg_assembly_destroy_layer()
 *       to free, or let kg_assembly_finalize_hemisphere() take ownership.
 */
kg_layer_wiring_t* kg_assembly_create_layer(uint8_t layer_index);

/**
 * @brief Add a module wiring to a layer
 *
 * WHAT: Register a module's wiring descriptor with its assigned layer
 * WHY:  Collect all modules that belong to a cortical layer
 * HOW:  Adds pointer to modules array, does NOT copy the wiring
 *
 * @param layer Layer wiring to add to
 * @param module Module wiring descriptor to add
 * @return 0 on success, -1 on error
 *
 * @note The layer does NOT take ownership of the module wiring.
 * @note Must not be called after kg_assembly_finalize_layer().
 */
int kg_assembly_add_module_to_layer(
    kg_layer_wiring_t* layer,
    kg_module_wiring_t* module
);

/**
 * @brief Add an intra-layer edge to a layer
 *
 * WHAT: Register a connection between two modules in the same layer
 * WHY:  Capture lateral connections for layer assembly
 * HOW:  Adds edge to internal_edges array
 *
 * @param layer Layer wiring to add to
 * @param from Source module node ID
 * @param to Target module node ID
 * @param edge_type Edge type description (e.g., "lateral_inhibition")
 * @return 0 on success, -1 on error
 */
int kg_assembly_add_internal_edge(
    kg_layer_wiring_t* layer,
    brain_kg_node_id_t from,
    brain_kg_node_id_t to,
    const char* edge_type
);

/**
 * @brief Add a cross-layer edge to a layer
 *
 * WHAT: Register a connection from this layer to another layer
 * WHY:  Capture feedforward/feedback connections
 * HOW:  Adds edge to external_edges array
 *
 * @param layer Layer wiring to add to
 * @param from Source module node ID (in this layer)
 * @param target_layer Target layer index (0-5)
 * @param to Target module node ID
 * @param edge_type Edge type description (e.g., "feedforward", "feedback")
 * @return 0 on success, -1 on error
 */
int kg_assembly_add_external_edge(
    kg_layer_wiring_t* layer,
    brain_kg_node_id_t from,
    uint8_t target_layer,
    brain_kg_node_id_t to,
    const char* edge_type
);

/**
 * @brief Finalize layer assembly
 *
 * WHAT: Complete layer assembly and generate metadata
 * WHY:  Prepares layer for hemisphere assembly
 * HOW:  Validates, computes statistics, generates metadata
 *
 * After finalization:
 * - No more modules or edges can be added
 * - Layer metadata is populated
 * - Layer is ready for hemisphere assembly
 *
 * @param layer Layer wiring to finalize
 * @return 0 on success, -1 on error
 */
int kg_assembly_finalize_layer(kg_layer_wiring_t* layer);

/**
 * @brief Destroy layer wiring structure
 *
 * WHAT: Free layer wiring and associated resources
 * WHY:  Cleanup after error or when layer is no longer needed
 * HOW:  Frees arrays, metadata, does NOT free module wirings
 *
 * @param layer Layer wiring to destroy (NULL safe)
 *
 * @note Does NOT free the module wiring pointers - those are owned externally.
 */
void kg_assembly_destroy_layer(kg_layer_wiring_t* layer);

/* ============================================================================
 * Hemisphere Assembly API
 * ============================================================================ */

/**
 * @brief Create a new hemisphere wiring structure
 *
 * WHAT: Allocate and initialize a hemisphere wiring for assembly
 * WHY:  Container for 6 cortical layers
 * HOW:  Allocates structure, initializes layers, sets hemisphere ID
 *
 * @param hemisphere Hemisphere ID (KG_ASSEMBLY_HEMISPHERE_LEFT or RIGHT)
 * @return Allocated hemisphere wiring or NULL on error
 *
 * @note Caller owns the returned structure. Use kg_assembly_destroy_hemisphere()
 *       to free, or let kg_assembly_finalize_brain() take ownership.
 */
kg_hemisphere_wiring_t* kg_assembly_create_hemisphere(uint8_t hemisphere);

/**
 * @brief Add a layer wiring to a hemisphere
 *
 * WHAT: Register a finalized layer with its parent hemisphere
 * WHY:  Build up hemisphere from its 6 layers
 * HOW:  Copies layer wiring into hemisphere's layer array
 *
 * @param hemi Hemisphere wiring to add to
 * @param layer Finalized layer wiring to add
 * @return 0 on success, -1 on error
 *
 * @note The hemisphere COPIES the layer wiring (shallow copy of pointers).
 * @note Layer must be finalized before adding.
 * @note Layer index determines slot in hemisphere's layers array.
 */
int kg_assembly_add_layer_to_hemisphere(
    kg_hemisphere_wiring_t* hemi,
    kg_layer_wiring_t* layer
);

/**
 * @brief Finalize hemisphere assembly
 *
 * WHAT: Complete hemisphere assembly and generate metadata
 * WHY:  Prepares hemisphere for brain assembly
 * HOW:  Validates, computes inter-layer statistics, generates metadata
 *
 * After finalization:
 * - No more layers can be added
 * - Inter-layer connection counts are computed
 * - Hemisphere metadata is populated
 * - Hemisphere is ready for brain assembly
 *
 * @param hemi Hemisphere wiring to finalize
 * @return 0 on success, -1 on error
 */
int kg_assembly_finalize_hemisphere(kg_hemisphere_wiring_t* hemi);

/**
 * @brief Destroy hemisphere wiring structure
 *
 * WHAT: Free hemisphere wiring and associated resources
 * WHY:  Cleanup after error or when hemisphere is no longer needed
 * HOW:  Frees all layers, metadata, does NOT free module wirings
 *
 * @param hemi Hemisphere wiring to destroy (NULL safe)
 */
void kg_assembly_destroy_hemisphere(kg_hemisphere_wiring_t* hemi);

/* ============================================================================
 * Brain Assembly API
 * ============================================================================ */

/**
 * @brief Create a new brain wiring structure
 *
 * WHAT: Allocate and initialize a brain wiring for assembly
 * WHY:  Top-level container for complete brain wiring
 * HOW:  Allocates structure, initializes hemispheres, prepares for assembly
 *
 * @return Allocated brain wiring or NULL on error
 *
 * @note Caller owns the returned structure. Use kg_assembly_destroy_brain() to free.
 */
kg_brain_wiring_t* kg_assembly_create_brain(void);

/**
 * @brief Add a hemisphere wiring to the brain
 *
 * WHAT: Register a finalized hemisphere with the brain
 * WHY:  Build complete brain from left and right hemispheres
 * HOW:  Copies hemisphere wiring into brain's left or right slot
 *
 * @param brain Brain wiring to add to
 * @param hemi Finalized hemisphere wiring to add
 * @return 0 on success, -1 on error
 *
 * @note The brain COPIES the hemisphere wiring (shallow copy).
 * @note Hemisphere must be finalized before adding.
 * @note Hemisphere ID determines which slot (left or right).
 */
int kg_assembly_add_hemisphere_to_brain(
    kg_brain_wiring_t* brain,
    kg_hemisphere_wiring_t* hemi
);

/**
 * @brief Add a corpus callosum connection
 *
 * WHAT: Register an inter-hemispheric connection
 * WHY:  Capture cross-hemisphere communication pathways
 * HOW:  Adds connection to callosal_connections array
 *
 * @param brain Brain wiring to add to
 * @param left_node Module node ID in left hemisphere
 * @param right_node Module node ID in right hemisphere
 * @param bandwidth Connection strength [0.0-1.0]
 * @return 0 on success, -1 on error
 */
int kg_assembly_add_callosal_connection(
    kg_brain_wiring_t* brain,
    brain_kg_node_id_t left_node,
    brain_kg_node_id_t right_node,
    float bandwidth
);

/**
 * @brief Finalize brain assembly
 *
 * WHAT: Complete brain assembly and generate metadata
 * WHY:  Prepares brain wiring for conversion to KG
 * HOW:  Validates, computes totals, generates metadata, builds search index
 *
 * After finalization:
 * - No more hemispheres or connections can be added
 * - Total module and connection counts are computed
 * - System metadata is populated
 * - Search index is built
 * - Brain wiring is ready for KG conversion
 *
 * @param brain Brain wiring to finalize
 * @return 0 on success, -1 on error
 */
int kg_assembly_finalize_brain(kg_brain_wiring_t* brain);

/**
 * @brief Destroy brain wiring structure
 *
 * WHAT: Free brain wiring and all associated resources
 * WHY:  Cleanup after use or error
 * HOW:  Frees hemispheres, metadata, search index, callosal connections
 *
 * @param brain Brain wiring to destroy (NULL safe)
 *
 * @note Does NOT free the underlying module wiring pointers.
 */
void kg_assembly_destroy_brain(kg_brain_wiring_t* brain);

/* ============================================================================
 * KG Conversion API
 * ============================================================================ */

/**
 * @brief Write assembled brain wiring to knowledge graph
 *
 * WHAT: Convert complete brain wiring assembly into KG nodes and edges
 * WHY:  Transforms structural wiring into queryable knowledge graph
 * HOW:  Creates nodes for modules, edges for connections, adds metadata
 *
 * This function:
 * 1. Creates KG nodes for each module wiring
 * 2. Creates KG edges for intra-layer connections
 * 3. Creates KG edges for cross-layer connections
 * 4. Creates KG edges for corpus callosum connections
 * 5. Adds metadata to nodes from module, layer, hemisphere, system levels
 *
 * @param wiring Finalized brain wiring assembly
 * @param kg Target knowledge graph to populate
 * @return 0 on success, -1 on error
 *
 * @note Brain wiring must be finalized before calling this function.
 * @note KG should be empty or prepared for bulk insertion.
 */
int kg_assembly_write_to_kg(
    const kg_brain_wiring_t* wiring,
    brain_kg_t* kg
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get layer name string
 *
 * @param layer_index Layer index (0-5)
 * @return Layer name (e.g., "Layer I", "Layer IV") or "Unknown"
 */
const char* kg_assembly_layer_name(uint8_t layer_index);

/**
 * @brief Get hemisphere name string
 *
 * @param hemisphere Hemisphere ID
 * @return Hemisphere name ("Left", "Right") or "Unknown"
 */
const char* kg_assembly_hemisphere_name(uint8_t hemisphere);

/**
 * @brief Validate brain wiring consistency
 *
 * WHAT: Check brain wiring for structural consistency
 * WHY:  Detect errors before KG conversion
 * HOW:  Validates node IDs, cross-references, edge targets
 *
 * Checks performed:
 * - All referenced node IDs exist
 * - No duplicate modules
 * - Cross-layer edges reference valid layers
 * - Corpus callosum connects modules in correct hemispheres
 *
 * @param brain Brain wiring to validate
 * @return 0 if valid, number of errors found (positive) on issues
 */
int kg_assembly_validate(const kg_brain_wiring_t* brain);

/**
 * @brief Get assembly statistics
 *
 * @param brain Brain wiring to analyze
 * @param total_modules Output: total module count
 * @param total_internal_edges Output: total intra-layer edges
 * @param total_external_edges Output: total cross-layer edges
 * @param total_callosal Output: total corpus callosum connections
 * @return 0 on success
 */
int kg_assembly_get_stats(
    const kg_brain_wiring_t* brain,
    uint32_t* total_modules,
    uint32_t* total_internal_edges,
    uint32_t* total_external_edges,
    uint32_t* total_callosal
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KG_ASSEMBLY_H */
