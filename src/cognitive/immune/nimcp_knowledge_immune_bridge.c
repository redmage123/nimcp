/**
 * @file nimcp_knowledge_immune_bridge.c
 * @brief Knowledge Base-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional coupling between brain immune and knowledge systems
 * WHY:  Biological realism - cytokines impair semantic memory, health knowledge affects immunity
 * HOW:  Monitor cytokine levels to slow retrieval, monitor knowledge to prime immune
 */

#include "cognitive/immune/nimcp_knowledge_immune_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(knowledge_immune_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_knowledge_immune_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_knowledge_immune_bridge_mesh_registry = NULL;

nimcp_error_t knowledge_immune_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_knowledge_immune_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "knowledge_immune_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_SECURITY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "knowledge_immune_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_knowledge_immune_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_knowledge_immune_bridge_mesh_registry = registry;
    return err;
}

void knowledge_immune_bridge_mesh_unregister(void) {
    if (g_knowledge_immune_bridge_mesh_registry && g_knowledge_immune_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_knowledge_immune_bridge_mesh_registry, g_knowledge_immune_bridge_mesh_id);
        g_knowledge_immune_bridge_mesh_id = 0;
        g_knowledge_immune_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from knowledge_immune_bridge module (instance-level) */
static inline void knowledge_immune_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_knowledge_immune_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_knowledge_immune_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_knowledge_immune_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}



/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Clamp value to range
 *
 * WHAT: Constrain value to [min, max]
 * WHY:  Prevent overflow/underflow
 * HOW:  Return min if below, max if above, value otherwise
 */
static inline float clamp_f(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

/**
 * @brief Get cytokine concentration from immune system
 *
 * WHAT: Query immune system for specific cytokine level
 * WHY:  Need current cytokine state to compute effects
 * HOW:  Search cytokine array for type, return concentration
 */
static float get_cytokine_level(
    const brain_immune_system_t* immune,
    brain_cytokine_type_t type
) {
    if (!immune) return 0.0f;

    /* Query immune system cytokines */
    float max_concentration = 0.0f;
    for (size_t i = 0; i < immune->cytokine_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && immune->cytokine_count > 256) {
            knowledge_immune_bridge_heartbeat("knowledge_im_loop",
                             (float)(i + 1) / (float)immune->cytokine_count);
        }

        if (immune->cytokines[i].type == type) {
            if (immune->cytokines[i].concentration > max_concentration) {
                max_concentration = immune->cytokines[i].concentration;
            }
        }
    }

    return max_concentration;
}

/**
 * @brief Get max inflammation level from immune system
 *
 * WHAT: Query highest inflammation level across all sites
 * WHY:  Max inflammation determines cognitive impact
 * HOW:  Iterate inflammation sites, return highest level
 */
static brain_inflammation_level_t get_max_inflammation_level(
    const brain_immune_system_t* immune
) {
    if (!immune) return INFLAMMATION_NONE;

    brain_inflammation_level_t max_level = INFLAMMATION_NONE;
    for (size_t i = 0; i < immune->inflammation_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && immune->inflammation_count > 256) {
            knowledge_immune_bridge_heartbeat("knowledge_im_loop",
                             (float)(i + 1) / (float)immune->inflammation_count);
        }

        if (immune->inflammation_sites[i].level > max_level) {
            max_level = immune->inflammation_sites[i].level;
        }
    }

    return max_level;
}

/**
 * @brief Get inflammation duration in seconds
 *
 * WHAT: Calculate how long inflammation has persisted
 * WHY:  Chronic inflammation has cumulative cognitive effects
 * HOW:  Find oldest inflammation site, compute duration from start time
 */
static float get_inflammation_duration_sec(const brain_immune_system_t* immune) {
    if (!immune || immune->inflammation_count == 0) return 0.0f;

    /* Find oldest inflammation site */
    uint64_t oldest_start = UINT64_MAX;
    for (size_t i = 0; i < immune->inflammation_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && immune->inflammation_count > 256) {
            knowledge_immune_bridge_heartbeat("knowledge_im_loop",
                             (float)(i + 1) / (float)immune->inflammation_count);
        }

        if (immune->inflammation_sites[i].start_time < oldest_start) {
            oldest_start = immune->inflammation_sites[i].start_time;
        }
    }

    if (oldest_start == UINT64_MAX) return 0.0f;

    /* Calculate duration (simplified - would use actual timestamp) */
    /* For now, return 0 - actual implementation would compute current_time - oldest_start */
    return 0.0f;
}

/**
 * @brief Compute sickness behavior level
 *
 * WHAT: Calculate overall sickness behavior intensity
 * WHY:  Sickness behavior reduces learning motivation
 * HOW:  Weighted sum of pro-inflammatory cytokines
 */
static float compute_sickness_behavior(const brain_immune_system_t* immune) {
    if (!immune) return 0.0f;

    float il1_level = get_cytokine_level(immune, BRAIN_CYTOKINE_IL1);
    float il6_level = get_cytokine_level(immune, BRAIN_CYTOKINE_IL6);
    float tnf_level = get_cytokine_level(immune, BRAIN_CYTOKINE_TNF);

    /* Sickness behavior weighted combination */
    float sickness = (il1_level * 0.4f) + (il6_level * 0.3f) + (tnf_level * 0.3f);
    return clamp_f(sickness, 0.0f, 1.0f);
}

/**
 * @brief Check if concept is health-related
 *
 * WHAT: Determine if concept belongs to health/medical domain
 * WHY:  Health knowledge gets priority during illness
 * HOW:  Simple keyword matching (real impl would use domain tagging)
 */
static bool is_health_concept(const char* concept) {
    if (!concept) {
        return false;
    }

    /* Simple keyword matching - real implementation would check domain */
    const char* health_keywords[] = {
        "health", "disease", "illness", "medicine", "treatment",
        "symptom", "diagnosis", "infection", "immune", "pathogen",
        "virus", "bacteria", "antibody", "vaccine", NULL
    };

    for (int i = 0; health_keywords[i] != NULL; i++) {
        if (strstr(concept, health_keywords[i]) != NULL) {
            return true;
        }
    }

    return false;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int knowledge_immune_default_config(knowledge_immune_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* All features enabled by default */
    /* Phase 8: Heartbeat at operation start */
    knowledge_immune_bridge_heartbeat("knowledge_im_knowledge_immune_def", 0.0f);


    config->enable_cytokine_retrieval_modulation = true;
    config->enable_inflammation_encoding_impairment = true;
    config->enable_knowledge_immune_priming = true;
    config->enable_illness_knowledge_priority = true;
    config->enable_sickness_learning_impairment = true;

    /* Biologically-based default sensitivities */
    config->cytokine_sensitivity = 1.0f;
    config->inflammation_sensitivity = 1.0f;
    config->knowledge_priming_sensitivity = 1.0f;

    /* Evidence-based thresholds */
    config->sickness_learning_threshold = SICKNESS_LEARNING_THRESHOLD;
    config->chronic_inflammation_days = 7.0f;

    /* Baseline performance */
    config->baseline_retrieval_latency_ms = 50.0f; /* 50ms baseline */

    return 0;
}

knowledge_immune_bridge_t* knowledge_immune_bridge_create(
    const knowledge_immune_config_t* config,
    brain_immune_system_t* immune_system,
    knowledge_system_t knowledge_system
) {
    /* Guard: require both systems */
    if (!immune_system || !knowledge_system) {
        LOG_MODULE_ERROR("knowledge_immune_bridge",
                  "Cannot create bridge without immune and knowledge systems");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_immune_bridge_create: required parameter is NULL (immune_system, knowledge_system)");
        return NULL;
    }

    /* Allocate bridge */
    /* Phase 8: Heartbeat at operation start */
    knowledge_immune_bridge_heartbeat("knowledge_im_create", 0.0f);


    knowledge_immune_bridge_t* bridge = (knowledge_immune_bridge_t*)
        nimcp_malloc(sizeof(knowledge_immune_bridge_t));
    if (!bridge) {
        LOG_MODULE_ERROR("knowledge_immune_bridge", "Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "knowledge_immune_bridge_create: malloc returned NULL");

        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(knowledge_immune_bridge_t));

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->knowledge_system = knowledge_system;

    /* Apply configuration */
    if (config) {
        bridge->enable_cytokine_retrieval_modulation = config->enable_cytokine_retrieval_modulation;
        bridge->enable_inflammation_encoding_impairment = config->enable_inflammation_encoding_impairment;
        bridge->enable_knowledge_immune_priming = config->enable_knowledge_immune_priming;
        bridge->enable_illness_knowledge_priority = config->enable_illness_knowledge_priority;
        bridge->enable_sickness_learning_impairment = config->enable_sickness_learning_impairment;
        bridge->baseline_retrieval_latency_ms = config->baseline_retrieval_latency_ms;
    } else {
        /* Use defaults */
        knowledge_immune_config_t default_cfg;
        knowledge_immune_default_config(&default_cfg);
        bridge->enable_cytokine_retrieval_modulation = default_cfg.enable_cytokine_retrieval_modulation;
        bridge->enable_inflammation_encoding_impairment = default_cfg.enable_inflammation_encoding_impairment;
        bridge->enable_knowledge_immune_priming = default_cfg.enable_knowledge_immune_priming;
        bridge->enable_illness_knowledge_priority = default_cfg.enable_illness_knowledge_priority;
        bridge->enable_sickness_learning_impairment = default_cfg.enable_sickness_learning_impairment;
        bridge->baseline_retrieval_latency_ms = default_cfg.baseline_retrieval_latency_ms;
    }

    /* Initialize current latency to baseline */
    bridge->current_retrieval_latency_ms = bridge->baseline_retrieval_latency_ms;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "knowledge_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);    return NULL;
    }

    LOG_MODULE_INFO("knowledge_immune_bridge", "Bridge created successfully");
    return bridge;
}

void knowledge_immune_bridge_destroy(knowledge_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Destroy mutex */
    /* Phase 8: Heartbeat at operation start */
    knowledge_immune_bridge_heartbeat("knowledge_im_destroy", 0.0f);


    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free bridge (don't destroy linked systems - we don't own them) */
    nimcp_free(bridge);
    bridge = NULL;
    LOG_MODULE_INFO("knowledge_immune_bridge", "Bridge destroyed");
}

/* ============================================================================
 * Immune → Knowledge Implementation
 * ============================================================================ */

int knowledge_immune_apply_cytokine_effects(knowledge_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_cytokine_retrieval_modulation) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_immune_apply_cytokine_effects: bridge->immune_system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_immune_bridge_heartbeat("knowledge_im_knowledge_immune_app", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Get cytokine levels */
    float il1 = get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IL1);
    float il6 = get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IL6);
    float tnf = get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_TNF);
    float ifn_gamma = get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IFN_GAMMA);
    float il10 = get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IL10);

    /* Compute individual cytokine effects */
    bridge->cytokine_effects.il1_latency_multiplier =
        1.0f + (il1 * (CYTOKINE_IL1_RETRIEVAL_IMPACT - 1.0f));
    bridge->cytokine_effects.il6_latency_multiplier =
        1.0f + (il6 * (CYTOKINE_IL6_RETRIEVAL_IMPACT - 1.0f));
    bridge->cytokine_effects.tnf_latency_multiplier =
        1.0f + (tnf * (CYTOKINE_TNF_RETRIEVAL_IMPACT - 1.0f));
    bridge->cytokine_effects.ifn_gamma_latency_multiplier =
        1.0f + (ifn_gamma * (CYTOKINE_IFN_GAMMA_RETRIEVAL_IMPACT - 1.0f));

    /* IL-10 provides benefit (reduces latency) */
    bridge->cytokine_effects.il10_latency_benefit =
        1.0f - (il10 * (1.0f - CYTOKINE_IL10_RETRIEVAL_BENEFIT));

    /* Combine effects (pro-inflammatory increases, anti-inflammatory decreases) */
    float pro_inflammatory_effect =
        bridge->cytokine_effects.il1_latency_multiplier *
        bridge->cytokine_effects.il6_latency_multiplier *
        bridge->cytokine_effects.tnf_latency_multiplier *
        bridge->cytokine_effects.ifn_gamma_latency_multiplier;

    bridge->cytokine_effects.total_latency_multiplier =
        pro_inflammatory_effect * bridge->cytokine_effects.il10_latency_benefit;

    /* Compute overall impairment metrics */
    bridge->cytokine_effects.retrieval_impairment =
        clamp_f((bridge->cytokine_effects.total_latency_multiplier - 1.0f) / 0.5f, 0.0f, 1.0f);

    bridge->cytokine_effects.encoding_impairment =
        clamp_f((il1 + il6 + tnf) / 3.0f, 0.0f, 1.0f);

    /* Update current retrieval latency */
    bridge->current_retrieval_latency_ms =
        bridge->baseline_retrieval_latency_ms * bridge->cytokine_effects.total_latency_multiplier;

    bridge->cytokine_modulations++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int knowledge_immune_apply_inflammation_encoding(knowledge_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_inflammation_encoding_impairment) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_immune_apply_inflammation_encoding: bridge->immune_system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_immune_bridge_heartbeat("knowledge_im_knowledge_immune_app", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Get inflammation state */
    brain_inflammation_level_t level = get_max_inflammation_level(bridge->immune_system);
    float duration = get_inflammation_duration_sec(bridge->immune_system);

    bridge->inflammation_state.current_level = level;
    bridge->inflammation_state.inflammation_duration_sec = duration;
    bridge->inflammation_state.is_chronic = (duration >= CHRONIC_INFLAMMATION_THRESHOLD);

    /* Map inflammation level to encoding penalty */
    float encoding_multiplier = 1.0f;
    switch (level) {
        case INFLAMMATION_NONE:
            encoding_multiplier = 1.0f;
            break;
        case INFLAMMATION_LOCAL:
            encoding_multiplier = INFLAMMATION_ENCODING_PENALTY_LOCAL;
            break;
        case INFLAMMATION_REGIONAL:
            encoding_multiplier = INFLAMMATION_ENCODING_PENALTY_REGIONAL;
            break;
        case INFLAMMATION_SYSTEMIC:
            encoding_multiplier = INFLAMMATION_ENCODING_PENALTY_SYSTEMIC;
            break;
        case INFLAMMATION_STORM:
            encoding_multiplier = INFLAMMATION_ENCODING_PENALTY_STORM;
            break;
    }

    bridge->inflammation_state.encoding_penalty = 1.0f - encoding_multiplier;

    /* Chronic inflammation causes progressive decline */
    if (bridge->inflammation_state.is_chronic) {
        float chronic_factor = clamp_f(duration / (86400.0f * 30.0f), 0.0f, 1.0f); /* 30 days max */
        bridge->inflammation_state.cognitive_decline = chronic_factor * 0.3f; /* Max 30% decline */
        bridge->inflammation_state.encoding_penalty += bridge->inflammation_state.cognitive_decline;
    }

    bridge->inflammation_state.encoding_penalty = clamp_f(
        bridge->inflammation_state.encoding_penalty, 0.0f, 1.0f
    );

    /* Retrieval slowdown tracks inflammation level */
    bridge->inflammation_state.retrieval_slowdown = bridge->inflammation_state.encoding_penalty;

    /* Association weakening from chronic inflammation */
    bridge->inflammation_state.association_weakening =
        bridge->inflammation_state.is_chronic ? bridge->inflammation_state.cognitive_decline : 0.0f;

    bridge->inflammation_impairments++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float knowledge_immune_get_retrieval_latency_multiplier(
    const knowledge_immune_bridge_t* bridge
) {
    if (!bridge) return 1.0f;
    /* Phase 8: Heartbeat at operation start */
    knowledge_immune_bridge_heartbeat("knowledge_im_knowledge_immune_get", 0.0f);


    return bridge->cytokine_effects.total_latency_multiplier;
}

float knowledge_immune_get_encoding_penalty(const knowledge_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    knowledge_immune_bridge_heartbeat("knowledge_im_knowledge_immune_get", 0.0f);


    return bridge->inflammation_state.encoding_penalty;
}

int knowledge_immune_apply_sickness_learning_impairment(
    knowledge_immune_bridge_t* bridge
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_sickness_learning_impairment) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_immune_apply_sickness_learning_impairment: bridge->immune_system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_immune_bridge_heartbeat("knowledge_im_knowledge_immune_app", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute sickness behavior level */
    float sickness = compute_sickness_behavior(bridge->immune_system);
    bridge->inflammation_state.sickness_level = sickness;

    /* Sickness suppresses curiosity and learning motivation */
    if (sickness >= SICKNESS_LEARNING_THRESHOLD) {
        bridge->inflammation_state.curiosity_suppression = SICKNESS_CURIOSITY_SUPPRESSION;
        bridge->inflammation_state.learning_motivation = 1.0f - sickness;
    } else {
        bridge->inflammation_state.curiosity_suppression = 0.0f;
        bridge->inflammation_state.learning_motivation = 1.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Knowledge → Immune Implementation
 * ============================================================================ */

int knowledge_immune_prime_from_health_knowledge(knowledge_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_knowledge_immune_priming) return 0;
    if (!bridge->knowledge_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_immune_prime_from_health_knowledge: bridge->knowledge_system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_immune_bridge_heartbeat("knowledge_im_knowledge_immune_pri", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Query health domain knowledge coverage */
    domain_knowledge_t health_assessment;
    bool has_health = knowledge_assess_domain(
        bridge->knowledge_system,
        KNOWLEDGE_DOMAIN_SCIENCE, /* Health falls under science domain */
        &health_assessment
    );

    if (has_health) {
        bridge->knowledge_modulation.health_concepts_known = health_assessment.concepts_known;
        bridge->knowledge_modulation.health_knowledge_depth = health_assessment.avg_confidence;

        /* More health knowledge = better immune preparedness */
        bridge->knowledge_modulation.immune_preparedness_boost =
            health_assessment.avg_confidence * 0.3f; /* Max 30% boost */

        /* Better risk assessment from knowledge */
        bridge->knowledge_modulation.risk_assessment_accuracy =
            health_assessment.coverage_percentage / 100.0f;

        bridge->knowledge_priming_events++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int knowledge_immune_assess_threat(
    knowledge_immune_bridge_t* bridge,
    const char* threat_description,
    float* assessed_severity
) {
    /* Guard clauses */
    if (!bridge || !threat_description || !assessed_severity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_immune_assess_threat: required parameter is NULL (bridge, threat_description, assessed_severity)");
        return -1;
    }
    if (!bridge->knowledge_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_immune_assess_threat: bridge->knowledge_system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_immune_bridge_heartbeat("knowledge_im_knowledge_immune_ass", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Try to retrieve knowledge about threat */
    knowledge_item_t threat_knowledge;
    bool found = knowledge_retrieve(
        bridge->knowledge_system,
        threat_description,
        &threat_knowledge
    );

    if (found) {
        /* Use confidence as severity indicator */
        *assessed_severity = threat_knowledge.confidence * 10.0f;

        /* Boost severity if health-related */
        if (threat_knowledge.domain == KNOWLEDGE_DOMAIN_SCIENCE) {
            *assessed_severity *= 1.2f;
        }
    } else {
        /* Unknown threat = moderate severity */
        *assessed_severity = 5.0f;
    }

    *assessed_severity = clamp_f(*assessed_severity, 1.0f, 10.0f);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int knowledge_immune_trigger_from_threat_learning(
    knowledge_immune_bridge_t* bridge,
    const char* learned_concept
) {
    /* Guard clauses */
    if (!bridge || !learned_concept) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_immune_trigger_from_threat_learning: required parameter is NULL (bridge, learned_concept)");
        return -1;
    }
    if (!bridge->enable_knowledge_immune_priming) return 0;

    /* Phase 8: Heartbeat at operation start */
    knowledge_immune_bridge_heartbeat("knowledge_im_knowledge_immune_tri", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if learned concept is health/threat related */
    if (is_health_concept(learned_concept)) {
        /* Increase immune vigilance */
        bridge->knowledge_modulation.threat_awareness += 0.1f;
        bridge->knowledge_modulation.threat_awareness =
            clamp_f(bridge->knowledge_modulation.threat_awareness, 0.0f, 1.0f);

        bridge->knowledge_priming_events++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Illness-Based Prioritization Implementation
 * ============================================================================ */

int knowledge_immune_prioritize_health_knowledge(
    knowledge_immune_bridge_t* bridge
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_illness_knowledge_priority) return 0;

    /* Phase 8: Heartbeat at operation start */
    knowledge_immune_bridge_heartbeat("knowledge_im_knowledge_immune_pri", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if experiencing sickness behavior */
    float sickness = compute_sickness_behavior(bridge->immune_system);
    bridge->illness_priority.is_sick = (sickness >= SICKNESS_LEARNING_THRESHOLD);

    if (bridge->illness_priority.is_sick) {
        /* Boost health knowledge retrieval */
        bridge->illness_priority.health_relevance_boost = HEALTH_KNOWLEDGE_BOOST_MULTIPLIER;
        bridge->illness_priority.non_health_suppression = 1.2f; /* 20% slower */

        /* Prioritize health-related domains */
        bridge->illness_priority.prioritized_domains[0] = KNOWLEDGE_DOMAIN_SCIENCE;
        bridge->illness_priority.prioritized_domains[1] = KNOWLEDGE_DOMAIN_GENERAL;
        bridge->illness_priority.num_prioritized_domains = 2;

        bridge->illness_priority.domain_boost_multipliers[0] = 0.67f; /* 50% faster */
        bridge->illness_priority.domain_boost_multipliers[1] = 0.9f;  /* 10% faster */

        bridge->illness_prioritizations++;
    } else {
        /* Normal state - no prioritization */
        bridge->illness_priority.health_relevance_boost = 1.0f;
        bridge->illness_priority.non_health_suppression = 1.0f;
        bridge->illness_priority.num_prioritized_domains = 0;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float knowledge_immune_get_domain_retrieval_multiplier(
    const knowledge_immune_bridge_t* bridge,
    knowledge_domain_t domain
) {
    if (!bridge) return 1.0f;

    /* Check if domain is prioritized */
    /* Phase 8: Heartbeat at operation start */
    knowledge_immune_bridge_heartbeat("knowledge_im_knowledge_immune_get", 0.0f);


    for (uint32_t i = 0; i < bridge->illness_priority.num_prioritized_domains; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->illness_priority.num_prioritized_domains > 256) {
            knowledge_immune_bridge_heartbeat("knowledge_im_loop",
                             (float)(i + 1) / (float)bridge->illness_priority.num_prioritized_domains);
        }

        if (bridge->illness_priority.prioritized_domains[i] == domain) {
            return bridge->illness_priority.domain_boost_multipliers[i];
        }
    }

    /* Non-prioritized domains get suppression during illness */
    if (bridge->illness_priority.is_sick) {
        return bridge->illness_priority.non_health_suppression;
    }

    return 1.0f;
}

/* ============================================================================
 * Bidirectional Update Implementation
 * ============================================================================ */

int knowledge_immune_bridge_update(
    knowledge_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Apply immune → knowledge effects */
    /* Phase 8: Heartbeat at operation start */
    knowledge_immune_bridge_heartbeat("knowledge_im_update", 0.0f);


    knowledge_immune_apply_cytokine_effects(bridge);
    knowledge_immune_apply_inflammation_encoding(bridge);
    knowledge_immune_apply_sickness_learning_impairment(bridge);

    /* Apply knowledge → immune effects */
    knowledge_immune_prime_from_health_knowledge(bridge);

    /* Apply illness-based prioritization */
    knowledge_immune_prioritize_health_knowledge(bridge);

    bridge->total_updates++;
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int knowledge_immune_get_cytokine_effects(
    const knowledge_immune_bridge_t* bridge,
    cytokine_knowledge_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_immune_get_cytokine_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_immune_bridge_heartbeat("knowledge_im_knowledge_immune_get", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->cytokine_effects;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int knowledge_immune_get_inflammation_state(
    const knowledge_immune_bridge_t* bridge,
    inflammation_knowledge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_immune_get_inflammation_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_immune_bridge_heartbeat("knowledge_im_knowledge_immune_get", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->inflammation_state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool knowledge_immune_is_cognitively_impaired(
    const knowledge_immune_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }

    /* Significant impairment if retrieval > 30% slower or encoding > 40% impaired */
    /* Phase 8: Heartbeat at operation start */
    knowledge_immune_bridge_heartbeat("knowledge_im_knowledge_immune_is_", 0.0f);


    return (bridge->cytokine_effects.retrieval_impairment > 0.3f) ||
           (bridge->inflammation_state.encoding_penalty > 0.4f);
}

float knowledge_immune_get_retrieval_latency_increase_pct(
    const knowledge_immune_bridge_t* bridge
) {
    if (!bridge) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    knowledge_immune_bridge_heartbeat("knowledge_im_knowledge_immune_get", 0.0f);


    float increase = bridge->cytokine_effects.total_latency_multiplier - 1.0f;
    return increase * 100.0f;
}

float knowledge_immune_get_encoding_success_rate(
    const knowledge_immune_bridge_t* bridge
) {
    if (!bridge) return 1.0f;

    /* Success rate = 1 - penalty */
    /* Phase 8: Heartbeat at operation start */
    knowledge_immune_bridge_heartbeat("knowledge_im_knowledge_immune_get", 0.0f);


    return 1.0f - bridge->inflammation_state.encoding_penalty;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

#define KNOWLEDGE_IMMUNE_MODULE_NAME "knowledge_immune_bridge"

/**
 * @brief Connect bridge to bio-async router
 */
int knowledge_immune_connect_bio_async(knowledge_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    knowledge_immune_bridge_heartbeat("knowledge_im_knowledge_immune_con", 0.0f);


    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_KNOWLEDGE,
        .module_name = KNOWLEDGE_IMMUNE_MODULE_NAME,
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("knowledge_immune_bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

/**
 * @brief Disconnect from bio-async router
 */
int knowledge_immune_disconnect_bio_async(knowledge_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    knowledge_immune_bridge_heartbeat("knowledge_im_knowledge_immune_dis", 0.0f);


    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_DEBUG("knowledge_immune_bridge disconnected from bio-async router");
    return 0;
}

/**
 * @brief Check if bio-async is connected
 */
bool knowledge_immune_is_bio_async_connected(const knowledge_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    knowledge_immune_bridge_heartbeat("knowledge_im_knowledge_immune_is_", 0.0f);


    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Query KG for module self-awareness information
 * WHY:  Enable introspective self-knowledge about knowledge immune bridge
 * HOW:  Look up entity and relations in KG
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 otherwise
 */
int knowledge_immune_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    knowledge_immune_bridge_heartbeat("knowledge_im_knowledge_immune_que", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Knowledge_Immune_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                knowledge_immune_bridge_heartbeat("knowledge_im_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Knowledge immune bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Knowledge_Immune_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Knowledge_Immune_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void knowledge_immune_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_knowledge_immune_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int knowledge_immune_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "knowledge_immune_bridge_training_begin: NULL argument");
        return -1;
    }
    /* TODO: pass cast instance as nimcp_health_agent_t* once bridge stores per-instance agent */
    knowledge_immune_bridge_heartbeat_instance(NULL, "knowledge_immune_bridge_training_begin", 0.0f);
    return 0;
}

int knowledge_immune_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "knowledge_immune_bridge_training_end: NULL argument");
        return -1;
    }
    /* TODO: pass cast instance as nimcp_health_agent_t* once bridge stores per-instance agent */
    knowledge_immune_bridge_heartbeat_instance(NULL, "knowledge_immune_bridge_training_end", 1.0f);
    return 0;
}

int knowledge_immune_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "knowledge_immune_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    /* TODO: pass cast instance as nimcp_health_agent_t* once bridge stores per-instance agent */
    knowledge_immune_bridge_heartbeat_instance(NULL, "knowledge_immune_bridge_training_step", progress);
    return 0;
}
