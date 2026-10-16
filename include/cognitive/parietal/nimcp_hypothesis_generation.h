/**
 * @file nimcp_hypothesis_generation.h
 * @brief Hypothesis generation engine for theory creation
 *
 * WHAT: Engine for generating and testing hypotheses
 * WHY:  Enable abductive reasoning and theory formation
 * HOW:  Inference to best explanation, prediction generation
 *
 * @author NIMCP Development Team
 * @date 2025-12-29
 * @version 1.0.0
 */

#ifndef NIMCP_HYPOTHESIS_GENERATION_H
#define NIMCP_HYPOTHESIS_GENERATION_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HYPOGEN_MAX_DESCRIPTION     512
#define HYPOGEN_MAX_OBSERVATIONS    64
#define HYPOGEN_MAX_PREDICTIONS     32
#define HYPOGEN_MAX_ASSUMPTIONS     16
#define BIO_MODULE_HYPOTHESIS_GEN   0x03A3

typedef struct hypothesis_engine hypothesis_engine_t;

typedef struct {
    uint32_t id;
    float* data;
    uint32_t dim;
    float confidence;
    bool is_surprising;
    char description[256];
} hypogen_observation_t;

typedef struct {
    uint32_t id;
    char statement[HYPOGEN_MAX_DESCRIPTION];
    float* parameters;
    uint32_t num_params;
    float explanatory_power;
    float parsimony;
    float falsifiability;
    float prior;
    float likelihood;
    float posterior;
    char** assumptions;
    uint32_t num_assumptions;
} hypogen_theory_t;

typedef struct {
    uint32_t id;
    char description[256];
    float* predicted_values;
    uint32_t num_values;
    float confidence;
    bool is_testable;
    bool is_novel;
} hypogen_prediction_t;

typedef struct {
    float min_explanatory_power;
    float parsimony_weight;
    float novelty_bonus;
    bool enable_abduction;
    bool enable_prediction;
    uint32_t max_hypotheses;
    float inflammation_sensitivity;
    float fatigue_sensitivity;
} hypogen_config_t;

typedef struct {
    uint64_t hypotheses_generated;
    uint64_t predictions_made;
    uint64_t hypotheses_confirmed;
    uint64_t hypotheses_rejected;
    float avg_explanatory_power;
} hypogen_stats_t;

/* Lifecycle */
hypothesis_engine_t* hypothesis_engine_create(void);
hypothesis_engine_t* hypothesis_engine_create_custom(const hypogen_config_t* config);
void hypothesis_engine_destroy(hypothesis_engine_t* engine);
hypogen_config_t hypothesis_engine_default_config(void);

/* Hypothesis Generation */
hypogen_theory_t** hypothesis_generate_explanations(hypothesis_engine_t* engine,
    const hypogen_observation_t* observations, uint32_t num_obs, uint32_t* num_theories);
hypogen_theory_t* hypothesis_abductive_inference(hypothesis_engine_t* engine,
    const hypogen_observation_t* surprising_fact);
int hypothesis_rank_theories(hypothesis_engine_t* engine,
    hypogen_theory_t** theories, uint32_t num_theories, uint32_t* rankings);

/* Prediction */
hypogen_prediction_t** hypothesis_derive_predictions(hypothesis_engine_t* engine,
    const hypogen_theory_t* theory, uint32_t* num_predictions);
int hypothesis_test_prediction(hypothesis_engine_t* engine,
    hypogen_prediction_t* prediction, const hypogen_observation_t* observation);

/* Theory Revision */
hypogen_theory_t* hypothesis_revise_theory(hypothesis_engine_t* engine,
    hypogen_theory_t* theory, const hypogen_observation_t* new_evidence);
int hypothesis_evaluate_theory(hypothesis_engine_t* engine,
    const hypogen_theory_t* theory, float* score);

/* Cleanup */
void hypothesis_free_theory(hypogen_theory_t* theory);
void hypothesis_free_prediction(hypogen_prediction_t* prediction);

/* Modulation */
int hypothesis_set_inflammation(hypothesis_engine_t* engine, float level);
int hypothesis_set_fatigue(hypothesis_engine_t* engine, float level);

/* Statistics */
int hypothesis_get_stats(const hypothesis_engine_t* engine, hypogen_stats_t* stats);
void hypothesis_reset_stats(hypothesis_engine_t* engine);
const char* hypothesis_get_last_error(void);

/* MCTS-based Hypothesis Search */
hypogen_theory_t* hypothesis_search_mcts(
    hypothesis_engine_t* engine,
    hypogen_theory_t** theories,
    uint32_t num_theories,
    uint32_t num_iterations);

/* Monte Carlo Utilities */
hypogen_theory_t* hypothesis_sample_mc(
    hypothesis_engine_t* engine,
    hypogen_theory_t** theories,
    uint32_t num_theories,
    float temperature);

float hypothesis_estimate_confidence_mc(
    hypothesis_engine_t* engine,
    const hypogen_theory_t* theory,
    uint32_t num_samples);

void hypothesis_add_exploration_noise_mc(
    hypothesis_engine_t* engine,
    hypogen_theory_t** theories,
    uint32_t num_theories,
    float noise_scale);

uint32_t* hypothesis_get_mc_seed(hypothesis_engine_t* engine);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHESIS_GENERATION_H */
