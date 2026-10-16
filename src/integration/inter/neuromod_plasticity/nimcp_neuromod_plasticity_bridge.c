/**
 * @file nimcp_neuromod_plasticity_bridge.c
 * @brief Neuromodulatory-Plasticity Inter-Layer Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-12
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "integration/inter/neuromod_plasticity/nimcp_neuromod_plasticity_bridge.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_constants.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neuromod_plasticity_bridge)

#define LOG_MODULE "NEUROMOD_PLASTICITY_BRIDGE"


/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct neuromod_plasticity_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    uint32_t magic;
    neuromod_plasticity_config_t config;
    neuromod_plasticity_state_t state;
    neuromod_plasticity_stats_t stats;
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

neuromod_plasticity_config_t neuromod_plasticity_default_config(void) {
    neuromod_plasticity_config_t config = {
        /* DA-LTP coupling */
        .da_ltp_gate_threshold = DA_LTP_GATE_THRESHOLD,
        .da_ltp_gate_strength = DA_LTP_GATE_STRENGTH,
        .da_ltd_gate_strength = 0.5f,
        .da_reward_pe_coupling = 0.6f,

        /* NE-Memory coupling */
        .ne_memory_boost_coupling = NE_MEMORY_BOOST_COUPLING,
        .ne_emotional_tag_threshold = 0.6f,
        .ne_rapid_learning_boost = 0.4f,

        /* 5-HT-Consolidation coupling */
        .ht_consolidation_coupling = HT_CONSOLIDATION_COUPLING,
        .ht_interference_reduction = 0.3f,

        /* Habenula-Avoidance coupling */
        .hab_avoidance_coupling = HAB_AVOIDANCE_COUPLING,
        .hab_extinction_coupling = 0.4f,

        /* Eligibility traces */
        .eligibility_decay = ELIGIBILITY_TRACE_DECAY,
        .eligibility_window_ms = ELIGIBILITY_WINDOW_MS,

        /* Top-down feedback */
        .success_vta_trigger_gain = 0.6f,
        .novelty_lc_trigger_gain = 0.7f,
        .conflict_ht_modulation_gain = 0.5f,
        .miss_hab_trigger_gain = 0.5f,

        /* Timing */
        .update_interval_ms = 10,

        /* Features */
        .enable_da_gating = true,
        .enable_ne_boost = true,
        .enable_consolidation = true,
        .enable_avoidance_learning = true,
        .enable_eligibility_traces = true,
        .enable_logging = false
    };
    return config;
}

neuromod_plasticity_bridge_t* neuromod_plasticity_create(const neuromod_plasticity_config_t* config) {
    neuromod_plasticity_bridge_t* bridge = nimcp_calloc(1, sizeof(neuromod_plasticity_bridge_t));
    NIMCP_API_CHECK_ALLOC(bridge, "Failed to allocate neuromod-plasticity bridge");

    bridge->magic = NEUROMOD_PLASTICITY_BRIDGE_MAGIC;

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = neuromod_plasticity_default_config();
    }

    /* Initialize state */
    bridge->state.ltp_gate_level = 0.0f;
    bridge->state.ltd_gate_level = 0.0f;
    bridge->state.memory_boost = 1.0f;
    bridge->state.consolidation_rate = 0.5f;
    bridge->state.avoidance_signal = 0.0f;
    bridge->state.current_mode = PLASTICITY_MODE_NORMAL;
    bridge->state.eligibility_level = 0.0f;
    bridge->state.eligibility_start_us = 0;
    bridge->state.reward_prediction_error = 0.0f;
    bridge->state.expected_reward = 0.5f;
    bridge->state.learning_efficiency = 1.0f;
    bridge->state.bridge_coherence = 1.0f;
    bridge->state.last_update_us = get_timestamp_us();

    bridge->connected = true;

    NIMCP_LOGGING_INFO("Created %s bridge", "neuromod_plasticity");
    return bridge;
}

void neuromod_plasticity_destroy(neuromod_plasticity_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_PLASTICITY_BRIDGE_MAGIC) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "neuromod_plasticity");
    bridge->magic = 0;
    nimcp_free(bridge);
}

/* ============================================================================
 * Bottom-Up Modulation (Neuromod -> Plasticity)
 * ============================================================================ */

int neuromod_plasticity_apply_da_gating(neuromod_plasticity_bridge_t* bridge,
                                         float da_level, float* ltp_gate_out) {
    if (!bridge || bridge->magic != NEUROMOD_PLASTICITY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_plasticity_apply_da_gating: invalid parameter");
        return -1;
    }
    if (!bridge->config.enable_da_gating) {
        bridge->state.ltp_gate_level = 1.0f;  /* Always open */
        if (ltp_gate_out) *ltp_gate_out = 1.0f;
        return 0;
    }

    da_level = nimcp_clampf(da_level, 0.0f, 1.0f);
    bridge->state.da_level = da_level;

    /* DA opens the LTP gate when above threshold */
    if (da_level >= bridge->config.da_ltp_gate_threshold) {
        float gate_opening = (da_level - bridge->config.da_ltp_gate_threshold) /
                             (1.0f - bridge->config.da_ltp_gate_threshold);
        bridge->state.ltp_gate_level = gate_opening * bridge->config.da_ltp_gate_strength;
        bridge->stats.ltp_gate_openings++;
    } else {
        bridge->state.ltp_gate_level *= 0.9f;  /* Decay gate */
    }

    /* LTD is enabled at low DA (enables extinction) */
    if (da_level < 0.3f) {
        bridge->state.ltd_gate_level = (0.3f - da_level) / 0.3f * bridge->config.da_ltd_gate_strength;
        bridge->stats.ltd_gate_openings++;
    } else {
        bridge->state.ltd_gate_level *= 0.9f;
    }

    /* Update plasticity mode */
    if (da_level < bridge->config.da_ltp_gate_threshold) {
        bridge->state.current_mode = PLASTICITY_MODE_GATED;
    } else if (bridge->state.memory_boost > 1.2f) {
        bridge->state.current_mode = PLASTICITY_MODE_BOOSTED;
    } else {
        bridge->state.current_mode = PLASTICITY_MODE_NORMAL;
    }

    if (ltp_gate_out) *ltp_gate_out = bridge->state.ltp_gate_level;
    return 0;
}

int neuromod_plasticity_apply_reward_pe(neuromod_plasticity_bridge_t* bridge,
                                         float reward, float expected, float* rpe_out) {
    if (!bridge || bridge->magic != NEUROMOD_PLASTICITY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_plasticity_apply_reward_pe: invalid parameter");
        return -1;
    }

    reward = nimcp_clampf(reward, 0.0f, 1.0f);
    expected = nimcp_clampf(expected, 0.0f, 1.0f);

    /* Compute reward prediction error (RPE) */
    float rpe = (reward - expected) * bridge->config.da_reward_pe_coupling;
    bridge->state.reward_prediction_error = nimcp_clampf(rpe, -1.0f, 1.0f);
    bridge->state.expected_reward = expected;

    /* RPE affects LTP gate */
    if (rpe > 0.0f) {
        /* Positive RPE enhances LTP gate */
        bridge->state.ltp_gate_level = fminf(bridge->state.ltp_gate_level + rpe * 0.3f, 1.0f);
    } else if (rpe < 0.0f) {
        /* Negative RPE enhances LTD gate */
        bridge->state.ltd_gate_level = fminf(bridge->state.ltd_gate_level + fabsf(rpe) * 0.3f, 1.0f);
    }

    /* Update cumulative RPE */
    bridge->stats.total_rpe += rpe;
    float alpha = 0.1f;
    bridge->stats.avg_rpe = alpha * rpe + (1.0f - alpha) * bridge->stats.avg_rpe;

    if (rpe_out) *rpe_out = bridge->state.reward_prediction_error;
    return 0;
}

int neuromod_plasticity_apply_ne_boost(neuromod_plasticity_bridge_t* bridge,
                                        float ne_level, float* boost_out) {
    if (!bridge || bridge->magic != NEUROMOD_PLASTICITY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_plasticity_apply_ne_boost: invalid parameter");
        return -1;
    }
    if (!bridge->config.enable_ne_boost) {
        bridge->state.memory_boost = 1.0f;
        if (boost_out) *boost_out = 1.0f;
        return 0;
    }

    ne_level = nimcp_clampf(ne_level, 0.0f, 1.0f);
    bridge->state.ne_level = ne_level;

    /* NE boosts memory strength for emotionally salient events */
    float base_boost = 1.0f;
    if (ne_level >= bridge->config.ne_emotional_tag_threshold) {
        float boost_factor = (ne_level - bridge->config.ne_emotional_tag_threshold) /
                             (1.0f - bridge->config.ne_emotional_tag_threshold);
        bridge->state.memory_boost = base_boost +
            boost_factor * bridge->config.ne_memory_boost_coupling;

        /* Also enables rapid learning */
        bridge->state.memory_boost += boost_factor * bridge->config.ne_rapid_learning_boost;

        bridge->stats.memory_boosts++;
        bridge->state.current_mode = PLASTICITY_MODE_BOOSTED;
    } else {
        /* Decay boost toward baseline */
        bridge->state.memory_boost = bridge->state.memory_boost * 0.95f + base_boost * 0.05f;
    }

    if (boost_out) *boost_out = bridge->state.memory_boost;
    return 0;
}

int neuromod_plasticity_apply_ht_consolidation(neuromod_plasticity_bridge_t* bridge,
                                                float ht_level, float* rate_out) {
    if (!bridge || bridge->magic != NEUROMOD_PLASTICITY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_plasticity_apply_ht_consolidation: invalid parameter");
        return -1;
    }
    if (!bridge->config.enable_consolidation) {
        bridge->state.consolidation_rate = 0.5f;
        if (rate_out) *rate_out = 0.5f;
        return 0;
    }

    ht_level = nimcp_clampf(ht_level, 0.0f, 1.0f);
    bridge->state.ht_level = ht_level;

    /* 5-HT affects consolidation rate */
    float base_rate = 0.3f;
    bridge->state.consolidation_rate = base_rate +
        (1.0f - base_rate) * ht_level * bridge->config.ht_consolidation_coupling;

    /* High 5-HT during rest/sleep increases consolidation mode */
    if (ht_level > 0.7f && bridge->state.ne_level < 0.3f) {
        bridge->state.current_mode = PLASTICITY_MODE_CONSOLIDATING;
        bridge->stats.consolidation_events++;
    }

    if (rate_out) *rate_out = bridge->state.consolidation_rate;
    return 0;
}

int neuromod_plasticity_apply_hab_avoidance(neuromod_plasticity_bridge_t* bridge,
                                             float hab_level, float* avoidance_out) {
    if (!bridge || bridge->magic != NEUROMOD_PLASTICITY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_plasticity_apply_hab_avoidance: invalid parameter");
        return -1;
    }
    if (!bridge->config.enable_avoidance_learning) {
        bridge->state.avoidance_signal = 0.0f;
        if (avoidance_out) *avoidance_out = 0.0f;
        return 0;
    }

    hab_level = nimcp_clampf(hab_level, 0.0f, 1.0f);
    bridge->state.hab_level = hab_level;

    /* Habenula activity enables avoidance learning */
    bridge->state.avoidance_signal = hab_level * bridge->config.hab_avoidance_coupling;

    /* High habenula suppresses positive learning */
    if (hab_level > 0.7f) {
        bridge->state.current_mode = PLASTICITY_MODE_SUPPRESSED;
        bridge->state.ltp_gate_level *= (1.0f - hab_level * 0.5f);
    }

    if (hab_level > 0.5f) {
        bridge->stats.avoidance_signals++;
    }

    if (avoidance_out) *avoidance_out = bridge->state.avoidance_signal;
    return 0;
}

/* ============================================================================
 * Eligibility Trace Management
 * ============================================================================ */

int neuromod_plasticity_set_eligibility(neuromod_plasticity_bridge_t* bridge, float level) {
    if (!bridge || bridge->magic != NEUROMOD_PLASTICITY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_plasticity_set_eligibility: invalid parameter");
        return -1;
    }
    if (!bridge->config.enable_eligibility_traces) return 0;

    bridge->state.eligibility_level = nimcp_clampf(level, 0.0f, 1.0f);
    if (level > 0.0f) {
        bridge->state.eligibility_start_us = get_timestamp_us();
    }
    return 0;
}

int neuromod_plasticity_capture_eligibility(neuromod_plasticity_bridge_t* bridge, float da_signal) {
    if (!bridge || bridge->magic != NEUROMOD_PLASTICITY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_plasticity_capture_eligibility: invalid parameter");
        return -1;
    }
    if (!bridge->config.enable_eligibility_traces) return 0;

    da_signal = nimcp_clampf(da_signal, 0.0f, 1.0f);

    /* Check if within eligibility window */
    uint64_t now = get_timestamp_us();
    uint64_t elapsed_us = now - bridge->state.eligibility_start_us;
    uint64_t window_us = (uint64_t)bridge->config.eligibility_window_ms * 1000ULL;

    if (bridge->state.eligibility_level > 0.1f && elapsed_us < window_us) {
        /* DA captures the eligibility trace, converting to permanent change */
        float capture_strength = bridge->state.eligibility_level * da_signal;

        /* Boost LTP gate based on capture */
        bridge->state.ltp_gate_level = fminf(
            bridge->state.ltp_gate_level + capture_strength * 0.5f,
            1.0f
        );

        bridge->stats.eligibility_captures++;

        /* Reset eligibility after capture */
        bridge->state.eligibility_level *= 0.5f;
    }

    return 0;
}

int neuromod_plasticity_decay_eligibility(neuromod_plasticity_bridge_t* bridge, float delta_ms) {
    if (!bridge || bridge->magic != NEUROMOD_PLASTICITY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_plasticity_decay_eligibility: invalid parameter");
        return -1;
    }
    if (!bridge->config.enable_eligibility_traces) return 0;

    /* Decay eligibility trace */
    float decay_per_step = powf(bridge->config.eligibility_decay, delta_ms / 10.0f);
    bridge->state.eligibility_level *= decay_per_step;

    if (bridge->state.eligibility_level < 0.01f) {
        bridge->state.eligibility_level = 0.0f;
        if (bridge->state.eligibility_start_us > 0) {
            bridge->stats.eligibility_decays++;
            bridge->state.eligibility_start_us = 0;
        }
    }

    return 0;
}

/* ============================================================================
 * Top-Down Feedback (Plasticity -> Neuromod)
 * ============================================================================ */

int neuromod_plasticity_report_success(neuromod_plasticity_bridge_t* bridge,
                                        float success, float* vta_trigger_out) {
    if (!bridge || bridge->magic != NEUROMOD_PLASTICITY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_plasticity_report_success: invalid parameter");
        return -1;
    }

    success = nimcp_clampf(success, 0.0f, 1.0f);
    bridge->state.learning_success = success;

    /* Learning success triggers VTA reward signal */
    float vta_trigger = success * bridge->config.success_vta_trigger_gain;

    if (success > 0.5f) {
        bridge->stats.success_signals++;
    }

    bridge->stats.top_down_messages++;

    if (vta_trigger_out) *vta_trigger_out = vta_trigger;
    return 0;
}

int neuromod_plasticity_report_novelty(neuromod_plasticity_bridge_t* bridge,
                                        float novelty, float* lc_trigger_out) {
    if (!bridge || bridge->magic != NEUROMOD_PLASTICITY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_plasticity_report_novelty: invalid parameter");
        return -1;
    }

    novelty = nimcp_clampf(novelty, 0.0f, 1.0f);
    bridge->state.novelty_signal = novelty;

    /* Novel patterns trigger LC activation */
    float lc_trigger = novelty * bridge->config.novelty_lc_trigger_gain;

    if (novelty > 0.5f) {
        bridge->stats.novelty_triggers++;
    }

    bridge->stats.top_down_messages++;

    if (lc_trigger_out) *lc_trigger_out = lc_trigger;
    return 0;
}

int neuromod_plasticity_report_conflict(neuromod_plasticity_bridge_t* bridge,
                                         float conflict, float* ht_mod_out) {
    if (!bridge || bridge->magic != NEUROMOD_PLASTICITY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_plasticity_report_conflict: invalid parameter");
        return -1;
    }

    conflict = nimcp_clampf(conflict, 0.0f, 1.0f);
    bridge->state.memory_conflict = conflict;

    /* Memory conflict modulates 5-HT demand */
    float ht_mod = conflict * bridge->config.conflict_ht_modulation_gain;

    if (conflict > 0.5f) {
        bridge->stats.conflict_signals++;
    }

    bridge->stats.top_down_messages++;

    if (ht_mod_out) *ht_mod_out = ht_mod;
    return 0;
}

int neuromod_plasticity_report_prediction_miss(neuromod_plasticity_bridge_t* bridge,
                                                float miss, float* hab_trigger_out) {
    if (!bridge || bridge->magic != NEUROMOD_PLASTICITY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_plasticity_report_prediction_miss: invalid parameter");
        return -1;
    }

    miss = nimcp_clampf(miss, 0.0f, 1.0f);
    bridge->state.prediction_miss = miss;

    /* Prediction miss activates habenula */
    float hab_trigger = miss * bridge->config.miss_hab_trigger_gain;

    if (miss > 0.5f) {
        bridge->stats.prediction_misses++;
    }

    bridge->stats.top_down_messages++;

    if (hab_trigger_out) *hab_trigger_out = hab_trigger;
    return 0;
}

/* ============================================================================
 * Unified Modulation
 * ============================================================================ */

int neuromod_plasticity_compute_modulation(neuromod_plasticity_bridge_t* bridge,
                                           float da_level, float ne_level,
                                           float ht_level, float hab_level,
                                           neuromod_plasticity_state_t* state_out) {
    if (!bridge || bridge->magic != NEUROMOD_PLASTICITY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_plasticity_compute_modulation: invalid parameter");
        return -1;
    }

    /* Apply all modulations */
    neuromod_plasticity_apply_da_gating(bridge, da_level, NULL);
    neuromod_plasticity_apply_ne_boost(bridge, ne_level, NULL);
    neuromod_plasticity_apply_ht_consolidation(bridge, ht_level, NULL);
    neuromod_plasticity_apply_hab_avoidance(bridge, hab_level, NULL);

    /* Compute learning efficiency
     * High efficiency: good DA gating + memory boost + consolidation - avoidance */
    bridge->state.learning_efficiency =
        0.3f * bridge->state.ltp_gate_level +
        0.2f * (bridge->state.memory_boost - 1.0f) / 0.5f +  /* Normalize boost contribution */
        0.2f * bridge->state.consolidation_rate +
        0.3f * (1.0f - bridge->state.avoidance_signal);
    bridge->state.learning_efficiency = nimcp_clampf(bridge->state.learning_efficiency, 0.0f, 1.0f);

    /* Compute bridge coherence */
    float coherence = 1.0f;
    /* Reduce coherence if conflicting signals */
    if (bridge->state.ltp_gate_level > 0.5f && bridge->state.avoidance_signal > 0.5f) {
        coherence -= 0.2f;
    }
    if (bridge->state.memory_boost > 1.3f && bridge->state.consolidation_rate < 0.3f) {
        coherence -= 0.15f;
    }
    bridge->state.bridge_coherence = nimcp_clampf(coherence, 0.0f, 1.0f);

    bridge->state.last_update_us = get_timestamp_us();
    bridge->stats.bottom_up_messages += 4;
    bridge->stats.total_updates++;

    /* Update running averages */
    float alpha = 0.1f;
    bridge->stats.avg_ltp_gate = alpha * bridge->state.ltp_gate_level +
                                  (1.0f - alpha) * bridge->stats.avg_ltp_gate;
    bridge->stats.avg_memory_boost = alpha * bridge->state.memory_boost +
                                      (1.0f - alpha) * bridge->stats.avg_memory_boost;

    if (state_out) *state_out = bridge->state;
    return 0;
}

/* ============================================================================
 * Update and State
 * ============================================================================ */

int neuromod_plasticity_update(neuromod_plasticity_bridge_t* bridge, float delta_ms) {
    if (!bridge || bridge->magic != NEUROMOD_PLASTICITY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_plasticity_update: invalid parameter");
        return -1;
    }

    /* Decay eligibility trace */
    neuromod_plasticity_decay_eligibility(bridge, delta_ms);

    /* Decay top-down signals */
    float decay = expf(-delta_ms / 100.0f);
    bridge->state.learning_success *= decay;
    bridge->state.novelty_signal *= decay;
    bridge->state.memory_conflict *= decay;
    bridge->state.prediction_miss *= decay;

    /* Decay gates slowly */
    float slow_decay = expf(-delta_ms / 200.0f);
    bridge->state.ltp_gate_level *= slow_decay;
    bridge->state.ltd_gate_level *= slow_decay;

    /* Memory boost decays toward baseline */
    bridge->state.memory_boost = bridge->state.memory_boost * NIMCP_EMA_DECAY_DEFAULT + 1.0f * NIMCP_LEARNING_RATE_DEFAULT;

    bridge->state.last_update_us = get_timestamp_us();
    return 0;
}

int neuromod_plasticity_get_state(const neuromod_plasticity_bridge_t* bridge,
                                   neuromod_plasticity_state_t* state_out) {
    if (!bridge || bridge->magic != NEUROMOD_PLASTICITY_BRIDGE_MAGIC || !state_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_plasticity_get_state: invalid parameter");
        return -1;
    }
    *state_out = bridge->state;
    return 0;
}

int neuromod_plasticity_get_stats(const neuromod_plasticity_bridge_t* bridge,
                                   neuromod_plasticity_stats_t* stats_out) {
    if (!bridge || bridge->magic != NEUROMOD_PLASTICITY_BRIDGE_MAGIC || !stats_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_plasticity_get_stats: invalid parameter");
        return -1;
    }
    *stats_out = bridge->stats;
    return 0;
}

int neuromod_plasticity_reset_stats(neuromod_plasticity_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_PLASTICITY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_plasticity_reset_stats: invalid parameter");
        return -1;
    }
    memset(&bridge->stats, 0, sizeof(neuromod_plasticity_stats_t));
    return 0;
}

/* ============================================================================
 * Diagnostics
 * ============================================================================ */

bool neuromod_plasticity_is_connected(const neuromod_plasticity_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_PLASTICITY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_plasticity_is_connected: invalid parameter");
        return false;
    }
    return bridge->connected;
}

float neuromod_plasticity_get_efficiency(const neuromod_plasticity_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_PLASTICITY_BRIDGE_MAGIC) return 0.0f;
    return bridge->state.learning_efficiency;
}

float neuromod_plasticity_get_coherence(const neuromod_plasticity_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_PLASTICITY_BRIDGE_MAGIC) return 0.0f;
    return bridge->state.bridge_coherence;
}

plasticity_mode_t neuromod_plasticity_get_mode(const neuromod_plasticity_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_PLASTICITY_BRIDGE_MAGIC) {
        return PLASTICITY_MODE_NORMAL;
    }
    return bridge->state.current_mode;
}

const char* neuromod_plasticity_mode_name(plasticity_mode_t mode) {
    switch (mode) {
        case PLASTICITY_MODE_NORMAL:        return "Normal";
        case PLASTICITY_MODE_BOOSTED:       return "Boosted";
        case PLASTICITY_MODE_GATED:         return "Gated";
        case PLASTICITY_MODE_CONSOLIDATING: return "Consolidating";
        case PLASTICITY_MODE_SUPPRESSED:    return "Suppressed";
        default:                            return "Unknown";
    }
}

void neuromod_plasticity_print_summary(const neuromod_plasticity_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_PLASTICITY_BRIDGE_MAGIC) return;

    printf("=== Neuromod-Plasticity Bridge Summary ===\n");
    printf("State:\n");
    printf("  LTP Gate: %.2f\n", bridge->state.ltp_gate_level);
    printf("  LTD Gate: %.2f\n", bridge->state.ltd_gate_level);
    printf("  Memory Boost: %.2fx\n", bridge->state.memory_boost);
    printf("  Consolidation Rate: %.2f\n", bridge->state.consolidation_rate);
    printf("  Avoidance Signal: %.2f\n", bridge->state.avoidance_signal);
    printf("  Eligibility: %.2f\n", bridge->state.eligibility_level);
    printf("  Mode: %s\n", neuromod_plasticity_mode_name(bridge->state.current_mode));
    printf("  Efficiency: %.2f\n", bridge->state.learning_efficiency);
    printf("  Coherence: %.2f\n", bridge->state.bridge_coherence);
    printf("Stats:\n");
    printf("  LTP Gate Openings: %u\n", bridge->stats.ltp_gate_openings);
    printf("  Eligibility Captures: %u\n", bridge->stats.eligibility_captures);
    printf("  Avg RPE: %.3f\n", bridge->stats.avg_rpe);
    printf("  Total Updates: %lu\n", (unsigned long)bridge->stats.total_updates);
}
