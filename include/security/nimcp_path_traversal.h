/**
 * @file nimcp_path_traversal.h
 * @brief Path Traversal Detection - BBB Input Gate Enhanced Pattern Detection
 *
 * WHAT: Comprehensive path traversal attack pattern detection for file paths and URLs
 * WHY:  Prevent unauthorized file system access via directory traversal attacks
 * HOW:  Multi-layer detection using pattern matching, normalization, and context analysis
 *
 * BIOLOGICAL MODEL:
 * ```
 * Selective Transport Channel (Endothelial selectivity) -> Path validator
 *   - Validates molecular structure before transport
 *   - Rejects malformed or suspicious molecular patterns
 * ```
 *
 * DETECTION PATTERNS:
 * - Basic traversal: ../, ..\\, ..;
 * - URL encoded: %2e%2e%2f, %2e%2e/, ..%2f, %2e%2e%5c
 * - Double URL encoded: %252e%252e%252f
 * - Unicode/UTF-8: %c0%ae%c0%ae%c0%af, ..%c0%af
 * - Null byte injection: ../../../etc/passwd%00.jpg
 * - Absolute paths: /etc/passwd, C:\\Windows\\, file://
 * - Path normalization bypass: ....//, ..../,  ..\\.\\
 * - Windows-specific: ..\\..\\, ....\\\\
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe operations
 * - Opaque pointer types
 * - nimcp_ prefix for all public symbols
 *
 * @author NIMCP Team
 * @date 2025-12-07
 */

#ifndef NIMCP_PATH_TRAVERSAL_H
#define NIMCP_PATH_TRAVERSAL_H

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

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct nimcp_path_validator_struct* nimcp_path_validator_t;

//=============================================================================
// Constants
//=============================================================================

/** Magic number for path validator */
#define NIMCP_PATH_VALIDATOR_MAGIC 0x50415448  /* "PATH" */

/** Maximum path length for validation */
#define NIMCP_MAX_PATH_LENGTH 4096

/** Maximum normalized path length */
#define NIMCP_MAX_NORMALIZED_PATH 8192

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Path validation context types
 */
typedef enum {
    NIMCP_PATH_CONTEXT_FILE = 0,      /**< File system path */
    NIMCP_PATH_CONTEXT_URL,           /**< URL path component */
    NIMCP_PATH_CONTEXT_AUTO           /**< Auto-detect context */
} nimcp_path_context_t;

/**
 * @brief Path traversal pattern types
 */
typedef enum {
    NIMCP_PATH_PATTERN_NONE = 0,      /**< No pattern detected */
    NIMCP_PATH_PATTERN_BASIC,         /**< Basic ../ or ..\\ */
    NIMCP_PATH_PATTERN_URL_ENCODED,   /**< URL encoded traversal */
    NIMCP_PATH_PATTERN_DOUBLE_ENCODED,/**< Double URL encoded */
    NIMCP_PATH_PATTERN_UNICODE,       /**< Unicode/UTF-8 encoded */
    NIMCP_PATH_PATTERN_NULL_BYTE,     /**< Null byte injection */
    NIMCP_PATH_PATTERN_ABSOLUTE,      /**< Absolute path */
    NIMCP_PATH_PATTERN_NORMALIZED,    /**< Normalization bypass */
    NIMCP_PATH_PATTERN_WINDOWS        /**< Windows-specific */
} nimcp_path_pattern_t;

/**
 * @brief Path validation severity levels
 */
typedef enum {
    NIMCP_PATH_SEVERITY_NONE = 0,     /**< No threat */
    NIMCP_PATH_SEVERITY_LOW = 1,      /**< Low severity */
    NIMCP_PATH_SEVERITY_MEDIUM = 2,   /**< Medium severity */
    NIMCP_PATH_SEVERITY_HIGH = 3,     /**< High severity */
    NIMCP_PATH_SEVERITY_CRITICAL = 4  /**< Critical severity */
} nimcp_path_severity_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Path validator configuration
 */
typedef struct {
    bool enable_basic_detection;      /**< Detect basic ../ patterns */
    bool enable_url_encoding;         /**< Detect URL encoded patterns */
    bool enable_unicode;              /**< Detect Unicode bypass */
    bool enable_null_byte;            /**< Detect null byte injection */
    bool enable_absolute_path;        /**< Detect absolute paths */
    bool enable_normalization;        /**< Enable path normalization */
    bool strict_mode;                 /**< Strict validation mode */
    size_t max_path_length;           /**< Maximum allowed path length */
    nimcp_path_context_t default_context; /**< Default validation context */
} nimcp_path_validator_config_t;

//=============================================================================
// Validation Result
//=============================================================================

/**
 * @brief Path validation result
 */
typedef struct {
    bool valid;                       /**< Whether path is valid */
    nimcp_path_pattern_t pattern;     /**< Detected pattern (if invalid) */
    nimcp_path_severity_t severity;   /**< Severity level */
    char reason[256];                 /**< Reason for rejection */
    size_t pattern_offset;            /**< Offset where pattern found */
    uint32_t pattern_count;           /**< Number of patterns detected */
    bool contains_absolute;           /**< Contains absolute path */
    bool contains_null_byte;          /**< Contains null byte */
    int traversal_depth;              /**< Directory traversal depth */
} nimcp_path_validation_result_t;

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Path validator statistics
 */
typedef struct {
    uint64_t total_validations;       /**< Total validations performed */
    uint64_t threats_detected;        /**< Total threats detected */
    uint64_t basic_patterns;          /**< Basic traversal patterns */
    uint64_t url_encoded_patterns;    /**< URL encoded patterns */
    uint64_t double_encoded_patterns; /**< Double encoded patterns */
    uint64_t unicode_patterns;        /**< Unicode patterns */
    uint64_t null_byte_patterns;      /**< Null byte patterns */
    uint64_t absolute_paths;          /**< Absolute path detections */
    uint64_t normalization_bypasses;  /**< Normalization bypasses */
    uint64_t windows_patterns;        /**< Windows-specific patterns */
} nimcp_path_validator_stats_t;

//=============================================================================
// Error Codes
//=============================================================================

/**
 * @brief Path validator error codes
 */
typedef enum {
    NIMCP_PATH_SUCCESS = 0,           /**< Success */
    NIMCP_PATH_ERROR_INVALID_PARAM,   /**< Invalid parameter */
    NIMCP_PATH_ERROR_NULL_POINTER,    /**< NULL pointer */
    NIMCP_PATH_ERROR_BUFFER_TOO_SMALL,/**< Buffer too small */
    NIMCP_PATH_ERROR_ALLOCATION,      /**< Memory allocation failed */
    NIMCP_PATH_ERROR_INVALID_MAGIC,   /**< Invalid magic number */
    NIMCP_PATH_ERROR_THREAT_DETECTED  /**< Threat detected */
} nimcp_path_error_t;

//=============================================================================
// Public API
//=============================================================================

/**
 * @brief Get default path validator configuration
 *
 * WHAT: Returns default configuration with all detections enabled
 * WHY:  Provide safe defaults for path validation
 * HOW:  Pre-configured struct with conservative settings
 *
 * @return Default configuration
 */
NIMCP_EXPORT nimcp_path_validator_config_t nimcp_path_validator_default_config(void);

/**
 * @brief Create path validator
 *
 * WHAT: Allocate and initialize path validator instance
 * WHY:  Enable path traversal detection with custom configuration
 * HOW:  Allocate structure, initialize pattern matchers, set magic
 *
 * @param config Configuration (NULL for defaults)
 * @return Path validator handle, or NULL on failure
 */
NIMCP_EXPORT nimcp_path_validator_t nimcp_path_validator_create(
    const nimcp_path_validator_config_t* config);

/**
 * @brief Destroy path validator
 *
 * WHAT: Free path validator and all associated resources
 * WHY:  Prevent memory leaks, clean up pattern matchers
 * HOW:  Validate magic, free internal structures, zero memory
 *
 * @param validator Path validator handle
 */
NIMCP_EXPORT void nimcp_path_validator_destroy(nimcp_path_validator_t validator);

/**
 * @brief Validate path for traversal attacks
 *
 * WHAT: Comprehensive path validation against all attack patterns
 * WHY:  Detect and block path traversal attempts before file access
 * HOW:  Multi-pass detection: basic, encoded, unicode, null byte, absolute
 *
 * @param validator Path validator handle
 * @param path Path to validate
 * @param context Validation context (file, URL, auto)
 * @param result Output validation result
 * @return Error code (NIMCP_PATH_SUCCESS if no threat)
 */
NIMCP_EXPORT nimcp_path_error_t nimcp_path_validate(
    nimcp_path_validator_t validator,
    const char* path,
    nimcp_path_context_t context,
    nimcp_path_validation_result_t* result);

/**
 * @brief Normalize path (resolve .., ., remove double slashes)
 *
 * WHAT: Convert path to canonical form for safe comparison
 * WHY:  Eliminate normalization bypass attacks
 * HOW:  Parse path components, resolve references, rebuild path
 *
 * @param path Input path
 * @param normalized Output buffer for normalized path
 * @param max_len Maximum length of output buffer
 * @return Error code (NIMCP_PATH_SUCCESS on success)
 */
NIMCP_EXPORT nimcp_path_error_t nimcp_path_normalize(
    const char* path,
    char* normalized,
    size_t max_len);

/**
 * @brief URL decode path component
 *
 * WHAT: Decode URL-encoded characters in path
 * WHY:  Reveal hidden traversal patterns
 * HOW:  Parse %XX sequences, convert to characters
 *
 * @param encoded Input URL-encoded path
 * @param decoded Output buffer for decoded path
 * @param max_len Maximum length of output buffer
 * @return Error code (NIMCP_PATH_SUCCESS on success)
 */
NIMCP_EXPORT nimcp_path_error_t nimcp_path_url_decode(
    const char* encoded,
    char* decoded,
    size_t max_len);

/**
 * @brief Get path validator statistics
 *
 * WHAT: Retrieve current statistics from validator
 * WHY:  Monitor detection effectiveness, track threats
 * HOW:  Copy internal statistics to output struct
 *
 * @param validator Path validator handle
 * @param stats Output statistics structure
 * @return Error code (NIMCP_PATH_SUCCESS on success)
 */
NIMCP_EXPORT nimcp_path_error_t nimcp_path_validator_get_stats(
    nimcp_path_validator_t validator,
    nimcp_path_validator_stats_t* stats);

/**
 * @brief Reset path validator statistics
 *
 * WHAT: Clear all statistics counters
 * WHY:  Enable fresh monitoring period
 * HOW:  Zero all counters in statistics structure
 *
 * @param validator Path validator handle
 * @return Error code (NIMCP_PATH_SUCCESS on success)
 */
NIMCP_EXPORT nimcp_path_error_t nimcp_path_validator_reset_stats(
    nimcp_path_validator_t validator);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get pattern type name
 *
 * WHAT: Convert pattern enum to human-readable string
 * WHY:  Enable logging and reporting
 * HOW:  Lookup table for pattern names
 *
 * @param pattern Pattern type
 * @return Human-readable name
 */
NIMCP_EXPORT const char* nimcp_path_pattern_name(nimcp_path_pattern_t pattern);

/**
 * @brief Get severity name
 *
 * WHAT: Convert severity enum to human-readable string
 * WHY:  Enable logging and reporting
 * HOW:  Lookup table for severity names
 *
 * @param severity Severity level
 * @return Human-readable name
 */
NIMCP_EXPORT const char* nimcp_path_severity_name(nimcp_path_severity_t severity);

/**
 * @brief Get error name
 *
 * WHAT: Convert error code to human-readable string
 * WHY:  Enable error reporting and debugging
 * HOW:  Lookup table for error names
 *
 * @param error Error code
 * @return Human-readable name
 */
NIMCP_EXPORT const char* nimcp_path_error_name(nimcp_path_error_t error);

/**
 * @brief Print validation result to stdout
 *
 * WHAT: Display validation result in human-readable format
 * WHY:  Debugging and monitoring
 * HOW:  Format and print all result fields
 *
 * @param result Validation result to print
 */
NIMCP_EXPORT void nimcp_path_print_result(const nimcp_path_validation_result_t* result);

/**
 * @brief Print statistics to stdout
 *
 * WHAT: Display validator statistics in human-readable format
 * WHY:  Monitoring and analysis
 * HOW:  Format and print all statistics fields
 *
 * @param stats Statistics to print
 */
NIMCP_EXPORT void nimcp_path_print_stats(const nimcp_path_validator_stats_t* stats);

//=============================================================================
// Convenience Functions
//=============================================================================

/**
 * @brief Quick path validation without managing validator instance
 *
 * WHAT: Validate a path for traversal attacks using a thread-local validator
 * WHY:  Simplify common use case of one-off path validation before file access
 * HOW:  Uses a lazily-initialized thread-local validator with default config
 *
 * @param path Path to validate
 * @return true if path is safe, false if threat detected
 */
NIMCP_EXPORT bool nimcp_path_is_safe(const char* path);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PATH_TRAVERSAL_H */
