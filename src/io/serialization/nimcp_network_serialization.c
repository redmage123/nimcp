#include <stddef.h>  /* for NULL */
// nimcp_network_serialization.c

#include "io/serialization/nimcp_network_serialization.h"
#include "io/serialization/nimcp_encryption.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/neuralnet/nimcp_neuron_synapse_access.h"
#include "utils/memory/nimcp_memory.h"
#include <string.h>
#include <time.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

// Global BBB security system
static bbb_system_t g_bbb_system = NULL;



//=============================================================================
// Security Initialization
//=============================================================================

/**
 * @brief Initialize security subsystem for network_serialization
 *
 * WHAT: Create and configure BBB system for input validation
 * WHY: Protect against malicious external input
 * HOW: Initialize with conservative security settings
 */
static void network_serialization_security_init(void) {
    if (g_bbb_system) {
        return;  // Already initialized
    }

    bbb_config_t config = bbb_default_config();
    config.strict_mode = false;  // Don't block, just log
    config.default_action = BBB_ACTION_LOG;
    config.input.validate_strings = true;
    config.input.validate_integers = true;
    config.input.max_string_length = 4096;  // Reasonable limit

    g_bbb_system = bbb_system_create(&config);
    if (!g_bbb_system) {
        LOG_ERROR("network_serialization: Failed to initialize security subsystem");
    } else {
        LOG_INFO("network_serialization: Security subsystem initialized");
    }
}

/**
 * @brief Cleanup security subsystem
 * WHY: __attribute__((destructor)) ensures cleanup on library unload,
 *      since this static function would otherwise never be called.
 */
__attribute__((destructor))
static void network_serialization_security_cleanup(void) {
    if (g_bbb_system) {
        bbb_system_destroy(g_bbb_system);
        g_bbb_system = NULL;
    }
}



#define LOG_MODULE "nimcp_network_serialization"
#define LOG_MODULE_ID 0x052E
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(network_serialization)

/* Single authoritative definition of neural_network_struct */
#include "core/neuralnet/nimcp_neuralnet_internal.h"

//=============================================================================
// Forward Declarations
//=============================================================================

static bool write_network_header(NimcpSerializer* serializer, bool compress);
static bool write_network_metadata(NimcpSerializer* serializer, neural_network_t network);
static bool write_network_config(NimcpSerializer* serializer, const network_config_t* config);
static bool write_neuron(NimcpSerializer* serializer, const neuron_t* neuron);
static bool write_synapse(NimcpSerializer* serializer, const synapse_handle_t* synapse);
static uint32_t calculate_checksum(const uint8_t* data, size_t length);

static bool read_network_header(NimcpSerializer* serializer, uint8_t* version, uint8_t* flags);
static bool read_network_metadata(NimcpSerializer* serializer, neural_network_t network);
static bool read_network_config(NimcpSerializer* serializer, network_config_t* config);
static bool read_neuron(NimcpSerializer* serializer, neuron_t* neuron);
static bool read_synapse(NimcpSerializer* serializer, synapse_handle_t* synapse);

//=============================================================================
// Error Messages
//=============================================================================

const char* nimcp_network_serial_strerror(nimcp_network_serial_result_t result)
{
    switch (result) {
        case NIMCP_NETWORK_SERIAL_SUCCESS:
            return "Success";
        case NIMCP_NETWORK_SERIAL_ERROR_NULL_NETWORK:
            return "NULL network pointer";
        case NIMCP_NETWORK_SERIAL_ERROR_NULL_SERIALIZER:
            return "NULL serializer pointer";
        case NIMCP_NETWORK_SERIAL_ERROR_WRITE_FAILED:
            return "Write operation failed";
        case NIMCP_NETWORK_SERIAL_ERROR_READ_FAILED:
            return "Read operation failed";
        case NIMCP_NETWORK_SERIAL_ERROR_INVALID_MAGIC:
            return "Invalid magic number";
        case NIMCP_NETWORK_SERIAL_ERROR_UNSUPPORTED_VERSION:
            return "Unsupported serialization version";
        case NIMCP_NETWORK_SERIAL_ERROR_CHECKSUM_MISMATCH:
            return "Checksum mismatch - data corrupted";
        case NIMCP_NETWORK_SERIAL_ERROR_ALLOCATION_FAILED:
            return "Memory allocation failed";
        case NIMCP_NETWORK_SERIAL_ERROR_CORRUPT_DATA:
            return "Corrupt or invalid data";
        case NIMCP_NETWORK_SERIAL_ERROR_ENCRYPTION_NOT_AVAILABLE:
            return "Encryption not available - library not compiled with libsodium";
        case NIMCP_NETWORK_SERIAL_ERROR_ENCRYPTION_FAILED:
            return "Encryption operation failed";
        case NIMCP_NETWORK_SERIAL_ERROR_DECRYPTION_FAILED:
            return "Decryption operation failed";
        case NIMCP_NETWORK_SERIAL_ERROR_INVALID_PASSWORD:
            return "Invalid password or tampered data";
        default:
            return "Unknown error";
    }
}

//=============================================================================
// Serialization Implementation
//=============================================================================

nimcp_network_serial_result_t nimcp_network_serialize(
    neural_network_t network,
    NimcpSerializer* serializer,
    bool compress,
    const char* password,
    size_t password_len,
    nimcp_serial_stats_t* stats
)
{
    if (!network) {
        return NIMCP_NETWORK_SERIAL_ERROR_NULL_NETWORK;
    }
    if (!serializer) {
        return NIMCP_NETWORK_SERIAL_ERROR_NULL_SERIALIZER;
    }

    // Check if encryption requested but not available
    bool encrypt = (password != NULL && password_len > 0);
    if (encrypt && !nimcp_encryption_available()) {
        return NIMCP_NETWORK_SERIAL_ERROR_ENCRYPTION_NOT_AVAILABLE;
    }

    // Reset serializer
    nimcp_serializer_reset(serializer);

    // Write header (encryption flag will be set later if needed)
    if (!write_network_header(serializer, compress)) {
        return NIMCP_NETWORK_SERIAL_ERROR_WRITE_FAILED;
    }

    // Write network metadata
    if (!write_network_metadata(serializer, network)) {
        return NIMCP_NETWORK_SERIAL_ERROR_WRITE_FAILED;
    }

    // Write configuration
    if (!write_network_config(serializer, &network->config)) {
        return NIMCP_NETWORK_SERIAL_ERROR_WRITE_FAILED;
    }

    // Write neuron count
    if (!nimcp_write_uint32(serializer, network->num_neurons)) {
        return NIMCP_NETWORK_SERIAL_ERROR_WRITE_FAILED;
    }

    // Write each neuron (including synapses)
    uint32_t total_synapses = 0;
    for (uint32_t i = 0; i < network->num_neurons; i++) {
        if (!write_neuron(serializer, &network->neurons[i])) {
            return NIMCP_NETWORK_SERIAL_ERROR_WRITE_FAILED;
        }
        total_synapses += NEURON_OUT_COUNT(&network->neurons[i]);
    }

    // Calculate and write checksum
    size_t data_length = nimcp_serializer_get_length(serializer);
    uint32_t checksum = calculate_checksum(
        nimcp_serializer_get_buffer(serializer) + 6,  // Skip magic + version + flags
        data_length - 6
    );
    if (!nimcp_write_uint32(serializer, checksum)) {
        return NIMCP_NETWORK_SERIAL_ERROR_WRITE_FAILED;
    }

    size_t uncompressed_size = nimcp_serializer_get_length(serializer);

    // Compress if requested
    // Strategy: Keep first 6 bytes (magic + version + flags) uncompressed
    // so we can read the compression flag during deserialization
    if (compress) {
        // Get the full data
        uint8_t* full_data = nimcp_serializer_get_buffer(serializer);
        size_t full_length = nimcp_serializer_get_length(serializer);

        // Save the header (first 6 bytes)
        uint8_t header[6];
        memcpy(header, full_data, 6);

        // Create a temporary serializer for the data after header
        NimcpSerializer* data_serializer = nimcp_serializer_create(full_length - 6);
        if (!data_serializer) {
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, full_length - 6,
                              "Failed to allocate compression serializer buffer");
            return NIMCP_NETWORK_SERIAL_ERROR_WRITE_FAILED;
        }

        // Copy data after header to temp serializer
        nimcp_write_bytes(data_serializer, full_data + 6, full_length - 6);

        // Compress the data portion
        NimcpSerialResult result = nimcp_serializer_compress(data_serializer);
        if (result != NIMCP_SERIAL_SUCCESS) {
            NIMCP_THROW_IO(NIMCP_ERROR_OPERATION_FAILED, "network_serialize",
                          "Failed to compress network data");
            nimcp_serializer_destroy(data_serializer);
            return NIMCP_NETWORK_SERIAL_ERROR_WRITE_FAILED;
        }

        // Get compressed data
        size_t compressed_length = nimcp_serializer_get_length(data_serializer);
        uint8_t* compressed_data = nimcp_serializer_get_buffer(data_serializer);

        // Reset main serializer and write header + compressed data
        nimcp_serializer_reset(serializer);
        nimcp_write_bytes(serializer, header, 6);
        nimcp_write_bytes(serializer, compressed_data, compressed_length);

        nimcp_serializer_destroy(data_serializer);
    }

    // Encrypt if requested
    // Strategy: Keep first 6 bytes (magic + version + flags) unencrypted
    // so we can read the encrypted flag during deserialization
    if (encrypt) {
        // Get the full data
        uint8_t* full_data = nimcp_serializer_get_buffer(serializer);
        size_t full_length = nimcp_serializer_get_length(serializer);

        // Save the header (first 6 bytes) and update encrypted flag
        uint8_t header[6];
        memcpy(header, full_data, 6);
        header[5] |= NIMCP_FLAG_ENCRYPTED;  // Set encrypted flag

        // Calculate encrypted size
        size_t plaintext_len = full_length - 6;
        size_t encrypted_len = nimcp_encrypted_size(plaintext_len);

        // Allocate buffer for encrypted data
        uint8_t* encrypted_buffer = (uint8_t*)nimcp_malloc(encrypted_len);
        if (!encrypted_buffer) {
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, encrypted_len,
                              "Failed to allocate encryption buffer");
            return NIMCP_NETWORK_SERIAL_ERROR_ALLOCATION_FAILED;
        }

        // Encrypt data after header
        size_t actual_encrypted_len = encrypted_len;
        if (!nimcp_encrypt_with_password(
                full_data + 6,           // Plaintext (data after header)
                plaintext_len,
                password,
                password_len,
                encrypted_buffer,
                &actual_encrypted_len
            )) {
            NIMCP_THROW_IO(NIMCP_ERROR_OPERATION_FAILED, "network_serialize",
                          "Failed to encrypt network data");
            nimcp_free(encrypted_buffer);
            return NIMCP_NETWORK_SERIAL_ERROR_ENCRYPTION_FAILED;
        }

        // Reset main serializer and write header + encrypted data
        nimcp_serializer_reset(serializer);
        nimcp_write_bytes(serializer, header, 6);
        nimcp_write_bytes(serializer, encrypted_buffer, actual_encrypted_len);

        nimcp_free(encrypted_buffer);
    }

    // Fill statistics if requested
    if (stats) {
        stats->total_bytes = uncompressed_size;
        stats->compressed_bytes = nimcp_serializer_get_length(serializer);
        stats->num_neurons_serialized = network->num_neurons;
        stats->num_synapses_serialized = total_synapses;
        stats->timestamp = (uint64_t)time(NULL);
        stats->compression_ratio = compress ?
            (float)uncompressed_size / (float)stats->compressed_bytes : 1.0F;
    }

    return NIMCP_NETWORK_SERIAL_SUCCESS;
}

//=============================================================================
// Deserialization Implementation
//=============================================================================

nimcp_network_serial_result_t nimcp_network_deserialize(
    NimcpSerializer* serializer,
    neural_network_t* network_out,
    const char* password,
    size_t password_len,
    nimcp_serial_stats_t* stats
)
{
    if (!serializer) {
        return NIMCP_NETWORK_SERIAL_ERROR_NULL_SERIALIZER;
    }
    if (!network_out) {
        return NIMCP_NETWORK_SERIAL_ERROR_NULL_NETWORK;
    }

    // Read header first (always unencrypted/uncompressed)
    uint8_t version, flags;
    if (!read_network_header(serializer, &version, &flags)) {

    // BBB: Validate external input
    // WHAT: Check input for security threats before processing
    // WHY: Prevent injection attacks and buffer overflows
    // TODO: Customize validation for specific input parameters
    /*
    bbb_validation_result_t val_result = {0};
    if (!bbb_validate_input(system, data, size, &val_result)) {
        LOG_ERROR("Input validation failed in if: %s", val_result.reason);
        return NIMCP_ERROR_INVALID_INPUT;
    }
    */
        return NIMCP_NETWORK_SERIAL_ERROR_INVALID_MAGIC;
    }

    if (version != NIMCP_SERIALIZATION_VERSION) {
        return NIMCP_NETWORK_SERIAL_ERROR_UNSUPPORTED_VERSION;
    }

    // Check if data is encrypted and handle decryption first
    NimcpSerializer* working_serializer = serializer;
    NimcpSerializer* decrypted_serializer = NULL;

    if (flags & NIMCP_FLAG_ENCRYPTED) {
        // Check if password provided
        if (!password || password_len == 0) {
            return NIMCP_NETWORK_SERIAL_ERROR_INVALID_PASSWORD;
        }

        // Check if decryption is available
        if (!nimcp_encryption_available()) {
            return NIMCP_NETWORK_SERIAL_ERROR_ENCRYPTION_NOT_AVAILABLE;
        }

        // Get the encrypted data (everything after the 6-byte header)
        size_t current_pos = nimcp_serializer_get_position(serializer);
        size_t total_length = nimcp_serializer_get_length(serializer);
        size_t encrypted_length = total_length - current_pos;

        uint8_t* full_buffer = nimcp_serializer_get_buffer(serializer);
        uint8_t* encrypted_data = full_buffer + current_pos;

        // Save header (without encrypted flag)
        uint8_t header[6];
        memcpy(header, full_buffer, 6);
        header[5] &= ~NIMCP_FLAG_ENCRYPTED;  // Clear encrypted flag

        // Estimate decrypted size (will be smaller than encrypted)
        size_t decrypted_len = encrypted_length;  // Upper bound
        uint8_t* decrypted_buffer = (uint8_t*)nimcp_malloc(decrypted_len);
        if (!decrypted_buffer) {
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, decrypted_len,
                              "Failed to allocate decryption buffer");
            return NIMCP_NETWORK_SERIAL_ERROR_ALLOCATION_FAILED;
        }

        // Decrypt data
        if (!nimcp_decrypt_with_password(
                encrypted_data,
                encrypted_length,
                password,
                password_len,
                decrypted_buffer,
                &decrypted_len
            )) {
            NIMCP_THROW_IO(NIMCP_ERROR_OPERATION_FAILED, "network_deserialize",
                          "Failed to decrypt network data - invalid password or corrupted data");
            nimcp_free(decrypted_buffer);
            return NIMCP_NETWORK_SERIAL_ERROR_INVALID_PASSWORD;
        }

        // Create new serializer with full decrypted buffer
        decrypted_serializer = nimcp_serializer_create(6 + decrypted_len);
        if (!decrypted_serializer) {
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, 6 + decrypted_len,
                              "Failed to allocate decrypted serializer");
            nimcp_free(decrypted_buffer);
            return NIMCP_NETWORK_SERIAL_ERROR_ALLOCATION_FAILED;
        }

        // Write header + decrypted data
        nimcp_write_bytes(decrypted_serializer, header, 6);
        nimcp_write_bytes(decrypted_serializer, decrypted_buffer, decrypted_len);

        nimcp_free(decrypted_buffer);

        // Use the decrypted serializer for the rest of deserialization
        working_serializer = decrypted_serializer;

        // Re-read header from decrypted data
        nimcp_serializer_set_position(working_serializer, 0);
        read_network_header(working_serializer, &version, &flags);
    }

    // If data is compressed, we need to decompress and use a new serializer
    NimcpSerializer* decompressed_serializer = NULL;

    if (flags & NIMCP_FLAG_COMPRESSED) {
        // Get the compressed data (everything after the 6-byte header)
        size_t current_pos = nimcp_serializer_get_position(working_serializer);
        size_t total_length = nimcp_serializer_get_length(working_serializer);
        size_t compressed_length = total_length - current_pos;

        uint8_t* full_buffer = nimcp_serializer_get_buffer(working_serializer);
        uint8_t* compressed_data = full_buffer + current_pos;

        // Save header (without compressed flag)
        uint8_t header[6];
        memcpy(header, full_buffer, 6);
        header[5] = 0;  // Clear compressed flag

        // Create temp serializer with compressed data
        NimcpSerializer* compressed_serializer = nimcp_serializer_create(compressed_length);
        if (!compressed_serializer) {
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, compressed_length,
                              "Failed to allocate compressed serializer buffer");
            if (decrypted_serializer) nimcp_serializer_destroy(decrypted_serializer);
            return NIMCP_NETWORK_SERIAL_ERROR_READ_FAILED;
        }

        nimcp_serializer_set_buffer(compressed_serializer, compressed_data, compressed_length);
        nimcp_serializer_mark_compressed(compressed_serializer);

        // Decompress
        NimcpSerialResult result = nimcp_serializer_decompress(compressed_serializer);
        if (result != NIMCP_SERIAL_SUCCESS) {
            NIMCP_THROW_IO(NIMCP_ERROR_OPERATION_FAILED, "network_deserialize",
                          "Failed to decompress network data");
            nimcp_serializer_destroy(compressed_serializer);
            if (decrypted_serializer) nimcp_serializer_destroy(decrypted_serializer);
            return NIMCP_NETWORK_SERIAL_ERROR_READ_FAILED;
        }

        // Get decompressed data
        size_t decompressed_length = nimcp_serializer_get_length(compressed_serializer);
        uint8_t* decompressed_data = nimcp_serializer_get_buffer(compressed_serializer);

        // Create new serializer with full decompressed buffer
        decompressed_serializer = nimcp_serializer_create(6 + decompressed_length);
        if (!decompressed_serializer) {
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, 6 + decompressed_length,
                              "Failed to allocate decompressed serializer");
            nimcp_serializer_destroy(compressed_serializer);
            if (decrypted_serializer) nimcp_serializer_destroy(decrypted_serializer);
            return NIMCP_NETWORK_SERIAL_ERROR_ALLOCATION_FAILED;
        }

        // Write header + decompressed data
        nimcp_write_bytes(decompressed_serializer, header, 6);
        nimcp_write_bytes(decompressed_serializer, decompressed_data, decompressed_length);

        nimcp_serializer_destroy(compressed_serializer);

        // Use the decompressed serializer for the rest of deserialization
        working_serializer = decompressed_serializer;

        // Re-read header from decompressed data
        nimcp_serializer_set_position(working_serializer, 0);
        read_network_header(working_serializer, &version, &flags);
    }

    // Read configuration to create network
    network_config_t config;

    // First need to read metadata to skip it
    uint64_t current_time = nimcp_read_uint64(working_serializer);
    uint64_t network_time = nimcp_read_uint64(working_serializer);
    float global_activity = nimcp_read_float(working_serializer);
    float network_stability = nimcp_read_float(working_serializer);
    float learning_momentum = nimcp_read_float(working_serializer);
    float last_avg_weight = nimcp_read_float(working_serializer);
    uint64_t last_maintenance = nimcp_read_uint64(working_serializer);

    if (!read_network_config(working_serializer, &config)) {

    // BBB: Validate external input
    // WHAT: Check input for security threats before processing
    // WHY: Prevent injection attacks and buffer overflows
    // TODO: Customize validation for specific input parameters
    /*
    bbb_validation_result_t val_result = {0};
    if (!bbb_validate_input(system, data, size, &val_result)) {
        LOG_ERROR("Input validation failed in if: %s", val_result.reason);
        return NIMCP_ERROR_INVALID_INPUT;
    }
    */
        if (decrypted_serializer) nimcp_serializer_destroy(decrypted_serializer);
        if (decompressed_serializer) nimcp_serializer_destroy(decompressed_serializer);
        return NIMCP_NETWORK_SERIAL_ERROR_READ_FAILED;
    }

    // Create network with configuration
    neural_network_t network = neural_network_create(&config);
    if (!network) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, 0,
                          "Failed to create neural network during deserialization");
        if (decrypted_serializer) nimcp_serializer_destroy(decrypted_serializer);
        if (decompressed_serializer) nimcp_serializer_destroy(decompressed_serializer);
        return NIMCP_NETWORK_SERIAL_ERROR_ALLOCATION_FAILED;
    }

    // Restore metadata
    network->current_time = current_time;
    network->network_time = network_time;
    network->global_activity = global_activity;
    network->network_stability = network_stability;
    network->learning_momentum = learning_momentum;
    network->last_avg_weight = last_avg_weight;
    network->last_maintenance = last_maintenance;

    // Read neuron count
    uint32_t num_neurons = nimcp_read_uint32(working_serializer);
    if (nimcp_serializer_has_error(working_serializer)) {
        neural_network_destroy(network);
        if (decrypted_serializer) nimcp_serializer_destroy(decrypted_serializer);
        if (decompressed_serializer) nimcp_serializer_destroy(decompressed_serializer);
        return NIMCP_NETWORK_SERIAL_ERROR_READ_FAILED;
    }

    // Validate neuron count is reasonable (catch corrupted data early)
    // MAX_NEURONS is typically 100,000
    if (num_neurons > MAX_NEURONS) {
        neural_network_destroy(network);
        if (decrypted_serializer) nimcp_serializer_destroy(decrypted_serializer);
        if (decompressed_serializer) nimcp_serializer_destroy(decompressed_serializer);
        return NIMCP_NETWORK_SERIAL_ERROR_CORRUPT_DATA;
    }

    // Read each neuron
    uint32_t total_synapses = 0;
    for (uint32_t i = 0; i < num_neurons; i++) {

    // BBB: Validate external input
    // WHAT: Check input for security threats before processing
    // WHY: Prevent injection attacks and buffer overflows
    // TODO: Customize validation for specific input parameters
    /*
    bbb_validation_result_t val_result = {0};
    if (!bbb_validate_input(system, data, size, &val_result)) {
        LOG_ERROR("Input validation failed in if: %s", val_result.reason);
        return NIMCP_ERROR_INVALID_INPUT;
    }
    */
        if (!read_neuron(working_serializer, &network->neurons[i])) {
            neural_network_destroy(network);
            if (decrypted_serializer) nimcp_serializer_destroy(decrypted_serializer);
            if (decompressed_serializer) nimcp_serializer_destroy(decompressed_serializer);
            return NIMCP_NETWORK_SERIAL_ERROR_READ_FAILED;
        }
        total_synapses += NEURON_OUT_COUNT(&network->neurons[i]);
    }
    network->num_neurons = num_neurons;

    // Rebuild incoming synapses from outgoing data for forward pass
    neural_network_rebuild_incoming(network);

    // Verify checksum
    uint32_t stored_checksum = nimcp_read_uint32(working_serializer);

    // Recalculate checksum from deserialized data (skip magic + version + flags = 6 bytes)
    size_t data_length = nimcp_serializer_get_position(working_serializer) - 4;  // Position before checksum
    const uint8_t* data_buffer = nimcp_serializer_get_buffer(working_serializer);
    uint32_t calculated_checksum = calculate_checksum(data_buffer + 6, data_length - 6);

    // Verify checksums match
    if (stored_checksum != calculated_checksum) {
        neural_network_destroy(network);
        if (decrypted_serializer) {
            nimcp_serializer_destroy(decrypted_serializer);
        }
        if (decompressed_serializer) {
            nimcp_serializer_destroy(decompressed_serializer);
        }
        return NIMCP_NETWORK_SERIAL_ERROR_CHECKSUM_MISMATCH;
    }

    // Clean up temporary serializers if we created them
    if (decrypted_serializer) {
        nimcp_serializer_destroy(decrypted_serializer);
    }
    if (decompressed_serializer) {
        nimcp_serializer_destroy(decompressed_serializer);
    }

    // Fill statistics if requested
    if (stats) {
        stats->total_bytes = nimcp_serializer_get_length(serializer);
        stats->compressed_bytes = 0;
        stats->num_neurons_serialized = num_neurons;
        stats->num_synapses_serialized = total_synapses;
        stats->timestamp = (uint64_t)time(NULL);
        stats->compression_ratio = 1.0F;
    }

    *network_out = network;
    return NIMCP_NETWORK_SERIAL_SUCCESS;
}

//=============================================================================
// Validation
//=============================================================================

bool nimcp_network_validate_serialized(NimcpSerializer* serializer)
{
    if (!serializer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_network_validate_serialized: serializer is NULL");

            return false;
    }

    size_t original_position = nimcp_serializer_get_position(serializer);
    nimcp_serializer_set_position(serializer, 0);

    // Check magic number
    uint32_t magic = nimcp_read_uint32(serializer);
    if (magic != NIMCP_SERIALIZATION_MAGIC) {
        nimcp_serializer_set_position(serializer, original_position);
        return false;  /* Invalid magic is normal validation result */
    }

    // Check version
    uint8_t version = nimcp_read_uint8(serializer);
    if (version != NIMCP_SERIALIZATION_VERSION) {
        nimcp_serializer_set_position(serializer, original_position);
        return false;  /* Version mismatch is normal validation result */
    }

    // Restore position
    nimcp_serializer_set_position(serializer, original_position);
    return true;
}

//=============================================================================
// Helper Functions - Write Operations
//=============================================================================

static bool write_network_header(NimcpSerializer* serializer, bool compress)
{
    if (!nimcp_write_uint32(serializer, NIMCP_SERIALIZATION_MAGIC)) {
        return false;
    }
    if (!nimcp_write_uint8(serializer, NIMCP_SERIALIZATION_VERSION)) {
        return false;
    }

    uint8_t flags = compress ? NIMCP_FLAG_COMPRESSED : 0;
    if (!nimcp_write_uint8(serializer, flags)) {
        return false;
    }

    return true;
}

static bool write_network_metadata(NimcpSerializer* serializer, neural_network_t network)
{
    if (!nimcp_write_uint64(serializer, network->current_time)) {

        return false;
    }
    if (!nimcp_write_uint64(serializer, network->network_time)) {

        return false;
    }
    if (!nimcp_write_float(serializer, network->global_activity)) {

        return false;
    }
    if (!nimcp_write_float(serializer, network->network_stability)) {

        return false;
    }
    if (!nimcp_write_float(serializer, network->learning_momentum)) {

        return false;
    }
    if (!nimcp_write_float(serializer, network->last_avg_weight)) {

        return false;
    }
    if (!nimcp_write_uint64(serializer, network->last_maintenance)) {

        return false;
    }
    return true;
}

static bool write_network_config(NimcpSerializer* serializer, const network_config_t* config)
{
    if (!nimcp_write_uint32(serializer, config->num_neurons)) {

        return false;
    }
    if (!nimcp_write_float(serializer, config->ei_ratio)) {

        return false;
    }
    if (!nimcp_write_float(serializer, config->learning_rate)) {

        return false;
    }
    if (!nimcp_write_float(serializer, config->hebbian_rate)) {

        return false;
    }
    if (!nimcp_write_float(serializer, config->stdp_window)) {

        return false;
    }
    if (!nimcp_write_float(serializer, config->homeostatic_rate)) {

        return false;
    }
    if (!nimcp_write_float(serializer, config->target_activity)) {

        return false;
    }
    if (!nimcp_write_float(serializer, config->adaptation_rate)) {

        return false;
    }
    if (!nimcp_write_float(serializer, config->refractory_period)) {

        return false;
    }
    if (!nimcp_write_float(serializer, config->min_weight)) {

        return false;
    }
    if (!nimcp_write_float(serializer, config->max_weight)) {

        return false;
    }
    if (!nimcp_write_uint32(serializer, config->update_interval)) {

        return false;
    }
    if (!nimcp_write_uint32(serializer, config->input_size)) {

        return false;
    }
    if (!nimcp_write_uint32(serializer, config->output_size)) {

        return false;
    }
    if (!nimcp_write_uint32(serializer, config->num_layers)) {

        return false;
    }
    if (!nimcp_write_bool(serializer, config->enable_stdp)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_network_config: serializer write failed");
        return false;
    }
    if (!nimcp_write_bool(serializer, config->enable_hebbian)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_network_config: serializer write failed");
        return false;
    }
    if (!nimcp_write_bool(serializer, config->enable_oja)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_network_config: serializer write failed");
        return false;
    }
    if (!nimcp_write_bool(serializer, config->enable_homeostasis)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_network_config: serializer write failed");
        return false;
    }
    // Note: layer_sizes array and neuron_model/model_params are not serialized in v1
    return true;
}

static bool write_neuron(NimcpSerializer* serializer, const neuron_t* neuron)
{
    // Write neuron basic properties
    if (!nimcp_write_uint32(serializer, neuron->id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_neuron: serializer write failed");
        return false;
    }
    if (!nimcp_write_uint8(serializer, (uint8_t)neuron->type)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_neuron: serializer write failed");
        return false;
    }
    if (!nimcp_write_float(serializer, neuron->state)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_neuron: serializer write failed");
        return false;
    }
    if (!nimcp_write_float(serializer, neuron->rest_potential)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_neuron: serializer write failed");
        return false;
    }
    if (!nimcp_write_float(serializer, neuron->threshold)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_neuron: serializer write failed");
        return false;
    }
    if (!nimcp_write_float(serializer, neuron->adaptation)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_neuron: serializer write failed");
        return false;
    }
    if (!nimcp_write_float(serializer, neuron->refractory_period)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_neuron: serializer write failed");
        return false;
    }
    if (!nimcp_write_float(serializer, neuron->bias)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_neuron: serializer write failed");
        return false;
    }

    // Write learning parameters (simplified - just key values)
    if (!nimcp_write_float(serializer, neuron->plasticity_rate)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_neuron: serializer write failed");
        return false;
    }
    if (!nimcp_write_float(serializer, neuron->homeostatic_factor)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_neuron: serializer write failed");
        return false;
    }
    if (!nimcp_write_float(serializer, neuron->calcium_concentration)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_neuron: serializer write failed");
        return false;
    }
    if (!nimcp_write_float(serializer, neuron->weight_norm)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_neuron: serializer write failed");
        return false;
    }
    if (!nimcp_write_float(serializer, neuron->avg_activity)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_neuron: serializer write failed");
        return false;
    }
    if (!nimcp_write_uint64(serializer, neuron->last_spike)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_neuron: serializer write failed");
        return false;
    }
    if (!nimcp_write_uint64(serializer, neuron->last_update)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_neuron: serializer write failed");
        return false;
    }
    if (!nimcp_write_uint64(serializer, neuron->creation_time)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_neuron: serializer write failed");
        return false;
    }

    // Write synapses
    uint32_t out_count = NEURON_OUT_COUNT(neuron);
    if (!nimcp_write_uint32(serializer, out_count)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_neuron: serializer write failed");
        return false;
    }
    for (uint32_t i = 0; i < out_count; i++) {
        synapse_handle_t* handle = NEURON_OUT_HANDLE((neuron_t*)neuron, i);
        if (!write_synapse(serializer, handle)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_neuron: synapse serialization failed");
            return false;
        }
    }

    // Note: spike_history, activity_history, and neuron model not serialized in v1
    return true;
}

static bool write_synapse(NimcpSerializer* serializer, const synapse_handle_t* synapse)
{
    if (!nimcp_write_uint32(serializer, synapse->target_neuron_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_synapse: serializer write failed");
        return false;
    }
    if (!nimcp_write_float(serializer, synapse->weight)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_synapse: serializer write failed");
        return false;
    }
    if (!nimcp_write_float(serializer, 0.0f)) {  // plasticity: not in handle
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_synapse: serializer write failed");
        return false;
    }
    if (!nimcp_write_float(serializer, 0.0f)) {  // last_change: not in handle
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_synapse: serializer write failed");
        return false;
    }
    if (!nimcp_write_uint64(serializer, 0)) {  // last_active: not in handle
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_synapse: serializer write failed");
        return false;
    }
    if (!nimcp_write_float(serializer, synapse->strength)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_synapse: serializer write failed");
        return false;
    }
    if (!nimcp_write_float(serializer, 0.0f)) {  // meta_plasticity: not in handle
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_synapse: serializer write failed");
        return false;
    }
    if (!nimcp_write_float(serializer, 0.0f)) {  // trace: not in handle
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_synapse: serializer write failed");
        return false;
    }
    // Note: STP state not serialized in v1
    return true;
}

//=============================================================================
// Helper Functions - Read Operations
//=============================================================================

static bool read_network_header(NimcpSerializer* serializer, uint8_t* version, uint8_t* flags)
{

    // BBB: Validate external input
    // WHAT: Check input for security threats before processing
    // WHY: Prevent injection attacks and buffer overflows
    // TODO: Customize validation for specific input parameters
    /*
    bbb_validation_result_t val_result = {0};
    if (!bbb_validate_input(system, data, size, &val_result)) {
        LOG_ERROR("Input validation failed in read_network_header: %s", val_result.reason);
        return NIMCP_ERROR_INVALID_INPUT;
    }
    */
    uint32_t magic = nimcp_read_uint32(serializer);
    if (magic != NIMCP_SERIALIZATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "read_network_header: validation failed");
        return false;
    }

    *version = nimcp_read_uint8(serializer);
    *flags = nimcp_read_uint8(serializer);


    // BBB: Validate external input
    // WHAT: Check input for security threats before processing
    // WHY: Prevent injection attacks and buffer overflows
    // TODO: Customize validation for specific input parameters
    /*
    bbb_validation_result_t val_result = {0};
    if (!bbb_validate_input(system, data, size, &val_result)) {
        LOG_ERROR("Input validation failed in read_network_config: %s", val_result.reason);
        return NIMCP_ERROR_INVALID_INPUT;
    }
    */
    return !nimcp_serializer_has_error(serializer);
}

static bool read_network_config(NimcpSerializer* serializer, network_config_t* config)
{
    config->num_neurons = nimcp_read_uint32(serializer);
    config->ei_ratio = nimcp_read_float(serializer);
    config->learning_rate = nimcp_read_float(serializer);
    config->hebbian_rate = nimcp_read_float(serializer);
    config->stdp_window = nimcp_read_float(serializer);
    config->homeostatic_rate = nimcp_read_float(serializer);
    config->target_activity = nimcp_read_float(serializer);
    config->adaptation_rate = nimcp_read_float(serializer);
    config->refractory_period = nimcp_read_float(serializer);
    config->min_weight = nimcp_read_float(serializer);
    config->max_weight = nimcp_read_float(serializer);
    config->update_interval = nimcp_read_uint32(serializer);
    config->input_size = nimcp_read_uint32(serializer);
    config->output_size = nimcp_read_uint32(serializer);
    config->num_layers = nimcp_read_uint32(serializer);
    config->enable_stdp = nimcp_read_bool(serializer);
    config->enable_hebbian = nimcp_read_bool(serializer);
    config->enable_oja = nimcp_read_bool(serializer);
    config->enable_homeostasis = nimcp_read_bool(serializer);

    // Validate counts are reasonable (detect corrupted data early)
    if (config->num_neurons > MAX_NEURONS || config->num_layers > 10000) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "read_network_config: validation failed");
        return false;  // Corrupted data
    }

    // Set defaults for non-serialized fields
    config->layer_sizes = NULL;
    config->neuron_model = NEURON_MODEL_LIF;
    config->model_params = NULL;

    // BBB: Validate external input
    // WHAT: Check input for security threats before processing
    // WHY: Prevent injection attacks and buffer overflows
    // TODO: Customize validation for specific input parameters
    /*
    bbb_validation_result_t val_result = {0};
    if (!bbb_validate_input(system, data, size, &val_result)) {
        LOG_ERROR("Input validation failed in read_neuron: %s", val_result.reason);
        return NIMCP_ERROR_INVALID_INPUT;
    }
    */

    return !nimcp_serializer_has_error(serializer);
}

static bool read_neuron(NimcpSerializer* serializer, neuron_t* neuron)
{
    neuron->id = nimcp_read_uint32(serializer);
    neuron->type = (neuron_type_t)nimcp_read_uint8(serializer);
    neuron->state = nimcp_read_float(serializer);
    neuron->rest_potential = nimcp_read_float(serializer);
    neuron->threshold = nimcp_read_float(serializer);
    neuron->adaptation = nimcp_read_float(serializer);
    neuron->refractory_period = nimcp_read_float(serializer);
    neuron->bias = nimcp_read_float(serializer);

    neuron->plasticity_rate = nimcp_read_float(serializer);
    neuron->homeostatic_factor = nimcp_read_float(serializer);
    neuron->calcium_concentration = nimcp_read_float(serializer);
    neuron->weight_norm = nimcp_read_float(serializer);
    neuron->avg_activity = nimcp_read_float(serializer);
    neuron->last_spike = nimcp_read_uint64(serializer);
    neuron->last_update = nimcp_read_uint64(serializer);
    neuron->creation_time = nimcp_read_uint64(serializer);

    uint32_t num_synapses = nimcp_read_uint32(serializer);

    // Validate synapse count is reasonable (detect corrupted data early)
    if (num_synapses > MAX_SYNAPSES_PER_NEURON) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "read_neuron: validation failed");
        return false;  // Corrupted data
    }

    // Initialize sparse outgoing storage and populate from serialized data
    sparse_synapse_storage_init(&neuron->outgoing);

    for (uint32_t i = 0; i < num_synapses; i++) {
        synapse_handle_t* handle = &neuron->outgoing.embedded[i];
        if (!read_synapse(serializer, handle)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "read_neuron: synapse deserialization failed");
            return false;
        }
        neuron->outgoing.embedded_count++;
    }

    // Initialize non-serialized fields to defaults
    sparse_synapse_storage_init(&neuron->incoming);

    // BBB: Validate external input
    // WHAT: Check input for security threats before processing
    // WHY: Prevent injection attacks and buffer overflows
    // TODO: Customize validation for specific input parameters
    /*
    bbb_validation_result_t val_result = {0};
    if (!bbb_validate_input(system, data, size, &val_result)) {
        LOG_ERROR("Input validation failed in read_synapse: %s", val_result.reason);
        return NIMCP_ERROR_INVALID_INPUT;
    }
    */
    neuron->spike_history_index = 0;
    neuron->spike_history_count = 0;

    return !nimcp_serializer_has_error(serializer);
}

static bool read_synapse(NimcpSerializer* serializer, synapse_handle_t* synapse)
{
    synapse->target_neuron_id = nimcp_read_uint32(serializer);
    synapse->weight = nimcp_read_float(serializer);
    (void)nimcp_read_float(serializer);   // plasticity: not in handle, skip
    (void)nimcp_read_float(serializer);   // last_change: not in handle, skip
    (void)nimcp_read_uint64(serializer);  // last_active: not in handle, skip
    synapse->strength = nimcp_read_float(serializer);
    (void)nimcp_read_float(serializer);   // meta_plasticity: not in handle, skip
    (void)nimcp_read_float(serializer);   // trace: not in handle, skip

    // Initialize remaining handle fields to defaults
    synapse->metadata_index = SPARSE_SYNAPSE_NO_METADATA;
    synapse->peer_index = SPARSE_SYNAPSE_NO_PEER;
    synapse->ternary_weight = 0;
    synapse->use_ternary_weight = 0;
    synapse->reserved[0] = 0;
    synapse->reserved[1] = 0;

    return !nimcp_serializer_has_error(serializer);
}

//=============================================================================
// Checksum
//=============================================================================

static uint32_t calculate_checksum(const uint8_t* data, size_t length)
{
    // Simple CRC32 implementation
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }

    return ~crc;
}
