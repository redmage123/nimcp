/**
 * @file nimcp_mesh_security_integration.c
 * @brief Mesh Network Security Integration Implementation
 *
 * WHAT: Implementation of unified exception/immune/BBB integration for mesh
 * WHY:  Coordinated security response across mesh via integrated systems
 * HOW:  Exception -> Antigen -> Immune routing with BBB validation hooks
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#include "mesh/nimcp_mesh_security_integration.h"
#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_integration.h"
#include "mesh/nimcp_mesh_transaction.h"
#include "mesh/nimcp_mesh_channel.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

#define SECURITY_INTEGRATION_MAGIC 0x53454349  /* "SECI" */

/**
 * @brief Pending security event entry
 */
typedef struct pending_event {
    mesh_security_event_t event;
    bool processed;
    struct pending_event* next;
} pending_event_t;

/**
 * @brief Threat history entry
 */
typedef struct threat_entry {
    mesh_participant_id_t participant;
    float threat_score;
    uint32_t occurrence_count;
    uint64_t first_seen_ns;
    uint64_t last_seen_ns;
    bool active;
} threat_entry_t;

/**
 * @brief Security integration structure
 */
struct mesh_security_integration {
    uint32_t magic;

    /* Configuration */
    mesh_security_config_t config;

    /* Dependencies */
    mesh_bootstrap_t* bootstrap;
    mesh_integration_t* integration;
    mesh_exception_bridge_t* exception_bridge;
    bbb_system_t bbb;
    brain_immune_system_t* immune;
    mesh_msp_t* msp;

    /* Pending events */
    pending_event_t* pending_events_head;
    size_t pending_count;

    /* Threat history */
    threat_entry_t threat_history[MESH_SECURITY_MAX_THREAT_HISTORY];
    size_t threat_count;

    /* Callbacks */
    mesh_security_event_cb_t event_callback;
    void* event_callback_data;
    mesh_security_quarantine_cb_t quarantine_callback;
    void* quarantine_callback_data;
    mesh_security_bbb_validate_cb_t bbb_validate_callback;
    void* bbb_validate_callback_data;

    /* Statistics */
    mesh_security_stats_t stats;

    /* Broadcast timing */
    uint64_t last_broadcast_ns;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint64_t get_time_ns(void) {
    return nimcp_time_now_ns();
}

/**
 * @brief Find threat entry for participant
 */
static threat_entry_t* find_threat_entry(
    mesh_security_integration_t* si,
    mesh_participant_id_t participant
) {
    for (size_t i = 0; i < si->threat_count; i++) {
        if (si->threat_history[i].participant == participant &&
            si->threat_history[i].active) {
            return &si->threat_history[i];
        }
    }
    /* Not found is normal -- no threat is good */
    return NULL;
}

/**
 * @brief Record threat for participant
 */
static void record_threat(
    mesh_security_integration_t* si,
    mesh_participant_id_t participant,
    float threat_score
) {
    threat_entry_t* entry = find_threat_entry(si, participant);
    uint64_t now = get_time_ns();

    if (entry) {
        entry->threat_score = (entry->threat_score + threat_score) / 2.0f;
        entry->occurrence_count++;
        entry->last_seen_ns = now;
    } else if (si->threat_count < MESH_SECURITY_MAX_THREAT_HISTORY) {
        entry = &si->threat_history[si->threat_count++];
        entry->participant = participant;
        entry->threat_score = threat_score;
        entry->occurrence_count = 1;
        entry->first_seen_ns = now;
        entry->last_seen_ns = now;
        entry->active = true;
    }
}

/**
 * @brief Queue security event
 */
static nimcp_error_t queue_event(
    mesh_security_integration_t* si,
    const mesh_security_event_t* event
) {
    if (si->pending_count >= MESH_SECURITY_MAX_PENDING_EVENTS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_CAPACITY_EXCEEDED, "mesh_security_integration: error condition");
        return NIMCP_ERROR_CAPACITY_EXCEEDED;
    }

    pending_event_t* pending = nimcp_calloc(1, sizeof(pending_event_t));
    if (!pending) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_security_integration: memory allocation failed");
        return NIMCP_ERROR_NO_MEMORY;
    }

    pending->event = *event;
    pending->processed = false;
    pending->next = si->pending_events_head;
    si->pending_events_head = pending;
    si->pending_count++;

    return NIMCP_SUCCESS;
}

/**
 * @brief Invoke event callback
 */
static void invoke_event_callback(
    mesh_security_integration_t* si,
    const mesh_security_event_t* event
) {
    if (si->event_callback) {
        si->event_callback(si, event, si->event_callback_data);
    }
}

/**
 * @brief Map BBB severity to mesh exception severity
 */
static mesh_exception_severity_t bbb_to_mesh_severity(bbb_severity_t sev) {
    switch (sev) {
        case BBB_SEVERITY_NONE:
            return MESH_EXC_SEVERITY_TRACE;
        case BBB_SEVERITY_LOW:
            return MESH_EXC_SEVERITY_INFO;
        case BBB_SEVERITY_MEDIUM:
            return MESH_EXC_SEVERITY_WARNING;
        case BBB_SEVERITY_HIGH:
            return MESH_EXC_SEVERITY_SEVERE;
        case BBB_SEVERITY_CRITICAL:
            return MESH_EXC_SEVERITY_CRITICAL;
        default:
            return MESH_EXC_SEVERITY_ERROR;
    }
}

/**
 * @brief Map immune action to MSP event type
 */
static int immune_action_to_msp_event(mesh_immune_action_t action) {
    switch (action) {
        case MESH_IMMUNE_ACTION_QUARANTINE:
            return 0;  /* Threat detected */
        case MESH_IMMUNE_ACTION_REVOKE:
        case MESH_IMMUNE_ACTION_SHUTDOWN:
            return 1;  /* Chronic failure */
        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "immune_action_to_msp_event: operation failed");
            return -1; /* No MSP action needed */
    }
}

/* ============================================================================
 * Configuration
 * ============================================================================ */

nimcp_error_t mesh_security_default_config(mesh_security_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_security_default_config: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    memset(config, 0, sizeof(*config));

    config->bbb_threat_threshold = MESH_SECURITY_DEFAULT_THREAT_THRESHOLD;
    config->severity_threshold = MESH_SECURITY_DEFAULT_SEVERITY_THRESHOLD;
    config->inflammation_threshold = 0.5f;

    config->quarantine_duration_ms = MESH_MSP_DEFAULT_QUARANTINE_MS;
    config->recovery_timeout_ms = 60000;
    config->broadcast_interval_ms = 100;

    config->enable_auto_quarantine = true;
    config->enable_bbb_validation = true;
    config->enable_immune_routing = true;
    config->enable_security_broadcasts = true;
    config->enable_credential_tracking = true;

    config->verbose_logging = false;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

mesh_security_integration_t* mesh_security_create(
    mesh_bootstrap_t* bootstrap,
    bbb_system_t bbb,
    brain_immune_system_t* immune,
    mesh_msp_t* msp,
    const mesh_security_config_t* config
) {
    if (!bootstrap) {
        LOG_ERROR("Cannot create security integration without bootstrap");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_security_create: bootstrap is NULL");
        return NULL;
    }

    mesh_security_integration_t* si = nimcp_calloc(1, sizeof(*si));
    if (!si) {
        LOG_ERROR("Failed to allocate security integration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_security_create: si is NULL");
        return NULL;
    }

    si->magic = SECURITY_INTEGRATION_MAGIC;

    /* Configuration */
    if (config) {
        si->config = *config;
    } else {
        mesh_security_default_config(&si->config);
    }

    /* Dependencies */
    si->bootstrap = bootstrap;
    si->integration = mesh_bootstrap_get_integration(bootstrap);
    si->bbb = bbb;
    si->immune = immune;
    si->msp = msp;

    /* Create mutex */
    mutex_attr_t attr = {0};
    si->mutex = nimcp_mutex_create(&attr);
    if (!si->mutex) {
        LOG_ERROR("Failed to create security integration mutex");
        nimcp_free(si);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_security_create: si->mutex is NULL");
        return NULL;
    }

    LOG_DEBUG("Security integration created");
    return si;
}

void mesh_security_destroy(mesh_security_integration_t* si) {
    if (!si || si->magic != SECURITY_INTEGRATION_MAGIC) {
        return;
    }

    nimcp_mutex_lock(si->mutex);

    /* Free pending events */
    pending_event_t* event = si->pending_events_head;
    while (event) {
        pending_event_t* next = event->next;
        nimcp_free(event);
        event = next;
    }

    nimcp_mutex_unlock(si->mutex);
    nimcp_mutex_destroy(si->mutex);

    si->magic = 0;
    nimcp_free(si);

    LOG_DEBUG("Security integration destroyed");
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

nimcp_error_t mesh_security_set_bbb(
    mesh_security_integration_t* si,
    bbb_system_t bbb
) {
    if (!si || si->magic != SECURITY_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_security_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(si->mutex);
    si->bbb = bbb;

    /* Also set on exception bridge if present */
    if (si->exception_bridge) {
        mesh_exception_bridge_set_bbb(si->exception_bridge, (blood_brain_barrier_t*)bbb);
    }

    nimcp_mutex_unlock(si->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_security_set_immune(
    mesh_security_integration_t* si,
    brain_immune_system_t* immune
) {
    if (!si || si->magic != SECURITY_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_security_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(si->mutex);
    si->immune = immune;
    nimcp_mutex_unlock(si->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_security_set_msp(
    mesh_security_integration_t* si,
    mesh_msp_t* msp
) {
    if (!si || si->magic != SECURITY_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_security_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(si->mutex);
    si->msp = msp;
    nimcp_mutex_unlock(si->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_security_set_exception_bridge(
    mesh_security_integration_t* si,
    mesh_exception_bridge_t* bridge
) {
    if (!si || si->magic != SECURITY_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_security_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(si->mutex);
    si->exception_bridge = bridge;

    /* Wire up BBB if we have it */
    if (bridge && si->bbb) {
        mesh_exception_bridge_set_bbb(bridge, (blood_brain_barrier_t*)si->bbb);
    }

    nimcp_mutex_unlock(si->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Exception -> Antigen -> Immune Pathway
 * ============================================================================ */

nimcp_error_t mesh_security_route_exception(
    mesh_security_integration_t* si,
    nimcp_error_t error_code,
    const char* message,
    mesh_participant_id_t source_module,
    const char* source_file,
    uint32_t source_line,
    mesh_exception_response_t* response_out
) {
    if (!si || si->magic != SECURITY_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_security_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(si->mutex);
    si->stats.exceptions_routed++;

    mesh_exception_response_t response;
    memset(&response, 0, sizeof(response));

    /* Use exception bridge if available */
    if (si->exception_bridge) {
        nimcp_mutex_unlock(si->mutex);

        nimcp_error_t err = mesh_exception_bridge_route_error(
            si->exception_bridge,
            error_code,
            message,
            source_module,
            source_file,
            source_line,
            &response
        );

        nimcp_mutex_lock(si->mutex);

        if (err != NIMCP_SUCCESS) {
            nimcp_mutex_unlock(si->mutex);
            return err;
        }
    } else {
        /* Classify directly */
        mesh_exception_category_t category;
        mesh_exception_severity_t severity;
        mesh_exception_bridge_classify(error_code, &category, &severity);

        response.primary_action = (severity >= MESH_EXC_SEVERITY_SEVERE) ?
            MESH_IMMUNE_ACTION_QUARANTINE : MESH_IMMUNE_ACTION_LOG;
        response.threat_score = (float)severity / (float)MESH_EXC_SEVERITY_CRITICAL;
    }

    /* Record threat */
    record_threat(si, source_module, response.threat_score);

    /* Create security event */
    mesh_security_event_t event = {0};
    event.type = MESH_SEC_EVENT_EXCEPTION_DETECTED;
    event.participant = source_module;
    event.channel = mesh_get_channel(source_module);
    event.data.exception.error_code = error_code;
    event.timestamp_ns = get_time_ns();

    /* Classify for event data */
    mesh_exception_bridge_classify(
        error_code,
        &event.data.exception.category,
        &event.data.exception.severity
    );

    /* Queue event */
    queue_event(si, &event);
    invoke_event_callback(si, &event);

    /* Route to immune system if enabled and severity meets threshold */
    if (si->config.enable_immune_routing && si->immune) {
        if (event.data.exception.severity >= si->config.severity_threshold) {
            /* Create antigen from exception */
            mesh_exception_antigen_t antigen = {0};
            antigen.error_code = error_code;
            antigen.source_module = source_module;
            antigen.severity = event.data.exception.severity;
            antigen.category = event.data.exception.category;
            antigen.timestamp_ns = get_time_ns();

            if (message) {
                strncpy(antigen.error_message, message,
                        sizeof(antigen.error_message) - 1);
            }
            if (source_file) {
                strncpy(antigen.source_file, source_file,
                        sizeof(antigen.source_file) - 1);
            }
            antigen.source_line = source_line;

            nimcp_mutex_unlock(si->mutex);

            /* Present to immune system */
            mesh_security_present_antigen(si, &antigen, NULL);

            nimcp_mutex_lock(si->mutex);
        }
    }

    /* Handle immune response action */
    if (response.primary_action >= MESH_IMMUNE_ACTION_QUARANTINE) {
        if (si->config.enable_auto_quarantine && si->msp) {
            nimcp_mutex_unlock(si->mutex);

            mesh_security_quarantine(
                si,
                source_module,
                response.quarantine_duration_ms > 0 ?
                    response.quarantine_duration_ms :
                    si->config.quarantine_duration_ms,
                "Auto-quarantine from exception routing"
            );

            nimcp_mutex_lock(si->mutex);
        }
    }

    if (response_out) {
        *response_out = response;
    }

    nimcp_mutex_unlock(si->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_security_present_antigen(
    mesh_security_integration_t* si,
    const mesh_exception_antigen_t* antigen,
    mesh_exception_response_t* response_out
) {
    if (!si || si->magic != SECURITY_INTEGRATION_MAGIC || !antigen) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_security_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(si->mutex);
    si->stats.antigens_presented++;

    /* BBB validation if enabled */
    float threat_score = 0.0f;
    if (si->config.enable_bbb_validation && si->exception_bridge) {
        nimcp_mutex_unlock(si->mutex);

        mesh_exception_bridge_bbb_validate(
            si->exception_bridge, antigen, &threat_score);

        nimcp_mutex_lock(si->mutex);
        si->stats.bbb_validations++;

        if (threat_score >= si->config.bbb_threat_threshold) {
            si->stats.bbb_threats_detected++;
        }
    } else {
        threat_score = (float)antigen->severity /
                       (float)MESH_EXC_SEVERITY_CRITICAL;
    }

    /* Present to brain immune system if available */
    if (si->immune) {
        /* Convert mesh antigen to brain immune format and present */
        uint8_t epitope[64];
        size_t epitope_len = 0;

        /* Encode exception info as epitope */
        memcpy(epitope, &antigen->error_code, sizeof(antigen->error_code));
        epitope_len += sizeof(antigen->error_code);
        memcpy(epitope + epitope_len, &antigen->category,
               sizeof(antigen->category));
        epitope_len += sizeof(antigen->category);

        /* Present to brain immune */
        uint32_t brain_antigen_id = 0;
        brain_immune_present_antigen(
            si->immune,
            ANTIGEN_SOURCE_MANUAL,
            epitope,
            epitope_len,
            antigen->severity + 1,  /* Map to 1-10 */
            (uint32_t)antigen->source_module,
            &brain_antigen_id
        );

        /* Create antigen event */
        mesh_security_event_t event = {0};
        event.type = MESH_SEC_EVENT_ANTIGEN_PRESENTED;
        event.participant = antigen->source_module;
        event.channel = antigen->source_channel;
        event.data.antigen.antigen_id = brain_antigen_id;
        event.data.antigen.threat_score = threat_score;
        event.timestamp_ns = get_time_ns();

        queue_event(si, &event);
        invoke_event_callback(si, &event);
    }

    /* Build response */
    if (response_out) {
        memset(response_out, 0, sizeof(*response_out));
        response_out->threat_score = threat_score;

        if (threat_score >= 0.9f) {
            response_out->primary_action = MESH_IMMUNE_ACTION_SHUTDOWN;
        } else if (threat_score >= si->config.bbb_threat_threshold) {
            response_out->primary_action = MESH_IMMUNE_ACTION_QUARANTINE;
            response_out->quarantine_duration_ms = si->config.quarantine_duration_ms;
        } else if (threat_score >= 0.4f) {
            response_out->primary_action = MESH_IMMUNE_ACTION_WARN;
        } else {
            response_out->primary_action = MESH_IMMUNE_ACTION_LOG;
        }
    }

    nimcp_mutex_unlock(si->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * BBB Mesh Validation Hooks
 * ============================================================================ */

nimcp_error_t mesh_security_validate_transaction(
    mesh_security_integration_t* si,
    const mesh_transaction_t* tx,
    float* threat_score_out
) {
    if (!si || si->magic != SECURITY_INTEGRATION_MAGIC || !tx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_security_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(si->mutex);
    si->stats.bbb_validations++;

    float threat_score = 0.0f;

    /* Custom callback first */
    if (si->bbb_validate_callback) {
        nimcp_mutex_unlock(si->mutex);

        bool allowed = si->bbb_validate_callback(
            si, tx, &threat_score, si->bbb_validate_callback_data);

        nimcp_mutex_lock(si->mutex);

        if (!allowed) {
            si->stats.bbb_threats_detected++;
            record_threat(si, tx->proposer_id, threat_score);
            nimcp_mutex_unlock(si->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BBB_VALIDATION, "mesh_security_integration: error condition");
            return NIMCP_ERROR_BBB_VALIDATION;
        }
    }

    /* BBB validation if available */
    if (si->bbb) {
        bbb_validation_result_t result;

        nimcp_mutex_unlock(si->mutex);

        bool valid = bbb_validate_input(
            si->bbb,
            tx->payload,
            tx->payload_size,
            &result
        );

        nimcp_mutex_lock(si->mutex);

        if (!valid) {
            threat_score = (float)result.severity /
                           (float)BBB_SEVERITY_CRITICAL;
            si->stats.bbb_threats_detected++;
            record_threat(si, tx->proposer_id, threat_score);

            if (threat_score_out) *threat_score_out = threat_score;
            nimcp_mutex_unlock(si->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BBB_VALIDATION, "mesh_security_integration: error condition");
            return NIMCP_ERROR_BBB_VALIDATION;
        }
    }

    /* Check proposer threat history */
    threat_entry_t* entry = find_threat_entry(si, tx->proposer_id);
    if (entry) {
        threat_score = entry->threat_score;
        if (threat_score >= si->config.bbb_threat_threshold) {
            si->stats.bbb_threats_detected++;
            if (threat_score_out) *threat_score_out = threat_score;
            nimcp_mutex_unlock(si->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BBB_VALIDATION, "mesh_security_integration: error condition");
            return NIMCP_ERROR_BBB_VALIDATION;
        }
    }

    /* Check quarantine status via MSP */
    if (si->msp) {
        if (mesh_msp_is_quarantined(si->msp, tx->proposer_id)) {
            if (threat_score_out) *threat_score_out = 1.0f;
            nimcp_mutex_unlock(si->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_ACCESS_DENIED, "mesh_security_integration: error condition");
            return NIMCP_ERROR_ACCESS_DENIED;
        }
    }

    if (threat_score_out) *threat_score_out = threat_score;

    nimcp_mutex_unlock(si->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_security_validate_credential(
    mesh_security_integration_t* si,
    mesh_participant_id_t participant,
    const credential_t* credential
) {
    if (!si || si->magic != SECURITY_INTEGRATION_MAGIC || !credential) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_security_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(si->mutex);

    /* Validate credential state */
    if (credential->state != CREDENTIAL_STATE_VALID) {
        nimcp_mutex_unlock(si->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "mesh_security_integration: invalid state");
        return NIMCP_ERROR_INVALID_STATE;
    }

    /* Check expiration */
    uint64_t now = get_time_ns();
    if (credential->expires_at_ns > 0 && now > credential->expires_at_ns) {
        nimcp_mutex_unlock(si->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_TIMEOUT, "mesh_security_integration: error condition");
        return NIMCP_ERROR_TIMEOUT;  /* Credential expired */
    }

    /* BBB signature verification if available */
    if (si->bbb && si->config.enable_bbb_validation) {
        nimcp_mutex_unlock(si->mutex);

        bool valid = bbb_verify_signature(
            si->bbb,
            credential->id,
            MESH_CREDENTIAL_ID_SIZE,
            credential->msp_signature,
            MESH_SIGNATURE_SIZE
        );

        nimcp_mutex_lock(si->mutex);

        if (!valid) {
            si->stats.bbb_threats_detected++;
            record_threat(si, participant, 0.8f);
            nimcp_mutex_unlock(si->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BBB_VALIDATION, "mesh_security_integration: error condition");
            return NIMCP_ERROR_BBB_VALIDATION;
        }
    }

    nimcp_mutex_unlock(si->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_security_check_participant(
    mesh_security_integration_t* si,
    mesh_participant_id_t participant,
    bool* is_quarantined_out,
    float* threat_level_out
) {
    if (!si || si->magic != SECURITY_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_security_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(si->mutex);

    bool quarantined = false;
    float threat = 0.0f;

    /* Check MSP quarantine */
    if (si->msp) {
        quarantined = mesh_msp_is_quarantined(si->msp, participant);
    }

    /* Check threat history */
    threat_entry_t* entry = find_threat_entry(si, participant);
    if (entry) {
        threat = entry->threat_score;
    }

    if (is_quarantined_out) *is_quarantined_out = quarantined;
    if (threat_level_out) *threat_level_out = threat;

    nimcp_mutex_unlock(si->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Quarantine and Revocation API
 * ============================================================================ */

nimcp_error_t mesh_security_quarantine(
    mesh_security_integration_t* si,
    mesh_participant_id_t participant,
    uint64_t duration_ms,
    const char* reason
) {
    if (!si || si->magic != SECURITY_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_security_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(si->mutex);

    /* Check quarantine callback */
    if (si->quarantine_callback) {
        float threat = 0.0f;
        threat_entry_t* entry = find_threat_entry(si, participant);
        if (entry) threat = entry->threat_score;

        nimcp_mutex_unlock(si->mutex);

        bool proceed = si->quarantine_callback(
            si, participant, threat, reason, si->quarantine_callback_data);

        nimcp_mutex_lock(si->mutex);

        if (!proceed) {
            nimcp_mutex_unlock(si->mutex);
            return NIMCP_SUCCESS;  /* Callback overrode quarantine */
        }
    }

    /* Issue quarantine via MSP */
    if (si->msp) {
        nimcp_error_t err = mesh_msp_quarantine(si->msp, participant, duration_ms);
        if (err != NIMCP_SUCCESS && err != NIMCP_ERROR_NOT_FOUND) {
            nimcp_mutex_unlock(si->mutex);
            return err;
        }
    }

    si->stats.quarantine_issued++;

    /* Create event */
    mesh_security_event_t event = {0};
    event.type = MESH_SEC_EVENT_QUARANTINE_ISSUED;
    event.participant = participant;
    event.channel = mesh_get_channel(participant);
    event.data.quarantine.duration_ms = duration_ms;
    event.data.quarantine.reason = reason;
    event.timestamp_ns = get_time_ns();

    queue_event(si, &event);
    invoke_event_callback(si, &event);

    /* Broadcast if enabled */
    if (si->config.enable_security_broadcasts) {
        nimcp_mutex_unlock(si->mutex);
        mesh_security_broadcast_quarantine(si, participant, duration_ms, reason);
        nimcp_mutex_lock(si->mutex);
    }

    /* Notify immune system */
    if (si->immune) {
        brain_immune_handle_bft_quarantine(
            si->immune,
            (uint32_t)participant,
            duration_ms,
            0.5f  /* Trust score after quarantine */
        );
    }

    nimcp_mutex_unlock(si->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_security_release_quarantine(
    mesh_security_integration_t* si,
    mesh_participant_id_t participant
) {
    if (!si || si->magic != SECURITY_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_security_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(si->mutex);

    /* Release via MSP */
    if (si->msp) {
        mesh_msp_release_quarantine(si->msp, participant);
    }

    si->stats.quarantine_released++;

    /* Create event */
    mesh_security_event_t event = {0};
    event.type = MESH_SEC_EVENT_QUARANTINE_RELEASED;
    event.participant = participant;
    event.channel = mesh_get_channel(participant);
    event.timestamp_ns = get_time_ns();

    queue_event(si, &event);
    invoke_event_callback(si, &event);

    /* Clear threat history */
    threat_entry_t* entry = find_threat_entry(si, participant);
    if (entry) {
        entry->threat_score = entry->threat_score * 0.5f;  /* Reduce but remember */
    }

    /* Notify immune - recovery event */
    if (si->msp) {
        mesh_msp_handle_immune_event(si->msp, participant, 2);  /* Recovery */
    }

    nimcp_mutex_unlock(si->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_security_revoke_credential(
    mesh_security_integration_t* si,
    mesh_participant_id_t participant,
    const char* reason
) {
    if (!si || si->magic != SECURITY_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_security_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(si->mutex);

    /* Revoke via MSP */
    if (si->msp) {
        mesh_msp_revoke_credential(si->msp, participant, reason);
    }

    si->stats.credentials_revoked++;

    /* Create event */
    mesh_security_event_t event = {0};
    event.type = MESH_SEC_EVENT_CREDENTIAL_REVOKED;
    event.participant = participant;
    event.channel = mesh_get_channel(participant);
    event.timestamp_ns = get_time_ns();

    queue_event(si, &event);
    invoke_event_callback(si, &event);

    /* Broadcast if enabled */
    if (si->config.enable_security_broadcasts) {
        nimcp_mutex_unlock(si->mutex);
        mesh_security_broadcast_revocation(si, participant, reason);
        nimcp_mutex_lock(si->mutex);
    }

    /* Mark threat as permanent */
    threat_entry_t* entry = find_threat_entry(si, participant);
    if (entry) {
        entry->threat_score = 1.0f;
    } else {
        record_threat(si, participant, 1.0f);
    }

    nimcp_mutex_unlock(si->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Immune System Response API
 * ============================================================================ */

nimcp_error_t mesh_security_handle_immune_response(
    mesh_security_integration_t* si,
    mesh_participant_id_t participant,
    mesh_immune_action_t action,
    float inflammation_level
) {
    if (!si || si->magic != SECURITY_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_security_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(si->mutex);
    si->stats.immune_responses++;
    si->stats.current_inflammation = inflammation_level;

    /* Map action to MSP event and handle */
    int msp_event = immune_action_to_msp_event(action);

    if (si->msp && msp_event >= 0) {
        nimcp_mutex_unlock(si->mutex);
        mesh_msp_handle_immune_event(si->msp, participant, msp_event);
        nimcp_mutex_lock(si->mutex);
    }

    /* Handle specific actions */
    switch (action) {
        case MESH_IMMUNE_ACTION_QUARANTINE:
            nimcp_mutex_unlock(si->mutex);
            mesh_security_quarantine(
                si, participant,
                si->config.quarantine_duration_ms,
                "Immune system quarantine"
            );
            nimcp_mutex_lock(si->mutex);
            break;

        case MESH_IMMUNE_ACTION_REVOKE:
        case MESH_IMMUNE_ACTION_SHUTDOWN:
            nimcp_mutex_unlock(si->mutex);
            mesh_security_revoke_credential(
                si, participant, "Immune system revocation");
            nimcp_mutex_lock(si->mutex);
            break;

        default:
            break;
    }

    /* Check inflammation threshold */
    if (inflammation_level >= si->config.inflammation_threshold) {
        si->stats.inflammation_events++;

        mesh_security_event_t event = {0};
        event.type = MESH_SEC_EVENT_INFLAMMATION;
        event.participant = participant;
        event.data.immune.action = action;
        event.data.immune.inflammation_level = inflammation_level;
        event.timestamp_ns = get_time_ns();

        queue_event(si, &event);
        invoke_event_callback(si, &event);
    }

    nimcp_mutex_unlock(si->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_security_notify_recovery(
    mesh_security_integration_t* si,
    mesh_participant_id_t participant,
    bool success
) {
    if (!si || si->magic != SECURITY_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_security_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(si->mutex);

    if (success) {
        /* Release quarantine on successful recovery */
        nimcp_mutex_unlock(si->mutex);
        mesh_security_release_quarantine(si, participant);
        nimcp_mutex_lock(si->mutex);

        /* Notify immune of trust recovery */
        if (si->immune) {
            brain_immune_handle_bft_trust_recovery(
                si->immune,
                (uint32_t)participant,
                0.5f,  /* Old trust */
                0.8f   /* New trust */
            );
        }
    }

    mesh_security_event_t event = {0};
    event.type = MESH_SEC_EVENT_RECOVERY_COMPLETE;
    event.participant = participant;
    event.timestamp_ns = get_time_ns();

    queue_event(si, &event);
    invoke_event_callback(si, &event);

    nimcp_mutex_unlock(si->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Security Broadcast API
 * ============================================================================ */

nimcp_error_t mesh_security_broadcast_event(
    mesh_security_integration_t* si,
    const mesh_security_event_t* event
) {
    if (!si || si->magic != SECURITY_INTEGRATION_MAGIC || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_security_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(si->mutex);

    /* Check broadcast rate limit */
    uint64_t now = get_time_ns();
    uint64_t interval_ns = si->config.broadcast_interval_ms * 1000000ULL;

    if (now - si->last_broadcast_ns < interval_ns) {
        nimcp_mutex_unlock(si->mutex);
        return NIMCP_SUCCESS;  /* Rate limited */
    }

    si->last_broadcast_ns = now;
    si->stats.security_broadcasts++;

    /* Create security broadcast event */
    mesh_security_event_t broadcast_event = *event;
    broadcast_event.type = MESH_SEC_EVENT_SECURITY_BROADCAST;
    broadcast_event.timestamp_ns = now;

    /* Would send via mesh integration here */
    /* For now, log it */
    if (si->config.verbose_logging) {
        LOG_DEBUG("Security broadcast: type=%d participant=%lu",
                 event->type, (unsigned long)event->participant);
    }

    nimcp_mutex_unlock(si->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_security_broadcast_quarantine(
    mesh_security_integration_t* si,
    mesh_participant_id_t participant,
    uint64_t duration_ms,
    const char* reason
) {
    mesh_security_event_t event = {0};
    event.type = MESH_SEC_EVENT_QUARANTINE_ISSUED;
    event.participant = participant;
    event.channel = mesh_get_channel(participant);
    event.data.quarantine.duration_ms = duration_ms;
    event.data.quarantine.reason = reason;
    event.timestamp_ns = get_time_ns();

    return mesh_security_broadcast_event(si, &event);
}

nimcp_error_t mesh_security_broadcast_revocation(
    mesh_security_integration_t* si,
    mesh_participant_id_t participant,
    const char* reason
) {
    mesh_security_event_t event = {0};
    event.type = MESH_SEC_EVENT_CREDENTIAL_REVOKED;
    event.participant = participant;
    event.channel = mesh_get_channel(participant);
    event.data.quarantine.reason = reason;
    event.timestamp_ns = get_time_ns();

    return mesh_security_broadcast_event(si, &event);
}

/* ============================================================================
 * Callback Registration API
 * ============================================================================ */

nimcp_error_t mesh_security_set_event_callback(
    mesh_security_integration_t* si,
    mesh_security_event_cb_t callback,
    void* user_data
) {
    if (!si || si->magic != SECURITY_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_security_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(si->mutex);
    si->event_callback = callback;
    si->event_callback_data = user_data;
    nimcp_mutex_unlock(si->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_security_set_quarantine_callback(
    mesh_security_integration_t* si,
    mesh_security_quarantine_cb_t callback,
    void* user_data
) {
    if (!si || si->magic != SECURITY_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_security_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(si->mutex);
    si->quarantine_callback = callback;
    si->quarantine_callback_data = user_data;
    nimcp_mutex_unlock(si->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_security_set_bbb_validate_callback(
    mesh_security_integration_t* si,
    mesh_security_bbb_validate_cb_t callback,
    void* user_data
) {
    if (!si || si->magic != SECURITY_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_security_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(si->mutex);
    si->bbb_validate_callback = callback;
    si->bbb_validate_callback_data = user_data;
    nimcp_mutex_unlock(si->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Update and Statistics API
 * ============================================================================ */

nimcp_error_t mesh_security_update(
    mesh_security_integration_t* si,
    uint64_t delta_ms
) {
    if (!si || si->magic != SECURITY_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_security_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    (void)delta_ms;

    nimcp_mutex_lock(si->mutex);

    /* Process pending events */
    pending_event_t* prev = NULL;
    pending_event_t* event = si->pending_events_head;

    while (event) {
        if (event->processed) {
            /* Remove processed event */
            if (prev) {
                prev->next = event->next;
            } else {
                si->pending_events_head = event->next;
            }
            pending_event_t* to_free = event;
            event = event->next;
            nimcp_free(to_free);
            si->pending_count--;
        } else {
            event->processed = true;
            prev = event;
            event = event->next;
        }
    }

    /* Update MSP if available */
    if (si->msp) {
        nimcp_mutex_unlock(si->mutex);
        mesh_msp_update(si->msp, delta_ms);
        nimcp_mutex_lock(si->mutex);
    }

    /* Decay old threat entries */
    uint64_t now = get_time_ns();
    uint64_t decay_threshold_ns = 300000000000ULL;  /* 5 minutes */

    for (size_t i = 0; i < si->threat_count; i++) {
        threat_entry_t* entry = &si->threat_history[i];
        if (entry->active && (now - entry->last_seen_ns) > decay_threshold_ns) {
            entry->threat_score *= 0.9f;  /* Decay threat */
            if (entry->threat_score < 0.1f) {
                entry->active = false;
            }
        }
    }

    nimcp_mutex_unlock(si->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_security_get_stats(
    const mesh_security_integration_t* si,
    mesh_security_stats_t* stats
) {
    if (!si || si->magic != SECURITY_INTEGRATION_MAGIC || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_security_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(((mesh_security_integration_t*)si)->mutex);
    *stats = si->stats;
    nimcp_mutex_unlock(((mesh_security_integration_t*)si)->mutex);

    return NIMCP_SUCCESS;
}

void mesh_security_reset_stats(mesh_security_integration_t* si) {
    if (!si || si->magic != SECURITY_INTEGRATION_MAGIC) {
        return;
    }

    nimcp_mutex_lock(si->mutex);
    memset(&si->stats, 0, sizeof(si->stats));
    nimcp_mutex_unlock(si->mutex);
}

void mesh_security_print_status(const mesh_security_integration_t* si) {
    if (!si || si->magic != SECURITY_INTEGRATION_MAGIC) {
        printf("Security Integration: NULL or invalid\n");
        return;
    }

    printf("=== Mesh Security Integration Status ===\n");
    printf("BBB connected: %s\n", si->bbb ? "yes" : "no");
    printf("Immune connected: %s\n", si->immune ? "yes" : "no");
    printf("MSP connected: %s\n", si->msp ? "yes" : "no");
    printf("Exception bridge: %s\n", si->exception_bridge ? "yes" : "no");
    printf("\nStatistics:\n");
    printf("  Exceptions routed: %lu\n", (unsigned long)si->stats.exceptions_routed);
    printf("  Antigens presented: %lu\n", (unsigned long)si->stats.antigens_presented);
    printf("  BBB validations: %lu\n", (unsigned long)si->stats.bbb_validations);
    printf("  BBB threats detected: %lu\n", (unsigned long)si->stats.bbb_threats_detected);
    printf("  Quarantines issued: %lu\n", (unsigned long)si->stats.quarantine_issued);
    printf("  Quarantines released: %lu\n", (unsigned long)si->stats.quarantine_released);
    printf("  Credentials revoked: %lu\n", (unsigned long)si->stats.credentials_revoked);
    printf("  Immune responses: %lu\n", (unsigned long)si->stats.immune_responses);
    printf("  Current inflammation: %.2f\n", si->stats.current_inflammation);
    printf("  Threat history entries: %zu\n", si->threat_count);
    printf("  Pending events: %zu\n", si->pending_count);
}
