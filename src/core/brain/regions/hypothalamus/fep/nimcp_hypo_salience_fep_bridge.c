/**
 * @file nimcp_hypo_salience_fep_bridge.c
 * @brief Implementation of Hypothalamus Salience FEP Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for hypothalamic salience-based prioritization
 * WHY:  Drive urgency modulates attention through salience weights
 * HOW:  Map drive urgency to salience weights, fatigue to precision reduction
 */

#include "core/brain/regions/hypothalamus/fep/nimcp_hypo_salience_fep_bridge.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(hypo_salience_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Internal Helper Declarations
 * ============================================================================ */

static void compute_salience_weights(hypo_salience_fep_bridge_t* bridge);

static float compute_fe_from_urgencies(const hypo_salience_fep_bridge_t* bridge);

static hypo_salience_fep_level_t classify_urgency_level(
    float free_energy,
    const hypo_salience_fep_config_t* config);

static hypo_salience_fep_response_t determine_response(
    hypo_salience_fep_level_t level,
    float conflict_intensity);

static hypo_salience_conflict_t compute_conflict(
    const hypo_salience_fep_bridge_t* bridge,
    float* intensity_out);

static void update_running_averages(hypo_salience_fep_bridge_t* bridge,
                                    float free_energy,
                                    float surprise,
                                    float pred_error);

static void update_salience_tracking(hypo_salience_fep_bridge_t* bridge);

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

/**
 * WHAT: Get default configuration for hypothalamus salience FEP bridge
 * WHY:  Provide sensible starting point for attention allocation
 * HOW:  Set biologically-plausible defaults for salience parameters
 */
int hypo_salience_fep_default_config(hypo_salience_fep_config_t* config) {
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

    /* Salience computation */
    config->softmax_temperature = HYPO_SALIENCE_FEP_DEFAULT_TEMPERATURE;
    config->urgency_to_salience_scale = 1.0f;
    config->salience_decay_rate = 0.1f;

    /* Fatigue effects */
    config->fatigue_precision_scale = HYPO_SALIENCE_FEP_FATIGUE_PRECISION_SCALE;
    config->fatigue_salience_scale = HYPO_SALIENCE_FEP_FATIGUE_SALIENCE_SCALE;
    config->enable_fatigue_adaptation = true;

    /* Conflict resolution */
    config->conflict_threshold = 0.3f;  /* Conflict when top-2 within 30% */
    config->conflict_resolution_tau = 100.0f;  /* 100ms time constant */
    config->enable_winner_take_all = false;

    /* Detection parameters */
    config->free_energy_threshold = HYPO_SALIENCE_FEP_HIGH_THRESHOLD;
    config->surprise_threshold = 8.0f;
    config->precision_learning_rate = 0.05f;

    /* Learning */
    config->enable_online_learning = true;
    config->learning_rate = 0.01f;

    return 0;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

/**
 * WHAT: Create hypothalamus salience FEP bridge
 * WHY:  Initialize FEP integration for attention allocation
 * HOW:  Allocate structure, initialize base, apply configuration
 */
hypo_salience_fep_bridge_t* hypo_salience_fep_create(
    const hypo_salience_fep_config_t* config,
    hypo_drive_system_handle_t* drive_system,
    fep_system_t* fep_system
) {
    /* Validate required parameters - fep_system required, drive_system optional */
    if (!fep_system) {
        NIMCP_LOGGING_ERROR("Hypo Salience FEP bridge: fep_system is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_salience_fep_create: fep_system is NULL");
        return NULL;
    }

    /* Allocate bridge structure */
    hypo_salience_fep_bridge_t* bridge = (hypo_salience_fep_bridge_t*)nimcp_malloc(
        sizeof(hypo_salience_fep_bridge_t)
    );
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Hypo Salience FEP bridge: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_salience_fep_create: bridge is NULL");
        return NULL;
    }

    /* Zero initialize */
    memset(bridge, 0, sizeof(hypo_salience_fep_bridge_t));

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        hypo_salience_fep_default_config(&bridge->config);
    }

    /* Store system references */
    bridge->drive_system = drive_system;
    bridge->fep_system = fep_system;

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "hypo_salience_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Hypo Salience FEP bridge: mutex creation failed");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_salience_fep_create: bridge->base is NULL");
        return NULL;
    }

    /* Initialize state */
    bridge->state.active = true;
    bridge->state.current_precision = HYPO_SALIENCE_FEP_DEFAULT_PRECISION;
    bridge->state.avg_surprise = 0.0f;
    bridge->state.avg_prediction_error = 0.0f;
    bridge->state.last_level = HYPO_SALIENCE_FEP_LEVEL_LOW;

    /* Initialize salience weights to uniform */
    float initial_weight = 1.0f / (float)HYPO_DRIVE_COUNT;
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        bridge->fep_effects.salience_weights[i] = initial_weight;
        bridge->state.salience_tracking.predicted_salience[i] = initial_weight;
    }
    bridge->fep_effects.total_salience = 1.0f;

    /* Initialize FEP effects */
    bridge->fep_effects.urgency_level = HYPO_SALIENCE_FEP_LEVEL_LOW;
    bridge->fep_effects.precision = HYPO_SALIENCE_FEP_DEFAULT_PRECISION;
    bridge->fep_effects.attention_capacity = 1.0f;
    bridge->fep_effects.attention_focus = 0.5f;
    bridge->fep_effects.conflict_level = HYPO_SALIENCE_CONFLICT_NONE;

    /* Bio-async not yet connected */
    bridge->base.bio_async_enabled = false;
    bridge->base.module_id = BIO_MODULE_HYPO_SALIENCE_FEP;
    bridge->base.module_name = "hypo_salience_fep_bridge";

    NIMCP_LOGGING_INFO("Hypo Salience FEP bridge created");
    return bridge;
}

/**
 * WHAT: Destroy hypothalamus salience FEP bridge
 * WHY:  Clean up all resources
 * HOW:  Disconnect bio-async, destroy mutex, free memory
 */
void hypo_salience_fep_destroy(hypo_salience_fep_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    if (bridge->base.bio_async_enabled) {
        hypo_salience_fep_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
        bridge->base.mutex = NULL;
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Hypo Salience FEP bridge destroyed");
}

/**
 * WHAT: Reset bridge to initial state
 * WHY:  Allow reuse without full recreation
 * HOW:  Clear state and statistics, preserve connections
 */
int hypo_salience_fep_reset(hypo_salience_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Reset state */
    bridge->state.update_count = 0;
    bridge->state.detection_count = 0;
    bridge->state.current_precision = HYPO_SALIENCE_FEP_DEFAULT_PRECISION;
    bridge->state.avg_surprise = 0.0f;
    bridge->state.avg_prediction_error = 0.0f;
    bridge->state.last_level = HYPO_SALIENCE_FEP_LEVEL_LOW;
    bridge->state.last_detection_time_ms = 0;

    /* Reset salience tracking */
    memset(&bridge->state.salience_tracking, 0, sizeof(hypo_salience_tracking_t));
    float initial_weight = 1.0f / (float)HYPO_DRIVE_COUNT;
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        bridge->state.salience_tracking.predicted_salience[i] = initial_weight;
    }

    /* Reset effects */
    memset(&bridge->fep_effects, 0, sizeof(hypo_salience_fep_effects_t));
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        bridge->fep_effects.salience_weights[i] = initial_weight;
    }
    bridge->fep_effects.total_salience = 1.0f;
    bridge->fep_effects.attention_capacity = 1.0f;
    bridge->fep_effects.attention_focus = 0.5f;
    bridge->fep_effects.precision = HYPO_SALIENCE_FEP_DEFAULT_PRECISION;

    memset(&bridge->sal_effects, 0, sizeof(salience_to_fep_effects_t));

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(hypo_salience_fep_stats_t));
    bridge->stats.current_precision = HYPO_SALIENCE_FEP_DEFAULT_PRECISION;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Hypo Salience FEP bridge reset");
    return 0;
}

/**
 * WHAT: Update bridge state
 * WHY:  Main update loop for bridge synchronization
 * HOW:  Compute effects, update salience, apply precision modulation
 */
int hypo_salience_fep_update(hypo_salience_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Sync drive urgencies from stored drive system if available */
    if (bridge->drive_system) {
        hypo_drive_system_t ds_state;
        if (hypo_drive_get_system_state(bridge->drive_system, &ds_state)) {
            for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
                bridge->sal_effects.drive_urgencies[i] = ds_state.drives[i].urgency;
            }
            bridge->sal_effects.priority_drive = ds_state.highest_priority;
            bridge->sal_effects.priority_urgency =
                ds_state.drives[ds_state.highest_priority].urgency;
        }
    }

    /* Compute salience weights */
    compute_salience_weights(bridge);

    /* Compute free energy from urgencies */
    float current_fe = compute_fe_from_urgencies(bridge);

    /* Get FEP system metrics */
    float surprise = fep_compute_surprise(bridge->fep_system);
    float pred_error = fep_get_prediction_error(bridge->fep_system, 0);

    /* Update running averages */
    update_running_averages(bridge, current_fe, surprise, pred_error);

    /* Store in FEP effects */
    bridge->fep_effects.free_energy = current_fe;
    bridge->fep_effects.prediction_error = pred_error;
    bridge->fep_effects.precision = bridge->state.current_precision;

    /* Classify urgency level */
    bridge->fep_effects.urgency_level = classify_urgency_level(current_fe, &bridge->config);

    /* Detect conflict */
    bridge->fep_effects.conflict_level = compute_conflict(bridge,
                                                           &bridge->fep_effects.conflict_intensity);

    /* Determine response */
    bridge->fep_effects.recommended_response = determine_response(
        bridge->fep_effects.urgency_level,
        bridge->fep_effects.conflict_intensity
    );

    /* Compute attention metrics */
    float fatigue = bridge->sal_effects.current_fatigue;
    bridge->fep_effects.attention_capacity = 1.0f -
        (fatigue * bridge->config.fatigue_salience_scale);
    if (bridge->fep_effects.attention_capacity < 0.1f) {
        bridge->fep_effects.attention_capacity = 0.1f;
    }

    /* Attention focus based on dominant salience */
    bridge->fep_effects.attention_focus = bridge->fep_effects.dominant_salience;

    /* Response urgency */
    float urgency = current_fe / HYPO_SALIENCE_FEP_CRITICAL_THRESHOLD;
    if (urgency > 1.0f) urgency = 1.0f;
    bridge->fep_effects.response_urgency = urgency;

    /* Update salience tracking */
    update_salience_tracking(bridge);

    /* Update statistics */
    bridge->state.update_count++;
    bridge->stats.total_updates++;
    bridge->stats.current_precision = bridge->state.current_precision;

    if (current_fe > bridge->stats.max_free_energy) {
        bridge->stats.max_free_energy = current_fe;
    }
    if (bridge->fep_effects.dominant_salience > bridge->stats.max_salience) {
        bridge->stats.max_salience = bridge->fep_effects.dominant_salience;
    }
    if (bridge->fep_effects.conflict_intensity > bridge->stats.max_conflict_intensity) {
        bridge->stats.max_conflict_intensity = bridge->fep_effects.conflict_intensity;
    }

    /* Track dominance */
    if (bridge->fep_effects.dominant_drive < HYPO_DRIVE_COUNT) {
        bridge->stats.dominance_counts[bridge->fep_effects.dominant_drive]++;
    }

    /* Track conflicts */
    if (bridge->fep_effects.conflict_level != HYPO_SALIENCE_CONFLICT_NONE) {
        bridge->stats.conflicts_detected++;
        bridge->sal_effects.conflicts_detected++;
    }

    /* Average attention capacity */
    float alpha = 0.1f;
    bridge->stats.avg_attention_capacity =
        (1.0f - alpha) * bridge->stats.avg_attention_capacity +
        alpha * bridge->fep_effects.attention_capacity;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Core Operations Implementation
 * ============================================================================ */

/**
 * WHAT: Compute free energy from drive state
 * WHY:  Core FEP computation for salience allocation
 * HOW:  Map drive urgencies to free energy
 */
int hypo_salience_fep_compute_fe(
    hypo_salience_fep_bridge_t* bridge,
    const hypo_drive_system_t* drives
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_salience_fep_compute_fe: bridge is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Store drive urgencies if drives provided */
    if (drives) {
        for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
            bridge->sal_effects.drive_urgencies[i] = drives->drives[i].urgency;
        }
        bridge->sal_effects.priority_drive = drives->highest_priority;
        bridge->sal_effects.priority_urgency = drives->drives[drives->highest_priority].urgency;
    }

    /* Compute salience weights based on urgencies */
    compute_salience_weights(bridge);

    /* Compute free energy */
    float total_fe = compute_fe_from_urgencies(bridge);
    bridge->fep_effects.free_energy = total_fe;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Modulate precision based on fatigue
 * WHY:  Fatigue reduces attention capacity and precision
 * HOW:  Scale precision inversely with fatigue
 */
int hypo_salience_fep_modulate_precision(
    hypo_salience_fep_bridge_t* bridge,
    float fatigue
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (fatigue < 0.0f) fatigue = 0.0f;
    if (fatigue > 1.0f) fatigue = 1.0f;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Store fatigue */
    bridge->sal_effects.current_fatigue = fatigue;

    /* Higher fatigue = lower precision */
    float precision_mod = 1.0f - (fatigue * bridge->config.fatigue_precision_scale);
    if (precision_mod < 0.1f) precision_mod = 0.1f;

    float new_precision = HYPO_SALIENCE_FEP_DEFAULT_PRECISION * precision_mod;

    /* Smooth adaptation */
    float alpha = bridge->config.precision_learning_rate;
    bridge->state.current_precision =
        (1.0f - alpha) * bridge->state.current_precision + alpha * new_precision;

    /* Clamp */
    if (bridge->state.current_precision < HYPO_SALIENCE_FEP_MIN_PRECISION) {
        bridge->state.current_precision = HYPO_SALIENCE_FEP_MIN_PRECISION;
    }
    if (bridge->state.current_precision > HYPO_SALIENCE_FEP_MAX_PRECISION) {
        bridge->state.current_precision = HYPO_SALIENCE_FEP_MAX_PRECISION;
    }

    bridge->fep_effects.precision = bridge->state.current_precision;
    bridge->stats.precision_adaptations++;

    /* Also reduce salience if fatigued */
    if (fatigue > 0.5f && bridge->config.enable_fatigue_adaptation) {
        bridge->sal_effects.attention_depleted = true;
    } else {
        bridge->sal_effects.attention_depleted = false;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Get current FEP effects
 * WHY:  Allow inspection of current effects
 * HOW:  Copy effects structure
 */
int hypo_salience_fep_get_effects(
    const hypo_salience_fep_bridge_t* bridge,
    hypo_salience_fep_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_salience_fep_get_effects: bridge or effects is NULL");
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
int hypo_salience_fep_get_stats(
    const hypo_salience_fep_bridge_t* bridge,
    hypo_salience_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_salience_fep_get_stats: bridge or stats is NULL");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

/* ============================================================================
 * Salience API Implementation
 * ============================================================================ */

/**
 * WHAT: Get salience weight for a drive
 * WHY:  Enable drive-specific attention queries
 * HOW:  Return cached weight
 */
float hypo_salience_fep_get_weight(
    const hypo_salience_fep_bridge_t* bridge,
    hypo_drive_type_t drive
) {
    if (!bridge || drive >= HYPO_DRIVE_COUNT) {
        return -1.0f;
    }

    return bridge->fep_effects.salience_weights[drive];
}

/**
 * WHAT: Get all salience weights
 * WHY:  Enable batch attention allocation
 * HOW:  Copy weight array
 */
int hypo_salience_fep_get_weights(
    const hypo_salience_fep_bridge_t* bridge,
    float* weights
) {
    if (!bridge || !weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_salience_fep_get_weights: bridge or weights is NULL");
        return -1;
    }

    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        weights[i] = bridge->fep_effects.salience_weights[i];
    }

    return 0;
}

/**
 * WHAT: Detect salience conflict
 * WHY:  Conflict requires resolution strategy
 * HOW:  Compute conflict from weight distribution
 */
int hypo_salience_fep_detect_conflict(
    hypo_salience_fep_bridge_t* bridge,
    hypo_salience_conflict_t* conflict_out,
    float* intensity_out
) {
    if (!bridge || !conflict_out || !intensity_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_salience_fep_detect_conflict: NULL parameter");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    *conflict_out = compute_conflict(bridge, intensity_out);

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

int hypo_salience_fep_connect_bio_async(
    hypo_salience_fep_bridge_t* bridge,
    bio_router_t* router
) {
    if (!bridge) {
        return 0;  /* Graceful no-op for NULL bridge */
    }

    (void)router;

    if (bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_HYPO_SALIENCE_FEP,
        .module_name = "hypo_salience_fep_bridge",
        .inbox_capacity = 64,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Hypo Salience FEP bridge connected to bio-async");
    }

    return 0;
}

int hypo_salience_fep_disconnect_bio_async(hypo_salience_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Hypo Salience FEP bridge disconnected from bio-async");
    return 0;
}

int hypo_salience_fep_process_messages(
    hypo_salience_fep_bridge_t* bridge,
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

const char* hypo_salience_fep_level_name(hypo_salience_fep_level_t level) {
    switch (level) {
        case HYPO_SALIENCE_FEP_LEVEL_LOW:
            return "Low";
        case HYPO_SALIENCE_FEP_LEVEL_MODERATE:
            return "Moderate";
        case HYPO_SALIENCE_FEP_LEVEL_ELEVATED:
            return "Elevated";
        case HYPO_SALIENCE_FEP_LEVEL_HIGH:
            return "High";
        case HYPO_SALIENCE_FEP_LEVEL_CRITICAL:
            return "Critical";
        default:
            return "Unknown";
    }
}

const char* hypo_salience_fep_response_name(hypo_salience_fep_response_t response) {
    switch (response) {
        case HYPO_SALIENCE_FEP_RESPONSE_MAINTAIN:
            return "Maintain";
        case HYPO_SALIENCE_FEP_RESPONSE_SHIFT:
            return "Shift";
        case HYPO_SALIENCE_FEP_RESPONSE_FOCUS:
            return "Focus";
        case HYPO_SALIENCE_FEP_RESPONSE_NARROW:
            return "Narrow";
        case HYPO_SALIENCE_FEP_RESPONSE_EMERGENCY:
            return "Emergency";
        default:
            return "Unknown";
    }
}

const char* hypo_salience_fep_conflict_name(hypo_salience_conflict_t conflict) {
    switch (conflict) {
        case HYPO_SALIENCE_CONFLICT_NONE:
            return "None";
        case HYPO_SALIENCE_CONFLICT_MILD:
            return "Mild";
        case HYPO_SALIENCE_CONFLICT_MODERATE:
            return "Moderate";
        case HYPO_SALIENCE_CONFLICT_SEVERE:
            return "Severe";
        default:
            return "Unknown";
    }
}

void hypo_salience_fep_print_summary(const hypo_salience_fep_bridge_t* bridge) {
    if (!bridge) {
        printf("Hypo Salience FEP Bridge: NULL\n");
        return;
    }

    printf("=== Hypothalamus Salience FEP Bridge Summary ===\n");
    printf("State:\n");
    printf("  Active: %s\n", bridge->state.active ? "yes" : "no");
    printf("  Updates: %lu\n", (unsigned long)bridge->state.update_count);
    printf("  Precision: %.3f\n", bridge->state.current_precision);
    printf("\n");
    printf("Salience Weights:\n");
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        printf("  Drive %d: %.3f\n", i, bridge->fep_effects.salience_weights[i]);
    }
    printf("  Total: %.3f\n", bridge->fep_effects.total_salience);
    printf("  Dominant: %d (%.3f)\n",
           bridge->fep_effects.dominant_drive,
           bridge->fep_effects.dominant_salience);
    printf("\n");
    printf("FEP Effects:\n");
    printf("  Free Energy: %.3f\n", bridge->fep_effects.free_energy);
    printf("  Urgency Level: %s\n",
           hypo_salience_fep_level_name(bridge->fep_effects.urgency_level));
    printf("  Conflict: %s (%.3f)\n",
           hypo_salience_fep_conflict_name(bridge->fep_effects.conflict_level),
           bridge->fep_effects.conflict_intensity);
    printf("  Attention Capacity: %.3f\n", bridge->fep_effects.attention_capacity);
    printf("  Attention Focus: %.3f\n", bridge->fep_effects.attention_focus);
    printf("  Recommended Response: %s\n",
           hypo_salience_fep_response_name(bridge->fep_effects.recommended_response));
    printf("================================================\n");
}

/* ============================================================================
 * Internal Helper Implementation
 * ============================================================================ */

/**
 * WHAT: Compute salience weights using softmax
 * WHY:  Convert urgencies to normalized attention weights
 * HOW:  softmax(-urgency / temperature) for each drive
 */
static void compute_salience_weights(hypo_salience_fep_bridge_t* bridge) {
    float max_urgency = 0.0f;
    float temperature = bridge->config.softmax_temperature;

    /* Find max urgency for numerical stability */
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        float urgency = bridge->sal_effects.drive_urgencies[i];
        if (urgency > max_urgency) {
            max_urgency = urgency;
        }
    }

    /* Compute exp(urgency/temp) for each drive */
    float sum_exp = 0.0f;
    float exp_values[HYPO_DRIVE_COUNT];

    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        float urgency = bridge->sal_effects.drive_urgencies[i];
        /* Subtract max for numerical stability */
        exp_values[i] = expf((urgency - max_urgency) / temperature);
        sum_exp += exp_values[i];
    }

    /* Normalize to get softmax */
    float dominant_weight = 0.0f;
    hypo_drive_type_t dominant_drive = HYPO_DRIVE_HUNGER;
    float total = 0.0f;

    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        float weight = exp_values[i] / (sum_exp + 0.0001f);

        /* Apply fatigue reduction if enabled */
        if (bridge->config.enable_fatigue_adaptation) {
            float fatigue_mod = 1.0f -
                (bridge->sal_effects.current_fatigue * bridge->config.fatigue_salience_scale);
            if (fatigue_mod < 0.1f) fatigue_mod = 0.1f;
            weight *= fatigue_mod;
        }

        /* Clamp */
        if (weight < HYPO_SALIENCE_FEP_MIN_WEIGHT) {
            weight = HYPO_SALIENCE_FEP_MIN_WEIGHT;
        }
        if (weight > HYPO_SALIENCE_FEP_MAX_WEIGHT) {
            weight = HYPO_SALIENCE_FEP_MAX_WEIGHT;
        }

        bridge->fep_effects.salience_weights[i] = weight;
        total += weight;

        if (weight > dominant_weight) {
            dominant_weight = weight;
            dominant_drive = (hypo_drive_type_t)i;
        }
    }

    bridge->fep_effects.total_salience = total;
    bridge->fep_effects.dominant_drive = dominant_drive;
    bridge->fep_effects.dominant_salience = dominant_weight;
}

/**
 * WHAT: Compute free energy from urgencies
 * WHY:  Map urgency domain to FEP domain
 * HOW:  Weighted sum of urgency deviations from baseline
 */
static float compute_fe_from_urgencies(const hypo_salience_fep_bridge_t* bridge) {
    float fe = 0.0f;

    /* Base FE from urgency levels */
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        float urgency = bridge->sal_effects.drive_urgencies[i];
        /* Higher urgency = higher free energy (deviation from homeostasis) */
        fe += urgency * urgency * bridge->config.drive_fe_weight;
    }

    /* Add conflict-based FE */
    fe += bridge->fep_effects.conflict_intensity * 5.0f;

    return fe;
}

/**
 * WHAT: Classify urgency level from free energy
 * WHY:  Map continuous FE to discrete categories
 * HOW:  Threshold-based classification
 */
static hypo_salience_fep_level_t classify_urgency_level(
    float free_energy,
    const hypo_salience_fep_config_t* config
) {
    if (free_energy >= HYPO_SALIENCE_FEP_CRITICAL_THRESHOLD) {
        return HYPO_SALIENCE_FEP_LEVEL_CRITICAL;
    } else if (free_energy >= config->free_energy_threshold) {
        return HYPO_SALIENCE_FEP_LEVEL_HIGH;
    } else if (free_energy >= HYPO_SALIENCE_FEP_MEDIUM_THRESHOLD) {
        return HYPO_SALIENCE_FEP_LEVEL_ELEVATED;
    } else if (free_energy >= HYPO_SALIENCE_FEP_LOW_THRESHOLD) {
        return HYPO_SALIENCE_FEP_LEVEL_MODERATE;
    } else {
        return HYPO_SALIENCE_FEP_LEVEL_LOW;
    }
}

/**
 * WHAT: Determine appropriate response
 * WHY:  Active inference selects attention allocation
 * HOW:  Map urgency level and conflict to response type
 */
static hypo_salience_fep_response_t determine_response(
    hypo_salience_fep_level_t level,
    float conflict_intensity
) {
    /* Emergency response if critical */
    if (level == HYPO_SALIENCE_FEP_LEVEL_CRITICAL) {
        return HYPO_SALIENCE_FEP_RESPONSE_EMERGENCY;
    }

    /* Narrow focus under severe conflict */
    if (conflict_intensity > 0.7f) {
        return HYPO_SALIENCE_FEP_RESPONSE_NARROW;
    }

    switch (level) {
        case HYPO_SALIENCE_FEP_LEVEL_HIGH:
            return HYPO_SALIENCE_FEP_RESPONSE_FOCUS;

        case HYPO_SALIENCE_FEP_LEVEL_ELEVATED:
            if (conflict_intensity > 0.3f) {
                return HYPO_SALIENCE_FEP_RESPONSE_SHIFT;
            }
            return HYPO_SALIENCE_FEP_RESPONSE_FOCUS;

        case HYPO_SALIENCE_FEP_LEVEL_MODERATE:
            return HYPO_SALIENCE_FEP_RESPONSE_SHIFT;

        case HYPO_SALIENCE_FEP_LEVEL_LOW:
        default:
            return HYPO_SALIENCE_FEP_RESPONSE_MAINTAIN;
    }
}

/**
 * WHAT: Compute conflict level
 * WHY:  Detect competing drives
 * HOW:  Compare top-2 salience weights
 */
static hypo_salience_conflict_t compute_conflict(
    const hypo_salience_fep_bridge_t* bridge,
    float* intensity_out
) {
    /* Find top-2 weights */
    float top1 = 0.0f, top2 = 0.0f;

    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        float weight = bridge->fep_effects.salience_weights[i];
        if (weight > top1) {
            top2 = top1;
            top1 = weight;
        } else if (weight > top2) {
            top2 = weight;
        }
    }

    /* Conflict intensity based on how close top-2 are */
    float ratio = (top1 > 0.01f) ? (top2 / top1) : 0.0f;

    *intensity_out = ratio;

    /* Classify conflict */
    if (ratio < bridge->config.conflict_threshold) {
        return HYPO_SALIENCE_CONFLICT_NONE;
    } else if (ratio < 0.5f) {
        return HYPO_SALIENCE_CONFLICT_MILD;
    } else if (ratio < 0.7f) {
        return HYPO_SALIENCE_CONFLICT_MODERATE;
    } else {
        return HYPO_SALIENCE_CONFLICT_SEVERE;
    }
}

/**
 * WHAT: Update running averages for metrics
 * WHY:  Smooth metrics over time for stability
 * HOW:  Exponential moving average
 */
static void update_running_averages(
    hypo_salience_fep_bridge_t* bridge,
    float free_energy,
    float surprise,
    float pred_error
) {
    const float alpha = 0.1f;

    bridge->state.avg_surprise =
        (1.0f - alpha) * bridge->state.avg_surprise + alpha * surprise;

    bridge->state.avg_prediction_error =
        (1.0f - alpha) * bridge->state.avg_prediction_error + alpha * pred_error;

    bridge->stats.avg_free_energy =
        (1.0f - alpha) * bridge->stats.avg_free_energy + alpha * free_energy;
    bridge->stats.avg_surprise = bridge->state.avg_surprise;
    bridge->stats.avg_prediction_error = bridge->state.avg_prediction_error;
}

/**
 * WHAT: Update salience tracking
 * WHY:  Track salience evolution for predictions
 * HOW:  Store history, update predictions
 */
static void update_salience_tracking(hypo_salience_fep_bridge_t* bridge) {
    hypo_salience_tracking_t* tracking = &bridge->state.salience_tracking;

    /* Store in history */
    uint32_t idx = tracking->history_idx;
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        tracking->salience_history[i][idx] = bridge->fep_effects.salience_weights[i];

        /* Update prediction using EMA */
        float alpha = 0.1f;
        tracking->predicted_salience[i] =
            (1.0f - alpha) * tracking->predicted_salience[i] +
            alpha * bridge->fep_effects.salience_weights[i];

        /* Compute velocity */
        if (idx > 0) {
            float prev = tracking->salience_history[i][(idx - 1) % 16];
            tracking->salience_velocity[i] = bridge->fep_effects.salience_weights[i] - prev;
        }
    }

    tracking->history_idx = (idx + 1) % 16;

    /* Track attention switches */
    if (bridge->fep_effects.dominant_drive != tracking->last_dominant) {
        uint64_t now_ms = nimcp_platform_time_monotonic_ms();
        if (tracking->last_switch_time_ms > 0) {
            float interval = (float)(now_ms - tracking->last_switch_time_ms);
            float alpha = 0.1f;
            bridge->stats.avg_switch_interval_ms =
                (1.0f - alpha) * bridge->stats.avg_switch_interval_ms + alpha * interval;
        }
        tracking->last_switch_time_ms = now_ms;
        tracking->last_dominant = bridge->fep_effects.dominant_drive;
        bridge->stats.salience_shifts++;
        bridge->sal_effects.attention_switches++;
    }
}
