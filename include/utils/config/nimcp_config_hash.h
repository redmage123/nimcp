//=============================================================================
// nimcp_config_hash.h - High-Performance Hash Table for Config Module
//=============================================================================
/**
 * @file nimcp_config_hash.h
 * @brief Thread-safe hash table with O(1) lookups for configuration storage
 *
 * WHAT: High-performance hash table replacing O(n) linear search with O(1) lookups
 * WHY:  Config lookups are on critical path - linear search doesn't scale
 * HOW:  FNV-1a hashing + open addressing + dynamic resizing + thread safety
 *
 * ARCHITECTURE:
 *
 *   Hash Table Structure:
 *   ┌──────────────────────────────────────────────────────────────────────┐
 *   │                        Config Hash Table                             │
 *   │  ┌────────────────────────────────────────────────────────────────┐  │
 *   │  │  Buckets Array (dynamically resized)                          │  │
 *   │  │  ┌────────┬────────┬────────┬────────┬────────┬────────┐      │  │
 *   │  │  │ Entry  │ Entry  │ NULL   │ Entry  │ NULL   │ Entry  │ ...  │  │
 *   │  │  └───┬────┴────────┴────────┴────────┴────────┴────────┘      │  │
 *   │  └──────┼─────────────────────────────────────────────────────────┘  │
 *   │         │                                                             │
 *   │         ▼                                                             │
 *   │    ┌─────────────────┐                                               │
 *   │    │  Hash Entry     │                                               │
 *   │    │  - key (string) │                                               │
 *   │    │  - value (union)│                                               │
 *   │    │  - type (enum)  │                                               │
 *   │    │  - occupied     │                                               │
 *   │    │  - deleted      │                                               │
 *   │    └─────────────────┘                                               │
 *   │                                                                       │
 *   │  pthread_rwlock_t (thread safety)                                    │
 *   │  Unified Memory (nimcp_unified_alloc)                                │
 *   │  Security Integration (registered module)                            │
 *   └──────────────────────────────────────────────────────────────────────┘
 *
 * FEATURES:
 * - FNV-1a hash function (fast, good distribution)
 * - Open addressing with linear probing
 * - Dynamic resizing when load factor > 0.75
 * - Thread-safe operations (pthread_rwlock)
 * - Unified memory management
 * - Security module registration
 * - Copy-on-write snapshot support
 * - Comprehensive logging
 *
 * PERFORMANCE:
 * - Insert: O(1) average, O(n) worst case (degenerate hash)
 * - Lookup: O(1) average, O(n) worst case
 * - Delete: O(1) average, O(n) worst case
 * - Resize: O(n) when triggered
 *
 * THREAD SAFETY:
 * - Multiple concurrent readers (shared lock)
 * - Exclusive writer (exclusive lock)
 * - Lock-free reads when load factor < 0.75
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 * @version 1.0.0
 */

#ifndef NIMCP_CONFIG_HASH_H
#define NIMCP_CONFIG_HASH_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

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

/**
 * @brief Default initial capacity (must be power of 2)
 */
#define CONFIG_HASH_DEFAULT_CAPACITY 64

/**
 * @brief Maximum load factor before resize (0.75 = 75%)
 */
#define CONFIG_HASH_MAX_LOAD_FACTOR 0.75

/**
 * @brief Minimum capacity (must be power of 2)
 */
#define CONFIG_HASH_MIN_CAPACITY 16

/**
 * @brief Maximum capacity
 */
#define CONFIG_HASH_MAX_CAPACITY (1u << 30)

/**
 * @brief Maximum key length
 */
#define CONFIG_HASH_MAX_KEY_LEN 256

/**
 * @brief Magic value for validation
 */
#define CONFIG_HASH_MAGIC 0x43464748  // 'CFGH'

//=============================================================================
// Types and Enumerations
//=============================================================================

/**
 * @brief Configuration value types
 *
 * Matches types supported by NIMCP config system.
 */
typedef enum {
    CONFIG_VALUE_INT = 0,       /**< Integer value */
    CONFIG_VALUE_UINT,          /**< Unsigned integer value */
    CONFIG_VALUE_FLOAT,         /**< Floating-point value */
    CONFIG_VALUE_DOUBLE,        /**< Double-precision float */
    CONFIG_VALUE_BOOL,          /**< Boolean value */
    CONFIG_VALUE_STRING,        /**< String value (allocated) */
    CONFIG_VALUE_INVALID        /**< Invalid/uninitialized */
} config_value_type_t;

/**
 * @brief Configuration value union
 *
 * Stores different value types in a space-efficient manner.
 */
typedef union {
    int64_t i;                  /**< Integer value */
    uint64_t u;                 /**< Unsigned integer value */
    float f;                    /**< Float value */
    double d;                   /**< Double value */
    bool b;                     /**< Boolean value */
    char* s;                    /**< String value (heap-allocated) */
} config_value_t;

/**
 * @brief Hash table handle (opaque)
 */
typedef struct config_hash_table* config_hash_table_t;

/**
 * @brief Iterator callback function type
 *
 * @param key Configuration key
 * @param value Configuration value
 * @param type Value type
 * @param user_data User-provided context
 */
typedef void (*config_hash_iterator_t)(
    const char* key,
    const config_value_t* value,
    config_value_type_t type,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create hash table
 *
 * WHAT: Creates a new hash table with specified initial capacity
 * WHY:  Enable efficient config storage with O(1) lookups
 * HOW:  Allocates buckets array, initializes lock, registers with security
 *
 * @param initial_capacity Initial capacity (0 for default)
 * @return Hash table handle or NULL on failure
 *
 * COMPLEXITY: O(n) where n = initial_capacity
 * THREAD SAFETY: Thread-safe
 *
 * EXAMPLE:
 * ```c
 * config_hash_table_t table = config_hash_create(128);
 * if (!table) {
 *     LOG_ERROR("Failed to create config hash table");
 *     return NULL;
 * }
 * ```
 */
NIMCP_EXPORT config_hash_table_t config_hash_create(size_t initial_capacity);

/**
 * @brief Destroy hash table
 *
 * WHAT: Frees all resources associated with the hash table
 * WHY:  Clean shutdown and memory reclamation
 * HOW:  Frees all entries, strings, destroys lock, unregisters from security
 *
 * @param table Hash table handle
 *
 * COMPLEXITY: O(n) where n = capacity
 * THREAD SAFETY: Not thread-safe (caller must ensure no concurrent access)
 */
NIMCP_EXPORT void config_hash_destroy(config_hash_table_t table);

//=============================================================================
// Core Operations
//=============================================================================

/**
 * @brief Set configuration value
 *
 * WHAT: Inserts or updates a key-value pair in the hash table
 * WHY:  Store configuration settings with fast lookup
 * HOW:  Hash key, probe for slot, insert/update entry, resize if needed
 *
 * @param table Hash table handle
 * @param key Configuration key (null-terminated string)
 * @param value Configuration value
 * @param type Value type
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(1) average, O(n) worst case
 * THREAD SAFETY: Thread-safe (exclusive lock)
 *
 * EXAMPLE:
 * ```c
 * config_value_t val;
 * val.f = 0.01f;
 * config_hash_set(table, "learning_rate", &val, CONFIG_VALUE_FLOAT);
 * ```
 */
NIMCP_EXPORT bool config_hash_set(
    config_hash_table_t table,
    const char* key,
    const config_value_t* value,
    config_value_type_t type
);

/**
 * @brief Get configuration value
 *
 * WHAT: Retrieves value associated with key from hash table
 * WHY:  Fast O(1) config lookups
 * HOW:  Hash key, probe for entry, copy value if found
 *
 * @param table Hash table handle
 * @param key Configuration key
 * @param value Output: configuration value
 * @param type Output: value type (may be NULL)
 * @return true if found, false if not found
 *
 * COMPLEXITY: O(1) average, O(n) worst case
 * THREAD SAFETY: Thread-safe (shared lock)
 *
 * EXAMPLE:
 * ```c
 * config_value_t val;
 * config_value_type_t type;
 * if (config_hash_get(table, "learning_rate", &val, &type)) {
 *     if (type == CONFIG_VALUE_FLOAT) {
 *         float lr = val.f;
 *     }
 * }
 * ```
 */
NIMCP_EXPORT bool config_hash_get(
    config_hash_table_t table,
    const char* key,
    config_value_t* value,
    config_value_type_t* type
);

/**
 * @brief Remove configuration entry
 *
 * WHAT: Removes key-value pair from hash table
 * WHY:  Delete obsolete configuration entries
 * HOW:  Hash key, find entry, mark as deleted (tombstone)
 *
 * @param table Hash table handle
 * @param key Configuration key
 * @return true if removed, false if not found
 *
 * COMPLEXITY: O(1) average, O(n) worst case
 * THREAD SAFETY: Thread-safe (exclusive lock)
 *
 * NOTE: Uses tombstone deletion to maintain probe chains
 */
NIMCP_EXPORT bool config_hash_remove(
    config_hash_table_t table,
    const char* key
);

/**
 * @brief Check if key exists
 *
 * WHAT: Tests whether a key is present in the hash table
 * WHY:  Fast existence check without copying value
 * HOW:  Hash key, probe for entry
 *
 * @param table Hash table handle
 * @param key Configuration key
 * @return true if key exists, false otherwise
 *
 * COMPLEXITY: O(1) average, O(n) worst case
 * THREAD SAFETY: Thread-safe (shared lock)
 */
NIMCP_EXPORT bool config_hash_contains(
    config_hash_table_t table,
    const char* key
);

/**
 * @brief Get number of entries
 *
 * @param table Hash table handle
 * @return Number of key-value pairs stored
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe (shared lock)
 */
NIMCP_EXPORT size_t config_hash_size(config_hash_table_t table);

//=============================================================================
// Iteration
//=============================================================================

/**
 * @brief Iterate over all entries
 *
 * WHAT: Visits each key-value pair in the hash table
 * WHY:  Enable bulk operations on config entries
 * HOW:  Walks buckets array, calls callback for occupied entries
 *
 * @param table Hash table handle
 * @param iter Iterator callback function
 * @param user_data User context for callback
 *
 * COMPLEXITY: O(capacity) = O(n / load_factor)
 * THREAD SAFETY: Thread-safe (shared lock for iteration)
 *
 * EXAMPLE:
 * ```c
 * void print_entry(const char* key, const config_value_t* val,
 *                  config_value_type_t type, void* user_data) {
 *     printf("%s = ", key);
 *     if (type == CONFIG_VALUE_FLOAT) printf("%f\n", val->f);
 * }
 * config_hash_iterate(table, print_entry, NULL);
 * ```
 */
NIMCP_EXPORT void config_hash_iterate(
    config_hash_table_t table,
    config_hash_iterator_t iter,
    void* user_data
);

//=============================================================================
// Snapshot and Swap (for atomic config updates)
//=============================================================================

/**
 * @brief Create snapshot of hash table
 *
 * WHAT: Creates a deep copy of the entire hash table
 * WHY:  Enable atomic config updates via copy-on-write
 * HOW:  Allocates new table, copies all entries
 *
 * @param table Hash table to snapshot
 * @return New hash table or NULL on failure
 *
 * COMPLEXITY: O(n)
 * THREAD SAFETY: Thread-safe (shared lock on source)
 *
 * EXAMPLE:
 * ```c
 * // Atomic config update pattern
 * config_hash_table_t new_table = config_hash_snapshot(current);
 * config_hash_set(new_table, "key", &val, type);  // Modify copy
 * config_hash_swap(&current, &new_table);         // Atomic swap
 * config_hash_destroy(new_table);                 // Free old table
 * ```
 */
NIMCP_EXPORT config_hash_table_t config_hash_snapshot(
    config_hash_table_t table
);

/**
 * @brief Swap two hash tables atomically
 *
 * WHAT: Atomically exchanges two hash table pointers
 * WHY:  Enable lock-free config updates for readers
 * HOW:  Simple pointer swap with memory barrier
 *
 * @param t1 Pointer to first table handle
 * @param t2 Pointer to second table handle
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe (atomic operation)
 *
 * NOTE: Caller must ensure no other threads are modifying tables
 */
NIMCP_EXPORT void config_hash_swap(
    config_hash_table_t* t1,
    config_hash_table_t* t2
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get value type name
 *
 * @param type Value type enum
 * @return Human-readable type name
 */
static inline const char* config_value_type_name(config_value_type_t type) {
    switch (type) {
        case CONFIG_VALUE_INT:     return "int";
        case CONFIG_VALUE_UINT:    return "uint";
        case CONFIG_VALUE_FLOAT:   return "float";
        case CONFIG_VALUE_DOUBLE:  return "double";
        case CONFIG_VALUE_BOOL:    return "bool";
        case CONFIG_VALUE_STRING:  return "string";
        case CONFIG_VALUE_INVALID: return "invalid";
        default:                   return "unknown";
    }
}

/**
 * @brief Get current load factor
 *
 * @param table Hash table handle
 * @return Load factor (0.0 to 1.0)
 */
NIMCP_EXPORT double config_hash_load_factor(config_hash_table_t table);

/**
 * @brief Get capacity
 *
 * @param table Hash table handle
 * @return Current capacity (number of buckets)
 */
NIMCP_EXPORT size_t config_hash_capacity(config_hash_table_t table);

/**
 * @brief Get security module ID
 *
 * @param table Hash table handle
 * @return Security module ID or 0 if not registered
 */
NIMCP_EXPORT uint32_t config_hash_get_security_id(config_hash_table_t table);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_CONFIG_HASH_H
