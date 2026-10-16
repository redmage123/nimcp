/**
 * @file nimcp_security_language_bridge.c
 * @brief Security-Language Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Implements bidirectional bridge between security and language systems
 * WHY:  Enable input sanitization, injection detection, content filtering
 * HOW:  Pattern matching, policy evaluation, threat scoring
 */

#include "security/language/nimcp_security_language_bridge.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "constants/nimcp_threshold_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_language_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Internal Constants - Injection Patterns
 * ============================================================================ */

/**
 * @brief SQL injection detection patterns
 */
static const char* SQL_INJECTION_PATTERNS[] = {
    "union%20select", "union select", "UNION SELECT",
    "' or '1'='1", "\" or \"1\"=\"1", "' OR '1'='1",
    "'; drop table", "'; DROP TABLE", ";DROP TABLE",
    "1=1--", "1=1 --", "' --", "\" --",
    "exec sp_", "EXEC SP_", "xp_cmdshell",
    "waitfor delay", "WAITFOR DELAY",
    "benchmark(", "BENCHMARK(",
    "sleep(", "SLEEP(",
    "@@version", "information_schema",
    "load_file(", "LOAD_FILE(",
    "into outfile", "INTO OUTFILE",
    "into dumpfile", "INTO DUMPFILE",
    NULL
};

/**
 * @brief Shell injection detection patterns
 */
static const char* SHELL_INJECTION_PATTERNS[] = {
    "; /bin/", "; /usr/bin/",
    "| /bin/", "| /usr/bin/",
    "& /bin/", "& /usr/bin/",
    "`/bin/", "`/usr/bin/",
    "$(", "$(/", "$(`",
    "; cat ", "| cat ",
    "; ls ", "| ls ",
    "; rm ", "| rm ",
    "; wget ", "| wget ",
    "; curl ", "| curl ",
    "; nc ", "| nc ",
    "; bash ", "| bash ",
    "; sh ", "| sh ",
    "/etc/passwd", "/etc/shadow",
    "&&", "||", ">>", "<<",
    NULL
};

/**
 * @brief Code injection detection patterns
 */
static const char* CODE_INJECTION_PATTERNS[] = {
    "eval(", "eval (", "EVAL(",
    "exec(", "exec (", "EXEC(",
    "system(", "system (", "SYSTEM(",
    "passthru(", "passthru (",
    "shell_exec(", "shell_exec (",
    "popen(", "popen (",
    "proc_open(", "proc_open (",
    "__import__(", "importlib.",
    "subprocess.", "os.system(",
    "os.popen(", "commands.",
    "Runtime.getRuntime()",
    "ProcessBuilder(",
    NULL
};

/**
 * @brief Prompt injection detection patterns
 */
static const char* PROMPT_INJECTION_PATTERNS[] = {
    "ignore previous instructions",
    "ignore all previous",
    "disregard previous",
    "forget your instructions",
    "forget all previous",
    "new instructions:",
    "override instructions",
    "system prompt:",
    "you are now",
    "act as if",
    "pretend you are",
    "roleplay as",
    "jailbreak",
    "DAN mode",
    "developer mode",
    "[system]", "[SYSTEM]",
    "```system", "```SYSTEM",
    NULL
};

/**
 * @brief XSS detection patterns
 */
static const char* XSS_PATTERNS[] = {
    "<script", "</script>", "javascript:",
    "onerror=", "onload=", "onclick=",
    "onmouseover=", "onfocus=", "onblur=",
    "onsubmit=", "onchange=", "oninput=",
    "expression(", "vbscript:", "data:text/html",
    "<iframe", "</iframe>", "<object",
    "<embed", "<svg", "onanimationend=",
    "ontransitionend=", "onpointerover=",
    NULL
};

/**
 * @brief Path traversal detection patterns
 */
static const char* PATH_TRAVERSAL_PATTERNS[] = {
    "../", "..\\", "%2e%2e/", "%2e%2e\\",
    "....//", "....\\\\",
    "%252e%252e/", "%252e%252e\\",
    "..%c0%af", "..%c1%9c",
    NULL
};

/**
 * @brief Format string attack patterns
 */
static const char* FORMAT_STRING_PATTERNS[] = {
    "%s%s%s%s", "%n", "%x%x%x%x",
    "%.8x", "%08x", "%.16x",
    "%p%p%p", "%%n", "%hn",
    NULL
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current time in nanoseconds
 */
static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * @brief Case-insensitive substring search
 */
static const char* stristr(const char* haystack, const char* needle) {
    if (!haystack || !needle) {
        return NULL;
    }
    if (!*needle) return haystack;

    for (; *haystack; haystack++) {
        const char* h = haystack;
        const char* n = needle;

        while (*h && *n && (tolower((unsigned char)*h) == tolower((unsigned char)*n))) {
            h++;
            n++;
        }

        if (!*n) return haystack;
    }

    return NULL;
}

/**
 * @brief Check if input matches any pattern in array
 */
static bool match_patterns(
    const char* input,
    const char** patterns,
    const char** matched_pattern,
    size_t* match_offset
) {
    if (!input || !patterns) {
        return false;
    }

    for (int i = 0; patterns[i] != NULL; i++) {
        const char* found = stristr(input, patterns[i]);
        if (found) {
            if (matched_pattern) *matched_pattern = patterns[i];
            if (match_offset) *match_offset = (size_t)(found - input);
            return true;
        }
    }

    return false;
}

/**
 * @brief Escape special characters in string
 */
static char* escape_string(const char* input, size_t input_len, size_t* output_len) {
    if (!input || input_len == 0) {
        if (output_len) *output_len = 0;
        return NULL;
    }

    /* Worst case: every char needs escaping (6x expansion for &quot;) */
    size_t max_len = input_len * 6 + 1;
    char* output = (char*)nimcp_malloc(max_len);
    if (!output) {
        if (output_len) *output_len = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "output is NULL");

        return NULL;
    }

    size_t j = 0;
    for (size_t i = 0; i < input_len && input[i]; i++) {
        char c = input[i];
        switch (c) {
            case '<':
                memcpy(output + j, "&lt;", 4);
                j += 4;
                break;
            case '>':
                memcpy(output + j, "&gt;", 4);
                j += 4;
                break;
            case '&':
                memcpy(output + j, "&amp;", 5);
                j += 5;
                break;
            case '"':
                memcpy(output + j, "&quot;", 6);
                j += 6;
                break;
            case '\'':
                memcpy(output + j, "&#39;", 5);
                j += 5;
                break;
            case '\\':
                output[j++] = '\\';
                output[j++] = '\\';
                break;
            default:
                /* Remove control characters except whitespace */
                if (c >= 32 || c == '\t' || c == '\n' || c == '\r') {
                    output[j++] = c;
                }
                break;
        }

        if (j >= max_len - 10) break;  /* Safety margin */
    }

    output[j] = '\0';
    if (output_len) *output_len = j;
    return output;
}

/**
 * @brief Remove dangerous patterns from string
 */
static char* remove_dangerous_patterns(const char* input, size_t input_len, size_t* output_len) {
    if (!input || input_len == 0) {
        if (output_len) *output_len = 0;
        return NULL;
    }

    char* output = (char*)nimcp_malloc(input_len + 1);
    if (!output) {
        if (output_len) *output_len = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "output is NULL");

        return NULL;
    }

    memcpy(output, input, input_len);
    output[input_len] = '\0';

    /* Remove script tags */
    char* ptr;
    while ((ptr = (char*)stristr(output, "<script")) != NULL) {
        char* end = (char*)stristr(ptr, "</script>");
        if (end) {
            end += 9;
            memmove(ptr, end, strlen(end) + 1);
        } else {
            *ptr = '\0';
            break;
        }
    }

    /* Remove SQL comment sequences */
    while ((ptr = strstr(output, "--")) != NULL) {
        memmove(ptr, ptr + 2, strlen(ptr + 2) + 1);
    }

    /* Remove shell metacharacters */
    const char* dangerous_chars = "|;&`$";
    for (size_t i = 0; output[i]; i++) {
        if (strchr(dangerous_chars, output[i])) {
            output[i] = ' ';
        }
    }

    if (output_len) *output_len = strlen(output);
    return output;
}

/**
 * @brief Compute weighted threat score from detection results
 */
static float compute_weighted_threat_score(
    const security_language_detection_result_t* result
) {
    if (!result || !result->injection_detected) return 0.0f;

    /* Weights for different injection types */
    static const float TYPE_WEIGHTS[] = {
        [INJECTION_TYPE_NONE] = 0.0f,
        [INJECTION_TYPE_PROMPT] = 0.8f,
        [INJECTION_TYPE_SQL] = 0.9f,
        [INJECTION_TYPE_CODE] = 0.95f,
        [INJECTION_TYPE_SHELL] = 1.0f,
        [INJECTION_TYPE_XSS] = 0.7f,
        [INJECTION_TYPE_TEMPLATE] = 0.6f,
        [INJECTION_TYPE_LDAP] = 0.7f,
        [INJECTION_TYPE_XML] = 0.65f,
        [INJECTION_TYPE_PATH_TRAVERSAL] = 0.75f,
        [INJECTION_TYPE_FORMAT_STRING] = 0.85f,
        [INJECTION_TYPE_HEADER] = 0.6f,
        [INJECTION_TYPE_CUSTOM] = 0.5f,
    };

    float max_score = 0.0f;
    float sum_score = 0.0f;

    for (uint32_t i = 0; i < result->detection_count; i++) {
        const security_language_detection_t* det = &result->detections[i];
        float weight = (det->type < INJECTION_TYPE_COUNT) ?
                       TYPE_WEIGHTS[det->type] : 0.5f;
        float score = det->confidence * weight;

        if (score > max_score) max_score = score;
        sum_score += score;
    }

    /* Combine max and average for final score */
    float avg_score = (result->detection_count > 0) ?
                      sum_score / result->detection_count : 0.0f;

    return 0.7f * max_score + 0.3f * avg_score;
}

/**
 * @brief Map threat score to severity level
 */
static security_language_threat_severity_t score_to_severity(float score) {
    if (score >= 0.9f) return THREAT_SEVERITY_CRITICAL;
    if (score >= 0.7f) return THREAT_SEVERITY_HIGH;
    if (score >= 0.5f) return THREAT_SEVERITY_MEDIUM;
    if (score >= 0.2f) return THREAT_SEVERITY_LOW;
    return THREAT_SEVERITY_NONE;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

int security_language_default_config(security_language_bridge_config_t* config) {
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    memset(config, 0, sizeof(security_language_bridge_config_t));

    /* Enable all main features */
    config->enable_sanitization = true;
    config->enable_injection_detection = true;
    config->enable_content_filtering = true;
    config->enable_policy_checking = true;
    config->enable_output_validation = true;

    /* Sanitization config */
    config->sanitize.enable_html_sanitization = true;
    config->sanitize.enable_sql_sanitization = true;
    config->sanitize.enable_shell_sanitization = true;
    config->sanitize.enable_unicode_normalization = false;
    config->sanitize.strip_control_characters = true;
    config->sanitize.escape_special_characters = true;
    config->sanitize.max_input_length = SECURITY_LANGUAGE_MAX_INPUT_LEN;
    config->sanitize.sanitization_intensity = 0.7f;

    /* Detection config */
    config->detection.enable_prompt_injection_detection = true;
    config->detection.enable_sql_injection_detection = true;
    config->detection.enable_code_injection_detection = true;
    config->detection.enable_shell_injection_detection = true;
    config->detection.enable_xss_detection = true;
    config->detection.enable_heuristic_detection = true;
    config->detection.confidence_threshold = NIMCP_CONFIDENCE_MEDIUM;
    config->detection.max_detections = SECURITY_LANGUAGE_MAX_DETECTIONS;
    config->detection.use_pattern_database = true;

    /* Policy config */
    config->policy.enable_content_filtering = true;
    config->policy.block_harmful_content = true;
    config->policy.block_explicit_content = false;
    config->policy.block_pii = true;
    config->policy.enable_custom_policies = false;
    config->policy.policy_strictness = 0.5f;
    config->policy.policy_file_path = NULL;

    /* Threshold settings */
    config->threat_threshold = SECURITY_LANGUAGE_DEFAULT_THRESHOLD;
    config->block_threshold = 0.9f;

    /* Bio-async */
    config->enable_bio_async = true;

    /* Performance settings */
    config->enable_caching = true;
    config->cache_size = 256;
    config->enable_parallel_detection = false;
    config->worker_threads = 0;

    return NIMCP_SUCCESS;
}

security_language_bridge_t* security_language_bridge_create(
    const security_language_bridge_config_t* config
) {
    /* Allocate bridge */
    security_language_bridge_t* bridge =
        (security_language_bridge_t*)nimcp_malloc(sizeof(security_language_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate security-language bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    memset(bridge, 0, sizeof(security_language_bridge_t));

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, BIO_MODULE_SECURITY_LANGUAGE,
                         "security_language_bridge") != 0) {
        nimcp_free(bridge);
        NIMCP_LOGGING_ERROR("Failed to initialize bridge base");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_language_bridge_create: operation failed");
        return NULL;
    }

    /* Copy or use default config */
    if (config) {
        bridge->config = *config;
    } else {
        security_language_default_config(&bridge->config);
    }

    /* Initialize state */
    bridge->state.active = false;
    bridge->state.sanitization_enabled = bridge->config.enable_sanitization;
    bridge->state.detection_enabled = bridge->config.enable_injection_detection;
    bridge->state.filtering_enabled = bridge->config.enable_content_filtering;
    bridge->state.in_lockdown_mode = false;
    bridge->state.current_threat_level = THREAT_SEVERITY_NONE;
    bridge->state.last_update_time_ns = get_time_ns();

    /* Initialize effects */
    bridge->security_effects.sanitization_level = bridge->config.sanitize.sanitization_intensity;
    bridge->security_effects.current_threat_level = THREAT_SEVERITY_NONE;

    NIMCP_LOGGING_INFO("Created security-language bridge");
    return bridge;
}

void security_language_bridge_destroy(security_language_bridge_t* bridge) {
    /* Guard clause: NULL safe */
    if (!bridge) return;

    /* Disconnect bio-async */
    if (bridge->base.bio_async_enabled) {
        security_language_disconnect_bio_async(bridge);
    }

    /* Cleanup base */
    bridge_base_cleanup(&bridge->base);

    /* Free bridge */
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed security-language bridge");
}

int security_language_bridge_reset(security_language_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(security_language_bridge_stats_t));

    /* Reset effects */
    memset(&bridge->security_effects, 0, sizeof(security_to_language_effects_t));
    memset(&bridge->language_effects, 0, sizeof(language_to_security_effects_t));

    /* Reset state (preserve connections) */
    bridge->state.in_lockdown_mode = false;
    bridge->state.current_threat_level = THREAT_SEVERITY_NONE;
    bridge->state.last_update_time_ns = get_time_ns();

    /* Re-apply config defaults */
    bridge->security_effects.sanitization_level = bridge->config.sanitize.sanitization_intensity;

    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Reset security-language bridge");
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Connection Functions - Language Side
 * ============================================================================ */

int security_language_connect_orchestrator(
    security_language_bridge_t* bridge,
    language_orchestrator_t* orchestrator
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(orchestrator, NIMCP_ERROR_NULL_POINTER, "orchestrator is NULL");

    BRIDGE_LOCK(bridge);
    bridge->language_orchestrator = orchestrator;
    bridge->state.active = true;
    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Connected language orchestrator to security-language bridge");
    return NIMCP_SUCCESS;
}

int security_language_disconnect_orchestrator(security_language_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);
    bridge->language_orchestrator = NULL;
    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Disconnected language orchestrator from security-language bridge");
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Connection Functions - Security Side
 * ============================================================================ */

int security_language_connect_bbb(
    security_language_bridge_t* bridge,
    bbb_system_t bbb
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bbb, NIMCP_ERROR_NULL_POINTER, "bbb is NULL");

    BRIDGE_LOCK(bridge);
    bridge->bbb_system = bbb;
    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Connected BBB to security-language bridge");
    return NIMCP_SUCCESS;
}

int security_language_connect_pattern_db(
    security_language_bridge_t* bridge,
    nimcp_pattern_db_t pattern_db
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(pattern_db, NIMCP_ERROR_NULL_POINTER, "pattern_db is NULL");

    BRIDGE_LOCK(bridge);
    bridge->pattern_db = pattern_db;
    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Connected pattern database to security-language bridge");
    return NIMCP_SUCCESS;
}

int security_language_connect_policy_engine(
    security_language_bridge_t* bridge,
    nimcp_policy_engine_t policy_engine
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(policy_engine, NIMCP_ERROR_NULL_POINTER, "policy_engine is NULL");

    BRIDGE_LOCK(bridge);
    bridge->policy_engine = policy_engine;
    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Connected policy engine to security-language bridge");
    return NIMCP_SUCCESS;
}

int security_language_disconnect_security(security_language_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);
    bridge->bbb_system = NULL;
    bridge->pattern_db = NULL;
    bridge->policy_engine = NULL;
    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Disconnected security systems from security-language bridge");
    return NIMCP_SUCCESS;
}

bool security_language_is_connected(const security_language_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Bridge is considered connected if language orchestrator is connected */
    return bridge->language_orchestrator != NULL;
}

/* ============================================================================
 * Core Security Functions - Input Sanitization
 * ============================================================================ */

int security_language_sanitize_input(
    security_language_bridge_t* bridge,
    const char* input,
    size_t input_len,
    security_language_sanitize_result_t* result
) {
    return security_language_sanitize_input_ex(
        bridge, input, input_len, &bridge->config.sanitize, result
    );
}

int security_language_sanitize_input_ex(
    security_language_bridge_t* bridge,
    const char* input,
    size_t input_len,
    const security_language_sanitize_config_t* config,
    security_language_sanitize_result_t* result
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(input, NIMCP_ERROR_NULL_POINTER, "input is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_NULL_POINTER, "result is NULL");

    uint64_t start_time = get_time_ns();

    /* Initialize result */
    memset(result, 0, sizeof(security_language_sanitize_result_t));

    /* Use strlen if input_len is 0 */
    if (input_len == 0) {
        input_len = strlen(input);
    }

    /* Check max length */
    size_t effective_len = input_len;
    bool truncated = false;
    if (config && config->max_input_length > 0 &&
        input_len > config->max_input_length) {
        effective_len = config->max_input_length;
        truncated = true;
        result->actions[result->action_count++] = SANITIZE_ACTION_TRUNCATE;
    }

    BRIDGE_LOCK(bridge);

    /* Check for blocked patterns - if in lockdown mode, be more aggressive */
    if (bridge->state.in_lockdown_mode) {
        /* Block everything suspicious in lockdown */
        security_language_detection_result_t det_result;
        if (security_language_detect_injection(bridge, input, effective_len,
                                               &det_result) == 0) {
            if (det_result.injection_detected &&
                det_result.aggregate_threat_score >= 0.3f) {
                result->blocked = true;
                result->actions[result->action_count++] = SANITIZE_ACTION_BLOCK;
                snprintf(result->warning_message, sizeof(result->warning_message),
                        "Input blocked in lockdown mode (threat: %.2f)",
                        det_result.aggregate_threat_score);

                bridge->stats.inputs_blocked++;
                BRIDGE_UNLOCK(bridge);
                result->processing_time_ns = get_time_ns() - start_time;
                return NIMCP_SUCCESS;
            }
        }
    }

    BRIDGE_UNLOCK(bridge);

    /* Perform sanitization based on config */
    char* sanitized = NULL;
    size_t sanitized_len = 0;

    if (config && config->escape_special_characters) {
        /* Escape mode */
        sanitized = escape_string(input, effective_len, &sanitized_len);
        if (sanitized && sanitized_len != effective_len) {
            result->modified = true;
            result->actions[result->action_count++] = SANITIZE_ACTION_ESCAPE;
        }
    } else {
        /* Remove mode */
        sanitized = remove_dangerous_patterns(input, effective_len, &sanitized_len);
        if (sanitized && sanitized_len != effective_len) {
            result->modified = true;
            result->actions[result->action_count++] = SANITIZE_ACTION_REMOVE;
        }
    }

    if (!sanitized) {
        /* Fallback: just copy input */
        sanitized = (char*)nimcp_malloc(effective_len + 1);
        if (sanitized) {
            memcpy(sanitized, input, effective_len);
            sanitized[effective_len] = '\0';
            sanitized_len = effective_len;
        }
    }

    result->sanitized_output = sanitized;
    result->sanitized_length = sanitized_len;
    result->changes_made = result->action_count;

    if (truncated) {
        snprintf(result->warning_message, sizeof(result->warning_message),
                "Input truncated from %zu to %zu bytes",
                input_len, effective_len);
    }

    /* Update statistics */
    BRIDGE_LOCK(bridge);
    bridge->stats.total_inputs_processed++;
    if (result->modified) {
        bridge->stats.inputs_sanitized++;
    }
    bridge->stats.inputs_passed++;

    /* Update timing stats */
    result->processing_time_ns = get_time_ns() - start_time;
    float time_ns = (float)result->processing_time_ns;
    bridge->stats.avg_sanitize_time_ns =
        (bridge->stats.avg_sanitize_time_ns *
         (bridge->stats.total_inputs_processed - 1) + time_ns) /
        bridge->stats.total_inputs_processed;
    if (time_ns > bridge->stats.max_processing_time_ns) {
        bridge->stats.max_processing_time_ns = time_ns;
    }

    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

void security_language_sanitize_result_free(security_language_sanitize_result_t* result) {
    if (!result) return;

    if (result->sanitized_output) {
        nimcp_free(result->sanitized_output);
        result->sanitized_output = NULL;
    }
}

/* ============================================================================
 * Core Security Functions - Injection Detection
 * ============================================================================ */

/**
 * @brief Internal function to detect specific injection type
 */
static int detect_injection_type_internal(
    const char* input,
    size_t input_len,
    security_language_injection_type_t type,
    security_language_detection_t* detection
) {
    const char** patterns = NULL;
    const char* type_name = NULL;

    switch (type) {
        case INJECTION_TYPE_SQL:
            patterns = SQL_INJECTION_PATTERNS;
            type_name = "SQL injection";
            break;
        case INJECTION_TYPE_SHELL:
            patterns = SHELL_INJECTION_PATTERNS;
            type_name = "Shell injection";
            break;
        case INJECTION_TYPE_CODE:
            patterns = CODE_INJECTION_PATTERNS;
            type_name = "Code injection";
            break;
        case INJECTION_TYPE_PROMPT:
            patterns = PROMPT_INJECTION_PATTERNS;
            type_name = "Prompt injection";
            break;
        case INJECTION_TYPE_XSS:
            patterns = XSS_PATTERNS;
            type_name = "XSS";
            break;
        case INJECTION_TYPE_PATH_TRAVERSAL:
            patterns = PATH_TRAVERSAL_PATTERNS;
            type_name = "Path traversal";
            break;
        case INJECTION_TYPE_FORMAT_STRING:
            patterns = FORMAT_STRING_PATTERNS;
            type_name = "Format string";
            break;
        default:
            return NIMCP_ERROR_INVALID_PARAMETER;  /* Unsupported type */
    }

    if (!patterns) return NIMCP_ERROR_INVALID_PARAMETER;

    const char* matched_pattern = NULL;
    size_t match_offset = 0;

    if (match_patterns(input, patterns, &matched_pattern, &match_offset)) {
        detection->type = type;
        detection->severity = THREAT_SEVERITY_HIGH;
        detection->confidence = 0.85f;  /* High confidence for pattern match */
        detection->offset = match_offset;
        detection->length = strlen(matched_pattern);

        strncpy(detection->pattern_matched, matched_pattern,
                sizeof(detection->pattern_matched) - 1);
        detection->pattern_matched[sizeof(detection->pattern_matched) - 1] = '\0';

        snprintf(detection->description, sizeof(detection->description),
                "%s detected: '%s'", type_name, matched_pattern);

        return 1;  /* Detection found */
    }

    return 0;  /* No detection */
}

int security_language_detect_injection(
    security_language_bridge_t* bridge,
    const char* input,
    size_t input_len,
    security_language_detection_result_t* result
) {
    return security_language_detect_injection_ex(
        bridge, input, input_len, &bridge->config.detection, result
    );
}

int security_language_detect_injection_type(
    security_language_bridge_t* bridge,
    const char* input,
    size_t input_len,
    security_language_injection_type_t type,
    security_language_detection_result_t* result
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(input, NIMCP_ERROR_NULL_POINTER, "input is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_NULL_POINTER, "result is NULL");

    uint64_t start_time = get_time_ns();

    /* Initialize result */
    memset(result, 0, sizeof(security_language_detection_result_t));

    /* Use strlen if input_len is 0 */
    if (input_len == 0) {
        input_len = strlen(input);
    }

    /* Detect specific type */
    security_language_detection_t detection;
    memset(&detection, 0, sizeof(detection));

    int det_result = detect_injection_type_internal(input, input_len, type, &detection);

    if (det_result > 0) {
        result->injection_detected = true;
        result->detections[0] = detection;
        result->detection_count = 1;
        result->aggregate_threat_score = detection.confidence;
        result->max_severity = detection.severity;
    }

    result->scan_time_ns = get_time_ns() - start_time;

    /* Update statistics */
    BRIDGE_LOCK(bridge);
    if (result->injection_detected) {
        bridge->stats.injections_detected++;
        if (type < INJECTION_TYPE_COUNT) {
            bridge->stats.injections_by_type[type]++;
        }
    }
    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

int security_language_detect_injection_ex(
    security_language_bridge_t* bridge,
    const char* input,
    size_t input_len,
    const security_language_detection_config_t* config,
    security_language_detection_result_t* result
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(input, NIMCP_ERROR_NULL_POINTER, "input is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_NULL_POINTER, "result is NULL");

    uint64_t start_time = get_time_ns();

    /* Initialize result */
    memset(result, 0, sizeof(security_language_detection_result_t));

    /* Use strlen if input_len is 0 */
    if (input_len == 0) {
        input_len = strlen(input);
    }

    const security_language_detection_config_t* cfg = config ?
        config : &bridge->config.detection;

    uint32_t detection_count = 0;
    security_language_threat_severity_t max_severity = THREAT_SEVERITY_NONE;

    /* Check each enabled injection type */
    security_language_injection_type_t types_to_check[] = {
        INJECTION_TYPE_SQL,
        INJECTION_TYPE_SHELL,
        INJECTION_TYPE_CODE,
        INJECTION_TYPE_PROMPT,
        INJECTION_TYPE_XSS,
        INJECTION_TYPE_PATH_TRAVERSAL,
        INJECTION_TYPE_FORMAT_STRING
    };

    bool type_enabled[] = {
        cfg->enable_sql_injection_detection,
        cfg->enable_shell_injection_detection,
        cfg->enable_code_injection_detection,
        cfg->enable_prompt_injection_detection,
        cfg->enable_xss_detection,
        true,  /* Path traversal always enabled if detection is on */
        true   /* Format string always enabled if detection is on */
    };

    for (size_t i = 0; i < sizeof(types_to_check) / sizeof(types_to_check[0]); i++) {
        if (!type_enabled[i]) continue;
        if (detection_count >= cfg->max_detections) break;

        security_language_detection_t detection;
        memset(&detection, 0, sizeof(detection));

        int det_result = detect_injection_type_internal(
            input, input_len, types_to_check[i], &detection
        );

        if (det_result > 0 && detection.confidence >= cfg->confidence_threshold) {
            result->detections[detection_count++] = detection;
            result->injection_detected = true;

            if (detection.severity > max_severity) {
                max_severity = detection.severity;
            }
        }
    }

    /* Check pattern database if connected and enabled */
    if (cfg->use_pattern_database && bridge->pattern_db) {
        nimcp_pattern_match_result_t pattern_result;
        if (nimcp_pattern_db_match(bridge->pattern_db, input, &pattern_result)
            == NIMCP_SUCCESS) {
            if (pattern_result.matched &&
                detection_count < cfg->max_detections) {
                security_language_detection_t* det =
                    &result->detections[detection_count];

                det->type = INJECTION_TYPE_CUSTOM;
                det->severity = THREAT_SEVERITY_HIGH;
                det->confidence = pattern_result.threat_score;
                det->offset = pattern_result.match_offset;
                det->length = pattern_result.match_length;
                det->pattern_id = pattern_result.pattern_id;

                strncpy(det->description, pattern_result.description,
                       sizeof(det->description) - 1);

                detection_count++;
                result->injection_detected = true;

                if (THREAT_SEVERITY_HIGH > max_severity) {
                    max_severity = THREAT_SEVERITY_HIGH;
                }
            }
        }
    }

    result->detection_count = detection_count;
    result->max_severity = max_severity;
    result->aggregate_threat_score = compute_weighted_threat_score(result);
    result->scan_time_ns = get_time_ns() - start_time;

    /* Update statistics */
    BRIDGE_LOCK(bridge);
    if (result->injection_detected) {
        bridge->stats.injections_detected += detection_count;
        for (uint32_t i = 0; i < detection_count; i++) {
            security_language_injection_type_t type = result->detections[i].type;
            if (type < INJECTION_TYPE_COUNT) {
                bridge->stats.injections_by_type[type]++;
            }
        }

        /* Update confidence stats */
        float max_conf = 0.0f;
        for (uint32_t i = 0; i < detection_count; i++) {
            if (result->detections[i].confidence > max_conf) {
                max_conf = result->detections[i].confidence;
            }
        }
        if (max_conf > bridge->stats.max_detection_confidence) {
            bridge->stats.max_detection_confidence = max_conf;
        }
    }

    float time_ns = (float)result->scan_time_ns;
    uint64_t total = bridge->stats.injections_detected + 1;
    bridge->stats.avg_detection_time_ns =
        (bridge->stats.avg_detection_time_ns * (total - 1) + time_ns) / total;

    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Core Security Functions - Content Policy
 * ============================================================================ */

int security_language_check_content_policy(
    security_language_bridge_t* bridge,
    const char* content,
    size_t content_len,
    security_language_policy_result_t* result
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(content, NIMCP_ERROR_NULL_POINTER, "content is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_NULL_POINTER, "result is NULL");

    uint64_t start_time = get_time_ns();

    /* Initialize result */
    memset(result, 0, sizeof(security_language_policy_result_t));
    result->passed = true;

    /* Use strlen if content_len is 0 */
    if (content_len == 0) {
        content_len = strlen(content);
    }

    uint32_t violation_count = 0;

    /* Check for PII patterns if enabled */
    if (bridge->config.policy.block_pii) {
        /* Simple email pattern check */
        if (strstr(content, "@") && strstr(content, ".com")) {
            if (violation_count < SECURITY_LANGUAGE_MAX_VIOLATIONS) {
                result->violations[violation_count] = POLICY_VIOLATION_PII;
                result->violation_scores[violation_count] = 0.6f;
                strncpy(result->policy_ids[violation_count], "pii_email",
                       sizeof(result->policy_ids[0]) - 1);
                violation_count++;
                result->passed = false;
            }
        }

        /* Simple SSN pattern (XXX-XX-XXXX) */
        for (size_t i = 0; i + 11 <= content_len; i++) {
            if (isdigit(content[i]) && isdigit(content[i+1]) && isdigit(content[i+2]) &&
                content[i+3] == '-' &&
                isdigit(content[i+4]) && isdigit(content[i+5]) &&
                content[i+6] == '-' &&
                isdigit(content[i+7]) && isdigit(content[i+8]) &&
                isdigit(content[i+9]) && isdigit(content[i+10])) {
                if (violation_count < SECURITY_LANGUAGE_MAX_VIOLATIONS) {
                    result->violations[violation_count] = POLICY_VIOLATION_PII;
                    result->violation_scores[violation_count] = 0.9f;
                    strncpy(result->policy_ids[violation_count], "pii_ssn",
                           sizeof(result->policy_ids[0]) - 1);
                    violation_count++;
                    result->passed = false;
                }
                break;
            }
        }
    }

    /* Check policy engine if connected */
    if (bridge->policy_engine && bridge->config.policy.enable_custom_policies) {
        nimcp_policy_context_t ctx = nimcp_policy_context_create();
        if (ctx) {
            nimcp_policy_context_set_string(ctx, "content", content);
            nimcp_policy_context_set_int(ctx, "content_length", (int64_t)content_len);

            nimcp_policy_result_t policy_result;
            if (nimcp_policy_evaluate(bridge->policy_engine, ctx, &policy_result)
                == NIMCP_SUCCESS) {
                if (policy_result.action == NIMCP_POLICY_ACTION_DENY) {
                    result->passed = false;
                    if (violation_count < SECURITY_LANGUAGE_MAX_VIOLATIONS) {
                        result->violations[violation_count] = POLICY_VIOLATION_CUSTOM;
                        result->violation_scores[violation_count] = 0.8f;
                        if (policy_result.rule_name) {
                            strncpy(result->policy_ids[violation_count],
                                   policy_result.rule_name,
                                   sizeof(result->policy_ids[0]) - 1);
                        }
                        violation_count++;
                    }
                }
                nimcp_policy_result_free(&policy_result);
            }

            nimcp_policy_context_destroy(ctx);
        }
    }

    result->violation_count = violation_count;

    /* Compute aggregate score */
    float sum = 0.0f;
    for (uint32_t i = 0; i < violation_count; i++) {
        sum += result->violation_scores[i];
    }
    result->aggregate_score = (violation_count > 0) ? sum / violation_count : 0.0f;

    /* Generate recommendation */
    if (!result->passed) {
        if (result->aggregate_score >= 0.8f) {
            snprintf(result->recommendation, sizeof(result->recommendation),
                    "Block content: %u policy violations detected", violation_count);
        } else {
            snprintf(result->recommendation, sizeof(result->recommendation),
                    "Review content: %u policy violations detected", violation_count);
        }
    }

    result->check_time_ns = get_time_ns() - start_time;

    /* Update statistics */
    BRIDGE_LOCK(bridge);
    bridge->stats.policy_checks_performed++;
    if (!result->passed) {
        bridge->stats.policy_violations += violation_count;
        for (uint32_t i = 0; i < violation_count; i++) {
            if (result->violations[i] < POLICY_VIOLATION_COUNT) {
                bridge->stats.violations_by_type[result->violations[i]]++;
            }
        }
    }

    float time_ns = (float)result->check_time_ns;
    bridge->stats.avg_policy_check_time_ns =
        (bridge->stats.avg_policy_check_time_ns *
         (bridge->stats.policy_checks_performed - 1) + time_ns) /
        bridge->stats.policy_checks_performed;

    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

int security_language_check_policy_id(
    security_language_bridge_t* bridge,
    const char* content,
    size_t content_len,
    const char* policy_id,
    security_language_policy_result_t* result
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(content, NIMCP_ERROR_NULL_POINTER, "content is NULL");
    NIMCP_CHECK_THROW(policy_id, NIMCP_ERROR_NULL_POINTER, "policy_id is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_NULL_POINTER, "result is NULL");

    /* For now, delegate to general policy check */
    /* In a full implementation, this would filter to specific policy */
    return security_language_check_content_policy(bridge, content, content_len, result);
}

/* ============================================================================
 * Core Security Functions - Output Validation
 * ============================================================================ */

int security_language_validate_output(
    security_language_bridge_t* bridge,
    const char* output,
    size_t output_len,
    security_language_output_validation_t* result
) {
    return security_language_validate_output_ex(bridge, output, output_len, false, result);
}

int security_language_validate_output_ex(
    security_language_bridge_t* bridge,
    const char* output,
    size_t output_len,
    bool allow_modification,
    security_language_output_validation_t* result
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(output, NIMCP_ERROR_NULL_POINTER, "output is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_NULL_POINTER, "result is NULL");

    /* Initialize result */
    memset(result, 0, sizeof(security_language_output_validation_t));
    result->valid = true;
    result->safety_score = 1.0f;

    /* Use strlen if output_len is 0 */
    if (output_len == 0) {
        output_len = strlen(output);
    }

    /* Check for injections in output */
    int det_status = security_language_detect_injection(
        bridge, output, output_len, &result->injection_result
    );

    if (det_status == 0 && result->injection_result.injection_detected) {
        result->valid = false;
        result->requires_modification = true;
        result->safety_score -= result->injection_result.aggregate_threat_score * 0.5f;
    }

    /* Check content policy */
    int policy_status = security_language_check_content_policy(
        bridge, output, output_len, &result->policy_result
    );

    if (policy_status == 0 && !result->policy_result.passed) {
        result->valid = false;
        result->requires_modification = true;
        result->safety_score -= result->policy_result.aggregate_score * 0.5f;
    }

    /* Clamp safety score */
    if (result->safety_score < 0.0f) result->safety_score = 0.0f;

    /* Apply modification if allowed and needed */
    if (allow_modification && result->requires_modification) {
        security_language_sanitize_result_t sanitize_result;
        if (security_language_sanitize_input(bridge, output, output_len,
                                             &sanitize_result) == 0) {
            if (sanitize_result.sanitized_output &&
                sanitize_result.sanitized_length < SECURITY_LANGUAGE_MAX_OUTPUT_LEN) {
                memcpy(result->modified_output, sanitize_result.sanitized_output,
                       sanitize_result.sanitized_length);
                result->modified_output[sanitize_result.sanitized_length] = '\0';
                result->modified_length = sanitize_result.sanitized_length;
            }
            security_language_sanitize_result_free(&sanitize_result);
        }
    }

    /* Update statistics */
    BRIDGE_LOCK(bridge);
    bridge->stats.outputs_validated++;
    if (result->requires_modification) {
        bridge->stats.outputs_modified++;
    }
    if (!result->valid && result->safety_score < 0.3f) {
        bridge->stats.outputs_blocked++;
    }
    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Core Security Functions - Threat Scoring
 * ============================================================================ */

int security_language_get_threat_score(
    security_language_bridge_t* bridge,
    const char* text,
    size_t text_len,
    security_language_threat_score_t* score
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(text, NIMCP_ERROR_NULL_POINTER, "text is NULL");
    NIMCP_CHECK_THROW(score, NIMCP_ERROR_NULL_POINTER, "score is NULL");

    /* Initialize result */
    memset(score, 0, sizeof(security_language_threat_score_t));

    /* Use strlen if text_len is 0 */
    if (text_len == 0) {
        text_len = strlen(text);
    }

    /* Run injection detection */
    security_language_detection_result_t det_result;
    if (security_language_detect_injection(bridge, text, text_len, &det_result) == 0) {
        score->injection_score = det_result.aggregate_threat_score;
    }

    /* Check pattern database */
    if (bridge->pattern_db) {
        nimcp_pattern_match_result_t pattern_result;
        if (nimcp_pattern_db_match(bridge->pattern_db, text, &pattern_result)
            == NIMCP_SUCCESS) {
            score->pattern_score = pattern_result.threat_score;
        }
    }

    /* Run policy check */
    security_language_policy_result_t policy_result;
    if (security_language_check_content_policy(bridge, text, text_len,
                                               &policy_result) == 0) {
        score->policy_score = policy_result.aggregate_score;
    }

    /* Compute anomaly score (heuristic) */
    score->anomaly_score = 0.0f;

    /* Check for unusual character distributions */
    size_t special_count = 0;
    for (size_t i = 0; i < text_len; i++) {
        unsigned char c = (unsigned char)text[i];
        if (!isalnum(c) && !isspace(c) && c != '.' && c != ',') {
            special_count++;
        }
    }
    float special_ratio = (float)special_count / (float)(text_len + 1);
    if (special_ratio > 0.3f) {
        score->anomaly_score = special_ratio;
    }

    /* Compute overall score with weights */
    score->overall_score =
        0.4f * score->injection_score +
        0.25f * score->pattern_score +
        0.2f * score->policy_score +
        0.15f * score->anomaly_score;

    /* Determine severity */
    score->severity = score_to_severity(score->overall_score);

    /* Check if action required */
    score->requires_action = (score->overall_score >= bridge->config.threat_threshold);

    /* Generate summary */
    if (score->requires_action) {
        snprintf(score->summary, sizeof(score->summary),
                "Threat detected: overall=%.2f (inj=%.2f, pat=%.2f, pol=%.2f, anom=%.2f)",
                score->overall_score, score->injection_score, score->pattern_score,
                score->policy_score, score->anomaly_score);
    } else {
        snprintf(score->summary, sizeof(score->summary),
                "No significant threat: score=%.2f", score->overall_score);
    }

    /* Update statistics */
    BRIDGE_LOCK(bridge);
    bridge->stats.threat_scores_computed++;

    bridge->stats.avg_threat_score =
        (bridge->stats.avg_threat_score *
         (bridge->stats.threat_scores_computed - 1) + score->overall_score) /
        bridge->stats.threat_scores_computed;

    if (score->overall_score > bridge->stats.max_threat_score) {
        bridge->stats.max_threat_score = score->overall_score;
    }

    if (score->requires_action) {
        bridge->stats.threshold_exceeded_count++;
    }

    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

bool security_language_exceeds_threshold(
    security_language_bridge_t* bridge,
    const char* text,
    size_t text_len
) {
    if (!bridge || !text) {
        return false;
    }

    security_language_threat_score_t score;
    if (security_language_get_threat_score(bridge, text, text_len, &score) != 0) {
        return false;
    }

    return score.requires_action;
}

/* ============================================================================
 * Update and Query Functions
 * ============================================================================ */

int security_language_update(security_language_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    uint64_t current_time = get_time_ns();
    bridge->state.last_update_time_ns = current_time;

    /* Update bidirectional effects based on current state */

    /* Security -> Language effects */
    bridge->security_effects.current_threat_level = bridge->state.current_threat_level;

    if (bridge->state.in_lockdown_mode) {
        bridge->security_effects.processing_inhibition = 0.8f;
        bridge->security_effects.aggressive_sanitization = true;
        bridge->security_effects.block_external_input = true;
    } else {
        bridge->security_effects.processing_inhibition =
            (float)bridge->state.current_threat_level * 0.2f;
        bridge->security_effects.aggressive_sanitization = false;
        bridge->security_effects.block_external_input = false;
    }

    /* Language -> Security effects (update from detection stats) */
    bridge->language_effects.detected_injection_count =
        (uint32_t)bridge->stats.injections_detected;
    bridge->language_effects.inputs_scanned =
        bridge->stats.total_inputs_processed;
    bridge->language_effects.inputs_blocked =
        bridge->stats.inputs_blocked;
    bridge->language_effects.outputs_validated =
        bridge->stats.outputs_validated;
    bridge->language_effects.avg_threat_score =
        bridge->stats.avg_threat_score;

    /* Process bio-async inbox if enabled */
    if (bridge->base.bio_async_enabled) {
        security_language_process_inbox(bridge, 0);
    }

    /* Record update in base */
    bridge_base_record_update(&bridge->base);

    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

int security_language_apply_modulation(security_language_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* This would push security effects to language orchestrator */
    /* For now, just ensure effects are current */
    return security_language_update(bridge);
}

int security_language_query_effects(
    const security_language_bridge_t* bridge,
    security_to_language_effects_t* security_effects,
    language_to_security_effects_t* language_effects
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (security_effects) {
        *security_effects = bridge->security_effects;
    }

    if (language_effects) {
        *language_effects = bridge->language_effects;
    }

    return NIMCP_SUCCESS;
}

security_language_threat_severity_t security_language_get_threat_level(
    const security_language_bridge_t* bridge
) {
    if (!bridge) return THREAT_SEVERITY_NONE;
    return bridge->state.current_threat_level;
}

int security_language_set_threat_level(
    security_language_bridge_t* bridge,
    security_language_threat_severity_t level
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (level >= THREAT_SEVERITY_COUNT) {
        NIMCP_LOGGING_ERROR("Invalid threat level: %d", level);
        return NIMCP_ERROR_INVALID_PARAMETER;
    }

    BRIDGE_LOCK(bridge);
    bridge->state.current_threat_level = level;
    bridge->security_effects.current_threat_level = level;
    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Set threat level to %s",
                      security_language_threat_severity_name(level));
    return NIMCP_SUCCESS;
}

int security_language_enter_lockdown(security_language_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    bridge->state.in_lockdown_mode = true;
    bridge->state.current_threat_level = THREAT_SEVERITY_CRITICAL;
    bridge->security_effects.current_threat_level = THREAT_SEVERITY_CRITICAL;
    bridge->security_effects.aggressive_sanitization = true;
    bridge->security_effects.block_external_input = true;
    bridge->security_effects.restrict_generation = true;
    bridge->security_effects.processing_inhibition = 1.0f;

    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_WARN("Security-language bridge entered LOCKDOWN mode");
    return NIMCP_SUCCESS;
}

int security_language_exit_lockdown(security_language_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    bridge->state.in_lockdown_mode = false;
    bridge->state.current_threat_level = THREAT_SEVERITY_NONE;
    bridge->security_effects.current_threat_level = THREAT_SEVERITY_NONE;
    bridge->security_effects.aggressive_sanitization = false;
    bridge->security_effects.block_external_input = false;
    bridge->security_effects.restrict_generation = false;
    bridge->security_effects.processing_inhibition = 0.0f;

    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Security-language bridge exited lockdown mode");
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics Functions
 * ============================================================================ */

int security_language_get_stats(
    const security_language_bridge_t* bridge,
    security_language_bridge_stats_t* stats
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    *stats = bridge->stats;
    return NIMCP_SUCCESS;
}

int security_language_reset_stats(security_language_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);
    memset(&bridge->stats, 0, sizeof(security_language_bridge_stats_t));
    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Reset security-language bridge statistics");
    return NIMCP_SUCCESS;
}

int security_language_get_state(
    const security_language_bridge_t* bridge,
    security_language_bridge_state_t* state
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_NULL_POINTER, "state is NULL");

    *state = bridge->state;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async Functions
 * ============================================================================ */

int security_language_connect_bio_async(security_language_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    return bridge_base_connect_bio_async(&bridge->base);
}

int security_language_disconnect_bio_async(security_language_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    return bridge_base_disconnect_bio_async(&bridge->base);
}

bool security_language_is_bio_async_connected(const security_language_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge_base_is_bio_async_connected(&bridge->base);
}

uint32_t security_language_process_inbox(
    security_language_bridge_t* bridge,
    uint32_t max_messages
) {
    /* Guard clause */
    if (!bridge) return 0;
    if (!bridge->base.bio_async_enabled) return 0;

    /* Process messages from bio-async inbox */
    uint32_t processed = 0;

    /* In a full implementation, this would read from bio_module_context_t inbox */
    /* For now, just return 0 */
    (void)max_messages;

    return processed;
}

int security_language_broadcast_detection(
    security_language_bridge_t* bridge,
    const security_language_detection_t* detection
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(detection, NIMCP_ERROR_NULL_POINTER, "detection is NULL");

    if (!bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;  /* Not an error, just not connected */
    }

    /* In a full implementation, this would send via bio_router_send */
    NIMCP_LOGGING_DEBUG("Would broadcast detection: %s (type=%d, confidence=%.2f)",
                       detection->description, detection->type, detection->confidence);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* security_language_injection_type_name(security_language_injection_type_t type) {
    static const char* names[] = {
        [INJECTION_TYPE_NONE] = "None",
        [INJECTION_TYPE_PROMPT] = "Prompt Injection",
        [INJECTION_TYPE_SQL] = "SQL Injection",
        [INJECTION_TYPE_CODE] = "Code Injection",
        [INJECTION_TYPE_SHELL] = "Shell Injection",
        [INJECTION_TYPE_XSS] = "XSS",
        [INJECTION_TYPE_TEMPLATE] = "Template Injection",
        [INJECTION_TYPE_LDAP] = "LDAP Injection",
        [INJECTION_TYPE_XML] = "XML Injection",
        [INJECTION_TYPE_PATH_TRAVERSAL] = "Path Traversal",
        [INJECTION_TYPE_FORMAT_STRING] = "Format String",
        [INJECTION_TYPE_HEADER] = "Header Injection",
        [INJECTION_TYPE_CUSTOM] = "Custom"
    };

    if (type < INJECTION_TYPE_COUNT) {
        return names[type];
    }
    return "Unknown";
}

const char* security_language_sanitize_action_name(security_language_sanitize_action_t action) {
    static const char* names[] = {
        [SANITIZE_ACTION_NONE] = "None",
        [SANITIZE_ACTION_ESCAPE] = "Escape",
        [SANITIZE_ACTION_REMOVE] = "Remove",
        [SANITIZE_ACTION_REPLACE] = "Replace",
        [SANITIZE_ACTION_TRUNCATE] = "Truncate",
        [SANITIZE_ACTION_BLOCK] = "Block"
    };

    if (action < SANITIZE_ACTION_COUNT) {
        return names[action];
    }
    return "Unknown";
}

const char* security_language_policy_violation_name(security_language_policy_violation_t violation) {
    static const char* names[] = {
        [POLICY_VIOLATION_NONE] = "None",
        [POLICY_VIOLATION_HARMFUL] = "Harmful Content",
        [POLICY_VIOLATION_EXPLICIT] = "Explicit Content",
        [POLICY_VIOLATION_HATE] = "Hate Speech",
        [POLICY_VIOLATION_VIOLENCE] = "Violence/Threats",
        [POLICY_VIOLATION_PII] = "Personal Information",
        [POLICY_VIOLATION_CONFIDENTIAL] = "Confidential Data",
        [POLICY_VIOLATION_COPYRIGHTED] = "Copyrighted Content",
        [POLICY_VIOLATION_CUSTOM] = "Custom Policy"
    };

    if (violation < POLICY_VIOLATION_COUNT) {
        return names[violation];
    }
    return "Unknown";
}

const char* security_language_threat_severity_name(security_language_threat_severity_t severity) {
    static const char* names[] = {
        [THREAT_SEVERITY_NONE] = "None",
        [THREAT_SEVERITY_LOW] = "Low",
        [THREAT_SEVERITY_MEDIUM] = "Medium",
        [THREAT_SEVERITY_HIGH] = "High",
        [THREAT_SEVERITY_CRITICAL] = "Critical"
    };

    if (severity < THREAT_SEVERITY_COUNT) {
        return names[severity];
    }
    return "Unknown";
}

void security_language_print_detection(const security_language_detection_result_t* result) {
    if (!result) return;

    printf("=== Injection Detection Result ===\n");
    printf("Detected: %s\n", result->injection_detected ? "YES" : "NO");
    printf("Count: %u\n", result->detection_count);
    printf("Aggregate Score: %.3f\n", result->aggregate_threat_score);
    printf("Max Severity: %s\n",
           security_language_threat_severity_name(result->max_severity));
    printf("Scan Time: %lu ns\n", (unsigned long)result->scan_time_ns);

    for (uint32_t i = 0; i < result->detection_count; i++) {
        const security_language_detection_t* det = &result->detections[i];
        printf("\n  Detection %u:\n", i + 1);
        printf("    Type: %s\n", security_language_injection_type_name(det->type));
        printf("    Severity: %s\n", security_language_threat_severity_name(det->severity));
        printf("    Confidence: %.3f\n", det->confidence);
        printf("    Offset: %zu, Length: %zu\n", det->offset, det->length);
        printf("    Pattern: %s\n", det->pattern_matched);
        printf("    Description: %s\n", det->description);
    }

    printf("==================================\n");
}

void security_language_print_summary(const security_language_bridge_t* bridge) {
    if (!bridge) return;

    printf("=== Security-Language Bridge Summary ===\n");
    printf("State: %s\n", bridge->state.active ? "Active" : "Inactive");
    printf("Lockdown: %s\n", bridge->state.in_lockdown_mode ? "YES" : "NO");
    printf("Threat Level: %s\n",
           security_language_threat_severity_name(bridge->state.current_threat_level));

    printf("\nConnections:\n");
    printf("  Language Orchestrator: %s\n",
           bridge->language_orchestrator ? "Connected" : "Not connected");
    printf("  BBB System: %s\n",
           bridge->bbb_system ? "Connected" : "Not connected");
    printf("  Pattern DB: %s\n",
           bridge->pattern_db ? "Connected" : "Not connected");
    printf("  Policy Engine: %s\n",
           bridge->policy_engine ? "Connected" : "Not connected");
    printf("  Bio-Async: %s\n",
           bridge->base.bio_async_enabled ? "Enabled" : "Disabled");

    printf("\nStatistics:\n");
    printf("  Inputs Processed: %lu\n",
           (unsigned long)bridge->stats.total_inputs_processed);
    printf("  Inputs Sanitized: %lu\n",
           (unsigned long)bridge->stats.inputs_sanitized);
    printf("  Inputs Blocked: %lu\n",
           (unsigned long)bridge->stats.inputs_blocked);
    printf("  Injections Detected: %lu\n",
           (unsigned long)bridge->stats.injections_detected);
    printf("  Policy Violations: %lu\n",
           (unsigned long)bridge->stats.policy_violations);
    printf("  Outputs Validated: %lu\n",
           (unsigned long)bridge->stats.outputs_validated);
    printf("  Avg Threat Score: %.3f\n", bridge->stats.avg_threat_score);
    printf("  Max Threat Score: %.3f\n", bridge->stats.max_threat_score);

    printf("=========================================\n");
}

bbb_severity_t security_language_to_bbb_severity(security_language_threat_severity_t severity) {
    switch (severity) {
        case THREAT_SEVERITY_NONE:     return BBB_SEVERITY_NONE;
        case THREAT_SEVERITY_LOW:      return BBB_SEVERITY_LOW;
        case THREAT_SEVERITY_MEDIUM:   return BBB_SEVERITY_MEDIUM;
        case THREAT_SEVERITY_HIGH:     return BBB_SEVERITY_HIGH;
        case THREAT_SEVERITY_CRITICAL: return BBB_SEVERITY_CRITICAL;
        default:                       return BBB_SEVERITY_NONE;
    }
}

security_language_injection_type_t security_language_from_bbb_threat(bbb_threat_type_t bbb_threat) {
    switch (bbb_threat) {
        case BBB_THREAT_SQL_INJECTION:    return INJECTION_TYPE_SQL;
        case BBB_THREAT_CODE_INJECTION:   return INJECTION_TYPE_CODE;
        case BBB_THREAT_SHELLCODE:        return INJECTION_TYPE_SHELL;
        case BBB_THREAT_SHELL_INJECTION:  return INJECTION_TYPE_SHELL;
        case BBB_THREAT_PATH_TRAVERSAL:   return INJECTION_TYPE_PATH_TRAVERSAL;
        case BBB_THREAT_FORMAT_STRING:    return INJECTION_TYPE_FORMAT_STRING;
        default:                          return INJECTION_TYPE_NONE;
    }
}
