/**
 * @file nimcp_emotional_tagging.h
 * @brief Emotional tagging system for cognitive representations
 *
 * WHAT: Tags representations with emotional states (valence + arousal)
 * WHY:  Emotions enhance memory, guide attention, and influence decisions
 * HOW:  Russell's circumplex model (2D: valence × arousal)
 *
 * BIOLOGICAL BASIS:
 * - Amygdala tags emotional events for enhanced memory consolidation
 * - High arousal emotions receive attentional priority
 * - Emotional context aids retrieval and reasoning
 *
 * THEORETICAL FOUNDATION:
 * - Russell (1980): Circumplex model of affect
 * - LaBar & Cabeza (2006): Emotional memory enhancement
 * - Phelps (2004): Amygdala-hippocampus interaction
 *
 * INTEGRATION POINTS:
 * - Working memory: Emotional tags attached to items
 * - Salience computation: Arousal and valence boost salience
 * - Brain processing: Emotions detected from cognitive state
 *
 * CODING STANDARDS:
 * - Guard clauses (no nested ifs)
 * - Helper functions (<50 lines)
 * - WHAT-WHY-HOW documentation
 * - Single Responsibility Principle
 *
 * @author NIMCP Development Team - Phase 10.3
 * @date 2025-11-09
 * @version 2.7.0 Phase 10.3
 */

#ifndef NIMCP_EMOTIONAL_TAGGING_H
#define NIMCP_EMOTIONAL_TAGGING_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Emotion Categories (Russell's Circumplex Model)
//=============================================================================

/**
 * @brief Emotion categories derived from valence-arousal space
 *
 * Based on Russell's circumplex model quadrants
 *
 * NOTE: If nimcp_emotion_recognition.h is included first, its more comprehensive
 * emotion_category_t (Ekman + extended) will be used instead of this simpler version.
 */
#ifndef NIMCP_EMOTION_CATEGORY_T_DEFINED
#define NIMCP_EMOTION_CATEGORY_T_DEFINED
typedef enum {
    EMOTION_CAT_NEUTRAL,      /**< valence ≈ 0, arousal < 0.3 (no strong emotion) */
    EMOTION_CAT_JOY,          /**< valence > 0.5, arousal > 0.5 (happy, excited) */
    EMOTION_CAT_EXCITEMENT,   /**< valence > 0.3, arousal > 0.7 (energized, alert) */
    EMOTION_CAT_CALM,         /**< valence > 0.2, arousal < 0.3 (peaceful, relaxed) */
    EMOTION_CAT_FEAR,         /**< valence < -0.3, arousal > 0.6 (afraid, anxious) */
    EMOTION_CAT_ANGER,        /**< valence < -0.4, arousal > 0.6 (frustrated, angry) */
    EMOTION_CAT_SADNESS,      /**< valence < -0.3, arousal < 0.4 (sad, depressed) */
    EMOTION_CAT_ANXIETY,      /**< valence < -0.2, arousal > 0.5 (worried, tense) */
    EMOTION_CAT_BOREDOM       /**< valence < 0, arousal < 0.2 (disinterested) */
} emotion_category_t;
#endif /* NIMCP_EMOTION_CATEGORY_T_DEFINED */

//=============================================================================
// Core Types
//=============================================================================

/**
 * @brief Emotional tag for cognitive representation
 *
 * WHAT: Two-dimensional emotional state (valence + arousal)
 * WHY:  Capture emotional context of representations
 * HOW:  Russell's circumplex model with derived category
 *
 * DESIGN: Struct (not opaque) for direct access in performance-critical code
 */
typedef struct emotional_tag {
    float valence;               /**< Emotional valence: -1.0 (negative) to +1.0 (positive) */
    float arousal;               /**< Emotional arousal: 0.0 (calm) to 1.0 (excited) */
    uint64_t timestamp_ms;       /**< When emotion was tagged (milliseconds) */
    emotion_category_t category; /**< Derived category (computed from valence+arousal) */
    float intensity;             /**< Overall emotional intensity [0.0, 1.0] */
} emotional_tag_t;

//=============================================================================
// Constants
//=============================================================================

/**
 * @brief Default neutral emotion (no emotional content)
 */
#define EMOTIONAL_TAG_NEUTRAL {0.0f, 0.0f, 0, EMOTION_CAT_NEUTRAL, 0.0f}

/**
 * @brief Arousal multiplier for salience boost
 *
 * WHAT: How much arousal increases salience
 * WHY:  High arousal events grab attention
 * HOW:  Multiplier of 0.5 = up to 50% boost from arousal alone
 */
#define EMOTIONAL_AROUSAL_SALIENCE_FACTOR 0.5f

/**
 * @brief Valence multiplier for salience boost
 *
 * WHAT: How much valence intensity increases salience
 * WHY:  Strong emotions (positive OR negative) are important
 * HOW:  Multiplier of 0.3 = up to 30% boost from valence alone
 */
#define EMOTIONAL_VALENCE_SALIENCE_FACTOR 0.3f

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create emotional tag from valence and arousal
 *
 * WHAT: Initialize emotional tag with 2D emotional state
 * WHY:  Primary constructor for emotional tags
 * HOW:  Classify category, compute intensity from components
 *
 * COMPLEXITY: O(1) - simple computation
 *
 * @param valence Emotional valence [-1.0, +1.0]
 * @param arousal Emotional arousal [0.0, 1.0]
 * @param timestamp_ms Current time in milliseconds
 * @return Emotional tag structure
 *
 * @note Valence and arousal are clamped to valid ranges
 */
emotional_tag_t emotional_tag_create(
    float valence,
    float arousal,
    uint64_t timestamp_ms
);

/**
 * @brief Create neutral emotional tag (no emotion)
 *
 * WHAT: Initialize tag with zero valence and arousal
 * WHY:  Convenience function for neutral states
 * HOW:  Set all fields to neutral/zero values
 *
 * COMPLEXITY: O(1)
 *
 * @return Neutral emotional tag
 */
emotional_tag_t emotional_tag_neutral(void);

//=============================================================================
// Classification & Analysis
//=============================================================================

/**
 * @brief Classify emotion into discrete category
 *
 * WHAT: Map valence-arousal coordinates to emotion category
 * WHY:  Provide human-readable emotional state
 * HOW:  Threshold-based classification in 2D space
 *
 * ALGORITHM:
 * - High arousal + positive valence = JOY/EXCITEMENT
 * - High arousal + negative valence = FEAR/ANGER/ANXIETY
 * - Low arousal + positive valence = CALM
 * - Low arousal + negative valence = SADNESS/BOREDOM
 * - Neutral otherwise
 *
 * COMPLEXITY: O(1) - threshold comparisons
 *
 * @param tag Emotional tag to classify
 * @return Emotion category
 */
emotion_category_t emotional_tag_classify(const emotional_tag_t* tag);

/**
 * @brief Compute overall emotional intensity
 *
 * WHAT: Single scalar representing emotional strength
 * WHY:  Combine arousal and valence into one measure
 * HOW:  Vector magnitude in valence-arousal space
 *
 * FORMULA: intensity = sqrt(valence² + arousal²) / sqrt(2)
 * RANGE: [0.0, 1.0]
 *
 * COMPLEXITY: O(1)
 *
 * @param tag Emotional tag
 * @return Emotional intensity [0.0, 1.0]
 */
float emotional_tag_intensity(const emotional_tag_t* tag);

/**
 * @brief Get human-readable emotion name
 *
 * WHAT: Convert emotion category to string
 * WHY:  Debugging and logging
 * HOW:  Static string lookup
 *
 * COMPLEXITY: O(1)
 *
 * @param category Emotion category
 * @return String name (e.g., "JOY", "FEAR", "NEUTRAL")
 */
const char* emotional_category_name(emotion_category_t category);

//=============================================================================
// Salience Modulation
//=============================================================================

/**
 * @brief Compute salience boost from emotional tag
 *
 * WHAT: Multiplier for base salience based on emotion
 * WHY:  Emotional events receive attentional priority
 * HOW:  Arousal and valence intensity both increase salience
 *
 * FORMULA:
 * boost = 1.0 + (arousal × AROUSAL_FACTOR) + (|valence| × VALENCE_FACTOR)
 *
 * EXAMPLES:
 * - Fear (valence=-0.8, arousal=0.9): boost ≈ 1.69× (69% increase)
 * - Joy (valence=0.9, arousal=0.8): boost ≈ 1.67× (67% increase)
 * - Neutral (valence=0, arousal=0): boost = 1.0× (no change)
 *
 * BIOLOGICAL: Amygdala arousal enhances hippocampal encoding
 *
 * COMPLEXITY: O(1)
 *
 * @param tag Emotional tag
 * @return Salience multiplier [1.0, ~2.0]
 */
float emotional_compute_salience_boost(const emotional_tag_t* tag);

/**
 * @brief Apply emotional boost to base salience
 *
 * WHAT: Compute final salience with emotional enhancement
 * WHY:  Convenience function for salience computation
 * HOW:  Multiply base salience by emotional boost
 *
 * COMPLEXITY: O(1)
 *
 * @param base_salience Original salience value
 * @param tag Emotional tag
 * @return Enhanced salience
 */
float emotional_apply_salience_boost(
    float base_salience,
    const emotional_tag_t* tag
);

//=============================================================================
// Emotion Detection from Cognitive State
//=============================================================================

/**
 * @brief Detect emotion from cognitive processing state
 *
 * WHAT: Infer emotional tag from cognitive assessment outputs
 * WHY:  Automatic emotion tagging during brain processing
 * HOW:  Map cognitive signals to valence-arousal space
 *
 * MAPPING:
 * - confidence → valence (high confidence = positive)
 * - uncertainty → arousal (high uncertainty = high arousal)
 * - novelty → positive valence + arousal (curiosity)
 * - ethical_violation → strong negative valence
 *
 * COMPLEXITY: O(1)
 *
 * @param confidence Prediction confidence [0.0, 1.0]
 * @param uncertainty Introspection uncertainty [0.0, 1.0]
 * @param novelty Novelty/curiosity score [0.0, 1.0]
 * @param ethical_approved Whether action is ethically approved
 * @param timestamp_ms Current time in milliseconds
 * @return Detected emotional tag
 */
emotional_tag_t emotional_tag_from_cognitive_state(
    float confidence,
    float uncertainty,
    float novelty,
    bool ethical_approved,
    uint64_t timestamp_ms
);

//=============================================================================
// Validation
//=============================================================================

/**
 * @brief Validate emotional tag values
 *
 * WHAT: Check if valence and arousal are in valid ranges
 * WHY:  Prevent invalid emotional states
 * HOW:  Range checks on valence and arousal
 *
 * VALIDITY:
 * - valence ∈ [-1.0, +1.0]
 * - arousal ∈ [0.0, 1.0]
 * - intensity ∈ [0.0, 1.0]
 *
 * COMPLEXITY: O(1)
 *
 * @param tag Emotional tag to validate
 * @return true if valid, false otherwise
 */
bool emotional_tag_is_valid(const emotional_tag_t* tag);

/**
 * @brief Clamp emotional tag to valid ranges
 *
 * WHAT: Force valence and arousal into valid ranges
 * WHY:  Sanitize potentially invalid emotional states
 * HOW:  Clamp to [-1,+1] for valence, [0,1] for arousal
 *
 * COMPLEXITY: O(1)
 *
 * @param tag Emotional tag to clamp (modified in place)
 */
void emotional_tag_clamp(emotional_tag_t* tag);

//=============================================================================
// Bidirectional Feedback Functions (Phase 10.11.3)
//=============================================================================

/**
 * @brief Get emotional valence from tag
 *
 * WHAT: Query emotional positivity/negativity
 * WHY:  Salience can bias attention based on mood
 * HOW:  Return valence from emotional tag
 *
 * BIOLOGY: Mood-congruent perception bias
 *
 * @param tag Emotional tag
 * @return Valence [-1, +1] (negative to positive)
 */
float emotional_get_valence(const emotional_tag_t* tag);

/**
 * @brief Get emotional arousal from tag
 *
 * WHAT: Query emotional activation level
 * WHY:  Salience boosted by high arousal
 * HOW:  Return arousal from emotional tag
 *
 * @param tag Emotional tag
 * @return Arousal [0, 1] (calm to excited)
 */
float emotional_get_arousal(const emotional_tag_t* tag);

/**
 * @brief Modulate arousal in emotional tag
 *
 * WHAT: Allow salience to influence emotional arousal
 * WHY:  Surprising/salient events increase arousal
 * HOW:  Add delta to current arousal, clamped to [0, 1]
 *
 * @param tag Emotional tag to modulate (modified in place)
 * @param arousal_delta Arousal change [-1, +1]
 */
void emotional_modulate_arousal(emotional_tag_t* tag, float arousal_delta);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_EMOTIONAL_TAGGING_H
