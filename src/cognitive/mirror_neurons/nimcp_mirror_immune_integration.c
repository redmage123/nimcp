/**
 * @file nimcp_mirror_immune_integration.c
 * @brief Implementation of Mirror Neuron - Immune System Integration
 * @version 1.0.0
 * @date 2025-12-11
 *
 * NIMCP CODING STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT/WHY/HOW documentation
 * - Thread-safe via mutex
 * - Single responsibility principle
 */

#include "cognitive/mirror_neurons/nimcp_mirror_immune_integration.h"
#include "cognitive/nimcp_mirror_neurons.h"
#include "cognitive/mirror_neurons/nimcp_mirror_resonance.h"
#include "cognitive/mirror_neurons/nimcp_mirror_hierarchy.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"

#include "async/nimcp_bio_messages.h"

#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(mirror_immune_integration, MESH_ADAPTER_CATEGORY_COGNITIVE)

/* Forward declarations for bio-async functions used before definition */
bool mirror_immune_register_bio_async(mirror_immune_integration_t* integration);
void mirror_immune_unregister_bio_async(mirror_immune_integration_t* integration);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current time in microseconds
 *
 * WHAT: Platform-independent timestamp
 * WHY:  Need microsecond precision for timing
 * HOW:  Use clock_gettime on POSIX
 */
static uint64_t get_current_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int mirror_immune_get_default_config(mirror_immune_config_t* config) {
    /* WHAT: Populate with biological defaults
     * WHY:  Provide literature-based parameters
     * HOW:  Set struct fields */
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_immune_get_default_config: config is NULL");
        return -1;
    }

    /* Immune → Mirror modulation */
    /* Phase 8: Heartbeat at operation start */
    mirror_immune_integration_heartbeat("mirror_immun_mirror_immune_get_de", 0.0f);


    config->cytokine_sensitivity = MIRROR_IMMUNE_CYTOKINE_SENSITIVITY;
    config->max_resonance_suppression = MIRROR_IMMUNE_MAX_SUPPRESSION;
    config->enable_sickness_behavior = true;
    config->il1_suppression_gain = 0.4f;
    config->il6_suppression_gain = 0.3f;
    config->tnf_suppression_gain = 0.5f;
    config->il10_restoration_gain = 0.6f;

    /* Mirror → Immune feedback */
    config->enable_isolation_detection = true;
    config->isolation_threshold_s = MIRROR_IMMUNE_ISOLATION_THRESHOLD_S;
    config->isolation_il6_release = MIRROR_IMMUNE_IL6_ISOLATION_AMOUNT;
    config->rejection_inflammation_gain = 0.3f;
    config->enable_social_recovery = true;
    config->social_success_il10_release = MIRROR_IMMUNE_IL10_RELEASE_AMOUNT;

    /* Thresholds */
    config->empathy_threshold_baseline = 0.5f;
    config->inflammation_threshold = 0.2f;

    /* Update timing */
    config->update_interval_ms = 1000;

    config->bio_async_enabled = true;

    return 0;
}

mirror_immune_integration_t* mirror_immune_create(
    const mirror_immune_config_t* config,
    mirror_neurons_t mirror_system,
    brain_immune_system_t* immune_system
) {
    /* WHAT: Allocate and initialize integration
     * WHY:  Set up bidirectional coupling
     * HOW:  Allocate, copy config, init state */
    if (!mirror_system || !immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_immune_create: required parameter is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_immune_integration_heartbeat("mirror_immun_mirror_immune_create", 0.0f);


    mirror_immune_integration_t* integration = nimcp_calloc(1, sizeof(*integration));
    if (!integration) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate integration");

        return NULL;

    }

    /* Copy or use default config */
    if (config) {
        integration->config = *config;
    } else {
        mirror_immune_get_default_config(&integration->config);
    }

    /* Store system references */
    integration->mirror_system = mirror_system;
    integration->immune_system = immune_system;

    /* Initialize state */
    integration->state.social_state = SOCIAL_STATE_ENGAGED;
    integration->state.immune_effect = IMMUNE_EFFECT_HEALTHY;
    integration->state.last_observation_time = get_current_time_us();
    integration->state.last_imitation_time = get_current_time_us();

    /* Create mutex */
    integration->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    if (!integration->mutex) {
        nimcp_free(integration);
        integration = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_immune_create: integration->mutex is NULL");
        return NULL;
    }
    if (nimcp_mutex_init(integration->mutex, NULL) != NIMCP_SUCCESS) {
        nimcp_free(integration->mutex);
        nimcp_free(integration);
        integration = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "mirror_immune_create: validation failed");
        return NULL;
    }

    integration->enabled = false;
    integration->last_update_time = get_current_time_us();

    /* Register with bio-async if enabled */
    if (integration->config.bio_async_enabled) {
        mirror_immune_register_bio_async(integration);
    }

    NIMCP_LOGGING_INFO("Mirror-immune integration created");
    return integration;
}

void mirror_immune_destroy(mirror_immune_integration_t* integration) {
    /* WHAT: Clean up integration
     * WHY:  Free resources
     * HOW:  Destroy mutex, free struct */
    if (!integration) return;

    if (integration->bio_async_registered) {
        mirror_immune_unregister_bio_async(integration);
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_immune_integration_heartbeat("mirror_immun_mirror_immune_destro", 0.0f);


    if (integration->mutex) {
        nimcp_mutex_destroy(integration->mutex);
    }

    nimcp_free(integration);
    integration = NULL;
    NIMCP_LOGGING_INFO("Mirror-immune integration destroyed");
}

int mirror_immune_enable(mirror_immune_integration_t* integration) {
    /* WHAT: Activate integration
     * WHY:  Begin bidirectional modulation
     * HOW:  Set flag, reset timers */
    if (!integration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_immune_enable: integration is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_immune_integration_heartbeat("mirror_immun_mirror_immune_enable", 0.0f);


    nimcp_mutex_lock(integration->mutex);
    integration->enabled = true;
    integration->last_update_time = get_current_time_us();
    integration->state.last_observation_time = integration->last_update_time;
    integration->state.last_imitation_time = integration->last_update_time;
    nimcp_mutex_unlock(integration->mutex);

    NIMCP_LOGGING_INFO("Mirror-immune integration enabled");
    return 0;
}

int mirror_immune_disable(mirror_immune_integration_t* integration) {
    /* WHAT: Deactivate integration
     * WHY:  Allow independent operation
     * HOW:  Clear flag, reset modulation */
    if (!integration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_immune_disable: integration is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_immune_integration_heartbeat("mirror_immun_mirror_immune_disabl", 0.0f);


    nimcp_mutex_lock(integration->mutex);
    integration->enabled = false;
    integration->state.resonance_suppression = 0.0f;
    integration->state.empathy_threshold_mod = 0.0f;
    nimcp_mutex_unlock(integration->mutex);

    NIMCP_LOGGING_INFO("Mirror-immune integration disabled");
    return 0;
}

/* ============================================================================
 * Immune → Mirror Neuron Modulation Implementation
 * ============================================================================ */

int mirror_immune_apply_immune_modulation(mirror_immune_integration_t* integration) {
    /* WHAT: Update mirror parameters from immune state
     * WHY:  Inflammation reduces resonance
     * HOW:  Sample cytokines, compute suppression, apply */
    if (!integration || !integration->enabled) {
        if (!integration) NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_immune_apply_immune_modulation: integration is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_immune_integration_heartbeat("mirror_immun_mirror_immune_apply_", 0.0f);


    nimcp_mutex_lock(integration->mutex);

    /* Sample cytokine levels from immune system */
    brain_immune_stats_t immune_stats;
    if (brain_immune_get_stats(integration->immune_system, &immune_stats) == 0) {
        /* Estimate cytokine levels from inflammation sites and activity
         * In real implementation, would query actual cytokine concentrations */
        float inflammation_level = (float)immune_stats.inflammation_sites / 10.0f;
        integration->state.current_inflammation = nimcp_clampf(inflammation_level, 0.0f, 1.0f);

        /* Estimate individual cytokine levels (simplified model) */
        integration->state.il1_level = integration->state.current_inflammation * 0.6f;
        integration->state.il6_level = integration->state.current_inflammation * 0.5f;
        integration->state.tnf_level = integration->state.current_inflammation * 0.7f;

        /* IL-10 inversely related to inflammation (anti-inflammatory) */
        integration->state.il10_level = 1.0f - integration->state.current_inflammation;
    }

    /* Compute resonance suppression */
    float suppression = mirror_immune_compute_resonance_suppression(integration);
    integration->state.resonance_suppression = suppression;

    /* Update empathy threshold */
    float threshold = mirror_immune_compute_empathy_threshold(integration);
    integration->state.empathy_threshold_mod = threshold;

    /* Apply sickness behavior if inflammation high */
    if (integration->state.current_inflammation > integration->config.inflammation_threshold &&
        integration->config.enable_sickness_behavior) {
        mirror_immune_apply_sickness_behavior(integration, integration->state.current_inflammation);
    }

    /* Restore function if IL-10 high */
    if (integration->state.il10_level > 0.5f) {
        mirror_immune_restore_social_function(integration, integration->state.il10_level);
    }

    nimcp_mutex_unlock(integration->mutex);
    return 0;
}

float mirror_immune_compute_resonance_suppression(
    const mirror_immune_integration_t* integration
) {
    /* WHAT: Calculate suppression from cytokines
     * WHY:  Pro-inflammatory reduces social motivation
     * HOW:  Weighted sum of cytokines */
    if (!integration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_immune_compute_resonance_suppression: integration is NULL");
        return 0.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_immune_integration_heartbeat("mirror_immun_mirror_immune_comput", 0.0f);


    const mirror_immune_config_t* cfg = &integration->config;
    const mirror_immune_state_t* state = &integration->state;

    /* Weighted sum: IL-1β + IL-6 + TNF-α - IL-10 */
    float pro_inflammatory =
        state->il1_level * cfg->il1_suppression_gain +
        state->il6_level * cfg->il6_suppression_gain +
        state->tnf_level * cfg->tnf_suppression_gain;

    float anti_inflammatory = state->il10_level * cfg->il10_restoration_gain;

    float net_suppression = (pro_inflammatory - anti_inflammatory) * cfg->cytokine_sensitivity;

    return nimcp_clampf(net_suppression, 0.0f, cfg->max_resonance_suppression);
}

float mirror_immune_compute_empathy_threshold(
    const mirror_immune_integration_t* integration
) {
    /* WHAT: Calculate empathy threshold modifier
     * WHY:  Inflammation raises execution threshold
     * HOW:  Scale baseline by inflammation */
    if (!integration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_immune_compute_empathy_threshold: integration is NULL");
        return 0.5f;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_immune_integration_heartbeat("mirror_immun_mirror_immune_comput", 0.0f);


    float baseline = integration->config.empathy_threshold_baseline;
    float inflammation_mod = integration->state.current_inflammation * 0.5f;

    return baseline + inflammation_mod;
}

int mirror_immune_apply_sickness_behavior(
    mirror_immune_integration_t* integration,
    float severity
) {
    /* WHAT: Suppress social processing
     * WHY:  Conserve energy during illness
     * HOW:  Boost BG inhibition, raise thresholds */
    if (!integration || !integration->mirror_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_immune_apply_sickness_behavior: required parameter is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_immune_integration_heartbeat("mirror_immun_mirror_immune_apply_", 0.0f);


    severity = nimcp_clampf(severity, 0.0f, 1.0f);

    /* Get resonance system if available */
    motor_resonance_t resonance = mirror_neurons_get_resonance(integration->mirror_system);
    if (resonance) {
        /* Boost basal ganglia inhibition to suppress automatic imitation */
        float bg_boost = 0.5f + severity * 0.4f;  /* 0.5 to 0.9 */
        motor_resonance_set_bg_inhibition(resonance, bg_boost);
    }

    /* Update immune effect state */
    integration->state.immune_effect = IMMUNE_EFFECT_SICKNESS;
    integration->state.sickness_behavior_events++;

    NIMCP_LOGGING_DEBUG("Sickness behavior applied: severity=%.2f", severity);
    return 0;
}

int mirror_immune_restore_social_function(
    mirror_immune_integration_t* integration,
    float il10_level
) {
    /* WHAT: Restore normal resonance
     * WHY:  IL-10 resolution enables social function
     * HOW:  Reduce suppression, lower thresholds */
    if (!integration || !integration->mirror_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_immune_restore_social_function: required parameter is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_immune_integration_heartbeat("mirror_immun_mirror_immune_restor", 0.0f);


    il10_level = nimcp_clampf(il10_level, 0.0f, 1.0f);

    /* Get resonance system */
    motor_resonance_t resonance = mirror_neurons_get_resonance(integration->mirror_system);
    if (resonance) {
        /* Restore normal BG inhibition */
        float normal_bg = 0.5f - il10_level * 0.2f;  /* Reduce from high to normal */
        motor_resonance_set_bg_inhibition(resonance, normal_bg);
    }

    /* Update state */
    integration->state.immune_effect = IMMUNE_EFFECT_RECOVERY;

    NIMCP_LOGGING_DEBUG("Social function restoring: IL-10=%.2f", il10_level);
    return 0;
}

/* ============================================================================
 * Mirror Neuron → Immune System Feedback Implementation
 * ============================================================================ */

bool mirror_immune_detect_isolation(
    mirror_immune_integration_t* integration,
    uint64_t current_time
) {
    /* WHAT: Check if isolated
     * WHY:  Trigger inflammation on prolonged isolation
     * HOW:  Compare time since last observation */
    if (!integration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_immune_detect_isolation: integration is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_immune_integration_heartbeat("mirror_immun_mirror_immune_detect", 0.0f);


    uint64_t time_since_obs = current_time - integration->state.last_observation_time;
    uint64_t threshold_us = (uint64_t)(integration->config.isolation_threshold_s * 1000000);

    return (time_since_obs > threshold_us);
}

int mirror_immune_trigger_isolation_response(
    mirror_immune_integration_t* integration
) {
    /* WHAT: Release IL-6 on isolation
     * WHY:  Social isolation activates inflammation
     * HOW:  Call brain immune to release cytokine */
    if (!integration || !integration->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_immune_trigger_isolation_response: required parameter is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_immune_integration_heartbeat("mirror_immun_mirror_immune_trigge", 0.0f);


    uint32_t cytokine_id = 0;
    int result = brain_immune_release_cytokine(
        integration->immune_system,
        CYTOKINE_IL6,
        0,  /* Source cell ID (0 = system) */
        integration->config.isolation_il6_release,
        0,  /* Target region (0 = broadcast) */
        &cytokine_id
    );

    if (result == 0) {
        integration->state.immune_triggered_il6++;
        integration->state.isolation_events++;
        NIMCP_LOGGING_INFO("Isolation detected: IL-6 released (id=%u)", cytokine_id);
    }

    return result;
}

int mirror_immune_trigger_rejection_response(
    mirror_immune_integration_t* integration
) {
    /* WHAT: Release stress cytokines on rejection
     * WHY:  Failed imitation = social rejection
     * HOW:  Count failures, release IL-6 if threshold exceeded */
    if (!integration || !integration->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_immune_trigger_rejection_response: required parameter is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_immune_integration_heartbeat("mirror_immun_mirror_immune_trigge", 0.0f);


    const uint32_t FAILURE_THRESHOLD = 5;
    if (integration->state.failed_imitation_count < FAILURE_THRESHOLD) {
        return 0;  /* Not enough failures yet */
    }

    /* Release IL-6 for stress response */
    float stress_level = integration->config.rejection_inflammation_gain;
    uint32_t cytokine_id = 0;
    int result = brain_immune_release_cytokine(
        integration->immune_system,
        CYTOKINE_IL6,
        0,
        stress_level,
        0,
        &cytokine_id
    );

    if (result == 0) {
        integration->state.failed_imitation_count = 0;  /* Reset counter */
        integration->state.immune_triggered_il6++;
        NIMCP_LOGGING_INFO("Rejection stress: IL-6 released (failures=%u)", FAILURE_THRESHOLD);
    }

    return result;
}

int mirror_immune_release_social_success_il10(
    mirror_immune_integration_t* integration
) {
    /* WHAT: Release IL-10 on success
     * WHY:  Positive social interaction reduces inflammation
     * HOW:  Call brain immune to release anti-inflammatory */
    if (!integration || !integration->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_immune_release_social_success_il10: required parameter is NULL");
        return -1;
    }

    if (!integration->config.enable_social_recovery) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_immune_integration_heartbeat("mirror_immun_mirror_immune_releas", 0.0f);


    uint32_t cytokine_id = 0;
    int result = brain_immune_release_cytokine(
        integration->immune_system,
        CYTOKINE_IL10,
        0,
        integration->config.social_success_il10_release,
        0,
        &cytokine_id
    );

    if (result == 0) {
        integration->state.immune_triggered_il10++;
        NIMCP_LOGGING_DEBUG("Social success: IL-10 released (id=%u)", cytokine_id);
    }

    return result;
}

int mirror_immune_update_social_state(
    mirror_immune_integration_t* integration,
    uint64_t current_time
) {
    /* WHAT: Classify social engagement
     * WHY:  Determine immune feedback type
     * HOW:  Analyze activity patterns */
    if (!integration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_immune_update_social_state: integration is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_immune_integration_heartbeat("mirror_immun_mirror_immune_update", 0.0f);


    nimcp_mutex_lock(integration->mutex);

    mirror_social_state_t old_state = integration->state.social_state;
    mirror_social_state_t new_state = old_state;

    /* Check if isolated */
    if (mirror_immune_detect_isolation(integration, current_time)) {
        new_state = SOCIAL_STATE_ISOLATED;

        /* Mark isolation start if transitioning */
        if (old_state != SOCIAL_STATE_ISOLATED) {
            integration->state.isolation_start_time = current_time;
        }
    } else {
        /* Not isolated, check activity level */
        uint64_t time_since_imitation = current_time - integration->state.last_imitation_time;
        uint64_t recent_threshold = 10000000;  /* 10 seconds */

        if (time_since_imitation < recent_threshold) {
            new_state = SOCIAL_STATE_ENGAGED;
        } else {
            new_state = SOCIAL_STATE_PASSIVE;
        }

        /* Check for rejection (many recent failures) */
        if (integration->state.failed_imitation_count >= 5) {
            new_state = SOCIAL_STATE_REJECTED;
        }
    }

    integration->state.social_state = new_state;

    nimcp_mutex_unlock(integration->mutex);
    return 0;
}

/* ============================================================================
 * Update and Query API Implementation
 * ============================================================================ */

int mirror_immune_update(
    mirror_immune_integration_t* integration,
    uint64_t current_time
) {
    /* WHAT: Process bidirectional integration
     * WHY:  Maintain coupling between systems
     * HOW:  Update social state, apply modulation, check triggers */
    if (!integration || !integration->enabled) {
        if (!integration) NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_immune_update: integration is NULL");
        return -1;
    }

    /* Check if update interval elapsed */
    /* Phase 8: Heartbeat at operation start */
    mirror_immune_integration_heartbeat("mirror_immun_mirror_immune_update", 0.0f);


    uint64_t elapsed_ms = (current_time - integration->last_update_time) / 1000;
    if (elapsed_ms < integration->config.update_interval_ms) {
        return 0;  /* Too soon */
    }

    /* Update social state from mirror neuron activity */
    /* Note: mirror_immune_update_social_state acquires its own lock */
    mirror_immune_update_social_state(integration, current_time);

    /* Apply immune modulation to mirror neurons (Immune → Mirror) */
    /* Note: mirror_immune_apply_immune_modulation acquires its own lock */
    mirror_immune_apply_immune_modulation(integration);
    nimcp_mutex_lock(integration->mutex);

    /* Check for immune feedback triggers (Mirror → Immune) */
    if (integration->state.social_state == SOCIAL_STATE_ISOLATED &&
        integration->config.enable_isolation_detection) {
        nimcp_mutex_unlock(integration->mutex);
        mirror_immune_trigger_isolation_response(integration);
        nimcp_mutex_lock(integration->mutex);
    }

    if (integration->state.social_state == SOCIAL_STATE_REJECTED) {
        nimcp_mutex_unlock(integration->mutex);
        mirror_immune_trigger_rejection_response(integration);
        nimcp_mutex_lock(integration->mutex);
    }

    integration->last_update_time = current_time;

    nimcp_mutex_unlock(integration->mutex);
    return 0;
}

int mirror_immune_get_stats(
    mirror_immune_integration_t* integration,
    mirror_immune_stats_t* stats
) {
    /* WHAT: Retrieve statistics
     * WHY:  Monitor integration health
     * HOW:  Copy accumulated metrics */
    if (!integration || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_immune_get_stats: required parameter is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_immune_integration_heartbeat("mirror_immun_mirror_immune_get_st", 0.0f);


    nimcp_mutex_lock(integration->mutex);

    memset(stats, 0, sizeof(*stats));

    /* Immune → Mirror effects */
    stats->resonance_suppression_events = integration->state.sickness_behavior_events;
    stats->avg_cytokine_level = integration->state.current_cytokine_level;
    stats->avg_resonance_suppression = integration->state.resonance_suppression;
    stats->sickness_behavior_activations = integration->state.sickness_behavior_events;

    /* Mirror → Immune effects */
    stats->isolation_detections = integration->state.isolation_events;
    stats->il6_releases = integration->state.immune_triggered_il6;
    stats->il10_releases = integration->state.immune_triggered_il10;

    nimcp_mutex_unlock(integration->mutex);
    return 0;
}

mirror_social_state_t mirror_immune_get_social_state(
    const mirror_immune_integration_t* integration
) {
    /* WHAT: Get current social state
     * WHY:  Query integration status
     * HOW:  Return state field */
    /* Phase 8: Heartbeat at operation start */
    mirror_immune_integration_heartbeat("mirror_immun_mirror_immune_get_so", 0.0f);


    return integration ? integration->state.social_state : SOCIAL_STATE_ISOLATED;
}

mirror_immune_effect_t mirror_immune_get_immune_effect(
    const mirror_immune_integration_t* integration
) {
    /* WHAT: Get current immune effect
     * WHY:  Query modulation type
     * HOW:  Return effect field */
    /* Phase 8: Heartbeat at operation start */
    mirror_immune_integration_heartbeat("mirror_immun_mirror_immune_get_im", 0.0f);


    return integration ? integration->state.immune_effect : IMMUNE_EFFECT_NONE;
}

float mirror_immune_get_resonance_suppression(
    const mirror_immune_integration_t* integration
) {
    /* WHAT: Get suppression factor
     * WHY:  Query modulation strength
     * HOW:  Return suppression field */
    /* Phase 8: Heartbeat at operation start */
    mirror_immune_integration_heartbeat("mirror_immun_mirror_immune_get_re", 0.0f);


    return integration ? integration->state.resonance_suppression : 0.0f;
}

/* ============================================================================
 * Event Notification API Implementation
 * ============================================================================ */

void mirror_immune_notify_observation(
    mirror_immune_integration_t* integration,
    uint64_t timestamp_us
) {
    /* WHAT: Update observation timestamp
     * WHY:  Track social activity
     * HOW:  Set last observation time */
    if (!integration || !integration->enabled) {
        if (!integration) NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_immune_notify_observation: integration is NULL");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_immune_integration_heartbeat("mirror_immun_mirror_immune_notify", 0.0f);


    nimcp_mutex_lock(integration->mutex);
    integration->state.last_observation_time = timestamp_us;

    /* Clear isolation if previously isolated */
    if (integration->state.social_state == SOCIAL_STATE_ISOLATED) {
        integration->state.isolation_start_time = 0;
    }

    nimcp_mutex_unlock(integration->mutex);
}

void mirror_immune_notify_imitation_success(
    mirror_immune_integration_t* integration,
    uint64_t timestamp_us
) {
    /* WHAT: Record successful imitation
     * WHY:  Trigger IL-10 release
     * HOW:  Update timestamp, release cytokine */
    if (!integration || !integration->enabled) {
        if (!integration) NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_immune_notify_imitation_success: integration is NULL");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_immune_integration_heartbeat("mirror_immun_mirror_immune_notify", 0.0f);


    nimcp_mutex_lock(integration->mutex);
    integration->state.last_imitation_time = timestamp_us;
    integration->state.failed_imitation_count = 0;  /* Reset failures */
    nimcp_mutex_unlock(integration->mutex);

    /* Release IL-10 (anti-inflammatory) */
    mirror_immune_release_social_success_il10(integration);
}

void mirror_immune_notify_imitation_failure(
    mirror_immune_integration_t* integration
) {
    /* WHAT: Count imitation failure
     * WHY:  Trigger stress response on repeated failures
     * HOW:  Increment counter */
    if (!integration || !integration->enabled) {
        if (!integration) NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_immune_notify_imitation_failure: integration is NULL");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_immune_integration_heartbeat("mirror_immun_mirror_immune_notify", 0.0f);


    nimcp_mutex_lock(integration->mutex);
    integration->state.failed_imitation_count++;
    nimcp_mutex_unlock(integration->mutex);
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

bool mirror_immune_register_bio_async(mirror_immune_integration_t* integration) {
    if (!integration || integration->bio_async_registered) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_immune_register_bio_async: integration is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_immune_integration_heartbeat("mirror_immun_register_bio_async", 0.0f);

    integration->bio_async_registered = true;
    return true;
}

void mirror_immune_unregister_bio_async(mirror_immune_integration_t* integration) {
    if (!integration || !integration->bio_async_registered) return;

    /* Phase 8: Heartbeat at operation start */
    mirror_immune_integration_heartbeat("mirror_immun_unregister_bio_async", 0.0f);

    integration->bio_async_registered = false;
}

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

const char* mirror_social_state_to_string(mirror_social_state_t state) {
    switch (state) {
        case SOCIAL_STATE_ENGAGED: return "ENGAGED";
        case SOCIAL_STATE_PASSIVE: return "PASSIVE";
        case SOCIAL_STATE_ISOLATED: return "ISOLATED";
        case SOCIAL_STATE_REJECTED: return "REJECTED";
        default: return "UNKNOWN";
    }
}

const char* mirror_immune_effect_to_string(mirror_immune_effect_t effect) {
    switch (effect) {
        case IMMUNE_EFFECT_NONE: return "NONE";
        case IMMUNE_EFFECT_SICKNESS: return "SICKNESS";
        case IMMUNE_EFFECT_STRESS: return "STRESS";
        case IMMUNE_EFFECT_RECOVERY: return "RECOVERY";
        case IMMUNE_EFFECT_HEALTHY: return "HEALTHY";
        default: return "UNKNOWN";
    }
}

/* ============================================================================
 * KG Self-Awareness Integration
 * ============================================================================ */

int mirror_immune_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    mirror_immune_integration_heartbeat("mirror_immun_mirror_immune_query_", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Mirror_Immune_Integration");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                mirror_immune_integration_heartbeat("mirror_immun_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Mirror immune self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Mirror_Immune_Integration");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Mirror_Immune_Integration");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void mirror_immune_integration_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_mirror_immune_integration_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training stubs
 * ============================================================================ */
int mirror_immune_integration_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_immune_integration_training_begin: NULL argument");
        return -1;
    }
    mirror_immune_integration_heartbeat_instance(NULL, "mirror_immune_integration_training_begin", 0.0f);
    return 0;
}

int mirror_immune_integration_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_immune_integration_training_end: NULL argument");
        return -1;
    }
    mirror_immune_integration_heartbeat_instance(NULL, "mirror_immune_integration_training_end", 1.0f);
    return 0;
}

int mirror_immune_integration_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_immune_integration_training_step: NULL argument");
        return -1;
    }
    mirror_immune_integration_heartbeat_instance(NULL, "mirror_immune_integration_training_step", progress);
    return 0;
}
