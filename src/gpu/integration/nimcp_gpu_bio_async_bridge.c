/**
 * @file nimcp_gpu_bio_async_bridge.c
 * @brief GPU Module Bio-Async Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-01-13
 *
 * WHAT: Implementation of GPU-bio-async integration bridge
 * WHY:  Enable asynchronous GPU operations via biological signaling
 * HOW:  Message routing, handler registration, status broadcasting
 */

#include "gpu/integration/nimcp_gpu_bio_async_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <time.h>
#include <stdatomic.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(gpu_bio_async_bridge)

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Initialize message header
 */
static void init_message_header(
    bio_message_header_t* header,
    bio_message_type_t type,
    uint32_t source,
    uint32_t target,
    uint32_t payload_size,
    nimcp_bio_channel_type_t channel
) {
    if (!header) return;

    static _Atomic uint32_t sequence_counter = 0;

    header->type = type;
    header->sequence_id = atomic_fetch_add(&sequence_counter, 1) + 1;
    header->source_module = source;
    header->target_module = target;
    header->timestamp_us = get_time_us();
    header->channel = channel;
    header->payload_size = payload_size;
    header->flags = 0;
}

/* ============================================================================
 * Internal Message Handlers
 * ============================================================================ */

/**
 * @brief Handle incoming compute request
 */
static nimcp_error_t handle_compute_request(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    gpu_bio_bridge_t* bridge = (gpu_bio_bridge_t*)user_data;
    if (!bridge || !msg) return NIMCP_ERROR_NULL_POINTER;

    const gpu_compute_request_msg_t* request = (const gpu_compute_request_msg_t*)msg;

    NIMCP_LOGGING_DEBUG("gpu_bio_bridge: received compute request op_id=%llu device=%u",
        (unsigned long long)request->operation_id, request->device_id);

    /* Track pending operation */
    bridge->pending_operations++;
    bridge->stats.compute_requests_sent++;

    /* Complete promise if provided */
    if (response_promise) {
        gpu_compute_complete_msg_t response;
        memset(&response, 0, sizeof(response));
        response.operation_id = request->operation_id;
        response.device_id = request->device_id;
        response.result_code = 0;
        nimcp_bio_promise_complete_sized(response_promise, &response, sizeof(response));
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle incoming transfer request
 */
static nimcp_error_t handle_transfer_request(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    gpu_bio_bridge_t* bridge = (gpu_bio_bridge_t*)user_data;
    if (!bridge || !msg) return NIMCP_ERROR_NULL_POINTER;

    const gpu_transfer_request_msg_t* request = (const gpu_transfer_request_msg_t*)msg;

    NIMCP_LOGGING_DEBUG("gpu_bio_bridge: received transfer request id=%llu size=%zu",
        (unsigned long long)request->transfer_id, request->size);

    bridge->stats.transfer_requests_sent++;

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle incoming status query
 */
static nimcp_error_t handle_status_query(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    gpu_bio_bridge_t* bridge = (gpu_bio_bridge_t*)user_data;
    if (!bridge || !msg) return NIMCP_ERROR_NULL_POINTER;

    const gpu_status_msg_t* query = (const gpu_status_msg_t*)msg;

    /* Build status response */
    gpu_status_msg_t response;
    memset(&response, 0, sizeof(response));

    uint32_t device_id = query->device_id;
    if (device_id < bridge->device_count) {
        response.device_id = device_id;
        response.status = bridge->device_status[device_id];
        response.compute_utilization = bridge->device_utilization[device_id];
        response.memory_free = bridge->device_memory_free[device_id];
    }

    init_message_header(&response.header, (bio_message_type_t)GPU_MSG_STATUS_RESPONSE,
        BIO_MODULE_GPU_BRIDGE, query->header.source_module,
        sizeof(response), BIO_CHANNEL_ACETYLCHOLINE);

    if (response_promise) {
        nimcp_bio_promise_complete_sized(response_promise, &response, sizeof(response));
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle multi-GPU sync request
 */
static nimcp_error_t handle_sync_request(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    gpu_bio_bridge_t* bridge = (gpu_bio_bridge_t*)user_data;
    if (!bridge || !msg) return NIMCP_ERROR_NULL_POINTER;

    const gpu_multigpu_sync_msg_t* request = (const gpu_multigpu_sync_msg_t*)msg;

    NIMCP_LOGGING_DEBUG("gpu_bio_bridge: received sync request id=%llu devices=%u",
        (unsigned long long)request->sync_id, request->device_count);

    /* Perform sync if multigpu context available */
    if (bridge->multigpu_ctx) {
        bool synced = multigpu_synchronize(bridge->multigpu_ctx);
        if (!synced) {
            bridge->stats.sync_timeouts++;
            return GPU_BIO_ERROR_SYNC_TIMEOUT;
        }
    }

    bridge->stats.multigpu_syncs++;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int gpu_bio_bridge_default_config(gpu_bio_bridge_config_t* config) {
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    memset(config, 0, sizeof(gpu_bio_bridge_config_t));

    /* Bio-async settings */
    config->primary_channel = BIO_CHANNEL_DOPAMINE;
    config->error_channel = BIO_CHANNEL_NOREPINEPHRINE;
    config->enable_broadcast = true;
    config->broadcast_interval_ms = 1000;

    /* Multi-GPU settings */
    config->enable_multigpu_coordination = true;
    config->sync_timeout_ms = 5000;
    config->load_imbalance_threshold = 0.15f;

    /* Message handling */
    config->max_pending_operations = 1024;
    config->max_message_size = 64 * 1024;
    config->enable_operation_logging = false;

    /* Performance */
    config->enable_prefetch = true;
    config->enable_batching = true;
    config->batch_size = 32;

    return NIMCP_SUCCESS;
}

gpu_bio_bridge_t* gpu_bio_bridge_create(
    const gpu_bio_bridge_config_t* config,
    multigpu_context_t multigpu_ctx
) {
    gpu_bio_bridge_t* bridge = nimcp_malloc(sizeof(gpu_bio_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("gpu_bio_bridge_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    memset(bridge, 0, sizeof(gpu_bio_bridge_t));

    int result = gpu_bio_bridge_init(bridge, config, multigpu_ctx);
    if (result != NIMCP_SUCCESS) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gpu_bio_bridge_create: validation failed");
        return NULL;
    }

    return bridge;
}

int gpu_bio_bridge_init(
    gpu_bio_bridge_t* bridge,
    const gpu_bio_bridge_config_t* config,
    multigpu_context_t multigpu_ctx
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Initialize base bridge */
    int result = bridge_base_init(&bridge->base, BIO_MODULE_GPU_BRIDGE, "gpu_bio_bridge");
    if (result != 0) {
        NIMCP_LOGGING_ERROR("gpu_bio_bridge_init: base init failed");
        return result;
    }

    /* Apply configuration */
    gpu_bio_bridge_config_t default_config;
    if (!config) {
        gpu_bio_bridge_default_config(&default_config);
        config = &default_config;
    }
    memcpy(&bridge->config, config, sizeof(gpu_bio_bridge_config_t));

    /* Store multi-GPU context */
    bridge->multigpu_ctx = multigpu_ctx;

    /* Get device count */
    if (multigpu_ctx) {
        bridge->device_count = multigpu_get_device_count(multigpu_ctx);
    } else {
        bridge->device_count = 1;  /* Single GPU mode */
    }

    /* Allocate per-device arrays */
    if (bridge->device_count > 0) {
        bridge->device_status = nimcp_malloc(bridge->device_count * sizeof(gpu_device_status_t));
        bridge->device_utilization = nimcp_malloc(bridge->device_count * sizeof(float));
        bridge->device_memory_free = nimcp_malloc(bridge->device_count * sizeof(uint64_t));

        if (!bridge->device_status || !bridge->device_utilization || !bridge->device_memory_free) {
            NIMCP_LOGGING_ERROR("gpu_bio_bridge_init: per-device array allocation failed");
            if (bridge->device_status) nimcp_free(bridge->device_status);
            if (bridge->device_utilization) nimcp_free(bridge->device_utilization);
            if (bridge->device_memory_free) nimcp_free(bridge->device_memory_free);
            bridge_base_cleanup(&bridge->base);
            return NIMCP_ERROR_NO_MEMORY;
        }

        /* Initialize device state */
        for (uint32_t i = 0; i < bridge->device_count; i++) {
            bridge->device_status[i] = GPU_STATUS_IDLE;
            bridge->device_utilization[i] = 0.0f;
            bridge->device_memory_free[i] = 0;
        }
    }

    /* Initialize operation tracking */
    bridge->next_operation_id = 1;
    bridge->next_transfer_id = 1;
    bridge->pending_operations = 0;

    /* Initialize timing */
    bridge->last_broadcast_time_us = get_time_us();
    bridge->last_rebalance_time_us = bridge->last_broadcast_time_us;

    /* Clear statistics */
    memset(&bridge->stats, 0, sizeof(gpu_bio_bridge_stats_t));

    NIMCP_LOGGING_INFO("gpu_bio_bridge: initialized with %u devices", bridge->device_count);
    return NIMCP_SUCCESS;
}

void gpu_bio_bridge_destroy(gpu_bio_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect from bio-async */
    if (bridge->base.bio_async_enabled) {
        gpu_bio_bridge_disconnect_bio_async(bridge);
    }

    /* Free per-device arrays */
    if (bridge->device_status) {
        nimcp_free(bridge->device_status);
    }
    if (bridge->device_utilization) {
        nimcp_free(bridge->device_utilization);
    }
    if (bridge->device_memory_free) {
        nimcp_free(bridge->device_memory_free);
    }

    /* Cleanup base */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("gpu_bio_bridge: destroyed");
}

/* ============================================================================
 * Bio-Async Connection API
 * ============================================================================ */

int gpu_bio_bridge_connect_bio_async(gpu_bio_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;  /* Already connected */
    }

    /* Check router availability */
    if (!bio_router_is_initialized()) {
        NIMCP_LOGGING_WARN("gpu_bio_bridge: bio-router not initialized");
        return GPU_BIO_ERROR_ROUTER_UNAVAILABLE;
    }

    /* Register module with router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_GPU_BRIDGE,
        .module_name = "gpu_bio_bridge",
        .inbox_capacity = 256,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (!bridge->base.bio_ctx) {
        NIMCP_LOGGING_ERROR("gpu_bio_bridge: failed to register with bio-router");
        return GPU_BIO_ERROR_ROUTER_UNAVAILABLE;
    }

    /* Register message handlers */
    bio_router_register_handler(bridge->base.bio_ctx,
        (bio_message_type_t)GPU_MSG_COMPUTE_REQUEST, handle_compute_request);
    bio_router_register_handler(bridge->base.bio_ctx,
        (bio_message_type_t)GPU_MSG_TRANSFER_REQUEST, handle_transfer_request);
    bio_router_register_handler(bridge->base.bio_ctx,
        (bio_message_type_t)GPU_MSG_STATUS_QUERY, handle_status_query);
    bio_router_register_handler(bridge->base.bio_ctx,
        (bio_message_type_t)GPU_MSG_MULTIGPU_SYNC_REQUEST, handle_sync_request);

    bridge->base.bio_async_enabled = true;

    NIMCP_LOGGING_INFO("gpu_bio_bridge: connected to bio-async router");
    return NIMCP_SUCCESS;
}

int gpu_bio_bridge_disconnect_bio_async(gpu_bio_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (!bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;  /* Already disconnected */
    }

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("gpu_bio_bridge: disconnected from bio-async router");
    return NIMCP_SUCCESS;
}

bool gpu_bio_bridge_is_bio_async_connected(const gpu_bio_bridge_t* bridge) {
    return bridge && bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Compute Message API
 * ============================================================================ */

uint64_t gpu_bio_bridge_send_compute_request(
    gpu_bio_bridge_t* bridge,
    uint32_t device_id,
    gpu_operation_type_t operation,
    const void* data,
    size_t data_size,
    float priority
) {
    if (!bridge) return 0;

    if (!bridge->base.bio_async_enabled) {
        NIMCP_LOGGING_WARN("gpu_bio_bridge: not connected to bio-async");
        return 0;
    }

    if (device_id >= bridge->device_count) {
        NIMCP_LOGGING_ERROR("gpu_bio_bridge: invalid device_id %u", device_id);
        return 0;
    }

    if (bridge->pending_operations >= bridge->config.max_pending_operations) {
        NIMCP_LOGGING_WARN("gpu_bio_bridge: max pending operations reached");
        bridge->stats.message_drops++;
        return 0;
    }

    /* Build compute request message */
    gpu_compute_request_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    uint64_t operation_id = bridge->next_operation_id++;

    init_message_header(&msg.header, (bio_message_type_t)GPU_MSG_COMPUTE_REQUEST,
        BIO_MODULE_GPU_BRIDGE, BIO_MODULE_GPU_COMPUTE,
        sizeof(msg), bridge->config.primary_channel);

    msg.device_id = device_id;
    msg.operation = operation;
    msg.operation_id = operation_id;
    msg.data_size = data_size;
    msg.data_ptr = (void*)data;  /* Note: pointer, not copied */
    msg.priority = nimcp_clamp01(priority);
    msg.timeout_ms = bridge->config.sync_timeout_ms;

    /* Send message */
    nimcp_error_t result = bio_router_send(
        bridge->base.bio_ctx, &msg, sizeof(msg), 0);

    if (result != NIMCP_SUCCESS) {
        NIMCP_LOGGING_ERROR("gpu_bio_bridge: failed to send compute request");
        bridge->stats.message_drops++;
        return 0;
    }

    bridge->pending_operations++;
    bridge->stats.compute_requests_sent++;

    if (bridge->config.enable_operation_logging) {
        NIMCP_LOGGING_DEBUG("gpu_bio_bridge: sent compute request op_id=%llu device=%u op=%s",
            (unsigned long long)operation_id, device_id, gpu_op_type_name(operation));
    }

    return operation_id;
}

int gpu_bio_bridge_send_compute_complete(
    gpu_bio_bridge_t* bridge,
    uint64_t operation_id,
    uint32_t device_id,
    int32_t result_code,
    uint64_t elapsed_us
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (!bridge->base.bio_async_enabled) {
        return GPU_BIO_ERROR_ROUTER_UNAVAILABLE;
    }

    gpu_compute_complete_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_message_header(&msg.header, (bio_message_type_t)GPU_MSG_COMPUTE_COMPLETE,
        BIO_MODULE_GPU_BRIDGE, 0,  /* Broadcast */
        sizeof(msg), bridge->config.primary_channel);
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;

    msg.device_id = device_id;
    msg.operation_id = operation_id;
    msg.result_code = result_code;
    msg.elapsed_us = elapsed_us;

    if (device_id < bridge->device_count) {
        msg.gpu_utilization = bridge->device_utilization[device_id];
    }

    nimcp_error_t send_result = bio_router_broadcast(
        bridge->base.bio_ctx, &msg, sizeof(msg));

    if (send_result == NIMCP_SUCCESS) {
        if (bridge->pending_operations > 0) {
            bridge->pending_operations--;
        }
        bridge->stats.compute_requests_completed++;

        /* Update average latency */
        float new_latency = (float)elapsed_us;
        bridge->stats.avg_compute_latency_us =
            (bridge->stats.avg_compute_latency_us * 0.9f) + (new_latency * 0.1f);
    }

    return send_result;
}

/* ============================================================================
 * Transfer Message API
 * ============================================================================ */

uint64_t gpu_bio_bridge_send_transfer_request(
    gpu_bio_bridge_t* bridge,
    int32_t src_device,
    int32_t dst_device,
    const void* src_ptr,
    void* dst_ptr,
    size_t size,
    bool async
) {
    if (!bridge) return 0;

    if (!bridge->base.bio_async_enabled) {
        return 0;
    }

    gpu_transfer_request_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    uint64_t transfer_id = bridge->next_transfer_id++;

    /* Determine channel based on transfer type */
    nimcp_bio_channel_type_t channel = async ?
        BIO_CHANNEL_SEROTONIN :  /* Async = slow, sustained */
        BIO_CHANNEL_ACETYLCHOLINE;  /* Sync = fast, immediate */

    init_message_header(&msg.header, (bio_message_type_t)GPU_MSG_TRANSFER_REQUEST,
        BIO_MODULE_GPU_BRIDGE, BIO_MODULE_GPU_TRANSFER,
        sizeof(msg), channel);

    msg.src_device_id = (uint32_t)src_device;
    msg.dst_device_id = (uint32_t)dst_device;
    msg.transfer_id = transfer_id;
    msg.src_ptr = (void*)src_ptr;
    msg.dst_ptr = dst_ptr;
    msg.size = size;
    msg.async = async;

    nimcp_error_t result = bio_router_send(
        bridge->base.bio_ctx, &msg, sizeof(msg), 0);

    if (result != NIMCP_SUCCESS) {
        bridge->stats.message_drops++;
        return 0;
    }

    bridge->stats.transfer_requests_sent++;

    /* Track P2P transfers */
    if (src_device >= 0 && dst_device >= 0 && src_device != dst_device) {
        bridge->stats.p2p_transfers++;
    }

    return transfer_id;
}

int gpu_bio_bridge_send_transfer_complete(
    gpu_bio_bridge_t* bridge,
    uint64_t transfer_id,
    int32_t result_code,
    uint64_t elapsed_us,
    float bandwidth_gbps
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (!bridge->base.bio_async_enabled) {
        return GPU_BIO_ERROR_ROUTER_UNAVAILABLE;
    }

    gpu_transfer_complete_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_message_header(&msg.header, (bio_message_type_t)GPU_MSG_TRANSFER_COMPLETE,
        BIO_MODULE_GPU_BRIDGE, 0,
        sizeof(msg), bridge->config.primary_channel);
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;

    msg.transfer_id = transfer_id;
    msg.result_code = result_code;
    msg.elapsed_us = elapsed_us;
    msg.bandwidth_gbps = bandwidth_gbps;

    nimcp_error_t send_result = bio_router_broadcast(
        bridge->base.bio_ctx, &msg, sizeof(msg));

    if (send_result == NIMCP_SUCCESS) {
        bridge->stats.transfer_requests_completed++;

        float new_latency = (float)elapsed_us;
        bridge->stats.avg_transfer_latency_us =
            (bridge->stats.avg_transfer_latency_us * 0.9f) + (new_latency * 0.1f);

        if (bandwidth_gbps > bridge->stats.peak_bandwidth_gbps) {
            bridge->stats.peak_bandwidth_gbps = bandwidth_gbps;
        }
    } else {
        bridge->stats.transfer_failures++;
    }

    return send_result;
}

/* ============================================================================
 * Status Message API
 * ============================================================================ */

int gpu_bio_bridge_broadcast_status(gpu_bio_bridge_t* bridge, uint32_t device_id) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (!bridge->base.bio_async_enabled) {
        return GPU_BIO_ERROR_ROUTER_UNAVAILABLE;
    }

    if (device_id >= bridge->device_count) {
        return GPU_BIO_ERROR_INVALID_DEVICE;
    }

    gpu_status_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_message_header(&msg.header, (bio_message_type_t)GPU_MSG_STATUS_RESPONSE,
        BIO_MODULE_GPU_BRIDGE, 0,
        sizeof(msg), BIO_CHANNEL_ACETYLCHOLINE);
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;

    msg.device_id = device_id;
    msg.status = bridge->device_status[device_id];
    msg.compute_utilization = bridge->device_utilization[device_id];
    msg.memory_free = bridge->device_memory_free[device_id];

    /* Query multi-GPU context for additional info if available */
    if (bridge->multigpu_ctx) {
        multigpu_device_info_t info;
        if (multigpu_get_device_info(bridge->multigpu_ctx, device_id, &info)) {
            msg.compute_utilization = info.compute_utilization;
            msg.memory_utilization = info.memory_utilization;
            msg.memory_free = info.free_memory_bytes;
            msg.memory_total = info.total_memory_bytes;
        }
    }

    nimcp_error_t result = bio_router_broadcast(
        bridge->base.bio_ctx, &msg, sizeof(msg));

    if (result == NIMCP_SUCCESS) {
        bridge->stats.status_updates_sent++;
    }

    return result;
}

int gpu_bio_bridge_broadcast_all_status(gpu_bio_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    for (uint32_t i = 0; i < bridge->device_count; i++) {
        int result = gpu_bio_bridge_broadcast_status(bridge, i);
        if (result != NIMCP_SUCCESS) {
            return result;
        }
    }

    bridge->last_broadcast_time_us = get_time_us();
    return NIMCP_SUCCESS;
}

int gpu_bio_bridge_send_error_report(
    gpu_bio_bridge_t* bridge,
    uint32_t device_id,
    int32_t error_code,
    const char* error_message
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (!bridge->base.bio_async_enabled) {
        return GPU_BIO_ERROR_ROUTER_UNAVAILABLE;
    }

    gpu_status_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_message_header(&msg.header, (bio_message_type_t)GPU_MSG_ERROR_REPORT,
        BIO_MODULE_GPU_BRIDGE, 0,
        sizeof(msg), bridge->config.error_channel);
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST | BIO_MSG_FLAG_URGENT;

    msg.device_id = device_id;
    msg.status = GPU_STATUS_ERROR;

    if (device_id < bridge->device_count) {
        bridge->device_status[device_id] = GPU_STATUS_ERROR;
    }

    nimcp_error_t result = bio_router_broadcast(
        bridge->base.bio_ctx, &msg, sizeof(msg));

    NIMCP_LOGGING_ERROR("gpu_bio_bridge: error on device %u: %s (code %d)",
        device_id, error_message ? error_message : "unknown", error_code);

    return result;
}

/* ============================================================================
 * Multi-GPU Coordination API
 * ============================================================================ */

uint64_t gpu_bio_bridge_send_sync_request(
    gpu_bio_bridge_t* bridge,
    const uint32_t* device_ids,
    uint32_t device_count,
    uint32_t timeout_ms
) {
    if (!bridge) return 0;

    if (!bridge->base.bio_async_enabled) {
        return 0;
    }

    static uint64_t sync_counter = 0;
    uint64_t sync_id = ++sync_counter;

    gpu_multigpu_sync_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_message_header(&msg.header, (bio_message_type_t)GPU_MSG_MULTIGPU_SYNC_REQUEST,
        BIO_MODULE_GPU_BRIDGE, BIO_MODULE_GPU_MULTIGPU,
        sizeof(msg), BIO_CHANNEL_ACETYLCHOLINE);

    msg.device_ids = (uint32_t*)device_ids;
    msg.device_count = device_count > 0 ? device_count : bridge->device_count;
    msg.sync_id = sync_id;
    msg.timeout_ms = timeout_ms > 0 ? timeout_ms : bridge->config.sync_timeout_ms;

    nimcp_error_t result = bio_router_send(
        bridge->base.bio_ctx, &msg, sizeof(msg), 0);

    if (result != NIMCP_SUCCESS) {
        return 0;
    }

    return sync_id;
}

int gpu_bio_bridge_send_rebalance_request(gpu_bio_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (!bridge->base.bio_async_enabled) {
        return GPU_BIO_ERROR_ROUTER_UNAVAILABLE;
    }

    bio_message_header_t msg;
    init_message_header(&msg, (bio_message_type_t)GPU_MSG_REBALANCE_REQUEST,
        BIO_MODULE_GPU_BRIDGE, BIO_MODULE_GPU_MULTIGPU,
        sizeof(msg), BIO_CHANNEL_DOPAMINE);

    nimcp_error_t result = bio_router_send(
        bridge->base.bio_ctx, &msg, sizeof(msg), 0);

    if (result == NIMCP_SUCCESS) {
        bridge->stats.rebalance_events++;
        bridge->last_rebalance_time_us = get_time_us();
    }

    return result;
}

int gpu_bio_bridge_broadcast_coordination_status(gpu_bio_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (!bridge->base.bio_async_enabled) {
        return GPU_BIO_ERROR_ROUTER_UNAVAILABLE;
    }

    gpu_coordination_status_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_message_header(&msg.header, (bio_message_type_t)GPU_MSG_DEVICE_ASSIGNMENT,
        BIO_MODULE_GPU_BRIDGE, 0,
        sizeof(msg), BIO_CHANNEL_ACETYLCHOLINE);
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;

    msg.active_device_count = bridge->device_count;
    msg.p2p_enabled = true;  /* Assume P2P if multi-GPU */

    /* Calculate load imbalance */
    if (bridge->device_count > 1) {
        float min_util = 1.0f, max_util = 0.0f;
        for (uint32_t i = 0; i < bridge->device_count; i++) {
            float util = bridge->device_utilization[i];
            if (util < min_util) min_util = util;
            if (util > max_util) max_util = util;
        }
        msg.load_imbalance = max_util - min_util;
    }

    msg.last_rebalance_us = get_time_us() - bridge->last_rebalance_time_us;

    return bio_router_broadcast(bridge->base.bio_ctx, &msg, sizeof(msg));
}

/* ============================================================================
 * Query API
 * ============================================================================ */

gpu_device_status_t gpu_bio_bridge_get_device_status(
    const gpu_bio_bridge_t* bridge,
    uint32_t device_id
) {
    if (!bridge || device_id >= bridge->device_count) {
        return GPU_STATUS_UNKNOWN;
    }
    return bridge->device_status[device_id];
}

float gpu_bio_bridge_get_device_utilization(
    const gpu_bio_bridge_t* bridge,
    uint32_t device_id
) {
    if (!bridge || device_id >= bridge->device_count) {
        return 0.0f;
    }
    return bridge->device_utilization[device_id];
}

uint32_t gpu_bio_bridge_get_pending_operations(const gpu_bio_bridge_t* bridge) {
    if (!bridge) return 0;
    return bridge->pending_operations;
}

int gpu_bio_bridge_get_stats(
    const gpu_bio_bridge_t* bridge,
    gpu_bio_bridge_stats_t* stats
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");
    memcpy(stats, &bridge->stats, sizeof(gpu_bio_bridge_stats_t));
    return NIMCP_SUCCESS;
}

int gpu_bio_bridge_reset_stats(gpu_bio_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    memset(&bridge->stats, 0, sizeof(gpu_bio_bridge_stats_t));
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Update API
 * ============================================================================ */

int gpu_bio_bridge_update(gpu_bio_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Update device status from multi-GPU context */
    if (bridge->multigpu_ctx) {
        for (uint32_t i = 0; i < bridge->device_count; i++) {
            multigpu_device_info_t info;
            if (multigpu_get_device_info(bridge->multigpu_ctx, i, &info)) {
                bridge->device_utilization[i] = info.compute_utilization;
                bridge->device_memory_free[i] = info.free_memory_bytes;

                /* Update status based on utilization */
                if (info.compute_utilization > 0.95f) {
                    bridge->device_status[i] = GPU_STATUS_OVERLOADED;
                } else if (info.compute_utilization > 0.1f) {
                    bridge->device_status[i] = GPU_STATUS_BUSY;
                } else {
                    bridge->device_status[i] = GPU_STATUS_IDLE;
                }
            }
        }
    }

    /* Check if broadcast is needed */
    if (bridge->config.enable_broadcast && bridge->base.bio_async_enabled) {
        uint64_t now_us = get_time_us();
        uint64_t interval_us = (uint64_t)bridge->config.broadcast_interval_ms * 1000ULL;

        if (now_us - bridge->last_broadcast_time_us >= interval_us) {
            gpu_bio_bridge_broadcast_all_status(bridge);
        }
    }

    /* Process incoming messages */
    if (bridge->base.bio_async_enabled) {
        gpu_bio_bridge_process_messages(bridge, 0);
    }

    bridge_base_record_update(&bridge->base);
    return NIMCP_SUCCESS;
}

uint32_t gpu_bio_bridge_process_messages(
    gpu_bio_bridge_t* bridge,
    uint32_t max_messages
) {
    if (!bridge || !bridge->base.bio_ctx) return 0;
    return bio_router_process_inbox(bridge->base.bio_ctx, max_messages);
}

/* ============================================================================
 * Message Handler Registration
 * ============================================================================ */

int gpu_bio_bridge_register_handler(
    gpu_bio_bridge_t* bridge,
    gpu_message_type_t msg_type,
    bio_message_handler_t handler
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(handler, NIMCP_ERROR_NULL_POINTER, "handler is NULL");

    if (!bridge->base.bio_ctx) {
        return GPU_BIO_ERROR_ROUTER_UNAVAILABLE;
    }

    return bio_router_register_handler(
        bridge->base.bio_ctx,
        (bio_message_type_t)msg_type,
        handler
    );
}

int gpu_bio_bridge_unregister_handler(
    gpu_bio_bridge_t* bridge,
    gpu_message_type_t msg_type
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (!bridge->base.bio_ctx) {
        return GPU_BIO_ERROR_ROUTER_UNAVAILABLE;
    }

    return bio_router_unregister_handler(
        bridge->base.bio_ctx,
        (bio_message_type_t)msg_type
    );
}
