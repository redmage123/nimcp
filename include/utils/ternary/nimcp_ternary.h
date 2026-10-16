//=============================================================================
// nimcp_ternary.h - Master Header for Ternary Logic Module
//=============================================================================
/**
 * @file nimcp_ternary.h
 * @brief Master header for NIMCP ternary (three-valued) logic system
 *
 * WHAT: Unified include for all ternary logic functionality
 * WHY:  Single include for ternary types, logic, storage, vectors, matrices
 * HOW:  Aggregates all ternary sub-headers
 *
 * BIOLOGICAL BASIS:
 * - Neural systems often operate with three states:
 *   - Inhibitory (-1): Suppressive, LTD, reject
 *   - Silent/Unknown (0): Subthreshold, neutral, abstain
 *   - Excitatory (+1): Activating, LTP, accept
 * - Ternary logic captures uncertainty and neutrality inherent in neural computation
 *
 * MEMORY EFFICIENCY:
 * | Encoding     | Trits/Byte | 1M trits | Use Case              |
 * |--------------|------------|----------|-----------------------|
 * | Unpacked     | 1          | 1 MB     | Fast random access    |
 * | 2-bit packed | 4          | 256 KB   | Balanced              |
 * | Base-243     | 5          | 200 KB   | Maximum compression   |
 *
 * USAGE:
 * ```c
 * #include "utils/ternary/nimcp_ternary.h"
 *
 * // Create ternary vector
 * trit_vector_t* weights = trit_vector_create(1000, TERNARY_PACK_BASE243);
 *
 * // Set some values
 * trit_vector_set(weights, 0, TRIT_POSITIVE);
 * trit_vector_set(weights, 1, TRIT_NEGATIVE);
 *
 * // Logic operations
 * trit_t result = trit_and(TRIT_POSITIVE, TRIT_UNKNOWN);  // Returns 0
 *
 * // Convert from float weights
 * float floats[3] = {0.8f, -0.2f, 0.0f};
 * trit_vector_t* quantized = trit_vector_from_floats(floats, 3, 0.3f, TERNARY_PACK_NONE);
 *
 * // Cleanup
 * trit_vector_destroy(weights);
 * trit_vector_destroy(quantized);
 * ```
 *
 * MODULE INTEGRATIONS:
 * - SNN: Ternary synaptic weights (20x memory savings)
 * - Ethics: FORBID/NEUTRAL/ALLOW decisions
 * - Swarm: DISAGREE/ABSTAIN/AGREE voting
 * - Plasticity: LTD/STABLE/LTP updates
 * - Attention: SUPPRESS/NEUTRAL/ENHANCE gating
 * - Reasoning: Three-valued logic with UNKNOWN state
 * - Emotion: NEGATIVE/NEUTRAL/POSITIVE valence
 *
 * @author NIMCP Development Team
 * @date 2025-12-21
 */

#ifndef NIMCP_TERNARY_H
#define NIMCP_TERNARY_H

/* Core type definitions */
#include "nimcp_ternary_types.h"

/* Logic operations (Kleene, Lukasiewicz) */
#include "nimcp_ternary_logic.h"

/* Packed storage (2-bit, base-243) */
#include "nimcp_ternary_storage.h"

/* Vector operations */
#include "nimcp_ternary_vector.h"

/* Matrix operations */
#include "nimcp_ternary_matrix.h"

/* N-dimensional tensor operations */
#include "nimcp_ternary_nd.h"

/* Type conversions (float, double, probabilistic) */
#include "nimcp_ternary_convert.h"

/* Tensor integration (requires nimcp_tensor.h) */
#include "nimcp_ternary_tensor.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Module Version
//=============================================================================

#define NIMCP_TERNARY_VERSION_MAJOR 1
#define NIMCP_TERNARY_VERSION_MINOR 0
#define NIMCP_TERNARY_VERSION_PATCH 0

/**
 * @brief Get ternary module version string
 * @return Version string "major.minor.patch"
 */
static inline const char* nimcp_ternary_version(void) {
    return "1.0.0";
}

//=============================================================================
// Module Information
//=============================================================================

/**
 * @brief Ternary module capabilities info
 */
typedef struct {
    bool has_simd;              /**< SIMD acceleration available */
    bool has_packed_base243;    /**< Base-243 packing supported */
    bool has_packed_2bit;       /**< 2-bit packing supported */
    size_t max_vector_size;     /**< Maximum vector size */
    size_t max_matrix_elements; /**< Maximum matrix elements */
} ternary_capabilities_t;

/**
 * @brief Get ternary module capabilities
 *
 * @param caps Output capabilities structure
 */
static inline void nimcp_ternary_get_capabilities(ternary_capabilities_t* caps) {
    if (!caps) return;

    caps->has_simd = false;  /* TODO: Add SIMD detection */
    caps->has_packed_base243 = true;
    caps->has_packed_2bit = true;
    caps->max_vector_size = (size_t)1 << 30;  /* 1 billion trits */
    caps->max_matrix_elements = (size_t)1 << 30;
}

//=============================================================================
// Convenience Macros
//=============================================================================

/**
 * @brief Quick trit array literal
 *
 * Usage: TRIT_ARRAY(+1, 0, -1, +1)
 */
#define TRIT_ARRAY(...) ((trit_t[]){__VA_ARGS__})

/**
 * @brief Count of trit array literal
 */
#define TRIT_COUNT(...) (sizeof(TRIT_ARRAY(__VA_ARGS__)) / sizeof(trit_t))

/**
 * @brief Create inline trit vector from values
 *
 * Note: Creates unpacked vector on stack, must copy for persistence
 */
#define TRIT_VEC_INLINE(name, ...) \
    trit_t name##_data[] = {__VA_ARGS__}; \
    size_t name##_len = sizeof(name##_data) / sizeof(trit_t)

//=============================================================================
// Debug Helpers
//=============================================================================

#ifdef NIMCP_DEBUG

#include <stdio.h>

/**
 * @brief Print trit vector to stdout
 *
 * @param vec Vector to print
 * @param label Optional label
 */
static inline void trit_vector_print(const trit_vector_t* vec, const char* label) {
    if (label) printf("%s: ", label);
    if (!vec || vec->magic != TERNARY_MAGIC) {
        printf("(null/invalid)\n");
        return;
    }

    printf("[");
    for (size_t i = 0; i < vec->length; i++) {
        if (i > 0) printf(", ");
        if (i > 10) {
            printf("...");
            break;
        }
        printf("%s", trit_to_string(trit_vector_get(vec, i)));
    }
    printf("] (len=%zu, mode=%d)\n", vec->length, vec->pack_mode);
}

/**
 * @brief Print trit matrix to stdout
 *
 * @param mat Matrix to print
 * @param label Optional label
 */
static inline void trit_matrix_print(const trit_matrix_t* mat, const char* label) {
    if (label) printf("%s:\n", label);
    if (!mat || mat->magic != TERNARY_MAGIC) {
        printf("(null/invalid)\n");
        return;
    }

    printf("Matrix %zux%zu (mode=%d):\n", mat->rows, mat->cols, mat->pack_mode);
    for (size_t row = 0; row < mat->rows && row < 5; row++) {
        printf("  [");
        for (size_t col = 0; col < mat->cols && col < 10; col++) {
            if (col > 0) printf(", ");
            printf("%2d", trit_matrix_get(mat, row, col));
        }
        if (mat->cols > 10) printf(", ...");
        printf("]\n");
    }
    if (mat->rows > 5) printf("  ...\n");
}

#endif /* NIMCP_DEBUG */

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TERNARY_H */
