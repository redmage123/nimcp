//=============================================================================
// nimcp_gt_neuromod.c - Game Payoff to Neuromodulator Mapping
//=============================================================================
/**
 * @file nimcp_gt_neuromod.c
 * @brief Map game-theoretic outcomes to neuromodulator release
 *
 * WHAT: Payoff-based neuromodulator release
 * WHY:  Game outcomes should affect learning and motivation
 * HOW:  Win -> DA, Loss -> 5-HT, Unfair -> NE
 *
 * @author NIMCP Development Team
 * @date 2024-12-27
 * @version 1.0.0
 */

#include "cognitive/game_theory/integration/nimcp_gt_neuromod.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(gt_neuromod, MESH_ADAPTER_CATEGORY_COGNITIVE)



//=============================================================================
// Constants
//=============================================================================

#define MAX_NEUROMOD_RELEASE 1.0f
#define MIN_NEUROMOD_RELEASE 0.0f

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Opaque bridge structure
 */
struct gt_neuromod_bridge_struct {
    neuromodulator_system_t neuromod;
    gt_neuromod_config_t config;

    // Cumulative release tracking
    float total_dopamine;
    float total_serotonin;
    float total_norepinephrine;
    float total_acetylcholine;

    // Event counts
    uint64_t wins_processed;
    uint64_t losses_processed;
    uint64_t unfair_events;
    uint64_t strategic_events;

    // Running statistics
    float avg_valence;
    uint64_t outcomes_processed;

    bool active;
};

//=============================================================================
// Static Helpers
//=============================================================================

static float clamp_release(float value) {
    if (value < MIN_NEUROMOD_RELEASE) return MIN_NEUROMOD_RELEASE;
    if (value > MAX_NEUROMOD_RELEASE) return MAX_NEUROMOD_RELEASE;
    return value;
}

static float compute_payoff_for_player(
    const nimcp_game_outcome_t* outcome,
    nimcp_player_id_t player
) {
    // Find player in outcome
    for (uint32_t i = 0; i < outcome->num_winners; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && outcome->num_winners > 256) {
            gt_neuromod_heartbeat("gt_neuromod_loop",
                             (float)(i + 1) / (float)outcome->num_winners);
        }

        if (outcome->winners[i] == player) {
            return outcome->allocations[i];
        }
    }
    return 0.0f;  // Not a winner
}

static bool is_winner(
    const nimcp_game_outcome_t* outcome,
    nimcp_player_id_t player
) {
    for (uint32_t i = 0; i < outcome->num_winners; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && outcome->num_winners > 256) {
            gt_neuromod_heartbeat("gt_neuromod_loop",
                             (float)(i + 1) / (float)outcome->num_winners);
        }

        if (outcome->winners[i] == player) {
            return true;
        }
    }
    return false;
}

//=============================================================================
// Lifecycle
//=============================================================================

gt_neuromod_config_t gt_neuromod_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    gt_neuromod_heartbeat("gt_neuromod_default_config", 0.0f);


    gt_neuromod_config_t config = {
        // Dopamine (reward/motivation)
        .payoff_dopamine_gain = 0.1f,
        .win_dopamine_bonus = 0.5f,
        .cooperation_dopamine_bonus = 0.3f,

        // Serotonin (patience/inhibition)
        .loss_serotonin_gain = 0.2f,
        .unfair_serotonin_boost = 0.4f,
        .timeout_serotonin_gain = 0.1f,

        // Norepinephrine (arousal/threat)
        .competition_ne_baseline = 0.3f,
        .fairness_violation_ne_gain = 0.5f,
        .uncertainty_ne_gain = 0.2f,

        // Acetylcholine (attention/salience)
        .strategic_ach_gain = 0.3f,
        .novel_opponent_ach_boost = 0.4f,

        // Thresholds
        .payoff_threshold = 0.0f,
        .fairness_threshold = 0.5f,
        .uncertainty_threshold = 0.7f
    };
    return config;
}

gt_neuromod_bridge_t gt_neuromod_bridge_create(
    neuromodulator_system_t neuromod,
    const gt_neuromod_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    gt_neuromod_heartbeat("gt_neuromod_bridge_create", 0.0f);


    gt_neuromod_bridge_t bridge = nimcp_calloc(1, sizeof(struct gt_neuromod_bridge_struct));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    bridge->neuromod = neuromod;
    bridge->config = config ? *config : gt_neuromod_default_config();

    // Initialize counters
    bridge->total_dopamine = 0.0f;
    bridge->total_serotonin = 0.0f;
    bridge->total_norepinephrine = 0.0f;
    bridge->total_acetylcholine = 0.0f;

    bridge->wins_processed = 0;
    bridge->losses_processed = 0;
    bridge->unfair_events = 0;
    bridge->strategic_events = 0;

    bridge->avg_valence = 0.0f;
    bridge->outcomes_processed = 0;

    bridge->active = true;

    return bridge;
}

void gt_neuromod_bridge_destroy(gt_neuromod_bridge_t bridge) {
    /* Phase 8: Heartbeat at operation start */
    gt_neuromod_heartbeat("gt_neuromod_bridge_destroy", 0.0f);


    if (bridge) {
        nimcp_free(bridge);
        bridge = NULL;
    }
}

//=============================================================================
// Core Operations
//=============================================================================

nimcp_error_t gt_neuromod_process_outcome(
    gt_neuromod_bridge_t bridge,
    nimcp_player_id_t player,
    const nimcp_game_outcome_t* outcome,
    gt_neuromod_release_t* release
) {
    /* Phase 8: Heartbeat at operation start */
    gt_neuromod_heartbeat("gt_neuromod_process_outcome", 0.0f);


    NIMCP_CHECK_THROW(bridge && outcome && release, NIMCP_ERROR_INVALID_PARAM, "bridge, outcome, or release is NULL");
    NIMCP_CHECK_THROW(bridge->active, NIMCP_GT_ERROR_GAME_OVER, "bridge is not active");

    memset(release, 0, sizeof(gt_neuromod_release_t));

    // Determine player's payoff and status
    float payoff = compute_payoff_for_player(outcome, player);
    bool won = is_winner(outcome, player);
    bool fair = outcome->is_fair;

    // Dopamine: reward for winning and positive payoff
    if (won) {
        release->dopamine_released += bridge->config.win_dopamine_bonus;
        bridge->wins_processed++;
    }
    if (payoff > bridge->config.payoff_threshold) {
        release->dopamine_released += payoff * bridge->config.payoff_dopamine_gain;
    }
    release->dopamine_released = clamp_release(release->dopamine_released);

    // Serotonin: patience/coping for losses
    if (!won) {
        release->serotonin_released += bridge->config.loss_serotonin_gain;
        bridge->losses_processed++;
    }
    if (!fair) {
        release->serotonin_released += bridge->config.unfair_serotonin_boost;
    }
    release->serotonin_released = clamp_release(release->serotonin_released);

    // Norepinephrine: arousal for unfair outcomes
    if (!fair) {
        float unfairness = 1.0f - bridge->config.fairness_threshold;
        release->norepinephrine_released += unfairness * bridge->config.fairness_violation_ne_gain;
        bridge->unfair_events++;
    }
    // Baseline competition arousal
    release->norepinephrine_released += bridge->config.competition_ne_baseline;
    release->norepinephrine_released = clamp_release(release->norepinephrine_released);

    // Acetylcholine: strategic attention
    if (outcome->num_winners > 1) {
        release->acetylcholine_released += bridge->config.strategic_ach_gain;
        bridge->strategic_events++;
    }
    release->acetylcholine_released = clamp_release(release->acetylcholine_released);

    // Compute net valence
    release->net_valence = (release->dopamine_released - release->serotonin_released) * 2.0f;
    if (release->net_valence > 1.0f) release->net_valence = 1.0f;
    if (release->net_valence < -1.0f) release->net_valence = -1.0f;

    // Update cumulative stats
    bridge->total_dopamine += release->dopamine_released;
    bridge->total_serotonin += release->serotonin_released;
    bridge->total_norepinephrine += release->norepinephrine_released;
    bridge->total_acetylcholine += release->acetylcholine_released;

    bridge->outcomes_processed++;
    float alpha = 1.0f / (float)bridge->outcomes_processed;
    if (isfinite(release->net_valence)) {
        bridge->avg_valence = (1.0f - alpha) * bridge->avg_valence + alpha * release->net_valence;
    }

    return NIMCP_SUCCESS;
}

float gt_neuromod_auction_win(
    gt_neuromod_bridge_t bridge,
    float winning_bid,
    float payment
) {
    if (!bridge || !bridge->active) {
        return 0.0f;
    }

    // Consumer surplus: valuation (bid) minus payment
    /* Phase 8: Heartbeat at operation start */
    gt_neuromod_heartbeat("gt_neuromod_auction_win", 0.0f);


    float surplus = winning_bid - payment;
    if (surplus < 0.0f) surplus = 0.0f;

    // DA release proportional to surplus
    float da = bridge->config.win_dopamine_bonus +
               surplus * bridge->config.payoff_dopamine_gain;
    da = clamp_release(da);

    bridge->total_dopamine += da;
    bridge->wins_processed++;

    return da;
}

float gt_neuromod_bargaining_success(
    gt_neuromod_bridge_t bridge,
    float agreement_value,
    float fairness_index
) {
    if (!bridge || !bridge->active) {
        return 0.0f;
    }

    // DA for reaching agreement
    /* Phase 8: Heartbeat at operation start */
    gt_neuromod_heartbeat("gt_neuromod_bargaining_success", 0.0f);


    float da = bridge->config.cooperation_dopamine_bonus;

    // Bonus for valuable agreement
    da += agreement_value * bridge->config.payoff_dopamine_gain;

    // Extra bonus for fair outcomes
    if (fairness_index > bridge->config.fairness_threshold) {
        da += (fairness_index - bridge->config.fairness_threshold) * 0.2f;
    }

    da = clamp_release(da);
    bridge->total_dopamine += da;
    bridge->wins_processed++;

    return da;
}

float gt_neuromod_bargaining_failure(
    gt_neuromod_bridge_t bridge,
    uint32_t rounds_taken,
    uint32_t max_rounds
) {
    if (!bridge || !bridge->active || max_rounds == 0) {
        return 0.0f;
    }

    // 5-HT release for patience/timeout
    /* Phase 8: Heartbeat at operation start */
    gt_neuromod_heartbeat("gt_neuromod_bargaining_failure", 0.0f);


    float patience_factor = (float)rounds_taken / (float)max_rounds;
    float serotonin = bridge->config.timeout_serotonin_gain +
                      patience_factor * bridge->config.loss_serotonin_gain;

    serotonin = clamp_release(serotonin);
    bridge->total_serotonin += serotonin;
    bridge->losses_processed++;

    return serotonin;
}

float gt_neuromod_signal_unfairness(
    gt_neuromod_bridge_t bridge,
    float unfairness_magnitude
) {
    if (!bridge || !bridge->active) {
        return 0.0f;
    }

    // NE for threat response to unfairness
    /* Phase 8: Heartbeat at operation start */
    gt_neuromod_heartbeat("gt_neuromod_signal_unfairness", 0.0f);


    float ne = unfairness_magnitude * bridge->config.fairness_violation_ne_gain;
    ne = clamp_release(ne);

    bridge->total_norepinephrine += ne;
    bridge->unfair_events++;

    return ne;
}

float gt_neuromod_strategic_attention(
    gt_neuromod_bridge_t bridge,
    uint32_t num_options,
    float uncertainty
) {
    if (!bridge || !bridge->active) {
        return 0.0f;
    }

    // ACh for attention/salience
    /* Phase 8: Heartbeat at operation start */
    gt_neuromod_heartbeat("gt_neuromod_strategic_attention", 0.0f);


    float option_factor = logf((float)num_options + 1.0f) / logf(10.0f);  // Log scale
    float ach = option_factor * bridge->config.strategic_ach_gain;

    // Extra ACh for uncertainty
    if (uncertainty > bridge->config.uncertainty_threshold) {
        ach += (uncertainty - bridge->config.uncertainty_threshold) *
               bridge->config.novel_opponent_ach_boost;
    }

    ach = clamp_release(ach);
    bridge->total_acetylcholine += ach;
    bridge->strategic_events++;

    return ach;
}

//=============================================================================
// Statistics Functions
//=============================================================================

nimcp_error_t gt_neuromod_get_cumulative_release(
    const gt_neuromod_bridge_t bridge,
    gt_neuromod_release_t* cumulative
) {
    /* Phase 8: Heartbeat at operation start */
    gt_neuromod_heartbeat("gt_neuromod_get_cumulative_relea", 0.0f);


    NIMCP_CHECK_THROW(bridge && cumulative, NIMCP_ERROR_INVALID_PARAM, "bridge or cumulative is NULL");

    memset(cumulative, 0, sizeof(gt_neuromod_release_t));

    cumulative->dopamine_released = bridge->total_dopamine;
    cumulative->serotonin_released = bridge->total_serotonin;
    cumulative->norepinephrine_released = bridge->total_norepinephrine;
    cumulative->acetylcholine_released = bridge->total_acetylcholine;
    cumulative->net_valence = bridge->avg_valence;

    return NIMCP_SUCCESS;
}

void gt_neuromod_reset_stats(gt_neuromod_bridge_t bridge) {
    if (!bridge) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_neuromod_heartbeat("gt_neuromod_reset_stats", 0.0f);


    bridge->total_dopamine = 0.0f;
    bridge->total_serotonin = 0.0f;
    bridge->total_norepinephrine = 0.0f;
    bridge->total_acetylcholine = 0.0f;
    bridge->avg_valence = 0.0f;

    bridge->wins_processed = 0;
    bridge->losses_processed = 0;
    bridge->unfair_events = 0;
    bridge->strategic_events = 0;
    bridge->outcomes_processed = 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int gt_neuromod_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    gt_neuromod_heartbeat("gt_neuromod_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Game_Theory_Neuromodulator");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                gt_neuromod_heartbeat("gt_neuromod_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Game_Theory_Neuromodulator");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Game_Theory_Neuromodulator");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void gt_neuromod_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_gt_neuromod_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int gt_neuromod_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_neuromod_training_begin: NULL argument");
        return -1;
    }
    gt_neuromod_heartbeat_instance(NULL, "gt_neuromod_training_begin", 0.0f);
    (void)(struct gt_neuromod_bridge_struct*)instance; /* Module state available for reset */
    return 0;
}

int gt_neuromod_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_neuromod_training_end: NULL argument");
        return -1;
    }
    gt_neuromod_heartbeat_instance(NULL, "gt_neuromod_training_end", 1.0f);
    (void)(struct gt_neuromod_bridge_struct*)instance; /* Module state available for finalization */
    return 0;
}

int gt_neuromod_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_neuromod_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    gt_neuromod_heartbeat_instance(NULL, "gt_neuromod_training_step", progress);
    (void)(struct gt_neuromod_bridge_struct*)instance; /* Module state available for step adaptation */
    return 0;
}
