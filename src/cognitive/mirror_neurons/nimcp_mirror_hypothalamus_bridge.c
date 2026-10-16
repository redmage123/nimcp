/**
 * @file nimcp_mirror_hypothalamus_bridge.c
 * @brief Implementation of Mirror Neuron - Hypothalamus Bidirectional Bridge
 * @version 1.0.0
 * @date 2025-01-05
 *
 * NIMCP CODING STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT/WHY/HOW documentation
 * - Thread-safe via mutex
 * - Single responsibility principle
 */

#include "cognitive/mirror_neurons/nimcp_mirror_hypothalamus_bridge.h"
#include "cognitive/nimcp_mirror_neurons.h"
#include "cognitive/mirror_neurons/nimcp_mirror_resonance.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_adapter.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include "glial/myelin_sheath/nimcp_myelin_math.h"

#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(mirror_hypothalamus_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_mirror_hypothalamus_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_mirror_hypothalamus_bridge_mesh_registry = NULL;

nimcp_error_t mirror_hypothalamus_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_mirror_hypothalamus_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "mirror_hypothalamus_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "mirror_hypothalamus_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_mirror_hypothalamus_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_mirror_hypothalamus_bridge_mesh_registry = registry;
    return err;
}

void mirror_hypothalamus_bridge_mesh_unregister(void) {
    if (g_mirror_hypothalamus_bridge_mesh_registry && g_mirror_hypothalamus_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_mirror_hypothalamus_bridge_mesh_registry, g_mirror_hypothalamus_bridge_mesh_id);
        g_mirror_hypothalamus_bridge_mesh_id = 0;
        g_mirror_hypothalamus_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from mirror_hypothalamus_bridge module (instance-level) */
static inline void mirror_hypothalamus_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_mirror_hypothalamus_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_mirror_hypothalamus_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_mirror_hypothalamus_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(mirror_hypo, mirror_hypo_bridge_t)

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

/**
 * @brief Check if isolation threshold exceeded
 *
 * WHAT: Determine if social isolation has occurred
 * WHY:  Need to trigger stress response on prolonged isolation
 * HOW:  Compare time since last observation to threshold
 */
static bool check_isolation(const mirror_hypo_bridge_t* bridge, uint64_t current_time) {
    if (!bridge) {
        return false;
    }

    /* Use 5 minutes (300 seconds) as isolation threshold */
    const uint64_t ISOLATION_THRESHOLD_US = 300 * 1000000;
    uint64_t time_since_observation = current_time - bridge->state.last_observation_time;

    return (time_since_observation > ISOLATION_THRESHOLD_US);
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int mirror_hypo_get_default_config(mirror_hypo_config_t* config) {
    /* WHAT: Populate with biological defaults
     * WHY:  Provide literature-based parameters
     * HOW:  Set struct fields */
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hypo_get_default_config: config is NULL");
        return -1;
    }

    /* Hypothalamus -> Mirror modulation */
    /* Phase 8: Heartbeat at operation start */
    mirror_hypothalamus_bridge_heartbeat("mirror_hypot_mirror_hypo_get_defa", 0.0f);


    config->cortisol_sensitivity = MIRROR_HYPO_CORTISOL_SENSITIVITY;
    config->max_stress_suppression = MIRROR_HYPO_MAX_STRESS_SUPPRESSION;
    config->enable_circadian_gating = true;
    config->circadian_night_threshold_boost = MIRROR_HYPO_CIRCADIAN_NIGHT_BOOST;
    config->enable_drive_suppression = true;
    config->hunger_suppression_threshold = MIRROR_HYPO_HUNGER_SUPPRESSION_THRESH;
    config->thirst_suppression_threshold = MIRROR_HYPO_THIRST_SUPPRESSION_THRESH;
    config->drive_suppression_gain = MIRROR_HYPO_DRIVE_SUPPRESSION_GAIN;

    /* Mirror -> Hypothalamus feedback */
    config->enable_isolation_stress = true;
    config->isolation_stress_level = MIRROR_HYPO_ISOLATION_STRESS_LEVEL;
    config->enable_empathic_arousal = true;
    config->empathic_arousal_gain = MIRROR_HYPO_EMPATHIC_AROUSAL_GAIN;
    config->enable_imitation_reward = true;
    config->imitation_reward_signal = MIRROR_HYPO_IMITATION_REWARD_SIGNAL;

    /* Update timing */
    config->update_interval_ms = 100;

    return 0;
}

mirror_hypo_bridge_t* mirror_hypo_create(
    const mirror_hypo_config_t* config,
    mirror_neurons_t mirror_system,
    hypothalamus_adapter_t* hypothalamus
) {
    /* WHAT: Allocate and initialize bridge
     * WHY:  Set up bidirectional coupling
     * HOW:  Allocate, copy config, init state */
    if (!mirror_system || !hypothalamus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hypo_create: mirror_system or hypothalamus is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hypothalamus_bridge_heartbeat("mirror_hypot_mirror_hypo_create", 0.0f);


    mirror_hypo_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    /* Copy or use default config */
    if (config) {
        bridge->config = *config;
    } else {
        mirror_hypo_get_default_config(&bridge->config);
    }

    /* Store system references */
    bridge->mirror_system = mirror_system;
    bridge->hypothalamus = hypothalamus;

    /* Initialize state */
    uint64_t now = get_current_time_us();
    bridge->state.current_effect = HYPO_EFFECT_NONE;
    bridge->state.last_feedback = MIRROR_FEEDBACK_NONE;
    bridge->state.social_phase = CIRCADIAN_SOCIAL_HIGH;
    bridge->state.last_observation_time = now;
    bridge->state.last_imitation_time = now;

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, BIO_MODULE_MIRROR_HYPOTHALAMUS_BRIDGE, "mirror_hypothalamus") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "mirror_hypo_create: validation failed");
        return NULL;
    }

    bridge->enabled = false;
    bridge->last_update_time = now;
    bridge->creation_time = now;

    NIMCP_LOGGING_INFO("Mirror-hypothalamus bridge created (module ID: 0x%04X)",
                       BIO_MODULE_MIRROR_HYPOTHALAMUS_BRIDGE);
    return bridge;
}

void mirror_hypo_destroy(mirror_hypo_bridge_t* bridge) {
    /* WHAT: Clean up bridge
     * WHY:  Free resources
     * HOW:  Destroy mutex, free struct */
    if (!bridge) return;

    /* Phase 8: Heartbeat at operation start */
    mirror_hypothalamus_bridge_heartbeat("mirror_hypot_mirror_hypo_destroy", 0.0f);


    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
    NIMCP_LOGGING_INFO("Mirror-hypothalamus bridge destroyed");
}

int mirror_hypo_enable(mirror_hypo_bridge_t* bridge) {
    /* WHAT: Activate bridge
     * WHY:  Begin bidirectional modulation
     * HOW:  Set flag, reset timers */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hypo_enable: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hypothalamus_bridge_heartbeat("mirror_hypot_mirror_hypo_enable", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->enabled = true;
    uint64_t now = get_current_time_us();
    bridge->last_update_time = now;
    bridge->state.last_observation_time = now;
    bridge->state.last_imitation_time = now;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Mirror-hypothalamus bridge enabled");
    return 0;
}

int mirror_hypo_disable(mirror_hypo_bridge_t* bridge) {
    /* WHAT: Deactivate bridge
     * WHY:  Allow independent operation
     * HOW:  Clear flag, reset modulation */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hypo_disable: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hypothalamus_bridge_heartbeat("mirror_hypot_mirror_hypo_disable", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->enabled = false;
    bridge->state.stress_suppression = 0.0f;
    bridge->state.circadian_threshold_mod = 0.0f;
    bridge->state.drive_suppression = 0.0f;
    bridge->state.current_effect = HYPO_EFFECT_NONE;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Mirror-hypothalamus bridge disabled");
    return 0;
}

int mirror_hypo_reset(mirror_hypo_bridge_t* bridge) {
    /* WHAT: Reset bridge to initial state
     * WHY:  Prepare for new simulation
     * HOW:  Zero state, reset counters */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hypo_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hypothalamus_bridge_heartbeat("mirror_hypot_mirror_hypo_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset state */
    memset(&bridge->state, 0, sizeof(bridge->state));
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    uint64_t now = get_current_time_us();
    bridge->state.last_observation_time = now;
    bridge->state.last_imitation_time = now;
    bridge->state.social_phase = CIRCADIAN_SOCIAL_HIGH;
    bridge->last_update_time = now;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Mirror-hypothalamus bridge reset");
    return 0;
}

/* ============================================================================
 * Hypothalamus -> Mirror Neuron Modulation Implementation
 * ============================================================================ */

int mirror_hypo_apply_modulation(mirror_hypo_bridge_t* bridge) {
    /* WHAT: Update mirror parameters from hypothalamus state
     * WHY:  Stress, circadian, drives affect social cognition
     * HOW:  Sample hypothalamus, compute effects, apply */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_hypo_apply_modulation: bridge is NULL");
        return -1;
    }
    if (!bridge->enabled) return 0;  /* Disabled bridge is normal — no-op */

    /* Phase 8: Heartbeat at operation start */
    mirror_hypothalamus_bridge_heartbeat("mirror_hypot_mirror_hypo_apply_mo", 0.0f);


    /* Sample hypothalamus state OUTSIDE mutex to avoid deadlock */
    float cortisol = hypothalamus_get_cortisol(bridge->hypothalamus);

    appetite_state_t appetite;
    bool has_appetite = hypothalamus_get_appetite(bridge->hypothalamus, &appetite);

    hydration_state_t hydration;
    bool has_hydration = hypothalamus_get_hydration(bridge->hypothalamus, &hydration);

    autonomic_state_t autonomic;
    bool has_autonomic = hypothalamus_get_autonomic(bridge->hypothalamus, &autonomic);

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state.cortisol_level = cortisol;
    if (has_appetite) {
        bridge->state.hunger_drive = appetite.hunger_drive;
    }
    if (has_hydration) {
        bridge->state.thirst_drive = hydration.thirst_drive;
    }
    if (has_autonomic) {
        bridge->state.sympathetic_tone = autonomic.sympathetic_tone;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Compute and apply modulation effects */
    float stress_supp = mirror_hypo_compute_stress_suppression(bridge);
    float circadian_mod = mirror_hypo_compute_circadian_modifier(bridge);
    float drive_supp = mirror_hypo_compute_drive_suppression(bridge);

    /* Get social phase OUTSIDE mutex (calls into hypothalamus) */
    circadian_social_phase_t social_phase = mirror_hypo_get_social_phase(bridge);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->state.stress_suppression = stress_supp;
    bridge->state.circadian_threshold_mod = circadian_mod;
    bridge->state.drive_suppression = drive_supp;
    bridge->state.social_phase = social_phase;

    /* Determine active effect type */
    if (stress_supp > 0.1f && drive_supp > 0.1f) {
        bridge->state.current_effect = HYPO_EFFECT_COMBINED;
    } else if (stress_supp > 0.1f) {
        bridge->state.current_effect = HYPO_EFFECT_STRESS_SUPPRESSION;
        bridge->stats.stress_suppression_events++;
    } else if (drive_supp > 0.1f) {
        bridge->state.current_effect = HYPO_EFFECT_DRIVE_OVERRIDE;
        bridge->stats.drive_override_events++;
    } else if (circadian_mod > 0.1f) {
        bridge->state.current_effect = HYPO_EFFECT_CIRCADIAN_GATING;
        bridge->stats.circadian_gating_events++;
    } else {
        bridge->state.current_effect = HYPO_EFFECT_NONE;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Apply to mirror neuron resonance system if available */
    motor_resonance_t resonance = mirror_neurons_get_resonance(bridge->mirror_system);
    if (resonance) {
        float total_supp = mirror_hypo_get_total_suppression(bridge);
        float bg_inhibition = 0.5f + total_supp * 0.4f;
        motor_resonance_set_bg_inhibition(resonance, bg_inhibition);
    }

    return 0;
}

float mirror_hypo_compute_stress_suppression(const mirror_hypo_bridge_t* bridge) {
    /* WHAT: Calculate suppression from cortisol
     * WHY:  Stress reduces social mirroring sensitivity
     * HOW:  Scale cortisol by sensitivity */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hypo_compute_stress_suppression: bridge is NULL");
        return 0.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hypothalamus_bridge_heartbeat("mirror_hypot_mirror_hypo_compute_", 0.0f);


    float cortisol = bridge->state.cortisol_level;
    float sensitivity = bridge->config.cortisol_sensitivity;
    float max_supp = bridge->config.max_stress_suppression;

    float suppression = cortisol * sensitivity;
    return nimcp_myelin_clamp(suppression, 0.0f, max_supp);
}

float mirror_hypo_compute_circadian_modifier(const mirror_hypo_bridge_t* bridge) {
    /* WHAT: Calculate observation threshold modifier
     * WHY:  Circadian phase affects social receptivity
     * HOW:  Map circadian phase to modifier */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_hypo_compute_circadian_modifier: bridge is NULL");
        return 0.0f;
    }
    if (!bridge->config.enable_circadian_gating) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    mirror_hypothalamus_bridge_heartbeat("mirror_hypot_mirror_hypo_compute_", 0.0f);


    hypo_circadian_phase_t phase = hypothalamus_get_circadian_phase(bridge->hypothalamus);
    float night_boost = bridge->config.circadian_night_threshold_boost;

    /* Map circadian phase to social threshold modifier */
    switch (phase) {
        case HYPO_CIRCADIAN_PHASE_EARLY_MORNING:
        case HYPO_CIRCADIAN_PHASE_LATE_MORNING:
            return 0.0f;  /* High social receptivity */

        case HYPO_CIRCADIAN_PHASE_EARLY_AFTERNOON:
        case HYPO_CIRCADIAN_PHASE_LATE_AFTERNOON:
            return night_boost * 0.3f;  /* Slight boost */

        case HYPO_CIRCADIAN_PHASE_EVENING:
            return night_boost * 0.5f;  /* Moderate boost */

        case HYPO_CIRCADIAN_PHASE_EARLY_NIGHT:
        case HYPO_CIRCADIAN_PHASE_MID_NIGHT:
        case HYPO_CIRCADIAN_PHASE_LATE_NIGHT:
            return night_boost;  /* Full night boost */

        default:
            return 0.0f;
    }
}

float mirror_hypo_compute_drive_suppression(const mirror_hypo_bridge_t* bridge) {
    /* WHAT: Calculate suppression from drives
     * WHY:  Survival drives override social learning
     * HOW:  Check thresholds, compute weighted suppression */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_hypo_compute_drive_suppression: bridge is NULL");
        return 0.0f;
    }
    if (!bridge->config.enable_drive_suppression) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    mirror_hypothalamus_bridge_heartbeat("mirror_hypot_mirror_hypo_compute_", 0.0f);


    float hunger = bridge->state.hunger_drive;
    float thirst = bridge->state.thirst_drive;
    float hunger_thresh = bridge->config.hunger_suppression_threshold;
    float thirst_thresh = bridge->config.thirst_suppression_threshold;
    float gain = bridge->config.drive_suppression_gain;

    float hunger_contrib = 0.0f;
    float thirst_contrib = 0.0f;

    if (hunger > hunger_thresh) {
        hunger_contrib = (hunger - hunger_thresh) / (1.0f - hunger_thresh);
    }

    if (thirst > thirst_thresh) {
        thirst_contrib = (thirst - thirst_thresh) / (1.0f - thirst_thresh);
    }

    /* Take max of drives (competing survival needs) */
    float max_drive = (hunger_contrib > thirst_contrib) ? hunger_contrib : thirst_contrib;

    return nimcp_myelin_clamp(max_drive * gain, 0.0f, 1.0f);
}

circadian_social_phase_t mirror_hypo_get_social_phase(const mirror_hypo_bridge_t* bridge) {
    /* WHAT: Map hypothalamus circadian to social phase
     * WHY:  Simplify social receptivity assessment
     * HOW:  Query hypothalamus, map phases */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hypo_get_social_phase: bridge is NULL");
        return CIRCADIAN_SOCIAL_HIGH;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hypothalamus_bridge_heartbeat("mirror_hypot_mirror_hypo_get_soci", 0.0f);


    hypo_circadian_phase_t phase = hypothalamus_get_circadian_phase(bridge->hypothalamus);

    switch (phase) {
        case HYPO_CIRCADIAN_PHASE_EARLY_MORNING:
        case HYPO_CIRCADIAN_PHASE_LATE_MORNING:
            return CIRCADIAN_SOCIAL_HIGH;

        case HYPO_CIRCADIAN_PHASE_EARLY_AFTERNOON:
        case HYPO_CIRCADIAN_PHASE_LATE_AFTERNOON:
            return CIRCADIAN_SOCIAL_MODERATE;

        case HYPO_CIRCADIAN_PHASE_EVENING:
            return CIRCADIAN_SOCIAL_LOW;

        case HYPO_CIRCADIAN_PHASE_EARLY_NIGHT:
        case HYPO_CIRCADIAN_PHASE_MID_NIGHT:
        case HYPO_CIRCADIAN_PHASE_LATE_NIGHT:
            return CIRCADIAN_SOCIAL_MINIMAL;

        default:
            return CIRCADIAN_SOCIAL_MODERATE;
    }
}

/* ============================================================================
 * Mirror Neuron -> Hypothalamus Feedback Implementation
 * ============================================================================ */

int mirror_hypo_trigger_isolation_stress(mirror_hypo_bridge_t* bridge) {
    /* WHAT: Activate HPA on isolation
     * WHY:  Social isolation triggers stress hormones
     * HOW:  Apply stress to hypothalamus */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_hypo_trigger_isolation_stress: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_isolation_stress) return 0;

    /* Phase 8: Heartbeat at operation start */
    mirror_hypothalamus_bridge_heartbeat("mirror_hypot_mirror_hypo_trigger_", 0.0f);


    float stress_level = bridge->config.isolation_stress_level;
    float cortisol_delta = hypothalamus_apply_stress(bridge->hypothalamus, stress_level);

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state.last_feedback = MIRROR_FEEDBACK_ISOLATION;
    bridge->state.last_isolation_trigger = get_current_time_us();
    bridge->stats.isolation_stress_triggers++;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Isolation stress triggered: cortisol delta=%.3f", cortisol_delta);
    return 0;
}

int mirror_hypo_trigger_empathic_arousal(mirror_hypo_bridge_t* bridge, float arousal_level) {
    /* WHAT: Activate sympathetic on empathy
     * WHY:  Observing distress activates autonomic
     * HOW:  Modulate autonomic balance */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_hypo_trigger_empathic_arousal: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_empathic_arousal) return 0;

    /* Phase 8: Heartbeat at operation start */
    mirror_hypothalamus_bridge_heartbeat("mirror_hypot_mirror_hypo_trigger_", 0.0f);


    arousal_level = nimcp_myelin_clamp(arousal_level, 0.0f, 1.0f);

    /* Apply stress proportional to arousal (empathic stress transfer) */
    float stress_level = arousal_level * bridge->config.empathic_arousal_gain;
    hypothalamus_apply_stress(bridge->hypothalamus, stress_level);

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state.last_feedback = MIRROR_FEEDBACK_AROUSAL;
    bridge->state.last_arousal_trigger = get_current_time_us();
    bridge->stats.empathic_arousal_triggers++;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Empathic arousal triggered: level=%.2f", arousal_level);
    return 0;
}

int mirror_hypo_send_reward_signal(mirror_hypo_bridge_t* bridge) {
    /* WHAT: Send reward on successful imitation
     * WHY:  Social learning activates reward
     * HOW:  Reduce stress, signal positive outcome */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_hypo_send_reward_signal: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_imitation_reward) return 0;

    /* Negative stress = reward/relaxation effect */
    /* Phase 8: Heartbeat at operation start */
    mirror_hypothalamus_bridge_heartbeat("mirror_hypot_mirror_hypo_send_rew", 0.0f);


    float reward_level = -bridge->config.imitation_reward_signal * 0.5f;
    hypothalamus_apply_stress(bridge->hypothalamus, reward_level);

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state.last_feedback = MIRROR_FEEDBACK_REWARD;
    bridge->state.last_reward_trigger = get_current_time_us();
    bridge->stats.imitation_reward_signals++;
    bridge->state.failed_imitation_count = 0;  /* Reset failures on success */
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Imitation reward signal sent");
    return 0;
}

int mirror_hypo_trigger_rejection_stress(mirror_hypo_bridge_t* bridge) {
    /* WHAT: Trigger stress on repeated failures
     * WHY:  Social rejection activates HPA
     * HOW:  Check failure count, apply stress */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hypo_trigger_rejection_stress: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hypothalamus_bridge_heartbeat("mirror_hypot_mirror_hypo_trigger_", 0.0f);


    const uint32_t REJECTION_THRESHOLD = 5;

    nimcp_mutex_lock(bridge->base.mutex);
    uint32_t failures = bridge->state.failed_imitation_count;
    nimcp_mutex_unlock(bridge->base.mutex);

    if (failures < REJECTION_THRESHOLD) {
        return 0;  /* Not enough failures yet */
    }

    /* Apply rejection stress */
    float stress_level = bridge->config.isolation_stress_level * 0.7f;
    hypothalamus_apply_stress(bridge->hypothalamus, stress_level);

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state.last_feedback = MIRROR_FEEDBACK_REJECTION;
    bridge->state.failed_imitation_count = 0;  /* Reset counter */
    bridge->stats.rejection_stress_triggers++;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Rejection stress triggered: failures=%u", REJECTION_THRESHOLD);
    return 0;
}

/* ============================================================================
 * Update and Query API Implementation
 * ============================================================================ */

int mirror_hypo_update(mirror_hypo_bridge_t* bridge, uint64_t current_time) {
    /* WHAT: Process bidirectional integration
     * WHY:  Maintain coupling between systems
     * HOW:  Check interval, apply modulation, check triggers */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_hypo_update: bridge is NULL");
        return -1;
    }
    if (!bridge->enabled) return 0;  /* Disabled bridge is normal — no-op */

    /* Check if update interval elapsed */
    /* Phase 8: Heartbeat at operation start */
    mirror_hypothalamus_bridge_heartbeat("mirror_hypot_mirror_hypo_update", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t elapsed_ms = (current_time - bridge->last_update_time) / 1000;
    if (elapsed_ms < bridge->config.update_interval_ms) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;  /* Too soon */
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Apply hypothalamus -> mirror modulation */
    mirror_hypo_apply_modulation(bridge);

    /* Check for mirror -> hypothalamus triggers */
    if (check_isolation(bridge, current_time)) {
        mirror_hypo_trigger_isolation_stress(bridge);
    }

    /* Check for empathic arousal trigger */
    nimcp_mutex_lock(bridge->base.mutex);
    float resonance = bridge->state.empathic_resonance_level;
    nimcp_mutex_unlock(bridge->base.mutex);

    if (resonance > 0.7f) {
        mirror_hypo_trigger_empathic_arousal(bridge, resonance);
    }

    /* Check for rejection stress */
    mirror_hypo_trigger_rejection_stress(bridge);

    /* Update timing stats */
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->last_update_time = current_time;
    bridge->stats.total_updates++;

    /* Update running averages */
    float alpha = 0.01f;
    bridge->stats.avg_stress_suppression =
        (1.0f - alpha) * bridge->stats.avg_stress_suppression +
        alpha * bridge->state.stress_suppression;
    bridge->stats.avg_circadian_threshold_mod =
        (1.0f - alpha) * bridge->stats.avg_circadian_threshold_mod +
        alpha * bridge->state.circadian_threshold_mod;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int mirror_hypo_get_stats(mirror_hypo_bridge_t* bridge, mirror_hypo_stats_t* stats) {
    /* WHAT: Retrieve statistics
     * WHY:  Monitor bridge health
     * HOW:  Copy accumulated metrics */
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hypo_get_stats: bridge or stats is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hypothalamus_bridge_heartbeat("mirror_hypot_mirror_hypo_get_stat", 0.0f);


    nimcp_mutex_lock(((mirror_hypo_bridge_t*)bridge)->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(((mirror_hypo_bridge_t*)bridge)->base.mutex);

    return 0;
}

int mirror_hypo_get_state(mirror_hypo_bridge_t* bridge, mirror_hypo_state_t* state) {
    /* WHAT: Retrieve current state
     * WHY:  Query modulation effects
     * HOW:  Copy state struct */
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hypo_get_state: bridge or state is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hypothalamus_bridge_heartbeat("mirror_hypot_mirror_hypo_get_stat", 0.0f);


    nimcp_mutex_lock(((mirror_hypo_bridge_t*)bridge)->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(((mirror_hypo_bridge_t*)bridge)->base.mutex);

    return 0;
}

mirror_hypo_effect_t mirror_hypo_get_effect(const mirror_hypo_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hypo_get_effect: bridge is NULL");
        return HYPO_EFFECT_NONE;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hypothalamus_bridge_heartbeat("mirror_hypot_mirror_hypo_get_effe", 0.0f);


    return bridge->state.current_effect;
}

mirror_hypo_feedback_t mirror_hypo_get_last_feedback(const mirror_hypo_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hypo_get_last_feedback: bridge is NULL");
        return MIRROR_FEEDBACK_NONE;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hypothalamus_bridge_heartbeat("mirror_hypot_mirror_hypo_get_last", 0.0f);


    return bridge->state.last_feedback;
}

float mirror_hypo_get_total_suppression(const mirror_hypo_bridge_t* bridge) {
    /* WHAT: Get combined suppression
     * WHY:  Single value for modulation
     * HOW:  Combine stress, circadian, drive effects */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hypo_get_total_suppression: bridge is NULL");
        return 0.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hypothalamus_bridge_heartbeat("mirror_hypot_mirror_hypo_get_tota", 0.0f);


    float stress = bridge->state.stress_suppression;
    float circadian = bridge->state.circadian_threshold_mod;
    float drive = bridge->state.drive_suppression;

    /* Use max suppression (competing effects don't stack fully) */
    float max_supp = stress;
    if (circadian > max_supp) max_supp = circadian;
    if (drive > max_supp) max_supp = drive;

    /* Add minor contribution from secondary effects */
    float total = max_supp + (stress + circadian + drive - max_supp) * 0.3f;

    return nimcp_myelin_clamp(total, 0.0f, 1.0f);
}

/* ============================================================================
 * Event Notification API Implementation
 * ============================================================================ */

void mirror_hypo_notify_observation(mirror_hypo_bridge_t* bridge, uint64_t timestamp_us) {
    /* WHAT: Update observation timestamp
     * WHY:  Track social activity
     * HOW:  Set last observation time */
    if (!bridge || !bridge->enabled) return;  /* Disabled bridge — no-op */

    /* Phase 8: Heartbeat at operation start */
    mirror_hypothalamus_bridge_heartbeat("mirror_hypot_mirror_hypo_notify_o", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state.last_observation_time = timestamp_us;
    nimcp_mutex_unlock(bridge->base.mutex);
}

void mirror_hypo_notify_imitation_success(mirror_hypo_bridge_t* bridge, uint64_t timestamp_us) {
    /* WHAT: Record successful imitation
     * WHY:  Trigger reward signal
     * HOW:  Update timestamp, send reward */
    if (!bridge || !bridge->enabled) return;  /* Disabled bridge — no-op */

    /* Phase 8: Heartbeat at operation start */
    mirror_hypothalamus_bridge_heartbeat("mirror_hypot_mirror_hypo_notify_i", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state.last_imitation_time = timestamp_us;
    nimcp_mutex_unlock(bridge->base.mutex);

    mirror_hypo_send_reward_signal(bridge);
}

void mirror_hypo_notify_imitation_failure(mirror_hypo_bridge_t* bridge) {
    /* WHAT: Count imitation failure
     * WHY:  Track for rejection stress
     * HOW:  Increment counter */
    if (!bridge || !bridge->enabled) return;  /* Disabled bridge — no-op */

    /* Phase 8: Heartbeat at operation start */
    mirror_hypothalamus_bridge_heartbeat("mirror_hypot_mirror_hypo_notify_i", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state.failed_imitation_count++;
    nimcp_mutex_unlock(bridge->base.mutex);
}

void mirror_hypo_notify_empathic_resonance(mirror_hypo_bridge_t* bridge, float resonance_level) {
    /* WHAT: Update empathic resonance
     * WHY:  Trigger arousal if high
     * HOW:  Store level for update check */
    if (!bridge || !bridge->enabled) return;  /* Disabled bridge — no-op */

    /* Phase 8: Heartbeat at operation start */
    mirror_hypothalamus_bridge_heartbeat("mirror_hypot_mirror_hypo_notify_e", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state.empathic_resonance_level = nimcp_myelin_clamp(resonance_level, 0.0f, 1.0f);
    nimcp_mutex_unlock(bridge->base.mutex);
}

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

const char* mirror_hypo_effect_to_string(mirror_hypo_effect_t effect) {
    switch (effect) {
        case HYPO_EFFECT_NONE:              return "NONE";
        case HYPO_EFFECT_STRESS_SUPPRESSION: return "STRESS_SUPPRESSION";
        case HYPO_EFFECT_CIRCADIAN_GATING:  return "CIRCADIAN_GATING";
        case HYPO_EFFECT_DRIVE_OVERRIDE:    return "DRIVE_OVERRIDE";
        case HYPO_EFFECT_COMBINED:          return "COMBINED";
        default:                            return "UNKNOWN";
    }
}

const char* mirror_hypo_feedback_to_string(mirror_hypo_feedback_t feedback) {
    switch (feedback) {
        case MIRROR_FEEDBACK_NONE:      return "NONE";
        case MIRROR_FEEDBACK_ISOLATION: return "ISOLATION";
        case MIRROR_FEEDBACK_AROUSAL:   return "AROUSAL";
        case MIRROR_FEEDBACK_REWARD:    return "REWARD";
        case MIRROR_FEEDBACK_REJECTION: return "REJECTION";
        default:                        return "UNKNOWN";
    }
}

const char* circadian_social_phase_to_string(circadian_social_phase_t phase) {
    switch (phase) {
        case CIRCADIAN_SOCIAL_HIGH:     return "HIGH";
        case CIRCADIAN_SOCIAL_MODERATE: return "MODERATE";
        case CIRCADIAN_SOCIAL_LOW:      return "LOW";
        case CIRCADIAN_SOCIAL_MINIMAL:  return "MINIMAL";
        default:                        return "UNKNOWN";
    }
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void mirror_hypothalamus_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_mirror_hypothalamus_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int mirror_hypothalamus_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_hypothalamus_bridge_training_begin: NULL argument");
        return -1;
    }
    mirror_hypothalamus_bridge_heartbeat_instance(NULL, "mirror_hypothalamus_bridge_training_begin", 0.0f);
    return 0;
}

int mirror_hypothalamus_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_hypothalamus_bridge_training_end: NULL argument");
        return -1;
    }
    mirror_hypothalamus_bridge_heartbeat_instance(NULL, "mirror_hypothalamus_bridge_training_end", 1.0f);
    return 0;
}

int mirror_hypothalamus_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_hypothalamus_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    mirror_hypothalamus_bridge_heartbeat_instance(NULL, "mirror_hypothalamus_bridge_training_step", progress);
    return 0;
}
