/**
 * @file nimcp_hash_table.h
 * @brief Generic hash table implementation for NIMCP
 *
 * WHAT: Generic hash table with configurable key types and hash functions
 * WHY: Consolidate 4 duplicate hash table implementations across codebase
 * HOW: Separate chaining collision resolution with pluggable hash functions
 *
 * USAGE:
 *   // String keys (case-insensitive)
 *   hash_table_config_t config = {
 *       .initial_buckets = 256,
 *       .key_type = HASH_KEY_STRING,
 *       .hash_algorithm = HASH_ALG_FNV1A,
 *       .case_insensitive = true
 *   };
 *   hash_table_t* table = hash_table_create(&config);
 *   hash_table_insert_string(table, "concept_key", value_ptr, value_size);
 *   void* val = hash_table_lookup_string(table, "concept_key");
 *   hash_table_destroy(table);
 *
 *   // Integer keys
 *   hash_table_config_t config = {
 *       .initial_buckets = 256,
 *       .key_type = HASH_KEY_UINT32,
 *       .hash_algorithm = HASH_ALG_MURMUR3
 *   };
 *   hash_table_t* table = hash_table_create(&config);
 *   hash_table_insert_uint32(table, 12345, value_ptr, value_size);
 *
 * PERFORMANCE:
 *   - O(1) average case insert/lookup/delete
 *   - O(n) worst case with many collisions
 *   - Memory: sizeof(bucket_array) + chain_nodes
 */

#ifndef NIMCP_HASH_TABLE_H
#define NIMCP_HASH_TABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * WHAT: Opaque hash table handle
 * WHY: Hide implementation details from users
 * HOW: Forward declaration, defined in .c file
 */
typedef struct hash_table_t hash_table_t;

/**
 * WHAT: Key type enumeration
 * WHY: Support different key types with appropriate hashing
 * HOW: Tag in config determines key handling
 */
typedef enum {
    HASH_KEY_STRING,  // Null-terminated string keys
    HASH_KEY_UINT32,  // 32-bit unsigned integer keys
    HASH_KEY_UINT64,  // 64-bit unsigned integer keys
    HASH_KEY_CUSTOM   // Custom key type with user-provided hash/compare
} hash_key_type_t;

/**
 * WHAT: Hash algorithm selection
 * WHY: Different algorithms have different performance characteristics
 * HOW: FNV-1a (strings), djb2 (strings), MurmurHash3 (integers)
 */
typedef enum {
    HASH_ALG_FNV1A,    // Fast, good distribution for strings
    HASH_ALG_DJB2,     // Simple, fast for strings
    HASH_ALG_MURMUR3,  // Excellent distribution for integers
    HASH_ALG_CUSTOM    // User-provided hash function
} hash_algorithm_t;

/**
 * WHAT: Custom hash function pointer
 * WHY: Allow users to provide their own hash algorithms
 * HOW: Takes key data and size, returns hash value
 *
 * @param key Pointer to key data
 * @param key_size Size of key data in bytes
 * @return Hash value
 */
typedef uint32_t (*hash_function_t)(const void* key, size_t key_size);

/**
 * WHAT: Custom key comparison function pointer
 * WHY: Allow users to provide their own comparison logic
 * HOW: Returns true if keys are equal
 *
 * @param key1 First key
 * @param key1_size Size of first key
 * @param key2 Second key
 * @param key2_size Size of second key
 * @return true if keys are equal, false otherwise
 */
typedef bool (*key_compare_fn_t)(const void* key1, size_t key1_size, const void* key2,
                                 size_t key2_size);

/**
 * WHAT: Value destructor function pointer
 * WHY: Allow custom cleanup for stored values
 * HOW: Called when entry is removed or table destroyed
 *
 * @param value Pointer to value being destroyed
 * @param value_size Size of value in bytes
 */
typedef void (*value_destructor_fn_t)(void* value, size_t value_size);

/**
 * WHAT: Hash table configuration structure
 * WHY: Flexible initialization with sensible defaults
 * HOW: Pass to hash_table_create()
 */
typedef struct {
    /** WHAT: Number of buckets in hash table
     *  WHY: Affects collision rate and memory usage
     *  NOTE: Should be power of 2 for optimal performance */
    size_t initial_buckets;

    /** WHAT: Type of keys stored in table
     *  WHY: Determines hashing and comparison strategy */
    hash_key_type_t key_type;

    /** WHAT: Hash algorithm to use
     *  WHY: Different algorithms for different key types */
    hash_algorithm_t hash_algorithm;

    /** WHAT: Custom hash function (if HASH_ALG_CUSTOM)
     *  WHY: Allow user-defined hashing */
    hash_function_t custom_hash_fn;

    /** WHAT: Custom key comparison (if HASH_KEY_CUSTOM)
     *  WHY: Allow user-defined key equality */
    key_compare_fn_t custom_compare_fn;

    /** WHAT: Optional value destructor
     *  WHY: Automatic cleanup of stored values */
    value_destructor_fn_t value_destructor;

    /** WHAT: Case-insensitive string keys (only for STRING keys)
     *  WHY: Support case-insensitive lookups like curiosity engine */
    bool case_insensitive;

    /** WHAT: Thread-safe operations (future feature)
     *  WHY: Enable concurrent access
     *  NOTE: Currently not implemented */
    bool thread_safe;
} hash_table_config_t;

/**
 * WHAT: Iterator callback function
 * WHY: Allow traversal of all entries
 * HOW: Called for each entry during iteration
 *
 * @param key Pointer to entry key
 * @param key_size Size of key in bytes
 * @param value Pointer to entry value
 * @param value_size Size of value in bytes
 * @param user_data User-provided context pointer
 * @return true to continue iteration, false to stop
 */
typedef bool (*hash_table_iterator_fn_t)(const void* key, size_t key_size, void* value,
                                         size_t value_size, void* user_data);

//=============================================================================
// Core Operations
//=============================================================================

/**
 * WHAT: Create a new hash table
 * WHY: Initialize hash table with specified configuration
 * HOW: Allocate bucket array and initialize based on config
 *
 * @param config Configuration structure (NULL for defaults)
 * @return New hash table handle, or NULL on allocation failure
 *
 * DEFAULTS (if config is NULL):
 *   - 256 buckets
 *   - STRING keys
 *   - FNV1A hash
 *   - Case-sensitive
 *   - No thread safety
 */
hash_table_t* hash_table_create(const hash_table_config_t* config);

/**
 * WHAT: Destroy hash table and free all memory
 * WHY: Clean up resources
 * HOW: Free all chains, call value destructors, free table
 *
 * @param table Hash table to destroy (NULL is safe)
 *
 * NOTE: Calls value_destructor for each entry if configured
 */
void hash_table_destroy(hash_table_t* table);

/**
 * WHAT: Get number of entries in table
 * WHY: Track table size
 * HOW: Maintained counter updated on insert/remove
 *
 * @param table Hash table
 * @return Number of entries, or 0 if table is NULL
 */
size_t hash_table_size(const hash_table_t* table);

/**
 * WHAT: Get number of buckets in table
 * WHY: Calculate load factor and collision metrics
 * HOW: Return bucket array size
 *
 * @param table Hash table
 * @return Number of buckets, or 0 if table is NULL
 */
size_t hash_table_bucket_count(const hash_table_t* table);

/**
 * WHAT: Remove all entries from table
 * WHY: Reset table without destroying it
 * HOW: Free all chains, call destructors
 *
 * @param table Hash table to clear
 *
 * NOTE: Calls value_destructor for each entry if configured
 */
void hash_table_clear(hash_table_t* table);

//=============================================================================
// String Key Operations
//=============================================================================

/**
 * WHAT: Insert or update string key entry
 * WHY: Store value with string key
 * HOW: Hash key, find/create entry, copy value
 *
 * @param table Hash table
 * @param key Null-terminated string key
 * @param value Pointer to value data to store (will be copied)
 * @param value_size Size of value in bytes
 * @return true on success, false on allocation failure
 *
 * NOTE: If key exists, old value is replaced (destructor called if configured)
 * NOTE: Value data is copied, so caller retains ownership of input
 */
bool hash_table_insert_string(hash_table_t* table, const char* key, const void* value,
                              size_t value_size);

/**
 * WHAT: Lookup value by string key
 * WHY: Retrieve stored value
 * HOW: Hash key, traverse chain, compare keys
 *
 * @param table Hash table
 * @param key Null-terminated string key
 * @return Pointer to stored value, or NULL if not found
 *
 * NOTE: Returns pointer to internal storage, valid until entry removed
 */
void* hash_table_lookup_string(hash_table_t* table, const char* key);

/**
 * WHAT: Remove entry by string key
 * WHY: Delete entry from table
 * HOW: Hash key, find entry, unlink from chain, free
 *
 * @param table Hash table
 * @param key Null-terminated string key
 * @return true if entry was found and removed, false if not found
 *
 * NOTE: Calls value_destructor if configured
 */
bool hash_table_remove_string(hash_table_t* table, const char* key);

//=============================================================================
// Integer Key Operations (uint32_t)
//=============================================================================

/**
 * WHAT: Insert or update uint32_t key entry
 * WHY: Store value with integer key
 * HOW: Hash key, find/create entry, copy value
 *
 * @param table Hash table
 * @param key 32-bit unsigned integer key
 * @param value Pointer to value data to store (will be copied)
 * @param value_size Size of value in bytes
 * @return true on success, false on allocation failure
 */
bool hash_table_insert_uint32(hash_table_t* table, uint32_t key, const void* value,
                              size_t value_size);

/**
 * WHAT: Lookup value by uint32_t key
 * WHY: Retrieve stored value
 * HOW: Hash key, traverse chain, compare keys
 *
 * @param table Hash table
 * @param key 32-bit unsigned integer key
 * @return Pointer to stored value, or NULL if not found
 */
void* hash_table_lookup_uint32(hash_table_t* table, uint32_t key);

/**
 * WHAT: Remove entry by uint32_t key
 * WHY: Delete entry from table
 * HOW: Hash key, find entry, unlink from chain, free
 *
 * @param table Hash table
 * @param key 32-bit unsigned integer key
 * @return true if entry was found and removed, false if not found
 */
bool hash_table_remove_uint32(hash_table_t* table, uint32_t key);

//=============================================================================
// Integer Key Operations (uint64_t)
//=============================================================================

/**
 * WHAT: Insert or update uint64_t key entry
 * WHY: Store value with 64-bit integer key without truncation
 * HOW: Hash key, find/create entry, copy value
 *
 * @param table Hash table (should be created with HASH_KEY_UINT64)
 * @param key 64-bit unsigned integer key
 * @param value Pointer to value data to store (will be copied)
 * @param value_size Size of value in bytes
 * @return true on success, false on allocation failure
 */
bool hash_table_insert_uint64(hash_table_t* table, uint64_t key, const void* value,
                              size_t value_size);

/**
 * WHAT: Lookup value by uint64_t key
 * WHY: Retrieve stored value using full 64-bit key
 * HOW: Hash key, traverse chain, compare keys
 *
 * @param table Hash table
 * @param key 64-bit unsigned integer key
 * @return Pointer to stored value, or NULL if not found
 */
void* hash_table_lookup_uint64(hash_table_t* table, uint64_t key);

/**
 * WHAT: Remove entry by uint64_t key
 * WHY: Delete entry from table using full 64-bit key
 * HOW: Hash key, find entry, unlink from chain, free
 *
 * @param table Hash table
 * @param key 64-bit unsigned integer key
 * @return true if entry was found and removed, false if not found
 */
bool hash_table_remove_uint64(hash_table_t* table, uint64_t key);

//=============================================================================
// Iteration
//=============================================================================

/**
 * WHAT: Iterate over all entries in table
 * WHY: Process all stored values
 * HOW: Traverse all buckets and chains, call callback
 *
 * @param table Hash table
 * @param callback Function to call for each entry
 * @param user_data Opaque pointer passed to callback
 *
 * NOTE: Order is not guaranteed (depends on hash values)
 * NOTE: Do not modify table structure during iteration
 */
void hash_table_iterate(hash_table_t* table, hash_table_iterator_fn_t callback, void* user_data);

//=============================================================================
// Statistics and Debugging
//=============================================================================

/**
 * WHAT: Get collision statistics
 * WHY: Diagnose performance issues
 * HOW: Count entries in each bucket
 *
 * @param table Hash table
 * @param max_chain_length [OUT] Longest chain length
 * @param avg_chain_length [OUT] Average chain length (only non-empty buckets)
 * @param empty_buckets [OUT] Number of empty buckets
 */
void hash_table_stats(const hash_table_t* table, size_t* max_chain_length, float* avg_chain_length,
                      size_t* empty_buckets);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HASH_TABLE_H */
