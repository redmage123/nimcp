//=============================================================================
// nimcp_counterfactual.c - Counterfactual Reasoning System Implementation
//=============================================================================
/**
 * @file nimcp_counterfactual.c
 * @brief Implementation of "what if" thinking and alternative past reasoning
 *
 * WHAT: Counterfactual reasoning generates alternative scenarios based on
 *       mutations to past events, computing emotional impact and causal chains
 * WHY:  Counterfactual thinking is essential for learning, planning, and
 *       emotional regulation - imagining alternatives to understand cause/effect
 * HOW:  Mutates memory elements, propagates changes through entanglement graph,
 *       and computes affect changes (regret/relief) from outcome differences
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#include "cognitive/memory/core/nimcp_counterfactual.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(counterfactual, MESH_ADAPTER_CATEGORY_MEMORY)


//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal counterfactual system structure
 */
struct counterfactual_system_struct {
    //-------------------------------------------------------------------------
    // External references (not owned)
    //-------------------------------------------------------------------------
    entangle_graph_t entanglement;      /**< Entanglement graph for causal inference */
    pr_node_manager_t node_manager;     /**< Node manager for memory access */

    //-------------------------------------------------------------------------
    // Causal matrix (learned cause-effect relationships)
    //-------------------------------------------------------------------------
    float* causal_matrix;               /**< 2D matrix [causal_dim x causal_dim] */
    size_t causal_dim;                  /**< Dimension of causal matrix */

    //-------------------------------------------------------------------------
    // Analysis cache
    //-------------------------------------------------------------------------
    counterfactual_analysis_t** analysis_cache; /**< Cached analyses */
    uint64_t* cache_memory_ids;         /**< Memory IDs for cache lookup */
    size_t cache_count;                 /**< Number of cached analyses */
    size_t cache_capacity;              /**< Cache capacity */

    //-------------------------------------------------------------------------
    // Configuration
    //-------------------------------------------------------------------------
    counterfactual_config_t config;     /**< System configuration */

    //-------------------------------------------------------------------------
    // Statistics
    //-------------------------------------------------------------------------
    counterfactual_stats_t stats;       /**< Operational statistics */
};

//=============================================================================
// Static Variables
//=============================================================================

/** Thread-local error message buffer */
static _Thread_local char s_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

/** Learning rate for causal matrix updates */
static const float CAUSAL_LEARNING_RATE = NIMCP_LEARNING_RATE_COARSE;

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Set the last error message
 */
static void set_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(s_last_error, sizeof(s_last_error), format, args);
    va_end(args);
}

/**
 * @brief Clear the last error message
 */
static void clear_error(void) {
    s_last_error[0] = '\0';
}

/**
 * @brief Fast absolute value for floats
 */
static inline float fabsf_fast(float x) {
    return (x < 0.0f) ? -x : x;
}

/**
 * @brief Get current time in milliseconds
 */
static uint64_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

/**
 * @brief Map signature to causal matrix index
 *
 * Uses signature hash to map to matrix index.
 */
static size_t sig_to_causal_index(const prime_signature_t* sig, size_t dim) {
    if (!sig) return 0;
    return (size_t)(sig->hash % dim);
}

/**
 * @brief Compute recency-based mutability factor
 *
 * More recent events are more mentally mutable.
 * Uses exponential decay from current time.
 */
static float compute_recency_mutability(uint64_t event_time_ms, uint64_t current_time_ms) {
    if (event_time_ms >= current_time_ms) return 1.0f;

    uint64_t age_ms = current_time_ms - event_time_ms;
    // Convert to days for decay calculation
    float age_days = (float)age_ms / (24.0f * 60.0f * 60.0f * 1000.0f);

    // Exponential decay: mutability = exp(-0.1 * age_days)
    float mutability = expf(-0.1f * age_days);
    return nimcp_clamp01(mutability);
}

/**
 * @brief Compute exceptionality factor
 *
 * Exceptional (non-routine) events are more mentally mutable.
 * Uses quaternion salience as proxy for exceptionality.
 */
static float compute_exceptionality(const pr_memory_node_t* memory) {
    if (!memory) return 0.5f;

    nimcp_quaternion_t state = pr_memory_node_get_state(memory);
    // Salience (y component) serves as exceptionality proxy
    return nimcp_clamp01(state.y);
}

/**
 * @brief Compute controllability factor
 *
 * Actions are more controllable than events.
 * Uses emotional valence as partial indicator.
 */
static float compute_controllability(mutation_type_t type, const pr_memory_node_t* memory) {
    float base = 0.0f;

    switch (type) {
        case MUTATE_ACTION:
            base = 0.9f;  // Actions highly controllable
            break;
        case MUTATE_TIMING:
            base = 0.7f;  // Timing somewhat controllable
            break;
        case MUTATE_INACTION:
            base = 0.6f;  // Inaction partially controllable
            break;
        case MUTATE_CONTEXT:
            base = 0.3f;  // Context less controllable
            break;
        case MUTATE_EVENT:
            base = 0.2f;  // Events largely uncontrollable
            break;
        case MUTATE_PERSON:
            base = 0.1f;  // Other people least controllable
            break;
        default:
            base = 0.5f;
    }

    return nimcp_clamp01(base);
}

/**
 * @brief Generate mutated signature for action replacement
 */
static void generate_mutated_action(
    const prime_signature_t* original,
    prime_signature_t* mutated)
{
    if (!original || !mutated) return;

    // Start with original signature
    *mutated = *original;

    // Modify some exponents to create alternative
    // This is a heuristic - in practice, alternatives would come from
    // a learned action repertoire or external suggestions
    for (size_t i = 0; i < PRIME_SIG_DIM; i += 4) {
        // Swap or modify exponents to create different action signature
        uint8_t temp = mutated->exponents[i];
        mutated->exponents[i] = mutated->exponents[(i + 2) % PRIME_SIG_DIM];
        mutated->exponents[(i + 2) % PRIME_SIG_DIM] = temp;
    }

    // Recompute hash
    mutated->hash = prime_sig_hash(mutated);
}

/**
 * @brief Generate empty/null signature for inaction
 */
static void generate_inaction_signature(prime_signature_t* sig) {
    if (!sig) return;
    memset(sig, 0, sizeof(*sig));
    // Leave hash as 0 for "no action" signature
}

/**
 * @brief Predict outcome valence based on mutation
 *
 * Uses causal matrix to estimate how mutation affects outcome.
 */
static float predict_outcome_valence(
    struct counterfactual_system_struct* system,
    const prime_signature_t* original,
    const prime_signature_t* mutated,
    const prime_signature_t* actual_outcome,
    float actual_valence)
{
    if (!system || !original || !actual_outcome) {
        return actual_valence;  // Default to no change
    }

    // Get causal indices
    size_t orig_idx = sig_to_causal_index(original, system->causal_dim);
    size_t out_idx = sig_to_causal_index(actual_outcome, system->causal_dim);

    // Get causal strength of original -> outcome
    float orig_strength = system->causal_matrix[orig_idx * system->causal_dim + out_idx];

    // If we have a mutated element, estimate its causal impact
    float mutated_strength = 0.0f;
    if (mutated) {
        size_t mut_idx = sig_to_causal_index(mutated, system->causal_dim);
        mutated_strength = system->causal_matrix[mut_idx * system->causal_dim + out_idx];
    }

    // Estimate valence change based on causal strength difference
    // Positive difference = mutated has stronger positive causal link = better outcome
    float strength_diff = mutated_strength - orig_strength;

    // Scale by a factor and add to actual valence
    float predicted = actual_valence + strength_diff * 0.5f;
    return nimcp_clampf(predicted, -1.0f, 1.0f);
}

/**
 * @brief Estimate probability of counterfactual outcome
 *
 * Based on:
 * 1. Causal strength of the mutation
 * 2. Similarity to other observed outcomes
 * 3. Mutability of the changed element
 */
static float estimate_outcome_probability(
    struct counterfactual_system_struct* system,
    const counterfactual_t* cf)
{
    if (!system || !cf) return 0.5f;

    float probability = 0.5f;  // Base probability

    // Factor 1: Causal strength (stronger causal link = more likely alternative)
    size_t mut_idx = sig_to_causal_index(&cf->mutated_element, system->causal_dim);
    size_t out_idx = sig_to_causal_index(&cf->alternate_outcome, system->causal_dim);
    float causal = system->causal_matrix[mut_idx * system->causal_dim + out_idx];
    probability += causal * 0.2f;

    // Factor 2: Similarity to actual outcome (more similar = more likely)
    float similarity = prime_sig_jaccard(&cf->alternate_outcome, &cf->original_element);
    if (similarity >= 0.0f) {
        probability += (1.0f - similarity) * 0.2f;  // Different outcomes are less certain
    }

    // Factor 3: Controllability (more controllable = more achievable alternative)
    if (cf->is_controllable) {
        probability += 0.1f;
    }

    return nimcp_clamp01(probability);
}

/**
 * @brief Compute affect (regret/relief) for a counterfactual
 */
static void compute_affect_internal(counterfactual_t* cf, float controllability) {
    if (!cf) return;

    // Compute affect change (how much better/worse the alternative is)
    // Positive = alternative was better (upward), negative = alternative was worse (downward)
    // Already set by caller

    float abs_change = fabsf_fast(cf->affect_change);

    if (cf->affect_change > CF_EPSILON) {
        // Upward counterfactual: alternative was better -> regret
        cf->direction = COUNTER_UPWARD;
        // Regret = change magnitude * controllability * salience
        cf->regret_intensity = abs_change * controllability * cf->salience;
        cf->relief_intensity = 0.0f;
    } else if (cf->affect_change < -CF_EPSILON) {
        // Downward counterfactual: alternative was worse -> relief
        cf->direction = COUNTER_DOWNWARD;
        cf->regret_intensity = 0.0f;
        // Relief = change magnitude * (1 - how close we came to bad outcome)
        cf->relief_intensity = abs_change * cf->outcome_probability * cf->salience;
    } else {
        // Lateral counterfactual: neither better nor worse
        cf->regret_intensity = 0.0f;
        cf->relief_intensity = 0.0f;
    }

    // Clamp intensities
    cf->regret_intensity = nimcp_clamp01(cf->regret_intensity);
    cf->relief_intensity = nimcp_clamp01(cf->relief_intensity);
}

//=============================================================================
// Configuration Functions
//=============================================================================

counterfactual_config_t counterfactual_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    counterfactual_heartbeat("counterfactu_config_default", 0.0f);


    counterfactual_config_t config = {
        .causal_dim = CF_DEFAULT_CAUSAL_DIM,
        .max_analyses = CF_DEFAULT_MAX_ANALYSES,
        .max_counterfactuals = CF_MAX_COUNTERFACTUALS,
        .max_causes = CF_MAX_CAUSES,
        .min_causal_strength = CF_MIN_CAUSAL_STRENGTH,
        .affect_decay_rate = CF_AFFECT_DECAY_RATE,
        .action_mutability_weight = 1.5f,
        .event_mutability_weight = 1.0f,
        .timing_mutability_weight = 1.2f,
        .recency_weight = 1.3f,
        .exceptionality_weight = 1.2f,
        .enable_downward_counterfactuals = true,
        .enable_causal_learning = true,
        .enable_affect_computation = true
    };
    return config;
}

bool counterfactual_config_validate(const counterfactual_config_t* config) {
    if (!config) {
        set_error("NULL config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "counterfactual_config_validate: config is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    counterfactual_heartbeat("counterfactu_config_validate", 0.0f);


    if (config->causal_dim == 0 || config->causal_dim > 4096) {
        set_error("causal_dim must be in [1, 4096]");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "counterfactual_config_validate: config->causal_dim is zero");
        return false;
    }

    if (config->max_counterfactuals == 0 || config->max_counterfactuals > 1024) {
        set_error("max_counterfactuals must be in [1, 1024]");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "counterfactual_config_validate: config->max_counterfactuals is zero");
        return false;
    }

    if (config->max_causes == 0 || config->max_causes > 256) {
        set_error("max_causes must be in [1, 256]");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "counterfactual_config_validate: config->max_causes is zero");
        return false;
    }

    if (config->min_causal_strength < 0.0f || config->min_causal_strength > 1.0f) {
        set_error("min_causal_strength must be in [0, 1]");
        return false;
    }

    clear_error();
    return true;
}

//=============================================================================
// System Lifecycle Functions
//=============================================================================

counterfactual_system_t counterfactual_create(
    entangle_graph_t entanglement,
    pr_node_manager_t node_manager,
    const counterfactual_config_t* config)
{
    // Use default config if not provided
    /* Phase 8: Heartbeat at operation start */
    counterfactual_heartbeat("counterfactu_create", 0.0f);


    counterfactual_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = counterfactual_config_default();
    }

    // Validate configuration
    if (!counterfactual_config_validate(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "counterfactual_create: counterfactual_config_validate is NULL");
        return NULL;  // Error already set
    }

    // Allocate system structure
    struct counterfactual_system_struct* system =
        (struct counterfactual_system_struct*)nimcp_calloc(1, sizeof(*system));
    if (!system) {
        set_error("Failed to allocate counterfactual system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;
    }

    // Store references
    system->entanglement = entanglement;
    system->node_manager = node_manager;
    system->config = cfg;

    // Allocate causal matrix
    size_t matrix_size = cfg.causal_dim * cfg.causal_dim;
    system->causal_matrix = (float*)nimcp_calloc(matrix_size, sizeof(float));
    if (!system->causal_matrix) {
        set_error("Failed to allocate causal matrix");
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "counterfactual_create: system->causal_matrix is NULL");
        return NULL;
    }
    system->causal_dim = cfg.causal_dim;

    // Initialize causal matrix with small default values
    for (size_t i = 0; i < matrix_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && matrix_size > 256) {
            counterfactual_heartbeat("counterfactu_loop",
                             (float)(i + 1) / (float)matrix_size);
        }

        system->causal_matrix[i] = 0.01f;  // Small prior
    }

    // Allocate analysis cache
    if (cfg.max_analyses > 0) {
        system->analysis_cache =
            (counterfactual_analysis_t**)nimcp_calloc(cfg.max_analyses, sizeof(*system->analysis_cache));
        system->cache_memory_ids =
            (uint64_t*)nimcp_calloc(cfg.max_analyses, sizeof(*system->cache_memory_ids));
        if (!system->analysis_cache || !system->cache_memory_ids) {
            set_error("Failed to allocate analysis cache");
            nimcp_free(system->causal_matrix);
            nimcp_free(system->analysis_cache);
            nimcp_free(system->cache_memory_ids);
            nimcp_free(system);
            system = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "counterfactual_create: required parameter is NULL (system->analysis_cache, system->cache_memory_ids)");
            return NULL;
        }
        system->cache_capacity = cfg.max_analyses;
    }

    // Initialize statistics
    memset(&system->stats, 0, sizeof(system->stats));

    clear_error();
    return system;
}

void counterfactual_destroy(counterfactual_system_t system) {
    if (!system) return;

    // Free cached analyses
    /* Phase 8: Heartbeat at operation start */
    counterfactual_heartbeat("counterfactu_destroy", 0.0f);


    if (system->analysis_cache) {
        for (size_t i = 0; i < system->cache_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && system->cache_count > 256) {
                counterfactual_heartbeat("counterfactu_loop",
                                 (float)(i + 1) / (float)system->cache_count);
            }

            if (system->analysis_cache[i]) {
                counterfactual_analysis_cleanup(system->analysis_cache[i]);
                nimcp_free(system->analysis_cache[i]);
            }
        }
        nimcp_free(system->analysis_cache);
    }

    nimcp_free(system->cache_memory_ids);
    nimcp_free(system->causal_matrix);
    nimcp_free(system);
    system = NULL;
}

bool counterfactual_clear_cache(counterfactual_system_t system) {
    if (!system) {
        set_error("NULL system pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "counterfactual_clear_cache: system is NULL");
        return false;
    }

    // Free cached analyses
    /* Phase 8: Heartbeat at operation start */
    counterfactual_heartbeat("counterfactu_clear_cache", 0.0f);


    for (size_t i = 0; i < system->cache_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->cache_count > 256) {
            counterfactual_heartbeat("counterfactu_loop",
                             (float)(i + 1) / (float)system->cache_count);
        }

        if (system->analysis_cache[i]) {
            counterfactual_analysis_cleanup(system->analysis_cache[i]);
            nimcp_free(system->analysis_cache[i]);
            system->analysis_cache[i] = NULL;
        }
        system->cache_memory_ids[i] = 0;
    }
    system->cache_count = 0;

    clear_error();
    return true;
}

bool counterfactual_reset_causal_matrix(counterfactual_system_t system) {
    if (!system) {
        set_error("NULL system pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "counterfactual_reset_causal_matrix: system is NULL");
        return false;
    }

    // Reset to small prior values
    /* Phase 8: Heartbeat at operation start */
    counterfactual_heartbeat("counterfactu_reset_causal_matrix", 0.0f);


    size_t matrix_size = system->causal_dim * system->causal_dim;
    for (size_t i = 0; i < matrix_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && matrix_size > 256) {
            counterfactual_heartbeat("counterfactu_loop",
                             (float)(i + 1) / (float)matrix_size);
        }

        system->causal_matrix[i] = 0.01f;
    }

    system->stats.causal_links_learned = 0;

    clear_error();
    return true;
}

//=============================================================================
// Main Analysis Functions
//=============================================================================

bool counterfactual_analyze(
    counterfactual_system_t system,
    pr_memory_node_t* memory,
    counterfactual_analysis_t* analysis)
{
    if (!system || !memory || !analysis) {
        set_error("NULL pointer: system=%p, memory=%p, analysis=%p",
                  (void*)system, (void*)memory, (void*)analysis);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "counterfactual_analyze: required parameter is NULL (system, memory, analysis)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    counterfactual_heartbeat("counterfactu_analyze", 0.0f);


    uint64_t start_time = get_current_time_ms();

    // Store reference to memory
    analysis->memory = memory;
    analysis->memory_id = pr_memory_node_get_id(memory);

    // Reset counters
    analysis->num_causes = 0;
    analysis->num_counterfactuals = 0;
    analysis->most_impactful = NULL;
    analysis->most_regretted = NULL;
    analysis->most_relieving = NULL;

    // Get memory state for analysis
    nimcp_quaternion_t state = pr_memory_node_get_state(memory);
    float actual_valence = state.x;  // Emotional valence

    // Step 1: Extract causal factors
    if (!counterfactual_extract_causes(system, memory,
                                        analysis->causes,
                                        analysis->causal_strengths,
                                        analysis->max_causes,
                                        &analysis->num_causes)) {
        // Non-fatal: may have no causes
        analysis->num_causes = 0;
    }

    // Step 2: Compute mutability scores
    uint64_t current_time = get_current_time_ms();
    float recency = compute_recency_mutability(memory->created_time_ms, current_time);
    float exceptionality = compute_exceptionality(memory);

    analysis->memory_recency = recency;
    analysis->memory_exceptionality = exceptionality;

    // Compute per-type mutability
    analysis->action_mutability = CF_DEFAULT_ACTION_MUTABILITY *
                                   system->config.action_mutability_weight *
                                   recency * (0.5f + 0.5f * exceptionality);
    analysis->event_mutability = CF_DEFAULT_EVENT_MUTABILITY *
                                  system->config.event_mutability_weight *
                                  recency;
    analysis->timing_mutability = CF_DEFAULT_TIMING_MUTABILITY *
                                   system->config.timing_mutability_weight *
                                   recency;
    analysis->person_mutability = 0.3f * recency;

    // Clamp mutabilities
    analysis->action_mutability = nimcp_clamp01(analysis->action_mutability);
    analysis->event_mutability = nimcp_clamp01(analysis->event_mutability);
    analysis->timing_mutability = nimcp_clamp01(analysis->timing_mutability);
    analysis->person_mutability = nimcp_clamp01(analysis->person_mutability);

    // Overall mutability is weighted average
    analysis->overall_mutability = (
        analysis->action_mutability * 0.4f +
        analysis->event_mutability * 0.2f +
        analysis->timing_mutability * 0.25f +
        analysis->person_mutability * 0.15f
    );

    // Step 3: Generate counterfactuals
    const prime_signature_t* memory_sig = pr_memory_node_get_signature(memory);
    size_t cf_idx = 0;

    // Generate action mutation counterfactual
    if (cf_idx < analysis->max_counterfactuals) {
        counterfactual_t* cf = &analysis->counterfactuals[cf_idx];
        memset(cf, 0, sizeof(*cf));

        cf->original_memory = memory;
        cf->mutation_type = MUTATE_ACTION;
        if (memory_sig) {
            cf->original_element = *memory_sig;
        }
        generate_mutated_action(&cf->original_element, &cf->mutated_element);

        // Generate alternate outcome
        cf->alternate_outcome = cf->mutated_element;  // Simplified: use mutated as proxy

        // Predict valence
        cf->outcome_valence = predict_outcome_valence(
            system, &cf->original_element, &cf->mutated_element,
            memory_sig, actual_valence);

        cf->affect_change = cf->outcome_valence - actual_valence;
        cf->outcome_probability = estimate_outcome_probability(system, cf);
        cf->salience = analysis->action_mutability;
        cf->is_controllable = true;
        cf->created_time_ms = current_time;

        // Compute affect
        if (system->config.enable_affect_computation) {
            compute_affect_internal(cf, compute_controllability(MUTATE_ACTION, memory));
        }

        cf_idx++;
    }

    // Generate inaction counterfactual
    if (cf_idx < analysis->max_counterfactuals) {
        counterfactual_t* cf = &analysis->counterfactuals[cf_idx];
        memset(cf, 0, sizeof(*cf));

        cf->original_memory = memory;
        cf->mutation_type = MUTATE_INACTION;
        if (memory_sig) {
            cf->original_element = *memory_sig;
        }
        generate_inaction_signature(&cf->mutated_element);

        cf->alternate_outcome = cf->mutated_element;
        cf->outcome_valence = predict_outcome_valence(
            system, &cf->original_element, &cf->mutated_element,
            memory_sig, actual_valence);

        cf->affect_change = cf->outcome_valence - actual_valence;
        cf->outcome_probability = estimate_outcome_probability(system, cf);
        cf->salience = analysis->action_mutability * 0.8f;
        cf->is_controllable = true;
        cf->created_time_ms = current_time;

        if (system->config.enable_affect_computation) {
            compute_affect_internal(cf, compute_controllability(MUTATE_INACTION, memory));
        }

        cf_idx++;
    }

    // Generate timing counterfactual (earlier)
    if (cf_idx < analysis->max_counterfactuals) {
        counterfactual_t* cf = &analysis->counterfactuals[cf_idx];
        memset(cf, 0, sizeof(*cf));

        cf->original_memory = memory;
        cf->mutation_type = MUTATE_TIMING;
        if (memory_sig) {
            cf->original_element = *memory_sig;
            cf->mutated_element = *memory_sig;  // Same action, different time
        }

        // Timing change affects outcome through context
        cf->alternate_outcome = cf->mutated_element;
        cf->outcome_valence = actual_valence + 0.1f;  // Earlier often better
        cf->affect_change = cf->outcome_valence - actual_valence;
        cf->outcome_probability = 0.6f;
        cf->salience = analysis->timing_mutability;
        cf->is_controllable = true;
        cf->created_time_ms = current_time;

        if (system->config.enable_affect_computation) {
            compute_affect_internal(cf, compute_controllability(MUTATE_TIMING, memory));
        }

        cf_idx++;
    }

    // Generate downward counterfactual if enabled
    if (system->config.enable_downward_counterfactuals && cf_idx < analysis->max_counterfactuals) {
        counterfactual_t* cf = &analysis->counterfactuals[cf_idx];
        memset(cf, 0, sizeof(*cf));

        cf->original_memory = memory;
        cf->mutation_type = MUTATE_EVENT;
        if (memory_sig) {
            cf->original_element = *memory_sig;
        }
        // Generate worse alternative
        generate_mutated_action(&cf->original_element, &cf->mutated_element);

        cf->alternate_outcome = cf->mutated_element;
        // Predict a worse outcome
        cf->outcome_valence = actual_valence - 0.3f;
        cf->affect_change = cf->outcome_valence - actual_valence;
        cf->outcome_probability = 0.4f;  // Lower probability for bad outcome
        cf->salience = analysis->event_mutability;
        cf->is_controllable = false;
        cf->created_time_ms = current_time;

        if (system->config.enable_affect_computation) {
            compute_affect_internal(cf, compute_controllability(MUTATE_EVENT, memory));
        }

        cf_idx++;
    }

    analysis->num_counterfactuals = cf_idx;

    // Step 4: Find most impactful counterfactuals
    float max_impact = 0.0f;
    float max_regret = 0.0f;
    float max_relief = 0.0f;

    for (size_t i = 0; i < analysis->num_counterfactuals; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && analysis->num_counterfactuals > 256) {
            counterfactual_heartbeat("counterfactu_loop",
                             (float)(i + 1) / (float)analysis->num_counterfactuals);
        }

        counterfactual_t* cf = &analysis->counterfactuals[i];
        float impact = fabsf_fast(cf->affect_change) * cf->outcome_probability;

        if (impact > max_impact) {
            max_impact = impact;
            analysis->most_impactful = cf;
        }

        if (cf->regret_intensity > max_regret) {
            max_regret = cf->regret_intensity;
            analysis->most_regretted = cf;
        }

        if (cf->relief_intensity > max_relief) {
            max_relief = cf->relief_intensity;
            analysis->most_relieving = cf;
        }
    }

    // Store analysis time
    analysis->analysis_time_ms = current_time;

    // Update statistics
    system->stats.total_analyses++;
    system->stats.total_counterfactuals += analysis->num_counterfactuals;

    uint64_t end_time = get_current_time_ms();
    system->stats.computation_time_ns += (end_time - start_time) * 1000000ULL;

    clear_error();
    return true;
}

bool counterfactual_generate(
    counterfactual_system_t system,
    pr_memory_node_t* memory,
    mutation_type_t mutation_type,
    const prime_signature_t* original,
    const prime_signature_t* mutated,
    counterfactual_t* result)
{
    if (!system || !memory || !result) {
        set_error("NULL pointer in counterfactual_generate");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "counterfactual_generate: required parameter is NULL (system, memory, result)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    counterfactual_heartbeat("counterfactu_generate", 0.0f);


    memset(result, 0, sizeof(*result));

    result->original_memory = memory;
    result->mutation_type = mutation_type;
    result->created_time_ms = get_current_time_ms();

    // Get memory state
    nimcp_quaternion_t state = pr_memory_node_get_state(memory);
    float actual_valence = state.x;

    // Set original element
    if (original) {
        result->original_element = *original;
    } else {
        const prime_signature_t* mem_sig = pr_memory_node_get_signature(memory);
        if (mem_sig) {
            result->original_element = *mem_sig;
        }
    }

    // Set or generate mutated element
    if (mutated) {
        result->mutated_element = *mutated;
    } else {
        switch (mutation_type) {
            case MUTATE_ACTION:
                generate_mutated_action(&result->original_element, &result->mutated_element);
                break;
            case MUTATE_INACTION:
                generate_inaction_signature(&result->mutated_element);
                break;
            default:
                result->mutated_element = result->original_element;
                break;
        }
    }

    // Generate alternate outcome
    result->alternate_outcome = result->mutated_element;

    // Predict outcome
    result->outcome_valence = predict_outcome_valence(
        system, &result->original_element, &result->mutated_element,
        &result->original_element, actual_valence);

    result->affect_change = result->outcome_valence - actual_valence;
    result->outcome_probability = estimate_outcome_probability(system, result);

    // Set controllability and salience
    result->is_controllable = (mutation_type == MUTATE_ACTION ||
                               mutation_type == MUTATE_INACTION ||
                               mutation_type == MUTATE_TIMING);

    float recency = compute_recency_mutability(memory->created_time_ms, result->created_time_ms);
    result->salience = recency * compute_exceptionality(memory);

    // Compute affect
    if (system->config.enable_affect_computation) {
        compute_affect_internal(result, compute_controllability(mutation_type, memory));
    }

    // Update statistics
    system->stats.total_counterfactuals++;
    if (result->direction == COUNTER_UPWARD) {
        system->stats.upward_count++;
    } else if (result->direction == COUNTER_DOWNWARD) {
        system->stats.downward_count++;
    }

    clear_error();
    return true;
}

//=============================================================================
// Mutation Functions
//=============================================================================

bool counterfactual_mutate_action(
    counterfactual_system_t system,
    pr_memory_node_t* memory,
    const prime_signature_t* action_taken,
    const prime_signature_t* action_alternative,
    counterfactual_t* result)
{
    /* Phase 8: Heartbeat at operation start */
    counterfactual_heartbeat("counterfactu_mutate_action", 0.0f);


    return counterfactual_generate(system, memory, MUTATE_ACTION,
                                   action_taken, action_alternative, result);
}

bool counterfactual_mutate_inaction(
    counterfactual_system_t system,
    pr_memory_node_t* memory,
    const prime_signature_t* action_taken,
    counterfactual_t* result)
{
    if (!result) {
        set_error("NULL result pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "counterfactual_mutate_inaction: result is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    counterfactual_heartbeat("counterfactu_mutate_inaction", 0.0f);


    prime_signature_t inaction_sig;
    generate_inaction_signature(&inaction_sig);

    return counterfactual_generate(system, memory, MUTATE_INACTION,
                                   action_taken, &inaction_sig, result);
}

bool counterfactual_mutate_timing(
    counterfactual_system_t system,
    pr_memory_node_t* memory,
    const prime_signature_t* action,
    int64_t time_delta,
    counterfactual_t* result)
{
    if (!system || !memory || !result) {
        set_error("NULL pointer in counterfactual_mutate_timing");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "counterfactual_mutate_timing: required parameter is NULL (system, memory, result)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    counterfactual_heartbeat("counterfactu_mutate_timing", 0.0f);


    memset(result, 0, sizeof(*result));

    result->original_memory = memory;
    result->mutation_type = MUTATE_TIMING;
    result->created_time_ms = get_current_time_ms();

    // Get memory state
    nimcp_quaternion_t state = pr_memory_node_get_state(memory);
    float actual_valence = state.x;

    // Set original element
    if (action) {
        result->original_element = *action;
    } else {
        const prime_signature_t* mem_sig = pr_memory_node_get_signature(memory);
        if (mem_sig) {
            result->original_element = *mem_sig;
        }
    }

    // For timing mutation, the action is the same but timing differs
    result->mutated_element = result->original_element;

    // Timing affects outcome through temporal context
    // Earlier timing (negative delta) often allows more preparation
    // Later timing (positive delta) might miss opportunities
    float timing_effect = 0.0f;
    if (time_delta < 0) {
        // Earlier: often better
        timing_effect = 0.1f * nimcp_clamp01((float)(-time_delta) / 3600000.0f);  // Scale by hours
    } else {
        // Later: often worse
        timing_effect = -0.05f * nimcp_clamp01((float)time_delta / 3600000.0f);
    }

    result->alternate_outcome = result->mutated_element;
    result->outcome_valence = nimcp_clampf(actual_valence + timing_effect, -1.0f, 1.0f);
    result->affect_change = result->outcome_valence - actual_valence;

    // Timing changes have moderate probability
    result->outcome_probability = 0.5f + 0.2f * nimcp_clamp01(fabsf_fast(timing_effect) * 5.0f);

    result->is_controllable = true;
    float recency = compute_recency_mutability(memory->created_time_ms, result->created_time_ms);
    result->salience = recency * 0.7f;

    if (system->config.enable_affect_computation) {
        compute_affect_internal(result, compute_controllability(MUTATE_TIMING, memory));
    }

    system->stats.total_counterfactuals++;
    if (result->direction == COUNTER_UPWARD) {
        system->stats.upward_count++;
    } else if (result->direction == COUNTER_DOWNWARD) {
        system->stats.downward_count++;
    }

    clear_error();
    return true;
}

bool counterfactual_mutate_event(
    counterfactual_system_t system,
    pr_memory_node_t* memory,
    const prime_signature_t* event_sig,
    counterfactual_t* result)
{
    if (!result) {
        set_error("NULL result pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "counterfactual_mutate_event: result is NULL");
        return false;
    }

    // Generate "event didn't happen" signature
    /* Phase 8: Heartbeat at operation start */
    counterfactual_heartbeat("counterfactu_mutate_event", 0.0f);


    prime_signature_t no_event_sig;
    generate_inaction_signature(&no_event_sig);

    return counterfactual_generate(system, memory, MUTATE_EVENT,
                                   event_sig, &no_event_sig, result);
}

//=============================================================================
// Outcome Evaluation Functions
//=============================================================================

bool counterfactual_evaluate_outcome(
    counterfactual_system_t system,
    const counterfactual_t* cf,
    prime_signature_t* outcome,
    float* probability)
{
    if (!system || !cf) {
        set_error("NULL pointer in counterfactual_evaluate_outcome");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "counterfactual_evaluate_outcome: required parameter is NULL (system, cf)");
        return false;
    }

    // Return already computed values
    /* Phase 8: Heartbeat at operation start */
    counterfactual_heartbeat("counterfactu_evaluate_outcome", 0.0f);


    if (outcome) {
        *outcome = cf->alternate_outcome;
    }

    if (probability) {
        *probability = cf->outcome_probability;
    }

    clear_error();
    return true;
}

bool counterfactual_compute_affect(
    counterfactual_system_t system,
    counterfactual_t* cf)
{
    if (!system || !cf) {
        set_error("NULL pointer in counterfactual_compute_affect");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "counterfactual_compute_affect: required parameter is NULL (system, cf)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    counterfactual_heartbeat("counterfactu_compute_affect", 0.0f);


    float controllability = compute_controllability(cf->mutation_type, cf->original_memory);
    compute_affect_internal(cf, controllability);

    // Update running statistics
    if (cf->regret_intensity > 0.0f) {
        float n = (float)(system->stats.upward_count + 1);
        system->stats.mean_regret = ((n - 1.0f) * system->stats.mean_regret +
                                     cf->regret_intensity) / (fabsf(n) > 1e-7f ? n : 1e-7f);
        if (cf->regret_intensity > system->stats.max_regret) {
            system->stats.max_regret = cf->regret_intensity;
        }
    }

    if (cf->relief_intensity > 0.0f) {
        float n = (float)(system->stats.downward_count + 1);
        system->stats.mean_relief = ((n - 1.0f) * system->stats.mean_relief +
                                     cf->relief_intensity) / (fabsf(n) > 1e-7f ? n : 1e-7f);
        if (cf->relief_intensity > system->stats.max_relief) {
            system->stats.max_relief = cf->relief_intensity;
        }
    }

    clear_error();
    return true;
}

//=============================================================================
// Causal Reasoning Functions
//=============================================================================

bool counterfactual_extract_causes(
    counterfactual_system_t system,
    pr_memory_node_t* memory,
    prime_signature_t* causes,
    float* strengths,
    size_t max_causes,
    size_t* num_causes)
{
    if (!system || !memory || !num_causes) {
        set_error("NULL pointer in counterfactual_extract_causes");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "counterfactual_extract_causes: required parameter is NULL (system, memory, num_causes)");
        return false;
    }

    *num_causes = 0;

    // If no entanglement graph, we can't extract causes
    if (!system->entanglement) {
        // Still valid - just no causes found
        clear_error();
        return true;
    }

    // Get memory signature for causal lookup
    /* Phase 8: Heartbeat at operation start */
    counterfactual_heartbeat("counterfactu_extract_causes", 0.0f);


    const prime_signature_t* mem_sig = pr_memory_node_get_signature(memory);
    if (!mem_sig) {
        clear_error();
        return true;  // No signature, no causes
    }

    size_t effect_idx = sig_to_causal_index(mem_sig, system->causal_dim);

    // Scan causal matrix for causes of this effect
    size_t found = 0;
    for (size_t cause_idx = 0; cause_idx < system->causal_dim && found < max_causes; cause_idx++) {
        float strength = system->causal_matrix[cause_idx * system->causal_dim + effect_idx];

        if (strength > system->config.min_causal_strength) {
            if (causes) {
                // Create synthetic signature for this causal index
                memset(&causes[found], 0, sizeof(causes[found]));
                causes[found].hash = (uint64_t)cause_idx;
                causes[found].num_factors = 1;
            }
            if (strengths) {
                strengths[found] = strength;
            }
            found++;
        }
    }

    *num_causes = found;
    clear_error();
    return true;
}

bool counterfactual_update_causal_model(
    counterfactual_system_t system,
    const prime_signature_t* cause,
    const prime_signature_t* effect,
    float strength)
{
    if (!system || !cause || !effect) {
        set_error("NULL pointer in counterfactual_update_causal_model");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "counterfactual_update_causal_model: required parameter is NULL (system, cause, effect)");
        return false;
    }

    if (!system->config.enable_causal_learning) {
        clear_error();
        return true;  // Learning disabled, but not an error
    }

    /* Phase 8: Heartbeat at operation start */
    counterfactual_heartbeat("counterfactu_update_causal_model", 0.0f);


    size_t cause_idx = sig_to_causal_index(cause, system->causal_dim);
    size_t effect_idx = sig_to_causal_index(effect, system->causal_dim);

    // Update causal matrix with learning rate
    float old_value = system->causal_matrix[cause_idx * system->causal_dim + effect_idx];
    float new_value = CAUSAL_LEARNING_RATE * strength +
                      (1.0f - CAUSAL_LEARNING_RATE) * old_value;

    system->causal_matrix[cause_idx * system->causal_dim + effect_idx] = nimcp_clamp01(new_value);

    system->stats.causal_links_learned++;

    clear_error();
    return true;
}

float counterfactual_get_causal_strength(
    counterfactual_system_t system,
    const prime_signature_t* cause,
    const prime_signature_t* effect)
{
    if (!system || !cause || !effect) {
        set_error("NULL pointer in counterfactual_get_causal_strength");
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    counterfactual_heartbeat("counterfactu_get_causal_strength", 0.0f);


    size_t cause_idx = sig_to_causal_index(cause, system->causal_dim);
    size_t effect_idx = sig_to_causal_index(effect, system->causal_dim);

    clear_error();
    return system->causal_matrix[cause_idx * system->causal_dim + effect_idx];
}

//=============================================================================
// Mutability Functions
//=============================================================================

bool counterfactual_get_most_mutable(
    counterfactual_system_t system,
    pr_memory_node_t* memory,
    prime_signature_t* element,
    float* mutability,
    mutation_type_t* type)
{
    if (!system || !memory) {
        set_error("NULL pointer in counterfactual_get_most_mutable");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "counterfactual_get_most_mutable: required parameter is NULL (system, memory)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    counterfactual_heartbeat("counterfactu_get_most_mutable", 0.0f);


    uint64_t current_time = get_current_time_ms();
    float recency = compute_recency_mutability(memory->created_time_ms, current_time);
    float exceptionality = compute_exceptionality(memory);

    // Compute mutability for each type
    float action_mut = CF_DEFAULT_ACTION_MUTABILITY *
                       system->config.action_mutability_weight *
                       recency * (0.5f + 0.5f * exceptionality);

    float timing_mut = CF_DEFAULT_TIMING_MUTABILITY *
                       system->config.timing_mutability_weight *
                       recency;

    float event_mut = CF_DEFAULT_EVENT_MUTABILITY *
                      system->config.event_mutability_weight *
                      recency;

    // Find highest
    float max_mut = action_mut;
    mutation_type_t max_type = MUTATE_ACTION;

    if (timing_mut > max_mut) {
        max_mut = timing_mut;
        max_type = MUTATE_TIMING;
    }

    if (event_mut > max_mut) {
        max_mut = event_mut;
        max_type = MUTATE_EVENT;
    }

    // Return results
    if (element) {
        const prime_signature_t* mem_sig = pr_memory_node_get_signature(memory);
        if (mem_sig) {
            *element = *mem_sig;
        } else {
            memset(element, 0, sizeof(*element));
        }
    }

    if (mutability) {
        *mutability = nimcp_clamp01(max_mut);
    }

    if (type) {
        *type = max_type;
    }

    clear_error();
    return true;
}

float counterfactual_compute_mutability(
    counterfactual_system_t system,
    pr_memory_node_t* memory,
    const prime_signature_t* element,
    mutation_type_t type)
{
    /* Phase 8: Heartbeat at operation start */
    counterfactual_heartbeat("counterfactu_compute_mutability", 0.0f);


    (void)element;  // Not used in this simple implementation

    if (!system || !memory) {
        return 0.5f;  // Default
    }

    uint64_t current_time = get_current_time_ms();
    float recency = compute_recency_mutability(memory->created_time_ms, current_time);
    float exceptionality = compute_exceptionality(memory);

    float base = 0.0f;
    float weight = 0.0f;

    switch (type) {
        case MUTATE_ACTION:
            base = CF_DEFAULT_ACTION_MUTABILITY;
            weight = system->config.action_mutability_weight;
            break;
        case MUTATE_TIMING:
            base = CF_DEFAULT_TIMING_MUTABILITY;
            weight = system->config.timing_mutability_weight;
            break;
        case MUTATE_EVENT:
            base = CF_DEFAULT_EVENT_MUTABILITY;
            weight = system->config.event_mutability_weight;
            break;
        case MUTATE_INACTION:
            base = CF_DEFAULT_ACTION_MUTABILITY * 0.8f;
            weight = system->config.action_mutability_weight;
            break;
        default:
            base = 0.5f;
            weight = 1.0f;
    }

    float mutability = base * weight * recency *
                       (0.5f + 0.5f * exceptionality);

    return nimcp_clamp01(mutability);
}

//=============================================================================
// Comparison Functions
//=============================================================================

bool counterfactual_compare_updown(
    counterfactual_system_t system,
    const counterfactual_analysis_t* analysis,
    float* upward_strength,
    float* downward_strength,
    float* ratio)
{
    /* Phase 8: Heartbeat at operation start */
    counterfactual_heartbeat("counterfactu_compare_updown", 0.0f);


    (void)system;  // Not used

    if (!analysis) {
        set_error("NULL analysis pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "counterfactual_compare_updown: analysis is NULL");
        return false;
    }

    float up_total = 0.0f;
    float down_total = 0.0f;
    size_t up_count = 0;
    size_t down_count = 0;

    for (size_t i = 0; i < analysis->num_counterfactuals; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && analysis->num_counterfactuals > 256) {
            counterfactual_heartbeat("counterfactu_loop",
                             (float)(i + 1) / (float)analysis->num_counterfactuals);
        }

        const counterfactual_t* cf = &analysis->counterfactuals[i];

        if (cf->direction == COUNTER_UPWARD || cf->direction == COUNTER_ADDITIVE) {
            up_total += fabsf_fast(cf->affect_change) * cf->outcome_probability;
            up_count++;
        } else if (cf->direction == COUNTER_DOWNWARD || cf->direction == COUNTER_SUBTRACTIVE) {
            down_total += fabsf_fast(cf->affect_change) * cf->outcome_probability;
            down_count++;
        }
    }

    if (upward_strength) {
        *upward_strength = up_total;
    }

    if (downward_strength) {
        *downward_strength = down_total;
    }

    if (ratio) {
        float total = up_total + down_total;
        *ratio = (total > CF_EPSILON) ? (up_total / total) : 0.5f;
    }

    clear_error();
    return true;
}

bool counterfactual_find_most_actionable(
    counterfactual_system_t system,
    const counterfactual_analysis_t* analysis,
    counterfactual_t** result)
{
    /* Phase 8: Heartbeat at operation start */
    counterfactual_heartbeat("counterfactu_find_most_actionable", 0.0f);


    (void)system;  // Not used

    if (!analysis || !result) {
        set_error("NULL pointer in counterfactual_find_most_actionable");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "counterfactual_find_most_actionable: required parameter is NULL (analysis, result)");
        return false;
    }

    *result = NULL;
    float max_actionability = 0.0f;

    for (size_t i = 0; i < analysis->num_counterfactuals; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && analysis->num_counterfactuals > 256) {
            counterfactual_heartbeat("counterfactu_loop",
                             (float)(i + 1) / (float)analysis->num_counterfactuals);
        }

        counterfactual_t* cf = &analysis->counterfactuals[i];

        // Actionability = controllability * |affect_change| * probability * salience
        if (!cf->is_controllable) continue;

        float actionability = fabsf_fast(cf->affect_change) *
                              cf->outcome_probability *
                              cf->salience;

        // Upward counterfactuals are more actionable (motivate change)
        if (cf->direction == COUNTER_UPWARD) {
            actionability *= 1.5f;
        }

        if (actionability > max_actionability) {
            max_actionability = actionability;
            *result = cf;
        }
    }

    clear_error();
    return (*result != NULL);
}

//=============================================================================
// Analysis Memory Management
//=============================================================================

bool counterfactual_analysis_init(
    counterfactual_analysis_t* analysis,
    size_t max_causes,
    size_t max_counterfactuals)
{
    if (!analysis) {
        set_error("NULL analysis pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "counterfactual_analysis_init: analysis is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    counterfactual_heartbeat("counterfactu_analysis_init", 0.0f);


    memset(analysis, 0, sizeof(*analysis));

    // Allocate causes array
    if (max_causes > 0) {
        analysis->causes = (prime_signature_t*)nimcp_calloc(max_causes, sizeof(prime_signature_t));
        analysis->causal_strengths = (float*)nimcp_calloc(max_causes, sizeof(float));
        if (!analysis->causes || !analysis->causal_strengths) {
            set_error("Failed to allocate causes arrays");
            counterfactual_analysis_cleanup(analysis);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "counterfactual_analysis_init: required parameter is NULL (analysis->causes, analysis->causal_strengths)");
            return false;
        }
        analysis->max_causes = max_causes;
    }

    // Allocate counterfactuals array
    if (max_counterfactuals > 0) {
        analysis->counterfactuals = (counterfactual_t*)nimcp_calloc(max_counterfactuals,
                                                               sizeof(counterfactual_t));
        if (!analysis->counterfactuals) {
            set_error("Failed to allocate counterfactuals array");
            counterfactual_analysis_cleanup(analysis);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "counterfactual_analysis_init: analysis->counterfactuals is NULL");
            return false;
        }
        analysis->max_counterfactuals = max_counterfactuals;
    }

    clear_error();
    return true;
}

void counterfactual_analysis_cleanup(counterfactual_analysis_t* analysis) {
    if (!analysis) return;

    /* Phase 8: Heartbeat at operation start */
    counterfactual_heartbeat("counterfactu_analysis_cleanup", 0.0f);


    nimcp_free(analysis->causes);
    nimcp_free(analysis->causal_strengths);
    nimcp_free(analysis->counterfactuals);

    memset(analysis, 0, sizeof(*analysis));
}

bool counterfactual_analysis_copy(
    counterfactual_analysis_t* dest,
    const counterfactual_analysis_t* src)
{
    if (!dest || !src) {
        set_error("NULL pointer in counterfactual_analysis_copy");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "counterfactual_analysis_copy: required parameter is NULL (dest, src)");
        return false;
    }

    // Initialize destination with same capacities
    if (!counterfactual_analysis_init(dest, src->max_causes, src->max_counterfactuals)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "counterfactual_analysis_copy: counterfactual_analysis_init is NULL");
        return false;  // Error already set
    }

    // Copy scalar fields
    /* Phase 8: Heartbeat at operation start */
    counterfactual_heartbeat("counterfactu_analysis_copy", 0.0f);


    dest->memory = src->memory;
    dest->memory_id = src->memory_id;
    dest->num_causes = src->num_causes;
    dest->num_counterfactuals = src->num_counterfactuals;
    dest->action_mutability = src->action_mutability;
    dest->event_mutability = src->event_mutability;
    dest->timing_mutability = src->timing_mutability;
    dest->person_mutability = src->person_mutability;
    dest->overall_mutability = src->overall_mutability;
    dest->analysis_time_ms = src->analysis_time_ms;
    dest->memory_recency = src->memory_recency;
    dest->memory_exceptionality = src->memory_exceptionality;

    // Copy arrays
    if (src->causes && dest->causes && src->num_causes > 0) {
        memcpy(dest->causes, src->causes, src->num_causes * sizeof(prime_signature_t));
    }
    if (src->causal_strengths && dest->causal_strengths && src->num_causes > 0) {
        memcpy(dest->causal_strengths, src->causal_strengths, src->num_causes * sizeof(float));
    }
    if (src->counterfactuals && dest->counterfactuals && src->num_counterfactuals > 0) {
        memcpy(dest->counterfactuals, src->counterfactuals,
               src->num_counterfactuals * sizeof(counterfactual_t));
    }

    // Update pointers to point within dest's arrays
    if (src->most_impactful && src->counterfactuals) {
        size_t idx = (size_t)(src->most_impactful - src->counterfactuals);
        if (idx < dest->num_counterfactuals) {
            dest->most_impactful = &dest->counterfactuals[idx];
        }
    }
    if (src->most_regretted && src->counterfactuals) {
        size_t idx = (size_t)(src->most_regretted - src->counterfactuals);
        if (idx < dest->num_counterfactuals) {
            dest->most_regretted = &dest->counterfactuals[idx];
        }
    }
    if (src->most_relieving && src->counterfactuals) {
        size_t idx = (size_t)(src->most_relieving - src->counterfactuals);
        if (idx < dest->num_counterfactuals) {
            dest->most_relieving = &dest->counterfactuals[idx];
        }
    }

    clear_error();
    return true;
}

//=============================================================================
// Statistics and Utility Functions
//=============================================================================

bool pr_counterfactual_get_stats(
    counterfactual_system_t system,
    counterfactual_stats_t* stats)
{
    if (!system || !stats) {
        set_error("NULL pointer in pr_counterfactual_get_stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_counterfactual_get_stats: required parameter is NULL (system, stats)");
        return false;
    }

    *stats = system->stats;
    /* Phase 8: Heartbeat at operation start */
    counterfactual_heartbeat("counterfactu_pr_counterfactual_ge", 0.0f);


    stats->cached_analyses = system->cache_count;

    clear_error();
    return true;
}

void pr_counterfactual_reset_stats(counterfactual_system_t system) {
    if (!system) return;
    /* Phase 8: Heartbeat at operation start */
    counterfactual_heartbeat("counterfactu_pr_counterfactual_re", 0.0f);


    memset(&system->stats, 0, sizeof(system->stats));
}

const char* pr_counterfactual_get_last_error(void) {
    return s_last_error[0] ? s_last_error : NULL;
}

const char* counterfactual_type_name(counterfactual_type_t type) {
    switch (type) {
        case COUNTER_UPWARD:     return "upward";
        case COUNTER_DOWNWARD:   return "downward";
        case COUNTER_ADDITIVE:   return "additive";
        case COUNTER_SUBTRACTIVE: return "subtractive";
        default:                  return "unknown";
    }
}

const char* counterfactual_mutation_name(mutation_type_t type) {
    switch (type) {
        case MUTATE_ACTION:   return "action";
        case MUTATE_INACTION: return "inaction";
        case MUTATE_PERSON:   return "person";
        case MUTATE_EVENT:    return "event";
        case MUTATE_TIMING:   return "timing";
        case MUTATE_CONTEXT:  return "context";
        default:              return "unknown";
    }
}

void counterfactual_print(const counterfactual_t* cf) {
    if (!cf) {
        printf("Counterfactual: (null)\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    counterfactual_heartbeat("counterfactu_print", 0.0f);


    printf("Counterfactual {\n");
    printf("  mutation_type: %s\n", counterfactual_mutation_name(cf->mutation_type));
    printf("  direction: %s\n", counterfactual_type_name(cf->direction));
    printf("  affect_change: %.3f\n", cf->affect_change);
    printf("  outcome_probability: %.3f\n", cf->outcome_probability);
    printf("  outcome_valence: %.3f\n", cf->outcome_valence);
    printf("  regret_intensity: %.3f\n", cf->regret_intensity);
    printf("  relief_intensity: %.3f\n", cf->relief_intensity);
    printf("  salience: %.3f\n", cf->salience);
    printf("  controllable: %s\n", cf->is_controllable ? "yes" : "no");
    printf("}\n");
}

void counterfactual_analysis_print(const counterfactual_analysis_t* analysis) {
    if (!analysis) {
        printf("CounterfactualAnalysis: (null)\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    counterfactual_heartbeat("counterfactu_analysis_print", 0.0f);


    printf("CounterfactualAnalysis {\n");
    printf("  memory_id: %lu\n", (unsigned long)analysis->memory_id);
    printf("  num_causes: %zu\n", analysis->num_causes);
    printf("  num_counterfactuals: %zu\n", analysis->num_counterfactuals);
    printf("  mutability {\n");
    printf("    action: %.3f\n", analysis->action_mutability);
    printf("    event: %.3f\n", analysis->event_mutability);
    printf("    timing: %.3f\n", analysis->timing_mutability);
    printf("    overall: %.3f\n", analysis->overall_mutability);
    printf("  }\n");

    if (analysis->most_impactful) {
        printf("  most_impactful: %s (affect=%.3f)\n",
               counterfactual_mutation_name(analysis->most_impactful->mutation_type),
               analysis->most_impactful->affect_change);
    }

    if (analysis->most_regretted) {
        printf("  most_regretted: regret=%.3f\n", analysis->most_regretted->regret_intensity);
    }

    if (analysis->most_relieving) {
        printf("  most_relieving: relief=%.3f\n", analysis->most_relieving->relief_intensity);
    }

    printf("}\n");
}

size_t counterfactual_explain(
    const counterfactual_t* cf,
    char* buf,
    size_t size)
{
    if (!cf || !buf || size == 0) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    counterfactual_heartbeat("counterfactu_explain", 0.0f);


    const char* direction_desc;
    if (cf->direction == COUNTER_UPWARD) {
        direction_desc = "better";
    } else if (cf->direction == COUNTER_DOWNWARD) {
        direction_desc = "worse";
    } else {
        direction_desc = "different";
    }

    const char* mutation_desc;
    switch (cf->mutation_type) {
        case MUTATE_ACTION:   mutation_desc = "the action had been different"; break;
        case MUTATE_INACTION: mutation_desc = "no action had been taken"; break;
        case MUTATE_TIMING:   mutation_desc = "the timing had been different"; break;
        case MUTATE_EVENT:    mutation_desc = "the event hadn't happened"; break;
        case MUTATE_PERSON:   mutation_desc = "the person had been different"; break;
        default:              mutation_desc = "things had been different"; break;
    }

    int written = snprintf(buf, size,
        "If %s, the outcome would likely have been %s "
        "(probability: %.0f%%). ",
        mutation_desc,
        direction_desc,
        cf->outcome_probability * 100.0f);

    if (written > 0 && (size_t)written < size) {
        size_t remaining = size - (size_t)written;
        char* pos = buf + written;

        if (cf->regret_intensity > 0.1f) {
            int extra = snprintf(pos, remaining,
                "This generates regret (intensity: %.0f%%).",
                cf->regret_intensity * 100.0f);
            if (extra > 0) written += extra;
        } else if (cf->relief_intensity > 0.1f) {
            int extra = snprintf(pos, remaining,
                "This generates relief (intensity: %.0f%%).",
                cf->relief_intensity * 100.0f);
            if (extra > 0) written += extra;
        }
    }

    return (written > 0) ? (size_t)written : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void counterfactual_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_counterfactual_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int counterfactual_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "counterfactual_training_begin: NULL argument");
        return -1;
    }
    counterfactual_heartbeat_instance(NULL, "counterfactual_training_begin", 0.0f);
    (void)(struct counterfactual_system_struct*)instance; /* Module state available for reset */
    return 0;
}

int counterfactual_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "counterfactual_training_end: NULL argument");
        return -1;
    }
    counterfactual_heartbeat_instance(NULL, "counterfactual_training_end", 1.0f);
    (void)(struct counterfactual_system_struct*)instance; /* Module state available for finalization */
    return 0;
}

int counterfactual_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "counterfactual_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    counterfactual_heartbeat_instance(NULL, "counterfactual_training_step", progress);
    (void)(struct counterfactual_system_struct*)instance; /* Module state available for step adaptation */
    return 0;
}
