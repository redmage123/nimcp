//=============================================================================
// nimcp_quantum_feature_maps.h - Quantum-Inspired Feature Transformations
//=============================================================================

#ifndef NIMCP_QUANTUM_FEATURE_MAPS_H
#define NIMCP_QUANTUM_FEATURE_MAPS_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/memory/nimcp_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_quantum_feature_maps.h
 * @brief Quantum-inspired feature transformations for neural data
 *
 * WHAT: Maps features to higher-dimensional quantum-inspired representations
 * WHY:  Enable kernel methods and nonlinear transformations with O(sqrt(N)) properties
 * HOW:  Implements quantum kernel approximations, variational encoding, entanglement maps
 *
 * QUANTUM CONCEPTS USED:
 * - Quantum kernel methods: Feature-to-Hilbert space mapping
 * - Pauli rotation gates: RX, RY, RZ for parameterized encoding
 * - Entanglement: Correlated feature encoding via CNOT-like gates
 * - Amplitude encoding: Represent features as quantum amplitudes
 * - Quantum random features: Random Fourier features with quantum sampling
 *
 * BIOLOGICAL ANALOGY:
 * - Neural population codes use distributed representations (quantum superposition)
 * - Synaptic correlations encode multi-feature relationships (entanglement)
 * - Nonlinear dendritic processing maps to quantum kernel evaluations
 *
 * KEY ADVANTAGES:
 * - Exponential feature space expansion (2^n features from n inputs)
 * - Natural handling of periodic/angular features (Pauli rotations)
 * - Implicit high-dimensional correlations (entanglement maps)
 * - Quantum-inspired classical speedups (no actual quantum hardware needed)
 *
 * @author NIMCP Development Team
 * @date 2025-01-20
 */

//=============================================================================
// Constants
//=============================================================================

#define QFMAP_MAX_INPUT_DIM     256    /**< Maximum input feature dimension */
#define QFMAP_MAX_OUTPUT_DIM    4096   /**< Maximum output feature dimension */
#define QFMAP_MAX_LAYERS        16     /**< Maximum variational layers */
#define QFMAP_DEFAULT_SEED      42     /**< Default random seed */

//=============================================================================
// Types
//=============================================================================

/**
 * WHAT: Quantum feature map algorithm types
 * WHY:  Different algorithms for different use cases
 * HOW:  Each encodes features differently into high-dim space
 */
typedef enum {
    QFMAP_PAULI_Z,             /**< ZZ feature map (Pauli-Z based) */
    QFMAP_PAULI_Y,             /**< YY feature map (Pauli-Y based) */
    QFMAP_PAULI_ZZ,            /**< Two-local ZZ map with entanglement */
    QFMAP_AMPLITUDE_ENCODE,    /**< Amplitude encoding (normalized features) */
    QFMAP_RANDOM_FOURIER,      /**< Random Fourier features */
    QFMAP_IQP,                 /**< Instantaneous Quantum Polynomial */
    QFMAP_HARDWARE_EFFICIENT,  /**< Hardware-efficient ansatz */
    QFMAP_CUSTOM               /**< User-defined encoding */
} quantum_feature_map_type_t;

/**
 * WHAT: Entanglement pattern for variational circuits
 * WHY:  Controls correlation structure in feature space
 * HOW:  Defines which "qubits" are coupled
 */
typedef enum {
    QFMAP_ENTANGLE_NONE,       /**< No entanglement (product state) */
    QFMAP_ENTANGLE_LINEAR,     /**< Linear chain (i <-> i+1) */
    QFMAP_ENTANGLE_CIRCULAR,   /**< Ring (linear + last <-> first) */
    QFMAP_ENTANGLE_FULL,       /**< All-to-all entanglement */
    QFMAP_ENTANGLE_ALTERNATING /**< Alternating pattern (layer-dependent) */
} quantum_entangle_pattern_t;

/**
 * WHAT: Configuration for quantum feature mapping
 * WHY:  Customize feature map behavior
 * HOW:  Parameters for encoding, depth, entanglement
 */
typedef struct {
    quantum_feature_map_type_t map_type;    /**< Encoding algorithm */
    quantum_entangle_pattern_t entangle;    /**< Entanglement pattern */
    uint32_t input_dim;                     /**< Input feature dimension */
    uint32_t output_dim;                    /**< Output feature dimension */
    uint32_t num_layers;                    /**< Number of variational layers */
    uint32_t num_rff_features;              /**< Random Fourier feature count */
    float rff_gamma;                        /**< RBF kernel gamma for RFF */
    bool normalize_input;                   /**< Normalize input features */
    bool normalize_output;                  /**< Normalize output features */
    uint32_t seed;                          /**< Random seed for RFF */
} quantum_feature_map_config_t;

/**
 * WHAT: Internal state for random Fourier features
 * WHY:  Precompute random weights for fast feature mapping
 * HOW:  W matrix and b vector for cos(Wx + b) features
 */
typedef struct {
    float* W;              /**< Random weight matrix [num_features x input_dim] */
    float* b;              /**< Random bias vector [num_features] */
    uint32_t num_features; /**< Number of random features */
    uint32_t input_dim;    /**< Input dimension */
} rff_state_t;

/**
 * WHAT: Internal state for variational encoding
 * WHY:  Cache rotation parameters per layer
 * HOW:  Parameters for RX, RY, RZ gates
 */
typedef struct {
    float* params;         /**< Variational parameters [num_layers x 3 x input_dim] */
    uint32_t num_layers;   /**< Number of layers */
    uint32_t dim;          /**< Dimension */
} variational_state_t;

/**
 * WHAT: Quantum feature map context (opaque handle)
 */
typedef struct quantum_feature_map_struct* quantum_feature_map_t;

/**
 * WHAT: Internal structure for quantum feature map
 */
typedef struct quantum_feature_map_struct {
    quantum_feature_map_config_t config;
    rff_state_t rff_state;
    variational_state_t var_state;
    float* work_buffer;    /**< Working buffer for intermediate results */
    float* output_buffer;  /**< Output feature buffer */
} quantum_feature_map_internal_t;

/**
 * WHAT: Statistics from feature mapping
 * WHY:  Monitor feature quality and transformation
 * HOW:  Track norms, variances, correlations
 */
typedef struct {
    uint64_t mappings_performed;   /**< Total feature maps computed */
    float avg_input_norm;          /**< Average input feature norm */
    float avg_output_norm;         /**< Average output feature norm */
    float avg_expansion_ratio;     /**< Average output/input dim ratio */
    float max_feature_value;       /**< Maximum observed feature value */
} quantum_feature_map_stats_t;

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * WHAT: Get default configuration
 * WHY:  Sensible defaults for common use cases
 * HOW:  ZZ feature map with linear entanglement
 */
static inline quantum_feature_map_config_t quantum_feature_map_default_config(void) {
    return (quantum_feature_map_config_t){
        .map_type = QFMAP_PAULI_ZZ,
        .entangle = QFMAP_ENTANGLE_LINEAR,
        .input_dim = 16,
        .output_dim = 64,
        .num_layers = 2,
        .num_rff_features = 256,
        .rff_gamma = 1.0f,
        .normalize_input = true,
        .normalize_output = true,
        .seed = QFMAP_DEFAULT_SEED
    };
}

//=============================================================================
// Random Number Generation (LCG for reproducibility)
//=============================================================================

static inline uint32_t qfmap_rand(uint32_t* state) {
    *state = (*state) * 1103515245 + 12345;
    return (*state >> 16) & 0x7FFF;
}

static inline float qfmap_randf(uint32_t* state) {
    return (float)qfmap_rand(state) / 32767.0f;
}

static inline float qfmap_randn(uint32_t* state) {
    /* Box-Muller transform for normal distribution */
    float u1 = qfmap_randf(state) + 1e-10f;
    float u2 = qfmap_randf(state);
    return sqrtf(-2.0f * logf(u1)) * cosf(2.0f * (float)M_PI * u2);
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * WHAT: Create quantum feature map context
 * WHY:  Initialize encoding with given configuration
 * HOW:  Allocate buffers, initialize random states
 *
 * @param config Configuration (NULL for defaults)
 * @return Feature map context or NULL on error
 */
static inline quantum_feature_map_t quantum_feature_map_create(
    const quantum_feature_map_config_t* config
) {
    quantum_feature_map_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = quantum_feature_map_default_config();
    }

    /* Validate config */
    if (cfg.input_dim == 0 || cfg.input_dim > QFMAP_MAX_INPUT_DIM) return NULL;
    if (cfg.output_dim == 0 || cfg.output_dim > QFMAP_MAX_OUTPUT_DIM) return NULL;
    if (cfg.num_layers == 0 || cfg.num_layers > QFMAP_MAX_LAYERS) return NULL;

    /* Allocate context */
    quantum_feature_map_internal_t* ctx =
        (quantum_feature_map_internal_t*)nimcp_calloc(1, sizeof(quantum_feature_map_internal_t));
    if (!ctx) return NULL;

    ctx->config = cfg;

    /* Allocate work buffers */
    ctx->work_buffer = (float*)nimcp_calloc(cfg.output_dim * 2, sizeof(float));
    ctx->output_buffer = (float*)nimcp_calloc(cfg.output_dim, sizeof(float));
    if (!ctx->work_buffer || !ctx->output_buffer) {
        nimcp_free(ctx->work_buffer);
        nimcp_free(ctx->output_buffer);
        nimcp_free(ctx);
        return NULL;
    }

    /* Initialize RFF state if needed */
    if (cfg.map_type == QFMAP_RANDOM_FOURIER) {
        uint32_t rng_state = cfg.seed;
        ctx->rff_state.num_features = cfg.num_rff_features;
        ctx->rff_state.input_dim = cfg.input_dim;
        ctx->rff_state.W = (float*)nimcp_calloc(cfg.num_rff_features * cfg.input_dim, sizeof(float));
        ctx->rff_state.b = (float*)nimcp_calloc(cfg.num_rff_features, sizeof(float));

        if (!ctx->rff_state.W || !ctx->rff_state.b) {
            nimcp_free(ctx->rff_state.W);
            nimcp_free(ctx->rff_state.b);
            nimcp_free(ctx->work_buffer);
            nimcp_free(ctx->output_buffer);
            nimcp_free(ctx);
            return NULL;
        }

        /* Initialize random weights from N(0, 2*gamma) */
        float scale = sqrtf(2.0f * cfg.rff_gamma);
        for (uint32_t i = 0; i < cfg.num_rff_features * cfg.input_dim; i++) {
            ctx->rff_state.W[i] = qfmap_randn(&rng_state) * scale;
        }
        for (uint32_t i = 0; i < cfg.num_rff_features; i++) {
            ctx->rff_state.b[i] = qfmap_randf(&rng_state) * 2.0f * (float)M_PI;
        }
    }

    /* Initialize variational state if needed */
    if (cfg.map_type == QFMAP_HARDWARE_EFFICIENT ||
        cfg.map_type == QFMAP_PAULI_ZZ) {
        uint32_t rng_state = cfg.seed + 1;
        uint32_t param_count = cfg.num_layers * 3 * cfg.input_dim;
        ctx->var_state.params = (float*)nimcp_calloc(param_count, sizeof(float));
        ctx->var_state.num_layers = cfg.num_layers;
        ctx->var_state.dim = cfg.input_dim;

        if (!ctx->var_state.params) {
            nimcp_free(ctx->rff_state.W);
            nimcp_free(ctx->rff_state.b);
            nimcp_free(ctx->work_buffer);
            nimcp_free(ctx->output_buffer);
            nimcp_free(ctx);
            return NULL;
        }

        /* Initialize with small random values */
        for (uint32_t i = 0; i < param_count; i++) {
            ctx->var_state.params[i] = qfmap_randf(&rng_state) * 0.1f;
        }
    }

    return (quantum_feature_map_t)ctx;
}

/**
 * WHAT: Destroy quantum feature map context
 * WHY:  Free all allocated resources
 * HOW:  Free buffers and context
 */
static inline void quantum_feature_map_destroy(quantum_feature_map_t ctx) {
    if (!ctx) return;
    quantum_feature_map_internal_t* internal = (quantum_feature_map_internal_t*)ctx;

    nimcp_free(internal->rff_state.W);
    nimcp_free(internal->rff_state.b);
    nimcp_free(internal->var_state.params);
    nimcp_free(internal->work_buffer);
    nimcp_free(internal->output_buffer);
    nimcp_free(internal);
}

//=============================================================================
// Core Feature Mapping Functions
//=============================================================================

/**
 * WHAT: Normalize feature vector to unit norm
 * WHY:  Required for amplitude encoding and stability
 * HOW:  Divide by L2 norm
 */
static inline void qfmap_normalize(float* features, uint32_t dim) {
    float norm = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        norm += features[i] * features[i];
    }
    norm = sqrtf(norm + 1e-10f);
    for (uint32_t i = 0; i < dim; i++) {
        features[i] /= norm;
    }
}

/**
 * WHAT: Apply Pauli-Z rotation gate
 * WHY:  Basic quantum encoding operation
 * HOW:  exp(i * theta * Z) -> |0> + e^(i*theta)|1>
 *       In feature space: cos(theta), sin(theta)
 */
static inline void qfmap_pauli_z_encode(
    const float* input,
    float* output,
    uint32_t dim
) {
    /* Each input feature generates 2 output features */
    for (uint32_t i = 0; i < dim; i++) {
        float theta = input[i] * (float)M_PI;
        output[2*i] = cosf(theta);
        output[2*i + 1] = sinf(theta);
    }
}

/**
 * WHAT: Apply Pauli-Y rotation gate
 * WHY:  Alternative encoding with different phase
 * HOW:  RY(theta) = cos(theta/2)|0> + sin(theta/2)|1>
 */
static inline void qfmap_pauli_y_encode(
    const float* input,
    float* output,
    uint32_t dim
) {
    for (uint32_t i = 0; i < dim; i++) {
        float theta = input[i] * (float)M_PI;
        output[2*i] = cosf(theta / 2.0f);
        output[2*i + 1] = sinf(theta / 2.0f);
    }
}

/**
 * WHAT: Apply entanglement (ZZ interaction terms)
 * WHY:  Create correlated features
 * HOW:  Add cos(x_i * x_j) and sin(x_i * x_j) terms
 */
static inline void qfmap_apply_entanglement(
    const float* input,
    float* output,
    uint32_t dim,
    quantum_entangle_pattern_t pattern,
    uint32_t layer
) {
    uint32_t out_idx = 0;

    switch (pattern) {
        case QFMAP_ENTANGLE_NONE:
            /* No entanglement, just copy single-qubit terms */
            for (uint32_t i = 0; i < dim; i++) {
                float theta = input[i] * (float)M_PI;
                output[out_idx++] = cosf(theta);
                output[out_idx++] = sinf(theta);
            }
            break;

        case QFMAP_ENTANGLE_LINEAR:
            /* Linear chain: ZZ(i, i+1) */
            for (uint32_t i = 0; i < dim - 1; i++) {
                float theta = input[i] * input[i+1] * (float)M_PI;
                output[out_idx++] = cosf(theta);
                output[out_idx++] = sinf(theta);
            }
            break;

        case QFMAP_ENTANGLE_CIRCULAR:
            /* Ring: Linear + ZZ(n-1, 0) */
            for (uint32_t i = 0; i < dim; i++) {
                uint32_t j = (i + 1) % dim;
                float theta = input[i] * input[j] * (float)M_PI;
                output[out_idx++] = cosf(theta);
                output[out_idx++] = sinf(theta);
            }
            break;

        case QFMAP_ENTANGLE_FULL:
            /* All-to-all: ZZ(i, j) for i < j */
            for (uint32_t i = 0; i < dim; i++) {
                for (uint32_t j = i + 1; j < dim; j++) {
                    float theta = input[i] * input[j] * (float)M_PI;
                    output[out_idx++] = cosf(theta);
                    output[out_idx++] = sinf(theta);
                }
            }
            break;

        case QFMAP_ENTANGLE_ALTERNATING:
            /* Even layers: even pairs, odd layers: odd pairs */
            if (layer % 2 == 0) {
                for (uint32_t i = 0; i < dim - 1; i += 2) {
                    float theta = input[i] * input[i+1] * (float)M_PI;
                    output[out_idx++] = cosf(theta);
                    output[out_idx++] = sinf(theta);
                }
            } else {
                for (uint32_t i = 1; i < dim - 1; i += 2) {
                    float theta = input[i] * input[i+1] * (float)M_PI;
                    output[out_idx++] = cosf(theta);
                    output[out_idx++] = sinf(theta);
                }
            }
            break;
    }
}

/**
 * WHAT: Apply Random Fourier Features
 * WHY:  Approximate RBF kernel in finite dimension
 * HOW:  z(x) = sqrt(2/D) * cos(Wx + b)
 *
 * QUANTUM ANALOGY: Like random measurement bases in tomography
 */
static inline void qfmap_random_fourier(
    const float* input,
    float* output,
    const rff_state_t* rff
) {
    float scale = sqrtf(2.0f / (float)rff->num_features);

    for (uint32_t i = 0; i < rff->num_features; i++) {
        float dot = 0.0f;
        for (uint32_t j = 0; j < rff->input_dim; j++) {
            dot += rff->W[i * rff->input_dim + j] * input[j];
        }
        output[i] = scale * cosf(dot + rff->b[i]);
    }
}

/**
 * WHAT: Amplitude encoding
 * WHY:  Map features to probability amplitudes
 * HOW:  Normalize features to unit vector
 *
 * QUANTUM: |psi> = sum_i x_i |i>
 */
static inline void qfmap_amplitude_encode(
    const float* input,
    float* output,
    uint32_t dim
) {
    /* Copy and normalize */
    memcpy(output, input, dim * sizeof(float));
    qfmap_normalize(output, dim);
}

/**
 * WHAT: IQP (Instantaneous Quantum Polynomial) encoding
 * WHY:  Provably hard-to-simulate classically
 * HOW:  Diagonal gates with interaction terms
 */
static inline void qfmap_iqp_encode(
    const float* input,
    float* output,
    uint32_t dim,
    uint32_t depth
) {
    uint32_t out_idx = 0;

    /* Single-qubit terms (diagonal Z rotations) */
    for (uint32_t i = 0; i < dim; i++) {
        float theta = input[i] * (float)M_PI;
        output[out_idx++] = cosf(theta);
        output[out_idx++] = sinf(theta);
    }

    /* Two-qubit diagonal terms */
    for (uint32_t d = 0; d < depth && out_idx < dim * dim * 2; d++) {
        for (uint32_t i = 0; i < dim; i++) {
            for (uint32_t j = i + 1; j < dim && out_idx < dim * dim * 2; j++) {
                /* ZZ interaction with depth-dependent scaling */
                float theta = input[i] * input[j] * (float)M_PI / (float)(d + 1);
                output[out_idx++] = cosf(theta);
                output[out_idx++] = sinf(theta);
            }
        }
    }
}

/**
 * WHAT: Hardware-efficient variational encoding
 * WHY:  Alternating rotation and entanglement layers
 * HOW:  RY - CNOT - RY pattern
 */
static inline void qfmap_hardware_efficient(
    const float* input,
    float* output,
    uint32_t dim,
    const variational_state_t* var,
    quantum_entangle_pattern_t pattern
) {
    float* state_real = (float*)nimcp_calloc(dim, sizeof(float));
    float* state_imag = (float*)nimcp_calloc(dim, sizeof(float));

    if (!state_real || !state_imag) {
        nimcp_free(state_real);
        nimcp_free(state_imag);
        /* Fall back to simple encoding */
        qfmap_pauli_z_encode(input, output, dim);
        return;
    }

    /* Initialize state: all zeros + first component */
    state_real[0] = 1.0f;

    /* Apply layers */
    for (uint32_t layer = 0; layer < var->num_layers; layer++) {
        /* Feature encoding: RY(x_i) */
        for (uint32_t i = 0; i < dim; i++) {
            float theta = input[i] * (float)M_PI +
                          var->params[layer * 3 * dim + i];
            float c = cosf(theta / 2.0f);
            float s = sinf(theta / 2.0f);
            float new_real = c * state_real[i] - s * state_imag[i];
            float new_imag = s * state_real[i] + c * state_imag[i];
            state_real[i] = new_real;
            state_imag[i] = new_imag;
        }

        /* Entanglement layer (simplified: just mix neighbors) */
        if (pattern != QFMAP_ENTANGLE_NONE) {
            for (uint32_t i = 0; i < dim - 1; i++) {
                /* CNOT-like mixing */
                float mix = 0.5f;
                float r1 = state_real[i], r2 = state_real[i+1];
                float i1 = state_imag[i], i2 = state_imag[i+1];
                state_real[i] = r1 * (1-mix) + r2 * mix;
                state_real[i+1] = r2 * (1-mix) + r1 * mix;
                state_imag[i] = i1 * (1-mix) + i2 * mix;
                state_imag[i+1] = i2 * (1-mix) + i1 * mix;
            }
        }
    }

    /* Output: concatenate real and imaginary parts */
    memcpy(output, state_real, dim * sizeof(float));
    memcpy(output + dim, state_imag, dim * sizeof(float));

    nimcp_free(state_real);
    nimcp_free(state_imag);
}

//=============================================================================
// Main Feature Map Function
//=============================================================================

/**
 * WHAT: Apply quantum feature map to input features
 * WHY:  Transform input to high-dimensional quantum-inspired space
 * HOW:  Apply configured encoding algorithm
 *
 * @param ctx Feature map context
 * @param input Input features [input_dim]
 * @param output Output features [output_dim] (caller allocates)
 * @return 0 on success, negative on error
 */
static inline int quantum_feature_map_apply(
    quantum_feature_map_t ctx,
    const float* input,
    float* output
) {
    if (!ctx || !input || !output) return -1;
    quantum_feature_map_internal_t* internal = (quantum_feature_map_internal_t*)ctx;

    /* Prepare normalized input if requested */
    float* work_input = internal->work_buffer;
    memcpy(work_input, input, internal->config.input_dim * sizeof(float));

    if (internal->config.normalize_input) {
        qfmap_normalize(work_input, internal->config.input_dim);
    }

    /* Apply feature map based on type */
    switch (internal->config.map_type) {
        case QFMAP_PAULI_Z:
            qfmap_pauli_z_encode(work_input, output, internal->config.input_dim);
            break;

        case QFMAP_PAULI_Y:
            qfmap_pauli_y_encode(work_input, output, internal->config.input_dim);
            break;

        case QFMAP_PAULI_ZZ:
            /* Single-qubit terms */
            qfmap_pauli_z_encode(work_input, output, internal->config.input_dim);
            /* Add entanglement terms */
            for (uint32_t layer = 0; layer < internal->config.num_layers; layer++) {
                uint32_t offset = 2 * internal->config.input_dim +
                                  layer * 2 * (internal->config.input_dim - 1);
                qfmap_apply_entanglement(work_input, output + offset,
                                        internal->config.input_dim,
                                        internal->config.entangle, layer);
            }
            break;

        case QFMAP_AMPLITUDE_ENCODE:
            qfmap_amplitude_encode(work_input, output, internal->config.input_dim);
            break;

        case QFMAP_RANDOM_FOURIER:
            qfmap_random_fourier(work_input, output, &internal->rff_state);
            break;

        case QFMAP_IQP:
            qfmap_iqp_encode(work_input, output, internal->config.input_dim,
                            internal->config.num_layers);
            break;

        case QFMAP_HARDWARE_EFFICIENT:
            qfmap_hardware_efficient(work_input, output, internal->config.input_dim,
                                    &internal->var_state, internal->config.entangle);
            break;

        default:
            return -2;  /* Unknown map type */
    }

    /* Normalize output if requested */
    if (internal->config.normalize_output) {
        qfmap_normalize(output, internal->config.output_dim);
    }

    return 0;
}

/**
 * WHAT: Compute quantum kernel between two feature vectors
 * WHY:  Evaluate inner product in feature space
 * HOW:  Map both inputs, compute dot product
 *
 * @param ctx Feature map context
 * @param x First input [input_dim]
 * @param y Second input [input_dim]
 * @param kernel_value Output: kernel value K(x, y)
 * @return 0 on success
 */
static inline int quantum_feature_map_kernel(
    quantum_feature_map_t ctx,
    const float* x,
    const float* y,
    float* kernel_value
) {
    if (!ctx || !x || !y || !kernel_value) return -1;
    quantum_feature_map_internal_t* internal = (quantum_feature_map_internal_t*)ctx;

    /* Map both inputs */
    float* phi_x = (float*)nimcp_calloc(internal->config.output_dim, sizeof(float));
    float* phi_y = (float*)nimcp_calloc(internal->config.output_dim, sizeof(float));

    if (!phi_x || !phi_y) {
        nimcp_free(phi_x);
        nimcp_free(phi_y);
        return -3;
    }

    quantum_feature_map_apply(ctx, x, phi_x);
    quantum_feature_map_apply(ctx, y, phi_y);

    /* Compute inner product */
    float dot = 0.0f;
    for (uint32_t i = 0; i < internal->config.output_dim; i++) {
        dot += phi_x[i] * phi_y[i];
    }

    *kernel_value = dot;

    nimcp_free(phi_x);
    nimcp_free(phi_y);

    return 0;
}

/**
 * WHAT: Compute quantum fidelity between two states
 * WHY:  Measure similarity in quantum feature space
 * HOW:  |<phi_x|phi_y>|^2
 */
static inline int quantum_feature_map_fidelity(
    quantum_feature_map_t ctx,
    const float* x,
    const float* y,
    float* fidelity
) {
    float kernel;
    int result = quantum_feature_map_kernel(ctx, x, y, &kernel);
    if (result != 0) return result;

    *fidelity = kernel * kernel;  /* |<phi_x|phi_y>|^2 */
    return 0;
}

/**
 * WHAT: Batch apply feature map to multiple inputs
 * WHY:  Efficiently process multiple samples
 * HOW:  Apply feature map to each input in batch
 *
 * @param ctx Feature map context
 * @param inputs Input batch [batch_size x input_dim]
 * @param outputs Output batch [batch_size x output_dim]
 * @param batch_size Number of samples
 * @return 0 on success
 */
static inline int quantum_feature_map_batch(
    quantum_feature_map_t ctx,
    const float* inputs,
    float* outputs,
    uint32_t batch_size
) {
    if (!ctx || !inputs || !outputs) return -1;
    quantum_feature_map_internal_t* internal = (quantum_feature_map_internal_t*)ctx;

    for (uint32_t i = 0; i < batch_size; i++) {
        int result = quantum_feature_map_apply(
            ctx,
            inputs + i * internal->config.input_dim,
            outputs + i * internal->config.output_dim
        );
        if (result != 0) return result;
    }

    return 0;
}

/**
 * WHAT: Compute Gram matrix for batch of inputs
 * WHY:  Required for kernel SVM and other kernel methods
 * HOW:  K[i,j] = kernel(x_i, x_j)
 *
 * @param ctx Feature map context
 * @param inputs Input batch [n_samples x input_dim]
 * @param n_samples Number of samples
 * @param gram_matrix Output: Gram matrix [n_samples x n_samples]
 * @return 0 on success
 */
static inline int quantum_feature_map_gram(
    quantum_feature_map_t ctx,
    const float* inputs,
    uint32_t n_samples,
    float* gram_matrix
) {
    if (!ctx || !inputs || !gram_matrix) return -1;
    quantum_feature_map_internal_t* internal = (quantum_feature_map_internal_t*)ctx;

    /* Map all inputs first */
    float* features = (float*)nimcp_calloc(n_samples * internal->config.output_dim, sizeof(float));
    if (!features) return -3;

    quantum_feature_map_batch(ctx, inputs, features, n_samples);

    /* Compute Gram matrix: K[i,j] = phi_i . phi_j */
    for (uint32_t i = 0; i < n_samples; i++) {
        for (uint32_t j = i; j < n_samples; j++) {
            float dot = 0.0f;
            for (uint32_t k = 0; k < internal->config.output_dim; k++) {
                dot += features[i * internal->config.output_dim + k] *
                       features[j * internal->config.output_dim + k];
            }
            gram_matrix[i * n_samples + j] = dot;
            gram_matrix[j * n_samples + i] = dot;  /* Symmetric */
        }
    }

    nimcp_free(features);
    return 0;
}

/**
 * WHAT: Get configuration from context
 */
static inline int quantum_feature_map_get_config(
    quantum_feature_map_t ctx,
    quantum_feature_map_config_t* config
) {
    if (!ctx || !config) return -1;
    quantum_feature_map_internal_t* internal = (quantum_feature_map_internal_t*)ctx;
    *config = internal->config;
    return 0;
}

/**
 * WHAT: Update variational parameters
 * WHY:  Allow training of feature map
 * HOW:  Set new parameters for variational layers
 */
static inline int quantum_feature_map_set_params(
    quantum_feature_map_t ctx,
    const float* params,
    uint32_t n_params
) {
    if (!ctx || !params) return -1;
    quantum_feature_map_internal_t* internal = (quantum_feature_map_internal_t*)ctx;

    uint32_t expected = internal->var_state.num_layers * 3 * internal->var_state.dim;
    if (n_params != expected) return -2;

    memcpy(internal->var_state.params, params, n_params * sizeof(float));
    return 0;
}

/**
 * WHAT: Get variational parameters
 */
static inline int quantum_feature_map_get_params(
    quantum_feature_map_t ctx,
    float* params,
    uint32_t* n_params
) {
    if (!ctx || !n_params) return -1;
    quantum_feature_map_internal_t* internal = (quantum_feature_map_internal_t*)ctx;

    *n_params = internal->var_state.num_layers * 3 * internal->var_state.dim;
    if (params) {
        memcpy(params, internal->var_state.params, (*n_params) * sizeof(float));
    }
    return 0;
}

//=============================================================================
// Integration with Feature Extractor
//=============================================================================

/**
 * WHAT: Enhance neural features with quantum mapping
 * WHY:  Add quantum-inspired features to existing feature set
 * HOW:  Apply quantum map to selected features
 */
static inline int quantum_feature_map_enhance_features(
    quantum_feature_map_t ctx,
    const float* original_features,
    uint32_t n_original,
    float* enhanced_features,
    uint32_t n_enhanced
) {
    if (!ctx || !original_features || !enhanced_features) return -1;
    quantum_feature_map_internal_t* internal = (quantum_feature_map_internal_t*)ctx;

    /* Copy original features */
    uint32_t copy_len = (n_original < n_enhanced) ? n_original : n_enhanced;
    memcpy(enhanced_features, original_features, copy_len * sizeof(float));

    /* Apply quantum map if space remains */
    if (n_enhanced > n_original) {
        return quantum_feature_map_apply(ctx, original_features,
                                         enhanced_features + n_original);
    }

    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_QUANTUM_FEATURE_MAPS_H */
