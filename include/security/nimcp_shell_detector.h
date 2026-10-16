/**
 * @file nimcp_shell_detector.h
 * @brief Shell Command Injection Detection - BBB Input Gate Enhanced Pattern Detection
 *
 * WHAT: Comprehensive shell command injection pattern detection for user input
 * WHY:  Prevent command injection attacks that execute unauthorized shell commands
 * HOW:  Multi-layer detection using pattern matching, context analysis, and encoding detection
 *
 * BIOLOGICAL MODEL:
 * ```
 * Molecular Recognition (Receptor selectivity) -> Command detector
 *   - Identifies dangerous molecular structures
 *   - Rejects toxic compounds before cellular entry
 * ```
 *
 * DETECTION PATTERNS:
 * - Command separators: ;, &&, ||, |, &
 * - Command substitution: $(...), backticks, ${...}
 * - Dangerous commands: rm, cat, wget, curl, chmod, chown, dd, mkfs
 * - Shells: /bin/sh, /bin/bash, cmd.exe, powershell
 * - Redirection: >, >>, <, 2>&1
 * - Newline injection: \n, \r, %0a, %0d
 * - Environment manipulation: export, env, PATH=
 * - Common attack payloads: ; rm -rf /, && cat /etc/passwd
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

#ifndef NIMCP_SHELL_DETECTOR_H
#define NIMCP_SHELL_DETECTOR_H

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

typedef struct nimcp_shell_detector_struct* nimcp_shell_detector_t;

//=============================================================================
// Constants
//=============================================================================

/** Magic number for shell detector */
#define NIMCP_SHELL_DETECTOR_MAGIC 0x5348454C  /* "SHEL" */

/** Maximum input length for validation */
#define NIMCP_MAX_SHELL_INPUT_LENGTH 8192

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Shell context types (OS-specific)
 */
typedef enum {
    NIMCP_SHELL_CONTEXT_UNIX = 0,     /**< Unix/Linux shell context */
    NIMCP_SHELL_CONTEXT_WINDOWS,      /**< Windows cmd/PowerShell */
    NIMCP_SHELL_CONTEXT_AUTO          /**< Auto-detect based on patterns */
} nimcp_shell_context_t;

/**
 * @brief Shell injection pattern types
 */
typedef enum {
    NIMCP_SHELL_PATTERN_NONE = 0,     /**< No pattern detected */
    NIMCP_SHELL_PATTERN_SEPARATOR,    /**< Command separator (;, &&, ||) */
    NIMCP_SHELL_PATTERN_SUBSTITUTION, /**< Command substitution $() `` */
    NIMCP_SHELL_PATTERN_DANGEROUS_CMD,/**< Dangerous command (rm, wget) */
    NIMCP_SHELL_PATTERN_SHELL_INVOKE, /**< Shell invocation */
    NIMCP_SHELL_PATTERN_REDIRECTION,  /**< I/O redirection (>, <, |) */
    NIMCP_SHELL_PATTERN_NEWLINE,      /**< Newline injection */
    NIMCP_SHELL_PATTERN_ENVIRONMENT,  /**< Environment manipulation */
    NIMCP_SHELL_PATTERN_ENCODED       /**< Encoded injection attempt */
} nimcp_shell_pattern_t;

/**
 * @brief Shell injection severity levels
 */
typedef enum {
    NIMCP_SHELL_SEVERITY_NONE = 0,    /**< No threat */
    NIMCP_SHELL_SEVERITY_LOW = 1,     /**< Low severity */
    NIMCP_SHELL_SEVERITY_MEDIUM = 2,  /**< Medium severity */
    NIMCP_SHELL_SEVERITY_HIGH = 3,    /**< High severity */
    NIMCP_SHELL_SEVERITY_CRITICAL = 4 /**< Critical severity */
} nimcp_shell_severity_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Shell detector configuration
 */
typedef struct {
    bool enable_separator_detection;  /**< Detect command separators */
    bool enable_substitution_detection;/**< Detect command substitution */
    bool enable_dangerous_cmd_detection;/**< Detect dangerous commands */
    bool enable_shell_detection;      /**< Detect shell invocations */
    bool enable_redirection_detection;/**< Detect I/O redirection */
    bool enable_newline_detection;    /**< Detect newline injection */
    bool enable_environment_detection;/**< Detect env manipulation */
    bool enable_encoding_detection;   /**< Detect encoded patterns */
    bool strict_mode;                 /**< Strict validation mode */
    size_t max_input_length;          /**< Maximum allowed input length */
    nimcp_shell_context_t default_context; /**< Default shell context */
} nimcp_shell_detector_config_t;

//=============================================================================
// Detection Result
//=============================================================================

/**
 * @brief Shell injection detection result
 */
typedef struct {
    bool valid;                       /**< Whether input is valid */
    nimcp_shell_pattern_t pattern;    /**< Detected pattern (if invalid) */
    nimcp_shell_severity_t severity;  /**< Severity level */
    char reason[256];                 /**< Reason for rejection */
    size_t pattern_offset;            /**< Offset where pattern found */
    uint32_t pattern_count;           /**< Number of patterns detected */
    bool contains_separator;          /**< Contains command separator */
    bool contains_substitution;       /**< Contains command substitution */
    bool contains_redirection;        /**< Contains I/O redirection */
    bool contains_dangerous_cmd;      /**< Contains dangerous command */
    char detected_command[128];       /**< Detected dangerous command */
} nimcp_shell_detection_result_t;

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Shell detector statistics
 */
typedef struct {
    uint64_t total_detections;        /**< Total detections performed */
    uint64_t threats_detected;        /**< Total threats detected */
    uint64_t separator_patterns;      /**< Command separator patterns */
    uint64_t substitution_patterns;   /**< Command substitution patterns */
    uint64_t dangerous_cmd_patterns;  /**< Dangerous command patterns */
    uint64_t shell_invoke_patterns;   /**< Shell invocation patterns */
    uint64_t redirection_patterns;    /**< I/O redirection patterns */
    uint64_t newline_patterns;        /**< Newline injection patterns */
    uint64_t environment_patterns;    /**< Environment manipulation */
    uint64_t encoded_patterns;        /**< Encoded injection patterns */
} nimcp_shell_detector_stats_t;

//=============================================================================
// Error Codes
//=============================================================================

/**
 * @brief Shell detector error codes
 */
typedef enum {
    NIMCP_SHELL_SUCCESS = 0,          /**< Success */
    NIMCP_SHELL_ERROR_INVALID_PARAM,  /**< Invalid parameter */
    NIMCP_SHELL_ERROR_NULL_POINTER,   /**< NULL pointer */
    NIMCP_SHELL_ERROR_BUFFER_TOO_SMALL,/**< Buffer too small */
    NIMCP_SHELL_ERROR_ALLOCATION,     /**< Memory allocation failed */
    NIMCP_SHELL_ERROR_INVALID_MAGIC,  /**< Invalid magic number */
    NIMCP_SHELL_ERROR_THREAT_DETECTED /**< Threat detected */
} nimcp_shell_error_t;

//=============================================================================
// Public API
//=============================================================================

/**
 * @brief Get default shell detector configuration
 *
 * WHAT: Returns default configuration with all detections enabled
 * WHY:  Provide safe defaults for shell injection detection
 * HOW:  Pre-configured struct with conservative settings
 *
 * @return Default configuration
 */
NIMCP_EXPORT nimcp_shell_detector_config_t nimcp_shell_detector_default_config(void);

/**
 * @brief Create shell detector
 *
 * WHAT: Allocate and initialize shell detector instance
 * WHY:  Enable shell injection detection with custom configuration
 * HOW:  Allocate structure, initialize pattern matchers, set magic
 *
 * @param config Configuration (NULL for defaults)
 * @return Shell detector handle, or NULL on failure
 */
NIMCP_EXPORT nimcp_shell_detector_t nimcp_shell_detector_create(
    const nimcp_shell_detector_config_t* config);

/**
 * @brief Destroy shell detector
 *
 * WHAT: Free shell detector and all associated resources
 * WHY:  Prevent memory leaks, clean up pattern matchers
 * HOW:  Validate magic, free internal structures, zero memory
 *
 * @param detector Shell detector handle
 */
NIMCP_EXPORT void nimcp_shell_detector_destroy(nimcp_shell_detector_t detector);

/**
 * @brief Detect shell command injection in input
 *
 * WHAT: Comprehensive shell injection detection against all patterns
 * WHY:  Detect and block shell injection attempts before execution
 * HOW:  Multi-pass: separators -> substitution -> dangerous -> encoding
 *
 * @param detector Shell detector handle
 * @param input Input string to analyze
 * @param context Shell context (Unix, Windows, auto)
 * @param result Output detection result
 * @return Error code (NIMCP_SHELL_SUCCESS if no threat)
 */
NIMCP_EXPORT nimcp_shell_error_t nimcp_shell_detect(
    nimcp_shell_detector_t detector,
    const char* input,
    nimcp_shell_context_t context,
    nimcp_shell_detection_result_t* result);

/**
 * @brief Sanitize input by removing shell metacharacters
 *
 * WHAT: Remove or escape shell-special characters
 * WHY:  Allow safe processing of user input
 * HOW:  Whitelist approach - keep only safe characters
 *
 * @param detector Shell detector handle
 * @param input Input string
 * @param output Output buffer for sanitized string
 * @param max_len Maximum length of output buffer
 * @return Error code (NIMCP_SHELL_SUCCESS on success)
 */
NIMCP_EXPORT nimcp_shell_error_t nimcp_shell_sanitize(
    nimcp_shell_detector_t detector,
    const char* input,
    char* output,
    size_t max_len);

/**
 * @brief Get shell detector statistics
 *
 * WHAT: Retrieve current statistics from detector
 * WHY:  Monitor detection effectiveness, track threats
 * HOW:  Copy internal statistics to output struct
 *
 * @param detector Shell detector handle
 * @param stats Output statistics structure
 * @return Error code (NIMCP_SHELL_SUCCESS on success)
 */
NIMCP_EXPORT nimcp_shell_error_t nimcp_shell_detector_get_stats(
    nimcp_shell_detector_t detector,
    nimcp_shell_detector_stats_t* stats);

/**
 * @brief Reset shell detector statistics
 *
 * WHAT: Clear all statistics counters
 * WHY:  Enable fresh monitoring period
 * HOW:  Zero all counters in statistics structure
 *
 * @param detector Shell detector handle
 * @return Error code (NIMCP_SHELL_SUCCESS on success)
 */
NIMCP_EXPORT nimcp_shell_error_t nimcp_shell_detector_reset_stats(
    nimcp_shell_detector_t detector);

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
NIMCP_EXPORT const char* nimcp_shell_pattern_name(nimcp_shell_pattern_t pattern);

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
NIMCP_EXPORT const char* nimcp_shell_severity_name(nimcp_shell_severity_t severity);

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
NIMCP_EXPORT const char* nimcp_shell_error_name(nimcp_shell_error_t error);

/**
 * @brief Print detection result to stdout
 *
 * WHAT: Display detection result in human-readable format
 * WHY:  Debugging and monitoring
 * HOW:  Format and print all result fields
 *
 * @param result Detection result to print
 */
NIMCP_EXPORT void nimcp_shell_print_result(const nimcp_shell_detection_result_t* result);

/**
 * @brief Print statistics to stdout
 *
 * WHAT: Display detector statistics in human-readable format
 * WHY:  Monitoring and analysis
 * HOW:  Format and print all statistics fields
 *
 * @param stats Statistics to print
 */
NIMCP_EXPORT void nimcp_shell_print_stats(const nimcp_shell_detector_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SHELL_DETECTOR_H */
