/**
 * @file nimcp_vae_bio_async.c
 * @brief VAE Bio-Async Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-30
 *
 * Implements bio-async message handling for the VAE cognitive system.
 *
 * BIO_MODULE: 0x1F00
 */

#include "cognitive/vae/nimcp_vae_bio_async.h"
#include "cognitive/vae/nimcp_vae.h"
#include "cognitive/vae/nimcp_vae_latent.h"

#include "utils/logging/nimcp_logging.h"
#include "utils/tensor/nimcp_tensor_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdatomic.h>

/* ============================================================================
 * Module Constants
 * ============================================================================ */

#define VAE_BIO_ASYNC_MODULE_ID     BIO_MODULE_VAE_BIO_ASYNC
#define VAE_BIO_ASYNC_MODULE_NAME   "VAE-BioAsync"

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static uint64_t get_timestamp_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Initialize message header
 */
static void init_msg_header(vae_bio_msg_header_t* header,
                            vae_bio_message_type_t type,
                            vae_bio_async_bridge_t* bridge,
                            uint8_t priority,
                            nimcp_bio_channel_type_t channel)
{
    if (!header || !bridge) return;

    header->type = type;
    header->sequence_id = bridge->next_sequence_id++;
    header->timestamp_us = get_timestamp_us();
    header->source_module = BIO_MODULE_VAE_BIO_ASYNC;
    header->priority = priority;
    header->channel = (uint8_t)channel;
}

/**
 * @brief Send message via bio-router
 */
static int send_message(vae_bio_async_bridge_t* bridge,
                        const void* msg,
                        size_t msg_size,
                        nimcp_bio_channel_type_t channel,
                        uint8_t priority)
{
    if (!bridge || !msg) return NIMCP_ERROR_VAE_BIO_ASYNC_NULL;
    if (bridge->state != VAE_BIO_ASYNC_CONNECTED) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_BIO_ASYNC_NOT_CONNECTED, "vae_bio_async: error condition");
        return NIMCP_ERROR_VAE_BIO_ASYNC_NOT_CONNECTED;
    }

    /* Send via bio-router */
    nimcp_error_t result = bio_router_send(
        bridge->bio_context,
        msg,
        msg_size,
        1000  /* 1 second timeout */
    );

    if (result == NIMCP_SUCCESS) {
        bridge->stats.messages_sent++;
        bridge->stats.last_activity_us = get_timestamp_us();
    } else {
        bridge->stats.messages_dropped++;
        if (bridge->config.enable_logging) {
            LOG_WARNING("[VAE-BioAsync] Failed to send message: %d", result);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_BIO_ASYNC_SEND_FAILED, "vae_bio_async: error condition");
        return NIMCP_ERROR_VAE_BIO_ASYNC_SEND_FAILED;
    }

    return 0;
}

/**
 * @brief Send message with async response
 */
static int send_message_async(vae_bio_async_bridge_t* bridge,
                               const void* msg,
                               size_t msg_size,
                               nimcp_bio_channel_type_t channel,
                               nimcp_bio_promise_t* promise)
{
    if (!bridge || !msg) return NIMCP_ERROR_VAE_BIO_ASYNC_NULL;
    if (bridge->state != VAE_BIO_ASYNC_CONNECTED) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_BIO_ASYNC_NOT_CONNECTED, "vae_bio_async: error condition");
        return NIMCP_ERROR_VAE_BIO_ASYNC_NOT_CONNECTED;
    }

    /* Send async via bio-router */
    *promise = bio_router_send_async(
        bridge->bio_context,
        msg,
        msg_size,
        channel
    );

    bridge->stats.messages_sent++;
    bridge->stats.last_activity_us = get_timestamp_us();
    bridge->pending_count++;

    return 0;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int vae_bio_async_default_config(vae_bio_async_config_t* config)
{
    if (!config) return NIMCP_ERROR_VAE_BIO_ASYNC_NULL;

    memset(config, 0, sizeof(*config));

    /* Channel assignments (biological metaphor) */
    config->encode_channel = BIO_CHANNEL_ACETYLCHOLINE;   /* Fast attention */
    config->decode_channel = BIO_CHANNEL_ACETYLCHOLINE;   /* Fast attention */
    config->alert_channel = BIO_CHANNEL_NOREPINEPHRINE;   /* Alertness */
    config->health_channel = BIO_CHANNEL_SEROTONIN;       /* State/mood */

    /* Timing */
    config->heartbeat_interval_ms = 1000;    /* 1 Hz heartbeat */
    config->metrics_interval_ms = 5000;      /* 0.2 Hz metrics */
    config->health_check_interval_ms = 10000;/* 0.1 Hz health */

    /* Priorities (0-15, higher = more urgent) */
    config->encode_priority = 8;    /* Medium-high */
    config->decode_priority = 8;    /* Medium-high */
    config->alert_priority = 12;    /* High (anomalies) */
    config->health_priority = 4;    /* Low (background) */

    /* Message buffering */
    config->inbox_capacity = 64;
    config->outbox_capacity = 64;
    config->batch_encode_requests = false;
    config->batch_threshold = 8;

    /* Features */
    config->enable_anomaly_detection = true;
    config->enable_collapse_detection = true;
    config->enable_metrics_reporting = true;
    config->enable_logging = false;

    return 0;
}

vae_bio_async_bridge_t* vae_bio_async_create(const vae_bio_async_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_bio_async_create: config is NULL");
        return NULL;
    }

    vae_bio_async_bridge_t* bridge = nimcp_calloc(1, sizeof(vae_bio_async_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_bio_async_create: bridge is NULL");
        return NULL;
    }

    bridge->config = *config;
    bridge->state = VAE_BIO_ASYNC_DISCONNECTED;
    bridge->is_initialized = false;
    bridge->creation_time_us = get_timestamp_us();
    bridge->next_sequence_id = 1;

    /* Initialize statistics */
    bridge->stats.creation_time_us = bridge->creation_time_us;

    bridge->is_initialized = true;

    if (config->enable_logging) {
        LOG_INFO("[VAE-BioAsync] Bridge created");
    }

    return bridge;
}

void vae_bio_async_destroy(vae_bio_async_bridge_t* bridge)
{
    if (!bridge) return;

    /* Disconnect first */
    vae_bio_async_disconnect(bridge);

    /* Free pending responses tracking (if implemented) */
    /* nimcp_free(bridge->pending_responses); */

    nimcp_free(bridge);
    bridge = NULL;
}

int vae_bio_async_connect_vae(vae_bio_async_bridge_t* bridge, vae_system_t* vae)
{
    if (!bridge) return NIMCP_ERROR_VAE_BIO_ASYNC_NULL;
    if (!vae) return NIMCP_ERROR_VAE_BIO_ASYNC_NULL;

    bridge->vae = vae;

    if (bridge->config.enable_logging) {
        LOG_INFO("[VAE-BioAsync] VAE system connected");
    }

    return 0;
}

int vae_bio_async_connect_router(vae_bio_async_bridge_t* bridge)
{
    if (!bridge) return NIMCP_ERROR_VAE_BIO_ASYNC_NULL;

    bridge->state = VAE_BIO_ASYNC_CONNECTING;

    /* Check if bio-router is initialized */
    if (!bio_router_is_initialized()) {
        if (bridge->config.enable_logging) {
            LOG_WARNING("[VAE-BioAsync] Bio-router not initialized");
        }
        bridge->state = VAE_BIO_ASYNC_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_BIO_ASYNC_NOT_CONNECTED, "vae_bio_async: error condition");
        return NIMCP_ERROR_VAE_BIO_ASYNC_NOT_CONNECTED;
    }

    /* Register with bio-router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_VAE_BIO_ASYNC,
        .module_name = VAE_BIO_ASYNC_MODULE_NAME,
        .inbox_capacity = bridge->config.inbox_capacity,
        .user_data = bridge
    };

    bridge->bio_context = bio_router_register_module(&info);
    if (!bridge->bio_context) {
        bridge->state = VAE_BIO_ASYNC_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_BIO_ASYNC_NOT_CONNECTED, "vae_bio_async: error condition");
        return NIMCP_ERROR_VAE_BIO_ASYNC_NOT_CONNECTED;
    }

    /* Register message handlers */
    int ret = vae_bio_async_register_handlers(bridge);
    if (ret != 0) {
        bio_router_unregister_module(bridge->bio_context);
        bridge->bio_context = NULL;
        bridge->state = VAE_BIO_ASYNC_ERROR;
        return ret;
    }

    bridge->state = VAE_BIO_ASYNC_CONNECTED;
    bridge->last_heartbeat_us = get_timestamp_us();

    if (bridge->config.enable_logging) {
        LOG_INFO("[VAE-BioAsync] Connected to bio-router");
    }

    return 0;
}

int vae_bio_async_disconnect(vae_bio_async_bridge_t* bridge)
{
    if (!bridge) return NIMCP_ERROR_VAE_BIO_ASYNC_NULL;

    if (bridge->state == VAE_BIO_ASYNC_CONNECTED ||
        bridge->state == VAE_BIO_ASYNC_PROCESSING) {

        /* Unregister handlers */
        vae_bio_async_unregister_handlers(bridge);

        /* Unregister from router */
        if (bridge->bio_context) {
            bio_router_unregister_module(bridge->bio_context);
            bridge->bio_context = NULL;
        }
    }

    bridge->state = VAE_BIO_ASYNC_DISCONNECTED;
    bridge->vae = NULL;

    if (bridge->config.enable_logging) {
        LOG_INFO("[VAE-BioAsync] Disconnected from bio-router");
    }

    return 0;
}

bool vae_bio_async_is_connected(const vae_bio_async_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return bridge->state == VAE_BIO_ASYNC_CONNECTED ||
           bridge->state == VAE_BIO_ASYNC_PROCESSING;
}

/* ============================================================================
 * Handler Registration API
 * ============================================================================ */

int vae_bio_async_register_handlers(vae_bio_async_bridge_t* bridge)
{
    if (!bridge) return NIMCP_ERROR_VAE_BIO_ASYNC_NULL;
    if (!bridge->bio_context) return NIMCP_ERROR_VAE_BIO_ASYNC_NOT_CONNECTED;

    nimcp_error_t result;

    /* Register encode request handler */
    result = bio_router_register_handler(
        bridge->bio_context,
        BIO_MSG_VAE_ENCODE_REQUEST,
        vae_bio_async_handle_encode_request
    );
    if (result != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_BIO_ASYNC_HANDLER_FAIL, "vae_bio_async: error condition");
        return NIMCP_ERROR_VAE_BIO_ASYNC_HANDLER_FAIL;
    }

    /* Register decode request handler */
    result = bio_router_register_handler(
        bridge->bio_context,
        BIO_MSG_VAE_DECODE_REQUEST,
        vae_bio_async_handle_decode_request
    );
    if (result != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_BIO_ASYNC_HANDLER_FAIL, "vae_bio_async: error condition");
        return NIMCP_ERROR_VAE_BIO_ASYNC_HANDLER_FAIL;
    }

    /* Register sample request handler */
    result = bio_router_register_handler(
        bridge->bio_context,
        BIO_MSG_VAE_SAMPLE_REQUEST,
        vae_bio_async_handle_sample_request
    );
    if (result != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_BIO_ASYNC_HANDLER_FAIL, "vae_bio_async: error condition");
        return NIMCP_ERROR_VAE_BIO_ASYNC_HANDLER_FAIL;
    }

    /* Register FEP sync handler */
    result = bio_router_register_handler(
        bridge->bio_context,
        BIO_MSG_VAE_FEP_SYNC_REQUEST,
        vae_bio_async_handle_fep_sync_request
    );
    if (result != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_BIO_ASYNC_HANDLER_FAIL, "vae_bio_async: error condition");
        return NIMCP_ERROR_VAE_BIO_ASYNC_HANDLER_FAIL;
    }

    bridge->handlers_registered = true;

    if (bridge->config.enable_logging) {
        LOG_INFO("[VAE-BioAsync] Registered %d message handlers", 4);
    }

    return 0;
}

int vae_bio_async_unregister_handlers(vae_bio_async_bridge_t* bridge)
{
    if (!bridge) return NIMCP_ERROR_VAE_BIO_ASYNC_NULL;

    if (bridge->bio_context && bridge->handlers_registered) {
        bio_router_clear_handlers(bridge->bio_context);
        bridge->handlers_registered = false;
    }

    return 0;
}

int vae_bio_async_register_handler(vae_bio_async_bridge_t* bridge,
                                    vae_bio_message_type_t msg_type,
                                    bio_message_handler_t handler,
                                    void* user_data)
{
    if (!bridge || !handler) return NIMCP_ERROR_VAE_BIO_ASYNC_NULL;
    if (!bridge->bio_context) return NIMCP_ERROR_VAE_BIO_ASYNC_NOT_CONNECTED;

    (void)user_data; /* May be used for custom handler context */

    nimcp_error_t result = bio_router_register_handler(
        bridge->bio_context,
        msg_type,
        handler
    );

    return (result == NIMCP_SUCCESS) ? 0 : NIMCP_ERROR_VAE_BIO_ASYNC_HANDLER_FAIL;
}

/* ============================================================================
 * Message Sending API
 * ============================================================================ */

int vae_bio_async_send_encode_request(vae_bio_async_bridge_t* bridge,
                                       const float* input, uint32_t input_dim,
                                       nimcp_bio_promise_t* promise)
{
    if (!bridge || !input || !promise) return NIMCP_ERROR_VAE_BIO_ASYNC_NULL;
    if (input_dim > VAE_BIO_ASYNC_MAX_INPUT_DIM) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vae_bio_async_send_encode_request: validation failed");
        return -1;
    }

    vae_bio_msg_encode_request_t msg;
    memset(&msg, 0, sizeof(msg));

    init_msg_header(&msg.header, BIO_MSG_VAE_ENCODE_REQUEST, bridge,
                    bridge->config.encode_priority,
                    bridge->config.encode_channel);

    memcpy(msg.input, input, input_dim * sizeof(float));
    msg.input_dim = input_dim;
    msg.return_full_result = true;

    return send_message_async(bridge, &msg, sizeof(msg),
                               bridge->config.encode_channel, promise);
}

int vae_bio_async_send_decode_request(vae_bio_async_bridge_t* bridge,
                                       const float* latent, uint32_t latent_dim,
                                       nimcp_bio_promise_t* promise)
{
    if (!bridge || !latent || !promise) return NIMCP_ERROR_VAE_BIO_ASYNC_NULL;
    if (latent_dim > VAE_BIO_ASYNC_MAX_LATENT_DIM) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vae_bio_async_send_decode_request: validation failed");
        return -1;
    }

    vae_bio_msg_decode_request_t msg;
    memset(&msg, 0, sizeof(msg));

    init_msg_header(&msg.header, BIO_MSG_VAE_DECODE_REQUEST, bridge,
                    bridge->config.decode_priority,
                    bridge->config.decode_channel);

    memcpy(msg.latent, latent, latent_dim * sizeof(float));
    msg.latent_dim = latent_dim;

    return send_message_async(bridge, &msg, sizeof(msg),
                               bridge->config.decode_channel, promise);
}

int vae_bio_async_send_sample_request(vae_bio_async_bridge_t* bridge,
                                       uint32_t num_samples,
                                       float temperature,
                                       nimcp_bio_promise_t* promise)
{
    if (!bridge || !promise) return NIMCP_ERROR_VAE_BIO_ASYNC_NULL;

    vae_bio_msg_sample_request_t msg;
    memset(&msg, 0, sizeof(msg));

    init_msg_header(&msg.header, BIO_MSG_VAE_SAMPLE_REQUEST, bridge,
                    bridge->config.encode_priority,
                    bridge->config.encode_channel);

    msg.num_samples = num_samples;
    msg.temperature = temperature;
    msg.from_prior = true;

    return send_message_async(bridge, &msg, sizeof(msg),
                               bridge->config.encode_channel, promise);
}

int vae_bio_async_send_free_energy(vae_bio_async_bridge_t* bridge,
                                    float free_energy,
                                    float inaccuracy,
                                    float complexity)
{
    if (!bridge) return NIMCP_ERROR_VAE_BIO_ASYNC_NULL;

    vae_bio_msg_free_energy_t msg;
    memset(&msg, 0, sizeof(msg));

    init_msg_header(&msg.header, BIO_MSG_VAE_FEP_FREE_ENERGY, bridge,
                    bridge->config.encode_priority,
                    BIO_CHANNEL_DOPAMINE);  /* Reward channel */

    msg.free_energy = free_energy;
    msg.inaccuracy = inaccuracy;
    msg.complexity = complexity;
    msg.elbo = -free_energy;

    return send_message(bridge, &msg, sizeof(msg),
                        BIO_CHANNEL_DOPAMINE,
                        bridge->config.encode_priority);
}

int vae_bio_async_send_anomaly(vae_bio_async_bridge_t* bridge,
                                uint32_t anomaly_type,
                                float anomaly_score,
                                const char* description)
{
    if (!bridge) return NIMCP_ERROR_VAE_BIO_ASYNC_NULL;

    vae_bio_msg_anomaly_t msg;
    memset(&msg, 0, sizeof(msg));

    init_msg_header(&msg.header, BIO_MSG_VAE_ANOMALY_DETECTED, bridge,
                    bridge->config.alert_priority,
                    bridge->config.alert_channel);

    msg.anomaly_type = anomaly_type;
    msg.anomaly_score = anomaly_score;
    if (description) {
        strncpy(msg.description, description, sizeof(msg.description) - 1);
    }

    return send_message(bridge, &msg, sizeof(msg),
                        bridge->config.alert_channel,
                        bridge->config.alert_priority);
}

int vae_bio_async_send_heartbeat(vae_bio_async_bridge_t* bridge)
{
    if (!bridge) return NIMCP_ERROR_VAE_BIO_ASYNC_NULL;

    vae_bio_msg_heartbeat_t msg;
    memset(&msg, 0, sizeof(msg));

    init_msg_header(&msg.header, BIO_MSG_VAE_HEARTBEAT, bridge,
                    bridge->config.health_priority,
                    bridge->config.health_channel);

    msg.heartbeat_count = ++bridge->heartbeat_count;
    msg.uptime_us = get_timestamp_us() - bridge->creation_time_us;

    /* Get health from VAE if connected */
    if (bridge->vae) {
        vae_state_t state = vae_get_state(bridge->vae);
        msg.state = (uint32_t)state;
        msg.health_score = (state == VAE_STATE_IDLE) ? 1.0f : 0.5f;
    } else {
        msg.health_score = 0.0f;
    }

    bridge->last_heartbeat_us = get_timestamp_us();
    bridge->stats.heartbeats_sent++;

    return send_message(bridge, &msg, sizeof(msg),
                        bridge->config.health_channel,
                        bridge->config.health_priority);
}

int vae_bio_async_send_health_status(vae_bio_async_bridge_t* bridge)
{
    if (!bridge) return NIMCP_ERROR_VAE_BIO_ASYNC_NULL;

    vae_bio_msg_health_status_t msg;
    memset(&msg, 0, sizeof(msg));

    init_msg_header(&msg.header, BIO_MSG_VAE_HEALTH_STATUS, bridge,
                    bridge->config.health_priority,
                    bridge->config.health_channel);

    /* Get stats from VAE */
    if (bridge->vae) {
        vae_stats_t stats;
        if (vae_get_stats(bridge->vae, &stats) == 0) {
            msg.avg_recon_error = stats.ema_reconstruction_loss;
            msg.avg_kl_divergence = stats.ema_kl_divergence;
            msg.avg_elbo = -stats.ema_free_energy;  /* ELBO = -F */
            msg.total_encodes = stats.total_encode_calls;
            msg.total_decodes = stats.total_decode_calls;
            msg.is_healthy = stats.ema_reconstruction_loss < 0.5f;
        }
    }

    snprintf(msg.status_message, sizeof(msg.status_message),
             "VAE healthy, sent=%lu, rcvd=%lu",
             (unsigned long)bridge->stats.messages_sent,
             (unsigned long)bridge->stats.messages_received);

    return send_message(bridge, &msg, sizeof(msg),
                        bridge->config.health_channel,
                        bridge->config.health_priority);
}

int vae_bio_async_send_metrics(vae_bio_async_bridge_t* bridge)
{
    if (!bridge) return NIMCP_ERROR_VAE_BIO_ASYNC_NULL;

    vae_bio_msg_metrics_t msg;
    memset(&msg, 0, sizeof(msg));

    init_msg_header(&msg.header, BIO_MSG_VAE_METRICS_REPORT, bridge,
                    bridge->config.health_priority,
                    bridge->config.health_channel);

    /* Get stats from VAE */
    if (bridge->vae) {
        vae_stats_t stats;
        if (vae_get_stats(bridge->vae, &stats) == 0) {
            msg.avg_encode_time_us = stats.avg_encode_latency_us;
            msg.avg_decode_time_us = stats.avg_decode_latency_us;
        }
    }

    msg.messages_sent = bridge->stats.messages_sent;
    msg.messages_received = bridge->stats.messages_received;

    /* Calculate throughput */
    uint64_t uptime_us = get_timestamp_us() - bridge->creation_time_us;
    if (uptime_us > 0) {
        msg.throughput_ops_sec = (float)(bridge->stats.messages_sent +
                                         bridge->stats.messages_received) *
                                 1000000.0f / (float)uptime_us;
    }

    return send_message(bridge, &msg, sizeof(msg),
                        bridge->config.health_channel,
                        bridge->config.health_priority);
}

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

uint32_t vae_bio_async_process_messages(vae_bio_async_bridge_t* bridge,
                                         uint32_t max_messages)
{
    if (!bridge || !bridge->bio_context) return 0;

    bridge->state = VAE_BIO_ASYNC_PROCESSING;

    uint32_t processed = bio_router_process_inbox(bridge->bio_context,
                                                   max_messages);

    bridge->stats.messages_received += processed;
    bridge->stats.last_activity_us = get_timestamp_us();

    bridge->state = VAE_BIO_ASYNC_CONNECTED;

    return processed;
}

int vae_bio_async_periodic_update(vae_bio_async_bridge_t* bridge)
{
    if (!bridge) return NIMCP_ERROR_VAE_BIO_ASYNC_NULL;

    uint64_t now = get_timestamp_us();

    /* Send heartbeat if interval elapsed */
    uint64_t heartbeat_elapsed = (now - bridge->last_heartbeat_us) / 1000;
    if (heartbeat_elapsed >= bridge->config.heartbeat_interval_ms) {
        vae_bio_async_send_heartbeat(bridge);
    }

    /* Send metrics if enabled and interval elapsed (atomic for thread safety) */
    if (bridge->config.enable_metrics_reporting) {
        static _Atomic uint64_t last_metrics_us = 0;
        uint64_t prev_metrics = atomic_load(&last_metrics_us);
        uint64_t metrics_elapsed = (now - prev_metrics) / 1000;
        if (metrics_elapsed >= bridge->config.metrics_interval_ms) {
            vae_bio_async_send_metrics(bridge);
            atomic_store(&last_metrics_us, now);
        }
    }

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

vae_bio_async_state_t vae_bio_async_get_state(const vae_bio_async_bridge_t* bridge)
{
    if (!bridge) return VAE_BIO_ASYNC_ERROR;
    return bridge->state;
}

int vae_bio_async_get_stats(const vae_bio_async_bridge_t* bridge,
                             vae_bio_async_stats_t* stats)
{
    if (!bridge || !stats) return NIMCP_ERROR_VAE_BIO_ASYNC_NULL;
    *stats = bridge->stats;
    return 0;
}

const char* vae_bio_message_type_to_string(vae_bio_message_type_t type)
{
    switch (type) {
        /* Core VAE */
        case BIO_MSG_VAE_ENCODE_REQUEST: return "encode_request";
        case BIO_MSG_VAE_ENCODE_RESPONSE: return "encode_response";
        case BIO_MSG_VAE_DECODE_REQUEST: return "decode_request";
        case BIO_MSG_VAE_DECODE_RESPONSE: return "decode_response";
        case BIO_MSG_VAE_SAMPLE_REQUEST: return "sample_request";
        case BIO_MSG_VAE_SAMPLE_RESPONSE: return "sample_response";
        case BIO_MSG_VAE_STATE_UPDATE: return "state_update";
        case BIO_MSG_VAE_TRAINING_UPDATE: return "training_update";

        /* VAE-FEP */
        case BIO_MSG_VAE_FEP_SYNC_REQUEST: return "fep_sync_request";
        case BIO_MSG_VAE_FEP_SYNC_RESPONSE: return "fep_sync_response";
        case BIO_MSG_VAE_FEP_FREE_ENERGY: return "fep_free_energy";
        case BIO_MSG_VAE_FEP_PREDICTION_ERROR: return "fep_prediction_error";
        case BIO_MSG_VAE_FEP_PRECISION_UPDATE: return "fep_precision_update";

        /* Health */
        case BIO_MSG_VAE_HEARTBEAT: return "heartbeat";
        case BIO_MSG_VAE_HEALTH_STATUS: return "health_status";
        case BIO_MSG_VAE_ANOMALY_DETECTED: return "anomaly_detected";
        case BIO_MSG_VAE_COLLAPSE_WARNING: return "collapse_warning";
        case BIO_MSG_VAE_METRICS_REPORT: return "metrics_report";

        default: return "unknown";
    }
}

/* ============================================================================
 * Default Message Handlers
 *
 * Note: These handlers provide stub implementations. Full tensor-based
 * processing requires integration with the VAE tensor API which is done
 * at the application level. These handlers acknowledge messages and
 * can be replaced via vae_bio_async_register_handler() for actual processing.
 * ============================================================================ */

nimcp_error_t vae_bio_async_handle_encode_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    vae_bio_async_bridge_t* bridge = (vae_bio_async_bridge_t*)user_data;

    if (!msg || msg_size < sizeof(vae_bio_msg_encode_request_t) || !bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vae_bio_async: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const vae_bio_msg_encode_request_t* request =
        (const vae_bio_msg_encode_request_t*)msg;

    /* Prepare response */
    vae_bio_msg_encode_response_t response;
    memset(&response, 0, sizeof(response));

    init_msg_header(&response.header, BIO_MSG_VAE_ENCODE_RESPONSE, bridge,
                    bridge->config.encode_priority,
                    bridge->config.encode_channel);

    /*
     * Note: Full encoding requires tensor conversion from float array.
     * This stub handler acknowledges the message. For actual processing,
     * register a custom handler via vae_bio_async_register_handler().
     */
    if (bridge->vae) {
        uint64_t start_us = get_timestamp_us();

        /* Stub: Copy input dimensions to response */
        response.latent_dim = (request->input_dim < VAE_BIO_ASYNC_MAX_LATENT_DIM) ?
                              request->input_dim / 4 : VAE_BIO_ASYNC_MAX_LATENT_DIM / 4;
        response.error_code = 0;  /* Success - message acknowledged */
        response.encoding_time_us = (float)(get_timestamp_us() - start_us);
    } else {
        response.error_code = NIMCP_ERROR_VAE_BIO_ASYNC_NOT_CONNECTED;
    }

    /* Response would be sent via promise in full implementation */
    (void)response_promise;

    bridge->stats.messages_received++;

    return NIMCP_SUCCESS;
}

nimcp_error_t vae_bio_async_handle_decode_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    vae_bio_async_bridge_t* bridge = (vae_bio_async_bridge_t*)user_data;

    if (!msg || msg_size < sizeof(vae_bio_msg_decode_request_t) || !bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vae_bio_async: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const vae_bio_msg_decode_request_t* request =
        (const vae_bio_msg_decode_request_t*)msg;

    /* Prepare response */
    vae_bio_msg_decode_response_t response;
    memset(&response, 0, sizeof(response));

    init_msg_header(&response.header, BIO_MSG_VAE_DECODE_RESPONSE, bridge,
                    bridge->config.decode_priority,
                    bridge->config.decode_channel);

    /*
     * Note: Full decoding requires tensor conversion from float array.
     * This stub handler acknowledges the message.
     */
    if (bridge->vae) {
        uint64_t start_us = get_timestamp_us();

        /* Stub: Set output dimensions based on latent */
        response.output_dim = request->latent_dim * 4;
        if (response.output_dim > VAE_BIO_ASYNC_MAX_INPUT_DIM) {
            response.output_dim = VAE_BIO_ASYNC_MAX_INPUT_DIM;
        }
        response.reconstruction_loss = 0.0f;
        response.error_code = 0;  /* Success - message acknowledged */
        response.decoding_time_us = (float)(get_timestamp_us() - start_us);
    } else {
        response.error_code = NIMCP_ERROR_VAE_BIO_ASYNC_NOT_CONNECTED;
    }

    (void)response_promise;

    bridge->stats.messages_received++;

    return NIMCP_SUCCESS;
}

nimcp_error_t vae_bio_async_handle_sample_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    vae_bio_async_bridge_t* bridge = (vae_bio_async_bridge_t*)user_data;

    if (!msg || msg_size < sizeof(vae_bio_msg_sample_request_t) || !bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vae_bio_async: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const vae_bio_msg_sample_request_t* request =
        (const vae_bio_msg_sample_request_t*)msg;

    /* Prepare response */
    vae_bio_msg_sample_response_t response;
    memset(&response, 0, sizeof(response));

    init_msg_header(&response.header, BIO_MSG_VAE_SAMPLE_RESPONSE, bridge,
                    bridge->config.encode_priority,
                    bridge->config.encode_channel);

    /*
     * Note: Full sampling requires tensor-based prior sampling.
     * This stub handler acknowledges the message.
     */
    if (bridge->vae) {
        uint64_t start_us = get_timestamp_us();

        /* Stub: Acknowledge sampling request */
        response.latent_dim = 32;  /* Default latent dim */
        response.num_samples = request->num_samples;
        response.error_code = 0;  /* Success - message acknowledged */
        response.sampling_time_us = (float)(get_timestamp_us() - start_us);
    } else {
        response.error_code = NIMCP_ERROR_VAE_BIO_ASYNC_NOT_CONNECTED;
    }

    (void)response_promise;

    bridge->stats.messages_received++;

    return NIMCP_SUCCESS;
}

nimcp_error_t vae_bio_async_handle_fep_sync_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    vae_bio_async_bridge_t* bridge = (vae_bio_async_bridge_t*)user_data;

    if (!msg || !bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vae_bio_async: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    (void)msg_size;
    (void)response_promise;

    /* FEP sync would coordinate VAE latent space with FEP beliefs */
    /* This is handled by the VAE-FEP bridge, so just acknowledge */

    if (bridge->config.enable_logging) {
        LOG_DEBUG("[VAE-BioAsync] Received FEP sync request");
    }

    return NIMCP_SUCCESS;
}
