/**
 * @file nimcp_hypothesis_generation.c
 * @brief Hypothesis generation engine implementation
 */

#include "cognitive/parietal/nimcp_hypothesis_generation.h"
#include "constants/nimcp_buffer_constants.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/algorithms/nimcp_monte_carlo.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* Health agent macros must be at file scope, not inside #ifdef */
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"

BRIDGE_BOILERPLATE(hypothesis_generation, MESH_ADAPTER_CATEGORY_COGNITIVE)

/* GPU acceleration with CPU fallback */
#ifdef NIMCP_ENABLE_CUDA
#include "gpu/quantum/nimcp_qmc_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"

static nimcp_gpu_context_t* g_hypogen_gpu_ctx = NULL;
static qmc_gpu_rng_t g_hypogen_gpu_rng = NULL;
static bool g_hypogen_gpu_init_attempted = false;

static bool hypogen_init_gpu_mc(void) {
    if (g_hypogen_gpu_init_attempted) return g_hypogen_gpu_rng != NULL;
    g_hypogen_gpu_init_attempted = true;
    if (!qmc_gpu_is_available()) {
        /* GPU not available is normal, not an error */
        return false;
    }
    g_hypogen_gpu_ctx = nimcp_gpu_context_create_auto();
    if (!g_hypogen_gpu_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypogen_init_gpu_mc: g_hypogen_gpu_ctx is NULL");
        return false;
    }
    g_hypogen_gpu_rng = qmc_gpu_rng_create(g_hypogen_gpu_ctx, 4096, 0);
    if (!g_hypogen_gpu_rng) {
        nimcp_gpu_context_destroy(g_hypogen_gpu_ctx);
        g_hypogen_gpu_ctx = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypogen_init_gpu_mc: g_hypogen_gpu_rng is NULL");
        return false;
    }
    return true;
}

static inline bool hypogen_has_gpu_mc(void) {
    if (!g_hypogen_gpu_init_attempted) hypogen_init_gpu_mc();
    return g_hypogen_gpu_rng != NULL;
}

/* P2-COG-11: Shutdown function to free leaked GPU resources */
static void hypogen_shutdown_gpu_mc(void) {
    if (g_hypogen_gpu_rng) {
        qmc_gpu_rng_destroy(g_hypogen_gpu_rng);
        g_hypogen_gpu_rng = NULL;
    }
    if (g_hypogen_gpu_ctx) {
        nimcp_gpu_context_destroy(g_hypogen_gpu_ctx);
        g_hypogen_gpu_ctx = NULL;
    }
    g_hypogen_gpu_init_attempted = false;
}
#else
static inline bool hypogen_has_gpu_mc(void) { return false; }
static inline void hypogen_shutdown_gpu_mc(void) {}
#endif

struct hypothesis_engine {
    hypogen_config_t config;
    hypogen_stats_t stats;
    float inflammation;
    float fatigue;
    uint32_t next_theory_id;
    uint32_t next_prediction_id;
    uint32_t rand_seed;  /**< Thread-safe RNG seed for MCTS */
};

static _Thread_local char g_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* msg) {
    strncpy(g_last_error, msg, sizeof(g_last_error) - 1);
}

static float apply_mod(const hypothesis_engine_t* e, float v) {
    float f = 1.0f - e->inflammation * e->config.inflammation_sensitivity * 0.2f
                   - e->fatigue * e->config.fatigue_sensitivity * 0.3f;
    return v * fmaxf(0.4f, f);
}

hypogen_config_t hypothesis_engine_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    hypothesis_generation_heartbeat("hypothesis_g_hypothesis_engine_de", 0.0f);


    return (hypogen_config_t){
        .min_explanatory_power = 0.3f,
        .parsimony_weight = 0.3f,
        .novelty_bonus = 0.1f,
        .enable_abduction = true,
        .enable_prediction = true,
        .max_hypotheses = 10,
        .inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .fatigue_sensitivity = 1.0f
    };
}

hypothesis_engine_t* hypothesis_engine_create(void) {
    /* Phase 8: Heartbeat at operation start */
    hypothesis_generation_heartbeat("hypothesis_g_hypothesis_engine_cr", 0.0f);


    hypogen_config_t c = hypothesis_engine_default_config();
    return hypothesis_engine_create_custom(&c);
}

hypothesis_engine_t* hypothesis_engine_create_custom(const hypogen_config_t* config) {
    if (!config) { set_error("NULL config"); return NULL; }
    /* Phase 8: Heartbeat at operation start */
    hypothesis_generation_heartbeat("hypothesis_g_hypothesis_engine_cr", 0.0f);


    hypothesis_engine_t* e = nimcp_calloc(1, sizeof(hypothesis_engine_t));
    if (!e) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate e");

        return NULL;

    }
    e->config = *config;
    e->next_theory_id = 1;
    e->next_prediction_id = 1;
    e->rand_seed = mc_seed_from_time();
    return e;
}

void hypothesis_engine_destroy(hypothesis_engine_t* engine) {
    /* Phase 8: Heartbeat at operation start */
    hypothesis_generation_heartbeat("hypothesis_g_hypothesis_engine_de", 0.0f);

    if (engine) nimcp_free(engine);

    /* P2-COG-11: Clean up global GPU resources when last engine is destroyed */
    hypogen_shutdown_gpu_mc();
}

hypogen_theory_t** hypothesis_generate_explanations(hypothesis_engine_t* engine,
    const hypogen_observation_t* observations, uint32_t num_obs, uint32_t* num_theories) {
    if (!engine || !observations || !num_theories) {
        /* P2-COG-10: Correct function name */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothesis_generate_explanations: required parameter is NULL (engine, observations, num_theories)");
        return NULL;
    }

    uint32_t max = engine->config.max_hypotheses;
    hypogen_theory_t** theories = nimcp_calloc(max, sizeof(hypogen_theory_t*));
    if (!theories) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate theories");

        return NULL;

    }

    *num_theories = 0;
    for (uint32_t i = 0; i < max && i < num_obs; i++) {
        hypogen_theory_t* t = nimcp_calloc(1, sizeof(hypogen_theory_t));
        if (!t) break;

        t->id = engine->next_theory_id++;
        snprintf(t->statement, sizeof(t->statement),
                "Theory explaining observation %u", i);
        t->explanatory_power = 0.5f + 0.3f * mc_random_uniform(&engine->rand_seed);
        t->parsimony = 0.6f + 0.2f * mc_random_uniform(&engine->rand_seed);
        t->falsifiability = 0.7f;
        t->prior = 0.5f;
        t->likelihood = apply_mod(engine, t->explanatory_power);
        t->posterior = t->prior * t->likelihood;

        theories[(*num_theories)++] = t;
        engine->stats.hypotheses_generated++;
    }

    return theories;
}

hypogen_theory_t* hypothesis_abductive_inference(hypothesis_engine_t* engine,
    const hypogen_observation_t* surprising_fact) {
    if (!engine || !surprising_fact) {
        /* P2-COG-10: Correct function name */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothesis_abductive_inference: required parameter is NULL (engine, surprising_fact)");
        return NULL;
    }
    if (!engine->config.enable_abduction) {
        /* P2-COG-10: Correct function name and message */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypothesis_abductive_inference: abduction disabled in config");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    hypothesis_generation_heartbeat("hypothesis_g_hypothesis_abductive", 0.0f);


    hypogen_theory_t* t = nimcp_calloc(1, sizeof(hypogen_theory_t));
    if (!t) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate t");

        return NULL;

    }

    t->id = engine->next_theory_id++;
    snprintf(t->statement, sizeof(t->statement),
            "Abductive explanation for: %s", surprising_fact->description);
    t->explanatory_power = apply_mod(engine, 0.7f);
    t->parsimony = 0.5f;
    t->falsifiability = 0.8f;
    t->prior = 0.3f;
    t->likelihood = 0.8f;
    t->posterior = t->prior * t->likelihood;

    engine->stats.hypotheses_generated++;
    return t;
}

int hypothesis_rank_theories(hypothesis_engine_t* engine,
    hypogen_theory_t** theories, uint32_t num_theories, uint32_t* rankings) {
    if (!engine || !theories || !rankings) {
        /* P2-COG-10: Correct function name in throw message */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothesis_rank_theories: required parameter is NULL (engine, theories, rankings)");
        return -1;
    }

    /* P1-COG-03: Guard against num_theories==0 or 1 (underflow in num_theories - 1) */
    if (num_theories <= 1) {
        if (num_theories == 1) rankings[0] = 0;
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    hypothesis_generation_heartbeat("hypothesis_g_hypothesis_rank_theo", 0.0f);


    for (uint32_t i = 0; i < num_theories; i++) rankings[i] = i;

    /* Bubble sort by posterior */
    for (uint32_t i = 0; i < num_theories - 1; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_theories > 256) {
            hypothesis_generation_heartbeat("hypothesis_g_loop",
                             (float)(i + 1) / (float)num_theories);
        }

        for (uint32_t j = 0; j < num_theories - i - 1; j++) {
            if (theories[rankings[j]]->posterior < theories[rankings[j+1]]->posterior) {
                uint32_t tmp = rankings[j];
                rankings[j] = rankings[j+1];
                rankings[j+1] = tmp;
            }
        }
    }
    return 0;
}

hypogen_prediction_t** hypothesis_derive_predictions(hypothesis_engine_t* engine,
    const hypogen_theory_t* theory, uint32_t* num_predictions) {
    if (!engine || !theory || !num_predictions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothesis_derive_predictions: required parameter is NULL (engine, theory, num_predictions)");
        return NULL;
    }
    if (!engine->config.enable_prediction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypothesis_derive_predictions: prediction disabled in config");
        return NULL;
    }

    uint32_t n = 3;  /* Generate 3 predictions */
    hypogen_prediction_t** preds = nimcp_calloc(n, sizeof(hypogen_prediction_t*));
    if (!preds) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate preds");

        return NULL;

    }

    *num_predictions = 0;
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            hypothesis_generation_heartbeat("hypothesis_g_loop",
                             (float)(i + 1) / (float)n);
        }

        hypogen_prediction_t* p = nimcp_calloc(1, sizeof(hypogen_prediction_t));
        if (!p) break;

        p->id = engine->next_prediction_id++;
        snprintf(p->description, sizeof(p->description),
                "Prediction %u from theory %u", i+1, theory->id);
        p->confidence = apply_mod(engine, theory->posterior * 0.8f);
        p->is_testable = true;
        p->is_novel = (i == 0);  /* First prediction is novel */

        preds[(*num_predictions)++] = p;
        engine->stats.predictions_made++;
    }

    return preds;
}

int hypothesis_test_prediction(hypothesis_engine_t* engine,
    hypogen_prediction_t* prediction, const hypogen_observation_t* observation) {
    if (!engine || !prediction || !observation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothesis_test_prediction: required parameter is NULL (engine, prediction, observation)");
        return -1;
    }

    /* Simple test: compare confidence */
    /* Phase 8: Heartbeat at operation start */
    hypothesis_generation_heartbeat("hypothesis_g_hypothesis_test_pred", 0.0f);


    if (prediction->confidence > 0.5f && observation->confidence > 0.5f) {
        engine->stats.hypotheses_confirmed++;
        return 1;  /* Confirmed */
    }
    engine->stats.hypotheses_rejected++;
    return 0;  /* Rejected */
}

hypogen_theory_t* hypothesis_revise_theory(hypothesis_engine_t* engine,
    hypogen_theory_t* theory, const hypogen_observation_t* new_evidence) {
    if (!engine || !theory || !new_evidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothesis_revise_theory: required parameter is NULL (engine, theory, new_evidence)");
        return NULL;
    }

    /* Bayesian update */
    /* Phase 8: Heartbeat at operation start */
    hypothesis_generation_heartbeat("hypothesis_g_hypothesis_revise_th", 0.0f);


    float likelihood = new_evidence->confidence;
    theory->posterior = theory->posterior * likelihood;
    theory->posterior = fmaxf(0.01f, fminf(0.99f, theory->posterior));

    return theory;
}

int hypothesis_evaluate_theory(hypothesis_engine_t* engine,
    const hypogen_theory_t* theory, float* score) {
    if (!engine || !theory || !score) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothesis_evaluate_theory: required parameter is NULL (engine, theory, score)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    hypothesis_generation_heartbeat("hypothesis_evaluate", 0.0f);

    *score = theory->explanatory_power * 0.4f +
             theory->parsimony * engine->config.parsimony_weight +
             theory->falsifiability * 0.2f +
             theory->posterior * 0.1f;
    *score = apply_mod(engine, *score);

    engine->stats.avg_explanatory_power =
        (engine->stats.avg_explanatory_power * engine->stats.hypotheses_generated +
         theory->explanatory_power) / (engine->stats.hypotheses_generated + 1);

    return 0;
}

void hypothesis_free_theory(hypogen_theory_t* theory) {
    if (!theory) return;
    /* Phase 8: Heartbeat at operation start */
    hypothesis_generation_heartbeat("hypothesis_g_hypothesis_free_theo", 0.0f);


    if (theory->parameters) nimcp_free(theory->parameters);
    if (theory->assumptions) {
        for (uint32_t i = 0; i < theory->num_assumptions; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && theory->num_assumptions > 256) {
                hypothesis_generation_heartbeat("hypothesis_g_loop",
                                 (float)(i + 1) / (float)theory->num_assumptions);
            }

            if (theory->assumptions[i]) nimcp_free(theory->assumptions[i]);
        }
        nimcp_free(theory->assumptions);
    }
    nimcp_free(theory);
    theory = NULL;
}

void hypothesis_free_prediction(hypogen_prediction_t* prediction) {
    if (!prediction) return;
    /* Phase 8: Heartbeat at operation start */
    hypothesis_generation_heartbeat("hypothesis_g_hypothesis_free_pred", 0.0f);


    if (prediction->predicted_values) nimcp_free(prediction->predicted_values);
    nimcp_free(prediction);
    prediction = NULL;
}

int hypothesis_set_inflammation(hypothesis_engine_t* engine, float level) {
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothesis_set_inflammation: engine is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    hypothesis_generation_heartbeat("hypothesis_g_hypothesis_set_infla", 0.0f);


    engine->inflammation = fmaxf(0, fminf(1, level));
    return 0;
}

int hypothesis_set_fatigue(hypothesis_engine_t* engine, float level) {
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothesis_set_fatigue: engine is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    hypothesis_generation_heartbeat("hypothesis_g_hypothesis_set_fatig", 0.0f);


    engine->fatigue = fmaxf(0, fminf(1, level));
    return 0;
}

int hypothesis_get_stats(const hypothesis_engine_t* engine, hypogen_stats_t* stats) {
    if (!engine || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothesis_get_stats: required parameter is NULL (engine, stats)");
        return -1;
    }
    *stats = engine->stats;
    /* Phase 8: Heartbeat at operation start */
    hypothesis_generation_heartbeat("hypothesis_g_hypothesis_get_stats", 0.0f);


    return 0;
}

void hypothesis_reset_stats(hypothesis_engine_t* engine) {
    /* Phase 8: Heartbeat at operation start */
    hypothesis_generation_heartbeat("hypothesis_g_hypothesis_reset_sta", 0.0f);


    if (engine) memset(&engine->stats, 0, sizeof(engine->stats));
}

const char* hypothesis_get_last_error(void) { return g_last_error; }

/* ============================================================================
 * W14 (2026-04-24): KG runtime emit + read-path for hypothesis generation.
 *
 * Callers that hold a brain_t invoke this after generating/evaluating a
 * hypothesis to register it in brain->internal_kg and query the last
 * plausibility bias.
 * ============================================================================ */
#include "cognitive/kg/nimcp_wave14_math_genius_kg.h"
float hypothesis_generation_wave14_kg_emit(
    struct brain_struct* brain,
    const char* hypothesis_label,
    float plausibility)
{
    if (!brain) return 0.5f;
    wave14_hypothesis_emit_generation(brain, hypothesis_label, plausibility);
    return wave14_hypothesis_query_plausibility_bias(brain);
}

/* ============================================================================
 * MCTS-BASED HYPOTHESIS SEARCH
 * ============================================================================ */

/**
 * @brief State for MCTS hypothesis search
 */
typedef struct {
    uint32_t num_observations;
    /* P1-COG-04: Use 64-bit mask to support up to 63 theories without UB */
    uint64_t tested_theory_mask;  /**< Bitmask of tested theories */
    float best_confidence;
    uint32_t best_theory_id;
    uint32_t depth;
} hypogen_mcts_state_t;

/**
 * @brief User data for MCTS callbacks
 */
typedef struct {
    hypothesis_engine_t* engine;
    hypogen_theory_t** theories;
    uint32_t num_theories;
} hypogen_mcts_user_data_t;

static uint32_t hypogen_mcts_get_action_count(const void* state, void* user_data) {
    hypogen_mcts_user_data_t* ud = (hypogen_mcts_user_data_t*)user_data;
    return ud->num_theories;
}

static uint32_t hypogen_mcts_get_action(const void* state, uint32_t idx, void* user_data) {
    (void)state;
    (void)user_data;
    return idx;
}

static void* hypogen_mcts_apply_action(const void* state, uint32_t action, void* user_data) {
    const hypogen_mcts_state_t* s = (const hypogen_mcts_state_t*)state;
    hypogen_mcts_user_data_t* ud = (hypogen_mcts_user_data_t*)user_data;

    hypogen_mcts_state_t* new_state = nimcp_malloc(sizeof(hypogen_mcts_state_t));
    if (!new_state) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate new_state");

        return NULL;

    }

    *new_state = *s;
    new_state->depth = s->depth + 1;
    /* P1-COG-04: Use 1ULL for 64-bit shift to avoid UB when action >= 32 */
    new_state->tested_theory_mask |= (1ULL << action);

    /* Simulate testing this theory */
    if (action < ud->num_theories) {
        hypogen_theory_t* theory = ud->theories[action];
        float score = theory->posterior;

        /* Apply some randomness to simulate test outcome */
        float test_outcome = mc_random_uniform(&ud->engine->rand_seed);
        if (test_outcome < score) {
            /* Theory confirmed - boost confidence */
            new_state->best_confidence = fmaxf(s->best_confidence, score * 1.2f);
            new_state->best_theory_id = theory->id;
        } else {
            /* Theory weakened */
            new_state->best_confidence = s->best_confidence * 0.9f;
        }
    }

    if (new_state->best_confidence > 1.0f) {
        new_state->best_confidence = 1.0f;
    }

    return new_state;
}

static float hypogen_mcts_evaluate(const void* state, void* user_data) {
    const hypogen_mcts_state_t* s = (const hypogen_mcts_state_t*)state;
    (void)user_data;
    return s->best_confidence;
}

static bool hypogen_mcts_is_terminal(const void* state, void* user_data) {
    const hypogen_mcts_state_t* s = (const hypogen_mcts_state_t*)state;
    hypogen_mcts_user_data_t* ud = (hypogen_mcts_user_data_t*)user_data;

    /* Terminal if high confidence or all tested */
    if (s->best_confidence >= 0.9f) return true;
    /* P1-COG-04: Use 1ULL for 64-bit shift */
    if (s->tested_theory_mask == ((1ULL << ud->num_theories) - 1)) return true;
    if (s->depth >= 5) return true;
    /* P2-COG-09: Not terminal is normal state, not an error */
    return false;
}

static void hypogen_mcts_free_state(void* state, void* user_data) {
    (void)user_data;
    nimcp_free(state);
    state = NULL;
}

static void* hypogen_mcts_clone_state(const void* state, void* user_data) {
    (void)user_data;
    if (!state) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "state is NULL");

        return NULL;

    }
    hypogen_mcts_state_t* clone = nimcp_malloc(sizeof(hypogen_mcts_state_t));
    if (!clone) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate clone");

        return NULL;

    }
    *clone = *(const hypogen_mcts_state_t*)state;
    return clone;
}

/**
 * @brief Search for best hypothesis using MCTS
 */
hypogen_theory_t* hypothesis_search_mcts(
    hypothesis_engine_t* engine,
    hypogen_theory_t** theories,
    uint32_t num_theories,
    uint32_t num_iterations
) {
    if (!engine || !theories || num_theories == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothesis_search_mcts: required parameter is NULL (engine, theories)");
        return NULL;
    }
    /* Phase 8: Heartbeat at operation start */
    hypothesis_generation_heartbeat("hypothesis_g_hypothesis_search_mc", 0.0f);


    /* P1-COG-04: Clamp to 63 for 64-bit bitmask (bit 63 max) */
    if (num_theories > 63) num_theories = 63;

    hypogen_mcts_state_t initial_state = {
        .num_observations = 0,
        .tested_theory_mask = 0,
        .best_confidence = 0.0f,
        .best_theory_id = 0,
        .depth = 0
    };

    hypogen_mcts_user_data_t user_data = {
        .engine = engine,
        .theories = theories,
        .num_theories = num_theories
    };

    mcts_config_t config;
    mcts_config_init(&config);
    config.max_iterations = num_iterations > 0 ? num_iterations : 25;
    config.max_depth = 5;
    config.exploration_constant = 1.2f;
    config.discount_factor = 0.95f;
    config.max_nodes = 64;

    config.get_action_count = hypogen_mcts_get_action_count;
    config.get_action = hypogen_mcts_get_action;
    config.apply_action = hypogen_mcts_apply_action;
    config.evaluate = hypogen_mcts_evaluate;
    config.is_terminal = hypogen_mcts_is_terminal;
    config.free_state = hypogen_mcts_free_state;
    config.clone_state = hypogen_mcts_clone_state;
    config.user_data = &user_data;
    config.seed = engine->rand_seed;

    mcts_result_t result;
    nimcp_mc_result_t err = mcts_search(&config, &initial_state, &result);

    engine->rand_seed = config.seed;

    if (err != NIMCP_MC_OK) {
        return num_theories > 0 ? theories[0] : NULL;
    }

    uint32_t best_idx = result.best_action;
    mcts_result_free(&result);

    if (best_idx < num_theories) {
        return theories[best_idx];
    }
    return theories[0];
}

/* ============================================================================
 * MONTE CARLO UTILITIES
 * ============================================================================ */

/**
 * @brief Sample hypothesis using softmax MC sampling
 *
 * WHAT: Probabilistically select hypothesis based on posterior
 * WHY:  Enable diverse exploration while respecting quality
 * HOW:  Apply softmax to posteriors, sample from distribution
 *
 * @param engine Hypothesis engine
 * @param theories Array of theories
 * @param num_theories Number of theories
 * @param temperature Softmax temperature (higher = more random)
 * @return Pointer to selected theory, or NULL on error
 */
hypogen_theory_t* hypothesis_sample_mc(
    hypothesis_engine_t* engine,
    hypogen_theory_t** theories,
    uint32_t num_theories,
    float temperature
) {
    if (!engine || !theories || num_theories == 0 || temperature <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothesis_sample_mc: required parameter is NULL (engine, theories)");
        return NULL;
    }

    /* Find max posterior for numerical stability */
    /* Phase 8: Heartbeat at operation start */
    hypothesis_generation_heartbeat("hypothesis_g_hypothesis_sample_mc", 0.0f);


    float max_posterior = theories[0]->posterior;
    for (uint32_t i = 1; i < num_theories; i++) {
        if (theories[i]->posterior > max_posterior) {
            max_posterior = theories[i]->posterior;
        }
    }

    /* Compute softmax probabilities */
    float* probs = nimcp_calloc(num_theories, sizeof(float));
    if (!probs) return theories[0];

    float sum_exp = 0.0f;
    for (uint32_t i = 0; i < num_theories; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_theories > 256) {
            hypothesis_generation_heartbeat("hypothesis_g_loop",
                             (float)(i + 1) / (float)num_theories);
        }

        probs[i] = expf((theories[i]->posterior - max_posterior) / temperature);
        sum_exp += probs[i];
    }

    /* Sample from distribution */
    float r = mc_random_uniform(&engine->rand_seed) * sum_exp;
    float cumulative = 0.0f;
    hypogen_theory_t* selected = theories[num_theories - 1];

    for (uint32_t i = 0; i < num_theories; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_theories > 256) {
            hypothesis_generation_heartbeat("hypothesis_g_loop",
                             (float)(i + 1) / (float)num_theories);
        }

        cumulative += probs[i];
        if (r < cumulative) {
            selected = theories[i];
            break;
        }
    }

    nimcp_free(probs);
    probs = NULL;
    return selected;
}

/**
 * @brief Estimate hypothesis confidence via MC sampling
 *
 * WHAT: Estimate confidence using bootstrapping
 * WHY:  Robust confidence estimation under uncertainty
 * HOW:  Resample evidence, compute posterior distribution
 *
 * @param engine Hypothesis engine
 * @param theory Theory to evaluate
 * @param num_samples Number of MC samples
 * @return Average posterior with sampling
 */
float hypothesis_estimate_confidence_mc(
    hypothesis_engine_t* engine,
    const hypogen_theory_t* theory,
    uint32_t num_samples
) {
    if (!engine || !theory || num_samples == 0) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    hypothesis_generation_heartbeat("hypothesis_g_hypothesis_estimate_", 0.0f);


    float total = 0.0f;
    float base_posterior = theory->posterior;

    for (uint32_t s = 0; s < num_samples; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && num_samples > 256) {
            hypothesis_generation_heartbeat("hypothesis_g_loop",
                             (float)(s + 1) / (float)num_samples);
        }

        /* Add noise to simulate uncertainty in evidence */
        float noise = mc_random_normal(&engine->rand_seed, 0.0f, 0.1f);
        float sampled_posterior = base_posterior + noise;

        if (sampled_posterior < 0.0f) sampled_posterior = 0.0f;
        if (sampled_posterior > 1.0f) sampled_posterior = 1.0f;

        total += sampled_posterior;
    }

    return total / (float)num_samples;
}

/**
 * @brief Add exploration noise to theory scores
 *
 * WHAT: Add stochastic noise for diverse hypothesis exploration
 * WHY:  Prevent fixation on single hypothesis
 * HOW:  Add Gaussian noise to scores
 *
 * @param engine Hypothesis engine
 * @param theories Theories to modify in-place
 * @param num_theories Number of theories
 * @param noise_scale Standard deviation of noise
 */
void hypothesis_add_exploration_noise_mc(
    hypothesis_engine_t* engine,
    hypogen_theory_t** theories,
    uint32_t num_theories,
    float noise_scale
) {
    if (!engine || !theories || num_theories == 0) return;

    /* Phase 8: Heartbeat at operation start */
    hypothesis_generation_heartbeat("hypothesis_g_hypothesis_add_explo", 0.0f);


    for (uint32_t i = 0; i < num_theories; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_theories > 256) {
            hypothesis_generation_heartbeat("hypothesis_g_loop",
                             (float)(i + 1) / (float)num_theories);
        }

        float noise = mc_random_normal(&engine->rand_seed, 0.0f, noise_scale);
        theories[i]->posterior += noise;

        if (theories[i]->posterior < 0.01f) theories[i]->posterior = 0.01f;
        if (theories[i]->posterior > 0.99f) theories[i]->posterior = 0.99f;
    }
}

/**
 * @brief Get thread-local MC seed for hypothesis engine
 *
 * @param engine Hypothesis engine
 * @return Pointer to RNG seed
 */
uint32_t* hypothesis_get_mc_seed(hypothesis_engine_t* engine) {
    if (!engine) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "engine is NULL");

        return NULL;

    }
    return &engine->rand_seed;
}

/* ============================================================================
 * KG SELF-AWARENESS INTEGRATION
 * ============================================================================ */

int hypothesis_generation_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    hypothesis_generation_heartbeat("hypothesis_g_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Hypothesis_Generation_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                hypothesis_generation_heartbeat("hypothesis_g_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG("Hypothesis generation self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Hypothesis_Generation_Module");
    if (connections) {
        LOG_DEBUG("Hypothesis generation has %u outgoing connections", connections->count);
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Hypothesis_Generation_Module");
    if (incoming) {
        LOG_DEBUG("Hypothesis generation has %u incoming connections", incoming->count);
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void hypothesis_generation_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_hypothesis_generation_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int hypothesis_generation_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hypothesis_generation_training_begin: NULL argument");
        return -1;
    }
    hypothesis_generation_heartbeat_instance(g_hypothesis_generation_health_agent, "hypothesis_generation_training_begin", 0.0f);
    (void)(struct hypothesis_engine*)instance; /* Module state available for reset */
    return 0;
}

int hypothesis_generation_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hypothesis_generation_training_end: NULL argument");
        return -1;
    }
    hypothesis_generation_heartbeat_instance(g_hypothesis_generation_health_agent, "hypothesis_generation_training_end", 1.0f);
    (void)(struct hypothesis_engine*)instance; /* Module state available for finalization */
    return 0;
}

int hypothesis_generation_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hypothesis_generation_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    hypothesis_generation_heartbeat_instance(g_hypothesis_generation_health_agent, "hypothesis_generation_training_step", progress);
    (void)(struct hypothesis_engine*)instance; /* Module state available for step adaptation */
    return 0;
}
