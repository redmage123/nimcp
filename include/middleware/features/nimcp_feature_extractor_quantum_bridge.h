/**
 * @file nimcp_feature_extractor_quantum_bridge.h
 * @brief Quantum feature maps for enhanced feature extraction
 *
 * WHAT: Integrates quantum feature map algorithms with feature extractor
 * WHY:  Higher-dimensional feature representations for better classification
 * HOW:  Pauli rotations, RFF, amplitude encoding for kernel approximation
 *
 * BIOLOGICAL INSPIRATION:
 * - Population coding in sensory cortex
 * - Sparse distributed representations
 * - Kernel-like computations in dendritic trees
 */

#ifndef NIMCP_FEATURE_EXTRACTOR_QUANTUM_BRIDGE_H
#define NIMCP_FEATURE_EXTRACTOR_QUANTUM_BRIDGE_H

#include "middleware/features/nimcp_quantum_feature_maps.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Types
//=============================================================================

typedef struct feature_quantum_bridge feature_quantum_bridge_t;

typedef struct {
    bool enabled;
    quantum_feature_map_type_t map_type;
    uint32_t input_dim;
    uint32_t output_dim;
    float rff_gamma;  /* RBF kernel width */
    uint32_t n_layers;
} feature_quantum_config_t;

typedef struct {
    uint64_t quantum_transforms;
    uint64_t classical_fallbacks;
    float avg_output_norm;
    float avg_sparsity;
} feature_quantum_stats_t;

//=============================================================================
// API
//=============================================================================

feature_quantum_config_t feature_quantum_default_config(void);

feature_quantum_bridge_t* feature_quantum_bridge_create(
    const feature_quantum_config_t* config
);

void feature_quantum_bridge_destroy(feature_quantum_bridge_t* bridge);

bool feature_quantum_bridge_is_enabled(const feature_quantum_bridge_t* bridge);

void feature_quantum_bridge_set_enabled(feature_quantum_bridge_t* bridge, bool enabled);

/**
 * WHAT: Transform features using quantum feature map
 */
int feature_quantum_transform(
    feature_quantum_bridge_t* bridge,
    const float* input,
    float* output
);

/**
 * WHAT: Compute quantum kernel between two feature vectors
 */
int feature_quantum_kernel(
    feature_quantum_bridge_t* bridge,
    const float* x1,
    const float* x2,
    float* kernel_value
);

int feature_quantum_get_stats(
    const feature_quantum_bridge_t* bridge,
    feature_quantum_stats_t* stats
);

void feature_quantum_reset_stats(feature_quantum_bridge_t* bridge);

//=============================================================================
// Implementation
//=============================================================================

#ifdef NIMCP_FEATURE_QUANTUM_BRIDGE_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/memory/nimcp_memory.h"

struct feature_quantum_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    feature_quantum_config_t config;
    quantum_feature_map_t qfm;  /* Direct handle, not pointer-to-pointer */
    feature_quantum_stats_t stats;
};

feature_quantum_config_t feature_quantum_default_config(void) {
    return (feature_quantum_config_t){
        .enabled = true,
        .map_type = QFMAP_RANDOM_FOURIER,
        .input_dim = 64,
        .output_dim = 128,
        .rff_gamma = 1.0f,
        .n_layers = 2
    };
}

feature_quantum_bridge_t* feature_quantum_bridge_create(
    const feature_quantum_config_t* config
) {
    feature_quantum_bridge_t* bridge = (feature_quantum_bridge_t*)nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) return NULL;

    bridge->config = config ? *config : feature_quantum_default_config();

    quantum_feature_map_config_t qconfig = quantum_feature_map_default_config();
    qconfig.map_type = bridge->config.map_type;
    qconfig.input_dim = bridge->config.input_dim;
    qconfig.output_dim = bridge->config.output_dim;
    qconfig.rff_gamma = bridge->config.rff_gamma;
    qconfig.num_layers = bridge->config.n_layers;

    bridge->qfm = quantum_feature_map_create(&qconfig);
    if (!bridge->qfm) {
        nimcp_free(bridge);
        return NULL;
    }

    return bridge;
}

void feature_quantum_bridge_destroy(feature_quantum_bridge_t* bridge) {
    if (!bridge) return;
    if (bridge->qfm) quantum_feature_map_destroy(bridge->qfm);
    nimcp_free(bridge);
}

bool feature_quantum_bridge_is_enabled(const feature_quantum_bridge_t* bridge) {
    return bridge && bridge->config.enabled;
}

void feature_quantum_bridge_set_enabled(feature_quantum_bridge_t* bridge, bool enabled) {
    if (bridge) bridge->config.enabled = enabled;
}

int feature_quantum_transform(
    feature_quantum_bridge_t* bridge,
    const float* input,
    float* output
) {
    if (!bridge || !input || !output) return -1;

    if (!bridge->config.enabled) {
        bridge->stats.classical_fallbacks++;
        /* Passthrough - copy input_dim values */
        memcpy(output, input, bridge->config.input_dim * sizeof(float));
        return 0;
    }

    int status = quantum_feature_map_apply(bridge->qfm, input, output);
    if (status < 0) return status;

    bridge->stats.quantum_transforms++;

    /* Compute output norm */
    float norm = 0;
    for (uint32_t i = 0; i < bridge->config.output_dim; i++) {
        norm += output[i] * output[i];
    }
    norm = sqrtf(norm);

    bridge->stats.avg_output_norm =
        (bridge->stats.avg_output_norm * (bridge->stats.quantum_transforms - 1) + norm)
        / bridge->stats.quantum_transforms;

    return 0;
}

int feature_quantum_kernel(
    feature_quantum_bridge_t* bridge,
    const float* x1,
    const float* x2,
    float* kernel_value
) {
    if (!bridge || !x1 || !x2 || !kernel_value) return -1;
    return quantum_feature_map_kernel(bridge->qfm, x1, x2, kernel_value);
}

int feature_quantum_get_stats(
    const feature_quantum_bridge_t* bridge,
    feature_quantum_stats_t* stats
) {
    if (!bridge || !stats) return -1;
    *stats = bridge->stats;
    return 0;
}

void feature_quantum_reset_stats(feature_quantum_bridge_t* bridge) {
    if (bridge) memset(&bridge->stats, 0, sizeof(bridge->stats));
}

#endif // NIMCP_FEATURE_QUANTUM_BRIDGE_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif // NIMCP_FEATURE_EXTRACTOR_QUANTUM_BRIDGE_H
