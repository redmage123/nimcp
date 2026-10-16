/**
 * @file nimcp_pr_immune_bridge.c
 * @brief Prime Resonant Immune Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Implementation of bidirectional integration between Prime Resonant
 *       memory system and brain immune system
 * WHY:  Enable biologically realistic immune-memory coupling where cytokines
 *       affect consolidation and immune system tags corrupted memories
 * HOW:  Implements cytokine modulation, cleanup tagging, inflammation
 *       processing, and sleep-immune-memory coordination
 */

#include "cognitive/memory/core/nimcp_pr_immune_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"

#include <string.h>
#include <math.h>
#include <stdio.h>
#include "glial/myelin_sheath/nimcp_myelin_math.h"

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(pr_immune_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_pr_immune_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_pr_immune_bridge_mesh_registry = NULL;

nimcp_error_t pr_immune_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_pr_immune_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "pr_immune_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_MEMORY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "pr_immune_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_pr_immune_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_pr_immune_bridge_mesh_registry = registry;
    return err;
}

void pr_immune_bridge_mesh_unregister(void) {
    if (g_pr_immune_bridge_mesh_registry && g_pr_immune_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_pr_immune_bridge_mesh_registry, g_pr_immune_bridge_mesh_id);
        g_pr_immune_bridge_mesh_id = 0;
        g_pr_immune_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from pr_immune_bridge module (instance-level) */
static inline void pr_immune_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_pr_immune_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_pr_immune_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_pr_immune_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "PR_IMMUNE_BRIDGE"

/* Security subsystem setters (Phase 1: Audit Gap Remediation) */
//=============================================================================
// Internal Structure Definition
//=============================================================================

/**
 * @brief Internal bridge structure
 *
 * WHAT: Complete state for immune-memory bridge
 * WHY:  Encapsulate all bridge data for thread safety
 */
struct pr_immune_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    pr_immune_bridge_config_t config;

    /* Connected systems */
    brain_immune_system_t* immune_system;
    sleep_system_t sleep_system;

    /* Cleanup queue (circular buffer) */
    pr_cleanup_tag_t* cleanup_queue;
    uint32_t cleanup_capacity;
    uint32_t cleanup_count;
    uint32_t cleanup_read_idx;
    uint32_t cleanup_write_idx;

    /* Current cytokine effects */
    pr_cytokine_memory_effects_t cytokine_effects;

    /* Current SIM coordination */
    pr_sim_coordination_t sim_coordination;

    /* Inflammation tracking */
    float inflammation_level;
    uint64_t inflammation_start_ms;
    bool chronic_inflammation;

    /* Statistics */
    pr_immune_bridge_stats_t stats;

    /* Bio-async */
    bool bio_async_connected;

    /* Update tracking */
    uint64_t last_update_ms;

    /* Health agent (instance-level) - Phase 8 */
    nimcp_health_agent_t* health_agent;
};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(pr_immune_bridge, struct pr_immune_bridge_struct)

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Convert sleep state to SIM phase
 */
static pr_sim_phase_t sleep_state_to_sim_phase(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:
            return PR_SIM_PHASE_AWAKE;
        case SLEEP_STATE_DROWSY:
        case SLEEP_STATE_LIGHT_NREM:
            return PR_SIM_PHASE_LIGHT_SLEEP;
        case SLEEP_STATE_DEEP_NREM:
            return PR_SIM_PHASE_DEEP_SLEEP;
        case SLEEP_STATE_REM:
            return PR_SIM_PHASE_REM_SLEEP;
        default:
            return PR_SIM_PHASE_TRANSITION;
    }
}

/**
 * @brief Compute inflammation impact from level
 */
static pr_inflammation_impact_t level_to_impact(float level) {
    if (level < PR_IMMUNE_INFLAMMATION_MILD_THRESHOLD) {
        return PR_INFLAMMATION_NONE;
    } else if (level < PR_IMMUNE_INFLAMMATION_HIGH_THRESHOLD) {
        return PR_INFLAMMATION_MILD;
    } else if (level < PR_IMMUNE_INFLAMMATION_SEVERE_THRESHOLD) {
        return PR_INFLAMMATION_MODERATE;
    } else if (level < 1.0f) {
        return PR_INFLAMMATION_SEVERE;
    }
    return PR_INFLAMMATION_CRITICAL;
}

/**
 * @brief Find cleanup tag by node ID (internal, assumes lock held)
 */
static pr_cleanup_tag_t* find_cleanup_tag_unlocked(
    pr_immune_bridge_t bridge,
    uint64_t node_id)
{
    if (!bridge->cleanup_queue || bridge->cleanup_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_cleanup_tag_unlocked: bridge->cleanup_queue is NULL");
        return NULL;
    }

    for (uint32_t i = 0; i < bridge->cleanup_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->cleanup_count > 256) {
            pr_immune_bridge_heartbeat("pr_immune_br_loop",
                             (float)(i + 1) / (float)bridge->cleanup_count);
        }

        uint32_t idx = (bridge->cleanup_read_idx + i) % bridge->cleanup_capacity;
        if (bridge->cleanup_queue[idx].node_id == node_id) {
            return &bridge->cleanup_queue[idx];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_cleanup_tag_unlocked: validation failed");
    return NULL;
}

/**
 * @brief Add tag to cleanup queue (internal, assumes lock held)
 */
static int add_cleanup_tag_unlocked(
    pr_immune_bridge_t bridge,
    const pr_cleanup_tag_t* tag)
{
    if (bridge->cleanup_count >= bridge->cleanup_capacity) {
        /* Queue full */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "add_cleanup_tag_unlocked: capacity exceeded");
        return -1;
    }

    bridge->cleanup_queue[bridge->cleanup_write_idx] = *tag;
    bridge->cleanup_write_idx = (bridge->cleanup_write_idx + 1) % bridge->cleanup_capacity;
    bridge->cleanup_count++;
    bridge->stats.nodes_tagged++;

    return 0;
}

/**
 * @brief Update cytokine effects from immune system
 */
static void update_cytokine_effects(pr_immune_bridge_t bridge) {
    if (!bridge->immune_system) {
        /* No immune system connected - reset to neutral */
        memset(&bridge->cytokine_effects, 0, sizeof(pr_cytokine_memory_effects_t));
        bridge->cytokine_effects.decay_rate_multiplier = 1.0f;
        return;
    }

    /* Get cytokine levels from immune system */
    float il1 = brain_immune_get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IL1);
    float tnf = brain_immune_get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_TNF);
    float il6 = brain_immune_get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IL6);
    float il10 = brain_immune_get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IL10);
    float ifn_gamma = brain_immune_get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IFN_GAMMA);

    /* Apply sensitivities */
    float sensitivity = bridge->config.cytokine_sensitivity;
    il1 *= bridge->config.il1_sensitivity * sensitivity;
    tnf *= bridge->config.tnf_sensitivity * sensitivity;
    il6 *= bridge->config.il6_sensitivity * sensitivity;
    il10 *= bridge->config.il10_sensitivity * sensitivity;

    /* Compute individual effects */
    bridge->cytokine_effects.il1_consolidation_reduction =
        il1 * PR_IMMUNE_IL1_CONSOLIDATION_REDUCTION;
    bridge->cytokine_effects.tnf_consolidation_reduction =
        tnf * PR_IMMUNE_TNF_CONSOLIDATION_REDUCTION;
    bridge->cytokine_effects.il6_accessibility_reduction =
        il6 * PR_IMMUNE_IL6_ACCESSIBILITY_REDUCTION;
    bridge->cytokine_effects.il10_consolidation_boost =
        il10 * PR_IMMUNE_IL10_CONSOLIDATION_BOOST;
    bridge->cytokine_effects.ifn_gamma_surveillance_level = ifn_gamma;

    /* Compute net effects */
    float pro_inflammatory = bridge->cytokine_effects.il1_consolidation_reduction +
                            bridge->cytokine_effects.tnf_consolidation_reduction;
    float anti_inflammatory = bridge->cytokine_effects.il10_consolidation_boost;

    bridge->cytokine_effects.net_consolidation_modifier =
        nimcp_myelin_clamp(anti_inflammatory - pro_inflammatory, -1.0f, 1.0f);

    bridge->cytokine_effects.net_accessibility_modifier =
        nimcp_myelin_clamp(-bridge->cytokine_effects.il6_accessibility_reduction, -1.0f, 0.0f);

    /* Decay rate multiplier: higher with more pro-inflammatory cytokines */
    float decay_mult = 1.0f + (pro_inflammatory * (PR_IMMUNE_INFLAMMATION_DECAY_MULT - 1.0f));
    bridge->cytokine_effects.decay_rate_multiplier = nimcp_myelin_clamp(decay_mult, 1.0f, 3.0f);

    /* Update inflammation impact */
    bridge->cytokine_effects.impact_level = level_to_impact(bridge->inflammation_level);
    bridge->cytokine_effects.is_chronic = bridge->chronic_inflammation;
}

/**
 * @brief Update SIM coordination state
 */
static void update_sim_coordination(pr_immune_bridge_t bridge) {
    if (!bridge->sleep_system) {
        /* No sleep system - default to awake */
        bridge->sim_coordination.current_phase = PR_SIM_PHASE_AWAKE;
        bridge->sim_coordination.consolidation_multiplier = PR_IMMUNE_SLEEP_AWAKE_MULT;
        bridge->sim_coordination.cleanup_efficiency = 0.1f;
        bridge->sim_coordination.immune_consolidation_active = false;
        return;
    }

    /* Get current sleep state */
    sleep_state_t sleep_state = sleep_get_current_state(bridge->sleep_system);
    pr_sim_phase_t new_phase = sleep_state_to_sim_phase(sleep_state);

    /* Check for phase transition */
    if (new_phase != bridge->sim_coordination.current_phase) {
        bridge->sim_coordination.current_phase = new_phase;
        bridge->sim_coordination.phase_duration_ms = 0;
    }

    /* Set phase-specific parameters */
    switch (new_phase) {
        case PR_SIM_PHASE_AWAKE:
            bridge->sim_coordination.consolidation_multiplier = PR_IMMUNE_SLEEP_AWAKE_MULT;
            bridge->sim_coordination.cleanup_efficiency = 0.1f;
            bridge->sim_coordination.immune_consolidation_active = false;
            break;

        case PR_SIM_PHASE_LIGHT_SLEEP:
            bridge->sim_coordination.consolidation_multiplier = PR_IMMUNE_SLEEP_LIGHT_MULT;
            bridge->sim_coordination.cleanup_efficiency = 0.2f;
            bridge->sim_coordination.immune_consolidation_active = true;
            break;

        case PR_SIM_PHASE_DEEP_SLEEP:
            bridge->sim_coordination.consolidation_multiplier = PR_IMMUNE_SLEEP_DEEP_MULT;
            bridge->sim_coordination.cleanup_efficiency = 0.15f;
            bridge->sim_coordination.immune_consolidation_active = true;
            break;

        case PR_SIM_PHASE_REM_SLEEP:
            bridge->sim_coordination.consolidation_multiplier = PR_IMMUNE_SLEEP_REM_MULT;
            bridge->sim_coordination.cleanup_efficiency = bridge->config.rem_cleanup_efficiency;
            bridge->sim_coordination.immune_consolidation_active = false;
            break;

        default:
            bridge->sim_coordination.consolidation_multiplier = 1.0f;
            bridge->sim_coordination.cleanup_efficiency = 0.05f;
            bridge->sim_coordination.immune_consolidation_active = false;
            break;
    }

    /* Apply inflammation reduction to consolidation */
    if (bridge->cytokine_effects.impact_level >= PR_INFLAMMATION_MODERATE) {
        float reduction = 1.0f - (0.2f * (bridge->cytokine_effects.impact_level - 1));
        bridge->sim_coordination.consolidation_multiplier *= nimcp_myelin_clamp(reduction, 0.3f, 1.0f);
    }

    /* Compute immune sync level */
    if (bridge->immune_system && bridge->sim_coordination.immune_consolidation_active) {
        /* Higher sync when cytokines are balanced */
        float balance = 1.0f - fabsf(bridge->cytokine_effects.net_consolidation_modifier);
        bridge->sim_coordination.immune_sync_level = balance * 0.8f;
    } else {
        bridge->sim_coordination.immune_sync_level = 0.1f;
    }
}

//=============================================================================
// Configuration Functions
//=============================================================================

pr_immune_bridge_config_t pr_immune_bridge_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_config_default", 0.0f);


    pr_immune_bridge_config_t config;
    memset(&config, 0, sizeof(config));

    /* Feature enables */
    config.enable_cytokine_modulation = true;
    config.enable_cleanup_tagging = true;
    config.enable_inflammation_effects = true;
    config.enable_sleep_coordination = true;
    config.enable_corruption_detection = true;

    /* Cytokine sensitivity */
    config.cytokine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config.il1_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config.tnf_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config.il6_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config.il10_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    /* Cleanup thresholds */
    config.cleanup_strength_threshold = PR_IMMUNE_CLEANUP_STRENGTH_THRESHOLD;
    config.corruption_detection_threshold = 0.01f;
    config.max_cleanup_per_cycle = PR_IMMUNE_MAX_TAGS_PER_CYCLE;

    /* Sleep coordination */
    config.deep_sleep_consolidation_boost = PR_IMMUNE_DEEP_SLEEP_CONSOLIDATION;
    config.rem_cleanup_efficiency = PR_IMMUNE_REM_CLEANUP_EFFICIENCY;

    /* Event logging */
    config.enable_event_logging = false;
    config.max_events = 1000;

    /* Bio-async */
    config.enable_bio_async = false;

    return config;
}

bool pr_immune_bridge_config_validate(const pr_immune_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_immune_bridge_config_validate: config is NULL");
        return false;
    }

    /* Check sensitivity ranges */
    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_config_validate", 0.0f);


    if (config->cytokine_sensitivity < 0.0f || config->cytokine_sensitivity > 10.0f) {
        return false;
    }

    /* Check threshold ranges */
    if (config->cleanup_strength_threshold < 0.0f ||
        config->cleanup_strength_threshold > 1.0f) {
        return false;
    }

    /* Check sleep coordination ranges */
    if (config->deep_sleep_consolidation_boost < 0.0f ||
        config->rem_cleanup_efficiency < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_immune_bridge_config_validate: operation failed");
        return false;
    }

    return true;
}

//=============================================================================
// Bridge Lifecycle
//=============================================================================

pr_immune_bridge_t pr_immune_bridge_create(
    const pr_immune_bridge_config_t* config,
    brain_immune_system_t* immune_system,
    sleep_system_t sleep_system)
{
    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_create", 0.0f);


    pr_immune_bridge_config_t actual_config;
    if (config) {
        if (!pr_immune_bridge_config_validate(config)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_immune_bridge_create: pr_immune_bridge_config_validate is NULL");
            return NULL;
        }
        actual_config = *config;
    } else {
        actual_config = pr_immune_bridge_config_default();
    }

    /* Allocate bridge structure */
    pr_immune_bridge_t bridge = nimcp_malloc(sizeof(struct pr_immune_bridge_struct));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    memset(bridge, 0, sizeof(struct pr_immune_bridge_struct));
    bridge->config = actual_config;
    bridge->immune_system = immune_system;
    bridge->sleep_system = sleep_system;

    /* Allocate cleanup queue */
    bridge->cleanup_capacity = PR_IMMUNE_MAX_CLEANUP_QUEUE;
    bridge->cleanup_queue = nimcp_calloc(bridge->cleanup_capacity, sizeof(pr_cleanup_tag_t));
    if (!bridge->cleanup_queue) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_immune_bridge_create: bridge->cleanup_queue is NULL");
        return NULL;
    }
    /* calloc zero-initializes */

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "pr_immune") != 0) {
        nimcp_free(bridge->cleanup_queue);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "pr_immune_bridge_create: validation failed");
        return NULL;
    }

    /* Initialize state */
    bridge->cytokine_effects.decay_rate_multiplier = 1.0f;
    bridge->sim_coordination.current_phase = PR_SIM_PHASE_AWAKE;
    bridge->sim_coordination.consolidation_multiplier = 1.0f;
    bridge->last_update_ms = nimcp_time_get_ms();

    /* Initial update */
    update_cytokine_effects(bridge);
    update_sim_coordination(bridge);

    return bridge;
}

void pr_immune_bridge_destroy(pr_immune_bridge_t bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "pr_immune");

    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_destroy", 0.0f);


    if (bridge->cleanup_queue) {
        nimcp_free(bridge->cleanup_queue);
    }

    /* Cleanup base bridge infrastructure */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
}

int pr_immune_bridge_reset(pr_immune_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_immune_bridge_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset cleanup queue */
    bridge->cleanup_count = 0;
    bridge->cleanup_read_idx = 0;
    bridge->cleanup_write_idx = 0;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(pr_immune_bridge_stats_t));

    /* Reset SIM coordination counters */
    bridge->sim_coordination.nodes_consolidated = 0;
    bridge->sim_coordination.nodes_cleaned = 0;
    bridge->sim_coordination.promotions_triggered = 0;
    bridge->sim_coordination.entanglements_pruned = 0;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int pr_immune_bridge_connect_immune(
    pr_immune_bridge_t bridge,
    brain_immune_system_t* immune_system)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_immune_bridge_connect_immune: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_connect_immune", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->immune_system = immune_system;
    update_cytokine_effects(bridge);
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pr_immune_bridge_connect_sleep(
    pr_immune_bridge_t bridge,
    sleep_system_t sleep_system)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_immune_bridge_connect_sleep: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_connect_sleep", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->sleep_system = sleep_system;
    update_sim_coordination(bridge);
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Consolidation Modulation API
//=============================================================================

nimcp_quaternion_t pr_immune_bridge_modulate_consolidation(
    pr_immune_bridge_t bridge,
    const pr_memory_node_t* node)
{
    /* Return identity quaternion on error */
    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_modulate_consolidati", 0.0f);


    nimcp_quaternion_t result = quat_identity();
    if (!bridge || !node) return result;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Get base quaternion from node */
    result = node->state;

    if (bridge->config.enable_cytokine_modulation) {
        /* Apply consolidation modifier to w component */
        float w_modifier = bridge->cytokine_effects.net_consolidation_modifier;
        result.w = nimcp_myelin_clamp(result.w + (w_modifier * 0.1f),
                          PR_IMMUNE_QUAT_MIN, PR_IMMUNE_QUAT_MAX);

        /* Apply accessibility modifier to z component */
        float z_modifier = bridge->cytokine_effects.net_accessibility_modifier;
        result.z = nimcp_myelin_clamp(result.z + (z_modifier * 0.1f),
                          PR_IMMUNE_QUAT_MIN, PR_IMMUNE_QUAT_MAX);

        bridge->stats.cytokine_modulations++;
    }

    if (bridge->config.enable_inflammation_effects) {
        /* Apply inflammation reduction */
        pr_inflammation_impact_t impact = bridge->cytokine_effects.impact_level;
        if (impact >= PR_INFLAMMATION_MILD) {
            float reduction = 0.05f * impact;
            result.w = nimcp_myelin_clamp(result.w - reduction, PR_IMMUNE_QUAT_MIN, PR_IMMUNE_QUAT_MAX);
            bridge->stats.inflammation_effects++;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

int pr_immune_bridge_apply_inflammation(
    pr_immune_bridge_t bridge,
    const pr_memory_node_t* node,
    nimcp_quaternion_t* quat_out)
{
    if (!bridge || !node || !quat_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_immune_bridge_apply_inflammation: required parameter is NULL (bridge, node, quat_out)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_apply_inflammation", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    *quat_out = node->state;

    if (!bridge->config.enable_inflammation_effects) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Apply inflammation-based reduction */
    pr_inflammation_impact_t impact = bridge->cytokine_effects.impact_level;
    if (impact > PR_INFLAMMATION_NONE) {
        /* Reduce consolidation proportional to impact */
        float w_reduction = 0.1f * impact;
        quat_out->w = nimcp_myelin_clamp(quat_out->w - w_reduction,
                             PR_IMMUNE_QUAT_MIN, PR_IMMUNE_QUAT_MAX);

        /* Reduce accessibility proportional to impact */
        float z_reduction = 0.05f * impact;
        quat_out->z = nimcp_myelin_clamp(quat_out->z - z_reduction,
                             PR_IMMUNE_QUAT_MIN, PR_IMMUNE_QUAT_MAX);

        bridge->stats.inflammation_effects++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float pr_immune_bridge_get_consolidation_modifier(pr_immune_bridge_t bridge) {
    if (!bridge) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_get_consolidation_mo", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float modifier = bridge->cytokine_effects.net_consolidation_modifier;
    nimcp_mutex_unlock(bridge->base.mutex);

    return modifier;
}

float pr_immune_bridge_get_accessibility_modifier(pr_immune_bridge_t bridge) {
    if (!bridge) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_get_accessibility_mo", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float modifier = bridge->cytokine_effects.net_accessibility_modifier;
    nimcp_mutex_unlock(bridge->base.mutex);

    return modifier;
}

//=============================================================================
// Cleanup Tagging API
//=============================================================================

int pr_immune_bridge_tag_for_cleanup(
    pr_immune_bridge_t bridge,
    uint64_t node_id,
    pr_cleanup_reason_t reason,
    float strength)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_immune_bridge_tag_for_cleanup: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_cleanup_tagging) return 0;

    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_tag_for_cleanup", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if already tagged */
    if (find_cleanup_tag_unlocked(bridge, node_id)) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;  /* Already tagged */
    }

    /* Create tag */
    pr_cleanup_tag_t tag = {
        .node_id = node_id,
        .reason = reason,
        .strength_at_tag = strength,
        .tag_time_ms = nimcp_time_get_ms(),
        .processed = false,
        .cleanup_attempts = 0
    };

    int result = add_cleanup_tag_unlocked(bridge, &tag);

    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

bool pr_immune_bridge_is_tagged(
    pr_immune_bridge_t bridge,
    uint64_t node_id)
{
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_is_tagged", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool tagged = (find_cleanup_tag_unlocked(bridge, node_id) != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);

    return tagged;
}

int pr_immune_bridge_get_tag(
    pr_immune_bridge_t bridge,
    uint64_t node_id,
    pr_cleanup_tag_t* tag_out)
{
    if (!bridge || !tag_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_immune_bridge_get_tag: required parameter is NULL (bridge, tag_out)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_get_tag", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    pr_cleanup_tag_t* tag = find_cleanup_tag_unlocked(bridge, node_id);
    if (!tag) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_immune_bridge_get_tag: tag is NULL");
        return -1;
    }

    *tag_out = *tag;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int pr_immune_bridge_untag(
    pr_immune_bridge_t bridge,
    uint64_t node_id)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_immune_bridge_untag: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_untag", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    pr_cleanup_tag_t* tag = find_cleanup_tag_unlocked(bridge, node_id);
    if (!tag) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_immune_bridge_untag: tag is NULL");
        return -1;
    }

    /* Mark as processed to effectively remove */
    tag->processed = true;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int pr_immune_bridge_get_next_cleanup(
    pr_immune_bridge_t bridge,
    pr_cleanup_tag_t* tag_out)
{
    if (!bridge || !tag_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_immune_bridge_get_next_cleanup: required parameter is NULL (bridge, tag_out)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_get_next_cleanup", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Find next unprocessed tag */
    while (bridge->cleanup_count > 0) {
        pr_cleanup_tag_t* tag = &bridge->cleanup_queue[bridge->cleanup_read_idx];

        if (!tag->processed) {
            *tag_out = *tag;
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }

        /* Skip processed tags */
        bridge->cleanup_read_idx = (bridge->cleanup_read_idx + 1) % bridge->cleanup_capacity;
        bridge->cleanup_count--;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_immune_bridge_get_next_cleanup: operation failed");
    return -1;  /* Queue empty */
}

int pr_immune_bridge_cleanup_complete(
    pr_immune_bridge_t bridge,
    uint64_t node_id)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_immune_bridge_cleanup_complete: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_cleanup_complete", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    pr_cleanup_tag_t* tag = find_cleanup_tag_unlocked(bridge, node_id);
    if (tag) {
        tag->processed = true;
        bridge->stats.nodes_cleaned++;
        bridge->sim_coordination.nodes_cleaned++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return tag ? 0 : -1;
}

//=============================================================================
// Inflammation Processing API
//=============================================================================

int pr_immune_bridge_process_inflammation(pr_immune_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_immune_bridge_process_inflammation: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_process_inflammation", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->immune_system) {
        /* Get inflammation level from immune system */
        brain_inflammation_level_t level =
            brain_immune_get_inflammation_level(bridge->immune_system);

        /* Convert to 0-1 scale */
        float new_level = (float)level / (float)INFLAMMATION_STORM;
        new_level = nimcp_myelin_clamp(new_level, 0.0f, 1.0f);

        /* Track chronic inflammation */
        uint64_t now = nimcp_time_get_ms();
        if (new_level >= PR_IMMUNE_INFLAMMATION_MILD_THRESHOLD) {
            if (bridge->inflammation_level < PR_IMMUNE_INFLAMMATION_MILD_THRESHOLD) {
                bridge->inflammation_start_ms = now;
            }

            /* Check for chronic (> 1 hour of inflammation) */
            uint64_t duration_ms = now - bridge->inflammation_start_ms;
            bridge->chronic_inflammation = (duration_ms > 3600000);  /* 1 hour */
        } else {
            bridge->inflammation_start_ms = 0;
            bridge->chronic_inflammation = false;
        }

        bridge->inflammation_level = new_level;
    }

    /* Update derived effects */
    update_cytokine_effects(bridge);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

pr_inflammation_impact_t pr_immune_bridge_get_inflammation_impact(
    pr_immune_bridge_t bridge)
{
    if (!bridge) return PR_INFLAMMATION_NONE;

    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_get_inflammation_imp", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    pr_inflammation_impact_t impact = bridge->cytokine_effects.impact_level;
    nimcp_mutex_unlock(bridge->base.mutex);

    return impact;
}

bool pr_immune_bridge_is_chronic_inflammation(pr_immune_bridge_t bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_is_chronic_inflammat", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool chronic = bridge->chronic_inflammation;
    nimcp_mutex_unlock(bridge->base.mutex);

    return chronic;
}

float pr_immune_bridge_get_decay_multiplier(pr_immune_bridge_t bridge) {
    if (!bridge) return 1.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_get_decay_multiplier", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float mult = bridge->cytokine_effects.decay_rate_multiplier;
    nimcp_mutex_unlock(bridge->base.mutex);

    return mult;
}

//=============================================================================
// Cytokine to Quaternion Mapping API
//=============================================================================

int pr_immune_bridge_cytokine_to_quaternion(
    pr_immune_bridge_t bridge,
    nimcp_quaternion_t base_quat,
    nimcp_quaternion_t* modulated_out)
{
    if (!bridge || !modulated_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_immune_bridge_cytokine_to_quaternion: required parameter is NULL (bridge, modulated_out)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_cytokine_to_quaterni", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    *modulated_out = base_quat;

    if (!bridge->config.enable_cytokine_modulation) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Apply net consolidation modifier to w */
    float w_delta = bridge->cytokine_effects.net_consolidation_modifier * 0.15f;
    modulated_out->w = nimcp_myelin_clamp(modulated_out->w + w_delta,
                              PR_IMMUNE_QUAT_MIN, PR_IMMUNE_QUAT_MAX);

    /* Apply net accessibility modifier to z */
    float z_delta = bridge->cytokine_effects.net_accessibility_modifier * 0.1f;
    modulated_out->z = nimcp_myelin_clamp(modulated_out->z + z_delta,
                              PR_IMMUNE_QUAT_MIN, PR_IMMUNE_QUAT_MAX);

    /* IFN-gamma can reduce salience (y) during surveillance */
    if (bridge->cytokine_effects.ifn_gamma_surveillance_level > 0.5f) {
        float y_delta = -0.05f * (bridge->cytokine_effects.ifn_gamma_surveillance_level - 0.5f);
        modulated_out->y = nimcp_myelin_clamp(modulated_out->y + y_delta,
                                  PR_IMMUNE_QUAT_MIN, PR_IMMUNE_QUAT_MAX);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int pr_immune_bridge_get_cytokine_effects(
    pr_immune_bridge_t bridge,
    pr_cytokine_memory_effects_t* effects_out)
{
    if (!bridge || !effects_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_immune_bridge_get_cytokine_effects: required parameter is NULL (bridge, effects_out)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_get_cytokine_effects", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *effects_out = bridge->cytokine_effects;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pr_immune_bridge_apply_cytokine(
    pr_immune_bridge_t bridge,
    brain_cytokine_type_t cytokine,
    float concentration,
    nimcp_quaternion_t base_quat,
    nimcp_quaternion_t* result_out)
{
    if (!bridge || !result_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_immune_bridge_apply_cytokine: required parameter is NULL (bridge, result_out)");
        return -1;
    }

    *result_out = base_quat;
    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_apply_cytokine", 0.0f);


    concentration = nimcp_myelin_clamp(concentration, 0.0f, 1.0f);

    switch (cytokine) {
        case BRAIN_CYTOKINE_IL1:
            /* IL-1β reduces consolidation */
            result_out->w = nimcp_myelin_clamp(result_out->w -
                concentration * PR_IMMUNE_IL1_CONSOLIDATION_REDUCTION,
                PR_IMMUNE_QUAT_MIN, PR_IMMUNE_QUAT_MAX);
            break;

        case BRAIN_CYTOKINE_TNF:
            /* TNF-α reduces consolidation more strongly */
            result_out->w = nimcp_myelin_clamp(result_out->w -
                concentration * PR_IMMUNE_TNF_CONSOLIDATION_REDUCTION,
                PR_IMMUNE_QUAT_MIN, PR_IMMUNE_QUAT_MAX);
            break;

        case BRAIN_CYTOKINE_IL6:
            /* IL-6 reduces accessibility */
            result_out->z = nimcp_myelin_clamp(result_out->z -
                concentration * PR_IMMUNE_IL6_ACCESSIBILITY_REDUCTION,
                PR_IMMUNE_QUAT_MIN, PR_IMMUNE_QUAT_MAX);
            break;

        case BRAIN_CYTOKINE_IL10:
            /* IL-10 boosts consolidation */
            result_out->w = nimcp_myelin_clamp(result_out->w +
                concentration * PR_IMMUNE_IL10_CONSOLIDATION_BOOST,
                PR_IMMUNE_QUAT_MIN, PR_IMMUNE_QUAT_MAX);
            break;

        case BRAIN_CYTOKINE_IFN_GAMMA:
            /* IFN-γ doesn't directly affect quaternion */
            break;

        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_immune_bridge_apply_cytokine: operation failed");
            return -1;
    }

    return 0;
}

//=============================================================================
// Sleep-Immune-Memory Coordination API
//=============================================================================

int pr_immune_bridge_sleep_consolidation(
    pr_immune_bridge_t bridge,
    const pr_memory_node_t* node,
    nimcp_quaternion_t* quat_out)
{
    if (!bridge || !node || !quat_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_immune_bridge_sleep_consolidation: required parameter is NULL (bridge, node, quat_out)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_sleep_consolidation", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    *quat_out = node->state;

    if (!bridge->config.enable_sleep_coordination) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Apply phase-specific consolidation boost */
    pr_sim_phase_t phase = bridge->sim_coordination.current_phase;
    float boost = 0.0f;

    switch (phase) {
        case PR_SIM_PHASE_DEEP_SLEEP:
            /* Strong consolidation in deep NREM */
            boost = bridge->config.deep_sleep_consolidation_boost *
                    bridge->sim_coordination.immune_sync_level;
            quat_out->w = nimcp_myelin_clamp(quat_out->w + boost,
                                 PR_IMMUNE_QUAT_MIN, PR_IMMUNE_QUAT_MAX);
            bridge->stats.consolidation_boosts++;
            bridge->stats.sleep_consolidations++;
            break;

        case PR_SIM_PHASE_LIGHT_SLEEP:
            /* Mild consolidation in light sleep */
            boost = bridge->config.deep_sleep_consolidation_boost * 0.3f;
            quat_out->w = nimcp_myelin_clamp(quat_out->w + boost,
                                 PR_IMMUNE_QUAT_MIN, PR_IMMUNE_QUAT_MAX);
            bridge->stats.consolidation_boosts++;
            bridge->stats.sleep_consolidations++;
            break;

        case PR_SIM_PHASE_REM_SLEEP:
            /* REM focuses on cleanup, not consolidation */
            /* But does improve accessibility */
            quat_out->z = nimcp_myelin_clamp(quat_out->z + 0.02f,
                                 PR_IMMUNE_QUAT_MIN, PR_IMMUNE_QUAT_MAX);
            break;

        default:
            break;
    }

    /* Apply cytokine modulation during sleep */
    if (bridge->config.enable_cytokine_modulation &&
        bridge->sim_coordination.immune_consolidation_active) {
        float w_mod = bridge->cytokine_effects.net_consolidation_modifier * 0.05f;
        quat_out->w = nimcp_myelin_clamp(quat_out->w + w_mod,
                             PR_IMMUNE_QUAT_MIN, PR_IMMUNE_QUAT_MAX);
    }

    bridge->sim_coordination.nodes_consolidated++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

pr_sim_phase_t pr_immune_bridge_get_sim_phase(pr_immune_bridge_t bridge) {
    if (!bridge) return PR_SIM_PHASE_AWAKE;

    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_get_sim_phase", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    pr_sim_phase_t phase = bridge->sim_coordination.current_phase;
    nimcp_mutex_unlock(bridge->base.mutex);

    return phase;
}

int pr_immune_bridge_get_sim_coordination(
    pr_immune_bridge_t bridge,
    pr_sim_coordination_t* coordination_out)
{
    if (!bridge || !coordination_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_immune_bridge_get_sim_coordination: required parameter is NULL (bridge, coordination_out)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_get_sim_coordination", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *coordination_out = bridge->sim_coordination;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pr_immune_bridge_sync_sleep_phase(pr_immune_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_immune_bridge_sync_sleep_phase: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_sync_sleep_phase", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    update_sim_coordination(bridge);
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool pr_immune_bridge_is_deep_sleep_consolidation(pr_immune_bridge_t bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_is_deep_sleep_consol", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool active = (bridge->sim_coordination.current_phase == PR_SIM_PHASE_DEEP_SLEEP) &&
                  bridge->sim_coordination.immune_consolidation_active;
    nimcp_mutex_unlock(bridge->base.mutex);

    return active;
}

bool pr_immune_bridge_is_rem_cleanup_active(pr_immune_bridge_t bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_is_rem_cleanup_activ", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool active = (bridge->sim_coordination.current_phase == PR_SIM_PHASE_REM_SLEEP);
    nimcp_mutex_unlock(bridge->base.mutex);

    return active;
}

//=============================================================================
// Corruption Detection API
//=============================================================================

bool pr_immune_bridge_detect_corruption(
    pr_immune_bridge_t bridge,
    const pr_memory_node_t* node)
{
    if (!bridge || !node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_immune_bridge_detect_corruption: required parameter is NULL (bridge, node)");
        return false;
    }
    if (!bridge->config.enable_corruption_detection) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_immune_bridge_detect_corruption: bridge->config is NULL");
        return false;
    }

    /* Check quaternion validity */
    if (!pr_immune_bridge_validate_quaternion(bridge, node->state)) {
        return true;
    }

    /* Check for NaN in strength */
    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_detect_corruption", 0.0f);


    if (isnan(node->current_strength) || isinf(node->current_strength)) {
        return true;
    }

    /* Check for negative strength */
    if (node->current_strength < 0.0f) {
        return true;
    }

    /* Check tier validity */
    if (node->tier >= PR_MEMORY_TIER_COUNT) {
        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_immune_bridge_detect_corruption: capacity exceeded");
    return false;
}

bool pr_immune_bridge_validate_quaternion(
    pr_immune_bridge_t bridge,
    nimcp_quaternion_t quat)
{
    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_validate_quaternion", 0.0f);


    (void)bridge;  /* Unused but kept for API consistency */

    /* Check for NaN/Inf */
    if (isnan(quat.w) || isnan(quat.x) || isnan(quat.y) || isnan(quat.z)) {
        return false;
    }
    if (isinf(quat.w) || isinf(quat.x) || isinf(quat.y) || isinf(quat.z)) {
        return false;
    }

    /* Check component ranges for memory semantics */
    /* w (consolidation): [0, 1] */
    if (quat.w < -PR_IMMUNE_EPSILON || quat.w > 1.0f + PR_IMMUNE_EPSILON) {
        return false;
    }

    /* x (emotion): [-1, +1] */
    if (quat.x < -1.0f - PR_IMMUNE_EPSILON || quat.x > 1.0f + PR_IMMUNE_EPSILON) {
        return false;
    }

    /* y (salience): [0, 1] */
    if (quat.y < -PR_IMMUNE_EPSILON || quat.y > 1.0f + PR_IMMUNE_EPSILON) {
        return false;
    }

    /* z (accessibility): [0, 1] */
    if (quat.z < -PR_IMMUNE_EPSILON || quat.z > 1.0f + PR_IMMUNE_EPSILON) {
        return false;
    }

    return true;
}

//=============================================================================
// Main Update Function
//=============================================================================

int pr_immune_bridge_update(
    pr_immune_bridge_t bridge,
    float dt_ms)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_immune_bridge_update: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_update", 0.0f);


    uint64_t start_time = nimcp_time_get_us();

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update cytokine effects from immune system */
    update_cytokine_effects(bridge);

    /* Process inflammation state */
    if (bridge->config.enable_inflammation_effects && bridge->immune_system) {
        brain_inflammation_level_t level =
            brain_immune_get_inflammation_level(bridge->immune_system);
        bridge->inflammation_level = (float)level / (float)INFLAMMATION_STORM;
    }

    /* Update SIM coordination */
    if (bridge->config.enable_sleep_coordination) {
        update_sim_coordination(bridge);
        bridge->sim_coordination.phase_duration_ms += (uint64_t)dt_ms;
    }

    /* Process cleanup queue (limited per cycle) */
    if (bridge->config.enable_cleanup_tagging) {
        uint32_t processed = 0;
        uint32_t max_per_cycle = bridge->config.max_cleanup_per_cycle;

        /* Expire old tags */
        uint64_t now = nimcp_time_get_ms();
        uint64_t expire_threshold = 300000;  /* 5 minutes */

        for (uint32_t i = 0; i < bridge->cleanup_count && processed < max_per_cycle; i++) {
            uint32_t idx = (bridge->cleanup_read_idx + i) % bridge->cleanup_capacity;
            pr_cleanup_tag_t* tag = &bridge->cleanup_queue[idx];

            if (!tag->processed && (now - tag->tag_time_ms) > expire_threshold) {
                tag->processed = true;
                bridge->stats.tags_expired++;
                processed++;
            }
        }
    }

    /* Update statistics */
    bridge->stats.total_updates++;
    bridge->stats.current_impact = bridge->cytokine_effects.impact_level;
    bridge->stats.cleanup_queue_size = bridge->cleanup_count;

    /* Update average modifiers (exponential moving average) */
    float alpha = 0.1f;
    bridge->stats.avg_consolidation_modifier =
        alpha * bridge->cytokine_effects.net_consolidation_modifier +
        (1.0f - alpha) * bridge->stats.avg_consolidation_modifier;
    bridge->stats.avg_accessibility_modifier =
        alpha * bridge->cytokine_effects.net_accessibility_modifier +
        (1.0f - alpha) * bridge->stats.avg_accessibility_modifier;

    bridge->last_update_ms = nimcp_time_get_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Update timing stats */
    uint64_t elapsed_us = nimcp_time_get_us() - start_time;
    bridge->stats.avg_update_time_us =
        alpha * (float)elapsed_us + (1.0f - alpha) * bridge->stats.avg_update_time_us;

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

//=============================================================================
// Statistics and Information
//=============================================================================

int pr_immune_bridge_get_stats(
    pr_immune_bridge_t bridge,
    pr_immune_bridge_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_immune_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_get_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

uint32_t pr_immune_bridge_get_cleanup_queue_size(pr_immune_bridge_t bridge) {
    if (!bridge) return 0;

    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_get_cleanup_queue_si", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    uint32_t size = bridge->cleanup_count;
    nimcp_mutex_unlock(bridge->base.mutex);

    return size;
}

bool pr_immune_bridge_is_immune_connected(pr_immune_bridge_t bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_is_immune_connected", 0.0f);


    return bridge->immune_system != NULL;
}

bool pr_immune_bridge_is_sleep_connected(pr_immune_bridge_t bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_is_sleep_connected", 0.0f);


    return bridge->sleep_system != NULL;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int pr_immune_bridge_connect_bio_async(pr_immune_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_immune_bridge_connect_bio_async: bridge is NULL");
        return -1;
    }
    /* Bio-async integration would be implemented here */
    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_connect_bio_async", 0.0f);


    bridge->bio_async_connected = true;
    return 0;
}

int pr_immune_bridge_disconnect_bio_async(pr_immune_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_immune_bridge_disconnect_bio_async: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_disconnect_bio_async", 0.0f);


    bridge->bio_async_connected = false;
    return 0;
}

bool pr_immune_bridge_is_bio_async_connected(pr_immune_bridge_t bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_is_bio_async_connect", 0.0f);


    return bridge->bio_async_connected;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* pr_cleanup_reason_name(pr_cleanup_reason_t reason) {
    switch (reason) {
        case PR_CLEANUP_REASON_NONE:         return "none";
        case PR_CLEANUP_REASON_DECAY:        return "decay";
        case PR_CLEANUP_REASON_CORRUPTION:   return "corruption";
        case PR_CLEANUP_REASON_SIGNATURE:    return "signature";
        case PR_CLEANUP_REASON_ORPHAN:       return "orphan";
        case PR_CLEANUP_REASON_INFLAMMATION: return "inflammation";
        case PR_CLEANUP_REASON_MANUAL:       return "manual";
        default:                             return "unknown";
    }
}

const char* pr_inflammation_impact_name(pr_inflammation_impact_t impact) {
    switch (impact) {
        case PR_INFLAMMATION_NONE:     return "none";
        case PR_INFLAMMATION_MILD:     return "mild";
        case PR_INFLAMMATION_MODERATE: return "moderate";
        case PR_INFLAMMATION_SEVERE:   return "severe";
        case PR_INFLAMMATION_CRITICAL: return "critical";
        default:                       return "unknown";
    }
}

const char* pr_sim_phase_name(pr_sim_phase_t phase) {
    switch (phase) {
        case PR_SIM_PHASE_AWAKE:       return "awake";
        case PR_SIM_PHASE_LIGHT_SLEEP: return "light_sleep";
        case PR_SIM_PHASE_DEEP_SLEEP:  return "deep_sleep";
        case PR_SIM_PHASE_REM_SLEEP:   return "rem_sleep";
        case PR_SIM_PHASE_TRANSITION:  return "transition";
        default:                       return "unknown";
    }
}

void pr_immune_bridge_print_stats(pr_immune_bridge_t bridge) {
    if (!bridge) {
        printf("PR Immune Bridge: NULL\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_immune_bridge_heartbeat("pr_immune_br_print_stats", 0.0f);


    pr_immune_bridge_stats_t stats;
    pr_immune_bridge_get_stats(bridge, &stats);

    printf("=== PR Immune Bridge Statistics ===\n");
    printf("Modulations:\n");
    printf("  Cytokine modulations: %llu\n", (unsigned long long)stats.cytokine_modulations);
    printf("  Inflammation effects: %llu\n", (unsigned long long)stats.inflammation_effects);
    printf("  Consolidation boosts: %llu\n", (unsigned long long)stats.consolidation_boosts);
    printf("  Accessibility reductions: %llu\n", (unsigned long long)stats.accessibility_reductions);
    printf("\n");

    printf("Cleanup:\n");
    printf("  Nodes tagged: %llu\n", (unsigned long long)stats.nodes_tagged);
    printf("  Nodes cleaned: %llu\n", (unsigned long long)stats.nodes_cleaned);
    printf("  Tags expired: %llu\n", (unsigned long long)stats.tags_expired);
    printf("  Corruption detected: %llu\n", (unsigned long long)stats.corruption_detected);
    printf("  Queue size: %u\n", stats.cleanup_queue_size);
    printf("\n");

    printf("Sleep Coordination:\n");
    printf("  Sleep consolidations: %llu\n", (unsigned long long)stats.sleep_consolidations);
    printf("  REM cleanups: %llu\n", (unsigned long long)stats.rem_cleanups);
    printf("  Deep sleep promotions: %llu\n", (unsigned long long)stats.deep_sleep_promotions);
    printf("\n");

    printf("Current State:\n");
    printf("  Avg consolidation modifier: %.3f\n", stats.avg_consolidation_modifier);
    printf("  Avg accessibility modifier: %.3f\n", stats.avg_accessibility_modifier);
    printf("  Current impact: %s\n", pr_inflammation_impact_name(stats.current_impact));
    printf("\n");

    printf("Performance:\n");
    printf("  Total updates: %llu\n", (unsigned long long)stats.total_updates);
    printf("  Avg update time: %.2f us\n", stats.avg_update_time_us);
    printf("=====================================\n");
}

//=============================================================================
// Instance Health Agent Setter (B25 Upgrade)
//=============================================================================

void pr_immune_bridge_set_instance_health_agent(
    pr_immune_bridge_t bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B25 Upgrade)
//=============================================================================

int pr_immune_bridge_training_begin(pr_immune_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_immune_bridge_training_begin: NULL argument");
        return -1;
    }
    pr_immune_bridge_heartbeat_instance(bridge->health_agent, "pr_immune_bridge_training_begin", 0.0f);
    return 0;
}

int pr_immune_bridge_training_end(pr_immune_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_immune_bridge_training_end: NULL argument");
        return -1;
    }
    pr_immune_bridge_heartbeat_instance(bridge->health_agent, "pr_immune_bridge_training_end", 1.0f);
    return 0;
}

int pr_immune_bridge_training_step(pr_immune_bridge_t bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_immune_bridge_training_step: NULL argument");
        return -1;
    }
    pr_immune_bridge_heartbeat_instance(bridge->health_agent, "pr_immune_bridge_training_step", progress);
    return 0;
}
