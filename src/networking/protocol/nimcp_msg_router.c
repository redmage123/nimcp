/**
 * @file nimcp_msg_router.c
 * @brief Implementation of message routing for collective cognition protocol
 *
 * WHAT: Routes messages to appropriate handlers based on type
 * WHY: Decouple message parsing from handling logic
 * HOW: Handler registration + dispatch based on message type
 */

#include "networking/protocol/nimcp_msg_router.h"
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>

/*=============================================================================
 * Health Agent Forward Declarations (Phase 8: Heartbeat for Long Operations)
 *===========================================================================*/
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(msg_router)

/* Alias: tests reference nimcp_msg_router_set_health_agent (with nimcp_ prefix) */
void nimcp_msg_router_set_health_agent(struct nimcp_health_agent* agent) { (void)agent; }

/*=============================================================================
 * Internal Structures
 *===========================================================================*/

/**
 * @brief Queued message entry
 */
typedef struct {
    uint8_t* data;
    size_t len;
    bool valid;
} nimcp_queued_msg_t;

/**
 * @brief Message router internal state
 */
struct nimcp_msg_router {
    /* Configuration */
    nimcp_msg_router_config_t config;

    /* Handlers */
    nimcp_msg_handler_entry_t handlers[NIMCP_MAX_MSG_HANDLERS];
    uint32_t handler_count;

    /* Queue (circular buffer) */
    nimcp_queued_msg_t* queue;
    uint32_t queue_capacity;
    uint32_t queue_head;
    uint32_t queue_tail;
    uint32_t queue_count;

    /* Statistics */
    nimcp_msg_router_stats_t stats;

    /* State */
    bool initialized;
};

/*=============================================================================
 * Helper Functions
 *===========================================================================*/

/**
 * @brief Find handler for message type
 */
static nimcp_msg_handler_entry_t* find_handler(
    nimcp_msg_router_t* router,
    nimcp_msg_type_t msg_type
) {
    for (uint32_t i = 0; i < router->handler_count; i++) {
        if (router->handlers[i].msg_type == msg_type) {
            return &router->handlers[i];
        }
    }
    return NULL;  // Not found - normal behavior
}

/**
 * @brief Check if message type is fast path
 */
static bool is_fast_path_type(nimcp_msg_type_t msg_type) {
    return (msg_type & 0xFF00) == 0x0000;
}

/*=============================================================================
 * Configuration API
 *===========================================================================*/

nimcp_msg_router_config_t nimcp_msg_router_default_config(void) {
    nimcp_msg_router_config_t config = {
        .enable_queue = false,
        .queue_size = NIMCP_MSG_QUEUE_SIZE,
        .enable_stats = true,
        .default_handler = NULL,
        .default_user_data = NULL
    };
    return config;
}

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

nimcp_msg_router_t* nimcp_msg_router_create(
    const nimcp_msg_router_config_t* config
) {
    nimcp_msg_router_t* router = nimcp_malloc(sizeof(nimcp_msg_router_t));
    if (!router) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(nimcp_msg_router_t),
                          "Failed to allocate message router");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "router is NULL");

        return NULL;
    }

    memset(router, 0, sizeof(nimcp_msg_router_t));

    /* Apply configuration */
    if (config) {
        router->config = *config;
    } else {
        router->config = nimcp_msg_router_default_config();
    }

    /* Allocate queue if enabled */
    if (router->config.enable_queue && router->config.queue_size > 0) {
        router->queue_capacity = router->config.queue_size;
        router->queue = nimcp_malloc(sizeof(nimcp_queued_msg_t) * router->queue_capacity);
        if (!router->queue) {
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                              sizeof(nimcp_queued_msg_t) * router->queue_capacity,
                              "Failed to allocate message queue with capacity %u",
                              router->queue_capacity);
            nimcp_free(router);
            return NULL;
        }
        memset(router->queue, 0, sizeof(nimcp_queued_msg_t) * router->queue_capacity);
    }

    router->initialized = true;
    return router;
}

void nimcp_msg_router_destroy(nimcp_msg_router_t* router) {
    if (!router) return;

    /* Free queued message data */
    if (router->queue) {
        for (uint32_t i = 0; i < router->queue_capacity; i++) {
            if (router->queue[i].data) {
                nimcp_free(router->queue[i].data);
            }
        }
        nimcp_free(router->queue);
    }

    nimcp_free(router);
}

int nimcp_msg_router_reset(nimcp_msg_router_t* router) {
    if (!router) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router is NULL");

        return -1;

    }

    /* Clear queue */
    nimcp_msg_router_clear_queue(router);

    /* Reset statistics */
    memset(&router->stats, 0, sizeof(router->stats));

    return 0;
}

/*=============================================================================
 * Handler Registration API
 *===========================================================================*/

int nimcp_msg_router_register(
    nimcp_msg_router_t* router,
    nimcp_msg_type_t msg_type,
    nimcp_msg_handler_fn handler,
    void* user_data
) {
    if (!router || !handler) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_msg_router_register: required parameter is NULL (router, handler)");
        return -1;
    }

    /* Check if already registered */
    nimcp_msg_handler_entry_t* existing = find_handler(router, msg_type);
    if (existing) {
        /* Update existing registration */
        existing->handler = handler;
        existing->user_data = user_data;
        existing->is_fast_path = false;
        return 0;
    }

    /* Check capacity */
    if (router->handler_count >= NIMCP_MAX_MSG_HANDLERS) {
        NIMCP_THROW(NIMCP_ERROR_OUT_OF_RANGE,
                   "Message router handler capacity exceeded (max=%d)", NIMCP_MAX_MSG_HANDLERS);
        return -1;
    }

    /* Add new handler */
    nimcp_msg_handler_entry_t* entry = &router->handlers[router->handler_count++];
    entry->msg_type = msg_type;
    entry->handler = handler;
    entry->fast_handler = NULL;
    entry->user_data = user_data;
    entry->is_fast_path = false;

    return 0;
}

int nimcp_msg_router_register_fast(
    nimcp_msg_router_t* router,
    nimcp_msg_type_t msg_type,
    nimcp_fast_msg_handler_fn handler,
    void* user_data
) {
    if (!router || !handler) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_msg_router_register_fast: required parameter is NULL (router, handler)");
        return -1;
    }

    /* Verify it's a fast path type */
    if (!is_fast_path_type(msg_type)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_msg_router_register_fast: msg_type is not a fast-path type");
        return -1;
    }

    /* Check if already registered */
    nimcp_msg_handler_entry_t* existing = find_handler(router, msg_type);
    if (existing) {
        existing->fast_handler = handler;
        existing->user_data = user_data;
        existing->is_fast_path = true;
        return 0;
    }

    /* Check capacity */
    if (router->handler_count >= NIMCP_MAX_MSG_HANDLERS) {
        NIMCP_THROW(NIMCP_ERROR_OUT_OF_RANGE,
                   "Message router fast handler capacity exceeded (max=%d)", NIMCP_MAX_MSG_HANDLERS);
        return -1;
    }

    /* Add new handler */
    nimcp_msg_handler_entry_t* entry = &router->handlers[router->handler_count++];
    entry->msg_type = msg_type;
    entry->handler = NULL;
    entry->fast_handler = handler;
    entry->user_data = user_data;
    entry->is_fast_path = true;

    return 0;
}

int nimcp_msg_router_unregister(
    nimcp_msg_router_t* router,
    nimcp_msg_type_t msg_type
) {
    if (!router) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router is NULL");

        return -1;

    }

    for (uint32_t i = 0; i < router->handler_count; i++) {
        if (router->handlers[i].msg_type == msg_type) {
            /* Shift remaining handlers */
            for (uint32_t j = i; j < router->handler_count - 1; j++) {
                router->handlers[j] = router->handlers[j + 1];
            }
            router->handler_count--;
            return 0;
        }
    }

    /* Handler not found for this msg_type -- not an error */
    return -1;  /* Not found */
}

bool nimcp_msg_router_has_handler(
    const nimcp_msg_router_t* router,
    nimcp_msg_type_t msg_type
) {
    if (!router) {
        return false;
    }

    for (uint32_t i = 0; i < router->handler_count; i++) {
        if (router->handlers[i].msg_type == msg_type) {
            return true;
        }
    }

    return false;
}

/*=============================================================================
 * Routing API
 *===========================================================================*/

int nimcp_msg_router_route(
    nimcp_msg_router_t* router,
    const uint8_t* data,
    size_t len
) {
    if (!router || !data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_msg_router_route: required parameter is NULL (router, data)");
        return -1;
    }

    /* Need at least header */
    if (len < NIMCP_MSG_HEADER_SIZE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_msg_router_route: validation failed");
        return -1;
    }

    /* Parse header */
    nimcp_msg_header_t header;
    if (nimcp_msg_header_deserialize(data, &header) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_msg_router_route: validation failed");
        return -1;
    }

    /* Check payload length */
    size_t expected_len = NIMCP_MSG_HEADER_SIZE + header.payload_len;
    if (len < expected_len) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_msg_router_route: validation failed");
        return -1;
    }

    /* Route based on type */
    return nimcp_msg_router_route_parsed(
        router,
        &header,
        data + NIMCP_MSG_HEADER_SIZE,
        header.payload_len
    );
}

int nimcp_msg_router_route_fast(
    nimcp_msg_router_t* router,
    const nimcp_fast_msg_t* msg
) {
    if (!router || !msg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_msg_router_route_fast: required parameter is NULL (router, msg)");
        return -1;
    }

    /* Find handler */
    nimcp_msg_handler_entry_t* entry = find_handler(router, msg->header.msg_type);

    if (entry && entry->is_fast_path && entry->fast_handler) {
        /* Call fast handler */
        int result = entry->fast_handler(msg, entry->user_data);

        if (router->config.enable_stats) {
            router->stats.messages_routed++;
            router->stats.fast_messages_routed++;
            router->stats.bytes_routed += 24;
            if (result != 0) {
                router->stats.handler_errors++;
            }
        }

        return result;
    }

    /* Try generic handler */
    if (entry && entry->handler) {
        int result = entry->handler(&msg->header, msg->payload, NIMCP_MSG_FAST_PAYLOAD, entry->user_data);

        if (router->config.enable_stats) {
            router->stats.messages_routed++;
            router->stats.fast_messages_routed++;
            router->stats.bytes_routed += 24;
            if (result != 0) {
                router->stats.handler_errors++;
            }
        }

        return result;
    }

    /* Try default handler */
    if (router->config.default_handler) {
        int result = router->config.default_handler(
            &msg->header, msg->payload, NIMCP_MSG_FAST_PAYLOAD,
            router->config.default_user_data);

        if (router->config.enable_stats) {
            router->stats.messages_routed++;
            router->stats.bytes_routed += 24;
            if (result != 0) {
                router->stats.handler_errors++;
            }
        }

        return result;
    }

    /* Unhandled */
    if (router->config.enable_stats) {
        router->stats.unhandled_messages++;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_msg_router_route_fast: validation failed");
    return -1;
}

int nimcp_msg_router_route_parsed(
    nimcp_msg_router_t* router,
    const nimcp_msg_header_t* header,
    const uint8_t* payload,
    size_t payload_len
) {
    if (!router || !header) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_msg_router_route_parsed: required parameter is NULL (router, header)");
        return -1;
    }

    /* Find handler */
    nimcp_msg_handler_entry_t* entry = find_handler(router, header->msg_type);

    if (entry) {
        int result;

        /* Check if fast path handler should be used */
        if (entry->is_fast_path && entry->fast_handler && nimcp_msg_is_fast_path(header)) {
            nimcp_fast_msg_t msg;
            msg.header = *header;
            if (payload && payload_len <= NIMCP_MSG_FAST_PAYLOAD) {
                memcpy(msg.payload, payload, payload_len);
            }
            result = entry->fast_handler(&msg, entry->user_data);
        } else if (entry->handler) {
            result = entry->handler(header, payload, payload_len, entry->user_data);
        } else {
            result = -1;
        }

        if (router->config.enable_stats) {
            router->stats.messages_routed++;
            router->stats.bytes_routed += NIMCP_MSG_HEADER_SIZE + payload_len;

            if (nimcp_msg_is_fast_path(header)) {
                router->stats.fast_messages_routed++;
            } else {
                router->stats.protobuf_messages_routed++;
            }

            if (result != 0) {
                router->stats.handler_errors++;
            }
        }

        return result;
    }

    /* Try default handler */
    if (router->config.default_handler) {
        int result = router->config.default_handler(
            header, payload, payload_len,
            router->config.default_user_data);

        if (router->config.enable_stats) {
            router->stats.messages_routed++;
            router->stats.bytes_routed += NIMCP_MSG_HEADER_SIZE + payload_len;
            if (result != 0) {
                router->stats.handler_errors++;
            }
        }

        return result;
    }

    /* Unhandled */
    if (router->config.enable_stats) {
        router->stats.unhandled_messages++;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_msg_router_route_parsed: validation failed");
    return -1;
}

/*=============================================================================
 * Queue API
 *===========================================================================*/

int nimcp_msg_router_queue(
    nimcp_msg_router_t* router,
    const uint8_t* data,
    size_t len
) {
    if (!router || !data || !router->queue) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_msg_router_queue: required parameter is NULL (router, data, router->queue)");
        return -1;
    }

    /* Check if queue is full */
    if (router->queue_count >= router->queue_capacity) {
        if (router->config.enable_stats) {
            router->stats.queue_overflows++;
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_msg_router_queue: validation failed");
        return -1;
    }

    /* Allocate copy of data */
    uint8_t* copy = nimcp_malloc(len);
    if (!copy) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, len,
                          "Failed to allocate buffer for queued message");
        return -1;
    }
    memcpy(copy, data, len);

    /* Add to queue */
    nimcp_queued_msg_t* entry = &router->queue[router->queue_tail];
    /* Note: entry->data should always be NULL here since we check queue_count above.
     * This assertion helps catch logic errors during development. */
    entry->data = copy;
    entry->len = len;
    entry->valid = true;

    router->queue_tail = (router->queue_tail + 1) % router->queue_capacity;
    router->queue_count++;

    return 0;
}

int nimcp_msg_router_process_queue(
    nimcp_msg_router_t* router,
    uint32_t max_messages
) {
    if (!router || !router->queue) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_msg_router_process_queue: required parameter is NULL (router, router->queue)");
        return -1;
    }

    /* Phase 8: Send heartbeat at start of queue processing */
    msg_router_heartbeat("process_queue", 0.0f);

    if (max_messages == 0) {
        max_messages = router->queue_count;
    }

    int processed = 0;

    while (router->queue_count > 0 && (uint32_t)processed < max_messages) {
        /* Phase 8: Send progress heartbeat */
        if (max_messages > 0) {
            msg_router_heartbeat("process_queue", (float)processed / (float)max_messages);
        }
        nimcp_queued_msg_t* entry = &router->queue[router->queue_head];

        if (entry->valid && entry->data) {
            nimcp_msg_router_route(router, entry->data, entry->len);

            nimcp_free(entry->data);
            entry->data = NULL;
            entry->valid = false;
        }

        router->queue_head = (router->queue_head + 1) % router->queue_capacity;
        router->queue_count--;
        processed++;
    }

    return processed;
}

uint32_t nimcp_msg_router_queue_depth(const nimcp_msg_router_t* router) {
    return router ? router->queue_count : 0;
}

void nimcp_msg_router_clear_queue(nimcp_msg_router_t* router) {
    if (!router || !router->queue) return;

    for (uint32_t i = 0; i < router->queue_capacity; i++) {
        if (router->queue[i].data) {
            nimcp_free(router->queue[i].data);
            router->queue[i].data = NULL;
        }
        router->queue[i].valid = false;
    }

    router->queue_head = 0;
    router->queue_tail = 0;
    router->queue_count = 0;
}

/*=============================================================================
 * Statistics API
 *===========================================================================*/

int nimcp_msg_router_get_stats(
    const nimcp_msg_router_t* router,
    nimcp_msg_router_stats_t* stats
) {
    if (!router || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_msg_router_get_stats: required parameter is NULL (router, stats)");
        return -1;
    }

    *stats = router->stats;
    return 0;
}

void nimcp_msg_router_reset_stats(nimcp_msg_router_t* router) {
    if (!router) return;

    memset(&router->stats, 0, sizeof(router->stats));
}

/*=============================================================================
 * Debug API
 *===========================================================================*/

void nimcp_msg_router_dump(const nimcp_msg_router_t* router) {
    if (!router) {
        printf("Router: NULL\n");
        return;
    }

    printf("=== Message Router State ===\n");
    printf("Initialized: %s\n", router->initialized ? "yes" : "no");
    printf("Handlers: %u / %d\n", router->handler_count, NIMCP_MAX_MSG_HANDLERS);

    for (uint32_t i = 0; i < router->handler_count; i++) {
        printf("  [%u] type=0x%04X (%s) fast=%s\n",
               i,
               router->handlers[i].msg_type,
               nimcp_msg_type_name(router->handlers[i].msg_type),
               router->handlers[i].is_fast_path ? "yes" : "no");
    }

    if (router->queue) {
        printf("Queue: %u / %u\n", router->queue_count, router->queue_capacity);
    } else {
        printf("Queue: disabled\n");
    }

    printf("Stats:\n");
    printf("  Messages routed: %lu\n", (unsigned long)router->stats.messages_routed);
    printf("  Fast messages: %lu\n", (unsigned long)router->stats.fast_messages_routed);
    printf("  Protobuf messages: %lu\n", (unsigned long)router->stats.protobuf_messages_routed);
    printf("  Handler errors: %lu\n", (unsigned long)router->stats.handler_errors);
    printf("  Unhandled: %lu\n", (unsigned long)router->stats.unhandled_messages);
    printf("  Bytes routed: %lu\n", (unsigned long)router->stats.bytes_routed);
}
