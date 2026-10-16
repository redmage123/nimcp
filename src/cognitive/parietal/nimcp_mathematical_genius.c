/**
 * @file nimcp_mathematical_genius.c
 * @brief Mathematical Genius Module Implementation
 *
 * Implements the core mathematical genius framework that coordinates
 * different genius modes (Gauss, Newton, Erdős, etc.) for advanced
 * mathematical reasoning.
 *
 * @author NIMCP Team
 * @version 2.6.3
 */

#include "cognitive/parietal/nimcp_mathematical_genius.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "async/nimcp_bio_router.h"

#define LOG_MODULE "MATH_GENIUS"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(mathematical_genius, MESH_ADAPTER_CATEGORY_COGNITIVE)

#include <math.h>
#include <string.h>
#include <stdlib.h>

/* ============================================================================
 * Forward Declarations for Mode-Specific Functions
 * ============================================================================ */

/* Defined in nimcp_genius_gauss.c */
extern nimcp_error_t genius_gauss_analyze_impl(
    mathematical_genius_t* genius,
    const math_problem_t* problem,
    genius_result_t* result);

/* Defined in nimcp_genius_newton.c */
extern nimcp_error_t genius_newton_analyze_impl(
    mathematical_genius_t* genius,
    const math_problem_t* problem,
    genius_result_t* result);

/* Defined in nimcp_genius_erdos.c */
extern nimcp_error_t genius_erdos_analyze_impl(
    mathematical_genius_t* genius,
    const math_problem_t* problem,
    genius_result_t* result);

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

/**
 * @brief Internal structure for mathematical genius
 */
struct mathematical_genius {
    /* Configuration */
    genius_config_t config;

    /* Current mode */
    genius_mode_t active_mode;

    /* Mode capabilities */
    genius_mode_capabilities_t capabilities[GENIUS_MODE_COUNT];

    /* Parietal lobe integrations */
    void* number_sense;          /* Number sense module */
    void* spatial;               /* Spatial reasoning */
    void* equation_engine;       /* Equation manipulation */
    void* pattern_detector;      /* Pattern detection */

    /* External integrations */
    void* game_theory;
    void* quantum_engine;
    void* hypergraph;
    void* fep_system;

    /* Modulation state */
    float inflammation_level;
    float fatigue_level;
    float atp_level;
    float effective_creativity;
    float effective_rigor;

    /* Statistics */
    genius_stats_t stats;

    /* Bio-async */
    uint16_t bio_module_id;
    const char* module_name;
    bool bio_async_enabled;
    bio_module_context_t bio_ctx;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* RNG state */
    uint64_t rng_state;

    /* State flags */
    bool initialized;
};

/* ============================================================================
 * RNG Helpers
 * ============================================================================ */

static uint64_t genius_xorshift64(uint64_t* state) {
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

static float genius_random_uniform(mathematical_genius_t* genius) {
    return (float)(genius_xorshift64(&genius->rng_state) >> 11) *
           (1.0f / 9007199254740992.0f);
}

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Select best mode for a problem based on domain
 */
static genius_mode_t select_mode_for_problem(
    mathematical_genius_t* genius,
    const math_problem_t* problem) {

    switch (problem->domain) {
        case GENIUS_DOMAIN_NUMBER_THEORY:
        case GENIUS_DOMAIN_ALGEBRA:
        case GENIUS_DOMAIN_STATISTICS:
            return GENIUS_MODE_GAUSS;

        case GENIUS_DOMAIN_CALCULUS:
        case GENIUS_DOMAIN_DIFFERENTIAL_EQ:
        case GENIUS_DOMAIN_PHYSICS:
            return GENIUS_MODE_NEWTON;

        case GENIUS_DOMAIN_COMBINATORICS:
        case GENIUS_DOMAIN_GRAPH_THEORY:
        case GENIUS_DOMAIN_PROBABILITY:
            return GENIUS_MODE_ERDOS;

        case GENIUS_DOMAIN_ANALYSIS:
        case GENIUS_DOMAIN_TOPOLOGY:
            return GENIUS_MODE_EULER;

        case GENIUS_DOMAIN_INFINITE_SERIES:
            return GENIUS_MODE_RAMANUJAN;

        default:
            return GENIUS_MODE_ADAPTIVE;
    }
}

/**
 * @brief Apply modulation effects to cognitive parameters
 */
static void apply_modulation(mathematical_genius_t* genius) {
    /* Inflammation reduces creativity */
    genius->effective_creativity = genius->config.creativity_level *
                                   (1.0f - 0.5f * genius->inflammation_level);

    /* Fatigue reduces both creativity and rigor */
    float fatigue_factor = 1.0f - 0.3f * genius->fatigue_level;
    genius->effective_creativity *= fatigue_factor;
    genius->effective_rigor = genius->config.rigor_level * fatigue_factor;

    /* Low ATP reduces overall cognitive capacity */
    float atp_factor = 0.5f + 0.5f * genius->atp_level;
    genius->effective_creativity *= atp_factor;
    genius->effective_rigor *= atp_factor;

    /* Clamp values */
    if (genius->effective_creativity < 0.1f) genius->effective_creativity = 0.1f;
    if (genius->effective_creativity > 1.0f) genius->effective_creativity = 1.0f;
    if (genius->effective_rigor < 0.1f) genius->effective_rigor = 0.1f;
    if (genius->effective_rigor > 1.0f) genius->effective_rigor = 1.0f;
}

/**
 * @brief Initialize capabilities for a mode
 */
static void init_mode_capabilities(genius_mode_capabilities_t* caps, genius_mode_t mode) {
    memset(caps, 0, sizeof(genius_mode_capabilities_t));
    caps->mode = mode;
    caps->atp_cost_multiplier = 1.0f;

    switch (mode) {
        case GENIUS_MODE_GAUSS:
            caps->name = "Gauss";
            caps->description = "Number theory, patterns, statistics";
            caps->domain_strengths[GENIUS_DOMAIN_NUMBER_THEORY] = 0.95f;
            caps->domain_strengths[GENIUS_DOMAIN_ALGEBRA] = 0.85f;
            caps->domain_strengths[GENIUS_DOMAIN_STATISTICS] = 0.90f;
            caps->creativity = 0.85f;
            caps->rigor = 0.90f;
            caps->intuition = 0.85f;
            caps->computation = 0.95f;
            caps->abstraction = 0.80f;
            caps->brain_regions = "intraparietal_sulcus,prefrontal";
            break;

        case GENIUS_MODE_NEWTON:
            caps->name = "Newton";
            caps->description = "Calculus, physics, analysis";
            caps->domain_strengths[GENIUS_DOMAIN_CALCULUS] = 0.98f;
            caps->domain_strengths[GENIUS_DOMAIN_PHYSICS] = 0.95f;
            caps->domain_strengths[GENIUS_DOMAIN_DIFFERENTIAL_EQ] = 0.95f;
            caps->creativity = 0.90f;
            caps->rigor = 0.85f;
            caps->intuition = 0.90f;
            caps->computation = 0.90f;
            caps->abstraction = 0.85f;
            caps->brain_regions = "parietal,motor_planning";
            break;

        case GENIUS_MODE_ERDOS:
            caps->name = "Erdős";
            caps->description = "Combinatorics, graph theory, probabilistic";
            caps->domain_strengths[GENIUS_DOMAIN_COMBINATORICS] = 0.98f;
            caps->domain_strengths[GENIUS_DOMAIN_GRAPH_THEORY] = 0.95f;
            caps->domain_strengths[GENIUS_DOMAIN_PROBABILITY] = 0.90f;
            caps->creativity = 0.95f;
            caps->rigor = 0.80f;
            caps->intuition = 0.95f;
            caps->computation = 0.75f;
            caps->collaboration = 0.99f;
            caps->brain_regions = "prefrontal,social_cognition";
            break;

        case GENIUS_MODE_EULER:
            caps->name = "Euler";
            caps->description = "Universal connections, analysis";
            caps->domain_strengths[GENIUS_DOMAIN_ANALYSIS] = 0.95f;
            caps->domain_strengths[GENIUS_DOMAIN_NUMBER_THEORY] = 0.90f;
            caps->domain_strengths[GENIUS_DOMAIN_TOPOLOGY] = 0.85f;
            caps->domain_strengths[GENIUS_DOMAIN_GRAPH_THEORY] = 0.90f;
            caps->creativity = 0.90f;
            caps->rigor = 0.90f;
            caps->intuition = 0.90f;
            caps->abstraction = 0.95f;
            caps->brain_regions = "multimodal_integration";
            break;

        case GENIUS_MODE_RAMANUJAN:
            caps->name = "Ramanujan";
            caps->description = "Infinite series, partitions, intuition";
            caps->domain_strengths[GENIUS_DOMAIN_INFINITE_SERIES] = 0.99f;
            caps->domain_strengths[GENIUS_DOMAIN_NUMBER_THEORY] = 0.95f;
            caps->domain_strengths[GENIUS_DOMAIN_ANALYSIS] = 0.90f;
            caps->creativity = 0.99f;
            caps->rigor = 0.70f;
            caps->intuition = 0.99f;
            caps->computation = 0.95f;
            caps->brain_regions = "default_mode_network";
            break;

        default:
            /* Adaptive mode has balanced capabilities */
            caps->name = "Adaptive";
            caps->description = "Auto-select based on problem";
            for (int i = 0; i < GENIUS_DOMAIN_COUNT; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && GENIUS_DOMAIN_COUNT > 256) {
                    mathematical_genius_heartbeat("mathematical_loop",
                                     (float)(i + 1) / (float)GENIUS_DOMAIN_COUNT);
                }

                caps->domain_strengths[i] = 0.80f;
            }
            caps->creativity = 0.80f;
            caps->rigor = 0.80f;
            caps->intuition = 0.80f;
            caps->computation = 0.80f;
            caps->abstraction = 0.80f;
            caps->collaboration = 0.80f;
            caps->brain_regions = "whole_brain";
            break;
    }
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

NIMCP_API mathematical_genius_t* genius_create(const genius_config_t* config) {
    mathematical_genius_t* genius = nimcp_calloc(1, sizeof(mathematical_genius_t));
    if (!genius) {
        NIMCP_LOG_ERROR("Failed to allocate mathematical genius");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate genius");

        return NULL;
    }

    /* Apply configuration */
    if (config) {
        memcpy(&genius->config, config, sizeof(genius_config_t));
    } else {
        genius_get_default_config(&genius->config);
    }

    /* Create mutex */
    mutex_attr_t attr;
    attr.type = MUTEX_TYPE_NORMAL;
    genius->mutex = nimcp_mutex_create(&attr);
    if (!genius->mutex) {
        nimcp_free(genius);
        genius = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "genius_create: genius->mutex is NULL");
        return NULL;
    }

    /* Initialize mode capabilities */
    for (int i = 0; i < GENIUS_MODE_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && GENIUS_MODE_COUNT > 256) {
            mathematical_genius_heartbeat("mathematical_loop",
                             (float)(i + 1) / (float)GENIUS_MODE_COUNT);
        }

        init_mode_capabilities(&genius->capabilities[i], (genius_mode_t)i);
    }

    /* Initialize RNG */
    genius->rng_state = (uint64_t)nimcp_time_monotonic_us() ^ 0xFEDCBA9876543210ULL;

    /* Set default modulation */
    genius->inflammation_level = 0.0f;
    genius->fatigue_level = 0.0f;
    genius->atp_level = 1.0f;
    apply_modulation(genius);

    genius->active_mode = genius->config.default_mode;
    genius->bio_module_id = BIO_MODULE_MATHEMATICAL_GENIUS;
    genius->initialized = true;

    NIMCP_LOG_INFO("Created mathematical genius with mode %d", genius->active_mode);

    return genius;
}

NIMCP_API void genius_destroy(mathematical_genius_t* genius) {
    if (!genius) return;

    if (genius->bio_async_enabled) {
        genius_unregister_bio_async(genius);
    }

    if (genius->mutex) {
        nimcp_mutex_destroy(genius->mutex);
    }

    nimcp_free(genius);
    genius = NULL;

    NIMCP_LOG_DEBUG("Destroyed mathematical genius");
}

NIMCP_API nimcp_error_t genius_reset(mathematical_genius_t* genius) {
    if (!genius) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(genius->mutex);

    memset(&genius->stats, 0, sizeof(genius_stats_t));
    genius->inflammation_level = 0.0f;
    genius->fatigue_level = 0.0f;
    genius->atp_level = 1.0f;
    apply_modulation(genius);

    genius->active_mode = genius->config.default_mode;

    nimcp_mutex_unlock(genius->mutex);

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t genius_get_default_config(genius_config_t* config) {
    if (!config) return NIMCP_ERROR_INVALID_PARAM;

    memset(config, 0, sizeof(genius_config_t));

    config->default_mode = GENIUS_MODE_ADAPTIVE;
    config->creativity_level = GENIUS_DEFAULT_CREATIVITY;
    config->rigor_level = GENIUS_DEFAULT_RIGOR;
    config->collaboration_weight = 0.5f;

    config->enable_quantum_search = true;
    config->enable_pattern_mining = true;
    config->enable_analogy_engine = true;
    config->max_proof_depth = GENIUS_MAX_PROOF_STEPS;
    config->max_conjecture_candidates = GENIUS_MAX_INSIGHTS;

    config->enable_fep_integration = true;
    config->enable_bio_async = true;

    config->inflammation_sensitivity = 0.5f;
    config->fatigue_sensitivity = 0.5f;
    config->atp_sensitivity = 0.5f;

    config->max_thinking_time_ms = 10000;
    config->max_atp_budget = 100.0f;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Problem Solving Interface
 * ============================================================================ */

NIMCP_API nimcp_error_t genius_solve_problem(
    mathematical_genius_t* genius,
    const math_problem_t* problem,
    genius_result_t* result) {

    if (!genius || !problem || !result) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    {
        nimcp_mutex_lock(genius->mutex);

        uint64_t start_time = nimcp_time_monotonic_us();

        /* Initialize result */
        genius_result_init(result);

        /* Select mode */
        genius_mode_t mode = (genius->config.default_mode == GENIUS_MODE_ADAPTIVE)
            ? select_mode_for_problem(genius, problem)
            : genius->config.default_mode;

        result->mode_used = mode;
        genius->active_mode = mode;

        /* Apply current modulation */
        apply_modulation(genius);

        /* Dispatch to mode-specific analysis */
        nimcp_error_t err = NIMCP_SUCCESS;
        switch (mode) {
            case GENIUS_MODE_GAUSS:
                err = genius_gauss_analyze_impl(genius, problem, result);
                break;
            case GENIUS_MODE_NEWTON:
                err = genius_newton_analyze_impl(genius, problem, result);
                break;
            case GENIUS_MODE_ERDOS:
                err = genius_erdos_analyze_impl(genius, problem, result);
                break;
            default:
                /* Use Gauss as default for now */
                err = genius_gauss_analyze_impl(genius, problem, result);
                break;
        }

        result->thinking_time_us = nimcp_time_monotonic_us() - start_time;

        /* Estimate ATP consumed */
        result->atp_consumed = (float)result->thinking_time_us * 0.001f *
                               (1.0f + genius->effective_creativity);

        /* Update statistics */
        genius->stats.problems_attempted++;
        if (result->solved) {
            genius->stats.problems_solved++;
        }
        genius->stats.mode_usage[mode]++;
        genius->stats.total_thinking_time_us += result->thinking_time_us;
        genius->stats.total_atp_consumed += result->atp_consumed;

        /* Update mode success rate */
        if (genius->stats.mode_usage[mode] > 0) {
            genius->stats.mode_success_rate[mode] =
                (genius->stats.mode_success_rate[mode] *
                 (genius->stats.mode_usage[mode] - 1) +
                 (result->solved ? 1.0f : 0.0f)) /
                genius->stats.mode_usage[mode];
        }

        nimcp_mutex_unlock(genius->mutex);

        return err;

    }

    return NIMCP_SUCCESS;
}

NIMCP_API uint32_t genius_generate_conjectures(
    mathematical_genius_t* genius,
    genius_domain_t domain,
    const void* context,
    conjecture_t* conjectures,
    uint32_t max_conjectures) {

    if (!genius || !conjectures || max_conjectures == 0) return 0;

    nimcp_mutex_lock(genius->mutex);

    uint32_t count = 0;

    /* Generate conjectures based on patterns and domain */
    for (uint32_t i = 0; i < max_conjectures && i < 5; i++) {
        conjectures[i].id = i;
        conjectures[i].domain = domain;
        conjectures[i].confidence = 0.5f + 0.3f * genius_random_uniform(genius);
        conjectures[i].novelty = genius_random_uniform(genius);
        conjectures[i].importance = genius_random_uniform(genius);
        conjectures[i].is_verified = false;
        conjectures[i].counter_example_count = 0;
        conjectures[i].generating_mode = genius->active_mode;
        conjectures[i].generated_time_us = nimcp_time_monotonic_us();
        conjectures[i].statement = NULL;
        conjectures[i].supporting_evidence = NULL;
        count++;
    }

    genius->stats.conjectures_generated += count;

    nimcp_mutex_unlock(genius->mutex);

    return count;
}

NIMCP_API nimcp_error_t genius_prove_theorem(
    mathematical_genius_t* genius,
    const char* theorem,
    uint32_t max_depth,
    proof_trace_t* trace) {

    if (!genius || !theorem || !trace) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    {
        nimcp_mutex_lock(genius->mutex);

        uint64_t start_time = nimcp_time_monotonic_us();

        /* Initialize trace */
        genius_proof_trace_init(trace, max_depth);

        /* Simulate proof search (placeholder implementation) */
        bool found = false;
        uint32_t depth = 0;

        while (depth < max_depth && !found) {
            /* Check for proof completion (placeholder) */
            if (genius_random_uniform(genius) < 0.05f) {
                found = true;
            }
            depth++;
        }

        trace->is_complete = found;
        trace->is_valid = found;
        trace->num_steps = depth;
        trace->construction_time_us = nimcp_time_monotonic_us() - start_time;
        trace->constructing_mode = genius->active_mode;

        if (found) {
            trace->elegance_score = 0.5f + 0.5f * genius_random_uniform(genius);
            trace->difficulty_score = (float)depth / max_depth;
            genius->stats.proofs_constructed++;
        }

        nimcp_mutex_unlock(genius->mutex);

        return found ? NIMCP_SUCCESS : NIMCP_ERROR_NOT_FOUND;

    }

    return NIMCP_SUCCESS;
}

NIMCP_API uint32_t genius_find_analogies(
    mathematical_genius_t* genius,
    genius_domain_t source_domain,
    genius_domain_t target_domain,
    genius_analogy_result_t* analogies,
    uint32_t max_analogies) {

    if (!genius || !analogies || max_analogies == 0) return 0;

    nimcp_mutex_lock(genius->mutex);

    uint32_t count = 0;

    /* Generate analogies (placeholder) */
    for (uint32_t i = 0; i < max_analogies && i < 3; i++) {
        analogies[i].source = NULL;
        analogies[i].target = NULL;
        analogies[i].mapping = NULL;
        analogies[i].source_domain = source_domain;
        analogies[i].target_domain = target_domain;
        analogies[i].similarity = 0.5f + 0.4f * genius_random_uniform(genius);
        analogies[i].usefulness = 0.5f + 0.4f * genius_random_uniform(genius);
        count++;
    }

    nimcp_mutex_unlock(genius->mutex);

    return count;
}

/* ============================================================================
 * Mode-Specific Entry Points
 * ============================================================================ */

NIMCP_API nimcp_error_t genius_gauss_analyze(
    mathematical_genius_t* genius,
    const math_problem_t* problem,
    genius_result_t* result) {

    if (!genius || !problem || !result) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    genius->active_mode = GENIUS_MODE_GAUSS;
    return genius_solve_problem(genius, problem, result);
}

NIMCP_API nimcp_error_t genius_newton_analyze(
    mathematical_genius_t* genius,
    const math_problem_t* problem,
    genius_result_t* result) {

    if (!genius || !problem || !result) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    genius->active_mode = GENIUS_MODE_NEWTON;
    return genius_solve_problem(genius, problem, result);
}

NIMCP_API nimcp_error_t genius_erdos_analyze(
    mathematical_genius_t* genius,
    const math_problem_t* problem,
    genius_result_t* result) {

    if (!genius || !problem || !result) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    genius->active_mode = GENIUS_MODE_ERDOS;
    return genius_solve_problem(genius, problem, result);
}

/* ============================================================================
 * Collaboration Functions
 * ============================================================================ */

NIMCP_API nimcp_error_t genius_collaborate(
    mathematical_genius_t** geniuses,
    uint32_t num_geniuses,
    const math_problem_t* problem,
    genius_result_t* result) {

    if (!geniuses || num_geniuses == 0 || !problem || !result) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Each genius contributes insights */
    genius_result_init(result);

    genius_result_t* individual_results = nimcp_calloc(num_geniuses,
                                                        sizeof(genius_result_t));
    if (!individual_results) return NIMCP_ERROR_NO_MEMORY;

    float best_score = 0.0f;
    uint32_t best_idx = 0;

    for (uint32_t i = 0; i < num_geniuses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_geniuses > 256) {
            mathematical_genius_heartbeat("mathematical_loop",
                             (float)(i + 1) / (float)num_geniuses);
        }

        genius_solve_problem(geniuses[i], problem, &individual_results[i]);

        float score = individual_results[i].elegance_score +
                      individual_results[i].novelty_score;
        if (score > best_score) {
            best_score = score;
            best_idx = i;
        }
    }

    /* Combine results (simplified - take best and aggregate insights) */
    memcpy(result, &individual_results[best_idx], sizeof(genius_result_t));

    /* Aggregate statistics */
    result->num_insights = 0;
    for (uint32_t i = 0; i < num_geniuses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_geniuses > 256) {
            mathematical_genius_heartbeat("mathematical_loop",
                             (float)(i + 1) / (float)num_geniuses);
        }

        result->num_insights += individual_results[i].num_insights;
    }

    for (uint32_t i = 0; i < num_geniuses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_geniuses > 256) {
            mathematical_genius_heartbeat("mathematical_loop",
                             (float)(i + 1) / (float)num_geniuses);
        }

        geniuses[i]->stats.collaborations++;
    }

    nimcp_free(individual_results);
    individual_results = NULL;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Integration Functions
 * ============================================================================ */

NIMCP_API nimcp_error_t genius_link_game_theory(
    mathematical_genius_t* genius,
    void* game_theory) {

    if (!genius) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(genius->mutex);
    genius->game_theory = game_theory;
    nimcp_mutex_unlock(genius->mutex);

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t genius_link_quantum_engine(
    mathematical_genius_t* genius,
    void* quantum_engine) {

    if (!genius) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(genius->mutex);
    genius->quantum_engine = quantum_engine;
    nimcp_mutex_unlock(genius->mutex);

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t genius_link_hypergraph(
    mathematical_genius_t* genius,
    void* hypergraph) {

    if (!genius) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(genius->mutex);
    genius->hypergraph = hypergraph;
    nimcp_mutex_unlock(genius->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Modulation
 * ============================================================================ */

NIMCP_API nimcp_error_t genius_modulate_inflammation(
    mathematical_genius_t* genius,
    float inflammation) {

    if (!genius) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(genius->mutex);
    genius->inflammation_level = inflammation;
    apply_modulation(genius);
    nimcp_mutex_unlock(genius->mutex);

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t genius_modulate_fatigue(
    mathematical_genius_t* genius,
    float fatigue) {

    if (!genius) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(genius->mutex);
    genius->fatigue_level = fatigue;
    apply_modulation(genius);
    nimcp_mutex_unlock(genius->mutex);

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t genius_modulate_atp(
    mathematical_genius_t* genius,
    float atp_level) {

    if (!genius) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(genius->mutex);
    genius->atp_level = atp_level;
    apply_modulation(genius);
    nimcp_mutex_unlock(genius->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

NIMCP_API nimcp_error_t genius_register_bio_async(
    mathematical_genius_t* genius) {

    if (!genius) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(genius->mutex);

    if (genius->bio_async_enabled) {
        nimcp_mutex_unlock(genius->mutex);
        return NIMCP_SUCCESS;
    }

    if (!bio_router_is_initialized()) {
        nimcp_mutex_unlock(genius->mutex);
        return NIMCP_SUCCESS;
    }

    bio_module_info_t info = {
        .module_id = genius->bio_module_id,
        .module_name = genius->module_name,
        .inbox_capacity = 32,
        .user_data = genius
    };

    genius->bio_ctx = bio_router_register_module(&info);
    if (genius->bio_ctx) {
        genius->bio_async_enabled = true;
    }

    nimcp_mutex_unlock(genius->mutex);

    NIMCP_LOG_DEBUG("Mathematical genius registered with bio-async");
    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t genius_unregister_bio_async(
    mathematical_genius_t* genius) {

    if (!genius) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(genius->mutex);

    if (genius->bio_async_enabled && genius->bio_ctx) {
        bio_router_unregister_module(genius->bio_ctx);
        genius->bio_ctx = NULL;
        genius->bio_async_enabled = false;
    }

    nimcp_mutex_unlock(genius->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics and Diagnostics
 * ============================================================================ */

NIMCP_API nimcp_error_t genius_get_stats(
    const mathematical_genius_t* genius,
    genius_stats_t* stats) {

    if (!genius || !stats) return NIMCP_ERROR_INVALID_PARAM;

    memcpy(stats, &genius->stats, sizeof(genius_stats_t));

    /* Compute averages */
    if (genius->stats.problems_attempted > 0) {
        stats->avg_thinking_time_us = genius->stats.total_thinking_time_us /
                                       genius->stats.problems_attempted;
    }

    return NIMCP_SUCCESS;
}

NIMCP_API void genius_print_diagnostics(const mathematical_genius_t* genius) {
    if (!genius) return;

    NIMCP_LOG_INFO("=== Mathematical Genius Diagnostics ===");
    NIMCP_LOG_INFO("Active mode: %d", genius->active_mode);
    NIMCP_LOG_INFO("Problems: %lu/%lu solved",
                   genius->stats.problems_solved,
                   genius->stats.problems_attempted);
    NIMCP_LOG_INFO("Proofs constructed: %lu", genius->stats.proofs_constructed);
    NIMCP_LOG_INFO("Conjectures: %lu", genius->stats.conjectures_generated);
    NIMCP_LOG_INFO("Insights: %lu", genius->stats.insights_discovered);
    NIMCP_LOG_INFO("Collaborations: %lu", genius->stats.collaborations);
    NIMCP_LOG_INFO("Effective creativity: %.3f", genius->effective_creativity);
    NIMCP_LOG_INFO("Effective rigor: %.3f", genius->effective_rigor);
    NIMCP_LOG_INFO("ATP consumed: %.2f", genius->stats.total_atp_consumed);
}

/* ============================================================================
 * Result Management
 * ============================================================================ */

NIMCP_API nimcp_error_t genius_result_init(genius_result_t* result) {
    if (!result) return NIMCP_ERROR_INVALID_PARAM;

    memset(result, 0, sizeof(genius_result_t));
    return NIMCP_SUCCESS;
}

NIMCP_API void genius_result_cleanup(genius_result_t* result) {
    if (!result) return;

    nimcp_free(result->detected_patterns);
    nimcp_free(result->conjectures);
    nimcp_free(result->insights);
    nimcp_free(result->analogies);

    if (result->proofs) {
        for (uint32_t i = 0; i < result->num_proofs; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && result->num_proofs > 256) {
                mathematical_genius_heartbeat("mathematical_loop",
                                 (float)(i + 1) / (float)result->num_proofs);
            }

            genius_proof_trace_cleanup(&result->proofs[i]);
        }
        nimcp_free(result->proofs);
    }

    memset(result, 0, sizeof(genius_result_t));
}

NIMCP_API nimcp_error_t genius_proof_trace_init(
    proof_trace_t* trace,
    uint32_t capacity) {

    if (!trace) return NIMCP_ERROR_INVALID_PARAM;

    memset(trace, 0, sizeof(proof_trace_t));

    if (capacity > 0) {
        trace->steps = nimcp_calloc(capacity, sizeof(genius_proof_step_t));
        if (!trace->steps) return NIMCP_ERROR_NO_MEMORY;
        trace->capacity = capacity;
    }

    return NIMCP_SUCCESS;
}

NIMCP_API void genius_proof_trace_cleanup(proof_trace_t* trace) {
    if (!trace) return;

    if (trace->steps) {
        for (uint32_t i = 0; i < trace->num_steps; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && trace->num_steps > 256) {
                mathematical_genius_heartbeat("mathematical_loop",
                                 (float)(i + 1) / (float)trace->num_steps);
            }

            nimcp_free(trace->steps[i].statement);
            nimcp_free(trace->steps[i].justification);
            nimcp_free(trace->steps[i].premises);
        }
        nimcp_free(trace->steps);
    }

    memset(trace, 0, sizeof(proof_trace_t));
}

/* ============================================================================
 * Gauss Mode Functions (stubs - full implementation in nimcp_genius_gauss.c)
 * ============================================================================ */

NIMCP_API nimcp_error_t genius_gauss_discover_pattern(
    mathematical_genius_t* genius,
    const int64_t* sequence,
    uint32_t length,
    conjecture_t* conjecture) {

    if (!genius || !sequence || !conjecture) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Delegate to gauss implementation */
    extern nimcp_error_t genius_gauss_discover_pattern_impl(
        mathematical_genius_t*, const int64_t*, uint32_t, conjecture_t*);
    return genius_gauss_discover_pattern_impl(genius, sequence, length, conjecture);
}

NIMCP_API bool genius_gauss_is_prime(
    mathematical_genius_t* genius,
    uint64_t n,
    float certainty) {

    if (!genius || n < 2) {
        return false;
    }

    /* Simple primality test */
    if (n == 2) return true;
    if (n % 2 == 0) {
        return false;
    }

    uint64_t limit = (uint64_t)sqrtf((float)n) + 1;
    for (uint64_t i = 3; i <= limit; i += 2) {
        if (n % i == 0) {
            return false;
        }
    }

    return true;
}

NIMCP_API uint32_t genius_gauss_factor(
    mathematical_genius_t* genius,
    uint64_t n,
    uint64_t* factors,
    uint32_t max_factors) {

    if (!genius || !factors || max_factors == 0 || n < 2) return 0;

    uint32_t count = 0;
    uint64_t temp = n;

    /* Factor out 2s */
    while (temp % 2 == 0 && count < max_factors) {
        factors[count++] = 2;
        temp /= 2;
    }

    /* Factor odd numbers */
    for (uint64_t i = 3; i * i <= temp && count < max_factors; i += 2) {
        while (temp % i == 0 && count < max_factors) {
            factors[count++] = i;
            temp /= i;
        }
    }

    /* Remaining prime */
    if (temp > 1 && count < max_factors) {
        factors[count++] = temp;
    }

    return count;
}

NIMCP_API uint64_t genius_gauss_modular_pow(
    uint64_t base,
    uint64_t exp,
    uint64_t mod) {

    if (mod == 0) return 0;
    if (mod == 1) return 0;

    uint64_t result = 1;
    base %= mod;

    while (exp > 0) {
        if (exp & 1) {
            result = (result * base) % mod;
        }
        exp >>= 1;
        base = (base * base) % mod;
    }

    return result;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void mathematical_genius_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_mathematical_genius_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Functions
 * ============================================================================ */

int mathematical_genius_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mathematical_genius_training_begin: NULL argument");
        return -1;
    }
    mathematical_genius_heartbeat_instance(g_mathematical_genius_health_agent, "mathematical_genius_training_begin", 0.0f);
    mathematical_genius_t* mg = (mathematical_genius_t*)instance;
    mg->stats.problems_attempted = 0;
    mg->stats.problems_solved = 0;
    mg->stats.proofs_constructed = 0;
    mg->stats.conjectures_generated = 0;
    mg->stats.insights_discovered = 0;
    mg->stats.avg_elegance = 0.0f;
    mg->stats.avg_novelty = 0.0f;
    mg->stats.avg_rigor = 0.0f;
    mg->stats.total_thinking_time_us = 0;
    mg->stats.avg_thinking_time_us = 0.0f;
    mg->stats.total_atp_consumed = 0.0f;
    for (int i = 0; i < GENIUS_MODE_COUNT; i++) {
        mg->stats.mode_usage[i] = 0;
        mg->stats.mode_success_rate[i] = 0.5f;
    }
    NIMCP_LOGGING_INFO("Mathematical genius training begin: counters reset, mode=%d",
                       (int)mg->active_mode);
    return 0;
}

int mathematical_genius_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mathematical_genius_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    mathematical_genius_heartbeat_instance(g_mathematical_genius_health_agent, "mathematical_genius_training_step", progress);
    mathematical_genius_t* mg = (mathematical_genius_t*)instance;
    mg->stats.problems_attempted++;
    /* Progressive creativity/rigor balance during training */
    mg->effective_creativity = mg->config.creativity_level * (0.7f + 0.3f * progress);
    if (mg->effective_creativity > 1.0f) mg->effective_creativity = 1.0f;
    mg->effective_rigor = mg->config.rigor_level * (0.8f + 0.2f * progress);
    if (mg->effective_rigor > 1.0f) mg->effective_rigor = 1.0f;
    /* Mode success rate adaptation */
    int mode_idx = (int)mg->active_mode;
    if (mode_idx >= 0 && mode_idx < GENIUS_MODE_COUNT) {
        mg->stats.mode_success_rate[mode_idx] =
            mg->stats.mode_success_rate[mode_idx] * (1.0f - 0.1f * progress) +
            progress * 0.1f;
    }
    /* ATP consumption tracking */
    mg->stats.total_atp_consumed += 0.01f * (1.0f + progress);
    return 0;
}

int mathematical_genius_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mathematical_genius_training_end: NULL argument");
        return -1;
    }
    mathematical_genius_heartbeat_instance(g_mathematical_genius_health_agent, "mathematical_genius_training_end", 1.0f);
    mathematical_genius_t* mg = (mathematical_genius_t*)instance;
    if (mg->stats.problems_attempted > 0) {
        mg->stats.avg_thinking_time_us =
            (float)mg->stats.total_thinking_time_us / (float)mg->stats.problems_attempted;
    }
    float solve_rate = (mg->stats.problems_attempted > 0)
        ? (float)mg->stats.problems_solved / (float)mg->stats.problems_attempted
        : 0.0f;
    NIMCP_LOGGING_INFO("Mathematical genius training end: %lu attempted, %lu solved, solve_rate=%.4f, atp=%.2f",
                       (unsigned long)mg->stats.problems_attempted,
                       (unsigned long)mg->stats.problems_solved,
                       solve_rate,
                       mg->stats.total_atp_consumed);
    return 0;
}
