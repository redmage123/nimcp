/**
 * @file nimcp_hypo_immune_fep_bridge.c
 * @brief Implementation of Hypothalamus Immune FEP Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for hypothalamic immune system coordination
 * WHY:  Cytokine levels represent high-surprise homeostatic deviations
 * HOW:  Map cytokine levels to free energy, immune response to prediction error
 */

#include "core/brain/regions/hypothalamus/fep/nimcp_hypo_immune_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hypo_immune_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Internal Helper Declarations
 * ============================================================================ */

static float compute_fe_from_cytokines(const hypo_immune_fep_bridge_t* bridge);

static hypo_immune_fep_state_t classify_immune_state(
    float free_energy,
    const hypo_immune_fep_config_t* config);

static hypo_immune_fep_response_t determine_response(
    hypo_immune_fep_state_t state,
    float urgency);

static hypo_immune_event_type_t identify_event(
    const hypo_immune_fep_bridge_t* bridge,
    float free_energy);

static void update_running_averages(hypo_immune_fep_bridge_t* bridge,
                                    float free_energy,
                                    float surprise,
                                    float pred_error);

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

/**
 * WHAT: Get default configuration for hypothalamus immune FEP bridge
 * WHY:  Provide sensible starting point for immune monitoring
 * HOW:  Set biologically-plausible defaults for cytokine weights
 */
int hypo_immune_fep_default_config(hypo_immune_fep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* FEP parameters */
    config->drive_fe_weight = 1.0f;
    config->prediction_error_gain = 2.0f;
    config->precision_modulation = 1.0f;
    config->enable_active_inference = true;
    config->enable_bio_async = true;

    /* Cytokine weights */
    config->il1_weight = HYPO_IMMUNE_FEP_IL1_WEIGHT;
    config->il6_weight = HYPO_IMMUNE_FEP_IL6_WEIGHT;
    config->tnf_weight = HYPO_IMMUNE_FEP_TNF_WEIGHT;
    config->il10_weight = HYPO_IMMUNE_FEP_IL10_WEIGHT;
    config->ifn_weight = 2.5f;

    /* Baseline levels */
    config->il1_baseline = HYPO_IMMUNE_FEP_IL1_BASELINE;
    config->il6_baseline = HYPO_IMMUNE_FEP_IL6_BASELINE;
    config->tnf_baseline = HYPO_IMMUNE_FEP_TNF_BASELINE;
    config->il10_baseline = HYPO_IMMUNE_FEP_IL10_BASELINE;
    config->ifn_baseline = HYPO_IMMUNE_FEP_IFN_BASELINE;

    /* Detection parameters */
    config->free_energy_threshold = HYPO_IMMUNE_FEP_INFLAMMATION_THRESHOLD;
    config->surprise_threshold = 8.0f;
    config->precision_learning_rate = 0.05f;

    /* Inflammation response */
    config->enable_fever_response = true;
    config->enable_sickness_behavior = true;
    config->inflammation_precision_reduction = 0.3f;

    /* Learning */
    config->enable_online_learning = true;
    config->learning_rate = 0.01f;

    return 0;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

/**
 * WHAT: Create hypothalamus immune FEP bridge
 * WHY:  Initialize FEP integration for immune monitoring
 * HOW:  Allocate structure, initialize base, apply configuration
 */
hypo_immune_fep_bridge_t* hypo_immune_fep_create(
    const hypo_immune_fep_config_t* config,
    hypo_drive_system_handle_t* drive_system,
    brain_immune_system_t* immune_system,
    fep_system_t* fep_system
) {
    /* Validate required parameters */
    if (!drive_system || !fep_system) {
        NIMCP_LOGGING_ERROR("Hypo Immune FEP bridge: NULL system pointers");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_immune_fep_create: required parameter is NULL (drive_system, fep_system)");
        return NULL;
    }

    /* Allocate bridge structure */
    hypo_immune_fep_bridge_t* bridge = (hypo_immune_fep_bridge_t*)nimcp_malloc(
        sizeof(hypo_immune_fep_bridge_t)
    );
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Hypo Immune FEP bridge: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_immune_fep_create: bridge is NULL");
        return NULL;
    }

    /* Zero initialize */
    memset(bridge, 0, sizeof(hypo_immune_fep_bridge_t));

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        hypo_immune_fep_default_config(&bridge->config);
    }

    /* Store system references */
    bridge->drive_system = drive_system;
    bridge->immune_system = immune_system;
    bridge->fep_system = fep_system;

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "hypo_immune_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Hypo Immune FEP bridge: mutex creation failed");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_immune_fep_create: bridge->base is NULL");
        return NULL;
    }

    /* Initialize state */
    bridge->state.active = true;
    bridge->state.current_precision = HYPO_IMMUNE_FEP_DEFAULT_PRECISION;
    bridge->state.avg_surprise = 0.0f;
    bridge->state.avg_prediction_error = 0.0f;
    bridge->state.last_state = HYPO_IMMUNE_FEP_STATE_NORMAL;

    /* Initialize cytokine effects to baseline */
    bridge->immune_effects.cytokine_il1 = bridge->config.il1_baseline;
    bridge->immune_effects.cytokine_il6 = bridge->config.il6_baseline;
    bridge->immune_effects.cytokine_tnf = bridge->config.tnf_baseline;
    bridge->immune_effects.cytokine_il10 = bridge->config.il10_baseline;
    bridge->immune_effects.cytokine_ifn = bridge->config.ifn_baseline;

    /* Initialize FEP effects */
    bridge->fep_effects.immune_state = HYPO_IMMUNE_FEP_STATE_NORMAL;
    bridge->fep_effects.precision = HYPO_IMMUNE_FEP_DEFAULT_PRECISION;
    bridge->fep_effects.immune_health = 1.0f;
    bridge->fep_effects.detected_event = HYPO_IMMUNE_EVENT_NONE;

    /* Bio-async not yet connected */
    bridge->base.bio_async_enabled = false;
    bridge->base.module_id = BIO_MODULE_HYPO_IMMUNE_FEP;
    bridge->base.module_name = "hypo_immune_fep_bridge";

    NIMCP_LOGGING_INFO("Hypo Immune FEP bridge created");
    return bridge;
}

/**
 * WHAT: Destroy hypothalamus immune FEP bridge
 * WHY:  Clean up all resources
 * HOW:  Disconnect bio-async, destroy mutex, free memory
 */
void hypo_immune_fep_destroy(hypo_immune_fep_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    if (bridge->base.bio_async_enabled) {
        hypo_immune_fep_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
        bridge->base.mutex = NULL;
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Hypo Immune FEP bridge destroyed");
}

/**
 * WHAT: Reset bridge to initial state
 * WHY:  Allow reuse without full recreation
 * HOW:  Clear state and statistics, preserve connections
 */
int hypo_immune_fep_reset(hypo_immune_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Reset state */
    bridge->state.update_count = 0;
    bridge->state.detection_count = 0;
    bridge->state.current_precision = HYPO_IMMUNE_FEP_DEFAULT_PRECISION;
    bridge->state.avg_surprise = 0.0f;
    bridge->state.avg_prediction_error = 0.0f;
    bridge->state.last_state = HYPO_IMMUNE_FEP_STATE_NORMAL;
    bridge->state.last_detection_time_ms = 0;

    /* Reset cytokine history */
    memset(bridge->state.cytokine_history, 0, sizeof(bridge->state.cytokine_history));
    bridge->state.history_idx = 0;

    /* Reset effects */
    memset(&bridge->fep_effects, 0, sizeof(hypo_immune_fep_effects_t));
    bridge->fep_effects.immune_health = 1.0f;
    bridge->fep_effects.precision = HYPO_IMMUNE_FEP_DEFAULT_PRECISION;

    /* Reset immune effects to baseline */
    bridge->immune_effects.cytokine_il1 = bridge->config.il1_baseline;
    bridge->immune_effects.cytokine_il6 = bridge->config.il6_baseline;
    bridge->immune_effects.cytokine_tnf = bridge->config.tnf_baseline;
    bridge->immune_effects.cytokine_il10 = bridge->config.il10_baseline;
    bridge->immune_effects.cytokine_ifn = bridge->config.ifn_baseline;
    bridge->immune_effects.activations_detected = 0;
    bridge->immune_effects.inflammations_detected = 0;
    bridge->immune_effects.resolutions_detected = 0;
    bridge->immune_effects.storms_detected = 0;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(hypo_immune_fep_stats_t));
    bridge->stats.current_precision = HYPO_IMMUNE_FEP_DEFAULT_PRECISION;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Hypo Immune FEP bridge reset");
    return 0;
}

/**
 * WHAT: Update bridge state
 * WHY:  Main update loop for bridge synchronization
 * HOW:  Compute effects, apply precision modulation, update state
 */
int hypo_immune_fep_update(hypo_immune_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute free energy from cytokines */
    float current_fe = compute_fe_from_cytokines(bridge);

    /* Get FEP system metrics */
    float surprise = fep_compute_surprise(bridge->fep_system);
    float pred_error = fep_get_prediction_error(bridge->fep_system, 0);

    /* Update running averages */
    update_running_averages(bridge, current_fe, surprise, pred_error);

    /* Store in FEP effects */
    bridge->fep_effects.free_energy = current_fe;
    bridge->fep_effects.prediction_error = pred_error;
    bridge->fep_effects.precision = bridge->state.current_precision;

    /* Classify immune state */
    bridge->fep_effects.immune_state = classify_immune_state(current_fe, &bridge->config);

    /* Identify event type */
    bridge->fep_effects.detected_event = identify_event(bridge, current_fe);

    /* Compute immune health estimate */
    float health = 1.0f - (current_fe / HYPO_IMMUNE_FEP_STORM_THRESHOLD);
    if (health < 0.0f) health = 0.0f;
    if (health > 1.0f) health = 1.0f;
    bridge->fep_effects.immune_health = health;

    /* Compute inflammation level */
    float inflammation = (bridge->immune_effects.cytokine_il1 +
                          bridge->immune_effects.cytokine_il6 +
                          bridge->immune_effects.cytokine_tnf) / 3.0f;
    bridge->fep_effects.inflammation_level = inflammation;

    /* Compute resolution progress (IL-10 mediated) */
    bridge->fep_effects.resolution_progress = bridge->immune_effects.cytokine_il10;

    /* Compute fever signal (IL-1 driven) */
    float fever_denom = 1.0f - bridge->config.il1_baseline;
    float fever = (fever_denom > 1e-6f)
        ? (bridge->immune_effects.cytokine_il1 - bridge->config.il1_baseline) / fever_denom
        : 0.0f;
    if (fever < 0.0f) fever = 0.0f;
    if (fever > 1.0f) fever = 1.0f;
    bridge->fep_effects.fever_signal = fever;

    /* Determine response */
    float urgency = current_fe / HYPO_IMMUNE_FEP_STORM_THRESHOLD;
    if (urgency > 1.0f) urgency = 1.0f;
    bridge->fep_effects.response_urgency = urgency;
    bridge->fep_effects.recommended_response = determine_response(
        bridge->fep_effects.immune_state, urgency
    );

    /* Apply precision reduction during inflammation */
    if (bridge->fep_effects.inflammation_level > 0.3f && bridge->config.enable_sickness_behavior) {
        float precision_reduction = bridge->fep_effects.inflammation_level *
                                    bridge->config.inflammation_precision_reduction;
        bridge->state.current_precision *= (1.0f - precision_reduction);
        if (bridge->state.current_precision < HYPO_IMMUNE_FEP_MIN_PRECISION) {
            bridge->state.current_precision = HYPO_IMMUNE_FEP_MIN_PRECISION;
        }
    }

    /* Update statistics */
    bridge->state.update_count++;
    bridge->stats.total_updates++;
    bridge->stats.current_precision = bridge->state.current_precision;

    if (current_fe > bridge->stats.max_free_energy) {
        bridge->stats.max_free_energy = current_fe;
    }
    if (bridge->fep_effects.inflammation_level > bridge->stats.max_inflammation) {
        bridge->stats.max_inflammation = bridge->fep_effects.inflammation_level;
    }

    /* Track event counts */
    if (bridge->fep_effects.detected_event != HYPO_IMMUNE_EVENT_NONE) {
        bridge->stats.immune_events_detected++;
        switch (bridge->fep_effects.detected_event) {
            case HYPO_IMMUNE_EVENT_ACTIVATION:
                bridge->stats.activations++;
                bridge->immune_effects.activations_detected++;
                break;
            case HYPO_IMMUNE_EVENT_INFLAMMATION:
                bridge->stats.inflammations++;
                bridge->immune_effects.inflammations_detected++;
                break;
            case HYPO_IMMUNE_EVENT_RESOLUTION:
                bridge->stats.resolutions++;
                bridge->immune_effects.resolutions_detected++;
                break;
            case HYPO_IMMUNE_EVENT_STORM:
                bridge->stats.storms++;
                bridge->immune_effects.storms_detected++;
                break;
            default:
                break;
        }
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Core Operations Implementation
 * ============================================================================ */

/**
 * WHAT: Compute free energy from drive state
 * WHY:  Core FEP computation for immune monitoring
 * HOW:  Map drive deviations and cytokines to free energy
 */
int hypo_immune_fep_compute_fe(
    hypo_immune_fep_bridge_t* bridge,
    const hypo_drive_system_t* drives
) {
    if (!bridge || !drives) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_immune_fep_compute_fe: bridge or drives is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute FE from drive deviations */
    float drive_fe = 0.0f;
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        float deviation = fabsf(drives->drives[i].deviation);
        float urgency = drives->drives[i].urgency;
        drive_fe += deviation * urgency * bridge->config.drive_fe_weight;
    }

    /* Compute FE from cytokines */
    float cytokine_fe = compute_fe_from_cytokines(bridge);

    /* Total free energy */
    float total_fe = drive_fe + cytokine_fe;

    /* Update effects */
    bridge->fep_effects.free_energy = total_fe;

    /* Update drive impacts */
    bridge->immune_effects.fatigue_drive_impact = bridge->fep_effects.inflammation_level * 0.5f;
    bridge->immune_effects.social_drive_impact = -bridge->fep_effects.inflammation_level * 0.3f;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Modulate precision based on fatigue
 * WHY:  Precision represents confidence; illness reduces confidence
 * HOW:  Scale precision inversely with fatigue and inflammation
 */
int hypo_immune_fep_modulate_precision(
    hypo_immune_fep_bridge_t* bridge,
    float fatigue
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (fatigue < 0.0f) fatigue = 0.0f;
    if (fatigue > 1.0f) fatigue = 1.0f;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Higher fatigue and inflammation = lower precision */
    float fatigue_mod = 1.0f - (fatigue * bridge->config.precision_modulation);
    float inflammation_mod = 1.0f - (bridge->fep_effects.inflammation_level *
                                      bridge->config.inflammation_precision_reduction);

    float precision_mod = fatigue_mod * inflammation_mod;
    if (precision_mod < 0.1f) precision_mod = 0.1f;

    float new_precision = HYPO_IMMUNE_FEP_DEFAULT_PRECISION * precision_mod;

    /* Smooth adaptation */
    float alpha = bridge->config.precision_learning_rate;
    bridge->state.current_precision =
        (1.0f - alpha) * bridge->state.current_precision + alpha * new_precision;

    /* Clamp */
    if (bridge->state.current_precision < HYPO_IMMUNE_FEP_MIN_PRECISION) {
        bridge->state.current_precision = HYPO_IMMUNE_FEP_MIN_PRECISION;
    }
    if (bridge->state.current_precision > HYPO_IMMUNE_FEP_MAX_PRECISION) {
        bridge->state.current_precision = HYPO_IMMUNE_FEP_MAX_PRECISION;
    }

    bridge->fep_effects.precision = bridge->state.current_precision;
    bridge->stats.precision_adaptations++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Get current FEP effects
 * WHY:  Allow inspection of current effects
 * HOW:  Copy effects structure
 */
int hypo_immune_fep_get_effects(
    const hypo_immune_fep_bridge_t* bridge,
    hypo_immune_fep_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_immune_fep_get_effects: bridge or effects is NULL");
        return -1;
    }

    *effects = bridge->fep_effects;
    return 0;
}

/**
 * WHAT: Get bridge statistics
 * WHY:  Performance monitoring
 * HOW:  Copy statistics structure
 */
int hypo_immune_fep_get_stats(
    const hypo_immune_fep_bridge_t* bridge,
    hypo_immune_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_immune_fep_get_stats: bridge or stats is NULL");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

/* ============================================================================
 * Cytokine Integration Implementation
 * ============================================================================ */

/**
 * WHAT: Update cytokine levels
 * WHY:  Cytokines are primary immune signals
 * HOW:  Store levels, update history
 */
int hypo_immune_fep_update_cytokines(
    hypo_immune_fep_bridge_t* bridge,
    float il1,
    float il6,
    float tnf,
    float il10,
    float ifn
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Clamp values */
    if (il1 < 0.0f) il1 = 0.0f; if (il1 > 1.0f) il1 = 1.0f;
    if (il6 < 0.0f) il6 = 0.0f; if (il6 > 1.0f) il6 = 1.0f;
    if (tnf < 0.0f) tnf = 0.0f; if (tnf > 1.0f) tnf = 1.0f;
    if (il10 < 0.0f) il10 = 0.0f; if (il10 > 1.0f) il10 = 1.0f;
    if (ifn < 0.0f) ifn = 0.0f; if (ifn > 1.0f) ifn = 1.0f;

    /* Store current values */
    bridge->immune_effects.cytokine_il1 = il1;
    bridge->immune_effects.cytokine_il6 = il6;
    bridge->immune_effects.cytokine_tnf = tnf;
    bridge->immune_effects.cytokine_il10 = il10;
    bridge->immune_effects.cytokine_ifn = ifn;

    /* Update history */
    uint32_t idx = bridge->state.history_idx;
    bridge->state.cytokine_history[0][idx] = il1;
    bridge->state.cytokine_history[1][idx] = il6;
    bridge->state.cytokine_history[2][idx] = tnf;
    bridge->state.cytokine_history[3][idx] = il10;
    bridge->state.cytokine_history[4][idx] = ifn;
    bridge->state.history_idx = (idx + 1) % 16;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Get fever signal
 * WHY:  IL-1 triggers fever via hypothalamus
 * HOW:  Return cached fever signal
 */
float hypo_immune_fep_get_fever_signal(const hypo_immune_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1.0f;
    }
    return bridge->fep_effects.fever_signal;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

int hypo_immune_fep_connect_bio_async(
    hypo_immune_fep_bridge_t* bridge,
    bio_router_t* router
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    (void)router;

    if (bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_HYPO_IMMUNE_FEP,
        .module_name = "hypo_immune_fep_bridge",
        .inbox_capacity = 64,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Hypo Immune FEP bridge connected to bio-async");
    }

    return 0;
}

int hypo_immune_fep_disconnect_bio_async(hypo_immune_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Hypo Immune FEP bridge disconnected from bio-async");
    return 0;
}

int hypo_immune_fep_process_messages(
    hypo_immune_fep_bridge_t* bridge,
    uint32_t max_messages
) {
    if (!bridge || !bridge->base.bio_async_enabled || !bridge->base.bio_ctx) {
        return 0;
    }

    return (int)bio_router_process_inbox(bridge->base.bio_ctx, max_messages);
}

/* ============================================================================
 * Utility Implementation
 * ============================================================================ */

const char* hypo_immune_fep_state_name(hypo_immune_fep_state_t state) {
    switch (state) {
        case HYPO_IMMUNE_FEP_STATE_NORMAL:
            return "Normal";
        case HYPO_IMMUNE_FEP_STATE_VIGILANT:
            return "Vigilant";
        case HYPO_IMMUNE_FEP_STATE_ACTIVATED:
            return "Activated";
        case HYPO_IMMUNE_FEP_STATE_INFLAMED:
            return "Inflamed";
        case HYPO_IMMUNE_FEP_STATE_STORM:
            return "Storm";
        default:
            return "Unknown";
    }
}

const char* hypo_immune_fep_response_name(hypo_immune_fep_response_t response) {
    switch (response) {
        case HYPO_IMMUNE_FEP_RESPONSE_NONE:
            return "None";
        case HYPO_IMMUNE_FEP_RESPONSE_MONITOR:
            return "Monitor";
        case HYPO_IMMUNE_FEP_RESPONSE_MODULATE:
            return "Modulate";
        case HYPO_IMMUNE_FEP_RESPONSE_SUPPRESS:
            return "Suppress";
        case HYPO_IMMUNE_FEP_RESPONSE_EMERGENCY:
            return "Emergency";
        default:
            return "Unknown";
    }
}

const char* hypo_immune_fep_event_name(hypo_immune_event_type_t event) {
    switch (event) {
        case HYPO_IMMUNE_EVENT_NONE:
            return "None";
        case HYPO_IMMUNE_EVENT_ACTIVATION:
            return "Activation";
        case HYPO_IMMUNE_EVENT_INFLAMMATION:
            return "Inflammation";
        case HYPO_IMMUNE_EVENT_RESOLUTION:
            return "Resolution";
        case HYPO_IMMUNE_EVENT_STORM:
            return "Storm";
        default:
            return "Unknown";
    }
}

void hypo_immune_fep_print_summary(const hypo_immune_fep_bridge_t* bridge) {
    if (!bridge) {
        printf("Hypo Immune FEP Bridge: NULL\n");
        return;
    }

    printf("=== Hypothalamus Immune FEP Bridge Summary ===\n");
    printf("State:\n");
    printf("  Active: %s\n", bridge->state.active ? "yes" : "no");
    printf("  Updates: %lu\n", (unsigned long)bridge->state.update_count);
    printf("  Precision: %.3f\n", bridge->state.current_precision);
    printf("\n");
    printf("Cytokines:\n");
    printf("  IL-1: %.3f\n", bridge->immune_effects.cytokine_il1);
    printf("  IL-6: %.3f\n", bridge->immune_effects.cytokine_il6);
    printf("  TNF-a: %.3f\n", bridge->immune_effects.cytokine_tnf);
    printf("  IL-10: %.3f\n", bridge->immune_effects.cytokine_il10);
    printf("  IFN-g: %.3f\n", bridge->immune_effects.cytokine_ifn);
    printf("\n");
    printf("FEP Effects:\n");
    printf("  Free Energy: %.3f\n", bridge->fep_effects.free_energy);
    printf("  Immune State: %s\n",
           hypo_immune_fep_state_name(bridge->fep_effects.immune_state));
    printf("  Inflammation: %.3f\n", bridge->fep_effects.inflammation_level);
    printf("  Fever Signal: %.3f\n", bridge->fep_effects.fever_signal);
    printf("  Immune Health: %.3f\n", bridge->fep_effects.immune_health);
    printf("  Recommended Response: %s\n",
           hypo_immune_fep_response_name(bridge->fep_effects.recommended_response));
    printf("==============================================\n");
}

/* ============================================================================
 * Internal Helper Implementation
 * ============================================================================ */

/**
 * WHAT: Compute free energy from cytokine levels
 * WHY:  Map cytokine domain to FEP domain
 * HOW:  Weighted combination of cytokine deviations from baseline
 */
static float compute_fe_from_cytokines(const hypo_immune_fep_bridge_t* bridge) {
    float fe = 0.0f;

    /* IL-1 deviation (pro-inflammatory) */
    float il1_dev = bridge->immune_effects.cytokine_il1 - bridge->config.il1_baseline;
    if (il1_dev > 0.0f) {
        fe += il1_dev * bridge->config.il1_weight;
    }

    /* IL-6 deviation (pro-inflammatory) */
    float il6_dev = bridge->immune_effects.cytokine_il6 - bridge->config.il6_baseline;
    if (il6_dev > 0.0f) {
        fe += il6_dev * bridge->config.il6_weight;
    }

    /* TNF-alpha deviation (pro-inflammatory) */
    float tnf_dev = bridge->immune_effects.cytokine_tnf - bridge->config.tnf_baseline;
    if (tnf_dev > 0.0f) {
        fe += tnf_dev * bridge->config.tnf_weight;
    }

    /* IL-10 (anti-inflammatory - reduces FE) */
    float il10_dev = bridge->immune_effects.cytokine_il10 - bridge->config.il10_baseline;
    if (il10_dev > 0.0f) {
        fe += il10_dev * bridge->config.il10_weight;  /* Note: weight is negative */
    }

    /* IFN-gamma deviation */
    float ifn_dev = bridge->immune_effects.cytokine_ifn - bridge->config.ifn_baseline;
    if (ifn_dev > 0.0f) {
        fe += ifn_dev * bridge->config.ifn_weight;
    }

    if (fe < 0.0f) fe = 0.0f;

    return fe;
}

/**
 * WHAT: Classify immune state from free energy
 * WHY:  Map continuous FE to discrete categories
 * HOW:  Threshold-based classification
 */
static hypo_immune_fep_state_t classify_immune_state(
    float free_energy,
    const hypo_immune_fep_config_t* config
) {
    if (free_energy >= HYPO_IMMUNE_FEP_STORM_THRESHOLD) {
        return HYPO_IMMUNE_FEP_STATE_STORM;
    } else if (free_energy >= config->free_energy_threshold) {
        return HYPO_IMMUNE_FEP_STATE_INFLAMED;
    } else if (free_energy >= HYPO_IMMUNE_FEP_ACTIVATION_THRESHOLD) {
        return HYPO_IMMUNE_FEP_STATE_ACTIVATED;
    } else if (free_energy >= HYPO_IMMUNE_FEP_NORMAL_THRESHOLD) {
        return HYPO_IMMUNE_FEP_STATE_VIGILANT;
    } else {
        return HYPO_IMMUNE_FEP_STATE_NORMAL;
    }
}

/**
 * WHAT: Determine appropriate response
 * WHY:  Active inference selects actions to minimize expected FE
 * HOW:  Map immune state and urgency to response type
 */
static hypo_immune_fep_response_t determine_response(
    hypo_immune_fep_state_t state,
    float urgency
) {
    switch (state) {
        case HYPO_IMMUNE_FEP_STATE_STORM:
            return HYPO_IMMUNE_FEP_RESPONSE_EMERGENCY;

        case HYPO_IMMUNE_FEP_STATE_INFLAMED:
            if (urgency > 0.8f) {
                return HYPO_IMMUNE_FEP_RESPONSE_EMERGENCY;
            }
            return HYPO_IMMUNE_FEP_RESPONSE_SUPPRESS;

        case HYPO_IMMUNE_FEP_STATE_ACTIVATED:
            return HYPO_IMMUNE_FEP_RESPONSE_MODULATE;

        case HYPO_IMMUNE_FEP_STATE_VIGILANT:
            return HYPO_IMMUNE_FEP_RESPONSE_MONITOR;

        case HYPO_IMMUNE_FEP_STATE_NORMAL:
        default:
            return HYPO_IMMUNE_FEP_RESPONSE_NONE;
    }
}

/**
 * WHAT: Identify immune event type
 * WHY:  Classify the nature of the immune event
 * HOW:  Analyze cytokine patterns and FE level
 */
static hypo_immune_event_type_t identify_event(
    const hypo_immune_fep_bridge_t* bridge,
    float free_energy
) {
    /* Check for storm */
    if (free_energy >= HYPO_IMMUNE_FEP_STORM_THRESHOLD) {
        return HYPO_IMMUNE_EVENT_STORM;
    }

    /* Check for resolution (high IL-10, declining inflammation) */
    if (bridge->immune_effects.cytokine_il10 > 0.5f &&
        bridge->fep_effects.inflammation_level < 0.3f) {
        return HYPO_IMMUNE_EVENT_RESOLUTION;
    }

    /* Check for inflammation */
    if (free_energy >= HYPO_IMMUNE_FEP_INFLAMMATION_THRESHOLD) {
        return HYPO_IMMUNE_EVENT_INFLAMMATION;
    }

    /* Check for activation */
    if (free_energy >= HYPO_IMMUNE_FEP_ACTIVATION_THRESHOLD) {
        return HYPO_IMMUNE_EVENT_ACTIVATION;
    }

    return HYPO_IMMUNE_EVENT_NONE;
}

/**
 * WHAT: Update running averages for metrics
 * WHY:  Smooth metrics over time for stability
 * HOW:  Exponential moving average
 */
static void update_running_averages(
    hypo_immune_fep_bridge_t* bridge,
    float free_energy,
    float surprise,
    float pred_error
) {
    const float alpha = 0.1f;

    float new_surprise = (1.0f - alpha) * bridge->state.avg_surprise + alpha * surprise;
    if (isfinite(new_surprise)) bridge->state.avg_surprise = new_surprise;

    float new_pred_error = (1.0f - alpha) * bridge->state.avg_prediction_error + alpha * pred_error;
    if (isfinite(new_pred_error)) bridge->state.avg_prediction_error = new_pred_error;

    float new_avg_fe = (1.0f - alpha) * bridge->stats.avg_free_energy + alpha * free_energy;
    if (isfinite(new_avg_fe)) bridge->stats.avg_free_energy = new_avg_fe;
    bridge->stats.avg_surprise = bridge->state.avg_surprise;
    bridge->stats.avg_prediction_error = bridge->state.avg_prediction_error;
}
