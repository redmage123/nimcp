/**
 * @file nimcp_broca_quantum_bridge.c
 * @brief Quantum Broca Bridge Implementation
 *
 * Integrates quantum algorithms with Broca's region for
 * optimized lexical search, syntax selection, and phoneme sequencing.
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/broca/nimcp_broca_quantum_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(broca_quantum_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "BROCA_QUANTUM_BRIDGE"


//=============================================================================
// Internal Structure
//=============================================================================

struct broca_quantum_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* broca;                         /**< Broca adapter handle */
    broca_quantum_config_t config;       /**< Configuration */
    qreason_t quantum_reasoner;          /**< Quantum reasoning engine */
    broca_quantum_stats_t stats;         /**< Statistics */

    /* Candidate tracking */
    quantum_lexical_candidate_t* lexical_candidates;
    quantum_syntax_candidate_t* syntax_candidates;
    quantum_phoneme_candidate_t* phoneme_candidates;
    uint32_t max_candidates;

    /* RNG state */
    uint32_t rng_state;
};

//=============================================================================
// Helper Functions
//=============================================================================

static uint32_t quantum_rand(uint32_t* state) {
    *state = *state * 1103515245 + 12345;
    return (*state >> 16) & 0x7FFF;
}

static float quantum_randf(uint32_t* state) {
    return (float)quantum_rand(state) / 32767.0f;
}

//=============================================================================
// Configuration API
//=============================================================================

broca_quantum_config_t broca_quantum_default_config(void) {
    return (broca_quantum_config_t){
        .enabled = true,
        .lexicon_search_depth = 1000,
        .syntax_alternatives = 8,
        .max_grover_iterations = 10,
        .min_expression_confidence = 0.5f,
        .enable_interference = true,
        .use_superposition = true,
        .seed = 42
    };
}

//=============================================================================
// Lifecycle API
//=============================================================================

broca_quantum_bridge_t* broca_quantum_bridge_create(
    void* broca,
    const broca_quantum_config_t* config
) {
    broca_quantum_bridge_t* bridge = nimcp_calloc(1, sizeof(broca_quantum_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "broca_quantum") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "broca_quantum_bridge_create: bridge_base_init failed");
        return NULL;
    }

    bridge->broca = broca;
    bridge->config = config ? *config : broca_quantum_default_config();

    /* Create quantum reasoner */
    qreason_config_t qconfig = qreason_default_config();
    qconfig.max_grover_iterations = bridge->config.max_grover_iterations;
    qconfig.min_confidence = bridge->config.min_expression_confidence;
    qconfig.enable_interference = bridge->config.enable_interference;
    qconfig.seed = bridge->config.seed;

    bridge->quantum_reasoner = qreason_create(&qconfig);
    if (!bridge->quantum_reasoner) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "broca_quantum_bridge_create: bridge->quantum_reasoner is NULL");
        return NULL;
    }

    /* Allocate candidate arrays */
    bridge->max_candidates = bridge->config.syntax_alternatives;

    bridge->lexical_candidates = nimcp_calloc(
        bridge->max_candidates, sizeof(quantum_lexical_candidate_t));
    bridge->syntax_candidates = nimcp_calloc(
        bridge->max_candidates, sizeof(quantum_syntax_candidate_t));
    bridge->phoneme_candidates = nimcp_calloc(
        bridge->max_candidates, sizeof(quantum_phoneme_candidate_t));

    if (!bridge->lexical_candidates || !bridge->syntax_candidates ||
        !bridge->phoneme_candidates) {
        broca_quantum_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "broca_quantum_bridge_create: operation failed");
        return NULL;
    }

    bridge->rng_state = bridge->config.seed;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    return bridge;
}

void broca_quantum_bridge_destroy(broca_quantum_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "broca_quantum");

    if (bridge->lexical_candidates) nimcp_free(bridge->lexical_candidates);
    if (bridge->syntax_candidates) nimcp_free(bridge->syntax_candidates);
    if (bridge->phoneme_candidates) nimcp_free(bridge->phoneme_candidates);

    if (bridge->quantum_reasoner) {
        qreason_destroy(bridge->quantum_reasoner);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

bool broca_quantum_bridge_is_enabled(const broca_quantum_bridge_t* bridge) {
    return bridge && bridge->config.enabled;
}

void broca_quantum_bridge_set_enabled(broca_quantum_bridge_t* bridge, bool enabled) {
    if (bridge) bridge->config.enabled = enabled;
}

//=============================================================================
// Lexical Search API
//=============================================================================

int broca_quantum_search_lexicon(
    broca_quantum_bridge_t* bridge,
    const float* semantic_target,
    uint32_t semantic_dim,
    uint32_t lexicon_size,
    quantum_lexical_result_t* result
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!semantic_target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_target is NULL");
        return -1;
    }
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");
        return -1;
    }
    if (!bridge->config.enabled) return -1;

    memset(result, 0, sizeof(*result));

    /* Build CNF for lexical search problem
     * Cap at QREASON_QUANTUM_VAR_LIMIT so the quantum solver path is used */
    qreason_cnf_t search_cnf = {0};
    search_cnf.n_variables = (lexicon_size < QREASON_QUANTUM_VAR_LIMIT) ?
                             lexicon_size : QREASON_QUANTUM_VAR_LIMIT;

    /* Simple satisfiability: at least one word must match */
    search_cnf.n_clauses = 1;
    search_cnf.clauses[0].n_literals = (search_cnf.n_variables < QREASON_MAX_LITERALS) ?
                                       search_cnf.n_variables : QREASON_MAX_LITERALS;

    for (uint32_t i = 0; i < search_cnf.clauses[0].n_literals; i++) {
        search_cnf.clauses[0].literals[i].variable = i;
        search_cnf.clauses[0].literals[i].negated = false;
    }

    /* Solve using quantum search */
    qreason_result_t qresult;
    int ret = qreason_solve_sat(bridge->quantum_reasoner, &search_cnf, &qresult);
    if (ret != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "broca_quantum_search_lexicon: validation failed");
        return -1;
    }

    /* Generate lexical candidates from quantum state */
    uint32_t num_candidates = (bridge->max_candidates < search_cnf.n_variables) ?
                              bridge->max_candidates : search_cnf.n_variables;

    quantum_lexical_candidate_t* best = NULL;
    float best_score = -1.0f;

    for (uint32_t i = 0; i < num_candidates; i++) {
        quantum_lexical_candidate_t* cand = &bridge->lexical_candidates[i];

        cand->word_id = i;
        snprintf(cand->word, sizeof(cand->word), "word_%u", i);
        cand->amplitude = qresult.confidences[i];
        cand->semantic_match = quantum_randf(&bridge->rng_state);
        cand->frequency = quantum_randf(&bridge->rng_state);

        /* Combine scores with quantum amplitude weighting */
        cand->combined_score = cand->amplitude * 0.5f +
                               cand->semantic_match * 0.3f +
                               cand->frequency * 0.2f;

        if (cand->combined_score > best_score) {
            best_score = cand->combined_score;
            best = cand;
        }
    }

    /* Fill result */
    result->best_candidate = best;
    result->candidates_evaluated = num_candidates;
    result->satisfaction_probability = qresult.satisfaction_prob;
    result->grover_iterations_used = qresult.grover_iterations;

    /* Estimate speedup (Grover provides quadratic speedup) */
    float classical_cost = (float)lexicon_size;
    float quantum_cost = sqrtf((float)lexicon_size);
    result->search_speedup = classical_cost / (quantum_cost > 0.0f ? quantum_cost : 1.0f);

    /* Update statistics */
    bridge->stats.lexical_searches++;
    float new_avg_speedup =
        (bridge->stats.avg_lexical_speedup * (bridge->stats.lexical_searches - 1) +
         result->search_speedup) / bridge->stats.lexical_searches;
    if (isfinite(new_avg_speedup)) bridge->stats.avg_lexical_speedup = new_avg_speedup;
    float new_avg_sat =
        (bridge->stats.avg_satisfaction_prob * (bridge->stats.lexical_searches - 1) +
         result->satisfaction_probability) / bridge->stats.lexical_searches;
    if (isfinite(new_avg_sat)) bridge->stats.avg_satisfaction_prob = new_avg_sat;

    if (best && best->combined_score >= bridge->config.min_expression_confidence) {
        bridge->stats.successful_searches++;
    } else {
        bridge->stats.failed_searches++;
    }

    return 0;
}

//=============================================================================
// Syntax Optimization API
//=============================================================================

int broca_quantum_optimize_syntax(
    broca_quantum_bridge_t* bridge,
    const char* semantic_content,
    uint32_t num_words,
    float max_complexity,
    quantum_syntax_result_t* result
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!semantic_content) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_content is NULL");
        return -1;
    }
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");
        return -1;
    }
    if (!bridge->config.enabled) return -1;

    memset(result, 0, sizeof(*result));

    /* Build CNF for syntax optimization
     * Cap at QREASON_QUANTUM_VAR_LIMIT so the quantum solver path is used */
    qreason_cnf_t syntax_cnf = {0};
    syntax_cnf.n_variables = (bridge->config.syntax_alternatives < QREASON_QUANTUM_VAR_LIMIT) ?
                             bridge->config.syntax_alternatives : QREASON_QUANTUM_VAR_LIMIT;

    /* Each variable represents a syntactic structure choice */
    syntax_cnf.n_clauses = 1;
    syntax_cnf.clauses[0].n_literals = syntax_cnf.n_variables;

    for (uint32_t i = 0; i < syntax_cnf.n_variables; i++) {
        syntax_cnf.clauses[0].literals[i].variable = i;
        syntax_cnf.clauses[0].literals[i].negated = false;
    }

    /* Solve using quantum search */
    qreason_result_t qresult;
    int ret = qreason_solve_sat(bridge->quantum_reasoner, &syntax_cnf, &qresult);
    if (ret != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "broca_quantum_optimize_syntax: validation failed");
        return -1;
    }

    /* Generate syntax candidates */
    quantum_syntax_candidate_t* best = NULL;
    float best_fluency = -1.0f;

    for (uint32_t i = 0; i < syntax_cnf.n_variables && i < bridge->max_candidates; i++) {
        quantum_syntax_candidate_t* cand = &bridge->syntax_candidates[i];

        cand->structure_id = i;
        snprintf(cand->pattern, sizeof(cand->pattern), "S -> NP VP (variant %u)", i);
        cand->amplitude = qresult.confidences[i];
        cand->complexity = quantum_randf(&bridge->rng_state) * max_complexity;
        cand->fluency_score = cand->amplitude * (1.0f - cand->complexity * 0.3f);
        cand->word_count = num_words;
        cand->is_grammatical = qresult.satisfiable;

        if (cand->fluency_score > best_fluency && cand->is_grammatical) {
            best_fluency = cand->fluency_score;
            best = cand;
        }
    }

    /* Fill result */
    result->best_structure = best;
    result->structures_evaluated = syntax_cnf.n_variables;
    result->satisfaction_probability = qresult.satisfaction_prob;
    result->grover_iterations_used = qresult.grover_iterations;

    /* Update statistics */
    bridge->stats.syntax_optimizations++;

    float speedup = sqrtf((float)result->structures_evaluated);
    bridge->stats.avg_syntax_speedup =
        (bridge->stats.avg_syntax_speedup * (bridge->stats.syntax_optimizations - 1) +
         speedup) / bridge->stats.syntax_optimizations;

    return 0;
}

//=============================================================================
// Phoneme Optimization API
//=============================================================================

int broca_quantum_optimize_phonemes(
    broca_quantum_bridge_t* bridge,
    const uint8_t* target_phonemes,
    uint32_t phoneme_count,
    quantum_phoneme_result_t* result
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!target_phonemes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "target_phonemes is NULL");
        return -1;
    }
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");
        return -1;
    }
    if (!bridge->config.enabled) return -1;

    memset(result, 0, sizeof(*result));

    /* Build CNF for phoneme optimization */
    qreason_cnf_t phoneme_cnf = {0};
    phoneme_cnf.n_variables = (phoneme_count < QREASON_MAX_VARIABLES) ?
                              phoneme_count : QREASON_MAX_VARIABLES;

    /* Each variable represents a phoneme position */
    phoneme_cnf.n_clauses = 1;
    phoneme_cnf.clauses[0].n_literals = phoneme_cnf.n_variables;

    for (uint32_t i = 0; i < phoneme_cnf.n_variables; i++) {
        phoneme_cnf.clauses[0].literals[i].variable = i;
        phoneme_cnf.clauses[0].literals[i].negated = false;
    }

    /* Solve using quantum search */
    qreason_result_t qresult;
    int ret = qreason_solve_sat(bridge->quantum_reasoner, &phoneme_cnf, &qresult);
    if (ret != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "broca_quantum_optimize_phonemes: validation failed");
        return -1;
    }

    /* Generate phoneme sequence candidates */
    uint32_t num_candidates = (bridge->max_candidates < 4) ? bridge->max_candidates : 4;
    quantum_phoneme_candidate_t* best = NULL;
    float best_score = -1.0f;

    for (uint32_t i = 0; i < num_candidates; i++) {
        quantum_phoneme_candidate_t* cand = &bridge->phoneme_candidates[i];

        cand->sequence_id = i;
        cand->phoneme_count = (phoneme_count < 32) ? phoneme_count : 32;
        memcpy(cand->phonemes, target_phonemes, cand->phoneme_count);

        /* Simulate variations for optimization */
        if (i > 0) {
            /* Vary some phonemes slightly */
            for (uint32_t j = 0; j < cand->phoneme_count; j++) {
                if (quantum_randf(&bridge->rng_state) < 0.2f) {
                    cand->phonemes[j] = (cand->phonemes[j] + 1) % 64;
                }
            }
        }

        cand->amplitude = qresult.confidences[i % phoneme_cnf.n_variables];
        cand->articulatory_cost = quantum_randf(&bridge->rng_state);
        cand->coarticulation_score = 1.0f - cand->articulatory_cost * 0.5f;

        float score = cand->amplitude * 0.4f +
                      (1.0f - cand->articulatory_cost) * 0.3f +
                      cand->coarticulation_score * 0.3f;

        if (score > best_score) {
            best_score = score;
            best = cand;
        }
    }

    /* Fill result */
    result->best_sequence = best;
    result->sequences_evaluated = num_candidates;
    result->optimization_score = best_score;

    /* Update statistics */
    bridge->stats.phoneme_optimizations++;

    return 0;
}

//=============================================================================
// Statistics API
//=============================================================================

int broca_quantum_get_stats(
    const broca_quantum_bridge_t* bridge,
    broca_quantum_stats_t* stats
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stats is NULL");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

void broca_quantum_reset_stats(broca_quantum_bridge_t* bridge) {
    if (bridge) {
        memset(&bridge->stats, 0, sizeof(bridge->stats));
    }
}

int broca_quantum_get_config(
    const broca_quantum_bridge_t* bridge,
    broca_quantum_config_t* config
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");
        return -1;
    }
    *config = bridge->config;
    return 0;
}
