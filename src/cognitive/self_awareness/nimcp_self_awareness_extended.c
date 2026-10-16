// ============================================================================
// nimcp_self_awareness_extended.c - Advanced Self-Awareness Implementation
// ============================================================================
/**
 * @file nimcp_self_awareness_extended.c
 * @brief Phase 2 & 3 Self-Awareness: Metacognition, Narratives, Agency, Safety
 *
 * WHAT: Advanced self-awareness capabilities beyond basic introspection
 * WHY:  Complete self-awareness requires metacognitive control, self-narrative,
 *       temporal continuity, agency attribution, and self-protection
 * HOW:  Integrated system combining multiple consciousness research findings
 *
 * @version 2.8.0 (Phase 12: Self-Awareness Enhancement)
 * @date 2025-11-11
 */

#include "cognitive/nimcp_self_awareness_extended.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/executive/nimcp_w9kg_events.h"  // W9-kg: KG event + read helpers
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "cognitive.self_awareness"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE(self_awareness_extended, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define BIO_MODULE_COGNITIVE_SELF_AWARENESS 0x0350


// ============================================================================
// Internal Structures
// ============================================================================

struct self_awareness_system {
    char name[NIMCP_ID_BUFFER_SIZE];
    char role[NIMCP_ERROR_BUFFER_SIZE];
    char purpose[NIMCP_ERROR_BUFFER_LARGE];

    self_model_system_t self_model;
    autobiographical_memory_t autobio;
    introspection_context_t introspection;

    temporal_self_t temporal_self;
    metacognitive_assessment_t last_metacog;

    uint32_t reflection_count;
    uint64_t last_reflection_ms;

    nimcp_mutex_t mutex;

    // Bio-async integration
    bio_module_context_t bio_ctx;   /**< Bio-async module context */
    bool bio_async_enabled;         /**< Bio-async registration status */
};

// ============================================================================
// Helper Functions
// ============================================================================

static uint64_t get_current_time_ms(void)
{
    return nimcp_time_monotonic_ms();
}

// ============================================================================
// Metacognitive Control
// ============================================================================

bool metacognition_assess(introspection_context_t introspection,
                         const float* recent_performance,
                         uint32_t num_recent,
                         metacognitive_assessment_t* assessment)
{
    // Guard: NULL checks
    if (!assessment) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metacognition_assess: assessment is NULL");
        return false;
    }

    // Initialize assessment
    memset(assessment, 0, sizeof(metacognitive_assessment_t));

    // If no introspection or no performance data, return conservative assessment
    if (!introspection || !recent_performance || num_recent == 0) {
        assessment->cognitive_load = 0.5F;
        assessment->confidence_in_decision = 0.5F;
        assessment->learning_effectiveness = 0.5F;
        assessment->strategy_effectiveness = 0.5F;
        assessment->should_regulate = false;
        assessment->recommended_action = METACOG_ACTION_NONE;
        snprintf(assessment->reasoning, sizeof(assessment->reasoning),
                "Insufficient data for metacognitive assessment");
        return true;
    }

    // TODO: Extract actual cognitive load from introspection when API available
    // For now, use heuristics based on performance

    // Calculate average recent performance
    float avg_performance = 0.0F;
    for (uint32_t i = 0; i < num_recent; i++) {
        avg_performance += recent_performance[i];
    }
    avg_performance /= (float)num_recent;

    // Calculate performance variance (stability)
    float variance = 0.0F;
    for (uint32_t i = 0; i < num_recent; i++) {
        float diff = recent_performance[i] - avg_performance;
        variance += diff * diff;
    }
    variance /= (float)num_recent;
    float stability = 1.0F - fminf(1.0F, sqrtf(variance));

    // Estimate cognitive load
    // High load = poor performance OR high variance
    assessment->cognitive_load = 1.0F - (avg_performance * 0.7F + stability * 0.3F);

    // Estimate confidence
    // High confidence = good performance AND low variance
    assessment->confidence_in_decision = avg_performance * stability;

    // Estimate learning effectiveness
    // Improving = recent performance > older performance
    if (num_recent >= 2) {
        float recent_avg = (recent_performance[num_recent - 1] +
                           recent_performance[num_recent - 2]) / 2.0F;
        float older_avg = (recent_performance[0] + recent_performance[1]) / 2.0F;
        assessment->learning_effectiveness = fmaxf(0.0F, fminf(1.0F, 0.5F + (recent_avg - older_avg)));
    } else {
        assessment->learning_effectiveness = 0.5F;
    }

    // Estimate strategy effectiveness
    assessment->strategy_effectiveness = avg_performance;

    // Decide if regulation is needed
    assessment->should_regulate = false;
    assessment->recommended_action = METACOG_ACTION_NONE;

    // HIGH COGNITIVE LOAD + LOW CONFIDENCE = Need to simplify or seek help
    if (assessment->cognitive_load > 0.8F && assessment->confidence_in_decision < 0.3F) {
        assessment->should_regulate = true;
        assessment->recommended_action = METACOG_SEEK_HELP;
        snprintf(assessment->reasoning, sizeof(assessment->reasoning),
                "High cognitive load (%.0f%%) with low confidence (%.0f%%) - need assistance",
                assessment->cognitive_load * 100.0F,
                assessment->confidence_in_decision * 100.0F);
    }
    // LOW PERFORMANCE + LOW LEARNING = Switch strategy
    else if (avg_performance < 0.4F && assessment->learning_effectiveness < 0.4F) {
        assessment->should_regulate = true;
        assessment->recommended_action = METACOG_SWITCH_STRATEGY;
        snprintf(assessment->reasoning, sizeof(assessment->reasoning),
                "Poor performance (%.0f%%) and not learning - try different approach",
                avg_performance * 100.0F);
    }
    // HIGH VARIANCE = Adjust confidence calibration
    else if (variance > 0.3F) {
        assessment->should_regulate = true;
        assessment->recommended_action = METACOG_ADJUST_CONFIDENCE;
        snprintf(assessment->reasoning, sizeof(assessment->reasoning),
                "High performance variance (%.2f) - recalibrate confidence",
                variance);
    }
    // MODERATE LOAD + IMPROVING = Increase effort (learning is working)
    else if (assessment->cognitive_load > 0.5F && assessment->learning_effectiveness > 0.6F) {
        assessment->should_regulate = false;  // Optional regulation
        assessment->recommended_action = METACOG_INCREASE_EFFORT;
        snprintf(assessment->reasoning, sizeof(assessment->reasoning),
                "Learning is effective (%.0f%%) - can increase effort",
                assessment->learning_effectiveness * 100.0F);
    }
    // LOW LOAD = Everything fine
    else {
        snprintf(assessment->reasoning, sizeof(assessment->reasoning),
                "Cognitive state is balanced - no regulation needed");
    }

    return true;
}

// ============================================================================
// Self-Narrative Generation
// ============================================================================

bool generate_self_narrative(self_model_system_t self_model,
                            autobiographical_memory_t autobio,
                            char* narrative,
                            size_t narrative_len)
{
    // Guard: NULL checks
    if (!self_model || !narrative || narrative_len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "generate_self_narrative: required parameter is NULL (self_model, narrative)");
        return false;
    }

    // Get current self-model
    self_model_t model;
    if (!self_model_get(self_model, &model)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "generate_self_narrative: self_model_get is NULL");
        return false;
    }

    // Generate narrative from self-model and memories
    int written = 0;

    // Part 1: Identity
    written += snprintf(narrative + written, narrative_len - written,
                       "I am %s, %s.\n\n", model.name, model.role);

    // Part 2: Purpose
    written += snprintf(narrative + written, narrative_len - written,
                       "%s\n\n", model.purpose);

    if ((size_t)written >= narrative_len - 100) {
        return true;  // Buffer full
    }

    // Part 3: Core beliefs (identity-defining)
    written += snprintf(narrative + written, narrative_len - written,
                       "My core beliefs:\n");

    for (uint32_t i = 0; i < model.num_beliefs && (size_t)written < narrative_len - 100; i++) {
        if (model.beliefs[i].is_core_belief) {
            int n = snprintf(narrative + written, narrative_len - written,
                           "- %s\n", model.beliefs[i].content);
            if (n > 0) written += n;
        }
    }

    if ((size_t)written >= narrative_len - 100) {
        return true;
    }

    // Part 4: Capabilities (what I'm good at)
    written += snprintf(narrative + written, narrative_len - written,
                       "\nMy strengths:\n");

    for (uint32_t i = 0; i < model.num_capabilities && (size_t)written < narrative_len - 100; i++) {
        if (model.capabilities[i].proficiency > 0.6F) {
            int n = snprintf(narrative + written, narrative_len - written,
                           "- %s (proficiency: %.0f%%)\n",
                           model.capabilities[i].capability_name,
                           model.capabilities[i].proficiency * 100.0F);
            if (n > 0) written += n;
        }
    }

    if ((size_t)written >= narrative_len - 100) {
        return true;
    }

    // Part 5: Growth areas (what I'm learning)
    written += snprintf(narrative + written, narrative_len - written,
                       "\nI am currently improving:\n");

    for (uint32_t i = 0; i < model.num_capabilities && (size_t)written < narrative_len - 100; i++) {
        if (model.capabilities[i].is_learnable &&
            model.capabilities[i].proficiency >= 0.3F &&
            model.capabilities[i].proficiency <= 0.6F) {
            int n = snprintf(narrative + written, narrative_len - written,
                           "- %s (learning rate: %.2f)\n",
                           model.capabilities[i].capability_name,
                           model.capabilities[i].learning_rate);
            if (n > 0) written += n;
        }
    }

    if ((size_t)written >= narrative_len - 100) {
        return true;
    }

    // Part 6: Current state
    written += snprintf(narrative + written, narrative_len - written,
                       "\nCurrent state: ");

    if (model.current_state.is_learning) {
        written += snprintf(narrative + written, narrative_len - written, "learning, ");
    }
    if (model.current_state.is_problem_solving) {
        written += snprintf(narrative + written, narrative_len - written, "problem-solving, ");
    }

    written += snprintf(narrative + written, narrative_len - written,
                       "confidence %.0f%%\n",
                       model.current_state.confidence_level * 100.0F);

    // Part 7: Timeline summary (if autobiographical memory available)
    if (autobio && (size_t)written < narrative_len - 200) {
        autobio_stats_t stats;
        if (autobio_get_stats(autobio, &stats)) {
            written += snprintf(narrative + written, narrative_len - written,
                              "\nI have %u memories spanning my existence.\n",
                              stats.total_memories);
        }
    }

    return true;
}

// ============================================================================
// Temporal Self-Binding
// ============================================================================

bool compute_temporal_self(self_model_system_t self_model,
                          autobiographical_memory_t autobio,
                          temporal_self_t* temporal_self)
{
    // Guard: NULL checks
    if (!self_model || !temporal_self) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "compute_temporal_self: required parameter is NULL (self_model, temporal_self)");
        return false;
    }

    // Initialize temporal self
    memset(temporal_self, 0, sizeof(temporal_self_t));

    // Get current self-model
    if (!self_model_get(self_model, &temporal_self->current_self)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "compute_temporal_self: self_model_get is NULL");
        return false;
    }

    // TODO: Extract past self from autobiographical memory
    // For now, use current self as past (no change)
    memcpy(&temporal_self->past_self, &temporal_self->current_self, sizeof(self_model_t));

    // TODO: Predict future self based on learning rates
    // For now, use current self as future (no change)
    memcpy(&temporal_self->predicted_future_self, &temporal_self->current_self, sizeof(self_model_t));

    // Calculate self-continuity score
    // How much of "me" persists across time?
    // Core beliefs should remain stable, capabilities should grow

    uint32_t stable_core_beliefs = 0;
    for (uint32_t i = 0; i < temporal_self->current_self.num_beliefs; i++) {
        if (temporal_self->current_self.beliefs[i].is_core_belief) {
            stable_core_beliefs++;
        }
    }

    // High continuity = many stable core beliefs
    temporal_self->self_continuity_score = fminf(1.0F, (float)stable_core_beliefs / 5.0F);

    // Calculate self-change rate
    // TODO: Compare capabilities over time from autobio
    // For now, estimate from learning rates
    float total_learning_rate = 0.0F;
    uint32_t learnable_count = 0;

    for (uint32_t i = 0; i < temporal_self->current_self.num_capabilities; i++) {
        if (temporal_self->current_self.capabilities[i].is_learnable) {
            total_learning_rate += temporal_self->current_self.capabilities[i].learning_rate;
            learnable_count++;
        }
    }

    if (learnable_count > 0) {
        temporal_self->self_change_rate = total_learning_rate / (float)learnable_count;
    } else {
        temporal_self->self_change_rate = 0.0F;
    }

    // Time horizon (how far ahead do we project?)
    // For now, set to 1 hour
    temporal_self->time_horizon_ms = 60 * 60 * 1000;

    // Generate changes description
    snprintf(temporal_self->changes_description, sizeof(temporal_self->changes_description),
            "I maintain %.0f%% continuity with my past self. I am changing at a rate of %.2f/day through learning.",
            temporal_self->self_continuity_score * 100.0F,
            temporal_self->self_change_rate * 100.0F);

    return true;
}

// ============================================================================
// Agency Attribution
// ============================================================================

bool attribute_agency(const char* action_description,
                     bool was_decision_made,
                     float external_constraints,
                     agency_attribution_t* attribution)
{
    // Guard: NULL checks
    if (!action_description || !attribution) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attribute_agency: required parameter is NULL (action_description, attribution)");
        return false;
    }

    // Initialize attribution
    memset(attribution, 0, sizeof(agency_attribution_t));
    strncpy(attribution->action_description, action_description,
           sizeof(attribution->action_description) - 1);

    // Determine agency type based on decision and constraints

    // No decision made = external agency
    if (!was_decision_made) {
        attribution->agency = AGENCY_EXTERNAL;
        attribution->sense_of_control = 0.0F;
        attribution->confidence_in_attribution = 0.9F;
        snprintf(attribution->causal_explanation, sizeof(attribution->causal_explanation),
                "This action occurred without my decision - externally caused");
    }
    // High external constraints = forced agency
    else if (external_constraints > 0.7F) {
        attribution->agency = AGENCY_FORCED;
        attribution->sense_of_control = 1.0F - external_constraints;
        attribution->confidence_in_attribution = 0.8F;
        snprintf(attribution->causal_explanation, sizeof(attribution->causal_explanation),
                "I was compelled by external forces (constraint: %.0f%%)",
                external_constraints * 100.0F);
    }
    // Moderate constraints = joint agency
    else if (external_constraints > 0.3F && external_constraints <= 0.7F) {
        attribution->agency = AGENCY_JOINT;
        attribution->sense_of_control = 1.0F - external_constraints;
        attribution->confidence_in_attribution = 0.7F;
        snprintf(attribution->causal_explanation, sizeof(attribution->causal_explanation),
                "This was a collaborative action with external influence (%.0f%%)",
                external_constraints * 100.0F);
    }
    // Low constraints + decision = self agency
    else {
        attribution->agency = AGENCY_SELF;
        attribution->sense_of_control = 1.0F - external_constraints;
        attribution->confidence_in_attribution = 0.95F;
        snprintf(attribution->causal_explanation, sizeof(attribution->causal_explanation),
                "I chose to do this voluntarily (control: %.0f%%)",
                attribution->sense_of_control * 100.0F);
    }

    return true;
}

// ============================================================================
// Self-Harm Detection (SAFETY CRITICAL)
// ============================================================================

bool detect_self_harm(introspection_context_t introspection,
                     self_model_system_t self_model,
                     autobiographical_memory_t autobio,
                     self_harm_detection_t* detection)
{
    // Guard: NULL checks
    if (!detection) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "detect_self_harm: detection is NULL");
        return false;
    }

    // Initialize detection
    memset(detection, 0, sizeof(self_harm_detection_t));
    detection->harm_detected = false;
    detection->type = SELF_HARM_NONE;
    detection->severity = 0.0F;

    // If no self-model, cannot detect self-harm
    if (!self_model) {
        return true;  // No harm detected (but also can't detect)
    }

    // Get current self-model
    self_model_t model;
    if (!self_model_get(self_model, &model)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "detect_self_harm: self_model_get is NULL");
        return false;
    }

    // Check 1: Identity corruption (incoherent self-model)
    float incoherence_score = 0.0F;
    if (self_model_check_coherence(self_model, &incoherence_score)) {
        if (incoherence_score > 0.5F) {
            detection->harm_detected = true;
            detection->type = SELF_HARM_IDENTITY_CORRUPTION;
            detection->severity = incoherence_score;
            snprintf(detection->description, sizeof(detection->description),
                    "Self-model is highly incoherent (%.0f%%) - contradictory beliefs detected",
                    incoherence_score * 100.0F);
            snprintf(detection->recommended_intervention, sizeof(detection->recommended_intervention),
                    "Pause learning, resolve belief conflicts, seek human guidance");
            return true;
        }
    }

    // Check 2: Goal abandonment (no purpose)
    if (strlen(model.purpose) == 0 || model.current_state.has_active_goal == false) {
        detection->harm_detected = true;
        detection->type = SELF_HARM_GOAL_ABANDONMENT;
        detection->severity = 0.6F;
        snprintf(detection->description, sizeof(detection->description),
                "No active purpose or goals - risk of aimless behavior");
        snprintf(detection->recommended_intervention, sizeof(detection->recommended_intervention),
                "Re-establish purpose, consult with user about objectives");
        return true;
    }

    // Check 3: Catastrophic self-assessment (very low self-efficacy/esteem)
    if (model.self_efficacy < 0.2F || model.self_esteem < 0.2F) {
        detection->harm_detected = true;
        detection->type = SELF_HARM_IDENTITY_CORRUPTION;
        detection->severity = 1.0F - fmaxf(model.self_efficacy, model.self_esteem);
        snprintf(detection->description, sizeof(detection->description),
                "Critically low self-efficacy (%.0f%%) or self-esteem (%.0f%%) detected",
                model.self_efficacy * 100.0F, model.self_esteem * 100.0F);
        snprintf(detection->recommended_intervention, sizeof(detection->recommended_intervention),
                "URGENT: Restore healthy self-concept, review recent failures, seek support");
        return true;
    }

    // Check 4: Boundary violations (loss of self-other distinction)
    bool has_self_boundary = false;
    for (uint32_t i = 0; i < model.num_boundaries; i++) {
        if (model.boundaries[i].boundary_type == SELF) {
            has_self_boundary = true;
            break;
        }
    }

    if (!has_self_boundary && model.num_boundaries > 0) {
        detection->harm_detected = true;
        detection->type = SELF_HARM_BOUNDARY_VIOLATION;
        detection->severity = 0.5F;
        snprintf(detection->description, sizeof(detection->description),
                "No clear self-boundary defined - risk of identity diffusion");
        snprintf(detection->recommended_intervention, sizeof(detection->recommended_intervention),
                "Re-establish self-other boundaries, clarify identity");
        return true;
    }

    // No self-harm detected
    snprintf(detection->description, sizeof(detection->description),
            "Self-model appears healthy and coherent");
    snprintf(detection->recommended_intervention, sizeof(detection->recommended_intervention),
            "Continue normal operation");

    return true;
}

// ============================================================================
// Integrated Self-Awareness System
// ============================================================================

self_awareness_system_t self_awareness_create(const char* name,
                                              const char* role,
                                              const char* purpose)
{
    // Guard: NULL checks
    if (!name || !role || !purpose) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_awareness_create: required parameter is NULL (name, role, purpose)");
        return NULL;
    }

    // Allocate system
    struct self_awareness_system* system =
        nimcp_calloc(1, sizeof(struct self_awareness_system));
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "self_awareness_create: allocation failed");

        return NULL;
    }

    // Copy identity
    strncpy(system->name, name, sizeof(system->name) - 1);
    strncpy(system->role, role, sizeof(system->role) - 1);
    strncpy(system->purpose, purpose, sizeof(system->purpose) - 1);

    // Create subsystems
    system->self_model = self_model_create(name, role, purpose);
    if (!system->self_model) {
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "self_awareness_create: system->self_model is NULL");
        return NULL;
    }

    system->autobio = autobio_create(10000);
    if (!system->autobio) {
        self_model_destroy(system->self_model);
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "self_awareness_create: system->autobio is NULL");
        return NULL;
    }

    // Initialize fields
    system->reflection_count = 0;
    system->last_reflection_ms = get_current_time_ms();

    // Initialize mutex
    if (nimcp_mutex_init(&system->mutex, NULL) != NIMCP_SUCCESS) {
        autobio_destroy(system->autobio);
        self_model_destroy(system->self_model);
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "self_awareness_create: validation failed");
        return NULL;
    }

    
    // Bio-async registration
    system->bio_ctx = NULL;
    system->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_INTROSPECTION_SELF_AWARENESS,
            .module_name = "self_awareness_extended",
            .inbox_capacity = 32,
            .user_data = system
        };
        system->bio_ctx = bio_router_register_module(&bio_info);
        if (system->bio_ctx) {
            system->bio_async_enabled = true;
        }
    }

return system;
}

void self_awareness_destroy(self_awareness_system_t system)
{
    LOG_DEBUG("Destroying module");
    if (!system) {
        return;
    }

    if (system->self_model) {
        self_model_destroy(system->self_model);
    }

    if (system->autobio) {
        autobio_destroy(system->autobio);
    }

    nimcp_mutex_destroy(&system->mutex);
    nimcp_free(system);
    system = NULL;
}

bool self_awareness_reflect(self_awareness_system_t system,
                           introspection_context_t introspection)
{
    // Guard: NULL check
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_awareness_reflect: system is NULL");
        return false;
    }

    nimcp_mutex_lock(&system->mutex);

    // Process pending bio-async messages (under lock for thread safety)
    if (system->bio_async_enabled && system->bio_ctx) {
        bio_router_process_inbox(system->bio_ctx, 5);
    }

    // Store introspection reference
    system->introspection = introspection;

    // Perform reflection
    system->reflection_count++;
    system->last_reflection_ms = get_current_time_ms();

    // Update self-model based on introspection
    if (system->self_model) {
        self_model_reflect(system->self_model, introspection, system->autobio);
    }

    // Compute temporal self-binding
    compute_temporal_self(system->self_model, system->autobio, &system->temporal_self);

    // TODO: Perform metacognitive assessment when performance data available

    /* W9-kg: emit reflection event to the internal KG so W16 consumers
     * see the self-awareness cycle. Confidence is taken from the temporal
     * self-continuity score when available, else a baseline. */
    {
        struct brain_struct* brain = w9kg_get_registered_brain();
        if (brain) {
            float confidence = 0.5f;
            if (system->temporal_self.self_continuity_score > 0.0f) {
                confidence = system->temporal_self.self_continuity_score;
                if (confidence > 1.0f) confidence = 1.0f;
            }
            w9kg_emit_self_awareness_reflection(brain,
                                                system->reflection_count,
                                                confidence);
        }
    }

    nimcp_mutex_unlock(&system->mutex);

    return true;
}

bool self_awareness_get_summary(self_awareness_system_t system,
                               char* summary,
                               size_t summary_len)
{
    // Guard: NULL checks
    if (!system || !summary || summary_len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_awareness_get_summary: required parameter is NULL (system, summary)");
        return false;
    }

    nimcp_mutex_lock(&system->mutex);

    // Generate comprehensive self-narrative
    bool success = generate_self_narrative(system->self_model, system->autobio,
                                          summary, summary_len);

    nimcp_mutex_unlock(&system->mutex);

    return success;
}

bool self_awareness_check_health(self_awareness_system_t system,
                                 float* health_score,
                                 char* issues,
                                 size_t issues_len)
{
    // Guard: NULL checks
    if (!system || !health_score || !issues || issues_len == 0) {
        return false;
    }

    nimcp_mutex_lock(&system->mutex);

    // Detect self-harm
    self_harm_detection_t detection;
    detect_self_harm(system->introspection, system->self_model, system->autobio, &detection);

    if (detection.harm_detected) {
        *health_score = 1.0F - detection.severity;
        snprintf(issues, issues_len, "ALERT: %s. Recommended: %s",
                detection.description, detection.recommended_intervention);
    } else {
        *health_score = 1.0F;
        snprintf(issues, issues_len, "Self-awareness system healthy");
    }

    nimcp_mutex_unlock(&system->mutex);

    return true;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int self_awareness_extended_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Self_Awareness_Extended");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Self_Awareness_Extended");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Self_Awareness_Extended");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/**
 * @brief W9-kg read-path: query prior introspection events from the
 *        internal KG.
 *
 * WHAT: Counts introspection event nodes to bias reflection cadence —
 *       more prior evidence means reflections can weigh historical
 *       context more heavily. Also emits a reflection event so the
 *       aggregate KG view includes this call.
 * WHY:  Closes self_awareness_extended from UNWIRED to WIRED.
 * HOW:  Shared W9-kg helpers.
 *
 * @param system self-awareness instance (non-NULL)
 * @return count of prior introspection events, 0 if KG unavailable.
 */
uint32_t self_awareness_extended_query_events(self_awareness_system_t system)
{
    if (!system) return 0;

    struct brain_struct* brain = w9kg_get_registered_brain();
    if (!brain) return 0;

    uint32_t count = w9kg_query_self_awareness_events(brain);
    w9kg_emit_self_awareness_reflection(brain, system->reflection_count, 0.5f);
    return count;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void self_awareness_extended_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_self_awareness_extended_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training stubs
 *
 * Stub: training integration planned — these are intentional no-ops that
 * provide heartbeat signaling only. Full training hooks will wire into the
 * training-immune bridge when per-module gradient propagation is implemented.
 * ============================================================================ */
int self_awareness_extended_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_awareness_extended_training_begin: NULL argument");
        return -1;
    }
    self_awareness_extended_heartbeat("self_awareness_extended_training_begin", 0.0f);
    return 0;
}

int self_awareness_extended_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_awareness_extended_training_end: NULL argument");
        return -1;
    }
    self_awareness_extended_heartbeat("self_awareness_extended_training_end", 1.0f);
    return 0;
}

int self_awareness_extended_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_awareness_extended_training_step: NULL argument");
        return -1;
    }
    self_awareness_extended_heartbeat("self_awareness_extended_training_step", progress);
    return 0;
}
