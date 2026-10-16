/**
 * @file nimcp_recovery_episodic_memory.h
 * @brief Episodic Memory for Recovery History - Content-Addressable Experience Storage
 *
 * WHAT: Circular buffer storing recovery episodes with LSH-based similarity search
 * WHY:  Enable learning from recovery experience via analogical recall
 * HOW:  Circular buffer (10K episodes) + LSH indexing + emotional tagging + consolidation
 *
 * BIOLOGICAL BASIS:
 * - Hippocampal episodic memory formation and retrieval
 * - Amygdala emotional tagging for prioritization
 * - Sleep-based consolidation to semantic memory
 * - Content-addressable recall via pattern completion
 *
 * ARCHITECTURE:
 * - Storage: O(1) insertion into circular buffer
 * - Retrieval: O(log N) LSH-based similarity search
 * - Capacity: 10,000 episodes (configurable)
 * - Eviction: FIFO with emotional priority override
 * - Memory: ~16MB for 10K episodes
 *
 * INTEGRATION POINTS:
 * - Executive Function: Query for recovery strategy selection
 * - Working Memory: Recent episodes cached for active context
 * - Attention: Emotional tags influence retrieval priority
 * - Semantic Memory: Consolidation extracts general patterns
 * - Predictive Coding: Historical patterns inform predictions
 *
 * EXAMPLE USAGE:
 * ```c
 * // Create episodic memory
 * episodic_memory_t* memory = episodic_memory_create_default();
 *
 * // Store successful recovery
 * recovery_episode_t episode = {
 *     .episode_id = 1,
 *     .error_sig = {ERROR_TYPE_SIGSEGV, 0x1234, hash},
 *     .strategy_type = STRATEGY_RELOAD_CHECKPOINT,
 *     .success = true,
 *     .recovery_time_us = 15000,
 *     .emotional_tag = 0.8  // Relief - it worked!
 * };
 * episodic_memory_store(memory, &episode);
 *
 * // Later: recall similar failures
 * error_signature_t query = {ERROR_TYPE_SIGSEGV, 0x1240, hash};
 * uint32_t count;
 * recovery_episode_t** similar = episodic_memory_recall_similar(
 *     memory, &query, 5, &count);
 *
 * // Result: "I've seen 5 similar SIGSEGV crashes,
 * //          4 succeeded with checkpoint reload (80% success rate)"
 *
 * // Consolidate to semantic memory
 * episodic_memory_consolidate(memory);
 * ```
 *
 * PERFORMANCE:
 * - Storage: < 100us per episode (O(1))
 * - Search: < 1ms for similarity query (O(log N))
 * - Memory: ~1.6KB per episode (avg)
 * - Consolidation: Background async, no latency impact
 *
 * @author NIMCP Development Team
 * @date 2025-01-09
 * @version 2.7.0 Phase 10.1
 */

#ifndef NIMCP_RECOVERY_EPISODIC_MEMORY_H
#define NIMCP_RECOVERY_EPISODIC_MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration and Constants
//=============================================================================

/**
 * @brief Default capacity for episodic memory (per architecture spec)
 */
#define EPISODIC_MEMORY_DEFAULT_CAPACITY 10000

/**
 * @brief Default LSH parameters for content-addressable recall
 */
#define EPISODIC_MEMORY_DEFAULT_LSH_TABLES 8
#define EPISODIC_MEMORY_DEFAULT_LSH_HASHES 16

/**
 * @brief Default consolidation threshold (minimum episodes for pattern extraction)
 */
#define EPISODIC_MEMORY_DEFAULT_CONSOLIDATION_THRESHOLD 10

//=============================================================================
// Error Types and Recovery Strategies
//=============================================================================

/**
 * @brief Error type classification
 *
 * WHAT: Categorization of fault types for pattern matching
 * WHY:  Enable similarity-based retrieval
 */
typedef enum {
    ERROR_TYPE_UNKNOWN = 0,
    ERROR_TYPE_SIGSEGV,           /**< Segmentation fault */
    ERROR_TYPE_SIGFPE,            /**< Floating point exception */
    ERROR_TYPE_SIGILL,            /**< Illegal instruction */
    ERROR_TYPE_KERNEL_PANIC,      /**< Kernel panic */
    ERROR_TYPE_DATA_CORRUPTION,   /**< Data corruption detected */
    ERROR_TYPE_TIMEOUT,           /**< Operation timeout */
    ERROR_TYPE_OOM,               /**< Out of memory */
    ERROR_TYPE_GRADIENT_EXPLOSION,/**< Gradient explosion (training) */
    ERROR_TYPE_NAN_DETECTED,      /**< NaN in computation */
    ERROR_TYPE_DIVERGENCE,        /**< Model divergence */
    ERROR_TYPE_COUNT              /**< Total error types */
} error_type_t;

/**
 * @brief Recovery strategy types
 *
 * WHAT: Actions taken to recover from errors
 * WHY:  Track which strategies work for which errors
 */
typedef enum {
    STRATEGY_UNKNOWN = 0,
    STRATEGY_RETRY,               /**< Simple retry */
    STRATEGY_RELOAD_CHECKPOINT,   /**< Reload from checkpoint */
    STRATEGY_REDUCE_LOAD,         /**< Reduce computational load */
    STRATEGY_CPU_FALLBACK,        /**< Fall back to CPU */
    STRATEGY_EMERGENCY_SHUTDOWN,  /**< Emergency shutdown */
    STRATEGY_REDUCE_LR,           /**< Reduce learning rate */
    STRATEGY_GRADIENT_CLIP,       /**< Apply gradient clipping */
    STRATEGY_RESET_STATE,         /**< Reset internal state */
    STRATEGY_MULTI_STEP,          /**< Multi-step recovery plan */
    STRATEGY_COUNT                /**< Total strategy types */
} recovery_strategy_type_t;

//=============================================================================
// Core Data Structures
//=============================================================================

/**
 * @brief Error signature for pattern matching
 *
 * WHAT: Compact representation of error for similarity search
 * WHY:  Enable content-addressable recall
 * HOW:  Hash-based signature with type and code
 */
typedef struct {
    error_type_t error_type;      /**< Error category */
    uint32_t error_code;          /**< Specific error code */
    uint64_t signature_hash;      /**< Combined hash for LSH */
} error_signature_t;

/**
 * @brief Brain state snapshot (minimal representation)
 *
 * WHAT: Context at time of error
 * WHY:  Include environmental factors in similarity matching
 */
typedef struct {
    uint32_t neuron_count;        /**< Active neurons */
    float avg_activity;           /**< Average neural activity */
    float learning_rate;          /**< Current learning rate */
    uint32_t training_step;       /**< Training iteration */
} brain_state_snapshot_t;

/**
 * @brief Recovery episode - complete record of recovery attempt
 *
 * WHAT: Full context of one recovery experience
 * WHY:  Store all information needed for learning
 * HOW:  Immutable record with error, action, outcome
 *
 * SIZE: ~200 bytes per episode (without extension data)
 */
typedef struct {
    // ======== Episode Identification ========
    uint64_t episode_id;          /**< Unique episode ID */
    uint64_t timestamp;           /**< Time of occurrence (ms) */

    // ======== Error Context ========
    error_signature_t error_sig;  /**< Error signature for matching */
    brain_state_snapshot_t brain_state; /**< Brain state at error time */
    char error_message[128];      /**< Human-readable error (optional) */

    // ======== Recovery Action ========
    recovery_strategy_type_t strategy_type; /**< Strategy used */
    uint32_t num_recovery_steps; /**< Number of steps (multi-step plans) */
    float strategy_params[4];     /**< Strategy-specific parameters */

    // ======== Outcome ========
    bool success;                 /**< Did recovery succeed? */
    uint64_t recovery_time_us;    /**< Time taken (microseconds) */
    float success_confidence;     /**< Confidence in success [0,1] */

    // ======== Emotional Tagging ========
    float emotional_tag;          /**< Valence: -1.0 (fear) to +1.0 (relief) */

    // ======== Metadata ========
    uint32_t replay_count;        /**< Times replayed for learning */
    bool consolidated;            /**< Transferred to semantic memory? */
} recovery_episode_t;

//=============================================================================
// Opaque Handle
//=============================================================================

/**
 * @brief Opaque episodic memory handle
 *
 * WHAT: Private implementation structure
 * WHY:  Encapsulation and ABI stability
 */
typedef struct episodic_memory episodic_memory_t;

//=============================================================================
// Configuration Structure
//=============================================================================

/**
 * @brief Episodic memory configuration
 *
 * WHAT: Customizable parameters for memory system
 * WHY:  Support different capacity and performance requirements
 */
typedef struct {
    // ======== Capacity ========
    uint32_t max_episodes;        /**< Maximum episodes (default: 10000) */

    // ======== LSH Configuration ========
    uint32_t lsh_num_tables;      /**< Number of LSH tables (default: 8) */
    uint32_t lsh_num_hashes;      /**< Hashes per table (default: 16) */

    // ======== Emotional Tagging ========
    bool enable_emotional_tagging; /**< Enable emotion-based prioritization */
    bool enable_emotional_eviction; /**< Evict low-emotion first */

    // ======== Consolidation ========
    bool enable_consolidation;    /**< Enable semantic consolidation */
    uint32_t consolidation_threshold; /**< Min episodes for pattern (default: 10) */

} episodic_memory_config_t;

//=============================================================================
// Statistics and Results
//=============================================================================

/**
 * @brief Episodic memory statistics
 *
 * WHAT: Performance and usage metrics
 * WHY:  Monitor memory health and performance
 */
typedef struct {
    uint32_t current_episode_count; /**< Current episodes stored */
    uint32_t total_episodes_stored; /**< Total ever stored (incl. evicted) */
    uint32_t total_queries;         /**< Total similarity queries */
    uint64_t total_storage_time_us; /**< Cumulative storage time */
    uint64_t total_query_time_us;   /**< Cumulative query time */
    uint32_t consolidation_runs;    /**< Times consolidation run */
    uint32_t patterns_extracted;    /**< Total semantic patterns */
} episodic_memory_stats_t;

/**
 * @brief Consolidation result
 *
 * WHAT: Output of consolidation process
 * WHY:  Report patterns extracted
 */
typedef struct {
    bool success;                 /**< Consolidation succeeded? */
    uint32_t patterns_extracted;  /**< Number of patterns found */
    float confidence;             /**< Average pattern confidence */
} consolidation_result_t;

/**
 * @brief Episode replay result
 *
 * WHAT: Result of replaying episode
 * WHY:  Support replay-based learning
 */
typedef struct {
    bool success;                 /**< Replay succeeded? */
    uint64_t episode_id;          /**< Episode replayed */
} replay_result_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default episodic memory configuration
 *
 * WHAT: Return sensible defaults for all parameters
 * WHY:  Simplify creation with working defaults
 * HOW:  Static initialization of config struct
 *
 * COMPLEXITY: O(1)
 * MEMORY: O(1)
 *
 * @return Default configuration
 */
episodic_memory_config_t episodic_memory_default_config(void);

//=============================================================================
// Creation and Destruction
//=============================================================================

/**
 * @brief Create episodic memory with default configuration
 *
 * WHAT: Allocate and initialize episodic memory system
 * WHY:  Simplified creation for common use case
 * HOW:  Uses default capacity (10K episodes)
 *
 * COMPLEXITY: O(N) where N = max_episodes (buffer allocation)
 * MEMORY: ~16MB for 10K episodes
 *
 * @return Episodic memory handle or NULL on failure
 */
episodic_memory_t* episodic_memory_create_default(void);

/**
 * @brief Create episodic memory with custom configuration
 *
 * WHAT: Allocate and initialize with user-specified parameters
 * WHY:  Support different capacity and performance requirements
 * HOW:  Allocates circular buffer, LSH tables, tracking structures
 *
 * COMPLEXITY: O(N) where N = config->max_episodes
 * MEMORY: ~1.6KB per episode capacity
 *
 * @param config Configuration parameters (non-NULL)
 * @return Episodic memory handle or NULL on failure
 */
episodic_memory_t* episodic_memory_create_custom(
    const episodic_memory_config_t* config);

/**
 * @brief Destroy episodic memory and free all resources
 *
 * WHAT: Free all memory used by episodic memory system
 * WHY:  Prevent memory leaks
 * HOW:  Frees circular buffer, LSH tables, all episodes
 *
 * COMPLEXITY: O(N) where N = current episode count
 * MEMORY: Frees all allocated memory
 *
 * @param memory Episodic memory to destroy (NULL-safe)
 */
void episodic_memory_destroy(episodic_memory_t* memory);

//=============================================================================
// Episode Storage
//=============================================================================

/**
 * @brief Store recovery episode in episodic memory
 *
 * WHAT: Add episode to circular buffer and LSH index
 * WHY:  Record experience for future learning
 * HOW:  Insert into buffer (evict oldest if full), index in LSH tables
 *
 * ALGORITHM:
 * 1. Validate episode (NULL check, ID check)
 * 2. If at capacity, evict oldest (or lowest-emotion if enabled)
 * 3. Copy episode to buffer slot
 * 4. Index in LSH tables for fast retrieval
 * 5. Update statistics
 *
 * COMPLEXITY: O(1) average (O(K) for LSH indexing, K = num_tables)
 * MEMORY: O(1) - reuses buffer slots
 *
 * @param memory Episodic memory instance (non-NULL)
 * @param episode Episode to store (non-NULL)
 * @return true on success, false on invalid parameters
 *
 * @note Episode is deep-copied; caller retains ownership
 * @note If episode_id is 0, auto-assigns unique ID
 * @note Emotional tags clamped to [-1.0, +1.0]
 */
bool episodic_memory_store(
    episodic_memory_t* memory,
    const recovery_episode_t* episode);

//=============================================================================
// Content-Addressable Retrieval (LSH-Based)
//=============================================================================

/**
 * @brief Recall similar episodes using content-addressable search
 *
 * WHAT: Find episodes with similar error signatures
 * WHY:  Enable analogical reasoning ("I've seen this before")
 * HOW:  LSH-based approximate nearest neighbor search
 *
 * ALGORITHM:
 * 1. Hash query signature using LSH
 * 2. Lookup candidate episodes in LSH buckets
 * 3. Rank by similarity score
 * 4. Return top-K most similar
 *
 * COMPLEXITY: O(log N) average with LSH
 * MEMORY: O(K) where K = max_results
 *
 * @param memory Episodic memory instance (non-NULL)
 * @param query Error signature to match (non-NULL)
 * @param max_results Maximum episodes to return
 * @param count Output: number of episodes found (non-NULL)
 * @return Array of episode pointers (caller must free) or NULL
 *
 * @note Returns NULL if no matches found
 * @note Returned episodes are pointers to internal data (read-only)
 * @note Caller must free returned array with nimcp_free()
 * @note Episodes sorted by similarity (most similar first)
 */
recovery_episode_t** episodic_memory_recall_similar(
    episodic_memory_t* memory,
    const error_signature_t* query,
    uint32_t max_results,
    uint32_t* count);

//=============================================================================
// Episode Replay
//=============================================================================

/**
 * @brief Replay specific episode for learning
 *
 * WHAT: Re-activate episode for consolidation or training
 * WHY:  Support replay-based memory consolidation
 * HOW:  Increment replay count, mark for processing
 *
 * COMPLEXITY: O(log N) to find episode by ID
 * MEMORY: O(1)
 *
 * @param memory Episodic memory instance (non-NULL)
 * @param episode_id ID of episode to replay
 * @return Replay result (success/failure)
 *
 * @note Increments replay_count for episode
 * @note Used during sleep/idle for consolidation
 */
replay_result_t episodic_memory_replay(
    episodic_memory_t* memory,
    uint64_t episode_id);

//=============================================================================
// Consolidation (Episodic → Semantic)
//=============================================================================

/**
 * @brief Consolidate episodic patterns to semantic memory
 *
 * WHAT: Extract general patterns from specific episodes
 * WHY:  Build semantic rules from experience
 * HOW:  Cluster similar episodes, extract common patterns
 *
 * ALGORITHM:
 * 1. Group episodes by error signature similarity
 * 2. For each group with count >= threshold:
 *    - Calculate success rate for each strategy
 *    - If pattern reliable (>70% success), create semantic rule
 * 3. Mark consolidated episodes
 *
 * EXAMPLE:
 * Input: 20 episodes of "SIGSEGV at 0x5000", 18 succeeded with RELOAD_CHECKPOINT
 * Output: Semantic rule "SIGSEGV near 0x5000 → RELOAD_CHECKPOINT (90% success)"
 *
 * COMPLEXITY: O(N log N) where N = episode count
 * MEMORY: O(P) where P = patterns extracted
 *
 * @param memory Episodic memory instance (non-NULL)
 * @return Consolidation result with pattern count
 *
 * @note Typically run during "sleep" or idle periods
 * @note Does not modify or delete episodes
 * @note Marks episodes as consolidated to avoid re-processing
 */
consolidation_result_t episodic_memory_consolidate(
    episodic_memory_t* memory);

//=============================================================================
// Getters and Statistics
//=============================================================================

/**
 * @brief Get current episode count
 *
 * WHAT: Number of episodes currently stored
 * WHY:  Monitor memory usage
 * HOW:  Return current buffer size
 *
 * COMPLEXITY: O(1)
 *
 * @param memory Episodic memory instance
 * @return Current episode count (0 if NULL)
 */
uint32_t episodic_memory_get_count(const episodic_memory_t* memory);

/**
 * @brief Get maximum capacity
 *
 * WHAT: Maximum episodes that can be stored
 * WHY:  Understand memory limits
 * HOW:  Return configured capacity
 *
 * COMPLEXITY: O(1)
 *
 * @param memory Episodic memory instance
 * @return Maximum capacity (0 if NULL)
 */
uint32_t episodic_memory_get_capacity(const episodic_memory_t* memory);

/**
 * @brief Get all stored episodes
 *
 * WHAT: Retrieve all current episodes
 * WHY:  Analysis and debugging
 * HOW:  Copy episode pointers to array
 *
 * COMPLEXITY: O(N) where N = current count
 * MEMORY: O(N)
 *
 * @param memory Episodic memory instance (non-NULL)
 * @param count Output: number of episodes (non-NULL)
 * @return Array of episode pointers (caller must free) or NULL
 *
 * @note Caller must free returned array with nimcp_free()
 * @note Episodes are internal pointers (read-only)
 */
recovery_episode_t** episodic_memory_get_all(
    episodic_memory_t* memory,
    uint32_t* count);

/**
 * @brief Get episodic memory statistics
 *
 * WHAT: Performance and usage metrics
 * WHY:  Monitor memory health and efficiency
 * HOW:  Copy statistics structure
 *
 * COMPLEXITY: O(1)
 *
 * @param memory Episodic memory instance (non-NULL)
 * @return Statistics structure
 */
episodic_memory_stats_t episodic_memory_get_stats(
    const episodic_memory_t* memory);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Compute error signature hash
 *
 * WHAT: Combine error type and code into signature hash
 * WHY:  Enable LSH-based similarity search
 * HOW:  Hash function combining type and code
 *
 * COMPLEXITY: O(1)
 *
 * @param error_type Error category
 * @param error_code Specific error code
 * @return 64-bit signature hash
 */
uint64_t compute_error_signature_hash(
    error_type_t error_type,
    uint32_t error_code);

/**
 * @brief Get current timestamp in milliseconds
 *
 * WHAT: System time for episode timestamping
 * WHY:  Track temporal patterns
 * HOW:  Uses system clock
 *
 * COMPLEXITY: O(1)
 *
 * @return Current time in milliseconds since epoch
 */
uint64_t get_current_timestamp_ms(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_RECOVERY_EPISODIC_MEMORY_H
