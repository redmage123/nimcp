/**
 * @file nimcp_executive_immune_bridge.c
 * @brief Executive Function-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional coupling between brain immune and executive functions
 * WHY:  Biological realism - cytokines impair executive control, stress affects immunity
 * HOW:  Monitor cytokine levels to modulate executive capacity, monitor load to trigger immune responses
 */

#include "cognitive/immune/nimcp_executive_immune_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(executive_immune_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_executive_immune_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_executive_immune_bridge_mesh_registry = NULL;

nimcp_error_t executive_immune_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_executive_immune_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "executive_immune_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_SECURITY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "executive_immune_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_executive_immune_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_executive_immune_bridge_mesh_registry = registry;
    return err;
}

void executive_immune_bridge_mesh_unregister(void) {
    if (g_executive_immune_bridge_mesh_registry && g_executive_immune_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_executive_immune_bridge_mesh_registry, g_executive_immune_bridge_mesh_id);
        g_executive_immune_bridge_mesh_id = 0;
        g_executive_immune_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from executive_immune_bridge module (instance-level) */
static inline void executive_immune_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_executive_immune_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_executive_immune_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_executive_immune_bridge_health_agent) {
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
 * @brief Compute cognitive fog level from cytokines
 *
 * WHAT: Calculate overall cognitive fog intensity
 * WHY:  Cognitive fog is distinct syndrome from pro-inflammatory cytokines
 * HOW:  Weighted sum of IL-1, IL-6, TNF-α concentrations
 */
static float compute_cognitive_fog(const brain_immune_system_t* immune) {
    if (!immune) return 0.0f;

    /* Query cytokine concentrations from immune system */
    float il1_level = 0.0f;
    float il6_level = 0.0f;
    float tnf_level = 0.0f;

    /* Count active cytokines as proxy for concentration */
    if (immune->cytokines && immune->cytokine_count > 0) {
        uint32_t il1_count = 0, il6_count = 0, tnf_count = 0;
        for (size_t i = 0; i < immune->cytokine_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && immune->cytokine_count > 256) {
                executive_immune_bridge_heartbeat("executive_im_loop",
                                 (float)(i + 1) / (float)immune->cytokine_count);
            }

            if (!immune->cytokines[i].delivered) continue;

            switch (immune->cytokines[i].type) {
                case BRAIN_CYTOKINE_IL1:
                    il1_count++;
                    il1_level += immune->cytokines[i].concentration;
                    break;
                case BRAIN_CYTOKINE_IL6:
                    il6_count++;
                    il6_level += immune->cytokines[i].concentration;
                    break;
                case BRAIN_CYTOKINE_TNF:
                    tnf_count++;
                    tnf_level += immune->cytokines[i].concentration;
                    break;
                default:
                    break;
            }
        }
        /* Normalize by max count */
        il1_level = clamp_f(il1_level / 10.0f, 0.0f, 1.0f);
        il6_level = clamp_f(il6_level / 10.0f, 0.0f, 1.0f);
        tnf_level = clamp_f(tnf_level / 10.0f, 0.0f, 1.0f);
    }

    /* Cognitive fog is weighted combination (IL-6 strongest effect) */
    float fog = (il1_level * 0.3f) + (il6_level * 0.5f) + (tnf_level * 0.2f);
    return clamp_f(fog, 0.0f, 1.0f);
}

/**
 * @brief Get inflammation duration
 *
 * WHAT: Calculate how long inflammation has persisted
 * WHY:  Chronic inflammation (>4 hours) has different executive effects
 * HOW:  Find oldest active inflammation site, compute duration
 */
static float get_inflammation_duration_sec(const brain_immune_system_t* immune) {
    if (!immune || !immune->inflammation_sites || immune->inflammation_count == 0) {
        return 0.0f;
    }

    /* Find oldest inflammation site */
    uint64_t oldest_start = UINT64_MAX;
    for (size_t i = 0; i < immune->inflammation_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && immune->inflammation_count > 256) {
            executive_immune_bridge_heartbeat("executive_im_loop",
                             (float)(i + 1) / (float)immune->inflammation_count);
        }

        if (immune->inflammation_sites[i].start_time < oldest_start) {
            oldest_start = immune->inflammation_sites[i].start_time;
        }
    }

    if (oldest_start == UINT64_MAX) return 0.0f;

    /* Calculate duration using monotonic clock */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t current_time = (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
    return (float)(current_time - oldest_start) / 1000.0f; /* ms to sec */
}

/**
 * @brief Get current inflammation level
 *
 * WHAT: Get highest inflammation level in system
 * WHY:  Max inflammation determines executive impact
 * HOW:  Query immune system for max inflammation site level
 */
static brain_inflammation_level_t get_max_inflammation_level(
    const brain_immune_system_t* immune
) {
    if (!immune || !immune->inflammation_sites || immune->inflammation_count == 0) {
        return INFLAMMATION_NONE;
    }

    brain_inflammation_level_t max_level = INFLAMMATION_NONE;
    for (size_t i = 0; i < immune->inflammation_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && immune->inflammation_count > 256) {
            executive_immune_bridge_heartbeat("executive_im_loop",
                             (float)(i + 1) / (float)immune->inflammation_count);
        }

        if (immune->inflammation_sites[i].level > max_level) {
            max_level = immune->inflammation_sites[i].level;
        }
    }

    return max_level;
}

/**
 * @brief Get inflammation level as normalized float
 *
 * WHAT: Convert inflammation enum to [0-1] scale
 * WHY:  Easier computation for modulation
 * HOW:  Map INFLAMMATION_NONE=0.0 to INFLAMMATION_STORM=1.0
 */
static float inflammation_level_to_float(brain_inflammation_level_t level) {
    return (float)level / (float)INFLAMMATION_STORM;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int executive_immune_default_config(executive_immune_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* All features enabled by default */
    /* Phase 8: Heartbeat at operation start */
    executive_immune_bridge_heartbeat("executive_im_executive_immune_def", 0.0f);


    config->enable_cytokine_executive_modulation = true;
    config->enable_inflammation_impairment = true;
    config->enable_executive_immune_trigger = true;
    config->enable_success_immune_boost = true;
    config->enable_overload_monitoring = true;

    /* Biologically-based default sensitivities */
    config->cytokine_sensitivity = 1.0f;
    config->inflammation_sensitivity = 1.0f;
    config->overload_trigger_sensitivity = 1.0f;

    /* Evidence-based thresholds */
    config->overload_trigger_threshold = EXECUTIVE_OVERLOAD_THRESHOLD;
    config->burnout_threshold = EXECUTIVE_BURNOUT_THRESHOLD;
    config->capacity_floor = INFLAMMATION_CAPACITY_FLOOR;

    return 0;
}

executive_immune_bridge_t* executive_immune_bridge_create(
    const executive_immune_config_t* config,
    brain_immune_system_t* immune_system,
    executive_controller_t* executive_controller
) {
    /* Guard: require immune and executive systems */
    if (!immune_system || !executive_controller) {
        LOG_MODULE_ERROR("executive_immune_bridge",
                  "Cannot create bridge without immune and executive systems");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_immune_bridge_create: required parameter is NULL (immune_system, executive_controller)");
        return NULL;
    }

    /* Allocate bridge */
    /* Phase 8: Heartbeat at operation start */
    executive_immune_bridge_heartbeat("executive_im_create", 0.0f);


    executive_immune_bridge_t* bridge = (executive_immune_bridge_t*)
        nimcp_malloc(sizeof(executive_immune_bridge_t));
    if (!bridge) {
        LOG_MODULE_ERROR("executive_immune_bridge", "Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "executive_immune_bridge_create: allocation failed");

        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(executive_immune_bridge_t));

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->executive_controller = executive_controller;

    /* Apply configuration */
    if (config) {
        bridge->enable_cytokine_executive_modulation = config->enable_cytokine_executive_modulation;
        bridge->enable_inflammation_impairment = config->enable_inflammation_impairment;
        bridge->enable_executive_immune_trigger = config->enable_executive_immune_trigger;
        bridge->enable_success_immune_boost = config->enable_success_immune_boost;
        bridge->enable_overload_monitoring = config->enable_overload_monitoring;
    } else {
        /* Use defaults */
        executive_immune_config_t default_cfg;
        executive_immune_default_config(&default_cfg);
        bridge->enable_cytokine_executive_modulation = default_cfg.enable_cytokine_executive_modulation;
        bridge->enable_inflammation_impairment = default_cfg.enable_inflammation_impairment;
        bridge->enable_executive_immune_trigger = default_cfg.enable_executive_immune_trigger;
        bridge->enable_success_immune_boost = default_cfg.enable_success_immune_boost;
        bridge->enable_overload_monitoring = default_cfg.enable_overload_monitoring;
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "executive_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);    return NULL;
    }

    LOG_MODULE_INFO("executive_immune_bridge", "Bridge created successfully");
    return bridge;
}

void executive_immune_bridge_destroy(executive_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Destroy mutex */
    /* Phase 8: Heartbeat at operation start */
    executive_immune_bridge_heartbeat("executive_im_destroy", 0.0f);


    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free bridge (don't destroy linked systems - we don't own them) */
    nimcp_free(bridge);
    bridge = NULL;
    LOG_MODULE_INFO("executive_immune_bridge", "Bridge destroyed");
}

/* ============================================================================
 * Immune → Executive Implementation
 * ============================================================================ */

int executive_immune_apply_cytokine_effects(executive_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_cytokine_executive_modulation) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_immune_apply_cytokine_effects: bridge->immune_system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    executive_immune_bridge_heartbeat("executive_im_executive_immune_app", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute cytokine effects */
    cytokine_executive_effects_t* effects = &bridge->cytokine_effects;

    /* Pro-inflammatory cytokines → capacity reduction */
    /* Query actual cytokine levels from immune system */
    float il1_level = 0.0f;
    float il6_level = 0.0f;
    float tnf_level = 0.0f;
    float ifn_gamma_level = 0.0f;
    float il10_level = 0.0f;

    if (bridge->immune_system->cytokines) {
        for (size_t i = 0; i < bridge->immune_system->cytokine_count; i++) {
            if (!bridge->immune_system->cytokines[i].delivered) continue;

            float conc = bridge->immune_system->cytokines[i].concentration;
            switch (bridge->immune_system->cytokines[i].type) {
                case BRAIN_CYTOKINE_IL1:
                    il1_level += conc;
                    break;
                case BRAIN_CYTOKINE_IL6:
                    il6_level += conc;
                    break;
                case BRAIN_CYTOKINE_TNF:
                    tnf_level += conc;
                    break;
                case BRAIN_CYTOKINE_IFN_GAMMA:
                    ifn_gamma_level += conc;
                    break;
                case BRAIN_CYTOKINE_IL10:
                    il10_level += conc;
                    break;
                default:
                    break;
            }
        }
        /* Normalize */
        il1_level = clamp_f(il1_level / 5.0f, 0.0f, 1.0f);
        il6_level = clamp_f(il6_level / 5.0f, 0.0f, 1.0f);
        tnf_level = clamp_f(tnf_level / 5.0f, 0.0f, 1.0f);
        ifn_gamma_level = clamp_f(ifn_gamma_level / 5.0f, 0.0f, 1.0f);
        il10_level = clamp_f(il10_level / 5.0f, 0.0f, 1.0f);
    }

    effects->il1_capacity_reduction = il1_level * CYTOKINE_IL1_CAPACITY_IMPACT;
    effects->il6_capacity_reduction = il6_level * CYTOKINE_IL6_CAPACITY_IMPACT;
    effects->tnf_capacity_reduction = tnf_level * CYTOKINE_TNF_CAPACITY_IMPACT;
    effects->ifn_gamma_capacity_reduction = ifn_gamma_level * CYTOKINE_IFN_GAMMA_CAPACITY_IMPACT;

    /* Anti-inflammatory cytokines → recovery */
    effects->il10_capacity_recovery = il10_level * CYTOKINE_IL10_CAPACITY_IMPACT;

    /* Aggregate effects */
    effects->total_capacity_impact =
        effects->il1_capacity_reduction +
        effects->il6_capacity_reduction +
        effects->tnf_capacity_reduction +
        effects->ifn_gamma_capacity_reduction +
        effects->il10_capacity_recovery;

    /* Cognitive fog */
    effects->cognitive_fog_level = compute_cognitive_fog(bridge->immune_system);

    /* Processing slowdown (IL-6 strongest effect) */
    effects->processing_slowdown = clamp_f(il6_level * 0.8f, 0.0f, 1.0f);

    /* Flexibility impairment (set-shifting) */
    float proinflam_total = fabs(effects->il1_capacity_reduction) +
                           fabs(effects->il6_capacity_reduction) +
                           fabs(effects->tnf_capacity_reduction);
    effects->flexibility_impairment = clamp_f(proinflam_total * 0.7f, 0.0f, 1.0f);

    bridge->cytokine_modulations++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int executive_immune_apply_inflammation_effects(executive_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_inflammation_impairment) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_immune_apply_inflammation_effects: bridge->immune_system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    executive_immune_bridge_heartbeat("executive_im_executive_immune_app", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    inflammation_executive_state_t* state = &bridge->inflammation_state;

    /* Get inflammation state */
    state->current_level = get_max_inflammation_level(bridge->immune_system);
    state->inflammation_duration_sec = get_inflammation_duration_sec(bridge->immune_system);
    state->is_chronic = (state->inflammation_duration_sec >= CHRONIC_OVERLOAD_THRESHOLD);

    /* Inflammation intensity [0-1] */
    float inflammation_intensity = inflammation_level_to_float(state->current_level);

    /* Capacity reduction scales with inflammation */
    state->capacity_reduction = clamp_f(inflammation_intensity * 0.9f, 0.0f, 0.9f);

    /* Switch cost increases (perseveration) */
    state->switch_cost_increase = 1.0f + (inflammation_intensity * INFLAMMATION_SWITCH_COST_MULT);

    /* Inhibition impairment */
    state->inhibition_impairment = inflammation_intensity * INFLAMMATION_INHIBITION_PENALTY;

    /* Planning depth reduction */
    state->planning_depth_reduction = inflammation_intensity * INFLAMMATION_PLANNING_REDUCTION;

    /* Working memory impairment (7±2 → 3-4 items) */
    state->working_memory_impairment = clamp_f(inflammation_intensity * 0.6f, 0.0f, 0.6f);

    /* Perseveration increase */
    state->perseveration_increase = clamp_f(inflammation_intensity * 0.8f, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float executive_immune_compute_capacity_reduction(const executive_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;

    /* Combine cytokine-induced and inflammation-induced reduction */
    /* Phase 8: Heartbeat at operation start */
    executive_immune_bridge_heartbeat("executive_im_executive_immune_com", 0.0f);


    float cytokine_reduction = -bridge->cytokine_effects.total_capacity_impact;
    cytokine_reduction = clamp_f(cytokine_reduction, 0.0f, 1.0f);

    float inflammation_reduction = bridge->inflammation_state.capacity_reduction;

    /* Take maximum (not additive - they're correlated) */
    float total_reduction = fmaxf(cytokine_reduction, inflammation_reduction);

    /* Apply floor */
    total_reduction = clamp_f(total_reduction, 0.0f, 1.0f - INFLAMMATION_CAPACITY_FLOOR);

    return total_reduction;
}

float executive_immune_compute_switch_cost_increase(const executive_immune_bridge_t* bridge) {
    if (!bridge) return 1.0f; /* No increase */

    /* Use inflammation state switch cost multiplier */
    /* Phase 8: Heartbeat at operation start */
    executive_immune_bridge_heartbeat("executive_im_executive_immune_com", 0.0f);


    return bridge->inflammation_state.switch_cost_increase;
}

float executive_immune_compute_inhibition_impairment(const executive_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;

    /* Use inflammation state inhibition impairment */
    /* Phase 8: Heartbeat at operation start */
    executive_immune_bridge_heartbeat("executive_im_executive_immune_com", 0.0f);


    return bridge->inflammation_state.inhibition_impairment;
}

float executive_immune_compute_planning_reduction(const executive_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;

    /* Use inflammation state planning depth reduction */
    /* Phase 8: Heartbeat at operation start */
    executive_immune_bridge_heartbeat("executive_im_executive_immune_com", 0.0f);


    return bridge->inflammation_state.planning_depth_reduction;
}

/* ============================================================================
 * Executive → Immune Implementation
 * ============================================================================ */

int executive_immune_trigger_from_overload(executive_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_executive_immune_trigger) return 0;
    if (!bridge->immune_system || !bridge->executive_controller) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_immune_trigger_from_overload: required parameter is NULL (bridge->immune_system, bridge->executive_controller)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    executive_immune_bridge_heartbeat("executive_im_executive_immune_tri", 0.0f);


    /* Get cognitive load before acquiring lock (external call) */
    float cognitive_load = executive_get_cognitive_load(bridge->executive_controller);

    nimcp_mutex_lock(bridge->base.mutex);

    executive_immune_trigger_t* trigger = &bridge->executive_trigger;
    trigger->cognitive_load = cognitive_load;

    /* Check if overload threshold exceeded */
    bool do_cytokine_release = (trigger->cognitive_load >= EXECUTIVE_OVERLOAD_THRESHOLD);
    if (do_cytokine_release) {
        trigger->cortisol_triggered = true;
        trigger->immune_suppression = clamp_f(trigger->cognitive_load * 0.5f, 0.0f, 1.0f);

        bridge->executive_triggered_responses++;
        bridge->overload_events++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Release cytokine outside lock to avoid deadlock */
    if (do_cytokine_release) {
        uint32_t cytokine_id = 0;
        brain_immune_release_cytokine(
            bridge->immune_system,
            BRAIN_CYTOKINE_IL6,
            0, /* source cell (executive system) */
            cognitive_load, /* concentration based on load */
            0, /* broadcast */
            &cytokine_id
        );
    }
    return 0;
}

int executive_immune_amplify_from_frustration(executive_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_executive_immune_trigger) return 0;
    if (!bridge->immune_system || !bridge->executive_controller) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_immune_amplify_from_frustration: required parameter is NULL (bridge->immune_system, bridge->executive_controller)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    executive_immune_bridge_heartbeat("executive_im_executive_immune_amp", 0.0f);


    /* Get executive stats before acquiring lock (external call) */
    executive_stats_t stats;
    if (!executive_get_stats(bridge->executive_controller, &stats)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "executive_immune_amplify_from_frustration: executive_get_stats is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    executive_immune_trigger_t* trigger = &bridge->executive_trigger;

    /* Calculate recent failures (delta from previous) */
    uint32_t recent_failures = stats.failed_tasks - bridge->prev_failed_tasks;
    bridge->prev_failed_tasks = stats.failed_tasks;

    trigger->failed_tasks = recent_failures;

    /* Frustration amplifies inflammation - collect site IDs under lock */
    size_t inflam_count = bridge->immune_system->inflammation_count;
    uint32_t site_ids[64]; /* Fixed buffer for site IDs */
    size_t ids_to_escalate = 0;
    if (recent_failures > 0 && inflam_count > 0) {
        size_t limit = inflam_count < 64 ? inflam_count : 64;
        for (size_t i = 0; i < limit; i++) {
            site_ids[ids_to_escalate++] = bridge->immune_system->inflammation_sites[i].id;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Escalate inflammation outside lock to avoid deadlock */
    for (size_t i = 0; i < ids_to_escalate; i++) {
        brain_immune_escalate_inflammation(bridge->immune_system, site_ids[i]);
    }
    return 0;
}

int executive_immune_detect_burnout(executive_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_overload_monitoring) return 0;
    if (!bridge->immune_system || !bridge->executive_controller) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_immune_detect_burnout: required parameter is NULL (bridge->immune_system, bridge->executive_controller)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    executive_immune_bridge_heartbeat("executive_im_executive_immune_det", 0.0f);


    /* Get cognitive load before acquiring lock (external call) */
    float burnout_load = executive_get_cognitive_load(bridge->executive_controller);

    nimcp_mutex_lock(bridge->base.mutex);

    executive_immune_trigger_t* trigger = &bridge->executive_trigger;
    trigger->cognitive_load = burnout_load;

    /* Track overload duration */
    bool do_present_antigen = false;
    float burnout_level = 0.0f;
    if (trigger->cognitive_load >= EXECUTIVE_BURNOUT_THRESHOLD) {
        /* Would increment overload duration based on delta_ms in update() */
        trigger->chronic_overload = (trigger->overload_duration_sec >= CHRONIC_OVERLOAD_THRESHOLD);

        if (trigger->chronic_overload) {
            /* Calculate burnout level */
            float duration_factor = clamp_f(
                trigger->overload_duration_sec / (CHRONIC_OVERLOAD_THRESHOLD * 2.0f),
                0.0f, 1.0f
            );
            trigger->burnout_level = clamp_f(duration_factor * 0.9f, 0.0f, 1.0f);
            trigger->immune_dysregulation = trigger->burnout_level;
            burnout_level = trigger->burnout_level;

            /* Trigger systemic inflammation if severe burnout */
            if (trigger->burnout_level > 0.7f) {
                do_present_antigen = true;
                bridge->burnout_events++;
            }
        }
    } else {
        /* Reset overload duration if load is low */
        trigger->overload_duration_sec = 0.0f;
        trigger->chronic_overload = false;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Present antigen and initiate inflammation outside lock to avoid deadlock */
    if (do_present_antigen) {
        uint32_t site_id = 0;
        uint32_t antigen_id = 0;
        uint8_t epitope[64] = "burnout_stress";

        brain_immune_present_antigen(bridge->immune_system, ANTIGEN_SOURCE_MANUAL,
            epitope, strlen((const char*)epitope), 8, 0, &antigen_id);
        brain_immune_initiate_inflammation(bridge->immune_system, 0, antigen_id, &site_id);
    }
    return 0;
}

int executive_immune_boost_from_success(executive_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_success_immune_boost) return 0;
    if (!bridge->immune_system || !bridge->executive_controller) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_immune_boost_from_success: required parameter is NULL (bridge->immune_system, bridge->executive_controller)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    executive_immune_bridge_heartbeat("executive_im_executive_immune_boo", 0.0f);


    /* Get executive stats before acquiring lock (external call) */
    executive_stats_t stats;
    if (!executive_get_stats(bridge->executive_controller, &stats)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "executive_immune_boost_from_success: executive_get_stats is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    executive_success_immune_boost_t* boost = &bridge->success_boost;

    boost->completed_tasks = stats.completed_tasks;

    /* Calculate success rate */
    uint32_t total_tasks = stats.completed_tasks + stats.failed_tasks;
    if (total_tasks > 0) {
        boost->success_rate = (float)stats.completed_tasks / (float)total_tasks;
    } else {
        boost->success_rate = 0.0f;
    }

    /* High success rate → immune boost */
    bool do_il10_boost = (boost->success_rate > 0.7f);
    float il10_release_boost = 0.0f;
    if (do_il10_boost) {
        boost->immune_enhancement = clamp_f(boost->success_rate * 0.5f, 0.0f, 1.0f);
        boost->il10_release_boost = boost->success_rate * 0.3f;
        il10_release_boost = boost->il10_release_boost;

        /* Reduce inflammation */
        boost->inflammation_reduction = boost->success_rate * 0.4f;
        boost->stress_recovery = boost->success_rate * 0.6f;

        bridge->success_boosts++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Release cytokine outside lock to avoid deadlock */
    if (do_il10_boost) {
        uint32_t cytokine_id = 0;
        brain_immune_release_cytokine(
            bridge->immune_system,
            BRAIN_CYTOKINE_IL10,
            0, /* source cell */
            il10_release_boost,
            0, /* broadcast */
            &cytokine_id
        );
    }
    return 0;
}

/* ============================================================================
 * Bidirectional Update Implementation
 * ============================================================================ */

int executive_immune_bridge_update(
    executive_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    executive_immune_bridge_heartbeat("executive_im_update", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Update overload duration tracking */
    float delta_sec = (float)delta_ms / 1000.0f;
    if (bridge->executive_trigger.cognitive_load >= EXECUTIVE_BURNOUT_THRESHOLD) {
        bridge->executive_trigger.overload_duration_sec += delta_sec;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Immune → Executive: Apply cytokine and inflammation effects */
    executive_immune_apply_cytokine_effects(bridge);
    executive_immune_apply_inflammation_effects(bridge);

    /* Executive → Immune: Check for overload, frustration, success */
    executive_immune_trigger_from_overload(bridge);
    executive_immune_amplify_from_frustration(bridge);
    executive_immune_detect_burnout(bridge);
    executive_immune_boost_from_success(bridge);

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->total_updates++;
    struct timespec upd_ts;
    clock_gettime(CLOCK_MONOTONIC, &upd_ts);
    uint64_t now_ms = (uint64_t)upd_ts.tv_sec * 1000ULL + (uint64_t)upd_ts.tv_nsec / 1000000ULL;
    bridge->last_update_time = now_ms;
    bridge->base.last_update_time_ms = now_ms;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int executive_immune_get_cytokine_effects(
    const executive_immune_bridge_t* bridge,
    cytokine_executive_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_immune_get_cytokine_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    executive_immune_bridge_heartbeat("executive_im_executive_immune_get", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->cytokine_effects;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int executive_immune_get_inflammation_state(
    const executive_immune_bridge_t* bridge,
    inflammation_executive_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_immune_get_inflammation_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    executive_immune_bridge_heartbeat("executive_im_executive_immune_get", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->inflammation_state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool executive_immune_is_cognitive_fog(const executive_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    executive_immune_bridge_heartbeat("executive_im_executive_immune_is_", 0.0f);


    return bridge->cytokine_effects.cognitive_fog_level > 0.5f;
}

float executive_immune_get_cognitive_fog_severity(const executive_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    executive_immune_bridge_heartbeat("executive_im_executive_immune_get", 0.0f);


    return bridge->cytokine_effects.cognitive_fog_level;
}

bool executive_immune_is_burnout(const executive_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    executive_immune_bridge_heartbeat("executive_im_executive_immune_is_", 0.0f);


    return bridge->executive_trigger.burnout_level > 0.5f;
}

float executive_immune_get_burnout_severity(const executive_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    executive_immune_bridge_heartbeat("executive_im_executive_immune_get", 0.0f);


    return bridge->executive_trigger.burnout_level;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

#define EXECUTIVE_IMMUNE_MODULE_NAME "executive_immune_bridge"

/**
 * @brief Connect bridge to bio-async router
 */
int executive_immune_connect_bio_async(executive_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    executive_immune_bridge_heartbeat("executive_im_executive_immune_con", 0.0f);


    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_EXECUTIVE,
        .module_name = EXECUTIVE_IMMUNE_MODULE_NAME,
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("executive_immune_bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

/**
 * @brief Disconnect from bio-async router
 */
int executive_immune_disconnect_bio_async(executive_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    executive_immune_bridge_heartbeat("executive_im_executive_immune_dis", 0.0f);


    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_DEBUG("executive_immune_bridge disconnected from bio-async router");
    return 0;
}

/**
 * @brief Check if bio-async is connected
 */
bool executive_immune_is_bio_async_connected(const executive_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    executive_immune_bridge_heartbeat("executive_im_executive_immune_is_", 0.0f);


    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Query KG for module self-awareness information
 * WHY:  Enable introspective self-knowledge about executive immune bridge
 * HOW:  Look up entity and relations in KG
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 otherwise
 */
int executive_immune_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    executive_immune_bridge_heartbeat("executive_im_executive_immune_que", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Executive_Immune_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                executive_immune_bridge_heartbeat("executive_im_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Executive immune bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Executive_Immune_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Executive_Immune_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void executive_immune_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_executive_immune_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int executive_immune_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "executive_immune_bridge_training_begin: NULL argument");
        return -1;
    }
    /* TODO: Pass instance cast to (executive_immune_bridge_t*)->health_agent once instance health agent is stored per-instance */
    executive_immune_bridge_heartbeat_instance(NULL, "executive_immune_bridge_training_begin", 0.0f);
    return 0;
}

int executive_immune_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "executive_immune_bridge_training_end: NULL argument");
        return -1;
    }
    /* TODO: Pass instance cast to (executive_immune_bridge_t*)->health_agent once instance health agent is stored per-instance */
    executive_immune_bridge_heartbeat_instance(NULL, "executive_immune_bridge_training_end", 1.0f);
    return 0;
}

int executive_immune_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "executive_immune_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    /* TODO: Pass instance cast to (executive_immune_bridge_t*)->health_agent once instance health agent is stored per-instance */
    executive_immune_bridge_heartbeat_instance(NULL, "executive_immune_bridge_training_step", progress);
    return 0;
}
