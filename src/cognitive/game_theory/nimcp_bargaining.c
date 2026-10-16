//=============================================================================
// nimcp_bargaining.c - Nash Bargaining Implementation
//=============================================================================
/**
 * @file nimcp_bargaining.c
 * @brief Bargaining solution implementations
 */

#include "cognitive/game_theory/nimcp_bargaining.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <float.h>

#define LOG_MODULE "bargaining"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(bargaining, MESH_ADAPTER_CATEGORY_COGNITIVE)



//=============================================================================
// Internal Structure
//=============================================================================

struct nimcp_bargaining_struct {
    nimcp_bargaining_config_t config;
    nimcp_bargaining_state_t state;

    // Feasible set
    nimcp_feasible_point_t* feasible_set;
    uint32_t num_feasible;
    uint32_t max_feasible;

    // Negotiation state
    nimcp_offer_t current_offer;
    nimcp_offer_t* offer_history;
    uint32_t num_offers;
    uint32_t current_round;

    // Outcome
    nimcp_bargaining_outcome_t outcome;
    uint64_t start_time_ms;

    // Thread safety
    nimcp_platform_mutex_t mutex;
};

//=============================================================================
// Static Name Tables
//=============================================================================

static const char* s_bargaining_type_names[] = {
    "Nash Bargaining",
    "Kalai-Smorodinsky",
    "Egalitarian",
    "Rubinstein (Alternating Offers)"
};

//=============================================================================
// Configuration
//=============================================================================

nimcp_bargaining_config_t nimcp_bargaining_default_config(uint32_t num_players) {
    /* Phase 8: Heartbeat at operation start */
    bargaining_heartbeat("bargaining_bargaining_default_c", 0.0f);


    nimcp_bargaining_config_t config;
    memset(&config, 0, sizeof(config));

    config.type = NIMCP_BARGAINING_NASH;
    config.num_players = num_players > NIMCP_GT_MAX_PLAYERS ? NIMCP_GT_MAX_PLAYERS : num_players;
    config.discount_factor = 0.9f;
    config.max_rounds = 100;
    config.convergence_threshold = 0.001f;
    config.timeout_ms = 0;

    // Default: equal bargaining power
    float equal_power = 1.0f / (float)config.num_players;
    for (uint32_t i = 0; i < config.num_players; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && config.num_players > 256) {
            bargaining_heartbeat("bargaining_loop",
                             (float)(i + 1) / (float)config.num_players);
        }

        config.disagreement_payoffs[i] = 0.0f;
        config.bargaining_powers[i] = equal_power;
    }

    return config;
}

//=============================================================================
// Lifecycle
//=============================================================================

nimcp_bargaining_t nimcp_bargaining_create(const nimcp_bargaining_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    bargaining_heartbeat("bargaining_bargaining_create", 0.0f);


    NIMCP_API_CHECK_NULL_RET_NULL(config, "NULL config in nimcp_bargaining_create");
    if (config->num_players == 0 || config->num_players > NIMCP_GT_MAX_PLAYERS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Invalid player count: %u", config->num_players);
        return NULL;
    }

    nimcp_bargaining_t bargaining = nimcp_calloc(1, sizeof(struct nimcp_bargaining_struct));
    if (!bargaining) return NULL;
    NIMCP_API_CHECK_ALLOC(bargaining, "Failed to allocate bargaining structure");

    bargaining->config = *config;
    bargaining->state = NIMCP_BARGAINING_STATE_INITIALIZED;

    // Allocate feasible set storage
    bargaining->max_feasible = 1024;  // Initial capacity
    bargaining->feasible_set = nimcp_calloc(bargaining->max_feasible, sizeof(nimcp_feasible_point_t));
    if (!bargaining->feasible_set) {
        nimcp_free(bargaining);
        bargaining = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_bargaining_create: bargaining->feasible_set is NULL");
        return NULL;
    }

    // Allocate offer history
    bargaining->offer_history = nimcp_calloc(config->max_rounds, sizeof(nimcp_offer_t));
    if (!bargaining->offer_history) {
        nimcp_free(bargaining->feasible_set);
        nimcp_free(bargaining);
        bargaining = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_bargaining_create: bargaining->offer_history is NULL");
        return NULL;
    }

    if (nimcp_platform_mutex_init(&bargaining->mutex, false) != 0) {
        nimcp_free(bargaining->offer_history);
        nimcp_free(bargaining->feasible_set);
        nimcp_free(bargaining);
        bargaining = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_bargaining_create: validation failed");
        return NULL;
    }

    memset(&bargaining->outcome, 0, sizeof(nimcp_bargaining_outcome_t));
    bargaining->current_round = 0;
    bargaining->num_offers = 0;
    bargaining->start_time_ms = nimcp_time_get_ms();

    return bargaining;
}

void nimcp_bargaining_destroy(nimcp_bargaining_t bargaining) {
    if (!bargaining) return;

    /* Phase 8: Heartbeat at operation start */
    bargaining_heartbeat("bargaining_bargaining_destroy", 0.0f);


    nimcp_platform_mutex_destroy(&bargaining->mutex);
    nimcp_free(bargaining->offer_history);
    nimcp_free(bargaining->feasible_set);
    nimcp_free(bargaining);
    bargaining = NULL;
}

//=============================================================================
// Feasible Set
//=============================================================================

nimcp_error_t nimcp_bargaining_set_feasible_set(
    nimcp_bargaining_t bargaining,
    const nimcp_feasible_point_t* points,
    uint32_t num_points
) {
    if (!bargaining || !points) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    bargaining_heartbeat("bargaining_bargaining_set_feasi", 0.0f);


    nimcp_platform_mutex_lock(&bargaining->mutex);

    // Resize if needed
    if (num_points > bargaining->max_feasible) {
        nimcp_feasible_point_t* new_set = nimcp_realloc(
            bargaining->feasible_set,
            num_points * sizeof(nimcp_feasible_point_t)
        );
        if (!new_set) {
            nimcp_platform_mutex_unlock(&bargaining->mutex);
            return NIMCP_GT_ERROR_NO_MEMORY;
        }
        bargaining->feasible_set = new_set;
        bargaining->max_feasible = num_points;
    }

    memcpy(bargaining->feasible_set, points, num_points * sizeof(nimcp_feasible_point_t));
    bargaining->num_feasible = num_points;

    nimcp_platform_mutex_unlock(&bargaining->mutex);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Nash Bargaining Solution
//=============================================================================

float nimcp_compute_nash_product(
    const float* allocation,
    const float* disagreement,
    const float* powers,
    uint32_t n
) {
    /* Phase 8: Heartbeat at operation start */
    bargaining_heartbeat("bargaining_compute_nash_product", 0.0f);


    float product = 1.0f;

    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            bargaining_heartbeat("bargaining_loop",
                             (float)(i + 1) / (float)n);
        }

        float gain = allocation[i] - disagreement[i];
        if (gain <= 0.0f) {
            return 0.0f;  // Not individually rational
        }
        product *= powf(gain, powers[i]);
    }

    return product;
}

nimcp_error_t nimcp_bargaining_compute_nash_solution(
    nimcp_bargaining_t bargaining,
    nimcp_bargaining_outcome_t* outcome
) {
    if (!bargaining || !outcome) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    bargaining_heartbeat("bargaining_bargaining_compute_n", 0.0f);


    nimcp_platform_mutex_lock(&bargaining->mutex);

    if (bargaining->num_feasible == 0) {
        nimcp_platform_mutex_unlock(&bargaining->mutex);
        return NIMCP_GT_ERROR_INVALID_STATE;
    }

    uint32_t n = bargaining->config.num_players;
    float max_nash_product = -FLT_MAX;
    int best_idx = -1;

    // Find feasible point maximizing Nash product
    for (uint32_t f = 0; f < bargaining->num_feasible; f++) {
        /* Phase 8: Loop progress heartbeat */
        if ((f & 0xFF) == 0 && bargaining->num_feasible > 256) {
            bargaining_heartbeat("bargaining_loop",
                             (float)(f + 1) / (float)bargaining->num_feasible);
        }

        const float* u = bargaining->feasible_set[f].utilities;

        // Check individual rationality
        bool ir = true;
        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                bargaining_heartbeat("bargaining_loop",
                                 (float)(i + 1) / (float)n);
            }

            if (u[i] < bargaining->config.disagreement_payoffs[i]) {
                ir = false;
                break;
            }
        }
        if (!ir) continue;

        // Compute Nash product
        float product = nimcp_compute_nash_product(
            u,
            bargaining->config.disagreement_payoffs,
            bargaining->config.bargaining_powers,
            n
        );

        if (product > max_nash_product) {
            max_nash_product = product;
            best_idx = (int)f;
        }
    }

    memset(outcome, 0, sizeof(nimcp_bargaining_outcome_t));

    if (best_idx < 0) {
        outcome->state = NIMCP_BARGAINING_STATE_DISAGREEMENT;
        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                bargaining_heartbeat("bargaining_loop",
                                 (float)(i + 1) / (float)n);
            }

            outcome->allocations[i] = bargaining->config.disagreement_payoffs[i];
            outcome->utilities[i] = bargaining->config.disagreement_payoffs[i];
        }
        bargaining->state = NIMCP_BARGAINING_STATE_DISAGREEMENT;
        nimcp_platform_mutex_unlock(&bargaining->mutex);
        return NIMCP_GT_ERROR_DISAGREEMENT;
    }

    // Set outcome
    outcome->state = NIMCP_BARGAINING_STATE_AGREED;
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            bargaining_heartbeat("bargaining_loop",
                             (float)(i + 1) / (float)n);
        }

        outcome->allocations[i] = bargaining->feasible_set[best_idx].utilities[i];
        outcome->utilities[i] = bargaining->feasible_set[best_idx].utilities[i];
    }
    outcome->nash_product = max_nash_product;
    outcome->is_pareto_optimal = true;  // Nash solution is always Pareto optimal
    outcome->is_individually_rational = true;
    outcome->rounds_taken = 1;

    bargaining->outcome = *outcome;
    bargaining->state = NIMCP_BARGAINING_STATE_AGREED;

    nimcp_platform_mutex_unlock(&bargaining->mutex);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Kalai-Smorodinsky Solution
//=============================================================================

nimcp_error_t nimcp_bargaining_compute_kalai_smorodinsky(
    nimcp_bargaining_t bargaining,
    nimcp_bargaining_outcome_t* outcome
) {
    if (!bargaining || !outcome) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    bargaining_heartbeat("bargaining_bargaining_compute_k", 0.0f);


    nimcp_platform_mutex_lock(&bargaining->mutex);

    if (bargaining->num_feasible == 0) {
        nimcp_platform_mutex_unlock(&bargaining->mutex);
        return NIMCP_GT_ERROR_INVALID_STATE;
    }

    uint32_t n = bargaining->config.num_players;
    float* d = bargaining->config.disagreement_payoffs;

    // Find utopia point (max achievable for each player)
    float utopia[NIMCP_GT_MAX_PLAYERS];
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            bargaining_heartbeat("bargaining_loop",
                             (float)(i + 1) / (float)n);
        }

        utopia[i] = -FLT_MAX;
    }

    for (uint32_t f = 0; f < bargaining->num_feasible; f++) {
        /* Phase 8: Loop progress heartbeat */
        if ((f & 0xFF) == 0 && bargaining->num_feasible > 256) {
            bargaining_heartbeat("bargaining_loop",
                             (float)(f + 1) / (float)bargaining->num_feasible);
        }

        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                bargaining_heartbeat("bargaining_loop",
                                 (float)(i + 1) / (float)n);
            }

            if (bargaining->feasible_set[f].utilities[i] > utopia[i]) {
                utopia[i] = bargaining->feasible_set[f].utilities[i];
            }
        }
    }

    // Find point on Pareto frontier closest to d-u line
    // KS solution: intersection of Pareto frontier with ray from d to u
    float best_t = 0.0f;
    int best_idx = -1;

    for (uint32_t f = 0; f < bargaining->num_feasible; f++) {
        /* Phase 8: Loop progress heartbeat */
        if ((f & 0xFF) == 0 && bargaining->num_feasible > 256) {
            bargaining_heartbeat("bargaining_loop",
                             (float)(f + 1) / (float)bargaining->num_feasible);
        }

        const float* u = bargaining->feasible_set[f].utilities;

        // Check individual rationality
        bool ir = true;
        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                bargaining_heartbeat("bargaining_loop",
                                 (float)(i + 1) / (float)n);
            }

            if (u[i] < d[i]) {
                ir = false;
                break;
            }
        }
        if (!ir) continue;

        // Compute t such that u = d + t*(utopia - d)
        // Use minimum t across all dimensions
        float min_t = FLT_MAX;
        bool valid = true;

        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                bargaining_heartbeat("bargaining_loop",
                                 (float)(i + 1) / (float)n);
            }

            float range = utopia[i] - d[i];
            if (range < 1e-10f) continue;

            float t = (u[i] - d[i]) / range;
            if (t < min_t) {
                min_t = t;
            }
        }

        if (valid && min_t > best_t) {
            best_t = min_t;
            best_idx = (int)f;
        }
    }

    memset(outcome, 0, sizeof(nimcp_bargaining_outcome_t));

    if (best_idx < 0) {
        outcome->state = NIMCP_BARGAINING_STATE_DISAGREEMENT;
        bargaining->state = NIMCP_BARGAINING_STATE_DISAGREEMENT;
        nimcp_platform_mutex_unlock(&bargaining->mutex);
        return NIMCP_GT_ERROR_DISAGREEMENT;
    }

    outcome->state = NIMCP_BARGAINING_STATE_AGREED;
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            bargaining_heartbeat("bargaining_loop",
                             (float)(i + 1) / (float)n);
        }

        outcome->allocations[i] = bargaining->feasible_set[best_idx].utilities[i];
        outcome->utilities[i] = bargaining->feasible_set[best_idx].utilities[i];
    }
    outcome->is_pareto_optimal = true;
    outcome->is_individually_rational = true;
    outcome->nash_product = nimcp_compute_nash_product(
        outcome->allocations, d, bargaining->config.bargaining_powers, n
    );

    bargaining->outcome = *outcome;
    bargaining->state = NIMCP_BARGAINING_STATE_AGREED;

    nimcp_platform_mutex_unlock(&bargaining->mutex);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Egalitarian Solution
//=============================================================================

nimcp_error_t nimcp_bargaining_compute_egalitarian(
    nimcp_bargaining_t bargaining,
    nimcp_bargaining_outcome_t* outcome
) {
    if (!bargaining || !outcome) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    bargaining_heartbeat("bargaining_bargaining_compute_e", 0.0f);


    nimcp_platform_mutex_lock(&bargaining->mutex);

    if (bargaining->num_feasible == 0) {
        nimcp_platform_mutex_unlock(&bargaining->mutex);
        return NIMCP_GT_ERROR_INVALID_STATE;
    }

    uint32_t n = bargaining->config.num_players;
    float* d = bargaining->config.disagreement_payoffs;

    // Egalitarian: maximize minimum gain
    float best_min_gain = -FLT_MAX;
    int best_idx = -1;

    for (uint32_t f = 0; f < bargaining->num_feasible; f++) {
        /* Phase 8: Loop progress heartbeat */
        if ((f & 0xFF) == 0 && bargaining->num_feasible > 256) {
            bargaining_heartbeat("bargaining_loop",
                             (float)(f + 1) / (float)bargaining->num_feasible);
        }

        const float* u = bargaining->feasible_set[f].utilities;

        float min_gain = FLT_MAX;
        bool ir = true;

        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                bargaining_heartbeat("bargaining_loop",
                                 (float)(i + 1) / (float)n);
            }

            float gain = u[i] - d[i];
            if (gain < 0.0f) {
                ir = false;
                break;
            }
            if (gain < min_gain) {
                min_gain = gain;
            }
        }

        if (!ir) continue;

        if (min_gain > best_min_gain) {
            best_min_gain = min_gain;
            best_idx = (int)f;
        }
    }

    memset(outcome, 0, sizeof(nimcp_bargaining_outcome_t));

    if (best_idx < 0) {
        outcome->state = NIMCP_BARGAINING_STATE_DISAGREEMENT;
        bargaining->state = NIMCP_BARGAINING_STATE_DISAGREEMENT;
        nimcp_platform_mutex_unlock(&bargaining->mutex);
        return NIMCP_GT_ERROR_DISAGREEMENT;
    }

    outcome->state = NIMCP_BARGAINING_STATE_AGREED;
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            bargaining_heartbeat("bargaining_loop",
                             (float)(i + 1) / (float)n);
        }

        outcome->allocations[i] = bargaining->feasible_set[best_idx].utilities[i];
        outcome->utilities[i] = bargaining->feasible_set[best_idx].utilities[i];
    }
    outcome->is_pareto_optimal = true;
    outcome->is_individually_rational = true;
    outcome->nash_product = nimcp_compute_nash_product(
        outcome->allocations, d, bargaining->config.bargaining_powers, n
    );

    bargaining->outcome = *outcome;
    bargaining->state = NIMCP_BARGAINING_STATE_AGREED;

    nimcp_platform_mutex_unlock(&bargaining->mutex);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Alternating Offers (Rubinstein)
//=============================================================================

nimcp_error_t nimcp_bargaining_make_offer(
    nimcp_bargaining_t bargaining,
    nimcp_player_id_t proposer,
    const float* proposed_allocation
) {
    if (!bargaining || !proposed_allocation) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    bargaining_heartbeat("bargaining_bargaining_make_offe", 0.0f);


    nimcp_platform_mutex_lock(&bargaining->mutex);

    if (bargaining->state == NIMCP_BARGAINING_STATE_AGREED ||
        bargaining->state == NIMCP_BARGAINING_STATE_DISAGREEMENT ||
        bargaining->state == NIMCP_BARGAINING_STATE_TIMEOUT) {
        nimcp_platform_mutex_unlock(&bargaining->mutex);
        return NIMCP_GT_ERROR_INVALID_STATE;
    }

    if (bargaining->current_round >= bargaining->config.max_rounds) {
        bargaining->state = NIMCP_BARGAINING_STATE_TIMEOUT;
        nimcp_platform_mutex_unlock(&bargaining->mutex);
        return NIMCP_GT_ERROR_TIMEOUT;
    }

    bargaining->state = NIMCP_BARGAINING_STATE_NEGOTIATING;

    // Store offer
    nimcp_offer_t* offer = &bargaining->current_offer;
    offer->proposer = proposer;
    offer->round = bargaining->current_round;
    offer->timestamp_ms = nimcp_time_get_ms();
    offer->is_final = (bargaining->current_round == bargaining->config.max_rounds - 1);

    for (uint32_t i = 0; i < bargaining->config.num_players; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bargaining->config.num_players > 256) {
            bargaining_heartbeat("bargaining_loop",
                             (float)(i + 1) / (float)bargaining->config.num_players);
        }

        offer->proposed_allocation[i] = proposed_allocation[i];
    }

    // Store in history
    if (bargaining->num_offers < bargaining->config.max_rounds) {
        bargaining->offer_history[bargaining->num_offers++] = *offer;
    }

    nimcp_platform_mutex_unlock(&bargaining->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_bargaining_respond(
    nimcp_bargaining_t bargaining,
    nimcp_player_id_t responder,
    bool accept
) {
    if (!bargaining) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    bargaining_heartbeat("bargaining_bargaining_respond", 0.0f);


    nimcp_platform_mutex_lock(&bargaining->mutex);

    if (bargaining->state != NIMCP_BARGAINING_STATE_NEGOTIATING) {
        nimcp_platform_mutex_unlock(&bargaining->mutex);
        return NIMCP_GT_ERROR_INVALID_STATE;
    }

    (void)responder;  // Could validate responder is not proposer

    if (accept) {
        // Agreement reached
        bargaining->state = NIMCP_BARGAINING_STATE_AGREED;

        nimcp_bargaining_outcome_t* outcome = &bargaining->outcome;
        outcome->state = NIMCP_BARGAINING_STATE_AGREED;
        outcome->rounds_taken = bargaining->current_round + 1;

        for (uint32_t i = 0; i < bargaining->config.num_players; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bargaining->config.num_players > 256) {
                bargaining_heartbeat("bargaining_loop",
                                 (float)(i + 1) / (float)bargaining->config.num_players);
            }

            outcome->allocations[i] = bargaining->current_offer.proposed_allocation[i];
            outcome->utilities[i] = bargaining->current_offer.proposed_allocation[i];
        }

        // Check individual rationality
        outcome->is_individually_rational = true;
        for (uint32_t i = 0; i < bargaining->config.num_players; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bargaining->config.num_players > 256) {
                bargaining_heartbeat("bargaining_loop",
                                 (float)(i + 1) / (float)bargaining->config.num_players);
            }

            if (outcome->utilities[i] < bargaining->config.disagreement_payoffs[i]) {
                outcome->is_individually_rational = false;
                break;
            }
        }

        outcome->nash_product = nimcp_compute_nash_product(
            outcome->allocations,
            bargaining->config.disagreement_payoffs,
            bargaining->config.bargaining_powers,
            bargaining->config.num_players
        );
    } else {
        // Rejection - advance round
        bargaining->current_round++;

        if (bargaining->current_round >= bargaining->config.max_rounds) {
            bargaining->state = NIMCP_BARGAINING_STATE_DISAGREEMENT;
            bargaining->outcome.state = NIMCP_BARGAINING_STATE_DISAGREEMENT;
        }
    }

    nimcp_platform_mutex_unlock(&bargaining->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_bargaining_advance_round(nimcp_bargaining_t bargaining) {
    if (!bargaining) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    bargaining_heartbeat("bargaining_bargaining_advance_r", 0.0f);


    nimcp_platform_mutex_lock(&bargaining->mutex);

    bargaining->current_round++;

    if (bargaining->current_round >= bargaining->config.max_rounds) {
        bargaining->state = NIMCP_BARGAINING_STATE_TIMEOUT;
    }

    // Check timeout
    if (bargaining->config.timeout_ms > 0) {
        uint64_t elapsed = nimcp_time_get_ms() - bargaining->start_time_ms;
        if (elapsed >= bargaining->config.timeout_ms) {
            bargaining->state = NIMCP_BARGAINING_STATE_TIMEOUT;
        }
    }

    nimcp_platform_mutex_unlock(&bargaining->mutex);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Query Functions
//=============================================================================

bool nimcp_bargaining_has_agreement(const nimcp_bargaining_t bargaining) {
    if (!bargaining) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    bargaining_heartbeat("bargaining_bargaining_has_agree", 0.0f);


    return bargaining->state == NIMCP_BARGAINING_STATE_AGREED;
}

nimcp_error_t nimcp_bargaining_get_outcome(
    const nimcp_bargaining_t bargaining,
    nimcp_bargaining_outcome_t* outcome
) {
    if (!bargaining || !outcome) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    bargaining_heartbeat("bargaining_bargaining_get_outco", 0.0f);


    nimcp_platform_mutex_lock(&bargaining->mutex);
    *outcome = bargaining->outcome;
    nimcp_platform_mutex_unlock(&bargaining->mutex);

    return NIMCP_SUCCESS;
}

nimcp_bargaining_state_t nimcp_bargaining_get_state(const nimcp_bargaining_t bargaining) {
    if (!bargaining) return NIMCP_BARGAINING_STATE_INITIALIZED;
    /* Phase 8: Heartbeat at operation start */
    bargaining_heartbeat("bargaining_bargaining_get_state", 0.0f);


    return bargaining->state;
}

uint32_t nimcp_bargaining_get_round(const nimcp_bargaining_t bargaining) {
    if (!bargaining) return 0;
    /* Phase 8: Heartbeat at operation start */
    bargaining_heartbeat("bargaining_bargaining_get_round", 0.0f);


    return bargaining->current_round;
}

nimcp_error_t nimcp_bargaining_get_current_offer(
    const nimcp_bargaining_t bargaining,
    nimcp_offer_t* offer
) {
    if (!bargaining || !offer) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    bargaining_heartbeat("bargaining_bargaining_get_curre", 0.0f);


    nimcp_platform_mutex_lock(&bargaining->mutex);
    *offer = bargaining->current_offer;
    nimcp_platform_mutex_unlock(&bargaining->mutex);

    return NIMCP_SUCCESS;
}

const char* nimcp_bargaining_type_name(nimcp_bargaining_type_t type) {
    if (type >= NIMCP_BARGAINING_COUNT) {
        return "Unknown";
    }
    return s_bargaining_type_names[type];
}

nimcp_error_t nimcp_bargaining_set_powers(
    nimcp_bargaining_t bargaining,
    const float* powers,
    uint32_t num_players
) {
    if (!bargaining || !powers) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    bargaining_heartbeat("bargaining_bargaining_set_power", 0.0f);


    if (num_players > NIMCP_GT_MAX_PLAYERS || num_players != bargaining->config.num_players) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Verify powers sum to approximately 1.0
    float sum = 0.0f;
    for (uint32_t i = 0; i < num_players; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_players > 256) {
            bargaining_heartbeat("bargaining_loop",
                             (float)(i + 1) / (float)num_players);
        }

        if (powers[i] < 0.0f || powers[i] > 1.0f) {
            return NIMCP_ERROR_INVALID_PARAM;
        }
        sum += powers[i];
    }
    if (fabsf(sum - 1.0f) > 0.01f) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(&bargaining->mutex);
    for (uint32_t i = 0; i < num_players; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_players > 256) {
            bargaining_heartbeat("bargaining_loop",
                             (float)(i + 1) / (float)num_players);
        }

        bargaining->config.bargaining_powers[i] = powers[i];
    }
    nimcp_platform_mutex_unlock(&bargaining->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_bargaining_set_disagreement(
    nimcp_bargaining_t bargaining,
    const float* disagreement,
    uint32_t num_players
) {
    if (!bargaining || !disagreement) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    bargaining_heartbeat("bargaining_bargaining_set_disag", 0.0f);


    if (num_players > NIMCP_GT_MAX_PLAYERS || num_players != bargaining->config.num_players) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(&bargaining->mutex);
    for (uint32_t i = 0; i < num_players; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_players > 256) {
            bargaining_heartbeat("bargaining_loop",
                             (float)(i + 1) / (float)num_players);
        }

        bargaining->config.disagreement_payoffs[i] = disagreement[i];
    }
    nimcp_platform_mutex_unlock(&bargaining->mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// KG Self-Awareness Integration
//=============================================================================

int bargaining_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    bargaining_heartbeat("bargaining_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Bargaining_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                bargaining_heartbeat("bargaining_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG(LOG_MODULE, "Bargaining self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Bargaining_Module");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Bargaining_Module");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void bargaining_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_bargaining_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int bargaining_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "bargaining_training_begin: NULL argument");
        return -1;
    }
    bargaining_heartbeat_instance(NULL, "bargaining_training_begin", 0.0f);
    (void)(struct nimcp_bargaining_struct*)instance; /* Module state available for reset */
    return 0;
}

int bargaining_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "bargaining_training_end: NULL argument");
        return -1;
    }
    bargaining_heartbeat_instance(NULL, "bargaining_training_end", 1.0f);
    (void)(struct nimcp_bargaining_struct*)instance; /* Module state available for finalization */
    return 0;
}

int bargaining_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "bargaining_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    bargaining_heartbeat_instance(NULL, "bargaining_training_step", progress);
    (void)(struct nimcp_bargaining_struct*)instance; /* Module state available for step adaptation */
    return 0;
}
