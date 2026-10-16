/**
 * @file nimcp_hash_table.c
 * @brief Generic hash table implementation
 *
 * WHAT: Hash table with separate chaining for collision resolution
 * WHY: Consolidate duplicate hash table code across NIMCP
 * HOW: Configurable key types, hash functions, and comparison strategies
 */

#include "utils/containers/nimcp_hash_table.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"  // CRITICAL: Declares nimcp_calloc/nimcp_free return types
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(hash_table)

//=============================================================================
// Internal Data Structures
//=============================================================================

/**
 * WHAT: Chain node for collision resolution
 * WHY: Separate chaining with linked list
 * HOW: Each bucket points to head of chain
 */
typedef struct hash_entry_t {
    void* key;                  // Key data (copied)
    size_t key_size;            // Size of key in bytes
    void* value;                // Value data (copied)
    size_t value_size;          // Size of value in bytes
    uint32_t hash;              // Cached hash value
    struct hash_entry_t* next;  // Next in chain
} hash_entry_t;

/**
 * WHAT: Hash table structure
 * WHY: Encapsulate all table state
 * HOW: Bucket array + configuration
 *
 * Dynamic resizing: When load factor exceeds 0.75, the table doubles its bucket
 * count and rehashes all entries. This maintains O(1) average-case lookup even
 * as entry count grows well beyond initial capacity.
 */
struct hash_table_t {
    hash_entry_t** buckets;      // Array of bucket heads
    size_t bucket_count;         // Number of buckets
    size_t entry_count;          // Number of entries
    hash_table_config_t config;  // Configuration
};

//=============================================================================
// Dynamic Resizing
//=============================================================================

/**
 * WHAT: Maximum load factor before triggering resize
 * WHY: 0.75 balances memory usage vs chain length
 * HOW: Checked after every successful insert of a new entry
 */
#define HASH_TABLE_MAX_LOAD_FACTOR 0.75f

/**
 * WHAT: Resize hash table by doubling bucket count and rehashing all entries
 * WHY: Keep load factor below threshold to maintain O(1) average lookup
 * HOW: Allocate new bucket array at 2x capacity, walk old buckets rehashing
 *      each entry into new array, then free old array
 *
 * @param table Hash table to resize
 * @return true on success, false on allocation failure (table unchanged)
 */
static bool hash_table_resize(hash_table_t* table)
{
    if (!table || !table->buckets) {
        return false;
    }

    /* Guard: prevent overflow when doubling */
    if (table->bucket_count > SIZE_MAX / 2) {
        LOG_WARN("nimcp_hash_table", "Cannot resize: bucket_count overflow");
        return false;
    }

    size_t new_bucket_count = table->bucket_count * 2;
    hash_entry_t** new_buckets = (hash_entry_t**)nimcp_calloc(new_bucket_count, sizeof(hash_entry_t*));
    if (!new_buckets) {
        LOG_WARN("nimcp_hash_table",
                 "Failed to allocate %zu buckets for resize, continuing with current capacity",
                 new_bucket_count);
        return false;  /* Non-fatal: table still works, just slower */
    }

    /* Rehash all entries into new bucket array */
    for (size_t i = 0; i < table->bucket_count; i++) {
        hash_entry_t* current = table->buckets[i];
        while (current) {
            hash_entry_t* next = current->next;

            /* Use cached hash to compute new bucket index */
            size_t new_index = current->hash % new_bucket_count;

            /* Insert at head of new chain */
            current->next = new_buckets[new_index];
            new_buckets[new_index] = current;

            current = next;
        }
    }

    /* Swap in new bucket array */
    nimcp_free(table->buckets);
    table->buckets = new_buckets;
    table->bucket_count = new_bucket_count;

    return true;
}

/**
 * WHAT: Check load factor and resize if needed
 * WHY: Called after each new entry insertion
 * HOW: If entries/buckets > 0.75, trigger resize
 */
static void hash_table_maybe_resize(hash_table_t* table)
{
    if (!table || table->bucket_count == 0) {
        return;
    }

    float load_factor = (float)table->entry_count / (float)table->bucket_count;
    if (load_factor > HASH_TABLE_MAX_LOAD_FACTOR) {
        hash_table_resize(table);
    }
}

//=============================================================================
// Hash Functions
//=============================================================================

/**
 * WHAT: FNV-1a hash algorithm
 * WHY: Fast, good distribution for strings
 * HOW: XOR with prime, multiply by prime
 */
static uint32_t hash_fnv1a(const void* key, size_t key_size)
{
    const uint8_t* data = (const uint8_t*) key;
    uint32_t hash = 2166136261U;  // FNV offset basis

    for (size_t i = 0; i < key_size; i++) {
        hash ^= data[i];
        hash *= 16777619U;  // FNV prime
    }

    return hash;
}

/**
 * WHAT: djb2 hash algorithm
 * WHY: Simple, fast for strings
 * HOW: hash = hash * 33 + byte
 */
static uint32_t hash_djb2(const void* key, size_t key_size)
{
    const uint8_t* data = (const uint8_t*) key;
    uint32_t hash = 5381;

    for (size_t i = 0; i < key_size; i++) {
        hash = ((hash << 5) + hash) + data[i];  // hash * 33 + byte
    }

    return hash;
}

/**
 * WHAT: MurmurHash3 32-bit finalizer
 * WHY: Excellent avalanche properties for integers
 * HOW: Bit mixing operations
 */
static uint32_t hash_murmur3_32(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

/**
 * WHAT: MurmurHash3 for arbitrary data
 * WHY: General-purpose hashing for any key type
 * HOW: Process 4-byte blocks, finalize with tail bytes
 */
static uint32_t hash_murmur3(const void* key, size_t key_size)
{
    const uint8_t* data = (const uint8_t*) key;
    const size_t nblocks = key_size / 4;
    uint32_t h = 0;  // seed = 0

    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;

    // Body - process 4-byte blocks (use memcpy to avoid unaligned access UB)
    for (size_t i = 0; i < nblocks; i++) {
        uint32_t k;
        memcpy(&k, data + i * 4, sizeof(uint32_t));

        k *= c1;
        k = (k << 15) | (k >> 17);  // rotl32(k, 15)
        k *= c2;

        h ^= k;
        h = (h << 13) | (h >> 19);  // rotl32(h, 13)
        h = h * 5 + 0xe6546b64;
    }

    // Tail - process remaining bytes
    const uint8_t* tail = (const uint8_t*) (data + nblocks * 4);
    uint32_t k = 0;

    switch (key_size & 3) {
        case 3:
            k ^= tail[2] << 16;  // fallthrough
        case 2:
            k ^= tail[1] << 8;  // fallthrough
        case 1:
            k ^= tail[0];
            k *= c1;
            k = (k << 15) | (k >> 17);
            k *= c2;
            h ^= k;
    }

    // Finalization — explicitly cast to uint32_t for MurmurHash3 spec compliance.
    // On 64-bit systems, key_size is size_t (64-bit); the standard MurmurHash3
    // finalization XORs with the 32-bit length value.
    h ^= (uint32_t)key_size;
    return hash_murmur3_32(h);
}

//=============================================================================
// Key Comparison Functions
//=============================================================================

/**
 * WHAT: Case-sensitive string comparison
 * WHY: Default string key comparison
 * HOW: memcmp after size check
 */
static bool key_compare_string_sensitive(const void* key1, size_t key1_size, const void* key2,
                                         size_t key2_size)
{
    if (key1_size != key2_size) {
        return false;
    }
    return memcmp(key1, key2, key1_size) == 0;
}

/**
 * WHAT: Case-insensitive string comparison
 * WHY: Support curiosity engine use case
 * HOW: Convert to lowercase during comparison
 */
static bool key_compare_string_insensitive(const void* key1, size_t key1_size, const void* key2,
                                           size_t key2_size)
{
    if (key1_size != key2_size) {
        return false;
    }

    const char* s1 = (const char*) key1;
    const char* s2 = (const char*) key2;

    for (size_t i = 0; i < key1_size; i++) {
        if (tolower((unsigned char) s1[i]) != tolower((unsigned char) s2[i])) {
            return false;
        }
    }

    return true;
}

/**
 * WHAT: Integer key comparison
 * WHY: Fast comparison for uint32_t and uint64_t keys
 * HOW: memcmp is sufficient for integer types
 */
static bool key_compare_integer(const void* key1, size_t key1_size, const void* key2,
                                size_t key2_size)
{
    if (key1_size != key2_size) {
        return false;
    }
    return memcmp(key1, key2, key1_size) == 0;
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * WHAT: Compute hash for a key
 * WHY: Centralize hash computation logic
 * HOW: Dispatch to appropriate hash function, normalize case if needed
 *
 * CRITICAL: For case-insensitive string keys, must normalize to lowercase
 * before hashing so "ConcePT", "concept", and "CONCEPT" hash to same bucket
 */
static uint32_t compute_hash(hash_table_t* table, const void* key, size_t key_size)
{
    // For case-insensitive string keys, normalize to lowercase before hashing
    if (table->config.key_type == HASH_KEY_STRING && table->config.case_insensitive) {
        /* P2-U1: Use stack buffer for small keys to avoid heap allocation on every
         * hash computation. Only fall back to heap for keys > 1024 bytes. */
        #define HASH_STACK_BUF_SIZE 1024
        char stack_buf[HASH_STACK_BUF_SIZE];
        char* lowercase_key;
        bool heap_allocated = false;

        if (key_size <= HASH_STACK_BUF_SIZE) {
            lowercase_key = stack_buf;
        } else {
            lowercase_key = (char*)nimcp_malloc(key_size);
            if (!lowercase_key) {
                // Fallback to original key on allocation failure
                LOG_WARN("nimcp_hash_table",
                         "Failed to allocate %zu bytes for lowercase key normalization. "
                         "Falling back to original key - case-insensitive matching may fail.",
                         key_size);
                goto hash_original_key;
            }
            heap_allocated = true;
        }

        // Convert to lowercase
        const char* str = (const char*)key;
        for (size_t i = 0; i < key_size; i++) {
            lowercase_key[i] = tolower((unsigned char)str[i]);
        }

        // Hash the lowercase version
        uint32_t hash;
        switch (table->config.hash_algorithm) {
            case HASH_ALG_FNV1A:
                hash = hash_fnv1a(lowercase_key, key_size);
                break;
            case HASH_ALG_DJB2:
                hash = hash_djb2(lowercase_key, key_size);
                break;
            case HASH_ALG_MURMUR3:
                hash = hash_murmur3(lowercase_key, key_size);
                break;
            case HASH_ALG_CUSTOM:
                if (table->config.custom_hash_fn) {
                    hash = table->config.custom_hash_fn(lowercase_key, key_size);
                } else {
                    hash = hash_fnv1a(lowercase_key, key_size);
                }
                break;
            default:
                hash = hash_fnv1a(lowercase_key, key_size);
                break;
        }

        if (heap_allocated) {
            nimcp_free(lowercase_key);
        }
        return hash;
    }

hash_original_key:
    // Normal path: hash original key
    switch (table->config.hash_algorithm) {
        case HASH_ALG_FNV1A:
            return hash_fnv1a(key, key_size);
        case HASH_ALG_DJB2:
            return hash_djb2(key, key_size);
        case HASH_ALG_MURMUR3:
            return hash_murmur3(key, key_size);
        case HASH_ALG_CUSTOM:
            if (table->config.custom_hash_fn) {
                return table->config.custom_hash_fn(key, key_size);
            }
            // Fallback to FNV-1a if custom function not provided
            return hash_fnv1a(key, key_size);
        default:
            return hash_fnv1a(key, key_size);
    }
}

/**
 * WHAT: Compare two keys for equality
 * WHY: Centralize key comparison logic
 * HOW: Dispatch to appropriate comparison function
 */
static bool keys_equal(hash_table_t* table, const void* key1, size_t key1_size, const void* key2,
                       size_t key2_size)
{
    // First check if custom comparison provided
    if (table->config.key_type == HASH_KEY_CUSTOM && table->config.custom_compare_fn) {
        return table->config.custom_compare_fn(key1, key1_size, key2, key2_size);
    }

    // String comparison
    if (table->config.key_type == HASH_KEY_STRING) {
        if (table->config.case_insensitive) {
            return key_compare_string_insensitive(key1, key1_size, key2, key2_size);
        } else {
            return key_compare_string_sensitive(key1, key1_size, key2, key2_size);
        }
    }

    // Integer comparison (uint32_t, uint64_t, or custom)
    return key_compare_integer(key1, key1_size, key2, key2_size);
}

/**
 * WHAT: Create a new hash entry
 * WHY: Allocate and initialize entry node
 * HOW: Copy key and value data
 */
static hash_entry_t* create_entry(const void* key, size_t key_size, const void* value,
                                  size_t value_size, uint32_t hash)
{
    hash_entry_t* entry = (hash_entry_t*) nimcp_malloc(sizeof(hash_entry_t));
    NIMCP_API_CHECK_ALLOC(entry, "Failed to allocate hash entry");

    // Copy key
    entry->key = nimcp_malloc(key_size);
    if (!entry->key) {
        LOG_ERROR("Failed to allocate key memory (%zu bytes)", key_size);
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, key_size, "Failed to allocate key memory");
        nimcp_free(entry);
        return NULL;
    }
    memcpy(entry->key, key, key_size);
    entry->key_size = key_size;

    // Copy value
    entry->value = nimcp_malloc(value_size);
    if (!entry->value) {
        LOG_ERROR("Failed to allocate value memory (%zu bytes)", value_size);
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, value_size, "Failed to allocate value memory");
        nimcp_free(entry->key);
        nimcp_free(entry);
        return NULL;
    }
    memcpy(entry->value, value, value_size);
    entry->value_size = value_size;

    entry->hash = hash;
    entry->next = NULL;

    return entry;
}

/**
 * WHAT: Free a hash entry
 * WHY: Release all memory associated with entry
 * HOW: Call destructor if configured, free key/value/entry
 */
static void free_entry(hash_table_t* table, hash_entry_t* entry)
{
    if (!entry) {
        return;
    }

    // Call value destructor if configured
    if (table->config.value_destructor) {
        table->config.value_destructor(entry->value, entry->value_size);
    }

    nimcp_free(entry->key);
    nimcp_free(entry->value);
    nimcp_free(entry);
}

/**
 * WHAT: Find entry in chain
 * WHY: Locate entry for lookup/update/remove
 * HOW: Traverse chain, compare keys
 *
 * @param prev [OUT] Previous entry in chain (for removal)
 */
static hash_entry_t* find_entry(hash_table_t* table, hash_entry_t* head, const void* key,
                                size_t key_size, uint32_t hash, hash_entry_t** prev)
{
    hash_entry_t* current = head;
    hash_entry_t* previous = NULL;

    while (current) {
        // Check hash first (fast rejection)
        if (current->hash == hash) {
            // Hash matches, now compare keys
            if (keys_equal(table, current->key, current->key_size, key, key_size)) {
                if (prev) {
                    *prev = previous;
                }
                return current;
            }
        }

        previous = current;
        current = current->next;
    }

    if (prev) {
        *prev = previous;
    }
    return NULL;  /* Key not found - normal return, not an error */
}

//=============================================================================
// Core Operations
//=============================================================================

hash_table_t* hash_table_create(const hash_table_config_t* config)
{
    hash_table_t* table = (hash_table_t*) nimcp_malloc(sizeof(hash_table_t));
    NIMCP_API_CHECK_ALLOC(table, "Failed to allocate hash table structure");

    // Set configuration with defaults
    if (config) {
        table->config = *config;

        // CRITICAL: thread_safe flag is NOT implemented
        // Reject creation if caller expects thread safety we cannot provide
        if (config->thread_safe) {
            LOG_ERROR("nimcp_hash_table",
                      "thread_safe=true requested but NOT IMPLEMENTED. "
                      "Hash table operations are NOT thread-safe. "
                      "Caller MUST provide external synchronization (e.g., nimcp_mutex_t) "
                      "around ALL hash table operations including create, insert, lookup, "
                      "remove, iterate, and destroy. Failing to do so will cause data races.");
            nimcp_free(table);
            /* P2: Use NIMCP_ERROR_NOT_SUPPORTED (thread_safe not implemented), not NO_MEMORY */
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_SUPPORTED, "hash_table_create: thread_safe=true not implemented");
            return NULL;
        }
    } else {
        // Default configuration
        memset(&table->config, 0, sizeof(hash_table_config_t));
        table->config.initial_buckets = 256;
        table->config.key_type = HASH_KEY_STRING;
        table->config.hash_algorithm = HASH_ALG_FNV1A;
        table->config.case_insensitive = false;
        table->config.thread_safe = false;
    }

    // Ensure bucket count is at least 1
    if (table->config.initial_buckets == 0) {
        table->config.initial_buckets = 256;
    }

    // Allocate bucket array
    table->bucket_count = table->config.initial_buckets;
    table->buckets = (hash_entry_t**) nimcp_calloc(table->bucket_count, sizeof(hash_entry_t*));
    if (!table->buckets) {
        LOG_ERROR("Failed to allocate bucket array (%zu buckets)", table->bucket_count);
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, table->bucket_count * sizeof(hash_entry_t*),
                          "Failed to allocate bucket array");
        nimcp_free(table);
        return NULL;
    }

    table->entry_count = 0;

    return table;
}

void hash_table_destroy(hash_table_t* table)
{
    if (!table) {
        return;
    }

    // Free all entries in all buckets
    if (table->buckets) {
        for (size_t i = 0; i < table->bucket_count; i++) {
            hash_entry_t* current = table->buckets[i];
            while (current) {
                hash_entry_t* next = current->next;
                free_entry(table, current);
                current = next;
            }
        }
        nimcp_free(table->buckets);
    }

    nimcp_free(table);
}

size_t hash_table_size(const hash_table_t* table)
{
    return table ? table->entry_count : 0;
}

size_t hash_table_bucket_count(const hash_table_t* table)
{
    return table ? table->bucket_count : 0;
}

void hash_table_clear(hash_table_t* table)
{
    if (!table || !table->buckets) {
        return;
    }

    // Free all entries in all buckets
    for (size_t i = 0; i < table->bucket_count; i++) {
        hash_entry_t* current = table->buckets[i];
        while (current) {
            hash_entry_t* next = current->next;
            free_entry(table, current);
            current = next;
        }
        table->buckets[i] = NULL;
    }

    table->entry_count = 0;
}

//=============================================================================
// Generic Insert/Lookup/Remove
//=============================================================================

/**
 * WHAT: Generic insert function
 * WHY: Common logic for all key types
 * HOW: Hash key, find bucket, insert or update
 */
static bool hash_table_insert_generic(hash_table_t* table, const void* key, size_t key_size,
                                      const void* value, size_t value_size)
{
    if (!table || !key || !value || !table->buckets) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hash_table_insert_generic: required parameter is NULL (table, key, value, table->buckets)");
        return false;
    }

    if (table->bucket_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hash_table_insert_generic: table->bucket_count is zero");
        return false;  // Defensive check for zero buckets
    }

    // Compute hash
    uint32_t hash = compute_hash(table, key, key_size);
    size_t bucket_index = hash % table->bucket_count;

    // Check if key already exists
    hash_entry_t* existing =
        find_entry(table, table->buckets[bucket_index], key, key_size, hash, NULL);

    if (existing) {
        // Update existing entry
        void* new_value = nimcp_malloc(value_size);
        if (!new_value) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hash_table_insert_generic: new_value is NULL");
            return false;
        }
        memcpy(new_value, value, value_size);

        // Call destructor on old value
        if (table->config.value_destructor) {
            table->config.value_destructor(existing->value, existing->value_size);
        }

        nimcp_free(existing->value);
        existing->value = new_value;
        existing->value_size = value_size;

        return true;
    }

    // Create new entry
    hash_entry_t* new_entry = create_entry(key, key_size, value, value_size, hash);
    if (!new_entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hash_table_insert_generic: new_entry is NULL");
        return false;
    }

    // Insert at head of chain
    new_entry->next = table->buckets[bucket_index];
    table->buckets[bucket_index] = new_entry;
    table->entry_count++;

    // Check load factor and resize if needed
    hash_table_maybe_resize(table);

    return true;
}

/**
 * WHAT: Generic lookup function
 * WHY: Common logic for all key types
 * HOW: Hash key, find bucket, search chain
 */
static void* hash_table_lookup_generic(hash_table_t* table, const void* key, size_t key_size)
{
    if (!table || !key || !table->buckets) {
        return NULL;
    }

    if (table->bucket_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hash_table_lookup_generic: table->bucket_count is zero");
        return NULL;  // Defensive check for zero buckets
    }

    // Compute hash
    uint32_t hash = compute_hash(table, key, key_size);
    size_t bucket_index = hash % table->bucket_count;

    // Find entry
    hash_entry_t* entry =
        find_entry(table, table->buckets[bucket_index], key, key_size, hash, NULL);

    return entry ? entry->value : NULL;
}

/**
 * WHAT: Generic remove function
 * WHY: Common logic for all key types
 * HOW: Hash key, find bucket, unlink from chain
 */
static bool hash_table_remove_generic(hash_table_t* table, const void* key, size_t key_size)
{
    if (!table || !key || !table->buckets) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hash_table_remove_generic: required parameter is NULL (table, key, table->buckets)");
        return false;
    }

    if (table->bucket_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hash_table_remove_generic: table->bucket_count is zero");
        return false;  // Defensive check for zero buckets
    }

    // Compute hash
    uint32_t hash = compute_hash(table, key, key_size);
    size_t bucket_index = hash % table->bucket_count;

    // Find entry and its predecessor
    hash_entry_t* prev = NULL;
    hash_entry_t* entry =
        find_entry(table, table->buckets[bucket_index], key, key_size, hash, &prev);

    if (!entry) {
        return false;  /* Not found - normal return */
    }

    // Unlink from chain
    if (prev) {
        prev->next = entry->next;
    } else {
        table->buckets[bucket_index] = entry->next;
    }

    // Free entry
    free_entry(table, entry);
    table->entry_count--;

    return true;
}

//=============================================================================
// String Key Operations
//=============================================================================

bool hash_table_insert_string(hash_table_t* table, const char* key, const void* value,
                              size_t value_size)
{
    if (!key) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hash_table_insert_string: key is NULL");
        return false;
    }

    size_t key_size = strlen(key) + 1;  // Include null terminator
    return hash_table_insert_generic(table, key, key_size, value, value_size);
}

void* hash_table_lookup_string(hash_table_t* table, const char* key)
{
    if (!key) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "key is NULL");

        return NULL;
    }

    size_t key_size = strlen(key) + 1;  // Include null terminator
    return hash_table_lookup_generic(table, key, key_size);
}

bool hash_table_remove_string(hash_table_t* table, const char* key)
{
    if (!key) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hash_table_remove_string: key is NULL");
        return false;
    }

    size_t key_size = strlen(key) + 1;  // Include null terminator
    return hash_table_remove_generic(table, key, key_size);
}

//=============================================================================
// Integer Key Operations (uint32_t)
//=============================================================================

bool hash_table_insert_uint32(hash_table_t* table, uint32_t key, const void* value,
                              size_t value_size)
{
    return hash_table_insert_generic(table, &key, sizeof(uint32_t), value, value_size);
}

void* hash_table_lookup_uint32(hash_table_t* table, uint32_t key)
{
    return hash_table_lookup_generic(table, &key, sizeof(uint32_t));
}

bool hash_table_remove_uint32(hash_table_t* table, uint32_t key)
{
    return hash_table_remove_generic(table, &key, sizeof(uint32_t));
}

//=============================================================================
// Integer Key Operations (uint64_t)
//=============================================================================

bool hash_table_insert_uint64(hash_table_t* table, uint64_t key, const void* value,
                              size_t value_size)
{
    return hash_table_insert_generic(table, &key, sizeof(uint64_t), value, value_size);
}

void* hash_table_lookup_uint64(hash_table_t* table, uint64_t key)
{
    return hash_table_lookup_generic(table, &key, sizeof(uint64_t));
}

bool hash_table_remove_uint64(hash_table_t* table, uint64_t key)
{
    return hash_table_remove_generic(table, &key, sizeof(uint64_t));
}

//=============================================================================
// Iteration
//=============================================================================

void hash_table_iterate(hash_table_t* table, hash_table_iterator_fn_t callback, void* user_data)
{
    if (!table || !callback || !table->buckets) {
        return;
    }

    for (size_t i = 0; i < table->bucket_count; i++) {
        hash_entry_t* current = table->buckets[i];
        while (current) {
            // Call callback
            bool continue_iteration = callback(current->key, current->key_size, current->value,
                                               current->value_size, user_data);

            if (!continue_iteration) {
                return;  // Stop iteration
            }

            current = current->next;
        }
    }
}

//=============================================================================
// Statistics
//=============================================================================

void hash_table_stats(const hash_table_t* table, size_t* max_chain_length, float* avg_chain_length,
                      size_t* empty_buckets)
{
    if (!table || !table->buckets) {
        if (max_chain_length)
            *max_chain_length = 0;
        if (avg_chain_length)
            *avg_chain_length = 0.0F;
        if (empty_buckets)
            *empty_buckets = 0;
        return;
    }

    size_t max_len = 0;
    size_t total_len = 0;
    size_t non_empty = 0;
    size_t empty = 0;

    for (size_t i = 0; i < table->bucket_count; i++) {
        size_t chain_len = 0;
        hash_entry_t* current = table->buckets[i];

        while (current) {
            chain_len++;
            current = current->next;
        }

        if (chain_len == 0) {
            empty++;
        } else {
            non_empty++;
            total_len += chain_len;
            if (chain_len > max_len) {
                max_len = chain_len;
            }
        }
    }

    if (max_chain_length) {
        *max_chain_length = max_len;
    }

    if (avg_chain_length) {
        *avg_chain_length = non_empty > 0 ? (float) total_len / non_empty : 0.0F;
    }

    if (empty_buckets) {
        *empty_buckets = empty;
    }
}
