/**
 * @file nimcp_inter_layer_router.c
 * @brief Inter-Layer Router Implementation
 * @version 1.0.0
 * @date 2026-01-10
 */

#include "integration/core/nimcp_inter_layer_router.h"
#include "utils/memory/nimcp_memory.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_timing_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(inter_layer_router)

#define MAX_PENDING_MESSAGES 1024
#define MAX_QUEUES NIMCP_LAYER_COUNT

typedef struct {
    nimcp_layer_msg_t** messages;  // Dynamically allocated based on config
    uint32_t capacity;             // Queue capacity (from config.default_queue_depth)
    uint32_t head;
    uint32_t tail;
    uint32_t count;
} message_queue_t;

struct nimcp_inter_layer_router_struct {
    nimcp_inter_layer_router_config_t config;
    nimcp_layer_registry_t registry;
    message_queue_t queues[MAX_QUEUES];
    nimcp_route_callback_t callback;
    void* callback_data;
    nimcp_inter_layer_router_stats_t stats;
};

nimcp_inter_layer_router_config_t nimcp_inter_layer_router_default_config(void) {
    nimcp_inter_layer_router_config_t config = {0};
    config.route_mode = NIMCP_ROUTE_MODE_HYBRID;
    config.default_queue_depth = 256;
    config.max_pending_messages = MAX_PENDING_MESSAGES;
    config.process_batch_size = 16;
    config.enable_priority_queuing = true;
    config.enable_message_logging = false;
    config.enable_latency_tracking = true;
    config.timeout_ms = NIMCP_FAST_TIMEOUT_MS;
    return config;
}

nimcp_inter_layer_router_t nimcp_inter_layer_router_create(
    const nimcp_inter_layer_router_config_t* config, nimcp_layer_registry_t registry
) {
    nimcp_inter_layer_router_t router = (nimcp_inter_layer_router_t)nimcp_calloc(1, sizeof(struct nimcp_inter_layer_router_struct));
    NIMCP_API_CHECK_ALLOC(router, "Failed to allocate inter-layer router");
    router->config = config ? *config : nimcp_inter_layer_router_default_config();
    router->registry = registry;

    /* P4 fix: Allocate queue message arrays based on configured queue depth
     * instead of hardcoded 256 */
    uint32_t queue_depth = router->config.default_queue_depth;
    if (queue_depth == 0) {
        queue_depth = 256;  /* Fallback to safe default */
    }

    for (int i = 0; i < MAX_QUEUES; i++) {
        router->queues[i].messages = (nimcp_layer_msg_t**)nimcp_calloc(queue_depth, sizeof(nimcp_layer_msg_t*));
        if (!router->queues[i].messages) {
            /* Cleanup already-allocated queues */
            for (int j = 0; j < i; j++) {
                nimcp_free(router->queues[j].messages);
            }
            nimcp_free(router);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_inter_layer_router_create: queue allocation failed");
            return NULL;
        }
        router->queues[i].capacity = queue_depth;
    }

    return router;
}

void nimcp_inter_layer_router_destroy(nimcp_inter_layer_router_t router) {
    if (!router) return;
    /* Clear queues and free dynamic arrays */
    for (int i = 0; i < MAX_QUEUES; i++) {
        message_queue_t* q = &router->queues[i];
        while (q->count > 0 && q->messages) {
            nimcp_layer_msg_destroy(q->messages[q->head]);
            q->head = (q->head + 1) % q->capacity;
            q->count--;
        }
        nimcp_free(q->messages);
    }
    nimcp_free(router);
}

nimcp_layer_error_t nimcp_inter_layer_router_reset(nimcp_inter_layer_router_t router) {
    NIMCP_API_CHECK_NULL(router, NIMCP_LAYER_ERR_NULL_PTR, "Router is NULL in reset");
    for (int i = 0; i < MAX_QUEUES; i++) {
        message_queue_t* q = &router->queues[i];
        while (q->count > 0 && q->messages) {
            nimcp_layer_msg_destroy(q->messages[q->head]);
            q->head = (q->head + 1) % q->capacity;
            q->count--;
        }
    }
    memset(&router->stats, 0, sizeof(router->stats));
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_inter_layer_router_route(nimcp_inter_layer_router_t router, nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(router, NIMCP_LAYER_ERR_NULL_PTR, "Router is NULL in route");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL in route");

    nimcp_layer_id_t target = msg->header.target_layer;
    NIMCP_API_CHECK(target < NIMCP_LAYER_COUNT, NIMCP_LAYER_ERR_INVALID_LAYER, "Invalid target layer in route");

    message_queue_t* q = &router->queues[target];
    if (q->count >= q->capacity) {
        router->stats.messages_dropped++;
        return NIMCP_LAYER_ERR_QUEUE_FULL;
    }

    q->messages[q->tail] = msg;
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
    router->stats.messages_routed++;

    if (msg->header.direction == NIMCP_MSG_DIR_BOTTOM_UP) router->stats.bottom_up_count++;
    else if (msg->header.direction == NIMCP_MSG_DIR_TOP_DOWN) router->stats.top_down_count++;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_inter_layer_router_route_directed(
    nimcp_inter_layer_router_t router, nimcp_layer_msg_t* msg, nimcp_msg_direction_t direction
) {
    NIMCP_API_CHECK_NULL(router, NIMCP_LAYER_ERR_NULL_PTR, "Router is NULL in route_directed");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL in route_directed");
    msg->header.direction = direction;
    return nimcp_inter_layer_router_route(router, msg);
}

nimcp_layer_error_t nimcp_inter_layer_router_broadcast(
    nimcp_inter_layer_router_t router, nimcp_layer_id_t source_layer, const nimcp_layer_msg_t* msg
) {
    NIMCP_API_CHECK_NULL(router, NIMCP_LAYER_ERR_NULL_PTR, "Router is NULL in broadcast");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL in broadcast");

    for (int i = 0; i < NIMCP_LAYER_COUNT; i++) {
        if (i != source_layer && router->registry && nimcp_layer_registry_is_layer_registered(router->registry, i)) {
            nimcp_layer_msg_t* clone = nimcp_layer_msg_clone(msg);
            if (clone) {
                clone->header.target_layer = i;
                clone->header.source_layer = source_layer;
                nimcp_inter_layer_router_route(router, clone);
            }
        }
    }
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_inter_layer_router_broadcast_directed(
    nimcp_inter_layer_router_t router, nimcp_layer_id_t source_layer,
    const nimcp_layer_msg_t* msg, nimcp_msg_direction_t direction
) {
    NIMCP_API_CHECK_NULL(router, NIMCP_LAYER_ERR_NULL_PTR, "Router is NULL in broadcast_directed");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL in broadcast_directed");

    for (int i = 0; i < NIMCP_LAYER_COUNT; i++) {
        if (i != source_layer) {
            bool route_ok = false;
            if (direction == NIMCP_MSG_DIR_BOTTOM_UP && i > source_layer) route_ok = true;
            else if (direction == NIMCP_MSG_DIR_TOP_DOWN && i < source_layer) route_ok = true;

            if (route_ok && router->registry && nimcp_layer_registry_is_layer_registered(router->registry, i)) {
                nimcp_layer_msg_t* clone = nimcp_layer_msg_clone(msg);
                if (clone) {
                    clone->header.target_layer = i;
                    clone->header.source_layer = source_layer;
                    clone->header.direction = direction;
                    nimcp_inter_layer_router_route(router, clone);
                }
            }
        }
    }
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_inter_layer_router_process_layer(
    nimcp_inter_layer_router_t router, nimcp_layer_id_t layer_id,
    uint32_t max_messages, uint32_t* processed_out
) {
    NIMCP_API_CHECK_NULL(router, NIMCP_LAYER_ERR_NULL_PTR, "Router is NULL in process_layer");
    NIMCP_API_CHECK(layer_id < NIMCP_LAYER_COUNT, NIMCP_LAYER_ERR_INVALID_LAYER, "Invalid layer_id in process_layer");

    message_queue_t* q = &router->queues[layer_id];
    uint32_t processed = 0;
    uint32_t limit = (max_messages == 0) ? q->count : max_messages;

    while (q->count > 0 && processed < limit) {
        nimcp_layer_msg_t* msg = q->messages[q->head];
        if (router->callback) {
            router->callback(msg->header.source_layer, layer_id, msg, router->callback_data);
        }
        nimcp_layer_msg_destroy(msg);
        q->head = (q->head + 1) % q->capacity;
        q->count--;
        processed++;
    }

    if (processed_out) *processed_out = processed;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_inter_layer_router_process_all(
    nimcp_inter_layer_router_t router, uint32_t max_messages, uint32_t* processed_out
) {
    NIMCP_API_CHECK_NULL(router, NIMCP_LAYER_ERR_NULL_PTR, "Router is NULL in process_all");

    uint32_t total_processed = 0;
    for (int i = 0; i < NIMCP_LAYER_COUNT && (max_messages == 0 || total_processed < max_messages); i++) {
        uint32_t remaining = (max_messages == 0) ? 0 : (max_messages - total_processed);
        uint32_t processed = 0;
        nimcp_inter_layer_router_process_layer(router, i, remaining, &processed);
        total_processed += processed;
    }

    if (processed_out) *processed_out = total_processed;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_inter_layer_router_peek(
    nimcp_inter_layer_router_t router, nimcp_layer_id_t layer_id, const nimcp_layer_msg_t** msg_out
) {
    NIMCP_API_CHECK_NULL(router, NIMCP_LAYER_ERR_NULL_PTR, "Router is NULL in peek");
    NIMCP_API_CHECK_NULL(msg_out, NIMCP_LAYER_ERR_NULL_PTR, "msg_out is NULL in peek");
    NIMCP_API_CHECK(layer_id < NIMCP_LAYER_COUNT, NIMCP_LAYER_ERR_INVALID_LAYER, "Invalid layer_id in peek");

    message_queue_t* q = &router->queues[layer_id];
    if (q->count == 0) return NIMCP_LAYER_ERR_QUEUE_EMPTY;

    *msg_out = q->messages[q->head];
    return NIMCP_LAYER_OK;
}

int nimcp_inter_layer_router_get_queue_depth(nimcp_inter_layer_router_t router, nimcp_layer_id_t layer_id) {
    if (!router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_inter_layer_router_get_queue_depth: router is NULL");
        return -1;
    }
    if (layer_id >= NIMCP_LAYER_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_inter_layer_router_get_queue_depth: layer_id out of range");
        return -1;
    }
    return (int)router->queues[layer_id].count;
}

nimcp_layer_error_t nimcp_inter_layer_router_set_route_enabled(
    nimcp_inter_layer_router_t router, nimcp_layer_id_t source, nimcp_layer_id_t target, bool enabled
) {
    (void)router; (void)source; (void)target; (void)enabled;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_inter_layer_router_set_coupling(
    nimcp_inter_layer_router_t router, nimcp_layer_id_t source, nimcp_layer_id_t target, float strength
) {
    (void)router; (void)source; (void)target; (void)strength;
    return NIMCP_LAYER_OK;
}

bool nimcp_inter_layer_router_route_available(
    nimcp_inter_layer_router_t router, nimcp_layer_id_t source, nimcp_layer_id_t target
) {
    if (!router) {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_inter_layer_router_route_available: router is NULL");

            return false;

        }
    if (source >= NIMCP_LAYER_COUNT || target >= NIMCP_LAYER_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_inter_layer_router_route_available: capacity exceeded");
        return false;
    }
    return true;
}

nimcp_layer_error_t nimcp_inter_layer_router_get_stats(
    nimcp_inter_layer_router_t router, nimcp_inter_layer_router_stats_t* stats_out
) {
    NIMCP_API_CHECK_NULL(router, NIMCP_LAYER_ERR_NULL_PTR, "Router is NULL in get_stats");
    NIMCP_API_CHECK_NULL(stats_out, NIMCP_LAYER_ERR_NULL_PTR, "stats_out is NULL in get_stats");
    *stats_out = router->stats;
    router->stats.current_queue_depth = 0;
    for (int i = 0; i < MAX_QUEUES; i++) {
        router->stats.current_queue_depth += router->queues[i].count;
    }
    *stats_out = router->stats;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_inter_layer_router_reset_stats(nimcp_inter_layer_router_t router) {
    NIMCP_API_CHECK_NULL(router, NIMCP_LAYER_ERR_NULL_PTR, "Router is NULL in reset_stats");
    memset(&router->stats, 0, sizeof(router->stats));
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_inter_layer_router_get_path(
    nimcp_inter_layer_router_t router, nimcp_layer_id_t source, nimcp_layer_id_t target,
    nimcp_layer_id_t* path_out, size_t max_path_len, size_t* path_len_out
) {
    NIMCP_API_CHECK_NULL(router, NIMCP_LAYER_ERR_NULL_PTR, "Router is NULL in get_path");
    NIMCP_API_CHECK_NULL(path_out, NIMCP_LAYER_ERR_NULL_PTR, "path_out is NULL in get_path");
    NIMCP_API_CHECK_NULL(path_len_out, NIMCP_LAYER_ERR_NULL_PTR, "path_len_out is NULL in get_path");
    NIMCP_API_CHECK(max_path_len >= 2, NIMCP_LAYER_ERR_CAPACITY, "max_path_len too small in get_path");

    /* Simple direct path */
    path_out[0] = source;
    path_out[1] = target;
    *path_len_out = 2;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_inter_layer_router_set_callback(
    nimcp_inter_layer_router_t router, nimcp_route_callback_t callback, void* user_data
) {
    NIMCP_API_CHECK_NULL(router, NIMCP_LAYER_ERR_NULL_PTR, "Router is NULL in set_callback");
    router->callback = callback;
    router->callback_data = user_data;
    return NIMCP_LAYER_OK;
}
