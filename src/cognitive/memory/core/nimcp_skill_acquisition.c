//=============================================================================
// nimcp_skill_acquisition.c - Skill Acquisition System Implementation
//=============================================================================
/**
 * @file nimcp_skill_acquisition.c
 * @brief Implementation of skill acquisition with power law, transfer, plateaus
 *
 * Implements the power law of practice, skill transfer computation,
 * learning plateau detection, and deliberate practice planning.
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#include "cognitive/memory/core/nimcp_skill_acquisition.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <float.h>
#include <stdatomic.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(skill_acquisition, MESH_ADAPTER_CATEGORY_MEMORY)


//=============================================================================
// Thread-local Error Storage
//=============================================================================

#ifdef _WIN32
    #define THREAD_LOCAL __declspec(thread)
#else
    #define THREAD_LOCAL _Thread_local
#endif

static THREAD_LOCAL char g_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* msg) {
    if (msg) {
        strncpy(g_last_error, msg, sizeof(g_last_error) - 1);
        g_last_error[sizeof(g_last_error) - 1] = '\0';
    }
}

//=============================================================================
// Internal Constants
//=============================================================================

/** Initial capacity for skills */
#define SKILL_INITIAL_CAPACITY 32

/** Growth factor for dynamic arrays */
#define SKILL_GROWTH_FACTOR 2

/** Minimum trials for power law fitting */
#define SKILL_MIN_TRIALS_FOR_FIT 10

/** Default improvement window for rate calculation */
#define SKILL_DEFAULT_IMPROVEMENT_WINDOW 20

/** Variance ratio threshold for plateau */
#define SKILL_PLATEAU_VARIANCE_RATIO 0.05f

//=============================================================================
// Helper Functions - Math
//=============================================================================

/**
 * @brief Compute power law performance: P(n) = a * n^(-b) + c
 */
static float power_law_predict(const power_law_state_t* state, size_t trial) {
    if (!state || trial == 0) return FLT_MAX;

    float n = (float)trial;
    return state->initial_performance * powf(n, -state->learning_rate) + state->asymptote;
}

/**
 * @brief Compute mean of float array
 */
static float compute_mean(const float* data, size_t count) {
    if (!data || count == 0) return 0.0f;

    float sum = 0.0f;
    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            skill_acquisition_heartbeat("skill_acquis_loop",
                             (float)(i + 1) / (float)count);
        }

        sum += data[i];
    }
    return sum / (float)count;
}

/**
 * @brief Compute variance of float array
 */
static float compute_variance(const float* data, size_t count) {
    if (!data || count < 2) return 0.0f;

    float mean = compute_mean(data, count);
    float sum_sq_diff = 0.0f;

    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            skill_acquisition_heartbeat("skill_acquis_loop",
                             (float)(i + 1) / (float)count);
        }

        float diff = data[i] - mean;
        sum_sq_diff += diff * diff;
    }

    return sum_sq_diff / (float)(count - 1);
}

/**
 * @brief Compute standard deviation
 */
static float compute_std(const float* data, size_t count) {
    float var = compute_variance(data, count);
    return sqrtf(var);
}

/**
 * @brief Compute linear regression slope
 */
static float compute_slope(const float* y, size_t count) {
    if (!y || count < 2) return 0.0f;

    float sum_x = 0.0f, sum_y = 0.0f, sum_xy = 0.0f, sum_xx = 0.0f;

    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            skill_acquisition_heartbeat("skill_acquis_loop",
                             (float)(i + 1) / (float)count);
        }

        float x = (float)i;
        sum_x += x;
        sum_y += y[i];
        sum_xy += x * y[i];
        sum_xx += x * x;
    }

    float n = (float)count;
    float denom = n * sum_xx - sum_x * sum_x;
    if (fabsf(denom) < SKILL_EPSILON) return 0.0f;

    return (n * sum_xy - sum_x * sum_y) / denom;
}

/**
 * @brief Compute R-squared for power law fit
 */
static float compute_r_squared(const float* actual, const float* predicted, size_t count) {
    if (!actual || !predicted || count < 2) return 0.0f;

    float mean_actual = compute_mean(actual, count);
    float ss_tot = 0.0f, ss_res = 0.0f;

    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            skill_acquisition_heartbeat("skill_acquis_loop",
                             (float)(i + 1) / (float)count);
        }

        float diff_tot = actual[i] - mean_actual;
        float diff_res = actual[i] - predicted[i];
        ss_tot += diff_tot * diff_tot;
        ss_res += diff_res * diff_res;
    }

    if (ss_tot < SKILL_EPSILON) return 0.0f;

    return 1.0f - (ss_res / ss_tot);
}

//=============================================================================
// Helper Functions - Skill Management
//=============================================================================

/**
 * @brief Find skill state by ID
 */
static skill_acquisition_state_t* find_state(skill_acquisition_t* sa, uint64_t skill_id) {
    if (!sa) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sa is NULL");

        return NULL;

    }

    for (size_t i = 0; i < sa->num_states; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sa->num_states > 256) {
            skill_acquisition_heartbeat("skill_acquis_loop",
                             (float)(i + 1) / (float)sa->num_states);
        }

        if (sa->states[i] && sa->states[i]->skill &&
            sa->states[i]->skill->skill_id == skill_id) {
            return sa->states[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_state: operation failed");
    return NULL;
}

/**
 * @brief Get skill index in states array
 */
static size_t find_state_index(skill_acquisition_t* sa, uint64_t skill_id) {
    if (!sa) return SIZE_MAX;

    for (size_t i = 0; i < sa->num_states; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sa->num_states > 256) {
            skill_acquisition_heartbeat("skill_acquis_loop",
                             (float)(i + 1) / (float)sa->num_states);
        }

        if (sa->states[i] && sa->states[i]->skill &&
            sa->states[i]->skill->skill_id == skill_id) {
            return i;
        }
    }
    return SIZE_MAX;
}

/**
 * @brief Initialize power law state with defaults
 */
static void init_power_law(power_law_state_t* state, const skill_acquisition_config_t* config) {
    if (!state) return;

    state->initial_performance = config ? config->default_initial_performance
                                        : SKILL_DEFAULT_INITIAL_PERFORMANCE;
    state->learning_rate = config ? config->default_learning_rate
                                  : SKILL_DEFAULT_LEARNING_RATE;
    state->asymptote = config ? config->default_asymptote
                              : SKILL_DEFAULT_ASYMPTOTE;
    state->practice_count = 0;
    state->fit_r_squared = 0.0f;
    state->fit_valid = false;
}

/**
 * @brief Create new skill acquisition state
 */
static skill_acquisition_state_t* create_state(procedural_skill_t* skill,
                                               const skill_acquisition_config_t* config) {
    skill_acquisition_state_t* state = (skill_acquisition_state_t*)nimcp_calloc(
        1, sizeof(skill_acquisition_state_t));
    if (!state) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate state");

        return NULL;

    }

    state->skill = skill;

    // Initialize power law
    init_power_law(&state->power_law, config);

    // Allocate performance history
    size_t history_len = config ? config->default_history_len : SKILL_DEFAULT_HISTORY_LEN;
    state->performance_history = (float*)nimcp_calloc(history_len, sizeof(float));
    if (!state->performance_history) {
        nimcp_free(state);
        state = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_power_law: state->performance_history is NULL");
        return NULL;
    }
    state->history_len = history_len;
    state->history_count = 0;

    // Allocate transfers
    state->max_transfers = SKILL_MAX_TRANSFERS;
    state->transfers = (skill_transfer_t*)nimcp_calloc(state->max_transfers, sizeof(skill_transfer_t));
    if (!state->transfers) {
        nimcp_free(state->performance_history);
        nimcp_free(state);
        state = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_power_law: state->transfers is NULL");
        return NULL;
    }
    state->num_transfers = 0;

    // Allocate plateaus
    state->max_plateaus = SKILL_MAX_PLATEAUS;
    state->plateaus = (learning_plateau_t*)nimcp_calloc(state->max_plateaus, sizeof(learning_plateau_t));
    if (!state->plateaus) {
        nimcp_free(state->transfers);
        nimcp_free(state->performance_history);
        nimcp_free(state);
        state = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_power_law: state->plateaus is NULL");
        return NULL;
    }
    state->num_plateaus = 0;
    state->current_plateau = NULL;

    // Allocate step-level tracking
    if (skill && skill->num_steps > 0) {
        state->step_errors = (size_t*)nimcp_calloc(skill->num_steps, sizeof(size_t));
        state->step_practice_count = (size_t*)nimcp_calloc(skill->num_steps, sizeof(size_t));
        if (!state->step_practice_count) return NULL;
        state->step_difficulty = (float*)nimcp_calloc(skill->num_steps, sizeof(float));

        if (!state->step_errors || !state->step_practice_count || !state->step_difficulty) {
            nimcp_free(state->step_errors);
            nimcp_free(state->step_practice_count);
            nimcp_free(state->step_difficulty);
            nimcp_free(state->plateaus);
            nimcp_free(state->transfers);
            nimcp_free(state->performance_history);
            nimcp_free(state);
            state = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_power_law: required parameter is NULL (state->step_errors, state->step_practice_count, state->step_difficulty)");
            return NULL;
        }

        // Initialize difficulty to 0.5 (unknown)
        for (size_t i = 0; i < skill->num_steps; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && skill->num_steps > 256) {
                skill_acquisition_heartbeat("skill_acquis_loop",
                                 (float)(i + 1) / (float)skill->num_steps);
            }

            state->step_difficulty[i] = 0.5f;
        }
    }

    state->most_difficult_step = 0;
    state->practice_quality = PRACTICE_NAIVE;
    state->feedback_frequency = 0.5f;
    state->focus_intensity = 0.5f;

    state->total_practice_time_ms = 0;
    state->best_performance = FLT_MAX;
    state->worst_performance = 0.0f;
    state->recent_improvement = 0.0f;

    return state;
}

/**
 * @brief Destroy skill acquisition state
 */
static void destroy_state(skill_acquisition_state_t* state) {
    if (!state) return;

    // Free skill (owned by state)
    if (state->skill) {
        if (state->skill->step_names) {
            for (size_t i = 0; i < state->skill->num_steps; i++) {
                nimcp_free(state->skill->step_names[i]);
            }
            nimcp_free(state->skill->step_names);
        }
        if (state->skill->signature) {
            prime_sig_destroy(state->skill->signature);
        }
        nimcp_free(state->skill);
    }

    nimcp_free(state->performance_history);
    nimcp_free(state->transfers);
    nimcp_free(state->plateaus);
    nimcp_free(state->step_errors);
    nimcp_free(state->step_practice_count);
    nimcp_free(state->step_difficulty);
    nimcp_free(state);
    state = NULL;
}

/**
 * @brief Update step difficulty estimates
 */
static void update_step_difficulty(skill_acquisition_state_t* state) {
    if (!state || !state->skill || state->skill->num_steps == 0) return;
    if (!state->step_errors || !state->step_practice_count) return;

    float max_difficulty = 0.0f;
    size_t hardest_step = 0;

    for (size_t i = 0; i < state->skill->num_steps; i++) {
        if (state->step_practice_count[i] > 0) {
            // Difficulty = error_rate
            float error_rate = (float)state->step_errors[i] / (float)state->step_practice_count[i];
            state->step_difficulty[i] = error_rate;

            if (error_rate > max_difficulty) {
                max_difficulty = error_rate;
                hardest_step = i;
            }
        }
    }

    state->most_difficult_step = hardest_step;
}

/**
 * @brief Check for plateau condition
 */
static bool check_plateau_condition(skill_acquisition_state_t* state,
                                    const skill_acquisition_config_t* config) {
    if (!state || state->history_count < config->min_trials_for_plateau) {
        return false;
    }

    size_t window = config->plateau_window;
    if (state->history_count < window) {
        window = state->history_count;
    }

    // Get recent performance window
    size_t start = state->history_count - window;
    float* recent = &state->performance_history[start];

    // Compute variance in window
    float variance = compute_variance(recent, window);
    float mean = compute_mean(recent, window);

    // Coefficient of variation (CV) = std / mean
    float cv = (mean > SKILL_EPSILON) ? sqrtf(variance) / (fabsf(mean) > 1e-7f ? mean : 1e-7f) : 0.0f;

    // Check if variance is below threshold (plateau)
    if (cv < config->plateau_threshold) {
        // Also check if there's no improvement trend
        float slope = compute_slope(recent, window);
        // For performance (lower is better), positive slope means worsening
        // We're on plateau if slope is near zero or positive
        if (slope > -config->plateau_threshold) {
            return true;
        }
    }

    return false;
}

/**
 * @brief Start tracking a new plateau
 */
static void start_plateau(skill_acquisition_state_t* state) {
    if (!state || state->num_plateaus >= state->max_plateaus) return;

    learning_plateau_t* plateau = &state->plateaus[state->num_plateaus];

    plateau->start_trial = state->power_law.practice_count;
    plateau->end_trial = 0;
    plateau->plateau_performance = state->performance_history[state->history_count - 1];
    plateau->performance_variance = 0.0f;
    plateau->duration = 0;
    plateau->overcome = false;
    plateau->strategy_used = PLATEAU_STRATEGY_NONE;

    state->current_plateau = plateau;
    state->num_plateaus++;
}

/**
 * @brief End current plateau
 */
static void end_plateau(skill_acquisition_state_t* state, plateau_strategy_t strategy) {
    if (!state || !state->current_plateau) return;

    state->current_plateau->end_trial = state->power_law.practice_count;
    state->current_plateau->duration =
        state->current_plateau->end_trial - state->current_plateau->start_trial;
    state->current_plateau->overcome = true;
    state->current_plateau->strategy_used = strategy;

    state->current_plateau = NULL;
}

//=============================================================================
// Power Law Fitting (Levenberg-Marquardt-style iterative fitting)
//=============================================================================

/**
 * @brief Fit power law parameters to performance data
 *
 * Uses iterative least-squares fitting to find optimal a, b, c
 */
static skill_error_t fit_power_law_internal(skill_acquisition_state_t* state) {
    if (!state || state->history_count < SKILL_MIN_TRIALS_FOR_FIT) {
        return SKILL_ERROR_INSUFFICIENT_DATA;
    }

    // Initial guesses
    float a = state->performance_history[0];  // Initial performance
    float c = state->best_performance * 0.9f; // Asymptote near best
    float b = SKILL_DEFAULT_LEARNING_RATE;    // Default learning rate

    // Ensure asymptote is reasonable
    if (c < 1.0f) c = 1.0f;
    if (a < c) a = c * 2.0f;

    // Iterative fitting
    float best_error = FLT_MAX;
    float best_a = a, best_b = b, best_c = c;

    for (int iter = 0; iter < SKILL_FIT_MAX_ITERATIONS; iter++) {
        /* Phase 8: Loop progress heartbeat */
        if ((iter & 0xFF) == 0 && SKILL_FIT_MAX_ITERATIONS > 256) {
            skill_acquisition_heartbeat("skill_acquis_loop",
                             (float)(iter + 1) / (float)SKILL_FIT_MAX_ITERATIONS);
        }

        // Compute current error
        float total_error = 0.0f;

        for (size_t i = 0; i < state->history_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && state->history_count > 256) {
                skill_acquisition_heartbeat("skill_acquis_loop",
                                 (float)(i + 1) / (float)state->history_count);
            }

            float n = (float)(i + 1);
            float predicted = a * powf(n, -b) + c;
            float diff = state->performance_history[i] - predicted;
            total_error += diff * diff;
        }

        total_error /= (float)state->history_count;

        if (total_error < best_error) {
            best_error = total_error;
            best_a = a;
            best_b = b;
            best_c = c;
        }

        // Convergence check
        if (total_error < SKILL_FIT_TOLERANCE) {
            break;
        }

        // Compute gradients (numerical)
        float da = 0.01f * a;
        float db = 0.01f;
        float dc = 0.01f * c;

        // Gradient for a
        float error_plus_a = 0.0f;
        for (size_t i = 0; i < state->history_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && state->history_count > 256) {
                skill_acquisition_heartbeat("skill_acquis_loop",
                                 (float)(i + 1) / (float)state->history_count);
            }

            float n = (float)(i + 1);
            float predicted = (a + da) * powf(n, -b) + c;
            float diff = state->performance_history[i] - predicted;
            error_plus_a += diff * diff;
        }
        error_plus_a /= (float)state->history_count;

        float grad_a = (error_plus_a - total_error) / da;

        // Gradient for b
        float error_plus_b = 0.0f;
        for (size_t i = 0; i < state->history_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && state->history_count > 256) {
                skill_acquisition_heartbeat("skill_acquis_loop",
                                 (float)(i + 1) / (float)state->history_count);
            }

            float n = (float)(i + 1);
            float predicted = a * powf(n, -(b + db)) + c;
            float diff = state->performance_history[i] - predicted;
            error_plus_b += diff * diff;
        }
        error_plus_b /= (float)state->history_count;

        float grad_b = (error_plus_b - total_error) / db;

        // Gradient for c
        float error_plus_c = 0.0f;
        for (size_t i = 0; i < state->history_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && state->history_count > 256) {
                skill_acquisition_heartbeat("skill_acquis_loop",
                                 (float)(i + 1) / (float)state->history_count);
            }

            float n = (float)(i + 1);
            float predicted = a * powf(n, -b) + (c + dc);
            float diff = state->performance_history[i] - predicted;
            error_plus_c += diff * diff;
        }
        error_plus_c /= (float)state->history_count;

        float grad_c = (error_plus_c - total_error) / dc;

        // Update with adaptive learning rate
        float lr = 0.1f / (1.0f + 0.01f * (float)iter);

        a -= lr * grad_a;
        b -= lr * grad_b;
        c -= lr * grad_c;

        // Clamp to valid ranges
        if (a < 1.0f) a = 1.0f;
        if (b < 0.01f) b = 0.01f;
        if (b > 1.0f) b = 1.0f;
        if (c < 0.0f) c = 0.0f;
        if (c > a) c = a * 0.5f;
    }

    // Store best fit
    state->power_law.initial_performance = best_a;
    state->power_law.learning_rate = best_b;
    state->power_law.asymptote = best_c;

    // Compute R-squared
    float* predicted = (float*)nimcp_calloc(state->history_count, sizeof(float));
    if (predicted) {
        for (size_t i = 0; i < state->history_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && state->history_count > 256) {
                skill_acquisition_heartbeat("skill_acquis_loop",
                                 (float)(i + 1) / (float)state->history_count);
            }

            predicted[i] = power_law_predict(&state->power_law, i + 1);
        }
        state->power_law.fit_r_squared = compute_r_squared(
            state->performance_history, predicted, state->history_count);
        nimcp_free(predicted);
        predicted = NULL;
    }

    state->power_law.fit_valid = (state->power_law.fit_r_squared > 0.5f);

    return SKILL_SUCCESS;
}

//=============================================================================
// Configuration Functions
//=============================================================================

NIMCP_EXPORT skill_acquisition_config_t skill_acquisition_config_default(void) {
    skill_acquisition_config_t config = {
        .default_initial_performance = SKILL_DEFAULT_INITIAL_PERFORMANCE,
        .default_learning_rate = SKILL_DEFAULT_LEARNING_RATE,
        .default_asymptote = SKILL_DEFAULT_ASYMPTOTE,
        .default_history_len = SKILL_DEFAULT_HISTORY_LEN,
        .plateau_threshold = SKILL_DEFAULT_PLATEAU_THRESHOLD,
        .plateau_window = SKILL_DEFAULT_PLATEAU_WINDOW,
        .min_trials_for_plateau = SKILL_MIN_TRIALS_FOR_PLATEAU,
        .weak_point_threshold = SKILL_DEFAULT_WEAK_POINT_THRESHOLD,
        .element_weight = 0.3f,
        .surface_weight = 0.2f,
        .structural_weight = 0.3f,
        .signature_weight = 0.2f,
        .enable_statistics = true,
        .auto_fit_power_law = true,
        .fit_interval = 10
    };
    return config;
}

NIMCP_EXPORT bool skill_acquisition_config_validate(const skill_acquisition_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "skill_acquisition_config_validate: config is NULL");
        return false;
    }

    if (config->default_initial_performance <= 0) {
        return false;
    }
    if (config->default_learning_rate <= 0 || config->default_learning_rate > 1) {
        return false;
    }
    if (config->default_asymptote < 0) {
        return false;
    }
    if (config->default_history_len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "skill_acquisition_config_validate: config->default_history_len is zero");
        return false;
    }
    if (config->plateau_threshold < 0 || config->plateau_threshold > 1) {
        return false;
    }
    if (config->plateau_window == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "skill_acquisition_config_validate: config->plateau_window is zero");
        return false;
    }
    if (config->weak_point_threshold < 0 || config->weak_point_threshold > 1) {
        return false;
    }

    // Weights should sum to approximately 1.0
    float weight_sum = config->element_weight + config->surface_weight +
                       config->structural_weight + config->signature_weight;
    if (fabsf(weight_sum - 1.0f) > 0.1f) {
        return false;
    }

    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

NIMCP_EXPORT skill_acquisition_t* skill_acquisition_create(
    const skill_acquisition_config_t* config
) {
    skill_acquisition_config_t cfg;
    if (config) {
        cfg = *config;
        if (!skill_acquisition_config_validate(&cfg)) {
            set_error("Invalid configuration");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "skill_acquisition_create: skill_acquisition_config_validate is NULL");
            return NULL;
        }
    } else {
        cfg = skill_acquisition_config_default();
    }

    skill_acquisition_t* sa = (skill_acquisition_t*)nimcp_calloc(1, sizeof(skill_acquisition_t));
    if (!sa) {
        set_error("Memory allocation failed for skill acquisition");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "skill_acquisition_create: sa is NULL");
        return NULL;
    }

    // Allocate states array
    sa->max_states = SKILL_INITIAL_CAPACITY;
    sa->states = (skill_acquisition_state_t**)nimcp_calloc(sa->max_states,
                                                      sizeof(skill_acquisition_state_t*));
    if (!sa->states) {
        set_error("Memory allocation failed for states array");
        nimcp_free(sa);
        sa = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "skill_acquisition_create: sa->states is NULL");
        return NULL;
    }
    sa->num_states = 0;
    sa->current = NULL;

    // Initialize transfer matrix as NULL (created on demand)
    sa->transfer_matrix = NULL;
    sa->matrix_size = 0;

    // Store config
    sa->config = cfg;
    sa->plateau_threshold = cfg.plateau_threshold;
    sa->plateau_window = cfg.plateau_window;

    // Initialize stats
    sa->total_trials_recorded = 0;
    sa->plateaus_detected = 0;
    sa->plateaus_overcome = 0;
    sa->avg_transfer_magnitude = 0.0f;

    return sa;
}

NIMCP_EXPORT void skill_acquisition_destroy(skill_acquisition_t* sa) {
    if (!sa) return;

    // Destroy all states
    if (sa->states) {
        for (size_t i = 0; i < sa->num_states; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && sa->num_states > 256) {
                skill_acquisition_heartbeat("skill_acquis_loop",
                                 (float)(i + 1) / (float)sa->num_states);
            }

            destroy_state(sa->states[i]);
        }
        nimcp_free(sa->states);
    }

    // Free transfer matrix
    if (sa->transfer_matrix) {
        for (size_t i = 0; i < sa->matrix_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && sa->matrix_size > 256) {
                skill_acquisition_heartbeat("skill_acquis_loop",
                                 (float)(i + 1) / (float)sa->matrix_size);
            }

            nimcp_free(sa->transfer_matrix[i]);
        }
        nimcp_free(sa->transfer_matrix);
    }

    nimcp_free(sa);
    sa = NULL;
}

NIMCP_EXPORT skill_error_t skill_acquisition_reset(skill_acquisition_t* sa) {
    if (!sa) {
        set_error("NULL skill acquisition");
        return SKILL_ERROR_NULL_POINTER;
    }

    // Destroy all states
    for (size_t i = 0; i < sa->num_states; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sa->num_states > 256) {
            skill_acquisition_heartbeat("skill_acquis_loop",
                             (float)(i + 1) / (float)sa->num_states);
        }

        destroy_state(sa->states[i]);
        sa->states[i] = NULL;
    }
    sa->num_states = 0;
    sa->current = NULL;

    // Free transfer matrix
    if (sa->transfer_matrix) {
        for (size_t i = 0; i < sa->matrix_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && sa->matrix_size > 256) {
                skill_acquisition_heartbeat("skill_acquis_loop",
                                 (float)(i + 1) / (float)sa->matrix_size);
            }

            nimcp_free(sa->transfer_matrix[i]);
        }
        nimcp_free(sa->transfer_matrix);
        sa->transfer_matrix = NULL;
        sa->matrix_size = 0;
    }

    // Reset stats
    sa->total_trials_recorded = 0;
    sa->plateaus_detected = 0;
    sa->plateaus_overcome = 0;
    sa->avg_transfer_magnitude = 0.0f;

    return SKILL_SUCCESS;
}

//=============================================================================
// Skill Registration Functions
//=============================================================================

NIMCP_EXPORT uint64_t skill_acquisition_register_skill(
    skill_acquisition_t* sa,
    const char* name,
    size_t num_steps,
    const char** step_names,
    const prime_signature_t* signature
) {
    if (!sa || !name) {
        set_error("NULL pointer argument");
        return SKILL_INVALID_ID;
    }

    // Expand states array if needed
    if (sa->num_states >= sa->max_states) {
        size_t new_max = sa->max_states * SKILL_GROWTH_FACTOR;
        skill_acquisition_state_t** new_states = (skill_acquisition_state_t**)nimcp_realloc(
            sa->states, new_max * sizeof(skill_acquisition_state_t*));
        if (!new_states) {
            set_error("Failed to expand states array");
            return SKILL_INVALID_ID;
        }
        sa->states = new_states;

        // Initialize new slots to NULL
        for (size_t i = sa->max_states; i < new_max; i++) {
            sa->states[i] = NULL;
        }
        sa->max_states = new_max;
    }

    // Create procedural skill
    procedural_skill_t* skill = (procedural_skill_t*)nimcp_calloc(1, sizeof(procedural_skill_t));
    if (!skill) {
        set_error("Failed to allocate skill");
        return SKILL_INVALID_ID;
    }

    // Generate skill ID (atomic for thread safety)
    static _Atomic uint64_t next_skill_id = 1;
    skill->skill_id = atomic_fetch_add(&next_skill_id, 1);

    // Copy name
    strncpy(skill->name, name, SKILL_MAX_NAME_LEN - 1);
    skill->name[SKILL_MAX_NAME_LEN - 1] = '\0';

    // Set up steps
    skill->num_steps = num_steps;
    if (num_steps > 0 && step_names) {
        skill->step_names = (char**)nimcp_calloc(num_steps, sizeof(char*));
        if (skill->step_names) {
            for (size_t i = 0; i < num_steps; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && num_steps > 256) {
                    skill_acquisition_heartbeat("skill_acquis_loop",
                                     (float)(i + 1) / (float)num_steps);
                }

                if (step_names[i]) {
                    skill->step_names[i] = strdup(step_names[i]);
                    /* NULL on OOM is tolerable - step name will be missing */
                }
            }
        }
    }

    // Copy or compute signature
    if (signature) {
        skill->signature = prime_sig_copy(signature);
    } else {
        // Auto-compute from name
        skill->signature = prime_sig_from_text(name);
    }

    // Initialize quaternion state
    skill->state = quat_identity();

    // Record creation time
    skill->created_time_ms = skill_current_time_ms();

    // Create acquisition state
    skill_acquisition_state_t* state = create_state(skill, &sa->config);
    if (!state) {
        set_error("Failed to create acquisition state");
        if (skill->step_names) {
            for (size_t i = 0; i < num_steps; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && num_steps > 256) {
                    skill_acquisition_heartbeat("skill_acquis_loop",
                                     (float)(i + 1) / (float)num_steps);
                }

                nimcp_free(skill->step_names[i]);
            }
            nimcp_free(skill->step_names);
        }
        if (skill->signature) {
            prime_sig_destroy(skill->signature);
        }
        nimcp_free(skill);
        skill = NULL;
        return SKILL_INVALID_ID;
    }

    // Add to states array
    sa->states[sa->num_states++] = state;

    return skill->skill_id;
}

NIMCP_EXPORT skill_error_t skill_acquisition_unregister_skill(
    skill_acquisition_t* sa,
    uint64_t skill_id
) {
    if (!sa) {
        set_error("NULL skill acquisition");
        return SKILL_ERROR_NULL_POINTER;
    }

    size_t idx = find_state_index(sa, skill_id);
    if (idx == SIZE_MAX) {
        set_error("Skill not found");
        return SKILL_ERROR_NOT_FOUND;
    }

    // Destroy state
    destroy_state(sa->states[idx]);

    // Shift remaining states down
    for (size_t i = idx; i < sa->num_states - 1; i++) {
        sa->states[i] = sa->states[i + 1];
    }
    sa->states[--sa->num_states] = NULL;

    // Clear current if it was this skill
    if (sa->current && sa->current->skill && sa->current->skill->skill_id == skill_id) {
        sa->current = NULL;
    }

    return SKILL_SUCCESS;
}

NIMCP_EXPORT procedural_skill_t* skill_acquisition_get_skill(
    skill_acquisition_t* sa,
    uint64_t skill_id
) {
    skill_acquisition_state_t* state = find_state(sa, skill_id);
    return state ? state->skill : NULL;
}

NIMCP_EXPORT skill_acquisition_state_t* skill_acquisition_get_state(
    skill_acquisition_t* sa,
    uint64_t skill_id
) {
    return find_state(sa, skill_id);
}

NIMCP_EXPORT skill_error_t skill_acquisition_set_current(
    skill_acquisition_t* sa,
    uint64_t skill_id
) {
    if (!sa) {
        set_error("NULL skill acquisition");
        return SKILL_ERROR_NULL_POINTER;
    }

    skill_acquisition_state_t* state = find_state(sa, skill_id);
    if (!state) {
        set_error("Skill not found");
        return SKILL_ERROR_NOT_FOUND;
    }

    sa->current = state;
    return SKILL_SUCCESS;
}

//=============================================================================
// Trial Recording Functions
//=============================================================================

NIMCP_EXPORT skill_error_t skill_acquisition_record_trial(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    const trial_result_t* result
) {
    if (!sa || !result) {
        set_error("NULL pointer argument");
        return SKILL_ERROR_NULL_POINTER;
    }

    skill_acquisition_state_t* state = find_state(sa, skill_id);
    if (!state) {
        set_error("Skill not found");
        return SKILL_ERROR_NOT_FOUND;
    }

    // Add to performance history (circular buffer)
    size_t idx = state->history_count % state->history_len;
    state->performance_history[idx] = result->performance;

    if (state->history_count < state->history_len) {
        state->history_count++;
    }

    // Update practice count
    state->power_law.practice_count++;

    // Update best/worst
    if (result->performance < state->best_performance) {
        state->best_performance = result->performance;
    }
    if (result->performance > state->worst_performance) {
        state->worst_performance = result->performance;
    }

    // Record step-level data if provided
    if (result->step_performances && result->num_steps > 0 && state->skill) {
        size_t num_steps = (result->num_steps < state->skill->num_steps)
                          ? result->num_steps : state->skill->num_steps;

        for (size_t i = 0; i < num_steps; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && num_steps > 256) {
                skill_acquisition_heartbeat("skill_acquis_loop",
                                 (float)(i + 1) / (float)num_steps);
            }

            state->step_practice_count[i]++;
            // Consider high performance (time) as an error indicator
            float mean_perf = compute_mean(state->performance_history, state->history_count);
            if (result->step_performances[i] > mean_perf * 1.5f) {
                state->step_errors[i]++;
            }
        }

        update_step_difficulty(state);
    }

    // Update total practice time
    if (result->timestamp_ms > 0) {
        state->total_practice_time_ms += result->timestamp_ms;
    }

    // Update feedback tracking
    if (result->feedback_given > 0) {
        // Running average of feedback frequency
        float n = (float)state->power_law.practice_count;
        state->feedback_frequency =
            ((n - 1) * state->feedback_frequency + result->feedback_given) / (fabsf(n) > 1e-7f ? n : 1e-7f);
    }

    // Check for plateau
    bool was_on_plateau = (state->current_plateau != NULL);
    bool is_on_plateau = check_plateau_condition(state, &sa->config);

    if (!was_on_plateau && is_on_plateau) {
        // Starting new plateau
        start_plateau(state);
        sa->plateaus_detected++;
    } else if (was_on_plateau && !is_on_plateau) {
        // Exiting plateau (natural improvement)
        end_plateau(state, PLATEAU_STRATEGY_NONE);
        sa->plateaus_overcome++;
    }

    // Auto-fit power law if enabled
    if (sa->config.auto_fit_power_law &&
        state->power_law.practice_count >= SKILL_MIN_TRIALS_FOR_FIT &&
        state->power_law.practice_count % sa->config.fit_interval == 0) {
        fit_power_law_internal(state);
    }

    // Compute recent improvement
    if (state->history_count >= 2) {
        size_t window = SKILL_DEFAULT_IMPROVEMENT_WINDOW;
        if (state->history_count < window) window = state->history_count;

        size_t start = state->history_count - window;
        float* recent = &state->performance_history[start];
        state->recent_improvement = -compute_slope(recent, window);  // Negative slope = improvement
    }

    sa->total_trials_recorded++;

    return SKILL_SUCCESS;
}

NIMCP_EXPORT size_t skill_acquisition_record_trials(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    const trial_result_t* results,
    size_t count
) {
    if (!sa || !results || count == 0) return 0;

    size_t recorded = 0;
    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            skill_acquisition_heartbeat("skill_acquis_loop",
                             (float)(i + 1) / (float)count);
        }

        if (skill_acquisition_record_trial(sa, skill_id, &results[i]) == SKILL_SUCCESS) {
            recorded++;
        }
    }
    return recorded;
}

NIMCP_EXPORT skill_error_t skill_acquisition_record_step_error(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    size_t step_index,
    size_t error_count
) {
    if (!sa) {
        set_error("NULL skill acquisition");
        return SKILL_ERROR_NULL_POINTER;
    }

    skill_acquisition_state_t* state = find_state(sa, skill_id);
    if (!state) {
        set_error("Skill not found");
        return SKILL_ERROR_NOT_FOUND;
    }

    if (!state->skill || step_index >= state->skill->num_steps) {
        set_error("Invalid step index");
        return SKILL_ERROR_INVALID_PARAM;
    }

    state->step_errors[step_index] += error_count;
    update_step_difficulty(state);

    return SKILL_SUCCESS;
}

//=============================================================================
// Performance Prediction Functions
//=============================================================================

NIMCP_EXPORT skill_error_t skill_acquisition_predict_performance(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    size_t trial_number,
    performance_prediction_t* prediction
) {
    if (!sa || !prediction) {
        set_error("NULL pointer argument");
        return SKILL_ERROR_NULL_POINTER;
    }

    skill_acquisition_state_t* state = find_state(sa, skill_id);
    if (!state) {
        set_error("Skill not found");
        return SKILL_ERROR_NOT_FOUND;
    }

    if (trial_number == 0) {
        set_error("Invalid trial number (must be >= 1)");
        return SKILL_ERROR_INVALID_PARAM;
    }

    // Predict using power law
    float predicted = power_law_predict(&state->power_law, trial_number);

    prediction->predicted_performance = predicted;
    prediction->target_trial = trial_number;

    // Compute confidence bounds based on fit quality
    float uncertainty = 0.2f;  // Default 20% uncertainty
    if (state->power_law.fit_valid) {
        uncertainty = 1.0f - state->power_law.fit_r_squared;
    }

    prediction->confidence_low = predicted * (1.0f - uncertainty);
    prediction->confidence_high = predicted * (1.0f + uncertainty);

    // Compute improvement rate (derivative of power law)
    // d/dn[a * n^(-b) + c] = -a * b * n^(-b-1)
    float n = (float)trial_number;
    prediction->improvement_rate = -state->power_law.initial_performance *
                                   state->power_law.learning_rate *
                                   powf(n, -state->power_law.learning_rate - 1.0f);

    return SKILL_SUCCESS;
}

NIMCP_EXPORT skill_error_t skill_acquisition_estimate_time_to_mastery(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    float target_performance,
    mastery_estimate_t* estimate
) {
    if (!sa || !estimate) {
        set_error("NULL pointer argument");
        return SKILL_ERROR_NULL_POINTER;
    }

    skill_acquisition_state_t* state = find_state(sa, skill_id);
    if (!state) {
        set_error("Skill not found");
        return SKILL_ERROR_NOT_FOUND;
    }

    const power_law_state_t* pl = &state->power_law;

    estimate->target_performance = target_performance;
    estimate->current_performance = state->history_count > 0
        ? state->performance_history[state->history_count - 1]
        : pl->initial_performance;

    // Check if already at target
    if (estimate->current_performance <= target_performance) {
        estimate->trials_to_mastery = 0;
        estimate->time_to_mastery_ms = 0;
        estimate->confidence = 0.9f;
        estimate->already_mastered = true;
        return SKILL_SUCCESS;
    }
    estimate->already_mastered = false;

    // Check if target is achievable (below asymptote)
    if (target_performance < pl->asymptote) {
        // Target is below asymptote - compute trials to asymptote instead
        target_performance = pl->asymptote * 1.01f;  // Slightly above asymptote
    }

    // Solve: target = a * n^(-b) + c
    // target - c = a * n^(-b)
    // (target - c) / a = n^(-b)
    // log((target - c) / a) = -b * log(n)
    // log(n) = -log((target - c) / a) / b
    // n = exp(-log((target - c) / a) / b)

    float numerator = target_performance - pl->asymptote;
    if (numerator <= 0) numerator = 0.001f;

    float ratio = numerator / pl->initial_performance;
    if (ratio <= 0) ratio = 0.001f;
    if (ratio > 1) ratio = 1.0f;

    float log_ratio = logf(ratio);
    float target_trial = expf(-log_ratio / pl->learning_rate);

    // Trials needed from current position
    size_t current_trial = pl->practice_count;
    if (current_trial == 0) current_trial = 1;

    if (target_trial > (float)current_trial) {
        estimate->trials_to_mastery = (size_t)(target_trial - (float)current_trial);
    } else {
        estimate->trials_to_mastery = 0;
    }

    // Estimate time based on average trial duration
    float avg_trial_ms = 0.0f;
    if (state->total_practice_time_ms > 0 && pl->practice_count > 0) {
        avg_trial_ms = (float)state->total_practice_time_ms / (float)pl->practice_count;
    } else {
        avg_trial_ms = 60000.0f;  // Default 1 minute per trial
    }

    estimate->time_to_mastery_ms = (float)estimate->trials_to_mastery * avg_trial_ms;

    // Confidence based on fit quality and extrapolation distance
    estimate->confidence = pl->fit_valid ? pl->fit_r_squared * 0.8f : 0.3f;

    // Reduce confidence for far extrapolations
    float extrapolation_ratio = target_trial / (float)current_trial;
    if (extrapolation_ratio > 10.0f) {
        estimate->confidence *= 0.5f;
    } else if (extrapolation_ratio > 5.0f) {
        estimate->confidence *= 0.7f;
    }

    return SKILL_SUCCESS;
}

NIMCP_EXPORT float skill_acquisition_get_current_performance(
    skill_acquisition_t* sa,
    uint64_t skill_id
) {
    skill_acquisition_state_t* state = find_state(sa, skill_id);
    if (!state || state->history_count == 0) {
        return NAN;
    }

    return state->performance_history[state->history_count - 1];
}

//=============================================================================
// Transfer Computation Functions
//=============================================================================

NIMCP_EXPORT skill_error_t skill_acquisition_compute_transfer(
    skill_acquisition_t* sa,
    uint64_t source_skill_id,
    uint64_t target_skill_id,
    skill_transfer_t* transfer
) {
    if (!sa || !transfer) {
        set_error("NULL pointer argument");
        return SKILL_ERROR_NULL_POINTER;
    }

    skill_acquisition_state_t* source = find_state(sa, source_skill_id);
    skill_acquisition_state_t* target = find_state(sa, target_skill_id);

    if (!source || !target) {
        set_error("Skill not found");
        return SKILL_ERROR_NOT_FOUND;
    }

    transfer->source_skill_id = source_skill_id;
    transfer->target_skill_id = target_skill_id;

    // Compute element overlap (shared steps)
    float element_overlap = 0.0f;
    if (source->skill && target->skill &&
        source->skill->num_steps > 0 && target->skill->num_steps > 0) {
        // Simple overlap: ratio of min to max steps
        size_t min_steps = (source->skill->num_steps < target->skill->num_steps)
                           ? source->skill->num_steps : target->skill->num_steps;
        size_t max_steps = (source->skill->num_steps > target->skill->num_steps)
                           ? source->skill->num_steps : target->skill->num_steps;
        element_overlap = (float)min_steps / (float)max_steps;
    }
    transfer->element_overlap = element_overlap;

    // Compute prime signature similarity
    float sig_similarity = 0.0f;
    if (source->skill->signature && target->skill->signature) {
        sig_similarity = prime_sig_jaccard(source->skill->signature, target->skill->signature);
    }
    transfer->prime_sig_similarity = sig_similarity;

    // Compute surface similarity (name similarity via signature)
    float surface_similarity = sig_similarity;  // Use signature as proxy
    transfer->surface_similarity = surface_similarity;

    // Compute structural similarity (based on step patterns)
    float structural_similarity = element_overlap;  // Use element overlap as proxy
    transfer->structural_similarity = structural_similarity;

    // Compute weighted transfer magnitude
    float magnitude = sa->config.element_weight * element_overlap +
                      sa->config.surface_weight * surface_similarity +
                      sa->config.structural_weight * structural_similarity +
                      sa->config.signature_weight * sig_similarity;

    // Determine transfer type based on magnitude
    if (magnitude > 0.5f) {
        transfer->type = TRANSFER_POSITIVE;
        transfer->transfer_magnitude = magnitude;
    } else if (magnitude < 0.2f) {
        transfer->type = TRANSFER_NEUTRAL;
        transfer->transfer_magnitude = 0.0f;
    } else {
        // Check for negative transfer (high surface similarity but low structural)
        if (surface_similarity > 0.6f && structural_similarity < 0.3f) {
            transfer->type = TRANSFER_NEGATIVE;
            transfer->transfer_magnitude = -(surface_similarity - structural_similarity);
        } else {
            transfer->type = TRANSFER_NEUTRAL;
            transfer->transfer_magnitude = magnitude - 0.35f;  // Small positive
        }
    }

    // Update running average
    sa->avg_transfer_magnitude =
        (sa->avg_transfer_magnitude * 0.9f) + (fabsf(transfer->transfer_magnitude) * 0.1f);

    return SKILL_SUCCESS;
}

NIMCP_EXPORT skill_error_t skill_acquisition_apply_transfer(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    const skill_transfer_t* transfer
) {
    if (!sa || !transfer) {
        set_error("NULL pointer argument");
        return SKILL_ERROR_NULL_POINTER;
    }

    skill_acquisition_state_t* state = find_state(sa, skill_id);
    if (!state) {
        set_error("Skill not found");
        return SKILL_ERROR_NOT_FOUND;
    }

    // Apply transfer effects to power law parameters
    power_law_state_t* pl = &state->power_law;

    switch (transfer->type) {
        case TRANSFER_POSITIVE:
            // Positive transfer: Better starting performance, faster learning
            pl->initial_performance *= (1.0f - 0.3f * transfer->transfer_magnitude);
            pl->learning_rate *= (1.0f + 0.2f * transfer->transfer_magnitude);
            if (pl->learning_rate > 0.8f) pl->learning_rate = 0.8f;
            break;

        case TRANSFER_NEGATIVE:
            // Negative transfer: Worse starting performance, slower learning
            pl->initial_performance *= (1.0f + 0.3f * fabsf(transfer->transfer_magnitude));
            pl->learning_rate *= (1.0f - 0.2f * fabsf(transfer->transfer_magnitude));
            if (pl->learning_rate < 0.1f) pl->learning_rate = 0.1f;
            break;

        case TRANSFER_NEUTRAL:
        default:
            // No change
            break;
    }

    // Record transfer in state
    if (state->num_transfers < state->max_transfers) {
        state->transfers[state->num_transfers++] = *transfer;
    }

    return SKILL_SUCCESS;
}

NIMCP_EXPORT skill_error_t skill_acquisition_find_best_transfer(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    uint64_t* source_skill_id,
    float* similarity
) {
    if (!sa || !source_skill_id || !similarity) {
        set_error("NULL pointer argument");
        return SKILL_ERROR_NULL_POINTER;
    }

    skill_acquisition_state_t* target = find_state(sa, skill_id);
    if (!target) {
        set_error("Skill not found");
        return SKILL_ERROR_NOT_FOUND;
    }

    float best_similarity = -1.0f;
    uint64_t best_source = SKILL_INVALID_ID;

    for (size_t i = 0; i < sa->num_states; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sa->num_states > 256) {
            skill_acquisition_heartbeat("skill_acquis_loop",
                             (float)(i + 1) / (float)sa->num_states);
        }

        if (!sa->states[i] || !sa->states[i]->skill) continue;

        uint64_t candidate_id = sa->states[i]->skill->skill_id;
        if (candidate_id == skill_id) continue;  // Skip self

        skill_transfer_t transfer;
        if (skill_acquisition_compute_transfer(sa, candidate_id, skill_id, &transfer) == SKILL_SUCCESS) {
            if (transfer.type == TRANSFER_POSITIVE &&
                transfer.transfer_magnitude > best_similarity) {
                best_similarity = transfer.transfer_magnitude;
                best_source = candidate_id;
            }
        }
    }

    if (best_source == SKILL_INVALID_ID) {
        set_error("No suitable transfer source found");
        return SKILL_ERROR_NOT_FOUND;
    }

    *source_skill_id = best_source;
    *similarity = best_similarity;

    return SKILL_SUCCESS;
}

NIMCP_EXPORT float skill_acquisition_get_transfer(
    skill_acquisition_t* sa,
    uint64_t source_skill_id,
    uint64_t target_skill_id
) {
    skill_transfer_t transfer;
    if (skill_acquisition_compute_transfer(sa, source_skill_id, target_skill_id, &transfer)
        != SKILL_SUCCESS) {
        return NAN;
    }
    return transfer.transfer_magnitude;
}

//=============================================================================
// Plateau Detection Functions
//=============================================================================

NIMCP_EXPORT bool skill_acquisition_detect_plateau(
    skill_acquisition_t* sa,
    uint64_t skill_id
) {
    if (!sa) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "skill_acquisition_detect_plateau: sa is NULL");
        return false;
    }

    skill_acquisition_state_t* state = find_state(sa, skill_id);
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "skill_acquisition_detect_plateau: state is NULL");
        return false;
    }

    return (state->current_plateau != NULL);
}

NIMCP_EXPORT learning_plateau_t* skill_acquisition_get_current_plateau(
    skill_acquisition_t* sa,
    uint64_t skill_id
) {
    if (!sa) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sa is NULL");

        return NULL;

    }

    skill_acquisition_state_t* state = find_state(sa, skill_id);
    return state ? state->current_plateau : NULL;
}

NIMCP_EXPORT skill_error_t skill_acquisition_suggest_breakthrough(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    breakthrough_suggestion_t* suggestion
) {
    if (!sa || !suggestion) {
        set_error("NULL pointer argument");
        return SKILL_ERROR_NULL_POINTER;
    }

    skill_acquisition_state_t* state = find_state(sa, skill_id);
    if (!state) {
        set_error("Skill not found");
        return SKILL_ERROR_NOT_FOUND;
    }

    if (!state->current_plateau) {
        set_error("Not currently on plateau");
        return SKILL_ERROR_NOT_ON_PLATEAU;
    }

    memset(suggestion, 0, sizeof(*suggestion));

    size_t plateau_duration = state->power_law.practice_count - state->current_plateau->start_trial;

    // Analyze weak points
    weak_point_analysis_t weak_analysis = {0};
    skill_acquisition_analyze_weak_points(sa, skill_id, &weak_analysis);

    // Choose strategy based on conditions
    if (weak_analysis.num_weak_steps > 0 && weak_analysis.overall_weakness > 0.3f) {
        // Significant weak points - focus on them
        suggestion->primary_strategy = PLATEAU_STRATEGY_FOCUS_WEAK;
        suggestion->secondary_strategy = PLATEAU_STRATEGY_DECOMPOSE;
        suggestion->rationale = strdup("High error rate in specific steps suggests focused practice needed.");
    } else if (state->feedback_frequency < 0.3f) {
        // Low feedback - increase it
        suggestion->primary_strategy = PLATEAU_STRATEGY_INCREASE_FEEDBACK;
        suggestion->secondary_strategy = PLATEAU_STRATEGY_FOCUS_WEAK;
        suggestion->rationale = strdup("Low feedback frequency may be limiting learning.");
    } else if (plateau_duration > 100) {
        // Long plateau - try strategy change
        suggestion->primary_strategy = PLATEAU_STRATEGY_CHANGE_APPROACH;
        suggestion->secondary_strategy = PLATEAU_STRATEGY_REST;
        suggestion->rationale = strdup("Extended plateau suggests current strategy is optimized; try a new approach.");
    } else if (plateau_duration > 50) {
        // Medium plateau - vary context
        suggestion->primary_strategy = PLATEAU_STRATEGY_VARY_CONTEXT;
        suggestion->secondary_strategy = PLATEAU_STRATEGY_FOCUS_WEAK;
        suggestion->rationale = strdup("Practice in varied contexts to promote transfer and flexibility.");
    } else {
        // Recent plateau - may just need rest
        suggestion->primary_strategy = PLATEAU_STRATEGY_REST;
        suggestion->secondary_strategy = PLATEAU_STRATEGY_VARY_CONTEXT;
        suggestion->rationale = strdup("Recent plateau may benefit from consolidation rest period.");
    }

    // Estimate breakthrough trials
    suggestion->estimated_breakthrough_trials = (float)plateau_duration * 0.5f + 20.0f;
    suggestion->confidence = 0.6f;

    // Clean up
    skill_acquisition_free_weak_point_analysis(&weak_analysis);

    return SKILL_SUCCESS;
}

NIMCP_EXPORT skill_error_t skill_acquisition_overcome_plateau(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    plateau_strategy_t strategy
) {
    if (!sa) {
        set_error("NULL skill acquisition");
        return SKILL_ERROR_NULL_POINTER;
    }

    skill_acquisition_state_t* state = find_state(sa, skill_id);
    if (!state) {
        set_error("Skill not found");
        return SKILL_ERROR_NOT_FOUND;
    }

    if (!state->current_plateau) {
        set_error("Not currently on plateau");
        return SKILL_ERROR_NOT_ON_PLATEAU;
    }

    end_plateau(state, strategy);
    sa->plateaus_overcome++;

    return SKILL_SUCCESS;
}

NIMCP_EXPORT skill_error_t skill_acquisition_get_plateau_history(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    learning_plateau_t* plateaus,
    size_t max_plateaus,
    size_t* count
) {
    if (!sa) {
        set_error("NULL skill acquisition");
        return SKILL_ERROR_NULL_POINTER;
    }

    skill_acquisition_state_t* state = find_state(sa, skill_id);
    if (!state) {
        set_error("Skill not found");
        return SKILL_ERROR_NOT_FOUND;
    }

    size_t to_copy = (state->num_plateaus < max_plateaus) ? state->num_plateaus : max_plateaus;

    if (plateaus && to_copy > 0) {
        memcpy(plateaus, state->plateaus, to_copy * sizeof(learning_plateau_t));
    }

    if (count) *count = to_copy;

    return SKILL_SUCCESS;
}

//=============================================================================
// Weak Point Analysis Functions
//=============================================================================

NIMCP_EXPORT skill_error_t skill_acquisition_analyze_weak_points(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    weak_point_analysis_t* analysis
) {
    if (!sa || !analysis) {
        set_error("NULL pointer argument");
        return SKILL_ERROR_NULL_POINTER;
    }

    skill_acquisition_state_t* state = find_state(sa, skill_id);
    if (!state) {
        set_error("Skill not found");
        return SKILL_ERROR_NOT_FOUND;
    }

    memset(analysis, 0, sizeof(*analysis));

    if (!state->skill || state->skill->num_steps == 0) {
        return SKILL_SUCCESS;  // No steps to analyze
    }

    // Count weak steps
    size_t num_weak = 0;
    for (size_t i = 0; i < state->skill->num_steps; i++) {
        if (state->step_difficulty[i] > sa->config.weak_point_threshold) {
            num_weak++;
        }
    }

    if (num_weak == 0) {
        return SKILL_SUCCESS;  // No weak points
    }

    // Allocate arrays
    analysis->weak_step_indices = (size_t*)nimcp_calloc(num_weak, sizeof(size_t));
    analysis->weak_step_errors = (float*)nimcp_calloc(num_weak, sizeof(float));

    if (!analysis->weak_step_indices || !analysis->weak_step_errors) {
        nimcp_free(analysis->weak_step_indices);
        nimcp_free(analysis->weak_step_errors);
        set_error("Memory allocation failed");
        return SKILL_ERROR_NO_MEMORY;
    }

    // Fill arrays
    size_t weak_idx = 0;
    float max_error = 0.0f;
    size_t most_critical = 0;
    float total_weakness = 0.0f;

    for (size_t i = 0; i < state->skill->num_steps; i++) {
        if (state->step_difficulty[i] > sa->config.weak_point_threshold) {
            analysis->weak_step_indices[weak_idx] = i;
            analysis->weak_step_errors[weak_idx] = state->step_difficulty[i];

            if (state->step_difficulty[i] > max_error) {
                max_error = state->step_difficulty[i];
                most_critical = weak_idx;
            }

            total_weakness += state->step_difficulty[i];
            weak_idx++;
        }
    }

    analysis->num_weak_steps = num_weak;
    analysis->most_critical = most_critical;
    analysis->overall_weakness = total_weakness / (float)state->skill->num_steps;

    return SKILL_SUCCESS;
}

NIMCP_EXPORT float skill_acquisition_get_step_difficulty(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    size_t step_index
) {
    skill_acquisition_state_t* state = find_state(sa, skill_id);
    if (!state || !state->skill || step_index >= state->skill->num_steps) {
        return NAN;
    }

    return state->step_difficulty[step_index];
}

NIMCP_EXPORT size_t skill_acquisition_get_most_difficult_step(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    float* difficulty
) {
    skill_acquisition_state_t* state = find_state(sa, skill_id);
    if (!state || !state->skill || state->skill->num_steps == 0) {
        return SIZE_MAX;
    }

    if (difficulty) {
        *difficulty = state->step_difficulty[state->most_difficult_step];
    }

    return state->most_difficult_step;
}

NIMCP_EXPORT void skill_acquisition_free_weak_point_analysis(weak_point_analysis_t* analysis) {
    if (!analysis) return;

    nimcp_free(analysis->weak_step_indices);
    nimcp_free(analysis->weak_step_errors);

    analysis->weak_step_indices = NULL;
    analysis->weak_step_errors = NULL;
    analysis->num_weak_steps = 0;
}

//=============================================================================
// Deliberate Practice Functions
//=============================================================================

NIMCP_EXPORT skill_error_t skill_acquisition_plan_deliberate_practice(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    deliberate_practice_plan_t* plan
) {
    if (!sa || !plan) {
        set_error("NULL pointer argument");
        return SKILL_ERROR_NULL_POINTER;
    }

    skill_acquisition_state_t* state = find_state(sa, skill_id);
    if (!state) {
        set_error("Skill not found");
        return SKILL_ERROR_NOT_FOUND;
    }

    memset(plan, 0, sizeof(*plan));

    // Analyze weak points
    weak_point_analysis_t weak_analysis = {0};
    skill_acquisition_analyze_weak_points(sa, skill_id, &weak_analysis);

    if (weak_analysis.num_weak_steps == 0) {
        // No weak points - distribute practice evenly
        if (state->skill && state->skill->num_steps > 0) {
            plan->num_focus_steps = state->skill->num_steps;
            plan->focus_steps = (size_t*)nimcp_calloc(plan->num_focus_steps, sizeof(size_t));
            if (!plan->focus_steps) return -1;
            plan->focus_durations = (float*)nimcp_calloc(plan->num_focus_steps, sizeof(float));

            if (plan->focus_steps && plan->focus_durations) {
                float equal_time = 1.0f / (float)plan->num_focus_steps;
                for (size_t i = 0; i < plan->num_focus_steps; i++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((i & 0xFF) == 0 && plan->num_focus_steps > 256) {
                        skill_acquisition_heartbeat("skill_acquis_loop",
                                         (float)(i + 1) / (float)plan->num_focus_steps);
                    }

                    plan->focus_steps[i] = i;
                    plan->focus_durations[i] = equal_time;
                }
            }
        }

        plan->recommended_feedback_freq = 0.5f;
        plan->target_quality = PRACTICE_INTENTIONAL;
        plan->strategy_description = strdup("No weak points identified. Practice all steps evenly with attention to form.");
    } else {
        // Focus on weak points
        plan->num_focus_steps = weak_analysis.num_weak_steps;
        plan->focus_steps = weak_analysis.weak_step_indices;
        weak_analysis.weak_step_indices = NULL;  // Transfer ownership

        plan->focus_durations = (float*)nimcp_calloc(plan->num_focus_steps, sizeof(float));
        if (plan->focus_durations) {
            // Allocate time proportional to difficulty
            float total_difficulty = 0.0f;
            for (size_t i = 0; i < weak_analysis.num_weak_steps; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && weak_analysis.num_weak_steps > 256) {
                    skill_acquisition_heartbeat("skill_acquis_loop",
                                     (float)(i + 1) / (float)weak_analysis.num_weak_steps);
                }

                total_difficulty += weak_analysis.weak_step_errors[i];
            }

            for (size_t i = 0; i < plan->num_focus_steps; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && plan->num_focus_steps > 256) {
                    skill_acquisition_heartbeat("skill_acquis_loop",
                                     (float)(i + 1) / (float)plan->num_focus_steps);
                }

                plan->focus_durations[i] = weak_analysis.weak_step_errors[i] / total_difficulty;
            }
        }

        // More feedback for higher weakness
        plan->recommended_feedback_freq = 0.3f + (weak_analysis.overall_weakness * 0.5f);
        if (plan->recommended_feedback_freq > 1.0f) {
            plan->recommended_feedback_freq = 1.0f;
        }

        plan->target_quality = PRACTICE_DELIBERATE;

        char description[NIMCP_ERROR_BUFFER_SIZE];
        snprintf(description, sizeof(description),
                 "Focus on %zu weak steps. Most critical: step %zu (%.1f%% error rate). "
                 "Request frequent feedback.",
                 weak_analysis.num_weak_steps,
                 weak_analysis.most_critical,
                 weak_analysis.weak_step_errors[weak_analysis.most_critical] * 100.0f);
        plan->strategy_description = strdup(description);

        nimcp_free(weak_analysis.weak_step_errors);
    }

    return SKILL_SUCCESS;
}

NIMCP_EXPORT skill_error_t skill_acquisition_set_practice_quality(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    practice_quality_t quality
) {
    if (!sa) {
        set_error("NULL skill acquisition");
        return SKILL_ERROR_NULL_POINTER;
    }

    skill_acquisition_state_t* state = find_state(sa, skill_id);
    if (!state) {
        set_error("Skill not found");
        return SKILL_ERROR_NOT_FOUND;
    }

    state->practice_quality = quality;

    // Adjust learning parameters based on practice quality
    switch (quality) {
        case PRACTICE_NAIVE:
            state->focus_intensity = 0.3f;
            break;
        case PRACTICE_INTENTIONAL:
            state->focus_intensity = 0.5f;
            break;
        case PRACTICE_DELIBERATE:
            state->focus_intensity = 0.8f;
            break;
        case PRACTICE_EXPERT:
            state->focus_intensity = 1.0f;
            break;
    }

    return SKILL_SUCCESS;
}

NIMCP_EXPORT skill_error_t skill_acquisition_set_feedback_frequency(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    float frequency
) {
    if (!sa) {
        set_error("NULL skill acquisition");
        return SKILL_ERROR_NULL_POINTER;
    }

    if (frequency < 0.0f || frequency > 1.0f) {
        set_error("Frequency must be in [0, 1]");
        return SKILL_ERROR_INVALID_PARAM;
    }

    skill_acquisition_state_t* state = find_state(sa, skill_id);
    if (!state) {
        set_error("Skill not found");
        return SKILL_ERROR_NOT_FOUND;
    }

    state->feedback_frequency = frequency;

    return SKILL_SUCCESS;
}

NIMCP_EXPORT void skill_acquisition_free_practice_plan(deliberate_practice_plan_t* plan) {
    if (!plan) return;

    nimcp_free(plan->focus_steps);
    nimcp_free(plan->focus_durations);
    nimcp_free(plan->strategy_description);

    plan->focus_steps = NULL;
    plan->focus_durations = NULL;
    plan->strategy_description = NULL;
    plan->num_focus_steps = 0;
}

//=============================================================================
// Power Law Fitting Functions
//=============================================================================

NIMCP_EXPORT skill_error_t skill_acquisition_fit_power_law(
    skill_acquisition_t* sa,
    uint64_t skill_id
) {
    if (!sa) {
        set_error("NULL skill acquisition");
        return SKILL_ERROR_NULL_POINTER;
    }

    skill_acquisition_state_t* state = find_state(sa, skill_id);
    if (!state) {
        set_error("Skill not found");
        return SKILL_ERROR_NOT_FOUND;
    }

    return fit_power_law_internal(state);
}

NIMCP_EXPORT skill_error_t skill_acquisition_get_power_law(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    power_law_state_t* state_out
) {
    if (!sa || !state_out) {
        set_error("NULL pointer argument");
        return SKILL_ERROR_NULL_POINTER;
    }

    skill_acquisition_state_t* state = find_state(sa, skill_id);
    if (!state) {
        set_error("Skill not found");
        return SKILL_ERROR_NOT_FOUND;
    }

    *state_out = state->power_law;

    return SKILL_SUCCESS;
}

NIMCP_EXPORT skill_error_t skill_acquisition_set_power_law(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    float initial_performance,
    float learning_rate,
    float asymptote
) {
    if (!sa) {
        set_error("NULL skill acquisition");
        return SKILL_ERROR_NULL_POINTER;
    }

    if (initial_performance <= 0 || learning_rate <= 0 || learning_rate > 1 || asymptote < 0) {
        set_error("Invalid power law parameters");
        return SKILL_ERROR_INVALID_PARAM;
    }

    skill_acquisition_state_t* state = find_state(sa, skill_id);
    if (!state) {
        set_error("Skill not found");
        return SKILL_ERROR_NOT_FOUND;
    }

    state->power_law.initial_performance = initial_performance;
    state->power_law.learning_rate = learning_rate;
    state->power_law.asymptote = asymptote;
    state->power_law.fit_valid = true;

    return SKILL_SUCCESS;
}

//=============================================================================
// Learning Curve Functions
//=============================================================================

NIMCP_EXPORT skill_error_t skill_acquisition_get_learning_curve(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    learning_curve_t* curve
) {
    if (!sa || !curve) {
        set_error("NULL pointer argument");
        return SKILL_ERROR_NULL_POINTER;
    }

    skill_acquisition_state_t* state = find_state(sa, skill_id);
    if (!state) {
        set_error("Skill not found");
        return SKILL_ERROR_NOT_FOUND;
    }

    memset(curve, 0, sizeof(*curve));

    if (state->history_count == 0) {
        return SKILL_SUCCESS;  // No data
    }

    // Allocate arrays
    curve->trial_numbers = (float*)nimcp_calloc(state->history_count, sizeof(float));
    curve->performances = (float*)nimcp_calloc(state->history_count, sizeof(float));
    if (!curve->performances) return -1;
    curve->predicted = (float*)nimcp_calloc(state->history_count, sizeof(float));

    if (!curve->trial_numbers || !curve->performances || !curve->predicted) {
        nimcp_free(curve->trial_numbers);
        nimcp_free(curve->performances);
        nimcp_free(curve->predicted);
        set_error("Memory allocation failed");
        return SKILL_ERROR_NO_MEMORY;
    }

    // Fill arrays
    for (size_t i = 0; i < state->history_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && state->history_count > 256) {
            skill_acquisition_heartbeat("skill_acquis_loop",
                             (float)(i + 1) / (float)state->history_count);
        }

        curve->trial_numbers[i] = (float)(i + 1);
        curve->performances[i] = state->performance_history[i];
        curve->predicted[i] = power_law_predict(&state->power_law, i + 1);
    }

    curve->num_points = state->history_count;
    curve->fit_r_squared = state->power_law.fit_r_squared;

    return SKILL_SUCCESS;
}

NIMCP_EXPORT void skill_acquisition_free_learning_curve(learning_curve_t* curve) {
    if (!curve) return;

    nimcp_free(curve->trial_numbers);
    nimcp_free(curve->performances);
    nimcp_free(curve->predicted);

    curve->trial_numbers = NULL;
    curve->performances = NULL;
    curve->predicted = NULL;
    curve->num_points = 0;
}

NIMCP_EXPORT skill_error_t skill_acquisition_get_performance_history(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    float* performances,
    size_t max_count,
    size_t* count
) {
    if (!sa) {
        set_error("NULL skill acquisition");
        return SKILL_ERROR_NULL_POINTER;
    }

    skill_acquisition_state_t* state = find_state(sa, skill_id);
    if (!state) {
        set_error("Skill not found");
        return SKILL_ERROR_NOT_FOUND;
    }

    size_t to_copy = (state->history_count < max_count) ? state->history_count : max_count;

    if (performances && to_copy > 0) {
        memcpy(performances, state->performance_history, to_copy * sizeof(float));
    }

    if (count) *count = to_copy;

    return SKILL_SUCCESS;
}

//=============================================================================
// Statistics Functions
//=============================================================================

NIMCP_EXPORT skill_error_t skill_acquisition_get_statistics(
    skill_acquisition_t* sa,
    size_t* total_skills,
    uint64_t* total_trials,
    uint64_t* plateaus_detected,
    float* avg_learning_rate
) {
    if (!sa) {
        set_error("NULL skill acquisition");
        return SKILL_ERROR_NULL_POINTER;
    }

    if (total_skills) *total_skills = sa->num_states;
    if (total_trials) *total_trials = sa->total_trials_recorded;
    if (plateaus_detected) *plateaus_detected = sa->plateaus_detected;

    if (avg_learning_rate && sa->num_states > 0) {
        float sum = 0.0f;
        for (size_t i = 0; i < sa->num_states; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && sa->num_states > 256) {
                skill_acquisition_heartbeat("skill_acquis_loop",
                                 (float)(i + 1) / (float)sa->num_states);
            }

            if (sa->states[i]) {
                sum += sa->states[i]->power_law.learning_rate;
            }
        }
        *avg_learning_rate = sum / (float)sa->num_states;
    } else if (avg_learning_rate) {
        *avg_learning_rate = 0.0f;
    }

    return SKILL_SUCCESS;
}

NIMCP_EXPORT float skill_acquisition_get_improvement_rate(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    size_t window
) {
    skill_acquisition_state_t* state = find_state(sa, skill_id);
    if (!state || state->history_count < 2) {
        return NAN;
    }

    if (window == 0) window = SKILL_DEFAULT_IMPROVEMENT_WINDOW;
    if (window > state->history_count) window = state->history_count;

    size_t start = state->history_count - window;
    float* recent = &state->performance_history[start];

    // Negative slope indicates improvement (lower is better)
    return -compute_slope(recent, window);
}

//=============================================================================
// Utility Functions
//=============================================================================

NIMCP_EXPORT const char* skill_transfer_type_name(transfer_type_t type) {
    switch (type) {
        case TRANSFER_POSITIVE: return "POSITIVE";
        case TRANSFER_NEGATIVE: return "NEGATIVE";
        case TRANSFER_NEUTRAL:  return "NEUTRAL";
        default:                return "UNKNOWN";
    }
}

NIMCP_EXPORT const char* skill_plateau_strategy_name(plateau_strategy_t strategy) {
    switch (strategy) {
        case PLATEAU_STRATEGY_NONE:              return "NONE";
        case PLATEAU_STRATEGY_VARY_CONTEXT:      return "VARY_CONTEXT";
        case PLATEAU_STRATEGY_FOCUS_WEAK:        return "FOCUS_WEAK";
        case PLATEAU_STRATEGY_CHANGE_APPROACH:   return "CHANGE_APPROACH";
        case PLATEAU_STRATEGY_REST:              return "REST";
        case PLATEAU_STRATEGY_INCREASE_FEEDBACK: return "INCREASE_FEEDBACK";
        case PLATEAU_STRATEGY_DECOMPOSE:         return "DECOMPOSE";
        default:                                 return "UNKNOWN";
    }
}

NIMCP_EXPORT const char* skill_practice_quality_name(practice_quality_t quality) {
    switch (quality) {
        case PRACTICE_NAIVE:       return "NAIVE";
        case PRACTICE_INTENTIONAL: return "INTENTIONAL";
        case PRACTICE_DELIBERATE:  return "DELIBERATE";
        case PRACTICE_EXPERT:      return "EXPERT";
        default:                   return "UNKNOWN";
    }
}

NIMCP_EXPORT const char* skill_error_string(skill_error_t error) {
    switch (error) {
        case SKILL_SUCCESS:                  return "Success";
        case SKILL_ERROR_NULL_POINTER:       return "NULL pointer";
        case SKILL_ERROR_INVALID_ID:         return "Invalid skill ID";
        case SKILL_ERROR_NOT_FOUND:          return "Skill not found";
        case SKILL_ERROR_NO_MEMORY:          return "Memory allocation failed";
        case SKILL_ERROR_INVALID_PARAM:      return "Invalid parameter";
        case SKILL_ERROR_ALREADY_EXISTS:     return "Skill already exists";
        case SKILL_ERROR_FULL:               return "No capacity for more skills";
        case SKILL_ERROR_INSUFFICIENT_DATA:  return "Insufficient data";
        case SKILL_ERROR_FIT_FAILED:         return "Power law fitting failed";
        case SKILL_ERROR_NOT_ON_PLATEAU:     return "Not currently on plateau";
        default:                             return "Unknown error";
    }
}

NIMCP_EXPORT void skill_acquisition_print_state(const skill_acquisition_state_t* state) {
    if (!state) {
        printf("Skill Acquisition State: NULL\n");
        return;
    }

    printf("=== Skill Acquisition State ===\n");
    if (state->skill) {
        printf("Skill: %s (ID: %lu)\n", state->skill->name,
               (unsigned long)state->skill->skill_id);
        printf("Steps: %zu\n", state->skill->num_steps);
    }

    printf("\nPower Law:\n");
    skill_acquisition_print_power_law(&state->power_law);

    printf("\nPerformance:\n");
    printf("  History entries: %zu / %zu\n", state->history_count, state->history_len);
    printf("  Best: %.2f, Worst: %.2f\n", state->best_performance, state->worst_performance);
    printf("  Recent improvement: %.4f\n", state->recent_improvement);

    printf("\nPractice:\n");
    printf("  Quality: %s\n", skill_practice_quality_name(state->practice_quality));
    printf("  Feedback frequency: %.2f\n", state->feedback_frequency);
    printf("  Total time: %lu ms\n", (unsigned long)state->total_practice_time_ms);

    printf("\nPlateaus:\n");
    printf("  Total recorded: %zu\n", state->num_plateaus);
    printf("  Currently on plateau: %s\n", state->current_plateau ? "YES" : "NO");

    printf("\nTransfers:\n");
    printf("  Recorded: %zu\n", state->num_transfers);
}

NIMCP_EXPORT void skill_acquisition_print_power_law(const power_law_state_t* state) {
    if (!state) {
        printf("Power Law State: NULL\n");
        return;
    }

    printf("  P(n) = %.2f * n^(-%.4f) + %.2f\n",
           state->initial_performance, state->learning_rate, state->asymptote);
    printf("  Practice count: %zu\n", state->practice_count);
    printf("  Fit R^2: %.4f (%s)\n", state->fit_r_squared,
           state->fit_valid ? "valid" : "invalid");

    // Print predictions for key trial numbers
    if (state->practice_count > 0) {
        printf("  Predictions:\n");
        size_t trials[] = {1, 10, 100, 1000};
        for (size_t i = 0; i < 4; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && 4 > 256) {
                skill_acquisition_heartbeat("skill_acquis_loop",
                                 (float)(i + 1) / (float)4);
            }

            if (trials[i] > state->practice_count * 10) break;
            printf("    Trial %zu: %.2f\n", trials[i], power_law_predict(state, trials[i]));
        }
    }
}

NIMCP_EXPORT uint64_t skill_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void skill_acquisition_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_skill_acquisition_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int skill_acquisition_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "skill_acquisition_training_begin: NULL argument");
        return -1;
    }
    skill_acquisition_heartbeat_instance(NULL, "skill_acquisition_training_begin", 0.0f);
    return 0;
}

int skill_acquisition_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "skill_acquisition_training_end: NULL argument");
        return -1;
    }
    skill_acquisition_heartbeat_instance(NULL, "skill_acquisition_training_end", 1.0f);
    return 0;
}

int skill_acquisition_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "skill_acquisition_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    skill_acquisition_heartbeat_instance(NULL, "skill_acquisition_training_step", progress);
    return 0;
}
