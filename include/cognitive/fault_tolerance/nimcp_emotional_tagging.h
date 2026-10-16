/**
 * @file nimcp_emotional_tagging.h
 * @brief Emotional Tagging for Critical Failures
 *
 * WHAT: Mark severe failures with emotional tags for priority handling and learning
 * WHY:  Not all failures are equal - critical ones need emphasis and stronger memory
 * HOW:  Compute emotional valence/arousal, boost memory strength, prioritize by emotion
 *
 * BIOLOGICAL BASIS:
 * - Amygdala tags emotionally salient events for enhanced memory formation
 * - Emotional arousal modulates hippocampal consolidation (stronger memories)
 * - Negative valence (fear) prioritizes threat-related learning
 * - Positive valence (relief) reinforces successful coping strategies
 *
 * EMOTIONAL DIMENSIONS:
 * - Valence: Negative (-1.0) to Positive (+1.0) - "good" vs "bad"
 * - Arousal: Low (0.0) to High (1.0) - importance/intensity
 * - Fear: Data loss risk, catastrophic failure
 * - Relief: Successful recovery, crisis averted
 * - Frustration: Repeated failures, inability to resolve
 *
 * INTEGRATION POINTS:
 * - Episodic memory: Tagged episodes stored with emotional strength
 * - Recovery planning: High-emotion failures prioritized for learning
 * - Attention system: Emotional salience guides focus
 *
 * @author NIMCP Development Team
 * @date 2025-01-20
 * @version 2.7.0 Phase 10.8
 */

#ifndef NIMCP_EMOTIONAL_TAGGING_H
#define NIMCP_EMOTIONAL_TAGGING_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Recovery episode for emotional tagging
 *
 * WHAT: Describes a fault recovery event with contextual information
 * WHY:  Provides data for computing appropriate emotional response
 */
typedef struct {
    char error_type[64];          /**< Error type (SIGSEGV, TIMEOUT, etc.) */
    bool success;                 /**< Whether recovery succeeded */
    float data_loss_risk;         /**< Data loss risk [0.0, 1.0] */
    uint64_t recovery_time_us;    /**< Recovery duration in microseconds */
    uint32_t retry_count;         /**< Number of retry attempts */
    uint64_t timestamp_us;        /**< When episode occurred */
} nimcp_recovery_episode_t;

/**
 * @brief Emotional tag structure
 *
 * WHAT: Emotional characterization of a failure/recovery event
 * WHY:  Enables emotion-based memory prioritization and learning
 *
 * RANGES:
 * - valence: [-1.0, 1.0] - negative to positive
 * - arousal: [0.0, 1.0] - calm to highly aroused
 * - fear, relief, frustration: [0.0, 1.0] - none to maximum
 */
typedef struct {
    float valence;        /**< Emotional valence: -1.0 (bad) to +1.0 (good) */
    float arousal;        /**< Arousal level: 0.0 (calm) to 1.0 (intense) */

    /* Specific emotions */
    float fear;           /**< Fear of data loss, system failure */
    float relief;         /**< Relief from successful recovery */
    float frustration;    /**< Frustration from repeated failures */
} nimcp_emotional_tag_t;

/**
 * @brief Emotional tagging statistics
 *
 * WHAT: Tracks emotional tagging over time
 * WHY:  Monitor emotional patterns and system stress
 */
typedef struct {
    uint64_t total_tags;          /**< Total number of tags computed */
    float avg_valence;            /**< Average valence over all tags */
    float avg_arousal;            /**< Average arousal over all tags */
    uint64_t high_fear_count;     /**< Count of high-fear events */
    uint64_t high_relief_count;   /**< Count of high-relief events */
    uint64_t high_frustration_count; /**< Count of high-frustration events */
} nimcp_emotional_tagger_stats_t;

/**
 * @brief Emotional tagger instance (opaque)
 */
typedef struct nimcp_emotional_tagger nimcp_emotional_tagger_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create emotional tagger instance
 *
 * WHAT: Allocates and initializes emotional tagging system
 * WHY:  Required before computing emotional tags
 * HOW:  Allocates memory, initializes statistics
 *
 * COMPLEXITY: O(1)
 * MEMORY: ~128 bytes
 *
 * @return Tagger instance, NULL on failure
 */
nimcp_emotional_tagger_t* nimcp_emotional_tagger_create(void);

/**
 * @brief Destroy emotional tagger instance
 *
 * WHAT: Frees all resources associated with tagger
 * WHY:  Prevents memory leaks
 * HOW:  Validates pointer, frees memory
 *
 * COMPLEXITY: O(1)
 * MEMORY: Frees ~128 bytes
 *
 * @param tagger Tagger instance to destroy (NULL safe)
 */
void nimcp_emotional_tagger_destroy(nimcp_emotional_tagger_t* tagger);

/**
 * @brief Reset tagger statistics
 *
 * WHAT: Clears all accumulated statistics
 * WHY:  Start fresh statistics collection
 * HOW:  Zeros all counters, keeps tagger functional
 *
 * COMPLEXITY: O(1)
 *
 * @param tagger Tagger instance
 * @return true on success, false if tagger is NULL
 */
bool nimcp_emotional_tagger_reset(nimcp_emotional_tagger_t* tagger);

//=============================================================================
// Core Emotional Tagging Functions
//=============================================================================

/**
 * @brief Compute emotional tag for recovery episode
 *
 * WHAT: Analyzes episode and computes appropriate emotional response
 * WHY:  Tag failures with emotions for priority learning and memory
 * HOW:  Evaluates error severity, data loss risk, recovery success/time, retries
 *
 * ALGORITHM:
 * 1. Compute valence from success/failure and severity
 * 2. Compute arousal from data loss risk and error type
 * 3. Compute fear from data loss risk and failure type
 * 4. Compute relief from success and fast recovery
 * 5. Compute frustration from retry count
 * 6. Clamp all values to valid ranges
 *
 * COMPLEXITY: O(1)
 * MEMORY: O(1)
 *
 * EMOTIONAL RULES:
 * - SIGSEGV + high data loss → high fear, negative valence, high arousal
 * - Fast successful recovery → high relief, positive valence, moderate arousal
 * - Many retries → high frustration, negative valence
 *
 * @param tagger Tagger instance (non-NULL)
 * @param episode Recovery episode to tag (non-NULL)
 * @param output Output emotional tag (non-NULL)
 * @return true on success, false on invalid parameters
 */
bool nimcp_emotional_tagger_compute_tag(
    nimcp_emotional_tagger_t* tagger,
    const nimcp_recovery_episode_t* episode,
    nimcp_emotional_tag_t* output
);

/**
 * @brief Compute memory strength boost from emotion
 *
 * WHAT: Calculates memory consolidation multiplier based on emotional intensity
 * WHY:  Emotionally salient events form stronger memories (biological principle)
 * HOW:  Combines arousal and valence magnitude to determine boost
 *
 * ALGORITHM:
 * - High arousal + extreme valence → 2.0x boost (very strong memory)
 * - Moderate arousal → 1.5x boost (stronger memory)
 * - Low arousal → 1.0x-1.2x boost (normal to slightly stronger)
 *
 * BIOLOGICAL BASIS:
 * - Amygdala activation during arousal enhances hippocampal consolidation
 * - Stress hormones (cortisol) modulate memory formation
 * - Emotional events remembered better than neutral ones
 *
 * COMPLEXITY: O(1)
 * MEMORY: O(1)
 *
 * @param emotion Emotional tag (NULL returns 1.0)
 * @return Memory strength multiplier [1.0, 2.5]
 */
float nimcp_emotional_memory_boost(const nimcp_emotional_tag_t* emotion);

/**
 * @brief Compute priority from emotional tag
 *
 * WHAT: Calculates processing priority based on emotional salience
 * WHY:  High-emotion events should be prioritized for learning and attention
 * HOW:  Combines arousal and specific emotions (fear boosts priority)
 *
 * ALGORITHM:
 * - Base priority = arousal
 * - Fear boost: Add 0.2 * fear (threat detection priority)
 * - Clamp to [0.0, 1.0]
 *
 * BIOLOGICAL BASIS:
 * - Amygdala threat detection prioritizes fearful stimuli
 * - Arousal gates attention and working memory access
 * - Emotional salience determines learning rate
 *
 * COMPLEXITY: O(1)
 * MEMORY: O(1)
 *
 * @param emotion Emotional tag (NULL returns 0.0)
 * @return Priority value [0.0, 1.0]
 */
float nimcp_emotional_priority(const nimcp_emotional_tag_t* emotion);

//=============================================================================
// Statistics and Monitoring
//=============================================================================

/**
 * @brief Get emotional tagging statistics
 *
 * WHAT: Returns accumulated statistics about emotional tagging
 * WHY:  Monitor emotional patterns and system stress levels
 * HOW:  Thread-safe copy of statistics structure
 *
 * COMPLEXITY: O(1)
 *
 * @param tagger Tagger instance
 * @param stats Output statistics structure
 * @return true on success, false on invalid parameters
 */
bool nimcp_emotional_tagger_get_stats(
    const nimcp_emotional_tagger_t* tagger,
    nimcp_emotional_tagger_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMOTIONAL_TAGGING_H */
