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
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_wiring_helpers.h"
#include "nimcp.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/memory/nimcp_memory_guards.h"  // For nimcp_calloc/nimcp_free
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "REMORSE"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(remorse_regret, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define BIO_MODULE_REMORSE 0x0325

/*=============================================================================
 * BIO-ASYNC MESSAGE HANDLERS
 *============================================================================*/

/**
 * @brief Handle ethics evaluation response (may trigger remorse)
 */
static nimcp_error_t handle_ethics_response(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    (void)msg_size;
    (void)response_promise;
    NIMCP_CHECK_THROW(msg && user_data, NIMCP_ERROR_NULL_ARG, "msg or user_data is NULL");

    const bio_msg_ethics_response_t* response = (const bio_msg_ethics_response_t*)msg;
    remorse_regret_system_t* system = (remorse_regret_system_t*)user_data;
    (void)system;

    LOG_DEBUG(LOG_MODULE, "Received ethics response: score=%.2f, veto=%d",
              response->ethical_score, response->veto);

    return NIMCP_SUCCESS;
}

/**
 * @brief KG-driven wiring handler callback
 *
 * WHAT: Register message handlers based on discovered wiring from KG
 * WHY:  Enables runtime assembly - module discovers its handlers from KG
 * HOW:  Orchestrator invokes this with message types from HANDLES_MESSAGE relations
 *
 * @param ctx Bio-async module context
 * @param message_types Array of message types to handle (from KG)
 * @param message_count Number of message types
 * @param user_data Module context pointer
 * @return 0 on success, -1 on error
 */
static int remorse_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    if (!ctx || !message_types || message_count == 0) {
        return 0;  /* No handlers to register */
    }

    (void)user_data;

    LOG_INFO(LOG_MODULE,
        "remorse_wiring_handler_callback: registering %u handlers from KG",
        message_count);

    for (uint32_t i = 0; i < message_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && message_count > 256) {
            remorse_regret_heartbeat("remorse_regr_loop",
                             (float)(i + 1) / (float)message_count);
        }

        switch (message_types[i]) {
            case BIO_MSG_ETHICS_EVALUATION_RESPONSE:
                bio_router_register_handler(ctx, message_types[i], handle_ethics_response);
                LOG_DEBUG(LOG_MODULE,
                    "  Registered handler for BIO_MSG_ETHICS_EVALUATION_RESPONSE");
                break;

            default:
                LOG_DEBUG(LOG_MODULE,
                    "  Unknown message type 0x%04X, skipping", message_types[i]);
                break;
        }
    }

    return 0;
}

/**
 * @brief Broadcast remorse/guilt state to other modules
 */
static void bio_broadcast_remorse_state(remorse_regret_system_t* system) {
    if (!system || !system->bio_async_enabled || !system->bio_ctx_ptr) { return; }

    bio_msg_ethics_response_t msg = {0};
    bio_msg_init_header(&msg.header, BIO_MSG_ETHICS_EVALUATION_RESPONSE,
                        bio_module_context_get_id(system->bio_ctx_ptr), 0, sizeof(msg));
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg.action_id = 0;
    msg.ethical_score = 1.0F - system->emotion.remorse_intensity;
    msg.confidence = system->emotion.guilt_intensity;
    msg.veto = system->emotion.experiencing_shame;
    msg.primary_concern = (uint32_t)system->emotion.dominant_emotion;

    bio_router_broadcast(system->bio_ctx_ptr, &msg, sizeof(msg));
    LOG_DEBUG(LOG_MODULE, "Broadcast remorse state: guilt=%.2f, remorse=%.2f, shame=%.2f",
              system->emotion.guilt_intensity, system->emotion.remorse_intensity,
              system->emotion.shame_intensity);
}

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

    float base = harm * 0.5F + controllability * 0.3F + moral_severity * 0.2F;
    return fminf(1.0F, fmaxf(0.0F, base));
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

    if (!moral_violation && harm_caused < 0.3F) {
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

    /* Phase 8: Heartbeat at operation start */
    remorse_regret_heartbeat("remorse_regr_system_create", 0.0f);


    remorse_regret_system_t* system = (remorse_regret_system_t*)nimcp_calloc(1, sizeof(remorse_regret_system_t));
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate system");

        return NULL;

    }

    // Set personality defaults (neutral values)
    system->conscientiousness = 0.5F;
    system->neuroticism = 0.5F;
    system->self_compassion = 0.5F;

    // Enable integration by default
    system->integrate_with_ethics = true;
    system->integrate_with_theory_of_mind = true;
    system->integrate_with_learning = true;
    system->learns_from_mistakes = true;

    // Initialize emotional state
    system->emotion.self_worth = 0.7F;  // Start with moderate self-worth

    
    // Bio-async registration
    system->bio_ctx_ptr = NULL;
    system->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_REMORSE,
            .module_name = "remorse_regret",
            .inbox_capacity = 32,
            .user_data = system
        };
        system->bio_ctx_ptr = bio_router_register_module(&bio_info);
        if (system->bio_ctx_ptr) {
            system->bio_async_enabled = true;

            /* KG-Driven Wiring: Register callback for orchestrator to invoke
             * When orchestrator starts, it discovers HANDLES_MESSAGE relations
             * from the KG and invokes this callback with the message types */
            nimcp_error_t cb_result = bio_router_register_wiring_callback(
                BIO_MODULE_REMORSE,
                (void*)remorse_wiring_handler_callback,
                system
            );

            if (cb_result != NIMCP_SUCCESS) {
                /* Fallback: Direct registration if orchestrator not available
                 * This ensures backward compatibility with non-KG systems */
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(system->bio_ctx_ptr, BIO_MSG_ETHICS_EVALUATION_RESPONSE,
                                                handle_ethics_response)
                );
                LOG_INFO(LOG_MODULE, "Bio-async registered (module_id=0x%04X, legacy)", BIO_MODULE_REMORSE);
            } else {
                LOG_INFO(LOG_MODULE, "Bio-async registered (module_id=0x%04X, KG-driven)", BIO_MODULE_REMORSE);
            }
        }
    }

    return system;
}

void remorse_regret_system_destroy(remorse_regret_system_t* system) {
    // WHAT: Free remorse/regret system resources
    // WHY:  Prevent memory leaks
    // HOW:  Simple nimcp_free(no sub-allocations)

    if (!system) return;
    // Unregister from bio-router
    /* Phase 8: Heartbeat at operation start */
    remorse_regret_heartbeat("remorse_regr_system_destroy", 0.0f);


    if (system->bio_async_enabled && system->bio_ctx_ptr) {
        bio_router_unregister_module(system->bio_ctx_ptr);
        system->bio_ctx_ptr = NULL;
        system->bio_async_enabled = false;
    }

    nimcp_free(system);
    system = NULL;
}

void remorse_regret_system_reset(remorse_regret_system_t* system) {
    // WHAT: Reset system to initial state
    // WHY:  Clear event history while preserving personality traits
    // HOW:  Zero out events and stats, preserve configuration

    if (!system) return;

    // Save personality traits
    /* Phase 8: Heartbeat at operation start */
    remorse_regret_heartbeat("remorse_regr_system_reset", 0.0f);


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
    system->lessons_learned = 0.0F;
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
    system->emotion.self_worth = 0.7F;
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
    /* Phase 8: Heartbeat at operation start */
    remorse_regret_heartbeat("remorse_regr_remorse_process_even", 0.0f);


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
    event->moral_severity = (num_values > 0) ? harm_caused : 0.0F;
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
    base_intensity *= (0.5F + system->conscientiousness * 0.5F);

    switch (event->emotion_type) {
        case MORAL_EMOTION_GUILT:
            event->regret_intensity = base_intensity * 0.6F;
            event->remorse_intensity = 0.0F;
            event->shame_intensity = 0.0F;
            system->total_regrets++;
            break;

        case MORAL_EMOTION_REMORSE:
            event->regret_intensity = base_intensity * 0.5F;
            event->remorse_intensity = base_intensity;
            event->shame_intensity = 0.0F;
            system->total_remorse_events++;
            break;

        case MORAL_EMOTION_SHAME:
            event->regret_intensity = base_intensity * 0.3F;
            event->remorse_intensity = base_intensity * 0.7F;
            event->shame_intensity = base_intensity;
            system->total_shame_events++;
            // Shame impacts self-worth
            system->emotion.self_worth *= (1.0F - base_intensity * 0.3F);
            break;

        default:
            event->regret_intensity = base_intensity * 0.3F;
            event->remorse_intensity = 0.0F;
            event->shame_intensity = 0.0F;
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
        system->emotion.atonement_motivation = event->remorse_intensity * 0.8F;
    }

    // Advance ring buffer
    system->event_history_index = (system->event_history_index + 1) % REGRET_MAX_EVENTS;
    if (system->event_count < REGRET_MAX_EVENTS) {
        system->event_count++;
    }

    /* Broadcast remorse state if significant */
    if (system->emotion.remorse_intensity > 0.3F || system->emotion.guilt_intensity > 0.3F) {
        bio_broadcast_remorse_state(system);
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

    /* Phase 8: Heartbeat at operation start */
    remorse_regret_heartbeat("remorse_regr_remorse_run_counterf", 0.0f);


    regret_event_t* event = &system->events[event_index];
    event->counterfactual_type = direction;
    event->alternative_outcome = alternative_outcome;

    // Update counterfactual system
    system->counterfactual.simulations_run++;
    system->counterfactual.actual_outcome_value = 1.0F - event->harm_caused;
    system->counterfactual.best_alternative_value = alternative_outcome;

    // Adjust regret based on direction
    if (direction == COUNTERFACTUAL_UPWARD) {
        // "If only I had..." increases regret
        float regret_boost = (alternative_outcome - system->counterfactual.actual_outcome_value) * 0.5F;
        event->regret_intensity += regret_boost;
        event->regret_intensity = fminf(1.0F, event->regret_intensity);
        system->counterfactual.upward_tendency += 0.1F;
    } else if (direction == COUNTERFACTUAL_DOWNWARD) {
        // "At least I didn't..." decreases regret
        float regret_reduction = (system->counterfactual.actual_outcome_value - alternative_outcome) * 0.3F;
        event->regret_intensity -= regret_reduction;
        event->regret_intensity = fmaxf(0.0F, event->regret_intensity);
        system->counterfactual.downward_tendency += 0.1F;
    }

    // Learn from counterfactual
    if (system->learns_from_mistakes) {
        system->lessons_learned += 0.05F;
        system->lessons_learned = fminf(1.0F, system->lessons_learned);
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

    /* Phase 8: Heartbeat at operation start */
    remorse_regret_heartbeat("remorse_regr_remorse_attempt_aton", 0.0f);


    regret_event_t* event = &system->events[event_index];
    event->atonement_attempted = true;
    event->atonement_effectiveness = effectiveness;
    event->forgiven_by_others = forgiven;

    // Reduce remorse based on effectiveness
    float reduction = effectiveness * 0.6F;
    if (forgiven) {
        reduction *= 1.5F;  // Forgiveness amplifies reduction
    }

    event->remorse_intensity *= (1.0F - reduction);
    event->regret_intensity *= (1.0F - reduction * 0.8F);

    // Update system emotional state
    system->emotion.remorse_intensity *= (1.0F - reduction * 0.5F);
    system->emotion.guilt_intensity *= (1.0F - reduction * 0.4F);
    system->emotion.atonement_motivation *= (1.0F - effectiveness);

    // Restore self-worth if shame was involved
    if (event->shame_intensity > 0.0F) {
        system->emotion.self_worth += effectiveness * 0.2F;
        system->emotion.self_worth = fminf(1.0F, system->emotion.self_worth);
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

    /* Phase 8: Heartbeat at operation start */
    remorse_regret_heartbeat("remorse_regr_remorse_practice_sel", 0.0f);


    regret_event_t* event = &system->events[event_index];

    // Self-forgiveness requires time and compassion
    float forgiveness_progress = compassion_level * system->self_compassion;
    event->self_forgiveness = (forgiveness_progress > 0.6F);

    if (event->self_forgiveness) {
        // Reduce all negative emotions
        event->shame_intensity *= 0.5F;
        event->remorse_intensity *= 0.7F;
        event->regret_intensity *= 0.8F;

        // Restore self-worth
        system->emotion.self_worth += 0.15F;
        system->emotion.self_worth = fminf(1.0F, system->emotion.self_worth);

        // Update system state
        system->emotion.shame_intensity *= 0.6F;
        system->emotion.experiencing_shame = (system->emotion.shame_intensity > SHAME_THRESHOLD);
    }
}

void remorse_update(remorse_regret_system_t* system, float dt, uint64_t current_time_us) {
    // Process pending bio-async messages
    /* Phase 8: Heartbeat at operation start */
    remorse_regret_heartbeat("remorse_regr_remorse_update", 0.0f);


    if (system && system->bio_async_enabled && system->bio_ctx_ptr) {
        bio_router_process_inbox(system->bio_ctx_ptr, 5);
    }

    // WHAT: Update emotional state over time
    // WHY:  Emotions naturally decay and fade
    // HOW:  Exponential decay modulated by personality

    if (!system) return;

    // Suppress unused parameter warnings
    (void)dt;
    (void)current_time_us;

    system->total_update_calls++;

    // Decay rate influenced by neuroticism (higher = slower decay)
    float decay_rate = 0.995F - (system->neuroticism * 0.01F);

    // Decay all active events
    for (uint32_t i = 0; i < REGRET_MAX_EVENTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && REGRET_MAX_EVENTS > 256) {
            remorse_regret_heartbeat("remorse_regr_loop",
                             (float)(i + 1) / (float)REGRET_MAX_EVENTS);
        }

        if (!system->events[i].active) continue;

        regret_event_t* event = &system->events[i];

        // Natural decay
        event->regret_intensity *= decay_rate;
        event->remorse_intensity *= decay_rate;
        event->shame_intensity *= decay_rate;

        // Deactivate if negligible
        if (event->regret_intensity < 0.01F &&
            event->remorse_intensity < 0.01F &&
            event->shame_intensity < 0.01F) {
            event->active = false;
        }
    }

    // Update system emotional state
    system->emotion.guilt_intensity *= decay_rate;
    system->emotion.remorse_intensity *= decay_rate;
    system->emotion.shame_intensity *= decay_rate;
    system->emotion.regret_intensity *= decay_rate;
    system->emotion.rumination_level *= 0.98F;

    // Gradually restore self-worth
    if (system->emotion.self_worth < 0.7F) {
        system->emotion.self_worth += 0.001F * system->self_compassion;
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
    /* Phase 8: Heartbeat at operation start */
    remorse_regret_heartbeat("remorse_regr_remorse_is_guilty", 0.0f);


    return system ? system->emotion.experiencing_guilt : false;
}

bool remorse_is_remorseful(const remorse_regret_system_t* system) {
    /* Phase 8: Heartbeat at operation start */
    remorse_regret_heartbeat("remorse_regr_remorse_is_remorsefu", 0.0f);


    return system ? system->emotion.experiencing_remorse : false;
}

bool remorse_is_ashamed(const remorse_regret_system_t* system) {
    /* Phase 8: Heartbeat at operation start */
    remorse_regret_heartbeat("remorse_regr_remorse_is_ashamed", 0.0f);


    return system ? system->emotion.experiencing_shame : false;
}

float remorse_get_regret_intensity(const remorse_regret_system_t* system) {
    /* Phase 8: Heartbeat at operation start */
    remorse_regret_heartbeat("remorse_regr_remorse_get_regret_i", 0.0f);


    return system ? system->emotion.regret_intensity : 0.0F;
}

float remorse_get_self_worth(const remorse_regret_system_t* system) {
    /* Phase 8: Heartbeat at operation start */
    remorse_regret_heartbeat("remorse_regr_remorse_get_self_wor", 0.0f);


    return system ? system->emotion.self_worth : 0.7F;
}

float remorse_get_lessons_learned(const remorse_regret_system_t* system) {
    /* Phase 8: Heartbeat at operation start */
    remorse_regret_heartbeat("remorse_regr_remorse_get_lessons_", 0.0f);


    return system ? system->lessons_learned : 0.0F;
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
    /* Phase 8: Heartbeat at operation start */
    remorse_regret_heartbeat("remorse_regr_remorse_get_neuromod", 0.0f);


    float negative_emotion = (system->emotion.guilt_intensity * 0.3F +
                             system->emotion.remorse_intensity * 0.5F +
                             system->emotion.shame_intensity * 0.7F);

    *dopamine_factor = 1.0F - negative_emotion * 0.4F;
    *serotonin_factor = 1.0F - negative_emotion * 0.5F;  // Lower mood
    *norepinephrine_factor = 1.0F + system->emotion.rumination_level * 0.3F;  // Stress
}

emotional_tag_t remorse_get_emotion(const remorse_regret_system_t* system) {
    // WHAT: Get current moral emotion as emotional_tag_t
    // WHY:  Integration with emotional tagging system
    // HOW:  Map moral emotions to valence/arousal space

    /* Phase 8: Heartbeat at operation start */
    remorse_regret_heartbeat("remorse_regr_remorse_get_emotion", 0.0f);


    emotional_tag_t tag = {0};
    if (!system) return tag;

    switch (system->emotion.dominant_emotion) {
        case MORAL_EMOTION_GUILT:
            tag.valence = -0.3F - system->emotion.guilt_intensity * 0.3F;
            tag.arousal = 0.4F + system->emotion.guilt_intensity * 0.2F;
            tag.intensity = system->emotion.guilt_intensity;
            break;

        case MORAL_EMOTION_REMORSE:
            tag.valence = -0.6F - system->emotion.remorse_intensity * 0.3F;
            tag.arousal = 0.6F + system->emotion.remorse_intensity * 0.2F;
            tag.intensity = system->emotion.remorse_intensity;
            break;

        case MORAL_EMOTION_SHAME:
            tag.valence = -0.7F - system->emotion.shame_intensity * 0.25F;
            tag.arousal = 0.2F + system->emotion.shame_intensity * 0.2F;  // Withdrawal
            tag.intensity = system->emotion.shame_intensity;
            break;

        default:
            tag.valence = 0.0F;
            tag.arousal = 0.0F;
            tag.intensity = 0.0F;
            break;
    }

    tag.timestamp_ms = system->emotion.remorse_onset_time / 1000;  // Convert us to ms

    return tag;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int remorse_regret_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    remorse_regret_heartbeat("remorse_regr_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Remorse_Regret");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                remorse_regret_heartbeat("remorse_regr_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Remorse_Regret");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Remorse_Regret");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void remorse_regret_set_instance_health_agent(nimcp_health_agent_t* agent) {
    g_remorse_regret_instance_health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration Stubs
 * ============================================================================ */

int remorse_regret_training_begin(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "remorse_regret_training_begin: ctx is NULL");
        return -1;
    }
    remorse_regret_heartbeat_instance(g_remorse_regret_instance_health_agent, "remorse_regret_training_begin", 0.0f);
    return 0;
}

int remorse_regret_training_end(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "remorse_regret_training_end: ctx is NULL");
        return -1;
    }
    remorse_regret_heartbeat_instance(g_remorse_regret_instance_health_agent, "remorse_regret_training_end", 1.0f);
    return 0;
}

int remorse_regret_training_step(void* ctx, float progress) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "remorse_regret_training_step: ctx is NULL");
        return -1;
    }
    remorse_regret_heartbeat_instance(g_remorse_regret_instance_health_agent, "remorse_regret_training_step", progress);
    return 0;
}
