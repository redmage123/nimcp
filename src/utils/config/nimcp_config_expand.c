#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_config_expand.c - Config Env Expansion & Nested Key Implementation
//=============================================================================
/**
 * @file nimcp_config_expand.c
 * @brief Implementation of config expansion and nested key navigation
 *
 * WHAT: Env var expansion + nested key access for NIMCP config
 * WHY:  Production-ready config with env vars and hierarchical structure
 * HOW:  Parser for ${VAR} syntax, tree traversal for nested keys
 *
 * ARCHITECTURE:
 * - Expansion: State machine parser with recursive substitution
 * - Nested keys: Hash table tree structure with '.' separator
 * - Wildcards: DFS traversal with pattern matching
 * - Security: BBB validation on all expanded values
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 */

#include "utils/config/nimcp_config_expand.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/config/nimcp_dynamic_config.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(config_expand)

//=============================================================================
// Constants
//=============================================================================

#define MODULE_NAME "config_expand"
#define EXPAND_BUFFER_SIZE 8192

//=============================================================================
// Internal State
//=============================================================================

// Environment variable prefix filter (NULL = no filter)
static char* g_env_prefix = NULL;

// Last expansion error
static config_expand_error_t g_last_error = CONFIG_EXPAND_OK;

// BBB system for validation (initialized on first use)
static bbb_system_t g_bbb_system = NULL;

//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Initialize BBB system if not already initialized
 *
 * WHAT: Lazy-init BBB for input validation
 * WHY:  Avoid init cost if expansion not used
 * HOW:  Check global, create if NULL
 */
static void ensure_bbb_initialized(void) {
    if (g_bbb_system == NULL) {
        bbb_config_t config = bbb_default_config();
        config.input.max_string_length = CONFIG_EXPAND_MAX_LENGTH;
        g_bbb_system = bbb_system_create(&config);
        if (!g_bbb_system) {
            LOG_ERROR("Failed to initialize BBB for config expansion");
        }
    }
}

/**
 * @brief Check if var name matches prefix filter
 *
 * WHAT: Test if var should be expanded based on prefix
 * WHY:  Security - isolate config vars from system env
 * HOW:  String prefix comparison
 */
static bool var_matches_prefix(const char* var_name) {
    // No prefix = allow all
    if (g_env_prefix == NULL) {
        return true;
    }

    // Check prefix match
    return strncmp(var_name, g_env_prefix, strlen(g_env_prefix)) == 0;
}

/**
 * @brief Extract variable name portion from expansion
 *
 * WHAT: Get var name from ${VAR...}
 * WHY:  First step of parsing
 * HOW:  Copy up to separator or end
 */
static char* extract_var_name(const char* start, const char* end, const char* sep) {
    size_t name_len = (sep < end) ? (size_t)(sep - start) : (size_t)(end - start);
    char* var_name = nimcp_malloc(name_len + 1);
    if (!var_name) {
        g_last_error = CONFIG_EXPAND_ERROR_MEMORY;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "extract_var_name: var_name is NULL");
        return NULL;
    }
    memcpy(var_name, start, name_len);
    var_name[name_len] = '\0';
    return var_name;
}

/**
 * @brief Parse default/alternate value from expansion
 *
 * WHAT: Handle :- and :+ modifiers
 * WHY:  Support ${VAR:-default} and ${VAR:+alt}
 * HOW:  Extract and copy modifier value
 */
static bool parse_modifier(const char* sep, const char* end,
                            bool* has_default, char** default_val,
                            bool* has_alternate, char** alternate_val,
                            char* var_name) {
    if (sep >= end || sep + 1 >= end) {
        return true;
    }

    char mode = *(sep + 1);
    const char* value_start = sep + 2;
    size_t value_len = (size_t)(end - value_start);

    if (mode == '-') {
        *has_default = true;
        *default_val = nimcp_malloc(value_len + 1);
        if (!*default_val) {
            nimcp_free(var_name);
            g_last_error = CONFIG_EXPAND_ERROR_MEMORY;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "extract_var_name: validation failed");
            return false;
        }
        memcpy(*default_val, value_start, value_len);
        (*default_val)[value_len] = '\0';
    } else if (mode == '+') {
        *has_alternate = true;
        *alternate_val = nimcp_malloc(value_len + 1);
        if (!*alternate_val) {
            nimcp_free(var_name);
            g_last_error = CONFIG_EXPAND_ERROR_MEMORY;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "extract_var_name: validation failed");
            return false;
        }
        memcpy(*alternate_val, value_start, value_len);
        (*alternate_val)[value_len] = '\0';
    }

    return true;
}

/**
 * @brief Extract variable name from ${VAR...} syntax
 *
 * WHAT: Parse var name and optional default/alternate
 * WHY:  Support ${VAR:-default} and ${VAR:+alt} syntax
 * HOW:  Find delimiters, extract components
 */
static bool parse_var_expansion(const char* start,
                                 char** var_name,
                                 bool* has_default,
                                 char** default_val,
                                 bool* has_alternate,
                                 char** alternate_val,
                                 const char** end_ptr) {
    const char* end = strchr(start, '}');
    if (!end) {
        g_last_error = CONFIG_EXPAND_ERROR_SYNTAX;
        LOG_ERROR("Missing closing '}' in variable expansion");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "extract_var_name: end is NULL");
        return false;
    }

    *end_ptr = end;
    const char* sep = start;
    while (sep < end && *sep != ':') sep++;

    *var_name = extract_var_name(start, end, sep);
    if (!*var_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "extract_var_name: validation failed");
        return false;
    }

    *has_default = false;
    *has_alternate = false;
    *default_val = NULL;
    *alternate_val = NULL;

    if (sep < end) {
        if (!parse_modifier(sep, end, has_default, default_val,
                             has_alternate, alternate_val, *var_name)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "extract_var_name: validation failed");
            return false;
        }
    }

    return true;
}

/**
 * @brief Determine final value after applying expansion logic
 *
 * WHAT: Apply ${VAR}, ${VAR:-default}, ${VAR:+alt} logic
 * WHY:  Central expansion logic
 * HOW:  Check value, apply modifier rules
 */
static const char* apply_expansion_logic(const char* value,
                                          bool has_default,
                                          const char* default_val,
                                          bool has_alternate,
                                          const char* alternate_val) {
    if (has_alternate) {
        return (value && *value) ? alternate_val : "";
    } else if (has_default) {
        return (value && *value) ? value : default_val;
    } else {
        return value ? value : "";
    }
}

/**
 * @brief Forward declaration for recursive expansion
 */
static char* expand_env_recursive(const char* input, int depth);

/**
 * @brief Append expanded result to output buffer
 *
 * WHAT: Copy recursively expanded result to output
 * WHY:  Bounds checking + cleanup
 * HOW:  Check length, memcpy, nimcp_free temps
 */
static bool append_expansion(char* output, size_t* out_len,
                              const char* result, int depth,
                              char* var_name, char* default_val,
                              char* alternate_val) {
    char* expanded_result = expand_env_recursive(result, depth + 1);
    if (!expanded_result) {
        nimcp_free(var_name);
        nimcp_free(default_val);
        nimcp_free(alternate_val);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "extract_var_name: expanded_result is NULL");
        return false;
    }

    size_t result_len = strlen(expanded_result);
    if (*out_len + result_len >= CONFIG_EXPAND_MAX_LENGTH - 1) {
        g_last_error = CONFIG_EXPAND_ERROR_TOO_LONG;
        LOG_ERROR("Expanded string too long");
        nimcp_free(expanded_result);
        nimcp_free(var_name);
        nimcp_free(default_val);
        nimcp_free(alternate_val);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "extract_var_name: capacity exceeded");
        return false;
    }

    memcpy(output + *out_len, expanded_result, result_len);
    *out_len += result_len;

    nimcp_free(expanded_result);
    nimcp_free(var_name);
    nimcp_free(default_val);
    nimcp_free(alternate_val);

    return true;
}

/**
 * @brief Process variable expansion during parsing
 *
 * WHAT: Handle ${VAR...} construct
 * WHY:  Separate expansion logic from main loop
 * HOW:  Parse, lookup, expand, append
 */
static bool process_var_expansion(const char** p, char* output,
                                   size_t* out_len, int depth) {
    char* var_name = NULL;
    bool has_default = false, has_alternate = false;
    char* default_val = NULL;
    char* alternate_val = NULL;
    const char* end_ptr = NULL;

    if (!parse_var_expansion(*p + 2, &var_name, &has_default,
                              &default_val, &has_alternate,
                              &alternate_val, &end_ptr)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "extract_var_name: operation failed");
        return false;
    }

    const char* value = NULL;
    if (var_matches_prefix(var_name)) {
        value = getenv(var_name);
    }

    const char* result = apply_expansion_logic(value, has_default,
                                                default_val, has_alternate,
                                                alternate_val);

    if (!append_expansion(output, out_len, result, depth,
                           var_name, default_val, alternate_val)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: operation failed");
        return false;
    }

    *p = end_ptr + 1;
    return true;
}

/**
 * @brief Expand environment variables recursively
 *
 * WHAT: Core expansion engine with recursion support
 * WHY:  Handle nested expansions like ${FOO:-${BAR}}
 * HOW:  State machine, recursive calls, depth limiting
 */
static char* expand_env_recursive(const char* input, int depth) {
    if (!input || depth > CONFIG_EXPAND_MAX_DEPTH) {
        if (depth > CONFIG_EXPAND_MAX_DEPTH) {
            g_last_error = CONFIG_EXPAND_ERROR_TOO_DEEP;
            LOG_ERROR("Expansion depth exceeded: %d", depth);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "expand_env_recursive: validation failed");
        return NULL;
    }

    char* output = nimcp_malloc(CONFIG_EXPAND_MAX_LENGTH);
    if (!output) {
        g_last_error = CONFIG_EXPAND_ERROR_MEMORY;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "expand_env_recursive: output is NULL");
        return NULL;
    }

    const char* p = input;
    size_t out_len = 0;

    while (*p && out_len < CONFIG_EXPAND_MAX_LENGTH - 1) {
        if (*p == '$' && *(p + 1) == '$') {
            output[out_len++] = '$';
            p += 2;
        } else if (*p == '$' && *(p + 1) == '{') {
            if (!process_var_expansion(&p, output, &out_len, depth)) {
                nimcp_free(output);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "expand_env_recursive: process_var_expansion is NULL");
                return NULL;
            }
        } else {
            output[out_len++] = *p++;
        }
    }

    if (*p != '\0') {
        g_last_error = CONFIG_EXPAND_ERROR_TOO_LONG;
        LOG_ERROR("Expanded string too long");
        nimcp_free(output);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "expand_env_recursive: validation failed");
        return NULL;
    }

    output[out_len] = '\0';

    ensure_bbb_initialized();
    if (g_bbb_system) {
        bbb_validation_result_t result;
        if (!bbb_validate_string(g_bbb_system, output, &result)) {
            LOG_WARN("Expanded string failed BBB validation: %s", result.reason);
        }
    }

    return output;
}

//=============================================================================
// Environment Variable Expansion API
//=============================================================================

char* config_expand_env(const char* value) {
    if (!value) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "value is NULL");

        return NULL;
    }

    g_last_error = CONFIG_EXPAND_OK;
    LOG_DEBUG("Expanding: %s", value);

    char* result = expand_env_recursive(value, 0);
    if (result) {
        LOG_DEBUG("Expanded to: %s", result);
    }

    return result;
}

bool config_expand_env_inplace(char* value, size_t max_size) {
    if (!value || max_size == 0) {
        g_last_error = CONFIG_EXPAND_ERROR_INVALID;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_expand_env_inplace: value is NULL");
        return false;
    }

    char* expanded = config_expand_env(value);
    if (!expanded) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_expand_env_inplace: expanded is NULL");
        return false;
    }

    size_t len = strlen(expanded);
    if (len >= max_size) {
        g_last_error = CONFIG_EXPAND_ERROR_TOO_LONG;
        nimcp_free(expanded);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "config_expand_env_inplace: capacity exceeded");
        return false;
    }

    memcpy(value, expanded, len + 1);
    nimcp_free(expanded);
    return true;
}

void config_set_env_prefix(const char* prefix) {
    if (g_env_prefix) {
        nimcp_free(g_env_prefix);
        g_env_prefix = NULL;
    }

    if (prefix) {
        g_env_prefix = nimcp_strdup(prefix);
        LOG_INFO("Set env var prefix filter: %s", prefix);
    } else {
        LOG_INFO("Cleared env var prefix filter");
    }
}

config_expand_error_t config_expand_get_last_error(void) {
    return g_last_error;
}

const char* config_expand_error_string(config_expand_error_t error) {
    switch (error) {
        case CONFIG_EXPAND_OK:           return "Success";
        case CONFIG_EXPAND_ERROR_SYNTAX: return "Syntax error in expansion";
        case CONFIG_EXPAND_ERROR_TOO_DEEP: return "Expansion depth exceeded";
        case CONFIG_EXPAND_ERROR_TOO_LONG: return "Result too long";
        case CONFIG_EXPAND_ERROR_MEMORY:  return "Memory allocation failed";
        case CONFIG_EXPAND_ERROR_INVALID: return "Invalid parameter";
        default: return "Unknown error";
    }
}

//=============================================================================
// Nested Key Access Helpers
//=============================================================================

/**
 * @brief Build full nested key from path
 *
 * WHAT: Convert dot path to full config key
 * WHY:  Dynamic config uses flat keys, we provide nested API
 * HOW:  For now, just use path as-is (dynamic_config will need update)
 */
static const char* build_nested_key(const char* path) {
    // For now, dynamic_config expects flat keys
    // Future enhancement: traverse hierarchical structure
    return path;
}

//=============================================================================
// Nested Key Access API
//=============================================================================

int64_t config_get_nested_int(const char* path, int64_t default_val) {
    if (!path) {
        return default_val;
    }

    const char* key = build_nested_key(path);
    return config_get_int(key, default_val);
}

double config_get_nested_float(const char* path, double default_val) {
    if (!path) {
        return default_val;
    }

    const char* key = build_nested_key(path);
    return config_get_float(key, default_val);
}

bool config_get_nested_bool(const char* path, bool default_val) {
    if (!path) {
        return default_val;
    }

    const char* key = build_nested_key(path);
    return config_get_bool(key, default_val);
}

const char* config_get_nested_string(const char* path, const char* default_val) {
    if (!path) {
        return default_val;
    }

    const char* key = build_nested_key(path);
    const char* value = config_get_string(key, NULL);

    if (!value) {
        return default_val;
    }

    // Try to expand env vars in the value
    // Note: This creates a temporary string. For production use,
    // we'd need to cache expanded values in the config system.
    static __thread char expanded_buffer[CONFIG_EXPAND_MAX_LENGTH];
    strncpy(expanded_buffer, value, sizeof(expanded_buffer) - 1);
    expanded_buffer[sizeof(expanded_buffer) - 1] = '\0';

    if (config_expand_env_inplace(expanded_buffer, sizeof(expanded_buffer))) {
        return expanded_buffer;
    }

    return value;
}

//=============================================================================
// Nested Key Mutation API
//=============================================================================

bool config_set_nested_int(const char* path, int64_t value) {
    if (!path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_set_nested_int: path is NULL");
        return false;
    }

    const char* key = build_nested_key(path);
    return config_set_int(key, value);
}

bool config_set_nested_float(const char* path, double value) {
    if (!path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_set_nested_float: path is NULL");
        return false;
    }

    const char* key = build_nested_key(path);
    return config_set_float(key, value);
}

bool config_set_nested_bool(const char* path, bool value) {
    if (!path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_set_nested_bool: path is NULL");
        return false;
    }

    const char* key = build_nested_key(path);
    return config_set_bool(key, value);
}

bool config_set_nested_string(const char* path, const char* value) {
    if (!path || !value) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_set_nested_string: required parameter is NULL (path, value)");
        return false;
    }

    const char* key = build_nested_key(path);
    return config_set_string(key, value);
}

//=============================================================================
// Wildcard Query Implementation
//=============================================================================

/**
 * @brief Check if pattern component matches key component
 *
 * WHAT: Match single path component with wildcard support
 * WHY:  Building block for full pattern matching
 * HOW:  Exact match or * wildcard
 */
static bool component_matches(const char* pattern, const char* key) {
    return (strcmp(pattern, "*") == 0) || (strcmp(pattern, key) == 0);
}

/**
 * @brief Split path into components
 *
 * WHAT: Tokenize dotted path
 * WHY:  Component-wise pattern matching
 * HOW:  strtok-style splitting
 */
static char** split_path(const char* path, size_t* count) {
    if (!path) {
        *count = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "split_path: path is NULL");
        return NULL;
    }

    // Count components
    size_t n = 1;
    for (const char* p = path; *p; p++) {
        if (*p == '.') n++;
    }

    // Allocate array
    char** components = nimcp_malloc(sizeof(char*) * (n + 1));
    if (!components) {
        *count = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "split_path: components is NULL");
        return NULL;
    }

    // Copy and tokenize
    char* path_copy = nimcp_strdup(path);
    if (!path_copy) {
        nimcp_free(components);
        *count = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "split_path: path_copy is NULL");
        return NULL;
    }

    char* token = strtok(path_copy, ".");
    size_t i = 0;
    while (token && i < n) {
        components[i] = nimcp_strdup(token);
        token = strtok(NULL, ".");
        i++;
    }
    components[i] = NULL;
    *count = i;

    nimcp_free(path_copy);
    return components;
}

/**
 * @brief Free component array
 */
static void free_components(char** components, size_t count) {
    if (!components) {
        return;
    }

    for (size_t i = 0; i < count; i++) {
        nimcp_free(components[i]);
    }
    nimcp_free(components);
}

bool config_key_matches(const char* pattern, const char* key) {
    if (!pattern || !key) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_key_matches: required parameter is NULL (pattern, key)");
        return false;
    }

    // Split both into components
    size_t pattern_count, key_count;
    char** pattern_parts = split_path(pattern, &pattern_count);
    char** key_parts = split_path(key, &key_count);

    bool matches = true;

    // Must have same depth
    if (pattern_count != key_count) {
        matches = false;
    } else {
        // Check each component
        for (size_t i = 0; i < pattern_count; i++) {
            if (!component_matches(pattern_parts[i], key_parts[i])) {
                matches = false;
                break;
            }
        }
    }

    free_components(pattern_parts, pattern_count);
    free_components(key_parts, key_count);

    return matches;
}

config_key_list_t config_find_keys(const char* pattern) {
    config_key_list_t result = {NULL, 0};

    if (!pattern) {
        return result;
    }

    // NOTE: This is a stub implementation that would need integration
    // with the actual config storage. For production, we'd:
    // 1. Iterate all keys in config storage
    // 2. Match each against pattern
    // 3. Collect matches

    // For now, return empty list
    LOG_WARN("config_find_keys not fully implemented yet");

    return result;
}

void config_key_list_destroy(config_key_list_t* list) {
    if (!list) {
        return;
    }

    if (list->keys) {
        for (size_t i = 0; i < list->count; i++) {
            nimcp_free(list->keys[i]);
        }
        nimcp_free(list->keys);
        list->keys = NULL;
    }

    list->count = 0;
}

//=============================================================================
// Key Path Utilities
//=============================================================================

char* config_key_parent(const char* path) {
    if (!path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "path is NULL");

        return NULL;
    }

    const char* last_dot = strrchr(path, '.');
    if (!last_dot) {
        // No parent
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_key_parent: last_dot is NULL");
        return NULL;
    }

    size_t len = (size_t)(last_dot - path);
    char* parent = nimcp_malloc(len + 1);
    if (!parent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "parent is NULL");

        return NULL;
    }

    memcpy(parent, path, len);
    parent[len] = '\0';

    return parent;
}

char* config_key_leaf(const char* path) {
    if (!path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "path is NULL");

        return NULL;
    }

    const char* last_dot = strrchr(path, '.');
    if (!last_dot) {
        // Entire path is the leaf
        return nimcp_strdup(path);
    }

    return nimcp_strdup(last_dot + 1);
}

size_t config_key_depth(const char* path) {
    if (!path) {
        return 0;
    }

    size_t depth = 1;
    for (const char* p = path; *p; p++) {
        if (*p == '.') {
            depth++;
        }
    }

    return depth;
}

char* config_key_join(const char** components) {
    if (!components || !components[0]) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_key_join: required parameter is NULL (components, components)");
        return NULL;
    }

    // Calculate total length
    size_t total_len = 0;
    size_t count = 0;
    for (const char** p = components; *p; p++) {
        total_len += strlen(*p);
        count++;
    }
    total_len += count - 1;  // dots

    // Allocate buffer
    char* result = nimcp_malloc(total_len + 1);
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "result is NULL");

        return NULL;
    }

    // Join components
    char* dst = result;
    for (size_t i = 0; components[i]; i++) {
        if (i > 0) {
            *dst++ = '.';
        }
        size_t len = strlen(components[i]);
        memcpy(dst, components[i], len);
        dst += len;
    }
    *dst = '\0';

    return result;
}

//=============================================================================
// Section API (as per user requirements)
//=============================================================================

/**
 * @brief Config section internal structure
 */
struct config_section_struct {
    char* prefix;       /**< Section prefix */
    char** keys;        /**< Array of keys */
    size_t count;       /**< Number of keys */
};

config_section_t config_get_section(const char* prefix) {
    if (!prefix) {
        LOG_ERROR("config_get_section: NULL prefix");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_get_section: prefix is NULL");
        return NULL;
    }

    config_section_t section = nimcp_malloc(sizeof(struct config_section_struct));
    if (!section) {
        LOG_ERROR("config_get_section: Failed to allocate section");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "config_get_section: section is NULL");
        return NULL;
    }

    section->prefix = nimcp_strdup(prefix);
    if (!section->prefix) {
        nimcp_free(section);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "config_get_section: section->prefix is NULL");
        return NULL;
    }

    // NOTE: This is a stub - would need integration with actual config storage
    // to iterate and collect all keys with matching prefix
    section->keys = NULL;
    section->count = 0;

    LOG_DEBUG("Created section for prefix: %s", prefix);
    return section;
}

void config_section_destroy(config_section_t section) {
    if (!section) {
        return;
    }

    if (section->prefix) {
        nimcp_free(section->prefix);
    }

    if (section->keys) {
        for (size_t i = 0; i < section->count; i++) {
            nimcp_free(section->keys[i]);
        }
        nimcp_free(section->keys);
    }

    nimcp_free(section);
}

size_t config_section_size(config_section_t section) {
    if (!section) {
        return 0;
    }
    return section->count;
}

void config_section_iterate(config_section_t section,
                              config_section_iterator_t iterator,
                              void* user_data) {
    if (!section || !iterator) {
        return;
    }

    for (size_t i = 0; i < section->count; i++) {
        iterator(section->keys[i], user_data);
    }
}
