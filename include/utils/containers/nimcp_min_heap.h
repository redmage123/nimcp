/**
 * @file nimcp_min_heap.h
 * @brief Binary min-heap for O(log n) priority queue operations
 *
 * This module implements a binary min-heap data structure optimized for
 * Dijkstra's shortest path algorithm. The heap supports:
 * - O(log n) insertion
 * - O(log n) extract-minimum
 * - O(log n) decrease-key (with vertex index tracking)
 *
 * DESIGN PATTERN: Abstract Data Type
 * - Clean interface hiding implementation details
 * - Generic element type with priority
 * - Automatic resizing for dynamic workloads
 *
 * THREAD SAFETY: NOT thread-safe (caller must synchronize)
 * MEMORY MANAGEMENT: Uses nimcp_malloc/free for consistency
 *
 * @author NIMCP Project
 * @date 2025-11-01
 */

#ifndef NIMCP_MIN_HEAP_H
#define NIMCP_MIN_HEAP_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Heap element structure
 *
 * Stores a vertex ID and its priority (distance in Dijkstra's algorithm)
 */
typedef struct {
    uint32_t vertex_id; /**< Vertex identifier */
    float priority;     /**< Priority value (lower = higher priority) */
} nimcp_heap_element_t;

/**
 * @brief Opaque min-heap structure
 *
 * Internal implementation hidden from users (ADT pattern)
 */
typedef struct nimcp_min_heap_t nimcp_min_heap_t;

/**
 * @brief Create a new min-heap
 *
 * Allocates and initializes a min-heap with specified capacity.
 * The heap will automatically track vertex positions for O(log n) decrease-key.
 *
 * @param capacity Initial capacity (will grow if needed)
 * @return Pointer to new heap or NULL on allocation failure
 *
 * @complexity O(n) for initialization
 */
nimcp_min_heap_t* nimcp_min_heap_create(uint32_t capacity);

/**
 * @brief Destroy a min-heap and free all resources
 *
 * @param heap Pointer to heap to destroy (NULL safe)
 *
 * @complexity O(1)
 */
void nimcp_min_heap_destroy(nimcp_min_heap_t* heap);

/**
 * @brief Insert an element into the heap
 *
 * Adds a new vertex with given priority. Maintains min-heap property
 * via bubble-up operation.
 *
 * @param heap Target heap
 * @param element Element to insert
 * @return true on success, false if heap is full or NULL
 *
 * @complexity O(log n)
 */
bool nimcp_min_heap_insert(nimcp_min_heap_t* heap, const nimcp_heap_element_t* element);

/**
 * @brief Extract the minimum element
 *
 * Removes and returns the element with lowest priority.
 * Maintains min-heap property via bubble-down operation.
 *
 * @param heap Source heap
 * @param element Pointer to store extracted element
 * @return true on success, false if heap is empty or NULL
 *
 * @complexity O(log n)
 */
bool nimcp_min_heap_extract_min(nimcp_min_heap_t* heap, nimcp_heap_element_t* element);

/**
 * @brief Peek at the minimum element without removing it
 *
 * @param heap Source heap
 * @param element Pointer to store minimum element
 * @return true on success, false if heap is empty or NULL
 *
 * @complexity O(1)
 */
bool nimcp_min_heap_peek_min(const nimcp_min_heap_t* heap, nimcp_heap_element_t* element);

/**
 * @brief Decrease the priority of a vertex
 *
 * Updates the priority of a vertex to a smaller value.
 * Essential for Dijkstra's algorithm when finding shorter paths.
 *
 * REQUIRES: new_priority < current priority of vertex_id
 *
 * @param heap Target heap
 * @param vertex_id Vertex whose priority to decrease
 * @param new_priority New (lower) priority value
 * @return true on success, false if vertex not in heap or invalid parameters
 *
 * @complexity O(log n)
 */
bool nimcp_min_heap_decrease_key(nimcp_min_heap_t* heap, uint32_t vertex_id, float new_priority);

/**
 * @brief Check if heap is empty
 *
 * @param heap Heap to check
 * @return true if empty or NULL, false otherwise
 *
 * @complexity O(1)
 */
bool nimcp_min_heap_is_empty(const nimcp_min_heap_t* heap);

/**
 * @brief Check if heap is full
 *
 * @param heap Heap to check
 * @return true if full, false otherwise
 *
 * @complexity O(1)
 */
bool nimcp_min_heap_is_full(const nimcp_min_heap_t* heap);

/**
 * @brief Get current number of elements in heap
 *
 * @param heap Heap to query
 * @return Number of elements (0 if NULL)
 *
 * @complexity O(1)
 */
uint32_t nimcp_min_heap_size(const nimcp_min_heap_t* heap);

/**
 * @brief Get heap capacity
 *
 * @param heap Heap to query
 * @return Maximum capacity (0 if NULL)
 *
 * @complexity O(1)
 */
uint32_t nimcp_min_heap_capacity(const nimcp_min_heap_t* heap);

/**
 * @brief Clear all elements from heap
 *
 * Resets heap to empty state without deallocating memory.
 *
 * @param heap Heap to clear
 *
 * @complexity O(1)
 */
void nimcp_min_heap_clear(nimcp_min_heap_t* heap);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MIN_HEAP_H */
