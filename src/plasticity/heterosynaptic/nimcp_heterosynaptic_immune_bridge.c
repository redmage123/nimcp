/**
 * @file nimcp_heterosynaptic_immune_bridge.c
 * @brief Heterosynaptic-Immune Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-19
 */

#include "plasticity/heterosynaptic/nimcp_heterosynaptic_immune_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(heterosynaptic_immune_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(hetero_immune_bridge)

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int hetero_immune_default_config(hetero_immune_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hetero_immune_default_config: config is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    config->enable_cytokine_modulation = true;
    config->enable_inflammation_suppression = true;
    config->enable_instability_detection = true;
    config->enable_homeostatic_feedback = true;

    config->cytokine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->instability_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    config->base_competition = 1.0f;
    config->base_depression = HETERO_DEFAULT_DEPRESSION_FACTOR;
    config->base_radius = HETERO_DEFAULT_NEIGHBOR_RADIUS;

    config->runaway_pruning_threshold = HETERO_RUNAWAY_PRUNING_THRESHOLD;
    config->competition_failure_threshold = HETERO_COMPETITION_FAILURE_THRESHOLD;

    return 0;
}

hetero_immune_bridge_t* hetero_immune_bridge_create(
    const hetero_immune_config_t* config,
    brain_immune_system_t* immune_system,
    hetero_system_t* hetero_system)
{
    if (!hetero_system) {
        NIMCP_LOGGING_ERROR("Heterosynaptic system is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hetero_system is NULL");

        return NULL;
    }

    /* Use defaults if no config */
    hetero_immune_config_t default_config;
    if (!config) {
        hetero_immune_default_config(&default_config);
        config = &default_config;
    }

    /* Allocate bridge */
    hetero_immune_bridge_t* bridge = nimcp_malloc(sizeof(hetero_immune_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate hetero-immune bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hetero_immune_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Initialize fields */
    memset(bridge, 0, sizeof(hetero_immune_bridge_t));
    bridge->immune_system = immune_system;
    bridge->hetero_system = hetero_system;

    bridge->base_competition = config->base_competition;
    bridge->base_depression = config->base_depression;
    bridge->base_radius = config->base_radius;

    bridge->enable_cytokine_modulation = config->enable_cytokine_modulation;
    bridge->enable_inflammation_suppression = config->enable_inflammation_suppression;
    bridge->enable_instability_detection = config->enable_instability_detection;
    bridge->enable_homeostatic_feedback = config->enable_homeostatic_feedback;

    /* Initialize cytokine effects to neutral */
    bridge->cytokine_effects.competition_factor = 1.0f;
    bridge->cytokine_effects.depression_factor = 1.0f;
    bridge->cytokine_effects.radius_factor = 1.0f;

    /* Initialize inflammation state */
    bridge->inflammation_state.current_level = INFLAMMATION_NONE;
    bridge->inflammation_state.inflammation_duration_sec = 0.0f;
    bridge->inflammation_state.is_chronic = false;
    bridge->inflammation_state.competition_suppression = INFLAMMATION_COMPETITION_NONE;
    bridge->inflammation_state.radius_narrowing = INFLAMMATION_RADIUS_NONE;
    bridge->inflammation_state.wta_weakening = 0.0f;
    bridge->inflammation_state.pruning_excess = 0.0f;
    bridge->inflammation_state.competition_deficit = 0.0f;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "heterosynaptic_immune") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "hetero_immune_bridge_create: failed to initialize bridge base");
        nimcp_free(bridge);
        return NULL;
    }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hetero_immune_bridge_create: failed to create mutex");
        nimcp_free(bridge);
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created hetero-immune bridge");
    return bridge;
}

void hetero_immune_bridge_destroy(hetero_immune_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->base.bio_async_enabled) {
        hetero_immune_disconnect_bio_async(bridge);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("Destroyed hetero-immune bridge");
}

/* ============================================================================
 * Immune → Heterosynaptic API Implementation
 * ============================================================================ */

int hetero_immune_apply_cytokine_effects(hetero_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hetero_immune_apply_cytokine_effects: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hetero_immune_apply_cytokine_effects: immune_system is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!bridge->enable_cytokine_modulation) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get cytokine levels from immune system */
    float il1 = brain_immune_get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IL1);
    float il6 = brain_immune_get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IL6);
    float tnf = brain_immune_get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_TNF);
    float il10 = brain_immune_get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IL10);

    /* Compute individual cytokine effects */
    bridge->cytokine_effects.il1_competition_reduction =
        1.0f - il1 * (1.0f - CYTOKINE_IL1_COMPETITION_REDUCTION);
    bridge->cytokine_effects.il6_competition_reduction =
        1.0f - il6 * (1.0f - CYTOKINE_IL6_COMPETITION_REDUCTION);
    bridge->cytokine_effects.tnf_competition_reduction =
        1.0f - tnf * (1.0f - CYTOKINE_TNF_COMPETITION_REDUCTION);
    bridge->cytokine_effects.il10_competition_restoration =
        1.0f + il10 * (CYTOKINE_IL10_COMPETITION_RESTORATION - 1.0f);

    /* Aggregate competition factor */
    float competition = bridge->cytokine_effects.il1_competition_reduction *
                       bridge->cytokine_effects.il6_competition_reduction *
                       bridge->cytokine_effects.tnf_competition_reduction *
                       bridge->cytokine_effects.il10_competition_restoration;

    bridge->cytokine_effects.competition_factor = competition;
    bridge->cytokine_effects.depression_factor = competition;  /* Similar effect */
    bridge->cytokine_effects.radius_factor = competition;      /* Radius scales with competition */

    bridge->cytokine_modulations++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int hetero_immune_apply_inflammation_effects(hetero_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hetero_immune_apply_inflammation_effects: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hetero_immune_apply_inflammation_effects: immune_system is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!bridge->enable_inflammation_suppression) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get inflammation level */
    bridge->inflammation_state.current_level =
        brain_immune_get_inflammation_level(bridge->immune_system);

    /* Map inflammation to competition suppression */
    switch (bridge->inflammation_state.current_level) {
        case INFLAMMATION_NONE:
            bridge->inflammation_state.competition_suppression = INFLAMMATION_COMPETITION_NONE;
            bridge->inflammation_state.radius_narrowing = INFLAMMATION_RADIUS_NONE;
            break;
        case INFLAMMATION_LOCAL:
            bridge->inflammation_state.competition_suppression = INFLAMMATION_COMPETITION_LOCAL;
            bridge->inflammation_state.radius_narrowing = INFLAMMATION_RADIUS_LOCAL;
            break;
        case INFLAMMATION_REGIONAL:
            bridge->inflammation_state.competition_suppression = INFLAMMATION_COMPETITION_REGIONAL;
            bridge->inflammation_state.radius_narrowing = INFLAMMATION_RADIUS_REGIONAL;
            break;
        case INFLAMMATION_SYSTEMIC:
            bridge->inflammation_state.competition_suppression = INFLAMMATION_COMPETITION_SYSTEMIC;
            bridge->inflammation_state.radius_narrowing = INFLAMMATION_RADIUS_SYSTEMIC;
            break;
        case INFLAMMATION_STORM:
            bridge->inflammation_state.competition_suppression = INFLAMMATION_COMPETITION_STORM;
            bridge->inflammation_state.radius_narrowing = INFLAMMATION_RADIUS_STORM;
            break;
        default:
            bridge->inflammation_state.competition_suppression = 1.0f;
            bridge->inflammation_state.radius_narrowing = 1.0f;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

float hetero_immune_get_effective_competition(
    const hetero_immune_bridge_t* bridge,
    float base_competition)
{
    if (!bridge) return base_competition;

    float modulation = bridge->cytokine_effects.competition_factor *
                      bridge->inflammation_state.competition_suppression;

    return base_competition * modulation;
}

int hetero_immune_get_modulation_state(
    const hetero_immune_bridge_t* bridge,
    hetero_modulation_state_t* modulation)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hetero_immune_get_modulation_state: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hetero_immune_get_modulation_state: modulation is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    modulation->competition_modulation = bridge->cytokine_effects.competition_factor *
                                        bridge->inflammation_state.competition_suppression;
    modulation->depression_modulation = bridge->cytokine_effects.depression_factor *
                                        bridge->inflammation_state.competition_suppression;
    modulation->radius_modulation = bridge->cytokine_effects.radius_factor *
                                    bridge->inflammation_state.radius_narrowing;

    modulation->effective_competition = bridge->base_competition * modulation->competition_modulation;
    modulation->effective_depression = bridge->base_depression * modulation->depression_modulation;
    modulation->effective_radius = bridge->base_radius * modulation->radius_modulation;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hetero_immune_restore_competition(
    hetero_immune_bridge_t* bridge,
    float recovery_factor)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hetero_immune_restore_competition: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Interpolate toward baseline */
    float current_competition = bridge->cytokine_effects.competition_factor;
    float restored = current_competition + recovery_factor * (1.0f - current_competition);

    bridge->cytokine_effects.competition_factor = restored;
    bridge->cytokine_effects.depression_factor = restored;
    bridge->cytokine_effects.radius_factor = restored;

    bridge->competition_restorations++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Heterosynaptic → Immune API Implementation
 * ============================================================================ */

int hetero_immune_detect_instability(hetero_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hetero_immune_detect_instability: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!bridge->hetero_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hetero_immune_detect_instability: hetero_system is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!bridge->enable_instability_detection) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get heterosynaptic statistics */
    uint64_t total_competitions, total_depressions;
    float avg_neighbors;
    hetero_get_statistics(bridge->hetero_system, &total_competitions,
                         &total_depressions, &avg_neighbors);

    /* Compute depression rate (simplified) */
    float recent_depression = (float)total_depressions;
    float expected_depression = (float)total_competitions * 2.0f;  /* Expected ~2 per competition */

    bridge->instability_state.recent_depression_rate = recent_depression;
    bridge->instability_state.expected_depression_rate = expected_depression;

    /* Check for runaway pruning */
    if (expected_depression > 0.0f) {
        float depression_ratio = recent_depression / expected_depression;
        bridge->instability_state.runaway_pruning_detected =
            (depression_ratio > HETERO_RUNAWAY_PRUNING_THRESHOLD);

        /* Check for competition failure */
        bridge->instability_state.competition_failure_detected =
            (depression_ratio < HETERO_COMPETITION_FAILURE_THRESHOLD);

        /* Check for balanced competition */
        bridge->instability_state.balanced_competition =
            (depression_ratio >= HETERO_BALANCED_DEPRESSION_MIN &&
             depression_ratio <= HETERO_BALANCED_DEPRESSION_MAX);

        bridge->instability_state.competition_efficiency = depression_ratio;
    }

    /* Compute severity */
    if (bridge->instability_state.runaway_pruning_detected) {
        bridge->instability_state.instability_severity = 0.8f;
    } else if (bridge->instability_state.competition_failure_detected) {
        bridge->instability_state.instability_severity = 0.6f;
    } else if (bridge->instability_state.balanced_competition) {
        bridge->instability_state.instability_severity = 0.0f;
    } else {
        bridge->instability_state.instability_severity = 0.3f;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hetero_immune_alert_instability(
    hetero_immune_bridge_t* bridge,
    uint32_t* antigen_id)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hetero_immune_alert_instability: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hetero_immune_alert_instability: immune_system is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!antigen_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hetero_immune_alert_instability: antigen_id is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Detect current instability */
    hetero_immune_detect_instability(bridge);

    if (bridge->instability_state.runaway_pruning_detected) {
        /* Create antigen for runaway pruning */
        uint8_t epitope[] = {'H', 'E', 'T', 'E', 'R', 'O', '_', 'P', 'R', 'U', 'N', 'E'};
        int result = brain_immune_present_antigen(
            bridge->immune_system,
            ANTIGEN_SOURCE_MANUAL,
            epitope,
            sizeof(epitope),
            8,  /* Severity */
            0,  /* Node ID */
            antigen_id
        );

        if (result == 0) {
            bridge->instability_alerts++;
            NIMCP_LOGGING_WARN("Runaway heterosynaptic pruning detected, immune alert triggered");
        }

        return result;

    } else if (bridge->instability_state.competition_failure_detected) {
        /* Create antigen for competition failure */
        uint8_t epitope[] = {'H', 'E', 'T', 'E', 'R', 'O', '_', 'F', 'A', 'I', 'L'};
        int result = brain_immune_present_antigen(
            bridge->immune_system,
            ANTIGEN_SOURCE_MANUAL,
            epitope,
            sizeof(epitope),
            6,  /* Severity */
            0,
            antigen_id
        );

        if (result == 0) {
            bridge->instability_alerts++;
            NIMCP_LOGGING_WARN("Heterosynaptic competition failure detected");
        }

        return result;
    }

    return 0;  /* No instability */
}

int hetero_immune_signal_balanced_competition(hetero_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hetero_immune_signal_balanced_competition: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hetero_immune_signal_balanced_competition: immune_system is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!bridge->enable_homeostatic_feedback) return 0;

    if (bridge->instability_state.balanced_competition) {
        /* Request IL-10 release for healthy competition */
        /* This would be implemented through brain immune system API */
        NIMCP_LOGGING_DEBUG("Balanced heterosynaptic competition, promoting IL-10");
        return 0;
    }

    return 0;
}

/* ============================================================================
 * Bidirectional Update API Implementation
 * ============================================================================ */

int hetero_immune_bridge_update(
    hetero_immune_bridge_t* bridge,
    uint64_t delta_ms)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hetero_immune_bridge_update: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Apply immune → heterosynaptic effects */
    hetero_immune_apply_cytokine_effects(bridge);
    hetero_immune_apply_inflammation_effects(bridge);

    /* Detect heterosynaptic → immune instabilities */
    hetero_immune_detect_instability(bridge);

    /* Update inflammation duration */
    if (bridge->inflammation_state.current_level != INFLAMMATION_NONE) {
        bridge->inflammation_state.inflammation_duration_sec += delta_ms / 1000.0f;
        bridge->inflammation_state.is_chronic =
            (bridge->inflammation_state.inflammation_duration_sec >= 604800.0f);  /* 7 days */
    } else {
        bridge->inflammation_state.inflammation_duration_sec = 0.0f;
        bridge->inflammation_state.is_chronic = false;
    }

    bridge->total_updates++;

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

int hetero_immune_get_cytokine_effects(
    const hetero_immune_bridge_t* bridge,
    cytokine_hetero_effects_t* effects)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hetero_immune_get_cytokine_effects: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hetero_immune_get_cytokine_effects: effects is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    memcpy(effects, &bridge->cytokine_effects, sizeof(cytokine_hetero_effects_t));
    return 0;
}

int hetero_immune_get_inflammation_state(
    const hetero_immune_bridge_t* bridge,
    inflammation_hetero_state_t* state)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hetero_immune_get_inflammation_state: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hetero_immune_get_inflammation_state: state is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    memcpy(state, &bridge->inflammation_state, sizeof(inflammation_hetero_state_t));
    return 0;
}

int hetero_immune_get_instability_state(
    const hetero_immune_bridge_t* bridge,
    hetero_instability_state_t* state)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hetero_immune_get_instability_state: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hetero_immune_get_instability_state: state is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    memcpy(state, &bridge->instability_state, sizeof(hetero_instability_state_t));
    return 0;
}

bool hetero_immune_is_competition_impaired(const hetero_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return (bridge->cytokine_effects.competition_factor < 1.0f ||
            bridge->inflammation_state.competition_suppression < 1.0f);
}

float hetero_immune_get_competition_reduction(const hetero_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    float factor = bridge->cytokine_effects.competition_factor *
                   bridge->inflammation_state.competition_suppression;
    return (1.0f - factor) * 100.0f;  /* Percentage reduction */
}

/* ============================================================================
 * Bio-Async Integration API Implementation
 * ============================================================================ */

int hetero_immune_connect_bio_async(hetero_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hetero_immune_connect_bio_async: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = 0x0D40,  /* BIO_MODULE_IMMUNE_HETEROSYNAPTIC */
        .module_name = "heterosynaptic_immune_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected hetero-immune bridge to bio-async router");
        return 0;
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
        return NIMCP_ERROR_OPERATION_FAILED;
    }
}

int hetero_immune_disconnect_bio_async(hetero_immune_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected hetero-immune bridge from bio-async router");
    return 0;
}

bool hetero_immune_is_bio_async_connected(const hetero_immune_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}
