/**
 * @file nimcp_emotion_consolidation.h
 * @brief Emotion-Modulated Memory Consolidation System
 *
 * WHAT: Integrates emotion tensor with memory consolidation
 * WHY:  Emotional memories consolidate faster and stronger than neutral ones
 * HOW:  Subscribe to emotion updates, scale consolidation by emotional arousal
 *
 * THEORETICAL FOUNDATIONS:
 * - McGaugh (2000): Amygdala modulates memory consolidation strength
 * - LaBar & Cabeza (2006): Emotional enhancement of memory
 * - Dolcos et al. (2004): Emotional arousal enhances encoding and consolidation
 * - Cahill & McGaugh (1998): Beta-adrenergic activation strengthens consolidation
 *
 * BIOLOGICAL BASIS:
 * - Amygdala activation during encoding predicts later memory strength
 * - Emotional arousal triggers norepinephrine/cortisol release
 * - These neuromodulators enhance hippocampal consolidation
 * - Emotionally salient memories receive preferential overnight consolidation
 *
 * INTEGRATION:
 * - Subscribes to BIO_MSG_EMOTION_TENSOR_UPDATE via bio-async
 * - Tags consolidating memories with emotion tensor state at encoding
 * - Scales consolidation strength by emotional arousal/intensity
 * - Prioritizes emotionally-tagged memories during replay
 *
 * CODING STANDARDS:
 * - WHAT-WHY-HOW documentation
 * - Functions < 50 lines
 * - Guard clauses first
 * - No stubs - real implementation
 * - 100% test coverage
 *
 * @author NIMCP Development Team
 * @date 2025-12-09
 * @version 1.0.0
 */

#ifndef NIMCP_EMOTION_CONSOLIDATION_H
#define NIMCP_EMOTION_CONSOLIDATION_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/nimcp_emotion_tensor.h"
#include "cognitive/consolidation/nimcp_consolidation.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct emotion_consolidation_system emotion_consolidation_system_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Configuration for emotion-modulated consolidation
 *
 * WHAT: Parameters controlling emotion-memory integration
 * WHY:  Tune how emotions affect consolidation processes
 * HOW:  Scaling factors and thresholds for modulation
 */
typedef struct {
    float arousal_consolidation_boost; /**< Boost consolidation by arousal [1.0-3.0] */
    float valence_asymmetry;           /**< Negative valence boost factor [0.8-1.2] */
    float min_emotion_threshold;       /**< Min arousal for emotion effect [0.1-0.5] */
    float max_consolidation_boost;     /**< Maximum boost multiplier [1.0-5.0] */
    bool enable_emotion_tagging;       /**< Tag memories with emotion state */
    bool prioritize_emotional;         /**< Prioritize emotional memories */
    float decay_inhibition_factor;     /**< Slow decay for emotional memories [0.5-1.0] */
} emotion_consolidation_config_t;

/**
 * @brief Emotion tag for memory items
 *
 * WHAT: Snapshot of emotion state at memory encoding
 * WHY:  Track which memories are emotionally significant
 * HOW:  Store tensor state when memory was formed
 */
typedef struct {
    float arousal;                          /**< Arousal at encoding [0.0-1.0] */
    float valence;                          /**< Valence at encoding [-1.0-1.0] */
    emotion_primary_t dominant_emotion;     /**< Primary emotion during encoding */
    float emotion_intensity;                /**< Overall emotional intensity [0.0-1.0] */
    uint64_t encoding_timestamp_ms;         /**< When memory was encoded */
    bool is_emotionally_tagged;             /**< Has significant emotion */
} memory_emotion_tag_t;

/**
 * @brief Statistics for emotion-modulated consolidation
 *
 * WHAT: Metrics tracking emotion-memory interactions
 * WHY:  Monitor system behavior and effectiveness
 * HOW:  Counters and aggregates updated during consolidation
 */
typedef struct {
    uint64_t emotion_updates_received;      /**< Total emotion tensor updates */
    uint64_t emotional_memories_tagged;     /**< Memories tagged with emotions */
    uint64_t emotional_boosts_applied;      /**< Times consolidation was boosted */
    float avg_emotional_boost;              /**< Average boost factor */
    float avg_emotional_arousal;            /**< Average arousal during consolidation */
    uint64_t high_arousal_consolidations;   /**< Consolidations during high arousal */
    uint64_t low_arousal_consolidations;    /**< Consolidations during low arousal */
    float current_arousal;                  /**< Current arousal level */
    float current_consolidation_boost;      /**< Current boost multiplier */
} emotion_consolidation_stats_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create emotion-modulated consolidation system
 *
 * WHAT: Initialize emotion-consolidation integration
 * WHY:  Enable emotional modulation of memory consolidation
 * HOW:  Allocate system, register bio-async callbacks, set defaults
 *
 * @param emotion_tensor Emotion tensor system to subscribe to
 * @param consolidation_handle Consolidation system to modulate (can be NULL)
 * @param config Configuration (NULL = defaults)
 * @return System handle or NULL on error
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe
 */
emotion_consolidation_system_t* emotion_consolidation_create(
    emotion_tensor_system_t* emotion_tensor,
    consolidation_handle_t consolidation_handle,
    const emotion_consolidation_config_t* config
);

/**
 * @brief Get default configuration
 *
 * WHAT: Return sensible defaults
 * WHY:  Convenient initialization
 * HOW:  Return pre-filled config struct
 *
 * @return Default configuration
 */
emotion_consolidation_config_t emotion_consolidation_default_config(void);

/**
 * @brief Destroy emotion-consolidation system
 *
 * WHAT: Free all resources
 * WHY:  Prevent memory leaks
 * HOW:  Unregister callbacks, free system
 *
 * @param system System handle
 */
void emotion_consolidation_destroy(emotion_consolidation_system_t* system);

//=============================================================================
// Memory Tagging API
//=============================================================================

/**
 * @brief Tag memory with current emotion state
 *
 * WHAT: Snapshot emotion tensor state for memory
 * WHY:  Track which memories are emotionally significant
 * HOW:  Query emotion tensor, create tag
 *
 * @param system Emotion-consolidation system
 * @param tag Output emotion tag
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe
 */
bool emotion_consolidation_tag_memory(
    emotion_consolidation_system_t* system,
    memory_emotion_tag_t* tag
);

/**
 * @brief Compute consolidation strength for memory
 *
 * WHAT: Calculate how strongly to consolidate this memory
 * WHY:  Emotional memories consolidate more than neutral ones
 * HOW:  Apply arousal-based boost to base strength
 *
 * MODULATION:
 * - High arousal → stronger consolidation (up to 3x)
 * - Negative valence → slight boost (negativity bias)
 * - Low arousal → normal consolidation
 *
 * @param system Emotion-consolidation system
 * @param base_strength Base consolidation strength [0.0-1.0]
 * @param emotion_tag Emotion tag for this memory
 * @return Modulated consolidation strength [0.0-1.0]
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe
 */
float emotion_consolidation_compute_strength(
    emotion_consolidation_system_t* system,
    float base_strength,
    const memory_emotion_tag_t* emotion_tag
);

/**
 * @brief Check if memory should be prioritized
 *
 * WHAT: Determine if memory is emotionally significant
 * WHY:  Prioritize emotional memories during consolidation
 * HOW:  Check arousal/intensity thresholds
 *
 * @param system Emotion-consolidation system
 * @param emotion_tag Emotion tag for memory
 * @return true if memory should be prioritized
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe
 */
bool emotion_consolidation_should_prioritize(
    emotion_consolidation_system_t* system,
    const memory_emotion_tag_t* emotion_tag
);

//=============================================================================
// Consolidation Modulation API
//=============================================================================

/**
 * @brief Get current consolidation boost factor
 *
 * WHAT: Query how much emotion is boosting consolidation
 * WHY:  Monitor emotional modulation strength
 * HOW:  Return current boost multiplier [1.0-max]
 *
 * @param system Emotion-consolidation system
 * @return Boost factor [1.0-max] or 1.0 on error
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe
 */
float emotion_consolidation_get_boost(const emotion_consolidation_system_t* system);

/**
 * @brief Apply emotion-modulated decay inhibition
 *
 * WHAT: Slow memory decay for emotional memories
 * WHY:  Emotional memories persist longer
 * HOW:  Reduce decay rate based on emotion intensity
 *
 * @param system Emotion-consolidation system
 * @param base_decay Base decay rate [0.0-1.0]
 * @param emotion_tag Emotion tag for memory
 * @return Modulated decay rate [0.0-1.0]
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe
 */
float emotion_consolidation_modulate_decay(
    emotion_consolidation_system_t* system,
    float base_decay,
    const memory_emotion_tag_t* emotion_tag
);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get emotion-consolidation statistics
 *
 * WHAT: Query system statistics
 * WHY:  Monitor performance and behavior
 * HOW:  Copy internal stats to output
 *
 * @param system Emotion-consolidation system
 * @param stats Output statistics
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe
 */
bool emotion_consolidation_get_stats(
    const emotion_consolidation_system_t* system,
    emotion_consolidation_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * WHAT: Clear all counters and accumulators
 * WHY:  Start fresh measurement period
 * HOW:  Zero stats structure
 *
 * @param system Emotion-consolidation system
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe
 */
void emotion_consolidation_reset_stats(emotion_consolidation_system_t* system);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Register with bio-async system
 *
 * WHAT: Subscribe to emotion tensor updates
 * WHY:  Receive real-time emotion state changes
 * HOW:  Register callback for BIO_MSG_EMOTION_TENSOR_UPDATE
 *
 * @param system Emotion-consolidation system
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe
 */
bool emotion_consolidation_register_bio_async(emotion_consolidation_system_t* system);

/**
 * @brief Unregister from bio-async system
 *
 * WHAT: Unsubscribe from emotion updates
 * WHY:  Clean shutdown
 * HOW:  Remove callback registration
 *
 * @param system Emotion-consolidation system
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe
 */
void emotion_consolidation_unregister_bio_async(emotion_consolidation_system_t* system);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMOTION_CONSOLIDATION_H */
