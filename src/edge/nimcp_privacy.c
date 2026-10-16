/**
 * @file nimcp_privacy.c
 * @brief Differential privacy — gradient privatization for federated learning
 *
 * WHAT: Clips gradient norms and adds calibrated Gaussian noise before
 *       sharing gradients with the master or peers.
 * WHY:  Prevents reconstruction of private training data from shared gradients.
 * HOW:  L2 norm clipping + Gaussian noise N(0, sigma^2 * clip_norm^2),
 *       with privacy budget tracking (simplified Gaussian mechanism).
 */

#include "edge/nimcp_edge.h"
#include "utils/memory/nimcp_memory.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>

/* ============================================================================
 * Box-Muller Transform for Gaussian Random Numbers
 * ============================================================================ */

static volatile char seeded = 0;

/**
 * @brief Generate a standard normal variate N(0,1) using Box-Muller.
 *
 * NOTE: The static seeding is a benign race — multiple threads may call
 * srand() simultaneously, but duplicate srand(time(NULL)) calls are
 * harmless (same seed within the same second). We use __atomic_test_and_set
 * to minimize redundant calls.
 */
static float gaussian_noise(void) {
    if (!__atomic_test_and_set(&seeded, __ATOMIC_RELAXED)) {
        srand((unsigned int)time(NULL));
    }

    float u1, u2;
    do {
        u1 = (float)rand() / (float)RAND_MAX;
    } while (u1 <= 1e-10f);
    u2 = (float)rand() / (float)RAND_MAX;

    return sqrtf(-2.0f * logf(u1)) * cosf(2.0f * (float)M_PI * u2);
}

/* ============================================================================
 * Init
 * ============================================================================ */

int nimcp_edge_dp_init(nimcp_edge_dp_config_t* config) {
    if (!config) {
        return -1;
    }

    config->noise_scale = 0.01f;
    config->gradient_clip_norm = 1.0f;
    config->privacy_budget_epsilon = 1.0f;
    config->privacy_spent = 0.0f;
    config->enabled = true;

    return 0;
}

/* ============================================================================
 * Gradient Privatization
 * ============================================================================ */

int nimcp_edge_dp_privatize_gradients(nimcp_edge_dp_config_t* config,
                                  float* gradients, uint32_t num_params) {
    if (!config || !gradients || num_params == 0) {
        return -1;
    }
    if (!config->enabled) {
        return 0;
    }

    float clip_norm = config->gradient_clip_norm;
    float sigma = config->noise_scale;

    /* Step 1: Compute gradient L2 norm */
    float norm_sq = 0.0f;
    for (uint32_t i = 0; i < num_params; i++) {
        norm_sq += gradients[i] * gradients[i];
    }
    float norm = sqrtf(norm_sq);

    /* Step 2: Clip if norm > clip_norm */
    if (norm > clip_norm && norm > 0.0f) {
        float scale = clip_norm / norm;
        for (uint32_t i = 0; i < num_params; i++) {
            gradients[i] *= scale;
        }
    }

    /* Step 3: Add Gaussian noise N(0, sigma^2 * clip_norm^2) */
    float noise_std = sigma * clip_norm;
    for (uint32_t i = 0; i < num_params; i++) {
        gradients[i] += noise_std * gaussian_noise();
    }

    /* Step 4: Track privacy spent (simplified Gaussian mechanism).
     * epsilon_spent += sqrt(2 * log(1.25 / delta)) * sigma
     * Using delta = 1e-5 as a standard choice. */
    static const float DELTA = 1e-5f;
    float log_term = logf(1.25f / DELTA);
    float epsilon_step = sqrtf(2.0f * log_term) * sigma;
    config->privacy_spent += epsilon_step;

    return 0;
}

/* ============================================================================
 * Budget Check
 * ============================================================================ */

bool nimcp_edge_dp_budget_exhausted(const nimcp_edge_dp_config_t* config) {
    if (!config) {
        return true; /* Fail safe: no config = no budget */
    }
    return config->privacy_spent >= config->privacy_budget_epsilon;
}
