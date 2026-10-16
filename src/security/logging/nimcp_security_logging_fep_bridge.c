/**
 * @file nimcp_security_logging_fep_bridge.c
 * @brief Implementation of Security Logging FEP Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Implementation of FEP integration for security logging
 * WHY:  Map log injection, tampering, and audit trail corruption
 *       to free energy for unified detection and active inference protection
 * HOW:  Compute FE from log state, use prediction errors for detection,
 *       apply active inference for protective actions
 */

#include "security/logging/nimcp_security_logging_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_logging_fep_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Internal Constants
 * ============================================================================ */

/** @brief Minimum time between protective actions (ms) */
#define MIN_PROTECTION_INTERVAL_MS 500

/** @brief Smoothing factor for running averages */
#define RUNNING_AVG_ALPHA 0.1f

#include "constants/nimcp_constants.h"
/** @brief Small epsilon for division safety */
#define FEP_EPSILON NIMCP_EPSILON_NUMERICAL

/** @brief Maximum timestamp drift before alert (nanoseconds) */
#define DEFAULT_TIMESTAMP_TOLERANCE_NS 1000000000ULL  /* 1 second */

/* ============================================================================
 * Injection Pattern Definitions
 * ============================================================================ */

/** @brief SQL injection patterns */
static const char* g_sql_patterns[] = {
    "SELECT ",
    "INSERT ",
    "UPDATE ",
    "DELETE ",
    "DROP ",
    "UNION ",
    "--",
    "' OR '",
    "' AND '",
    "1=1",
    "'; --",
    "/*",
    "*/",
    "EXEC ",
    "EXECUTE ",
    "xp_",
    "sp_",
    NULL
};

/** @brief Command injection patterns */
static const char* g_command_patterns[] = {
    "| ",
    "; ",
    "` ",
    "$(",
    "&& ",
    "|| ",
    "> ",
    "< ",
    "$((",
    "eval ",
    "exec(",
    "system(",
    "/bin/",
    "/usr/bin/",
    "cmd.exe",
    "powershell",
    NULL
};

/** @brief Script injection patterns */
static const char* g_script_patterns[] = {
    "<script",
    "</script>",
    "javascript:",
    "onerror=",
    "onclick=",
    "onload=",
    "onmouseover=",
    "eval(",
    "document.",
    "window.",
    "alert(",
    "String.fromCharCode",
    NULL
};

/** @brief Format string patterns */
static const char* g_format_patterns[] = {
    "%n",
    "%s%s%s",
    "%x%x%x",
    "%p%p%p",
    "%d%d%d%d%d%d%d%d",
    "%.16705u",
    "%((",
    NULL
};

/* ============================================================================
 * String Tables
 * ============================================================================ */

static const char* g_integrity_names[SEC_LOG_FEP_INTEGRITY_COUNT] = {
    "NORMAL",
    "SUSPICIOUS",
    "TAMPERED",
    "COMPROMISED"
};

static const char* g_action_names[SEC_LOG_FEP_ACTION_COUNT] = {
    "NONE",
    "MONITOR",
    "PRESERVE",
    "INCREASE_DETAIL",
    "BLOCK_SOURCE",
    "ALERT",
    "LOCKDOWN",
    "ROTATE"
};

static const char* g_detection_names[SEC_LOG_FEP_DETECT_COUNT] = {
    "NONE",
    "INJECTION",
    "DELETION",
    "TIMESTAMP_MANIP",
    "AUDIT_CORRUPT",
    "PATTERN_ANOMALY",
    "SEQUENCE_GAP",
    "FORMAT_VIOLATION"
};

static const char* g_inject_type_names[SEC_LOG_FEP_INJECT_COUNT] = {
    "NONE",
    "SQL",
    "COMMAND",
    "SCRIPT",
    "NEWLINE",
    "FORMAT_STRING",
    "NULL_BYTE"
};

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in milliseconds
 *
 * WHAT: Retrieve monotonic time for timing operations
 * WHY:  Track update intervals and protection timing
 * HOW:  Use platform time API
 */
static uint64_t get_timestamp_ms(void)
{
    return nimcp_time_monotonic_us() / 1000;
}

/**
 * @brief Case-insensitive substring search
 *
 * WHAT: Find pattern in string ignoring case
 * WHY:  Injection attacks may use various case combinations
 * HOW:  Compare lowercase versions
 */
static bool contains_pattern_icase(const char* haystack, const char* needle)
{
    if (!haystack || !needle) {
        return false;
    }

    size_t hay_len = strlen(haystack);
    size_t needle_len = strlen(needle);

    if (needle_len > hay_len) {
        return false;
    }

    for (size_t i = 0; i <= hay_len - needle_len; i++) {
        bool match = true;
        for (size_t j = 0; j < needle_len; j++) {
            if (tolower((unsigned char)haystack[i + j]) !=
                tolower((unsigned char)needle[j])) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

/**
 * @brief Check for null byte in string
 *
 * WHAT: Detect embedded null bytes (truncation attack)
 * WHY:  Null bytes can truncate log entries
 * HOW:  Scan for \0 before expected end
 */
static bool contains_null_byte(const char* str, size_t len)
{
    if (!str) {
        return false;
    }

    for (size_t i = 0; i < len; i++) {
        if (str[i] == '\0') {
            /* Found null before end of declared length */
            return (i < len - 1);
        }
    }
    return false;
}

/**
 * @brief Check for newline injection
 *
 * WHAT: Detect newline characters that could split log entries
 * WHY:  Attackers may inject newlines to forge entries
 * HOW:  Check for \n, \r, or CRLF patterns
 */
static bool contains_newline_injection(const char* str, size_t len)
{
    if (!str) {
        return false;
    }

    for (size_t i = 0; i < len && str[i] != '\0'; i++) {
        if (str[i] == '\n' || str[i] == '\r') {
            return true;
        }
    }
    return false;
}

/**
 * @brief Compute softmax over action values
 *
 * WHAT: Convert action values to probabilities via softmax
 * WHY:  Stochastic action selection for active inference
 * HOW:  exp(v/T) / sum(exp(v/T))
 */
static void softmax_actions(
    const float* values,
    float* probs,
    uint32_t count,
    float temperature
)
{
    if (!values || !probs || count == 0) {
        return;
    }

    /* Find max for numerical stability */
    float max_val = values[0];
    for (uint32_t i = 1; i < count; i++) {
        if (values[i] > max_val) {
            max_val = values[i];
        }
    }

    /* Compute exp and sum */
    float sum = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        probs[i] = expf((values[i] - max_val) / (temperature + FEP_EPSILON));
        sum += probs[i];
    }

    /* Normalize */
    if (sum > FEP_EPSILON) {
        for (uint32_t i = 0; i < count; i++) {
            probs[i] /= sum;
        }
    } else {
        /* Uniform if sum is too small */
        for (uint32_t i = 0; i < count; i++) {
            probs[i] = 1.0f / (float)count;
        }
    }
}

/**
 * @brief Classify integrity level from free energy
 *
 * WHAT: Map FE value to integrity classification
 * WHY:  Determine appropriate security response
 * HOW:  Threshold-based classification
 */
static sec_log_fep_integrity_t classify_integrity(
    float free_energy,
    const sec_log_fep_config_t* config
)
{
    if (free_energy < config->normal_fe_threshold) {
        return SEC_LOG_FEP_INTEGRITY_NORMAL;
    } else if (free_energy < config->anomaly_fe_threshold) {
        return SEC_LOG_FEP_INTEGRITY_SUSPICIOUS;
    } else if (free_energy < config->attack_fe_threshold) {
        return SEC_LOG_FEP_INTEGRITY_TAMPERED;
    } else {
        return SEC_LOG_FEP_INTEGRITY_COMPROMISED;
    }
}

/**
 * @brief Update running average
 *
 * WHAT: Exponential moving average update
 * WHY:  Smooth metrics over time
 * HOW:  new_avg = alpha * new_val + (1 - alpha) * old_avg
 */
static float update_running_avg(float old_avg, float new_val, float alpha)
{
    return alpha * new_val + (1.0f - alpha) * old_avg;
}

/**
 * @brief Record value in history buffer
 *
 * WHAT: Add value to circular history buffer
 * WHY:  Track recent values for analysis
 * HOW:  Circular buffer with head tracking
 */
static void record_history(
    float* history,
    uint32_t* head,
    uint32_t* count,
    uint32_t max_size,
    float value
)
{
    if (!history || !head || !count) {
        return;
    }

    history[*head] = value;
    *head = (*head + 1) % max_size;
    if (*count < max_size) {
        (*count)++;
    }
}

/**
 * @brief Compute expected free energy for protective action
 *
 * WHAT: Estimate EFE for a potential protective action
 * WHY:  Active inference selects actions minimizing EFE
 * HOW:  Model expected FE reduction for each action type
 */
static float compute_action_efe(
    const sec_log_fep_bridge_t* bridge,
    sec_log_fep_action_t action,
    float current_fe
)
{
    /*
     * WHAT: Compute expected free energy for protective action
     * WHY:  Lower EFE means more effective at reducing uncertainty/threat
     * HOW:  Estimate reduction and cost/ambiguity for each action
     */

    float efe = current_fe;
    float expected_reduction = 0.0f;
    float ambiguity = 0.0f;

    switch (action) {
        case SEC_LOG_FEP_ACTION_NONE:
            expected_reduction = 0.0f;
            ambiguity = 0.0f;
            break;

        case SEC_LOG_FEP_ACTION_MONITOR:
            /* Monitoring clarifies state, low cost */
            expected_reduction = current_fe * 0.1f;
            ambiguity = 0.05f;
            break;

        case SEC_LOG_FEP_ACTION_PRESERVE:
            /* Preservation protects evidence */
            expected_reduction = current_fe * 0.15f;
            ambiguity = 0.1f;
            break;

        case SEC_LOG_FEP_ACTION_INCREASE_DETAIL:
            /* More detail reduces future uncertainty */
            expected_reduction = current_fe * 0.2f;
            ambiguity = 0.15f;
            break;

        case SEC_LOG_FEP_ACTION_BLOCK_SOURCE:
            /* Blocking is aggressive, high reduction but uncertainty */
            expected_reduction = current_fe * 0.5f;
            ambiguity = 0.4f;
            break;

        case SEC_LOG_FEP_ACTION_ALERT:
            /* Alerting enables external response */
            expected_reduction = current_fe * 0.25f;
            ambiguity = 0.2f;
            break;

        case SEC_LOG_FEP_ACTION_LOCKDOWN:
            /* Lockdown is maximum protection, high cost */
            expected_reduction = current_fe * 0.8f;
            ambiguity = 0.6f;
            break;

        case SEC_LOG_FEP_ACTION_ROTATE:
            /* Rotation preserves and starts fresh */
            expected_reduction = current_fe * 0.3f;
            ambiguity = 0.15f;
            break;

        default:
            expected_reduction = 0.0f;
            ambiguity = 1.0f;
            break;
    }

    /* Scale by precision (higher precision = more confident) */
    float precision_factor = bridge->state.current_precision / SEC_LOG_FEP_DEFAULT_PRECISION;
    expected_reduction *= precision_factor;

    /* EFE = expected FE after action + ambiguity */
    efe = (current_fe - expected_reduction) + ambiguity * bridge->config.surprise_threshold;

    return efe;
}

/**
 * @brief Check message for injection patterns
 *
 * WHAT: Scan message against known injection patterns
 * WHY:  Detect common attack signatures
 * HOW:  Pattern matching against known injection strings
 */
static sec_log_fep_inject_type_t check_injection_patterns(
    const sec_log_fep_bridge_t* bridge,
    const char* message,
    size_t message_len,
    char* matched_pattern,
    size_t pattern_buf_size
)
{
    if (!message || message_len == 0) {
        return SEC_LOG_FEP_INJECT_NONE;
    }

    /* Check for null byte injection */
    if (contains_null_byte(message, message_len)) {
        if (matched_pattern && pattern_buf_size > 0) {
            snprintf(matched_pattern, pattern_buf_size, "NULL_BYTE");
        }
        return SEC_LOG_FEP_INJECT_NULL_BYTE;
    }

    /* Check for newline injection */
    if (contains_newline_injection(message, message_len)) {
        if (matched_pattern && pattern_buf_size > 0) {
            snprintf(matched_pattern, pattern_buf_size, "NEWLINE (\\n or \\r)");
        }
        return SEC_LOG_FEP_INJECT_NEWLINE;
    }

    /* Check SQL injection patterns */
    if (bridge->config.detect_sql_injection) {
        for (const char** pattern = g_sql_patterns; *pattern != NULL; pattern++) {
            if (contains_pattern_icase(message, *pattern)) {
                if (matched_pattern && pattern_buf_size > 0) {
                    snprintf(matched_pattern, pattern_buf_size, "SQL: %s", *pattern);
                }
                return SEC_LOG_FEP_INJECT_SQL;
            }
        }
    }

    /* Check command injection patterns */
    if (bridge->config.detect_command_injection) {
        for (const char** pattern = g_command_patterns; *pattern != NULL; pattern++) {
            if (contains_pattern_icase(message, *pattern)) {
                if (matched_pattern && pattern_buf_size > 0) {
                    snprintf(matched_pattern, pattern_buf_size, "CMD: %s", *pattern);
                }
                return SEC_LOG_FEP_INJECT_COMMAND;
            }
        }
    }

    /* Check script injection patterns */
    if (bridge->config.detect_script_injection) {
        for (const char** pattern = g_script_patterns; *pattern != NULL; pattern++) {
            if (contains_pattern_icase(message, *pattern)) {
                if (matched_pattern && pattern_buf_size > 0) {
                    snprintf(matched_pattern, pattern_buf_size, "SCRIPT: %s", *pattern);
                }
                return SEC_LOG_FEP_INJECT_SCRIPT;
            }
        }
    }

    /* Check format string patterns */
    if (bridge->config.detect_format_string) {
        for (const char** pattern = g_format_patterns; *pattern != NULL; pattern++) {
            if (contains_pattern_icase(message, *pattern)) {
                if (matched_pattern && pattern_buf_size > 0) {
                    snprintf(matched_pattern, pattern_buf_size, "FMT: %s", *pattern);
                }
                return SEC_LOG_FEP_INJECT_FORMAT_STRING;
            }
        }
    }

    return SEC_LOG_FEP_INJECT_NONE;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int sec_log_fep_default_config(sec_log_fep_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* FEP parameters */
    config->anomaly_fe_threshold = SEC_LOG_FEP_SUSPICIOUS_THRESHOLD;
    config->surprise_threshold = SEC_LOG_FEP_SURPRISE_MEDIUM;
    config->precision_learning_rate = 0.05f;

    /* Detection parameters */
    config->use_fep_scoring = true;
    config->enable_precision_modulation = true;
    config->normal_fe_threshold = SEC_LOG_FEP_NORMAL_THRESHOLD;
    config->attack_fe_threshold = SEC_LOG_FEP_ATTACK_THRESHOLD;

    /* Injection detection - all enabled by default */
    config->detect_sql_injection = true;
    config->detect_command_injection = true;
    config->detect_script_injection = true;
    config->detect_format_string = true;

    /* Temporal analysis */
    config->enable_timestamp_analysis = true;
    config->timestamp_tolerance_ms = 1000.0f;  /* 1 second */
    config->enforce_monotonic_time = true;

    /* Sequence analysis */
    config->enable_sequence_analysis = true;
    config->max_sequence_gap = 10;

    /* Active inference protection */
    config->enable_active_protection = true;
    config->action_temperature = 1.0f;
    config->max_protective_actions = 3;

    /* Detection weights */
    config->injection_weight = 0.35f;
    config->deletion_weight = 0.25f;
    config->timestamp_weight = 0.20f;
    config->audit_weight = 0.20f;

    /* Learning parameters */
    config->enable_online_learning = true;
    config->learning_rate = 0.01f;
    config->learn_from_false_positives = true;

    /* Bio-async integration */
    config->enable_bio_async = false;

    return 0;
}

sec_log_fep_bridge_t* sec_log_fep_create(
    const sec_log_fep_config_t* config,
    security_logging_bridge_t* log_bridge,
    fep_system_t* fep_system
)
{
    /*
     * WHAT: Create and initialize the FEP-logging security bridge
     * WHY:  Enable free energy-based log tampering and injection detection
     * HOW:  Allocate bridge, initialize base, connect systems, allocate history
     */

    if (!log_bridge || !fep_system) {
        NIMCP_LOGGING_ERROR("sec_log_fep_create: NULL pointer for required system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_log_fep_create: required parameter is NULL (log_bridge, fep_system)");
        return NULL;
    }

    /* Allocate bridge */
    sec_log_fep_bridge_t* bridge = nimcp_malloc(sizeof(sec_log_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("sec_log_fep_create: failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sec_log_fep_create: bridge is NULL");
        return NULL;
    }

    memset(bridge, 0, sizeof(sec_log_fep_bridge_t));

    /* Initialize base */
    if (bridge_base_init(&bridge->base, BIO_MODULE_SECURITY_LOGGING_FEP,
                         "security_logging_fep") != 0) {
        NIMCP_LOGGING_ERROR("sec_log_fep_create: failed to initialize base");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sec_log_fep_create: operation failed");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        sec_log_fep_default_config(&bridge->config);
    }

    /* Connect systems */
    bridge->fep_system = fep_system;
    bridge->log_bridge = log_bridge;
    bridge->base.system_a = fep_system;
    bridge->base.system_a_connected = true;
    bridge->base.system_b = log_bridge;
    bridge->base.system_b_connected = true;
    bridge->base.bridge_active = true;

    /* Allocate history buffers */
    bridge->fe_history = nimcp_malloc(SEC_LOG_FEP_HISTORY_SIZE * sizeof(float));
    bridge->surprise_history = nimcp_malloc(SEC_LOG_FEP_HISTORY_SIZE * sizeof(float));

    if (!bridge->fe_history || !bridge->surprise_history) {
        NIMCP_LOGGING_ERROR("sec_log_fep_create: failed to allocate history buffers");
        if (bridge->fe_history) nimcp_free(bridge->fe_history);
        if (bridge->surprise_history) nimcp_free(bridge->surprise_history);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sec_log_fep_create: validation failed");
        return NULL;
    }

    memset(bridge->fe_history, 0, SEC_LOG_FEP_HISTORY_SIZE * sizeof(float));
    memset(bridge->surprise_history, 0, SEC_LOG_FEP_HISTORY_SIZE * sizeof(float));

    /* Initialize state */
    bridge->state.active = true;
    bridge->state.current_precision = SEC_LOG_FEP_DEFAULT_PRECISION;
    bridge->state.last_integrity = SEC_LOG_FEP_INTEGRITY_NORMAL;

    /* Initialize effects */
    bridge->fep_effects.integrity_level = SEC_LOG_FEP_INTEGRITY_NORMAL;
    bridge->fep_effects.log_integrity_score = 1.0f;
    bridge->fep_effects.temporal_integrity_score = 1.0f;
    bridge->fep_effects.sequence_integrity_score = 1.0f;

    NIMCP_LOGGING_INFO("Security logging FEP bridge created successfully");
    return bridge;
}

void sec_log_fep_destroy(sec_log_fep_bridge_t* bridge)
{
    /*
     * WHAT: Clean up and destroy the bridge
     * WHY:  Prevent memory leaks and ensure clean shutdown
     * HOW:  Disconnect bio-async, free buffers, cleanup base, free bridge
     */

    if (!bridge) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        sec_log_fep_disconnect_bio_async(bridge);
    }

    /* Free history buffers */
    if (bridge->fe_history) {
        nimcp_free(bridge->fe_history);
        bridge->fe_history = NULL;
    }

    if (bridge->surprise_history) {
        nimcp_free(bridge->surprise_history);
        bridge->surprise_history = NULL;
    }

    /* Cleanup base */
    bridge_base_cleanup(&bridge->base);

    /* Free bridge */
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("Security logging FEP bridge destroyed");
}

int sec_log_fep_reset(sec_log_fep_bridge_t* bridge)
{
    /*
     * WHAT: Reset bridge state while preserving connections
     * WHY:  Allow fresh start without reconnection
     * HOW:  Zero effects, statistics, history; reset precision
     */

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return -1;
    }

    BRIDGE_LOCK(bridge);

    /* Reset effects */
    memset(&bridge->fep_effects, 0, sizeof(sec_log_fep_effects_t));
    memset(&bridge->sec_effects, 0, sizeof(fep_security_log_effects_t));

    /* Reset state (preserve active flag) */
    bridge->state.update_count = 0;
    bridge->state.entries_analyzed = 0;
    bridge->state.detection_count = 0;
    bridge->state.protection_count = 0;
    bridge->state.current_precision = SEC_LOG_FEP_DEFAULT_PRECISION;
    bridge->state.precision_velocity = 0.0f;
    bridge->state.avg_free_energy = 0.0f;
    bridge->state.avg_surprise = 0.0f;
    bridge->state.avg_prediction_error = 0.0f;
    bridge->state.last_integrity = SEC_LOG_FEP_INTEGRITY_NORMAL;
    bridge->state.integrity_transitions = 0;
    bridge->state.last_timestamp_ns = 0;
    bridge->state.last_sequence_num = 0;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(sec_log_fep_stats_t));

    /* Reset history */
    if (bridge->fe_history) {
        memset(bridge->fe_history, 0, SEC_LOG_FEP_HISTORY_SIZE * sizeof(float));
    }
    if (bridge->surprise_history) {
        memset(bridge->surprise_history, 0, SEC_LOG_FEP_HISTORY_SIZE * sizeof(float));
    }
    bridge->history_head = 0;
    bridge->history_count = 0;
    bridge->fe_history_head = 0;
    bridge->fe_history_count = 0;
    bridge->surprise_history_head = 0;
    bridge->surprise_history_count = 0;

    /* Reset protection state */
    bridge->pending_action = SEC_LOG_FEP_ACTION_NONE;
    bridge->last_protection_time = 0;

    /* Reset base */
    bridge_base_reset_unlocked(&bridge->base);

    /* Restore integrity defaults */
    bridge->fep_effects.integrity_level = SEC_LOG_FEP_INTEGRITY_NORMAL;
    bridge->fep_effects.log_integrity_score = 1.0f;
    bridge->fep_effects.temporal_integrity_score = 1.0f;
    bridge->fep_effects.sequence_integrity_score = 1.0f;

    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Security logging FEP bridge reset");
    return 0;
}

/* ============================================================================
 * Configuration Implementation
 * ============================================================================ */

int sec_log_fep_get_config(
    const sec_log_fep_bridge_t* bridge,
    sec_log_fep_config_t* config
)
{
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_log_fep_get_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    *config = bridge->config;
    return 0;
}

int sec_log_fep_set_config(
    sec_log_fep_bridge_t* bridge,
    const sec_log_fep_config_t* config
)
{
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_log_fep_set_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    BRIDGE_LOCK(bridge);
    bridge->config = *config;
    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Security logging FEP bridge config updated");
    return 0;
}

/* ============================================================================
 * Compute and Update Implementation
 * ============================================================================ */

int sec_log_fep_compute_effects(sec_log_fep_bridge_t* bridge)
{
    /*
     * WHAT: Compute FEP-derived effects on security logging
     * WHY:  Free energy provides anomaly and integrity indicators
     * HOW:  Get FEP state, compute metrics, classify integrity
     */

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return -1;
    }

    if (!bridge->state.active) {
        return -1;
    }

    BRIDGE_LOCK(bridge);

    /* Get current FEP state */
    float current_fe = fep_get_free_energy(bridge->fep_system);
    float surprise = fep_compute_surprise(bridge->fep_system);
    float pred_error = fep_get_prediction_error(bridge->fep_system, 0);

    /* P1 fix: Use separate head/count per history buffer to prevent misalignment */
    record_history(bridge->fe_history, &bridge->fe_history_head, &bridge->fe_history_count,
                   SEC_LOG_FEP_HISTORY_SIZE, current_fe);
    record_history(bridge->surprise_history, &bridge->surprise_history_head, &bridge->surprise_history_count,
                   SEC_LOG_FEP_HISTORY_SIZE, surprise);

    /* Update running averages */
    bridge->state.avg_free_energy = update_running_avg(
        bridge->state.avg_free_energy, current_fe, RUNNING_AVG_ALPHA);
    bridge->state.avg_surprise = update_running_avg(
        bridge->state.avg_surprise, surprise, RUNNING_AVG_ALPHA);
    bridge->state.avg_prediction_error = update_running_avg(
        bridge->state.avg_prediction_error, pred_error, RUNNING_AVG_ALPHA);

    /* Compute anomaly score (normalized FE) */
    float anomaly_score = current_fe / (bridge->config.attack_fe_threshold + FEP_EPSILON);
    anomaly_score = nimcp_clampf(anomaly_score, 0.0f, 1.0f);
    bridge->fep_effects.fep_anomaly_score = anomaly_score;

    /* Compute log integrity (inverse of anomaly) */
    bridge->fep_effects.log_integrity_score = 1.0f - anomaly_score * bridge->config.injection_weight;

    /* Compute temporal integrity from prediction error */
    float temporal_score = 1.0f - (pred_error / (bridge->config.surprise_threshold + FEP_EPSILON)) *
                                   bridge->config.timestamp_weight;
    temporal_score = nimcp_clampf(temporal_score, 0.0f, 1.0f);
    bridge->fep_effects.temporal_integrity_score = temporal_score;

    /* Compute sequence integrity */
    bridge->fep_effects.sequence_integrity_score =
        1.0f - (bridge->sec_effects.sequence_gaps / (float)(bridge->state.entries_analyzed + 1));
    bridge->fep_effects.sequence_integrity_score =
        nimcp_clampf(bridge->fep_effects.sequence_integrity_score, 0.0f, 1.0f);

    /* Compute surprise scores */
    bridge->fep_effects.surprise_score = surprise / (bridge->config.surprise_threshold + FEP_EPSILON);
    bridge->fep_effects.surprise_score = nimcp_clampf(bridge->fep_effects.surprise_score, 0.0f, 1.0f);
    bridge->fep_effects.temporal_surprise = bridge->fep_effects.surprise_score *
                                             bridge->config.timestamp_weight;
    bridge->fep_effects.content_surprise = bridge->fep_effects.surprise_score *
                                            bridge->config.injection_weight;

    /* Precision-adjusted detection threshold */
    bridge->fep_effects.detection_sensitivity = bridge->state.current_precision;
    bridge->fep_effects.adjusted_anomaly_threshold =
        bridge->config.anomaly_fe_threshold / (bridge->state.current_precision + FEP_EPSILON);

    /* Classify integrity level */
    sec_log_fep_integrity_t new_integrity = classify_integrity(current_fe, &bridge->config);

    /* Track integrity transitions */
    if (new_integrity != bridge->state.last_integrity) {
        bridge->state.integrity_transitions++;
        bridge->state.last_integrity = new_integrity;
    }
    bridge->fep_effects.integrity_level = new_integrity;

    /* Update integrity statistics */
    switch (new_integrity) {
        case SEC_LOG_FEP_INTEGRITY_NORMAL:
            bridge->stats.normal_states++;
            break;
        case SEC_LOG_FEP_INTEGRITY_SUSPICIOUS:
            bridge->stats.suspicious_states++;
            break;
        case SEC_LOG_FEP_INTEGRITY_TAMPERED:
            bridge->stats.tampered_states++;
            break;
        case SEC_LOG_FEP_INTEGRITY_COMPROMISED:
            bridge->stats.compromised_states++;
            break;
        default:
            break;
    }

    /* Select recommended action via active inference (if enabled) */
    if (bridge->config.enable_active_protection &&
        new_integrity >= SEC_LOG_FEP_INTEGRITY_SUSPICIOUS) {

        sec_log_fep_action_t best_action = SEC_LOG_FEP_ACTION_NONE;
        float best_efe = current_fe;
        float action_confidence = 0.0f;

        /* Evaluate each action */
        for (int action = SEC_LOG_FEP_ACTION_NONE; action < SEC_LOG_FEP_ACTION_COUNT; action++) {
            float efe = compute_action_efe(bridge, (sec_log_fep_action_t)action, current_fe);
            if (efe < best_efe) {
                best_efe = efe;
                best_action = (sec_log_fep_action_t)action;
            }
        }

        /* Compute confidence in selected action */
        if (best_action != SEC_LOG_FEP_ACTION_NONE) {
            action_confidence = (current_fe - best_efe) / (current_fe + FEP_EPSILON);
            action_confidence = nimcp_clampf(action_confidence, 0.0f, 1.0f);
        }

        bridge->fep_effects.recommended_action = best_action;
        bridge->fep_effects.action_confidence = action_confidence;
    } else {
        bridge->fep_effects.recommended_action = SEC_LOG_FEP_ACTION_NONE;
        bridge->fep_effects.action_confidence = 0.0f;
    }

    /* Update statistics */
    bridge->stats.avg_free_energy = bridge->state.avg_free_energy;
    bridge->stats.avg_surprise = bridge->state.avg_surprise;
    if (current_fe > bridge->stats.max_free_energy) {
        bridge->stats.max_free_energy = current_fe;
    }
    if (surprise > bridge->stats.max_surprise) {
        bridge->stats.max_surprise = surprise;
    }

    bridge->state.update_count++;
    bridge_base_record_update(&bridge->base);

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int sec_log_fep_analyze_entry(
    sec_log_fep_bridge_t* bridge,
    const security_log_entry_t* entry,
    sec_log_fep_result_t* result
)
{
    /*
     * WHAT: Analyze a log entry for tampering using FEP
     * WHY:  Detect injection, manipulation, and anomalies in individual entries
     * HOW:  Combine pattern detection with FEP surprise analysis
     */

    if (!bridge || !entry || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_log_fep_analyze_entry: required parameter is NULL (bridge, entry, result)");
        return -1;
    }

    memset(result, 0, sizeof(sec_log_fep_result_t));

    BRIDGE_LOCK(bridge);

    bridge->state.entries_analyzed++;
    bridge->stats.total_entries_analyzed++;

    /* Get FEP metrics */
    float current_fe = fep_get_free_energy(bridge->fep_system);
    float surprise = fep_compute_surprise(bridge->fep_system);
    float pred_error = fep_get_prediction_error(bridge->fep_system, 0);

    result->free_energy = current_fe;
    result->surprise = surprise;
    result->prediction_error = pred_error;
    result->affected_entry_id = entry->entry_id;
    result->affected_timestamp_ns = entry->timestamp_ns;

    float detection_score = 0.0f;
    sec_log_fep_detection_t detection = SEC_LOG_FEP_DETECT_NONE;
    sec_log_fep_inject_type_t inject = SEC_LOG_FEP_INJECT_NONE;

    /* P1 fix: Use sizeof(entry->message) instead of strlen() so contains_null_byte()
     * can detect embedded null bytes (strlen stops at first null, defeating the check) */
    inject = check_injection_patterns(bridge, entry->message, sizeof(entry->message),
                                       result->pattern_matched, sizeof(result->pattern_matched));

    if (inject != SEC_LOG_FEP_INJECT_NONE) {
        detection = SEC_LOG_FEP_DETECT_INJECTION;
        detection_score = 0.8f;
        result->inject_type = inject;

        /* Update injection statistics */
        bridge->sec_effects.injections_detected++;
        switch (inject) {
            case SEC_LOG_FEP_INJECT_SQL:
                bridge->stats.sql_injections++;
                break;
            case SEC_LOG_FEP_INJECT_COMMAND:
                bridge->stats.command_injections++;
                break;
            case SEC_LOG_FEP_INJECT_SCRIPT:
                bridge->stats.script_injections++;
                break;
            case SEC_LOG_FEP_INJECT_FORMAT_STRING:
                bridge->stats.format_string_attacks++;
                break;
            case SEC_LOG_FEP_INJECT_NEWLINE:
                bridge->stats.newline_injections++;
                break;
            default:
                break;
        }
    }

    /* Check timestamp validity if enabled */
    if (bridge->config.enable_timestamp_analysis) {
        if (bridge->state.last_timestamp_ns > 0) {
            if (bridge->config.enforce_monotonic_time &&
                entry->timestamp_ns < bridge->state.last_timestamp_ns) {
                /* Non-monotonic timestamp */
                if (detection == SEC_LOG_FEP_DETECT_NONE) {
                    detection = SEC_LOG_FEP_DETECT_TIMESTAMP_MANIP;
                }
                detection_score = fmaxf(detection_score, 0.7f);
                bridge->sec_effects.timestamp_anomalies++;
                bridge->stats.timestamp_violations++;
            }
        }
        bridge->state.last_timestamp_ns = entry->timestamp_ns;
    }

    /* Check sequence if enabled */
    if (bridge->config.enable_sequence_analysis) {
        if (bridge->state.last_sequence_num > 0) {
            uint32_t gap = entry->sequence_number - bridge->state.last_sequence_num;
            if (gap > bridge->config.max_sequence_gap && gap < UINT32_MAX - 100) {
                /* Sequence gap detected */
                if (detection == SEC_LOG_FEP_DETECT_NONE) {
                    detection = SEC_LOG_FEP_DETECT_SEQUENCE_GAP;
                }
                detection_score = fmaxf(detection_score, 0.6f);
                bridge->sec_effects.sequence_gaps++;
            }
        }
        bridge->state.last_sequence_num = entry->sequence_number;
    }

    /* Compute FEP-based score */
    float fep_score = current_fe / (bridge->config.attack_fe_threshold + FEP_EPSILON);
    fep_score = nimcp_clampf(fep_score, 0.0f, 1.0f);

    /* Combine scores */
    if (bridge->config.use_fep_scoring) {
        result->anomaly_score = 0.4f * fep_score + 0.6f * detection_score;
    } else {
        result->anomaly_score = detection_score;
    }

    result->detection_type = detection;
    result->confidence = 1.0f - (pred_error / (bridge->config.surprise_threshold + FEP_EPSILON));
    result->confidence = nimcp_clampf(result->confidence, 0.0f, 1.0f);

    /* Classify integrity */
    result->integrity = classify_integrity(current_fe, &bridge->config);
    result->requires_action = (result->integrity >= SEC_LOG_FEP_INTEGRITY_SUSPICIOUS) ||
                              (result->anomaly_score > 0.5f);

    if (result->requires_action) {
        result->recommended_action = bridge->fep_effects.recommended_action;
    } else {
        result->recommended_action = SEC_LOG_FEP_ACTION_NONE;
    }

    /* Generate explanation */
    if (detection != SEC_LOG_FEP_DETECT_NONE) {
        snprintf(result->explanation, sizeof(result->explanation),
                 "Entry %lu: %s detected, score=%.2f, FE=%.2f",
                 (unsigned long)entry->entry_id,
                 sec_log_fep_detection_name(detection),
                 result->anomaly_score,
                 current_fe);
        bridge->state.detection_count++;
    } else {
        snprintf(result->explanation, sizeof(result->explanation),
                 "Entry %lu: normal, score=%.2f, FE=%.2f",
                 (unsigned long)entry->entry_id,
                 result->anomaly_score,
                 current_fe);
        bridge->sec_effects.entries_verified++;
    }

    /* Update FEP statistics */
    bridge->stats.fep_based_detections++;

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int sec_log_fep_detect_injection(
    sec_log_fep_bridge_t* bridge,
    const char* message,
    size_t message_len,
    sec_log_fep_result_t* result
)
{
    /*
     * WHAT: Specifically check message for injection attacks
     * WHY:  Allow targeted injection detection without full entry analysis
     * HOW:  Pattern matching + FEP-based scoring
     */

    if (!bridge || !message || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_log_fep_detect_injection: required parameter is NULL (bridge, message, result)");
        return -1;
    }

    memset(result, 0, sizeof(sec_log_fep_result_t));

    BRIDGE_LOCK(bridge);

    /* Check patterns */
    result->inject_type = check_injection_patterns(bridge, message, message_len,
                                                    result->pattern_matched,
                                                    sizeof(result->pattern_matched));

    if (result->inject_type != SEC_LOG_FEP_INJECT_NONE) {
        result->detection_type = SEC_LOG_FEP_DETECT_INJECTION;
        result->anomaly_score = 0.85f;
        result->confidence = 0.9f;

        bridge->sec_effects.injections_detected++;
        bridge->stats.injections_found++;

        snprintf(result->explanation, sizeof(result->explanation),
                 "Injection detected: %s, pattern: %s",
                 sec_log_fep_inject_type_name(result->inject_type),
                 result->pattern_matched);
    } else {
        result->detection_type = SEC_LOG_FEP_DETECT_NONE;
        result->anomaly_score = 0.0f;
        result->confidence = 1.0f;
        snprintf(result->explanation, sizeof(result->explanation),
                 "No injection patterns detected");
    }

    /* Get FEP state for context */
    result->free_energy = fep_get_free_energy(bridge->fep_system);
    result->surprise = fep_compute_surprise(bridge->fep_system);
    result->prediction_error = fep_get_prediction_error(bridge->fep_system, 0);

    result->integrity = classify_integrity(result->free_energy, &bridge->config);
    result->requires_action = (result->inject_type != SEC_LOG_FEP_INJECT_NONE);
    result->recommended_action = result->requires_action ?
                                  SEC_LOG_FEP_ACTION_BLOCK_SOURCE : SEC_LOG_FEP_ACTION_NONE;

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int sec_log_fep_detect_deletion(
    sec_log_fep_bridge_t* bridge,
    uint32_t expected_seq,
    uint32_t actual_seq,
    sec_log_fep_result_t* result
)
{
    /*
     * WHAT: Detect log deletion/truncation via sequence gap
     * WHY:  Attackers may delete incriminating log entries
     * HOW:  Compare expected vs actual sequence numbers
     */

    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_log_fep_detect_deletion: required parameter is NULL (bridge, result)");
        return -1;
    }

    memset(result, 0, sizeof(sec_log_fep_result_t));

    BRIDGE_LOCK(bridge);

    uint32_t gap = actual_seq - expected_seq;

    if (gap > 0 && gap < UINT32_MAX - 100) {
        result->detection_type = SEC_LOG_FEP_DETECT_DELETION;
        result->anomaly_score = nimcp_clampf((float)gap / 100.0f, 0.3f, 1.0f);
        result->confidence = 0.85f;

        bridge->sec_effects.deletions_detected += gap;
        bridge->stats.deletions_found += gap;

        snprintf(result->explanation, sizeof(result->explanation),
                 "Deletion detected: %u entries missing (expected %u, got %u)",
                 gap, expected_seq, actual_seq);
    } else {
        result->detection_type = SEC_LOG_FEP_DETECT_NONE;
        result->anomaly_score = 0.0f;
        result->confidence = 1.0f;
        snprintf(result->explanation, sizeof(result->explanation),
                 "Sequence valid: expected %u, got %u",
                 expected_seq, actual_seq);
    }

    result->free_energy = fep_get_free_energy(bridge->fep_system);
    result->surprise = fep_compute_surprise(bridge->fep_system);
    result->prediction_error = fep_get_prediction_error(bridge->fep_system, 0);

    result->integrity = classify_integrity(result->free_energy, &bridge->config);
    result->requires_action = (gap > bridge->config.max_sequence_gap);
    result->recommended_action = result->requires_action ?
                                  SEC_LOG_FEP_ACTION_ALERT : SEC_LOG_FEP_ACTION_NONE;

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int sec_log_fep_detect_timestamp_manipulation(
    sec_log_fep_bridge_t* bridge,
    uint64_t timestamp_ns,
    sec_log_fep_result_t* result
)
{
    /*
     * WHAT: Detect timestamp manipulation (non-monotonic, implausible)
     * WHY:  Attackers may backdate logs to hide activity
     * HOW:  Check monotonicity and drift from expected time
     */

    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_log_fep_detect_timestamp_manipulation: required parameter is NULL (bridge, result)");
        return -1;
    }

    memset(result, 0, sizeof(sec_log_fep_result_t));

    BRIDGE_LOCK(bridge);

    result->affected_timestamp_ns = timestamp_ns;
    bool violation = false;

    /* Check monotonicity */
    if (bridge->config.enforce_monotonic_time &&
        bridge->state.last_timestamp_ns > 0 &&
        timestamp_ns < bridge->state.last_timestamp_ns) {
        violation = true;
        result->detection_type = SEC_LOG_FEP_DETECT_TIMESTAMP_MANIP;
        result->anomaly_score = 0.8f;

        snprintf(result->explanation, sizeof(result->explanation),
                 "Non-monotonic timestamp: %lu < previous %lu",
                 (unsigned long)timestamp_ns,
                 (unsigned long)bridge->state.last_timestamp_ns);
    }

    /* Check drift from wall clock (if we have a reference) */
    uint64_t now_ns = nimcp_time_monotonic_us() * 1000;
    uint64_t tolerance_ns = (uint64_t)(bridge->config.timestamp_tolerance_ms * 1000000.0f);

    if (timestamp_ns > now_ns + tolerance_ns) {
        /* Future timestamp */
        violation = true;
        result->detection_type = SEC_LOG_FEP_DETECT_TIMESTAMP_MANIP;
        result->anomaly_score = fmaxf(result->anomaly_score, 0.9f);

        snprintf(result->explanation, sizeof(result->explanation),
                 "Future timestamp detected: %lu > now + tolerance",
                 (unsigned long)timestamp_ns);
    }

    if (violation) {
        result->confidence = 0.9f;
        bridge->sec_effects.timestamp_anomalies++;
        bridge->stats.timestamp_violations++;
    } else {
        result->detection_type = SEC_LOG_FEP_DETECT_NONE;
        result->anomaly_score = 0.0f;
        result->confidence = 1.0f;
        snprintf(result->explanation, sizeof(result->explanation),
                 "Timestamp valid: %lu", (unsigned long)timestamp_ns);
    }

    bridge->state.last_timestamp_ns = timestamp_ns;

    result->free_energy = fep_get_free_energy(bridge->fep_system);
    result->surprise = fep_compute_surprise(bridge->fep_system);
    result->prediction_error = fep_get_prediction_error(bridge->fep_system, 0);

    result->integrity = classify_integrity(result->free_energy, &bridge->config);
    result->requires_action = violation;
    result->recommended_action = violation ? SEC_LOG_FEP_ACTION_PRESERVE : SEC_LOG_FEP_ACTION_NONE;

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int sec_log_fep_verify_audit_trail(
    sec_log_fep_bridge_t* bridge,
    uint64_t expected_hash,
    uint64_t actual_hash,
    sec_log_fep_result_t* result
)
{
    /*
     * WHAT: Verify audit trail hash chain integrity
     * WHY:  Detect tampering with historical logs
     * HOW:  Compare expected vs actual hash values
     */

    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_log_fep_verify_audit_trail: required parameter is NULL (bridge, result)");
        return -1;
    }

    memset(result, 0, sizeof(sec_log_fep_result_t));

    BRIDGE_LOCK(bridge);

    if (expected_hash != actual_hash) {
        result->detection_type = SEC_LOG_FEP_DETECT_AUDIT_CORRUPT;
        result->anomaly_score = 0.95f;
        result->confidence = 0.99f;

        bridge->sec_effects.audit_corruptions++;
        bridge->stats.audit_violations++;

        snprintf(result->explanation, sizeof(result->explanation),
                 "Audit trail corruption: expected hash %016lx, got %016lx",
                 (unsigned long)expected_hash,
                 (unsigned long)actual_hash);

        result->requires_action = true;
        result->recommended_action = SEC_LOG_FEP_ACTION_LOCKDOWN;
    } else {
        result->detection_type = SEC_LOG_FEP_DETECT_NONE;
        result->anomaly_score = 0.0f;
        result->confidence = 1.0f;

        snprintf(result->explanation, sizeof(result->explanation),
                 "Audit trail verified: hash %016lx",
                 (unsigned long)actual_hash);

        result->requires_action = false;
        result->recommended_action = SEC_LOG_FEP_ACTION_NONE;
    }

    result->free_energy = fep_get_free_energy(bridge->fep_system);
    result->surprise = fep_compute_surprise(bridge->fep_system);
    result->prediction_error = fep_get_prediction_error(bridge->fep_system, 0);
    result->integrity = classify_integrity(result->free_energy, &bridge->config);

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int sec_log_fep_update_from_detection(
    sec_log_fep_bridge_t* bridge,
    sec_log_fep_detection_t detection,
    float severity,
    uint64_t entry_id
)
{
    /*
     * WHAT: Update FEP state from detection event
     * WHY:  Security events inform FEP generative model
     * HOW:  Convert detection to observation, update beliefs/precision
     */

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return -1;
    }

    (void)entry_id; /* May be used for entry-specific updates */

    BRIDGE_LOCK(bridge);

    bridge->state.detection_count++;

    /* Update security effects based on detection type */
    switch (detection) {
        case SEC_LOG_FEP_DETECT_INJECTION:
            bridge->sec_effects.injections_detected++;
            bridge->stats.injections_found++;
            break;

        case SEC_LOG_FEP_DETECT_DELETION:
            bridge->sec_effects.deletions_detected++;
            bridge->stats.deletions_found++;
            break;

        case SEC_LOG_FEP_DETECT_TIMESTAMP_MANIP:
            bridge->sec_effects.timestamp_anomalies++;
            bridge->stats.timestamp_violations++;
            break;

        case SEC_LOG_FEP_DETECT_AUDIT_CORRUPT:
            bridge->sec_effects.audit_corruptions++;
            bridge->stats.audit_violations++;
            break;

        case SEC_LOG_FEP_DETECT_SEQUENCE_GAP:
            bridge->sec_effects.sequence_gaps++;
            break;

        case SEC_LOG_FEP_DETECT_NONE:
            bridge->sec_effects.entries_verified++;
            break;

        default:
            break;
    }

    /* Update running averages */
    bridge->sec_effects.avg_entry_anomaly = update_running_avg(
        bridge->sec_effects.avg_entry_anomaly, severity, RUNNING_AVG_ALPHA);

    /* Update FEP if online learning is enabled */
    if (bridge->config.enable_online_learning && detection != SEC_LOG_FEP_DETECT_NONE) {
        /* Detection events are high-surprise observations */
        if (severity > 0.5f) {
            fep_update_precision(bridge->fep_system);
        }
    }

    /* Track attack state */
    if (severity > 0.7f) {
        bridge->sec_effects.attack_in_progress = true;
        bridge->sec_effects.current_attack = detection;
        bridge->sec_effects.attack_severity = severity;
    } else if (severity < 0.2f) {
        bridge->sec_effects.attack_in_progress = false;
        bridge->sec_effects.attack_severity = 0.0f;
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int sec_log_fep_apply_precision_modulation(sec_log_fep_bridge_t* bridge)
{
    /*
     * WHAT: Adapt detection precision based on FEP state
     * WHY:  Higher precision = more sensitive detection when needed
     * HOW:  Adjust precision based on integrity level and detection rate
     */

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return -1;
    }

    if (!bridge->config.enable_precision_modulation) {
        return 0;
    }

    BRIDGE_LOCK(bridge);

    /* Compute target precision based on integrity level */
    float target_precision = SEC_LOG_FEP_DEFAULT_PRECISION;

    switch (bridge->fep_effects.integrity_level) {
        case SEC_LOG_FEP_INTEGRITY_NORMAL:
            target_precision = SEC_LOG_FEP_DEFAULT_PRECISION;
            break;

        case SEC_LOG_FEP_INTEGRITY_SUSPICIOUS:
            target_precision = SEC_LOG_FEP_DEFAULT_PRECISION * 2.0f;
            break;

        case SEC_LOG_FEP_INTEGRITY_TAMPERED:
            target_precision = SEC_LOG_FEP_DEFAULT_PRECISION * 4.0f;
            break;

        case SEC_LOG_FEP_INTEGRITY_COMPROMISED:
            target_precision = SEC_LOG_FEP_MAX_PRECISION;
            break;

        default:
            break;
    }

    /* Clamp target precision */
    target_precision = nimcp_clampf(target_precision,
                                    SEC_LOG_FEP_MIN_PRECISION,
                                    SEC_LOG_FEP_MAX_PRECISION);

    /* Smooth adaptation */
    float old_precision = bridge->state.current_precision;
    float alpha = bridge->config.precision_learning_rate;
    bridge->state.current_precision =
        (1.0f - alpha) * old_precision + alpha * target_precision;

    /* Track precision velocity */
    bridge->state.precision_velocity = bridge->state.current_precision - old_precision;

    /* Update statistics */
    bridge->stats.precision_adaptations++;
    bridge->stats.current_precision = bridge->state.current_precision;

    BRIDGE_UNLOCK(bridge);
    return 0;
}

/* ============================================================================
 * Active Inference Protection Implementation
 * ============================================================================ */

int sec_log_fep_select_protection(
    sec_log_fep_bridge_t* bridge,
    sec_log_fep_action_t* action_out,
    float* confidence_out
)
{
    /*
     * WHAT: Select protective action via active inference
     * WHY:  Choose action minimizing expected free energy
     * HOW:  Evaluate each action's EFE, select via softmax
     */

    if (!bridge || !action_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_log_fep_select_protection: required parameter is NULL (bridge, action_out)");
        return -1;
    }

    BRIDGE_LOCK(bridge);

    float current_fe = bridge->state.avg_free_energy;

    /* If integrity is normal, no action needed */
    if (bridge->fep_effects.integrity_level == SEC_LOG_FEP_INTEGRITY_NORMAL) {
        *action_out = SEC_LOG_FEP_ACTION_NONE;
        if (confidence_out) *confidence_out = 1.0f;
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    /* Compute EFE for each action */
    float efe_values[SEC_LOG_FEP_ACTION_COUNT];
    float action_probs[SEC_LOG_FEP_ACTION_COUNT];

    for (int i = 0; i < SEC_LOG_FEP_ACTION_COUNT; i++) {
        efe_values[i] = -compute_action_efe(bridge, (sec_log_fep_action_t)i, current_fe);
    }

    /* Apply softmax to get action probabilities */
    softmax_actions(efe_values, action_probs, SEC_LOG_FEP_ACTION_COUNT,
                    bridge->config.action_temperature);

    /* Select action with highest probability */
    float max_prob = 0.0f;
    sec_log_fep_action_t best_action = SEC_LOG_FEP_ACTION_NONE;

    for (int i = 0; i < SEC_LOG_FEP_ACTION_COUNT; i++) {
        if (action_probs[i] > max_prob) {
            max_prob = action_probs[i];
            best_action = (sec_log_fep_action_t)i;
        }
    }

    *action_out = best_action;
    if (confidence_out) {
        *confidence_out = max_prob;
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int sec_log_fep_execute_protection(
    sec_log_fep_bridge_t* bridge,
    sec_log_fep_action_t action,
    uint64_t target_entry_id
)
{
    /*
     * WHAT: Execute the selected protective action
     * WHY:  Reduce free energy by protecting log integrity
     * HOW:  Dispatch action to logging system
     */

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return -1;
    }

    (void)target_entry_id;  /* May be used for targeted actions */

    BRIDGE_LOCK(bridge);

    uint64_t now = get_timestamp_ms();

    /* Rate limit protections */
    if (now - bridge->last_protection_time < MIN_PROTECTION_INTERVAL_MS) {
        BRIDGE_UNLOCK(bridge);
        return -1;  /* Rate limited */
    }

    int result = 0;

    switch (action) {
        case SEC_LOG_FEP_ACTION_NONE:
            break;

        case SEC_LOG_FEP_ACTION_MONITOR:
            /* Flag for increased monitoring */
            bridge->pending_action = action;
            break;

        case SEC_LOG_FEP_ACTION_PRESERVE:
            /* Force log flush/snapshot */
            result = security_logging_flush(bridge->log_bridge);
            break;

        case SEC_LOG_FEP_ACTION_INCREASE_DETAIL:
            /* Would increase log verbosity */
            bridge->pending_action = action;
            break;

        case SEC_LOG_FEP_ACTION_BLOCK_SOURCE:
            /* Would block source - tracked for now */
            bridge->pending_action = action;
            break;

        case SEC_LOG_FEP_ACTION_ALERT:
            /* Log critical alert */
            security_logging_log_audit(
                bridge->log_bridge,
                SECURITY_LOG_SEV_ALERT,
                "FEP_BRIDGE",
                "Security logging integrity alert triggered",
                "Active inference protection activated"
            );
            break;

        case SEC_LOG_FEP_ACTION_LOCKDOWN:
            /* Force log rotation and increase security */
            result = security_logging_rotate(bridge->log_bridge);
            break;

        case SEC_LOG_FEP_ACTION_ROTATE:
            result = security_logging_rotate(bridge->log_bridge);
            break;

        default:
            result = -1;
            break;
    }

    if (result == 0) {
        bridge->state.protection_count++;
        bridge->stats.protections_attempted++;
        bridge->last_protection_time = now;
    }

    BRIDGE_UNLOCK(bridge);
    return result;
}

int sec_log_fep_report_protection(
    sec_log_fep_bridge_t* bridge,
    sec_log_fep_action_t action,
    bool success,
    float fe_reduction
)
{
    /*
     * WHAT: Report protection outcome for learning
     * WHY:  Update FEP from action outcomes
     * HOW:  Adjust precision based on success
     */

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return -1;
    }

    (void)action;

    BRIDGE_LOCK(bridge);

    if (success) {
        bridge->stats.protections_successful++;

        bridge->stats.avg_protection_fe_reduction = update_running_avg(
            bridge->stats.avg_protection_fe_reduction, fe_reduction, RUNNING_AVG_ALPHA);

        if (bridge->config.enable_online_learning) {
            bridge->state.current_precision *= 1.02f;
            bridge->state.current_precision = nimcp_clampf(
                bridge->state.current_precision,
                SEC_LOG_FEP_MIN_PRECISION,
                SEC_LOG_FEP_MAX_PRECISION);
        }
    } else {
        if (bridge->config.enable_online_learning) {
            bridge->state.current_precision *= 0.95f;
            bridge->state.current_precision = nimcp_clampf(
                bridge->state.current_precision,
                SEC_LOG_FEP_MIN_PRECISION,
                SEC_LOG_FEP_MAX_PRECISION);
        }
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int sec_log_fep_get_effects(
    const sec_log_fep_bridge_t* bridge,
    sec_log_fep_effects_t* effects
)
{
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_log_fep_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    *effects = bridge->fep_effects;
    return 0;
}

int sec_log_fep_get_security_effects(
    const sec_log_fep_bridge_t* bridge,
    fep_security_log_effects_t* effects
)
{
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_log_fep_get_security_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    *effects = bridge->sec_effects;
    return 0;
}

int sec_log_fep_get_stats(
    const sec_log_fep_bridge_t* bridge,
    sec_log_fep_stats_t* stats
)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_log_fep_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

float sec_log_fep_get_anomaly_score(const sec_log_fep_bridge_t* bridge)
{
    if (!bridge) {
        return -1.0f;
    }

    return bridge->fep_effects.fep_anomaly_score;
}

sec_log_fep_integrity_t sec_log_fep_get_integrity(
    const sec_log_fep_bridge_t* bridge
)
{
    if (!bridge) {
        return (sec_log_fep_integrity_t)-1;
    }

    return bridge->fep_effects.integrity_level;
}

float sec_log_fep_get_free_energy(const sec_log_fep_bridge_t* bridge)
{
    if (!bridge) {
        return -1.0f;
    }

    return bridge->state.avg_free_energy;
}

int sec_log_fep_reset_stats(sec_log_fep_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    BRIDGE_LOCK(bridge);
    memset(&bridge->stats, 0, sizeof(sec_log_fep_stats_t));
    bridge->stats.current_precision = bridge->state.current_precision;
    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

int sec_log_fep_connect_bio_async(sec_log_fep_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SECURITY_LOGGING_FEP,
        .module_name = "security_logging_fep",
        .inbox_capacity = 64,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Security logging FEP bridge connected to bio-async");
    }

    return 0;
}

int sec_log_fep_disconnect_bio_async(sec_log_fep_bridge_t* bridge)
{
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Security logging FEP bridge disconnected from bio-async");
    return 0;
}

bool sec_log_fep_is_bio_async_connected(const sec_log_fep_bridge_t* bridge)
{
    return bridge ? bridge->base.bio_async_enabled : false;
}

uint32_t sec_log_fep_process_inbox(
    sec_log_fep_bridge_t* bridge,
    uint32_t max_messages
)
{
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    return bio_router_process_inbox(bridge->base.bio_ctx, max_messages);
}

/* ============================================================================
 * Utility Implementation
 * ============================================================================ */

const char* sec_log_fep_integrity_name(sec_log_fep_integrity_t level)
{
    if (level >= 0 && level < SEC_LOG_FEP_INTEGRITY_COUNT) {
        return g_integrity_names[level];
    }
    return "UNKNOWN";
}

const char* sec_log_fep_action_name(sec_log_fep_action_t action)
{
    if (action >= 0 && action < SEC_LOG_FEP_ACTION_COUNT) {
        return g_action_names[action];
    }
    return "UNKNOWN";
}

const char* sec_log_fep_detection_name(sec_log_fep_detection_t detection)
{
    if (detection >= 0 && detection < SEC_LOG_FEP_DETECT_COUNT) {
        return g_detection_names[detection];
    }
    return "UNKNOWN";
}

const char* sec_log_fep_inject_type_name(sec_log_fep_inject_type_t inject_type)
{
    if (inject_type >= 0 && inject_type < SEC_LOG_FEP_INJECT_COUNT) {
        return g_inject_type_names[inject_type];
    }
    return "UNKNOWN";
}

void sec_log_fep_print_summary(const sec_log_fep_bridge_t* bridge)
{
    if (!bridge) {
        printf("Security Logging FEP Bridge: NULL\n");
        return;
    }

    printf("\n========================================\n");
    printf("Security Logging FEP Bridge Summary\n");
    printf("========================================\n");
    printf("Active: %s\n", bridge->state.active ? "Yes" : "No");
    printf("Updates: %lu\n", (unsigned long)bridge->state.update_count);
    printf("Entries Analyzed: %lu\n", (unsigned long)bridge->state.entries_analyzed);
    printf("Detections: %lu\n", (unsigned long)bridge->state.detection_count);
    printf("Protections: %lu\n", (unsigned long)bridge->state.protection_count);
    printf("\nFEP State:\n");
    printf("  Free Energy (avg): %.4f\n", bridge->state.avg_free_energy);
    printf("  Surprise (avg): %.4f\n", bridge->state.avg_surprise);
    printf("  Precision: %.4f\n", bridge->state.current_precision);
    printf("\nIntegrity:\n");
    printf("  Level: %s\n", sec_log_fep_integrity_name(bridge->fep_effects.integrity_level));
    printf("  Anomaly Score: %.4f\n", bridge->fep_effects.fep_anomaly_score);
    printf("  Log Integrity: %.4f\n", bridge->fep_effects.log_integrity_score);
    printf("  Temporal Integrity: %.4f\n", bridge->fep_effects.temporal_integrity_score);
    printf("  Sequence Integrity: %.4f\n", bridge->fep_effects.sequence_integrity_score);
    printf("\nDetection Breakdown:\n");
    printf("  Injections: %lu\n", (unsigned long)bridge->sec_effects.injections_detected);
    printf("  Deletions: %lu\n", (unsigned long)bridge->sec_effects.deletions_detected);
    printf("  Timestamp Anomalies: %lu\n", (unsigned long)bridge->sec_effects.timestamp_anomalies);
    printf("  Audit Corruptions: %lu\n", (unsigned long)bridge->sec_effects.audit_corruptions);
    printf("\nRecommended Action: %s (conf: %.2f)\n",
           sec_log_fep_action_name(bridge->fep_effects.recommended_action),
           bridge->fep_effects.action_confidence);
    printf("Bio-Async: %s\n", bridge->base.bio_async_enabled ? "Connected" : "Not connected");
    printf("========================================\n\n");
}

void sec_log_fep_print_stats(const sec_log_fep_stats_t* stats)
{
    if (!stats) {
        printf("Statistics: NULL\n");
        return;
    }

    printf("\n========================================\n");
    printf("Security Logging FEP Statistics\n");
    printf("========================================\n");
    printf("Detection Stats:\n");
    printf("  Total Entries Analyzed: %lu\n", (unsigned long)stats->total_entries_analyzed);
    printf("  FEP-based Detections: %lu\n", (unsigned long)stats->fep_based_detections);
    printf("  Injections Found: %lu\n", (unsigned long)stats->injections_found);
    printf("  Deletions Found: %lu\n", (unsigned long)stats->deletions_found);
    printf("  Timestamp Violations: %lu\n", (unsigned long)stats->timestamp_violations);
    printf("  Audit Violations: %lu\n", (unsigned long)stats->audit_violations);
    printf("  False Positives: %lu\n", (unsigned long)stats->false_positives);
    printf("\nInjection Breakdown:\n");
    printf("  SQL: %lu\n", (unsigned long)stats->sql_injections);
    printf("  Command: %lu\n", (unsigned long)stats->command_injections);
    printf("  Script: %lu\n", (unsigned long)stats->script_injections);
    printf("  Format String: %lu\n", (unsigned long)stats->format_string_attacks);
    printf("  Newline: %lu\n", (unsigned long)stats->newline_injections);
    printf("\nFEP Metrics:\n");
    printf("  Avg Free Energy: %.4f\n", stats->avg_free_energy);
    printf("  Max Free Energy: %.4f\n", stats->max_free_energy);
    printf("  Avg Surprise: %.4f\n", stats->avg_surprise);
    printf("  Max Surprise: %.4f\n", stats->max_surprise);
    printf("\nPrecision:\n");
    printf("  Current: %.4f\n", stats->current_precision);
    printf("  Adaptations: %lu\n", (unsigned long)stats->precision_adaptations);
    printf("\nProtection Stats:\n");
    printf("  Attempted: %lu\n", (unsigned long)stats->protections_attempted);
    printf("  Successful: %lu\n", (unsigned long)stats->protections_successful);
    printf("  Avg FE Reduction: %.4f\n", stats->avg_protection_fe_reduction);
    printf("\nIntegrity State Distribution:\n");
    printf("  Normal: %lu\n", (unsigned long)stats->normal_states);
    printf("  Suspicious: %lu\n", (unsigned long)stats->suspicious_states);
    printf("  Tampered: %lu\n", (unsigned long)stats->tampered_states);
    printf("  Compromised: %lu\n", (unsigned long)stats->compromised_states);
    printf("========================================\n\n");
}
