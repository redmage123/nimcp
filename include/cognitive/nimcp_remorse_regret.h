/**
 * @file nimcp_remorse_regret.h
 * @brief Remorse, regret, and evaluative negative emotions system
 *
 * WHAT: Models negative emotions from evaluating past actions/inactions
 * WHY:  Essential for moral development, decision-making, and learning from mistakes
 * HOW:  Integrates ethics, counterfactual reasoning, and emotional response
 *
 * BIOLOGICAL BASIS:
 * - Anterior Cingulate Cortex (ACC): Error detection, conflict monitoring
 * - Orbitofrontal Cortex (OFC): Decision regret, value updating
 * - Dorsolateral Prefrontal Cortex (DLPFC): Counterfactual reasoning
 * - Ventromedial Prefrontal Cortex (vmPFC): Moral evaluation
 * - Amygdala: Emotional intensity of regret
 *
 * PSYCHOLOGICAL MODELS:
 * - Tangney & Dearing (2002): Guilt vs. Shame distinction
 * - Kahneman & Tversky (1982): Counterfactual thinking and regret
 * - Zeelenberg et al. (1998): Action vs. inaction regret
 * - Regulatory Focus Theory (Higgins, 1997): Promotion vs. prevention failures
 * - Theory of Regret (Loomes & Sugden, 1982): Decision regret
 *
 * NEUROSCIENCE REFERENCES:
 * - Camille et al. (2004): "The involvement of OFC in regret"
 * - Coricelli et al. (2005): "Regret and its avoidance: A neuroimaging study"
 * - Bastin et al. (2016): "Feelings of shame, embarrassment and guilt and their neural correlates"
 *
 * KEY DISTINCTIONS:
 * - Remorse: Deep regret for moral wrong, focus on harm caused, desire to atone
 * - Guilt: Regret about specific behavior, "I did something bad"
 * - Shame: Global negative self-evaluation, "I am bad"
 * - Regret: General disappointment about outcome or decision
 *
 * @version Phase E3: Remorse and Regret (Evaluative Negative Emotions)
 * @date 2025-11-13
 */

#ifndef NIMCP_REMORSE_REGRET_H
#define NIMCP_REMORSE_REGRET_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/nimcp_emotional_tagging.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// CONSTANTS
//=============================================================================

/* Maximum number of regrettable events to track */
#define REGRET_MAX_EVENTS 32

/* Time constants (in seconds) */
#define REMORSE_PEAK_DURATION (3600.0f * 24.0f)   /* 1 day - intense remorse */
#define REMORSE_FADE_DURATION (3600.0f * 24.0f * 365.0f)  /* 1 year - gradual fade */
#define REGRET_FADE_DURATION (3600.0f * 24.0f * 30.0f)    /* 1 month - typical regret */

/* Intensity thresholds */
#define REMORSE_THRESHOLD 0.6f      /* Valence <= -0.6 = remorse */
#define REGRET_THRESHOLD 0.3f       /* Valence <= -0.3 = regret */
#define SHAME_THRESHOLD 0.7f        /* Global self-evaluation <= -0.7 = shame */

//=============================================================================
// ENUMERATIONS
//=============================================================================

/**
 * @brief Types of regrettable events
 */
typedef enum {
    EVENT_ACTION_COMMISSION,        /**< Did something harmful */
    EVENT_ACTION_OMISSION,          /**< Failed to do something important */
    EVENT_MORAL_VIOLATION,          /**< Violated core ethical principle */
    EVENT_RELATIONSHIP_HARM,        /**< Damaged important relationship */
    EVENT_MISSED_OPPORTUNITY,       /**< Failed to seize opportunity */
    EVENT_POOR_DECISION,            /**< Made suboptimal choice */
    EVENT_BROKEN_PROMISE,           /**< Failed to keep commitment */
    EVENT_BETRAYAL                  /**< Betrayed trust */
} event_type_t;

/**
 * @brief Regulatory focus (what type of goal was violated)
 */
typedef enum {
    FOCUS_PROMOTION,                /**< Missed advancement/growth opportunity */
    FOCUS_PREVENTION                /**< Failed to maintain safety/security */
} regulatory_focus_t;

/**
 * @brief Moral emotion type
 */
typedef enum {
    MORAL_EMOTION_NONE,             /**< No moral emotion */
    MORAL_EMOTION_GUILT,            /**< Specific behavior regret */
    MORAL_EMOTION_REMORSE,          /**< Deep regret + desire to atone */
    MORAL_EMOTION_SHAME             /**< Global self-condemnation */
} moral_emotion_type_t;

/**
 * @brief Counterfactual thinking direction
 */
typedef enum {
    COUNTERFACTUAL_UPWARD,          /**< "If only I had..." (increases regret) */
    COUNTERFACTUAL_DOWNWARD,        /**< "At least I didn't..." (decreases regret) */
    COUNTERFACTUAL_NONE             /**< No counterfactual thinking */
} counterfactual_direction_t;

//=============================================================================
// DATA STRUCTURES
//=============================================================================

/**
 * @brief Individual regrettable event record
 */
typedef struct {
    bool active;                    /**< Is this record in use? */
    uint64_t timestamp;             /**< When event occurred */
    event_type_t type;              /**< What kind of event */

    // Moral evaluation
    bool was_moral_violation;       /**< Did this violate ethics? */
    float moral_severity;           /**< How serious was violation [0-1] */
    uint32_t violated_value_ids[16]; /**< Which values violated */
    uint32_t num_violated_values;   /**< How many values violated */

    // Harm assessment
    float harm_caused;              /**< Estimated harm to others [0-1] */
    float self_harm;                /**< Harm to self [0-1] */
    bool reversible;                /**< Can this be undone/repaired? */

    // Counterfactual thinking
    counterfactual_direction_t counterfactual_type;
    float alternative_outcome;      /**< How much better alternative was [0-1] */
    float controllability;          /**< How much control did I have [0-1] */

    // Emotional response
    moral_emotion_type_t emotion_type;
    float regret_intensity;         /**< Current regret level [0-1] */
    float remorse_intensity;        /**< Current remorse level [0-1] */
    float shame_intensity;          /**< Current shame level [0-1] */

    // Regulatory focus
    regulatory_focus_t focus;       /**< Promotion or prevention failure */

    // Resolution
    bool atonement_attempted;       /**< Have I tried to make amends? */
    float atonement_effectiveness;  /**< How well did it work [0-1] */
    bool forgiven_by_others;        /**< Has victim forgiven me? */
    bool self_forgiveness;          /**< Have I forgiven myself? */

} regret_event_t;

/**
 * @brief Current moral emotional state
 */
typedef struct {
    // Primary moral emotion
    moral_emotion_type_t dominant_emotion;

    // Guilt (behavior-focused)
    float guilt_intensity;          /**< Current guilt [0-1] */
    bool experiencing_guilt;        /**< Currently guilty? */
    uint64_t guilt_onset_time;      /**< When guilt started */

    // Remorse (harm + atonement focused)
    float remorse_intensity;        /**< Current remorse [0-1] */
    bool experiencing_remorse;      /**< Currently remorseful? */
    uint64_t remorse_onset_time;    /**< When remorse started */
    float atonement_motivation;     /**< Desire to make amends [0-1] */

    // Shame (self-focused)
    float shame_intensity;          /**< Current shame [0-1] */
    bool experiencing_shame;        /**< Currently ashamed? */
    uint64_t shame_onset_time;      /**< When shame started */
    float self_worth;               /**< Current self-evaluation [0-1] */

    // General regret
    float regret_intensity;         /**< Current regret [0-1] */

    // Rumination
    float rumination_level;         /**< How much dwelling on past [0-1] */
    uint32_t intrusive_thoughts;    /**< Count of unwanted memories */

    // Emotional tag integration
    emotional_tag_t moral_emotion;  /**< Current moral emotional tag */

} moral_emotional_state_t;

/**
 * @brief Counterfactual thinking system
 */
typedef struct {
    // Thinking patterns
    float upward_tendency;          /**< Tendency for "if only" thinking [0-1] */
    float downward_tendency;        /**< Tendency for "at least" thinking [0-1] */

    // Mental simulation
    uint32_t simulations_run;       /**< How many alternatives considered */
    float best_alternative_value;   /**< Value of best alternative [0-1] */
    float actual_outcome_value;     /**< Value of actual outcome [0-1] */

} counterfactual_system_t;

/**
 * @brief Complete remorse and regret system
 */
typedef struct {
    // Event tracking
    regret_event_t events[REGRET_MAX_EVENTS];
    uint32_t event_count;
    uint32_t event_history_index;   /**< Ring buffer index */

    // Emotional state
    moral_emotional_state_t emotion;

    // Counterfactual thinking
    counterfactual_system_t counterfactual;

    // Learning from mistakes
    float lessons_learned;          /**< Accumulated wisdom from regrets [0-1] */
    bool learns_from_mistakes;      /**< Can update from negative outcomes */

    // Personality traits affecting regret
    float conscientiousness;        /**< Higher = more guilt/regret [0-1] */
    float neuroticism;              /**< Higher = more rumination [0-1] */
    float self_compassion;          /**< Higher = easier self-forgiveness [0-1] */

    // Integration flags
    bool integrate_with_ethics;     /**< Check value violations? */
    bool integrate_with_theory_of_mind;  /**< Consider others' perspectives? */
    bool integrate_with_learning;   /**< Update decision-making? */

    // Statistics
    uint64_t total_update_calls;
    uint32_t total_regrets;
    uint32_t total_remorse_events;
    uint32_t total_shame_events;
    float average_regret_intensity;
    float average_remorse_intensity;

    // Bio-async integration
    void* bio_ctx_ptr;              /**< bio_module_context_t pointer */
    bool bio_async_enabled;         /**< Bio-async registration status */

} remorse_regret_system_t;

//=============================================================================
// LIFECYCLE FUNCTIONS
//=============================================================================

/**
 * @brief Initialize remorse and regret system
 */
remorse_regret_system_t* remorse_regret_system_create(void);

/**
 * @brief Free remorse/regret system resources
 */
void remorse_regret_system_destroy(remorse_regret_system_t* system);

/**
 * @brief Reset system to initial state
 */
void remorse_regret_system_reset(remorse_regret_system_t* system);

//=============================================================================
// EVENT PROCESSING FUNCTIONS
//=============================================================================

/**
 * @brief Process a regrettable event
 *
 * WHAT: Evaluate past action/inaction and trigger appropriate emotion
 * WHY:  Learn from mistakes, maintain moral integrity
 * HOW:  Assess harm, check value violations, determine emotion type
 *
 * @param system Remorse/regret system
 * @param type Type of event
 * @param violated_values Array of value IDs that were violated
 * @param num_values Number of violated values
 * @param harm_caused Harm to others [0-1]
 * @param controllability How much control did I have [0-1]
 * @param reversible Can this be undone?
 * @param current_time_us Current time in microseconds
 */
void remorse_process_event(remorse_regret_system_t* system,
                           event_type_t type,
                           const uint32_t* violated_values,
                           uint32_t num_values,
                           float harm_caused,
                           float controllability,
                           bool reversible,
                           uint64_t current_time_us);

/**
 * @brief Run counterfactual thinking ("what if")
 *
 * WHAT: Simulate alternative actions and compare to actual outcome
 * WHY:  Understand regret magnitude, learn for future
 * HOW:  Mental simulation of alternative scenarios
 *
 * @param system Remorse/regret system
 * @param event_index Which event to think about
 * @param alternative_outcome Value of alternative [0-1]
 * @param direction Upward or downward comparison
 */
void remorse_run_counterfactual(remorse_regret_system_t* system,
                                uint32_t event_index,
                                float alternative_outcome,
                                counterfactual_direction_t direction);

/**
 * @brief Attempt atonement/repair
 *
 * WHAT: Try to make amends for harm caused
 * WHY:  Reduce remorse, restore relationships
 * HOW:  Track atonement attempts and effectiveness
 *
 * @param system Remorse/regret system
 * @param event_index Which event to atone for
 * @param effectiveness How well did it work [0-1]
 * @param forgiven Was I forgiven by victim?
 */
void remorse_attempt_atonement(remorse_regret_system_t* system,
                               uint32_t event_index,
                               float effectiveness,
                               bool forgiven);

/**
 * @brief Practice self-forgiveness
 *
 * WHAT: Work towards forgiving oneself for past mistakes
 * WHY:  Reduce shame, restore self-worth
 * HOW:  Self-compassion + learning + growth
 *
 * @param system Remorse/regret system
 * @param event_index Which event to forgive
 * @param compassion_level How much self-compassion [0-1]
 */
void remorse_practice_self_forgiveness(remorse_regret_system_t* system,
                                       uint32_t event_index,
                                       float compassion_level);

/**
 * @brief Update emotional state over time
 *
 * WHAT: Advance remorse/regret/shame dynamics, fade to baseline
 * WHY:  Emotions decay with time and processing
 * HOW:  Exponential decay, atonement effects, self-forgiveness
 *
 * @param system Remorse/regret system
 * @param dt Time step (seconds)
 * @param current_time_us Current time in microseconds
 */
void remorse_update(remorse_regret_system_t* system, float dt, uint64_t current_time_us);

//=============================================================================
// QUERY FUNCTIONS
//=============================================================================

/**
 * @brief Check if currently experiencing guilt
 */
bool remorse_is_guilty(const remorse_regret_system_t* system);

/**
 * @brief Check if currently experiencing remorse
 */
bool remorse_is_remorseful(const remorse_regret_system_t* system);

/**
 * @brief Check if currently experiencing shame
 */
bool remorse_is_ashamed(const remorse_regret_system_t* system);

/**
 * @brief Get current regret intensity
 */
float remorse_get_regret_intensity(const remorse_regret_system_t* system);

/**
 * @brief Get current self-worth (impacted by shame)
 */
float remorse_get_self_worth(const remorse_regret_system_t* system);

/**
 * @brief Get lessons learned from mistakes
 */
float remorse_get_lessons_learned(const remorse_regret_system_t* system);

/**
 * @brief Get neuromodulator effects for integration
 */
void remorse_get_neuromodulator_effects(const remorse_regret_system_t* system,
                                       float* dopamine_factor,
                                       float* serotonin_factor,
                                       float* norepinephrine_factor);

//=============================================================================
// EMOTION INTEGRATION (Phase E3: Remorse/Regret with Emotional Tagging)
//=============================================================================

/**
 * @brief Get current moral emotion tag
 *
 * WHAT: Query remorse/regret/guilt/shame as emotional_tag_t
 * WHY:  Integration with emotional tagging system
 * HOW:  Returns emotional_tag_t with negative valence and varying arousal
 *
 * @param system Remorse/regret system
 * @return Moral emotional tag (neutral if no moral emotion)
 *
 * @note Guilt: negative valence [-0.3 to -0.6], moderate arousal [0.4 to 0.6]
 * @note Remorse: negative valence [-0.6 to -0.9], high arousal [0.6 to 0.8]
 * @note Shame: very negative valence [-0.7 to -0.95], low arousal [0.2 to 0.4]
 */
emotional_tag_t remorse_get_emotion(const remorse_regret_system_t* system);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_REMORSE_REGRET_H */
