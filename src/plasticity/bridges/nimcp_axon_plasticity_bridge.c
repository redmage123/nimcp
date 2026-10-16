/**
 * @file nimcp_axon_plasticity_bridge.c
 * @brief Axon-Plasticity Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Connects axons to structural plasticity, intrinsic excitability, and myelination
 * WHY:  Axons need adaptive conduction properties
 * HOW:  Routes activity and metabolic signals between axons and plasticity
 *
 * @author NIMCP Development Team
 */

#include "plasticity/bridges/nimcp_axon_plasticity_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/validation/nimcp_common.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_learning_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(axon_plasticity_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(axon_plasticity_bridge)

/* ============================================================================
 * Static Helpers
 * ============================================================================ */

static axon_segment_state_t* find_or_create_segment(
    axon_plasticity_bridge_t* bridge,
    uint32_t segment_id)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }

    for (size_t i = 0; i < bridge->num_segments; i++) {
        if (bridge->segments[i].segment_id == segment_id) {
            return &bridge->segments[i];
        }
    }

    if (bridge->num_segments >= bridge->segment_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "find_or_create_segment: capacity exceeded");
        return NULL;
    }

    axon_segment_state_t* seg = &bridge->segments[bridge->num_segments];
    memset(seg, 0, sizeof(*seg));
    seg->segment_id = segment_id;
    seg->myelination_level = 0.0f;
    seg->conduction_velocity = bridge->config.base_conduction_velocity;
    seg->excitability = 0.5f;
    seg->conduction_state = CONDUCTION_NORMAL;
    bridge->num_segments++;

    return seg;
}

/* NOTE: Message handlers are registered separately via bio_router_register_handler() */

/* ============================================================================
 * KG-Driven Wiring Callback
 * ============================================================================ */

/**
 * @brief Wiring callback for KG-driven handler registration
 *
 * WHAT: Register message handlers based on discovered wiring from KG
 * WHY:  Enables runtime assembly - module discovers its handlers from KG
 * HOW:  Orchestrator invokes this with message types from HANDLES_MESSAGE relations
 */
static int axon_plasticity_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    (void)user_data;

    if (!ctx || !message_types || message_count == 0) {
        return 0;
    }

    int registered = 0;
    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            /* Add handlers for specific message types as needed */
            /* Currently no handlers defined - placeholder for future extension */
            default:
                NIMCP_LOGGING_DEBUG("Axon plasticity: unknown message type %d in wiring callback",
                                    message_types[i]);
                break;
        }
    }

    return (registered > 0) ? 0 : -1;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int axon_plasticity_default_config(axon_plasticity_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "axon_plasticity_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(*config));

    /* Conduction parameters */
    config->base_conduction_velocity = 1.0f;   /* m/s unmyelinated */
    config->max_conduction_velocity = 100.0f;  /* m/s fully myelinated */
    config->conduction_fatigue_rate = 0.001f;
    config->conduction_recovery_tau_ms = 100.0f;

    /* Myelination */
    config->activity_myelination_gain = 0.01f;
    config->myelination_velocity_gain = 50.0f;
    config->enable_adaptive_myelination = true;

    /* Intrinsic plasticity */
    config->excitability_adaptation_rate = 0.001f;
    config->excitability_min = 0.1f;
    config->excitability_max = 2.0f;
    config->enable_intrinsic_plasticity = true;

    /* Structural plasticity */
    config->branch_growth_threshold = 0.8f;
    config->branch_prune_threshold = 0.1f;
    config->enable_structural_plasticity = true;

    /* Bio-async */
    config->enable_bio_async = true;
    config->inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL;

    return 0;
}

axon_plasticity_bridge_t* axon_plasticity_create(
    const axon_plasticity_config_t* config,
    axon_t* axon,
    plasticity_orchestrator_t* orch)
{
    axon_plasticity_bridge_t* bridge = nimcp_malloc(sizeof(axon_plasticity_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate axon plasticity bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "axon_plasticity_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(*bridge));

    if (config) {
        bridge->config = *config;
    } else {
        axon_plasticity_default_config(&bridge->config);
    }

    bridge->axon = axon;
    bridge->plasticity_orch = orch;

    /* Allocate segment array */
    bridge->segment_capacity = AXON_PLASTICITY_MAX_SEGMENTS;
    bridge->segments = nimcp_malloc(
        bridge->segment_capacity * sizeof(axon_segment_state_t));
    if (!bridge->segments) {
        NIMCP_LOGGING_ERROR("Failed to allocate segment array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "axon_plasticity_create: failed to allocate segment array");
        goto cleanup;
    }
    memset(bridge->segments, 0,
           bridge->segment_capacity * sizeof(axon_segment_state_t));

    /* Initialize bridge base (allocates mutex, sets module name) */
    if (bridge_base_init(&bridge->base, 0, "axon_plasticity") != 0) {
        goto cleanup;
    }

    bridge->initialized = true;
    bridge->last_update_time = nimcp_time_get_us();
    bridge->avg_conduction_velocity = bridge->config.base_conduction_velocity;

    NIMCP_LOGGING_INFO("Created axon-plasticity bridge");
    return bridge;

cleanup:
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge->segments);
    nimcp_free(bridge);
    return NULL;
}

void axon_plasticity_destroy(axon_plasticity_bridge_t* bridge)
{
    if (!bridge) return;

    /* Cleanup bridge base (disconnects bio-async, destroys+frees mutex) */
    bridge_base_cleanup(&bridge->base);

    if (bridge->segments) {
        nimcp_free(bridge->segments);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed axon-plasticity bridge");
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int axon_plasticity_connect_structural(axon_plasticity_bridge_t* bridge, structural_state_t* s)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "axon_plasticity_connect_structural: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->structural = s;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int axon_plasticity_connect_intrinsic(axon_plasticity_bridge_t* bridge, intrinsic_excitability_state_t* i)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "axon_plasticity_connect_intrinsic: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->intrinsic = i;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int axon_plasticity_connect_metabolic(axon_plasticity_bridge_t* bridge, metabolic_state_t* m)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "axon_plasticity_connect_metabolic: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->metabolic = m;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int axon_plasticity_connect_myelin(axon_plasticity_bridge_t* bridge, nimcp_myelin_sheath_t* myelin)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "axon_plasticity_connect_myelin: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->myelin = myelin;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int axon_plasticity_connect_bio_async(axon_plasticity_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "axon_plasticity_connect_bio_async: bridge is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (bridge->base.bio_async_enabled) return NIMCP_SUCCESS;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_AXON,
        .module_name = AXON_PLASTICITY_MODULE_NAME,
        .inbox_capacity = bridge->config.inbox_capacity,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;

        /* Try KG-driven wiring callback registration first */
        nimcp_error_t wiring_result = bio_router_register_wiring_callback(
            BIO_MODULE_AXON,
            (void*)axon_plasticity_wiring_handler_callback,
            bridge
        );

        if (wiring_result == NIMCP_SUCCESS) {
            NIMCP_LOGGING_INFO("Axon plasticity: KG-driven wiring callback registered");
        } else {
            /* Legacy fallback - no specific handlers currently defined */
            LEGACY_HANDLER_REGISTRATION(
                /* No handlers registered currently - module uses direct function calls */
                (void)0
            );
            NIMCP_LOGGING_INFO("Axon plasticity: legacy handler registration (no handlers)");
        }
    }
    return NIMCP_SUCCESS;
}

int axon_plasticity_disconnect_bio_async(axon_plasticity_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "axon_plasticity_disconnect_bio_async: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool axon_plasticity_is_bio_async_connected(const axon_plasticity_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Conduction API
 * ============================================================================ */

int axon_plasticity_update_conduction(axon_plasticity_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "axon_plasticity_update_conduction: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    float sum_velocity = 0.0f;
    for (size_t i = 0; i < bridge->num_segments; i++) {
        axon_segment_state_t* seg = &bridge->segments[i];

        /* Compute velocity based on myelination */
        float myelin_factor = 1.0f + seg->myelination_level *
                              bridge->config.myelination_velocity_gain;
        float fatigue_factor = 1.0f - seg->fatigue_level;

        seg->conduction_velocity = bridge->config.base_conduction_velocity *
                                   myelin_factor * fatigue_factor;

        /* Clamp to max */
        if (seg->conduction_velocity > bridge->config.max_conduction_velocity) {
            seg->conduction_velocity = bridge->config.max_conduction_velocity;
        }

        /* Update conduction state */
        if (seg->fatigue_level > 0.9f) {
            seg->conduction_state = CONDUCTION_BLOCKED;
        } else if (seg->fatigue_level > 0.5f) {
            seg->conduction_state = CONDUCTION_SLOWED;
        } else if (seg->myelination_level > 0.5f) {
            seg->conduction_state = CONDUCTION_ENHANCED;
        } else {
            seg->conduction_state = CONDUCTION_NORMAL;
        }

        sum_velocity += seg->conduction_velocity;
    }

    if (bridge->num_segments > 0) {
        bridge->avg_conduction_velocity = sum_velocity / (float)bridge->num_segments;
        if (!isfinite(bridge->avg_conduction_velocity)) bridge->avg_conduction_velocity = 1.0f;
        bridge->stats.avg_conduction_velocity = bridge->avg_conduction_velocity;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float axon_plasticity_get_conduction_velocity(axon_plasticity_bridge_t* bridge)
{
    if (!bridge) return 1.0f;
    return bridge->avg_conduction_velocity;
}

int axon_plasticity_on_spike(
    axon_plasticity_bridge_t* bridge,
    uint32_t segment_id,
    uint64_t spike_time)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "axon_plasticity_on_spike: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    axon_segment_state_t* seg = find_or_create_segment(bridge, segment_id);
    if (!seg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "axon_plasticity_on_spike: failed to find or create segment");
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Check for conduction block */
    if (seg->conduction_state == CONDUCTION_BLOCKED) {
        bridge->stats.conduction_failures++;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "axon_plasticity_on_spike: validation failed");
        return -1;
    }

    /* Update activity */
    seg->activity_score += 1.0f;
    seg->last_spike_time = spike_time;

    /* Apply fatigue */
    seg->fatigue_level += bridge->config.conduction_fatigue_rate;
    if (seg->fatigue_level > 1.0f) seg->fatigue_level = 1.0f;

    /* Update stats */
    bridge->stats.spikes_propagated++;
    bridge->total_activity += 1.0f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Myelination API
 * ============================================================================ */

int axon_plasticity_update_myelination(axon_plasticity_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "axon_plasticity_update_myelination: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!bridge->config.enable_adaptive_myelination) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    float sum_myelin = 0.0f;
    for (size_t i = 0; i < bridge->num_segments; i++) {
        axon_segment_state_t* seg = &bridge->segments[i];

        /* Activity-dependent myelination */
        float delta_myelin = seg->activity_score * bridge->config.activity_myelination_gain;
        seg->myelination_level += delta_myelin;

        /* Clamp */
        if (seg->myelination_level > 1.0f) seg->myelination_level = 1.0f;
        if (seg->myelination_level < 0.0f) seg->myelination_level = 0.0f;

        sum_myelin += seg->myelination_level;
    }

    if (bridge->num_segments > 0) {
        bridge->avg_myelination_factor = sum_myelin / (float)bridge->num_segments;
        if (!isfinite(bridge->avg_myelination_factor)) bridge->avg_myelination_factor = 0.0f;
        bridge->stats.avg_myelination = bridge->avg_myelination_factor;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float axon_plasticity_get_myelination(
    const axon_plasticity_bridge_t* bridge,
    uint32_t segment_id)
{
    if (!bridge) return 0.0f;

    for (size_t i = 0; i < bridge->num_segments; i++) {
        if (bridge->segments[i].segment_id == segment_id) {
            return bridge->segments[i].myelination_level;
        }
    }
    return 0.0f;
}

/* ============================================================================
 * Structural Plasticity API
 * ============================================================================ */

int axon_plasticity_apply_structural(axon_plasticity_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "axon_plasticity_apply_structural: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!bridge->config.enable_structural_plasticity) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    for (size_t i = 0; i < bridge->num_segments; i++) {
        axon_segment_state_t* seg = &bridge->segments[i];

        if (seg->activity_score > bridge->config.branch_growth_threshold) {
            bridge->stats.structural_events++;
        } else if (seg->activity_score < bridge->config.branch_prune_threshold) {
            bridge->stats.structural_events++;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Update and Query API
 * ============================================================================ */

int axon_plasticity_update(axon_plasticity_bridge_t* bridge, float dt_ms)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "axon_plasticity_update: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Decay fatigue and activity */
    float fatigue_decay = expf(-dt_ms / bridge->config.conduction_recovery_tau_ms);
    float activity_decay = NIMCP_EMA_DECAY_DEFAULT;

    float sum_excitability = 0.0f;
    for (size_t i = 0; i < bridge->num_segments; i++) {
        axon_segment_state_t* seg = &bridge->segments[i];

        seg->fatigue_level *= fatigue_decay;
        seg->activity_score *= activity_decay;

        /* Intrinsic plasticity: adapt excitability */
        if (bridge->config.enable_intrinsic_plasticity) {
            float target = 0.5f;
            seg->excitability += bridge->config.excitability_adaptation_rate *
                                (target - seg->excitability);

            if (seg->excitability < bridge->config.excitability_min) {
                seg->excitability = bridge->config.excitability_min;
            }
            if (seg->excitability > bridge->config.excitability_max) {
                seg->excitability = bridge->config.excitability_max;
            }
        }

        sum_excitability += seg->excitability;
    }

    if (bridge->num_segments > 0) {
        bridge->stats.avg_excitability = sum_excitability / (float)bridge->num_segments;
        if (!isfinite(bridge->stats.avg_excitability)) bridge->stats.avg_excitability = 1.0f;
    }

    bridge->last_update_time = nimcp_time_get_us();

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Update conduction and myelination */
    axon_plasticity_update_conduction(bridge);
    axon_plasticity_update_myelination(bridge);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int axon_plasticity_get_stats(
    const axon_plasticity_bridge_t* bridge,
    axon_plasticity_stats_t* stats)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "axon_plasticity_get_stats: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "axon_plasticity_get_stats: stats is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    *stats = bridge->stats;
    return 0;
}

void axon_plasticity_reset_stats(axon_plasticity_bridge_t* bridge)
{
    if (!bridge) return;
    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
}

/* ============================================================================
 * String Conversion
 * ============================================================================ */

const char* axon_plasticity_event_to_string(axon_plasticity_event_t event)
{
    switch (event) {
        case AXON_EVENT_SPIKE_GENERATED:    return "SPIKE_GENERATED";
        case AXON_EVENT_SPIKE_PROPAGATED:   return "SPIKE_PROPAGATED";
        case AXON_EVENT_CONDUCTION_FAILURE: return "CONDUCTION_FAILURE";
        case AXON_EVENT_MYELINATION_CHANGE: return "MYELINATION_CHANGE";
        case AXON_EVENT_BRANCH_GROWTH:      return "BRANCH_GROWTH";
        case AXON_EVENT_BRANCH_RETRACTION:  return "BRANCH_RETRACTION";
        case AXON_EVENT_AIS_SHIFT:          return "AIS_SHIFT";
        default: return "UNKNOWN";
    }
}

const char* axon_conduction_state_to_string(axon_conduction_state_t state)
{
    switch (state) {
        case CONDUCTION_NORMAL:   return "NORMAL";
        case CONDUCTION_SLOWED:   return "SLOWED";
        case CONDUCTION_BLOCKED:  return "BLOCKED";
        case CONDUCTION_ENHANCED: return "ENHANCED";
        default: return "UNKNOWN";
    }
}
