/**
 * @file nimcp_autobiographical_immune_bridge.c
 * @brief Autobiographical Memory-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional coupling between brain immune and autobiographical memory
 * WHY:  Biological realism - cytokines impair encoding, inflammation affects hippocampus
 * HOW:  Monitor cytokine levels to modulate encoding, track sickness landmarks
 */

#include "cognitive/immune/nimcp_autobiographical_immune_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(autobiographical_immune_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_autobiographical_immune_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_autobiographical_immune_bridge_mesh_registry = NULL;

nimcp_error_t autobiographical_immune_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_autobiographical_immune_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "autobiographical_immune_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_SECURITY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "autobiographical_immune_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_autobiographical_immune_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_autobiographical_immune_bridge_mesh_registry = registry;
    return err;
}

void autobiographical_immune_bridge_mesh_unregister(void) {
    if (g_autobiographical_immune_bridge_mesh_registry && g_autobiographical_immune_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_autobiographical_immune_bridge_mesh_registry, g_autobiographical_immune_bridge_mesh_id);
        g_autobiographical_immune_bridge_mesh_id = 0;
        g_autobiographical_immune_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from autobiographical_immune_bridge module (instance-level) */
static inline void autobiographical_immune_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_autobiographical_immune_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_autobiographical_immune_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_autobiographical_immune_bridge_health_agent) {
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
 * WHAT: Query specific cytokine level
 * WHY:  Need individual cytokine concentrations for encoding modulation
 * HOW:  Iterate through immune system cytokines, find matching type
 */
static float get_cytokine_concentration(
    const brain_immune_system_t* immune,
    brain_cytokine_type_t type
) {
    if (!immune) return 0.0f;

    /* Iterate through active cytokines */
    float total = 0.0f;
    for (size_t i = 0; i < immune->cytokine_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && immune->cytokine_count > 256) {
            autobiographical_immune_bridge_heartbeat("autobiograph_loop",
                             (float)(i + 1) / (float)immune->cytokine_count);
        }

        if (immune->cytokines[i].type == type) {
            total += immune->cytokines[i].concentration;
        }
    }

    return clamp_f(total, 0.0f, 1.0f);
}

/**
 * @brief Get inflammation duration
 *
 * WHAT: Calculate how long inflammation has persisted
 * WHY:  Chronic inflammation (>7 days) has different memory effects
 * HOW:  Find oldest active inflammation site, compute duration
 */
static float get_inflammation_duration_sec(const brain_immune_system_t* immune) {
    if (!immune || immune->inflammation_count == 0) return 0.0f;

    /* Find oldest inflammation site */
    uint64_t oldest_time = immune->inflammation_sites[0].start_time;
    for (size_t i = 1; i < immune->inflammation_count; i++) {
        if (immune->inflammation_sites[i].start_time < oldest_time) {
            oldest_time = immune->inflammation_sites[i].start_time;
        }
    }

    /* Compute duration using monotonic clock */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t current_time = (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
    if (current_time > oldest_time) {
        return (float)(current_time - oldest_time) / 1000.0f; /* ms to sec */
    }
    return 0.0f;
}

/**
 * @brief Get current inflammation level
 *
 * WHAT: Get highest inflammation level in system
 * WHY:  Max inflammation determines memory impact
 * HOW:  Query immune system for max inflammation site level
 */
static brain_inflammation_level_t get_max_inflammation_level(
    const brain_immune_system_t* immune
) {
    if (!immune || immune->inflammation_count == 0) {
        return INFLAMMATION_NONE;
    }

    brain_inflammation_level_t max_level = INFLAMMATION_NONE;
    for (size_t i = 0; i < immune->inflammation_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && immune->inflammation_count > 256) {
            autobiographical_immune_bridge_heartbeat("autobiograph_loop",
                             (float)(i + 1) / (float)immune->inflammation_count);
        }

        if (immune->inflammation_sites[i].level > max_level) {
            max_level = immune->inflammation_sites[i].level;
        }
    }

    return max_level;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int autobio_immune_default_config(autobio_immune_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* All features enabled by default */
    /* Phase 8: Heartbeat at operation start */
    autobiographical_immune_bridge_heartbeat("autobiograph_autobio_immune_defau", 0.0f);


    config->enable_cytokine_encoding_modulation = true;
    config->enable_inflammation_consolidation_impairment = true;
    config->enable_sickness_landmark_creation = true;
    config->enable_trauma_memory_immune_trigger = true;
    config->enable_positive_memory_immune_boost = true;
    config->enable_rumination_tracking = true;

    /* Biologically-based default sensitivities */
    config->cytokine_sensitivity = 1.0f;
    config->inflammation_sensitivity = 1.0f;
    config->memory_trigger_sensitivity = 1.0f;

    /* Evidence-based thresholds */
    config->trauma_trigger_threshold = TRAUMA_MEMORY_IMMUNE_THRESHOLD;
    config->negative_stress_threshold = NEGATIVE_MEMORY_STRESS_THRESHOLD;

    /* Sickness landmark settings */
    config->max_sickness_landmarks = 100;

    return 0;
}

autobio_immune_bridge_t* autobio_immune_bridge_create(
    const autobio_immune_config_t* config,
    brain_immune_system_t* immune_system,
    autobiographical_memory_t* autobio_memory
) {
    /* Guard: require both systems */
    if (!immune_system || !autobio_memory) {
        LOG_MODULE_ERROR("autobio_immune_bridge",
                  "Cannot create bridge without immune and autobio memory systems");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_immune_bridge_create: required parameter is NULL (immune_system, autobio_memory)");
        return NULL;
    }

    /* Allocate bridge */
    /* Phase 8: Heartbeat at operation start */
    autobiographical_immune_bridge_heartbeat("autobiograph_autobio_immune_bridg", 0.0f);


    autobio_immune_bridge_t* bridge = (autobio_immune_bridge_t*)
        nimcp_malloc(sizeof(autobio_immune_bridge_t));
    if (!bridge) {
        LOG_MODULE_ERROR("autobio_immune_bridge", "Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "autobio_immune_bridge_create: malloc returned NULL");

        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(autobio_immune_bridge_t));

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->autobio_memory = autobio_memory;

    /* Apply configuration */
    autobio_immune_config_t default_cfg;
    if (!config) {
        autobio_immune_default_config(&default_cfg);
        config = &default_cfg;
    }

    bridge->enable_cytokine_encoding_modulation = config->enable_cytokine_encoding_modulation;
    bridge->enable_inflammation_consolidation_impairment = config->enable_inflammation_consolidation_impairment;
    bridge->enable_sickness_landmark_creation = config->enable_sickness_landmark_creation;
    bridge->enable_trauma_memory_immune_trigger = config->enable_trauma_memory_immune_trigger;
    bridge->enable_positive_memory_immune_boost = config->enable_positive_memory_immune_boost;
    bridge->enable_rumination_tracking = config->enable_rumination_tracking;

    /* Allocate sickness landmarks array */
    bridge->sickness_landmark_capacity = config->max_sickness_landmarks;
    bridge->sickness_landmarks = (sickness_landmark_t*)
        nimcp_calloc(bridge->sickness_landmark_capacity, sizeof(sickness_landmark_t));
    if (!bridge->sickness_landmarks) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "autobio_immune_bridge_create: bridge->sickness_landmarks is NULL");
        return NULL;
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "autobiographical_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge->sickness_landmarks);
        nimcp_free(bridge);    return NULL;
    }

    LOG_MODULE_INFO("autobio_immune_bridge", "Bridge created successfully");
    return bridge;
}

void autobio_immune_bridge_destroy(autobio_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Destroy mutex */
    /* Phase 8: Heartbeat at operation start */
    autobiographical_immune_bridge_heartbeat("autobiograph_autobio_immune_bridg", 0.0f);


    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free sickness landmarks */
    if (bridge->sickness_landmarks) {
        nimcp_free(bridge->sickness_landmarks);
    }

    /* Free bridge (don't destroy linked systems - we don't own them) */
    nimcp_free(bridge);
    bridge = NULL;
    LOG_MODULE_INFO("autobio_immune_bridge", "Bridge destroyed");
}

/* ============================================================================
 * Immune → Memory Implementation
 * ============================================================================ */

/**
 * @brief Internal unlocked variant - caller must hold bridge->base.mutex
 */
static int autobio_immune_apply_cytokine_encoding_effects_unlocked(autobio_immune_bridge_t* bridge) {
    if (!bridge->enable_cytokine_encoding_modulation) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_immune_apply_cytokine_encoding_effects: bridge->immune_system is NULL");
        return -1;
    }

    /* Query cytokine concentrations */
    float il1 = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL1);
    float il6 = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL6);
    float tnf = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_TNF);
    float ifn_gamma = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IFN_GAMMA);
    float il10 = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL10);

    /* Compute encoding impairments */
    bridge->cytokine_effects.il1_encoding_impairment = il1 * fabsf(CYTOKINE_IL1_ENCODING_IMPACT);
    bridge->cytokine_effects.il6_encoding_impairment = il6 * fabsf(CYTOKINE_IL6_ENCODING_IMPACT);
    bridge->cytokine_effects.tnf_encoding_impairment = tnf * fabsf(CYTOKINE_TNF_ENCODING_IMPACT);
    bridge->cytokine_effects.ifn_gamma_impairment = ifn_gamma * fabsf(CYTOKINE_IFN_GAMMA_ENCODING_IMPACT);
    bridge->cytokine_effects.il10_encoding_boost = il10 * CYTOKINE_IL10_ENCODING_BOOST;

    /* Total encoding modulation (1.0 = normal, <1.0 = impaired, >1.0 = boosted) */
    float impairment = bridge->cytokine_effects.il1_encoding_impairment +
                       bridge->cytokine_effects.il6_encoding_impairment +
                       bridge->cytokine_effects.tnf_encoding_impairment +
                       bridge->cytokine_effects.ifn_gamma_impairment;

    bridge->cytokine_effects.total_encoding_modulation =
        1.0f - impairment + bridge->cytokine_effects.il10_encoding_boost;
    bridge->cytokine_effects.total_encoding_modulation =
        clamp_f(bridge->cytokine_effects.total_encoding_modulation, 0.0f, 1.5f);

    /* Emotional salience modulation */
    float inflammation_level = (float)get_max_inflammation_level(bridge->immune_system) /
                               (float)INFLAMMATION_STORM;
    bridge->cytokine_effects.negative_salience_boost =
        inflammation_level * INFLAMMATION_NEGATIVE_SALIENCE_BOOST;
    bridge->cytokine_effects.positive_salience_reduction =
        inflammation_level * INFLAMMATION_POSITIVE_SALIENCE_REDUCE;

    /* Consolidation impairment */
    bridge->cytokine_effects.consolidation_impairment = impairment;

    bridge->encoding_modulations++;

    return 0;
}

int autobio_immune_apply_cytokine_encoding_effects(autobio_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    autobiographical_immune_bridge_heartbeat("autobiograph_autobio_immune_apply", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    int result = autobio_immune_apply_cytokine_encoding_effects_unlocked(bridge);
    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

/**
 * @brief Internal unlocked variant - caller must hold bridge->base.mutex
 */
static int autobio_immune_apply_inflammation_consolidation_effects_unlocked(
    autobio_immune_bridge_t* bridge
) {
    if (!bridge->enable_inflammation_consolidation_impairment) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_immune_apply_inflammation_consolidation_effects: bridge->immune_system is NULL");
        return -1;
    }

    /* Get inflammation state */
    brain_inflammation_level_t level = get_max_inflammation_level(bridge->immune_system);
    float duration = get_inflammation_duration_sec(bridge->immune_system);

    bridge->inflammation_state.current_level = level;
    bridge->inflammation_state.inflammation_duration_sec = duration;
    bridge->inflammation_state.is_chronic =
        (duration >= CHRONIC_INFLAMMATION_MEMORY_THRESHOLD);

    /* Compute memory impacts based on inflammation level */
    float severity = (float)level / (float)INFLAMMATION_STORM;

    /* Encoding efficiency decreases with inflammation */
    bridge->inflammation_state.encoding_efficiency =
        1.0f - (severity * 0.5f);

    /* Consolidation quality impaired */
    bridge->inflammation_state.consolidation_quality =
        1.0f - (severity * 0.6f);

    /* Retrieval accuracy slightly impaired */
    bridge->inflammation_state.retrieval_accuracy =
        1.0f - (severity * 0.3f);

    /* False memory risk increases */
    bridge->inflammation_state.false_memory_risk = severity * 0.4f;

    /* Chronic inflammation accelerates decline */
    if (bridge->inflammation_state.is_chronic) {
        bridge->inflammation_state.memory_decline_rate = severity * 0.3f;
        bridge->inflammation_state.hippocampal_impairment = severity * 0.5f;
    } else {
        bridge->inflammation_state.memory_decline_rate = 0.0f;
        bridge->inflammation_state.hippocampal_impairment = severity * 0.2f;
    }

    return 0;
}

int autobio_immune_apply_inflammation_consolidation_effects(
    autobio_immune_bridge_t* bridge
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    autobiographical_immune_bridge_heartbeat("autobiograph_autobio_immune_apply", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    int result = autobio_immune_apply_inflammation_consolidation_effects_unlocked(bridge);
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

float autobio_immune_modulate_memory_salience(
    const autobio_immune_bridge_t* bridge,
    const autobiographical_memory_entry_t* memory
) {
    /* Guard clauses */
    if (!bridge || !memory) return 1.0f;
    if (!bridge->enable_cytokine_encoding_modulation) return 1.0f;

    /* Phase 8: Heartbeat at operation start */
    autobiographical_immune_bridge_heartbeat("autobiograph_autobio_immune_modul", 0.0f);


    float modulation = 1.0f;

    /* Enhance negative memories during inflammation */
    if (memory->valence < VALENCE_NEUTRAL) {
        modulation += bridge->cytokine_effects.negative_salience_boost;
    }
    /* Reduce positive memories during inflammation */
    else if (memory->valence > VALENCE_NEUTRAL) {
        modulation += bridge->cytokine_effects.positive_salience_reduction;
    }

    return clamp_f(modulation, 0.5f, 1.5f);
}

/**
 * @brief Internal unlocked variant - caller must hold bridge->base.mutex
 */
static int autobio_immune_create_sickness_landmark_unlocked(
    autobio_immune_bridge_t* bridge,
    brain_inflammation_level_t severity,
    uint64_t* landmark_id
) {
    if (!bridge->enable_sickness_landmark_creation) return 0;
    if (!bridge->autobio_memory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_immune_create_sickness_landmark: bridge->autobio_memory is NULL");
        return -1;
    }
    if (severity < INFLAMMATION_SYSTEMIC) return 0; /* Only create for systemic+ */

    /* Check capacity */
    if (bridge->sickness_landmark_count >= bridge->sickness_landmark_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "autobio_immune_create_sickness_landmark: capacity exceeded");
        return -1;
    }

    /* Create autobiographical memory for sickness episode */
    autobiographical_memory_entry_t memory;
    memset(&memory, 0, sizeof(memory));

    memory.type = AUTOBIO_CRISIS;
    memory.valence = VALENCE_NEGATIVE;
    memory.emotional_intensity = (float)severity / (float)INFLAMMATION_STORM;
    memory.arousal = 0.7f; /* Sickness is moderate arousal */
    memory.importance = SICKNESS_LANDMARK_IMPORTANCE;
    memory.self_relevance = 1.0f; /* Highly self-relevant */
    memory.identity_defining = (severity == INFLAMMATION_STORM); /* Storm is identity-defining */

    snprintf(memory.what_happened, AUTOBIO_MAX_DESCRIPTION_LEN,
             "I experienced %s inflammation (sickness episode)",
             brain_immune_inflammation_to_string(severity));
    snprintf(memory.why_it_happened, AUTOBIO_MAX_REASONING_LEN,
             "Immune system activated against threat");
    snprintf(memory.outcome, AUTOBIO_MAX_OUTCOME_LEN,
             "Episode ongoing, experiencing sickness behavior");

    /* Store in autobiographical memory */
    uint64_t mem_id = autobio_store(*bridge->autobio_memory, &memory);
    if (mem_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "autobio_immune_create_sickness_landmark: mem_id is zero");
        return -1;
    }

    /* Create sickness landmark record */
    sickness_landmark_t* landmark =
        &bridge->sickness_landmarks[bridge->sickness_landmark_count];

    landmark->memory_id = mem_id;
    landmark->start_time_ms = 0; /* Would get from system */
    landmark->end_time_ms = 0; /* Ongoing */
    landmark->severity = severity;
    landmark->emotional_intensity = memory.emotional_intensity;
    landmark->identity_defining = memory.identity_defining;
    snprintf(landmark->description, sizeof(landmark->description),
             "%s", memory.what_happened);

    bridge->sickness_landmark_count++;
    bridge->active_sickness_landmark_id = (uint32_t)mem_id;
    bridge->sickness_landmarks_created++;

    *landmark_id = mem_id;

    LOG_MODULE_INFO("autobio_immune_bridge",
                  "Created sickness landmark: %s", landmark->description);

    return 0;
}

int autobio_immune_create_sickness_landmark(
    autobio_immune_bridge_t* bridge,
    brain_inflammation_level_t severity,
    uint64_t* landmark_id
) {
    /* Guard clauses */
    if (!bridge || !landmark_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_immune_create_sickness_landmark: required parameter is NULL (bridge, landmark_id)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    autobiographical_immune_bridge_heartbeat("autobiograph_autobio_immune_creat", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    int result = autobio_immune_create_sickness_landmark_unlocked(bridge, severity, landmark_id);
    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

/**
 * @brief Internal unlocked variant - caller must hold bridge->base.mutex
 */
static int autobio_immune_close_sickness_landmark_unlocked(
    autobio_immune_bridge_t* bridge,
    uint64_t landmark_id
) {
    if (!bridge->autobio_memory) return -1;
    if (landmark_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "autobio_immune_close_sickness_landmark: landmark_id is zero");
        return -1;
    }

    /* Find landmark */
    sickness_landmark_t* landmark = NULL;
    for (uint32_t i = 0; i < bridge->sickness_landmark_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->sickness_landmark_count > 256) {
            autobiographical_immune_bridge_heartbeat("autobiograph_loop",
                             (float)(i + 1) / (float)bridge->sickness_landmark_count);
        }

        if (bridge->sickness_landmarks[i].memory_id == landmark_id) {
            landmark = &bridge->sickness_landmarks[i];
            break;
        }
    }

    if (!landmark) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_immune_close_sickness_landmark: landmark is NULL");
        return -1;
    }

    /* Mark as ended */
    landmark->end_time_ms = 0; /* Would get from system */

    /* Update memory with outcome */
    autobiographical_memory_entry_t memory;
    if (autobio_retrieve(*bridge->autobio_memory, landmark_id, &memory)) {
        snprintf(memory.outcome, AUTOBIO_MAX_OUTCOME_LEN,
                 "Recovered from %s inflammation, immune system resolved threat",
                 brain_immune_inflammation_to_string(landmark->severity));

        /* Update importance based on how it was resolved */
        memory.importance = clamp_f(memory.importance * 0.9f, 0.3f, 1.0f);
    }

    /* Clear active landmark if this was it */
    if (bridge->active_sickness_landmark_id == (uint32_t)landmark_id) {
        bridge->active_sickness_landmark_id = 0;
    }

    LOG_MODULE_INFO("autobio_immune_bridge",
              "Closed sickness landmark: %llu", (unsigned long long)landmark_id);

    return 0;
}

int autobio_immune_close_sickness_landmark(
    autobio_immune_bridge_t* bridge,
    uint64_t landmark_id
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    autobiographical_immune_bridge_heartbeat("autobiograph_autobio_immune_close", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    int result = autobio_immune_close_sickness_landmark_unlocked(bridge, landmark_id);
    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

float autobio_immune_get_encoding_efficiency(const autobio_immune_bridge_t* bridge) {
    if (!bridge) return 1.0f;
    /* Phase 8: Heartbeat at operation start */
    autobiographical_immune_bridge_heartbeat("autobiograph_autobio_immune_get_e", 0.0f);


    return bridge->cytokine_effects.total_encoding_modulation;
}

/* ============================================================================
 * Memory → Immune Implementation
 * ============================================================================ */

int autobio_immune_trigger_from_trauma_recall(
    autobio_immune_bridge_t* bridge,
    const autobiographical_memory_entry_t* memory
) {
    /* Guard clauses */
    if (!bridge || !memory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_immune_trigger_from_trauma_recall: required parameter is NULL (bridge, memory)");
        return -1;
    }
    if (!bridge->enable_trauma_memory_immune_trigger) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_immune_trigger_from_trauma_recall: bridge->immune_system is NULL");
        return -1;
    }

    /* Check if memory is traumatic enough to trigger immune */
    /* Phase 8: Heartbeat at operation start */
    autobiographical_immune_bridge_heartbeat("autobiograph_autobio_immune_trigg", 0.0f);


    bool is_trauma = (memory->type == AUTOBIO_FAILURE ||
                      memory->type == AUTOBIO_CRISIS) &&
                     (memory->valence <= VALENCE_NEGATIVE) &&
                     (memory->importance >= TRAUMA_MEMORY_IMMUNE_THRESHOLD);

    if (!is_trauma) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update trigger state */
    bridge->memory_trigger.memory_type = memory->type;
    bridge->memory_trigger.valence = memory->valence;
    bridge->memory_trigger.importance = memory->importance;
    bridge->memory_trigger.emotional_intensity = memory->emotional_intensity;
    bridge->memory_trigger.trauma_triggered = true;

    /* Compute cortisol and inflammatory response */
    bridge->memory_trigger.cortisol_release =
        memory->importance * memory->emotional_intensity;
    bridge->memory_trigger.inflammatory_response =
        bridge->memory_trigger.cortisol_release * 0.6f; /* Rebound after cortisol */

    bridge->memory_triggered_responses++;
    bridge->trauma_recalls++;

    /* Trigger immune system (simplified - would create antigen) */
    if (bridge->memory_trigger.inflammatory_response > 0.5f) {
        /* High trauma recall triggers immune activation */
        LOG_MODULE_DEBUG("autobio_immune_bridge",
                  "Trauma recall triggered immune response: importance=%.2f",
                  memory->importance);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int autobio_immune_ruminate_on_negative_memory(
    autobio_immune_bridge_t* bridge,
    uint64_t memory_id
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_rumination_tracking) return 0;
    if (!bridge->autobio_memory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_immune_ruminate_on_negative_memory: bridge->autobio_memory is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    autobiographical_immune_bridge_heartbeat("autobiograph_autobio_immune_rumin", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Track rumination */
    bridge->memory_trigger.rumination_count++;
    bridge->memory_trigger.chronic_stress_active =
        (bridge->memory_trigger.rumination_count > 5);

    /* Chronic rumination escalates inflammation */
    if (bridge->memory_trigger.chronic_stress_active) {
        bridge->memory_trigger.inflammatory_response =
            clamp_f(bridge->memory_trigger.inflammatory_response * 1.2f, 0.0f, 1.0f);

        LOG_MODULE_DEBUG("autobio_immune_bridge",
                  "Chronic rumination detected, escalating inflammation");
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int autobio_immune_boost_from_positive_memory(
    autobio_immune_bridge_t* bridge,
    const autobiographical_memory_entry_t* memory
) {
    /* Guard clauses */
    if (!bridge || !memory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_immune_boost_from_positive_memory: required parameter is NULL (bridge, memory)");
        return -1;
    }
    if (!bridge->enable_positive_memory_immune_boost) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_immune_boost_from_positive_memory: bridge->immune_system is NULL");
        return -1;
    }

    /* Check if memory is positive enough to boost immune */
    /* Phase 8: Heartbeat at operation start */
    autobiographical_immune_bridge_heartbeat("autobiograph_autobio_immune_boost", 0.0f);


    bool is_positive = (memory->valence > VALENCE_NEUTRAL) &&
                       (memory->importance > 0.4f);

    if (!is_positive) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Track positive memory types */
    if (memory->type == AUTOBIO_ACHIEVEMENT) {
        bridge->positive_boost.achievement_count++;
    } else if (memory->type == AUTOBIO_LEARNING) {
        bridge->positive_boost.learning_count++;
    } else if (memory->type == AUTOBIO_INTERACTION) {
        bridge->positive_boost.social_bond_count++;
    }

    /* Compute immune benefits */
    bridge->positive_boost.positive_valence_avg =
        (float)(memory->valence - VALENCE_NEUTRAL) / 2.0f;
    bridge->positive_boost.immune_enhancement =
        memory->importance * bridge->positive_boost.positive_valence_avg * 0.3f;
    bridge->positive_boost.cortisol_reduction =
        bridge->positive_boost.immune_enhancement * 0.5f;
    bridge->positive_boost.il10_release_boost =
        bridge->positive_boost.immune_enhancement * 0.4f;
    bridge->positive_boost.resilience_factor =
        clamp_f(bridge->positive_boost.immune_enhancement, 0.0f, 1.0f);

    bridge->positive_boosts++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

bool autobio_immune_is_identity_threatening(
    const autobiographical_memory_entry_t* memory
) {
    if (!memory) {
        return false;
    }

    /* Identity-threatening: core memory + negative + high importance */
    /* Phase 8: Heartbeat at operation start */
    autobiographical_immune_bridge_heartbeat("autobiograph_autobio_immune_is_id", 0.0f);


    return memory->is_core_memory &&
           (memory->valence <= VALENCE_NEGATIVE) &&
           (memory->importance >= 0.7f);
}

/* ============================================================================
 * Bidirectional Update Implementation
 * ============================================================================ */

int autobio_immune_bridge_update(
    autobio_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    autobiographical_immune_bridge_heartbeat("autobiograph_autobio_immune_bridg", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Update inflammation state (unlocked - we already hold the mutex) */
    autobio_immune_apply_inflammation_consolidation_effects_unlocked(bridge);

    /* Check if should create sickness landmark */
    if (bridge->enable_sickness_landmark_creation) {
        brain_inflammation_level_t level =
            get_max_inflammation_level(bridge->immune_system);

        /* Create landmark if systemic+ and no active landmark */
        if (level >= INFLAMMATION_SYSTEMIC &&
            bridge->active_sickness_landmark_id == 0) {
            uint64_t landmark_id = 0;
            autobio_immune_create_sickness_landmark_unlocked(bridge, level, &landmark_id);
        }
        /* Close landmark if inflammation resolved */
        else if (level < INFLAMMATION_REGIONAL &&
                 bridge->active_sickness_landmark_id != 0) {
            autobio_immune_close_sickness_landmark_unlocked(
                bridge, bridge->active_sickness_landmark_id);
        }
    }

    /* Apply cytokine encoding effects (unlocked - we already hold the mutex) */
    autobio_immune_apply_cytokine_encoding_effects_unlocked(bridge);

    /* Update rumination duration */
    if (bridge->memory_trigger.chronic_stress_active) {
        bridge->memory_trigger.rumination_duration_sec += (float)delta_ms / 1000.0f;
    } else {
        bridge->memory_trigger.rumination_duration_sec = 0.0f;
    }

    bridge->total_updates++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int autobio_immune_get_cytokine_effects(
    const autobio_immune_bridge_t* bridge,
    cytokine_memory_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_immune_get_cytokine_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    autobiographical_immune_bridge_heartbeat("autobiograph_autobio_immune_get_c", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->cytokine_effects, sizeof(cytokine_memory_effects_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int autobio_immune_get_inflammation_state(
    const autobio_immune_bridge_t* bridge,
    inflammation_memory_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_immune_get_inflammation_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    autobiographical_immune_bridge_heartbeat("autobiograph_autobio_immune_get_i", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(state, &bridge->inflammation_state, sizeof(inflammation_memory_state_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool autobio_immune_is_sickness_affecting_memory(const autobio_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    autobiographical_immune_bridge_heartbeat("autobiograph_autobio_immune_is_si", 0.0f);


    return (bridge->inflammation_state.current_level >= INFLAMMATION_REGIONAL) &&
           (bridge->cytokine_effects.total_encoding_modulation < 0.8f);
}

int autobio_immune_get_sickness_landmarks(
    const autobio_immune_bridge_t* bridge,
    sickness_landmark_t* landmarks,
    uint32_t max_landmarks,
    uint32_t* num_found
) {
    if (!bridge || !landmarks || !num_found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_immune_get_sickness_landmarks: required parameter is NULL (bridge, landmarks, num_found)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    autobiographical_immune_bridge_heartbeat("autobiograph_autobio_immune_get_s", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t count = bridge->sickness_landmark_count;
    if (count > max_landmarks) count = max_landmarks;

    memcpy(landmarks, bridge->sickness_landmarks,
           count * sizeof(sickness_landmark_t));
    *num_found = count;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float autobio_immune_get_consolidation_impairment(
    const autobio_immune_bridge_t* bridge
) {
    if (!bridge) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    autobiographical_immune_bridge_heartbeat("autobiograph_autobio_immune_get_c", 0.0f);


    return bridge->cytokine_effects.consolidation_impairment;
}

float autobio_immune_get_memory_decline_rate(const autobio_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    autobiographical_immune_bridge_heartbeat("autobiograph_autobio_immune_get_m", 0.0f);


    return bridge->inflammation_state.memory_decline_rate;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

#define AUTOBIOGRAPHICAL_IMMUNE_MODULE_NAME "autobiographical_immune_bridge"

/**
 * @brief Connect bridge to bio-async router
 */
int autobiographical_immune_connect_bio_async(autobio_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    autobiographical_immune_bridge_heartbeat("autobiograph_autobiographical_imm", 0.0f);


    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_AUTOBIOGRAPHICAL,
        .module_name = AUTOBIOGRAPHICAL_IMMUNE_MODULE_NAME,
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("autobiographical_immune_bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

/**
 * @brief Disconnect from bio-async router
 */
int autobiographical_immune_disconnect_bio_async(autobio_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    autobiographical_immune_bridge_heartbeat("autobiograph_autobiographical_imm", 0.0f);


    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_DEBUG("autobiographical_immune_bridge disconnected from bio-async router");
    return 0;
}

/**
 * @brief Check if bio-async is connected
 */
bool autobiographical_immune_is_bio_async_connected(const autobio_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    autobiographical_immune_bridge_heartbeat("autobiograph_autobiographical_imm", 0.0f);


    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Query KG for module self-awareness information
 * WHY:  Enable introspective self-knowledge about autobiographical immune bridge
 * HOW:  Look up entity and relations in KG
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 otherwise
 */
int autobiographical_immune_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    autobiographical_immune_bridge_heartbeat("autobiograph_autobiographical_imm", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Autobiographical_Immune_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                autobiographical_immune_bridge_heartbeat("autobiograph_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Autobiographical immune bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Autobiographical_Immune_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Autobiographical_Immune_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void autobiographical_immune_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_autobiographical_immune_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int autobiographical_immune_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "autobiographical_immune_bridge_training_begin: NULL argument");
        return -1;
    }
    /* TODO: pass cast instance as nimcp_health_agent_t* once bridge stores per-instance agent */
    autobiographical_immune_bridge_heartbeat_instance(NULL, "autobiographical_immune_bridge_training_begin", 0.0f);
    return 0;
}

int autobiographical_immune_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "autobiographical_immune_bridge_training_end: NULL argument");
        return -1;
    }
    /* TODO: pass cast instance as nimcp_health_agent_t* once bridge stores per-instance agent */
    autobiographical_immune_bridge_heartbeat_instance(NULL, "autobiographical_immune_bridge_training_end", 1.0f);
    return 0;
}

int autobiographical_immune_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "autobiographical_immune_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    /* TODO: pass cast instance as nimcp_health_agent_t* once bridge stores per-instance agent */
    autobiographical_immune_bridge_heartbeat_instance(NULL, "autobiographical_immune_bridge_training_step", progress);
    return 0;
}
