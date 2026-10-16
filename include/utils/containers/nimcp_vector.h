/**
 * @file nimcp_vector.h
 * @brief Vector mathematics utilities for NIMCP
 *
 * WHAT: Fast vector operations for neural network computations
 * WHY: Centralize common vector math to eliminate code duplication
 * HOW: Optimized implementations of dot products, norms, distances
 *
 * USAGE:
 *   float similarity = nimcp_vector_cosine_similarity(vec_a, vec_b, size);
 *   float dist = nimcp_vector_cosine_distance(vec_a, vec_b, size);
 *   float norm = nimcp_vector_norm_l2(vec, size);
 *
 * PERFORMANCE:
 *   - O(n) complexity for all operations
 *   - Single-pass algorithms where possible
 *   - Numerical stability with epsilon guards
 */

#ifndef NIMCP_VECTOR_H
#define NIMCP_VECTOR_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/**
 * WHAT: Epsilon for numerical stability
 * WHY: Prevent division by zero in normalizations
 */
#define NIMCP_VECTOR_EPSILON 1e-10f

//=============================================================================
// Basic Vector Operations
//=============================================================================

/**
 * WHAT: Compute dot product of two vectors
 * WHY: Foundation for many vector similarity metrics
 * HOW: sum(a[i] * b[i]) for all i
 *
 * @param a First vector
 * @param b Second vector
 * @param size Number of elements in vectors
 * @return Dot product value
 *
 * COMPLEXITY: O(n)
 */
float nimcp_vector_dot_product(const float* a, const float* b, uint32_t size);

/**
 * WHAT: Compute L2 (Euclidean) norm of a vector
 * WHY: Needed for normalization and distance calculations
 * HOW: sqrt(sum(vec[i]^2) for all i)
 *
 * @param vec Vector
 * @param size Number of elements
 * @return L2 norm (magnitude) of vector
 *
 * COMPLEXITY: O(n)
 */
float nimcp_vector_norm_l2(const float* vec, uint32_t size);

/**
 * WHAT: Compute L1 (Manhattan) norm of a vector
 * WHY: Sum of absolute values, useful for some normalizations
 * HOW: sum(|vec[i]|) for all i
 *
 * @param vec Vector
 * @param size Number of elements
 * @return L1 norm of vector
 *
 * COMPLEXITY: O(n)
 */
float nimcp_vector_norm_l1(const float* vec, uint32_t size);

/**
 * WHAT: Copy vector from source to destination
 * WHY: Simple wrapper with bounds checking
 * HOW: memcpy with float type safety
 *
 * @param src Source vector
 * @param dst Destination vector
 * @param size Number of elements to copy
 *
 * COMPLEXITY: O(n)
 */
void nimcp_vector_copy(const float* src, float* dst, uint32_t size);

//=============================================================================
// Similarity and Distance Metrics
//=============================================================================

/**
 * WHAT: Compute cosine similarity between two vectors
 * WHY: Scale-invariant similarity metric for comparing neural patterns
 * HOW: dot(a, b) / (norm(a) * norm(b))
 *
 * @param a First vector
 * @param b Second vector
 * @param size Number of elements
 * @return Cosine similarity in range [-1, 1]
 *         1.0 = identical direction
 *         0.0 = orthogonal
 *        -1.0 = opposite direction
 *         Special cases:
 *         - Both vectors zero: returns 1.0 (perfect match)
 *         - One vector zero: returns 0.0 (no similarity)
 *
 * COMPLEXITY: O(n)
 * NOTE: Numerically stable with epsilon guard
 */
float nimcp_vector_cosine_similarity(const float* a, const float* b, uint32_t size);

/**
 * WHAT: Compute cosine distance between two vectors
 * WHY: Distance metric derived from cosine similarity
 * HOW: 1.0 - cosine_similarity(a, b)
 *
 * @param a First vector
 * @param b Second vector
 * @param size Number of elements
 * @return Cosine distance in range [0, 2]
 *         0.0 = identical direction
 *         1.0 = orthogonal
 *         2.0 = opposite direction
 *
 * COMPLEXITY: O(n)
 */
float nimcp_vector_cosine_distance(const float* a, const float* b, uint32_t size);

/**
 * WHAT: Compute Euclidean distance between two vectors
 * WHY: Standard distance metric
 * HOW: sqrt(sum((a[i] - b[i])^2) for all i)
 *
 * @param a First vector
 * @param b Second vector
 * @param size Number of elements
 * @return Euclidean distance
 *
 * COMPLEXITY: O(n)
 */
float nimcp_vector_euclidean_distance(const float* a, const float* b, uint32_t size);

//=============================================================================
// Normalization Functions
//=============================================================================

/**
 * WHAT: Normalize vector to L2 (Euclidean) norm
 * WHY: Scale vector to unit length
 * HOW: vec[i] = vec[i] / norm_l2(vec) * target_norm
 *
 * @param vec Vector to normalize (modified in place)
 * @param size Number of elements
 * @param target_norm Desired norm (typically 1.0 for unit vector)
 * @return Actual norm before normalization (0.0 if vector was zero)
 *
 * COMPLEXITY: O(n)
 * NOTE: No-op if vector norm is below epsilon
 */
float nimcp_vector_normalize_l2(float* vec, uint32_t size, float target_norm);

/**
 * WHAT: Normalize vector to L1 (Manhattan) norm
 * WHY: Scale vector so sum of absolute values equals target
 * HOW: vec[i] = vec[i] / norm_l1(vec) * target_norm
 *
 * @param vec Vector to normalize (modified in place)
 * @param size Number of elements
 * @param target_norm Desired L1 norm
 * @return Actual norm before normalization (0.0 if vector was zero)
 *
 * COMPLEXITY: O(n)
 * NOTE: No-op if vector norm is below epsilon
 */
float nimcp_vector_normalize_l1(float* vec, uint32_t size, float target_norm);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VECTOR_H */
