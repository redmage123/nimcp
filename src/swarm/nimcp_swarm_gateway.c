/**
 * @file nimcp_swarm_gateway.c
 * @brief Server-to-Swarm Gateway Implementation
 */

#include "swarm/nimcp_swarm_gateway.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/thread/nimcp_thread.h"
#include "common/nimcp_internal.h"
#include "security/nimcp_bbb_helpers.h"
#include "api/nimcp_api_exception.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/thread/nimcp_atomic.h"
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_gateway)

/* Thread-safe BBB registration using atomic compare-exchange */
static nimcp_atomic_bool_t g_bbb_registered = {0};

/**
 * @brief Initialize BBB security for gateway (thread-safe)
 *
 * Uses atomic compare-exchange to ensure exactly one thread
 * performs the registration, preventing TOCTOU race conditions.
 */
static void gateway_init_bbb(void)
{
    bool expected = false;
    if (nimcp_atomic_compare_exchange_bool(&g_bbb_registered, &expected, true,
                                           NIMCP_MEMORY_ORDER_ACQ_REL)) {
        bbb_register_module("swarm_gateway", BBB_MODULE_TYPE_SWARM);
        bbb_audit_log(BBB_AUDIT_INFO, "swarm_gateway", "init", "Module registered with BBB");
    }
}

/* ========================================================================
 * Internal Structures
 * ======================================================================== */

/**
 * @brief Connected swarm information
 */
typedef struct {
    char swarm_id[32];               /**< Swarm identifier */
    char endpoint[NIMCP_LABEL_BUFFER_SIZE];              /**< Connection endpoint */
    swarm_status_t status;           /**< Current status */
    uint32_t last_contact_time;      /**< Last communication timestamp */
    uint32_t connect_time;           /**< Connection establishment time */

    /* Swarm metrics */
    uint32_t num_drones_total;       /**< Total drones in swarm */
    uint32_t num_drones_active;      /**< Currently active drones */
    uint32_t num_drones_failed;      /**< Failed drones */

    /* Communication statistics */
    uint64_t packets_sent;           /**< Total packets sent */
    uint64_t packets_received;       /**< Total packets received */
    uint64_t bytes_sent;             /**< Total bytes sent */
    uint64_t bytes_received;         /**< Total bytes received */
    float latency_ms;                /**< Average round-trip latency */

    /* Latest telemetry */
    swarm_telemetry_t latest_telemetry;
    bool telemetry_valid;            /**< Telemetry data validity flag */

    /* Sequence tracking */
    uint32_t next_seq_num;           /**< Next sequence number to send */
    uint32_t last_ack_seq;           /**< Last acknowledged sequence */

    /* P2P relay drone */
    uint32_t relay_drone_id;         /**< ID of drone used for P2P relay */

} swarm_connection_t;

/**
 * @brief Learning state tracker
 */
typedef struct {
    uint32_t last_update_id;         /**< Last sent update ID */
    uint32_t last_sync_time;         /**< Last synchronization timestamp */
    float* weight_snapshot;          /**< Snapshot of last synced weights */
    size_t weight_count;             /**< Number of weights */
    bool snapshot_valid;             /**< Snapshot validity flag */
} learning_state_t;

/**
 * @brief Gateway implementation
 */
struct swarm_gateway {
    /* Configuration */
    swarm_gateway_config_t config;

    /* Server brain */
    brain_t server_brain;

    /* Connected swarms */
    swarm_connection_t* swarms;
    uint32_t num_swarms;
    uint32_t max_swarms;

    /* Learning state */
    learning_state_t learning_state;

    /* Callbacks */
    telemetry_callback_t telemetry_callback;
    void* telemetry_user_data;
    swarm_event_callback_t event_callback;
    void* event_user_data;

    /* Statistics */
    uint64_t total_msgs_sent;
    uint64_t total_msgs_received;
    uint32_t total_drones;

    /* Timing */
    uint32_t last_broadcast_time;
    uint32_t last_process_time;

    /* Thread safety */
    nimcp_mutex_t mutex;
    bool mutex_initialized;
};

/* ========================================================================
 * Internal Helper Functions
 * ======================================================================== */

/**
 * @brief Get current timestamp in milliseconds
 */
static uint32_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/**
 * @brief Find swarm connection by ID
 */
static swarm_connection_t* find_swarm(swarm_gateway_t* gateway,
                                      const char* swarm_id) {
    if (!gateway || !swarm_id) {
        return NULL;  /* Defensive guard */
    }

    for (uint32_t i = 0; i < gateway->num_swarms; i++) {
        if (strcmp(gateway->swarms[i].swarm_id, swarm_id) == 0) {
            return &gateway->swarms[i];
        }
    }

    return NULL;  /* Not found - normal search miss */
}

/**
 * @brief Compute delta encoding for weight updates
 */
static int compute_weight_deltas(swarm_gateway_t* gateway,
                                learning_update_t* update) {
    if (!gateway || !update || !gateway->server_brain) {
        return -EINVAL;
    }

    /* Get current weight count from server brain */
    /* In a real implementation, this would extract actual weights */
    size_t weight_count = 10000; /* Placeholder */

    /* Allocate space for deltas */
    update->num_deltas = 0;
    update->delta_size = weight_count * sizeof(float);
    update->delta_data = nimcp_malloc(update->delta_size);

    if (!update->delta_data) {
        LOG_ERROR("Failed to allocate delta data");
        return -ENOMEM;
    }

    float* deltas = (float*)update->delta_data;
    uint32_t significant_deltas = 0;

    /* Compute deltas against last snapshot */
    if (gateway->learning_state.snapshot_valid &&
        gateway->learning_state.weight_count == weight_count) {

        /* In production, get actual weights from brain */
        for (size_t i = 0; i < weight_count; i++) {
            float current = 0.0F; /* Get from brain */
            float previous = gateway->learning_state.weight_snapshot[i];
            float delta = current - previous;

            /* Only include significant deltas (simple compression) */
            if (fabsf(delta) > 1e-6F) {
                deltas[significant_deltas++] = delta;
            }
        }

        update->num_deltas = significant_deltas;
        update->compression_ratio = (float)weight_count / (float)significant_deltas;

        LOG_DEBUG("Computed %u deltas from %zu weights (%.2fx compression)",
                       significant_deltas, weight_count,
                       update->compression_ratio);
    } else {
        /* First sync - send all weights */
        update->num_deltas = weight_count;
        update->compression_ratio = 1.0F;

        LOG_INFO("First learning sync - sending full weights");
    }

    /* Update snapshot */
    if (!gateway->learning_state.weight_snapshot ||
        gateway->learning_state.weight_count != weight_count) {

        nimcp_free(gateway->learning_state.weight_snapshot);
        gateway->learning_state.weight_snapshot =
            nimcp_malloc(weight_count * sizeof(float));

        if (!gateway->learning_state.weight_snapshot) {
            nimcp_free(update->delta_data);
            return -ENOMEM;
        }

        gateway->learning_state.weight_count = weight_count;
    }

    /* Update snapshot with current weights */
    /* In production: memcpy current weights */
    gateway->learning_state.snapshot_valid = true;

    return 0;
}

/**
 * @brief Send message via P2P relay
 *
 * Sends to one drone in swarm, which propagates internally
 */
static int send_via_p2p_relay(swarm_gateway_t* gateway,
                              swarm_connection_t* swarm,
                              const gateway_message_t* message) {
    if (!gateway || !swarm || !message) {
        return -EINVAL;
    }

    /* In production, this would use actual P2P protocol */
    /* For now, simulate successful transmission */

    swarm->packets_sent++;
    swarm->bytes_sent += message->payload_size;
    swarm->last_contact_time = get_timestamp_ms();
    swarm->next_seq_num++;

    LOG_DEBUG("Sent %s to swarm '%s' via P2P relay (seq=%u, size=%zu)",
                   swarm_gateway_msg_type_to_string(message->type),
                   swarm->swarm_id, message->sequence_num,
                   message->payload_size);

    return 0;
}

/**
 * @brief Update swarm telemetry from received data
 */
static void update_telemetry(swarm_connection_t* swarm,
                            const swarm_telemetry_t* telemetry) {
    if (!swarm || !telemetry) {
        return;
    }

    memcpy(&swarm->latest_telemetry, telemetry, sizeof(swarm_telemetry_t));
    swarm->telemetry_valid = true;
    swarm->last_contact_time = get_timestamp_ms();

    /* Update swarm metrics */
    swarm->num_drones_total = telemetry->num_drones;
    swarm->num_drones_active = telemetry->num_responsive;

    LOG_DEBUG("Updated telemetry for swarm '%s': %u/%u drones active, "
                   "battery=%.1f%%, coherence=%.2f",
                   swarm->swarm_id, swarm->num_drones_active,
                   swarm->num_drones_total, telemetry->avg_battery_level,
                   telemetry->formation_coherence);
}

/**
 * @brief Check for swarm timeouts
 */
static void check_timeouts(swarm_gateway_t* gateway) {
    if (!gateway) {
        return;
    }

    uint32_t current_time = get_timestamp_ms();

    for (uint32_t i = 0; i < gateway->num_swarms; i++) {
        swarm_connection_t* swarm = &gateway->swarms[i];

        if (swarm->status != SWARM_STATUS_CONNECTED) {
            continue;
        }

        uint32_t time_since_contact = current_time - swarm->last_contact_time;

        if (time_since_contact > gateway->config.timeout_ms) {
            LOG_WARNING("Swarm '%s' timeout (no contact for %u ms)",
                          swarm->swarm_id, time_since_contact);

            swarm->status = SWARM_STATUS_TIMEOUT;

            /* Trigger event callback */
            if (gateway->event_callback) {
                gateway->event_callback(swarm->swarm_id, "timeout",
                                      &time_since_contact,
                                      gateway->event_user_data);
            }
        } else if (time_since_contact > gateway->config.timeout_ms / 2) {
            /* Degraded performance */
            if (swarm->status == SWARM_STATUS_CONNECTED) {
                swarm->status = SWARM_STATUS_DEGRADED;

                LOG_DEBUG("Swarm '%s' degraded connection",
                              swarm->swarm_id);
            }
        }
    }
}

/**
 * @brief Send periodic heartbeats
 */
static void send_heartbeats(swarm_gateway_t* gateway) {
    if (!gateway) {
        return;
    }

    uint32_t current_time = get_timestamp_ms();
    uint32_t heartbeat_interval = gateway->config.broadcast_interval_ms * 2;

    if (current_time - gateway->last_broadcast_time < heartbeat_interval) {
        return;
    }

    for (uint32_t i = 0; i < gateway->num_swarms; i++) {
        swarm_connection_t* swarm = &gateway->swarms[i];

        if (swarm->status != SWARM_STATUS_CONNECTED &&
            swarm->status != SWARM_STATUS_DEGRADED) {
            continue;
        }

        /* Create heartbeat message */
        gateway_message_t heartbeat = {
            .type = GATEWAY_MSG_HEARTBEAT,
            .timestamp = current_time,
            .sequence_num = swarm->next_seq_num,
            .requires_ack = false,
            .payload = NULL,
            .payload_size = 0
        };

        strncpy(heartbeat.target_swarm, swarm->swarm_id, 31);
        heartbeat.target_swarm[31] = '\0';

        send_via_p2p_relay(gateway, swarm, &heartbeat);
    }

    gateway->last_broadcast_time = current_time;
}

/**
 * @brief Simulate receiving telemetry (in production, from network)
 */
static void simulate_telemetry_reception(swarm_gateway_t* gateway) {
    if (!gateway || !gateway->config.enable_telemetry) {
        return;
    }

    /* In production, this would receive actual telemetry from network */
    /* For now, simulate periodic telemetry updates */

    uint32_t current_time = get_timestamp_ms();

    for (uint32_t i = 0; i < gateway->num_swarms; i++) {
        swarm_connection_t* swarm = &gateway->swarms[i];

        if (swarm->status != SWARM_STATUS_CONNECTED &&
            swarm->status != SWARM_STATUS_DEGRADED) {
            continue;
        }

        /* Simulate telemetry every 500ms */
        if (current_time - swarm->last_contact_time > 500) {
            swarm_telemetry_t telemetry = {0};
            strncpy(telemetry.swarm_id, swarm->swarm_id, 31);
            telemetry.timestamp = current_time;
            telemetry.num_drones = swarm->num_drones_total;
            telemetry.num_responsive = swarm->num_drones_active;
            telemetry.avg_battery_level = 75.0F + (nimcp_tl_rand() % 20);
            telemetry.avg_cpu_usage = 40.0F + (nimcp_tl_rand() % 30);
            telemetry.avg_memory_usage = 50.0F + (nimcp_tl_rand() % 20);
            telemetry.formation_coherence = 0.8F + (nimcp_tl_rand() % 20) / 100.0F;
            telemetry.mission_progress = 0.5F;
            telemetry.communication_health = 85 + (nimcp_tl_rand() % 15);

            update_telemetry(swarm, &telemetry);
            swarm->packets_received++;

            /* Trigger callback */
            if (gateway->telemetry_callback) {
                gateway->telemetry_callback(swarm->swarm_id, &telemetry,
                                          gateway->telemetry_user_data);
            }
        }
    }
}

/* Note: BIO-async support can be added later by integrating with nimcp_bio_router */

/* ========================================================================
 * Core Gateway Functions
 * ======================================================================== */

swarm_gateway_t* swarm_gateway_create(brain_t server_brain,
                                      const swarm_gateway_config_t* config) {
    gateway_init_bbb();

    if (!bbb_check_pointer(server_brain, "swarm_gateway_create")) {
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_gateway", "create_error", "Invalid server_brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_gateway_create: server_brain is NULL");
        return NULL;
    }

    if (!bbb_check_pointer(config, "swarm_gateway_create")) {
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_gateway", "create_error", "Invalid config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_gateway_create: config is NULL");
        return NULL;
    }

    if (config->max_swarms == 0 || config->max_swarms > 1000) {
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_gateway", "create_error",
                     "Invalid max_swarms: %u", config->max_swarms);
        LOG_ERROR("Invalid max_swarms: %u", config->max_swarms);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_gateway_create: max_swarms out of range");
        return NULL;
    }

    LOG_INFO("Creating swarm gateway '%s' (max_swarms=%u, "
                  "broadcast_interval=%ums, timeout=%ums)",
                  config->gateway_name, config->max_swarms,
                  config->broadcast_interval_ms, config->timeout_ms);

    swarm_gateway_t* gateway = nimcp_calloc(1, sizeof(swarm_gateway_t));
    if (!gateway) {
        LOG_ERROR("Failed to allocate gateway");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_gateway_create: alloc failed");

        return NULL;
    }

    /* Copy configuration */
    memcpy(&gateway->config, config, sizeof(swarm_gateway_config_t));

    /* Store server brain */
    gateway->server_brain = server_brain;

    /* Allocate swarm array */
    gateway->max_swarms = config->max_swarms;
    gateway->swarms = nimcp_calloc(config->max_swarms,
                                   sizeof(swarm_connection_t));
    if (!gateway->swarms) {
        LOG_ERROR("Failed to allocate swarm array");
        nimcp_free(gateway);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "simulate_telemetry_reception: gateway->swarms is NULL");
        return NULL;
    }

    /* Initialize learning state */
    gateway->learning_state.last_update_id = 0;
    gateway->learning_state.last_sync_time = 0;
    gateway->learning_state.weight_snapshot = NULL;
    gateway->learning_state.weight_count = 0;
    gateway->learning_state.snapshot_valid = false;

    /* Initialize mutex */
    if (nimcp_mutex_init(&gateway->mutex, NULL) != NIMCP_SUCCESS) {
        LOG_ERROR("Failed to initialize gateway mutex");
        nimcp_free(gateway->swarms);
        nimcp_free(gateway);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "simulate_telemetry_reception: validation failed");
        return NULL;
    }
    gateway->mutex_initialized = true;

    /* Initialize timing */
    gateway->last_broadcast_time = get_timestamp_ms();
    gateway->last_process_time = get_timestamp_ms();

    bbb_audit_log(BBB_AUDIT_INFO, "swarm_gateway", "created",
                 "Gateway '%s' created: max_swarms=%u, broadcast_interval=%ums",
                 config->gateway_name, config->max_swarms, config->broadcast_interval_ms);
    LOG_INFO("Swarm gateway '%s' created successfully",
                  config->gateway_name);

    return gateway;
}

void swarm_gateway_destroy(swarm_gateway_t* gateway) {
    if (!gateway) {
        return;
    }

    LOG_INFO("Destroying swarm gateway '%s'", gateway->config.gateway_name);

    /* Lock for cleanup */
    if (gateway->mutex_initialized) {
        nimcp_mutex_lock(&gateway->mutex);
    }

    /* Disconnect all swarms */
    for (uint32_t i = 0; i < gateway->num_swarms; i++) {
        swarm_connection_t* swarm = &gateway->swarms[i];

        LOG_DEBUG("Disconnecting swarm '%s'", swarm->swarm_id);

        /* Send disconnect notification in production */
        swarm->status = SWARM_STATUS_DISCONNECTED;
    }

    /* Free learning state */
    nimcp_free(gateway->learning_state.weight_snapshot);

    /* Free swarm array */
    nimcp_free(gateway->swarms);

    /* Unlock and destroy mutex */
    if (gateway->mutex_initialized) {
        nimcp_mutex_unlock(&gateway->mutex);
        nimcp_mutex_destroy(&gateway->mutex);
    }

    /* Free gateway */
    nimcp_free(gateway);

    LOG_INFO("Swarm gateway destroyed");
}

int swarm_gateway_connect_swarm(swarm_gateway_t* gateway,
                                const char* swarm_id,
                                const char* endpoint) {
    if (!bbb_check_pointer(gateway, "swarm_gateway_connect_swarm")) {
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_gateway", "connect_error", "Invalid gateway");
        return -EINVAL;
    }

    if (!bbb_check_string(swarm_id, 32, "swarm_gateway_connect_swarm")) {
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_gateway", "connect_error", "Invalid swarm_id");
        return -EINVAL;
    }

    if (!bbb_check_string(endpoint, 128, "swarm_gateway_connect_swarm")) {
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_gateway", "connect_error", "Invalid endpoint");
        return -EINVAL;
    }

    nimcp_mutex_lock(&gateway->mutex);

    /* Check if already at capacity */
    if (gateway->num_swarms >= gateway->max_swarms) {
        LOG_ERROR("Gateway at max capacity (%u swarms)",
                       gateway->max_swarms);
        nimcp_mutex_unlock(&gateway->mutex);
        return -ENOSPC;
    }

    /* Check if swarm already connected */
    if (find_swarm(gateway, swarm_id) != NULL) {
        LOG_WARNING("Swarm '%s' already connected", swarm_id);
        nimcp_mutex_unlock(&gateway->mutex);
        return -EEXIST;
    }

    LOG_INFO("Connecting to swarm '%s' at %s", swarm_id, endpoint);

    /* Add new swarm */
    swarm_connection_t* swarm = &gateway->swarms[gateway->num_swarms];

    strncpy(swarm->swarm_id, swarm_id, 31);
    swarm->swarm_id[31] = '\0';
    strncpy(swarm->endpoint, endpoint, 127);
    swarm->endpoint[127] = '\0';

    swarm->status = SWARM_STATUS_CONNECTING;
    swarm->connect_time = get_timestamp_ms();
    swarm->last_contact_time = swarm->connect_time;
    swarm->next_seq_num = 1;
    swarm->last_ack_seq = 0;
    swarm->telemetry_valid = false;

    /* In production: establish actual network connection */
    /* For now, immediately mark as connected */
    swarm->status = SWARM_STATUS_CONNECTED;

    /* Simulate initial swarm configuration */
    swarm->num_drones_total = 10; /* Would be discovered */
    swarm->num_drones_active = 10;
    swarm->num_drones_failed = 0;
    swarm->relay_drone_id = 1; /* First drone as relay */

    gateway->num_swarms++;
    gateway->total_drones += swarm->num_drones_total;

    bbb_audit_log(BBB_AUDIT_INFO, "swarm_gateway", "connected",
                 "Connected to swarm '%s' at %s (%u drones)",
                 swarm_id, endpoint, swarm->num_drones_total);
    LOG_INFO("Successfully connected to swarm '%s' (%u drones)",
                  swarm_id, swarm->num_drones_total);

    /* Trigger event callback */
    if (gateway->event_callback) {
        gateway->event_callback(swarm_id, "connected", swarm,
                              gateway->event_user_data);
    }

    nimcp_mutex_unlock(&gateway->mutex);

    return 0;
}

int swarm_gateway_disconnect_swarm(swarm_gateway_t* gateway,
                                   const char* swarm_id) {
    NIMCP_CHECK_NULL(gateway, -EINVAL);
    NIMCP_CHECK_NULL(swarm_id, -EINVAL);

    nimcp_mutex_lock(&gateway->mutex);

    swarm_connection_t* swarm = find_swarm(gateway, swarm_id);
    if (!swarm) {
        LOG_WARNING("Swarm '%s' not found", swarm_id);
        nimcp_mutex_unlock(&gateway->mutex);
        return -ENOENT;
    }

    LOG_INFO("Disconnecting from swarm '%s'", swarm_id);

    /* Send disconnect message in production */

    /* Update statistics */
    gateway->total_drones -= swarm->num_drones_total;

    /* Mark as disconnected */
    swarm->status = SWARM_STATUS_DISCONNECTED;

    /* Trigger event callback */
    if (gateway->event_callback) {
        gateway->event_callback(swarm_id, "disconnected", swarm,
                              gateway->event_user_data);
    }

    /* Remove from array by shifting */
    uint32_t index = swarm - gateway->swarms;
    if (index < gateway->num_swarms - 1) {
        memmove(&gateway->swarms[index],
                &gateway->swarms[index + 1],
                (gateway->num_swarms - index - 1) * sizeof(swarm_connection_t));
    }

    gateway->num_swarms--;

    LOG_INFO("Swarm '%s' disconnected", swarm_id);

    nimcp_mutex_unlock(&gateway->mutex);

    return 0;
}

/* ========================================================================
 * Message Transmission Functions
 * ======================================================================== */

int swarm_gateway_broadcast_update(swarm_gateway_t* gateway,
                                   const gateway_message_t* message) {
    NIMCP_CHECK_NULL(gateway, -EINVAL);
    NIMCP_CHECK_NULL(message, -EINVAL);

    nimcp_mutex_lock(&gateway->mutex);

    int swarms_reached = 0;

    LOG_DEBUG("Broadcasting %s to all swarms",
                   swarm_gateway_msg_type_to_string(message->type));

    for (uint32_t i = 0; i < gateway->num_swarms; i++) {
        swarm_connection_t* swarm = &gateway->swarms[i];

        if (swarm->status != SWARM_STATUS_CONNECTED &&
            swarm->status != SWARM_STATUS_DEGRADED) {
            continue;
        }

        /* Create message copy with swarm-specific sequence */
        gateway_message_t swarm_msg = *message;
        swarm_msg.sequence_num = swarm->next_seq_num;
        strncpy(swarm_msg.target_swarm, swarm->swarm_id, 31);
        swarm_msg.target_swarm[31] = '\0';

        if (send_via_p2p_relay(gateway, swarm, &swarm_msg) == 0) {
            swarms_reached++;
        }
    }

    gateway->total_msgs_sent += swarms_reached;

    LOG_INFO("Broadcast reached %d swarms", swarms_reached);

    nimcp_mutex_unlock(&gateway->mutex);

    return swarms_reached;
}

int swarm_gateway_send_to_swarm(swarm_gateway_t* gateway,
                                const char* swarm_id,
                                const gateway_message_t* message) {
    NIMCP_CHECK_NULL(gateway, -EINVAL);
    NIMCP_CHECK_NULL(swarm_id, -EINVAL);
    NIMCP_CHECK_NULL(message, -EINVAL);

    nimcp_mutex_lock(&gateway->mutex);

    swarm_connection_t* swarm = find_swarm(gateway, swarm_id);
    if (!swarm) {
        LOG_ERROR("Swarm '%s' not found", swarm_id);
        nimcp_mutex_unlock(&gateway->mutex);
        return -ENOENT;
    }

    if (swarm->status != SWARM_STATUS_CONNECTED &&
        swarm->status != SWARM_STATUS_DEGRADED) {
        LOG_ERROR("Swarm '%s' not in connected state", swarm_id);
        nimcp_mutex_unlock(&gateway->mutex);
        return -ENOTCONN;
    }

    /* Create message with proper sequence */
    gateway_message_t swarm_msg = *message;
    swarm_msg.sequence_num = swarm->next_seq_num;
    strncpy(swarm_msg.target_swarm, swarm_id, 31);
    swarm_msg.target_swarm[31] = '\0';

    int result = send_via_p2p_relay(gateway, swarm, &swarm_msg);

    if (result == 0) {
        gateway->total_msgs_sent++;
    }

    nimcp_mutex_unlock(&gateway->mutex);

    return result;
}

int swarm_gateway_send_mission(swarm_gateway_t* gateway,
                               const char* swarm_id,
                               const mission_params_t* mission) {
    if (!bbb_check_pointer(gateway, "swarm_gateway_send_mission")) {
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_gateway", "send_mission_error", "Invalid gateway");
        return -EINVAL;
    }

    if (!bbb_check_string(swarm_id, 32, "swarm_gateway_send_mission")) {
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_gateway", "send_mission_error", "Invalid swarm_id");
        return -EINVAL;
    }

    if (!bbb_check_pointer(mission, "swarm_gateway_send_mission")) {
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_gateway", "send_mission_error", "Invalid mission");
        return -EINVAL;
    }

    if (!gateway->config.enable_mission_control) {
        bbb_audit_log(BBB_AUDIT_ERROR, "swarm_gateway", "send_mission_error", "Mission control not enabled");
        LOG_ERROR("Mission control not enabled");
        return -EPERM;
    }

    bbb_audit_log(BBB_AUDIT_INFO, "swarm_gateway", "send_mission",
                 "Sending mission '%s' to swarm '%s'", mission->mission_id, swarm_id);
    LOG_INFO("Sending mission '%s' to swarm '%s'",
                  mission->mission_id, swarm_id);

    gateway_message_t message = {
        .type = GATEWAY_MSG_MISSION_PARAMS,
        .timestamp = get_timestamp_ms(),
        .requires_ack = true,
        .payload = (void*)mission,
        .payload_size = sizeof(mission_params_t) + mission->objective_size
    };

    return swarm_gateway_send_to_swarm(gateway, swarm_id, &message);
}

int swarm_gateway_send_learning_update(swarm_gateway_t* gateway,
                                       const char* swarm_id,
                                       const learning_update_t* update) {
    NIMCP_CHECK_NULL(gateway, -EINVAL);
    NIMCP_CHECK_NULL(update, -EINVAL);

    if (!gateway->config.enable_learning_sync) {
        LOG_ERROR("Learning sync not enabled");
        return -EPERM;
    }

    LOG_INFO("Sending learning update #%u (%u deltas, %.2fx compression)",
                  update->update_id, update->num_deltas,
                  update->compression_ratio);

    gateway_message_t message = {
        .type = GATEWAY_MSG_LEARNING_UPDATE,
        .timestamp = get_timestamp_ms(),
        .requires_ack = false,
        .payload = (void*)update,
        .payload_size = sizeof(learning_update_t) + update->delta_size
    };

    if (swarm_id) {
        /* Send to specific swarm */
        return swarm_gateway_send_to_swarm(gateway, swarm_id, &message);
    } else {
        /* Broadcast to all swarms */
        return swarm_gateway_broadcast_update(gateway, &message);
    }
}

int swarm_gateway_send_threat_intel(swarm_gateway_t* gateway,
                                    const char* swarm_id,
                                    const threat_intel_t* threat) {
    if (!bbb_check_pointer(gateway, "swarm_gateway_send_threat_intel")) {
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_gateway", "send_threat_error", "Invalid gateway");
        return -EINVAL;
    }

    if (!bbb_check_pointer(threat, "swarm_gateway_send_threat_intel")) {
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_gateway", "send_threat_error", "Invalid threat");
        return -EINVAL;
    }

    bbb_audit_log(BBB_AUDIT_CRITICAL, "swarm_gateway", "send_threat",
                 "Sending threat intel #%u (level=%u, type=%s) to %s",
                 threat->threat_id, threat->threat_level,
                 threat->threat_type, swarm_id ? swarm_id : "ALL");
    LOG_INFO("Sending threat intel #%u (level=%u, type=%s)",
                  threat->threat_id, threat->threat_level,
                  threat->threat_type);

    gateway_message_t message = {
        .type = GATEWAY_MSG_THREAT_INTEL,
        .timestamp = get_timestamp_ms(),
        .requires_ack = true,
        .payload = (void*)threat,
        .payload_size = sizeof(threat_intel_t)
    };

    if (swarm_id) {
        return swarm_gateway_send_to_swarm(gateway, swarm_id, &message);
    } else {
        return swarm_gateway_broadcast_update(gateway, &message);
    }
}

int swarm_gateway_send_formation_cmd(swarm_gateway_t* gateway,
                                     const char* swarm_id,
                                     const formation_cmd_t* formation) {
    NIMCP_CHECK_NULL(gateway, -EINVAL);
    NIMCP_CHECK_NULL(swarm_id, -EINVAL);
    NIMCP_CHECK_NULL(formation, -EINVAL);

    LOG_INFO("Sending formation command (type=%u) to swarm '%s'",
                  formation->formation_type, swarm_id);

    gateway_message_t message = {
        .type = GATEWAY_MSG_FORMATION_CMD,
        .timestamp = get_timestamp_ms(),
        .requires_ack = true,
        .payload = (void*)formation,
        .payload_size = sizeof(formation_cmd_t)
    };

    return swarm_gateway_send_to_swarm(gateway, swarm_id, &message);
}

int swarm_gateway_send_recall(swarm_gateway_t* gateway,
                              const char* swarm_id,
                              bool emergency) {
    NIMCP_CHECK_NULL(gateway, -EINVAL);
    NIMCP_CHECK_NULL(swarm_id, -EINVAL);

    LOG_WARNING("Sending %srecall command to swarm '%s'",
                  emergency ? "EMERGENCY " : "", swarm_id);

    uint32_t recall_flag = emergency ? 1 : 0;

    gateway_message_t message = {
        .type = GATEWAY_MSG_RECALL,
        .timestamp = get_timestamp_ms(),
        .requires_ack = true,
        .payload = &recall_flag,
        .payload_size = sizeof(uint32_t)
    };

    return swarm_gateway_send_to_swarm(gateway, swarm_id, &message);
}

int swarm_gateway_send_neuromod_override(swarm_gateway_t* gateway,
                                         const char* swarm_id,
                                         const neuromod_override_t* override) {
    NIMCP_CHECK_NULL(gateway, -EINVAL);
    NIMCP_CHECK_NULL(swarm_id, -EINVAL);
    NIMCP_CHECK_NULL(override, -EINVAL);

    LOG_INFO("Sending neuromodulator override (type=%u, value=%.2f) "
                  "to swarm '%s'", override->modulator_type,
                  override->override_value, swarm_id);

    gateway_message_t message = {
        .type = GATEWAY_MSG_NEUROMOD_OVERRIDE,
        .timestamp = get_timestamp_ms(),
        .requires_ack = true,
        .payload = (void*)override,
        .payload_size = sizeof(neuromod_override_t)
    };

    return swarm_gateway_send_to_swarm(gateway, swarm_id, &message);
}

/* ========================================================================
 * Telemetry and Status Functions
 * ======================================================================== */

int swarm_gateway_receive_telemetry(swarm_gateway_t* gateway,
                                    const char* swarm_id,
                                    swarm_telemetry_t* telemetry) {
    NIMCP_CHECK_NULL(gateway, -EINVAL);
    NIMCP_CHECK_NULL(swarm_id, -EINVAL);
    NIMCP_CHECK_NULL(telemetry, -EINVAL);

    nimcp_mutex_lock(&gateway->mutex);

    swarm_connection_t* swarm = find_swarm(gateway, swarm_id);
    if (!swarm) {
        nimcp_mutex_unlock(&gateway->mutex);
        return -ENOENT;
    }

    if (!swarm->telemetry_valid) {
        nimcp_mutex_unlock(&gateway->mutex);
        return -EAGAIN;
    }

    memcpy(telemetry, &swarm->latest_telemetry, sizeof(swarm_telemetry_t));

    nimcp_mutex_unlock(&gateway->mutex);

    return 0;
}

int swarm_gateway_get_swarm_status(swarm_gateway_t* gateway,
                                   const char* swarm_id,
                                   swarm_health_t* health) {
    NIMCP_CHECK_NULL(gateway, -EINVAL);
    NIMCP_CHECK_NULL(swarm_id, -EINVAL);
    NIMCP_CHECK_NULL(health, -EINVAL);

    nimcp_mutex_lock(&gateway->mutex);

    swarm_connection_t* swarm = find_swarm(gateway, swarm_id);
    if (!swarm) {
        nimcp_mutex_unlock(&gateway->mutex);
        return -ENOENT;
    }

    /* Populate health structure */
    strncpy(health->swarm_id, swarm->swarm_id, 31);
    health->swarm_id[31] = '\0';
    health->status = swarm->status;
    health->last_contact_ms = get_timestamp_ms() - swarm->last_contact_time;
    health->num_drones_total = swarm->num_drones_total;
    health->num_drones_active = swarm->num_drones_active;
    health->num_drones_failed = swarm->num_drones_failed;
    health->packets_sent = swarm->packets_sent;
    health->packets_received = swarm->packets_received;
    health->latency_ms = swarm->latency_ms;

    /* Compute overall health score */
    float drone_health = (float)swarm->num_drones_active /
                        (float)swarm->num_drones_total;
    float connection_health = (swarm->status == SWARM_STATUS_CONNECTED) ? 1.0F :
                             (swarm->status == SWARM_STATUS_DEGRADED) ? 0.7F : 0.0F;

    health->overall_health = (drone_health + connection_health) / 2.0F;

    nimcp_mutex_unlock(&gateway->mutex);

    return 0;
}

int swarm_gateway_get_connected_swarms(swarm_gateway_t* gateway,
                                       char swarm_ids[][32],
                                       uint32_t max_swarms) {
    NIMCP_CHECK_NULL(gateway, -EINVAL);
    NIMCP_CHECK_NULL(swarm_ids, -EINVAL);

    nimcp_mutex_lock(&gateway->mutex);

    uint32_t count = 0;
    for (uint32_t i = 0; i < gateway->num_swarms && count < max_swarms; i++) {
        if (gateway->swarms[i].status == SWARM_STATUS_CONNECTED ||
            gateway->swarms[i].status == SWARM_STATUS_DEGRADED) {
            strncpy(swarm_ids[count], gateway->swarms[i].swarm_id, 31);
            swarm_ids[count][31] = '\0';
            count++;
        }
    }

    nimcp_mutex_unlock(&gateway->mutex);

    return count;
}

int swarm_gateway_register_telemetry_callback(swarm_gateway_t* gateway,
                                              telemetry_callback_t callback,
                                              void* user_data) {
    NIMCP_CHECK_NULL(gateway, -EINVAL);

    nimcp_mutex_lock(&gateway->mutex);

    gateway->telemetry_callback = callback;
    gateway->telemetry_user_data = user_data;

    LOG_DEBUG("Telemetry callback registered");

    nimcp_mutex_unlock(&gateway->mutex);

    return 0;
}

int swarm_gateway_register_event_callback(swarm_gateway_t* gateway,
                                          swarm_event_callback_t callback,
                                          void* user_data) {
    NIMCP_CHECK_NULL(gateway, -EINVAL);

    nimcp_mutex_lock(&gateway->mutex);

    gateway->event_callback = callback;
    gateway->event_user_data = user_data;

    LOG_DEBUG("Event callback registered");

    nimcp_mutex_unlock(&gateway->mutex);

    return 0;
}

/* ========================================================================
 * Processing and Maintenance Functions
 * ======================================================================== */

int swarm_gateway_process(swarm_gateway_t* gateway, uint32_t timeout_ms) {
    NIMCP_CHECK_NULL(gateway, -EINVAL);

    nimcp_mutex_lock(&gateway->mutex);

    int events_processed = 0;
    uint32_t start_time = get_timestamp_ms();

    /* Check for swarm timeouts */
    check_timeouts(gateway);
    events_processed++;

    /* Send periodic heartbeats */
    send_heartbeats(gateway);
    events_processed++;

    /* Receive and process telemetry */
    simulate_telemetry_reception(gateway);
    events_processed++;

    /* Periodic learning sync */
    if (gateway->config.enable_learning_sync) {
        uint32_t current_time = get_timestamp_ms();
        uint32_t sync_interval = gateway->config.broadcast_interval_ms * 10;

        if (current_time - gateway->learning_state.last_sync_time > sync_interval) {
            /* Trigger automatic learning sync */
            LOG_DEBUG("Automatic learning sync triggered");
            /* This would call swarm_gateway_sync_learning internally */
            gateway->learning_state.last_sync_time = current_time;
            events_processed++;
        }
    }

    /* Update process time */
    gateway->last_process_time = get_timestamp_ms();

    nimcp_mutex_unlock(&gateway->mutex);

    /* If timeout specified, sleep for remaining time */
    if (timeout_ms > 0) {
        uint32_t elapsed = get_timestamp_ms() - start_time;
        if (elapsed < timeout_ms) {
            /* In production, wait for network events */
            /* For now, simple sleep */
        }
    }

    return events_processed;
}

int swarm_gateway_sync_learning(swarm_gateway_t* gateway) {
    NIMCP_CHECK_NULL(gateway, -EINVAL);

    if (!gateway->config.enable_learning_sync) {
        LOG_ERROR("Learning sync not enabled");
        return -EPERM;
    }

    LOG_INFO("Synchronizing learning updates to swarms");

    nimcp_mutex_lock(&gateway->mutex);

    /* Create learning update */
    learning_update_t update = {0};
    update.update_id = ++gateway->learning_state.last_update_id;
    update.base_timestamp = get_timestamp_ms();

    int result = compute_weight_deltas(gateway, &update);
    if (result < 0) {
        LOG_ERROR("Failed to compute weight deltas: %d", result);
        nimcp_mutex_unlock(&gateway->mutex);
        return result;
    }

    /* Broadcast to all swarms */
    int swarms_synced = swarm_gateway_send_learning_update(gateway, NULL, &update);

    /* Free update data */
    nimcp_free(update.delta_data);

    gateway->learning_state.last_sync_time = get_timestamp_ms();

    LOG_INFO("Learning sync complete: %d swarms updated", swarms_synced);

    nimcp_mutex_unlock(&gateway->mutex);

    return swarms_synced;
}

int swarm_gateway_aggregate_to_server(swarm_gateway_t* gateway) {
    NIMCP_CHECK_NULL(gateway, -EINVAL);

    if (!gateway->config.enable_telemetry) {
        return -EPERM;
    }

    LOG_DEBUG("Aggregating swarm data to server brain");

    nimcp_mutex_lock(&gateway->mutex);

    /* Aggregate telemetry from all swarms */
    float total_battery = 0.0F;
    float total_coherence = 0.0F;
    float total_progress = 0.0F;
    uint32_t valid_swarms = 0;

    for (uint32_t i = 0; i < gateway->num_swarms; i++) {
        swarm_connection_t* swarm = &gateway->swarms[i];

        if (!swarm->telemetry_valid) {
            continue;
        }

        total_battery += swarm->latest_telemetry.avg_battery_level;
        total_coherence += swarm->latest_telemetry.formation_coherence;
        total_progress += swarm->latest_telemetry.mission_progress;
        valid_swarms++;
    }

    if (valid_swarms > 0) {
        float avg_battery = total_battery / valid_swarms;
        float avg_coherence = total_coherence / valid_swarms;
        float avg_progress = total_progress / valid_swarms;

        LOG_INFO("Aggregated metrics: battery=%.1f%%, coherence=%.2f, "
                      "progress=%.1f%% (from %u swarms)",
                      avg_battery, avg_coherence, avg_progress * 100.0F,
                      valid_swarms);

        /* Feed back to server brain for macro decisions */
        /* In production, this would update brain state or trigger responses */
    }

    nimcp_mutex_unlock(&gateway->mutex);

    return valid_swarms;
}

/* ========================================================================
 * Utility Functions
 * ======================================================================== */

int swarm_gateway_create_learning_update(swarm_gateway_t* gateway,
                                         learning_update_t* update) {
    NIMCP_CHECK_NULL(gateway, -EINVAL);
    NIMCP_CHECK_NULL(update, -EINVAL);

    memset(update, 0, sizeof(learning_update_t));

    update->update_id = gateway->learning_state.last_update_id + 1;
    update->base_timestamp = get_timestamp_ms();

    return compute_weight_deltas(gateway, update);
}

void swarm_gateway_free_learning_update(learning_update_t* update) {
    if (!update) {
        return;
    }

    nimcp_free(update->delta_data);
    update->delta_data = NULL;
    update->delta_size = 0;
    update->num_deltas = 0;
}

int swarm_gateway_get_stats(swarm_gateway_t* gateway,
                            uint32_t* num_swarms_out,
                            uint32_t* total_drones_out,
                            uint64_t* msgs_sent_out,
                            uint64_t* msgs_received_out) {
    NIMCP_CHECK_NULL(gateway, -EINVAL);

    nimcp_mutex_lock(&gateway->mutex);

    if (num_swarms_out) {
        *num_swarms_out = gateway->num_swarms;
    }

    if (total_drones_out) {
        *total_drones_out = gateway->total_drones;
    }

    if (msgs_sent_out) {
        *msgs_sent_out = gateway->total_msgs_sent;
    }

    if (msgs_received_out) {
        *msgs_received_out = gateway->total_msgs_received;
    }

    nimcp_mutex_unlock(&gateway->mutex);

    return 0;
}

const char* swarm_gateway_status_to_string(swarm_status_t status) {
    switch (status) {
        case SWARM_STATUS_DISCONNECTED: return "DISCONNECTED";
        case SWARM_STATUS_CONNECTING:   return "CONNECTING";
        case SWARM_STATUS_CONNECTED:    return "CONNECTED";
        case SWARM_STATUS_DEGRADED:     return "DEGRADED";
        case SWARM_STATUS_TIMEOUT:      return "TIMEOUT";
        default:                        return "UNKNOWN";
    }
}

const char* swarm_gateway_msg_type_to_string(swarm_gateway_msg_type_t msg_type) {
    switch (msg_type) {
        case GATEWAY_MSG_LEARNING_UPDATE:    return "LEARNING_UPDATE";
        case GATEWAY_MSG_MISSION_PARAMS:     return "MISSION_PARAMS";
        case GATEWAY_MSG_THREAT_INTEL:       return "THREAT_INTEL";
        case GATEWAY_MSG_FORMATION_CMD:      return "FORMATION_CMD";
        case GATEWAY_MSG_RECALL:             return "RECALL";
        case GATEWAY_MSG_NEUROMOD_OVERRIDE:  return "NEUROMOD_OVERRIDE";
        case GATEWAY_MSG_HEARTBEAT:          return "HEARTBEAT";
        case GATEWAY_MSG_SYNC_REQUEST:       return "SYNC_REQUEST";
        default:                             return "UNKNOWN";
    }
}

/* ========================================================================
 * KG Self-Awareness Integration
 * ======================================================================== */

/**
 * @brief Query knowledge graph for module self-knowledge
 *
 * WHAT: Introspect module identity from knowledge graph
 * WHY:  Enable self-awareness and runtime reflection
 * HOW:  Query KG for Swarm_Gateway entity and its relations
 *
 * @param kg Knowledge graph reader
 * @return 1 if self-knowledge found, 0 otherwise
 */
int swarm_gateway_query_self_knowledge(kg_reader_t* kg)
{
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Swarm_Gateway");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            LOG_DEBUG("Gateway self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Swarm_Gateway");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Swarm_Gateway");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}
