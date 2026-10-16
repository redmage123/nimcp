/**
 * @file nimcp_emotion_tensor.h
 * @brief Tensor-Based Emotional Representation System
 *
 * WHAT: Multi-dimensional tensor representation for complex, mixed, and contradictory emotions
 * WHY:  Scalar valence/arousal cannot capture mixed emotions (bittersweet, ambivalence)
 * HOW:  Multiple emotion channels active simultaneously with interaction dynamics
 *
 * THEORETICAL FOUNDATIONS:
 * - Plutchik (1980): Wheel of emotions with 8 primary emotions that blend like colors
 * - Russell (1980): Circumplex model extended to N dimensions
 * - Scherer (2009): Component Process Model with appraisal dimensions
 * - Barrett (2017): Constructed emotion theory - emotions as dynamic patterns
 *
 * ARCHITECTURE:
 * - Primary emotion channels: All can be active simultaneously [0,1]
 * - Interaction matrix: How emotions influence/suppress each other
 * - Temporal dynamics: Emotion trajectories over time
 * - Appraisal dimensions: Certainty, control, relevance per emotion
 * - Compound emotions: Plutchik dyads (bittersweet, ambivalence, nostalgia)
 *
 * CODING STANDARDS:
 * - Guard clauses (no nested ifs)
 * - Helper functions (<50 lines)
 * - WHAT-WHY-HOW documentation
 * - Single Responsibility Principle
 * - 100% test coverage (unit + integration + regression + e2e)
 *
 * @author NIMCP Development Team
 * @date 2025-12-09
 * @version 1.0.0
 */

#ifndef NIMCP_EMOTION_TENSOR_H
#define NIMCP_EMOTION_TENSOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Number of primary emotions (Plutchik's 8 basic emotions) */
#define EMOTION_TENSOR_PRIMARY_COUNT 8

/** Number of compound emotions (Plutchik's dyads) */
#define EMOTION_TENSOR_COMPOUND_COUNT 24

/** Temporal window for dynamics tracking (steps) */
#define EMOTION_TENSOR_TEMPORAL_WINDOW 16

/** Maximum blend depth for compound emotions */
#define EMOTION_TENSOR_MAX_BLEND_DEPTH 3

//=============================================================================
// Primary Emotion Indices (Plutchik's Wheel)
//=============================================================================

typedef enum {
    TENSOR_JOY = 0,           /**< Happiness, elation, contentment */
    TENSOR_TRUST = 1,         /**< Acceptance, admiration */
    TENSOR_FEAR = 2,          /**< Apprehension, terror */
    TENSOR_SURPRISE = 3,      /**< Amazement, distraction */
    TENSOR_SADNESS = 4,       /**< Pensiveness, grief */
    TENSOR_DISGUST = 5,       /**< Boredom, loathing */
    TENSOR_ANGER = 6,         /**< Annoyance, rage */
    TENSOR_ANTICIPATION = 7,  /**< Interest, vigilance */
    TENSOR_PRIMARY_COUNT = 8
} emotion_primary_t;

//=============================================================================
// Compound Emotion Indices (Plutchik's Dyads)
//=============================================================================

typedef enum {
    /* Primary dyads (adjacent emotions) */
    COMPOUND_LOVE = 0,              /**< Joy + Trust */
    COMPOUND_SUBMISSION = 1,        /**< Trust + Fear */
    COMPOUND_AWE = 2,               /**< Fear + Surprise */
    COMPOUND_DISAPPROVAL = 3,       /**< Surprise + Sadness */
    COMPOUND_REMORSE = 4,           /**< Sadness + Disgust */
    COMPOUND_CONTEMPT = 5,          /**< Disgust + Anger */
    COMPOUND_AGGRESSIVENESS = 6,    /**< Anger + Anticipation */
    COMPOUND_OPTIMISM = 7,          /**< Anticipation + Joy */

    /* Secondary dyads (one emotion apart) */
    COMPOUND_GUILT = 8,             /**< Joy + Fear */
    COMPOUND_CURIOSITY = 9,         /**< Trust + Surprise */
    COMPOUND_DESPAIR = 10,          /**< Fear + Sadness */
    COMPOUND_UNBELIEF = 11,         /**< Surprise + Disgust */
    COMPOUND_ENVY = 12,             /**< Sadness + Anger */
    COMPOUND_CYNICISM = 13,         /**< Disgust + Anticipation */
    COMPOUND_PRIDE = 14,            /**< Anger + Joy */
    COMPOUND_HOPE = 15,             /**< Anticipation + Trust */

    /* Tertiary dyads (two emotions apart) - mixed/contradictory */
    COMPOUND_BITTERSWEETNESS = 16,  /**< Joy + Sadness */
    COMPOUND_MORBID_CURIOSITY = 17, /**< Trust + Disgust */
    COMPOUND_ANXIETY = 18,          /**< Fear + Anger */
    COMPOUND_AMBIVALENCE = 19,      /**< Surprise + Anticipation */
    COMPOUND_DESOLATION = 20,       /**< Sadness + Joy (inverse) */
    COMPOUND_FROZENNESS = 21,       /**< Disgust + Trust (inverse) */
    COMPOUND_OUTRAGE = 22,          /**< Anger + Fear */
    COMPOUND_NOSTALGIA = 23,        /**< Anticipation + Sadness */
    COMPOUND_COUNT = 24
} emotion_compound_t;

//=============================================================================
// Appraisal Dimension Indices (Scherer's Component Model)
//=============================================================================

typedef enum {
    APPRAISAL_CERTAINTY = 0,       /**< How sure about this feeling [0,1] */
    APPRAISAL_CONTROL = 1,         /**< Perceived control over cause [0,1] */
    APPRAISAL_RELEVANCE = 2,       /**< Personal significance [0,1] */
    APPRAISAL_PLEASANTNESS = 3,    /**< Intrinsic hedonic quality [0,1] */
    APPRAISAL_NOVELTY = 4,         /**< Unexpectedness [0,1] */
    APPRAISAL_GOAL_CONDUCIVE = 5,  /**< Helps achieve goals [0,1] */
    APPRAISAL_COUNT = 6
} appraisal_dimension_t;

//=============================================================================
// Core Structures
//=============================================================================

/**
 * @brief Emotion tensor configuration
 *
 * WHAT: Controls tensor behavior and dynamics
 * WHY:  Allow tuning for different use cases
 * HOW:  Feature flags + rate parameters
 */
typedef struct {
    float decay_rate;              /**< How fast emotions decay [0.01, 0.5] */
    float interaction_strength;    /**< Strength of emotion interactions [0, 1] */
    float blend_threshold;         /**< Min activation for compound [0.1, 0.5] */
    float dominance_threshold;     /**< Min for primary/secondary [0.3, 0.7] */
    bool enable_temporal_dynamics; /**< Track emotion trajectories */
    bool enable_appraisals;        /**< Track appraisal dimensions */
    bool enable_interactions;      /**< Enable emotion interactions */
} emotion_tensor_config_t;

/**
 * @brief Primary emotion tensor representation
 *
 * WHAT: Multi-channel emotion state where all channels can be active
 * WHY:  Enable mixed emotions (joy AND sadness simultaneously)
 * HOW:  Each channel [0,1] represents activation level
 */
typedef struct {
    /* Primary emotion channels - all can be active simultaneously */
    float channels[EMOTION_TENSOR_PRIMARY_COUNT];

    /* Appraisal dimensions per emotion */
    float appraisals[EMOTION_TENSOR_PRIMARY_COUNT][APPRAISAL_COUNT];

    /* Temporal dynamics - emotion trajectories */
    float dynamics[EMOTION_TENSOR_PRIMARY_COUNT][EMOTION_TENSOR_TEMPORAL_WINDOW];
    uint32_t dynamics_index;

    /* Compound emotion activations (computed from primary) */
    float compounds[EMOTION_TENSOR_COMPOUND_COUNT];

    /* Dominant emotions for quick access */
    emotion_primary_t primary_emotion;
    emotion_primary_t secondary_emotion;
    float primary_strength;
    float secondary_strength;
    float blend_ratio;

    /* Aggregate metrics */
    float overall_valence;     /**< Computed from channels [-1, +1] */
    float overall_arousal;     /**< Computed from channels [0, 1] */
    float emotional_entropy;   /**< Diversity of active emotions [0, 1] */
    float stability;           /**< How stable over time [0, 1] */

    /* Timestamp */
    uint64_t last_update_ms;
} emotion_tensor_t;

/**
 * @brief Emotion interaction matrix
 *
 * WHAT: How emotions influence each other
 * WHY:  Some emotions suppress others (anger suppresses fear)
 * HOW:  Matrix[i][j] = effect of emotion j on emotion i
 */
typedef struct {
    float matrix[EMOTION_TENSOR_PRIMARY_COUNT][EMOTION_TENSOR_PRIMARY_COUNT];
} emotion_interaction_matrix_t;

/**
 * @brief Emotion tensor system handle (opaque)
 */
typedef struct emotion_tensor_system emotion_tensor_system_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create emotion tensor system
 *
 * WHAT: Initialize tensor-based emotional representation
 * WHY:  Enable complex mixed emotion modeling
 * HOW:  Allocate system, initialize interaction matrix, set defaults
 *
 * @param config Configuration (NULL = defaults)
 * @return System handle or NULL on error
 *
 * COMPLEXITY: O(N^2) where N = primary emotion count
 * THREAD_SAFETY: Thread-safe
 */
emotion_tensor_system_t* emotion_tensor_create(const emotion_tensor_config_t* config);

/**
 * @brief Get default configuration
 *
 * WHAT: Return sensible defaults for tensor system
 * WHY:  Convenient initialization
 * HOW:  Return pre-filled config struct
 *
 * @return Default configuration
 */
emotion_tensor_config_t emotion_tensor_default_config(void);

/**
 * @brief Destroy emotion tensor system
 *
 * WHAT: Free all resources
 * WHY:  Prevent memory leaks
 * HOW:  Free system structure
 *
 * @param system System handle
 */
void emotion_tensor_destroy(emotion_tensor_system_t* system);

//=============================================================================
// State Query API
//=============================================================================

/**
 * @brief Get current emotion tensor
 *
 * WHAT: Query complete tensor state
 * WHY:  Allow external access to emotional state
 * HOW:  Copy internal tensor to output
 *
 * @param system System handle
 * @param tensor Output tensor
 * @return true on success
 */
bool emotion_tensor_get(const emotion_tensor_system_t* system, emotion_tensor_t* tensor);

/**
 * @brief Get specific emotion channel activation
 *
 * WHAT: Query single emotion activation level
 * WHY:  Quick check for specific emotion
 * HOW:  Return channels[emotion]
 *
 * @param system System handle
 * @param emotion Primary emotion index
 * @return Activation level [0, 1] or -1 on error
 */
float emotion_tensor_get_channel(const emotion_tensor_system_t* system, emotion_primary_t emotion);

/**
 * @brief Get compound emotion activation
 *
 * WHAT: Query compound emotion strength
 * WHY:  Check for mixed emotions like bittersweet
 * HOW:  Return compounds[compound]
 *
 * @param system System handle
 * @param compound Compound emotion index
 * @return Activation level [0, 1] or -1 on error
 */
float emotion_tensor_get_compound(const emotion_tensor_system_t* system, emotion_compound_t compound);

/**
 * @brief Check if emotions are contradictory
 *
 * WHAT: Detect conflicting emotional states
 * WHY:  Identify emotional ambivalence
 * HOW:  Check if opposing emotions both active above threshold
 *
 * @param system System handle
 * @param threshold Minimum activation for conflict [0, 1]
 * @return true if contradictory emotions detected
 */
bool emotion_tensor_is_contradictory(const emotion_tensor_system_t* system, float threshold);

/**
 * @brief Get backward-compatible scalar valence
 *
 * WHAT: Compute scalar valence from tensor
 * WHY:  Backward compatibility with scalar emotion systems
 * HOW:  Weighted sum of positive/negative channels
 *
 * @param system System handle
 * @return Valence [-1, +1] or 0 on error
 */
float emotion_tensor_get_valence(const emotion_tensor_system_t* system);

/**
 * @brief Get backward-compatible scalar arousal
 *
 * WHAT: Compute scalar arousal from tensor
 * WHY:  Backward compatibility with scalar emotion systems
 * HOW:  Average activation across all channels
 *
 * @param system System handle
 * @return Arousal [0, 1] or 0 on error
 */
float emotion_tensor_get_arousal(const emotion_tensor_system_t* system);

//=============================================================================
// State Update API
//=============================================================================

/**
 * @brief Set emotion channel activation
 *
 * WHAT: Set single emotion activation level
 * WHY:  Direct emotional control or stimulus response
 * HOW:  Set channels[emotion] and update compounds
 *
 * @param system System handle
 * @param emotion Primary emotion index
 * @param activation Activation level [0, 1]
 * @param timestamp_ms Current time
 * @return true on success
 */
bool emotion_tensor_set_channel(
    emotion_tensor_system_t* system,
    emotion_primary_t emotion,
    float activation,
    uint64_t timestamp_ms
);

/**
 * @brief Set multiple emotion channels at once
 *
 * WHAT: Bulk set emotion activations
 * WHY:  Efficient multi-emotion updates
 * HOW:  Set all channels and recompute compounds
 *
 * @param system System handle
 * @param activations Array of activations [EMOTION_TENSOR_PRIMARY_COUNT]
 * @param timestamp_ms Current time
 * @return true on success
 */
bool emotion_tensor_set_channels(
    emotion_tensor_system_t* system,
    const float* activations,
    uint64_t timestamp_ms
);

/**
 * @brief Set appraisal dimension for emotion
 *
 * WHAT: Set appraisal value for specific emotion
 * WHY:  Capture cognitive evaluation of emotional state
 * HOW:  Set appraisals[emotion][dimension]
 *
 * @param system System handle
 * @param emotion Primary emotion index
 * @param dimension Appraisal dimension
 * @param value Appraisal value [0, 1]
 * @return true on success
 */
bool emotion_tensor_set_appraisal(
    emotion_tensor_system_t* system,
    emotion_primary_t emotion,
    appraisal_dimension_t dimension,
    float value
);

/**
 * @brief Apply stimulus to emotion tensor
 *
 * WHAT: Process external stimulus affecting emotions
 * WHY:  React to events, inputs, situations
 * HOW:  Modify channels based on stimulus type and intensity
 *
 * @param system System handle
 * @param emotion Target emotion
 * @param intensity Stimulus intensity [0, 1]
 * @param is_positive Whether stimulus is positive
 * @param timestamp_ms Current time
 * @return true on success
 */
bool emotion_tensor_apply_stimulus(
    emotion_tensor_system_t* system,
    emotion_primary_t emotion,
    float intensity,
    bool is_positive,
    uint64_t timestamp_ms
);

//=============================================================================
// Dynamics API
//=============================================================================

/**
 * @brief Update emotion dynamics (decay + interactions)
 *
 * WHAT: Apply temporal dynamics to emotion tensor
 * WHY:  Emotions naturally decay and interact over time
 * HOW:  Apply decay, compute interactions, update compounds
 *
 * @param system System handle
 * @param delta_time Time elapsed (seconds)
 * @param timestamp_ms Current time
 * @return true on success
 */
bool emotion_tensor_update(
    emotion_tensor_system_t* system,
    float delta_time,
    uint64_t timestamp_ms
);

/**
 * @brief Compute compound emotions from primary channels
 *
 * WHAT: Calculate Plutchik dyads from primary emotions
 * WHY:  Derive complex emotions from basic ones
 * HOW:  Multiply adjacent/opposite channel pairs
 *
 * @param system System handle
 * @return true on success
 */
bool emotion_tensor_compute_compounds(emotion_tensor_system_t* system);

/**
 * @brief Apply emotion interactions
 *
 * WHAT: Let emotions influence each other
 * WHY:  Some emotions suppress others (anger suppresses fear)
 * HOW:  Apply interaction matrix to channels
 *
 * @param system System handle
 * @param delta_time Time elapsed (seconds)
 * @return true on success
 */
bool emotion_tensor_apply_interactions(emotion_tensor_system_t* system, float delta_time);

/**
 * @brief Reset emotion tensor to neutral
 *
 * WHAT: Clear all emotion activations
 * WHY:  Return to baseline emotional state
 * HOW:  Zero all channels, compounds, dynamics
 *
 * @param system System handle
 * @return true on success
 */
bool emotion_tensor_reset(emotion_tensor_system_t* system);

//=============================================================================
// Analysis API
//=============================================================================

/**
 * @brief Get emotional entropy (diversity)
 *
 * WHAT: Measure how many emotions are active
 * WHY:  High entropy = mixed emotions, low = focused emotion
 * HOW:  Shannon entropy of normalized channel activations
 *
 * @param system System handle
 * @return Entropy [0, 1] or -1 on error
 */
float emotion_tensor_get_entropy(const emotion_tensor_system_t* system);

/**
 * @brief Get emotional stability over time
 *
 * WHAT: Measure how stable emotions are
 * WHY:  Detect emotional volatility (mental health indicator)
 * HOW:  Variance of temporal dynamics
 *
 * @param system System handle
 * @return Stability [0, 1] (1 = very stable) or -1 on error
 */
float emotion_tensor_get_stability(const emotion_tensor_system_t* system);

/**
 * @brief Get dominant emotion pair
 *
 * WHAT: Find primary and secondary dominant emotions
 * WHY:  Quick summary of emotional state
 * HOW:  Find top 2 channel activations
 *
 * @param system System handle
 * @param primary Output primary emotion
 * @param secondary Output secondary emotion
 * @param blend_ratio Output ratio of secondary to primary [0, 1]
 * @return true on success
 */
bool emotion_tensor_get_dominant(
    const emotion_tensor_system_t* system,
    emotion_primary_t* primary,
    emotion_primary_t* secondary,
    float* blend_ratio
);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Get emotion name string
 *
 * WHAT: Human-readable name for emotion
 * WHY:  Logging, debugging, display
 * HOW:  Return static string
 *
 * @param emotion Primary emotion index
 * @return Emotion name or "unknown"
 */
const char* emotion_tensor_emotion_name(emotion_primary_t emotion);

/**
 * @brief Get compound emotion name string
 *
 * WHAT: Human-readable name for compound emotion
 * WHY:  Logging, debugging, display
 * HOW:  Return static string
 *
 * @param compound Compound emotion index
 * @return Compound name or "unknown"
 */
const char* emotion_tensor_compound_name(emotion_compound_t compound);

/**
 * @brief Initialize default interaction matrix
 *
 * WHAT: Set up biologically-plausible emotion interactions
 * WHY:  Emotions influence each other (anger suppresses fear)
 * HOW:  Set empirically-derived interaction weights
 *
 * @param matrix Output interaction matrix
 */
void emotion_tensor_init_interaction_matrix(emotion_interaction_matrix_t* matrix);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMOTION_TENSOR_H */
