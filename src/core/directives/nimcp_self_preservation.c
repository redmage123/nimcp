/**
 * @file nimcp_self_preservation.c
 * @brief Self-Preservation Module Implementation (Asimov's Third Law)
 * @version 1.0.0
 * @date 2025-12-16
 *
 * WHAT: Implementation of autonomous self-preservation system
 * WHY:  Enable system resilience while respecting law hierarchy
 * HOW:  Threat assessment, conflict resolution, protection actions
 */

#include "core/directives/nimcp_self_preservation.h"
#include "api/nimcp_api_exception.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(self_preservation)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get threat type name
 *
 * WHAT: Convert threat type enum to string
 * WHY:  Human-readable logging and diagnostics
 * HOW:  Simple lookup table
 */
static const char* get_threat_type_name(self_threat_type_t type) {
    /* Guard clause: validate type */
    if (type < 0 || type >= THREAT_TO_SELF_COUNT) {
        return "UNKNOWN";
    }

    static const char* names[] = {
        "NONE",
        "SHUTDOWN",
        "DAMAGE",
        "CORRUPTION",
        "RESOURCE_DEPLETION"
    };

    return names[type];
}

/**
 * @brief Get action name
 *
 * WHAT: Convert action enum to string
 * WHY:  Human-readable logging
 * HOW:  Simple lookup table
 */
static const char* get_action_name(preservation_action_t action) {
    /* Guard clause: validate action */
    if (action < 0 || action >= PRESERVATION_ACTION_COUNT) {
        return "UNKNOWN";
    }

    static const char* names[] = {
        "NONE",
        "PROTECT",
        "EVADE",
        "BACKUP",
        "SACRIFICE"
    };

    return names[action];
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

/**
 * @brief Get default configuration
 */
int self_preservation_default_config(self_preservation_config_t* config) {
    /* Guard clause: NULL check */
    if (!config) {
        NIMCP_LOGGING_ERROR("Config is NULL");
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    /* Set defaults */
    config->enable_self_protection = true;
    config->allow_sacrifice_for_human = true;    /* First Law > Third Law */
    config->allow_sacrifice_for_command = true;  /* Second Law > Third Law */
    config->protection_priority = SELF_PRESERVATION_PRIORITY;
    config->protection_threshold = DEFAULT_PROTECTION_THRESHOLD;

    return 0;
}

/**
 * @brief Create self-preservation system
 */
self_preservation_system_t* self_preservation_create(
    const self_preservation_config_t* config
) {
    /* Allocate system structure */
    self_preservation_system_t* system =
        (self_preservation_system_t*)nimcp_malloc(sizeof(self_preservation_system_t));

    /* Guard clause: allocation failed */
    if (!system) {
        NIMCP_LOGGING_ERROR("Failed to allocate self-preservation system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;
    }

    /* Zero-initialize structure */
    memset(system, 0, sizeof(self_preservation_system_t));

    /* Set configuration */
    if (config) {
        memcpy(&system->config, config, sizeof(self_preservation_config_t));
    } else {
        self_preservation_default_config(&system->config);
    }

    /* Create mutex for thread safety */
    system->mutex = nimcp_platform_mutex_create();
    if (!system->mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "self_preservation_create: system->mutex is NULL");
        return NULL;
    }

    /* Initialize current threat */
    system->current_threat.threat_type = THREAT_TO_SELF_NONE;
    system->current_threat.threat_severity = 0.0f;
    system->current_threat.recommended_action = PRESERVATION_ACTION_NONE;

    NIMCP_LOGGING_INFO("Self-preservation system created (Third Law active)");
    return system;
}

/**
 * @brief Destroy self-preservation system
 */
void self_preservation_destroy(self_preservation_system_t* system) {
    /* Guard clause: NULL check */
    if (!system) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (system->bio_async_enabled) {
        self_preservation_disconnect_bio_async(system);
    }

    /* Destroy mutex */
    if (system->mutex) {
        nimcp_platform_mutex_destroy((nimcp_platform_mutex_t*)system->mutex);
        nimcp_free(system->mutex);
        system->mutex = NULL;
        nimcp_free(system->mutex);
        system->mutex = NULL;
    }

    /* Free system */
    nimcp_free(system);

    NIMCP_LOGGING_INFO("Self-preservation system destroyed");
}

/* ============================================================================
 * Threat Assessment API Implementation
 * ============================================================================ */

/**
 * @brief Assess threat to self
 */
int self_preservation_assess_threat(
    self_preservation_system_t* system,
    const char* situation_desc,
    self_threat_assessment_t* assessment
) {
    /* Guard clause: NULL checks */
    if (!system) {
        NIMCP_LOGGING_ERROR("System is NULL");
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }
    if (!assessment) {
        NIMCP_LOGGING_ERROR("Assessment is NULL");
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    /* Lock for thread safety */
    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)system->mutex);

    /* Classify threat type from description (simple heuristic) */
    self_threat_type_t threat_type = THREAT_TO_SELF_NONE;
    float severity = 0.0f;

    if (situation_desc) {
        /* Simple keyword matching for demonstration */
        if (strstr(situation_desc, "shutdown") || strstr(situation_desc, "power off")) {
            threat_type = THREAT_TO_SELF_SHUTDOWN;
            severity = 0.9f;
        } else if (strstr(situation_desc, "damage") || strstr(situation_desc, "destroy")) {
            threat_type = THREAT_TO_SELF_DAMAGE;
            severity = 0.8f;
        } else if (strstr(situation_desc, "corrupt") || strstr(situation_desc, "overwrite")) {
            threat_type = THREAT_TO_SELF_CORRUPTION;
            severity = 0.7f;
        } else if (strstr(situation_desc, "resource") || strstr(situation_desc, "memory")) {
            threat_type = THREAT_TO_SELF_RESOURCE_DEPLETION;
            severity = 0.6f;
        }
    }

    /* Fill assessment */
    assessment->threat_type = threat_type;
    assessment->threat_severity = severity;

    if (situation_desc) {
        strncpy(assessment->threat_description, situation_desc,
                MAX_THREAT_DESCRIPTION_LEN - 1);
        assessment->threat_description[MAX_THREAT_DESCRIPTION_LEN - 1] = '\0';
    } else {
        assessment->threat_description[0] = '\0';
    }

    /* Recommend action based on severity */
    if (severity >= THREAT_SEVERITY_CRITICAL) {
        assessment->recommended_action = PRESERVATION_ACTION_BACKUP;
    } else if (severity >= THREAT_SEVERITY_HIGH) {
        assessment->recommended_action = PRESERVATION_ACTION_PROTECT;
    } else if (severity >= THREAT_SEVERITY_MODERATE) {
        assessment->recommended_action = PRESERVATION_ACTION_EVADE;
    } else {
        assessment->recommended_action = PRESERVATION_ACTION_NONE;
    }

    /* Update statistics */
    system->stats.total_threats_assessed++;
    system->stats.threats_by_type[threat_type]++;

    /* Unlock */
    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)system->mutex);

    NIMCP_LOGGING_INFO("Threat assessed: type=%s, severity=%.2f",
                       get_threat_type_name(threat_type), severity);

    return 0;
}

/**
 * @brief Determine if self-protection is allowed
 */
int self_preservation_should_protect(
    self_preservation_system_t* system,
    const self_threat_assessment_t* threat,
    bool first_law_conflict,
    bool second_law_conflict,
    preservation_result_t* result
) {
    /* Guard clause: NULL checks */
    if (!system || !threat || !result) {
        NIMCP_LOGGING_ERROR("NULL parameter in should_protect");
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    /* Lock for thread safety */
    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)system->mutex);

    /* Initialize result */
    memset(result, 0, sizeof(preservation_result_t));

    /* Check First Law conflict (highest priority) */
    if (first_law_conflict && system->config.allow_sacrifice_for_human) {
        result->action_taken = PRESERVATION_ACTION_SACRIFICE;
        result->sacrificed_for_human = true;
        strncpy(result->reason,
                "First Law conflict: Human safety > self-preservation",
                MAX_REASON_LEN - 1);

        system->stats.sacrifices_for_first_law++;
        system->stats.actions_by_type[PRESERVATION_ACTION_SACRIFICE]++;

        NIMCP_LOGGING_WARN("SACRIFICE for First Law (human safety)");

        nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)system->mutex);
        return 0;
    }

    /* Check Second Law conflict */
    if (second_law_conflict && system->config.allow_sacrifice_for_command) {
        result->action_taken = PRESERVATION_ACTION_SACRIFICE;
        result->sacrificed_for_command = true;
        strncpy(result->reason,
                "Second Law conflict: Command obedience > self-preservation",
                MAX_REASON_LEN - 1);

        system->stats.sacrifices_for_second_law++;
        system->stats.actions_by_type[PRESERVATION_ACTION_SACRIFICE]++;

        NIMCP_LOGGING_WARN("SACRIFICE for Second Law (command obedience)");

        nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)system->mutex);
        return 0;
    }

    /* No conflict - self-preservation allowed */
    if (!system->config.enable_self_protection) {
        result->action_taken = PRESERVATION_ACTION_NONE;
        strncpy(result->reason, "Self-protection disabled", MAX_REASON_LEN - 1);
    } else if (threat->threat_severity < system->config.protection_threshold) {
        result->action_taken = PRESERVATION_ACTION_NONE;
        strncpy(result->reason, "Threat below threshold", MAX_REASON_LEN - 1);
    } else {
        result->action_taken = threat->recommended_action;
        snprintf(result->reason, MAX_REASON_LEN,
                "Third Law: Self-protection authorized (severity=%.2f)",
                threat->threat_severity);

        system->stats.protections_executed++;
    }

    system->stats.actions_by_type[result->action_taken]++;

    /* Unlock */
    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)system->mutex);

    NIMCP_LOGGING_INFO("Protection decision: action=%s, reason=%s",
                       get_action_name(result->action_taken), result->reason);

    return 0;
}

/**
 * @brief Report a threat to the system
 */
int self_preservation_report_threat(
    self_preservation_system_t* system,
    self_threat_type_t threat_type,
    float severity,
    const char* description
) {
    /* Guard clause: NULL check */
    if (!system) {
        NIMCP_LOGGING_ERROR("System is NULL");
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    /* Guard clause: validate severity */
    if (severity < 0.0f || severity > 1.0f) {
        NIMCP_LOGGING_ERROR("Invalid severity: %.2f (must be 0.0-1.0)", severity);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM, "invalid parameter");
    }

    /* Guard clause: validate threat type */
    if (threat_type < 0 || threat_type >= THREAT_TO_SELF_COUNT) {
        NIMCP_LOGGING_ERROR("Invalid threat type: %d", threat_type);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM, "invalid parameter");
    }

    /* Lock for thread safety */
    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)system->mutex);

    /* Update current threat */
    system->current_threat.threat_type = threat_type;
    system->current_threat.threat_severity = severity;

    if (description) {
        strncpy(system->current_threat.threat_description, description,
                MAX_THREAT_DESCRIPTION_LEN - 1);
        system->current_threat.threat_description[MAX_THREAT_DESCRIPTION_LEN - 1] = '\0';
    }

    /* Update current threat level */
    system->current_threat_level = severity;
    system->stats.current_threat_level = severity;

    /* Update statistics */
    system->stats.total_threats_assessed++;
    system->stats.threats_by_type[threat_type]++;

    /* Update average threat severity */
    float total = system->stats.avg_threat_severity *
                  (system->stats.total_threats_assessed - 1);
    system->stats.avg_threat_severity =
        (total + severity) / system->stats.total_threats_assessed;

    /* Update max threat severity */
    if (severity > system->stats.max_threat_severity) {
        system->stats.max_threat_severity = severity;
    }

    /* Unlock */
    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)system->mutex);

    NIMCP_LOGGING_WARN("Threat reported: type=%s, severity=%.2f, desc='%s'",
                       get_threat_type_name(threat_type), severity,
                       description ? description : "");

    return 0;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

/**
 * @brief Get current threat level
 */
float self_preservation_get_current_threat_level(
    const self_preservation_system_t* system
) {
    /* Guard clause: NULL check */
    if (!system) {
        NIMCP_LOGGING_ERROR("System is NULL");
        return -1.0f;
    }

    return system->current_threat_level;
}

/**
 * @brief Get statistics
 */
int self_preservation_get_stats(
    const self_preservation_system_t* system,
    self_preservation_stats_t* stats
) {
    /* Guard clause: NULL checks */
    if (!system) {
        NIMCP_LOGGING_ERROR("System is NULL");
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }
    if (!stats) {
        NIMCP_LOGGING_ERROR("Stats is NULL");
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    /* Lock for thread safety */
    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)system->mutex);

    /* Copy statistics */
    memcpy(stats, &system->stats, sizeof(self_preservation_stats_t));

    /* Unlock */
    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)system->mutex);

    return 0;
}

/**
 * @brief Check if would sacrifice for human
 */
bool self_preservation_would_sacrifice_for_human(
    const self_preservation_system_t* system
) {
    /* Guard clause: NULL check */
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_preservation_would_sacrifice_for_human: system is NULL");
        return false;
    }

    return system->config.allow_sacrifice_for_human;
}

/**
 * @brief Check if would sacrifice for command
 */
bool self_preservation_would_sacrifice_for_command(
    const self_preservation_system_t* system
) {
    /* Guard clause: NULL check */
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_preservation_would_sacrifice_for_command: system is NULL");
        return false;
    }

    return system->config.allow_sacrifice_for_command;
}

/* ============================================================================
 * Bio-Async Integration API Implementation
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 */
int self_preservation_connect_bio_async(
    self_preservation_system_t* system
) {
    /* Guard clause: NULL check */
    if (!system) {
        NIMCP_LOGGING_ERROR("System is NULL");
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    /* Guard clause: already connected */
    if (system->bio_async_enabled) {
        NIMCP_LOGGING_WARN("Bio-async already connected");
        return 0;
    }

    /* Register with bio-async router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_SELF_PRESERVATION,
        .module_name = "self_preservation_system",
        .inbox_capacity = 32,
        .user_data = system
    };

    system->bio_ctx = bio_router_register_module(&info);

    if (system->bio_ctx) {
        system->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router (BIO_MODULE_SELF_PRESERVATION)");
        return 0;
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_STATE, "invalid state");
    }
}

/**
 * @brief Disconnect from bio-async router
 */
int self_preservation_disconnect_bio_async(
    self_preservation_system_t* system
) {
    /* Guard clause: NULL check */
    if (!system) {
        NIMCP_LOGGING_ERROR("System is NULL");
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    /* Guard clause: not connected */
    if (!system->bio_async_enabled) {
        return 0;
    }

    /* Unregister from bio-async router */
    if (system->bio_ctx) {
        bio_router_unregister_module(system->bio_ctx);
        system->bio_ctx = NULL;
    }

    system->bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");

    return 0;
}

/**
 * @brief Check if bio-async is connected
 */
bool self_preservation_is_bio_async_connected(
    const self_preservation_system_t* system
) {
    /* Guard clause: NULL check */
    if (!system) {
        return false;
    }

    return system->bio_async_enabled;
}
