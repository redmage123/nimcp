/**
 * @file nimcp_gabor.c
 * @brief Implementation of unified Gabor filter library
 *
 * WHAT: Complete implementation of Gabor filter operations for V1 processing.
 * WHY:  Consolidates duplicate code from visual_cortex and orientation_columns.
 * HOW:  Implements 2D Gabor with kernel generation, convolution, and energy models.
 *
 * @version 1.0.0
 * @date 2025-12-19
 * @author NIMCP Development Team
 */

#include "utils/gabor/nimcp_gabor.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define LOG_MODULE "gabor"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(gabor)

/* ============================================================================
 * Global Statistics
 * ========================================================================== */

static gabor_stats_t g_gabor_stats = {0};

/* ============================================================================
 * Internal Constants
 * ========================================================================== */

#include "constants/nimcp_constants.h"
/** Small epsilon for numerical stability */
#define EPSILON NIMCP_EPSILON_NUMERICAL

/** Maximum bandwidth value */
#define MAX_BANDWIDTH 10.0f

/** Minimum wavelength value */
#define MIN_WAVELENGTH 0.1f

/* ============================================================================
 * Parameter Functions Implementation
 * ========================================================================== */

void gabor_default_params(gabor_filter_params_t* params) {
    if (!params) {
        return;
    }

    memset(params, 0, sizeof(gabor_filter_params_t));

    params->orientation_deg = 0.0f;
    params->wavelength = GABOR_DEFAULT_WAVELENGTH;
    params->phase_deg = 0.0f;
    params->aspect_ratio = GABOR_DEFAULT_ASPECT_RATIO;
    params->bandwidth = GABOR_DEFAULT_BANDWIDTH;
    params->sigma_x_override = 0.0f;  /* Use automatic calculation */
    params->sigma_y_override = 0.0f;
}

void gabor_params_for_orientation(gabor_filter_params_t* params, float orientation_deg) {
    if (!params) {
        return;
    }

    gabor_default_params(params);
    params->orientation_deg = gabor_normalize_orientation(orientation_deg);
}

void gabor_params_from_frequency(gabor_filter_params_t* params,
                                  float spatial_frequency,
                                  float orientation_deg) {
    if (!params) {
        return;
    }

    gabor_default_params(params);

    if (spatial_frequency > EPSILON) {
        params->wavelength = 1.0f / spatial_frequency;
    }

    params->orientation_deg = gabor_normalize_orientation(orientation_deg);
}

bool gabor_validate_params(const gabor_filter_params_t* params) {
    if (!params) {
        NIMCP_LOGGING_ERROR("NULL params");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gabor_validate_params: params is NULL");
        return false;
    }

    if (params->wavelength < MIN_WAVELENGTH) {
        NIMCP_LOGGING_ERROR("Invalid wavelength: %.4f (min: %.4f)",
                           params->wavelength, MIN_WAVELENGTH);
        return false;
    }

    if (params->aspect_ratio < EPSILON || params->aspect_ratio > 10.0f) {
        NIMCP_LOGGING_ERROR("Invalid aspect ratio: %.4f", params->aspect_ratio);
        return false;
    }

    if (params->bandwidth < EPSILON || params->bandwidth > MAX_BANDWIDTH) {
        NIMCP_LOGGING_ERROR("Invalid bandwidth: %.4f", params->bandwidth);
        return false;
    }

    return true;
}

void gabor_compute_sigmas(const gabor_filter_params_t* params,
                          float* sigma_x,
                          float* sigma_y) {
    if (!params || !sigma_x || !sigma_y) {
        return;
    }

    /* Use override if specified, otherwise compute from wavelength */
    if (params->sigma_x_override > EPSILON) {
        *sigma_x = params->sigma_x_override;
    } else {
        *sigma_x = params->wavelength * params->bandwidth;
    }

    if (params->sigma_y_override > EPSILON) {
        *sigma_y = params->sigma_y_override;
    } else {
        *sigma_y = *sigma_x / params->aspect_ratio;
    }
}

/* ============================================================================
 * Point Evaluation Functions Implementation
 * ========================================================================== */

float gabor_evaluate(float x, float y, const gabor_filter_params_t* params) {
    if (!params) {
        return 0.0f;
    }

    g_gabor_stats.point_evaluations++;

    /* Convert orientation to radians */
    float theta = gabor_deg_to_rad(params->orientation_deg);
    float cos_theta = cosf(theta);
    float sin_theta = sinf(theta);

    /* Rotate coordinates */
    float x_rot = x * cos_theta + y * sin_theta;
    float y_rot = -x * sin_theta + y * cos_theta;

    /* Compute effective sigmas */
    float sigma_x, sigma_y;
    gabor_compute_sigmas(params, &sigma_x, &sigma_y);

    if (sigma_x < EPSILON || sigma_y < EPSILON) {
        return 0.0f;
    }

    /* Apply aspect ratio to y coordinate */
    float gamma = params->aspect_ratio;

    /* Gaussian envelope: exp(-(x'² + γ²y'²)/(2σ²)) */
    float gaussian = expf(-(x_rot * x_rot + gamma * gamma * y_rot * y_rot) /
                          (2.0f * sigma_x * sigma_x));

    /* Sinusoidal carrier: cos(2π×x'/λ + ψ) */
    float phase_rad = gabor_deg_to_rad(params->phase_deg);
    float sinusoid = cosf(2.0f * (float)M_PI * x_rot / params->wavelength + phase_rad);

    return gaussian * sinusoid;
}

float gabor_evaluate_even(float x, float y, const gabor_filter_params_t* params) {
    if (!params) {
        return 0.0f;
    }

    /* Create temporary params with phase = 0 */
    gabor_filter_params_t even_params = *params;
    even_params.phase_deg = 0.0f;

    return gabor_evaluate(x, y, &even_params);
}

float gabor_evaluate_odd(float x, float y, const gabor_filter_params_t* params) {
    if (!params) {
        return 0.0f;
    }

    /* Create temporary params with phase = 90 */
    gabor_filter_params_t odd_params = *params;
    odd_params.phase_deg = 90.0f;

    return gabor_evaluate(x, y, &odd_params);
}

float gabor_compute_energy(float x, float y, const gabor_filter_params_t* params) {
    if (!params) {
        return 0.0f;
    }

    float even = gabor_evaluate_even(x, y, params);
    float odd = gabor_evaluate_odd(x, y, params);

    return sqrtf(even * even + odd * odd);
}

/* ============================================================================
 * Kernel Functions Implementation
 * ========================================================================== */

gabor_kernel_t* gabor_kernel_create(uint32_t size,
                                     const gabor_filter_params_t* params,
                                     bool dc_balance) {
    /* Guard clauses */
    if (!params) {
        NIMCP_LOGGING_ERROR("NULL params");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gabor_compute_energy: params is NULL");
        return NULL;
    }

    if (size < GABOR_MIN_KERNEL_SIZE || size > GABOR_MAX_KERNEL_SIZE) {
        NIMCP_LOGGING_ERROR("Invalid kernel size: %u (must be %d-%d)",
                           size, GABOR_MIN_KERNEL_SIZE, GABOR_MAX_KERNEL_SIZE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gabor_compute_energy: validation failed");
        return NULL;
    }

    if (size % 2 == 0) {
        NIMCP_LOGGING_ERROR("Kernel size must be odd: %u", size);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gabor_compute_energy: 2 is zero");
        return NULL;
    }

    if (!gabor_validate_params(params)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gabor_compute_energy: gabor_validate_params is NULL");
        return NULL;
    }

    /* Allocate kernel structure */
    gabor_kernel_t* kernel = (gabor_kernel_t*)nimcp_malloc(sizeof(gabor_kernel_t));
    if (!kernel) {
        NIMCP_LOGGING_ERROR("Failed to allocate kernel structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gabor_compute_energy: kernel is NULL");
        return NULL;
    }

    memset(kernel, 0, sizeof(gabor_kernel_t));

    /* Allocate kernel data */
    kernel->data = (float*)nimcp_calloc(size * size, sizeof(float));
    if (!kernel->data) {
        NIMCP_LOGGING_ERROR("Failed to allocate kernel data");
        nimcp_free(kernel);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gabor_compute_energy: kernel->data is NULL");
        return NULL;
    }

    kernel->size = size;
    kernel->params = *params;
    kernel->dc_balanced = dc_balance;

    int center = (int)size / 2;
    float sum = 0.0f;

    /* Generate Gabor kernel values */
    for (uint32_t y = 0; y < size; y++) {
        for (uint32_t x = 0; x < size; x++) {
            float x_offset = (float)((int)x - center);
            float y_offset = (float)((int)y - center);

            float value = gabor_evaluate(x_offset, y_offset, params);
            kernel->data[y * size + x] = value;
            sum += value;
        }
    }

    kernel->sum = sum;

    /* Apply DC balance if requested */
    if (dc_balance) {
        float mean = sum / (float)(size * size);
        for (uint32_t i = 0; i < size * size; i++) {
            kernel->data[i] -= mean;
        }
    }

    g_gabor_stats.kernels_created++;

    NIMCP_LOGGING_DEBUG("Created Gabor kernel: size=%u, orientation=%.1f°, wavelength=%.2f",
                        size, params->orientation_deg, params->wavelength);

    return kernel;
}

gabor_kernel_t* gabor_kernel_create_auto_size(const gabor_filter_params_t* params,
                                               bool dc_balance) {
    if (!params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "params is NULL");

        return NULL;
    }

    uint32_t size = gabor_optimal_kernel_size(params);
    return gabor_kernel_create(size, params, dc_balance);
}

void gabor_kernel_destroy(gabor_kernel_t* kernel) {
    if (!kernel) {
        return;
    }

    if (kernel->data) {
        nimcp_free(kernel->data);
    }

    nimcp_free(kernel);
}

float* gabor_create_kernel_data(int kernel_size, const gabor_filter_params_t* params) {
    if (!params || kernel_size <= 0 || kernel_size % 2 == 0) {
        NIMCP_LOGGING_ERROR("Invalid parameters for gabor_create_kernel_data");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gabor_create_kernel_data: params is NULL");
        return NULL;
    }

    gabor_kernel_t* kernel = gabor_kernel_create((uint32_t)kernel_size, params, true);
    if (!kernel) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kernel is NULL");

        return NULL;
    }

    /* Extract data and destroy wrapper */
    float* data = kernel->data;
    kernel->data = NULL;  /* Prevent double free */
    gabor_kernel_destroy(kernel);

    return data;
}

float gabor_kernel_get(const gabor_kernel_t* kernel, uint32_t x, uint32_t y) {
    if (!kernel || !kernel->data) {
        return 0.0f;
    }

    if (x >= kernel->size || y >= kernel->size) {
        return 0.0f;
    }

    return kernel->data[y * kernel->size + x];
}

bool gabor_kernel_normalize(gabor_kernel_t* kernel, float target_sum) {
    if (!kernel || !kernel->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gabor_kernel_normalize: required parameter is NULL (kernel, kernel->data)");
        return false;
    }

    /* Compute current sum */
    float current_sum = 0.0f;
    uint32_t n = kernel->size * kernel->size;

    for (uint32_t i = 0; i < n; i++) {
        current_sum += kernel->data[i];
    }

    if (fabsf(current_sum) < EPSILON) {
        NIMCP_LOGGING_WARN("Cannot normalize kernel with near-zero sum");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gabor_kernel_normalize: validation failed");
        return false;
    }

    float scale = target_sum / current_sum;
    for (uint32_t i = 0; i < n; i++) {
        kernel->data[i] *= scale;
    }

    kernel->sum = target_sum;
    return true;
}

/* ============================================================================
 * Filter Bank Functions Implementation
 * ========================================================================== */

gabor_filter_bank_t* gabor_filter_bank_create(uint32_t num_orientations,
                                               uint32_t kernel_size,
                                               float wavelength,
                                               bool include_quadrature) {
    /* Guard clauses */
    if (num_orientations == 0 || num_orientations > 64) {
        NIMCP_LOGGING_ERROR("Invalid num_orientations: %u", num_orientations);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gabor_kernel_normalize: num_orientations is zero");
        return NULL;
    }

    if (kernel_size < GABOR_MIN_KERNEL_SIZE || kernel_size > GABOR_MAX_KERNEL_SIZE) {
        NIMCP_LOGGING_ERROR("Invalid kernel_size: %u", kernel_size);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gabor_kernel_normalize: validation failed");
        return NULL;
    }

    /* Allocate bank structure */
    gabor_filter_bank_t* bank = (gabor_filter_bank_t*)nimcp_malloc(
        sizeof(gabor_filter_bank_t));
    if (!bank) {
        NIMCP_LOGGING_ERROR("Failed to allocate filter bank");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gabor_kernel_normalize: bank is NULL");
        return NULL;
    }

    memset(bank, 0, sizeof(gabor_filter_bank_t));

    bank->num_orientations = num_orientations;
    bank->kernel_size = kernel_size;
    bank->wavelength = wavelength;
    bank->include_quadrature = include_quadrature;
    bank->total_kernels = include_quadrature ? num_orientations * 2 : num_orientations;

    /* Allocate kernel array */
    bank->kernels = (gabor_kernel_t**)nimcp_calloc(bank->total_kernels,
                                                    sizeof(gabor_kernel_t*));
    if (!bank->kernels) {
        NIMCP_LOGGING_ERROR("Failed to allocate kernel array");
        nimcp_free(bank);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gabor_kernel_normalize: bank->kernels is NULL");
        return NULL;
    }

    /* Create kernels for each orientation */
    float orientation_step = 180.0f / (float)num_orientations;

    for (uint32_t i = 0; i < num_orientations; i++) {
        gabor_filter_params_t params;
        gabor_default_params(&params);
        params.orientation_deg = (float)i * orientation_step;
        params.wavelength = wavelength;

        /* Even kernel (phase = 0) */
        params.phase_deg = 0.0f;
        bank->kernels[i] = gabor_kernel_create(kernel_size, &params, true);
        if (!bank->kernels[i]) {
            NIMCP_LOGGING_ERROR("Failed to create even kernel %u", i);
            gabor_filter_bank_destroy(bank);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gabor_kernel_normalize: bank->kernels is NULL");
            return NULL;
        }

        /* Odd kernel (phase = 90) if quadrature enabled */
        if (include_quadrature) {
            params.phase_deg = 90.0f;
            uint32_t odd_idx = num_orientations + i;
            bank->kernels[odd_idx] = gabor_kernel_create(kernel_size, &params, true);
            if (!bank->kernels[odd_idx]) {
                NIMCP_LOGGING_ERROR("Failed to create odd kernel %u", i);
                gabor_filter_bank_destroy(bank);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gabor_kernel_normalize: bank->kernels is NULL");
                return NULL;
            }
        }
    }

    NIMCP_LOGGING_DEBUG("Created filter bank: %u orientations, size=%u, quadrature=%s",
                        num_orientations, kernel_size, include_quadrature ? "yes" : "no");

    return bank;
}

void gabor_filter_bank_destroy(gabor_filter_bank_t* bank) {
    if (!bank) {
        return;
    }

    if (bank->kernels) {
        for (uint32_t i = 0; i < bank->total_kernels; i++) {
            if (bank->kernels[i]) {
                gabor_kernel_destroy(bank->kernels[i]);
            }
        }
        nimcp_free(bank->kernels);
    }

    nimcp_free(bank);
}

const gabor_kernel_t* gabor_filter_bank_get_kernel(const gabor_filter_bank_t* bank,
                                                    uint32_t orientation_idx,
                                                    bool is_odd) {
    if (!bank || !bank->kernels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gabor_filter_bank_destroy: required parameter is NULL (bank, bank->kernels)");
        return NULL;
    }

    if (orientation_idx >= bank->num_orientations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gabor_filter_bank_destroy: capacity exceeded");
        return NULL;
    }

    if (is_odd && !bank->include_quadrature) {
        NIMCP_LOGGING_WARN("Quadrature not enabled for this filter bank");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gabor_filter_bank_destroy: bank->include_quadrature is NULL");
        return NULL;
    }

    uint32_t idx = is_odd ? (bank->num_orientations + orientation_idx) : orientation_idx;
    return bank->kernels[idx];
}

/* ============================================================================
 * Convolution Functions Implementation
 * ========================================================================== */

float gabor_convolve(const gabor_kernel_t* kernel,
                     const float* image,
                     uint32_t width,
                     uint32_t height) {
    /* Guard clauses */
    if (!kernel || !kernel->data || !image) {
        NIMCP_LOGGING_ERROR("NULL parameter in gabor_convolve");
        return 0.0f;
    }

    if (width == 0 || height == 0) {
        return 0.0f;
    }

    g_gabor_stats.convolutions++;

    float response = 0.0f;
    int center_x = (int)width / 2;
    int center_y = (int)height / 2;
    int k_center = (int)kernel->size / 2;

    /* Convolve kernel with image patch */
    for (uint32_t ky = 0; ky < kernel->size; ky++) {
        for (uint32_t kx = 0; kx < kernel->size; kx++) {
            int img_x = center_x + ((int)kx - k_center);
            int img_y = center_y + ((int)ky - k_center);

            /* Skip if outside image bounds */
            if (img_x < 0 || img_x >= (int)width ||
                img_y < 0 || img_y >= (int)height) {
                continue;
            }

            float kernel_val = kernel->data[ky * kernel->size + kx];
            float pixel_val = image[img_y * width + img_x];
            response += kernel_val * pixel_val;
        }
    }

    return response;
}

float gabor_energy_response(const gabor_filter_params_t* params,
                            const float* image,
                            uint32_t width,
                            uint32_t height,
                            uint32_t kernel_size) {
    if (!params || !image) {
        return 0.0f;
    }

    /* Create even and odd kernels */
    gabor_filter_params_t even_params = *params;
    even_params.phase_deg = 0.0f;

    gabor_filter_params_t odd_params = *params;
    odd_params.phase_deg = 90.0f;

    gabor_kernel_t* even_kernel = gabor_kernel_create(kernel_size, &even_params, true);
    gabor_kernel_t* odd_kernel = gabor_kernel_create(kernel_size, &odd_params, true);

    if (!even_kernel || !odd_kernel) {
        gabor_kernel_destroy(even_kernel);
        gabor_kernel_destroy(odd_kernel);
        return 0.0f;
    }

    float even_response = gabor_convolve(even_kernel, image, width, height);
    float odd_response = gabor_convolve(odd_kernel, image, width, height);

    gabor_kernel_destroy(even_kernel);
    gabor_kernel_destroy(odd_kernel);

    return sqrtf(even_response * even_response + odd_response * odd_response);
}

bool gabor_filter_bank_apply(const gabor_filter_bank_t* bank,
                             const float* image,
                             uint32_t width,
                             uint32_t height,
                             float* responses) {
    /* Guard clauses */
    if (!bank || !image || !responses) {
        NIMCP_LOGGING_ERROR("NULL parameter in gabor_filter_bank_apply");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gabor_filter_bank_destroy: required parameter is NULL (bank, image, responses)");
        return false;
    }

    /* Compute response for each kernel */
    for (uint32_t i = 0; i < bank->total_kernels; i++) {
        if (bank->kernels[i]) {
            responses[i] = gabor_convolve(bank->kernels[i], image, width, height);
        } else {
            responses[i] = 0.0f;
        }
    }

    return true;
}

/* ============================================================================
 * Utility Functions Implementation
 * ========================================================================== */

float gabor_deg_to_rad(float degrees) {
    return degrees * (float)M_PI / 180.0f;
}

float gabor_rad_to_deg(float radians) {
    return radians * 180.0f / (float)M_PI;
}

float gabor_normalize_orientation(float orientation) {
    while (orientation < 0.0f) {
        orientation += 180.0f;
    }
    while (orientation >= 180.0f) {
        orientation -= 180.0f;
    }
    return orientation;
}

float gabor_angular_difference(float angle1, float angle2) {
    float diff = fabsf(gabor_normalize_orientation(angle1) -
                       gabor_normalize_orientation(angle2));
    if (diff > 90.0f) {
        diff = 180.0f - diff;
    }
    return diff;
}

uint32_t gabor_optimal_kernel_size(const gabor_filter_params_t* params) {
    if (!params) {
        return 7;  /* Default */
    }

    float sigma_x, sigma_y;
    gabor_compute_sigmas(params, &sigma_x, &sigma_y);

    float max_sigma = (sigma_x > sigma_y) ? sigma_x : sigma_y;
    uint32_t size = (uint32_t)(2.0f * ceilf(max_sigma * GABOR_KERNEL_SIGMA_MULTIPLIER)) + 1;

    /* Clamp to valid range */
    if (size < GABOR_MIN_KERNEL_SIZE) {
        size = GABOR_MIN_KERNEL_SIZE;
    }
    if (size > GABOR_MAX_KERNEL_SIZE) {
        size = GABOR_MAX_KERNEL_SIZE;
    }

    /* Ensure odd */
    if (size % 2 == 0) {
        size++;
    }

    return size;
}

void gabor_get_stats(gabor_stats_t* stats) {
    if (!stats) {
        return;
    }
    memcpy(stats, &g_gabor_stats, sizeof(gabor_stats_t));
}

void gabor_reset_stats(void) {
    memset(&g_gabor_stats, 0, sizeof(gabor_stats_t));
}
