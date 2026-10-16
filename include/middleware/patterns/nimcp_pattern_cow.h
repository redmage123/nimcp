//=============================================================================
// nimcp_pattern_cow.h - Copy-on-Write Pattern Data Wrapper
//=============================================================================

#ifndef NIMCP_PATTERN_COW_H
#define NIMCP_PATTERN_COW_H

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_pattern_cow.h
 * @brief Copy-on-Write wrapper for pattern data sharing
 *
 * WHAT: Reference-counted pattern data for zero-copy sharing
 * WHY:  Multiple pattern libraries can share template data
 * HOW:  Atomic reference counting with CoW semantics
 *
 * PATTERN: Copy-on-Write (CoW)
 * - Cheap to clone (increment refcount)
 * - Automatic cleanup (decrement refcount)
 * - Write triggers copy (make_unique)
 */

/**
 * @brief Copy-on-Write pattern data wrapper
 *
 * WHAT: Reference-counted container for pattern vector
 * WHY:  Share pattern data across multiple libraries
 * HOW:  Atomic refcount, automatic cleanup on zero
 */
typedef struct pattern_cow {
    float* data;                    /**< Pattern vector data */
    uint32_t dimension;             /**< Vector dimension */
    atomic_uint_least32_t refcount; /**< Reference count (atomic) */
} pattern_cow_t;

/**
 * @brief Create CoW pattern data wrapper
 *
 * WHAT: Allocate and initialize reference-counted pattern data
 * WHY:  Factory for creating shared pattern data
 * HOW:  Allocate, copy data, set refcount=1
 *
 * COMPLEXITY: O(dimension)
 * THREAD-SAFE: Yes
 *
 * @param data Pattern vector to wrap
 * @param dimension Vector dimension
 * @return CoW wrapper or NULL on error
 */
pattern_cow_t* pattern_cow_create(const float* data, uint32_t dimension);

/**
 * @brief Clone CoW wrapper (increment refcount)
 *
 * WHAT: Create new reference to same data (zero-copy)
 * WHY:  Share pattern data across libraries
 * HOW:  Atomic increment refcount, return same pointer
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 *
 * @param cow CoW wrapper to clone
 * @return Same CoW wrapper with incremented refcount
 */
pattern_cow_t* pattern_cow_clone(pattern_cow_t* cow);

/**
 * @brief Release CoW reference (decrement refcount)
 *
 * WHAT: Decrease reference count, free if zero
 * WHY:  Automatic cleanup when last reference dropped
 * HOW:  Atomic decrement, free if refcount reaches 0
 *
 * COMPLEXITY: O(1) typical, O(dimension) if last reference
 * THREAD-SAFE: Yes
 *
 * @param cow CoW wrapper to release
 */
void pattern_cow_release(pattern_cow_t* cow);

/**
 * @brief Get read-only access to pattern data
 *
 * WHAT: Return pointer to pattern vector
 * WHY:  Read pattern data without copying
 * HOW:  Direct pointer access (caller must not modify!)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 *
 * @param cow CoW wrapper
 * @return Pointer to pattern data (read-only!)
 */
const float* pattern_cow_data(const pattern_cow_t* cow);

/**
 * @brief Get pattern dimension
 *
 * @param cow CoW wrapper
 * @return Pattern dimension
 */
uint32_t pattern_cow_dimension(const pattern_cow_t* cow);

/**
 * @brief Get current reference count (for debugging)
 *
 * @param cow CoW wrapper
 * @return Current refcount
 */
uint32_t pattern_cow_refcount(const pattern_cow_t* cow);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PATTERN_COW_H
