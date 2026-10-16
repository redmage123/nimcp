/**
 * @file nimcp_kdtree.h
 * @brief K-D Tree for efficient spatial indexing and nearest neighbor search
 *
 * WHAT: K-dimensional tree data structure for 3D points
 * WHY:  Enable O(log N) nearest neighbor queries instead of O(N) linear search
 * HOW:  Binary space partitioning tree, alternating split dimensions
 *
 * ALGORITHM:
 * - Build: O(N log N) time, recursively partition points by median
 * - Search: O(log N) average case for nearest neighbor
 * - Memory: O(N) space for N points
 *
 * BIOLOGICAL USE CASE:
 * - Astrocyte network: Find nearest astrocyte to synapse location
 * - Neuron placement: Find nearby neurons for connection formation
 * - Spatial queries: Range search, k-nearest neighbors
 *
 * @author NIMCP Development Team
 * @date 2025-01-16
 */

#ifndef NIMCP_KDTREE_H
#define NIMCP_KDTREE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Types
//=============================================================================

/**
 * @brief KD-tree node (internal)
 *
 * DESIGN: Minimalist node structure for cache efficiency
 * - 64 bytes total (1 cache line on most systems)
 * - Left/right pointers for traversal
 * - User data pointer for associated object
 */
typedef struct kdtree_node_t kdtree_node_t;

/**
 * @brief KD-tree handle (opaque)
 */
typedef struct kdtree_t kdtree_t;

/**
 * @brief Point in 3D space
 */
typedef float kdtree_point_t[3];

//=============================================================================
// Creation & Destruction
//=============================================================================

/**
 * @brief Create empty KD-tree
 *
 * @return KD-tree handle or NULL on error
 */
kdtree_t* kdtree_create(void);

/**
 * @brief Destroy KD-tree and free all resources
 *
 * EFFECT: Frees tree nodes, does NOT free user data pointers
 *
 * @param tree KD-tree to destroy (NULL-safe)
 */
void kdtree_destroy(kdtree_t* tree);

//=============================================================================
// Building
//=============================================================================

/**
 * @brief Build KD-tree from array of points
 *
 * ALGORITHM:
 * 1. Sort points by median along dimension (x, y, z alternating)
 * 2. Recursively split left/right subtrees
 * 3. O(N log N) build time
 *
 * THREAD-SAFETY: Not thread-safe, must rebuild if data changes
 *
 * @param tree KD-tree
 * @param points Array of 3D points
 * @param user_data Array of user data pointers (parallel to points)
 * @param count Number of points
 * @return true on success
 */
bool kdtree_build(kdtree_t* tree, const kdtree_point_t* points,
                  void** user_data, uint32_t count);

/**
 * @brief Clear tree and reset to empty state
 *
 * @param tree KD-tree
 */
void kdtree_clear(kdtree_t* tree);

//=============================================================================
// Nearest Neighbor Search
//=============================================================================

/**
 * @brief Find nearest point to query location
 *
 * ALGORITHM: Depth-first search with backtracking
 * - O(log N) average case
 * - O(N) worst case (degenerate tree)
 *
 * @param tree KD-tree
 * @param query Query point [x, y, z]
 * @param dist_sq Output: squared distance to nearest point (can be NULL)
 * @return User data pointer of nearest point, or NULL if tree empty
 */
void* kdtree_nearest(const kdtree_t* tree, const kdtree_point_t query,
                     float* dist_sq);

/**
 * @brief Find k nearest neighbors
 *
 * @param tree KD-tree
 * @param query Query point [x, y, z]
 * @param k Number of neighbors to find
 * @param results Output array of user data pointers (size >= k)
 * @param distances Output array of squared distances (size >= k, can be NULL)
 * @return Number of neighbors found (may be less than k if tree has fewer points)
 */
uint32_t kdtree_k_nearest(const kdtree_t* tree, const kdtree_point_t query,
                          uint32_t k, void** results, float* distances);

//=============================================================================
// Range Search
//=============================================================================

/**
 * @brief Find all points within radius of query point
 *
 * @param tree KD-tree
 * @param query Query point [x, y, z]
 * @param radius Search radius
 * @param results Output array of user data pointers (size >= capacity)
 * @param capacity Maximum number of results
 * @return Number of points found
 */
uint32_t kdtree_range_search(const kdtree_t* tree, const kdtree_point_t query,
                              float radius, void** results, uint32_t capacity);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get number of points in tree
 *
 * @param tree KD-tree
 * @return Point count
 */
uint32_t kdtree_size(const kdtree_t* tree);

/**
 * @brief Get tree depth (for performance analysis)
 *
 * @param tree KD-tree
 * @return Maximum depth (0 = empty, 1 = root only)
 */
uint32_t kdtree_depth(const kdtree_t* tree);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_KDTREE_H
