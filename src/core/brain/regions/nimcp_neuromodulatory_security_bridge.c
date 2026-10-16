/**
 * @file nimcp_neuromodulatory_security_bridge.c
 * @brief Implementation of Unified Neuromodulatory-Security Bridge
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/nimcp_neuromodulatory_security_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdio.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(neuromodulatory_security_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "NEUROMODULATORY_SECURITY_BRIDGE"


/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct neuromod_security_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    uint32_t magic;
    neuromod_security_bridge_config_t config;

    /* Adapter connections */
    nimcp_lc_adapter_t lc_adapter;
    nimcp_vta_adapter_t vta_adapter;
    nimcp_raphe_adapter_t raphe_adapter;
    nimcp_habenula_adapter_t habenula_adapter;

    /* Security connection */
    nimcp_security_context_t security;
    bool connected;

    /* State caches */
    neuromod_security_modulation_t modulation;
    neuromod_security_feedback_t feedback;

    /* Timing */
    uint64_t last_update_us;
    float time_since_update_ms;

    /* Statistics */
    neuromod_security_bridge_stats_t stats;
};

/* ============================================================================
 * Helpers
 * ============================================================================ */

static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

int neuromod_security_bridge_default_config(neuromod_security_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_security_bridge_default_config: config is NULL");
        return -1;
    }

    config->enable_lc_security_modulation = true;
    config->enable_vta_security_modulation = true;
    config->enable_raphe_security_modulation = true;
    config->enable_habenula_security_modulation = true;

    config->enable_threat_feedback = true;
    config->enable_rate_limit_feedback = true;
    config->enable_pattern_feedback = true;

    config->ne_sensitivity_weight = NE_THREAT_SENSITIVITY_BOOST;
    config->da_learning_weight = DA_LEARNING_RATE_SCALE;
    config->ht_patience_weight = HT_PATIENCE_FACTOR;
    config->hab_defensive_weight = HAB_DEFENSIVE_BOOST;

    config->update_interval_ms = NEUROMOD_SEC_DEFAULT_UPDATE_MS;
    config->broadcast_on_change = true;

    config->event_buffer_size = NEUROMOD_SEC_MAX_EVENT_BUFFER;

    return 0;
}

neuromod_security_bridge_t* neuromod_security_bridge_create(const neuromod_security_bridge_config_t* config) {
    neuromod_security_bridge_t* bridge = nimcp_calloc(1, sizeof(neuromod_security_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    bridge->magic = NEUROMOD_SECURITY_BRIDGE_MAGIC;

    if (config) {
        bridge->config = *config;
    } else {
        neuromod_security_bridge_default_config(&bridge->config);
    }

    /* Initialize default modulation state */
    bridge->modulation.threat_sensitivity_boost = 1.0f;
    bridge->modulation.vigilance_level = 0.5f;
    bridge->modulation.gain_modulation = 1.0f;
    bridge->modulation.pattern_learning_rate = 1.0f;
    bridge->modulation.policy_adaptation_rate = 1.0f;
    bridge->modulation.tolerance_window = 1.0f;
    bridge->modulation.patience_level = 0.5f;
    bridge->modulation.impulse_inhibition = 0.5f;

    bridge->last_update_us = get_timestamp_us();
    NIMCP_LOGGING_INFO("Created %s bridge", "neuromodulatory_security");
    return bridge;
}

void neuromod_security_bridge_destroy(neuromod_security_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "neuromodulatory_security");
    if (bridge->magic != NEUROMOD_SECURITY_BRIDGE_MAGIC) return;

    if (bridge->connected) {
        neuromod_security_bridge_disconnect(bridge);
    }

    bridge->magic = 0;
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection
 * ============================================================================ */

int neuromod_security_bridge_connect_security(neuromod_security_bridge_t* bridge, nimcp_security_context_t security) {
    if (!bridge || bridge->magic != NEUROMOD_SECURITY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_security_bridge_connect_security: bridge is NULL");
        return -1;
    }

    bridge->security = security;
    bridge->connected = true;

    return 0;
}

int neuromod_security_bridge_disconnect(neuromod_security_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_SECURITY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_security_bridge_disconnect: bridge is NULL");
        return -1;
    }

    bridge->security = NULL;
    bridge->connected = false;

    return 0;
}

bool neuromod_security_bridge_is_connected(const neuromod_security_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_SECURITY_BRIDGE_MAGIC) {
        return false;
    }
    return bridge->connected;
}

/* ============================================================================
 * Adapter Registration
 * ============================================================================ */

int neuromod_security_bridge_register_lc(neuromod_security_bridge_t* bridge, nimcp_lc_adapter_t adapter) {
    if (!bridge || bridge->magic != NEUROMOD_SECURITY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_security_bridge_register_lc: bridge is NULL");
        return -1;
    }
    bridge->lc_adapter = adapter;
    return 0;
}

int neuromod_security_bridge_register_vta(neuromod_security_bridge_t* bridge, nimcp_vta_adapter_t adapter) {
    if (!bridge || bridge->magic != NEUROMOD_SECURITY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_security_bridge_register_vta: bridge is NULL");
        return -1;
    }
    bridge->vta_adapter = adapter;
    return 0;
}

int neuromod_security_bridge_register_raphe(neuromod_security_bridge_t* bridge, nimcp_raphe_adapter_t adapter) {
    if (!bridge || bridge->magic != NEUROMOD_SECURITY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_security_bridge_register_raphe: bridge is NULL");
        return -1;
    }
    bridge->raphe_adapter = adapter;
    return 0;
}

int neuromod_security_bridge_register_habenula(neuromod_security_bridge_t* bridge, nimcp_habenula_adapter_t adapter) {
    if (!bridge || bridge->magic != NEUROMOD_SECURITY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_security_bridge_register_habenula: bridge is NULL");
        return -1;
    }
    bridge->habenula_adapter = adapter;
    return 0;
}

/* ============================================================================
 * Update and Processing
 * ============================================================================ */

int neuromod_security_bridge_update(neuromod_security_bridge_t* bridge, float delta_ms) {
    if (!bridge || bridge->magic != NEUROMOD_SECURITY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_security_bridge_update: bridge is NULL");
        return -1;
    }

    bridge->time_since_update_ms += delta_ms;
    bridge->modulation.timestamp_us = get_timestamp_us();

    return 0;
}

int neuromod_security_bridge_process_events(neuromod_security_bridge_t* bridge, uint32_t max_events) {
    if (!bridge || bridge->magic != NEUROMOD_SECURITY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_security_bridge_process_events: bridge is NULL");
        return -1;
    }

    /* Process pending security events - stub for now */
    (void)max_events;
    return 0;
}

/* ============================================================================
 * Neuromodulatory -> Security Modulation
 * ============================================================================ */

int neuromod_security_apply_arousal(neuromod_security_bridge_t* bridge, const neuromod_sec_arousal_payload_t* payload) {
    if (!bridge || bridge->magic != NEUROMOD_SECURITY_BRIDGE_MAGIC || !payload) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_security_apply_arousal: required parameter is NULL (bridge, payload)");
        return -1;
    }
    if (!bridge->config.enable_lc_security_modulation) return 0;

    /* Apply arousal effects to security modulation state */
    bridge->modulation.threat_sensitivity_boost = 1.0f +
        (payload->arousal_level * bridge->config.ne_sensitivity_weight);
    bridge->modulation.vigilance_level = nimcp_clampf(payload->vigilance, 0.0f, 1.0f);
    bridge->modulation.gain_modulation = 1.0f + (payload->sensitivity_boost * 0.5f);
    bridge->modulation.phasic_alert_active = payload->phasic_burst;

    bridge->stats.lc_modulations_sent++;
    bridge->stats.total_events_sent++;
    bridge->stats.last_activity_us = get_timestamp_us();

    return 0;
}

int neuromod_security_apply_learning(neuromod_security_bridge_t* bridge, const neuromod_sec_learning_payload_t* payload) {
    if (!bridge || bridge->magic != NEUROMOD_SECURITY_BRIDGE_MAGIC || !payload) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_security_apply_learning: required parameter is NULL (bridge, payload)");
        return -1;
    }
    if (!bridge->config.enable_vta_security_modulation) return 0;

    /* Apply learning signals to security modulation state */
    bridge->modulation.pattern_learning_rate = 1.0f +
        (fabsf(payload->rpe) * bridge->config.da_learning_weight);
    bridge->modulation.policy_adaptation_rate = payload->adaptation_rate;
    bridge->modulation.last_rpe = payload->rpe;
    bridge->modulation.safety_confirmed = payload->positive_outcome;

    bridge->stats.vta_modulations_sent++;
    bridge->stats.total_events_sent++;
    bridge->stats.last_activity_us = get_timestamp_us();

    return 0;
}

int neuromod_security_apply_patience(neuromod_security_bridge_t* bridge, const neuromod_sec_patience_payload_t* payload) {
    if (!bridge || bridge->magic != NEUROMOD_SECURITY_BRIDGE_MAGIC || !payload) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_security_apply_patience: required parameter is NULL (bridge, payload)");
        return -1;
    }
    if (!bridge->config.enable_raphe_security_modulation) return 0;

    /* Apply patience/mood effects to security modulation state */
    bridge->modulation.tolerance_window = 1.0f +
        (payload->patience * bridge->config.ht_patience_weight);
    bridge->modulation.impulse_inhibition = nimcp_clampf(payload->impulse_inhibition, 0.0f, 1.0f);
    bridge->modulation.patience_level = nimcp_clampf(payload->patience, 0.0f, 1.0f);
    bridge->modulation.false_positive_threshold = payload->tolerance_boost;

    bridge->stats.raphe_modulations_sent++;
    bridge->stats.total_events_sent++;
    bridge->stats.last_activity_us = get_timestamp_us();

    return 0;
}

int neuromod_security_apply_aversive(neuromod_security_bridge_t* bridge, const neuromod_sec_aversive_payload_t* payload) {
    if (!bridge || bridge->magic != NEUROMOD_SECURITY_BRIDGE_MAGIC || !payload) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_security_apply_aversive: required parameter is NULL (bridge, payload)");
        return -1;
    }
    if (!bridge->config.enable_habenula_security_modulation) return 0;

    /* Apply aversive signals to security modulation state */
    bridge->modulation.defensive_boost = 1.0f +
        (payload->punishment_strength * bridge->config.hab_defensive_weight);
    bridge->modulation.avoidance_drive = nimcp_clampf(payload->avoidance_drive, 0.0f, 1.0f);
    bridge->modulation.punishment_signal = payload->punishment_strength;
    bridge->modulation.quarantine_mode = payload->quarantine_request;

    bridge->stats.habenula_modulations_sent++;
    bridge->stats.total_events_sent++;
    bridge->stats.last_activity_us = get_timestamp_us();

    return 0;
}

/* ============================================================================
 * Security -> Neuromodulatory Feedback
 * ============================================================================ */

int neuromod_security_report_threat(neuromod_security_bridge_t* bridge, const neuromod_sec_threat_payload_t* payload) {
    if (!bridge || bridge->magic != NEUROMOD_SECURITY_BRIDGE_MAGIC || !payload) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_security_report_threat: required parameter is NULL (bridge, payload)");
        return -1;
    }
    if (!bridge->config.enable_threat_feedback) return 0;

    /* Update security feedback state */
    bridge->feedback.current_threat_level = nimcp_clampf(payload->threat_level, 0.0f, 1.0f);
    bridge->feedback.threats_detected = payload->threat_count;
    bridge->feedback.under_attack = payload->urgent;
    bridge->feedback.last_update_us = get_timestamp_us();

    bridge->stats.threat_events_received++;
    bridge->stats.total_events_received++;
    bridge->stats.last_activity_us = get_timestamp_us();

    /* Track correlation with arousal */
    if (bridge->modulation.vigilance_level > 0.7f) {
        bridge->stats.threats_detected_during_high_arousal++;
    }

    return 0;
}

/* ============================================================================
 * State Access
 * ============================================================================ */

int neuromod_security_bridge_get_modulation(const neuromod_security_bridge_t* bridge, neuromod_security_modulation_t* modulation) {
    if (!bridge || bridge->magic != NEUROMOD_SECURITY_BRIDGE_MAGIC || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_security_bridge_get_modulation: required parameter is NULL (bridge, modulation)");
        return -1;
    }
    *modulation = bridge->modulation;
    return 0;
}

int neuromod_security_bridge_get_feedback(const neuromod_security_bridge_t* bridge, neuromod_security_feedback_t* feedback) {
    if (!bridge || bridge->magic != NEUROMOD_SECURITY_BRIDGE_MAGIC || !feedback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_security_bridge_get_feedback: required parameter is NULL (bridge, feedback)");
        return -1;
    }
    *feedback = bridge->feedback;
    return 0;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

int neuromod_security_bridge_get_stats(const neuromod_security_bridge_t* bridge, neuromod_security_bridge_stats_t* stats) {
    if (!bridge || bridge->magic != NEUROMOD_SECURITY_BRIDGE_MAGIC || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_security_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

int neuromod_security_bridge_reset_stats(neuromod_security_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_SECURITY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_security_bridge_reset_stats: bridge is NULL");
        return -1;
    }
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    return 0;
}

/* ============================================================================
 * Diagnostics
 * ============================================================================ */

static const char* event_names[] = {
    "AROUSAL_BOOST", "VIGILANCE_INCREASE", "PHASIC_ALERT",
    "PATTERN_LEARN", "ADAPTATION_SIGNAL", "REWARD_SAFETY",
    "PATIENCE_INCREASE", "IMPULSE_CONTROL", "FP_REDUCTION",
    "AVERSIVE_TRIGGER", "PUNISHMENT_SIGNAL", "QUARANTINE_REQUEST",
    "THREAT_DETECTED", "PATTERN_MATCHED", "RATE_LIMIT_HIT", "ATTACK_BLOCKED"
};

const char* neuromod_sec_event_name(neuromod_sec_event_t event) {
    if (event >= NEUROMOD_SEC_EVENT_COUNT) return "UNKNOWN";
    return event_names[event];
}

void neuromod_security_bridge_print_summary(const neuromod_security_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_SECURITY_BRIDGE_MAGIC) {
        printf("Neuromodulatory Security Bridge: NULL or invalid\n");
        return;
    }

    printf("Neuromodulatory Security Bridge Summary:\n");
    printf("  Connected: %s\n", bridge->connected ? "yes" : "no");
    printf("  Adapters registered:\n");
    printf("    LC: %s, VTA: %s, Raphe: %s, Habenula: %s\n",
           bridge->lc_adapter ? "yes" : "no",
           bridge->vta_adapter ? "yes" : "no",
           bridge->raphe_adapter ? "yes" : "no",
           bridge->habenula_adapter ? "yes" : "no");
    printf("  Modulations sent:\n");
    printf("    LC: %u, VTA: %u, Raphe: %u, Habenula: %u\n",
           bridge->stats.lc_modulations_sent,
           bridge->stats.vta_modulations_sent,
           bridge->stats.raphe_modulations_sent,
           bridge->stats.habenula_modulations_sent);
    printf("  Security feedback:\n");
    printf("    Threats: %u, Patterns: %u, Rate limits: %u\n",
           bridge->stats.threat_events_received,
           bridge->stats.pattern_events_received,
           bridge->stats.rate_limit_events_received);
    printf("  Current modulation state:\n");
    printf("    Sensitivity: %.2f, Vigilance: %.2f, Learning: %.2f\n",
           bridge->modulation.threat_sensitivity_boost,
           bridge->modulation.vigilance_level,
           bridge->modulation.pattern_learning_rate);
    printf("    Patience: %.2f, Defense: %.2f, Quarantine: %s\n",
           bridge->modulation.patience_level,
           bridge->modulation.defensive_boost,
           bridge->modulation.quarantine_mode ? "yes" : "no");
}
