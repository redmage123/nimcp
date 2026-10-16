/**
 * @file nimcp_neuromod_wm_bridge.c
 * @brief Neuromodulatory-Working Memory Inter-Layer Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-12
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "integration/inter/neuromod_wm/nimcp_neuromod_wm_bridge.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neuromod_wm_bridge)

#define LOG_MODULE "NEUROMOD_WM_BRIDGE"


/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct neuromod_wm_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    uint32_t magic;
    neuromod_wm_config_t config;
    neuromod_wm_state_t state;
    neuromod_wm_stats_t stats;
    bool connected;
};

static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

neuromod_wm_config_t neuromod_wm_default_config(void) {
    neuromod_wm_config_t config = {
        /* DA-WM coupling */
        .da_wm_gain_coupling = 0.7f,
        .da_optimal_level = DA_OPTIMAL_LEVEL,
        .d1_stability_weight = 0.6f,
        .d2_flexibility_weight = 0.4f,

        /* NE-Flexibility coupling */
        .ne_flexibility_coupling = NE_FLEXIBILITY_COUPLING,
        .ne_reset_threshold = 0.8f,
        .ne_tonic_baseline = 0.4f,

        /* 5-HT-Delay coupling */
        .ht_delay_coupling = HT_DELAY_COUPLING,
        .ht_impulse_suppression = 0.5f,

        /* Top-down feedback */
        .load_da_demand_gain = 0.6f,
        .switch_lc_trigger_gain = 0.7f,
        .overflow_stress_gain = 0.5f,

        /* Timing */
        .update_interval_ms = 10,

        /* Features */
        .enable_inverted_u = true,
        .enable_d1_d2_balance = true,
        .enable_flexibility_modulation = true,
        .enable_delay_tolerance = true,
        .enable_logging = false
    };
    return config;
}

neuromod_wm_bridge_t* neuromod_wm_create(const neuromod_wm_config_t* config) {
    neuromod_wm_bridge_t* bridge = nimcp_calloc(1, sizeof(neuromod_wm_bridge_t));
    NIMCP_API_CHECK_ALLOC(bridge, "Failed to allocate neuromod-wm bridge");

    bridge->magic = NEUROMOD_WM_BRIDGE_MAGIC;

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = neuromod_wm_default_config();
    }

    /* Initialize state */
    bridge->state.wm_gain = DA_WM_GAIN_BASELINE;
    bridge->state.stability_level = 0.5f;
    bridge->state.flexibility_level = 0.5f;
    bridge->state.delay_tolerance = 0.5f;
    bridge->state.d1_d2_balance = D1_D2_BALANCE_BASELINE;
    bridge->state.bridge_coherence = 1.0f;
    bridge->state.last_update_us = get_timestamp_us();

    bridge->connected = true;

    NIMCP_LOGGING_INFO("Created %s bridge", "neuromod_wm");
    return bridge;
}

void neuromod_wm_destroy(neuromod_wm_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_WM_BRIDGE_MAGIC) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "neuromod_wm");
    bridge->magic = 0;
    nimcp_free(bridge);
}

/* ============================================================================
 * Bottom-Up Modulation (Neuromod -> WM)
 * ============================================================================ */

int neuromod_wm_apply_da_gain(neuromod_wm_bridge_t* bridge,
                              float da_level, float* gain_out) {
    if (!bridge || bridge->magic != NEUROMOD_WM_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_wm_apply_da_gain: invalid parameter");
        return -1;
    }

    da_level = nimcp_clampf(da_level, 0.0f, 1.0f);
    bridge->state.da_level = da_level;

    /* Compute WM gain based on DA level
     * Inverted-U relationship:
     * - Low DA: poor maintenance (low gain)
     * - Optimal DA: best WM performance (high gain)
     * - High DA: noisy, distractible (reduced gain) */
    float optimal = bridge->config.da_optimal_level;
    float distance_from_optimal = fabsf(da_level - optimal);

    if (bridge->config.enable_inverted_u) {
        /* Inverted-U: peak at optimal, decline at extremes */
        float base_gain = DA_WM_GAIN_BASELINE;
        float max_boost = (DA_WM_GAIN_MAX - base_gain) * bridge->config.da_wm_gain_coupling;
        /* Gaussian-like curve centered at optimal */
        float sigma = 0.3f;
        float gain_boost = max_boost * expf(-distance_from_optimal * distance_from_optimal / (2.0f * sigma * sigma));
        bridge->state.wm_gain = base_gain + gain_boost;
    } else {
        /* Linear relationship */
        bridge->state.wm_gain = DA_WM_GAIN_BASELINE +
            (DA_WM_GAIN_MAX - DA_WM_GAIN_BASELINE) * da_level * bridge->config.da_wm_gain_coupling;
    }

    bridge->stats.gain_modulations++;

    if (gain_out) *gain_out = bridge->state.wm_gain;
    return 0;
}

int neuromod_wm_apply_d1_stability(neuromod_wm_bridge_t* bridge,
                                    float d1_activation, float* stability_out) {
    if (!bridge || bridge->magic != NEUROMOD_WM_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_wm_apply_d1_stability: invalid parameter");
        return -1;
    }
    if (!bridge->config.enable_d1_d2_balance) {
        if (stability_out) *stability_out = 0.5f;
        return 0;
    }

    d1_activation = nimcp_clampf(d1_activation, 0.0f, 1.0f);

    /* D1 receptor activation enhances WM maintenance (stability) */
    bridge->state.stability_level = d1_activation * bridge->config.d1_stability_weight;

    /* Update D1/D2 balance toward D1 */
    bridge->state.d1_d2_balance = nimcp_clampf(
        bridge->state.d1_d2_balance + (d1_activation - 0.5f) * 0.1f,
        0.0f, 1.0f
    );

    bridge->stats.stability_adjustments++;

    if (stability_out) *stability_out = bridge->state.stability_level;
    return 0;
}

int neuromod_wm_apply_d2_flexibility(neuromod_wm_bridge_t* bridge,
                                      float d2_activation, float* flexibility_out) {
    if (!bridge || bridge->magic != NEUROMOD_WM_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_wm_apply_d2_flexibility: invalid parameter");
        return -1;
    }
    if (!bridge->config.enable_d1_d2_balance) {
        if (flexibility_out) *flexibility_out = 0.5f;
        return 0;
    }

    d2_activation = nimcp_clampf(d2_activation, 0.0f, 1.0f);

    /* D2 receptor activation enhances WM updating (flexibility) */
    float d2_contribution = d2_activation * bridge->config.d2_flexibility_weight;
    bridge->state.flexibility_level = nimcp_clampf(
        bridge->state.flexibility_level * 0.8f + d2_contribution * 0.2f,
        0.0f, 1.0f
    );

    /* Update D1/D2 balance toward D2 */
    bridge->state.d1_d2_balance = nimcp_clampf(
        bridge->state.d1_d2_balance - (d2_activation - 0.5f) * 0.1f,
        0.0f, 1.0f
    );

    bridge->stats.flexibility_adjustments++;

    if (flexibility_out) *flexibility_out = bridge->state.flexibility_level;
    return 0;
}

int neuromod_wm_apply_ne_flexibility(neuromod_wm_bridge_t* bridge,
                                      float ne_level, float* flexibility_out) {
    if (!bridge || bridge->magic != NEUROMOD_WM_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_wm_apply_ne_flexibility: invalid parameter");
        return -1;
    }
    if (!bridge->config.enable_flexibility_modulation) {
        if (flexibility_out) *flexibility_out = 0.5f;
        return 0;
    }

    ne_level = nimcp_clampf(ne_level, 0.0f, 1.0f);
    bridge->state.ne_level = ne_level;

    /* High tonic NE promotes exploration/flexibility
     * Low tonic NE promotes exploitation/focus */
    float ne_deviation = ne_level - bridge->config.ne_tonic_baseline;
    float flexibility_change = ne_deviation * bridge->config.ne_flexibility_coupling;

    bridge->state.flexibility_level = nimcp_clampf(
        0.5f + flexibility_change,
        0.0f, 1.0f
    );

    bridge->stats.flexibility_adjustments++;

    if (flexibility_out) *flexibility_out = bridge->state.flexibility_level;
    return 0;
}

int neuromod_wm_apply_ne_reset(neuromod_wm_bridge_t* bridge,
                                float ne_burst, bool* reset_triggered) {
    if (!bridge || bridge->magic != NEUROMOD_WM_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_wm_apply_ne_reset: invalid parameter");
        return -1;
    }

    ne_burst = nimcp_clampf(ne_burst, 0.0f, 1.0f);

    bool triggered = ne_burst >= bridge->config.ne_reset_threshold;

    if (triggered) {
        bridge->stats.wm_resets++;
        /* Reset WM state - represents attention shift/task switch */
        bridge->state.wm_load = 0.0f;
        bridge->state.switch_demand = 0.0f;
        /* Temporarily boost flexibility */
        bridge->state.flexibility_level = fminf(bridge->state.flexibility_level + 0.3f, 1.0f);
    }

    if (reset_triggered) *reset_triggered = triggered;
    return 0;
}

int neuromod_wm_apply_ht_delay(neuromod_wm_bridge_t* bridge,
                                float ht_level, float* delay_out) {
    if (!bridge || bridge->magic != NEUROMOD_WM_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_wm_apply_ht_delay: invalid parameter");
        return -1;
    }
    if (!bridge->config.enable_delay_tolerance) {
        if (delay_out) *delay_out = 0.5f;
        return 0;
    }

    ht_level = nimcp_clampf(ht_level, 0.0f, 1.0f);
    bridge->state.ht_level = ht_level;

    /* 5-HT enables patience - maintaining WM during delays */
    bridge->state.delay_tolerance = 0.3f + ht_level * bridge->config.ht_delay_coupling;

    bridge->stats.delay_modulations++;

    if (delay_out) *delay_out = bridge->state.delay_tolerance;
    return 0;
}

/* ============================================================================
 * Top-Down Feedback (WM -> Neuromod)
 * ============================================================================ */

int neuromod_wm_report_load(neuromod_wm_bridge_t* bridge,
                            float wm_load, float* da_demand_out) {
    if (!bridge || bridge->magic != NEUROMOD_WM_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_wm_report_load: invalid parameter");
        return -1;
    }

    wm_load = nimcp_clampf(wm_load, 0.0f, 1.0f);
    bridge->state.wm_load = wm_load;

    /* Higher WM load signals greater DA demand to VTA */
    float da_demand = wm_load * bridge->config.load_da_demand_gain;

    if (wm_load > 0.5f) {
        bridge->stats.load_signals++;
    }

    bridge->stats.top_down_messages++;

    if (da_demand_out) *da_demand_out = da_demand;
    return 0;
}

int neuromod_wm_report_switch_need(neuromod_wm_bridge_t* bridge,
                                    float switch_urgency, float* lc_trigger_out) {
    if (!bridge || bridge->magic != NEUROMOD_WM_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_wm_report_switch_need: invalid parameter");
        return -1;
    }

    switch_urgency = nimcp_clampf(switch_urgency, 0.0f, 1.0f);
    bridge->state.switch_demand = switch_urgency;

    /* Task-switch need triggers LC phasic response */
    float lc_trigger = switch_urgency * bridge->config.switch_lc_trigger_gain;

    if (switch_urgency > 0.5f) {
        bridge->stats.switch_triggers++;
    }

    bridge->stats.top_down_messages++;

    if (lc_trigger_out) *lc_trigger_out = lc_trigger;
    return 0;
}

int neuromod_wm_report_overflow(neuromod_wm_bridge_t* bridge,
                                 float overflow_level, float* stress_out) {
    if (!bridge || bridge->magic != NEUROMOD_WM_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_wm_report_overflow: invalid parameter");
        return -1;
    }

    overflow_level = nimcp_clampf(overflow_level, 0.0f, 1.0f);
    bridge->state.overflow_signal = overflow_level;

    /* WM overflow indicates stress, increases NE/cortisol */
    float stress = overflow_level * bridge->config.overflow_stress_gain;

    if (overflow_level > 0.7f) {
        bridge->stats.overflow_events++;
    }

    bridge->stats.top_down_messages++;

    if (stress_out) *stress_out = stress;
    return 0;
}

/* ============================================================================
 * Unified Modulation
 * ============================================================================ */

int neuromod_wm_compute_modulation(neuromod_wm_bridge_t* bridge,
                                   float da_level, float ne_level, float ht_level,
                                   neuromod_wm_state_t* state_out) {
    if (!bridge || bridge->magic != NEUROMOD_WM_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_wm_compute_modulation: invalid parameter");
        return -1;
    }

    /* Apply all modulations */
    neuromod_wm_apply_da_gain(bridge, da_level, NULL);
    neuromod_wm_apply_ne_flexibility(bridge, ne_level, NULL);
    neuromod_wm_apply_ht_delay(bridge, ht_level, NULL);

    /* Compute D1/D2 balance from DA level */
    if (bridge->config.enable_d1_d2_balance) {
        /* At optimal DA, balance is neutral
         * Below optimal: favor D2 (flexibility to acquire more DA)
         * Above optimal: favor D1 (stability to maintain) */
        float optimal = bridge->config.da_optimal_level;
        if (da_level < optimal) {
            float d2_bias = (optimal - da_level) / optimal;
            neuromod_wm_apply_d2_flexibility(bridge, 0.5f + d2_bias * 0.3f, NULL);
        } else {
            float d1_bias = (da_level - optimal) / (1.0f - optimal);
            neuromod_wm_apply_d1_stability(bridge, 0.5f + d1_bias * 0.3f, NULL);
        }
    }

    /* Compute bridge coherence */
    float coherence = 1.0f;
    /* Reduce coherence if conflicting demands (high load + low DA) */
    if (bridge->state.wm_load > 0.7f && da_level < 0.3f) {
        coherence -= 0.3f;
    }
    /* Reduce coherence if WM overflow with poor delay tolerance */
    if (bridge->state.overflow_signal > 0.5f && bridge->state.delay_tolerance < 0.4f) {
        coherence -= 0.2f;
    }
    bridge->state.bridge_coherence = nimcp_clampf(coherence, 0.0f, 1.0f);

    bridge->state.last_update_us = get_timestamp_us();
    bridge->stats.bottom_up_messages += 3;
    bridge->stats.total_updates++;

    /* Update running averages */
    float alpha = 0.1f;
    bridge->stats.avg_wm_gain = alpha * bridge->state.wm_gain +
                                 (1.0f - alpha) * bridge->stats.avg_wm_gain;
    bridge->stats.avg_flexibility = alpha * bridge->state.flexibility_level +
                                     (1.0f - alpha) * bridge->stats.avg_flexibility;
    bridge->stats.avg_coherence = alpha * bridge->state.bridge_coherence +
                                   (1.0f - alpha) * bridge->stats.avg_coherence;

    if (state_out) *state_out = bridge->state;
    return 0;
}

/* ============================================================================
 * Update and State
 * ============================================================================ */

int neuromod_wm_update(neuromod_wm_bridge_t* bridge, float delta_ms) {
    if (!bridge || bridge->magic != NEUROMOD_WM_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_wm_update: invalid parameter");
        return -1;
    }

    /* Decay signals over time */
    float decay = expf(-delta_ms / 100.0f);  /* 100ms time constant */
    bridge->state.switch_demand *= decay;
    bridge->state.overflow_signal *= decay;

    /* Flexibility drifts back to baseline */
    float flexibility_drift = 0.01f * delta_ms / 10.0f;
    if (bridge->state.flexibility_level > 0.5f) {
        bridge->state.flexibility_level -= flexibility_drift;
    } else if (bridge->state.flexibility_level < 0.5f) {
        bridge->state.flexibility_level += flexibility_drift;
    }
    bridge->state.flexibility_level = nimcp_clampf(bridge->state.flexibility_level, 0.0f, 1.0f);

    bridge->state.last_update_us = get_timestamp_us();
    return 0;
}

int neuromod_wm_get_state(const neuromod_wm_bridge_t* bridge,
                          neuromod_wm_state_t* state_out) {
    if (!bridge || bridge->magic != NEUROMOD_WM_BRIDGE_MAGIC || !state_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_wm_get_state: invalid parameter");
        return -1;
    }
    *state_out = bridge->state;
    return 0;
}

int neuromod_wm_get_stats(const neuromod_wm_bridge_t* bridge,
                          neuromod_wm_stats_t* stats_out) {
    if (!bridge || bridge->magic != NEUROMOD_WM_BRIDGE_MAGIC || !stats_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_wm_get_stats: invalid parameter");
        return -1;
    }
    *stats_out = bridge->stats;
    return 0;
}

int neuromod_wm_reset_stats(neuromod_wm_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_WM_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_wm_reset_stats: invalid parameter");
        return -1;
    }
    memset(&bridge->stats, 0, sizeof(neuromod_wm_stats_t));
    return 0;
}

/* ============================================================================
 * Diagnostics
 * ============================================================================ */

bool neuromod_wm_is_connected(const neuromod_wm_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_WM_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_wm_is_connected: invalid parameter");
        return false;
    }
    return bridge->connected;
}

float neuromod_wm_get_coherence(const neuromod_wm_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_WM_BRIDGE_MAGIC) return 0.0f;
    return bridge->state.bridge_coherence;
}

void neuromod_wm_print_summary(const neuromod_wm_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_WM_BRIDGE_MAGIC) return;

    printf("=== Neuromod-WM Bridge Summary ===\n");
    printf("State:\n");
    printf("  WM Gain: %.2f\n", bridge->state.wm_gain);
    printf("  Stability: %.2f\n", bridge->state.stability_level);
    printf("  Flexibility: %.2f\n", bridge->state.flexibility_level);
    printf("  Delay Tolerance: %.2f\n", bridge->state.delay_tolerance);
    printf("  D1/D2 Balance: %.2f\n", bridge->state.d1_d2_balance);
    printf("  Coherence: %.2f\n", bridge->state.bridge_coherence);
    printf("Stats:\n");
    printf("  Gain Modulations: %u\n", bridge->stats.gain_modulations);
    printf("  WM Resets: %u\n", bridge->stats.wm_resets);
    printf("  Overflow Events: %u\n", bridge->stats.overflow_events);
    printf("  Total Updates: %lu\n", (unsigned long)bridge->stats.total_updates);
}
