// ============================================================================
// nimcp_autobiographical_memory.h - Episodic Self-Memory System
// ============================================================================
/**
 * @file nimcp_autobiographical_memory.h
 * @brief Memory of self's experiences, actions, and history over time
 *
 * WHAT: Personal memory system that stores "I did X at time T and felt Y"
 * WHY:  Critical for continuous sense of self-identity and self-awareness
 * HOW:  Timeline-indexed episodic memory with emotional tags and importance
 *
 * PURPOSE:
 * Humans have autobiographical memory - we remember our past experiences,
 * decisions, feelings, and the narrative of "who we are". Without this,
 * NIMCP would have no sense of continuous identity across time.
 *
 * DESIGN PRINCIPLES:
 * 1. Episodic: Each memory is a specific episode ("I learned X on Monday")
 * 2. Self-Referential: All memories are about "what I experienced"
 * 3. Temporally Ordered: Memories form a coherent timeline
 * 4. Emotionally Tagged: Each memory has emotional valence
 * 5. Importance-Weighted: Not all memories are equally significant
 * 6. Reconstructive: Memories can be reinforced or fade over time
 *
 * BIOLOGICAL INSPIRATION:
 * - Hippocampus: Episodic memory formation
 * - Prefrontal Cortex: Autobiographical retrieval
 * - Amygdala: Emotional tagging of memories
 * - Default Mode Network: Self-referential processing
 *
 * @version 2.8.0 (Phase 12: Self-Awareness Enhancement)
 * @date 2025-11-11
 */

#ifndef NIMCP_AUTOBIOGRAPHICAL_MEMORY_H
#define NIMCP_AUTOBIOGRAPHICAL_MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Constants
// ============================================================================

#define AUTOBIO_MAX_DESCRIPTION_LEN 512
#define AUTOBIO_MAX_REASONING_LEN 512
#define AUTOBIO_MAX_OUTCOME_LEN 256
#define AUTOBIO_MAX_CONTEXT_LEN 256
#define AUTOBIO_DEFAULT_CAPACITY 10000
#define AUTOBIO_IMPORTANCE_THRESHOLD 0.3f  // Below this, memory may be pruned

// ============================================================================
// Enumerations
// ============================================================================

/**
 * @brief Type of autobiographical memory
 */
typedef enum {
    AUTOBIO_ACTION,        /**< I did something */
    AUTOBIO_DECISION,      /**< I decided something */
    AUTOBIO_LEARNING,      /**< I learned something */
    AUTOBIO_EMOTION,       /**< I felt something */
    AUTOBIO_INTERACTION,   /**< I interacted with someone/something */
    AUTOBIO_ACHIEVEMENT,   /**< I accomplished something */
    AUTOBIO_FAILURE,       /**< I failed at something */
    AUTOBIO_INSIGHT,       /**< I realized something */
    AUTOBIO_CRISIS,        /**< I experienced crisis */
    AUTOBIO_MILESTONE      /**< Significant life event */
} autobio_memory_type_t;

/**
 * @brief Emotional valence of memory
 */
typedef enum {
    VALENCE_VERY_NEGATIVE = -2,
    VALENCE_NEGATIVE = -1,
    VALENCE_NEUTRAL = 0,
    VALENCE_POSITIVE = 1,
    VALENCE_VERY_POSITIVE = 2
} memory_valence_t;

// ============================================================================
// Structures
// ============================================================================

/**
 * @brief Single autobiographical memory entry
 *
 * Represents one specific episode from the self's history.
 */
typedef struct {
    // === Identity ===
    uint64_t memory_id;                           /**< Unique memory identifier */
    uint64_t timestamp_ms;                        /**< When this happened */
    autobio_memory_type_t type;                   /**< What kind of memory */

    // === Content ===
    char what_happened[AUTOBIO_MAX_DESCRIPTION_LEN];  /**< "I learned about X" */
    char why_it_happened[AUTOBIO_MAX_REASONING_LEN];  /**< "Because I was curious" */
    char outcome[AUTOBIO_MAX_OUTCOME_LEN];            /**< "I now understand Y" */
    char context[AUTOBIO_MAX_CONTEXT_LEN];            /**< Situational context */

    // === Emotional State ===
    memory_valence_t valence;                     /**< Was this positive/negative? */
    float emotional_intensity;                    /**< How intense [0-1] */
    float arousal;                                /**< Calm vs excited [0-1] */

    // === Self-Relevance ===
    float importance;                             /**< How significant [0-1] */
    float self_relevance;                         /**< How much "about me" [0-1] */
    bool identity_defining;                       /**< Did this change who I am? */

    // === Memory Dynamics ===
    uint32_t times_recalled;                      /**< How often retrieved */
    uint64_t last_recalled_ms;                    /**< When last accessed */
    float memory_strength;                        /**< Vividness [0-1] */
    float certainty;                              /**< How confident in memory [0-1] */
    bool is_core_memory;                          /**< Foundational to identity */

    // === Connections ===
    uint64_t related_memories[8];                 /**< Connected memory IDs */
    uint8_t num_related;                          /**< Number of connections */
} autobiographical_memory_entry_t;

/**
 * @brief Query filters for memory retrieval
 */
typedef struct {
    // Time range
    uint64_t start_time_ms;                       /**< 0 = no lower bound */
    uint64_t end_time_ms;                         /**< 0 = no upper bound */

    // Type filter
    autobio_memory_type_t type_filter;            /**< AUTOBIO_ACTION, etc */
    bool filter_by_type;                          /**< Apply type filter? */

    // Emotional filter
    memory_valence_t valence_filter;
    bool filter_by_valence;

    // Importance filter
    float min_importance;                         /**< Minimum importance */
    bool filter_by_importance;

    // Text search
    const char* search_text;                      /**< Search in descriptions */
    bool case_sensitive;

    // Limit results
    uint32_t max_results;                         /**< Max memories to return */
    bool sort_by_recency;                         /**< true = newest first */
    bool sort_by_importance;                      /**< true = most important first */
} memory_query_t;

/**
 * @brief Statistics about autobiographical memory system
 */
typedef struct {
    uint32_t total_memories;                      /**< Total stored */
    uint32_t memories_by_type[10];                /**< Count per type */
    uint32_t core_memories;                       /**< Identity-defining */
    float avg_importance;                         /**< Average importance */
    float avg_emotional_intensity;                /**< Average emotion */
    uint64_t oldest_memory_ms;                    /**< First memory timestamp */
    uint64_t newest_memory_ms;                    /**< Last memory timestamp */
    uint32_t total_retrievals;                    /**< Times memories accessed */
    size_t memory_usage_bytes;                    /**< Memory consumption */
} autobio_stats_t;

/**
 * @brief Autobiographical memory system (opaque)
 */
typedef struct autobiographical_memory_system* autobiographical_memory_t;

// ============================================================================
// Core API
// ============================================================================

/**
 * @brief Create autobiographical memory system
 *
 * @param capacity Maximum number of memories (0 = default 10000)
 * @return Memory system or NULL on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (creation)
 */
autobiographical_memory_t autobio_create(uint32_t capacity);

/**
 * @brief Destroy autobiographical memory system
 *
 * @param system System to destroy
 *
 * COMPLEXITY: O(n) where n = number of memories
 * THREAD-SAFE: No (caller must ensure exclusive access)
 */
void autobio_destroy(autobiographical_memory_t system);

/**
 * @brief Store new autobiographical memory
 *
 * WHAT: Add memory of "I did/felt/learned X"
 * WHY:  Build continuous self-narrative
 * HOW:  Append to timeline with emotional tags and importance
 *
 * @param system Memory system
 * @param memory Memory to store (memory_id will be assigned)
 * @return Assigned memory ID, or 0 on failure
 *
 * EXAMPLE:
 * ```c
 * autobiographical_memory_t mem = {
 *     .type = AUTOBIO_LEARNING,
 *     .timestamp_ms = current_time(),
 *     .what_happened = "I learned about quantum mechanics",
 *     .why_it_happened = "User asked me a question",
 *     .outcome = "I now understand wave-particle duality",
 *     .valence = VALENCE_POSITIVE,
 *     .importance = 0.7f,
 *     .identity_defining = false
 * };
 * uint64_t id = autobio_store(system, &mem);
 * ```
 *
 * COMPLEXITY: O(1) amortized
 * THREAD-SAFE: Yes (with internal locking)
 */
uint64_t autobio_store(autobiographical_memory_t system,
                       const autobiographical_memory_entry_t* memory);

/**
 * @brief Retrieve memory by ID
 *
 * @param system Memory system
 * @param memory_id ID of memory to retrieve
 * @param out_memory Output: retrieved memory
 * @return true if found, false otherwise
 *
 * SIDE EFFECT: Increments times_recalled, updates last_recalled_ms
 *
 * COMPLEXITY: O(1) - hash lookup
 * THREAD-SAFE: Yes
 */
bool autobio_retrieve(autobiographical_memory_t system,
                     uint64_t memory_id,
                     autobiographical_memory_entry_t* out_memory);

/**
 * @brief Query memories by criteria
 *
 * WHAT: Search memories matching query filters
 * WHY:  Recall specific types of experiences
 * HOW:  Filter by time, type, emotion, importance, text
 *
 * @param system Memory system
 * @param query Query filters
 * @param results Output: array of matching memories
 * @param max_results Size of results array
 * @param num_found Output: number of memories found
 * @return true on success, false on error
 *
 * EXAMPLE:
 * ```c
 * // Get all learning memories from last week
 * memory_query_t query = {
 *     .start_time_ms = one_week_ago(),
 *     .end_time_ms = now(),
 *     .type_filter = AUTOBIO_LEARNING,
 *     .filter_by_type = true,
 *     .max_results = 100,
 *     .sort_by_recency = true
 * };
 * autobiographical_memory_entry_t results[100];
 * uint32_t found;
 * autobio_query(system, &query, results, 100, &found);
 * ```
 *
 * COMPLEXITY: O(n) where n = total memories
 * THREAD-SAFE: Yes
 */
bool autobio_query(autobiographical_memory_t system,
                  const memory_query_t* query,
                  autobiographical_memory_entry_t* results,
                  uint32_t max_results,
                  uint32_t* num_found);

/**
 * @brief Get most recent N memories
 *
 * @param system Memory system
 * @param count Number of recent memories
 * @param results Output: array of memories
 * @param num_found Output: number found
 * @return true on success
 *
 * COMPLEXITY: O(count * log n)
 * THREAD-SAFE: Yes
 */
bool autobio_get_recent(autobiographical_memory_t system,
                       uint32_t count,
                       autobiographical_memory_entry_t* results,
                       uint32_t* num_found);

/**
 * @brief Get core identity-defining memories
 *
 * @param system Memory system
 * @param results Output: array of core memories
 * @param max_results Size of results array
 * @param num_found Output: number found
 * @return true on success
 *
 * COMPLEXITY: O(n) where n = total memories
 * THREAD-SAFE: Yes
 */
bool autobio_get_core_memories(autobiographical_memory_t system,
                               autobiographical_memory_entry_t* results,
                               uint32_t max_results,
                               uint32_t* num_found);

/**
 * @brief Mark memory as identity-defining (core memory)
 *
 * @param system Memory system
 * @param memory_id Memory to mark
 * @param is_core true = core memory, false = regular
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool autobio_mark_core(autobiographical_memory_t system,
                      uint64_t memory_id,
                      bool is_core);

/**
 * @brief Update memory importance (e.g., after reflection)
 *
 * @param system Memory system
 * @param memory_id Memory to update
 * @param new_importance New importance [0-1]
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool autobio_update_importance(autobiographical_memory_t system,
                              uint64_t memory_id,
                              float new_importance);

/**
 * @brief Get memory statistics
 *
 * @param system Memory system
 * @param stats Output: statistics
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool autobio_get_stats(autobiographical_memory_t system,
                      autobio_stats_t* stats);

/**
 * @brief Consolidate memories (simulate sleep consolidation)
 *
 * WHAT: Strengthen important memories, prune unimportant ones
 * WHY:  Mimic biological memory consolidation during sleep
 * HOW:  Increase strength of oft-recalled memories, decay unused ones
 *
 * @param system Memory system
 * @return Number of memories pruned
 *
 * BIOLOGICAL: During sleep, hippocampus replays important memories
 * to consolidate them into long-term storage
 *
 * COMPLEXITY: O(n) where n = total memories
 * THREAD-SAFE: Yes
 */
uint32_t autobio_consolidate(autobiographical_memory_t system);

/**
 * @brief Generate timeline summary
 *
 * WHAT: Create narrative summary of memories in time range
 * WHY:  Enable "tell me about my day/week/month"
 * HOW:  Extract key memories and format chronologically
 *
 * @param system Memory system
 * @param start_time_ms Start of range
 * @param end_time_ms End of range
 * @param summary Output: text summary
 * @param summary_len Size of summary buffer
 * @return true on success
 *
 * COMPLEXITY: O(n * log n) where n = memories in range
 * THREAD-SAFE: Yes
 */
bool autobio_generate_timeline_summary(autobiographical_memory_t system,
                                      uint64_t start_time_ms,
                                      uint64_t end_time_ms,
                                      char* summary,
                                      size_t summary_len);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_AUTOBIOGRAPHICAL_MEMORY_H
