/**
 * @file nimcp_predictive_coding.c
 * @brief Implementation of Predictive Coding - Hierarchical Error Minimization
 *
 * ARCHITECTURAL OVERVIEW:
 * - Chain of Responsibility: Hierarchical error propagation
 * - Strategy Pattern: Different prediction/error functions
 * - Observer Pattern: Convergence notifications
 *
 * PERFORMANCE OPTIMIZATIONS:
 * - SIMD-friendly loops for vector operations
 * - Cache-coherent layer-by-layer processing
 * - Inline helpers for core computations
 * - Parallelizable across layers (future)
 *
 * COMPLEXITY ANALYSIS:
 * - pc_layer_compute_error: O(n) per layer
 * - pc_generate_prediction: O(n × m) weight matrix
 * - pc_hierarchy_inference_step: O(L × n × m) for full hierarchy
 *
 * BIOLOGICAL BASIS:
 * - Free Energy Principle (Friston 2010)
 * - Predictive Processing (Rao & Ballard 1999)
 * - Precision-weighted prediction errors
 */

#include "plasticity/predictive/nimcp_predictive_coding.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "security/nimcp_security.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>

#define LOG_MODULE "plasticity_predictive_coding"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(predictive_coding)

//=============================================================================
// Constants
//=============================================================================

#define EPSILON PC_EPSILON
#define DEFAULT_PRECISION PC_DEFAULT_PRECISION
#define VARIANCE_SMOOTHING 0.99f       /**< Exponential smoothing for variance */
#define CONVERGENCE_SAMPLES 10         /**< Samples for convergence detection */

//=============================================================================
// Internal Hierarchy Structure
//=============================================================================

/**
 * @brief Internal predictive coding hierarchy structure
 */
struct pc_hierarchy_struct {
    pc_hierarchy_config_t config;

    /* Layer states and parameters */
    uint32_t num_levels;
    pc_layer_state_t** layer_states;
    pc_layer_params_t* layer_params;

    /* Inter-layer prediction weights */
    pc_prediction_weights_t** prediction_weights;  /**< [num_levels-1] */

    /* Statistics */
    pc_hierarchy_stats_t stats;
    float* free_energy_history;        /**< For convergence detection */
    uint32_t history_index;

    /* Cached arrays for efficiency */
    float* temp_prediction;            /**< Temporary buffer for predictions */
    uint32_t max_layer_size;
};

static inline float safe_divide(float numerator, float denominator) {
    return numerator / (denominator + EPSILON);
}

static inline float decay_factor(float dt, float tau) {
    return 1.0F - expf(-dt / (tau + EPSILON));
}

/**
 * @brief ReLU activation for nonlinear predictions
 */
static inline float relu(float x) {
    return fmaxf(0.0F, x);
}

/**
 * @brief Sigmoid activation
 */
static inline float sigmoid(float x) {
    return 1.0F / (1.0F + expf(-nimcp_clampf(x, -20.0F, 20.0F)));
}

//=============================================================================
// Factory Functions
//=============================================================================

pc_layer_params_t pc_layer_params_default(uint32_t num_units) {
    /* WHAT: Default layer parameters
     * WHY:  Provides reasonable defaults for predictive coding
     */
    pc_layer_params_t params = {
        .num_units = num_units,
        .pred_type = PC_PREDICT_LINEAR,
        .error_type = PC_ERROR_PRECISION_WEIGHTED,
        .learning_rate_mu = 0.1F,           /* Learning rate for representations */
        .learning_rate_precision = 0.01F,   /* Slower precision learning */
        .learning_rate_weights = 0.001F,    /* Slowest weight learning */
        .prediction_tau = 10.0F,            /* ms */
        .error_tau = 5.0F,                  /* ms */
        .min_precision = 0.01F,
        .max_precision = 100.0F
    };
    return params;
}

pc_hierarchy_config_t pc_hierarchy_config_default(uint32_t num_levels,
                                                   const uint32_t* units_per_level) {
    /* WHAT: Default hierarchy configuration
     * WHY:  Standard configuration with reasonable defaults
     */
    if (num_levels == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pc_hierarchy_config_default: num_levels is 0");
        pc_hierarchy_config_t empty = {0};
        return empty;
    }
    pc_hierarchy_config_t config = {
        .num_levels = num_levels,
        .units_per_level = NULL,  /* Will be copied */
        .pred_type = PC_PREDICT_LINEAR,
        .error_type = PC_ERROR_PRECISION_WEIGHTED,
        .learning_rate = 0.1F,
        .precision_learning_rate = 0.01F,
        .learn_precisions = true,
        .use_lateral_connections = false,
        .dt = 1.0F
    };

    /* Note: units_per_level must be allocated by caller */
    (void)units_per_level;  /* Used in create, not stored here */

    return config;
}

pc_hierarchy_config_t pc_hierarchy_config_sensory(uint32_t input_dim,
                                                   uint32_t num_levels) {
    /* WHAT: Sensory processing configuration
     * WHY:  Decreasing size hierarchy for feature extraction
     */
    pc_hierarchy_config_t config = pc_hierarchy_config_default(num_levels, NULL);
    config.pred_type = PC_PREDICT_NONLINEAR;  /* Nonlinear for sensory */
    config.error_type = PC_ERROR_PRECISION_WEIGHTED;
    config.learning_rate = 0.05F;  /* Lower rate for stability */
    (void)input_dim;  /* Used when creating hierarchy */
    return config;
}

pc_hierarchy_config_t pc_hierarchy_config_motor(uint32_t output_dim,
                                                 uint32_t num_levels) {
    /* WHAT: Motor control configuration
     * WHY:  Increasing size hierarchy for action generation
     */
    pc_hierarchy_config_t config = pc_hierarchy_config_default(num_levels, NULL);
    config.pred_type = PC_PREDICT_LINEAR;  /* Linear for motor */
    config.error_type = PC_ERROR_STANDARD;  /* Standard errors for motor */
    config.learning_rate = 0.2F;  /* Faster adaptation for motor */
    (void)output_dim;
    return config;
}

//=============================================================================
// Layer State Functions
//=============================================================================

pc_layer_state_t* pc_layer_state_create(const pc_layer_params_t* params) {
    /* WHAT: Allocate and initialize layer state
     * WHY:  Factory method ensures valid initial state
     */

    if (!params || params->num_units == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pc_layer_state_create: params is NULL or num_units is 0");
        NIMCP_LOGGING_ERROR("Invalid params in pc_layer_state_create");
        return NULL;
    }

    pc_layer_state_t* state = nimcp_calloc(1, sizeof(pc_layer_state_t));
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pc_layer_state_create: failed to allocate state");
        return NULL;
    }

    state->num_units = params->num_units;

    /* Allocate arrays */
    state->mu = nimcp_calloc(params->num_units, sizeof(float));
    state->mu_prior = nimcp_calloc(params->num_units, sizeof(float));
    state->error = nimcp_calloc(params->num_units, sizeof(float));
    state->precision = nimcp_calloc(params->num_units, sizeof(float));
    state->precision_log = nimcp_calloc(params->num_units, sizeof(float));
    state->error_variance = nimcp_calloc(params->num_units, sizeof(float));

    if (!state->mu || !state->mu_prior || !state->error ||
        !state->precision || !state->precision_log || !state->error_variance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pc_layer_state_create: failed to allocate arrays");
        pc_layer_state_destroy(state);
        return NULL;
    }

    /* Initialize precisions to 1.0 (unit variance) */
    for (uint32_t i = 0; i < params->num_units; i++) {
        state->precision[i] = DEFAULT_PRECISION;
        state->precision_log[i] = 0.0F;  /* log(1) = 0 */
        state->error_variance[i] = 1.0F;
    }

    state->free_energy = 0.0F;

    return state;
}

void pc_layer_state_destroy(pc_layer_state_t* state) {
    if (!state) return;

    nimcp_free(state->mu);
    nimcp_free(state->mu_prior);
    nimcp_free(state->error);
    nimcp_free(state->precision);
    nimcp_free(state->precision_log);
    nimcp_free(state->error_variance);
    nimcp_free(state);
}

void pc_layer_compute_error(pc_layer_state_t* state,
                            const float* input,
                            const pc_layer_params_t* params) {
    /* WHAT: Compute prediction error
     * WHY:  Core computation of predictive coding
     *
     * FORMULA: ε = x - μ̂ (or precision-weighted)
     *
     * COMPLEXITY: O(n)
     */

    if (!state || !input || !params) return;

    for (uint32_t i = 0; i < state->num_units; i++) {
        /* Basic error: difference between input and prediction */
        float raw_error = input[i] - state->mu_prior[i];

        /* Apply error type */
        switch (params->error_type) {
            case PC_ERROR_STANDARD:
                state->error[i] = raw_error;
                break;

            case PC_ERROR_PRECISION_WEIGHTED:
                /* Weight error by precision */
                state->error[i] = state->precision[i] * raw_error;
                break;

            case PC_ERROR_LOG_PRECISION:
                /* Log precision weighting (for precision learning) */
                state->error[i] = state->precision_log[i] * raw_error * raw_error;
                break;

            case PC_ERROR_TEMPORAL:
                /* Temporal error (would need previous state) */
                state->error[i] = raw_error;
                break;
        }

        /* Update error variance (exponential moving average) */
        float error_sq = raw_error * raw_error;
        state->error_variance[i] = VARIANCE_SMOOTHING * state->error_variance[i] +
                                   (1.0F - VARIANCE_SMOOTHING) * error_sq;
    }
}

void pc_layer_update_representations(pc_layer_state_t* state,
                                     float dt,
                                     const pc_layer_params_t* params) {
    /* WHAT: Update representations from error
     * WHY:  Inference step - minimize prediction error
     *
     * FORMULA: dμ/dt = κ × ε
     *
     * COMPLEXITY: O(n)
     */

    if (!state || !params) return;
    if (dt <= 0.0F) return;

    float decay = decay_factor(dt, params->prediction_tau);

    for (uint32_t i = 0; i < state->num_units; i++) {
        /* Gradient descent on free energy */
        float delta = params->learning_rate_mu * state->error[i] * decay;
        state->mu[i] += delta;
    }
}

void pc_layer_update_precisions(pc_layer_state_t* state,
                                float dt,
                                const pc_layer_params_t* params) {
    /* WHAT: Update precisions from error statistics
     * WHY:  Learn precision (inverse variance) from data
     *
     * FORMULA: π = 1 / <ε²>
     *          d(log π)/dt = 1 - π × <ε²>
     *
     * COMPLEXITY: O(n)
     */

    if (!state || !params) return;
    if (dt <= 0.0F) return;

    float decay = decay_factor(dt, params->error_tau);

    for (uint32_t i = 0; i < state->num_units; i++) {
        /* Update log precision for numerical stability */
        float expected_precision_error = state->precision[i] * state->error_variance[i];
        float delta_log = params->learning_rate_precision * (1.0F - expected_precision_error) * decay;

        state->precision_log[i] += delta_log;

        /* Convert back to precision */
        state->precision[i] = expf(state->precision_log[i]);

        /* Clamp to valid range */
        state->precision[i] = nimcp_clampf(state->precision[i],
                                      params->min_precision,
                                      params->max_precision);
        state->precision_log[i] = logf(state->precision[i]);
    }
}

float pc_layer_compute_free_energy(const pc_layer_state_t* state) {
    /* WHAT: Compute layer free energy
     * WHY:  Free energy quantifies surprise/prediction quality
     *
     * FORMULA: F = Σ π × ε² - ln(π)
     *            = Σ π × ε² + ln(σ²)
     *
     * COMPLEXITY: O(n)
     */

    if (!state) return 0.0F;

    float free_energy = 0.0F;

    for (uint32_t i = 0; i < state->num_units; i++) {
        /* Precision-weighted squared error */
        float weighted_error = state->precision[i] * state->error[i] * state->error[i];

        /* Complexity term: -log(precision) = log(variance) */
        float complexity = -state->precision_log[i];

        free_energy += weighted_error + complexity;
    }

    return free_energy * 0.5F;  /* Factor of 0.5 from Gaussian */
}

//=============================================================================
// Prediction Weight Functions
//=============================================================================

pc_prediction_weights_t* pc_prediction_weights_create(uint32_t num_lower,
                                                       uint32_t num_higher) {
    /* WHAT: Allocate and initialize prediction weights
     * WHY:  Connect layers in hierarchy
     */

    if (num_lower == 0 || num_higher == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pc_prediction_weights_create: num_lower or num_higher is 0");
        return NULL;
    }

    pc_prediction_weights_t* weights = nimcp_calloc(1, sizeof(pc_prediction_weights_t));
    if (!weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pc_prediction_weights_create: failed to allocate weights");
        return NULL;
    }

    weights->num_lower = num_lower;
    weights->num_higher = num_higher;

    /* Allocate weight matrix */
    weights->weights = nimcp_calloc(num_lower * num_higher, sizeof(float));
    weights->bias = nimcp_calloc(num_lower, sizeof(float));

    if (!weights->weights || !weights->bias) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pc_prediction_weights_create: failed to allocate weight arrays");
        pc_prediction_weights_destroy(weights);
        return NULL;
    }

    /* Initialize with small random-like values (deterministic for reproducibility) */
    float scale = 1.0F / sqrtf((float)num_higher);
    for (uint32_t i = 0; i < num_lower * num_higher; i++) {
        /* Simple pseudo-random initialization using LCG */
        float pseudo = ((float)((i * NIMCP_LCG_MULTIPLIER + NIMCP_LCG_INCREMENT) % 1000) / 1000.0F) - 0.5F;
        weights->weights[i] = pseudo * scale;
    }

    return weights;
}

void pc_prediction_weights_destroy(pc_prediction_weights_t* weights) {
    if (!weights) return;
    nimcp_free(weights->weights);
    nimcp_free(weights->bias);
    nimcp_free(weights);
}

void pc_generate_prediction(const pc_prediction_weights_t* weights,
                            const float* higher_mu,
                            float* prediction,
                            pc_prediction_type_t pred_type) {
    /* WHAT: Generate top-down prediction
     * WHY:  Higher layers predict lower layer states
     *
     * FORMULA: μ̂_lower = f(W × μ_higher + b)
     *
     * COMPLEXITY: O(n_lower × n_higher)
     */

    if (!weights || !higher_mu || !prediction) return;

    /* Matrix multiplication: prediction = W × higher_mu + bias */
    for (uint32_t i = 0; i < weights->num_lower; i++) {
        float sum = weights->bias[i];

        for (uint32_t j = 0; j < weights->num_higher; j++) {
            sum += weights->weights[i * weights->num_higher + j] * higher_mu[j];
        }

        /* Apply activation function based on prediction type */
        switch (pred_type) {
            case PC_PREDICT_LINEAR:
                prediction[i] = sum;
                break;

            case PC_PREDICT_NONLINEAR:
                prediction[i] = relu(sum);
                break;

            case PC_PREDICT_GAUSSIAN:
                /* Add small noise for Gaussian prediction (deterministic approx) */
                prediction[i] = sum;
                break;

            case PC_PREDICT_SPARSE:
                /* Soft thresholding for sparsity */
                prediction[i] = (fabsf(sum) > 0.1F) ? sum : 0.0F;
                break;

            case PC_PREDICT_IDENTITY:
                prediction[i] = higher_mu[i % weights->num_higher];
                break;
        }
    }
}

void pc_update_prediction_weights(pc_prediction_weights_t* weights,
                                  const float* lower_error,
                                  const float* higher_mu,
                                  float learning_rate) {
    /* WHAT: Update prediction weights via gradient descent
     * WHY:  Improve predictions over time
     *
     * FORMULA: ΔW = -κ × ε_lower × μ_higher^T
     *
     * COMPLEXITY: O(n_lower × n_higher)
     */

    if (!weights || !lower_error || !higher_mu) return;
    if (learning_rate <= 0.0F) return;

    /* Update weights: W -= lr × error × mu^T */
    for (uint32_t i = 0; i < weights->num_lower; i++) {
        for (uint32_t j = 0; j < weights->num_higher; j++) {
            float delta = learning_rate * lower_error[i] * higher_mu[j];
            weights->weights[i * weights->num_higher + j] -= delta;
        }

        /* Update bias */
        weights->bias[i] -= learning_rate * lower_error[i];
    }
}

//=============================================================================
// Hierarchy Functions
//=============================================================================

pc_hierarchy_t pc_hierarchy_create(const pc_hierarchy_config_t* config) {
    /* WHAT: Create predictive coding hierarchy
     * WHY:  Factory method for complete hierarchy
     */

    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pc_hierarchy_create: config is NULL");
        NIMCP_LOGGING_ERROR("Null config in pc_hierarchy_create");
        return NULL;
    }
    if (config->num_levels == 0 || config->num_levels > PC_MAX_HIERARCHY_LEVELS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pc_hierarchy_create: invalid num_levels");
        NIMCP_LOGGING_ERROR("Invalid num_levels: %u", config->num_levels);
        return NULL;
    }
    if (!config->units_per_level) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pc_hierarchy_create: units_per_level is NULL");
        NIMCP_LOGGING_ERROR("Null units_per_level");
        return NULL;
    }

    /* Allocate hierarchy */
    pc_hierarchy_t hier = nimcp_calloc(1, sizeof(struct pc_hierarchy_struct));
    if (!hier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pc_hierarchy_create: failed to allocate hierarchy");
        return NULL;
    }

    /* Copy configuration */
    memcpy(&hier->config, config, sizeof(pc_hierarchy_config_t));
    hier->num_levels = config->num_levels;

    /* Find max layer size for temp buffers */
    hier->max_layer_size = 0;
    for (uint32_t l = 0; l < config->num_levels; l++) {
        if (config->units_per_level[l] > hier->max_layer_size) {
            hier->max_layer_size = config->units_per_level[l];
        }
    }

    /* Allocate layer arrays */
    hier->layer_states = nimcp_calloc(config->num_levels, sizeof(pc_layer_state_t*));
    hier->layer_params = nimcp_calloc(config->num_levels, sizeof(pc_layer_params_t));
    if (!hier->layer_states || !hier->layer_params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pc_hierarchy_create: failed to allocate layer arrays");
        pc_hierarchy_destroy(hier);
        return NULL;
    }

    /* Create layers */
    for (uint32_t l = 0; l < config->num_levels; l++) {
        hier->layer_params[l] = pc_layer_params_default(config->units_per_level[l]);
        hier->layer_params[l].pred_type = config->pred_type;
        hier->layer_params[l].error_type = config->error_type;
        hier->layer_params[l].learning_rate_mu = config->learning_rate;
        hier->layer_params[l].learning_rate_precision = config->precision_learning_rate;

        hier->layer_states[l] = pc_layer_state_create(&hier->layer_params[l]);
        if (!hier->layer_states[l]) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pc_hierarchy_create: failed to create layer state");
            pc_hierarchy_destroy(hier);
            return NULL;
        }
    }

    /* Allocate prediction weights (between adjacent levels) */
    if (config->num_levels > 1) {
        hier->prediction_weights = nimcp_calloc(config->num_levels - 1,
                                                sizeof(pc_prediction_weights_t*));
        if (!hier->prediction_weights) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pc_hierarchy_create: failed to allocate prediction weights array");
            pc_hierarchy_destroy(hier);
            return NULL;
        }

        /* Create weights: from level L+1 to level L */
        for (uint32_t l = 0; l < config->num_levels - 1; l++) {
            hier->prediction_weights[l] = pc_prediction_weights_create(
                config->units_per_level[l],      /* Lower */
                config->units_per_level[l + 1]   /* Higher */
            );
            if (!hier->prediction_weights[l]) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pc_hierarchy_create: failed to create prediction weights");
                pc_hierarchy_destroy(hier);
                return NULL;
            }
        }
    }

    /* Allocate statistics arrays */
    hier->stats.layer_free_energies = nimcp_calloc(config->num_levels, sizeof(float));
    hier->stats.layer_mean_errors = nimcp_calloc(config->num_levels, sizeof(float));
    hier->free_energy_history = nimcp_calloc(CONVERGENCE_SAMPLES, sizeof(float));

    if (!hier->stats.layer_free_energies || !hier->stats.layer_mean_errors ||
        !hier->free_energy_history) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pc_hierarchy_create: failed to allocate statistics arrays");
        pc_hierarchy_destroy(hier);
        return NULL;
    }

    /* Allocate temp buffer */
    hier->temp_prediction = nimcp_calloc(hier->max_layer_size, sizeof(float));
    if (!hier->temp_prediction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pc_hierarchy_create: failed to allocate temp buffer");
        pc_hierarchy_destroy(hier);
        return NULL;
    }

    hier->history_index = 0;

    NIMCP_LOGGING_INFO("Created PC hierarchy: levels=%u, learn_precision=%d",
                       config->num_levels, config->learn_precisions);

    return hier;
}

void pc_hierarchy_destroy(pc_hierarchy_t hierarchy) {
    if (!hierarchy) return;

    /* Free layers */
    if (hierarchy->layer_states) {
        for (uint32_t l = 0; l < hierarchy->num_levels; l++) {
            pc_layer_state_destroy(hierarchy->layer_states[l]);
        }
        nimcp_free(hierarchy->layer_states);
    }
    nimcp_free(hierarchy->layer_params);

    /* Free prediction weights */
    if (hierarchy->prediction_weights && hierarchy->num_levels > 1) {
        for (uint32_t l = 0; l < hierarchy->num_levels - 1; l++) {
            pc_prediction_weights_destroy(hierarchy->prediction_weights[l]);
        }
        nimcp_free(hierarchy->prediction_weights);
    }

    /* Free statistics */
    nimcp_free(hierarchy->stats.layer_free_energies);
    nimcp_free(hierarchy->stats.layer_mean_errors);
    nimcp_free(hierarchy->free_energy_history);

    /* Free temp buffer */
    nimcp_free(hierarchy->temp_prediction);

    nimcp_free(hierarchy);
}

void pc_hierarchy_set_input(pc_hierarchy_t hierarchy,
                            const float* input) {
    /* WHAT: Set sensory input at bottom level
     * WHY:  Drive inference from bottom up
     */

    if (!hierarchy || !input) return;
    if (hierarchy->num_levels == 0) return;

    pc_layer_state_t* bottom = hierarchy->layer_states[0];
    memcpy(bottom->mu, input, bottom->num_units * sizeof(float));
}

void pc_hierarchy_set_prior(pc_hierarchy_t hierarchy,
                            const float* prior) {
    /* WHAT: Set prior at top level
     * WHY:  Incorporate top-down expectations
     */

    if (!hierarchy || !prior) return;
    if (hierarchy->num_levels == 0) return;

    pc_layer_state_t* top = hierarchy->layer_states[hierarchy->num_levels - 1];
    memcpy(top->mu_prior, prior, top->num_units * sizeof(float));
}

void pc_hierarchy_inference_step(pc_hierarchy_t hierarchy,
                                 float dt,
                                 bool learn) {
    /* WHAT: Run one inference step
     * WHY:  Iterative inference via message passing
     *
     * ALGORITHM:
     * 1. Generate top-down predictions
     * 2. Compute prediction errors (bottom-up)
     * 3. Update representations
     * 4. Update precisions (if enabled)
     * 5. Update weights (if learning)
     */

    if (!hierarchy) return;

    hierarchy->stats.total_updates++;

    float total_free_energy = 0.0F;
    float total_error = 0.0F;
    float total_precision = 0.0F;
    uint32_t total_units = 0;

    /* Step 1: Generate top-down predictions */
    for (int32_t l = (int32_t)hierarchy->num_levels - 2; l >= 0; l--) {
        pc_layer_state_t* higher = hierarchy->layer_states[l + 1];
        pc_layer_state_t* lower = hierarchy->layer_states[l];
        pc_prediction_weights_t* weights = hierarchy->prediction_weights[l];

        pc_generate_prediction(weights, higher->mu, lower->mu_prior,
                               hierarchy->layer_params[l].pred_type);
    }

    /* Step 2 & 3: Compute errors and update representations */
    for (uint32_t l = 0; l < hierarchy->num_levels; l++) {
        pc_layer_state_t* state = hierarchy->layer_states[l];
        const pc_layer_params_t* params = &hierarchy->layer_params[l];

        /* For bottom level, use mu as input (set by set_input) */
        /* For higher levels, compute error against prediction from above */
        if (l > 0 || hierarchy->num_levels == 1) {
            pc_layer_compute_error(state, state->mu, params);
        }

        /* Update representations */
        pc_layer_update_representations(state, dt, params);

        /* Update precisions if enabled */
        if (hierarchy->config.learn_precisions) {
            pc_layer_update_precisions(state, dt, params);
        }

        /* Compute free energy for this layer */
        float layer_fe = pc_layer_compute_free_energy(state);
        hierarchy->stats.layer_free_energies[l] = layer_fe;
        total_free_energy += layer_fe;

        /* Compute mean error */
        float layer_error = 0.0F;
        float layer_precision = 0.0F;
        for (uint32_t i = 0; i < state->num_units; i++) {
            layer_error += fabsf(state->error[i]);
            layer_precision += state->precision[i];
        }
        hierarchy->stats.layer_mean_errors[l] = layer_error / state->num_units;
        total_error += layer_error;
        total_precision += layer_precision;
        total_units += state->num_units;
    }

    /* Step 5: Update weights if learning */
    if (learn && hierarchy->prediction_weights) {
        for (uint32_t l = 0; l < hierarchy->num_levels - 1; l++) {
            pc_layer_state_t* lower = hierarchy->layer_states[l];
            pc_layer_state_t* higher = hierarchy->layer_states[l + 1];
            pc_prediction_weights_t* weights = hierarchy->prediction_weights[l];

            pc_update_prediction_weights(weights, lower->error, higher->mu,
                                         hierarchy->layer_params[l].learning_rate_weights);
        }
    }

    /* Update statistics */
    hierarchy->stats.total_free_energy = total_free_energy;
    hierarchy->stats.mean_error = total_error / total_units;
    hierarchy->stats.mean_precision = total_precision / total_units;

    /* Update convergence tracking */
    float prev_fe = hierarchy->free_energy_history[hierarchy->history_index];
    hierarchy->free_energy_history[hierarchy->history_index] = total_free_energy;
    hierarchy->history_index = (hierarchy->history_index + 1) % CONVERGENCE_SAMPLES;

    /* Compute convergence rate */
    if (prev_fe > 0.0F) {
        hierarchy->stats.convergence_rate = (prev_fe - total_free_energy) / prev_fe;
    }

    /* Check convergence */
    hierarchy->stats.is_converged = (fabsf(hierarchy->stats.convergence_rate) < 0.001F);
}

uint32_t pc_hierarchy_inference_converge(pc_hierarchy_t hierarchy,
                                          uint32_t max_iterations,
                                          float tolerance,
                                          bool learn) {
    /* WHAT: Run inference to convergence
     * WHY:  Find best representation for input
     */

    if (!hierarchy) return 0;

    float prev_fe = 1e10F;

    for (uint32_t iter = 0; iter < max_iterations; iter++) {
        pc_hierarchy_inference_step(hierarchy, hierarchy->config.dt, learn);

        /* Check convergence */
        float current_fe = hierarchy->stats.total_free_energy;
        float change = fabsf(current_fe - prev_fe) / (fabsf(prev_fe) + EPSILON);

        if (change < tolerance) {
            return iter + 1;
        }

        prev_fe = current_fe;
    }

    return max_iterations;
}

bool pc_hierarchy_get_representations(pc_hierarchy_t hierarchy,
                                       uint32_t level,
                                       float* output) {
    /* WHAT: Read inferred representations
     * WHY:  Access internal states
     */

    if (!hierarchy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pc_hierarchy_get_representations: hierarchy is NULL");
        return false;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pc_hierarchy_get_representations: output is NULL");
        return false;
    }
    if (level >= hierarchy->num_levels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pc_hierarchy_get_representations: level out of range");
        return false;
    }

    pc_layer_state_t* state = hierarchy->layer_states[level];
    memcpy(output, state->mu, state->num_units * sizeof(float));
    return true;
}

bool pc_hierarchy_get_errors(pc_hierarchy_t hierarchy,
                              uint32_t level,
                              float* output) {
    /* WHAT: Read prediction errors
     * WHY:  Errors indicate surprise/novelty
     */

    if (!hierarchy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pc_hierarchy_get_errors: hierarchy is NULL");
        return false;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pc_hierarchy_get_errors: output is NULL");
        return false;
    }
    if (level >= hierarchy->num_levels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pc_hierarchy_get_errors: level out of range");
        return false;
    }

    pc_layer_state_t* state = hierarchy->layer_states[level];
    memcpy(output, state->error, state->num_units * sizeof(float));
    return true;
}

float pc_hierarchy_get_free_energy(pc_hierarchy_t hierarchy) {
    if (!hierarchy) return 0.0F;
    return hierarchy->stats.total_free_energy;
}

bool pc_hierarchy_get_stats(pc_hierarchy_t hierarchy,
                            pc_hierarchy_stats_t* stats) {
    if (!hierarchy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pc_hierarchy_get_stats: hierarchy is NULL");
        return false;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pc_hierarchy_get_stats: stats is NULL");
        return false;
    }
    memcpy(stats, &hierarchy->stats, sizeof(pc_hierarchy_stats_t));
    return true;
}

void pc_hierarchy_reset(pc_hierarchy_t hierarchy) {
    if (!hierarchy) return;

    /* Reset all layers */
    for (uint32_t l = 0; l < hierarchy->num_levels; l++) {
        pc_layer_state_t* state = hierarchy->layer_states[l];
        if (state) {
            memset(state->mu, 0, state->num_units * sizeof(float));
            memset(state->mu_prior, 0, state->num_units * sizeof(float));
            memset(state->error, 0, state->num_units * sizeof(float));

            for (uint32_t i = 0; i < state->num_units; i++) {
                state->precision[i] = DEFAULT_PRECISION;
                state->precision_log[i] = 0.0F;
                state->error_variance[i] = 1.0F;
            }
            state->free_energy = 0.0F;
        }
    }

    /* Reset statistics (keep total_updates) */
    uint64_t updates = hierarchy->stats.total_updates;
    memset(&hierarchy->stats, 0, sizeof(pc_hierarchy_stats_t) -
           2 * sizeof(float*));  /* Don't clear pointers */
    hierarchy->stats.total_updates = updates;

    /* Re-allocate per-layer stats arrays if needed */
    if (hierarchy->stats.layer_free_energies) {
        memset(hierarchy->stats.layer_free_energies, 0,
               hierarchy->num_levels * sizeof(float));
    }
    if (hierarchy->stats.layer_mean_errors) {
        memset(hierarchy->stats.layer_mean_errors, 0,
               hierarchy->num_levels * sizeof(float));
    }

    /* Reset convergence history */
    memset(hierarchy->free_energy_history, 0, CONVERGENCE_SAMPLES * sizeof(float));
    hierarchy->history_index = 0;
}

//=============================================================================
// Utility Functions
//=============================================================================

float pc_kl_divergence_gaussian(const float* mu_q,
                                const float* precision_q,
                                const float* mu_p,
                                const float* precision_p,
                                uint32_t dim) {
    /* WHAT: KL divergence between Gaussians
     * WHY:  Component of variational free energy
     *
     * FORMULA (diagonal covariance):
     * KL = 0.5 × Σ (log(σ_p²/σ_q²) + σ_q²/σ_p² + (μ_p - μ_q)²/σ_p² - 1)
     *    = 0.5 × Σ (log(π_q/π_p) + π_p/π_q + π_p × (μ_p - μ_q)² - 1)
     */

    if (!mu_q || !precision_q || !mu_p || !precision_p || dim == 0) {
        return 0.0F;
    }

    float kl = 0.0F;

    for (uint32_t i = 0; i < dim; i++) {
        float log_ratio = logf(precision_q[i] / (precision_p[i] + EPSILON));
        float precision_ratio = precision_p[i] / (precision_q[i] + EPSILON);
        float mean_diff = mu_p[i] - mu_q[i];
        float mean_term = precision_p[i] * mean_diff * mean_diff;

        kl += log_ratio + precision_ratio + mean_term - 1.0F;
    }

    return 0.5F * kl;
}

void pc_softmax_precision(const float* precisions,
                          float* output,
                          uint32_t dim,
                          float temperature) {
    /* WHAT: Softmax for precision normalization
     * WHY:  Attention-like precision weighting
     */

    if (!precisions || !output || dim == 0) return;

    temperature = fmaxf(temperature, EPSILON);

    /* Find max for numerical stability */
    float max_val = precisions[0];
    for (uint32_t i = 1; i < dim; i++) {
        if (precisions[i] > max_val) {
            max_val = precisions[i];
        }
    }

    /* Compute exp and sum */
    float sum = 0.0F;
    for (uint32_t i = 0; i < dim; i++) {
        output[i] = expf((precisions[i] - max_val) / temperature);
        sum += output[i];
    }

    /* Normalize */
    for (uint32_t i = 0; i < dim; i++) {
        output[i] /= (sum + EPSILON);
    }
}
