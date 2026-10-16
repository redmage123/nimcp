/**
 * @file nimcp_bbb_input_gate.c
 * @brief Blood-Brain Barrier Input Gate - Input Validation Layer
 *
 * WHAT: Input validation and sanitization for the BBB perimeter defense
 * WHY:  Prevent code injection, buffer overflow, SQL injection, and XSS attacks
 *       before malicious data can reach the neural network core
 * HOW:  Multi-layer detection using pattern matching, bounds checking, and
 *       character analysis with sanitization for safe processing
 *
 * BIOLOGICAL MODEL:
 * ```
 * Endothelial cells (tight junctions) -> Input validation gates
 *   - Selective permeability based on molecular properties
 *   - Rejects harmful substances while allowing nutrients
 * ```
 *
 * DETECTION PATTERNS:
 * - SQL injection: SELECT, DROP, INSERT, --, ', UNION, etc.
 * - XSS attacks: <script>, javascript:, onerror=, etc.
 * - Format string: %n, %s without args, excessive format specifiers
 * - Buffer overflow: Strings exceeding max length
 * - Shellcode: 0x90 NOP sled, common opcodes, null bytes
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe operations
 *
 * @author NIMCP Team
 * @date 2025-11-24
 */

#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_bbb_enhanced_detection.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"

#define LOG_MODULE "security_bbb_input_gate"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(bbb_input_gate, MESH_ADAPTER_CATEGORY_SECURITY)


#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <sys/types.h>  /* For ssize_t */

//=============================================================================
// Internal Function Declarations (from core BBB module)
//=============================================================================

/* Statistics and config accessor functions from nimcp_blood_brain_barrier.c */
extern void bbb_system_inc_validations(bbb_system_t system);
extern void bbb_system_inc_threats(bbb_system_t system);
extern size_t bbb_system_get_max_string_length(bbb_system_t system);
extern int64_t bbb_system_get_min_integer(bbb_system_t system);
extern int64_t bbb_system_get_max_integer(bbb_system_t system);

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of format specifiers allowed in a string */
#define MAX_FORMAT_SPECIFIERS 10

/** Threshold for suspicious special character density */
#define SPECIAL_CHAR_THRESHOLD_RATIO 3

/** NOP sled detection threshold (consecutive NOPs) */
#define NOP_SLED_THRESHOLD 4

//=============================================================================
// SQL Injection Patterns
//=============================================================================

/**
 * WHAT: SQL injection detection patterns
 * WHY:  Common SQL keywords and operators used in injection attacks
 * HOW:  Case-insensitive substring matching during validation
 */
static const char* sql_patterns[] = {
    /* UNION injection (almost always malicious) */
    " union select", " union all select",
    "' union ", "\" union ",
    /* Boolean injection patterns */
    "' or ", "\" or ",
    " or 1=1", " or '1'='1",
    "'='", "\"=\"",
    /* Terminated/stacked injection (semicolon followed by command) */
    "'; drop ", "'; delete ",
    "'; insert ", "'; update ",
    "'; exec ", "'; execute ",
    "; drop ", "; delete ",          /* Stacked queries */
    "; insert ", "; update ",
    "; exec ", "; execute ",
    /* Comment indicators with quotes */
    "'--", "\"--",
    "/*", "*/",
    /* Dangerous stored procedures */
    "xp_cmdshell", "sp_execute",
    /* Schema probing */
    "information_schema",
    /* Tautology with comment */
    "1=1--", "1=1 --",
    NULL
};

//=============================================================================
// XSS Patterns
//=============================================================================

/**
 * WHAT: XSS attack detection patterns
 * WHY:  Common JavaScript injection vectors for cross-site scripting
 * HOW:  Case-insensitive pattern matching in input strings
 */
static const char* xss_patterns[] = {
    "<script", "</script",
    "javascript:", "vbscript:",
    "onerror=", "onload=", "onclick=", "onmouseover=",
    "onfocus=", "onblur=", "onchange=",
    "<iframe", "<object", "<embed",
    "<img src=", "<svg ",
    "expression(", "eval(",
    "document.cookie", "document.write",
    "window.location",
    NULL
};

//=============================================================================
// Format String Patterns
//=============================================================================

/**
 * WHAT: Dangerous format string specifiers
 * WHY:  %n writes to memory, %s without args crashes, etc.
 * HOW:  Pattern detection in user input that might be used in printf
 */
static const char* format_patterns[] = {
    "%n", "%hn", "%hhn", "%ln",
    "%s%s%s%s",      /* Stack smashing attempt */
    "%x%x%x%x",      /* Stack reading attempt */
    "%p%p%p%p",      /* Pointer leaking */
    "%08x.%08x",     /* Memory dump pattern with width specifier */
    /* Positional format specifiers (POSIX %N$X) — used by attackers to
     * pick stack slots out of order and to bypass naive %n filters. */
    "%1$", "%2$", "%3$", "%4$", "%5$",
    "%6$", "%7$", "%8$", "%9$",
    NULL
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Case-insensitive substring search
 *
 * WHAT: Find pattern in text regardless of case
 * WHY:  Attackers use mixed case to bypass simple detection
 * HOW:  Convert both strings to lowercase during comparison
 *
 * @param text Text to search in
 * @param pattern Pattern to find
 * @return Pointer to match or NULL
 */
static const char* case_insensitive_strstr(const char* text, const char* pattern)
{
    if (!text || !pattern) {
        return NULL;
    }
    if (!*pattern) return text;

    size_t pattern_len = strlen(pattern);
    size_t text_len = strlen(text);

    if (pattern_len > text_len) {
        return NULL;  /* Normal: pattern can't be in shorter text */
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
    return NULL;  /* Normal: no match found */
}

/**
 * @brief Check for pattern array match in input
 *
 * WHAT: Scan input against array of dangerous patterns
 * WHY:  Centralized pattern matching for multiple threat types
 * HOW:  Iterate patterns, return on first match
 *
 * @param input Input string to check
 * @param patterns NULL-terminated array of patterns
 * @return true if any pattern found
 */
static bool check_patterns(const char* input, const char* patterns[])
{
    if (!input || !patterns) {
        return false;
    }

    for (int i = 0; patterns[i] != NULL; i++) {
        if (case_insensitive_strstr(input, patterns[i])) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Length-aware case-insensitive substring search
 *
 * WHAT: memmem-style search that scans the FULL (data, size) buffer
 * WHY:  bbb_validate_input must not be fooled by an attacker-embedded NUL
 *       byte that truncates the string mid-payload. strlen-based scans
 *       miss anything after the first NUL.
 * HOW:  Linear scan using tolower() on each byte; treats input as bytes,
 *       not C-strings.
 *
 * @param data   Buffer to search (any bytes, may contain NULs)
 * @param size   Buffer length in bytes
 * @param pat    NUL-terminated pattern (treated as ASCII bytes)
 * @return true if pattern found
 */
static bool memmem_ci(const void* data, size_t size, const char* pat)
{
    if (!data || !pat || !*pat || size == 0) {
        return false;
    }
    const unsigned char* hay = (const unsigned char*)data;
    size_t plen = strlen(pat);
    if (plen > size) return false;

    for (size_t i = 0; i + plen <= size; i++) {
        size_t j;
        for (j = 0; j < plen; j++) {
            if (tolower(hay[i + j]) !=
                tolower((unsigned char)pat[j])) {
                break;
            }
        }
        if (j == plen) return true;
    }
    return false;
}

/**
 * @brief Length-aware version of check_patterns
 *
 * WHAT: Scan a fixed-size buffer for any pattern, ignoring embedded NULs
 * WHY:  Defense against NUL-truncation attacks against bbb_validate_input
 * HOW:  Iterate patterns, call memmem_ci on each
 */
static bool check_patterns_buf(const void* data, size_t size,
                               const char* patterns[])
{
    if (!data || !patterns || size == 0) {
        return false;
    }
    for (int i = 0; patterns[i] != NULL; i++) {
        if (memmem_ci(data, size, patterns[i])) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Detect shellcode patterns in binary data
 *
 * WHAT: Identify common shellcode signatures
 * WHY:  Prevent code execution attacks via buffer overflow
 * HOW:  Look for NOP sleds, common opcodes, suspicious byte patterns
 *
 * @param data Binary data to analyze
 * @param size Size of data
 * @return true if shellcode detected
 */
static bool detect_shellcode(const void* data, size_t size)
{
    if (!data || size == 0) {
        return false;
    }

    const uint8_t* bytes = (const uint8_t*)data;
    int nop_count = 0;
    int null_count = 0;
    int suspicious_opcodes = 0;

    for (size_t i = 0; i < size; i++) {
        /* NOP sled detection (0x90 on x86) */
        if (bytes[i] == 0x90) {
            nop_count++;
            if (nop_count >= NOP_SLED_THRESHOLD) {
                return true;
            }
        } else {
            nop_count = 0;
        }

        /* Null byte count (common in shellcode) */
        if (bytes[i] == 0x00) {
            null_count++;
        }

        /* Common shellcode opcodes */
        switch (bytes[i]) {
            case 0xCD:  /* int instruction */
            case 0x80:  /* syscall interrupt */
            case 0x0F:  /* syscall/sysenter prefix */
            case 0xCC:  /* int3 debug */
                suspicious_opcodes++;
                break;
        }
    }

    /* High ratio of suspicious bytes indicates shellcode */
    float suspicious_ratio = (float)suspicious_opcodes / size;
    if (suspicious_ratio > 0.1F && size > 10) {
        return true;
    }

    return false;  /* Normal: no shellcode detected */
}

/**
 * @brief Detect dangerous format specifiers with large width/precision
 *
 * WHAT: Check for format specifiers with dangerous width/precision values
 * WHY:  Large width/precision like %.100000s can cause DoS or memory issues
 * HOW:  Parse format specifiers and check for values over threshold
 *
 * @param str String to analyze
 * @return true if dangerous format specifier found
 */
static bool detect_dangerous_format_specifier(const char* str)
{
    if (!str) {
        return false;
    }

    const int DANGEROUS_WIDTH_THRESHOLD = 10000;

    for (const char* p = str; *p; p++) {
        if (*p != '%') continue;
        p++;
        if (!*p || *p == '%') continue;  /* %% is literal percent */

        /* Parse optional precision .N or width N */
        int value = 0;
        bool has_dot = (*p == '.');
        if (has_dot) p++;

        /* Parse the number */
        while (*p && isdigit((unsigned char)*p)) {
            /* W4-13: Check for overflow BEFORE multiplication */
            if (value > INT_MAX / 10) {
                return true;  /* Would overflow */
            }
            value = value * 10 + (*p - '0');
            if (value > DANGEROUS_WIDTH_THRESHOLD) {
                return true;  /* Dangerous width/precision */
            }
            p++;
        }

        /* Check if we have a format specifier after the number */
        if (*p && (strchr("diouxXeEfFgGaAcspn", *p) != NULL)) {
            if (value > DANGEROUS_WIDTH_THRESHOLD) {
                return true;
            }
        }

        if (!*p) break;
        p--;  /* Back up since outer loop will advance */
    }
    return false;
}

/**
 * @brief Count format specifiers in string
 *
 * WHAT: Count % format specifiers in input
 * WHY:  Too many specifiers suggests format string attack
 * HOW:  Scan for % followed by format character
 *
 * @param str String to analyze
 * @return Count of format specifiers
 */
static int count_format_specifiers(const char* str)
{
    if (!str) return 0;

    int count = 0;
    for (const char* p = str; *p; p++) {
        if (*p == '%' && *(p + 1)) {
            const char* next = p + 1;
            /* Skip optional width/precision specifiers */
            if (*next == '.') next++;
            while (*next && isdigit((unsigned char)*next)) next++;
            /* Common format specifiers */
            if (*next == 'd' || *next == 'i' || *next == 's' ||
                *next == 'x' || *next == 'X' || *next == 'p' ||
                *next == 'n' || *next == 'f' || *next == 'c' ||
                *next == 'u' || *next == 'o' || *next == 'e') {
                count++;
            }
        }
    }
    return count;
}

//=============================================================================
// Public API Implementation
//=============================================================================

/**
 * @brief Validate raw input data for threats
 *
 * WHAT: Comprehensive validation of arbitrary binary data
 * WHY:  First line of defense against malicious input entering the system
 * HOW:  Check for shellcode, buffer overflow, and suspicious patterns
 *
 * @param system BBB system handle
 * @param data Input data to validate
 * @param size Size of input data
 * @param result Output validation result
 * @return true if valid (no threats detected)
 */
bool bbb_validate_input(bbb_system_t system, const void* data,
                        size_t size, bbb_validation_result_t* result)
{
    /* Guard: NULL parameters */
    if (!result) {
        return false;
    }

    /* Initialize result to valid */
    memset(result, 0, sizeof(bbb_validation_result_t));
    result->valid = true;

    /* Guard: NULL system is invalid for input validation */
    if (!system) {
        result->valid = false;
        result->threat = BBB_THREAT_UNAUTHORIZED_ACCESS;
        result->severity = BBB_SEVERITY_HIGH;
        snprintf(result->reason, sizeof(result->reason),
                 "NULL BBB system - validation requires system context");
        return false;
    }

    /* Update validation statistics (always count validation attempts) */
    bbb_system_inc_validations(system);

    /* Guard: NULL data is invalid */
    if (!data) {
        result->valid = false;
        result->threat = BBB_THREAT_BUFFER_OVERFLOW;
        result->severity = BBB_SEVERITY_HIGH;
        snprintf(result->reason, sizeof(result->reason), "NULL data pointer");
        return false;
    }

    /* Guard: Zero size is valid (nothing to validate) */
    if (size == 0) {
        return true;
    }

    /* Check for shellcode patterns */
    if (detect_shellcode(data, size)) {
        result->valid = false;
        result->threat = BBB_THREAT_SHELLCODE;
        result->severity = BBB_SEVERITY_CRITICAL;
        snprintf(result->reason, sizeof(result->reason),
                 "Shellcode pattern detected in input data");
        bbb_report_threat(system, BBB_THREAT_SHELLCODE, BBB_SEVERITY_CRITICAL,
                          result->reason, data, data, size);
        return false;
    }

    /* For text data, perform string validation across the FULL byte range.
     *
     * EMBEDDED-NUL FIX: The previous implementation broke on the first '\0'
     * and called bbb_validate_string on a truncated prefix. An attacker could
     * sneak a benign prefix + NUL + dangerous payload past every validator
     * because strlen()-based scanning stops at the NUL. We now build a
     * NUL-stripped copy of the entire (data, size) buffer (replacing each
     * 0x00 with space 0x20 so pattern matching still works) and validate
     * that. Non-printable/non-space bytes still mark the buffer as binary
     * and fall through to the shellcode-only path above.
     *
     * Rationale: pattern matchers use strstr/strchr which are NUL-terminated,
     * so we cannot pass them a buffer with embedded NULs. Replacing NULs with
     * space is the simplest fix that preserves token boundaries without
     * silently dropping bytes (which would let "AA\0BB" look like "AA"+"BB"
     * and bypass detection of multi-character attack strings spanning the
     * NUL). */
    const char* str = (const char*)data;
    bool is_printable = true;
    for (size_t i = 0; i < size; i++) {
        unsigned char c = (unsigned char)str[i];
        if (c == 0) continue;  /* embedded NUL — allowed in scanning */
        if (!isprint(c) && !isspace(c)) {
            is_printable = false;
            break;
        }
    }

    if (is_printable && size > 0) {
        /* Create NUL-stripped copy of the entire buffer for string validation */
        char* safe_str = (char*)nimcp_malloc(size + 1);
        if (safe_str) {
            for (size_t i = 0; i < size; i++) {
                unsigned char c = (unsigned char)str[i];
                safe_str[i] = (c == 0) ? ' ' : (char)c;
            }
            safe_str[size] = '\0';
            bool valid = bbb_validate_string(system, safe_str, result);
            nimcp_free(safe_str);
            return valid;
        }
        /* P1-2 FIX: If malloc fails, treat as validation failure (return false),
         * not success. The old code fell through to 'return true' which is a
         * security hole - untrusted input would be treated as VALID on OOM. */
        result->valid = false;
        result->threat = BBB_THREAT_BUFFER_OVERFLOW;
        result->severity = BBB_SEVERITY_HIGH;
        snprintf(result->reason, sizeof(result->reason),
                 "Memory allocation failed during input validation");
        return false;
    }

    return true;
}

/**
 * @brief Validate string for SQL injection, XSS, and format string attacks
 *
 * WHAT: String-specific security validation
 * WHY:  Strings are the most common attack vector for injection attacks
 * HOW:  Pattern matching against known attack signatures
 *
 * @param system BBB system handle
 * @param str String to validate
 * @param result Output validation result
 * @return true if valid
 */
bool bbb_validate_string(bbb_system_t system, const char* str,
                         bbb_validation_result_t* result)
{
    /* Guard: NULL result */
    if (!result) {
        return false;
    }

    /* Initialize result */
    memset(result, 0, sizeof(bbb_validation_result_t));
    result->valid = true;

    /* Guard: NULL system requires system context for full validation */
    if (!system) {
        result->valid = false;
        result->threat = BBB_THREAT_UNAUTHORIZED_ACCESS;
        result->severity = BBB_SEVERITY_MEDIUM;
        snprintf(result->reason, sizeof(result->reason),
                 "NULL BBB system - string validation requires system context");
        return false;
    }

    /* Update validation statistics */
    bbb_system_inc_validations(system);

    /* Guard: NULL string is invalid */
    if (!str) {
        result->valid = false;
        result->threat = BBB_THREAT_BUFFER_OVERFLOW;
        result->severity = BBB_SEVERITY_HIGH;
        snprintf(result->reason, sizeof(result->reason), "NULL string pointer");
        return false;
    }

    /* Guard: Empty string is valid */
    if (*str == '\0') {
        return true;
    }

    /* Length check - use system config if available, otherwise default */
    size_t max_string_length = bbb_system_get_max_string_length(system);
    if (max_string_length == 0) {
        bbb_config_t cfg = bbb_default_config();
        max_string_length = cfg.input.max_string_length;
    }
    size_t len = strlen(str);
    if (len > max_string_length) {
        result->valid = false;
        result->threat = BBB_THREAT_BUFFER_OVERFLOW;
        result->severity = BBB_SEVERITY_HIGH;
        snprintf(result->reason, sizeof(result->reason),
                 "String exceeds max length (%zu > %zu)", len, max_string_length);
        bbb_report_threat(system, BBB_THREAT_BUFFER_OVERFLOW, BBB_SEVERITY_HIGH,
                          result->reason, str, str, len);
        return false;
    }

    /* SQL Injection Detection */
    bool sql_detected = check_patterns(str, sql_patterns);
    if (sql_detected) {
        result->valid = false;
        result->threat = BBB_THREAT_SQL_INJECTION;
        result->severity = BBB_SEVERITY_HIGH;
        snprintf(result->reason, sizeof(result->reason),
                 "SQL injection pattern detected");
        if (system) {
            bbb_report_threat(system, BBB_THREAT_SQL_INJECTION, BBB_SEVERITY_HIGH,
                              result->reason, str, str, strlen(str));
        }
        return false;
    }

    /* XSS Detection */
    if (check_patterns(str, xss_patterns)) {
        result->valid = false;
        result->threat = BBB_THREAT_CODE_INJECTION;
        result->severity = BBB_SEVERITY_HIGH;
        snprintf(result->reason, sizeof(result->reason),
                 "XSS attack pattern detected");
        bbb_report_threat(system, BBB_THREAT_CODE_INJECTION, BBB_SEVERITY_HIGH,
                          result->reason, str, str, strlen(str));
        return false;
    }

    /* Format String Detection - pattern-based */
    if (check_patterns(str, format_patterns)) {
        result->valid = false;
        result->threat = BBB_THREAT_FORMAT_STRING;
        result->severity = BBB_SEVERITY_CRITICAL;
        snprintf(result->reason, sizeof(result->reason),
                 "Format string attack pattern detected");
        bbb_report_threat(system, BBB_THREAT_FORMAT_STRING, BBB_SEVERITY_CRITICAL,
                          result->reason, str, str, strlen(str));
        return false;
    }

    /* Format String Detection - dangerous width/precision */
    if (detect_dangerous_format_specifier(str)) {
        result->valid = false;
        result->threat = BBB_THREAT_FORMAT_STRING;
        result->severity = BBB_SEVERITY_HIGH;
        snprintf(result->reason, sizeof(result->reason),
                 "Dangerous format specifier detected (large width/precision)");
        bbb_report_threat(system, BBB_THREAT_FORMAT_STRING, BBB_SEVERITY_HIGH,
                          result->reason, str, str, strlen(str));
        return false;
    }

    /* Excessive format specifiers */
    if (count_format_specifiers(str) > MAX_FORMAT_SPECIFIERS) {
        result->valid = false;
        result->threat = BBB_THREAT_FORMAT_STRING;
        result->severity = BBB_SEVERITY_MEDIUM;
        snprintf(result->reason, sizeof(result->reason),
                 "Excessive format specifiers detected");
        bbb_report_threat(system, BBB_THREAT_FORMAT_STRING, BBB_SEVERITY_MEDIUM,
                          result->reason, str, str, strlen(str));
        return false;
    }

    /* Enhanced detection: path traversal + shell injection.
     * These detectors run after the static patterns and propagate their
     * findings back to the caller's result struct. They internally call
     * bbb_report_threat() so the immune system gets notified. */
    {
        bbb_validation_result_t path_result;
        if (!bbb_validate_file_path(system, str, &path_result)) {
            *result = path_result;
            return false;
        }
        bbb_validation_result_t cmd_result;
        if (!bbb_validate_command(system, str, &cmd_result)) {
            *result = cmd_result;
            return false;
        }
    }

    return true;
}

/**
 * @brief Validate integer for overflow risks
 *
 * WHAT: Check integer values against safe bounds
 * WHY:  Integer overflow can lead to buffer overflows and logic errors
 * HOW:  Range checking against configured limits
 *
 * @param system BBB system handle
 * @param value Integer to validate
 * @param result Output validation result
 * @return true if valid
 */
bool bbb_validate_integer(bbb_system_t system, int64_t value,
                          bbb_validation_result_t* result)
{
    /* Guard: NULL result */
    if (!result) {
        return false;
    }

    /* Initialize result */
    memset(result, 0, sizeof(bbb_validation_result_t));
    result->valid = true;

    /* Guard: NULL system */
    if (!system) {
        result->valid = false;
        result->threat = BBB_THREAT_UNAUTHORIZED_ACCESS;
        result->severity = BBB_SEVERITY_MEDIUM;
        snprintf(result->reason, sizeof(result->reason),
                 "NULL BBB system - integer validation requires system context");
        return false;
    }

    /* Check for extreme values that often indicate overflow */
    /* INT64_MAX and INT64_MIN often result from overflow */
    if (value == INT64_MAX || value == INT64_MIN) {
        result->valid = false;
        result->threat = BBB_THREAT_INTEGER_OVERFLOW;
        result->severity = BBB_SEVERITY_MEDIUM;
        snprintf(result->reason, sizeof(result->reason),
                 "Integer at boundary - possible overflow");
        return false;
    }

    /* Honor configured bounds from bbb_input_config_t.min_integer/max_integer.
     * Out-of-range integers are treated as overflow attacks and reported to
     * the immune system. Only enforce when bounds are actually configured
     * (min != max) so a default zero-zero config doesn't reject every
     * non-zero integer. */
    int64_t min_v = bbb_system_get_min_integer(system);
    int64_t max_v = bbb_system_get_max_integer(system);
    if (min_v != max_v && (value < min_v || value > max_v)) {
        result->valid = false;
        result->threat = BBB_THREAT_INTEGER_OVERFLOW;
        result->severity = BBB_SEVERITY_MEDIUM;
        snprintf(result->reason, sizeof(result->reason),
                 "Integer %lld out of configured range [%lld, %lld]",
                 (long long)value, (long long)min_v, (long long)max_v);
        bbb_report_threat(system, BBB_THREAT_INTEGER_OVERFLOW, BBB_SEVERITY_MEDIUM,
                          result->reason, &value, &value, sizeof(value));
        return false;
    }

    /* Check for suspicious wrap-around patterns */
    /* Negative size values often indicate unsigned overflow */
    /* This is a heuristic - specific bounds should be checked by caller */

    return true;
}

/**
 * @brief Validate pointer is in valid memory range
 *
 * WHAT: Basic pointer validity check
 * WHY:  Prevent use of NULL, invalid, or out-of-bounds pointers
 * HOW:  Check for NULL and obviously invalid patterns
 *
 * @param system BBB system handle
 * @param ptr Pointer to validate
 * @param expected_size Expected accessible size
 * @param result Output validation result
 * @return true if valid
 */
bool bbb_validate_pointer(bbb_system_t system, const void* ptr,
                          size_t expected_size, bbb_validation_result_t* result)
{
    /* Use local result if caller didn't provide one */
    bbb_validation_result_t local_result;
    if (!result) {
        result = &local_result;
    }

    /* Initialize result */
    memset(result, 0, sizeof(bbb_validation_result_t));
    result->valid = true;

    bbb_validation_result_t* res = result;

    /* NULL system is acceptable - skip system-specific checks */
    (void)system;

    /* NULL pointer check */
    if (!ptr) {
        res->valid = false;
        res->threat = BBB_THREAT_MEMORY_VIOLATION;
        res->severity = BBB_SEVERITY_HIGH;
        snprintf(res->reason, sizeof(res->reason),
                 "NULL pointer detected");
        return false;
    }

    /* Zero size check */
    if (expected_size == 0) {
        res->valid = false;
        res->threat = BBB_THREAT_MEMORY_VIOLATION;
        res->severity = BBB_SEVERITY_LOW;
        snprintf(res->reason, sizeof(res->reason),
                 "Zero-size memory access");
        return false;
    }

    /* Check for obviously invalid addresses */
    uintptr_t addr = (uintptr_t)ptr;

    /* Very low addresses (often NULL-ish or kernel space) */
    if (addr < 0x1000) {
        res->valid = false;
        res->threat = BBB_THREAT_MEMORY_VIOLATION;
        res->severity = BBB_SEVERITY_HIGH;
        snprintf(res->reason, sizeof(res->reason),
                 "Pointer in low memory region (0x%lx)", (unsigned long)addr);
        return false;
    }

    /* x86_64 canonical-address split:
     *   user space:    0x0000_0000_0000_0000 .. 0x0000_7fff_ffff_ffff
     *   non-canonical: 0x0000_8000_0000_0000 .. 0xffff_7fff_ffff_ffff
     *   kernel space:  0xffff_8000_0000_0000 .. 0xffff_ffff_ffff_ffff
     * User-mode pointers MUST live in the low half. Anything in the
     * non-canonical hole or kernel half is an attack/garbage pointer.
     * Gated to 64-bit so 32-bit builds don't get bogus rejections. */
#if defined(__x86_64__) || (UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFULL)
    if (addr >= 0x0000800000000000ULL && addr < 0xFFFF800000000000ULL) {
        res->valid = false;
        res->threat = BBB_THREAT_MEMORY_VIOLATION;
        res->severity = BBB_SEVERITY_HIGH;
        snprintf(res->reason, sizeof(res->reason),
                 "Non-canonical pointer (0x%lx) - outside x86_64 user range",
                 (unsigned long)addr);
        return false;
    }
    if (addr >= 0xFFFF800000000000ULL) {
        res->valid = false;
        res->threat = BBB_THREAT_MEMORY_VIOLATION;
        res->severity = BBB_SEVERITY_HIGH;
        snprintf(res->reason, sizeof(res->reason),
                 "Kernel-space pointer (0x%lx) rejected", (unsigned long)addr);
        return false;
    }
#endif

    /* Alignment check for common data types */
    if (expected_size >= 8 && (addr & 0x7) != 0) {
        /* 8-byte alignment expected but not met */
        res->severity = BBB_SEVERITY_LOW;
        snprintf(res->reason, sizeof(res->reason),
                 "Pointer misaligned for 8-byte access");
        /* Not invalid, just a warning - don't fail */
    }

    return true;
}

/**
 * @brief Sanitize string by removing dangerous characters
 *
 * WHAT: Remove or escape dangerous characters from input
 * WHY:  Allow processing of input while neutralizing attack vectors
 * HOW:  Whitelist approach - keep only safe characters
 *
 * @param system BBB system handle
 * @param input Input string
 * @param output Output buffer for sanitized string
 * @param output_size Size of output buffer
 * @return Length of sanitized string, or -1 on error
 */
ssize_t bbb_sanitize_string(bbb_system_t system, const char* input,
                            char* output, size_t output_size)
{
    /* Guard: Invalid parameters */
    if (!input || !output || output_size == 0) {
        return -1;
    }

    (void)system;  /* Available for future configuration */

    size_t out_idx = 0;
    size_t in_len = strlen(input);

    for (size_t i = 0; i < in_len && out_idx < output_size - 1; i++) {
        char c = input[i];

        /* Whitelist: alphanumeric, spaces, basic punctuation */
        if (isalnum((unsigned char)c)) {
            output[out_idx++] = c;
        } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            output[out_idx++] = c;
        } else if (c == '.' || c == ',' || c == '!' || c == '?') {
            output[out_idx++] = c;
        } else if (c == '-' || c == '_' || c == ':') {
            output[out_idx++] = c;
        } else if (c == '(' || c == ')' || c == '[' || c == ']') {
            output[out_idx++] = c;
        } else if (c == '@' || c == '#' || c == '$' || c == '&') {
            /* Keep some special chars but not dangerous ones */
            output[out_idx++] = c;
        }
        /* Skip: < > ' " % \ / ` { } | ^ ~ and control characters */
    }

    /* Null terminate */
    output[out_idx] = '\0';

    return (ssize_t)out_idx;
}

//=============================================================================
// Test Reset Support
//=============================================================================

/**
 * @brief Reset input gate state for test isolation
 *
 * WHAT: Clear any accumulated input gate state between tests
 * WHY:  Prevent validation statistics and cached state from leaking between tests
 * HOW:  Reset is minimal since input gate has no significant mutable global state
 *       beyond mesh registration (which is not test-sensitive)
 *
 * NOTE: Called by bbb_reset_test_state() for unified BBB reset
 */
void bbb_input_gate_reset_internal(void)
{
    /* Input gate has no significant mutable global state that affects tests.
     * Mesh registration and health agent state are intentionally preserved.
     * This function exists for completeness and forward-compatibility
     * if input gate gains test-sensitive state in the future. */
}
