/**
 * @file nimcp_swarm_signal.h
 * @brief NIMCP Swarm Signal Adapter for Radio/Transport Abstraction
 *
 * Provides a unified interface for different radio types and transport layers
 * used in swarm communication. Supports LoRa, WiFi, Bluetooth, ultrasonic,
 * optical, and custom radio implementations.
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_SWARM_SIGNAL_H
#define NIMCP_SWARM_SIGNAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Forward declaration for positional encoding
typedef struct nimcp_pos_encoder_s nimcp_pos_encoder_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Radio types supported by the swarm signal adapter
 */
typedef enum {
    SWARM_RADIO_LORA,       /**< Long range, low power (256 byte packets) */
    SWARM_RADIO_WIFI,       /**< High bandwidth */
    SWARM_RADIO_BLUETOOTH,  /**< Short range mesh */
    SWARM_RADIO_ULTRASONIC, /**< Acoustic/underwater */
    SWARM_RADIO_OPTICAL,    /**< Line-of-sight */
    SWARM_RADIO_SIMULATION, /**< For testing */
    SWARM_RADIO_CUSTOM      /**< User callback */
} swarm_radio_type_t;

/**
 * @brief Signal adapter configuration
 */
typedef struct {
    swarm_radio_type_t radio_type;   /**< Radio type */
    uint32_t frequency_hz;            /**< Radio frequency */
    uint32_t bandwidth_hz;            /**< Bandwidth */
    int8_t tx_power_dbm;              /**< Transmit power */
    uint32_t max_packet_size;         /**< Max payload bytes */
    uint32_t retry_count;             /**< Retransmit attempts */
    uint32_t timeout_ms;              /**< Receive timeout */
    uint32_t node_id;                 /**< Node identifier (0 = auto-assign) */

    /** Custom callback for SWARM_RADIO_CUSTOM */
    bool (*custom_send)(const uint8_t* data, uint32_t len, void* ctx);
    bool (*custom_recv)(uint8_t* data, uint32_t* len, void* ctx);
    void* custom_ctx;
} swarm_signal_config_t;

/**
 * @brief Signal adapter statistics
 */
typedef struct {
    uint64_t packets_sent;       /**< Total packets sent */
    uint64_t packets_received;   /**< Total packets received */
    uint64_t packets_dropped;    /**< Total packets dropped */
    uint64_t bytes_sent;         /**< Total bytes sent */
    uint64_t bytes_received;     /**< Total bytes received */
    uint64_t retransmits;        /**< Total retransmissions */
    double avg_latency_ms;       /**< Average latency in milliseconds */
    double min_latency_ms;       /**< Minimum latency */
    double max_latency_ms;       /**< Maximum latency */
    uint32_t buffer_overruns;    /**< Buffer overflow count */
    uint32_t crc_errors;         /**< CRC error count */
} swarm_signal_stats_t;

/**
 * @brief Opaque signal adapter handle
 */
typedef struct nimcp_swarm_signal_adapter nimcp_swarm_signal_adapter_t;

/**
 * @brief Create a swarm signal adapter
 *
 * @param config Configuration for the signal adapter
 * @return Pointer to signal adapter on success, NULL on failure
 */
nimcp_swarm_signal_adapter_t* swarm_signal_adapter_create(
    const swarm_signal_config_t* config
);

/**
 * @brief Destroy a swarm signal adapter
 *
 * @param adapter Signal adapter to destroy
 */
void swarm_signal_adapter_destroy(nimcp_swarm_signal_adapter_t* adapter);

/**
 * @brief Send encoded message through the adapter
 *
 * @param adapter Signal adapter
 * @param data Data to send
 * @param len Length of data
 * @param dest_id Destination ID (0 for broadcast)
 * @return true on success, false on failure
 */
bool swarm_signal_send(
    nimcp_swarm_signal_adapter_t* adapter,
    const uint8_t* data,
    uint32_t len,
    uint32_t dest_id
);

/**
 * @brief Receive message (non-blocking)
 *
 * @param adapter Signal adapter
 * @param buffer Buffer to receive data
 * @param buffer_size Size of receive buffer
 * @param received_len Pointer to store received length
 * @param source_id Pointer to store source ID (optional)
 * @return true if message received, false otherwise
 */
bool swarm_signal_receive(
    nimcp_swarm_signal_adapter_t* adapter,
    uint8_t* buffer,
    uint32_t buffer_size,
    uint32_t* received_len,
    uint32_t* source_id
);

/**
 * @brief Receive message (blocking with timeout)
 *
 * @param adapter Signal adapter
 * @param buffer Buffer to receive data
 * @param buffer_size Size of receive buffer
 * @param received_len Pointer to store received length
 * @param source_id Pointer to store source ID (optional)
 * @param timeout_ms Timeout in milliseconds
 * @return true if message received, false on timeout or error
 */
bool swarm_signal_receive_blocking(
    nimcp_swarm_signal_adapter_t* adapter,
    uint8_t* buffer,
    uint32_t buffer_size,
    uint32_t* received_len,
    uint32_t* source_id,
    uint32_t timeout_ms
);

/**
 * @brief Broadcast message to all nodes
 *
 * @param adapter Signal adapter
 * @param data Data to broadcast
 * @param len Length of data
 * @return true on success, false on failure
 */
bool swarm_signal_broadcast(
    nimcp_swarm_signal_adapter_t* adapter,
    const uint8_t* data,
    uint32_t len
);

/**
 * @brief Get signal adapter statistics
 *
 * @param adapter Signal adapter
 * @param stats Pointer to store statistics
 * @return true on success, false on failure
 */
bool swarm_signal_get_stats(
    nimcp_swarm_signal_adapter_t* adapter,
    swarm_signal_stats_t* stats
);

/**
 * @brief Reset signal adapter statistics
 *
 * @param adapter Signal adapter
 * @return true on success, false on failure
 */
bool swarm_signal_reset_stats(nimcp_swarm_signal_adapter_t* adapter);

/**
 * @brief Get radio type name as string
 *
 * @param radio_type Radio type
 * @return String representation of radio type
 */
const char* swarm_signal_radio_type_string(swarm_radio_type_t radio_type);

/**
 * @brief Set adapter transmit power
 *
 * @param adapter Signal adapter
 * @param tx_power_dbm Transmit power in dBm
 * @return true on success, false on failure
 */
bool swarm_signal_set_tx_power(
    nimcp_swarm_signal_adapter_t* adapter,
    int8_t tx_power_dbm
);

/**
 * @brief Get adapter transmit power
 *
 * @param adapter Signal adapter
 * @return Current transmit power in dBm, or 0 on error
 */
int8_t swarm_signal_get_tx_power(nimcp_swarm_signal_adapter_t* adapter);

/**
 * @brief Check if adapter is operational
 *
 * @param adapter Signal adapter
 * @return true if operational, false otherwise
 */
bool swarm_signal_is_operational(nimcp_swarm_signal_adapter_t* adapter);

/**
 * @brief Flush pending transmissions
 *
 * @param adapter Signal adapter
 * @return true on success, false on failure
 */
bool swarm_signal_flush(nimcp_swarm_signal_adapter_t* adapter);

//=============================================================================
// POSITIONAL ENCODING INTEGRATION
//=============================================================================

/**
 * @brief Set positional encoding configuration for signal adapter
 *
 * WHAT: Configure sinusoidal PE and ALiBi for packet sequence ordering
 * WHY:  Enable temporal context in swarm signal sequences
 * HOW:  Attach PE encoder to adapter for sequence numbering
 *
 * @param adapter Signal adapter to configure
 * @param encoder Positional encoder instance (SINUSOIDAL or ALIBI type)
 * @return true on success, false on failure
 */
bool swarm_signal_set_pe_config(
    nimcp_swarm_signal_adapter_t* adapter,
    nimcp_pos_encoder_t* encoder
);

/**
 * @brief Encode signal sequence with positional embeddings
 *
 * WHAT: Apply positional encoding to a sequence of signal packets
 * WHY:  Add temporal ordering context to packet sequences
 * HOW:  Generate PE for each position in sequence and apply to packet data
 *
 * @param adapter Signal adapter
 * @param sequence_start Starting sequence number
 * @param sequence_length Length of sequence
 * @param embeddings_out Output buffer for embeddings (seq_length * embedding_dim floats)
 * @return true on success, false if PE not configured
 */
bool swarm_signal_encode_sequence(
    nimcp_swarm_signal_adapter_t* adapter,
    uint32_t sequence_start,
    uint32_t sequence_length,
    float* embeddings_out
);

/**
 * @brief Get temporal embedding for current signal state
 *
 * WHAT: Retrieve positional encoding for current packet sequence position
 * WHY:  Add temporal context to individual signal transmissions
 * HOW:  Use current sequence number to generate PE
 *
 * @param adapter Signal adapter
 * @param output Output buffer for embedding (must be encoder's embedding_dim size)
 * @return true on success, false if PE not configured
 */
bool swarm_signal_get_temporal_embedding(
    nimcp_swarm_signal_adapter_t* adapter,
    float* output
);

//=============================================================================
// BIO-ASYNC INTEGRATION
//=============================================================================

/**
 * @brief Connect signal adapter to bio-async router
 *
 * WHAT: Register signal module with bio-async messaging system
 * WHY:  Enable inter-module messaging for swarm coordination
 * HOW:  Register with BIO_MODULE_SWARM_SIGNAL ID
 *
 * @param adapter Signal adapter
 * @return true on success, false on failure
 */
bool swarm_signal_connect_bio_async(nimcp_swarm_signal_adapter_t* adapter);

/**
 * @brief Disconnect signal adapter from bio-async router
 *
 * WHAT: Unregister from bio-async messaging system
 * WHY:  Clean shutdown of messaging
 * HOW:  Deregister module and cleanup
 *
 * @param adapter Signal adapter
 * @return true on success, false on failure
 */
bool swarm_signal_disconnect_bio_async(nimcp_swarm_signal_adapter_t* adapter);

/**
 * @brief Check if signal adapter is connected to bio-async
 *
 * WHAT: Query bio-async connection status
 * WHY:  Verify messaging availability
 * HOW:  Check bio_async_enabled flag
 *
 * @param adapter Signal adapter
 * @return true if connected, false otherwise
 */
bool swarm_signal_is_bio_async_connected(const nimcp_swarm_signal_adapter_t* adapter);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_SIGNAL_H */
