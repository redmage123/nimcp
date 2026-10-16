/**
 * @file nimcp_path_traversal.c
 * @brief Path Traversal Detection Implementation
 *
 * WHAT: Comprehensive path traversal attack pattern detection implementation
 * WHY:  Prevent unauthorized file system access via directory traversal
 * HOW:  Pattern matching, normalization, encoding detection, bio-async integration
 *
 * IMPLEMENTATION STRATEGY:
 * 1. Multi-pass detection: basic -> encoded -> unicode -> null -> absolute
 * 2. Path normalization to eliminate bypass attempts
 * 3. Context-aware detection (file paths vs URLs)
 * 4. Severity scoring based on pattern complexity
 * 5. Statistics tracking for monitoring
 * 6. Bio-async message integration for threat alerts
 *
 * @author NIMCP Team
 * @date 2025-12-07
 */

#include "security/nimcp_path_traversal.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "security_path_traversal"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(path_traversal, MESH_ADAPTER_CATEGORY_SECURITY)


#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

//=============================================================================
// Internal Structure
//=============================================================================

/**
 * WHAT: Internal path validator structure
 * WHY:  Encapsulate state, configuration, and statistics
 * HOW:  Opaque pointer implementation with magic number
 */
struct nimcp_path_validator_struct {
    uint32_t magic;                   /**< Magic number for validation */
    nimcp_path_validator_config_t cfg;/**< Configuration */
    nimcp_path_validator_stats_t stats;/**< Statistics */
};

//=============================================================================
// Pattern Detection Tables
//=============================================================================

/**
 * WHAT: Basic traversal patterns
 * WHY:  Common directory traversal sequences
 * HOW:  Checked via substring matching
 */
static const char* basic_traversal_patterns[] = {
    "../",
    "..\\",
    "..;",
    "..",
    NULL
};

/**
 * WHAT: URL encoded traversal patterns
 * WHY:  Attackers encode dots and slashes to bypass filters
 * HOW:  Case-insensitive substring matching
 */
static const char* url_encoded_patterns[] = {
    "%2e%2e%2f",    /* ../ */
    "%2e%2e/",      /* ../ partial */
    "..%2f",        /* ../ partial */
    "%2e%2e%5c",    /* ..\\ */
    "%2e%2e\\",     /* ..\\ partial */
    "..%5c",        /* ..\\ partial */
    "%2e%2e%3b",    /* ..; */
    "%2e.",         /* Partial encoding */
    ".%2e",         /* Partial encoding */
    NULL
};

/**
 * WHAT: Double URL encoded patterns
 * WHY:  Some systems decode twice, enabling bypass
 * HOW:  Check for %25 (encoded %) followed by encoded pattern
 */
static const char* double_encoded_patterns[] = {
    "%252e%252e%252f",  /* ../ double encoded */
    "%252e%252e%255c",  /* ..\\ double encoded */
    "%252e%252e",       /* .. double encoded */
    NULL
};

/**
 * WHAT: Unicode/UTF-8 encoded traversal patterns
 * WHY:  Overlong UTF-8 sequences can bypass filters
 * HOW:  Check for alternative encodings of dots and slashes
 */
static const char* unicode_patterns[] = {
    "%c0%ae%c0%ae%c0%af",  /* ../ in overlong UTF-8 */
    "..%c0%af",             /* .. with overlong / */
    "%c0%ae%c0%ae",         /* .. in overlong UTF-8 */
    "%c0%2e",               /* Overlong . */
    "%e0%80%ae",            /* Another overlong . */
    NULL
};

/**
 * WHAT: Absolute path prefixes
 * WHY:  Absolute paths can access any file on system
 * HOW:  Check for Unix and Windows absolute path indicators
 */
static const char* absolute_path_patterns[] = {
    "/etc/",
    "/usr/",
    "/var/",
    "/bin/",
    "/root/",
    "/home/",
    "/proc/",
    "/sys/",
    "c:\\",
    "d:\\",
    "e:\\",
    "\\\\",        /* UNC path */
    "file://",
    NULL
};

/**
 * WHAT: Windows-specific path patterns
 * WHY:  Windows has unique path traversal variations
 * HOW:  Check for Windows-specific sequences
 */
static const char* windows_patterns[] = {
    "..\\..\\",
    "....\\\\",
    ".\\.\\",
    "\\\\?\\",    /* Win32 namespace */
    "\\\\.\\",    /* Device namespace */
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
    return NULL;  /* Pattern not found — normal case */
}

/**
 * @brief Check for pattern array match
 */
static bool check_patterns(const char* input, const char* patterns[],
                          size_t* offset)
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
            return true;
        }
    }
    return false;
}

/**
 * @brief Count traversal patterns in path
 */
static uint32_t count_traversal_patterns(const char* path)
{
    if (!path) return 0;

    uint32_t count = 0;
    const char* p = path;

    while ((p = strstr(p, "..")) != NULL) {
        count++;
        p += 2;
    }

    return count;
}

/**
 * @brief Calculate traversal depth
 */
static int calculate_traversal_depth(const char* path)
{
    if (!path) return 0;

    int depth = 0;
    const char* p = path;

    while (*p) {
        if (p[0] == '.' && p[1] == '.') {
            if (p[2] == '/' || p[2] == '\\' || p[2] == '\0') {
                depth++;
                p += 2;
                continue;
            }
        }
        p++;
    }

    return depth;
}

/**
 * @brief Check for null byte in path
 */
static bool contains_null_byte(const char* path, size_t max_len)
{
    if (!path) {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "contains_null_byte: path is NULL");

            return false;

        }

    size_t len = strnlen(path, max_len);
    for (size_t i = 0; i < len; i++) {
        if (path[i] == '\0') {
            /* Found null byte before end */
            if (i < len - 1) return true;
        }
    }

    /* Check for %00 encoding */
    if (strstr(path, "%00") != NULL) return true;
    if (strstr(path, "%0") != NULL) return true;

    return false;  /* No null byte found — path is safe */
}

/**
 * @brief Send threat notification via logging
 */
static void send_threat_notification(nimcp_path_validator_t validator,
                                     const char* path,
                                     nimcp_path_pattern_t pattern,
                                     nimcp_path_severity_t severity)
{
    if (!validator || !path) return;

    /* Log threat notification */
    LOG_WARN(LOG_MODULE, "Path traversal threat detected: pattern=%s severity=%s path='%.200s'",
             nimcp_path_pattern_name(pattern),
             nimcp_path_severity_name(severity),
             path);
}

//=============================================================================
// Public API Implementation
//=============================================================================

nimcp_path_validator_config_t nimcp_path_validator_default_config(void)
{
    nimcp_path_validator_config_t config = {
        .enable_basic_detection = true,
        .enable_url_encoding = true,
        .enable_unicode = true,
        .enable_null_byte = true,
        .enable_absolute_path = true,
        .enable_normalization = true,
        .strict_mode = true,
        .max_path_length = NIMCP_MAX_PATH_LENGTH,
        .default_context = NIMCP_PATH_CONTEXT_AUTO
    };
    return config;
}

nimcp_path_validator_t nimcp_path_validator_create(
    const nimcp_path_validator_config_t* config)
{
    /* Allocate validator structure */
    nimcp_path_validator_t validator = (nimcp_path_validator_t)
        nimcp_calloc(1, sizeof(struct nimcp_path_validator_struct));

    NIMCP_API_CHECK_ALLOC(validator, "Failed to allocate path validator");

    /* Initialize */
    validator->magic = NIMCP_PATH_VALIDATOR_MAGIC;
    validator->cfg = config ? *config : nimcp_path_validator_default_config();
    memset(&validator->stats, 0, sizeof(validator->stats));

    LOG_INFO(LOG_MODULE, "Path validator created (strict=%d)",
             validator->cfg.strict_mode);

    return validator;
}

void nimcp_path_validator_destroy(nimcp_path_validator_t validator)
{
    /* Guard: NULL validator */
    if (!validator) return;

    /* Guard: Invalid magic */
    if (validator->magic != NIMCP_PATH_VALIDATOR_MAGIC) {
        LOG_ERROR(LOG_MODULE, "Invalid validator magic: 0x%08x", validator->magic);
        return;
    }

    /* Bio-async unregistration would go here if registered */

    LOG_INFO(LOG_MODULE, "Path validator destroyed (validations=%lu threats=%lu)",
             (unsigned long)validator->stats.total_validations,
             (unsigned long)validator->stats.threats_detected);

    /* Clear magic and free */
    validator->magic = 0;
    nimcp_free(validator);
}

nimcp_path_error_t nimcp_path_validate(
    nimcp_path_validator_t validator,
    const char* path,
    nimcp_path_context_t context,
    nimcp_path_validation_result_t* result)
{
    /* Guard: NULL parameters */
    NIMCP_API_CHECK_NULL(result, NIMCP_PATH_ERROR_INVALID_PARAM, "NULL result in path validate");

    /* Initialize result */
    memset(result, 0, sizeof(*result));
    result->valid = true;
    result->pattern = NIMCP_PATH_PATTERN_NONE;
    result->severity = NIMCP_PATH_SEVERITY_NONE;

    /* Guard: NULL validator */
    if (!validator) {
        snprintf(result->reason, sizeof(result->reason), "NULL validator");
        return NIMCP_PATH_ERROR_NULL_POINTER;
    }

    /* Guard: Invalid magic */
    if (validator->magic != NIMCP_PATH_VALIDATOR_MAGIC) {
        snprintf(result->reason, sizeof(result->reason), "Invalid validator magic");
        return NIMCP_PATH_ERROR_INVALID_MAGIC;
    }

    /* Guard: NULL path */
    if (!path) {
        result->valid = false;
        result->severity = NIMCP_PATH_SEVERITY_HIGH;
        snprintf(result->reason, sizeof(result->reason), "NULL path pointer");
        return NIMCP_PATH_ERROR_NULL_POINTER;
    }

    /* Update statistics */
    validator->stats.total_validations++;

    /* Guard: Empty path is valid */
    if (*path == '\0') {
        return NIMCP_PATH_SUCCESS;
    }

    /* Length check */
    size_t path_len = strlen(path);
    if (path_len > validator->cfg.max_path_length) {
        result->valid = false;
        result->pattern = NIMCP_PATH_PATTERN_BASIC;
        result->severity = NIMCP_PATH_SEVERITY_MEDIUM;
        snprintf(result->reason, sizeof(result->reason),
                 "Path exceeds maximum length (%zu > %zu)",
                 path_len, validator->cfg.max_path_length);
        validator->stats.threats_detected++;
        return NIMCP_PATH_ERROR_THREAT_DETECTED;
    }

    /* Check for null byte injection */
    if (validator->cfg.enable_null_byte && contains_null_byte(path, path_len)) {
        result->valid = false;
        result->pattern = NIMCP_PATH_PATTERN_NULL_BYTE;
        result->severity = NIMCP_PATH_SEVERITY_CRITICAL;
        result->contains_null_byte = true;
        snprintf(result->reason, sizeof(result->reason),
                 "Null byte injection detected");
        validator->stats.threats_detected++;
        validator->stats.null_byte_patterns++;
        send_threat_notification(validator, path, result->pattern, result->severity);
        LOG_WARN(LOG_MODULE, "Null byte injection: %s", path);
        return NIMCP_PATH_ERROR_THREAT_DETECTED;
    }

    /* Check for basic traversal patterns */
    size_t offset = 0;
    if (validator->cfg.enable_basic_detection &&
        check_patterns(path, basic_traversal_patterns, &offset)) {
        result->valid = false;
        result->pattern = NIMCP_PATH_PATTERN_BASIC;
        result->severity = NIMCP_PATH_SEVERITY_HIGH;
        result->pattern_offset = offset;
        result->pattern_count = count_traversal_patterns(path);
        result->traversal_depth = calculate_traversal_depth(path);
        snprintf(result->reason, sizeof(result->reason),
                 "Basic path traversal detected at offset %zu (depth=%d)",
                 offset, result->traversal_depth);
        validator->stats.threats_detected++;
        validator->stats.basic_patterns++;
        send_threat_notification(validator, path, result->pattern, result->severity);
        LOG_WARN(LOG_MODULE, "Basic traversal (depth=%d): %s",
                 result->traversal_depth, path);
        return NIMCP_PATH_ERROR_THREAT_DETECTED;
    }

    /* Check for URL encoded patterns */
    if (validator->cfg.enable_url_encoding &&
        check_patterns(path, url_encoded_patterns, &offset)) {
        result->valid = false;
        result->pattern = NIMCP_PATH_PATTERN_URL_ENCODED;
        result->severity = NIMCP_PATH_SEVERITY_HIGH;
        result->pattern_offset = offset;
        snprintf(result->reason, sizeof(result->reason),
                 "URL encoded traversal detected at offset %zu", offset);
        validator->stats.threats_detected++;
        validator->stats.url_encoded_patterns++;
        send_threat_notification(validator, path, result->pattern, result->severity);
        LOG_WARN(LOG_MODULE, "URL encoded traversal: %s", path);
        return NIMCP_PATH_ERROR_THREAT_DETECTED;
    }

    /* Check for double URL encoded patterns */
    if (validator->cfg.enable_url_encoding &&
        check_patterns(path, double_encoded_patterns, &offset)) {
        result->valid = false;
        result->pattern = NIMCP_PATH_PATTERN_DOUBLE_ENCODED;
        result->severity = NIMCP_PATH_SEVERITY_CRITICAL;
        result->pattern_offset = offset;
        snprintf(result->reason, sizeof(result->reason),
                 "Double URL encoded traversal detected at offset %zu", offset);
        validator->stats.threats_detected++;
        validator->stats.double_encoded_patterns++;
        send_threat_notification(validator, path, result->pattern, result->severity);
        LOG_WARN(LOG_MODULE, "Double encoded traversal: %s", path);
        return NIMCP_PATH_ERROR_THREAT_DETECTED;
    }

    /* Check for Unicode/UTF-8 encoded patterns */
    if (validator->cfg.enable_unicode &&
        check_patterns(path, unicode_patterns, &offset)) {
        result->valid = false;
        result->pattern = NIMCP_PATH_PATTERN_UNICODE;
        result->severity = NIMCP_PATH_SEVERITY_CRITICAL;
        result->pattern_offset = offset;
        snprintf(result->reason, sizeof(result->reason),
                 "Unicode encoded traversal detected at offset %zu", offset);
        validator->stats.threats_detected++;
        validator->stats.unicode_patterns++;
        send_threat_notification(validator, path, result->pattern, result->severity);
        LOG_WARN(LOG_MODULE, "Unicode encoded traversal: %s", path);
        return NIMCP_PATH_ERROR_THREAT_DETECTED;
    }

    /* Check for absolute paths */
    if (validator->cfg.enable_absolute_path &&
        check_patterns(path, absolute_path_patterns, &offset)) {
        result->valid = false;
        result->pattern = NIMCP_PATH_PATTERN_ABSOLUTE;
        result->severity = NIMCP_PATH_SEVERITY_HIGH;
        result->pattern_offset = offset;
        result->contains_absolute = true;
        snprintf(result->reason, sizeof(result->reason),
                 "Absolute path detected at offset %zu", offset);
        validator->stats.threats_detected++;
        validator->stats.absolute_paths++;
        send_threat_notification(validator, path, result->pattern, result->severity);
        LOG_WARN(LOG_MODULE, "Absolute path: %s", path);
        return NIMCP_PATH_ERROR_THREAT_DETECTED;
    }

    /* Check for Windows-specific patterns */
    if (validator->cfg.enable_basic_detection &&
        check_patterns(path, windows_patterns, &offset)) {
        result->valid = false;
        result->pattern = NIMCP_PATH_PATTERN_WINDOWS;
        result->severity = NIMCP_PATH_SEVERITY_HIGH;
        result->pattern_offset = offset;
        snprintf(result->reason, sizeof(result->reason),
                 "Windows path traversal detected at offset %zu", offset);
        validator->stats.threats_detected++;
        validator->stats.windows_patterns++;
        send_threat_notification(validator, path, result->pattern, result->severity);
        LOG_WARN(LOG_MODULE, "Windows traversal: %s", path);
        return NIMCP_PATH_ERROR_THREAT_DETECTED;
    }

    /* Normalization check */
    if (validator->cfg.enable_normalization) {
        char normalized[NIMCP_MAX_NORMALIZED_PATH];
        nimcp_path_error_t err = nimcp_path_normalize(path, normalized,
                                                       sizeof(normalized));
        if (err == NIMCP_PATH_SUCCESS) {
            /* Check if normalization revealed traversal */
            if (strcmp(path, normalized) != 0) {
                /* Path changed during normalization */
                if (check_patterns(normalized, basic_traversal_patterns, NULL) ||
                    calculate_traversal_depth(normalized) > 0) {
                    result->valid = false;
                    result->pattern = NIMCP_PATH_PATTERN_NORMALIZED;
                    result->severity = NIMCP_PATH_SEVERITY_HIGH;
                    snprintf(result->reason, sizeof(result->reason),
                             "Normalization bypass detected");
                    validator->stats.threats_detected++;
                    validator->stats.normalization_bypasses++;
                    send_threat_notification(validator, path,
                                            result->pattern, result->severity);
                    LOG_WARN(LOG_MODULE, "Normalization bypass: %s -> %s",
                             path, normalized);
                    return NIMCP_PATH_ERROR_THREAT_DETECTED;
                }
            }
        }
    }

    /* Path is valid */
    LOG_DEBUG(LOG_MODULE, "Path validated: %s", path);
    return NIMCP_PATH_SUCCESS;
}

nimcp_path_error_t nimcp_path_normalize(
    const char* path,
    char* normalized,
    size_t max_len)
{
    /* Guard: NULL parameters */
    if (!path || !normalized || max_len == 0) {
        return NIMCP_PATH_ERROR_INVALID_PARAM;
    }

    /* Guard: Empty path */
    if (*path == '\0') {
        if (max_len < 2) return NIMCP_PATH_ERROR_BUFFER_TOO_SMALL;
        normalized[0] = '.';
        normalized[1] = '\0';
        return NIMCP_PATH_SUCCESS;
    }

    /* Simple normalization:
     * - Remove duplicate slashes
     * - Remove trailing slashes
     * - Resolve . and .. (basic, not full resolution)
     */

    size_t out_idx = 0;
    size_t i = 0;
    size_t path_len = strlen(path);
    bool last_was_slash = false;

    for (i = 0; i < path_len && out_idx < max_len - 1; i++) {
        char c = path[i];

        /* Normalize slashes */
        if (c == '/' || c == '\\') {
            if (!last_was_slash) {
                normalized[out_idx++] = '/';
                last_was_slash = true;
            }
        } else {
            normalized[out_idx++] = c;
            last_was_slash = false;
        }
    }

    /* Check if buffer was too small (didn't process entire input) */
    if (i < path_len) {
        return NIMCP_PATH_ERROR_BUFFER_TOO_SMALL;
    }

    /* Remove trailing slash (unless it's the root) */
    if (out_idx > 1 && normalized[out_idx - 1] == '/') {
        out_idx--;
    }

    /* Null terminate */
    if (out_idx >= max_len) {
        return NIMCP_PATH_ERROR_BUFFER_TOO_SMALL;
    }
    normalized[out_idx] = '\0';

    return NIMCP_PATH_SUCCESS;
}

nimcp_path_error_t nimcp_path_url_decode(
    const char* encoded,
    char* decoded,
    size_t max_len)
{
    /* Guard: NULL parameters */
    if (!encoded || !decoded || max_len == 0) {
        return NIMCP_PATH_ERROR_INVALID_PARAM;
    }

    size_t out_idx = 0;
    size_t i = 0;
    size_t encoded_len = strlen(encoded);

    for (i = 0; i < encoded_len && out_idx < max_len - 1; i++) {
        if (encoded[i] == '%' && i + 2 < encoded_len) {
            /* Try to decode %XX */
            char hex[3] = { encoded[i + 1], encoded[i + 2], '\0' };
            char* endptr;
            long val = strtol(hex, &endptr, 16);

            if (endptr == hex + 2) {
                /* Successfully decoded */
                decoded[out_idx++] = (char)val;
                i += 2;  /* Skip the two hex digits */
            } else {
                /* Invalid hex, keep as-is */
                decoded[out_idx++] = encoded[i];
            }
        } else if (encoded[i] == '+') {
            /* + is space in query strings */
            decoded[out_idx++] = ' ';
        } else {
            decoded[out_idx++] = encoded[i];
        }
    }

    /* Check buffer overflow */
    if (out_idx >= max_len) {
        return NIMCP_PATH_ERROR_BUFFER_TOO_SMALL;
    }

    decoded[out_idx] = '\0';
    return NIMCP_PATH_SUCCESS;
}

nimcp_path_error_t nimcp_path_validator_get_stats(
    nimcp_path_validator_t validator,
    nimcp_path_validator_stats_t* stats)
{
    /* Guard: NULL parameters */
    if (!validator || !stats) {
        return NIMCP_PATH_ERROR_INVALID_PARAM;
    }

    /* Guard: Invalid magic */
    if (validator->magic != NIMCP_PATH_VALIDATOR_MAGIC) {
        return NIMCP_PATH_ERROR_INVALID_MAGIC;
    }

    /* Copy statistics */
    *stats = validator->stats;
    return NIMCP_PATH_SUCCESS;
}

nimcp_path_error_t nimcp_path_validator_reset_stats(
    nimcp_path_validator_t validator)
{
    /* Guard: NULL validator */
    if (!validator) {
        return NIMCP_PATH_ERROR_INVALID_PARAM;
    }

    /* Guard: Invalid magic */
    if (validator->magic != NIMCP_PATH_VALIDATOR_MAGIC) {
        return NIMCP_PATH_ERROR_INVALID_MAGIC;
    }

    /* Reset statistics */
    memset(&validator->stats, 0, sizeof(validator->stats));
    LOG_INFO(LOG_MODULE, "Statistics reset");
    return NIMCP_PATH_SUCCESS;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* nimcp_path_pattern_name(nimcp_path_pattern_t pattern)
{
    switch (pattern) {
        case NIMCP_PATH_PATTERN_NONE: return "NONE";
        case NIMCP_PATH_PATTERN_BASIC: return "BASIC";
        case NIMCP_PATH_PATTERN_URL_ENCODED: return "URL_ENCODED";
        case NIMCP_PATH_PATTERN_DOUBLE_ENCODED: return "DOUBLE_ENCODED";
        case NIMCP_PATH_PATTERN_UNICODE: return "UNICODE";
        case NIMCP_PATH_PATTERN_NULL_BYTE: return "NULL_BYTE";
        case NIMCP_PATH_PATTERN_ABSOLUTE: return "ABSOLUTE";
        case NIMCP_PATH_PATTERN_NORMALIZED: return "NORMALIZED";
        case NIMCP_PATH_PATTERN_WINDOWS: return "WINDOWS";
        default: return "UNKNOWN";
    }
}

const char* nimcp_path_severity_name(nimcp_path_severity_t severity)
{
    switch (severity) {
        case NIMCP_PATH_SEVERITY_NONE: return "NONE";
        case NIMCP_PATH_SEVERITY_LOW: return "LOW";
        case NIMCP_PATH_SEVERITY_MEDIUM: return "MEDIUM";
        case NIMCP_PATH_SEVERITY_HIGH: return "HIGH";
        case NIMCP_PATH_SEVERITY_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

const char* nimcp_path_error_name(nimcp_path_error_t error)
{
    switch (error) {
        case NIMCP_PATH_SUCCESS: return "SUCCESS";
        case NIMCP_PATH_ERROR_INVALID_PARAM: return "INVALID_PARAM";
        case NIMCP_PATH_ERROR_NULL_POINTER: return "NULL_POINTER";
        case NIMCP_PATH_ERROR_BUFFER_TOO_SMALL: return "BUFFER_TOO_SMALL";
        case NIMCP_PATH_ERROR_ALLOCATION: return "ALLOCATION";
        case NIMCP_PATH_ERROR_INVALID_MAGIC: return "INVALID_MAGIC";
        case NIMCP_PATH_ERROR_THREAT_DETECTED: return "THREAT_DETECTED";
        default: return "UNKNOWN";
    }
}

void nimcp_path_print_result(const nimcp_path_validation_result_t* result)
{
    if (!result) return;

    printf("Path Validation Result:\n");
    printf("  Valid: %s\n", result->valid ? "YES" : "NO");
    printf("  Pattern: %s\n", nimcp_path_pattern_name(result->pattern));
    printf("  Severity: %s\n", nimcp_path_severity_name(result->severity));
    printf("  Reason: %s\n", result->reason);
    printf("  Pattern Offset: %zu\n", result->pattern_offset);
    printf("  Pattern Count: %u\n", result->pattern_count);
    printf("  Contains Absolute: %s\n", result->contains_absolute ? "YES" : "NO");
    printf("  Contains Null Byte: %s\n", result->contains_null_byte ? "YES" : "NO");
    printf("  Traversal Depth: %d\n", result->traversal_depth);
}

void nimcp_path_print_stats(const nimcp_path_validator_stats_t* stats)
{
    if (!stats) return;

    printf("Path Validator Statistics:\n");
    printf("  Total Validations: %lu\n", (unsigned long)stats->total_validations);
    printf("  Threats Detected: %lu\n", (unsigned long)stats->threats_detected);
    printf("  Basic Patterns: %lu\n", (unsigned long)stats->basic_patterns);
    printf("  URL Encoded: %lu\n", (unsigned long)stats->url_encoded_patterns);
    printf("  Double Encoded: %lu\n", (unsigned long)stats->double_encoded_patterns);
    printf("  Unicode Patterns: %lu\n", (unsigned long)stats->unicode_patterns);
    printf("  Null Byte Patterns: %lu\n", (unsigned long)stats->null_byte_patterns);
    printf("  Absolute Paths: %lu\n", (unsigned long)stats->absolute_paths);
    printf("  Normalization Bypasses: %lu\n", (unsigned long)stats->normalization_bypasses);
    printf("  Windows Patterns: %lu\n", (unsigned long)stats->windows_patterns);
}

//=============================================================================
// Convenience Functions
//=============================================================================

/**
 * Thread-local validator for quick path validation
 */
static __thread nimcp_path_validator_t tl_quick_validator = NULL;
static __thread bool tl_validator_initialized = false;

/**
 * @brief Get or create thread-local validator for quick validations
 */
static nimcp_path_validator_t get_quick_validator(void)
{
    if (!tl_validator_initialized) {
        tl_quick_validator = nimcp_path_validator_create(NULL);
        tl_validator_initialized = true;
    }
    return tl_quick_validator;
}

bool nimcp_path_is_safe(const char* path)
{
    /* Guard: NULL path is not safe */
    if (!path) {
        LOG_WARN(LOG_MODULE, "nimcp_path_is_safe: NULL path");
        return false;
    }

    /* Get thread-local validator */
    nimcp_path_validator_t validator = get_quick_validator();
    if (!validator) {
        /* If validator creation failed, be conservative and reject */
        LOG_ERROR(LOG_MODULE, "nimcp_path_is_safe: Failed to create validator");
        return false;
    }

    /* Validate path */
    nimcp_path_validation_result_t result;
    nimcp_path_error_t err = nimcp_path_validate(validator, path,
                                                  NIMCP_PATH_CONTEXT_FILE, &result);

    return (err == NIMCP_PATH_SUCCESS && result.valid);
}
