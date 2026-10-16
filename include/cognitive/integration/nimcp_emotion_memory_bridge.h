/**
 * @file nimcp_emotion_memory_bridge.h
 * @brief Emotion-Memory Integration Bridge
 * @version 1.0.0
 * @date 2025-12-31
 *
 * WHAT: Bidirectional integration between emotion and memory systems
 * WHY:  Emotional experiences enhance memory consolidation and retrieval.
 *       Memory retrieval can trigger emotional responses.
 * HOW:  Emotions tag memories with valence/arousal; retrieval recreates
 *       emotional context; emotional intensity modulates consolidation strength.
 *
 * BIOLOGICAL BASIS:
 * - Amygdala-hippocampus interactions enhance emotional memory encoding
 * - Emotional arousal releases norepinephrine, strengthening consolidation
 * - Memory retrieval reactivates associated emotional circuits
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_EMOTION_MEMORY_BRIDGE_H
#define NIMCP_EMOTION_MEMORY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define EMOTION_MEMORY_MAX_MEMORIES       1024
#define EMOTION_MEMORY_VALENCE_MIN       -1.0f
#define EMOTION_MEMORY_VALENCE_MAX        1.0f
#define EMOTION_MEMORY_AROUSAL_MIN        0.0f
#define EMOTION_MEMORY_AROUSAL_MAX        1.0f

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct emotion_memory_bridge emotion_memory_bridge_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Emotional tag for a memory
 */
typedef struct {
    float valence;                       /**< Positive/negative [-1, 1] */
    float arousal;                       /**< Activation level [0, 1] */
    uint64_t timestamp;                  /**< When tag was applied */
} emotion_memory_tag_t;

/**
 * @brief Emotion output from memory retrieval
 */
typedef struct {
    float valence;                       /**< Retrieved emotional valence */
    float arousal;                       /**< Retrieved emotional arousal */
    float intensity;                     /**< Overall emotional intensity */
    bool has_emotion;                    /**< Whether memory has emotional tag */
} emotion_memory_emotion_out_t;

/**
 * @brief Configuration for Emotion-Memory bridge
 */
typedef struct {
    float emotional_weight_factor;       /**< Weight of emotion in memory ops */
    float consolidation_threshold;       /**< Min intensity for consolidation boost */
    float valence_sensitivity;           /**< Sensitivity to valence differences */
} emotion_memory_config_t;

/**
 * @brief Statistics for Emotion-Memory bridge
 */
typedef struct {
    uint64_t memories_tagged;            /**< Total memories tagged with emotion */
    uint64_t retrievals_with_emotion;    /**< Retrievals that triggered emotion */
    uint64_t consolidation_boosts;       /**< Times consolidation was boosted */
    float avg_valence;                   /**< Average valence of tagged memories */
    float avg_arousal;                   /**< Average arousal of tagged memories */
} emotion_memory_stats_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default Emotion-Memory configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible defaults
 * HOW:  Set standard thresholds and weights
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int emotion_memory_bridge_default_config(emotion_memory_config_t* config);

/**
 * @brief Create Emotion-Memory bridge
 *
 * WHAT: Initialize Emotion-Memory integration bridge
 * WHY:  Enable bidirectional emotion-memory interaction
 * HOW:  Allocate bridge, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
emotion_memory_bridge_t* emotion_memory_bridge_create(
    const emotion_memory_config_t* config
);

/**
 * @brief Destroy Emotion-Memory bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free memory, clear state
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void emotion_memory_bridge_destroy(emotion_memory_bridge_t* bridge);

/* ============================================================================
 * Emotion -> Memory Direction
 * ============================================================================ */

/**
 * @brief Tag a memory with emotional valence and arousal
 *
 * WHAT: Associate emotional state with a memory
 * WHY:  Emotional tagging enhances encoding and future retrieval
 * HOW:  Store valence/arousal association with memory_id
 *
 * @param bridge Emotion-Memory bridge
 * @param memory_id ID of memory to tag
 * @param valence Emotional valence [-1, 1]
 * @param arousal Emotional arousal [0, 1]
 * @return 0 on success, -1 on error
 */
int emotion_memory_tag_memory(
    emotion_memory_bridge_t* bridge,
    uint64_t memory_id,
    float valence,
    float arousal
);

/**
 * @brief Modulate memory consolidation based on emotional intensity
 *
 * WHAT: Boost consolidation strength for emotionally intense memories
 * WHY:  Emotional arousal enhances long-term memory formation
 * HOW:  Scale consolidation weight by emotional intensity
 *
 * @param bridge Emotion-Memory bridge
 * @param memory_id ID of memory being consolidated
 * @param emotional_intensity Intensity of associated emotion [0, 1]
 * @return 0 on success, -1 on error
 */
int emotion_memory_modulate_consolidation(
    emotion_memory_bridge_t* bridge,
    uint64_t memory_id,
    float emotional_intensity
);

/* ============================================================================
 * Memory -> Emotion Direction
 * ============================================================================ */

/**
 * @brief Trigger emotional response on memory retrieval
 *
 * WHAT: Retrieve emotional state associated with a memory
 * WHY:  Memory retrieval reactivates emotional context
 * HOW:  Look up emotional tag, output emotion state
 *
 * @param bridge Emotion-Memory bridge
 * @param memory_id ID of retrieved memory
 * @param emotion_out Output emotional state
 * @return 0 on success, -1 on error
 */
int emotion_memory_on_retrieval(
    emotion_memory_bridge_t* bridge,
    uint64_t memory_id,
    emotion_memory_emotion_out_t* emotion_out
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get memories within a valence range
 *
 * WHAT: Query memories by emotional valence
 * WHY:  Enable emotion-based memory search
 * HOW:  Filter memories by valence threshold
 *
 * @param bridge Emotion-Memory bridge
 * @param valence_min Minimum valence threshold
 * @param valence_max Maximum valence threshold
 * @param memory_ids Output array of matching memory IDs
 * @param max_count Maximum number of results
 * @return Number of memories found, -1 on error
 */
int emotion_memory_get_emotional_memories(
    emotion_memory_bridge_t* bridge,
    float valence_min,
    float valence_max,
    uint64_t memory_ids[],
    size_t max_count
);

/* ============================================================================
 * Stats API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Emotion-Memory bridge
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int emotion_memory_bridge_get_stats(
    const emotion_memory_bridge_t* bridge,
    emotion_memory_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMOTION_MEMORY_BRIDGE_H */
