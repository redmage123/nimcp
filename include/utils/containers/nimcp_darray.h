/**
 * @file nimcp_darray.h
 * @brief Generic dynamic array for arbitrary element types
 *
 * This module implements a resizable array that can hold elements of any type.
 * It provides:
 * - O(1) amortized append
 * - O(1) random access by index
 * - O(n) insertion/removal at arbitrary positions
 * - Automatic resizing with configurable growth factor
 *
 * DESIGN PATTERN: Generic container via element_size
 * - Works with any struct or primitive type
 * - Type safety is caller's responsibility
 * - Elements are copied by value (deep copy not provided)
 *
 * THREAD SAFETY: NOT thread-safe (caller must synchronize)
 * MEMORY MANAGEMENT: Uses nimcp_malloc/realloc/free for consistency
 *
 * USAGE EXAMPLE:
 *   // Create array for int elements
 *   nimcp_darray_t* arr = nimcp_darray_create(sizeof(int), 16);
 *
 *   // Add elements
 *   int val = 42;
 *   nimcp_darray_push_back(arr, &val);
 *
 *   // Access elements
 *   int* ptr = (int*)nimcp_darray_at(arr, 0);
 *
 *   // Iterate
 *   for (size_t i = 0; i < nimcp_darray_size(arr); i++) {
 *       int* elem = (int*)nimcp_darray_at(arr, i);
 *   }
 *
 *   // Cleanup
 *   nimcp_darray_destroy(arr);
 *
 * @author NIMCP Project
 * @date 2025-12-08
 */

#ifndef NIMCP_DARRAY_H
#define NIMCP_DARRAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Default initial capacity if none specified
 */
#define NIMCP_DARRAY_DEFAULT_CAPACITY 16

/**
 * @brief Default growth factor (2x)
 */
#define NIMCP_DARRAY_GROWTH_FACTOR 2

/**
 * @brief Opaque dynamic array structure
 */
typedef struct nimcp_darray_t nimcp_darray_t;

/**
 * @brief Optional element destructor callback
 *
 * Called for each element when array is destroyed or element is removed.
 * Use this to free nested pointers within elements.
 *
 * @param element Pointer to element being destroyed
 */
typedef void (*nimcp_darray_destructor_t)(void* element);

/**
 * @brief Create a new dynamic array
 *
 * @param element_size Size of each element in bytes (use sizeof)
 * @param initial_capacity Initial capacity (0 for default)
 * @return Pointer to new array or NULL on allocation failure
 *
 * @complexity O(1)
 */
nimcp_darray_t* nimcp_darray_create(size_t element_size, size_t initial_capacity);

/**
 * @brief Create array with custom destructor
 *
 * The destructor is called for each element when:
 * - Array is destroyed
 * - Element is removed via pop_back, remove_at, or clear
 *
 * @param element_size Size of each element in bytes
 * @param initial_capacity Initial capacity (0 for default)
 * @param destructor Optional element destructor (can be NULL)
 * @return Pointer to new array or NULL on allocation failure
 *
 * @complexity O(1)
 */
nimcp_darray_t* nimcp_darray_create_with_destructor(
    size_t element_size,
    size_t initial_capacity,
    nimcp_darray_destructor_t destructor
);

/**
 * @brief Destroy array and free all resources
 *
 * Calls destructor for each element if one was provided.
 *
 * @param arr Array to destroy (NULL safe)
 *
 * @complexity O(n) if destructor is set, O(1) otherwise
 */
void nimcp_darray_destroy(nimcp_darray_t* arr);

/**
 * @brief Append element to end of array
 *
 * Element is copied into the array. Array grows automatically if needed.
 *
 * @param arr Target array
 * @param element Pointer to element to copy (must be element_size bytes)
 * @return true on success, false on allocation failure or NULL params
 *
 * @complexity O(1) amortized, O(n) worst case when resizing
 */
bool nimcp_darray_push_back(nimcp_darray_t* arr, const void* element);

/**
 * @brief Remove and optionally return last element
 *
 * @param arr Source array
 * @param out_element Optional output buffer to copy removed element (can be NULL)
 * @return true on success, false if array is empty or NULL
 *
 * @complexity O(1), plus destructor time if set
 */
bool nimcp_darray_pop_back(nimcp_darray_t* arr, void* out_element);

/**
 * @brief Get pointer to element at index
 *
 * Returns direct pointer into array storage - valid until next
 * modification that causes reallocation.
 *
 * @param arr Source array
 * @param index Element index (0-based)
 * @return Pointer to element or NULL if index out of bounds
 *
 * @complexity O(1)
 */
void* nimcp_darray_at(nimcp_darray_t* arr, size_t index);

/**
 * @brief Get const pointer to element at index
 *
 * @param arr Source array
 * @param index Element index (0-based)
 * @return Const pointer to element or NULL if index out of bounds
 *
 * @complexity O(1)
 */
const void* nimcp_darray_at_const(const nimcp_darray_t* arr, size_t index);

/**
 * @brief Set element at index
 *
 * Copies element to specified position. Index must be within current size.
 * Use push_back to add new elements.
 *
 * @param arr Target array
 * @param index Element index
 * @param element Pointer to new element value
 * @return true on success, false if index out of bounds
 *
 * @complexity O(1), plus destructor time if set
 */
bool nimcp_darray_set(nimcp_darray_t* arr, size_t index, const void* element);

/**
 * @brief Insert element at index, shifting subsequent elements
 *
 * @param arr Target array
 * @param index Position to insert at (0 to size inclusive)
 * @param element Element to insert
 * @return true on success, false on failure
 *
 * @complexity O(n) due to element shifting
 */
bool nimcp_darray_insert(nimcp_darray_t* arr, size_t index, const void* element);

/**
 * @brief Remove element at index, shifting subsequent elements
 *
 * @param arr Target array
 * @param index Position to remove from
 * @param out_element Optional output buffer for removed element
 * @return true on success, false if index out of bounds
 *
 * @complexity O(n) due to element shifting
 */
bool nimcp_darray_remove_at(nimcp_darray_t* arr, size_t index, void* out_element);

/**
 * @brief Get first element
 *
 * @param arr Source array
 * @return Pointer to first element or NULL if empty
 *
 * @complexity O(1)
 */
void* nimcp_darray_front(nimcp_darray_t* arr);

/**
 * @brief Get last element
 *
 * @param arr Source array
 * @return Pointer to last element or NULL if empty
 *
 * @complexity O(1)
 */
void* nimcp_darray_back(nimcp_darray_t* arr);

/**
 * @brief Get raw data pointer
 *
 * Returns pointer to underlying contiguous storage.
 * Valid until next modification that causes reallocation.
 *
 * @param arr Source array
 * @return Pointer to first element or NULL if empty/NULL array
 *
 * @complexity O(1)
 */
void* nimcp_darray_data(nimcp_darray_t* arr);

/**
 * @brief Get current number of elements
 *
 * @param arr Array to query
 * @return Number of elements (0 if NULL)
 *
 * @complexity O(1)
 */
size_t nimcp_darray_size(const nimcp_darray_t* arr);

/**
 * @brief Get current capacity
 *
 * @param arr Array to query
 * @return Capacity in elements (0 if NULL)
 *
 * @complexity O(1)
 */
size_t nimcp_darray_capacity(const nimcp_darray_t* arr);

/**
 * @brief Get element size
 *
 * @param arr Array to query
 * @return Element size in bytes (0 if NULL)
 *
 * @complexity O(1)
 */
size_t nimcp_darray_element_size(const nimcp_darray_t* arr);

/**
 * @brief Check if array is empty
 *
 * @param arr Array to check
 * @return true if empty or NULL
 *
 * @complexity O(1)
 */
bool nimcp_darray_is_empty(const nimcp_darray_t* arr);

/**
 * @brief Clear all elements without deallocating storage
 *
 * Calls destructor for each element if one was provided.
 *
 * @param arr Array to clear
 *
 * @complexity O(n) if destructor is set, O(1) otherwise
 */
void nimcp_darray_clear(nimcp_darray_t* arr);

/**
 * @brief Reserve capacity for at least n elements
 *
 * Pre-allocates storage to avoid reallocation during push_back.
 * Does nothing if current capacity is already sufficient.
 *
 * @param arr Target array
 * @param capacity Minimum capacity to reserve
 * @return true on success, false on allocation failure
 *
 * @complexity O(n) if reallocation needed, O(1) otherwise
 */
bool nimcp_darray_reserve(nimcp_darray_t* arr, size_t capacity);

/**
 * @brief Shrink capacity to match current size
 *
 * Reduces memory usage by reallocating to exact size.
 *
 * @param arr Target array
 * @return true on success, false on reallocation failure
 *
 * @complexity O(n)
 */
bool nimcp_darray_shrink_to_fit(nimcp_darray_t* arr);

/**
 * @brief Resize array to specific size
 *
 * If new_size > current size, new elements are zero-initialized.
 * If new_size < current size, excess elements are destroyed.
 *
 * @param arr Target array
 * @param new_size Desired size
 * @return true on success, false on allocation failure
 *
 * @complexity O(n)
 */
bool nimcp_darray_resize(nimcp_darray_t* arr, size_t new_size);

/**
 * @brief Swap contents of two arrays
 *
 * Both arrays must have same element_size.
 *
 * @param arr1 First array
 * @param arr2 Second array
 * @return true on success, false if element sizes differ
 *
 * @complexity O(1)
 */
bool nimcp_darray_swap(nimcp_darray_t* arr1, nimcp_darray_t* arr2);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DARRAY_H */
