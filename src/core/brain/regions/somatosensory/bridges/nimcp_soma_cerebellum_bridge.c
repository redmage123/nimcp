/**
 * @file nimcp_soma_cerebellum_bridge.c
 * @brief Somatosensory-Cerebellum Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-12
 *
 * @author NIMCP Development Team
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/somatosensory/bridges/nimcp_soma_cerebellum_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(soma_cerebellum_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "SOMA_CEREBELLUM_BRIDGE"


/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct soma_cereb_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    soma_cereb_config_t config;
    nimcp_somatosensory_t* soma;
    void* cerebellum;

    bool is_connected;
    soma_cereb_status_t status;

    /* State tracking */
    soma_cereb_joint_state_t* joint_states;
    uint32_t num_joints;

    float* prediction_buffer;
    uint32_t prediction_dim;

    soma_cereb_stats_t stats;
};

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int soma_cereb_default_config(soma_cereb_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_cereb_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(soma_cereb_config_t));

    config->max_joints = SOMA_CEREB_MAX_JOINTS;
    config->max_effectors = SOMA_CEREB_MAX_EFFECTORS;
    config->prediction_horizon_ms = SOMA_CEREB_PREDICTION_HORIZON;
    config->error_gain = 1.0f;
    config->adaptation_rate = 0.01f;
    config->enable_prediction = true;
    config->enable_adaptation = true;
    config->enable_logging = false;

    return 0;
}

soma_cereb_bridge_t* soma_cereb_bridge_create(const soma_cereb_config_t* config) {
    soma_cereb_bridge_t* bridge = (soma_cereb_bridge_t*)nimcp_calloc(1, sizeof(soma_cereb_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "soma_cereb_bridge_create: allocation failed");
        return NULL;
    }

    bridge_base_init(&bridge->base, 0, "soma_cerebellum");

    if (config) {
        memcpy(&bridge->config, config, sizeof(soma_cereb_config_t));
    } else {
        soma_cereb_default_config(&bridge->config);
    }

    bridge->joint_states = (soma_cereb_joint_state_t*)nimcp_calloc(
        bridge->config.max_joints, sizeof(soma_cereb_joint_state_t));
    if (!bridge->joint_states) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "soma_cereb_bridge_create: bridge->joint_states is NULL");
        return NULL;
    }

    bridge->prediction_dim = 64;
    bridge->prediction_buffer = (float*)nimcp_calloc(bridge->prediction_dim, sizeof(float));
    if (!bridge->prediction_buffer) {
        nimcp_free(bridge->joint_states);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "soma_cereb_bridge_create: bridge->prediction_buffer is NULL");
        return NULL;
    }

    bridge->is_connected = false;
    bridge->status = SOMA_CEREB_STATUS_IDLE;

    NIMCP_LOGGING_INFO("Created %s bridge", "soma_cerebellum");
    return bridge;
}

void soma_cereb_bridge_destroy(soma_cereb_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "soma_cerebellum");

    nimcp_free(bridge->joint_states);
    nimcp_free(bridge->prediction_buffer);
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

int soma_cereb_connect(soma_cereb_bridge_t* bridge, nimcp_somatosensory_t* soma, void* cerebellum) {
    if (!bridge || !soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_cereb_connect: required parameter is NULL (bridge, soma)");
        return -1;
    }

    bridge->soma = soma;
    bridge->cerebellum = cerebellum;
    bridge->is_connected = true;
    bridge->status = SOMA_CEREB_STATUS_IDLE;

    return 0;
}

int soma_cereb_disconnect(soma_cereb_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_cereb_disconnect: bridge is NULL");
        return -1;
    }

    bridge->soma = NULL;
    bridge->cerebellum = NULL;
    bridge->is_connected = false;

    return 0;
}

bool soma_cereb_is_connected(const soma_cereb_bridge_t* bridge) {
    return bridge && bridge->is_connected;
}

/* ============================================================================
 * Proprioceptive API Implementation
 * ============================================================================ */

int soma_cereb_send_proprio(soma_cereb_bridge_t* bridge,
                            const soma_cereb_joint_state_t* joints,
                            uint32_t num_joints) {
    if (!bridge || !bridge->is_connected || !joints) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_cereb_is_connected: required parameter is NULL (bridge, bridge->is_connected, joints)");
        return -1;
    }
    if (num_joints > bridge->config.max_joints) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "soma_cereb_is_connected: validation failed");
        return -1;
    }

    memcpy(bridge->joint_states, joints, num_joints * sizeof(soma_cereb_joint_state_t));
    bridge->num_joints = num_joints;

    bridge->stats.proprio_updates++;

    return 0;
}

int soma_cereb_send_touch_during_move(soma_cereb_bridge_t* bridge,
                                      body_segment_t region,
                                      float intensity) {
    if (!bridge || !bridge->is_connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_cereb_is_connected: required parameter is NULL (bridge, bridge->is_connected)");
        return -1;
    }
    (void)region;
    (void)intensity;

    bridge->stats.touch_events++;

    return 0;
}

int soma_cereb_send_balance_update(soma_cereb_bridge_t* bridge,
                                   float roll, float pitch, float yaw) {
    if (!bridge || !bridge->is_connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_cereb_is_connected: required parameter is NULL (bridge, bridge->is_connected)");
        return -1;
    }
    (void)roll;
    (void)pitch;
    (void)yaw;

    return 0;
}

/* ============================================================================
 * Prediction API Implementation
 * ============================================================================ */

int soma_cereb_send_prediction_error(soma_cereb_bridge_t* bridge,
                                     const soma_cereb_prediction_error_t* error) {
    if (!bridge || !bridge->is_connected || !error) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_cereb_is_connected: required parameter is NULL (bridge, bridge->is_connected, error)");
        return -1;
    }

    bridge->stats.prediction_errors++;
    {
        float new_avg = bridge->stats.avg_prediction_error * 0.9f + error->error_magnitude * 0.1f;
        if (isfinite(new_avg)) {
            bridge->stats.avg_prediction_error = new_avg;
        }
    }

    if (bridge->config.enable_adaptation) {
        bridge->stats.adaptation_progress += bridge->config.adaptation_rate;
        if (bridge->stats.adaptation_progress > 1.0f) {
            bridge->stats.adaptation_progress = 1.0f;
        }
    }

    return 0;
}

int soma_cereb_receive_efference(soma_cereb_bridge_t* bridge,
                                 soma_cereb_efference_t* efference) {
    if (!bridge || !bridge->is_connected || !efference) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_cereb_is_connected: required parameter is NULL (bridge, bridge->is_connected, efference)");
        return -1;
    }

    bridge->stats.efference_copies++;

    return 0;
}

int soma_cereb_compute_prediction(soma_cereb_bridge_t* bridge,
                                  const soma_cereb_efference_t* efference,
                                  float* prediction) {
    if (!bridge || !efference || !prediction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_cereb_is_connected: required parameter is NULL (bridge, efference, prediction)");
        return -1;
    }
    if (!bridge->config.enable_prediction) {
        return 0;  /* Feature disabled, not an error */
    }

    /* Simple forward model prediction */
    for (uint32_t i = 0; i < efference->prediction_dim && i < bridge->prediction_dim; i++) {
        prediction[i] = efference->predicted_sensory[i];
    }

    return 0;
}

/* ============================================================================
 * Coordination API Implementation
 * ============================================================================ */

int soma_cereb_request_coordination(soma_cereb_bridge_t* bridge, uint32_t task_id) {
    if (!bridge || !bridge->is_connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_cereb_request_coordination: required parameter is NULL (bridge, bridge->is_connected)");
        return -1;
    }
    (void)task_id;

    bridge->status = SOMA_CEREB_STATUS_COORDINATING;

    return 0;
}

int soma_cereb_report_movement_complete(soma_cereb_bridge_t* bridge,
                                        uint32_t task_id, bool success) {
    if (!bridge || !bridge->is_connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_cereb_request_coordination: required parameter is NULL (bridge, bridge->is_connected)");
        return -1;
    }
    (void)task_id;
    (void)success;

    bridge->status = SOMA_CEREB_STATUS_IDLE;

    return 0;
}

/* ============================================================================
 * Statistics API Implementation
 * ============================================================================ */

int soma_cereb_get_stats(const soma_cereb_bridge_t* bridge, soma_cereb_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_cereb_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    memcpy(stats, &bridge->stats, sizeof(soma_cereb_stats_t));
    return 0;
}

int soma_cereb_reset_stats(soma_cereb_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_cereb_reset_stats: bridge is NULL");
        return -1;
    }
    memset(&bridge->stats, 0, sizeof(soma_cereb_stats_t));
    return 0;
}

void soma_cereb_print_summary(const soma_cereb_bridge_t* bridge) {
    if (!bridge) return;

    printf("=== Somatosensory-Cerebellum Bridge Summary ===\n");
    printf("Connected: %s\n", bridge->is_connected ? "Yes" : "No");
    printf("Proprio Updates: %lu\n", (unsigned long)bridge->stats.proprio_updates);
    printf("Touch Events: %lu\n", (unsigned long)bridge->stats.touch_events);
    printf("Prediction Errors: %lu\n", (unsigned long)bridge->stats.prediction_errors);
    printf("Avg Prediction Error: %.4f\n", bridge->stats.avg_prediction_error);
    printf("Adaptation Progress: %.2f%%\n", bridge->stats.adaptation_progress * 100.0f);
    printf("==============================================\n");
}
