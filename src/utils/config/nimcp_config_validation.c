#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_config_validation.c - Configuration Validation Implementation
//=============================================================================
/**
 * @file nimcp_config_validation.c
 * @brief Implementation of config validation with schemas, ranges, and dependencies
 *
 * WHAT: Full-featured config validation framework
 * WHY:  Ensure config integrity before loading into brain modules
 * HOW:  Schema-based validation with range checks, custom validators, dependencies
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 * @version 1.0.0
 */

#include "utils/config/nimcp_config_validation.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/config/nimcp_dynamic_config.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_rwlock.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "security/nimcp_security.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(config_validation)

//=============================================================================
// Internal Data Structures
//=============================================================================

/**
 * @brief Field definition in schema
 *
 * WHAT: Complete metadata for a config field
 * WHY:  Store type, range, validators, dependencies
 * HOW:  Allocated from unified memory pool
 */
typedef struct {
    char key[CONFIG_FIELD_NAME_LEN];           /**< Field name */
    config_value_type_t type;                  /**< Field type */
    bool required;                             /**< Is field required? */

    // Default and range values
    config_value_t default_value;              /**< Default value (if optional) */
    int64_t int_min;                           /**< Min value (int) */
    int64_t int_max;                           /**< Max value (int) */
    double float_min;                          /**< Min value (float) */
    double float_max;                          /**< Max value (float) */
    size_t string_max_len;                     /**< Max length (string) */

    // Custom validators
    config_validator_t validators[CONFIG_MAX_VALIDATORS_PER_FIELD];
    uint32_t validator_count;                  /**< Number of validators */

    // Dependencies
    char dependencies[CONFIG_MAX_DEPENDENCIES_PER_FIELD][CONFIG_FIELD_NAME_LEN];
    config_dependency_validator_t dep_validators[CONFIG_MAX_DEPENDENCIES_PER_FIELD];
    uint32_t dependency_count;                 /**< Number of dependencies */

    bool in_use;                               /**< Is this slot active? */
} config_field_t;

/**
 * @brief Config schema structure
 *
 * WHAT: Container for all field definitions
 * WHY:  Organize schema metadata in memory-efficient structure
 * HOW:  Use unified memory for CoW support, pthread locks for thread safety
 */
struct config_validation_schema_struct {
    uint32_t magic;                            /**< Magic for validation */
    uint32_t version;                          /**< Schema version */

    // Fields
    config_field_t fields[CONFIG_SCHEMA_MAX_FIELDS];
    uint32_t field_count;                      /**< Number of fields */

    // Statistics
    config_schema_stats_t stats;               /**< Schema statistics */

    // Thread safety
    nimcp_platform_rwlock_t lock;                     /**< Read-write lock */

    // Memory integration
    unified_mem_manager_t memory_manager;      /**< Unified memory manager */
    unified_mem_handle_t memory_handle;        /**< Memory handle for this schema */

    // Security integration
    uint32_t security_module_id;               /**< Security module ID */
};

//=============================================================================
// Global State
//=============================================================================

static unified_mem_manager_t g_mem_manager = NULL;
static nimcp_platform_mutex_t g_init_lock = NIMCP_MUTEX_INITIALIZER;
static bool g_initialized = false;
static uint32_t g_security_module_id = 0;

//=============================================================================
// Forward Declarations
//=============================================================================

static bool validate_field(
    config_validation_schema_t schema,
    const config_field_t* field,
    const config_value_t* value,
    config_validation_result_t* result
);

static bool check_dependencies(
    config_validation_schema_t schema,
    const config_field_t* field,
    const config_value_t* value,
    config_validation_result_t* result
);

static bool has_circular_dependency_from(
    config_validation_schema_t schema,
    const char* key,
    const char* target,
    bool* visited
);

static void add_error(
    config_validation_result_t* result,
    const char* format,
    ...
);

static config_field_t* find_field(
    config_validation_schema_t schema,
    const char* key
);

//=============================================================================
// Initialization and Lifecycle
//=============================================================================

/**
 * @brief Initialize validation system
 *
 * WHAT: One-time initialization of global resources
 * WHY:  Setup unified memory and security registration
 * HOW:  Create memory manager, register with security module
 */
static bool ensure_initialized(void) {
    // WHAT: Lazy initialization on first use
    // WHY:  Avoid initialization order issues
    // HOW:  Double-checked locking pattern

    if (g_initialized) {
        return true;
    }

    nimcp_platform_mutex_lock(&g_init_lock);

    if (g_initialized) {
        nimcp_platform_mutex_unlock(&g_init_lock);
        return true;
    }

    // Create unified memory manager for schemas
    unified_mem_config_t mem_config = unified_mem_default_config();
    mem_config.enable_cow = true;
    mem_config.enable_tracking = true;
    g_mem_manager = unified_mem_create(&mem_config);

    if (!g_mem_manager) {
        LOG_ERROR("Failed to create unified memory manager for config validation");
        nimcp_platform_mutex_unlock(&g_init_lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ensure_initialized: g_mem_manager is NULL");
        return false;
    }

    // Register with security module (log security events)
    g_security_module_id = 0;  // TODO: Register when security module supports it

    LOG_INFO("Config validation system initialized");
    g_initialized = true;

    nimcp_platform_mutex_unlock(&g_init_lock);
    return true;
}

config_validation_schema_t config_schema_create(void) {
    // WHAT: Allocate and initialize new schema
    // WHY:  Create container for field definitions
    // HOW:  Use unified memory, initialize locks and statistics

    if (!ensure_initialized()) {
        LOG_ERROR("Failed to initialize validation system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "config_schema_create: ensure_initialized is NULL");
        return NULL;
    }

    // Allocate schema from unified memory
    unified_mem_request_t req = unified_mem_request(
        sizeof(struct config_validation_schema_struct),
        NULL,
        false  // No CoW needed for schema itself
    );

    unified_mem_handle_t handle = unified_mem_alloc(g_mem_manager, &req);
    if (!handle) {
        LOG_ERROR("Failed to allocate memory for schema");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "handle is NULL");

        return NULL;
    }

    config_validation_schema_t schema = (config_validation_schema_t)unified_mem_write(handle);
    if (!schema) {
        LOG_ERROR("Failed to get writable pointer for schema");
        unified_mem_free(handle);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema is NULL");

        return NULL;
    }

    // Initialize schema
    memset(schema, 0, sizeof(struct config_validation_schema_struct));
    schema->magic = CONFIG_VALIDATION_MAGIC;
    schema->version = 1;
    schema->memory_manager = g_mem_manager;
    schema->memory_handle = handle;
    schema->security_module_id = g_security_module_id;

    // Initialize locks
    if (nimcp_platform_rwlock_init(&schema->lock) != 0) {
        LOG_ERROR("Failed to initialize schema lock");
        unified_mem_free(handle);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "config_schema_create: validation failed");
        return NULL;
    }

    LOG_DEBUG("Created config schema %p", (void*)schema);
    return schema;
}

void config_schema_destroy(config_validation_schema_t schema) {
    // WHAT: Free all resources associated with schema
    // WHY:  Clean shutdown
    // HOW:  Destroy lock, free unified memory

    if (!schema || schema->magic != CONFIG_VALIDATION_MAGIC) {
        LOG_WARN("Attempted to destroy invalid schema");
        return;
    }

    LOG_DEBUG("Destroying config schema %p", (void*)schema);

    // Destroy lock
    nimcp_platform_rwlock_destroy(&schema->lock);

    // Invalidate magic
    schema->magic = 0;

    // Free unified memory
    if (schema->memory_handle) {
        unified_mem_free(schema->memory_handle);
    }
}

config_validation_schema_t config_schema_clone(config_validation_schema_t schema) {
    // WHAT: Create deep copy of schema
    // WHY:  Support schema versioning
    // HOW:  Create new schema, copy all fields

    if (!schema || schema->magic != CONFIG_VALIDATION_MAGIC) {
        LOG_ERROR("Invalid schema for cloning");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "config_schema_clone: schema is NULL");
        return NULL;
    }

    config_validation_schema_t new_schema = config_schema_create();
    if (!new_schema) {
        LOG_ERROR("Failed to create new schema for cloning");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "new_schema is NULL");

        return NULL;
    }

    nimcp_platform_rwlock_rdlock(&schema->lock);

    // Copy all fields
    memcpy(new_schema->fields, schema->fields, sizeof(schema->fields));
    new_schema->field_count = schema->field_count;
    new_schema->stats = schema->stats;
    new_schema->version = schema->version + 1;

    nimcp_platform_rwlock_unlock(&schema->lock);

    LOG_DEBUG("Cloned schema %p -> %p (version %u -> %u)",
              (void*)schema, (void*)new_schema,
              schema->version, new_schema->version);

    return new_schema;
}

//=============================================================================
// Field Definition API
//=============================================================================

bool config_schema_add_int(
    config_validation_schema_t schema,
    const char* key,
    bool required,
    int64_t default_value,
    int64_t min,
    int64_t max
) {
    // WHAT: Add integer field with range constraints
    // WHY:  Define expected integer config values
    // HOW:  Store field metadata, validate min <= default <= max

    if (!schema || schema->magic != CONFIG_VALIDATION_MAGIC || !key) {
        LOG_ERROR("Invalid arguments to config_schema_add_int");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_schema_add_int: required parameter is NULL (schema, key)");
        return false;
    }

    if (min > max) {
        LOG_ERROR("Invalid range for field '%s': min=%lld > max=%lld",
                  key, (long long)min, (long long)max);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_schema_add_int: validation failed");
        return false;
    }

    if (!required && (default_value < min || default_value > max)) {
        LOG_ERROR("Default value %lld for field '%s' out of range [%lld, %lld]",
                  (long long)default_value, key, (long long)min, (long long)max);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_schema_add_int: required is NULL");
        return false;
    }

    nimcp_platform_rwlock_wrlock(&schema->lock);

    // Check if field already exists
    config_field_t* field = find_field(schema, key);
    if (!field) {
        // Find free slot
        if (schema->field_count >= CONFIG_SCHEMA_MAX_FIELDS) {
            LOG_ERROR("Schema full, cannot add field '%s'", key);
            nimcp_platform_rwlock_unlock(&schema->lock);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "config_schema_add_int: capacity exceeded");
            return false;
        }

        // Allocate new field
        for (uint32_t i = 0; i < CONFIG_SCHEMA_MAX_FIELDS; i++) {
            if (!schema->fields[i].in_use) {
                field = &schema->fields[i];
                schema->field_count++;
                break;
            }
        }
    }

    if (!field) {
        LOG_ERROR("Failed to allocate field for '%s'", key);
        nimcp_platform_rwlock_unlock(&schema->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "config_schema_add_int: field is NULL");
        return false;
    }

    // Initialize field
    memset(field, 0, sizeof(config_field_t));
    strncpy(field->key, key, CONFIG_FIELD_NAME_LEN - 1);
    field->type = CONFIG_TYPE_INT;
    field->required = required;
    field->default_value.int_val = default_value;
    field->int_min = min;
    field->int_max = max;
    field->in_use = true;

    // Update statistics
    if (required) {
        schema->stats.required_fields++;
    } else {
        schema->stats.optional_fields++;
    }
    schema->stats.total_fields++;

    nimcp_platform_rwlock_unlock(&schema->lock);

    LOG_DEBUG("Added int field '%s': required=%d, default=%lld, range=[%lld,%lld]",
              key, required, (long long)default_value, (long long)min, (long long)max);

    return true;
}

bool config_schema_add_float(
    config_validation_schema_t schema,
    const char* key,
    bool required,
    double default_value,
    double min,
    double max
) {
    // WHAT: Add float field with range constraints
    // WHY:  Define expected float config values
    // HOW:  Store field metadata, validate min <= default <= max

    if (!schema || schema->magic != CONFIG_VALIDATION_MAGIC || !key) {
        LOG_ERROR("Invalid arguments to config_schema_add_float");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_schema_add_float: required parameter is NULL (schema, key)");
        return false;
    }

    if (min > max) {
        LOG_ERROR("Invalid range for field '%s': min=%f > max=%f", key, min, max);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_schema_add_float: validation failed");
        return false;
    }

    if (!required && (default_value < min || default_value > max)) {
        LOG_ERROR("Default value %f for field '%s' out of range [%f, %f]",
                  default_value, key, min, max);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_schema_add_float: required is NULL");
        return false;
    }

    nimcp_platform_rwlock_wrlock(&schema->lock);

    config_field_t* field = find_field(schema, key);
    if (!field) {
        if (schema->field_count >= CONFIG_SCHEMA_MAX_FIELDS) {
            LOG_ERROR("Schema full, cannot add field '%s'", key);
            nimcp_platform_rwlock_unlock(&schema->lock);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "config_schema_add_float: capacity exceeded");
            return false;
        }

        for (uint32_t i = 0; i < CONFIG_SCHEMA_MAX_FIELDS; i++) {
            if (!schema->fields[i].in_use) {
                field = &schema->fields[i];
                schema->field_count++;
                break;
            }
        }
    }

    if (!field) {
        LOG_ERROR("Failed to allocate field for '%s'", key);
        nimcp_platform_rwlock_unlock(&schema->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "config_schema_add_float: field is NULL");
        return false;
    }

    memset(field, 0, sizeof(config_field_t));
    strncpy(field->key, key, CONFIG_FIELD_NAME_LEN - 1);
    field->type = CONFIG_TYPE_FLOAT;
    field->required = required;
    field->default_value.float_val = default_value;
    field->float_min = min;
    field->float_max = max;
    field->in_use = true;

    if (required) {
        schema->stats.required_fields++;
    } else {
        schema->stats.optional_fields++;
    }
    schema->stats.total_fields++;

    nimcp_platform_rwlock_unlock(&schema->lock);

    LOG_DEBUG("Added float field '%s': required=%d, default=%f, range=[%f,%f]",
              key, required, default_value, min, max);

    return true;
}

bool config_schema_add_bool(
    config_validation_schema_t schema,
    const char* key,
    bool required,
    bool default_value
) {
    // WHAT: Add boolean field
    // WHY:  Define expected bool config values
    // HOW:  Store field metadata with default

    if (!schema || schema->magic != CONFIG_VALIDATION_MAGIC || !key) {
        LOG_ERROR("Invalid arguments to config_schema_add_bool");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_schema_add_bool: required parameter is NULL (schema, key)");
        return false;
    }

    nimcp_platform_rwlock_wrlock(&schema->lock);

    config_field_t* field = find_field(schema, key);
    if (!field) {
        if (schema->field_count >= CONFIG_SCHEMA_MAX_FIELDS) {
            LOG_ERROR("Schema full, cannot add field '%s'", key);
            nimcp_platform_rwlock_unlock(&schema->lock);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "config_schema_add_bool: capacity exceeded");
            return false;
        }

        for (uint32_t i = 0; i < CONFIG_SCHEMA_MAX_FIELDS; i++) {
            if (!schema->fields[i].in_use) {
                field = &schema->fields[i];
                schema->field_count++;
                break;
            }
        }
    }

    if (!field) {
        LOG_ERROR("Failed to allocate field for '%s'", key);
        nimcp_platform_rwlock_unlock(&schema->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "config_schema_add_bool: field is NULL");
        return false;
    }

    memset(field, 0, sizeof(config_field_t));
    strncpy(field->key, key, CONFIG_FIELD_NAME_LEN - 1);
    field->type = CONFIG_TYPE_BOOL;
    field->required = required;
    field->default_value.bool_val = default_value;
    field->in_use = true;

    if (required) {
        schema->stats.required_fields++;
    } else {
        schema->stats.optional_fields++;
    }
    schema->stats.total_fields++;

    nimcp_platform_rwlock_unlock(&schema->lock);

    LOG_DEBUG("Added bool field '%s': required=%d, default=%s",
              key, required, default_value ? "true" : "false");

    return true;
}

bool config_schema_add_string(
    config_validation_schema_t schema,
    const char* key,
    bool required,
    const char* default_value,
    size_t max_len
) {
    // WHAT: Add string field with length constraint
    // WHY:  Define expected string config values
    // HOW:  Store field metadata with max length

    if (!schema || schema->magic != CONFIG_VALIDATION_MAGIC || !key) {
        LOG_ERROR("Invalid arguments to config_schema_add_string");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_schema_add_string: required parameter is NULL (schema, key)");
        return false;
    }

    if (!required && default_value && max_len > 0 && strlen(default_value) > max_len) {
        LOG_ERROR("Default value for field '%s' exceeds max length %zu",
                  key, max_len);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_schema_add_string: required is NULL");
        return false;
    }

    nimcp_platform_rwlock_wrlock(&schema->lock);

    config_field_t* field = find_field(schema, key);
    if (!field) {
        if (schema->field_count >= CONFIG_SCHEMA_MAX_FIELDS) {
            LOG_ERROR("Schema full, cannot add field '%s'", key);
            nimcp_platform_rwlock_unlock(&schema->lock);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "config_schema_add_string: capacity exceeded");
            return false;
        }

        for (uint32_t i = 0; i < CONFIG_SCHEMA_MAX_FIELDS; i++) {
            if (!schema->fields[i].in_use) {
                field = &schema->fields[i];
                schema->field_count++;
                break;
            }
        }
    }

    if (!field) {
        LOG_ERROR("Failed to allocate field for '%s'", key);
        nimcp_platform_rwlock_unlock(&schema->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "config_schema_add_string: field is NULL");
        return false;
    }

    memset(field, 0, sizeof(config_field_t));
    strncpy(field->key, key, CONFIG_FIELD_NAME_LEN - 1);
    field->type = CONFIG_TYPE_STRING;
    field->required = required;
    field->string_max_len = max_len;
    field->in_use = true;

    // Note: We don't store string default in field, just in config system

    if (required) {
        schema->stats.required_fields++;
    } else {
        schema->stats.optional_fields++;
    }
    schema->stats.total_fields++;

    nimcp_platform_rwlock_unlock(&schema->lock);

    LOG_DEBUG("Added string field '%s': required=%d, max_len=%zu",
              key, required, max_len);

    return true;
}

//=============================================================================
// Custom Validator API
//=============================================================================

bool config_schema_add_validator(
    config_validation_schema_t schema,
    const char* key,
    config_validator_t validator
) {
    // WHAT: Attach custom validator to field
    // WHY:  Enable application-specific validation
    // HOW:  Store validator callback in field metadata

    if (!schema || schema->magic != CONFIG_VALIDATION_MAGIC || !key || !validator) {
        LOG_ERROR("Invalid arguments to config_schema_add_validator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_schema_add_validator: required parameter is NULL (schema, key, validator)");
        return false;
    }

    nimcp_platform_rwlock_wrlock(&schema->lock);

    config_field_t* field = find_field(schema, key);
    if (!field) {
        LOG_ERROR("Field '%s' not found in schema", key);
        nimcp_platform_rwlock_unlock(&schema->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_schema_add_validator: field is NULL");
        return false;
    }

    if (field->validator_count >= CONFIG_MAX_VALIDATORS_PER_FIELD) {
        LOG_ERROR("Max validators reached for field '%s'", key);
        nimcp_platform_rwlock_unlock(&schema->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "config_schema_add_validator: capacity exceeded");
        return false;
    }

    field->validators[field->validator_count++] = validator;
    schema->stats.validators_registered++;

    nimcp_platform_rwlock_unlock(&schema->lock);

    LOG_DEBUG("Added validator to field '%s' (total: %u)",
              key, field->validator_count);

    return true;
}

bool config_schema_remove_validator(
    config_validation_schema_t schema,
    const char* key,
    config_validator_t validator
) {
    // WHAT: Remove custom validator from field
    // WHY:  Support dynamic validator management
    // HOW:  Find and remove validator from array

    if (!schema || schema->magic != CONFIG_VALIDATION_MAGIC || !key || !validator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_schema_remove_validator: required parameter is NULL (schema, key, validator)");
        return false;
    }

    nimcp_platform_rwlock_wrlock(&schema->lock);

    config_field_t* field = find_field(schema, key);
    if (!field) {
        nimcp_platform_rwlock_unlock(&schema->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_schema_remove_validator: field is NULL");
        return false;
    }

    // Find and remove validator
    bool found = false;
    for (uint32_t i = 0; i < field->validator_count; i++) {
        if (field->validators[i] == validator) {
            // Shift remaining validators down
            for (uint32_t j = i; j < field->validator_count - 1; j++) {
                field->validators[j] = field->validators[j + 1];
            }
            field->validator_count--;
            schema->stats.validators_registered--;
            found = true;
            break;
        }
    }

    nimcp_platform_rwlock_unlock(&schema->lock);
    return found;
}

//=============================================================================
// Dependency API
//=============================================================================

bool config_schema_add_dependency(
    config_validation_schema_t schema,
    const char* key,
    const char* depends_on
) {
    // WHAT: Add field dependency
    // WHY:  Enable cross-field validation
    // HOW:  Store dependency, check for circular dependencies

    if (!schema || schema->magic != CONFIG_VALIDATION_MAGIC || !key || !depends_on) {
        LOG_ERROR("Invalid arguments to config_schema_add_dependency");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_schema_add_dependency: required parameter is NULL (schema, key, depends_on)");
        return false;
    }

    nimcp_platform_rwlock_wrlock(&schema->lock);

    config_field_t* field = find_field(schema, key);
    if (!field) {
        LOG_ERROR("Field '%s' not found in schema", key);
        nimcp_platform_rwlock_unlock(&schema->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_schema_add_dependency: field is NULL");
        return false;
    }

    if (field->dependency_count >= CONFIG_MAX_DEPENDENCIES_PER_FIELD) {
        LOG_ERROR("Max dependencies reached for field '%s'", key);
        nimcp_platform_rwlock_unlock(&schema->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "config_schema_add_dependency: capacity exceeded");
        return false;
    }

    // Check for circular dependency
    bool visited[CONFIG_SCHEMA_MAX_FIELDS] = {false};
    if (has_circular_dependency_from(schema, depends_on, key, visited)) {
        LOG_ERROR("Circular dependency detected: %s -> %s", key, depends_on);
        nimcp_platform_rwlock_unlock(&schema->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_schema_add_dependency: validation failed");
        return false;
    }

    strncpy(field->dependencies[field->dependency_count],
            depends_on, CONFIG_FIELD_NAME_LEN - 1);
    field->dep_validators[field->dependency_count] = NULL;
    field->dependency_count++;
    schema->stats.dependencies_registered++;

    nimcp_platform_rwlock_unlock(&schema->lock);

    LOG_DEBUG("Added dependency: '%s' depends on '%s'", key, depends_on);

    return true;
}

bool config_schema_add_dependency_with_constraint(
    config_validation_schema_t schema,
    const char* key,
    const char* depends_on,
    config_dependency_validator_t constraint
) {
    // WHAT: Add dependency with constraint validator
    // WHY:  Enable complex inter-field validation
    // HOW:  Store dependency + validator

    if (!schema || schema->magic != CONFIG_VALIDATION_MAGIC || !key || !depends_on) {
        LOG_ERROR("Invalid arguments to config_schema_add_dependency_with_constraint");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_schema_add_dependency_with_constraint: required parameter is NULL (schema, key, depends_on)");
        return false;
    }

    nimcp_platform_rwlock_wrlock(&schema->lock);

    config_field_t* field = find_field(schema, key);
    if (!field) {
        LOG_ERROR("Field '%s' not found in schema", key);
        nimcp_platform_rwlock_unlock(&schema->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_schema_add_dependency_with_constraint: field is NULL");
        return false;
    }

    if (field->dependency_count >= CONFIG_MAX_DEPENDENCIES_PER_FIELD) {
        LOG_ERROR("Max dependencies reached for field '%s'", key);
        nimcp_platform_rwlock_unlock(&schema->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "config_schema_add_dependency_with_constraint: capacity exceeded");
        return false;
    }

    // Check for circular dependency
    bool visited[CONFIG_SCHEMA_MAX_FIELDS] = {false};
    if (has_circular_dependency_from(schema, depends_on, key, visited)) {
        LOG_ERROR("Circular dependency detected: %s -> %s", key, depends_on);
        nimcp_platform_rwlock_unlock(&schema->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_schema_add_dependency_with_constraint: validation failed");
        return false;
    }

    strncpy(field->dependencies[field->dependency_count],
            depends_on, CONFIG_FIELD_NAME_LEN - 1);
    field->dep_validators[field->dependency_count] = constraint;
    field->dependency_count++;
    schema->stats.dependencies_registered++;

    nimcp_platform_rwlock_unlock(&schema->lock);

    LOG_DEBUG("Added dependency with constraint: '%s' depends on '%s'",
              key, depends_on);

    return true;
}

bool config_schema_remove_dependency(
    config_validation_schema_t schema,
    const char* key,
    const char* depends_on
) {
    // WHAT: Remove field dependency
    // WHY:  Support dynamic dependency management
    // HOW:  Find and remove dependency from array

    if (!schema || schema->magic != CONFIG_VALIDATION_MAGIC || !key || !depends_on) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_schema_remove_dependency: required parameter is NULL (schema, key, depends_on)");
        return false;
    }

    nimcp_platform_rwlock_wrlock(&schema->lock);

    config_field_t* field = find_field(schema, key);
    if (!field) {
        nimcp_platform_rwlock_unlock(&schema->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_schema_remove_dependency: field is NULL");
        return false;
    }

    bool found = false;
    for (uint32_t i = 0; i < field->dependency_count; i++) {
        if (strcmp(field->dependencies[i], depends_on) == 0) {
            // Shift remaining dependencies down
            for (uint32_t j = i; j < field->dependency_count - 1; j++) {
                strncpy(field->dependencies[j], field->dependencies[j + 1],
                       CONFIG_FIELD_NAME_LEN);
                field->dep_validators[j] = field->dep_validators[j + 1];
            }
            field->dependency_count--;
            schema->stats.dependencies_registered--;
            found = true;
            break;
        }
    }

    nimcp_platform_rwlock_unlock(&schema->lock);
    return found;
}

bool config_schema_has_circular_dependencies(config_validation_schema_t schema) {
    // WHAT: Check entire schema for circular dependencies
    // WHY:  Prevent infinite loops during validation
    // HOW:  DFS from each field

    if (!schema || schema->magic != CONFIG_VALIDATION_MAGIC) {
        return false;
    }

    nimcp_platform_rwlock_rdlock(&schema->lock);

    for (uint32_t i = 0; i < CONFIG_SCHEMA_MAX_FIELDS; i++) {
        if (!schema->fields[i].in_use) {
            continue;
        }

        bool visited[CONFIG_SCHEMA_MAX_FIELDS] = {false};
        if (has_circular_dependency_from(schema, schema->fields[i].key,
                                         schema->fields[i].key, visited)) {
            nimcp_platform_rwlock_unlock(&schema->lock);
            return true;
        }
    }

    nimcp_platform_rwlock_unlock(&schema->lock);
    return false;
}

//=============================================================================
// Validation API
//=============================================================================

bool config_validate_against_schema(
    config_validation_schema_t schema,
    config_validation_result_t* result
) {
    // WHAT: Comprehensive validation against schema
    // WHY:  Ensure config integrity
    // HOW:  Check required fields, types, ranges, validators, dependencies

    if (!schema || schema->magic != CONFIG_VALIDATION_MAGIC) {
        LOG_ERROR("Invalid schema for validation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_validate_against_schema: schema is NULL");
        return false;
    }

    if (result) {
        config_validation_clear_result(result);
    }

    nimcp_platform_rwlock_rdlock(&schema->lock);

    schema->stats.validations_performed++;
    bool all_valid = true;

    // Validate each field in schema
    for (uint32_t i = 0; i < CONFIG_SCHEMA_MAX_FIELDS; i++) {
        if (!schema->fields[i].in_use) {
            continue;
        }

        const config_field_t* field = &schema->fields[i];

        // Get current value from config system
        config_value_t value;
        bool has_value = false;

        switch (field->type) {
            case CONFIG_TYPE_INT:
                value.int_val = config_get_int(field->key, field->default_value.int_val);
                has_value = true;
                break;
            case CONFIG_TYPE_FLOAT:
                value.float_val = config_get_float(field->key, field->default_value.float_val);
                has_value = true;
                break;
            case CONFIG_TYPE_BOOL:
                value.bool_val = config_get_bool(field->key, field->default_value.bool_val);
                has_value = true;
                break;
            case CONFIG_TYPE_STRING:
                value.string_val = (char*)config_get_string(field->key, NULL);
                has_value = (value.string_val != NULL);
                break;
        }

        // Check required field
        if (field->required && !has_value) {
            add_error(result, "Required field '%s' is missing", field->key);
            all_valid = false;
            continue;
        }

        if (!has_value) {
            continue;  // Optional field missing, skip validation
        }

        // Validate field
        if (!validate_field(schema, field, &value, result)) {
            all_valid = false;
        }

        // Check dependencies
        if (!check_dependencies(schema, field, &value, result)) {
            all_valid = false;
        }
    }

    if (all_valid) {
        schema->stats.validations_passed++;
    } else {
        schema->stats.validations_failed++;
    }

    nimcp_platform_rwlock_unlock(&schema->lock);

    if (result) {
        result->valid = all_valid;
    }

    if (!all_valid) {
        LOG_WARN("Config validation failed with %u errors",
                 result ? result->error_count : 0);
    } else {
        LOG_INFO("Config validation passed");
    }

    return all_valid;
}

bool config_validate_value_range(
    const char* key,
    const config_value_t* value,
    config_value_type_t type,
    int64_t min,
    int64_t max,
    char* error,
    size_t error_size
) {
    // WHAT: Standalone range validation
    // WHY:  Quick validation without full schema
    // HOW:  Check value against min/max bounds

    if (!key || !value || !error) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_validate_value_range: required parameter is NULL (key, value, error)");
        return false;
    }

    error[0] = '\0';

    switch (type) {
        case CONFIG_TYPE_INT:
            if (value->int_val < min || value->int_val > max) {
                snprintf(error, error_size,
                        "Field '%s' value %lld out of range [%lld, %lld]",
                        key, (long long)value->int_val,
                        (long long)min, (long long)max);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_validate_value_range: validation failed");
                return false;
            }
            break;

        case CONFIG_TYPE_FLOAT: {
            double fmin = (double)min;
            double fmax = (double)max;
            if (value->float_val < fmin || value->float_val > fmax) {
                snprintf(error, error_size,
                        "Field '%s' value %f out of range [%f, %f]",
                        key, value->float_val, fmin, fmax);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_validate_value_range: validation failed");
                return false;
            }
            break;
        }

        case CONFIG_TYPE_BOOL:
            // Bool always valid
            break;

        case CONFIG_TYPE_STRING:
            // Use max as string length limit
            if (value->string_val && strlen(value->string_val) > (size_t)max) {
                snprintf(error, error_size,
                        "Field '%s' string too long: %zu > %lld",
                        key, strlen(value->string_val), (long long)max);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_validate_value_range: validation failed");
                return false;
            }
            break;
    }

    return true;
}

bool config_validate_string_length(
    const char* key,
    const char* value,
    size_t max_len,
    char* error,
    size_t error_size
) {
    // WHAT: Validate string length
    // WHY:  Prevent buffer overflows
    // HOW:  Check strlen against max

    if (!key || !value || !error) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_validate_string_length: required parameter is NULL (key, value, error)");
        return false;
    }

    size_t len = strlen(value);
    if (max_len > 0 && len > max_len) {
        snprintf(error, error_size,
                "Field '%s' string too long: %zu > %zu",
                key, len, max_len);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_validate_string_length: validation failed");
        return false;
    }

    return true;
}

bool config_apply_defaults(config_validation_schema_t schema) {
    // WHAT: Apply default values to config
    // WHY:  Ensure all fields have valid values
    // HOW:  Iterate schema, set defaults for missing fields

    if (!schema || schema->magic != CONFIG_VALIDATION_MAGIC) {
        LOG_ERROR("Invalid schema for apply defaults");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_apply_defaults: schema is NULL");
        return false;
    }

    nimcp_platform_rwlock_rdlock(&schema->lock);

    for (uint32_t i = 0; i < CONFIG_SCHEMA_MAX_FIELDS; i++) {
        if (!schema->fields[i].in_use || schema->fields[i].required) {
            continue;
        }

        const config_field_t* field = &schema->fields[i];

        // Check if value already exists
        bool exists = false;
        switch (field->type) {
            case CONFIG_TYPE_INT:
                exists = (config_get_int(field->key, INT64_MIN) != INT64_MIN);
                if (!exists) {
                    config_set_int(field->key, field->default_value.int_val);
                }
                break;
            case CONFIG_TYPE_FLOAT:
                exists = !isnan(config_get_float(field->key, NAN));
                if (!exists) {
                    config_set_float(field->key, field->default_value.float_val);
                }
                break;
            case CONFIG_TYPE_BOOL:
                // Bool always exists with default
                config_set_bool(field->key, field->default_value.bool_val);
                break;
            case CONFIG_TYPE_STRING:
                exists = (config_get_string(field->key, NULL) != NULL);
                // String defaults not stored in field, skip
                break;
        }
    }

    nimcp_platform_rwlock_unlock(&schema->lock);

    LOG_DEBUG("Applied defaults to config from schema");
    return true;
}

//=============================================================================
// Query API
//=============================================================================

bool config_schema_has_field(config_validation_schema_t schema, const char* key) {
    if (!schema || schema->magic != CONFIG_VALIDATION_MAGIC || !key) {
        return false;
    }

    nimcp_platform_rwlock_rdlock(&schema->lock);
    bool found = (find_field(schema, key) != NULL);
    nimcp_platform_rwlock_unlock(&schema->lock);

    return found;
}

bool config_schema_get_field_type(
    config_validation_schema_t schema,
    const char* key,
    config_value_type_t* type
) {
    if (!schema || schema->magic != CONFIG_VALIDATION_MAGIC || !key || !type) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_schema_get_field_type: required parameter is NULL (schema, key, type)");
        return false;
    }

    nimcp_platform_rwlock_rdlock(&schema->lock);

    config_field_t* field = find_field(schema, key);
    if (field) {
        *type = field->type;
    }

    nimcp_platform_rwlock_unlock(&schema->lock);

    return (field != NULL);
}

bool config_schema_is_field_required(config_validation_schema_t schema, const char* key) {
    if (!schema || schema->magic != CONFIG_VALIDATION_MAGIC || !key) {
        return false;
    }

    nimcp_platform_rwlock_rdlock(&schema->lock);

    config_field_t* field = find_field(schema, key);
    bool required = (field && field->required);

    nimcp_platform_rwlock_unlock(&schema->lock);

    return required;
}

bool config_schema_get_stats(
    config_validation_schema_t schema,
    config_schema_stats_t* stats
) {
    if (!schema || schema->magic != CONFIG_VALIDATION_MAGIC || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_schema_get_stats: required parameter is NULL (schema, stats)");
        return false;
    }

    nimcp_platform_rwlock_rdlock(&schema->lock);
    *stats = schema->stats;
    nimcp_platform_rwlock_unlock(&schema->lock);

    return true;
}

//=============================================================================
// Utility Functions
//=============================================================================

void config_validation_print_errors(const config_validation_result_t* result) {
    if (!result || result->error_count == 0) {
        return;
    }

    fprintf(stderr, "Config validation errors:\n");
    for (uint32_t i = 0; i < result->error_count; i++) {
        fprintf(stderr, "  [%u] %s\n", i + 1, result->errors[i]);
    }
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

static config_field_t* find_field(config_validation_schema_t schema, const char* key) {
    // WHAT: Find field by key name
    // WHY:  Lookup field metadata
    // HOW:  Linear search through fields

    for (uint32_t i = 0; i < CONFIG_SCHEMA_MAX_FIELDS; i++) {
        if (schema->fields[i].in_use &&
            strcmp(schema->fields[i].key, key) == 0) {
            return &schema->fields[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_field: operation failed");
    return NULL;
}

static bool validate_field(
    config_validation_schema_t schema,
    const config_field_t* field,
    const config_value_t* value,
    config_validation_result_t* result
) {
    // WHAT: Validate single field value
    // WHY:  Check type, range, custom validators
    // HOW:  Sequential validation steps

    bool valid = true;
    char error[CONFIG_ERROR_MESSAGE_LEN];

    // Check range
    switch (field->type) {
        case CONFIG_TYPE_INT:
            if (value->int_val < field->int_min || value->int_val > field->int_max) {
                add_error(result,
                         "Field '%s' value %lld out of range [%lld, %lld]",
                         field->key, (long long)value->int_val,
                         (long long)field->int_min, (long long)field->int_max);
                valid = false;
            }
            break;

        case CONFIG_TYPE_FLOAT:
            if (value->float_val < field->float_min || value->float_val > field->float_max) {
                add_error(result,
                         "Field '%s' value %f out of range [%f, %f]",
                         field->key, value->float_val,
                         field->float_min, field->float_max);
                valid = false;
            }
            break;

        case CONFIG_TYPE_STRING:
            if (field->string_max_len > 0 &&
                value->string_val &&
                strlen(value->string_val) > field->string_max_len) {
                add_error(result,
                         "Field '%s' string too long: %zu > %zu",
                         field->key, strlen(value->string_val),
                         field->string_max_len);
                valid = false;
            }
            break;

        case CONFIG_TYPE_BOOL:
            // Bool always valid
            break;
    }

    // Run custom validators
    for (uint32_t i = 0; i < field->validator_count; i++) {
        if (!field->validators[i](field->key, value, error, sizeof(error))) {
            add_error(result, "%s", error);
            valid = false;
        }
    }

    return valid;
}

static bool check_dependencies(
    config_validation_schema_t schema,
    const config_field_t* field,
    const config_value_t* value,
    config_validation_result_t* result
) {
    // WHAT: Check all dependencies for a field
    // WHY:  Validate inter-field constraints
    // HOW:  Iterate dependencies, check constraints

    bool valid = true;

    for (uint32_t i = 0; i < field->dependency_count; i++) {
        const char* dep_key = field->dependencies[i];

        // Get dependency field
        config_field_t* dep_field = find_field(schema, dep_key);
        if (!dep_field) {
            add_error(result,
                     "Field '%s' depends on non-existent field '%s'",
                     field->key, dep_key);
            valid = false;
            continue;
        }

        // Get dependency value
        config_value_t dep_value;
        bool has_dep = false;

        switch (dep_field->type) {
            case CONFIG_TYPE_INT:
                dep_value.int_val = config_get_int(dep_key, dep_field->default_value.int_val);
                has_dep = true;
                break;
            case CONFIG_TYPE_FLOAT:
                dep_value.float_val = config_get_float(dep_key, dep_field->default_value.float_val);
                has_dep = true;
                break;
            case CONFIG_TYPE_BOOL:
                dep_value.bool_val = config_get_bool(dep_key, dep_field->default_value.bool_val);
                has_dep = true;
                break;
            case CONFIG_TYPE_STRING:
                dep_value.string_val = (char*)config_get_string(dep_key, NULL);
                has_dep = (dep_value.string_val != NULL);
                break;
        }

        if (!has_dep) {
            add_error(result,
                     "Field '%s' depends on missing field '%s'",
                     field->key, dep_key);
            valid = false;
            continue;
        }

        // Run constraint validator if present
        if (field->dep_validators[i]) {
            char error[CONFIG_ERROR_MESSAGE_LEN];
            if (!field->dep_validators[i](field->key, value, dep_key, &dep_value,
                                          error, sizeof(error))) {
                add_error(result, "%s", error);
                valid = false;
            }
        }
    }

    return valid;
}

static bool has_circular_dependency_from(
    config_validation_schema_t schema,
    const char* key,
    const char* target,
    bool* visited
) {
    // WHAT: DFS to detect circular dependencies
    // WHY:  Prevent infinite loops
    // HOW:  Recursive depth-first search

    config_field_t* field = find_field(schema, key);
    if (!field) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "has_circular_dependency_from: field is NULL");
        return false;
    }

    // Mark visited
    for (uint32_t i = 0; i < CONFIG_SCHEMA_MAX_FIELDS; i++) {
        if (schema->fields[i].in_use && strcmp(schema->fields[i].key, key) == 0) {
            if (visited[i]) {
                return strcmp(key, target) == 0;
            }
            visited[i] = true;
            break;
        }
    }

    // Check dependencies
    for (uint32_t i = 0; i < field->dependency_count; i++) {
        if (strcmp(field->dependencies[i], target) == 0) {
            return true;  // Found cycle
        }

        if (has_circular_dependency_from(schema, field->dependencies[i],
                                         target, visited)) {
            return true;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "has_circular_dependency_from: operation failed");
    return false;
}

static void add_error(
    config_validation_result_t* result,
    const char* format,
    ...
) {
    // WHAT: Add error message to validation result
    // WHY:  Collect all errors for reporting
    // HOW:  Format message, append to result

    if (!result || result->error_count >= CONFIG_MAX_VALIDATION_ERRORS) {
        return;
    }

    va_list args;
    va_start(args, format);
    vsnprintf(result->errors[result->error_count],
             CONFIG_ERROR_MESSAGE_LEN, format, args);
    va_end(args);

    result->error_count++;
    result->valid = false;
}
