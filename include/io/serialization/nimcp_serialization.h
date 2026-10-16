// src/includes/protocol/nimcp_serialization.h
#ifndef NIMCP_SERIALIZATION_H
#define NIMCP_SERIALIZATION_H

#include <lz4.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "utils/validation/nimcp_common.h"

// Error codes for serialization operations
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NIMCP_SERIAL_SUCCESS = 0,
    NIMCP_SERIAL_ERROR_MEMORY,
    NIMCP_SERIAL_ERROR_BOUNDS,
    NIMCP_SERIAL_ERROR_COMPRESSION,
    NIMCP_SERIAL_ERROR_INVALID_PARAM
} NimcpSerialResult;

// Core serializer structure
typedef struct {
    uint8_t* buffer;
    size_t capacity;
    size_t position;
    size_t length;
    bool is_compressed;
    bool has_error;
} NimcpSerializer;

// Constants
#define NIMCP_SERIALIZER_INITIAL_SIZE 1024
#define NIMCP_SERIALIZER_MAX_SIZE (1024 * 1024 * 64)  // 64MB limit
#define NIMCP_SERIALIZER_VERSION 1

// Core API
NimcpSerializer* nimcp_serializer_create(size_t initial_capacity);
void nimcp_serializer_destroy(NimcpSerializer* serializer);
void nimcp_serializer_reset(NimcpSerializer* serializer);
bool nimcp_serializer_set_buffer(NimcpSerializer* serializer, const uint8_t* data, size_t length);

// Buffer management
uint8_t* nimcp_serializer_get_buffer(NimcpSerializer* serializer);
size_t nimcp_serializer_get_length(NimcpSerializer* serializer);
size_t nimcp_serializer_get_position(NimcpSerializer* serializer);
bool nimcp_serializer_set_position(NimcpSerializer* serializer, size_t position);
void nimcp_serializer_mark_compressed(NimcpSerializer* serializer);
bool nimcp_serializer_is_compressed(const NimcpSerializer* serializer);

// Compression operations
NimcpSerialResult nimcp_serializer_compress(NimcpSerializer* serializer);
NimcpSerialResult nimcp_serializer_decompress(NimcpSerializer* serializer);

// Write operations
bool nimcp_write_uint8(NimcpSerializer* serializer, uint8_t value);
bool nimcp_write_uint16(NimcpSerializer* serializer, uint16_t value);
bool nimcp_write_uint32(NimcpSerializer* serializer, uint32_t value);
bool nimcp_write_uint64(NimcpSerializer* serializer, uint64_t value);
bool nimcp_write_int8(NimcpSerializer* serializer, int8_t value);
bool nimcp_write_int16(NimcpSerializer* serializer, int16_t value);
bool nimcp_write_int32(NimcpSerializer* serializer, int32_t value);
bool nimcp_write_int64(NimcpSerializer* serializer, int64_t value);
bool nimcp_write_float(NimcpSerializer* serializer, float value);
bool nimcp_write_double(NimcpSerializer* serializer, double value);
bool nimcp_write_bool(NimcpSerializer* serializer, bool value);
bool nimcp_write_bytes(NimcpSerializer* serializer, const uint8_t* data, size_t length);

// Read operations
uint8_t nimcp_read_uint8(NimcpSerializer* serializer);
uint16_t nimcp_read_uint16(NimcpSerializer* serializer);
uint32_t nimcp_read_uint32(NimcpSerializer* serializer);
uint64_t nimcp_read_uint64(NimcpSerializer* serializer);
int8_t nimcp_read_int8(NimcpSerializer* serializer);
int16_t nimcp_read_int16(NimcpSerializer* serializer);
int32_t nimcp_read_int32(NimcpSerializer* serializer);
int64_t nimcp_read_int64(NimcpSerializer* serializer);
float nimcp_read_float(NimcpSerializer* serializer);
double nimcp_read_double(NimcpSerializer* serializer);
bool nimcp_read_bool(NimcpSerializer* serializer);
const uint8_t* nimcp_read_bytes(NimcpSerializer* serializer, size_t* length);


/**
 * @brief Checks if the serializer has encountered any errors
 *
 * @param serializer The serializer instance to check
 * @return true if an error has occurred, false otherwise. Returns true if serializer is NULL
 */
bool nimcp_serializer_has_error(const NimcpSerializer* serializer);

/**
 * @brief Clears any error state in the serializer
 *
 * @param serializer The serializer instance to clear errors from
 */
void nimcp_serializer_clear_error(NimcpSerializer* serializer);


#ifdef __cplusplus
}
#endif
#endif  // NIMCP_SERIALIZATION_H
