#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_config_array.c - Array Type Support for NIMCP Config Implementation
//=============================================================================
/**
 * @file nimcp_config_array.c
 * @brief Implementation of homogeneous array support for dynamic configuration
 *
 * WHAT: Thread-safe array handling with unified memory integration
 * WHY:  Support complex multi-value config parameters
 * HOW:  Dynamic arrays with CoW, parsing, and serialization
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 * @version 1.0.0
 */

#include "utils/config/nimcp_config_array.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_rwlock.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(config_array)

//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Validate array handle
 */
static inline bool validate_array(const config_array_full_t* arr) {
    return arr != NULL && arr->magic == CONFIG_ARRAY_MAGIC;
}

/**
 * @brief Grow array capacity
 */
static bool grow_array(config_array_t* arr, size_t min_capacity) {
    if (!validate_array(arr)) {
        LOG_ERROR("grow_array: invalid array handle");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "grow_array: validate_array is NULL");
        return false;
    }

    size_t new_capacity = arr->capacity;
    if (new_capacity == 0) {
        new_capacity = CONFIG_ARRAY_DEFAULT_CAPACITY;
    }

    while (new_capacity < min_capacity) {
        new_capacity *= CONFIG_ARRAY_GROWTH_FACTOR;
        if (new_capacity > CONFIG_ARRAY_MAX_SIZE) {
            LOG_ERROR("grow_array: capacity exceeds maximum (%zu)", CONFIG_ARRAY_MAX_SIZE);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "grow_array: validation failed");
            return false;
        }
    }

    size_t elem_size = 0;
    void* old_data = NULL;
    void* new_data = NULL;

    switch (arr->element_type) {
        case CONFIG_TYPE_INT:
            elem_size = sizeof(int64_t);
            old_data = arr->data.int_vals;
            new_data = nimcp_realloc(old_data, new_capacity * elem_size);
            if (!new_data) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "grow_array: new_data is NULL");
                return false;
            }
            arr->data.int_vals = (int64_t*)new_data;
            break;

        case CONFIG_TYPE_FLOAT:
            elem_size = sizeof(double);
            old_data = arr->data.float_vals;
            new_data = nimcp_realloc(old_data, new_capacity * elem_size);
            if (!new_data) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "grow_array: new_data is NULL");
                return false;
            }
            arr->data.float_vals = (double*)new_data;
            break;

        case CONFIG_TYPE_BOOL:
            elem_size = sizeof(bool);
            old_data = arr->data.bool_vals;
            new_data = nimcp_realloc(old_data, new_capacity * elem_size);
            if (!new_data) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "grow_array: new_data is NULL");
                return false;
            }
            arr->data.bool_vals = (bool*)new_data;
            break;

        case CONFIG_TYPE_STRING:
            elem_size = sizeof(char*);
            old_data = arr->data.string_vals;
            new_data = nimcp_realloc(old_data, new_capacity * elem_size);
            if (!new_data) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "grow_array: new_data is NULL");
                return false;
            }
            arr->data.string_vals = (char**)new_data;
            // Zero out new string pointers
            memset(arr->data.string_vals + arr->capacity, 0,
                   (new_capacity - arr->capacity) * sizeof(char*));
            break;

        default:
            LOG_ERROR("grow_array: unknown element type %d", arr->element_type);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "grow_array: new_data is NULL");
            return false;
    }

    LOG_DEBUG("grow_array: expanded from %zu to %zu (count=%zu)",
              arr->capacity, new_capacity, arr->count);
    arr->capacity = new_capacity;
    return true;
}

/**
 * @brief Trim whitespace and quotes from string
 */
static char* trim_and_unquote(char* str) {
    if (!str) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "str is NULL");

        return NULL;

    }

    // Trim leading whitespace
    while (*str && isspace((unsigned char)*str)) str++;

    // Trim trailing whitespace
    char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }

    // Remove quotes
    size_t len = strlen(str);
    if (len >= 2 && ((str[0] == '"' && str[len-1] == '"') ||
                     (str[0] == '\'' && str[len-1] == '\''))) {
        str[len-1] = '\0';
        str++;
    }

    return str;
}

//=============================================================================
// Array Lifecycle API
//=============================================================================

config_array_t* config_array_create(config_value_type_t element_type, size_t capacity) {
    if (capacity > CONFIG_ARRAY_MAX_SIZE) {
        LOG_ERROR("config_array_create: capacity %zu exceeds maximum", capacity);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "config_array_create: validation failed");
        return NULL;
    }

    if (capacity == 0) {
        capacity = CONFIG_ARRAY_DEFAULT_CAPACITY;
    }

    config_array_t* arr = (config_array_t*)nimcp_calloc(1, sizeof(config_array_t));
    if (!arr) {
        LOG_ERROR("config_array_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "config_array_create: arr is NULL");
        return NULL;
    }

    arr->magic = CONFIG_ARRAY_MAGIC;
    arr->element_type = element_type;
    arr->count = 0;
    arr->capacity = capacity;
    arr->unified_mem_handle = NULL;

    // Create RW lock
    nimcp_platform_rwlock_t* rwlock = (nimcp_platform_rwlock_t*)nimcp_malloc(sizeof(nimcp_platform_rwlock_t));
    if (!rwlock) {
        LOG_ERROR("config_array_create: rwlock allocation failed");
        nimcp_free(arr);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "config_array_create: rwlock is NULL");
        return NULL;
    }

    if (nimcp_platform_rwlock_init(rwlock) != 0) {
        LOG_ERROR("config_array_create: rwlock init failed");
        nimcp_free(rwlock);
        nimcp_free(arr);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "config_array_create: validation failed");
        return NULL;
    }
    arr->rwlock = rwlock;

    // Allocate backing storage
    size_t elem_size = 0;
    switch (element_type) {
        case CONFIG_TYPE_INT:
            elem_size = sizeof(int64_t);
            arr->data.int_vals = (int64_t*)nimcp_calloc(capacity, elem_size);
            if (!arr->data.int_vals) goto alloc_error;
            break;

        case CONFIG_TYPE_FLOAT:
            elem_size = sizeof(double);
            arr->data.float_vals = (double*)nimcp_calloc(capacity, elem_size);
            if (!arr->data.float_vals) goto alloc_error;
            break;

        case CONFIG_TYPE_BOOL:
            elem_size = sizeof(bool);
            arr->data.bool_vals = (bool*)nimcp_calloc(capacity, elem_size);
            if (!arr->data.bool_vals) goto alloc_error;
            break;

        case CONFIG_TYPE_STRING:
            elem_size = sizeof(char*);
            arr->data.string_vals = (char**)nimcp_calloc(capacity, elem_size);
            if (!arr->data.string_vals) goto alloc_error;
            break;

        default:
            LOG_ERROR("config_array_create: unknown element type %d", element_type);
            goto alloc_error;
    }

    LOG_DEBUG("config_array_create: type=%d capacity=%zu elem_size=%zu",
              element_type, capacity, elem_size);
    return arr;

alloc_error:
    LOG_ERROR("config_array_create: data allocation failed");
    nimcp_platform_rwlock_destroy((nimcp_platform_rwlock_t*)arr->rwlock);
    nimcp_free(arr->rwlock);
    nimcp_free(arr);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "config_array_create: operation failed");
    return NULL;
}

void config_array_destroy(config_array_t* arr) {
    if (!arr) return;

    if (!validate_array(arr)) {
        LOG_WARN("config_array_destroy: invalid array (corrupted or double-free)");
        return;
    }

    LOG_DEBUG("config_array_destroy: type=%d count=%zu capacity=%zu",
              arr->element_type, arr->count, arr->capacity);

    // Free string data
    if (arr->element_type == CONFIG_TYPE_STRING && arr->data.string_vals) {
        for (size_t i = 0; i < arr->count; i++) {
            if (arr->data.string_vals[i]) {
                nimcp_free(arr->data.string_vals[i]);
            }
        }
    }

    // Free backing storage
    if (arr->data.int_vals) {  // Union, same for all types
        nimcp_free(arr->data.int_vals);
    }

    // Destroy lock
    if (arr->rwlock) {
        nimcp_platform_rwlock_destroy((nimcp_platform_rwlock_t*)arr->rwlock);
        nimcp_free(arr->rwlock);
    }

    // Invalidate and free
    arr->magic = 0;
    nimcp_free(arr);
}

config_array_t* config_array_clone(const config_array_t* arr) {
    if (!validate_array(arr)) {
        LOG_ERROR("config_array_clone: invalid source array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_array_clone: validate_array is NULL");
        return NULL;
    }

    nimcp_platform_rwlock_rdlock((nimcp_platform_rwlock_t*)arr->rwlock);

    config_array_t* clone = config_array_create(arr->element_type, arr->capacity);
    if (!clone) {
        nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "config_array_clone: clone is NULL");
        return NULL;
    }

    clone->count = arr->count;

    // Deep copy data
    switch (arr->element_type) {
        case CONFIG_TYPE_INT:
            memcpy(clone->data.int_vals, arr->data.int_vals, arr->count * sizeof(int64_t));
            break;

        case CONFIG_TYPE_FLOAT:
            memcpy(clone->data.float_vals, arr->data.float_vals, arr->count * sizeof(double));
            break;

        case CONFIG_TYPE_BOOL:
            memcpy(clone->data.bool_vals, arr->data.bool_vals, arr->count * sizeof(bool));
            break;

        case CONFIG_TYPE_STRING:
            for (size_t i = 0; i < arr->count; i++) {
                if (arr->data.string_vals[i]) {
                    clone->data.string_vals[i] = nimcp_strdup(arr->data.string_vals[i]);
                    if (!clone->data.string_vals[i]) {
                        LOG_ERROR("config_array_clone: string duplication failed at index %zu", i);
                        config_array_destroy(clone);
                        nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);
                        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "config_array_clone: clone->data is NULL");
                        return NULL;
                    }
                }
            }
            break;
    }

    nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);

    LOG_DEBUG("config_array_clone: cloned array type=%d count=%zu",
              arr->element_type, arr->count);
    return clone;
}

void config_array_clear(config_array_t* arr) {
    if (!validate_array(arr)) {
        LOG_ERROR("config_array_clear: invalid array");
        return;
    }

    nimcp_platform_rwlock_wrlock((nimcp_platform_rwlock_t*)arr->rwlock);

    // Free strings
    if (arr->element_type == CONFIG_TYPE_STRING && arr->data.string_vals) {
        for (size_t i = 0; i < arr->count; i++) {
            if (arr->data.string_vals[i]) {
                nimcp_free(arr->data.string_vals[i]);
                arr->data.string_vals[i] = NULL;
            }
        }
    }

    arr->count = 0;
    nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);

    LOG_DEBUG("config_array_clear: cleared array");
}

bool config_array_reserve(config_array_t* arr, size_t new_capacity) {
    if (!validate_array(arr)) {
        LOG_ERROR("config_array_reserve: invalid array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_array_reserve: validate_array is NULL");
        return false;
    }

    if (new_capacity <= arr->capacity) {
        return true;  // Already have enough capacity
    }

    nimcp_platform_rwlock_wrlock((nimcp_platform_rwlock_t*)arr->rwlock);
    bool result = grow_array(arr, new_capacity);
    nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);

    return result;
}

//=============================================================================
// Array Access API
//=============================================================================

config_value_type_t config_array_get_type(const config_array_t* arr) {
    if (!validate_array(arr)) return CONFIG_TYPE_INT;  // Safe default
    return arr->element_type;
}

size_t config_array_size(const config_array_t* arr) {
    if (!validate_array(arr)) return 0;
    return arr->count;
}

size_t config_array_capacity(const config_array_t* arr) {
    if (!validate_array(arr)) return 0;
    return arr->capacity;
}

bool config_array_is_empty(const config_array_t* arr) {
    if (!validate_array(arr)) return true;
    return arr->count == 0;
}

//=============================================================================
// Array Append API
//=============================================================================

bool config_array_append_int(config_array_t* arr, int64_t val) {
    if (!validate_array(arr) || arr->element_type != CONFIG_TYPE_INT) {
        LOG_ERROR("config_array_append_int: invalid array or type mismatch");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_array_append_int: validate_array is NULL");
        return false;
    }

    nimcp_platform_rwlock_wrlock((nimcp_platform_rwlock_t*)arr->rwlock);

    if (arr->count >= arr->capacity) {
        if (!grow_array(arr, arr->count + 1)) {
            nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_array_append_int: grow_array is NULL");
            return false;
        }
    }

    arr->data.int_vals[arr->count++] = val;
    nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);

    return true;
}

bool config_array_append_float(config_array_t* arr, double val) {
    if (!validate_array(arr) || arr->element_type != CONFIG_TYPE_FLOAT) {
        LOG_ERROR("config_array_append_float: invalid array or type mismatch");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_array_append_float: validate_array is NULL");
        return false;
    }

    nimcp_platform_rwlock_wrlock((nimcp_platform_rwlock_t*)arr->rwlock);

    if (arr->count >= arr->capacity) {
        if (!grow_array(arr, arr->count + 1)) {
            nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_array_append_float: grow_array is NULL");
            return false;
        }
    }

    arr->data.float_vals[arr->count++] = val;
    nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);

    return true;
}

bool config_array_append_bool(config_array_t* arr, bool val) {
    if (!validate_array(arr) || arr->element_type != CONFIG_TYPE_BOOL) {
        LOG_ERROR("config_array_append_bool: invalid array or type mismatch");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_array_append_bool: validate_array is NULL");
        return false;
    }

    nimcp_platform_rwlock_wrlock((nimcp_platform_rwlock_t*)arr->rwlock);

    if (arr->count >= arr->capacity) {
        if (!grow_array(arr, arr->count + 1)) {
            nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_array_append_bool: grow_array is NULL");
            return false;
        }
    }

    arr->data.bool_vals[arr->count++] = val;
    nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);

    return true;
}

bool config_array_append_string(config_array_t* arr, const char* val) {
    if (!validate_array(arr) || arr->element_type != CONFIG_TYPE_STRING) {
        LOG_ERROR("config_array_append_string: invalid array or type mismatch");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_array_append_string: validate_array is NULL");
        return false;
    }

    if (!val) {
        LOG_ERROR("config_array_append_string: NULL value");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_array_append_string: val is NULL");
        return false;
    }

    nimcp_platform_rwlock_wrlock((nimcp_platform_rwlock_t*)arr->rwlock);

    if (arr->count >= arr->capacity) {
        if (!grow_array(arr, arr->count + 1)) {
            nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_array_append_string: grow_array is NULL");
            return false;
        }
    }

    arr->data.string_vals[arr->count] = nimcp_strdup(val);
    if (!arr->data.string_vals[arr->count]) {
        LOG_ERROR("config_array_append_string: string duplication failed");
        nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "config_array_append_string: arr->data is NULL");
        return false;
    }

    arr->count++;
    nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);

    return true;
}

//=============================================================================
// Array Get API
//=============================================================================

int64_t config_array_get_int(const config_array_t* arr, size_t idx, int64_t default_val) {
    if (!validate_array(arr) || arr->element_type != CONFIG_TYPE_INT) {
        return default_val;
    }

    nimcp_platform_rwlock_rdlock((nimcp_platform_rwlock_t*)arr->rwlock);

    int64_t result = default_val;
    if (idx < arr->count) {
        result = arr->data.int_vals[idx];
    }

    nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);
    return result;
}

double config_array_get_float(const config_array_t* arr, size_t idx, double default_val) {
    if (!validate_array(arr) || arr->element_type != CONFIG_TYPE_FLOAT) {
        return default_val;
    }

    nimcp_platform_rwlock_rdlock((nimcp_platform_rwlock_t*)arr->rwlock);

    double result = default_val;
    if (idx < arr->count) {
        result = arr->data.float_vals[idx];
    }

    nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);
    return result;
}

bool config_array_get_bool(const config_array_t* arr, size_t idx, bool default_val) {
    if (!validate_array(arr) || arr->element_type != CONFIG_TYPE_BOOL) {
        return default_val;
    }

    nimcp_platform_rwlock_rdlock((nimcp_platform_rwlock_t*)arr->rwlock);

    bool result = default_val;
    if (idx < arr->count) {
        result = arr->data.bool_vals[idx];
    }

    nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);
    return result;
}

const char* config_array_get_string(const config_array_t* arr, size_t idx,
                                     const char* default_val) {
    if (!validate_array(arr) || arr->element_type != CONFIG_TYPE_STRING) {
        return default_val;
    }

    nimcp_platform_rwlock_rdlock((nimcp_platform_rwlock_t*)arr->rwlock);

    const char* result = default_val;
    if (idx < arr->count && arr->data.string_vals[idx]) {
        result = arr->data.string_vals[idx];
    }

    nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);
    return result;
}

//=============================================================================
// Array Set API
//=============================================================================

bool config_array_set_int(config_array_t* arr, size_t idx, int64_t val) {
    if (!validate_array(arr) || arr->element_type != CONFIG_TYPE_INT) {
        LOG_ERROR("config_array_set_int: invalid array or type mismatch");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_array_set_int: validate_array is NULL");
        return false;
    }

    nimcp_platform_rwlock_wrlock((nimcp_platform_rwlock_t*)arr->rwlock);

    bool result = false;
    if (idx < arr->count) {
        arr->data.int_vals[idx] = val;
        result = true;
    } else {
        LOG_ERROR("config_array_set_int: index %zu out of bounds (count=%zu)", idx, arr->count);
    }

    nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);
    return result;
}

bool config_array_set_float(config_array_t* arr, size_t idx, double val) {
    if (!validate_array(arr) || arr->element_type != CONFIG_TYPE_FLOAT) {
        LOG_ERROR("config_array_set_float: invalid array or type mismatch");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_array_set_float: validate_array is NULL");
        return false;
    }

    nimcp_platform_rwlock_wrlock((nimcp_platform_rwlock_t*)arr->rwlock);

    bool result = false;
    if (idx < arr->count) {
        arr->data.float_vals[idx] = val;
        result = true;
    } else {
        LOG_ERROR("config_array_set_float: index %zu out of bounds (count=%zu)", idx, arr->count);
    }

    nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);
    return result;
}

bool config_array_set_bool(config_array_t* arr, size_t idx, bool val) {
    if (!validate_array(arr) || arr->element_type != CONFIG_TYPE_BOOL) {
        LOG_ERROR("config_array_set_bool: invalid array or type mismatch");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_array_set_bool: validate_array is NULL");
        return false;
    }

    nimcp_platform_rwlock_wrlock((nimcp_platform_rwlock_t*)arr->rwlock);

    bool result = false;
    if (idx < arr->count) {
        arr->data.bool_vals[idx] = val;
        result = true;
    } else {
        LOG_ERROR("config_array_set_bool: index %zu out of bounds (count=%zu)", idx, arr->count);
    }

    nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);
    return result;
}

bool config_array_set_string(config_array_t* arr, size_t idx, const char* val) {
    if (!validate_array(arr) || arr->element_type != CONFIG_TYPE_STRING) {
        LOG_ERROR("config_array_set_string: invalid array or type mismatch");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_array_set_string: validate_array is NULL");
        return false;
    }

    if (!val) {
        LOG_ERROR("config_array_set_string: NULL value");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_array_set_string: val is NULL");
        return false;
    }

    nimcp_platform_rwlock_wrlock((nimcp_platform_rwlock_t*)arr->rwlock);

    bool result = false;
    if (idx < arr->count) {
        // Free old string
        if (arr->data.string_vals[idx]) {
            nimcp_free(arr->data.string_vals[idx]);
        }

        // Duplicate new string
        arr->data.string_vals[idx] = nimcp_strdup(val);
        if (arr->data.string_vals[idx]) {
            result = true;
        } else {
            LOG_ERROR("config_array_set_string: string duplication failed");
        }
    } else {
        LOG_ERROR("config_array_set_string: index %zu out of bounds (count=%zu)", idx, arr->count);
    }

    nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);
    return result;
}

//=============================================================================
// Array Modification API
//=============================================================================

bool config_array_remove(config_array_t* arr, size_t idx) {
    if (!validate_array(arr)) {
        LOG_ERROR("config_array_remove: invalid array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_array_remove: validate_array is NULL");
        return false;
    }

    nimcp_platform_rwlock_wrlock((nimcp_platform_rwlock_t*)arr->rwlock);

    if (idx >= arr->count) {
        LOG_ERROR("config_array_remove: index %zu out of bounds (count=%zu)", idx, arr->count);
        nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "config_array_remove: capacity exceeded");
        return false;
    }

    // Free string if needed
    if (arr->element_type == CONFIG_TYPE_STRING && arr->data.string_vals[idx]) {
        nimcp_free(arr->data.string_vals[idx]);
    }

    // Shift remaining elements
    size_t elem_size = 0;
    void* src_ptr = NULL;
    void* dst_ptr = NULL;

    switch (arr->element_type) {
        case CONFIG_TYPE_INT:
            elem_size = sizeof(int64_t);
            dst_ptr = &arr->data.int_vals[idx];
            src_ptr = &arr->data.int_vals[idx + 1];
            break;

        case CONFIG_TYPE_FLOAT:
            elem_size = sizeof(double);
            dst_ptr = &arr->data.float_vals[idx];
            src_ptr = &arr->data.float_vals[idx + 1];
            break;

        case CONFIG_TYPE_BOOL:
            elem_size = sizeof(bool);
            dst_ptr = &arr->data.bool_vals[idx];
            src_ptr = &arr->data.bool_vals[idx + 1];
            break;

        case CONFIG_TYPE_STRING:
            elem_size = sizeof(char*);
            dst_ptr = &arr->data.string_vals[idx];
            src_ptr = &arr->data.string_vals[idx + 1];
            break;

        default:
            nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_array_remove: operation failed");
            return false;
    }

    // Shift elements down
    if (idx < arr->count - 1) {
        memmove(dst_ptr, src_ptr, (arr->count - idx - 1) * elem_size);
    }

    arr->count--;
    nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);

    LOG_DEBUG("config_array_remove: removed element at index %zu (new count=%zu)", idx, arr->count);
    return true;
}

bool config_array_resize(config_array_t* arr, size_t new_capacity) {
    if (!validate_array(arr)) {
        LOG_ERROR("config_array_resize: invalid array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_array_resize: validate_array is NULL");
        return false;
    }

    if (new_capacity < arr->count) {
        LOG_ERROR("config_array_resize: cannot shrink below count (new_capacity=%zu count=%zu)",
                  new_capacity, arr->count);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_array_resize: validation failed");
        return false;
    }

    if (new_capacity > CONFIG_ARRAY_MAX_SIZE) {
        LOG_ERROR("config_array_resize: capacity %zu exceeds maximum", new_capacity);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_array_resize: validation failed");
        return false;
    }

    nimcp_platform_rwlock_wrlock((nimcp_platform_rwlock_t*)arr->rwlock);

    size_t old_capacity = arr->capacity;
    arr->capacity = new_capacity;
    bool result = grow_array(arr, new_capacity);

    if (!result) {
        arr->capacity = old_capacity;  // Restore on failure
    }

    nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);
    return result;
}

//=============================================================================
// Parsing API
//=============================================================================

config_array_t* config_parse_int_array(const char* str) {
    if (!str) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "str is NULL");

        return NULL;

    }

    // Create array
    config_array_t* arr = config_array_create(CONFIG_TYPE_INT, 0);
    if (!arr) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "arr is NULL");

        return NULL;

    }

    // Copy string for tokenization
    size_t str_len = strlen(str) + 1;
    char* copy = (char*)nimcp_malloc(str_len);
    if (!copy) {
        config_array_destroy(arr);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "config_parse_int_array: copy is NULL");
        return NULL;
    }
    memcpy(copy, str, str_len);

    // Remove brackets if present
    char* start = copy;
    while (*start && isspace((unsigned char)*start)) start++;
    if (*start == '[') start++;

    char* end = start + strlen(start) - 1;
    while (end > start && (isspace((unsigned char)*end) || *end == ']')) {
        *end = '\0';
        end--;
    }

    // Parse comma-separated values
    char* token = strtok(start, ",");
    while (token) {
        token = trim_and_unquote(token);
        int64_t val = atoll(token);
        if (!config_array_append_int(arr, val)) {
            LOG_ERROR("config_parse_int_array: failed to append value");
            config_array_destroy(arr);
            nimcp_free(copy);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_parse_int_array: config_array_append_int is NULL");
            return NULL;
        }
        token = strtok(NULL, ",");
    }

    nimcp_free(copy);
    LOG_DEBUG("config_parse_int_array: parsed %zu values", arr->count);
    return arr;
}

config_array_t* config_parse_float_array(const char* str) {
    if (!str) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "str is NULL");

        return NULL;

    }

    config_array_t* arr = config_array_create(CONFIG_TYPE_FLOAT, 0);
    if (!arr) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "arr is NULL");

        return NULL;

    }

    size_t str_len = strlen(str) + 1;
    char* copy = (char*)nimcp_malloc(str_len);
    if (!copy) {
        config_array_destroy(arr);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "config_parse_float_array: copy is NULL");
        return NULL;
    }
    memcpy(copy, str, str_len);

    char* start = copy;
    while (*start && isspace((unsigned char)*start)) start++;
    if (*start == '[') start++;

    char* end = start + strlen(start) - 1;
    while (end > start && (isspace((unsigned char)*end) || *end == ']')) {
        *end = '\0';
        end--;
    }

    char* token = strtok(start, ",");
    while (token) {
        token = trim_and_unquote(token);
        // P1-2 fix: Use strtod instead of atof for safe conversion
        char* endptr;
        errno = 0;
        double val = strtod(token, &endptr);
        if (endptr == token || errno == ERANGE) {
            LOG_WARN("config_parse_float_array: invalid float value '%s'", token);
            val = 0.0;
        }
        if (!config_array_append_float(arr, val)) {
            LOG_ERROR("config_parse_float_array: failed to append value");
            config_array_destroy(arr);
            nimcp_free(copy);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_parse_float_array: config_array_append_float is NULL");
            return NULL;
        }
        token = strtok(NULL, ",");
    }

    nimcp_free(copy);
    LOG_DEBUG("config_parse_float_array: parsed %zu values", arr->count);
    return arr;
}

config_array_t* config_parse_bool_array(const char* str) {
    if (!str) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "str is NULL");

        return NULL;

    }

    config_array_t* arr = config_array_create(CONFIG_TYPE_BOOL, 0);
    if (!arr) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "arr is NULL");

        return NULL;

    }

    size_t str_len = strlen(str) + 1;
    char* copy = (char*)nimcp_malloc(str_len);
    if (!copy) {
        config_array_destroy(arr);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "config_parse_bool_array: copy is NULL");
        return NULL;
    }
    memcpy(copy, str, str_len);

    char* start = copy;
    while (*start && isspace((unsigned char)*start)) start++;
    if (*start == '[') start++;

    char* end = start + strlen(start) - 1;
    while (end > start && (isspace((unsigned char)*end) || *end == ']')) {
        *end = '\0';
        end--;
    }

    char* token = strtok(start, ",");
    while (token) {
        token = trim_and_unquote(token);
        bool val = (strcmp(token, "true") == 0 || strcmp(token, "1") == 0);
        if (!config_array_append_bool(arr, val)) {
            LOG_ERROR("config_parse_bool_array: failed to append value");
            config_array_destroy(arr);
            nimcp_free(copy);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_parse_bool_array: config_array_append_bool is NULL");
            return NULL;
        }
        token = strtok(NULL, ",");
    }

    nimcp_free(copy);
    LOG_DEBUG("config_parse_bool_array: parsed %zu values", arr->count);
    return arr;
}

config_array_t* config_parse_string_array(const char* str) {
    if (!str) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "str is NULL");

        return NULL;

    }

    config_array_t* arr = config_array_create(CONFIG_TYPE_STRING, 0);
    if (!arr) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "arr is NULL");

        return NULL;

    }

    size_t str_len = strlen(str) + 1;
    char* copy = (char*)nimcp_malloc(str_len);
    if (!copy) {
        config_array_destroy(arr);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "config_parse_string_array: copy is NULL");
        return NULL;
    }
    memcpy(copy, str, str_len);

    char* start = copy;
    while (*start && isspace((unsigned char)*start)) start++;
    if (*start == '[') start++;

    char* end = start + strlen(start) - 1;
    while (end > start && (isspace((unsigned char)*end) || *end == ']')) {
        *end = '\0';
        end--;
    }

    // Parse quoted strings (handles "str1", "str2", etc.)
    char* p = start;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;

        char* token_start = p;
        bool in_quotes = false;

        // Find end of token (accounting for quotes)
        while (*p && (in_quotes || *p != ',')) {
            if (*p == '"' || *p == '\'') {
                in_quotes = !in_quotes;
            }
            p++;
        }

        if (*p == ',') {
            *p = '\0';
            p++;
        }

        char* token = trim_and_unquote(token_start);
        if (*token) {
            if (!config_array_append_string(arr, token)) {
                LOG_ERROR("config_parse_string_array: failed to append value");
                config_array_destroy(arr);
                nimcp_free(copy);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_parse_string_array: config_array_append_string is NULL");
                return NULL;
            }
        }
    }

    nimcp_free(copy);
    LOG_DEBUG("config_parse_string_array: parsed %zu values", arr->count);
    return arr;
}

config_array_t* config_parse_array_auto(const char* str) {
    if (!str) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "str is NULL");

        return NULL;

    }

    LOG_DEBUG("config_parse_array_auto: auto-detecting type for: %s", str);

    // Skip whitespace and brackets
    const char* p = str;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '[') p++;
    while (*p && isspace((unsigned char)*p)) p++;

    // Empty array - default to INT
    if (*p == ']' || *p == '\0') {
        LOG_DEBUG("config_parse_array_auto: empty array, defaulting to INT");
        return config_array_create(CONFIG_TYPE_INT, 0);
    }

    // Check first element to detect type
    if (*p == '"' || *p == '\'') {
        // String array
        LOG_DEBUG("config_parse_array_auto: detected STRING type");
        return config_parse_string_array(str);
    }

    // Check for boolean keywords
    if (strncmp(p, "true", 4) == 0 || strncmp(p, "false", 5) == 0 ||
        ((*p == '1' || *p == '0') && (p[1] == ',' || p[1] == ']' || isspace((unsigned char)p[1])))) {
        LOG_DEBUG("config_parse_array_auto: detected BOOL type");
        return config_parse_bool_array(str);
    }

    // Check for float (contains decimal point or scientific notation)
    const char* scan = p;
    bool has_decimal = false;
    bool has_exp = false;
    while (*scan && *scan != ',' && *scan != ']') {
        if (*scan == '.') has_decimal = true;
        if (*scan == 'e' || *scan == 'E') has_exp = true;
        scan++;
    }

    if (has_decimal || has_exp) {
        LOG_DEBUG("config_parse_array_auto: detected FLOAT type");
        return config_parse_float_array(str);
    }

    // Default to INT
    LOG_DEBUG("config_parse_array_auto: defaulting to INT type");
    return config_parse_int_array(str);
}

//=============================================================================
// Serialization API
//=============================================================================

char* config_array_to_string(const config_array_t* arr) {
    if (!validate_array(arr)) {
        LOG_ERROR("config_array_to_string: invalid array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_array_to_string: validate_array is NULL");
        return NULL;
    }

    nimcp_platform_rwlock_rdlock((nimcp_platform_rwlock_t*)arr->rwlock);

    // Calculate buffer size (conservative estimate)
    size_t buf_size = 64 + arr->count * 64;  // 64 bytes per element should be enough
    char* buffer = (char*)nimcp_malloc(buf_size);
    if (!buffer) {
        nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "config_array_to_string: buffer is NULL");
        return NULL;
    }

    size_t remaining = buf_size;
    char* p = buffer;
    int written = snprintf(p, remaining, "[");
    if (written < 0 || (size_t)written >= remaining) {
        nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);
        nimcp_free(buffer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_array_to_string: capacity exceeded");
        return NULL;
    }
    p += written;
    remaining -= (size_t)written;

    for (size_t i = 0; i < arr->count; i++) {
        if (i > 0) {
            written = snprintf(p, remaining, ", ");
            if (written < 0 || (size_t)written >= remaining) {
                LOG_ERROR("config_array_to_string: buffer overflow prevented");
                nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);
                nimcp_free(buffer);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_array_to_string: capacity exceeded");
                return NULL;
            }
            p += written;
            remaining -= (size_t)written;
        }

        switch (arr->element_type) {
            case CONFIG_TYPE_INT:
                written = snprintf(p, remaining, "%ld", (long)arr->data.int_vals[i]);
                break;

            case CONFIG_TYPE_FLOAT:
                written = snprintf(p, remaining, "%.6f", arr->data.float_vals[i]);
                break;

            case CONFIG_TYPE_BOOL:
                written = snprintf(p, remaining, "%s", arr->data.bool_vals[i] ? "true" : "false");
                break;

            case CONFIG_TYPE_STRING:
                if (arr->data.string_vals[i]) {
                    written = snprintf(p, remaining, "\"%s\"", arr->data.string_vals[i]);
                } else {
                    written = snprintf(p, remaining, "null");
                }
                break;

            default:
                written = snprintf(p, remaining, "?");
                break;
        }

        if (written < 0 || (size_t)written >= remaining) {
            LOG_ERROR("config_array_to_string: buffer overflow prevented");
            nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);
            nimcp_free(buffer);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_array_to_string: capacity exceeded");
            return NULL;
        }
        p += written;
        remaining -= (size_t)written;
    }

    written = snprintf(p, remaining, "]");
    if (written < 0 || (size_t)written >= remaining) {
        LOG_ERROR("config_array_to_string: buffer overflow prevented");
        nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);
        nimcp_free(buffer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_array_to_string: capacity exceeded");
        return NULL;
    }
    nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);

    return buffer;
}

//=============================================================================
// Integration with Dynamic Config API
//=============================================================================

// Note: These are stubs. Full integration requires modifying nimcp_dynamic_config.c
// to add CONFIG_TYPE_ARRAY support to the config_value_type_t enum.

bool config_set_array(const char* key, const config_array_t* arr) {
    if (!key || !validate_array(arr)) {
        LOG_ERROR("config_set_array: invalid key or array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_set_array: required parameter is NULL (key, validate_array)");
        return false;
    }

    // TODO: Implement integration with dynamic config system
    // This requires adding CONFIG_TYPE_ARRAY to config_value_type_t
    LOG_WARN("config_set_array: not yet integrated with dynamic config system");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_set_array: required parameter is NULL (key, validate_array)");
    return false;
}

const config_array_t* config_get_array(const char* key) {
    if (!key) {
        LOG_ERROR("config_get_array: NULL key");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_get_array: key is NULL");
        return NULL;
    }

    // TODO: Implement integration with dynamic config system
    LOG_WARN("config_get_array: not yet integrated with dynamic config system");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_get_array: key is NULL");
    return NULL;
}

//=============================================================================
// Validation API
//=============================================================================

bool config_array_is_valid(const config_array_t* arr) {
    return validate_array(arr);
}

size_t config_array_memory_usage(const config_array_t* arr) {
    if (!validate_array(arr)) return 0;

    nimcp_platform_rwlock_rdlock((nimcp_platform_rwlock_t*)arr->rwlock);

    size_t usage = sizeof(config_array_t);
    usage += sizeof(nimcp_platform_rwlock_t);

    switch (arr->element_type) {
        case CONFIG_TYPE_INT:
            usage += arr->capacity * sizeof(int64_t);
            break;

        case CONFIG_TYPE_FLOAT:
            usage += arr->capacity * sizeof(double);
            break;

        case CONFIG_TYPE_BOOL:
            usage += arr->capacity * sizeof(bool);
            break;

        case CONFIG_TYPE_STRING:
            usage += arr->capacity * sizeof(char*);
            for (size_t i = 0; i < arr->count; i++) {
                if (arr->data.string_vals[i]) {
                    usage += strlen(arr->data.string_vals[i]) + 1;
                }
            }
            break;
    }

    nimcp_platform_rwlock_unlock((nimcp_platform_rwlock_t*)arr->rwlock);
    return usage;
}

//=============================================================================
// Config System Integration Helpers
//=============================================================================

size_t config_get_array_size(const char* key) {
    const config_array_t* arr = config_get_array(key);
    return arr ? config_array_size(arr) : 0;
}

int64_t config_get_array_int_at(const char* key, size_t index, int64_t default_value) {
    const config_array_t* arr = config_get_array(key);
    if (!arr) return default_value;
    return config_array_get_int(arr, index, default_value);
}

double config_get_array_float_at(const char* key, size_t index, double default_value) {
    const config_array_t* arr = config_get_array(key);
    if (!arr) return default_value;
    return config_array_get_float(arr, index, default_value);
}

bool config_get_array_bool_at(const char* key, size_t index, bool default_value) {
    const config_array_t* arr = config_get_array(key);
    if (!arr) return default_value;
    return config_array_get_bool(arr, index, default_value);
}

const char* config_get_array_string_at(const char* key, size_t index, const char* default_value) {
    const config_array_t* arr = config_get_array(key);
    if (!arr) return default_value;
    return config_array_get_string(arr, index, default_value);
}
