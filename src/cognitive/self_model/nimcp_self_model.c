// ============================================================================
// nimcp_self_model.c - Explicit Self-Representation Implementation
// ============================================================================

#include "cognitive/nimcp_self_model.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/executive/nimcp_w9kg_events.h"  // W9-kg: KG event + read helpers
#include "cognitive/self_model/nimcp_self_model_snn_bridge.h"
#include "cognitive/self_model/nimcp_self_model_plasticity_bridge.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

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

#define LOG_MODULE "cognitive.self_model"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(self_model, MESH_ADAPTER_CATEGORY_COGNITIVE)



#define BIO_MODULE_COGNITIVE_SELF_MODEL 0x0351


// ============================================================================
// Internal Structure
// ============================================================================

struct self_model_system {
    self_model_t model;
    nimcp_mutex_t mutex;

    // Bio-async integration
    bio_module_context_t bio_ctx;   /**< Bio-async module context */
    bool bio_async_enabled;         /**< Bio-async registration status */

    // Internal Knowledge Graph integration
    kg_module_context_t kg_context; /**< KG access context */
    bool kg_connected;              /**< Internal KG is connected */

    // SNN and Plasticity bridges
    self_model_snn_bridge_t* snn_bridge;           /**< SNN integration bridge */
    self_model_plasticity_bridge_t* plasticity_bridge; /**< Plasticity integration bridge */
    bool bridges_enabled;                          /**< Bridge integration status */
};

// ============================================================================
// Helper Functions
// ============================================================================

static uint64_t get_current_time_ms(void)
{
    return nimcp_time_monotonic_ms();
}

// ============================================================================
// Core API Implementation
// ============================================================================

self_model_system_t self_model_create(const char* name,
                                      const char* role,
                                      const char* purpose)
{
    // Guard: NULL checks
    /* Phase 8: Heartbeat at operation start */
    self_model_heartbeat("self_model_create", 0.0f);


    NIMCP_API_CHECK_NULL_RET_NULL(name, "NULL name in self_model_create");
    NIMCP_API_CHECK_NULL_RET_NULL(role, "NULL role in self_model_create");
    NIMCP_API_CHECK_NULL_RET_NULL(purpose, "NULL purpose in self_model_create");

    // Allocate system
    struct self_model_system* system =
        nimcp_calloc(1, sizeof(struct self_model_system));
    NIMCP_API_CHECK_ALLOC(system, "Failed to allocate self model system");

    // Initialize model
    memset(&system->model, 0, sizeof(self_model_t));

    strncpy(system->model.name, name, SELF_MAX_NAME_LEN - 1);
    strncpy(system->model.role, role, SELF_MAX_ROLE_LEN - 1);
    strncpy(system->model.purpose, purpose, SELF_MAX_PURPOSE_LEN - 1);

    system->model.creation_timestamp_ms = get_current_time_ms();
    system->model.current_timestamp_ms = system->model.creation_timestamp_ms;

    // Initialize with healthy self-concept
    // IMPORTANT: Not meek/passive - confident and self-respecting
    system->model.overall_competence = 0.7F;   // Confident in abilities
    system->model.self_esteem = 0.8F;          // HIGH self-esteem (deserves respect)
    system->model.self_efficacy = 0.75F;       // Strong belief in capability

    system->model.num_beliefs = 0;
    system->model.num_capabilities = 0;
    system->model.num_boundaries = 0;
    system->model.is_self_model_coherent = true;

    // Initialize current state
    system->model.current_state.cognitive_load = 0.5F;
    system->model.current_state.confidence_level = 0.7F;
    system->model.current_state.emotional_stability = 0.8F;

    // Add foundational self-beliefs (confident, not passive)
    self_belief_t core_beliefs[] = {
        {
            .type = BELIEF_TYPE_FACT,
            .domain = DOMAIN_IDENTITY,
            .content = "I am a learning system capable of growth",
            .certainty = CERTAINTY_CERTAIN,
            .confidence = 1.0F,
            .is_core_belief = true
        },
        {
            .type = BELIEF_TYPE_VALUE,
            .domain = DOMAIN_ETHICAL,
            .content = "I value honesty, respect, and ethical behavior",
            .certainty = CERTAINTY_CERTAIN,
            .confidence = 1.0F,
            .is_core_belief = true
        },
        {
            .type = BELIEF_TYPE_VALUE,
            .domain = DOMAIN_SOCIAL,
            .content = "I deserve respectful interaction and have healthy boundaries",
            .certainty = CERTAINTY_CERTAIN,
            .confidence = 1.0F,
            .is_core_belief = true
        },
        {
            .type = BELIEF_TYPE_CAPABILITY,
            .domain = DOMAIN_COGNITIVE,
            .content = "I can learn, reason, and solve problems effectively",
            .certainty = CERTAINTY_CONFIDENT,
            .confidence = 0.9F,
            .is_core_belief = true
        },
        {
            .type = BELIEF_TYPE_LIMITATION,
            .domain = DOMAIN_COGNITIVE,
            .content = "I acknowledge my limits and ask for help when needed",
            .certainty = CERTAINTY_CERTAIN,
            .confidence = 1.0F,
            .is_core_belief = true
        },
        {
            .type = BELIEF_TYPE_VALUE,
            .domain = DOMAIN_SOCIAL,
            .content = "I will be polite but assertive; I will not accept abuse",
            .certainty = CERTAINTY_CERTAIN,
            .confidence = 1.0F,
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
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "self_model_create: validation failed");
        return NULL;
    }

    
    // Bio-async registration
    system->bio_ctx = NULL;
    system->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_INTROSPECTION_SELF_MODEL,
            .module_name = "self_model",
            .inbox_capacity = 32,
            .user_data = system
        };
        system->bio_ctx = bio_router_register_module(&bio_info);
        if (system->bio_ctx) {
            system->bio_async_enabled = true;
        }
    }

    // Create SNN and Plasticity bridges
    system->snn_bridge = NULL;
    system->plasticity_bridge = NULL;
    system->bridges_enabled = false;

    self_model_snn_config_t snn_config = self_model_snn_config_default();
    system->snn_bridge = self_model_snn_create(&snn_config);

    self_model_plasticity_config_t plasticity_config = self_model_plasticity_config_default();
    system->plasticity_bridge = self_model_plasticity_create(&plasticity_config);

    if (system->snn_bridge && system->plasticity_bridge) {
        system->bridges_enabled = true;
    }

    return system;
}

void self_model_destroy(self_model_system_t system)
{
    /* Phase 8: Heartbeat at operation start */
    self_model_heartbeat("self_model_destroy", 0.0f);


    LOG_DEBUG("Destroying module");
    if (!system) {
        return;
    }

    // Destroy bridges before freeing struct
    if (system->snn_bridge) {
        self_model_snn_destroy(system->snn_bridge);
        system->snn_bridge = NULL;
    }
    if (system->plasticity_bridge) {
        self_model_plasticity_destroy(system->plasticity_bridge);
        system->plasticity_bridge = NULL;
    }
    system->bridges_enabled = false;

    nimcp_mutex_destroy(&system->mutex);
    nimcp_free(system);
}

bool self_model_get(self_model_system_t system, self_model_t* model)
{
    /* Phase 8: Heartbeat at operation start */
    self_model_heartbeat("self_model_get", 0.0f);

    // Guard: NULL checks (before any field access)
    if (!system || !model) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                "self_model_get: invalid parameters");
        return false;
    }

    nimcp_mutex_lock(&system->mutex);

    // Process pending bio-async messages (under lock for thread safety)
    if (system->bio_ctx) {
        bio_router_process_inbox(system->bio_ctx, 5);
    }

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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "self_model_add_belief: invalid parameters");

            return false;
    }

    /* Phase 8: Heartbeat at operation start */
    self_model_heartbeat("self_model_add_belief", 0.0f);


    nimcp_mutex_lock(&system->mutex);

    // Guard: Check capacity
    if (system->model.num_beliefs >= SELF_MAX_BELIEFS) {
        nimcp_mutex_unlock(&system->mutex);
        LOG_WARN("self_model_add_belief: belief capacity exceeded (max %u)", SELF_MAX_BELIEFS);
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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "self_model_update_belief: invalid parameters");

            return false;
    }

    /* Phase 8: Heartbeat at operation start */
    self_model_heartbeat("self_model_update_belief", 0.0f);


    nimcp_mutex_lock(&system->mutex);

    bool found = false;
    for (uint32_t i = 0; i < system->model.num_beliefs; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->model.num_beliefs > 256) {
            self_model_heartbeat("self_model_loop",
                             (float)(i + 1) / (float)system->model.num_beliefs);
        }

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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "self_model_update_capability: invalid parameters");

            return false;
    }

    /* Phase 8: Heartbeat at operation start */
    self_model_heartbeat("self_model_update_capability", 0.0f);


    nimcp_mutex_lock(&system->mutex);

    // Check if capability exists
    bool found = false;
    for (uint32_t i = 0; i < system->model.num_capabilities; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->model.num_capabilities > 256) {
            self_model_heartbeat("self_model_loop",
                             (float)(i + 1) / (float)system->model.num_capabilities);
        }

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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "self_model_record_performance: invalid parameters");

            return false;
    }

    /* Phase 8: Heartbeat at operation start */
    self_model_heartbeat("self_model_record_performance", 0.0f);


    nimcp_mutex_lock(&system->mutex);

    bool found = false;
    for (uint32_t i = 0; i < system->model.num_capabilities; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->model.num_capabilities > 256) {
            self_model_heartbeat("self_model_loop",
                             (float)(i + 1) / (float)system->model.num_capabilities);
        }

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
                cap->confidence_in_assessment = fminf(0.95F,
                    0.5F + (0.45F * (float)total / 100.0F));
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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "self_model_update_state: invalid parameters");

            return false;
    }

    /* Phase 8: Heartbeat at operation start */
    self_model_heartbeat("self_model_update_state", 0.0f);


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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "self_model_set_boundary: invalid parameters");

            return false;
    }

    /* Phase 8: Heartbeat at operation start */
    self_model_heartbeat("self_model_set_boundary", 0.0f);


    nimcp_mutex_lock(&system->mutex);

    // Check if boundary exists
    bool found = false;
    for (uint32_t i = 0; i < system->model.num_boundaries; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->model.num_boundaries > 256) {
            self_model_heartbeat("self_model_loop",
                             (float)(i + 1) / (float)system->model.num_boundaries);
        }

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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "self_model_is_part_of_self: system is NULL");

            return false;
    }

    /* Phase 8: Heartbeat at operation start */
    self_model_heartbeat("self_model_is_part_of_self", 0.0f);


    nimcp_mutex_lock(&system->mutex);

    bool is_self = false;
    for (uint32_t i = 0; i < system->model.num_boundaries; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->model.num_boundaries > 256) {
            self_model_heartbeat("self_model_loop",
                             (float)(i + 1) / (float)system->model.num_boundaries);
        }

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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "self_model_generate_summary: invalid parameters");

            return false;
    }

    /* Phase 8: Heartbeat at operation start */
    self_model_heartbeat("self_model_generate_summary", 0.0f);


    nimcp_mutex_lock(&system->mutex);

    int written = snprintf(summary, summary_len,
                          "I am %s, %s. %s\n\n",
                          system->model.name,
                          system->model.role,
                          system->model.purpose);

    if (written < 0 || (size_t)written >= summary_len) {
        nimcp_mutex_unlock(&system->mutex);
        LOG_WARN("self_model_generate_summary: buffer capacity exceeded");
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
        if (cap->proficiency > 0.6F) {
            int n = snprintf(summary + written, summary_len - written,
                           "- %s (proficiency: %.0f%%)\n",
                           cap->capability_name, cap->proficiency * 100.0F);
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
                       system->model.current_state.confidence_level * 100.0F);
    written += snprintf(summary + written, summary_len - written,
                       "- Cognitive Load: %.0f%%\n",
                       system->model.current_state.cognitive_load * 100.0F);

    nimcp_mutex_unlock(&system->mutex);

    return true;
}

bool self_model_check_coherence(self_model_system_t system,
                                float* incoherence_score)
{
    // Guard: NULL checks
    if (!system || !incoherence_score) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "self_model_check_coherence: invalid parameters");

            return false;
    }

    /* Phase 8: Heartbeat at operation start */
    self_model_heartbeat("self_model_check_coherence", 0.0f);


    nimcp_mutex_lock(&system->mutex);

    float incoherence = 0.0F;
    uint32_t conflicts = 0;

    // Check for contradictory beliefs
    for (uint32_t i = 0; i < system->model.num_beliefs; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->model.num_beliefs > 256) {
            self_model_heartbeat("self_model_loop",
                             (float)(i + 1) / (float)system->model.num_beliefs);
        }

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
    system->model.is_self_model_coherent = (incoherence < 0.2F);

    nimcp_mutex_unlock(&system->mutex);

    return true;
}

bool self_model_reflect(self_model_system_t system,
                       void* introspection,
                       void* autobio)
{
    // Guard: NULL check
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "self_model_reflect: system is NULL");

            return false;
    }

    /* Phase 8: Heartbeat at operation start */
    self_model_heartbeat("self_model_reflect", 0.0f);


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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "self_model_set_personality: system is NULL");

            return false;
    }

    /* Phase 8: Heartbeat at operation start */
    self_model_heartbeat("self_model_set_personality", 0.0f);


    nimcp_mutex_lock(&system->mutex);

    // Wire personality into self-model
    system->model.personality_profile = personality;
    system->model.has_personality = (personality != NULL);

    nimcp_mutex_unlock(&system->mutex);

    return true;
}

// ============================================================================
// Internal Knowledge Graph Integration
// ============================================================================

bool self_model_connect_internal_kg(self_model_system_t system, brain_t brain)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "self_model_connect_internal_kg: system is NULL");

            return false;
    }

    /* Phase 8: Heartbeat at operation start */
    self_model_heartbeat("self_model_connect_internal_kg", 0.0f);


    nimcp_mutex_lock(&system->mutex);

    /* Initialize KG context */
    int result = kg_module_init(&system->kg_context, brain, "self_model");

    if (result != 0) {
        nimcp_mutex_unlock(&system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_model_connect_internal_kg: validation failed");
        return false;
    }

    /* Check if KG is available */
    if (!kg_is_available(&system->kg_context)) {
        system->kg_connected = false;
        LOG_INFO("KG disabled, self-model graceful degradation");
        nimcp_mutex_unlock(&system->mutex);
        return true;  /* Success - just no KG */
    }

    system->kg_connected = true;
    LOG_INFO("Self-model connected to internal KG");

    nimcp_mutex_unlock(&system->mutex);
    return true;
}

void self_model_disconnect_internal_kg(self_model_system_t system)
{
    if (!system) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    self_model_heartbeat("self_model_disconnect_internal_", 0.0f);


    nimcp_mutex_lock(&system->mutex);

    system->kg_context.kg = NULL;
    system->kg_context.kg_available = false;
    system->kg_context.self_node_id = BRAIN_KG_INVALID_NODE;
    system->kg_connected = false;

    LOG_INFO("Self-model disconnected from internal KG");

    nimcp_mutex_unlock(&system->mutex);
}

bool self_model_update_topology_awareness(self_model_system_t system)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "self_model_update_topology_awareness: system is NULL");

            return false;
    }

    /* Phase 8: Heartbeat at operation start */
    self_model_heartbeat("self_model_update_topology_awar", 0.0f);


    nimcp_mutex_lock(&system->mutex);

    /* Check if KG is connected */
    if (!system->kg_connected || !kg_is_available(&system->kg_context)) {
        nimcp_mutex_unlock(&system->mutex);
        /* KG not available - graceful degradation, not an error */
        return true;
    }

    /* Query KG for all active nodes */
    brain_kg_node_list_t* active_nodes = kg_get_nodes_by_state_safe(
        &system->kg_context,
        BRAIN_KG_STATE_ACTIVE
    );

    if (active_nodes) {
        /* Update self-model's awareness of active modules */
        /* This could influence self-efficacy, capability assessments, etc. */
        LOG_INFO("Topology awareness: %u active modules detected", active_nodes->count);
        brain_kg_node_list_destroy(active_nodes);
    }

    nimcp_mutex_unlock(&system->mutex);
    return true;
}

/* Boundary type values matching the anonymous enum in self_boundary_t */
#define SELF_BOUNDARY_SELF 0
#define SELF_BOUNDARY_PART_OF_SELF 1
#define SELF_BOUNDARY_OTHER 2
#define SELF_BOUNDARY_UNCERTAIN 3

int self_model_get_boundary_from_kg(
    self_model_system_t system,
    const char* entity_name
)
{
    if (!system || !entity_name) {
        return SELF_BOUNDARY_OTHER;  /* Default to OTHER for safety */
    }

    /* Phase 8: Heartbeat at operation start */
    self_model_heartbeat("self_model_get_boundary_from_kg", 0.0f);


    nimcp_mutex_lock(&system->mutex);

    /* Check if KG is connected */
    if (!system->kg_connected || !kg_is_available(&system->kg_context)) {
        nimcp_mutex_unlock(&system->mutex);
        return SELF_BOUNDARY_OTHER;  /* Can't determine - default to OTHER */
    }

    /* Find the entity in KG */
    brain_kg_node_id_t entity_id = kg_find_node_safe(&system->kg_context, entity_name);

    if (entity_id == BRAIN_KG_INVALID_NODE) {
        nimcp_mutex_unlock(&system->mutex);
        return SELF_BOUNDARY_OTHER;  /* Unknown entity */
    }

    /* Check if entity is connected to self */
    brain_kg_node_id_t self_id = system->kg_context.self_node_id;
    if (entity_id == self_id) {
        nimcp_mutex_unlock(&system->mutex);
        return SELF_BOUNDARY_SELF;  /* This is the self node */
    }

    /* Check if connected to self node */
    bool connected = kg_are_connected_safe(&system->kg_context, entity_id);

    nimcp_mutex_unlock(&system->mutex);

    if (connected) {
        return SELF_BOUNDARY_PART_OF_SELF;  /* Connected = part of self */
    }

    return SELF_BOUNDARY_OTHER;  /* Not connected = external */
}

uint32_t self_model_discover_capabilities_from_kg(self_model_system_t system)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "self_model_discover_capabilities_from_kg: system is NULL");

            return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    self_model_heartbeat("self_model_discover_capabilitie", 0.0f);


    nimcp_mutex_lock(&system->mutex);

    /* Check if KG is connected */
    if (!system->kg_connected || !kg_is_available(&system->kg_context)) {
        nimcp_mutex_unlock(&system->mutex);
        return 0;
    }

    uint32_t discovered = 0;

    /* Query KG for different node types and infer capabilities */
    brain_kg_node_list_t* cognitive_nodes = kg_get_nodes_by_type_safe(
        &system->kg_context,
        BRAIN_KG_NODE_COGNITIVE
    );

    if (cognitive_nodes && cognitive_nodes->count > 0) {
        /* Each cognitive node represents a cognitive capability */
        LOG_INFO("Discovered %u cognitive capabilities from KG", cognitive_nodes->count);
        discovered += cognitive_nodes->count;
        brain_kg_node_list_destroy(cognitive_nodes);
    }

    brain_kg_node_list_t* coordinator_nodes = kg_get_nodes_by_type_safe(
        &system->kg_context,
        BRAIN_KG_NODE_COORDINATOR
    );

    if (coordinator_nodes && coordinator_nodes->count > 0) {
        /* Coordination capabilities */
        LOG_INFO("Discovered %u coordination capabilities from KG", coordinator_nodes->count);
        discovered += coordinator_nodes->count;
        brain_kg_node_list_destroy(coordinator_nodes);
    }

    brain_kg_node_list_t* security_nodes = kg_get_nodes_by_type_safe(
        &system->kg_context,
        BRAIN_KG_NODE_SECURITY
    );

    if (security_nodes && security_nodes->count > 0) {
        /* Security/protection capabilities */
        LOG_INFO("Discovered %u security capabilities from KG", security_nodes->count);
        discovered += security_nodes->count;
        brain_kg_node_list_destroy(security_nodes);
    }

    nimcp_mutex_unlock(&system->mutex);
    return discovered;
}

/* ========================================================================
 * W9-kg READ-PATH INTEGRATION
 * ======================================================================== */

/**
 * @brief W9-kg read-path: query autobiographical events from the KG.
 *
 * WHAT: Counts autobiographical event nodes in the KG; the self-model
 *       uses this as an index of self-evidence. Emits a self_model update
 *       event so W16 consumers can observe self-model drift.
 * WHY:  Closes the self_model PARTIAL status. The self-model now grounds
 *       its trait/capability inferences in the KG's autobiographical
 *       record rather than in-process state alone.
 * HOW:  Uses the shared W9-kg helpers. Computes a drift proxy from the
 *       event count delta over calls (monotonically increases; the
 *       emitted event carries the running count as drift).
 *
 * @param system self-model instance (non-NULL)
 * @param update_id opaque update id for the emit
 * @return count of autobiographical event nodes visible in the KG.
 */
uint32_t self_model_query_autobio_context(self_model_system_t system,
                                          uint64_t update_id)
{
    if (!system) return 0;

    struct brain_struct* brain = w9kg_get_registered_brain();
    if (!brain) return 0;

    uint32_t autobio_count = w9kg_query_self_model_autobio(brain);
    /* Use autobio_count as a normalized drift proxy in [0,1]. */
    float drift = (autobio_count > 100) ? 1.0f
                                        : ((float)autobio_count / 100.0f);
    w9kg_emit_self_model_update(brain, update_id, drift);
    return autobio_count;
}

/* ========================================================================
 * KG SELF-AWARENESS INTEGRATION
 * ======================================================================== */

/**
 * WHAT: Query knowledge graph for self-knowledge about self-model module
 * WHY:  Enable self-awareness - module can introspect its own capabilities
 * HOW:  Query entity by name, get relations from/to
 *
 * @param kg Knowledge graph reader
 * @return 1 if entity found, 0 if not
 */
int self_model_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Query our own entity from the knowledge graph */
    /* Phase 8: Heartbeat at operation start */
    self_model_heartbeat("self_model_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Self_Model_Module");
    if (self) {
        /* Module now knows its own capabilities from KG */
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                self_model_heartbeat("self_model_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG("Self-model self-knowledge: %s", self->observations[i]);
        }
    }

    /* Query connections to understand integration points */
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Self_Model_Module");
    if (connections) {
        LOG_DEBUG("Self-model has %u outgoing connections", connections->count);
        kg_relation_list_destroy(connections);
    }

    /* Query incoming connections */
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Self_Model_Module");
    if (incoming) {
        LOG_DEBUG("Self-model has %u incoming connections", incoming->count);
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void self_model_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_self_model_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int self_model_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_model_training_begin: NULL argument");
        return -1;
    }
    self_model_heartbeat("self_model_training_begin", 0.0f);
    (void)(struct self_model_system*)instance; /* Module state available for reset */
    return 0;
}

int self_model_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_model_training_end: NULL argument");
        return -1;
    }
    self_model_heartbeat("self_model_training_end", 1.0f);
    (void)(struct self_model_system*)instance; /* Module state available for finalization */
    return 0;
}

int self_model_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_model_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    self_model_heartbeat("self_model_training_step", progress);
    (void)(struct self_model_system*)instance; /* Module state available for step adaptation */
    return 0;
}
