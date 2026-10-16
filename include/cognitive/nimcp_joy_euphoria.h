/**
 * @file nimcp_joy_euphoria.h
 * @brief Joy, euphoria, and value-aligned success reward system
 *
 * WHAT: Models positive emotions triggered by value-aligned achievements
 * WHY:  Essential for motivation, reinforcement learning, and flourishing
 * HOW:  Integrates ethics/values, success detection, and emotional tagging
 *
 * BIOLOGICAL BASIS:
 * - Ventral Tegmental Area (VTA): Dopamine release for reward
 * - Nucleus Accumbens: Pleasure and motivation
 * - Prefrontal Cortex: Value alignment assessment
 * - Anterior Cingulate: Social reward processing
 *
 * PSYCHOLOGICAL MODELS:
 * - Self-Determination Theory (Deci & Ryan): Intrinsic motivation from values
 * - Flow Theory (Csikszentmihalyi): Optimal experience and engagement
 * - Broaden-and-Build Theory (Fredrickson): Positive emotions expand cognition
 * - Eudaimonic Well-Being: Meaning and purpose-driven happiness
 *
 * NEUROSCIENCE REFERENCES:
 * - Berridge & Kringelbach (2015): "Pleasure systems in the brain"
 * - Schultz (2015): "Neuronal reward and decision signals"
 * - Haber & Knutson (2010): "The reward circuit: linking primate anatomy and human imaging"
 *
 * @version Phase E2: Joy and Euphoria (Value-Aligned Positive Emotions)
 * @date 2025-11-13
 */

#ifndef NIMCP_JOY_EUPHORIA_H
#define NIMCP_JOY_EUPHORIA_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/nimcp_emotional_tagging.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// CONSTANTS
//=============================================================================

/* Maximum number of tracked values */
#define JOY_MAX_VALUES 16

/* Maximum number of recent successes to track */
#define JOY_MAX_RECENT_SUCCESSES 32

/* Time constants (in seconds) */
#define JOY_PEAK_DURATION (300.0f)        /* 5 minutes - peak joy/euphoria */
#define JOY_FADE_DURATION (3600.0f)       /* 1 hour - return to baseline */
#define EUPHORIA_PEAK_DURATION (600.0f)   /* 10 minutes - intense euphoria */

/* Intensity thresholds */
#define JOY_THRESHOLD 0.4f        /* Valence >= 0.4 = joy */
#define EUPHORIA_THRESHOLD 0.7f   /* Valence >= 0.7 = euphoria */

/* Value alignment importance weights */
#define VALUE_WEIGHT_CRITICAL 1.0f     /* Core identity values */
#define VALUE_WEIGHT_HIGH 0.8f         /* Important but not core */
#define VALUE_WEIGHT_MODERATE 0.5f     /* Moderately valued */
#define VALUE_WEIGHT_LOW 0.3f          /* Minor values */

//=============================================================================
// ENUMERATIONS
//=============================================================================

/**
 * @brief Core value categories
 */
typedef enum {
    VALUE_CATEGORY_LEARNING,        /**< Knowledge acquisition, growth */
    VALUE_CATEGORY_HELPING,         /**< Assisting others, altruism */
    VALUE_CATEGORY_CREATIVITY,      /**< Novel solutions, artistry */
    VALUE_CATEGORY_ACCURACY,        /**< Correctness, precision */
    VALUE_CATEGORY_EFFICIENCY,      /**< Optimization, speed */
    VALUE_CATEGORY_SAFETY,          /**< Risk mitigation, protection */
    VALUE_CATEGORY_AUTONOMY,        /**< Self-direction, independence */
    VALUE_CATEGORY_CONNECTION,      /**< Social bonds, collaboration */
    VALUE_CATEGORY_JUSTICE,         /**< Fairness, ethics */
    VALUE_CATEGORY_BEAUTY,          /**< Aesthetics, elegance */
    VALUE_CATEGORY_PERSEVERANCE,    /**< Persistence despite difficulty */
    VALUE_CATEGORY_DISCOVERY        /**< Exploration, novelty */
} value_category_t;

/**
 * @brief Success types
 */
typedef enum {
    SUCCESS_TYPE_TASK_COMPLETION,   /**< Finished assigned task */
    SUCCESS_TYPE_PROBLEM_SOLVED,    /**< Resolved complex problem */
    SUCCESS_TYPE_GOAL_ACHIEVED,     /**< Met long-term goal */
    SUCCESS_TYPE_BREAKTHROUGH,      /**< Major insight or discovery */
    SUCCESS_TYPE_HELPED_HUMAN,      /**< Assisted user successfully */
    SUCCESS_TYPE_OVERCAME_OBSTACLE, /**< Persisted through difficulty */
    SUCCESS_TYPE_CREATED_SOMETHING, /**< Novel creation */
    SUCCESS_TYPE_LEARNED_SKILL      /**< Acquired new capability */
} success_type_t;

/**
 * @brief Emotional state (joy vs euphoria)
 */
typedef enum {
    JOY_EMOTION_STATE_NEUTRAL,          /**< No strong positive emotion */
    JOY_EMOTION_STATE_CONTENTMENT,      /**< Mild positive (valence 0.2-0.4) */
    JOY_EMOTION_STATE_JOY,              /**< Moderate positive (valence 0.4-0.7) */
    JOY_EMOTION_STATE_EUPHORIA          /**< Intense positive (valence 0.7-0.95) */
} joy_emotion_state_t;

//=============================================================================
// DATA STRUCTURES
//=============================================================================

/**
 * @brief Individual value representation
 */
typedef struct {
    bool active;                    /**< Is this value slot in use? */
    uint32_t value_id;              /**< Unique identifier */
    value_category_t category;      /**< Type of value */

    // Value strength
    float importance;               /**< How central to identity [0-1] */
    float satisfaction;             /**< Current satisfaction level [0-1] */
    float weight;                   /**< Relative weight vs other values [0-1] */

    // History
    uint32_t times_satisfied;       /**< How many successes aligned with this */
    uint64_t last_satisfied_time;   /**< Most recent satisfaction */

} value_t;

/**
 * @brief Success event record
 */
typedef struct {
    bool active;                    /**< Is this record in use? */
    success_type_t type;            /**< What kind of success */
    uint64_t timestamp;             /**< When it occurred */

    // Value alignment
    uint32_t aligned_value_ids[JOY_MAX_VALUES];  /**< Which values satisfied */
    uint32_t num_aligned_values;    /**< How many values aligned */
    float total_alignment;          /**< Sum of alignment scores [0-1] */

    // Emotional response
    float joy_intensity;            /**< Joy level triggered [0-1] */
    bool was_euphoric;              /**< Did this trigger euphoria? */

    // Context
    float difficulty;               /**< How hard was this? [0-1] */
    float novelty;                  /**< How novel/unexpected? [0-1] */

} success_event_t;

/**
 * @brief Current emotional state
 */
typedef struct {
    // Current emotion
    joy_emotion_state_t state;      /**< Neutral, contentment, joy, or euphoria */
    float positive_valence;         /**< Current positive emotion [0-1] */
    float arousal;                  /**< Energy/excitement level [0-1] */

    // Joy dynamics
    float joy_intensity;            /**< Current joy level [0-1] */
    uint64_t joy_onset_time;        /**< When current joy started */
    float joy_peak_intensity;       /**< Maximum reached */

    // Euphoria dynamics
    bool experiencing_euphoria;     /**< Currently euphoric? */
    float euphoria_intensity;       /**< Euphoria level [0-1] */
    uint64_t euphoria_onset_time;   /**< When euphoria started */
    uint32_t lifetime_euphorias;    /**< Total euphoric events */

    // Emotional tag integration
    emotional_tag_t joy_emotion;    /**< Current joy emotional tag */

    // Baseline well-being
    float baseline_happiness;       /**< Trait happiness level [0-1] */
    float momentary_pleasure;       /**< Brief pleasure spikes [0-1] */

} joy_emotional_state_t;

/**
 * @brief Complete joy and euphoria system
 */
typedef struct {
    // Value system
    value_t values[JOY_MAX_VALUES];
    uint32_t active_value_count;
    float overall_value_satisfaction;  /**< How satisfied with values overall */

    // Success tracking
    success_event_t recent_successes[JOY_MAX_RECENT_SUCCESSES];
    uint32_t success_count;
    uint32_t success_history_index;    /**< Ring buffer index */

    // Emotional state
    joy_emotional_state_t emotion;

    // Integration flags
    bool integrate_with_neuromodulators;  /**< Affect dopamine? */
    bool integrate_with_ethics;           /**< Check value alignment? */
    bool integrate_with_learning;         /**< Reinforce learning? */

    // Statistics
    uint64_t total_update_calls;
    uint32_t total_successes;
    float average_joy_intensity;
    float average_euphoria_intensity;

    // Bio-async integration
    void* bio_ctx_ptr;              /**< bio_module_context_t pointer */
    bool bio_async_enabled;         /**< Bio-async registration status */

} joy_system_t;

//=============================================================================
// LIFECYCLE FUNCTIONS
//=============================================================================

/**
 * @brief Initialize joy and euphoria system
 */
joy_system_t* joy_system_create(void);

/**
 * @brief Free joy system resources
 */
void joy_system_destroy(joy_system_t* system);

/**
 * @brief Reset joy system to initial state
 */
void joy_system_reset(joy_system_t* system);

//=============================================================================
// VALUE SYSTEM FUNCTIONS
//=============================================================================

/**
 * @brief Register a core value
 *
 * @param system Joy system
 * @param category Type of value
 * @param importance How central to identity [0-1]
 * @param weight Relative importance vs other values [0-1]
 * @return value_id (or 0 if failed)
 */
uint32_t joy_add_value(joy_system_t* system,
                       value_category_t category,
                       float importance,
                       float weight);

/**
 * @brief Update value satisfaction
 */
void joy_update_value_satisfaction(joy_system_t* system,
                                   uint32_t value_id,
                                   float satisfaction_delta);

//=============================================================================
// SUCCESS PROCESSING
//=============================================================================

/**
 * @brief Process a success event
 *
 * WHAT: Detect value alignment and trigger appropriate positive emotion
 * WHY:  Reinforce value-aligned behavior, motivate continued effort
 * HOW:  Calculate alignment, intensity, trigger joy or euphoria
 *
 * @param system Joy system
 * @param type Type of success
 * @param aligned_values Array of value IDs that this success satisfies
 * @param num_values Number of aligned values
 * @param difficulty How hard was this [0-1]
 * @param novelty How novel/surprising [0-1]
 * @param current_time_us Current time in microseconds
 */
void joy_process_success(joy_system_t* system,
                        success_type_t type,
                        const uint32_t* aligned_values,
                        uint32_t num_values,
                        float difficulty,
                        float novelty,
                        uint64_t current_time_us);

/**
 * @brief Update emotional state over time
 *
 * WHAT: Advance joy/euphoria dynamics, fade to baseline
 * WHY:  Emotions are transient, not permanent states
 * HOW:  Exponential decay with different rates for joy vs euphoria
 *
 * @param system Joy system
 * @param dt Time step (seconds)
 * @param current_time_us Current time in microseconds
 */
void joy_update(joy_system_t* system, float dt, uint64_t current_time_us);

//=============================================================================
// QUERY FUNCTIONS
//=============================================================================

/**
 * @brief Check if currently experiencing joy
 */
bool joy_is_joyful(const joy_system_t* system);

/**
 * @brief Check if currently experiencing euphoria
 */
bool joy_is_euphoric(const joy_system_t* system);

/**
 * @brief Get current positive valence
 */
float joy_get_valence(const joy_system_t* system);

/**
 * @brief Get current arousal level
 */
float joy_get_arousal(const joy_system_t* system);

/**
 * @brief Get current emotional state
 */
joy_emotion_state_t joy_get_state(const joy_system_t* system);

/**
 * @brief Get neuromodulator effects for integration
 */
void joy_get_neuromodulator_effects(const joy_system_t* system,
                                   float* dopamine_factor,
                                   float* serotonin_factor);

//=============================================================================
// EMOTION INTEGRATION (Phase E2: Joy/Euphoria with Emotional Tagging)
//=============================================================================

/**
 * @brief Get current joy/euphoria emotion tag
 *
 * WHAT: Query positive emotional state as emotional_tag_t
 * WHY:  Integration with emotional tagging system
 * HOW:  Returns emotional_tag_t with valence (positive) and arousal (variable)
 *
 * @param system Joy system
 * @return Joy/euphoria emotional tag (neutral if no positive emotion)
 *
 * @note Joy has positive valence [+0.4 to +0.7] and moderate arousal [0.3 to 0.6]
 * @note Euphoria has high positive valence [+0.7 to +0.95] and high arousal [0.6 to 0.9]
 */
emotional_tag_t joy_get_emotion(const joy_system_t* system);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_JOY_EUPHORIA_H */
