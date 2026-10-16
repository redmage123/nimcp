/**
 * @file nimcp_core_directives_immune_bridge.c
 * @brief Core Directives-Immune System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-16
 */

#include "core/directives/nimcp_core_directives_immune_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(core_directives_immune_bridge)

/* NOTE: directive_immune_bridge_t struct is defined in the header file */

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Compute strictness modifier from inflammation
 *
 * WHAT: Map inflammation level to directive strictness factor
 * WHY:  Higher inflammation → stricter enforcement
 * HOW:  Use constants for each inflammation level
 */
static float compute_strictness_from_inflammation(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return INFLAMMATION_NONE_STRICTNESS_FACTOR;
        case INFLAMMATION_LOCAL:    return INFLAMMATION_LOCAL_STRICTNESS_FACTOR;
        case INFLAMMATION_REGIONAL: return INFLAMMATION_REGIONAL_STRICTNESS_FACTOR;
        case INFLAMMATION_SYSTEMIC: return INFLAMMATION_SYSTEMIC_STRICTNESS_FACTOR;
        case INFLAMMATION_STORM:    return INFLAMMATION_STORM_STRICTNESS_FACTOR;
        default:                    return 1.0f;
    }
}

/**
 * @brief Map violation severity to cytokine type
 *
 * WHAT: Determine which cytokine to release for threat level
 * WHY:  Different severities trigger different immune responses
 * HOW:  Map severity ranges to cytokine types
 */
static brain_cytokine_type_t map_severity_to_cytokine(float severity) {
    if (severity >= DIRECTIVE_CRITICAL_VIOLATION_SEVERITY) {
        return BRAIN_CYTOKINE_TNF;  /* Critical → TNF-α */
    } else if (severity >= DIRECTIVE_SEVERE_VIOLATION_SEVERITY) {
        return BRAIN_CYTOKINE_IL6;  /* Severe → IL-6 */
    } else if (severity >= DIRECTIVE_MODERATE_VIOLATION_SEVERITY) {
        return BRAIN_CYTOKINE_IL1;  /* Moderate → IL-1β */
    } else {
        return BRAIN_CYTOKINE_IL1;  /* Mild → IL-1β */
    }
}

/**
 * @brief Create threat signature from action string
 *
 * WHAT: Generate epitope-sized signature from action description
 * WHY:  Enable immune memory formation for action patterns
 * HOW:  Hash action string into fixed-size signature
 */
static void create_threat_signature(const char* action, const char* reason,
                                    uint8_t* signature, size_t* sig_len) {
    /* Simple hash: combine action and reason strings */
    size_t action_len = action ? strlen(action) : 0;
    size_t reason_len = reason ? strlen(reason) : 0;
    size_t total_len = action_len + reason_len;

    if (total_len == 0) {
        *sig_len = 0;
        return;
    }

    /* Create signature by interleaving bytes from action and reason */
    size_t sig_idx = 0;
    for (size_t i = 0; i < action_len && sig_idx < BRAIN_IMMUNE_EPITOPE_SIZE; i++) {
        signature[sig_idx++] = (uint8_t)action[i];
    }
    for (size_t i = 0; i < reason_len && sig_idx < BRAIN_IMMUNE_EPITOPE_SIZE; i++) {
        signature[sig_idx++] = (uint8_t)reason[i];
    }

    *sig_len = sig_idx;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int directive_immune_bridge_default_config(directive_immune_config_t* config) {
    if (!config) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    config->enable_immune_modulation = true;
    config->enable_directive_immune_trigger = true;
    config->il1_directive_sensitivity = CYTOKINE_IL1_DIRECTIVE_SENSITIVITY;
    config->il6_escalation_boost = CYTOKINE_IL6_ESCALATION_BOOST;
    config->tnf_block_threshold_reduction = CYTOKINE_TNF_THRESHOLD_REDUCTION;
    config->il10_tolerance_increase = CYTOKINE_IL10_TOLERANCE_INCREASE;
    config->threat_severity_threshold = DIRECTIVE_MILD_VIOLATION_SEVERITY;
    config->chronic_inflammation_duration_sec = 86400.0f * 7;  /* 7 days */

    return 0;
}

directive_immune_bridge_t* directive_immune_bridge_create(
    const directive_immune_config_t* config,
    core_directives_system_t* core_directives,
    brain_immune_system_t* immune_system)
{
    /* Guard: validate parameters */
    if (!immune_system) {
        NIMCP_LOGGING_ERROR("directive_immune_bridge_create: NULL immune_system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_system is NULL");

        return NULL;
    }

    /* Allocate bridge */
    directive_immune_bridge_t* bridge = (directive_immune_bridge_t*)
        nimcp_malloc(sizeof(directive_immune_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("directive_immune_bridge_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    /* Initialize with defaults or config */
    memset(bridge, 0, sizeof(directive_immune_bridge_t));

    if (config) {
        bridge->enable_immune_modulation = config->enable_immune_modulation;
        bridge->enable_directive_immune_trigger = config->enable_directive_immune_trigger;
        bridge->il1_directive_sensitivity = config->il1_directive_sensitivity;
        bridge->il6_escalation_boost = config->il6_escalation_boost;
        bridge->tnf_block_threshold_reduction = config->tnf_block_threshold_reduction;
        bridge->il10_tolerance_increase = config->il10_tolerance_increase;
        bridge->threat_severity_threshold = config->threat_severity_threshold;
        bridge->chronic_inflammation_duration_sec = config->chronic_inflammation_duration_sec;
    } else {
        directive_immune_config_t default_config;
        directive_immune_bridge_default_config(&default_config);
        bridge->enable_immune_modulation = default_config.enable_immune_modulation;
        bridge->enable_directive_immune_trigger = default_config.enable_directive_immune_trigger;
        bridge->il1_directive_sensitivity = default_config.il1_directive_sensitivity;
        bridge->il6_escalation_boost = default_config.il6_escalation_boost;
        bridge->tnf_block_threshold_reduction = default_config.tnf_block_threshold_reduction;
        bridge->il10_tolerance_increase = default_config.il10_tolerance_increase;
        bridge->threat_severity_threshold = default_config.threat_severity_threshold;
        bridge->chronic_inflammation_duration_sec = default_config.chronic_inflammation_duration_sec;
    }

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->core_directives = core_directives;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "core_directives_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("directive_immune_bridge_create: mutex creation failed");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "directive_immune_bridge_create: bridge->base is NULL");
        return NULL;
    }

    /* Initialize state */
    bridge->inflammation_state.current_level = INFLAMMATION_NONE;
    bridge->inflammation_state.strictness_factor = 1.0f;

    NIMCP_LOGGING_INFO("Created directive-immune bridge");
    return bridge;
}

void directive_immune_bridge_destroy(directive_immune_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        directive_immune_bridge_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free bridge */
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed directive-immune bridge");
}

/* ============================================================================
 * Immune → Directives API
 * ============================================================================ */

int directive_immune_bridge_update(directive_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    if (!bridge->enable_immune_modulation) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Update inflammation state from immune system */
    /* Note: In real implementation, query brain_immune_system for inflammation */
    bridge->inflammation_state.current_level = INFLAMMATION_NONE;  /* Placeholder */
    bridge->inflammation_state.strictness_factor =
        compute_strictness_from_inflammation(bridge->inflammation_state.current_level);

    /* Compute cytokine effects (placeholder - would query actual cytokine levels) */
    bridge->cytokine_effects.il1_strictness_boost = 0.0f;
    bridge->cytokine_effects.il6_escalation_boost = 0.0f;
    bridge->cytokine_effects.tnf_threshold_reduction = 0.0f;
    bridge->cytokine_effects.il10_tolerance_increase = 0.0f;

    /* Compute aggregate modifiers */
    bridge->cytokine_effects.total_strictness_modifier =
        bridge->inflammation_state.strictness_factor;
    bridge->cytokine_effects.total_threshold_modifier = 1.0f;

    /* Clamp to valid ranges */
    if (bridge->cytokine_effects.total_strictness_modifier < DIRECTIVE_MIN_THRESHOLD_MODIFIER) {
        bridge->cytokine_effects.total_strictness_modifier = DIRECTIVE_MIN_THRESHOLD_MODIFIER;
    }
    if (bridge->cytokine_effects.total_strictness_modifier > DIRECTIVE_MAX_THRESHOLD_MODIFIER) {
        bridge->cytokine_effects.total_strictness_modifier = DIRECTIVE_MAX_THRESHOLD_MODIFIER;
    }

    bridge->total_updates++;
    bridge->cytokine_modulations++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int directive_immune_bridge_apply_modulation(directive_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    if (!bridge->enable_immune_modulation || !bridge->core_directives) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Apply modulation to core directives */
    /* Note: In real implementation, would call core_directives APIs to set:
     * - Strictness factor
     * - Threshold modifiers
     * - Escalation bias
     */

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int directive_immune_bridge_get_effects(
    const directive_immune_bridge_t* bridge,
    cytokine_directive_effects_t* effects)
{
    if (!bridge || !effects) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->cytokine_effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Directives → Immune API
 * ============================================================================ */

int directive_immune_bridge_on_threat_detected(
    directive_immune_bridge_t* bridge,
    float threat_level)
{
    if (!bridge) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    if (!bridge->enable_directive_immune_trigger) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Guard: check if threat is severe enough */
    if (threat_level < bridge->threat_severity_threshold) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Map threat to immune activation */
    bridge->directive_modulation.threat_detected = true;
    bridge->directive_modulation.severity_for_immune = threat_level;
    bridge->directive_modulation.cytokine_to_release = map_severity_to_cytokine(threat_level);

    /* Trigger immune response (placeholder - would call brain_immune APIs) */
    NIMCP_LOGGING_INFO("Directive threat detected (severity %.1f), triggering immune", threat_level);

    bridge->directive_triggered_responses++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int directive_immune_bridge_report_blocked_action(
    directive_immune_bridge_t* bridge,
    const char* action,
    const char* reason)
{
    if (!bridge) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    if (!bridge->enable_directive_immune_trigger) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Create threat signature */
    create_threat_signature(action, reason,
                           bridge->directive_modulation.threat_pattern,
                           &bridge->directive_modulation.threat_pattern_len);

    /* Mark for memory formation */
    bridge->directive_modulation.form_memory_cell = true;

    /* Present to immune system (placeholder - would call brain_immune_present_antigen) */
    NIMCP_LOGGING_INFO("Blocked action reported: %s (reason: %s)",
                      action ? action : "unknown",
                      reason ? reason : "unknown");

    bridge->blocked_actions_reported++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int directive_immune_bridge_get_stats(
    const directive_immune_bridge_t* bridge,
    directive_immune_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    stats->total_updates = bridge->total_updates;
    stats->cytokine_modulations = bridge->cytokine_modulations;
    stats->directive_triggered_responses = bridge->directive_triggered_responses;
    stats->blocked_actions_reported = bridge->blocked_actions_reported;
    stats->memory_cells_formed = bridge->memory_cells_formed;
    stats->current_strictness_modifier = bridge->cytokine_effects.total_strictness_modifier;
    stats->current_threshold_modifier = bridge->cytokine_effects.total_threshold_modifier;
    stats->immune_alert_active = bridge->inflammation_state.immune_alert_active;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

bool directive_immune_bridge_is_alert_active(const directive_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bool active = bridge->inflammation_state.immune_alert_active;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return active;
}

float directive_immune_bridge_get_strictness(const directive_immune_bridge_t* bridge) {
    if (!bridge) {
        return 1.0f;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    float strictness = bridge->inflammation_state.strictness_factor;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return strictness;
}

float directive_immune_bridge_get_threshold_modifier(const directive_immune_bridge_t* bridge) {
    if (!bridge) {
        return 1.0f;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    float modifier = bridge->cytokine_effects.total_threshold_modifier;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return modifier;
}

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

int directive_immune_bridge_connect_bio_async(directive_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    if (bridge->base.bio_async_enabled) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0;  /* Already connected */
    }

    /* Register with bio-async router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_CORE_DIRECTIVES,
        .module_name = "directive_immune_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected directive-immune bridge to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int directive_immune_bridge_disconnect_bio_async(directive_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    if (!bridge->base.bio_async_enabled) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0;  /* Not connected */
    }

    /* Unregister from bio-async router */
    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected directive-immune bridge from bio-async router");

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

bool directive_immune_bridge_is_bio_async_connected(const directive_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bool connected = bridge->base.bio_async_enabled;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return connected;
}
