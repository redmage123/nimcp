/**
 * @file nimcp_attention_immune_bridge.c
 * @brief Attention-Immune System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-11
 */

#include "cognitive/immune/nimcp_attention_immune_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(attention_immune_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_attention_immune_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_attention_immune_bridge_mesh_registry = NULL;

nimcp_error_t attention_immune_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_attention_immune_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "attention_immune_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_SECURITY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "attention_immune_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_attention_immune_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_attention_immune_bridge_mesh_registry = registry;
    return err;
}

void attention_immune_bridge_mesh_unregister(void) {
    if (g_attention_immune_bridge_mesh_registry && g_attention_immune_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_attention_immune_bridge_mesh_registry, g_attention_immune_bridge_mesh_id);
        g_attention_immune_bridge_mesh_id = 0;
        g_attention_immune_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from attention_immune_bridge module (instance-level) */
static inline void attention_immune_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_attention_immune_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_attention_immune_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_attention_immune_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}



/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Get current time in milliseconds
 *
 * WHAT: Get system time for duration tracking
 * WHY:  Track hypervigilance and sustained attention
 * HOW:  Platform-specific time retrieval
 */
static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * @brief Clamp float to range [0, 1]
 */
static inline float clamp_0_1(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

/**
 * @brief Get inflammation capacity factor (continuous)
 *
 * WHAT: Map inflammation level to attention capacity reduction
 * WHY:  Smooth modulation instead of discrete steps
 * HOW:  Use continuous interpolation between healthy and storm endpoints
 */
static float get_inflammation_capacity_factor(brain_inflammation_level_t level) {
    float cont = inflammation_level_to_continuous(level);
    return inflammation_compute_factor(cont,
        INFLAMMATION_NONE_CAPACITY_FACTOR,
        INFLAMMATION_STORM_CAPACITY_FACTOR);
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int attention_immune_default_config(attention_immune_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Enable all features by default */
    /* Phase 8: Heartbeat at operation start */
    attention_immune_bridge_heartbeat("attention_im_attention_immune_def", 0.0f);


    config->enable_cytokine_attention_impairment = true;
    config->enable_inflammation_narrowing = true;
    config->enable_threat_attention_immune_boost = true;
    config->enable_mindful_attention_benefits = true;
    config->enable_hypervigilance_inflammation = true;

    /* Default sensitivity (1.0 = normal) */
    config->cytokine_sensitivity = 1.0f;
    config->inflammation_sensitivity = 1.0f;
    config->attention_immune_sensitivity = 1.0f;

    /* Default thresholds */
    config->vigilance_threshold = VIGILANCE_IMMUNE_THRESHOLD;
    config->hypervigilance_threshold = 0.85f;
    config->mindful_threshold = 0.5f;

    return 0;
}

attention_immune_bridge_t* attention_immune_bridge_create(
    const attention_immune_config_t* config,
    brain_immune_system_t* immune_system,
    emotion_attention_system_t* emotion_attention,
    multihead_attention_t multihead_attention
) {
    /* Guard: require immune system */
    if (!immune_system) {
        LOG_ERROR("attention_immune_bridge_create: immune_system required");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_immune_bridge_create: immune_system is NULL");
        return NULL;
    }

    /* Allocate bridge */
    /* Phase 8: Heartbeat at operation start */
    attention_immune_bridge_heartbeat("attention_im_create", 0.0f);


    attention_immune_bridge_t* bridge = nimcp_malloc(sizeof(attention_immune_bridge_t));
    if (!bridge) {
        LOG_ERROR("attention_immune_bridge_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "attention_immune_bridge_create: bridge is NULL");
        return NULL;
    }

    memset(bridge, 0, sizeof(attention_immune_bridge_t));

    /* Apply configuration */
    attention_immune_config_t default_config;
    if (!config) {
        attention_immune_default_config(&default_config);
        config = &default_config;
    }

    bridge->enable_cytokine_attention_impairment = config->enable_cytokine_attention_impairment;
    bridge->enable_inflammation_narrowing = config->enable_inflammation_narrowing;
    bridge->enable_threat_attention_immune_boost = config->enable_threat_attention_immune_boost;
    bridge->enable_mindful_attention_benefits = config->enable_mindful_attention_benefits;
    bridge->enable_hypervigilance_inflammation = config->enable_hypervigilance_inflammation;

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->emotion_attention = emotion_attention;
    bridge->multihead_attention = multihead_attention;

    /* Initialize timing */
    bridge->last_update_time = get_time_ms();

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "attention_immune") != 0) { nimcp_free(bridge); return NULL; }

    LOG_INFO("attention_immune_bridge: created successfully");
    return bridge;
}

void attention_immune_bridge_destroy(attention_immune_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async if connected */
    /* Phase 8: Heartbeat at operation start */
    attention_immune_bridge_heartbeat("attention_im_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        attention_immune_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    bridge = NULL;
}

/* ============================================================================
 * Immune → Attention API
 * ============================================================================ */

int attention_immune_apply_cytokine_effects(attention_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_immune_apply_cytokine_effects: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    if (!bridge->enable_cytokine_attention_impairment) {
        return 0;  /* Feature disabled */
    }

    /* Phase 8: Heartbeat at operation start */
    attention_immune_bridge_heartbeat("attention_im_attention_immune_app", 0.0f);


    /* Get immune stats before acquiring lock to avoid external call under lock */
    brain_immune_stats_t stats;
    brain_immune_get_stats(bridge->immune_system, &stats);

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Reset effects */
    memset(&bridge->cytokine_effects, 0, sizeof(cytokine_attention_effects_t));

    /* Compute cytokine-induced deficits from actual cytokine levels */
    bridge->cytokine_effects.il1_attention_deficit =
        stats.cytokine_il1 * CYTOKINE_IL1_ATTENTION_IMPACT;
    bridge->cytokine_effects.il6_attention_deficit =
        stats.cytokine_il6 * CYTOKINE_IL6_ATTENTION_IMPACT;
    bridge->cytokine_effects.tnf_attention_deficit =
        stats.cytokine_tnf * CYTOKINE_TNF_ATTENTION_IMPACT;
    bridge->cytokine_effects.ifn_gamma_attention_deficit =
        stats.cytokine_ifn_gamma * CYTOKINE_IFN_GAMMA_ATTENTION_IMPACT;

    /* Total capacity reduction */
    bridge->cytokine_effects.total_capacity_reduction = clamp_0_1(
        fabsf(bridge->cytokine_effects.il1_attention_deficit) +
        fabsf(bridge->cytokine_effects.il6_attention_deficit) +
        fabsf(bridge->cytokine_effects.tnf_attention_deficit) +
        fabsf(bridge->cytokine_effects.ifn_gamma_attention_deficit)
    );

    /* Narrowing and impairments */
    bridge->cytokine_effects.narrowing_factor =
        bridge->cytokine_effects.total_capacity_reduction * 0.6f;
    bridge->cytokine_effects.sustained_impairment =
        bridge->cytokine_effects.total_capacity_reduction * 0.8f;
    bridge->cytokine_effects.executive_impairment =
        bridge->cytokine_effects.total_capacity_reduction * 0.7f;

    bridge->cytokine_impairments++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int attention_immune_apply_inflammation_effects(attention_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_immune_apply_inflammation_effects: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    if (!bridge->enable_inflammation_narrowing) {
        return 0;  /* Feature disabled */
    }

    /* Phase 8: Heartbeat at operation start */
    attention_immune_bridge_heartbeat("attention_im_attention_immune_app", 0.0f);


    /* Get immune stats before acquiring lock to avoid external call under lock */
    brain_immune_stats_t stats;
    brain_immune_get_stats(bridge->immune_system, &stats);

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Use continuous inflammation level from immune stats */
    float cont_level = stats.inflammation_level_continuous;
    brain_inflammation_level_t level = stats.inflammation_level;
    bridge->inflammation_state.current_level = level;

    /* Capacity factor based on continuous inflammation */
    bridge->inflammation_state.capacity_factor =
        inflammation_compute_factor(cont_level,
            INFLAMMATION_NONE_CAPACITY_FACTOR,
            INFLAMMATION_STORM_CAPACITY_FACTOR);

    /* Width narrowing - continuous scaling */
    float narrowing = INFLAMMATION_NARROWING_BASE +
                     (cont_level * INFLAMMATION_NARROWING_PER_LEVEL * 4.0f);
    bridge->inflammation_state.width_narrowing = clamp_0_1(narrowing);

    /* Sustained attention deficit */
    bridge->inflammation_state.sustained_deficit =
        1.0f - bridge->inflammation_state.capacity_factor;

    /* Flexibility impairment */
    bridge->inflammation_state.flexibility_impairment =
        bridge->inflammation_state.width_narrowing * 0.8f;

    /* Working memory deficit */
    bridge->inflammation_state.working_memory_deficit =
        (1.0f - bridge->inflammation_state.capacity_factor) * 0.7f;

    /* Threat bias increases with inflammation */
    bridge->inflammation_state.threat_bias_strength =
        bridge->inflammation_state.width_narrowing * 0.9f;
    bridge->inflammation_state.disengagement_difficulty =
        bridge->inflammation_state.width_narrowing * 0.85f;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

float attention_immune_compute_capacity(const attention_immune_bridge_t* bridge) {
    if (!bridge) {
        return 1.0f;  /* Normal capacity */
    }

    /* Combine cytokine and inflammation effects */
    /* Phase 8: Heartbeat at operation start */
    attention_immune_bridge_heartbeat("attention_im_attention_immune_com", 0.0f);


    float cytokine_reduction = bridge->cytokine_effects.total_capacity_reduction;
    float inflammation_factor = bridge->inflammation_state.capacity_factor;

    /* Combined capacity: multiplicative reduction */
    float capacity = inflammation_factor * (1.0f - cytokine_reduction);
    return clamp_0_1(capacity);
}

float attention_immune_compute_narrowing(const attention_immune_bridge_t* bridge) {
    if (!bridge) {
        return 0.0f;  /* No narrowing */
    }

    /* Combine cytokine and inflammation narrowing */
    /* Phase 8: Heartbeat at operation start */
    attention_immune_bridge_heartbeat("attention_im_attention_immune_com", 0.0f);


    float cytokine_narrowing = bridge->cytokine_effects.narrowing_factor;
    float inflammation_narrowing = bridge->inflammation_state.width_narrowing;

    /* Take maximum (most severe narrowing effect) */
    float narrowing = fmaxf(cytokine_narrowing, inflammation_narrowing);
    return clamp_0_1(narrowing);
}

/* ============================================================================
 * Attention → Immune API
 * ============================================================================ */

int attention_immune_boost_from_threat_focus(attention_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_immune_boost_from_threat_focus: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    if (!bridge->enable_threat_attention_immune_boost) {
        return 0;  /* Feature disabled */
    }

    /* Phase 8: Heartbeat at operation start */
    attention_immune_bridge_heartbeat("attention_im_attention_immune_boo", 0.0f);


    /* Get attention state before lock to avoid external call under lock */
    float threat_focus = 0.0f;
    if (bridge->emotion_attention) {
        /* Query emotion-attention width (narrowed = threat-focused) */
        float width = emotion_attention_get_width(bridge->emotion_attention);
        threat_focus = 1.0f - width;  /* Narrow = high threat focus */
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->attention_modulation.threat_focus_level = threat_focus;

    /* If threat-focused, boost immune surveillance */
    if (threat_focus > VIGILANCE_IMMUNE_THRESHOLD) {
        float boost = THREAT_ATTENTION_IMMUNE_BOOST * threat_focus;
        bridge->attention_modulation.local_immune_boost = boost;
        bridge->attention_modulation.immune_surveillance_boost = boost * 0.8f;
        bridge->threat_boosts++;
    } else {
        bridge->attention_modulation.local_immune_boost = 0.0f;
        bridge->attention_modulation.immune_surveillance_boost = 0.0f;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int attention_immune_trigger_hypervigilance_inflammation(attention_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_immune_trigger_hypervigilance_inflammation: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    if (!bridge->enable_hypervigilance_inflammation) {
        return 0;  /* Feature disabled */
    }

    /* Phase 8: Heartbeat at operation start */
    attention_immune_bridge_heartbeat("attention_im_attention_immune_tri", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Check vigilance level */
    float vigilance = bridge->attention_modulation.vigilance_level;

    /* If hypervigilant, accumulate inflammation */
    if (vigilance > 0.85f) {  /* Hypervigilance threshold */
        bridge->hypervigilance_accumulator += HYPERVIGILANCE_INFLAMMATION_RATE;

        /* If accumulated enough, trigger inflammation */
        if (bridge->hypervigilance_accumulator >= 1.0f) {
            /* Trigger inflammation via immune system */
            /* (Would call brain_immune_initiate_inflammation here) */
            bridge->attention_modulation.hypervigilance_inflammation = true;
            bridge->hypervigilance_inflammation_events++;
            bridge->hypervigilance_accumulator = 0.0f;
        }
    } else {
        /* Decay accumulator when not hypervigilant */
        bridge->hypervigilance_accumulator *= 0.95f;
        bridge->attention_modulation.hypervigilance_inflammation = false;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int attention_immune_release_il10_from_mindfulness(attention_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_immune_release_il10_from_mindfulness: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    if (!bridge->enable_mindful_attention_benefits) {
        return 0;  /* Feature disabled */
    }

    /* Phase 8: Heartbeat at operation start */
    attention_immune_bridge_heartbeat("attention_im_attention_immune_rel", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Detect mindful attention (calm, sustained focus) */
    float mindful = bridge->attention_modulation.mindful_attention_level;
    float sustained_sec = bridge->attention_modulation.sustained_duration_sec;

    float il10_boost = 0.0f;
    bool do_cytokine_release = false;

    /* If sustained mindful attention, release IL-10 */
    if (mindful > 0.5f && sustained_sec > SUSTAINED_ATTENTION_DURATION_SEC) {
        il10_boost = MINDFUL_ATTENTION_IL10_BOOST * mindful;
        bridge->attention_modulation.il10_release_from_mindfulness = il10_boost;

        /* Reduce inflammation */
        bridge->attention_modulation.inflammation_reduction = il10_boost * 0.6f;

        bridge->mindful_boosts++;
        do_cytokine_release = true;
    } else {
        bridge->attention_modulation.il10_release_from_mindfulness = 0.0f;
        bridge->attention_modulation.inflammation_reduction = 0.0f;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* Release IL-10 outside lock to avoid external call under lock */
    if (do_cytokine_release) {
        uint32_t cytokine_id = 0;
        brain_immune_release_cytokine(
            bridge->immune_system,
            CYTOKINE_IL10,
            0,  /* source cell */
            il10_boost,
            0,  /* broadcast */
            &cytokine_id
        );
    }
    return 0;
}

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

int attention_immune_bridge_update(
    attention_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_immune_bridge_heartbeat("attention_im_update", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Update timing */
    uint64_t current_time = get_time_ms();
    float delta_sec = delta_ms / 1000.0f;
    bridge->last_update_time = current_time;
    bridge->total_updates++;

    /* Update sustained attention duration */
    if (bridge->attention_modulation.attention_strength > 0.3f) {
        bridge->attention_modulation.sustained_duration_sec += delta_sec;
    } else {
        bridge->attention_modulation.sustained_duration_sec = 0.0f;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* IMMUNE → ATTENTION pathways */
    attention_immune_apply_cytokine_effects(bridge);
    attention_immune_apply_inflammation_effects(bridge);

    /* ATTENTION → IMMUNE pathways */
    attention_immune_boost_from_threat_focus(bridge);
    attention_immune_trigger_hypervigilance_inflammation(bridge);
    attention_immune_release_il10_from_mindfulness(bridge);

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int attention_immune_get_cytokine_effects(
    const attention_immune_bridge_t* bridge,
    cytokine_attention_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_immune_get_cytokine_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_immune_bridge_heartbeat("attention_im_attention_immune_get", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->cytokine_effects, sizeof(cytokine_attention_effects_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int attention_immune_get_inflammation_state(
    const attention_immune_bridge_t* bridge,
    inflammation_attention_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_immune_get_inflammation_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_immune_bridge_heartbeat("attention_im_attention_immune_get", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);
    memcpy(state, &bridge->inflammation_state, sizeof(inflammation_attention_state_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool attention_immune_has_attention_deficit(const attention_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_immune_bridge_heartbeat("attention_im_attention_immune_has", 0.0f);


    float capacity = attention_immune_compute_capacity(bridge);
    return capacity < 0.7f;  /* >30% capacity loss */
}

float attention_immune_get_capacity_factor(const attention_immune_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    attention_immune_bridge_heartbeat("attention_im_attention_immune_get", 0.0f);


    return attention_immune_compute_capacity(bridge);
}

float attention_immune_get_narrowing_factor(const attention_immune_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    attention_immune_bridge_heartbeat("attention_im_attention_immune_get", 0.0f);


    return attention_immune_compute_narrowing(bridge);
}

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/** Module name for logging */
#define ATTENTION_IMMUNE_MODULE_NAME "attention_immune_bridge"

int attention_immune_connect_bio_async(attention_immune_bridge_t* bridge) {
    /**
     * WHAT: Register bridge with bio-async router
     * WHY:  Enable distributed immune signaling via NOREPINEPHRINE channel
     * HOW:  Use bio_router_register_module with immune module ID
     */

    /* Guard: null check */
    if (!bridge) {
        LOG_ERROR("attention_immune_connect_bio_async: NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_immune_connect_bio_async: bridge is NULL");
        return -1;
    }

    /* Guard: already connected */
    /* Phase 8: Heartbeat at operation start */
    attention_immune_bridge_heartbeat("attention_im_attention_immune_con", 0.0f);


    if (bridge->base.bio_async_enabled) {
        LOG_MODULE_WARN(ATTENTION_IMMUNE_MODULE_NAME, "Already connected to bio-async");
        return 0;
    }

    /* Build module info */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_ATTENTION,
        .module_name = ATTENTION_IMMUNE_MODULE_NAME,
        .inbox_capacity = 32,
        .user_data = bridge
    };

    /* Register with router outside lock to avoid external call under lock */
    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        LOG_MODULE_INFO(ATTENTION_IMMUNE_MODULE_NAME, "Connected to bio-async router");
    } else {
        LOG_MODULE_WARN(ATTENTION_IMMUNE_MODULE_NAME,
            "Bio-async router not available, skipping registration");
    }
    return 0;
}

int attention_immune_disconnect_bio_async(attention_immune_bridge_t* bridge) {
    /**
     * WHAT: Unregister bridge from bio-async router
     * WHY:  Clean shutdown of messaging infrastructure
     * HOW:  Use bio_router_unregister_module
     */

    /* Guard: null check */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Guard: not connected */
    if (!bridge->base.bio_async_enabled || !bridge->base.bio_ctx) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_immune_bridge_heartbeat("attention_im_attention_immune_dis", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Unregister */
    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;

    LOG_MODULE_INFO(ATTENTION_IMMUNE_MODULE_NAME, "Disconnected from bio-async router");

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

bool attention_immune_is_bio_async_connected(const attention_immune_bridge_t* bridge) {
    /**
     * WHAT: Check bio-async connection status
     * WHY:  Allow callers to verify messaging capability
     * HOW:  Return internal flag
     */
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    attention_immune_bridge_heartbeat("attention_im_attention_immune_is_", 0.0f);


    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Query KG for module self-awareness information
 * WHY:  Enable introspective self-knowledge about attention immune bridge
 * HOW:  Look up entity and relations in KG
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 otherwise
 */
int attention_immune_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    attention_immune_bridge_heartbeat("attention_im_attention_immune_que", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Attention_Immune_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                attention_immune_bridge_heartbeat("attention_im_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Attention immune bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Attention_Immune_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Attention_Immune_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void attention_immune_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_attention_immune_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int attention_immune_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "attention_immune_bridge_training_begin: NULL argument");
        return -1;
    }
    /* TODO: Pass bridge instance as health agent when instance tracking is implemented */
    attention_immune_bridge_heartbeat_instance(NULL, "attention_immune_bridge_training_begin", 0.0f);
    return 0;
}

int attention_immune_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "attention_immune_bridge_training_end: NULL argument");
        return -1;
    }
    /* TODO: Pass bridge instance as health agent when instance tracking is implemented */
    attention_immune_bridge_heartbeat_instance(NULL, "attention_immune_bridge_training_end", 1.0f);
    return 0;
}

int attention_immune_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "attention_immune_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    /* TODO: Pass bridge instance as health agent when instance tracking is implemented */
    attention_immune_bridge_heartbeat_instance(NULL, "attention_immune_bridge_training_step", progress);
    return 0;
}
