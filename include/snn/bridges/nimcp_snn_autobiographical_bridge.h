/**
 * @file nimcp_snn_autobiographical_bridge.h
 * @brief SNN-Autobiographical Memory integration bridge
 *
 * WHAT: Bidirectional bridge between SNN and autobiographical memory system
 * WHY:  Enable spike-based episodic encoding and retrieval
 * HOW:  Convert experiences to spike sequences, retrieve via pattern completion
 *
 * BIOLOGICAL BASIS:
 * - Hippocampus encodes episodes via spike sequences (place cell replay)
 * - Neocortex stores consolidated memories via distributed patterns
 * - Retrieval is pattern completion from partial cues
 * - Consolidation strengthens synapses during offline replay
 *
 * INTEGRATION:
 * - SNN → Autobiographical: Spike sequences encode episodes
 * - SNN → Autobiographical: Replay consolidates memories
 * - Autobiographical → SNN: Retrieval cues trigger pattern completion
 * - Autobiographical → SNN: Recollection reactivates original spike patterns
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#ifndef NIMCP_SNN_AUTOBIOGRAPHICAL_BRIDGE_H
#define NIMCP_SNN_AUTOBIOGRAPHICAL_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"

//=============================================================================
// Configuration Types
//=============================================================================

/**
 * @brief Memory encoding strength
 *
 * WHAT: How strongly an episode is encoded
 * WHY:  Emotional/salient events encode more strongly
 * HOW:  Modulates synaptic strengthening
 */
typedef enum {
    SNN_ENCODING_WEAK = 0,      /**< Weak encoding (quickly forgotten) */
    SNN_ENCODING_MODERATE,      /**< Moderate encoding (normal memory) */
    SNN_ENCODING_STRONG,        /**< Strong encoding (salient event) */
    SNN_ENCODING_VIVID          /**< Vivid encoding (flashbulb memory) */
} snn_encoding_strength_t;

/**
 * @brief SNN-Autobiographical bridge configuration
 *
 * WHAT: Parameters for episodic memory integration
 * WHY:  Control encoding, retrieval, consolidation
 * HOW:  Thresholds and strength factors
 */
typedef struct snn_autobiographical_config_s {
    /* Encoding parameters */
    float encoding_threshold;       /**< Min spike rate for encoding (Hz) */
    float encoding_window_ms;       /**< Time window for episode capture */
    float salience_boost_factor;    /**< Boost encoding for salient events */
    uint32_t max_sequence_length;   /**< Max spike sequence length */

    /* Retrieval parameters */
    float retrieval_cue_strength;   /**< Min cue strength for retrieval [0, 1] */
    float pattern_completion_threshold; /**< Min overlap for completion [0, 1] */
    float retrieval_boost_factor;   /**< Boost recall activation */
    bool enable_partial_retrieval;  /**< Allow retrieval from partial cues */

    /* Consolidation parameters */
    float consolidation_rate;       /**< Synaptic strengthening rate */
    float replay_probability;       /**< Probability of offline replay */
    uint32_t consolidation_cycles;  /**< Number of replay cycles */
    bool enable_reconsolidation;    /**< Allow memory updating */

    /* Population mapping */
    uint32_t hippocampal_population_id; /**< Hippocampus (encoding) */
    uint32_t cortical_population_id;    /**< Cortex (storage) */
    uint32_t amygdala_population_id;    /**< Amygdala (emotional salience) */

    /* Memory capacity */
    uint32_t max_memories;          /**< Max autobiographical memories */

    /* Update timing */
    float update_interval_ms;       /**< Processing update rate */

    /* Bio-async */
    bool enable_bio_async;          /**< Enable bio-async messaging */
} snn_autobiographical_config_t;

/**
 * @brief Episodic memory representation
 *
 * WHAT: Stored autobiographical memory
 * WHY:  Track encoded episodes
 * HOW:  Spike sequence + metadata
 */
typedef struct snn_episodic_memory_s {
    uint32_t memory_id;             /**< Unique memory ID */
    float* spike_sequence;          /**< Encoded spike pattern */
    uint32_t sequence_length;       /**< Length of sequence */
    snn_encoding_strength_t encoding_strength; /**< Encoding strength */
    float emotional_valence;        /**< Emotional valence [-1, 1] */
    float consolidation_level;      /**< Consolidation progress [0, 1] */
    uint32_t retrieval_count;       /**< Number of retrievals */
    float last_access_time;         /**< Last retrieval time (ms) */
    bool is_consolidated;           /**< Fully consolidated flag */
} snn_episodic_memory_t;

/**
 * @brief Autobiographical memory state
 *
 * WHAT: Current state of memory system
 * WHY:  Track encoding, retrieval, consolidation
 * HOW:  Counters and metrics
 */
typedef struct snn_autobiographical_state_s {
    /* Encoding state */
    float encoding_strength;        /**< Current encoding strength [0, 1] */
    uint32_t encoding_count;        /**< Total encodings */
    bool encoding_active;           /**< Currently encoding */

    /* Retrieval state */
    float retrieval_success_rate;   /**< Retrieval success rate [0, 1] */
    uint32_t retrieval_attempts;    /**< Total retrieval attempts */
    uint32_t retrieval_successes;   /**< Successful retrievals */
    uint32_t active_memory_id;      /**< Currently retrieved memory */

    /* Consolidation state */
    float consolidation_progress;   /**< Overall consolidation [0, 1] */
    uint32_t replay_count;          /**< Total replay events */
    uint32_t memories_consolidated; /**< Fully consolidated memories */

    /* Memory statistics */
    uint32_t memory_count;          /**< Total stored memories */
    float avg_encoding_strength;    /**< Average encoding strength */
    float avg_consolidation;        /**< Average consolidation level */

    /* Update count */
    uint32_t update_count;          /**< Total updates */
} snn_autobiographical_state_t;

/**
 * @brief SNN-Autobiographical bridge structure
 *
 * WHAT: Context for SNN-autobiographical memory integration
 * WHY:  Maintain state of memory bridge
 * HOW:  Store references, memories, and state
 */
typedef struct snn_autobiographical_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* snn;                     /**< SNN network */
    snn_autobiographical_config_t config;   /**< Bridge configuration */
    snn_autobiographical_state_t state;     /**< Current state */

    /* Populations */
    snn_population_t* hippocampal_pop;      /**< Hippocampus */
    snn_population_t* cortical_pop;         /**< Cortex */
    snn_population_t* amygdala_pop;         /**< Amygdala */

    /* Memory storage */
    snn_episodic_memory_t* memories;        /**< Array of memories */
    uint32_t memory_capacity;               /**< Allocated capacity */

    /* Timing */
    float last_update_time;                 /**< Last update timestamp (ms) */
    float total_time;                       /**< Total time (ms) */

    /* Bio-async */
    bool bio_async_enabled;                 /**< Bio-async connected */
    bio_module_context_t bio_ctx;           /**< Bio-async context */

    /* Mutex for thread safety */
    void* mutex;
} snn_autobiographical_bridge_t;

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Initialize autobiographical bridge config with defaults
 *
 * WHAT: Set biologically-plausible defaults
 * WHY:  Convenient initialization
 * HOW:  Values from memory neuroscience literature
 *
 * @param config Config to initialize
 */
void snn_autobiographical_config_default(snn_autobiographical_config_t* config);

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * @brief Create SNN-autobiographical bridge
 *
 * WHAT: Initialize memory bridge
 * WHY:  Enable episodic memory system
 * HOW:  Allocate context, memory storage
 *
 * @param config Bridge configuration
 * @param snn SNN network
 * @return Bridge instance or NULL on failure
 */
snn_autobiographical_bridge_t* snn_autobiographical_bridge_create(
    const snn_autobiographical_config_t* config,
    snn_network_t* snn
);

/**
 * @brief Destroy SNN-autobiographical bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper cleanup
 * HOW:  Free memories, disconnect, free
 *
 * @param bridge Bridge to destroy
 */
void snn_autobiographical_bridge_destroy(snn_autobiographical_bridge_t* bridge);

/**
 * @brief Connect bridge to bio-async
 *
 * WHAT: Enable bio-async messaging
 * WHY:  Distributed memory coordination
 * HOW:  Register with bio-router
 *
 * @param bridge Bridge to connect
 * @return 0 on success, error code on failure
 */
int snn_autobiographical_bridge_connect_bio_async(snn_autobiographical_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success
 */
int snn_autobiographical_bridge_disconnect_bio_async(snn_autobiographical_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge to check
 * @return true if connected
 */
bool snn_autobiographical_bridge_is_bio_async_connected(const snn_autobiographical_bridge_t* bridge);

//=============================================================================
// Processing Functions
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Process encoding, retrieval, consolidation
 * WHY:  Maintain memory system
 * HOW:  Update ongoing operations
 *
 * @param bridge Bridge to update
 * @param dt Time step in milliseconds
 * @return 0 on success, error code on failure
 */
int snn_autobiographical_bridge_update(snn_autobiographical_bridge_t* bridge, float dt);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode current episode
 *
 * WHAT: Capture current spike pattern as memory
 * WHY:  Store episodic experience
 * HOW:  Record spike sequence, compute encoding strength
 *
 * @param bridge Bridge instance
 * @param encoding_strength Encoding strength modifier
 * @param emotional_valence Emotional valence [-1, 1]
 * @param memory_id Output: ID of encoded memory
 * @return 0 on success, error code on failure
 */
int snn_autobiographical_encode_episode(
    snn_autobiographical_bridge_t* bridge,
    snn_encoding_strength_t encoding_strength,
    float emotional_valence,
    uint32_t* memory_id
);

/**
 * @brief Compute encoding strength
 *
 * WHAT: Determine how strongly to encode current episode
 * WHY:  Salient/emotional events encode more strongly
 * HOW:  Combine spike rate, amygdala activity, attention
 *
 * @param bridge Bridge instance
 * @return Encoding strength [0, 1]
 */
float snn_autobiographical_compute_encoding_strength(
    snn_autobiographical_bridge_t* bridge
);

//=============================================================================
// Retrieval Functions
//=============================================================================

/**
 * @brief Retrieve memory from cue
 *
 * WHAT: Recall memory from partial spike pattern
 * WHY:  Pattern completion enables recollection
 * HOW:  Match cue to stored sequences, complete pattern
 *
 * @param bridge Bridge instance
 * @param cue_pattern Retrieval cue spike pattern
 * @param cue_length Length of cue
 * @param memory_id Output: Retrieved memory ID (if successful)
 * @return 0 on success, error code on failure
 */
int snn_autobiographical_retrieve_memory(
    snn_autobiographical_bridge_t* bridge,
    const float* cue_pattern,
    uint32_t cue_length,
    uint32_t* memory_id
);

/**
 * @brief Compute retrieval success rate
 *
 * WHAT: Calculate probability of successful retrieval
 * WHY:  Monitor memory system performance
 * HOW:  Ratio of successes to attempts
 *
 * @param bridge Bridge instance
 * @return Success rate [0, 1]
 */
float snn_autobiographical_get_retrieval_success_rate(
    const snn_autobiographical_bridge_t* bridge
);

//=============================================================================
// Consolidation Functions
//=============================================================================

/**
 * @brief Consolidate memory
 *
 * WHAT: Strengthen synapses for long-term storage
 * WHY:  Consolidation moves memories from hippocampus to cortex
 * HOW:  Replay spike sequence, strengthen connections
 *
 * @param bridge Bridge instance
 * @param memory_id Memory to consolidate
 * @return 0 on success, error code on failure
 */
int snn_autobiographical_consolidate_memory(
    snn_autobiographical_bridge_t* bridge,
    uint32_t memory_id
);

/**
 * @brief Trigger offline replay
 *
 * WHAT: Replay stored sequences during rest/sleep
 * WHY:  Replay consolidates memories
 * HOW:  Reactivate spike patterns at accelerated speed
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int snn_autobiographical_replay_memories(
    snn_autobiographical_bridge_t* bridge
);

//=============================================================================
// Memory Management
//=============================================================================

/**
 * @brief Get memory by ID
 *
 * @param bridge Bridge instance
 * @param memory_id Memory ID
 * @return Pointer to memory or NULL if not found
 */
snn_episodic_memory_t* snn_autobiographical_get_memory(
    snn_autobiographical_bridge_t* bridge,
    uint32_t memory_id
);

/**
 * @brief Delete memory
 *
 * @param bridge Bridge instance
 * @param memory_id Memory to delete
 * @return 0 on success, error code on failure
 */
int snn_autobiographical_delete_memory(
    snn_autobiographical_bridge_t* bridge,
    uint32_t memory_id
);

/**
 * @brief Clear all memories
 *
 * @param bridge Bridge instance
 */
void snn_autobiographical_clear_memories(snn_autobiographical_bridge_t* bridge);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get memory count
 *
 * @param bridge Bridge to query
 * @return Number of stored memories
 */
uint32_t snn_autobiographical_get_memory_count(const snn_autobiographical_bridge_t* bridge);

/**
 * @brief Get encoding strength
 *
 * @param bridge Bridge to query
 * @return Current encoding strength [0, 1]
 */
float snn_autobiographical_get_encoding_strength(const snn_autobiographical_bridge_t* bridge);

/**
 * @brief Get consolidation progress
 *
 * @param bridge Bridge to query
 * @return Consolidation progress [0, 1]
 */
float snn_autobiographical_get_consolidation_progress(const snn_autobiographical_bridge_t* bridge);

/**
 * @brief Get bridge state
 *
 * @param bridge Bridge to query
 * @param state Output state (copied)
 * @return 0 on success
 */
int snn_autobiographical_bridge_get_state(
    const snn_autobiographical_bridge_t* bridge,
    snn_autobiographical_state_t* state
);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get memory statistics
 *
 * @param bridge Bridge to query
 * @param memory_count Output: total memories
 * @param encoding_count Output: total encodings
 * @param retrieval_success_rate Output: retrieval success rate
 * @return 0 on success
 */
int snn_autobiographical_get_stats(
    const snn_autobiographical_bridge_t* bridge,
    uint32_t* memory_count,
    uint32_t* encoding_count,
    float* retrieval_success_rate
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge to reset
 */
void snn_autobiographical_reset_stats(snn_autobiographical_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_AUTOBIOGRAPHICAL_BRIDGE_H */
