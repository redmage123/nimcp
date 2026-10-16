#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_pink_noise_correlated.c - Correlated Multi-Channel Pink Noise
//=============================================================================
/**
 * WHAT: Multivariate pink noise with inter-channel correlations
 * WHY:  Neuromodulators have biologically realistic correlation structure
 * HOW:  Cholesky decomposition + per-channel pink filtering
 */

#include "plasticity/noise/nimcp_pink_noise_correlated.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_frequency_constants.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(pink_noise_correlated)

//=============================================================================
// Internal: Cholesky Decomposition
//=============================================================================

static bool compute_cholesky(
    const float* matrix,
    float* lower,
    uint32_t n
) {
    /**
     * WHAT: Compute Cholesky decomposition L such that LL^T = matrix
     * WHY:  Needed to generate correlated noise from independent sources
     * HOW:  Standard Cholesky-Banachiewicz algorithm
     */
    memset(lower, 0, n * n * sizeof(float));

    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j <= i; j++) {
            float sum = 0.0f;

            if (j == i) {
                // Diagonal element
                for (uint32_t k = 0; k < j; k++) {
                    sum += lower[j * n + k] * lower[j * n + k];
                }
                float diag = matrix[i * n + j] - sum;
                if (diag < 0.0f) {
                    NIMCP_LOGGING_ERROR("Matrix not positive semi-definite");
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "compute_cholesky: validation failed");
                    return false;
                }
                lower[i * n + j] = sqrtf(diag);
            } else {
                // Off-diagonal element
                for (uint32_t k = 0; k < j; k++) {
                    sum += lower[i * n + k] * lower[j * n + k];
                }
                float diag = lower[j * n + j];
                if (fabsf(diag) < 1e-10f) {
                    NIMCP_LOGGING_ERROR("Near-zero diagonal in Cholesky");
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "compute_cholesky: validation failed");
                    return false;
                }
                lower[i * n + j] = (matrix[i * n + j] - sum) / diag;
            }
        }
    }

    return true;
}

static float random_normal(uint32_t* state) {
    /**
     * WHAT: Generate standard normal random variable
     * WHY:  Needed for white noise generation
     * HOW:  Box-Muller transform
     */
    *state = *state * NIMCP_LCG_MULTIPLIER + NIMCP_LCG_INCREMENT;
    float u1 = (float)(*state % 65536) / 65536.0f;
    *state = *state * NIMCP_LCG_MULTIPLIER + NIMCP_LCG_INCREMENT;
    float u2 = (float)(*state % 65536) / 65536.0f;

    // Avoid log(0)
    if (u1 < 1e-10f) u1 = 1e-10f;

    return sqrtf(-2.0f * logf(u1)) * cosf(NIMCP_TWO_PI_F * u2);
}

//=============================================================================
// Default Configurations
//=============================================================================

pink_noise_correlated_config_t pink_noise_correlated_neuromod_config(void) {
    /**
     * WHAT: Create biologically-motivated neuromodulator configuration
     * WHY:  DA, 5-HT, ACh, NE have known correlation structure
     */
    pink_noise_correlated_config_t config = {0};

    config.num_channels = PINK_NOISE_NEUROMOD_CHANNELS;
    config.correlation_type = PINK_CORR_NEUROMODULATORS;
    config.sample_rate = NIMCP_OSC_SAMPLE_RATE_HZ;
    config.seed = 0;

    // Channel configurations
    config.channels[0].name = "dopamine";
    config.channels[0].alpha = 1.0f;
    config.channels[0].amplitude = 0.1f;

    config.channels[1].name = "serotonin";
    config.channels[1].alpha = 1.0f;
    config.channels[1].amplitude = 0.05f;

    config.channels[2].name = "acetylcholine";
    config.channels[2].alpha = 0.9f;  // Slightly whiter
    config.channels[2].amplitude = 0.15f;

    config.channels[3].name = "norepinephrine";
    config.channels[3].alpha = 1.0f;
    config.channels[3].amplitude = 0.08f;

    // Correlation matrix (row-major):
    //        DA    5-HT   ACh    NE
    // DA    1.00  -0.30  0.20   0.60
    // 5-HT -0.30   1.00  0.15  -0.20
    // ACh   0.20   0.15  1.00   0.25
    // NE    0.60  -0.20  0.25   1.00
    float corr[] = {
        1.00f, -0.30f, 0.20f, 0.60f,
       -0.30f,  1.00f, 0.15f,-0.20f,
        0.20f,  0.15f, 1.00f, 0.25f,
        0.60f, -0.20f, 0.25f, 1.00f
    };
    memcpy(config.correlation_matrix, corr, sizeof(corr));

    return config;
}

pink_noise_correlated_config_t pink_noise_correlated_independent_config(
    uint32_t num_channels
) {
    /**
     * WHAT: Create configuration with no correlation
     * WHY:  Baseline for comparison or when independence is desired
     */
    pink_noise_correlated_config_t config = {0};

    if (num_channels > PINK_NOISE_MAX_CHANNELS) {
        num_channels = PINK_NOISE_MAX_CHANNELS;
    }

    config.num_channels = num_channels;
    config.correlation_type = PINK_CORR_INDEPENDENT;
    config.sample_rate = NIMCP_OSC_SAMPLE_RATE_HZ;
    config.seed = 0;

    // Identity correlation matrix
    for (uint32_t i = 0; i < num_channels; i++) {
        config.channels[i].name = NULL;
        config.channels[i].alpha = 1.0f;
        config.channels[i].amplitude = 0.1f;
        config.correlation_matrix[i * PINK_NOISE_MAX_CHANNELS + i] = 1.0f;
    }

    return config;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

pink_noise_correlated_t* pink_noise_correlated_create(
    const pink_noise_correlated_config_t* config
) {
    /**
     * WHAT: Create correlated multi-channel noise generator
     * WHY:  Enable biologically realistic correlated fluctuations
     * HOW:  Compute Cholesky, create per-channel generators
     */
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;

    }
    if (config->num_channels == 0 || config->num_channels > PINK_NOISE_MAX_CHANNELS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAMETER, "pink_noise_correlated_create: invalid num_channels");
        NIMCP_LOGGING_ERROR("Invalid channel count: %u", config->num_channels);
        return NULL;
    }

    pink_noise_correlated_t* cn = nimcp_calloc(1, sizeof(pink_noise_correlated_t));
    if (!cn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cn is NULL");

        return NULL;

    }

    memcpy(&cn->config, config, sizeof(pink_noise_correlated_config_t));

    // Compute Cholesky decomposition
    cn->cholesky_valid = compute_cholesky(
        config->correlation_matrix,
        cn->cholesky_matrix,
        config->num_channels
    );

    if (!cn->cholesky_valid) {
        NIMCP_LOGGING_WARN("Cholesky failed, using identity (independent channels)");
        for (uint32_t i = 0; i < config->num_channels; i++) {
            cn->cholesky_matrix[i * PINK_NOISE_MAX_CHANNELS + i] = 1.0f;
        }
        cn->cholesky_valid = true;
    }

    // Create per-channel generators
    for (uint32_t i = 0; i < config->num_channels; i++) {
        pink_noise_config_t gen_config = pink_noise_default_config();
        gen_config.alpha = config->channels[i].alpha;
        gen_config.amplitude = config->channels[i].amplitude;
        gen_config.sample_rate = config->sample_rate;
        gen_config.seed = config->seed + i;

        cn->generators[i] = pink_noise_create(&gen_config);
        if (!cn->generators[i]) {
            NIMCP_LOGGING_ERROR("Failed to create generator for channel %u", i);
            pink_noise_correlated_destroy(cn);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pink_noise_correlated_create: cn->generators is NULL");
            return NULL;
        }
    }

    NIMCP_LOGGING_INFO("Created correlated noise with %u channels", config->num_channels);
    return cn;
}

void pink_noise_correlated_destroy(pink_noise_correlated_t* cn) {
    /**
     * WHAT: Free all resources
     * WHY:  Prevent memory leaks
     */
    if (!cn) return;

    for (uint32_t i = 0; i < cn->config.num_channels; i++) {
        if (cn->generators[i]) {
            pink_noise_destroy(cn->generators[i]);
        }
    }

    nimcp_free(cn);
}

//=============================================================================
// Generation Functions
//=============================================================================

int pink_noise_correlated_step(pink_noise_correlated_t* cn) {
    /**
     * WHAT: Generate one sample for all channels with correlation
     * WHY:  Streaming correlated noise generation
     * HOW:  Generate independent → apply Cholesky → filter to pink
     */
    if (!cn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_correlated_step: cn is NULL");
        return -1;
    }

    uint32_t n = cn->config.num_channels;

    // Generate independent pink noise samples
    for (uint32_t i = 0; i < n; i++) {
        float sample;
        if (!pink_noise_generate_sample(cn->generators[i], &sample)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pink_noise_correlated_step: pink_noise_generate_sample is NULL");
            return -1;
        }
        cn->white_buffer[i] = sample;
    }

    // Apply Cholesky transformation for correlation
    for (uint32_t i = 0; i < n; i++) {
        float sum = 0.0f;
        for (uint32_t j = 0; j <= i; j++) {
            sum += cn->cholesky_matrix[i * PINK_NOISE_MAX_CHANNELS + j] * cn->white_buffer[j];
        }
        cn->current_values[i] = sum;
    }

    cn->sample_count++;
    return 0;
}

float pink_noise_correlated_get_channel(
    const pink_noise_correlated_t* cn,
    uint32_t channel_index
) {
    /**
     * WHAT: Get current value for specific channel
     */
    if (!cn || channel_index >= cn->config.num_channels) return 0.0f;
    return cn->current_values[channel_index];
}

int pink_noise_correlated_get_all(
    const pink_noise_correlated_t* cn,
    float* values
) {
    /**
     * WHAT: Get all channel values
     */
    if (!cn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_correlated_get_all: cn is NULL");
        return -1;
    }
    if (!values) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_correlated_get_all: values is NULL");
        return -1;
    }

    memcpy(values, cn->current_values, cn->config.num_channels * sizeof(float));
    return 0;
}

float pink_noise_correlated_get_named(
    const pink_noise_correlated_t* cn,
    const char* name
) {
    /**
     * WHAT: Get channel by name
     * WHY:  Convenient access for neuromodulators
     */
    if (!cn || !name) return 0.0f;

    for (uint32_t i = 0; i < cn->config.num_channels; i++) {
        if (cn->config.channels[i].name &&
            strcmp(cn->config.channels[i].name, name) == 0) {
            return cn->current_values[i];
        }
    }

    return 0.0f;
}

int pink_noise_correlated_generate_batch(
    pink_noise_correlated_t* cn,
    float** outputs,
    uint32_t num_samples
) {
    /**
     * WHAT: Generate batch of correlated samples
     */
    if (!cn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_correlated_generate_batch: cn is NULL");
        return -1;
    }
    if (!outputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_correlated_generate_batch: outputs is NULL");
        return -1;
    }
    if (num_samples == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAMETER, "pink_noise_correlated_generate_batch: num_samples is 0");
        return -1;
    }

    for (uint32_t s = 0; s < num_samples; s++) {
        int result = pink_noise_correlated_step(cn);
        if (result != 0) return result;

        for (uint32_t i = 0; i < cn->config.num_channels; i++) {
            if (outputs[i]) {
                outputs[i][s] = cn->current_values[i];
            }
        }
    }

    return 0;
}

//=============================================================================
// Correlation Control
//=============================================================================

int pink_noise_correlated_set_correlation(
    pink_noise_correlated_t* cn,
    uint32_t channel_i,
    uint32_t channel_j,
    float correlation
) {
    /**
     * WHAT: Update single correlation coefficient
     * WHY:  Dynamic correlation adjustment
     */
    if (!cn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_correlated_set_correlation: cn is NULL");
        return -1;
    }
    if (channel_i >= cn->config.num_channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAMETER, "pink_noise_correlated_set_correlation: invalid channel_i");
        return -1;
    }
    if (channel_j >= cn->config.num_channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAMETER, "pink_noise_correlated_set_correlation: invalid channel_j");
        return -1;
    }
    if (correlation < -1.0f || correlation > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAMETER, "pink_noise_correlated_set_correlation: correlation out of range");
        return -1;
    }

    uint32_t n = cn->config.num_channels;
    cn->config.correlation_matrix[channel_i * PINK_NOISE_MAX_CHANNELS + channel_j] = correlation;
    cn->config.correlation_matrix[channel_j * PINK_NOISE_MAX_CHANNELS + channel_i] = correlation;

    // Recompute Cholesky
    cn->cholesky_valid = compute_cholesky(
        cn->config.correlation_matrix,
        cn->cholesky_matrix,
        n
    );

    return cn->cholesky_valid ? 0 : -1;
}

int pink_noise_correlated_set_matrix(
    pink_noise_correlated_t* cn,
    const float* matrix
) {
    /**
     * WHAT: Set full correlation matrix
     */
    if (!cn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_correlated_set_matrix: cn is NULL");
        return -1;
    }
    if (!matrix) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_correlated_set_matrix: matrix is NULL");
        return -1;
    }

    uint32_t n = cn->config.num_channels;
    memcpy(cn->config.correlation_matrix, matrix, n * n * sizeof(float));

    cn->cholesky_valid = compute_cholesky(
        cn->config.correlation_matrix,
        cn->cholesky_matrix,
        n
    );

    return cn->cholesky_valid ? 0 : -1;
}

//=============================================================================
// Statistics and Reset
//=============================================================================

int pink_noise_correlated_get_stats(
    const pink_noise_correlated_t* cn,
    pink_noise_correlated_stats_t* stats
) {
    /**
     * WHAT: Get correlation statistics
     */
    if (!cn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_correlated_get_stats: cn is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_correlated_get_stats: stats is NULL");
        return -1;
    }

    memset(stats, 0, sizeof(pink_noise_correlated_stats_t));
    memcpy(stats->measured_correlations, cn->config.correlation_matrix,
           cn->config.num_channels * cn->config.num_channels * sizeof(float));
    stats->total_samples = cn->sample_count;

    return 0;
}

int pink_noise_correlated_reset(
    pink_noise_correlated_t* cn,
    uint32_t new_seed
) {
    /**
     * WHAT: Reset all channels
     */
    if (!cn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_correlated_reset: cn is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < cn->config.num_channels; i++) {
        uint32_t seed = (new_seed == 0) ? cn->config.seed + i : new_seed + i;
        if (!pink_noise_reset(cn->generators[i], seed)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pink_noise_correlated_reset: pink_noise_reset is NULL");
            return -1;
        }
        cn->current_values[i] = 0.0f;
        cn->white_buffer[i] = 0.0f;
    }

    cn->sample_count = 0;
    return 0;
}
