/**
 * @file nimcp_sleep_immune_bridge.c
 * @brief Sleep-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional coupling between brain immune and sleep-wake systems
 * WHY:  Biological realism - cytokines induce sleep, sleep enhances immunity
 * HOW:  Monitor cytokine levels to modulate sleep, monitor sleep stages to boost immune function
 */

#include "cognitive/immune/nimcp_sleep_immune_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>
//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(sleep_immune_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_sleep_immune_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_sleep_immune_bridge_mesh_registry = NULL;

nimcp_error_t sleep_immune_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_sleep_immune_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "sleep_immune_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_SECURITY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "sleep_immune_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_sleep_immune_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_sleep_immune_bridge_mesh_registry = registry;
    return err;
}

void sleep_immune_bridge_mesh_unregister(void) {
    if (g_sleep_immune_bridge_mesh_registry && g_sleep_immune_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_sleep_immune_bridge_mesh_registry, g_sleep_immune_bridge_mesh_id);
        g_sleep_immune_bridge_mesh_id = 0;
        g_sleep_immune_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from sleep_immune_bridge module (instance-level) */
static inline void sleep_immune_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_sleep_immune_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_sleep_immune_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_sleep_immune_bridge_health_agent) {
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
 * WHY:  Need individual cytokine levels for sleep effects
 * HOW:  Query immune system cytokine array
 *
 * NOTE: Placeholder - actual implementation would query brain_immune_system_t
 */
static float get_cytokine_level(
    const brain_immune_system_t* immune,
    brain_cytokine_type_t type
) {
    if (!immune) return 0.0f;

    /* Query immune system for cytokine concentration */
    /* For now, return 0 - actual implementation would search cytokines array */
    for (size_t i = 0; i < immune->cytokine_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && immune->cytokine_count > 256) {
            sleep_immune_bridge_heartbeat("sleep_immune_loop",
                             (float)(i + 1) / (float)immune->cytokine_count);
        }

        if (immune->cytokines[i].type == type && !immune->cytokines[i].delivered) {
            return immune->cytokines[i].concentration;
        }
    }

    return 0.0f;
}

/**
 * @brief Get maximum inflammation level
 *
 * WHAT: Get highest inflammation level in system
 * WHY:  Max inflammation determines sleep disruption
 * HOW:  Query immune system for max inflammation site level
 */
static brain_inflammation_level_t get_max_inflammation_level(
    const brain_immune_system_t* immune
) {
    if (!immune) return INFLAMMATION_NONE;

    brain_inflammation_level_t max_level = INFLAMMATION_NONE;
    for (size_t i = 0; i < immune->inflammation_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && immune->inflammation_count > 256) {
            sleep_immune_bridge_heartbeat("sleep_immune_loop",
                             (float)(i + 1) / (float)immune->inflammation_count);
        }

        if (immune->inflammation_sites[i].level > max_level) {
            max_level = immune->inflammation_sites[i].level;
        }
    }

    return max_level;
}

/**
 * @brief Get inflammation duration
 *
 * WHAT: Calculate how long inflammation has persisted
 * WHY:  Chronic inflammation has different sleep effects
 * HOW:  Find oldest active inflammation site, compute duration
 */
static float get_inflammation_duration_sec(
    const brain_immune_system_t* immune,
    uint64_t current_time_ms
) {
    if (!immune || immune->inflammation_count == 0) return 0.0f;

    /* Find oldest inflammation site */
    uint64_t oldest_start = immune->inflammation_sites[0].start_time;
    for (size_t i = 1; i < immune->inflammation_count; i++) {
        if (immune->inflammation_sites[i].start_time < oldest_start) {
            oldest_start = immune->inflammation_sites[i].start_time;
        }
    }

    return (float)(current_time_ms - oldest_start) / 1000.0f;
}

/**
 * @brief Convert milliseconds to hours
 */
static inline float ms_to_hours(uint64_t ms) {
    return (float)ms / (1000.0f * 60.0f * 60.0f);
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int sleep_immune_default_config(sleep_immune_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* All features enabled by default */
    /* Phase 8: Heartbeat at operation start */
    sleep_immune_bridge_heartbeat("sleep_immune_sleep_immune_default", 0.0f);


    config->enable_cytokine_sleep_modulation = true;
    config->enable_inflammation_sleep_disruption = true;
    config->enable_sleep_immune_enhancement = true;
    config->enable_sleep_deprivation_tracking = true;
    config->enable_rem_memory_consolidation = true;

    /* Biologically-based default sensitivities */
    config->cytokine_sensitivity = 1.0f;
    config->inflammation_sensitivity = 1.0f;
    config->sleep_enhancement_factor = 1.0f;

    /* Evidence-based thresholds */
    config->sleep_deprivation_hours = SLEEP_DEPRIVATION_HOURS;
    config->inflammation_fragmentation_threshold = INFLAMMATION_FRAGMENTATION_THRESHOLD;

    return 0;
}

sleep_immune_bridge_t* sleep_immune_bridge_create(
    const sleep_immune_config_t* config,
    brain_immune_system_t* immune_system,
    sleep_system_t sleep_system
) {
    /* Guard: require immune and sleep systems */
    if (!immune_system || !sleep_system) {
        LOG_MODULE_ERROR("sleep_immune_bridge",
                  "Cannot create bridge without immune and sleep systems");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_immune_bridge_create: required parameter is NULL (immune_system, sleep_system)");
        return NULL;
    }

    /* Allocate bridge */
    /* Phase 8: Heartbeat at operation start */
    sleep_immune_bridge_heartbeat("sleep_immune_create", 0.0f);


    sleep_immune_bridge_t* bridge = (sleep_immune_bridge_t*)
        nimcp_malloc(sizeof(sleep_immune_bridge_t));
    if (!bridge) {
        LOG_MODULE_ERROR("sleep_immune_bridge", "Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(sleep_immune_bridge_t));

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->sleep_system = sleep_system;

    /* Apply configuration */
    if (config) {
        bridge->enable_cytokine_sleep_modulation = config->enable_cytokine_sleep_modulation;
        bridge->enable_inflammation_sleep_disruption = config->enable_inflammation_sleep_disruption;
        bridge->enable_sleep_immune_enhancement = config->enable_sleep_immune_enhancement;
        bridge->enable_sleep_deprivation_tracking = config->enable_sleep_deprivation_tracking;
        bridge->enable_rem_memory_consolidation = config->enable_rem_memory_consolidation;
    } else {
        /* Use defaults */
        sleep_immune_config_t default_cfg;
        sleep_immune_default_config(&default_cfg);
        bridge->enable_cytokine_sleep_modulation = default_cfg.enable_cytokine_sleep_modulation;
        bridge->enable_inflammation_sleep_disruption = default_cfg.enable_inflammation_sleep_disruption;
        bridge->enable_sleep_immune_enhancement = default_cfg.enable_sleep_immune_enhancement;
        bridge->enable_sleep_deprivation_tracking = default_cfg.enable_sleep_deprivation_tracking;
        bridge->enable_rem_memory_consolidation = default_cfg.enable_rem_memory_consolidation;
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "sleep_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);    return NULL;
    }

    LOG_MODULE_INFO("sleep_immune_bridge", "Bridge created successfully");
    return bridge;
}

void sleep_immune_bridge_destroy(sleep_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Destroy mutex */
    /* Phase 8: Heartbeat at operation start */
    sleep_immune_bridge_heartbeat("sleep_immune_destroy", 0.0f);


    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free bridge (don't destroy linked systems - we don't own them) */
    nimcp_free(bridge);
    bridge = NULL;
    LOG_MODULE_INFO("sleep_immune_bridge", "Bridge destroyed");
}

/* ============================================================================
 * Immune → Sleep Implementation
 * ============================================================================ */

int sleep_immune_apply_cytokine_effects(sleep_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_cytokine_sleep_modulation) return 0;
    if (!bridge->immune_system || !bridge->sleep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_immune_apply_cytokine_effects: required parameter is NULL (bridge->immune_system, bridge->sleep_system)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    sleep_immune_bridge_heartbeat("sleep_immune_sleep_immune_apply_c", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute cytokine effects */
    cytokine_sleep_effects_t* effects = &bridge->cytokine_effects;

    /* Pro-inflammatory cytokines → increase sleep pressure */
    float il1_level = get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IL1);
    float tnf_level = get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_TNF);
    float il6_level = get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IL6);

    effects->il1_sleep_pressure = il1_level * CYTOKINE_IL1_SLEEP_PRESSURE;
    effects->tnf_sleep_pressure = tnf_level * CYTOKINE_TNF_SLEEP_PRESSURE;
    effects->il6_sleep_pressure = il6_level * CYTOKINE_IL6_SLEEP_PRESSURE;

    /* Anti-inflammatory cytokines → improve sleep quality */
    float il10_level = get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IL10);
    effects->il10_sleep_quality = il10_level * CYTOKINE_IL10_SLEEP_QUALITY;

    /* Aggregate effects */
    effects->total_sleep_pressure_bonus =
        effects->il1_sleep_pressure +
        effects->tnf_sleep_pressure +
        effects->il6_sleep_pressure;

    /* Sleep quality modifier (1.0 = optimal, <1.0 = impaired, >1.0 = enhanced) */
    effects->sleep_quality_modifier = 1.0f + effects->il10_sleep_quality;

    /* Sickness behavior sleep drive */
    float proinflam_total = il1_level + tnf_level + il6_level;
    effects->sickness_sleep_drive = clamp_f(proinflam_total * 0.5f, 0.0f, 1.0f);

    /* Save pressure amount for external call after unlock */
    uint32_t pressure_to_accumulate = 0;
    if (effects->total_sleep_pressure_bonus > 0.01f) {
        pressure_to_accumulate = (uint32_t)(effects->total_sleep_pressure_bonus * 100.0f);
    }

    bridge->cytokine_modulations++;
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Apply to sleep system outside lock to avoid deadlock */
    if (pressure_to_accumulate > 0) {
        sleep_accumulate_pressure(bridge->sleep_system, pressure_to_accumulate);
    }
    return 0;
}

int sleep_immune_apply_inflammation_effects(sleep_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_inflammation_sleep_disruption) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_immune_apply_inflammation_effects: bridge->immune_system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    sleep_immune_bridge_heartbeat("sleep_immune_sleep_immune_apply_i", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    inflammation_sleep_state_t* state = &bridge->inflammation_state;

    /* Get inflammation state */
    state->current_level = get_max_inflammation_level(bridge->immune_system);
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t current_time = (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
    state->inflammation_duration_sec = get_inflammation_duration_sec(
        bridge->immune_system, current_time);
    state->is_chronic = (state->inflammation_duration_sec >= 604800.0f); /* 7 days */

    /* Inflammation severity as fraction */
    float inflammation_intensity = (float)state->current_level / (float)INFLAMMATION_STORM;

    /* Sleep fragmentation from inflammation */
    if (inflammation_intensity >= INFLAMMATION_FRAGMENTATION_THRESHOLD) {
        state->fragmentation_severity = (inflammation_intensity - INFLAMMATION_FRAGMENTATION_THRESHOLD) /
                                       (1.0f - INFLAMMATION_FRAGMENTATION_THRESHOLD);
        state->fragmentation_severity = clamp_f(state->fragmentation_severity, 0.0f, 1.0f);
    } else {
        state->fragmentation_severity = 0.0f;
    }

    /* Quality impairment */
    state->quality_impairment = clamp_f(inflammation_intensity * 0.7f, 0.0f, 1.0f);

    /* REM suppression (inflammation preferentially disrupts REM) */
    state->rem_suppression = clamp_f(inflammation_intensity * 0.6f, 0.0f, 1.0f);

    /* Deep sleep reduction */
    state->deep_sleep_reduction = clamp_f(inflammation_intensity * 0.4f, 0.0f, 1.0f);

    /* Cytokine storm increases sleep need (sickness behavior) */
    if (state->current_level == INFLAMMATION_STORM) {
        state->sickness_sleep_multiplier = INFLAMMATION_STORM_SLEEP_MULTIPLIER;
    } else {
        state->sickness_sleep_multiplier = 1.0f;
    }

    /* Store fragmentation level in cytokine effects for query */
    bridge->cytokine_effects.fragmentation_level = state->fragmentation_severity;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float sleep_immune_compute_cytokine_sleep_pressure(const sleep_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    sleep_immune_bridge_heartbeat("sleep_immune_sleep_immune_compute", 0.0f);


    return bridge->cytokine_effects.total_sleep_pressure_bonus;
}

bool sleep_immune_is_sleep_fragmented(const sleep_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    sleep_immune_bridge_heartbeat("sleep_immune_sleep_immune_is_slee", 0.0f);


    return bridge->inflammation_state.fragmentation_severity >= 0.3f;
}

/* ============================================================================
 * Sleep → Immune Implementation
 * ============================================================================ */

int sleep_immune_enhance_during_deep_sleep(sleep_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_sleep_immune_enhancement) return 0;
    if (!bridge->sleep_system || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_immune_enhance_during_deep_sleep: required parameter is NULL (bridge->sleep_system, bridge->immune_system)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    sleep_immune_bridge_heartbeat("sleep_immune_sleep_immune_enhance", 0.0f);


    /* Get current sleep state before acquiring lock (external call) */
    sleep_state_t current_sleep_state_deep = sleep_get_current_state(bridge->sleep_system);

    nimcp_mutex_lock(bridge->base.mutex);

    sleep_immune_modulation_t* modulation = &bridge->sleep_modulation;

    /* Use pre-fetched sleep state */
    modulation->current_state = current_sleep_state_deep;
    modulation->in_deep_sleep = (modulation->current_state == SLEEP_STATE_DEEP_NREM);

    /* Deep NREM enhances T cell activity and antibody production */
    if (modulation->in_deep_sleep) {
        modulation->t_cell_activity_multiplier = 1.0f + DEEP_SLEEP_T_CELL_BOOST;
        modulation->antibody_production_boost = DEEP_SLEEP_ANTIBODY_BOOST;
        modulation->memory_consolidation_rate = 0.3f; /* Moderate in deep sleep */

        /* Apply boost to immune system */
        /* Note: Would enhance T cell activation levels, antibody production rates */

        bridge->sleep_enhanced_immune_events++;
    } else {
        modulation->t_cell_activity_multiplier = 1.0f;
        modulation->antibody_production_boost = 0.0f;
        modulation->memory_consolidation_rate = 0.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sleep_immune_consolidate_memory_during_rem(sleep_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_rem_memory_consolidation) return 0;
    if (!bridge->sleep_system || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_immune_consolidate_memory_during_rem: required parameter is NULL (bridge->sleep_system, bridge->immune_system)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    sleep_immune_bridge_heartbeat("sleep_immune_sleep_immune_consoli", 0.0f);


    /* Get current sleep state before acquiring lock (external call) */
    sleep_state_t current_sleep_state_rem = sleep_get_current_state(bridge->sleep_system);

    nimcp_mutex_lock(bridge->base.mutex);

    sleep_immune_modulation_t* modulation = &bridge->sleep_modulation;

    /* Use pre-fetched sleep state */
    modulation->current_state = current_sleep_state_rem;
    modulation->in_rem_sleep = (modulation->current_state == SLEEP_STATE_REM);

    /* REM sleep consolidates immune memory */
    if (modulation->in_rem_sleep) {
        modulation->memory_consolidation_rate = REM_SLEEP_MEMORY_BOOST;

        /* Consolidate immune memory B cells */
        /* Iterate through memory B cells and strengthen patterns */
        for (size_t i = 0; i < bridge->immune_system->b_cell_count; i++) {
            if (bridge->immune_system->b_cells[i].state == B_CELL_MEMORY) {
                /* Strengthen affinity (consolidation) */
                float current_affinity = bridge->immune_system->b_cells[i].affinity;
                float consolidated = current_affinity + (REM_SLEEP_MEMORY_BOOST * 0.1f);
                bridge->immune_system->b_cells[i].affinity = clamp_f(consolidated, 0.0f, 1.0f);
            }
        }

        bridge->memory_consolidations++;
    } else {
        modulation->memory_consolidation_rate = 0.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sleep_immune_suppress_from_deprivation(sleep_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_sleep_deprivation_tracking) return 0;
    if (!bridge->sleep_system || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_immune_suppress_from_deprivation: required parameter is NULL (bridge->sleep_system, bridge->immune_system)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    sleep_immune_bridge_heartbeat("sleep_immune_sleep_immune_suppres", 0.0f);


    /* Get current sleep state before acquiring lock (external call) */
    sleep_state_t deprivation_sleep_state = sleep_get_current_state(bridge->sleep_system);

    nimcp_mutex_lock(bridge->base.mutex);

    sleep_deprivation_state_t* deprivation = &bridge->deprivation_state;

    /* Track time awake */
    sleep_state_t current_state = deprivation_sleep_state;
    if (current_state == SLEEP_STATE_AWAKE) {
        deprivation->time_awake_ms += 1000; /* Increment by update interval */
    } else {
        /* Reset on sleep */
        deprivation->time_awake_ms = 0;
        deprivation->sleep_debt_hours = 0.0f;
    }

    /* Convert to hours */
    float hours_awake = ms_to_hours(deprivation->time_awake_ms);
    deprivation->sleep_debt_hours = hours_awake;
    deprivation->is_sleep_deprived = (hours_awake >= SLEEP_DEPRIVATION_HOURS);

    /* Compute immune suppression from sleep deprivation */
    if (deprivation->is_sleep_deprived) {
        /* Linear increase in suppression after threshold */
        float excess_hours = hours_awake - SLEEP_DEPRIVATION_HOURS;
        float suppression_factor = clamp_f(excess_hours / 24.0f, 0.0f, 1.0f);

        deprivation->t_cell_suppression = suppression_factor * SLEEP_DEPRIVATION_IMMUNE_PENALTY;
        deprivation->antibody_suppression = suppression_factor * SLEEP_DEPRIVATION_IMMUNE_PENALTY;
        deprivation->memory_formation_impairment = suppression_factor * 0.7f;

        /* Apply suppression to immune system */
        /* Note: Would reduce T cell activation levels, antibody production rates */

        bridge->deprivation_suppression_events++;
    } else {
        deprivation->t_cell_suppression = 0.0f;
        deprivation->antibody_suppression = 0.0f;
        deprivation->memory_formation_impairment = 0.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sleep_immune_inflame_from_chronic_loss(sleep_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_sleep_deprivation_tracking) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_immune_inflame_from_chronic_loss: bridge->immune_system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    sleep_immune_bridge_heartbeat("sleep_immune_sleep_immune_inflame", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    sleep_deprivation_state_t* deprivation = &bridge->deprivation_state;

    /* Chronic sleep loss (>48 hours) increases inflammation */
    float hours_awake = ms_to_hours(deprivation->time_awake_ms);
    bool do_inflam_release = (hours_awake >= 48.0f);
    float pro_inflammatory_shift = 0.0f;
    if (do_inflam_release) {
        /* Pro-inflammatory shift */
        float chronic_factor = clamp_f((hours_awake - 48.0f) / 48.0f, 0.0f, 1.0f);
        deprivation->pro_inflammatory_shift = chronic_factor * 0.6f;
        deprivation->immune_dysregulation = chronic_factor * 0.5f;
        pro_inflammatory_shift = deprivation->pro_inflammatory_shift;
    } else {
        deprivation->pro_inflammatory_shift = 0.0f;
        deprivation->immune_dysregulation = 0.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Release cytokine outside lock to avoid deadlock */
    if (do_inflam_release) {
        uint32_t cytokine_id = 0;
        brain_immune_release_cytokine(
            bridge->immune_system,
            BRAIN_CYTOKINE_IL6,
            0, /* source cell */
            pro_inflammatory_shift,
            0, /* broadcast */
            &cytokine_id
        );
    }
    return 0;
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

int sleep_immune_bridge_update(
    sleep_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Apply all bidirectional effects */

    /* Immune → Sleep */
    /* Phase 8: Heartbeat at operation start */
    sleep_immune_bridge_heartbeat("sleep_immune_update", 0.0f);


    sleep_immune_apply_cytokine_effects(bridge);
    sleep_immune_apply_inflammation_effects(bridge);

    /* Sleep → Immune */
    sleep_immune_enhance_during_deep_sleep(bridge);
    sleep_immune_consolidate_memory_during_rem(bridge);
    sleep_immune_suppress_from_deprivation(bridge);
    sleep_immune_inflame_from_chronic_loss(bridge);

    /* Update base timestamp so callers see real wall-clock time */
    struct timespec upd_ts;
    clock_gettime(CLOCK_MONOTONIC, &upd_ts);
    bridge->base.last_update_time_ms =
        (uint64_t)upd_ts.tv_sec * 1000ULL + (uint64_t)upd_ts.tv_nsec / 1000000ULL;

    bridge->total_updates++;
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int sleep_immune_get_cytokine_effects(
    const sleep_immune_bridge_t* bridge,
    cytokine_sleep_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_immune_get_cytokine_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    sleep_immune_bridge_heartbeat("sleep_immune_sleep_immune_get_cyt", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->cytokine_effects, sizeof(cytokine_sleep_effects_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int sleep_immune_get_inflammation_state(
    const sleep_immune_bridge_t* bridge,
    inflammation_sleep_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_immune_get_inflammation_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    sleep_immune_bridge_heartbeat("sleep_immune_sleep_immune_get_inf", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(state, &bridge->inflammation_state, sizeof(inflammation_sleep_state_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int sleep_immune_get_sleep_modulation(
    const sleep_immune_bridge_t* bridge,
    sleep_immune_modulation_t* modulation
) {
    if (!bridge || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_immune_get_sleep_modulation: required parameter is NULL (bridge, modulation)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    sleep_immune_bridge_heartbeat("sleep_immune_sleep_immune_get_sle", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(modulation, &bridge->sleep_modulation, sizeof(sleep_immune_modulation_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int sleep_immune_get_deprivation_state(
    const sleep_immune_bridge_t* bridge,
    sleep_deprivation_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_immune_get_deprivation_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    sleep_immune_bridge_heartbeat("sleep_immune_sleep_immune_get_dep", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(state, &bridge->deprivation_state, sizeof(sleep_deprivation_state_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool sleep_immune_is_sickness_sleep(const sleep_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Sickness sleep threshold */
    /* Phase 8: Heartbeat at operation start */
    sleep_immune_bridge_heartbeat("sleep_immune_sleep_immune_is_sick", 0.0f);


    return bridge->cytokine_effects.sickness_sleep_drive >= 0.5f;
}

float sleep_immune_get_quality_impairment(const sleep_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    sleep_immune_bridge_heartbeat("sleep_immune_sleep_immune_get_qua", 0.0f);


    return bridge->inflammation_state.quality_impairment;
}

bool sleep_immune_is_sleep_deprived(const sleep_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    sleep_immune_bridge_heartbeat("sleep_immune_sleep_immune_is_slee", 0.0f);


    return bridge->deprivation_state.is_sleep_deprived;
}

float sleep_immune_get_suppression_level(const sleep_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;

    /* Return maximum suppression across T cell and antibody */
    /* Phase 8: Heartbeat at operation start */
    sleep_immune_bridge_heartbeat("sleep_immune_sleep_immune_get_sup", 0.0f);


    float t_cell_supp = bridge->deprivation_state.t_cell_suppression;
    float antibody_supp = bridge->deprivation_state.antibody_suppression;

    return fmaxf(t_cell_supp, antibody_supp);
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

#define SLEEP_IMMUNE_MODULE_NAME "sleep_immune_bridge"

/**
 * @brief Connect bridge to bio-async router
 */
int sleep_immune_connect_bio_async(sleep_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    sleep_immune_bridge_heartbeat("sleep_immune_sleep_immune_connect", 0.0f);


    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_SLEEP,
        .module_name = SLEEP_IMMUNE_MODULE_NAME,
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("sleep_immune_bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

/**
 * @brief Disconnect from bio-async router
 */
int sleep_immune_disconnect_bio_async(sleep_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    sleep_immune_bridge_heartbeat("sleep_immune_sleep_immune_disconn", 0.0f);


    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_DEBUG("sleep_immune_bridge disconnected from bio-async router");
    return 0;
}

/**
 * @brief Check if bio-async is connected
 */
bool sleep_immune_is_bio_async_connected(const sleep_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    sleep_immune_bridge_heartbeat("sleep_immune_sleep_immune_is_bio_", 0.0f);


    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Query KG for module self-awareness information
 * WHY:  Enable introspective self-knowledge about sleep immune bridge
 * HOW:  Look up entity and relations in KG
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 otherwise
 */
int sleep_immune_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    sleep_immune_bridge_heartbeat("sleep_immune_sleep_immune_query_s", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Sleep_Immune_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                sleep_immune_bridge_heartbeat("sleep_immune_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Sleep immune bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Sleep_Immune_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Sleep_Immune_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void sleep_immune_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_sleep_immune_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int sleep_immune_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sleep_immune_bridge_training_begin: NULL argument");
        return -1;
    }
    /* TODO: Pass instance cast to (sleep_immune_bridge_t*)->health_agent once instance health agent is stored per-instance */
    sleep_immune_bridge_heartbeat_instance(NULL, "sleep_immune_bridge_training_begin", 0.0f);
    return 0;
}

int sleep_immune_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sleep_immune_bridge_training_end: NULL argument");
        return -1;
    }
    /* TODO: Pass instance cast to (sleep_immune_bridge_t*)->health_agent once instance health agent is stored per-instance */
    sleep_immune_bridge_heartbeat_instance(NULL, "sleep_immune_bridge_training_end", 1.0f);
    return 0;
}

int sleep_immune_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sleep_immune_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    /* TODO: Pass instance cast to (sleep_immune_bridge_t*)->health_agent once instance health agent is stored per-instance */
    sleep_immune_bridge_heartbeat_instance(NULL, "sleep_immune_bridge_training_step", progress);
    return 0;
}
