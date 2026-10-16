/**
 * @file nimcp_remorse_regret.c
 * @brief Implementation of remorse, regret, and evaluative negative emotions
 *
 * WHAT: Models guilt, remorse, shame, and regret from evaluating past actions
 * WHY:  Essential for moral development, learning from mistakes, decision-making
 * HOW:  Tracks regrettable events, runs counterfactual thinking, enables atonement
 *
 * @version Phase E3: Remorse and Regret System
 * @date 2025-11-13
 */

#include "cognitive/nimcp_remorse_regret.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//=============================================================================
// HELPER FUNCTIONS
//=============================================================================

/**
 * @brief Calculate emotion intensity based on harm and controllability
 */
static float calculate_emotion_intensity(float harm, float controllability, float moral_severity) {
    // WHAT: Compute how intense the emotional response should be
    // WHY:  Greater harm + more control = stronger emotion
    // HOW:  Weighted combination of factors

    float base = harm * 0.5f + controllability * 0.3f + moral_severity * 0.2f;
    return fminf(1.0f, fmaxf(0.0f, base));
}

/**
 * @brief Determine moral emotion type from event characteristics
 */
static moral_emotion_type_t determine_emotion_type(
    bool moral_violation,
    float harm_caused,
    float moral_severity) {

    // WHAT: Decide if this should trigger guilt, remorse, or shame
    // WHY:  Different events elicit different moral emotions
    // HOW:  Use severity thresholds and violation type

    if (!moral_violation && harm_caused < 0.3f) {
        return MORAL_EMOTION_NONE;
    }

    if (moral_severity >= SHAME_THRESHOLD) {
        return MORAL_EMOTION_SHAME;  // Severe violation = shame
    }

    if (harm_caused >= REMORSE_THRESHOLD || moral_violation) {
        return MORAL_EMOTION_REMORSE;  // Harm/violation = remorse
    }

    return MORAL_EMOTION_GUILT;  // Default for lesser issues
}

//=============================================================================
// LIFECYCLE FUNCTIONS
//=============================================================================

remorse_regret_system_t* remorse_regret_system_create(void) {
    // WHAT: Allocate and initialize remorse/regret system
    // WHY:  Required for tracking moral emotions and learning from mistakes
    // HOW:  Allocate memory, zero-initialize, set defaults

    remorse_regret_system_t* system = (remorse_regret_system_t*)calloc(1, sizeof(remorse_regret_system_t));
    if (!system) return NULL;

    // Set personality defaults (neutral values)
    system->conscientiousness = 0.5f;
    system->neuroticism = 0.5f;
    system->self_compassion = 0.5f;

    // Enable integration by default
    system->integrate_with_ethics = true;
    system->integrate_with_theory_of_mind = true;
    system->integrate_with_learning = true;
    system->learns_from_mistakes = true;

    // Initialize emotional state
    system->emotion.self_worth = 0.7f;  // Start with moderate self-worth

    return system;
}

void remorse_regret_system_destroy(remorse_regret_system_t* system) {
    // WHAT: Free remorse/regret system resources
    // WHY:  Prevent memory leaks
    // HOW:  Simple free (no sub-allocations)

    if (!system) return;
    free(system);
}

void remorse_regret_system_reset(remorse_regret_system_t* system) {
    // WHAT: Reset system to initial state
    // WHY:  Clear event history while preserving personality traits
    // HOW:  Zero out events and stats, preserve configuration

    if (!system) return;

    // Save personality traits
    float conscientiousness = system->conscientiousness;
    float neuroticism = system->neuroticism;
    float self_compassion = system->self_compassion;
    bool integrate_ethics = system->integrate_with_ethics;
    bool integrate_tom = system->integrate_with_theory_of_mind;
    bool integrate_learn = system->integrate_with_learning;

    // Clear everything
    memset(system->events, 0, sizeof(system->events));
    memset(&system->emotion, 0, sizeof(system->emotion));
    memset(&system->counterfactual, 0, sizeof(system->counterfactual));
    system->event_count = 0;
    system->event_history_index = 0;
    system->lessons_learned = 0.0f;
    system->total_update_calls = 0;
    system->total_regrets = 0;
    system->total_remorse_events = 0;
    system->total_shame_events = 0;

    // Restore personality
    system->conscientiousness = conscientiousness;
    system->neuroticism = neuroticism;
    system->self_compassion = self_compassion;
    system->integrate_with_ethics = integrate_ethics;
    system->integrate_with_theory_of_mind = integrate_tom;
    system->integrate_with_learning = integrate_learn;
    system->learns_from_mistakes = true;
    system->emotion.self_worth = 0.7f;
}

//=============================================================================
// EVENT PROCESSING FUNCTIONS
//=============================================================================

void remorse_process_event(
    remorse_regret_system_t* system,
    event_type_t type,
    const uint32_t* violated_values,
    uint32_t num_values,
    float harm_caused,
    float controllability,
    bool reversible,
    uint64_t current_time_us) {

    // WHAT: Process a regrettable event and trigger appropriate emotion
    // WHY:  Learn from mistakes, maintain moral integrity
    // HOW:  Assess harm, check violations, determine emotion type

    if (!system) return;

    // Find or create event slot (ring buffer)
    uint32_t idx = system->event_history_index;
    regret_event_t* event = &system->events[idx];

    // Initialize event record
    event->active = true;
    event->timestamp = current_time_us;
    event->type = type;
    event->harm_caused = harm_caused;
    event->controllability = controllability;
    event->reversible = reversible;

    // Check for moral violation
    event->was_moral_violation = (num_values > 0);
    event->moral_severity = (num_values > 0) ? harm_caused : 0.0f;
    event->num_violated_values = (num_values > 16) ? 16 : num_values;
    if (violated_values && num_values > 0) {
        memcpy(event->violated_value_ids, violated_values,
               event->num_violated_values * sizeof(uint32_t));
    }

    // Determine emotion type
    event->emotion_type = determine_emotion_type(
        event->was_moral_violation, harm_caused, event->moral_severity);

    // Calculate emotion intensities
    float base_intensity = calculate_emotion_intensity(
        harm_caused, controllability, event->moral_severity);

    // Modulate by personality
    base_intensity *= (0.5f + system->conscientiousness * 0.5f);

    switch (event->emotion_type) {
        case MORAL_EMOTION_GUILT:
            event->regret_intensity = base_intensity * 0.6f;
            event->remorse_intensity = 0.0f;
            event->shame_intensity = 0.0f;
            system->total_regrets++;
            break;

        case MORAL_EMOTION_REMORSE:
            event->regret_intensity = base_intensity * 0.5f;
            event->remorse_intensity = base_intensity;
            event->shame_intensity = 0.0f;
            system->total_remorse_events++;
            break;

        case MORAL_EMOTION_SHAME:
            event->regret_intensity = base_intensity * 0.3f;
            event->remorse_intensity = base_intensity * 0.7f;
            event->shame_intensity = base_intensity;
            system->total_shame_events++;
            // Shame impacts self-worth
            system->emotion.self_worth *= (1.0f - base_intensity * 0.3f);
            break;

        default:
            event->regret_intensity = base_intensity * 0.3f;
            event->remorse_intensity = 0.0f;
            event->shame_intensity = 0.0f;
            break;
    }

    // Update current emotional state
    system->emotion.dominant_emotion = event->emotion_type;
    system->emotion.guilt_intensity = fmaxf(system->emotion.guilt_intensity,
                                            event->regret_intensity);
    system->emotion.remorse_intensity = fmaxf(system->emotion.remorse_intensity,
                                              event->remorse_intensity);
    system->emotion.shame_intensity = fmaxf(system->emotion.shame_intensity,
                                            event->shame_intensity);
    system->emotion.regret_intensity = fmaxf(system->emotion.regret_intensity,
                                             event->regret_intensity);

    // Set flags
    system->emotion.experiencing_guilt = (event->emotion_type == MORAL_EMOTION_GUILT);
    system->emotion.experiencing_remorse = (event->emotion_type == MORAL_EMOTION_REMORSE);
    system->emotion.experiencing_shame = (event->emotion_type == MORAL_EMOTION_SHAME);

    if (system->emotion.experiencing_remorse) {
        system->emotion.remorse_onset_time = current_time_us;
        system->emotion.atonement_motivation = event->remorse_intensity * 0.8f;
    }

    // Advance ring buffer
    system->event_history_index = (system->event_history_index + 1) % REGRET_MAX_EVENTS;
    if (system->event_count < REGRET_MAX_EVENTS) {
        system->event_count++;
    }
}

void remorse_run_counterfactual(
    remorse_regret_system_t* system,
    uint32_t event_index,
    float alternative_outcome,
    counterfactual_direction_t direction) {

    // WHAT: Run "what if" simulation on past event
    // WHY:  Understand regret magnitude, learn for future decisions
    // HOW:  Compare alternative to actual, adjust regret intensity

    if (!system || event_index >= REGRET_MAX_EVENTS) return;
    if (!system->events[event_index].active) return;

    regret_event_t* event = &system->events[event_index];
    event->counterfactual_type = direction;
    event->alternative_outcome = alternative_outcome;

    // Update counterfactual system
    system->counterfactual.simulations_run++;
    system->counterfactual.actual_outcome_value = 1.0f - event->harm_caused;
    system->counterfactual.best_alternative_value = alternative_outcome;

    // Adjust regret based on direction
    if (direction == COUNTERFACTUAL_UPWARD) {
        // "If only I had..." increases regret
        float regret_boost = (alternative_outcome - system->counterfactual.actual_outcome_value) * 0.5f;
        event->regret_intensity += regret_boost;
        event->regret_intensity = fminf(1.0f, event->regret_intensity);
        system->counterfactual.upward_tendency += 0.1f;
    } else if (direction == COUNTERFACTUAL_DOWNWARD) {
        // "At least I didn't..." decreases regret
        float regret_reduction = (system->counterfactual.actual_outcome_value - alternative_outcome) * 0.3f;
        event->regret_intensity -= regret_reduction;
        event->regret_intensity = fmaxf(0.0f, event->regret_intensity);
        system->counterfactual.downward_tendency += 0.1f;
    }

    // Learn from counterfactual
    if (system->learns_from_mistakes) {
        system->lessons_learned += 0.05f;
        system->lessons_learned = fminf(1.0f, system->lessons_learned);
    }
}

void remorse_attempt_atonement(
    remorse_regret_system_t* system,
    uint32_t event_index,
    float effectiveness,
    bool forgiven) {

    // WHAT: Attempt to make amends for harm caused
    // WHY:  Reduce remorse, restore relationships, moral repair
    // HOW:  Track atonement and reduce emotion intensities

    if (!system || event_index >= REGRET_MAX_EVENTS) return;
    if (!system->events[event_index].active) return;

    regret_event_t* event = &system->events[event_index];
    event->atonement_attempted = true;
    event->atonement_effectiveness = effectiveness;
    event->forgiven_by_others = forgiven;

    // Reduce remorse based on effectiveness
    float reduction = effectiveness * 0.6f;
    if (forgiven) {
        reduction *= 1.5f;  // Forgiveness amplifies reduction
    }

    event->remorse_intensity *= (1.0f - reduction);
    event->regret_intensity *= (1.0f - reduction * 0.8f);

    // Update system emotional state
    system->emotion.remorse_intensity *= (1.0f - reduction * 0.5f);
    system->emotion.guilt_intensity *= (1.0f - reduction * 0.4f);
    system->emotion.atonement_motivation *= (1.0f - effectiveness);

    // Restore self-worth if shame was involved
    if (event->shame_intensity > 0.0f) {
        system->emotion.self_worth += effectiveness * 0.2f;
        system->emotion.self_worth = fminf(1.0f, system->emotion.self_worth);
    }
}

void remorse_practice_self_forgiveness(
    remorse_regret_system_t* system,
    uint32_t event_index,
    float compassion_level) {

    // WHAT: Work towards forgiving oneself
    // WHY:  Reduce shame, restore self-worth, enable growth
    // HOW:  Self-compassion reduces emotional intensity

    if (!system || event_index >= REGRET_MAX_EVENTS) return;
    if (!system->events[event_index].active) return;

    regret_event_t* event = &system->events[event_index];

    // Self-forgiveness requires time and compassion
    float forgiveness_progress = compassion_level * system->self_compassion;
    event->self_forgiveness = (forgiveness_progress > 0.6f);

    if (event->self_forgiveness) {
        // Reduce all negative emotions
        event->shame_intensity *= 0.5f;
        event->remorse_intensity *= 0.7f;
        event->regret_intensity *= 0.8f;

        // Restore self-worth
        system->emotion.self_worth += 0.15f;
        system->emotion.self_worth = fminf(1.0f, system->emotion.self_worth);

        // Update system state
        system->emotion.shame_intensity *= 0.6f;
        system->emotion.experiencing_shame = (system->emotion.shame_intensity > SHAME_THRESHOLD);
    }
}

void remorse_update(remorse_regret_system_t* system, float dt, uint64_t current_time_us) {
    // WHAT: Update emotional state over time
    // WHY:  Emotions naturally decay and fade
    // HOW:  Exponential decay modulated by personality

    if (!system) return;

    // Suppress unused parameter warnings
    (void)dt;
    (void)current_time_us;

    system->total_update_calls++;

    // Decay rate influenced by neuroticism (higher = slower decay)
    float decay_rate = 0.995f - (system->neuroticism * 0.01f);

    // Decay all active events
    for (uint32_t i = 0; i < REGRET_MAX_EVENTS; i++) {
        if (!system->events[i].active) continue;

        regret_event_t* event = &system->events[i];

        // Natural decay
        event->regret_intensity *= decay_rate;
        event->remorse_intensity *= decay_rate;
        event->shame_intensity *= decay_rate;

        // Deactivate if negligible
        if (event->regret_intensity < 0.01f &&
            event->remorse_intensity < 0.01f &&
            event->shame_intensity < 0.01f) {
            event->active = false;
        }
    }

    // Update system emotional state
    system->emotion.guilt_intensity *= decay_rate;
    system->emotion.remorse_intensity *= decay_rate;
    system->emotion.shame_intensity *= decay_rate;
    system->emotion.regret_intensity *= decay_rate;
    system->emotion.rumination_level *= 0.98f;

    // Gradually restore self-worth
    if (system->emotion.self_worth < 0.7f) {
        system->emotion.self_worth += 0.001f * system->self_compassion;
    }

    // Update flags
    system->emotion.experiencing_guilt = (system->emotion.guilt_intensity > REGRET_THRESHOLD);
    system->emotion.experiencing_remorse = (system->emotion.remorse_intensity > REMORSE_THRESHOLD);
    system->emotion.experiencing_shame = (system->emotion.shame_intensity > SHAME_THRESHOLD);

    // Determine dominant emotion
    if (system->emotion.shame_intensity > system->emotion.remorse_intensity &&
        system->emotion.shame_intensity > system->emotion.guilt_intensity) {
        system->emotion.dominant_emotion = MORAL_EMOTION_SHAME;
    } else if (system->emotion.remorse_intensity > system->emotion.guilt_intensity) {
        system->emotion.dominant_emotion = MORAL_EMOTION_REMORSE;
    } else if (system->emotion.guilt_intensity > REGRET_THRESHOLD) {
        system->emotion.dominant_emotion = MORAL_EMOTION_GUILT;
    } else {
        system->emotion.dominant_emotion = MORAL_EMOTION_NONE;
    }
}

//=============================================================================
// QUERY FUNCTIONS
//=============================================================================

bool remorse_is_guilty(const remorse_regret_system_t* system) {
    return system ? system->emotion.experiencing_guilt : false;
}

bool remorse_is_remorseful(const remorse_regret_system_t* system) {
    return system ? system->emotion.experiencing_remorse : false;
}

bool remorse_is_ashamed(const remorse_regret_system_t* system) {
    return system ? system->emotion.experiencing_shame : false;
}

float remorse_get_regret_intensity(const remorse_regret_system_t* system) {
    return system ? system->emotion.regret_intensity : 0.0f;
}

float remorse_get_self_worth(const remorse_regret_system_t* system) {
    return system ? system->emotion.self_worth : 0.7f;
}

float remorse_get_lessons_learned(const remorse_regret_system_t* system) {
    return system ? system->lessons_learned : 0.0f;
}

void remorse_get_neuromodulator_effects(
    const remorse_regret_system_t* system,
    float* dopamine_factor,
    float* serotonin_factor,
    float* norepinephrine_factor) {

    // WHAT: Get neuromodulator modulation from moral emotions
    // WHY:  Integrate with neuromodulator system
    // HOW:  Negative emotions reduce dopamine/serotonin

    if (!system || !dopamine_factor || !serotonin_factor || !norepinephrine_factor) {
        return;
    }

    // Remorse/guilt/shame reduce dopamine (reduced motivation)
    float negative_emotion = (system->emotion.guilt_intensity * 0.3f +
                             system->emotion.remorse_intensity * 0.5f +
                             system->emotion.shame_intensity * 0.7f);

    *dopamine_factor = 1.0f - negative_emotion * 0.4f;
    *serotonin_factor = 1.0f - negative_emotion * 0.5f;  // Lower mood
    *norepinephrine_factor = 1.0f + system->emotion.rumination_level * 0.3f;  // Stress
}

emotional_tag_t remorse_get_emotion(const remorse_regret_system_t* system) {
    // WHAT: Get current moral emotion as emotional_tag_t
    // WHY:  Integration with emotional tagging system
    // HOW:  Map moral emotions to valence/arousal space

    emotional_tag_t tag = {0};
    if (!system) return tag;

    switch (system->emotion.dominant_emotion) {
        case MORAL_EMOTION_GUILT:
            tag.valence = -0.3f - system->emotion.guilt_intensity * 0.3f;
            tag.arousal = 0.4f + system->emotion.guilt_intensity * 0.2f;
            tag.intensity = system->emotion.guilt_intensity;
            break;

        case MORAL_EMOTION_REMORSE:
            tag.valence = -0.6f - system->emotion.remorse_intensity * 0.3f;
            tag.arousal = 0.6f + system->emotion.remorse_intensity * 0.2f;
            tag.intensity = system->emotion.remorse_intensity;
            break;

        case MORAL_EMOTION_SHAME:
            tag.valence = -0.7f - system->emotion.shame_intensity * 0.25f;
            tag.arousal = 0.2f + system->emotion.shame_intensity * 0.2f;  // Withdrawal
            tag.intensity = system->emotion.shame_intensity;
            break;

        default:
            tag.valence = 0.0f;
            tag.arousal = 0.0f;
            tag.intensity = 0.0f;
            break;
    }

    tag.timestamp_ms = system->emotion.remorse_onset_time / 1000;  // Convert us to ms

    return tag;
}
