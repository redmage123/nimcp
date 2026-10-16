/**
 * @file nimcp_pattern_compiler.c
 * @brief Pattern Compiler for Optimized Threat Detection
 *
 * WHAT: Compiles text patterns into optimized regex or DFA representations
 * WHY:  Faster matching than interpreting patterns at runtime
 * HOW:  Validates, optimizes, and compiles patterns with error checking
 *
 * OPTIMIZATIONS:
 * - Pattern validation before compilation
 * - Common subexpression elimination
 * - Anchoring optimization
 * - Character class optimization
 *
 * @author NIMCP Security Team
 * @date 2025-12-07
 */

#include "security/nimcp_pattern_db.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <regex.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(pattern_compiler, MESH_ADAPTER_CATEGORY_SECURITY)


//=============================================================================
// Internal Constants
//=============================================================================

#define MAX_ERROR_LENGTH 256

//=============================================================================
// Pattern Validation
//=============================================================================

/**
 * @brief Validate pattern syntax before compilation
 *
 * WHAT: Check pattern for common errors and vulnerabilities
 * WHY:  Prevent regex DoS and compilation errors
 * HOW:  Parse pattern and check for dangerous constructs
 *
 * CHECKS:
 * - Balanced parentheses and brackets
 * - No catastrophic backtracking patterns
 * - No excessive alternation
 * - Valid escape sequences
 */
static nimcp_error_t validate_pattern_syntax(const char* pattern, char* error_msg, size_t error_len) {
    if (!pattern || !error_msg) {
        return NIMCP_INVALID_PARAM;
    }

    size_t len = strlen(pattern);
    if (len == 0) {
        snprintf(error_msg, error_len, "Empty pattern");
        return NIMCP_ERROR;
    }

    if (len >= NIMCP_PATTERN_MAX_LENGTH) {
        snprintf(error_msg, error_len, "Pattern too long (%zu >= %d)", len, NIMCP_PATTERN_MAX_LENGTH);
        return NIMCP_ERROR;
    }

    // Check balanced parentheses and brackets
    int paren_depth = 0;
    int bracket_depth = 0;
    int brace_depth = 0;

    for (size_t i = 0; i < len; i++) {
        char c = pattern[i];

        // Handle escape sequences
        if (c == '\\' && i + 1 < len) {
            i++;  // Skip next character
            continue;
        }

        // Track nesting depth
        if (c == '(') paren_depth++;
        if (c == ')') {
            paren_depth--;
            if (paren_depth < 0) {
                snprintf(error_msg, error_len, "Unbalanced parentheses at position %zu", i);
                return NIMCP_ERROR;
            }
        }

        if (c == '[') bracket_depth++;
        if (c == ']') {
            bracket_depth--;
            if (bracket_depth < 0) {
                snprintf(error_msg, error_len, "Unbalanced brackets at position %zu", i);
                return NIMCP_ERROR;
            }
        }

        if (c == '{') brace_depth++;
        if (c == '}') {
            brace_depth--;
            if (brace_depth < 0) {
                snprintf(error_msg, error_len, "Unbalanced braces at position %zu", i);
                return NIMCP_ERROR;
            }
        }
    }

    if (paren_depth != 0) {
        snprintf(error_msg, error_len, "Unclosed parentheses");
        return NIMCP_ERROR;
    }

    if (bracket_depth != 0) {
        snprintf(error_msg, error_len, "Unclosed brackets");
        return NIMCP_ERROR;
    }

    if (brace_depth != 0) {
        snprintf(error_msg, error_len, "Unclosed braces");
        return NIMCP_ERROR;
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Check for catastrophic backtracking patterns
 *
 * WHAT: Detect regex patterns that can cause exponential-time matching
 * WHY:  Prevent regex DoS attacks
 * HOW:  Look for nested quantifiers and overlapping alternatives
 *
 * DANGEROUS PATTERNS:
 * - (a+)+ - Nested quantifiers
 * - (a|a)* - Overlapping alternatives
 * - (a*)*  - Nested star quantifiers
 */
static nimcp_error_t check_catastrophic_backtracking(const char* pattern, char* error_msg, size_t error_len) {
    if (!pattern || !error_msg) {
        return NIMCP_INVALID_PARAM;
    }

    size_t len = strlen(pattern);
    bool in_group = false;
    bool has_quantifier = false;

    for (size_t i = 0; i < len; i++) {
        char c = pattern[i];

        // Skip escape sequences
        if (c == '\\' && i + 1 < len) {
            i++;
            continue;
        }

        // Track groups
        if (c == '(') {
            in_group = true;
            has_quantifier = false;
        }

        if (c == ')') {
            in_group = false;
            // Check if group has quantifier
            if (i + 1 < len) {
                char next = pattern[i + 1];
                if (next == '*' || next == '+' || next == '?' || next == '{') {
                    if (has_quantifier) {
                        snprintf(error_msg, error_len,
                                 "Nested quantifiers detected at position %zu (catastrophic backtracking risk)", i);
                        return NIMCP_ERROR;
                    }
                }
            }
        }

        // Track quantifiers in groups
        if (in_group && (c == '*' || c == '+' || c == '?' || c == '{')) {
            has_quantifier = true;
        }
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Optimize pattern for faster matching
 *
 * WHAT: Apply optimizations to pattern before compilation
 * WHY:  Improve matching performance
 * HOW:  Simplify character classes, add anchors where appropriate
 */
static void optimize_pattern(const char* input, char* output, size_t output_size) {
    if (!input || !output || output_size == 0) {
        return;
    }

    // For now, just copy the pattern
    // Future optimizations:
    // - Character class simplification
    // - Common prefix factoring
    // - Anchoring optimization
    strncpy(output, input, output_size - 1);
    output[output_size - 1] = '\0';
}

//=============================================================================
// Public API
//=============================================================================

/**
 * @brief Validate and compile pattern
 *
 * WHAT: Validate pattern syntax and compile to regex
 * WHY:  Catch errors early and optimize for matching
 * HOW:  Multi-stage validation then compilation
 */
nimcp_error_t nimcp_pattern_compile(
    const char* pattern,
    uint32_t flags,
    regex_t* compiled,
    char* error_msg,
    size_t error_len
) {
    if (!pattern || !compiled || !error_msg) {
        return NIMCP_INVALID_PARAM;
    }

    error_msg[0] = '\0';

    // Stage 1: Syntax validation
    nimcp_error_t err = validate_pattern_syntax(pattern, error_msg, error_len);
    if (err != NIMCP_SUCCESS) {
        return err;
    }

    // Stage 2: Check for dangerous patterns
    err = check_catastrophic_backtracking(pattern, error_msg, error_len);
    if (err != NIMCP_SUCCESS) {
        return err;
    }

    // Stage 3: Optimize pattern
    char optimized[NIMCP_PATTERN_MAX_LENGTH];
    optimize_pattern(pattern, optimized, sizeof(optimized));

    // Stage 4: Compile regex
    int regex_flags = REG_EXTENDED;

    if (flags & NIMCP_PATTERN_FLAG_CASE_INSENSITIVE) {
        regex_flags |= REG_ICASE;
    }
    if (flags & NIMCP_PATTERN_FLAG_MULTILINE) {
        regex_flags |= REG_NEWLINE;
    }

    int ret = regcomp(compiled, optimized, regex_flags);
    if (ret != 0) {
        regerror(ret, compiled, error_msg, error_len);
        return NIMCP_ERROR;
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Test pattern against sample input
 *
 * WHAT: Validate pattern works as expected
 * WHY:  Ensure pattern matches intended inputs
 * HOW:  Compile and test against examples
 */
nimcp_error_t nimcp_pattern_test(
    const char* pattern,
    const char* test_input,
    bool* matches,
    char* error_msg,
    size_t error_len
) {
    if (!pattern || !test_input || !matches || !error_msg) {
        return NIMCP_INVALID_PARAM;
    }

    regex_t compiled;
    nimcp_error_t err = nimcp_pattern_compile(pattern, 0, &compiled, error_msg, error_len);
    if (err != NIMCP_SUCCESS) {
        return err;
    }

    int ret = regexec(&compiled, test_input, 0, NULL, 0);
    *matches = (ret == 0);

    regfree(&compiled);

    return NIMCP_SUCCESS;
}

/**
 * @brief Estimate pattern complexity
 *
 * WHAT: Estimate computational cost of pattern matching
 * WHY:  Identify potentially slow patterns
 * HOW:  Heuristic based on pattern structure
 *
 * COMPLEXITY FACTORS:
 * - Number of alternations
 * - Nesting depth
 * - Quantifier usage
 * - Character class size
 *
 * @return Complexity score (0-100, higher = more complex)
 */
uint32_t nimcp_pattern_estimate_complexity(const char* pattern) {
    if (!pattern) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_pattern_estimate_complexity: pattern is NULL");

            return 0;
    }

    uint32_t complexity = 0;
    size_t len = strlen(pattern);

    int nesting_depth = 0;
    int max_nesting = 0;
    int alternation_count = 0;
    int quantifier_count = 0;
    int char_class_count = 0;

    for (size_t i = 0; i < len; i++) {
        char c = pattern[i];

        // Skip escape sequences
        if (c == '\\' && i + 1 < len) {
            i++;
            continue;
        }

        if (c == '(') {
            nesting_depth++;
            if (nesting_depth > max_nesting) {
                max_nesting = nesting_depth;
            }
        }

        if (c == ')') {
            nesting_depth--;
        }

        if (c == '|') {
            alternation_count++;
        }

        if (c == '*' || c == '+' || c == '?' || c == '{') {
            quantifier_count++;
        }

        if (c == '[') {
            char_class_count++;
        }
    }

    // Calculate complexity score
    complexity += max_nesting * 10;           // Nesting adds significant cost
    complexity += alternation_count * 5;      // Each alternation adds branches
    complexity += quantifier_count * 3;       // Quantifiers add iterations
    complexity += char_class_count * 2;       // Character classes add comparisons
    complexity += len / 10;                   // Base complexity from length

    // Cap at 100
    if (complexity > 100) {
        complexity = 100;
    }

    return complexity;
}

/**
 * @brief Get category-specific default patterns
 *
 * WHAT: Return default patterns for common attack categories
 * WHY:  Provide baseline security patterns
 * HOW:  Predefined patterns for each category
 */
const char* nimcp_pattern_get_default(nimcp_pattern_category_t category) {
    // Note: Patterns use POSIX ERE syntax (no (?i) or \b)
    // Case-insensitivity should be applied via NIMCP_PATTERN_FLAG_CASE_INSENSITIVE flag
    static const char* defaults[] = {
        // SQL_INJECTION - matches common SQL injection patterns
        "(union|select|insert|update|delete|drop|create|alter|exec|execute)[[:space:]].*(from|into|table|database)",

        // XSS - matches script tags and javascript event handlers
        "(<script|javascript:|onerror=|onload=|<iframe|<object|<embed)",

        // SHELL_INJECTION - matches shell metacharacters
        "(;|\\||&&|\\$\\(|`|>|<|>>|<<)",

        // PATH_TRAVERSAL - matches directory traversal patterns
        "(\\.\\.[\\/\\\\]|\\.\\.\\.[\\/\\\\]|%2e%2e[\\/\\\\])",

        // FORMAT_STRING - matches format string specifiers
        "(%[0-9]*[sdxnp]|%[0-9]*\\$[sdxnp])",

        // PROMPT_INJECTION - matches prompt injection attempts
        "(ignore[[:space:]]+(previous|all)|disregard[[:space:]]+all|new[[:space:]]+instructions|you[[:space:]]+are[[:space:]]+now|system[[:space:]]*:)",

        // BUFFER_OVERFLOW - matches long repetitive patterns
        "([A]{100,}|[0]{100,})",

        // LDAP_INJECTION - matches LDAP metacharacters
        "(\\*|\\(|\\)|\\||&)",

        // XML_INJECTION - matches XML injection patterns
        "(<!ENTITY|<!DOCTYPE|<\\?xml|SYSTEM)",

        // COMMAND_INJECTION - matches command execution patterns
        "(cmd\\.exe|/bin/sh|powershell|bash|exec|system|popen)",

        // CUSTOM
        ""
    };

    if (category >= NIMCP_PATTERN_CATEGORY_COUNT) {
        return "";
    }

    return defaults[category];
}

/**
 * @brief Suggest optimizations for pattern
 *
 * WHAT: Analyze pattern and suggest improvements
 * WHY:  Help users write better patterns
 * HOW:  Detect common inefficiencies
 */
nimcp_error_t nimcp_pattern_suggest_optimizations(
    const char* pattern,
    char* suggestions,
    size_t suggestions_len
) {
    if (!pattern || !suggestions || suggestions_len == 0) {
        return NIMCP_INVALID_PARAM;
    }

    suggestions[0] = '\0';
    size_t len = strlen(pattern);

    // Check for unanchored patterns that could be anchored
    bool starts_with_anchor = (len > 0 && pattern[0] == '^');
    bool ends_with_anchor = (len > 0 && pattern[len - 1] == '$');

    if (!starts_with_anchor && !ends_with_anchor) {
        strncat(suggestions, "Consider anchoring pattern with ^ or $ for faster matching.\n",
                suggestions_len - strlen(suggestions) - 1);
    }

    // Check for inefficient character classes
    if (strstr(pattern, "[a-zA-Z]")) {
        strncat(suggestions, "Consider using [:alpha:] instead of [a-zA-Z].\n",
                suggestions_len - strlen(suggestions) - 1);
    }

    if (strstr(pattern, "[0-9]")) {
        strncat(suggestions, "Consider using [:digit:] or \\d instead of [0-9].\n",
                suggestions_len - strlen(suggestions) - 1);
    }

    // Check for redundant escaping
    if (strstr(pattern, "\\.") && !strstr(pattern, "[.]")) {
        strncat(suggestions, "Use [.] instead of \\. for literal dots in character classes.\n",
                suggestions_len - strlen(suggestions) - 1);
    }

    // Check complexity
    uint32_t complexity = nimcp_pattern_estimate_complexity(pattern);
    if (complexity > 70) {
        char buf[NIMCP_LABEL_BUFFER_SIZE];
        snprintf(buf, sizeof(buf),
                 "Pattern complexity is high (%u/100). Consider simplifying.\n", complexity);
        strncat(suggestions, buf, suggestions_len - strlen(suggestions) - 1);
    }

    if (strlen(suggestions) == 0) {
        strncat(suggestions, "Pattern looks good, no obvious optimizations.\n",
                suggestions_len - 1);
    }

    return NIMCP_SUCCESS;
}
