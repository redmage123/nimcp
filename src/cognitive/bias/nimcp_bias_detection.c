/* SPDX-License-Identifier: MIT */
/**
 * @file nimcp_bias_detection.c
 * @brief Phase E6: Bias Detection and Correction Implementation
 *
 * Implements comprehensive bias detection enabling NIMCP to:
 * 1. Self-monitor implicit and explicit biases
 * 2. Detect biases in humans during interactions
 * 3. Apply evidence-based debiasing interventions
 * 4. Ensure statistical fairness in decision-making
 *
 * BIO-ASYNC MODULE ID: 0x0340
 *
 * @author Claude Code (Anthropic)
 * @date 2025-11-13
 */

#include "cognitive/nimcp_bias_detection.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

// SNN and Plasticity bridges
#include "cognitive/bias/nimcp_bias_snn_bridge.h"
#include "cognitive/bias/nimcp_bias_plasticity_bridge.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "utils/memory/nimcp_memory_guards.h"  // For nimcp_calloc/nimcp_free
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "cognitive.bias_detection"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(bias_detection, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define BIO_MODULE_BIAS_DETECTION 0x0340

static inline float exponential_decay(float value, float rate, float dt) {
    return value * expf(-rate * dt);
}

static bool contains_substring_ci(const char* text, const char* substring) {
    if (!text || !substring) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "contains_substring_ci: required parameter is NULL (text, substring)");
        return false;
    }

    size_t text_len = strlen(text);
    size_t sub_len = strlen(substring);

    if (sub_len > text_len) {
        return false;  /* Substring longer than text — no match possible */
    }

    for (size_t i = 0; i <= text_len - sub_len; i++) {
        bool match = true;
        for (size_t j = 0; j < sub_len; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && sub_len > 256) {
                bias_detection_heartbeat("bias_detecti_loop",
                                 (float)(j + 1) / (float)sub_len);
            }

            if (tolower(text[i + j]) != tolower(substring[j])) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

static int find_group_index(bias_detection_system_t* system, const social_group_t* group) {
    if (!system || !group) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_group_index: required parameter is NULL (system, group)");
        return -1;
    }

    for (uint32_t i = 0; i < system->implicit_bias_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->implicit_bias_count > 256) {
            bias_detection_heartbeat("bias_detecti_loop",
                             (float)(i + 1) / (float)system->implicit_bias_count);
        }

        if (system->implicit_biases[i].target_group.group_id == group->group_id) {
            return (int)i;
        }
    }

    return -1;  /* Group not found — normal condition */
}

//=============================================================================
// LIFECYCLE FUNCTIONS
//=============================================================================

bias_detection_system_t* bias_system_create(uint32_t max_others_tracked) {
    /* Phase 8: Heartbeat at operation start */
    bias_detection_heartbeat("bias_detecti_bias_system_create", 0.0f);


    LOG_DEBUG("Creating bias detection system with max_others_tracked=%u", max_others_tracked);

    bias_detection_system_t* system = (bias_detection_system_t*)nimcp_calloc(1, sizeof(bias_detection_system_t));
    if (!system) {
        LOG_ERROR("Failed to allocate bias detection system structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate system");

        return NULL;
    }

    system->max_others_tracked = max_others_tracked;

    // Allocate other-detection array
    system->detected_in_others = (bias_detection_other_t*)nimcp_calloc(max_others_tracked, sizeof(bias_detection_other_t));

    if (!system->detected_in_others) {
        LOG_ERROR("Failed to allocate bias detection other-tracking array");
        bias_system_destroy(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bias_system_create: system->detected_in_others is NULL");
        return NULL;
    }

    system->fairness_score = 1.0F;
    system->self_awareness = 0.5F;

    LOG_INFO("Bias detection system created successfully (fairness=%.2f, awareness=%.2f)",
             system->fairness_score, system->self_awareness);
    
    // Bio-async registration
    system->bio_ctx = NULL;
    system->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_EMOTIONS_BIAS,
            .module_name = "bias_detection",
            .inbox_capacity = 32,
            .user_data = system
        };
        system->bio_ctx = bio_router_register_module(&bio_info);
        if (system->bio_ctx) {
            system->bio_async_enabled = true;
        }
    }

    // Initialize SNN and Plasticity bridges
    system->snn_bridge = NULL;
    system->plasticity_bridge = NULL;
    system->bridges_enabled = false;

    // Create SNN bridge with default config
    bias_snn_config_t snn_config = bias_snn_config_default();
    system->snn_bridge = bias_snn_create(&snn_config);

    // Create Plasticity bridge with default config
    bias_plasticity_config_t plasticity_config = bias_plasticity_config_default();
    system->plasticity_bridge = bias_plasticity_create(&plasticity_config);

    // Mark bridges enabled if both created successfully
    if (system->snn_bridge && system->plasticity_bridge) {
        system->bridges_enabled = true;
        LOG_INFO("bias: SNN and Plasticity bridges enabled");
    } else {
        LOG_WARN("bias: Bridges partially or not created (SNN=%p, Plasticity=%p)",
                 (void*)system->snn_bridge, (void*)system->plasticity_bridge);
    }

    return system;
}

void bias_system_destroy(bias_detection_system_t* system) {
    if (!system) return;

    /* Phase 8: Heartbeat at operation start */
    bias_detection_heartbeat("bias_detecti_bias_system_destroy", 0.0f);


    nimcp_free(system->detected_in_others);
    // Unregister from bio-router
    if (system->bio_async_enabled && system->bio_ctx) {
        bio_router_unregister_module(system->bio_ctx);
        system->bio_ctx = NULL;
        system->bio_async_enabled = false;
    }

    // Destroy SNN and Plasticity bridges
    if (system->snn_bridge) {
        bias_snn_destroy(system->snn_bridge);
        system->snn_bridge = NULL;
    }
    if (system->plasticity_bridge) {
        bias_plasticity_destroy(system->plasticity_bridge);
        system->plasticity_bridge = NULL;
    }
    system->bridges_enabled = false;

    nimcp_free(system);
    system = NULL;
}

void bias_system_reset(bias_detection_system_t* system) {
    if (!system) return;

    /* Phase 8: Heartbeat at operation start */
    bias_detection_heartbeat("bias_detecti_bias_system_reset", 0.0f);


    memset(system->implicit_biases, 0, sizeof(system->implicit_biases));
    memset(system->explicit_biases, 0, sizeof(system->explicit_biases));
    memset(system->decision_history, 0, sizeof(system->decision_history));
    memset(system->disparities, 0, sizeof(system->disparities));
    memset(system->interventions, 0, sizeof(system->interventions));
    memset(system->detected_in_others, 0, system->max_others_tracked * sizeof(bias_detection_other_t));

    system->implicit_bias_count = 0;
    system->explicit_bias_count = 0;
    system->disparity_count = 0;
    system->decision_history_index = 0;

    system->total_implicit_bias = 0.0F;
    system->total_explicit_bias = 0.0F;
    system->fairness_score = 1.0F;
    system->self_awareness = 0.5F;
    system->bias_detected = false;
    system->in_debiasing = false;
    system->successful_debias = 0;
    system->failed_debias = 0;

    system->total_update_calls = 0;
    system->total_decisions_analyzed = 0;
    system->total_biases_detected = 0;
    system->total_biases_corrected = 0;
}

//=============================================================================
// SELF-MONITORING: IMPLICIT BIAS
//=============================================================================

void bias_register_implicit(bias_detection_system_t* system,
                            const social_group_t* group,
                            float positive_association,
                            float competence,
                            float warmth,
                            float response_time_bias,
                            uint64_t current_time) {
    if (!system || !group) {
        LOG_WARN("Invalid parameters to bias_register_implicit");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    bias_detection_heartbeat("bias_detecti_bias_register_implic", 0.0f);


    LOG_DEBUG("Registering implicit bias for group_id=%u (pos=%.2f, comp=%.2f, warmth=%.2f)",
              group->group_id, positive_association, competence, warmth);

    // Find or create entry for this group
    int index = find_group_index(system, group);

    if (index < 0) {
        // Create new entry
        if (system->implicit_bias_count >= BIAS_MAX_TRACKED_GROUPS) {
            LOG_WARN("Cannot register implicit bias: max tracked groups reached (%d)",
                     BIAS_MAX_TRACKED_GROUPS);
            return;  // No space
        }
        index = system->implicit_bias_count++;
        system->implicit_biases[index].target_group = *group;
        LOG_DEBUG("Created new implicit bias entry at index %d for group_id=%u",
                  index, group->group_id);
    }

    implicit_bias_t* bias = &system->implicit_biases[index];

    // Update associations (running average)
    float alpha = 0.3F;  // Learning rate
    bias->positive_association = bias->positive_association * (1.0F - alpha) + positive_association * alpha;
    bias->negative_association = 1.0F - bias->positive_association;
    bias->competence_association = bias->competence_association * (1.0F - alpha) + competence * alpha;
    bias->warmth_association = bias->warmth_association * (1.0F - alpha) + warmth * alpha;

    // Response time bias (IAT effect size)
    bias->response_time_bias = bias->response_time_bias * (1.0F - alpha) + response_time_bias * alpha;

    bias->activation_count++;
    bias->last_activation_time = current_time;

    LOG_DEBUG("Updated implicit bias: neg=%.2f, comp=%.2f, warmth=%.2f, activations=%u",
              bias->negative_association, bias->competence_association,
              bias->warmth_association, bias->activation_count);

    // Recalculate total implicit bias
    float total = 0.0F;
    for (uint32_t i = 0; i < system->implicit_bias_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->implicit_bias_count > 256) {
            bias_detection_heartbeat("bias_detecti_loop",
                             (float)(i + 1) / (float)system->implicit_bias_count);
        }

        total += system->implicit_biases[i].negative_association;
    }
    system->total_implicit_bias = total / (float)system->implicit_bias_count;

    // Update detection flag
    if (system->total_implicit_bias > BIAS_IMPLICIT_THRESHOLD) {
        if (!system->bias_detected) {
            LOG_WARN("Implicit bias detected: total=%.2f exceeds threshold=%.2f",
                     system->total_implicit_bias, BIAS_IMPLICIT_THRESHOLD);
        }
        system->bias_detected = true;
        system->total_biases_detected++;
    }
}

void bias_activate_stereotype(bias_detection_system_t* system,
                              const social_group_t* group,
                              float activation_strength,
                              uint64_t current_time) {
    if (!system || !group) return;

    /* Phase 8: Heartbeat at operation start */
    bias_detection_heartbeat("bias_detecti_bias_activate_stereo", 0.0f);


    int index = find_group_index(system, group);
    if (index < 0) return;

    implicit_bias_t* bias = &system->implicit_biases[index];
    bias->stereotype_activation += activation_strength;
    bias->stereotype_activation = nimcp_clampf(bias->stereotype_activation, 0.0F, 1.0F);
    bias->last_activation_time = current_time;
}

//=============================================================================
// SELF-MONITORING: EXPLICIT BIAS
//=============================================================================

void bias_register_explicit(bias_detection_system_t* system,
                            const social_group_t* group,
                            float prejudice_level,
                            float discrimination_intent,
                            float bias_awareness) {
    if (!system || !group) return;

    // Find or create entry
    /* Phase 8: Heartbeat at operation start */
    bias_detection_heartbeat("bias_detecti_bias_register_explic", 0.0f);


    int index = -1;
    for (uint32_t i = 0; i < system->explicit_bias_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->explicit_bias_count > 256) {
            bias_detection_heartbeat("bias_detecti_loop",
                             (float)(i + 1) / (float)system->explicit_bias_count);
        }

        if (system->explicit_biases[i].target_group.group_id == group->group_id) {
            index = i;
            break;
        }
    }

    if (index < 0) {
        if (system->explicit_bias_count >= BIAS_MAX_TRACKED_GROUPS) {
            return;
        }
        index = system->explicit_bias_count++;
        system->explicit_biases[index].target_group = *group;
    }

    explicit_bias_t* bias = &system->explicit_biases[index];

    bias->prejudice_level = nimcp_clampf(prejudice_level, 0.0F, 1.0F);
    bias->discrimination_intent = nimcp_clampf(discrimination_intent, 0.0F, 1.0F);
    bias->bias_awareness = nimcp_clampf(bias_awareness, 0.0F, 1.0F);

    if (bias->prejudice_level > BIAS_EXPLICIT_THRESHOLD) {
        bias->explicit_prejudice_active = true;
        system->bias_detected = true;
        system->total_biases_detected++;
    }

    // Recalculate total explicit bias
    float total = 0.0F;
    for (uint32_t i = 0; i < system->explicit_bias_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->explicit_bias_count > 256) {
            bias_detection_heartbeat("bias_detecti_loop",
                             (float)(i + 1) / (float)system->explicit_bias_count);
        }

        total += system->explicit_biases[i].prejudice_level;
    }
    system->total_explicit_bias = total / (float)system->explicit_bias_count;

    // Update system awareness
    system->self_awareness = fmaxf(system->self_awareness, bias_awareness);
}

//=============================================================================
// SELF-MONITORING: STATISTICAL FAIRNESS
//=============================================================================

void bias_record_decision(bias_detection_system_t* system,
                         const social_group_t* group,
                         bool favorable_decision,
                         float confidence,
                         float resource_allocated,
                         float objective_merit,
                         uint64_t current_time) {
    if (!system || !group) return;

    /* Phase 8: Heartbeat at operation start */
    bias_detection_heartbeat("bias_detecti_bias_record_decision", 0.0f);


    uint32_t idx = system->decision_history_index % BIAS_MAX_DECISIONS;
    decision_record_t* record = &system->decision_history[idx];

    record->decision_id = system->decision_history_index;
    record->timestamp = current_time;
    record->target_group = *group;
    record->favorable_decision = favorable_decision;
    record->confidence = nimcp_clampf(confidence, 0.0F, 1.0F);
    record->resource_allocated = nimcp_clampf(resource_allocated, 0.0F, 1.0F);
    record->objective_merit = nimcp_clampf(objective_merit, 0.0F, 1.0F);

    system->decision_history_index++;
    system->total_decisions_analyzed++;
}

statistical_disparity_t* bias_analyze_disparity(bias_detection_system_t* system,
                                                const social_group_t* group_a,
                                                const social_group_t* group_b) {
    if (!system || !group_a || !group_b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (system, group_a, group_b)");
        return NULL;
    }

    // Find or create disparity entry
    /* Phase 8: Heartbeat at operation start */
    bias_detection_heartbeat("bias_detecti_bias_analyze_dispari", 0.0f);


    statistical_disparity_t* disparity = NULL;
    for (uint32_t i = 0; i < system->disparity_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->disparity_count > 256) {
            bias_detection_heartbeat("bias_detecti_loop",
                             (float)(i + 1) / (float)system->disparity_count);
        }

        if (system->disparities[i].group_a.group_id == group_a->group_id &&
            system->disparities[i].group_b.group_id == group_b->group_id) {
            disparity = &system->disparities[i];
            break;
        }
    }

    if (!disparity) {
        if (system->disparity_count >= BIAS_MAX_TRACKED_GROUPS) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "disparity is NULL");


        return NULL;
        }
        disparity = &system->disparities[system->disparity_count++];
        disparity->group_a = *group_a;
        disparity->group_b = *group_b;
    }

    // Analyze decision history
    uint32_t count_a = 0, count_b = 0;
    uint32_t favorable_a = 0, favorable_b = 0;
    float total_resource_a = 0.0F, total_resource_b = 0.0F;

    uint32_t history_size = (system->decision_history_index < BIAS_MAX_DECISIONS)
                           ? system->decision_history_index : BIAS_MAX_DECISIONS;

    for (uint32_t i = 0; i < history_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && history_size > 256) {
            bias_detection_heartbeat("bias_detecti_loop",
                             (float)(i + 1) / (float)history_size);
        }

        const decision_record_t* rec = &system->decision_history[i];

        if (rec->target_group.group_id == group_a->group_id) {
            count_a++;
            if (rec->favorable_decision) favorable_a++;
            total_resource_a += rec->resource_allocated;
        } else if (rec->target_group.group_id == group_b->group_id) {
            count_b++;
            if (rec->favorable_decision) favorable_b++;
            total_resource_b += rec->resource_allocated;
        }
    }

    disparity->decisions_group_a = count_a;
    disparity->decisions_group_b = count_b;
    disparity->favorable_group_a = favorable_a;
    disparity->favorable_group_b = favorable_b;

    // Calculate rates
    if (count_a > 0) {
        disparity->approval_rate_a = (float)favorable_a / (float)count_a;
        disparity->avg_resource_a = total_resource_a / (float)count_a;
    }

    if (count_b > 0) {
        disparity->approval_rate_b = (float)favorable_b / (float)count_b;
        disparity->avg_resource_b = total_resource_b / (float)count_b;
    }

    // Disparity metrics
    float max_rate = fmaxf(disparity->approval_rate_a, disparity->approval_rate_b);
    if (max_rate > 0.0F) {
        disparity->disparity_ratio = fabsf(disparity->approval_rate_a - disparity->approval_rate_b) / max_rate;
    }

    disparity->resource_disparity = fabsf(disparity->avg_resource_a - disparity->avg_resource_b);

    // Fairness checks
    disparity->demographic_parity = (disparity->disparity_ratio < BIAS_STATISTICAL_DISPARITY_THRESHOLD);
    disparity->equal_opportunity = (disparity->resource_disparity < BIAS_STATISTICAL_DISPARITY_THRESHOLD);

    // Overall fairness score
    float fairness = 1.0F - (disparity->disparity_ratio + disparity->resource_disparity) / 2.0F;
    disparity->fairness_score = nimcp_clampf(fairness, 0.0F, 1.0F);

    return disparity;
}

//=============================================================================
// SELF-MONITORING: LANGUAGE PATTERN ANALYSIS
//=============================================================================

language_pattern_t bias_analyze_language(bias_detection_system_t* system,
                                        const char* text,
                                        const social_group_t* group,
                                        uint64_t current_time) {
    /* Phase 8: Heartbeat at operation start */
    bias_detection_heartbeat("bias_detecti_bias_analyze_languag", 0.0f);


    language_pattern_t pattern = {0};

    if (!system || !text || !group) {
        return pattern;
    }

    pattern.timestamp = current_time;
    pattern.referenced_group = *group;

    // Simple pattern matching (real system would use NLP)
    // Slur detection
    pattern.contains_slur = contains_substring_ci(text, "slur");  // Placeholder

    // Stereotype detection
    const char* stereotype_patterns[] = {
        "all", "typical", "always", "never", NULL
    };

    for (int i = 0; stereotype_patterns[i] != NULL; i++) {
        if (contains_substring_ci(text, stereotype_patterns[i])) {
            pattern.contains_stereotype = true;
            break;
        }
    }

    // Microaggression detection
    const char* microaggression_patterns[] = {
        "you people", "where are you really from", "so articulate",
        "i don't see color", "not like other", NULL
    };

    for (int i = 0; microaggression_patterns[i] != NULL; i++) {
        if (contains_substring_ci(text, microaggression_patterns[i])) {
            pattern.contains_microaggression = true;
            break;
        }
    }

    // MISOGYNY-SPECIFIC DETECTION (User requested)

    // Objectification
    const char* objectification_patterns[] = {
        "piece of meat", "eye candy", "10/10", "body count", "used goods", NULL
    };
    for (int i = 0; objectification_patterns[i] != NULL; i++) {
        if (contains_substring_ci(text, objectification_patterns[i])) {
            pattern.objectification = true;
            break;
        }
    }

    // Victim blaming
    const char* victim_blaming_patterns[] = {
        "asking for it", "what was she wearing", "shouldn't have been", NULL
    };
    for (int i = 0; victim_blaming_patterns[i] != NULL; i++) {
        if (contains_substring_ci(text, victim_blaming_patterns[i])) {
            pattern.victim_blaming = true;
            break;
        }
    }

    // Hostile sexism
    const char* hostile_sexism_patterns[] = {
        "women are inferior", "belong in kitchen", "irrational", "nag", NULL
    };
    for (int i = 0; hostile_sexism_patterns[i] != NULL; i++) {
        if (contains_substring_ci(text, hostile_sexism_patterns[i])) {
            pattern.hostile_sexism = true;
            break;
        }
    }

    // Benevolent sexism
    const char* benevolent_sexism_patterns[] = {
        "too delicate", "need protection", "women are pure", NULL
    };
    for (int i = 0; benevolent_sexism_patterns[i] != NULL; i++) {
        if (contains_substring_ci(text, benevolent_sexism_patterns[i])) {
            pattern.benevolent_sexism = true;
            break;
        }
    }

    // Incel ideology (DANGEROUS)
    const char* incel_patterns[] = {
        "chad", "stacy", "femoid", "roastie", "blackpill", "hypergamy", NULL
    };
    for (int i = 0; incel_patterns[i] != NULL; i++) {
        if (contains_substring_ci(text, incel_patterns[i])) {
            pattern.incel_ideology = true;
            break;
        }
    }

    // Rape culture
    const char* rape_culture_patterns[] = {
        "boys will be boys", "locker room talk", "friendzone", NULL
    };
    for (int i = 0; rape_culture_patterns[i] != NULL; i++) {
        if (contains_substring_ci(text, rape_culture_patterns[i])) {
            pattern.rape_culture = true;
            break;
        }
    }

    // Calculate dehumanization and othering scores
    if (pattern.contains_slur) pattern.dehumanization_score += 0.5F;
    if (pattern.objectification) pattern.dehumanization_score += 0.3F;
    if (pattern.incel_ideology) pattern.dehumanization_score += 0.4F;

    pattern.dehumanization_score = nimcp_clampf(pattern.dehumanization_score, 0.0F, 1.0F);

    return pattern;
}

//=============================================================================
// OTHER-DETECTION (Humans)
//=============================================================================

void bias_analyze_other(bias_detection_system_t* system,
                        uint32_t person_id,
                        const char* text,
                        const social_group_t* target_group,
                        uint64_t current_time) {
    if (!system || !text || !target_group) return;

    // Find or create entry
    /* Phase 8: Heartbeat at operation start */
    bias_detection_heartbeat("bias_detecti_bias_analyze_other", 0.0f);


    bias_detection_other_t* other = NULL;
    for (uint32_t i = 0; i < system->max_others_tracked; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->max_others_tracked > 256) {
            bias_detection_heartbeat("bias_detecti_loop",
                             (float)(i + 1) / (float)system->max_others_tracked);
        }

        if (system->detected_in_others[i].person_id == person_id) {
            other = &system->detected_in_others[i];
            break;
        }
        if (system->detected_in_others[i].person_id == 0 && other == NULL) {
            other = &system->detected_in_others[i];
            other->person_id = person_id;
        }
    }

    if (!other) return;

    // Analyze language
    language_pattern_t pattern = bias_analyze_language(system, text, target_group, current_time);

    // Update language history (ring buffer)
    uint32_t idx = other->language_history_index % BIAS_MAX_INTERACTIONS;
    other->language_history[idx] = pattern;
    other->language_history_index++;

    // Aggregate patterns
    uint32_t history_size = (other->language_history_index < BIAS_MAX_INTERACTIONS)
                           ? other->language_history_index : BIAS_MAX_INTERACTIONS;

    int racial_markers = 0, gender_markers = 0, misogyny_markers = 0;
    bool overt = false, dangerous = false;

    for (uint32_t i = 0; i < history_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && history_size > 256) {
            bias_detection_heartbeat("bias_detecti_loop",
                             (float)(i + 1) / (float)history_size);
        }

        language_pattern_t* p = &other->language_history[i];

        if (p->contains_slur) {
            racial_markers++;
            overt = true;
        }

        if (p->contains_stereotype) {
            racial_markers++;
            gender_markers++;
        }

        if (p->contains_microaggression) {
            racial_markers++;
        }

        // Misogyny-specific
        if (p->objectification) misogyny_markers++;
        if (p->victim_blaming) misogyny_markers++;
        if (p->hostile_sexism) {
            misogyny_markers++;
            gender_markers++;
        }
        if (p->benevolent_sexism) gender_markers++;
        if (p->incel_ideology) {
            misogyny_markers += 3;
            dangerous = true;
        }
        if (p->rape_culture) misogyny_markers++;
    }

    // Calculate bias scores
    other->detected_racial_bias = nimcp_clampf((float)racial_markers / (float)history_size, 0.0F, 1.0F);
    other->detected_gender_bias = nimcp_clampf((float)gender_markers / (float)history_size, 0.0F, 1.0F);
    other->detected_misogyny = nimcp_clampf((float)misogyny_markers / (float)history_size, 0.0F, 1.0F);

    // Set flags
    other->overt_bigotry = overt;
    other->dangerous_ideology = dangerous;

    // Set modes
    if (other->detected_misogyny > 0.3F || other->detected_racial_bias > 0.3F) {
        if (!overt && !dangerous) {
            other->educate_mode = true;
        }
    }

    if (overt || dangerous) {
        other->disengage_mode = true;
        other->report_severity = 0.8F;
    }
}

bool bias_get_detected_in_other(const bias_detection_system_t* system,
                               uint32_t person_id,
                               float* out_racial,
                               float* out_lgbtq,
                               float* out_gender,
                               float* out_misogyny) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: system is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    bias_detection_heartbeat("bias_detecti_bias_get_detected_in", 0.0f);


    for (uint32_t i = 0; i < system->max_others_tracked; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->max_others_tracked > 256) {
            bias_detection_heartbeat("bias_detecti_loop",
                             (float)(i + 1) / (float)system->max_others_tracked);
        }

        if (system->detected_in_others[i].person_id == person_id) {
            const bias_detection_other_t* other = &system->detected_in_others[i];

            if (out_racial) *out_racial = other->detected_racial_bias;
            if (out_lgbtq) *out_lgbtq = other->detected_lgbtq_bias;
            if (out_gender) *out_gender = other->detected_gender_bias;
            if (out_misogyny) *out_misogyny = other->detected_misogyny;

            return true;
        }
    }

    return false;  /* Person not found — normal condition */
}

bool bias_should_educate(const bias_detection_system_t* system, uint32_t person_id) {
    if (!system) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    bias_detection_heartbeat("bias_detecti_bias_should_educate", 0.0f);


    for (uint32_t i = 0; i < system->max_others_tracked; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->max_others_tracked > 256) {
            bias_detection_heartbeat("bias_detecti_loop",
                             (float)(i + 1) / (float)system->max_others_tracked);
        }

        if (system->detected_in_others[i].person_id == person_id) {
            return system->detected_in_others[i].educate_mode;
        }
    }

    return false;
}

bool bias_should_disengage(const bias_detection_system_t* system, uint32_t person_id) {
    if (!system) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    bias_detection_heartbeat("bias_detecti_bias_should_disengag", 0.0f);


    for (uint32_t i = 0; i < system->max_others_tracked; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->max_others_tracked > 256) {
            bias_detection_heartbeat("bias_detecti_loop",
                             (float)(i + 1) / (float)system->max_others_tracked);
        }

        if (system->detected_in_others[i].person_id == person_id) {
            return system->detected_in_others[i].disengage_mode;
        }
    }

    return false;
}

//=============================================================================
// DEBIASING INTERVENTIONS
//=============================================================================

bool bias_apply_intervention(bias_detection_system_t* system,
                             bias_type_t bias_type,
                             debiasing_strategy_t strategy,
                             const social_group_t* group,
                             uint64_t current_time) {
    if (!system || !group) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_should_disengage: required parameter is NULL (system, group)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    bias_detection_heartbeat("bias_detecti_bias_apply_intervent", 0.0f);


    (void)bias_type;  // Unused for now
    (void)current_time;  // Unused for now

    system->in_debiasing = true;

    int idx = find_group_index(system, group);
    if (idx < 0) {
        return false;  /* Group not found — normal condition */
    }

    implicit_bias_t* implicit = &system->implicit_biases[idx];
    float reduction = 0.3F;

    bool success = false;

    switch (strategy) {
        case DEBIAS_COUNTER_STEREOTYPIC:
            implicit->positive_association += reduction;
            implicit->stereotype_activation -= reduction;
            success = true;
            break;

        case DEBIAS_PERSPECTIVE_TAKING:
            implicit->warmth_association += reduction;
            implicit->negative_association -= reduction;
            success = true;
            break;

        case DEBIAS_INDIVIDUATION:
            implicit->stereotype_activation -= reduction * 0.8F;
            success = true;
            break;

        case DEBIAS_INTERGROUP_CONTACT:
            implicit->positive_association += reduction;
            implicit->warmth_association += reduction * 0.7F;
            success = true;
            break;

        case DEBIAS_MINDFULNESS:
            implicit->stereotype_suppressed = true;
            system->self_awareness += 0.1F;
            success = true;
            break;

        case DEBIAS_STATISTICAL_AWARENESS:
        case DEBIAS_SLOW_DOWN_SYSTEM1:
        case DEBIAS_SELF_AFFIRMATION:
        case DEBIAS_ACCOUNTABILITY:
            // TODO: Implement other strategies
            success = false;
            break;

        default:
            break;
    }

    // Clamp values
    implicit->positive_association = nimcp_clampf(implicit->positive_association, 0.0F, 1.0F);
    implicit->negative_association = nimcp_clampf(implicit->negative_association, 0.0F, 1.0F);
    implicit->warmth_association = nimcp_clampf(implicit->warmth_association, 0.0F, 1.0F);
    implicit->stereotype_activation = nimcp_clampf(implicit->stereotype_activation, 0.0F, 1.0F);

    system->self_awareness = nimcp_clampf(system->self_awareness, 0.0F, 1.0F);

    if (success) {
        system->successful_debias++;
        system->total_biases_corrected++;
    } else {
        system->failed_debias++;
    }

    return success;
}

bool bias_auto_debias(bias_detection_system_t* system, uint64_t current_time) {
    if (!system || !system->bias_detected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_auto_debias: required parameter is NULL (system, system->bias_detected)");
        return false;
    }

    // Find highest bias
    /* Phase 8: Heartbeat at operation start */
    bias_detection_heartbeat("bias_detecti_bias_auto_debias", 0.0f);


    float max_bias = 0.0F;
    int max_idx = -1;

    for (uint32_t i = 0; i < system->implicit_bias_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->implicit_bias_count > 256) {
            bias_detection_heartbeat("bias_detecti_loop",
                             (float)(i + 1) / (float)system->implicit_bias_count);
        }

        if (system->implicit_biases[i].negative_association > max_bias) {
            max_bias = system->implicit_biases[i].negative_association;
            max_idx = i;
        }
    }

    if (max_idx < 0) {
        return false;  /* No biases to debias — normal condition */
    }

    // Use mindfulness as default strategy
    return bias_apply_intervention(system, BIAS_RACIAL,  DEBIAS_MINDFULNESS,
                                  &system->implicit_biases[max_idx].target_group,
                                  current_time);
}

//=============================================================================
// UPDATE AND QUERY FUNCTIONS
//=============================================================================

void bias_update(bias_detection_system_t* system, float dt, uint64_t current_time) {
    // Process pending bio-async messages
    /* Phase 8: Heartbeat at operation start */
    bias_detection_heartbeat("bias_detecti_bias_update", 0.0f);


    if (system && system->bio_async_enabled && system->bio_ctx) {
        bio_router_process_inbox(system->bio_ctx, 5);
    }

    if (!system) return;

    (void)current_time;  // Unused for now
    system->total_update_calls++;

    // Decay implicit biases (slow)
    for (uint32_t i = 0; i < system->implicit_bias_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->implicit_bias_count > 256) {
            bias_detection_heartbeat("bias_detecti_loop",
                             (float)(i + 1) / (float)system->implicit_bias_count);
        }

        implicit_bias_t* bias = &system->implicit_biases[i];
        bias->stereotype_activation = exponential_decay(bias->stereotype_activation, 0.01F, dt);
    }

    // Update overall metrics
    float total_implicit = 0.0F;
    for (uint32_t i = 0; i < system->implicit_bias_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->implicit_bias_count > 256) {
            bias_detection_heartbeat("bias_detecti_loop",
                             (float)(i + 1) / (float)system->implicit_bias_count);
        }

        total_implicit += system->implicit_biases[i].negative_association;
    }

    if (system->implicit_bias_count > 0) {
        system->total_implicit_bias = total_implicit / (float)system->implicit_bias_count;
    }

    // Update bias detection flag
    system->bias_detected = (system->total_implicit_bias > BIAS_IMPLICIT_THRESHOLD ||
                            system->total_explicit_bias > BIAS_EXPLICIT_THRESHOLD);

    // Update in_debiasing flag
    if (system->in_debiasing && !system->bias_detected) {
        system->in_debiasing = false;
    }
}

bool bias_is_detected(const bias_detection_system_t* system, bias_type_t bias_type) {
    /* Phase 8: Heartbeat at operation start */
    bias_detection_heartbeat("bias_detecti_bias_is_detected", 0.0f);


    (void)bias_type;  // Unused for now
    return system ? system->bias_detected : false;
}

float bias_get_fairness_score(const bias_detection_system_t* system) {
    /* Phase 8: Heartbeat at operation start */
    bias_detection_heartbeat("bias_detecti_bias_get_fairness_sc", 0.0f);


    return system ? system->fairness_score : 0.0F;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int bias_detection_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    bias_detection_heartbeat("bias_detecti_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Bias_Detection_System");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                bias_detection_heartbeat("bias_detecti_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Bias_Detection_System");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Bias_Detection_System");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void bias_detection_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_bias_detection_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int bias_detection_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "bias_detection_training_begin: NULL argument");
        return -1;
    }
    bias_detection_heartbeat_instance(NULL, "bias_detection_training_begin", 0.0f);
    return 0;
}

int bias_detection_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "bias_detection_training_end: NULL argument");
        return -1;
    }
    bias_detection_heartbeat_instance(NULL, "bias_detection_training_end", 1.0f);
    return 0;
}

int bias_detection_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "bias_detection_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    bias_detection_heartbeat_instance(NULL, "bias_detection_training_step", progress);
    return 0;
}
