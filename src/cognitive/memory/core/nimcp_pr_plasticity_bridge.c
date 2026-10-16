/**
 * @file nimcp_pr_plasticity_bridge.c
 * @brief Prime Resonant Plasticity Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Implementation of bidirectional integration between Prime Resonant
 *       memory system and synaptic plasticity mechanisms
 * WHY:  Enable biologically realistic learning where memory resonance affects
 *       plasticity and plasticity affects memory strength
 * HOW:  Implements STDP, BCM, homeostatic, metaplasticity, and structural
 *       plasticity with resonance/quaternion modulation
 */

#include "cognitive/memory/core/nimcp_pr_plasticity_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "glial/myelin_sheath/nimcp_myelin_math.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"

#include <string.h>
#include <math.h>
#include <stdio.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(pr_plasticity_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_pr_plasticity_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_pr_plasticity_bridge_mesh_registry = NULL;

nimcp_error_t pr_plasticity_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_pr_plasticity_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "pr_plasticity_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_MEMORY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "pr_plasticity_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_pr_plasticity_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_pr_plasticity_bridge_mesh_registry = registry;
    return err;
}

void pr_plasticity_bridge_mesh_unregister(void) {
    if (g_pr_plasticity_bridge_mesh_registry && g_pr_plasticity_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_pr_plasticity_bridge_mesh_registry, g_pr_plasticity_bridge_mesh_id);
        g_pr_plasticity_bridge_mesh_id = 0;
        g_pr_plasticity_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from pr_plasticity_bridge module (instance-level) */
static inline void pr_plasticity_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_pr_plasticity_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_pr_plasticity_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_pr_plasticity_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "PR_PLASTICITY_BRIDGE"

/* Security subsystem setters (Phase 1: Audit Gap Remediation) */
//=============================================================================
// Internal Structure Definition
//=============================================================================

/**
 * @brief Internal bridge structure
 *
 * WHAT: Complete state for plasticity bridge
 * WHY:  Encapsulate all bridge data for thread safety
 */
struct pr_plasticity_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    pr_plasticity_bridge_config_t config;

    /* BCM node state tracking */
    pr_bcm_node_state_t* bcm_nodes;
    uint32_t bcm_node_count;
    uint32_t bcm_node_capacity;

    /* Event buffer (circular) */
    pr_plasticity_event_t* events;
    uint32_t event_capacity;
    uint32_t event_count;
    uint32_t event_write_idx;

    /* Per-tier activity tracking */
    float tier_current_activity[PR_PLASTICITY_NUM_TIERS];
    uint32_t tier_node_count[PR_PLASTICITY_NUM_TIERS];

    /* Metaplasticity history */
    float* node_activity_history;
    uint32_t history_capacity;

    /* Statistics */
    pr_plasticity_bridge_stats_t stats;

    /* Bio-async */
    bool bio_async_connected;

    /* Structural plasticity timing */
    uint64_t last_remodel_time_ms;

    /* Instance health agent (B25 Upgrade) */
    nimcp_health_agent_t* health_agent;

};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(pr_plasticity_bridge, struct pr_plasticity_bridge_struct)

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Fast exponential approximation
 */
static inline float fast_exp(float x) {
    /* For STDP timescales, standard expf is fine */
    return expf(x);
}

/**
 * @brief Find BCM node state by ID
 */
static pr_bcm_node_state_t* find_bcm_node(
    pr_plasticity_bridge_t bridge,
    uint64_t node_id)
{
    for (uint32_t i = 0; i < bridge->bcm_node_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->bcm_node_count > 256) {
            pr_plasticity_bridge_heartbeat("pr_plasticit_loop",
                             (float)(i + 1) / (float)bridge->bcm_node_count);
        }

        if (bridge->bcm_nodes[i].node_id == node_id) {
            return &bridge->bcm_nodes[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_bcm_node: validation failed");
    return NULL;
}

/**
 * @brief Get or create BCM node state
 */
static pr_bcm_node_state_t* get_or_create_bcm_node(
    pr_plasticity_bridge_t bridge,
    uint64_t node_id)
{
    pr_bcm_node_state_t* node = find_bcm_node(bridge, node_id);
    if (node) return node;

    /* Need to create new entry */
    if (bridge->bcm_node_count >= bridge->bcm_node_capacity) {
        /* Expand capacity */
        uint32_t new_capacity = bridge->bcm_node_capacity * 2;
        pr_bcm_node_state_t* new_nodes = nimcp_realloc(
            bridge->bcm_nodes,
            new_capacity * sizeof(pr_bcm_node_state_t));
        if (!new_nodes) {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate new_nodes");

            return NULL;

        }
        bridge->bcm_nodes = new_nodes;
        bridge->bcm_node_capacity = new_capacity;
    }

    node = &bridge->bcm_nodes[bridge->bcm_node_count];
    node->node_id = node_id;
    node->theta = bridge->config.bcm.theta_initial;
    node->activity_avg = 0.0f;
    node->activity_squared_avg = 0.0f;
    node->resonance_avg = 0.0f;
    node->last_update_ms = nimcp_time_get_ms();
    bridge->bcm_node_count++;

    return node;
}

/**
 * @brief Add event to circular buffer
 */
static void add_event(pr_plasticity_bridge_t bridge, const pr_plasticity_event_t* event) {
    if (!bridge->config.enable_event_logging) return;
    if (!bridge->events) return;

    bridge->events[bridge->event_write_idx] = *event;
    bridge->event_write_idx = (bridge->event_write_idx + 1) % bridge->event_capacity;
    if (bridge->event_count < bridge->event_capacity) {
        bridge->event_count++;
    }
}

/**
 * @brief Get tier index for a node (placeholder - would integrate with Z-Ladder)
 */
static pr_memory_tier_t get_node_tier(
    pr_plasticity_bridge_t bridge,
    uint64_t node_id)
{
    /* In full implementation, would query Z-Ladder for tier */
    /* For now, use simple hash-based assignment for testing */
    (void)bridge;
    return (pr_memory_tier_t)(node_id % PR_PLASTICITY_NUM_TIERS);
}

//=============================================================================
// Configuration Functions
//=============================================================================

pr_plasticity_bridge_config_t pr_plasticity_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_plasticity_config", 0.0f);


    pr_plasticity_bridge_config_t config;
    memset(&config, 0, sizeof(config));

    /* STDP parameters */
    config.stdp.A_plus = PR_STDP_DEFAULT_A_PLUS;
    config.stdp.A_minus = PR_STDP_DEFAULT_A_MINUS;
    config.stdp.tau_plus = PR_STDP_DEFAULT_TAU_PLUS;
    config.stdp.tau_minus = PR_STDP_DEFAULT_TAU_MINUS;
    config.stdp.resonance_modulation = PR_STDP_DEFAULT_RESONANCE_MOD;

    /* BCM parameters */
    config.bcm.theta_initial = PR_BCM_DEFAULT_THETA_INITIAL;
    config.bcm.theta_tau = PR_BCM_DEFAULT_THETA_TAU;
    config.bcm.resonance_weight = PR_BCM_DEFAULT_RESONANCE_WEIGHT;

    /* Homeostatic parameters */
    config.homeostatic.target_rate[PR_TIER_Z0] = PR_HOMEOSTATIC_TARGET_Z0;
    config.homeostatic.target_rate[PR_TIER_Z1] = PR_HOMEOSTATIC_TARGET_Z1;
    config.homeostatic.target_rate[PR_TIER_Z2] = PR_HOMEOSTATIC_TARGET_Z2;
    config.homeostatic.target_rate[PR_TIER_Z3] = PR_HOMEOSTATIC_TARGET_Z3;
    config.homeostatic.scaling_tau = PR_HOMEOSTATIC_DEFAULT_SCALING_TAU;
    config.homeostatic.min_scale = PR_HOMEOSTATIC_MIN_SCALE;
    config.homeostatic.max_scale = PR_HOMEOSTATIC_MAX_SCALE;

    /* Metaplasticity parameters */
    config.meta.history_decay = NIMCP_EMA_DECAY_DEFAULT;
    config.meta.min_lr_scale = 0.1f;
    config.meta.max_lr_scale = 2.0f;
    config.meta.consolidation_protection = 0.5f;

    /* Structural plasticity parameters */
    config.structural.create_threshold = PR_STRUCTURAL_CREATE_THRESHOLD;
    config.structural.prune_threshold = PR_STRUCTURAL_PRUNE_THRESHOLD;
    config.structural.max_new_edges = PR_STRUCTURAL_MAX_NEW_EDGES;
    config.structural.max_prune_edges = PR_STRUCTURAL_MAX_PRUNE_EDGES;
    config.structural.remodel_interval_ms = 60000.0f;  /* 1 minute */

    /* Tier-specific parameters */
    for (int t = 0; t < PR_PLASTICITY_NUM_TIERS; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && PR_PLASTICITY_NUM_TIERS > 256) {
            pr_plasticity_bridge_heartbeat("pr_plasticit_loop",
                             (float)(t + 1) / (float)PR_PLASTICITY_NUM_TIERS);
        }

        float tier_factor = 1.0f - (float)t * 0.25f;  /* Z0=1.0, Z1=0.75, Z2=0.5, Z3=0.25 */
        config.tier[t].stdp_rate_scale = tier_factor;
        config.tier[t].bcm_tau_scale = 1.0f + (float)t * 0.5f;  /* Slower for deeper tiers */
        config.tier[t].homeostatic_strength = tier_factor;
        config.tier[t].target_activity = config.homeostatic.target_rate[t];
        config.tier[t].metaplasticity_gate = 1.0f - (float)t * 0.2f;
    }

    /* Feature enables */
    config.enable_stdp = true;
    config.enable_bcm = true;
    config.enable_homeostatic = true;
    config.enable_metaplasticity = true;
    config.enable_structural = true;

    config.consolidation_gate = PR_CONSOLIDATION_GATE_DEFAULT;
    config.enable_event_logging = true;
    config.max_events = PR_PLASTICITY_MAX_EVENTS;

    config.enable_bio_async = false;
    config.enable_coordinator_sync = false;

    return config;
}

bool pr_plasticity_config_validate(const pr_plasticity_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_plasticity_config_validate: config is NULL");
        return false;
    }

    /* STDP validation */
    if (config->stdp.A_plus <= 0.0f) {
        return false;
    }
    if (config->stdp.A_minus <= 0.0f) {
        return false;
    }
    if (config->stdp.tau_plus <= 0.0f) {
        return false;
    }
    if (config->stdp.tau_minus <= 0.0f) {
        return false;
    }
    if (config->stdp.resonance_modulation < 0.0f) {
        return false;
    }

    /* BCM validation */
    if (config->bcm.theta_tau <= 0.0f) {
        return false;
    }

    /* Homeostatic validation */
    if (config->homeostatic.scaling_tau <= 0.0f) {
        return false;
    }
    if (config->homeostatic.min_scale <= 0.0f) {
        return false;
    }
    if (config->homeostatic.max_scale <= config->homeostatic.min_scale) {
        return false;
    }

    /* Target rate validation */
    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_plasticity_config", 0.0f);


    for (int t = 0; t < PR_PLASTICITY_NUM_TIERS; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && PR_PLASTICITY_NUM_TIERS > 256) {
            pr_plasticity_bridge_heartbeat("pr_plasticit_loop",
                             (float)(t + 1) / (float)PR_PLASTICITY_NUM_TIERS);
        }

        if (config->homeostatic.target_rate[t] < 0.0f ||
            config->homeostatic.target_rate[t] > 1.0f) {
            return false;
        }
    }

    /* Consolidation gate validation */
    if (config->consolidation_gate < 0.0f || config->consolidation_gate > 1.0f) {
        return false;
    }

    return true;
}

//=============================================================================
// Bridge Lifecycle
//=============================================================================

pr_plasticity_bridge_t pr_plasticity_bridge_create(
    const pr_plasticity_bridge_config_t* config)
{
    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_create", 0.0f);


    pr_plasticity_bridge_t bridge = nimcp_calloc(1, sizeof(struct pr_plasticity_bridge_struct));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        if (!pr_plasticity_config_validate(config)) {
            nimcp_free(bridge);
            bridge = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_plasticity_bridge_create: pr_plasticity_config_validate is NULL");
            return NULL;
        }
        bridge->config = *config;
    } else {
        bridge->config = pr_plasticity_config_default();
    }

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "pr_plasticity") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "pr_plasticity_bridge_create: validation failed");
        return NULL;
    }

    /* Allocate BCM node state array */
    bridge->bcm_node_capacity = 256;
    bridge->bcm_nodes = nimcp_calloc(bridge->bcm_node_capacity, sizeof(pr_bcm_node_state_t));
    if (!bridge->bcm_nodes) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_plasticity_bridge_create: bridge->bcm_nodes is NULL");
        return NULL;
    }
    bridge->bcm_node_count = 0;

    /* Allocate event buffer */
    if (bridge->config.enable_event_logging) {
        bridge->event_capacity = bridge->config.max_events;
        bridge->events = nimcp_calloc(bridge->event_capacity, sizeof(pr_plasticity_event_t));
        if (!bridge->events) {
            nimcp_free(bridge->bcm_nodes);
            bridge_base_cleanup(&bridge->base);
            nimcp_free(bridge);
            bridge = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_plasticity_bridge_create: bridge->events is NULL");
            return NULL;
        }
        bridge->event_count = 0;
        bridge->event_write_idx = 0;
    }

    /* Initialize tier tracking */
    for (int t = 0; t < PR_PLASTICITY_NUM_TIERS; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && PR_PLASTICITY_NUM_TIERS > 256) {
            pr_plasticity_bridge_heartbeat("pr_plasticit_loop",
                             (float)(t + 1) / (float)PR_PLASTICITY_NUM_TIERS);
        }

        bridge->tier_current_activity[t] = bridge->config.homeostatic.target_rate[t];
        bridge->tier_node_count[t] = 0;
    }

    /* Initialize timing */
    bridge->last_remodel_time_ms = nimcp_time_get_ms();

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(pr_plasticity_bridge_stats_t));

    bridge->bio_async_connected = false;

    return bridge;
}

void pr_plasticity_bridge_destroy(pr_plasticity_bridge_t bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "pr_plasticity");

    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_destroy", 0.0f);


    if (bridge->bio_async_connected) {
        pr_plasticity_disconnect_bio_async(bridge);
    }

    if (bridge->bcm_nodes) nimcp_free(bridge->bcm_nodes);
    if (bridge->events) nimcp_free(bridge->events);
    if (bridge->node_activity_history) nimcp_free(bridge->node_activity_history);

    /* Cleanup base bridge infrastructure */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
}

int pr_plasticity_bridge_reset(pr_plasticity_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_plasticity_bridge_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset BCM nodes */
    for (uint32_t i = 0; i < bridge->bcm_node_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->bcm_node_count > 256) {
            pr_plasticity_bridge_heartbeat("pr_plasticit_loop",
                             (float)(i + 1) / (float)bridge->bcm_node_count);
        }

        bridge->bcm_nodes[i].theta = bridge->config.bcm.theta_initial;
        bridge->bcm_nodes[i].activity_avg = 0.0f;
        bridge->bcm_nodes[i].activity_squared_avg = 0.0f;
        bridge->bcm_nodes[i].resonance_avg = 0.0f;
        bridge->bcm_nodes[i].last_update_ms = nimcp_time_get_ms();
    }

    /* Reset event buffer */
    bridge->event_count = 0;
    bridge->event_write_idx = 0;

    /* Reset tier tracking */
    for (int t = 0; t < PR_PLASTICITY_NUM_TIERS; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && PR_PLASTICITY_NUM_TIERS > 256) {
            pr_plasticity_bridge_heartbeat("pr_plasticit_loop",
                             (float)(t + 1) / (float)PR_PLASTICITY_NUM_TIERS);
        }

        bridge->tier_current_activity[t] = bridge->config.homeostatic.target_rate[t];
        bridge->tier_node_count[t] = 0;
    }

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(pr_plasticity_bridge_stats_t));

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// STDP Integration Functions
//=============================================================================

float pr_stdp_compute_delta(
    pr_plasticity_bridge_t bridge,
    float pre_time_ms,
    float post_time_ms,
    float resonance)
{
    if (!bridge) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_stdp_compute_delt", 0.0f);


    float dt = post_time_ms - pre_time_ms;
    float delta = 0.0f;

    if (dt > 0) {
        /* LTP: Pre before Post */
        delta = bridge->config.stdp.A_plus * fast_exp(-dt / bridge->config.stdp.tau_plus);
    } else if (dt < 0) {
        /* LTD: Post before Pre */
        delta = -bridge->config.stdp.A_minus * fast_exp(dt / bridge->config.stdp.tau_minus);
    }

    /* Modulate by resonance */
    float modulation = 1.0f + bridge->config.stdp.resonance_modulation * resonance;
    return delta * modulation;
}

float pr_stdp_modulate_by_resonance(
    pr_plasticity_bridge_t bridge,
    float base_delta,
    float resonance)
{
    if (!bridge) return base_delta;
    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_stdp_modulate_by_", 0.0f);


    float modulation = 1.0f + bridge->config.stdp.resonance_modulation * resonance;
    return base_delta * modulation;
}

float pr_stdp_apply_to_entanglement(
    pr_plasticity_bridge_t bridge,
    entangle_graph_t graph,
    uint64_t from_id,
    uint64_t to_id,
    float pre_time_ms,
    float post_time_ms,
    float resonance)
{
    if (!bridge || !graph) return -1.0f;
    if (!bridge->config.enable_stdp) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_stdp_apply_to_ent", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Get current edge */
    entangle_edge_t edge;
    if (!entangle_get_edge(graph, from_id, to_id, &edge)) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return -1.0f;
    }

    /* Compute STDP delta */
    float delta = pr_stdp_compute_delta(bridge, pre_time_ms, post_time_ms, resonance);
    if (fabsf(delta) < PR_PLASTICITY_EPSILON) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return edge.weight;
    }

    /* Check consolidation gate */
    /* Note: Would need quaternion from memory node in full implementation */
    /* For now, we allow all plasticity */

    /* Apply weight change */
    float old_weight = edge.weight;
    edge.weight = nimcp_myelin_clamp(edge.weight + delta, 0.0f, 1.0f);

    /* Update edge in graph */
    entangle_update_edge(graph, &edge);

    /* Log event */
    if (bridge->config.enable_event_logging) {
        pr_plasticity_event_t event = {
            .from_node = from_id,
            .to_node = to_id,
            .type = PR_PLASTICITY_STDP,
            .delta_weight = delta,
            .pre_weight = old_weight,
            .post_weight = edge.weight,
            .resonance_at_event = resonance,
            .timestamp_ms = nimcp_time_get_ms()
        };
        add_event(bridge, &event);
    }

    /* Update statistics */
    if (delta > 0) {
        bridge->stats.stdp_ltp_events++;
    } else {
        bridge->stats.stdp_ltd_events++;
    }
    bridge->stats.total_weight_change += fabsf(delta);

    nimcp_mutex_unlock(bridge->base.mutex);

    return edge.weight;
}

uint32_t pr_stdp_batch_update(
    pr_plasticity_bridge_t bridge,
    entangle_graph_t graph,
    const uint64_t* from_ids,
    const uint64_t* to_ids,
    const float* pre_times,
    const float* post_times,
    const float* resonances,
    uint32_t count)
{
    if (!bridge || !graph) return 0;
    if (!from_ids || !to_ids || !pre_times || !post_times || !resonances) return 0;
    if (!bridge->config.enable_stdp) return 0;

    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_stdp_batch_update", 0.0f);


    uint32_t updated = 0;

    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_plasticity_bridge_heartbeat("pr_plasticit_loop",
                             (float)(i + 1) / (float)count);
        }

        float new_weight = pr_stdp_apply_to_entanglement(
            bridge, graph,
            from_ids[i], to_ids[i],
            pre_times[i], post_times[i],
            resonances[i]);
        if (new_weight >= 0.0f) {
            updated++;
        }
    }

    return updated;
}

//=============================================================================
// BCM Integration Functions
//=============================================================================

float pr_bcm_get_phi(float activity, float theta) {
    /* phi(y) = y * (y - theta) */
    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_bcm_get_phi", 0.0f);


    return activity * (activity - theta);
}

float pr_bcm_compute_threshold(
    pr_plasticity_bridge_t bridge,
    uint64_t node_id)
{
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_bcm_compute_thres", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    pr_bcm_node_state_t* node = find_bcm_node(bridge, node_id);
    if (!node) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return bridge->config.bcm.theta_initial;
    }

    float theta = node->theta;

    nimcp_mutex_unlock(bridge->base.mutex);

    return theta;
}

int pr_bcm_update_history(
    pr_plasticity_bridge_t bridge,
    uint64_t node_id,
    float activity)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_bcm_update_history: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bcm) return 0;

    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_bcm_update_histor", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    pr_bcm_node_state_t* node = get_or_create_bcm_node(bridge, node_id);
    if (!node) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_bcm_update_history: node is NULL");
        return -1;
    }

    /* Get time delta */
    uint64_t now_ms = nimcp_time_get_ms();
    float dt_hours = (float)(now_ms - node->last_update_ms) / (1000.0f * 3600.0f);
    node->last_update_ms = now_ms;

    /* Update running averages */
    float alpha = dt_hours / bridge->config.bcm.theta_tau;
    alpha = nimcp_myelin_clamp(alpha, 0.0f, 1.0f);

    node->activity_avg = (1.0f - alpha) * node->activity_avg + alpha * activity;
    node->activity_squared_avg = (1.0f - alpha) * node->activity_squared_avg + alpha * (activity * activity);

    /* Update theta (BCM threshold) */
    node->theta = node->activity_squared_avg;

    /* Weight by resonance if tracked */
    node->theta = node->theta * (1.0f - bridge->config.bcm.resonance_weight) +
                  node->resonance_avg * bridge->config.bcm.resonance_weight;

    /* Clamp theta to reasonable range */
    node->theta = nimcp_myelin_clamp(node->theta, 0.1f, 0.9f);

    bridge->stats.bcm_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pr_bcm_apply_to_node(
    pr_plasticity_bridge_t bridge,
    entangle_graph_t graph,
    uint64_t node_id,
    float activity)
{
    if (!bridge || !graph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_bcm_apply_to_node: required parameter is NULL (bridge, graph)");
        return -1;
    }
    if (!bridge->config.enable_bcm) return 0;

    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_bcm_apply_to_node", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Get BCM state */
    pr_bcm_node_state_t* bcm_node = get_or_create_bcm_node(bridge, node_id);
    if (!bcm_node) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_bcm_apply_to_node: bcm_node is NULL");
        return -1;
    }

    /* Compute phi */
    float phi = pr_bcm_get_phi(activity, bcm_node->theta);

    /* Get outgoing edges */
    entangle_edge_t edges[256];
    size_t edge_count = 0;
    if (!entangle_get_outgoing(graph, node_id, edges, 256, &edge_count)) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_bcm_apply_to_node: entangle_get_outgoing is NULL");
        return -1;
    }

    int updated = 0;

    /* Apply BCM to each edge */
    for (size_t i = 0; i < edge_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && edge_count > 256) {
            pr_plasticity_bridge_heartbeat("pr_plasticit_loop",
                             (float)(i + 1) / (float)edge_count);
        }

        float old_weight = edges[i].weight;

        /* BCM weight change: dw = eta * phi * x_pre (use weight as proxy for x_pre) */
        float dw = 0.01f * phi * edges[i].weight;
        edges[i].weight = nimcp_myelin_clamp(edges[i].weight + dw, 0.0f, 1.0f);

        if (fabsf(dw) > PR_PLASTICITY_EPSILON) {
            entangle_update_edge(graph, &edges[i]);
            updated++;

            /* Log event */
            if (bridge->config.enable_event_logging) {
                pr_plasticity_event_t event = {
                    .from_node = node_id,
                    .to_node = edges[i].to_id,
                    .type = PR_PLASTICITY_BCM,
                    .delta_weight = dw,
                    .pre_weight = old_weight,
                    .post_weight = edges[i].weight,
                    .resonance_at_event = edges[i].resonance_score,
                    .timestamp_ms = nimcp_time_get_ms()
                };
                add_event(bridge, &event);
            }

            /* Update stats */
            if (dw > 0) bridge->stats.stdp_ltp_events++;
            else bridge->stats.stdp_ltd_events++;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return updated;
}

//=============================================================================
// Homeostatic Integration Functions
//=============================================================================

float pr_homeostatic_get_scaling(
    pr_plasticity_bridge_t bridge,
    uint64_t node_id,
    float current_activity,
    pr_memory_tier_t tier)
{
    if (!bridge) return 1.0f;
    if (tier >= PR_PLASTICITY_NUM_TIERS) return 1.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_homeostatic_get_s", 0.0f);


    (void)node_id;  /* Could use for per-node tracking in extended version */

    float target = bridge->config.homeostatic.target_rate[tier];
    float deviation = target - current_activity;

    /* Scale factor: move toward target */
    float scale = 1.0f + deviation * bridge->config.tier[tier].homeostatic_strength *
                  (1.0f / bridge->config.homeostatic.scaling_tau);

    return nimcp_myelin_clamp(scale, bridge->config.homeostatic.min_scale,
                   bridge->config.homeostatic.max_scale);
}

float pr_homeostatic_apply_to_edge(
    pr_plasticity_bridge_t bridge,
    entangle_graph_t graph,
    uint64_t from_id,
    uint64_t to_id,
    float scale)
{
    if (!bridge || !graph) return -1.0f;
    if (scale <= 0.0f) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_homeostatic_apply", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    entangle_edge_t edge;
    if (!entangle_get_edge(graph, from_id, to_id, &edge)) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return -1.0f;
    }

    float old_weight = edge.weight;
    edge.weight = nimcp_myelin_clamp(edge.weight * scale, 0.0f, 1.0f);

    if (fabsf(edge.weight - old_weight) > PR_PLASTICITY_EPSILON) {
        entangle_update_edge(graph, &edge);

        /* Log event */
        if (bridge->config.enable_event_logging) {
            pr_plasticity_event_t event = {
                .from_node = from_id,
                .to_node = to_id,
                .type = PR_PLASTICITY_HOMEOSTATIC,
                .delta_weight = edge.weight - old_weight,
                .pre_weight = old_weight,
                .post_weight = edge.weight,
                .resonance_at_event = edge.resonance_score,
                .timestamp_ms = nimcp_time_get_ms()
            };
            add_event(bridge, &event);
        }

        bridge->stats.homeostatic_scalings++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return edge.weight;
}

uint32_t pr_homeostatic_scale_tier(
    pr_plasticity_bridge_t bridge,
    entangle_graph_t graph,
    pr_memory_tier_t tier,
    const uint64_t* node_ids,
    uint32_t node_count)
{
    if (!bridge || !graph || !node_ids) return 0;
    if (!bridge->config.enable_homeostatic) return 0;
    if (tier >= PR_PLASTICITY_NUM_TIERS) return 0;

    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_homeostatic_scale", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Calculate current tier activity */
    float total_activity = 0.0f;
    for (uint32_t n = 0; n < node_count; n++) {
        /* Phase 8: Loop progress heartbeat */
        if ((n & 0xFF) == 0 && node_count > 256) {
            pr_plasticity_bridge_heartbeat("pr_plasticit_loop",
                             (float)(n + 1) / (float)node_count);
        }

        /* Would get activity from node in full implementation */
        /* For now use BCM node state if available */
        pr_bcm_node_state_t* bcm_node = find_bcm_node(bridge, node_ids[n]);
        if (bcm_node) {
            total_activity += bcm_node->activity_avg;
        }
    }
    float avg_activity = node_count > 0 ? total_activity / (float)node_count : 0.0f;
    bridge->tier_current_activity[tier] = avg_activity;
    bridge->tier_node_count[tier] = node_count;

    /* Compute scaling factor */
    float scale = pr_homeostatic_get_scaling(bridge, 0, avg_activity, tier);

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Apply to all edges in tier */
    uint32_t scaled = 0;
    for (uint32_t n = 0; n < node_count; n++) {
        /* Phase 8: Loop progress heartbeat */
        if ((n & 0xFF) == 0 && node_count > 256) {
            pr_plasticity_bridge_heartbeat("pr_plasticit_loop",
                             (float)(n + 1) / (float)node_count);
        }

        entangle_edge_t edges[256];
        size_t edge_count = 0;

        if (entangle_get_outgoing(graph, node_ids[n], edges, 256, &edge_count)) {
            for (size_t e = 0; e < edge_count; e++) {
                /* Phase 8: Loop progress heartbeat */
                if ((e & 0xFF) == 0 && edge_count > 256) {
                    pr_plasticity_bridge_heartbeat("pr_plasticit_loop",
                                     (float)(e + 1) / (float)edge_count);
                }

                float new_weight = pr_homeostatic_apply_to_edge(
                    bridge, graph, node_ids[n], edges[e].to_id, scale);
                if (new_weight >= 0.0f) {
                    scaled++;
                }
            }
        }
    }

    return scaled;
}

int pr_homeostatic_update_targets(pr_plasticity_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_homeostatic_update_targets: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_homeostatic_updat", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Could adaptively update targets based on global network state */
    /* For now, keep configured targets */

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Metaplasticity Functions
//=============================================================================

float pr_metaplasticity_adjust_stdp(
    pr_plasticity_bridge_t bridge,
    uint64_t node_id)
{
    if (!bridge) return 1.0f;
    if (!bridge->config.enable_metaplasticity) return 1.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_metaplasticity_ad", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    pr_bcm_node_state_t* node = find_bcm_node(bridge, node_id);
    if (!node) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 1.0f;
    }

    /* High prior activity -> reduce learning rate */
    /* Low prior activity -> increase learning rate */
    float activity_level = node->activity_avg;

    /* Inverse relationship: high activity = lower rate */
    float rate_scale = 1.0f - activity_level * (1.0f - bridge->config.meta.min_lr_scale);
    rate_scale = nimcp_myelin_clamp(rate_scale, bridge->config.meta.min_lr_scale,
                         bridge->config.meta.max_lr_scale);

    bridge->stats.metaplasticity_events++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return rate_scale;
}

float pr_metaplasticity_from_consolidation(
    pr_plasticity_bridge_t bridge,
    nimcp_quaternion_t quat)
{
    if (!bridge) return 1.0f;
    if (!bridge->config.enable_metaplasticity) return 1.0f;

    /* Quaternion w = consolidation strength */
    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_metaplasticity_fr", 0.0f);


    float consolidation = quat.w;

    /* High consolidation -> low plasticity */
    float protection = bridge->config.meta.consolidation_protection;
    float rate_scale = 1.0f - consolidation * protection;

    return nimcp_myelin_clamp(rate_scale, bridge->config.meta.min_lr_scale, 1.0f);
}

//=============================================================================
// Structural Plasticity Functions
//=============================================================================

bool pr_structural_create_edge(
    pr_plasticity_bridge_t bridge,
    entangle_graph_t graph,
    uint64_t from_id,
    uint64_t to_id,
    float resonance)
{
    if (!bridge || !graph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_structural_create_edge: required parameter is NULL (bridge, graph)");
        return false;
    }
    if (!bridge->config.enable_structural) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_structural_create_edge: bridge->config is NULL");
        return false;
    }
    if (resonance < bridge->config.structural.create_threshold) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_structural_create_edge: validation failed");
        return false;
    }

    /* Check if edge already exists */
    if (entangle_has_edge(graph, from_id, to_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_structural_create_edge: validation failed");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_structural_create", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Create new edge */
    entangle_edge_t edge = {
        .from_id = from_id,
        .to_id = to_id,
        .resonance_score = resonance,
        .prime_similarity = resonance,  /* Approximate */
        .quat_similarity = 0.5f,
        .phase_coherence = 0.5f,
        .type = ENTANGLE_EDGE_ASSOCIATIVE,
        .created_time_ms = nimcp_time_get_ms(),
        .weight = resonance * 0.5f,  /* Initial weight proportional to resonance */
        .bidirectional = true
    };

    bool success = entangle_add_edge(graph, &edge);

    if (success) {
        bridge->stats.edges_created++;

        /* Log event */
        if (bridge->config.enable_event_logging) {
            pr_plasticity_event_t event = {
                .from_node = from_id,
                .to_node = to_id,
                .type = PR_PLASTICITY_STRUCTURAL,
                .delta_weight = edge.weight,
                .pre_weight = 0.0f,
                .post_weight = edge.weight,
                .resonance_at_event = resonance,
                .timestamp_ms = nimcp_time_get_ms()
            };
            add_event(bridge, &event);
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return success;
}

bool pr_structural_prune_edge(
    pr_plasticity_bridge_t bridge,
    entangle_graph_t graph,
    uint64_t from_id,
    uint64_t to_id)
{
    if (!bridge || !graph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_structural_prune_edge: required parameter is NULL (bridge, graph)");
        return false;
    }
    if (!bridge->config.enable_structural) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_structural_prune_edge: bridge->config is NULL");
        return false;
    }

    /* Get current edge */
    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_structural_prune_", 0.0f);


    entangle_edge_t edge;
    if (!entangle_get_edge(graph, from_id, to_id, &edge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_structural_prune_edge: entangle_get_edge is NULL");
        return false;
    }

    /* Check if weight is below prune threshold */
    if (edge.weight >= bridge->config.structural.prune_threshold) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_structural_prune_edge: capacity exceeded");
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bool success = entangle_remove_edge(graph, from_id, to_id);

    if (success) {
        bridge->stats.edges_pruned++;

        /* Log event */
        if (bridge->config.enable_event_logging) {
            pr_plasticity_event_t event = {
                .from_node = from_id,
                .to_node = to_id,
                .type = PR_PLASTICITY_STRUCTURAL,
                .delta_weight = -edge.weight,
                .pre_weight = edge.weight,
                .post_weight = 0.0f,
                .resonance_at_event = edge.resonance_score,
                .timestamp_ms = nimcp_time_get_ms()
            };
            add_event(bridge, &event);
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return success;
}

int pr_structural_remodel(
    pr_plasticity_bridge_t bridge,
    entangle_graph_t graph,
    const uint64_t* node_ids,
    uint32_t node_count,
    uint32_t* edges_created,
    uint32_t* edges_pruned)
{
    if (!bridge || !graph || !node_ids) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_structural_remodel: required parameter is NULL (bridge, graph, node_ids)");
        return -1;
    }
    if (!bridge->config.enable_structural) return 0;

    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_structural_remode", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, node_ids, sizeof(*node_ids));

    uint32_t created = 0;
    uint32_t pruned = 0;

    /* Phase 1: Prune weak edges */
    for (uint32_t n = 0; n < node_count && pruned < bridge->config.structural.max_prune_edges; n++) {
        entangle_edge_t edges[256];
        size_t edge_count = 0;

        if (entangle_get_outgoing(graph, node_ids[n], edges, 256, &edge_count)) {
            for (size_t e = 0; e < edge_count && pruned < bridge->config.structural.max_prune_edges; e++) {
                if (edges[e].weight < bridge->config.structural.prune_threshold) {
                    if (pr_structural_prune_edge(bridge, graph, node_ids[n], edges[e].to_id)) {
                        pruned++;
                    }
                }
            }
        }
    }

    /* Phase 2: Create new edges for high-resonance pairs */
    /* Note: In full implementation, would compute resonance between pairs */
    /* For now, skip creation to avoid expensive all-pairs computation */

    if (edges_created) *edges_created = created;
    if (edges_pruned) *edges_pruned = pruned;

    return 0;
}

//=============================================================================
// Quaternion <-> Plasticity Functions
//=============================================================================

bool pr_consolidation_gate(
    pr_plasticity_bridge_t bridge,
    nimcp_quaternion_t quat)
{
    if (!bridge) return true;  /* Allow if no bridge */

    /* Check consolidation strength */
    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_consolidation_gat", 0.0f);


    if (quat.w < bridge->config.consolidation_gate) {
        return true;  /* Plasticity allowed for fragile memories */
    }

    /* For consolidated memories, apply graduated protection */
    /* Higher w = lower probability of allowing plasticity */
    float protection = (quat.w - bridge->config.consolidation_gate) /
                       (1.0f - bridge->config.consolidation_gate);

    /* Always allow some plasticity (min 10% chance) */
    return protection < 0.9f;
}

int pr_plasticity_from_quaternion(
    pr_plasticity_bridge_t bridge,
    nimcp_quaternion_t quat,
    float* stdp_rate,
    float* bcm_rate,
    float* homeostatic_rate)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_plasticity_from_quaternion: bridge is NULL");
        return -1;
    }

    /* w (consolidation) -> protection level */
    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_plasticity_from_q", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, stdp_rate, sizeof(*stdp_rate));

    float consolidation_factor = 1.0f - quat.w * 0.5f;

    /* x (emotion) -> modulation (absolute value, extreme emotions enhance) */
    float emotion_factor = 1.0f + fabsf(quat.x) * 0.3f;

    /* y (salience) -> priority */
    float salience_factor = 1.0f + quat.y * 0.2f;

    /* z (accessibility) -> exposure */
    float accessibility_factor = 1.0f + quat.z * 0.1f;

    if (stdp_rate) {
        *stdp_rate = consolidation_factor * emotion_factor * salience_factor;
        *stdp_rate = nimcp_myelin_clamp(*stdp_rate, 0.1f, 2.0f);
    }

    if (bcm_rate) {
        *bcm_rate = consolidation_factor * accessibility_factor;
        *bcm_rate = nimcp_myelin_clamp(*bcm_rate, 0.1f, 2.0f);
    }

    if (homeostatic_rate) {
        *homeostatic_rate = 1.0f;  /* Homeostatic should be stable */
    }

    return 0;
}

int pr_quaternion_from_plasticity(
    pr_plasticity_bridge_t bridge,
    nimcp_quaternion_t quat_in,
    const pr_plasticity_event_t* events,
    uint32_t event_count,
    nimcp_quaternion_t* quat_out)
{
    if (!bridge || !events || !quat_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_quaternion_from_plasticity: required parameter is NULL (bridge, events, quat_out)");
        return -1;
    }

    *quat_out = quat_in;
    BRIDGE_BBB_VALIDATE(bridge, events, sizeof(*events));

    if (event_count == 0) return 0;

    /* Accumulate effect of plasticity events */
    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_quaternion_from_p", 0.0f);


    float total_ltp = 0.0f;
    float total_ltd = 0.0f;
    uint32_t access_count = 0;

    for (uint32_t i = 0; i < event_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && event_count > 256) {
            pr_plasticity_bridge_heartbeat("pr_plasticit_loop",
                             (float)(i + 1) / (float)event_count);
        }

        if (events[i].delta_weight > 0) {
            total_ltp += events[i].delta_weight;
        } else {
            total_ltd += fabsf(events[i].delta_weight);
        }
        access_count++;
    }

    /* Strong LTP -> increase consolidation (w) */
    if (total_ltp > total_ltd * 1.5f) {
        quat_out->w += total_ltp * 0.01f;
        quat_out->w = nimcp_myelin_clamp(quat_out->w, 0.0f, 1.0f);
    }

    /* Repeated access -> increase accessibility (z) */
    if (access_count > 5) {
        quat_out->z += (float)access_count * 0.005f;
        quat_out->z = nimcp_myelin_clamp(quat_out->z, 0.0f, 1.0f);
    }

    /* Normalize quaternion if needed */
    float mag = sqrtf(quat_out->w * quat_out->w + quat_out->x * quat_out->x +
                     quat_out->y * quat_out->y + quat_out->z * quat_out->z);
    if (mag > PR_PLASTICITY_EPSILON && fabsf(mag - 1.0f) > PR_PLASTICITY_EPSILON) {
        quat_out->w /= mag;
        quat_out->x /= mag;
        quat_out->y /= mag;
        quat_out->z /= mag;
    }

    return 0;
}

//=============================================================================
// Resonance <-> Plasticity Functions
//=============================================================================

float pr_resonance_to_learning_rate(
    pr_plasticity_bridge_t bridge,
    float resonance)
{
    if (!bridge) return 1.0f;
    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_resonance_to_lear", 0.0f);


    return 1.0f + bridge->config.stdp.resonance_modulation * resonance;
}

float pr_resonance_to_bcm_weight(
    pr_plasticity_bridge_t bridge,
    float resonance)
{
    if (!bridge) return resonance;
    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_resonance_to_bcm_", 0.0f);


    return resonance * bridge->config.bcm.resonance_weight;
}

float pr_plasticity_update_resonance(
    pr_plasticity_bridge_t bridge,
    entangle_graph_t graph,
    uint64_t from_id,
    uint64_t to_id,
    float delta_weight)
{
    if (!bridge || !graph) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_plasticity_update", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    entangle_edge_t edge;
    if (!entangle_get_edge(graph, from_id, to_id, &edge)) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return -1.0f;
    }

    /* Small resonance adjustment based on plasticity */
    float resonance_delta = delta_weight * 0.1f;  /* 10% of weight change */
    edge.resonance_score = nimcp_myelin_clamp(edge.resonance_score + resonance_delta, 0.0f, 1.0f);

    entangle_update_edge(graph, &edge);

    nimcp_mutex_unlock(bridge->base.mutex);

    return edge.resonance_score;
}

//=============================================================================
// Tier-Specific Functions
//=============================================================================

int pr_plasticity_get_tier_params(
    pr_plasticity_bridge_t bridge,
    pr_memory_tier_t tier,
    pr_tier_plasticity_params_t* params)
{
    if (!bridge || !params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_plasticity_get_tier_params: required parameter is NULL (bridge, params)");
        return -1;
    }
    if (tier >= PR_PLASTICITY_NUM_TIERS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_plasticity_get_tier_params: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_plasticity_get_ti", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *params = bridge->config.tier[tier];
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pr_plasticity_apply_tier_rules(
    pr_plasticity_bridge_t bridge,
    entangle_graph_t graph,
    uint64_t node_id,
    pr_memory_tier_t tier,
    float activity)
{
    if (!bridge || !graph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_plasticity_apply_tier_rules: required parameter is NULL (bridge, graph)");
        return -1;
    }
    if (tier >= PR_PLASTICITY_NUM_TIERS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_plasticity_apply_tier_rules: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_plasticity_apply_", 0.0f);


    int modifications = 0;

    /* Update BCM history */
    if (bridge->config.enable_bcm) {
        pr_bcm_update_history(bridge, node_id, activity);
        modifications++;
    }

    /* Apply BCM with tier scaling */
    if (bridge->config.enable_bcm) {
        int bcm_updates = pr_bcm_apply_to_node(bridge, graph, node_id, activity);
        modifications += bcm_updates;
    }

    /* Update tier activity tracking */
    nimcp_mutex_lock(bridge->base.mutex);
    pr_bcm_node_state_t* node = find_bcm_node(bridge, node_id);
    if (node) {
        bridge->stats.events_per_tier[tier]++;
        bridge->stats.avg_activity_per_tier[tier] =
            (bridge->stats.avg_activity_per_tier[tier] * 0.99f) + (activity * 0.01f);
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return modifications;
}

//=============================================================================
// Event Tracking Functions
//=============================================================================

int pr_plasticity_log_event(
    pr_plasticity_bridge_t bridge,
    const pr_plasticity_event_t* event)
{
    if (!bridge || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_plasticity_log_event: required parameter is NULL (bridge, event)");
        return -1;
    }
    if (!bridge->config.enable_event_logging) return 0;

    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_plasticity_log_ev", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, event, sizeof(*event));

    nimcp_mutex_lock(bridge->base.mutex);
    add_event(bridge, event);
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pr_plasticity_get_events(
    pr_plasticity_bridge_t bridge,
    pr_plasticity_event_t* events,
    uint32_t max_events,
    uint32_t* event_count)
{
    if (!bridge || !events || !event_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_plasticity_get_events: required parameter is NULL (bridge, events, event_count)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_plasticity_get_ev", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, event_count, sizeof(*event_count));

    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t to_copy = (max_events < bridge->event_count) ? max_events : bridge->event_count;

    /* Copy from circular buffer */
    if (to_copy > 0) {
        uint32_t start_idx = 0;
        if (bridge->event_count < bridge->event_capacity) {
            start_idx = 0;
        } else {
            start_idx = bridge->event_write_idx;
        }

        for (uint32_t i = 0; i < to_copy; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && to_copy > 256) {
                pr_plasticity_bridge_heartbeat("pr_plasticit_loop",
                                 (float)(i + 1) / (float)to_copy);
            }

            uint32_t idx = (start_idx + i) % bridge->event_capacity;
            events[i] = bridge->events[idx];
        }
    }

    *event_count = to_copy;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pr_plasticity_clear_events(pr_plasticity_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_plasticity_clear_events: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_plasticity_clear_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->event_count = 0;
    bridge->event_write_idx = 0;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pr_plasticity_get_stats(
    pr_plasticity_bridge_t bridge,
    pr_plasticity_bridge_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_plasticity_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_plasticity_get_st", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;

    /* Compute averages */
    uint64_t total_events = stats->stdp_ltp_events + stats->stdp_ltd_events;
    if (total_events > 0) {
        stats->avg_weight_change = stats->total_weight_change / (float)total_events;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Integration Functions
//=============================================================================

int pr_plasticity_sync_with_coordinator(pr_plasticity_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_plasticity_sync_with_coordinator: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_coordinator_sync) return 0;

    /* Would sync with plasticity_coordinator here */
    /* For now, just return success */

    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_plasticity_sync_w", 0.0f);


    return 0;
}

int pr_plasticity_connect_bio_async(pr_plasticity_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_plasticity_connect_bio_async: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) return 0;

    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_plasticity_connec", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pr_plasticity_disconnect_bio_async(pr_plasticity_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_plasticity_disconnect_bio_async: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_plasticity_discon", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool pr_plasticity_is_bio_async_connected(pr_plasticity_bridge_t bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_plasticity_is_bio", 0.0f);


    return bridge->bio_async_connected;
}

//=============================================================================
// Main Update Function
//=============================================================================

int pr_plasticity_bridge_update(
    pr_plasticity_bridge_t bridge,
    entangle_graph_t graph,
    float dt_ms)
{
    if (!bridge || !graph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_plasticity_bridge_update: required parameter is NULL (bridge, graph)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_update", 0.0f);


    uint64_t start_time_us = nimcp_time_get_us();

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update all BCM nodes */
    if (bridge->config.enable_bcm) {
        for (uint32_t i = 0; i < bridge->bcm_node_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->bcm_node_count > 256) {
                pr_plasticity_bridge_heartbeat("pr_plasticit_loop",
                                 (float)(i + 1) / (float)bridge->bcm_node_count);
            }

            pr_bcm_node_state_t* node = &bridge->bcm_nodes[i];

            /* Decay activity averages */
            float decay = 1.0f - dt_ms / (bridge->config.bcm.theta_tau * 3600.0f * 1000.0f);
            decay = nimcp_myelin_clamp(decay, 0.0f, 1.0f);

            node->activity_avg *= decay;
            node->activity_squared_avg *= decay;
        }
    }

    /* Check for structural remodeling */
    if (bridge->config.enable_structural) {
        uint64_t now_ms = nimcp_time_get_ms();
        float elapsed = (float)(now_ms - bridge->last_remodel_time_ms);

        if (elapsed >= bridge->config.structural.remodel_interval_ms) {
            bridge->last_remodel_time_ms = now_ms;
            /* Note: Structural remodel would need node list from Z-Ladder */
        }
    }

    /* Update statistics */
    bridge->stats.total_updates++;

    uint64_t end_time_us = nimcp_time_get_us();
    float update_time_us = (float)(end_time_us - start_time_us);
    bridge->stats.avg_update_time_us =
        (bridge->stats.avg_update_time_us * 0.99f) + (update_time_us * 0.01f);

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Sync with coordinator if enabled */
    if (bridge->config.enable_coordinator_sync) {
        pr_plasticity_sync_with_coordinator(bridge);
    }

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* pr_plasticity_type_name(pr_plasticity_type_t type) {
    switch (type) {
        case PR_PLASTICITY_STDP:          return "STDP";
        case PR_PLASTICITY_BCM:           return "BCM";
        case PR_PLASTICITY_HOMEOSTATIC:   return "Homeostatic";
        case PR_PLASTICITY_METAPLASTICITY: return "Metaplasticity";
        case PR_PLASTICITY_STRUCTURAL:    return "Structural";
        default:                          return "Unknown";
    }
}

const char* pr_tier_name(pr_memory_tier_t tier) {
    switch (tier) {
        case PR_TIER_Z0: return "Z0 (Working)";
        case PR_TIER_Z1: return "Z1 (Short-term)";
        case PR_TIER_Z2: return "Z2 (Long-term)";
        case PR_TIER_Z3: return "Z3 (Deep Storage)";
        default:         return "Unknown";
    }
}

void pr_plasticity_event_print(const pr_plasticity_event_t* event) {
    if (!event) return;

    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_plasticity_event_", 0.0f);


    printf("Plasticity Event:\n");
    printf("  Type: %s\n", pr_plasticity_type_name(event->type));
    printf("  Edge: %lu -> %lu\n", (unsigned long)event->from_node,
           (unsigned long)event->to_node);
    printf("  Weight: %.4f -> %.4f (delta: %.4f)\n",
           event->pre_weight, event->post_weight, event->delta_weight);
    printf("  Resonance: %.4f\n", event->resonance_at_event);
    printf("  Timestamp: %lu ms\n", (unsigned long)event->timestamp_ms);
}

void pr_plasticity_print_stats(pr_plasticity_bridge_t bridge) {
    if (!bridge) return;

    /* Phase 8: Heartbeat at operation start */
    pr_plasticity_bridge_heartbeat("pr_plasticit_pr_plasticity_print_", 0.0f);


    pr_plasticity_bridge_stats_t stats;
    if (pr_plasticity_get_stats(bridge, &stats) != 0) return;

    printf("=== Prime Resonant Plasticity Bridge Statistics ===\n");
    printf("\nEvent Counts:\n");
    printf("  STDP LTP events:     %lu\n", (unsigned long)stats.stdp_ltp_events);
    printf("  STDP LTD events:     %lu\n", (unsigned long)stats.stdp_ltd_events);
    printf("  BCM updates:         %lu\n", (unsigned long)stats.bcm_updates);
    printf("  Homeostatic scalings: %lu\n", (unsigned long)stats.homeostatic_scalings);
    printf("  Metaplasticity events: %lu\n", (unsigned long)stats.metaplasticity_events);
    printf("  Edges created:       %lu\n", (unsigned long)stats.edges_created);
    printf("  Edges pruned:        %lu\n", (unsigned long)stats.edges_pruned);

    printf("\nWeight Statistics:\n");
    printf("  Total weight change: %.4f\n", stats.total_weight_change);
    printf("  Avg weight change:   %.4f\n", stats.avg_weight_change);
    printf("  Max weight change:   %.4f\n", stats.max_weight_change);

    printf("\nModulation:\n");
    printf("  Avg resonance mod:   %.4f\n", stats.avg_resonance_modulation);
    printf("  Avg consolidation:   %.4f\n", stats.avg_consolidation_gate);
    printf("  Blocked by consol:   %lu\n", (unsigned long)stats.blocked_by_consolidation);

    printf("\nPer-Tier Events:\n");
    for (int t = 0; t < PR_PLASTICITY_NUM_TIERS; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && PR_PLASTICITY_NUM_TIERS > 256) {
            pr_plasticity_bridge_heartbeat("pr_plasticit_loop",
                             (float)(t + 1) / (float)PR_PLASTICITY_NUM_TIERS);
        }

        printf("  %s: %lu events, avg activity %.4f\n",
               pr_tier_name((pr_memory_tier_t)t),
               (unsigned long)stats.events_per_tier[t],
               stats.avg_activity_per_tier[t]);
    }

    printf("\nPerformance:\n");
    printf("  Total updates:       %lu\n", (unsigned long)stats.total_updates);
    printf("  Avg update time:     %.2f us\n", stats.avg_update_time_us);
    printf("=================================================\n");
}

//=============================================================================
// Instance Health Agent Setter (B25 Upgrade)
//=============================================================================

void pr_plasticity_bridge_set_instance_health_agent(
    pr_plasticity_bridge_t bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B25 Upgrade)
//=============================================================================

int pr_plasticity_bridge_training_begin(pr_plasticity_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_plasticity_bridge_training_begin: NULL argument");
        return -1;
    }
    pr_plasticity_bridge_heartbeat_instance(bridge->health_agent, "pr_plasticity_bridge_training_begin", 0.0f);
    return 0;
}

int pr_plasticity_bridge_training_end(pr_plasticity_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_plasticity_bridge_training_end: NULL argument");
        return -1;
    }
    pr_plasticity_bridge_heartbeat_instance(bridge->health_agent, "pr_plasticity_bridge_training_end", 1.0f);
    return 0;
}

int pr_plasticity_bridge_training_step(pr_plasticity_bridge_t bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_plasticity_bridge_training_step: NULL argument");
        return -1;
    }
    pr_plasticity_bridge_heartbeat_instance(bridge->health_agent, "pr_plasticity_bridge_training_step", progress);
    return 0;
}
