/**
 * @file nimcp_ring_buffer.h
 * @brief Fixed-size circular buffer with O(1) operations
 *
 * WHAT: Circular buffer that overwrites oldest entries when full
 * WHY:  Efficient bounded history storage (e.g., trajectory history)
 * HOW:  Fixed-size array with head/tail pointers, modular arithmetic
 *
 * PERFORMANCE:
 * - Push: O(1)
 * - Pop: O(1)
 * - Access by index: O(1)
 * - Memory: Fixed, contiguous, cache-friendly
 *
 * USAGE EXAMPLE:
 *   // Create ring buffer for 128 trajectory states
 *   nimcp_ring_buffer_t* rb = nimcp_ring_buffer_create(sizeof(state_t), 128);
 *
 *   // Add states (overwrites oldest when full)
 *   state_t state = {...};
 *   nimcp_ring_buffer_push(rb, &state);
 *
 *   // Access states (0 = oldest, size-1 = newest)
 *   state_t* oldest = nimcp_ring_buffer_at(rb, 0);
 *   state_t* newest = nimcp_ring_buffer_back(rb);
 *
 *   // Cleanup
 *   nimcp_ring_buffer_destroy(rb);
 *
 * @author NIMCP Development Team
 * @date 2026-01-02
 * @version 1.0.0
 */

#ifndef NIMCP_RING_BUFFER_H
#define NIMCP_RING_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

/** @brief Default capacity if none specified */
#define NIMCP_RING_BUFFER_DEFAULT_CAPACITY 64

/** @brief Maximum supported capacity */
#define NIMCP_RING_BUFFER_MAX_CAPACITY ((size_t)1 << 24)

/*============================================================================
 * Types
 *============================================================================*/

/**
 * @brief Optional element destructor callback
 *
 * Called when elements are overwritten or buffer is destroyed.
 */
typedef void (*nimcp_ring_buffer_destructor_t)(void* element);

/**
 * @brief Opaque ring buffer structure
 */
typedef struct nimcp_ring_buffer nimcp_ring_buffer_t;

/*============================================================================
 * Lifecycle API
 *============================================================================*/

/**
 * @brief Create a new ring buffer
 *
 * @param element_size Size of each element in bytes
 * @param capacity Maximum number of elements (fixed)
 * @return New ring buffer or NULL on failure
 */
nimcp_ring_buffer_t* nimcp_ring_buffer_create(size_t element_size, size_t capacity);

/**
 * @brief Create ring buffer with destructor
 *
 * Destructor is called when elements are overwritten or buffer is destroyed.
 *
 * @param element_size Size of each element in bytes
 * @param capacity Maximum number of elements
 * @param destructor Element destructor (can be NULL)
 * @return New ring buffer or NULL on failure
 */
nimcp_ring_buffer_t* nimcp_ring_buffer_create_with_destructor(
    size_t element_size,
    size_t capacity,
    nimcp_ring_buffer_destructor_t destructor);

/**
 * @brief Destroy ring buffer and free all resources
 *
 * Calls destructor for each element if one was provided.
 *
 * @param rb Ring buffer to destroy (NULL-safe)
 */
void nimcp_ring_buffer_destroy(nimcp_ring_buffer_t* rb);

/**
 * @brief Clear all elements without deallocating
 *
 * Calls destructor for each element if one was provided.
 *
 * @param rb Ring buffer to clear
 */
void nimcp_ring_buffer_clear(nimcp_ring_buffer_t* rb);

/*============================================================================
 * Capacity API
 *============================================================================*/

/**
 * @brief Get current number of elements
 */
size_t nimcp_ring_buffer_size(const nimcp_ring_buffer_t* rb);

/**
 * @brief Get maximum capacity
 */
size_t nimcp_ring_buffer_capacity(const nimcp_ring_buffer_t* rb);

/**
 * @brief Get element size in bytes
 */
size_t nimcp_ring_buffer_element_size(const nimcp_ring_buffer_t* rb);

/**
 * @brief Check if buffer is empty
 */
bool nimcp_ring_buffer_is_empty(const nimcp_ring_buffer_t* rb);

/**
 * @brief Check if buffer is full
 */
bool nimcp_ring_buffer_is_full(const nimcp_ring_buffer_t* rb);

/*============================================================================
 * Element Access API
 *============================================================================*/

/**
 * @brief Get element at index (0 = oldest, size-1 = newest)
 *
 * @param rb Ring buffer
 * @param index Logical index (0 = oldest)
 * @return Pointer to element or NULL if out of bounds
 */
void* nimcp_ring_buffer_at(nimcp_ring_buffer_t* rb, size_t index);

/**
 * @brief Get element at index (const version)
 */
const void* nimcp_ring_buffer_at_const(const nimcp_ring_buffer_t* rb, size_t index);

/**
 * @brief Get oldest element (front)
 */
void* nimcp_ring_buffer_front(nimcp_ring_buffer_t* rb);

/**
 * @brief Get newest element (back)
 */
void* nimcp_ring_buffer_back(nimcp_ring_buffer_t* rb);

/**
 * @brief Get oldest element (const version)
 */
const void* nimcp_ring_buffer_front_const(const nimcp_ring_buffer_t* rb);

/**
 * @brief Get newest element (const version)
 */
const void* nimcp_ring_buffer_back_const(const nimcp_ring_buffer_t* rb);

/*============================================================================
 * Modification API
 *============================================================================*/

/**
 * @brief Push element to back (newest position)
 *
 * If buffer is full, overwrites oldest element.
 * Destructor is called on overwritten element if set.
 *
 * @param rb Ring buffer
 * @param element Element to push (copied by value)
 * @return true on success, false on NULL params
 */
bool nimcp_ring_buffer_push(nimcp_ring_buffer_t* rb, const void* element);

/**
 * @brief Pop oldest element from front
 *
 * Does NOT call destructor.
 *
 * @param rb Ring buffer
 * @param out_element Optional output buffer for popped element
 * @return true on success, false if empty
 */
bool nimcp_ring_buffer_pop_front(nimcp_ring_buffer_t* rb, void* out_element);

/**
 * @brief Pop newest element from back
 *
 * Does NOT call destructor.
 *
 * @param rb Ring buffer
 * @param out_element Optional output buffer for popped element
 * @return true on success, false if empty
 */
bool nimcp_ring_buffer_pop_back(nimcp_ring_buffer_t* rb, void* out_element);

/**
 * @brief Peek at element N positions from back (newest)
 *
 * @param rb Ring buffer
 * @param n_from_back 0 = newest, 1 = second newest, etc.
 * @return Pointer to element or NULL if out of bounds
 */
void* nimcp_ring_buffer_peek_from_back(nimcp_ring_buffer_t* rb, size_t n_from_back);

/*============================================================================
 * Iteration API
 *============================================================================*/

/**
 * @brief Iterator callback type
 *
 * @param element Pointer to element
 * @param index Logical index (0 = oldest)
 * @param context User context
 * @return true to continue, false to stop iteration
 */
typedef bool (*nimcp_ring_buffer_iterator_t)(void* element, size_t index, void* context);

/**
 * @brief Iterate from oldest to newest
 *
 * @param rb Ring buffer
 * @param iterator Iterator function
 * @param context User context passed to iterator
 */
void nimcp_ring_buffer_foreach(nimcp_ring_buffer_t* rb,
                                nimcp_ring_buffer_iterator_t iterator,
                                void* context);

/**
 * @brief Iterate from newest to oldest
 *
 * @param rb Ring buffer
 * @param iterator Iterator function
 * @param context User context passed to iterator
 */
void nimcp_ring_buffer_foreach_reverse(nimcp_ring_buffer_t* rb,
                                        nimcp_ring_buffer_iterator_t iterator,
                                        void* context);

/*============================================================================
 * Utility API
 *============================================================================*/

/**
 * @brief Copy last N elements to output array
 *
 * Copies from newest to oldest.
 *
 * @param rb Ring buffer
 * @param out_array Output array (must be at least n * element_size bytes)
 * @param n Number of elements to copy
 * @return Number of elements actually copied
 */
size_t nimcp_ring_buffer_copy_last_n(const nimcp_ring_buffer_t* rb,
                                      void* out_array,
                                      size_t n);

/**
 * @brief Get raw data pointer (for debugging)
 *
 * Note: Data is circular, use at() for proper access.
 */
void* nimcp_ring_buffer_raw_data(nimcp_ring_buffer_t* rb);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RING_BUFFER_H */
