//=============================================================================
// nimcp_config_array.h - Array Type Support for NIMCP Config
//=============================================================================
/**
 * @file nimcp_config_array.h
 * @brief Homogeneous array type support for dynamic configuration
 *
 * WHAT: Array configuration values with dynamic sizing and thread-safe access
 * WHY:  Support complex config like layer sizes, learning rates, neuron IDs
 * HOW:  Homogeneous arrays with unified memory and JSON/INI parsing
 *
 * ARCHITECTURE:
 *
 *   Config Array:
 *   ┌──────────────────────────────────────────────────────────┐
 *   │  config_array_t                                          │
 *   │  ┌────────────────────────────────────────────┐          │
 *   │  │ element_type: CONFIG_TYPE_INT              │          │
 *   │  │ count: 5                                   │          │
 *   │  │ capacity: 10                               │          │
 *   │  └────────────────────────────────────────────┘          │
 *   │                    │                                     │
 *   │                    ▼                                     │
 *   │  ┌──────────────────────────────────────────────────┐   │
 *   │  │  Unified Memory Handle                           │   │
 *   │  │  [0] [1] [2] [3] [4] [ ] [ ] [ ] [ ] [ ]         │   │
 *   │  └──────────────────────────────────────────────────┘   │
 *   └──────────────────────────────────────────────────────────┘
 *
 * FEATURES:
 * - Homogeneous arrays (all elements same type)
 * - Dynamic growth with capacity management
 * - Thread-safe access via RW locks
 * - Parse from INI: key = [1, 2, 3]
 * - Parse from JSON: "key": [1, 2, 3]
 * - Unified memory integration for CoW and security
 * - Bounds checking and type safety
 * - Clone support (CoW-optimized)
 * - Serialization to string
 *
 * SUPPORTED TYPES:
 * - CONFIG_TYPE_INT: int64_t arrays
 * - CONFIG_TYPE_FLOAT: double arrays
 * - CONFIG_TYPE_BOOL: bool arrays
 * - CONFIG_TYPE_STRING: char* arrays
 *
 * USAGE EXAMPLES:
 *
 * 1. Create and populate:
 * ```c
 * config_array_t* arr = config_array_create(CONFIG_TYPE_INT, 10);
 * config_array_append_int(arr, 256);
 * config_array_append_int(arr, 512);
 * config_array_append_int(arr, 1024);
 * config_set_array("layer_sizes", arr);
 * ```
 *
 * 2. Parse from INI:
 * ```ini
 * layer_sizes = [256, 512, 1024]
 * learning_rates = [0.001, 0.01, 0.1]
 * enabled_modules = [true, false, true]
 * region_names = ["V1", "V2", "V4", "IT"]
 * ```
 *
 * 3. Access values:
 * ```c
 * const config_array_t* arr = config_get_array("layer_sizes");
 * for (size_t i = 0; i < arr->count; i++) {
 *     int64_t size = config_array_get_int(arr, i, 0);
 *     printf("Layer %zu: %ld neurons\n", i, size);
 * }
 * ```
 *
 * THREAD SAFETY:
 * - All operations are thread-safe
 * - Read-many/write-exclusive via RW locks
 * - CoW semantics for cloned arrays
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 * @version 1.0.0
 */

#ifndef NIMCP_CONFIG_ARRAY_H
#define NIMCP_CONFIG_ARRAY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "utils/config/nimcp_dynamic_config.h"
#include "utils/memory/nimcp_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants and Configuration
//=============================================================================

/** @brief Default initial capacity for arrays */
#define CONFIG_ARRAY_DEFAULT_CAPACITY 16

/** @brief Growth factor when expanding (2x) */
#define CONFIG_ARRAY_GROWTH_FACTOR 2

/** @brief Maximum array size (safety limit) */
#define CONFIG_ARRAY_MAX_SIZE (1024 * 1024)

/** @brief Magic value for validation */
#define CONFIG_ARRAY_MAGIC 0x41525259  // 'ARRY'

//=============================================================================
// Types and Structures
//=============================================================================

/**
 * @brief Configuration array structure (full definition)
 *
 * WHAT: Homogeneous array of config values with thread-safe access
 * WHY:  Support multi-value config parameters efficiently
 * HOW:  Type-tagged union with unified memory backing and RW locks
 *
 * NOTE: This is the full definition - config_array_t is typedef'd to this
 *       in nimcp_dynamic_config.h
 */
struct config_array_full {
    uint32_t magic;                 /**< Magic value for validation */
    config_value_type_t element_type; /**< Type of elements */
    size_t count;                   /**< Number of elements in use */
    size_t capacity;                /**< Total capacity */
    void* rwlock;                   /**< Read-write lock (pthread_rwlock_t*) */
    void* unified_mem_handle;       /**< Unified memory handle (optional) */

    // Data storage (only one is non-NULL based on element_type)
    union {
        int64_t* int_vals;          /**< Integer array data */
        double* float_vals;         /**< Float array data */
        bool* bool_vals;            /**< Boolean array data */
        char** string_vals;         /**< String array data (each element owned) */
    } data;
};

/** @brief Alias for full config array structure */
typedef struct config_array_full config_array_full_t;

//=============================================================================
// Array Lifecycle API
//=============================================================================

/**
 * @brief Create array with specified element type
 *
 * WHAT: Allocates array with initial capacity
 * WHY:  Initialize before populating
 * HOW:  Allocates backing storage via unified memory if available
 *
 * @param element_type Type of array elements
 * @param capacity Initial capacity (0 = default)
 * @return Array handle or NULL on failure
 *
 * COMPLEXITY: O(capacity)
 * MEMORY: sizeof(element_type) * capacity
 *
 * EXAMPLE:
 * ```c
 * config_array_full_t* layer_sizes = config_array_create(CONFIG_TYPE_INT, 10);
 * ```
 */
NIMCP_EXPORT config_array_full_t* config_array_create(
    config_value_type_t element_type,
    size_t capacity
);

/**
 * @brief Destroy array and free resources
 *
 * WHAT: Frees array and all owned data
 * WHY:  Clean up when done
 * HOW:  Frees strings individually, then array structure
 *
 * @param arr Array to destroy
 *
 * NOTE: Safe to call with NULL
 */
NIMCP_EXPORT void config_array_destroy(config_array_t* arr);

/**
 * @brief Clone array (CoW-optimized)
 *
 * WHAT: Creates a copy of the array
 * WHY:  Snapshot for rollback or parallel access
 * HOW:  Uses unified memory CoW if available, else deep copy
 *
 * @param arr Array to clone
 * @return Cloned array or NULL on failure
 *
 * COMPLEXITY: O(1) with CoW, O(n) without
 */
NIMCP_EXPORT config_array_t* config_array_clone(const config_array_t* arr);

/**
 * @brief Clear array contents (keep capacity)
 *
 * @param arr Array to clear
 *
 * NOTE: Frees strings but keeps backing storage
 */
NIMCP_EXPORT void config_array_clear(config_array_t* arr);

/**
 * @brief Reserve capacity without changing count
 *
 * @param arr Array to resize
 * @param new_capacity Minimum capacity
 * @return true on success, false on failure
 *
 * NOTE: Does not shrink array
 */
NIMCP_EXPORT bool config_array_reserve(config_array_t* arr, size_t new_capacity);

//=============================================================================
// Array Access API
//=============================================================================

/**
 * @brief Get array element type
 *
 * @param arr Array handle
 * @return Element type
 */
NIMCP_EXPORT config_value_type_t config_array_get_type(const config_array_t* arr);

/**
 * @brief Get array size
 *
 * @param arr Array handle
 * @return Number of elements
 */
NIMCP_EXPORT size_t config_array_size(const config_array_t* arr);

/**
 * @brief Get array capacity
 *
 * @param arr Array handle
 * @return Total capacity
 */
NIMCP_EXPORT size_t config_array_capacity(const config_array_t* arr);

/**
 * @brief Check if array is empty
 *
 * @param arr Array handle
 * @return true if count == 0
 */
NIMCP_EXPORT bool config_array_is_empty(const config_array_t* arr);

//=============================================================================
// Array Append API
//=============================================================================

/**
 * @brief Append integer to array
 *
 * @param arr Array handle (must be CONFIG_TYPE_INT)
 * @param val Value to append
 * @return true on success, false on type mismatch or error
 *
 * COMPLEXITY: O(1) amortized, O(n) when resizing
 */
NIMCP_EXPORT bool config_array_append_int(config_array_t* arr, int64_t val);

/**
 * @brief Append float to array
 *
 * @param arr Array handle (must be CONFIG_TYPE_FLOAT)
 * @param val Value to append
 * @return true on success, false on type mismatch or error
 */
NIMCP_EXPORT bool config_array_append_float(config_array_t* arr, double val);

/**
 * @brief Append boolean to array
 *
 * @param arr Array handle (must be CONFIG_TYPE_BOOL)
 * @param val Value to append
 * @return true on success, false on type mismatch or error
 */
NIMCP_EXPORT bool config_array_append_bool(config_array_t* arr, bool val);

/**
 * @brief Append string to array
 *
 * @param arr Array handle (must be CONFIG_TYPE_STRING)
 * @param val Value to append (copied internally)
 * @return true on success, false on type mismatch or error
 *
 * NOTE: String is copied and owned by the array
 */
NIMCP_EXPORT bool config_array_append_string(config_array_t* arr, const char* val);

//=============================================================================
// Array Get API
//=============================================================================

/**
 * @brief Get integer from array
 *
 * @param arr Array handle
 * @param idx Index to access
 * @param default_val Default if out of bounds or type mismatch
 * @return Value at index or default
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe (read lock)
 */
NIMCP_EXPORT int64_t config_array_get_int(
    const config_array_t* arr,
    size_t idx,
    int64_t default_val
);

/**
 * @brief Get float from array
 *
 * @param arr Array handle
 * @param idx Index to access
 * @param default_val Default if out of bounds or type mismatch
 * @return Value at index or default
 */
NIMCP_EXPORT double config_array_get_float(
    const config_array_t* arr,
    size_t idx,
    double default_val
);

/**
 * @brief Get boolean from array
 *
 * @param arr Array handle
 * @param idx Index to access
 * @param default_val Default if out of bounds or type mismatch
 * @return Value at index or default
 */
NIMCP_EXPORT bool config_array_get_bool(
    const config_array_t* arr,
    size_t idx,
    bool default_val
);

/**
 * @brief Get string from array
 *
 * @param arr Array handle
 * @param idx Index to access
 * @param default_val Default if out of bounds or type mismatch
 * @return Value at index or default (NOT owned by caller)
 *
 * WARNING: Returned pointer is valid until array is modified or destroyed
 */
NIMCP_EXPORT const char* config_array_get_string(
    const config_array_t* arr,
    size_t idx,
    const char* default_val
);

//=============================================================================
// Array Set API
//=============================================================================

/**
 * @brief Set integer at index
 *
 * @param arr Array handle
 * @param idx Index to modify
 * @param val New value
 * @return true on success, false on bounds or type error
 */
NIMCP_EXPORT bool config_array_set_int(config_array_t* arr, size_t idx, int64_t val);

/**
 * @brief Set float at index
 *
 * @param arr Array handle
 * @param idx Index to modify
 * @param val New value
 * @return true on success, false on bounds or type error
 */
NIMCP_EXPORT bool config_array_set_float(config_array_t* arr, size_t idx, double val);

/**
 * @brief Set boolean at index
 *
 * @param arr Array handle
 * @param idx Index to modify
 * @param val New value
 * @return true on success, false on bounds or type error
 */
NIMCP_EXPORT bool config_array_set_bool(config_array_t* arr, size_t idx, bool val);

/**
 * @brief Set string at index
 *
 * @param arr Array handle
 * @param idx Index to modify
 * @param val New value (copied)
 * @return true on success, false on bounds or type error
 */
NIMCP_EXPORT bool config_array_set_string(config_array_t* arr, size_t idx, const char* val);

//=============================================================================
// Array Modification API
//=============================================================================

/**
 * @brief Remove element at index
 *
 * WHAT: Delete element and shift remaining elements down
 * WHY:  Dynamically modify array contents
 * HOW:  Free element if string, memmove remaining, decrement count
 *
 * @param arr Array handle
 * @param idx Index to remove
 * @return true on success, false on bounds error
 *
 * COMPLEXITY: O(n - idx) due to shift
 *
 * EXAMPLE:
 * ```c
 * config_array_t* arr = config_parse_int_array("[10, 20, 30, 40]");
 * config_array_remove(arr, 1);  // Now [10, 30, 40]
 * ```
 */
NIMCP_EXPORT bool config_array_remove(config_array_t* arr, size_t idx);

/**
 * @brief Resize array capacity
 *
 * WHAT: Change allocated capacity (cannot shrink below count)
 * WHY:  Pre-allocate for performance or manage memory
 * HOW:  Reallocate storage, update capacity
 *
 * @param arr Array handle
 * @param new_capacity New capacity (must be >= count)
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(n) to copy elements
 *
 * NOTE: Cannot shrink below current count
 */
NIMCP_EXPORT bool config_array_resize(config_array_t* arr, size_t new_capacity);

//=============================================================================
// Parsing API
//=============================================================================

/**
 * @brief Parse integer array from string
 *
 * WHAT: Parses "[1, 2, 3]" or "1,2,3" into int array
 * WHY:  INI and JSON parsing support
 * HOW:  Tokenizes and parses each element
 *
 * @param str String to parse (e.g., "[1, 2, 3]")
 * @return Array or NULL on parse error
 *
 * EXAMPLE:
 * ```c
 * config_array_t* arr = config_parse_int_array("[256, 512, 1024]");
 * ```
 */
NIMCP_EXPORT config_array_t* config_parse_int_array(const char* str);

/**
 * @brief Parse float array from string
 *
 * @param str String to parse (e.g., "[0.1, 0.5, 0.9]")
 * @return Array or NULL on parse error
 */
NIMCP_EXPORT config_array_t* config_parse_float_array(const char* str);

/**
 * @brief Parse boolean array from string
 *
 * @param str String to parse (e.g., "[true, false, true]")
 * @return Array or NULL on parse error
 */
NIMCP_EXPORT config_array_t* config_parse_bool_array(const char* str);

/**
 * @brief Parse string array from string
 *
 * @param str String to parse (e.g., '["V1", "V2", "V4"]')
 * @return Array or NULL on parse error
 */
NIMCP_EXPORT config_array_t* config_parse_string_array(const char* str);

/**
 * @brief Parse array with auto-detected type
 *
 * WHAT: Automatically detect element type from first element
 * WHY:  Convenience when type is obvious from content
 * HOW:  Inspect first element, dispatch to appropriate parser
 *
 * @param str String to parse (e.g., "[1, 2, 3]" or "[true, false]")
 * @return Array or NULL on parse error
 *
 * TYPE DETECTION:
 * - Quoted string -> STRING
 * - "true"/"false" -> BOOL
 * - Contains '.' or 'e'/'E' -> FLOAT
 * - Numeric -> INT
 * - Empty -> INT (default)
 *
 * EXAMPLE:
 * ```c
 * config_array_t* arr = config_parse_array_auto("[0.1, 0.5, 0.9]");  // FLOAT
 * config_array_t* arr2 = config_parse_array_auto("[true, false]");   // BOOL
 * ```
 */
NIMCP_EXPORT config_array_t* config_parse_array_auto(const char* str);

//=============================================================================
// Serialization API
//=============================================================================

/**
 * @brief Convert array to string representation
 *
 * WHAT: Serializes array to "[val1, val2, ...]" format
 * WHY:  Config dumping and logging
 * HOW:  Formats each element, builds string
 *
 * @param arr Array to serialize
 * @return Allocated string (caller must free) or NULL on error
 *
 * EXAMPLE:
 * ```c
 * char* str = config_array_to_string(arr);
 * printf("layer_sizes = %s\n", str);
 * nimcp_free(str);
 * ```
 */
NIMCP_EXPORT char* config_array_to_string(const config_array_t* arr);

//=============================================================================
// Integration with Dynamic Config API
//=============================================================================

/**
 * @brief Set array in config system
 *
 * WHAT: Registers array under given key
 * WHY:  Make array accessible via config_get_array()
 * HOW:  Stores in config table with type CONFIG_TYPE_ARRAY
 *
 * @param key Config key
 * @param arr Array value (ownership transferred)
 * @return true on success, false on error
 *
 * NOTE: Config system takes ownership of array
 */
NIMCP_EXPORT bool config_set_array(const char* key, const config_array_t* arr);

/**
 * @brief Get array from config system
 *
 * @param key Config key
 * @return Array or NULL if not found
 *
 * WARNING: Returned pointer is owned by config system
 */
NIMCP_EXPORT const config_array_t* config_get_array(const char* key);

/**
 * @brief Get size of config array
 *
 * @param key Config key
 * @return Number of elements (0 if not found)
 *
 * EXAMPLE:
 * ```c
 * size_t num_layers = config_get_array_size("layer_sizes");
 * ```
 */
NIMCP_EXPORT size_t config_get_array_size(const char* key);

/**
 * @brief Get integer element from config array
 *
 * WHAT: Convenient single-call access to array element
 * WHY:  Avoid separate get_array + get_int calls
 * HOW:  Lookup array, check type, return element
 *
 * @param key Config key
 * @param index Element index
 * @param default_value Default if not found or out of bounds
 * @return Element value or default
 *
 * EXAMPLE:
 * ```c
 * // Config: layer_sizes = [256, 512, 1024]
 * int64_t hidden_size = config_get_array_int_at("layer_sizes", 1, 128);  // 512
 * ```
 */
NIMCP_EXPORT int64_t config_get_array_int_at(
    const char* key,
    size_t index,
    int64_t default_value
);

/**
 * @brief Get float element from config array
 *
 * @param key Config key
 * @param index Element index
 * @param default_value Default if not found
 * @return Element value or default
 */
NIMCP_EXPORT double config_get_array_float_at(
    const char* key,
    size_t index,
    double default_value
);

/**
 * @brief Get boolean element from config array
 *
 * @param key Config key
 * @param index Element index
 * @param default_value Default if not found
 * @return Element value or default
 */
NIMCP_EXPORT bool config_get_array_bool_at(
    const char* key,
    size_t index,
    bool default_value
);

/**
 * @brief Get string element from config array
 *
 * @param key Config key
 * @param index Element index
 * @param default_value Default if not found
 * @return Element value or default (NOT owned by caller)
 */
NIMCP_EXPORT const char* config_get_array_string_at(
    const char* key,
    size_t index,
    const char* default_value
);

//=============================================================================
// Validation API
//=============================================================================

/**
 * @brief Validate array handle
 *
 * @param arr Array to validate
 * @return true if valid, false if NULL or corrupted
 */
NIMCP_EXPORT bool config_array_is_valid(const config_array_t* arr);

/**
 * @brief Get memory usage of array
 *
 * @param arr Array handle
 * @return Bytes used by array and data
 */
NIMCP_EXPORT size_t config_array_memory_usage(const config_array_t* arr);

//=============================================================================
// Helper Macros
//=============================================================================

/**
 * @brief Iterate over integer array
 *
 * EXAMPLE:
 * ```c
 * const config_array_t* arr = config_get_array("layer_sizes");
 * CONFIG_ARRAY_FOREACH_INT(arr, i, val) {
 *     printf("Layer %zu: %ld\n", i, val);
 * }
 * ```
 */
#define CONFIG_ARRAY_FOREACH_INT(arr, idx_var, val_var) \
    for (size_t idx_var = 0, _n = config_array_size(arr); \
         idx_var < _n && (val_var = config_array_get_int(arr, idx_var, 0), 1); \
         idx_var++)

/**
 * @brief Iterate over float array
 */
#define CONFIG_ARRAY_FOREACH_FLOAT(arr, idx_var, val_var) \
    for (size_t idx_var = 0, _n = config_array_size(arr); \
         idx_var < _n && (val_var = config_array_get_float(arr, idx_var, 0.0), 1); \
         idx_var++)

/**
 * @brief Iterate over boolean array
 */
#define CONFIG_ARRAY_FOREACH_BOOL(arr, idx_var, val_var) \
    for (size_t idx_var = 0, _n = config_array_size(arr); \
         idx_var < _n && (val_var = config_array_get_bool(arr, idx_var, false), 1); \
         idx_var++)

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_CONFIG_ARRAY_H
