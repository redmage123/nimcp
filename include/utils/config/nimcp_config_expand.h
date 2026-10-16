//=============================================================================
// nimcp_config_expand.h - Config Env Expansion & Nested Key Support
//=============================================================================
/**
 * @file nimcp_config_expand.h
 * @brief Environment variable expansion and nested key navigation for config
 * @version 1.0.0
 *
 * WHAT: Advanced config features: env var expansion + nested key access
 * WHY:  Enable flexible config with env vars and hierarchical organization
 * HOW:  Parse expansion syntax, traverse dot-separated keys, wildcard matching
 *
 * FEATURES:
 * 1. Environment Variable Expansion:
 *    - ${VAR}         -> expand VAR
 *    - ${VAR:-default} -> expand VAR or use default if unset
 *    - ${VAR:+alternate} -> use alternate if VAR is set
 *    - $$            -> literal $ (escape)
 *
 * 2. Nested Keys:
 *    - database.host -> get "host" from "database" section
 *    - server.port   -> get "port" from "server" section
 *    - Supports arbitrary depth
 *
 * 3. Wildcard Queries:
 *    - database.*    -> all keys under "database"
 *    - *.port        -> all "port" keys in all sections
 *
 * 4. Key Utilities:
 *    - config_key_parent("a.b.c") -> "a.b"
 *    - config_key_leaf("a.b.c")   -> "c"
 *
 * ARCHITECTURE:
 * ```
 *   Config File                  Environment
 *   ┌─────────────┐              ┌──────────────┐
 *   │database:    │              │ DB_HOST=     │
 *   │  host: $    │──expand──────│  localhost   │
 *   │    {DB_HOST}│              │ DB_PORT=5432 │
 *   │  port: $    │              └──────────────┘
 *   │    {DB_PORT}│
 *   └─────────────┘
 *        │
 *        │ nested access
 *        ▼
 *   config_get_nested_string("database.host")
 *        │
 *        ▼
 *   "localhost" (expanded)
 * ```
 *
 * EXAMPLE USAGE:
 * ```c
 * // Expansion
 * char* expanded = config_expand_env("${HOME}/config");
 * // -> "/home/user/config"
 * nimcp_free(expanded);
 *
 * // Nested keys
 * const char* host = config_get_nested_string("database.host", "localhost");
 * int64_t port = config_get_nested_int("database.port", 5432);
 *
 * // Wildcards
 * config_key_list_t keys = config_find_keys("database.*");
 * for (size_t i = 0; i < keys.count; i++) {
 *     printf("%s\n", keys.keys[i]);
 * }
 * config_key_list_destroy(&keys);
 * ```
 *
 * THREAD SAFETY:
 * - All functions are thread-safe
 * - Internal config storage uses read-write locks
 * - Expansion is stateless and thread-safe
 *
 * SECURITY:
 * - All expanded values validated through BBB
 * - String length limits enforced
 * - No buffer overflows possible
 * - Configurable env var prefix for isolation
 *
 * NIMCP CODING STANDARDS:
 * - Functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT/WHY/HOW comments
 * - Full logging integration
 * - Unified memory for allocations
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 */

#ifndef NIMCP_CONFIG_EXPAND_H
#define NIMCP_CONFIG_EXPAND_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Export Macro
//=============================================================================

#include "common/nimcp_export.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum expansion depth (prevent infinite loops) */
#define CONFIG_EXPAND_MAX_DEPTH 32

/** @brief Maximum expanded string length */
#define CONFIG_EXPAND_MAX_LENGTH 4096

/** @brief Maximum number of keys in wildcard result */
#define CONFIG_EXPAND_MAX_KEYS 1024

/** @brief Nested key separator */
#define CONFIG_KEY_SEPARATOR '.'

/** @brief Wildcard character */
#define CONFIG_WILDCARD '*'

//=============================================================================
// Types
//=============================================================================

/**
 * @brief List of configuration keys (for wildcard queries)
 */
typedef struct {
    char** keys;        /**< Array of key strings (NULL-terminated) */
    size_t count;       /**< Number of keys in array */
} config_key_list_t;

/**
 * @brief Expansion error codes
 */
typedef enum {
    CONFIG_EXPAND_OK = 0,           /**< Success */
    CONFIG_EXPAND_ERROR_SYNTAX,     /**< Syntax error in expansion */
    CONFIG_EXPAND_ERROR_TOO_DEEP,   /**< Expansion depth exceeded */
    CONFIG_EXPAND_ERROR_TOO_LONG,   /**< Result too long */
    CONFIG_EXPAND_ERROR_MEMORY,     /**< Memory allocation failed */
    CONFIG_EXPAND_ERROR_INVALID     /**< Invalid parameter */
} config_expand_error_t;

//=============================================================================
// Environment Variable Expansion API
//=============================================================================

/**
 * @brief Expand environment variables in string (allocating)
 *
 * WHAT: Parse and expand ${VAR}, ${VAR:-default}, ${VAR:+alt} syntax
 * WHY:  Enable config files to reference environment variables
 * HOW:  State machine parser, getenv() lookup, substitute values
 *
 * Supported syntax:
 * - ${VAR}          -> getenv("VAR") or "" if unset
 * - ${VAR:-default} -> getenv("VAR") or "default" if unset/empty
 * - ${VAR:+alt}     -> "alt" if VAR set, "" if unset
 * - $$              -> literal "$"
 *
 * @param value String to expand (may be NULL)
 * @return Expanded string (caller must free) or NULL on error
 *
 * COMPLEXITY: O(n) where n = input length
 * MEMORY: Allocates via unified_mem, caller must free
 *
 * EXAMPLE:
 * ```c
 * char* expanded = config_expand_env("${HOME}/config");
 * if (expanded) {
 *     printf("Expanded: %s\n", expanded);
 *     nimcp_free(expanded);
 * }
 * ```
 */
NIMCP_EXPORT char* config_expand_env(const char* value);

/**
 * @brief Expand environment variables in-place
 *
 * WHAT: Same as config_expand_env but modifies buffer in-place
 * WHY:  Avoid allocation when buffer is available
 * HOW:  Expand into same buffer with bounds checking
 *
 * @param value Buffer containing string to expand (modified in-place)
 * @param max_size Maximum size of buffer
 * @return true on success, false if result doesn't fit or error
 *
 * COMPLEXITY: O(n)
 * MEMORY: No allocation
 *
 * WARNING: Expansion may fail if result > max_size
 */
NIMCP_EXPORT bool config_expand_env_inplace(char* value, size_t max_size);

/**
 * @brief Set environment variable prefix for isolation
 *
 * WHAT: Only expand vars with specific prefix (e.g., "NIMCP_")
 * WHY:  Prevent accidental expansion of system vars
 * HOW:  Store prefix, check before expansion
 *
 * @param prefix Prefix string (NULL to disable filtering)
 *
 * EXAMPLE:
 * ```c
 * config_set_env_prefix("NIMCP_");
 * // Now only ${NIMCP_XXX} vars will expand
 * ```
 */
NIMCP_EXPORT void config_set_env_prefix(const char* prefix);

/**
 * @brief Get last expansion error
 *
 * @return Last error code
 */
NIMCP_EXPORT config_expand_error_t config_expand_get_last_error(void);

/**
 * @brief Get error message for error code
 *
 * @param error Error code
 * @return Human-readable error message
 */
NIMCP_EXPORT const char* config_expand_error_string(config_expand_error_t error);

//=============================================================================
// Nested Key Access API
//=============================================================================

/**
 * @brief Get nested integer config value
 *
 * WHAT: Access config using dot-separated path (e.g., "db.port")
 * WHY:  Hierarchical config organization
 * HOW:  Split path by '.', traverse config tree
 *
 * @param path Dot-separated key path (e.g., "database.port")
 * @param default_val Default value if not found
 * @return Config value or default
 *
 * COMPLEXITY: O(k) where k = number of path components
 *
 * EXAMPLE:
 * ```c
 * int64_t port = config_get_nested_int("database.port", 5432);
 * ```
 */
NIMCP_EXPORT int64_t config_get_nested_int(const char* path, int64_t default_val);

/**
 * @brief Get nested float config value
 *
 * @param path Dot-separated key path
 * @param default_val Default value if not found
 * @return Config value or default
 */
NIMCP_EXPORT double config_get_nested_float(const char* path, double default_val);

/**
 * @brief Get nested boolean config value
 *
 * @param path Dot-separated key path
 * @param default_val Default value if not found
 * @return Config value or default
 */
NIMCP_EXPORT bool config_get_nested_bool(const char* path, bool default_val);

/**
 * @brief Get nested string config value
 *
 * WHAT: Get string value from nested path with env expansion
 * WHY:  Combine nested access + env var support
 * HOW:  Traverse path, get value, expand env vars
 *
 * @param path Dot-separated key path
 * @param default_val Default value if not found
 * @return Config value (expanded) or default (caller does NOT own memory)
 *
 * NOTE: Return value is valid until next config reload
 */
NIMCP_EXPORT const char* config_get_nested_string(const char* path,
                                                    const char* default_val);

//=============================================================================
// Nested Key Mutation API
//=============================================================================

/**
 * @brief Set nested integer config value
 *
 * WHAT: Set value at nested path, creating sections as needed
 * WHY:  Programmatic config construction
 * HOW:  Split path, create intermediate nodes, set value
 *
 * @param path Dot-separated key path
 * @param value Value to set
 * @return true on success, false on error
 *
 * COMPLEXITY: O(k) where k = number of path components
 * THREAD SAFETY: Thread-safe (write lock)
 *
 * EXAMPLE:
 * ```c
 * config_set_nested_int("database.port", 5432);
 * config_set_nested_int("database.pool.size", 10);
 * ```
 */
NIMCP_EXPORT bool config_set_nested_int(const char* path, int64_t value);

/**
 * @brief Set nested float config value
 *
 * @param path Dot-separated key path
 * @param value Value to set
 * @return true on success, false on error
 */
NIMCP_EXPORT bool config_set_nested_float(const char* path, double value);

/**
 * @brief Set nested boolean config value
 *
 * @param path Dot-separated key path
 * @param value Value to set
 * @return true on success, false on error
 */
NIMCP_EXPORT bool config_set_nested_bool(const char* path, bool value);

/**
 * @brief Set nested string config value
 *
 * WHAT: Set string at nested path (value is NOT expanded)
 * WHY:  Store literal config values
 * HOW:  Split path, create nodes, copy string
 *
 * @param path Dot-separated key path
 * @param value Value to set (copied internally, may contain ${VAR})
 * @return true on success, false on error
 *
 * NOTE: Value is stored as-is. Expansion happens on get.
 */
NIMCP_EXPORT bool config_set_nested_string(const char* path, const char* value);

//=============================================================================
// Wildcard Query API
//=============================================================================

/**
 * @brief Find keys matching wildcard pattern
 *
 * WHAT: Search config for keys matching glob-style pattern
 * WHY:  Query all keys in a section or matching suffix
 * HOW:  Traverse config tree, match pattern, collect keys
 *
 * Patterns:
 * - "database.*"    -> all keys under database section
 * - "*.port"        -> all keys ending in ".port"
 * - "*.*.*"         -> all keys at depth 3
 *
 * @param pattern Wildcard pattern (* matches any component)
 * @return List of matching keys (caller must destroy)
 *
 * COMPLEXITY: O(n) where n = total keys in config
 * MEMORY: Allocates key list, caller must call config_key_list_destroy
 *
 * EXAMPLE:
 * ```c
 * config_key_list_t keys = config_find_keys("database.*");
 * for (size_t i = 0; i < keys.count; i++) {
 *     printf("Found: %s\n", keys.keys[i]);
 * }
 * config_key_list_destroy(&keys);
 * ```
 */
NIMCP_EXPORT config_key_list_t config_find_keys(const char* pattern);

/**
 * @brief Destroy key list returned by config_find_keys
 *
 * WHAT: Free memory allocated for key list
 * WHY:  Proper cleanup
 * HOW:  Free each key string + array
 *
 * @param list Key list to destroy (may be NULL)
 */
NIMCP_EXPORT void config_key_list_destroy(config_key_list_t* list);

//=============================================================================
// Section Retrieval API
//=============================================================================

/**
 * @brief Config section handle (opaque)
 */
typedef struct config_section_struct* config_section_t;

/**
 * @brief Section iterator callback
 *
 * @param key Key name (relative to section prefix)
 * @param user_data User context
 */
typedef void (*config_section_iterator_t)(const char* key, void* user_data);

/**
 * @brief Get all config keys under a prefix
 *
 * WHAT: Retrieve section (all keys with common prefix)
 * WHY:  Group related config values
 * HOW:  Scan config table, collect matching keys
 *
 * @param prefix Key prefix (e.g., "database")
 * @return Section handle or NULL on error
 *
 * EXAMPLE:
 * ```c
 * // For config:
 * // database.host = localhost
 * // database.port = 5432
 * // database.user = admin
 * config_section_t db = config_get_section("database");
 * size_t count = config_section_size(db);  // Returns: 3
 * config_section_destroy(db);
 * ```
 */
NIMCP_EXPORT config_section_t config_get_section(const char* prefix);

/**
 * @brief Destroy section handle
 *
 * @param section Section to destroy
 */
NIMCP_EXPORT void config_section_destroy(config_section_t section);

/**
 * @brief Get number of keys in section
 *
 * @param section Section handle
 * @return Number of keys in section
 */
NIMCP_EXPORT size_t config_section_size(config_section_t section);

/**
 * @brief Iterate over keys in section
 *
 * WHAT: Call callback for each key in section
 * WHY:  Process all related config values
 * HOW:  Iterate internal key list, invoke callback
 *
 * @param section Section handle
 * @param iterator Callback function
 * @param user_data User context passed to callback
 *
 * EXAMPLE:
 * ```c
 * void print_key(const char* key, void* data) {
 *     printf("Key: %s\n", key);
 * }
 * config_section_t db = config_get_section("database");
 * config_section_iterate(db, print_key, NULL);
 * ```
 */
NIMCP_EXPORT void config_section_iterate(
    config_section_t section,
    config_section_iterator_t iterator,
    void* user_data
);

//=============================================================================
// Key Path Utilities
//=============================================================================

/**
 * @brief Get parent path of a key
 *
 * WHAT: Extract parent section from dotted path
 * WHY:  Navigate up hierarchy
 * HOW:  Find last '.', return substring
 *
 * @param path Dotted key path
 * @return Parent path (caller must free) or NULL if no parent
 *
 * EXAMPLE:
 * ```c
 * char* parent = config_key_parent("database.pool.size");
 * // Returns: "database.pool"
 * nimcp_free(parent);
 * ```
 */
NIMCP_EXPORT char* config_key_parent(const char* path);

/**
 * @brief Get leaf name of a key
 *
 * WHAT: Extract final component from dotted path
 * WHY:  Get key name without section
 * HOW:  Find last '.', return substring
 *
 * @param path Dotted key path
 * @return Leaf name (caller must free) or copy of path if no parent
 *
 * EXAMPLE:
 * ```c
 * char* leaf = config_key_leaf("database.pool.size");
 * // Returns: "size"
 * nimcp_free(leaf);
 * ```
 */
NIMCP_EXPORT char* config_key_leaf(const char* path);

/**
 * @brief Count depth of dotted path
 *
 * @param path Dotted key path
 * @return Depth (number of components)
 *
 * EXAMPLE:
 * ```c
 * size_t depth = config_key_depth("a.b.c"); // Returns 3
 * ```
 */
NIMCP_EXPORT size_t config_key_depth(const char* path);

/**
 * @brief Check if pattern matches key
 *
 * WHAT: Test if key matches wildcard pattern
 * WHY:  Pattern matching utility
 * HOW:  Component-wise comparison with * wildcard
 *
 * @param pattern Pattern with * wildcards
 * @param key Key to test
 * @return true if matches
 *
 * EXAMPLE:
 * ```c
 * bool match = config_key_matches("database.*", "database.port");
 * // Returns: true
 * ```
 */
NIMCP_EXPORT bool config_key_matches(const char* pattern, const char* key);

/**
 * @brief Join path components
 *
 * WHAT: Build dotted path from components
 * WHY:  Construct paths programmatically
 * HOW:  Join with '.' separator
 *
 * @param components NULL-terminated array of components
 * @return Joined path (caller must free)
 *
 * EXAMPLE:
 * ```c
 * const char* parts[] = {"database", "pool", "size", NULL};
 * char* path = config_key_join(parts);
 * // Returns: "database.pool.size"
 * nimcp_free(path);
 * ```
 */
NIMCP_EXPORT char* config_key_join(const char** components);

//=============================================================================
// Helper Macros
//=============================================================================

/**
 * @brief Convenience macro for nested integer access
 */
#define CONFIG_GET_NESTED_INT(path, default_val) \
    config_get_nested_int(path, default_val)

/**
 * @brief Convenience macro for nested float access
 */
#define CONFIG_GET_NESTED_FLOAT(path, default_val) \
    config_get_nested_float(path, default_val)

/**
 * @brief Convenience macro for nested bool access
 */
#define CONFIG_GET_NESTED_BOOL(path, default_val) \
    config_get_nested_bool(path, default_val)

/**
 * @brief Convenience macro for nested string access
 */
#define CONFIG_GET_NESTED_STRING(path, default_val) \
    config_get_nested_string(path, default_val)

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_CONFIG_EXPAND_H
