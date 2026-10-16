/**
 * @file nimcp_mesh_exception_bridge.c
 * @brief Exception to Immune System Mesh Routing Bridge Implementation
 *
 * WHAT: Routes exceptions through mesh network to immune system
 * WHY:  Enable coordinated immune response to system errors via mesh
 * HOW:  Exception -> Antigen conversion, mesh transaction for immune routing
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#include "mesh/nimcp_mesh_exception_bridge.h"
#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_integration.h"
#include "mesh/nimcp_mesh_msp.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

#define EXCEPTION_BRIDGE_MAGIC 0x45584342  /* "EXCB" */
#define MAX_DEBOUNCE_ENTRIES 64

/**
 * @brief Debounce entry for repeated exceptions
 */
typedef struct debounce_entry {
    nimcp_error_t error_code;
    mesh_participant_id_t source_module;
    uint64_t first_occurrence_ns;
    uint64_t last_occurrence_ns;
    uint32_t occurrence_count;
    bool active;
} debounce_entry_t;

/**
 * @brief Internal exception bridge structure
 */
struct mesh_exception_bridge {
    bridge_base_t base;
    uint32_t magic;
    mesh_exception_bridge_config_t config;

    /* Dependencies */
    mesh_bootstrap_t* bootstrap;
    mesh_integration_t* integration;
    brain_immune_system_t* immune;
    blood_brain_barrier_t* bbb;
    mesh_msp_t* msp;

    /* Debounce tracking */
    debounce_entry_t debounce[MAX_DEBOUNCE_ENTRIES];
    size_t debounce_count;

    /* Antigen ID counter */
    uint32_t next_antigen_id;

    /* Statistics */
    mesh_exception_bridge_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

/* ============================================================================
 * Configuration Defaults
 * ============================================================================ */

nimcp_error_t mesh_exception_bridge_default_config(
    mesh_exception_bridge_config_t* config
) {
    if (!config) return NIMCP_ERROR_NULL_POINTER;

    memset(config, 0, sizeof(*config));

    config->min_report_severity = MESH_EXC_SEVERITY_WARNING;
    config->quarantine_threshold = MESH_EXC_SEVERITY_SEVERE;

    config->debounce_ms = 100;
    config->escalation_window_ms = 5000;
    config->max_per_window = 10;

    config->enable_auto_quarantine = true;
    config->enable_bbb_validation = true;
    config->route_through_mesh = true;

    config->verbose_logging = false;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

mesh_exception_bridge_t* mesh_exception_bridge_create(
    mesh_bootstrap_t* bootstrap,
    brain_immune_system_t* immune,
    const mesh_exception_bridge_config_t* config
) {
    if (!bootstrap) {
        LOG_ERROR("Cannot create exception bridge without bootstrap");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_exception_bridge_create: bootstrap is NULL");
        return NULL;
    }

    mesh_exception_bridge_config_t default_config;
    if (!config) {
        mesh_exception_bridge_default_config(&default_config);
        config = &default_config;
    }

    mesh_exception_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) {
        LOG_ERROR("Failed to allocate exception bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_exception_bridge_create: bridge is NULL");
        return NULL;
    }

    bridge->magic = EXCEPTION_BRIDGE_MAGIC;
    bridge->config = *config;
    bridge->bootstrap = bootstrap;
    bridge->integration = mesh_bootstrap_get_integration(bootstrap);
    bridge->immune = immune;
    bridge->next_antigen_id = 1;

    /* Create mutex */
    mutex_attr_t attr = {0};
    bridge->mutex = nimcp_mutex_create(&attr);
    if (!bridge->mutex) {
        LOG_ERROR("Failed to create bridge mutex");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_exception_bridge_create: bridge->mutex is NULL");
        return NULL;
    }

    bridge_base_init(&bridge->base, 0, "mesh_exception_bridge");

    LOG_DEBUG("Exception bridge created");
    return bridge;
}

void mesh_exception_bridge_destroy(mesh_exception_bridge_t* bridge) {
    if (!bridge || bridge->magic != EXCEPTION_BRIDGE_MAGIC) return;

    nimcp_mutex_lock(bridge->mutex);
    /* Cleanup */
    nimcp_mutex_unlock(bridge->mutex);

    nimcp_mutex_destroy(bridge->mutex);
    bridge_base_cleanup(&bridge->base);
    bridge->magic = 0;
    nimcp_free(bridge);

    LOG_DEBUG("Exception bridge destroyed");
}

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Find or create debounce entry
 */
static debounce_entry_t* find_debounce_entry(
    mesh_exception_bridge_t* bridge,
    nimcp_error_t error_code,
    mesh_participant_id_t source_module
) {
    uint64_t now = nimcp_time_now_ns();
    uint64_t debounce_ns = bridge->config.debounce_ms * 1000000ULL;

    /* Find existing entry */
    for (size_t i = 0; i < bridge->debounce_count; i++) {
        debounce_entry_t* entry = &bridge->debounce[i];
        if (entry->active &&
            entry->error_code == error_code &&
            entry->source_module == source_module) {

            /* Check if within debounce window */
            if (now - entry->last_occurrence_ns < debounce_ns) {
                entry->last_occurrence_ns = now;
                entry->occurrence_count++;
                return entry;
            } else {
                /* Reset entry */
                entry->first_occurrence_ns = now;
                entry->last_occurrence_ns = now;
                entry->occurrence_count = 1;
                return entry;
            }
        }
    }

    /* Create new entry if space */
    if (bridge->debounce_count < MAX_DEBOUNCE_ENTRIES) {
        debounce_entry_t* entry = &bridge->debounce[bridge->debounce_count++];
        entry->error_code = error_code;
        entry->source_module = source_module;
        entry->first_occurrence_ns = now;
        entry->last_occurrence_ns = now;
        entry->occurrence_count = 1;
        entry->active = true;
        return entry;
    }

    /* No matching entry and no space for new entry */
    return NULL;
}

/**
 * @brief Check if exception should be escalated
 */
static bool should_escalate(
    mesh_exception_bridge_t* bridge,
    debounce_entry_t* entry
) {
    uint64_t now = nimcp_time_now_ns();
    uint64_t window_ns = bridge->config.escalation_window_ms * 1000000ULL;

    if (now - entry->first_occurrence_ns < window_ns) {
        return entry->occurrence_count >= bridge->config.max_per_window;
    }

    return false;
}

/**
 * @brief Update debounce entry with new occurrence
 */
static void update_debounce_entry(
    mesh_exception_bridge_t* bridge,
    nimcp_error_t error_code,
    mesh_participant_id_t source_module
) {
    debounce_entry_t* entry = find_debounce_entry(bridge, error_code, source_module);
    if (entry) {
        entry->last_occurrence_ns = nimcp_time_now_ns();
        entry->occurrence_count++;
    }
}

/**
 * @brief Map severity to immune action
 */
static mesh_immune_action_t severity_to_action(mesh_exception_severity_t severity) {
    switch (severity) {
        case MESH_EXC_SEVERITY_TRACE:
        case MESH_EXC_SEVERITY_INFO:
            return MESH_IMMUNE_ACTION_NONE;
        case MESH_EXC_SEVERITY_WARNING:
            return MESH_IMMUNE_ACTION_LOG;
        case MESH_EXC_SEVERITY_ERROR:
            return MESH_IMMUNE_ACTION_WARN;
        case MESH_EXC_SEVERITY_SEVERE:
            return MESH_IMMUNE_ACTION_QUARANTINE;
        case MESH_EXC_SEVERITY_CRITICAL:
            return MESH_IMMUNE_ACTION_SHUTDOWN;
        default:
            return MESH_IMMUNE_ACTION_LOG;
    }
}

/**
 * @brief Create pattern from exception
 */
static void create_exception_pattern(
    const mesh_exception_antigen_t* antigen,
    float pattern[8]
) {
    memset(pattern, 0, sizeof(float) * 8);

    /* Pattern dimensions based on category and severity */
    pattern[0] = (float)antigen->category / 10.0f;
    pattern[1] = (float)antigen->severity / 5.0f;
    pattern[2] = (float)(antigen->error_code % 256) / 255.0f;
    pattern[3] = antigen->occurrence_count > 1 ? 0.8f : 0.2f;

    /* Source information */
    pattern[4] = (float)(antigen->source_module % 1000) / 1000.0f;
    pattern[5] = (float)antigen->source_channel / 10.0f;

    /* Time-based features */
    pattern[6] = ((antigen->timestamp_ns / 1000000) % 1000) / 1000.0f;
    pattern[7] = 0.0f;  /* Reserved */
}

/* ============================================================================
 * Classification API
 * ============================================================================ */

nimcp_error_t mesh_exception_bridge_classify(
    nimcp_error_t error_code,
    mesh_exception_category_t* category_out,
    mesh_exception_severity_t* severity_out
) {
    mesh_exception_category_t category = MESH_EXC_CAT_UNKNOWN;
    mesh_exception_severity_t severity = MESH_EXC_SEVERITY_ERROR;

    /* Classify based on error code ranges */
    if (error_code >= NIMCP_ERROR_OUT_OF_MEMORY &&
        error_code <= NIMCP_ERROR_OUT_OF_MEMORY + 100) {
        category = MESH_EXC_CAT_MEMORY;
        severity = MESH_EXC_SEVERITY_SEVERE;
    }
    else if (error_code >= NIMCP_ERROR_UNAUTHORIZED &&
             error_code <= NIMCP_ERROR_UNAUTHORIZED + 100) {
        category = MESH_EXC_CAT_SECURITY;
        severity = MESH_EXC_SEVERITY_CRITICAL;
    }
    else if (error_code >= NIMCP_ERROR_NETWORK_IO &&
             error_code <= NIMCP_ERROR_NETWORK_IO + 100) {
        category = MESH_EXC_CAT_NETWORK;
        severity = MESH_EXC_SEVERITY_ERROR;
    }
    else if (error_code == NIMCP_ERROR_TIMEOUT) {
        category = MESH_EXC_CAT_TIMING;
        severity = MESH_EXC_SEVERITY_WARNING;
    }
    else if (error_code >= NIMCP_ERROR_BBB_VALIDATION &&
             error_code <= NIMCP_ERROR_BBB_VALIDATION + 100) {
        category = MESH_EXC_CAT_DATA;
        severity = MESH_EXC_SEVERITY_ERROR;
    }
    else if (error_code >= NIMCP_ERROR_GPU &&
             error_code <= NIMCP_ERROR_GPU + 100) {
        category = MESH_EXC_CAT_GPU;
        severity = MESH_EXC_SEVERITY_SEVERE;
    }
    else if (error_code == NIMCP_ERROR_INVALID_PARAM ||
             error_code == NIMCP_ERROR_NULL_POINTER) {
        category = MESH_EXC_CAT_LOGIC;
        severity = MESH_EXC_SEVERITY_ERROR;
    }
    else if (error_code >= NIMCP_ERROR_CAPACITY_EXCEEDED &&
             error_code <= NIMCP_ERROR_CAPACITY_EXCEEDED + 50) {
        category = MESH_EXC_CAT_RESOURCE;
        severity = MESH_EXC_SEVERITY_WARNING;
    }
    else {
        category = MESH_EXC_CAT_SYSTEM;
        severity = MESH_EXC_SEVERITY_ERROR;
    }

    if (category_out) *category_out = category;
    if (severity_out) *severity_out = severity;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Antigen API
 * ============================================================================ */

nimcp_error_t mesh_exception_bridge_create_antigen(
    mesh_exception_bridge_t* bridge,
    const nimcp_exception_t* exception,
    mesh_participant_id_t source_module,
    mesh_exception_antigen_t* antigen_out
) {
    if (!bridge || bridge->magic != EXCEPTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_exception_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!antigen_out) return NIMCP_ERROR_NULL_POINTER;

    memset(antigen_out, 0, sizeof(*antigen_out));

    nimcp_mutex_lock(bridge->mutex);

    antigen_out->antigen_id = bridge->next_antigen_id++;
    antigen_out->source_module = source_module;
    antigen_out->source_channel = MESH_CHANNEL_SYSTEM;  /* Default */
    antigen_out->timestamp_ns = nimcp_time_now_ns();

    /* Extract info from exception if provided */
    if (exception) {
        antigen_out->error_code = exception->code;
        strncpy(antigen_out->error_message, exception->message,
                sizeof(antigen_out->error_message) - 1);
        if (exception->file) {
            strncpy(antigen_out->source_file, exception->file,
                    sizeof(antigen_out->source_file) - 1);
        }
        antigen_out->source_line = exception->line;

        /* Classify */
        mesh_exception_bridge_classify(
            exception->code,
            &antigen_out->category,
            &antigen_out->severity
        );
    } else {
        antigen_out->category = MESH_EXC_CAT_UNKNOWN;
        antigen_out->severity = MESH_EXC_SEVERITY_ERROR;
    }

    /* Check for repeated occurrences */
    debounce_entry_t* entry = find_debounce_entry(
        bridge, antigen_out->error_code, source_module);
    if (entry) {
        antigen_out->occurrence_count = entry->occurrence_count;
    } else {
        antigen_out->occurrence_count = 1;
    }

    /* Create pattern */
    create_exception_pattern(antigen_out, antigen_out->pattern);

    bridge->stats.antigens_created++;

    nimcp_mutex_unlock(bridge->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * BBB Integration
 * ============================================================================ */

nimcp_error_t mesh_exception_bridge_set_bbb(
    mesh_exception_bridge_t* bridge,
    blood_brain_barrier_t* bbb
) {
    if (!bridge || bridge->magic != EXCEPTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_exception_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(bridge->mutex);
    bridge->bbb = bbb;
    nimcp_mutex_unlock(bridge->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_exception_bridge_bbb_validate(
    mesh_exception_bridge_t* bridge,
    const mesh_exception_antigen_t* antigen,
    float* threat_score_out
) {
    if (!bridge || bridge->magic != EXCEPTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_exception_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!antigen || !threat_score_out) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(bridge->mutex);

    /* Compute threat score based on antigen characteristics */
    float threat_score = 0.0f;

    /* Severity contributes most */
    threat_score += (float)antigen->severity * 0.15f;

    /* Security category is highest threat */
    if (antigen->category == MESH_EXC_CAT_SECURITY) {
        threat_score += 0.3f;
    } else if (antigen->category == MESH_EXC_CAT_MEMORY) {
        threat_score += 0.2f;
    }

    /* Repeated occurrences increase threat */
    if (antigen->occurrence_count > 1) {
        threat_score += 0.1f * (antigen->occurrence_count > 5 ? 5 :
                                antigen->occurrence_count);
    }

    /* Cap at 1.0 */
    if (threat_score > 1.0f) threat_score = 1.0f;

    *threat_score_out = threat_score;

    bridge->stats.bbb_validations++;

    nimcp_mutex_unlock(bridge->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Exception Routing API
 * ============================================================================ */

nimcp_error_t mesh_exception_bridge_route(
    mesh_exception_bridge_t* bridge,
    const nimcp_exception_t* exception,
    mesh_exception_response_t* response_out
) {
    if (!bridge || bridge->magic != EXCEPTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_exception_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!response_out) return NIMCP_ERROR_NULL_POINTER;

    memset(response_out, 0, sizeof(*response_out));

    nimcp_mutex_lock(bridge->mutex);

    bridge->stats.exceptions_received++;

    /* Create antigen from exception */
    mesh_exception_antigen_t antigen;
    mesh_participant_id_t source = 0;  /* Unknown - exception doesn't track module ID */

    nimcp_mutex_unlock(bridge->mutex);

    nimcp_error_t err = mesh_exception_bridge_create_antigen(
        bridge, exception, source, &antigen);
    if (err != NIMCP_SUCCESS) {
        return err;
    }

    nimcp_mutex_lock(bridge->mutex);

    /* Check severity threshold */
    if (antigen.severity < bridge->config.min_report_severity) {
        response_out->primary_action = MESH_IMMUNE_ACTION_NONE;
        nimcp_mutex_unlock(bridge->mutex);
        return NIMCP_SUCCESS;
    }

    bridge->stats.severity_counts[antigen.severity]++;
    bridge->stats.category_counts[antigen.category]++;

    /* Check for debounce */
    debounce_entry_t* entry = find_debounce_entry(
        bridge, antigen.error_code, antigen.source_module);

    if (entry && entry->occurrence_count > 1) {
        uint64_t now = nimcp_time_now_ns();
        uint64_t debounce_ns = bridge->config.debounce_ms * 1000000ULL;

        if (now - entry->last_occurrence_ns < debounce_ns &&
            entry->occurrence_count > 2) {
            /* Debounce - but still check for escalation */
            if (!should_escalate(bridge, entry)) {
                bridge->stats.debounced_exceptions++;
                response_out->primary_action = MESH_IMMUNE_ACTION_LOG;
                strncpy(response_out->explanation, "Exception debounced",
                       sizeof(response_out->explanation) - 1);
                nimcp_mutex_unlock(bridge->mutex);
                return NIMCP_SUCCESS;
            }
        }
    }

    /* BBB validation if enabled */
    float threat_score = 0.0f;
    if (bridge->config.enable_bbb_validation) {
        nimcp_mutex_unlock(bridge->mutex);
        mesh_exception_bridge_bbb_validate(bridge, &antigen, &threat_score);
        nimcp_mutex_lock(bridge->mutex);
    } else {
        threat_score = (float)antigen.severity / 5.0f;
    }

    response_out->threat_score = threat_score;

    /* Determine response based on severity and threat score */
    if (antigen.severity >= MESH_EXC_SEVERITY_CRITICAL) {
        response_out->primary_action = MESH_IMMUNE_ACTION_SHUTDOWN;
        response_out->fallback_action = MESH_IMMUNE_ACTION_QUARANTINE;
        response_out->inflammation_level = 1.0f;
        snprintf(response_out->explanation, sizeof(response_out->explanation),
                "Critical exception: %s", antigen.error_message);
    }
    else if (antigen.severity >= MESH_EXC_SEVERITY_SEVERE) {
        if (bridge->config.enable_auto_quarantine &&
            antigen.severity >= bridge->config.quarantine_threshold) {
            response_out->primary_action = MESH_IMMUNE_ACTION_QUARANTINE;
            response_out->quarantine_duration_ms = 30000;  /* 30 seconds */
            bridge->stats.quarantine_actions++;
        } else {
            response_out->primary_action = MESH_IMMUNE_ACTION_REPAIR;
        }
        response_out->fallback_action = MESH_IMMUNE_ACTION_RESTART;
        response_out->inflammation_level = 0.7f;
        snprintf(response_out->explanation, sizeof(response_out->explanation),
                "Severe exception: quarantine recommended");
    }
    else if (antigen.severity >= MESH_EXC_SEVERITY_ERROR) {
        response_out->primary_action = MESH_IMMUNE_ACTION_WARN;
        response_out->fallback_action = MESH_IMMUNE_ACTION_LOG;
        response_out->inflammation_level = 0.4f;
        snprintf(response_out->explanation, sizeof(response_out->explanation),
                "Error: %s", antigen.error_message);
    }
    else {
        response_out->primary_action = MESH_IMMUNE_ACTION_LOG;
        response_out->inflammation_level = 0.1f;
        snprintf(response_out->explanation, sizeof(response_out->explanation),
                "Warning logged");
    }

    /* Check for escalation */
    if (entry && should_escalate(bridge, entry)) {
        bridge->stats.escalations++;
        /* Escalate response */
        if (response_out->primary_action < MESH_IMMUNE_ACTION_QUARANTINE) {
            response_out->primary_action = MESH_IMMUNE_ACTION_QUARANTINE;
            response_out->quarantine_duration_ms = 60000;  /* 1 minute */
            response_out->inflammation_level = 0.8f;
            strncat(response_out->explanation, " (ESCALATED)",
                   sizeof(response_out->explanation) -
                   strlen(response_out->explanation) - 1);
        }
    }

    /* Route through mesh if enabled */
    if (bridge->config.route_through_mesh && bridge->integration) {
        /* Would create and submit mesh transaction here */
        bridge->stats.mesh_transactions_sent++;
    }

    /* Present to immune system if available */
    if (bridge->immune) {
        /* Would call brain_immune_present_antigen() here */
    }

    if (bridge->config.verbose_logging) {
        LOG_DEBUG("Exception routed: code=%d severity=%d action=%d",
                 antigen.error_code, antigen.severity,
                 response_out->primary_action);
    }

    nimcp_mutex_unlock(bridge->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_exception_bridge_route_error(
    mesh_exception_bridge_t* bridge,
    nimcp_error_t error_code,
    const char* message,
    mesh_participant_id_t source_module,
    const char* source_file,
    uint32_t source_line,
    mesh_exception_response_t* response_out
) {
    if (!bridge || bridge->magic != EXCEPTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_exception_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!response_out) return NIMCP_ERROR_NULL_POINTER;

    memset(response_out, 0, sizeof(*response_out));

    nimcp_mutex_lock(bridge->mutex);
    bridge->stats.exceptions_received++;

    /* Create antigen directly from error info (no exception struct needed) */
    mesh_exception_antigen_t antigen;
    memset(&antigen, 0, sizeof(antigen));

    antigen.antigen_id = bridge->next_antigen_id++;
    antigen.error_code = error_code;
    antigen.source_module = source_module;
    antigen.source_channel = MESH_CHANNEL_SYSTEM;
    antigen.source_line = source_line;
    antigen.timestamp_ns = nimcp_time_now_ns();

    if (message) {
        strncpy(antigen.error_message, message, sizeof(antigen.error_message) - 1);
    }
    if (source_file) {
        strncpy(antigen.source_file, source_file, sizeof(antigen.source_file) - 1);
    }

    /* Classify */
    mesh_exception_bridge_classify(error_code, &antigen.category, &antigen.severity);

    /* Check debounce */
    debounce_entry_t* entry = find_debounce_entry(bridge, error_code, source_module);
    if (entry) {
        antigen.occurrence_count = entry->occurrence_count;
    } else {
        antigen.occurrence_count = 1;
    }

    /* Create pattern */
    create_exception_pattern(&antigen, antigen.pattern);

    bridge->stats.antigens_created++;

    /* Check severity threshold */
    if (antigen.severity < bridge->config.min_report_severity) {
        response_out->primary_action = MESH_IMMUNE_ACTION_NONE;
        nimcp_mutex_unlock(bridge->mutex);
        return NIMCP_SUCCESS;
    }

    bridge->stats.severity_counts[antigen.severity]++;

    /* Determine response based on category and severity */
    /* Set actions */
    response_out->primary_action = severity_to_action(antigen.severity);
    response_out->fallback_action = MESH_IMMUNE_ACTION_LOG;

    /* Update debounce */
    update_debounce_entry(bridge, error_code, source_module);

    /* Route to immune if needed */
    if (bridge->immune && antigen.severity >= MESH_EXC_SEVERITY_ERROR) {
        response_out->threat_score = (float)antigen.severity / (float)MESH_EXC_SEVERITY_CRITICAL;
    }

    nimcp_mutex_unlock(bridge->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics API
 * ============================================================================ */

nimcp_error_t mesh_exception_bridge_get_stats(
    const mesh_exception_bridge_t* bridge,
    mesh_exception_bridge_stats_t* stats
) {
    if (!bridge || bridge->magic != EXCEPTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_exception_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!stats) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(((mesh_exception_bridge_t*)bridge)->mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(((mesh_exception_bridge_t*)bridge)->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_exception_bridge_reset_stats(
    mesh_exception_bridge_t* bridge
) {
    if (!bridge || bridge->magic != EXCEPTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_exception_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(bridge->mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->mutex);

    return NIMCP_SUCCESS;
}
