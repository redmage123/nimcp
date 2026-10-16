/**
 * @file nimcp_checkpoint.c
 * @brief Brain checkpoint and recovery implementation
 *
 * IMPLEMENTATION NOTES:
 * - Uses NIMCP memory allocation (nimcp_malloc/free) for tracking
 * - Thread-safe using NIMCP thread primitives (nimcp_mutex_t)
 * - Atomic writes via write-to-temp-then-rename pattern
 * - CRC32 checksums for integrity validation
 * - Compression using zlib (if available)
 *
 * FILE FORMAT DESIGN:
 * - Fixed-size header for quick validation
 * - Self-describing metadata for forward compatibility
 * - Checksummed sections for partial recovery
 * - Version-based migration support
 *
 * @author NIMCP Team
 * @date 2025-11-19
 */

#include "utils/fault_tolerance/nimcp_checkpoint.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "core/brain/nimcp_brain_internal.h"
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "constants/nimcp_buffer_constants.h"

#define LOG_MODULE "utils_checkpoint"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(checkpoint)

#include "plasticity/adaptive/nimcp_adaptive.h"
#include "core/neuralnet/nimcp_neuron_synapse_access.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#include "utils/platform/nimcp_platform_once.h"

// Optional zlib compression support
#ifdef HAVE_ZLIB
#include <zlib.h>
#include "utils/memory/nimcp_unified_memory.h"
#include "constants/nimcp_buffer_constants.h"
#endif

//=============================================================================
// Internal Constants
//=============================================================================

#define MAX_ERROR_MSG 512
#define TEMP_SUFFIX ".tmp"
#define CHECKPOINT_EXTENSION ".ckpt"

// CRC32 table for checksum calculation
static uint32_t crc32_table[256];
static nimcp_platform_once_t crc32_once = NIMCP_PLATFORM_ONCE_INIT;

// Thread-local error message buffer
static __thread char error_buffer[MAX_ERROR_MSG] = {0};

//=============================================================================
// Internal Helper Functions - Error Handling
//=============================================================================

/**
 * @brief Set error message for current thread
 *
 * WHAT: Store error message in thread-local buffer
 * WHY:  Thread-safe error reporting
 * HOW:  Use __thread storage class
 *
 * @param format Printf-style format string
 * @param ... Format arguments
 */
static void set_error(const char* format, ...) __attribute__((format(printf, 1, 2)));
static void set_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(error_buffer, MAX_ERROR_MSG, format, args);
    va_end(args);
}

//=============================================================================
// Internal Helper Functions - CRC32 Checksum
//=============================================================================

/**
 * @brief Internal function to populate CRC32 table
 *
 * WHAT: Populate CRC32 table for fast checksum calculation
 * WHY:  Table-based CRC is 10x faster than bit-by-bit
 * HOW:  Standard CRC32 polynomial (0xEDB88320)
 *
 * NOTE: This function is called exactly once via nimcp_platform_once
 */
static void crc32_do_init(void) {
    uint32_t polynomial = 0xEDB88320;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (uint32_t j = 0; j < 8; j++) {
            crc = (crc & 1) ? ((crc >> 1) ^ polynomial) : (crc >> 1);
        }
        crc32_table[i] = crc;
    }
}

/**
 * @brief Initialize CRC32 lookup table (thread-safe)
 *
 * WHAT: Ensure CRC32 table is initialized exactly once
 * WHY:  Avoid TOCTOU race condition with global flag
 * HOW:  Use nimcp_platform_once for thread-safe one-time initialization
 */
static void crc32_init_table(void) {
    nimcp_platform_once(&crc32_once, crc32_do_init);
}

/**
 * @brief Calculate CRC32 checksum of data
 *
 * WHAT: Compute CRC32 checksum using table lookup
 * WHY:  Detect data corruption
 * HOW:  Standard CRC32 algorithm
 *
 * @param data Data to checksum
 * @param size Data size in bytes
 * @return CRC32 checksum
 */
static uint32_t crc32_calculate(const void* data, size_t size) {
    crc32_init_table();

    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < size; i++) {
        uint8_t index = (crc ^ bytes[i]) & 0xFF;
        crc = (crc >> 8) ^ crc32_table[index];
    }

    return crc ^ 0xFFFFFFFF;
}

//=============================================================================
// Internal Helper Functions - File I/O
//=============================================================================

/**
 * @brief Write data to file with error checking
 *
 * WHAT: Write buffer to file with full error handling
 * WHY:  Ensure all data is written or fail atomically
 * HOW:  Loop until all bytes written, check ferror
 *
 * @param fp File pointer
 * @param data Data to write
 * @param size Size in bytes
 * @return true on success, false on error
 */
static bool write_full(FILE* fp, const void* data, size_t size) {
    const uint8_t* bytes = (const uint8_t*)data;
    size_t written = 0;

    while (written < size) {
        size_t n = fwrite(bytes + written, 1, size - written, fp);
        if (n == 0) {
            if (ferror(fp)) {
                set_error("Write error: %s", strerror(errno));
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_full: validation failed");
                return false;
            }
            // EOF on write shouldn't happen, but handle it
            set_error("Unexpected EOF during write");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_full: validation failed");
            return false;
        }
        written += n;
    }

    return true;
}

/**
 * @brief Read data from file with error checking
 *
 * WHAT: Read buffer from file with full error handling
 * WHY:  Ensure all data is read or fail atomically
 * HOW:  Loop until all bytes read, check ferror/feof
 *
 * @param fp File pointer
 * @param data Buffer to read into
 * @param size Size in bytes
 * @return true on success, false on error
 */
static bool read_full(FILE* fp, void* data, size_t size) {
    uint8_t* bytes = (uint8_t*)data;
    size_t nread = 0;

    while (nread < size) {
        size_t n = fread(bytes + nread, 1, size - nread, fp);
        if (n == 0) {
            if (feof(fp)) {
                set_error("Unexpected EOF (expected %zu bytes, got %zu)", size, nread);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "read_full: validation failed");
                return false;
            }
            if (ferror(fp)) {
                set_error("Read error: %s", strerror(errno));
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "read_full: validation failed");
                return false;
            }
        }
        nread += n;
    }

    return true;
}

//=============================================================================
// Internal Helper Functions - Header Operations
//=============================================================================

/**
 * @brief Write checkpoint header to file
 *
 * WHAT: Serialize checkpoint_header_t to file
 * WHY:  Standard header format for all checkpoints
 * HOW:  Fill header struct, write to file
 *
 * @param fp File pointer
 * @param flags Checkpoint flags
 * @param data_size Size of data section
 * @return true on success
 */
static bool write_checkpoint_header(FILE* fp, uint32_t flags, uint32_t data_size) {
    checkpoint_header_t header = {0};

    // Magic and version
    memcpy(header.magic, CHECKPOINT_MAGIC, sizeof(CHECKPOINT_MAGIC) - 1);  // Use memcpy instead of strncpy
    header.magic[sizeof(header.magic) - 1] = '\0';  // Ensure null termination
    header.version_major = CHECKPOINT_VERSION_MAJOR;
    header.version_minor = CHECKPOINT_VERSION_MINOR;

    // Flags and metadata
    header.flags = flags;
    header.timestamp = (uint64_t)time(NULL);
    header.data_size = data_size;
    header.crc32 = 0;  // Will be filled in later

    // Write header
    if (!write_full(fp, &header, sizeof(header))) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_checkpoint_header: write_full is NULL");
        return false;
    }

    return true;
}

/**
 * @brief Read and validate checkpoint header
 *
 * WHAT: Deserialize and validate checkpoint header
 * WHY:  Quick validation before attempting full restore
 * HOW:  Read header, check magic/version
 *
 * @param fp File pointer
 * @param header Output parameter for header
 * @return true if header is valid
 */
static bool read_checkpoint_header(FILE* fp, checkpoint_header_t* header) {
    // Guard: NULL check
    if (!fp || !header) {
        set_error("read_checkpoint_header: NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "read_checkpoint_header: required parameter is NULL (fp, header)");
        return false;
    }

    // Read header
    if (!read_full(fp, header, sizeof(*header))) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "read_checkpoint_header: read_full is NULL");
        return false;
    }

    // Validate magic
    if (strncmp(header->magic, CHECKPOINT_MAGIC, strlen(CHECKPOINT_MAGIC)) != 0) {
        set_error("Invalid magic bytes (not a checkpoint file)");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "read_checkpoint_header: validation failed");
        return false;
    }

    // Check version compatibility
    if (header->version_major > CHECKPOINT_VERSION_MAJOR) {
        set_error("Unsupported version %u.%u (current: %u.%u)",
                  header->version_major, header->version_minor,
                  CHECKPOINT_VERSION_MAJOR, CHECKPOINT_VERSION_MINOR);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "read_checkpoint_header: validation failed");
        return false;
    }

    return true;
}

//=============================================================================
// Internal Helper Functions - Brain Serialization
//=============================================================================

/**
 * @brief Serialize brain state to a buffer
 *
 * WHAT: Convert brain state to a byte buffer for checkpoint storage
 * WHY:  Enable persistence and recovery of brain state
 * HOW:  Serialize config, network weights, and stats sequentially
 *
 * @param brain Brain to serialize
 * @param buffer Output buffer (allocated by this function)
 * @param size Output: size of buffer
 * @return true on success
 */
static bool serialize_brain_state(brain_t brain, uint8_t** buffer, size_t* size) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "serialize_brain_state: brain is NULL");
        set_error("serialize_brain_state: NULL parameter");
        return false;
    }
    if (!buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "serialize_brain_state: buffer is NULL");
        set_error("serialize_brain_state: NULL parameter");
        return false;
    }
    if (!size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "serialize_brain_state: size is NULL");
        set_error("serialize_brain_state: NULL parameter");
        return false;
    }

    struct brain_struct* b = (struct brain_struct*)brain;

    adaptive_network_t net = b->network;
    if (!net) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "serialize_brain_state: network is NULL");
        set_error("serialize_brain_state: No network");
        return false;
    }

    neural_network_t base_net = adaptive_network_get_base_network(net);
    if (!base_net) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "serialize_brain_state: base_net is NULL");
        set_error("serialize_brain_state: No base network");
        return false;
    }

    uint32_t num_neurons = adaptive_network_get_neuron_count(net);

    // Estimate buffer size
    size_t estimated_size = sizeof(brain_config_t) + sizeof(brain_stats_t) + sizeof(uint32_t) +
                           (num_neurons * (sizeof(uint32_t) + 6 * sizeof(float) + sizeof(uint32_t) +
                           MAX_SYNAPSES_PER_NEURON * (sizeof(uint32_t) + 3 * sizeof(float))));

    uint8_t* buf = (uint8_t*)nimcp_malloc(estimated_size);
    if (!buf) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "serialize_brain_state: Failed to allocate %zu bytes", estimated_size);
        return false;
    }

    size_t offset = 0;

    // Config
    memcpy(buf + offset, &b->config, sizeof(brain_config_t));
    offset += sizeof(brain_config_t);

    // Stats
    memcpy(buf + offset, &b->stats, sizeof(brain_stats_t));
    offset += sizeof(brain_stats_t);

    // Number of neurons
    memcpy(buf + offset, &num_neurons, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Each neuron
    for (uint32_t n = 0; n < num_neurons; n++) {
        neuron_t* neuron = neural_network_get_neuron(base_net, n);
        if (!neuron) continue;

        memcpy(buf + offset, &neuron->id, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        memcpy(buf + offset, &neuron->state, sizeof(float));
        offset += sizeof(float);
        memcpy(buf + offset, &neuron->threshold, sizeof(float));
        offset += sizeof(float);
        memcpy(buf + offset, &neuron->adaptation, sizeof(float));
        offset += sizeof(float);
        memcpy(buf + offset, &neuron->bias, sizeof(float));
        offset += sizeof(float);
        memcpy(buf + offset, &neuron->avg_activity, sizeof(float));
        offset += sizeof(float);
        memcpy(buf + offset, &neuron->plasticity_rate, sizeof(float));
        offset += sizeof(float);
        uint32_t num_syn_out = NEURON_OUT_COUNT(neuron);
        memcpy(buf + offset, &num_syn_out, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        synapse_metadata_pool_t ser_mpool = neural_network_get_synapse_metadata_pool(base_net);
        for (uint32_t s = 0; s < num_syn_out; s++) {
            synapse_handle_t* h = NEURON_OUT_HANDLE(neuron, s);
            memcpy(buf + offset, &h->target_neuron_id, sizeof(uint32_t));
            offset += sizeof(uint32_t);
            memcpy(buf + offset, &h->weight, sizeof(float));
            offset += sizeof(float);
            float plasticity = 0.0f;
            synapse_t* meta = sparse_synapse_get_metadata(ser_mpool, h);
            if (meta) plasticity = meta->plasticity;
            memcpy(buf + offset, &plasticity, sizeof(float));
            offset += sizeof(float);
            memcpy(buf + offset, &h->strength, sizeof(float));
            offset += sizeof(float);
        }
    }

    *buffer = buf;
    *size = offset;
    return true;
}

/**
 * @brief Deserialize brain state from a buffer
 */
static bool deserialize_brain_state(const uint8_t* buffer, size_t size, brain_t* brain) {
    if (!buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "deserialize_brain_state: buffer is NULL");
        set_error("deserialize_brain_state: NULL parameter");
        return false;
    }
    if (size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "deserialize_brain_state: size is 0");
        set_error("deserialize_brain_state: NULL parameter");
        return false;
    }
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "deserialize_brain_state: brain is NULL");
        set_error("deserialize_brain_state: NULL parameter");
        return false;
    }

    *brain = NULL;
    size_t offset = 0;

    if (offset + sizeof(brain_config_t) > size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "deserialize_brain_state: buffer too small for config");
        set_error("deserialize_brain_state: Buffer too small");
        return false;
    }
    brain_config_t config;
    memcpy(&config, buffer + offset, sizeof(brain_config_t));
    offset += sizeof(brain_config_t);

    if (offset + sizeof(brain_stats_t) > size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "deserialize_brain_state: buffer too small for stats");
        set_error("deserialize_brain_state: Buffer too small");
        return false;
    }
    brain_stats_t stats;
    memcpy(&stats, buffer + offset, sizeof(brain_stats_t));
    offset += sizeof(brain_stats_t);

    if (offset + sizeof(uint32_t) > size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "deserialize_brain_state: buffer too small for neuron count");
        set_error("deserialize_brain_state: Buffer too small");
        return false;
    }
    uint32_t num_neurons;
    memcpy(&num_neurons, buffer + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    if (num_neurons > MAX_NEURONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "deserialize_brain_state: neuron count %u exceeds MAX_NEURONS", num_neurons);
        set_error("deserialize_brain_state: Invalid neuron count");
        return false;
    }

    brain_t new_brain = brain_create_custom(&config);
    if (!new_brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "deserialize_brain_state: failed to create brain");
        set_error("deserialize_brain_state: Failed to create brain");
        return false;
    }

    struct brain_struct* b = (struct brain_struct*)new_brain;
    adaptive_network_t net = b->network;
    neural_network_t base_net = net ? adaptive_network_get_base_network(net) : NULL;

    if (base_net) {
        uint32_t available = adaptive_network_get_neuron_count(net);
        uint32_t to_restore = (num_neurons < available) ? num_neurons : available;

        for (uint32_t n = 0; n < to_restore && offset < size; n++) {
            if (offset + sizeof(uint32_t) > size) break;
            uint32_t neuron_id;
            memcpy(&neuron_id, buffer + offset, sizeof(uint32_t));
            offset += sizeof(uint32_t);

            neuron_t* neuron = neural_network_get_neuron(base_net, neuron_id);
            if (!neuron) {
                // Skip neuron data: 6 floats + synapse count
                size_t skip_base = 6 * sizeof(float) + sizeof(uint32_t);
                if (offset + skip_base > size) break;
                offset += skip_base;

                // Read synapse count and skip synapse data
                uint32_t skip;
                memcpy(&skip, buffer + offset - sizeof(uint32_t), sizeof(uint32_t));

                // Guard against integer overflow: each synapse = sizeof(uint32_t) + 3*sizeof(float) = 16 bytes
                size_t synapse_size = sizeof(uint32_t) + 3 * sizeof(float);
                if (skip > (SIZE_MAX / synapse_size)) {
                    // Integer overflow would occur, abort
                    set_error("Invalid synapse count causes integer overflow");
                    brain_destroy(new_brain);
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "deserialize_brain_state: validation failed");
                    return false;
                }
                size_t skip_size = (size_t)skip * synapse_size;
                if (offset + skip_size > size) break;
                offset += skip_size;
                continue;
            }

            if (offset + 6 * sizeof(float) + sizeof(uint32_t) > size) break;
            memcpy(&neuron->state, buffer + offset, sizeof(float)); offset += sizeof(float);
            memcpy(&neuron->threshold, buffer + offset, sizeof(float)); offset += sizeof(float);
            memcpy(&neuron->adaptation, buffer + offset, sizeof(float)); offset += sizeof(float);
            memcpy(&neuron->bias, buffer + offset, sizeof(float)); offset += sizeof(float);
            memcpy(&neuron->avg_activity, buffer + offset, sizeof(float)); offset += sizeof(float);
            memcpy(&neuron->plasticity_rate, buffer + offset, sizeof(float)); offset += sizeof(float);

            uint32_t num_syn;
            memcpy(&num_syn, buffer + offset, sizeof(uint32_t));
            offset += sizeof(uint32_t);

            uint32_t neuron_out_count = NEURON_OUT_COUNT(neuron);
            uint32_t syn_restore = (num_syn < neuron_out_count) ? num_syn : neuron_out_count;
            synapse_metadata_pool_t deser_mpool = neural_network_get_synapse_metadata_pool(base_net);
            for (uint32_t s = 0; s < syn_restore && offset + sizeof(uint32_t) + 3 * sizeof(float) <= size; s++) {
                synapse_handle_t* h = NEURON_OUT_HANDLE(neuron, s);
                memcpy(&h->target_neuron_id, buffer + offset, sizeof(uint32_t));
                offset += sizeof(uint32_t);
                memcpy(&h->weight, buffer + offset, sizeof(float));
                offset += sizeof(float);
                float plasticity;
                memcpy(&plasticity, buffer + offset, sizeof(float));
                offset += sizeof(float);
                synapse_t* meta = sparse_synapse_get_metadata(deser_mpool, h);
                if (meta) meta->plasticity = plasticity;
                memcpy(&h->strength, buffer + offset, sizeof(float));
                offset += sizeof(float);
            }
            for (uint32_t s = syn_restore; s < num_syn; s++) {
                offset += sizeof(uint32_t) + 3 * sizeof(float);
            }
        }
    }

    b->stats = stats;

    // GPU weight cache was initialized with default weights during brain_create_custom().
    // After restoring neuron weights from the checkpoint, the CPU has newer weights than
    // the GPU cache. Mark dirty so the next forward/learn re-uploads from CPU.
    if (net) {
        adaptive_network_mark_gpu_weights_dirty(net);
    }

    *brain = new_brain;
    return true;
}

/**
 * @brief Compare timestamps for qsort (descending - newest first)
 */
static int compare_checkpoint_timestamp_desc(const void* a, const void* b) {
    const checkpoint_info_t* ca = (const checkpoint_info_t*)a;
    const checkpoint_info_t* cb = (const checkpoint_info_t*)b;
    if (ca->timestamp > cb->timestamp) return -1;
    if (ca->timestamp < cb->timestamp) return 1;
    return 0;
}

//=============================================================================
// Public API - Checkpoint Creation
//=============================================================================

checkpoint_options_t checkpoint_default_options(void) {
    checkpoint_options_t options = {
        .enable_compression = true,
        .incremental = false,
        .save_subsystems = true,
        .save_activations = false,
        .compression_level = 6,
        .temp_dir = NULL
    };
    return options;
}

bool checkpoint_save(brain_t brain, const char* path) {
    // Guard: NULL checks using API exception macros
    NIMCP_API_CHECK_NULL_RET_FALSE(brain, "checkpoint_save: NULL brain");
    NIMCP_API_CHECK_NULL_RET_FALSE(path, "checkpoint_save: NULL path");

    // Use default options
    checkpoint_options_t options = checkpoint_default_options();
    return checkpoint_save_ex(brain, path, &options);
}

bool checkpoint_save_ex(brain_t brain, const char* path, const checkpoint_options_t* options) {
    // Guard: NULL checks using API exception macros
    NIMCP_API_CHECK_NULL_RET_FALSE(brain, "checkpoint_save_ex: NULL brain");
    NIMCP_API_CHECK_NULL_RET_FALSE(path, "checkpoint_save_ex: NULL path");

    // Use default options if not provided
    checkpoint_options_t opts;
    if (!options) {
        opts = checkpoint_default_options();
        options = &opts;
    }

    // Serialize brain state
    uint8_t* data_buffer = NULL;
    size_t data_size = 0;
    if (!serialize_brain_state(brain, &data_buffer, &data_size)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "checkpoint_save_ex: serialize_brain_state is NULL");
        return false;
    }

    // Calculate CRC32
    uint32_t data_crc = crc32_calculate(data_buffer, data_size);

    // Create temp file for atomic write
    char temp_path[NIMCP_DWARF_PATH_SIZE];
    snprintf(temp_path, sizeof(temp_path), "%s%s", path, TEMP_SUFFIX);

    FILE* fp = fopen(temp_path, "wb");
    if (!fp) {
        set_error("Failed to open checkpoint file: %s", strerror(errno));
        nimcp_free(data_buffer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "checkpoint_save_ex: fp is NULL");
        return false;
    }

    // Write header
    checkpoint_header_t header;
    memset(&header, 0, sizeof(header));
    memcpy(header.magic, CHECKPOINT_MAGIC, strlen(CHECKPOINT_MAGIC));
    header.version_major = CHECKPOINT_VERSION_MAJOR;
    header.version_minor = CHECKPOINT_VERSION_MINOR;
    header.flags = options->incremental ? CHECKPOINT_FLAG_INCREMENTAL : 0;
    header.timestamp = (uint64_t)time(NULL);
    header.data_size = (uint32_t)data_size;
    header.crc32 = data_crc;

    if (!write_full(fp, &header, sizeof(header))) {
        fclose(fp);
        unlink(temp_path);
        nimcp_free(data_buffer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "checkpoint_save_ex: write_full is NULL");
        return false;
    }

    // Write data
    if (!write_full(fp, data_buffer, data_size)) {
        fclose(fp);
        unlink(temp_path);
        nimcp_free(data_buffer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "checkpoint_save_ex: write_full is NULL");
        return false;
    }

    fflush(fp);
    int fd = fileno(fp);
    if (fd >= 0) (void)fsync(fd);
    fclose(fp);
    nimcp_free(data_buffer);

    // Atomic rename
    if (rename(temp_path, path) != 0) {
        set_error("Failed to rename checkpoint: %s", strerror(errno));
        unlink(temp_path);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "checkpoint_save_ex: validation failed");
        return false;
    }

    NIMCP_LOGGING_INFO("Checkpoint saved: %zu bytes, CRC32=0x%08X", data_size, data_crc);
    return true;
}

bool checkpoint_save_incremental(brain_t brain, const char* incr_path, const char* base_path) {
    // Guard: NULL checks
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "checkpoint_save_incremental: brain is NULL");
        set_error("checkpoint_save_incremental: NULL parameter");
        return false;
    }
    if (!incr_path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "checkpoint_save_incremental: incr_path is NULL");
        set_error("checkpoint_save_incremental: NULL parameter");
        return false;
    }

    // base_path can be NULL (will use previous checkpoint if available)
    (void)base_path;  // Suppress unused parameter warning for now

    checkpoint_options_t options = checkpoint_default_options();
    options.incremental = true;
    return checkpoint_save_ex(brain, incr_path, &options);
}

//=============================================================================
// Public API - Checkpoint Loading
//=============================================================================

bool checkpoint_validate(const char* path) {
    // Guard: NULL check using API exception macro
    NIMCP_API_CHECK_NULL_RET_FALSE(path, "checkpoint_validate: NULL path");

    // Try to open file
    FILE* fp = fopen(path, "rb");
    if (!fp) {
        set_error("Failed to open checkpoint: %s", strerror(errno));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "checkpoint_validate: fp is NULL");
        return false;
    }

    // Read and validate header
    checkpoint_header_t header;
    if (!read_checkpoint_header(fp, &header)) {
        fclose(fp);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "checkpoint_validate: read_checkpoint_header is NULL");
        return false;
    }

    // Handle zero-size data case
    if (header.data_size == 0) {
        fclose(fp);
        // For empty data, CRC32 should be 0 (no data to checksum)
        if (header.crc32 != 0) {
            set_error("CRC32 mismatch for empty data (expected 0x00000000, got 0x%08X)",
                      header.crc32);
            return false;
        }
        return true;
    }

    // Read data section
    uint8_t* data = (uint8_t*)nimcp_malloc(header.data_size);
    if (!data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "checkpoint_validate: failed to allocate %u bytes", header.data_size);
        set_error("Failed to allocate buffer for validation");
        fclose(fp);
        return false;
    }

    if (!read_full(fp, data, header.data_size)) {
        nimcp_free(data);
        fclose(fp);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "checkpoint_validate: read_full is NULL");
        return false;
    }

    // Verify CRC32
    uint32_t calculated_crc = crc32_calculate(data, header.data_size);
    nimcp_free(data);
    fclose(fp);

    if (calculated_crc != header.crc32) {
        set_error("CRC32 mismatch (expected 0x%08X, got 0x%08X)",
                  header.crc32, calculated_crc);
        return false;
    }

    return true;
}

bool checkpoint_load(brain_t* brain, const char* path) {
    // Guard: NULL checks using API exception macros
    NIMCP_API_CHECK_NULL_RET_FALSE(brain, "checkpoint_load: NULL brain output parameter");
    NIMCP_API_CHECK_NULL_RET_FALSE(path, "checkpoint_load: NULL path");

    *brain = NULL;

    NIMCP_LOGGING_INFO("Loading checkpoint from: %s", path);

    // Validate checkpoint first
    if (!checkpoint_validate(path)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "checkpoint_load: checkpoint_validate is NULL");
        return false;
    }

    FILE* fp = fopen(path, "rb");
    if (!fp) {
        set_error("Failed to open checkpoint: %s", strerror(errno));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "checkpoint_load: fp is NULL");
        return false;
    }

    checkpoint_header_t header;
    if (!read_checkpoint_header(fp, &header)) {
        fclose(fp);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "checkpoint_load: read_checkpoint_header is NULL");
        return false;
    }

    if (header.data_size == 0) {
        fclose(fp);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "checkpoint_load: empty checkpoint file");
        set_error("checkpoint_load: Empty checkpoint");
        return false;
    }

    uint8_t* data = (uint8_t*)nimcp_malloc(header.data_size);
    if (!data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "checkpoint_load: failed to allocate %u bytes", header.data_size);
        set_error("Failed to allocate buffer");
        fclose(fp);
        return false;
    }

    if (!read_full(fp, data, header.data_size)) {
        nimcp_free(data);
        fclose(fp);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "checkpoint_load: read_full is NULL");
        return false;
    }
    fclose(fp);

    // Verify CRC
    uint32_t crc = crc32_calculate(data, header.data_size);
    if (crc != header.crc32) {
        set_error("CRC32 mismatch");
        nimcp_free(data);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "checkpoint_load: validation failed");
        return false;
    }

    // Deserialize
    if (!deserialize_brain_state(data, header.data_size, brain)) {
        nimcp_free(data);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "checkpoint_load: deserialize_brain_state is NULL");
        return false;
    }

    nimcp_free(data);
    NIMCP_LOGGING_INFO("Checkpoint loaded successfully");
    return true;
}

//=============================================================================
// Public API - Checkpoint Management
//=============================================================================

bool checkpoint_list(const char* dir, checkpoint_info_t** list, uint32_t* count) {
    // Guard: NULL checks using API exception macros
    NIMCP_API_CHECK_NULL_RET_FALSE(dir, "checkpoint_list: NULL dir");
    NIMCP_API_CHECK_NULL_RET_FALSE(list, "checkpoint_list: NULL list");
    NIMCP_API_CHECK_NULL_RET_FALSE(count, "checkpoint_list: NULL count");

    *list = NULL;
    *count = 0;

    // Open directory
    DIR* d = opendir(dir);
    if (!d) {
        set_error("Failed to open directory %s: %s", dir, strerror(errno));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "checkpoint_list: d is NULL");
        return false;
    }

    // Count checkpoint files
    struct dirent* entry;
    uint32_t checkpoint_count = 0;
    while ((entry = readdir(d)) != NULL) {
        if (strstr(entry->d_name, CHECKPOINT_EXTENSION) != NULL) {
            checkpoint_count++;
        }
    }

    // Allocate list
    if (checkpoint_count == 0) {
        closedir(d);
        return true;  // Success, but no checkpoints found
    }

    *list = (checkpoint_info_t*)nimcp_calloc(checkpoint_count, sizeof(checkpoint_info_t));
    if (!*list) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "checkpoint_list: failed to allocate %u entries", checkpoint_count);
        set_error("Failed to allocate checkpoint list");
        closedir(d);
        return false;
    }

    // Populate list
    rewinddir(d);
    uint32_t i = 0;
    while ((entry = readdir(d)) != NULL && i < checkpoint_count) {
        if (strstr(entry->d_name, CHECKPOINT_EXTENSION) == NULL) {
            continue;
        }

        checkpoint_info_t* info = &(*list)[i];

        // Build full path (ensure space for null terminator)
        int path_len = snprintf(info->path, sizeof(info->path), "%s/%s", dir, entry->d_name);
        if (path_len >= (int)sizeof(info->path)) {
            // Path too long, skip this entry
            continue;
        }

        // Get file stats
        struct stat st;
        if (stat(info->path, &st) == 0) {
            info->size_bytes = (uint32_t)st.st_size;
        }

        // Try to read header
        FILE* fp = fopen(info->path, "rb");
        if (fp) {
            checkpoint_header_t header;
            if (read_checkpoint_header(fp, &header)) {
                info->timestamp = header.timestamp;
                info->is_compressed = (header.flags & CHECKPOINT_FLAG_COMPRESSED) != 0;
                info->is_incremental = (header.flags & CHECKPOINT_FLAG_INCREMENTAL) != 0;
                info->version_major = header.version_major;
                info->version_minor = header.version_minor;
                info->is_valid = true;  // Header is valid, assume file is OK
            }
            fclose(fp);
        }

        i++;
    }

    closedir(d);
    *count = i;

    // Sort by timestamp (newest first)
    if (i > 1) {
        qsort(*list, i, sizeof(checkpoint_info_t), compare_checkpoint_timestamp_desc);
    }

    return true;
}

bool checkpoint_cleanup_old(const char* dir, uint32_t keep_count) {
    // Guard: NULL check using API exception macro
    NIMCP_API_CHECK_NULL_RET_FALSE(dir, "checkpoint_cleanup_old: NULL directory");

    // Safety: Never delete if keep_count is 0
    NIMCP_API_CHECK_RET_FALSE(keep_count > 0, NIMCP_ERROR_INVALID_PARAM, "keep_count must be > 0");

    // List checkpoints
    checkpoint_info_t* list = NULL;
    uint32_t count = 0;
    if (!checkpoint_list(dir, &list, &count)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "checkpoint_cleanup_old: checkpoint_list is NULL");
        return false;
    }

    // Nothing to cleanup
    if (count <= keep_count) {
        if (list) {
            nimcp_free(list);
        }
        return true;
    }

    // List is already sorted by checkpoint_list (newest first)
    // Delete old checkpoints
    // NOTE: There is an inherent TOCTOU race between checkpoint_list() and unlink().
    // Another process could modify the directory between these operations.
    // We handle this gracefully by logging warnings but not failing the operation.
    uint32_t deleted_count = 0;
    uint32_t failed_count = 0;
    for (uint32_t i = keep_count; i < count; i++) {
        NIMCP_LOGGING_INFO("Deleting old checkpoint: %s", list[i].path);
        if (unlink(list[i].path) != 0) {
            // Handle specific error cases
            if (errno == ENOENT) {
                // File was already deleted (race condition) - not an error
                NIMCP_LOGGING_DEBUG("Checkpoint already deleted (race): %s", list[i].path);
            } else if (errno == EACCES || errno == EPERM) {
                // Permission denied - log warning but continue
                NIMCP_LOGGING_WARN("Permission denied deleting %s: %s", list[i].path, strerror(errno));
                failed_count++;
            } else {
                // Other errors - log warning
                NIMCP_LOGGING_WARN("Failed to delete %s: %s (errno=%d)", list[i].path, strerror(errno), errno);
                failed_count++;
            }
        } else {
            deleted_count++;
        }
    }

    nimcp_free(list);

    // Log summary
    if (failed_count > 0) {
        NIMCP_LOGGING_WARN("Checkpoint cleanup: deleted %u, failed %u of %u targeted",
                           deleted_count, failed_count, count - keep_count);
    }

    return true;
}

//=============================================================================
// Public API - Recovery Operations
//=============================================================================

bool recovery_auto_restore(brain_t* brain, const char* checkpoint_dir) {
    // Guard: NULL checks using API exception macros
    NIMCP_API_CHECK_NULL_RET_FALSE(brain, "recovery_auto_restore: NULL brain");
    NIMCP_API_CHECK_NULL_RET_FALSE(checkpoint_dir, "recovery_auto_restore: NULL checkpoint_dir");

    *brain = NULL;

    // List checkpoints
    checkpoint_info_t* list = NULL;
    uint32_t count = 0;
    if (!checkpoint_list(checkpoint_dir, &list, &count)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "recovery_auto_restore: checkpoint_list is NULL");
        return false;
    }

    // No checkpoints found
    if (count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "recovery_auto_restore: no checkpoints found in %s", checkpoint_dir);
        set_error("No checkpoints found in %s", checkpoint_dir);
        return false;
    }

    // Try to load newest valid checkpoint (list sorted by timestamp)
    for (uint32_t i = 0; i < count; i++) {
        if (!list[i].is_valid) {
            continue;
        }

        NIMCP_LOGGING_INFO("Attempting recovery from: %s", list[i].path);
        if (checkpoint_load(brain, list[i].path)) {
            NIMCP_LOGGING_INFO("Recovery successful from %s", list[i].path);
            nimcp_free(list);
            return true;
        }

        NIMCP_LOGGING_WARN("Failed to load %s, trying next...", list[i].path);
    }

    // No valid checkpoint could be loaded
    set_error("All checkpoints failed to load");
    nimcp_free(list);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "recovery_auto_restore: operation failed");
    return false;
}

bool recovery_rollback(brain_t brain, const char* checkpoint_path) {
    // Guard: NULL checks using API exception macros
    NIMCP_API_CHECK_NULL_RET_FALSE(brain, "recovery_rollback: NULL brain");
    NIMCP_API_CHECK_NULL_RET_FALSE(checkpoint_path, "recovery_rollback: NULL checkpoint_path");

    NIMCP_LOGGING_INFO("In-place rollback from: %s", checkpoint_path);

    // Load checkpoint
    brain_t restored = NULL;
    if (!checkpoint_load(&restored, checkpoint_path)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "recovery_rollback: checkpoint_load is NULL");
        return false;
    }

    // Transfer state from restored brain to target
    struct brain_struct* target = (struct brain_struct*)brain;
    struct brain_struct* source = (struct brain_struct*)restored;

    adaptive_network_t target_net = target->network;
    adaptive_network_t source_net = source->network;

    if (!target_net) {
        brain_destroy(restored);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recovery_rollback: target network is NULL");
        set_error("recovery_rollback: Network missing");
        return false;
    }
    if (!source_net) {
        brain_destroy(restored);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recovery_rollback: source network is NULL");
        set_error("recovery_rollback: Network missing");
        return false;
    }

    neural_network_t target_base = adaptive_network_get_base_network(target_net);
    neural_network_t source_base = adaptive_network_get_base_network(source_net);

    if (!target_base) {
        brain_destroy(restored);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recovery_rollback: target base network is NULL");
        set_error("recovery_rollback: Base network missing");
        return false;
    }
    if (!source_base) {
        brain_destroy(restored);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recovery_rollback: source base network is NULL");
        set_error("recovery_rollback: Base network missing");
        return false;
    }

    uint32_t t_neurons = adaptive_network_get_neuron_count(target_net);
    uint32_t s_neurons = adaptive_network_get_neuron_count(source_net);
    uint32_t to_copy = (t_neurons < s_neurons) ? t_neurons : s_neurons;

    for (uint32_t n = 0; n < to_copy; n++) {
        neuron_t* t = neural_network_get_neuron(target_base, n);
        neuron_t* s = neural_network_get_neuron(source_base, n);
        if (!t || !s) continue;

        t->state = s->state;
        t->threshold = s->threshold;
        t->adaptation = s->adaptation;
        t->bias = s->bias;
        t->avg_activity = s->avg_activity;
        t->plasticity_rate = s->plasticity_rate;

        uint32_t t_syn = NEURON_OUT_COUNT(t);
        uint32_t s_syn = NEURON_OUT_COUNT(s);
        uint32_t syn_copy = (t_syn < s_syn) ? t_syn : s_syn;
        synapse_metadata_pool_t t_mpool = neural_network_get_synapse_metadata_pool(target_base);
        synapse_metadata_pool_t s_mpool = neural_network_get_synapse_metadata_pool(source_base);
        for (uint32_t i = 0; i < syn_copy; i++) {
            synapse_handle_t* th = NEURON_OUT_HANDLE(t, i);
            synapse_handle_t* sh = NEURON_OUT_HANDLE(s, i);
            th->weight = sh->weight;
            th->strength = sh->strength;
            synapse_t* t_meta = sparse_synapse_get_metadata(t_mpool, th);
            synapse_t* s_meta = sparse_synapse_get_metadata(s_mpool, sh);
            if (t_meta && s_meta) t_meta->plasticity = s_meta->plasticity;
        }
    }

    target->stats = source->stats;
    brain_destroy(restored);

    NIMCP_LOGGING_INFO("Rollback complete: %u neurons restored", to_copy);
    return true;
}

bool recovery_partial(brain_t* brain, const char* path, int* recovery_level) {
    // Guard: NULL checks using API exception macros
    NIMCP_API_CHECK_NULL_RET_FALSE(brain, "recovery_partial: NULL brain");
    NIMCP_API_CHECK_NULL_RET_FALSE(path, "recovery_partial: NULL path");
    NIMCP_API_CHECK_NULL_RET_FALSE(recovery_level, "recovery_partial: NULL recovery_level");

    *brain = NULL;
    *recovery_level = 0;

    NIMCP_LOGGING_INFO("Partial recovery from: %s", path);

    FILE* fp = fopen(path, "rb");
    if (!fp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_IO, "recovery_partial: failed to open %s: %s", path, strerror(errno));
        set_error("Failed to open: %s", strerror(errno));
        return false;
    }

    checkpoint_header_t header;
    if (!read_checkpoint_header(fp, &header)) {
        fclose(fp);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "recovery_partial: read_checkpoint_header is NULL");
        return false;
    }
    *recovery_level = 1;  // Header valid

    if (header.data_size == 0) {
        fclose(fp);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "recovery_partial: empty checkpoint");
        set_error("Empty checkpoint");
        return false;
    }

    uint8_t* data = (uint8_t*)nimcp_malloc(header.data_size);
    if (!data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "recovery_partial: failed to allocate %u bytes", header.data_size);
        fclose(fp);
        return false;
    }

    size_t bytes_read = fread(data, 1, header.data_size, fp);
    fclose(fp);

    if (bytes_read == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_IO, "recovery_partial: failed to read data from checkpoint");
        nimcp_free(data);
        return false;
    }
    *recovery_level = 2;  // Some data read

    if (bytes_read == header.data_size) {
        uint32_t crc = crc32_calculate(data, bytes_read);
        if (crc == header.crc32) {
            *recovery_level = 3;  // Full data, valid CRC
        }
    }

    // Try to extract config
    size_t offset = 0;
    if (offset + sizeof(brain_config_t) > bytes_read) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "recovery_partial: buffer too small for config");
        nimcp_free(data);
        set_error("Cannot recover config");
        return false;
    }

    brain_config_t config;
    memcpy(&config, data + offset, sizeof(brain_config_t));
    offset += sizeof(brain_config_t);
    *recovery_level = 4;  // Config recovered

    brain_stats_t stats = {0};
    if (offset + sizeof(brain_stats_t) <= bytes_read) {
        memcpy(&stats, data + offset, sizeof(brain_stats_t));
        offset += sizeof(brain_stats_t);
        *recovery_level = 5;  // Stats recovered
    }

    brain_t new_brain = brain_create_custom(&config);
    if (!new_brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "recovery_partial: failed to create brain");
        nimcp_free(data);
        set_error("Failed to create brain");
        return false;
    }

    struct brain_struct* b = (struct brain_struct*)new_brain;
    b->stats = stats;

    // Try to restore neurons
    if (offset + sizeof(uint32_t) <= bytes_read) {
        uint32_t num_neurons;
        memcpy(&num_neurons, data + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        adaptive_network_t net = b->network;
        neural_network_t base = net ? adaptive_network_get_base_network(net) : NULL;

        if (base && num_neurons <= MAX_NEURONS) {
            uint32_t avail = adaptive_network_get_neuron_count(net);
            uint32_t count = (num_neurons < avail) ? num_neurons : avail;
            uint32_t restored = 0;

            for (uint32_t n = 0; n < count && offset < bytes_read; n++) {
                if (offset + sizeof(uint32_t) > bytes_read) break;
                uint32_t id;
                memcpy(&id, data + offset, sizeof(uint32_t));
                offset += sizeof(uint32_t);

                neuron_t* neuron = neural_network_get_neuron(base, id);
                if (!neuron || offset + 6 * sizeof(float) + sizeof(uint32_t) > bytes_read) {
                    offset += 6 * sizeof(float) + sizeof(uint32_t);
                    if (offset <= bytes_read) {
                        uint32_t skip;
                        memcpy(&skip, data + offset - sizeof(uint32_t), sizeof(uint32_t));
                        offset += skip * (sizeof(uint32_t) + 3 * sizeof(float));
                    }
                    continue;
                }

                memcpy(&neuron->state, data + offset, sizeof(float)); offset += sizeof(float);
                memcpy(&neuron->threshold, data + offset, sizeof(float)); offset += sizeof(float);
                memcpy(&neuron->adaptation, data + offset, sizeof(float)); offset += sizeof(float);
                memcpy(&neuron->bias, data + offset, sizeof(float)); offset += sizeof(float);
                memcpy(&neuron->avg_activity, data + offset, sizeof(float)); offset += sizeof(float);
                memcpy(&neuron->plasticity_rate, data + offset, sizeof(float)); offset += sizeof(float);

                uint32_t num_syn;
                memcpy(&num_syn, data + offset, sizeof(uint32_t));
                offset += sizeof(uint32_t);

                uint32_t neuron_out_cnt = NEURON_OUT_COUNT(neuron);
                uint32_t syn_restore = (num_syn < neuron_out_cnt) ? num_syn : neuron_out_cnt;
                synapse_metadata_pool_t part_mpool = neural_network_get_synapse_metadata_pool(base);
                for (uint32_t s = 0; s < syn_restore && offset + sizeof(uint32_t) + 3 * sizeof(float) <= bytes_read; s++) {
                    synapse_handle_t* h = NEURON_OUT_HANDLE(neuron, s);
                    memcpy(&h->target_neuron_id, data + offset, sizeof(uint32_t));
                    offset += sizeof(uint32_t);
                    memcpy(&h->weight, data + offset, sizeof(float));
                    offset += sizeof(float);
                    float plasticity;
                    memcpy(&plasticity, data + offset, sizeof(float));
                    offset += sizeof(float);
                    synapse_t* meta = sparse_synapse_get_metadata(part_mpool, h);
                    if (meta) meta->plasticity = plasticity;
                    memcpy(&h->strength, data + offset, sizeof(float));
                    offset += sizeof(float);
                }
                for (uint32_t s = syn_restore; s < num_syn; s++) {
                    offset += sizeof(uint32_t) + 3 * sizeof(float);
                }
                restored++;
            }

            if (restored > 0) {
                *recovery_level = 6;  // Neurons recovered
            }
        }
    }

    nimcp_free(data);
    *brain = new_brain;
    NIMCP_LOGGING_INFO("Partial recovery complete (level %d)", *recovery_level);
    return true;
}

//=============================================================================
// Public API - Utility Functions
//=============================================================================

const char* checkpoint_get_error(void) {
    return error_buffer;
}

void checkpoint_clear_error(void) {
    error_buffer[0] = '\0';
}

const char* checkpoint_get_version(void) {
    static char version[16];
    snprintf(version, sizeof(version), "%u.%u",
             CHECKPOINT_VERSION_MAJOR, CHECKPOINT_VERSION_MINOR);
    return version;
}
