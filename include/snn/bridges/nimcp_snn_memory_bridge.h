/**
 * @file nimcp_snn_memory_bridge.h
 * @brief SNN-Working Memory integration bridge
 *
 * WHAT: Bidirectional bridge between SNN and working memory system
 * WHY:  Enable spike-based memory encoding/retrieval via persistent activity
 * HOW:  Convert spike patterns to memory items, recurrent activity maintains items
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal persistent activity maintains working memory items
 * - Spike patterns encode memory content (population code)
 * - Recurrent connections sustain activity during delay periods
 * - Capacity limited by population size and lateral inhibition (7±2 items)
 *
 * INTEGRATION:
 * - SNN → Memory: Spike patterns encode memory items
 * - SNN → Memory: Persistent activity prevents decay
 * - Memory → SNN: Memory items activate corresponding populations
 * - Memory → SNN: Salience modulates population excitability
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#ifndef NIMCP_SNN_MEMORY_BRIDGE_H
#define NIMCP_SNN_MEMORY_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "cognitive/nimcp_working_memory.h"

//=============================================================================
// Configuration Types
//=============================================================================

/**
 * @brief SNN-Memory bridge configuration
 *
 * WHAT: Parameters for SNN-memory integration
 * WHY:  Control bidirectional encoding/retrieval
 * HOW:  Thresholds, capacity, persistence parameters
 */
typedef struct snn_memory_config_s {
    /* Spike-to-memory encoding */
    float encoding_threshold_rate;  /**< Min spike rate to encode (Hz) */
    float encoding_window_ms;       /**< Time window for encoding */
    uint32_t population_code_size;  /**< Neurons per memory item */
    bool use_population_code;       /**< Enable population coding */

    /* Persistent activity */
    float persistence_rate_min;     /**< Min rate for persistence (Hz) */
    bool enable_recurrent_boost;    /**< Boost recurrent connections */
    float recurrent_weight_scale;   /**< Scale recurrent weights */

    /* Memory-to-spike retrieval */
    float retrieval_boost_factor;   /**< Boost population on retrieval */
    float salience_scaling;         /**< Scale salience to excitability */
    uint32_t retrieval_duration_ms; /**< Duration to sustain retrieval */

    /* Capacity management */
    uint32_t max_memory_items;      /**< Max items (default: 7) */
    bool enforce_capacity_limit;    /**< Evict on overflow */
    float lateral_inhibition;       /**< Inhibition between populations */

    /* Population mapping */
    uint32_t* memory_population_ids; /**< Population IDs for each slot */
    uint32_t num_memory_populations; /**< Number of populations */

    /* Update timing */
    float update_interval_ms;       /**< How often to sync (default: 50ms) */

    /* Bio-async */
    bool enable_bio_async;          /**< Enable bio-async messaging */
} snn_memory_config_t;

/**
 * @brief Spike pattern for memory encoding
 *
 * WHAT: Spike pattern representing a memory item
 * WHY:  Convert spike trains to memory content
 * HOW:  Population vector of spike rates
 */
typedef struct snn_memory_pattern_s {
    float* spike_rates;             /**< Spike rates per neuron */
    uint32_t num_neurons;           /**< Population size */
    uint64_t timestamp_us;          /**< When pattern was captured */
    float pattern_strength;         /**< Overall activity strength */
    bool is_persistent;             /**< Sustained activity detected */
} snn_memory_pattern_t;

/**
 * @brief SNN-Memory bridge state
 *
 * WHAT: Current state of bidirectional integration
 * WHY:  Track encoded items and retrieval state
 * HOW:  Cached patterns and statistics
 */
typedef struct snn_memory_state_s {
    /* Encoding state */
    uint32_t num_encoded_items;     /**< Items currently encoded */
    float* item_persistence;        /**< Persistence strength per item */
    uint64_t* item_timestamps;      /**< When items were encoded */

    /* Retrieval state */
    uint32_t num_active_retrievals; /**< Ongoing retrievals */
    float retrieval_strength;       /**< Current retrieval strength */

    /* Persistent activity tracking */
    float* population_rates;        /**< Current rate per population */
    bool* is_persistent_active;     /**< Per-population persistence */

    /* Statistics */
    uint32_t total_encodings;       /**< Total items encoded */
    uint32_t total_retrievals;      /**< Total retrievals */
    uint32_t capacity_evictions;    /**< Items evicted due to capacity */
    float avg_persistence_rate;     /**< Average persistence rate */
} snn_memory_state_t;

/**
 * @brief SNN-Memory bridge structure
 *
 * WHAT: Context for SNN-memory integration
 * WHY:  Maintain state of bidirectional bridge
 * HOW:  Store references and cached state
 */
typedef struct snn_memory_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* snn;                 /**< SNN network */
    working_memory_t* working_memory;   /**< Working memory system */
    snn_memory_config_t config;         /**< Bridge configuration */
    snn_memory_state_t state;           /**< Current state */

    /* Memory populations */
    snn_population_t** memory_pops;     /**< Populations for each memory slot */

    /* Spike patterns for active items */
    snn_memory_pattern_t* patterns;     /**< Encoded patterns [max_memory_items] */

    /* Timing */
    float last_update_time;             /**< Last update timestamp (ms) */

    /* Bio-async */
    bool bio_async_enabled;             /**< Bio-async connected */
    bio_module_context_t bio_ctx;       /**< Bio-async context */

    /* Mutex for thread safety */
    void* mutex;
} snn_memory_bridge_t;

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Initialize memory bridge config with defaults
 *
 * WHAT: Set biologically-plausible defaults
 * WHY:  Convenient initialization
 * HOW:  Values from working memory neuroscience
 *
 * @param config Config to initialize
 */
void snn_memory_config_default(snn_memory_config_t* config);

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * @brief Create SNN-memory bridge
 *
 * WHAT: Initialize bidirectional bridge
 * WHY:  Enable SNN-memory integration
 * HOW:  Allocate context, set up connections
 *
 * @param config Bridge configuration
 * @param snn SNN network
 * @param working_memory Working memory system
 * @return Bridge instance or NULL on failure
 */
snn_memory_bridge_t* snn_memory_bridge_create(
    const snn_memory_config_t* config,
    snn_network_t* snn,
    working_memory_t* working_memory
);

/**
 * @brief Destroy SNN-memory bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper cleanup
 * HOW:  Disconnect and free
 *
 * @param bridge Bridge to destroy
 */
void snn_memory_bridge_destroy(snn_memory_bridge_t* bridge);

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
int snn_memory_bridge_connect_bio_async(snn_memory_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success
 */
int snn_memory_bridge_disconnect_bio_async(snn_memory_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge to check
 * @return true if connected
 */
bool snn_memory_bridge_is_bio_async_connected(const snn_memory_bridge_t* bridge);

//=============================================================================
// Processing Functions
//=============================================================================

/**
 * @brief Process spike patterns for memory encoding
 *
 * WHAT: Convert spike activity to memory items
 * WHY:  SNN encodes information in memory
 * HOW:  Detect patterns, extract population code, add to memory
 *
 * @param bridge Bridge instance
 * @param input Input spike pattern (can be NULL for current state)
 * @param output Output memory representation
 * @return 0 on success, error code on failure
 */
int snn_memory_bridge_process(
    snn_memory_bridge_t* bridge,
    const float* input,
    float* output
);

/**
 * @brief Update bridge state
 *
 * WHAT: Synchronize SNN and memory systems
 * WHY:  Keep bidirectional signals current
 * HOW:  Update persistence, encoding, retrieval
 *
 * @param bridge Bridge to update
 * @param dt Time step in milliseconds
 * @return 0 on success, error code on failure
 */
int snn_memory_bridge_update(snn_memory_bridge_t* bridge, float dt);

//=============================================================================
// Spike-to-Memory Encoding
//=============================================================================

/**
 * @brief Encode spike pattern to memory item
 *
 * WHAT: Extract spike pattern and add to working memory
 * WHY:  Persistent activity encodes memory
 * HOW:  Compute population vector, add with salience
 *
 * @param bridge Bridge instance
 * @param population_id Population to encode from
 * @param salience Importance of item [0, 1]
 * @return 0 on success, error code on failure
 */
int snn_memory_encode_item(
    snn_memory_bridge_t* bridge,
    uint32_t population_id,
    float salience
);

/**
 * @brief Extract spike pattern from population
 *
 * WHAT: Capture current spike pattern as memory representation
 * WHY:  Population code represents memory content
 * HOW:  Sample spike rates, compute pattern
 *
 * @param bridge Bridge instance
 * @param population Population to extract from
 * @param pattern Output pattern
 * @return 0 on success, error code on failure
 */
int snn_memory_extract_pattern(
    snn_memory_bridge_t* bridge,
    snn_population_t* population,
    snn_memory_pattern_t* pattern
);

/**
 * @brief Check for persistent activity
 *
 * WHAT: Detect sustained firing indicating active maintenance
 * WHY:  Persistent activity is signature of working memory
 * HOW:  Check if rate sustained above threshold
 *
 * @param bridge Bridge instance
 * @param population_id Population to check
 * @return true if persistent activity detected
 */
bool snn_memory_is_persistent(
    snn_memory_bridge_t* bridge,
    uint32_t population_id
);

/**
 * @brief Boost recurrent connections for persistence
 *
 * WHAT: Strengthen recurrent synapses to maintain activity
 * WHY:  Recurrence sustains memory during delays
 * HOW:  Scale recurrent weights
 *
 * @param bridge Bridge instance
 * @param population_id Population to boost
 * @return 0 on success, error code on failure
 */
int snn_memory_boost_recurrence(
    snn_memory_bridge_t* bridge,
    uint32_t population_id
);

//=============================================================================
// Memory-to-Spike Retrieval
//=============================================================================

/**
 * @brief Retrieve memory item and activate population
 *
 * WHAT: Reactivate spike pattern from memory
 * WHY:  Memory recall drives spike activity
 * HOW:  Inject current to population based on stored pattern
 *
 * @param bridge Bridge instance
 * @param item_index Memory item index
 * @return 0 on success, error code on failure
 */
int snn_memory_retrieve_item(
    snn_memory_bridge_t* bridge,
    uint32_t item_index
);

/**
 * @brief Activate population from memory pattern
 *
 * WHAT: Drive population to match stored pattern
 * WHY:  Reactivation is neural basis of recall
 * HOW:  Inject currents matching pattern rates
 *
 * @param bridge Bridge instance
 * @param population Population to activate
 * @param pattern Pattern to reactivate
 * @return 0 on success, error code on failure
 */
int snn_memory_activate_pattern(
    snn_memory_bridge_t* bridge,
    snn_population_t* population,
    const snn_memory_pattern_t* pattern
);

/**
 * @brief Modulate population by salience
 *
 * WHAT: Scale excitability by memory salience
 * WHY:  Important items have stronger reactivation
 * HOW:  Boost input based on salience
 *
 * @param bridge Bridge instance
 * @param population_id Population to modulate
 * @param salience Salience value [0, 1]
 * @return 0 on success, error code on failure
 */
int snn_memory_modulate_by_salience(
    snn_memory_bridge_t* bridge,
    uint32_t population_id,
    float salience
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get current bridge state
 *
 * @param bridge Bridge to query
 * @param state Output state (copied)
 * @return 0 on success
 */
int snn_memory_bridge_get_state(
    const snn_memory_bridge_t* bridge,
    snn_memory_state_t* state
);

/**
 * @brief Get number of encoded items
 *
 * @param bridge Bridge to query
 * @return Number of active memory items
 */
uint32_t snn_memory_get_num_items(const snn_memory_bridge_t* bridge);

/**
 * @brief Check if population has persistent activity
 *
 * @param bridge Bridge to query
 * @param population_id Population to check
 * @return true if persistent
 */
bool snn_memory_has_persistent_activity(
    const snn_memory_bridge_t* bridge,
    uint32_t population_id
);

/**
 * @brief Get persistence strength for item
 *
 * @param bridge Bridge to query
 * @param item_index Item index
 * @return Persistence strength [0, 1]
 */
float snn_memory_get_persistence_strength(
    const snn_memory_bridge_t* bridge,
    uint32_t item_index
);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge to query
 * @param encodings Output: total encodings
 * @param retrievals Output: total retrievals
 * @param evictions Output: capacity evictions
 * @return 0 on success
 */
int snn_memory_get_stats(
    const snn_memory_bridge_t* bridge,
    uint32_t* encodings,
    uint32_t* retrievals,
    uint32_t* evictions
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge to reset
 */
void snn_memory_reset_stats(snn_memory_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_MEMORY_BRIDGE_H */
