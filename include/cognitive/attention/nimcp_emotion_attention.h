/**
 * @file nimcp_emotion_attention.h
 * @brief Emotion-Modulated Attention System
 *
 * WHAT: Integrates emotion tensor with attention to modulate attentional focus
 * WHY:  Emotions profoundly affect attention (fear narrows, joy broadens)
 * HOW:  Subscribe to emotion tensor updates, apply emotional biases to attention
 *
 * THEORETICAL FOUNDATIONS:
 * - Fredrickson (2001): Broaden-and-Build theory of positive emotions
 * - Easterbrook (1959): Arousal narrows attention (inverted-U)
 * - Pessoa (2008): Emotion-attention integration in the brain
 * - LeDoux (1996): Amygdala modulates attentional resources
 *
 * BIOLOGICAL BASIS:
 * - High arousal emotions (fear, anger) narrow attention to threat-relevant stimuli
 * - Positive emotions (joy) broaden attentional scope
 * - Emotional salience boosts attention to emotionally-tagged stimuli
 * - Amygdala gates sensory processing based on emotional significance
 *
 * INTEGRATION:
 * - Subscribes to BIO_MSG_EMOTION_TENSOR_UPDATE via bio-async
 * - Queries emotion tensor state to modulate attention parameters
 * - Applies arousal-based narrowing/broadening of attention
 * - Boosts salience for emotion-congruent stimuli
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

#ifndef NIMCP_EMOTION_ATTENTION_H
#define NIMCP_EMOTION_ATTENTION_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/nimcp_emotion_tensor.h"
#include "plasticity/attention/nimcp_attention.h"
#include "utils/encoding/nimcp_positional_encoding.h"
#include "cognitive/nimcp_sleep_wake.h"  // Sleep state integration

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct emotion_attention_system emotion_attention_system_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Configuration for emotion-modulated attention
 *
 * WHAT: Parameters controlling emotion-attention integration
 * WHY:  Tune how emotions affect attentional processes
 * HOW:  Scaling factors and thresholds for modulation
 */
typedef struct {
    float arousal_narrowing_factor;  /**< How much arousal narrows attention [0.0-1.0] */
    float valence_broadening_factor; /**< How much positive valence broadens [0.0-1.0] */
    float emotion_salience_boost;    /**< Boost salience for emotional stimuli [1.0-3.0] */
    float min_attention_width;       /**< Minimum attention width (narrowed) [0.1-0.5] */
    float max_attention_width;       /**< Maximum attention width (broadened) [0.5-1.0] */
    bool enable_emotion_gating;      /**< Enable emotional gating of attention */
    bool enable_congruency_bias;     /**< Bias attention to emotion-congruent stimuli */

    /* Positional Encoding Configuration */
    bool enable_temporal_encoding;   /**< Enable PE for temporal emotion sequences */
    bool enable_priority_encoding;   /**< Enable PE for emotion priority ordering */
    nimcp_pos_encoding_type_t temporal_pe_type;  /**< PE type for temporal sequences */
    nimcp_pos_encoding_type_t priority_pe_type;  /**< PE type for priority ordering */
    uint32_t max_temporal_sequence;  /**< Maximum temporal sequence length */
    uint32_t emotion_embedding_dim;  /**< Dimension of emotion embeddings */
} emotion_attention_config_t;

/**
 * @brief Statistics for emotion-modulated attention
 *
 * WHAT: Metrics tracking emotion-attention interactions
 * WHY:  Monitor system behavior and effectiveness
 * HOW:  Counters and aggregates updated during operation
 */
typedef struct {
    uint64_t emotion_updates_received; /**< Total emotion tensor updates */
    float avg_arousal_modulation;      /**< Average arousal effect on attention */
    float avg_valence_modulation;      /**< Average valence effect on attention */
    float avg_attention_width;         /**< Average attention window width */
    uint64_t emotional_gating_events;  /**< Times emotion gated attention */
    uint64_t congruency_biases;        /**< Times congruency bias applied */
    float current_arousal;             /**< Current arousal level */
    float current_valence;             /**< Current valence level */
    float current_attention_width;     /**< Current attention width */
} emotion_attention_stats_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create emotion-modulated attention system
 *
 * WHAT: Initialize emotion-attention integration
 * WHY:  Enable emotional modulation of attentional processes
 * HOW:  Allocate system, register bio-async callbacks, set defaults
 *
 * @param emotion_tensor Emotion tensor system to subscribe to
 * @param attention Multihead attention system to modulate
 * @param config Configuration (NULL = defaults)
 * @return System handle or NULL on error
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe
 */
emotion_attention_system_t* emotion_attention_create(
    emotion_tensor_system_t* emotion_tensor,
    multihead_attention_t attention,
    const emotion_attention_config_t* config
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
emotion_attention_config_t emotion_attention_default_config(void);

/**
 * @brief Destroy emotion-attention system
 *
 * WHAT: Free all resources
 * WHY:  Prevent memory leaks
 * HOW:  Unregister callbacks, free system
 *
 * @param system System handle
 */
void emotion_attention_destroy(emotion_attention_system_t* system);

//=============================================================================
// Modulation API
//=============================================================================

/**
 * @brief Apply emotion modulation to attention
 *
 * WHAT: Modulate attention based on current emotional state
 * WHY:  Emotions affect how we allocate attentional resources
 * HOW:  Query emotion tensor, adjust attention parameters
 *
 * MODULATION:
 * - High arousal (fear, anger) → narrowed attention focus
 * - Positive valence (joy) → broadened attention scope
 * - Negative valence (sadness) → narrowed, inward focus
 * - Emotional salience → boost attention to congruent stimuli
 *
 * @param system Emotion-attention system
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe
 */
bool emotion_attention_modulate(emotion_attention_system_t* system);

/**
 * @brief Compute emotionally-modulated salience
 *
 * WHAT: Boost salience for emotion-congruent stimuli
 * WHY:  Emotional stimuli capture attention more effectively
 * HOW:  Compare stimulus emotion with current state, boost if congruent
 *
 * EXAMPLES:
 * - Fear state → threats get higher salience
 * - Joy state → opportunities get higher salience
 * - Anger state → provocations get higher salience
 *
 * @param system Emotion-attention system
 * @param base_salience Base salience before emotion modulation [0.0-1.0]
 * @param stimulus_emotion Emotion category of stimulus
 * @return Modulated salience [0.0-1.0]
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe
 */
float emotion_attention_compute_salience(
    emotion_attention_system_t* system,
    float base_salience,
    emotion_primary_t stimulus_emotion
);

/**
 * @brief Get current attention width
 *
 * WHAT: Query how broad/narrow attention focus currently is
 * WHY:  Monitor attentional state
 * HOW:  Return current width parameter [0.0-1.0]
 *
 * @param system Emotion-attention system
 * @return Attention width [0.0-1.0] or -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe
 */
float emotion_attention_get_width(const emotion_attention_system_t* system);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get emotion-attention statistics
 *
 * WHAT: Query system statistics
 * WHY:  Monitor performance and behavior
 * HOW:  Copy internal stats to output
 *
 * @param system Emotion-attention system
 * @param stats Output statistics
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe
 */
bool emotion_attention_get_stats(
    const emotion_attention_system_t* system,
    emotion_attention_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * WHAT: Clear all counters and accumulators
 * WHY:  Start fresh measurement period
 * HOW:  Zero stats structure
 *
 * @param system Emotion-attention system
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe
 */
void emotion_attention_reset_stats(emotion_attention_system_t* system);

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
 * @param system Emotion-attention system
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe
 */
bool emotion_attention_register_bio_async(emotion_attention_system_t* system);

/**
 * @brief Unregister from bio-async system
 *
 * WHAT: Unsubscribe from emotion updates
 * WHY:  Clean shutdown
 * HOW:  Remove callback registration
 *
 * @param system Emotion-attention system
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe
 */
void emotion_attention_unregister_bio_async(emotion_attention_system_t* system);

//=============================================================================
// Sleep State Integration API
//=============================================================================

/**
 * @brief Set sleep state for attention modulation
 *
 * WHAT: Update current sleep state to modulate attention capacity and vigilance
 * WHY:  Sleep state affects attention performance (biological)
 * HOW:  Store state, modulation applied in emotion_attention_modulate()
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Full attention capacity (1.0), normal vigilance (1.0)
 * - DROWSY: Reduced capacity (0.6), lower vigilance (0.5)
 * - LIGHT_NREM: Minimal capacity (0.1), minimal vigilance (0.0)
 * - DEEP_NREM: Offline (0.0), no vigilance (0.0)
 * - REM: Internal only (0.3), low vigilance (0.2)
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe
 *
 * @param system Emotion-attention system (non-NULL)
 * @param state New sleep state
 * @return true on success, false on NULL pointer
 */
bool emotion_attention_set_sleep_state(emotion_attention_system_t* system, sleep_state_t state);

/**
 * @brief Get current sleep state
 *
 * WHAT: Query current sleep/wake state
 * WHY:  Check what modulation is being applied
 * HOW:  Return current_sleep_state field
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe
 *
 * @param system Emotion-attention system (non-NULL)
 * @return Current sleep state, or SLEEP_STATE_AWAKE if NULL
 */
sleep_state_t emotion_attention_get_sleep_state(const emotion_attention_system_t* system);

//=============================================================================
// Positional Encoding Integration API
//=============================================================================

/**
 * @brief Configure positional encoding parameters
 *
 * WHAT: Set or update PE configuration for emotion sequences
 * WHY:  Enable temporal and priority-based encoding of emotions
 * HOW:  Create/recreate PE encoders with specified parameters
 *
 * BIOLOGICAL BASIS:
 * - Emotional processing has temporal dynamics (amygdala→OFC pathway)
 * - Position encoding captures WHEN emotions occurred in sequence
 * - Priority ordering reflects attentional selection (salience-based)
 *
 * @param system Emotion-attention system
 * @param enable_temporal Enable sinusoidal PE for temporal sequences
 * @param enable_priority Enable learned PE for emotion priority
 * @param max_sequence Maximum temporal sequence length
 * @param embedding_dim Dimension of emotion embeddings
 * @return true on success
 *
 * COMPLEXITY: O(max_sequence * embedding_dim) if cache pre-computation enabled
 * THREAD_SAFETY: Thread-safe
 */
bool emotion_attention_set_pe_config(
    emotion_attention_system_t* system,
    bool enable_temporal,
    bool enable_priority,
    uint32_t max_sequence,
    uint32_t embedding_dim
);

/**
 * @brief Apply positional encoding to temporal emotion sequence
 *
 * WHAT: Add sinusoidal PE to emotion state sequence
 * WHY:  Encode temporal ordering of emotional states
 * HOW:  Use sinusoidal encoder to add position information
 *
 * BIOLOGICAL BASIS:
 * - Emotional memory formation is temporally ordered (hippocampal indexing)
 * - Amygdala→OFC pathway processes emotional sequences temporally
 * - Temporal context affects emotional interpretation (mood congruence)
 *
 * USAGE:
 * - emotion_sequence: Array of emotion embeddings [seq_len * embed_dim]
 * - seq_length: Number of emotion states in sequence
 * - output: Output with PE added [seq_len * embed_dim]
 *
 * @param system Emotion-attention system
 * @param emotion_sequence Input emotion embeddings
 * @param seq_length Sequence length
 * @param output Output buffer (can be same as input for in-place)
 * @return true on success
 *
 * COMPLEXITY: O(seq_length * embedding_dim)
 * THREAD_SAFETY: Thread-safe
 */
bool emotion_attention_encode_temporal(
    emotion_attention_system_t* system,
    const float* emotion_sequence,
    uint32_t seq_length,
    float* output
);

/**
 * @brief Get learned positional embedding for emotion priority
 *
 * WHAT: Retrieve learned PE vector for emotion priority position
 * WHY:  Encode attentional priority ordering of emotions
 * HOW:  Use learned encoder to lookup position-specific embedding
 *
 * BIOLOGICAL BASIS:
 * - Attentional selection prioritizes emotions by salience
 * - Priority ordering reflects competitive attention allocation
 * - Learned embeddings capture task-specific priority patterns
 *
 * USAGE:
 * - priority_rank: Priority position (0 = highest priority)
 * - output: Output embedding vector [embedding_dim floats]
 *
 * EXAMPLES:
 * - priority_rank=0: Highest priority emotion (e.g., imminent threat)
 * - priority_rank=1: Second priority (e.g., secondary concern)
 * - priority_rank=n: Lower priority emotions
 *
 * @param system Emotion-attention system
 * @param priority_rank Priority position (0-based, 0=highest)
 * @param output Output embedding buffer [embedding_dim floats]
 * @return true on success
 *
 * COMPLEXITY: O(embedding_dim)
 * THREAD_SAFETY: Thread-safe
 */
bool emotion_attention_get_priority_embedding(
    emotion_attention_system_t* system,
    uint32_t priority_rank,
    float* output
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMOTION_ATTENTION_H */
