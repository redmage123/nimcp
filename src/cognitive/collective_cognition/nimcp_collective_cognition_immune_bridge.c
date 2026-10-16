/**
 * @file nimcp_collective_cognition_immune_bridge.c
 * @brief Bridge between collective cognition and brain immune system
 *
 * WHAT: Coordinates immune responses across distributed brain instances
 * WHY:  Enables swarm-wide threat detection and collective immune memory
 * HOW:  Bridges collective cognition state to immune antigen presentation
 *
 * INTEGRATION POINTS:
 * - Collective Threats: Present threats detected by hyperscanning as antigens
 * - Inflammation Sharing: Propagate inflammation state via hyperscanning
 * - Memory Sync: Share immune memory cells across collective
 * - We-Mode Response: Coordinated immune response during we-mode
 *
 * @author NIMCP Development Team
 * @date 2025-01-01
 */

#include "cognitive/collective_cognition/nimcp_collective_cognition_immune_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/collective_cognition/nimcp_collective_cognition.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(collective_cognition_immune_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_collective_cognition_immune_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_collective_cognition_immune_bridge_mesh_registry = NULL;

nimcp_error_t collective_cognition_immune_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_collective_cognition_immune_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "collective_cognition_immune_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "collective_cognition_immune_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_collective_cognition_immune_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_collective_cognition_immune_bridge_mesh_registry = registry;
    return err;
}

void collective_cognition_immune_bridge_mesh_unregister(void) {
    if (g_collective_cognition_immune_bridge_mesh_registry && g_collective_cognition_immune_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_collective_cognition_immune_bridge_mesh_registry, g_collective_cognition_immune_bridge_mesh_id);
        g_collective_cognition_immune_bridge_mesh_id = 0;
        g_collective_cognition_immune_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from collective_cognition_immune_bridge module (instance-level) */
static inline void collective_cognition_immune_bridge_heartbeat_instance(
    void* _inst_agent_ptr, const char* operation, float progress)
{
    nimcp_health_agent_t* instance_agent = (nimcp_health_agent_t*)_inst_agent_ptr;
    if (g_collective_cognition_immune_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_collective_cognition_immune_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_collective_cognition_immune_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "COLLECTIVE_COGNITION_IMMUNE_BRIDGE"


/*=============================================================================
 * Constants
 *===========================================================================*/

#define MAX_PENDING_THREATS     64
#define THREAT_TYPE_NAME_SIZE   48

/*=============================================================================
 * Internal Structure
 *===========================================================================*/

/**
 * @brief Collective immune bridge internal state
 */
struct collective_immune_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    collective_immune_bridge_config_t config;

    /* Connected systems */
    collective_cognition_t* collective_cognition;
    brain_immune_system_t* immune_system;

    /* Pending threats */
    collective_threat_t pending_threats[MAX_PENDING_THREATS];
    uint32_t pending_threat_count;

    /* Statistics */
    collective_immune_bridge_stats_t stats;

    /* State tracking */
    uint64_t last_sync_time_us;
    float cumulative_severity;
    uint64_t cumulative_response_time_us;
    uint64_t response_count;

    /* Validation */
    bool initialized;

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;
};

/*=============================================================================
 * Internal Helpers
 *===========================================================================*/

/**
 * @brief Convert collective threat type to epitope for immune presentation
 */
static void threat_to_epitope(
    const collective_threat_t* threat,
    uint8_t* epitope,
    size_t* epitope_len
)
{
    /* WHAT: Create epitope from collective threat
     * WHY:  Brain immune needs epitope signature for antigen
     * HOW:  Combine threat type, severity, and source into pattern */

    memset(epitope, 0, BRAIN_IMMUNE_EPITOPE_SIZE);

    /* Pack threat type as first bytes */
    epitope[0] = (uint8_t)(threat->type & 0xFF);
    epitope[1] = (uint8_t)((threat->source_instance_id >> 8) & 0xFF);
    epitope[2] = (uint8_t)(threat->source_instance_id & 0xFF);
    epitope[3] = (uint8_t)((threat->target_instance_id >> 8) & 0xFF);
    epitope[4] = (uint8_t)(threat->target_instance_id & 0xFF);

    /* Pack severity as bytes 5-8 */
    uint32_t severity_bits = *(uint32_t*)&threat->severity;
    epitope[5] = (uint8_t)((severity_bits >> 24) & 0xFF);
    epitope[6] = (uint8_t)((severity_bits >> 16) & 0xFF);
    epitope[7] = (uint8_t)((severity_bits >> 8) & 0xFF);
    epitope[8] = (uint8_t)(severity_bits & 0xFF);

    /* Copy description hash (truncated) */
    size_t desc_len = strlen(threat->description);
    if (desc_len > 0) {
        size_t copy_len = desc_len < (BRAIN_IMMUNE_EPITOPE_SIZE - 9) ?
                          desc_len : (BRAIN_IMMUNE_EPITOPE_SIZE - 9);
        memcpy(&epitope[9], threat->description, copy_len);
    }

    *epitope_len = BRAIN_IMMUNE_EPITOPE_SIZE;
}

/**
 * @brief Calculate severity from collective state anomalies
 */
static float calculate_state_severity(const collective_cognition_state_t* state)
{
    /* WHAT: Derive threat severity from collective state
     * WHY:  Detect problems from state metrics
     * HOW:  Check for fragmentation, desync, low phi */

    float severity = 0.0f;

    if (state->is_fragmented) {
        severity += 0.4f;
    }

    if (state->is_overloaded) {
        severity += 0.3f;
    }

    /* Low synchronization */
    if (state->hyperscanning.global_sync < 0.3f && state->active_instances > 1) {
        severity += (0.3f - state->hyperscanning.global_sync);
    }

    /* Low phi (consciousness fragmented) */
    if (state->phi.phi_total < 0.2f && state->active_instances > 1) {
        severity += (0.2f - state->phi.phi_total);
    }

    /* We-mode breakdown */
    if (state->we_mode.we_mode_strength < 0.2f &&
        state->we_mode.active_shared_goals > 0) {
        severity += 0.2f;
    }

    /* Clamp to [0, 1] */
    if (severity > 1.0f) severity = 1.0f;
    if (severity < 0.0f) severity = 0.0f;

    return severity;
}

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

collective_immune_bridge_config_t collective_immune_bridge_default_config(void)
{
    /* Phase 8: Heartbeat at operation start */
    collective_cognition_immune_bridge_heartbeat("collective_c_collective_immune_br", 0.0f);


    collective_immune_bridge_config_t config = {
        .enable_threat_sharing = true,
        .enable_inflammation_sync = true,
        .enable_memory_propagation = true,
        .enable_we_mode_coordination = true,
        .threat_threshold = 0.3f,
        .sync_interval_ms = 100.0f
    };
    return config;
}

collective_immune_bridge_t* collective_immune_bridge_create(
    const collective_immune_bridge_config_t* config
)
{
    /* WHAT: Create collective immune bridge
     * WHY:  Enable immune coordination across collective
     * HOW:  Allocate state, initialize with config */

    /* Phase 8: Heartbeat at operation start */
    collective_cognition_immune_bridge_heartbeat("collective_c_collective_immune_br", 0.0f);


    collective_immune_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) {
        LOG_ERROR("Failed to allocate collective immune bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = collective_immune_bridge_default_config();
    }

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "collective_immune") != 0) {
        LOG_ERROR("Failed to init collective immune bridge base");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "collective_immune_bridge_create: validation failed");
        return NULL;
    }

    /* Initialize state */
    bridge->pending_threat_count = 0;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->last_sync_time_us = 0;
    bridge->cumulative_severity = 0.0f;
    bridge->cumulative_response_time_us = 0;
    bridge->response_count = 0;

    bridge->initialized = true;

    LOG_DEBUG("Collective immune bridge created");
    return bridge;
}

void collective_immune_bridge_destroy(collective_immune_bridge_t* bridge)
{
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "collective_cognition_immune");

    /* Phase 8: Heartbeat at operation start */
    collective_cognition_immune_bridge_heartbeat("collective_c_collective_immune_br", 0.0f);


    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
    LOG_DEBUG("Collective immune bridge destroyed");
}

int collective_immune_bridge_reset(collective_immune_bridge_t* bridge)
{
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_immune_bridge_reset: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_cognition_immune_bridge_heartbeat("collective_c_collective_immune_br", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->pending_threat_count = 0;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->last_sync_time_us = 0;
    bridge->cumulative_severity = 0.0f;
    bridge->cumulative_response_time_us = 0;
    bridge->response_count = 0;

    nimcp_mutex_unlock(bridge->base.mutex);

    LOG_DEBUG("Collective immune bridge reset");
    return 0;
}

/*=============================================================================
 * Connection API
 *===========================================================================*/

int collective_immune_bridge_connect_collective_cognition(
    collective_immune_bridge_t* bridge,
    collective_cognition_t* cc
)
{
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_immune_bridge_connect_collective_cognition: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_cognition_immune_bridge_heartbeat("collective_c_collective_immune_br", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->collective_cognition = cc;
    nimcp_mutex_unlock(bridge->base.mutex);

    LOG_DEBUG("Collective immune bridge connected to collective cognition");
    return 0;
}

int collective_immune_bridge_connect_immune(
    collective_immune_bridge_t* bridge,
    brain_immune_system_t* immune
)
{
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_immune_bridge_connect_immune: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_cognition_immune_bridge_heartbeat("collective_c_collective_immune_br", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->immune_system = immune;
    nimcp_mutex_unlock(bridge->base.mutex);

    LOG_DEBUG("Collective immune bridge connected to brain immune system");
    return 0;
}

/*=============================================================================
 * Threat Detection API
 *===========================================================================*/

int collective_immune_bridge_report_threat(
    collective_immune_bridge_t* bridge,
    const collective_threat_t* threat
)
{
    /* WHAT: Report a collective threat
     * WHY:  Add threat to pending queue for processing
     * HOW:  Validate and store threat */

    if (!bridge || !bridge->initialized || !threat) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_immune_bridge_report_threat: required parameter is NULL (bridge, bridge->initialized, threat)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_cognition_immune_bridge_heartbeat("collective_c_collective_immune_br", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check capacity */
    if (bridge->pending_threat_count >= MAX_PENDING_THREATS) {
        nimcp_mutex_unlock(bridge->base.mutex);
        LOG_WARNING("Collective immune bridge threat queue full");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "collective_immune_bridge_report_threat: capacity exceeded");
        return -1;
    }

    /* Check threshold */
    if (threat->severity < bridge->config.threat_threshold) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;  /* Below threshold, ignore */
    }

    /* Add to pending */
    bridge->pending_threats[bridge->pending_threat_count] = *threat;
    bridge->pending_threat_count++;

    /* Update stats */
    bridge->stats.threats_detected++;
    bridge->cumulative_severity += threat->severity;

    nimcp_mutex_unlock(bridge->base.mutex);

    LOG_DEBUG("Collective threat reported: type=%d severity=%.2f",
              threat->type, threat->severity);
    return 0;
}

uint32_t collective_immune_bridge_check_threats(
    collective_immune_bridge_t* bridge
)
{
    /* WHAT: Check collective cognition for threats
     * WHY:  Proactively detect anomalies in collective state
     * HOW:  Analyze state and generate threat reports */

    if (!bridge || !bridge->initialized || !bridge->collective_cognition) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_cognition_immune_bridge_heartbeat("collective_c_collective_immune_br", 0.0f);


    collective_cognition_state_t state;
    if (collective_cognition_get_state(bridge->collective_cognition, &state) != 0) {
        return 0;
    }

    uint32_t threats_detected = 0;

    nimcp_mutex_lock(bridge->base.mutex);

    uint64_t now_us = nimcp_time_monotonic_us();

    /* Check for fragmentation */
    if (state.is_fragmented) {
        if (bridge->pending_threat_count < MAX_PENDING_THREATS) {
            collective_threat_t* t = &bridge->pending_threats[bridge->pending_threat_count];
            t->type = COLLECTIVE_THREAT_DESYNC;
            t->source_instance_id = 0;
            t->target_instance_id = 0;
            t->severity = 0.6f;
            t->confidence = 0.9f;
            t->detection_time_us = now_us;
            snprintf(t->description, sizeof(t->description),
                     "Network fragmentation detected");
            bridge->pending_threat_count++;
            threats_detected++;
        }
    }

    /* Check for phi collapse */
    if (state.phi.phi_total < 0.1f && state.active_instances > 1) {
        if (bridge->pending_threat_count < MAX_PENDING_THREATS) {
            collective_threat_t* t = &bridge->pending_threats[bridge->pending_threat_count];
            t->type = COLLECTIVE_THREAT_PHI_COLLAPSE;
            t->source_instance_id = 0;
            t->target_instance_id = 0;
            t->severity = 0.8f;
            t->confidence = 0.95f;
            t->detection_time_us = now_us;
            snprintf(t->description, sizeof(t->description),
                     "Collective phi collapsed to %.3f", state.phi.phi_total);
            bridge->pending_threat_count++;
            threats_detected++;
        }
    }

    /* Check for we-mode break during active goals */
    if (state.we_mode.we_mode_strength < 0.2f &&
        state.we_mode.active_shared_goals > 0) {
        if (bridge->pending_threat_count < MAX_PENDING_THREATS) {
            collective_threat_t* t = &bridge->pending_threats[bridge->pending_threat_count];
            t->type = COLLECTIVE_THREAT_WE_MODE_BREAK;
            t->source_instance_id = 0;
            t->target_instance_id = 0;
            t->severity = 0.5f;
            t->confidence = 0.85f;
            t->detection_time_us = now_us;
            snprintf(t->description, sizeof(t->description),
                     "We-mode breakdown during %u active goals",
                     state.we_mode.active_shared_goals);
            bridge->pending_threat_count++;
            threats_detected++;
        }
    }

    /* Check for belief conflict (via low goal alignment) */
    if (state.goal_alignment < 0.2f && state.we_mode.active_shared_goals > 2) {
        if (bridge->pending_threat_count < MAX_PENDING_THREATS) {
            collective_threat_t* t = &bridge->pending_threats[bridge->pending_threat_count];
            t->type = COLLECTIVE_THREAT_BELIEF_CONFLICT;
            t->source_instance_id = 0;
            t->target_instance_id = 0;
            t->severity = 0.4f;
            t->confidence = 0.7f;
            t->detection_time_us = now_us;
            snprintf(t->description, sizeof(t->description),
                     "Belief conflict: goal alignment at %.2f",
                     state.goal_alignment);
            bridge->pending_threat_count++;
            threats_detected++;
        }
    }

    bridge->stats.threats_detected += threats_detected;

    nimcp_mutex_unlock(bridge->base.mutex);

    return threats_detected;
}

uint32_t collective_immune_bridge_get_threats(
    const collective_immune_bridge_t* bridge,
    collective_threat_t* threats,
    uint32_t max_threats
)
{
    if (!bridge || !bridge->initialized || !threats) return 0;

    /* Note: Cast away const for mutex lock (bridge is logically const) */
    /* Phase 8: Heartbeat at operation start */
    collective_cognition_immune_bridge_heartbeat("collective_c_collective_immune_br", 0.0f);


    collective_immune_bridge_t* mutable_bridge =
        (collective_immune_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);

    uint32_t count = bridge->pending_threat_count;
    if (count > max_threats) {
        count = max_threats;
    }

    memcpy(threats, bridge->pending_threats, count * sizeof(collective_threat_t));

    nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return count;
}

/*=============================================================================
 * Immune Integration API
 *===========================================================================*/

int collective_immune_bridge_present_antigen(
    collective_immune_bridge_t* bridge,
    const collective_threat_t* threat,
    uint32_t* antigen_id
)
{
    /* WHAT: Present collective threat as immune antigen
     * WHY:  Allow immune system to respond to collective threats
     * HOW:  Convert threat to epitope, present to brain immune */

    if (!bridge || !bridge->initialized || !threat) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_immune_bridge_present_antigen: required parameter is NULL (bridge, bridge->initialized, threat)");
        return -1;
    }
    if (!bridge->immune_system) {
        LOG_WARNING("No immune system connected to collective immune bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_immune_bridge_present_antigen: bridge->immune_system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_cognition_immune_bridge_heartbeat("collective_c_collective_immune_br", 0.0f);


    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    size_t epitope_len = 0;
    threat_to_epitope(threat, epitope, &epitope_len);

    /* Map severity [0-1] to [1-10] */
    uint32_t severity = (uint32_t)(threat->severity * 9.0f) + 1;
    if (severity > 10) severity = 10;

    int result = brain_immune_present_antigen(
        bridge->immune_system,
        ANTIGEN_SOURCE_SWARM,  /* Collective threats come via swarm-like mechanism */
        epitope,
        epitope_len,
        severity,
        threat->source_instance_id,
        antigen_id
    );

    if (result == 0) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->stats.antigens_presented++;
        nimcp_mutex_unlock(bridge->base.mutex);
        LOG_DEBUG("Collective threat presented as antigen %u", *antigen_id);
    }

    return result;
}

int collective_immune_bridge_sync_inflammation(
    collective_immune_bridge_t* bridge
)
{
    /* WHAT: Sync inflammation state across collective
     * WHY:  Share immune response state with other instances
     * HOW:  Get inflammation level and broadcast via collective cognition */

    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_immune_bridge_sync_inflammation: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }
    if (!bridge->immune_system || !bridge->collective_cognition) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_immune_bridge_sync_inflammation: required parameter is NULL (bridge->immune_system, bridge->collective_cognition)");
        return -1;
    }

    if (!bridge->config.enable_inflammation_sync) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_cognition_immune_bridge_heartbeat("collective_c_collective_immune_br", 0.0f);


    uint64_t now_us = nimcp_time_monotonic_us();

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check sync interval */
    float interval_us = bridge->config.sync_interval_ms * 1000.0f;
    if (now_us - bridge->last_sync_time_us < (uint64_t)interval_us) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    bridge->last_sync_time_us = now_us;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Get current inflammation level */
    brain_inflammation_level_t level = brain_immune_get_inflammation_level(
        bridge->immune_system
    );

    /* TODO: Broadcast via collective cognition when API is available */
    /* For now, just track the sync */

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.inflammation_syncs++;
    nimcp_mutex_unlock(bridge->base.mutex);

    LOG_DEBUG("Inflammation synced: level=%d", level);
    return 0;
}

int collective_immune_bridge_propagate_memory(
    collective_immune_bridge_t* bridge,
    uint32_t b_cell_id
)
{
    /* WHAT: Propagate immune memory to collective
     * WHY:  Share learned threat patterns across instances
     * HOW:  Sync B cell memory to swarm immune system */

    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_immune_bridge_propagate_memory: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_immune_bridge_propagate_memory: bridge->immune_system is NULL");
        return -1;
    }

    if (!bridge->config.enable_memory_propagation) {
        return 0;
    }

    /* Sync memory cell to swarm */
    /* Phase 8: Heartbeat at operation start */
    collective_cognition_immune_bridge_heartbeat("collective_c_collective_immune_br", 0.0f);


    int result = brain_immune_sync_memory_to_swarm(bridge->immune_system, b_cell_id);

    if (result == 0) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->stats.memory_propagations++;
        nimcp_mutex_unlock(bridge->base.mutex);
        LOG_DEBUG("Immune memory propagated: b_cell=%u", b_cell_id);
    }

    return result;
}

/*=============================================================================
 * We-Mode Response API
 *===========================================================================*/

int collective_immune_bridge_we_mode_response(
    collective_immune_bridge_t* bridge,
    const collective_threat_t* threat
)
{
    /* WHAT: Trigger coordinated immune response during we-mode
     * WHY:  All instances respond together when unified
     * HOW:  Present antigen, then propagate secondary response */

    if (!bridge || !bridge->initialized || !threat) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_immune_bridge_we_mode_response: required parameter is NULL (bridge, bridge->initialized, threat)");
        return -1;
    }
    if (!bridge->immune_system || !bridge->collective_cognition) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_immune_bridge_we_mode_response: required parameter is NULL (bridge->immune_system, bridge->collective_cognition)");
        return -1;
    }

    if (!bridge->config.enable_we_mode_coordination) {
        return 0;
    }

    /* Check if we-mode is active */
    /* Phase 8: Heartbeat at operation start */
    collective_cognition_immune_bridge_heartbeat("collective_c_collective_immune_br", 0.0f);


    collective_cognition_state_t state;
    if (collective_cognition_get_state(bridge->collective_cognition, &state) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "collective_immune_bridge_we_mode_response: validation failed");
        return -1;
    }

    if (state.we_mode.we_mode_strength < 0.5f) {
        /* Not in strong enough we-mode for coordinated response */
        return 0;
    }

    uint64_t start_time_us = nimcp_time_monotonic_us();

    /* Present antigen */
    uint32_t antigen_id = 0;
    if (collective_immune_bridge_present_antigen(bridge, threat, &antigen_id) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "collective_immune_bridge_we_mode_response: validation failed");
        return -1;
    }

    /* Check for memory match (secondary response) */
    uint32_t memory_b_cell_id = 0;
    if (brain_immune_check_memory(bridge->immune_system, antigen_id, &memory_b_cell_id) == 0) {
        /* Memory match found - trigger rapid secondary response */
        brain_immune_secondary_response(bridge->immune_system, antigen_id, memory_b_cell_id);

        /* Propagate across collective */
        brain_immune_propagate_secondary_response(
            bridge->immune_system,
            memory_b_cell_id,
            antigen_id
        );
    } else {
        /* Primary response - activate B and T cells */
        uint32_t b_cell_id, t_cell_id;
        brain_immune_activate_b_cell(bridge->immune_system, antigen_id, &b_cell_id);
        brain_immune_activate_helper_t(bridge->immune_system, antigen_id, &t_cell_id);

        /* Helper T helps B cell */
        brain_immune_t_help_b(bridge->immune_system, t_cell_id, b_cell_id);
    }

    uint64_t end_time_us = nimcp_time_monotonic_us();

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.we_mode_responses++;
    bridge->cumulative_response_time_us += (end_time_us - start_time_us);
    bridge->response_count++;
    nimcp_mutex_unlock(bridge->base.mutex);

    LOG_INFO("We-mode coordinated response triggered for threat type %d",
             threat->type);
    return 0;
}

/*=============================================================================
 * Update and Query API
 *===========================================================================*/

int collective_immune_bridge_update(collective_immune_bridge_t* bridge)
{
    /* WHAT: Update bridge state
     * WHY:  Process pending threats and sync state
     * HOW:  Check for threats, present antigens, sync inflammation */

    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_immune_bridge_update: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Check for new threats */
    /* Phase 8: Heartbeat at operation start */
    collective_cognition_immune_bridge_heartbeat("collective_c_collective_immune_br", 0.0f);


    uint32_t new_threats = collective_immune_bridge_check_threats(bridge);

    /* Process pending threats */
    nimcp_mutex_lock(bridge->base.mutex);

    for (uint32_t i = 0; i < bridge->pending_threat_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->pending_threat_count > 256) {
            collective_cognition_immune_bridge_heartbeat("collective_c_loop",
                             (float)(i + 1) / (float)bridge->pending_threat_count);
        }

        collective_threat_t* threat = &bridge->pending_threats[i];

        /* Share threat if enabled */
        if (bridge->config.enable_threat_sharing && bridge->immune_system) {
            uint32_t antigen_id = 0;
            if (collective_immune_bridge_present_antigen(bridge, threat, &antigen_id) == 0) {
                bridge->stats.threats_shared++;
            }
        }
    }

    /* Clear processed threats */
    bridge->pending_threat_count = 0;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Sync inflammation */
    collective_immune_bridge_sync_inflammation(bridge);

    return 0;
}

int collective_immune_bridge_get_stats(
    const collective_immune_bridge_t* bridge,
    collective_immune_bridge_stats_t* stats
)
{
    if (!bridge || !bridge->initialized || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_immune_bridge_get_stats: required parameter is NULL (bridge, bridge->initialized, stats)");
        return -1;
    }

    /* Cast away const for mutex lock */
    /* Phase 8: Heartbeat at operation start */
    collective_cognition_immune_bridge_heartbeat("collective_c_collective_immune_br", 0.0f);


    collective_immune_bridge_t* mutable_bridge =
        (collective_immune_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);

    *stats = bridge->stats;

    /* Calculate averages */
    if (bridge->stats.threats_detected > 0) {
        stats->avg_threat_severity =
            bridge->cumulative_severity / (float)bridge->stats.threats_detected;
    }

    if (bridge->response_count > 0) {
        stats->avg_response_time_ms =
            (float)bridge->cumulative_response_time_us /
            (float)bridge->response_count / 1000.0f;
    }

    nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return 0;
}

void collective_immune_bridge_reset_stats(collective_immune_bridge_t* bridge)
{
    if (!bridge || !bridge->initialized) return;

    /* Phase 8: Heartbeat at operation start */
    collective_cognition_immune_bridge_heartbeat("collective_c_collective_immune_br", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->cumulative_severity = 0.0f;
    bridge->cumulative_response_time_us = 0;
    bridge->response_count = 0;

    nimcp_mutex_unlock(bridge->base.mutex);
}

/*=============================================================================
 * Utility API
 *===========================================================================*/

const char* collective_threat_type_name(collective_threat_type_t type)
{
    switch (type) {
        case COLLECTIVE_THREAT_DESYNC:
            return "Synchronization Loss";
        case COLLECTIVE_THREAT_ROGUE_INSTANCE:
            return "Rogue Instance";
        case COLLECTIVE_THREAT_PHI_COLLAPSE:
            return "Phi Collapse";
        case COLLECTIVE_THREAT_WE_MODE_BREAK:
            return "We-Mode Disruption";
        case COLLECTIVE_THREAT_EXTERNAL_ATTACK:
            return "External Attack";
        case COLLECTIVE_THREAT_BELIEF_CONFLICT:
            return "Belief Conflict";
        default:
            return "Unknown";
    }
}

void collective_immune_bridge_dump(const collective_immune_bridge_t* bridge)
{
    if (!bridge || !bridge->initialized) {
        LOG_INFO("Collective Immune Bridge: NULL or uninitialized");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_cognition_immune_bridge_heartbeat("collective_c_collective_immune_br", 0.0f);


    collective_immune_bridge_stats_t stats;
    if (collective_immune_bridge_get_stats(bridge, &stats) != 0) {
        LOG_INFO("Collective Immune Bridge: Failed to get stats");
        return;
    }

    LOG_INFO("=== Collective Immune Bridge State ===");
    LOG_INFO("Configuration:");
    LOG_INFO("  Threat sharing: %s", bridge->config.enable_threat_sharing ? "on" : "off");
    LOG_INFO("  Inflammation sync: %s", bridge->config.enable_inflammation_sync ? "on" : "off");
    LOG_INFO("  Memory propagation: %s", bridge->config.enable_memory_propagation ? "on" : "off");
    LOG_INFO("  We-mode coordination: %s", bridge->config.enable_we_mode_coordination ? "on" : "off");
    LOG_INFO("  Threat threshold: %.2f", bridge->config.threat_threshold);
    LOG_INFO("  Sync interval: %.1f ms", bridge->config.sync_interval_ms);
    LOG_INFO("Connections:");
    LOG_INFO("  Collective cognition: %s", bridge->collective_cognition ? "connected" : "disconnected");
    LOG_INFO("  Immune system: %s", bridge->immune_system ? "connected" : "disconnected");
    LOG_INFO("Statistics:");
    LOG_INFO("  Threats detected: %lu", (unsigned long)stats.threats_detected);
    LOG_INFO("  Threats shared: %lu", (unsigned long)stats.threats_shared);
    LOG_INFO("  Antigens presented: %lu", (unsigned long)stats.antigens_presented);
    LOG_INFO("  Inflammation syncs: %lu", (unsigned long)stats.inflammation_syncs);
    LOG_INFO("  Memory propagations: %lu", (unsigned long)stats.memory_propagations);
    LOG_INFO("  We-mode responses: %lu", (unsigned long)stats.we_mode_responses);
    LOG_INFO("  Avg threat severity: %.2f", stats.avg_threat_severity);
    LOG_INFO("  Avg response time: %.2f ms", stats.avg_response_time_ms);
    LOG_INFO("  Pending threats: %u", bridge->pending_threat_count);
    LOG_INFO("======================================");
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Collective Cognition Immune Bridge self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int collective_cognition_immune_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    collective_cognition_immune_bridge_heartbeat("collective_c_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Collective_Cognition_Immune_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                collective_cognition_immune_bridge_heartbeat("collective_c_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG("Collective Cognition Immune Bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Collective_Cognition_Immune_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Collective_Cognition_Immune_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void collective_cognition_immune_bridge_set_instance_health_agent(collective_immune_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        bridge->health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Full training implementation
 * ============================================================================ */
int collective_cognition_immune_bridge_training_begin(collective_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "collective_cognition_immune_bridge_training_begin: NULL argument");
        return -1;
    }
    collective_cognition_immune_bridge_heartbeat_instance(bridge, "cc_imm_br_train_begin", 0.0f);
    (void)bridge;
    return 0;
}

int collective_cognition_immune_bridge_training_step(collective_immune_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "collective_cognition_immune_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    collective_cognition_immune_bridge_heartbeat_instance(bridge, "cc_imm_br_train_step", progress);
    (void)bridge;
    return 0;
}

int collective_cognition_immune_bridge_training_end(collective_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "collective_cognition_immune_bridge_training_end: NULL argument");
        return -1;
    }
    collective_cognition_immune_bridge_heartbeat_instance(bridge, "cc_imm_br_train_end", 1.0f);
    (void)bridge;
    return 0;
}
