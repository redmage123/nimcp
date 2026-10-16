//=============================================================================
// nimcp_config_validation.h - Configuration Validation and Schema System
//=============================================================================
/**
 * @file nimcp_config_validation.h
 * @brief Comprehensive config validation with schemas, ranges, and dependencies
 *
 * WHAT: Config validation framework with schema definition, range checks, custom validators
 * WHY:  Ensure config integrity before loading, prevent invalid configurations
 * HOW:  Define schemas with types/ranges/defaults, validate against schema, check dependencies
 *
 * ARCHITECTURE:
 *
 *   Config Validation Pipeline:
 *   ┌──────────────────────────────────────────────────────────────┐
 *   │                    Config Loading                            │
 *   │             (YAML/JSON/INI Parser)                          │
 *   └───────────────────────┬──────────────────────────────────────┘
 *                           │
 *                           ▼
 *   ┌──────────────────────────────────────────────────────────────┐
 *   │                Schema Validation                             │
 *   │  • Check required fields present                             │
 *   │  • Verify field types match schema                           │
 *   │  • Apply defaults for missing optional fields                │
 *   └───────────────────────┬──────────────────────────────────────┘
 *                           │
 *                           ▼
 *   ┌──────────────────────────────────────────────────────────────┐
 *   │                Range Validation                              │
 *   │  • Integer: min ≤ value ≤ max                                │
 *   │  • Float: min ≤ value ≤ max                                  │
 *   │  • String: length ≤ max_len                                  │
 *   │  • Bool: true/false only                                     │
 *   └───────────────────────┬──────────────────────────────────────┘
 *                           │
 *                           ▼
 *   ┌──────────────────────────────────────────────────────────────┐
 *   │            Custom Validator Callbacks                        │
 *   │  • User-defined validation logic                             │
 *   │  • Cross-field validation                                    │
 *   │  • Business logic constraints                                │
 *   └───────────────────────┬──────────────────────────────────────┘
 *                           │
 *                           ▼
 *   ┌──────────────────────────────────────────────────────────────┐
 *   │           Dependency Validation                              │
 *   │  • Check dependencies exist                                  │
 *   │  • Validate dependency constraints                           │
 *   │  • Detect circular dependencies                              │
 *   └───────────────────────┬──────────────────────────────────────┘
 *                           │
 *                           ▼
 *   ┌──────────────────────────────────────────────────────────────┐
 *   │            Validation Result                                 │
 *   │  • Valid: config accepted                                    │
 *   │  • Invalid: error messages with field names                  │
 *   └──────────────────────────────────────────────────────────────┘
 *
 * FEATURES:
 * - Schema-based validation with type checking
 * - Range validation for numeric types (min/max)
 * - String length validation
 * - Default value application for missing fields
 * - Custom validator callbacks for complex logic
 * - Inter-field dependency checking
 * - Circular dependency detection
 * - Detailed error reporting with field names
 * - Thread-safe schema access
 * - Unified memory integration
 * - Security module registration
 * - Full logging support
 *
 * USE CASES:
 * 1. Brain Config Validation: Ensure num_inputs > 0, learning_rate in [0, 1]
 * 2. Training Config: Validate batch_size is power of 2, epochs > 0
 * 3. Network Config: Validate IP addresses, port ranges
 * 4. Cross-Field: If enable_bcm=true, bcm_tau must be > 0
 * 5. Dependency: If early_stopping=true, patience must be set
 *
 * THREAD SAFETY:
 * - All operations are thread-safe
 * - Read-write locks for schema access
 * - Atomic reference counting for schemas
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 * @version 1.0.0
 */

#ifndef NIMCP_CONFIG_VALIDATION_H
#define NIMCP_CONFIG_VALIDATION_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "utils/config/nimcp_dynamic_config.h"

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

/** @brief Maximum number of fields in a schema */
#define CONFIG_SCHEMA_MAX_FIELDS 256

/** @brief Maximum number of custom validators per field */
#define CONFIG_MAX_VALIDATORS_PER_FIELD 8

/** @brief Maximum number of dependencies per field */
#define CONFIG_MAX_DEPENDENCIES_PER_FIELD 8

/** @brief Maximum number of validation errors to collect */
#define CONFIG_MAX_VALIDATION_ERRORS 16

/** @brief Maximum length of error messages */
#define CONFIG_ERROR_MESSAGE_LEN 256

/** @brief Maximum length of field names */
#define CONFIG_FIELD_NAME_LEN 128

/** @brief Magic value for validation */
#define CONFIG_VALIDATION_MAGIC 0x56414C44  // 'VALD'

//=============================================================================
// Types and Enumerations
//=============================================================================

/**
 * @brief Validation schema handle (opaque)
 *
 * NOTE: Named config_validation_schema_t to avoid conflict with
 *       config_validation_schema_t in nimcp_dynamic_config.h
 */
typedef struct config_validation_schema_struct* config_validation_schema_t;

/**
 * @brief Custom validator callback function type
 *
 * WHAT: User-defined validation function for complex logic
 * WHY:  Enable application-specific validation rules
 * HOW:  Return true if valid, false with error message if invalid
 *
 * @param key Field name being validated
 * @param value Field value
 * @param error Output buffer for error message (if invalid)
 * @param error_size Size of error buffer
 * @return true if valid, false if invalid (must fill error buffer)
 *
 * EXAMPLE:
 * ```c
 * bool validate_power_of_2(const char* key, const config_value_t* value,
 *                          char* error, size_t error_size) {
 *     int64_t val = value->int_val;
 *     if (val <= 0 || (val & (val - 1)) != 0) {
 *         snprintf(error, error_size, "%s must be a power of 2", key);
 *         return false;
 *     }
 *     return true;
 * }
 * ```
 */
typedef bool (*config_validator_t)(
    const char* key,
    const config_value_t* value,
    char* error,
    size_t error_size
);

/**
 * @brief Dependency constraint callback function type
 *
 * WHAT: User-defined function to check dependency constraints
 * WHY:  Enable complex inter-field validation logic
 * HOW:  Return true if dependency constraint is satisfied
 *
 * @param key Field name being validated
 * @param value Field value
 * @param dependency_key Dependency field name
 * @param dependency_value Dependency field value
 * @param error Output buffer for error message (if invalid)
 * @param error_size Size of error buffer
 * @return true if constraint satisfied, false if violated
 *
 * EXAMPLE:
 * ```c
 * bool validate_bcm_dependency(const char* key, const config_value_t* value,
 *                               const char* dep_key, const config_value_t* dep_value,
 *                               char* error, size_t error_size) {
 *     // If enable_bcm=true, bcm_tau must be > 0
 *     if (dep_value->bool_val == true && value->float_val <= 0.0) {
 *         snprintf(error, error_size, "bcm_tau must be > 0 when enable_bcm is true");
 *         return false;
 *     }
 *     return true;
 * }
 * ```
 */
typedef bool (*config_dependency_validator_t)(
    const char* key,
    const config_value_t* value,
    const char* dependency_key,
    const config_value_t* dependency_value,
    char* error,
    size_t error_size
);

/**
 * @brief Validation result structure
 *
 * WHAT: Detailed validation result with error messages
 * WHY:  Provide actionable feedback for invalid configs
 * HOW:  Collect all validation errors during validation pass
 */
typedef struct {
    bool valid;                                         /**< Overall validation result */
    uint32_t error_count;                               /**< Number of errors found */
    char errors[CONFIG_MAX_VALIDATION_ERRORS][CONFIG_ERROR_MESSAGE_LEN]; /**< Error messages */
} config_validation_result_t;

/**
 * @brief Schema statistics
 */
typedef struct {
    uint32_t total_fields;                              /**< Total fields in schema */
    uint32_t required_fields;                           /**< Number of required fields */
    uint32_t optional_fields;                           /**< Number of optional fields */
    uint32_t validators_registered;                     /**< Total custom validators */
    uint32_t dependencies_registered;                   /**< Total dependencies */
    uint64_t validations_performed;                     /**< Total validations run */
    uint64_t validations_passed;                        /**< Successful validations */
    uint64_t validations_failed;                        /**< Failed validations */
} config_schema_stats_t;

//=============================================================================
// Schema Lifecycle API
//=============================================================================

/**
 * @brief Create config schema
 *
 * WHAT: Creates new schema for config validation
 * WHY:  Define expected config structure before loading
 * HOW:  Allocates schema with unified memory, initializes fields
 *
 * @return Schema handle or NULL on failure
 *
 * COMPLEXITY: O(1)
 * MEMORY: ~16KB per schema (depends on field count)
 *
 * EXAMPLE:
 * ```c
 * config_validation_schema_t schema = config_schema_create();
 * config_schema_add_int(schema, "num_inputs", true, 256, 1, 100000);
 * config_schema_add_float(schema, "learning_rate", true, 0.001, 0.0, 1.0);
 * ```
 */
NIMCP_EXPORT config_validation_schema_t config_validation_schema_create(void);

/**
 * @brief Destroy config schema
 *
 * WHAT: Frees all resources associated with schema
 * WHY:  Clean shutdown
 * HOW:  Releases unified memory, frees validators
 *
 * @param schema Schema handle
 *
 * NOTE: All validation results from this schema become invalid after destroy
 */
NIMCP_EXPORT void config_schema_destroy(config_validation_schema_t schema);

/**
 * @brief Clone config schema
 *
 * WHAT: Creates deep copy of schema
 * WHY:  Support schema versioning and inheritance
 * HOW:  Copies all fields, validators, and dependencies
 *
 * @param schema Schema to clone
 * @return New schema handle or NULL on failure
 */
NIMCP_EXPORT config_validation_schema_t config_schema_clone(config_validation_schema_t schema);

//=============================================================================
// Schema Field Definition API
//=============================================================================

/**
 * @brief Add integer field to schema
 *
 * WHAT: Define integer field with range constraints
 * WHY:  Validate integer config values
 * HOW:  Store field metadata with min/max bounds
 *
 * @param schema Schema handle
 * @param key Field name
 * @param required Whether field is required (error if missing)
 * @param default_value Default value if not present (ignored if required)
 * @param min Minimum allowed value (inclusive)
 * @param max Maximum allowed value (inclusive)
 * @return true on success, false on failure
 *
 * EXAMPLE:
 * ```c
 * // Batch size: required, default 32, range [1, 10000]
 * config_schema_add_int(schema, "batch_size", true, 32, 1, 10000);
 * ```
 */
NIMCP_EXPORT bool config_schema_add_int(
    config_validation_schema_t schema,
    const char* key,
    bool required,
    int64_t default_value,
    int64_t min,
    int64_t max
);

/**
 * @brief Add float field to schema
 *
 * WHAT: Define float field with range constraints
 * WHY:  Validate floating-point config values
 * HOW:  Store field metadata with min/max bounds
 *
 * @param schema Schema handle
 * @param key Field name
 * @param required Whether field is required
 * @param default_value Default value if not present
 * @param min Minimum allowed value (inclusive)
 * @param max Maximum allowed value (inclusive)
 * @return true on success, false on failure
 *
 * EXAMPLE:
 * ```c
 * // Learning rate: required, default 0.001, range [0.0, 1.0]
 * config_schema_add_float(schema, "learning_rate", true, 0.001, 0.0, 1.0);
 * ```
 */
NIMCP_EXPORT bool config_schema_add_float(
    config_validation_schema_t schema,
    const char* key,
    bool required,
    double default_value,
    double min,
    double max
);

/**
 * @brief Add boolean field to schema
 *
 * WHAT: Define boolean field
 * WHY:  Validate true/false config values
 * HOW:  Store field metadata with default
 *
 * @param schema Schema handle
 * @param key Field name
 * @param required Whether field is required
 * @param default_value Default value if not present
 * @return true on success, false on failure
 *
 * EXAMPLE:
 * ```c
 * // Enable BCM: optional, default true
 * config_schema_add_bool(schema, "enable_bcm", false, true);
 * ```
 */
NIMCP_EXPORT bool config_schema_add_bool(
    config_validation_schema_t schema,
    const char* key,
    bool required,
    bool default_value
);

/**
 * @brief Add string field to schema
 *
 * WHAT: Define string field with length constraint
 * WHY:  Validate string config values
 * HOW:  Store field metadata with max length
 *
 * @param schema Schema handle
 * @param key Field name
 * @param required Whether field is required
 * @param default_value Default value if not present (NULL for empty)
 * @param max_len Maximum string length (0 = unlimited)
 * @return true on success, false on failure
 *
 * EXAMPLE:
 * ```c
 * // Model path: required, max 256 chars
 * config_schema_add_string(schema, "model_path", true, "/tmp/brain.model", 256);
 * ```
 */
NIMCP_EXPORT bool config_schema_add_string(
    config_validation_schema_t schema,
    const char* key,
    bool required,
    const char* default_value,
    size_t max_len
);

//=============================================================================
// Custom Validator API
//=============================================================================

/**
 * @brief Add custom validator to field
 *
 * WHAT: Attach custom validation function to a field
 * WHY:  Enable application-specific validation logic
 * HOW:  Store validator callback, invoke during validation
 *
 * @param schema Schema handle
 * @param key Field name
 * @param validator Validator callback function
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(1)
 *
 * EXAMPLE:
 * ```c
 * config_schema_add_int(schema, "batch_size", true, 32, 1, 10000);
 * config_schema_add_validator(schema, "batch_size", validate_power_of_2);
 * ```
 */
NIMCP_EXPORT bool config_schema_add_validator(
    config_validation_schema_t schema,
    const char* key,
    config_validator_t validator
);

/**
 * @brief Remove custom validator from field
 *
 * @param schema Schema handle
 * @param key Field name
 * @param validator Validator to remove
 * @return true if removed, false if not found
 */
NIMCP_EXPORT bool config_schema_remove_validator(
    config_validation_schema_t schema,
    const char* key,
    config_validator_t validator
);

//=============================================================================
// Dependency API
//=============================================================================

/**
 * @brief Add field dependency
 *
 * WHAT: Declare that a field depends on another field
 * WHY:  Enable cross-field validation logic
 * HOW:  Store dependency relationship, check during validation
 *
 * @param schema Schema handle
 * @param key Field that has a dependency
 * @param depends_on Field that key depends on
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(n) where n = existing dependencies (circular check)
 *
 * EXAMPLE:
 * ```c
 * // patience depends on early_stopping
 * config_schema_add_int(schema, "patience", false, 10, 1, 1000);
 * config_schema_add_bool(schema, "early_stopping", false, true);
 * config_schema_add_dependency(schema, "patience", "early_stopping");
 * ```
 */
NIMCP_EXPORT bool config_schema_add_dependency(
    config_validation_schema_t schema,
    const char* key,
    const char* depends_on
);

/**
 * @brief Add dependency with constraint validator
 *
 * WHAT: Add dependency with custom constraint logic
 * WHY:  Enable complex inter-field validation
 * HOW:  Store dependency + validator, invoke during validation
 *
 * @param schema Schema handle
 * @param key Field that has a dependency
 * @param depends_on Field that key depends on
 * @param constraint Constraint validator callback
 * @return true on success, false on failure
 *
 * EXAMPLE:
 * ```c
 * // If enable_bcm=true, bcm_tau must be > 0
 * config_schema_add_dependency_with_constraint(
 *     schema, "bcm_tau", "enable_bcm", validate_bcm_dependency
 * );
 * ```
 */
NIMCP_EXPORT bool config_schema_add_dependency_with_constraint(
    config_validation_schema_t schema,
    const char* key,
    const char* depends_on,
    config_dependency_validator_t constraint
);

/**
 * @brief Remove field dependency
 *
 * @param schema Schema handle
 * @param key Field name
 * @param depends_on Dependency to remove
 * @return true if removed, false if not found
 */
NIMCP_EXPORT bool config_schema_remove_dependency(
    config_validation_schema_t schema,
    const char* key,
    const char* depends_on
);

/**
 * @brief Check for circular dependencies
 *
 * WHAT: Detect circular dependency chains
 * WHY:  Prevent infinite loops during validation
 * HOW:  Depth-first search from each field
 *
 * @param schema Schema handle
 * @return true if circular dependency exists, false otherwise
 */
NIMCP_EXPORT bool config_schema_has_circular_dependencies(
    config_validation_schema_t schema
);

//=============================================================================
// Validation API
//=============================================================================

/**
 * @brief Validate config against schema
 *
 * WHAT: Comprehensive validation of config values against schema
 * WHY:  Ensure config integrity before use
 * HOW:  Check required fields, types, ranges, validators, dependencies
 *
 * @param schema Schema to validate against
 * @param result Output validation result (may be NULL)
 * @return true if valid, false if invalid (check result for errors)
 *
 * COMPLEXITY: O(n*m) where n=fields, m=validators per field
 * THREAD SAFETY: Thread-safe (read lock on schema)
 *
 * VALIDATION STEPS:
 * 1. Check all required fields are present
 * 2. Apply defaults for missing optional fields
 * 3. Check field types match schema
 * 4. Validate ranges for numeric/string types
 * 5. Run custom validators
 * 6. Check dependencies are satisfied
 *
 * EXAMPLE:
 * ```c
 * config_validation_result_t result;
 * if (!config_validate_against_schema(schema, &result)) {
 *     for (uint32_t i = 0; i < result.error_count; i++) {
 *         fprintf(stderr, "Error: %s\n", result.errors[i]);
 *     }
 *     return false;
 * }
 * ```
 */
NIMCP_EXPORT bool config_validate_against_schema(
    config_validation_schema_t schema,
    config_validation_result_t* result
);

/**
 * @brief Validate single value against range
 *
 * WHAT: Standalone range validation for a value
 * WHY:  Quick validation without full schema
 * HOW:  Check value against min/max bounds
 *
 * @param key Field name (for error messages)
 * @param value Value to validate
 * @param type Expected type
 * @param min Minimum value (int64_t or double depending on type)
 * @param max Maximum value (int64_t or double depending on type)
 * @param error Output buffer for error message
 * @param error_size Size of error buffer
 * @return true if valid, false if out of range
 *
 * COMPLEXITY: O(1)
 *
 * EXAMPLE:
 * ```c
 * config_value_t val = {.int_val = 64};
 * char error[256];
 * if (!config_validate_value_range("batch_size", &val, CONFIG_TYPE_INT,
 *                                   1, 10000, error, sizeof(error))) {
 *     fprintf(stderr, "%s\n", error);
 * }
 * ```
 */
NIMCP_EXPORT bool config_validate_value_range(
    const char* key,
    const config_value_t* value,
    config_value_type_t type,
    int64_t min,
    int64_t max,
    char* error,
    size_t error_size
);

/**
 * @brief Validate string length
 *
 * @param key Field name (for error messages)
 * @param value String value
 * @param max_len Maximum allowed length
 * @param error Output buffer for error message
 * @param error_size Size of error buffer
 * @return true if valid, false if too long
 */
NIMCP_EXPORT bool config_validate_string_length(
    const char* key,
    const char* value,
    size_t max_len,
    char* error,
    size_t error_size
);

/**
 * @brief Apply defaults to config for missing optional fields
 *
 * WHAT: Populate config with default values from schema
 * WHY:  Ensure all fields have valid values
 * HOW:  Iterate schema, set defaults for missing fields
 *
 * @param schema Schema with defaults
 * @return true on success, false on failure
 *
 * NOTE: Only applies to optional fields. Required fields cause validation error.
 */
NIMCP_EXPORT bool config_apply_defaults(config_validation_schema_t schema);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Check if field exists in schema
 *
 * @param schema Schema handle
 * @param key Field name
 * @return true if field exists, false otherwise
 */
NIMCP_EXPORT bool config_schema_has_field(
    config_validation_schema_t schema,
    const char* key
);

/**
 * @brief Get field type from schema
 *
 * @param schema Schema handle
 * @param key Field name
 * @param type Output field type
 * @return true if field found, false otherwise
 */
NIMCP_EXPORT bool config_schema_get_field_type(
    config_validation_schema_t schema,
    const char* key,
    config_value_type_t* type
);

/**
 * @brief Check if field is required
 *
 * @param schema Schema handle
 * @param key Field name
 * @return true if required, false if optional or not found
 */
NIMCP_EXPORT bool config_schema_is_field_required(
    config_validation_schema_t schema,
    const char* key
);

/**
 * @brief Get schema statistics
 *
 * @param schema Schema handle
 * @param stats Output statistics structure
 * @return true on success, false on failure
 */
NIMCP_EXPORT bool config_schema_get_stats(
    config_validation_schema_t schema,
    config_schema_stats_t* stats
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get validation result error count
 *
 * @param result Validation result
 * @return Number of errors
 */
static inline uint32_t config_validation_get_error_count(
    const config_validation_result_t* result
) {
    return result ? result->error_count : 0;
}

/**
 * @brief Get validation result error message
 *
 * @param result Validation result
 * @param index Error index [0, error_count)
 * @return Error message or NULL if invalid index
 */
static inline const char* config_validation_get_error(
    const config_validation_result_t* result,
    uint32_t index
) {
    if (!result || index >= result->error_count) {
        return NULL;
    }
    return result->errors[index];
}

/**
 * @brief Print validation errors to stderr
 *
 * @param result Validation result
 */
NIMCP_EXPORT void config_validation_print_errors(
    const config_validation_result_t* result
);

/**
 * @brief Clear validation result
 *
 * @param result Validation result to clear
 */
static inline void config_validation_clear_result(
    config_validation_result_t* result
) {
    if (result) {
        result->valid = true;
        result->error_count = 0;
    }
}

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_CONFIG_VALIDATION_H
