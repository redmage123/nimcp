/**
 * @file nimcp_neuromod_reasoning_bridge.c
 * @brief Neuromodulatory-Superhuman Reasoning Inter-Layer Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-12
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "integration/inter/neuromod_reasoning/nimcp_neuromod_reasoning_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neuromod_reasoning_bridge)

#define LOG_MODULE "NEUROMOD_REASONING_BRIDGE"


/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct neuromod_reasoning_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    uint32_t magic;
    neuromod_reasoning_config_t config;
    neuromod_reasoning_state_t state;
    neuromod_reasoning_stats_t stats;
    bool connected;

    /* Internal tracking for confidence calibration */
    float cumulative_confidence;
    float cumulative_accuracy;
    uint32_t calibration_samples;
};

static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

neuromod_reasoning_config_t neuromod_reasoning_default_config(void) {
    neuromod_reasoning_config_t config = {
        /* DA-Confidence coupling */
        .da_confidence_coupling = DA_CONFIDENCE_COUPLING,
        .da_curiosity_coupling = DA_CURIOSITY_COUPLING,
        .da_reward_sensitivity = 0.5f,

        /* NE-Control coupling */
        .ne_control_coupling = NE_CONTROL_COUPLING,
        .ne_alertness_coupling = NE_ALERTNESS_COUPLING,
        .ne_mode_switch_threshold = MODE_SWITCH_THRESHOLD,

        /* 5-HT-Deliberation coupling */
        .ht_deliberation_coupling = HT_DELIBERATION_COUPLING,
        .ht_patience_coupling = HT_PATIENCE_COUPLING,
        .ht_impulse_suppression = 0.5f,

        /* Habenula-Error coupling */
        .hab_error_coupling = HAB_ERROR_COUPLING,
        .hab_strategy_revision_gain = 0.5f,

        /* Metacognition parameters */
        .confidence_baseline = CONFIDENCE_BASELINE,
        .control_baseline = CONTROL_BASELINE,

        /* Top-down feedback */
        .success_vta_trigger_gain = 0.6f,
        .novelty_lc_trigger_gain = 0.7f,
        .depth_ht_demand_gain = 0.5f,
        .error_hab_trigger_gain = 0.5f,

        /* Timing */
        .update_interval_ms = 10,

        /* Features */
        .enable_confidence_modulation = true,
        .enable_control_modulation = true,
        .enable_deliberation_modulation = true,
        .enable_error_monitoring = true,
        .enable_mode_classification = true,
        .enable_logging = false
    };
    return config;
}

neuromod_reasoning_bridge_t* neuromod_reasoning_create(const neuromod_reasoning_config_t* config) {
    neuromod_reasoning_bridge_t* bridge = nimcp_calloc(1, sizeof(neuromod_reasoning_bridge_t));
    NIMCP_API_CHECK_ALLOC(bridge, "Failed to allocate neuromod-reasoning bridge");

    bridge->magic = NEUROMOD_REASONING_BRIDGE_MAGIC;

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = neuromod_reasoning_default_config();
    }

    /* Initialize state */
    bridge->state.confidence_level = bridge->config.confidence_baseline;
    bridge->state.curiosity_level = 0.5f;
    bridge->state.cognitive_control = bridge->config.control_baseline;
    bridge->state.alertness_level = 0.5f;
    bridge->state.deliberation_level = 0.5f;
    bridge->state.patience_level = 0.5f;
    bridge->state.error_sensitivity = 0.5f;
    bridge->state.current_mode = REASONING_MODE_BALANCED;
    bridge->state.reasoning_quality = 0.5f;
    bridge->state.metacognitive_awareness = 0.5f;
    bridge->state.bridge_coherence = 1.0f;
    bridge->state.last_update_us = get_timestamp_us();

    /* Initialize calibration tracking */
    bridge->cumulative_confidence = 0.0f;
    bridge->cumulative_accuracy = 0.0f;
    bridge->calibration_samples = 0;

    bridge->connected = true;

    NIMCP_LOGGING_INFO("Created %s bridge", "neuromod_reasoning");
    return bridge;
}

void neuromod_reasoning_destroy(neuromod_reasoning_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_REASONING_BRIDGE_MAGIC) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "neuromod_reasoning");
    bridge->magic = 0;
    nimcp_free(bridge);
}

/* ============================================================================
 * Bottom-Up Modulation (Neuromod -> Reasoning)
 * ============================================================================ */

int neuromod_reasoning_apply_da_confidence(neuromod_reasoning_bridge_t* bridge,
                                            float da_level, float* conf_out) {
    if (!bridge || bridge->magic != NEUROMOD_REASONING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_reasoning_apply_da_confidence: invalid parameter");
        return -1;
    }
    if (!bridge->config.enable_confidence_modulation) {
        if (conf_out) *conf_out = CONFIDENCE_BASELINE;
        return 0;
    }

    da_level = nimcp_clampf(da_level, 0.0f, 1.0f);
    bridge->state.da_level = da_level;

    /* DA increases confidence (but can lead to overconfidence) */
    float base_conf = bridge->config.confidence_baseline;
    bridge->state.confidence_level = base_conf +
        (1.0f - base_conf) * da_level * bridge->config.da_confidence_coupling;

    bridge->stats.confidence_modulations++;

    if (conf_out) *conf_out = bridge->state.confidence_level;
    return 0;
}

int neuromod_reasoning_apply_da_curiosity(neuromod_reasoning_bridge_t* bridge,
                                           float da_level, float* curiosity_out) {
    if (!bridge || bridge->magic != NEUROMOD_REASONING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_reasoning_apply_da_curiosity: invalid parameter");
        return -1;
    }

    da_level = nimcp_clampf(da_level, 0.0f, 1.0f);

    /* DA drives curiosity and exploration */
    float base_curiosity = 0.3f;
    bridge->state.curiosity_level = base_curiosity +
        (1.0f - base_curiosity) * da_level * bridge->config.da_curiosity_coupling;

    if (curiosity_out) *curiosity_out = bridge->state.curiosity_level;
    return 0;
}

int neuromod_reasoning_apply_ne_control(neuromod_reasoning_bridge_t* bridge,
                                         float ne_level, float* control_out) {
    if (!bridge || bridge->magic != NEUROMOD_REASONING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_reasoning_apply_ne_control: invalid parameter");
        return -1;
    }
    if (!bridge->config.enable_control_modulation) {
        if (control_out) *control_out = CONTROL_BASELINE;
        return 0;
    }

    ne_level = nimcp_clampf(ne_level, 0.0f, 1.0f);
    bridge->state.ne_level = ne_level;

    /* NE enhances cognitive control (inverted-U)
     * Too little: poor control (drowsy)
     * Optimal: best control
     * Too much: anxiety impairs control */
    float optimal_ne = 0.6f;
    float distance_from_optimal = fabsf(ne_level - optimal_ne);

    float base_control = bridge->config.control_baseline;
    float max_boost = (1.0f - base_control) * bridge->config.ne_control_coupling;

    /* Gaussian-like curve centered at optimal */
    float sigma = 0.25f;
    float control_boost = max_boost * expf(-distance_from_optimal * distance_from_optimal / (2.0f * sigma * sigma));
    bridge->state.cognitive_control = base_control + control_boost;

    bridge->stats.control_modulations++;

    if (control_out) *control_out = bridge->state.cognitive_control;
    return 0;
}

int neuromod_reasoning_apply_ne_alertness(neuromod_reasoning_bridge_t* bridge,
                                           float ne_level, float* alert_out) {
    if (!bridge || bridge->magic != NEUROMOD_REASONING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_reasoning_apply_ne_alertness: invalid parameter");
        return -1;
    }

    ne_level = nimcp_clampf(ne_level, 0.0f, 1.0f);

    /* NE directly affects alertness */
    float base_alertness = 0.2f;
    bridge->state.alertness_level = base_alertness +
        (1.0f - base_alertness) * ne_level * bridge->config.ne_alertness_coupling;

    if (alert_out) *alert_out = bridge->state.alertness_level;
    return 0;
}

int neuromod_reasoning_apply_ht_deliberation(neuromod_reasoning_bridge_t* bridge,
                                              float ht_level, float* delib_out) {
    if (!bridge || bridge->magic != NEUROMOD_REASONING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_reasoning_apply_ht_deliberation: invalid parameter");
        return -1;
    }
    if (!bridge->config.enable_deliberation_modulation) {
        if (delib_out) *delib_out = 0.5f;
        return 0;
    }

    ht_level = nimcp_clampf(ht_level, 0.0f, 1.0f);
    bridge->state.ht_level = ht_level;

    /* 5-HT enables deliberate, careful thinking */
    float base_delib = 0.3f;
    bridge->state.deliberation_level = base_delib +
        (1.0f - base_delib) * ht_level * bridge->config.ht_deliberation_coupling;

    bridge->stats.deliberation_modulations++;

    if (delib_out) *delib_out = bridge->state.deliberation_level;
    return 0;
}

int neuromod_reasoning_apply_ht_patience(neuromod_reasoning_bridge_t* bridge,
                                          float ht_level, float* patience_out) {
    if (!bridge || bridge->magic != NEUROMOD_REASONING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_reasoning_apply_ht_patience: invalid parameter");
        return -1;
    }

    ht_level = nimcp_clampf(ht_level, 0.0f, 1.0f);

    /* 5-HT enables patience and impulse control */
    float base_patience = 0.2f;
    bridge->state.patience_level = base_patience +
        (1.0f - base_patience) * ht_level * bridge->config.ht_patience_coupling;

    if (patience_out) *patience_out = bridge->state.patience_level;
    return 0;
}

int neuromod_reasoning_apply_hab_error(neuromod_reasoning_bridge_t* bridge,
                                        float hab_level, float* error_sens_out) {
    if (!bridge || bridge->magic != NEUROMOD_REASONING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_reasoning_apply_hab_error: invalid parameter");
        return -1;
    }
    if (!bridge->config.enable_error_monitoring) {
        if (error_sens_out) *error_sens_out = 0.5f;
        return 0;
    }

    hab_level = nimcp_clampf(hab_level, 0.0f, 1.0f);
    bridge->state.hab_level = hab_level;

    /* Habenula increases error sensitivity and triggers strategy revision */
    float base_sens = 0.3f;
    bridge->state.error_sensitivity = base_sens +
        (1.0f - base_sens) * hab_level * bridge->config.hab_error_coupling;

    /* High habenula reduces confidence */
    if (hab_level > 0.5f) {
        bridge->state.confidence_level *= (1.0f - hab_level * 0.3f);
        bridge->stats.error_signals++;
    }

    if (error_sens_out) *error_sens_out = bridge->state.error_sensitivity;
    return 0;
}

/* ============================================================================
 * Top-Down Feedback (Reasoning -> Neuromod)
 * ============================================================================ */

int neuromod_reasoning_report_success(neuromod_reasoning_bridge_t* bridge,
                                       float success, float* vta_trigger_out) {
    if (!bridge || bridge->magic != NEUROMOD_REASONING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_reasoning_report_success: invalid parameter");
        return -1;
    }

    success = nimcp_clampf(success, 0.0f, 1.0f);
    bridge->state.success_signal = success;

    /* Reasoning success triggers VTA reward */
    float vta_trigger = success * bridge->config.success_vta_trigger_gain;

    if (success > 0.5f) {
        bridge->stats.successful_reasoning++;
    }

    /* Update calibration tracking */
    bridge->cumulative_accuracy += success;
    bridge->cumulative_confidence += bridge->state.confidence_level;
    bridge->calibration_samples++;

    bridge->stats.top_down_messages++;

    if (vta_trigger_out) *vta_trigger_out = vta_trigger;
    return 0;
}

int neuromod_reasoning_report_novelty(neuromod_reasoning_bridge_t* bridge,
                                       float novelty, float* lc_trigger_out) {
    if (!bridge || bridge->magic != NEUROMOD_REASONING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_reasoning_report_novelty: invalid parameter");
        return -1;
    }

    novelty = nimcp_clampf(novelty, 0.0f, 1.0f);
    bridge->state.novelty_signal = novelty;

    /* Novel problems trigger LC activation */
    float lc_trigger = novelty * bridge->config.novelty_lc_trigger_gain;

    if (novelty > 0.5f) {
        bridge->stats.novel_problems++;
    }

    bridge->stats.top_down_messages++;

    if (lc_trigger_out) *lc_trigger_out = lc_trigger;
    return 0;
}

int neuromod_reasoning_report_depth_need(neuromod_reasoning_bridge_t* bridge,
                                          float depth, float* ht_demand_out) {
    if (!bridge || bridge->magic != NEUROMOD_REASONING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_reasoning_report_depth_need: invalid parameter");
        return -1;
    }

    depth = nimcp_clampf(depth, 0.0f, 1.0f);
    bridge->state.depth_demand = depth;

    /* Deep thinking need triggers 5-HT demand */
    float ht_demand = depth * bridge->config.depth_ht_demand_gain;

    if (depth > 0.5f) {
        bridge->stats.deep_thinking_episodes++;
    }

    bridge->stats.top_down_messages++;

    if (ht_demand_out) *ht_demand_out = ht_demand;
    return 0;
}

int neuromod_reasoning_report_error(neuromod_reasoning_bridge_t* bridge,
                                     float error_severity, float* hab_trigger_out) {
    if (!bridge || bridge->magic != NEUROMOD_REASONING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_reasoning_report_error: invalid parameter");
        return -1;
    }

    error_severity = nimcp_clampf(error_severity, 0.0f, 1.0f);
    bridge->state.error_signal = error_severity;

    /* Reasoning errors trigger habenula */
    float hab_trigger = error_severity * bridge->config.error_hab_trigger_gain;

    if (error_severity > 0.3f) {
        bridge->stats.reasoning_errors++;
    }

    bridge->stats.top_down_messages++;

    if (hab_trigger_out) *hab_trigger_out = hab_trigger;
    return 0;
}

/* ============================================================================
 * Metacognitive Queries
 * ============================================================================ */

float neuromod_reasoning_get_confidence_calibration(neuromod_reasoning_bridge_t* bridge,
                                                     float actual_accuracy) {
    if (!bridge || bridge->magic != NEUROMOD_REASONING_BRIDGE_MAGIC) return 0.0f;

    /* Compare average confidence to average accuracy */
    if (bridge->calibration_samples < 10) {
        return 0.5f;  /* Not enough data */
    }

    float avg_conf = bridge->cumulative_confidence / (float)bridge->calibration_samples;
    float avg_acc = bridge->cumulative_accuracy / (float)bridge->calibration_samples;

    /* Perfect calibration = 1.0, poor calibration = 0.0 */
    float calibration = 1.0f - fabsf(avg_conf - avg_acc);
    bridge->stats.confidence_accuracy_corr = calibration;

    return nimcp_clampf(calibration, 0.0f, 1.0f);
}

bool neuromod_reasoning_should_switch_mode(neuromod_reasoning_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_REASONING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_reasoning_should_switch_mode: invalid parameter");
        return false;
    }

    /* Switch mode if:
     * - High NE and currently in intuitive mode
     * - Low NE and currently in analytical mode
     * - High error signal */

    if (bridge->state.ne_level > bridge->config.ne_mode_switch_threshold &&
        bridge->state.current_mode == REASONING_MODE_INTUITIVE) {
        return true;
    }

    if (bridge->state.ne_level < 0.4f &&
        bridge->state.current_mode == REASONING_MODE_ANALYTICAL) {
        return true;
    }

    if (bridge->state.error_signal > 0.7f) {
        return true;
    }

    return false;  /* No mode switch needed - normal condition */
}

float neuromod_reasoning_estimate_effort_needed(neuromod_reasoning_bridge_t* bridge,
                                                 float problem_complexity) {
    if (!bridge || bridge->magic != NEUROMOD_REASONING_BRIDGE_MAGIC) return 0.5f;

    problem_complexity = nimcp_clampf(problem_complexity, 0.0f, 1.0f);

    /* Effort needed based on:
     * - Problem complexity
     * - Current cognitive control (inverse)
     * - Current alertness (inverse)
     * - Deliberation level (inverse) */
    float resource_availability = (bridge->state.cognitive_control +
                                   bridge->state.alertness_level +
                                   bridge->state.deliberation_level) / 3.0f;

    float effort = problem_complexity * (1.5f - resource_availability);
    return nimcp_clampf(effort, 0.0f, 1.0f);
}

/* ============================================================================
 * Mode Classification
 * ============================================================================ */

reasoning_mode_t neuromod_reasoning_classify_mode(const neuromod_reasoning_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_REASONING_BRIDGE_MAGIC) {
        return REASONING_MODE_BALANCED;
    }

    float ne = bridge->state.ne_level;
    float da = bridge->state.da_level;
    float ht = bridge->state.ht_level;
    float hab = bridge->state.hab_level;

    /* Check for impaired first */
    if (hab > 0.7f) {
        return REASONING_MODE_IMPAIRED;
    }

    /* High DA = creative/exploratory */
    if (da > 0.7f && bridge->state.curiosity_level > 0.6f) {
        return REASONING_MODE_CREATIVE;
    }

    /* High 5-HT = cautious */
    if (ht > 0.7f && bridge->state.patience_level > 0.6f) {
        return REASONING_MODE_CAUTIOUS;
    }

    /* NE determines intuitive vs analytical */
    if (ne > bridge->config.ne_mode_switch_threshold) {
        return REASONING_MODE_ANALYTICAL;
    } else if (ne < 0.4f) {
        return REASONING_MODE_INTUITIVE;
    }

    return REASONING_MODE_BALANCED;
}

const char* neuromod_reasoning_mode_name(reasoning_mode_t mode) {
    switch (mode) {
        case REASONING_MODE_BALANCED:   return "Balanced";
        case REASONING_MODE_INTUITIVE:  return "Intuitive";
        case REASONING_MODE_ANALYTICAL: return "Analytical";
        case REASONING_MODE_CREATIVE:   return "Creative";
        case REASONING_MODE_CAUTIOUS:   return "Cautious";
        case REASONING_MODE_IMPAIRED:   return "Impaired";
        default:                        return "Unknown";
    }
}

/* ============================================================================
 * Unified Modulation
 * ============================================================================ */

int neuromod_reasoning_compute_modulation(neuromod_reasoning_bridge_t* bridge,
                                          float da_level, float ne_level,
                                          float ht_level, float hab_level,
                                          neuromod_reasoning_state_t* state_out) {
    if (!bridge || bridge->magic != NEUROMOD_REASONING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_reasoning_compute_modulation: invalid parameter");
        return -1;
    }

    /* Apply all modulations */
    neuromod_reasoning_apply_da_confidence(bridge, da_level, NULL);
    neuromod_reasoning_apply_da_curiosity(bridge, da_level, NULL);
    neuromod_reasoning_apply_ne_control(bridge, ne_level, NULL);
    neuromod_reasoning_apply_ne_alertness(bridge, ne_level, NULL);
    neuromod_reasoning_apply_ht_deliberation(bridge, ht_level, NULL);
    neuromod_reasoning_apply_ht_patience(bridge, ht_level, NULL);
    neuromod_reasoning_apply_hab_error(bridge, hab_level, NULL);

    /* Classify mode */
    reasoning_mode_t new_mode = neuromod_reasoning_classify_mode(bridge);
    if (new_mode != bridge->state.current_mode) {
        bridge->stats.mode_switches++;

        /* Track mode episodes */
        switch (new_mode) {
            case REASONING_MODE_INTUITIVE:
                bridge->stats.intuitive_episodes++;
                break;
            case REASONING_MODE_ANALYTICAL:
                bridge->stats.analytical_episodes++;
                break;
            case REASONING_MODE_CREATIVE:
                bridge->stats.creative_episodes++;
                break;
            default:
                break;
        }

        bridge->state.current_mode = new_mode;
    }

    /* Compute reasoning quality */
    bridge->state.reasoning_quality =
        0.25f * bridge->state.cognitive_control +
        0.25f * bridge->state.alertness_level +
        0.20f * bridge->state.deliberation_level +
        0.15f * bridge->state.confidence_level +
        0.15f * (1.0f - bridge->state.error_sensitivity * 0.5f);
    bridge->state.reasoning_quality = nimcp_clampf(bridge->state.reasoning_quality, 0.0f, 1.0f);

    /* Compute metacognitive awareness */
    bridge->state.metacognitive_awareness =
        0.4f * bridge->state.cognitive_control +
        0.3f * bridge->state.deliberation_level +
        0.3f * bridge->state.error_sensitivity;

    /* Compute bridge coherence */
    float coherence = 1.0f;
    /* Reduce coherence if overconfident (high confidence + high error) */
    if (bridge->state.confidence_level > 0.7f && bridge->state.error_signal > 0.5f) {
        coherence -= 0.3f;
    }
    /* Reduce coherence if impaired */
    if (bridge->state.current_mode == REASONING_MODE_IMPAIRED) {
        coherence -= 0.3f;
    }
    bridge->state.bridge_coherence = nimcp_clampf(coherence, 0.0f, 1.0f);

    bridge->state.last_update_us = get_timestamp_us();
    bridge->stats.bottom_up_messages += 7;
    bridge->stats.total_updates++;

    /* Update running averages */
    float alpha = 0.1f;
    bridge->stats.avg_confidence = alpha * bridge->state.confidence_level +
                                    (1.0f - alpha) * bridge->stats.avg_confidence;
    bridge->stats.avg_control = alpha * bridge->state.cognitive_control +
                                 (1.0f - alpha) * bridge->stats.avg_control;
    bridge->stats.avg_reasoning_quality = alpha * bridge->state.reasoning_quality +
                                           (1.0f - alpha) * bridge->stats.avg_reasoning_quality;

    if (state_out) *state_out = bridge->state;
    return 0;
}

/* ============================================================================
 * Update and State
 * ============================================================================ */

int neuromod_reasoning_update(neuromod_reasoning_bridge_t* bridge, float delta_ms) {
    if (!bridge || bridge->magic != NEUROMOD_REASONING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_reasoning_update: invalid parameter");
        return -1;
    }

    /* Decay feedback signals */
    float decay = expf(-delta_ms / 100.0f);
    bridge->state.success_signal *= decay;
    bridge->state.novelty_signal *= decay;
    bridge->state.depth_demand *= decay;
    bridge->state.error_signal *= decay;

    /* Confidence slowly drifts toward baseline */
    float conf_drift = 0.001f * delta_ms / 10.0f;
    if (bridge->state.confidence_level > bridge->config.confidence_baseline) {
        bridge->state.confidence_level -= conf_drift;
    } else if (bridge->state.confidence_level < bridge->config.confidence_baseline) {
        bridge->state.confidence_level += conf_drift;
    }

    bridge->state.last_update_us = get_timestamp_us();
    return 0;
}

int neuromod_reasoning_get_state(const neuromod_reasoning_bridge_t* bridge,
                                  neuromod_reasoning_state_t* state_out) {
    if (!bridge || bridge->magic != NEUROMOD_REASONING_BRIDGE_MAGIC || !state_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_reasoning_get_state: invalid parameter");
        return -1;
    }
    *state_out = bridge->state;
    return 0;
}

int neuromod_reasoning_get_stats(const neuromod_reasoning_bridge_t* bridge,
                                  neuromod_reasoning_stats_t* stats_out) {
    if (!bridge || bridge->magic != NEUROMOD_REASONING_BRIDGE_MAGIC || !stats_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_reasoning_get_stats: invalid parameter");
        return -1;
    }
    *stats_out = bridge->stats;
    return 0;
}

int neuromod_reasoning_reset_stats(neuromod_reasoning_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_REASONING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_reasoning_reset_stats: invalid parameter");
        return -1;
    }
    memset(&bridge->stats, 0, sizeof(neuromod_reasoning_stats_t));
    bridge->cumulative_confidence = 0.0f;
    bridge->cumulative_accuracy = 0.0f;
    bridge->calibration_samples = 0;
    return 0;
}

/* ============================================================================
 * Diagnostics
 * ============================================================================ */

bool neuromod_reasoning_is_connected(const neuromod_reasoning_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_REASONING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_reasoning_is_connected: invalid parameter");
        return false;
    }
    return bridge->connected;
}

float neuromod_reasoning_get_quality(const neuromod_reasoning_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_REASONING_BRIDGE_MAGIC) return 0.0f;
    return bridge->state.reasoning_quality;
}

float neuromod_reasoning_get_coherence(const neuromod_reasoning_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_REASONING_BRIDGE_MAGIC) return 0.0f;
    return bridge->state.bridge_coherence;
}

void neuromod_reasoning_print_summary(const neuromod_reasoning_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_REASONING_BRIDGE_MAGIC) return;

    printf("=== Neuromod-Reasoning Bridge Summary ===\n");
    printf("State:\n");
    printf("  Confidence: %.2f\n", bridge->state.confidence_level);
    printf("  Curiosity: %.2f\n", bridge->state.curiosity_level);
    printf("  Cognitive Control: %.2f\n", bridge->state.cognitive_control);
    printf("  Alertness: %.2f\n", bridge->state.alertness_level);
    printf("  Deliberation: %.2f\n", bridge->state.deliberation_level);
    printf("  Patience: %.2f\n", bridge->state.patience_level);
    printf("  Error Sensitivity: %.2f\n", bridge->state.error_sensitivity);
    printf("  Mode: %s\n", neuromod_reasoning_mode_name(bridge->state.current_mode));
    printf("  Reasoning Quality: %.2f\n", bridge->state.reasoning_quality);
    printf("  Metacognitive Awareness: %.2f\n", bridge->state.metacognitive_awareness);
    printf("  Coherence: %.2f\n", bridge->state.bridge_coherence);
    printf("Stats:\n");
    printf("  Successful Reasoning: %u\n", bridge->stats.successful_reasoning);
    printf("  Reasoning Errors: %u\n", bridge->stats.reasoning_errors);
    printf("  Mode Switches: %u\n", bridge->stats.mode_switches);
    printf("  Confidence Calibration: %.2f\n", bridge->stats.confidence_accuracy_corr);
    printf("  Total Updates: %lu\n", (unsigned long)bridge->stats.total_updates);
}
