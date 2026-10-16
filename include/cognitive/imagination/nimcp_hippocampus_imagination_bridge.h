/**
 * @file nimcp_hippocampus_imagination_bridge.h
 * @brief Hippocampus-Imagination Bidirectional Bridge
 * @version 1.0.0
 * @date 2026-01-03
 *
 * WHAT: Bidirectional bridge connecting hippocampus (memory) with imagination engine
 * WHY:  Memory provides content for imagination; imagination creates memories to consolidate
 * HOW:  Full bridge pattern with effects in both directions
 *
 * BIOLOGICAL BASIS:
 * The hippocampus and default mode network (imagination) are tightly coupled:
 * - Memory retrieval provides raw material for imaginative construction
 * - Imagination reactivates and recombines episodic memories
 * - Dreaming (REM) involves hippocampal replay with imaginative recombination
 * - Prospective memory uses imagination to simulate future events
 * - Spatial imagination relies on hippocampal place/grid cells
 *
 * ARCHITECTURE:
 * ```
 * ┌──────────────────────┐                    ┌──────────────────────┐
 * │    HIPPOCAMPUS       │                    │  IMAGINATION ENGINE  │
 * │                      │                    │                      │
 * │ • Place cells        │◄──── memory ──────►│ • Scenario manager   │
 * │ • Grid cells         │      retrieval     │ • Latent space       │
 * │ • Pattern completion │                    │ • World model        │
 * │ • Memory encoding    │◄── consolidation ──│ • Visual generation  │
 * │ • Replay generation  │      signal        │ • Prospective sim    │
 * │                      │                    │                      │
 * └──────────────────────┘                    └──────────────────────┘
 *           │                                           │
 *           └───────────── BRIDGE ──────────────────────┘
 *                    (bidirectional effects)
 * ```
 *
 * EFFECTS:
 * - Hippocampus → Imagination:
 *   • Retrieved memories seed imagination content
 *   • Spatial context provides navigation for imagined scenes
 *   • Pattern completion fills in partial imagination queries
 *
 * - Imagination → Hippocampus:
 *   • Imagined scenarios can be encoded as pseudo-memories
 *   • Consolidation priority for frequently imagined content
 *   • Replay triggering during rest/sleep for dreaming
 *
 * USAGE:
 * ```c
 * hippocampus_imagination_bridge_t* bridge = hippocampus_imagination_bridge_create(NULL);
 * hippocampus_imagination_connect_hippocampus(bridge, hippocampus);
 * hippocampus_imagination_connect_imagination(bridge, imagination_engine);
 *
 * // In update loop:
 * hippocampus_imagination_update(bridge, delta_time);
 *
 * // Request memory-grounded imagination
 * hippocampus_imagination_request_memory_imagination(bridge, memory_cue, &goal);
 * ```
 */

#ifndef NIMCP_HIPPOCAMPUS_IMAGINATION_BRIDGE_H
#define NIMCP_HIPPOCAMPUS_IMAGINATION_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/tensor/nimcp_tensor.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct hippocampus_adapter;
struct imagination_engine;
struct imagination_scenario;
struct imagination_goal;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Maximum memories to retrieve for a single imagination request */
#define HIPP_IMAG_MAX_RETRIEVED_MEMORIES    8

/** Maximum imagination scenarios to track per hippocampus session */
#define HIPP_IMAG_MAX_TRACKED_SCENARIOS     4

/** Default memory relevance threshold */
#define HIPP_IMAG_DEFAULT_RELEVANCE_THRESHOLD   0.3f

/** Default consolidation boost for imagined content */
#define HIPP_IMAG_DEFAULT_CONSOLIDATION_BOOST   1.5f

/*=============================================================================
 * EFFECTS STRUCTURES
 *===========================================================================*/

/**
 * @brief Effects flowing from hippocampus to imagination
 *
 * WHAT: Memory-derived content and context for imagination
 * WHY:  Imagination needs grounding in stored experience
 */
typedef struct {
    /* Memory retrieval effects */
    float memory_relevance;              /**< Relevance of retrieved memories [0.0-1.0] */
    float pattern_completion_strength;   /**< How much pattern completion contributed */
    float spatial_context_strength;      /**< Spatial navigation context strength */

    /* Retrieved memory content */
    uint32_t num_retrieved_memories;     /**< Number of memories retrieved */
    nimcp_tensor_t* retrieved_embeddings; /**< Concatenated memory embeddings */
    float memory_ages[HIPP_IMAG_MAX_RETRIEVED_MEMORIES]; /**< Age of each memory */
    float memory_strengths[HIPP_IMAG_MAX_RETRIEVED_MEMORIES]; /**< Strength of each */

    /* Spatial context for navigation */
    float current_position[3];           /**< Current position (x, y, z) */
    float place_cell_activation;         /**< Place cell activation level */
    float grid_cell_coherence;           /**< Grid cell pattern coherence */

    /* Replay effects */
    bool replay_active;                  /**< Whether replay is currently active */
    float replay_content_influence;      /**< How much replay influences imagination */
} hippocampus_to_imagination_effects_t;

/**
 * @brief Effects flowing from imagination to hippocampus
 *
 * WHAT: Imagination-derived signals for memory encoding/consolidation
 * WHY:  Important imaginations should be remembered
 */
typedef struct {
    /* Consolidation signals */
    float consolidation_priority;        /**< Priority for memory consolidation [0.0-1.0] */
    float encoding_strength;             /**< Strength for encoding imagined as memory */
    bool trigger_replay;                 /**< Should trigger hippocampal replay */

    /* Imagined content to potentially encode */
    uint32_t scenario_id;                /**< Source scenario ID */
    nimcp_tensor_t* imagined_embedding;  /**< Embedding of imagined content */
    float emotional_salience;            /**< Emotional importance of content */
    float novelty_score;                 /**< How novel the imagined content is */

    /* Spatial imagination feedback */
    float imagined_position[3];          /**< Imagined position (for spatial scenes) */
    bool update_spatial_map;             /**< Whether to update spatial representation */

    /* Dreaming effects */
    bool is_dream_content;               /**< Content generated during REM-like state */
    float dream_vividness;               /**< Dream vividness [0.0-1.0] */
} imagination_to_hippocampus_effects_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Memory retrieval parameters */
    float relevance_threshold;           /**< Minimum relevance for retrieval [0.0-1.0] */
    uint32_t max_memories_per_request;   /**< Maximum memories to retrieve per request */
    float pattern_completion_weight;     /**< Weight for pattern completion [0.0-1.0] */

    /* Consolidation parameters */
    float consolidation_boost;           /**< Multiplier for consolidation priority */
    float encoding_threshold;            /**< Threshold for encoding imagined as memory */
    bool enable_pseudo_memory_encoding;  /**< Allow imagined content to become memories */

    /* Replay parameters */
    bool enable_replay_triggering;       /**< Allow imagination to trigger replay */
    float replay_trigger_threshold;      /**< Threshold for triggering replay */

    /* Spatial integration */
    bool enable_spatial_imagination;     /**< Enable spatial context for imagination */
    float spatial_context_weight;        /**< Weight for spatial context influence */

    /* Update frequency */
    float update_interval_ms;            /**< Minimum time between updates */

    /* Bio-async */
    bool enable_bio_async;               /**< Enable bio-async messaging */
} hippocampus_imagination_config_t;

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Memory retrieval stats */
    uint64_t memory_requests;            /**< Total memory retrieval requests */
    uint64_t memories_retrieved;         /**< Total memories retrieved */
    float avg_relevance;                 /**< Average memory relevance */

    /* Consolidation stats */
    uint64_t consolidation_triggers;     /**< Times consolidation was triggered */
    uint64_t pseudo_memories_encoded;    /**< Imagined content encoded as memory */

    /* Replay stats */
    uint64_t replay_triggers;            /**< Times replay was triggered */

    /* Spatial stats */
    uint64_t spatial_queries;            /**< Spatial context queries */

    /* Timing */
    uint64_t total_updates;              /**< Total update calls */
    float avg_update_time_ms;            /**< Average update time */
} hippocampus_imagination_stats_t;

/*=============================================================================
 * MAIN BRIDGE STRUCTURE
 *===========================================================================*/

/**
 * @brief Hippocampus-Imagination bridge
 *
 * Coordinates bidirectional communication between hippocampus and imagination.
 */
typedef struct hippocampus_imagination_bridge {
    bridge_base_t base;                  /**< MUST be first - base bridge infrastructure */

    /* Connected systems (typed for convenience, also in base) */
    struct hippocampus_adapter* hippocampus;
    struct imagination_engine* imagination;

    /* Bidirectional effects */
    hippocampus_to_imagination_effects_t hipp_to_imag;
    imagination_to_hippocampus_effects_t imag_to_hipp;

    /* Configuration */
    hippocampus_imagination_config_t config;

    /* State tracking */
    uint32_t active_scenarios[HIPP_IMAG_MAX_TRACKED_SCENARIOS];
    uint32_t num_active_scenarios;

    /* Pending requests */
    bool memory_request_pending;
    nimcp_tensor_t* pending_query_cue;

    /* Statistics */
    hippocampus_imagination_stats_t stats;

    /* Timing */
    uint64_t last_update_time_ms;
} hippocampus_imagination_bridge_t;

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int hippocampus_imagination_default_config(hippocampus_imagination_config_t* config);

/**
 * @brief Validate configuration
 *
 * @param config Configuration to validate
 * @return 0 if valid, -1 if invalid
 */
int hippocampus_imagination_validate_config(const hippocampus_imagination_config_t* config);

/**
 * @brief Create bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on error
 */
hippocampus_imagination_bridge_t* hippocampus_imagination_bridge_create(
    const hippocampus_imagination_config_t* config);

/**
 * @brief Destroy bridge
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void hippocampus_imagination_bridge_destroy(hippocampus_imagination_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * Clears effects and pending requests, keeps connections and config.
 *
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 */
int hippocampus_imagination_reset(hippocampus_imagination_bridge_t* bridge);

/*=============================================================================
 * CONNECTION API
 *===========================================================================*/

/**
 * @brief Connect hippocampus
 *
 * @param bridge Bridge
 * @param hippocampus Hippocampus adapter to connect
 * @return 0 on success, -1 on error
 */
int hippocampus_imagination_connect_hippocampus(
    hippocampus_imagination_bridge_t* bridge,
    struct hippocampus_adapter* hippocampus);

/**
 * @brief Connect imagination engine
 *
 * @param bridge Bridge
 * @param imagination Imagination engine to connect
 * @return 0 on success, -1 on error
 */
int hippocampus_imagination_connect_imagination(
    hippocampus_imagination_bridge_t* bridge,
    struct imagination_engine* imagination);

/**
 * @brief Disconnect hippocampus
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int hippocampus_imagination_disconnect_hippocampus(
    hippocampus_imagination_bridge_t* bridge);

/**
 * @brief Disconnect imagination
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int hippocampus_imagination_disconnect_imagination(
    hippocampus_imagination_bridge_t* bridge);

/**
 * @brief Check if bridge is fully connected
 *
 * @param bridge Bridge
 * @return true if both systems connected
 */
bool hippocampus_imagination_is_connected(const hippocampus_imagination_bridge_t* bridge);

/*=============================================================================
 * UPDATE API
 *===========================================================================*/

/**
 * @brief Main update function
 *
 * Computes and applies effects in both directions.
 *
 * @param bridge Bridge
 * @param delta_time_ms Time since last update in milliseconds
 * @return 0 on success, -1 on error
 */
int hippocampus_imagination_update(
    hippocampus_imagination_bridge_t* bridge,
    float delta_time_ms);

/**
 * @brief Compute hippocampus → imagination effects only
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int hippocampus_imagination_compute_hipp_effects(
    hippocampus_imagination_bridge_t* bridge);

/**
 * @brief Compute imagination → hippocampus effects only
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int hippocampus_imagination_compute_imag_effects(
    hippocampus_imagination_bridge_t* bridge);

/**
 * @brief Apply all computed effects to connected systems
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int hippocampus_imagination_apply_effects(hippocampus_imagination_bridge_t* bridge);

/*=============================================================================
 * MEMORY-IMAGINATION INTEGRATION API
 *===========================================================================*/

/**
 * @brief Request memory-grounded imagination
 *
 * Uses memory cue to retrieve relevant memories, then initiates
 * an imagination scenario grounded in those memories.
 *
 * @param bridge Bridge
 * @param memory_cue Cue for memory retrieval (latent representation)
 * @param goal Goal for the imagination scenario
 * @return Scenario ID on success, 0 on failure
 */
uint32_t hippocampus_imagination_request_memory_imagination(
    hippocampus_imagination_bridge_t* bridge,
    const nimcp_tensor_t* memory_cue,
    struct imagination_goal* goal);

/**
 * @brief Request spatial imagination
 *
 * Uses place/grid cells to provide spatial context for imagination.
 *
 * @param bridge Bridge
 * @param start_position Starting position for spatial imagination
 * @param goal Goal for the imagination scenario
 * @return Scenario ID on success, 0 on failure
 */
uint32_t hippocampus_imagination_request_spatial_imagination(
    hippocampus_imagination_bridge_t* bridge,
    const float start_position[3],
    struct imagination_goal* goal);

/**
 * @brief Encode imagined content as pseudo-memory
 *
 * Converts an imagination scenario into a memory that can be
 * consolidated and later retrieved.
 *
 * @param bridge Bridge
 * @param scenario Imagination scenario to encode
 * @param emotional_weight Emotional importance [0.0-1.0]
 * @return 0 on success, -1 on error
 */
int hippocampus_imagination_encode_as_memory(
    hippocampus_imagination_bridge_t* bridge,
    const struct imagination_scenario* scenario,
    float emotional_weight);

/**
 * @brief Trigger hippocampal replay from imagination
 *
 * Causes hippocampus to replay memories related to imagination content.
 *
 * @param bridge Bridge
 * @param trigger_cue Cue for replay (from imagination latent state)
 * @return 0 on success, -1 on error
 */
int hippocampus_imagination_trigger_replay(
    hippocampus_imagination_bridge_t* bridge,
    const nimcp_tensor_t* trigger_cue);

/**
 * @brief Get current memory contribution to imagination
 *
 * @param bridge Bridge
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int hippocampus_imagination_get_memory_effects(
    const hippocampus_imagination_bridge_t* bridge,
    hippocampus_to_imagination_effects_t* effects);

/**
 * @brief Get current imagination contribution to memory
 *
 * @param bridge Bridge
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int hippocampus_imagination_get_imagination_effects(
    const hippocampus_imagination_bridge_t* bridge,
    imagination_to_hippocampus_effects_t* effects);

/*=============================================================================
 * QUERY API
 *===========================================================================*/

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int hippocampus_imagination_get_stats(
    const hippocampus_imagination_bridge_t* bridge,
    hippocampus_imagination_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int hippocampus_imagination_reset_stats(hippocampus_imagination_bridge_t* bridge);

/**
 * @brief Get number of active imagination scenarios
 *
 * @param bridge Bridge
 * @return Number of active scenarios
 */
uint32_t hippocampus_imagination_get_active_scenario_count(
    const hippocampus_imagination_bridge_t* bridge);

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int hippocampus_imagination_connect_bio_async(
    hippocampus_imagination_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int hippocampus_imagination_disconnect_bio_async(
    hippocampus_imagination_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge
 * @return true if connected
 */
bool hippocampus_imagination_is_bio_async_connected(
    const hippocampus_imagination_bridge_t* bridge);

/**
 * @brief Process pending bio-async messages
 *
 * @param bridge Bridge
 * @return Number of messages processed
 */
int hippocampus_imagination_process_messages(
    hippocampus_imagination_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HIPPOCAMPUS_IMAGINATION_BRIDGE_H */
