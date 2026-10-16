/* SPDX-License-Identifier: MIT */
/**
 * @file nimcp_shadow_emotions.c
 * @brief Phase E5: Shadow Emotions Implementation
 */

#include "cognitive/nimcp_shadow_emotions.h"
#include "utils/memory/nimcp_memory.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//=============================================================================
// HELPER FUNCTIONS
//=============================================================================

static inline float clamp(float v, float min, float max) {
    return (v < min) ? min : (v > max) ? max : v;
}

static inline float exponential_decay(float current, float target, float rate, float dt) {
    float decay_factor = expf(-rate * dt);
    return current * decay_factor + target * (1.0f - decay_factor);
}

//=============================================================================
// LIFECYCLE FUNCTIONS
//=============================================================================

shadow_emotion_system_t* shadow_system_create(uint32_t max_others_tracked) {
    /* WHAT: Create shadow emotion monitoring system
     * WHY:  Enable self-awareness and other-detection of maladaptive patterns
     * HOW:  Allocate and initialize all components
     */
    
    shadow_emotion_system_t* system = (shadow_emotion_system_t*)nimcp_calloc(1, sizeof(shadow_emotion_system_t));
    if (!system) return NULL;

    // Allocate other-detection array
    system->max_others_tracked = max_others_tracked;
    system->detected_in_others = (other_detection_t*)nimcp_calloc(max_others_tracked, sizeof(other_detection_t));
    if (!system->detected_in_others) {
        nimcp_free(system);
        return NULL;
    }

    // Initialize baseline state
    shadow_system_reset(system);

    return system;
}

void shadow_system_destroy(shadow_emotion_system_t* system) {
    if (!system) return;

    if (system->detected_in_others) {
        nimcp_free(system->detected_in_others);
    }

    nimcp_free(system);
}

void shadow_system_reset(shadow_emotion_system_t* system) {
    if (!system) return;

    // Reset jealousy
    memset(&system->jealousy, 0, sizeof(jealousy_state_t));

    // Reset envy
    memset(&system->envy, 0, sizeof(envy_state_t));
    system->envy.self_esteem = 0.6f;  // Moderate baseline

    // Reset obsession
    memset(&system->obsession, 0, sizeof(obsession_state_t));
    system->obsession.cognitive_flexibility = 0.7f;  // Moderate baseline

    // Reset hubris
    memset(&system->hubris, 0, sizeof(hubris_state_t));
    system->hubris.accountability = 0.5f;  // Moderate baseline

    // Reset greed
    memset(&system->greed, 0, sizeof(greed_state_t));
    system->greed.generosity = 0.5f;  // Moderate baseline

    // Reset narcissism
    memset(&system->narcissism, 0, sizeof(narcissism_state_t));
    system->narcissism.self_awareness = 0.5f;  // Moderate baseline

    // Reset global state
    system->total_shadow_intensity = 0.0f;
    system->mental_health_impact = 0.0f;
    system->insight_level = 0.5f;  // Moderate self-awareness
    system->in_self_correction = false;

    // Reset other-detection
    if (system->detected_in_others) {
        memset(system->detected_in_others, 0, 
               system->max_others_tracked * sizeof(other_detection_t));
    }

    // Reset intervention records
    for (int i = 0; i < SHADOW_INTERVENTION_COUNT; i++) {
        system->interventions[i].active = false;
        system->interventions[i].effectiveness = 0.5f;
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
    /* WHAT: Update all shadow emotion states over time
     * WHY:  Emotions decay, interventions take effect, patterns evolve
     * HOW:  Apply exponential decay, check thresholds, update global state
     */
    
    if (!system) return;
    system->total_update_calls++;

    float decay_rate = 1.0f / 3600.0f;  // 1-hour time constant

    // Decay jealousy
    if (system->jealousy.active) {
        system->jealousy.intensity = exponential_decay(
            system->jealousy.intensity, 0.0f, decay_rate, dt);
        system->jealousy.rumination *= 0.99f;
        
        if (system->jealousy.intensity < SHADOW_JEALOUSY_THRESHOLD * 0.5f) {
            system->jealousy.active = false;
        }
    }

    // Decay envy
    for (int i = 0; i < SHADOW_MAX_ENVY_TARGETS; i++) {
        if (system->envy.targets[i].active) {
            system->envy.targets[i].intensity = exponential_decay(
                system->envy.targets[i].intensity, 0.0f, decay_rate, dt);
            
            if (system->envy.targets[i].intensity < SHADOW_ENVY_THRESHOLD * 0.5f) {
                system->envy.targets[i].active = false;
                system->envy.active_envy_count--;
            }
        }
    }

    // Decay obsessions
    for (int i = 0; i < SHADOW_MAX_OBSESSIVE_THOUGHTS; i++) {
        if (system->obsession.thoughts[i].active) {
            system->obsession.thoughts[i].intensity = exponential_decay(
                system->obsession.thoughts[i].intensity, 0.0f, decay_rate, dt);
            
            if (system->obsession.thoughts[i].intensity < SHADOW_OBSESSION_THRESHOLD * 0.5f) {
                system->obsession.thoughts[i].active = false;
                system->obsession.active_obsession_count--;
            }
        }
    }

    // Decay hubris
    if (system->hubris.active) {
        system->hubris.intensity = exponential_decay(
            system->hubris.intensity, 0.0f, decay_rate * 0.5f, dt);  // Slower decay
        
        if (system->hubris.intensity < SHADOW_HUBRIS_THRESHOLD * 0.5f) {
            system->hubris.active = false;
        }
    }

    // Decay greed
    if (system->greed.active) {
        system->greed.craving_intensity = exponential_decay(
            system->greed.craving_intensity, 0.0f, decay_rate, dt);
        system->greed.intensity = exponential_decay(
            system->greed.intensity, 0.0f, decay_rate * 0.5f, dt);
        
        if (system->greed.intensity < SHADOW_GREED_THRESHOLD * 0.5f) {
            system->greed.active = false;
        }
    }

    // Decay narcissism (slow - trait-like)
    if (system->narcissism.active) {
        system->narcissism.intensity = exponential_decay(
            system->narcissism.intensity, 0.0f, decay_rate * 0.1f, dt);
        
        if (system->narcissism.intensity < SHADOW_NARCISSISM_THRESHOLD * 0.5f) {
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
    float normalized_intensity = system->total_shadow_intensity / 6.0f;
    system->mental_health_impact = normalized_intensity * normalized_intensity;  // Quadratic
    system->mental_health_impact = clamp(system->mental_health_impact, 0.0f, 1.0f);

    // Insight increases with self-awareness and decreases with narcissism
    float insight_target = 0.5f + (1.0f - system->narcissism.lack_of_empathy) * 0.3f;
    system->insight_level = exponential_decay(
        system->insight_level, insight_target, 1.0f / 86400.0f, dt);
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

    threat_level = clamp(threat_level, 0.0f, 1.0f);
    attachment_strength = clamp(attachment_strength, 0.0f, 1.0f);

    system->jealousy.active = true;
    system->jealousy.threatened_bond_id = bond_id;
    system->jealousy.perceived_threat = threat_level;
    system->jealousy.attachment_strength = attachment_strength;

    // Intensity = threat × attachment (both high = intense jealousy)
    system->jealousy.intensity = threat_level * attachment_strength;
    system->jealousy.intensity = clamp(system->jealousy.intensity, 0.0f, 1.0f);

    // Cognitive distortions increase with intensity
    system->jealousy.catastrophizing = system->jealousy.intensity * 0.8f;
    system->jealousy.rumination = system->jealousy.intensity * 0.7f;

    // Behavioral urges
    system->jealousy.mate_guarding_urge = system->jealousy.intensity * 0.9f;
    system->jealousy.rival_derogation_urge = system->jealousy.intensity * 0.6f;

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

    self_level = clamp(self_level, 0.0f, 1.0f);
    other_level = clamp(other_level, 0.0f, 1.0f);
    maliciousness = clamp(maliciousness, 0.0f, 1.0f);

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
    target->intensity = clamp(target->discrepancy, 0.0f, 1.0f);

    // Deservingness belief increases with envy
    target->deservingness_belief = target->intensity * 0.7f;

    // Schadenfreude only if malicious
    target->schadenfreude = target->maliciousness * target->intensity * 0.5f;

    target->onset_time = current_time;

    // Update chronic envy (trait level)
    float envy_avg = 0.0f;
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

    intensity = clamp(intensity, 0.0f, 1.0f);
    distress = clamp(distress, 0.0f, 1.0f);

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
    thought->frequency = (float)thought->intrusion_count_today / 24.0f;  // per hour estimate
    thought->last_intrusion_time = current_time;

    // Compulsive urges scale with distress
    thought->checking_urge = distress * 0.7f;
    thought->neutralizing_urge = distress * 0.8f;

    // Update overall obsession level
    float total_intensity = 0.0f;
    int count = 0;
    for (int i = 0; i < SHADOW_MAX_OBSESSIVE_THOUGHTS; i++) {
        if (system->obsession.thoughts[i].active) {
            total_intensity += system->obsession.thoughts[i].intensity;
            count++;
        }
    }
    system->obsession.overall_obsession_level = count > 0 ? (total_intensity / (float)count) : 0.0f;

    // Cognitive flexibility decreases with obsession load
    system->obsession.cognitive_flexibility = 1.0f - (system->obsession.overall_obsession_level * 0.5f);
    system->obsession.cognitive_flexibility = clamp(system->obsession.cognitive_flexibility, 0.2f, 1.0f);

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

    power_level = clamp(power_level, 0.0f, 1.0f);
    accountability = clamp(accountability, 0.0f, 1.0f);

    system->hubris.recent_success_count = recent_success_count;
    system->hubris.power_level = power_level;
    system->hubris.accountability = accountability;

    // Hubris = success + power - accountability
    float hubris_factor = (recent_success_count * 0.2f + power_level) * (1.0f - accountability);
    system->hubris.intensity = clamp(hubris_factor, 0.0f, 1.0f);

    // Components
    system->hubris.grandiosity = system->hubris.intensity * 0.8f;
    system->hubris.overconfidence = system->hubris.intensity * 0.9f;
    system->hubris.invincibility_belief = system->hubris.intensity * 0.7f;

    // Behavioral consequences
    system->hubris.risk_taking = system->hubris.intensity * 0.8f;
    system->hubris.contempt_for_others = system->hubris.intensity * 0.6f;

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

    acquisition_value = clamp(acquisition_value, 0.0f, 1.0f);
    necessity = clamp(necessity, 0.0f, 1.0f);
    scarcity_context = clamp(scarcity_context, 0.0f, 1.0f);

    // Greed = (acquisition - necessity) weighted by scarcity
    float excess = acquisition_value - necessity;
    if (excess > 0.0f) {
        // Acquiring more than needed
        float greed_increment = excess * (1.0f - scarcity_context * 0.5f);  // Less greedy if scarce
        system->greed.intensity += greed_increment * 0.1f;
        system->greed.intensity = clamp(system->greed.intensity, 0.0f, 1.0f);
    }

    system->greed.scarcity_mindset = 1.0f - scarcity_context;
    system->greed.craving_intensity = system->greed.intensity * 0.9f;

    // Hoarding tendency increases with greed
    system->greed.hoarding_tendency = system->greed.intensity * 0.7f;
    system->greed.exploitation_level = system->greed.intensity * 0.5f;

    // Generosity is inverse of greed
    system->greed.generosity = 1.0f - system->greed.intensity;
    system->greed.generosity = clamp(system->greed.generosity, 0.1f, 1.0f);

    // Hedonic adaptation (pleasure fades)
    system->greed.hedonic_adaptation = 0.8f;  // High adaptation

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

    grandiosity_level = clamp(grandiosity_level, 0.0f, 1.0f);
    empathy_level = clamp(empathy_level, 0.0f, 1.0f);
    need_for_admiration = clamp(need_for_admiration, 0.0f, 1.0f);
    entitlement = clamp(entitlement, 0.0f, 1.0f);

    system->narcissism.grandiosity = grandiosity_level;
    system->narcissism.lack_of_empathy = 1.0f - empathy_level;  // Inverse
    system->narcissism.need_for_admiration = need_for_admiration;
    system->narcissism.entitlement = entitlement;

    // NPD intensity = mean of DSM-5 criteria
    system->narcissism.intensity = (
        system->narcissism.grandiosity +
        system->narcissism.lack_of_empathy +
        system->narcissism.need_for_admiration +
        system->narcissism.entitlement
    ) / 4.0f;

    // Determine subtype
    if (system->narcissism.intensity >= 0.7f && 
        system->narcissism.exploitativeness > 0.6f &&
        system->narcissism.paranoia > 0.5f) {
        system->narcissism.subtype = NARCISSISM_MALIGNANT;
    } else if (system->narcissism.intensity >= 0.5f) {
        system->narcissism.subtype = NARCISSISM_GRANDIOSE;
    } else {
        system->narcissism.subtype = NARCISSISM_VULNERABLE;
    }

    // Rage proneness increases with narcissism
    system->narcissism.rage_proneness = system->narcissism.intensity * 0.7f;

    // Self-awareness typically low in narcissism
    system->narcissism.self_awareness = 1.0f - (system->narcissism.intensity * 0.6f);
    system->narcissism.self_awareness = clamp(system->narcissism.self_awareness, 0.1f, 1.0f);

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

    manipulation_cues = clamp(manipulation_cues, 0.0f, 1.0f);
    empathy_cues = clamp(empathy_cues, 0.0f, 1.0f);
    grandiosity_cues = clamp(grandiosity_cues, 0.0f, 1.0f);

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
                other->trust_level = 0.5f;  // Start neutral
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
    pattern->empathy_deficit_score = 1.0f - empathy_cues;  // Low empathy = deficit
    pattern->exploitation_score = (manipulation_cues + (1.0f - empathy_cues)) / 2.0f;

    // Flag as toxic if patterns are severe
    pattern->flagged_as_toxic = (
        pattern->manipulation_score > 0.6f ||
        pattern->empathy_deficit_score > 0.7f ||
        pattern->exploitation_score > 0.65f
    );

    other->history_index++;

    // Aggregate detection scores (running average over history)
    float avg_manipulation = 0.0f;
    float avg_grandiosity = 0.0f;
    float avg_empathy_deficit = 0.0f;
    float avg_exploitation = 0.0f;
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
    other->detected_narcissism = (avg_grandiosity + avg_empathy_deficit) / 2.0f;
    other->detected_greed = avg_exploitation;
    other->detected_hubris = avg_grandiosity * 0.8f;

    // Jealousy and envy harder to detect - require specific cues (simplified here)
    other->detected_jealousy = 0.0f;  // Would need possessive behavior cues
    other->detected_envy = 0.0f;      // Would need resentment cues

    // Adjust trust based on toxicity
    if (avg_manipulation > 0.6f || avg_empathy_deficit > 0.7f) {
        other->trust_level *= 0.9f;  // Reduce trust
    } else if (avg_manipulation < 0.3f && avg_empathy_deficit < 0.4f) {
        other->trust_level += (1.0f - other->trust_level) * 0.05f;  // Slowly increase
    }
    other->trust_level = clamp(other->trust_level, 0.0f, 1.0f);

    // Set protective measures
    other->maintain_boundaries = (avg_exploitation > 0.6f || avg_empathy_deficit > 0.7f);
    other->use_gray_rock = (other->detected_narcissism > 0.65f);  // Boring responses

    system->total_detections_other++;
}

bool shadow_get_detected_in_other(const shadow_emotion_system_t* system,
                                  uint32_t person_id,
                                  float* out_jealousy,
                                  float* out_narcissism,
                                  float* out_greed) {
    if (!system || !system->detected_in_others) return false;

    for (uint32_t i = 0; i < system->max_others_tracked; i++) {
        if (system->detected_in_others[i].person_id == person_id) {
            if (out_jealousy) *out_jealousy = system->detected_in_others[i].detected_jealousy;
            if (out_narcissism) *out_narcissism = system->detected_in_others[i].detected_narcissism;
            if (out_greed) *out_greed = system->detected_in_others[i].detected_greed;
            return true;
        }
    }

    return false;
}

bool shadow_should_maintain_boundaries(const shadow_emotion_system_t* system,
                                       uint32_t person_id) {
    if (!system || !system->detected_in_others) return false;

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
    
    if (!system) return false;
    if (strategy >= SHADOW_INTERVENTION_COUNT) return false;

    intervention_record_t* intervention = &system->interventions[strategy];
    intervention->target_emotion = emotion;
    intervention->strategy = strategy;
    intervention->active = true;
    intervention->last_applied_time = current_time;
    intervention->application_count++;

    float reduction = 0.0f;

    // Apply strategy-specific effects
    switch (strategy) {
        case SHADOW_INTERVENTION_COGNITIVE_REFRAME:
            // Challenge distorted thoughts (effective for jealousy, hubris)
            if (emotion == SHADOW_JEALOUSY) {
                system->jealousy.catastrophizing *= 0.7f;
                system->jealousy.rumination *= 0.8f;
                reduction = 0.2f;
            } else if (emotion == SHADOW_HUBRIS) {
                system->hubris.overconfidence *= 0.8f;
                system->hubris.invincibility_belief *= 0.7f;
                reduction = 0.25f;
            }
            break;

        case SHADOW_INTERVENTION_MINDFULNESS:
            // Present moment awareness (effective for obsession, jealousy)
            if (emotion == SHADOW_OBSESSION) {
                system->obsession.overall_obsession_level *= 0.75f;
                system->obsession.cognitive_flexibility += 0.1f;
                system->obsession.cognitive_flexibility = clamp(system->obsession.cognitive_flexibility, 0.0f, 1.0f);
                reduction = 0.25f;
            } else if (emotion == SHADOW_JEALOUSY) {
                system->jealousy.rumination *= 0.7f;
                reduction = 0.15f;
            }
            break;

        case SHADOW_INTERVENTION_PERSPECTIVE_TAKING:
            // Empathy exercise (effective for narcissism, envy)
            if (emotion == SHADOW_NARCISSISM) {
                system->narcissism.lack_of_empathy *= 0.85f;
                system->narcissism.intensity *= 0.9f;
                system->narcissism.self_awareness += 0.1f;
                system->narcissism.self_awareness = clamp(system->narcissism.self_awareness, 0.0f, 1.0f);
                reduction = 0.15f;
            } else if (emotion == SHADOW_ENVY) {
                system->envy.chronic_envy *= 0.85f;
                reduction = 0.15f;
            }
            break;

        case SHADOW_INTERVENTION_GRATITUDE:
            // Counter envy and greed with gratitude
            if (emotion == SHADOW_ENVY) {
                system->envy.chronic_envy *= 0.8f;
                system->envy.self_esteem += 0.05f;
                system->envy.self_esteem = clamp(system->envy.self_esteem, 0.0f, 1.0f);
                reduction = 0.2f;
            } else if (emotion == SHADOW_GREED) {
                system->greed.intensity *= 0.85f;
                system->greed.generosity += 0.05f;
                system->greed.generosity = clamp(system->greed.generosity, 0.0f, 1.0f);
                reduction = 0.2f;
            }
            break;

        case SHADOW_INTERVENTION_REALITY_TESTING:
            // Counter hubris and narcissism with reality checks
            if (emotion == SHADOW_HUBRIS) {
                system->hubris.overconfidence *= 0.7f;
                system->hubris.invincibility_belief *= 0.6f;
                system->hubris.fall_count++;
                system->hubris.last_reality_check_time = current_time;
                reduction = 0.3f;
            } else if (emotion == SHADOW_NARCISSISM) {
                system->narcissism.grandiosity *= 0.8f;
                system->narcissism.self_awareness += 0.1f;
                system->narcissism.self_awareness = clamp(system->narcissism.self_awareness, 0.0f, 1.0f);
                reduction = 0.2f;
            }
            break;

        case SHADOW_INTERVENTION_EXPOSURE:
            // Gradual exposure (effective for obsession, jealousy)
            if (emotion == SHADOW_OBSESSION) {
                system->obsession.overall_obsession_level *= 0.8f;
                reduction = 0.2f;
            } else if (emotion == SHADOW_JEALOUSY) {
                system->jealousy.intensity *= 0.85f;
                system->jealousy.mate_guarding_urge *= 0.8f;
                reduction = 0.2f;
            }
            break;

        default:
            return false;
    }

    // Track effectiveness (success if reduction >= 10%)
    intervention->effectiveness = reduction;
    if (reduction >= 0.1f) {
        system->successful_interventions++;
        system->in_self_correction = true;
        return true;
    } else {
        system->failed_interventions++;
        return false;
    }
}

bool shadow_auto_intervene(shadow_emotion_system_t* system,
                           uint64_t current_time) {
    /* WHAT: Automatically select and apply best intervention
     * WHY:  Self-regulation without external direction
     * HOW:  Identify strongest shadow emotion, apply best-fit strategy
     */
    
    if (!system) return false;

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
    if (max_intensity < 0.5f) {
        system->in_self_correction = false;
        return false;
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
    if (!system) return false;

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
    if (!system) return 0.0f;

    switch (emotion) {
        case SHADOW_JEALOUSY: return system->jealousy.intensity;
        case SHADOW_ENVY: return system->envy.chronic_envy;
        case SHADOW_OBSESSION: return system->obsession.overall_obsession_level;
        case SHADOW_HUBRIS: return system->hubris.intensity;
        case SHADOW_GREED: return system->greed.intensity;
        case SHADOW_NARCISSISM: return system->narcissism.intensity;
        default: return 0.0f;
    }
}

float shadow_get_mental_health_impact(const shadow_emotion_system_t* system) {
    return system ? system->mental_health_impact : 0.0f;
}

float shadow_get_insight(const shadow_emotion_system_t* system) {
    return system ? system->insight_level : 0.0f;
}

bool shadow_is_correcting(const shadow_emotion_system_t* system) {
    return system ? system->in_self_correction : false;
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
    *dopamine_factor = 1.0f;
    *serotonin_factor = 1.0f;
    *cortisol_factor = 1.0f;

    // Jealousy: High stress, low mood
    if (system->jealousy.active) {
        *serotonin_factor -= system->jealousy.intensity * 0.3f;
        *cortisol_factor += system->jealousy.intensity * 0.4f;
    }

    // Envy: Reduces serotonin, increases cortisol
    if (system->envy.active_envy_count > 0) {
        *serotonin_factor -= system->envy.chronic_envy * 0.25f;
        *cortisol_factor += system->envy.chronic_envy * 0.3f;
    }

    // Obsession: Low serotonin (repetitive), high cortisol (anxiety)
    if (system->obsession.active_obsession_count > 0) {
        *serotonin_factor -= system->obsession.overall_obsession_level * 0.35f;
        *cortisol_factor += system->obsession.overall_obsession_level * 0.4f;
    }

    // Hubris: Low serotonin (poor impulse control), high dopamine (risk seeking)
    if (system->hubris.active) {
        *serotonin_factor -= system->hubris.intensity * 0.2f;
        *dopamine_factor += system->hubris.risk_taking * 0.3f;
    }

    // Greed: High dopamine (craving), low serotonin (impulse control)
    if (system->greed.active) {
        *dopamine_factor += system->greed.craving_intensity * 0.4f;
        *serotonin_factor -= system->greed.intensity * 0.25f;
    }

    // Narcissism: Dysregulated dopamine (validation cycles)
    if (system->narcissism.active) {
        *dopamine_factor += system->narcissism.need_for_admiration * 0.3f;
        *serotonin_factor -= system->narcissism.lack_of_empathy * 0.2f;
    }

    // Clamp to reasonable ranges
    *dopamine_factor = clamp(*dopamine_factor, 0.4f, 2.0f);
    *serotonin_factor = clamp(*serotonin_factor, 0.3f, 1.5f);
    *cortisol_factor = clamp(*cortisol_factor, 1.0f, 2.5f);
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
    *empathy_modulation = 1.0f;
    *trust_modulation = 1.0f;
    *engagement_modulation = 1.0f;

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
    if (other->detected_narcissism > 0.6f) {
        *empathy_modulation = 1.0f - (other->detected_narcissism * 0.5f);
    }

    // Engagement modulation: reduce if gray rock strategy needed
    if (other->use_gray_rock) {
        *engagement_modulation = 0.3f;  // Low engagement, boring responses
    }

    // Further reduce engagement if boundaries needed
    if (other->maintain_boundaries) {
        *engagement_modulation *= 0.6f;
    }

    // Clamp
    *empathy_modulation = clamp(*empathy_modulation, 0.1f, 1.0f);
    *trust_modulation = clamp(*trust_modulation, 0.0f, 1.0f);
    *engagement_modulation = clamp(*engagement_modulation, 0.1f, 1.0f);
}
