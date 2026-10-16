/**
 * @file nimcp_kdtree.c
 * @brief K-D Tree implementation for 3D spatial indexing
 */

#include "utils/spatial/nimcp_kdtree.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(kdtree)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief KD-tree node
 */
struct kdtree_node_t {
    float point[3];           /**< 3D coordinates */
    void* user_data;          /**< Associated user data */
    kdtree_node_t* left;      /**< Left child (points with coord < split) */
    kdtree_node_t* right;     /**< Right child (points with coord >= split) */
    uint8_t split_dim;        /**< Split dimension (0=x, 1=y, 2=z) */
};

/**
 * @brief KD-tree structure
 */
struct kdtree_t {
    kdtree_node_t* root;      /**< Root node */
    uint32_t size;            /**< Number of points */
};

//=============================================================================
// Helper Structures for Building
//=============================================================================

typedef struct {
    float point[3];
    void* user_data;
} kdtree_build_point_t;

//=============================================================================
// Helper Functions
//=============================================================================

static inline float distance_squared(const float a[3], const float b[3]) {
    float dx = a[0] - b[0];
    float dy = a[1] - b[1];
    float dz = a[2] - b[2];
    return dx*dx + dy*dy + dz*dz;
}

static int compare_points_x(const void* a, const void* b) {
    const kdtree_build_point_t* pa = (const kdtree_build_point_t*)a;
    const kdtree_build_point_t* pb = (const kdtree_build_point_t*)b;
    if (pa->point[0] < pb->point[0]) return -1;
    if (pa->point[0] > pb->point[0]) return 1;
    return 0;
}

static int compare_points_y(const void* a, const void* b) {
    const kdtree_build_point_t* pa = (const kdtree_build_point_t*)a;
    const kdtree_build_point_t* pb = (const kdtree_build_point_t*)b;
    if (pa->point[1] < pb->point[1]) return -1;
    if (pa->point[1] > pb->point[1]) return 1;
    return 0;
}

static int compare_points_z(const void* a, const void* b) {
    const kdtree_build_point_t* pa = (const kdtree_build_point_t*)a;
    const kdtree_build_point_t* pb = (const kdtree_build_point_t*)b;
    if (pa->point[2] < pb->point[2]) return -1;
    if (pa->point[2] > pb->point[2]) return 1;
    return 0;
}

/**
 * @brief Recursively build KD-tree
 *
 * ALGORITHM: Median split along alternating dimensions
 */
static kdtree_node_t* build_recursive(kdtree_build_point_t* points,
                                       uint32_t count, uint8_t depth) {
    if (count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "compare_points_z: count is zero");
        return NULL;
    }

    // Determine split dimension (cycle through x, y, z)
    uint8_t dim = depth % 3;

    // Sort points by dimension
    switch (dim) {
        case 0: qsort(points, count, sizeof(kdtree_build_point_t), compare_points_x); break;
        case 1: qsort(points, count, sizeof(kdtree_build_point_t), compare_points_y); break;
        case 2: qsort(points, count, sizeof(kdtree_build_point_t), compare_points_z); break;
    }

    // Find median
    uint32_t median = count / 2;

    // Create node
    kdtree_node_t* node = (kdtree_node_t*)nimcp_calloc(1, sizeof(kdtree_node_t));
    if (!node) {
        LOG_ERROR("KDTREE", "Failed to allocate kdtree node");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(kdtree_node_t), "Failed to allocate kdtree node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");

        return NULL;
    }

    memcpy(node->point, points[median].point, sizeof(node->point));
    node->user_data = points[median].user_data;
    node->split_dim = dim;

    // Recursively build subtrees
    node->left = build_recursive(points, median, depth + 1);
    node->right = build_recursive(points + median + 1, count - median - 1, depth + 1);

    return node;
}

/**
 * @brief Destroy tree recursively
 */
static void destroy_recursive(kdtree_node_t* node) {
    if (!node) {
        return;
    }
    destroy_recursive(node->left);
    destroy_recursive(node->right);
    nimcp_free(node);
}

/**
 * @brief Recursive nearest neighbor search
 */
static void nearest_recursive(const kdtree_node_t* node, const float query[3],
                               kdtree_node_t** best, float* best_dist_sq) {
    if (!node) {
        return;
    }

    // Check current node distance
    float dist_sq = distance_squared(node->point, query);
    if (dist_sq < *best_dist_sq) {
        *best_dist_sq = dist_sq;
        *best = (kdtree_node_t*)node;
    }

    // Determine which subtree to search first
    uint8_t dim = node->split_dim;
    float diff = query[dim] - node->point[dim];

    kdtree_node_t* near_child = (diff < 0) ? node->left : node->right;
    kdtree_node_t* far_child = (diff < 0) ? node->right : node->left;

    // Search near subtree
    nearest_recursive(near_child, query, best, best_dist_sq);

    // Check if we need to search far subtree
    // (if splitting plane is within current best distance)
    if (diff * diff < *best_dist_sq) {
        nearest_recursive(far_child, query, best, best_dist_sq);
    }
}

/**
 * @brief Compute tree depth recursively
 */
static uint32_t depth_recursive(const kdtree_node_t* node) {
    if (!node) {
        return 0;
    }
    uint32_t left_depth = depth_recursive(node->left);
    uint32_t right_depth = depth_recursive(node->right);
    return 1 + (left_depth > right_depth ? left_depth : right_depth);
}

//=============================================================================
// Public API
//=============================================================================

kdtree_t* kdtree_create(void) {
    kdtree_t* tree = (kdtree_t*)nimcp_calloc(1, sizeof(kdtree_t));
    NIMCP_API_CHECK_ALLOC(tree, "Failed to allocate kdtree structure");
    return tree;
}

void kdtree_destroy(kdtree_t* tree) {
    if (!tree) {
        return;
    }
    destroy_recursive(tree->root);
    nimcp_free(tree);
}

bool kdtree_build(kdtree_t* tree, const kdtree_point_t* points,
                  void** user_data, uint32_t count) {
    if (!tree || !points || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kdtree_destroy: required parameter is NULL (tree, points)");
        return false;
    }

    // Clear existing tree
    kdtree_clear(tree);

    // Create temporary array for building
    kdtree_build_point_t* build_points = (kdtree_build_point_t*)
        nimcp_malloc(count * sizeof(kdtree_build_point_t));
    if (!build_points) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kdtree_destroy: build_points is NULL");
        return false;
    }

    // Copy points and user data
    for (uint32_t i = 0; i < count; i++) {
        memcpy(build_points[i].point, points[i], sizeof(kdtree_point_t));
        build_points[i].user_data = user_data ? user_data[i] : NULL;
    }

    // Build tree recursively
    tree->root = build_recursive(build_points, count, 0);
    tree->size = count;

    nimcp_free(build_points);
    return (tree->root != NULL);
}

void kdtree_clear(kdtree_t* tree) {
    if (!tree) {
        return;
    }
    destroy_recursive(tree->root);
    tree->root = NULL;
    tree->size = 0;
}

void* kdtree_nearest(const kdtree_t* tree, const kdtree_point_t query,
                     float* dist_sq) {
    if (!tree || !tree->root) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kdtree_clear: required parameter is NULL (tree, tree->root)");
        return NULL;
    }

    kdtree_node_t* best = tree->root;
    float best_dist_sq = distance_squared(tree->root->point, query);

    nearest_recursive(tree->root, query, &best, &best_dist_sq);

    if (dist_sq) {
        *dist_sq = best_dist_sq;
    }

    return best->user_data;
}

/**
 * @brief Max-heap element for k-NN search
 *
 * WHAT: Stores candidate neighbor with distance
 * WHY:  Max-heap allows O(log k) pruning of furthest neighbor
 * HOW:  Store distance (key) and user_data pointer (value)
 */
typedef struct {
    float dist_sq;      /**< Squared distance (heap key) */
    void* user_data;    /**< Associated user data pointer */
} knn_heap_element_t;

/**
 * @brief Max-heap context for k-NN search
 */
typedef struct {
    knn_heap_element_t* elements;  /**< Heap array (1-indexed) */
    uint32_t size;                 /**< Current number of elements */
    uint32_t capacity;             /**< Maximum capacity (k) */
} knn_max_heap_t;

/**
 * @brief Initialize max-heap for k-NN search
 */
static void knn_heap_init(knn_max_heap_t* heap, knn_heap_element_t* buffer, uint32_t capacity) {
    heap->elements = buffer - 1;  /* 1-indexed */
    heap->size = 0;
    heap->capacity = capacity;
}

/**
 * @brief Sift up to restore heap property after insert
 */
static void knn_heap_sift_up(knn_max_heap_t* heap, uint32_t idx) {
    while (idx > 1) {
        uint32_t parent = idx / 2;
        if (heap->elements[parent].dist_sq < heap->elements[idx].dist_sq) {
            knn_heap_element_t tmp = heap->elements[parent];
            heap->elements[parent] = heap->elements[idx];
            heap->elements[idx] = tmp;
            idx = parent;
        } else {
            break;
        }
    }
}

/**
 * @brief Sift down to restore heap property after replace
 */
static void knn_heap_sift_down(knn_max_heap_t* heap, uint32_t idx) {
    while (2 * idx <= heap->size) {
        uint32_t child = 2 * idx;
        if (child + 1 <= heap->size &&
            heap->elements[child + 1].dist_sq > heap->elements[child].dist_sq) {
            child++;
        }
        if (heap->elements[child].dist_sq > heap->elements[idx].dist_sq) {
            knn_heap_element_t tmp = heap->elements[child];
            heap->elements[child] = heap->elements[idx];
            heap->elements[idx] = tmp;
            idx = child;
        } else {
            break;
        }
    }
}

/**
 * @brief Insert element into max-heap (or replace max if full and smaller)
 * @return true if element was added/replaced, false if rejected
 */
static bool knn_heap_insert(knn_max_heap_t* heap, float dist_sq, void* user_data) {
    if (heap->size < heap->capacity) {
        /* Heap not full - insert */
        heap->size++;
        heap->elements[heap->size].dist_sq = dist_sq;
        heap->elements[heap->size].user_data = user_data;
        knn_heap_sift_up(heap, heap->size);
        return true;
    } else if (dist_sq < heap->elements[1].dist_sq) {
        /* Heap full but this is closer than max - replace max */
        heap->elements[1].dist_sq = dist_sq;
        heap->elements[1].user_data = user_data;
        knn_heap_sift_down(heap, 1);
        return true;
    }
    return false;
}

/**
 * @brief Get maximum distance in heap (furthest neighbor)
 */
static float knn_heap_max_dist(const knn_max_heap_t* heap) {
    if (heap->size == 0) return INFINITY;
    return heap->elements[1].dist_sq;
}

/**
 * @brief Recursive k-NN search with max-heap pruning
 */
static void knn_recursive(const kdtree_node_t* node, const kdtree_point_t query,
                          knn_max_heap_t* heap) {
    if (!node) return;

    /* Check current node */
    float dist_sq = distance_squared(node->point, query);
    knn_heap_insert(heap, dist_sq, node->user_data);

    /* Determine which subtree to search first */
    uint8_t dim = node->split_dim;
    float diff = query[dim] - node->point[dim];

    kdtree_node_t* near_child = (diff < 0) ? node->left : node->right;
    kdtree_node_t* far_child = (diff < 0) ? node->right : node->left;

    /* Search near subtree */
    knn_recursive(near_child, query, heap);

    /* Check if we need to search far subtree
     * Only if splitting plane is closer than furthest k-th neighbor */
    if (diff * diff < knn_heap_max_dist(heap) || heap->size < heap->capacity) {
        knn_recursive(far_child, query, heap);
    }
}

uint32_t kdtree_k_nearest(const kdtree_t* tree, const kdtree_point_t query,
                          uint32_t k, void** results, float* distances) {
    if (!tree || !tree->root || !results || k == 0) {
        return 0;
    }

    /* Allocate heap buffer on stack for small k, heap for large k */
    knn_heap_element_t stack_buffer[64];
    knn_heap_element_t* heap_buffer = NULL;

    if (k <= 64) {
        heap_buffer = stack_buffer;
    } else {
        heap_buffer = (knn_heap_element_t*)nimcp_malloc(k * sizeof(knn_heap_element_t));
        if (!heap_buffer) {
            return 0;
        }
    }

    /* Initialize max-heap */
    knn_max_heap_t heap;
    knn_heap_init(&heap, heap_buffer, k);

    /* Perform k-NN search */
    knn_recursive(tree->root, query, &heap);

    /* Extract results from heap (currently in arbitrary order) */
    /* Sort by distance for consistent output */
    uint32_t count = heap.size;

    /* Simple insertion sort for small k */
    for (uint32_t i = 0; i < count; i++) {
        results[i] = heap_buffer[i].user_data;
        if (distances) {
            distances[i] = heap_buffer[i].dist_sq;
        }
    }

    /* Sort results by distance (ascending) */
    for (uint32_t i = 1; i < count; i++) {
        void* key_data = results[i];
        float key_dist = distances ? distances[i] : 0;
        int32_t j = (int32_t)i - 1;

        while (j >= 0 && distances && distances[j] > key_dist) {
            results[j + 1] = results[j];
            distances[j + 1] = distances[j];
            j--;
        }
        results[j + 1] = key_data;
        if (distances) {
            distances[j + 1] = key_dist;
        }
    }

    /* Cleanup if heap-allocated */
    if (heap_buffer != stack_buffer) {
        nimcp_free(heap_buffer);
    }

    return count;
}

/**
 * @brief Helper structure for range search results
 *
 * WHAT: Accumulator for points within search radius
 * WHY:  Need to track count and capacity during recursive traversal
 * HOW:  Pass by pointer through recursive calls
 */
typedef struct {
    void** results;      /**< Output array of user data pointers */
    uint32_t count;      /**< Current number of results found */
    uint32_t capacity;   /**< Maximum capacity of results array */
} range_search_context_t;

/**
 * @brief Recursively search for points within radius
 *
 * WHAT: Traverse KD-tree and collect all points within sphere
 * WHY:  Bulk spatial queries for astrocyte networks, connection formation
 * HOW:  Recursive DFS with bounding box pruning
 *
 * ALGORITHM:
 * 1. Test current node: if within radius, add to results
 * 2. Compute distance to splitting plane
 * 3. Recursively search near subtree
 * 4. If splitting plane intersects sphere, search far subtree too
 * 5. Pruning: Skip subtree if splitting plane beyond radius
 *
 * COMPLEXITY: O(k + sqrt(N)) average, where k = results count
 * BIOLOGICAL ANALOGY: Find all astrocytes within diffusion radius of synapse
 *
 * @param node Current node (NULL-safe)
 * @param query Query point [x, y, z]
 * @param radius_sq Squared search radius (for faster comparison)
 * @param ctx Context with results array and capacity
 */
static void range_search_recursive(const kdtree_node_t* node,
                                   const kdtree_point_t query,
                                   float radius_sq,
                                   range_search_context_t* ctx) {
    // Base case: null node
    if (!node) {
        return;
    }

    // Early exit: capacity reached
    if (ctx->count >= ctx->capacity) {
        return;
    }

    // STEP 1: Test current node - is it within radius?
    float dist_sq = distance_squared(node->point, query);
    if (dist_sq <= radius_sq) {
        // Point is within sphere - add to results
        ctx->results[ctx->count++] = node->user_data;

        // Early exit if capacity reached
        if (ctx->count >= ctx->capacity) {
            return;
        }
    }

    // STEP 2: Determine which subtree to search first
    // Get split dimension for this node
    uint8_t dim = node->split_dim;

    // Distance from query to splitting plane along split dimension
    float plane_dist = query[dim] - node->point[dim];

    // Determine near/far children based on which side of plane query is on
    kdtree_node_t* near_child = (plane_dist < 0) ? node->left : node->right;
    kdtree_node_t* far_child = (plane_dist < 0) ? node->right : node->left;

    // STEP 3: Always search near subtree
    range_search_recursive(near_child, query, radius_sq, ctx);

    // Early exit if capacity reached
    if (ctx->count >= ctx->capacity) {
        return;
    }

    // STEP 4: Conditionally search far subtree (bounding box pruning)
    // Only search if splitting plane intersects search sphere
    // plane_dist^2 < radius^2 means sphere crosses plane
    if (plane_dist * plane_dist <= radius_sq) {
        range_search_recursive(far_child, query, radius_sq, ctx);
    }
}

uint32_t kdtree_range_search(const kdtree_t* tree, const kdtree_point_t query,
                              float radius, void** results, uint32_t capacity) {
    // WHAT: Find all points within radius of query point
    // WHY:  Bulk spatial queries for synapse-astrocyte matching, connection formation
    // HOW:  Recursive DFS with bounding box pruning

    // Input validation
    if (!tree || !tree->root || !results || capacity == 0) {
        return 0;
    }

    // Validate radius (must be non-negative)
    if (radius < 0.0F) {
        return 0;
    }

    // Initialize search context
    range_search_context_t ctx = {
        .results = results,
        .count = 0,
        .capacity = capacity
    };

    // Compute squared radius once (avoid repeated sqrt in recursion)
    float radius_sq = radius * radius;

    // Perform recursive range search
    range_search_recursive(tree->root, query, radius_sq, &ctx);

    return ctx.count;
}

uint32_t kdtree_size(const kdtree_t* tree) {
    return tree ? tree->size : 0;
}

uint32_t kdtree_depth(const kdtree_t* tree) {
    if (!tree || !tree->root) {
        return 0;
    }
    return depth_recursive(tree->root);
}
