/**
 * @file nimcp_neuromod_attention_bridge.c
 * @brief Neuromodulatory-Attention Inter-Layer Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-12
 */

#define _POSIX_C_SOURCE 200809L

#include "utils/bridge/nimcp_bridge_base.h"
#include "integration/inter/neuromod_attention/nimcp_neuromod_attention_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neuromod_attention_bridge)

#define LOG_MODULE "NEUROMOD_ATTENTION_BRIDGE"


/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct neuromod_attention_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    uint32_t magic;
    neuromod_attention_config_t config;
    neuromod_attention_state_t state;
    neuromod_attention_stats_t stats;
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

neuromod_attention_config_t neuromod_attention_default_config(void) {
    neuromod_attention_config_t config = {
        /* NE-Attention coupling */
        .ne_gain_coupling = 0.6f,
        .ne_phasic_shift_threshold = NE_PHASIC_SHIFT_THRESHOLD,
        .ne_tonic_vigilance_coupling = 0.5f,

        /* DA-Salience coupling */
        .da_salience_coupling = DA_SALIENCE_COUPLING,
        .da_motivation_coupling = 0.4f,

        /* 5-HT-Patience coupling */
        .ht_patience_coupling = HT_PATIENCE_COUPLING,
        .ht_impulse_suppression = 0.5f,

        /* Habenula-Aversion coupling */
        .hab_aversion_coupling = HAB_AVERSION_COUPLING,
        .hab_withdrawal_threshold = 0.6f,

        /* Top-down feedback */
        .novelty_lc_trigger_gain = 0.7f,
        .reward_vta_trigger_gain = 0.6f,
        .conflict_arousal_gain = 0.5f,

        /* Timing */
        .update_interval_ms = 10,

        /* Features */
        .enable_adaptive_gain = true,
        .enable_salience_filtering = true,
        .enable_patience_modulation = true,
        .enable_aversion_withdrawal = true,
        .enable_logging = false
    };
    return config;
}

neuromod_attention_bridge_t* neuromod_attention_create(const neuromod_attention_config_t* config) {
    neuromod_attention_bridge_t* bridge = nimcp_calloc(1, sizeof(neuromod_attention_bridge_t));
    NIMCP_API_CHECK_ALLOC(bridge, "Failed to allocate neuromod-attention bridge");

    bridge->magic = NEUROMOD_ATTENTION_BRIDGE_MAGIC;

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = neuromod_attention_default_config();
    }

    /* Initialize state */
    bridge->state.attention_gain = NE_ATTENTION_GAIN_BASELINE;
    bridge->state.vigilance_level = 0.5f;
    bridge->state.salience_boost = 0.0f;
    bridge->state.patience_capacity = 0.5f;
    bridge->state.aversion_level = 0.0f;
    bridge->state.bridge_coherence = 1.0f;
    bridge->state.last_update_us = get_timestamp_us();

    bridge->connected = true;

    NIMCP_LOGGING_INFO("Created %s bridge", "neuromod_attention");
    return bridge;
}

void neuromod_attention_destroy(neuromod_attention_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_ATTENTION_BRIDGE_MAGIC) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "neuromod_attention");
    bridge->magic = 0;
    nimcp_free(bridge);
}

/* ============================================================================
 * Bottom-Up Modulation (Neuromod -> Attention)
 * ============================================================================ */

int neuromod_attention_apply_ne_gain(neuromod_attention_bridge_t* bridge,
                                      float ne_level, float* gain_out) {
    if (!bridge || bridge->magic != NEUROMOD_ATTENTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_attention_apply_ne_gain: invalid parameter");
        return -1;
    }

    ne_level = nimcp_clampf(ne_level, 0.0f, 1.0f);
    bridge->state.ne_level = ne_level;

    /* Compute attention gain based on NE level
     * Adaptive gain theory: inverted-U relationship
     * Low NE: low gain (drowsy)
     * Medium NE: optimal gain (focused)
     * High NE: reduced specificity (stressed) */
    float optimal_ne = 0.6f;
    float distance_from_optimal = fabsf(ne_level - optimal_ne);

    if (bridge->config.enable_adaptive_gain) {
        /* Inverted-U: peak at optimal, decline at extremes */
        float base_gain = NE_ATTENTION_GAIN_BASELINE;
        float max_boost = (NE_ATTENTION_GAIN_MAX - base_gain) * bridge->config.ne_gain_coupling;
        float gain_boost = max_boost * (1.0f - distance_from_optimal / optimal_ne);
        bridge->state.attention_gain = base_gain + nimcp_clampf(gain_boost, 0.0f, max_boost);
    } else {
        /* Linear relationship */
        bridge->state.attention_gain = NE_ATTENTION_GAIN_BASELINE +
            (NE_ATTENTION_GAIN_MAX - NE_ATTENTION_GAIN_BASELINE) * ne_level * bridge->config.ne_gain_coupling;
    }

    /* Update vigilance based on tonic NE */
    bridge->state.vigilance_level = ne_level * bridge->config.ne_tonic_vigilance_coupling;

    bridge->stats.gain_modulations++;

    if (gain_out) *gain_out = bridge->state.attention_gain;
    return 0;
}

int neuromod_attention_apply_phasic_shift(neuromod_attention_bridge_t* bridge,
                                           float ne_burst, bool* shift_triggered) {
    if (!bridge || bridge->magic != NEUROMOD_ATTENTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_attention_apply_phasic_shift: invalid parameter");
        return -1;
    }

    ne_burst = nimcp_clampf(ne_burst, 0.0f, 1.0f);

    bool triggered = ne_burst >= bridge->config.ne_phasic_shift_threshold;

    if (triggered) {
        bridge->stats.phasic_shifts++;
        /* Phasic burst temporarily boosts gain */
        bridge->state.attention_gain = fminf(bridge->state.attention_gain * 1.5f, NE_ATTENTION_GAIN_MAX);
    }

    if (shift_triggered) *shift_triggered = triggered;
    return 0;
}

int neuromod_attention_apply_da_salience(neuromod_attention_bridge_t* bridge,
                                          float da_level, float* salience_out) {
    if (!bridge || bridge->magic != NEUROMOD_ATTENTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_attention_apply_da_salience: invalid parameter");
        return -1;
    }
    if (!bridge->config.enable_salience_filtering) {
        if (salience_out) *salience_out = 0.0f;
        return 0;
    }

    da_level = nimcp_clampf(da_level, 0.0f, 1.0f);
    bridge->state.da_level = da_level;

    /* DA boosts salience of reward-predictive stimuli */
    bridge->state.salience_boost = da_level * bridge->config.da_salience_coupling;

    bridge->stats.salience_boosts++;

    if (salience_out) *salience_out = bridge->state.salience_boost;
    return 0;
}

int neuromod_attention_apply_ht_patience(neuromod_attention_bridge_t* bridge,
                                          float ht_level, float* patience_out) {
    if (!bridge || bridge->magic != NEUROMOD_ATTENTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_attention_apply_ht_patience: invalid parameter");
        return -1;
    }
    if (!bridge->config.enable_patience_modulation) {
        if (patience_out) *patience_out = 0.5f;
        return 0;
    }

    ht_level = nimcp_clampf(ht_level, 0.0f, 1.0f);
    bridge->state.ht_level = ht_level;

    /* 5-HT increases sustained attention capacity and patience */
    bridge->state.patience_capacity = 0.3f + ht_level * bridge->config.ht_patience_coupling;

    bridge->stats.patience_modulations++;

    if (patience_out) *patience_out = bridge->state.patience_capacity;
    return 0;
}

int neuromod_attention_apply_hab_aversion(neuromod_attention_bridge_t* bridge,
                                           float hab_level, float* withdrawal_out) {
    if (!bridge || bridge->magic != NEUROMOD_ATTENTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_attention_apply_hab_aversion: invalid parameter");
        return -1;
    }
    if (!bridge->config.enable_aversion_withdrawal) {
        if (withdrawal_out) *withdrawal_out = 0.0f;
        return 0;
    }

    hab_level = nimcp_clampf(hab_level, 0.0f, 1.0f);
    bridge->state.hab_level = hab_level;

    /* Habenula activity triggers attention withdrawal from aversive stimuli */
    if (hab_level >= bridge->config.hab_withdrawal_threshold) {
        bridge->state.aversion_level = (hab_level - bridge->config.hab_withdrawal_threshold) /
                                        (1.0f - bridge->config.hab_withdrawal_threshold);
        bridge->state.aversion_level *= bridge->config.hab_aversion_coupling;
        bridge->stats.aversion_withdrawals++;
    } else {
        bridge->state.aversion_level = 0.0f;
    }

    if (withdrawal_out) *withdrawal_out = bridge->state.aversion_level;
    return 0;
}

/* ============================================================================
 * Top-Down Feedback (Attention -> Neuromod)
 * ============================================================================ */

int neuromod_attention_report_novelty(neuromod_attention_bridge_t* bridge,
                                       float novelty_score, float* lc_trigger_out) {
    if (!bridge || bridge->magic != NEUROMOD_ATTENTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_attention_report_novelty: invalid parameter");
        return -1;
    }

    novelty_score = nimcp_clampf(novelty_score, 0.0f, 1.0f);
    bridge->state.novelty_signal = novelty_score;

    /* Novelty triggers LC phasic burst */
    float lc_trigger = novelty_score * bridge->config.novelty_lc_trigger_gain;

    if (novelty_score > 0.5f) {
        bridge->stats.novelty_triggers++;
    }

    bridge->stats.top_down_messages++;

    if (lc_trigger_out) *lc_trigger_out = lc_trigger;
    return 0;
}

int neuromod_attention_report_reward_feature(neuromod_attention_bridge_t* bridge,
                                              float reward_value, float* vta_trigger_out) {
    if (!bridge || bridge->magic != NEUROMOD_ATTENTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_attention_report_reward_feature: invalid parameter");
        return -1;
    }

    reward_value = nimcp_clampf(reward_value, 0.0f, 1.0f);
    bridge->state.reward_signal = reward_value;

    /* Reward features activate VTA */
    float vta_trigger = reward_value * bridge->config.reward_vta_trigger_gain;

    if (reward_value > 0.3f) {
        bridge->stats.reward_triggers++;
    }

    bridge->stats.top_down_messages++;

    if (vta_trigger_out) *vta_trigger_out = vta_trigger;
    return 0;
}

int neuromod_attention_report_conflict(neuromod_attention_bridge_t* bridge,
                                        float conflict_level, float* arousal_demand_out) {
    if (!bridge || bridge->magic != NEUROMOD_ATTENTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_attention_report_conflict: invalid parameter");
        return -1;
    }

    conflict_level = nimcp_clampf(conflict_level, 0.0f, 1.0f);
    bridge->state.conflict_signal = conflict_level;

    /* Conflict increases arousal demand (NE/DA) */
    float arousal_demand = conflict_level * bridge->config.conflict_arousal_gain;

    if (conflict_level > 0.4f) {
        bridge->stats.conflict_signals++;
    }

    bridge->stats.top_down_messages++;

    if (arousal_demand_out) *arousal_demand_out = arousal_demand;
    return 0;
}

/* ============================================================================
 * Unified Modulation
 * ============================================================================ */

int neuromod_attention_compute_modulation(neuromod_attention_bridge_t* bridge,
                                          float ne_level, float da_level,
                                          float ht_level, float hab_level,
                                          neuromod_attention_state_t* state_out) {
    if (!bridge || bridge->magic != NEUROMOD_ATTENTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_attention_compute_modulation: invalid parameter");
        return -1;
    }

    /* Apply all modulations */
    neuromod_attention_apply_ne_gain(bridge, ne_level, NULL);
    neuromod_attention_apply_da_salience(bridge, da_level, NULL);
    neuromod_attention_apply_ht_patience(bridge, ht_level, NULL);
    neuromod_attention_apply_hab_aversion(bridge, hab_level, NULL);

    /* Compute bridge coherence */
    float coherence = 1.0f;
    /* Reduce coherence if conflicting signals (high arousal + high aversion)
     * Note: vigilance max is ~0.5 with default coupling, aversion max ~0.6
     * Thresholds set to be reachable with high NE and HAB inputs */
    if (bridge->state.vigilance_level > 0.4f && bridge->state.aversion_level > 0.4f) {
        coherence -= 0.2f;
    }
    bridge->state.bridge_coherence = nimcp_clampf(coherence, 0.0f, 1.0f);

    bridge->state.last_update_us = get_timestamp_us();
    bridge->stats.bottom_up_messages += 4;
    bridge->stats.total_updates++;

    /* Update running averages */
    float alpha = 0.1f;  /* Exponential moving average factor */
    bridge->stats.avg_attention_gain = alpha * bridge->state.attention_gain +
                                        (1.0f - alpha) * bridge->stats.avg_attention_gain;
    bridge->stats.avg_vigilance = alpha * bridge->state.vigilance_level +
                                   (1.0f - alpha) * bridge->stats.avg_vigilance;
    bridge->stats.avg_coherence = alpha * bridge->state.bridge_coherence +
                                   (1.0f - alpha) * bridge->stats.avg_coherence;

    if (state_out) *state_out = bridge->state;
    return 0;
}

/* ============================================================================
 * Update and State
 * ============================================================================ */

int neuromod_attention_update(neuromod_attention_bridge_t* bridge, float delta_ms) {
    if (!bridge || bridge->magic != NEUROMOD_ATTENTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_attention_update: invalid parameter");
        return -1;
    }

    /* Decay signals over time */
    float decay = expf(-delta_ms / 100.0f);  /* 100ms time constant */
    bridge->state.novelty_signal *= decay;
    bridge->state.reward_signal *= decay;
    bridge->state.conflict_signal *= decay;

    bridge->state.last_update_us = get_timestamp_us();
    return 0;
}

int neuromod_attention_get_state(const neuromod_attention_bridge_t* bridge,
                                  neuromod_attention_state_t* state_out) {
    if (!bridge || bridge->magic != NEUROMOD_ATTENTION_BRIDGE_MAGIC || !state_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_attention_get_state: invalid parameter");
        return -1;
    }
    *state_out = bridge->state;
    return 0;
}

int neuromod_attention_get_stats(const neuromod_attention_bridge_t* bridge,
                                  neuromod_attention_stats_t* stats_out) {
    if (!bridge || bridge->magic != NEUROMOD_ATTENTION_BRIDGE_MAGIC || !stats_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_attention_get_stats: invalid parameter");
        return -1;
    }
    *stats_out = bridge->stats;
    return 0;
}

int neuromod_attention_reset_stats(neuromod_attention_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_ATTENTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_attention_reset_stats: invalid parameter");
        return -1;
    }
    memset(&bridge->stats, 0, sizeof(neuromod_attention_stats_t));
    return 0;
}

/* ============================================================================
 * Diagnostics
 * ============================================================================ */

bool neuromod_attention_is_connected(const neuromod_attention_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_ATTENTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_attention_is_connected: invalid parameter");
        return false;
    }
    return bridge->connected;
}

float neuromod_attention_get_coherence(const neuromod_attention_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_ATTENTION_BRIDGE_MAGIC) return 0.0f;
    return bridge->state.bridge_coherence;
}

void neuromod_attention_print_summary(const neuromod_attention_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_ATTENTION_BRIDGE_MAGIC) return;

    printf("=== Neuromod-Attention Bridge Summary ===\n");
    printf("State:\n");
    printf("  Attention Gain: %.2f\n", bridge->state.attention_gain);
    printf("  Vigilance: %.2f\n", bridge->state.vigilance_level);
    printf("  Salience Boost: %.2f\n", bridge->state.salience_boost);
    printf("  Patience: %.2f\n", bridge->state.patience_capacity);
    printf("  Aversion: %.2f\n", bridge->state.aversion_level);
    printf("  Coherence: %.2f\n", bridge->state.bridge_coherence);
    printf("Stats:\n");
    printf("  Gain Modulations: %u\n", bridge->stats.gain_modulations);
    printf("  Phasic Shifts: %u\n", bridge->stats.phasic_shifts);
    printf("  Novelty Triggers: %u\n", bridge->stats.novelty_triggers);
    printf("  Total Updates: %lu\n", (unsigned long)bridge->stats.total_updates);
}
