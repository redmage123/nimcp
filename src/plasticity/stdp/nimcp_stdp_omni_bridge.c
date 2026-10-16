/**
 * @file nimcp_stdp_omni_bridge.c
 * @brief Implementation of STDP ↔ Omnidirectional Inference Bridge
 * @version 1.0.0
 * @date 2026-01-12
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "plasticity/stdp/nimcp_stdp_omni_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

#ifdef _POSIX_C_SOURCE
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(stdp_omni_bridge)

/* Security integration */
#define LOG_MODULE "STDP_OMNI_BRIDGE"

#endif

//=============================================================================
// Internal Structure
//=============================================================================

struct stdp_omni_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    stdp_omni_bridge_config_t config;
    stdp_omni_bridge_state_t state;
    stdp_omni_bridge_stats_t stats;
    nimcp_platform_mutex_t mutex;
    bool mutex_initialized;
    bool initialized;
};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(stdp_omni_bridge, struct stdp_omni_bridge_struct)

//=============================================================================
// Helper Functions
//=============================================================================

static uint64_t get_timestamp_ms(void) {
#ifdef _POSIX_C_SOURCE
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#else
    return 0;
#endif
}

static float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

//=============================================================================
// Configuration Functions
//=============================================================================

stdp_omni_bridge_config_t stdp_omni_bridge_default_config(void) {
    stdp_omni_bridge_config_t config = {
        .pe_min_threshold = STDP_OMNI_PE_MIN_THRESHOLD,
        .pe_max_threshold = STDP_OMNI_PE_MAX_THRESHOLD,
        .pe_lr_scaling = STDP_OMNI_PE_LR_SCALING,

        .forward_weight = STDP_OMNI_FORWARD_WEIGHT,
        .backward_weight = STDP_OMNI_BACKWARD_WEIGHT,
        .lateral_weight = STDP_OMNI_LATERAL_WEIGHT,

        .wm_update_rate = STDP_OMNI_WM_UPDATE_RATE,
        .enable_wm_updates = true,

        .precision_min = STDP_OMNI_PRECISION_MIN,
        .precision_max = STDP_OMNI_PRECISION_MAX,
        .enable_precision_modulation = true,

        .enable_forward_pe = true,
        .enable_backward_pe = true,
        .enable_lateral_pe = true,

        .enable_bio_async = false
    };
    return config;
}

bool stdp_omni_bridge_validate_config(const stdp_omni_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_omni_bridge_validate_config: config is NULL");
        return false;
    }
    if (config->pe_min_threshold < 0 || config->pe_max_threshold < config->pe_min_threshold) {
        return false;
    }
    if (config->wm_update_rate < 0 || config->wm_update_rate > 1.0f) {
        return false;
    }
    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

stdp_omni_bridge_t stdp_omni_bridge_create(const stdp_omni_bridge_config_t* config) {
    stdp_omni_bridge_t bridge = nimcp_calloc(1, sizeof(struct stdp_omni_bridge_struct));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stdp_omni_bridge_create: failed to allocate bridge");
        return NULL;
    }

    if (config) {
        if (!stdp_omni_bridge_validate_config(config)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_omni_bridge_create: invalid config");
            nimcp_free(bridge);
            return NULL;
        }
        bridge->config = *config;
    } else {
        bridge->config = stdp_omni_bridge_default_config();
    }

    bridge->base.mutex = nimcp_mutex_create(NULL);
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stdp_omni_bridge_create: failed to init mutex");
        nimcp_free(bridge);
        return NULL;
    }
    bridge->mutex_initialized = true;

    bridge->state.current_forward_pe = 0.0f;
    bridge->state.current_backward_pe = 0.0f;
    bridge->state.current_lateral_pe = 0.0f;
    bridge->state.current_precision = 1.0f;
    bridge->state.cumulative_wm_delta = 0.0f;
    bridge->state.bridge_coherence = 1.0f;

    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->initialized = true;

    NIMCP_LOGGING_INFO("Created %s bridge", "stdp_omni");
    return bridge;
}

void stdp_omni_bridge_destroy(stdp_omni_bridge_t bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "stdp_omni");
    if (bridge->mutex_initialized) {
        nimcp_mutex_free(bridge->base.mutex);
    }
    nimcp_free(bridge);
}

bool stdp_omni_bridge_is_connected(stdp_omni_bridge_t bridge) {
    return bridge && bridge->initialized;
}

//=============================================================================
// Forward Direction: STDP → Omnidirectional
//=============================================================================

int stdp_omni_notify_weight_change(stdp_omni_bridge_t bridge,
                                   float weight_change,
                                   stdp_omni_direction_t direction,
                                   stdp_omni_forward_effect_t* effect) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_omni_notify_weight_change: bridge is NULL");
        return -1;
    }
    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "stdp_omni_notify_weight_change: bridge not initialized");
        return -1;
    }
    if (direction >= STDP_OMNI_DIR_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_omni_notify_weight_change: invalid direction");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    float wm_delta = 0.0f;
    if (bridge->config.enable_wm_updates &&
        fabsf(weight_change) >= STDP_OMNI_WM_MIN_DELTA) {
        wm_delta = weight_change * bridge->config.wm_update_rate;
    }

    bridge->state.cumulative_wm_delta += wm_delta;
    bridge->stats.wm_updates++;
    bridge->stats.total_wm_delta += fabsf(wm_delta);
    bridge->stats.forward_calls++;

    if (effect) {
        effect->weight_change = weight_change;
        effect->wm_weight_delta = wm_delta;
        effect->affected_dir = direction;
        effect->timestamp_ms = get_timestamp_ms();
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int stdp_omni_notify_ltp_forward(stdp_omni_bridge_t bridge,
                                 float weight_change,
                                 stdp_omni_forward_effect_t* effect) {
    return stdp_omni_notify_weight_change(bridge, weight_change,
                                          STDP_OMNI_DIR_FORWARD, effect);
}

int stdp_omni_notify_ltd_backward(stdp_omni_bridge_t bridge,
                                  float weight_change,
                                  stdp_omni_forward_effect_t* effect) {
    return stdp_omni_notify_weight_change(bridge, weight_change,
                                          STDP_OMNI_DIR_BACKWARD, effect);
}

int stdp_omni_notify_lateral(stdp_omni_bridge_t bridge,
                             float weight_change,
                             stdp_omni_forward_effect_t* effect) {
    return stdp_omni_notify_weight_change(bridge, weight_change,
                                          STDP_OMNI_DIR_LATERAL, effect);
}

//=============================================================================
// Backward Direction: Omnidirectional → STDP
//=============================================================================

static float compute_pe_modulation(stdp_omni_bridge_t bridge, float pe) {
    float abs_pe = fabsf(pe);
    if (abs_pe < bridge->config.pe_min_threshold) {
        return 1.0f;  /* Below threshold, no modulation */
    }

    float normalized_pe = (abs_pe - bridge->config.pe_min_threshold) /
                          (bridge->config.pe_max_threshold - bridge->config.pe_min_threshold);
    normalized_pe = nimcp_clampf(normalized_pe, 0.0f, 1.0f);

    return 1.0f + normalized_pe * bridge->config.pe_lr_scaling;
}

int stdp_omni_apply_forward_pe(stdp_omni_bridge_t bridge,
                               float prediction_error,
                               float base_lr,
                               float* modulated_lr) {
    BRIDGE_BBB_VALIDATE(bridge, modulated_lr, sizeof(*modulated_lr));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_omni_apply_forward_pe: bridge is NULL");
        return -1;
    }
    if (!modulated_lr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_omni_apply_forward_pe: modulated_lr is NULL");
        return -1;
    }
    if (!bridge->config.enable_forward_pe) {
        *modulated_lr = base_lr;
        return 0;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    float mod = compute_pe_modulation(bridge, prediction_error);
    mod *= bridge->config.forward_weight;
    *modulated_lr = base_lr * mod;

    bridge->state.current_forward_pe = prediction_error;
    bridge->stats.forward_pe_events++;
    bridge->stats.backward_calls++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int stdp_omni_apply_backward_pe(stdp_omni_bridge_t bridge,
                                float prediction_error,
                                float base_lr,
                                float* modulated_lr) {
    BRIDGE_BBB_VALIDATE(bridge, modulated_lr, sizeof(*modulated_lr));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_omni_apply_backward_pe: bridge is NULL");
        return -1;
    }
    if (!modulated_lr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_omni_apply_backward_pe: modulated_lr is NULL");
        return -1;
    }
    if (!bridge->config.enable_backward_pe) {
        *modulated_lr = base_lr;
        return 0;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    float mod = compute_pe_modulation(bridge, prediction_error);
    mod *= bridge->config.backward_weight;
    *modulated_lr = base_lr * mod;

    bridge->state.current_backward_pe = prediction_error;
    bridge->stats.backward_pe_events++;
    bridge->stats.backward_calls++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int stdp_omni_apply_lateral_pe(stdp_omni_bridge_t bridge,
                               float prediction_error,
                               float base_lr,
                               float* modulated_lr) {
    BRIDGE_BBB_VALIDATE(bridge, modulated_lr, sizeof(*modulated_lr));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_omni_apply_lateral_pe: bridge is NULL");
        return -1;
    }
    if (!modulated_lr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_omni_apply_lateral_pe: modulated_lr is NULL");
        return -1;
    }
    if (!bridge->config.enable_lateral_pe) {
        *modulated_lr = base_lr;
        return 0;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    float mod = compute_pe_modulation(bridge, prediction_error);
    mod *= bridge->config.lateral_weight;
    *modulated_lr = base_lr * mod;

    bridge->state.current_lateral_pe = prediction_error;
    bridge->stats.lateral_pe_events++;
    bridge->stats.backward_calls++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int stdp_omni_apply_precision(stdp_omni_bridge_t bridge,
                              float precision,
                              float base_lr,
                              float* modulated_lr) {
    BRIDGE_BBB_VALIDATE(bridge, modulated_lr, sizeof(*modulated_lr));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_omni_apply_precision: bridge is NULL");
        return -1;
    }
    if (!modulated_lr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_omni_apply_precision: modulated_lr is NULL");
        return -1;
    }
    if (!bridge->config.enable_precision_modulation) {
        *modulated_lr = base_lr;
        return 0;
    }

    precision = nimcp_clampf(precision, 0.0f, 1.0f);
    float factor = lerp(bridge->config.precision_min,
                        bridge->config.precision_max,
                        precision);
    *modulated_lr = base_lr * factor;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state.current_precision = precision;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int stdp_omni_compute_modulation(stdp_omni_bridge_t bridge,
                                 float forward_pe, float backward_pe,
                                 float lateral_pe, float precision,
                                 float base_a_plus, float base_a_minus,
                                 stdp_omni_backward_effect_t* effect) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_omni_compute_modulation: bridge is NULL");
        return -1;
    }
    if (!effect) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_omni_compute_modulation: effect is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute individual modulations */
    float fwd_mod = bridge->config.enable_forward_pe ?
                    compute_pe_modulation(bridge, forward_pe) * bridge->config.forward_weight : 1.0f;
    float bwd_mod = bridge->config.enable_backward_pe ?
                    compute_pe_modulation(bridge, backward_pe) * bridge->config.backward_weight : 1.0f;
    float lat_mod = bridge->config.enable_lateral_pe ?
                    compute_pe_modulation(bridge, lateral_pe) * bridge->config.lateral_weight : 1.0f;

    /* Combined PE (weighted average) */
    float total_weight = bridge->config.forward_weight +
                         bridge->config.backward_weight +
                         bridge->config.lateral_weight;
    float combined = (fwd_mod * bridge->config.forward_weight +
                      bwd_mod * bridge->config.backward_weight +
                      lat_mod * bridge->config.lateral_weight) / total_weight;

    /* Apply precision */
    precision = nimcp_clampf(precision, 0.0f, 1.0f);
    float prec_factor = bridge->config.enable_precision_modulation ?
                        lerp(bridge->config.precision_min, bridge->config.precision_max, precision) : 1.0f;

    float final_mod = combined * prec_factor;

    effect->forward_pe = forward_pe;
    effect->backward_pe = backward_pe;
    effect->lateral_pe = lateral_pe;
    effect->combined_pe = (fabsf(forward_pe) + fabsf(backward_pe) + fabsf(lateral_pe)) / 3.0f;
    effect->precision = precision;
    effect->lr_modulation = final_mod;
    effect->effective_a_plus = base_a_plus * final_mod;
    effect->effective_a_minus = base_a_minus * final_mod;

    /* Update state */
    bridge->state.current_forward_pe = forward_pe;
    bridge->state.current_backward_pe = backward_pe;
    bridge->state.current_lateral_pe = lateral_pe;
    bridge->state.current_precision = precision;
    bridge->stats.backward_calls++;
    bridge->stats.avg_pe_magnitude = 0.9f * bridge->stats.avg_pe_magnitude +
                                     0.1f * effect->combined_pe;
    bridge->stats.avg_lr_modulation = 0.9f * bridge->stats.avg_lr_modulation +
                                      0.1f * final_mod;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// State and Statistics
//=============================================================================

int stdp_omni_bridge_get_state(stdp_omni_bridge_t bridge,
                               stdp_omni_bridge_state_t* state) {
    BRIDGE_BBB_VALIDATE(bridge, state, sizeof(*state));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_omni_bridge_get_state: bridge is NULL");
        return -1;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_omni_bridge_get_state: state is NULL");
        return -1;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int stdp_omni_bridge_get_stats(stdp_omni_bridge_t bridge,
                               stdp_omni_bridge_stats_t* stats) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_omni_bridge_get_stats: bridge is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_omni_bridge_get_stats: stats is NULL");
        return -1;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int stdp_omni_bridge_reset_stats(stdp_omni_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_omni_bridge_reset_stats: bridge is NULL");
        return -1;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int stdp_omni_bridge_update(stdp_omni_bridge_t bridge, float dt_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_omni_bridge_update: bridge is NULL");
        return -1;
    }
    (void)dt_ms;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute coherence based on PE stability */
    float pe_variance = fabsf(bridge->state.current_forward_pe) +
                        fabsf(bridge->state.current_backward_pe) +
                        fabsf(bridge->state.current_lateral_pe);
    pe_variance /= 3.0f;

    /* Low PE variance and high precision → high coherence */
    float pe_factor = expf(-pe_variance);
    float prec_factor = bridge->state.current_precision;
    bridge->state.bridge_coherence = 0.6f * pe_factor + 0.4f * prec_factor;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

float stdp_omni_bridge_get_coherence(stdp_omni_bridge_t bridge) {
    if (!bridge) return -1.0f;
    nimcp_mutex_lock(bridge->base.mutex);
    float coherence = bridge->state.bridge_coherence;
    nimcp_mutex_unlock(bridge->base.mutex);
    return coherence;
}

void stdp_omni_bridge_print_summary(stdp_omni_bridge_t bridge) {
    if (!bridge) {
        printf("STDP-Omni Bridge: NULL\n");
        return;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    printf("=== STDP-Omni Bridge Summary ===\n");
    printf("State:\n");
    printf("  Forward PE: %.3f\n", bridge->state.current_forward_pe);
    printf("  Backward PE: %.3f\n", bridge->state.current_backward_pe);
    printf("  Lateral PE: %.3f\n", bridge->state.current_lateral_pe);
    printf("  Precision: %.3f\n", bridge->state.current_precision);
    printf("  Coherence: %.3f\n", bridge->state.bridge_coherence);
    printf("Statistics:\n");
    printf("  Forward PE events: %lu\n", (unsigned long)bridge->stats.forward_pe_events);
    printf("  Backward PE events: %lu\n", (unsigned long)bridge->stats.backward_pe_events);
    printf("  Lateral PE events: %lu\n", (unsigned long)bridge->stats.lateral_pe_events);
    printf("  WM updates: %lu\n", (unsigned long)bridge->stats.wm_updates);
    printf("  Avg LR modulation: %.3f\n", bridge->stats.avg_lr_modulation);

    nimcp_mutex_unlock(bridge->base.mutex);
}
