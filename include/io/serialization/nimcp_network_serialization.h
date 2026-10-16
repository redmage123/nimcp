// nimcp_network_serialization.h

#ifndef NIMCP_NETWORK_SERIALIZATION_H
#define NIMCP_NETWORK_SERIALIZATION_H

#include "io/serialization/nimcp_serialization.h"
#include "core/neuralnet/nimcp_neuralnet.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_network_serialization.h
 * @brief Network-level serialization for persistent student models
 *
 * WHAT: High-level serialization/deserialization for neural networks
 * WHY: Enable checkpoint/restore for Course Creator student models
 * HOW: Build on existing low-level binary serialization primitives
 *
 * DESIGN PRINCIPLES:
 * - Separation of concerns: Network logic vs storage (Course Creator responsibility)
 * - Binary format for compactness with optional compression
 * - Versioning for forward/backward compatibility
 * - Error handling with detailed diagnostics
 *
 * ARCHITECTURE:
 * - NIMCP Core: Provides serialize/deserialize primitives (THIS FILE)
 * - Course Creator: Handles student identity, access control, storage, versioning
 *
 * SERIALIZATION FORMAT (Version 1):
 * - Magic number: 0x4E494D43 ('NIMC')
 * - Version: uint8_t (currently 1)
 * - Flags: uint8_t (bit 0: compressed, bits 1-7: reserved)
 * - Network metadata (timestamps, activity, stability)
 * - Configuration
 * - For each neuron: state, parameters, synapses, history
 * - Checksum: uint32_t CRC32
 */

#define NIMCP_SERIALIZATION_MAGIC 0x4E494D43  // 'NIMC'
#define NIMCP_SERIALIZATION_VERSION 1
#define NIMCP_FLAG_COMPRESSED 0x01
#define NIMCP_FLAG_ENCRYPTED 0x02

/**
 * @brief Serialization result codes
 */
typedef enum {
    NIMCP_NETWORK_SERIAL_SUCCESS = 0,
    NIMCP_NETWORK_SERIAL_ERROR_NULL_NETWORK = -1,
    NIMCP_NETWORK_SERIAL_ERROR_NULL_SERIALIZER = -2,
    NIMCP_NETWORK_SERIAL_ERROR_WRITE_FAILED = -3,
    NIMCP_NETWORK_SERIAL_ERROR_READ_FAILED = -4,
    NIMCP_NETWORK_SERIAL_ERROR_INVALID_MAGIC = -5,
    NIMCP_NETWORK_SERIAL_ERROR_UNSUPPORTED_VERSION = -6,
    NIMCP_NETWORK_SERIAL_ERROR_CHECKSUM_MISMATCH = -7,
    NIMCP_NETWORK_SERIAL_ERROR_ALLOCATION_FAILED = -8,
    NIMCP_NETWORK_SERIAL_ERROR_CORRUPT_DATA = -9,
    NIMCP_NETWORK_SERIAL_ERROR_ENCRYPTION_NOT_AVAILABLE = -10,
    NIMCP_NETWORK_SERIAL_ERROR_ENCRYPTION_FAILED = -11,
    NIMCP_NETWORK_SERIAL_ERROR_DECRYPTION_FAILED = -12,
    NIMCP_NETWORK_SERIAL_ERROR_INVALID_PASSWORD = -13
} nimcp_network_serial_result_t;

/**
 * @brief Serialization statistics
 */
typedef struct {
    size_t total_bytes;
    size_t compressed_bytes;
    uint32_t num_neurons_serialized;
    uint32_t num_synapses_serialized;
    uint64_t timestamp;
    float compression_ratio;
} nimcp_serial_stats_t;

/**
 * @brief Serialize neural network to binary format
 *
 * WHAT: Converts neural network state to binary representation
 * WHY: Enable persistent storage for checkpointing
 * HOW: Uses low-level serialization primitives to write network state
 *
 * COMPLEXITY: O(N + S) where N=neurons, S=total synapses
 * MEMORY: O(N + S) for buffer allocation
 *
 * @param network Neural network to serialize
 * @param serializer Serializer instance (must be created by caller)
 * @param compress If true, compress the serialized data using LZ4
 * @param password Optional password for encryption (NULL = no encryption)
 * @param password_len Length of password in bytes (0 if password is NULL)
 * @param stats Optional statistics output (can be NULL)
 * @return Result code (NIMCP_NETWORK_SERIAL_SUCCESS on success)
 */
nimcp_network_serial_result_t nimcp_network_serialize(
    neural_network_t network,
    NimcpSerializer* serializer,
    bool compress,
    const char* password,
    size_t password_len,
    nimcp_serial_stats_t* stats
);

/**
 * @brief Deserialize neural network from binary format
 *
 * WHAT: Reconstructs neural network from binary representation
 * WHY: Restore checkpointed network state
 * HOW: Reads binary format, validates, allocates network, restores state
 *
 * COMPLEXITY: O(N + S) where N=neurons, S=total synapses
 * MEMORY: O(N + S) for network allocation
 *
 * @param serializer Serializer with network data (position at start of data)
 * @param network_out Output pointer to receive reconstructed network
 * @param password Optional password for decryption (NULL if not encrypted)
 * @param password_len Length of password in bytes (0 if password is NULL)
 * @param stats Optional statistics output (can be NULL)
 * @return Result code (NIMCP_NETWORK_SERIAL_SUCCESS on success)
 */
nimcp_network_serial_result_t nimcp_network_deserialize(
    NimcpSerializer* serializer,
    neural_network_t* network_out,
    const char* password,
    size_t password_len,
    nimcp_serial_stats_t* stats
);

/**
 * @brief Get human-readable error message for result code
 *
 * @param result Result code
 * @return Error message string (statically allocated)
 */
const char* nimcp_network_serial_strerror(nimcp_network_serial_result_t result);

/**
 * @brief Validate serialized network data without full deserialization
 *
 * WHAT: Quick validation of serialized network
 * WHY: Detect corruption before expensive deserialization
 * HOW: Checks magic, version, checksum without full reconstruction
 *
 * COMPLEXITY: O(1) - only reads header and checksum
 *
 * @param serializer Serializer with network data
 * @return true if data appears valid, false otherwise
 */
bool nimcp_network_validate_serialized(NimcpSerializer* serializer);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_NETWORK_SERIALIZATION_H
