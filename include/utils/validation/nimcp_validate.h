/**
 * @file nimcp_validate.h
 * @brief Field validation utilities for NIMCP protocol
 */

#ifndef NIMCP_VALIDATE_H
#define NIMCP_VALIDATE_H

#include <stdbool.h>
#include <stddef.h>
#include "utils/validation/nimcp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Validate integer field data
 *
 * @param field_data Pointer to field data
 * @param size Size of field in bytes
 * @return true if valid, false if invalid
 */
bool nimcp_validate_integer_field(const void* field_data, size_t size);

/**
 * @brief Validate floating point field data
 *
 * @param field_data Pointer to field data
 * @param size Size of field in bytes
 * @return true if valid, false if invalid
 */
bool nimcp_validate_float_field(const void* field_data, size_t size);

/**
 * @brief Validate string field data
 *
 * @param field_data Pointer to field data
 * @param size Size of field in bytes
 * @return true if valid, false if invalid
 */
bool nimcp_validate_string_field(const void* field_data, size_t size);

/**
 * @brief Validate array field data
 *
 * @param field_data Pointer to field data
 * @param size Size of field in bytes
 * @return true if valid, false if invalid
 */
bool nimcp_validate_array_field(const void* field_data, size_t size);

/**
 * @brief Validate complete state data structure
 *
 * @param state_data Pointer to state data
 * @param size Total size of state data
 * @return true if valid, false if invalid
 */
bool nimcp_validate_state_fields(const void* state_data, size_t size);

/**
 * @brief Validate pointer is non-NULL
 *
 * @param ptr Pointer to validate
 * @param name Name of pointer for error messages (can be NULL)
 * @return true if valid (non-NULL), false if NULL
 */
bool nimcp_validate_pointer(const void* ptr, const char* name);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VALIDATE_H */
