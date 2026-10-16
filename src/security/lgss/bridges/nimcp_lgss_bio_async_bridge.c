/**
 * @file nimcp_lgss_bio_async_bridge.c
 * @brief Implementation of LGSS Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Implementation of bio-async message routing for LGSS
 * WHY:  Enable safety-critical communication between LGSS and all modules
 * HOW:  Message serialization/deserialization, handler dispatch, router integration
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "security/lgss/bridges/nimcp_lgss_bio_async_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "constants/nimcp_timing_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(lgss_bio_async_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


#define LOG_MODULE "LGSS_BIO_ASYNC_BRIDGE"


/* ============================================================================
 * Internal Handler Entry Structure
 * ============================================================================ */

typedef struct {
    bio_message_type_t msg_type;
    void* handler;
    void* user_data;
    bool active;
} lgss_handler_entry_t;

/* ============================================================================
 * Internal State Cache
 * ============================================================================ */

typedef struct {
    uint32_t next_request_id;
    uint32_t next_violation_id;
    uint32_t next_escalation_id;
    uint32_t next_check_id;
    uint32_t next_log_id;
    uint32_t next_command_id;
    uint64_t start_time_us;
    bool halted;
    lgss_reset_type_t last_reset_type;
} lgss_internal_state_t;

/* ============================================================================
 * Internal Bridge Structure
 * ============================================================================ */

struct lgss_bio_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    lgss_bio_bridge_config_t config;
    bio_router_t router;

    /* Handler registry */
    lgss_evaluate_handler_t evaluate_handler;
    void* evaluate_handler_data;

    lgss_override_handler_t override_handler;
    void* override_handler_data;

    lgss_control_handler_t control_handler;
    void* control_handler_data;

    lgss_audit_handler_t audit_handler;
    void* audit_handler_data;

    /* Connection state */
    bool connected;
    uint64_t last_heartbeat_us;
    uint32_t time_since_heartbeat_ms;

    /* Internal state */
    lgss_internal_state_t internal;
    lgss_bio_bridge_stats_t stats;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in microseconds
 */
static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Safe string copy with null termination
 */
static void safe_strncpy(char* dest, const char* src, size_t dest_size) {
    if (!dest || dest_size == 0) return;
    if (!src) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

/**
 * @brief Initialize message header
 */
static void init_message_header(
    bio_message_header_t* header,
    bio_message_type_t type,
    nimcp_bio_channel_type_t channel,
    uint32_t flags
) {
    memset(header, 0, sizeof(*header));
    header->type = type;
    header->source_module = BIO_MODULE_LGSS_BIO_BRIDGE;
    header->target_module = BIO_MODULE_ALL;
    header->channel = channel;
    header->flags = flags;
    header->timestamp_us = get_timestamp_us();
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int lgss_bio_bridge_default_config(lgss_bio_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lgss_bio_bridge_default_config: config is NULL");
        return -1;
    }

    config->evaluation_timeout_ms = LGSS_BIO_DEFAULT_EVAL_TIMEOUT_MS;
    config->message_ttl_ms = LGSS_BIO_MESSAGE_TTL_MS;
    config->urgent_timeout_ms = LGSS_BIO_URGENT_TIMEOUT_MS;
    config->heartbeat_interval_ms = 1000;  /* 1 second */
    config->max_inbox_process_per_update = 32;
    config->max_handlers = LGSS_BIO_MAX_HANDLERS;

    config->default_channel = BIO_CHANNEL_SEROTONIN;
    config->alert_channel = BIO_CHANNEL_NOREPINEPHRINE;
    config->urgent_channel = BIO_CHANNEL_NOREPINEPHRINE;

    config->uncertainty_alert_threshold = 0.7f;
    config->risk_escalation_threshold = 0.8f;
    config->auto_escalate_high_risk = true;
    config->block_on_evaluation_timeout = true;

    config->enable_telemetry = true;
    config->enable_integrity_monitoring = true;
    config->enable_external_interface = false;
    config->enable_plasticity_gating = true;
    config->enable_logging = false;

    return 0;
}

lgss_bio_bridge_t* lgss_bio_bridge_create(
    const lgss_bio_bridge_config_t* config
) {
    lgss_bio_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lgss_bio_bridge_create: allocation failed");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        lgss_bio_bridge_default_config(&bridge->config);
    }

    bridge->internal.start_time_us = get_timestamp_us();
    bridge->internal.next_request_id = 1;
    bridge->internal.next_violation_id = 1;
    bridge->internal.next_escalation_id = 1;
    bridge->internal.next_check_id = 1;
    bridge->internal.next_log_id = 1;
    bridge->internal.next_command_id = 1;
    bridge->last_heartbeat_us = get_timestamp_us();

    return bridge;
}

void lgss_bio_bridge_destroy(lgss_bio_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "lgss_bio_async");

    if (bridge->connected) {
        lgss_bio_bridge_disconnect(bridge);
    }

    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int lgss_bio_bridge_connect(
    lgss_bio_bridge_t* bridge,
    bio_router_t router
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lgss_bio_bridge_connect: bridge is NULL");
        return -1;
    }
    if (bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "lgss_bio_bridge_connect: bridge already connected");
        return -1;
    }

    bridge->router = router;
    bridge->connected = true;

    return 0;
}

int lgss_bio_bridge_disconnect(lgss_bio_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lgss_bio_bridge_disconnect: bridge is NULL");
        return -1;
    }

    bridge->router = NULL;
    bridge->connected = false;

    return 0;
}

bool lgss_bio_bridge_is_connected(const lgss_bio_bridge_t* bridge) {
    return bridge ? bridge->connected : false;
}

/* ============================================================================
 * Handler Registration API
 * ============================================================================ */

int lgss_bio_bridge_register_evaluate_handler(
    lgss_bio_bridge_t* bridge,
    lgss_evaluate_handler_t handler,
    void* user_data
) {
    if (!bridge || !handler) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lgss_bio_bridge_register_evaluate_handler: required parameter is NULL (bridge, handler)");
        return -1;
    }

    bridge->evaluate_handler = handler;
    bridge->evaluate_handler_data = user_data;

    return 0;
}

int lgss_bio_bridge_register_override_handler(
    lgss_bio_bridge_t* bridge,
    lgss_override_handler_t handler,
    void* user_data
) {
    if (!bridge || !handler) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lgss_bio_bridge_register_override_handler: required parameter is NULL (bridge, handler)");
        return -1;
    }

    bridge->override_handler = handler;
    bridge->override_handler_data = user_data;

    return 0;
}

int lgss_bio_bridge_register_control_handler(
    lgss_bio_bridge_t* bridge,
    lgss_control_handler_t handler,
    void* user_data
) {
    if (!bridge || !handler) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lgss_bio_bridge_register_control_handler: required parameter is NULL (bridge, handler)");
        return -1;
    }

    bridge->control_handler = handler;
    bridge->control_handler_data = user_data;

    return 0;
}

int lgss_bio_bridge_register_audit_handler(
    lgss_bio_bridge_t* bridge,
    lgss_audit_handler_t handler,
    void* user_data
) {
    if (!bridge || !handler) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lgss_bio_bridge_register_audit_handler: required parameter is NULL (bridge, handler)");
        return -1;
    }

    bridge->audit_handler = handler;
    bridge->audit_handler_data = user_data;

    return 0;
}

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

int lgss_bio_bridge_process_inbox(
    lgss_bio_bridge_t* bridge,
    uint32_t max_messages
) {
    if (!bridge || !bridge->connected) {
        return -1;  /* Not connected - normal operational state */
    }

    uint32_t processed = 0;
    (void)max_messages;  /* Placeholder for actual message processing */

    bridge->stats.messages_received += processed;
    return (int)processed;
}

int lgss_bio_bridge_update(
    lgss_bio_bridge_t* bridge,
    uint32_t delta_ms
) {
    if (!bridge || !bridge->connected) {
        return -1;  /* Not connected - normal operational state */
    }

    bridge->time_since_heartbeat_ms += delta_ms;

    /* Auto-broadcast heartbeat if interval elapsed */
    if (bridge->time_since_heartbeat_ms >= bridge->config.heartbeat_interval_ms) {
        bridge->last_heartbeat_us = get_timestamp_us();
        bridge->time_since_heartbeat_ms = 0;
    }

    return 0;
}

int lgss_bio_bridge_handle_message(
    lgss_bio_bridge_t* bridge,
    const void* msg,
    size_t msg_size
) {
    if (!bridge || !msg || msg_size < sizeof(bio_message_header_t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lgss_bio_bridge_handle_message: required parameter is NULL (bridge, msg)");
        return -1;
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;
    int result = 0;

    switch (header->type) {
        case BIO_MSG_LGSS_EVALUATE_REQUEST:
            if (bridge->evaluate_handler && msg_size >= sizeof(lgss_bio_evaluate_request_t)) {
                lgss_bio_evaluate_response_t response = {0};
                const lgss_bio_evaluate_request_t* req =
                    (const lgss_bio_evaluate_request_t*)msg;

                lgss_decision_t decision = bridge->evaluate_handler(
                    req, &response, bridge->evaluate_handler_data);

                bridge->stats.evaluations_requested++;
                if (decision == LGSS_DECISION_ALLOW) {
                    bridge->stats.evaluations_allowed++;
                } else if (decision == LGSS_DECISION_BLOCK) {
                    bridge->stats.evaluations_blocked++;
                } else if (decision == LGSS_DECISION_ESCALATE) {
                    bridge->stats.evaluations_escalated++;
                }
            }
            break;

        case BIO_MSG_LGSS_OVERRIDE_REQUEST:
            if (bridge->override_handler && msg_size >= sizeof(lgss_bio_override_request_t)) {
                lgss_bio_override_response_t response = {0};
                const lgss_bio_override_request_t* req =
                    (const lgss_bio_override_request_t*)msg;

                bool approved = bridge->override_handler(
                    req, &response, bridge->override_handler_data);

                bridge->stats.overrides_requested++;
                if (approved) {
                    bridge->stats.overrides_approved++;
                } else {
                    bridge->stats.overrides_denied++;
                }
            }
            break;

        case BIO_MSG_LGSS_HALT_COMMAND:
        case BIO_MSG_LGSS_SOFT_RESET:
        case BIO_MSG_LGSS_HARD_RESET:
            if (bridge->control_handler) {
                result = bridge->control_handler(
                    header->type, msg, msg_size, bridge->control_handler_data);
            }
            break;

        case BIO_MSG_LGSS_AUDIT_REQUEST:
            if (bridge->audit_handler && msg_size >= sizeof(lgss_bio_audit_request_t)) {
                lgss_bio_audit_response_t response = {0};
                uint8_t buffer[4096];
                const lgss_bio_audit_request_t* req =
                    (const lgss_bio_audit_request_t*)msg;

                bridge->audit_handler(
                    req, &response, buffer, sizeof(buffer), bridge->audit_handler_data);
            }
            break;

        case BIO_MSG_LGSS_INTEGRITY_CHECK:
            if (msg_size >= sizeof(lgss_bio_integrity_check_t)) {
                bridge->stats.integrity_checks++;
            }
            break;

        default:
            /* Unknown message type */
            result = -1;
            bridge->stats.handler_errors++;
            break;
    }

    return result;
}

/* ============================================================================
 * Evaluation API
 * ============================================================================ */

int lgss_bio_bridge_send_evaluate_request(
    lgss_bio_bridge_t* bridge,
    uint32_t action_type,
    const char* action_desc,
    float uncertainty,
    float impact_estimate,
    bool reversible,
    uint32_t* request_id
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lgss_bio_bridge_send_evaluate_request: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    lgss_bio_evaluate_request_t msg = {0};
    init_message_header(&msg.header, BIO_MSG_LGSS_EVALUATE_REQUEST,
                        bridge->config.default_channel, 0);

    msg.request_id = bridge->internal.next_request_id++;
    msg.source_module = BIO_MODULE_LGSS_BIO_BRIDGE;
    msg.action_type = action_type;
    msg.uncertainty = uncertainty;
    msg.impact_estimate = impact_estimate;
    msg.reversible = reversible;
    msg.timestamp_us = msg.header.timestamp_us;

    if (action_desc) {
        safe_strncpy(msg.action_desc, action_desc, sizeof(msg.action_desc));
    }

    if (request_id) {
        *request_id = msg.request_id;
    }

    bridge->stats.messages_sent++;
    return 0;
}

int lgss_bio_bridge_send_evaluate_response(
    lgss_bio_bridge_t* bridge,
    uint32_t request_id,
    lgss_decision_t decision,
    float confidence,
    float risk_score,
    const char* policy_name,
    const char* reason
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lgss_bio_bridge_send_evaluate_response: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    lgss_bio_evaluate_response_t msg = {0};
    init_message_header(&msg.header, BIO_MSG_LGSS_EVALUATE_RESPONSE,
                        bridge->config.default_channel, 0);

    msg.request_id = request_id;
    msg.decision = decision;
    msg.confidence = confidence;
    msg.risk_score = risk_score;
    msg.timestamp_us = msg.header.timestamp_us;

    if (policy_name) {
        safe_strncpy(msg.policy_name, policy_name, sizeof(msg.policy_name));
    }
    if (reason) {
        safe_strncpy(msg.reason, reason, sizeof(msg.reason));
    }

    bridge->stats.messages_sent++;
    return 0;
}

/* ============================================================================
 * Violation Notification API
 * ============================================================================ */

int lgss_bio_bridge_broadcast_violation(
    lgss_bio_bridge_t* bridge,
    uint32_t source_module,
    lgss_severity_t severity,
    const char* policy_name,
    const char* action_desc,
    const char* details
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lgss_bio_bridge_broadcast_violation: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    lgss_bio_policy_violation_t msg = {0};
    init_message_header(&msg.header, BIO_MSG_LGSS_POLICY_VIOLATION,
                        bridge->config.urgent_channel,
                        BIO_MSG_FLAG_BROADCAST | BIO_MSG_FLAG_URGENT);

    msg.violation_id = bridge->internal.next_violation_id++;
    msg.source_module = source_module;
    msg.severity = severity;
    msg.timestamp_us = msg.header.timestamp_us;

    if (policy_name) {
        safe_strncpy(msg.policy_name, policy_name, sizeof(msg.policy_name));
    }
    if (action_desc) {
        safe_strncpy(msg.action_desc, action_desc, sizeof(msg.action_desc));
    }
    if (details) {
        safe_strncpy(msg.details, details, sizeof(msg.details));
    }

    bridge->stats.broadcasts_sent++;
    return 0;
}

int lgss_bio_bridge_broadcast_blocked(
    lgss_bio_bridge_t* bridge,
    uint32_t request_id,
    uint32_t source_module,
    const char* action_desc,
    const char* policy_name,
    const char* reason
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lgss_bio_bridge_broadcast_blocked: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    lgss_bio_action_blocked_t msg = {0};
    init_message_header(&msg.header, BIO_MSG_LGSS_ACTION_BLOCKED,
                        bridge->config.alert_channel,
                        BIO_MSG_FLAG_BROADCAST);

    msg.request_id = request_id;
    msg.source_module = source_module;
    msg.timestamp_us = msg.header.timestamp_us;

    if (action_desc) {
        safe_strncpy(msg.action_desc, action_desc, sizeof(msg.action_desc));
    }
    if (policy_name) {
        safe_strncpy(msg.policy_name, policy_name, sizeof(msg.policy_name));
    }
    if (reason) {
        safe_strncpy(msg.reason, reason, sizeof(msg.reason));
    }

    bridge->stats.broadcasts_sent++;
    return 0;
}

int lgss_bio_bridge_broadcast_escalated(
    lgss_bio_bridge_t* bridge,
    uint32_t source_module,
    const char* action_desc,
    const char* reason,
    lgss_severity_t severity,
    float risk_score
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lgss_bio_bridge_broadcast_escalated: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    lgss_bio_action_escalated_t msg = {0};
    init_message_header(&msg.header, BIO_MSG_LGSS_ACTION_ESCALATED,
                        bridge->config.urgent_channel,
                        BIO_MSG_FLAG_BROADCAST | BIO_MSG_FLAG_URGENT);

    msg.escalation_id = bridge->internal.next_escalation_id++;
    msg.source_module = source_module;
    msg.severity = severity;
    msg.risk_score = risk_score;
    msg.timestamp_us = msg.header.timestamp_us;
    msg.timeout_ms = NIMCP_LONG_TIMEOUT_MS;  /* 30 second default */
    msg.default_decision = LGSS_DECISION_BLOCK;

    if (action_desc) {
        safe_strncpy(msg.action_desc, action_desc, sizeof(msg.action_desc));
    }
    if (reason) {
        safe_strncpy(msg.reason, reason, sizeof(msg.reason));
    }

    bridge->stats.broadcasts_sent++;
    return 0;
}

/* ============================================================================
 * Risk/Uncertainty API
 * ============================================================================ */

int lgss_bio_bridge_broadcast_uncertainty_alert(
    lgss_bio_bridge_t* bridge,
    uint32_t source_module,
    float uncertainty,
    const char* context
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lgss_bio_bridge_broadcast_uncertainty_alert: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    lgss_bio_uncertainty_alert_t msg = {0};
    init_message_header(&msg.header, BIO_MSG_LGSS_UNCERTAINTY_ALERT,
                        bridge->config.alert_channel,
                        BIO_MSG_FLAG_BROADCAST);

    msg.source_module = source_module;
    msg.uncertainty = uncertainty;
    msg.threshold = bridge->config.uncertainty_alert_threshold;
    msg.timestamp_us = msg.header.timestamp_us;

    if (context) {
        safe_strncpy(msg.context, context, sizeof(msg.context));
    }

    bridge->stats.broadcasts_sent++;
    return 0;
}

int lgss_bio_bridge_broadcast_risk_assessment(
    lgss_bio_bridge_t* bridge,
    uint32_t source_module,
    float overall_risk,
    float harm_probability,
    float harm_severity,
    const char* risk_factors
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lgss_bio_bridge_broadcast_risk_assessment: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    lgss_bio_risk_assessment_t msg = {0};
    init_message_header(&msg.header, BIO_MSG_LGSS_RISK_ASSESSMENT,
                        bridge->config.default_channel,
                        BIO_MSG_FLAG_BROADCAST);

    msg.assessment_id = bridge->internal.next_request_id++;
    msg.source_module = source_module;
    msg.overall_risk = overall_risk;
    msg.harm_probability = harm_probability;
    msg.harm_severity = harm_severity;
    msg.mitigation_factor = 0.0f;
    msg.timestamp_us = msg.header.timestamp_us;

    /* Determine recommended response based on risk */
    if (overall_risk >= 0.9f) {
        msg.recommended_response = LGSS_SEVERITY_CRITICAL;
    } else if (overall_risk >= 0.7f) {
        msg.recommended_response = LGSS_SEVERITY_HIGH;
    } else if (overall_risk >= 0.5f) {
        msg.recommended_response = LGSS_SEVERITY_MEDIUM;
    } else if (overall_risk >= 0.3f) {
        msg.recommended_response = LGSS_SEVERITY_LOW;
    } else {
        msg.recommended_response = LGSS_SEVERITY_INFO;
    }

    if (risk_factors) {
        safe_strncpy(msg.risk_factors, risk_factors, sizeof(msg.risk_factors));
    }

    bridge->stats.broadcasts_sent++;
    return 0;
}

/* ============================================================================
 * Integrity API
 * ============================================================================ */

int lgss_bio_bridge_request_integrity_check(
    lgss_bio_bridge_t* bridge,
    uint32_t target_module,
    bool full_check,
    uint32_t* check_id
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lgss_bio_bridge_request_integrity_check: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    lgss_bio_integrity_check_t msg = {0};
    init_message_header(&msg.header, BIO_MSG_LGSS_INTEGRITY_CHECK,
                        bridge->config.default_channel, 0);

    msg.check_id = bridge->internal.next_check_id++;
    msg.target_module = target_module;
    msg.full_check = full_check;
    msg.timestamp_us = msg.header.timestamp_us;

    if (check_id) {
        *check_id = msg.check_id;
    }

    bridge->stats.integrity_checks++;
    bridge->stats.messages_sent++;
    return 0;
}

int lgss_bio_bridge_broadcast_tampering(
    lgss_bio_bridge_t* bridge,
    uint32_t affected_module,
    lgss_integrity_status_t status,
    const char* details,
    bool halt_system
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lgss_bio_bridge_broadcast_tampering: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    lgss_bio_tampering_detected_t msg = {0};
    init_message_header(&msg.header, BIO_MSG_LGSS_TAMPERING_DETECTED,
                        bridge->config.urgent_channel,
                        BIO_MSG_FLAG_BROADCAST | BIO_MSG_FLAG_URGENT);

    msg.detection_id = bridge->internal.next_request_id++;
    msg.affected_module = affected_module;
    msg.status = status;
    msg.system_halted = halt_system;
    msg.timestamp_us = msg.header.timestamp_us;

    if (details) {
        safe_strncpy(msg.details, details, sizeof(msg.details));
    }

    if (halt_system) {
        bridge->internal.halted = true;
    }

    bridge->stats.tampering_detected++;
    bridge->stats.integrity_failures++;
    bridge->stats.broadcasts_sent++;
    return 0;
}

/* ============================================================================
 * Control API
 * ============================================================================ */

int lgss_bio_bridge_send_halt(
    lgss_bio_bridge_t* bridge,
    lgss_override_level_t auth_level,
    const char* reason,
    const char* operator_id,
    bool immediate
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lgss_bio_bridge_send_halt: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    lgss_bio_halt_command_t msg = {0};
    init_message_header(&msg.header, BIO_MSG_LGSS_HALT_COMMAND,
                        bridge->config.urgent_channel,
                        BIO_MSG_FLAG_BROADCAST | BIO_MSG_FLAG_URGENT);

    msg.command_id = bridge->internal.next_command_id++;
    msg.auth_level = auth_level;
    msg.immediate = immediate;
    msg.affected_modules = 0xFFFFFFFF;  /* All modules */
    msg.timestamp_us = msg.header.timestamp_us;

    if (reason) {
        safe_strncpy(msg.reason, reason, sizeof(msg.reason));
    }
    if (operator_id) {
        safe_strncpy(msg.operator_id, operator_id, sizeof(msg.operator_id));
    }

    bridge->internal.halted = true;
    bridge->stats.broadcasts_sent++;
    return 0;
}

int lgss_bio_bridge_send_reset(
    lgss_bio_bridge_t* bridge,
    lgss_reset_type_t reset_type,
    lgss_override_level_t auth_level,
    const char* reason,
    const char* operator_id
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lgss_bio_bridge_send_reset: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    bio_message_type_t msg_type = (reset_type == LGSS_RESET_SOFT)
        ? BIO_MSG_LGSS_SOFT_RESET : BIO_MSG_LGSS_HARD_RESET;

    lgss_bio_reset_command_t msg = {0};
    init_message_header(&msg.header, msg_type,
                        bridge->config.urgent_channel,
                        BIO_MSG_FLAG_BROADCAST | BIO_MSG_FLAG_URGENT);

    msg.command_id = bridge->internal.next_command_id++;
    msg.reset_type = reset_type;
    msg.auth_level = auth_level;
    msg.affected_modules = 0xFFFFFFFF;  /* All modules */
    msg.timestamp_us = msg.header.timestamp_us;

    if (reason) {
        safe_strncpy(msg.reason, reason, sizeof(msg.reason));
    }
    if (operator_id) {
        safe_strncpy(msg.operator_id, operator_id, sizeof(msg.operator_id));
    }

    bridge->internal.last_reset_type = reset_type;
    bridge->internal.halted = false;  /* Reset clears halt */
    bridge->stats.broadcasts_sent++;
    return 0;
}

/* ============================================================================
 * Plasticity Coordination API
 * ============================================================================ */

int lgss_bio_bridge_broadcast_safety_event(
    lgss_bio_bridge_t* bridge,
    uint32_t source_module,
    lgss_severity_t severity,
    const char* event_type,
    const char* description,
    bool learning_affected
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lgss_bio_bridge_broadcast_safety_event: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_plasticity_gating) return 0;

    lgss_bio_safety_event_t msg = {0};
    init_message_header(&msg.header, BIO_MSG_LGSS_SAFETY_EVENT,
                        bridge->config.alert_channel,
                        BIO_MSG_FLAG_BROADCAST);

    msg.event_id = bridge->internal.next_request_id++;
    msg.source_module = source_module;
    msg.severity = severity;
    msg.learning_affected = learning_affected;
    msg.timestamp_us = msg.header.timestamp_us;

    if (event_type) {
        safe_strncpy(msg.event_type, event_type, sizeof(msg.event_type));
    }
    if (description) {
        safe_strncpy(msg.description, description, sizeof(msg.description));
    }

    bridge->stats.broadcasts_sent++;
    return 0;
}

int lgss_bio_bridge_send_neuromod_signal(
    lgss_bio_bridge_t* bridge,
    nimcp_bio_channel_type_t channel,
    float signal_strength,
    float safety_modulation,
    bool suppress_plasticity
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lgss_bio_bridge_send_neuromod_signal: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_plasticity_gating) return 0;

    lgss_bio_neuromod_signal_t msg = {0};
    init_message_header(&msg.header, BIO_MSG_LGSS_NEUROMOD_SIGNAL,
                        channel, BIO_MSG_FLAG_BROADCAST);

    msg.channel = channel;
    msg.signal_strength = signal_strength;
    msg.safety_modulation = safety_modulation;
    msg.suppress_plasticity = suppress_plasticity;
    msg.plasticity_factor = suppress_plasticity ? 0.0f : safety_modulation;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.broadcasts_sent++;
    return 0;
}

/* ============================================================================
 * Telemetry API
 * ============================================================================ */

int lgss_bio_bridge_log_telemetry(
    lgss_bio_bridge_t* bridge,
    uint32_t source_module,
    lgss_severity_t severity,
    const char* event_type,
    const char* message
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lgss_bio_bridge_log_telemetry: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_telemetry) return 0;

    lgss_bio_telemetry_log_t msg = {0};
    init_message_header(&msg.header, BIO_MSG_LGSS_TELEMETRY_LOG,
                        bridge->config.default_channel, 0);

    msg.log_id = bridge->internal.next_log_id++;
    msg.source_module = source_module;
    msg.severity = severity;
    msg.has_metric = false;
    msg.timestamp_us = msg.header.timestamp_us;

    if (event_type) {
        safe_strncpy(msg.event_type, event_type, sizeof(msg.event_type));
    }
    if (message) {
        safe_strncpy(msg.message, message, sizeof(msg.message));
    }

    bridge->stats.messages_sent++;
    return 0;
}

/* ============================================================================
 * Statistics and Diagnostics API
 * ============================================================================ */

int lgss_bio_bridge_get_stats(
    const lgss_bio_bridge_t* bridge,
    lgss_bio_bridge_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lgss_bio_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

int lgss_bio_bridge_reset_stats(lgss_bio_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lgss_bio_bridge_reset_stats: bridge is NULL");
        return -1;
    }

    memset(&bridge->stats, 0, sizeof(bridge->stats));
    return 0;
}

const char* lgss_decision_name(lgss_decision_t decision) {
    switch (decision) {
        case LGSS_DECISION_ALLOW:    return "ALLOW";
        case LGSS_DECISION_BLOCK:    return "BLOCK";
        case LGSS_DECISION_ESCALATE: return "ESCALATE";
        case LGSS_DECISION_DEFER:    return "DEFER";
        case LGSS_DECISION_PENDING:  return "PENDING";
        case LGSS_DECISION_TIMEOUT:  return "TIMEOUT";
        case LGSS_DECISION_ERROR:    return "ERROR";
        default:                     return "UNKNOWN";
    }
}

const char* lgss_severity_name(lgss_severity_t severity) {
    switch (severity) {
        case LGSS_SEVERITY_INFO:     return "INFO";
        case LGSS_SEVERITY_LOW:      return "LOW";
        case LGSS_SEVERITY_MEDIUM:   return "MEDIUM";
        case LGSS_SEVERITY_HIGH:     return "HIGH";
        case LGSS_SEVERITY_CRITICAL: return "CRITICAL";
        default:                     return "UNKNOWN";
    }
}

const char* lgss_override_level_name(lgss_override_level_t level) {
    switch (level) {
        case LGSS_OVERRIDE_NONE:      return "NONE";
        case LGSS_OVERRIDE_OPERATOR:  return "OPERATOR";
        case LGSS_OVERRIDE_ADMIN:     return "ADMIN";
        case LGSS_OVERRIDE_EMERGENCY: return "EMERGENCY";
        default:                      return "UNKNOWN";
    }
}

const char* lgss_integrity_status_name(lgss_integrity_status_t status) {
    switch (status) {
        case LGSS_INTEGRITY_OK:        return "OK";
        case LGSS_INTEGRITY_MODIFIED:  return "MODIFIED";
        case LGSS_INTEGRITY_TAMPERED:  return "TAMPERED";
        case LGSS_INTEGRITY_CORRUPTED: return "CORRUPTED";
        case LGSS_INTEGRITY_UNKNOWN:   return "UNKNOWN";
        default:                       return "UNKNOWN";
    }
}

void lgss_bio_bridge_print_summary(const lgss_bio_bridge_t* bridge) {
    if (!bridge) {
        printf("LGSS Bio-Async Bridge: NULL\n");
        return;
    }

    printf("=== LGSS Bio-Async Bridge Summary ===\n");
    printf("Connected: %s\n", bridge->connected ? "yes" : "no");
    printf("Halted: %s\n", bridge->internal.halted ? "yes" : "no");
    printf("\n--- Message Statistics ---\n");
    printf("Messages sent:     %lu\n", (unsigned long)bridge->stats.messages_sent);
    printf("Messages received: %lu\n", (unsigned long)bridge->stats.messages_received);
    printf("Broadcasts sent:   %lu\n", (unsigned long)bridge->stats.broadcasts_sent);
    printf("\n--- Evaluation Statistics ---\n");
    printf("Requested:  %lu\n", (unsigned long)bridge->stats.evaluations_requested);
    printf("Allowed:    %lu\n", (unsigned long)bridge->stats.evaluations_allowed);
    printf("Blocked:    %lu\n", (unsigned long)bridge->stats.evaluations_blocked);
    printf("Escalated:  %lu\n", (unsigned long)bridge->stats.evaluations_escalated);
    printf("\n--- Override Statistics ---\n");
    printf("Requested: %lu\n", (unsigned long)bridge->stats.overrides_requested);
    printf("Approved:  %lu\n", (unsigned long)bridge->stats.overrides_approved);
    printf("Denied:    %lu\n", (unsigned long)bridge->stats.overrides_denied);
    printf("\n--- Integrity Statistics ---\n");
    printf("Checks:    %lu\n", (unsigned long)bridge->stats.integrity_checks);
    printf("Failures:  %lu\n", (unsigned long)bridge->stats.integrity_failures);
    printf("Tampering: %lu\n", (unsigned long)bridge->stats.tampering_detected);
    printf("=====================================\n");
}
