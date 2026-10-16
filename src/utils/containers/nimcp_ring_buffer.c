/**
 * @file nimcp_ring_buffer.c
 * @brief Fixed-size circular buffer implementation
 *
 * WHAT: Circular buffer that overwrites oldest entries when full
 * WHY:  Efficient bounded history storage
 * HOW:  Fixed-size array with head/tail indices, modular arithmetic
 *
 * NOTE: This ring buffer is NOT thread-safe. External synchronization required.
 *       Callers must use a mutex (e.g., nimcp_mutex_t) around ALL ring buffer
 *       operations when accessed from multiple threads.
 *
 * @author NIMCP Development Team
 * @date 2026-01-02
 * @version 1.0.0
 */

#include "utils/containers/nimcp_ring_buffer.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(ring_buffer)

/*============================================================================
 * Internal Structure
 *============================================================================*/

struct nimcp_ring_buffer {
    void* data;                         /**< Contiguous data storage */
    size_t element_size;                /**< Size of each element */
    size_t capacity;                    /**< Maximum elements */
    size_t size;                        /**< Current number of elements */
    size_t head;                        /**< Index of oldest element */
    size_t tail;                        /**< Index after newest element */
    nimcp_ring_buffer_destructor_t destructor; /**< Element destructor */
};

/*============================================================================
 * Helper Functions
 *============================================================================*/

/**
 * @brief Get pointer to element at physical index
 */
static inline void* get_element_ptr(nimcp_ring_buffer_t* rb, size_t physical_index) {
    return (char*)rb->data + (physical_index * rb->element_size);
}

/**
 * @brief Get const pointer to element at physical index
 */
static inline const void* get_element_ptr_const(const nimcp_ring_buffer_t* rb,
                                                  size_t physical_index) {
    return (const char*)rb->data + (physical_index * rb->element_size);
}

/**
 * @brief Convert logical index to physical index
 */
static inline size_t logical_to_physical(const nimcp_ring_buffer_t* rb,
                                          size_t logical_index) {
    // Guard: capacity should never be 0, but protect against it
    if (rb->capacity == 0) return 0;
    return (rb->head + logical_index) % rb->capacity;
}

/*============================================================================
 * Lifecycle API
 *============================================================================*/

nimcp_ring_buffer_t* nimcp_ring_buffer_create(size_t element_size, size_t capacity) {
    return nimcp_ring_buffer_create_with_destructor(element_size, capacity, NULL);
}

nimcp_ring_buffer_t* nimcp_ring_buffer_create_with_destructor(
    size_t element_size,
    size_t capacity,
    nimcp_ring_buffer_destructor_t destructor) {

    if (element_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_ring_buffer_create_with_destructor: element_size is 0");
        return NULL;
    }
    if (capacity == 0) capacity = NIMCP_RING_BUFFER_DEFAULT_CAPACITY;

    /* P3-U2: Overflow check on capacity * element_size to prevent wrapping */
    if (element_size > 0 && capacity > SIZE_MAX / element_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_ring_buffer_create_with_destructor: capacity * element_size overflows");
        return NULL;
    }
    if (capacity > NIMCP_RING_BUFFER_MAX_CAPACITY) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_ring_buffer_create_with_destructor: capacity exceeds maximum");
        return NULL;
    }

    nimcp_ring_buffer_t* rb = (nimcp_ring_buffer_t*)nimcp_calloc(1, sizeof(nimcp_ring_buffer_t));
    if (!rb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_ring_buffer_create_with_destructor: failed to allocate ring buffer");
        return NULL;
    }

    rb->data = nimcp_calloc(capacity, element_size);
    if (!rb->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_ring_buffer_create_with_destructor: failed to allocate data array");
        nimcp_free(rb);
        return NULL;
    }

    rb->element_size = element_size;
    rb->capacity = capacity;
    rb->size = 0;
    rb->head = 0;
    rb->tail = 0;
    rb->destructor = destructor;

    return rb;
}

void nimcp_ring_buffer_destroy(nimcp_ring_buffer_t* rb) {
    if (!rb) return;

    /* Call destructor on all elements */
    if (rb->destructor) {
        for (size_t i = 0; i < rb->size; i++) {
            size_t phys_idx = logical_to_physical(rb, i);
            rb->destructor(get_element_ptr(rb, phys_idx));
        }
    }

    nimcp_free(rb->data);
    nimcp_free(rb);
}

void nimcp_ring_buffer_clear(nimcp_ring_buffer_t* rb) {
    if (!rb) return;

    /* Call destructor on all elements */
    if (rb->destructor) {
        for (size_t i = 0; i < rb->size; i++) {
            size_t phys_idx = logical_to_physical(rb, i);
            rb->destructor(get_element_ptr(rb, phys_idx));
        }
    }

    rb->size = 0;
    rb->head = 0;
    rb->tail = 0;
}

/*============================================================================
 * Capacity API
 *============================================================================*/

size_t nimcp_ring_buffer_size(const nimcp_ring_buffer_t* rb) {
    return rb ? rb->size : 0;
}

size_t nimcp_ring_buffer_capacity(const nimcp_ring_buffer_t* rb) {
    return rb ? rb->capacity : 0;
}

size_t nimcp_ring_buffer_element_size(const nimcp_ring_buffer_t* rb) {
    return rb ? rb->element_size : 0;
}

bool nimcp_ring_buffer_is_empty(const nimcp_ring_buffer_t* rb) {
    return !rb || rb->size == 0;
}

bool nimcp_ring_buffer_is_full(const nimcp_ring_buffer_t* rb) {
    return rb && rb->size == rb->capacity;
}

/*============================================================================
 * Element Access API
 *============================================================================*/

void* nimcp_ring_buffer_at(nimcp_ring_buffer_t* rb, size_t index) {
    if (!rb || index >= rb->size) {
        return NULL;
    }
    return get_element_ptr(rb, logical_to_physical(rb, index));
}

const void* nimcp_ring_buffer_at_const(const nimcp_ring_buffer_t* rb, size_t index) {
    if (!rb || index >= rb->size) {
        return NULL;
    }
    return get_element_ptr_const(rb, logical_to_physical(rb, index));
}

void* nimcp_ring_buffer_front(nimcp_ring_buffer_t* rb) {
    if (!rb || rb->size == 0) {
        return NULL;
    }
    return get_element_ptr(rb, rb->head);
}

void* nimcp_ring_buffer_back(nimcp_ring_buffer_t* rb) {
    if (!rb || rb->size == 0) {
        return NULL;
    }
    size_t back_idx = (rb->tail + rb->capacity - 1) % rb->capacity;
    return get_element_ptr(rb, back_idx);
}

const void* nimcp_ring_buffer_front_const(const nimcp_ring_buffer_t* rb) {
    if (!rb || rb->size == 0) {
        return NULL;
    }
    return get_element_ptr_const(rb, rb->head);
}

const void* nimcp_ring_buffer_back_const(const nimcp_ring_buffer_t* rb) {
    if (!rb || rb->size == 0) {
        return NULL;
    }
    size_t back_idx = (rb->tail + rb->capacity - 1) % rb->capacity;
    return get_element_ptr_const(rb, back_idx);
}

/*============================================================================
 * Modification API
 *============================================================================*/

bool nimcp_ring_buffer_push(nimcp_ring_buffer_t* rb, const void* element) {
    if (!rb || !element) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_ring_buffer_push: required parameter is NULL (rb, element)");
        return false;
    }

    /* If full, overwrite oldest element */
    if (rb->size == rb->capacity) {
        /* Call destructor on oldest element being overwritten */
        if (rb->destructor) {
            rb->destructor(get_element_ptr(rb, rb->head));
        }
        rb->head = (rb->head + 1) % rb->capacity;
    } else {
        rb->size++;
    }

    /* Copy new element to tail position */
    memcpy(get_element_ptr(rb, rb->tail), element, rb->element_size);
    rb->tail = (rb->tail + 1) % rb->capacity;

    return true;
}

bool nimcp_ring_buffer_pop_front(nimcp_ring_buffer_t* rb, void* out_element) {
    if (!rb || rb->size == 0) {
        return false;
    }

    /* Copy element if output provided */
    if (out_element) {
        memcpy(out_element, get_element_ptr(rb, rb->head), rb->element_size);
    }

    rb->head = (rb->head + 1) % rb->capacity;
    rb->size--;

    return true;
}

bool nimcp_ring_buffer_pop_back(nimcp_ring_buffer_t* rb, void* out_element) {
    if (!rb || rb->size == 0) {
        return false;
    }

    rb->tail = (rb->tail + rb->capacity - 1) % rb->capacity;
    rb->size--;

    /* Copy element if output provided */
    if (out_element) {
        memcpy(out_element, get_element_ptr(rb, rb->tail), rb->element_size);
    }

    return true;
}

void* nimcp_ring_buffer_peek_from_back(nimcp_ring_buffer_t* rb, size_t n_from_back) {
    if (!rb || n_from_back >= rb->size) {
        return NULL;
    }

    /* Convert to logical index (0 = oldest, size-1 = newest) */
    size_t logical_idx = rb->size - 1 - n_from_back;
    return get_element_ptr(rb, logical_to_physical(rb, logical_idx));
}

/*============================================================================
 * Iteration API
 *============================================================================*/

void nimcp_ring_buffer_foreach(nimcp_ring_buffer_t* rb,
                                nimcp_ring_buffer_iterator_t iterator,
                                void* context) {
    if (!rb || !iterator) return;

    for (size_t i = 0; i < rb->size; i++) {
        void* elem = get_element_ptr(rb, logical_to_physical(rb, i));
        if (!iterator(elem, i, context)) break;
    }
}

void nimcp_ring_buffer_foreach_reverse(nimcp_ring_buffer_t* rb,
                                        nimcp_ring_buffer_iterator_t iterator,
                                        void* context) {
    if (!rb || !iterator || rb->size == 0) return;

    for (size_t i = rb->size; i > 0; i--) {
        size_t idx = i - 1;
        void* elem = get_element_ptr(rb, logical_to_physical(rb, idx));
        if (!iterator(elem, idx, context)) break;
    }
}

/*============================================================================
 * Utility API
 *============================================================================*/

size_t nimcp_ring_buffer_copy_last_n(const nimcp_ring_buffer_t* rb,
                                      void* out_array,
                                      size_t n) {
    if (!rb || !out_array || n == 0) return 0;

    size_t to_copy = (n < rb->size) ? n : rb->size;
    char* out = (char*)out_array;

    /* Copy from newest to oldest */
    for (size_t i = 0; i < to_copy; i++) {
        size_t logical_idx = rb->size - 1 - i;
        const void* elem = get_element_ptr_const(rb, logical_to_physical(rb, logical_idx));
        memcpy(out + (i * rb->element_size), elem, rb->element_size);
    }

    return to_copy;
}

void* nimcp_ring_buffer_raw_data(nimcp_ring_buffer_t* rb) {
    return rb ? rb->data : NULL;
}
