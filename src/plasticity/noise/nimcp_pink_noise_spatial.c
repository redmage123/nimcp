#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_pink_noise_spatial.c - Spatially Correlated Pink Noise
//=============================================================================

#include "plasticity/noise/nimcp_pink_noise_spatial.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_frequency_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(pink_noise_spatial)

//=============================================================================
// Internal: Distance and Correlation
//=============================================================================

static float compute_distance(
    float x1, float y1, float z1,
    float x2, float y2, float z2
) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float dz = z2 - z1;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

static float compute_correlation(
    float distance,
    const pink_spatial_config_t* config
) {
    float lambda = config->length_constant;
    float corr;

    switch (config->decay_type) {
        case PINK_SPATIAL_DECAY_EXPONENTIAL:
            corr = expf(-distance / lambda);
            break;
        case PINK_SPATIAL_DECAY_GAUSSIAN:
            corr = expf(-distance * distance / (2.0f * lambda * lambda));
            break;
        case PINK_SPATIAL_DECAY_POWER_LAW:
            corr = 1.0f / powf(1.0f + distance / lambda, config->power_law_exponent);
            break;
        default:
            corr = 1.0f;
    }

    // Apply floor
    if (corr < config->min_correlation) {
        corr = config->min_correlation;
    }

    return corr;
}

static bool cholesky_decomposition(
    const float* matrix,
    float* lower,
    uint32_t n
) {
    memset(lower, 0, n * n * sizeof(float));

    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j <= i; j++) {
            float sum = 0.0f;

            if (j == i) {
                for (uint32_t k = 0; k < j; k++) {
                    sum += lower[j * n + k] * lower[j * n + k];
                }
                float diag = matrix[i * n + j] - sum;
                if (diag < 0.0f) {
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cholesky_decomposition: validation failed");
                    return false;
                }
                lower[i * n + j] = sqrtf(diag);
            } else {
                for (uint32_t k = 0; k < j; k++) {
                    sum += lower[i * n + k] * lower[j * n + k];
                }
                float d = lower[j * n + j];
                if (fabsf(d) < 1e-10f) {
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cholesky_decomposition: validation failed");
                    return false;
                }
                lower[i * n + j] = (matrix[i * n + j] - sum) / d;
            }
        }
    }

    return true;
}

/**
 * @brief Cholesky decomposition with strided matrices
 *
 * WHAT: Compute L such that A = LL^T, with matrices stored with stride
 * WHY:  Support matrices allocated with extra capacity
 * HOW:  Same algorithm as cholesky_decomposition but with stride indexing
 *
 * @param matrix Input symmetric positive-definite matrix (n x n, stride storage)
 * @param lower Output lower triangular matrix (n x n, stride storage)
 * @param n Active matrix dimension
 * @param stride Storage stride (>= n)
 * @return true on success, false if matrix is not positive-definite
 */
static bool cholesky_decomposition_strided(
    const float* matrix,
    float* lower,
    uint32_t n,
    uint32_t stride
) {
    // Zero the output matrix
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            lower[i * stride + j] = 0.0f;
        }
    }

    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j <= i; j++) {
            float sum = 0.0f;

            if (j == i) {
                for (uint32_t k = 0; k < j; k++) {
                    sum += lower[j * stride + k] * lower[j * stride + k];
                }
                float diag = matrix[i * stride + j] - sum;
                if (diag < 0.0f) {
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cholesky_decomposition_strided: validation failed");
                    return false;
                }
                lower[i * stride + j] = sqrtf(diag);
            } else {
                for (uint32_t k = 0; k < j; k++) {
                    sum += lower[i * stride + k] * lower[j * stride + k];
                }
                float d = lower[j * stride + j];
                if (fabsf(d) < 1e-10f) {
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cholesky_decomposition_strided: validation failed");
                    return false;
                }
                lower[i * stride + j] = (matrix[i * stride + j] - sum) / d;
            }
        }
    }

    return true;
}

//=============================================================================
// Internal: Matrix Reallocation
//=============================================================================

/**
 * @brief Ensure matrices have capacity for at least n regions
 *
 * WHAT: Reallocate correlation matrices if needed
 * WHY:  Allow dynamic region addition without buffer overflow
 * HOW:  Double capacity when exceeded, copy existing data
 *
 * @param spatial Spatial noise generator
 * @param required_capacity Required number of regions
 * @return 0 on success, -1 on failure
 */
static int ensure_matrix_capacity(pink_spatial_t* spatial, uint32_t required_capacity) {
    if (!spatial) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ensure_matrix_capacity: spatial is NULL");
        return -1;
    }

    // Already have enough capacity
    if (required_capacity <= spatial->matrix_capacity) {
        return 0;
    }

    // Calculate new capacity (at least double, or required)
    uint32_t new_capacity = spatial->matrix_capacity * 2;
    if (new_capacity < required_capacity) {
        new_capacity = required_capacity;
    }
    if (new_capacity > PINK_SPATIAL_MAX_REGIONS) {
        new_capacity = PINK_SPATIAL_MAX_REGIONS;
    }
    if (new_capacity < required_capacity) {
        NIMCP_LOGGING_ERROR("Cannot allocate %u regions, max is %u",
                           required_capacity, PINK_SPATIAL_MAX_REGIONS);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ensure_matrix_capacity: validation failed");
        return -1;
    }

    // Allocate new matrices
    size_t new_size = (size_t)new_capacity * new_capacity * sizeof(float);
    float* new_distance = NULL;
    float* new_correlation = NULL;
    float* new_cholesky = NULL;
    unified_mem_handle_t new_distance_handle = NULL;
    unified_mem_handle_t new_correlation_handle = NULL;
    unified_mem_handle_t new_cholesky_handle = NULL;

    // Allocate via UMM if available
    if (spatial->mem_manager) {
        unified_mem_request_t req = unified_mem_request(new_size, NULL, true);
        new_distance_handle = unified_mem_alloc(spatial->mem_manager, &req);
        new_correlation_handle = unified_mem_alloc(spatial->mem_manager, &req);
        new_cholesky_handle = unified_mem_alloc(spatial->mem_manager, &req);

        if (new_distance_handle && new_correlation_handle && new_cholesky_handle) {
            new_distance = (float*)unified_mem_write(new_distance_handle);
            new_correlation = (float*)unified_mem_write(new_correlation_handle);
            new_cholesky = (float*)unified_mem_write(new_cholesky_handle);
            memset(new_distance, 0, new_size);
            memset(new_correlation, 0, new_size);
            memset(new_cholesky, 0, new_size);
        } else {
            // UMM allocation failed, fall back to direct allocation
            if (new_distance_handle) unified_mem_free(new_distance_handle);
            if (new_correlation_handle) unified_mem_free(new_correlation_handle);
            if (new_cholesky_handle) unified_mem_free(new_cholesky_handle);
            new_distance_handle = NULL;
            new_correlation_handle = NULL;
            new_cholesky_handle = NULL;
        }
    }

    // Fallback to direct allocation
    if (!new_distance) {
        new_distance = nimcp_calloc(new_capacity * new_capacity, sizeof(float));
        new_correlation = nimcp_calloc(new_capacity * new_capacity, sizeof(float));
        new_cholesky = nimcp_calloc(new_capacity * new_capacity, sizeof(float));
    }

    if (!new_distance || !new_correlation || !new_cholesky) {
        if (new_distance) nimcp_free(new_distance);
        if (new_correlation) nimcp_free(new_correlation);
        if (new_cholesky) nimcp_free(new_cholesky);
        NIMCP_LOGGING_ERROR("Failed to allocate matrices for %u regions", new_capacity);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ensure_matrix_capacity: validation failed");
        return -1;
    }

    // Copy existing data if present
    if (spatial->distance_matrix && spatial->matrix_capacity > 0) {
        uint32_t old_n = spatial->matrix_capacity;
        for (uint32_t i = 0; i < old_n && i < new_capacity; i++) {
            for (uint32_t j = 0; j < old_n && j < new_capacity; j++) {
                new_distance[i * new_capacity + j] = spatial->distance_matrix[i * old_n + j];
                new_correlation[i * new_capacity + j] = spatial->correlation_matrix[i * old_n + j];
                new_cholesky[i * new_capacity + j] = spatial->cholesky_matrix[i * old_n + j];
            }
        }
    }

    // Free old matrices
    if (spatial->distance_matrix_handle) {
        unified_mem_free(spatial->distance_matrix_handle);
    } else if (spatial->distance_matrix) {
        nimcp_free(spatial->distance_matrix);
    }
    if (spatial->correlation_matrix_handle) {
        unified_mem_free(spatial->correlation_matrix_handle);
    } else if (spatial->correlation_matrix) {
        nimcp_free(spatial->correlation_matrix);
    }
    if (spatial->cholesky_matrix_handle) {
        unified_mem_free(spatial->cholesky_matrix_handle);
    } else if (spatial->cholesky_matrix) {
        nimcp_free(spatial->cholesky_matrix);
    }

    // Install new matrices
    spatial->distance_matrix = new_distance;
    spatial->correlation_matrix = new_correlation;
    spatial->cholesky_matrix = new_cholesky;
    spatial->distance_matrix_handle = new_distance_handle;
    spatial->correlation_matrix_handle = new_correlation_handle;
    spatial->cholesky_matrix_handle = new_cholesky_handle;
    spatial->matrix_capacity = new_capacity;

    NIMCP_LOGGING_DEBUG("Expanded spatial matrices to capacity %u%s", new_capacity,
                        new_distance_handle ? " (via UMM)" : "");
    return 0;
}

//=============================================================================
// Configuration
//=============================================================================

pink_spatial_config_t pink_spatial_default_config(void) {
    pink_spatial_config_t config = {0};

    config.num_regions = 4;
    config.length_constant = 20.0f;  // 20mm
    config.min_correlation = 0.05f;
    config.decay_type = PINK_SPATIAL_DECAY_EXPONENTIAL;
    config.power_law_exponent = 2.0f;
    config.sample_rate = NIMCP_OSC_SAMPLE_RATE_HZ;
    config.seed = 0;

    // Default: 4 cortical regions
    config.regions[0] = (pink_spatial_region_t){"V1", 0.0f, -80.0f, 0.0f, 1.0f, 0.05f};
    config.regions[1] = (pink_spatial_region_t){"MT", 40.0f, -60.0f, 10.0f, 1.0f, 0.05f};
    config.regions[2] = (pink_spatial_region_t){"PPC", 30.0f, -40.0f, 40.0f, 1.0f, 0.05f};
    config.regions[3] = (pink_spatial_region_t){"PFC", 30.0f, 50.0f, 20.0f, 1.0f, 0.05f};

    return config;
}

pink_spatial_config_t pink_spatial_network_config(const char* network_type) {
    pink_spatial_config_t config = pink_spatial_default_config();

    if (strcmp(network_type, "visual") == 0) {
        config.num_regions = 5;
        config.regions[0] = (pink_spatial_region_t){"V1", 0, -90, 0, 1.0f, 0.04f};
        config.regions[1] = (pink_spatial_region_t){"V2", 10, -80, 5, 1.0f, 0.04f};
        config.regions[2] = (pink_spatial_region_t){"V4", 30, -70, 10, 1.0f, 0.05f};
        config.regions[3] = (pink_spatial_region_t){"MT", 40, -60, 15, 1.0f, 0.05f};
        config.regions[4] = (pink_spatial_region_t){"IT", 50, -30, -10, 1.0f, 0.06f};
    } else if (strcmp(network_type, "motor") == 0) {
        config.num_regions = 4;
        config.regions[0] = (pink_spatial_region_t){"M1", 35, -20, 60, 0.9f, 0.04f};
        config.regions[1] = (pink_spatial_region_t){"SMA", 5, 0, 70, 0.95f, 0.05f};
        config.regions[2] = (pink_spatial_region_t){"PMC", 40, 0, 50, 0.95f, 0.05f};
        config.regions[3] = (pink_spatial_region_t){"Cerebellum", 25, -60, -30, 1.1f, 0.06f};
    } else if (strcmp(network_type, "default_mode") == 0) {
        config.num_regions = 5;
        config.regions[0] = (pink_spatial_region_t){"mPFC", 5, 55, 5, 1.1f, 0.06f};
        config.regions[1] = (pink_spatial_region_t){"PCC", 5, -55, 25, 1.1f, 0.06f};
        config.regions[2] = (pink_spatial_region_t){"AG_L", -50, -60, 30, 1.0f, 0.05f};
        config.regions[3] = (pink_spatial_region_t){"AG_R", 50, -60, 30, 1.0f, 0.05f};
        config.regions[4] = (pink_spatial_region_t){"MTL", 30, -15, -25, 1.2f, 0.07f};
    } else if (strcmp(network_type, "salience") == 0) {
        config.num_regions = 4;
        config.regions[0] = (pink_spatial_region_t){"AI_L", -35, 20, 5, 0.9f, 0.05f};
        config.regions[1] = (pink_spatial_region_t){"AI_R", 35, 20, 5, 0.9f, 0.05f};
        config.regions[2] = (pink_spatial_region_t){"dACC", 5, 25, 35, 0.95f, 0.05f};
        config.regions[3] = (pink_spatial_region_t){"Amygdala", 25, 0, -20, 1.1f, 0.06f};
    }

    return config;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

pink_spatial_t* pink_spatial_create(const pink_spatial_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_spatial_create: config is NULL");
        return NULL;
    }
    if (config->num_regions == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAMETER, "pink_spatial_create: num_regions is 0");
        return NULL;
    }
    if (config->num_regions > PINK_SPATIAL_MAX_REGIONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAMETER, "pink_spatial_create: num_regions exceeds max");
        return NULL;
    }

    pink_spatial_t* spatial = nimcp_calloc(1, sizeof(pink_spatial_t));
    if (!spatial) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "spatial is NULL");

        return NULL;

    }

    memcpy(&spatial->config, config, sizeof(pink_spatial_config_t));

    // Allocate matrices with some headroom for future add_region calls
    uint32_t n = config->num_regions;
    uint32_t initial_capacity = n + 4;  // Add headroom for dynamic additions
    if (initial_capacity > PINK_SPATIAL_MAX_REGIONS) {
        initial_capacity = PINK_SPATIAL_MAX_REGIONS;
    }

    spatial->matrix_capacity = initial_capacity;
    spatial->distance_matrix = nimcp_calloc(initial_capacity * initial_capacity, sizeof(float));
    spatial->correlation_matrix = nimcp_calloc(initial_capacity * initial_capacity, sizeof(float));
    spatial->cholesky_matrix = nimcp_calloc(initial_capacity * initial_capacity, sizeof(float));

    if (!spatial->distance_matrix || !spatial->correlation_matrix ||
        !spatial->cholesky_matrix) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pink_spatial_create: matrix allocation failed");
        pink_spatial_destroy(spatial);
        return NULL;
    }

    // Create generators for each region
    for (uint32_t i = 0; i < n; i++) {
        pink_noise_config_t gen_config = pink_noise_default_config();
        gen_config.alpha = config->regions[i].alpha;
        gen_config.amplitude = config->regions[i].amplitude;
        gen_config.seed = config->seed + i;

        spatial->generators[i] = pink_noise_create(&gen_config);
        if (!spatial->generators[i]) {
            pink_spatial_destroy(spatial);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pink_spatial_create: spatial->generators is NULL");
            return NULL;
        }
    }

    // Compute correlation matrices
    pink_spatial_compute_correlations(spatial);

    NIMCP_LOGGING_INFO("Created spatial pink noise with %u regions", n);
    return spatial;
}

void pink_spatial_destroy(pink_spatial_t* spatial) {
    if (!spatial) return;

    for (uint32_t i = 0; i < spatial->config.num_regions; i++) {
        if (spatial->generators[i]) {
            pink_noise_destroy(spatial->generators[i]);
        }
    }

    // Free matrices via UMM or direct free
    if (spatial->distance_matrix_handle) {
        unified_mem_free(spatial->distance_matrix_handle);
    } else if (spatial->distance_matrix) {
        nimcp_free(spatial->distance_matrix);
    }
    if (spatial->correlation_matrix_handle) {
        unified_mem_free(spatial->correlation_matrix_handle);
    } else if (spatial->correlation_matrix) {
        nimcp_free(spatial->correlation_matrix);
    }
    if (spatial->cholesky_matrix_handle) {
        unified_mem_free(spatial->cholesky_matrix_handle);
    } else if (spatial->cholesky_matrix) {
        nimcp_free(spatial->cholesky_matrix);
    }

    nimcp_free(spatial);
}

//=============================================================================
// Correlation Functions
//=============================================================================

int pink_spatial_compute_correlations(pink_spatial_t* spatial) {
    if (!spatial) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_spatial_compute_correlations: spatial is NULL");
        return -1;
    }

    uint32_t n = spatial->config.num_regions;
    uint32_t stride = spatial->matrix_capacity;

    // Ensure we have capacity
    if (n > stride) {
        if (ensure_matrix_capacity(spatial, n) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pink_spatial_compute_correlations: validation failed");
            return -1;
        }
        stride = spatial->matrix_capacity;
    }

    // Compute distance matrix (using stride for proper indexing)
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            if (i == j) {
                spatial->distance_matrix[i * stride + j] = 0.0f;
            } else {
                spatial->distance_matrix[i * stride + j] = compute_distance(
                    spatial->config.regions[i].x,
                    spatial->config.regions[i].y,
                    spatial->config.regions[i].z,
                    spatial->config.regions[j].x,
                    spatial->config.regions[j].y,
                    spatial->config.regions[j].z
                );
            }
        }
    }

    // Compute correlation matrix
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            if (i == j) {
                spatial->correlation_matrix[i * stride + j] = 1.0f;
            } else {
                spatial->correlation_matrix[i * stride + j] = compute_correlation(
                    spatial->distance_matrix[i * stride + j],
                    &spatial->config
                );
            }
        }
    }

    // Compute Cholesky decomposition with proper stride
    spatial->matrices_valid = cholesky_decomposition_strided(
        spatial->correlation_matrix,
        spatial->cholesky_matrix,
        n,
        stride
    );

    if (!spatial->matrices_valid) {
        NIMCP_LOGGING_WARN("Cholesky failed, using identity");
        for (uint32_t i = 0; i < n; i++) {
            spatial->cholesky_matrix[i * stride + i] = 1.0f;
        }
        spatial->matrices_valid = true;
    }

    return 0;
}

int pink_spatial_set_correlation_matrix(
    pink_spatial_t* spatial,
    const float* matrix
) {
    if (!spatial) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_spatial_set_correlation_matrix: spatial is NULL");
        return -1;
    }
    if (!matrix) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_spatial_set_correlation_matrix: matrix is NULL");
        return -1;
    }

    uint32_t n = spatial->config.num_regions;
    uint32_t stride = spatial->matrix_capacity;

    // Copy from dense input matrix to strided storage
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            spatial->correlation_matrix[i * stride + j] = matrix[i * n + j];
        }
    }

    // Compute Cholesky with strided storage
    spatial->matrices_valid = cholesky_decomposition_strided(
        spatial->correlation_matrix,
        spatial->cholesky_matrix,
        n,
        stride
    );

    return spatial->matrices_valid ? 0 : -1;
}

//=============================================================================
// Generation Functions
//=============================================================================

int pink_spatial_step(pink_spatial_t* spatial) {
    if (!spatial) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_spatial_step: spatial is NULL");
        return -1;
    }

    uint32_t n = spatial->config.num_regions;
    uint32_t stride = spatial->matrix_capacity;

    // Generate independent pink noise
    for (uint32_t i = 0; i < n; i++) {
        pink_noise_generate_sample(spatial->generators[i],
                                   &spatial->independent_values[i]);
    }

    // Apply Cholesky transformation for spatial correlation (using stride)
    for (uint32_t i = 0; i < n; i++) {
        float sum = 0.0f;
        for (uint32_t j = 0; j <= i; j++) {
            sum += spatial->cholesky_matrix[i * stride + j] * spatial->independent_values[j];
        }
        spatial->current_values[i] = sum;
    }

    spatial->sample_count++;
    return 0;
}

float pink_spatial_get_region(
    const pink_spatial_t* spatial,
    uint32_t region_index
) {
    if (!spatial || region_index >= spatial->config.num_regions) return 0.0f;
    return spatial->current_values[region_index];
}

float pink_spatial_get_named(
    const pink_spatial_t* spatial,
    const char* name
) {
    if (!spatial || !name) return 0.0f;

    for (uint32_t i = 0; i < spatial->config.num_regions; i++) {
        if (spatial->config.regions[i].name &&
            strcmp(spatial->config.regions[i].name, name) == 0) {
            return spatial->current_values[i];
        }
    }

    return 0.0f;
}

int pink_spatial_get_all(
    const pink_spatial_t* spatial,
    float* values
) {
    if (!spatial) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_spatial_get_all: spatial is NULL");
        return -1;
    }
    if (!values) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_spatial_get_all: values is NULL");
        return -1;
    }
    memcpy(values, spatial->current_values,
           spatial->config.num_regions * sizeof(float));
    return 0;
}

float pink_spatial_get_correlation(
    const pink_spatial_t* spatial,
    uint32_t region_i,
    uint32_t region_j
) {
    if (!spatial) return 0.0f;
    if (region_i >= spatial->config.num_regions) return 0.0f;
    if (region_j >= spatial->config.num_regions) return 0.0f;

    uint32_t stride = spatial->matrix_capacity;
    return spatial->correlation_matrix[region_i * stride + region_j];
}

float pink_spatial_get_distance(
    const pink_spatial_t* spatial,
    uint32_t region_i,
    uint32_t region_j
) {
    if (!spatial) return 0.0f;
    if (region_i >= spatial->config.num_regions) return 0.0f;
    if (region_j >= spatial->config.num_regions) return 0.0f;

    uint32_t stride = spatial->matrix_capacity;
    return spatial->distance_matrix[region_i * stride + region_j];
}

int pink_spatial_add_region(
    pink_spatial_t* spatial,
    const char* name,
    float x, float y, float z,
    float alpha,
    float amplitude
) {
    if (!spatial) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_spatial_add_region: spatial is NULL");
        return -1;
    }
    if (spatial->config.num_regions >= PINK_SPATIAL_MAX_REGIONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAMETER, "pink_spatial_add_region: max regions reached");
        return -1;
    }

    uint32_t idx = spatial->config.num_regions;
    uint32_t new_count = idx + 1;

    // Ensure we have capacity for the new region BEFORE adding it
    if (ensure_matrix_capacity(spatial, new_count) != 0) {
        NIMCP_LOGGING_ERROR("Failed to expand matrices for region %s", name);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pink_spatial_add_region: validation failed");
        return -1;
    }

    spatial->config.regions[idx].name = name;
    spatial->config.regions[idx].x = x;
    spatial->config.regions[idx].y = y;
    spatial->config.regions[idx].z = z;
    spatial->config.regions[idx].alpha = alpha;
    spatial->config.regions[idx].amplitude = amplitude;

    // Create generator for new region
    pink_noise_config_t gen_config = pink_noise_default_config();
    gen_config.alpha = alpha;
    gen_config.amplitude = amplitude;
    gen_config.seed = spatial->config.seed + idx;

    spatial->generators[idx] = pink_noise_create(&gen_config);
    if (!spatial->generators[idx]) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_spatial_add_region: spatial->generators is NULL");
        return -1;
    }

    spatial->config.num_regions++;

    // Recompute correlations (now safe with proper capacity)
    pink_spatial_compute_correlations(spatial);

    return 0;
}

int pink_spatial_reset(pink_spatial_t* spatial, uint32_t new_seed) {
    if (!spatial) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_spatial_reset: spatial is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < spatial->config.num_regions; i++) {
        uint32_t seed = (new_seed != 0) ? new_seed + i : spatial->config.seed + i;
        pink_noise_reset(spatial->generators[i], seed);
        spatial->current_values[i] = 0.0f;
        spatial->independent_values[i] = 0.0f;
    }

    spatial->sample_count = 0;
    return 0;
}

//=============================================================================
// Unified Memory Manager Integration
//=============================================================================

int pink_spatial_connect_memory_manager(
    pink_spatial_t* spatial,
    unified_mem_manager_t mem_manager
) {
    if (!spatial) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_spatial_connect_memory_manager: spatial is NULL");
        return -1;
    }

    // Store the memory manager
    spatial->mem_manager = mem_manager;

    // If we already have matrices allocated and manager is now set,
    // migrate them to UMM for CoW benefits
    if (mem_manager && spatial->distance_matrix && !spatial->distance_matrix_handle) {
        uint32_t n = spatial->matrix_capacity;
        size_t matrix_size = (size_t)n * n * sizeof(float);

        // Migrate each matrix to UMM
        unified_mem_request_t req;

        // Distance matrix
        req = unified_mem_request(matrix_size, spatial->distance_matrix, true);
        unified_mem_handle_t new_dist_handle = unified_mem_alloc(mem_manager, &req);

        // Correlation matrix
        req = unified_mem_request(matrix_size, spatial->correlation_matrix, true);
        unified_mem_handle_t new_corr_handle = unified_mem_alloc(mem_manager, &req);

        // Cholesky matrix
        req = unified_mem_request(matrix_size, spatial->cholesky_matrix, true);
        unified_mem_handle_t new_chol_handle = unified_mem_alloc(mem_manager, &req);

        // If all succeeded, install new handles
        if (new_dist_handle && new_corr_handle && new_chol_handle) {
            // Free old allocations
            nimcp_free(spatial->distance_matrix);
            nimcp_free(spatial->correlation_matrix);
            nimcp_free(spatial->cholesky_matrix);

            // Install new handles
            spatial->distance_matrix_handle = new_dist_handle;
            spatial->correlation_matrix_handle = new_corr_handle;
            spatial->cholesky_matrix_handle = new_chol_handle;
            spatial->distance_matrix = (float*)unified_mem_write(new_dist_handle);
            spatial->correlation_matrix = (float*)unified_mem_write(new_corr_handle);
            spatial->cholesky_matrix = (float*)unified_mem_write(new_chol_handle);

            NIMCP_LOGGING_INFO("Migrated spatial matrices to UMM (%zu bytes each)", matrix_size);
        } else {
            // Cleanup failed allocations
            if (new_dist_handle) unified_mem_free(new_dist_handle);
            if (new_corr_handle) unified_mem_free(new_corr_handle);
            if (new_chol_handle) unified_mem_free(new_chol_handle);
            NIMCP_LOGGING_WARN("Failed to migrate spatial matrices to UMM, keeping direct allocations");
        }
    }

    return 0;
}

bool pink_spatial_has_memory_manager(const pink_spatial_t* spatial) {
    if (!spatial) {
        return false;  /* Boolean query returning false for NULL is normal */
    }
    return spatial->mem_manager != NULL;
}
