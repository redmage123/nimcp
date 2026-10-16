//=============================================================================
// nimcp_cortical_hierarchy.h - Cortical Area Hierarchy & Connectivity
//=============================================================================
/**
 * @file nimcp_cortical_hierarchy.h
 * @brief Multi-area hierarchical cortical processing with laminar-specific connectivity
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Hierarchical cortical area organization with feedforward/feedback asymmetry
 * WHY:  Implement biological multi-area visual processing (V1→V2→V4→IT pathways)
 *       with laminar-specific inter-area connectivity patterns
 * HOW:  Areas organized in hierarchy with layer-specific projection patterns,
 *       receptive field expansion, and counter-stream architecture (dorsal/ventral)
 *
 * BIOLOGICAL FOUNDATION (Felleman & Van Essen 1991, Markov et al. 2014):
 *
 *   Visual Hierarchy:
 *   ┌────────────────────────────────────────────────────────────┐
 *   │  V1 (Lvl 0)  →  V2 (Lvl 1)  →  V4 (Lvl 2)  →  IT (Lvl 3)  │
 *   │    ↓ Dorsal      ↓              ↓              ↓           │
 *   │   MT (Motion)   PFC          Ventral        Recognition   │
 *   │  "Where/How"                  "What"                       │
 *   └────────────────────────────────────────────────────────────┘
 *
 * LAMINAR CONNECTIVITY PATTERNS:
 *
 *   Feedforward (FF) connections:
 *     Source: L2/3, L4 of lower area
 *     Target: L4 of higher area
 *     Strong, driving input
 *
 *   Feedback (FB) connections:
 *     Source: L5, L6 of higher area
 *     Target: L1, L5 of lower area
 *     Modulatory, predictive signals
 *
 *   Lateral connections:
 *     Source: L2/3
 *     Target: L2/3 of same level
 *     Feature binding, synchronization
 *
 * RECEPTIVE FIELD EXPANSION:
 *
 *   RF_size(level) = RF_base × expansion_factor^level
 *
 *   Example (visual):
 *     V1:  0.5° (simple/complex cells)
 *     V2:  2.0° (contour integration)
 *     V4:  8.0° (intermediate features)
 *     IT: 32.0° (whole objects)
 *
 * MATHEMATICAL ALGORITHMS:
 *
 *   Feedforward propagation:
 *     h_L+1 = σ(W_FF^L · h_L + b_L)
 *
 *   Feedback modulation:
 *     h_L = h_L ⊙ (1 + α · W_FB^L+1 · h_L+1)
 *     where ⊙ is element-wise product, α is modulation strength
 *
 *   Predictive coding error:
 *     e_L = h_L - W_FB^L+1 · h_L+1
 *
 * INTEGRATION:
 * - Uses cortical_column.h for within-area processing
 * - Uses cortical_layers.h for laminar organization
 * - Bio-async messaging for inter-area communication
 * - Thread-safe with nimcp_mutex
 *
 * REFERENCES:
 * - Felleman & Van Essen (1991) "Distributed hierarchical processing in primate cerebral cortex"
 * - Markov et al. (2014) "Anatomy of hierarchy: feedforward and feedback pathways in macaque visual cortex"
 * - Bastos et al. (2012) "Canonical microcircuits for predictive coding"
 * - Douglas & Martin (2004) "Neuronal circuits of the neocortex"
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CORTICAL_HIERARCHY_H
#define NIMCP_CORTICAL_HIERARCHY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct cortical_hierarchy cortical_hierarchy_t;
typedef struct cortical_area cortical_area_t;
typedef struct inter_area_connection inter_area_connection_t;

//=============================================================================
// Type Enumerations
//=============================================================================

/**
 * WHAT: Cortical area types in visual hierarchy
 * WHY:  Different areas have specialized functions and connectivity patterns
 * HOW:  Enum for standard visual areas plus custom extension
 */
typedef enum {
    CORTICAL_AREA_V1,      /**< Primary visual cortex (striate) */
    CORTICAL_AREA_V2,      /**< Secondary visual area (contours) */
    CORTICAL_AREA_V4,      /**< V4 (color, intermediate features) */
    CORTICAL_AREA_IT,      /**< Inferotemporal (object recognition) */
    CORTICAL_AREA_MT,      /**< Middle temporal (motion) */
    CORTICAL_AREA_PFC,     /**< Prefrontal cortex (executive) */
    CORTICAL_AREA_CUSTOM   /**< User-defined custom area */
} cortical_area_type_t;

/**
 * WHAT: Processing streams in visual cortex
 * WHY:  Dorsal/ventral pathways have different computational goals
 * HOW:  Binary classification for routing signals
 */
typedef enum {
    STREAM_VENTRAL,    /**< "What" pathway (object identity) */
    STREAM_DORSAL      /**< "Where/How" pathway (spatial location, action) */
} processing_stream_t;

/**
 * WHAT: Connection type classification
 * WHY:  Feedforward and feedback have different laminar patterns
 * HOW:  Determines source/target layers automatically
 */
typedef enum {
    CONNECTION_TYPE_FEEDFORWARD,  /**< FF: lower → higher area */
    CONNECTION_TYPE_FEEDBACK,     /**< FB: higher → lower area */
    CONNECTION_TYPE_LATERAL       /**< Lateral: same hierarchy level */
} connection_type_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * WHAT: Configuration for a single cortical area
 * WHY:  Each area has unique properties (level, RF size, stream)
 * HOW:  Stores biological parameters for area creation
 *
 * INVARIANTS:
 * - hierarchy_level >= 0
 * - rf_expansion_factor > 1.0 (typically 2.0-4.0)
 * - num_hypercolumns > 0
 * - 0.0 <= feedforward_strength <= 1.0
 * - 0.0 <= feedback_strength <= 1.0
 */
typedef struct cortical_area_config {
    cortical_area_type_t type;       /**< Area type identifier */
    processing_stream_t stream;      /**< Ventral or dorsal stream */
    uint32_t hierarchy_level;        /**< 0 = lowest (V1), increases upward */
    float rf_expansion_factor;       /**< RF size multiplier per level */
    uint32_t num_hypercolumns;       /**< Number of hypercolumns in area */
    uint32_t neurons_per_hypercolumn; /**< Neurons in each hypercolumn */
    float feedforward_strength;      /**< FF connection weight multiplier */
    float feedback_strength;         /**< FB connection weight multiplier */
    const char* custom_name;         /**< Name for custom areas (optional) */
} cortical_area_config_t;

/**
 * WHAT: Inter-area connection configuration
 * WHY:  Defines connectivity between cortical areas with laminar specificity
 * HOW:  Specifies source/target areas, layers, and connection properties
 *
 * INVARIANTS:
 * - source_layer, target_layer in [0, 4] (5 layers)
 * - delay_ms >= 0.0 (typically 5-50ms for visual areas)
 * - 0.0 < weight <= 1.0
 */
typedef struct inter_area_connection_config {
    uint32_t source_area_id;         /**< ID of source area */
    uint32_t target_area_id;         /**< ID of target area */
    connection_type_t type;          /**< FF, FB, or lateral */
    uint32_t source_layer;           /**< Layer index in source area */
    uint32_t target_layer;           /**< Layer index in target area */
    float weight;                    /**< Connection strength */
    float delay_ms;                  /**< Transmission delay */
    bool use_canonical_layers;       /**< Auto-set layers from connection type */
} inter_area_connection_config_t;

/**
 * WHAT: Configuration for entire hierarchical system
 * WHY:  System-wide parameters affecting all areas
 * HOW:  Global settings for hierarchy behavior
 */
typedef struct cortical_hierarchy_config {
    uint32_t max_areas;              /**< Maximum number of areas */
    uint32_t max_connections;        /**< Maximum inter-area connections */
    float default_rf_base;           /**< Base RF size (degrees visual angle) */
    float default_expansion_factor;  /**< Default RF expansion per level */
    bool enable_predictive_coding;   /**< Enable FB prediction errors */
    bool enable_bio_async;           /**< Enable bio-async messaging */
} cortical_hierarchy_config_t;

//=============================================================================
// Statistics Structures
//=============================================================================

/**
 * WHAT: Runtime statistics for a cortical area
 * WHY:  Monitor area activity, RF properties, and processing
 * HOW:  Collected during propagation operations
 */
typedef struct cortical_area_stats {
    uint32_t area_id;                /**< Area identifier */
    cortical_area_type_t type;       /**< Area type */
    uint32_t hierarchy_level;        /**< Hierarchical level */
    float receptive_field_size;      /**< Current RF size */
    float mean_activity;             /**< Average activation across area */
    float peak_activity;             /**< Maximum activation */
    uint32_t num_active_columns;     /**< Number of active hypercolumns */
    uint32_t num_ff_inputs;          /**< Feedforward input connections */
    uint32_t num_fb_inputs;          /**< Feedback input connections */
} cortical_area_stats_t;

/**
 * WHAT: Global hierarchy statistics
 * WHY:  Monitor overall system state and information flow
 * HOW:  Aggregated from all areas and connections
 */
typedef struct cortical_hierarchy_stats {
    uint32_t num_areas;              /**< Total areas in hierarchy */
    uint32_t num_connections;        /**< Total inter-area connections */
    uint32_t num_ff_connections;     /**< Feedforward connections */
    uint32_t num_fb_connections;     /**< Feedback connections */
    uint32_t max_hierarchy_level;    /**< Deepest level in hierarchy */
    float total_prediction_error;    /**< Sum of prediction errors (if enabled) */
    uint64_t total_propagations;     /**< Propagation operations executed */
} cortical_hierarchy_stats_t;

//=============================================================================
// Core API Functions
//=============================================================================

/**
 * WHAT: Get default hierarchy configuration
 * WHY:  Provide sensible defaults for visual hierarchy
 * HOW:  Returns pre-configured settings based on neuroscience literature
 *
 * @return Default configuration structure
 */
cortical_hierarchy_config_t cortical_hierarchy_default_config(void);

/**
 * WHAT: Create cortical hierarchy system
 * WHY:  Initialize multi-area hierarchical processing
 * HOW:  Allocates hierarchy structure with config parameters
 *
 * @param config System configuration
 * @return Hierarchy handle, NULL on failure
 */
cortical_hierarchy_t* cortical_hierarchy_create(
    const cortical_hierarchy_config_t* config);

/**
 * WHAT: Destroy cortical hierarchy and free resources
 * WHY:  Clean memory and release all areas/connections
 * HOW:  Frees areas, connections, internal buffers
 *
 * @param hierarchy Hierarchy to destroy
 */
void cortical_hierarchy_destroy(cortical_hierarchy_t* hierarchy);

//=============================================================================
// Area Management Functions
//=============================================================================

/**
 * WHAT: Add cortical area to hierarchy
 * WHY:  Build multi-area architecture incrementally
 * HOW:  Creates area with config, assigns ID, adds to hierarchy
 *
 * @param hierarchy Hierarchy system
 * @param config Area configuration
 * @param area_id_out Output: assigned area ID
 * @return 0 on success, negative on error
 */
int cortical_hierarchy_add_area(
    cortical_hierarchy_t* hierarchy,
    const cortical_area_config_t* config,
    uint32_t* area_id_out);

/**
 * WHAT: Remove area from hierarchy
 * WHY:  Support dynamic reconfiguration
 * HOW:  Removes area and all associated connections
 *
 * @param hierarchy Hierarchy system
 * @param area_id Area to remove
 * @return 0 on success, negative on error
 */
int cortical_hierarchy_remove_area(
    cortical_hierarchy_t* hierarchy,
    uint32_t area_id);

/**
 * WHAT: Get number of areas in hierarchy
 * WHY:  Query system size
 * HOW:  Returns current area count
 *
 * @param hierarchy Hierarchy system
 * @return Number of areas, 0 on error
 */
uint32_t cortical_hierarchy_get_num_areas(
    const cortical_hierarchy_t* hierarchy);

/**
 * WHAT: Get area configuration by ID
 * WHY:  Inspect area properties
 * HOW:  Returns pointer to area config (read-only)
 *
 * @param hierarchy Hierarchy system
 * @param area_id Area identifier
 * @return Pointer to config, NULL on error
 */
const cortical_area_config_t* cortical_hierarchy_get_area_config(
    const cortical_hierarchy_t* hierarchy,
    uint32_t area_id);

//=============================================================================
// Connection Management Functions
//=============================================================================

/**
 * WHAT: Connect two cortical areas
 * WHY:  Establish feedforward, feedback, or lateral projections
 * HOW:  Creates connection with laminar specificity based on type
 *
 * @param hierarchy Hierarchy system
 * @param config Connection configuration
 * @param connection_id_out Output: assigned connection ID
 * @return 0 on success, negative on error
 */
int cortical_hierarchy_connect_areas(
    cortical_hierarchy_t* hierarchy,
    const inter_area_connection_config_t* config,
    uint32_t* connection_id_out);

/**
 * WHAT: Remove connection between areas
 * WHY:  Support dynamic connectivity changes
 * HOW:  Removes connection from hierarchy
 *
 * @param hierarchy Hierarchy system
 * @param connection_id Connection to remove
 * @return 0 on success, negative on error
 */
int cortical_hierarchy_disconnect_areas(
    cortical_hierarchy_t* hierarchy,
    uint32_t connection_id);

/**
 * WHAT: Apply canonical visual hierarchy connections
 * WHY:  Quickly create standard V1→V2→V4→IT pathway
 * HOW:  Automatically creates FF and FB connections with proper laminar patterns
 *
 * @param hierarchy Hierarchy system
 * @return 0 on success, negative on error
 */
int cortical_hierarchy_apply_canonical_connections(
    cortical_hierarchy_t* hierarchy);

//=============================================================================
// Propagation Functions
//=============================================================================

/**
 * WHAT: Propagate activity feedforward through hierarchy
 * WHY:  Bottom-up processing from sensory input to higher cognition
 * HOW:  Processes areas in ascending hierarchy order (L0→L1→L2→...)
 *
 * @param hierarchy Hierarchy system
 * @param start_level Starting hierarchy level (typically 0 for V1)
 * @param end_level Ending level (inclusive)
 * @return 0 on success, negative on error
 */
int cortical_hierarchy_propagate_feedforward(
    cortical_hierarchy_t* hierarchy,
    uint32_t start_level,
    uint32_t end_level);

/**
 * WHAT: Propagate activity feedback through hierarchy
 * WHY:  Top-down modulation, predictions, attention
 * HOW:  Processes areas in descending order (LN→...→L1→L0)
 *
 * @param hierarchy Hierarchy system
 * @param start_level Starting level (typically highest)
 * @param end_level Ending level (inclusive, typically 0)
 * @return 0 on success, negative on error
 */
int cortical_hierarchy_propagate_feedback(
    cortical_hierarchy_t* hierarchy,
    uint32_t start_level,
    uint32_t end_level);

/**
 * WHAT: Compute prediction error at an area
 * WHY:  Predictive coding requires error signals for learning
 * HOW:  Calculates difference between actual and predicted activity
 *
 * @param hierarchy Hierarchy system
 * @param area_id Area to compute error for
 * @param error_out Output: prediction error value
 * @return 0 on success, negative on error
 */
int cortical_hierarchy_compute_prediction_error(
    cortical_hierarchy_t* hierarchy,
    uint32_t area_id,
    float* error_out);

//=============================================================================
// Activity Query Functions
//=============================================================================

/**
 * WHAT: Set input activity to a cortical area
 * WHY:  Inject sensory or external signals
 * HOW:  Sets activity buffer for specified area
 *
 * @param hierarchy Hierarchy system
 * @param area_id Target area
 * @param activity Activity values (size = num_hypercolumns)
 * @param size Number of values in activity array
 * @return 0 on success, negative on error
 */
int cortical_hierarchy_set_area_input(
    cortical_hierarchy_t* hierarchy,
    uint32_t area_id,
    const float* activity,
    uint32_t size);

/**
 * WHAT: Get current activity from a cortical area
 * WHY:  Read out area responses for downstream processing
 * HOW:  Copies activity buffer to output array
 *
 * @param hierarchy Hierarchy system
 * @param area_id Source area
 * @param activity_out Output: activity values
 * @param max_size Maximum values to write
 * @param actual_size_out Output: actual number of values written
 * @return 0 on success, negative on error
 */
int cortical_hierarchy_get_area_activity(
    const cortical_hierarchy_t* hierarchy,
    uint32_t area_id,
    float* activity_out,
    uint32_t max_size,
    uint32_t* actual_size_out);

/**
 * WHAT: Get receptive field size for area at given level
 * WHY:  RF size increases hierarchically (important for understanding representations)
 * HOW:  Calculates RF = base × expansion^level
 *
 * @param hierarchy Hierarchy system
 * @param area_id Area to query
 * @param rf_size_out Output: receptive field size
 * @return 0 on success, negative on error
 */
int cortical_hierarchy_get_receptive_field_size(
    const cortical_hierarchy_t* hierarchy,
    uint32_t area_id,
    float* rf_size_out);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * WHAT: Get statistics for specific area
 * WHY:  Monitor area-level activity and connectivity
 * HOW:  Computes stats from area state
 *
 * @param hierarchy Hierarchy system
 * @param area_id Area to get stats for
 * @param stats_out Output: area statistics
 * @return 0 on success, negative on error
 */
int cortical_hierarchy_get_area_stats(
    const cortical_hierarchy_t* hierarchy,
    uint32_t area_id,
    cortical_area_stats_t* stats_out);

/**
 * WHAT: Get global hierarchy statistics
 * WHY:  Monitor overall system state
 * HOW:  Aggregates stats from all areas
 *
 * @param hierarchy Hierarchy system
 * @param stats_out Output: hierarchy statistics
 * @return 0 on success, negative on error
 */
int cortical_hierarchy_get_stats(
    const cortical_hierarchy_t* hierarchy,
    cortical_hierarchy_stats_t* stats_out);

//=============================================================================
// Bio-Async Integration Functions
//=============================================================================

/**
 * WHAT: Connect hierarchy to bio-async messaging system
 * WHY:  Enable inter-area communication via biological signaling
 * HOW:  Registers module with bio-async router
 *
 * @param hierarchy Hierarchy system
 * @return 0 on success, negative on error
 */
int cortical_hierarchy_connect_bio_async(cortical_hierarchy_t* hierarchy);

/**
 * WHAT: Disconnect from bio-async system
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregisters module
 *
 * @param hierarchy Hierarchy system
 * @return 0 on success, negative on error
 */
int cortical_hierarchy_disconnect_bio_async(cortical_hierarchy_t* hierarchy);

/**
 * WHAT: Check if bio-async is connected
 * WHY:  Conditional messaging behavior
 * HOW:  Returns bio-async enabled flag
 *
 * @param hierarchy Hierarchy system
 * @return true if connected, false otherwise
 */
bool cortical_hierarchy_is_bio_async_connected(
    const cortical_hierarchy_t* hierarchy);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_CORTICAL_HIERARCHY_H
