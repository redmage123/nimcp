/**
 * @file nimcp_recovery_episodic_memory.c
 * @brief Implementation of episodic memory for recovery history
 *
 * WHAT: Circular buffer with LSH indexing for content-addressable episode recall
 * WHY:  Enable learning from recovery experience
 * HOW:  Circular buffer + LSH hash tables + emotional prioritization
 *
 * ARCHITECTURE:
 * - Episodes stored in circular buffer (FIFO with emotional override)
 * - LSH tables provide O(log N) similarity search
 * - Emotional tags influence eviction priority
 * - Consolidation extracts semantic patterns
 *
 * PERFORMANCE:
 * - Storage: O(1) average
 * - Retrieval: O(log N) with LSH
 * - Memory: ~1.6KB per episode
 *
 * @author NIMCP Development Team
 * @date 2025-01-09
 * @version 2.7.0 Phase 10.1
 */

#include "cognitive/fault_tolerance/nimcp_recovery_episodic_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

//=============================================================================
// LSH (Locality-Sensitive Hashing) Structures
//=============================================================================

/**
 * @brief LSH bucket - stores episode indices with same hash
 *
 * WHAT: Bucket in LSH hash table
 * WHY:  Enable fast similarity lookup
 */
typedef struct lsh_bucket {
    uint32_t* episode_indices;    /**< Indices into episode buffer */
    uint32_t count;               /**< Number of episodes in bucket */
    uint32_t capacity;            /**< Allocated capacity */
} lsh_bucket_t;

/**
 * @brief LSH table - one hash table in LSH family
 *
 * WHAT: Single hash table with multiple buckets
 * WHY:  Part of LSH multi-table approach
 */
typedef struct lsh_table {
    lsh_bucket_t* buckets;        /**< Array of buckets */
    uint32_t num_buckets;         /**< Number of buckets (power of 2) */
    uint64_t hash_seed;           /**< Random seed for this table */
} lsh_table_t;

//=============================================================================
// Private Episodic Memory Structure
//=============================================================================

/**
 * @brief Episodic memory internal structure
 *
 * WHAT: Complete episodic memory state
 * WHY:  Encapsulation and efficient access
 */
struct episodic_memory {
    // ======== Configuration ========
    episodic_memory_config_t config;

    // ======== Episode Storage (Circular Buffer) ========
    recovery_episode_t* episodes; /**< Circular buffer of episodes */
    uint32_t capacity;            /**< Max episodes (from config) */
    uint32_t count;               /**< Current episode count */
    uint32_t head;                /**< Next write position */

    // ======== LSH Indexing ========
    lsh_table_t** lsh_tables;     /**< Array of LSH table pointers */
    uint32_t num_lsh_tables;      /**< Number of tables */

    // ======== ID Management ========
    uint64_t next_episode_id;     /**< Auto-increment episode ID */

    // ======== Statistics ========
    episodic_memory_stats_t stats;
};

//=============================================================================
// LSH Hash Functions
//=============================================================================

/**
 * @brief Compute LSH hash for error signature
 *
 * WHAT: Hash error signature using table-specific seed
 * WHY:  Distribute similar episodes into same bucket
 * HOW:  XOR-shift hash with seed
 *
 * COMPLEXITY: O(1)
 *
 * @param sig Error signature to hash
 * @param seed Random seed for this table
 * @return Hash value
 */
static inline uint64_t lsh_hash_signature(
    const error_signature_t* sig,
    uint64_t seed)
{
    uint64_t hash = sig->signature_hash ^ seed;
    hash ^= (hash >> 33);
    hash *= 0xff51afd7ed558ccdULL;
    hash ^= (hash >> 33);
    hash *= 0xc4ceb9fe1a85ec53ULL;
    hash ^= (hash >> 33);
    return hash;
}

/**
 * @brief Get bucket index from hash
 *
 * WHAT: Map hash to bucket index
 * WHY:  Determine which bucket to use
 * HOW:  Modulo by num_buckets (power of 2, use AND)
 *
 * COMPLEXITY: O(1)
 */
static inline uint32_t lsh_bucket_index(uint64_t hash, uint32_t num_buckets)
{
    return (uint32_t)(hash & (num_buckets - 1));
}

//=============================================================================
// LSH Bucket Operations
//=============================================================================

/**
 * @brief Add episode index to LSH bucket
 *
 * WHAT: Insert episode index into bucket
 * WHY:  Index episode for fast retrieval
 * HOW:  Append to bucket array (grow if needed)
 *
 * COMPLEXITY: O(1) amortized
 */
static bool lsh_bucket_add(lsh_bucket_t* bucket, uint32_t episode_idx)
{
    if (!bucket) return false;

    // Grow if at capacity
    if (bucket->count >= bucket->capacity) {
        uint32_t new_capacity = bucket->capacity > 0 ?
            bucket->capacity * 2 : 16;

        uint32_t* new_indices = nimcp_realloc(
            bucket->episode_indices,
            new_capacity * sizeof(uint32_t));

        if (!new_indices) {
            LOG_ERROR("Failed to grow LSH bucket");
            return false;
        }

        bucket->episode_indices = new_indices;
        bucket->capacity = new_capacity;
    }

    // Add index
    bucket->episode_indices[bucket->count++] = episode_idx;
    return true;
}

/**
 * @brief Clear LSH bucket
 *
 * WHAT: Remove all entries from bucket
 * WHY:  Reset for rebuilding or cleanup
 * HOW:  Set count to 0 (keep allocated memory)
 */
static void lsh_bucket_clear(lsh_bucket_t* bucket)
{
    if (bucket) {
        bucket->count = 0;
    }
}

/**
 * @brief Destroy LSH bucket
 *
 * WHAT: Free bucket memory
 * WHY:  Cleanup on destroy
 */
static void lsh_bucket_destroy(lsh_bucket_t* bucket)
{
    if (bucket && bucket->episode_indices) {
        nimcp_free(bucket->episode_indices);
        bucket->episode_indices = NULL;
        bucket->count = 0;
        bucket->capacity = 0;
    }
}

//=============================================================================
// LSH Table Operations
//=============================================================================

/**
 * @brief Create LSH table
 *
 * WHAT: Allocate LSH table with buckets
 * WHY:  Initialize one table in LSH family
 * HOW:  Allocate bucket array, set random seed
 *
 * COMPLEXITY: O(B) where B = num_buckets
 */
static lsh_table_t* lsh_table_create(uint32_t num_buckets)
{
    lsh_table_t* table = nimcp_calloc(1, sizeof(lsh_table_t));
    if (!table) return NULL;

    // Ensure power of 2 for fast modulo
    uint32_t buckets_pow2 = 1;
    while (buckets_pow2 < num_buckets) {
        buckets_pow2 *= 2;
    }

    table->buckets = nimcp_calloc(buckets_pow2, sizeof(lsh_bucket_t));
    if (!table->buckets) {
        nimcp_free(table);
        return NULL;
    }

    table->num_buckets = buckets_pow2;
    table->hash_seed = (uint64_t)rand() * rand();  // Random seed

    return table;
}

/**
 * @brief Destroy LSH table
 *
 * WHAT: Free all table memory
 * WHY:  Cleanup
 */
static void lsh_table_destroy(lsh_table_t* table)
{
    if (!table) return;

    if (table->buckets) {
        for (uint32_t i = 0; i < table->num_buckets; i++) {
            lsh_bucket_destroy(&table->buckets[i]);
        }
        nimcp_free(table->buckets);
    }

    nimcp_free(table);
}

/**
 * @brief Add episode to LSH table
 *
 * WHAT: Index episode in this table
 * WHY:  Enable LSH-based retrieval
 * HOW:  Hash signature, add to bucket
 *
 * COMPLEXITY: O(1)
 */
static bool lsh_table_add(
    lsh_table_t* table,
    const error_signature_t* sig,
    uint32_t episode_idx)
{
    if (!table || !sig) return false;

    uint64_t hash = lsh_hash_signature(sig, table->hash_seed);
    uint32_t bucket_idx = lsh_bucket_index(hash, table->num_buckets);

    return lsh_bucket_add(&table->buckets[bucket_idx], episode_idx);
}

/**
 * @brief Query LSH table for similar signatures
 *
 * WHAT: Find episodes in same bucket as query
 * WHY:  Approximate nearest neighbor search
 * HOW:  Hash query, return bucket contents
 *
 * COMPLEXITY: O(1) to find bucket, O(K) to copy indices
 */
static void lsh_table_query(
    const lsh_table_t* table,
    const error_signature_t* query,
    uint32_t* indices,
    uint32_t* count,
    uint32_t max_results)
{
    if (!table || !query || !indices || !count) return;

    uint64_t hash = lsh_hash_signature(query, table->hash_seed);
    uint32_t bucket_idx = lsh_bucket_index(hash, table->num_buckets);

    const lsh_bucket_t* bucket = &table->buckets[bucket_idx];

    // Copy indices from bucket (up to max_results)
    uint32_t to_copy = bucket->count < max_results ?
        bucket->count : max_results;

    for (uint32_t i = 0; i < to_copy; i++) {
        indices[*count] = bucket->episode_indices[i];
        (*count)++;
    }
}

/**
 * @brief Clear all buckets in table
 *
 * WHAT: Reset table for rebuilding
 * WHY:  Rebuild index after evictions
 */
static void lsh_table_clear(lsh_table_t* table)
{
    if (!table) return;

    for (uint32_t i = 0; i < table->num_buckets; i++) {
        lsh_bucket_clear(&table->buckets[i]);
    }
}

//=============================================================================
// Configuration
//=============================================================================

episodic_memory_config_t episodic_memory_default_config(void)
{
    episodic_memory_config_t config = {0};

    config.max_episodes = EPISODIC_MEMORY_DEFAULT_CAPACITY;
    config.lsh_num_tables = EPISODIC_MEMORY_DEFAULT_LSH_TABLES;
    config.lsh_num_hashes = EPISODIC_MEMORY_DEFAULT_LSH_HASHES;
    config.enable_emotional_tagging = true;
    config.enable_emotional_eviction = true;
    config.enable_consolidation = true;
    config.consolidation_threshold = EPISODIC_MEMORY_DEFAULT_CONSOLIDATION_THRESHOLD;

    return config;
}

//=============================================================================
// Creation and Destruction
//=============================================================================

episodic_memory_t* episodic_memory_create_default(void)
{
    episodic_memory_config_t config = episodic_memory_default_config();
    return episodic_memory_create_custom(&config);
}

episodic_memory_t* episodic_memory_create_custom(
    const episodic_memory_config_t* config)
{
    // GUARD: NULL config
    if (!config) {
        LOG_ERROR("NULL config in episodic_memory_create_custom");
        return NULL;
    }

    // GUARD: Invalid capacity
    if (config->max_episodes == 0) {
        LOG_ERROR("max_episodes cannot be 0");
        return NULL;
    }

    // Allocate main structure
    episodic_memory_t* memory = nimcp_calloc(1, sizeof(episodic_memory_t));
    if (!memory) {
        LOG_ERROR("Failed to allocate episodic_memory_t");
        return NULL;
    }

    // Copy configuration
    memory->config = *config;
    memory->capacity = config->max_episodes;

    // Allocate episode buffer
    memory->episodes = nimcp_calloc(
        memory->capacity, sizeof(recovery_episode_t));

    if (!memory->episodes) {
        LOG_ERROR("Failed to allocate episode buffer");
        nimcp_free(memory);
        return NULL;
    }

    // Create LSH tables
    memory->num_lsh_tables = config->lsh_num_tables;
    memory->lsh_tables = nimcp_calloc(
        memory->num_lsh_tables, sizeof(lsh_table_t*));

    if (!memory->lsh_tables) {
        LOG_ERROR("Failed to allocate LSH table array");
        nimcp_free(memory->episodes);
        nimcp_free(memory);
        return NULL;
    }

    // Initialize each LSH table
    uint32_t num_buckets = config->lsh_num_hashes;
    for (uint32_t i = 0; i < memory->num_lsh_tables; i++) {
        memory->lsh_tables[i] = lsh_table_create(num_buckets);

        if (!memory->lsh_tables[i]) {
            LOG_ERROR("Failed to create LSH table %u", i);

            // Cleanup already created tables
            for (uint32_t j = 0; j < i; j++) {
                lsh_table_destroy(memory->lsh_tables[j]);
            }

            nimcp_free(memory->lsh_tables);
            nimcp_free(memory->episodes);
            nimcp_free(memory);
            return NULL;
        }
    }

    // Initialize ID counter
    memory->next_episode_id = 1;

    // Initialize stats
    memset(&memory->stats, 0, sizeof(episodic_memory_stats_t));

    LOG_INFO("Created episodic memory: capacity=%u, lsh_tables=%u",
             memory->capacity, memory->num_lsh_tables);

    return memory;
}

void episodic_memory_destroy(episodic_memory_t* memory)
{
    if (!memory) return;

    // Free LSH tables
    if (memory->lsh_tables) {
        for (uint32_t i = 0; i < memory->num_lsh_tables; i++) {
            lsh_table_destroy(memory->lsh_tables[i]);
        }
        nimcp_free(memory->lsh_tables);
    }

    // Free episode buffer
    if (memory->episodes) {
        nimcp_free(memory->episodes);
    }

    // Free main structure
    nimcp_free(memory);
}

//=============================================================================
// Episode Storage
//=============================================================================

/**
 * @brief Find lowest emotion episode for eviction
 *
 * WHAT: Find episode with emotional tag closest to 0
 * WHY:  Preserve emotionally salient memories
 * HOW:  Linear search for min |emotional_tag|
 *
 * COMPLEXITY: O(N)
 */
static uint32_t find_lowest_emotion_index(const episodic_memory_t* memory)
{
    uint32_t min_idx = 0;
    float min_emotion = fabsf(memory->episodes[0].emotional_tag);

    for (uint32_t i = 1; i < memory->count; i++) {
        float emotion = fabsf(memory->episodes[i].emotional_tag);
        if (emotion < min_emotion) {
            min_emotion = emotion;
            min_idx = i;
        }
    }

    return min_idx;
}

/**
 * @brief Evict episode at index
 *
 * WHAT: Remove episode from buffer
 * WHY:  Make room for new episode
 * HOW:  Shift remaining episodes, decrement count
 *
 * COMPLEXITY: O(N)
 */
static void evict_episode_at_index(episodic_memory_t* memory, uint32_t idx)
{
    if (!memory || idx >= memory->count) return;

    // Shift remaining episodes
    if (idx < memory->count - 1) {
        memmove(&memory->episodes[idx],
                &memory->episodes[idx + 1],
                (memory->count - idx - 1) * sizeof(recovery_episode_t));
    }

    memory->count--;

    // Rebuild LSH index (simplified approach - could be optimized)
    // Clear all tables
    for (uint32_t i = 0; i < memory->num_lsh_tables; i++) {
        lsh_table_clear(memory->lsh_tables[i]);
    }

    // Re-index all remaining episodes
    for (uint32_t i = 0; i < memory->count; i++) {
        for (uint32_t t = 0; t < memory->num_lsh_tables; t++) {
            lsh_table_add(memory->lsh_tables[t],
                         &memory->episodes[i].error_sig, i);
        }
    }
}

/**
 * @brief Clamp emotional tag to valid range
 *
 * WHAT: Ensure emotion in [-1.0, +1.0]
 * WHY:  Prevent invalid values
 */
static inline float clamp_emotion(float emotion)
{
    if (emotion < -1.0f) return -1.0f;
    if (emotion > 1.0f) return 1.0f;
    return emotion;
}

bool episodic_memory_store(
    episodic_memory_t* memory,
    const recovery_episode_t* episode)
{
    // GUARD: NULL checks
    if (!memory) {
        LOG_ERROR("NULL memory in episodic_memory_store");
        return false;
    }

    if (!episode) {
        LOG_ERROR("NULL episode in episodic_memory_store");
        return false;
    }

    uint64_t start_time = get_current_timestamp_ms();

    // Make local copy to modify
    recovery_episode_t ep = *episode;

    // Auto-assign ID if not provided
    if (ep.episode_id == 0) {
        ep.episode_id = memory->next_episode_id++;
    }

    // Clamp emotional tag
    ep.emotional_tag = clamp_emotion(ep.emotional_tag);

    // Handle capacity limit
    if (memory->count >= memory->capacity) {
        // Evict based on strategy
        uint32_t evict_idx;

        if (memory->config.enable_emotional_eviction) {
            // Evict lowest-emotion episode
            evict_idx = find_lowest_emotion_index(memory);
        } else {
            // FIFO: evict oldest (index 0)
            evict_idx = 0;
        }

        evict_episode_at_index(memory, evict_idx);
    }

    // Add episode to buffer
    uint32_t insert_idx = memory->count;
    memory->episodes[insert_idx] = ep;
    memory->count++;

    // Index in LSH tables
    for (uint32_t i = 0; i < memory->num_lsh_tables; i++) {
        lsh_table_add(memory->lsh_tables[i], &ep.error_sig, insert_idx);
    }

    // Update statistics
    memory->stats.total_episodes_stored++;
    memory->stats.current_episode_count = memory->count;

    uint64_t end_time = get_current_timestamp_ms();
    memory->stats.total_storage_time_us += (end_time - start_time) * 1000;

    return true;
}

//=============================================================================
// Content-Addressable Retrieval
//=============================================================================

/**
 * @brief Calculate similarity score between signatures
 *
 * WHAT: Compute similarity metric
 * WHY:  Rank retrieval results
 * HOW:  Compare error types and codes
 *
 * COMPLEXITY: O(1)
 *
 * @return Similarity score [0.0, 1.0], higher = more similar
 */
static float calculate_similarity(
    const error_signature_t* sig1,
    const error_signature_t* sig2)
{
    float similarity = 0.0f;

    // Exact type match: +0.5
    if (sig1->error_type == sig2->error_type) {
        similarity += 0.5f;
    }

    // Code proximity: up to +0.5
    // Closer codes = higher similarity
    uint32_t code_diff = sig1->error_code > sig2->error_code ?
        sig1->error_code - sig2->error_code :
        sig2->error_code - sig1->error_code;

    float code_sim = 1.0f / (1.0f + (float)code_diff / 100.0f);
    similarity += 0.5f * code_sim;

    return similarity;
}

/**
 * @brief Comparison function for sorting by similarity
 */
typedef struct {
    uint32_t index;
    float similarity;
} episode_score_t;

static int compare_scores(const void* a, const void* b)
{
    const episode_score_t* sa = (const episode_score_t*)a;
    const episode_score_t* sb = (const episode_score_t*)b;

    // Sort descending (highest similarity first)
    if (sa->similarity > sb->similarity) return -1;
    if (sa->similarity < sb->similarity) return 1;
    return 0;
}

recovery_episode_t** episodic_memory_recall_similar(
    episodic_memory_t* memory,
    const error_signature_t* query,
    uint32_t max_results,
    uint32_t* count)
{
    // GUARD: NULL checks
    if (!memory || !query || !count) {
        LOG_ERROR("NULL parameter in episodic_memory_recall_similar");
        return NULL;
    }

    *count = 0;

    // GUARD: Empty memory
    if (memory->count == 0) {
        return NULL;
    }

    uint64_t start_time = get_current_timestamp_ms();

    // Collect candidate indices from all LSH tables
    uint32_t* candidates = nimcp_malloc(
        memory->count * sizeof(uint32_t));

    if (!candidates) {
        LOG_ERROR("Failed to allocate candidate array");
        return NULL;
    }

    uint32_t candidate_count = 0;

    for (uint32_t i = 0; i < memory->num_lsh_tables; i++) {
        lsh_table_query(memory->lsh_tables[i], query,
                       candidates, &candidate_count,
                       memory->count - candidate_count);
    }

    // Remove duplicates by marking (simple approach)
    bool* seen = nimcp_calloc(memory->count, sizeof(bool));
    if (!seen) {
        nimcp_free(candidates);
        return NULL;
    }

    uint32_t unique_count = 0;
    for (uint32_t i = 0; i < candidate_count; i++) {
        uint32_t idx = candidates[i];
        if (idx < memory->count && !seen[idx]) {
            seen[idx] = true;
            candidates[unique_count++] = idx;
        }
    }

    nimcp_free(seen);

    if (unique_count == 0) {
        nimcp_free(candidates);
        return NULL;
    }

    // Score and sort candidates
    episode_score_t* scores = nimcp_malloc(
        unique_count * sizeof(episode_score_t));

    if (!scores) {
        nimcp_free(candidates);
        return NULL;
    }

    for (uint32_t i = 0; i < unique_count; i++) {
        uint32_t idx = candidates[i];
        scores[i].index = idx;
        scores[i].similarity = calculate_similarity(
            &memory->episodes[idx].error_sig, query);
    }

    // Sort by similarity (descending)
    qsort(scores, unique_count, sizeof(episode_score_t), compare_scores);

    // Take top-K results
    uint32_t result_count = unique_count < max_results ?
        unique_count : max_results;

    recovery_episode_t** results = nimcp_malloc(
        result_count * sizeof(recovery_episode_t*));

    if (!results) {
        nimcp_free(scores);
        nimcp_free(candidates);
        return NULL;
    }

    for (uint32_t i = 0; i < result_count; i++) {
        results[i] = &memory->episodes[scores[i].index];
    }

    *count = result_count;

    // Update statistics
    memory->stats.total_queries++;
    uint64_t end_time = get_current_timestamp_ms();
    memory->stats.total_query_time_us += (end_time - start_time) * 1000;

    nimcp_free(scores);
    nimcp_free(candidates);

    return results;
}

//=============================================================================
// Episode Replay
//=============================================================================

replay_result_t episodic_memory_replay(
    episodic_memory_t* memory,
    uint64_t episode_id)
{
    replay_result_t result = {0};

    // GUARD: NULL memory
    if (!memory) {
        LOG_ERROR("NULL memory in episodic_memory_replay");
        return result;
    }

    // Find episode by ID
    for (uint32_t i = 0; i < memory->count; i++) {
        if (memory->episodes[i].episode_id == episode_id) {
            // Found it - increment replay count
            memory->episodes[i].replay_count++;

            result.success = true;
            result.episode_id = episode_id;

            LOG_DEBUG("Replayed episode %lu (count=%u)",
                     episode_id, memory->episodes[i].replay_count);

            return result;
        }
    }

    // Not found
    LOG_WARNING("Episode %lu not found for replay", episode_id);
    return result;
}

//=============================================================================
// Consolidation
//=============================================================================

consolidation_result_t episodic_memory_consolidate(
    episodic_memory_t* memory)
{
    consolidation_result_t result = {0};

    // GUARD: NULL memory
    if (!memory) {
        LOG_ERROR("NULL memory in episodic_memory_consolidate");
        return result;
    }

    // GUARD: Consolidation disabled
    if (!memory->config.enable_consolidation) {
        LOG_DEBUG("Consolidation disabled");
        return result;
    }

    // GUARD: Insufficient data
    if (memory->count < memory->config.consolidation_threshold) {
        LOG_DEBUG("Insufficient episodes for consolidation (%u < %u)",
                 memory->count, memory->config.consolidation_threshold);
        return result;
    }

    // Group episodes by error signature
    // Simplified approach: exact error_type match
    uint32_t patterns_found = 0;

    for (error_type_t err_type = 0; err_type < ERROR_TYPE_COUNT; err_type++) {
        // Count episodes of this type
        uint32_t type_count = 0;
        for (uint32_t i = 0; i < memory->count; i++) {
            if (memory->episodes[i].error_sig.error_type == err_type) {
                type_count++;
            }
        }

        // If enough episodes, analyze pattern
        if (type_count >= memory->config.consolidation_threshold) {
            // Count success rate per strategy
            uint32_t strategy_counts[STRATEGY_COUNT] = {0};
            uint32_t strategy_success[STRATEGY_COUNT] = {0};

            for (uint32_t i = 0; i < memory->count; i++) {
                if (memory->episodes[i].error_sig.error_type == err_type) {
                    recovery_strategy_type_t strat =
                        memory->episodes[i].strategy_type;

                    if (strat < STRATEGY_COUNT) {
                        strategy_counts[strat]++;
                        if (memory->episodes[i].success) {
                            strategy_success[strat]++;
                        }

                        // Mark as consolidated
                        memory->episodes[i].consolidated = true;
                    }
                }
            }

            // Find best strategy (highest success rate with >= 5 samples)
            for (recovery_strategy_type_t s = 0; s < STRATEGY_COUNT; s++) {
                if (strategy_counts[s] >= 5) {
                    float success_rate =
                        (float)strategy_success[s] / strategy_counts[s];

                    if (success_rate >= 0.7f) {
                        // Reliable pattern found
                        patterns_found++;

                        LOG_INFO("Pattern: error_type=%d -> strategy=%d "
                                "(success=%.1f%%, N=%u)",
                                err_type, s, success_rate * 100.0f,
                                strategy_counts[s]);
                    }
                }
            }
        }
    }

    result.success = patterns_found > 0;
    result.patterns_extracted = patterns_found;
    result.confidence = patterns_found > 0 ? 0.8f : 0.0f;

    memory->stats.consolidation_runs++;
    memory->stats.patterns_extracted += patterns_found;

    LOG_INFO("Consolidation complete: %u patterns extracted", patterns_found);

    return result;
}

//=============================================================================
// Getters and Statistics
//=============================================================================

uint32_t episodic_memory_get_count(const episodic_memory_t* memory)
{
    return memory ? memory->count : 0;
}

uint32_t episodic_memory_get_capacity(const episodic_memory_t* memory)
{
    return memory ? memory->capacity : 0;
}

recovery_episode_t** episodic_memory_get_all(
    episodic_memory_t* memory,
    uint32_t* count)
{
    if (!memory || !count) {
        LOG_ERROR("NULL parameter in episodic_memory_get_all");
        return NULL;
    }

    *count = memory->count;

    if (memory->count == 0) {
        return NULL;
    }

    // Allocate array of pointers
    recovery_episode_t** results = nimcp_malloc(
        memory->count * sizeof(recovery_episode_t*));

    if (!results) {
        LOG_ERROR("Failed to allocate results array");
        *count = 0;
        return NULL;
    }

    // Fill with pointers to episodes
    for (uint32_t i = 0; i < memory->count; i++) {
        results[i] = &memory->episodes[i];
    }

    return results;
}

episodic_memory_stats_t episodic_memory_get_stats(
    const episodic_memory_t* memory)
{
    episodic_memory_stats_t stats = {0};

    if (memory) {
        stats = memory->stats;
    }

    return stats;
}

//=============================================================================
// Utility Functions
//=============================================================================

uint64_t compute_error_signature_hash(
    error_type_t error_type,
    uint32_t error_code)
{
    // Combine type and code into hash
    uint64_t hash = ((uint64_t)error_type << 32) | error_code;

    // Mix bits
    hash ^= (hash >> 33);
    hash *= 0xff51afd7ed558ccdULL;
    hash ^= (hash >> 33);
    hash *= 0xc4ceb9fe1a85ec53ULL;
    hash ^= (hash >> 33);

    return hash;
}

uint64_t get_current_timestamp_ms(void)
{
    struct timespec ts;

    #ifdef __linux__
    clock_gettime(CLOCK_MONOTONIC, &ts);
    #else
    clock_gettime(CLOCK_REALTIME, &ts);
    #endif

    return (uint64_t)ts.tv_sec * 1000ULL +
           (uint64_t)ts.tv_nsec / 1000000ULL;
}
