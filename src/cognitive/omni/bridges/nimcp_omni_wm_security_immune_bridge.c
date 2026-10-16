/**
 * @file nimcp_omni_wm_security_immune_bridge.c
 * @brief World Model Security-Immune Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-17
 *
 * WHAT: Bidirectional bridge connecting World Model with Security and Immune systems
 * WHY:  Enable security-aware predictions with immune cytokine modulation
 * HOW:  WM predicts threats; security events train WM; cytokines modulate predictions
 *
 * IMPLEMENTATION NOTES:
 * =====================
 * This implementation integrates several key concepts:
 *
 * 1. PREDICTIVE SECURITY:
 *    - Use RSSM forward predictions to forecast anomalies
 *    - Train world model from verified security events
 *    - Model BBB dynamics for security boundary awareness
 *
 * 2. CYTOKINE MODULATION (Neuroimmune Effects):
 *    - IL-1: Reduces confidence, shortens horizon, increases vigilance
 *    - IL-6: Accelerates updates, biases toward threat detection
 *    - TNF-alpha: Conservative predictions, triggers immune on high PE
 *    - IL-10: Restores normal operation, enables refinement
 *    - IFN-gamma: Enhances pattern learning, updates threat signatures
 *
 * 3. PE -> IMMUNE TRIGGERING:
 *    - Large prediction errors may indicate threats
 *    - Present PE as antigen to immune system for response
 */

#include "cognitive/omni/bridges/nimcp_omni_wm_security_immune_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>

/* ============================================================================
 * Module-level Constants
 * ============================================================================ */

#define LOG_MODULE "wm_security_immune_bridge"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(omni_wm_security_immune_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_omni_wm_security_immune_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_omni_wm_security_immune_bridge_mesh_registry = NULL;

nimcp_error_t omni_wm_security_immune_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_omni_wm_security_immune_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "omni_wm_security_immune_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "omni_wm_security_immune_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_omni_wm_security_immune_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_omni_wm_security_immune_bridge_mesh_registry = registry;
    return err;
}

void omni_wm_security_immune_bridge_mesh_unregister(void) {
    if (g_omni_wm_security_immune_bridge_mesh_registry && g_omni_wm_security_immune_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_omni_wm_security_immune_bridge_mesh_registry, g_omni_wm_security_immune_bridge_mesh_id);
        g_omni_wm_security_immune_bridge_mesh_id = 0;
        g_omni_wm_security_immune_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from omni_wm_security_immune_bridge module (instance-level) */
static inline void omni_wm_security_immune_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_omni_wm_security_immune_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_omni_wm_security_immune_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_omni_wm_security_immune_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

void omni_wm_security_immune_bridge_set_instance_health_agent(
    omni_wm_security_immune_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "omni_wm_security_immune_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Instance-level Training Functions
 * ============================================================================ */

int omni_wm_security_immune_bridge_training_begin(omni_wm_security_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_wm_security_immune_bridge_training_begin: NULL argument");
        return -1;
    }
    omni_wm_security_immune_bridge_heartbeat_instance(g_omni_wm_security_immune_bridge_health_agent, "training_begin", 0.0f);
    (void)bridge;
    return 0;
}

int omni_wm_security_immune_bridge_training_step(omni_wm_security_immune_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_wm_security_immune_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    omni_wm_security_immune_bridge_heartbeat_instance(g_omni_wm_security_immune_bridge_health_agent, "training_step", progress);
    (void)bridge;
    return 0;
}

int omni_wm_security_immune_bridge_training_end(omni_wm_security_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_wm_security_immune_bridge_training_end: NULL argument");
        return -1;
    }
    omni_wm_security_immune_bridge_heartbeat_instance(g_omni_wm_security_immune_bridge_health_agent, "training_end", 1.0f);
    (void)bridge;
    return 0;
}

/** Default prediction buffer capacity */
#define DEFAULT_PREDICTION_BUFFER_CAPACITY 16

/** Default event buffer capacity */
#define DEFAULT_EVENT_BUFFER_CAPACITY 32

/** Default signature cache capacity */
#define DEFAULT_SIGNATURE_CAPACITY 64

/** Cytokine decay per second (biological half-life approximation) */
#define DEFAULT_CYTOKINE_DECAY_RATE 0.1f

/** Minimum combined confidence (safety floor) */
#define MIN_COMBINED_CONFIDENCE 0.3f

/** Maximum combined confidence */
#define MAX_COMBINED_CONFIDENCE 1.2f

/** Minimum horizon multiplier (safety floor) */
#define MIN_HORIZON_MULTIPLIER 0.2f

/** Maximum learning rate multiplier */
#define MAX_LEARNING_MULTIPLIER 2.5f

/* ============================================================================
 * Internal Helper Forward Declarations
 * ============================================================================ */

static nimcp_error_t allocate_prediction_buffer(
    omni_wm_security_immune_bridge_t* bridge);
static void free_prediction_buffer(omni_wm_security_immune_bridge_t* bridge);
static nimcp_error_t allocate_event_buffer(
    omni_wm_security_immune_bridge_t* bridge);
static void free_event_buffer(omni_wm_security_immune_bridge_t* bridge);
static nimcp_error_t allocate_signature_cache(
    omni_wm_security_immune_bridge_t* bridge);
static void free_signature_cache(omni_wm_security_immune_bridge_t* bridge);

static nimcp_error_t update_cytokine_effects(
    omni_wm_security_immune_bridge_t* bridge, float dt);
static nimcp_error_t update_wm_to_security_effects(
    omni_wm_security_immune_bridge_t* bridge);
static nimcp_error_t update_security_to_wm_effects(
    omni_wm_security_immune_bridge_t* bridge);
static nimcp_error_t process_pending_events(
    omni_wm_security_immune_bridge_t* bridge);
static nimcp_error_t decay_cytokines(
    omni_wm_security_immune_bridge_t* bridge, float dt);

static uint64_t get_current_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Allocate prediction buffer
 */
static nimcp_error_t allocate_prediction_buffer(
    omni_wm_security_immune_bridge_t* bridge) {

    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    uint32_t capacity = bridge->config.max_active_forecasts;
    if (capacity == 0) capacity = DEFAULT_PREDICTION_BUFFER_CAPACITY;

    bridge->prediction_buffer = nimcp_calloc(
        capacity, sizeof(wm_threat_prediction_t));
    if (!bridge->prediction_buffer) return NIMCP_ERROR_NO_MEMORY;

    bridge->prediction_buffer_capacity = capacity;
    bridge->prediction_buffer_size = 0;

    return NIMCP_SUCCESS;
}

/**
 * @brief Free prediction buffer
 */
static void free_prediction_buffer(omni_wm_security_immune_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->prediction_buffer) {
        /* Free any allocated state vectors in predictions */
        for (uint32_t i = 0; i < bridge->prediction_buffer_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->prediction_buffer_size > 256) {
                omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_loop",
                                 (float)(i + 1) / (float)bridge->prediction_buffer_size);
            }

            nimcp_free(bridge->prediction_buffer[i].predicted_state);
        }
        nimcp_free(bridge->prediction_buffer);
        bridge->prediction_buffer = NULL;
    }
    bridge->prediction_buffer_size = 0;
    bridge->prediction_buffer_capacity = 0;
}

/**
 * @brief Allocate event buffer
 */
static nimcp_error_t allocate_event_buffer(
    omni_wm_security_immune_bridge_t* bridge) {

    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    uint32_t capacity = bridge->config.max_pending_events;
    if (capacity == 0) capacity = DEFAULT_EVENT_BUFFER_CAPACITY;

    bridge->event_buffer = nimcp_calloc(
        capacity, sizeof(security_event_for_wm_t));
    if (!bridge->event_buffer) return NIMCP_ERROR_NO_MEMORY;

    bridge->event_buffer_capacity = capacity;
    bridge->event_buffer_size = 0;

    return NIMCP_SUCCESS;
}

/**
 * @brief Free event buffer
 */
static void free_event_buffer(omni_wm_security_immune_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->event_buffer) {
        for (uint32_t i = 0; i < bridge->event_buffer_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->event_buffer_size > 256) {
                omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_loop",
                                 (float)(i + 1) / (float)bridge->event_buffer_size);
            }

            nimcp_free(bridge->event_buffer[i].pre_event_state);
            nimcp_free(bridge->event_buffer[i].post_event_state);
            nimcp_free(bridge->event_buffer[i].threat_signature);
        }
        nimcp_free(bridge->event_buffer);
        bridge->event_buffer = NULL;
    }
    bridge->event_buffer_size = 0;
    bridge->event_buffer_capacity = 0;
}

/**
 * @brief Allocate signature cache
 */
static nimcp_error_t allocate_signature_cache(
    omni_wm_security_immune_bridge_t* bridge) {

    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    uint32_t capacity = DEFAULT_SIGNATURE_CAPACITY;
    bridge->signature_cache = nimcp_calloc(capacity, sizeof(float*));
    if (!bridge->signature_cache) return NIMCP_ERROR_NO_MEMORY;

    bridge->signature_capacity = capacity;
    bridge->signature_count = 0;
    bridge->signature_dim = WM_SECURITY_MAX_STATE_DIM;

    return NIMCP_SUCCESS;
}

/**
 * @brief Free signature cache
 */
static void free_signature_cache(omni_wm_security_immune_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->signature_cache) {
        for (uint32_t i = 0; i < bridge->signature_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->signature_count > 256) {
                omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_loop",
                                 (float)(i + 1) / (float)bridge->signature_count);
            }

            nimcp_free(bridge->signature_cache[i]);
        }
        nimcp_free(bridge->signature_cache);
        bridge->signature_cache = NULL;
    }
    bridge->signature_count = 0;
    bridge->signature_capacity = 0;
}

/**
 * @brief Decay cytokine levels over time
 *
 * WHAT: Apply exponential decay to cytokine levels
 * WHY:  Cytokines naturally decay, restoring normal function
 * HOW:  level = level * exp(-decay_rate * dt)
 */
static nimcp_error_t decay_cytokines(
    omni_wm_security_immune_bridge_t* bridge, float dt) {

    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    immune_to_wm_modulation_t* mod = &bridge->current_modulation;
    float decay = expf(-mod->decay_rate * dt);

    /* Decay pro-inflammatory cytokines */
    mod->il1_effect.level *= decay;
    mod->il6_effect.level *= decay;
    mod->tnf_alpha_effect.level *= decay;
    mod->ifn_gamma_effect.level *= decay;

    /* IL-10 decays slower (anti-inflammatory resolution) */
    mod->il10_effect.level *= powf(decay, 0.5f);

    /* Clamp small values to zero */
    if (mod->il1_effect.level < 0.01f) mod->il1_effect.level = 0.0f;
    if (mod->il6_effect.level < 0.01f) mod->il6_effect.level = 0.0f;
    if (mod->tnf_alpha_effect.level < 0.01f) mod->tnf_alpha_effect.level = 0.0f;
    if (mod->il10_effect.level < 0.01f) mod->il10_effect.level = 0.0f;
    if (mod->ifn_gamma_effect.level < 0.01f) mod->ifn_gamma_effect.level = 0.0f;

    return NIMCP_SUCCESS;
}

/**
 * @brief Update cytokine effects on WM parameters
 *
 * WHAT: Compute combined modulation from all cytokines
 * WHY:  Translate cytokine levels to prediction parameter changes
 * HOW:  Apply configured factors for each cytokine type
 */
static nimcp_error_t update_cytokine_effects(
    omni_wm_security_immune_bridge_t* bridge, float dt) {

    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    const omni_wm_security_immune_config_t* cfg = &bridge->config;
    immune_to_wm_modulation_t* mod = &bridge->current_modulation;

    /* Decay existing cytokine levels */
    decay_cytokines(bridge, dt);

    /* Compute IL-1 effects (pro-inflammatory, sickness behavior) */
    mod->il1_effect.confidence_multiplier =
        1.0f - (mod->il1_effect.level * (1.0f - cfg->il1_confidence_factor));
    mod->il1_effect.horizon_multiplier =
        1.0f - (mod->il1_effect.level * (1.0f - cfg->il1_horizon_factor));
    mod->il1_effect.learning_rate_multiplier = 1.0f;
    mod->il1_effect.vigilance_boost =
        mod->il1_effect.level * cfg->il1_vigilance_boost;

    /* Compute IL-6 effects (acute phase, accelerated response) */
    mod->il6_effect.confidence_multiplier = 1.0f;
    mod->il6_effect.horizon_multiplier = 1.0f;
    mod->il6_effect.learning_rate_multiplier =
        1.0f + (mod->il6_effect.level * (cfg->il6_learning_factor - 1.0f));
    mod->il6_effect.vigilance_boost =
        mod->il6_effect.level * cfg->il6_vigilance_boost;

    /* Compute TNF-alpha effects (damage signal, conservative) */
    mod->tnf_alpha_effect.confidence_multiplier =
        1.0f - (mod->tnf_alpha_effect.level * (1.0f - cfg->tnf_confidence_factor));
    mod->tnf_alpha_effect.horizon_multiplier =
        1.0f - (mod->tnf_alpha_effect.level * 0.5f);
    mod->tnf_alpha_effect.learning_rate_multiplier =
        1.0f - (mod->tnf_alpha_effect.level * 0.3f);
    mod->tnf_alpha_effect.vigilance_boost =
        mod->tnf_alpha_effect.level * cfg->tnf_conservatism_boost;

    /* Compute IL-10 effects (anti-inflammatory, restoration) */
    float restoration = mod->il10_effect.level * cfg->il10_restoration_rate;
    mod->il10_effect.confidence_multiplier = 1.0f + (restoration * 0.1f);
    mod->il10_effect.horizon_multiplier = 1.0f + (restoration * 0.1f);
    mod->il10_effect.learning_rate_multiplier = 1.0f;
    mod->il10_effect.vigilance_boost = -mod->il10_effect.level * 0.1f;

    /* Compute IFN-gamma effects (adaptive immunity, learning) */
    mod->ifn_gamma_effect.confidence_multiplier = 1.0f;
    mod->ifn_gamma_effect.horizon_multiplier = 1.0f;
    mod->ifn_gamma_effect.learning_rate_multiplier =
        1.0f + (mod->ifn_gamma_effect.level * (cfg->ifn_learning_boost - 1.0f));
    mod->ifn_gamma_effect.vigilance_boost = 0.0f;

    /* Compute combined modulation values */
    mod->combined_confidence_mod =
        mod->il1_effect.confidence_multiplier *
        mod->tnf_alpha_effect.confidence_multiplier *
        mod->il10_effect.confidence_multiplier;

    mod->combined_horizon_mod =
        mod->il1_effect.horizon_multiplier *
        mod->tnf_alpha_effect.horizon_multiplier *
        mod->il10_effect.horizon_multiplier;

    mod->combined_learning_mod =
        mod->il6_effect.learning_rate_multiplier *
        mod->tnf_alpha_effect.learning_rate_multiplier *
        mod->ifn_gamma_effect.learning_rate_multiplier;

    mod->combined_vigilance =
        mod->il1_effect.vigilance_boost +
        mod->il6_effect.vigilance_boost +
        mod->tnf_alpha_effect.vigilance_boost +
        mod->il10_effect.vigilance_boost +
        mod->ifn_gamma_effect.vigilance_boost;

    /* Clamp combined values to safe ranges */
    mod->combined_confidence_mod = nimcp_clampf(
        mod->combined_confidence_mod, MIN_COMBINED_CONFIDENCE, MAX_COMBINED_CONFIDENCE);
    mod->combined_horizon_mod = nimcp_clampf(
        mod->combined_horizon_mod, MIN_HORIZON_MULTIPLIER, 1.0f);
    mod->combined_learning_mod = nimcp_clampf(
        mod->combined_learning_mod, 0.5f, MAX_LEARNING_MULTIPLIER);
    mod->combined_vigilance = nimcp_clampf(
        mod->combined_vigilance, 0.0f, 1.0f);

    /* Check for cytokine storm (dangerous condition) */
    float total_proinflammatory =
        mod->il1_effect.level +
        mod->il6_effect.level +
        mod->tnf_alpha_effect.level;

    mod->is_cytokine_storm = (total_proinflammatory > 2.5f);
    if (mod->is_cytokine_storm) {
        bridge->stats.cytokine_storms++;
        NIMCP_LOGGING_WARN("Cytokine storm detected! Total pro-inflammatory: %.2f",
                          total_proinflammatory);
    }

    /* Update inflammation level based on cytokine profile */
    if (total_proinflammatory < 0.3f) {
        mod->inflammation_level = 0; /* None */
    } else if (total_proinflammatory < 0.8f) {
        mod->inflammation_level = 1; /* Local */
    } else if (total_proinflammatory < 1.5f) {
        mod->inflammation_level = 2; /* Regional */
    } else if (total_proinflammatory < 2.5f) {
        mod->inflammation_level = 3; /* Systemic */
    } else {
        mod->inflammation_level = 4; /* Storm */
    }

    mod->last_update_us = get_current_time_us();

    return NIMCP_SUCCESS;
}

/**
 * @brief Update effects flowing from WM to security
 */
static nimcp_error_t update_wm_to_security_effects(
    omni_wm_security_immune_bridge_t* bridge) {

    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    omni_wm_to_security_effects_t* effects = &bridge->wm_to_security;

    /* Update timestamp */
    effects->snapshot_timestamp = (double)get_current_time_us() / 1000000.0;

    /* If world model connected, extract state and compute PE */
    if (bridge->world_model) {
        /* Placeholder: would query actual WM state */
        effects->state_uncertainty = 0.3f * bridge->current_modulation.combined_confidence_mod;

        /* Compute PE-based immune trigger */
        effects->combined_pe = effects->forward_pe * 0.6f + effects->backward_pe * 0.4f;
        effects->pe_precision = bridge->current_modulation.combined_confidence_mod;

        float pe_threshold = bridge->config.pe_immune_threshold /
                             (1.0f + bridge->current_modulation.combined_vigilance);

        effects->should_trigger_immune = (effects->combined_pe > pe_threshold);
        effects->immune_trigger_strength = nimcp_clampf(
            (effects->combined_pe - pe_threshold) * bridge->config.pe_immune_scale,
            0.0f, 1.0f);
    }

    /* Update anomaly status */
    effects->anomaly_detected =
        (effects->anomaly_score > bridge->config.anomaly_threshold) &&
        (effects->anomaly_confidence > bridge->config.anomaly_confidence_min);

    return NIMCP_SUCCESS;
}

/**
 * @brief Update effects flowing from security to WM
 */
static nimcp_error_t update_security_to_wm_effects(
    omni_wm_security_immune_bridge_t* bridge) {

    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    security_immune_to_wm_effects_t* effects = &bridge->security_to_wm;

    /* Copy current modulation to effects */
    effects->immune_modulation = bridge->current_modulation;
    effects->modulation_active = bridge->modulation_active;

    /* Update alert state */
    effects->security_alert_level = bridge->alert_level;
    effects->under_attack = bridge->under_attack;

    return NIMCP_SUCCESS;
}

/**
 * @brief Process pending security events for WM training
 */
static nimcp_error_t process_pending_events(
    omni_wm_security_immune_bridge_t* bridge) {

    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->world_model) return NIMCP_SUCCESS;
    if (bridge->event_buffer_size == 0) return NIMCP_SUCCESS;

    /* Process events (placeholder - would invoke WM training) */
    for (uint32_t i = 0; i < bridge->event_buffer_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->event_buffer_size > 256) {
            omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_loop",
                             (float)(i + 1) / (float)bridge->event_buffer_size);
        }

        security_event_for_wm_t* event = &bridge->event_buffer[i];

        /* In full implementation:
         * 1. Create training sample from pre/post states
         * 2. Call omni_wm_update() with event data
         * 3. Store threat signature if novel */

        bridge->stats.security_events_processed++;

        /* Store threat signature if we have capacity */
        if (event->threat_signature && bridge->signature_count < bridge->signature_capacity) {
            float* sig_copy = nimcp_calloc(event->signature_dim, sizeof(float));
            if (sig_copy) {
                memcpy(sig_copy, event->threat_signature,
                       event->signature_dim * sizeof(float));
                bridge->signature_cache[bridge->signature_count++] = sig_copy;
                bridge->stats.threat_signatures_learned++;
            }
        }

        /* Free event data */
        nimcp_free(event->pre_event_state);
        nimcp_free(event->post_event_state);
        nimcp_free(event->threat_signature);
        memset(event, 0, sizeof(security_event_for_wm_t));
    }

    bridge->event_buffer_size = 0;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async Message Handlers
 * ============================================================================ */

static nimcp_error_t handle_anomaly_detected(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t promise, void* user_data) {

    (void)msg;
    (void)msg_size;
    (void)promise;

    if (!user_data) return NIMCP_ERROR_NULL_POINTER;

    omni_wm_security_immune_bridge_t* bridge =
        (omni_wm_security_immune_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.anomalies_predicted++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_security_event(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t promise, void* user_data) {

    (void)msg;
    (void)msg_size;
    (void)promise;

    if (!user_data) return NIMCP_ERROR_NULL_POINTER;

    omni_wm_security_immune_bridge_t* bridge =
        (omni_wm_security_immune_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    /* Would parse event and add to buffer */
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_bbb_state(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t promise, void* user_data) {

    (void)msg;
    (void)msg_size;
    (void)promise;

    if (!user_data) return NIMCP_ERROR_NULL_POINTER;

    omni_wm_security_immune_bridge_t* bridge =
        (omni_wm_security_immune_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.bbb_state_updates++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_cytokine_update(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t promise, void* user_data) {

    (void)msg;
    (void)msg_size;
    (void)promise;

    if (!user_data) return NIMCP_ERROR_NULL_POINTER;

    omni_wm_security_immune_bridge_t* bridge =
        (omni_wm_security_immune_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.modulation_updates++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_inflammation_state(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t promise, void* user_data) {

    (void)msg;
    (void)msg_size;
    (void)promise;

    if (!user_data) return NIMCP_ERROR_NULL_POINTER;

    /* Would parse inflammation level and update modulation */
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_security_immune_bridge_default_config(
    omni_wm_security_immune_config_t* config) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_default_config", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    memset(config, 0, sizeof(omni_wm_security_immune_config_t));

    /* General settings */
    config->enable_modulation = true;
    config->sensitivity = 1.0f;

    /* Anomaly detection settings */
    config->enable_anomaly_prediction = true;
    config->anomaly_threshold = WM_SECURITY_DEFAULT_ANOMALY_THRESHOLD;
    config->anomaly_confidence_min = 0.6f;
    config->anomaly_feature_count = 12;

    /* Threat forecasting settings */
    config->enable_threat_forecasting = true;
    config->forecast_horizon = 8;
    config->forecast_confidence_min = 0.7f;
    config->max_active_forecasts = 16;

    /* Security event training settings */
    config->enable_security_training = true;
    config->security_learning_rate = 0.001f;
    config->max_pending_events = 32;
    config->event_priority_decay = 0.95f;

    /* BBB integration settings */
    config->enable_bbb_modeling = true;
    config->bbb_state_weight = 0.3f;
    config->bbb_breach_pe_threshold = 0.5f;

    /* Immune modulation settings */
    config->enable_immune_modulation = true;
    config->immune_sensitivity = WM_SECURITY_DEFAULT_IMMUNE_SENSITIVITY;
    config->cytokine_decay_rate = DEFAULT_CYTOKINE_DECAY_RATE;

    /* IL-1 parameters (pro-inflammatory, sickness behavior) */
    config->il1_confidence_factor = 0.75f;  /* Reduces confidence to 75% */
    config->il1_horizon_factor = 0.6f;      /* Reduces horizon to 60% */
    config->il1_vigilance_boost = 0.2f;     /* Increases vigilance by 20% */

    /* IL-6 parameters (acute phase, accelerated response) */
    config->il6_learning_factor = 1.3f;     /* Increases learning by 30% */
    config->il6_update_factor = 1.2f;       /* Increases update rate by 20% */
    config->il6_vigilance_boost = 0.15f;    /* Increases vigilance by 15% */

    /* TNF-alpha parameters (damage signal, conservative) */
    config->tnf_confidence_factor = 0.6f;   /* Reduces confidence to 60% */
    config->tnf_conservatism_boost = 0.3f;  /* Increases conservatism by 30% */
    config->tnf_pe_immune_threshold = 0.4f; /* PE threshold for immune trigger */

    /* IL-10 parameters (anti-inflammatory, restoration) */
    config->il10_restoration_rate = 0.5f;   /* Restoration rate */
    config->il10_resolution_factor = 1.2f;  /* Resolution acceleration */

    /* IFN-gamma parameters (adaptive immunity, learning) */
    config->ifn_learning_boost = 1.8f;      /* Increases learning by 80% */
    config->ifn_pattern_strength = 1.5f;    /* Pattern encoding strength */

    /* PE -> Immune trigger settings */
    config->enable_pe_immune_trigger = true;
    config->pe_immune_threshold = 0.5f;
    config->pe_immune_scale = 1.0f;

    /* Bio-async settings */
    config->enable_bio_async = true;

    return NIMCP_SUCCESS;
}

omni_wm_security_immune_bridge_t* omni_wm_security_immune_bridge_create(
    const omni_wm_security_immune_config_t* config) {

    /* Allocate bridge structure */
    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_create", 0.0f);


    omni_wm_security_immune_bridge_t* bridge =
        nimcp_calloc(1, sizeof(omni_wm_security_immune_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate WM security-immune bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: bridge is NULL");
        return NULL;
    }

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, BIO_MODULE_WM_SECURITY_IMMUNE_BRIDGE,
                         "wm_security_immune_bridge") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_LOGGING_ERROR("Failed to initialize bridge base");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: operation failed");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        omni_wm_security_immune_bridge_default_config(&bridge->config);
    }

    /* Allocate prediction buffer */
    nimcp_error_t err = allocate_prediction_buffer(bridge);
    if (err != NIMCP_SUCCESS) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_LOGGING_ERROR("Failed to allocate prediction buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: validation failed");
        return NULL;
    }

    /* Allocate event buffer */
    err = allocate_event_buffer(bridge);
    if (err != NIMCP_SUCCESS) {
        free_prediction_buffer(bridge);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_LOGGING_ERROR("Failed to allocate event buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: validation failed");
        return NULL;
    }

    /* Allocate signature cache */
    err = allocate_signature_cache(bridge);
    if (err != NIMCP_SUCCESS) {
        free_event_buffer(bridge);
        free_prediction_buffer(bridge);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_LOGGING_ERROR("Failed to allocate signature cache");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: validation failed");
        return NULL;
    }

    /* Initialize baseline values */
    bridge->baseline_confidence = 1.0f;
    bridge->baseline_horizon = bridge->config.forecast_horizon;
    bridge->baseline_learning_rate = bridge->config.security_learning_rate;

    /* Initialize modulation to neutral */
    bridge->current_modulation.combined_confidence_mod = 1.0f;
    bridge->current_modulation.combined_horizon_mod = 1.0f;
    bridge->current_modulation.combined_learning_mod = 1.0f;
    bridge->current_modulation.combined_vigilance = 0.0f;
    bridge->current_modulation.decay_rate = bridge->config.cytokine_decay_rate;
    bridge->current_modulation.inflammation_level = 0;
    bridge->current_modulation.is_cytokine_storm = false;

    /* Initialize state */
    bridge->under_attack = false;
    bridge->alert_level = 0;
    bridge->modulation_active = bridge->config.enable_immune_modulation;

    NIMCP_LOGGING_INFO("WM Security-Immune Bridge created successfully");
    return bridge;
}

void omni_wm_security_immune_bridge_destroy(
    omni_wm_security_immune_bridge_t* bridge) {

    if (!bridge) return;

    /* Disconnect bio-async if connected */
    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        omni_wm_security_immune_bridge_disconnect_bio_async(bridge);
    }

    /* Free WM to security effects dynamic arrays */
    nimcp_free(bridge->wm_to_security.anomaly_features);
    if (bridge->wm_to_security.active_predictions) {
        for (uint32_t i = 0; i < bridge->wm_to_security.num_predictions; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->wm_to_security.num_predictions > 256) {
                omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_loop",
                                 (float)(i + 1) / (float)bridge->wm_to_security.num_predictions);
            }

            nimcp_free(bridge->wm_to_security.active_predictions[i].predicted_state);
        }
        nimcp_free(bridge->wm_to_security.active_predictions);
    }
    nimcp_free(bridge->wm_to_security.current_state_snapshot);

    /* Free security to WM effects dynamic arrays */
    if (bridge->security_to_wm.pending_events) {
        for (uint32_t i = 0; i < bridge->security_to_wm.num_pending_events; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->security_to_wm.num_pending_events > 256) {
                omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_loop",
                                 (float)(i + 1) / (float)bridge->security_to_wm.num_pending_events);
            }

            nimcp_free(bridge->security_to_wm.pending_events[i].pre_event_state);
            nimcp_free(bridge->security_to_wm.pending_events[i].post_event_state);
            nimcp_free(bridge->security_to_wm.pending_events[i].threat_signature);
        }
        nimcp_free(bridge->security_to_wm.pending_events);
    }
    if (bridge->security_to_wm.threat_signatures) {
        for (uint32_t i = 0; i < bridge->security_to_wm.num_signatures; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->security_to_wm.num_signatures > 256) {
                omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_loop",
                                 (float)(i + 1) / (float)bridge->security_to_wm.num_signatures);
            }

            nimcp_free(bridge->security_to_wm.threat_signatures[i]);
        }
        nimcp_free(bridge->security_to_wm.threat_signatures);
    }

    /* Free internal buffers */
    free_prediction_buffer(bridge);
    free_event_buffer(bridge);
    free_signature_cache(bridge);

    /* Cleanup base and free */
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;

    NIMCP_LOGGING_INFO("WM Security-Immune Bridge destroyed");
}

nimcp_error_t omni_wm_security_immune_bridge_reset(
    omni_wm_security_immune_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_reset", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset effects */
    memset(&bridge->wm_to_security, 0, sizeof(omni_wm_to_security_effects_t));
    memset(&bridge->security_to_wm, 0, sizeof(security_immune_to_wm_effects_t));

    /* Reset modulation to baseline */
    bridge->current_modulation.il1_effect.level = 0.0f;
    bridge->current_modulation.il6_effect.level = 0.0f;
    bridge->current_modulation.tnf_alpha_effect.level = 0.0f;
    bridge->current_modulation.il10_effect.level = 0.0f;
    bridge->current_modulation.ifn_gamma_effect.level = 0.0f;
    bridge->current_modulation.combined_confidence_mod = 1.0f;
    bridge->current_modulation.combined_horizon_mod = 1.0f;
    bridge->current_modulation.combined_learning_mod = 1.0f;
    bridge->current_modulation.combined_vigilance = 0.0f;
    bridge->current_modulation.inflammation_level = 0;
    bridge->current_modulation.is_cytokine_storm = false;

    /* Reset internal state */
    bridge->under_attack = false;
    bridge->alert_level = 0;

    /* Clear buffers */
    bridge->prediction_buffer_size = 0;
    bridge->event_buffer_size = 0;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(omni_wm_security_immune_stats_t));

    /* Reset base bridge (unlocked since we already hold the mutex) */
    bridge_base_reset_unlocked(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_security_immune_bridge_connect(
    omni_wm_security_immune_bridge_t* bridge,
    omni_world_model_t* world_model,
    nimcp_security_system_t* security,
    bbb_system_t bbb_system,
    brain_immune_system_t* immune) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_connect", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(world_model, NIMCP_ERROR_INVALID_PARAM, "world_model is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->world_model = world_model;
    bridge->security = security;
    bridge->bbb_system = bbb_system;
    bridge->immune = immune;

    /* Update base connection state */
    bridge->base.system_a = world_model;
    bridge->base.system_a_connected = true;
    bridge->base.bridge_active = true;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("WM Security-Immune Bridge connected: WM=%p, Security=%p, "
                       "BBB=%p, Immune=%p",
                       (void*)world_model, (void*)security,
                       (void*)bbb_system, (void*)immune);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_security_immune_bridge_connect_world_model(
    omni_wm_security_immune_bridge_t* bridge,
    omni_world_model_t* world_model) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_connect_world_model", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(world_model, NIMCP_ERROR_NULL_POINTER, "world_model is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->world_model = world_model;
    bridge->base.system_a = world_model;
    bridge->base.system_a_connected = true;
    bridge->base.bridge_active = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_security_immune_bridge_connect_security(
    omni_wm_security_immune_bridge_t* bridge,
    nimcp_security_system_t* security) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_connect_security", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(security, NIMCP_ERROR_NULL_POINTER, "security is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->security = security;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_security_immune_bridge_connect_bbb(
    omni_wm_security_immune_bridge_t* bridge,
    bbb_system_t bbb_system) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_connect_bbb", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bbb_system, NIMCP_ERROR_NULL_POINTER, "bbb_system is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bbb_system = bbb_system;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_security_immune_bridge_connect_immune(
    omni_wm_security_immune_bridge_t* bridge,
    brain_immune_system_t* immune) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_connect_immune", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(immune, NIMCP_ERROR_NULL_POINTER, "immune is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->immune = immune;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_security_immune_bridge_connect_anomaly_detector(
    omni_wm_security_immune_bridge_t* bridge,
    nimcp_anomaly_detector_t detector) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_connect_anomaly_dete", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(detector, NIMCP_ERROR_NULL_POINTER, "detector is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->anomaly_detector = detector;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

bool omni_wm_security_immune_bridge_is_connected(
    const omni_wm_security_immune_bridge_t* bridge) {

    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_is_connected", 0.0f);


    return bridge->world_model != NULL;
}

/* ============================================================================
 * Update API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_security_immune_bridge_update(
    omni_wm_security_immune_bridge_t* bridge,
    float dt) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_update", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Validate dt - negative timesteps are invalid */
    if (dt < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "omni_wm_security_immune_bridge_update: negative dt (%.4f)", dt);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Must be connected (world_model set) to perform updates */
    if (!bridge->world_model) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED,
            "omni_wm_security_immune_bridge_update: bridge not connected (world_model is NULL)");
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    if (!bridge->config.enable_modulation) return NIMCP_SUCCESS;

    uint64_t start_time = get_current_time_us();

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update cytokine effects (includes decay) */
    if (bridge->config.enable_immune_modulation) {
        update_cytokine_effects(bridge, dt);
    }

    /* Process pending security events */
    if (bridge->config.enable_security_training) {
        process_pending_events(bridge);
    }

    /* Update effects in both directions */
    update_wm_to_security_effects(bridge);
    update_security_to_wm_effects(bridge);

    /* Update timing statistics */
    bridge->stats.total_updates++;
    uint64_t elapsed = get_current_time_us() - start_time;
    bridge->stats.total_processing_time_ms += (double)elapsed / 1000.0;
    bridge->stats.mean_update_time_ms = bridge->stats.total_processing_time_ms /
                                        (double)bridge->stats.total_updates;
    bridge->stats.last_update_time_us = start_time;

    /* Update running statistics */
    bridge->stats.mean_confidence_modifier =
        0.1f * bridge->current_modulation.combined_confidence_mod +
        0.9f * bridge->stats.mean_confidence_modifier;
    bridge->stats.mean_vigilance_level =
        0.1f * bridge->current_modulation.combined_vigilance +
        0.9f * bridge->stats.mean_vigilance_level;

    /* Record base update */
    bridge_base_record_update(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Anomaly Prediction API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_security_immune_bridge_predict_anomaly(
    omni_wm_security_immune_bridge_t* bridge,
    uint32_t horizon_steps,
    float* anomaly_score_out,
    float* confidence_out) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_predict_anomaly", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(anomaly_score_out, NIMCP_ERROR_NULL_POINTER, "anomaly_score_out is NULL");
    NIMCP_CHECK_THROW(confidence_out, NIMCP_ERROR_NULL_POINTER, "confidence_out is NULL");
    NIMCP_CHECK_THROW(horizon_steps > 0, NIMCP_ERROR_INVALID_PARAM, "predict_anomaly: horizon_steps must be > 0");

    if (!bridge->config.enable_anomaly_prediction) {
        *anomaly_score_out = 0.0f;
        *confidence_out = 0.0f;
        return NIMCP_SUCCESS;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Adjust horizon based on immune modulation */
    uint32_t modulated_horizon = (uint32_t)(
        (float)horizon_steps * bridge->current_modulation.combined_horizon_mod);
    if (modulated_horizon < 1) modulated_horizon = 1;
    if (modulated_horizon > WM_SECURITY_MAX_FORECAST_HORIZON) {
        modulated_horizon = WM_SECURITY_MAX_FORECAST_HORIZON;
    }

    /* In full implementation, would:
     * 1. Get current WM state
     * 2. Roll out forward predictions
     * 3. Score trajectory for anomalies
     * For now, return placeholder based on vigilance */

    float base_score = bridge->current_modulation.combined_vigilance * 0.3f;
    if (bridge->under_attack) {
        base_score += 0.3f;
    }

    *anomaly_score_out = nimcp_clampf(base_score, 0.0f, 1.0f);
    *confidence_out = bridge->current_modulation.combined_confidence_mod * 0.8f;

    /* Update effects */
    bridge->wm_to_security.anomaly_score = *anomaly_score_out;
    bridge->wm_to_security.anomaly_confidence = *confidence_out;

    bridge->stats.anomalies_predicted++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_security_immune_bridge_report_anomaly(
    omni_wm_security_immune_bridge_t* bridge,
    bool was_true_positive,
    float actual_severity) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_report_anomaly", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    if (was_true_positive) {
        bridge->stats.anomalies_verified++;
    } else {
        bridge->stats.false_positives++;
    }

    /* Update precision/recall */
    uint64_t tp = bridge->stats.anomalies_verified;
    uint64_t fp = bridge->stats.false_positives;
    uint64_t fn = bridge->stats.false_negatives;

    if (tp + fp > 0) {
        bridge->stats.anomaly_precision = (float)tp / (float)(tp + fp);
    }
    if (tp + fn > 0) {
        bridge->stats.anomaly_recall = (float)tp / (float)(tp + fn);
    }

    (void)actual_severity; /* Would use for threshold adjustment */

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Threat Forecasting API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_security_immune_bridge_forecast_threat(
    omni_wm_security_immune_bridge_t* bridge,
    uint32_t threat_type,
    wm_threat_prediction_t* prediction_out) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_forecast_threat", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(prediction_out, NIMCP_ERROR_NULL_POINTER, "prediction_out is NULL");

    if (!bridge->config.enable_threat_forecasting) {
        memset(prediction_out, 0, sizeof(wm_threat_prediction_t));
        return NIMCP_SUCCESS;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* In full implementation, would:
     * 1. Get current state
     * 2. Roll out predictions
     * 3. Classify trajectory for threat_type
     * For now, return placeholder */

    prediction_out->threat_type = threat_type;
    prediction_out->confidence = bridge->current_modulation.combined_confidence_mod * 0.5f;
    prediction_out->horizon_steps = (uint32_t)(
        bridge->config.forecast_horizon *
        bridge->current_modulation.combined_horizon_mod);
    prediction_out->time_to_threat_ms = 1000.0f * prediction_out->horizon_steps;
    prediction_out->severity_estimate = bridge->current_modulation.combined_vigilance * 0.5f;
    prediction_out->timestamp_us = get_current_time_us();

    bridge->stats.threats_forecasted++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

const wm_threat_prediction_t* omni_wm_security_immune_bridge_get_active_predictions(
    const omni_wm_security_immune_bridge_t* bridge,
    uint32_t* count_out) {

    if (!bridge) {
        if (count_out) *count_out = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: validation failed");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_get_active_predictio", 0.0f);


    if (count_out) *count_out = bridge->prediction_buffer_size;
    nimcp_mutex_unlock(bridge->base.mutex);
    return bridge->prediction_buffer;
}

/* ============================================================================
 * Security Event API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_security_immune_bridge_process_security_event(
    omni_wm_security_immune_bridge_t* bridge,
    const security_event_for_wm_t* event) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_process_security_eve", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(event, NIMCP_ERROR_NULL_POINTER, "event is NULL");

    if (!bridge->config.enable_security_training) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check buffer capacity */
    if (bridge->event_buffer_size >= bridge->event_buffer_capacity) {
        /* Buffer full - process immediately or drop oldest */
        process_pending_events(bridge);
    }

    /* Copy event to buffer */
    if (bridge->event_buffer_size < bridge->event_buffer_capacity) {
        security_event_for_wm_t* dest =
            &bridge->event_buffer[bridge->event_buffer_size];

        dest->event_type = event->event_type;
        dest->severity = event->severity;
        dest->was_predicted = event->was_predicted;
        dest->prediction_lead_time_ms = event->prediction_lead_time_ms;
        dest->timestamp_us = event->timestamp_us;
        dest->state_dim = event->state_dim;
        dest->signature_dim = event->signature_dim;

        /* Copy state vectors */
        if (event->pre_event_state && event->state_dim > 0) {
            dest->pre_event_state = nimcp_calloc(event->state_dim, sizeof(float));
            if (dest->pre_event_state) {
                memcpy(dest->pre_event_state, event->pre_event_state,
                       event->state_dim * sizeof(float));
            }
        }
        if (event->post_event_state && event->state_dim > 0) {
            dest->post_event_state = nimcp_calloc(event->state_dim, sizeof(float));
            if (dest->post_event_state) {
                memcpy(dest->post_event_state, event->post_event_state,
                       event->state_dim * sizeof(float));
            }
        }
        if (event->threat_signature && event->signature_dim > 0) {
            dest->threat_signature = nimcp_calloc(event->signature_dim, sizeof(float));
            if (dest->threat_signature) {
                memcpy(dest->threat_signature, event->threat_signature,
                       event->signature_dim * sizeof(float));
            }
        }

        bridge->event_buffer_size++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_security_immune_bridge_update_bbb_state(
    omni_wm_security_immune_bridge_t* bridge,
    const bbb_state_for_wm_t* bbb_state) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_update_bbb_state", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bbb_state, NIMCP_ERROR_NULL_POINTER, "bbb_state is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->security_to_wm.bbb_state = *bbb_state;
    bridge->security_to_wm.bbb_state_valid = true;
    bridge->stats.bbb_state_updates++;

    /* Track breaches */
    if (bbb_state->breaches > 0) {
        bridge->stats.bbb_breaches_detected += bbb_state->breaches;
    }

    /* Update mean integrity */
    bridge->stats.mean_bbb_integrity =
        0.1f * bbb_state->integrity +
        0.9f * bridge->stats.mean_bbb_integrity;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_security_immune_bridge_set_alert_level(
    omni_wm_security_immune_bridge_t* bridge,
    uint32_t alert_level,
    bool under_attack) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_set_alert_level", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Valid alert levels are 0-4 */
    if (alert_level > 4) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "set_alert_level: level %u out of range (max 4)", alert_level);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bool was_under_attack = bridge->under_attack;
    bridge->alert_level = alert_level;
    bridge->under_attack = under_attack;

    /* Track attack timing */
    if (under_attack && !was_under_attack) {
        bridge->attack_start_time_us = get_current_time_us();
        NIMCP_LOGGING_WARN("Attack started, alert level %u", alert_level);
    } else if (!under_attack && was_under_attack) {
        NIMCP_LOGGING_INFO("Attack ended after %lu us",
                          get_current_time_us() - bridge->attack_start_time_us);
    }

    /* Update effects */
    bridge->security_to_wm.security_alert_level = alert_level;
    bridge->security_to_wm.under_attack = under_attack;
    bridge->security_to_wm.attack_severity = (float)alert_level / 4.0f;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Immune Modulation API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_security_immune_bridge_update_cytokines(
    omni_wm_security_immune_bridge_t* bridge,
    float il1_level,
    float il6_level,
    float tnf_alpha_level,
    float il10_level,
    float ifn_gamma_level) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_update_cytokines", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update cytokine levels (clamped to [0, 1]) */
    bridge->current_modulation.il1_effect.level = nimcp_clampf(il1_level, 0.0f, 1.0f);
    bridge->current_modulation.il6_effect.level = nimcp_clampf(il6_level, 0.0f, 1.0f);
    bridge->current_modulation.tnf_alpha_effect.level = nimcp_clampf(tnf_alpha_level, 0.0f, 1.0f);
    bridge->current_modulation.il10_effect.level = nimcp_clampf(il10_level, 0.0f, 1.0f);
    bridge->current_modulation.ifn_gamma_effect.level = nimcp_clampf(ifn_gamma_level, 0.0f, 1.0f);

    bridge->stats.modulation_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Cytokines updated: IL1=%.2f, IL6=%.2f, TNF=%.2f, IL10=%.2f, IFN=%.2f",
                       il1_level, il6_level, tnf_alpha_level, il10_level, ifn_gamma_level);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_security_immune_bridge_set_inflammation(
    omni_wm_security_immune_bridge_t* bridge,
    uint32_t inflammation_level) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_set_inflammation", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Valid inflammation levels are 0-4 */
    if (inflammation_level > 4) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "set_inflammation: level %u out of range (max 4)", inflammation_level);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Map inflammation level to cytokine profile */
    float base_level = (float)inflammation_level / 4.0f;

    bridge->current_modulation.il1_effect.level = base_level * 0.8f;
    bridge->current_modulation.il6_effect.level = base_level * 0.6f;
    bridge->current_modulation.tnf_alpha_effect.level = base_level * 0.5f;
    bridge->current_modulation.il10_effect.level = (1.0f - base_level) * 0.3f;
    bridge->current_modulation.ifn_gamma_effect.level = base_level * 0.3f;

    /* Level 4 = cytokine storm */
    bridge->current_modulation.is_cytokine_storm = (inflammation_level >= 4);

    bridge->current_modulation.inflammation_level = inflammation_level;
    bridge->stats.modulation_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

const immune_to_wm_modulation_t* omni_wm_security_immune_bridge_get_modulation(
    const omni_wm_security_immune_bridge_t* bridge) {

    if (!bridge) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return NULL;


    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_get_modulation", 0.0f);


    return &bridge->current_modulation;
}

nimcp_error_t omni_wm_security_immune_bridge_check_pe_trigger(
    omni_wm_security_immune_bridge_t* bridge,
    float prediction_error,
    bool* should_trigger_out,
    float* trigger_strength_out) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_check_pe_trigger", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(should_trigger_out, NIMCP_ERROR_NULL_POINTER, "should_trigger_out is NULL");
    NIMCP_CHECK_THROW(trigger_strength_out, NIMCP_ERROR_NULL_POINTER, "trigger_strength_out is NULL");

    if (!bridge->config.enable_pe_immune_trigger) {
        *should_trigger_out = false;
        *trigger_strength_out = 0.0f;
        return NIMCP_SUCCESS;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Adjust threshold based on vigilance (higher vigilance = lower threshold) */
    float adjusted_threshold = bridge->config.pe_immune_threshold /
        (1.0f + bridge->current_modulation.combined_vigilance);

    *should_trigger_out = (prediction_error > adjusted_threshold);
    *trigger_strength_out = nimcp_clampf(
        (prediction_error - adjusted_threshold) * bridge->config.pe_immune_scale,
        0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_security_immune_bridge_trigger_immune(
    omni_wm_security_immune_bridge_t* bridge,
    float prediction_error,
    uint32_t error_source) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_trigger_immune", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* In full implementation, would:
     * 1. Create antigen from PE
     * 2. Present to brain immune system
     * 3. Trigger appropriate response */

    bridge->stats.pe_immune_triggers++;
    bridge->wm_to_security.should_trigger_immune = true;
    bridge->wm_to_security.immune_trigger_strength = prediction_error;
    bridge->wm_to_security.suggested_response = error_source;

    NIMCP_LOGGING_DEBUG("PE immune trigger: error=%.3f, source=%u",
                       prediction_error, error_source);

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

const omni_wm_to_security_effects_t* omni_wm_security_immune_bridge_get_wm_effects(
    const omni_wm_security_immune_bridge_t* bridge) {

    if (!bridge) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return NULL;


    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_get_wm_effects", 0.0f);


    return &bridge->wm_to_security;
}

const security_immune_to_wm_effects_t* omni_wm_security_immune_bridge_get_security_effects(
    const omni_wm_security_immune_bridge_t* bridge) {

    if (!bridge) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return NULL;


    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_get_security_effects", 0.0f);


    return &bridge->security_to_wm;
}

nimcp_error_t omni_wm_security_immune_bridge_get_stats(
    const omni_wm_security_immune_bridge_t* bridge,
    omni_wm_security_immune_stats_t* stats) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_get_stats", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_security_immune_bridge_reset_stats(
    omni_wm_security_immune_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_reset_stats", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(omni_wm_security_immune_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

float omni_wm_security_immune_bridge_get_modulated_confidence(
    const omni_wm_security_immune_bridge_t* bridge) {

    if (!bridge) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_get_modulated_confid", 0.0f);


    return bridge->baseline_confidence *
           bridge->current_modulation.combined_confidence_mod;
}

uint32_t omni_wm_security_immune_bridge_get_modulated_horizon(
    const omni_wm_security_immune_bridge_t* bridge) {

    if (!bridge) return 0;
    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_get_modulated_horizo", 0.0f);


    uint32_t modulated = (uint32_t)(
        (float)bridge->baseline_horizon *
        bridge->current_modulation.combined_horizon_mod);
    return modulated > 0 ? modulated : 1;
}

float omni_wm_security_immune_bridge_get_modulated_learning_rate(
    const omni_wm_security_immune_bridge_t* bridge) {

    if (!bridge) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_get_modulated_learni", 0.0f);


    return bridge->baseline_learning_rate *
           bridge->current_modulation.combined_learning_mod;
}

/* ============================================================================
 * Bio-Async API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_security_immune_bridge_connect_bio_async(
    omni_wm_security_immune_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_connect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_bio_async) return NIMCP_SUCCESS;
    if (bridge->base.bio_async_enabled) return NIMCP_SUCCESS;

    /* Check if router is initialized */
    if (!bio_router_is_initialized()) {
        NIMCP_LOGGING_DEBUG("Bio-async router not initialized, skipping registration");
        return NIMCP_SUCCESS;
    }

    /* Register module with router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_WM_SECURITY_IMMUNE_BRIDGE,
        .module_name = "wm_security_immune_bridge",
        .inbox_capacity = 64,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (!bridge->base.bio_ctx) {
        NIMCP_LOGGING_WARN("Failed to register with bio-async router");
        return NIMCP_SUCCESS; /* Non-fatal */
    }

    /* Register message handlers */
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_SECURITY_ANOMALY_DETECTED,
                                handle_anomaly_detected);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_SECURITY_EVENT,
                                handle_security_event);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_SECURITY_BBB_STATE,
                                handle_bbb_state);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_IMMUNE_CYTOKINE_UPDATE,
                                handle_cytokine_update);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_IMMUNE_INFLAMMATION_STATE,
                                handle_inflammation_state);

    bridge->base.bio_async_enabled = true;
    NIMCP_LOGGING_INFO("WM Security-Immune Bridge connected to bio-async router");

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_security_immune_bridge_disconnect_bio_async(
    omni_wm_security_immune_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_disconnect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->base.bio_async_enabled) return NIMCP_SUCCESS;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("WM Security-Immune Bridge disconnected from bio-async router");

    return NIMCP_SUCCESS;
}

bool omni_wm_security_immune_bridge_is_bio_async_connected(
    const omni_wm_security_immune_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_is_bio_async_connect", 0.0f);


    return bridge_base_is_bio_async_connected(bridge ? &bridge->base : NULL);
}

/* ============================================================================
 * Utility Functions Implementation
 * ============================================================================ */

const char* omni_wm_security_immune_msg_type_to_string(
    omni_wm_security_immune_msg_type_t msg_type) {

    switch (msg_type) {
        case BIO_MSG_WM_SECURITY_ANOMALY_PRED:
            return "ANOMALY_PRED";
        case BIO_MSG_WM_SECURITY_ANOMALY_DETECTED:
            return "ANOMALY_DETECTED";
        case BIO_MSG_WM_SECURITY_ANOMALY_RESOLVED:
            return "ANOMALY_RESOLVED";
        case BIO_MSG_WM_SECURITY_THREAT_FORECAST:
            return "THREAT_FORECAST";
        case BIO_MSG_WM_SECURITY_THREAT_VERIFIED:
            return "THREAT_VERIFIED";
        case BIO_MSG_WM_SECURITY_THREAT_SIGNATURE:
            return "THREAT_SIGNATURE";
        case BIO_MSG_WM_SECURITY_EVENT:
            return "SECURITY_EVENT";
        case BIO_MSG_WM_SECURITY_BBB_STATE:
            return "BBB_STATE";
        case BIO_MSG_WM_SECURITY_BBB_BREACH:
            return "BBB_BREACH";
        case BIO_MSG_WM_IMMUNE_CYTOKINE_UPDATE:
            return "CYTOKINE_UPDATE";
        case BIO_MSG_WM_IMMUNE_MODULATION_APPLIED:
            return "MODULATION_APPLIED";
        case BIO_MSG_WM_IMMUNE_INFLAMMATION_STATE:
            return "INFLAMMATION_STATE";
        case BIO_MSG_WM_PE_IMMUNE_TRIGGER:
            return "PE_IMMUNE_TRIGGER";
        case BIO_MSG_WM_THREAT_IMMUNE_ALERT:
            return "THREAT_IMMUNE_ALERT";
        case BIO_MSG_WM_ANOMALY_ANTIGEN_PRESENT:
            return "ANOMALY_ANTIGEN_PRESENT";
        case BIO_MSG_WM_SECURITY_IMMUNE_STATUS:
            return "BRIDGE_STATUS";
        case BIO_MSG_WM_SECURITY_IMMUNE_ERROR:
            return "BRIDGE_ERROR";
        case BIO_MSG_WM_SECURITY_IMMUNE_STATS:
            return "STATS_UPDATE";
        default:
            return "UNKNOWN";
    }
}

nimcp_error_t omni_wm_security_immune_bridge_validate_config(
    const omni_wm_security_immune_config_t* config) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_validate_config", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    /* Validate sensitivity range */
    if (config->sensitivity < 0.5f || config->sensitivity > 2.0f) {
        NIMCP_LOGGING_WARN("Sensitivity %.2f out of range [0.5, 2.0]",
                          config->sensitivity);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Validate anomaly settings */
    if (config->enable_anomaly_prediction) {
        if (config->anomaly_threshold < 0.0f || config->anomaly_threshold > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid anomaly_threshold: %.2f",
                              config->anomaly_threshold);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate threat forecast settings */
    if (config->enable_threat_forecasting) {
        if (config->forecast_horizon == 0 ||
            config->forecast_horizon > WM_SECURITY_MAX_FORECAST_HORIZON) {
            NIMCP_LOGGING_WARN("Invalid forecast_horizon: %u",
                              config->forecast_horizon);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate immune modulation settings */
    if (config->enable_immune_modulation) {
        if (config->immune_sensitivity < 0.5f || config->immune_sensitivity > 2.0f) {
            NIMCP_LOGGING_WARN("Invalid immune_sensitivity: %.2f",
                              config->immune_sensitivity);
            return NIMCP_ERROR_INVALID_PARAM;
        }

        /* Validate cytokine factors */
        if (config->il1_confidence_factor < 0.5f || config->il1_confidence_factor > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid il1_confidence_factor: %.2f",
                              config->il1_confidence_factor);
            return NIMCP_ERROR_INVALID_PARAM;
        }
        if (config->tnf_confidence_factor < 0.3f || config->tnf_confidence_factor > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid tnf_confidence_factor: %.2f",
                              config->tnf_confidence_factor);
            return NIMCP_ERROR_INVALID_PARAM;
        }
        if (config->ifn_learning_boost < 1.0f || config->ifn_learning_boost > 3.0f) {
            NIMCP_LOGGING_WARN("Invalid ifn_learning_boost: %.2f",
                              config->ifn_learning_boost);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate PE trigger settings */
    if (config->enable_pe_immune_trigger) {
        if (config->pe_immune_threshold < 0.1f || config->pe_immune_threshold > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid pe_immune_threshold: %.2f",
                              config->pe_immune_threshold);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_security_immune_bridge_compute_modulation(
    omni_wm_security_immune_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_security_immune_bridge_heartbeat("omni_wm_secu_compute_modulation", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    nimcp_error_t err = update_cytokine_effects(bridge, 0.0f);
    nimcp_mutex_unlock(bridge->base.mutex);

    return err;
}
