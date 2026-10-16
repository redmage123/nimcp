/**
 * @file nimcp_gust_quantum_bridge.c
 * @brief Gustatory Cortex Quantum Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-12
 *
 * @author NIMCP Development Team
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/gustatory/bridges/nimcp_gust_quantum_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/thread/nimcp_thread_rand.h"

BRIDGE_BOILERPLATE_MESH_ONLY(gust_quantum_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "GUST_QUANTUM_BRIDGE"


/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct gust_quantum_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    gust_quantum_config_t config;
    nimcp_gustatory_t* gust;

    bool is_connected;
    gust_quantum_status_t status;

    gust_quantum_stats_t stats;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static float randf(void) {
    return (float)nimcp_tl_rand() / (float)RAND_MAX;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int gust_quantum_default_config(gust_quantum_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_quantum_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(gust_quantum_config_t));

    config->qmc_samples = GUST_QUANTUM_DEFAULT_QMC_SAMPLES;
    config->walk_steps = GUST_QUANTUM_DEFAULT_WALK_STEPS;
    config->anneal_steps = 300;
    config->anneal_initial_temp = 50.0f;
    config->anneal_final_temp = 0.01f;

    config->max_qubits = GUST_QUANTUM_MAX_QUBITS;
    config->convergence_threshold = 0.001f;
    config->max_iterations = 500;

    config->enable_qmc = true;
    config->enable_walks = true;
    config->enable_annealing = true;
    config->enable_vqe = true;
    config->use_classical_fallback = true;

    config->async_computation = false;
    config->timeout_ms = 3000;
    config->enable_logging = false;

    return 0;
}

gust_quantum_bridge_t* gust_quantum_bridge_create(const gust_quantum_config_t* config) {
    gust_quantum_bridge_t* bridge = (gust_quantum_bridge_t*)nimcp_calloc(1, sizeof(gust_quantum_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gust_quantum_bridge_create: allocation failed");
        return NULL;
    }

    bridge_base_init(&bridge->base, 0, "gust_quantum");

    if (config) {
        memcpy(&bridge->config, config, sizeof(gust_quantum_config_t));
    } else {
        gust_quantum_default_config(&bridge->config);
    }

    bridge->is_connected = false;
    bridge->status = GUST_QUANTUM_STATUS_IDLE;

    NIMCP_LOGGING_INFO("Created %s bridge", "gust_quantum");
    return bridge;
}

void gust_quantum_bridge_destroy(gust_quantum_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "gust_quantum");
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

int gust_quantum_connect(gust_quantum_bridge_t* bridge, nimcp_gustatory_t* gust) {
    if (!bridge || !gust) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_quantum_connect: required parameter is NULL (bridge, gust)");
        return -1;
    }

    bridge->gust = gust;
    bridge->is_connected = true;
    bridge->status = GUST_QUANTUM_STATUS_IDLE;

    return 0;
}

int gust_quantum_disconnect(gust_quantum_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_quantum_disconnect: bridge is NULL");
        return -1;
    }

    bridge->gust = NULL;
    bridge->is_connected = false;

    return 0;
}

bool gust_quantum_is_connected(const gust_quantum_bridge_t* bridge) {
    return bridge && bridge->is_connected;
}

gust_quantum_status_t gust_quantum_get_status(const gust_quantum_bridge_t* bridge) {
    if (!bridge) return GUST_QUANTUM_STATUS_ERROR;
    return bridge->status;
}

/* ============================================================================
 * QMC API Implementation
 * ============================================================================ */

int gust_quantum_calibrate_thresholds(gust_quantum_bridge_t* bridge,
                                      const gust_threshold_cal_spec_t* spec,
                                      gust_qmc_threshold_result_t* result) {
    if (!bridge || !spec || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_quantum_get_status: required parameter is NULL (bridge, spec, result)");
        return -1;
    }
    if (!bridge->config.enable_qmc) {
        return 0;  /* Feature disabled, not an error */
    }

    bridge->status = GUST_QUANTUM_STATUS_COMPUTING;

    /* Monte Carlo threshold calibration */
    float best_quality = 0.0f;

    for (uint32_t s = 0; s < bridge->config.qmc_samples; s++) {
        float candidate[TASTE_COUNT];
        float quality = 0.0f;

        /* Generate candidate thresholds */
        for (int t = 0; t < TASTE_COUNT; t++) {
            float perturbation = (randf() - 0.5f) * 0.1f;
            candidate[t] = spec->current_thresholds[t] + perturbation;
            if (candidate[t] < 0.0f) candidate[t] = 0.01f;
            if (candidate[t] > 1.0f) candidate[t] = 1.0f;
        }

        /* Evaluate quality based on stimulus history */
        for (uint32_t h = 0; h < spec->history_length; h++) {
            for (int t = 0; t < TASTE_COUNT; t++) {
                float stimulus = spec->stimulus_history[h * TASTE_COUNT + t];
                float detection = (stimulus > candidate[t]) ? 1.0f : 0.0f;
                float target = (stimulus > spec->target_sensitivity) ? 1.0f : 0.0f;
                quality += 1.0f - fabsf(detection - target);
            }
        }
        quality /= (spec->history_length * TASTE_COUNT);

        if (quality > best_quality) {
            best_quality = quality;
            for (int t = 0; t < TASTE_COUNT; t++) {
                result->thresholds[t] = candidate[t];
            }
        }
    }

    /* Compute sensitivity from thresholds */
    for (int t = 0; t < TASTE_COUNT; t++) {
        result->sensitivity[t] = 1.0f - result->thresholds[t];
    }

    result->variance = 0.05f;
    result->samples_used = bridge->config.qmc_samples;
    result->calibration_quality = best_quality;

    bridge->status = GUST_QUANTUM_STATUS_COMPLETE;
    bridge->stats.qmc_computations++;
    bridge->stats.successful_computations++;

    return 0;
}

int gust_quantum_sample_taste_response(gust_quantum_bridge_t* bridge,
                                       basic_taste_t taste,
                                       float concentration,
                                       uint32_t num_samples,
                                       float* responses) {
    if (!bridge || !responses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_quantum_get_status: required parameter is NULL (bridge, responses)");
        return -1;
    }
    if (taste >= TASTE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "gust_quantum_get_status: capacity exceeded");
        return -1;
    }

    /* Simulate taste response with noise */
    for (uint32_t i = 0; i < num_samples; i++) {
        float noise = (randf() - 0.5f) * 0.2f;
        responses[i] = concentration + noise;
        if (responses[i] < 0.0f) responses[i] = 0.0f;
        if (responses[i] > 1.0f) responses[i] = 1.0f;
    }

    return 0;
}

/* ============================================================================
 * Quantum Walk API Implementation
 * ============================================================================ */

int gust_quantum_search_flavors(gust_quantum_bridge_t* bridge,
                                const taste_stimulus_t* stimulus,
                                gust_quantum_flavor_result_t* result) {
    if (!bridge || !stimulus || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_quantum_get_status: required parameter is NULL (bridge, stimulus, result)");
        return -1;
    }
    if (!bridge->config.enable_walks) {
        return 0;  /* Feature disabled, not an error */
    }

    bridge->status = GUST_QUANTUM_STATUS_COMPUTING;

    uint32_t max_results = 10;
    result->similar_foods = (uint32_t*)nimcp_calloc(max_results, sizeof(uint32_t));
    result->similarity_scores = (float*)nimcp_calloc(max_results, sizeof(float));
    if (!result->similar_foods || !result->similarity_scores) {
        nimcp_free(result->similar_foods);
        nimcp_free(result->similarity_scores);
        bridge->status = GUST_QUANTUM_STATUS_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gust_quantum_get_status: required parameter is NULL (result->similar_foods, result->similarity_scores)");
        return -1;
    }

    /* Compute taste signature */
    float signature = stimulus->sweet * 0.3f + stimulus->umami * 0.25f +
                     stimulus->salty * 0.2f - stimulus->bitter * 0.15f -
                     stimulus->sour * 0.1f;

    /* Random walk flavor search */
    result->num_similar = 0;
    result->best_score = 0.0f;

    for (uint32_t step = 0; step < bridge->config.walk_steps && result->num_similar < max_results; step++) {
        float similarity = randf() * (0.5f + signature * 0.5f);

        if (similarity > 0.3f) {
            result->similar_foods[result->num_similar] = step * 7;  /* Pseudo food ID */
            result->similarity_scores[result->num_similar] = similarity;

            if (similarity > result->best_score) {
                result->best_score = similarity;
            }

            result->num_similar++;
        }
    }

    /* Determine best category */
    if (stimulus->sweet > 0.5f) {
        result->best_category = FOOD_CAT_CARBOHYDRATE;
    } else if (stimulus->umami > 0.5f) {
        result->best_category = FOOD_CAT_PROTEIN;
    } else {
        result->best_category = FOOD_CAT_UNKNOWN;
    }

    result->steps_taken = bridge->config.walk_steps;

    bridge->status = GUST_QUANTUM_STATUS_COMPLETE;
    bridge->stats.walk_computations++;
    bridge->stats.successful_computations++;

    return 0;
}

int gust_quantum_predict_food_category(gust_quantum_bridge_t* bridge,
                                       const taste_stimulus_t* stimulus,
                                       food_category_t* category,
                                       float* confidence) {
    if (!bridge || !stimulus || !category || !confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_quantum_get_status: required parameter is NULL (bridge, stimulus, category, confidence)");
        return -1;
    }

    /* Simple heuristic classification */
    if (stimulus->bitter > 0.6f || stimulus->sour > 0.7f) {
        *category = FOOD_CAT_TOXIC;
        *confidence = stimulus->bitter * 0.8f;
    } else if (stimulus->sweet > 0.5f) {
        *category = FOOD_CAT_FRUIT;
        *confidence = stimulus->sweet * 0.9f;
    } else if (stimulus->umami > 0.5f) {
        *category = FOOD_CAT_PROTEIN;
        *confidence = stimulus->umami * 0.85f;
    } else if (stimulus->fat_content > 0.5f) {
        *category = FOOD_CAT_FAT;
        *confidence = stimulus->fat_content * 0.8f;
    } else {
        *category = FOOD_CAT_UNKNOWN;
        *confidence = 0.3f;
    }

    return 0;
}

/* ============================================================================
 * Quantum Annealing API Implementation
 * ============================================================================ */

int gust_quantum_optimize_preferences(gust_quantum_bridge_t* bridge,
                                      const gust_preference_opt_spec_t* spec,
                                      gust_quantum_preference_result_t* result) {
    if (!bridge || !spec || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_quantum_get_status: required parameter is NULL (bridge, spec, result)");
        return -1;
    }
    if (!bridge->config.enable_annealing) {
        return 0;  /* Feature disabled, not an error */
    }

    bridge->status = GUST_QUANTUM_STATUS_COMPUTING;

    /* Simulated annealing for preference optimization */
    float current_prefs[TASTE_COUNT];
    memcpy(current_prefs, spec->current_preferences, TASTE_COUNT * sizeof(float));

    float temperature = bridge->config.anneal_initial_temp;
    float temp_decay = powf(bridge->config.anneal_final_temp / bridge->config.anneal_initial_temp,
                           1.0f / bridge->config.anneal_steps);

    auto float compute_reward(const float* prefs) {
        float reward = 0.0f;
        for (uint32_t i = 0; i < spec->num_recent_rewards && i < 16; i++) {
            for (int t = 0; t < TASTE_COUNT; t++) {
                reward += prefs[t] * spec->recent_rewards[i] / (i + 1);
            }
        }
        /* Satiety modulation */
        reward *= (1.0f - spec->satiety_level * 0.5f);
        return reward;
    }

    float current_reward = compute_reward(current_prefs);

    for (uint32_t step = 0; step < bridge->config.anneal_steps; step++) {
        /* Propose change */
        int t = (int)(randf() * TASTE_COUNT) % TASTE_COUNT;
        float old_val = current_prefs[t];
        current_prefs[t] += (randf() - 0.5f) * spec->learning_rate;
        if (current_prefs[t] < -1.0f) current_prefs[t] = -1.0f;
        if (current_prefs[t] > 1.0f) current_prefs[t] = 1.0f;

        float new_reward = compute_reward(current_prefs);

        /* Accept or reject */
        if (new_reward > current_reward ||
            randf() < expf((new_reward - current_reward) / temperature)) {
            current_reward = new_reward;
        } else {
            current_prefs[t] = old_val;
        }

        temperature *= temp_decay;
    }

    memcpy(result->preferences, current_prefs, TASTE_COUNT * sizeof(float));
    result->predicted_palatability = (current_reward + 1.0f) / 2.0f;
    result->predicted_reward = current_reward;
    result->final_energy = -current_reward;
    result->converged = true;
    result->iterations = bridge->config.anneal_steps;

    bridge->status = GUST_QUANTUM_STATUS_COMPLETE;
    bridge->stats.anneal_computations++;
    bridge->stats.successful_computations++;

    return 0;
}

int gust_quantum_predict_reward(gust_quantum_bridge_t* bridge,
                                const taste_stimulus_t* stimulus,
                                float satiety,
                                gust_quantum_reward_result_t* result) {
    if (!bridge || !stimulus || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, stimulus, result)");
        return -1;
    }

    result->contribution_weights = (float*)nimcp_calloc(TASTE_COUNT, sizeof(float));
    if (!result->contribution_weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: result->contribution_weights is NULL");
        return -1;
    }

    /* Compute reward from stimulus */
    float reward = 0.0f;

    result->contribution_weights[TASTE_SWEET] = stimulus->sweet * 0.35f;
    result->contribution_weights[TASTE_UMAMI] = stimulus->umami * 0.30f;
    result->contribution_weights[TASTE_SALTY] = stimulus->salty * 0.15f;
    result->contribution_weights[TASTE_SOUR] = -stimulus->sour * 0.10f;
    result->contribution_weights[TASTE_BITTER] = -stimulus->bitter * 0.20f;

    for (int t = 0; t < TASTE_COUNT; t++) {
        reward += result->contribution_weights[t];
    }

    /* Novelty bonus (placeholder) */
    result->novelty_bonus = randf() * 0.1f;

    /* Satiety penalty */
    result->satiety_penalty = satiety * 0.3f;

    result->predicted_reward = reward + result->novelty_bonus - result->satiety_penalty;
    if (result->predicted_reward < 0.0f) result->predicted_reward = 0.0f;
    if (result->predicted_reward > 1.0f) result->predicted_reward = 1.0f;

    result->confidence = 0.8f;

    return 0;
}

int gust_quantum_optimize_disgust_threshold(gust_quantum_bridge_t* bridge,
                                            float current_threshold,
                                            float* optimal) {
    if (!bridge || !optimal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, optimal)");
        return -1;
    }

    /* Simple optimization - threshold should be sensitive enough */
    *optimal = current_threshold * 0.9f + randf() * 0.05f;
    if (*optimal < 0.1f) *optimal = 0.1f;

    return 0;
}

/* ============================================================================
 * Result Management
 * ============================================================================ */

void gust_qmc_threshold_result_free(gust_qmc_threshold_result_t* result) {
    (void)result;  /* No dynamic allocations */
}

void gust_quantum_flavor_result_free(gust_quantum_flavor_result_t* result) {
    if (!result) return;
    nimcp_free(result->similar_foods);
    nimcp_free(result->similarity_scores);
    result->similar_foods = NULL;
    result->similarity_scores = NULL;
}

void gust_quantum_preference_result_free(gust_quantum_preference_result_t* result) {
    (void)result;  /* No dynamic allocations */
}

void gust_quantum_reward_result_free(gust_quantum_reward_result_t* result) {
    if (!result) return;
    nimcp_free(result->contribution_weights);
    result->contribution_weights = NULL;
}

/* ============================================================================
 * Statistics API Implementation
 * ============================================================================ */

int gust_quantum_get_stats(const gust_quantum_bridge_t* bridge, gust_quantum_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_quantum_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    memcpy(stats, &bridge->stats, sizeof(gust_quantum_stats_t));
    return 0;
}

int gust_quantum_reset_stats(gust_quantum_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_quantum_reset_stats: bridge is NULL");
        return -1;
    }
    memset(&bridge->stats, 0, sizeof(gust_quantum_stats_t));
    return 0;
}

const char* gust_quantum_algorithm_name(gust_quantum_algorithm_t alg) {
    switch (alg) {
        case GUST_QUANTUM_ALG_QMC:    return "QMC";
        case GUST_QUANTUM_ALG_WALK:   return "QUANTUM_WALK";
        case GUST_QUANTUM_ALG_ANNEAL: return "ANNEALING";
        case GUST_QUANTUM_ALG_VQE:    return "VQE";
        default:                       return "UNKNOWN";
    }
}

const char* gust_quantum_problem_name(gust_quantum_problem_t prob) {
    switch (prob) {
        case GUST_QUANTUM_PROB_THRESHOLD_CAL: return "THRESHOLD_CAL";
        case GUST_QUANTUM_PROB_FLAVOR_SEARCH: return "FLAVOR_SEARCH";
        case GUST_QUANTUM_PROB_PREFERENCE_OPT: return "PREFERENCE_OPT";
        case GUST_QUANTUM_PROB_REWARD_PRED:   return "REWARD_PRED";
        case GUST_QUANTUM_PROB_DISGUST_OPT:   return "DISGUST_OPT";
        default:                               return "UNKNOWN";
    }
}

void gust_quantum_print_summary(const gust_quantum_bridge_t* bridge) {
    if (!bridge) return;

    printf("=== Gustatory Quantum Bridge Summary ===\n");
    printf("Connected: %s\n", bridge->is_connected ? "Yes" : "No");
    printf("QMC: %lu, Walk: %lu, Anneal: %lu, VQE: %lu\n",
           (unsigned long)bridge->stats.qmc_computations,
           (unsigned long)bridge->stats.walk_computations,
           (unsigned long)bridge->stats.anneal_computations,
           (unsigned long)bridge->stats.vqe_computations);
    printf("========================================\n");
}
