/**
 * @file nimcp_neuromod_gametheory_bridge.c
 * @brief Neuromodulatory-Game Theory Inter-Layer Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-12
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "integration/inter/neuromod_gametheory/nimcp_neuromod_gametheory_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neuromod_gametheory_bridge)

#define LOG_MODULE "NEUROMOD_GAMETHEORY_BRIDGE"


/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct neuromod_gametheory_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    uint32_t magic;
    neuromod_gametheory_config_t config;
    neuromod_gametheory_state_t state;
    neuromod_gametheory_stats_t stats;
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

neuromod_gametheory_config_t neuromod_gametheory_default_config(void) {
    neuromod_gametheory_config_t config = {
        /* 5-HT-Cooperation coupling */
        .ht_cooperation_coupling = HT_COOPERATION_COUPLING,
        .ht_fairness_sensitivity = 0.5f,
        .ht_trust_coupling = 0.4f,

        /* DA-Competition coupling */
        .da_competition_coupling = DA_COMPETITION_COUPLING,
        .da_risk_coupling = DA_RISK_COUPLING,
        .da_dominance_coupling = 0.4f,

        /* NE-Urgency coupling */
        .ne_urgency_coupling = NE_URGENCY_COUPLING,
        .ne_exploration_coupling = 0.5f,

        /* Habenula-Loss coupling */
        .hab_loss_aversion_coupling = HAB_LOSS_AVERSION_COUPLING,
        .hab_withdrawal_coupling = 0.5f,

        /* Fair offer parameters */
        .fair_offer_threshold = FAIR_OFFER_THRESHOLD,
        .unfair_rejection_gain = 0.6f,

        /* Top-down feedback */
        .fairness_ht_trigger_gain = 0.5f,
        .winning_vta_trigger_gain = 0.6f,
        .pressure_lc_trigger_gain = 0.6f,
        .losing_hab_trigger_gain = 0.5f,

        /* Timing */
        .update_interval_ms = 10,

        /* Features */
        .enable_cooperation_modulation = true,
        .enable_competition_modulation = true,
        .enable_urgency_modulation = true,
        .enable_loss_aversion = true,
        .enable_strategy_classification = true,
        .enable_logging = false
    };
    return config;
}

neuromod_gametheory_bridge_t* neuromod_gametheory_create(const neuromod_gametheory_config_t* config) {
    neuromod_gametheory_bridge_t* bridge = nimcp_calloc(1, sizeof(neuromod_gametheory_bridge_t));
    NIMCP_API_CHECK_ALLOC(bridge, "Failed to allocate neuromod-gametheory bridge");

    bridge->magic = NEUROMOD_GAMETHEORY_BRIDGE_MAGIC;

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = neuromod_gametheory_default_config();
    }

    /* Initialize state to balanced */
    bridge->state.cooperation_tendency = COOPERATION_DEFAULT;
    bridge->state.competition_tendency = COOPERATION_DEFAULT;
    bridge->state.risk_tolerance = RISK_NEUTRAL;
    bridge->state.loss_aversion = 0.5f;
    bridge->state.decision_urgency = 0.5f;
    bridge->state.trust_level = 0.5f;
    bridge->state.fairness_threshold = bridge->config.fair_offer_threshold;
    bridge->state.current_strategy = GT_STRATEGY_BALANCED;
    bridge->state.strategic_coherence = 1.0f;
    bridge->state.bridge_coherence = 1.0f;
    bridge->state.last_update_us = get_timestamp_us();

    bridge->connected = true;

    NIMCP_LOGGING_INFO("Created %s bridge", "neuromod_gametheory");
    return bridge;
}

void neuromod_gametheory_destroy(neuromod_gametheory_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_GAMETHEORY_BRIDGE_MAGIC) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "neuromod_gametheory");
    bridge->magic = 0;
    nimcp_free(bridge);
}

/* ============================================================================
 * Bottom-Up Modulation (Neuromod -> Game Theory)
 * ============================================================================ */

int neuromod_gametheory_apply_ht_cooperation(neuromod_gametheory_bridge_t* bridge,
                                              float ht_level, float* coop_out) {
    if (!bridge || bridge->magic != NEUROMOD_GAMETHEORY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_gametheory_apply_ht_cooperation: invalid parameter");
        return -1;
    }
    if (!bridge->config.enable_cooperation_modulation) {
        if (coop_out) *coop_out = COOPERATION_DEFAULT;
        return 0;
    }

    ht_level = nimcp_clampf(ht_level, 0.0f, 1.0f);
    bridge->state.ht_level = ht_level;

    /* High 5-HT promotes cooperation */
    float base_coop = 0.3f;
    bridge->state.cooperation_tendency = base_coop +
        (1.0f - base_coop) * ht_level * bridge->config.ht_cooperation_coupling;

    /* 5-HT also increases fairness sensitivity and trust */
    bridge->state.fairness_threshold = bridge->config.fair_offer_threshold +
        (0.6f - bridge->config.fair_offer_threshold) * ht_level * bridge->config.ht_fairness_sensitivity;

    bridge->state.trust_level = 0.3f +
        0.7f * ht_level * bridge->config.ht_trust_coupling;

    bridge->stats.cooperation_modulations++;

    if (coop_out) *coop_out = bridge->state.cooperation_tendency;
    return 0;
}

int neuromod_gametheory_apply_da_competition(neuromod_gametheory_bridge_t* bridge,
                                              float da_level, float* comp_out) {
    if (!bridge || bridge->magic != NEUROMOD_GAMETHEORY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_gametheory_apply_da_competition: invalid parameter");
        return -1;
    }
    if (!bridge->config.enable_competition_modulation) {
        if (comp_out) *comp_out = 0.5f;
        return 0;
    }

    da_level = nimcp_clampf(da_level, 0.0f, 1.0f);
    bridge->state.da_level = da_level;

    /* High DA promotes competition and dominance */
    float base_comp = 0.2f;
    bridge->state.competition_tendency = base_comp +
        (1.0f - base_comp) * da_level * bridge->config.da_competition_coupling;

    bridge->stats.competition_modulations++;

    if (comp_out) *comp_out = bridge->state.competition_tendency;
    return 0;
}

int neuromod_gametheory_apply_da_risk(neuromod_gametheory_bridge_t* bridge,
                                       float da_level, float* risk_out) {
    if (!bridge || bridge->magic != NEUROMOD_GAMETHEORY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_gametheory_apply_da_risk: invalid parameter");
        return -1;
    }

    da_level = nimcp_clampf(da_level, 0.0f, 1.0f);

    /* High DA increases risk tolerance */
    float base_risk = 0.3f;
    bridge->state.risk_tolerance = base_risk +
        (1.0f - base_risk) * da_level * bridge->config.da_risk_coupling;

    if (risk_out) *risk_out = bridge->state.risk_tolerance;
    return 0;
}

int neuromod_gametheory_apply_ne_urgency(neuromod_gametheory_bridge_t* bridge,
                                          float ne_level, float* urgency_out) {
    if (!bridge || bridge->magic != NEUROMOD_GAMETHEORY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_gametheory_apply_ne_urgency: invalid parameter");
        return -1;
    }
    if (!bridge->config.enable_urgency_modulation) {
        if (urgency_out) *urgency_out = 0.5f;
        return 0;
    }

    ne_level = nimcp_clampf(ne_level, 0.0f, 1.0f);
    bridge->state.ne_level = ne_level;

    /* High NE increases decision urgency */
    float base_urgency = 0.2f;
    bridge->state.decision_urgency = base_urgency +
        (1.0f - base_urgency) * ne_level * bridge->config.ne_urgency_coupling;

    bridge->stats.urgency_modulations++;

    if (urgency_out) *urgency_out = bridge->state.decision_urgency;
    return 0;
}

int neuromod_gametheory_apply_hab_loss_aversion(neuromod_gametheory_bridge_t* bridge,
                                                 float hab_level, float* aversion_out) {
    if (!bridge || bridge->magic != NEUROMOD_GAMETHEORY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_gametheory_apply_hab_loss_aversion: invalid parameter");
        return -1;
    }
    if (!bridge->config.enable_loss_aversion) {
        if (aversion_out) *aversion_out = 0.5f;
        return 0;
    }

    hab_level = nimcp_clampf(hab_level, 0.0f, 1.0f);
    bridge->state.hab_level = hab_level;

    /* Habenula activity increases loss aversion */
    float base_aversion = 0.3f;
    bridge->state.loss_aversion = base_aversion +
        (1.0f - base_aversion) * hab_level * bridge->config.hab_loss_aversion_coupling;

    if (hab_level > 0.5f) {
        bridge->stats.loss_aversion_events++;
    }

    if (aversion_out) *aversion_out = bridge->state.loss_aversion;
    return 0;
}

/* ============================================================================
 * Top-Down Feedback (Game Theory -> Neuromod)
 * ============================================================================ */

int neuromod_gametheory_report_fair_treatment(neuromod_gametheory_bridge_t* bridge,
                                               float fairness, float* ht_trigger_out) {
    if (!bridge || bridge->magic != NEUROMOD_GAMETHEORY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_gametheory_report_fair_treatment: invalid parameter");
        return -1;
    }

    fairness = nimcp_clampf(fairness, 0.0f, 1.0f);
    bridge->state.fair_treatment_signal = fairness;

    /* Fair treatment triggers 5-HT satisfaction */
    float ht_trigger = fairness * bridge->config.fairness_ht_trigger_gain;

    if (fairness > bridge->state.fairness_threshold) {
        bridge->stats.fair_offers_received++;
    } else {
        bridge->stats.unfair_offers_rejected++;
    }

    bridge->stats.top_down_messages++;

    if (ht_trigger_out) *ht_trigger_out = ht_trigger;
    return 0;
}

int neuromod_gametheory_report_game_won(neuromod_gametheory_bridge_t* bridge,
                                         float win_magnitude, float* vta_trigger_out) {
    if (!bridge || bridge->magic != NEUROMOD_GAMETHEORY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_gametheory_report_game_won: invalid parameter");
        return -1;
    }

    win_magnitude = nimcp_clampf(win_magnitude, 0.0f, 1.0f);
    bridge->state.winning_signal = win_magnitude;

    /* Winning triggers VTA reward */
    float vta_trigger = win_magnitude * bridge->config.winning_vta_trigger_gain;

    if (win_magnitude > 0.3f) {
        bridge->stats.games_won++;
    }

    bridge->stats.top_down_messages++;

    if (vta_trigger_out) *vta_trigger_out = vta_trigger;
    return 0;
}

int neuromod_gametheory_report_game_lost(neuromod_gametheory_bridge_t* bridge,
                                          float loss_magnitude, float* hab_trigger_out) {
    if (!bridge || bridge->magic != NEUROMOD_GAMETHEORY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_gametheory_report_game_lost: invalid parameter");
        return -1;
    }

    loss_magnitude = nimcp_clampf(loss_magnitude, 0.0f, 1.0f);
    bridge->state.losing_signal = loss_magnitude;

    /* Losing triggers habenula */
    float hab_trigger = loss_magnitude * bridge->config.losing_hab_trigger_gain;

    if (loss_magnitude > 0.3f) {
        bridge->stats.games_lost++;
    }

    bridge->stats.top_down_messages++;

    if (hab_trigger_out) *hab_trigger_out = hab_trigger;
    return 0;
}

int neuromod_gametheory_report_betrayal(neuromod_gametheory_bridge_t* bridge,
                                         float severity, float* hab_trigger_out) {
    if (!bridge || bridge->magic != NEUROMOD_GAMETHEORY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_gametheory_report_betrayal: invalid parameter");
        return -1;
    }

    severity = nimcp_clampf(severity, 0.0f, 1.0f);
    bridge->state.betrayal_signal = severity;

    /* Betrayal strongly activates habenula and reduces trust */
    float hab_trigger = severity * bridge->config.losing_hab_trigger_gain * 1.5f;
    hab_trigger = nimcp_clampf(hab_trigger, 0.0f, 1.0f);

    /* Reduce trust */
    bridge->state.trust_level *= (1.0f - severity * 0.3f);

    if (severity > 0.4f) {
        bridge->stats.betrayals_detected++;
    }

    bridge->stats.top_down_messages++;

    if (hab_trigger_out) *hab_trigger_out = hab_trigger;
    return 0;
}

int neuromod_gametheory_report_time_pressure(neuromod_gametheory_bridge_t* bridge,
                                              float pressure, float* lc_trigger_out) {
    if (!bridge || bridge->magic != NEUROMOD_GAMETHEORY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_gametheory_report_time_pressure: invalid parameter");
        return -1;
    }

    pressure = nimcp_clampf(pressure, 0.0f, 1.0f);

    /* Time pressure triggers LC */
    float lc_trigger = pressure * bridge->config.pressure_lc_trigger_gain;

    bridge->stats.top_down_messages++;

    if (lc_trigger_out) *lc_trigger_out = lc_trigger;
    return 0;
}

/* ============================================================================
 * Decision Support
 * ============================================================================ */

float neuromod_gametheory_evaluate_offer(neuromod_gametheory_bridge_t* bridge, float offer_value) {
    if (!bridge || bridge->magic != NEUROMOD_GAMETHEORY_BRIDGE_MAGIC) return 0.5f;

    offer_value = nimcp_clampf(offer_value, 0.0f, 1.0f);

    /* Evaluate offer based on fairness threshold and loss aversion */
    float threshold = bridge->state.fairness_threshold;

    if (offer_value >= threshold) {
        /* Fair offer - acceptance probability based on cooperation */
        return 0.5f + 0.5f * bridge->state.cooperation_tendency;
    } else {
        /* Unfair offer - rejection strength based on 5-HT */
        float rejection = (threshold - offer_value) / threshold;
        rejection *= bridge->config.unfair_rejection_gain;
        rejection *= bridge->state.ht_level;  /* Higher 5-HT = stronger rejection */
        return 0.5f - 0.5f * rejection;
    }
}

bool neuromod_gametheory_should_cooperate(neuromod_gametheory_bridge_t* bridge,
                                           float opponent_history) {
    if (!bridge || bridge->magic != NEUROMOD_GAMETHEORY_BRIDGE_MAGIC) return true;

    opponent_history = nimcp_clampf(opponent_history, 0.0f, 1.0f);  /* 1.0 = always cooperated */

    /* Decision based on:
     * - Own cooperation tendency
     * - Opponent's history
     * - Trust level */
    float coop_score = bridge->state.cooperation_tendency * 0.4f +
                       opponent_history * 0.4f +
                       bridge->state.trust_level * 0.2f;

    /* High competition tendency reduces cooperation */
    coop_score -= bridge->state.competition_tendency * 0.3f;

    if (coop_score > 0.5f) {
        bridge->stats.cooperative_choices++;
        return true;
    } else {
        bridge->stats.competitive_choices++;
        return false;  /* Non-cooperative choice - normal game theory outcome */
    }
}

bool neuromod_gametheory_should_take_risk(neuromod_gametheory_bridge_t* bridge,
                                           float expected_value, float variance) {
    if (!bridge || bridge->magic != NEUROMOD_GAMETHEORY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_gametheory_should_take_risk: invalid parameter");
        return false;
    }

    expected_value = nimcp_clampf(expected_value, -1.0f, 1.0f);
    variance = nimcp_clampf(variance, 0.0f, 1.0f);

    /* Risk-adjusted value based on risk tolerance and loss aversion */
    float risk_premium = variance * (1.0f - bridge->state.risk_tolerance);
    float loss_penalty = 0.0f;

    /* Loss aversion makes negative outcomes worse */
    if (expected_value < 0.0f) {
        loss_penalty = fabsf(expected_value) * bridge->state.loss_aversion;
    }

    float adjusted_value = expected_value - risk_premium - loss_penalty;

    return adjusted_value > 0.0f;
}

/* ============================================================================
 * Strategy Classification
 * ============================================================================ */

gt_strategy_t neuromod_gametheory_classify_strategy(const neuromod_gametheory_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_GAMETHEORY_BRIDGE_MAGIC) {
        return GT_STRATEGY_BALANCED;
    }

    float coop = bridge->state.cooperation_tendency;
    float comp = bridge->state.competition_tendency;
    float risk = bridge->state.risk_tolerance;
    float aversion = bridge->state.loss_aversion;
    float urgency = bridge->state.decision_urgency;

    /* Check for urgent strategy first
     * Note: urgency max ~0.56 with default coupling, so threshold 0.55 */
    if (urgency > 0.55f) {
        return GT_STRATEGY_URGENT;
    }

    /* Check for cautious strategy (high loss aversion)
     * Note: aversion max ~0.6 with default coupling, threshold 0.55 */
    if (aversion > 0.55f && risk < 0.45f) {
        return GT_STRATEGY_CAUTIOUS;
    }

    /* Check for aggressive strategy (high risk, low aversion) */
    if (risk > 0.55f && aversion < 0.45f) {
        return GT_STRATEGY_AGGRESSIVE;
    }

    /* Check for cooperative vs competitive
     * Lower margin to 0.15 to allow distinction with typical coupling values */
    if (coop > comp + 0.15f) {
        return GT_STRATEGY_COOPERATIVE;
    } else if (comp > coop + 0.15f) {
        return GT_STRATEGY_COMPETITIVE;
    }

    return GT_STRATEGY_BALANCED;
}

const char* neuromod_gametheory_strategy_name(gt_strategy_t strategy) {
    switch (strategy) {
        case GT_STRATEGY_BALANCED:     return "Balanced";
        case GT_STRATEGY_COOPERATIVE:  return "Cooperative";
        case GT_STRATEGY_COMPETITIVE:  return "Competitive";
        case GT_STRATEGY_CAUTIOUS:     return "Cautious";
        case GT_STRATEGY_AGGRESSIVE:   return "Aggressive";
        case GT_STRATEGY_URGENT:       return "Urgent";
        default:                       return "Unknown";
    }
}

/* ============================================================================
 * Unified Modulation
 * ============================================================================ */

int neuromod_gametheory_compute_modulation(neuromod_gametheory_bridge_t* bridge,
                                           float ht_level, float da_level,
                                           float ne_level, float hab_level,
                                           neuromod_gametheory_state_t* state_out) {
    if (!bridge || bridge->magic != NEUROMOD_GAMETHEORY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_gametheory_compute_modulation: invalid parameter");
        return -1;
    }

    /* Apply all modulations */
    neuromod_gametheory_apply_ht_cooperation(bridge, ht_level, NULL);
    neuromod_gametheory_apply_da_competition(bridge, da_level, NULL);
    neuromod_gametheory_apply_da_risk(bridge, da_level, NULL);
    neuromod_gametheory_apply_ne_urgency(bridge, ne_level, NULL);
    neuromod_gametheory_apply_hab_loss_aversion(bridge, hab_level, NULL);

    /* Classify strategy */
    gt_strategy_t new_strategy = neuromod_gametheory_classify_strategy(bridge);
    if (new_strategy != bridge->state.current_strategy) {
        bridge->stats.strategy_shifts++;
        bridge->state.current_strategy = new_strategy;
    }

    /* Compute strategic coherence
     * High coherence = consistent strategy parameters */
    float coop_comp_conflict = fabsf(bridge->state.cooperation_tendency -
                                     (1.0f - bridge->state.competition_tendency));
    float risk_aversion_conflict = fabsf(bridge->state.risk_tolerance -
                                         (1.0f - bridge->state.loss_aversion));
    bridge->state.strategic_coherence = 1.0f - 0.5f * (coop_comp_conflict + risk_aversion_conflict);
    bridge->state.strategic_coherence = nimcp_clampf(bridge->state.strategic_coherence, 0.0f, 1.0f);

    /* Compute bridge coherence */
    bridge->state.bridge_coherence = bridge->state.strategic_coherence;

    bridge->state.last_update_us = get_timestamp_us();
    bridge->stats.bottom_up_messages += 5;
    bridge->stats.total_updates++;

    /* Update running averages */
    float alpha = 0.1f;
    bridge->stats.avg_cooperation = alpha * bridge->state.cooperation_tendency +
                                     (1.0f - alpha) * bridge->stats.avg_cooperation;
    bridge->stats.avg_risk_tolerance = alpha * bridge->state.risk_tolerance +
                                        (1.0f - alpha) * bridge->stats.avg_risk_tolerance;
    bridge->stats.avg_loss_aversion = alpha * bridge->state.loss_aversion +
                                       (1.0f - alpha) * bridge->stats.avg_loss_aversion;

    if (state_out) *state_out = bridge->state;
    return 0;
}

/* ============================================================================
 * Update and State
 * ============================================================================ */

int neuromod_gametheory_update(neuromod_gametheory_bridge_t* bridge, float delta_ms) {
    if (!bridge || bridge->magic != NEUROMOD_GAMETHEORY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_gametheory_update: invalid parameter");
        return -1;
    }

    /* Decay outcome signals */
    float decay = expf(-delta_ms / 150.0f);
    bridge->state.fair_treatment_signal *= decay;
    bridge->state.winning_signal *= decay;
    bridge->state.losing_signal *= decay;
    bridge->state.betrayal_signal *= decay;

    /* Trust slowly recovers */
    float trust_recovery = 0.001f * delta_ms / 10.0f;
    bridge->state.trust_level = fminf(bridge->state.trust_level + trust_recovery, 0.7f);

    /* Urgency decays if no time pressure */
    float urgency_decay = 0.01f * delta_ms / 10.0f;
    bridge->state.decision_urgency = fmaxf(bridge->state.decision_urgency - urgency_decay, 0.3f);

    bridge->state.last_update_us = get_timestamp_us();
    return 0;
}

int neuromod_gametheory_get_state(const neuromod_gametheory_bridge_t* bridge,
                                   neuromod_gametheory_state_t* state_out) {
    if (!bridge || bridge->magic != NEUROMOD_GAMETHEORY_BRIDGE_MAGIC || !state_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_gametheory_get_state: invalid parameter");
        return -1;
    }
    *state_out = bridge->state;
    return 0;
}

int neuromod_gametheory_get_stats(const neuromod_gametheory_bridge_t* bridge,
                                   neuromod_gametheory_stats_t* stats_out) {
    if (!bridge || bridge->magic != NEUROMOD_GAMETHEORY_BRIDGE_MAGIC || !stats_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_gametheory_get_stats: invalid parameter");
        return -1;
    }
    *stats_out = bridge->stats;
    return 0;
}

int neuromod_gametheory_reset_stats(neuromod_gametheory_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_GAMETHEORY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_gametheory_reset_stats: invalid parameter");
        return -1;
    }
    memset(&bridge->stats, 0, sizeof(neuromod_gametheory_stats_t));
    return 0;
}

/* ============================================================================
 * Diagnostics
 * ============================================================================ */

bool neuromod_gametheory_is_connected(const neuromod_gametheory_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_GAMETHEORY_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "neuromod_gametheory_is_connected: invalid parameter");
        return false;
    }
    return bridge->connected;
}

float neuromod_gametheory_get_coherence(const neuromod_gametheory_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_GAMETHEORY_BRIDGE_MAGIC) return 0.0f;
    return bridge->state.bridge_coherence;
}

void neuromod_gametheory_print_summary(const neuromod_gametheory_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_GAMETHEORY_BRIDGE_MAGIC) return;

    printf("=== Neuromod-Game Theory Bridge Summary ===\n");
    printf("State:\n");
    printf("  Cooperation: %.2f\n", bridge->state.cooperation_tendency);
    printf("  Competition: %.2f\n", bridge->state.competition_tendency);
    printf("  Risk Tolerance: %.2f\n", bridge->state.risk_tolerance);
    printf("  Loss Aversion: %.2f\n", bridge->state.loss_aversion);
    printf("  Decision Urgency: %.2f\n", bridge->state.decision_urgency);
    printf("  Trust Level: %.2f\n", bridge->state.trust_level);
    printf("  Strategy: %s\n", neuromod_gametheory_strategy_name(bridge->state.current_strategy));
    printf("  Strategic Coherence: %.2f\n", bridge->state.strategic_coherence);
    printf("Stats:\n");
    printf("  Cooperative Choices: %u\n", bridge->stats.cooperative_choices);
    printf("  Competitive Choices: %u\n", bridge->stats.competitive_choices);
    printf("  Games Won: %u\n", bridge->stats.games_won);
    printf("  Games Lost: %u\n", bridge->stats.games_lost);
    printf("  Betrayals Detected: %u\n", bridge->stats.betrayals_detected);
    printf("  Strategy Shifts: %u\n", bridge->stats.strategy_shifts);
    printf("  Total Updates: %lu\n", (unsigned long)bridge->stats.total_updates);
}
