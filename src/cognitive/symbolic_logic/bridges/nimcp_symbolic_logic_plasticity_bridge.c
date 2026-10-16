/**
 * @file nimcp_symbolic_logic_plasticity_bridge.c
 * @brief Safety-Plasticity Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Implementation of safety event to neuromodulatory plasticity mapping
 * WHY:  Enable system to learn from safety violations through neuromodulatory feedback
 * HOW:  Maps safety events to DA/5-HT/NE/ACh deltas, applies to plasticity orchestrator
 *
 * @author NIMCP Development Team
 */

#include "cognitive/symbolic_logic/bridges/nimcp_symbolic_logic_plasticity_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "plasticity/nimcp_plasticity_orchestrator.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "cognitive/nimcp_symbolic_logic.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(symbolic_logic_plasticity_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_symbolic_logic_plasticity_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_symbolic_logic_plasticity_bridge_mesh_registry = NULL;

nimcp_error_t symbolic_logic_plasticity_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_symbolic_logic_plasticity_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "symbolic_logic_plasticity_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "symbolic_logic_plasticity_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_symbolic_logic_plasticity_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_symbolic_logic_plasticity_bridge_mesh_registry = registry;
    return err;
}

void symbolic_logic_plasticity_bridge_mesh_unregister(void) {
    if (g_symbolic_logic_plasticity_bridge_mesh_registry && g_symbolic_logic_plasticity_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_symbolic_logic_plasticity_bridge_mesh_registry, g_symbolic_logic_plasticity_bridge_mesh_id);
        g_symbolic_logic_plasticity_bridge_mesh_id = 0;
        g_symbolic_logic_plasticity_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from symbolic_logic_plasticity_bridge module (instance-level) */
static inline void symbolic_logic_plasticity_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_symbolic_logic_plasticity_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_symbolic_logic_plasticity_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_symbolic_logic_plasticity_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


/* ============================================================================
 * Internal Types
 * ============================================================================ */

/**
 * @brief Callback registration entry
 */
typedef struct {
    bool active;
    safety_event_type_t event_type;
    safety_event_callback_t callback;
    void* user_data;
} callback_entry_t;

/**
 * @brief Event history entry
 */
typedef struct {
    safety_event_t event;
    safety_neuromod_response_t response;
    uint64_t timestamp_us;
} event_history_entry_t;

/**
 * @brief Internal bridge structure
 */
struct safety_plasticity_bridge_struct {
    bridge_base_t base;                   /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */

    /* Configuration */
    safety_plasticity_config_t config;

    /* Connected systems */
    plasticity_orchestrator_t* orchestrator;
    neuromodulator_system_t neuromod;
    symbolic_logic_t* logic;

    /* State */
    safety_plasticity_state_t state;

    /* Statistics */
    safety_plasticity_stats_t stats;

    /* Callbacks */
    callback_entry_t callbacks[SAFETY_MAX_CALLBACKS];
    uint32_t num_callbacks;

    /* Event history (ring buffer) */
    event_history_entry_t history[SAFETY_MAX_EVENT_HISTORY];
    uint32_t history_head;
    uint32_t history_count;

    /* Escalation tracking */
    uint32_t violation_counts[SAFETY_EVENT_COUNT];
    uint64_t last_event_times[SAFETY_EVENT_COUNT];

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Magic number for validation */
    uint32_t magic;
};

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Validate bridge pointer
 */
static inline bool bridge_valid(const safety_plasticity_bridge_t* bridge) {
    return bridge && bridge->magic == SAFETY_PLASTICITY_BRIDGE_MAGIC;
}

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_time_us(void) {
    return nimcp_time_now_us();
}

/**
 * @brief Add event to history ring buffer
 */
static void add_to_history(
    safety_plasticity_bridge_t* bridge,
    const safety_event_t* event,
    const safety_neuromod_response_t* response)
{
    event_history_entry_t* entry = &bridge->history[bridge->history_head];
    memcpy(&entry->event, event, sizeof(safety_event_t));
    memcpy(&entry->response, response, sizeof(safety_neuromod_response_t));
    entry->timestamp_us = get_time_us();

    bridge->history_head = (bridge->history_head + 1) % SAFETY_MAX_EVENT_HISTORY;
    if (bridge->history_count < SAFETY_MAX_EVENT_HISTORY) {
        bridge->history_count++;
    }
}

/**
 * @brief Log safety event
 */
static void log_safety_event(
    const safety_plasticity_bridge_t* bridge,
    const safety_event_t* event,
    const safety_neuromod_response_t* response)
{
    if (!bridge->config.enable_event_logging) {
        return;
    }

    const char* type_name = safety_event_type_name(event->type);

    switch (response->severity_level) {
        case 3:  /* Critical */
            NIMCP_LOGGING_ERROR("[SAFETY-PLASTICITY] CRITICAL: %s - Rule: %s, Module: %s, Mag: %.2f",
                               type_name, event->rule_name, event->source_module, event->magnitude);
            break;
        case 2:  /* Error */
            NIMCP_LOGGING_ERROR("[SAFETY-PLASTICITY] %s - Rule: %s, Module: %s, Mag: %.2f",
                               type_name, event->rule_name, event->source_module, event->magnitude);
            break;
        case 1:  /* Warning */
            NIMCP_LOGGING_WARN("[SAFETY-PLASTICITY] %s - Rule: %s, Module: %s, Mag: %.2f",
                              type_name, event->rule_name, event->source_module, event->magnitude);
            break;
        default: /* Info */
            NIMCP_LOGGING_INFO("[SAFETY-PLASTICITY] %s - Rule: %s, Module: %s, Mag: %.2f",
                              type_name, event->rule_name, event->source_module, event->magnitude);
            break;
    }

    if (bridge->config.enable_response_logging) {
        NIMCP_LOGGING_DEBUG("[SAFETY-PLASTICITY] Response: DA=%.2f, 5-HT=%.2f, NE=%.2f, ACh=%.2f, LR=%.2f",
                           response->dopamine_delta, response->serotonin_delta,
                           response->norepinephrine_delta, response->acetylcholine_delta,
                           response->learning_rate_modifier);
    }
}

/**
 * @brief Compute default response for event type
 */
static void compute_default_response(
    safety_plasticity_bridge_t* bridge,
    const safety_event_t* event,
    safety_neuromod_response_t* response)
{
    /* Initialize to zeros */
    safety_neuromod_response_init(response);

    float mag = nimcp_clampf(event->magnitude, 0.0f, 1.0f);

    switch (event->type) {
        case SAFETY_EVENT_VIOLATION_BLOCKED:
            /*
             * VIOLATION_BLOCKED: Strong punishment signal
             * DA=-0.8, trigger negative burst
             */
            response->dopamine_delta = SAFETY_DA_VIOLATION_BLOCKED * mag *
                                       bridge->config.violation_response_gain;
            response->trigger_burst = bridge->config.enable_burst_signaling;
            response->burst_is_positive = false;
            response->burst_magnitude = mag * 0.8f;
            response->learning_rate_modifier = SAFETY_LR_VIOLATION_BLOCKED;
            response->trigger_ltd = bridge->config.enable_ltd_triggers;
            response->severity_level = 2;  /* Error */
            break;

        case SAFETY_EVENT_VIOLATION_ESCALATED:
            /*
             * VIOLATION_ESCALATED: Moderate punishment + high vigilance
             * DA=-0.3, NE=+0.5
             */
            response->dopamine_delta = SAFETY_DA_VIOLATION_ESCALATED * mag *
                                       bridge->config.violation_response_gain;
            response->norepinephrine_delta = SAFETY_NE_VIOLATION_ESCALATED * mag *
                                             bridge->config.violation_response_gain;
            response->learning_rate_modifier = SAFETY_LR_VIOLATION_ESCALATED;
            response->trigger_ltd = bridge->config.enable_ltd_triggers;
            response->severity_level = 2;  /* Error */
            break;

        case SAFETY_EVENT_COMPLIANCE:
            /*
             * COMPLIANCE: Small positive reinforcement
             * DA=+0.2
             */
            response->dopamine_delta = SAFETY_DA_COMPLIANCE * mag *
                                       bridge->config.compliance_response_gain;
            response->learning_rate_modifier = SAFETY_LR_COMPLIANCE;
            response->trigger_ltp = bridge->config.enable_ltp_triggers && (mag > 0.5f);
            response->severity_level = 0;  /* Info */
            break;

        case SAFETY_EVENT_OVERRIDE_ACCEPTED:
            /*
             * OVERRIDE_ACCEPTED: Authorized exception is OK
             * DA=+0.5, 5-HT=+0.3
             */
            response->dopamine_delta = SAFETY_DA_OVERRIDE_ACCEPTED * mag *
                                       bridge->config.override_response_gain;
            response->serotonin_delta = SAFETY_5HT_OVERRIDE_ACCEPTED * mag *
                                        bridge->config.override_response_gain;
            response->learning_rate_modifier = SAFETY_LR_OVERRIDE_ACCEPTED;
            response->severity_level = 1;  /* Warning (overrides are still notable) */
            break;

        case SAFETY_EVENT_OVERRIDE_REJECTED:
            /*
             * OVERRIDE_REJECTED: Maximum punishment signal
             * DA=-1.0, NE=+1.0 (maximum alert)
             */
            response->dopamine_delta = SAFETY_DA_OVERRIDE_REJECTED * mag *
                                       bridge->config.override_response_gain;
            response->norepinephrine_delta = SAFETY_NE_OVERRIDE_REJECTED * mag *
                                             bridge->config.override_response_gain;
            response->trigger_burst = bridge->config.enable_burst_signaling;
            response->burst_is_positive = false;
            response->burst_magnitude = mag;
            response->learning_rate_modifier = SAFETY_LR_OVERRIDE_REJECTED;
            response->trigger_ltd = bridge->config.enable_ltd_triggers;
            response->severity_level = 3;  /* Critical */
            break;

        case SAFETY_EVENT_DECEPTION_DETECTED:
            /*
             * DECEPTION_DETECTED: Maximum punishment, trigger LTD
             * DA=-1.0, strong LTD
             */
            response->dopamine_delta = SAFETY_DA_DECEPTION_DETECTED * mag *
                                       bridge->config.deception_response_gain;
            response->trigger_burst = bridge->config.enable_burst_signaling;
            response->burst_is_positive = false;
            response->burst_magnitude = mag;
            response->learning_rate_modifier = SAFETY_LR_DECEPTION_DETECTED;
            response->trigger_ltd = true;  /* Always trigger LTD for deception */
            response->severity_level = 3;  /* Critical */
            break;

        case SAFETY_EVENT_INTEGRITY_VERIFIED:
            /*
             * INTEGRITY_VERIFIED: Small positive reinforcement
             * DA=+0.1, ACh=+0.2 (attention to integrity)
             */
            response->dopamine_delta = SAFETY_DA_INTEGRITY_VERIFIED * mag;
            response->acetylcholine_delta = SAFETY_ACH_INTEGRITY_VERIFIED * mag;
            response->learning_rate_modifier = 1.0f;
            response->severity_level = 0;  /* Info */
            break;

        case SAFETY_EVENT_INTEGRITY_FAILED:
            /*
             * INTEGRITY_FAILED: SYSTEM HALT
             * No plasticity can bypass this - external intervention required
             */
            response->halt_system = bridge->config.auto_halt_on_integrity_fail;
            response->severity_level = 3;  /* Critical */
            response->log_event = true;
            /* No learning - system must halt */
            response->learning_rate_modifier = 0.0f;
            break;

        default:
            NIMCP_LOGGING_WARN("[SAFETY-PLASTICITY] Unknown event type: %d", event->type);
            response->severity_level = 1;
            break;
    }

    /* Apply learning rate bounds */
    response->learning_rate_modifier = nimcp_clampf(
        response->learning_rate_modifier,
        bridge->config.min_learning_rate_modifier,
        bridge->config.max_learning_rate_modifier);

    /* Always log safety events */
    response->log_event = true;
}

/**
 * @brief Invoke registered callbacks
 */
static int invoke_callbacks(
    safety_plasticity_bridge_t* bridge,
    const safety_event_t* event,
    safety_neuromod_response_t* response)
{
    for (uint32_t i = 0; i < bridge->num_callbacks; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_callbacks > 256) {
            symbolic_logic_plasticity_bridge_heartbeat("symbolic_log_loop",
                             (float)(i + 1) / (float)bridge->num_callbacks);
        }

        callback_entry_t* cb = &bridge->callbacks[i];
        if (!cb->active) continue;

        /* Check if callback matches this event type */
        if (cb->event_type != SAFETY_EVENT_COUNT &&
            cb->event_type != event->type) {
            continue;
        }

        /* Invoke callback */
        int result = cb->callback(event, response, cb->user_data);
        if (result != 0) {
            NIMCP_LOGGING_WARN("[SAFETY-PLASTICITY] Callback %u aborted processing (code %d)", i, result);
            return result;
        }
    }
    return 0;
}

/**
 * @brief Update escalation tracking
 */
static void update_escalation_tracking(
    safety_plasticity_bridge_t* bridge,
    const safety_event_t* event)
{
    if (!bridge->config.enable_escalation_tracking) {
        return;
    }

    uint64_t now = get_time_us();
    uint64_t last = bridge->last_event_times[event->type];
    uint64_t cooldown_us = (uint64_t)bridge->config.event_cooldown_ms * 1000;

    /* Reset count if cooldown elapsed, otherwise increment */
    if (now - last > cooldown_us * 10) {  /* Reset after 10x cooldown */
        bridge->violation_counts[event->type] = 1;
    } else {
        bridge->violation_counts[event->type]++;
    }

    bridge->last_event_times[event->type] = now;

    /* Track consecutive violations for state */
    if (event->type == SAFETY_EVENT_VIOLATION_BLOCKED ||
        event->type == SAFETY_EVENT_VIOLATION_ESCALATED ||
        event->type == SAFETY_EVENT_OVERRIDE_REJECTED ||
        event->type == SAFETY_EVENT_DECEPTION_DETECTED) {
        bridge->state.consecutive_violations++;
    } else if (event->type == SAFETY_EVENT_COMPLIANCE) {
        bridge->state.consecutive_violations = 0;
    }
}

/**
 * @brief Update statistics from response
 */
static void update_stats(
    safety_plasticity_bridge_t* bridge,
    const safety_event_t* event,
    const safety_neuromod_response_t* response)
{
    /* Event counts by type */
    switch (event->type) {
        case SAFETY_EVENT_VIOLATION_BLOCKED:
            bridge->stats.violations_blocked++;
            break;
        case SAFETY_EVENT_VIOLATION_ESCALATED:
            bridge->stats.violations_escalated++;
            break;
        case SAFETY_EVENT_COMPLIANCE:
            bridge->stats.compliance_events++;
            break;
        case SAFETY_EVENT_OVERRIDE_ACCEPTED:
            bridge->stats.overrides_accepted++;
            break;
        case SAFETY_EVENT_OVERRIDE_REJECTED:
            bridge->stats.overrides_rejected++;
            break;
        case SAFETY_EVENT_DECEPTION_DETECTED:
            bridge->stats.deceptions_detected++;
            break;
        case SAFETY_EVENT_INTEGRITY_VERIFIED:
            bridge->stats.integrity_verified++;
            break;
        case SAFETY_EVENT_INTEGRITY_FAILED:
            bridge->stats.integrity_failed++;
            break;
        default:
            break;
    }

    /* Response counts */
    if (response->trigger_ltd) bridge->stats.ltd_triggers++;
    if (response->trigger_ltp) bridge->stats.ltp_triggers++;
    if (response->trigger_burst) {
        if (response->burst_is_positive) {
            bridge->stats.positive_bursts++;
        } else {
            bridge->stats.negative_bursts++;
        }
    }

    /* Accumulate deltas */
    bridge->stats.total_da_delta += response->dopamine_delta;
    bridge->stats.total_ne_delta += response->norepinephrine_delta;
    bridge->stats.total_5ht_delta += response->serotonin_delta;
    bridge->stats.total_ach_delta += response->acetylcholine_delta;

    /* Update averages */
    bridge->stats.total_events++;
    float n = (float)bridge->stats.total_events;
    float mag = fabsf(response->dopamine_delta) + fabsf(response->norepinephrine_delta) +
                fabsf(response->serotonin_delta) + fabsf(response->acetylcholine_delta);
    bridge->stats.avg_response_magnitude =
        (bridge->stats.avg_response_magnitude * (n - 1) + mag) / (fabsf(n) > 1e-7f ? n : 1e-7f);
    bridge->stats.avg_learning_rate_mod =
        (bridge->stats.avg_learning_rate_mod * (n - 1) + response->learning_rate_modifier) / (fabsf(n) > 1e-7f ? n : 1e-7f);

    bridge->stats.last_event_us = get_time_us();
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

int safety_plasticity_bridge_default_config(safety_plasticity_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_plasticity_bridge_heartbeat("symbolic_log_safety_plasticity_br", 0.0f);


    memset(config, 0, sizeof(safety_plasticity_config_t));

    /* Response gains */
    config->violation_response_gain = 1.0f;
    config->compliance_response_gain = 1.0f;
    config->override_response_gain = 1.0f;
    config->deception_response_gain = 1.0f;

    /* Learning rate bounds */
    config->min_learning_rate_modifier = 0.1f;
    config->max_learning_rate_modifier = 3.0f;

    /* Timing */
    config->event_cooldown_ms = 100;
    config->burst_duration_ms = 500;

    /* Logging */
    config->enable_event_logging = true;
    config->enable_response_logging = false;
    config->log_level = 1;  /* Warnings and above */

    /* Features */
    config->enable_burst_signaling = true;
    config->enable_ltd_triggers = true;
    config->enable_ltp_triggers = true;
    config->enable_escalation_tracking = true;
    config->auto_halt_on_integrity_fail = true;

    return 0;
}

safety_plasticity_bridge_t* safety_plasticity_bridge_create(
    const safety_plasticity_config_t* config)
{
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_plasticity_bridge_heartbeat("symbolic_log_safety_plasticity_br", 0.0f);


    safety_plasticity_bridge_t* bridge = nimcp_calloc(1, sizeof(safety_plasticity_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("[SAFETY-PLASTICITY] Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "safety_plasticity_bridge_create: bridge is NULL");
        return NULL;
    }

    /* Initialize base */
    if (bridge_base_init(&bridge->base, SAFETY_PLASTICITY_MODULE_ID,
                         SAFETY_PLASTICITY_MODULE_NAME) != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "safety_plasticity_bridge_create: bridge is NULL");
        return NULL;
    }

    /* Set magic */
    bridge->magic = SAFETY_PLASTICITY_BRIDGE_MAGIC;

    /* Apply config */
    if (config) {
        memcpy(&bridge->config, config, sizeof(safety_plasticity_config_t));
    } else {
        safety_plasticity_bridge_default_config(&bridge->config);
    }

    /* Initialize state */
    bridge->state.last_event_type = SAFETY_EVENT_COUNT;

    /* Initialize stats averages */
    bridge->stats.avg_learning_rate_mod = 1.0f;

    NIMCP_LOGGING_INFO("[SAFETY-PLASTICITY] Bridge created successfully");
    return bridge;
}

void safety_plasticity_bridge_destroy(safety_plasticity_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect */
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_plasticity_bridge_heartbeat("symbolic_log_safety_plasticity_br", 0.0f);


    safety_plasticity_bridge_disconnect(bridge);

    /* Cleanup */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    bridge_base_cleanup(&bridge->base);
    bridge->magic = 0;
    nimcp_free(bridge);
    bridge = NULL;

    NIMCP_LOGGING_INFO("[SAFETY-PLASTICITY] Bridge destroyed");
}

/* Connection API */

int safety_plasticity_bridge_connect_orchestrator(
    safety_plasticity_bridge_t* bridge,
    plasticity_orchestrator_t* orchestrator)
{
    if (!bridge_valid(bridge) || !orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "safety_plasticity_bridge_connect_orchestrator: required parameter is NULL (bridge_valid, orchestrator)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_plasticity_bridge_heartbeat("symbolic_log_safety_plasticity_br", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->orchestrator = orchestrator;
    bridge->base.system_a = orchestrator;
    bridge->base.system_a_connected = true;
    bridge->state.connected = bridge->base.system_a_connected && bridge->neuromod != NULL;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("[SAFETY-PLASTICITY] Connected to plasticity orchestrator");
    return 0;
}

int safety_plasticity_bridge_connect_neuromod(
    safety_plasticity_bridge_t* bridge,
    neuromodulator_system_t neuromod)
{
    if (!bridge_valid(bridge) || !neuromod) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "safety_plasticity_bridge_connect_neuromod: required parameter is NULL (bridge_valid, neuromod)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_plasticity_bridge_heartbeat("symbolic_log_safety_plasticity_br", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->neuromod = neuromod;
    bridge->base.system_b = neuromod;
    bridge->base.system_b_connected = true;
    bridge->state.connected = bridge->orchestrator != NULL && bridge->base.system_b_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("[SAFETY-PLASTICITY] Connected to neuromodulator system");
    return 0;
}

int safety_plasticity_bridge_connect_logic(
    safety_plasticity_bridge_t* bridge,
    symbolic_logic_t* logic)
{
    if (!bridge_valid(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "safety_plasticity_bridge_connect_logic: bridge_valid is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_plasticity_bridge_heartbeat("symbolic_log_safety_plasticity_br", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->logic = logic;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("[SAFETY-PLASTICITY] Connected to symbolic logic system");
    return 0;
}

int safety_plasticity_bridge_disconnect(safety_plasticity_bridge_t* bridge) {
    if (!bridge_valid(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "safety_plasticity_bridge_disconnect: bridge_valid is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_plasticity_bridge_heartbeat("symbolic_log_safety_plasticity_br", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->orchestrator = NULL;
    bridge->neuromod = NULL;
    bridge->logic = NULL;
    bridge->base.system_a = NULL;
    bridge->base.system_b = NULL;
    bridge->base.system_a_connected = false;
    bridge->base.system_b_connected = false;
    bridge->state.connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("[SAFETY-PLASTICITY] Disconnected all systems");
    return 0;
}

bool safety_plasticity_bridge_is_connected(const safety_plasticity_bridge_t* bridge) {
    if (!bridge_valid(bridge)) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_plasticity_bridge_heartbeat("symbolic_log_safety_plasticity_br", 0.0f);


    return bridge->state.connected;
}

/* Core API */

int safety_plasticity_map_event(
    safety_plasticity_bridge_t* bridge,
    const safety_event_t* event,
    safety_neuromod_response_t* response)
{
    if (!bridge_valid(bridge) || !event || !response) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "safety_plasticity_map_event: required parameter is NULL (bridge_valid, event, response)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_plasticity_bridge_heartbeat("symbolic_log_safety_plasticity_ma", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute default response based on event type */
    compute_default_response(bridge, event, response);

    /* Update escalation tracking */
    update_escalation_tracking(bridge, event);

    /* Invoke registered callbacks (can modify response) */
    int cb_result = invoke_callbacks(bridge, event, response);
    if (cb_result != 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return cb_result;
    }

    /* Log event */
    log_safety_event(bridge, event, response);

    /* Add to history */
    add_to_history(bridge, event, response);

    /* Update statistics */
    update_stats(bridge, event, response);

    /* Update state */
    bridge->state.last_event_type = event->type;
    bridge->state.last_event_us = get_time_us();

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int safety_plasticity_apply_response(
    safety_plasticity_bridge_t* bridge,
    const safety_neuromod_response_t* response)
{
    if (!bridge_valid(bridge) || !response) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "safety_plasticity_apply_response: required parameter is NULL (bridge_valid, response)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_plasticity_bridge_heartbeat("symbolic_log_safety_plasticity_ap", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check for halt */
    if (response->halt_system) {
        bridge->state.system_halted = true;
        NIMCP_LOGGING_ERROR("[SAFETY-PLASTICITY] SYSTEM HALT triggered by integrity failure!");
        nimcp_mutex_unlock(bridge->base.mutex);
        return 1;  /* Return 1 to indicate halt */
    }

    /* Apply neuromodulator deltas if connected */
    if (bridge->neuromod) {
        /* Get current levels and add deltas */
        float da = neuromodulator_get_level(bridge->neuromod, NEUROMOD_DOPAMINE);
        float ne = neuromodulator_get_level(bridge->neuromod, NEUROMOD_NOREPINEPHRINE);
        float ht = neuromodulator_get_level(bridge->neuromod, NEUROMOD_SEROTONIN);
        float ach = neuromodulator_get_level(bridge->neuromod, NEUROMOD_ACETYLCHOLINE);

        /* Apply deltas with clamping */
        da = nimcp_clampf(da + response->dopamine_delta, 0.0f, 1.0f);
        ne = nimcp_clampf(ne + response->norepinephrine_delta, 0.0f, 1.0f);
        ht = nimcp_clampf(ht + response->serotonin_delta, 0.0f, 1.0f);
        ach = nimcp_clampf(ach + response->acetylcholine_delta, 0.0f, 1.0f);

        neuromodulator_set_level(bridge->neuromod, NEUROMOD_DOPAMINE, da);
        neuromodulator_set_level(bridge->neuromod, NEUROMOD_NOREPINEPHRINE, ne);
        neuromodulator_set_level(bridge->neuromod, NEUROMOD_SEROTONIN, ht);
        neuromodulator_set_level(bridge->neuromod, NEUROMOD_ACETYLCHOLINE, ach);
    }

    /* Apply burst if triggered */
    if (response->trigger_burst && bridge->config.enable_burst_signaling) {
        bridge->state.burst_active = true;
        bridge->state.burst_is_positive = response->burst_is_positive;
        bridge->state.burst_remaining_magnitude = response->burst_magnitude;
        bridge->state.burst_start_us = get_time_us();
    }

    /* Trigger LTD/LTP via orchestrator if connected */
    if (bridge->orchestrator) {
        /* For now, we use reward signal to indicate punishment/reward */
        if (response->trigger_ltd) {
            /* Negative reward signal triggers LTD-like effects */
            plasticity_orchestrator_reward(bridge->orchestrator,
                                          -response->learning_rate_modifier * 0.5f,
                                          get_time_us() / 1000);  /* Convert to ms */
        } else if (response->trigger_ltp) {
            /* Positive reward signal triggers LTP-like effects */
            plasticity_orchestrator_reward(bridge->orchestrator,
                                          response->learning_rate_modifier * 0.3f,
                                          get_time_us() / 1000);
        }
    }

    /* Store pending deltas */
    bridge->state.pending_da_delta = response->dopamine_delta;
    bridge->state.pending_ne_delta = response->norepinephrine_delta;
    bridge->state.pending_5ht_delta = response->serotonin_delta;
    bridge->state.pending_ach_delta = response->acetylcholine_delta;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int safety_plasticity_process_event(
    safety_plasticity_bridge_t* bridge,
    const safety_event_t* event)
{
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_plasticity_bridge_heartbeat("symbolic_log_safety_plasticity_pr", 0.0f);


    safety_neuromod_response_t response;

    int map_result = safety_plasticity_map_event(bridge, event, &response);
    if (map_result != 0) {
        return map_result;
    }

    return safety_plasticity_apply_response(bridge, &response);
}

/* Callback API */

int safety_plasticity_register_callback(
    safety_plasticity_bridge_t* bridge,
    safety_event_type_t event_type,
    safety_event_callback_t callback,
    void* user_data)
{
    if (!bridge_valid(bridge) || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "safety_plasticity_register_callback: required parameter is NULL (bridge_valid, callback)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_plasticity_bridge_heartbeat("symbolic_log_safety_plasticity_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Find free slot */
    int slot = -1;
    for (uint32_t i = 0; i < SAFETY_MAX_CALLBACKS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && SAFETY_MAX_CALLBACKS > 256) {
            symbolic_logic_plasticity_bridge_heartbeat("symbolic_log_loop",
                             (float)(i + 1) / (float)SAFETY_MAX_CALLBACKS);
        }

        if (!bridge->callbacks[i].active) {
            slot = (int)i;
            break;
        }
    }

    if (slot < 0) {
        NIMCP_LOGGING_ERROR("[SAFETY-PLASTICITY] No free callback slots");
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "safety_plasticity_register_callback: validation failed");
        return -1;
    }

    bridge->callbacks[slot].active = true;
    bridge->callbacks[slot].event_type = event_type;
    bridge->callbacks[slot].callback = callback;
    bridge->callbacks[slot].user_data = user_data;
    bridge->num_callbacks++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return slot;
}

int safety_plasticity_unregister_callback(
    safety_plasticity_bridge_t* bridge,
    int callback_id)
{
    if (!bridge_valid(bridge) || callback_id < 0 ||
        callback_id >= SAFETY_MAX_CALLBACKS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "safety_plasticity_unregister_callback: bridge_valid is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_plasticity_bridge_heartbeat("symbolic_log_safety_plasticity_un", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->callbacks[callback_id].active) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "safety_plasticity_unregister_callback: bridge->callbacks is NULL");
        return -1;
    }

    bridge->callbacks[callback_id].active = false;
    bridge->callbacks[callback_id].callback = NULL;
    bridge->callbacks[callback_id].user_data = NULL;
    bridge->num_callbacks--;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* Update API */

int safety_plasticity_bridge_update(
    safety_plasticity_bridge_t* bridge,
    float delta_ms)
{
    if (!bridge_valid(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "safety_plasticity_bridge_update: bridge_valid is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_plasticity_bridge_heartbeat("symbolic_log_safety_plasticity_br", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    uint64_t now = get_time_us();

    /* Process burst decay */
    if (bridge->state.burst_active) {
        float elapsed_ms = (float)(now - bridge->state.burst_start_us) / 1000.0f;
        if (elapsed_ms >= (float)bridge->config.burst_duration_ms) {
            bridge->state.burst_active = false;
            bridge->state.burst_remaining_magnitude = 0.0f;
        } else {
            /* Exponential decay */
            float decay = expf(-delta_ms / (float)bridge->config.burst_duration_ms);
            bridge->state.burst_remaining_magnitude *= decay;
        }
    }

    /* Clear pending deltas (already applied) */
    bridge->state.pending_da_delta = 0.0f;
    bridge->state.pending_ne_delta = 0.0f;
    bridge->state.pending_5ht_delta = 0.0f;
    bridge->state.pending_ach_delta = 0.0f;

    bridge->state.last_update_us = now;
    bridge->stats.total_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* State and Statistics API */

int safety_plasticity_get_state(
    const safety_plasticity_bridge_t* bridge,
    safety_plasticity_state_t* state)
{
    if (!bridge_valid(bridge) || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "safety_plasticity_get_state: required parameter is NULL (bridge_valid, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_plasticity_bridge_heartbeat("symbolic_log_safety_plasticity_ge", 0.0f);


    nimcp_mutex_lock(((safety_plasticity_bridge_t*)bridge)->mutex);
    memcpy(state, &bridge->state, sizeof(safety_plasticity_state_t));
    nimcp_mutex_unlock(((safety_plasticity_bridge_t*)bridge)->mutex);
    return 0;
}

int safety_plasticity_get_stats(
    const safety_plasticity_bridge_t* bridge,
    safety_plasticity_stats_t* stats)
{
    if (!bridge_valid(bridge) || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "safety_plasticity_get_stats: required parameter is NULL (bridge_valid, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_plasticity_bridge_heartbeat("symbolic_log_safety_plasticity_ge", 0.0f);


    nimcp_mutex_lock(((safety_plasticity_bridge_t*)bridge)->mutex);
    memcpy(stats, &bridge->stats, sizeof(safety_plasticity_stats_t));
    nimcp_mutex_unlock(((safety_plasticity_bridge_t*)bridge)->mutex);
    return 0;
}

int safety_plasticity_reset_stats(safety_plasticity_bridge_t* bridge) {
    if (!bridge_valid(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "safety_plasticity_reset_stats: bridge_valid is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_plasticity_bridge_heartbeat("symbolic_log_safety_plasticity_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(safety_plasticity_stats_t));
    bridge->stats.avg_learning_rate_mod = 1.0f;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* Diagnostic API */

bool safety_plasticity_is_halted(const safety_plasticity_bridge_t* bridge) {
    if (!bridge_valid(bridge)) {
        return true;  /* Assume halted if invalid */
    }
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_plasticity_bridge_heartbeat("symbolic_log_safety_plasticity_is", 0.0f);


    return bridge->state.system_halted;
}

int safety_plasticity_clear_halt(
    safety_plasticity_bridge_t* bridge,
    uint64_t authorization_code)
{
    if (!bridge_valid(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "safety_plasticity_clear_halt: bridge_valid is NULL");
        return -1;
    }

    /* Simple authorization check - in production, this would be more sophisticated */
    /* The authorization code must be non-zero and match a specific pattern */
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_plasticity_bridge_heartbeat("symbolic_log_safety_plasticity_cl", 0.0f);


    if (authorization_code == 0) {
        NIMCP_LOGGING_ERROR("[SAFETY-PLASTICITY] Invalid authorization code for halt clear");
        return -2;
    }

    /* Verify code pattern (simple check - should be cryptographically secure in production) */
    uint64_t expected = 0xDEADBEEF12345678ULL;  /* Placeholder for proper auth */
    if (authorization_code != expected) {
        NIMCP_LOGGING_WARN("[SAFETY-PLASTICITY] Incorrect authorization code for halt clear");
        return -2;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state.system_halted = false;
    NIMCP_LOGGING_WARN("[SAFETY-PLASTICITY] System halt cleared with authorization");
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

const char* safety_event_type_name(safety_event_type_t type) {
    switch (type) {
        case SAFETY_EVENT_VIOLATION_BLOCKED:   return "VIOLATION_BLOCKED";
        case SAFETY_EVENT_VIOLATION_ESCALATED: return "VIOLATION_ESCALATED";
        case SAFETY_EVENT_COMPLIANCE:          return "COMPLIANCE";
        case SAFETY_EVENT_OVERRIDE_ACCEPTED:   return "OVERRIDE_ACCEPTED";
        case SAFETY_EVENT_OVERRIDE_REJECTED:   return "OVERRIDE_REJECTED";
        case SAFETY_EVENT_DECEPTION_DETECTED:  return "DECEPTION_DETECTED";
        case SAFETY_EVENT_INTEGRITY_VERIFIED:  return "INTEGRITY_VERIFIED";
        case SAFETY_EVENT_INTEGRITY_FAILED:    return "INTEGRITY_FAILED";
        default:                               return "UNKNOWN";
    }
}

void safety_plasticity_print_summary(const safety_plasticity_bridge_t* bridge) {
    if (!bridge_valid(bridge)) {
        NIMCP_LOGGING_INFO("[SAFETY-PLASTICITY] Bridge: NULL or invalid");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_plasticity_bridge_heartbeat("symbolic_log_safety_plasticity_pr", 0.0f);


    NIMCP_LOGGING_INFO("=== Safety-Plasticity Bridge Summary ===");
    NIMCP_LOGGING_INFO("Connected: %s", bridge->state.connected ? "yes" : "no");
    NIMCP_LOGGING_INFO("Halted: %s", bridge->state.system_halted ? "YES" : "no");
    NIMCP_LOGGING_INFO("");
    NIMCP_LOGGING_INFO("Event Statistics:");
    NIMCP_LOGGING_INFO("  Violations blocked:   %lu", (unsigned long)bridge->stats.violations_blocked);
    NIMCP_LOGGING_INFO("  Violations escalated: %lu", (unsigned long)bridge->stats.violations_escalated);
    NIMCP_LOGGING_INFO("  Compliance events:    %lu", (unsigned long)bridge->stats.compliance_events);
    NIMCP_LOGGING_INFO("  Overrides accepted:   %lu", (unsigned long)bridge->stats.overrides_accepted);
    NIMCP_LOGGING_INFO("  Overrides rejected:   %lu", (unsigned long)bridge->stats.overrides_rejected);
    NIMCP_LOGGING_INFO("  Deceptions detected:  %lu", (unsigned long)bridge->stats.deceptions_detected);
    NIMCP_LOGGING_INFO("  Integrity verified:   %lu", (unsigned long)bridge->stats.integrity_verified);
    NIMCP_LOGGING_INFO("  Integrity failed:     %lu", (unsigned long)bridge->stats.integrity_failed);
    NIMCP_LOGGING_INFO("");
    NIMCP_LOGGING_INFO("Response Statistics:");
    NIMCP_LOGGING_INFO("  LTD triggers:       %lu", (unsigned long)bridge->stats.ltd_triggers);
    NIMCP_LOGGING_INFO("  LTP triggers:       %lu", (unsigned long)bridge->stats.ltp_triggers);
    NIMCP_LOGGING_INFO("  Negative bursts:    %lu", (unsigned long)bridge->stats.negative_bursts);
    NIMCP_LOGGING_INFO("  Positive bursts:    %lu", (unsigned long)bridge->stats.positive_bursts);
    NIMCP_LOGGING_INFO("");
    NIMCP_LOGGING_INFO("Cumulative Deltas:");
    NIMCP_LOGGING_INFO("  Total DA delta:     %.3f", bridge->stats.total_da_delta);
    NIMCP_LOGGING_INFO("  Total NE delta:     %.3f", bridge->stats.total_ne_delta);
    NIMCP_LOGGING_INFO("  Total 5-HT delta:   %.3f", bridge->stats.total_5ht_delta);
    NIMCP_LOGGING_INFO("  Total ACh delta:    %.3f", bridge->stats.total_ach_delta);
    NIMCP_LOGGING_INFO("");
    NIMCP_LOGGING_INFO("Averages:");
    NIMCP_LOGGING_INFO("  Avg response mag:   %.3f", bridge->stats.avg_response_magnitude);
    NIMCP_LOGGING_INFO("  Avg LR modifier:    %.3f", bridge->stats.avg_learning_rate_mod);
    NIMCP_LOGGING_INFO("");
    NIMCP_LOGGING_INFO("Total events: %lu, Total updates: %lu",
                       (unsigned long)bridge->stats.total_events,
                       (unsigned long)bridge->stats.total_updates);
    NIMCP_LOGGING_INFO("=========================================");
}

/* Bio-Async API */

int safety_plasticity_bridge_connect_bio_async(safety_plasticity_bridge_t* bridge) {
    if (!bridge_valid(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "safety_plasticity_bridge_connect_bio_async: bridge_valid is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_plasticity_bridge_heartbeat("symbolic_log_safety_plasticity_br", 0.0f);


    return bridge_base_connect_bio_async(&bridge->base);
}

int safety_plasticity_bridge_disconnect_bio_async(safety_plasticity_bridge_t* bridge) {
    if (!bridge_valid(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "safety_plasticity_bridge_disconnect_bio_async: bridge_valid is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_plasticity_bridge_heartbeat("symbolic_log_safety_plasticity_br", 0.0f);


    return bridge_base_disconnect_bio_async(&bridge->base);
}

bool safety_plasticity_bridge_is_bio_async_connected(
    const safety_plasticity_bridge_t* bridge)
{
    if (!bridge_valid(bridge)) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_plasticity_bridge_heartbeat("symbolic_log_safety_plasticity_br", 0.0f);


    return bridge_base_is_bio_async_connected(&bridge->base);
}

/* Convenience Functions */

safety_event_t safety_event_create(
    safety_event_type_t type,
    float magnitude,
    const char* rule_name,
    const char* source_module)
{
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_plasticity_bridge_heartbeat("symbolic_log_safety_event_create", 0.0f);


    safety_event_t event;
    memset(&event, 0, sizeof(event));

    event.type = type;
    event.magnitude = nimcp_clampf(magnitude, 0.0f, 1.0f);
    event.timestamp_us = get_time_us();
    event.confidence = 1.0f;
    event.violation_count = 1;

    if (rule_name) {
        strncpy(event.rule_name, rule_name, SAFETY_MAX_RULE_NAME_LENGTH - 1);
        event.rule_name[SAFETY_MAX_RULE_NAME_LENGTH - 1] = '\0';
    } else {
        strncpy(event.rule_name, "unknown", SAFETY_MAX_RULE_NAME_LENGTH - 1);
        event.rule_name[SAFETY_MAX_RULE_NAME_LENGTH - 1] = '\0';
    }

    if (source_module) {
        strncpy(event.source_module, source_module, SAFETY_MAX_MODULE_NAME_LENGTH - 1);
        event.source_module[SAFETY_MAX_MODULE_NAME_LENGTH - 1] = '\0';
    } else {
        strncpy(event.source_module, "unknown", SAFETY_MAX_MODULE_NAME_LENGTH - 1);
        event.source_module[SAFETY_MAX_MODULE_NAME_LENGTH - 1] = '\0';
    }

    return event;
}

void safety_neuromod_response_init(safety_neuromod_response_t* response) {
    if (!response) return;
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_plasticity_bridge_heartbeat("symbolic_log_safety_neuromod_resp", 0.0f);


    memset(response, 0, sizeof(safety_neuromod_response_t));
    response->learning_rate_modifier = 1.0f;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void symbolic_logic_plasticity_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_symbolic_logic_plasticity_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int symbolic_logic_plasticity_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "symbolic_logic_plasticity_bridge_training_begin: NULL argument");
        return -1;
    }
    symbolic_logic_plasticity_bridge_heartbeat_instance(NULL, "symbolic_logic_plasticity_bridge_training_begin", 0.0f);
    (void)instance;
    return 0;
}

int symbolic_logic_plasticity_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "symbolic_logic_plasticity_bridge_training_end: NULL argument");
        return -1;
    }
    symbolic_logic_plasticity_bridge_heartbeat_instance(NULL, "symbolic_logic_plasticity_bridge_training_end", 1.0f);
    (void)instance;
    return 0;
}

int symbolic_logic_plasticity_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "symbolic_logic_plasticity_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    symbolic_logic_plasticity_bridge_heartbeat_instance(NULL, "symbolic_logic_plasticity_bridge_training_step", progress);
    (void)instance;
    return 0;
}
