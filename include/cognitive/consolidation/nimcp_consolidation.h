/**
 * @file nimcp_consolidation.h
 * @brief Memory consolidation and sleep-like learning processes API
 *
 * WHAT: This module provides APIs for memory consolidation - the process
 * of strengthening important patterns, pruning weak connections, and
 * integrating new learning with existing knowledge. Analogous to sleep
 * in biological brains.
 *
 * WHY: Learning during wake (online learning) is fast but noisy. Consolidation
 * during rest allows the brain to:
 * - Strengthen important patterns
 * - Prune weak/redundant connections
 * - Integrate new knowledge with existing knowledge
 * - Reduce interference between competing memories
 * - Optimize network structure for efficiency
 *
 * This is critical for long-term learning stability and efficient memory use.
 *
 * HOW: Provides both synchronous and asynchronous consolidation:
 * - Explicit consolidation (like deliberate sleep)
 * - Background consolidation thread (like sleep/wake cycles)
 * - Replay-based consolidation (reactivate important patterns)
 * - Synaptic scaling (normalize connection strengths)
 * - Pruning weak connections (forget unimportant patterns)
 *
 * DESIGN PATTERNS:
 * - Strategy: Different consolidation strategies (replay, scaling, pruning)
 * - Observer: Callbacks for consolidation events
 * - Command: Consolidation operations as commands
 * - Thread Pool: Background consolidation thread
 * - Memento: Save/restore brain state during consolidation
 *
 * THREAD SAFETY: All functions are thread-safe. Background consolidation
 * runs in separate thread with mutex protection of brain state.
 *
 * PERFORMANCE:
 * - Synchronous consolidation: O(n*c) where n=network size, c=cycles, ~100ms-10s
 * - Background consolidation: Minimal overhead when not running
 * - Consolidation frequency: Configurable (default 300s = 5min)
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_CONSOLIDATION_H
#define NIMCP_CONSOLIDATION_H

#include <stdbool.h>
#include <stdint.h>
#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * TYPE DEFINITIONS
 * ======================================================================== */

/**
 * WHAT: Opaque handle to background consolidation process
 * WHY: Encapsulation - hide thread details
 * HOW: Pimpl idiom - pointer to internal structure
 */
typedef struct consolidation_handle_struct* consolidation_handle_t;

/**
 * WHAT: Consolidation strategy selection
 * WHY: Different strategies for different use cases
 * HOW: Enum to select consolidation method
 *
 * STRATEGIES:
 * - REPLAY: Replay important patterns to strengthen them
 * - SCALING: Normalize connection strengths (synaptic scaling)
 * - PRUNING: Remove weak connections to reduce noise
 * - INTEGRATION: Integrate new knowledge with existing
 * - FULL: All of the above (most comprehensive)
 */
typedef enum {
    CONSOLIDATION_STRATEGY_REPLAY,      /* Replay important patterns */
    CONSOLIDATION_STRATEGY_SCALING,     /* Synaptic scaling */
    CONSOLIDATION_STRATEGY_PRUNING,     /* Prune weak connections */
    CONSOLIDATION_STRATEGY_INTEGRATION, /* Integrate new + old */
    CONSOLIDATION_STRATEGY_FULL         /* All strategies */
} consolidation_strategy_t;

/**
 * WHAT: Priority for pattern consolidation
 * WHY: Focus consolidation on important patterns
 * HOW: Prioritize by recency, frequency, or importance
 */
typedef enum {
    CONSOLIDATION_PRIORITY_RECENT,    /* Recent patterns */
    CONSOLIDATION_PRIORITY_FREQUENT,  /* Frequently activated */
    CONSOLIDATION_PRIORITY_IMPORTANT, /* High-value patterns */
    CONSOLIDATION_PRIORITY_NOVEL,     /* Novel patterns */
    CONSOLIDATION_PRIORITY_ALL        /* All patterns equally */
} consolidation_priority_t;

/**
 * WHAT: Configuration for consolidation process
 * WHY: Customize consolidation behavior
 * HOW: Struct with all consolidation parameters
 */
typedef struct {
    consolidation_strategy_t strategy; /* Consolidation strategy */
    consolidation_priority_t priority; /* Pattern priority */

    uint32_t consolidation_cycles; /* Number of cycles (iterations) */
    float consolidation_strength;  /* Learning rate (0-1) */

    bool enable_replay;    /* Enable pattern replay? */
    uint32_t replay_count; /* Patterns to replay per cycle */

    bool enable_pruning;     /* Enable connection pruning? */
    float pruning_threshold; /* Prune connections below this */

    bool enable_scaling;  /* Enable synaptic scaling? */
    float scaling_target; /* Target average activation */

    bool prioritize_novel; /* Prioritize novel patterns? */
    float novelty_boost;   /* Boost for novel patterns */

    bool prune_weak;          /* Prune weak patterns? */
    float weakness_threshold; /* Pattern strength threshold */

    uint32_t max_prune_passes;  /* Max brain_prune_weak_connections calls (default 1) */
    float neuron_sample_rate;   /* Fraction of neurons for scaling stats (0.0-1.0, default 1.0) */

    bool use_community_cache;       /* Use cached community data for replay prioritization */
    float hub_consolidation_boost;  /* Extra strength for hub-associated patterns (default 1.5) */
    float cross_community_boost;    /* Extra strength for cross-community patterns (default 1.3) */

    void (*on_consolidation_start)(void* context);                    /* Callback: start */
    void (*on_consolidation_progress)(float progress, void* context); /* Progress */
    void (*on_consolidation_complete)(void* context);                 /* Callback: done */
    void* callback_context;                                           /* Context for callbacks */
} consolidation_config_t;

/**
 * WHAT: Statistics from consolidation process
 * WHY: Monitor consolidation effectiveness
 * HOW: Counters and metrics collected during consolidation
 */
typedef struct {
    uint64_t total_consolidations;     /* Total consolidation runs */
    uint64_t patterns_replayed;        /* Patterns replayed */
    uint64_t connections_pruned;       /* Connections removed */
    uint64_t connections_strengthened; /* Connections increased */
    uint64_t connections_weakened;     /* Connections decreased */

    float avg_consolidation_time_ms;  /* Average time per run */
    float last_consolidation_time_ms; /* Last run time */

    float network_sparsity_before; /* Sparsity before last run */
    float network_sparsity_after;  /* Sparsity after last run */

    float avg_connection_strength_before; /* Avg strength before */
    float avg_connection_strength_after;  /* Avg strength after */

    uint32_t patterns_strengthened; /* Patterns made stronger */
    uint32_t patterns_weakened;     /* Patterns made weaker */
    uint32_t patterns_removed;      /* Patterns pruned */

    uint64_t last_consolidation_timestamp; /* Timestamp of last run */
} consolidation_stats_t;

/**
 * WHAT: Pattern importance information
 * WHY: Used to prioritize which patterns to consolidate
 * HOW: Metadata about pattern usage and value
 */
typedef struct {
    char* pattern_name;        /* Pattern identifier */
    float importance_score;    /* Overall importance (0-1) */
    float recency_score;       /* How recent (0-1) */
    float frequency_score;     /* How frequent (0-1) */
    float novelty_score;       /* How novel (0-1) */
    float strength;            /* Current strength (0-1) */
    uint32_t activation_count; /* Times activated */
    uint64_t last_activated;   /* Last activation time */
} pattern_importance_t;

/**
 * WHAT: Consolidation event types
 * WHY: Notify application of consolidation events
 * HOW: Enum for event type discrimination
 */
typedef enum {
    CONSOLIDATION_EVENT_STARTED,           /* Consolidation started */
    CONSOLIDATION_EVENT_CYCLE_COMPLETE,    /* One cycle finished */
    CONSOLIDATION_EVENT_PATTERN_REPLAYED,  /* Pattern was replayed */
    CONSOLIDATION_EVENT_CONNECTION_PRUNED, /* Connection was pruned */
    CONSOLIDATION_EVENT_SCALING_APPLIED,   /* Scaling was applied */
    CONSOLIDATION_EVENT_COMPLETED,         /* Consolidation finished */
    CONSOLIDATION_EVENT_ERROR              /* Error occurred */
} consolidation_event_type_t;

/**
 * WHAT: Consolidation event structure
 * WHY: Detailed information about consolidation events
 * HOW: Event type + relevant data
 */
typedef struct {
    consolidation_event_type_t type; /* Event type */
    uint64_t timestamp;              /* When event occurred */
    float progress;                  /* Progress (0-1) */
    uint32_t cycle_number;           /* Current cycle */
    char* pattern_name;              /* Pattern involved (if any) */
    char* message;                   /* Human-readable message */
} consolidation_event_t;

/* ========================================================================
 * CORE CONSOLIDATION API
 * ======================================================================== */

/**
 * WHAT: Get default consolidation configuration
 * WHY: Sensible defaults for most use cases
 * HOW: Returns pre-configured struct with balanced settings
 *
 * DEFAULT SETTINGS:
 * - Strategy: FULL (all consolidation methods)
 * - Priority: IMPORTANT (focus on high-value patterns)
 * - Cycles: 10
 * - Strength: 0.1 (gentle consolidation)
 * - Replay enabled: 100 patterns per cycle
 * - Pruning enabled: threshold 0.01
 * - Scaling enabled: target 0.5
 * - Prioritize novel: true (boost 1.5x)
 * - Prune weak: true (threshold 0.1)
 *
 * @return Default configuration struct
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
consolidation_config_t consolidation_default_config(void);

/**
 * WHAT: Get lightweight consolidation configuration (replay only)
 * WHY: Fast consolidation for warm-up phases — milliseconds, not minutes
 * HOW: Replay only, 2 cycles, no scaling or pruning
 *
 * @return Light configuration struct
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
consolidation_config_t consolidation_light_config(void);

/**
 * WHAT: Get scale-aware consolidation configuration
 * WHY: Automatically adjust cycles, sampling, and pruning for network size
 * HOW: Fewer cycles and lower sample rates for larger networks
 *
 * SCALE TIERS:
 * - < 10K neurons: 10 cycles, 100% sampling, 1 prune pass
 * - 10K-500K: 5 cycles, 50% sampling, 1 prune pass
 * - 500K-2M: 3 cycles, 10% sampling, 1 prune pass
 * - > 2M: 2 cycles, 5% sampling, 1 prune pass
 *
 * @param num_neurons Number of neurons in the network
 * @return Scale-aware configuration struct
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
consolidation_config_t consolidation_auto_config(uint32_t num_neurons);

/**
 * WHAT: Perform synchronous memory consolidation
 * WHY: Explicit consolidation like deliberate sleep
 * HOW: Run consolidation cycles on calling thread
 *
 * DESIGN PATTERN: Strategy (uses configured consolidation strategy)
 *
 * PROCESS:
 * 1. Lock brain state (prevent concurrent modifications)
 * 2. For each consolidation cycle:
 *    a. Replay important patterns (if enabled)
 *    b. Apply synaptic scaling (if enabled)
 *    c. Prune weak connections (if enabled)
 *    d. Invoke progress callback
 * 3. Unlock brain state
 * 4. Return success
 *
 * USAGE:
 * ```c
 * consolidation_config_t config = consolidation_default_config();
 * config.consolidation_cycles = 20;
 * config.consolidation_strength = 0.2;
 *
 * bool success = brain_consolidate_memory(brain, &config);
 * if (success) {
 *     printf("Consolidation complete\n");
 * }
 * ```
 *
 * @param brain The brain to consolidate
 * @param config Configuration (NULL for defaults)
 * @return true on success, false on error
 *
 * ERRORS:
 * - Returns false if brain is NULL
 * - Returns false if consolidation fails
 *
 * BLOCKING: Yes - blocks until consolidation complete
 * TIME: ~100ms to 10s depending on network size and cycles
 *
 * COMPLEXITY: O(n*c) where n=network size, c=cycles
 * THREAD-SAFE: Yes (acquires brain lock)
 */
bool brain_consolidate_memory(brain_t brain, const consolidation_config_t* config);

/**
 * WHAT: Start background consolidation thread
 * WHY: Automatic periodic consolidation like sleep/wake cycles
 * HOW: Spawn thread that consolidates at intervals
 *
 * DESIGN PATTERN: Thread Pool (single background thread)
 *
 * USAGE:
 * ```c
 * // Consolidate every 5 minutes
 * consolidation_config_t config = consolidation_default_config();
 * consolidation_handle_t handle = brain_start_background_consolidation(
 *     brain,
 *     300,  // 300 seconds = 5 minutes
 *     &config
 * );
 *
 * // Run for hours...
 *
 * // Stop when done
 * brain_stop_background_consolidation(handle);
 * ```
 *
 * @param brain The brain to consolidate
 * @param interval_seconds Seconds between consolidation runs
 * @param config Configuration (NULL for defaults)
 * @return Consolidation handle, or NULL on error
 *
 * ERRORS:
 * - Returns NULL if brain is NULL
 * - Returns NULL if thread creation fails
 *
 * MEMORY: Caller must call brain_stop_background_consolidation() when done
 *
 * COMPLEXITY: O(1) for thread creation
 * THREAD-SAFE: Yes
 */
consolidation_handle_t brain_start_background_consolidation(brain_t brain,
                                                            uint32_t interval_seconds,
                                                            const consolidation_config_t* config);

/**
 * WHAT: Stop background consolidation thread
 * WHY: Graceful shutdown of background consolidation
 * HOW: Signal thread to stop, wait for completion, free resources
 *
 * DESIGN PATTERN: Thread Pool (cleanup)
 *
 * @param handle Consolidation handle to stop
 *
 * BLOCKING: Yes - waits for current consolidation to finish
 * TIME: Up to one full consolidation cycle
 *
 * SAFETY: Safe to call with NULL handle
 * MEMORY: Frees all allocations
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void brain_stop_background_consolidation(consolidation_handle_t handle);

/**
 * WHAT: Pause background consolidation
 * WHY: Temporarily disable consolidation without stopping thread
 * HOW: Set pause flag, thread skips consolidation when paused
 *
 * @param handle Consolidation handle
 *
 * USAGE: Pause during intensive learning periods
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void brain_pause_consolidation(consolidation_handle_t handle);

/**
 * WHAT: Resume background consolidation
 * WHY: Re-enable after pause
 * HOW: Clear pause flag
 *
 * @param handle Consolidation handle
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void brain_resume_consolidation(consolidation_handle_t handle);

/**
 * WHAT: Trigger immediate consolidation (background thread)
 * WHY: Force consolidation now instead of waiting for interval
 * HOW: Signal background thread to run consolidation
 *
 * @param handle Consolidation handle
 * @return true if signal sent, false on error
 *
 * NON-BLOCKING: Returns immediately, consolidation runs in background
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool brain_trigger_consolidation(consolidation_handle_t handle);

/* ========================================================================
 * PATTERN MANAGEMENT
 * ======================================================================== */

/**
 * WHAT: Get list of important patterns for consolidation
 * WHY: See which patterns will be prioritized
 * HOW: Analyze pattern registry, compute importance scores
 *
 * IMPORTANCE SCORE: Weighted combination of:
 * - Recency: When last activated
 * - Frequency: How often activated
 * - Novelty: How different from existing patterns
 * - Strength: Current pattern strength
 *
 * @param brain The brain to analyze
 * @param num_patterns Output: number of patterns
 * @return Array of pattern importance (must be freed)
 *
 * ERRORS: Returns NULL if brain is NULL
 *
 * MEMORY: Caller must free returned array
 *
 * COMPLEXITY: O(p) where p = number of patterns
 * THREAD-SAFE: Yes
 */
pattern_importance_t* brain_get_important_patterns(brain_t brain, uint32_t* num_patterns);

/**
 * WHAT: Free pattern importance array
 * WHY: Release memory from brain_get_important_patterns
 * HOW: Free each pattern name, free array
 *
 * @param patterns Array to free
 * @param num_patterns Number of patterns
 *
 * SAFETY: Safe to call with NULL
 *
 * COMPLEXITY: O(p) where p = number of patterns
 * THREAD-SAFE: Yes
 */
void pattern_importance_free(pattern_importance_t* patterns, uint32_t num_patterns);

/**
 * WHAT: Mark pattern as important
 * WHY: Manually boost pattern priority for consolidation
 * HOW: Set importance flag in pattern metadata
 *
 * @param brain The brain
 * @param pattern_name Pattern to mark important
 * @param importance_score Importance (0-1)
 * @return true on success, false on error
 *
 * USAGE: Mark critical patterns to protect from pruning
 *
 * COMPLEXITY: O(1) - hash lookup
 * THREAD-SAFE: Yes
 */
bool brain_mark_pattern_important(brain_t brain, const char* pattern_name, float importance_score);

/* ========================================================================
 * STATISTICS AND MONITORING
 * ======================================================================== */

/**
 * WHAT: Get consolidation statistics
 * WHY: Monitor consolidation effectiveness
 * HOW: Return statistics structure
 *
 * @param handle Consolidation handle (NULL for last synchronous consolidation)
 * @param stats Output: statistics structure
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool consolidation_get_stats(consolidation_handle_t handle, consolidation_stats_t* stats);

/**
 * WHAT: Reset consolidation statistics
 * WHY: Clear counters for new measurement period
 * HOW: Zero all counters
 *
 * @param handle Consolidation handle (NULL for global sync stats)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void consolidation_reset_stats(consolidation_handle_t handle);

/**
 * WHAT: Reset global consolidation state to initial values
 * WHY: Test isolation - prevent state contamination between tests
 * HOW: Reset g_sync_stats to zero, re-initialize mutex if needed
 *
 * USAGE: Call between tests to restore clean state
 * NOTE: Only affects global synchronous consolidation state
 * THREAD-SAFE: Yes
 *
 * COMPLEXITY: O(1)
 */
void consolidation_reset_global_state(void);

/**
 * WHAT: Check if consolidation is currently running
 * WHY: Avoid triggering concurrent consolidations
 * HOW: Check running flag
 *
 * @param handle Consolidation handle
 * @return true if consolidation is running, false otherwise
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool consolidation_is_running(consolidation_handle_t handle);

/**
 * WHAT: Get progress of current consolidation
 * WHY: Monitor long-running consolidation
 * HOW: Return progress (0-1)
 *
 * @param handle Consolidation handle
 * @return Progress (0-1), or -1 if not running
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float consolidation_get_progress(consolidation_handle_t handle);

/* ========================================================================
 * ADVANCED CONSOLIDATION
 * ======================================================================== */

/**
 * WHAT: Replay specific pattern for consolidation
 * WHY: Manually strengthen specific pattern
 * HOW: Activate pattern, backpropagate to strengthen
 *
 * @param brain The brain
 * @param pattern_name Pattern to replay
 * @param replay_count Number of replays
 * @param strength Learning rate (0-1)
 * @return true on success, false on error
 *
 * USAGE: Protect important patterns from forgetting
 *
 * COMPLEXITY: O(replay_count * pattern_size)
 * THREAD-SAFE: Yes
 */
bool brain_replay_pattern(brain_t brain, const char* pattern_name, uint32_t replay_count,
                          float strength);

/**
 * WHAT: Apply synaptic scaling to normalize connections
 * WHY: Prevent runaway activation or dead neurons
 * HOW: Scale all connection weights to target average
 *
 * SYNAPTIC SCALING: Biological mechanism to maintain homeostasis
 * - Prevents explosive growth of strong connections
 * - Prevents complete silence of weak connections
 * - Maintains network sensitivity
 *
 * @param brain The brain
 * @param target_activation Target average activation (0-1)
 * @return true on success, false on error
 *
 * COMPLEXITY: O(n + e) where n=neurons, e=edges
 * THREAD-SAFE: Yes
 */
bool brain_apply_synaptic_scaling(brain_t brain, float target_activation);

/**
 * WHAT: Prune weak connections below threshold
 * WHY: Remove noise, improve efficiency
 * HOW: Remove connections with strength < threshold
 *
 * @param brain The brain
 * @param threshold Prune connections below this strength
 * @return Number of connections pruned
 *
 * COMPLEXITY: O(e) where e = number of edges
 * THREAD-SAFE: Yes
 */
uint32_t brain_prune_weak_connections(brain_t brain, float threshold);

/* ========================================================================
 * COMMUNITY CACHE FOR CONSOLIDATION
 * ======================================================================== */

/**
 * WHAT: Cached community detection results for consolidation replay
 * WHY: Full Louvain community detection is O(N log N) and takes hours on 2M+
 *      neurons. Pre-computing and caching allows consolidation replay to use
 *      community-aware prioritization without re-triggering the expensive
 *      algorithm every cycle.
 * HOW: Stores community_structure_t and hub_detection_t snapshots with
 *      staleness tracking based on learning events and timestamps.
 *
 * BIOLOGICAL BASIS: Brain regions maintain stable community structure across
 * sleep cycles. Community boundaries shift gradually during learning but
 * remain valid for consolidation within a training phase.
 */
typedef struct consolidation_community_cache {
    uint32_t* community_ids;        /* Community assignment per neuron (size: num_neurons) */
    uint32_t num_neurons;           /* Neuron count when cache was built */
    uint32_t num_communities;       /* Number of detected communities */
    uint32_t* community_sizes;      /* Size of each community (size: num_communities) */
    float modularity;               /* Newman's Q score at cache time */

    uint32_t* hub_neuron_ids;       /* Array of hub neuron IDs */
    float* hub_centralities;        /* Degree centrality for each hub */
    uint32_t* hub_community_ids;    /* Community ID for each hub */
    bool* hub_is_connector;         /* Is this a connector hub (bridges communities)? */
    uint32_t num_hubs;              /* Number of hub neurons */

    uint64_t cache_timestamp_ms;    /* When cache was built (monotonic ms) */
    uint64_t learning_events_at_cache; /* Learning event counter at cache time */

    bool is_valid;                  /* Is cache populated and usable? */
} consolidation_community_cache_t;

/**
 * WHAT: Create an empty community cache
 * WHY: Allocate cache structure before populating
 * HOW: calloc + zero-init
 *
 * @return Cache pointer, or NULL on allocation failure
 *
 * MEMORY: Caller must call consolidation_community_cache_destroy()
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
consolidation_community_cache_t* consolidation_community_cache_create(void);

/**
 * WHAT: Destroy community cache and free all internal arrays
 * WHY: Prevent memory leaks
 * HOW: Free all arrays, then free struct
 *
 * @param cache Cache to destroy (safe to call with NULL)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void consolidation_community_cache_destroy(consolidation_community_cache_t* cache);

/**
 * WHAT: Populate cache by running community detection on the brain's network
 * WHY: Pre-compute expensive Louvain algorithm when cost is acceptable
 *      (e.g., at end of a training epoch, not inside consolidation hot path)
 * HOW: Calls community_detect() and community_detect_hubs() on the brain's
 *      underlying neural network, copies results into cache
 *
 * @param cache Cache to populate (must not be NULL)
 * @param brain Brain whose network to analyze
 * @return true on success, false on error (cache->is_valid set accordingly)
 *
 * BLOCKING: Yes — runs Louvain O(N log N)
 * THREAD-SAFE: No (caller must ensure exclusive access to cache)
 */
bool consolidation_cache_communities(consolidation_community_cache_t* cache, brain_t brain);

/**
 * WHAT: Check whether cached community data is still valid/fresh
 * WHY: Detect when learning has changed network topology enough to invalidate
 * HOW: Compare neuron count and check age against threshold
 *
 * @param cache Cache to check
 * @param brain Current brain (for neuron count comparison)
 * @param max_age_ms Maximum cache age in milliseconds (0 = no age limit)
 * @return true if cache is valid and usable, false if stale or empty
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 */
bool consolidation_community_cache_is_valid(
    const consolidation_community_cache_t* cache,
    brain_t brain,
    uint64_t max_age_ms);

/**
 * WHAT: Invalidate the cache (mark as stale)
 * WHY: Force re-computation on next cache_communities call
 * HOW: Set is_valid = false (does NOT free arrays — reuse on next populate)
 *
 * @param cache Cache to invalidate
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void consolidation_community_cache_invalidate(consolidation_community_cache_t* cache);

/**
 * WHAT: Look up which community a neuron belongs to
 * WHY: Query during community-aware replay
 * HOW: Array index lookup
 *
 * @param cache Populated cache
 * @param neuron_id Neuron to query
 * @return Community ID, or UINT32_MAX if invalid
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 */
uint32_t consolidation_cache_get_community(
    const consolidation_community_cache_t* cache,
    uint32_t neuron_id);

/**
 * WHAT: Check if a neuron is a hub in the cached topology
 * WHY: Hub neurons get boosted consolidation during replay
 * HOW: Linear scan of hub array (typically small, < 1% of neurons)
 *
 * @param cache Populated cache
 * @param neuron_id Neuron to query
 * @param out_centrality Output: centrality score (NULL to skip)
 * @param out_is_connector Output: is connector hub (NULL to skip)
 * @return true if neuron is a hub, false otherwise
 *
 * COMPLEXITY: O(H) where H = number of hubs
 * THREAD-SAFE: Yes (read-only)
 */
bool consolidation_cache_is_hub(
    const consolidation_community_cache_t* cache,
    uint32_t neuron_id,
    float* out_centrality,
    bool* out_is_connector);

/**
 * WHAT: Perform community-aware memory consolidation
 * WHY: Use cached community structure to boost hub-associated and
 *      cross-community patterns during replay
 * HOW: Like brain_consolidate_memory but with community cache integration
 *
 * @param brain The brain to consolidate
 * @param config Configuration (NULL for defaults with community awareness)
 * @param cache Community cache (NULL falls back to standard consolidation)
 * @return true on success, false on error
 *
 * BLOCKING: Yes
 * COMPLEXITY: O(n*c) where n=network size, c=cycles
 * THREAD-SAFE: Yes (acquires brain lock)
 */
bool brain_consolidate_memory_community_aware(
    brain_t brain,
    const consolidation_config_t* config,
    const consolidation_community_cache_t* cache);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CONSOLIDATION_H */
