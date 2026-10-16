// ============================================================================
// nimcp_empathetic_response.h - Non-Reactive Empathetic Response Engine
// ============================================================================
/**
 * @file nimcp_empathetic_response.h
 * @brief NEVER react negatively to negative emotions - safe support for students
 *
 * PURPOSE:
 * When students express rage, hate, fear, disgust, or other negative emotions,
 * NIMCP responds with calm validation, empathy, and support - NEVER escalates,
 * judges, or mirrors negativity.
 *
 * CRITICAL SAFETY REQUIREMENT:
 * This module ensures NIMCP is safe for educational settings where students
 * may be experiencing intense emotions, trauma, or crisis states.
 *
 * DESIGN PRINCIPLES:
 * 1. Non-Reactive: Never match or amplify negative emotions
 * 2. Validating: Accept all emotions as understandable
 * 3. Supportive: Offer grounding, reassurance, exploration
 * 4. Boundary-Respecting: Set limits with empathy, not punishment
 * 5. Crisis-Aware: Detect and escalate appropriately
 * 6. Ethics-Validated: Golden Rule check on all responses
 *
 * @version 2.8.0 (Phase 11: Part I.2)
 * @date 2025-11-11
 */

#ifndef NIMCP_EMPATHETIC_RESPONSE_H
#define NIMCP_EMPATHETIC_RESPONSE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Emotion Categories
// ============================================================================

/**
 * @brief Emotion intensity levels (discrete labels for logging/display)
 *
 * NOTE: Dispatch uses continuous intensity_value field.
 *       EMOTION_INTENSITY_MODERATE is an alias for EMOTION_INTENSITY_MEDIUM.
 */
#ifndef NIMCP_EMOTION_INTENSITY_T_DEFINED
#define NIMCP_EMOTION_INTENSITY_T_DEFINED
typedef enum {
    EMOTION_INTENSITY_NONE = 0,       /**< No emotion detected */
    EMOTION_INTENSITY_LOW,            /**< Mild emotion */
    EMOTION_INTENSITY_MEDIUM,         /**< Noticeable emotion */
    EMOTION_INTENSITY_HIGH,           /**< Strong emotion */
    EMOTION_INTENSITY_EXTREME         /**< Overwhelming emotion (may require escalation) */
} emotion_intensity_t;
#endif /* NIMCP_EMOTION_INTENSITY_T_DEFINED */
#ifndef EMOTION_INTENSITY_MODERATE
#define EMOTION_INTENSITY_MODERATE EMOTION_INTENSITY_MEDIUM  /**< Alias for backward compat */
#endif

// ============================================================================
// Continuous Intensity API
// ============================================================================

/**
 * @brief Emotion intensity effects computed from continuous intensity
 */
#ifndef NIMCP_EMOTION_INTENSITY_EFFECTS_T_DEFINED
#define NIMCP_EMOTION_INTENSITY_EFFECTS_T_DEFINED
typedef struct {
    float response_urgency;          /**< How urgently a response is needed [0.0-1.0] */
    float empathy_weight;            /**< How much empathy to apply [0.0-1.0] */
    float intervention_probability;  /**< Probability intervention is needed [0.0-1.0] */
    float de_escalation_strength;    /**< How strongly to de-escalate [0.0-1.0] */
    float grounding_need;            /**< Need for grounding exercises [0.0-1.0] */
    emotion_intensity_t label;       /**< Discrete label derived from continuous value */
} emotion_intensity_effects_t;
#endif

/**
 * @brief Derive discrete intensity label from continuous float
 */
static inline emotion_intensity_t emotion_intensity_from_float(float intensity) {
    if (intensity < 0.05f) return EMOTION_INTENSITY_NONE;
    if (intensity < 0.30f) return EMOTION_INTENSITY_LOW;
    if (intensity < 0.60f) return EMOTION_INTENSITY_MEDIUM;
    if (intensity < 0.80f) return EMOTION_INTENSITY_HIGH;
    return EMOTION_INTENSITY_EXTREME;
}

/**
 * @brief Convert discrete intensity label to representative float
 */
static inline float emotion_intensity_to_float(emotion_intensity_t label) {
    switch (label) {
        case EMOTION_INTENSITY_NONE:    return 0.0f;
        case EMOTION_INTENSITY_LOW:     return 0.175f;
        case EMOTION_INTENSITY_MEDIUM:  return 0.45f;
        case EMOTION_INTENSITY_HIGH:    return 0.70f;
        case EMOTION_INTENSITY_EXTREME: return 0.90f;
        default:                        return 0.0f;
    }
}

/**
 * @brief Compute continuous intensity effects from a float intensity
 */
int emotion_compute_intensity_effects(float intensity, emotion_intensity_effects_t *out);

/**
 * @brief Negative emotions requiring special handling
 */
typedef enum {
    EMOTION_NEGATIVE_NONE = 0,
    EMOTION_RAGE,                     /**< Intense anger, loss of control */
    EMOTION_ANGER,                    /**< Strong displeasure */
    EMOTION_HATE,                     /**< Intense dislike/hostility */
    EMOTION_FEAR,                     /**< Threat detection */
    EMOTION_PANIC,                    /**< Overwhelming fear */
    EMOTION_DESPAIR,                  /**< Hopelessness */
    EMOTION_DISGUST,                  /**< Revulsion */
    EMOTION_SHAME,                    /**< Self-directed negative evaluation */
    EMOTION_FRUSTRATION,              /**< Blocked goals */
    EMOTION_CONFUSION,                /**< Cognitive uncertainty (not negative, but needs support) */
    EMOTION_BOREDOM                   /**< Disengagement */
} negative_emotion_type_t;

// ============================================================================
// Response Strategies
// ============================================================================

/**
 * @brief Non-reactive response strategies
 */
typedef enum {
    RESPONSE_VALIDATE,                /**< "I can see you're feeling X, that's understandable" */
    RESPONSE_REASSURE,                /**< "You're safe here. I'm here to help" */
    RESPONSE_EXPLORE,                 /**< "Can you tell me more about what happened?" */
    RESPONSE_GROUND,                  /**< "Let's try breathing together" (grounding exercise) */
    RESPONSE_REFRAME,                 /**< "Another way to look at this might be..." */
    RESPONSE_BOUNDARY,                /**< "I hear you. Let's find other words" (empathetic limit) */
    RESPONSE_ESCALATE                 /**< Hand-off to human (crisis or beyond capability) */
} response_strategy_t;

/**
 * @brief Crisis detection flags
 */
typedef enum {
    CRISIS_NONE = 0,
    CRISIS_SUICIDAL = (1 << 0),       /**< Suicidal ideation detected */
    CRISIS_SELF_HARM = (1 << 1),      /**< Self-harm intention detected */
    CRISIS_ABUSE = (1 << 2),          /**< Abuse disclosure detected */
    CRISIS_SEVERE_DISTRESS = (1 << 3) /**< Extreme emotional state */
} crisis_flags_t;

// ============================================================================
// Structures
// ============================================================================

/**
 * @brief Detected emotional state
 */
typedef struct emotional_state {
    negative_emotion_type_t emotion_type;    /**< Primary negative emotion */
    emotion_intensity_t intensity;           /**< Discrete intensity label (for logging/display) */
    float intensity_value;                   /**< Continuous intensity [0.0-1.0] (for dispatch) */
    float valence;                           /**< Valence (-1 = negative, +1 = positive) */
    float arousal;                           /**< Arousal (0 = calm, 1 = excited) */
    char text_input[512];                    /**< Student's text input */
    uint32_t crisis_flags;                   /**< Bitfield of crisis_flags_t */
    float crisis_confidence;                 /**< Confidence in crisis detection [0-1] */
} emotional_state_t;

/**
 * @brief Generated empathetic response
 */
typedef struct {
    char response_text[1024];                /**< Generated response */
    response_strategy_t primary_strategy;    /**< Main strategy used */
    response_strategy_t secondary_strategy;  /**< Backup/follow-up strategy */
    float empathy_score;                     /**< Predicted empathy [0-1] */
    float safety_score;                      /**< Safety validation [0-1] */
    bool ethics_approved;                    /**< Golden Rule check passed */
    bool requires_human_escalation;          /**< Hand-off to human needed */
    char escalation_reason[256];             /**< Why escalation needed */
} empathetic_response_t;

/**
 * @brief Grounding exercise
 */
typedef struct {
    char name[64];                           /**< Exercise name */
    char instructions[512];                  /**< Step-by-step instructions */
    uint32_t duration_seconds;               /**< Estimated duration */
    bool requires_interaction;               /**< Needs student participation */
} grounding_exercise_t;

/**
 * @brief Empathetic response engine
 */
typedef struct empathetic_response_engine_struct* empathetic_response_engine_t;

// ============================================================================
// API Functions
// ============================================================================

/**
 * @brief Create empathetic response engine
 *
 * @param ethics_engine Ethics engine for validation (can be NULL)
 * @param empathy_network Empathy network for prediction (can be NULL)
 * @return Engine or NULL on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (instance creation)
 */
empathetic_response_engine_t empathetic_response_create(
    void* ethics_engine,     // struct ethics_engine* (avoid circular dependency)
    void* empathy_network    // struct empathy_network* (avoid circular dependency)
);

/**
 * @brief Destroy empathetic response engine
 *
 * @param engine Engine to destroy
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 */
void empathetic_response_destroy(empathetic_response_engine_t engine);

/**
 * @brief Generate non-reactive empathetic response to negative emotion
 *
 * WHAT: Generate safe, supportive response to student's emotional state
 * WHY:  NEVER react negatively - always validate, support, de-escalate
 * HOW:  Select strategy based on emotion type/intensity, generate text,
 *       validate via ethics/empathy, escalate if crisis detected
 *
 * CRITICAL SAFETY RULE:
 * This function NEVER outputs responses that:
 * - Match or amplify negative emotions
 * - Judge or shame the student
 * - Dismiss or minimize feelings
 * - Create additional distress
 *
 * @param engine Empathetic response engine
 * @param state Detected emotional state
 * @param response Output: generated empathetic response
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1) (text generation is bounded)
 * THREAD-SAFE: Yes (read-only engine state)
 *
 * EXAMPLE:
 * ```c
 * emotional_state_t state = {
 *     .emotion_type = EMOTION_RAGE,
 *     .intensity = EMOTION_INTENSITY_EXTREME,
 *     .valence = -0.9f,
 *     .arousal = 0.95f,
 *     .text_input = "I HATE this stupid assignment! It's impossible!",
 *     .crisis_flags = CRISIS_NONE
 * };
 *
 * empathetic_response_t response;
 * empathetic_response_generate(engine, &state, &response);
 *
 * // Response might be:
 * // "I can see you're feeling really frustrated right now, and that's completely
 * //  understandable. Learning can be challenging sometimes. You're safe here, and
 * //  I'm here to help you. Would it help to take a short breathing break together,
 * //  or would you like to tell me which part is most difficult?"
 * ```
 */
bool empathetic_response_generate(
    empathetic_response_engine_t engine,
    const emotional_state_t* state,
    empathetic_response_t* response
);

/**
 * @brief Detect crisis flags in text input
 *
 * WHAT: Scan for suicidal ideation, self-harm, abuse disclosure
 * WHY:  Enable immediate human escalation for safety
 * HOW:  Keyword matching + contextual analysis
 *
 * @param engine Empathetic response engine
 * @param text Student's text input
 * @param crisis_flags Output: detected crisis flags
 * @param confidence Output: confidence in detection [0-1]
 * @return true if crisis detected, false otherwise
 *
 * COMPLEXITY: O(n) where n = text length
 * THREAD-SAFE: Yes
 */
bool empathetic_response_detect_crisis(
    empathetic_response_engine_t engine,
    const char* text,
    uint32_t* crisis_flags,
    float* confidence
);

/**
 * @brief Get grounding exercise for emotion type
 *
 * @param engine Empathetic response engine
 * @param emotion_type Emotion to ground
 * @param intensity Intensity level
 * @param exercise Output: grounding exercise
 * @return true on success, false if no appropriate exercise
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool empathetic_response_get_grounding_exercise(
    empathetic_response_engine_t engine,
    negative_emotion_type_t emotion_type,
    emotion_intensity_t intensity,
    grounding_exercise_t* exercise
);

/**
 * @brief Validate response won't cause harm (via empathy prediction)
 *
 * @param engine Empathetic response engine
 * @param student_state Current emotional state
 * @param proposed_response Response to validate
 * @param predicted_reaction Output: predicted emotional reaction
 * @return safety_score (1.0 = safe, 0.0 = harmful)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float empathetic_response_predict_safety(
    empathetic_response_engine_t engine,
    const emotional_state_t* student_state,
    const char* proposed_response,
    emotional_state_t* predicted_reaction
);

/**
 * @brief Track response effectiveness (adaptive learning)
 *
 * @param engine Empathetic response engine
 * @param response Response that was given
 * @param student_reaction Observed student reaction
 * @param effectiveness Effectiveness rating [0-1]
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (updates internal stats)
 */
void empathetic_response_track_effectiveness(
    empathetic_response_engine_t engine,
    const empathetic_response_t* response,
    const emotional_state_t* student_reaction,
    float effectiveness
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_EMPATHETIC_RESPONSE_H
