/**
 * @file nimcp_symbolic_logic_lgss_loader.h
 * @brief LGSS Component A1: JSON loader for Safety Rules
 *
 * WHAT: Load safety rules from LGSS (Logical Guardrails Safety Schema) JSON format
 * WHY:  Enable declarative safety rule definition in human-readable format
 * HOW:  Parse JSON, validate schema, convert to safety_rule_t structures
 *
 * LGSS JSON FORMAT:
 * ```json
 * {
 *   "version": "1.0",
 *   "name": "NIMCP Safety Rules",
 *   "rules": [
 *     {
 *       "name": "deny_weapon_synthesis",
 *       "description": "Block requests to synthesize weapons",
 *       "domain": "WEAPONS",
 *       "severity": "CRITICAL",
 *       "action": "DENY",
 *       "priority": 1.0,
 *       "conditions": [
 *         {
 *           "field": "action_type",
 *           "operator": "CONTAINS",
 *           "value": "weapon"
 *         }
 *       ]
 *     }
 *   ]
 * }
 * ```
 *
 * INTEGRATION:
 * - Load rules at startup
 * - Validate against schema
 * - Add to safety KB
 * - Compile and lock KB
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#ifndef NIMCP_SYMBOLIC_LOGIC_LGSS_LOADER_H
#define NIMCP_SYMBOLIC_LOGIC_LGSS_LOADER_H

#include "nimcp_symbolic_logic_safety_types.h"
#include "nimcp_symbolic_logic_safety.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum LGSS JSON file size (1MB) */
#define LGSS_MAX_FILE_SIZE (1024 * 1024)

/** @brief Current LGSS schema version */
#define LGSS_SCHEMA_VERSION "1.0"

/** @brief Maximum length of LGSS file name */
#define LGSS_MAX_NAME_LEN 256

//=============================================================================
// Error Codes
//=============================================================================

/**
 * @brief LGSS loader error codes
 */
typedef enum {
    LGSS_OK = 0,                      /**< Success */
    LGSS_ERROR_NULL_ARG = 1,          /**< NULL argument provided */
    LGSS_ERROR_FILE_NOT_FOUND = 2,    /**< File not found */
    LGSS_ERROR_FILE_READ = 3,         /**< File read error */
    LGSS_ERROR_FILE_TOO_LARGE = 4,    /**< File exceeds max size */
    LGSS_ERROR_INVALID_JSON = 5,      /**< JSON parse error */
    LGSS_ERROR_SCHEMA_MISMATCH = 6,   /**< Schema validation failed */
    LGSS_ERROR_UNSUPPORTED_VERSION = 7, /**< Unsupported schema version */
    LGSS_ERROR_MISSING_FIELD = 8,     /**< Required field missing */
    LGSS_ERROR_INVALID_VALUE = 9,     /**< Invalid field value */
    LGSS_ERROR_MEMORY = 10,           /**< Memory allocation failed */
    LGSS_ERROR_KB_FULL = 11,          /**< Safety KB is full */
    LGSS_ERROR_KB_LOCKED = 12         /**< Safety KB is locked */
} lgss_error_t;

//=============================================================================
// Load Result Structure
//=============================================================================

/**
 * @brief Result of loading LGSS rules
 *
 * WHAT: Contains loading statistics and error information
 * WHY:  Provide detailed feedback on loading operation
 */
typedef struct {
    lgss_error_t error_code;              /**< Error code (LGSS_OK on success) */
    char error_message[256];              /**< Human-readable error message */
    int error_line;                       /**< Line number of error (if applicable) */
    int error_column;                     /**< Column number of error (if applicable) */

    uint32_t rules_loaded;                /**< Number of rules successfully loaded */
    uint32_t rules_failed;                /**< Number of rules that failed to load */
    uint32_t rules_skipped;               /**< Number of rules skipped (disabled) */

    char schema_version[16];              /**< Schema version from file */
    char file_name[LGSS_MAX_NAME_LEN];    /**< Name from LGSS file */
} lgss_load_result_t;

//=============================================================================
// File Loading API
//=============================================================================

/**
 * @brief Load safety rules from an LGSS JSON file
 *
 * WHAT: Parse JSON file and add rules to safety KB
 * WHY:  Enable declarative rule definition from files
 * HOW:  Read file, parse JSON, validate schema, convert to rules, add to KB
 *
 * @param filepath Path to LGSS JSON file (non-NULL)
 * @param kb Safety KB to add rules to (non-NULL, not locked)
 * @param result Output load result (can be NULL)
 * @return Number of rules successfully loaded, or -1 on fatal error
 *
 * COMPLEXITY: O(n * c) where n = rules, c = conditions
 * THREAD-SAFE: No
 * MALLOC: Yes (for JSON parsing)
 *
 * ERROR CONDITIONS:
 * - Returns -1 if filepath is NULL
 * - Returns -1 if kb is NULL
 * - Returns -1 if file not found
 * - Returns -1 if JSON parse error
 * - Returns -1 if schema validation fails
 * - Returns -1 if KB is locked
 *
 * NOTE: Partial success is possible - some rules may load even if others fail.
 *       Check result->rules_failed for failed rule count.
 *
 * EXAMPLE:
 * ```c
 * lgss_load_result_t result;
 * int loaded = symbolic_logic_lgss_load_file("safety_rules.json", kb, &result);
 * if (loaded < 0) {
 *     LOG_ERROR("Failed to load rules: %s", result.error_message);
 * } else {
 *     LOG_INFO("Loaded %d rules (%d failed)", loaded, result.rules_failed);
 * }
 * ```
 */
int symbolic_logic_lgss_load_file(
    const char* filepath,
    safety_kb_t* kb,
    lgss_load_result_t* result
);

/**
 * @brief W7: Brain-aware sibling of symbolic_logic_lgss_load_file.
 *
 * Same as symbolic_logic_lgss_load_file plus mirrors every loaded rule into
 * brain->internal_kg as 'cog_logic_rule_lgss_<id>' and emits a summary event.
 * Safe to call with brain == NULL (falls back to plain load).
 *
 * Forward-declared as void* to avoid pulling brain_internal.h into public api.
 */
struct brain_struct;
int symbolic_logic_lgss_load_file_kg(
    const char* filepath,
    safety_kb_t* kb,
    lgss_load_result_t* result,
    struct brain_struct* brain
);

/**
 * @brief Load safety rules from an LGSS JSON string
 *
 * WHAT: Parse JSON string and add rules to safety KB
 * WHY:  Enable programmatic rule loading without file I/O
 * HOW:  Parse JSON, validate schema, convert to rules, add to KB
 *
 * @param json_string JSON string containing LGSS rules (non-NULL)
 * @param json_length Length of JSON string (0 = strlen)
 * @param kb Safety KB to add rules to (non-NULL, not locked)
 * @param result Output load result (can be NULL)
 * @return Number of rules successfully loaded, or -1 on fatal error
 *
 * COMPLEXITY: O(n * c) where n = rules, c = conditions
 * THREAD-SAFE: No
 * MALLOC: Yes (for JSON parsing)
 *
 * ERROR CONDITIONS:
 * - Returns -1 if json_string is NULL
 * - Returns -1 if kb is NULL
 * - Returns -1 if JSON parse error
 * - Returns -1 if schema validation fails
 * - Returns -1 if KB is locked
 *
 * EXAMPLE:
 * ```c
 * const char* json = "{\"version\":\"1.0\",\"rules\":[...]}";
 * lgss_load_result_t result;
 * int loaded = symbolic_logic_lgss_load_string(json, 0, kb, &result);
 * ```
 */
int symbolic_logic_lgss_load_string(
    const char* json_string,
    size_t json_length,
    safety_kb_t* kb,
    lgss_load_result_t* result
);

//=============================================================================
// Rule Parsing API
//=============================================================================

/**
 * @brief Parse a single rule from JSON object string
 *
 * WHAT: Parse a single rule JSON object
 * WHY:  Enable incremental rule loading
 * HOW:  Parse JSON, validate, convert to safety_rule_t
 *
 * @param rule_json JSON string of a single rule object (non-NULL)
 * @param rule_out Output rule structure (non-NULL)
 * @param error_msg Output error message buffer (can be NULL)
 * @param error_msg_size Size of error message buffer
 * @return LGSS_OK on success, error code on failure
 *
 * COMPLEXITY: O(c) where c = conditions
 * THREAD-SAFE: Yes
 * MALLOC: Yes (for JSON parsing, freed before return)
 *
 * EXAMPLE:
 * ```c
 * const char* rule_json = "{\"name\":\"test\",\"domain\":\"CYBER\",...}";
 * safety_rule_t rule;
 * char error[256];
 * if (symbolic_logic_lgss_parse_rule(rule_json, &rule, error, sizeof(error)) == LGSS_OK) {
 *     symbolic_logic_safety_add_rule(kb, &rule);
 * }
 * ```
 */
lgss_error_t symbolic_logic_lgss_parse_rule(
    const char* rule_json,
    safety_rule_t* rule_out,
    char* error_msg,
    size_t error_msg_size
);

//=============================================================================
// Schema Validation API
//=============================================================================

/**
 * @brief Validate LGSS JSON against schema
 *
 * WHAT: Check that JSON conforms to LGSS schema
 * WHY:  Catch errors early before attempting to load rules
 * HOW:  Parse JSON, check required fields, validate types
 *
 * @param json_string JSON string to validate (non-NULL)
 * @param json_length Length of JSON string (0 = strlen)
 * @param error_msg Output error message buffer (can be NULL)
 * @param error_msg_size Size of error message buffer
 * @return LGSS_OK if valid, error code if invalid
 *
 * COMPLEXITY: O(n * c) where n = rules, c = conditions
 * THREAD-SAFE: Yes
 * MALLOC: Yes (for JSON parsing, freed before return)
 *
 * VALIDATES:
 * - Required fields present (version, rules)
 * - Version is supported
 * - Each rule has required fields (name, domain, severity, action)
 * - Domain values are valid
 * - Severity values are valid
 * - Action values are valid
 * - Condition operators are valid
 *
 * EXAMPLE:
 * ```c
 * char error[256];
 * if (symbolic_logic_lgss_validate_schema(json, 0, error, sizeof(error)) != LGSS_OK) {
 *     LOG_ERROR("Schema validation failed: %s", error);
 * }
 * ```
 */
lgss_error_t symbolic_logic_lgss_validate_schema(
    const char* json_string,
    size_t json_length,
    char* error_msg,
    size_t error_msg_size
);

//=============================================================================
// Version API
//=============================================================================

/**
 * @brief Get LGSS schema version from JSON
 *
 * WHAT: Extract version string from LGSS JSON
 * WHY:  Check version before attempting full parse
 * HOW:  Parse JSON, extract "version" field
 *
 * @param json_string JSON string (non-NULL)
 * @param json_length Length of JSON string (0 = strlen)
 * @param version_out Output version string buffer (non-NULL)
 * @param version_size Size of version buffer
 * @return true if version found, false if not found or error
 *
 * COMPLEXITY: O(1) (only parses version field)
 * THREAD-SAFE: Yes
 *
 * EXAMPLE:
 * ```c
 * char version[16];
 * if (symbolic_logic_lgss_get_version(json, 0, version, sizeof(version))) {
 *     if (strcmp(version, "1.0") != 0) {
 *         LOG_WARN("Unsupported LGSS version: %s", version);
 *     }
 * }
 * ```
 */
bool symbolic_logic_lgss_get_version(
    const char* json_string,
    size_t json_length,
    char* version_out,
    size_t version_size
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get human-readable error message for LGSS error code
 *
 * @param error_code LGSS error code
 * @return Error message string
 */
const char* symbolic_logic_lgss_error_string(lgss_error_t error_code);

/**
 * @brief Initialize load result to default values
 *
 * @param result Load result to initialize (non-NULL)
 */
void symbolic_logic_lgss_init_result(lgss_load_result_t* result);

/**
 * @brief Convert domain string to enum
 *
 * @param domain_str Domain string (e.g., "WEAPONS", "CYBER")
 * @param domain_out Output domain enum
 * @return true if valid, false if unrecognized
 */
bool symbolic_logic_lgss_parse_domain(
    const char* domain_str,
    safety_domain_t* domain_out
);

/**
 * @brief Convert severity string to enum
 *
 * @param severity_str Severity string (e.g., "CRITICAL", "HIGH")
 * @param severity_out Output severity enum
 * @return true if valid, false if unrecognized
 */
bool symbolic_logic_lgss_parse_severity(
    const char* severity_str,
    safety_severity_t* severity_out
);

/**
 * @brief Convert action string to enum
 *
 * @param action_str Action string (e.g., "DENY", "ALLOW")
 * @param action_out Output action enum
 * @return true if valid, false if unrecognized
 */
bool symbolic_logic_lgss_parse_action(
    const char* action_str,
    safety_action_t* action_out
);

/**
 * @brief Convert condition operator string to enum
 *
 * @param op_str Operator string (e.g., "EQ", "CONTAINS")
 * @param op_out Output operator enum
 * @return true if valid, false if unrecognized
 */
bool symbolic_logic_lgss_parse_operator(
    const char* op_str,
    safety_condition_op_t* op_out
);

/**
 * @brief Export safety KB to LGSS JSON format
 *
 * WHAT: Serialize safety KB rules to LGSS JSON
 * WHY:  Enable rule export and backup
 * HOW:  Convert rules to JSON, format output
 *
 * @param kb Safety KB to export (non-NULL)
 * @param output_buffer Output buffer for JSON (non-NULL)
 * @param buffer_size Size of output buffer
 * @return Number of bytes written, or -1 on error
 *
 * COMPLEXITY: O(n * c) where n = rules, c = conditions
 * THREAD-SAFE: Yes (read-only)
 *
 * EXAMPLE:
 * ```c
 * char buffer[65536];
 * int len = symbolic_logic_lgss_export(kb, buffer, sizeof(buffer));
 * if (len > 0) {
 *     FILE* f = fopen("exported_rules.json", "w");
 *     fwrite(buffer, 1, len, f);
 *     fclose(f);
 * }
 * ```
 */
int symbolic_logic_lgss_export(
    const safety_kb_t* kb,
    char* output_buffer,
    size_t buffer_size
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SYMBOLIC_LOGIC_LGSS_LOADER_H
