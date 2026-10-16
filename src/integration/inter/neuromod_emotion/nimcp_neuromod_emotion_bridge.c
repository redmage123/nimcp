/**
 * @file nimcp_neuromod_emotion_bridge.c
 * @brief Neuromodulatory-Emotion Inter-Layer Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-12
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "integration/inter/neuromod_emotion/nimcp_neuromod_emotion_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neuromod_emotion_bridge)

#define LOG_MODULE "NEUROMOD_EMOTION_BRIDGE"


/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct neuromod_emotion_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    uint32_t magic;
    neuromod_emotion_config_t config;
    neuromod_emotion_state_t state;
    neuromod_emotion_stats_t stats;
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

neuromod_emotion_config_t neuromod_emotion_default_config(void) {
    neuromod_emotion_config_t config = {
        /* NE-Arousal coupling */
        .ne_arousal_coupling = NE_AROUSAL_COUPLING,
        .ne_amygdala_potentiation = AMYGDALA_NE_POTENTIATION,
        .arousal_anxiety_threshold = AROUSAL_ANXIETY_THRESHOLD,

        /* DA-Valence coupling */
        .da_valence_coupling = DA_VALENCE_COUPLING,
        .da_motivation_coupling = 0.5f,
        .valence_anhedonia_threshold = VALENCE_ANHEDONIA_THRESHOLD,

        /* 5-HT-Regulation coupling */
        .ht_regulation_coupling = HT_REGULATION_COUPLING,
        .ht_anxiety_reduction = 0.4f,
        .regulation_dysreg_threshold = REGULATION_DYSREGULATION_THRESH,

        /* Habenula-Aversion coupling */
        .hab_aversion_coupling = HAB_AVERSION_COUPLING,
        .hab_learned_helplessness = 0.3f,

        /* Top-down feedback */
        .fear_lc_trigger_gain = 0.7f,
        .reward_vta_trigger_gain = 0.6f,
        .conflict_ht_demand_gain = 0.5f,
        .disappoint_hab_trigger_gain = 0.6f,

        /* Timing */
        .update_interval_ms = 10,

        /* Features */
        .enable_arousal_modulation = true,
        .enable_valence_modulation = true,
        .enable_regulation_modulation = true,
        .enable_aversion_modulation = true,
        .enable_state_classification = true,
        .enable_logging = false
    };
    return config;
}

neuromod_emotion_bridge_t* neuromod_emotion_create(const neuromod_emotion_config_t* config) {
    neuromod_emotion_bridge_t* bridge = nimcp_calloc(1, sizeof(neuromod_emotion_bridge_t));
    NIMCP_API_CHECK_ALLOC(bridge, "Failed to allocate neuromod-emotion bridge");

    bridge->magic = NEUROMOD_EMOTION_BRIDGE_MAGIC;

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = neuromod_emotion_default_config();
    }

    /* Initialize state to neutral */
    bridge->state.arousal_level = 0.5f;
    bridge->state.valence_level = 0.0f;  /* Neutral valence */
    bridge->state.regulation_capacity = 0.5f;
    bridge->state.aversion_level = 0.0f;
    bridge->state.anxiety_level = 0.0f;
    bridge->state.motivation_level = 0.5f;
    bridge->state.current_state = EMOTION_STATE_NEUTRAL;
    bridge->state.emotional_stability = 1.0f;
    bridge->state.bridge_coherence = 1.0f;
    bridge->state.last_update_us = get_timestamp_us();

    bridge->connected = true;

    NIMCP_LOGGING_INFO("Created %s bridge", "neuromod_emotion");
    return bridge;
}

void neuromod_emotion_destroy(neuromod_emotion_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_EMOTION_BRIDGE_MAGIC) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "neuromod_emotion");
    bridge->magic = 0;
    nimcp_free(bridge);
}

/* ============================================================================
 * Bottom-Up Modulation (Neuromod -> Emotion)
 * ============================================================================ */

int neuromod_emotion_apply_ne_arousal(neuromod_emotion_bridge_t* bridge,
                                       float ne_level, float* arousal_out) {
    if (!bridge || bridge->magic != NEUROMOD_EMOTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_emotion_apply_ne_arousal: invalid parameter");
        return -1;
    }
    if (!bridge->config.enable_arousal_modulation) {
        if (arousal_out) *arousal_out = 0.5f;
        return 0;
    }

    ne_level = nimcp_clampf(ne_level, 0.0f, 1.0f);
    bridge->state.ne_level = ne_level;

    /* NE directly drives arousal with some baseline */
    float base_arousal = 0.3f;
    bridge->state.arousal_level = base_arousal +
        (1.0f - base_arousal) * ne_level * bridge->config.ne_arousal_coupling;

    /* Update anxiety if arousal is high and valence is negative */
    if (bridge->state.arousal_level > bridge->config.arousal_anxiety_threshold &&
        bridge->state.valence_level < 0.0f) {
        bridge->state.anxiety_level = (bridge->state.arousal_level -
            bridge->config.arousal_anxiety_threshold) /
            (1.0f - bridge->config.arousal_anxiety_threshold);
        bridge->state.anxiety_level *= fabsf(bridge->state.valence_level);
        bridge->stats.anxiety_episodes++;
    } else {
        bridge->state.anxiety_level *= 0.9f;  /* Decay anxiety */
    }

    bridge->stats.arousal_modulations++;

    if (arousal_out) *arousal_out = bridge->state.arousal_level;
    return 0;
}

int neuromod_emotion_apply_da_valence(neuromod_emotion_bridge_t* bridge,
                                       float da_level, float* valence_out) {
    if (!bridge || bridge->magic != NEUROMOD_EMOTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_emotion_apply_da_valence: invalid parameter");
        return -1;
    }
    if (!bridge->config.enable_valence_modulation) {
        if (valence_out) *valence_out = 0.0f;
        return 0;
    }

    da_level = nimcp_clampf(da_level, 0.0f, 1.0f);
    bridge->state.da_level = da_level;

    /* DA shifts valence toward positive
     * Low DA = negative valence (anhedonia)
     * High DA = positive valence (pleasure/motivation) */
    float neutral_da = 0.4f;  /* DA level for neutral valence */
    bridge->state.valence_level = (da_level - neutral_da) * 2.0f * bridge->config.da_valence_coupling;
    bridge->state.valence_level = nimcp_clampf(bridge->state.valence_level, -1.0f, 1.0f);

    /* Update motivation based on DA */
    bridge->state.motivation_level = da_level * bridge->config.da_motivation_coupling;

    /* Check for anhedonia */
    if (da_level < bridge->config.valence_anhedonia_threshold) {
        bridge->stats.anhedonia_episodes++;
    }

    bridge->stats.valence_modulations++;

    if (valence_out) *valence_out = bridge->state.valence_level;
    return 0;
}

int neuromod_emotion_apply_ht_regulation(neuromod_emotion_bridge_t* bridge,
                                          float ht_level, float* regulation_out) {
    if (!bridge || bridge->magic != NEUROMOD_EMOTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_emotion_apply_ht_regulation: invalid parameter");
        return -1;
    }
    if (!bridge->config.enable_regulation_modulation) {
        if (regulation_out) *regulation_out = 0.5f;
        return 0;
    }

    ht_level = nimcp_clampf(ht_level, 0.0f, 1.0f);
    bridge->state.ht_level = ht_level;

    /* 5-HT enables emotional regulation */
    float base_regulation = 0.2f;
    bridge->state.regulation_capacity = base_regulation +
        (1.0f - base_regulation) * ht_level * bridge->config.ht_regulation_coupling;

    /* 5-HT reduces anxiety */
    bridge->state.anxiety_level *= (1.0f - ht_level * bridge->config.ht_anxiety_reduction);

    /* Check for dysregulation */
    if (bridge->state.regulation_capacity < bridge->config.regulation_dysreg_threshold) {
        bridge->stats.dysregulation_episodes++;
    }

    bridge->stats.regulation_modulations++;

    if (regulation_out) *regulation_out = bridge->state.regulation_capacity;
    return 0;
}

int neuromod_emotion_apply_hab_aversion(neuromod_emotion_bridge_t* bridge,
                                         float hab_level, float* aversion_out) {
    if (!bridge || bridge->magic != NEUROMOD_EMOTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_emotion_apply_hab_aversion: invalid parameter");
        return -1;
    }
    if (!bridge->config.enable_aversion_modulation) {
        if (aversion_out) *aversion_out = 0.0f;
        return 0;
    }

    hab_level = nimcp_clampf(hab_level, 0.0f, 1.0f);
    bridge->state.hab_level = hab_level;

    /* Habenula activity signals aversion/disappointment */
    bridge->state.aversion_level = hab_level * bridge->config.hab_aversion_coupling;

    /* High habenula activity shifts valence negative */
    if (hab_level > 0.5f) {
        float valence_shift = -(hab_level - 0.5f) * bridge->config.hab_aversion_coupling;
        bridge->state.valence_level = nimcp_clampf(
            bridge->state.valence_level + valence_shift,
            -1.0f, 1.0f
        );
    }

    if (hab_level > 0.6f) {
        bridge->stats.aversion_events++;
    }

    if (aversion_out) *aversion_out = bridge->state.aversion_level;
    return 0;
}

/* ============================================================================
 * Top-Down Feedback (Emotion -> Neuromod)
 * ============================================================================ */

int neuromod_emotion_report_fear(neuromod_emotion_bridge_t* bridge,
                                  float fear_intensity, float* lc_trigger_out) {
    if (!bridge || bridge->magic != NEUROMOD_EMOTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_emotion_report_fear: invalid parameter");
        return -1;
    }

    fear_intensity = nimcp_clampf(fear_intensity, 0.0f, 1.0f);
    bridge->state.fear_signal = fear_intensity;

    /* Fear triggers LC activation for arousal */
    float lc_trigger = fear_intensity * bridge->config.fear_lc_trigger_gain;

    if (fear_intensity > 0.5f) {
        bridge->stats.fear_triggers++;
    }

    bridge->stats.top_down_messages++;

    if (lc_trigger_out) *lc_trigger_out = lc_trigger;
    return 0;
}

int neuromod_emotion_report_reward_anticipation(neuromod_emotion_bridge_t* bridge,
                                                 float anticipation, float* vta_trigger_out) {
    if (!bridge || bridge->magic != NEUROMOD_EMOTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_emotion_report_reward_anticipation: invalid parameter");
        return -1;
    }

    anticipation = nimcp_clampf(anticipation, 0.0f, 1.0f);
    bridge->state.reward_anticipation = anticipation;

    /* Reward anticipation activates VTA */
    float vta_trigger = anticipation * bridge->config.reward_vta_trigger_gain;

    if (anticipation > 0.4f) {
        bridge->stats.reward_triggers++;
    }

    bridge->stats.top_down_messages++;

    if (vta_trigger_out) *vta_trigger_out = vta_trigger;
    return 0;
}

int neuromod_emotion_report_conflict(neuromod_emotion_bridge_t* bridge,
                                      float conflict_level, float* ht_demand_out) {
    if (!bridge || bridge->magic != NEUROMOD_EMOTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_emotion_report_conflict: invalid parameter");
        return -1;
    }

    conflict_level = nimcp_clampf(conflict_level, 0.0f, 1.0f);
    bridge->state.conflict_signal = conflict_level;

    /* Emotional conflict increases 5-HT demand for regulation */
    float ht_demand = conflict_level * bridge->config.conflict_ht_demand_gain;

    if (conflict_level > 0.5f) {
        bridge->stats.conflict_signals++;
    }

    bridge->stats.top_down_messages++;

    if (ht_demand_out) *ht_demand_out = ht_demand;
    return 0;
}

int neuromod_emotion_report_disappointment(neuromod_emotion_bridge_t* bridge,
                                            float disappointment, float* hab_trigger_out) {
    if (!bridge || bridge->magic != NEUROMOD_EMOTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_emotion_report_disappointment: invalid parameter");
        return -1;
    }

    disappointment = nimcp_clampf(disappointment, 0.0f, 1.0f);
    bridge->state.disappointment_signal = disappointment;

    /* Disappointment activates habenula */
    float hab_trigger = disappointment * bridge->config.disappoint_hab_trigger_gain;

    if (disappointment > 0.5f) {
        bridge->stats.disappointment_signals++;
    }

    bridge->stats.top_down_messages++;

    if (hab_trigger_out) *hab_trigger_out = hab_trigger;
    return 0;
}

/* ============================================================================
 * State Classification
 * ============================================================================ */

emotional_state_t neuromod_emotion_classify_state(const neuromod_emotion_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_EMOTION_BRIDGE_MAGIC) {
        return EMOTION_STATE_NEUTRAL;
    }

    float arousal = bridge->state.arousal_level;
    float valence = bridge->state.valence_level;
    float regulation = bridge->state.regulation_capacity;

    /* Check for dysregulation first */
    if (regulation < bridge->config.regulation_dysreg_threshold) {
        return EMOTION_STATE_DYSREGULATED;
    }

    /* Check for anxiety (high arousal + negative valence) */
    if (arousal > bridge->config.arousal_anxiety_threshold && valence < -0.2f) {
        return EMOTION_STATE_ANXIOUS;
    }

    /* Check for anhedonia (low arousal + low/negative valence) */
    if (arousal < 0.3f && valence < 0.0f) {
        return EMOTION_STATE_ANHEDONIC;
    }

    /* Classify based on arousal and valence quadrant */
    if (valence > 0.2f) {
        /* Positive valence */
        if (arousal > 0.6f) {
            return EMOTION_STATE_POSITIVE_HIGH;  /* Excited, euphoric */
        } else {
            return EMOTION_STATE_POSITIVE_LOW;   /* Content, calm */
        }
    } else if (valence < -0.2f) {
        /* Negative valence */
        if (arousal > 0.6f) {
            return EMOTION_STATE_NEGATIVE_HIGH;  /* Angry, fearful */
        } else {
            return EMOTION_STATE_NEGATIVE_LOW;   /* Sad, melancholic */
        }
    }

    return EMOTION_STATE_NEUTRAL;
}

const char* neuromod_emotion_state_name(emotional_state_t state) {
    switch (state) {
        case EMOTION_STATE_NEUTRAL:       return "Neutral";
        case EMOTION_STATE_POSITIVE_LOW:  return "Content";
        case EMOTION_STATE_POSITIVE_HIGH: return "Excited";
        case EMOTION_STATE_NEGATIVE_LOW:  return "Sad";
        case EMOTION_STATE_NEGATIVE_HIGH: return "Fearful/Angry";
        case EMOTION_STATE_ANXIOUS:       return "Anxious";
        case EMOTION_STATE_ANHEDONIC:     return "Anhedonic";
        case EMOTION_STATE_DYSREGULATED:  return "Dysregulated";
        default:                          return "Unknown";
    }
}

/* ============================================================================
 * Unified Modulation
 * ============================================================================ */

int neuromod_emotion_compute_modulation(neuromod_emotion_bridge_t* bridge,
                                        float ne_level, float da_level,
                                        float ht_level, float hab_level,
                                        neuromod_emotion_state_t* state_out) {
    if (!bridge || bridge->magic != NEUROMOD_EMOTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_emotion_compute_modulation: invalid parameter");
        return -1;
    }

    /* Apply all modulations */
    neuromod_emotion_apply_ne_arousal(bridge, ne_level, NULL);
    neuromod_emotion_apply_da_valence(bridge, da_level, NULL);
    neuromod_emotion_apply_ht_regulation(bridge, ht_level, NULL);
    neuromod_emotion_apply_hab_aversion(bridge, hab_level, NULL);

    /* Classify emotional state */
    if (bridge->config.enable_state_classification) {
        bridge->state.current_state = neuromod_emotion_classify_state(bridge);
    }

    /* Compute emotional stability
     * High stability = good regulation + moderate arousal + neutral-to-positive valence */
    float arousal_stability = 1.0f - fabsf(bridge->state.arousal_level - 0.5f);
    float valence_stability = 1.0f - fabsf(bridge->state.valence_level) * 0.5f;
    bridge->state.emotional_stability =
        0.4f * bridge->state.regulation_capacity +
        0.3f * arousal_stability +
        0.3f * valence_stability;

    /* Compute bridge coherence */
    float coherence = 1.0f;
    /* Reduce coherence if regulation is low but arousal is high */
    if (bridge->state.regulation_capacity < 0.4f && bridge->state.arousal_level > 0.7f) {
        coherence -= 0.3f;
    }
    /* Reduce coherence if valence and aversion conflict */
    if (bridge->state.valence_level > 0.3f && bridge->state.aversion_level > 0.5f) {
        coherence -= 0.2f;
    }
    bridge->state.bridge_coherence = nimcp_clampf(coherence, 0.0f, 1.0f);

    bridge->state.last_update_us = get_timestamp_us();
    bridge->stats.bottom_up_messages += 4;
    bridge->stats.total_updates++;

    /* Update running averages */
    float alpha = 0.1f;
    bridge->stats.avg_arousal = alpha * bridge->state.arousal_level +
                                 (1.0f - alpha) * bridge->stats.avg_arousal;
    bridge->stats.avg_valence = alpha * bridge->state.valence_level +
                                 (1.0f - alpha) * bridge->stats.avg_valence;
    bridge->stats.avg_stability = alpha * bridge->state.emotional_stability +
                                   (1.0f - alpha) * bridge->stats.avg_stability;

    if (state_out) *state_out = bridge->state;
    return 0;
}

/* ============================================================================
 * Update and State
 * ============================================================================ */

int neuromod_emotion_update(neuromod_emotion_bridge_t* bridge, float delta_ms) {
    if (!bridge || bridge->magic != NEUROMOD_EMOTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_emotion_update: invalid parameter");
        return -1;
    }

    /* Decay signals over time */
    float decay = expf(-delta_ms / 200.0f);  /* 200ms time constant for emotions */
    bridge->state.fear_signal *= decay;
    bridge->state.reward_anticipation *= decay;
    bridge->state.conflict_signal *= decay;
    bridge->state.disappointment_signal *= decay;

    /* Arousal and anxiety decay more slowly */
    float slow_decay = expf(-delta_ms / 500.0f);
    bridge->state.anxiety_level *= slow_decay;

    /* Valence drifts toward neutral */
    float valence_drift = 0.001f * delta_ms / 10.0f;
    if (bridge->state.valence_level > 0.0f) {
        bridge->state.valence_level = fmaxf(bridge->state.valence_level - valence_drift, 0.0f);
    } else if (bridge->state.valence_level < 0.0f) {
        bridge->state.valence_level = fminf(bridge->state.valence_level + valence_drift, 0.0f);
    }

    bridge->state.last_update_us = get_timestamp_us();
    return 0;
}

int neuromod_emotion_get_state(const neuromod_emotion_bridge_t* bridge,
                                neuromod_emotion_state_t* state_out) {
    if (!bridge || bridge->magic != NEUROMOD_EMOTION_BRIDGE_MAGIC || !state_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_emotion_get_state: invalid parameter");
        return -1;
    }
    *state_out = bridge->state;
    return 0;
}

int neuromod_emotion_get_stats(const neuromod_emotion_bridge_t* bridge,
                                neuromod_emotion_stats_t* stats_out) {
    if (!bridge || bridge->magic != NEUROMOD_EMOTION_BRIDGE_MAGIC || !stats_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_emotion_get_stats: invalid parameter");
        return -1;
    }
    *stats_out = bridge->stats;
    return 0;
}

int neuromod_emotion_reset_stats(neuromod_emotion_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_EMOTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_emotion_reset_stats: invalid parameter");
        return -1;
    }
    memset(&bridge->stats, 0, sizeof(neuromod_emotion_stats_t));
    return 0;
}

/* ============================================================================
 * Diagnostics
 * ============================================================================ */

bool neuromod_emotion_is_connected(const neuromod_emotion_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_EMOTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_emotion_is_connected: invalid parameter");
        return false;
    }
    return bridge->connected;
}

float neuromod_emotion_get_stability(const neuromod_emotion_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_EMOTION_BRIDGE_MAGIC) return 0.0f;
    return bridge->state.emotional_stability;
}

float neuromod_emotion_get_coherence(const neuromod_emotion_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_EMOTION_BRIDGE_MAGIC) return 0.0f;
    return bridge->state.bridge_coherence;
}

void neuromod_emotion_print_summary(const neuromod_emotion_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_EMOTION_BRIDGE_MAGIC) return;

    printf("=== Neuromod-Emotion Bridge Summary ===\n");
    printf("State:\n");
    printf("  Arousal: %.2f\n", bridge->state.arousal_level);
    printf("  Valence: %.2f\n", bridge->state.valence_level);
    printf("  Regulation: %.2f\n", bridge->state.regulation_capacity);
    printf("  Aversion: %.2f\n", bridge->state.aversion_level);
    printf("  Anxiety: %.2f\n", bridge->state.anxiety_level);
    printf("  Emotional State: %s\n", neuromod_emotion_state_name(bridge->state.current_state));
    printf("  Stability: %.2f\n", bridge->state.emotional_stability);
    printf("  Coherence: %.2f\n", bridge->state.bridge_coherence);
    printf("Stats:\n");
    printf("  Anxiety Episodes: %u\n", bridge->stats.anxiety_episodes);
    printf("  Anhedonia Episodes: %u\n", bridge->stats.anhedonia_episodes);
    printf("  Dysregulation Episodes: %u\n", bridge->stats.dysregulation_episodes);
    printf("  Total Updates: %lu\n", (unsigned long)bridge->stats.total_updates);
}
