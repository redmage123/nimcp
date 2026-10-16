/**
 * @file nimcp_soma_medulla_bridge.c
 * @brief Somatosensory-Medulla Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-12
 *
 * @author NIMCP Development Team
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/somatosensory/bridges/nimcp_soma_medulla_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdatomic.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(soma_medulla_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "SOMA_MEDULLA_BRIDGE"


/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct soma_medulla_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    soma_medulla_config_t config;
    nimcp_somatosensory_t* soma;
    void* medulla;

    bool is_connected;
    soma_medulla_status_t status;

    /* Autonomic state */
    soma_medulla_autonomic_t current_autonomic;

    /* Active reflex */
    soma_medulla_reflex_response_t active_reflex;
    bool reflex_pending;

    soma_medulla_stats_t stats;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint64_t get_timestamp(void) {
    static _Atomic uint64_t counter = 0;
    return counter++;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int soma_medulla_default_config(soma_medulla_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_medulla_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(soma_medulla_config_t));

    config->pain_urgent_threshold = SOMA_MEDULLA_PAIN_URGENT_THRESHOLD;
    config->temp_danger_low = SOMA_MEDULLA_TEMP_DANGER_LOW;
    config->temp_danger_high = SOMA_MEDULLA_TEMP_DANGER_HIGH;
    config->reflex_latency_ms = SOMA_MEDULLA_REFLEX_LATENCY_MS;
    config->enable_withdrawal_reflex = true;
    config->enable_autonomic_response = true;
    config->enable_logging = false;

    return 0;
}

soma_medulla_bridge_t* soma_medulla_bridge_create(const soma_medulla_config_t* config) {
    soma_medulla_bridge_t* bridge = (soma_medulla_bridge_t*)nimcp_calloc(1, sizeof(soma_medulla_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "soma_medulla_bridge_create: allocation failed");
        return NULL;
    }

    bridge_base_init(&bridge->base, 0, "soma_medulla");

    if (config) {
        memcpy(&bridge->config, config, sizeof(soma_medulla_config_t));
    } else {
        soma_medulla_default_config(&bridge->config);
    }

    bridge->is_connected = false;
    bridge->status = SOMA_MEDULLA_STATUS_IDLE;
    bridge->reflex_pending = false;

    NIMCP_LOGGING_INFO("Created %s bridge", "soma_medulla");
    return bridge;
}

void soma_medulla_bridge_destroy(soma_medulla_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "soma_medulla");
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

int soma_medulla_connect(soma_medulla_bridge_t* bridge, nimcp_somatosensory_t* soma, void* medulla) {
    if (!bridge || !soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_medulla_connect: required parameter is NULL (bridge, soma)");
        return -1;
    }

    bridge->soma = soma;
    bridge->medulla = medulla;
    bridge->is_connected = true;
    bridge->status = SOMA_MEDULLA_STATUS_IDLE;

    return 0;
}

int soma_medulla_disconnect(soma_medulla_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_medulla_disconnect: bridge is NULL");
        return -1;
    }

    bridge->soma = NULL;
    bridge->medulla = NULL;
    bridge->is_connected = false;

    return 0;
}

bool soma_medulla_is_connected(const soma_medulla_bridge_t* bridge) {
    return bridge && bridge->is_connected;
}

/* ============================================================================
 * Pain/Nociception API Implementation
 * ============================================================================ */

int soma_medulla_send_pain_urgent(soma_medulla_bridge_t* bridge,
                                  const soma_medulla_pain_t* pain) {
    if (!bridge || !bridge->is_connected || !pain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_medulla_is_connected: required parameter is NULL (bridge, bridge->is_connected, pain)");
        return -1;
    }

    bridge->stats.pain_signals++;

    if (pain->intensity >= bridge->config.pain_urgent_threshold &&
        pain->requires_withdrawal &&
        bridge->config.enable_withdrawal_reflex) {

        /* Trigger withdrawal reflex */
        bridge->active_reflex.type = SOMA_MEDULLA_REFLEX_WITHDRAWAL;
        bridge->active_reflex.affected = pain->region;
        bridge->active_reflex.strength = pain->intensity;
        bridge->active_reflex.latency_ms = bridge->config.reflex_latency_ms;
        bridge->active_reflex.completed = false;
        bridge->reflex_pending = true;

        bridge->status = SOMA_MEDULLA_STATUS_REFLEX_ACTIVE;
        bridge->stats.reflexes_triggered++;
    }

    return 0;
}

int soma_medulla_request_withdrawal(soma_medulla_bridge_t* bridge, body_segment_t region) {
    if (!bridge || !bridge->is_connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_medulla_request_withdrawal: required parameter is NULL (bridge, bridge->is_connected)");
        return -1;
    }
    if (!bridge->config.enable_withdrawal_reflex) {
        return 0;  /* Feature disabled, not an error */
    }

    bridge->active_reflex.type = SOMA_MEDULLA_REFLEX_WITHDRAWAL;
    bridge->active_reflex.affected = region;
    bridge->active_reflex.strength = 1.0f;
    bridge->active_reflex.latency_ms = bridge->config.reflex_latency_ms;
    bridge->active_reflex.completed = false;
    bridge->reflex_pending = true;

    bridge->status = SOMA_MEDULLA_STATUS_REFLEX_ACTIVE;
    bridge->stats.reflexes_triggered++;

    return 0;
}

int soma_medulla_check_reflex_status(soma_medulla_bridge_t* bridge,
                                     soma_medulla_reflex_response_t* response) {
    if (!bridge || !response) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_medulla_request_withdrawal: required parameter is NULL (bridge, response)");
        return -1;
    }

    if (bridge->reflex_pending) {
        memcpy(response, &bridge->active_reflex, sizeof(soma_medulla_reflex_response_t));

        /* Simulate reflex completion */
        bridge->active_reflex.completed = true;
        bridge->reflex_pending = false;
        bridge->status = SOMA_MEDULLA_STATUS_IDLE;

        /* Update average latency */
        float new_latency = bridge->stats.avg_reflex_latency_ms * 0.9f +
            (float)bridge->config.reflex_latency_ms * 0.1f;
        if (isfinite(new_latency)) {
            bridge->stats.avg_reflex_latency_ms = new_latency;
        }
    } else {
        memset(response, 0, sizeof(soma_medulla_reflex_response_t));
    }

    return 0;
}

/* ============================================================================
 * Temperature API Implementation
 * ============================================================================ */

int soma_medulla_send_temp_extreme(soma_medulla_bridge_t* bridge,
                                   const soma_medulla_temp_extreme_t* temp) {
    if (!bridge || !bridge->is_connected || !temp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_medulla_request_withdrawal: required parameter is NULL (bridge, bridge->is_connected, temp)");
        return -1;
    }

    bridge->stats.temp_extremes++;

    if (bridge->config.enable_autonomic_response) {
        /* Trigger thermoregulatory response */
        if (temp->is_cold) {
            bridge->current_autonomic.perspiration = 0.0f;
            bridge->current_autonomic.heart_rate_delta = 10.0f;  /* Increase HR */
        } else {
            bridge->current_autonomic.perspiration = 0.8f;
            bridge->current_autonomic.heart_rate_delta = 5.0f;
        }
        bridge->current_autonomic.timestamp = get_timestamp();

        bridge->status = SOMA_MEDULLA_STATUS_AUTONOMIC_ACTIVE;
        bridge->stats.autonomic_adjustments++;
    }

    return 0;
}

int soma_medulla_request_thermoregulation(soma_medulla_bridge_t* bridge, float target_temp) {
    if (!bridge || !bridge->is_connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_medulla_request_thermoregulation: required parameter is NULL (bridge, bridge->is_connected)");
        return -1;
    }
    (void)target_temp;

    bridge->stats.autonomic_adjustments++;

    return 0;
}

/* ============================================================================
 * Autonomic API Implementation
 * ============================================================================ */

int soma_medulla_trigger_autonomic(soma_medulla_bridge_t* bridge,
                                   const soma_medulla_autonomic_t* adj) {
    if (!bridge || !bridge->is_connected || !adj) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_medulla_request_thermoregulation: required parameter is NULL (bridge, bridge->is_connected, adj)");
        return -1;
    }
    if (!bridge->config.enable_autonomic_response) {
        return 0;  /* Feature disabled, not an error */
    }

    memcpy(&bridge->current_autonomic, adj, sizeof(soma_medulla_autonomic_t));
    bridge->current_autonomic.timestamp = get_timestamp();

    bridge->status = SOMA_MEDULLA_STATUS_AUTONOMIC_ACTIVE;
    bridge->stats.autonomic_adjustments++;

    return 0;
}

int soma_medulla_get_autonomic_state(soma_medulla_bridge_t* bridge,
                                     soma_medulla_autonomic_t* state) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_medulla_request_thermoregulation: required parameter is NULL (bridge, state)");
        return -1;
    }

    memcpy(state, &bridge->current_autonomic, sizeof(soma_medulla_autonomic_t));

    return 0;
}

/* ============================================================================
 * Statistics API Implementation
 * ============================================================================ */

int soma_medulla_get_stats(const soma_medulla_bridge_t* bridge, soma_medulla_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_medulla_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    memcpy(stats, &bridge->stats, sizeof(soma_medulla_stats_t));
    return 0;
}

int soma_medulla_reset_stats(soma_medulla_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_medulla_reset_stats: bridge is NULL");
        return -1;
    }
    memset(&bridge->stats, 0, sizeof(soma_medulla_stats_t));
    return 0;
}

void soma_medulla_print_summary(const soma_medulla_bridge_t* bridge) {
    if (!bridge) return;

    printf("=== Somatosensory-Medulla Bridge Summary ===\n");
    printf("Connected: %s\n", bridge->is_connected ? "Yes" : "No");
    printf("Pain Signals: %lu\n", (unsigned long)bridge->stats.pain_signals);
    printf("Temp Extremes: %lu\n", (unsigned long)bridge->stats.temp_extremes);
    printf("Reflexes Triggered: %lu\n", (unsigned long)bridge->stats.reflexes_triggered);
    printf("Autonomic Adjustments: %lu\n", (unsigned long)bridge->stats.autonomic_adjustments);
    printf("Avg Reflex Latency: %.2f ms\n", bridge->stats.avg_reflex_latency_ms);
    printf("============================================\n");
}
