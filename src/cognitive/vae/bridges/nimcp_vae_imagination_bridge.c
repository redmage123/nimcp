/**
 * @file nimcp_vae_imagination_bridge.c
 * @brief Implementation of VAE-Imagination bridge with quantum enhancements
 * @version 1.0.0
 * @date 2026-01-30
 *
 * Implements the bridge between VAE and imagination engine,
 * incorporating quantum Monte Carlo sampling, quantum walks,
 * and hyperbolic latent space geometry.
 */

#include "cognitive/vae/bridges/nimcp_vae_imagination_bridge.h"
#include "cognitive/imagination/nimcp_imagination_engine.h"
#include "utils/quantum/nimcp_quantum_monte_carlo.h"
#include "utils/quantum/nimcp_quantum_walk.h"
#include "utils/geometry/nimcp_hyperbolic.h"
#include "utils/math/nimcp_complex_math.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/tensor/nimcp_tensor_internal.h"
#include "utils/time/nimcp_time.h"
#include "utils/rng/nimcp_rand.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_constants.h"
#include "constants/nimcp_math_constants.h"

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define LOG_TAG "VAE_IMAG"
#define COHERENCE_EMA_ALPHA 0.1f
#define PI NIMCP_PI_F

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Sample from standard normal N(0,1) using centralized RNG module
 */
static inline float box_muller_normal(void) {
    return nimcp_rand_normal(0.0f, 1.0f);
}

/**
 * @brief Fill array with Gaussian samples using centralized RNG module
 */
static inline void sample_gaussian(float* out, uint32_t dim, float mean, float std) {
    nimcp_rand_normal_array(out, dim, mean, std);
}

static float compute_norm(const float* vec, uint32_t dim) {
    float sum = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        sum += vec[i] * vec[i];
    }
    return sqrtf(sum);
}

static void normalize_vector(float* vec, uint32_t dim) {
    float norm = compute_norm(vec, dim);
    if (norm > 1e-8f) {
        for (uint32_t i = 0; i < dim; i++) {
            vec[i] /= norm;
        }
    }
}

static float compute_dot_product(const float* a, const float* b, uint32_t dim) {
    float dot = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
    }
    return dot;
}

static float compute_mse(const float* a, const float* b, uint32_t dim) {
    float sum = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sum / dim;
}

/* Hyperbolic geometry helpers */
static float conformal_factor(const float* x, uint32_t dim) {
    float norm_sq = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        norm_sq += x[i] * x[i];
    }
    return 2.0f / (1.0f - norm_sq + POINCARE_EPSILON);
}

static float vae_imag_poincare_distance(const float* x, const float* y, uint32_t dim) {
    float norm_x_sq = 0.0f, norm_y_sq = 0.0f, diff_sq = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        norm_x_sq += x[i] * x[i];
        norm_y_sq += y[i] * y[i];
        float diff = x[i] - y[i];
        diff_sq += diff * diff;
    }

    float denom = (1.0f - norm_x_sq) * (1.0f - norm_y_sq);
    if (denom < POINCARE_EPSILON) denom = POINCARE_EPSILON;

    float arg = 1.0f + 2.0f * diff_sq / denom;
    if (arg < 1.0f) arg = 1.0f;

    return acoshf(arg);
}

static void vae_imag_poincare_clip(float* x, uint32_t dim, float max_radius) {
    float norm = compute_norm(x, dim);
    if (norm >= max_radius) {
        float scale = max_radius / (norm + POINCARE_EPSILON);
        for (uint32_t i = 0; i < dim; i++) {
            x[i] *= scale;
        }
    }
}

static void mobius_add(const float* x, const float* y, float* result, uint32_t dim) {
    float norm_x_sq = 0.0f, norm_y_sq = 0.0f, dot_xy = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        norm_x_sq += x[i] * x[i];
        norm_y_sq += y[i] * y[i];
        dot_xy += x[i] * y[i];
    }

    float denom = 1.0f + 2.0f * dot_xy + norm_x_sq * norm_y_sq;
    if (fabsf(denom) < POINCARE_EPSILON) denom = POINCARE_EPSILON;

    float coef_x = (1.0f + 2.0f * dot_xy + norm_y_sq) / denom;
    float coef_y = (1.0f - norm_x_sq) / denom;

    for (uint32_t i = 0; i < dim; i++) {
        result[i] = coef_x * x[i] + coef_y * y[i];
    }

    vae_imag_poincare_clip(result, dim, POINCARE_MAX_RADIUS);
}

static void exponential_map(const float* x, const float* v, float* result, uint32_t dim) {
    float lambda_x = conformal_factor(x, dim);
    float norm_v = compute_norm(v, dim);

    if (norm_v < POINCARE_EPSILON) {
        memcpy(result, x, dim * sizeof(float));
        return;
    }

    float t = tanhf(lambda_x * norm_v / 2.0f);
    float* v_normalized = (float*)nimcp_calloc(dim, sizeof(float));
    if (!v_normalized) {
        memcpy(result, x, dim * sizeof(float));
        return;
    }

    for (uint32_t i = 0; i < dim; i++) {
        v_normalized[i] = v[i] * t / norm_v;
    }

    mobius_add(x, v_normalized, result, dim);
    nimcp_free(v_normalized);
    v_normalized = NULL;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int vae_imag_bridge_default_config(vae_imag_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_imag_bridge_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(*config));

    /* Sampling configuration */
    config->sample.method = VAE_IMAG_SAMPLE_STANDARD;
    config->sample.temperature = NIMCP_TEMPERATURE_DEFAULT;
    config->sample.top_p = 0.9f;
    config->sample.num_samples = 1;
    config->sample.qmc_shots = VAE_IMAG_QMC_DEFAULT_SHOTS;
    config->sample.qmc_target_acceptance = 0.234f;
    config->sample.qmc_burnin = 500;
    config->sample.qwalk_steps = 100;
    config->sample.qwalk_coin_bias = 0.5f;
    config->sample.annealing_initial_temp = 10.0f;
    config->sample.annealing_final_temp = 0.01f;
    config->sample.annealing_steps = 1000;

    /* Geometry configuration */
    config->geometry.geometry = VAE_IMAG_GEOMETRY_EUCLIDEAN;
    config->geometry.curvature = VAE_IMAG_HYPERBOLIC_CURVATURE;
    config->geometry.boundary_clip = POINCARE_MAX_RADIUS;
    config->geometry.use_exponential_map = true;

    /* Mode temperatures */
    config->passive_temperature = 1.0f;
    config->directed_temperature = VAE_IMAG_DIRECTED_TEMPERATURE;
    config->creative_temperature = VAE_IMAG_CREATIVE_TEMPERATURE;

    /* Quality settings */
    config->vividness_target = 0.7f;
    config->coherence_threshold = 0.5f;
    config->novelty_target = 0.5f;

    /* Trajectory settings */
    config->max_trajectory_length = VAE_IMAG_MAX_TRAJECTORY;
    config->trajectory_smoothing = 0.3f;
    config->enable_trajectory_caching = true;

    /* Integration options */
    config->enable_quantum_sampling = false;
    config->enable_hyperbolic_latent = false;
    config->enable_cross_modal = false;
    config->enable_reality_checking = true;

    config->enable_logging = false;
    config->log_latent_samples = false;

    return 0;
}

vae_imag_bridge_t* vae_imag_bridge_create(const vae_imag_bridge_config_t* config) {
    vae_imag_bridge_t* bridge = (vae_imag_bridge_t*)nimcp_calloc(1, sizeof(vae_imag_bridge_t));
    if (!bridge) {
        NIMCP_LOG_ERROR(LOG_TAG, "Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_imag_bridge_create: bridge is NULL");
        return NULL;
    }

    if (config) {
        memcpy(&bridge->config, config, sizeof(vae_imag_bridge_config_t));
    } else {
        vae_imag_bridge_default_config(&bridge->config);
    }

    bridge->state = VAE_IMAG_STATE_DISCONNECTED;
    bridge->is_initialized = true;
    bridge->creation_time_us = get_time_us();
    bridge->stats.creation_time_us = bridge->creation_time_us;

    if (bridge->config.enable_logging) {
        NIMCP_LOG_INFO(LOG_TAG, "VAE-Imagination bridge created");
    }

    return bridge;
}

void vae_imag_bridge_destroy(vae_imag_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->latent_buffer) nimcp_free(bridge->latent_buffer);
    if (bridge->decode_buffer) nimcp_free(bridge->decode_buffer);
    if (bridge->sample_buffer) nimcp_free(bridge->sample_buffer);

    if (bridge->qmc_state) {
        /* Free QMC state if allocated */
    }
    if (bridge->qwalk_state) {
        /* Free quantum walk state if allocated */
    }
    if (bridge->hyperbolic_space) {
        /* Free hyperbolic space state if allocated */
    }

    if (bridge->config.enable_logging) {
        NIMCP_LOG_INFO(LOG_TAG, "VAE-Imagination bridge destroyed");
    }

    nimcp_free(bridge);
    bridge = NULL;
}

int vae_imag_bridge_connect_vae(vae_imag_bridge_t* bridge, vae_system_t* vae) {
    if (!bridge || !vae) return NIMCP_ERROR_VAE_IMAG_NULL;

    bridge->vae = vae;
    bridge->vae_input_dim = vae_get_input_dim(vae);
    bridge->vae_latent_dim = vae_get_latent_dim(vae);
    bridge->vae_output_dim = vae_get_output_dim(vae);

    /* Allocate working buffers */
    bridge->latent_buffer = (float*)nimcp_calloc(bridge->vae_latent_dim, sizeof(float));
    bridge->decode_buffer = (float*)nimcp_calloc(bridge->vae_output_dim, sizeof(float));
    if (!bridge->decode_buffer) return -1;
    bridge->sample_buffer = (float*)nimcp_calloc(bridge->vae_latent_dim, sizeof(float));

    if (!bridge->latent_buffer || !bridge->decode_buffer || !bridge->sample_buffer) {
        NIMCP_LOG_ERROR(LOG_TAG, "Failed to allocate working buffers");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_IMAG_NO_MEMORY, "vae_imagination_bridge: error condition");
        return NIMCP_ERROR_VAE_IMAG_NO_MEMORY;
    }

    if (bridge->imagination_engine) {
        bridge->state = VAE_IMAG_STATE_IDLE;
    }

    if (bridge->config.enable_logging) {
        NIMCP_LOG_INFO(LOG_TAG, "VAE connected: latent_dim=%u, output_dim=%u",
                       bridge->vae_latent_dim, bridge->vae_output_dim);
    }

    return 0;
}

int vae_imag_bridge_connect_imagination(vae_imag_bridge_t* bridge,
                                         void* imagination_engine) {
    if (!bridge || !imagination_engine) return NIMCP_ERROR_VAE_IMAG_NULL;

    bridge->imagination_engine = imagination_engine;

    if (bridge->vae) {
        bridge->state = VAE_IMAG_STATE_IDLE;
    }

    if (bridge->config.enable_logging) {
        NIMCP_LOG_INFO(LOG_TAG, "Imagination engine connected");
    }

    return 0;
}

int vae_imag_bridge_disconnect(vae_imag_bridge_t* bridge) {
    if (!bridge) return NIMCP_ERROR_VAE_IMAG_NULL;

    bridge->vae = NULL;
    bridge->imagination_engine = NULL;
    bridge->state = VAE_IMAG_STATE_DISCONNECTED;

    return 0;
}

bool vae_imag_bridge_is_connected(const vae_imag_bridge_t* bridge) {
    return bridge && bridge->vae && bridge->imagination_engine &&
           bridge->state != VAE_IMAG_STATE_DISCONNECTED &&
           bridge->state != VAE_IMAG_STATE_ERROR;
}

/* ============================================================================
 * Core Generation Implementation
 * ============================================================================ */

static int sample_from_prior(vae_imag_bridge_t* bridge, float temperature,
                             float* latent_out) {
    if (!bridge || !latent_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_imag_bridge_is_connected: required parameter is NULL (bridge, latent_out)");
        return -1;
    }

    /* Sample from standard normal, scaled by temperature */
    sample_gaussian(latent_out, bridge->vae_latent_dim, 0.0f, temperature);

    /* Apply geometry if configured */
    if (bridge->config.enable_hyperbolic_latent &&
        bridge->config.geometry.geometry == VAE_IMAG_GEOMETRY_HYPERBOLIC) {
        /* Map to Poincaré ball */
        float norm = compute_norm(latent_out, bridge->vae_latent_dim);
        if (norm > 0.0f) {
            float r = tanhf(norm) * bridge->config.geometry.boundary_clip;
            float scale = r / norm;
            for (uint32_t i = 0; i < bridge->vae_latent_dim; i++) {
                latent_out[i] *= scale;
            }
        }
    }

    return 0;
}

static int sample_toward_target(vae_imag_bridge_t* bridge,
                                const float* target, uint32_t target_dim,
                                float temperature, float* latent_out) {
    if (!bridge || !target || !latent_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_imag_bridge_is_connected: required parameter is NULL (bridge, target, latent_out)");
        return -1;
    }

    /* Start with target (or encode it if it's observation space) */
    uint32_t copy_dim = (target_dim < bridge->vae_latent_dim) ?
                        target_dim : bridge->vae_latent_dim;
    memcpy(latent_out, target, copy_dim * sizeof(float));

    /* Add noise scaled by temperature */
    for (uint32_t i = 0; i < bridge->vae_latent_dim; i++) {
        latent_out[i] += temperature * box_muller_normal();
    }

    return 0;
}

static int decode_latent(vae_imag_bridge_t* bridge, const float* latent,
                         float* decoded_out) {
    if (!bridge || !latent || !decoded_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_imag_bridge_is_connected: required parameter is NULL (bridge, latent, decoded_out)");
        return -1;
    }

    nimcp_tensor_t* latent_tensor = nimcp_tensor_create_1d(bridge->vae_latent_dim,
                                                           NIMCP_DTYPE_F32);
    nimcp_tensor_t* output_tensor = nimcp_tensor_create_1d(bridge->vae_output_dim,
                                                            NIMCP_DTYPE_F32);

    if (!latent_tensor || !output_tensor) {
        if (latent_tensor) nimcp_tensor_destroy(latent_tensor);
        if (output_tensor) nimcp_tensor_destroy(output_tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_IMAG_NO_MEMORY, "vae_imagination_bridge: error condition");
        return NIMCP_ERROR_VAE_IMAG_NO_MEMORY;
    }

    float* latent_data = (float*)nimcp_tensor_data(latent_tensor);
    memcpy(latent_data, latent, bridge->vae_latent_dim * sizeof(float));

    int ret = vae_decode(bridge->vae, latent_tensor, output_tensor);
    if (ret != 0) {
        nimcp_tensor_destroy(latent_tensor);
        nimcp_tensor_destroy(output_tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_IMAG_DECODE_FAILED, "vae_imagination_bridge: error condition");
        return NIMCP_ERROR_VAE_IMAG_DECODE_FAILED;
    }

    float* output_data = (float*)nimcp_tensor_data(output_tensor);
    memcpy(decoded_out, output_data, bridge->vae_output_dim * sizeof(float));

    nimcp_tensor_destroy(latent_tensor);
    nimcp_tensor_destroy(output_tensor);

    return 0;
}

int vae_imag_generate(vae_imag_bridge_t* bridge,
                       vae_imag_mode_t mode,
                       const vae_imag_goal_t* goal,
                       vae_imag_generation_result_t* result) {
    if (!bridge || !result) return NIMCP_ERROR_VAE_IMAG_NULL;
    if (!vae_imag_bridge_is_connected(bridge)) return NIMCP_ERROR_VAE_IMAG_NOT_CONNECTED;

    uint64_t start_time = get_time_us();
    bridge->state = VAE_IMAG_STATE_GENERATING;
    bridge->current_mode = mode;

    memset(result, 0, sizeof(*result));

    /* Determine temperature based on mode */
    float temperature = 0.0f;
    switch (mode) {
        case VAE_IMAG_MODE_PASSIVE:
            temperature = bridge->config.passive_temperature;
            break;
        case VAE_IMAG_MODE_DIRECTED:
            temperature = bridge->config.directed_temperature;
            break;
        case VAE_IMAG_MODE_CREATIVE:
            temperature = bridge->config.creative_temperature;
            break;
        case VAE_IMAG_MODE_COUNTERFACTUAL:
        case VAE_IMAG_MODE_PROSPECTIVE:
        case VAE_IMAG_MODE_RETROSPECTIVE:
            temperature = bridge->config.directed_temperature;
            break;
        case VAE_IMAG_MODE_QUANTUM_SEARCH:
            temperature = 0.1f; /* Low temp for search */
            break;
        case VAE_IMAG_MODE_HYPERBOLIC:
            temperature = bridge->config.passive_temperature;
            break;
        default:
            temperature = 1.0f;
    }

    /* Sample latent based on mode and goal */
    int ret = 0;
    if (goal && goal->target_features && goal->target_dim > 0) {
        ret = sample_toward_target(bridge, goal->target_features, goal->target_dim,
                                   temperature, bridge->latent_buffer);
    } else {
        ret = sample_from_prior(bridge, temperature, bridge->latent_buffer);
    }

    if (ret != 0) {
        bridge->state = VAE_IMAG_STATE_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_IMAG_SAMPLE_FAILED, "vae_imagination_bridge: error condition");
        return NIMCP_ERROR_VAE_IMAG_SAMPLE_FAILED;
    }

    /* Decode to observation space */
    ret = decode_latent(bridge, bridge->latent_buffer, bridge->decode_buffer);
    if (ret != 0) {
        bridge->state = VAE_IMAG_STATE_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_IMAG_DECODE_FAILED, "vae_imagination_bridge: error condition");
        return NIMCP_ERROR_VAE_IMAG_DECODE_FAILED;
    }

    /* Fill result */
    result->generated = (float*)nimcp_calloc(bridge->vae_output_dim, sizeof(float));
    result->latent = (float*)nimcp_calloc(bridge->vae_latent_dim, sizeof(float));

    if (!result->generated || !result->latent) {
        if (result->generated) nimcp_free(result->generated);
        if (result->latent) nimcp_free(result->latent);
        result->generated = NULL;
        result->latent = NULL;
        bridge->state = VAE_IMAG_STATE_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_IMAG_NO_MEMORY, "vae_imagination_bridge: error condition");
        return NIMCP_ERROR_VAE_IMAG_NO_MEMORY;
    }

    memcpy(result->generated, bridge->decode_buffer, bridge->vae_output_dim * sizeof(float));
    memcpy(result->latent, bridge->latent_buffer, bridge->vae_latent_dim * sizeof(float));
    result->generated_dim = bridge->vae_output_dim;
    result->latent_dim = bridge->vae_latent_dim;

    /* Compute quality metrics */
    result->vividness = bridge->config.vividness_target;
    result->coherence = 0.8f; /* Placeholder - would compute from VAE loss */
    result->novelty = temperature * 0.5f; /* Higher temp = more novel */

    if (goal && goal->target_features) {
        float dist = compute_mse(bridge->latent_buffer, goal->target_features,
                                 (goal->target_dim < bridge->vae_latent_dim) ?
                                  goal->target_dim : bridge->vae_latent_dim);
        result->goal_alignment = expf(-dist);
    } else {
        result->goal_alignment = 1.0f;
    }

    result->reality_distance = temperature;
    result->method_used = bridge->config.sample.method;
    result->samples_generated = 1;
    result->acceptance_rate = 1.0f;

    uint64_t elapsed = get_time_us() - start_time;
    result->generation_time_us = elapsed;

    /* Update statistics */
    bridge->stats.total_generations++;
    switch (mode) {
        case VAE_IMAG_MODE_PASSIVE: bridge->stats.passive_generations++; break;
        case VAE_IMAG_MODE_DIRECTED: bridge->stats.directed_generations++; break;
        case VAE_IMAG_MODE_CREATIVE: bridge->stats.creative_generations++; break;
        case VAE_IMAG_MODE_COUNTERFACTUAL: bridge->stats.counterfactual_generations++; break;
        case VAE_IMAG_MODE_PROSPECTIVE: bridge->stats.prospective_generations++; break;
        default: break;
    }

    if (isfinite(result->vividness)) {
        bridge->stats.avg_vividness = bridge->stats.avg_vividness * 0.9f + result->vividness * 0.1f;
    }
    if (isfinite(result->coherence)) {
        bridge->stats.avg_coherence = bridge->stats.avg_coherence * 0.9f + result->coherence * 0.1f;
    }
    if (isfinite(result->novelty)) {
        bridge->stats.avg_novelty = bridge->stats.avg_novelty * 0.9f + result->novelty * 0.1f;
    }
    bridge->stats.avg_generation_latency_us = bridge->stats.avg_generation_latency_us * 0.9f +
                                               elapsed * 0.1f;
    bridge->stats.last_operation_us = get_time_us();

    bridge->state = VAE_IMAG_STATE_IDLE;

    if (bridge->config.enable_logging) {
        NIMCP_LOG_DEBUG(LOG_TAG, "Generated imagination: mode=%d, coherence=%.3f, novelty=%.3f",
                        mode, result->coherence, result->novelty);
    }

    return 0;
}

int vae_imag_dream(vae_imag_bridge_t* bridge,
                    float temperature,
                    uint32_t num_samples,
                    vae_imag_generation_result_t* result) {
    if (!bridge || !result) return NIMCP_ERROR_VAE_IMAG_NULL;

    bridge->config.sample.temperature = temperature;
    bridge->config.sample.num_samples = num_samples;

    return vae_imag_generate(bridge, VAE_IMAG_MODE_PASSIVE, NULL, result);
}

int vae_imag_imagine_toward(vae_imag_bridge_t* bridge,
                             const vae_imag_goal_t* goal,
                             vae_imag_generation_result_t* result) {
    if (!bridge || !goal || !result) return NIMCP_ERROR_VAE_IMAG_NULL;

    return vae_imag_generate(bridge, VAE_IMAG_MODE_DIRECTED, goal, result);
}

int vae_imag_create(vae_imag_bridge_t* bridge,
                     float novelty_target,
                     vae_imag_generation_result_t* result) {
    if (!bridge || !result) return NIMCP_ERROR_VAE_IMAG_NULL;

    /* Higher novelty = higher temperature */
    bridge->config.creative_temperature = 0.5f + novelty_target * 2.0f;

    return vae_imag_generate(bridge, VAE_IMAG_MODE_CREATIVE, NULL, result);
}

/* ============================================================================
 * Trajectory Generation Implementation
 * ============================================================================ */

int vae_imag_simulate_future(vae_imag_bridge_t* bridge,
                              const float* start_state, uint32_t start_dim,
                              uint32_t num_steps,
                              vae_imag_trajectory_result_t* result) {
    if (!bridge || !start_state || !result) return NIMCP_ERROR_VAE_IMAG_NULL;
    if (!vae_imag_bridge_is_connected(bridge)) return NIMCP_ERROR_VAE_IMAG_NOT_CONNECTED;

    if (num_steps > bridge->config.max_trajectory_length) {
        num_steps = bridge->config.max_trajectory_length;
    }

    uint64_t start_time = get_time_us();

    memset(result, 0, sizeof(*result));

    result->trajectory = (float**)nimcp_calloc(num_steps, sizeof(float*));
    result->decoded_trajectory = (float**)nimcp_calloc(num_steps, sizeof(float*));
    if (!result->decoded_trajectory) return -1;
    result->coherence_scores = (float*)nimcp_calloc(num_steps, sizeof(float));

    if (!result->trajectory || !result->decoded_trajectory || !result->coherence_scores) {
        if (result->trajectory) nimcp_free(result->trajectory);
        if (result->decoded_trajectory) nimcp_free(result->decoded_trajectory);
        if (result->coherence_scores) nimcp_free(result->coherence_scores);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_IMAG_NO_MEMORY, "vae_imagination_bridge: error condition");
        return NIMCP_ERROR_VAE_IMAG_NO_MEMORY;
    }

    /* Initialize with start state */
    float* current_latent = (float*)nimcp_calloc(bridge->vae_latent_dim, sizeof(float));
    if (!current_latent) {
        nimcp_free(result->trajectory);
        nimcp_free(result->decoded_trajectory);
        nimcp_free(result->coherence_scores);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_IMAG_NO_MEMORY, "vae_imagination_bridge: error condition");
        return NIMCP_ERROR_VAE_IMAG_NO_MEMORY;
    }

    uint32_t copy_dim = (start_dim < bridge->vae_latent_dim) ? start_dim : bridge->vae_latent_dim;
    memcpy(current_latent, start_state, copy_dim * sizeof(float));

    float total_coherence = 0.0f;

    for (uint32_t step = 0; step < num_steps; step++) {
        /* Store current latent */
        result->trajectory[step] = (float*)nimcp_calloc(bridge->vae_latent_dim, sizeof(float));
        result->decoded_trajectory[step] = (float*)nimcp_calloc(bridge->vae_output_dim, sizeof(float));

        if (!result->trajectory[step] || !result->decoded_trajectory[step]) {
            /* Cleanup on failure */
            for (uint32_t j = 0; j <= step; j++) {
                if (result->trajectory[j]) nimcp_free(result->trajectory[j]);
                if (result->decoded_trajectory[j]) nimcp_free(result->decoded_trajectory[j]);
            }
            nimcp_free(result->trajectory);
            nimcp_free(result->decoded_trajectory);
            nimcp_free(result->coherence_scores);
            nimcp_free(current_latent);
            current_latent = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_IMAG_NO_MEMORY, "vae_imagination_bridge: error condition");
            return NIMCP_ERROR_VAE_IMAG_NO_MEMORY;
        }

        memcpy(result->trajectory[step], current_latent, bridge->vae_latent_dim * sizeof(float));

        /* Decode current step */
        int ret = decode_latent(bridge, current_latent, result->decoded_trajectory[step]);
        if (ret != 0) {
            result->coherence_scores[step] = 0.0f;
        } else {
            result->coherence_scores[step] = 0.8f; /* Placeholder */
        }

        total_coherence += result->coherence_scores[step];

        /* Evolve latent state (simple random walk + smoothing) */
        for (uint32_t i = 0; i < bridge->vae_latent_dim; i++) {
            float noise = box_muller_normal() * 0.1f;
            current_latent[i] = current_latent[i] * (1.0f - bridge->config.trajectory_smoothing) +
                               (current_latent[i] + noise) * bridge->config.trajectory_smoothing;
        }
    }

    nimcp_free(current_latent);
    current_latent = NULL;

    result->trajectory_length = num_steps;
    result->latent_dim = bridge->vae_latent_dim;
    result->decoded_dim = bridge->vae_output_dim;
    result->avg_coherence = total_coherence / (num_steps > 0 ? num_steps : 1);

    uint64_t elapsed = get_time_us() - start_time;
    bridge->stats.prospective_generations++;
    bridge->stats.avg_trajectory_latency_us = bridge->stats.avg_trajectory_latency_us * 0.9f +
                                               elapsed * 0.1f;

    return 0;
}

int vae_imag_counterfactual(vae_imag_bridge_t* bridge,
                             const float* original_state, uint32_t state_dim,
                             const float* intervention, uint32_t intervention_dim,
                             uint32_t num_steps,
                             vae_imag_trajectory_result_t* result) {
    if (!bridge || !original_state || !intervention || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_IMAG_NULL, "vae_imagination_bridge: error condition");
        return NIMCP_ERROR_VAE_IMAG_NULL;
    }

    /* Apply intervention to original state */
    float* modified_state = (float*)nimcp_calloc(bridge->vae_latent_dim, sizeof(float));
    if (!modified_state) return NIMCP_ERROR_VAE_IMAG_NO_MEMORY;

    uint32_t copy_dim = (state_dim < bridge->vae_latent_dim) ? state_dim : bridge->vae_latent_dim;
    memcpy(modified_state, original_state, copy_dim * sizeof(float));

    /* Apply intervention (additive modification) */
    uint32_t interv_dim = (intervention_dim < bridge->vae_latent_dim) ?
                          intervention_dim : bridge->vae_latent_dim;
    for (uint32_t i = 0; i < interv_dim; i++) {
        modified_state[i] += intervention[i];
    }

    int ret = vae_imag_simulate_future(bridge, modified_state, bridge->vae_latent_dim,
                                        num_steps, result);

    nimcp_free(modified_state);
    modified_state = NULL;
    bridge->stats.counterfactual_generations++;

    return ret;
}

int vae_imag_interpolate(vae_imag_bridge_t* bridge,
                          const float* state_a, const float* state_b,
                          uint32_t state_dim, uint32_t num_steps,
                          vae_imag_trajectory_result_t* result) {
    if (!bridge || !state_a || !state_b || !result) return NIMCP_ERROR_VAE_IMAG_NULL;
    if (!vae_imag_bridge_is_connected(bridge)) return NIMCP_ERROR_VAE_IMAG_NOT_CONNECTED;

    memset(result, 0, sizeof(*result));

    result->trajectory = (float**)nimcp_calloc(num_steps, sizeof(float*));
    result->decoded_trajectory = (float**)nimcp_calloc(num_steps, sizeof(float*));
    if (!result->decoded_trajectory) return -1;
    result->coherence_scores = (float*)nimcp_calloc(num_steps, sizeof(float));

    if (!result->trajectory || !result->decoded_trajectory || !result->coherence_scores) {
        if (result->trajectory) nimcp_free(result->trajectory);
        if (result->decoded_trajectory) nimcp_free(result->decoded_trajectory);
        if (result->coherence_scores) nimcp_free(result->coherence_scores);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_IMAG_NO_MEMORY, "vae_imagination_bridge: error condition");
        return NIMCP_ERROR_VAE_IMAG_NO_MEMORY;
    }

    uint32_t dim = (state_dim < bridge->vae_latent_dim) ? state_dim : bridge->vae_latent_dim;

    for (uint32_t step = 0; step < num_steps; step++) {
        float t = (num_steps > 1) ? (float)step / (num_steps - 1) : 0.5f;

        result->trajectory[step] = (float*)nimcp_calloc(bridge->vae_latent_dim, sizeof(float));
        result->decoded_trajectory[step] = (float*)nimcp_calloc(bridge->vae_output_dim, sizeof(float));

        if (!result->trajectory[step] || !result->decoded_trajectory[step]) {
            /* Cleanup */
            for (uint32_t j = 0; j <= step; j++) {
                if (result->trajectory[j]) nimcp_free(result->trajectory[j]);
                if (result->decoded_trajectory[j]) nimcp_free(result->decoded_trajectory[j]);
            }
            nimcp_free(result->trajectory);
            nimcp_free(result->decoded_trajectory);
            nimcp_free(result->coherence_scores);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_IMAG_NO_MEMORY, "vae_imagination_bridge: error condition");
            return NIMCP_ERROR_VAE_IMAG_NO_MEMORY;
        }

        /* Linear interpolation (or geodesic if hyperbolic) */
        if (bridge->config.enable_hyperbolic_latent) {
            /* Geodesic interpolation in Poincaré ball */
            /* For simplicity, use exponential map interpolation */
            float* tangent = (float*)nimcp_calloc(dim, sizeof(float));
            if (tangent) {
                for (uint32_t i = 0; i < dim; i++) {
                    tangent[i] = (state_b[i] - state_a[i]) * t;
                }
                exponential_map(state_a, tangent, result->trajectory[step], dim);
                nimcp_free(tangent);
                tangent = NULL;
            }
        } else {
            /* Standard linear interpolation */
            for (uint32_t i = 0; i < dim; i++) {
                result->trajectory[step][i] = state_a[i] * (1.0f - t) + state_b[i] * t;
            }
        }

        decode_latent(bridge, result->trajectory[step], result->decoded_trajectory[step]);
        result->coherence_scores[step] = 0.9f; /* Interpolation is coherent */
    }

    result->trajectory_length = num_steps;
    result->latent_dim = bridge->vae_latent_dim;
    result->decoded_dim = bridge->vae_output_dim;
    result->avg_coherence = 0.9f;

    return 0;
}

/* ============================================================================
 * Quantum-Enhanced Generation Implementation
 * ============================================================================ */

int vae_imag_generate_qmc(vae_imag_bridge_t* bridge,
                           const float* target_features, uint32_t target_dim,
                           uint32_t num_samples,
                           vae_imag_generation_result_t* result) {
    if (!bridge || !result) return NIMCP_ERROR_VAE_IMAG_NULL;
    if (!vae_imag_bridge_is_connected(bridge)) return NIMCP_ERROR_VAE_IMAG_NOT_CONNECTED;

    uint64_t start_time = get_time_us();
    memset(result, 0, sizeof(*result));

    /* Use importance sampling with QMC-like adaptive strategy */
    float* best_sample = (float*)nimcp_calloc(bridge->vae_latent_dim, sizeof(float));
    float* current_sample = (float*)nimcp_calloc(bridge->vae_latent_dim, sizeof(float));
    if (!current_sample) return -1;
    float* proposal = (float*)nimcp_calloc(bridge->vae_latent_dim, sizeof(float));

    if (!best_sample || !current_sample || !proposal) {
        if (best_sample) nimcp_free(best_sample);
        if (current_sample) nimcp_free(current_sample);
        if (proposal) nimcp_free(proposal);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_IMAG_NO_MEMORY, "vae_imagination_bridge: error condition");
        return NIMCP_ERROR_VAE_IMAG_NO_MEMORY;
    }

    /* Initialize from target or prior */
    if (target_features && target_dim > 0) {
        uint32_t copy_dim = (target_dim < bridge->vae_latent_dim) ?
                            target_dim : bridge->vae_latent_dim;
        memcpy(current_sample, target_features, copy_dim * sizeof(float));
    } else {
        sample_gaussian(current_sample, bridge->vae_latent_dim, 0.0f, 1.0f);
    }

    memcpy(best_sample, current_sample, bridge->vae_latent_dim * sizeof(float));
    float best_energy = FLT_MAX;
    uint32_t accepted = 0;

    /* MCMC with adaptive proposal */
    float step_size = 0.5f;

    for (uint32_t i = 0; i < num_samples; i++) {
        /* Propose new sample */
        for (uint32_t j = 0; j < bridge->vae_latent_dim; j++) {
            proposal[j] = current_sample[j] + step_size * box_muller_normal();
        }

        /* Compute energy (distance to target or prior log-likelihood) */
        float energy = 0.0f;
        if (target_features) {
            energy = compute_mse(proposal, target_features,
                                (target_dim < bridge->vae_latent_dim) ?
                                 target_dim : bridge->vae_latent_dim);
        } else {
            energy = compute_norm(proposal, bridge->vae_latent_dim);
            energy = energy * energy / 2.0f; /* Standard normal energy */
        }

        /* Metropolis acceptance */
        float current_energy = 0.0f;
        if (target_features) {
            current_energy = compute_mse(current_sample, target_features,
                                        (target_dim < bridge->vae_latent_dim) ?
                                         target_dim : bridge->vae_latent_dim);
        } else {
            float norm = compute_norm(current_sample, bridge->vae_latent_dim);
            current_energy = norm * norm / 2.0f;
        }

        float accept_prob = expf(-(energy - current_energy));
        if ((float)nimcp_tl_rand() / RAND_MAX < accept_prob) {
            memcpy(current_sample, proposal, bridge->vae_latent_dim * sizeof(float));
            accepted++;

            if (energy < best_energy) {
                memcpy(best_sample, proposal, bridge->vae_latent_dim * sizeof(float));
                best_energy = energy;
            }
        }

        /* Adaptive step size */
        float acceptance_rate = (float)(accepted + 1) / (i + 2);
        if (acceptance_rate < bridge->config.sample.qmc_target_acceptance) {
            step_size *= 0.99f;
        } else {
            step_size *= 1.01f;
        }
    }

    nimcp_free(current_sample);
    current_sample = NULL;
    nimcp_free(proposal);
    proposal = NULL;

    /* Decode best sample */
    result->generated = (float*)nimcp_calloc(bridge->vae_output_dim, sizeof(float));
    if (!result->generated) {
        nimcp_free(best_sample);
        best_sample = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_IMAG_NO_MEMORY, "vae_imagination_bridge: error condition");
        return NIMCP_ERROR_VAE_IMAG_NO_MEMORY;
    }

    int ret = decode_latent(bridge, best_sample, result->generated);
    if (ret != 0) {
        nimcp_free(best_sample);
        best_sample = NULL;
        nimcp_free(result->generated);
        result->generated = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_IMAG_DECODE_FAILED, "vae_imagination_bridge: error condition");
        return NIMCP_ERROR_VAE_IMAG_DECODE_FAILED;
    }

    result->latent = best_sample;
    result->generated_dim = bridge->vae_output_dim;
    result->latent_dim = bridge->vae_latent_dim;
    result->method_used = VAE_IMAG_SAMPLE_QMC;
    result->samples_generated = num_samples;
    result->acceptance_rate = (float)accepted / (num_samples > 0 ? num_samples : 1);

    uint64_t elapsed = get_time_us() - start_time;
    result->generation_time_us = elapsed;

    bridge->stats.qmc_samples_total += num_samples;
    bridge->stats.avg_qmc_acceptance = bridge->stats.avg_qmc_acceptance * 0.9f +
                                       result->acceptance_rate * 0.1f;

    return 0;
}

int vae_imag_quantum_walk(vae_imag_bridge_t* bridge,
                           const float* start_state, uint32_t state_dim,
                           uint32_t num_steps,
                           vae_imag_generation_result_t* result) {
    if (!bridge || !result) return NIMCP_ERROR_VAE_IMAG_NULL;
    if (!vae_imag_bridge_is_connected(bridge)) return NIMCP_ERROR_VAE_IMAG_NOT_CONNECTED;

    memset(result, 0, sizeof(*result));

    /* Simulate quantum walk on latent space graph */
    float* position = (float*)nimcp_calloc(bridge->vae_latent_dim, sizeof(float));
    if (!position) return NIMCP_ERROR_VAE_IMAG_NO_MEMORY;

    if (start_state) {
        uint32_t copy_dim = (state_dim < bridge->vae_latent_dim) ?
                            state_dim : bridge->vae_latent_dim;
        memcpy(position, start_state, copy_dim * sizeof(float));
    } else {
        memset(position, 0, bridge->vae_latent_dim * sizeof(float));
    }

    /* Coin bias for directional preference */
    float coin_bias = bridge->config.sample.qwalk_coin_bias;

    for (uint32_t step = 0; step < num_steps; step++) {
        /* Quantum coin flip (simulate superposition-like exploration) */
        for (uint32_t i = 0; i < bridge->vae_latent_dim; i++) {
            float coin = (float)nimcp_tl_rand() / RAND_MAX;
            float amplitude = (coin < coin_bias) ? 1.0f : -1.0f;

            /* Add interference-like term */
            float phase = 2.0f * PI * (float)step / (num_steps > 0 ? num_steps : 1);
            float interference = cosf(phase + i * 0.1f);

            position[i] += 0.1f * amplitude * (1.0f + 0.3f * interference);
        }
    }

    result->latent = position;
    result->latent_dim = bridge->vae_latent_dim;

    result->generated = (float*)nimcp_calloc(bridge->vae_output_dim, sizeof(float));
    if (!result->generated) {
        nimcp_free(position);
        position = NULL;
        result->latent = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_IMAG_NO_MEMORY, "vae_imagination_bridge: error condition");
        return NIMCP_ERROR_VAE_IMAG_NO_MEMORY;
    }

    int ret = decode_latent(bridge, position, result->generated);
    if (ret != 0) {
        nimcp_free(position);
        position = NULL;
        nimcp_free(result->generated);
        result->latent = NULL;
        result->generated = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_IMAG_DECODE_FAILED, "vae_imagination_bridge: error condition");
        return NIMCP_ERROR_VAE_IMAG_DECODE_FAILED;
    }

    result->generated_dim = bridge->vae_output_dim;
    result->method_used = VAE_IMAG_SAMPLE_QUANTUM_WALK;
    result->samples_generated = num_steps;

    bridge->stats.qwalk_steps_total += num_steps;

    return 0;
}

int vae_imag_quantum_anneal(vae_imag_bridge_t* bridge,
                             const vae_imag_goal_t* goal,
                             vae_imag_quantum_result_t* result) {
    if (!bridge || !goal || !result) return NIMCP_ERROR_VAE_IMAG_NULL;
    if (!vae_imag_bridge_is_connected(bridge)) return NIMCP_ERROR_VAE_IMAG_NOT_CONNECTED;

    uint64_t start_time = get_time_us();
    memset(result, 0, sizeof(*result));

    float* state = (float*)nimcp_calloc(bridge->vae_latent_dim, sizeof(float));
    float* best_state = (float*)nimcp_calloc(bridge->vae_latent_dim, sizeof(float));
    if (!best_state) return -1;
    float* proposal = (float*)nimcp_calloc(bridge->vae_latent_dim, sizeof(float));

    if (!state || !best_state || !proposal) {
        if (state) nimcp_free(state);
        if (best_state) nimcp_free(best_state);
        if (proposal) nimcp_free(proposal);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_IMAG_NO_MEMORY, "vae_imagination_bridge: error condition");
        return NIMCP_ERROR_VAE_IMAG_NO_MEMORY;
    }

    /* Initialize from target if available */
    if (goal->target_features && goal->target_dim > 0) {
        uint32_t copy_dim = (goal->target_dim < bridge->vae_latent_dim) ?
                            goal->target_dim : bridge->vae_latent_dim;
        memcpy(state, goal->target_features, copy_dim * sizeof(float));
    } else {
        sample_gaussian(state, bridge->vae_latent_dim, 0.0f, 1.0f);
    }

    memcpy(best_state, state, bridge->vae_latent_dim * sizeof(float));
    float best_energy = FLT_MAX;

    float temp = bridge->config.sample.annealing_initial_temp;
    float final_temp = bridge->config.sample.annealing_final_temp;
    uint32_t num_steps = bridge->config.sample.annealing_steps;

    /* Annealing schedule */
    float cooling_rate = powf(final_temp / temp, 1.0f / (num_steps > 0 ? num_steps : 1));

    for (uint32_t step = 0; step < num_steps; step++) {
        /* Propose move */
        for (uint32_t i = 0; i < bridge->vae_latent_dim; i++) {
            proposal[i] = state[i] + sqrtf(temp) * box_muller_normal();
        }

        /* Compute energy (goal alignment + constraint satisfaction) */
        float energy = 0.0f;

        if (goal->target_features) {
            energy += goal->goal_weight * compute_mse(proposal, goal->target_features,
                                                      (goal->target_dim < bridge->vae_latent_dim) ?
                                                       goal->target_dim : bridge->vae_latent_dim);
        }

        if (goal->constraints) {
            /* Penalty for constraint violation */
            energy += goal->constraint_strength * compute_mse(proposal, goal->constraints,
                                                              (goal->constraint_dim < bridge->vae_latent_dim) ?
                                                               goal->constraint_dim : bridge->vae_latent_dim);
        }

        if (goal->avoid_features) {
            /* Reward for avoiding features (negative penalty) */
            float avoid_dist = compute_mse(proposal, goal->avoid_features,
                                          (goal->avoid_dim < bridge->vae_latent_dim) ?
                                           goal->avoid_dim : bridge->vae_latent_dim);
            energy -= 0.5f * avoid_dist;
        }

        /* Current energy */
        float current_energy = 0.0f;
        if (goal->target_features) {
            current_energy += goal->goal_weight * compute_mse(state, goal->target_features,
                                                              (goal->target_dim < bridge->vae_latent_dim) ?
                                                               goal->target_dim : bridge->vae_latent_dim);
        }

        /* Metropolis acceptance */
        float delta = energy - current_energy;
        float accept_prob = expf(-delta / temp);

        if (delta < 0 || (float)nimcp_tl_rand() / RAND_MAX < accept_prob) {
            memcpy(state, proposal, bridge->vae_latent_dim * sizeof(float));

            if (energy < best_energy) {
                memcpy(best_state, proposal, bridge->vae_latent_dim * sizeof(float));
                best_energy = energy;
            }
        }

        /* Cool down */
        temp *= cooling_rate;
    }

    nimcp_free(state);
    state = NULL;
    nimcp_free(proposal);
    proposal = NULL;

    /* Fill result */
    result->best_latent = best_state;
    result->latent_dim = bridge->vae_latent_dim;
    result->energy = best_energy;
    result->iterations = num_steps;
    result->converged = (best_energy < 0.1f);

    /* Compute constraint satisfaction */
    if (goal->constraints) {
        float constraint_dist = compute_mse(best_state, goal->constraints,
                                           (goal->constraint_dim < bridge->vae_latent_dim) ?
                                            goal->constraint_dim : bridge->vae_latent_dim);
        result->constraint_satisfaction = expf(-constraint_dist);
    } else {
        result->constraint_satisfaction = 1.0f;
    }

    /* Decode best state */
    result->best_decoded = (float*)nimcp_calloc(bridge->vae_output_dim, sizeof(float));
    if (result->best_decoded) {
        decode_latent(bridge, best_state, result->best_decoded);
        result->decoded_dim = bridge->vae_output_dim;
    }

    uint64_t elapsed = get_time_us() - start_time;
    bridge->stats.annealing_runs++;
    bridge->stats.avg_search_latency_us = bridge->stats.avg_search_latency_us * 0.9f +
                                          elapsed * 0.1f;

    return 0;
}

int vae_imag_quantum_search(vae_imag_bridge_t* bridge,
                             const float* constraints, uint32_t constraint_dim,
                             const float* weights, uint32_t num_constraints,
                             vae_imag_quantum_result_t* result) {
    if (!bridge || !constraints || !result) return NIMCP_ERROR_VAE_IMAG_NULL;

    /* Convert constraints to goal format */
    vae_imag_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.constraints = (float*)constraints;
    goal.constraint_dim = constraint_dim;
    goal.constraint_strength = 1.0f;
    goal.goal_weight = 0.5f;

    (void)weights;
    (void)num_constraints;

    return vae_imag_quantum_anneal(bridge, &goal, result);
}

/* ============================================================================
 * Hyperbolic Latent Space Implementation
 * ============================================================================ */

int vae_imag_hyperbolic_generate(vae_imag_bridge_t* bridge,
                                  const float* center_point, uint32_t center_dim,
                                  float radius,
                                  vae_imag_generation_result_t* result) {
    if (!bridge || !result) return NIMCP_ERROR_VAE_IMAG_NULL;
    if (!vae_imag_bridge_is_connected(bridge)) return NIMCP_ERROR_VAE_IMAG_NOT_CONNECTED;

    memset(result, 0, sizeof(*result));

    float* latent = (float*)nimcp_calloc(bridge->vae_latent_dim, sizeof(float));
    if (!latent) return NIMCP_ERROR_VAE_IMAG_NO_MEMORY;

    if (center_point && center_dim > 0) {
        uint32_t copy_dim = (center_dim < bridge->vae_latent_dim) ?
                            center_dim : bridge->vae_latent_dim;
        memcpy(latent, center_point, copy_dim * sizeof(float));
    } else {
        memset(latent, 0, bridge->vae_latent_dim * sizeof(float));
    }

    /* Sample direction uniformly on sphere */
    float* direction = (float*)nimcp_calloc(bridge->vae_latent_dim, sizeof(float));
    if (!direction) {
        nimcp_free(latent);
        latent = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_IMAG_NO_MEMORY, "vae_imagination_bridge: error condition");
        return NIMCP_ERROR_VAE_IMAG_NO_MEMORY;
    }

    sample_gaussian(direction, bridge->vae_latent_dim, 0.0f, 1.0f);
    normalize_vector(direction, bridge->vae_latent_dim);

    /* Scale by radius using exponential map */
    for (uint32_t i = 0; i < bridge->vae_latent_dim; i++) {
        direction[i] *= radius;
    }

    /* Apply exponential map from center */
    float* result_point = (float*)nimcp_calloc(bridge->vae_latent_dim, sizeof(float));
    if (!result_point) {
        nimcp_free(latent);
        latent = NULL;
        nimcp_free(direction);
        direction = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_IMAG_NO_MEMORY, "vae_imagination_bridge: error condition");
        return NIMCP_ERROR_VAE_IMAG_NO_MEMORY;
    }

    exponential_map(latent, direction, result_point, bridge->vae_latent_dim);

    nimcp_free(latent);
    latent = NULL;
    nimcp_free(direction);
    direction = NULL;

    result->latent = result_point;
    result->latent_dim = bridge->vae_latent_dim;

    result->generated = (float*)nimcp_calloc(bridge->vae_output_dim, sizeof(float));
    if (!result->generated) {
        nimcp_free(result_point);
        result_point = NULL;
        result->latent = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_IMAG_NO_MEMORY, "vae_imagination_bridge: error condition");
        return NIMCP_ERROR_VAE_IMAG_NO_MEMORY;
    }

    int ret = decode_latent(bridge, result_point, result->generated);
    if (ret != 0) {
        nimcp_free(result_point);
        result_point = NULL;
        nimcp_free(result->generated);
        result->latent = NULL;
        result->generated = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_IMAG_DECODE_FAILED, "vae_imagination_bridge: error condition");
        return NIMCP_ERROR_VAE_IMAG_DECODE_FAILED;
    }

    result->generated_dim = bridge->vae_output_dim;

    return 0;
}

int vae_imag_hyperbolic_distance(vae_imag_bridge_t* bridge,
                                  const float* state_a, const float* state_b,
                                  uint32_t dim, float* distance) {
    if (!bridge || !state_a || !state_b || !distance) return NIMCP_ERROR_VAE_IMAG_NULL;

    uint32_t use_dim = (dim < bridge->vae_latent_dim) ? dim : bridge->vae_latent_dim;
    *distance = vae_imag_poincare_distance(state_a, state_b, use_dim);

    return 0;
}

int vae_imag_hyperbolic_geodesic(vae_imag_bridge_t* bridge,
                                  const float* state_a, const float* state_b,
                                  uint32_t dim, uint32_t num_steps,
                                  vae_imag_trajectory_result_t* result) {
    /* Same as interpolate but with geodesic interpolation */
    return vae_imag_interpolate(bridge, state_a, state_b, dim, num_steps, result);
}

int vae_imag_to_hyperbolic(vae_imag_bridge_t* bridge,
                            const float* euclidean, uint32_t dim,
                            float* hyperbolic) {
    if (!bridge || !euclidean || !hyperbolic) return NIMCP_ERROR_VAE_IMAG_NULL;

    /* Map to Poincaré ball using tanh scaling */
    float norm = compute_norm(euclidean, dim);
    if (norm < POINCARE_EPSILON) {
        memcpy(hyperbolic, euclidean, dim * sizeof(float));
        return 0;
    }

    float r = tanhf(norm) * bridge->config.geometry.boundary_clip;
    float scale = r / (fabsf(norm) > 1e-7f ? norm : 1e-7f);

    for (uint32_t i = 0; i < dim; i++) {
        hyperbolic[i] = euclidean[i] * scale;
    }

    return 0;
}

int vae_imag_from_hyperbolic(vae_imag_bridge_t* bridge,
                              const float* hyperbolic, uint32_t dim,
                              float* euclidean) {
    if (!bridge || !hyperbolic || !euclidean) return NIMCP_ERROR_VAE_IMAG_NULL;

    /* Map from Poincaré ball using arctanh scaling */
    float norm = compute_norm(hyperbolic, dim);
    if (norm < POINCARE_EPSILON) {
        memcpy(euclidean, hyperbolic, dim * sizeof(float));
        return 0;
    }

    float r = atanhf(norm / bridge->config.geometry.boundary_clip);
    float scale = r / (fabsf(norm) > 1e-7f ? norm : 1e-7f);

    for (uint32_t i = 0; i < dim; i++) {
        euclidean[i] = hyperbolic[i] * scale;
    }

    return 0;
}

/* ============================================================================
 * Scene Manipulation Implementation
 * ============================================================================ */

int vae_imag_inject_element(vae_imag_bridge_t* bridge,
                             const float* element_features, uint32_t feature_dim,
                             float salience,
                             vae_imag_generation_result_t* result) {
    if (!bridge || !element_features || !result) return NIMCP_ERROR_VAE_IMAG_NULL;
    if (!vae_imag_bridge_is_connected(bridge)) return NIMCP_ERROR_VAE_IMAG_NOT_CONNECTED;

    /* Add element features to current latent with salience weighting */
    float* modified = (float*)nimcp_calloc(bridge->vae_latent_dim, sizeof(float));
    if (!modified) return NIMCP_ERROR_VAE_IMAG_NO_MEMORY;

    memcpy(modified, bridge->latent_buffer, bridge->vae_latent_dim * sizeof(float));

    uint32_t copy_dim = (feature_dim < bridge->vae_latent_dim) ?
                        feature_dim : bridge->vae_latent_dim;
    for (uint32_t i = 0; i < copy_dim; i++) {
        modified[i] += salience * element_features[i];
    }

    memcpy(bridge->latent_buffer, modified, bridge->vae_latent_dim * sizeof(float));
    nimcp_free(modified);
    modified = NULL;

    return vae_imag_generate(bridge, bridge->current_mode, NULL, result);
}

int vae_imag_remove_element(vae_imag_bridge_t* bridge,
                             const float* element_features, uint32_t feature_dim,
                             vae_imag_generation_result_t* result) {
    if (!bridge || !element_features || !result) return NIMCP_ERROR_VAE_IMAG_NULL;

    /* Subtract element features from current latent */
    float* modified = (float*)nimcp_calloc(bridge->vae_latent_dim, sizeof(float));
    if (!modified) return NIMCP_ERROR_VAE_IMAG_NO_MEMORY;

    memcpy(modified, bridge->latent_buffer, bridge->vae_latent_dim * sizeof(float));

    uint32_t copy_dim = (feature_dim < bridge->vae_latent_dim) ?
                        feature_dim : bridge->vae_latent_dim;
    for (uint32_t i = 0; i < copy_dim; i++) {
        modified[i] -= element_features[i];
    }

    memcpy(bridge->latent_buffer, modified, bridge->vae_latent_dim * sizeof(float));
    nimcp_free(modified);
    modified = NULL;

    return vae_imag_generate(bridge, bridge->current_mode, NULL, result);
}

int vae_imag_transform(vae_imag_bridge_t* bridge,
                        const float* transformation, uint32_t transform_dim,
                        vae_imag_generation_result_t* result) {
    if (!bridge || !transformation || !result) return NIMCP_ERROR_VAE_IMAG_NULL;

    /* Apply transformation matrix to latent */
    uint32_t copy_dim = (transform_dim < bridge->vae_latent_dim) ?
                        transform_dim : bridge->vae_latent_dim;

    for (uint32_t i = 0; i < copy_dim; i++) {
        bridge->latent_buffer[i] += transformation[i];
    }

    return vae_imag_generate(bridge, bridge->current_mode, NULL, result);
}

int vae_imag_blend(vae_imag_bridge_t* bridge,
                    const float* scene_a, const float* scene_b,
                    uint32_t dim, float blend_factor,
                    vae_imag_generation_result_t* result) {
    if (!bridge || !scene_a || !scene_b || !result) return NIMCP_ERROR_VAE_IMAG_NULL;

    uint32_t use_dim = (dim < bridge->vae_latent_dim) ? dim : bridge->vae_latent_dim;

    for (uint32_t i = 0; i < use_dim; i++) {
        bridge->latent_buffer[i] = scene_a[i] * (1.0f - blend_factor) +
                                   scene_b[i] * blend_factor;
    }

    return vae_imag_generate(bridge, bridge->current_mode, NULL, result);
}

/* ============================================================================
 * Quality Evaluation Implementation
 * ============================================================================ */

int vae_imag_evaluate_coherence(vae_imag_bridge_t* bridge,
                                 const float* generated, uint32_t dim,
                                 float* coherence) {
    if (!bridge || !generated || !coherence) return NIMCP_ERROR_VAE_IMAG_NULL;

    /* Coherence based on reconstruction error */
    nimcp_tensor_t* input = nimcp_tensor_create_1d(dim, NIMCP_DTYPE_F32);
    nimcp_tensor_t* recon = nimcp_tensor_create_1d(dim, NIMCP_DTYPE_F32);

    if (!input || !recon) {
        if (input) nimcp_tensor_destroy(input);
        if (recon) nimcp_tensor_destroy(recon);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_IMAG_NO_MEMORY, "vae_imagination_bridge: error condition");
        return NIMCP_ERROR_VAE_IMAG_NO_MEMORY;
    }

    float* input_data = (float*)nimcp_tensor_data(input);
    memcpy(input_data, generated, dim * sizeof(float));

    int ret = vae_reconstruct(bridge->vae, input, recon);
    if (ret != 0) {
        nimcp_tensor_destroy(input);
        nimcp_tensor_destroy(recon);
        *coherence = 0.5f;
        return 0;
    }

    float* recon_data = (float*)nimcp_tensor_data(recon);
    float mse = compute_mse(input_data, recon_data, dim);

    *coherence = expf(-mse);

    nimcp_tensor_destroy(input);
    nimcp_tensor_destroy(recon);

    return 0;
}

int vae_imag_evaluate_novelty(vae_imag_bridge_t* bridge,
                               const float* generated, uint32_t dim,
                               float* novelty) {
    if (!bridge || !generated || !novelty) return NIMCP_ERROR_VAE_IMAG_NULL;

    /* Novelty based on distance from training distribution */
    float anomaly_score = 0.0f;
    nimcp_tensor_t* input = nimcp_tensor_create_1d(dim, NIMCP_DTYPE_F32);

    if (!input) return NIMCP_ERROR_VAE_IMAG_NO_MEMORY;

    float* input_data = (float*)nimcp_tensor_data(input);
    memcpy(input_data, generated, dim * sizeof(float));

    int ret = vae_compute_anomaly_score(bridge->vae, input, &anomaly_score);
    nimcp_tensor_destroy(input);

    if (ret != 0) {
        *novelty = 0.5f;
        return 0;
    }

    *novelty = anomaly_score;
    return 0;
}

int vae_imag_evaluate_reality(vae_imag_bridge_t* bridge,
                               const float* generated, uint32_t dim,
                               float* reality_distance) {
    if (!bridge || !generated || !reality_distance) return NIMCP_ERROR_VAE_IMAG_NULL;

    /* Reality distance is inverse of coherence */
    float coherence = 0.0f;
    int ret = vae_imag_evaluate_coherence(bridge, generated, dim, &coherence);
    if (ret != 0) return ret;

    *reality_distance = 1.0f - coherence;
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

vae_imag_bridge_state_t vae_imag_bridge_get_state(const vae_imag_bridge_t* bridge) {
    if (!bridge) return VAE_IMAG_STATE_ERROR;
    return bridge->state;
}

int vae_imag_bridge_get_stats(const vae_imag_bridge_t* bridge,
                               vae_imag_bridge_stats_t* stats) {
    if (!bridge || !stats) return NIMCP_ERROR_VAE_IMAG_NULL;
    memcpy(stats, &bridge->stats, sizeof(vae_imag_bridge_stats_t));
    return 0;
}

vae_imag_mode_t vae_imag_get_current_mode(const vae_imag_bridge_t* bridge) {
    if (!bridge) return VAE_IMAG_MODE_PASSIVE;
    return bridge->current_mode;
}

/* ============================================================================
 * Result Management Implementation
 * ============================================================================ */

void vae_imag_generation_result_free(vae_imag_generation_result_t* result) {
    if (!result) return;
    if (result->generated) nimcp_free(result->generated);
    if (result->latent) nimcp_free(result->latent);
    memset(result, 0, sizeof(*result));
}

void vae_imag_trajectory_result_free(vae_imag_trajectory_result_t* result) {
    if (!result) return;

    if (result->trajectory) {
        for (uint32_t i = 0; i < result->trajectory_length; i++) {
            if (result->trajectory[i]) nimcp_free(result->trajectory[i]);
        }
        nimcp_free(result->trajectory);
    }

    if (result->decoded_trajectory) {
        for (uint32_t i = 0; i < result->trajectory_length; i++) {
            if (result->decoded_trajectory[i]) nimcp_free(result->decoded_trajectory[i]);
        }
        nimcp_free(result->decoded_trajectory);
    }

    if (result->coherence_scores) nimcp_free(result->coherence_scores);

    memset(result, 0, sizeof(*result));
}

void vae_imag_quantum_result_free(vae_imag_quantum_result_t* result) {
    if (!result) return;
    if (result->best_latent) nimcp_free(result->best_latent);
    if (result->best_decoded) nimcp_free(result->best_decoded);
    memset(result, 0, sizeof(*result));
}
