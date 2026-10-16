/**
 * @file nimcp_neuromodulatory_cognitive_bridge.c
 * @brief Implementation of Unified Neuromodulatory-Cognitive Hub Bridge
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/nimcp_neuromodulatory_cognitive_bridge.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(neuromodulatory_cognitive_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "NEUROMODULATORY_COGNITIVE_BRIDGE"


/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct neuromod_cognitive_hub_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    neuromod_cognitive_hub_config_t config;

    /* Adapter connections */
    nimcp_lc_adapter_t lc_adapter;
    nimcp_vta_adapter_t vta_adapter;
    nimcp_raphe_adapter_t raphe_adapter;
    nimcp_habenula_adapter_t habenula_adapter;

    /* Cognitive hub connection */
    cognitive_integration_hub_t cog_hub;
    bool connected;

    /* State cache */
    neuromod_cog_state_t state;
    neuromod_cognitive_feedback_t feedback;

    /* Timing */
    uint64_t last_broadcast_us;
    float time_since_broadcast_ms;

    /* Statistics */
    neuromod_cognitive_hub_stats_t stats;
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

int neuromod_cognitive_hub_default_config(neuromod_cognitive_hub_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_cognitive_hub_default_config: config is NULL");
        return -1;
    }

    config->enable_lc_integration = true;
    config->enable_vta_integration = true;
    config->enable_raphe_integration = true;
    config->enable_habenula_integration = true;

    config->update_interval_ms = NEUROMOD_COG_DEFAULT_UPDATE_MS;
    config->broadcast_on_change = true;

    config->enable_cognitive_feedback = true;
    config->cognitive_weight = 0.3f;

    config->subscribe_emotion_updates = true;
    config->subscribe_attention_updates = true;
    config->subscribe_decision_updates = true;
    config->subscribe_learning_updates = true;

    config->event_buffer_size = NEUROMOD_COG_MAX_EVENT_BUFFER;

    return 0;
}

neuromod_cognitive_hub_bridge_t* neuromod_cognitive_hub_create(const neuromod_cognitive_hub_config_t* config) {
    neuromod_cognitive_hub_bridge_t* bridge = nimcp_calloc(1, sizeof(neuromod_cognitive_hub_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        neuromod_cognitive_hub_default_config(&bridge->config);
    }

    bridge->last_broadcast_us = get_timestamp_us();
    NIMCP_LOGGING_INFO("Created %s bridge", "neuromodulatory_cognitive");
    return bridge;
}

void neuromod_cognitive_hub_destroy(neuromod_cognitive_hub_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "neuromodulatory_cognitive");

    if (bridge->connected) {
        neuromod_cognitive_hub_disconnect(bridge);
    }

    nimcp_free(bridge);
}

/* ============================================================================
 * Connection
 * ============================================================================ */

int neuromod_cognitive_hub_connect(
    neuromod_cognitive_hub_bridge_t* bridge,
    cognitive_integration_hub_t cog_hub
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_cognitive_hub_connect: bridge is NULL");
        return -1;
    }

    bridge->cog_hub = cog_hub;
    bridge->connected = true;

    return 0;
}

int neuromod_cognitive_hub_disconnect(neuromod_cognitive_hub_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_cognitive_hub_disconnect: bridge is NULL");
        return -1;
    }

    bridge->cog_hub = NULL;
    bridge->connected = false;

    return 0;
}

bool neuromod_cognitive_hub_is_connected(const neuromod_cognitive_hub_bridge_t* bridge) {
    return bridge ? bridge->connected : false;
}

/* ============================================================================
 * Adapter Registration
 * ============================================================================ */

int neuromod_cognitive_hub_register_lc(neuromod_cognitive_hub_bridge_t* bridge, nimcp_lc_adapter_t adapter) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_cognitive_hub_register_lc: bridge is NULL");
        return -1;
    }
    bridge->lc_adapter = adapter;
    return 0;
}

int neuromod_cognitive_hub_register_vta(neuromod_cognitive_hub_bridge_t* bridge, nimcp_vta_adapter_t adapter) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_cognitive_hub_register_vta: bridge is NULL");
        return -1;
    }
    bridge->vta_adapter = adapter;
    return 0;
}

int neuromod_cognitive_hub_register_raphe(neuromod_cognitive_hub_bridge_t* bridge, nimcp_raphe_adapter_t adapter) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_cognitive_hub_register_raphe: bridge is NULL");
        return -1;
    }
    bridge->raphe_adapter = adapter;
    return 0;
}

int neuromod_cognitive_hub_register_habenula(neuromod_cognitive_hub_bridge_t* bridge, nimcp_habenula_adapter_t adapter) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_cognitive_hub_register_habenula: bridge is NULL");
        return -1;
    }
    bridge->habenula_adapter = adapter;
    return 0;
}

/* ============================================================================
 * Update and Processing
 * ============================================================================ */

int neuromod_cognitive_hub_update(neuromod_cognitive_hub_bridge_t* bridge, float delta_ms) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_cognitive_hub_update: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    bridge->time_since_broadcast_ms += delta_ms;

    /* Auto-broadcast if interval elapsed */
    if (bridge->time_since_broadcast_ms >= bridge->config.update_interval_ms) {
        neuromod_cognitive_hub_broadcast_state(bridge);
        bridge->time_since_broadcast_ms = 0.0f;
    }

    return 0;
}

int neuromod_cognitive_hub_process_events(neuromod_cognitive_hub_bridge_t* bridge, uint32_t max_events) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_cognitive_hub_process_events: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    /* Process incoming cognitive events - stub for now */
    (void)max_events;
    return 0;
}

/* ============================================================================
 * Publishing
 * ============================================================================ */

int neuromod_cognitive_hub_publish_arousal(neuromod_cognitive_hub_bridge_t* bridge, const neuromod_arousal_payload_t* payload) {
    if (!bridge || !bridge->connected || !payload) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_cognitive_hub_publish_arousal: required parameter is NULL (bridge, bridge->connected, payload)");
        return -1;
    }
    if (!bridge->config.enable_lc_integration) return 0;

    bridge->state.arousal = payload->arousal_level;
    bridge->state.alertness = payload->alertness;
    bridge->state.gain_modulation = payload->gain_factor;
    bridge->state.phasic_mode = payload->phasic_burst;

    bridge->stats.lc_events_published++;
    bridge->stats.total_events_published++;
    bridge->stats.last_publish_time_us = get_timestamp_us();

    return 0;
}

int neuromod_cognitive_hub_publish_reward(neuromod_cognitive_hub_bridge_t* bridge, const neuromod_reward_payload_t* payload) {
    if (!bridge || !bridge->connected || !payload) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_cognitive_hub_publish_reward: required parameter is NULL (bridge, bridge->connected, payload)");
        return -1;
    }
    if (!bridge->config.enable_vta_integration) return 0;

    bridge->state.last_rpe = payload->rpe;
    bridge->state.motivation = payload->motivation;
    bridge->state.value_estimate = payload->value;
    bridge->state.reward_predicted = payload->positive_rpe;

    bridge->stats.vta_events_published++;
    bridge->stats.total_events_published++;
    bridge->stats.last_publish_time_us = get_timestamp_us();

    return 0;
}

int neuromod_cognitive_hub_publish_mood(neuromod_cognitive_hub_bridge_t* bridge, const neuromod_mood_payload_t* payload) {
    if (!bridge || !bridge->connected || !payload) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_cognitive_hub_publish_mood: required parameter is NULL (bridge, bridge->connected, payload)");
        return -1;
    }
    if (!bridge->config.enable_raphe_integration) return 0;

    bridge->state.mood = payload->mood_level;
    bridge->state.impulse_control = payload->impulse_inhibition;
    bridge->state.patience = payload->patience;
    bridge->state.social_confidence = payload->social_confidence;

    bridge->stats.raphe_events_published++;
    bridge->stats.total_events_published++;
    bridge->stats.last_publish_time_us = get_timestamp_us();

    return 0;
}

int neuromod_cognitive_hub_publish_aversive(neuromod_cognitive_hub_bridge_t* bridge, const neuromod_aversive_payload_t* payload) {
    if (!bridge || !bridge->connected || !payload) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_cognitive_hub_publish_aversive: required parameter is NULL (bridge, bridge->connected, payload)");
        return -1;
    }
    if (!bridge->config.enable_habenula_integration) return 0;

    bridge->state.negative_rpe = payload->negative_rpe;
    bridge->state.avoidance_drive = payload->avoidance_strength;
    bridge->state.disappointment = payload->disappointment;
    bridge->state.punishment_detected = payload->urgent;

    bridge->stats.habenula_events_published++;
    bridge->stats.total_events_published++;
    bridge->stats.last_publish_time_us = get_timestamp_us();

    return 0;
}

int neuromod_cognitive_hub_publish_plasticity_gate(neuromod_cognitive_hub_bridge_t* bridge, const neuromod_plasticity_gate_payload_t* payload) {
    if (!bridge || !bridge->connected || !payload) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_cognitive_hub_publish_plasticity_gate: required parameter is NULL (bridge, bridge->connected, payload)");
        return -1;
    }

    bridge->stats.plasticity_gates_sent++;
    bridge->stats.total_events_published++;
    bridge->stats.last_publish_time_us = get_timestamp_us();

    return 0;
}

/* ============================================================================
 * State Broadcast
 * ============================================================================ */

int neuromod_cognitive_hub_broadcast_state(neuromod_cognitive_hub_bridge_t* bridge) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_cognitive_hub_broadcast_state: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    bridge->state.timestamp_us = get_timestamp_us();

    /*
     * Note: State is populated via publish functions (publish_arousal, publish_reward, etc.)
     * rather than querying adapters, since adapter state types have limited neuromodulatory
     * detail. The adapters track statistics while the hub tracks neurotransmitter state.
     */

    /* Update statistics for LC */
    if (bridge->lc_adapter && bridge->config.enable_lc_integration) {
        bridge->stats.lc_events_published++;
    }

    /* Update statistics for VTA */
    if (bridge->vta_adapter && bridge->config.enable_vta_integration) {
        bridge->stats.vta_events_published++;
    }

    /* Update statistics for Raphe */
    if (bridge->raphe_adapter && bridge->config.enable_raphe_integration) {
        bridge->stats.raphe_events_published++;
    }

    /* Update statistics for Habenula */
    if (bridge->habenula_adapter && bridge->config.enable_habenula_integration) {
        bridge->stats.habenula_events_published++;
    }

    bridge->stats.total_events_published++;
    bridge->last_broadcast_us = bridge->state.timestamp_us;

    return 0;
}

/* ============================================================================
 * State Access
 * ============================================================================ */

int neuromod_cognitive_hub_get_state(const neuromod_cognitive_hub_bridge_t* bridge, neuromod_cog_state_t* state) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_cognitive_hub_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    *state = bridge->state;
    return 0;
}

int neuromod_cognitive_hub_get_feedback(const neuromod_cognitive_hub_bridge_t* bridge, neuromod_cognitive_feedback_t* feedback) {
    if (!bridge || !feedback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_cognitive_hub_get_feedback: required parameter is NULL (bridge, feedback)");
        return -1;
    }
    *feedback = bridge->feedback;
    return 0;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

int neuromod_cognitive_hub_get_stats(const neuromod_cognitive_hub_bridge_t* bridge, neuromod_cognitive_hub_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_cognitive_hub_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

int neuromod_cognitive_hub_reset_stats(neuromod_cognitive_hub_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_cognitive_hub_reset_stats: bridge is NULL");
        return -1;
    }
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    return 0;
}

/* ============================================================================
 * Diagnostics
 * ============================================================================ */

static const char* center_names[] = {
    "Locus Coeruleus (NE)",
    "Ventral Tegmental Area (DA)",
    "Raphe Nuclei (5-HT)",
    "Habenula"
};

static const char* event_names[] = {
    "AROUSAL_CHANGE", "GAIN_MODULATION", "VIGILANCE_UPDATE", "PHASIC_NE_BURST",
    "RPE_SIGNAL", "MOTIVATION_UPDATE", "VALUE_PREDICTION", "DA_BURST",
    "MOOD_CHANGE", "IMPULSE_CONTROL", "PATIENCE_UPDATE", "SOCIAL_MODULATION",
    "NEGATIVE_RPE", "PUNISHMENT_SIGNAL", "AVOIDANCE_TRIGGER", "DISAPPOINTMENT",
    "PLASTICITY_GATE", "STATE_QUERY"
};

const char* neuromod_center_name(neuromod_center_t center) {
    if (center >= NEUROMOD_CENTER_COUNT) return "UNKNOWN";
    return center_names[center];
}

const char* neuromod_cog_event_name(neuromod_cog_event_t event) {
    if (event >= NEUROMOD_COG_EVENT_COUNT) return "UNKNOWN";
    return event_names[event];
}

void neuromod_cognitive_hub_print_summary(const neuromod_cognitive_hub_bridge_t* bridge) {
    if (!bridge) {
        printf("Neuromodulatory Cognitive Hub Bridge: NULL\n");
        return;
    }

    printf("Neuromodulatory Cognitive Hub Bridge Summary:\n");
    printf("  Connected: %s\n", bridge->connected ? "yes" : "no");
    printf("  Adapters registered:\n");
    printf("    LC: %s, VTA: %s, Raphe: %s, Habenula: %s\n",
           bridge->lc_adapter ? "yes" : "no",
           bridge->vta_adapter ? "yes" : "no",
           bridge->raphe_adapter ? "yes" : "no",
           bridge->habenula_adapter ? "yes" : "no");
    printf("  Events published:\n");
    printf("    LC: %u, VTA: %u, Raphe: %u, Habenula: %u\n",
           bridge->stats.lc_events_published,
           bridge->stats.vta_events_published,
           bridge->stats.raphe_events_published,
           bridge->stats.habenula_events_published);
    printf("    Total: %u, Plasticity gates: %u\n",
           bridge->stats.total_events_published,
           bridge->stats.plasticity_gates_sent);
    printf("  Current state:\n");
    printf("    NE: %.2f, DA: %.2f, 5-HT: %.2f, Habenula: %.2f\n",
           bridge->state.ne_level, bridge->state.da_level,
           bridge->state.ht_level, bridge->state.habenula_activation);
}
