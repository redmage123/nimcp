/**
 * @file nimcp_layer_coordinator.c
 * @brief Central Layer Coordinator Implementation
 * @version 1.0.0
 * @date 2026-01-10
 */

#include "integration/core/nimcp_layer_coordinator.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"
#include "constants/nimcp_timing_constants.h"
#include "constants/nimcp_neural_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(layer_coordinator)

struct nimcp_layer_coordinator_struct {
    nimcp_layer_coordinator_config_t config;
    nimcp_layer_registry_t registry;
    nimcp_inter_layer_router_t router;
    brain_t brain;
    bio_router_t bio_router;
    nimcp_brain_immune_t immune;
    nimcp_layer_coordinator_state_t state;
    nimcp_layer_coordinator_stats_t stats;
    float layer_coherences[NIMCP_LAYER_COUNT];
    nimcp_layer_error_t last_error;
    char last_error_msg[NIMCP_ERROR_BUFFER_SIZE];
};

nimcp_layer_coordinator_config_t nimcp_layer_coordinator_default_config(void) {
    nimcp_layer_coordinator_config_t config = {0};
    config.registry_config = nimcp_layer_registry_default_config();
    config.router_config = nimcp_inter_layer_router_default_config();
    config.update_interval_ms = 10;
    config.parallel_layer_update = false;
    config.enable_coherence_tracking = true;
    config.enable_bio_async = true;
    config.enable_immune_integration = true;
    config.enable_logging = false;
    config.enable_metrics = true;
    config.coherence_threshold = NIMCP_PHASE_COHERENCE_THRESHOLD;
    config.sync_timeout_ms = NIMCP_FAST_TIMEOUT_MS;
    return config;
}

nimcp_layer_coordinator_t nimcp_layer_coordinator_create(const nimcp_layer_coordinator_config_t* config, brain_t brain) {
    nimcp_layer_coordinator_t coord = (nimcp_layer_coordinator_t)nimcp_calloc(1, sizeof(struct nimcp_layer_coordinator_struct));
    NIMCP_API_CHECK_ALLOC(coord, "Failed to allocate layer coordinator");

    coord->config = config ? *config : nimcp_layer_coordinator_default_config();
    coord->brain = brain;
    coord->state = NIMCP_COORD_STATE_UNINITIALIZED;

    /* Create registry */
    coord->registry = nimcp_layer_registry_create(&coord->config.registry_config);
    if (!coord->registry) {
        nimcp_free(coord);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_layer_coordinator_create: coord->registry is NULL");
        return NULL;
    }

    /* Create router */
    coord->router = nimcp_inter_layer_router_create(&coord->config.router_config, coord->registry);
    if (!coord->router) {
        nimcp_layer_registry_destroy(coord->registry);
        nimcp_free(coord);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_layer_coordinator_create: coord->router is NULL");
        return NULL;
    }

    return coord;
}

void nimcp_layer_coordinator_destroy(nimcp_layer_coordinator_t coord) {
    if (!coord) return;
    if (coord->state == NIMCP_COORD_STATE_RUNNING) {
        nimcp_layer_coordinator_shutdown(coord);
    }
    if (coord->router) nimcp_inter_layer_router_destroy(coord->router);
    if (coord->registry) nimcp_layer_registry_destroy(coord->registry);
    nimcp_free(coord);
}

nimcp_layer_error_t nimcp_layer_coordinator_init_all(nimcp_layer_coordinator_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in init_all");
    if (coord->state == NIMCP_COORD_STATE_RUNNING) return NIMCP_LAYER_ERR_ALREADY_REGISTERED;

    coord->state = NIMCP_COORD_STATE_INITIALIZING;

    /* Initialize coherences */
    for (int i = 0; i < NIMCP_LAYER_COUNT; i++) {
        coord->layer_coherences[i] = 1.0f;
    }

    coord->state = NIMCP_COORD_STATE_RUNNING;
    coord->stats.state = NIMCP_COORD_STATE_RUNNING;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_layer_coordinator_shutdown(nimcp_layer_coordinator_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in shutdown");
    if (coord->state != NIMCP_COORD_STATE_RUNNING && coord->state != NIMCP_COORD_STATE_PAUSED) {
        return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    }

    coord->state = NIMCP_COORD_STATE_SHUTTING_DOWN;

    /* Reset router */
    if (coord->router) nimcp_inter_layer_router_reset(coord->router);

    coord->state = NIMCP_COORD_STATE_UNINITIALIZED;
    coord->stats.state = NIMCP_COORD_STATE_UNINITIALIZED;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_layer_coordinator_reset(nimcp_layer_coordinator_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in reset");
    if (coord->router) nimcp_inter_layer_router_reset(coord->router);
    if (coord->registry) nimcp_layer_registry_reset(coord->registry);
    memset(&coord->stats, 0, sizeof(coord->stats));
    coord->state = NIMCP_COORD_STATE_UNINITIALIZED;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_layer_coordinator_register_layer(nimcp_layer_coordinator_t coord, const nimcp_layer_config_t* config) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in register_layer");
    NIMCP_API_CHECK_NULL(config, NIMCP_LAYER_ERR_NULL_PTR, "Config is NULL in register_layer");
    nimcp_layer_error_t err = nimcp_layer_registry_register_layer(coord->registry, config);
    if (err == NIMCP_LAYER_OK) {
        coord->stats.layers_registered++;
    }
    return err;
}

nimcp_layer_error_t nimcp_layer_coordinator_register_standard_layers(nimcp_layer_coordinator_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in register_standard_layers");

    nimcp_layer_config_t config;
    nimcp_layer_id_t layers[] = {
        NIMCP_LAYER_PHYSICS, NIMCP_LAYER_CHEMISTRY, NIMCP_LAYER_BIOLOGY,
        NIMCP_LAYER_NEUROMODULATORY, NIMCP_LAYER_SENSORY, NIMCP_LAYER_MEMORY,
        NIMCP_LAYER_EXECUTIVE, NIMCP_LAYER_INTEGRATION, NIMCP_LAYER_SUPERHUMAN
    };

    for (size_t i = 0; i < sizeof(layers) / sizeof(layers[0]); i++) {
        config = nimcp_layer_default_config(layers[i]);
        nimcp_layer_error_t err = nimcp_layer_registry_register_layer(coord->registry, &config);
        if (err != NIMCP_LAYER_OK && err != NIMCP_LAYER_ERR_ALREADY_REGISTERED) {
            return err;
        }
        coord->stats.layers_registered++;
    }

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_layer_coordinator_register_standard_connections(nimcp_layer_coordinator_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in register_standard_connections");

    /* Define standard connections */
    nimcp_layer_connection_t conn = {0};
    conn.bidirectional = true;
    conn.bottom_up_enabled = true;
    conn.top_down_enabled = true;
    conn.coupling_strength = 1.0f;
    conn.queue_depth = NIMCP_LAYER_MSG_QUEUE_DEPTH;

    /* Physics <-> Chemistry */
    conn.layer_a = NIMCP_LAYER_PHYSICS;
    conn.layer_b = NIMCP_LAYER_CHEMISTRY;
    nimcp_layer_registry_register_connection(coord->registry, &conn);
    coord->stats.connections_active++;

    /* Chemistry <-> Biology */
    conn.layer_a = NIMCP_LAYER_CHEMISTRY;
    conn.layer_b = NIMCP_LAYER_BIOLOGY;
    nimcp_layer_registry_register_connection(coord->registry, &conn);
    coord->stats.connections_active++;

    /* Biology <-> Neuromodulatory */
    conn.layer_a = NIMCP_LAYER_BIOLOGY;
    conn.layer_b = NIMCP_LAYER_NEUROMODULATORY;
    nimcp_layer_registry_register_connection(coord->registry, &conn);
    coord->stats.connections_active++;

    /* Neuromodulatory <-> Sensory/Memory/Executive */
    conn.layer_a = NIMCP_LAYER_NEUROMODULATORY;
    conn.layer_b = NIMCP_LAYER_SENSORY;
    nimcp_layer_registry_register_connection(coord->registry, &conn);
    coord->stats.connections_active++;

    conn.layer_b = NIMCP_LAYER_MEMORY;
    nimcp_layer_registry_register_connection(coord->registry, &conn);
    coord->stats.connections_active++;

    conn.layer_b = NIMCP_LAYER_EXECUTIVE;
    nimcp_layer_registry_register_connection(coord->registry, &conn);
    coord->stats.connections_active++;

    /* Processing layer interconnections */
    conn.layer_a = NIMCP_LAYER_SENSORY;
    conn.layer_b = NIMCP_LAYER_MEMORY;
    nimcp_layer_registry_register_connection(coord->registry, &conn);
    coord->stats.connections_active++;

    conn.layer_b = NIMCP_LAYER_EXECUTIVE;
    nimcp_layer_registry_register_connection(coord->registry, &conn);
    coord->stats.connections_active++;

    conn.layer_a = NIMCP_LAYER_MEMORY;
    conn.layer_b = NIMCP_LAYER_EXECUTIVE;
    nimcp_layer_registry_register_connection(coord->registry, &conn);
    coord->stats.connections_active++;

    /* Processing <-> Integration */
    conn.layer_a = NIMCP_LAYER_MEMORY;
    conn.layer_b = NIMCP_LAYER_INTEGRATION;
    nimcp_layer_registry_register_connection(coord->registry, &conn);
    coord->stats.connections_active++;

    conn.layer_a = NIMCP_LAYER_EXECUTIVE;
    nimcp_layer_registry_register_connection(coord->registry, &conn);
    coord->stats.connections_active++;

    /* Integration <-> Superhuman */
    conn.layer_a = NIMCP_LAYER_INTEGRATION;
    conn.layer_b = NIMCP_LAYER_SUPERHUMAN;
    nimcp_layer_registry_register_connection(coord->registry, &conn);
    coord->stats.connections_active++;

    /* Superhuman feedback to Neuromodulatory */
    conn.layer_a = NIMCP_LAYER_SUPERHUMAN;
    conn.layer_b = NIMCP_LAYER_NEUROMODULATORY;
    nimcp_layer_registry_register_connection(coord->registry, &conn);
    coord->stats.connections_active++;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_layer_coordinator_register_module(
    nimcp_layer_coordinator_t coord, nimcp_layer_id_t layer_id,
    void* module_ptr, nimcp_module_interface_t* interface, const char* name, uint32_t* module_id_out
) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in register_module");
    nimcp_layer_error_t err = nimcp_layer_registry_register_module(coord->registry, layer_id, module_ptr, interface, name, module_id_out);
    if (err == NIMCP_LAYER_OK) {
        coord->stats.modules_registered++;
    }
    return err;
}

nimcp_layer_error_t nimcp_layer_coordinator_update(nimcp_layer_coordinator_t coord, float dt) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in update");
    if (coord->state != NIMCP_COORD_STATE_RUNNING) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    coord->stats.update_count++;
    (void)dt;

    /* Process inter-layer messages */
    uint32_t processed = 0;
    nimcp_inter_layer_router_process_all(coord->router, 0, &processed);
    coord->stats.messages_routed += processed;

    /* Update coherence */
    if (coord->config.enable_coherence_tracking) {
        float sum = 0.0f;
        int count = 0;
        for (int i = 0; i < NIMCP_LAYER_COUNT; i++) {
            if (nimcp_layer_registry_is_layer_registered(coord->registry, (nimcp_layer_id_t)i)) {
                sum += coord->layer_coherences[i];
                count++;
            }
        }
        coord->stats.global_coherence = count > 0 ? sum / count : 0.0f;
        coord->stats.avg_layer_coherence = coord->stats.global_coherence;
    }

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_layer_coordinator_update_layer(nimcp_layer_coordinator_t coord, nimcp_layer_id_t layer_id, float dt) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in update_layer");
    NIMCP_API_CHECK(coord->state == NIMCP_COORD_STATE_RUNNING, NIMCP_LAYER_ERR_NOT_INITIALIZED, "Coordinator not running in update_layer");
    NIMCP_API_CHECK(layer_id < NIMCP_LAYER_COUNT, NIMCP_LAYER_ERR_INVALID_LAYER, "Invalid layer_id in update_layer");
    (void)dt;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_layer_coordinator_pause(nimcp_layer_coordinator_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in pause");
    if (coord->state != NIMCP_COORD_STATE_RUNNING) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    coord->state = NIMCP_COORD_STATE_PAUSED;
    coord->stats.state = NIMCP_COORD_STATE_PAUSED;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_layer_coordinator_resume(nimcp_layer_coordinator_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in resume");
    if (coord->state != NIMCP_COORD_STATE_PAUSED) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    coord->state = NIMCP_COORD_STATE_RUNNING;
    coord->stats.state = NIMCP_COORD_STATE_RUNNING;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_layer_coordinator_send_message(nimcp_layer_coordinator_t coord, nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in send_message");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL in send_message");
    if (coord->state != NIMCP_COORD_STATE_RUNNING) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    return nimcp_inter_layer_router_route(coord->router, msg);
}

nimcp_layer_error_t nimcp_layer_coordinator_broadcast(nimcp_layer_coordinator_t coord, nimcp_layer_id_t source_layer, const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in broadcast");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL in broadcast");
    if (coord->state != NIMCP_COORD_STATE_RUNNING) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    return nimcp_inter_layer_router_broadcast(coord->router, source_layer, msg);
}

nimcp_layer_error_t nimcp_layer_coordinator_broadcast_bottom_up(nimcp_layer_coordinator_t coord, nimcp_layer_id_t source_layer, const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in broadcast_bottom_up");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL in broadcast_bottom_up");
    if (coord->state != NIMCP_COORD_STATE_RUNNING) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    return nimcp_inter_layer_router_broadcast_directed(coord->router, source_layer, msg, NIMCP_MSG_DIR_BOTTOM_UP);
}

nimcp_layer_error_t nimcp_layer_coordinator_broadcast_top_down(nimcp_layer_coordinator_t coord, nimcp_layer_id_t source_layer, const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in broadcast_top_down");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL in broadcast_top_down");
    if (coord->state != NIMCP_COORD_STATE_RUNNING) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    return nimcp_inter_layer_router_broadcast_directed(coord->router, source_layer, msg, NIMCP_MSG_DIR_TOP_DOWN);
}

nimcp_layer_error_t nimcp_layer_coordinator_sync(nimcp_layer_coordinator_t coord, uint32_t timeout_ms) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in sync");
    if (coord->state != NIMCP_COORD_STATE_RUNNING) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    (void)timeout_ms;
    return NIMCP_LAYER_OK;
}

float nimcp_layer_coordinator_get_coherence(nimcp_layer_coordinator_t coord) {
    if (!coord) return -1.0f;
    return coord->stats.global_coherence;
}

float nimcp_layer_coordinator_get_layer_coherence(nimcp_layer_coordinator_t coord, nimcp_layer_id_t layer_id) {
    if (!coord || layer_id >= NIMCP_LAYER_COUNT) return -1.0f;
    return coord->layer_coherences[layer_id];
}

nimcp_layer_error_t nimcp_layer_coordinator_connect_bio_async(nimcp_layer_coordinator_t coord, bio_router_t router) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in connect_bio_async");
    coord->bio_router = router;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_layer_coordinator_connect_immune(nimcp_layer_coordinator_t coord, nimcp_brain_immune_t immune) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in connect_immune");
    coord->immune = immune;
    return NIMCP_LAYER_OK;
}

brain_t nimcp_layer_coordinator_get_brain(nimcp_layer_coordinator_t coord) {
    return coord ? coord->brain : NULL;
}

nimcp_layer_coordinator_state_t nimcp_layer_coordinator_get_state(nimcp_layer_coordinator_t coord) {
    return coord ? coord->state : NIMCP_COORD_STATE_UNINITIALIZED;
}

nimcp_layer_error_t nimcp_layer_coordinator_get_stats(nimcp_layer_coordinator_t coord, nimcp_layer_coordinator_stats_t* stats_out) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in get_stats");
    NIMCP_API_CHECK_NULL(stats_out, NIMCP_LAYER_ERR_NULL_PTR, "stats_out is NULL in get_stats");
    *stats_out = coord->stats;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_layer_coordinator_reset_stats(nimcp_layer_coordinator_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in reset_stats");
    nimcp_layer_coordinator_state_t state = coord->stats.state;
    memset(&coord->stats, 0, sizeof(coord->stats));
    coord->stats.state = state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_registry_t nimcp_layer_coordinator_get_registry(nimcp_layer_coordinator_t coord) {
    return coord ? coord->registry : NULL;
}

nimcp_inter_layer_router_t nimcp_layer_coordinator_get_router(nimcp_layer_coordinator_t coord) {
    return coord ? coord->router : NULL;
}

nimcp_layer_error_t nimcp_layer_coordinator_get_last_error(nimcp_layer_coordinator_t coord) {
    return coord ? coord->last_error : NIMCP_LAYER_ERR_NULL_PTR;
}

const char* nimcp_layer_coordinator_get_last_error_msg(nimcp_layer_coordinator_t coord) {
    if (!coord) return "NULL coordinator";
    return coord->last_error_msg[0] ? coord->last_error_msg : "No error";
}

void nimcp_layer_coordinator_clear_error(nimcp_layer_coordinator_t coord) {
    if (!coord) return;
    coord->last_error = NIMCP_LAYER_OK;
    coord->last_error_msg[0] = '\0';
}
