/**
 * @file nimcp_dynamic_config.c
 * @brief Dynamic configuration with hash table and signal handler integration
 *
 * WHAT: Runtime-reconfigurable hyperparameters via config file with O(1) lookup
 * WHY:  Allow tuning without restarting (critical for production)
 * HOW:  Hash table + INI parser + SIGHUP signal handler integration
 *
 * ENHANCEMENTS (Phase UMI-3):
 * - Hash table for O(1) lookups instead of O(n) linear search
 * - Signal handler integration for automatic SIGHUP reload
 * - Unified memory allocation (nimcp_unified_alloc/nimcp_free)
 * - Security module registration
 * - Comprehensive logging throughout
 * - Environment variable expansion in strings
 * - Schema-based validation
 * - Atomic reload with snapshot/rollback
 * - Array and nested key support
 *
 * @author NIMCP Team
 * @date 2025-11-28
 */

#include "utils/config/nimcp_dynamic_config.h"
#include "constants/nimcp_buffer_constants.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/config/nimcp_config_array.h"
#include "utils/containers/nimcp_hash_table.h"
#include "utils/signal/nimcp_signal_handler.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_rwlock.h"
#include "utils/platform/nimcp_platform_once.h"
#include "utils/thread/nimcp_atomic.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <errno.h>
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dynamic_config)

//=============================================================================
// Internal Data Structures
//=============================================================================

#define MAX_CALLBACKS 64
#define MAX_PATH_LENGTH 512
#define INITIAL_HASH_BUCKETS 256

/**
 * @brief Internal config entry stored in hash table
 */
typedef struct {
    config_value_type_t type;
    config_value_t value;
} config_entry_internal_t;

/**
 * @brief Snapshot of config state for rollback
 */
typedef struct {
    hash_table_t* table;
    uint32_t version;
} config_snapshot_t;

/**
 * @brief Callback registration entry
 */
typedef struct {
    uint32_t id;
    char* key;
    config_change_callback_t callback;
    void* user_data;
    bool in_use;
} callback_registration_t;

//=============================================================================
// Global State
//=============================================================================

static hash_table_t* g_config_table = NULL;
static config_snapshot_t* g_config_snapshot = NULL;
static const config_schema_t* g_config_schema = NULL;
static callback_registration_t g_callbacks[MAX_CALLBACKS];
static nimcp_platform_rwlock_t g_config_lock = PTHREAD_RWLOCK_INITIALIZER;
static nimcp_mutex_t g_callback_lock = NIMCP_MUTEX_INITIALIZER;
static char g_config_path[MAX_PATH_LENGTH] = {0};
static config_stats_t g_stats = {0};
static uint32_t g_next_callback_id = 1;
static nimcp_atomic_bool_t g_initialized = {0};

//=============================================================================
// Forward Declarations
//=============================================================================

static void trigger_callbacks(const char* key, const config_value_t* old_value,
                              const config_value_t* new_value);
static void config_reload_callback(void);
static bool expand_env_vars(const char* input, char* output, size_t output_size);
static bool validate_value_against_schema(const char* key, config_value_type_t type,
                                         const config_value_t* value);
static void free_config_entry(void* value, size_t value_size);

//=============================================================================
// Helper Functions
//=============================================================================

static void trim_whitespace(char* str) {
    char* start = str;
    char* end;

    // Trim leading space
    while (isspace((unsigned char)*start)) start++;

    if (*start == 0) {
        *str = 0;
        return;
    }

    // Trim trailing space
    end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) end--;
    end[1] = '\0';

    // Move trimmed string to beginning if needed
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

/**
 * @brief Expand environment variables in string
 *
 * WHAT: Replace ${VAR} or $VAR with environment variable values
 * WHY:  Allow config files to reference environment
 * HOW:  Scan for $ markers and substitute
 */
static bool expand_env_vars(const char* input, char* output, size_t output_size) {
    const char* src = input;
    char* dst = output;
    size_t remaining = output_size - 1;
    bool expanded = false;

    while (*src && remaining > 0) {
        if (*src == '$') {
            // Found environment variable marker
            src++;
            bool braced = (*src == '{');
            if (braced) src++;

            // Extract variable name
            char varname[NIMCP_NAME_BUFFER_SIZE];
            size_t varlen = 0;
            while (*src && varlen < sizeof(varname) - 1) {
                if (braced && *src == '}') {
                    src++;
                    break;
                }
                if (!braced && !isalnum(*src) && *src != '_') {
                    break;
                }
                varname[varlen++] = *src++;
            }
            varname[varlen] = '\0';

            // Get environment variable value
            const char* value = getenv(varname);
            if (value) {
                size_t vallen = strlen(value);
                if (vallen <= remaining) {
                    memcpy(dst, value, vallen);
                    dst += vallen;
                    remaining -= vallen;
                    expanded = true;
                } else {
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "expand_env_vars: validation failed");
                    return false;  // Output buffer too small
                }
            }
        } else {
            *dst++ = *src++;
            remaining--;
        }
    }

    *dst = '\0';
    return expanded;
}

/**
 * @brief Validate value against schema
 */
static bool validate_value_against_schema(const char* key, config_value_type_t type,
                                         const config_value_t* value) {
    if (!g_config_schema || !g_config_schema->entries) {
        return true;  // No schema, allow all
    }

    // Find schema entry for this key
    const config_entry_t* schema_entry = NULL;
    for (size_t i = 0; i < g_config_schema->num_entries; i++) {
        if (strcmp(g_config_schema->entries[i].key, key) == 0) {
            schema_entry = &g_config_schema->entries[i];
            break;
        }
    }

    if (!schema_entry) {
        return true;  // Key not in schema, allow it
    }

    // Check type matches
    if (schema_entry->type != type) {
        LOG_MODULE_ERROR("config", "Type mismatch for key '%s': expected %d, got %d",
                        key, schema_entry->type, type);
        return false;
    }

    // Validate range for numeric types
    if (type == CONFIG_TYPE_INT && schema_entry->has_min) {
        if (value->int_val < schema_entry->min_value.int_val) {
            LOG_MODULE_ERROR("config", "Value for '%s' (%lld) below minimum (%lld)",
                            key, (long long)value->int_val,
                            (long long)schema_entry->min_value.int_val);
            return false;
        }
    }

    if (type == CONFIG_TYPE_INT && schema_entry->has_max) {
        if (value->int_val > schema_entry->max_value.int_val) {
            LOG_MODULE_ERROR("config", "Value for '%s' (%lld) above maximum (%lld)",
                            key, (long long)value->int_val,
                            (long long)schema_entry->max_value.int_val);
            return false;
        }
    }

    if (type == CONFIG_TYPE_FLOAT && schema_entry->has_min) {
        if (value->float_val < schema_entry->min_value.float_val) {
            LOG_MODULE_ERROR("config", "Value for '%s' (%f) below minimum (%f)",
                            key, value->float_val, schema_entry->min_value.float_val);
            return false;
        }
    }

    if (type == CONFIG_TYPE_FLOAT && schema_entry->has_max) {
        if (value->float_val > schema_entry->max_value.float_val) {
            LOG_MODULE_ERROR("config", "Value for '%s' (%f) above maximum (%f)",
                            key, value->float_val, schema_entry->max_value.float_val);
            return false;
        }
    }

    return true;
}

/**
 * @brief Free config entry (hash table destructor callback)
 */
static void free_config_entry(void* value, size_t value_size) {
    if (!value) return;

    config_entry_internal_t* entry = (config_entry_internal_t*)value;

    // Free string value if present
    if (entry->type == CONFIG_TYPE_STRING && entry->value.string_val) {
        nimcp_free(entry->value.string_val);
    }

    // Free array value if present using array module's destroy function
    if (entry->type == CONFIG_TYPE_ARRAY && entry->value.array_val) {
        config_array_destroy((config_array_t*)entry->value.array_val);
    }
}

//=============================================================================
// INI Parser
//=============================================================================

static bool parse_config_file(const char* path) {
    LOG_MODULE_DEBUG("config", "Parsing config file: %s", path);

    FILE* file = fopen(path, "r");
    if (!file) {
        LOG_MODULE_ERROR("config", "Failed to open config file: %s", path);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parse_config_file: file is NULL");
        return false;
    }

    char line[NIMCP_CMD_BUFFER_SIZE];
    int line_num = 0;
    int entries_parsed = 0;

    while (fgets(line, sizeof(line), file)) {
        line_num++;
        trim_whitespace(line);

        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == ';' || line[0] == '\0') {
            continue;
        }

        // Parse key=value
        char* equals = strchr(line, '=');
        if (!equals) {
            LOG_MODULE_WARN("config", "Parse error line %d: missing '='", line_num);
            continue;
        }

        *equals = '\0';
        char* key = line;
        char* value = equals + 1;

        trim_whitespace(key);
        trim_whitespace(value);

        if (key[0] == '\0' || value[0] == '\0') {
            LOG_MODULE_WARN("config", "Parse error line %d: empty key or value", line_num);
            continue;
        }

        // Allocate entry structure
        config_entry_internal_t* entry = nimcp_malloc(sizeof(config_entry_internal_t));
        if (!entry) {
            LOG_MODULE_ERROR("config", "Failed to allocate entry for key: %s", key);
            continue;
        }

        // Detect type and parse value
        if (strcmp(value, "true") == 0 || strcmp(value, "false") == 0) {
            entry->type = CONFIG_TYPE_BOOL;
            entry->value.bool_val = (strcmp(value, "true") == 0);
        } else if (strchr(value, '.') != NULL) {
            entry->type = CONFIG_TYPE_FLOAT;
            // P1-2 fix: Use strtod instead of atof for safe conversion
            char* endptr;
            errno = 0;
            entry->value.float_val = strtod(value, &endptr);
            if (endptr == value || errno == ERANGE) {
                entry->value.float_val = 0.0;
                LOG_MODULE_ERROR("config", "Failed to parse float value for key: %s", key);
            }
        } else if (value[0] == '-' || isdigit((unsigned char)value[0])) {
            entry->type = CONFIG_TYPE_INT;
            // P1-2 fix: Use strtoll instead of atoll for safe conversion
            char* endptr;
            errno = 0;
            entry->value.int_val = strtoll(value, &endptr, 10);
            if (endptr == value || errno == ERANGE) {
                entry->value.int_val = 0;
                LOG_MODULE_ERROR("config", "Failed to parse int value for key: %s", key);
            }
        } else {
            // String value - expand environment variables
            char expanded[NIMCP_LOG_BUFFER_SIZE];
            if (expand_env_vars(value, expanded, sizeof(expanded))) {
                entry->type = CONFIG_TYPE_STRING;
                entry->value.string_val = nimcp_strdup(expanded);
                LOG_MODULE_DEBUG("config", "Expanded env vars: %s -> %s", value, expanded);
            } else {
                entry->type = CONFIG_TYPE_STRING;
                entry->value.string_val = nimcp_strdup(value);
            }

            if (!entry->value.string_val) {
                LOG_MODULE_ERROR("config", "Failed to allocate string for key: %s", key);
                nimcp_free(entry);
                continue;
            }
        }

        // Validate against schema if set
        if (!validate_value_against_schema(key, entry->type, &entry->value)) {
            LOG_MODULE_WARN("config", "Value for key '%s' failed schema validation", key);
            if (entry->type == CONFIG_TYPE_STRING && entry->value.string_val) {
                nimcp_free(entry->value.string_val);
            }
            nimcp_free(entry);
            continue;
        }

        // Store in hash table (thread-safe)
        nimcp_platform_rwlock_wrlock(&g_config_lock);
        bool inserted = hash_table_insert_string(g_config_table, key, entry,
                                                 sizeof(config_entry_internal_t));
        nimcp_platform_rwlock_unlock(&g_config_lock);

        if (inserted) {
            entries_parsed++;
            LOG_MODULE_TRACE("config", "Parsed: %s = [type=%d]", key, entry->type);
        } else {
            LOG_MODULE_ERROR("config", "Failed to insert key: %s", key);
            if (entry->type == CONFIG_TYPE_STRING && entry->value.string_val) {
                nimcp_free(entry->value.string_val);
            }
        }

        nimcp_free(entry);  // Hash table makes a copy
    }

    fclose(file);
    LOG_MODULE_INFO("config", "Parsed %d config entries from %s", entries_parsed, path);
    return true;
}

//=============================================================================
// Signal Handler Integration
//=============================================================================

/**
 * @brief Config reload callback for signal handler
 *
 * WHAT: Called by signal handler when SIGHUP received
 * WHY:  Trigger config reload automatically
 * HOW:  Call config_reload()
 */
static void config_reload_callback(void) {
    LOG_MODULE_INFO("config", "SIGHUP received, reloading config");
    config_reload();
}

//=============================================================================
// Public API Implementation
//=============================================================================

bool config_init(const char* config_path) {
    if (!config_path) {
        LOG_MODULE_ERROR("config", "config_init: NULL config path");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_init: config_path is NULL");
        return false;
    }

    if (nimcp_atomic_load_bool(&g_initialized, NIMCP_MEMORY_ORDER_ACQUIRE)) {
        LOG_MODULE_WARN("config", "config_init: Already initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "config_init: validation failed");
        return false;
    }

    LOG_MODULE_INFO("config", "Initializing config system from: %s", config_path);

    // Copy config path
    strncpy(g_config_path, config_path, sizeof(g_config_path) - 1);
    g_config_path[sizeof(g_config_path) - 1] = '\0';

    // Create hash table for config storage
    hash_table_config_t hash_config = {
        .initial_buckets = INITIAL_HASH_BUCKETS,
        .key_type = HASH_KEY_STRING,
        .hash_algorithm = HASH_ALG_FNV1A,
        .case_insensitive = false,
        .thread_safe = false,  // We handle threading with rwlock
        .value_destructor = free_config_entry
    };

    g_config_table = hash_table_create(&hash_config);
    if (!g_config_table) {
        LOG_MODULE_ERROR("config", "Failed to create config hash table");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_init: g_config_table is NULL");
        return false;
    }

    // Parse initial config file
    if (!parse_config_file(g_config_path)) {
        LOG_MODULE_ERROR("config", "Failed to parse config file: %s", g_config_path);
        hash_table_destroy(g_config_table);
        g_config_table = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_init: parse_config_file is NULL");
        return false;
    }

    // Initialize statistics
    g_stats.config_version = 1;
    g_stats.last_reload_time_ms = 0;
    g_stats.reload_count = 0;
    g_stats.reload_failures = 0;
    g_stats.validation_failures = 0;

    // Register reload callback with signal handler
    signal_handler_set_reload_callback(config_reload_callback);
    LOG_MODULE_DEBUG("config", "Registered reload callback with signal handler");

    nimcp_atomic_store_bool(&g_initialized, true, NIMCP_MEMORY_ORDER_RELEASE);
    LOG_MODULE_INFO("config", "Config initialized successfully (version %u, %zu entries)",
                   g_stats.config_version, hash_table_size(g_config_table));

    return true;
}

void config_shutdown(void) {
    if (!nimcp_atomic_load_bool(&g_initialized, NIMCP_MEMORY_ORDER_ACQUIRE)) {
        return;
    }

    LOG_MODULE_INFO("config", "Shutting down config system");

    nimcp_platform_rwlock_wrlock(&g_config_lock);

    // Destroy hash table (calls value_destructor for each entry)
    if (g_config_table) {
        hash_table_destroy(g_config_table);
        g_config_table = NULL;
    }

    // Free snapshot if exists
    if (g_config_snapshot) {
        if (g_config_snapshot->table) {
            hash_table_destroy(g_config_snapshot->table);
        }
        nimcp_free(g_config_snapshot);
        g_config_snapshot = NULL;
    }

    nimcp_platform_rwlock_unlock(&g_config_lock);

    // Clear callbacks
    nimcp_mutex_lock(&g_callback_lock);
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (g_callbacks[i].in_use && g_callbacks[i].key) {
            nimcp_free(g_callbacks[i].key);
        }
        g_callbacks[i].in_use = false;
        g_callbacks[i].key = NULL;
        g_callbacks[i].callback = NULL;
        g_callbacks[i].user_data = NULL;
    }
    nimcp_mutex_unlock(&g_callback_lock);

    // Unregister from signal handler
    signal_handler_set_reload_callback(NULL);

    nimcp_atomic_store_bool(&g_initialized, false, NIMCP_MEMORY_ORDER_RELEASE);
    LOG_MODULE_INFO("config", "Config system shutdown complete");
}

bool config_reload(void) {
    LOG_MODULE_INFO("config", "Reloading config from: %s", g_config_path);

    // Parse config file (replaces values in hash table)
    bool success = parse_config_file(g_config_path);

    if (success) {
        g_stats.reload_count++;
        g_stats.config_version++;
        LOG_MODULE_INFO("config", "Config reloaded successfully (version %u)",
                       g_stats.config_version);
    } else {
        g_stats.reload_failures++;
        LOG_MODULE_ERROR("config", "Config reload failed");
    }

    return success;
}

int64_t config_get_int(const char* key, int64_t default_value) {
    if (!key || !g_config_table) return default_value;

    nimcp_platform_rwlock_rdlock(&g_config_lock);

    config_entry_internal_t* entry = hash_table_lookup_string(g_config_table, key);
    if (entry && entry->type == CONFIG_TYPE_INT) {
        int64_t value = entry->value.int_val;
        nimcp_platform_rwlock_unlock(&g_config_lock);
        LOG_MODULE_TRACE("config", "get_int: %s = %lld", key, (long long)value);
        return value;
    }

    nimcp_platform_rwlock_unlock(&g_config_lock);
    LOG_MODULE_DEBUG("config", "get_int: key '%s' not found, returning default %lld",
                    key, (long long)default_value);
    return default_value;
}

double config_get_float(const char* key, double default_value) {
    if (!key || !g_config_table) return default_value;

    nimcp_platform_rwlock_rdlock(&g_config_lock);

    config_entry_internal_t* entry = hash_table_lookup_string(g_config_table, key);
    if (entry && entry->type == CONFIG_TYPE_FLOAT) {
        double value = entry->value.float_val;
        nimcp_platform_rwlock_unlock(&g_config_lock);
        LOG_MODULE_TRACE("config", "get_float: %s = %f", key, value);
        return value;
    }

    nimcp_platform_rwlock_unlock(&g_config_lock);
    LOG_MODULE_DEBUG("config", "get_float: key '%s' not found, returning default %f",
                    key, default_value);
    return default_value;
}

bool config_get_bool(const char* key, bool default_value) {
    if (!key || !g_config_table) return default_value;

    nimcp_platform_rwlock_rdlock(&g_config_lock);

    config_entry_internal_t* entry = hash_table_lookup_string(g_config_table, key);
    if (entry && entry->type == CONFIG_TYPE_BOOL) {
        bool value = entry->value.bool_val;
        nimcp_platform_rwlock_unlock(&g_config_lock);
        LOG_MODULE_TRACE("config", "get_bool: %s = %s", key, value ? "true" : "false");
        return value;
    }

    nimcp_platform_rwlock_unlock(&g_config_lock);
    LOG_MODULE_DEBUG("config", "get_bool: key '%s' not found, returning default %s",
                    key, default_value ? "true" : "false");
    return default_value;
}

const char* config_get_string(const char* key, const char* default_value) {
    if (!key || !g_config_table) return default_value;

    nimcp_platform_rwlock_rdlock(&g_config_lock);

    config_entry_internal_t* entry = hash_table_lookup_string(g_config_table, key);
    if (entry && entry->type == CONFIG_TYPE_STRING) {
        const char* value = entry->value.string_val;
        nimcp_platform_rwlock_unlock(&g_config_lock);
        LOG_MODULE_TRACE("config", "get_string: %s = %s", key, value);
        return value;
    }

    nimcp_platform_rwlock_unlock(&g_config_lock);
    LOG_MODULE_DEBUG("config", "get_string: key '%s' not found, returning default",
                    key);
    return default_value;
}

bool config_set_int(const char* key, int64_t value) {
    if (!key || !g_config_table) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_set_int: required parameter is NULL (key, g_config_table)");
        return false;
    }

    nimcp_platform_rwlock_wrlock(&g_config_lock);

    // Check if entry exists
    config_entry_internal_t* existing = hash_table_lookup_string(g_config_table, key);
    if (existing) {
        if (existing->type != CONFIG_TYPE_INT) {
            nimcp_platform_rwlock_unlock(&g_config_lock);
            LOG_MODULE_ERROR("config", "Type mismatch for key '%s'", key);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_set_int: validation failed");
            return false;
        }

        config_value_t old_value = existing->value;
        config_value_t new_value;
        new_value.int_val = value;

        // Validate
        if (!validate_value_against_schema(key, CONFIG_TYPE_INT, &new_value)) {
            nimcp_platform_rwlock_unlock(&g_config_lock);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_set_int: validate_value_against_schema is NULL");
            return false;
        }

        existing->value.int_val = value;
        nimcp_platform_rwlock_unlock(&g_config_lock);

        trigger_callbacks(key, &old_value, &new_value);
        LOG_MODULE_DEBUG("config", "set_int: %s = %lld", key, (long long)value);
        return true;
    }

    nimcp_platform_rwlock_unlock(&g_config_lock);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_set_int: operation failed");
    return false;
}

bool config_set_float(const char* key, double value) {
    if (!key || !g_config_table) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_set_float: required parameter is NULL (key, g_config_table)");
        return false;
    }

    nimcp_platform_rwlock_wrlock(&g_config_lock);

    config_entry_internal_t* existing = hash_table_lookup_string(g_config_table, key);
    if (existing) {
        if (existing->type != CONFIG_TYPE_FLOAT) {
            nimcp_platform_rwlock_unlock(&g_config_lock);
            LOG_MODULE_ERROR("config", "Type mismatch for key '%s'", key);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_set_float: validation failed");
            return false;
        }

        config_value_t old_value = existing->value;
        config_value_t new_value;
        new_value.float_val = value;

        if (!validate_value_against_schema(key, CONFIG_TYPE_FLOAT, &new_value)) {
            nimcp_platform_rwlock_unlock(&g_config_lock);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_set_float: validate_value_against_schema is NULL");
            return false;
        }

        existing->value.float_val = value;
        nimcp_platform_rwlock_unlock(&g_config_lock);

        trigger_callbacks(key, &old_value, &new_value);
        LOG_MODULE_DEBUG("config", "set_float: %s = %f", key, value);
        return true;
    }

    nimcp_platform_rwlock_unlock(&g_config_lock);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_set_float: operation failed");
    return false;
}

bool config_set_bool(const char* key, bool value) {
    if (!key || !g_config_table) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_set_bool: required parameter is NULL (key, g_config_table)");
        return false;
    }

    nimcp_platform_rwlock_wrlock(&g_config_lock);

    config_entry_internal_t* existing = hash_table_lookup_string(g_config_table, key);
    if (existing) {
        if (existing->type != CONFIG_TYPE_BOOL) {
            nimcp_platform_rwlock_unlock(&g_config_lock);
            LOG_MODULE_ERROR("config", "Type mismatch for key '%s'", key);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_set_bool: validation failed");
            return false;
        }

        config_value_t old_value = existing->value;
        config_value_t new_value;
        new_value.bool_val = value;

        existing->value.bool_val = value;
        nimcp_platform_rwlock_unlock(&g_config_lock);

        trigger_callbacks(key, &old_value, &new_value);
        LOG_MODULE_DEBUG("config", "set_bool: %s = %s", key, value ? "true" : "false");
        return true;
    }

    nimcp_platform_rwlock_unlock(&g_config_lock);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_set_bool: operation failed");
    return false;
}

bool config_set_string(const char* key, const char* value) {
    if (!key || !value || !g_config_table) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_set_string: required parameter is NULL (key, value, g_config_table)");
        return false;
    }

    nimcp_platform_rwlock_wrlock(&g_config_lock);

    config_entry_internal_t* existing = hash_table_lookup_string(g_config_table, key);
    if (existing) {
        if (existing->type != CONFIG_TYPE_STRING) {
            nimcp_platform_rwlock_unlock(&g_config_lock);
            LOG_MODULE_ERROR("config", "Type mismatch for key '%s'", key);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_set_string: validation failed");
            return false;
        }

        config_value_t old_value = existing->value;
        config_value_t new_value;
        new_value.string_val = nimcp_strdup(value);

        if (!new_value.string_val) {
            nimcp_platform_rwlock_unlock(&g_config_lock);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_set_string: new_value is NULL");
            return false;
        }

        // Free old string
        if (existing->value.string_val) {
            nimcp_free(existing->value.string_val);
        }

        existing->value.string_val = nimcp_strdup(value);
        nimcp_platform_rwlock_unlock(&g_config_lock);

        trigger_callbacks(key, &old_value, &new_value);

        // Free temp copy
        nimcp_free(new_value.string_val);
        LOG_MODULE_DEBUG("config", "set_string: %s = %s", key, value);
        return true;
    }

    nimcp_platform_rwlock_unlock(&g_config_lock);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_set_string: operation failed");
    return false;
}

config_stats_t config_get_stats(void) {
    return g_stats;
}

void config_print(void) {
    // Note: Hash table iteration would require additional implementation
    LOG_MODULE_INFO("config", "Config version: %u, Reloads: %lu",
                   g_stats.config_version, (unsigned long)g_stats.reload_count);
}

/**
 * @brief Context for config dump iteration
 */
typedef struct {
    FILE* file;
    bool error;
    size_t count;
} config_dump_context_t;

/**
 * @brief Iterator callback for dumping config entries to file
 *
 * WHAT: Write a single config entry to INI format
 * WHY:  Used by hash_table_iterate to dump all entries
 * HOW:  Format value based on type and write to file
 */
static bool dump_entry_callback(const void* key, size_t key_size,
                                 void* value, size_t value_size,
                                 void* user_data) {
    (void)key_size;
    (void)value_size;

    config_dump_context_t* ctx = (config_dump_context_t*)user_data;
    if (!ctx || !ctx->file || ctx->error) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_print: required parameter is NULL (ctx, ctx->file)");
        return false;  // Stop iteration
    }

    const char* key_str = (const char*)key;
    config_entry_internal_t* entry = (config_entry_internal_t*)value;

    if (!key_str || !entry) {
        return true;  // Skip invalid entry, continue iteration
    }

    int written = 0;
    switch (entry->type) {
        case CONFIG_TYPE_INT:
            written = fprintf(ctx->file, "%s = %lld\n",
                            key_str, (long long)entry->value.int_val);
            break;

        case CONFIG_TYPE_FLOAT:
            written = fprintf(ctx->file, "%s = %.6f\n",
                            key_str, entry->value.float_val);
            break;

        case CONFIG_TYPE_BOOL:
            written = fprintf(ctx->file, "%s = %s\n",
                            key_str, entry->value.bool_val ? "true" : "false");
            break;

        case CONFIG_TYPE_STRING:
            if (entry->value.string_val) {
                written = fprintf(ctx->file, "%s = %s\n",
                                key_str, entry->value.string_val);
            } else {
                written = fprintf(ctx->file, "%s = \n", key_str);
            }
            break;

        case CONFIG_TYPE_ARRAY:
            // Arrays are written as comments indicating presence
            written = fprintf(ctx->file, "# %s = [array - %zu elements]\n",
                            key_str, entry->value.array_val ?
                            config_array_size((config_array_t*)entry->value.array_val) : 0);
            break;

        default:
            LOG_MODULE_WARN("config", "Unknown type for key '%s' in dump", key_str);
            return true;  // Continue iteration
    }

    if (written < 0) {
        ctx->error = true;
        LOG_MODULE_ERROR("config", "Failed to write entry '%s' to dump file", key_str);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_print: validation failed");
        return false;  // Stop iteration
    }

    ctx->count++;
    return true;  // Continue iteration
}

/* Context for config_iterate_entries */
typedef struct {
    config_entry_iterator_fn_t callback;
    void* user_data;
    size_t count;
} config_iterate_context_t;

static bool iterate_entry_adapter(const void* key, size_t key_size,
                                   void* value, size_t value_size,
                                   void* user_data) {
    (void)key_size;
    (void)value_size;
    config_iterate_context_t* ctx = (config_iterate_context_t*)user_data;
    const char* key_str = (const char*)key;
    config_entry_internal_t* entry = (config_entry_internal_t*)value;
    if (!key_str || !entry) return true;

    ctx->count++;
    return ctx->callback(key_str, entry->type, &entry->value, ctx->user_data);
}

size_t config_iterate_entries(config_entry_iterator_fn_t callback, void* user_data) {
    if (!callback || !g_config_table) return 0;

    nimcp_platform_rwlock_rdlock(&g_config_lock);

    config_iterate_context_t ctx = {
        .callback = callback,
        .user_data = user_data,
        .count = 0
    };

    hash_table_iterate(g_config_table, iterate_entry_adapter, &ctx);

    nimcp_platform_rwlock_unlock(&g_config_lock);
    return ctx.count;
}

bool config_dump(const char* output_path) {
    if (!output_path) {
        LOG_MODULE_ERROR("config", "config_dump: NULL output path");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_dump: output_path is NULL");
        return false;
    }

    nimcp_rwlock_rdlock(&g_config_lock);

    if (!g_config_table) {
        nimcp_rwlock_unlock(&g_config_lock);
        LOG_MODULE_ERROR("config", "config_dump: config not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_dump: g_config_table is NULL");
        return false;
    }

    FILE* file = fopen(output_path, "w");
    if (!file) {
        nimcp_rwlock_unlock(&g_config_lock);
        LOG_MODULE_ERROR("config", "config_dump: failed to open '%s' for writing",
                        output_path);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_dump: file is NULL");
        return false;
    }

    // Write header
    fprintf(file, "# NIMCP Configuration Dump\n");
    fprintf(file, "# Version: %u\n", g_stats.config_version);
    fprintf(file, "# Generated by config_dump()\n\n");

    // Iterate and dump all entries
    config_dump_context_t ctx = {
        .file = file,
        .error = false,
        .count = 0
    };

    hash_table_iterate(g_config_table, dump_entry_callback, &ctx);

    nimcp_rwlock_unlock(&g_config_lock);

    // Write footer
    fprintf(file, "\n# Total entries: %zu\n", ctx.count);

    if (fclose(file) != 0) {
        LOG_MODULE_ERROR("config", "config_dump: failed to close file '%s'",
                        output_path);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_dump: validation failed");
        return false;
    }

    if (ctx.error) {
        LOG_MODULE_ERROR("config", "config_dump: errors occurred during dump");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_dump: validation failed");
        return false;
    }

    LOG_MODULE_INFO("config", "Config dumped to '%s' (%zu entries)",
                   output_path, ctx.count);
    return true;
}

bool config_validate(const char* config_path) {
    if (!config_path) {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "config_validate: config_path is NULL");

            return false;

        }

    FILE* file = fopen(config_path, "r");
    if (!file) {
        LOG_MODULE_ERROR("config", "Cannot open config file: %s", config_path);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_validate: file is NULL");
        return false;
    }

    char line[NIMCP_CMD_BUFFER_SIZE];
    int line_num = 0;
    bool valid = true;

    while (fgets(line, sizeof(line), file)) {
        line_num++;
        trim_whitespace(line);

        if (line[0] == '#' || line[0] == ';' || line[0] == '\0') {
            continue;
        }

        char* equals = strchr(line, '=');
        if (!equals) {
            LOG_MODULE_ERROR("config", "Validation error line %d: missing '='", line_num);
            valid = false;
            continue;
        }

        *equals = '\0';
        char* key = line;
        char* value = equals + 1;

        trim_whitespace(key);
        trim_whitespace(value);

        if (key[0] == '\0' || value[0] == '\0') {
            LOG_MODULE_ERROR("config", "Validation error line %d: empty key or value",
                            line_num);
            valid = false;
        }
    }

    fclose(file);
    return valid;
}

void config_set_schema(const config_schema_t* schema) {
    g_config_schema = schema;
    LOG_MODULE_INFO("config", "Config schema set with %zu entries",
                   schema ? schema->num_entries : 0);
}

const config_schema_t* config_get_schema(void) {
    return g_config_schema;
}

// NOTE: config_set_array, config_get_array are implemented in nimcp_config_array.c
// NOTE: config_get_nested_* functions are implemented in nimcp_config_expand.c
// NOTE: config_atomic_reload, config_rollback are implemented in nimcp_config_signal.c

//=============================================================================
// Callback System
//=============================================================================

static void trigger_callbacks(const char* key, const config_value_t* old_value,
                              const config_value_t* new_value) {
    if (!key) return;

    nimcp_mutex_lock(&g_callback_lock);

    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (!g_callbacks[i].in_use || !g_callbacks[i].callback) {
            continue;
        }

        bool matches = (g_callbacks[i].key == NULL) ||
                      (strcmp(g_callbacks[i].key, key) == 0);

        if (matches) {
            nimcp_mutex_unlock(&g_callback_lock);
            g_callbacks[i].callback(key, old_value, new_value, g_callbacks[i].user_data);
            nimcp_mutex_lock(&g_callback_lock);
        }
    }

    nimcp_mutex_unlock(&g_callback_lock);
}

uint32_t config_register_callback(const char* key, config_change_callback_t callback,
                                   void* user_data) {
    if (!callback) {
        LOG_MODULE_ERROR("config", "config_register_callback: NULL callback");
        return 0;
    }

    nimcp_mutex_lock(&g_callback_lock);

    uint32_t slot = 0;
    bool found = false;
    for (uint32_t i = 0; i < MAX_CALLBACKS; i++) {
        if (!g_callbacks[i].in_use) {
            slot = i;
            found = true;
            break;
        }
    }

    if (!found) {
        nimcp_mutex_unlock(&g_callback_lock);
        LOG_MODULE_ERROR("config", "No nimcp_free callback slots (max %d)", MAX_CALLBACKS);
        return 0;
    }

    uint32_t id = g_next_callback_id++;

    g_callbacks[slot].id = id;
    g_callbacks[slot].callback = callback;
    g_callbacks[slot].user_data = user_data;
    g_callbacks[slot].in_use = true;

    if (key) {
        g_callbacks[slot].key = nimcp_strdup(key);
    } else {
        g_callbacks[slot].key = NULL;
    }

    nimcp_mutex_unlock(&g_callback_lock);

    LOG_MODULE_DEBUG("config", "Registered callback ID %u for key '%s'",
                    id, key ? key : "all");
    return id;
}

void config_unregister_callback(uint32_t registration_id) {
    if (registration_id == 0) return;

    nimcp_mutex_lock(&g_callback_lock);

    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (g_callbacks[i].in_use && g_callbacks[i].id == registration_id) {
            if (g_callbacks[i].key) {
                nimcp_free(g_callbacks[i].key);
            }
            g_callbacks[i].in_use = false;
            g_callbacks[i].callback = NULL;
            g_callbacks[i].user_data = NULL;
            g_callbacks[i].key = NULL;
            LOG_MODULE_DEBUG("config", "Unregistered callback ID %u", registration_id);
            break;
        }
    }

    nimcp_mutex_unlock(&g_callback_lock);
}
