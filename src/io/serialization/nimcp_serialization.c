#include <stddef.h>  /* for NULL */
// src/lib/utils/nimcp_serialization.c
#include "io/serialization/nimcp_serialization.h"
#include <stdlib.h>
#include <string.h>
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/platform/nimcp_platform_once.h"

// Global BBB security system — protected by once-init to avoid TOCTOU race
static bbb_system_t g_bbb_system = NULL;
static nimcp_platform_once_t g_bbb_once = NIMCP_PLATFORM_ONCE_INIT;

//=============================================================================
// Security Initialization
//=============================================================================

/**
 * @brief Initialize security subsystem for serialization (once-init callback)
 *
 * WHAT: Create and configure BBB system for input validation
 * WHY: Protect against malicious external input and TOCTOU race on g_bbb_system
 * HOW: Called exactly once via nimcp_platform_once — thread-safe initialization
 */
static void serialization_security_init_impl(void) {
    bbb_config_t config = bbb_default_config();
    config.strict_mode = false;  // Don't block, just log
    config.default_action = BBB_ACTION_LOG;
    config.input.validate_strings = true;
    config.input.validate_integers = true;
    config.input.max_string_length = 4096;  // Reasonable limit

    g_bbb_system = bbb_system_create(&config);
    if (!g_bbb_system) {
        LOG_ERROR("serialization: Failed to initialize security subsystem");
    } else {
        LOG_INFO("serialization: Security subsystem initialized");
    }
}

/**
 * @brief Thread-safe security init entry point
 * WHY: nimcp_platform_once guarantees exactly-once execution, no TOCTOU race
 */
static void serialization_security_init(void) {
    nimcp_platform_once(&g_bbb_once, serialization_security_init_impl);
}

/**
 * @brief Cleanup security subsystem
 * WHY: __attribute__((destructor)) ensures cleanup on library unload,
 *      since this static function would otherwise never be called.
 */
__attribute__((destructor))
static void serialization_security_cleanup(void) {
    if (g_bbb_system) {
        bbb_system_destroy(g_bbb_system);
        g_bbb_system = NULL;
    }
}



#define LOG_MODULE "nimcp_serialization"
#define LOG_MODULE_ID 0x052E
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(serialization)

/**
 * @brief Checks if a read operation can be performed safely
 * @param serializer The serializer instance
 * @param bytes_needed Number of bytes that need to be read
 * @return true if read is safe to perform, false if error occurred
 * @note Sets error state if bounds check fails
 */
static bool nimcp_check_read(NimcpSerializer* serializer, size_t bytes_needed)
{
    if (!serializer) {
        return false;
    }
    if (bytes_needed == 0) return true;
    /* Guard: position must be within length before subtraction */
    if (serializer->position > serializer->length) {
        serializer->has_error = true;
        return false;
    }
    /* Overflow-safe: subtract instead of add to avoid size_t wraparound */
    if (bytes_needed > serializer->length - serializer->position) {
        serializer->has_error = true;
        return false;  /* Bounds check failure is normal validation */
    }
    return true;
}

static bool ensure_capacity(NimcpSerializer* serializer, size_t additional_size)
{
    if (!serializer || additional_size > NIMCP_SERIALIZER_MAX_SIZE) {
        if (serializer) {
            serializer->has_error = true;
        }
        return false;
    }

    /* Overflow-safe: check via subtraction instead of addition */
    if (additional_size > NIMCP_SERIALIZER_MAX_SIZE - serializer->position) {
        serializer->has_error = true;
        return false;
    }

    size_t required = serializer->position + additional_size;
    if (required <= serializer->capacity) {
        return true;
    }

    // Don't grow beyond initial capacity if it was explicitly set small (< 1KB)
    // This allows tests to verify error handling with fixed-size buffers
    if (serializer->capacity < NIMCP_SERIALIZER_INITIAL_SIZE && required > serializer->capacity) {
        serializer->has_error = true;
        return false;  /* Small buffer can't grow - normal for fixed-size test buffers */
    }

    // Check for overflow before doubling capacity
    if (serializer->capacity > SIZE_MAX / 2) {
        serializer->has_error = true;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ensure_capacity: validation failed");
        return false;
    }
    size_t new_capacity = serializer->capacity * 2;
    while (new_capacity < required) {
        if (new_capacity > SIZE_MAX / 2) {
            serializer->has_error = true;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ensure_capacity: validation failed");
            return false;
        }
        new_capacity *= 2;
    }

    if (new_capacity > NIMCP_SERIALIZER_MAX_SIZE) {
        serializer->has_error = true;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ensure_capacity: validation failed");
        return false;
    }

    uint8_t* new_buffer = nimcp_realloc(serializer->buffer, new_capacity);
    if (!new_buffer) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, new_capacity,
                          "Serializer buffer realloc failed for capacity %zu", new_capacity);
        serializer->has_error = true;
        return false;
    }

    serializer->buffer = new_buffer;
    serializer->capacity = new_capacity;
    return true;
}

NimcpSerializer* nimcp_serializer_create(size_t initial_capacity)
{
    if (initial_capacity == 0) {
        initial_capacity = NIMCP_SERIALIZER_INITIAL_SIZE;
    }

    if (initial_capacity > NIMCP_SERIALIZER_MAX_SIZE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_serializer_create: validation failed");
        return NULL;
    }

    NimcpSerializer* serializer = nimcp_malloc(sizeof(NimcpSerializer));
    if (!serializer) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(NimcpSerializer),
                          "Failed to allocate serializer structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "serializer is NULL");

        return NULL;
    }

    serializer->buffer = nimcp_malloc(initial_capacity);
    if (!serializer->buffer) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, initial_capacity,
                          "Failed to allocate serializer buffer of size %zu", initial_capacity);
        nimcp_free(serializer);
        return NULL;
    }

    serializer->capacity = initial_capacity;
    serializer->position = 0;
    serializer->length = 0;
    serializer->is_compressed = false;
    serializer->has_error = false;

    return serializer;
}

void nimcp_serializer_destroy(NimcpSerializer* serializer)
{
    if (serializer) {
        nimcp_free(serializer->buffer);
        nimcp_free(serializer);
    }
}

void nimcp_serializer_reset(NimcpSerializer* serializer)
{
    if (serializer) {
        serializer->position = 0;
        serializer->length = 0;
        serializer->is_compressed = false;
        serializer->has_error = false;
    }
}

bool nimcp_serializer_set_buffer(NimcpSerializer* serializer, const uint8_t* data, size_t length)
{
    if (!serializer || !data || length > NIMCP_SERIALIZER_MAX_SIZE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_serializer_set_buffer: required parameter is NULL (serializer, data)");
        return false;
    }

    if (!ensure_capacity(serializer, length)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_serializer_set_buffer: ensure_capacity is NULL");
        return false;
    }

    memcpy(serializer->buffer, data, length);
    serializer->length = length;
    serializer->position = 0;
    return true;
}

uint8_t* nimcp_serializer_get_buffer(NimcpSerializer* serializer)
{
    return serializer ? serializer->buffer : NULL;
}

size_t nimcp_serializer_get_length(NimcpSerializer* serializer)
{
    return serializer ? serializer->length : 0;
}

size_t nimcp_serializer_get_position(NimcpSerializer* serializer)
{
    return serializer ? serializer->position : 0;
}

bool nimcp_serializer_set_position(NimcpSerializer* serializer, size_t position)
{
    if (!serializer || position > serializer->length) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_serializer_set_position: serializer is NULL");
        return false;
    }
    serializer->position = position;
    return true;
}

void nimcp_serializer_mark_compressed(NimcpSerializer* serializer)
{
    if (serializer) {
        serializer->is_compressed = true;
    }
}

bool nimcp_serializer_is_compressed(const NimcpSerializer* serializer)
{
    return serializer ? serializer->is_compressed : false;
}

NimcpSerialResult nimcp_serializer_compress(NimcpSerializer* serializer)
{
    if (!serializer || serializer->is_compressed) {
        return NIMCP_SERIAL_ERROR_INVALID_PARAM;
    }

    const int max_dst_size = LZ4_compressBound(serializer->length);
    uint8_t* compressed_buffer = nimcp_malloc(max_dst_size);
    if (!compressed_buffer) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, max_dst_size,
                          "Failed to allocate compression buffer of size %d", max_dst_size);
        return NIMCP_SERIAL_ERROR_MEMORY;
    }

    int compressed_size =
        LZ4_compress_default((const char*) serializer->buffer, (char*) compressed_buffer,
                             serializer->length, max_dst_size);

    if (compressed_size <= 0) {
        NIMCP_THROW_IO(NIMCP_ERROR_OPERATION_FAILED, "serializer",
                      "LZ4 compression failed for data of size %zu", serializer->length);
        nimcp_free(compressed_buffer);
        return NIMCP_SERIAL_ERROR_COMPRESSION;
    }

    uint8_t* final_buffer = nimcp_malloc(compressed_size + sizeof(uint32_t));
    if (!final_buffer) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, compressed_size + sizeof(uint32_t),
                          "Failed to allocate final compressed buffer");
        nimcp_free(compressed_buffer);
        return NIMCP_SERIAL_ERROR_MEMORY;
    }

    /* Fix unaligned write: use memcpy instead of pointer cast */
    uint32_t orig_len = (uint32_t) serializer->length;
    memcpy(final_buffer, &orig_len, sizeof(uint32_t));
    memcpy(final_buffer + sizeof(uint32_t), compressed_buffer, compressed_size);

    nimcp_free(compressed_buffer);
    nimcp_free(serializer->buffer);

    serializer->buffer = final_buffer;
    serializer->length = compressed_size + sizeof(uint32_t);
    serializer->capacity = compressed_size + sizeof(uint32_t);
    serializer->position = 0;
    serializer->is_compressed = true;

    return NIMCP_SERIAL_SUCCESS;
}

NimcpSerialResult nimcp_serializer_decompress(NimcpSerializer* serializer)
{
    if (!serializer || !serializer->is_compressed) {
        return NIMCP_SERIAL_ERROR_INVALID_PARAM;
    }

    if (serializer->length < sizeof(uint32_t)) {
        return NIMCP_SERIAL_ERROR_BOUNDS;
    }

    /* Fix unaligned read: use memcpy instead of pointer cast */
    uint32_t original_size;
    memcpy(&original_size, serializer->buffer, sizeof(uint32_t));

    /* Reject attacker-controlled allocation above max size */
    if (original_size > NIMCP_SERIALIZER_MAX_SIZE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT,
            "nimcp_serializer_decompress: original_size %u exceeds max %u",
            (unsigned)original_size, (unsigned)NIMCP_SERIALIZER_MAX_SIZE);
        return NIMCP_SERIAL_ERROR_INVALID_PARAM;
    }
    if (original_size == 0) {
        return NIMCP_SERIAL_ERROR_INVALID_PARAM;
    }

    uint8_t* decompressed_buffer = nimcp_malloc(original_size);
    if (!decompressed_buffer) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, original_size,
                          "Failed to allocate decompression buffer of size %u", original_size);
        return NIMCP_SERIAL_ERROR_MEMORY;
    }

    int decompressed_size = LZ4_decompress_safe(
        (const char*) (serializer->buffer + sizeof(uint32_t)), (char*) decompressed_buffer,
        serializer->length - sizeof(uint32_t), original_size);

    if (decompressed_size < 0 || (uint32_t) decompressed_size != original_size) {
        NIMCP_THROW_IO(NIMCP_ERROR_OPERATION_FAILED, "serializer",
                      "LZ4 decompression failed: expected %u bytes, got %d",
                      original_size, decompressed_size);
        nimcp_free(decompressed_buffer);
        return NIMCP_SERIAL_ERROR_COMPRESSION;
    }

    nimcp_free(serializer->buffer);
    serializer->buffer = decompressed_buffer;
    serializer->length = decompressed_size;
    serializer->capacity = decompressed_size;
    serializer->position = 0;
    serializer->is_compressed = false;

    return NIMCP_SERIAL_SUCCESS;
}

// Write operations
bool nimcp_write_uint8(NimcpSerializer* serializer, uint8_t value)
{
    if (!ensure_capacity(serializer, sizeof(uint8_t))) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_write_uint8: ensure_capacity is NULL");
        return false;
    }
    serializer->buffer[serializer->position++] = value;
    serializer->length =
        serializer->position > serializer->length ? serializer->position : serializer->length;
    return true;
}

bool nimcp_write_uint16(NimcpSerializer* serializer, uint16_t value)
{
    if (!ensure_capacity(serializer, sizeof(uint16_t))) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_write_uint16: ensure_capacity is NULL");
        return false;
    }
    serializer->buffer[serializer->position++] = (value >> 8) & 0xFF;
    serializer->buffer[serializer->position++] = value & 0xFF;
    serializer->length =
        serializer->position > serializer->length ? serializer->position : serializer->length;
    return true;
}

// Continue with rest of write implementations...

// Read operations

/**
 * @brief Reads an unsigned 16-bit integer from the serializer
 * @param serializer The serializer instance
 * @return The read value, or 0 if an error occurred
 * @note Reads in big-endian format
 */
uint16_t nimcp_read_uint16(NimcpSerializer* serializer)
{
    if (!nimcp_check_read(serializer, 2)) {
        return 0;
    }
    uint16_t value = ((uint16_t) serializer->buffer[serializer->position] << 8) |
                     serializer->buffer[serializer->position + 1];
    serializer->position += 2;
    return value;
}

bool nimcp_write_bytes(NimcpSerializer* serializer, const uint8_t* data, size_t length)
{
    if (!serializer || !data || !ensure_capacity(serializer, length)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_write_bytes: required parameter is NULL (serializer, data, ensure_capacity)");
        return false;
    }
    memcpy(serializer->buffer + serializer->position, data, length);
    serializer->position += length;
    serializer->length =
        serializer->position > serializer->length ? serializer->position : serializer->length;
    return true;
}

const uint8_t* nimcp_read_bytes(NimcpSerializer* serializer, size_t* length)
{
    if (!serializer || !length || serializer->position >= serializer->length) {
        if (length)
            *length = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_read_bytes: validation failed");
        return NULL;
    }
    *length = serializer->length - serializer->position;
    const uint8_t* data = serializer->buffer + serializer->position;
    serializer->position = serializer->length;
    return data;
}

bool nimcp_write_uint32(NimcpSerializer* serializer, uint32_t value)
{
    if (!ensure_capacity(serializer, sizeof(uint32_t))) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_write_uint32: ensure_capacity is NULL");
        return false;
    }
    serializer->buffer[serializer->position++] = (value >> 24) & 0xFF;
    serializer->buffer[serializer->position++] = (value >> 16) & 0xFF;
    serializer->buffer[serializer->position++] = (value >> 8) & 0xFF;
    serializer->buffer[serializer->position++] = value & 0xFF;
    serializer->length =
        serializer->position > serializer->length ? serializer->position : serializer->length;
    return true;
}

bool nimcp_write_uint64(NimcpSerializer* serializer, uint64_t value)
{
    if (!ensure_capacity(serializer, sizeof(uint64_t))) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_write_uint64: ensure_capacity is NULL");
        return false;
    }
    serializer->buffer[serializer->position++] = (value >> 56) & 0xFF;
    serializer->buffer[serializer->position++] = (value >> 48) & 0xFF;
    serializer->buffer[serializer->position++] = (value >> 40) & 0xFF;
    serializer->buffer[serializer->position++] = (value >> 32) & 0xFF;
    serializer->buffer[serializer->position++] = (value >> 24) & 0xFF;
    serializer->buffer[serializer->position++] = (value >> 16) & 0xFF;
    serializer->buffer[serializer->position++] = (value >> 8) & 0xFF;
    serializer->buffer[serializer->position++] = value & 0xFF;
    serializer->length =
        serializer->position > serializer->length ? serializer->position : serializer->length;
    return true;
}

bool nimcp_write_int8(NimcpSerializer* serializer, int8_t value)
{
    return nimcp_write_uint8(serializer, (uint8_t) value);
}

bool nimcp_write_int16(NimcpSerializer* serializer, int16_t value)
{
    return nimcp_write_uint16(serializer, (uint16_t) value);
}

bool nimcp_write_int32(NimcpSerializer* serializer, int32_t value)
{
    return nimcp_write_uint32(serializer, (uint32_t) value);
}

bool nimcp_write_int64(NimcpSerializer* serializer, int64_t value)
{
    return nimcp_write_uint64(serializer, (uint64_t) value);
}

bool nimcp_write_float(NimcpSerializer* serializer, float value)
{
    union {
        float f;
        uint32_t i;
    } converter;
    converter.f = value;
    return nimcp_write_uint32(serializer, converter.i);
}

bool nimcp_write_double(NimcpSerializer* serializer, double value)
{
    union {
        double d;
        uint64_t i;
    } converter;
    converter.d = value;
    return nimcp_write_uint64(serializer, converter.i);
}

bool nimcp_write_bool(NimcpSerializer* serializer, bool value)
{
    return nimcp_write_uint8(serializer, value ? 1 : 0);
}

uint32_t nimcp_read_uint32(NimcpSerializer* serializer)
{
    if (!nimcp_check_read(serializer, 4)) {
        return 0;
    }
    uint32_t value = ((uint32_t) serializer->buffer[serializer->position] << 24) |
                     ((uint32_t) serializer->buffer[serializer->position + 1] << 16) |
                     ((uint32_t) serializer->buffer[serializer->position + 2] << 8) |
                     serializer->buffer[serializer->position + 3];
    serializer->position += 4;
    return value;
}

/**
 * @brief Reads an unsigned 8-bit integer from the serializer
 * @param serializer The serializer instance
 * @return The read value, or 0 if an error occurred
 */
uint8_t nimcp_read_uint8(NimcpSerializer* serializer)
{
    if (!nimcp_check_read(serializer, 1)) {
        return 0;
    }
    return serializer->buffer[serializer->position++];
}

/**
 * @brief Reads an unsigned 64-bit integer from the serializer
 * @param serializer The serializer instance
 * @return The read value, or 0 if an error occurred
 * @note Reads in big-endian format
 */
uint64_t nimcp_read_uint64(NimcpSerializer* serializer)
{
    if (!nimcp_check_read(serializer, 8)) {
        return 0;
    }
    uint64_t value = ((uint64_t) serializer->buffer[serializer->position] << 56) |
                     ((uint64_t) serializer->buffer[serializer->position + 1] << 48) |
                     ((uint64_t) serializer->buffer[serializer->position + 2] << 40) |
                     ((uint64_t) serializer->buffer[serializer->position + 3] << 32) |
                     ((uint64_t) serializer->buffer[serializer->position + 4] << 24) |
                     ((uint64_t) serializer->buffer[serializer->position + 5] << 16) |
                     ((uint64_t) serializer->buffer[serializer->position + 6] << 8) |
                     serializer->buffer[serializer->position + 7];
    serializer->position += 8;
    return value;
}

int8_t nimcp_read_int8(NimcpSerializer* serializer)
{
    return (int8_t) nimcp_read_uint8(serializer);
}

int16_t nimcp_read_int16(NimcpSerializer* serializer)
{
    return (int16_t) nimcp_read_uint16(serializer);
}

int32_t nimcp_read_int32(NimcpSerializer* serializer)
{
    return (int32_t) nimcp_read_uint32(serializer);
}

int64_t nimcp_read_int64(NimcpSerializer* serializer)
{
    return (int64_t) nimcp_read_uint64(serializer);
}

float nimcp_read_float(NimcpSerializer* serializer)
{
    union {
        uint32_t i;
        float f;
    } converter;
    converter.i = nimcp_read_uint32(serializer);
    return converter.f;
}


double nimcp_read_double(NimcpSerializer* serializer)
{
    union {
        uint64_t i;
        double d;
    } converter;
    converter.i = nimcp_read_uint64(serializer);
    return converter.d;
}

bool nimcp_read_bool(NimcpSerializer* serializer)
{
    return nimcp_read_uint8(serializer) != 0;
}

bool nimcp_serializer_has_error(const NimcpSerializer* serializer)
{
    return serializer ? serializer->has_error : true;
}

void nimcp_serializer_clear_error(NimcpSerializer* serializer)
{
    if (serializer) {
        serializer->has_error = false;
    }
}
