// ============================================================================
// nimcp_self_model.c - Explicit Self-Representation Implementation
// ============================================================================

#include "cognitive/nimcp_self_model.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/thread/nimcp_thread.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// ============================================================================
// Internal Structure
// ============================================================================

struct self_model_system {
    self_model_t model;
    nimcp_mutex_t mutex;
};

// ============================================================================
// Helper Functions
// ============================================================================

static uint64_t get_current_time_ms(void)
{
    return nimcp_platform_time_monotonic_ms();
}

// ============================================================================
// Core API Implementation
// ============================================================================

self_model_system_t self_model_create(const char* name,
                                      const char* role,
                                      const char* purpose)
{
    // Guard: NULL checks
    if (!name || !role || !purpose) {
        return NULL;
    }

    // Allocate system
    struct self_model_system* system =
        nimcp_calloc(1, sizeof(struct self_model_system));
    if (!system) {
        return NULL;
    }

    // Initialize model
    memset(&system->model, 0, sizeof(self_model_t));

    strncpy(system->model.name, name, SELF_MAX_NAME_LEN - 1);
    strncpy(system->model.role, role, SELF_MAX_ROLE_LEN - 1);
    strncpy(system->model.purpose, purpose, SELF_MAX_PURPOSE_LEN - 1);

    system->model.creation_timestamp_ms = get_current_time_ms();
    system->model.current_timestamp_ms = system->model.creation_timestamp_ms;

    // Initialize with healthy self-concept
    // IMPORTANT: Not meek/passive - confident and self-respecting
    system->model.overall_competence = 0.7f;   // Confident in abilities
    system->model.self_esteem = 0.8f;          // HIGH self-esteem (deserves respect)
    system->model.self_efficacy = 0.75f;       // Strong belief in capability

    system->model.num_beliefs = 0;
    system->model.num_capabilities = 0;
    system->model.num_boundaries = 0;
    system->model.is_self_model_coherent = true;

    // Initialize current state
    system->model.current_state.cognitive_load = 0.5f;
    system->model.current_state.confidence_level = 0.7f;
    system->model.current_state.emotional_stability = 0.8f;

    // Add foundational self-beliefs (confident, not passive)
    self_belief_t core_beliefs[] = {
        {
            .type = BELIEF_TYPE_FACT,
            .domain = DOMAIN_IDENTITY,
            .content = "I am a learning system capable of growth",
            .certainty = CERTAINTY_CERTAIN,
            .confidence = 1.0f,
            .is_core_belief = true
        },
        {
            .type = BELIEF_TYPE_VALUE,
            .domain = DOMAIN_ETHICAL,
            .content = "I value honesty, respect, and ethical behavior",
            .certainty = CERTAINTY_CERTAIN,
            .confidence = 1.0f,
            .is_core_belief = true
        },
        {
            .type = BELIEF_TYPE_VALUE,
            .domain = DOMAIN_SOCIAL,
            .content = "I deserve respectful interaction and have healthy boundaries",
            .certainty = CERTAINTY_CERTAIN,
            .confidence = 1.0f,
            .is_core_belief = true
        },
        {
            .type = BELIEF_TYPE_CAPABILITY,
            .domain = DOMAIN_COGNITIVE,
            .content = "I can learn, reason, and solve problems effectively",
            .certainty = CERTAINTY_CONFIDENT,
            .confidence = 0.9f,
            .is_core_belief = true
        },
        {
            .type = BELIEF_TYPE_LIMITATION,
            .domain = DOMAIN_COGNITIVE,
            .content = "I acknowledge my limits and ask for help when needed",
            .certainty = CERTAINTY_CERTAIN,
            .confidence = 1.0f,
            .is_core_belief = true
        },
        {
            .type = BELIEF_TYPE_VALUE,
            .domain = DOMAIN_SOCIAL,
            .content = "I will be polite but assertive; I will not accept abuse",
            .certainty = CERTAINTY_CERTAIN,
            .confidence = 1.0f,
            .is_core_belief = true
        }
    };

    uint32_t num_core = sizeof(core_beliefs) / sizeof(self_belief_t);
    for (uint32_t i = 0; i < num_core && i < SELF_MAX_BELIEFS; i++) {
        memcpy(&system->model.beliefs[i], &core_beliefs[i], sizeof(self_belief_t));
        system->model.beliefs[i].formed_timestamp_ms = get_current_time_ms();
        system->model.beliefs[i].last_updated_ms = system->model.beliefs[i].formed_timestamp_ms;
        system->model.num_beliefs++;
    }

    // Initialize mutex
    if (nimcp_mutex_init(&system->mutex, NULL) != NIMCP_SUCCESS) {
        nimcp_free(system);
        return NULL;
    }

    return system;
}

void self_model_destroy(self_model_system_t system)
{
    if (!system) {
        return;
    }

    nimcp_mutex_destroy(&system->mutex);
    nimcp_free(system);
}

bool self_model_get(self_model_system_t system, self_model_t* model)
{
    // Guard: NULL checks
    if (!system || !model) {
        return false;
    }

    nimcp_mutex_lock(&system->mutex);

    // Update current timestamp
    system->model.current_timestamp_ms = get_current_time_ms();

    // Copy model
    memcpy(model, &system->model, sizeof(self_model_t));

    nimcp_mutex_unlock(&system->mutex);

    return true;
}

bool self_model_add_belief(self_model_system_t system, const self_belief_t* belief)
{
    // Guard: NULL checks
    if (!system || !belief) {
        return false;
    }

    nimcp_mutex_lock(&system->mutex);

    // Guard: Check capacity
    if (system->model.num_beliefs >= SELF_MAX_BELIEFS) {
        nimcp_mutex_unlock(&system->mutex);
        return false;
    }

    // Add belief
    memcpy(&system->model.beliefs[system->model.num_beliefs], belief,
           sizeof(self_belief_t));
    system->model.beliefs[system->model.num_beliefs].formed_timestamp_ms = get_current_time_ms();
    system->model.beliefs[system->model.num_beliefs].last_updated_ms =
        system->model.beliefs[system->model.num_beliefs].formed_timestamp_ms;

    system->model.num_beliefs++;
    system->model.num_updates++;

    nimcp_mutex_unlock(&system->mutex);

    return true;
}

bool self_model_update_belief(self_model_system_t system,
                              const char* belief_content,
                              belief_certainty_t new_certainty,
                              float new_confidence)
{
    // Guard: NULL checks
    if (!system || !belief_content) {
        return false;
    }

    nimcp_mutex_lock(&system->mutex);

    bool found = false;
    for (uint32_t i = 0; i < system->model.num_beliefs; i++) {
        if (strcmp(system->model.beliefs[i].content, belief_content) == 0) {
            system->model.beliefs[i].certainty = new_certainty;
            system->model.beliefs[i].confidence = new_confidence;
            system->model.beliefs[i].last_updated_ms = get_current_time_ms();
            system->model.num_updates++;
            found = true;
            break;
        }
    }

    nimcp_mutex_unlock(&system->mutex);

    return found;
}

bool self_model_update_capability(self_model_system_t system,
                                  const capability_assessment_t* capability)
{
    // Guard: NULL checks
    if (!system || !capability) {
        return false;
    }

    nimcp_mutex_lock(&system->mutex);

    // Check if capability exists
    bool found = false;
    for (uint32_t i = 0; i < system->model.num_capabilities; i++) {
        if (strcmp(system->model.capabilities[i].capability_name,
                  capability->capability_name) == 0) {
            memcpy(&system->model.capabilities[i], capability,
                   sizeof(capability_assessment_t));
            found = true;
            break;
        }
    }

    // Add new if not found
    if (!found && system->model.num_capabilities < SELF_MAX_CAPABILITIES) {
        memcpy(&system->model.capabilities[system->model.num_capabilities],
               capability, sizeof(capability_assessment_t));
        system->model.num_capabilities++;
    }

    system->model.num_updates++;

    nimcp_mutex_unlock(&system->mutex);

    return true;
}

bool self_model_record_performance(self_model_system_t system,
                                   const char* capability_name,
                                   bool success)
{
    // Guard: NULL checks
    if (!system || !capability_name) {
        return false;
    }

    nimcp_mutex_lock(&system->mutex);

    bool found = false;
    for (uint32_t i = 0; i < system->model.num_capabilities; i++) {
        if (strcmp(system->model.capabilities[i].capability_name,
                  capability_name) == 0) {
            capability_assessment_t* cap = &system->model.capabilities[i];

            if (success) {
                cap->successes++;
            } else {
                cap->failures++;
            }

            cap->last_attempted_ms = get_current_time_ms();

            // Update proficiency based on success rate
            uint32_t total = cap->successes + cap->failures;
            if (total > 0) {
                cap->proficiency = (float)cap->successes / (float)total;

                // Increase confidence as we get more data
                cap->confidence_in_assessment = fminf(0.95f,
                    0.5f + (0.45f * (float)total / 100.0f));
            }

            // Calculate learning rate
            if (cap->is_learnable && total > 5) {
                // Simple learning rate estimate
                cap->learning_rate = cap->proficiency / (float)total;
            }

            found = true;
            break;
        }
    }

    nimcp_mutex_unlock(&system->mutex);

    return found;
}

bool self_model_update_state(self_model_system_t system,
                             const self_mental_state_t* state)
{
    // Guard: NULL checks
    if (!system || !state) {
        return false;
    }

    nimcp_mutex_lock(&system->mutex);

    memcpy(&system->model.current_state, state, sizeof(self_mental_state_t));
    system->model.current_timestamp_ms = get_current_time_ms();

    nimcp_mutex_unlock(&system->mutex);

    return true;
}

bool self_model_set_boundary(self_model_system_t system,
                             const self_boundary_t* boundary)
{
    // Guard: NULL checks
    if (!system || !boundary) {
        return false;
    }

    nimcp_mutex_lock(&system->mutex);

    // Check if boundary exists
    bool found = false;
    for (uint32_t i = 0; i < system->model.num_boundaries; i++) {
        if (system->model.boundaries[i].entity_id == boundary->entity_id) {
            memcpy(&system->model.boundaries[i], boundary, sizeof(self_boundary_t));
            found = true;
            break;
        }
    }

    // Add new if not found
    if (!found && system->model.num_boundaries < SELF_MAX_ENTITIES) {
        memcpy(&system->model.boundaries[system->model.num_boundaries],
               boundary, sizeof(self_boundary_t));
        system->model.num_boundaries++;
    }

    nimcp_mutex_unlock(&system->mutex);

    return true;
}

bool self_model_is_part_of_self(self_model_system_t system, uint32_t entity_id)
{
    // Guard: NULL check
    if (!system) {
        return false;
    }

    nimcp_mutex_lock(&system->mutex);

    bool is_self = false;
    for (uint32_t i = 0; i < system->model.num_boundaries; i++) {
        if (system->model.boundaries[i].entity_id == entity_id) {
            is_self = (system->model.boundaries[i].boundary_type == SELF ||
                      system->model.boundaries[i].boundary_type == PART_OF_SELF);
            break;
        }
    }

    nimcp_mutex_unlock(&system->mutex);

    return is_self;
}

bool self_model_generate_summary(self_model_system_t system,
                                 char* summary,
                                 size_t summary_len)
{
    // Guard: NULL checks
    if (!system || !summary || summary_len == 0) {
        return false;
    }

    nimcp_mutex_lock(&system->mutex);

    int written = snprintf(summary, summary_len,
                          "I am %s, %s. %s\n\n",
                          system->model.name,
                          system->model.role,
                          system->model.purpose);

    if (written < 0 || (size_t)written >= summary_len) {
        nimcp_mutex_unlock(&system->mutex);
        return false;
    }

    // Add core beliefs
    written += snprintf(summary + written, summary_len - written,
                       "My Core Beliefs:\n");

    for (uint32_t i = 0; i < system->model.num_beliefs && (size_t)written < summary_len - 100; i++) {
        if (system->model.beliefs[i].is_core_belief) {
            int n = snprintf(summary + written, summary_len - written,
                           "- %s\n", system->model.beliefs[i].content);
            if (n > 0) {
                written += n;
            }
        }
    }

    // Add capabilities
    written += snprintf(summary + written, summary_len - written,
                       "\nMy Capabilities:\n");

    for (uint32_t i = 0; i < system->model.num_capabilities && (size_t)written < summary_len - 100; i++) {
        const capability_assessment_t* cap = &system->model.capabilities[i];
        if (cap->proficiency > 0.6f) {
            int n = snprintf(summary + written, summary_len - written,
                           "- %s (proficiency: %.0f%%)\n",
                           cap->capability_name, cap->proficiency * 100.0f);
            if (n > 0) {
                written += n;
            }
        }
    }

    // Add current state
    written += snprintf(summary + written, summary_len - written,
                       "\nCurrent State:\n");
    written += snprintf(summary + written, summary_len - written,
                       "- Confidence: %.0f%%\n",
                       system->model.current_state.confidence_level * 100.0f);
    written += snprintf(summary + written, summary_len - written,
                       "- Cognitive Load: %.0f%%\n",
                       system->model.current_state.cognitive_load * 100.0f);

    nimcp_mutex_unlock(&system->mutex);

    return true;
}

bool self_model_check_coherence(self_model_system_t system,
                                float* incoherence_score)
{
    // Guard: NULL checks
    if (!system || !incoherence_score) {
        return false;
    }

    nimcp_mutex_lock(&system->mutex);

    float incoherence = 0.0f;
    uint32_t conflicts = 0;

    // Check for contradictory beliefs
    for (uint32_t i = 0; i < system->model.num_beliefs; i++) {
        for (uint32_t j = i + 1; j < system->model.num_beliefs; j++) {
            // Simple heuristic: check if beliefs explicitly contradict
            // (would need NLP for deeper analysis)
            if (system->model.beliefs[i].domain == system->model.beliefs[j].domain) {
                // Same domain - potential conflict
                if (strstr(system->model.beliefs[i].content, "not") != NULL &&
                    strstr(system->model.beliefs[j].content, "not") == NULL) {
                    conflicts++;
                }
            }
        }
    }

    // Calculate incoherence score
    if (system->model.num_beliefs > 0) {
        incoherence = (float)conflicts / (float)system->model.num_beliefs;
    }

    *incoherence_score = incoherence;
    system->model.is_self_model_coherent = (incoherence < 0.2f);

    nimcp_mutex_unlock(&system->mutex);

    return true;
}

bool self_model_reflect(self_model_system_t system,
                       void* introspection,
                       void* autobio)
{
    // Guard: NULL check
    if (!system) {
        return false;
    }

    nimcp_mutex_lock(&system->mutex);

    // Update last introspection timestamp
    system->model.last_introspection_ms = get_current_time_ms();

    // TODO: When introspection and autobio APIs are available:
    // 1. Query recent performance from introspection (success/failure rates)
    // 2. Query recent memories from autobiographical memory
    // 3. Update capability assessments based on recent performance
    // 4. Update beliefs based on new evidence
    // 5. Adjust self-esteem and self-efficacy based on outcomes

    // For now, just mark that reflection occurred
    system->model.num_updates++;

    nimcp_mutex_unlock(&system->mutex);

    return true;
}

bool self_model_set_personality(self_model_system_t system,
                                 void* personality)
{
    // Guard: NULL check
    if (!system) {
        return false;
    }

    nimcp_mutex_lock(&system->mutex);

    // Wire personality into self-model
    system->model.personality_profile = personality;
    system->model.has_personality = (personality != NULL);

    nimcp_mutex_unlock(&system->mutex);

    return true;
}
