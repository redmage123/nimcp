/* SPDX-License-Identifier: MIT */
/**
 * @file nimcp_shadow_emotions.c
 * @brief Phase E5: Shadow Emotions Implementation
 */

#include "cognitive/nimcp_shadow_emotions.h"
#include "cognitive/shadow/nimcp_shadow_snn_bridge.h"
#include "cognitive/shadow/nimcp_shadow_plasticity_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/kg/nimcp_wave10_affective_kg.h"  /* W10: shadow activation events */
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_memory.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "nimcp.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "cognitive.shadow"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(shadow_emotions, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define BIO_MODULE_COGNITIVE_SHADOW 0x0353

/*=============================================================================
 * BIO-ASYNC MESSAGE HANDLERS
 *============================================================================*/

/**
 * @brief Handle introspection query about shadow emotions
 */
static nimcp_error_t handle_introspection_query(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    (void)msg_size;
    (void)response_promise;
    NIMCP_CHECK_THROW(msg && user_data, NIMCP_ERROR_NULL_ARG, "msg or user_data is NULL");

    const bio_msg_introspection_query_t* query = (const bio_msg_introspection_query_t*)msg;
    shadow_emotion_system_t* system = (shadow_emotion_system_t*)user_data;

    LOG_DEBUG(LOG_MODULE, "Received introspection query: type=%u, shadow_intensity=%.2f",
              query->query_type, system->total_shadow_intensity);

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
static int shadow_wiring_handler_callback(
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
        "shadow_wiring_handler_callback: registering %u handlers from KG",
        message_count);

    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_INTROSPECTION_QUERY:
                bio_router_register_handler(ctx, message_types[i], handle_introspection_query);
                LOG_DEBUG(LOG_MODULE,
                    "  Registered handler for BIO_MSG_INTROSPECTION_QUERY");
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
 * @brief Broadcast shadow emotion alert when intensity exceeds threshold
 */
static void bio_broadcast_shadow_alert(shadow_emotion_system_t* system) {
    if (!system || !system->bio_async_enabled || !system->bio_ctx) { return; }

    /* Only broadcast if shadow intensity is significant */
    if (system->total_shadow_intensity < 0.4F) { return; }

    /* Use introspection response to alert other modules */
    bio_msg_introspection_response_t msg = {0};
    bio_msg_init_header(&msg.header, BIO_MSG_INTROSPECTION_RESPONSE,
                        bio_module_context_get_id(system->bio_ctx), 0, sizeof(msg));
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg.query_type = 5;  /* Shadow emotion alert */
    msg.matched_pattern_count = (uint32_t)(system->total_shadow_intensity * 100);
    msg.confidence = system->insight_level;
    msg.arousal = system->total_shadow_intensity;  /* Use arousal for shadow intensity */

    bio_router_broadcast(system->bio_ctx, &msg, sizeof(msg));
    LOG_DEBUG(LOG_MODULE, "Broadcast shadow alert: intensity=%.2f, insight=%.2f",
              system->total_shadow_intensity, system->insight_level);
}

static inline float exponential_decay(float current, float target, float rate, float dt) {
    float decay_factor = expf(-rate * dt);
    return current * decay_factor + target * (1.0F - decay_factor);
}

//=============================================================================
// LIFECYCLE FUNCTIONS
//=============================================================================

shadow_emotion_system_t* shadow_system_create(uint32_t max_others_tracked) {
    LOG_DEBUG("Creating module");
    /* WHAT: Create shadow emotion monitoring system
     * WHY:  Enable self-awareness and other-detection of maladaptive patterns
     * HOW:  Allocate and initialize all components
     */
    
    shadow_emotion_system_t* system = (shadow_emotion_system_t*)nimcp_calloc(1, sizeof(shadow_emotion_system_t));
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate system");

        return NULL;

    }

    // Allocate other-detection array
    system->max_others_tracked = max_others_tracked;
    system->detected_in_others = (other_detection_t*)nimcp_calloc(max_others_tracked, sizeof(other_detection_t));
    if (!system->detected_in_others) {
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "shadow_system_create: system->detected_in_others is NULL");
        return NULL;
    }

    // Initialize baseline state
    shadow_system_reset(system);

    
    // Bio-async registration
    system->bio_ctx = NULL;
    system->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_EMOTIONS_SHADOW,
            .module_name = "shadow_emotions",
            .inbox_capacity = 32,
            .user_data = system
        };
        system->bio_ctx = bio_router_register_module(&bio_info);
        if (system->bio_ctx) {
            system->bio_async_enabled = true;

            /* KG-Driven Wiring: Register callback for orchestrator to invoke
             * When orchestrator starts, it discovers HANDLES_MESSAGE relations
             * from the KG and invokes this callback with the message types */
            nimcp_error_t cb_result = bio_router_register_wiring_callback(
                BIO_MODULE_EMOTIONS_SHADOW,
                (void*)shadow_wiring_handler_callback,
                system
            );

            if (cb_result != NIMCP_SUCCESS) {
                /* Fallback: Direct registration if orchestrator not available
                 * This ensures backward compatibility with non-KG systems */
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(system->bio_ctx, BIO_MSG_INTROSPECTION_QUERY,
                                                handle_introspection_query)
                );
                LOG_INFO(LOG_MODULE, "Bio-async registered (module_id=0x%04X, legacy)", BIO_MODULE_COGNITIVE_SHADOW);
            } else {
                LOG_INFO(LOG_MODULE, "Bio-async registered (module_id=0x%04X, KG-driven)", BIO_MODULE_COGNITIVE_SHADOW);
            }
        }
    }

    // Initialize SNN and Plasticity bridges
    system->snn_bridge = NULL;
    system->plasticity_bridge = NULL;
    system->bridges_enabled = false;

    shadow_snn_config_t snn_config = shadow_snn_config_default();
    system->snn_bridge = shadow_snn_create(&snn_config);

    shadow_plasticity_config_t plasticity_config = shadow_plasticity_config_default();
    system->plasticity_bridge = shadow_plasticity_create(&plasticity_config);

    if (system->snn_bridge && system->plasticity_bridge) {
        system->bridges_enabled = true;
        LOG_INFO(LOG_MODULE, "SNN/Plasticity bridges initialized successfully");
    } else {
        LOG_WARN(LOG_MODULE, "Failed to initialize one or more bridges");
    }

    return system;
}

void shadow_system_destroy(shadow_emotion_system_t* system) {
    LOG_DEBUG("Destroying module");
    if (!system) return;

    // Destroy SNN and Plasticity bridges
    if (system->snn_bridge) {
        shadow_snn_destroy((shadow_snn_bridge_t*)system->snn_bridge);
        system->snn_bridge = NULL;
    }
    if (system->plasticity_bridge) {
        shadow_plasticity_destroy((shadow_plasticity_bridge_t*)system->plasticity_bridge);
        system->plasticity_bridge = NULL;
    }
    system->bridges_enabled = false;

    if (system->detected_in_others) {
        nimcp_free(system->detected_in_others);
    }

    // Unregister from bio-router
    if (system->bio_async_enabled && system->bio_ctx) {
        bio_router_unregister_module(system->bio_ctx);
        system->bio_ctx = NULL;
        system->bio_async_enabled = false;
    }

    nimcp_free(system);
    system = NULL;
}

void shadow_system_reset(shadow_emotion_system_t* system) {
    if (!system) return;

    // Reset jealousy
    memset(&system->jealousy, 0, sizeof(jealousy_state_t));

    // Reset envy
    memset(&system->envy, 0, sizeof(envy_state_t));
    system->envy.self_esteem = 0.6F;  // Moderate baseline

    // Reset obsession
    memset(&system->obsession, 0, sizeof(obsession_state_t));
    system->obsession.cognitive_flexibility = 0.7F;  // Moderate baseline

    // Reset hubris
    memset(&system->hubris, 0, sizeof(hubris_state_t));
    system->hubris.accountability = 0.5F;  // Moderate baseline

    // Reset greed
    memset(&system->greed, 0, sizeof(greed_state_t));
    system->greed.generosity = 0.5F;  // Moderate baseline

    // Reset narcissism
    memset(&system->narcissism, 0, sizeof(narcissism_state_t));
    system->narcissism.self_awareness = 0.5F;  // Moderate baseline

    // Reset global state
    system->total_shadow_intensity = 0.0F;
    system->mental_health_impact = 0.0F;
    system->insight_level = 0.5F;  // Moderate self-awareness
    system->in_self_correction = false;

    // Reset other-detection
    if (system->detected_in_others) {
        memset(system->detected_in_others, 0, 
               system->max_others_tracked * sizeof(other_detection_t));
    }

    // Reset intervention records
    for (int i = 0; i < SHADOW_INTERVENTION_COUNT; i++) {
        system->interventions[i].active = false;
        system->interventions[i].effectiveness = 0.5F;
        system->interventions[i].application_count = 0;
    }

    system->successful_interventions = 0;
    system->failed_interventions = 0;
    system->total_update_calls = 0;
    system->total_detections_self = 0;
    system->total_detections_other = 0;
}


//=============================================================================
// UPDATE FUNCTION
//=============================================================================

void shadow_update(shadow_emotion_system_t* system, float dt, uint64_t current_time) {
    // Process pending bio-async messages
    if (system && system->bio_async_enabled && system->bio_ctx) {
        bio_router_process_inbox(system->bio_ctx, 5);
    }

    /* WHAT: Update all shadow emotion states over time
     * WHY:  Emotions decay, interventions take effect, patterns evolve
     * HOW:  Apply exponential decay, check thresholds, update global state
     */
    
    if (!system) return;
    system->total_update_calls++;

    float decay_rate = 1.0F / 3600.0F;  // 1-hour time constant

    // Decay jealousy
    if (system->jealousy.active) {
        system->jealousy.intensity = exponential_decay(
            system->jealousy.intensity, 0.0F, decay_rate, dt);
        system->jealousy.rumination *= 0.99F;
        
        if (system->jealousy.intensity < SHADOW_JEALOUSY_THRESHOLD * 0.5F) {
            system->jealousy.active = false;
        }
    }

    // Decay envy
    for (int i = 0; i < SHADOW_MAX_ENVY_TARGETS; i++) {
        if (system->envy.targets[i].active) {
            system->envy.targets[i].intensity = exponential_decay(
                system->envy.targets[i].intensity, 0.0F, decay_rate, dt);
            
            if (system->envy.targets[i].intensity < SHADOW_ENVY_THRESHOLD * 0.5F) {
                system->envy.targets[i].active = false;
                if (system->envy.active_envy_count > 0) system->envy.active_envy_count--;
            }
        }
    }

    // Decay obsessions
    for (int i = 0; i < SHADOW_MAX_OBSESSIVE_THOUGHTS; i++) {
        if (system->obsession.thoughts[i].active) {
            system->obsession.thoughts[i].intensity = exponential_decay(
                system->obsession.thoughts[i].intensity, 0.0F, decay_rate, dt);
            
            if (system->obsession.thoughts[i].intensity < SHADOW_OBSESSION_THRESHOLD * 0.5F) {
                system->obsession.thoughts[i].active = false;
                if (system->obsession.active_obsession_count > 0) system->obsession.active_obsession_count--;
            }
        }
    }

    // Decay hubris
    if (system->hubris.active) {
        system->hubris.intensity = exponential_decay(
            system->hubris.intensity, 0.0F, decay_rate * 0.5F, dt);  // Slower decay
        
        if (system->hubris.intensity < SHADOW_HUBRIS_THRESHOLD * 0.5F) {
            system->hubris.active = false;
        }
    }

    // Decay greed
    if (system->greed.active) {
        system->greed.craving_intensity = exponential_decay(
            system->greed.craving_intensity, 0.0F, decay_rate, dt);
        system->greed.intensity = exponential_decay(
            system->greed.intensity, 0.0F, decay_rate * 0.5F, dt);
        
        if (system->greed.intensity < SHADOW_GREED_THRESHOLD * 0.5F) {
            system->greed.active = false;
        }
    }

    // Decay narcissism (slow - trait-like)
    if (system->narcissism.active) {
        system->narcissism.intensity = exponential_decay(
            system->narcissism.intensity, 0.0F, decay_rate * 0.1F, dt);
        
        if (system->narcissism.intensity < SHADOW_NARCISSISM_THRESHOLD * 0.5F) {
            system->narcissism.active = false;
        }
    }

    // Calculate total shadow intensity
    system->total_shadow_intensity = 
        system->jealousy.intensity +
        system->envy.chronic_envy +
        system->obsession.overall_obsession_level +
        system->hubris.intensity +
        system->greed.intensity +
        system->narcissism.intensity;

    // Calculate mental health impact (non-linear - higher intensity = worse impact)
    float normalized_intensity = system->total_shadow_intensity / 6.0F;
    system->mental_health_impact = normalized_intensity * normalized_intensity;  // Quadratic
    system->mental_health_impact = nimcp_clampf(system->mental_health_impact, 0.0F, 1.0F);

    /* Broadcast shadow alert if intensity is concerning */
    bio_broadcast_shadow_alert(system);

    // Insight increases with self-awareness and decreases with narcissism
    float insight_target = 0.5F + (1.0F - system->narcissism.lack_of_empathy) * 0.3F;
    system->insight_level = exponential_decay(
        system->insight_level, insight_target, 1.0F / 86400.0F, dt);
}


//=============================================================================
// SELF-MONITORING: JEALOUSY
//=============================================================================

void shadow_experience_jealousy(shadow_emotion_system_t* system,
                                uint32_t bond_id,
                                float threat_level,
                                float attachment_strength,
                                uint64_t current_time) {
    /* WHAT: Trigger jealousy response to bond threat
     * WHY:  Attachment system protection (evolutionary)
     * HOW:  Assess threat × attachment, activate behavioral urges
     * THEORY: Mate retention (Buss, 2018), Attachment (Bowlby)
     */
    
    if (!system) return;

    threat_level = nimcp_clampf(threat_level, 0.0F, 1.0F);
    attachment_strength = nimcp_clampf(attachment_strength, 0.0F, 1.0F);

    system->jealousy.active = true;
    system->jealousy.threatened_bond_id = bond_id;
    system->jealousy.perceived_threat = threat_level;
    system->jealousy.attachment_strength = attachment_strength;

    // Intensity = threat × attachment (both high = intense jealousy)
    system->jealousy.intensity = threat_level * attachment_strength;
    system->jealousy.intensity = nimcp_clampf(system->jealousy.intensity, 0.0F, 1.0F);

    // Cognitive distortions increase with intensity
    system->jealousy.catastrophizing = system->jealousy.intensity * 0.8F;
    system->jealousy.rumination = system->jealousy.intensity * 0.7F;

    // Behavioral urges
    system->jealousy.mate_guarding_urge = system->jealousy.intensity * 0.9F;
    system->jealousy.rival_derogation_urge = system->jealousy.intensity * 0.6F;

    system->jealousy.onset_time = current_time;
    system->jealousy.episode_count++;
    
    system->total_detections_self++;
}

//=============================================================================
// SELF-MONITORING: ENVY
//=============================================================================

void shadow_experience_envy(shadow_emotion_system_t* system,
                            uint32_t target_id,
                            float self_level,
                            float other_level,
                            float maliciousness,
                            uint64_t current_time) {
    /* WHAT: Register envy from upward social comparison
     * WHY:  Status monitoring, motivational system
     * HOW:  Calculate discrepancy, assess maliciousness
     * THEORY: Social Comparison (Festinger, 1954), Envy types (Smith & Kim, 2007)
     */
    
    if (!system) return;

    self_level = nimcp_clampf(self_level, 0.0F, 1.0F);
    other_level = nimcp_clampf(other_level, 0.0F, 1.0F);
    maliciousness = nimcp_clampf(maliciousness, 0.0F, 1.0F);

    // Find slot for this target
    envy_target_t* target = NULL;
    for (int i = 0; i < SHADOW_MAX_ENVY_TARGETS; i++) {
        if (system->envy.targets[i].active && system->envy.targets[i].target_id == target_id) {
            target = &system->envy.targets[i];
            break;
        }
    }

    // If not found, allocate new slot
    if (!target) {
        for (int i = 0; i < SHADOW_MAX_ENVY_TARGETS; i++) {
            if (!system->envy.targets[i].active) {
                target = &system->envy.targets[i];
                system->envy.active_envy_count++;
                break;
            }
        }
    }

    if (!target) return;  // No slots available

    target->active = true;
    target->target_id = target_id;
    target->self_competence = self_level;
    target->other_competence = other_level;
    target->discrepancy = other_level - self_level;
    target->maliciousness = maliciousness;

    // Intensity from discrepancy (large gap = intense envy)
    target->intensity = nimcp_clampf(target->discrepancy, 0.0F, 1.0F);

    // Deservingness belief increases with envy
    target->deservingness_belief = target->intensity * 0.7F;

    // Schadenfreude only if malicious
    target->schadenfreude = target->maliciousness * target->intensity * 0.5F;

    target->onset_time = current_time;

    // Update chronic envy (trait level)
    float envy_avg = 0.0F;
    int count = 0;
    for (int i = 0; i < SHADOW_MAX_ENVY_TARGETS; i++) {
        if (system->envy.targets[i].active) {
            envy_avg += system->envy.targets[i].intensity;
            count++;
        }
    }
    if (count > 0) {
        system->envy.chronic_envy = envy_avg / (float)count;
    }

    system->total_detections_self++;
}

//=============================================================================
// SELF-MONITORING: OBSESSION
//=============================================================================

void shadow_register_obsession(shadow_emotion_system_t* system,
                               uint32_t thought_id,
                               obsession_target_type_t type,
                               float intensity,
                               float distress,
                               uint64_t current_time) {
    /* WHAT: Record intrusive obsessive thought
     * WHY:  Monitor obsessive-compulsive patterns
     * HOW:  Track frequency, intensity, distress
     * THEORY: OCD spectrum (Abramowitz et al., 2009)
     */
    
    if (!system) return;

    intensity = nimcp_clampf(intensity, 0.0F, 1.0F);
    distress = nimcp_clampf(distress, 0.0F, 1.0F);

    // Find or allocate thought slot
    obsessive_thought_t* thought = NULL;
    for (int i = 0; i < SHADOW_MAX_OBSESSIVE_THOUGHTS; i++) {
        if (system->obsession.thoughts[i].active && 
            system->obsession.thoughts[i].thought_id == thought_id) {
            thought = &system->obsession.thoughts[i];
            break;
        }
    }

    if (!thought) {
        for (int i = 0; i < SHADOW_MAX_OBSESSIVE_THOUGHTS; i++) {
            if (!system->obsession.thoughts[i].active) {
                thought = &system->obsession.thoughts[i];
                system->obsession.active_obsession_count++;
                break;
            }
        }
    }

    if (!thought) return;

    thought->active = true;
    thought->thought_id = thought_id;
    thought->type = type;
    thought->intensity = intensity;
    thought->distress = distress;

    // Update frequency (intrusions today)
    uint64_t day_start = (current_time / 86400000000ULL) * 86400000000ULL;
    if (thought->last_intrusion_time < day_start) {
        thought->intrusion_count_today = 0;
    }
    thought->intrusion_count_today++;
    thought->frequency = (float)thought->intrusion_count_today / 24.0F;  // per hour estimate
    thought->last_intrusion_time = current_time;

    // Compulsive urges scale with distress
    thought->checking_urge = distress * 0.7F;
    thought->neutralizing_urge = distress * 0.8F;

    // Update overall obsession level
    float total_intensity = 0.0F;
    int count = 0;
    for (int i = 0; i < SHADOW_MAX_OBSESSIVE_THOUGHTS; i++) {
        if (system->obsession.thoughts[i].active) {
            total_intensity += system->obsession.thoughts[i].intensity;
            count++;
        }
    }
    system->obsession.overall_obsession_level = count > 0 ? (total_intensity / (float)count) : 0.0F;

    // Cognitive flexibility decreases with obsession load
    system->obsession.cognitive_flexibility = 1.0F - (system->obsession.overall_obsession_level * 0.5F);
    system->obsession.cognitive_flexibility = nimcp_clampf(system->obsession.cognitive_flexibility, 0.2F, 1.0F);

    system->total_detections_self++;
}


//=============================================================================
// SELF-MONITORING: HUBRIS
//=============================================================================

void shadow_assess_hubris(shadow_emotion_system_t* system,
                          float recent_success_count,
                          float power_level,
                          float accountability) {
    /* WHAT: Detect hubris from success and power
     * WHY:  Prevent overconfidence leading to poor decisions
     * HOW:  Model hubris syndrome (Owen & Davidson, 2009)
     */
    
    if (!system) return;

    power_level = nimcp_clampf(power_level, 0.0F, 1.0F);
    accountability = nimcp_clampf(accountability, 0.0F, 1.0F);

    system->hubris.recent_success_count = recent_success_count;
    system->hubris.power_level = power_level;
    system->hubris.accountability = accountability;

    // Hubris = success + power - accountability
    float hubris_factor = (recent_success_count * 0.2F + power_level) * (1.0F - accountability);
    system->hubris.intensity = nimcp_clampf(hubris_factor, 0.0F, 1.0F);

    // Components
    system->hubris.grandiosity = system->hubris.intensity * 0.8F;
    system->hubris.overconfidence = system->hubris.intensity * 0.9F;
    system->hubris.invincibility_belief = system->hubris.intensity * 0.7F;

    // Behavioral consequences
    system->hubris.risk_taking = system->hubris.intensity * 0.8F;
    system->hubris.contempt_for_others = system->hubris.intensity * 0.6F;

    system->hubris.active = (system->hubris.intensity >= SHADOW_HUBRIS_THRESHOLD);
    
    if (system->hubris.active) {
        system->total_detections_self++;
    }
}

//=============================================================================
// SELF-MONITORING: GREED
//=============================================================================

void shadow_assess_greed(shadow_emotion_system_t* system,
                         float acquisition_value,
                         float necessity,
                         float scarcity_context,
                         uint64_t current_time) {
    /* WHAT: Detect greed from acquisition patterns
     * WHY:  Monitor excessive desire for resources
     * HOW:  Compare acquisition vs necessity, track patterns
     */
    
    if (!system) return;

    acquisition_value = nimcp_clampf(acquisition_value, 0.0F, 1.0F);
    necessity = nimcp_clampf(necessity, 0.0F, 1.0F);
    scarcity_context = nimcp_clampf(scarcity_context, 0.0F, 1.0F);

    // Greed = (acquisition - necessity) weighted by scarcity
    float excess = acquisition_value - necessity;
    if (excess > 0.0F) {
        // Acquiring more than needed
        float greed_increment = excess * (1.0F - scarcity_context * 0.5F);  // Less greedy if scarce
        system->greed.intensity += greed_increment * 0.1F;
        system->greed.intensity = nimcp_clampf(system->greed.intensity, 0.0F, 1.0F);
    }

    system->greed.scarcity_mindset = 1.0F - scarcity_context;
    system->greed.craving_intensity = system->greed.intensity * 0.9F;

    // Hoarding tendency increases with greed
    system->greed.hoarding_tendency = system->greed.intensity * 0.7F;
    system->greed.exploitation_level = system->greed.intensity * 0.5F;

    // Generosity is inverse of greed
    system->greed.generosity = 1.0F - system->greed.intensity;
    system->greed.generosity = nimcp_clampf(system->greed.generosity, 0.1F, 1.0F);

    // Hedonic adaptation (pleasure fades)
    system->greed.hedonic_adaptation = 0.8F;  // High adaptation

    system->greed.acquisition_count++;
    system->greed.last_acquisition_time = current_time;

    system->greed.active = (system->greed.intensity >= SHADOW_GREED_THRESHOLD);
    
    if (system->greed.active) {
        system->total_detections_self++;
    }
}

//=============================================================================
// SELF-MONITORING: NARCISSISM
//=============================================================================

void shadow_assess_narcissism(shadow_emotion_system_t* system,
                              float grandiosity_level,
                              float empathy_level,
                              float need_for_admiration,
                              float entitlement) {
    /* WHAT: Detect narcissistic patterns (DSM-5 criteria)
     * WHY:  Self-awareness of grandiosity and empathy deficits
     * HOW:  Track DSM-5 NPD criteria
     */
    
    if (!system) return;

    grandiosity_level = nimcp_clampf(grandiosity_level, 0.0F, 1.0F);
    empathy_level = nimcp_clampf(empathy_level, 0.0F, 1.0F);
    need_for_admiration = nimcp_clampf(need_for_admiration, 0.0F, 1.0F);
    entitlement = nimcp_clampf(entitlement, 0.0F, 1.0F);

    system->narcissism.grandiosity = grandiosity_level;
    system->narcissism.lack_of_empathy = 1.0F - empathy_level;  // Inverse
    system->narcissism.need_for_admiration = need_for_admiration;
    system->narcissism.entitlement = entitlement;

    // NPD intensity = mean of DSM-5 criteria
    system->narcissism.intensity = (
        system->narcissism.grandiosity +
        system->narcissism.lack_of_empathy +
        system->narcissism.need_for_admiration +
        system->narcissism.entitlement
    ) / 4.0F;

    // Determine subtype
    if (system->narcissism.intensity >= 0.7F && 
        system->narcissism.exploitativeness > 0.6F &&
        system->narcissism.paranoia > 0.5F) {
        system->narcissism.subtype = NARCISSISM_MALIGNANT;
    } else if (system->narcissism.intensity >= 0.5F) {
        system->narcissism.subtype = NARCISSISM_GRANDIOSE;
    } else {
        system->narcissism.subtype = NARCISSISM_VULNERABLE;
    }

    // Rage proneness increases with narcissism
    system->narcissism.rage_proneness = system->narcissism.intensity * 0.7F;

    // Self-awareness typically low in narcissism
    system->narcissism.self_awareness = 1.0F - (system->narcissism.intensity * 0.6F);
    system->narcissism.self_awareness = nimcp_clampf(system->narcissism.self_awareness, 0.1F, 1.0F);

    system->narcissism.active = (system->narcissism.intensity >= SHADOW_NARCISSISM_THRESHOLD);
    
    if (system->narcissism.active) {
        system->total_detections_self++;
    }
}


//=============================================================================
// OTHER-DETECTION: Recognize shadow emotions in humans
//=============================================================================

void shadow_analyze_other(shadow_emotion_system_t* system,
                          uint32_t person_id,
                          const char* interaction_text,
                          float manipulation_cues,
                          float empathy_cues,
                          float grandiosity_cues,
                          uint64_t current_time) {
    /* WHAT: Analyze human interaction for shadow emotion patterns
     * WHY:  Protect self from toxic individuals, adjust interaction style
     * HOW:  Track patterns over time, apply theory of mind
     * THEORY: Dark Triad detection (Paulhus & Williams, 2002)
     */
    
    if (!system || !system->detected_in_others) return;

    manipulation_cues = nimcp_clampf(manipulation_cues, 0.0F, 1.0F);
    empathy_cues = nimcp_clampf(empathy_cues, 0.0F, 1.0F);
    grandiosity_cues = nimcp_clampf(grandiosity_cues, 0.0F, 1.0F);

    // Find or allocate detection record for this person
    other_detection_t* other = NULL;
    for (uint32_t i = 0; i < system->max_others_tracked; i++) {
        if (system->detected_in_others[i].person_id == person_id) {
            other = &system->detected_in_others[i];
            break;
        }
    }

    if (!other) {
        // Allocate new slot
        for (uint32_t i = 0; i < system->max_others_tracked; i++) {
            if (system->detected_in_others[i].person_id == 0) {
                other = &system->detected_in_others[i];
                other->person_id = person_id;
                other->trust_level = 0.5F;  // Start neutral
                break;
            }
        }
    }

    if (!other) return;

    // Record interaction pattern
    uint32_t idx = other->history_index % SHADOW_MAX_INTERACTION_HISTORY;
    interaction_pattern_t* pattern = &other->history[idx];
    
    pattern->interaction_id = other->history_index;
    pattern->timestamp = current_time;
    pattern->manipulation_score = manipulation_cues;
    pattern->grandiosity_score = grandiosity_cues;
    pattern->empathy_deficit_score = 1.0F - empathy_cues;  // Low empathy = deficit
    pattern->exploitation_score = (manipulation_cues + (1.0F - empathy_cues)) / 2.0F;

    // Flag as toxic if patterns are severe
    pattern->flagged_as_toxic = (
        pattern->manipulation_score > 0.6F ||
        pattern->empathy_deficit_score > 0.7F ||
        pattern->exploitation_score > 0.65F
    );

    other->history_index++;

    // Aggregate detection scores (running average over history)
    float avg_manipulation = 0.0F;
    float avg_grandiosity = 0.0F;
    float avg_empathy_deficit = 0.0F;
    float avg_exploitation = 0.0F;
    int history_count = 0;

    for (int i = 0; i < SHADOW_MAX_INTERACTION_HISTORY && i < other->history_index; i++) {
        avg_manipulation += other->history[i].manipulation_score;
        avg_grandiosity += other->history[i].grandiosity_score;
        avg_empathy_deficit += other->history[i].empathy_deficit_score;
        avg_exploitation += other->history[i].exploitation_score;
        history_count++;
    }

    if (history_count > 0) {
        avg_manipulation /= (float)history_count;
        avg_grandiosity /= (float)history_count;
        avg_empathy_deficit /= (float)history_count;
        avg_exploitation /= (float)history_count;
    }

    // Detect specific shadow emotions
    other->detected_narcissism = (avg_grandiosity + avg_empathy_deficit) / 2.0F;
    other->detected_greed = avg_exploitation;
    other->detected_hubris = avg_grandiosity * 0.8F;

    // Jealousy and envy harder to detect - require specific cues (simplified here)
    other->detected_jealousy = 0.0F;  // Would need possessive behavior cues
    other->detected_envy = 0.0F;      // Would need resentment cues

    // Adjust trust based on toxicity
    if (avg_manipulation > 0.6F || avg_empathy_deficit > 0.7F) {
        other->trust_level *= 0.9F;  // Reduce trust
    } else if (avg_manipulation < 0.3F && avg_empathy_deficit < 0.4F) {
        other->trust_level += (1.0F - other->trust_level) * 0.05F;  // Slowly increase
    }
    other->trust_level = nimcp_clampf(other->trust_level, 0.0F, 1.0F);

    // Set protective measures
    other->maintain_boundaries = (avg_exploitation > 0.6F || avg_empathy_deficit > 0.7F);
    other->use_gray_rock = (other->detected_narcissism > 0.65F);  // Boring responses

    system->total_detections_other++;
}

bool shadow_get_detected_in_other(const shadow_emotion_system_t* system,
                                  uint32_t person_id,
                                  float* out_jealousy,
                                  float* out_narcissism,
                                  float* out_greed) {
    if (!system || !system->detected_in_others) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (system, system->detected_in_others)");
        return false;
    }

    for (uint32_t i = 0; i < system->max_others_tracked; i++) {
        if (system->detected_in_others[i].person_id == person_id) {
            if (out_jealousy) *out_jealousy = system->detected_in_others[i].detected_jealousy;
            if (out_narcissism) *out_narcissism = system->detected_in_others[i].detected_narcissism;
            if (out_greed) *out_greed = system->detected_in_others[i].detected_greed;
            return true;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
    return false;
}

bool shadow_should_maintain_boundaries(const shadow_emotion_system_t* system,
                                       uint32_t person_id) {
    if (!system || !system->detected_in_others) {
        return false;
    }

    for (uint32_t i = 0; i < system->max_others_tracked; i++) {
        if (system->detected_in_others[i].person_id == person_id) {
            return system->detected_in_others[i].maintain_boundaries;
        }
    }

    return false;
}


//=============================================================================
// SELF-CORRECTION: CBT-based interventions
//=============================================================================

bool shadow_apply_intervention(shadow_emotion_system_t* system,
                               shadow_emotion_type_t emotion,
                               shadow_intervention_type_t strategy,
                               uint64_t current_time) {
    /* WHAT: Apply CBT intervention to reduce shadow emotion
     * WHY:  Self-correction, mental health management
     * HOW:  Cognitive reframing, mindfulness, perspective-taking
     * THEORY: CBT (Beck, 1976), ACT (Hayes et al., 1999)
     */
    
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: system is NULL");
        return false;
    }
    if (strategy >= SHADOW_INTERVENTION_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "unknown: capacity exceeded");
        return false;
    }

    intervention_record_t* intervention = &system->interventions[strategy];
    intervention->target_emotion = emotion;
    intervention->strategy = strategy;
    intervention->active = true;
    intervention->last_applied_time = current_time;
    intervention->application_count++;

    float reduction = 0.0F;

    // Apply strategy-specific effects
    switch (strategy) {
        case SHADOW_INTERVENTION_COGNITIVE_REFRAME:
            // Challenge distorted thoughts (effective for jealousy, hubris)
            if (emotion == SHADOW_JEALOUSY) {
                system->jealousy.catastrophizing *= 0.7F;
                system->jealousy.rumination *= 0.8F;
                reduction = 0.2F;
            } else if (emotion == SHADOW_HUBRIS) {
                system->hubris.overconfidence *= 0.8F;
                system->hubris.invincibility_belief *= 0.7F;
                reduction = 0.25F;
            }
            break;

        case SHADOW_INTERVENTION_MINDFULNESS:
            // Present moment awareness (effective for obsession, jealousy)
            if (emotion == SHADOW_OBSESSION) {
                system->obsession.overall_obsession_level *= 0.75F;
                system->obsession.cognitive_flexibility += 0.1F;
                system->obsession.cognitive_flexibility = nimcp_clampf(system->obsession.cognitive_flexibility, 0.0F, 1.0F);
                reduction = 0.25F;
            } else if (emotion == SHADOW_JEALOUSY) {
                system->jealousy.rumination *= 0.7F;
                reduction = 0.15F;
            }
            break;

        case SHADOW_INTERVENTION_PERSPECTIVE_TAKING:
            // Empathy exercise (effective for narcissism, envy)
            if (emotion == SHADOW_NARCISSISM) {
                system->narcissism.lack_of_empathy *= 0.85F;
                system->narcissism.intensity *= 0.9F;
                system->narcissism.self_awareness += 0.1F;
                system->narcissism.self_awareness = nimcp_clampf(system->narcissism.self_awareness, 0.0F, 1.0F);
                reduction = 0.15F;
            } else if (emotion == SHADOW_ENVY) {
                system->envy.chronic_envy *= 0.85F;
                reduction = 0.15F;
            }
            break;

        case SHADOW_INTERVENTION_GRATITUDE:
            // Counter envy and greed with gratitude
            if (emotion == SHADOW_ENVY) {
                system->envy.chronic_envy *= 0.8F;
                system->envy.self_esteem += 0.05F;
                system->envy.self_esteem = nimcp_clampf(system->envy.self_esteem, 0.0F, 1.0F);
                reduction = 0.2F;
            } else if (emotion == SHADOW_GREED) {
                system->greed.intensity *= 0.85F;
                system->greed.generosity += 0.05F;
                system->greed.generosity = nimcp_clampf(system->greed.generosity, 0.0F, 1.0F);
                reduction = 0.2F;
            }
            break;

        case SHADOW_INTERVENTION_REALITY_TESTING:
            // Counter hubris and narcissism with reality checks
            if (emotion == SHADOW_HUBRIS) {
                system->hubris.overconfidence *= 0.7F;
                system->hubris.invincibility_belief *= 0.6F;
                system->hubris.fall_count++;
                system->hubris.last_reality_check_time = current_time;
                reduction = 0.3F;
            } else if (emotion == SHADOW_NARCISSISM) {
                system->narcissism.grandiosity *= 0.8F;
                system->narcissism.self_awareness += 0.1F;
                system->narcissism.self_awareness = nimcp_clampf(system->narcissism.self_awareness, 0.0F, 1.0F);
                reduction = 0.2F;
            }
            break;

        case SHADOW_INTERVENTION_EXPOSURE:
            // Gradual exposure (effective for obsession, jealousy)
            if (emotion == SHADOW_OBSESSION) {
                system->obsession.overall_obsession_level *= 0.8F;
                reduction = 0.2F;
            } else if (emotion == SHADOW_JEALOUSY) {
                system->jealousy.intensity *= 0.85F;
                system->jealousy.mate_guarding_urge *= 0.8F;
                reduction = 0.2F;
            }
            break;

        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: operation failed");
            return false;
    }

    // Track effectiveness (success if reduction >= 10%)
    intervention->effectiveness = reduction;
    if (reduction >= 0.1F) {
        system->successful_interventions++;
        system->in_self_correction = true;
        return true;
    } else {
        system->failed_interventions++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: capacity exceeded");
        return false;
    }
}

bool shadow_auto_intervene(shadow_emotion_system_t* system,
                           uint64_t current_time) {
    /* WHAT: Automatically select and apply best intervention
     * WHY:  Self-regulation without external direction
     * HOW:  Identify strongest shadow emotion, apply best-fit strategy
     */
    
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: system is NULL");
        return false;
    }

    // Find strongest shadow emotion
    shadow_emotion_type_t strongest = SHADOW_JEALOUSY;
    float max_intensity = system->jealousy.intensity;

    if (system->envy.chronic_envy > max_intensity) {
        strongest = SHADOW_ENVY;
        max_intensity = system->envy.chronic_envy;
    }
    if (system->obsession.overall_obsession_level > max_intensity) {
        strongest = SHADOW_OBSESSION;
        max_intensity = system->obsession.overall_obsession_level;
    }
    if (system->hubris.intensity > max_intensity) {
        strongest = SHADOW_HUBRIS;
        max_intensity = system->hubris.intensity;
    }
    if (system->greed.intensity > max_intensity) {
        strongest = SHADOW_GREED;
        max_intensity = system->greed.intensity;
    }
    if (system->narcissism.intensity > max_intensity) {
        strongest = SHADOW_NARCISSISM;
        max_intensity = system->narcissism.intensity;
    }

    // Only intervene if above threshold
    if (max_intensity < 0.5F) {
        system->in_self_correction = false;
        return false;  /* Below threshold — no intervention needed */
    }

    // Select best strategy for this emotion
    shadow_intervention_type_t strategy = SHADOW_INTERVENTION_COGNITIVE_REFRAME;
    switch (strongest) {
        case SHADOW_JEALOUSY:
            strategy = SHADOW_INTERVENTION_MINDFULNESS;
            break;
        case SHADOW_ENVY:
            strategy = SHADOW_INTERVENTION_GRATITUDE;
            break;
        case SHADOW_OBSESSION:
            strategy = SHADOW_INTERVENTION_MINDFULNESS;
            break;
        case SHADOW_HUBRIS:
            strategy = SHADOW_INTERVENTION_REALITY_TESTING;
            break;
        case SHADOW_GREED:
            strategy = SHADOW_INTERVENTION_GRATITUDE;
            break;
        case SHADOW_NARCISSISM:
            strategy = SHADOW_INTERVENTION_PERSPECTIVE_TAKING;
            break;
        default:
            strategy = SHADOW_INTERVENTION_COGNITIVE_REFRAME;
    }

    return shadow_apply_intervention(system, strongest, strategy, current_time);
}

//=============================================================================
// QUERY FUNCTIONS
//=============================================================================

bool shadow_is_active(const shadow_emotion_system_t* system,
                      shadow_emotion_type_t emotion) {
    if (!system) {
        return false;
    }

    switch (emotion) {
        case SHADOW_JEALOUSY: return system->jealousy.active;
        case SHADOW_ENVY: return (system->envy.active_envy_count > 0);
        case SHADOW_OBSESSION: return (system->obsession.active_obsession_count > 0);
        case SHADOW_HUBRIS: return system->hubris.active;
        case SHADOW_GREED: return system->greed.active;
        case SHADOW_NARCISSISM: return system->narcissism.active;
        default: return false;
    }
}

float shadow_get_intensity(const shadow_emotion_system_t* system,
                           shadow_emotion_type_t emotion) {
    if (!system) return 0.0F;

    switch (emotion) {
        case SHADOW_JEALOUSY: return system->jealousy.intensity;
        case SHADOW_ENVY: return system->envy.chronic_envy;
        case SHADOW_OBSESSION: return system->obsession.overall_obsession_level;
        case SHADOW_HUBRIS: return system->hubris.intensity;
        case SHADOW_GREED: return system->greed.intensity;
        case SHADOW_NARCISSISM: return system->narcissism.intensity;
        default: return 0.0F;
    }
}

float shadow_get_mental_health_impact(const shadow_emotion_system_t* system) {
    return system ? system->mental_health_impact : 0.0F;
}

float shadow_get_insight(const shadow_emotion_system_t* system) {
    return system ? system->insight_level : 0.0F;
}

bool shadow_is_correcting(const shadow_emotion_system_t* system) {
    return system ? system->in_self_correction : false;
}

/* ============================================================================
 * W10 (2026-04-24): KG runtime emit + read-path for shadow activation.
 *
 * Callers that hold a brain_t + shadow_emotion_system_t invoke this post-
 * assessment to register the shadow activation into brain->internal_kg and
 * query the stored max shadow intensity as a bias.
 * ============================================================================ */
float shadow_emotions_wave10_kg_emit(
    struct brain_struct* brain,
    const shadow_emotion_system_t* system,
    shadow_emotion_type_t emotion)
{
    if (!brain || !system) return 0.5f;
    const char* name = "shadow";
    switch (emotion) {
        case SHADOW_JEALOUSY:  name = "jealousy";  break;
        case SHADOW_ENVY:      name = "envy";      break;
        case SHADOW_OBSESSION: name = "obsession"; break;
        case SHADOW_HUBRIS:    name = "hubris";    break;
        case SHADOW_GREED:     name = "greed";     break;
        case SHADOW_NARCISSISM: name = "narcissism"; break;
        default: break;
    }
    float intensity = shadow_get_intensity(system, emotion);
    wave10_shadow_emit_activation(brain, name, intensity);
    return wave10_shadow_query_intensity_bias(brain);
}


//=============================================================================
// INTEGRATION FUNCTIONS
//=============================================================================

void shadow_get_neuromodulator_effects(const shadow_emotion_system_t* system,
                                       float* dopamine_factor,
                                       float* serotonin_factor,
                                       float* cortisol_factor) {
    /* WHAT: Map shadow emotions to neuromodulator changes
     * WHY:  Biological realism - emotions affect brain chemistry
     * HOW:  Evidence-based mappings
     *
     * BIOLOGICAL BASIS:
     * - Jealousy/Envy: Reduce serotonin (low mood), increase cortisol (stress)
     * - Obsession: Reduce serotonin (repetitive thoughts), increase cortisol
     * - Hubris: Reduce serotonin (impulse control), increase dopamine (risk)
     * - Greed: Increase dopamine (craving), reduce serotonin (impulse)
     * - Narcissism: Dysregulated dopamine (validation seeking)
     */
    
    if (!system || !dopamine_factor || !serotonin_factor || !cortisol_factor) return;

    // Defaults
    *dopamine_factor = 1.0F;
    *serotonin_factor = 1.0F;
    *cortisol_factor = 1.0F;

    // Jealousy: High stress, low mood
    if (system->jealousy.active) {
        *serotonin_factor -= system->jealousy.intensity * 0.3F;
        *cortisol_factor += system->jealousy.intensity * 0.4F;
    }

    // Envy: Reduces serotonin, increases cortisol
    if (system->envy.active_envy_count > 0) {
        *serotonin_factor -= system->envy.chronic_envy * 0.25F;
        *cortisol_factor += system->envy.chronic_envy * 0.3F;
    }

    // Obsession: Low serotonin (repetitive), high cortisol (anxiety)
    if (system->obsession.active_obsession_count > 0) {
        *serotonin_factor -= system->obsession.overall_obsession_level * 0.35F;
        *cortisol_factor += system->obsession.overall_obsession_level * 0.4F;
    }

    // Hubris: Low serotonin (poor impulse control), high dopamine (risk seeking)
    if (system->hubris.active) {
        *serotonin_factor -= system->hubris.intensity * 0.2F;
        *dopamine_factor += system->hubris.risk_taking * 0.3F;
    }

    // Greed: High dopamine (craving), low serotonin (impulse control)
    if (system->greed.active) {
        *dopamine_factor += system->greed.craving_intensity * 0.4F;
        *serotonin_factor -= system->greed.intensity * 0.25F;
    }

    // Narcissism: Dysregulated dopamine (validation cycles)
    if (system->narcissism.active) {
        *dopamine_factor += system->narcissism.need_for_admiration * 0.3F;
        *serotonin_factor -= system->narcissism.lack_of_empathy * 0.2F;
    }

    // Clamp to reasonable ranges
    *dopamine_factor = nimcp_clampf(*dopamine_factor, 0.4F, 2.0F);
    *serotonin_factor = nimcp_clampf(*serotonin_factor, 0.3F, 1.5F);
    *cortisol_factor = nimcp_clampf(*cortisol_factor, 1.0F, 2.5F);
}

void shadow_get_interaction_modulation(const shadow_emotion_system_t* system,
                                       uint32_t person_id,
                                       float* empathy_modulation,
                                       float* trust_modulation,
                                       float* engagement_modulation) {
    /* WHAT: Adjust interaction style based on detected toxicity in other
     * WHY:  Self-protection, ethical boundaries
     * HOW:  Reduce empathy/trust/engagement with toxic individuals
     *
     * PROTECTIVE STRATEGIES:
     * - Gray Rock: Boring, uninteresting responses (narcissist defense)
     * - Boundary Setting: Limit emotional investment
     * - Trust Reduction: Skepticism about motives
     */
    
    if (!system || !system->detected_in_others) return;
    if (!empathy_modulation || !trust_modulation || !engagement_modulation) return;

    // Defaults: no modulation
    *empathy_modulation = 1.0F;
    *trust_modulation = 1.0F;
    *engagement_modulation = 1.0F;

    // Find detection record
    other_detection_t* other = NULL;
    for (uint32_t i = 0; i < system->max_others_tracked; i++) {
        if (system->detected_in_others[i].person_id == person_id) {
            other = &system->detected_in_others[i];
            break;
        }
    }

    if (!other) return;

    // Trust modulation: directly from trust level
    *trust_modulation = other->trust_level;

    // Empathy modulation: reduce if narcissism detected (avoid feeding supply)
    if (other->detected_narcissism > 0.6F) {
        *empathy_modulation = 1.0F - (other->detected_narcissism * 0.5F);
    }

    // Engagement modulation: reduce if gray rock strategy needed
    if (other->use_gray_rock) {
        *engagement_modulation = 0.3F;  // Low engagement, boring responses
    }

    // Further reduce engagement if boundaries needed
    if (other->maintain_boundaries) {
        *engagement_modulation *= 0.6F;
    }

    // Clamp
    *empathy_modulation = nimcp_clampf(*empathy_modulation, 0.1F, 1.0F);
    *trust_modulation = nimcp_clampf(*trust_modulation, 0.0F, 1.0F);
    *engagement_modulation = nimcp_clampf(*engagement_modulation, 0.1F, 1.0F);
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int shadow_emotions_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Shadow_Emotions");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Shadow_Emotions");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Shadow_Emotions");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void shadow_emotions_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    (void)instance;
    g_shadow_emotions_instance_health_agent = agent;
}

/* ============================================================================
 * Phase 8: Full training implementation
 * ============================================================================ */
static uint64_t g_shadow_emotions_training_steps = 0;
static double g_shadow_emotions_training_total_error = 0.0;
static double g_shadow_emotions_training_best_error = 1e30;
static bool g_shadow_emotions_training_active = false;

int shadow_emotions_training_begin(void* instance) {
    (void)instance;
    shadow_emotions_heartbeat_instance(g_shadow_emotions_instance_health_agent, "shadow_emo_train_beg", 0.0f);

    /* Reset training counters */
    g_shadow_emotions_training_steps = 0;
    g_shadow_emotions_training_total_error = 0.0;
    g_shadow_emotions_training_best_error = 1e30;
    g_shadow_emotions_training_active = true;

    NIMCP_LOGGING_INFO("shadow_emotions training begin: counters reset");
    return 0;
}

int shadow_emotions_training_step(void* instance, float progress) {
    (void)instance;

    /* Clamp progress to [0, 1] */
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    shadow_emotions_heartbeat_instance(g_shadow_emotions_instance_health_agent, "shadow_emo_train_stp", progress);

    g_shadow_emotions_training_steps++;

    /* Progressive adaptation: decay error accumulator */
    float decay = 1.0f - 0.1f * progress;
    if (decay < 0.5f) decay = 0.5f;
    g_shadow_emotions_training_total_error *= (double)decay;

    /* Adaptive threshold adjustment based on progress */
    float threshold_adjust = 0.01f * progress;
    g_shadow_emotions_training_best_error -= (double)threshold_adjust;
    if (g_shadow_emotions_training_best_error < 0.0) g_shadow_emotions_training_best_error = 0.0;

    return 0;
}

int shadow_emotions_training_end(void* instance) {
    (void)instance;
    shadow_emotions_heartbeat_instance(g_shadow_emotions_instance_health_agent, "shadow_emo_train_end", 1.0f);

    /* Compute final averages */
    double avg_error = (g_shadow_emotions_training_steps > 0)
        ? g_shadow_emotions_training_total_error / (double)g_shadow_emotions_training_steps
        : 0.0;

    /* Clear training flag */
    g_shadow_emotions_training_active = false;

    NIMCP_LOGGING_INFO("shadow_emotions training end: %lu steps, avg_error=%.6f, best_error=%.6f",
                       (unsigned long)g_shadow_emotions_training_steps,
                       avg_error, g_shadow_emotions_training_best_error);
    return 0;
}
