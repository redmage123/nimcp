/**
 * @file nimcp_shell_detector.c
 * @brief Shell Command Injection Detection Implementation
 *
 * WHAT: Comprehensive shell command injection pattern detection implementation
 * WHY:  Prevent command injection attacks that execute unauthorized commands
 * HOW:  Pattern matching, context analysis, encoding detection, bio-async integration
 *
 * IMPLEMENTATION STRATEGY:
 * 1. Multi-pass detection: separators -> substitution -> dangerous -> encoding
 * 2. OS-specific pattern matching (Unix vs Windows)
 * 3. Context-aware severity scoring
 * 4. Statistics tracking for monitoring
 * 5. Bio-async message integration for threat alerts
 *
 * @author NIMCP Team
 * @date 2025-12-07
 */

#include "security/nimcp_shell_detector.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "security_shell_detector"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(shell_detector, MESH_ADAPTER_CATEGORY_SECURITY)


#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include "constants/nimcp_buffer_constants.h"

//=============================================================================
// Internal Structure
//=============================================================================

/**
 * WHAT: Internal shell detector structure
 * WHY:  Encapsulate state, configuration, and statistics
 * HOW:  Opaque pointer implementation with magic number
 */
struct nimcp_shell_detector_struct {
    uint32_t magic;                   /**< Magic number for validation */
    nimcp_shell_detector_config_t cfg;/**< Configuration */
    nimcp_shell_detector_stats_t stats;/**< Statistics */
};

//=============================================================================
// Pattern Detection Tables
//=============================================================================

/**
 * WHAT: Command separator patterns
 * WHY:  Used to chain multiple commands together
 * HOW:  Check for these characters/sequences
 */
static const char* separator_patterns[] = {
    ";",
    "&&",
    "||",
    "|",
    "&",
    "\n",
    "\r",
    NULL
};

/**
 * WHAT: Command substitution patterns
 * WHY:  Execute nested commands and capture output
 * HOW:  Detect various substitution syntaxes
 */
static const char* substitution_patterns[] = {
    "$(",
    "${",
    "`",
    "$((",  /* Arithmetic expansion */
    NULL
};

/**
 * WHAT: Dangerous Unix commands
 * WHY:  Commands that can cause significant damage or data exfiltration
 * HOW:  Case-insensitive substring matching
 */
static const char* dangerous_unix_commands[] = {
    "rm ",
    "dd ",
    "mkfs",
    "wget ",
    "curl ",
    "nc ",
    "netcat",
    "chmod ",
    "chown ",
    "cat /etc/passwd",
    "cat /etc/shadow",
    "/bin/sh",
    "/bin/bash",
    "/bin/dash",
    "/bin/ksh",
    "/bin/zsh",
    "sudo ",
    "su ",
    "exec ",
    "eval ",
    "sh -c",
    "bash -c",
    "perl -e",
    "python -c",
    "ruby -e",
    "awk ",
    "sed ",
    "find ",
    "kill ",
    "killall",
    "pkill",
    "reboot",
    "shutdown",
    "init ",
    "telinit",
    NULL
};

/**
 * WHAT: Dangerous Windows commands
 * WHY:  Windows-specific dangerous commands
 * HOW:  Case-insensitive matching
 */
static const char* dangerous_windows_commands[] = {
    "cmd.exe",
    "cmd /c",
    "powershell",
    "pwsh",
    "del ",
    "erase ",
    "format ",
    "rd ",
    "rmdir ",
    "cacls",
    "icacls",
    "net user",
    "net share",
    "reg add",
    "reg delete",
    "schtasks",
    "wmic",
    "sc create",
    "sc delete",
    "taskkill",
    "shutdown",
    "certutil",
    "bitsadmin",
    "regsvr32",
    "rundll32",
    "mshta",
    "wscript",
    "cscript",
    NULL
};

/**
 * WHAT: I/O redirection operators
 * WHY:  Used to redirect input/output, can leak or modify data
 * HOW:  Check for redirection characters
 */
static const char* redirection_patterns[] = {
    ">",
    ">>",
    "<",
    "2>",
    "2>&1",
    "&>",
    "<<<",
    NULL
};

/**
 * WHAT: Newline injection patterns
 * WHY:  Inject newlines to break out of quotes or add commands
 * HOW:  Literal and encoded newline sequences
 */
static const char* newline_patterns[] = {
    "%0a",
    "%0d",
    "%0A",
    "%0D",
    "\\n",
    "\\r",
    "\n",
    "\r",
    NULL
};

/**
 * WHAT: Environment manipulation patterns
 * WHY:  Modify environment to change program behavior
 * HOW:  Check for variable assignment and export
 */
static const char* environment_patterns[] = {
    "export ",
    "set ",
    "env ",
    "PATH=",
    "LD_LIBRARY_PATH=",
    "LD_PRELOAD=",
    "IFS=",
    "HOME=",
    "SHELL=",
    NULL
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Case-insensitive substring search
 */
static const char* case_insensitive_strstr(const char* text, const char* pattern)
{
    if (!text || !pattern) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "case_insensitive_strstr: required parameter is NULL (text, pattern)");
        return NULL;
    }
    if (!*pattern) return text;

    size_t pattern_len = strlen(pattern);
    size_t text_len = strlen(text);

    if (pattern_len > text_len) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "case_insensitive_strstr: validation failed");
        return NULL;
    }

    for (size_t i = 0; i <= text_len - pattern_len; i++) {
        size_t j;
        for (j = 0; j < pattern_len; j++) {
            if (tolower((unsigned char)text[i + j]) !=
                tolower((unsigned char)pattern[j])) {
                break;
            }
        }
        if (j == pattern_len) {
            return &text[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "case_insensitive_strstr: validation failed");
    return NULL;
}

/**
 * @brief Check for pattern array match
 */
static bool check_patterns(const char* input, const char* patterns[],
                          size_t* offset, char* detected_cmd,
                          size_t detected_cmd_size)
{
    if (!input || !patterns) {
        return false;
    }

    for (int i = 0; patterns[i] != NULL; i++) {
        const char* found = case_insensitive_strstr(input, patterns[i]);
        if (found) {
            if (offset) {
                *offset = (size_t)(found - input);
            }
            if (detected_cmd && detected_cmd_size > 0) {
                strncpy(detected_cmd, patterns[i], detected_cmd_size - 1);
                detected_cmd[detected_cmd_size - 1] = '\0';
            }
            return true;
        }
    }
    return false;
}

/**
 * @brief Count pattern occurrences
 */
static uint32_t count_pattern_occurrences(const char* input,
                                         const char* patterns[])
{
    if (!input || !patterns) return 0;

    uint32_t count = 0;
    for (int i = 0; patterns[i] != NULL; i++) {
        const char* p = input;
        while ((p = case_insensitive_strstr(p, patterns[i])) != NULL) {
            count++;
            p += strlen(patterns[i]);
        }
    }
    return count;
}

/**
 * @brief Check for exact pattern match (not substring)
 */
static bool check_exact_patterns(const char* input, const char* patterns[])
{
    if (!input || !patterns) {
        return false;
    }

    for (int i = 0; patterns[i] != NULL; i++) {
        if (strchr(input, patterns[i][0]) != NULL) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Send threat notification via logging
 */
static void send_threat_notification(nimcp_shell_detector_t detector,
                                     const char* input,
                                     nimcp_shell_pattern_t pattern,
                                     nimcp_shell_severity_t severity,
                                     const char* detected_cmd)
{
    if (!detector || !input) return;

    /* Log threat notification */
    if (detected_cmd && detected_cmd[0]) {
        LOG_WARN(LOG_MODULE, "Shell injection threat: pattern=%s severity=%s cmd='%s' input='%.150s'",
                 nimcp_shell_pattern_name(pattern),
                 nimcp_shell_severity_name(severity),
                 detected_cmd,
                 input);
    } else {
        LOG_WARN(LOG_MODULE, "Shell injection threat: pattern=%s severity=%s input='%.200s'",
                 nimcp_shell_pattern_name(pattern),
                 nimcp_shell_severity_name(severity),
                 input);
    }
}

//=============================================================================
// Public API Implementation
//=============================================================================

nimcp_shell_detector_config_t nimcp_shell_detector_default_config(void)
{
    nimcp_shell_detector_config_t config = {
        .enable_separator_detection = true,
        .enable_substitution_detection = true,
        .enable_dangerous_cmd_detection = true,
        .enable_shell_detection = true,
        .enable_redirection_detection = true,
        .enable_newline_detection = true,
        .enable_environment_detection = true,
        .enable_encoding_detection = true,
        .strict_mode = true,
        .max_input_length = NIMCP_MAX_SHELL_INPUT_LENGTH,
        .default_context = NIMCP_SHELL_CONTEXT_AUTO
    };
    return config;
}

nimcp_shell_detector_t nimcp_shell_detector_create(
    const nimcp_shell_detector_config_t* config)
{
    /* Allocate detector structure */
    nimcp_shell_detector_t detector = (nimcp_shell_detector_t)
        nimcp_calloc(1, sizeof(struct nimcp_shell_detector_struct));

    if (!detector) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate shell detector");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate shell detector");
        return NULL;
    }

    /* Initialize */
    detector->magic = NIMCP_SHELL_DETECTOR_MAGIC;
    detector->cfg = config ? *config : nimcp_shell_detector_default_config();
    memset(&detector->stats, 0, sizeof(detector->stats));

    LOG_INFO(LOG_MODULE, "Shell detector created (strict=%d)",
             detector->cfg.strict_mode);

    return detector;
}

void nimcp_shell_detector_destroy(nimcp_shell_detector_t detector)
{
    /* Guard: NULL detector */
    if (!detector) return;

    /* Guard: Invalid magic */
    if (detector->magic != NIMCP_SHELL_DETECTOR_MAGIC) {
        LOG_ERROR(LOG_MODULE, "Invalid detector magic: 0x%08x", detector->magic);
        return;
    }

    /* Bio-async unregistration would go here if registered */

    LOG_INFO(LOG_MODULE, "Shell detector destroyed (detections=%lu threats=%lu)",
             (unsigned long)detector->stats.total_detections,
             (unsigned long)detector->stats.threats_detected);

    /* Clear magic and free */
    detector->magic = 0;
    nimcp_free(detector);
}

nimcp_shell_error_t nimcp_shell_detect(
    nimcp_shell_detector_t detector,
    const char* input,
    nimcp_shell_context_t context,
    nimcp_shell_detection_result_t* result)
{
    /* Guard: NULL parameters */
    if (!result) return NIMCP_SHELL_ERROR_INVALID_PARAM;

    /* Initialize result */
    memset(result, 0, sizeof(*result));
    result->valid = true;
    result->pattern = NIMCP_SHELL_PATTERN_NONE;
    result->severity = NIMCP_SHELL_SEVERITY_NONE;

    /* Guard: NULL detector */
    if (!detector) {
        snprintf(result->reason, sizeof(result->reason), "NULL detector");
        return NIMCP_SHELL_ERROR_NULL_POINTER;
    }

    /* Guard: Invalid magic */
    if (detector->magic != NIMCP_SHELL_DETECTOR_MAGIC) {
        snprintf(result->reason, sizeof(result->reason), "Invalid detector magic");
        return NIMCP_SHELL_ERROR_INVALID_MAGIC;
    }

    /* Guard: NULL input */
    if (!input) {
        result->valid = false;
        result->severity = NIMCP_SHELL_SEVERITY_HIGH;
        snprintf(result->reason, sizeof(result->reason), "NULL input pointer");
        return NIMCP_SHELL_ERROR_NULL_POINTER;
    }

    /* Update statistics */
    detector->stats.total_detections++;

    /* Guard: Empty input is valid */
    if (*input == '\0') {
        return NIMCP_SHELL_SUCCESS;
    }

    /* Length check */
    size_t input_len = strlen(input);
    if (input_len > detector->cfg.max_input_length) {
        result->valid = false;
        result->pattern = NIMCP_SHELL_PATTERN_SEPARATOR;
        result->severity = NIMCP_SHELL_SEVERITY_MEDIUM;
        snprintf(result->reason, sizeof(result->reason),
                 "Input exceeds maximum length (%zu > %zu)",
                 input_len, detector->cfg.max_input_length);
        detector->stats.threats_detected++;
        return NIMCP_SHELL_ERROR_THREAT_DETECTED;
    }

    /* Auto-detect context if requested */
    if (context == NIMCP_SHELL_CONTEXT_AUTO) {
        /* Simple heuristic: check for Windows-specific patterns */
        if (case_insensitive_strstr(input, "cmd") ||
            case_insensitive_strstr(input, "powershell") ||
            case_insensitive_strstr(input, ".exe")) {
            context = NIMCP_SHELL_CONTEXT_WINDOWS;
        } else {
            context = NIMCP_SHELL_CONTEXT_UNIX;
        }
    }

    size_t offset = 0;
    char detected_cmd[NIMCP_LABEL_BUFFER_SIZE] = {0};

    /* Check for command separators */
    if (detector->cfg.enable_separator_detection &&
        check_patterns(input, separator_patterns, &offset, detected_cmd,
                      sizeof(detected_cmd))) {
        result->valid = false;
        result->pattern = NIMCP_SHELL_PATTERN_SEPARATOR;
        result->severity = NIMCP_SHELL_SEVERITY_HIGH;
        result->pattern_offset = offset;
        result->contains_separator = true;
        result->pattern_count = count_pattern_occurrences(input, separator_patterns);
        snprintf(result->reason, sizeof(result->reason),
                 "Command separator detected at offset %zu", offset);
        detector->stats.threats_detected++;
        detector->stats.separator_patterns++;
        send_threat_notification(detector, input, result->pattern,
                                result->severity, detected_cmd);
        LOG_WARN(LOG_MODULE, "Command separator: %s", input);
        return NIMCP_SHELL_ERROR_THREAT_DETECTED;
    }

    /* Check for command substitution */
    if (detector->cfg.enable_substitution_detection &&
        check_patterns(input, substitution_patterns, &offset, detected_cmd,
                      sizeof(detected_cmd))) {
        result->valid = false;
        result->pattern = NIMCP_SHELL_PATTERN_SUBSTITUTION;
        result->severity = NIMCP_SHELL_SEVERITY_CRITICAL;
        result->pattern_offset = offset;
        result->contains_substitution = true;
        snprintf(result->reason, sizeof(result->reason),
                 "Command substitution detected at offset %zu", offset);
        detector->stats.threats_detected++;
        detector->stats.substitution_patterns++;
        send_threat_notification(detector, input, result->pattern,
                                result->severity, detected_cmd);
        LOG_WARN(LOG_MODULE, "Command substitution: %s", input);
        return NIMCP_SHELL_ERROR_THREAT_DETECTED;
    }

    /* Check for dangerous commands (context-specific) */
    if (detector->cfg.enable_dangerous_cmd_detection) {
        const char** dangerous_cmds = (context == NIMCP_SHELL_CONTEXT_WINDOWS) ?
                                      dangerous_windows_commands :
                                      dangerous_unix_commands;

        if (check_patterns(input, dangerous_cmds, &offset, detected_cmd,
                          sizeof(detected_cmd))) {
            result->valid = false;
            result->pattern = NIMCP_SHELL_PATTERN_DANGEROUS_CMD;
            result->severity = NIMCP_SHELL_SEVERITY_CRITICAL;
            result->pattern_offset = offset;
            result->contains_dangerous_cmd = true;
            strncpy(result->detected_command, detected_cmd,
                   sizeof(result->detected_command) - 1);
            snprintf(result->reason, sizeof(result->reason),
                     "Dangerous command detected: '%s'", detected_cmd);
            detector->stats.threats_detected++;
            detector->stats.dangerous_cmd_patterns++;
            send_threat_notification(detector, input, result->pattern,
                                    result->severity, detected_cmd);
            LOG_WARN(LOG_MODULE, "Dangerous command '%s': %s",
                     detected_cmd, input);
            return NIMCP_SHELL_ERROR_THREAT_DETECTED;
        }
    }

    /* Check for I/O redirection */
    if (detector->cfg.enable_redirection_detection &&
        check_patterns(input, redirection_patterns, &offset, detected_cmd,
                      sizeof(detected_cmd))) {
        result->valid = false;
        result->pattern = NIMCP_SHELL_PATTERN_REDIRECTION;
        result->severity = NIMCP_SHELL_SEVERITY_HIGH;
        result->pattern_offset = offset;
        result->contains_redirection = true;
        snprintf(result->reason, sizeof(result->reason),
                 "I/O redirection detected at offset %zu", offset);
        detector->stats.threats_detected++;
        detector->stats.redirection_patterns++;
        send_threat_notification(detector, input, result->pattern,
                                result->severity, detected_cmd);
        LOG_WARN(LOG_MODULE, "I/O redirection: %s", input);
        return NIMCP_SHELL_ERROR_THREAT_DETECTED;
    }

    /* Check for newline injection */
    if (detector->cfg.enable_newline_detection &&
        check_patterns(input, newline_patterns, &offset, detected_cmd,
                      sizeof(detected_cmd))) {
        result->valid = false;
        result->pattern = NIMCP_SHELL_PATTERN_NEWLINE;
        result->severity = NIMCP_SHELL_SEVERITY_HIGH;
        result->pattern_offset = offset;
        snprintf(result->reason, sizeof(result->reason),
                 "Newline injection detected at offset %zu", offset);
        detector->stats.threats_detected++;
        detector->stats.newline_patterns++;
        send_threat_notification(detector, input, result->pattern,
                                result->severity, detected_cmd);
        LOG_WARN(LOG_MODULE, "Newline injection: %s", input);
        return NIMCP_SHELL_ERROR_THREAT_DETECTED;
    }

    /* Check for environment manipulation */
    if (detector->cfg.enable_environment_detection &&
        check_patterns(input, environment_patterns, &offset, detected_cmd,
                      sizeof(detected_cmd))) {
        result->valid = false;
        result->pattern = NIMCP_SHELL_PATTERN_ENVIRONMENT;
        result->severity = NIMCP_SHELL_SEVERITY_MEDIUM;
        result->pattern_offset = offset;
        snprintf(result->reason, sizeof(result->reason),
                 "Environment manipulation detected at offset %zu", offset);
        detector->stats.threats_detected++;
        detector->stats.environment_patterns++;
        send_threat_notification(detector, input, result->pattern,
                                result->severity, detected_cmd);
        LOG_WARN(LOG_MODULE, "Environment manipulation: %s", input);
        return NIMCP_SHELL_ERROR_THREAT_DETECTED;
    }

    /* Input is valid */
    LOG_DEBUG(LOG_MODULE, "Input validated: %s", input);
    return NIMCP_SHELL_SUCCESS;
}

nimcp_shell_error_t nimcp_shell_sanitize(
    nimcp_shell_detector_t detector,
    const char* input,
    char* output,
    size_t max_len)
{
    /* Guard: Invalid parameters */
    if (!input || !output || max_len == 0) {
        return NIMCP_SHELL_ERROR_INVALID_PARAM;
    }

    /* Guard: NULL detector */
    if (!detector) {
        return NIMCP_SHELL_ERROR_NULL_POINTER;
    }

    /* Guard: Invalid magic */
    if (detector->magic != NIMCP_SHELL_DETECTOR_MAGIC) {
        return NIMCP_SHELL_ERROR_INVALID_MAGIC;
    }

    size_t out_idx = 0;
    size_t in_len = strlen(input);

    /* Whitelist approach: keep only safe characters */
    for (size_t i = 0; i < in_len && out_idx < max_len - 1; i++) {
        char c = input[i];

        /* Allow alphanumeric, spaces, and safe punctuation */
        if (isalnum((unsigned char)c)) {
            output[out_idx++] = c;
        } else if (c == ' ' || c == '\t') {
            output[out_idx++] = c;
        } else if (c == '.' || c == ',' || c == '-' || c == '_') {
            output[out_idx++] = c;
        } else if (c == '@' || c == '#') {
            output[out_idx++] = c;
        }
        /* Skip dangerous characters: ; & | $ ` < > \ / ' " % { } ( ) */
    }

    /* Null terminate */
    output[out_idx] = '\0';

    LOG_DEBUG(LOG_MODULE, "Sanitized: '%s' -> '%s'", input, output);
    return NIMCP_SHELL_SUCCESS;
}

nimcp_shell_error_t nimcp_shell_detector_get_stats(
    nimcp_shell_detector_t detector,
    nimcp_shell_detector_stats_t* stats)
{
    /* Guard: NULL parameters */
    if (!detector || !stats) {
        return NIMCP_SHELL_ERROR_INVALID_PARAM;
    }

    /* Guard: Invalid magic */
    if (detector->magic != NIMCP_SHELL_DETECTOR_MAGIC) {
        return NIMCP_SHELL_ERROR_INVALID_MAGIC;
    }

    /* Copy statistics */
    *stats = detector->stats;
    return NIMCP_SHELL_SUCCESS;
}

nimcp_shell_error_t nimcp_shell_detector_reset_stats(
    nimcp_shell_detector_t detector)
{
    /* Guard: NULL detector */
    if (!detector) {
        return NIMCP_SHELL_ERROR_INVALID_PARAM;
    }

    /* Guard: Invalid magic */
    if (detector->magic != NIMCP_SHELL_DETECTOR_MAGIC) {
        return NIMCP_SHELL_ERROR_INVALID_MAGIC;
    }

    /* Reset statistics */
    memset(&detector->stats, 0, sizeof(detector->stats));
    LOG_INFO(LOG_MODULE, "Statistics reset");
    return NIMCP_SHELL_SUCCESS;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* nimcp_shell_pattern_name(nimcp_shell_pattern_t pattern)
{
    switch (pattern) {
        case NIMCP_SHELL_PATTERN_NONE: return "NONE";
        case NIMCP_SHELL_PATTERN_SEPARATOR: return "SEPARATOR";
        case NIMCP_SHELL_PATTERN_SUBSTITUTION: return "SUBSTITUTION";
        case NIMCP_SHELL_PATTERN_DANGEROUS_CMD: return "DANGEROUS_CMD";
        case NIMCP_SHELL_PATTERN_SHELL_INVOKE: return "SHELL_INVOKE";
        case NIMCP_SHELL_PATTERN_REDIRECTION: return "REDIRECTION";
        case NIMCP_SHELL_PATTERN_NEWLINE: return "NEWLINE";
        case NIMCP_SHELL_PATTERN_ENVIRONMENT: return "ENVIRONMENT";
        case NIMCP_SHELL_PATTERN_ENCODED: return "ENCODED";
        default: return "UNKNOWN";
    }
}

const char* nimcp_shell_severity_name(nimcp_shell_severity_t severity)
{
    switch (severity) {
        case NIMCP_SHELL_SEVERITY_NONE: return "NONE";
        case NIMCP_SHELL_SEVERITY_LOW: return "LOW";
        case NIMCP_SHELL_SEVERITY_MEDIUM: return "MEDIUM";
        case NIMCP_SHELL_SEVERITY_HIGH: return "HIGH";
        case NIMCP_SHELL_SEVERITY_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

const char* nimcp_shell_error_name(nimcp_shell_error_t error)
{
    switch (error) {
        case NIMCP_SHELL_SUCCESS: return "SUCCESS";
        case NIMCP_SHELL_ERROR_INVALID_PARAM: return "INVALID_PARAM";
        case NIMCP_SHELL_ERROR_NULL_POINTER: return "NULL_POINTER";
        case NIMCP_SHELL_ERROR_BUFFER_TOO_SMALL: return "BUFFER_TOO_SMALL";
        case NIMCP_SHELL_ERROR_ALLOCATION: return "ALLOCATION";
        case NIMCP_SHELL_ERROR_INVALID_MAGIC: return "INVALID_MAGIC";
        case NIMCP_SHELL_ERROR_THREAT_DETECTED: return "THREAT_DETECTED";
        default: return "UNKNOWN";
    }
}

void nimcp_shell_print_result(const nimcp_shell_detection_result_t* result)
{
    if (!result) return;

    printf("Shell Detection Result:\n");
    printf("  Valid: %s\n", result->valid ? "YES" : "NO");
    printf("  Pattern: %s\n", nimcp_shell_pattern_name(result->pattern));
    printf("  Severity: %s\n", nimcp_shell_severity_name(result->severity));
    printf("  Reason: %s\n", result->reason);
    printf("  Pattern Offset: %zu\n", result->pattern_offset);
    printf("  Pattern Count: %u\n", result->pattern_count);
    printf("  Contains Separator: %s\n", result->contains_separator ? "YES" : "NO");
    printf("  Contains Substitution: %s\n", result->contains_substitution ? "YES" : "NO");
    printf("  Contains Redirection: %s\n", result->contains_redirection ? "YES" : "NO");
    printf("  Contains Dangerous Cmd: %s\n", result->contains_dangerous_cmd ? "YES" : "NO");
    if (result->detected_command[0]) {
        printf("  Detected Command: %s\n", result->detected_command);
    }
}

void nimcp_shell_print_stats(const nimcp_shell_detector_stats_t* stats)
{
    if (!stats) return;

    printf("Shell Detector Statistics:\n");
    printf("  Total Detections: %lu\n", (unsigned long)stats->total_detections);
    printf("  Threats Detected: %lu\n", (unsigned long)stats->threats_detected);
    printf("  Separator Patterns: %lu\n", (unsigned long)stats->separator_patterns);
    printf("  Substitution Patterns: %lu\n", (unsigned long)stats->substitution_patterns);
    printf("  Dangerous Cmd Patterns: %lu\n", (unsigned long)stats->dangerous_cmd_patterns);
    printf("  Shell Invoke Patterns: %lu\n", (unsigned long)stats->shell_invoke_patterns);
    printf("  Redirection Patterns: %lu\n", (unsigned long)stats->redirection_patterns);
    printf("  Newline Patterns: %lu\n", (unsigned long)stats->newline_patterns);
    printf("  Environment Patterns: %lu\n", (unsigned long)stats->environment_patterns);
    printf("  Encoded Patterns: %lu\n", (unsigned long)stats->encoded_patterns);
}
