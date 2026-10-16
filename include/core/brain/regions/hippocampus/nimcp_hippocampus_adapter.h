/**
 * @file nimcp_hippocampus_adapter.h
 * @brief Brain adapter for Hippocampus integration
 *
 * WHAT: Unified adapter connecting hippocampal sub-modules to the brain system
 * WHY:  Enable seamless integration with cognitive layers, memory, and spatial navigation
 * HOW:  Orchestrates place cells, grid cells, pattern separation/completion as a cohesive unit
 *
 * ARCHITECTURE:
 * - Wraps all hippocampal sub-modules (place cells, grid cells, memory encoding)
 * - Provides high-level API for memory formation and retrieval
 * - Integrates with cortical areas for memory consolidation
 * - Connects to event bus for inter-module communication
 * - Supports training through backpropagation adapters
 *
 * BIOLOGICAL BASIS:
 * - Models CA1, CA3, Dentate Gyrus, and Entorhinal Cortex
 * - Place cells for location encoding
 * - Grid cells for spatial representation
 * - Pattern separation in DG, pattern completion in CA3
 * - Memory encoding and consolidation
 *
 * @version Phase H1: Hippocampus Brain Integration
 * @date 2025-12-30
 */

#ifndef NIMCP_HIPPOCAMPUS_ADAPTER_H
#define NIMCP_HIPPOCAMPUS_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Bio-async communication system */
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

/* Logging system */
#include "utils/logging/nimcp_logging.h"

/* Unified memory system */
#include "utils/memory/nimcp_unified_memory.h"

/* Forward declarations for sub-modules */
typedef struct place_cell_network place_cell_network_t;
typedef struct grid_cell_network grid_cell_network_t;
typedef struct pattern_separator pattern_separator_t;
typedef struct memory_encoder memory_encoder_t;

/* Forward declaration for opaque adapter type */
typedef struct hippocampus_adapter hippocampus_adapter_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Default configuration values
 */
#define HIPPOCAMPUS_DEFAULT_NUM_PLACE_CELLS     256
#define HIPPOCAMPUS_DEFAULT_NUM_GRID_CELLS      128
#define HIPPOCAMPUS_DEFAULT_CA1_SIZE            512
#define HIPPOCAMPUS_DEFAULT_CA3_SIZE            256
#define HIPPOCAMPUS_DEFAULT_DG_SIZE             1024
#define HIPPOCAMPUS_DEFAULT_EC_SIZE             256
#define HIPPOCAMPUS_DEFAULT_MEMORY_CAPACITY     1000
#define HIPPOCAMPUS_DEFAULT_SPATIAL_DIM         2
#define HIPPOCAMPUS_DEFAULT_FEATURE_DIM         64

/**
 * @brief Hippocampus adapter configuration
 */
typedef struct {
    /* Capacity limits */
    uint32_t num_place_cells;        /**< Number of place cells */
    uint32_t num_grid_cells;         /**< Number of grid cells */
    uint32_t ca1_size;               /**< CA1 pyramidal cell count */
    uint32_t ca3_size;               /**< CA3 pyramidal cell count */
    uint32_t dg_size;                /**< Dentate gyrus granule cell count */
    uint32_t ec_size;                /**< Entorhinal cortex cell count */

    /* Memory parameters */
    uint32_t memory_capacity;        /**< Max memories to store */
    float memory_decay_rate;         /**< Memory strength decay per tick */
    float consolidation_threshold;   /**< Threshold for cortical transfer */

    /* Spatial parameters */
    uint32_t spatial_dim;            /**< Spatial dimension (2D or 3D) */
    float place_field_radius;        /**< Place field radius */
    float grid_spacing;              /**< Grid cell spacing */
    uint32_t num_grid_scales;        /**< Number of grid scales */

    /* Feature parameters */
    uint32_t feature_dim;            /**< Feature vector dimension */

    /* Processing options */
    bool enable_pattern_separation;  /**< Enable DG pattern separation */
    bool enable_pattern_completion;  /**< Enable CA3 pattern completion */
    bool enable_replay;              /**< Enable memory replay */
    bool enable_spatial_navigation;  /**< Enable spatial navigation */

    /* Event system */
    bool enable_events;              /**< Enable event bus integration */

    /* Training */
    bool enable_training;            /**< Enable learning capabilities */
    float learning_rate;             /**< Base learning rate */

    /* Bio-async communication */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    nimcp_bio_channel_type_t default_channel; /**< Default neuromodulator channel */
} hippocampus_config_t;

/*=============================================================================
 * STATUS AND STATE
 *===========================================================================*/

/**
 * @brief Processing status of the adapter
 */
typedef enum {
    HIPPOCAMPUS_STATUS_IDLE = 0,         /**< Ready for input */
    HIPPOCAMPUS_STATUS_ENCODING,         /**< Encoding new memory */
    HIPPOCAMPUS_STATUS_RETRIEVING,       /**< Retrieving memory */
    HIPPOCAMPUS_STATUS_NAVIGATING,       /**< Spatial navigation active */
    HIPPOCAMPUS_STATUS_CONSOLIDATING,    /**< Memory consolidation */
    HIPPOCAMPUS_STATUS_REPLAYING,        /**< Memory replay active */
    HIPPOCAMPUS_STATUS_READY,            /**< Output ready for retrieval */
    HIPPOCAMPUS_STATUS_ERROR             /**< Error state */
} hippocampus_status_t;

/**
 * @brief Error codes for hippocampus operations
 */
typedef enum {
    HIPPOCAMPUS_ERROR_NONE = 0,
    HIPPOCAMPUS_ERROR_INVALID_INPUT,
    HIPPOCAMPUS_ERROR_ENCODING_FAILURE,
    HIPPOCAMPUS_ERROR_RETRIEVAL_FAILURE,
    HIPPOCAMPUS_ERROR_NAVIGATION_FAILURE,
    HIPPOCAMPUS_ERROR_MEMORY_FULL,
    HIPPOCAMPUS_ERROR_PATTERN_SEPARATION_FAILURE,
    HIPPOCAMPUS_ERROR_PATTERN_COMPLETION_FAILURE,
    HIPPOCAMPUS_ERROR_BUFFER_OVERFLOW,
    HIPPOCAMPUS_ERROR_INTERNAL
} hippocampus_error_t;

/*=============================================================================
 * INPUT/OUTPUT STRUCTURES
 *===========================================================================*/

/**
 * @brief Spatial location representation
 */
typedef struct {
    float x;                         /**< X coordinate */
    float y;                         /**< Y coordinate */
    float z;                         /**< Z coordinate (if 3D) */
    float heading;                   /**< Heading direction (radians) */
    float velocity;                  /**< Movement velocity */
} hippocampus_location_t;

/**
 * @brief Place cell activity
 */
typedef struct {
    uint32_t cell_id;                /**< Place cell identifier */
    float activation;                /**< Activation level [0, 1] */
    hippocampus_location_t center;   /**< Place field center */
    float field_radius;              /**< Place field radius */
} place_cell_activity_t;

/**
 * @brief Grid cell activity
 */
typedef struct {
    uint32_t cell_id;                /**< Grid cell identifier */
    float activation;                /**< Activation level [0, 1] */
    float spacing;                   /**< Grid spacing */
    float orientation;               /**< Grid orientation (radians) */
    float phase_x;                   /**< Phase offset X */
    float phase_y;                   /**< Phase offset Y */
} grid_cell_activity_t;

/**
 * @brief Memory entry for encoding/retrieval
 */
typedef struct {
    uint32_t memory_id;              /**< Unique memory identifier */
    float* features;                 /**< Feature vector */
    uint32_t feature_count;          /**< Number of features */
    hippocampus_location_t location; /**< Associated spatial location */
    float strength;                  /**< Memory strength [0, 1] */
    float emotional_valence;         /**< Emotional valence [-1, 1] */
    uint64_t timestamp_ms;           /**< Encoding timestamp */
    bool is_consolidated;            /**< Transferred to cortex? */
} hippocampus_memory_t;

/**
 * @brief Pattern separation result
 */
typedef struct {
    float* sparse_code;              /**< Sparse DG output */
    uint32_t sparse_size;            /**< Size of sparse code */
    float sparsity;                  /**< Sparsity level [0, 1] */
    float separation_strength;       /**< Separation quality [0, 1] */
} pattern_separation_result_t;

/**
 * @brief Pattern completion result
 */
typedef struct {
    float* completed_pattern;        /**< Completed CA3 output */
    uint32_t pattern_size;           /**< Size of completed pattern */
    float completion_confidence;     /**< Confidence [0, 1] */
    uint32_t matched_memory_id;      /**< ID of matched memory (if any) */
} pattern_completion_result_t;

/**
 * @brief Spatial navigation result
 */
typedef struct {
    hippocampus_location_t current;  /**< Current estimated location */
    hippocampus_location_t goal;     /**< Goal location */
    float* path;                     /**< Waypoints (x,y pairs) */
    uint32_t path_length;            /**< Number of waypoints */
    float distance_to_goal;          /**< Estimated distance */
    float heading_error;             /**< Error to goal heading */
} navigation_result_t;

/**
 * @brief Memory retrieval result
 */
typedef struct {
    hippocampus_memory_t* memories;  /**< Retrieved memories */
    uint32_t count;                  /**< Number of memories */
    float* similarities;             /**< Similarity scores */
    bool retrieval_success;          /**< Retrieval succeeded? */
} retrieval_result_t;

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Adapter statistics
 */
typedef struct {
    /* Processing counts */
    uint64_t memories_encoded;       /**< Total memories encoded */
    uint64_t memories_retrieved;     /**< Total retrieval attempts */
    uint64_t successful_retrievals;  /**< Successful retrievals */
    uint64_t navigation_steps;       /**< Navigation update count */
    uint64_t replay_episodes;        /**< Replay episode count */

    /* Pattern operations */
    uint64_t separations_performed;  /**< Pattern separations */
    uint64_t completions_performed;  /**< Pattern completions */

    /* Memory state */
    uint32_t current_memory_count;   /**< Memories currently stored */
    uint32_t consolidated_count;     /**< Memories consolidated to cortex */

    /* Timing */
    float avg_encoding_latency_ms;   /**< Average encoding time */
    float avg_retrieval_latency_ms;  /**< Average retrieval time */

    /* Training */
    uint64_t training_iterations;    /**< Training updates */
    float training_loss;             /**< Current training loss */
} hippocampus_stats_t;

/*=============================================================================
 * CALLBACK TYPES
 *===========================================================================*/

/**
 * @brief Callback for memory consolidation (transfer to cortex)
 */
typedef void (*hippocampus_consolidation_callback_t)(
    const hippocampus_memory_t* memory,
    void* user_data
);

/**
 * @brief Callback for spatial position update
 */
typedef void (*hippocampus_position_callback_t)(
    const hippocampus_location_t* location,
    void* user_data
);

/**
 * @brief Callback for event notification
 */
typedef void (*hippocampus_event_callback_t)(
    uint32_t event_type,
    const void* event_data,
    void* user_data
);

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default configuration
 *
 * WHAT: Returns default configuration for hippocampus adapter
 * WHY:  Provide sensible defaults for common use cases
 * HOW:  Initialize all fields with biologically-motivated values
 *
 * @return Default configuration structure
 */
hippocampus_config_t hippocampus_default_config(void);

/**
 * @brief Create hippocampus adapter
 *
 * WHAT: Allocate and initialize the adapter with all sub-modules
 * WHY:  Central point for hippocampal function initialization
 * HOW:  Create place cells, grid cells, memory encoder; initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New adapter instance, or NULL on failure
 */
hippocampus_adapter_t* hippocampus_create(const hippocampus_config_t* config);

/**
 * @brief Destroy hippocampus adapter
 *
 * WHAT: Free all resources associated with the adapter
 * WHY:  Prevent memory leaks
 * HOW:  Destroy sub-modules, free buffers and memory store
 *
 * @param adapter Adapter to destroy
 */
void hippocampus_destroy(hippocampus_adapter_t* adapter);

/**
 * @brief Reset adapter state
 *
 * WHAT: Clear buffers and reset to idle state
 * WHY:  Prepare for new session without full reinitialization
 * HOW:  Reset all sub-modules, clear spatial state
 *
 * @param adapter Adapter instance
 * @return true on success, false on failure
 */
bool hippocampus_reset(hippocampus_adapter_t* adapter);

/*=============================================================================
 * MEMORY ENCODING API
 *===========================================================================*/

/**
 * @brief Encode new memory
 *
 * WHAT: Create new episodic memory from features and context
 * WHY:  Memory formation is core hippocampal function
 * HOW:  Pattern separate in DG, store in CA3/CA1
 *
 * @param adapter Adapter instance
 * @param features Feature vector to encode
 * @param num_features Number of features
 * @param location Spatial context (optional, can be NULL)
 * @param emotional_valence Emotional tag [-1, 1]
 * @return Memory ID on success, 0 on failure
 */
uint32_t hippocampus_encode_memory(
    hippocampus_adapter_t* adapter,
    const float* features,
    uint32_t num_features,
    const hippocampus_location_t* location,
    float emotional_valence
);

/**
 * @brief Retrieve memories by cue
 *
 * WHAT: Content-addressable memory retrieval
 * WHY:  Pattern completion enables partial cue recall
 * HOW:  Pattern complete in CA3, return matching memories
 *
 * @param adapter Adapter instance
 * @param cue Query feature vector
 * @param cue_size Size of cue vector
 * @param max_results Maximum memories to return
 * @param result Output result structure
 * @return true on success
 */
bool hippocampus_retrieve_by_cue(
    hippocampus_adapter_t* adapter,
    const float* cue,
    uint32_t cue_size,
    uint32_t max_results,
    retrieval_result_t* result
);

/**
 * @brief Retrieve memories by location
 *
 * WHAT: Spatial context-based memory retrieval
 * WHY:  Location is powerful retrieval cue
 * HOW:  Activate place cells, retrieve associated memories
 *
 * @param adapter Adapter instance
 * @param location Query location
 * @param radius Search radius
 * @param max_results Maximum memories to return
 * @param result Output result structure
 * @return true on success
 */
bool hippocampus_retrieve_by_location(
    hippocampus_adapter_t* adapter,
    const hippocampus_location_t* location,
    float radius,
    uint32_t max_results,
    retrieval_result_t* result
);

/**
 * @brief Get memory by ID
 *
 * WHAT: Direct memory access by identifier
 * WHY:  Allow precise memory inspection
 * HOW:  Lookup in memory store
 *
 * @param adapter Adapter instance
 * @param memory_id Memory identifier
 * @param memory Output memory structure
 * @return true if found
 */
bool hippocampus_get_memory(
    const hippocampus_adapter_t* adapter,
    uint32_t memory_id,
    hippocampus_memory_t* memory
);

/*=============================================================================
 * PATTERN SEPARATION AND COMPLETION API
 *===========================================================================*/

/**
 * @brief Perform pattern separation
 *
 * WHAT: Transform input to sparse distributed representation
 * WHY:  Reduce interference between similar patterns
 * HOW:  Dentate gyrus competitive learning
 *
 * @param adapter Adapter instance
 * @param input Input pattern
 * @param input_size Input size
 * @param result Output separation result
 * @return true on success
 */
bool hippocampus_pattern_separate(
    hippocampus_adapter_t* adapter,
    const float* input,
    uint32_t input_size,
    pattern_separation_result_t* result
);

/**
 * @brief Perform pattern completion
 *
 * WHAT: Reconstruct full pattern from partial cue
 * WHY:  Enable recall from degraded or partial input
 * HOW:  CA3 autoassociative network
 *
 * @param adapter Adapter instance
 * @param partial_cue Partial input pattern
 * @param cue_size Cue size
 * @param result Output completion result
 * @return true on success
 */
bool hippocampus_pattern_complete(
    hippocampus_adapter_t* adapter,
    const float* partial_cue,
    uint32_t cue_size,
    pattern_completion_result_t* result
);

/*=============================================================================
 * SPATIAL NAVIGATION API
 *===========================================================================*/

/**
 * @brief Update current position
 *
 * WHAT: Update spatial representation with new position
 * WHY:  Track location for navigation and memory context
 * HOW:  Update place and grid cell activations
 *
 * @param adapter Adapter instance
 * @param location New location
 * @return true on success
 */
bool hippocampus_update_position(
    hippocampus_adapter_t* adapter,
    const hippocampus_location_t* location
);

/**
 * @brief Get current position estimate
 *
 * WHAT: Decode position from place/grid cell activity
 * WHY:  Self-localization from neural activity
 * HOW:  Population vector decoding
 *
 * @param adapter Adapter instance
 * @param location Output location estimate
 * @return true on success
 */
bool hippocampus_get_position_estimate(
    const hippocampus_adapter_t* adapter,
    hippocampus_location_t* location
);

/**
 * @brief Set navigation goal
 *
 * WHAT: Set target location for navigation
 * WHY:  Enable goal-directed navigation
 * HOW:  Compute path through cognitive map
 *
 * @param adapter Adapter instance
 * @param goal Target location
 * @return true on success
 */
bool hippocampus_set_navigation_goal(
    hippocampus_adapter_t* adapter,
    const hippocampus_location_t* goal
);

/**
 * @brief Get navigation guidance
 *
 * WHAT: Compute next navigation step
 * WHY:  Guide movement toward goal
 * HOW:  Use place cell lookahead and grid cell vector
 *
 * @param adapter Adapter instance
 * @param result Output navigation result
 * @return true on success
 */
bool hippocampus_get_navigation_guidance(
    hippocampus_adapter_t* adapter,
    navigation_result_t* result
);

/**
 * @brief Get place cell activations
 *
 * WHAT: Retrieve current place cell activity pattern
 * WHY:  Access spatial representation
 * HOW:  Return activation vector
 *
 * @param adapter Adapter instance
 * @param activities Output array (must be pre-allocated)
 * @param max_count Maximum activities to return
 * @param count Output: actual count
 * @return true on success
 */
bool hippocampus_get_place_cell_activity(
    const hippocampus_adapter_t* adapter,
    place_cell_activity_t* activities,
    uint32_t max_count,
    uint32_t* count
);

/**
 * @brief Get grid cell activations
 *
 * WHAT: Retrieve current grid cell activity pattern
 * WHY:  Access metric spatial representation
 * HOW:  Return activation vector
 *
 * @param adapter Adapter instance
 * @param activities Output array (must be pre-allocated)
 * @param max_count Maximum activities to return
 * @param count Output: actual count
 * @return true on success
 */
bool hippocampus_get_grid_cell_activity(
    const hippocampus_adapter_t* adapter,
    grid_cell_activity_t* activities,
    uint32_t max_count,
    uint32_t* count
);

/*=============================================================================
 * MEMORY CONSOLIDATION API
 *===========================================================================*/

/**
 * @brief Trigger memory consolidation
 *
 * WHAT: Transfer strong memories to cortical storage
 * WHY:  Systems consolidation for long-term storage
 * HOW:  Replay memories above threshold, invoke callback
 *
 * @param adapter Adapter instance
 * @param strength_threshold Minimum strength for consolidation
 * @return Number of memories consolidated
 */
uint32_t hippocampus_consolidate_memories(
    hippocampus_adapter_t* adapter,
    float strength_threshold
);

/**
 * @brief Set consolidation callback
 *
 * WHAT: Register callback for consolidated memories
 * WHY:  Allow cortical systems to receive memories
 * HOW:  Invoke callback during consolidation
 *
 * @param adapter Adapter instance
 * @param callback Consolidation handler function
 * @param user_data User context
 * @return true on success
 */
bool hippocampus_set_consolidation_callback(
    hippocampus_adapter_t* adapter,
    hippocampus_consolidation_callback_t callback,
    void* user_data
);

/**
 * @brief Trigger memory replay
 *
 * WHAT: Replay stored memories (forward or reverse)
 * WHY:  Support consolidation and planning
 * HOW:  Sequentially activate stored patterns
 *
 * @param adapter Adapter instance
 * @param reverse Reverse replay order (for credit assignment)
 * @param num_episodes Number of episodes to replay
 * @return true on success
 */
bool hippocampus_trigger_replay(
    hippocampus_adapter_t* adapter,
    bool reverse,
    uint32_t num_episodes
);

/*=============================================================================
 * EVENT INTEGRATION
 *===========================================================================*/

/**
 * @brief Set event callback
 *
 * WHAT: Register callback for hippocampal events
 * WHY:  Allow external monitoring and reaction
 * HOW:  Store callback, invoke on significant events
 *
 * @param adapter Adapter instance
 * @param callback Event handler function
 * @param user_data User context
 * @return true on success
 */
bool hippocampus_set_event_callback(
    hippocampus_adapter_t* adapter,
    hippocampus_event_callback_t callback,
    void* user_data
);

/*=============================================================================
 * TRAINING INTERFACE
 *===========================================================================*/

/**
 * @brief Train on memory-cue pair
 *
 * WHAT: Strengthen association between cue and memory
 * WHY:  Enable supervised learning of associations
 * HOW:  Hebbian learning in CA3 network
 *
 * @param adapter Adapter instance
 * @param cue Input cue pattern
 * @param cue_size Cue size
 * @param target_memory_id Target memory to associate
 * @param learning_rate Learning rate (0 = use config default)
 * @return true on success
 */
bool hippocampus_train_association(
    hippocampus_adapter_t* adapter,
    const float* cue,
    uint32_t cue_size,
    uint32_t target_memory_id,
    float learning_rate
);

/**
 * @brief Train place cell representation
 *
 * WHAT: Learn place cell tuning for location
 * WHY:  Build cognitive map from experience
 * HOW:  Competitive learning in place cell network
 *
 * @param adapter Adapter instance
 * @param location Training location
 * @param features Associated features
 * @param num_features Number of features
 * @return true on success
 */
bool hippocampus_train_place_field(
    hippocampus_adapter_t* adapter,
    const hippocampus_location_t* location,
    const float* features,
    uint32_t num_features
);

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

/**
 * @brief Get current processing status
 *
 * @param adapter Adapter instance
 * @return Current status
 */
hippocampus_status_t hippocampus_get_status(const hippocampus_adapter_t* adapter);

/**
 * @brief Get last error code
 *
 * @param adapter Adapter instance
 * @return Last error, or HIPPOCAMPUS_ERROR_NONE
 */
hippocampus_error_t hippocampus_get_last_error(const hippocampus_adapter_t* adapter);

/**
 * @brief Get error description string
 *
 * @param error Error code
 * @return Human-readable error description
 */
const char* hippocampus_error_string(hippocampus_error_t error);

/**
 * @brief Get status description string
 *
 * @param status Status code
 * @return Human-readable status description
 */
const char* hippocampus_status_string(hippocampus_status_t status);

/**
 * @brief Get adapter statistics
 *
 * @param adapter Adapter instance
 * @param stats Output statistics structure
 * @return true on success
 */
bool hippocampus_get_stats(const hippocampus_adapter_t* adapter, hippocampus_stats_t* stats);

/**
 * @brief Get adapter configuration
 *
 * @param adapter Adapter instance
 * @param config Output configuration structure
 * @return true on success
 */
bool hippocampus_get_config(const hippocampus_adapter_t* adapter, hippocampus_config_t* config);

/*=============================================================================
 * SUB-MODULE ACCESS (Advanced)
 *===========================================================================*/

/**
 * @brief Get place cell network handle
 *
 * @param adapter Adapter instance
 * @return Place cell network, or NULL
 */
place_cell_network_t* hippocampus_get_place_cells(hippocampus_adapter_t* adapter);

/**
 * @brief Get grid cell network handle
 *
 * @param adapter Adapter instance
 * @return Grid cell network, or NULL
 */
grid_cell_network_t* hippocampus_get_grid_cells(hippocampus_adapter_t* adapter);

/**
 * @brief Get pattern separator handle
 *
 * @param adapter Adapter instance
 * @return Pattern separator (DG), or NULL
 */
pattern_separator_t* hippocampus_get_pattern_separator(hippocampus_adapter_t* adapter);

/**
 * @brief Get memory encoder handle
 *
 * @param adapter Adapter instance
 * @return Memory encoder (CA3/CA1), or NULL
 */
memory_encoder_t* hippocampus_get_memory_encoder(hippocampus_adapter_t* adapter);

/*=============================================================================
 * BIO-ASYNC COMMUNICATION
 *===========================================================================*/

/**
 * @brief Get bio-async module context
 *
 * @param adapter Adapter instance
 * @return Bio-async module context, or NULL if not enabled
 */
bio_module_context_t hippocampus_get_bio_context(hippocampus_adapter_t* adapter);

/**
 * @brief Process pending bio-async messages
 *
 * @param adapter Adapter instance
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t hippocampus_process_bio_messages(hippocampus_adapter_t* adapter, uint32_t max_messages);

/**
 * @brief Request memory encoding asynchronously
 *
 * @param adapter Adapter instance
 * @param features Feature vector
 * @param num_features Number of features
 * @param location Spatial context (optional)
 * @return Future for encoding response, or NULL on failure
 */
nimcp_bio_future_t hippocampus_request_encode_async(
    hippocampus_adapter_t* adapter,
    const float* features,
    uint32_t num_features,
    const hippocampus_location_t* location
);

/**
 * @brief Request memory retrieval asynchronously
 *
 * @param adapter Adapter instance
 * @param cue Query pattern
 * @param cue_size Cue size
 * @return Future for retrieval response, or NULL on failure
 */
nimcp_bio_future_t hippocampus_request_retrieve_async(
    hippocampus_adapter_t* adapter,
    const float* cue,
    uint32_t cue_size
);

/**
 * @brief Broadcast memory encoded event
 *
 * @param adapter Adapter instance
 * @param memory_id ID of encoded memory
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t hippocampus_broadcast_memory_encoded(
    hippocampus_adapter_t* adapter,
    uint32_t memory_id
);

/**
 * @brief Handle incoming consolidation request
 *
 * @param adapter Adapter instance
 * @param from_cortex Request from cortical area
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t hippocampus_handle_consolidation_request(
    hippocampus_adapter_t* adapter,
    bool from_cortex
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HIPPOCAMPUS_ADAPTER_H */
