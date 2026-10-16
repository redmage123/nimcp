/**
 * @file nimcp_swarm_signal.c
 * @brief NIMCP Swarm Signal Adapter Implementation with BBB Security
 *
 * SECURITY: Integrated with Blood-Brain Barrier for input validation,
 *           threat detection, and audit logging
 */

#include "swarm/nimcp_swarm_signal.h"
#include "security/nimcp_bbb_helpers.h"
#include <stdio.h>
#include "security/nimcp_security.h"
#include "utils/validation/nimcp_common.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/encoding/nimcp_positional_encoding.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/thread/nimcp_thread_rand.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_signal)

/* LoRa protocol constants */
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYNC_WORD 0x34
#define LORA_HEADER_SIZE 4
#define LORA_CRC_SIZE 2
#define LORA_MAX_PAYLOAD 255

/* Simulation queue constants */
#define SIM_QUEUE_SIZE 256
#define SIM_MAX_NODES 64

/* Packet header structure */
typedef struct {
    uint32_t source_id;
    uint32_t dest_id;
    uint32_t sequence;
    uint16_t payload_len;
    uint16_t crc;
} __attribute__((packed)) packet_header_t;

/* Simulation queue packet */
typedef struct {
    uint8_t data[LORA_MAX_PAYLOAD + sizeof(packet_header_t)];
    uint32_t len;
    uint32_t source_id;
    uint64_t timestamp_ns;
} sim_packet_t;

/* Simulation queue */
typedef struct {
    sim_packet_t packets[SIM_QUEUE_SIZE];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    nimcp_mutex_t lock;
} sim_queue_t;

/* Global simulation queues (one per node) */
static sim_queue_t* g_sim_queues[SIM_MAX_NODES] = {0};
static nimcp_mutex_t g_sim_lock;

/* Thread-safe one-time initialization using pthread_once pattern */
static nimcp_once_t g_sim_once = NIMCP_ONCE_INIT;

/**
 * @brief Signal adapter structure
 */
struct nimcp_swarm_signal_adapter {
    swarm_signal_config_t config;
    swarm_signal_stats_t stats;

    uint32_t node_id;
    uint32_t sequence_num;

    /* Timing for latency tracking */
    uint64_t last_send_time_ns;
    uint64_t total_latency_samples;

    /* Simulation queue index */
    int sim_queue_index;

    /* Thread safety */
    nimcp_mutex_t stats_lock;

    /* Operational state */
    bool operational;

    /* Positional encoding support */
    nimcp_pos_encoder_t* pe_encoder;  /**< Positional encoder instance (optional) */

    /* Bio-async integration */
    bio_module_context_t bio_ctx;      /**< Bio-async module context */
    bool bio_async_enabled;            /**< Whether bio-async is active */
};

/* ==================== Internal Functions ==================== */

/**
 * @brief Calculate CRC16 checksum
 */
static uint16_t calculate_crc16(const uint8_t* data, uint32_t len) {
    uint16_t crc = 0xFFFF;

    for (uint32_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

/**
 * @brief One-time initialization routine for simulation subsystem
 *
 * Called exactly once via nimcp_once() to prevent race conditions.
 */
static void sim_init_once(void) {
    nimcp_mutex_init(&g_sim_lock, NULL);
    memset(g_sim_queues, 0, sizeof(g_sim_queues));
}

/**
 * @brief Initialize simulation subsystem (thread-safe)
 *
 * Uses pthread_once pattern via nimcp_once() to ensure exactly one
 * thread initializes the subsystem, preventing TOCTOU race conditions.
 */
static void sim_init(void) {
    nimcp_once(&g_sim_once, sim_init_once);
}

/**
 * @brief Allocate simulation queue
 */
static int sim_queue_alloc(void) {
    sim_init();

    nimcp_mutex_lock(&g_sim_lock);

    int index = -1;
    for (int i = 0; i < SIM_MAX_NODES; i++) {
        if (g_sim_queues[i] == NULL) {
            g_sim_queues[i] = (sim_queue_t*)nimcp_malloc(sizeof(sim_queue_t));
            if (g_sim_queues[i]) {
                memset(g_sim_queues[i], 0, sizeof(sim_queue_t));
                nimcp_mutex_init(&g_sim_queues[i]->lock, NULL);
                index = i;
            }
            break;
        }
    }

    nimcp_mutex_unlock(&g_sim_lock);
    return index;
}

/**
 * @brief Free simulation queue
 */
static void sim_queue_free(int index) {
    if (index < 0 || index >= SIM_MAX_NODES) {
        return;
    }

    nimcp_mutex_lock(&g_sim_lock);

    if (g_sim_queues[index]) {
        nimcp_mutex_destroy(&g_sim_queues[index]->lock);
        nimcp_free(g_sim_queues[index]);
        g_sim_queues[index] = NULL;
    }

    nimcp_mutex_unlock(&g_sim_lock);
}

/**
 * @brief Enqueue packet to simulation queue
 */
static bool sim_queue_enqueue(int index, const uint8_t* data, uint32_t len,
                              uint32_t source_id) {
    if (index < 0 || index >= SIM_MAX_NODES || !g_sim_queues[index]) {
        return false;  /* Invalid index - defensive guard */
    }

    // BBB: Validate buffer bounds
    if (!bbb_validate_buffer_access(data, 0, len, LORA_MAX_PAYLOAD * 2, "sim_queue_enqueue")) {
        return false;
    }

    sim_queue_t* queue = g_sim_queues[index];
    nimcp_mutex_lock(&queue->lock);

    if (queue->count >= SIM_QUEUE_SIZE) {
        nimcp_mutex_unlock(&queue->lock);
        return false;  /* Queue full - normal condition */
    }

    sim_packet_t* packet = &queue->packets[queue->tail];
    memcpy(packet->data, data, len);
    packet->len = len;
    packet->source_id = source_id;
    packet->timestamp_ns = nimcp_time_get_us() * 1000;

    queue->tail = (queue->tail + 1) % SIM_QUEUE_SIZE;
    queue->count++;

    nimcp_mutex_unlock(&queue->lock);
    return true;
}

/**
 * @brief Dequeue packet from simulation queue
 */
static bool sim_queue_dequeue(int index, uint8_t* data, uint32_t* len,
                              uint32_t* source_id, uint64_t* timestamp_ns) {
    if (index < 0 || index >= SIM_MAX_NODES || !g_sim_queues[index]) {
        return false;  /* Invalid index - defensive guard */
    }

    sim_queue_t* queue = g_sim_queues[index];
    nimcp_mutex_lock(&queue->lock);

    if (queue->count == 0) {
        nimcp_mutex_unlock(&queue->lock);
        return false;  /* Queue empty - normal condition */
    }

    sim_packet_t* packet = &queue->packets[queue->head];
    memcpy(data, packet->data, packet->len);
    *len = packet->len;
    *source_id = packet->source_id;
    *timestamp_ns = packet->timestamp_ns;

    queue->head = (queue->head + 1) % SIM_QUEUE_SIZE;
    queue->count--;

    nimcp_mutex_unlock(&queue->lock);
    return true;
}

/**
 * @brief Broadcast to all simulation queues
 */
static void sim_queue_broadcast(const uint8_t* data, uint32_t len,
                                uint32_t source_id, int exclude_index) {
    nimcp_mutex_lock(&g_sim_lock);

    for (int i = 0; i < SIM_MAX_NODES; i++) {
        if (i != exclude_index && g_sim_queues[i]) {
            sim_queue_enqueue(i, data, len, source_id);
        }
    }

    nimcp_mutex_unlock(&g_sim_lock);
}

/**
 * @brief Encode packet with LoRa framing
 */
static bool encode_lora_packet(uint8_t* output, uint32_t* output_len,
                               const uint8_t* payload, uint32_t payload_len,
                               uint32_t source_id, uint32_t dest_id,
                               uint32_t sequence) {
    if (payload_len > LORA_MAX_PAYLOAD) {
        return false;  /* Payload too large */
    }

    uint32_t pos = 0;

    /* Preamble */
    for (int i = 0; i < LORA_PREAMBLE_LENGTH; i++) {
        output[pos++] = 0xAA;
    }

    /* Sync word */
    output[pos++] = LORA_SYNC_WORD;

    /* Header */
    packet_header_t header;
    header.source_id = source_id;
    header.dest_id = dest_id;
    header.sequence = sequence;
    header.payload_len = (uint16_t)payload_len;
    header.crc = 0;

    memcpy(&output[pos], &header, sizeof(header));
    pos += sizeof(header);

    /* Payload */
    memcpy(&output[pos], payload, payload_len);
    pos += payload_len;

    /* Calculate CRC over header + payload */
    uint16_t crc = calculate_crc16(&output[LORA_PREAMBLE_LENGTH + 1],
                                   sizeof(header) + payload_len);
    memcpy(&output[pos], &crc, sizeof(crc));
    pos += sizeof(crc);

    /* Note: header.crc field is not modified after CRC calculation
     * to ensure CRC verification succeeds on decode */

    *output_len = pos;
    return true;
}

/**
 * @brief Decode LoRa packet
 */
static bool decode_lora_packet(const uint8_t* input, uint32_t input_len,
                               uint8_t* payload, uint32_t* payload_len,
                               uint32_t* source_id, uint32_t* dest_id) {
    if (input_len < LORA_PREAMBLE_LENGTH + 1 + sizeof(packet_header_t) + LORA_CRC_SIZE) {
        LOG_DEBUG("decode_lora_packet: too short: len=%u need=%zu",
                  input_len, LORA_PREAMBLE_LENGTH + 1 + sizeof(packet_header_t) + LORA_CRC_SIZE);
        return false;  /* Packet too short */
    }

    /* Verify preamble */
    for (int i = 0; i < LORA_PREAMBLE_LENGTH; i++) {
        if (input[i] != 0xAA) {
            LOG_DEBUG("decode_lora_packet: bad preamble at %d: 0x%02x", i, input[i]);
            return false;  /* Bad preamble */
        }
    }

    /* Verify sync word */
    if (input[LORA_PREAMBLE_LENGTH] != LORA_SYNC_WORD) {
        LOG_DEBUG("decode_lora_packet: bad sync: 0x%02x expected 0x%02x",
                  input[LORA_PREAMBLE_LENGTH], LORA_SYNC_WORD);
        return false;  /* Bad sync word */
    }

    /* Extract header */
    packet_header_t header;
    memcpy(&header, &input[LORA_PREAMBLE_LENGTH + 1], sizeof(header));

    /* Verify CRC */
    uint32_t header_pos = LORA_PREAMBLE_LENGTH + 1;
    uint32_t payload_pos = header_pos + sizeof(packet_header_t);
    uint32_t crc_pos = payload_pos + header.payload_len;

    if (crc_pos + LORA_CRC_SIZE > input_len) {
        return false;  /* CRC position overflows input */
    }

    uint16_t calculated_crc = calculate_crc16(&input[header_pos],
                                              sizeof(header) + header.payload_len);
    uint16_t received_crc;
    memcpy(&received_crc, &input[crc_pos], sizeof(received_crc));

    if (calculated_crc != received_crc) {
        return false;  /* CRC mismatch */
    }

    /* Extract payload */
    if (header.payload_len > *payload_len) {
        return false;  /* Output buffer too small */
    }

    memcpy(payload, &input[payload_pos], header.payload_len);
    *payload_len = header.payload_len;
    *source_id = header.source_id;
    *dest_id = header.dest_id;

    return true;
}

/**
 * @brief Update statistics with latency
 */
static void update_latency_stats(nimcp_swarm_signal_adapter_t* adapter,
                                 double latency_ms) {
    nimcp_mutex_lock(&adapter->stats_lock);

    if (adapter->stats.avg_latency_ms == 0.0) {
        adapter->stats.avg_latency_ms = latency_ms;
        adapter->stats.min_latency_ms = latency_ms;
        adapter->stats.max_latency_ms = latency_ms;
    } else {
        /* Running average */
        uint64_t n = adapter->total_latency_samples;
        adapter->stats.avg_latency_ms =
            (adapter->stats.avg_latency_ms * n + latency_ms) / (n + 1);

        if (latency_ms < adapter->stats.min_latency_ms) {
            adapter->stats.min_latency_ms = latency_ms;
        }
        if (latency_ms > adapter->stats.max_latency_ms) {
            adapter->stats.max_latency_ms = latency_ms;
        }
    }

    adapter->total_latency_samples++;

    nimcp_mutex_unlock(&adapter->stats_lock);
}

/* ==================== Public API ==================== */

nimcp_swarm_signal_adapter_t* swarm_signal_adapter_create(
    const swarm_signal_config_t* config) {

    // BBB: Validate input pointer
    if (!bbb_check_pointer(config, "swarm_signal_adapter_create")) {
        bbb_audit_log(BBB_AUDIT_ERROR, "swarm_signal", "create_failed", "config=NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_signal_adapter_create: config is NULL");
        return NULL;
    }

    // BBB: Register module with security system
    bbb_register_module("swarm_signal", BBB_MODULE_TYPE_SWARM);

    /* Validate configuration */
    if (!bbb_validate_range_u(config->max_packet_size, 1, LORA_MAX_PAYLOAD,
                              "swarm_signal_adapter_create")) {
        bbb_audit_log(BBB_AUDIT_ERROR, "swarm_signal", "invalid_config",
                      "max_packet_size=%u exceeds max=%u", config->max_packet_size, LORA_MAX_PAYLOAD);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_signal_adapter_create: max_packet_size out of range");
        return NULL;
    }

    if (config->radio_type == SWARM_RADIO_CUSTOM) {
        if (!bbb_check_pointer(config->custom_send, "swarm_signal_adapter_create") ||
            !bbb_check_pointer(config->custom_recv, "swarm_signal_adapter_create")) {
            bbb_audit_log(BBB_AUDIT_ERROR, "swarm_signal", "invalid_config",
                          "Custom radio requires callbacks");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_signal_adapter_create: custom callbacks missing");
            return NULL;
        }
    }

    nimcp_swarm_signal_adapter_t* adapter =
        (nimcp_swarm_signal_adapter_t*)nimcp_malloc(sizeof(nimcp_swarm_signal_adapter_t));

    if (!adapter) {
        bbb_audit_log(BBB_AUDIT_ERROR, "swarm_signal", "alloc_failed", "size=%zu",
                      sizeof(nimcp_swarm_signal_adapter_t));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate signal adapter");
        return NULL;
    }

    memset(adapter, 0, sizeof(nimcp_swarm_signal_adapter_t));
    memcpy(&adapter->config, config, sizeof(swarm_signal_config_t));

    /* Initialize mutexes */
    nimcp_mutex_init(&adapter->stats_lock, NULL);

    /* Use configured node_id if provided, otherwise generate random */
    if (config->node_id > 0) {
        adapter->node_id = config->node_id;
    } else {
        srand((unsigned int)time(NULL));
        adapter->node_id = (uint32_t)nimcp_tl_rand();
    }

    /* Initialize simulation queue if needed */
    adapter->sim_queue_index = -1;
    if (config->radio_type == SWARM_RADIO_SIMULATION) {
        adapter->sim_queue_index = sim_queue_alloc();
        if (adapter->sim_queue_index < 0) {
            bbb_audit_log(BBB_AUDIT_ERROR, "swarm_signal", "sim_queue_alloc_failed", "");
            nimcp_mutex_destroy(&adapter->stats_lock);
            nimcp_free(adapter);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_signal_adapter_create: sim queue alloc failed");
            return NULL;
        }
    }

    adapter->operational = true;

    // Initialize PE encoder as NULL (not configured by default)
    adapter->pe_encoder = NULL;

    // Initialize bio-async fields
    adapter->bio_ctx = NULL;
    adapter->bio_async_enabled = false;

    // BBB: Log successful creation
    bbb_audit_log(BBB_AUDIT_INFO, "swarm_signal", "adapter_created",
                  "type=%d node_id=%u max_packet=%u", config->radio_type, adapter->node_id, config->max_packet_size);

    LOG_INFO("Created swarm signal adapter: type=%d, node_id=%u, max_packet=%u",
             config->radio_type, adapter->node_id, config->max_packet_size);

    return adapter;
}

void swarm_signal_adapter_destroy(nimcp_swarm_signal_adapter_t* adapter) {
    if (!bbb_check_pointer(adapter, "swarm_signal_adapter_destroy")) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (adapter->bio_async_enabled) {
        swarm_signal_disconnect_bio_async(adapter);
    }

    // BBB: Log destruction
    bbb_audit_log(BBB_AUDIT_INFO, "swarm_signal", "adapter_destroyed", "node_id=%u", adapter->node_id);

    LOG_INFO("Destroying swarm signal adapter: node_id=%u", adapter->node_id);

    /* Free simulation queue */
    if (adapter->sim_queue_index >= 0) {
        sim_queue_free(adapter->sim_queue_index);
    }

    nimcp_mutex_destroy(&adapter->stats_lock);
    nimcp_free(adapter);
}

bool swarm_signal_send(nimcp_swarm_signal_adapter_t* adapter,
                      const uint8_t* data, uint32_t len, uint32_t dest_id) {

    // BBB: Validate inputs
    if (!bbb_check_pointer(adapter, "swarm_signal_send")) {
        /* bbb_check_pointer already logged the error */
        return false;
    }
    if (!bbb_check_pointer(data, "swarm_signal_send")) {
        /* bbb_check_pointer already logged the error */
        return false;
    }
    if (!bbb_validate_range_u(len, 1, LORA_MAX_PAYLOAD, "swarm_signal_send")) {
        /* bbb_validate_range_u already logged the error */
        return false;
    }

    // BBB: Validate network data for injection attacks
    if (!bbb_validate_network_data(data, len, "swarm_signal_send")) {
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_signal", "invalid_data",
                      "length=%u dest=%u", len, dest_id);
        nimcp_mutex_lock(&adapter->stats_lock);
        adapter->stats.packets_dropped++;
        nimcp_mutex_unlock(&adapter->stats_lock);
        /* bbb_validate_network_data already logged the error */
        return false;
    }

    if (!adapter->operational) {
        LOG_WARNING("swarm_signal_send: Adapter not operational");
        /* Adapter not operational - not an immune error */
        return false;
    }

    if (len > adapter->config.max_packet_size) {
        LOG_ERROR("swarm_signal_send: Packet size %u exceeds max %u",
                  len, adapter->config.max_packet_size);
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_signal", "packet_too_large",
                      "size=%u max=%u", len, adapter->config.max_packet_size);
        nimcp_mutex_lock(&adapter->stats_lock);
        adapter->stats.packets_dropped++;
        nimcp_mutex_unlock(&adapter->stats_lock);
        /* Validation failed - not an immune error */
        return false;
    }

    bool success = false;
    uint32_t attempts = 0;

    adapter->last_send_time_ns = nimcp_time_get_us() * 1000;

    while (attempts <= adapter->config.retry_count && !success) {
        switch (adapter->config.radio_type) {
            case SWARM_RADIO_LORA: {
                uint8_t encoded[LORA_MAX_PAYLOAD * 2];
                uint32_t encoded_len;

                if (!encode_lora_packet(encoded, &encoded_len, data, len,
                                       adapter->node_id, dest_id,
                                       adapter->sequence_num)) {
                    LOG_ERROR("swarm_signal_send: LoRa encoding failed");
                    break;
                }

                success = true;
                break;
            }

            case SWARM_RADIO_WIFI:
            case SWARM_RADIO_BLUETOOTH:
            case SWARM_RADIO_ULTRASONIC:
            case SWARM_RADIO_OPTICAL:
                success = true;
                break;

            case SWARM_RADIO_SIMULATION: {
                uint8_t encoded[LORA_MAX_PAYLOAD * 2];
                uint32_t encoded_len;

                if (!encode_lora_packet(encoded, &encoded_len, data, len,
                                       adapter->node_id, dest_id,
                                       adapter->sequence_num)) {
                    break;
                }

                if (dest_id == 0) {
                    sim_queue_broadcast(encoded, encoded_len, adapter->node_id,
                                      adapter->sim_queue_index);
                    success = true;
                } else {
                    sim_queue_broadcast(encoded, encoded_len, adapter->node_id,
                                      adapter->sim_queue_index);
                    success = true;
                }
                break;
            }

            case SWARM_RADIO_CUSTOM:
                if (adapter->config.custom_send) {
                    success = adapter->config.custom_send(data, len,
                                                         adapter->config.custom_ctx);
                }
                break;
        }

        attempts++;

        if (!success && attempts <= adapter->config.retry_count) {
            nimcp_mutex_lock(&adapter->stats_lock);
            adapter->stats.retransmits++;
            nimcp_mutex_unlock(&adapter->stats_lock);
        }
    }

    /* Update statistics */
    nimcp_mutex_lock(&adapter->stats_lock);

    if (success) {
        adapter->stats.packets_sent++;
        adapter->stats.bytes_sent += len;
        adapter->sequence_num++;
        bbb_audit_log(BBB_AUDIT_DEBUG, "swarm_signal", "packet_sent",
                      "dest=%u size=%u seq=%u", dest_id, len, adapter->sequence_num);
    } else {
        adapter->stats.packets_dropped++;
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_signal", "send_failed",
                      "dest=%u size=%u", dest_id, len);
    }

    nimcp_mutex_unlock(&adapter->stats_lock);

    return success;
}

bool swarm_signal_receive(nimcp_swarm_signal_adapter_t* adapter,
                         uint8_t* buffer, uint32_t buffer_size,
                         uint32_t* received_len, uint32_t* source_id) {

    // BBB: Validate inputs
    if (!bbb_check_pointer(adapter, "swarm_signal_receive")) {
        /* bbb_check_pointer already logged the error */
        return false;
    }
    if (!bbb_check_pointer(buffer, "swarm_signal_receive")) {
        /* bbb_check_pointer already logged the error */
        return false;
    }
    if (!bbb_check_pointer(received_len, "swarm_signal_receive")) {
        /* bbb_check_pointer already logged the error */
        return false;
    }
    if (!bbb_validate_range_u(buffer_size, 1, LORA_MAX_PAYLOAD * 2,
                              "swarm_signal_receive")) {
        /* bbb_check_pointer already logged the error */
        return false;
    }

    if (!adapter->operational) {
        /* Adapter not operational - not an immune error */
        return false;
    }

    bool received = false;

    switch (adapter->config.radio_type) {
        case SWARM_RADIO_SIMULATION: {
            uint8_t encoded[LORA_MAX_PAYLOAD * 2];
            uint32_t encoded_len;
            uint32_t src_id;
            uint64_t timestamp_ns;

            if (!sim_queue_dequeue(adapter->sim_queue_index, encoded,
                                  &encoded_len, &src_id, &timestamp_ns)) {
                /* Operation failed - not an immune error */
                return false;
            }

            /* Decode packet */
            uint32_t dest_id;
            if (!decode_lora_packet(encoded, encoded_len, buffer, received_len,
                                   &src_id, &dest_id)) {
                bbb_audit_log(BBB_AUDIT_WARNING, "swarm_signal", "decode_failed",
                              "encoded_len=%u", encoded_len);
                nimcp_mutex_lock(&adapter->stats_lock);
                adapter->stats.crc_errors++;
                nimcp_mutex_unlock(&adapter->stats_lock);
                /* Operation failed - not an immune error */
                return false;
            }

            // BBB: Validate received data for threats
            bbb_threat_type_t threat = bbb_detect_threat(buffer, *received_len);
            if (threat != BBB_THREAT_NONE) {
                bbb_audit_log(BBB_AUDIT_CRITICAL, "swarm_signal", "threat_detected",
                              "threat=%d src=%u len=%u", threat, src_id, *received_len);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_signal_receive: threat detected in received data");
                return false;
            }

            /* Check if packet is for us or broadcast */
            if (dest_id != 0 && dest_id != adapter->node_id) {
                return false;  /* Packet not for us - normal condition */
            }

            if (source_id) {
                *source_id = src_id;
            }

            /* Calculate latency */
            uint64_t now_ns = nimcp_time_get_us() * 1000;
            double latency_ms = (double)(now_ns - timestamp_ns) / 1e6;
            update_latency_stats(adapter, latency_ms);

            received = true;
            break;
        }

        case SWARM_RADIO_CUSTOM:
            if (adapter->config.custom_recv) {
                received = adapter->config.custom_recv(buffer, received_len,
                                                      adapter->config.custom_ctx);
                if (received && source_id) {
                    *source_id = 0;
                }
            }
            break;

        default:
            break;
    }

    if (received) {
        nimcp_mutex_lock(&adapter->stats_lock);
        adapter->stats.packets_received++;
        adapter->stats.bytes_received += *received_len;
        nimcp_mutex_unlock(&adapter->stats_lock);

        bbb_audit_log(BBB_AUDIT_DEBUG, "swarm_signal", "packet_received",
                      "src=%u size=%u", source_id ? *source_id : 0, *received_len);
    }

    return received;
}

bool swarm_signal_receive_blocking(nimcp_swarm_signal_adapter_t* adapter,
                                  uint8_t* buffer, uint32_t buffer_size,
                                  uint32_t* received_len, uint32_t* source_id,
                                  uint32_t timeout_ms) {

    // BBB: Validate inputs
    if (!bbb_check_pointer(adapter, "swarm_signal_receive_blocking")) {
        /* bbb_check_pointer already logged the error */
        return false;
    }
    if (!bbb_validate_range_u(timeout_ms, 0, 60000, "swarm_signal_receive_blocking")) {
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_signal", "invalid_timeout",
                      "timeout_ms=%u", timeout_ms);
        /* bbb_validate_range_u already logged the error */
        return false;
    }

    uint64_t start_time = nimcp_time_get_ms();
    uint64_t elapsed_ms = 0;

    while (elapsed_ms < timeout_ms) {
        if (swarm_signal_receive(adapter, buffer, buffer_size,
                                received_len, source_id)) {
            return true;
        }

        nimcp_platform_sleep_ms(1);

        elapsed_ms = nimcp_time_get_ms() - start_time;
    }

    return false;  /* Timeout expired - normal condition */
}

bool swarm_signal_broadcast(nimcp_swarm_signal_adapter_t* adapter,
                           const uint8_t* data, uint32_t len) {

    return swarm_signal_send(adapter, data, len, 0);
}

bool swarm_signal_get_stats(nimcp_swarm_signal_adapter_t* adapter,
                           swarm_signal_stats_t* stats) {

    // BBB: Validate inputs
    if (!bbb_check_pointer(adapter, "swarm_signal_get_stats")) {
        /* bbb_check_pointer already logged the error */
        return false;
    }
    if (!bbb_check_pointer(stats, "swarm_signal_get_stats")) {
        /* bbb_check_pointer already logged the error */
        return false;
    }

    nimcp_mutex_lock(&adapter->stats_lock);
    memcpy(stats, &adapter->stats, sizeof(swarm_signal_stats_t));
    nimcp_mutex_unlock(&adapter->stats_lock);

    return true;
}

bool swarm_signal_reset_stats(nimcp_swarm_signal_adapter_t* adapter) {
    // BBB: Validate input
    if (!bbb_check_pointer(adapter, "swarm_signal_reset_stats")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_signal_reset_stats: bbb_check_pointer is NULL");
        return false;
    }

    nimcp_mutex_lock(&adapter->stats_lock);
    memset(&adapter->stats, 0, sizeof(swarm_signal_stats_t));
    adapter->total_latency_samples = 0;
    nimcp_mutex_unlock(&adapter->stats_lock);

    return true;
}

const char* swarm_signal_radio_type_string(swarm_radio_type_t radio_type) {
    switch (radio_type) {
        case SWARM_RADIO_LORA:       return "LoRa";
        case SWARM_RADIO_WIFI:       return "WiFi";
        case SWARM_RADIO_BLUETOOTH:  return "Bluetooth";
        case SWARM_RADIO_ULTRASONIC: return "Ultrasonic";
        case SWARM_RADIO_OPTICAL:    return "Optical";
        case SWARM_RADIO_SIMULATION: return "Simulation";
        case SWARM_RADIO_CUSTOM:     return "Custom";
        default:                     return "Unknown";
    }
}

bool swarm_signal_set_tx_power(nimcp_swarm_signal_adapter_t* adapter,
                              int8_t tx_power_dbm) {

    // BBB: Validate inputs
    if (!bbb_check_pointer(adapter, "swarm_signal_set_tx_power")) {
        /* bbb_check_pointer already logged the error */
        return false;
    }
    if (!bbb_validate_range(tx_power_dbm, -20, 30, "swarm_signal_set_tx_power")) {
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_signal", "invalid_tx_power",
                      "tx_power=%d", tx_power_dbm);
        /* bbb_validate_range already logged the error */
        return false;
    }

    adapter->config.tx_power_dbm = tx_power_dbm;
    bbb_audit_log(BBB_AUDIT_INFO, "swarm_signal", "tx_power_set",
                  "power=%d node=%u", tx_power_dbm, adapter->node_id);
    LOG_INFO("Set TX power to %d dBm for node %u",
             tx_power_dbm, adapter->node_id);
    return true;
}

int8_t swarm_signal_get_tx_power(nimcp_swarm_signal_adapter_t* adapter) {
    if (!bbb_check_pointer(adapter, "swarm_signal_get_tx_power")) {
        return 0;
    }

    return adapter->config.tx_power_dbm;
}

bool swarm_signal_is_operational(nimcp_swarm_signal_adapter_t* adapter) {
    if (!bbb_check_pointer(adapter, "swarm_signal_is_operational")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_signal_is_operational: bbb_check_pointer is NULL");
        return false;
    }

    return adapter->operational;
}

bool swarm_signal_flush(nimcp_swarm_signal_adapter_t* adapter) {
    if (!bbb_check_pointer(adapter, "swarm_signal_flush")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_signal_flush: bbb_check_pointer is NULL");
        return false;
    }

    /* For simulation mode, clear the queue */
    if (adapter->config.radio_type == SWARM_RADIO_SIMULATION) {
        if (adapter->sim_queue_index >= 0 &&
            adapter->sim_queue_index < SIM_MAX_NODES) {
            sim_queue_t* queue = g_sim_queues[adapter->sim_queue_index];
            if (queue) {
                nimcp_mutex_lock(&queue->lock);
                queue->head = 0;
                queue->tail = 0;
                queue->count = 0;
                nimcp_mutex_unlock(&queue->lock);
            }
        }
    }

    bbb_audit_log(BBB_AUDIT_INFO, "swarm_signal", "queue_flushed",
                  "node=%u", adapter->node_id);
    LOG_INFO("Flushed adapter for node %u", adapter->node_id);
    return true;
}

//=============================================================================
// POSITIONAL ENCODING INTEGRATION
//=============================================================================

bool swarm_signal_set_pe_config(
    nimcp_swarm_signal_adapter_t* adapter,
    nimcp_pos_encoder_t* encoder
) {
    // BBB: Validate inputs
    if (!bbb_check_pointer(adapter, "swarm_signal_set_pe_config")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_signal_set_pe_config: bbb_check_pointer is NULL");
        return false;
    }
    if (!bbb_check_pointer(encoder, "swarm_signal_set_pe_config")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_signal_set_pe_config: bbb_check_pointer is NULL");
        return false;
    }

    // Validate encoder type (should be SINUSOIDAL or ALIBI)
    nimcp_pos_encoding_type_t type = nimcp_pos_get_type(encoder);
    if (type != NIMCP_POS_SINUSOIDAL && type != NIMCP_POS_ALIBI) {
        LOG_ERROR("swarm_signal_set_pe_config: Invalid encoder type %d (expected SINUSOIDAL or ALIBI)",
                  type);
        bbb_audit_log(BBB_AUDIT_ERROR, "swarm_signal", "invalid_pe_type",
                      "type=%d node=%u", type, adapter->node_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_signal_set_pe_config: validation failed");
        return false;
    }

    // Attach encoder to adapter
    adapter->pe_encoder = encoder;

    bbb_audit_log(BBB_AUDIT_INFO, "swarm_signal", "pe_configured",
                  "type=%s node=%u max_seq=%u dim=%u",
                  nimcp_pos_type_to_string(type), adapter->node_id,
                  nimcp_pos_get_max_length(encoder),
                  nimcp_pos_get_dim(encoder));

    LOG_INFO("Swarm signal PE configured: type=%s, node=%u, max_seq=%u, dim=%u",
             nimcp_pos_type_to_string(type), adapter->node_id,
             nimcp_pos_get_max_length(encoder),
             nimcp_pos_get_dim(encoder));

    return true;
}

bool swarm_signal_encode_sequence(
    nimcp_swarm_signal_adapter_t* adapter,
    uint32_t sequence_start,
    uint32_t sequence_length,
    float* embeddings_out
) {
    // BBB: Validate inputs
    if (!bbb_check_pointer(adapter, "swarm_signal_encode_sequence")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_signal_encode_sequence: bbb_check_pointer is NULL");
        return false;
    }
    if (!bbb_check_pointer(embeddings_out, "swarm_signal_encode_sequence")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_signal_encode_sequence: bbb_check_pointer is NULL");
        return false;
    }
    if (!bbb_validate_range_u(sequence_length, 1, 65536, "swarm_signal_encode_sequence")) {
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_signal", "invalid_seq_length",
                      "length=%u node=%u", sequence_length, adapter->node_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_signal_encode_sequence: bbb_validate_range_u is NULL");
        return false;
    }

    // Guard: check PE configured
    if (!adapter->pe_encoder) {
        LOG_WARNING("swarm_signal_encode_sequence: PE not configured for node %u",
                    adapter->node_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_signal_encode_sequence: adapter->pe_encoder is NULL");
        return false;
    }

    // Validate sequence fits within encoder limits
    uint32_t max_seq = nimcp_pos_get_max_length(adapter->pe_encoder);
    if (sequence_start + sequence_length > max_seq) {
        LOG_ERROR("swarm_signal_encode_sequence: sequence [%u, %u) exceeds max %u",
                  sequence_start, sequence_start + sequence_length, max_seq);
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_signal", "seq_out_of_range",
                      "start=%u len=%u max=%u node=%u",
                      sequence_start, sequence_length, max_seq, adapter->node_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_signal_encode_sequence: validation failed");
        return false;
    }

    // Encode sequence using PE encoder
    int result = nimcp_pos_encode_sequence(
        adapter->pe_encoder,
        sequence_start,
        sequence_length,
        embeddings_out
    );

    if (result != NIMCP_POS_SUCCESS) {
        LOG_ERROR("swarm_signal_encode_sequence: encoding failed with error %d", result);
        bbb_audit_log(BBB_AUDIT_ERROR, "swarm_signal", "encoding_failed",
                      "error=%d node=%u", result, adapter->node_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_signal_encode_sequence: validation failed");
        return false;
    }

    return true;
}

bool swarm_signal_get_temporal_embedding(
    nimcp_swarm_signal_adapter_t* adapter,
    float* output
) {
    // BBB: Validate inputs
    if (!bbb_check_pointer(adapter, "swarm_signal_get_temporal_embedding")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_signal_get_temporal_embedding: bbb_check_pointer is NULL");
        return false;
    }
    if (!bbb_check_pointer(output, "swarm_signal_get_temporal_embedding")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_signal_get_temporal_embedding: bbb_check_pointer is NULL");
        return false;
    }

    // Guard: check PE configured
    if (!adapter->pe_encoder) {
        LOG_WARNING("swarm_signal_get_temporal_embedding: PE not configured for node %u",
                    adapter->node_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_signal_get_temporal_embedding: adapter->pe_encoder is NULL");
        return false;
    }

    // Use current sequence number as position
    uint32_t position = adapter->sequence_num;

    // Validate position within encoder limits
    uint32_t max_seq = nimcp_pos_get_max_length(adapter->pe_encoder);
    if (position >= max_seq) {
        LOG_ERROR("swarm_signal_get_temporal_embedding: position %u exceeds max %u",
                  position, max_seq);
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_signal", "position_out_of_range",
                      "pos=%u max=%u node=%u", position, max_seq, adapter->node_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "swarm_signal_get_temporal_embedding: capacity exceeded");
        return false;
    }

    // Encode current position
    int result = nimcp_pos_encode_position(adapter->pe_encoder, position, output);
    if (result != NIMCP_POS_SUCCESS) {
        LOG_ERROR("swarm_signal_get_temporal_embedding: encoding failed with error %d", result);
        bbb_audit_log(BBB_AUDIT_ERROR, "swarm_signal", "temporal_encoding_failed",
                      "error=%d pos=%u node=%u", result, position, adapter->node_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_signal_get_temporal_embedding: validation failed");
        return false;
    }

    return true;
}

//=============================================================================
// Bio-async Integration API
//=============================================================================

/**
 * @brief Connect signal adapter to bio-async router
 *
 * WHAT: Register with bio-async messaging system
 * WHY:  Enable inter-module messaging for swarm coordination
 * HOW:  Register as BIO_MODULE_SWARM_SIGNAL
 */
bool swarm_signal_connect_bio_async(nimcp_swarm_signal_adapter_t* adapter)
{
    if (!bbb_check_pointer(adapter, "swarm_signal_connect_bio_async")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_signal_connect_bio_async: bbb_check_pointer is NULL");
        return false;
    }

    if (adapter->bio_async_enabled) {
        return true;  // Already connected
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SWARM_SIGNAL,
        .module_name = "swarm_signal",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = adapter
    };

    adapter->bio_ctx = bio_router_register_module(&info);
    if (adapter->bio_ctx) {
        adapter->bio_async_enabled = true;
        LOG_INFO("Connected to bio-async router");
    } else {
        LOG_INFO("Bio-async router not available, skipping registration");
    }

    return true;
}

/**
 * @brief Disconnect signal adapter from bio-async router
 *
 * WHAT: Unregister from bio-async messaging
 * WHY:  Clean shutdown
 * HOW:  Deregister and cleanup
 */
bool swarm_signal_disconnect_bio_async(nimcp_swarm_signal_adapter_t* adapter)
{
    if (!bbb_check_pointer(adapter, "swarm_signal_disconnect_bio_async")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_signal_disconnect_bio_async: bbb_check_pointer is NULL");
        return false;
    }

    if (!adapter->bio_async_enabled) {
        return true;  // Not connected
    }

    if (adapter->bio_ctx) {
        bio_router_unregister_module(adapter->bio_ctx);
        adapter->bio_ctx = NULL;
    }

    adapter->bio_async_enabled = false;
    LOG_INFO("Disconnected from bio-async router");

    return true;
}

/**
 * @brief Check if signal adapter is connected to bio-async
 *
 * WHAT: Query bio-async connection status
 * WHY:  Verify messaging availability
 * HOW:  Check flag
 */
bool swarm_signal_is_bio_async_connected(const nimcp_swarm_signal_adapter_t* adapter)
{
    if (!bbb_check_pointer(adapter, "swarm_signal_is_bio_async_connected")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_signal_is_bio_async_connected: bbb_check_pointer is NULL");
        return false;
    }

    return adapter->bio_async_enabled;
}

//=============================================================================
// KG Self-Awareness Integration
//=============================================================================

/**
 * @brief Query knowledge graph for module self-knowledge
 *
 * WHAT: Introspect module identity from knowledge graph
 * WHY:  Enable self-awareness and runtime reflection
 * HOW:  Query KG for Swarm_Signal entity and its relations
 *
 * @param kg Knowledge graph reader
 * @return 1 if self-knowledge found, 0 otherwise
 */
int swarm_signal_query_self_knowledge(kg_reader_t* kg)
{
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Swarm_Signal");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            LOG_DEBUG("Signal self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Swarm_Signal");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Swarm_Signal");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}
