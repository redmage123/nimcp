/**
 * @file nimcp_swarm_memory.h
 * @brief Swarm Memory Consolidation System for NIMCP
 *
 * Biological Inspiration: Sleep-based memory consolidation in mammals
 *
 * This system implements distributed memory consolidation across the swarm,
 * inspired by how mammalian brains consolidate memories during sleep through
 * hippocampal replay and neocortical integration.
 *
 * Key Features:
 * - Multiple memory types (episodic, semantic, procedural, threat, spatial)
 * - Experience replay during low-activity periods
 * - Knowledge distillation and compression
 * - Time-based forgetting curves with importance weighting
 * - Periodic consolidation windows
 * - Distributed hippocampus architecture
 * - Semantic compression and pattern abstraction
 * - Full bio-async integration
 *
 * @version 1.0
 * @date 2025
 */

#ifndef NIMCP_SWARM_MEMORY_H
#define NIMCP_SWARM_MEMORY_H

#include "core/brain/nimcp_brain.h"
#include "async/nimcp_bio_messages.h"
#include "utils/time/nimcp_time.h"
#include "utils/containers/nimcp_hash_table.h"
#include "utils/containers/nimcp_min_heap.h"
#include <stdint.h>
#include <stdbool.h>

#include "async/nimcp_bio_router.h"
#include "utils/thread/nimcp_thread.h"
#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Memory Type Definitions
 * ============================================================================ */

/**
 * @brief Types of memories that can be consolidated
 */
typedef enum {
    NIMCP_MEMORY_EPISODIC,    /**< Specific events/experiences */
    NIMCP_MEMORY_SEMANTIC,    /**< General knowledge/patterns */
    NIMCP_MEMORY_PROCEDURAL,  /**< Learned behaviors */
    NIMCP_MEMORY_THREAT,      /**< Threat patterns */
    NIMCP_MEMORY_SPATIAL,     /**< Location/map data */
    NIMCP_MEMORY_TYPE_COUNT
} NimcpMemoryType;

/**
 * @brief Consolidation mode
 */
typedef enum {
    NIMCP_CONSOLIDATION_ACTIVE,   /**< Active consolidation (high priority) */
    NIMCP_CONSOLIDATION_PASSIVE,  /**< Passive consolidation (background) */
    NIMCP_CONSOLIDATION_SLEEP     /**< Sleep-like consolidation (offline) */
} NimcpConsolidationMode;

/**
 * @brief Memory importance level
 */
typedef enum {
    NIMCP_IMPORTANCE_LOW,
    NIMCP_IMPORTANCE_MEDIUM,
    NIMCP_IMPORTANCE_HIGH,
    NIMCP_IMPORTANCE_CRITICAL
} NimcpMemoryImportance;

/* ============================================================================
 * Memory Structure Definitions
 * ============================================================================ */

/**
 * @brief Individual memory entry
 */
typedef struct {
    char id[64];                      /**< Unique memory identifier */
    NimcpMemoryType type;             /**< Type of memory */
    NimcpMemoryImportance importance; /**< Importance level */

    void *data;                       /**< Memory data payload */
    size_t data_size;                 /**< Size of data in bytes */

    uint64_t created_at;              /**< Creation timestamp */
    uint64_t last_accessed;           /**< Last access timestamp */
    uint64_t last_rehearsed;          /**< Last rehearsal timestamp */

    uint32_t access_count;            /**< Number of accesses */
    uint32_t rehearsal_count;         /**< Number of rehearsals */

    float strength;                   /**< Memory strength (0.0-1.0) */
    float decay_rate;                 /**< Decay rate per time unit */
    float novelty_score;              /**< Novelty score (0.0-1.0) */

    bool is_compressed;               /**< Whether data is compressed */
    bool is_consolidated;             /**< Whether memory is consolidated */
    bool is_distributed;              /**< Whether memory is distributed to swarm */

    uint32_t replica_count;           /**< Number of replicas in swarm */
    char source_node[64];             /**< Node that created this memory */
} NimcpMemoryEntry;

/**
 * @brief Compressed memory for efficient transmission
 */
typedef struct {
    char id[64];                      /**< Memory identifier */
    NimcpMemoryType type;             /**< Memory type */
    NimcpMemoryImportance importance; /**< Importance level */

    void *compressed_data;            /**< Compressed data */
    size_t compressed_size;           /**< Compressed data size */
    size_t original_size;             /**< Original uncompressed size */

    float compression_ratio;          /**< Compression ratio achieved */
    uint32_t pattern_hash;            /**< Hash of extracted pattern */
} NimcpCompressedMemory;

/**
 * @brief Experience replay entry
 */
typedef struct {
    NimcpMemoryEntry *memory;         /**< Memory to replay */
    float replay_priority;            /**< Priority for replay */
    uint32_t replay_count;            /**< Number of times replayed */
    uint64_t next_replay_time;        /**< Next scheduled replay time */
} NimcpReplayEntry;

/**
 * @brief Forgetting curve parameters
 */
typedef struct {
    float initial_strength;           /**< Initial memory strength */
    float decay_rate;                 /**< Base decay rate */
    float importance_modifier;        /**< Importance-based modifier */
    float rehearsal_boost;            /**< Strength boost per rehearsal */
    uint64_t half_life_ms;            /**< Half-life in milliseconds */
} NimcpForgettingCurve;

/**
 * @brief Consolidation window configuration
 */
typedef struct {
    NimcpConsolidationMode mode;      /**< Consolidation mode */
    uint64_t window_start;            /**< Window start time */
    uint64_t window_duration_ms;      /**< Window duration */
    uint32_t max_memories_per_window; /**< Max memories to consolidate */
    float activity_threshold;         /**< Activity threshold for triggering */
    bool auto_schedule;               /**< Automatic scheduling enabled */
} NimcpConsolidationWindow;

/**
 * @brief Distributed hippocampus node
 */
typedef struct {
    char node_id[64];                 /**< Node identifier */
    bool is_active;                   /**< Whether node is active */
    uint32_t memory_count;            /**< Number of memories stored */
    uint64_t last_sync_time;          /**< Last synchronization time */
    float health_score;               /**< Node health (0.0-1.0) */
    uint32_t replica_capacity;        /**< Max replicas this node can hold */
} NimcpHippocampusNode;

/**
 * @brief Semantic compression context
 */
typedef struct {
    uint32_t pattern_count;           /**< Number of patterns extracted */
    void *pattern_tree;               /**< Hierarchical pattern tree */
    void *pattern_index;              /**< Pattern lookup index (stubbed) */
    float compression_target;         /**< Target compression ratio */
    uint32_t abstraction_level;       /**< Level of abstraction */
} NimcpSemanticCompression;

/**
 * @brief Memory statistics
 */
typedef struct {
    uint64_t total_memories;          /**< Total memories stored */
    uint64_t consolidated_memories;   /**< Consolidated memories */
    uint64_t distributed_memories;    /**< Distributed memories */
    uint64_t forgotten_memories;      /**< Forgotten memories */

    uint64_t total_replays;           /**< Total replay operations */
    uint64_t successful_replays;      /**< Successful replays */

    float avg_compression_ratio;      /**< Average compression ratio */
    float avg_memory_strength;        /**< Average memory strength */

    uint64_t bytes_transmitted;       /**< Bytes transmitted to swarm */
    uint64_t bytes_received;          /**< Bytes received from swarm */

    uint32_t active_nodes;            /**< Active hippocampus nodes */
    uint32_t total_replicas;          /**< Total memory replicas */
} NimcpMemoryStatistics;

/**
 * @brief Pattern statistics (forward declaration)
 */
typedef struct {
    uint32_t total_patterns;          /**< Total patterns stored */
    uint32_t active_patterns;         /**< Active patterns (confidence > threshold) */
    float avg_pattern_confidence;     /**< Average pattern confidence */
    uint32_t total_associations;      /**< Total pattern-outcome associations */
    uint64_t patterns_learned;        /**< Cumulative patterns learned */
    uint64_t patterns_forgotten;      /**< Cumulative patterns forgotten */
} swarm_pattern_stats_t;

/**
 * @brief Main swarm memory consolidation system
 */
typedef struct {
    /* Memory storage - using real container types */
    hash_table_t *memories;                   /**< All memories indexed by ID */
    hash_table_t *memories_by_type[NIMCP_MEMORY_TYPE_COUNT]; /**< By type */
    nimcp_min_heap_t *replay_queue;           /**< Priority queue for replay */

    /* Forgetting curves */
    NimcpForgettingCurve curves[NIMCP_MEMORY_TYPE_COUNT]; /**< Per-type curves */

    /* Consolidation */
    NimcpConsolidationWindow window;  /**< Current consolidation window */
    uint64_t last_consolidation;      /**< Last consolidation time */
    uint32_t consolidation_count;     /**< Total consolidations performed */

    /* Distributed hippocampus */
    hash_table_t *hippocampus_nodes;          /**< Active hippocampus nodes */
    uint32_t replication_factor;      /**< Target replication factor */
    float consensus_threshold;        /**< Consensus threshold (0.0-1.0) */

    /* Semantic compression */
    NimcpSemanticCompression compression; /**< Compression context */

    /* Pattern learning (NEW) */
    hash_table_t *patterns;           /**< Pattern database indexed by ID */
    hash_table_t *pattern_associations; /**< Pattern-outcome associations */
    hash_table_t *sequence_transitions; /**< Pattern sequence transitions */
    swarm_pattern_stats_t pattern_stats; /**< Pattern learning statistics */
    uint32_t next_pattern_id;         /**< Next pattern ID to assign */
    float pattern_similarity_threshold; /**< Similarity threshold for matching */
    uint32_t max_patterns;            /**< Maximum patterns to store */

    /* Bio-async integration (stubbed) */
    void *bio_ctx;                    /**< Bio-async context */
    bool bio_async_enabled;           /**< Bio-async enabled flag */

    /* Statistics */
    NimcpMemoryStatistics stats;      /**< System statistics */

    /* Configuration */
    float novelty_threshold;          /**< Threshold for novelty detection */
    float replay_probability;         /**< Probability of replay per cycle */
    uint32_t max_memory_capacity;     /**< Max memories to store */
    bool auto_compression;            /**< Automatic compression enabled */
    bool auto_distribution;           /**< Automatic distribution enabled */

    /* Synchronization */
    nimcp_platform_mutex_t* mutex;       /**< Thread-safety mutex */
    bool is_initialized;              /**< Initialization flag */
} NimcpSwarmMemory;

/* ============================================================================
 * Core API Functions
 * ============================================================================ */

/**
 * @brief Create a new swarm memory system
 *
 * @param max_capacity Maximum memory capacity
 * @param replication_factor Target replication factor
 * @return Pointer to created system, or NULL on failure
 */
NimcpSwarmMemory *nimcp_swarm_memory_create(
    uint32_t max_capacity,
    uint32_t replication_factor
);

/**
 * @brief Destroy swarm memory system and free resources
 *
 * @param memory Swarm memory system to destroy
 */
void nimcp_swarm_memory_destroy(NimcpSwarmMemory *memory);

/**
 * @brief Initialize swarm memory system
 *
 * @param memory Swarm memory system
 * @param bio_ctx Bio-async context (optional, can be NULL)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_memory_init(
    NimcpSwarmMemory *memory,
    void *bio_ctx
);

/* ============================================================================
 * Memory Management API
 * ============================================================================ */

/**
 * @brief Store a new memory
 *
 * @param memory Swarm memory system
 * @param type Memory type
 * @param importance Memory importance
 * @param data Memory data
 * @param data_size Size of data
 * @param memory_id Output: assigned memory ID
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_memory_store(
    NimcpSwarmMemory *memory,
    NimcpMemoryType type,
    NimcpMemoryImportance importance,
    const void *data,
    size_t data_size,
    char *memory_id
);

/**
 * @brief Retrieve a memory by ID
 *
 * @param memory Swarm memory system
 * @param memory_id Memory identifier
 * @param out_data Output buffer for data
 * @param data_size Size of output buffer
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_memory_retrieve(
    NimcpSwarmMemory *memory,
    const char *memory_id,
    void *out_data,
    size_t data_size
);

/**
 * @brief Update memory access tracking
 *
 * @param memory Swarm memory system
 * @param memory_id Memory identifier
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_memory_access(
    NimcpSwarmMemory *memory,
    const char *memory_id
);

/**
 * @brief Rehearse a memory to strengthen it
 *
 * @param memory Swarm memory system
 * @param memory_id Memory identifier
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_memory_rehearse(
    NimcpSwarmMemory *memory,
    const char *memory_id
);

/**
 * @brief Delete a memory
 *
 * @param memory Swarm memory system
 * @param memory_id Memory identifier
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_memory_delete(
    NimcpSwarmMemory *memory,
    const char *memory_id
);

/* ============================================================================
 * Experience Replay API
 * ============================================================================ */

/**
 * @brief Schedule a memory for replay
 *
 * @param memory Swarm memory system
 * @param memory_id Memory identifier
 * @param priority Replay priority
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_memory_schedule_replay(
    NimcpSwarmMemory *memory,
    const char *memory_id,
    float priority
);

/**
 * @brief Execute replay cycle
 *
 * Replays high-priority memories to strengthen them and share with swarm
 *
 * @param memory Swarm memory system
 * @param max_replays Maximum number of replays to perform
 * @param replays_performed Output: number of replays performed
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_memory_replay_cycle(
    NimcpSwarmMemory *memory,
    uint32_t max_replays,
    uint32_t *replays_performed
);

/**
 * @brief Calculate replay priority for a memory
 *
 * Based on novelty, importance, and recency
 *
 * @param memory Swarm memory system
 * @param memory_entry Memory entry
 * @return Replay priority (0.0-1.0)
 */
float nimcp_swarm_memory_calculate_replay_priority(
    NimcpSwarmMemory *memory,
    const NimcpMemoryEntry *memory_entry
);

/* ============================================================================
 * Knowledge Distillation API
 * ============================================================================ */

/**
 * @brief Compress a memory for efficient transmission
 *
 * @param memory Swarm memory system
 * @param memory_id Memory identifier
 * @param compressed Output: compressed memory
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_memory_compress(
    NimcpSwarmMemory *memory,
    const char *memory_id,
    NimcpCompressedMemory *compressed
);

/**
 * @brief Decompress a received memory
 *
 * @param memory Swarm memory system
 * @param compressed Compressed memory
 * @param decompressed Output buffer
 * @param buffer_size Size of output buffer
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_memory_decompress(
    NimcpSwarmMemory *memory,
    const NimcpCompressedMemory *compressed,
    void *decompressed,
    size_t buffer_size
);

/**
 * @brief Extract essential patterns from a memory
 *
 * @param memory Swarm memory system
 * @param memory_id Memory identifier
 * @param pattern_hash Output: hash of extracted pattern
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_memory_extract_pattern(
    NimcpSwarmMemory *memory,
    const char *memory_id,
    uint32_t *pattern_hash
);

/* ============================================================================
 * Forgetting Curve API
 * ============================================================================ */

/**
 * @brief Update forgetting curve parameters for a memory type
 *
 * @param memory Swarm memory system
 * @param type Memory type
 * @param curve Forgetting curve parameters
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_memory_set_forgetting_curve(
    NimcpSwarmMemory *memory,
    NimcpMemoryType type,
    const NimcpForgettingCurve *curve
);

/**
 * @brief Calculate current memory strength based on forgetting curve
 *
 * @param memory Swarm memory system
 * @param memory_entry Memory entry
 * @param current_time Current timestamp
 * @return Current memory strength (0.0-1.0)
 */
float nimcp_swarm_memory_calculate_strength(
    NimcpSwarmMemory *memory,
    const NimcpMemoryEntry *memory_entry,
    uint64_t current_time
);

/**
 * @brief Apply forgetting to all memories
 *
 * Updates memory strengths and removes forgotten memories
 *
 * @param memory Swarm memory system
 * @param forgotten_count Output: number of memories forgotten
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_memory_apply_forgetting(
    NimcpSwarmMemory *memory,
    uint32_t *forgotten_count
);

/* ============================================================================
 * Consolidation Window API
 * ============================================================================ */

/**
 * @brief Configure consolidation window
 *
 * @param memory Swarm memory system
 * @param window Consolidation window configuration
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_memory_configure_consolidation(
    NimcpSwarmMemory *memory,
    const NimcpConsolidationWindow *window
);

/**
 * @brief Start a consolidation window
 *
 * @param memory Swarm memory system
 * @param mode Consolidation mode
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_memory_start_consolidation(
    NimcpSwarmMemory *memory,
    NimcpConsolidationMode mode
);

/**
 * @brief Execute consolidation process
 *
 * Consolidates memories, applies forgetting, and distributes to swarm
 *
 * @param memory Swarm memory system
 * @param memories_consolidated Output: number of memories consolidated
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_memory_consolidate(
    NimcpSwarmMemory *memory,
    uint32_t *memories_consolidated
);

/**
 * @brief Check if consolidation window is active
 *
 * @param memory Swarm memory system
 * @return true if window is active, false otherwise
 */
bool nimcp_swarm_memory_is_consolidating(const NimcpSwarmMemory *memory);

/* ============================================================================
 * Distributed Hippocampus API
 * ============================================================================ */

/**
 * @brief Register a hippocampus node
 *
 * @param memory Swarm memory system
 * @param node_id Node identifier
 * @param capacity Node capacity
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_memory_register_node(
    NimcpSwarmMemory *memory,
    const char *node_id,
    uint32_t capacity
);

/**
 * @brief Unregister a hippocampus node
 *
 * @param memory Swarm memory system
 * @param node_id Node identifier
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_memory_unregister_node(
    NimcpSwarmMemory *memory,
    const char *node_id
);

/**
 * @brief Distribute a memory to hippocampus nodes
 *
 * @param memory Swarm memory system
 * @param memory_id Memory identifier
 * @param replicas_created Output: number of replicas created
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_memory_distribute(
    NimcpSwarmMemory *memory,
    const char *memory_id,
    uint32_t *replicas_created
);

/**
 * @brief Verify memory consensus across nodes
 *
 * @param memory Swarm memory system
 * @param memory_id Memory identifier
 * @param has_consensus Output: whether consensus was reached
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_memory_verify_consensus(
    NimcpSwarmMemory *memory,
    const char *memory_id,
    bool *has_consensus
);

/**
 * @brief Synchronize with a specific node
 *
 * @param memory Swarm memory system
 * @param node_id Node identifier
 * @param memories_synced Output: number of memories synchronized
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_memory_sync_with_node(
    NimcpSwarmMemory *memory,
    const char *node_id,
    uint32_t *memories_synced
);

/* ============================================================================
 * Semantic Compression API
 * ============================================================================ */

/**
 * @brief Abstract patterns from raw experiences
 *
 * @param memory Swarm memory system
 * @param memory_ids Array of memory IDs to abstract from
 * @param count Number of memory IDs
 * @param pattern_hash Output: hash of abstracted pattern
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_memory_abstract_pattern(
    NimcpSwarmMemory *memory,
    const char **memory_ids,
    uint32_t count,
    uint32_t *pattern_hash
);

/**
 * @brief Generalize specific memories into broader knowledge
 *
 * @param memory Swarm memory system
 * @param specific_ids Array of specific memory IDs
 * @param count Number of specific memories
 * @param generalized_id Output: ID of generalized memory
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_memory_generalize(
    NimcpSwarmMemory *memory,
    const char **specific_ids,
    uint32_t count,
    char *generalized_id
);

/**
 * @brief Build hierarchical knowledge structure
 *
 * @param memory Swarm memory system
 * @param levels Output: number of hierarchy levels created
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_memory_build_hierarchy(
    NimcpSwarmMemory *memory,
    uint32_t *levels
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Process incoming bio-async message
 *
 * @param memory Swarm memory system
 * @param msg Incoming message
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_memory_process_message(
    NimcpSwarmMemory *memory,
    const void *msg
);

/**
 * @brief Send memory to another node via bio-async
 *
 * @param memory Swarm memory system
 * @param memory_id Memory identifier
 * @param target_node Target node identifier
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_memory_send_memory(
    NimcpSwarmMemory *memory,
    const char *memory_id,
    const char *target_node
);

/**
 * @brief Request memory from another node
 *
 * @param memory Swarm memory system
 * @param memory_id Memory identifier
 * @param source_node Source node identifier
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_memory_request_memory(
    NimcpSwarmMemory *memory,
    const char *memory_id,
    const char *source_node
);

/**
 * @brief Broadcast consolidation signal to swarm
 *
 * @param memory Swarm memory system
 * @param mode Consolidation mode
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_memory_broadcast_consolidation(
    NimcpSwarmMemory *memory,
    NimcpConsolidationMode mode
);

/* ============================================================================
 * Statistics and Monitoring API
 * ============================================================================ */

/**
 * @brief Get system statistics
 *
 * @param memory Swarm memory system
 * @param stats Output: statistics structure
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_memory_get_statistics(
    const NimcpSwarmMemory *memory,
    NimcpMemoryStatistics *stats
);

/**
 * @brief Get memory count by type
 *
 * @param memory Swarm memory system
 * @param type Memory type
 * @return Number of memories of specified type
 */
uint32_t nimcp_swarm_memory_get_count_by_type(
    const NimcpSwarmMemory *memory,
    NimcpMemoryType type
);

/**
 * @brief Get system health score
 *
 * Based on node availability, replication factor, and consolidation status
 *
 * @param memory Swarm memory system
 * @return Health score (0.0-1.0)
 */
float nimcp_swarm_memory_get_health_score(const NimcpSwarmMemory *memory);

/**
 * @brief Print system status to log
 *
 * @param memory Swarm memory system
 * @param verbose Whether to print verbose information
 */
void nimcp_swarm_memory_print_status(
    const NimcpSwarmMemory *memory,
    bool verbose
);

/* ============================================================================
 * Pattern Learning API (NEW)
 * ============================================================================ */

/**
 * @brief Swarm pattern structure
 *
 * WHAT: Represents a learned behavioral pattern in the swarm
 * WHY: Enables pattern recognition and prediction for swarm intelligence
 * HOW: Stores pattern signature, occurrence stats, and confidence metrics
 */
typedef struct {
    uint32_t pattern_id;          /**< Unique pattern identifier */
    char label[64];               /**< Pattern label for identification */
    float *data;                  /**< Pattern data vector */
    size_t data_len;              /**< Length of data vector */
    float *signature;             /**< Pattern signature vector (for backward compat) */
    uint32_t signature_size;      /**< Size of signature vector */
    float strength;               /**< Pattern strength (0.0-1.0) */
    uint32_t occurrence_count;    /**< Number of times pattern observed */
    uint32_t access_count;        /**< Number of accesses */
    float confidence;             /**< Pattern confidence (0.0-1.0) */
    uint64_t first_seen_ms;       /**< First observation timestamp */
    uint64_t last_seen_ms;        /**< Last observation timestamp */
    uint64_t last_access_ms;      /**< Last access timestamp */
} swarm_pattern_t;

/**
 * @brief Temporal pattern structure
 *
 * WHAT: Represents a temporal sequence of patterns
 * WHY: Captures time-dependent behavior patterns
 * HOW: Stores pattern sequence with timing information
 */
typedef struct {
    uint32_t *sequence;           /**< Array of pattern IDs in sequence */
    size_t sequence_len;          /**< Length of sequence */
    uint64_t period_ms;           /**< Period between patterns in ms */
    float confidence;             /**< Sequence confidence (0.0-1.0) */
} temporal_pattern_t;

/**
 * @brief Pattern statistics result structure
 *
 * WHAT: Detailed statistics for a specific pattern
 * WHY: Provides insight into individual pattern usage
 * HOW: Aggregates access, strength, and association metrics
 */
typedef struct {
    uint32_t access_count;        /**< Number of accesses */
    uint64_t last_access_ms;      /**< Last access timestamp */
    float strength;               /**< Pattern strength */
    uint32_t associations_count;  /**< Number of associations */
} swarm_pattern_result_t;

/**
 * @brief Pattern association structure
 *
 * WHAT: Links patterns to outcomes for predictive learning
 * WHY: Enables outcome prediction based on recognized patterns
 * HOW: Tracks association strength and reinforcement count
 */
typedef struct {
    uint32_t pattern_id;              /**< Pattern identifier */
    uint32_t outcome_id;              /**< Outcome identifier */
    float association_strength;       /**< Association strength (0.0-1.0) */
    uint32_t reinforcement_count;     /**< Number of reinforcements */
} pattern_association_t;

/* swarm_pattern_stats_t already defined above before NimcpSwarmMemory struct */

/**
 * @brief Detect a pattern in observation data
 *
 * WHAT: Identifies if observation matches any known patterns
 * WHY: Core pattern recognition for swarm intelligence
 * HOW: Compares observation signature against stored patterns
 *
 * @param mem Swarm memory system
 * @param observation Observation data vector
 * @param obs_size Size of observation vector
 * @param matched Output: matched pattern (if found)
 * @return NIMCP_SUCCESS on match, NIMCP_ERROR_NOT_FOUND if no match
 */
nimcp_result_t swarm_memory_detect_pattern(
    NimcpSwarmMemory *mem,
    const float *observation,
    uint32_t obs_size,
    swarm_pattern_t *matched
);

/**
 * @brief Store a new pattern
 *
 * WHAT: Adds a new pattern to the pattern database
 * WHY: Expands pattern knowledge base for future recognition
 * HOW: Validates and stores pattern with metadata
 *
 * @param mem Swarm memory system
 * @param pattern Pattern to store
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
nimcp_result_t swarm_memory_store_pattern(
    NimcpSwarmMemory *mem,
    const swarm_pattern_t *pattern
);

/**
 * @brief Retrieve a pattern by ID
 *
 * WHAT: Fetches pattern details from pattern database
 * WHY: Access stored patterns for analysis or prediction
 * HOW: Looks up pattern by ID and copies to output
 *
 * @param mem Swarm memory system
 * @param pattern_id Pattern identifier
 * @param out Output buffer for pattern
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
nimcp_result_t swarm_memory_retrieve_pattern(
    NimcpSwarmMemory *mem,
    uint32_t pattern_id,
    swarm_pattern_t *out
);

/**
 * @brief Find similar patterns
 *
 * WHAT: Searches for patterns similar to query signature
 * WHY: Enables fuzzy pattern matching and generalization
 * HOW: Computes similarity scores and returns top matches
 *
 * @param mem Swarm memory system
 * @param query Query signature vector
 * @param query_size Size of query vector
 * @param results Output: array of similar patterns
 * @param count Output: number of patterns returned
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
nimcp_result_t swarm_memory_get_similar_patterns(
    NimcpSwarmMemory *mem,
    const float *query,
    uint32_t query_size,
    swarm_pattern_t **results,
    uint32_t *count
);

/**
 * @brief Associate pattern with outcome
 *
 * WHAT: Creates or strengthens pattern-outcome association
 * WHY: Enables learning from experience and prediction
 * HOW: Updates association strength based on reward signal
 *
 * @param mem Swarm memory system
 * @param pattern_id Pattern identifier
 * @param outcome_id Outcome identifier
 * @param reward Reward signal (-1.0 to 1.0)
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
nimcp_result_t swarm_memory_associate_pattern(
    NimcpSwarmMemory *mem,
    uint32_t pattern_id,
    uint32_t outcome_id,
    float reward
);

/**
 * @brief Predict outcome from pattern
 *
 * WHAT: Predicts likely outcome given a recognized pattern
 * WHY: Enables proactive swarm behavior based on past experience
 * HOW: Finds strongest pattern-outcome association
 *
 * @param mem Swarm memory system
 * @param pattern_id Pattern identifier
 * @param predicted_outcome Output: predicted outcome ID
 * @param confidence Output: prediction confidence
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
nimcp_result_t swarm_memory_predict_outcome(
    NimcpSwarmMemory *mem,
    uint32_t pattern_id,
    uint32_t *predicted_outcome,
    float *confidence
);

/**
 * @brief Learn a temporal sequence
 *
 * WHAT: Stores a sequence of patterns for temporal learning
 * WHY: Captures time-dependent swarm behaviors
 * HOW: Creates sequence associations between consecutive patterns
 *
 * @param mem Swarm memory system
 * @param pattern_sequence Array of pattern IDs in sequence
 * @param seq_length Length of sequence
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
nimcp_result_t swarm_memory_learn_sequence(
    NimcpSwarmMemory *mem,
    const uint32_t *pattern_sequence,
    uint32_t seq_length
);

/**
 * @brief Predict next pattern in sequence
 *
 * WHAT: Predicts next pattern given historical sequence
 * WHY: Enables anticipatory swarm coordination
 * HOW: Analyzes sequence history and finds most likely next pattern
 *
 * @param mem Swarm memory system
 * @param history Array of recent pattern IDs
 * @param history_len Length of history
 * @param predicted Output: predicted next pattern ID
 * @param confidence Output: prediction confidence
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
nimcp_result_t swarm_memory_predict_next(
    NimcpSwarmMemory *mem,
    const uint32_t *history,
    uint32_t history_len,
    uint32_t *predicted,
    float *confidence
);

/**
 * @brief Consolidate patterns
 *
 * WHAT: Strengthens important patterns, merges similar ones
 * WHY: Optimizes pattern database for efficiency and accuracy
 * HOW: Applies consolidation rules based on confidence and usage
 *
 * @param mem Swarm memory system
 * @param current_time_ms Current time in milliseconds
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
nimcp_result_t swarm_memory_consolidate_patterns(
    NimcpSwarmMemory *mem,
    uint64_t current_time_ms
);

/**
 * @brief Forget weak patterns
 *
 * WHAT: Removes patterns below confidence threshold
 * WHY: Prevents pattern database bloat and improves performance
 * HOW: Deletes patterns with confidence below threshold
 *
 * @param mem Swarm memory system
 * @param threshold Minimum confidence to retain (0.0-1.0)
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
nimcp_result_t swarm_memory_forget_weak_patterns(
    NimcpSwarmMemory *mem,
    float threshold
);

/**
 * @brief Get pattern learning statistics
 *
 * WHAT: Retrieves aggregate pattern learning metrics
 * WHY: Monitoring and debugging pattern learning system
 * HOW: Returns copy of current statistics
 *
 * @param mem Swarm memory system
 * @return Pattern statistics structure
 */
swarm_pattern_stats_t swarm_memory_get_pattern_stats(
    const NimcpSwarmMemory *mem
);

/* ============================================================================
 * New Pattern Learning Features (8 Features)
 * ============================================================================ */

/**
 * @brief Recognize a pattern in signal data
 *
 * WHAT: Matches signal against stored patterns using cosine similarity
 * WHY: Enables template-based pattern recognition
 * HOW: Computes similarity scores and returns best match
 *
 * @param memory Swarm memory system
 * @param signal Signal data to match
 * @param len Length of signal
 * @return Pattern ID of best match, or -1 if no match found
 */
int32_t swarm_memory_recognize_pattern(
    NimcpSwarmMemory *memory,
    const float *signal,
    size_t len
);

/**
 * @brief Store a new pattern with label
 *
 * WHAT: Stores pattern with label in hash table
 * WHY: Builds pattern library for recognition
 * HOW: Validates capacity and inserts into pattern database
 *
 * @param memory Swarm memory system
 * @param signal Signal data to store
 * @param len Length of signal
 * @param label Pattern label
 * @return Pattern ID assigned, or -1 on error
 */
int32_t swarm_memory_store_pattern_labeled(
    NimcpSwarmMemory *memory,
    const float *signal,
    size_t len,
    const char *label
);

/**
 * @brief Create bidirectional pattern association
 *
 * WHAT: Links two patterns with association strength
 * WHY: Enables pattern relationship learning
 * HOW: Stores association in both directions
 *
 * @param memory Swarm memory system
 * @param pattern_a First pattern ID
 * @param pattern_b Second pattern ID
 * @param strength Association strength (0.0-1.0)
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
nimcp_result_t swarm_memory_associate_patterns(
    NimcpSwarmMemory *memory,
    uint32_t pattern_a,
    uint32_t pattern_b,
    float strength
);

/**
 * @brief Detect temporal pattern in time-series data
 *
 * WHAT: Identifies recurring sequences in signal/timestamp pairs
 * WHY: Captures temporal behaviors and cycles
 * HOW: Analyzes signal sequences for repeated patterns
 *
 * @param memory Swarm memory system
 * @param signals Array of signal vectors
 * @param timestamps Array of timestamps
 * @param count Number of signals
 * @param result Output: detected temporal pattern
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
nimcp_result_t swarm_memory_detect_temporal_pattern(
    NimcpSwarmMemory *memory,
    const float **signals,
    const uint64_t *timestamps,
    size_t count,
    temporal_pattern_t *result
);

/**
 * @brief Consolidate similar patterns
 *
 * WHAT: Merges similar patterns and strengthens frequently accessed ones
 * WHY: Optimizes pattern database quality and efficiency
 * HOW: Finds similar patterns (>0.9 similarity) and consolidates
 *
 * @param memory Swarm memory system
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
nimcp_result_t swarm_memory_consolidate_patterns_full(
    NimcpSwarmMemory *memory
);

/**
 * @brief Get statistics for specific pattern
 *
 * WHAT: Retrieves detailed metrics for one pattern
 * WHY: Enables pattern-level monitoring and debugging
 * HOW: Looks up pattern and aggregates statistics
 *
 * @param memory Swarm memory system
 * @param pattern_id Pattern identifier
 * @param stats Output: pattern statistics
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
nimcp_result_t swarm_memory_get_pattern_stats_by_id(
    NimcpSwarmMemory *memory,
    uint32_t pattern_id,
    swarm_pattern_result_t *stats
);

/**
 * @brief Export patterns to file
 *
 * WHAT: Serializes all patterns to disk file
 * WHY: Enables pattern persistence and sharing
 * HOW: Writes pattern data in binary format
 *
 * @param memory Swarm memory system
 * @param filepath Output file path
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
nimcp_result_t swarm_memory_export_patterns(
    NimcpSwarmMemory *memory,
    const char *filepath
);

/**
 * @brief Import patterns from file
 *
 * WHAT: Loads patterns from disk file
 * WHY: Restores previously saved patterns
 * HOW: Reads binary format and reconstructs patterns
 *
 * @param memory Swarm memory system
 * @param filepath Input file path
 * @return Number of patterns imported, or -1 on error
 */
int32_t swarm_memory_import_patterns(
    NimcpSwarmMemory *memory,
    const char *filepath
);

/**
 * @brief Find similar patterns above threshold
 *
 * WHAT: Searches for all patterns similar to query signal
 * WHY: Enables fuzzy matching and pattern clustering
 * HOW: Computes similarity for all patterns, returns matches
 *
 * @param memory Swarm memory system
 * @param signal Query signal
 * @param len Signal length
 * @param threshold Minimum similarity threshold (0.0-1.0)
 * @param results Output: array of pattern IDs
 * @param max_results Maximum results to return
 * @return Number of patterns found
 */
int32_t swarm_memory_find_similar_patterns(
    NimcpSwarmMemory *memory,
    const float *signal,
    size_t len,
    float threshold,
    uint32_t *results,
    size_t max_results
);

/* ============================================================================
 * Ternary Confidence API
 * ============================================================================ */

/**
 * @brief Ternary confidence level for memory traces
 *
 * WHAT: Discrete confidence states for memory and pattern reliability
 * WHY:  Simplifies continuous confidence to actionable decision states
 * HOW:  Maps to memory retrieval behavior (certain, uncertain, unreliable)
 *
 * BIOLOGICAL BASIS:
 * - Hippocampal pattern completion strength varies discretely
 * - Memory familiarity vs recollection distinction
 * - Confidence-based memory selection in prefrontal cortex
 */
typedef enum {
    SWARM_CONFIDENCE_UNRELIABLE = -1,   /**< Low confidence: pattern/memory unreliable */
    SWARM_CONFIDENCE_UNCERTAIN = 0,     /**< Medium confidence: needs verification */
    SWARM_CONFIDENCE_CERTAIN = 1        /**< High confidence: reliable for decisions */
} ternary_swarm_confidence_t;

/**
 * @brief Ternary confidence configuration
 *
 * WHAT: Configuration for ternary confidence discretization
 * WHY:  Customize confidence thresholds for different use cases
 */
typedef struct {
    float certain_threshold;            /**< Confidence >= this is CERTAIN (default: 0.8) */
    float uncertain_threshold;          /**< Confidence >= this is UNCERTAIN (default: 0.4) */
    bool enable_for_patterns;           /**< Apply to pattern confidence (default: true) */
    bool enable_for_memories;           /**< Apply to memory strength (default: true) */
    bool enable_for_predictions;        /**< Apply to prediction confidence (default: true) */
    bool decay_aware;                   /**< Consider decay when computing confidence */
    float decay_penalty;                /**< Penalty for old memories (default: 0.1) */
} ternary_confidence_config_t;

/**
 * @brief Ternary confidence statistics
 *
 * WHAT: Statistics for ternary confidence distribution
 * WHY:  Monitor confidence distribution across memory system
 */
typedef struct {
    uint32_t certain_count;             /**< Memories/patterns in CERTAIN state */
    uint32_t uncertain_count;           /**< Memories/patterns in UNCERTAIN state */
    uint32_t unreliable_count;          /**< Memories/patterns in UNRELIABLE state */
    float avg_raw_confidence;           /**< Average raw confidence value */
    uint32_t transitions_up;            /**< Transitions from lower to higher confidence */
    uint32_t transitions_down;          /**< Transitions from higher to lower confidence */
} ternary_confidence_stats_t;

/**
 * @brief Get default ternary confidence configuration
 *
 * WHAT: Provide sensible defaults for ternary confidence
 * WHY:  Easy initialization with biologically-plausible values
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
static inline int ternary_confidence_default_config(ternary_confidence_config_t* config) {
    if (!config) return -1;

    config->certain_threshold = 0.8f;
    config->uncertain_threshold = 0.4f;
    config->enable_for_patterns = true;
    config->enable_for_memories = true;
    config->enable_for_predictions = true;
    config->decay_aware = true;
    config->decay_penalty = 0.1f;

    return 0;
}

/**
 * @brief Convert continuous confidence to ternary state
 *
 * WHAT: Discretize confidence value to ternary level
 * WHY:  Simplify confidence for decision-making
 *
 * @param config Ternary configuration (NULL for defaults)
 * @param confidence Continuous confidence value [0,1]
 * @return Ternary confidence level
 */
static inline ternary_swarm_confidence_t ternary_confidence_from_value(
    const ternary_confidence_config_t* config,
    float confidence
) {
    float certain_thresh = config ? config->certain_threshold : 0.8f;
    float uncertain_thresh = config ? config->uncertain_threshold : 0.4f;

    if (confidence >= certain_thresh) {
        return SWARM_CONFIDENCE_CERTAIN;
    }
    if (confidence >= uncertain_thresh) {
        return SWARM_CONFIDENCE_UNCERTAIN;
    }
    return SWARM_CONFIDENCE_UNRELIABLE;
}

/**
 * @brief Convert ternary state to representative confidence
 *
 * WHAT: Get typical confidence value for ternary state
 * WHY:  Interface with systems expecting continuous confidence
 *
 * @param state Ternary confidence level
 * @return Representative confidence value
 */
static inline float ternary_confidence_to_value(ternary_swarm_confidence_t state) {
    switch (state) {
        case SWARM_CONFIDENCE_CERTAIN:    return 0.9f;
        case SWARM_CONFIDENCE_UNCERTAIN:  return 0.6f;
        default:                          return 0.2f;
    }
}

/**
 * @brief Enable ternary confidence mode
 *
 * WHAT: Switch swarm memory to use ternary confidence
 * WHY:  Simplify confidence-based decisions
 *
 * @param memory Swarm memory system
 * @param config Ternary configuration (NULL for defaults)
 * @return NIMCP_OK on success
 */
nimcp_result_t swarm_memory_enable_ternary_confidence(
    NimcpSwarmMemory* memory,
    const ternary_confidence_config_t* config
);

/**
 * @brief Disable ternary confidence mode
 *
 * WHAT: Return to continuous confidence mode
 * WHY:  Allow switching between discrete and continuous
 *
 * @param memory Swarm memory system
 * @return NIMCP_OK on success
 */
nimcp_result_t swarm_memory_disable_ternary_confidence(NimcpSwarmMemory* memory);

/**
 * @brief Check if ternary confidence is enabled
 *
 * @param memory Swarm memory system
 * @return true if ternary confidence active
 */
bool swarm_memory_is_ternary_confidence(const NimcpSwarmMemory* memory);

/**
 * @brief Get ternary confidence for memory entry
 *
 * WHAT: Query discretized confidence for a memory
 * WHY:  Access ternary state for decision-making
 *
 * @param memory Swarm memory system
 * @param memory_id Memory identifier
 * @return Ternary confidence level
 */
ternary_swarm_confidence_t swarm_memory_get_ternary_confidence(
    NimcpSwarmMemory* memory,
    const char* memory_id
);

/**
 * @brief Get ternary confidence for pattern
 *
 * WHAT: Query discretized confidence for a pattern
 * WHY:  Access ternary state for pattern-based decisions
 *
 * @param memory Swarm memory system
 * @param pattern_id Pattern identifier
 * @return Ternary confidence level
 */
ternary_swarm_confidence_t swarm_memory_get_pattern_ternary_confidence(
    NimcpSwarmMemory* memory,
    uint32_t pattern_id
);

/**
 * @brief Filter memories by ternary confidence
 *
 * WHAT: Get all memories at specific confidence level
 * WHY:  Batch retrieval based on confidence state
 *
 * @param memory Swarm memory system
 * @param level Target confidence level
 * @param memory_ids Output: array of memory IDs (caller allocates)
 * @param max_count Maximum IDs to return
 * @return Number of memories found
 */
uint32_t swarm_memory_filter_by_ternary_confidence(
    NimcpSwarmMemory* memory,
    ternary_swarm_confidence_t level,
    char** memory_ids,
    uint32_t max_count
);

/**
 * @brief Filter patterns by ternary confidence
 *
 * WHAT: Get all patterns at specific confidence level
 * WHY:  Batch retrieval based on pattern reliability
 *
 * @param memory Swarm memory system
 * @param level Target confidence level
 * @param pattern_ids Output: array of pattern IDs (caller allocates)
 * @param max_count Maximum IDs to return
 * @return Number of patterns found
 */
uint32_t swarm_memory_filter_patterns_by_ternary_confidence(
    NimcpSwarmMemory* memory,
    ternary_swarm_confidence_t level,
    uint32_t* pattern_ids,
    uint32_t max_count
);

/**
 * @brief Get ternary confidence statistics
 *
 * WHAT: Retrieve distribution of confidence levels
 * WHY:  Monitor memory system health and confidence distribution
 *
 * @param memory Swarm memory system
 * @param stats Output statistics
 * @return NIMCP_OK on success
 */
nimcp_result_t swarm_memory_get_ternary_confidence_stats(
    const NimcpSwarmMemory* memory,
    ternary_confidence_stats_t* stats
);

/**
 * @brief Apply ternary-based forgetting
 *
 * WHAT: Remove memories based on ternary confidence threshold
 * WHY:  Efficient cleanup of unreliable memories
 *
 * @param memory Swarm memory system
 * @param min_level Minimum confidence level to retain
 * @return Number of memories removed
 */
uint32_t swarm_memory_ternary_forget(
    NimcpSwarmMemory* memory,
    ternary_swarm_confidence_t min_level
);

/**
 * @brief Consolidate using ternary confidence
 *
 * WHAT: Prioritize consolidation based on ternary confidence
 * WHY:  Focus resources on uncertain memories needing strengthening
 *
 * @param memory Swarm memory system
 * @param prioritize_uncertain If true, consolidate UNCERTAIN first
 * @return Number of memories consolidated
 */
uint32_t swarm_memory_ternary_consolidate(
    NimcpSwarmMemory* memory,
    bool prioritize_uncertain
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get string name for memory type
 *
 * @param type Memory type
 * @return String name
 */
const char *nimcp_memory_type_to_string(NimcpMemoryType type);

/**
 * @brief Get string name for consolidation mode
 *
 * @param mode Consolidation mode
 * @return String name
 */
const char *nimcp_consolidation_mode_to_string(NimcpConsolidationMode mode);

/**
 * @brief Get string name for memory importance
 *
 * @param importance Memory importance
 * @return String name
 */
const char *nimcp_memory_importance_to_string(NimcpMemoryImportance importance);

#include "async/nimcp_bio_router.h"
#include "utils/thread/nimcp_thread.h"
#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_MEMORY_H */
